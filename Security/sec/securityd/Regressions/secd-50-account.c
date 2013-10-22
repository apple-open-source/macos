/*
 *  secd_50_account.c
 *
 *  Created by Mitch Adler on 1/25/121.
 *  Copyright 2012 Apple Inc. All rights reserved.
 *
 */


#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 10 + kSecdTestSetupTestCount;
static void tests(void)
{
    secd_test_setup_temp_keychain("secd_50_account", ^{
    });

    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    SOSAccountKeyInterestBlock interest_block = ^(bool getNewKeysOnly, CFArrayRef alwaysKeys, CFArrayRef afterFirstUnlockKeys, CFArrayRef unlockedKeys) {};
    SOSAccountDataUpdateBlock update_block = ^ bool (CFDictionaryRef keys, CFErrorRef *error) {return true;};

    SOSDataSourceFactoryRef test_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef test_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactoryAddDataSource(test_factory, CFSTR("TestType"), test_source);

    CFDictionaryRef gestalt = SOSCreatePeerGestaltFromName(CFSTR("Test Device"));
    SOSAccountRef account = SOSAccountCreate(kCFAllocatorDefault, gestalt, test_factory, interest_block, update_block);
    ok(SOSAccountAssertUserCredentials(account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);

    ok(NULL != account, "Created");

    ok(1 == SOSAccountCountCircles(account), "Has one circle");

    size_t size = SOSAccountGetDEREncodedSize(account, &error);
    CFReleaseNull(error);
    uint8_t buffer[size];
    uint8_t* start = SOSAccountEncodeToDER(account, &error, buffer, buffer + sizeof(buffer));
    CFReleaseNull(error);

    ok(start, "successful encoding");
    ok(start == buffer, "Used whole buffer");

    const uint8_t *der = buffer;
    SOSAccountRef inflated = SOSAccountCreateFromDER(kCFAllocatorDefault, test_factory, interest_block, update_block,
                                                     &error, &der, buffer + sizeof(buffer));

    ok(inflated, "inflated");
    ok(CFEqual(inflated, account), "Compares");

    CFDictionaryRef new_gestalt = SOSCreatePeerGestaltFromName(CFSTR("New Device"));

    ok(SOSAccountResetToOffering(account, &error), "Reset to Offering  (%@)", error);
    CFReleaseNull(error);
    
    is(SOSAccountIsInCircles(account, &error), kSOSCCInCircle, "Was in Circle  (%@)", error);
    CFReleaseNull(error);

    SOSAccountUpdateGestalt(account, new_gestalt);

    is(SOSAccountIsInCircles(account, &error), kSOSCCInCircle, "Still in Circle  (%@)", error);
    CFReleaseNull(error);

    CFReleaseNull(gestalt);
    CFReleaseNull(new_gestalt);
    CFReleaseNull(account);
}

int secd_50_account(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
	
    tests();

	return 0;
}
