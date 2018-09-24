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
#include <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>

#include "secd_regressions.h"
#include "SOSAccountTesting.h"
#include "SecdTestKeychainUtilities.h"

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
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccount* bob_account = SOSTestCreateAccountAsSerialClone(CFSTR("Bob"), devClass, ghostSerialID, ghostIdsID);
    
    // Start Circle
    ok(SOSTestStartCircleWithAccount(alice_account, changes, cfaccount, cfpassword), "Have Alice start a circle");
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Joins");
    
    // Alice Leaves
    ok( [alice_account.trust leaveCircle:alice_account err:&error], "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    accounts_agree("Alice bails", bob_account, alice_account);
    is(countPeers(bob_account), 1, "There should only be 1 valid peer");
    // We're dropping all peers that are in the circle - leaving a circle with only one peer - and that's a ghost

    // Make new bob - same as the old bob except peerID

    SOSAccount* bobFinal = SOSTestCreateAccountAsSerialClone(CFSTR("BobFinal"), devClass, ghostSerialID, ghostIdsID);
    is(ProcessChangesUntilNoChange(changes, bobFinal, NULL), 1, "updates");

    ok(SOSTestJoinWith(cfpassword, cfaccount, changes, bobFinal), "Application Made");
    CFReleaseNull(cfpassword);

    // Did ghostbuster work?
    is(ProcessChangesUntilNoChange(changes, bobFinal, NULL), 2, "updates");
    if(expectGhostBusted) { // ghostbusting is currently disabled for MacOSX Peers
        ok([bobFinal isInCircle:NULL], "Bob is in");
    } else {
        ok(![bobFinal isInCircle:NULL], "Bob is not in");
    }

    is(countPeers(bobFinal), 1, "There should only be 1 valid peer");

    CFReleaseNull(changes);
    
    bob_account = nil;
    alice_account = nil;
    bobFinal = nil;
    SOSTestCleanup();
}

static void multiBob(SOSPeerInfoDeviceClass devClass, bool expectGhostBusted, bool delayedPrivKey, bool pairJoin) {
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef ghostSerialID = CFSTR("abababababab");
    CFStringRef ghostIdsID = CFSTR("targetIDS");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    
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
    
    SOSAccount* bobFinal_account = SOSTestCreateAccountAsSerialClone(CFSTR("BobFinal"), devClass, ghostSerialID, ghostIdsID);
    
    if(pairJoin) {
        SOSTestJoinThroughPiggyBack(cfpassword, cfaccount, changes, alice_account, bobFinal_account, KEEP_USERKEY, 6, true);
        is(countPeers(bobFinal_account), 6, "Expect ghosts still in circle");
    } else if(delayedPrivKey) {
        SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bobFinal_account, DROP_USERKEY, 6, true);
        is(countPeers(bobFinal_account), 6, "Expect ghosts still in circle");
    } else {
        SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bobFinal_account, KEEP_USERKEY, 2, true);
    }
    
    if(pairJoin || delayedPrivKey) { // this allows the ghostbusting to be done in a delayed fashion for the instances where that is proper
        SOSAccountTryUserCredentials(bobFinal_account, cfaccount, cfpassword, &error);
        ok(SOSTestChangeAccountDeviceName(bobFinal_account, CFSTR("ThereCanBeOnlyOneBob")), "force an unrelated circle change");
        is(ProcessChangesUntilNoChange(changes, alice_account, bobFinal_account, NULL), 3, "updates");
    }

    CFReleaseNull(cfpassword);

    
    ok([bobFinal_account isInCircle:NULL], "bobFinal_account is in");

    is(countPeers(bobFinal_account), 2, "Expect ghostBobs to be gone");
    is(countPeers(alice_account), 2, "Expect ghostBobs to be gone");
    accounts_agree_internal("Alice and ThereCanBeOnlyOneBob are the only circle peers and they agree", alice_account, bobFinal_account, false);

    CFReleaseNull(changes);

    alice_account = nil;
    bobFinal_account = nil;
    
    SOSTestCleanup();
}

static void iosICloudIdentity() {
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef ghostIdsID = CFSTR("targetIDS");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    
    // Start Circle
    ok(SOSTestStartCircleWithAccount(alice_account, changes, cfaccount, cfpassword), "Have Alice start a circle");
    
    SOSCircleRef circle = [alice_account.trust getCircle:NULL];
    __block CFStringRef serial = NULL;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            serial = SOSPeerInfoCopySerialNumber(peer);
        }
    });
    
    SOSAccount* bob_account = SOSTestCreateAccountAsSerialClone(CFSTR("Bob"), SOSPeerInfo_iOS, serial, ghostIdsID);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Joins");
    CFReleaseNull(cfpassword);

    circle = [alice_account.trust getCircle:&error];
    __block bool hasiCloudIdentity = false;
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            hasiCloudIdentity = true;
        }
    });

    ok(hasiCloudIdentity, "GhostBusting didn't mess with the iCloud Identity");
    
    CFReleaseNull(changes);
    alice_account = nil;
    SOSTestCleanup();
}

int secd_68_ghosts(int argc, char *const *argv)
{
    plan_tests(573);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    hauntedCircle(SOSPeerInfo_iOS, true);
    hauntedCircle(SOSPeerInfo_macOS, false);
    multiBob(SOSPeerInfo_iOS, true, false, false);
    multiBob(SOSPeerInfo_iOS, false, true, false);
    multiBob(SOSPeerInfo_iOS, false, false, true); // piggyback join case

    iosICloudIdentity();
    
    return 0;
}
