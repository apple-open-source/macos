//
//  secd-61-account-leave-not-in-kansas-anymore.c
//  sec
//
//  Created by Richard Murphy on 7/16/13.
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


static int kTestTestCount = 102;
#if 0
static int countPeers(SOSAccountRef account, bool active) {
    CFErrorRef error = NULL;
    CFArrayRef peers;

    if(active) peers = SOSAccountCopyActivePeers(account, &error);
    else peers = SOSAccountCopyPeers(account, &error);
    int retval = (int) CFArrayGetCount(peers);
    CFReleaseNull(error);
    CFReleaseNull(peers);
    return retval;
}
#endif
/*
 static void trim_retirements_from_circle(SOSAccountRef account) {
 SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
 SOSCircleRemoveRetired(circle, NULL);
 });
 }
 */
static bool accept_applicants(SOSAccountRef account, int count) {
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
    bool retval = false;
    ok(applicants, "Have Applicants");
    if(!applicants) goto errout;
    is(CFArrayGetCount(applicants), count, "See applicants %@ (%@)", applicants, error);
    if(CFArrayGetCount(applicants) != count) goto errout;
    ok(retval = SOSAccountAcceptApplicants(account, applicants, &error), "Account accepts (%@)", error);
errout:
    CFReleaseNull(error);
    CFReleaseNull(applicants);
    return retval;
}


static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountRef alice_account = CreateAccountForLocalChanges(changes, CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(changes, CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carole_account = CreateAccountForLocalChanges(changes, CFSTR("Carole"), CFSTR("TestSource"));
    SOSAccountRef david_account = CreateAccountForLocalChanges(changes, CFSTR("David"), CFSTR("TestSource"));

    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    // Bob wins writing at this point, feed the changes back to alice.

    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountAssertUserCredentials(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountAssertUserCredentials(david_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account);

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, alice_account);

    ok(accept_applicants(alice_account, 1), "Alice Accepts Application");

    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, david_account, NULL);

    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, david_account, NULL);

    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);

    accounts_agree("bob&alice pair", bob_account, alice_account);
    is(SOSAccountGetLastDepartureReason(bob_account, &error), kSOSNeverLeftCircle, "Bob affirms he hasn't left.");

    // ==============================  Alice and Bob are in the Account. ============================================

    ok(SOSAccountJoinCircles(carole_account, &error), "Carole Applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);

    ok(accept_applicants(alice_account, 1), "Alice Accepts Application");

    // Let everyone concur.
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);

    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 3, "See three peers %@ (%@)", peers, error);
    CFReleaseNull(peers);


    // SOSAccountPurgePrivateCredential(alice_account);

    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);

    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);

    ok(SOSAccountJoinCircles(david_account, &error), "David Applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);

    CFReleaseNull(error);
    ok(accept_applicants(carole_account, 1), "Carole Accepts Application");

    // ==============================  We added Carole and David while Bob was in a drawer. Alice has left ============================================

    // ==============================  Bob comes out of the drawer seeing alice left and doesn't recognize the remainder. ============================================

    FeedChangesToMulti(changes, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, bob_account, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, bob_account, NULL);
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, bob_account, NULL);
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, bob_account, NULL);
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, bob_account, NULL);
    FeedChangesToMulti(changes, alice_account, carole_account, david_account, bob_account, NULL);

    CFReleaseNull(error);
    is(SOSAccountIsInCircles(carole_account, &error), kSOSCCInCircle, "Carole still in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountIsInCircles(david_account, &error), kSOSCCInCircle, "David still in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountIsInCircles(bob_account, &error), kSOSCCNotInCircle, "Bob is not in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountGetLastDepartureReason(bob_account, &error), kSOSLeftUntrustedCircle, "Bob affirms he left because he doesn't know anyone.");
    CFReleaseNull(error);
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCNotInCircle, "Alice is not in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountGetLastDepartureReason(alice_account, &error), kSOSWithdrewMembership, "Alice affirms she left by request.");
    CFReleaseNull(error);


    CFReleaseNull(carole_account);
    CFReleaseNull(david_account);
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
}

int secd_61_account_leave_not_in_kansas_anymore(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
