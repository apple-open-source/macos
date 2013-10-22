//
//  sc-60-account-cloud-identity.c
//  sec
//
//  Created by Mitch Adler on 6/25/13.
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


static int kTestTestCount = 97;

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
    CFReleaseNull(cfpassword);
    CFReleaseNull(error);
    
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);

    // Lost Application Scenario
    FeedChangesToMulti(changes, bob_account, carole_account, NULL);

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);

    ok(SOSAccountJoinCircles(carole_account, &error), "Carole Applies too (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesToMulti(changes, alice_account, bob_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, NULL);

    accounts_agree("alice and carole agree", alice_account, carole_account);
    accounts_agree("alice and bob agree", alice_account, bob_account);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }

    FeedChangesToMulti(changes, bob_account, carole_account, NULL);
    FeedChangesToMulti(changes, bob_account, alice_account, carole_account, NULL);
    accounts_agree("alice and carole agree", alice_account, carole_account);
    ok(CFDictionaryGetCount(changes) == 0, "Nothing left to deal with (%@)", changes);
    CFReleaseNull(error);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        ok(SOSAccountRejectApplicants(alice_account, applicants, &error), "Everyone out the pool");
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }
    
    FeedChangesToMulti(changes, bob_account, carole_account, NULL);
    FeedChangesToMulti(changes, bob_account, alice_account, carole_account, NULL);
    accounts_agree("alice and carole agree", alice_account, carole_account);
    ok(CFDictionaryGetCount(changes) == 0, "Nothing left to deal with (%@)", changes);

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        ok(applicants && CFArrayGetCount(applicants) == 0, "See no applicants %@ (%@)", applicants, error);
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob asks again");
    CFReleaseNull(error);
    FeedChangesToMulti(changes, bob_account, alice_account, carole_account, NULL);
    ok(CFDictionaryGetCount(changes) == 0, "Nothing left to deal with (%@)", changes);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicants %@ (%@)", applicants, error);
        CFReleaseNull(error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Accept bob into the fold");
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }
    
    FeedChangesTo(changes, bob_account);    // Countersign
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, NULL); // Everyone sees the fallout.
    ok(CFDictionaryGetCount(changes) == 0, "Nothing left to deal with (%@)", changes);

#if 0
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "Bob automatically re-applied %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(countPeers(alice_account, 0), 3, "Bob is accepted after auto-reapply");
    FeedChangesToMulti(changes, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, david_account, NULL);
    accounts_agree("alice and carole agree after bob gets in", alice_account, carole_account);
    
    // Rejected Application Scenario
    ok(SOSAccountJoinCircles(david_account, &error), "Dave Applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesTo(changes, alice_account);
    SOSAccountPurgePrivateCredential(alice_account);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountRejectApplicants(alice_account, applicants, &error), "Alice rejects (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, david_account, NULL);
    accounts_agree("alice and carole still agree after david is rejected", alice_account, carole_account);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);


    

    FeedChangesToMulti(changes, alice_account, carole_account, NULL); // Everyone sees conurring circle

    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);

    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    ok(SOSAccountJoinCirclesAfterRestore(carole_account, &error), "Carole cloud identiy joins (%@)", error);
    CFReleaseNull(error);
    
    is(countPeers(carole_account, false), 3, "Carole sees 3 valid peers after sliding in");

    FeedChangesTo(changes, bob_account);
    FeedChangesTo(changes, alice_account);
    FeedChangesToMulti(changes, bob_account, carole_account, NULL); // Bob and carole see the final result.

    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
#endif
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carole_account);
}

int secd_56_account_apply(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
