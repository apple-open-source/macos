//
//  secd-100-initialsync.c
//  sec
//


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
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 43;

static void tests(void)
{

    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(test_factory, CFSTR("TestType"), test_source);
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("AliceAccount"),CFSTR("TestType") );
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("BobAccount"),CFSTR("TestType") );
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    ok(SOSAccountJoinCircles_wTxn(alice_account, &error), "Join circle: %@", error);
    ok(SOSAccountCheckHasBeenInSync_wTxn(alice_account), "Alice account initial sync done");

    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");

    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 4, "updates");
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }

    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");
    
    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    ok(!SOSAccountCheckHasBeenInSync_wTxn(bob_account), "Bob should not be initially synced");
    CFSetRef bob_viewSet = SOSPeerInfoCopyEnabledViews(SOSAccountGetMyPeerInfo(bob_account));
    is(CFSetGetCount(bob_viewSet), 14, "bob's initial view set should be just the 14 views");
    CFReleaseNull(bob_viewSet);
    
    ok(!SOSAccountCheckHasBeenInSync_wTxn(bob_account), "Bob should not be initially synced");
    SOSAccountPeerGotInSync_wTxn(bob_account, SOSAccountGetMyPeerInfo(alice_account));
    
    bob_viewSet = SOSPeerInfoCopyEnabledViews(SOSAccountGetMyPeerInfo(bob_account));
    is(CFSetGetCount(bob_viewSet), 18, "bob's initial view set should be just the back up");
    CFReleaseNull(bob_viewSet);
    
}

int secd_100_initialsync(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
