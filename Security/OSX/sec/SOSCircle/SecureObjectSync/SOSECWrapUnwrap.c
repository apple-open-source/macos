//
//  cc_CFData.c
//  ProtectedCloudStorage
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#include "SOSInternal.h"

#include <AssertMacros.h>

#include <CommonCrypto/CommonRandomSPI.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecBuffer.h>
#include <corecrypto/ccec.h>


#if 0 && defined(CCEC_RFC6637_DEBUG_KEYS)
#define DEBUGKEYS CCEC_RFC6637_DEBUG_KEYS
#else
#define DEBUGKEYS 0
#endif

const char fingerprint[20] = "fingerprint";

const uint8_t kAlgorithmID = 1;

CFMutableDataRef
SOSCopyECWrappedData(ccec_pub_ctx *ec_ctx, CFDataRef data, CFErrorRef *error)
{
    CFMutableDataRef result = NULL;
    CFMutableDataRef output = NULL;
    size_t outputLength;
    int res;

    require_quiet(SecRequirementError(data != NULL, error, CFSTR("data required for wrapping")), exit);
    require_quiet(SecRequirementError(ec_ctx != NULL, error, CFSTR("ec pub key required for wrapping")), exit);
    require_quiet(ec_ctx != NULL, exit);

    outputLength = ccec_rfc6637_wrap_key_size(ec_ctx, CCEC_RFC6637_COMPACT_KEYS | DEBUGKEYS, CFDataGetLength(data));

    output = CFDataCreateMutableWithScratch(NULL, outputLength);
    require_quiet(SecAllocationError(output, error, CFSTR("%s CFData allocation failed"), __FUNCTION__), exit);

    res = ccec_rfc6637_wrap_key(ec_ctx, CFDataGetMutableBytePtr(output), CCEC_RFC6637_COMPACT_KEYS | DEBUGKEYS, kAlgorithmID,
                                CFDataGetLength(data), CFDataGetBytePtr(data), &ccec_rfc6637_dh_curve_p256,
                                &ccec_rfc6637_wrap_sha256_kek_aes128, (const uint8_t *)fingerprint,
                                ccDevRandomGetRngState());
    require_noerr_action(res, exit, SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("Wrap failed with %d"), res));

    CFTransferRetained(result, output);

exit:
    CFReleaseNull(output);
    return result;
}

bool SOSPerformWithUnwrappedData(ccec_full_ctx_t ec_ctx, CFDataRef data, CFErrorRef *error,
                                 void (^operation)(size_t size, uint8_t *buffer))
{
    __block bool result = false;

    PerformWithBufferAndClear(CFDataGetLength(data), ^(size_t size, uint8_t *buffer) {
        size_t outputLength = size;
        int ec_result;
        uint8_t alg;
        ec_result = ccec_rfc6637_unwrap_key(ec_ctx, &outputLength, buffer,
                                            CCEC_RFC6637_COMPACT_KEYS | DEBUGKEYS, &alg, &ccec_rfc6637_dh_curve_p256,
                                            &ccec_rfc6637_unwrap_sha256_kek_aes128, (const uint8_t *)fingerprint,
                                            CFDataGetLength(data), CFDataGetBytePtr(data));

        require_noerr_action(ec_result, exit, SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("ccec_rfc6637_unwrap_key failed with %d"), ec_result));
        require_quiet(SecRequirementError(alg == kAlgorithmID, error, CFSTR("Unexpected algorithm: %d"), (int)alg), exit);

        operation(outputLength, buffer);

        result = true;
    exit:
        ;
    });

    return result;
}

CFMutableDataRef
SOSCopyECUnwrappedData(ccec_full_ctx_t ec_ctx, CFDataRef data, CFErrorRef *error)
{
    uint8_t alg;
    CFMutableDataRef result = NULL;
    CFMutableDataRef output = NULL;
    size_t outputLength = CFDataGetLength(data);
    int res;

    output = CFDataCreateMutableWithScratch(NULL, outputLength);
    require_quiet(SecAllocationError(output, error, CFSTR("%s CFData allocation failed"), __FUNCTION__), exit);

    res = ccec_rfc6637_unwrap_key(ec_ctx, &outputLength, CFDataGetMutableBytePtr(output),
                                  CCEC_RFC6637_COMPACT_KEYS | DEBUGKEYS, &alg, &ccec_rfc6637_dh_curve_p256,
                                  &ccec_rfc6637_unwrap_sha256_kek_aes128, (const uint8_t *)fingerprint,
                                  CFDataGetLength(data), CFDataGetBytePtr(data));
    require_noerr_action(res, exit, SOSErrorCreate(kSOSErrorProcessingFailure, error, NULL, CFSTR("Unwrap failed with %d"), res));
    require_quiet(SecRequirementError(alg == kAlgorithmID, error, CFSTR("Unexpected algorithm: %d"), (int)alg), exit);

    CFDataSetLength(output, outputLength);

    CFTransferRetained(result, output);

exit:
    CFReleaseNull(output);
    return result;
}
