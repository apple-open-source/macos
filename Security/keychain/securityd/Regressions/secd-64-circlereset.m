//
//  secd-64-circlereset.c
//  sec
//
//  Created by Richard Murphy on 7/22/15.
//
//



#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <CoreFoundation/CFDictionary.h>

#include "keychain/SecureObjectSync/SOSAccount.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include "keychain/securityd/SOSCloudCircleServer.h"

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static int64_t getCurrentGenCount(SOSAccount* account) {
    SOSAccountTrustClassic* trust = account.trust;
    return SOSCircleGetGenerationSint(trust.trustedCircle);
}

static bool SOSAccountResetWithGenCountValue(SOSAccount* account, int64_t gcount, CFErrorRef* error) {
    if (!SOSAccountHasPublicKey(account, error))
        return false;
    __block bool result = true;
    SOSAccountTrustClassic* trust = account.trust;

    result &= [account.trust resetAllRings:account err:error];

    trust.fullPeerInfo = nil;
    [trust setDepartureCode:kSOSWithdrewMembership];

    result &= [account.trust modifyCircle:account.circle_transport err:error action:^(SOSCircleRef circle) {
        SOSGenCountRef gencount = SOSGenerationCreateWithValue(gcount);
        result = SOSCircleResetToEmpty(circle, error);
        SOSCircleSetGeneration(circle, gencount);
        CFReleaseNull(gencount);
        return result;
    }];
    
    if (!result) {
        secerror("error: %@", error ? *error : NULL);
    }
    
    return result;
}

static SOSCircleRef SOSCircleCreateWithGenCount(int64_t gcount) {
    SOSCircleRef c = SOSCircleCreate(kCFAllocatorDefault, CFSTR("a"), NULL);
    SOSGenCountRef gencount = SOSGenerationCreateWithValue(gcount);
    SOSCircleSetGeneration(c, gencount);
    CFReleaseNull(gencount);
    return c;
}

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    SOSCircleRef c1 = SOSCircleCreateWithGenCount(1);
    SOSCircleRef c99 = SOSCircleCreateWithGenCount(99);
    ok(SOSCircleIsOlderGeneration(c1, c99), "Is Comparison working correctly?", NULL);
    CFReleaseNull(c1);
    CFReleaseNull(c99);
    
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    
    // Setup Circle with Bob and Alice in it
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");
    accounts_agree("bob&alice pair", bob_account, alice_account);
    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    uint64_t cnt = getCurrentGenCount(alice_account);

    ok(SOSAccountResetWithGenCountValue(alice_account, cnt-1, &error), "Alice resets the circle to empty with old value");
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    is([bob_account getCircleStatus:&error], 0, "Bob Survives bad circle post");
    is([alice_account getCircleStatus:&error], 1, "Alice does not survive bad circle post");

    CFReleaseNull(cfpassword);
    alice_account = nil;
    bob_account = nil;
    SOSTestCleanup();
}

int secd_64_circlereset(int argc, char *const *argv)
{
    plan_tests(35);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
