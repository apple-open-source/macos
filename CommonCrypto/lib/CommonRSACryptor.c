/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

// #define COMMON_RSA_FUNCTIONS
#include "CommonRSACryptor.h"
#include "CommonDigest.h"
#include "CommonDigestSPI.h"
#include "CommonDigestPriv.h"

#include "CommonRandomSPI.h"
#include <corecrypto/ccrsa.h>
#include <corecrypto/ccrsa_priv.h>
#include <corecrypto/ccasn1.h>

#if 0
#include "asn1Types.h"
#include "DER_Keys.h"
#include "DER_Encode.h"
#endif


#include "ccErrors.h"
#include "ccMemory.h"
#include "ccdebug.h"
#include <AssertMacros.h>

#pragma mark internal

#define kCCMaximumRSAKeyBits 4096
#define kCCMaximumRSAKeyBytes ccn_sizeof(kCCMaximumRSAKeyBits)
#define kCCRSAKeyContextSize ccrsa_full_ctx_size(kCCMaximumRSAKeyBytes)
#define RSA_PKCS1_PAD_ENCRYPT	0x02


typedef struct _CCRSACryptor {
    ccrsa_full_ctx_decl(kCCMaximumRSAKeyBits, fk);
    size_t keySize;
    CCRSAKeyType keyType;
} CCRSACryptor;

static CCRSACryptor *
ccMallocRSACryptor(size_t nbits, CCRSAKeyType keyType)
{
    CCRSACryptor *retval;
    cc_size n = ccn_nof(nbits);
    if((retval = CC_XMALLOC(sizeof(CCRSACryptor))) == NULL) return NULL;
    retval->keySize = nbits;
    ccrsa_ctx_n(retval->fk) = n;
    return retval;
}

static void
ccRSACryptorClear(CCRSACryptorRef theKey)
{
    CCRSACryptor *key = (CCRSACryptor *) theKey;
    if(!key) return;
    CC_XZEROMEM(key, sizeof(CCRSACryptor));
    CC_XFREE(key, sizeof(CCRSACryptor));
}

static inline size_t
ccRSAkeysize(CCRSACryptor *cryptor) {
    return ccn_bitlen(ccrsa_ctx_n(cryptor->fk), ccrsa_ctx_m(cryptor->fk));
}

/*
 
 This is done for FIPS.  Basically we make sure that the two keys will work to encrypt/decrypt
 each other's data.  This will test up to 4K bit keys.
 
*/

#define MAXKEYTEST 512

static bool
ccRSApairwiseConsistencyCheck(CCRSACryptorRef privateKey, CCRSACryptorRef publicKey)
{
	CCCryptorStatus status = kCCSuccess;
	size_t theDataLen = MAXKEYTEST, resultLen, recoveryLen;
	uint8_t theData[MAXKEYTEST];
	uint8_t theResult[MAXKEYTEST];
	uint8_t theRecovered[MAXKEYTEST];

    /* 
     
     the RSA keysize had better be equal - we convert keysizes to bytes since we need to
     work with the appropriate size data buffers for tests.
     
     */
    
    theDataLen = CCRSAGetKeySize(privateKey) / 8;
    if(theDataLen > MAXKEYTEST || theDataLen != (size_t) (CCRSAGetKeySize(publicKey) / 8)) {
        return false;
    }
    
    /* Fill the input buffer for the test */
    
    CC_XMEMSET(theData, 0x0a, theDataLen);
    
    /* Encrypt the buffer with the private key then be sure the output isn't the same as the input */
    resultLen = theDataLen;
    status = CCRSACryptorCrypt(privateKey, theData, theDataLen, theResult, &resultLen);
        
	if (kCCSuccess != status) {
        return false;
    }
    
    if(CC_XMEMCMP(theData, theResult, theDataLen) == 0) { 
        return false; 
    }
    
    /* Decrypt the buffer with the public key and be sure the output matches the original input */
	
    recoveryLen = theDataLen;
    status = CCRSACryptorCrypt(publicKey, theResult, resultLen, theRecovered, &recoveryLen);

	if (kCCSuccess != status) { 
        return false; 
    }
    
    if(recoveryLen != theDataLen) { 
        return false; 
    }
    
    if(CC_XMEMCMP(theData, theRecovered, theDataLen) != 0) { 
        return false; 
    }
    
    /* Cleanup and leave */
    
    CC_XZEROMEM(theData, MAXKEYTEST);
    CC_XZEROMEM(theResult, MAXKEYTEST);
    CC_XZEROMEM(theRecovered, MAXKEYTEST);

	return true;	
}


#pragma mark APIDone

CCCryptorStatus 
CCRSACryptorGeneratePair(size_t keysize, uint32_t e, CCRSACryptorRef *publicKey, CCRSACryptorRef *privateKey)
{
    CCCryptorStatus retval;
    CCRSACryptor *privateCryptor = NULL;
    CCRSACryptor *publicCryptor = NULL;
    struct ccrng_state *theRng1 = ccDRBGGetRngState();
    struct ccrng_state *theRng2 = ccDevRandomGetRngState();
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    // ccrsa_generate_key() requires the exponent as length / pointer to bytes
    cc_unit cc_unit_e = (cc_unit) e;
    
    size_t eSize = ccn_write_int_size(1, &cc_unit_e);
    uint8_t eBytes[eSize];
    ccn_write_int(1, &cc_unit_e, eSize, eBytes);
    
    *publicKey = *privateKey = NULL;
    
    __Require_Action((privateCryptor = ccMallocRSACryptor(keysize, ccRSAKeyPrivate)) != NULL, errOut, retval = kCCMemoryFailure);
        
    // __Require_Action((ccrsa_generate_key(keysize, privateCryptor->rsaKey.full, sizeof(e), &e, theRng) == 0), errOut, retval = kCCDecodeError);
    __Require_Action((ccrsa_generate_931_key(keysize, privateCryptor->fk, eSize, eBytes, theRng1, theRng2) == 0), errOut, retval = kCCDecodeError);
    
    privateCryptor->keyType = ccRSAKeyPrivate;
    
    __Require_Action((publicCryptor = CCRSACryptorGetPublicKeyFromPrivateKey(privateCryptor)) != NULL, errOut, retval = kCCMemoryFailure);
    
    *publicKey = publicCryptor;
    *privateKey = privateCryptor;
    
    __Require_Action(ccRSApairwiseConsistencyCheck(*privateKey, *publicKey) == true, errOut, retval = kCCDecodeError);

    return kCCSuccess;
    
errOut:
    if(privateCryptor) ccRSACryptorClear(privateCryptor);
    if(publicCryptor) ccRSACryptorClear(publicCryptor);
    *publicKey = *privateKey = NULL;
    return retval;
}

CCRSACryptorRef CCRSACryptorGetPublicKeyFromPrivateKey(CCRSACryptorRef privateCryptorRef)
{
    CCRSACryptor *publicCryptor = NULL, *privateCryptor = privateCryptorRef;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if((publicCryptor = ccMallocRSACryptor(privateCryptor->keySize, ccRSAKeyPublic)) == NULL)  return NULL;
    ccrsa_init_pub(ccrsa_ctx_public(publicCryptor->fk), ccrsa_ctx_m(privateCryptor->fk), ccrsa_ctx_e(privateCryptor->fk));
    publicCryptor->keyType = ccRSAKeyPublic;
    return publicCryptor;
}

CCRSAKeyType CCRSAGetKeyType(CCRSACryptorRef key)
{
    CCRSACryptor *cryptor = key;
    CCRSAKeyType retval;

    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(key == NULL) return ccRSABadKey;
    retval = cryptor->keyType;
    if(retval != ccRSAKeyPublic && retval != ccRSAKeyPrivate) return ccRSABadKey;
    return retval;
}

int CCRSAGetKeySize(CCRSACryptorRef key)
{
    CCRSACryptor *cryptor = key;
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(key == NULL) return kCCParamError;    
    
    return (int) cryptor->keySize;
}

void 
CCRSACryptorRelease(CCRSACryptorRef key)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    ccRSACryptorClear(key);
}


CCCryptorStatus CCRSACryptorImport(const void *keyPackage, size_t keyPackageLen, CCRSACryptorRef *key)
{
    CCRSACryptor *cryptor = NULL;
    CCCryptorStatus retval;
    CCRSAKeyType keyToMake;
    cc_size keyN;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!keyPackage || !key) return kCCParamError;
    if((keyN = ccrsa_import_priv_n(keyPackageLen, keyPackage)) != 0) keyToMake = ccRSAKeyPrivate;
    else if((keyN = ccrsa_import_pub_n(keyPackageLen, keyPackage)) != 0) keyToMake = ccRSAKeyPublic;
    else return kCCDecodeError;
    
    __Require_Action((cryptor = ccMallocRSACryptor(kCCMaximumRSAKeyBits, keyToMake)) != NULL, errOut, retval = kCCMemoryFailure);
    
    switch(keyToMake) {
        case ccRSAKeyPublic:
            ccrsa_ctx_n(ccrsa_ctx_public(cryptor->fk)) = keyN;
            if(ccrsa_import_pub(ccrsa_ctx_public(cryptor->fk), keyPackageLen, keyPackage)) {
                ccRSACryptorClear(cryptor);
                return kCCDecodeError;
            }
            break;
        case ccRSAKeyPrivate:
            ccrsa_ctx_n(cryptor->fk) = keyN;
            if(ccrsa_import_priv(cryptor->fk, keyPackageLen, keyPackage)) {
                ccRSACryptorClear(cryptor);
                return kCCDecodeError;
            }
            break;
    }
    cryptor->keyType = keyToMake;
    *key = cryptor;
    cryptor->keySize = ccRSAkeysize(cryptor);

    return kCCSuccess;
    
errOut:
    if(cryptor) ccRSACryptorClear(cryptor);
    *key = NULL;
    return retval;
}


CCCryptorStatus CCRSACryptorExport(CCRSACryptorRef cryptor, void *out, size_t *outLen)
{
    CCCryptorStatus retval = kCCSuccess;
    size_t bufsiz;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!cryptor || !out) return kCCParamError;
    switch(cryptor->keyType) {
        case ccRSAKeyPublic:
            bufsiz = ccrsa_export_pub_size(ccrsa_ctx_public(cryptor->fk));
            if(*outLen <= bufsiz) {
                *outLen = bufsiz;
                return kCCBufferTooSmall;
            }
            *outLen = bufsiz;
            if(ccrsa_export_pub(ccrsa_ctx_public(cryptor->fk), bufsiz, out))
                return kCCDecodeError;
            break;
        case ccRSAKeyPrivate:
            bufsiz = ccrsa_export_priv_size(cryptor->fk);
            if(*outLen < bufsiz) {
                *outLen = bufsiz;
                return kCCBufferTooSmall;
            }
            *outLen = bufsiz;
            if(ccrsa_export_priv(cryptor->fk, bufsiz, out))
                return kCCDecodeError;
            break;
        default:
            retval = kCCParamError;
    }
    return retval;
}







CCCryptorStatus 
CCRSACryptorEncrypt(CCRSACryptorRef publicKey, CCAsymmetricPadding padding, const void *plainText, size_t plainTextLen, void *cipherText, size_t *cipherTextLen,
	const void *tagData, size_t tagDataLen, CCDigestAlgorithm digestType)
{
    CCCryptorStatus retval = kCCSuccess;

    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!publicKey || !cipherText || !plainText || !cipherTextLen) return kCCParamError;
    
    switch(padding) {
        case ccPKCS1Padding:
            if(ccrsa_encrypt_eme_pkcs1v15(ccrsa_ctx_public(publicKey->fk), ccDRBGGetRngState(), cipherTextLen, cipherText, plainTextLen, (uint8_t *) plainText)  != 0)
                retval =  kCCDecodeError;
            break;
        case ccOAEPPadding:         
            if(ccrsa_encrypt_oaep(ccrsa_ctx_public(publicKey->fk), CCDigestGetDigestInfo(digestType), ccDRBGGetRngState(), cipherTextLen, cipherText, plainTextLen, (uint8_t *) plainText, tagDataLen, tagData) != 0) 
                retval =  kCCDecodeError;
            break;
        default:
            retval = kCCParamError;
            goto errOut;

    }
        
errOut:
    return retval;
}



CCCryptorStatus 
CCRSACryptorDecrypt(CCRSACryptorRef privateKey, CCAsymmetricPadding padding, const void *cipherText, size_t cipherTextLen,
				 void *plainText, size_t *plainTextLen, const void *tagData, size_t tagDataLen, CCDigestAlgorithm digestType)
{
    CCCryptorStatus retval = kCCSuccess;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!privateKey || !cipherText || !plainText || !plainTextLen) return kCCParamError;
    
    switch (padding) {
        case ccPKCS1Padding:
            if(ccrsa_decrypt_eme_pkcs1v15(privateKey->fk, plainTextLen, plainText, cipherTextLen, (uint8_t *) cipherText) != 0)
                retval =  kCCDecodeError;
            break;
        case ccOAEPPadding:
            if(ccrsa_decrypt_oaep(privateKey->fk, CCDigestGetDigestInfo(digestType), plainTextLen, plainText, cipherTextLen, (uint8_t *) cipherText,
                                  tagDataLen, tagData) != 0) 
                retval =  kCCDecodeError;
            break;
        default:
            goto errOut;
    }
    
errOut:
    
    return retval;
}

CCCryptorStatus 
CCRSACryptorCrypt(CCRSACryptorRef rsaKey, const void *in, size_t inLen, void *out, size_t *outLen)
{    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!rsaKey || !in || !out || !outLen) return kCCParamError;
    
    size_t keysizeBytes = (rsaKey->keySize+7)/8;
    
    if(inLen != keysizeBytes || *outLen < keysizeBytes) return kCCMemoryFailure;
    
    cc_size n = ccrsa_ctx_n(rsaKey->fk);
    cc_unit buf[n];
    ccn_read_uint(n, buf, inLen, in);
    
    switch(rsaKey->keyType) {
        case ccRSAKeyPublic: 
            ccrsa_pub_crypt(ccrsa_ctx_public(rsaKey->fk), buf, buf);
            break;
        case ccRSAKeyPrivate:
            ccrsa_priv_crypt(ccrsa_ctx_private(rsaKey->fk), buf, buf);
            break;
        default:
            return kCCParamError;
    }
    
    *outLen = keysizeBytes;
    ccn_write_uint_padded(n, buf, *outLen, out);
    return kCCSuccess;
}



static inline int cczp_read_uint(cczp_t r, size_t data_size, const uint8_t *data)
{
    if(ccn_read_uint(ccn_nof_size(data_size), CCZP_PRIME(r), data_size, data) != 0) return -1;
    CCZP_N(r) = ccn_nof_size(data_size);
    cczp_init(r);
    return 0;
}

static inline
CCCryptorStatus ccn_write_arg(size_t n, const cc_unit *source, uint8_t *dest, size_t *destLen)
{
    size_t len;
    if((len = ccn_write_uint_size(n, source)) > *destLen) {
        return kCCMemoryFailure;
    }
    *destLen = len;
    ccn_write_uint(n, source, *destLen, dest);
    return kCCSuccess;
}


CCCryptorStatus 
CCRSACryptorCreatePairFromData(uint32_t e, 
    uint8_t *xp1, size_t xp1Length,
    uint8_t *xp2, size_t xp2Length,
    uint8_t *xp, size_t xpLength,
    uint8_t *xq1, size_t xq1Length,
    uint8_t *xq2, size_t xq2Length,
    uint8_t *xq, size_t xqLength,
    CCRSACryptorRef *publicKey, CCRSACryptorRef *privateKey,
    uint8_t *retp, size_t *retpLength,
    uint8_t *retq, size_t *retqLength,
    uint8_t *retm, size_t *retmLength,
    uint8_t *retd, size_t *retdLength)
{
    CCCryptorStatus retval;
    CCRSACryptor *privateCryptor = NULL;
    CCRSACryptor *publicCryptor = NULL;
    cc_unit x_p1[ccn_nof_size(xp1Length)];
    cc_unit x_p2[ccn_nof_size(xp2Length)];
    cc_unit x_p[ccn_nof_size(xpLength)];
    cc_unit x_q1[ccn_nof_size(xq1Length)];
    cc_unit x_q2[ccn_nof_size(xq2Length)];
    cc_unit x_q[ccn_nof_size(xqLength)];
    cc_unit e_value[1];
    size_t nbits = xpLength * 8 + xqLength * 8; // or we'll add this as a parameter.  This appears to be correct for FIPS
    cc_size n = ccn_nof(nbits);
    cc_unit p[n], q[n], m[n], d[n];
    cc_size np, nq, nm, nd;
    
    np = nq = nm = nd = n;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    e_value[0] = (cc_unit) e;

    __Require_Action((privateCryptor = ccMallocRSACryptor(nbits, ccRSAKeyPrivate)) != NULL, errOut, retval = kCCMemoryFailure);

    __Require_Action(ccn_read_uint(ccn_nof_size(xp1Length), x_p1, xp1Length, xp1) == 0, errOut, retval = kCCParamError);
    __Require_Action(ccn_read_uint(ccn_nof_size(xp2Length), x_p2, xp2Length, xp2)== 0, errOut, retval = kCCParamError);
    __Require_Action(ccn_read_uint(ccn_nof_size(xpLength), x_p, xpLength, xp) == 0, errOut, retval = kCCParamError);
    __Require_Action(ccn_read_uint(ccn_nof_size(xq1Length), x_q1, xq1Length, xq1) == 0, errOut, retval = kCCParamError);
    __Require_Action(ccn_read_uint(ccn_nof_size(xq2Length), x_q2, xq2Length, xq2) == 0, errOut, retval = kCCParamError);
    __Require_Action(ccn_read_uint(ccn_nof_size(xqLength), x_q, xqLength, xq) == 0, errOut, retval = kCCParamError);
    
	__Require_Action(ccrsa_make_931_key(nbits, 1, e_value, 
                                        ccn_nof_size(xp1Length), x_p1, ccn_nof_size(xp2Length), x_p2, ccn_nof_size(xpLength), x_p,
                                        ccn_nof_size(xq1Length), x_q1, ccn_nof_size(xq2Length), x_q2, ccn_nof_size(xqLength), x_q,
                                        privateCryptor->fk,
                                        &np, p,
                                        &nq, q,
                                        &nm, m,
                                        &nd, d) == 0, errOut, retval = kCCDecodeError);
    
    privateCryptor->keyType = ccRSAKeyPrivate;
    
    __Require_Action((publicCryptor = CCRSACryptorGetPublicKeyFromPrivateKey(privateCryptor)) != NULL, errOut, retval = kCCMemoryFailure);

    *publicKey = publicCryptor;
    *privateKey = privateCryptor;
    ccn_write_arg(np, p, retp, retpLength);
    ccn_write_arg(nq, q, retq, retqLength);
    ccn_write_arg(nm, m, retm, retmLength);
    ccn_write_arg(nd, d, retd, retdLength);
    
    return kCCSuccess;
    
errOut:
    if(privateCryptor) ccRSACryptorClear(privateCryptor);
    if(publicCryptor) ccRSACryptorClear(publicCryptor);
    // CLEAR the bits
    *publicKey = *privateKey = NULL;
    return retval;

}



CCCryptorStatus
CCRSACryptorCreateFromData( CCRSAKeyType keyType, uint8_t *modulus, size_t modulusLength, 
                            uint8_t *exponent, size_t exponentLength,
                            uint8_t *p, size_t pLength, uint8_t *q, size_t qLength,
                            CCRSACryptorRef *ref)
{
    CCCryptorStatus retval = kCCSuccess;
	CCRSACryptor *rsaKey = NULL;
    size_t n = ccn_nof_size(modulusLength);
    cc_unit m[n];
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    __Require_Action(ccn_read_uint(n, m, modulusLength, modulus) == 0, errOut, retval = kCCParamError);
    size_t nbits = ccn_bitlen(n, m);

    __Require_Action((rsaKey = ccMallocRSACryptor(nbits, keyType)) != NULL, errOut, retval = kCCMemoryFailure);

    __Require_Action(ccn_read_uint(n, ccrsa_ctx_m(rsaKey->fk), modulusLength, modulus) == 0, errOut, retval = kCCParamError);
    __Require_Action(ccn_read_uint(n, ccrsa_ctx_e(rsaKey->fk), exponentLength, exponent) == 0, errOut, retval = kCCParamError);
    cczp_init(ccrsa_ctx_zm(rsaKey->fk));
    rsaKey->keySize = ccn_bitlen(n, ccrsa_ctx_m(rsaKey->fk));

	switch(keyType) {
		case ccRSAKeyPublic:
            rsaKey->keyType = ccRSAKeyPublic;
            break;
		
		case ccRSAKeyPrivate: {
            ccrsa_priv_ctx_t privk = ccrsa_ctx_private(rsaKey->fk);
            size_t psize = ccn_nof_size(pLength);
            size_t qsize = ccn_nof_size(qLength);

            
            CCZP_N(ccrsa_ctx_private_zp(privk)) = psize;
            __Require_Action(ccn_read_uint(psize, CCZP_PRIME(ccrsa_ctx_private_zp(privk)), pLength, p) == 0, errOut, retval = kCCParamError);
            CCZP_N(ccrsa_ctx_private_zq(privk)) = qsize;
            __Require_Action(ccn_read_uint(qsize, CCZP_PRIME(ccrsa_ctx_private_zq(privk)), qLength, q) == 0, errOut, retval = kCCParamError);

            ccrsa_crt_makekey(ccrsa_ctx_zm(rsaKey->fk), ccrsa_ctx_e(rsaKey->fk), ccrsa_ctx_d(rsaKey->fk),
                              ccrsa_ctx_private_zp(privk),
                              ccrsa_ctx_private_dp(privk), ccrsa_ctx_private_qinv(privk),
                              ccrsa_ctx_private_zq(privk), ccrsa_ctx_private_dq(privk));
            
            rsaKey->keyType = ccRSAKeyPrivate;

       		break;
        }
		
		default:
            retval = kCCParamError;
			goto errOut;
	}
	*ref = rsaKey;
	return kCCSuccess;
	
errOut:
	if(rsaKey) ccRSACryptorClear(rsaKey);
	return retval;
}




CCCryptorStatus
CCRSAGetKeyComponents(CCRSACryptorRef rsaKey, uint8_t *modulus, size_t *modulusLength, uint8_t *exponent, size_t *exponentLength,
                      uint8_t *p, size_t *pLength, uint8_t *q, size_t *qLength)
{
    CCRSACryptor *rsa = rsaKey;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	switch(rsa->keyType) {
		case ccRSAKeyPublic: {
            if(ccrsa_get_pubkey_components(ccrsa_ctx_public(rsaKey->fk), modulus, modulusLength, exponent, exponentLength)) return kCCParamError;
            break;
        }
            
		case ccRSAKeyPrivate: {
            if(ccrsa_get_fullkey_components(rsaKey->fk, modulus, modulusLength, exponent, exponentLength,
                                             p, pLength, q, qLength)) return kCCParamError;
            break;
        }
            
		default:
			return kCCParamError;
    }
            
    return kCCSuccess;
}


CCCryptorStatus 
CCRSACryptorSign(CCRSACryptorRef privateKey, CCAsymmetricPadding padding, 
                 const void *hashToSign, size_t hashSignLen,
                 CCDigestAlgorithm digestType, size_t saltLen,
                 void *signedData, size_t *signedDataLen)
{    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!privateKey || !hashToSign || !signedData) return kCCParamError;
    
    switch(padding) {
        case ccPKCS1Padding: 
            if(ccrsa_sign_pkcs1v15(privateKey->fk, CCDigestGetDigestInfo(digestType)->oid,
                                   hashSignLen, hashToSign, signedDataLen, signedData) != 0)
                return kCCDecodeError;
            break;
            
        case ccOAEPPadding:           
            if(ccrsa_sign_oaep(privateKey->fk, CCDigestGetDigestInfo(digestType),
                               ccDRBGGetRngState(), hashSignLen, hashToSign, 
                               signedDataLen, signedData) != 0)
                return kCCDecodeError;
            break;
        case ccX931Padding:
        case ccPKCS1PaddingRaw:
        case ccPaddingNone:
        default:
            return kCCParamError;
            break;
    }
    return kCCSuccess;
}



CCCryptorStatus 
CCRSACryptorVerify(CCRSACryptorRef publicKey, CCAsymmetricPadding padding,
                   const void *hash, size_t hashLen, 
                   CCDigestAlgorithm digestType, size_t saltLen,
                   const void *signedData, size_t signedDataLen)
{
    bool valid;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!publicKey || !hash || !signedData) return kCCParamError;
    
    switch(padding) {
        case ccPKCS1Padding: 
            if(ccrsa_verify_pkcs1v15(ccrsa_ctx_public(publicKey->fk), CCDigestGetDigestInfo(digestType)->oid,
                                     hashLen, hash, signedDataLen, signedData, &valid) != 0)
                return kCCDecodeError;
            if(!valid) return kCCDecodeError;
            break;
            
        case ccOAEPPadding:
            if(ccrsa_verify_oaep(ccrsa_ctx_public(publicKey->fk),  CCDigestGetDigestInfo(digestType),
                                 hashLen, hash, signedDataLen, signedData, &valid) != 0)
                return kCCDecodeError;
            if(!valid) return kCCDecodeError;
            break;
        case ccX931Padding:
        case ccPKCS1PaddingRaw:
        case ccPaddingNone:
        default:
            return kCCParamError;
            break;
    }
    return kCCSuccess;
}

#pragma mark APINotDone
#ifdef NEVER

// This was only here for FIPS.  If we move FIPS to the corecrypto layer it will need to be there.

CCCryptorStatus 
CCRSACryptorDecodePayloadPKCS1(
                               CCRSACryptorRef publicKey, 
                               const void *cipherText, 
                               size_t cipherTextLen,
                               void *plainText, 
                               size_t *plainTextLen)
{
    int tcReturn;
	int stat = 0;
    CCRSACryptor *publicCryptor = publicKey;
    uint8_t *message;
    unsigned long messageLen, modulusLen;
    CCCryptorStatus retval = kCCSuccess;
    
    modulusLen = CCRSAGetKeySize(publicKey);
    messageLen = modulusLen / 8;
    
    if((message = CC_XMALLOC(messageLen)) == NULL) return kCCMemoryFailure;
    
	tcReturn = rsa_exptmod(cipherText, cipherTextLen, message, messageLen, publicCryptor->keyType, &publicCryptor->key);
    if(tcReturn) {
        retval = kCCDecodeError;
        goto out;
    }
    tcReturn = pkcs_1_v1_5_decode(message, messageLen, LTC_PKCS_1_EME, modulusLen, plainText, plainTextLen, &stat);
    if(tcReturn) {
        retval = kCCDecodeError;
        goto out;        
    }
    if(!stat) {
        retval = kCCDecodeError;
        goto out;
    }
    
out:    
    CC_XZEROMEM(message, messageLen);
    CC_XFREE(message, messageLen);
    return retval;
}

#endif



