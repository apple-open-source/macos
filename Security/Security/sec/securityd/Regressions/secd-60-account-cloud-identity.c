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

static int kTestTestCount = 145;



static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountRef alice_account = CreateAccountForLocalChanges( CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carole_account = CreateAccountForLocalChanges(CFSTR("Carole"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountAssertUserCredentials(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 3, "updates");

    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    /*----- normal join after restore -----*/
    
    ok(SOSAccountJoinCirclesAfterRestore(carole_account, &error), "Carole cloud identity joins (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 4, "updates");

    is(countApplicants(alice_account), 0, "See no applicants");
    
    is(countPeers(carole_account), 3, "Carole sees 3 valid peers after sliding in");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
    
    ok(SOSAccountLeaveCircles(carole_account, &error), "Carol Leaves again");
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 3, "updates");

    /*----- join - join after restore -----*/
    
    ok(SOSAccountJoinCircles(carole_account, &error), "Carole normally joins (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    is(countApplicants(alice_account), 1, "See one applicant");
    
    ok(SOSAccountJoinCirclesAfterRestore(carole_account, &error), "Carole cloud identity joins (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 4, "updates");


    is(countApplicants(alice_account), 0, "See no applicants");
    
    is(countPeers(carole_account), 3, "Carole sees 3 valid peers after sliding in");

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
    
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

int secd_60_account_cloud_identity(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
