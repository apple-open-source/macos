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


static int kTestTestCount = 202;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfwrong_password = CFDataCreate(NULL, (uint8_t *) "NotFooFooFoo", 10);
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
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
    CFReleaseNull(error);
    ok(!SOSAccountTryUserCredentials(alice_account, cfaccount, cfwrong_password, &error), "Credential failing (%@)", error);
    CFReleaseNull(cfwrong_password);
    is(error ? CFErrorGetCode(error) : 0, kSOSErrorWrongPassword, "Expected SOSErrorWrongPassword");
    CFReleaseNull(error);

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

    // As of PR-13917727/PR-13906870 this no longer works (by "design"), in favor of making another more common
    // failure (apply/OSX-psudo-reject/re-apply) work.  Write races might be "fixed better" with affirmitave rejection.
#if 0

    //
    // Write race emulation.
    //
    //

    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    FeedChangesTo(changes, bob_account); // Bob sees Alice leaving and rejoining
    FeedChangesTo(changes, alice_account); // Alice sees bob concurring

    accounts_agree("Alice leaves & returns", bob_account, alice_account);

    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account); // Bob sees Alice Applying

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }

    CFMutableDictionaryRef bobAcceptanceChanges = ExtractPendingChanges(changes);

    // Alice re-applies without seeing that she was accepted.
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves again  (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);

    CFReleaseSafe(ExtractPendingChanges(changes)); // Alice loses the race to write her changes - bob never sees em.

    FeedChangesTo(&bobAcceptanceChanges, alice_account); // Alice sees bob inviting her in the circle.

    FeedChangesTo(changes, bob_account); // Bob sees Alice Concurring

    // As of PR-13917727/PR-13906870
    accounts_agree("Alice leave, applies back, loses a race and eventually gets in", bob_account, alice_account);
#endif

    // Both in circle.

    // We want Alice to leave circle while an Applicant on a full concordance signed circle with old-Alice as an Alum and Bob a peer.
    // ZZZ
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice leaves once more  (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account); // Bob sees Alice become an Alum.
    FeedChangesTo(changes, alice_account); // Alice sees bob concurring
    accounts_agree("Alice and Bob see Alice out of circle", bob_account, alice_account);

    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice leaves while applying  (%@)", error);
    FeedChangesTo(changes, bob_account); // Bob sees Alice become an Alum.

    CFReleaseNull(error);

    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCNotInCircle, "Alice isn't applying any more");
    accounts_agree("Alice leaves & some fancy concordance stuff happens", bob_account, alice_account);

    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account); // Bob sees Alice reapply.

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }

    FeedChangesTo(changes, alice_account); // Alice sees bob accepting her
    FeedChangesTo(changes, bob_account); // Bob sees Alice concur

    accounts_agree("Alice comes back", bob_account, alice_account);

    // Emulation of <rdar://problem/13889901>
    FeedChangesTo(changes, carol_account);

    ok(SOSAccountAssertUserCredentials(carol_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(cfpassword);
    ok(SOSAccountJoinCircles(carol_account, &error), "Carol Applies (%@)", error);
    CFReleaseNull(error);
    CFReleaseSafe(ExtractPendingChanges(changes)); // Alice and Bob loses the race to see Carol's changes.

    ok(SOSAccountResetToOffering(carol_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    FeedChangesTo(changes, bob_account);
    accounts_agree("13889901", carol_account, bob_account);
    is(SOSAccountGetLastDepartureReason(bob_account, &error), kSOSMembershipRevoked, "Bob affirms he hasn't left.");

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob ReApplies (%@)", error);
    FeedChangesTo(changes, carol_account);
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(carol_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(carol_account, applicants, &error), "Carol accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    FeedChangesTo(changes, bob_account);
    FeedChangesTo(changes, carol_account);
    accounts_agree("rdar://problem/13889901-II", bob_account, carol_account);

    CFReleaseNull(alice_new_gestalt);

    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carol_account);
}

int secd_55_account_circle(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
