/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <SecureObjectSync/SOSPeerInfoCollections.h>

#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecXPCError.h>
#include <corecrypto/ccder.h>
#include <SecureObjectSync/SOSInternal.h>
#include <AssertMacros.h>

//
// PeerInfoSetby ID handling
//

//
//
// CFSetRetainCallback
//

static Boolean		SOSPeerInfoIDEqual(const void *value1, const void *value2)
{
    SOSPeerInfoRef peer1 = (SOSPeerInfoRef) value1;
    SOSPeerInfoRef peer2 = (SOSPeerInfoRef) value2;
    
    return CFEqual(SOSPeerInfoGetPeerID(peer1), SOSPeerInfoGetPeerID(peer2));
}

static CFHashCode	SOSPeerInfoIDHash(const void *value)
{
    return CFHash(SOSPeerInfoGetPeerID((SOSPeerInfoRef) value));
}

bool SOSPeerInfoSetContainsIdenticalPeers(CFSetRef set1, CFSetRef set2){
    
    __block bool result = true;
    
    if(!CFEqualSafe(set1, set2))
        return false;
    
    CFSetForEach(set1, ^(const void *value) {
        SOSPeerInfoRef peer1 = (SOSPeerInfoRef)value;
        SOSPeerInfoRef peer2 = (SOSPeerInfoRef)CFSetGetValue(set2, peer1);
        result &= CFEqualSafe(peer1, peer2);
    });
    return result;
}
const CFSetCallBacks kSOSPeerSetCallbacks = {0, SecCFRetainForCollection, SecCFReleaseForCollection, CFCopyDescription, SOSPeerInfoIDEqual, SOSPeerInfoIDHash};

CFMutableSetRef CFSetCreateMutableForSOSPeerInfosByID(CFAllocatorRef allocator)
{
    return CFSetCreateMutable(allocator, 0, &kSOSPeerSetCallbacks);
}


//
// CFArray of Peer Info handling
//

void CFArrayOfSOSPeerInfosSortByID(CFMutableArrayRef peerInfoArray)
{
    CFArraySortValues(peerInfoArray, CFRangeMake(0, CFArrayGetCount(peerInfoArray)), SOSPeerInfoCompareByID, NULL);
}


//
// PeerInfoArray encoding decoding
//

CFMutableArrayRef SOSPeerInfoArrayCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                                const uint8_t** der_p, const uint8_t *der_end) {
    CFMutableArrayRef pia = CFArrayCreateMutableForCFTypes(allocator);
    
    const uint8_t *sequence_end;
    
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    
    require_action(*der_p, fail, SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Peer Info Array Sequence Header"), NULL, error));
    
    while (sequence_end != *der_p) {
        SOSPeerInfoRef pi = SOSPeerInfoCreateFromDER(allocator, error, der_p, sequence_end);
        
        if (pi == NULL || *der_p == NULL) {
            SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Peer Info Array DER"), (error != NULL ? *error : NULL), error);
            CFReleaseNull(pi);
            goto fail;
        }
        
        CFArrayAppendValue(pia, pi);
        CFReleaseSafe(pi);
    }
    
    if (!pia)
        *der_p = NULL;
    return pia;
    
fail:
    CFReleaseNull(pia);
    *der_p = NULL;
    return NULL;
}

size_t SOSPeerInfoArrayGetDEREncodedSize(CFArrayRef pia, CFErrorRef *error) {
    __block size_t array_size = 0;
    __block bool fail = false;
    
    CFArrayForEach(pia, ^(const void *value) {
        if (isSOSPeerInfo(value)) {
            SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
            size_t pi_size = SOSPeerInfoGetDEREncodedSize(pi, error);
            
            fail = (pi_size == 0);
            array_size += pi_size;
        } else {
            SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Non SOSPeerInfo in array"), (error != NULL ? *error : NULL), error);
            fail = true;
        }
    });
    
    return fail ? 0 : ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, array_size);
}

uint8_t* SOSPeerInfoArrayEncodeToDER(CFArrayRef pia, CFErrorRef* error, const uint8_t* der, uint8_t* der_end_param) {
    
    uint8_t* const sequence_end = der_end_param;
    __block uint8_t* der_end = der_end_param;
    
    CFArrayForEachReverse(pia, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        if (CFGetTypeID(pi) != SOSPeerInfoGetTypeID()) {
            SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Non SOSPeerInfo in array"), NULL, error);
            der_end = NULL; // Indicate error and continue.
        }
        if (der_end)
            der_end = SOSPeerInfoEncodeToDER(pi, error, der, der_end);
    });
    
    if (der_end == NULL)
        return NULL;
    
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, sequence_end, der, der_end);
}


CFMutableSetRef SOSPeerInfoSetCreateFromArrayDER(CFAllocatorRef allocator, const CFSetCallBacks *callbacks, CFErrorRef* error,
                                                 const uint8_t** der_p, const uint8_t *der_end) {
    CFMutableSetRef result = NULL;
    
    CFArrayRef peers = SOSPeerInfoArrayCreateFromDER(allocator, error, der_p, der_end);
    
    if (peers) {
        result = CFSetCreateMutable(allocator, 0, callbacks);
        CFSetSetValues(result, peers);
    }
    
    CFReleaseNull(peers);
    return result;
}

size_t SOSPeerInfoSetGetDEREncodedArraySize(CFSetRef pia, CFErrorRef *error) {
    __block size_t array_size = 0;
    __block bool fail = false;
    
    CFSetForEach(pia, ^(const void *value) {
        if (isSOSPeerInfo(value)) {
            SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
            size_t pi_size = SOSPeerInfoGetDEREncodedSize(pi, error);
            
            fail = (pi_size == 0);
            array_size += pi_size;
        } else {
            SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Non SOSPeerInfo in array"), (error != NULL ? *error : NULL), error);
            fail = true;
        }
    });
    
    return fail ? 0 : ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, array_size);
}

uint8_t* SOSPeerInfoSetEncodeToArrayDER(CFSetRef pis, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    CFArrayRef pia = CFSetCopyValues(pis);
    
    uint8_t* result = SOSPeerInfoArrayEncodeToDER(pia, error, der, der_end);
    
    CFReleaseNull(pia);
    
    return result;
}



CFArrayRef CreateArrayOfPeerInfoWithXPCObject(xpc_object_t peerArray, CFErrorRef* error) {
    if (!peerArray) {
        SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedNull, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("Unexpected Null Array to encode"));
        return NULL;
    }
    
    if (xpc_get_type(peerArray) != XPC_TYPE_DATA) {
        SecCFCreateErrorWithFormat(kSecXPCErrorUnexpectedType, sSecXPCErrorDomain, NULL, error, NULL, CFSTR("Array of peer info not array, got %@"), peerArray);
        return NULL;
    }
    
    const uint8_t* der = xpc_data_get_bytes_ptr(peerArray);
    const uint8_t* der_end = der + xpc_data_get_length(peerArray);
    
    return SOSPeerInfoArrayCreateFromDER(kCFAllocatorDefault, error, &der, der_end);
}

xpc_object_t CreateXPCObjectWithArrayOfPeerInfo(CFArrayRef array, CFErrorRef *error) {
    size_t data_size = SOSPeerInfoArrayGetDEREncodedSize(array, error);
    if (data_size == 0)
        return NULL;
    uint8_t *data = (uint8_t *)malloc(data_size);
    if (!data) return NULL;
    
    xpc_object_t result = NULL;
    if (SOSPeerInfoArrayEncodeToDER(array, error, data, data + data_size))
        result = xpc_data_create(data, data_size);
    
    free(data);
    return result;
}
