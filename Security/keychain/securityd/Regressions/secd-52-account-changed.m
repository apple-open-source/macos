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
#include "SOSAccountTesting.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>

#include "keychain/securityd/SOSCloudCircleServer.h"

#include "SecdTestKeychainUtilities.h"

#if SOS_ENABLED
static SOSAccount* accountWithDSID(CFStringRef cfaccount, char *passwordString, CFStringRef cfdsid, CFStringRef deviceName, CFMutableDictionaryRef kvsAccount) {
    CFDataRef password = CFDataCreate(NULL, (uint8_t *) passwordString, strlen(passwordString));
    SOSAccount* deviceAccount = CreateAccountForLocalChanges(CFSTR("iPhone1"), cfaccount);
    if(cfdsid) {
        SOSAccountAssertDSID(deviceAccount, cfdsid);
    }
    ok(SOSTestStartCircleWithAccount(deviceAccount, kvsAccount, cfaccount, password), "Have device start a circle");
    CFReleaseNull(password);
    return deviceAccount;
}

static SOSAccount* accountAssertsDSID(CFStringRef cfaccount, char *passwordString, CFStringRef cfdsid, CFStringRef deviceName, CFMutableDictionaryRef kvsAccount) {
    SOSAccount* deviceAccount = CreateAccountForLocalChanges(CFSTR("iPhone1"), cfaccount);
    if(cfdsid) {
        SOSAccountAssertDSID(deviceAccount, cfdsid);
    }
    return deviceAccount;
}

static bool changeDSID(CFStringRef fromDSID, CFStringRef toDSID) {
    CFMutableDictionaryRef fakeKVS = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* theAccount = accountWithDSID(CFSTR("test1@test.org"), "TestPassword1", fromDSID, CFSTR("TestPhone1"), fakeKVS);
    bool retval = SOSAccountAssertDSID(theAccount, toDSID);
    CFReleaseNull(fakeKVS);
    SOSTestCleanup();
    return retval;
}

static bool changeDSIDfromKVS(CFStringRef fromDSID, CFStringRef toDSID) {
    CFMutableDictionaryRef fakeKVS = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* theAccount = accountWithDSID(CFSTR("test1@test.org"), "TestPassword1", fromDSID, CFSTR("TestPhone1"), fakeKVS);
    accountAssertsDSID(CFSTR("test1@test.org"), "TestPassword1", toDSID, CFSTR("TestPhone2"), fakeKVS);
    ProcessChangesOnce(fakeKVS, theAccount, NULL); // only do this once. the fake KVS gets whacked by the potential account change.
    bool retval = ![theAccount isInCircle: NULL]; // if we're still in circle we didn't reset
    CFReleaseNull(fakeKVS);
    SOSTestCleanup();
    return retval;
}

static void tests(void) {
    __security_simulatecrash_enable(false);
    ok(!changeDSID(NULL, CFSTR("TEST1")), "Going from NULL to value(DSID) does not reset the account");
    ok(!changeDSID(NULL, NULL), "Going from NULL to NULL does not reset the account");
    ok(!changeDSID(CFSTR("TEST1"), NULL), "Going from value(DSID) to NULL does not reset the account");
    ok(!changeDSID(CFSTR("TEST1"), CFSTR("TEST1")), "Going from value(DSID) to same value(DSID) does not reset the account");
    ok(changeDSID(CFSTR("TEST1"), CFSTR("TEST2")), "Going from value(DSID) to different value(DSID) does reset the account");
    
    ok(!changeDSIDfromKVS(NULL, CFSTR("TEST1")), "Going from NULL to value(DSID) does not reset the account");
    ok(!changeDSIDfromKVS(CFSTR("TEST1"), CFSTR("TEST1")), "Going from value(DSID) to same value(DSID) does not reset the account");
    ok(changeDSIDfromKVS(CFSTR("TEST1"), CFSTR("TEST2")), "Going from value(DSID) to different value(DSID) does reset the account");
    __security_simulatecrash_enable(true);
}
#endif

int secd_52_account_changed(int argc, char *const *argv)
{
#if SOS_ENABLED
    secLogDisable();
    plan_tests(49);
    enableSOSCompatibilityForTests();

    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    tests();
    secd_test_teardown_delete_temp_keychain(__FUNCTION__);
#else
    plan_tests(0);
#endif
    return 0;
}
