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

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 81;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfwrong_password = CFDataCreate(NULL, (uint8_t *) "NotFooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carol_account = CreateAccountForLocalChanges(CFSTR("Carol"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
    CFReleaseNull(error);
    ok(!SOSAccountTryUserCredentials(alice_account, cfaccount, cfwrong_password, &error), "Credential failing (%@)", error);
    CFReleaseNull(cfwrong_password);
    is(error ? CFErrorGetCode(error) : 0, kSOSErrorWrongPassword, "Expected SOSErrorWrongPassword");
    CFReleaseNull(error);
    
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    
    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");
    
    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    //bob now goes def while Alice does some stuff.
    
    ok(SOSAccountLeaveCircle(alice_account, &error), "ALICE LEAVES THE CIRCLE (%@)", error);
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Alice resets to offering again (%@)", error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    accounts_agree("bob&alice pair", bob_account, alice_account);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "updates");

    
    ok(SOSAccountAssertUserCredentialsAndUpdate(carol_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    SOSAccountSetUserPublicTrustedForTesting(carol_account);
    ok(SOSAccountResetToOffering_wTxn(carol_account, &error), "Carol is going to push a reset to offering (%@)", error);

    int64_t valuePtr = 0;
    CFNumberRef gencount = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &valuePtr);
    SOSCircleSetGeneration(carol_account->trusted_circle, gencount);
    
    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, carol_account->user_key_parameters, &error);
    CFNumberRef genCountTest = SOSCircleGetGeneration(carol_account->trusted_circle);
    CFIndex testPtr;
    CFNumberGetValue(genCountTest, kCFNumberCFIndexType, &testPtr);
    ok(testPtr== 0);

    SOSCircleSignOldStyleResetToOfferingCircle(carol_account->trusted_circle, carol_account->my_identity, user_privkey, &error);
    SOSTransportCircleTestRemovePendingChange(carol_account->circle_transport, SOSCircleGetName(carol_account->trusted_circle), NULL);
    CFDataRef circle_data = SOSCircleCopyEncodedData(carol_account->trusted_circle, kCFAllocatorDefault, &error);
    if (circle_data) {
         SOSTransportCirclePostCircle(carol_account->circle_transport, SOSCircleGetName(carol_account->trusted_circle), circle_data, &error);
    }

    genCountTest = SOSCircleGetGeneration(carol_account->trusted_circle);
    CFNumberGetValue(genCountTest, kCFNumberCFIndexType, &testPtr);
    ok(testPtr== 0);

    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    SOSAccountSetUserPublicTrustedForTesting(alice_account);
    SOSAccountSetUserPublicTrustedForTesting(bob_account);

    is(ProcessChangesUntilNoChange(changes, carol_account, alice_account, bob_account, NULL), 2, "updates");

    ok(kSOSCCNotInCircle == SOSAccountGetCircleStatus(alice_account, &error), "alice is not in the account (%@)", error);
    ok(kSOSCCNotInCircle == SOSAccountGetCircleStatus(bob_account, &error), "bob is not in the account (%@)", error);
    ok(kSOSCCInCircle == SOSAccountGetCircleStatus(carol_account, &error), "carol is in the account (%@)", error);
    
    CFReleaseNull(gencount);
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(cfpassword);
    SOSTestCleanup();
}

int secd_52_offering_gencount_reset(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
