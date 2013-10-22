//
//  sc-57-account-leave.c
//  sec
//
//  Created by Mitch Adler on 6/18/13.
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


static int kTestTestCount = 158;

/*
static void trim_retirements_from_circle(SOSAccountRef account) {
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSCircleRemoveRetired(circle, NULL);
    });
}
 */


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
    is(SOSAccountGetLastDepartureReason(bob_account, &error), kSOSNeverLeftCircle, "Bob affirms he hasn't left.");

    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);

    SOSAccountPurgePrivateCredential(alice_account);

    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);

    FeedChangesTo(changes, bob_account); // Bob sees alice bail.

    FeedChangesTo(changes, alice_account); // Alice sees the fallout.

    accounts_agree("Alice bails", bob_account, alice_account);

    {
        CFArrayRef concurring = SOSAccountCopyConcurringPeers(alice_account, &error);
        
        ok(concurring && CFArrayGetCount(concurring) == 2, "See two concurring %@ (%@)", concurring, error);
        CFReleaseNull(error);
        CFReleaseNull(concurring);
    }
    
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesTo(changes, bob_account); // Bob sees alice request.
    
    is(countActivePeers(bob_account), 3, "Bob sees 2 active peers");
    is(countPeers(bob_account), 1, "Bob sees 1 valid peer");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(bob_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See alice's reapp. %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    

    is(countActivePeers(bob_account), 3, "Bob sees 3 active peers");
    is(countPeers(bob_account), 2, "Bob sees 2 valid peers");
    
    is(countActivePeers(alice_account), 3, "Alice sees 2 active peers");
    is(countPeers(alice_account), 1, "Alice sees 1 valid peers");

    FeedChangesToMulti(changes, alice_account, carole_account, NULL); // Alice sees bob accepts.
    
    FeedChangesToMulti(changes, bob_account, carole_account, NULL); // Bob sees Alice concurr.
    
    accounts_agree("Alice rejoined", bob_account, alice_account);
    accounts_agree_internal("Alice rejoined, carole noticed", bob_account, carole_account, false);
    
    ok(SOSAccountJoinCircles(carole_account, &error), "Carole applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesToMulti(changes, bob_account, alice_account, NULL); // Bob and carole see the final result.

    accounts_agree_internal("Carole applied", bob_account, alice_account, false);
    accounts_agree_internal("Carole applied - 2", bob_account, carole_account, false);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(bob_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See Carole's eapp. %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts carole (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    FeedChangesToMulti(changes, alice_account, NULL); // Alice sees the change and countersigns
    FeedChangesToMulti(changes, carole_account, NULL); // Carole countersigns her acceptance.
    FeedChangesToMulti(changes, bob_account, alice_account, david_account, NULL); // Bob and Alice see carole's counter signature.
    // david ends up with a 3 peerinfo member circle.

    accounts_agree_internal("Carole joined", bob_account, alice_account, false);
    accounts_agree_internal("Carole joined - 2", bob_account, carole_account, false);
    
    // Now test lost circle change when two leave simultaneously, needing us to see the retirement tickets
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountLeaveCircles(carole_account, &error), "carole Leaves (%@)", error);
    CFReleaseNull(error);

    FeedChangesToMulti(changes, bob_account, NULL); // Bob sees both retirements and a circle missing one
    
    is(countPeers(bob_account), 1, "Bob sees 1 valid peer");
    is(countActivePeers(bob_account), 4, "Bob sees 4 active peers");
 
    FeedChangesToMulti(changes, carole_account, alice_account, david_account, NULL);

    is(SOSAccountGetLastDepartureReason(carole_account, &error), kSOSWithdrewMembership, "Carole affirms she left on her own.");

    ok(SOSAccountAssertUserCredentials(david_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    is(countPeers(david_account), 1, "david sees 1 peers");
    is(countActivePeers(david_account), 4, "david sees 4 active peers");
    
    ok(SOSAccountJoinCircles(david_account, &error), "David applies (%@)", error);
    CFReleaseNull(error);
    is(countPeers(david_account), 1, "david sees 1 peers");
    is(countActivePeers(david_account), 4, "david sees 4 active peers");

    FeedChangesToMulti(changes, carole_account, alice_account, bob_account, NULL);

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(bob_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See David's app. %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts carole (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
   
    FeedChangesToMulti(changes, carole_account, alice_account, david_account, NULL);

    is(countPeers(bob_account), 2, "Bob sees 2 valid peer");
    is(countActivePeers(bob_account), 3, "Bob sees 3 active peers");

    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
}

int secd_57_account_leave(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
