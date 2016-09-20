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


#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecRandom.h>

#include "SOSInternal.h"

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"

#define encode_decode_count 2
#if !TARGET_IPHONE_SIMULATOR
static SOSBackupSliceKeyBagRef EncodeDecode(SOSBackupSliceKeyBagRef bag)
{
    SOSBackupSliceKeyBagRef result = NULL;
    CFErrorRef localError = NULL;
    SKIP: {
        skip("No bag to use", 2, bag);
        CFDataRef encoded = SOSBSKBCopyEncoded(bag, &localError);
        ok(encoded, "encode (%@)", localError);
        CFReleaseNull(localError);

        skip("Encode failed", 1, encoded);
        result = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, encoded, &localError);
        ok(result, "create (%@)", localError);
        CFReleaseNull(localError);

        CFReleaseNull(encoded);
    }

    return result;
}
#endif

#if 0
static CFDataRef CFDataCreateWithRandom(CFAllocatorRef allocator, size_t size) {
    CFMutableDataRef result = CFDataCreateMutableWithScratch(allocator, size);

    SecRandomCopyBytes(kSecRandomDefault, size, CFDataGetMutableBytePtr(result));

    return result;
}
#endif

static const uint8_t sEntropy1[] = {
    0xc4, 0xb9, 0xa6, 0x6e, 0xeb, 0x56, 0xa1, 0x5c, 0x1d, 0x30, 0x09, 0x40,
    0x41, 0xe9, 0x68, 0xb4, 0x12, 0xe0, 0xc6, 0x69, 0xfb, 0xdf, 0xcb, 0xe0,
    0x27, 0x4b, 0x54, 0xf0, 0xdd, 0x62, 0x10, 0x78
};

static const uint8_t sEntropy2[] = {
    0xef, 0xbd, 0x72, 0x57, 0x02, 0xe6, 0xbd, 0x0a, 0x22, 0x6e, 0x77, 0x93,
    0x17, 0xb3, 0x27, 0x12, 0x1b, 0x1f, 0xdf, 0xa0, 0x5b, 0xc6, 0x66, 0x54,
    0x3a, 0x91, 0x0d, 0xc1, 0x5f, 0x57, 0x98, 0x44
};

#if !TARGET_IPHONE_SIMULATOR
    #define tests_count (8 + encode_decode_count)
#else
    #define tests_count (6)
#endif

static void tests(void)
{
    CFErrorRef localError = NULL;
    CFMutableSetRef piSet = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);

    CFDataRef entropy1 = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, sEntropy1, sizeof(sEntropy1), kCFAllocatorNull);
    CFDataRef entropy2 = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, sEntropy2, sizeof(sEntropy2), kCFAllocatorNull);

    SecKeyRef peer1SigningKey = NULL;
    SOSFullPeerInfoRef fullPeer1WithBackup = SOSCreateFullPeerInfoFromName(CFSTR("peer1WithBackupID"), &peer1SigningKey, &localError);
    ok(fullPeer1WithBackup, "Allocate peer 1 (%@)", localError);
    CFReleaseNull(localError);

    CFDataRef peer1BackupPublic = SOSCopyDeviceBackupPublicKey(entropy1, &localError);
    ok(peer1BackupPublic, "Got backup key (%@)", localError);
    CFReleaseNull(localError);

    SOSFullPeerInfoUpdateBackupKey(fullPeer1WithBackup, peer1BackupPublic, &localError);

    SOSPeerInfoRef peer1WithBackup = SOSFullPeerInfoGetPeerInfo(fullPeer1WithBackup);

    SecKeyRef peer2SigningKey = NULL;
    SOSFullPeerInfoRef fullPeer2WithBackup = SOSCreateFullPeerInfoFromName(CFSTR("peer2WithBackupID"), &peer2SigningKey, &localError);
    ok(fullPeer2WithBackup, "Allocate peer 2 (%@)", localError);
    CFReleaseNull(localError);

    CFDataRef peer2BackupPublic = SOSCopyDeviceBackupPublicKey(entropy2, &localError);
    ok(peer2BackupPublic, "Got backup key (%@)", localError);
    CFReleaseNull(localError);

    SOSFullPeerInfoUpdateBackupKey(fullPeer2WithBackup, peer2BackupPublic, &localError);

    SOSPeerInfoRef peer2WithBackup = SOSFullPeerInfoGetPeerInfo(fullPeer2WithBackup);

    SOSBackupSliceKeyBagRef vb = SOSBackupSliceKeyBagCreate(kCFAllocatorDefault, piSet, &localError);
    ok(vb == NULL, "Should fail with no peers (%@)", localError);
    CFReleaseNull(localError);
    CFReleaseNull(vb);

    CFSetAddValue(piSet, peer1WithBackup);
    CFSetAddValue(piSet, peer2WithBackup);

    SOSBackupSliceKeyBagRef vb2 = NULL;

#if !TARGET_IPHONE_SIMULATOR
    vb = SOSBackupSliceKeyBagCreate(kCFAllocatorDefault, piSet, &localError);
    ok(vb != NULL, "Allocation: (%@)", localError);
    CFReleaseNull(localError);

    vb2 = EncodeDecode(vb);

    ok(vb2 != NULL, "transcoded");
#endif
#if 0
    // <rdar://problem/20561988> Have helper functions for new security object that load bags
    keybag_handle_t ourHandle = SOSBSKBLoadAndUnlockWithPeerSecret(vb, peer2WithBackup, entropy2, &localError);
    ok(ourHandle != bad_keybag_handle, "loaded with peer secret, handle %d (%@)", ourHandle, localError);
    CFReleaseNull(localError);

    aks_unload_bag(ourHandle);
#else
TODO:{
    todo("no simulator supprt");
    ok(false);
    }
#endif

    CFReleaseNull(vb);
    CFReleaseNull(vb2);
    CFReleaseNull(piSet);

    CFReleaseNull(peer1SigningKey);
    CFReleaseNull(peer1BackupPublic);
    CFReleaseNull(fullPeer1WithBackup);

    CFReleaseNull(peer2SigningKey);
    CFReleaseNull(peer2BackupPublic);
    CFReleaseNull(fullPeer2WithBackup);

    CFReleaseNull(entropy1);
    CFReleaseNull(entropy2);
}

static int kTestTestCount = tests_count;

int sc_153_backupslicekeybag(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

    return 0;
}
