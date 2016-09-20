//
//  SOSPeerInfoDER.c
//  sec
//
//  Created by Richard Murphy on 2/9/15.
//
//

#include <AssertMacros.h>
#include <SOSPeerInfoDER.h>

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <utilities/der_date.h>

#include <utilities/SecCFError.h>

size_t SOSPeerInfoGetDEREncodedSize(SOSPeerInfoRef peer, CFErrorRef *error) {
    size_t plist_size = der_sizeof_plist(peer->description, error);
    if (plist_size == 0)
        return 0;
    
    size_t signature_size = der_sizeof_data(peer->signature, error);
    if (signature_size == 0)
        return 0;
    
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        plist_size + signature_size);
}

uint8_t* SOSPeerInfoEncodeToDER(SOSPeerInfoRef peer, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    if(peer->version >= 2) SOSPeerInfoPackV2Data(peer);
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
                                       der_encode_plist(peer->description, error, der,
                                       der_encode_data(peer->signature, error, der, der_end)));
}

CFDataRef SOSPeerInfoCopyEncodedData(SOSPeerInfoRef peer, CFAllocatorRef allocator, CFErrorRef *error) {
    return CFDataCreateWithDER(kCFAllocatorDefault, SOSPeerInfoGetDEREncodedSize(peer, error), ^uint8_t*(size_t size, uint8_t *buffer) {
        return SOSPeerInfoEncodeToDER(peer, error, buffer, (uint8_t *) buffer + size);
    });
}



SOSPeerInfoRef SOSPeerInfoCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                        const uint8_t** der_p, const uint8_t *der_end) {
    SOSPeerInfoRef pi = SOSPeerInfoAllocate(allocator);
    SecKeyRef pubKey = NULL;
    const uint8_t *sequence_end;
    
    CFPropertyListRef pl = NULL;
    
    pi->gestalt = NULL;
    pi->version = 0; // TODO: Encode this in the DER
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    *der_p = der_decode_plist(allocator, kCFPropertyListImmutable, &pl, error, *der_p, sequence_end);
    *der_p = der_decode_data(allocator, kCFPropertyListImmutable, &pi->signature, error, *der_p, sequence_end);
    
    if (*der_p == NULL || *der_p != sequence_end) {
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Format of Peer Info DER"), NULL, error);
        goto fail;
    }
    
    if (CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(pl));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected dictionary got %@"), description);
        CFReleaseSafe(description);
        goto fail;
    }
    
    pi->description = (CFMutableDictionaryRef) pl;
    CFRetain(pi->description);
    CFReleaseNull(pl);
    
    CFNumberRef versionNumber = CFDictionaryGetValue(pi->description, sVersionKey);
    
    if (versionNumber) {
        if (CFGetTypeID(versionNumber) != CFNumberGetTypeID()) {
            CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(versionNumber));
            SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                     CFSTR("Expected (version) number got %@"), description);
            CFReleaseSafe(description);
            goto fail;
        }
        CFNumberGetValue(versionNumber, kCFNumberCFIndexType, &pi->version);
    }
    
    CFDictionaryRef gestalt = CFDictionaryGetValue(pi->description, sGestaltKey);
    
    if (gestalt == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("gestalt key missing"));
        goto fail;
    }
    
    if (!isDictionary(gestalt)) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(gestalt));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected dictionary got %@"), description);
        CFReleaseSafe(description);
        goto fail;
    }
    
    pi->gestalt = gestalt;
    CFRetain(pi->gestalt);
    
    pubKey = SOSPeerInfoCopyPubKey(pi, error);
    require_quiet(pubKey, fail);
    
    pi->id = SOSCopyIDOfKey(pubKey, error);
    require_quiet(pi->id, fail);
    
    if(pi->version >= 2) SOSPeerInfoExpandV2Data(pi, error);
    
    if(!SOSPeerInfoVerify(pi, error)) {
        SOSCreateErrorWithFormat(kSOSErrorBadSignature, NULL, error, NULL, CFSTR("Signature doesn't validate"));
        if (error)
            secerror("Can't validate PeerInfo: %@", *error);
        goto fail;
    }
    
    CFReleaseNull(pubKey);
    return pi;
    
fail:
    CFReleaseNull(pi);
    CFReleaseNull(pl);
    CFReleaseNull(pubKey);
    
    return NULL;
}

SOSPeerInfoRef SOSPeerInfoCreateFromData(CFAllocatorRef allocator, CFErrorRef* error,
                                         CFDataRef peerinfo_data) {
    const uint8_t *der = CFDataGetBytePtr(peerinfo_data);
    CFIndex len = CFDataGetLength(peerinfo_data);
    return SOSPeerInfoCreateFromDER(NULL, error, &der, der+len);
}

