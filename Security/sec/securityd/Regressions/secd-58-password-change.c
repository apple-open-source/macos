//
//  secd-58-password-change.c
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


static int kTestTestCount = 300;

static bool AssertCreds(SOSAccountRef account, CFStringRef acct_name, CFDataRef password) {
    CFErrorRef error = NULL;
    bool retval;
    ok((retval = SOSAccountAssertUserCredentials(account, acct_name, password, &error)), "Credential setting (%@)", error);
    CFReleaseNull(error);
    return retval;
}

static bool ResetToOffering(SOSAccountRef account) {
    CFErrorRef error = NULL;
    bool retval;
    ok((retval = SOSAccountResetToOffering(account, &error)), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    return retval;
}

static bool JoinCircle(SOSAccountRef account) {
    CFErrorRef error = NULL;
    bool retval;
    ok((retval = SOSAccountJoinCircles(account, &error)), "Join Circle (%@)", error);
    CFReleaseNull(error);
    return retval;
}

static bool AcceptApplicants(SOSAccountRef account, CFIndex cnt) {
    CFErrorRef error = NULL;
    bool retval = false;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
        
    ok((retval = (applicants && CFArrayGetCount(applicants) == cnt)), "See applicants %@ (%@)", applicants, error);
    if(retval) ok((retval = SOSAccountAcceptApplicants(account, applicants, &error)), "Accept Applicants (%@)", error);
    CFReleaseNull(applicants);
    CFReleaseNull(error);
    return retval;
}


static void tests(void)
{
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges(changes, CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(changes, CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carol_account = CreateAccountForLocalChanges(changes, CFSTR("Carol"), CFSTR("TestSource"));
    
    /* Set Initial Credentials and Parameters for the Syncing Circles ---------------------------------------*/
    ok(AssertCreds(bob_account, cfaccount, cfpassword), "Setting credentials for Bob");
    // Bob wins writing at this point, feed the changes back to alice.
    FeedChangesToMulti(changes, alice_account, carol_account, NULL);
    ok(AssertCreds(alice_account, cfaccount, cfpassword), "Setting credentials for Alice");
    ok(AssertCreds(carol_account, cfaccount, cfpassword), "Setting credentials for Carol");
    CFReleaseNull(cfpassword);
    
    /* Make Alice First Peer -------------------------------------------------------------------------------*/
    ok(ResetToOffering(alice_account), "Reset to offering - Alice as first peer");
    FeedChangesToMulti(changes, bob_account, carol_account, NULL);

    /* Bob Joins -------------------------------------------------------------------------------------------*/
    ok(JoinCircle(bob_account), "Bob Applies");
    FeedChangesToMulti(changes, alice_account, carol_account, NULL);

    /* Alice Accepts -------------------------------------------------------------------------------------------*/
    ok(AcceptApplicants(alice_account, 1), "Alice Accepts Bob's Application");
    FeedChangesToMulti(changes, bob_account, carol_account, NULL); // Bob sees he's accepted
    FeedChangesToMulti(changes, alice_account, carol_account, NULL); // Alice sees bob-concurring
    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);
    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    /* Carol Applies -------------------------------------------------------------------------------------------*/
    ok(JoinCircle(carol_account), "Carol Applies");
    FeedChangesToMulti(changes, alice_account, bob_account, NULL);
    
    is(countPeers(alice_account), 2, "See two peers");
    
    
    /* Change Password ------------------------------------------------------------------------------------------*/
    CFDataRef cfnewpassword = CFDataCreate(NULL, (uint8_t *) "ooFooFooF", 10);
    
    ok(AssertCreds(bob_account, cfaccount, cfnewpassword), "Credential resetting for Bob");
    is(countPeers(bob_account), 2, "There are two valid peers - iCloud and Bob");
    is(countActivePeers(bob_account), 3, "There are three active peers - bob, alice, and iCloud");
    is(countActiveValidPeers(bob_account), 2, "There is two active valid peer - Bob and iCloud");
    FeedChangesToMulti(changes, alice_account, carol_account, NULL);

    ok(AssertCreds(alice_account, cfaccount, cfnewpassword), "Credential resetting for Alice");
    is(countPeers(alice_account), 2, "There are two peers - bob and alice");
    is(countActiveValidPeers(alice_account), 3, "There are three active valid peers - alice, bob, and icloud");
    FeedChangesToMulti(changes, bob_account, carol_account, NULL);
    FeedChangesToMulti(changes, alice_account, carol_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, NULL);
    accounts_agree("bob&alice pair", bob_account, alice_account);
    is(countPeers(alice_account), 2, "There are two peers - bob and alice");
    is(countActiveValidPeers(alice_account), 3, "There are three active valid peers - alice, bob, and icloud");
    
    ok(AssertCreds(carol_account, cfaccount, cfnewpassword), "Credential resetting for Carol");
    FeedChangesToMulti(changes, alice_account, bob_account, NULL);
    FeedChangesToMulti(changes, bob_account, carol_account, alice_account, NULL);
    FeedChangesToMulti(changes, bob_account, carol_account, alice_account, NULL);
    FeedChangesToMulti(changes, bob_account, carol_account, alice_account, NULL);
    accounts_agree("bob&alice pair", bob_account, alice_account);

    ok(AcceptApplicants(alice_account, 1), "Alice Accepts Carol's Application");
    FeedChangesToMulti(changes, bob_account, carol_account, NULL); // Carol sees she's accepted
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // Alice sees bob-concurring
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // Alice sees bob-concurring
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL); // Alice sees bob-concurring
    accounts_agree_internal("bob&alice pair", bob_account, alice_account, false);
    accounts_agree_internal("bob&carol pair", bob_account, carol_account, false);
    accounts_agree_internal("carol&alice pair", alice_account, carol_account, false);
    
    
    /* Change Password 2 ----------------------------------------------------------------------------------------*/
    CFReleaseNull(cfnewpassword);
    cfnewpassword = CFDataCreate(NULL, (uint8_t *) "ffoffoffo", 10);
    
    /* Bob */
    ok(AssertCreds(bob_account, cfaccount, cfnewpassword), "Credential resetting for Bob");
    is(countPeers(bob_account), 3, "There are three peers - Alice, Carol, Bob");
    is(countActivePeers(bob_account), 4, "There are four active peers - bob, alice, carol and iCloud");
    is(countActiveValidPeers(bob_account), 2, "There is two active valid peer - Bob and iCloud");
    FeedChangesToMulti(changes, alice_account, carol_account, NULL);

    /* Alice */
    ok(AssertCreds(alice_account, cfaccount, cfnewpassword), "Credential resetting for Alice");
    is(countPeers(alice_account), 3, "There are three peers - Alice, Carol, Bob");
    is(countActivePeers(alice_account), 4, "There are four active peers - bob, alice, carol and iCloud");
    is(countActiveValidPeers(alice_account), 3, "There are three active valid peers - alice, bob, and icloud");
    FeedChangesToMulti(changes, bob_account, carol_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carol_account, NULL);
    
    /* Carol */
    ok(AssertCreds(carol_account, cfaccount, cfnewpassword), "Credential resetting for Carol");
    is(countPeers(carol_account), 3, "There are three peers - Alice, Carol, Bob");
    is(countActivePeers(carol_account), 4, "There are four active peers - bob, alice, carol and iCloud");
    is(countActiveValidPeers(carol_account), 4, "There are three active valid peers - alice, bob, carol, and icloud");

    FeedChangesToMulti(changes, alice_account, bob_account, NULL);
    FeedChangesToMulti(changes, bob_account, carol_account, alice_account, NULL);
    FeedChangesToMulti(changes, bob_account, carol_account, alice_account, NULL);
    FeedChangesToMulti(changes, bob_account, carol_account, alice_account, NULL);
    accounts_agree_internal("bob&alice pair", bob_account, alice_account, false);

    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carol_account);
    CFReleaseNull(cfnewpassword);

}

int secd_58_password_change(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
	return 0;
}
