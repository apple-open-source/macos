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

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>
#include <SecureObjectSync/SOSKVSKeys.h>
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


static int kTestTestCount = 10;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfwrong_password = CFDataCreate(NULL, (uint8_t *) "NotFooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef data_name = CFSTR("TestSource");
    CFStringRef circle_key_name = SOSCircleKeyCreateWithName(data_name, NULL);
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), data_name);
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), data_name);
    SOSAccountRef carol_account = CreateAccountForLocalChanges(CFSTR("Carol"), data_name);
    
    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    FillAllChanges(changes);
    FeedChangesToMulti(changes, alice_account, carol_account, NULL);
    
    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    ok(!SOSAccountTryUserCredentials(alice_account, cfaccount, cfwrong_password, &error), "Credential failing (%@)", error);
    CFReleaseNull(cfwrong_password);
    is(error ? CFErrorGetCode(error) : 0, kSOSErrorWrongPassword, "Expected SOSErrorWrongPassword");
    CFReleaseNull(error);

    CFDataRef incompatibleDER = SOSCircleCreateIncompatibleCircleDER(&error);

    InjectChangeToMulti(changes, circle_key_name, incompatibleDER, alice_account, NULL);
    CFReleaseNull(circle_key_name);
    CFReleaseNull(incompatibleDER);

    is(ProcessChangesUntilNoChange(changes, alice_account, NULL), 1, "updates");

#if 0
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCError, "Is in circle");
    CFReleaseNull(error);
    
//#if 0
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob);
    
    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesTo(changes, alice_account, transport_alice);
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob); // Bob sees he's accepted
    
    FillChanges(changes, CFSTR("Bob"));
    FeedChangesTo(changes, alice_account, transport_alice); // Alice sees bob-concurring
    
    ok(CFDictionaryGetCount(changes) == 0, "We converged. (%@)", changes);
    
    FillChanges(changes, CFSTR("Alice"));
    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    CFDictionaryRef alice_new_gestalt = SOSCreatePeerGestaltFromName(CFSTR("Alice, but different"));
    
    ok(SOSAccountUpdateGestalt(alice_account, alice_new_gestalt), "Update gestalt %@ (%@)", alice_account, error);
    CFReleaseNull(alice_new_gestalt);
    
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob); // Bob sees alice change her name.
    
    FillChanges(changes, CFSTR("Bob"));
    FeedChangesTo(changes, alice_account, transport_alice); // Alice sees the fallout.
    
    accounts_agree("Alice's name changed", bob_account, alice_account);
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob); // Bob sees alice bail.
    
    FillChanges(changes, CFSTR("Bob"));
    FeedChangesTo(changes, alice_account), transport_alice; // Alice sees the fallout.
    
    FillChanges(changes, CFSTR("Alice"));
    FillChanges(changes, CFSTR("Bob"));
    accounts_agree("Alice bails", bob_account, alice_account);
    
    peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 1, "See one peer %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob);
    
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    FillChanges(changes, CFSTR("Bob"));
    FeedChangesTo(changes, alice_account, transport_alice); // Alice sees bob accepting
    
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob); // Bob sees Alice concurring
    
    accounts_agree("Alice accepts' Bob", bob_account, alice_account);
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob); // Bob sees Alice leaving and rejoining
    FillChanges(changes, CFSTR("Bob"));
    FeedChangesTo(changes, alice_account, transport_alice); // Alice sees bob concurring
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    FillChanges(changes, CFSTR("Bob"));
    FeedChangesTo(changes, alice_account, transport_alice); // Alice sees bob accepting
    FillChanges(changes, CFSTR("Alice"));
    FeedChangesTo(changes, bob_account, transport_bob); // Bob sees Alice concurring
    
    accounts_agree("Bob accepts Alice", bob_account, alice_account);
    
    
    CFReleaseNull(alice_new_gestalt);
#endif
    
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    CFReleaseNull(carol_account);
    
    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
    
    
}

int secd_55_account_incompatibility(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
