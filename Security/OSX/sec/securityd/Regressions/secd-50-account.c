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

static int kTestTestCount = 9 + kSecdTestSetupTestCount;
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
    
    ok(NULL != account, "Created");

    size_t size = SOSAccountGetDEREncodedSize(account, &error);
    CFReleaseNull(error);
    uint8_t buffer[size];
    uint8_t* start = SOSAccountEncodeToDER(account, &error, buffer, buffer + sizeof(buffer));
    CFReleaseNull(error);
    
    ok(start, "successful encoding");
    ok(start == buffer, "Used whole buffer");
    
    const uint8_t *der = buffer;
    SOSAccountRef inflated = SOSAccountCreateFromDER(kCFAllocatorDefault, test_factory,
                                                     &error, &der, buffer + sizeof(buffer));
    
    SOSAccountEnsureFactoryCirclesTest(inflated, CFSTR("Test Device"));
    ok(inflated, "inflated");
    ok(CFEqual(inflated, account), "Compares");
    CFReleaseNull(inflated);
    
    CFDictionaryRef new_gestalt = SOSCreatePeerGestaltFromName(CFSTR("New Device"));
    ok(SOSAccountResetToOffering_wTxn(account, &error), "Reset to Offering  (%@)", error);
    CFReleaseNull(error);
    is(SOSAccountGetCircleStatus(account, &error), kSOSCCInCircle, "Was in Circle  (%@)", error);
    CFReleaseNull(error);
    
    SOSAccountUpdateGestalt(account, new_gestalt);
    
    is(SOSAccountGetCircleStatus(account, &error), kSOSCCInCircle, "Still in Circle  (%@)", error);
    CFReleaseNull(error);
    
    CFReleaseNull(new_gestalt);
    CFReleaseNull(account);
    
    SOSDataSourceFactoryRelease(test_factory);
    SOSDataSourceRelease(test_source, NULL);
        
    SOSTestCleanup();
}

int secd_50_account(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
