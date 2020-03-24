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

#include "keychain/SecureObjectSync/SOSAccount.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include <Security/SecureObjectSync/SOSViews.h>
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "keychain/SecureObjectSync/Regressions/SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <Security/SecRecoveryKey.h>

#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include "keychain/securityd/SOSCloudCircleServer.h"
#include "SecdTestKeychainUtilities.h"

#if TARGET_OS_SIMULATOR

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


static inline bool SOSAccountSetRecoveryKey_wTxn(SOSAccount* acct, CFDataRef recoveryPub, CFErrorRef* error) {
    __block bool result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountSetRecoveryKey(txn.account, recoveryPub, error);
    }];
    return result;
}

static inline bool SOSAccountSOSAccountRemoveRecoveryKey_wTxn(SOSAccount* acct, CFErrorRef* error) {
    __block bool result = false;
    [acct performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        result = SOSAccountRemoveRecoveryKey(txn.account, error);
    }];
    return result;
}

// 6 test cases
static void registerRecoveryKeyNow(CFMutableDictionaryRef changes, SOSAccount* registrar, SOSAccount* observer, CFDataRef recoveryPub, bool recKeyFirst) {
    CFErrorRef error = NULL;

    is(ProcessChangesUntilNoChange(changes, registrar, observer, NULL), 1, "updates");

    if(recoveryPub) {
        ok(SOSAccountSetRecoveryKey_wTxn(registrar, recoveryPub, &error), "Set Recovery Key");
        CFReleaseNull(error);
    } else {
        ok(SOSAccountSOSAccountRemoveRecoveryKey_wTxn(registrar, &error), "Clear Recovery Key");
        CFReleaseNull(error);
    }
    ok(error == NULL, "Error shouldn't be %@", error);
    CFReleaseNull(error);
    ProcessChangesUntilNoChange(changes, registrar, observer, NULL);
    
    CFDataRef registrar_recKey = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, registrar, &error);
    CFReleaseNull(error);
    CFDataRef observer_recKey = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, observer, &error);
    CFReleaseNull(error);

    if(recoveryPub) {
        ok(registrar_recKey, "Registrar retrieved recKey");
        ok(observer_recKey, "Observer retrieved recKey");
        ok(CFEqualSafe(registrar_recKey, observer_recKey), "recKeys are the same");
        ok(CFEqualSafe(registrar_recKey, recoveryPub), "recKeys are as expected");
    } else {
        ok((!registrar_recKey && !observer_recKey), "recKeys are NULL");
    }
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
    
    sRecKey = SecRKCreateRecoveryKeyWithError((__bridge NSString*)sock_drawer_key, NULL);
    ok(sRecKey, "Create SecRecoveryKey from String");
    if(sRecKey) {
        fullKeyBytes = (__bridge CFDataRef)(SecRKCopyBackupFullKey(sRecKey));
        pubKeyBytes = (__bridge CFDataRef)(SecRKCopyBackupPublicKey(sRecKey));
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
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountAssertDSID(alice_account, cfdsid);
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
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
    
    
    is([alice_account.trust updateView:alice_account name:kTestView1 code:kSOSCCViewEnable err:&error], kSOSCCViewMember, "Enable view (%@)", error);
    CFReleaseNull(error);
    
    ok([alice_account.trust checkForRings:&error], "Alice_account is good");
    CFReleaseNull(error);

    ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);
    
    is([bob_account.trust updateView:bob_account name:kTestView1 code:kSOSCCViewEnable err:&error], kSOSCCViewMember, "Enable view (%@)", error);
    CFReleaseNull(error);
    
    ok([bob_account.trust checkForRings:&error], "Bob_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountSetBackupPublicKey_wTxn(alice_account, alice_backup_key, &error), "Set backup public key, alice (%@)", error);
    CFReleaseNull(error);
    
    ok([alice_account.trust checkForRings:&error], "Alice_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountSetBackupPublicKey_wTxn(bob_account, bob_backup_key, &error), "Set backup public key, bob (%@)", error);
    CFReleaseNull(error);
    
    ok([bob_account.trust checkForRings:&error], "Bob_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(alice_account, kTestView1), "Is alice is in backup before sync?");
    
    if(!recKeyFirst) {
        SOSBackupSliceKeyBagRef bskb = SOSAccountBackupSliceKeyBagForView_wTxn(alice_account, kTestView1, &error);
        CFReleaseNull(error);
        ok(!SOSBSKBHasRecoveryKey(bskb), "BSKB should not have recovery key");
        CFReleaseNull(bskb);
    }

    ok([alice_account.trust checkForRings:&error], "Alice_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "Is bob in the backup after sync? - 1");
    
    ok([bob_account.trust checkForRings:&error], "Alice_account is good");
    CFReleaseNull(error);
    
    int passes = ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);
    ok(passes < 6, "updates"); // this was varying between 4 and 5 on different BATS runs - we just don't want it going crazy
    
    ok([alice_account.trust checkForRings:&error], "Alice_account is good");
    CFReleaseNull(error);
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(alice_account, kTestView1), "Is alice is in backup after sync?");
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "IS bob in the backup after sync");
    
    //
    //Bob leaves the circle
    //
    ok([bob_account.trust leaveCircle:bob_account err:&error], "Bob Leaves (%@)", error);
    CFReleaseNull(error);
    
    //Alice should kick Bob out of the backup!
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(alice_account, kTestView1), "Bob left the circle, Alice is in the backup");

    //ok(testAccountPersistence(alice_account), "Test Account->DER->Account Equivalence");
    SOSAccountTrustClassic* bobTrust = bob_account.trust;
    ok(!SOSAccountIsPeerInBackupAndCurrentInView_wTxn(alice_account, bobTrust.peerInfo, kTestView1), "Bob is still in the backup!");
    
    //Bob gets back into the circle
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Re-Joins");
    
    //enables view
    is([bob_account.trust updateView:bob_account name:kTestView1 code:kSOSCCViewEnable err:&error], kSOSCCViewMember, "Enable view (%@)", error);
    CFReleaseNull(error);
    
    ok(!SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "Bob isn't in the backup yet");

    ok(SOSAccountSetBackupPublicKey_wTxn(bob_account, bob_backup_key, &error), "Set backup public key, alice (%@)", error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");
    
    //
    //removing backup key for bob account
    //
    
    ok(SOSAccountRemoveBackupPublickey_wTxn(bob_account, &error), "Removing Bob's backup key (%@)", error);
    int nchanges = (recKeyFirst) ? 2: 2;
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), nchanges, "updates");

    ok(!SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "Bob's backup key is in the backup - should not be so!");
    ok(!SOSAccountIsPeerInBackupAndCurrentInView_wTxn(alice_account, bobTrust.peerInfo, kTestView1), "Bob is up to date in the backup!");
    
    //
    // Setting new backup public key for Bob
    //
    
    ok(SOSAccountSetBackupPublicKey_wTxn(bob_account, bob_backup_key, &error), "Set backup public key, alice (%@)", error);
    CFReleaseNull(error);
    
    is([bob_account.trust updateView:bob_account name:kTestView1 code:kSOSCCViewEnable err:&error], kSOSCCViewMember, "Enable view (%@)", error);
    ok(SOSAccountNewBKSBForView(bob_account, kTestView1, &error), "Setting new backup public key for bob account failed: (%@)", error);
    
    //bob is in his own backup
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "Bob's backup key is not in the backup");
    //alice does not have bob in her backup
    ok(!SOSAccountIsPeerInBackupAndCurrentInView_wTxn(alice_account, bobTrust.peerInfo, kTestView1), "Bob is up to date in the backup - should not be so!");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 5, "updates");
    
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "Bob's backup key should be in the backup");
    ok(SOSAccountIsMyPeerInBackupAndCurrentInView_wTxn(alice_account, kTestView1), "Alice is in the backup");
    
    if(!recKeyFirst) registerRecoveryKeyNow(changes, alice_account, bob_account, pubKeyBytes, recKeyFirst);
    
    ok(SOSAccountRecoveryKeyIsInBackupAndCurrentInView_wTxn(alice_account, kTestView1), "Recovery Key is also in the backup");
    ok(SOSAccountRecoveryKeyIsInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "Recovery Key is also in the backup");
    
    SOSBackupSliceKeyBagRef bskb = SOSAccountBackupSliceKeyBagForView_wTxn(alice_account, kTestView1, &error);
    CFReleaseNull(error);
    
    ok(SOSBSKBHasRecoveryKey(bskb), "BSKB should have recovery key");

    CFDataRef wrappingKey = CFStringCreateExternalRepresentation(kCFAllocatorDefault, sock_drawer_key, kCFStringEncodingUTF8, 0);
    ok(wrappingKey, "Made wrapping key from with sock drawer key");
    bskb_keybag_handle_t bskbHandle = SOSBSKBLoadAndUnlockWithWrappingSecret(bskb, wrappingKey, &error);
    ok(bskbHandle, "Made bskbHandle with recover key");
    ok(SOSAccountHasPublicKey(alice_account, &error), "Has Public Key" );
    
    // Testing reset (Null) recoveryKey =========
    
    CFReleaseNull(bskb);
    CFReleaseNull(wrappingKey);

    registerRecoveryKeyNow(changes, alice_account, bob_account, NULL, recKeyFirst);
    
    ok(!SOSAccountRecoveryKeyIsInBackupAndCurrentInView_wTxn(alice_account, kTestView1), "Recovery Key is not in the backup");
    ok(!SOSAccountRecoveryKeyIsInBackupAndCurrentInView_wTxn(bob_account, kTestView1), "Recovery Key is not in the backup");

    bskb = SOSAccountBackupSliceKeyBagForView_wTxn(alice_account, kTestView1, &error);
    CFReleaseNull(error);
    
    ok(!SOSBSKBHasRecoveryKey(bskb), "BSKB should not have recovery key");

    //=========
    

    ok([alice_account.trust resetAccountToEmpty:alice_account transport:alice_account.circle_transport err:&error], "Reset circle to empty");
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    ok(SOSAccountIsBackupRingEmpty(bob_account, kTestView1), "Bob should not be in the backup");
    ok(SOSAccountIsBackupRingEmpty(alice_account, kTestView1), "Alice should not be in the backup");
    
    CFReleaseNull(fullKeyBytes);
    CFReleaseNull(pubKeyBytes);
    CFReleaseNull(bskb);

    CFReleaseNull(cfpassword);
    CFReleaseNull(wrappingKey);
    bob_account = nil;
    alice_account = nil;

    SOSTestCleanup();
}

int secd_66_account_recovery(int argc, char *const *argv) {
    plan_tests(281);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests(true);
    tests(false);
    
    return 0;
}

#endif
