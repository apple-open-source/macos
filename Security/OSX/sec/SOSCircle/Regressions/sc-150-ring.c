//
//  sc-150-ring.c
//  sec
//
//  Created by Richard Murphy on 3/3/15.
//
//

#include <stdio.h>
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



#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKeyPriv.h>

#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSRingTypes.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"

static SOSFullPeerInfoRef SOSCreateApplicantFullPeerInfoFromName(CFStringRef peerName, SecKeyRef user_private_key,
                                                                 SecKeyRef* outSigningKey, CFErrorRef *error)
{
    SOSFullPeerInfoRef result = NULL;
    SOSFullPeerInfoRef fullPeer = SOSCreateFullPeerInfoFromName(peerName, outSigningKey, error);

    if (fullPeer && SOSFullPeerInfoPromoteToApplication(fullPeer, user_private_key, error))
        CFTransferRetained(result, fullPeer);

    CFReleaseNull(fullPeer);
    return result;
}

static int kTestTestCount = 24;
static void tests(void)
{

    //SecKeyRef publicKey = NULL;
    SecKeyRef dev_a_key = NULL;
    SecKeyRef dev_b_key = NULL;
    SecKeyRef dev_c_key = NULL;
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);

    ok(cfpassword, "no password");

    CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
    ok(parameters, "No parameters!");
    ok(error == NULL, "Error: (%@)", error);
    CFReleaseNull(error);

    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, &error);
    CFReleaseNull(parameters);

    SecKeyRef user_pubkey = SecKeyCreatePublicFromPrivate(user_privkey);


    SOSFullPeerInfoRef peer_a_full_info = SOSCreateApplicantFullPeerInfoFromName(CFSTR("Peer A"), user_privkey, &dev_a_key, NULL);
    SOSFullPeerInfoRef peer_b_full_info = SOSCreateApplicantFullPeerInfoFromName(CFSTR("Peer B"), user_privkey, &dev_b_key, NULL);
    SOSFullPeerInfoRef peer_c_full_info = SOSCreateApplicantFullPeerInfoFromName(CFSTR("Peer C"), user_privkey, &dev_c_key, NULL);
    CFStringRef peerID_a = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(peer_a_full_info));
    CFStringRef peerID_b = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(peer_b_full_info));
    SOSRingRef Ring = SOSRingCreate(CFSTR("TESTRING"), peerID_a, kSOSRingBase, NULL);

    ok(Ring, "Ring creation");


    ok(0 == SOSRingCountPeers(Ring), "Zero peers");

    ok(SOSRingApply(Ring, user_pubkey, peer_a_full_info, NULL));
    ok(SOSRingApply(Ring, user_pubkey, peer_b_full_info, NULL));

    ok(2 == SOSRingCountPeers(Ring), "Two peers");

    ok(SOSRingWithdraw(Ring, user_privkey, peer_b_full_info, NULL));

    ok(1 == SOSRingCountPeers(Ring), "One peer");

    ok(kSOSRingMember == SOSRingDeviceIsInRing(Ring, peerID_a), "peer_a is in Ring");
    ok(kSOSRingNotInRing == SOSRingDeviceIsInRing(Ring, peerID_b), "peer_b is not in Ring");
    CFStringRef lastmod = SOSRingGetLastModifier(Ring);
    ok(CFEqual(lastmod, peerID_b), "peer_b_full_info did last mod");

    ok(SOSRingResetToEmpty(Ring, peerID_a, NULL), "Reset the circle");
    ok(kSOSRingNotInRing == SOSRingDeviceIsInRing(Ring, peerID_a), "peer_a is not in Ring");

    ok(SOSRingResetToOffering(Ring, NULL, peer_a_full_info, NULL), "Reset Ring to Offering for PeerA");
    ok(kSOSRingMember == SOSRingDeviceIsInRing(Ring, peerID_a), "peer_a is in Ring");
    ok(kSOSRingNotInRing == SOSRingDeviceIsInRing(Ring, peerID_b), "peer_b is not in Ring");

    CFDataRef ringDER = SOSRingCopyEncodedData(Ring, NULL);
    ok(ringDER, "Successful encoding to DER of Ring");
    SOSRingRef Ring2 = SOSRingCreateFromData(NULL, ringDER);
    ok(Ring2, "Successful decoding of DER to Ring");

    ok(CFEqualSafe(Ring, Ring2), "Compares");

    ok(SOSRingApply(Ring, user_pubkey, peer_c_full_info, NULL));
    ok(SOSRingApply(Ring, user_pubkey, peer_b_full_info, NULL));

    CFReleaseNull(ringDER);
    CFReleaseNull(Ring2);
    ringDER = SOSRingCopyEncodedData(Ring, NULL);
    Ring2 = SOSRingCreateFromData(NULL, ringDER);
    ok(CFEqualSafe(Ring, Ring2), "Compares");

    CFReleaseNull(ringDER);
    CFReleaseNull(Ring2);
    CFReleaseNull(dev_a_key);
    CFReleaseNull(dev_b_key);
    CFReleaseNull(dev_c_key);
    CFReleaseNull(cfpassword);

    CFReleaseNull(user_privkey);
    CFReleaseNull(user_pubkey);

    CFReleaseNull(peer_a_full_info);
    CFReleaseNull(peer_b_full_info);
    CFReleaseNull(peer_c_full_info);
    CFReleaseNull(Ring);
}

int sc_150_Ring(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

    return 0;
}
