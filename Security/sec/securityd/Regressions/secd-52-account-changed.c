/*
 *  secd_52_account_changed.c
 *
 *  Created by Richard Murphy on 09152013.
 *  Copyright 2013 Apple Inc. All rights reserved.
 *
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


static int kTestTestCount = 133;


static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges(changes, CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(changes, CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carol_account = CreateAccountForLocalChanges(changes, CFSTR("Carol"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    
    FeedChangesToMulti(changes, alice_account, carol_account, NULL);
    
    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountAssertUserCredentials(carol_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    /* ==================== Three Accounts setup =============================================*/
    
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesToMulti(changes, bob_account, carol_account, NULL);
    
    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesToMulti(changes, alice_account, carol_account, bob_account, NULL);
    
    ok(SOSAccountJoinCircles(carol_account, &error), "Carol Applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesToMulti(changes, alice_account, carol_account, bob_account, NULL);

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    FeedChangesToMulti(changes, bob_account, NULL); // let bob concurr
    FeedChangesToMulti(changes, alice_account, carol_account, NULL); // let carol concurr
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // all synced
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // all synced
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // all synced
    
    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);
    
    accounts_agree_internal("bob&alice pair", bob_account, alice_account, false);
    accounts_agree_internal("bob&carol pair", bob_account, carol_account, false);
    /* ==================== Three Accounts in circle =============================================*/
    InjectChangeToMulti(CFSTR("^AccountChanged"), CFSTR("none"), alice_account, bob_account, carol_account, NULL);
    
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCError, "Account reset - no user keys - error");
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCError, "Account reset - no user keys - error");
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCError, "Account reset - no user keys - error");

    CFDataRef cfpassword2 = CFDataCreate(NULL, (uint8_t *) "ooFooFooF", 10);
    CFStringRef cfaccount2 = CFSTR("test2@test.org");

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount2, cfpassword2, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);

    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCCircleAbsent, "Account reset - circle is absent");
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCError, "Account reset - no user keys - error");
    FeedChangesToMulti(changes, bob_account, carol_account, NULL);

    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount2, cfpassword2, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountAssertUserCredentials(carol_account, cfaccount2, cfpassword2, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);

    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCCircleAbsent, "Account reset - circle is absent");
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCCircleAbsent, "Account reset - circle is absent");
    // Now everyone is playing the same account.

    /* ==================== Three Accounts setup =============================================*/

    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    is(countActivePeers(alice_account), 2, "2 peers - alice and icloud");

    FeedChangesToMulti(changes, bob_account, carol_account, NULL);
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCInCircle, "Alice is in circle");
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCNotInCircle, "Bob is not in circle");
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCNotInCircle, "Carol is not in circle");

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesToMulti(changes, alice_account, carol_account, bob_account, NULL);
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCRequestPending, "Bob has a pending request");

    ok(SOSAccountJoinCircles(carol_account, &error), "Carol Applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesToMulti(changes, alice_account, carol_account, bob_account, NULL);
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCRequestPending, "Carol has a pending request");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
        is(countActivePeers(alice_account), 4, "4 peers - alice, bob, carol, and icloud");
    }

    FeedChangesToMulti(changes, bob_account, NULL); // let bob concurr
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCInCircle, "Bob is in circle");
    FeedChangesToMulti(changes, alice_account, carol_account, NULL); // let carol concurr
    is(SOSAccountIsInCircles(carol_account, &error), kSOSCCInCircle, "Carol is in circle");
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // all synced
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // all synced
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // all synced

    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);

    accounts_agree_internal("bob&alice pair", bob_account, alice_account, false);
    accounts_agree_internal("bob&carol pair", bob_account, carol_account, false);

}


int secd_52_account_changed(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
	
    tests();
    
	return 0;
}
