//
//  secd-59-account-cleanup.c
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


static int kTestTestCount = 93;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef circle_name = CFSTR("TestSource");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges(changes, CFSTR("Alice"), circle_name);
    SOSAccountRef bob_account = CreateAccountForLocalChanges(changes, CFSTR("Bob"), circle_name);
    SOSAccountRef carole_account = CreateAccountForLocalChanges(changes, CFSTR("Carole"), circle_name);

    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    
    FeedChangesToMulti(changes, alice_account, carole_account, NULL);
    
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
    
    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);

    SOSFullPeerInfoRef fpiAlice = SOSAccountGetMyFullPeerInCircleNamed(alice_account, circle_name, NULL);
    CFStringRef alice_id = CFStringCreateCopy(NULL, SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(fpiAlice)));
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    FeedChangesTo(changes, bob_account); // Bob sees alice bail.
    
    is(CFDictionaryGetCountOfValue(changes, kCFNull),2, "2 Keys Nulled Out");

    CFDictionaryForEach(changes, ^(const void *key, const void *value) {
        if(isNull(value)) {
            CFStringRef circle_name = NULL, from_name = NULL, to_name = NULL;
            SOSKVSKeyType keytype = SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, &from_name, &to_name);
            is(keytype, kMessageKey, "Expect only a message key");
            bool testcmp = CFEqualSafe(alice_id, from_name) || CFEqualSafe(alice_id, to_name);
            ok(testcmp, "Alice is from_name(%@) or to_name(%@)", from_name, to_name);
            CFReleaseNull(circle_name);
            CFReleaseNull(from_name);
            CFReleaseNull(to_name);
        }
    });

    FeedChangesToMulti(changes, alice_account, carole_account, NULL);

    accounts_agree("Alice bails", bob_account, alice_account);
    accounts_agree("Alice bails", bob_account, carole_account);
    
    SOSAccountCleanupRetirementTickets(bob_account, 0, &error);
    is(CFDictionaryGetCountOfValue(changes, kCFNull),0, "0 Keys Nulled Out");
    
    ok(SOSAccountJoinCircles(carole_account, &error), "Carole Applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesTo(changes, bob_account);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(bob_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts Carole (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }

    // Bob should not yet cleanup Alice's retirment here on his own since it hasn't been long enough
    // by default.
    is(CFDictionaryGetCountOfValue(changes, kCFNull),0, "0 Keys Nulled Out");

    FeedChangesTo(changes, carole_account); // Carole sees he's accepted
    FeedChangesTo(changes, bob_account); // Bob sees she's all happy.

    accounts_agree("Carole joins", bob_account, carole_account);

    SOSAccountCleanupRetirementTickets(bob_account, 0, &error);

    is(countPeers(bob_account), 2, "Active peers after forced cleanup");
    is(countActivePeers(bob_account), 3, "Inactive peers after forced cleanup");

    is(CFDictionaryGetCountOfValue(changes, kCFNull), 1, "1 Keys Nulled Out");
    
    CFDictionaryForEach(changes, ^(const void *key, const void *value) {
        if(isNull(value)) {
            CFStringRef circle_name = NULL, retiree = NULL;
            SOSKVSKeyType keytype = SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, &retiree, NULL);
            is(keytype, kRetirementKey, "Expect only a retirement key");
            ok(CFEqualSafe(alice_id, retiree), "Alice (%@) is retiree (%@)", alice_id, retiree);
            CFReleaseNull(circle_name);
            CFReleaseNull(retiree);
        }
    });
    
    CFReleaseNull(alice_id);
    CFReleaseNull(carole_account);
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
}

int secd_59_account_cleanup(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
	return 0;
}
