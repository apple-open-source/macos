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

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSTransport.h>

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 118;

/*
 static void trim_retirements_from_circle(SOSAccountRef account) {
 SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
 SOSCircleRemoveRetired(circle, NULL);
 });
 }
 */
static bool accept_applicants(SOSAccountRef account, int count) {
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
    bool retval = false;
    ok(applicants, "Have Applicants");
    if(!applicants) goto errout;
    is(CFArrayGetCount(applicants), count, "See applicants %@ (%@)", applicants, error);
    if(CFArrayGetCount(applicants) != count) goto errout;
    ok(retval = SOSAccountAcceptApplicants(account, applicants, &error), "Account accepts (%@)", error);
errout:
    CFReleaseNull(error);
    CFReleaseNull(applicants);
    return retval;
}


static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges ( CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges ( CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carole_account = CreateAccountForLocalChanges ( CFSTR("Carole"), CFSTR("TestSource"));
    SOSAccountRef david_account = CreateAccountForLocalChanges ( CFSTR("David"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(david_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 2, "updates");


    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    
    ok(accept_applicants(alice_account, 1), "Alice Accepts Application");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 3, "updates");

    accounts_agree("bob&alice pair", bob_account, alice_account);
    is(SOSAccountGetLastDepartureReason(bob_account, &error), kSOSNeverLeftCircle, "Bob affirms he hasn't left.");
    
    // ==============================  Alice and Bob are in the Account. ============================================
    
    
    ok(SOSAccountJoinCircles_wTxn(carole_account, &error), "Carole Applies (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, carole_account, david_account, NULL), 2, "updates");
    
    ok(accept_applicants(alice_account, 1), "Alice Accepts Application");
    
    // Let everyone concur.
    is(ProcessChangesUntilNoChange(changes, alice_account, carole_account, david_account, NULL), 3, "updates");

    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    
    ok(peers && CFArrayGetCount(peers) == 3, "See three peers %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    // SOSAccountPurgePrivateCredential(alice_account);
    
    ok(SOSAccountLeaveCircle(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, carole_account, david_account, NULL), 2, "updates");

    
    ok(SOSAccountJoinCircles_wTxn(david_account, &error), "David Applies (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, carole_account, david_account, NULL), 2, "updates");

    ok(accept_applicants(carole_account, 1), "Carole Accepts Application");
    
    // ==============================  We added Carole and David while Bob was in a drawer. Alice has left ============================================
    
    // ==============================  Bob comes out of the drawer seeing alice left and doesn't recognize the remainder. ============================================
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, david_account, NULL), 3, "updates");

    CFReleaseNull(error);
    is(SOSAccountGetCircleStatus(carole_account, &error), kSOSCCInCircle, "Carole still in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountGetCircleStatus(david_account, &error), kSOSCCInCircle, "David still in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountGetCircleStatus(bob_account, &error), kSOSCCNotInCircle, "Bob is not in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountGetLastDepartureReason(bob_account, &error), kSOSLeftUntrustedCircle, "Bob affirms he left because he doesn't know anyone.");
    CFReleaseNull(error);
    is(SOSAccountGetCircleStatus(alice_account, &error), kSOSCCNotInCircle, "Alice is not in Circle  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountGetLastDepartureReason(alice_account, &error), kSOSWithdrewMembership, "Alice affirms she left by request.");
    CFReleaseNull(error);
    
    
    CFReleaseNull(carole_account);
    CFReleaseNull(david_account);
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(cfpassword);
    
    SOSTestCleanup();
}

int secd_61_account_leave_not_in_kansas_anymore(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
