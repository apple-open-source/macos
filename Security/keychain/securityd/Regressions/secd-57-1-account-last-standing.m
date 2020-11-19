//
//  secd-57-1-account-last-standing.c
//  sec
//
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
#include "SOSAccountTesting.h"

#if SOS_ENABLED

static bool acceptApplicants(SOSAccount* account, CFIndex count) {
    bool retval = false;
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);
    ok(applicants && CFArrayGetCount(applicants) == count, "See %ld applicants %@ (%@)", count, applicants, error);
    CFReleaseNull(error);
    require_quiet(CFArrayGetCount(applicants) == count, xit);
    ok((retval=SOSAccountAcceptApplicants(account, applicants, &error)), "Accept applicants into the fold");
    CFReleaseNull(error);
xit:
    CFReleaseNull(applicants);
    return retval;
}


static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account , cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, NULL), 1, "updates");
    
    ok(SOSAccountResetToOffering_wTxn(alice_account , &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, NULL), 1, "updates");
    
    ok([alice_account.trust leaveCircle:alice_account err:&error], "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, NULL), 1, "updates");
    
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountJoinCircles_wTxn(alice_account , &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, NULL), 1, "updates");
    
    ok([alice_account isInCircle:&error], "Alice is back in the circle (%@)", error);
    CFReleaseNull(error);

    is(countActivePeers(alice_account), 2, "Alice sees 2 active peers");
    is(countPeers(alice_account), 1, "Alice sees 1 valid peer");
    
    // Have Alice leave the circle just as Bob tries to join.
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccount* carole_account = CreateAccountForLocalChanges(CFSTR("Carole"), CFSTR("TestSource"));
    
    is(ProcessChangesUntilNoChange(changes, bob_account, alice_account, carole_account, NULL), 1, "updates");
    ok(SOSAccountTryUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, bob_account, alice_account, carole_account, NULL), 1, "updates");

    ok(SOSAccountJoinCircles_wTxn(carole_account , &error), "Carole applies (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, bob_account, alice_account, carole_account, NULL), 2, "updates");
    ok(acceptApplicants(alice_account, 1), "Alice accepts Carole");
    is(ProcessChangesUntilNoChange(changes, bob_account, alice_account, carole_account, NULL), 3, "updates");
    
    ok([alice_account.trust leaveCircle:alice_account err:&error], "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, carole_account, NULL), 2, "updates");

    ok([carole_account.trust leaveCircle:carole_account err:&error], "Carole Leaves (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, carole_account, NULL), 2, "updates");
    ok(SOSAccountJoinCircles_wTxn(bob_account , &error), "Bob applies (%@)", error);
    is(ProcessChangesUntilNoChange(changes, bob_account, NULL), 2, "updates");
    CFReleaseNull(error);
    
    is(countActivePeers(bob_account), 2, "Bob sees 2 active peers");
    is(countPeers(bob_account), 1, "Bob sees 1 valid peer");

    
    CFReleaseNull(cfpassword);

    alice_account = nil;
    SOSTestCleanup();
}
#endif

int secd_57_1_account_last_standing(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(45);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    tests();
#else
    plan_tests(0);
#endif
    return 0;
}
