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

// #define COMMON_EC_FUNCTIONS
#include "CommonECCryptor.h"
#include "CommonDigest.h"
#include "CommonDigestPriv.h"
#include "CommonRandomSPI.h"
#include "ccMemory.h"
#import <corecrypto/ccec.h>
#include <AssertMacros.h>
#include "ccdebug.h"


#pragma mark Internal Structures and Functions

typedef struct _CCECCryptor {
    union {
        ccec_full_ctx *private;
        ccec_pub_ctx *public;
        uint8_t *bytes;
    } ecKey;
    size_t keySize;
    CCECKeyType keyType;
} CCECCryptor;


static CCECCryptor *
ccMallocECCryptor(size_t nbits, CCECKeyType keyType)
{
    CCECCryptor *retval;
    size_t ctxSize = 0;
    
    
    if(!ccec_keysize_is_supported(nbits)) return NULL;
    ccec_const_cp_t cp = ccec_get_cp(nbits);    
    size_t len = ccec_cp_prime_size(cp);

    if((retval = CC_XMALLOC(sizeof(CCECCryptor))) == NULL) return NULL;
    
    retval->keySize = nbits;
    retval->ecKey.bytes = NULL;
    
    switch(keyType) {
        case ccECKeyPublic:
            retval->keyType = ccECBlankPublicKey;
            ctxSize = ccec_pub_ctx_size(len);
            break;
        case ccECKeyPrivate:
            retval->keyType = ccECBlankPrivateKey;
            ctxSize = ccec_full_ctx_size(len);
            break;
        default:
            goto errOut;
    }
    
    if((retval->ecKey.bytes = CC_XMALLOC(ctxSize)) == NULL) goto errOut;
    ccec_ctx_init(cp, retval->ecKey.public);

    return retval;
errOut:
    if(retval) {
        CC_XFREE(retval, sizeof(CCECCryptor));
    }
    return NULL;
}

static void
ccECCryptorFree(CCECCryptor *theKey)
{
    size_t nbits = theKey->keySize;
    size_t ctxSize = 0;
    
    ccec_const_cp_t cp = ccec_get_cp(nbits);    
    size_t len = ccec_cp_prime_size(cp);
    
    CCECCryptor *key = (CCECCryptor *) theKey;
    if(!key) return;
    
    switch(key->keyType) {
        case ccECKeyPublic:
        case ccECBlankPublicKey:
            ctxSize = ccec_pub_ctx_size(len);
            break;
        case ccECKeyPrivate:
        case ccECBlankPrivateKey:
            ctxSize = ccec_full_ctx_size(len);
            break;
        default:
            break;
    }
    
    if(ctxSize && key->ecKey.bytes) {
        CC_XZEROMEM(key->ecKey.bytes, ctxSize);
        CC_XFREE(key->ecKey.bytes, ctxSize);
    }

    CC_XZEROMEM(key, sizeof(CCECCryptor));
    CC_XFREE(theKey, sizeof(CCECCryptor));
}

static bool
ccECpairwiseConsistencyCheck(CCECCryptorRef privateKey, CCECCryptorRef publicKey)
{
	CCCryptorStatus status = kCCSuccess;
    uint8_t digestBuffer[CC_SHA1_DIGEST_LENGTH];
	size_t signedDataLen = 4096;
	uint8_t signedData[4096];
	uint32_t isValid = 0;
    
    CC_XMEMSET(digestBuffer, 0x0a, CC_SHA1_DIGEST_LENGTH);
    
	status = CCECCryptorSignHash(privateKey, 
                                 digestBuffer, CC_SHA1_DIGEST_LENGTH,
                                 signedData, &signedDataLen);
    
	if (kCCSuccess != status) return false;
	
	status = CCECCryptorVerifyHash(publicKey, 
                                   digestBuffer, CC_SHA1_DIGEST_LENGTH, 
                                   signedData, signedDataLen, &isValid);
    
	if (kCCSuccess != status || isValid != 1) return false;
	return true;	
}


#pragma mark API (SPI for now)


CCCryptorStatus 
CCECCryptorGeneratePair(size_t nbits, CCECCryptorRef *publicKey, CCECCryptorRef *privateKey)
{
    CCCryptorStatus retval;
    CCECCryptor *privateCryptor = NULL;
    CCECCryptor *publicCryptor = NULL;
    struct ccrng_state *theRng = ccDRBGGetRngState();
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!ccec_keysize_is_supported(nbits)) return kCCParamError;
    ccec_const_cp_t cp = ccec_get_cp(nbits);    

    __Require_Action((privateCryptor = ccMallocECCryptor(nbits, ccECKeyPrivate)) != NULL, errOut, retval = kCCMemoryFailure);
    privateCryptor->keySize = nbits;

    __Require_Action((ccec_generate_key(cp, theRng, privateCryptor->ecKey.private) == 0), errOut, retval = kCCDecodeError);
        
    privateCryptor->keyType = ccECKeyPrivate;
    
    __Require_Action((publicCryptor = CCECCryptorGetPublicKeyFromPrivateKey(privateCryptor)) != NULL, errOut, retval = kCCMemoryFailure);
    
    __Require_Action(ccECpairwiseConsistencyCheck(privateCryptor, publicCryptor) == true, errOut, retval = kCCDecodeError);
    
    *publicKey = publicCryptor;
    *privateKey = privateCryptor;

    return kCCSuccess;
    
errOut:
    if(privateCryptor) ccECCryptorFree(privateCryptor);
    if(publicCryptor) ccECCryptorFree(publicCryptor);
    *publicKey = *privateKey = NULL;
    return kCCDecodeError;
    
}

CCECCryptorRef 
CCECCryptorGetPublicKeyFromPrivateKey(CCECCryptorRef privateKey)
{
    CCECCryptor *publicCryptor = NULL;

    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    __Require((publicCryptor = ccMallocECCryptor(privateKey->keySize, ccECKeyPublic)) != NULL, errOut);
    ccec_const_cp_t cp = ccec_get_cp(privateKey->keySize);    
    size_t ctx_size = ccec_pub_ctx_size(ccec_cp_prime_size(cp));
    CC_XMEMCPY(publicCryptor->ecKey.public, privateKey->ecKey.public, ctx_size);
    publicCryptor->keySize = privateKey->keySize;
    publicCryptor->keyType = ccECKeyPublic;
    
    if(ccECpairwiseConsistencyCheck(privateKey, publicCryptor) == false) goto errOut;
    return publicCryptor;
    
errOut:
    if(publicCryptor) ccECCryptorFree(publicCryptor);
    return NULL;
    
}


CCCryptorStatus 
CCECCryptorGetKeyComponents(CCECCryptorRef ecKey, size_t *keySize,
                            uint8_t *qX, size_t *qXLength, 
                            uint8_t *qY, size_t *qYLength,
                            uint8_t *d, size_t *dLength)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    switch(ecKey->keyType) {
        case ccECKeyPublic:
            if(ccec_get_pubkey_components(ecKey->ecKey.public, keySize, 
                                          qX, qXLength, 
                                          qY, qYLength)) return kCCMemoryFailure;
            break;
        case ccECKeyPrivate:
            if(ccec_get_fullkey_components(ecKey->ecKey.private, keySize,
                                           qX, qXLength, 
                                           qY, qYLength, 
                                           d, dLength)) return kCCMemoryFailure;
            break;
        default: return kCCParamError;
    }
    return kCCSuccess;
}

CCCryptorStatus
CCECCryptorCreateFromData(size_t nbits,
                          uint8_t *qX, size_t qXLength, 
                          uint8_t *qY, size_t qYLength,
                          CCECCryptorRef *ref)
{
    CCECCryptor *publicCryptor;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    *ref = NULL;
    if((publicCryptor = ccMallocECCryptor(nbits, ccECKeyPublic)) == NULL) return kCCMemoryFailure;
    if(ccec_make_pub(nbits, qXLength, qX, qYLength, qY, publicCryptor->ecKey.public)) {
        ccECCryptorFree(publicCryptor);
        return kCCDecodeError;
    }
    publicCryptor->keyType = ccECKeyPublic;

    *ref = publicCryptor;
    return kCCSuccess;
}

CCECKeyType CCECGetKeyType(CCECCryptorRef key)
{
    CCECCryptor *cryptor = key;
    CCECKeyType retval;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(key == NULL) return ccECBlankPublicKey;
    retval = cryptor->keyType;
    if(retval != ccECKeyPublic && retval != ccECKeyPrivate) return ccECBadKey;
    return retval;
}

int CCECGetKeySize(CCECCryptorRef key)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(key == NULL) return kCCParamError;
    return (int) key->keySize;
}

void 
CCECCryptorRelease(CCECCryptorRef key)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    ccECCryptorFree(key);
}

CCCryptorStatus CCECCryptorImportPublicKey(void *keyPackage, size_t keyPackageLen, CCECCryptorRef *key)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return CCECCryptorImportKey(kCCImportKeyBinary, keyPackage, keyPackageLen, ccECKeyPublic, key);
}


CCCryptorStatus CCECCryptorImportKey(CCECKeyExternalFormat format, void *keyPackage, size_t keyPackageLen, CCECKeyType keyType, CCECCryptorRef *key)
{
    CCECCryptor *cryptor = NULL;
    CCCryptorStatus retval = kCCSuccess;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(keyPackage == NULL) return kCCParamError;
        
    switch(format) {
        case kCCImportKeyBinary:
            if(keyType == ccECKeyPrivate) {
                size_t nbits = ccec_x963_import_priv_size(keyPackageLen);
                if((cryptor = ccMallocECCryptor(nbits, ccECKeyPrivate)) == NULL) return kCCMemoryFailure;
                ccec_const_cp_t cp = ccec_get_cp(nbits);
                __Require_Action(ccec_x963_import_priv(cp, keyPackageLen, keyPackage, cryptor->ecKey.private) == 0, errOut, retval = kCCDecodeError);
                cryptor->keySize = nbits;
            } else if(keyType == ccECKeyPublic) {
                size_t nbits = ccec_x963_import_pub_size(keyPackageLen);
                if((cryptor = ccMallocECCryptor(nbits, ccECKeyPublic)) == NULL) return kCCMemoryFailure;
                ccec_const_cp_t cp = ccec_get_cp(nbits);
                __Require_Action(ccec_x963_import_pub(cp, keyPackageLen, keyPackage, cryptor->ecKey.public) == 0, errOut, retval = kCCDecodeError);
                cryptor->keySize = nbits;
            } else return kCCParamError;

            cryptor->keyType = keyType;
            *key = cryptor;
            break;
        case kCCImportKeyDER:
            retval = kCCUnimplemented;
            break;
        default:
            retval = kCCParamError;
            break;
    }
    
    
errOut:
    if(retval) {
        *key = NULL;
        if(cryptor) ccECCryptorFree(cryptor);
    }
    
    return retval;
}


CCCryptorStatus CCECCryptorExportPublicKey(CCECCryptorRef key, void *out, size_t *outLen)
{    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(key == NULL) return kCCParamError;
    if(out == NULL) return kCCParamError;
    
    return CCECCryptorExportKey(kCCImportKeyBinary, out, outLen, ccECKeyPublic, key);
}

CCCryptorStatus CCECCryptorExportKey(CCECKeyExternalFormat format, void *keyPackage, size_t *keyPackageLen, CCECKeyType keyType, CCECCryptorRef key)
{
    CCCryptorStatus retval = kCCSuccess;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(key == NULL) return kCCParamError;
    if(keyPackage == NULL) return kCCParamError;
    
    switch(format) {
        case kCCImportKeyBinary: {
            size_t len = ccec_x963_export_size(keyType == ccECKeyPrivate, key->ecKey.private);
            
            if(len > *keyPackageLen) {
                *keyPackageLen = len;
                return kCCMemoryFailure;
            }
            *keyPackageLen = len;
            
            ccec_x963_export(keyType == ccECKeyPrivate, keyPackage, key->ecKey.private);
            break;
        }
        case kCCImportKeyDER:
            retval = kCCUnimplemented;
            break;
        default:
            retval = kCCParamError;
            break;
    }

    return retval;    
}



CCCryptorStatus 
CCECCryptorSignHash(CCECCryptorRef privateKey, 
                    const void *hashToSign, size_t hashSignLen,
                    void *signedData, size_t *signedDataLen)
{
    CCCryptorStatus retval = kCCSuccess;
    
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(privateKey == NULL || hashToSign == NULL || signedData == NULL || signedDataLen == NULL) return kCCParamError;
    
    struct ccrng_state *therng = ccDRBGGetRngState();
    
    if(ccec_sign(privateKey->ecKey.private, hashSignLen, hashToSign, signedDataLen, signedData, therng) != 0)
        retval = kCCDecodeError;
    return retval;
}



CCCryptorStatus 
CCECCryptorVerifyHash(CCECCryptorRef publicKey,
                      const void *hash, size_t hashLen, 
                      const void *signedData, size_t signedDataLen, uint32_t *valid)
{
    CCCryptorStatus retval = kCCSuccess;
    bool           stat = 0;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(publicKey == NULL || hash == NULL || signedData == NULL) return kCCParamError;
    
    if(ccec_verify(publicKey->ecKey.public, hashLen, hash,
                   signedDataLen, signedData, &stat)) retval = kCCDecodeError;
	*valid = stat;
    return retval;
}


#pragma mark API for ECDH - needs corecrypto key import / export capability (SPI for now)


CCCryptorStatus 
CCECCryptorWrapKey(CCECCryptorRef publicKey, 
                   const void *plainText, size_t plainTextLen, 
                   void *cipherText, size_t *cipherTextLen,
                   CCDigestAlg digestType)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return kCCUnimplemented;
}


CCCryptorStatus 
CCECCryptorUnwrapKey(CCECCryptorRef privateKey, 
                     const void *cipherText, size_t cipherTextLen,
                     void *plainText, size_t *plainTextLen)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return kCCUnimplemented;
}


CCCryptorStatus 
CCECCryptorComputeSharedSecret(CCECCryptorRef privateKey, CCECCryptorRef publicKey, 
                               void *out, size_t *outLen)
{
    CCCryptorStatus retval = kCCSuccess;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(privateKey == NULL || publicKey == NULL) return kCCParamError;
    if(out == NULL) return kCCParamError;
    
    if(ccec_compute_key(privateKey->ecKey.private, publicKey->ecKey.public,
                        outLen, out)) return kCCDecodeError;
    
    return retval;
}


