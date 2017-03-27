//
//  secd-82-secproperties-basic.c
//  sec
//
//  Created by Richard Murphy on 4/16/15.
//
//

#include <stdio.h>
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


static void testSecurityProperties(SOSAccountRef account, SOSSecurityPropertyResultCode expected, CFStringRef property, SOSSecurityPropertyActionCode action, char *label) {
    CFErrorRef error = NULL;
    SOSSecurityPropertyResultCode pcode = 9999;
    switch(action) {
        case kSOSCCSecurityPropertyQuery:
            pcode = SOSAccountSecurityPropertyStatus(account, property, &error);
            break;
        case kSOSCCSecurityPropertyEnable:
        case kSOSCCSecurityPropertyDisable: // fallthrough
            pcode = SOSAccountUpdateSecurityProperty(account, property, action, &error);
            break;
        default:
            break;
    }
    ok((pcode == expected), "%s (%@)", label, error);
    CFReleaseNull(error);
}

static int kTestTestCount = 13;
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
    
    ok(SOSAccountJoinCircles_wTxn(account, &error), "Join Cirlce");
    
    ok(NULL != account, "Created");
    
    testSecurityProperties(account, kSOSCCSecurityPropertyNotValid, kSOSSecPropertyHasEntropy, kSOSCCSecurityPropertyQuery, "Expected no property: kSOSSecPropertyHasEntropy");
    testSecurityProperties(account, kSOSCCSecurityPropertyNotValid, kSOSSecPropertyScreenLock, kSOSCCSecurityPropertyQuery, "Expected no property: kSOSSecPropertyScreenLock");
    testSecurityProperties(account, kSOSCCSecurityPropertyNotValid, kSOSSecPropertySEP, kSOSCCSecurityPropertyQuery, "Expected no property: kSOSSecPropertySEP");
    testSecurityProperties(account, kSOSCCSecurityPropertyNotValid, kSOSSecPropertyIOS, kSOSCCSecurityPropertyQuery, "Expected no property: kSOSSecPropertyIOS");
    testSecurityProperties(account, kSOSCCSecurityPropertyValid, kSOSSecPropertyIOS, kSOSCCSecurityPropertyEnable, "Expected to add property: kSOSSecPropertyIOS");
    testSecurityProperties(account, kSOSCCSecurityPropertyValid, kSOSSecPropertyIOS, kSOSCCSecurityPropertyQuery, "Expected no property: kSOSSecPropertyIOS");
    testSecurityProperties(account, kSOSCCSecurityPropertyNotValid, kSOSSecPropertyIOS, kSOSCCSecurityPropertyDisable, "Expected to disable property: kSOSSecPropertyIOS");
    testSecurityProperties(account, kSOSCCSecurityPropertyNotValid, kSOSSecPropertyIOS, kSOSCCSecurityPropertyQuery, "Expected no property: kSOSSecPropertyIOS");
    testSecurityProperties(account, kSOSCCNoSuchSecurityProperty, CFSTR("FOO"), kSOSCCSecurityPropertyQuery, "Expected no such property for FOO");
    
    CFReleaseNull(account);
    
    SOSDataSourceRelease(test_source, NULL);
    SOSDataSourceFactoryRelease(test_factory);
    
    SOSTestCleanup();
}

int secd_82_secproperties_basic(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
