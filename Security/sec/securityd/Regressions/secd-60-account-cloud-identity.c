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


static int kTestTestCount = 95;



static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountRef alice_account = CreateAccountForLocalChanges(changes, CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(changes, CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carole_account = CreateAccountForLocalChanges(changes, CFSTR("Carole"), CFSTR("TestSource"));

    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    // Bob wins writing at this point, feed the changes back to alice.

    FeedChangesToMulti(changes, alice_account, carole_account, NULL);

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountAssertUserCredentials(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);

    FeedChangesToMulti(changes, bob_account, carole_account, NULL);

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

    FeedChangesToMulti(changes, alice_account, carole_account, NULL); // Everyone sees conurring circle

    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);

    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    /*----- normal join after restore -----*/

    ok(SOSAccountJoinCirclesAfterRestore(carole_account, &error), "Carole cloud identity joins (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesToMulti(changes, bob_account, carole_account, NULL); // Bob and carole see the final result.
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, NULL); // Bob and carole see the final result.

    is(countApplicants(alice_account), 0, "See no applicants");
    
    is(countPeers(carole_account), 3, "Carole sees 3 valid peers after sliding in");

    FeedChangesToMulti(changes, bob_account, carole_account, NULL); // Bob and carole see the final result.

    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
    
    ok(SOSAccountLeaveCircles(carole_account, &error), "Carol Leaves again");
    CFReleaseNull(error);
    FeedChangesToMulti(changes, bob_account, alice_account, NULL);
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, NULL);
    
    /*----- join - join after restore -----*/
    
    ok(SOSAccountJoinCircles(carole_account, &error), "Carole normally joins (%@)", error);
    CFReleaseNull(error);
    FeedChangesTo(changes, alice_account);
    
    is(countApplicants(alice_account), 1, "See one applicant");
    
    ok(SOSAccountJoinCirclesAfterRestore(carole_account, &error), "Carole cloud identity joins (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesToMulti(changes, bob_account, carole_account, NULL); // Bob and carole see the final result.
    FeedChangesToMulti(changes, alice_account, bob_account, carole_account, NULL); // Bob and carole see the final result.
    
    is(countApplicants(alice_account), 0, "See no applicants");
    
    is(countPeers(carole_account), 3, "Carole sees 3 valid peers after sliding in");
    
    FeedChangesToMulti(changes, bob_account, carole_account, NULL); // Bob and carole see the final result.
    
    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
    

    
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carole_account);
}

int secd_60_account_cloud_identity(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
