//
//  secd-66-account-recovery.c
//  Security
//
//  Created by Richard Murphy on 10/5/16.
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

#include <CoreFoundation/CFDictionary.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSViews.h>

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include "SecRecoveryKey.h"

#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>
#include "SecdTestKeychainUtilities.h"

#if TARGET_IPHONE_SIMULATOR

int secd_66_account_recovery(int argc, char *const *argv) {
    plan_tests(1);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    return 0;
}

#else

#include "SOSAccountTesting.h"


static CFDataRef CopyBackupKeyForString(CFStringRef string, CFErrorRef *error)
{
    __block CFDataRef result = NULL;
    CFStringPerformWithUTF8CFData(string, ^(CFDataRef stringAsData) {
        result = SOSCopyDeviceBackupPublicKey(stringAsData, error);
    });
    return result;
}

// 6 test cases
static void registerRecoveryKeyNow(CFMutableDictionaryRef changes, SOSAccountRef registrar, SOSAccountRef observer, CFDataRef recoveryPub, bool recKeyFirst) {
    CFErrorRef error = NULL;

    ok(SOSAccountSetRecoveryKey(registrar, recoveryPub, &error), "Set Recovery Key");
    CFReleaseNull(error);
    
    int nchanges = (recKeyFirst) ? 3: 4;
    is(ProcessChangesUntilNoChange(changes, registrar, observer, NULL), nchanges, "updates");
    
    CFDataRef registrar_recKey = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, registrar, &error);
    ok(registrar_recKey, "Registrar retrieved recKey");
    CFReleaseNull(error);
    
    CFDataRef observer_recKey = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, observer, &error);
    ok(observer_recKey, "Observer retrieved recKey");
    CFReleaseNull(error);
    
    ok(CFEqualSafe(registrar_recKey, observer_recKey), "recKeys are the same");
    ok(CFEqualSafe(registrar_recKey, recoveryPub), "recKeys are as expected");
    CFReleaseNull(observer_recKey);
    CFReleaseNull(registrar_recKey);
}

static void tests(bool recKeyFirst)
{
    __block CFErrorRef error = NULL;
    CFStringRef sock_drawer_key = CFSTR("AAAA-AAAA-AAAA-AAAA-AAAA-AAAA-AAGW");
    SecRecoveryKey *sRecKey = NULL;
    CFDataRef fullKeyBytes = NULL;
    CFDataRef pubKeyBytes = NULL;
    
    sRecKey = SecRKCreateRecoveryKey(sock_drawer_key);
    ok(sRecKey, "Create SecRecoveryKey from String");
    if(sRecKey) {
        fullKeyBytes = SecRKCopyBackupFullKey(sRecKey);
        pubKeyBytes = SecRKCopyBackupPublicKey(sRecKey);
        ok(fullKeyBytes && pubKeyBytes, "Got KeyPair from SecRecoveryKey");
    }
    if(!(fullKeyBytes && pubKeyBytes)) {
        diag("Cannot Proceed - couldn't make usable recoveryKey from sock-drawer-key");
        CFReleaseNull(fullKeyBytes);
        CFReleaseNull(pubKeyBytes);
        return;
    }

    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef cfdsid = CFSTR("DSIDFooFoo");
    
    secd_test_setup_testviews(); // for running this test solo
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountAssertDSID(alice_account, cfdsid);
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountAssertDSID(bob_account, cfdsid);

    CFDataRef alice_backup_key = CopyBackupKeyForString(CFSTR("Alice Backup Entropy"), &error);
    CFDataRef bob_backup_key = CopyBackupKeyForString(CFSTR("Bob Backup Entropy"), &error);
    
    // Start Circle
    ok(SOSTestStartCircleWithAccount(alice_account, changes, cfaccount, cfpassword), "Have Alice start a circle");
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Joins");
    
    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    if(recKeyFirst) registerRecoveryKeyNow(changes, alice_account, bob_account, pubKeyBytes, recKeyFirst);
    
    is(SOSAccountUpdateView(alice_account, kTestView1, kSOSCCViewEnable, &error), kSOSCCViewMember, "Enable view (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountCheckForRings(alice_account, &error), "Alice_account is good");
    CFReleaseNull(error);
    
    is(SOSAccountUpdateView(bob_account, kTestView1, kSOSCCViewEnable, &error), kSOSCCViewMember, "Enable view (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountCheckForRings(bob_account, &error), "Bob_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountSetBackupPublicKey_wTxn(alice_account, alice_backup_key, &error), "Set backup public key, alice (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountCheckForRings(alice_account, &error), "Alice_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountSetBackupPublicKey_wTxn(bob_account, bob_backup_key, &error), "Set backup public key, bob (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountCheckForRings(bob_account, &error), "Bob_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(alice_account, kTestView1), "Is alice is in backup before sync?");
    
    if(!recKeyFirst) {
        SOSBackupSliceKeyBagRef bskb = SOSAccountBackupSliceKeyBagForView(alice_account, kTestView1, &error);
        CFReleaseNull(error);
        ok(!SOSBSKBHasRecoveryKey(bskb), "BSKB should not have recovery key");
        CFReleaseNull(bskb);
    }

    ok(SOSAccountCheckForRings(alice_account, &error), "Alice_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(bob_account, kTestView1), "Is bob in the backup after sync? - 1");
    
    ok(SOSAccountCheckForRings(bob_account, &error), "Alice_account is good");
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 4, "updates");
    
    
    ok(SOSAccountCheckForRings(alice_account, &error), "Alice_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(alice_account, kTestView1), "Is alice is in backup after sync?");
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(bob_account, kTestView1), "IS bob in the backup after sync");
    
    ok(!SOSAccountIsLastBackupPeer(alice_account, &error), "Alice is not last backup peer");
    CFReleaseNull(error);
    
    //
    //Bob leaves the circle
    //
    ok(SOSAccountLeaveCircle(bob_account, &error), "Bob Leaves (%@)", error);
    CFReleaseNull(error);
    
    //Alice should kick Bob out of the backup!
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(alice_account, kTestView1), "Bob left the circle, Alice is not in the backup");
    
    ok(SOSAccountIsLastBackupPeer(alice_account, &error), "Alice is last backup peer");
    CFReleaseNull(error);
    ok(!SOSAccountIsLastBackupPeer(bob_account, &error), "Bob is not last backup peer");
    CFReleaseNull(error);
    
    ok(testAccountPersistence(alice_account), "Test Account->DER->Account Equivalence");
    
    ok(!SOSAccountIsPeerInBackupAndCurrentInView(alice_account, SOSFullPeerInfoGetPeerInfo(bob_account->my_identity), kTestView1), "Bob is still in the backup!");
    
    //Bob gets back into the circle
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Re-Joins");
    
    //enables view
    is(SOSAccountUpdateView(bob_account, kTestView1, kSOSCCViewEnable, &error), kSOSCCViewMember, "Enable view (%@)", error);
    CFReleaseNull(error);
    
    ok(!SOSAccountIsMyPeerInBackupAndCurrentInView(bob_account, kTestView1), "Bob isn't in the backup yet");
    
    ok(!SOSAccountIsLastBackupPeer(alice_account, &error), "Alice is not last backup peer - Bob still registers as one");
    CFReleaseNull(error);
    
    ok(SOSAccountSetBackupPublicKey_wTxn(bob_account, bob_backup_key, &error), "Set backup public key, alice (%@)", error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    
    ok(!SOSAccountIsLastBackupPeer(alice_account, &error), "Alice is not last backup peer");
    CFReleaseNull(error);
    
    //
    //removing backup key for bob account
    //
    
    ok(SOSAccountRemoveBackupPublickey_wTxn(bob_account, &error), "Removing Bob's backup key (%@)", error);
    int nchanges = (recKeyFirst) ? 4: 3;
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), nchanges, "updates");
    
    ok(!SOSAccountIsMyPeerInBackupAndCurrentInView(bob_account, kTestView1), "Bob's backup key is in the backup - should not be so!");
    ok(!SOSAccountIsPeerInBackupAndCurrentInView(alice_account, SOSFullPeerInfoGetPeerInfo(bob_account->my_identity), kTestView1), "Bob is up to date in the backup!");
    
    //
    // Setting new backup public key for Bob
    //
    
    ok(SOSAccountSetBackupPublicKey_wTxn(bob_account, bob_backup_key, &error), "Set backup public key, alice (%@)", error);
    CFReleaseNull(error);
    
    is(SOSAccountUpdateView(bob_account, kTestView1, kSOSCCViewEnable, &error), kSOSCCViewMember, "Enable view (%@)", error);
    ok(SOSAccountNewBKSBForView(bob_account, kTestView1, &error), "Setting new backup public key for bob account failed: (%@)", error);
    
    //bob is in his own backup
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(bob_account, kTestView1), "Bob's backup key is not in the backup");
    //alice does not have bob in her backup
    ok(!SOSAccountIsPeerInBackupAndCurrentInView(alice_account, SOSFullPeerInfoGetPeerInfo(bob_account->my_identity), kTestView1), "Bob is up to date in the backup - should not be so!");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 5, "updates");
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(bob_account, kTestView1), "Bob's backup key should be in the backup");
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView(alice_account, kTestView1), "Alice is in the backup");
    
    if(!recKeyFirst) registerRecoveryKeyNow(changes, alice_account, bob_account, pubKeyBytes, recKeyFirst);
    
    ok(SOSAccountRecoveryKeyIsInBackupAndCurrentInView(alice_account, kTestView1), "Recovery Key is also in the backup");
    ok(SOSAccountRecoveryKeyIsInBackupAndCurrentInView(bob_account, kTestView1), "Recovery Key is also in the backup");
    
    SOSBackupSliceKeyBagRef bskb = SOSAccountBackupSliceKeyBagForView(alice_account, kTestView1, &error);
    CFReleaseNull(error);
    
    ok(SOSBSKBHasRecoveryKey(bskb), "BSKB should have recovery key");

    CFDataRef wrappingKey = CFStringCreateExternalRepresentation(kCFAllocatorDefault, sock_drawer_key, kCFStringEncodingUTF8, 0);
    ok(wrappingKey, "Made wrapping key from with sock drawer key");
    bskb_keybag_handle_t bskbHandle = SOSBSKBLoadAndUnlockWithWrappingSecret(bskb, wrappingKey, &error);
    ok(bskbHandle, "Made bskbHandle with recover key");
    
    ok(SOSAccountResetToEmpty(alice_account, &error), "Reset circle to empty");
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    ok(SOSAccountIsBackupRingEmpty(bob_account, kTestView1), "Bob should not be in the backup");
    ok(SOSAccountIsBackupRingEmpty(alice_account, kTestView1), "Alice should not be in the backup");
    
    CFReleaseNull(fullKeyBytes);
    CFReleaseNull(pubKeyBytes);
    CFReleaseNull(bskb);

    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(cfpassword);
    CFReleaseNull(wrappingKey);
    
    SOSTestCleanup();
}

int secd_66_account_recovery(int argc, char *const *argv) {
    plan_tests(358);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests(true);
    tests(false);
    
    return 0;
}

#endif
