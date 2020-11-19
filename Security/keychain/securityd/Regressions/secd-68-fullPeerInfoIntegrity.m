//
//  secd-68-fullPeerInfoIntegrity.m
//  secdRegressions
//
//  Created by Richard Murphy on 4/30/20.
//

#import <Foundation/Foundation.h>

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
#if SOS_ENABLED

static NSString *makeCircle(SOSAccount* testaccount, CFMutableDictionaryRef changes) {
    CFErrorRef error = NULL;

    // Every time we resetToOffering should result in a new fpi
    NSString *lastPeerID = testaccount.peerID;
    ok(SOSAccountResetToOffering_wTxn(testaccount, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, testaccount, NULL), 1, "updates");
    NSString *currentPeerID = testaccount.peerID;
    ok(![lastPeerID isEqualToString:currentPeerID], "peerID changed on circle reset");
    return currentPeerID;
}

static void tests(void) {
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* testaccount = CreateAccountForLocalChanges(CFSTR("TestDev"), CFSTR("TestSource"));

    // Just making an account object to mess with
    ok(SOSAccountAssertUserCredentialsAndUpdate(testaccount, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    is(ProcessChangesUntilNoChange(changes, testaccount, NULL), 1, "updates");
    CFReleaseNull(error);

    // make a circle then make sure fpi isn't reset just for a normal ensureFullPeerAvailable
    NSString *lastPeerID = makeCircle(testaccount, changes);
    ok([testaccount.trust ensureFullPeerAvailable:testaccount err:&error], "fullPeer is available");
    NSString *currentPeerID = testaccount.peerID;
    ok([lastPeerID isEqualToString: currentPeerID], "peerID did not alter in trip through ensureFullPeerAvailable");


    // leaving a circle should reset the fpi
    lastPeerID = makeCircle(testaccount, changes);
    ok([testaccount.trust leaveCircle:testaccount err:&error], "leave the circle %@", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, testaccount, NULL), 1, "updates");
    currentPeerID = testaccount.peerID;
    ok(![lastPeerID isEqualToString:currentPeerID], "peerID changed on leaving circle");

    // break the fullpeerinfo by purging the private key - then fix in ensureFullPeerAvailable
    lastPeerID = makeCircle(testaccount, changes);
    ok(SOSFullPeerInfoPurgePersistentKey(testaccount.fullPeerInfo, &error), "purging persistent key %@", error);
    currentPeerID = testaccount.peerID;
    ok([lastPeerID isEqualToString:currentPeerID], "pre-ensuring peerID remains the same");
    lastPeerID = currentPeerID;
    ok([testaccount.trust ensureFullPeerAvailable:testaccount err:&error], "fullPeer is available");
    currentPeerID = testaccount.peerID;
    ok(![lastPeerID isEqualToString: currentPeerID], "peerID changed because fullPeerInfo fixed in ensureFullPeerAvailable");
    lastPeerID = currentPeerID;

    // If that last thing worked this peer won't be in the circle any more - changing fpi changes "me"
    ok(SOSAccountAssertUserCredentialsAndUpdate(testaccount, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    is(ProcessChangesUntilNoChange(changes, testaccount, NULL), 1, "updates");
    CFReleaseNull(error);
    ok(![testaccount isInCircle: &error], "No longer in circle");

    // This join should work because the peer we left in the circle will be a ghost and there are no other peers
    ok(SOSAccountJoinCircles_wTxn(testaccount, &error), "Apply to circle (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, testaccount, NULL), 1, "updates");
    ok([testaccount isInCircle: &error], "Is in circle");
    currentPeerID = testaccount.peerID;
    ok(![lastPeerID isEqualToString: currentPeerID], "peerID changed because fullPeerInfo changed during join");

    CFReleaseNull(cfpassword);
    testaccount = nil;
    SOSTestCleanup();
}
#endif

int secd_68_fullPeerInfoIntegrity(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(29);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    tests();
#else
    plan_tests(0);
#endif
    return 0;
}
