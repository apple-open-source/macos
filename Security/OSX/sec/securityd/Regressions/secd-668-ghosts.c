/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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

//
//  secd-668-ghosts.c
//  sec
//

#include <CoreFoundation/CFDictionary.h>
#include <utilities/SecCFWrappers.h>

#include <Security/SecureObjectSync/SOSAccount.h>

#include "secd_regressions.h"
#include "SOSAccountTesting.h"
#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 538;

/*
 Make a circle with two peers - alice and bob(bob is iOS and serial#"abababababab")
 have alice leave the circle
 release bob, make a new bob - iOS and same serial number
 try to join the circle - it should resetToOffering with ghost fix
 
 For phase 1 we expect the ghostfix to work with iOS devices, but not with MacOSX devices.
 */

static void hauntedCircle(SOSPeerInfoDeviceClass devClass, bool expectGhostBusted)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef ghostSerialID = CFSTR("abababababab");
    CFStringRef ghostIdsID = CFSTR("targetIDS");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = SOSTestCreateAccountAsSerialClone(CFSTR("Bob"), devClass, ghostSerialID, ghostIdsID);
    
    // Start Circle
    ok(SOSTestStartCircleWithAccount(alice_account, changes, cfaccount, cfpassword), "Have Alice start a circle");
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Joins");
    
    // Alice Leaves
    ok(SOSAccountLeaveCircle(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    accounts_agree("Alice bails", bob_account, alice_account);
    is(countPeers(bob_account), 1, "There should only be 1 valid peer");
    // We're dropping all peers that are in the circle - leaving a circle with only one peer - and that's a ghost
    CFReleaseNull(alice_account);
    CFReleaseNull(bob_account);

    // Make new bob - same as the old bob except peerID

    SOSAccountRef bobFinal = SOSTestCreateAccountAsSerialClone(CFSTR("BobFinal"), devClass, ghostSerialID, ghostIdsID);
    ok(SOSTestJoinWith(cfpassword, cfaccount, changes, bobFinal), "Application Made");
    
    // Did ghostbuster work?
    is(ProcessChangesUntilNoChange(changes, bobFinal, NULL), 2, "updates");
    if(expectGhostBusted) { // ghostbusting is currently disabled for MacOSX Peers
        ok(SOSAccountIsInCircle(bobFinal, NULL), "Bob is in");
    } else {
        ok(!SOSAccountIsInCircle(bobFinal, NULL), "Bob is not in");
    }

    is(countPeers(bobFinal), 1, "There should only be 1 valid peer");

    CFReleaseNull(bobFinal);
    CFReleaseNull(changes);

    SOSTestCleanup();
}

static void multiBob(SOSPeerInfoDeviceClass devClass, bool expectGhostBusted, bool delayedPrivKey) {
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef ghostSerialID = CFSTR("abababababab");
    CFStringRef ghostIdsID = CFSTR("targetIDS");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    
    // Start Circle
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    is(ProcessChangesUntilNoChange(changes, alice_account, NULL), 1, "updates");
    
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, NULL), 1, "updates");
    
    SOSTestMakeGhostInCircle(CFSTR("Bob1"), devClass, ghostSerialID, ghostIdsID, cfpassword, cfaccount, changes, alice_account, 2);
    SOSTestMakeGhostInCircle(CFSTR("Bob2"), devClass, ghostSerialID, ghostIdsID, cfpassword, cfaccount, changes, alice_account, 3);
    SOSTestMakeGhostInCircle(CFSTR("Bob3"), devClass, ghostSerialID, ghostIdsID, cfpassword, cfaccount, changes, alice_account, 4);
    SOSTestMakeGhostInCircle(CFSTR("Bob4"), devClass, ghostSerialID, ghostIdsID, cfpassword, cfaccount, changes, alice_account, 5);
    
    SOSAccountRef bobFinal_account = SOSTestCreateAccountAsSerialClone(CFSTR("BobFinal"), devClass, ghostSerialID, ghostIdsID);
    if(delayedPrivKey) {
        SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bobFinal_account, DROP_USERKEY, 6, true);
        is(countPeers(bobFinal_account), 6, "Expect ghosts still in circle");
        SOSAccountTryUserCredentials(bobFinal_account, cfaccount, cfpassword, &error);
        ok(SOSTestChangeAccountDeviceName(bobFinal_account, CFSTR("ThereCanBeOnlyOneBob")), "force an unrelated circle change");
        is(ProcessChangesUntilNoChange(changes, alice_account, bobFinal_account, NULL), 3, "updates");
    } else {
        SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bobFinal_account, KEEP_USERKEY, 2, true);
    }

    is(countPeers(bobFinal_account), 2, "Expect ghostBobs to be gone");
    is(countPeers(alice_account), 2, "Expect ghostBobs to be gone");
    accounts_agree_internal("Alice and ThereCanBeOnlyOneBob are the only circle peers and they agree", alice_account, bobFinal_account, false);

    CFReleaseNull(bobFinal_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(changes);

    SOSTestCleanup();
}

static void iosICloudIdentity() {
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef ghostIdsID = CFSTR("targetIDS");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    
    // Start Circle
    ok(SOSTestStartCircleWithAccount(alice_account, changes, cfaccount, cfpassword), "Have Alice start a circle");
    
    SOSCircleRef circle = SOSAccountGetCircle(alice_account, &error);
    __block CFStringRef serial = NULL;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            serial = SOSPeerInfoCopySerialNumber(peer);
        }
    });
    
    SOSAccountRef bob_account = SOSTestCreateAccountAsSerialClone(CFSTR("Bob"), SOSPeerInfo_iOS, serial, ghostIdsID);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Joins");

    circle = SOSAccountGetCircle(alice_account, &error);
    __block bool hasiCloudIdentity = false;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            hasiCloudIdentity = true;
        }
    });

    ok(hasiCloudIdentity, "GhostBusting didn't mess with the iCloud Identity");
    
    CFReleaseNull(alice_account);
    CFReleaseNull(bob_account);
    CFReleaseNull(changes);
    
    SOSTestCleanup();
}

int secd_668_ghosts(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    hauntedCircle(SOSPeerInfo_iOS, true);
    hauntedCircle(SOSPeerInfo_macOS, false);
    multiBob(SOSPeerInfo_iOS, true, false);
    multiBob(SOSPeerInfo_iOS, false, true);
    
    iosICloudIdentity();
    
    return 0;
}
