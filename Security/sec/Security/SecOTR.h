/*
 *  SecOTR.h
 *  libsecurity_libSecOTR
 *
 *  Created by Mitch Adler on 2/2/11.
 *  Copyright 2011 Apple Inc. All rights reserved.
 *
 */

#ifndef _SECOTR_H_
#define _SECOTR_H_

/*
 * Message Protection interfaces
*/

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>
#include <Security/SecKey.h>

#include <stdint.h>

__BEGIN_DECLS

/*!
    @typedef 
    @abstract   Full identity (public and private) for Message Protection
    @discussion Abstracts what kind of crypto is going on beyond it being public/priate
*/
typedef struct _SecOTRFullIdentity* SecOTRFullIdentityRef;

/*!
     @typedef 
     @abstract   Public identity for Message Protection message validation and encryption to send
     @discussion Abstracts what kind of crypto is going on beyond it being public/priate
 */
typedef struct _SecOTRPublicIdentity* SecOTRPublicIdentityRef;

/*
 * Full identity functions
 */
SecOTRFullIdentityRef SecOTRFullIdentityCreate(CFAllocatorRef allocator, CFErrorRef *error);
    
SecOTRFullIdentityRef SecOTRFullIdentityCreateFromSecKeyRef(CFAllocatorRef allocator, SecKeyRef privateKey,
                                                                CFErrorRef *error);
SecOTRFullIdentityRef SecOTRFullIdentityCreateFromData(CFAllocatorRef allocator, CFDataRef serializedData, CFErrorRef *error);
    
SecOTRFullIdentityRef SecOTRFullIdentityCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t *size, CFErrorRef *error);

bool SecOTRFIPurgeFromKeychain(SecOTRFullIdentityRef thisID, CFErrorRef *error);

bool SecOTRFIAppendSerialization(SecOTRFullIdentityRef fullID, CFMutableDataRef serializeInto, CFErrorRef *error);


bool SecOTRFIPurgeAllFromKeychain(CFErrorRef *error);


/*
 * Public identity functions
 */
SecOTRPublicIdentityRef SecOTRPublicIdentityCopyFromPrivate(CFAllocatorRef allocator, SecOTRFullIdentityRef fullID, CFErrorRef *error);
    
SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromSecKeyRef(CFAllocatorRef allocator, SecKeyRef publicKey,
                                                                    CFErrorRef *error);
    
SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromData(CFAllocatorRef allocator, CFDataRef serializedData, CFErrorRef *error);
SecOTRPublicIdentityRef SecOTRPublicIdentityCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t * size, CFErrorRef *error);

bool SecOTRPIAppendSerialization(SecOTRPublicIdentityRef publicID, CFMutableDataRef serializeInto, CFErrorRef *error);

void SecOTRAdvertiseHashes(bool advertise);
    
__END_DECLS
        
#endif
