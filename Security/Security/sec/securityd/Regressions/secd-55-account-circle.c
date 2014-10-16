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

static int kTestTestCount = 304;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfwrong_password = CFDataCreate(NULL, (uint8_t *) "NotFooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccountRef carol_account = CreateAccountForLocalChanges(CFSTR("Carol"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
    CFReleaseNull(error);
    ok(!SOSAccountTryUserCredentials(alice_account, cfaccount, cfwrong_password, &error), "Credential failing (%@)", error);
    CFReleaseNull(cfwrong_password);
    is(error ? CFErrorGetCode(error) : 0, kSOSErrorWrongPassword, "Expected SOSErrorWrongPassword");
    CFReleaseNull(error);
    
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 3, "updates");

    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    CFDictionaryRef alice_new_gestalt = SOSCreatePeerGestaltFromName(CFSTR("Alice, but different"));

    ok(SOSAccountUpdateGestalt(alice_account, alice_new_gestalt), "Update gestalt %@ (%@)", alice_account, error);
    SOSAccountUpdateTestTransports(alice_account, alice_new_gestalt);
    CFReleaseNull(alice_new_gestalt);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    accounts_agree("Alice's name changed", bob_account, alice_account);
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 3, "updates");

    accounts_agree("Alice bails", bob_account, alice_account);
    
    peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 1, "See one peer %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 3, "updates");

    accounts_agree("Alice accepts' Bob", bob_account, alice_account);
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 4, "updates");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 3, "updates");

    accounts_agree("Bob accepts Alice", bob_account, alice_account);
    
    // As of PR-13917727/PR-13906870 this no longer works (by "design"), in favor of making another more common
    // failure (apply/OSX-psudo-reject/re-apply) work.  Write races might be "fixed better" with affirmitave rejection.
#if 0
    
    //
    // Write race emulation.
    //
    //
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    FeedChangesTo(changes, bob_account); // Bob sees Alice leaving and rejoining
    FeedChangesTo(changes, alice_account); // Alice sees bob concurring
    
    accounts_agree("Alice leaves & returns", bob_account, alice_account);
    
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesTo(changes, bob_account); // Bob sees Alice Applying
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    CFMutableDictionaryRef bobAcceptanceChanges = ExtractPendingChanges(changes);
    
    // Alice re-applies without seeing that she was accepted.
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves again  (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    CFReleaseSafe(ExtractPendingChanges(changes)); // Alice loses the race to write her changes - bob never sees em.
    
    FeedChangesTo(&bobAcceptanceChanges, alice_account); // Alice sees bob inviting her in the circle.
    
    FeedChangesTo(changes, bob_account); // Bob sees Alice Concurring
    
    // As of PR-13917727/PR-13906870
    accounts_agree("Alice leave, applies back, loses a race and eventually gets in", bob_account, alice_account);
#endif
    
    // Both in circle.
    
    // Emulation of <rdar://problem/13919554> Innsbruck11A368 +Roots: Device A was removed when Device B joined.
    
    // We want Alice to leave circle while an Applicant on a full concordance signed circle with old-Alice as an Alum and Bob a peer.
    // ZZZ
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice leaves once more  (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 3, "updates");
    accounts_agree("Alice and Bob see Alice out of circle", bob_account, alice_account);
    
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice leaves while applying  (%@)", error);
    FeedChangesTo(changes, bob_account); // Bob sees Alice become an Alum.
    
    CFReleaseNull(error);
    
    is(SOSAccountIsInCircles(alice_account, &error), kSOSCCNotInCircle, "Alice isn't applying any more");
    accounts_agree("Alice leaves & some fancy concordance stuff happens", bob_account, alice_account);
    
    ok(SOSAccountJoinCircles(alice_account, &error), "Alice re-applies (%@)", error);
    CFReleaseNull(error);
    
    FeedChangesTo(changes, bob_account); // Bob sees Alice reapply.
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 2, "updates");

    accounts_agree("Alice comes back", bob_account, alice_account);
    
    // Emulation of <rdar://problem/13889901>
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carol_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentials(carol_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(cfpassword);
    ok(SOSAccountJoinCircles(carol_account, &error), "Carol Applies (%@)", error);
    CFReleaseNull(error);

    CFMutableDictionaryRef dropped_changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    FillChanges(dropped_changes, carol_account);
    CFReleaseNull(dropped_changes);

    ok(SOSAccountResetToOffering(carol_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, bob_account, carol_account,  NULL), 2, "updates");
    accounts_agree("13889901", carol_account, bob_account);
    is(SOSAccountGetLastDepartureReason(bob_account, &error), kSOSMembershipRevoked, "Bob affirms he hasn't left.");
    
    ok(SOSAccountJoinCircles(bob_account, &error), "Bob ReApplies (%@)", error);
    is(ProcessChangesUntilNoChange(changes, bob_account, carol_account, NULL), 2, "updates");
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(carol_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(carol_account, applicants, &error), "Carol accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    is(ProcessChangesUntilNoChange(changes, bob_account, carol_account, NULL), 3, "updates");
    accounts_agree("rdar://problem/13889901-II", bob_account, carol_account);
    
    CFReleaseNull(alice_new_gestalt);
    
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

int secd_55_account_circle(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
