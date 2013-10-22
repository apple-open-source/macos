//
//  secd-55-account-circle.c
//  sec
//
//  Created by Mitch Adler on 1/25/12.
//
//



#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <CoreFoundation/CFDictionary.h>

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
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"


static int kTestTestCount = 9;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfwrong_password = CFDataCreate(NULL, (uint8_t *) "NotFooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef data_name = CFSTR("TestSource");
    CFStringRef circle_key_name = SOSCircleKeyCreateWithName(data_name, NULL);

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountRef alice_account = CreateAccountForLocalChanges(changes, CFSTR("Alice"), data_name);
    SOSAccountRef bob_account = CreateAccountForLocalChanges(changes, CFSTR("Bob"), data_name);
    SOSAccountRef carol_account = CreateAccountForLocalChanges(changes, CFSTR("Carol"), data_name);

    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    // Bob wins writing at this point, feed the changes back to alice.

    FeedChangesToMulti(changes, alice_account, carol_account, NULL);

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    ok(!SOSAccountTryUserCredentials(alice_account, cfaccount, cfwrong_password, &error), "Credential failing (%@)", error);
    CFReleaseNull(cfwrong_password);
    is(error ? CFErrorGetCode(error) : 0, kSOSErrorWrongPassword, "Expected SOSErrorWrongPassword");
    CFReleaseNull(error);
    
    CFDataRef incompatibleDER = SOSCircleCreateIncompatibleCircleDER(&error);
    
    CFDictionarySetValue(changes, circle_key_name, incompatibleDER);
    
    FeedChangesTo(changes, alice_account);
    CFReleaseNull(incompatibleDER);
    CFReleaseNull(circle_key_name);
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCError, "Is in circle");
    CFReleaseNull(error);

#if 0
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account);

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, alice_account);

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }


    FeedChangesTo(changes, bob_account); // Bob sees he's accepted

    FeedChangesTo(changes, alice_account); // Alice sees bob-concurring

    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);

    accounts_agree("bob&alice pair", bob_account, alice_account);

    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);

    CFDictionaryRef alice_new_gestalt = SOSCreatePeerGestaltFromName(CFSTR("Alice, but different"));

    ok(SOSAccountUpdateGestalt(alice_account, alice_new_gestalt), "Update gestalt %@ (%@)", alice_account, error);
    CFReleaseNull(alice_new_gestalt);

    FeedChangesTo(changes, bob_account); // Bob sees alice change her name.

    FeedChangesTo(changes, alice_account); // Alice sees the fallout.

    accounts_agree("Alice's name changed", bob_account, alice_account);

    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account); // Bob sees alice bail.

    FeedChangesTo(changes, alice_account); // Alice sees the fallout.

    accounts_agree("Alice bails", bob_account, alice_account);

    peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 1, "See one peer %@ (%@)", peers, error);
    CFReleaseNull(peers);

    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account);


    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }

    FeedChangesTo(changes, alice_account); // Alice sees bob accepting

    FeedChangesTo(changes, bob_account); // Bob sees Alice concurring

    accounts_agree("Alice accepts' Bob", bob_account, alice_account);

    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account); // Bob sees Alice leaving and rejoining
    FeedChangesTo(changes, alice_account); // Alice sees bob concurring

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }

    FeedChangesTo(changes, alice_account); // Alice sees bob accepting

    FeedChangesTo(changes, bob_account); // Bob sees Alice concurring

    accounts_agree("Bob accepts Alice", bob_account, alice_account);


    CFReleaseNull(alice_new_gestalt);
#endif

    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carol_account);
}

int secd_55_account_incompatibility(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
