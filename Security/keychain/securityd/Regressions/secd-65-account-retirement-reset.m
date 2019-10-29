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


typedef void (^stir_block)(int expected_iterations);
typedef int (^execute_block)(void);

static void stirBetween(stir_block stir, ...) {
    va_list va;
    va_start(va, stir);

    execute_block execute = NULL;

    while ((execute = va_arg(va, execute_block)) != NULL)
        stir(execute());
}

__unused static void VerifyCountAndAcceptAllApplicants(SOSAccount* account, int expected)
{
    CFErrorRef error = NULL;
    CFArrayRef applicants = SOSAccountCopyApplicants(account, &error);

    SKIP: {
        skip("Empty applicant array", 2, applicants);

        is(CFArrayGetCount(applicants), expected, "Applicants: %@ (%@)", applicants, error);
        CFReleaseNull(error);

        ok(SOSAccountAcceptApplicants(account , applicants, &error), "Accepting all (%@)", error);
        CFReleaseNull(error);
    }

    CFReleaseNull(applicants);
}


static void tests(void)
{
    __block CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    const CFStringRef data_source_name = CFSTR("TestSource");

    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), data_source_name);
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), data_source_name);
    SOSAccount* carole_account = CreateAccountForLocalChanges(CFSTR("Carole"), data_source_name);

    SOSAccount* alice_resurrected = NULL;

    __block CFDataRef frozen_alice = NULL;


    stirBetween(^(int expected){
        is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), expected, "stirring");
    }, ^{
        ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account , cfaccount, cfpassword, &error), "bob credential setting (%@)", error);

        return 1;
    }, ^{
        ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account , cfaccount, cfpassword, &error), "alice credential setting (%@)", error);
        CFReleaseNull(error);

        ok(SOSAccountAssertUserCredentialsAndUpdate(carole_account,  cfaccount, cfpassword, &error), "carole credential setting (%@)", error);
        CFReleaseNull(error);

        ok(SOSAccountResetToOffering_wTxn(alice_account , &error), "Reset to offering (%@)", error);
        CFReleaseNull(error);

        return 2;
    }, ^{
        NSError *ns_error = nil;
        frozen_alice = (CFDataRef) CFBridgingRetain([alice_account encodedData:&ns_error]);
        ok(frozen_alice, "Copy encoded %@", ns_error);
        ns_error = nil;

        SOSAccountPurgePrivateCredential(alice_account);

        ok([alice_account.trust leaveCircle:alice_account err:&error], "Alice Leaves (%@)", error);
        CFReleaseNull(error);

        return 2;
    }, ^{
        ok(SOSAccountResetToOffering_wTxn(bob_account , &error), "Reset to offering (%@)", error);
        CFReleaseNull(error);

        return 2;
    },
    NULL);

    alice_resurrected = CreateAccountForLocalChangesFromData(frozen_alice, CFSTR("Alice risen"), data_source_name);
    // This is necessary from the change that makes accounts not inflate if the private key was lost - alice_resurected now
    // Starts as a brand new account, so this whole series of tests needs to amount to "is this brand new"?
    // The trigger is alice leaving the circle - that kills the deviceKey.
    ProcessChangesUntilNoChange(changes, alice_resurrected, bob_account, carole_account, NULL);

    stirBetween(^(int expected){
        is(ProcessChangesUntilNoChange(changes, alice_resurrected, bob_account, carole_account, NULL), expected, "stirring");
    }, ^{
        ok(SOSAccountAssertUserCredentialsAndUpdate(alice_resurrected,  cfaccount, cfpassword, &error), "alice_resurrected credential setting (%@)", error);
        CFReleaseNull(error);
        return 1;
    }, ^{
        ok(![alice_resurrected isInCircle:&error], "Ressurrected not in circle: %@", error);
        CFReleaseNull(error);

        ok([bob_account isInCircle:&error], "Should be in circle: %@", error);
        CFReleaseNull(error);

        return 1;
    },
    NULL);

    CFReleaseNull(frozen_alice);
    alice_account = nil;
    bob_account = nil;
    SOSTestCleanup();
}

int secd_65_account_retirement_reset(int argc, char *const *argv)
{
    plan_tests(28);

    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
