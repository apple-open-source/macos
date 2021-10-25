/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include "keychain/securityd/SOSCloudCircleServer.h"

#include "SOSAccountTesting.h"
#include "SecdTestKeychainUtilities.h"

#if SOS_ENABLED

static bool AssertCreds(SOSAccount* account,CFStringRef acct_name, CFDataRef password) {
    CFErrorRef error = NULL;
    bool retval;
    ok((retval = SOSAccountAssertUserCredentialsAndUpdate(account, acct_name, password, &error)), "Credential setting (%@)", error);
    CFReleaseNull(error);
    return retval;
}

static bool ResetToOffering(SOSAccount* account) {
    CFErrorRef error = NULL;
    bool retval;
    ok((retval = SOSAccountResetToOffering_wTxn(account, &error)), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    return retval;
}

static bool JoinCircle(SOSAccount* account) {
    CFErrorRef error = NULL;
    bool retval;
    ok((retval = SOSAccountJoinCircles_wTxn(account, &error)), "Join Circle (%@)", error);
    CFReleaseNull(error);
    return retval;
}

static bool AcceptApplicants(SOSAccount* account, CFIndex cnt) {
    CFErrorRef error = NULL;
    bool retval = false;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
    
    ok((retval = (applicants && CFArrayGetCount(applicants) == cnt)), "See applicants %@ (%@)", applicants, error);
    if(retval) ok((retval = SOSAccountAcceptApplicants(account, applicants, &error)), "Accept Applicants (%@)", error);
    CFReleaseNull(applicants);
    CFReleaseNull(error);
    return retval;
}


static bool joinWithApprover(SOSAccount* me, SOSAccount *approver, void (^action)(void)) {
    if(JoinCircle(me)) {
        action();
        return AcceptApplicants(approver, 1);
    }
    return false;
}

static bool test3peers(CFDataRef startPassword, bool joinAll, bool (^action)(CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount* alice_account, SOSAccount* bob_account, SOSAccount* carol_account) ) {
    bool retval = false;
    CFStringRef cfaccount = CFSTR("test@test.org");

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccount* carol_account = CreateAccountForLocalChanges(CFSTR("Carol"), CFSTR("TestSource"));
        
    /* Set Initial Credentials and Parameters for the Syncing Circles ---------------------------------------*/
    ok(AssertCreds(bob_account, cfaccount, startPassword), "Setting credentials for Bob");
    // Bob wins writing at this point, feed the changes back to alice.

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "updates");

    ok(AssertCreds(alice_account, cfaccount, startPassword), "Setting credentials for Alice");
    ok(AssertCreds(carol_account, cfaccount, startPassword), "Setting credentials for Carol");

    ok(ResetToOffering(alice_account), "Reset to offering - Alice as first peer");
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");
    
    if(joinAll) {
        ok(joinWithApprover(bob_account, alice_account, ^{
            is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");
        }), "Bob Joins");
        ok(joinWithApprover(carol_account, alice_account, ^{
            is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 5, "updates");
        }), "Bob Joins");
    }

    retval = action(cfaccount, changes, alice_account, bob_account, carol_account);
    
    CFReleaseNull(changes);
    
    return retval;
}

static bool accountSeesTheseCounts(SOSAccount* account, int peers, int activePeers, int activeValidPeers) {
    int cPeers = countPeers(account);  // how many peers representing devices (no hidden)
    int cActivePeers = countActivePeers(account); // how many peers including hidden
    int cActiveValidPeers = countActiveValidPeers(account);  // how many peers that validate - userKey and deviceKey
    
    is(cPeers, peers, "peer count doesn't match");
    is(cActivePeers, activePeers, "active peer count doesn't match");
    is(cActiveValidPeers, activeValidPeers, "active-valid peer count doesn't match");
    return (peers == cPeers) && (activePeers == cActivePeers) && (activeValidPeers == cActiveValidPeers);
}


static void test1(void) {
    // this is a standard "changing the password" for multiple peers already in the circle
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    test3peers(cfpassword, true, ^bool(CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount *alice_account, SOSAccount *bob_account, SOSAccount *carol_account) {
        /* Change Password ------------------------------------------------------------------------------------------*/
        CFDataRef cfnewpassword = CFDataCreate(NULL, (uint8_t *) "ffoffoffo", 10);
        
        /* Bob */
        ok(AssertCreds(bob_account , cfaccount, cfnewpassword), "Credential resetting for Bob");
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");
        ok(accountSeesTheseCounts(bob_account, 2, 3, 2), "peer counts match");

        /* Alice */
        ok(AssertCreds(alice_account , cfaccount, cfnewpassword), "Credential resetting for Alice");
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 3, "updates");
        ok(accountSeesTheseCounts(alice_account, 3, 4, 3), "peer counts match");
        
        /* Carol */
        ok(AssertCreds(carol_account , cfaccount, cfnewpassword), "Credential resetting for Carol");
        is(ProcessChangesUntilNoChange(changes, carol_account, alice_account, bob_account, NULL), 1, "updates");
        is(ProcessChangesUntilNoChange(changes, carol_account, alice_account, bob_account, NULL), 2, "updates");
        
        ok(accountSeesTheseCounts(carol_account, 3, 4, 4), "peer counts match");
        ok(accountSeesTheseCounts(alice_account, 3, 4, 4), "peer counts match");
        ok(accountSeesTheseCounts(bob_account, 3, 4, 4), "peer counts match");
        
        CFReleaseNull(cfnewpassword);
        return true;
    });
}

static void test2(void) {
    // Change password, but Carol does a "try" before the creds are really reset - then Carol should automatically fix creds once the parms are present.
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    
    test3peers(cfpassword, true, ^bool(CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount *alice_account, SOSAccount *bob_account, SOSAccount *carol_account) {
        /* Change Password ------------------------------------------------------------------------------------------*/
        CFDataRef cfnewpassword = CFDataCreate(NULL, (uint8_t *) "ooFooFooF", 10);
        
        // This should fail, but prime Carol's account to validate once the keyparms are present.
        ok(!SOSAccountTryUserCredentials(carol_account, cfaccount, cfnewpassword, NULL), "Carol tries password before it's set");
        
        ok(AssertCreds(alice_account , cfaccount, cfnewpassword), "Credential resetting for Alice");
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 3, "updates");
        
        ok([alice_account isInCircle: NULL], "Alice should be valid in circle");
        ok(![bob_account isInCircle: NULL], "Bob should not be valid in circle");
        ok([carol_account isInCircle: NULL], "Carol should be valid in circle");


        ok(AssertCreds(bob_account , cfaccount, cfnewpassword), "Credential resetting for Bob");
        ok(accountSeesTheseCounts(bob_account, 2, 3, 2), "peer counts match");
        
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 4, "updates");
        
        ok([alice_account isInCircle: NULL], "Alice should be valid in circle");
        ok([bob_account isInCircle: NULL], "Bob should be valid in circle");
        ok([carol_account isInCircle: NULL], "Carol should be valid in circle");
        
        ok(accountSeesTheseCounts(alice_account, 3, 4, 4), "peer counts match");
        ok(accountSeesTheseCounts(bob_account, 3, 4, 4), "peer counts match");
        ok(accountSeesTheseCounts(carol_account, 3, 4, 4), "peer counts match");
        CFReleaseNull(cfnewpassword);
        return true;
    });
}

static void test3(void) {
    /* Change Password 3 - cause a parm lost update collision ----------------------------------------------------*/
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    test3peers(cfpassword, true, ^bool(CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount *alice_account, SOSAccount *bob_account, SOSAccount *carol_account) {
        CFDataRef cfnewpassword = CFDataCreate(NULL, (uint8_t *) "cococococ", 10);
        
        ok(AssertCreds(bob_account , cfaccount, cfnewpassword), "Credential resetting for Bob");
        // without KVS going yet alice will write over Bob's parms
        ok(AssertCreds(alice_account , cfaccount, cfnewpassword), "Credential resetting for Alice");
        
        // but during the KVS goes round and round thing Bob will use the cached password.
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 4, "updates");
        
        ok(accountSeesTheseCounts(alice_account, 3, 4, 3), "peer counts match");
        ok(accountSeesTheseCounts(bob_account, 3, 4, 3), "peer counts match");
        ok(accountSeesTheseCounts(carol_account, 0, 0, 0), "peer counts match");
        CFReleaseNull(cfnewpassword);
        return true;
    });
}

static void test4(void) {
    /* Change Password 4 - new peer changes the password and joins ----------------------------------------------------*/
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    test3peers(cfpassword, true, ^bool(CFStringRef cfaccount, CFMutableDictionaryRef changes, SOSAccount *alice_account, SOSAccount *bob_account, SOSAccount *carol_account) {
        CFDataRef cfnewpassword = CFDataCreate(NULL, (uint8_t *) "dodododod", 10);

        // New peer changes creds - there are no valid peers to let it in.  This resets the circle.
        SOSAccount* david_account = CreateAccountForLocalChanges(CFSTR("David"), CFSTR("TestSource"));
        ok(AssertCreds(david_account , cfaccount, cfnewpassword), "Credential resetting for David");
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, david_account, NULL), 2, "updates");
        ok(JoinCircle(david_account), "David Applies");
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, david_account, NULL), 2, "updates");
        
        // only david_account should be in circle
        ok(![alice_account isInCircle: NULL], "Alice should not be valid in circle");
        ok(![bob_account isInCircle: NULL], "Bob should not be valid in circle");
        ok(![carol_account isInCircle: NULL], "Carol should not be valid in circle");
        ok([david_account isInCircle: NULL], "David should be valid in circle");

        ok(accountSeesTheseCounts(alice_account, 0, 0, 0), "peer counts match");
        ok(accountSeesTheseCounts(bob_account, 0, 0, 0), "peer counts match");
        ok(accountSeesTheseCounts(carol_account, 0, 0, 0), "peer counts match");
        ok(accountSeesTheseCounts(david_account, 1, 2, 2), "peer counts match");

        CFReleaseNull(cfnewpassword);
        return true;
    });
}
#endif

int secd_58_password_change(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(297);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    test1();
    test2();
    test3();
    test4();
    SOSTestCleanup();
    secd_test_teardown_delete_temp_keychain(__FUNCTION__);
#else
    plan_tests(0);
#endif
    return 0;
}
