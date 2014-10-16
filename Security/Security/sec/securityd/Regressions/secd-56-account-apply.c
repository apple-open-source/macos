/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>
#include <SecureObjectSync/SOSTransport.h>

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"


static int kTestTestCount = 125;

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

    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carole_account = CreateAccountForLocalChanges(CFSTR("Carole"), CFSTR("TestSource"));
    SOSAccountRef david_account = CreateAccountForLocalChanges(CFSTR("David"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 1, "updates");

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
    is(ProcessChangesOnce(changes, alice_account, bob_account, carole_account, david_account, NULL), 1, "updates");

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountJoinCircles(carole_account, &error), "Carole Applies too (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 3, "updates");
    
    accounts_agree("alice and carole agree", alice_account, carole_account);
    accounts_agree("alice and bob agree", alice_account, bob_account);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 1, "updates");

    accounts_agree("alice and carole agree", alice_account, carole_account);

    CFReleaseNull(error);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        ok(applicants && CFArrayGetCount(applicants) == 2, "See two applicants %@ (%@)", applicants, error);
        ok(SOSAccountRejectApplicants(alice_account, applicants, &error), "Everyone out the pool");
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 2, "updates");

    accounts_agree("alice and carole agree", alice_account, carole_account);

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        ok(applicants && CFArrayGetCount(applicants) == 0, "See no applicants %@ (%@)", applicants, error);
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }
    
    ok(SOSAccountJoinCircles(bob_account, &error), "Bob asks again");
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 2, "updates");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicants %@ (%@)", applicants, error);
        CFReleaseNull(error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Accept bob into the fold");
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 3, "updates");

#if 0
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "Bob automatically re-applied %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(countPeers(alice_account, 0), 3, "Bob is accepted after auto-reapply");
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    accounts_agree("alice and carole agree after bob gets in", alice_account, carole_account);
    
    // Rejected Application Scenario
    ok(SOSAccountJoinCircles(david_account, &error), "Dave Applies (%@)", error);
    CFReleaseNull(error);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    SOSAccountPurgePrivateCredential(alice_account);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountRejectApplicants(alice_account, applicants, &error), "Alice rejects (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    accounts_agree("alice and carole still agree after david is rejected", alice_account, carole_account);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    FillAllChanges(changes);
    
    ok(CFDictionaryGetCount(CarolChanges) == 0, "We converged. (%@)", CarolChanges);
    ok(CFDictionaryGetCount(BobChanges) == 0, "We converged. (%@)", BobChanges);
    ok(CFDictionaryGetCount(AliceChanges) == 0, "We converged. (%@)", AliceChanges);
    ok(CFDictionaryGetCount(DavidChanges) == 0, "We converged. (%@)", DavidChanges);
    
    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    ok(SOSAccountJoinCirclesAfterRestore(carole_account, &error), "Carole cloud identiy joins (%@)", error);
    CFReleaseNull(error);
    
    is(countPeers(carole_account, false), 3, "Carole sees 3 valid peers after sliding in");
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL);
    
    FillAllChanges(changes);
    FeedChangesToMulti(AliceChanges, bob_account, carole_account, david_account, NULL);
    FeedChangesToMulti(BobChanges, alice_account, carole_account, david_account, NULL);
    FeedChangesToMulti(CarolChanges, bob_account, alice_account, david_account, NULL);
    FeedChangesToMulti(DavidChanges, bob_account, alice_account, carole_account, NULL); // Bob and carole see the final result.
    
    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
#endif
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carole_account);
    
    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
    
}

int secd_56_account_apply(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
