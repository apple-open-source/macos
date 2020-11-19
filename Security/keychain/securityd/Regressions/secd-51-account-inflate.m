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

#include "keychain/SecureObjectSync/SOSAccount.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>

#include "keychain/securityd/SOSCloudCircleServer.h"

#include "SecdTestKeychainUtilities.h"
#include "SOSAccountTesting.h"
#include "SOSTransportTestTransports.h"

#if SOS_ENABLED

#define kCompatibilityTestCount 0
#if 0
static void test_v6(void) {
    SOSDataSourceFactoryRef ak_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(ak_factory, CFSTR("ak"), test_source);
    CFErrorRef error = NULL;

    const uint8_t *der_p = v6_der;
    
    SOSAccount* convertedAccount = SOSAccountCreateFromDER(kCFAllocatorDefault, ak_factory, &error, &der_p, v6_der + sizeof(v6_der));
    
    ok(convertedAccount, "inflate v6 account (%@)", error);
    CFReleaseSafe(error);
    
    is(kSOSCCInCircle, [convertedAccount.trust getCircleStatus:&error], "in the circle");
    
    CFReleaseSafe(convertedAccount);
    ak_factory->release(ak_factory);
    SOSDataSourceRelease(test_source, NULL);
}
#endif

static int kTestTestCount = 11 + kSecdTestSetupTestCount;
static void tests(void)
{

    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(test_factory, CFSTR("TestType"), test_source);
    
    SOSAccount* account = CreateAccountForLocalChanges(CFSTR("Test Device"), CFSTR("TestType"));
    ok(SOSAccountAssertUserCredentialsAndUpdate(account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    ok(NULL != account, "Created");
    
    ok(SOSAccountResetToOffering_wTxn(account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    ok(testAccountPersistence(account), "Test Account->DER->Account Equivalence");

    SOSTestCleanup();
    
    test_factory->release(test_factory);
    SOSDataSourceRelease(test_source, NULL);
}

#endif

int secd_51_account_inflate(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(kTestTestCount + kCompatibilityTestCount);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    secd_test_clear_testviews();
    tests();
#else
    plan_tests(0);
#endif

    return 0;
}
