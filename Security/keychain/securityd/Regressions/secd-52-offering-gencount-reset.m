/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */




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

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfwrong_password = CFDataCreate(NULL, (uint8_t *) "NotFooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccount* carol_account = CreateAccountForLocalChanges(CFSTR("Carol"), CFSTR("TestSource"));

    SOSAccountTrustClassic *carolTrust = carol_account.trust;
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
    CFReleaseNull(error);
    ok(!SOSAccountTryUserCredentials(alice_account, cfaccount, cfwrong_password, &error), "Credential failing (%@)", error);
    CFReleaseNull(cfwrong_password);
    is(error ? CFErrorGetCode(error) : 0, kSOSErrorWrongPassword, "Expected SOSErrorWrongPassword");
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
    
    //bob now goes def while Alice does some stuff.
    
    ok([alice_account.trust leaveCircle:alice_account err:&error], "ALICE LEAVES THE CIRCLE (%@)", error);
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Alice resets to offering again (%@)", error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    accounts_agree("bob&alice pair", bob_account, alice_account);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "updates");

    
    ok(SOSAccountAssertUserCredentialsAndUpdate(carol_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    SOSAccountSetUserPublicTrustedForTesting(carol_account);
    ok(SOSAccountResetToOffering_wTxn(carol_account, &error), "Carol is going to push a reset to offering (%@)", error);

    int64_t valuePtr = 0;
    CFNumberRef gencount = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &valuePtr);

    SOSCircleSetGeneration(carolTrust.trustedCircle, gencount);
    
    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, (__bridge CFDataRef)(carol_account.accountKeyDerivationParameters), &error);
    CFNumberRef genCountTest = SOSCircleGetGeneration(carolTrust.trustedCircle);
    CFIndex testPtr;
    CFNumberGetValue(genCountTest, kCFNumberCFIndexType, &testPtr);
    ok(testPtr== 0);

    SOSCircleSignOldStyleResetToOfferingCircle(carolTrust.trustedCircle, carolTrust.fullPeerInfo, user_privkey, &error);
    
    SOSTransportCircleTestRemovePendingChange((SOSCircleStorageTransportTest*)carol_account.circle_transport, SOSCircleGetName(carolTrust.trustedCircle), NULL);
    CFDataRef circle_data = SOSCircleCopyEncodedData(carolTrust.trustedCircle, kCFAllocatorDefault, &error);
    if (circle_data) {
        [carol_account.circle_transport postCircle:SOSCircleGetName(carolTrust.trustedCircle) circleData:circle_data err:&error];
    }
    CFReleaseNull(circle_data);

    genCountTest = SOSCircleGetGeneration(carolTrust.trustedCircle);
    CFNumberGetValue(genCountTest, kCFNumberCFIndexType, &testPtr);
    ok(testPtr== 0);

    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    SOSAccountSetUserPublicTrustedForTesting(alice_account);
    SOSAccountSetUserPublicTrustedForTesting(bob_account);

    is(ProcessChangesUntilNoChange(changes, carol_account, alice_account, bob_account, NULL), 2, "updates");

    is([alice_account getCircleStatus:&error],kSOSCCNotInCircle,"alice is not in the account (%@)", error);
    is([bob_account getCircleStatus:&error], kSOSCCNotInCircle,"bob is not in the account (%@)", error);
    is([carol_account getCircleStatus:&error], kSOSCCInCircle,"carol is in the account (%@)", error);
    secLogEnable();
    [carol_account iCloudIdentityStatus:^(NSData *json, NSError *error) {
        diag("icloud identity JSON\n%s\n", [[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding] UTF8String]);
    }];
    secLogDisable();
    CFReleaseNull(gencount);
    CFReleaseNull(cfpassword);
    CFReleaseNull(user_privkey);
    alice_account = nil;
    bob_account = nil;
    carol_account = nil;

    SOSTestCleanup();
}
#endif

int secd_52_offering_gencount_reset(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(63);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    tests();
#else
    plan_tests(0);
#endif
    return 0;
}
