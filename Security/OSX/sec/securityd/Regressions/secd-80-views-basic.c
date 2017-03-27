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
#include <Security/SecureObjectSync/SOSViews.h>

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

static void testViewLists(void) {
    CFSetRef allViews = SOSViewCopyViewSet(kViewSetAll);
    CFSetRef defaultViews = SOSViewCopyViewSet(kViewSetDefault);
    CFSetRef initialViews = SOSViewCopyViewSet(kViewSetInitial);
    CFSetRef alwaysOnViews = SOSViewCopyViewSet(kViewSetAlwaysOn);
    CFSetRef backupRequiredViews = SOSViewCopyViewSet(kViewSetRequiredForBackup);
    CFSetRef V0Views = SOSViewCopyViewSet(kViewSetV0);

    is(CFSetGetCount(allViews), 22, "make sure count of allViews is correct");
    is(CFSetGetCount(defaultViews), 18, "make sure count of defaultViews is correct");
    is(CFSetGetCount(initialViews), 14, "make sure count of initialViews is correct");
    is(CFSetGetCount(alwaysOnViews), 18, "make sure count of alwaysOnViews is correct");
    is(CFSetGetCount(backupRequiredViews), 3, "make sure count of backupRequiredViews is correct");
    is(CFSetGetCount(V0Views), 6, "make sure count of V0Views is correct");
    
    CFReleaseNull(allViews);
    CFReleaseNull(defaultViews);
    CFReleaseNull(initialViews);
    CFReleaseNull(alwaysOnViews);
    CFReleaseNull(backupRequiredViews);
    CFReleaseNull(V0Views);
}

static int kTestTestCount = 38;
static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFSetRef nullSet = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(test_factory, CFSTR("TestType"), test_source);
    
    SOSAccountRef account = CreateAccountForLocalChanges(CFSTR("Test Device"),CFSTR("TestType") );
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    ok(SOSAccountJoinCircles_wTxn(account, &error), "Join circle: %@", error);
    
    ok(NULL != account, "Created");

    ok(SOSAccountCheckHasBeenInSync_wTxn(account), "In sync already");

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
    
    ok(SOSAccountUpdateViewSets(account, SOSViewsGetV0ViewSet(), nullSet), "Expect not accepting kSOSKeychainV0");
    testView(account, kSOSCCViewNotMember, kSOSViewKeychainV0, kSOSCCViewQuery, "Expected no addition of kSOSKeychainV0");

    ok(SOSAccountUpdateViewSets(account, SOSViewsGetV0ViewSet(), nullSet), "Expect not accepting kSOSKeychainV0");
    testView(account, kSOSCCViewNotMember, kSOSViewKeychainV0, kSOSCCViewQuery, "Expected no addition of kSOSKeychainV0");

    SOSPeerInfoRef pi = SOSAccountGetMyPeerInfo(account);
    ok(pi, "should have the peerInfo");
    SOSViewResultCode vr = SOSViewsEnable(pi, kSOSViewKeychainV0, NULL);
    
    ok(vr == kSOSCCViewMember, "Set Virtual View manually");
    
    ok(!SOSAccountUpdateViewSets(account, nullSet, SOSViewsGetV0ViewSet()), "Expect not removing kSOSKeychainV0");
    testView(account, kSOSCCViewMember, kSOSViewKeychainV0, kSOSCCViewQuery, "Expected kSOSKeychainV0 is still there");
    
    ok(!SOSAccountUpdateViewSets(account, nullSet, SOSViewsGetV0ViewSet()), "Expect not removing kSOSKeychainV0");
    testView(account, kSOSCCViewMember, kSOSViewKeychainV0, kSOSCCViewQuery, "Expected kSOSKeychainV0 is still there");
   
    
    
    CFReleaseNull(account);
    
    SOSDataSourceRelease(test_source, NULL);
    SOSDataSourceFactoryRelease(test_factory);

    SOSTestCleanup();
}

int secd_80_views_basic(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    testViewLists();
    tests();
    
    return 0;
}
