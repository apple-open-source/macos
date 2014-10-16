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

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SecdTestKeychainUtilities.h"
#include "SOSAccountTesting.h"

static int kTestTestCount = 156;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges( CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carol_account = CreateAccountForLocalChanges(CFSTR("Carol"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "update");
    
    
    CFDictionaryRef new_gestalt = SOSCreatePeerGestaltFromName(CFSTR("New Device"));
    ok (SOSAccountUpdateGestalt(bob_account, new_gestalt), "did we send a null circle?");
    CFReleaseNull(new_gestalt);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "nothing published");

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountAssertUserCredentials(carol_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    /* ==================== Three Accounts setup =============================================*/
    
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "update");

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "update");

    
    ok(SOSAccountJoinCircles(carol_account, &error), "Carol Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "update");

    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 4, "update");

    accounts_agree_internal("bob&alice pair", bob_account, alice_account, false);
    accounts_agree_internal("bob&carol pair", bob_account, carol_account, false);

    /* ==================== Three Accounts in circle =============================================*/
    InjectChangeToMulti(changes, CFSTR("^AccountChanged"), CFSTR("none"), alice_account, bob_account, carol_account, NULL);

    SOSAccountEnsureFactoryCirclesTest(alice_account, CFSTR("Alice"));
    SOSAccountEnsureFactoryCirclesTest(bob_account, CFSTR("Bob"));
    SOSAccountEnsureFactoryCirclesTest(carol_account, CFSTR("Carol"));
    
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCError, "Account reset - no user keys - error");
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCError, "Account reset - no user keys - error");
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCError, "Account reset - no user keys - error");
    
    CFDataRef cfpassword2 = CFDataCreate(NULL, (uint8_t *) "ooFooFooF", 10);
    CFStringRef cfaccount2 = CFSTR("test2@test.org");
    
    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount2, cfpassword2, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCCircleAbsent, "Account reset - circle is absent");
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCError, "Account reset - no user keys - error");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount2, cfpassword2, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountAssertUserCredentials(carol_account, cfaccount2, cfpassword2, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "updates");

    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCCircleAbsent, "Account reset - circle is absent");
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCCircleAbsent, "Account reset - circle is absent");
    // Now everyone is playing the same account.
    
    /* ==================== Three Accounts setup =============================================*/
    
    ok(SOSAccountResetToOffering(alice_account,   &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    is(countActivePeers(alice_account), 2, "2 peers - alice and icloud");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCInCircle, "Alice is in circle");
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCNotInCircle, "Bob is not in circle");
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCNotInCircle, "Carol is not in circle");
    
    ok(SOSAccountJoinCircles(bob_account,   &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    ok(SOSAccountJoinCircles(carol_account,   &error), "Carol Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCRequestPending, "Carol has a pending request");
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account,   applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
        is(countActivePeers(alice_account), 4, "4 peers - alice, bob, carol, and icloud");
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 4, "updates");

    accounts_agree_internal("bob&alice pair", bob_account, alice_account, false);
    accounts_agree_internal("bob&carol pair", bob_account, carol_account, false);
    
    CFReleaseSafe(cfpassword2);
    
    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
    
}


int secd_52_account_changed(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
