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

#include "keychain/SecureObjectSync/SOSAccount.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
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
    NSError* error = nil;
    CFErrorRef cfError = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactorySetDataSource(test_factory, CFSTR("TestType"), test_source);
    
    SOSAccount* account = CreateAccountForLocalChanges(CFSTR("Test Device"),CFSTR("TestType") );
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(account, cfaccount, cfpassword, &cfError), "Credential setting (%@)", cfError);
    CFReleaseNull(cfError);
    CFReleaseNull(cfpassword);
    
    ok(NULL != account, "Created");

    size_t size = [account.trust getDEREncodedSize:account err:&error];

    error = nil;
    uint8_t buffer[size];

    uint8_t* start = [account.trust encodeToDER:account err:&error start:buffer end:buffer + sizeof(buffer)];
    error = nil;
    
    ok(start, "successful encoding");
    ok(start == buffer, "Used whole buffer");
    
    const uint8_t *der = buffer;
    SOSAccount* inflated = [SOSAccount accountFromDER:&der end:buffer + sizeof(buffer)
                                              factory:test_factory error:&error];

    ok(inflated, "inflated %@", error);
    ok([inflated isEqual:account], "Compares");

    error = nil;

    CFDictionaryRef new_gestalt = SOSCreatePeerGestaltFromName(CFSTR("New Device"));
    ok(SOSAccountResetToOffering_wTxn(account, &cfError), "Reset to Offering  (%@)", error);
    CFReleaseNull(cfError);
    
    is([account getCircleStatus:&cfError], kSOSCCInCircle, "Was in Circle  (%@)", error);
    CFReleaseNull(cfError);
    
    [account.trust updateGestalt:account newGestalt:new_gestalt];
    is([account getCircleStatus:&cfError], kSOSCCInCircle, "Still in Circle  (%@)", error);
    CFReleaseNull(cfError);
    
    CFReleaseNull(new_gestalt);

    SOSDataSourceFactoryRelease(test_factory);
    SOSDataSourceRelease(test_source, NULL);

    account = nil;
    inflated = nil;
    
    SOSTestCleanup();
}

int secd_50_account(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
