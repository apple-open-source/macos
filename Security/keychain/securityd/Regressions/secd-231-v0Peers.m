/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

static void testView(SOSAccount* account, SOSViewResultCode expected, CFStringRef view, SOSViewActionCode action, char *label) {
    CFErrorRef error = NULL;
    SOSViewResultCode vcode = 9999;
    switch(action) {
        case kSOSCCViewQuery:
            vcode = [account.trust viewStatus:account name:view err:&error];
            break;
        case kSOSCCViewEnable:
        case kSOSCCViewDisable: // fallthrough
            vcode = [account.trust updateView:account name:view code:action err:&error];
            break;
        default:
            break;
    }
    is(vcode, expected, "%s (%@)", label, error);
    CFReleaseNull(error);
}

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccount* alice_account = CreateAccountForLocalChanges( CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
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
    
    SOSAccountUpdatePeerInfoAndPush(bob_account, NULL, &error, ^bool(SOSPeerInfoRef peer, CFErrorRef *error) {
        ok(peer, "bob should have the peerInfo");

        SOSViewResultCode vr = SOSViewsEnable(peer, kSOSViewKeychainV0, NULL);
        
        ok(vr == kSOSCCViewMember, "Set Virtual View manually");
        return true;
    });
    
    testView(bob_account, kSOSCCViewMember, kSOSViewKeychainV0, kSOSCCViewQuery, "Bob's Account expected kSOSKeychainV0 enabled");

    is(ProcessChangesUntilNoChange(changes, bob_account, alice_account, NULL), 2, "updates");

    CFArrayRef peers = SOSAccountCopyViewUnawarePeers_wTxn(bob_account, &error);
    ok(peers, "peers should not be nil");
    is(CFArrayGetCount(peers), 1, "count should be 1");
    SOSPeerInfoRef bobPeer = (SOSPeerInfoRef)CFArrayGetValueAtIndex(peers, 0);
    ok(CFStringCompare(SOSPeerInfoGetPeerID(bobPeer), SOSPeerInfoGetPeerID(bob_account.peerInfo), 0) == kCFCompareEqualTo, "PeerInfo's should be equal");
    
    [alice_account removeV0Peers:^(bool removedV0Peer, NSError *error) {
        is(removedV0Peer, true, "v0 peer should be removed");
    }];
    
    is(ProcessChangesUntilNoChange(changes, bob_account, alice_account, NULL), 2, "updates");
    
    is([bob_account getCircleStatus:&error], kSOSCCNotInCircle, "Bob should not be in the circle");

    CFReleaseNull(changes);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);

    alice_account = nil;
    bob_account = nil;
    SOSTestCleanup();
}
#endif

int secd_231_v0Peers(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(41);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    tests();
    secd_test_teardown_delete_temp_keychain(__FUNCTION__);
#else
    plan_tests(0);
#endif
    return 0;
}
