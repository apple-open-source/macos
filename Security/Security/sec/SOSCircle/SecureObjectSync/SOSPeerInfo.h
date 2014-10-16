/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#ifndef _SOSPEERINFO_H_
#define _SOSPEERINFO_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKey.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <corecrypto/ccdigest.h>

__BEGIN_DECLS

typedef struct __OpaqueSOSPeerInfo   *SOSPeerInfoRef;

enum {
    kSOSPeerVersion = 2,
};


enum {
    SOSPeerCmpPubKeyHash = 0,
    SOSPeerCmpName = 1,
};
typedef uint32_t SOSPeerInfoCmpSelect;

CFTypeID SOSPeerInfoGetTypeID(void);

static inline bool isSOSPeerInfo(CFTypeRef obj) {
    return obj && (CFGetTypeID(obj) == SOSPeerInfoGetTypeID());
}

SOSPeerInfoRef SOSPeerInfoCreate(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error);

SOSPeerInfoRef SOSPeerInfoCreateCloudIdentity(CFAllocatorRef allocator, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error);

SOSPeerInfoRef SOSPeerInfoCreateCopy(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFErrorRef* error);
SOSPeerInfoRef SOSPeerInfoCopyWithGestaltUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef gestalt, SecKeyRef signingKey, CFErrorRef* error);
SOSPeerInfoRef SOSPeerInfoCopyAsApplication(SOSPeerInfoRef pi, SecKeyRef userkey, SecKeyRef peerkey, CFErrorRef *error);

bool SOSPeerInfoUpdateDigestWithPublicKeyBytes(SOSPeerInfoRef peer, const struct ccdigest_info *di,
                                               ccdigest_ctx_t ctx, CFErrorRef *error);
bool SOSPeerInfoUpdateDigestWithDescription(SOSPeerInfoRef peer, const struct ccdigest_info *di,
                                            ccdigest_ctx_t ctx, CFErrorRef *error);


bool SOSPeerInfoApplicationVerify(SOSPeerInfoRef pi, SecKeyRef userkey, CFErrorRef *error);

CF_RETURNS_RETAINED CFDateRef SOSPeerInfoGetApplicationDate(SOSPeerInfoRef pi);

//
// DER Import Export
//
SOSPeerInfoRef SOSPeerInfoCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end);

SOSPeerInfoRef SOSPeerInfoCreateFromData(CFAllocatorRef allocator, CFErrorRef* error,
                                         CFDataRef peerinfo_data);

size_t      SOSPeerInfoGetDEREncodedSize(SOSPeerInfoRef peer, CFErrorRef *error);
uint8_t*    SOSPeerInfoEncodeToDER(SOSPeerInfoRef peer, CFErrorRef* error,
                                   const uint8_t* der, uint8_t* der_end);

CFDataRef SOSPeerInfoCopyEncodedData(SOSPeerInfoRef peer, CFAllocatorRef allocator, CFErrorRef *error);

//
// Gestalt info about the peer. It was fetched by the implementation on the other side.
// probably has what you're looking for..
//
CFTypeRef SOSPeerInfoLookupGestaltValue(SOSPeerInfoRef pi, CFStringRef key);
CFDictionaryRef SOSPeerInfoCopyPeerGestalt(SOSPeerInfoRef pi);

//
// Syntactic Sugar for some commone ones, might get deprectated at this level.
//
CFStringRef SOSPeerInfoGetTransportType(SOSPeerInfoRef peer);
CFStringRef SOSPeerInfoGetPeerName(SOSPeerInfoRef peer);
CFStringRef SOSPeerInfoGetPeerDeviceType(SOSPeerInfoRef peer);
CFIndex SOSPeerInfoGetPeerProtocolVersion(SOSPeerInfoRef peer);

// IDSs device ID
CFStringRef SOSPeerInfoGetDeviceID(SOSPeerInfoRef peer);
void SOSPeerInfoSetDeviceID(SOSPeerInfoRef peer, CFStringRef IDS);

// Stringified ID for this peer, not human readable.
CFStringRef SOSPeerInfoGetPeerID(SOSPeerInfoRef peer);

CFIndex SOSPeerInfoGetVersion(SOSPeerInfoRef peer);

//
// Peer Info Gestalt Helpers
//
CFStringRef SOSPeerGestaltGetName(CFDictionaryRef gestalt);

// These are Mobile Gestalt questions. Not all Gestalt questions are carried.
CFTypeRef SOSPeerGestaltGetAnswer(CFDictionaryRef gestalt, CFStringRef question);

SecKeyRef SOSPeerInfoCopyPubKey(SOSPeerInfoRef peer);

CFComparisonResult SOSPeerInfoCompareByID(const void *val1, const void *val2, void *context);

SOSPeerInfoRef SOSPeerInfoCreateRetirementTicket(CFAllocatorRef allocator, SecKeyRef privKey, SOSPeerInfoRef peer, CFErrorRef *error);

CFStringRef SOSPeerInfoInspectRetirementTicket(SOSPeerInfoRef pi, CFErrorRef *error);

bool SOSPeerInfoRetireRetirementTicket(size_t max_days, SOSPeerInfoRef pi);

CF_RETURNS_RETAINED CFDateRef SOSPeerInfoGetRetirementDate(SOSPeerInfoRef pi);

bool SOSPeerInfoIsRetirementTicket(SOSPeerInfoRef pi);

bool SOSPeerInfoIsCloudIdentity(SOSPeerInfoRef pi);

SOSPeerInfoRef SOSPeerInfoUpgradeSignatures(CFAllocatorRef allocator, SecKeyRef privKey, SecKeyRef perKey, SOSPeerInfoRef peer, CFErrorRef *error);

__END_DECLS

#endif
