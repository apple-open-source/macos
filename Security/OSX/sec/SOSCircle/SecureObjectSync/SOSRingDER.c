//
//  SOSRingDER.c
//  sec
//
//  Created by Richard Murphy on 3/3/15.
//
//

#include "SOSRingDER.h"
#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFWrappers.h>

//#include "ckdUtilities.h"

#include <corecrypto/ccder.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>


#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <utilities/der_date.h>

#include <stdlib.h>
#include <assert.h>

#include "SOSRingUtils.h"

size_t SOSRingGetDEREncodedSize(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    size_t total_payload = 0;

    require_quiet(accumulate_size(&total_payload, der_sizeof_dictionary((CFDictionaryRef) ring->unSignedInformation, error)), fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_dictionary((CFDictionaryRef) ring->signedInformation, error)), fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_dictionary((CFDictionaryRef) ring->signatures, error)), fail);
    require_quiet(accumulate_size(&total_payload, der_sizeof_dictionary((CFDictionaryRef) ring->data, error)), fail);

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, total_payload);
fail:
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSRingEncodeToDER(SOSRingRef ring, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
        der_encode_dictionary(ring->unSignedInformation, error, der,
        der_encode_dictionary(ring->signedInformation, error, der,
        der_encode_dictionary(ring->signatures, error, der,
        der_encode_dictionary(ring->data, error, der, der_end)))));
}

CFDataRef SOSRingCopyEncodedData(SOSRingRef ring, CFErrorRef *error) {
    return CFDataCreateWithDER(kCFAllocatorDefault, SOSRingGetDEREncodedSize(ring, error), ^uint8_t*(size_t size, uint8_t *buffer) {
        return SOSRingEncodeToDER(ring, error, buffer, (uint8_t *) buffer + size);
    });
}

SOSRingRef SOSRingCreateFromDER(CFErrorRef* error, const uint8_t** der_p, const uint8_t *der_end) {
    SOSRingRef ring = SOSRingAllocate();
    SOSRingRef retval = NULL;
    const uint8_t *sequence_end;
    CFDictionaryRef unSignedInformation = NULL;
    CFDictionaryRef signedInformation = NULL;
    CFDictionaryRef signatures = NULL;
    CFDictionaryRef data = NULL;

    require_action_quiet(ring, errOut, secnotice("ring", "Unable to allocate ring"));
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    *der_p = der_decode_dictionary(ALLOCATOR, kCFPropertyListImmutable, &unSignedInformation, error, *der_p, sequence_end);
    *der_p = der_decode_dictionary(ALLOCATOR, kCFPropertyListImmutable, &signedInformation, error, *der_p, sequence_end);
    *der_p = der_decode_dictionary(ALLOCATOR, kCFPropertyListImmutable, &signatures, error, *der_p, sequence_end);
    *der_p = der_decode_dictionary(ALLOCATOR, kCFPropertyListImmutable, &data, error, *der_p, sequence_end);

    require_action_quiet(*der_p, errOut, secnotice("ring", "Unable to decode DER"));
    require_action_quiet(*der_p == der_end, errOut, secnotice("ring", "Unable to decode DER"));

    ring->unSignedInformation = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, unSignedInformation);
    ring->signedInformation = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, signedInformation);
    ring->signatures = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, signatures);
    ring->data = CFDictionaryCreateMutableCopy(ALLOCATOR, 0, data);
    retval = ring;
    ring = NULL;

errOut:
    CFReleaseNull(unSignedInformation);
    CFReleaseNull(signedInformation);
    CFReleaseNull(signatures);
    CFReleaseNull(data);
    CFReleaseNull(ring);

    return retval;
}

SOSRingRef SOSRingCreateFromData(CFErrorRef* error, CFDataRef ring_data) {
    const uint8_t *der = CFDataGetBytePtr(ring_data);
    CFIndex len = CFDataGetLength(ring_data);
    return SOSRingCreateFromDER(error, &der, der+len);
}
