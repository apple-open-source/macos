//
//  SOSCircleDer.c
//  sec
//
//  Created by Richard Murphy on 1/22/15.
//
//

#include <stdio.h>
#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFArray.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSCirclePriv.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecFramework.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <stdlib.h>
#include <assert.h>

#include "SOSCircleDer.h"

static const uint8_t* der_decode_mutable_dictionary(CFAllocatorRef allocator, CFOptionFlags mutability,
                                                    CFMutableDictionaryRef* dictionary, CFErrorRef *error,
                                                    const uint8_t* der, const uint8_t *der_end)
{
    CFDictionaryRef theDict;
    const uint8_t* result = der_decode_dictionary(allocator, mutability, &theDict, error, der, der_end);
    
    if (result != NULL)
        *dictionary = (CFMutableDictionaryRef)theDict;
    
    return result;
}


SOSCircleRef SOSCircleCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                    const uint8_t** der_p, const uint8_t *der_end) {
    SOSCircleRef cir = CFTypeAllocate(SOSCircle, struct __OpaqueSOSCircle, allocator);
    
    const uint8_t *sequence_end;
    
    cir->name = NULL;
    cir->generation = NULL;
    cir->peers = NULL;
    cir->applicants = NULL;
    cir->rejected_applicants = NULL;
    cir->signatures = NULL;
    
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    require_action_quiet(sequence_end != NULL, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Circle DER"), (error != NULL) ? *error : NULL, error));
    
    // Version first.
    uint64_t version = 0;
    *der_p = ccder_decode_uint64(&version, *der_p, der_end);
    
    require_action_quiet(version == kOnlyCompatibleVersion, fail,
                         SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("Bad Circle Version"), NULL, error));
    
    *der_p = der_decode_string(allocator, 0, &cir->name, error, *der_p, sequence_end);
    cir->generation = SOSGenCountCreateFromDER(kCFAllocatorDefault, error, der_p, sequence_end);
    
    cir->peers = SOSPeerInfoSetCreateFromArrayDER(allocator, &kSOSPeerSetCallbacks, error, der_p, sequence_end);
    cir->applicants = SOSPeerInfoSetCreateFromArrayDER(allocator, &kSOSPeerSetCallbacks, error, der_p, sequence_end);
    cir->rejected_applicants = SOSPeerInfoSetCreateFromArrayDER(allocator, &kSOSPeerSetCallbacks, error, der_p, sequence_end);
    
    *der_p = der_decode_mutable_dictionary(allocator, kCFPropertyListMutableContainersAndLeaves,
                                           &cir->signatures, error, *der_p, sequence_end);
    
    require_action_quiet(*der_p == sequence_end, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Circle DER"), (error != NULL) ? *error : NULL, error));
    
    return cir;
    
fail:
    CFReleaseNull(cir);
    return NULL;
}

SOSCircleRef SOSCircleCreateFromData(CFAllocatorRef allocator, CFDataRef circleData, CFErrorRef *error)
{
    size_t size = CFDataGetLength(circleData);
    const uint8_t *der = CFDataGetBytePtr(circleData);
    SOSCircleRef inflated = SOSCircleCreateFromDER(allocator, error, &der, der + size);
    return inflated;
}

size_t SOSCircleGetDEREncodedSize(SOSCircleRef cir, CFErrorRef *error) {
    SOSCircleAssertStable(cir);
    size_t total_payload = 0;
    
    require_quiet(accumulate_size(&total_payload, ccder_sizeof_uint64(kOnlyCompatibleVersion)),                        fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_string(cir->name, error)),                                fail);
    require_quiet(accumulate_size(&total_payload, SOSGenCountGetDEREncodedSize(cir->generation, error)),                          fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoSetGetDEREncodedArraySize(cir->peers, error)),            fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoSetGetDEREncodedArraySize(cir->applicants, error)),          fail);
    require_quiet(accumulate_size(&total_payload, SOSPeerInfoSetGetDEREncodedArraySize(cir->rejected_applicants, error)), fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_dictionary((CFDictionaryRef) cir->signatures, error)),    fail);
    
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, total_payload);
    
fail:
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSCircleEncodeToDER(SOSCircleRef cir, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    SOSCircleAssertStable(cir);
    
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                ccder_encode_uint64(kOnlyCompatibleVersion, der,
                der_encode_string(cir->name, error, der,
                SOSGenCountEncodeToDER(cir->generation, error, der,
                SOSPeerInfoSetEncodeToArrayDER(cir->peers, error, der,
                SOSPeerInfoSetEncodeToArrayDER(cir->applicants, error, der,
                SOSPeerInfoSetEncodeToArrayDER(cir->rejected_applicants, error, der,
                der_encode_dictionary((CFDictionaryRef) cir->signatures, error, der, der_end))))))));
}

CFDataRef SOSCircleCreateIncompatibleCircleDER(CFErrorRef* error)
{
    size_t total_payload = 0;
    size_t encoded_size = 0;
    uint8_t* der = 0;
    uint8_t* der_end = 0;
    CFMutableDataRef result = NULL;
    
    require_quiet(accumulate_size(&total_payload, ccder_sizeof_uint64(kAlwaysIncompatibleVersion)), fail);
    
    encoded_size = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, total_payload);
    
    result = CFDataCreateMutableWithScratch(kCFAllocatorDefault, encoded_size);
    
    der = CFDataGetMutableBytePtr(result);
    der_end = der + CFDataGetLength(result);
    
    der_end = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                          ccder_encode_uint64(kAlwaysIncompatibleVersion, der, der_end));
    
fail:
    if (der == NULL || der != der_end)
        CFReleaseNull(result);
    
    return result;
}


CFDataRef SOSCircleCopyEncodedData(SOSCircleRef circle, CFAllocatorRef allocator, CFErrorRef *error)
{
    return CFDataCreateWithDER(kCFAllocatorDefault, SOSCircleGetDEREncodedSize(circle, error), ^uint8_t*(size_t size, uint8_t *buffer) {
        return SOSCircleEncodeToDER(circle, error, buffer, (uint8_t *) buffer + size);
    });
}
