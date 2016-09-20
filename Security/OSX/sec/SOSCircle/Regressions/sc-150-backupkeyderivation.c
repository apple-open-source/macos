//
//  sc-150-backupkeyderivation.c
//  sec
//
//

#include <stdio.h>

/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecRandom.h>

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"
#include "SOSInternal.h"

#if 0
static inline CFMutableDataRef CFDataCreateMutableWithRandom(CFAllocatorRef allocator, CFIndex size) {
    CFMutableDataRef result = NULL;
    CFMutableDataRef data = CFDataCreateMutableWithScratch(allocator, size);

    require_quiet(errSecSuccess == SecRandomCopyBytes(kSecRandomDefault, size, CFDataGetMutableBytePtr(data)), fail);

    CFTransferRetained(result, data);

fail:
    CFReleaseNull(data);

    return result;
}
#endif

static const uint8_t sEntropy1[] = {   0xc4, 0xb9, 0xa6, 0x6e, 0xeb, 0x56, 0xa1, 0x5c, 0x1d, 0x30, 0x09, 0x40,
    0x41, 0xe9, 0x68, 0xb4, 0x12, 0xe0, 0xc6, 0x69, 0xfb, 0xdf, 0xcb, 0xe0,
    0x27, 0x4b, 0x54, 0xf0, 0xdd, 0x62, 0x10, 0x78
};

static const uint8_t sEntropy2[] = {   0xef, 0xbd, 0x72, 0x57, 0x02, 0xe6, 0xbd, 0x0a, 0x22, 0x6e, 0x77, 0x93,
    0x17, 0xb3, 0x27, 0x12, 0x1b, 0x1f, 0xdf, 0xa0, 0x5b, 0xc6, 0x66, 0x54,
    0x3a, 0x91, 0x0d, 0xc1, 0x5f, 0x57, 0x98, 0x44
};

static const uint8_t sEntropy3[] = {   0xea, 0x06, 0x34, 0x93, 0xd7, 0x8b, 0xd6, 0x0d, 0xce, 0x83, 0x00 };


#define tests_count (6)
static void tests(void)
{
    ccec_const_cp_t cp = SOSGetBackupKeyCurveParameters();
    CFErrorRef error = NULL;
    CFDataRef entropy1 = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, sEntropy1, sizeof(sEntropy1), kCFAllocatorNull);
    CFDataRef entropy2 = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, sEntropy2, sizeof(sEntropy2), kCFAllocatorNull);
    CFDataRef entropy3 = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, sEntropy3, sizeof(sEntropy3), kCFAllocatorNull);

    ccec_full_ctx_decl_cp(cp, fullKey1);
    ccec_full_ctx_decl_cp(cp, fullKey1a);
    ccec_full_ctx_decl_cp(cp, fullKey2);
    ccec_full_ctx_decl_cp(cp, fullKey3);

    ok(SOSGenerateDeviceBackupFullKey(fullKey1, cp, entropy1, &error), "Generate key 1 (%@)", error);
    CFReleaseNull(error);

    ok(SOSGenerateDeviceBackupFullKey(fullKey1a, cp, entropy1, &error), "Generate key 1a (%@)", error);
    CFReleaseNull(error);

    ok(SOSGenerateDeviceBackupFullKey(fullKey2, cp, entropy2, &error), "Generate key 2 (%@)", error);
    CFReleaseNull(error);

    ok(SOSGenerateDeviceBackupFullKey(fullKey3, cp, entropy3, &error), "Generate key 3 (%@)", error);
    CFReleaseNull(error);

    size_t comparisonSize = ccec_full_ctx_size(ccec_ccn_size(cp));

    ok(memcmp(fullKey1, fullKey1a, comparisonSize), "Two derivations match");

    CFDataRef publicKeyData = SOSCopyDeviceBackupPublicKey(entropy1, &error);
    ok(publicKeyData, "Public key copy");
    CFReleaseNull(error);

    CFReleaseNull(publicKeyData);
    CFReleaseNull(entropy1);
    CFReleaseNull(entropy2);
    CFReleaseNull(entropy3);
}

static int kTestTestCount = tests_count;

int sc_150_backupkeyderivation(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

    return 0;
}
