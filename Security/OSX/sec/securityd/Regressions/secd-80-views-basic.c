//
//  secd-80-views-basic.c
//  sec
//
//  Created by Richard Murphy on 1/26/15.
//
//

#include <stdio.h>
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

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>

#include <securityd/SOSCloudCircleServer.h>
#include "SecdTestKeychainUtilities.h"
#include "SOSAccountTesting.h"


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

static int kTestTestCount = 21;
static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(test_factory, CFSTR("TestType"), test_source);
    
    SOSAccountRef account = CreateAccountForLocalChanges(CFSTR("Test Device"),CFSTR("TestType") );
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    ok(SOSAccountJoinCircles(account, &error), "Join circle: %@", error);
    
    ok(NULL != account, "Created");

    testView(account, kSOSCCViewNotMember, kSOSViewKeychainV0, kSOSCCViewQuery, "Expected view capability for kSOSViewKeychain");
    // Default views no longer includes kSOSViewAppleTV
    testView(account, kSOSCCViewMember, kSOSViewAppleTV, kSOSCCViewQuery, "Expected view capability for kSOSViewAppleTV");
    testView(account, kSOSCCViewMember, kSOSViewPCSPhotos, kSOSCCViewQuery, "Expected no view capability for kSOSViewPCSPhotos");
    testView(account, kSOSCCViewMember, kSOSViewPCSiCloudDrive, kSOSCCViewQuery, "Expected no view capability for kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCNoSuchView, CFSTR("FOO"), kSOSCCViewQuery, "Expected no such view for FOO");
    
    testView(account, kSOSCCViewMember, kSOSViewPCSiCloudDrive, kSOSCCViewEnable, "Expected to enable kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCViewMember, kSOSViewPCSiCloudDrive, kSOSCCViewQuery, "Expected view capability for kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCViewNotMember, kSOSViewPCSiCloudDrive, kSOSCCViewDisable, "Expected to disable kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCViewNotMember, kSOSViewPCSiCloudDrive, kSOSCCViewQuery, "Expected no view capability for kSOSViewPCSiCloudDrive");

    testView(account, kSOSCCViewMember, kSOSViewPCSiCloudDrive, kSOSCCViewEnable, "Expected to enable kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCViewMember, kSOSViewPCSiCloudDrive, kSOSCCViewQuery, "Expected view capability for kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCViewNotMember, kSOSViewKeychainV0, kSOSCCViewEnable, "Expected to enable kSOSViewKeychainV0");
    testView(account, kSOSCCViewMember, kSOSViewPCSiCloudDrive, kSOSCCViewQuery, "Expected view capability for kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCViewMember, kSOSViewAppleTV, kSOSCCViewEnable, "Expected to enable kSOSViewAppleTV");

    testView(account, kSOSCCViewMember, kSOSViewPCSiCloudDrive, kSOSCCViewQuery, "Expected view capability for kSOSViewPCSiCloudDrive");
    testView(account, kSOSCCViewNotMember, kSOSViewKeychainV0, kSOSCCViewQuery, "Expected view capability for kSOSViewKeychainV0");
    testView(account, kSOSCCViewMember, kSOSViewAppleTV, kSOSCCViewQuery, "Expected view capability for kSOSViewAppleTV");

    CFReleaseNull(account);
    
    SOSDataSourceRelease(test_source, NULL);
    SOSDataSourceFactoryRelease(test_factory);

    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
    
}

int secd_80_views_basic(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
