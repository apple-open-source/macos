/*
 *  SecOTRIdentityPriv.h
 *  libsecurity_libSecOTR
 *
 *  Created by Mitch Adler on 2/9/11.
 *  Copyright 2011 Apple Inc. All rights reserved.
 *
 */

#ifndef _SECOTRIDENTITYPRIV_H_

#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFData.h>

#include <Security/SecKey.h>

#include <Security/oidsalg.h>

#include <CommonCrypto/CommonDigest.h> // DIGEST_LENGTH
#include <Security/SecOTR.h>

__BEGIN_DECLS

extern CFStringRef sErrorDomain;
    
// OAEP Padding, uses lots of space. Might need this to be data
// Driven when we support more key types.
#define kPaddingOverhead (2 + 2 * CC_SHA1_DIGEST_LENGTH + 1)
    
//
// Identity opaque structs
//

#define kMPIDHashSize   CC_SHA1_DIGEST_LENGTH

struct _SecOTRFullIdentity {
    CFRuntimeBase _base;
    
    SecKeyRef   publicSigningKey;
    SecKeyRef   privateSigningKey;
    
    uint8_t     publicIDHash[kMPIDHashSize];
};


struct _SecOTRPublicIdentity {
    CFRuntimeBase _base;
    
    SecKeyRef   publicSigningKey;

    bool        wantsHashes;

    uint8_t     hash[kMPIDHashSize];
};

enum SecOTRError {
    secOTRErrorLocal,
    secOTRErrorOSError,
};

const SecAsn1AlgId *kOTRSignatureAlgIDPtr;
void EnsureOTRAlgIDInited(void);
    
// Private functions for Public and Full IDs
SecOTRFullIdentityRef SecOTRFullIdentityCreateWithSize(CFAllocatorRef allocator, int bits);

bool SecOTRFIAppendSignature(SecOTRFullIdentityRef fullID,
                                CFDataRef dataToHash,
                                CFMutableDataRef appendTo,
                                CFErrorRef *error);

void SecOTRFIAppendPublicHash(SecOTRFullIdentityRef fullID, CFMutableDataRef appendTo);
bool SecOTRFIComparePublicHash(SecOTRFullIdentityRef fullID, const uint8_t hash[kMPIDHashSize]);

size_t SecOTRFISignatureSize(SecOTRFullIdentityRef privateID);

bool SecOTRPIVerifySignature(SecOTRPublicIdentityRef publicID,
                                const uint8_t *dataToHash, size_t amountToHash,
                                const uint8_t *signatureStart, size_t signatureSize, CFErrorRef *error);

bool SecOTRPIEqualToBytes(SecOTRPublicIdentityRef id, const uint8_t*bytes, CFIndex size);
bool SecOTRPIEqual(SecOTRPublicIdentityRef left, SecOTRPublicIdentityRef right);

size_t SecOTRPISignatureSize(SecOTRPublicIdentityRef publicID);
    
void SecOTRPICopyHash(SecOTRPublicIdentityRef publicID, uint8_t hash[kMPIDHashSize]);
void SecOTRPIAppendHash(SecOTRPublicIdentityRef publicID, CFMutableDataRef appendTo);

bool SecOTRPICompareHash(SecOTRPublicIdentityRef publicID, const uint8_t hash[kMPIDHashSize]);

// Utility streaming functions
OSStatus insertSize(CFIndex size, uint8_t* here);
OSStatus appendSize(CFIndex size, CFMutableDataRef into);
OSStatus readSize(const uint8_t** data, size_t* limit, uint16_t* size);

OSStatus appendPublicOctets(SecKeyRef fromKey, CFMutableDataRef appendTo);
OSStatus appendPublicOctetsAndSize(SecKeyRef fromKey, CFMutableDataRef appendTo);
OSStatus appendSizeAndData(CFDataRef data, CFMutableDataRef appendTo);

SecKeyRef CreateECPrivateKeyFrom(CFAllocatorRef allocator, const uint8_t** data, size_t* limit);
SecKeyRef CreateECPublicKeyFrom(CFAllocatorRef allocator, const uint8_t** data, size_t* limit);
    
void SecOTRCreateError(enum SecOTRError family, CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError);

__END_DECLS

#endif
