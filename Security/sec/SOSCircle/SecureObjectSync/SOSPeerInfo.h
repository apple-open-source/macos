//
//  SOSPeerInfo.h
//  sec
//
//  Created by Mitch Adler on 7/19/12.
//
//

#ifndef _SOSPEERINFO_H_
#define _SOSPEERINFO_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKey.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <corecrypto/ccdigest.h>
#include <xpc/xpc.h>

__BEGIN_DECLS

typedef struct __OpaqueSOSPeerInfo   *SOSPeerInfoRef;

enum {
    kSOSPeerVersion = 1,
};


enum {
    SOSPeerCmpPubKeyHash = 0,
    SOSPeerCmpName = 1,
};
typedef uint32_t SOSPeerInfoCmpSelect;

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
CFStringRef SOSPeerInfoGetPeerName(SOSPeerInfoRef peer);
CFStringRef SOSPeerInfoGetPeerDeviceType(SOSPeerInfoRef peer);


// Stringified ID for this peer, not human readable.
CFStringRef SOSPeerInfoGetPeerID(SOSPeerInfoRef peer);

CFIndex SOSPeerInfoGetVersion(SOSPeerInfoRef peer);



//
// Peer Info Arrays
//

CFMutableArrayRef SOSPeerInfoArrayCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                                const uint8_t** der_p, const uint8_t *der_end);
size_t SOSPeerInfoArrayGetDEREncodedSize(CFArrayRef pia, CFErrorRef *error);
uint8_t* SOSPeerInfoArrayEncodeToDER(CFArrayRef pia, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

CFArrayRef CreateArrayOfPeerInfoWithXPCObject(xpc_object_t peerArray, CFErrorRef* error);
xpc_object_t CreateXPCObjectWithArrayOfPeerInfo(CFArrayRef array, CFErrorRef *error);

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
