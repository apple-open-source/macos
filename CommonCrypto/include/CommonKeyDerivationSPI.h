//
//  CommonNistKeyDerivation.h
//  CommonCrypto
//
//  Created by Richard Murphy on 12/15/13.
//  Copyright (c) 2013 Platform Security. All rights reserved.
//

#ifndef CommonCrypto_CommonNistKeyDerivation_h
#define CommonCrypto_CommonNistKeyDerivation_h

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigestSPI.h>

/*!
    @enum       CCKDFAlgorithm
    @abstract   Key Derivation algorithms implemented by this module.

    @constant   kCCKDFAlgorithmPBKDF2_HMAC
 
    @constant   kCCKDFAlgorithmCTR_HMAC
    @constant   kCCKDFAlgorithmCTR_HMAC_FIXED
    @constant   kCCKDFAlgorithmFB_HMAC
    @constant   kCCKDFAlgorithmFB_HMAC_FIXED
 	@constant   kCCKDFAlgorithmDPIPE_HMAC
*/

enum {
    kCCKDFAlgorithmPBKDF2_HMAC = 0,
    kCCKDFAlgorithmCTR_HMAC,
    kCCKDFAlgorithmCTR_HMAC_FIXED,
    kCCKDFAlgorithmFB_HMAC,         // UNIMP
    kCCKDFAlgorithmFB_HMAC_FIXED,   // UNIMP
    kCCKDFAlgorithmDPIPE_HMAC,      // UNIMP
    kCCKDFAlgorithmHKDF
};
typedef uint32_t CCKDFAlgorithm;

CCStatus
CCKeyDerivationHMac(CCKDFAlgorithm algorithm, CCDigestAlgorithm digest,
                    uint rounds, // ignored except for PBKDF
                    const void *keyDerivationKey, size_t keyDerivationKeyLen,
                    const void *label, size_t labelLen,
                    const void *context, size_t contextLen, // or FIXED buffer (label | context)
                    const void *iv, size_t ivLen,           // for FB
                    const void *salt, size_t saltLen,       // for PBKDF
                    void *derivedKey, size_t derivedKeyLen)
__OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);

#endif
