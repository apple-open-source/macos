/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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

static int kTestTestCount = 75;


static bool SOSTestAcceptApplicant(SOSAccountRef acceptor, CFIndex appsExpected, CFErrorRef *error) {
    bool retval = true;
    CFArrayRef applicants = SOSAccountCopyApplicants(acceptor, error);
    CFStringRef acceptorName = SOSAccountCopyName(acceptor);
    
    ok(applicants && CFArrayGetCount(applicants) == appsExpected, "See %ld applicant %@ (%@)", appsExpected, applicants, *error);
    if(CFArrayGetCount(applicants) != appsExpected) retval = false;
    ok(retval && (retval = SOSAccountAcceptApplicants(acceptor, applicants, error)), "%@ accepts (%@)", acceptorName, *error);
    CFReleaseNull(*error);
    CFReleaseNull(applicants);
    CFReleaseNull(acceptorName);
    
    return retval;
}

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carole_account = CreateAccountForLocalChanges(CFSTR("Carol"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");
    
    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");
    
    ok(SOSTestAcceptApplicant(alice_account, 1, &error), "Alice Accepts");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 3, "updates");
    
    accounts_agree("bob&alice pair", bob_account, alice_account);

    ok(SOSAccountLeaveCircle(alice_account, &error));
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    ok(SOSAccountAssertUserCredentialsAndUpdate(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(cfpassword);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    SOSAccountTransactionRef txn = SOSAccountTransactionCreate(carole_account);

    SOSPeerInfoRef carole_pi = SOSAccountCopyApplication(carole_account, &error);
    ok(carole_pi != NULL, "Got Carole's Application PeerInfo");
    CFReleaseNull(error);

    SOSAccountTransactionFinish(txn);
    CFReleaseNull(txn);

    SOSAccountPurgePrivateCredential(bob_account);

    CFDataRef bobBlob = SOSAccountCopyCircleJoiningBlob(bob_account, carole_pi, &error);
    ok(bobBlob != NULL, "Got Piggy Back Blob from Bob");
    CFReleaseNull(error);
    
    ok(SOSAccountJoinWithCircleJoiningBlob(carole_account, bobBlob, &error), "Carole joins circle through piggy backing: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(bobBlob);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "Settle Circle");
    
    ok(!SOSAccountIsInCircle(alice_account, NULL), "Alice still retired");
    ok(SOSAccountIsInCircle(bob_account, NULL), "Bob still here");
    ok(SOSAccountIsInCircle(carole_account, NULL), "carol still here");

    ok(!SOSAccountCheckHasBeenInSync_wTxn(carole_account), "Carol not in sync");
    
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carole_account);
    
    SOSTestCleanup();
}


int secd_90_hsa2(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
