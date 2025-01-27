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

#include <libDER/DER_Decode.h>
#include "cmslocal.h"

#include <Security/SecAsn1Templates.h>
#include <Security/SecKeyPriv.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include "secoid.h"

/*
 * -------------------------------------------------------------------
 * Cipher stuff.
 */


#define BLOCK_SIZE 4096

struct SecCmsCipherContextStr {
    CSSM_CC_HANDLE cc; /* CSP CONTEXT */
    Boolean encrypt;   /* encrypt / decrypt switch */
};

typedef struct sec_rc2cbcParameterStr {
    SECItem rc2ParameterVersion;
    SECItem iv;
} sec_rc2cbcParameter;

static const SecAsn1Template sec_rc2cbc_parameter_template[] = {
    {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(sec_rc2cbcParameter)},
    {SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT, offsetof(sec_rc2cbcParameter, rc2ParameterVersion)},
    {SEC_ASN1_OCTET_STRING, offsetof(sec_rc2cbcParameter, iv)},
    {0}};

/* S/MIME picked id values to represent differnt keysizes */
/* I do have a formula, but it ain't pretty, and it only works because you
 * can always match three points to a parabola:) */
static unsigned char rc2_map(SECItem* version)
{
    DERLong x = 0;
    DERItem der_version = { .data = version->Data, .length = version->Length };
    DERParseInteger64(&der_version, &x);

    switch (x) {
        case 58:
            return 128;
        case 120:
            return 64;
        case 160:
            return 40;
    }
    return 128;
}

static unsigned long rc2_unmap(unsigned long x)
{
    switch (x) {
        case 128:
            return 58;
        case 64:
            return 120;
        case 40:
            return 160;
    }
    return 58;
}

/* default IV size in bytes */
#define DEFAULT_IV_SIZE 8
/* IV/block size for AES */
#define AES_BLOCK_SIZE 16
/* max IV size in bytes */
#define MAX_IV_SIZE AES_BLOCK_SIZE

static SecCmsCipherContextRef
SecCmsCipherContextStart(PRArenaPool* poolp, SecSymmetricKeyRef key, SECAlgorithmID* algid, Boolean encrypt)
{
    SecCmsCipherContextRef cc;
    CSSM_CC_HANDLE ciphercc = 0;
    SECOidData* oidData;
    SECOidTag algtag;
    CSSM_ALGORITHMS algorithm;
    CSSM_PADDING padding = CSSM_PADDING_PKCS7;
    CSSM_ENCRYPT_MODE mode;
    CSSM_CSP_HANDLE cspHandle;
    const CSSM_KEY* cssmKey;
    OSStatus rv;
    uint8 ivbuf[MAX_IV_SIZE];
    CSSM_DATA initVector = {DEFAULT_IV_SIZE, ivbuf};
    //CSSM_CONTEXT_ATTRIBUTE contextAttribute = { CSSM_ATTRIBUTE_ALG_PARAMS, sizeof(CSSM_DATA_PTR) };

    rv = SecKeyGetCSPHandle(key, &cspHandle);
    if (rv) {
        goto loser;
    }
    rv = SecKeyGetCSSMKey(key, &cssmKey);
    if (rv) {
        goto loser;
    }

    // @@@ Add support for PBE based stuff

    oidData = SECOID_FindOID(&algid->algorithm);
    if (!oidData)
        goto loser;
    algtag = oidData->offset;
    algorithm = oidData->cssmAlgorithm;
    if (!algorithm) {
        goto loser;
    }

    switch (algtag) {
        case SEC_OID_RC2_CBC:
        case SEC_OID_RC4:
        case SEC_OID_DES_EDE3_CBC:
        case SEC_OID_DES_EDE:
        case SEC_OID_DES_CBC:
        case SEC_OID_RC5_CBC_PAD:
        case SEC_OID_FORTEZZA_SKIPJACK:
            mode = CSSM_ALGMODE_CBCPadIV8;
            break;

        /* RFC 3565 says that these sizes refer to key size, NOT block size */
        case SEC_OID_AES_128_CBC:
        case SEC_OID_AES_192_CBC:
        case SEC_OID_AES_256_CBC:
            initVector.Length = AES_BLOCK_SIZE;
            mode = CSSM_ALGMODE_CBCPadIV8;
            break;

        case SEC_OID_DES_ECB:
        case SEC_OID_AES_128_ECB:
        case SEC_OID_AES_192_ECB:
        case SEC_OID_AES_256_ECB:
            mode = CSSM_ALGMODE_ECBPad;
            break;

        case SEC_OID_DES_OFB:
            mode = CSSM_ALGMODE_OFBPadIV8;
            break;

        case SEC_OID_DES_CFB:
            mode = CSSM_ALGMODE_CFBPadIV8;
            break;

        default:
            goto loser;
    }

    if (encrypt) {
        CSSM_CC_HANDLE randomcc;
        //SECItem *parameters;

        // Generate random initVector
        if (CSSM_CSP_CreateRandomGenContext(cspHandle,
                                            CSSM_ALGID_APPLE_YARROW,
                                            NULL, /* seed*/
                                            initVector.Length,
                                            &randomcc)) {
            goto loser;
        }

        if (CSSM_GenerateRandom(randomcc, &initVector)) {
            goto loser;
        }
        CSSM_DeleteContext(randomcc);

        // Put IV into algid.parameters
        switch (algtag) {
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
                if (!SEC_ASN1EncodeItem(poolp, &algid->parameters, &initVector, kSecAsn1OctetStringTemplate))
                    goto loser;
                break;

            case SEC_OID_RC2_CBC: {
                sec_rc2cbcParameter rc2 = {};
                unsigned long rc2version;
                SECItem* newParams;

                rc2.iv = initVector;
                rc2version = rc2_unmap(cssmKey->KeyHeader.LogicalKeySizeInBits);
                if (!SEC_ASN1EncodeUnsignedInteger(NULL, &(rc2.rc2ParameterVersion), rc2version))
                    goto loser;
                newParams = SEC_ASN1EncodeItem(poolp, &algid->parameters, &rc2, sec_rc2cbc_parameter_template);
                PORT_Free(rc2.rc2ParameterVersion.Data);
                rc2.rc2ParameterVersion.Data = NULL;
                if (newParams == NULL)
                    goto loser;
                break;
            }
            case SEC_OID_RC5_CBC_PAD:
            default:
                // @@@ Implement rc5 params stuff.
                goto loser;
                break;
        }
    } else {
        // Extract IV from algid.parameters
        // Put IV into algid.parameters
        switch (algtag) {
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
            case SEC_OID_DES_CFB: {
                CSSM_DATA iv = {};
                /* Just decode the initVector from an octet string. */
                rv = SEC_ASN1DecodeItem(NULL, &iv, kSecAsn1OctetStringTemplate, &(algid->parameters));
                if (rv)
                    goto loser;
                if (initVector.Length != iv.Length) {
                    PORT_Free(iv.Data);
                    iv.Data = NULL;
                    goto loser;
                }
                memcpy(initVector.Data, iv.Data, initVector.Length);
                PORT_Free(iv.Data);
                iv.Data = NULL;
                break;
            }
            case SEC_OID_RC2_CBC: {
                sec_rc2cbcParameter rc2 = {};
                unsigned long ulEffectiveBits;

                rv = SEC_ASN1DecodeItem(NULL, &rc2, sec_rc2cbc_parameter_template, &(algid->parameters));
                if (rv)
                    goto loser;

                if (initVector.Length != rc2.iv.Length) {
                    PORT_Free(rc2.iv.Data);
                    rc2.iv.Data = NULL;
                    PORT_Free(rc2.rc2ParameterVersion.Data);
                    rc2.rc2ParameterVersion.Data = NULL;
                    goto loser;
                }
                memcpy(initVector.Data, rc2.iv.Data, initVector.Length);
                PORT_Free(rc2.iv.Data);
                rc2.iv.Data = NULL;

                ulEffectiveBits = rc2_map(&rc2.rc2ParameterVersion);
                PORT_Free(rc2.rc2ParameterVersion.Data);
                rc2.rc2ParameterVersion.Data = NULL;
                if (ulEffectiveBits != cssmKey->KeyHeader.LogicalKeySizeInBits)
                    goto loser;
                break;
            }
            case SEC_OID_RC5_CBC_PAD:
            default:
                // @@@ Implement rc5 params stuff.
                goto loser;
                break;
        }
    }

    if (CSSM_CSP_CreateSymmetricContext(cspHandle,
                                        algorithm,
                                        mode,
                                        NULL, /* accessCred */
                                        cssmKey,
                                        &initVector,
                                        padding,
                                        NULL, /* reserved */
                                        &ciphercc)) {
        goto loser;
    }

    if (encrypt) {
        rv = CSSM_EncryptDataInit(ciphercc);
    } else {
        rv = CSSM_DecryptDataInit(ciphercc);
    }
    if (rv) {
        goto loser;
    }

    cc = (SecCmsCipherContextRef)PORT_ZAlloc(sizeof(SecCmsCipherContext));
    if (cc == NULL) {
        goto loser;
    }

    cc->cc = ciphercc;
    cc->encrypt = encrypt;

    return cc;
loser:
    if (ciphercc) {
        CSSM_DeleteContext(ciphercc);
    }

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
SecCmsCipherContextRef SecCmsCipherContextStartDecrypt(SecSymmetricKeyRef key, SECAlgorithmID* algid)
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
SecCmsCipherContextStartEncrypt(PRArenaPool* poolp, SecSymmetricKeyRef key, SECAlgorithmID* algid)
{
    return SecCmsCipherContextStart(poolp, key, algid, PR_TRUE);
}

void SecCmsCipherContextDestroy(SecCmsCipherContextRef cc)
{
    PORT_Assert(cc != NULL);
    if (cc == NULL) {
        return;
    }
    CSSM_DeleteContext(cc->cc);
    PORT_Free(cc);
}

static unsigned int
SecCmsCipherContextLength(SecCmsCipherContextRef cc, unsigned int input_len, Boolean final, Boolean encrypt)
{
    CSSM_QUERY_SIZE_DATA dataBlockSize[2] = {{input_len, 0}, {input_len, 0}};
    /* Hack CDSA treats the last block as the final one.  So unless we are being asked to report the final size we ask for 2 block and ignore the second (final) one. */
    OSStatus rv = CSSM_QuerySize(cc->cc, cc->encrypt, final ? 1 : 2, dataBlockSize);
    if (rv) {
        PORT_SetError(rv);
        return 0;
    }

    return dataBlockSize[0].SizeOutputBlock;
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
size_t SecCmsCipherContextDecryptLength(SecCmsCipherContextRef cc, size_t input_len, Boolean final)
{
    return SecCmsCipherContextLength(cc, (unsigned int)input_len, final, PR_FALSE);
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
size_t SecCmsCipherContextEncryptLength(SecCmsCipherContextRef cc, size_t input_len, Boolean final)
{
    return SecCmsCipherContextLength(cc, (unsigned int)input_len, final, PR_TRUE);
}


static OSStatus SecCmsCipherContextCrypt(SecCmsCipherContextRef cc,
                                         unsigned char* output,
                                         size_t* output_len_p,
                                         size_t max_output_len,
                                         const unsigned char* input,
                                         size_t input_len,
                                         Boolean final,
                                         Boolean encrypt)
{
    CSSM_DATA outputBuf = {max_output_len, output};
    CSSM_SIZE bytes_output = 0;
    OSStatus rv = 0;

    if (input_len) {
        CSSM_DATA inputBuf = {input_len, (uint8*)input};

        if (encrypt) {
            rv = CSSM_EncryptDataUpdate(cc->cc, &inputBuf, 1, &outputBuf, 1, &bytes_output);
        } else {
            rv = CSSM_DecryptDataUpdate(cc->cc, &inputBuf, 1, &outputBuf, 1, &bytes_output);
        }
    }

    if (!rv && final) {
        CSSM_DATA remainderBuf = {max_output_len - bytes_output, output + bytes_output};
        if (encrypt) {
            rv = CSSM_EncryptDataFinal(cc->cc, &remainderBuf);
        } else {
            rv = CSSM_DecryptDataFinal(cc->cc, &remainderBuf);
        }

        bytes_output += remainderBuf.Length;
    }

    if (rv) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
    } else if (output_len_p) {
        *output_len_p = bytes_output;
    }

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
OSStatus SecCmsCipherContextDecrypt(SecCmsCipherContextRef cc,
                                    unsigned char* output,
                                    size_t* output_len_p,
                                    size_t max_output_len,
                                    const unsigned char* input,
                                    size_t input_len,
                                    Boolean final)
{
    return SecCmsCipherContextCrypt(
        cc, output, output_len_p, max_output_len, input, input_len, final, PR_FALSE);
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
OSStatus SecCmsCipherContextEncrypt(SecCmsCipherContextRef cc,
                                    unsigned char* output,
                                    size_t* output_len_p,
                                    size_t max_output_len,
                                    const unsigned char* input,
                                    size_t input_len,
                                    Boolean final)
{
    return SecCmsCipherContextCrypt(
        cc, output, output_len_p, max_output_len, input, input_len, final, PR_TRUE);
}
