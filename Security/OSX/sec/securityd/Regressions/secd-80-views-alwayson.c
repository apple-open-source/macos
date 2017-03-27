/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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

//
//  secd-80-views-alwayson.c
//  Security
//
//
//


#include <CoreFoundation/CFDictionary.h>
#include <utilities/SecCFWrappers.h>

#include <Security/SecureObjectSync/SOSAccount.h>

#include "secd_regressions.h"
#include "SOSAccountTesting.h"
#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 46;


static void testView(SOSAccountRef account, SOSViewResultCode expected, CFStringRef view, SOSViewActionCode action, char *label) {
    CFErrorRef error = NULL;
    SOSViewResultCode vcode = 9999;
    switch(action) {
        case kSOSCCViewQuery:
            vcode = SOSAccountViewStatus(account, view, &error);
            break;
        case kSOSCCViewEnable:
        case kSOSCCViewDisable: // fallthrough
            vcode = SOSAccountUpdateView(account, view, action, &error);
            break;
        default:
            break;
    }
    is(vcode, expected, "%s (%@)", label, error);
    CFReleaseNull(error);
}

/*
 Make a circle with two peers - alice and bob
 Check for ContinuityUnlock View on Alice - it should be there
 turn off ContinuityUnlock on Alice
 Change the password with Bob - makeing Alice invalid
 Update Alice with the new password
 see that ContinuityUnlock is automatically back on because it's "always on"
 */

static void alwaysOnTest()
{
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfpasswordNew = CFDataCreate(NULL, (uint8_t *) "FooFooFo2", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    
    // Start Circle
    ok(SOSTestStartCircleWithAccount(alice_account, changes, cfaccount, cfpassword), "Have Alice start a circle");
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    ok(SOSTestJoinWithApproval(cfpassword, cfaccount, changes, alice_account, bob_account, KEEP_USERKEY, 2, false), "Bob Joins");
    
    testView(alice_account, kSOSCCViewMember, kSOSViewContinuityUnlock, kSOSCCViewQuery, "Expected view capability for kSOSViewContinuityUnlock");
    testView(alice_account, kSOSCCViewNotMember, kSOSViewContinuityUnlock, kSOSCCViewDisable, "Expected to disable kSOSViewContinuityUnlock");

    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpasswordNew, NULL), "Bob changes the password");
    testView(alice_account, kSOSCCViewNotMember, kSOSViewContinuityUnlock, kSOSCCViewQuery, "Expected  kSOSViewContinuityUnlock is off for alice still");
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpasswordNew, NULL), "Alice sets the new password");
    testView(alice_account, kSOSCCViewMember, kSOSViewContinuityUnlock, kSOSCCViewQuery, "Expected view capability for kSOSViewContinuityUnlock");

    CFReleaseNull(alice_account);
    CFReleaseNull(bob_account);
    CFReleaseNull(changes);
    
    SOSTestCleanup();
}

int secd_80_views_alwayson(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    alwaysOnTest();
    return 0;
}
