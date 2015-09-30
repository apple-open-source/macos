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

#include "cmslocal.h"

#include "secoid.h"
#include <security_asn1/secerr.h>
#include <security_asn1/secasn1.h>
#include <Security/SecAsn1Templates.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/SecKeyPriv.h>

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
    CSSM_CC_HANDLE	cc;			/* CSP CONTEXT */
    Boolean		encrypt;		/* encrypt / decrypt switch */
#else
    void *		cx;			/* PK11 cipher context */
    nss_cms_cipher_function doit;
    nss_cms_cipher_destroy destroy;
    Boolean		encrypt;		/* encrypt / decrypt switch */
    int			block_size;		/* block & pad sizes for cipher */
    int			pad_size;
    int			pending_count;		/* pending data (not yet en/decrypted */
    unsigned char	pending_buf[BLOCK_SIZE];/* because of blocking */
#endif
};

typedef struct sec_rc2cbcParameterStr {
    SECItem rc2ParameterVersion;
    SECItem iv;
} sec_rc2cbcParameter;

static const SecAsn1Template sec_rc2cbc_parameter_template[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(sec_rc2cbcParameter) },
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT,
          offsetof(sec_rc2cbcParameter,rc2ParameterVersion) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(sec_rc2cbcParameter,iv) },
    { 0 }
};

/*
** Convert a der encoded *signed* integer into a machine integral value.
** If an underflow/overflow occurs, sets error code and returns min/max.
*/
static long
DER_GetInteger(SECItem *it)
{
    long ival = 0;
    CSSM_SIZE len = it->Length;
    unsigned char *cp = it->Data;
    unsigned long overflow = 0x1ffUL << (((sizeof(ival) - 1) * 8) - 1);
    unsigned long ofloinit;

    if (*cp & 0x80)
        ival = -1L;
    ofloinit = ival & overflow;

    while (len) {
        if ((ival & overflow) != ofloinit) {
            PORT_SetError(SEC_ERROR_BAD_DER);
            if (ival < 0) {
                return LONG_MIN;
            }
            return LONG_MAX;
        }
        ival = ival << 8;
        ival |= *cp++;
        --len;
    }
    return ival;
}

/* S/MIME picked id values to represent differnt keysizes */      
/* I do have a formula, but it ain't pretty, and it only works because you
 * can always match three points to a parabola:) */
static unsigned char  rc2_map(SECItem *version)
{
    long x;

    x = DER_GetInteger(version);

    switch (x) {
        case 58: return 128;
        case 120: return 64;
        case 160: return 40;
    }
    return 128;     
}

static unsigned long  rc2_unmap(unsigned long x)
{
    switch (x) {
        case 128: return 58;
        case 64: return 120;
        case 40: return 160;
    }
    return 58;
}

/* default IV size in bytes */
#define DEFAULT_IV_SIZE	    8
/* IV/block size for AES */
#define AES_BLOCK_SIZE	    16
/* max IV size in bytes */
#define MAX_IV_SIZE	    AES_BLOCK_SIZE

static SecCmsCipherContextRef
SecCmsCipherContextStart(PRArenaPool *poolp, SecSymmetricKeyRef key, SECAlgorithmID *algid, Boolean encrypt)
{
    SecCmsCipherContextRef cc;
    CSSM_CC_HANDLE ciphercc = 0;
    SECOidData *oidData;
    SECOidTag algtag;
    CSSM_ALGORITHMS algorithm;
    CSSM_PADDING padding = CSSM_PADDING_PKCS7;
    CSSM_ENCRYPT_MODE mode;
    CSSM_CSP_HANDLE cspHandle;
    const CSSM_KEY *cssmKey;
    OSStatus rv;
    uint8 ivbuf[MAX_IV_SIZE];
    CSSM_DATA initVector = { DEFAULT_IV_SIZE, ivbuf };
    //CSSM_CONTEXT_ATTRIBUTE contextAttribute = { CSSM_ATTRIBUTE_ALG_PARAMS, sizeof(CSSM_DATA_PTR) };

    rv = SecKeyGetCSPHandle(key, &cspHandle);
    if (rv)
	goto loser;
    rv = SecKeyGetCSSMKey(key, &cssmKey);
    if (rv)
	goto loser;

    // @@@ Add support for PBE based stuff

    oidData = SECOID_FindOID(&algid->algorithm);
    if (!oidData)
	goto loser;
    algtag = oidData->offset;
    algorithm = oidData->cssmAlgorithm;
    if (!algorithm)
	goto loser;

    switch (algtag)
    {
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

    if (encrypt)
    {
	CSSM_CC_HANDLE randomcc;
	//SECItem *parameters;

	// Generate random initVector
	if (CSSM_CSP_CreateRandomGenContext(cspHandle,
		CSSM_ALGID_APPLE_YARROW,
		NULL, /* seed*/
		initVector.Length,
		&randomcc))
	    goto loser;

	if (CSSM_GenerateRandom(randomcc, &initVector))
	    goto loser;
	CSSM_DeleteContext(randomcc);

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
	{
	    sec_rc2cbcParameter rc2 = {};
	    unsigned long rc2version;
	    SECItem *newParams;

	    rc2.iv = initVector;
	    rc2version = rc2_unmap(cssmKey->KeyHeader.LogicalKeySizeInBits);
	    if (!SEC_ASN1EncodeUnsignedInteger (NULL, &(rc2.rc2ParameterVersion),
					       rc2version))
		goto loser;
	    newParams = SEC_ASN1EncodeItem (poolp, &algid->parameters, &rc2,
				sec_rc2cbc_parameter_template);
	    PORT_Free(rc2.rc2ParameterVersion.Data);
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
	    CSSM_DATA iv = {};
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
	{
	    sec_rc2cbcParameter rc2 = {};
	    unsigned long ulEffectiveBits;

	    rv = SEC_ASN1DecodeItem(NULL, &rc2 ,sec_rc2cbc_parameter_template,
							    &(algid->parameters));
	    if (rv)
		goto loser;

	    if (initVector.Length != rc2.iv.Length) {
		PORT_Free(rc2.iv.Data);
		PORT_Free(rc2.rc2ParameterVersion.Data);
		goto loser;
	    }
	    memcpy(initVector.Data, rc2.iv.Data, initVector.Length);
	    PORT_Free(rc2.iv.Data);

	    ulEffectiveBits = rc2_map(&rc2.rc2ParameterVersion);
	    PORT_Free(rc2.rc2ParameterVersion.Data);
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
	    &ciphercc))
	goto loser;

    if (encrypt)
	rv = CSSM_EncryptDataInit(ciphercc);
    else
	rv = CSSM_DecryptDataInit(ciphercc);
    if (rv)
	goto loser;

    cc = (SecCmsCipherContextRef)PORT_ZAlloc(sizeof(SecCmsCipherContext));
    if (cc == NULL)
	goto loser;

    cc->cc = ciphercc;
    cc->encrypt = encrypt;

    return cc;
loser:
    if (ciphercc)
	CSSM_DeleteContext(ciphercc);

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
#if 0
    SecCmsCipherContextRef cc;
    void *ciphercx;
    CK_MECHANISM_TYPE mechanism;
    CSSM_DATA_PTR param;
    PK11SlotInfo *slot;
    SECOidTag algtag;

    algtag = SECOID_GetAlgorithmTag(algid);

    /* set param and mechanism */
    if (SEC_PKCS5IsAlgorithmPBEAlg(algid)) {
	CK_MECHANISM pbeMech, cryptoMech;
	CSSM_DATA_PTR pbeParams;
	SEC_PKCS5KeyAndPassword *keyPwd;

	PORT_Memset(&pbeMech, 0, sizeof(CK_MECHANISM));
	PORT_Memset(&cryptoMech, 0, sizeof(CK_MECHANISM));

	/* HACK ALERT!
	 * in this case, key is not actually a SecSymmetricKeyRef, but a SEC_PKCS5KeyAndPassword *
	 */
	keyPwd = (SEC_PKCS5KeyAndPassword *)key;
	key = keyPwd->key;

	/* find correct PK11 mechanism and parameters to initialize pbeMech */
	pbeMech.mechanism = PK11_AlgtagToMechanism(algtag);
	pbeParams = PK11_ParamFromAlgid(algid);
	if (!pbeParams)
	    return NULL;
	pbeMech.pParameter = pbeParams->Data;
	pbeMech.ulParameterLen = pbeParams->Length;

	/* now map pbeMech to cryptoMech */
	if (PK11_MapPBEMechanismToCryptoMechanism(&pbeMech, &cryptoMech, keyPwd->pwitem,
						  PR_FALSE) != CKR_OK) { 
	    SECITEM_ZfreeItem(pbeParams, PR_TRUE);
	    return NULL;
	}
	SECITEM_ZfreeItem(pbeParams, PR_TRUE);

	/* and use it to initialize param & mechanism */
	if ((param = (CSSM_DATA_PTR)PORT_ZAlloc(sizeof(CSSM_DATA))) == NULL)
	     return NULL;

	param->Data = (unsigned char *)cryptoMech.pParameter;
	param->Length = cryptoMech.ulParameterLen;
	mechanism = cryptoMech.mechanism;
    } else {
	mechanism = PK11_AlgtagToMechanism(algtag);
	if ((param = PK11_ParamFromAlgid(algid)) == NULL)
	    return NULL;
    }

    cc = (SecCmsCipherContextRef)PORT_ZAlloc(sizeof(SecCmsCipherContext));
    if (cc == NULL) {
	SECITEM_FreeItem(param,PR_TRUE);
	return NULL;
    }

    /* figure out pad and block sizes */
    cc->pad_size = PK11_GetBlockSize(mechanism, param);
    slot = PK11_GetSlotFromKey(key);
    cc->block_size = PK11_IsHW(slot) ? BLOCK_SIZE : cc->pad_size;
    PK11_FreeSlot(slot);

    /* create PK11 cipher context */
    ciphercx = PK11_CreateContextBySymKey(mechanism, CKA_DECRYPT, key, param);
    SECITEM_FreeItem(param, PR_TRUE);
    if (ciphercx == NULL) {
	PORT_Free (cc);
	return NULL;
    }

    cc->cx = ciphercx;
    cc->doit =  (nss_cms_cipher_function) PK11_CipherOp;
    cc->destroy = (nss_cms_cipher_destroy) PK11_DestroyContext;
    cc->encrypt = PR_FALSE;
    cc->pending_count = 0;

    return cc;
#endif
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
#if 0
    SecCmsCipherContextRef cc;
    void *ciphercx;
    CSSM_DATA_PTR param;
    OSStatus rv;
    CK_MECHANISM_TYPE mechanism;
    PK11SlotInfo *slot;
    Boolean needToEncodeAlgid = PR_FALSE;
    SECOidTag algtag = SECOID_GetAlgorithmTag(algid);

    /* set param and mechanism */
    if (SEC_PKCS5IsAlgorithmPBEAlg(algid)) {
	CK_MECHANISM pbeMech, cryptoMech;
	CSSM_DATA_PTR pbeParams;
	SEC_PKCS5KeyAndPassword *keyPwd;

	PORT_Memset(&pbeMech, 0, sizeof(CK_MECHANISM));
	PORT_Memset(&cryptoMech, 0, sizeof(CK_MECHANISM));

	/* HACK ALERT!
	 * in this case, key is not actually a SecSymmetricKeyRef, but a SEC_PKCS5KeyAndPassword *
	 */
	keyPwd = (SEC_PKCS5KeyAndPassword *)key;
	key = keyPwd->key;

	/* find correct PK11 mechanism and parameters to initialize pbeMech */
	pbeMech.mechanism = PK11_AlgtagToMechanism(algtag);
	pbeParams = PK11_ParamFromAlgid(algid);
	if (!pbeParams)
	    return NULL;
	pbeMech.pParameter = pbeParams->Data;
	pbeMech.ulParameterLen = pbeParams->Length;

	/* now map pbeMech to cryptoMech */
	if (PK11_MapPBEMechanismToCryptoMechanism(&pbeMech, &cryptoMech, keyPwd->pwitem,
						  PR_FALSE) != CKR_OK) { 
	    SECITEM_ZfreeItem(pbeParams, PR_TRUE);
	    return NULL;
	}
	SECITEM_ZfreeItem(pbeParams, PR_TRUE);

	/* and use it to initialize param & mechanism */
	if ((param = (CSSM_DATA_PTR)PORT_ZAlloc(sizeof(CSSM_DATA))) == NULL)
	    return NULL;

	param->Data = (unsigned char *)cryptoMech.pParameter;
	param->Length = cryptoMech.ulParameterLen;
	mechanism = cryptoMech.mechanism;
    } else {
	mechanism = PK11_AlgtagToMechanism(algtag);
	if ((param = PK11_GenerateNewParam(mechanism, key)) == NULL)
	    return NULL;
	needToEncodeAlgid = PR_TRUE;
    }

    cc = (SecCmsCipherContextRef)PORT_ZAlloc(sizeof(SecCmsCipherContext));
    if (cc == NULL)
	return NULL;

    /* now find pad and block sizes for our mechanism */
    cc->pad_size = PK11_GetBlockSize(mechanism,param);
    slot = PK11_GetSlotFromKey(key);
    cc->block_size = PK11_IsHW(slot) ? BLOCK_SIZE : cc->pad_size;
    PK11_FreeSlot(slot);

    /* and here we go, creating a PK11 cipher context */
    ciphercx = PK11_CreateContextBySymKey(mechanism, CKA_ENCRYPT, key, param);
    if (ciphercx == NULL) {
	PORT_Free(cc);
	cc = NULL;
	goto loser;
    }

    /*
     * These are placed after the CreateContextBySymKey() because some
     * mechanisms have to generate their IVs from their card (i.e. FORTEZZA).
     * Don't move it from here.
     * XXX is that right? the purpose of this is to get the correct algid
     *     containing the IVs etc. for encoding. this means we need to set this up
     *     BEFORE encoding the algid in the contentInfo, right?
     */
    if (needToEncodeAlgid) {
	rv = PK11_ParamToAlgid(algtag, param, poolp, algid);
	if(rv != SECSuccess) {
	    PORT_Free(cc);
	    cc = NULL;
	    goto loser;
	}
    }

    cc->cx = ciphercx;
    cc->doit = (nss_cms_cipher_function)PK11_CipherOp;
    cc->destroy = (nss_cms_cipher_destroy)PK11_DestroyContext;
    cc->encrypt = PR_TRUE;
    cc->pending_count = 0;

loser:
    SECITEM_FreeItem(param, PR_TRUE);

    return cc;
#endif
}

void
SecCmsCipherContextDestroy(SecCmsCipherContextRef cc)
{
    PORT_Assert(cc != NULL);
    if (cc == NULL)
	return;
    CSSM_DeleteContext(cc->cc);
    PORT_Free(cc);
}

static unsigned int
SecCmsCipherContextLength(SecCmsCipherContextRef cc, unsigned int input_len, Boolean final, Boolean encrypt)
{
    CSSM_QUERY_SIZE_DATA dataBlockSize[2] = { { input_len, 0 }, { input_len, 0 } };
    /* Hack CDSA treats the last block as the final one.  So unless we are being asked to report the final size we ask for 2 block and ignore the second (final) one. */
    OSStatus rv = CSSM_QuerySize(cc->cc, cc->encrypt, final ? 1 : 2, dataBlockSize);
    if (rv)
    {
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
size_t
SecCmsCipherContextDecryptLength(SecCmsCipherContextRef cc, size_t input_len, Boolean final)
{
#if 1
    return SecCmsCipherContextLength(cc, (unsigned int)input_len, final, PR_FALSE);
#else
    int blocks, block_size;

    PORT_Assert (! cc->encrypt);

    block_size = cc->block_size;

    /*
     * If this is not a block cipher, then we always have the same
     * number of output bytes as we had input bytes.
     */
    if (block_size == 0)
	return input_len;

    /*
     * On the final call, we will always use up all of the pending
     * bytes plus all of the input bytes, *but*, there will be padding
     * at the end and we cannot predict how many bytes of padding we
     * will end up removing.  The amount given here is actually known
     * to be at least 1 byte too long (because we know we will have
     * at least 1 byte of padding), but seemed clearer/better to me.
     */
    if (final)
	return cc->pending_count + input_len;

    /*
     * Okay, this amount is exactly what we will output on the
     * next cipher operation.  We will always hang onto the last
     * 1 - block_size bytes for non-final operations.  That is,
     * we will do as many complete blocks as we can *except* the
     * last block (complete or partial).  (This is because until
     * we know we are at the end, we cannot know when to interpret
     * and removing the padding byte(s), which are guaranteed to
     * be there.)
     */
    blocks = (cc->pending_count + input_len - 1) / block_size;
    return blocks * block_size;
#endif
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
size_t
SecCmsCipherContextEncryptLength(SecCmsCipherContextRef cc, size_t input_len, Boolean final)
{
#if 1
    return SecCmsCipherContextLength(cc, (unsigned int)input_len, final, PR_TRUE);
#else
    int blocks, block_size;
    int pad_size;

    PORT_Assert (cc->encrypt);

    block_size = cc->block_size;
    pad_size = cc->pad_size;

    /*
     * If this is not a block cipher, then we always have the same
     * number of output bytes as we had input bytes.
     */
    if (block_size == 0)
	return input_len;

    /*
     * On the final call, we only send out what we need for
     * remaining bytes plus the padding.  (There is always padding,
     * so even if we have an exact number of blocks as input, we
     * will add another full block that is just padding.)
     */
    if (final) {
	if (pad_size == 0) {
    	    return cc->pending_count + input_len;
	} else {
    	    blocks = (cc->pending_count + input_len) / pad_size;
	    blocks++;
	    return blocks*pad_size;
	}
    }

    /*
     * Now, count the number of complete blocks of data we have.
     */
    blocks = (cc->pending_count + input_len) / block_size;


    return blocks * block_size;
#endif
}


static OSStatus
SecCmsCipherContextCrypt(SecCmsCipherContextRef cc, unsigned char *output,
		  size_t *output_len_p, size_t max_output_len,
		  const unsigned char *input, size_t input_len,
		  Boolean final, Boolean encrypt)
{
    CSSM_DATA outputBuf = { max_output_len, output };
    CSSM_SIZE bytes_output = 0;
    OSStatus rv = 0;

    if (input_len)
    {
	CSSM_DATA inputBuf = { input_len, (uint8 *)input };

	if (encrypt)
	    rv = CSSM_EncryptDataUpdate(cc->cc, &inputBuf, 1, &outputBuf, 1, &bytes_output);
	else
	    rv = CSSM_DecryptDataUpdate(cc->cc, &inputBuf, 1, &outputBuf, 1, &bytes_output);
    }

    if (!rv && final)
    {
	CSSM_DATA remainderBuf = { max_output_len - bytes_output, output + bytes_output };
	if (encrypt)
	    rv = CSSM_EncryptDataFinal(cc->cc, &remainderBuf);
	else
	    rv = CSSM_DecryptDataFinal(cc->cc, &remainderBuf);

	bytes_output += remainderBuf.Length;
    }

    if (rv)
	PORT_SetError(SEC_ERROR_BAD_DATA);
    else if (output_len_p)
	*output_len_p = bytes_output;

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
		  size_t *output_len_p, size_t max_output_len,
		  const unsigned char *input, size_t input_len,
		  Boolean final)
{
#if 1
    return SecCmsCipherContextCrypt(cc, output,
		  output_len_p,  max_output_len,
		  input, input_len,
		  final, PR_FALSE);
#else
    int blocks, bsize, pcount, padsize;
    unsigned int max_needed, ifraglen, ofraglen, output_len;
    unsigned char *pbuf;
    OSStatus rv;

    PORT_Assert (! cc->encrypt);

    /*
     * Check that we have enough room for the output.  Our caller should
     * already handle this; failure is really an internal error (i.e. bug).
     */
    max_needed = SecCmsCipherContextDecryptLength(cc, input_len, final);
    PORT_Assert (max_output_len >= max_needed);
    if (max_output_len < max_needed) {
	/* PORT_SetError (XXX); */
	return SECFailure;
    }

    /*
     * hardware encryption does not like small decryption sizes here, so we
     * allow both blocking and padding.
     */
    bsize = cc->block_size;
    padsize = cc->pad_size;

    /*
     * When no blocking or padding work to do, we can simply call the
     * cipher function and we are done.
     */
    if (bsize == 0) {
	return (* cc->doit) (cc->cx, output, output_len_p, max_output_len,
			      input, input_len);
    }

    pcount = cc->pending_count;
    pbuf = cc->pending_buf;

    output_len = 0;

    if (pcount) {
	/*
	 * Try to fill in an entire block, starting with the bytes
	 * we already have saved away.
	 */
	while (input_len && pcount < bsize) {
	    pbuf[pcount++] = *input++;
	    input_len--;
	}
	/*
	 * If we have at most a whole block and this is not our last call,
	 * then we are done for now.  (We do not try to decrypt a lone
	 * single block because we cannot interpret the padding bytes
	 * until we know we are handling the very last block of all input.)
	 */
	if (input_len == 0 && !final) {
	    cc->pending_count = pcount;
	    if (output_len_p)
		*output_len_p = 0;
	    return SECSuccess;
	}
	/*
	 * Given the logic above, we expect to have a full block by now.
	 * If we do not, there is something wrong, either with our own
	 * logic or with (length of) the data given to us.
	 */
	if ((padsize != 0) && (pcount % padsize) != 0) {
	    PORT_Assert (final);	
	    PORT_SetError (SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}
	/*
	 * Decrypt the block.
	 */
	rv = (*cc->doit)(cc->cx, output, &ofraglen, max_output_len,
			    pbuf, pcount);
	if (rv != SECSuccess)
	    return rv;

	/*
	 * For now anyway, all of our ciphers have the same number of
	 * bytes of output as they do input.  If this ever becomes untrue,
	 * then SecCmsCipherContextDecryptLength needs to be made smarter!
	 */
	PORT_Assert(ofraglen == pcount);

	/*
	 * Account for the bytes now in output.
	 */
	max_output_len -= ofraglen;
	output_len += ofraglen;
	output += ofraglen;
    }

    /*
     * If this is our last call, we expect to have an exact number of
     * blocks left to be decrypted; we will decrypt them all.
     * 
     * If not our last call, we always save between 1 and bsize bytes
     * until next time.  (We must do this because we cannot be sure
     * that none of the decrypted bytes are padding bytes until we
     * have at least another whole block of data.  You cannot tell by
     * looking -- the data could be anything -- you can only tell by
     * context, knowing you are looking at the last block.)  We could
     * decrypt a whole block now but it is easier if we just treat it
     * the same way we treat partial block bytes.
     */
    if (final) {
	if (padsize) {
	    blocks = input_len / padsize;
	    ifraglen = blocks * padsize;
	} else ifraglen = input_len;
	PORT_Assert (ifraglen == input_len);

	if (ifraglen != input_len) {
	    PORT_SetError(SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}
    } else {
	blocks = (input_len - 1) / bsize;
	ifraglen = blocks * bsize;
	PORT_Assert (ifraglen < input_len);

	pcount = input_len - ifraglen;
	PORT_Memcpy (pbuf, input + ifraglen, pcount);
	cc->pending_count = pcount;
    }

    if (ifraglen) {
	rv = (* cc->doit)(cc->cx, output, &ofraglen, max_output_len,
			    input, ifraglen);
	if (rv != SECSuccess)
	    return rv;

	/*
	 * For now anyway, all of our ciphers have the same number of
	 * bytes of output as they do input.  If this ever becomes untrue,
	 * then sec_PKCS7DecryptLength needs to be made smarter!
	 */
	PORT_Assert (ifraglen == ofraglen);
	if (ifraglen != ofraglen) {
	    PORT_SetError(SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}

	output_len += ofraglen;
    } else {
	ofraglen = 0;
    }

    /*
     * If we just did our very last block, "remove" the padding by
     * adjusting the output length.
     */
    if (final && (padsize != 0)) {
	unsigned int padlen = *(output + ofraglen - 1);

	if (padlen == 0 || padlen > padsize) {
	    PORT_SetError(SEC_ERROR_BAD_DATA);
	    return SECFailure;
	}
	output_len -= padlen;
    }

    PORT_Assert (output_len_p != NULL || output_len == 0);
    if (output_len_p != NULL)
	*output_len_p = output_len;

    return SECSuccess;
#endif
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
		  size_t *output_len_p, size_t max_output_len,
		  const unsigned char *input, size_t input_len,
		  Boolean final)
{
#if 1
    return SecCmsCipherContextCrypt(cc, output,
		  output_len_p,  max_output_len,
		  input, input_len,
		  final, PR_TRUE);
#else
    int blocks, bsize, padlen, pcount, padsize;
    unsigned int max_needed, ifraglen, ofraglen, output_len;
    unsigned char *pbuf;
    OSStatus rv;

    PORT_Assert (cc->encrypt);

    /*
     * Check that we have enough room for the output.  Our caller should
     * already handle this; failure is really an internal error (i.e. bug).
     */
    max_needed = SecCmsCipherContextEncryptLength (cc, input_len, final);
    PORT_Assert (max_output_len >= max_needed);
    if (max_output_len < max_needed) {
	/* PORT_SetError (XXX); */
	return SECFailure;
    }

    bsize = cc->block_size;
    padsize = cc->pad_size;

    /*
     * When no blocking and padding work to do, we can simply call the
     * cipher function and we are done.
     */
    if (bsize == 0) {
	return (*cc->doit)(cc->cx, output, output_len_p, max_output_len,
			      input, input_len);
    }

    pcount = cc->pending_count;
    pbuf = cc->pending_buf;

    output_len = 0;

    if (pcount) {
	/*
	 * Try to fill in an entire block, starting with the bytes
	 * we already have saved away.
	 */
	while (input_len && pcount < bsize) {
	    pbuf[pcount++] = *input++;
	    input_len--;
	}
	/*
	 * If we do not have a full block and we know we will be
	 * called again, then we are done for now.
	 */
	if (pcount < bsize && !final) {
	    cc->pending_count = pcount;
	    if (output_len_p != NULL)
		*output_len_p = 0;
	    return SECSuccess;
	}
	/*
	 * If we have a whole block available, encrypt it.
	 */
	if ((padsize == 0) || (pcount % padsize) == 0) {
	    rv = (* cc->doit) (cc->cx, output, &ofraglen, max_output_len,
				pbuf, pcount);
	    if (rv != SECSuccess)
		return rv;

	    /*
	     * For now anyway, all of our ciphers have the same number of
	     * bytes of output as they do input.  If this ever becomes untrue,
	     * then sec_PKCS7EncryptLength needs to be made smarter!
	     */
	    PORT_Assert (ofraglen == pcount);

	    /*
	     * Account for the bytes now in output.
	     */
	    max_output_len -= ofraglen;
	    output_len += ofraglen;
	    output += ofraglen;

	    pcount = 0;
	}
    }

    if (input_len) {
	PORT_Assert (pcount == 0);

	blocks = input_len / bsize;
	ifraglen = blocks * bsize;

	if (ifraglen) {
	    rv = (* cc->doit) (cc->cx, output, &ofraglen, max_output_len,
				input, ifraglen);
	    if (rv != SECSuccess)
		return rv;

	    /*
	     * For now anyway, all of our ciphers have the same number of
	     * bytes of output as they do input.  If this ever becomes untrue,
	     * then sec_PKCS7EncryptLength needs to be made smarter!
	     */
	    PORT_Assert (ifraglen == ofraglen);

	    max_output_len -= ofraglen;
	    output_len += ofraglen;
	    output += ofraglen;
	}

	pcount = input_len - ifraglen;
	PORT_Assert (pcount < bsize);
	if (pcount)
	    PORT_Memcpy (pbuf, input + ifraglen, pcount);
    }

    if (final) {
	padlen = padsize - (pcount % padsize);
	PORT_Memset (pbuf + pcount, padlen, padlen);
	rv = (* cc->doit) (cc->cx, output, &ofraglen, max_output_len,
			    pbuf, pcount+padlen);
	if (rv != SECSuccess)
	    return rv;

	/*
	 * For now anyway, all of our ciphers have the same number of
	 * bytes of output as they do input.  If this ever becomes untrue,
	 * then sec_PKCS7EncryptLength needs to be made smarter!
	 */
	PORT_Assert (ofraglen == (pcount+padlen));
	output_len += ofraglen;
    } else {
	cc->pending_count = pcount;
    }

    PORT_Assert (output_len_p != NULL || output_len == 0);
    if (output_len_p != NULL)
	*output_len_p = output_len;

    return SECSuccess;
#endif
}
