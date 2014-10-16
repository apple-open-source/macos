//
//  CommonNistKeyDerivation.c
//  CommonCrypto
//
//  Created by Richard Murphy on 12/15/13.
//  Copyright (c) 2013 Platform Security. All rights reserved.
//

#include <CommonCrypto/CommonKeyDerivationSPI.h>
#include <corecrypto/cchkdf.h>
#include <corecrypto/ccnistkdf.h>
#include <corecrypto/ccpbkdf2.h>
#include "CommonDigestPriv.h"
#include "ccMemory.h"
#include "ccdebug.h"
/*
enum {
    kCCKDFAlgorithmPBKDF2_HMAC = 0,
    kCCKDFAlgorithmCTR_HMAC,
    kCCKDFAlgorithmCTR_HMAC_FIXED,
    kCCKDFAlgorithmFB_HMAC,         // UNIMP
    kCCKDFAlgorithmFB_HMAC_FIXED,   // UNIMP
    kCCKDFAlgorithmDPIPE_HMAC,      // UNIMP
};
typedef uint32_t CCKDFAlgorithm;
*/

CCStatus
CCKeyDerivationHMac(CCKDFAlgorithm algorithm, CCDigestAlgorithm digest,
                    uint rounds, // ignored except for PBKDF
                    const void *keyDerivationKey, size_t keyDerivationKeyLen,
                    const void *label, size_t labelLen,
                    const void *context, size_t contextLen, // or FIXED buffer (label | context)
                    const void *iv, size_t ivLen,           // for FB
                    const void *salt, size_t saltLen,       // for PBKDF or HKDF
                    void *derivedKey, size_t derivedKeyLen) {
    int cc_retval = 0;
    const struct ccdigest_info *di = CCDigestGetDigestInfo(digest);
	
    if(di == NULL) {
        CC_DEBUG_LOG(CC_DEBUG, "CCKeyDerivationHMac Unknown Digest %d\n");
        return kCCParamError;
	}
    
    if(!keyDerivationKeyLen || !keyDerivationKey) {
        CC_DEBUG_LOG(CC_DEBUG, "CCKeyDerivationHMac bad KDK parameters %d\n");
        return kCCParamError;
	}
    
    if(!derivedKeyLen || !derivedKey) {
        CC_DEBUG_LOG(CC_DEBUG, "CCKeyDerivationHMac bad derived key parameters %d\n");
        return kCCParamError;
	}

    switch(algorithm) {
    case kCCKDFAlgorithmPBKDF2_HMAC:
        if(!salt) {
            if(saltLen) return kCCParamError;
            salt = "";
        }
        cc_retval = ccpbkdf2_hmac(di, keyDerivationKeyLen, keyDerivationKey,
                                    saltLen, salt, rounds,
                                    derivedKeyLen, derivedKey);
        if(cc_retval) return kCCParamError; // Probably a bad derivedKeyLen
        break;
    case kCCKDFAlgorithmCTR_HMAC:
        cc_retval = ccnistkdf_ctr_hmac(di, keyDerivationKeyLen, keyDerivationKey,
                        labelLen, label, contextLen, context,
                        derivedKeyLen, derivedKey);
        if(cc_retval) return kCCParamError; // Probably a bad derivedKeyLen
        break;
    case kCCKDFAlgorithmCTR_HMAC_FIXED:
        cc_retval = ccnistkdf_ctr_hmac_fixed(di, keyDerivationKeyLen, keyDerivationKey,
                        contextLen, context,
                        derivedKeyLen, derivedKey);
        if(cc_retval) return kCCParamError; // Probably a bad derivedKeyLen
        break;
    case kCCKDFAlgorithmHKDF:
    	cc_retval = cchkdf(di, keyDerivationKeyLen, keyDerivationKey, saltLen, salt, 
                           contextLen, context, derivedKeyLen, derivedKey);
    	if(cc_retval) return kCCParamError; // Probably a bad derivedKeyLen
    	break;
    case kCCKDFAlgorithmFB_HMAC:
    case kCCKDFAlgorithmFB_HMAC_FIXED:
    case kCCKDFAlgorithmDPIPE_HMAC:
    default:
        return kCCUnimplemented;
    }
    return kCCSuccess;
}
