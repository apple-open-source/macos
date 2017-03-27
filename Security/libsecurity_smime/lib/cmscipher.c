/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Encryption/decryption routines for CMS implementation, none of which are exported.
 *
 */
#include <limits.h>

#include "cmslocal.h"

#include "secoid.h"
#include <security_asn1/secerr.h>
#include <security_asn1/secasn1.h>
#include <security_asn1/secport.h>

#include <Security/SecAsn1Templates.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonCryptor.h>

/*
 * -------------------------------------------------------------------
 * Cipher stuff.
 */

#if 0
typedef OSStatus (*nss_cms_cipher_function) (void *, unsigned char *, unsigned int *,
					unsigned int, const unsigned char *, unsigned int);
typedef OSStatus (*nss_cms_cipher_destroy) (void *, Boolean);
#endif

#define BLOCK_SIZE 4096

struct SecCmsCipherContextStr {
#if 1
    void *              cc;			/* CSP CONTEXT */
    Boolean		encrypt;		/* encrypt / decrypt switch */
    int			block_size;		/* block & pad sizes for cipher */
#else
    void *		cx;			/* PK11 cipher context */
    nss_cms_cipher_function doit;
    nss_cms_cipher_destroy destroy;
    Boolean		encrypt;		/* encrypt / decrypt switch */
    int			pad_size;
    int			pending_count;		/* pending data (not yet en/decrypted */
    unsigned char	pending_buf[BLOCK_SIZE];/* because of blocking */
#endif
};

typedef struct sec_rc2cbcParameterStr {
    SecAsn1Item rc2ParameterVersion;
    SecAsn1Item iv;
} sec_rc2cbcParameter;

__unused static const SecAsn1Template sec_rc2cbc_parameter_template[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(sec_rc2cbcParameter) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
          offsetof(sec_rc2cbcParameter,rc2ParameterVersion) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(sec_rc2cbcParameter,iv) },
    { 0 }
};

/* default IV size in bytes */
#define DEFAULT_IV_SIZE	    8
/* IV/block size for AES */
#define AES_BLOCK_SIZE	    16
/* max IV size in bytes */
#define MAX_IV_SIZE	    AES_BLOCK_SIZE

#ifndef kCCKeySizeMaxRC2
#define kCCKeySizeMaxRC2 16
#endif
#ifndef kCCBlockSizeRC2
#define kCCBlockSizeRC2 8
#endif

static SecCmsCipherContextRef
SecCmsCipherContextStart(PRArenaPool *poolp, SecSymmetricKeyRef key, SECAlgorithmID *algid, Boolean encrypt)
{
    SecCmsCipherContextRef cc;
    SECOidData *oidData;
    SECOidTag algtag;
    OSStatus rv;
    uint8_t ivbuf[MAX_IV_SIZE];
    SecAsn1Item initVector = { DEFAULT_IV_SIZE, ivbuf };
    CCCryptorRef ciphercc = NULL;
    CCOptions cipheroptions = kCCOptionPKCS7Padding;
    int cipher_blocksize = 0;
    // @@@ Add support for PBE based stuff

    oidData = SECOID_FindOID(&algid->algorithm);
    if (!oidData)
	goto loser;
    algtag = oidData->offset;

    CCAlgorithm alg = -1;
    switch (algtag) {
        case SEC_OID_DES_CBC:
            alg = kCCAlgorithmDES;
            cipher_blocksize = kCCBlockSizeDES;
            break;
        case SEC_OID_DES_EDE3_CBC:
            alg = kCCAlgorithm3DES;
            cipher_blocksize = kCCBlockSize3DES;
            break;
        case SEC_OID_RC2_CBC:
            alg = kCCAlgorithmRC2;
            cipher_blocksize = kCCBlockSizeRC2;
            break;
        case SEC_OID_AES_128_CBC: 
        case SEC_OID_AES_192_CBC:
        case SEC_OID_AES_256_CBC:
            alg = kCCAlgorithmAES128;
            cipher_blocksize = kCCBlockSizeAES128;
            initVector.Length = AES_BLOCK_SIZE;
             break;
        default: 
            goto loser;
    }

    if (encrypt)
    {
        if (SecRandomCopyBytes(kSecRandomDefault, 
            initVector.Length, initVector.Data))
                goto loser;

	// Put IV into algid.parameters
	switch (algtag)
	{
	case SEC_OID_RC4:
	case SEC_OID_DES_EDE3_CBC:
	case SEC_OID_DES_EDE:
	case SEC_OID_DES_CBC:
	case SEC_OID_AES_128_CBC:
	case SEC_OID_AES_192_CBC:
	case SEC_OID_AES_256_CBC:
	case SEC_OID_FORTEZZA_SKIPJACK:
	case SEC_OID_DES_ECB:
	case SEC_OID_AES_128_ECB:
	case SEC_OID_AES_192_ECB:
	case SEC_OID_AES_256_ECB:
	case SEC_OID_DES_OFB:
	case SEC_OID_DES_CFB:
	    /* Just encode the initVector as an octet string. */
	    if (!SEC_ASN1EncodeItem(poolp, &algid->parameters,
				    &initVector, kSecAsn1OctetStringTemplate))
		goto loser;
	    break;
	case SEC_OID_RC2_CBC:
	case SEC_OID_RC5_CBC_PAD:
	default:
	    // @@@ Implement rc5 params stuff.
	    goto loser;
	    break;
	}
    }
    else
    {
	// Extract IV from algid.parameters
	// Put IV into algid.parameters
	switch (algtag)
	{
	case SEC_OID_RC4:
	case SEC_OID_DES_EDE3_CBC:
	case SEC_OID_DES_EDE:
	case SEC_OID_DES_CBC:
	case SEC_OID_AES_128_CBC:
	case SEC_OID_AES_192_CBC:
	case SEC_OID_AES_256_CBC:
	case SEC_OID_FORTEZZA_SKIPJACK:
	case SEC_OID_DES_ECB:
	case SEC_OID_AES_128_ECB:
	case SEC_OID_AES_192_ECB:
	case SEC_OID_AES_256_ECB:
	case SEC_OID_DES_OFB:
	case SEC_OID_DES_CFB:
	{
	    SecAsn1Item iv = {};
	    /* Just decode the initVector from an octet string. */
	    rv = SEC_ASN1DecodeItem(NULL, &iv, kSecAsn1OctetStringTemplate, &(algid->parameters));
	    if (rv)
		goto loser;
	    if (initVector.Length != iv.Length) {
		PORT_Free(iv.Data);
		goto loser;
	    }
	    memcpy(initVector.Data, iv.Data, initVector.Length);
	    PORT_Free(iv.Data);
	    break;
	}
	case SEC_OID_RC2_CBC:
	case SEC_OID_RC5_CBC_PAD:
	default:
	    // @@@ Implement rc5 params stuff.
	    goto loser;
	    break;
	}
    }

        if (CCCryptorCreate(encrypt ? kCCEncrypt : kCCDecrypt, 
            alg, cipheroptions, CFDataGetBytePtr(key), CFDataGetLength(key), 
            initVector.Data, &ciphercc))
                goto loser;

    cc = (SecCmsCipherContextRef)PORT_ZAlloc(sizeof(SecCmsCipherContext));
    if (cc == NULL)
	goto loser;

    cc->cc = ciphercc;
    cc->encrypt = encrypt;
    cc->block_size =cipher_blocksize;
    return cc;
loser:
    if (ciphercc)
        CCCryptorRelease(ciphercc);

    return NULL;
}

/*
 * SecCmsCipherContextStartDecrypt - create a cipher context to do decryption
 * based on the given bulk * encryption key and algorithm identifier (which may include an iv).
 *
 * XXX Once both are working, it might be nice to combine this and the
 * function below (for starting up encryption) into one routine, and just
 * have two simple cover functions which call it. 
 */
SecCmsCipherContextRef
SecCmsCipherContextStartDecrypt(SecSymmetricKeyRef key, SECAlgorithmID *algid)
{
    return SecCmsCipherContextStart(NULL, key, algid, PR_FALSE);
}

/*
 * SecCmsCipherContextStartEncrypt - create a cipher object to do encryption,
 * based on the given bulk encryption key and algorithm tag.  Fill in the algorithm
 * identifier (which may include an iv) appropriately.
 *
 * XXX Once both are working, it might be nice to combine this and the
 * function above (for starting up decryption) into one routine, and just
 * have two simple cover functions which call it. 
 */
SecCmsCipherContextRef
SecCmsCipherContextStartEncrypt(PRArenaPool *poolp, SecSymmetricKeyRef key, SECAlgorithmID *algid)
{
    return SecCmsCipherContextStart(poolp, key, algid, PR_TRUE);
}

void
SecCmsCipherContextDestroy(SecCmsCipherContextRef cc)
{
    PORT_Assert(cc != NULL);
    if (cc == NULL)
	return;

    CCCryptorRelease(cc->cc);

    PORT_Free(cc);
}

static unsigned int
SecCmsCipherContextLength(SecCmsCipherContextRef cc, unsigned int input_len, Boolean final, Boolean encrypt)
{
    return ((input_len + cc->block_size - 1) / cc->block_size * cc->block_size) + (final ? cc->block_size : 0);
}

/*
 * SecCmsCipherContextDecryptLength - find the output length of the next call to decrypt.
 *
 * cc - the cipher context
 * input_len - number of bytes used as input
 * final - true if this is the final chunk of data
 *
 * Result can be used to perform memory allocations.  Note that the amount
 * is exactly accurate only when not doing a block cipher or when final
 * is false, otherwise it is an upper bound on the amount because until
 * we see the data we do not know how many padding bytes there are
 * (always between 1 and bsize).
 *
 * Note that this can return zero, which does not mean that the decrypt
 * operation can be skipped!  (It simply means that there are not enough
 * bytes to make up an entire block; the bytes will be reserved until
 * there are enough to encrypt/decrypt at least one block.)  However,
 * if zero is returned it *does* mean that no output buffer need be
 * passed in to the subsequent decrypt operation, as no output bytes
 * will be stored.
 */
unsigned int
SecCmsCipherContextDecryptLength(SecCmsCipherContextRef cc, unsigned int input_len, Boolean final)
{
    return SecCmsCipherContextLength(cc, input_len, final, PR_FALSE);
}

/*
 * SecCmsCipherContextEncryptLength - find the output length of the next call to encrypt.
 *
 * cc - the cipher context
 * input_len - number of bytes used as input
 * final - true if this is the final chunk of data
 *
 * Result can be used to perform memory allocations.
 *
 * Note that this can return zero, which does not mean that the encrypt
 * operation can be skipped!  (It simply means that there are not enough
 * bytes to make up an entire block; the bytes will be reserved until
 * there are enough to encrypt/decrypt at least one block.)  However,
 * if zero is returned it *does* mean that no output buffer need be
 * passed in to the subsequent encrypt operation, as no output bytes
 * will be stored.
 */
unsigned int
SecCmsCipherContextEncryptLength(SecCmsCipherContextRef cc, unsigned int input_len, Boolean final)
{
    return SecCmsCipherContextLength(cc, input_len, final, PR_TRUE);
}


static OSStatus
SecCmsCipherContextCrypt(SecCmsCipherContextRef cc, unsigned char *output,
		  unsigned int *output_len_p, unsigned int max_output_len,
		  const unsigned char *input, unsigned int input_len,
		  Boolean final, Boolean encrypt)
{
    size_t bytes_output = 0;
    OSStatus rv = 0;

    if (input_len)
    {
        rv = CCCryptorUpdate(cc->cc, input, input_len, output, max_output_len, &bytes_output);
    }

    if (!rv && final)
    {
        size_t bytes_output_final = 0;
        rv = CCCryptorFinal(cc->cc, output+bytes_output, max_output_len-bytes_output, &bytes_output_final);
        bytes_output += bytes_output_final;
    }

    if (rv)
	PORT_SetError(SEC_ERROR_BAD_DATA);
    else if (output_len_p)
	*output_len_p = (unsigned int)bytes_output; /* This cast is safe since bytes_output can't be bigger than max_output_len */

    return rv;
}

/*
 * SecCmsCipherContextDecrypt - do the decryption
 *
 * cc - the cipher context
 * output - buffer for decrypted result bytes
 * output_len_p - number of bytes in output
 * max_output_len - upper bound on bytes to put into output
 * input - pointer to input bytes
 * input_len - number of input bytes
 * final - true if this is the final chunk of data
 *
 * Decrypts a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the decrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "cc" is the return value from SecCmsCipherStartDecrypt.
 * When "final" is true, this is the last of the data to be decrypted.
 *
 * This is much more complicated than it sounds when the cipher is
 * a block-type, meaning that the decryption function will only
 * operate on whole blocks.  But our caller is operating stream-wise,
 * and can pass in any number of bytes.  So we need to keep track
 * of block boundaries.  We save excess bytes between calls in "cc".
 * We also need to determine which bytes are padding, and remove
 * them from the output.  We can only do this step when we know we
 * have the final block of data.  PKCS #7 specifies that the padding
 * used for a block cipher is a string of bytes, each of whose value is
 * the same as the length of the padding, and that all data is padded.
 * (Even data that starts out with an exact multiple of blocks gets
 * added to it another block, all of which is padding.)
 */ 
OSStatus
SecCmsCipherContextDecrypt(SecCmsCipherContextRef cc, unsigned char *output,
		  unsigned int *output_len_p, unsigned int max_output_len,
		  const unsigned char *input, unsigned int input_len,
		  Boolean final)
{
    return SecCmsCipherContextCrypt(cc, output,
		  output_len_p,  max_output_len,
		  input, input_len,
		  final, PR_FALSE);
}

/*
 * SecCmsCipherContextEncrypt - do the encryption
 *
 * cc - the cipher context
 * output - buffer for decrypted result bytes
 * output_len_p - number of bytes in output
 * max_output_len - upper bound on bytes to put into output
 * input - pointer to input bytes
 * input_len - number of input bytes
 * final - true if this is the final chunk of data
 *
 * Encrypts a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the encrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "cc" is the return value from SecCmsCipherStartEncrypt.
 * When "final" is true, this is the last of the data to be encrypted.
 *
 * This is much more complicated than it sounds when the cipher is
 * a block-type, meaning that the encryption function will only
 * operate on whole blocks.  But our caller is operating stream-wise,
 * and can pass in any number of bytes.  So we need to keep track
 * of block boundaries.  We save excess bytes between calls in "cc".
 * We also need to add padding bytes at the end.  PKCS #7 specifies
 * that the padding used for a block cipher is a string of bytes,
 * each of whose value is the same as the length of the padding,
 * and that all data is padded.  (Even data that starts out with
 * an exact multiple of blocks gets added to it another block,
 * all of which is padding.)
 *
 * XXX I would kind of like to combine this with the function above
 * which does decryption, since they have a lot in common.  But the
 * tricky parts about padding and filling blocks would be much
 * harder to read that way, so I left them separate.  At least for
 * now until it is clear that they are right.
 */ 
OSStatus
SecCmsCipherContextEncrypt(SecCmsCipherContextRef cc, unsigned char *output,
		  unsigned int *output_len_p, unsigned int max_output_len,
		  const unsigned char *input, unsigned int input_len,
		  Boolean final)
{
    return SecCmsCipherContextCrypt(cc, output,
		  output_len_p,  max_output_len,
		  input, input_len,
		  final, PR_TRUE);
}
