/*
 * Copyright (c) 2013-2016 Apple Inc. All Rights Reserved.
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
//  secd-200-logstate.c
//  sec
//

#include <stdio.h>




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
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

#define HOW_MANY_MINIONS 4

static int kTestTestCount = (5+(HOW_MANY_MINIONS+1)*20);


static bool SOSArrayForEachAccount(CFArrayRef accounts, bool (^operation)(SOSAccountRef account)) {
    __block bool retval = true;
    CFArrayForEach(accounts, ^(const void *value) {
        SOSAccountRef account = (SOSAccountRef) value;
        retval &= operation(account);
    });
    return retval;
}


static inline void FeedChangesToMasterMinions(CFMutableDictionaryRef changes, SOSAccountRef master_account, CFArrayRef minion_accounts) {
    FeedChangesTo(changes, master_account);
    SOSArrayForEachAccount(minion_accounts, ^bool(SOSAccountRef account) {
        FeedChangesTo(changes, account);
        return true;
    });
    FeedChangesTo(changes, master_account);

}


static inline bool ProcessChangesOnceMasterMinions(CFMutableDictionaryRef changes, SOSAccountRef master_account, CFArrayRef minion_accounts) {
    bool result = FillAllChanges(changes);
    FeedChangesToMasterMinions(changes, master_account, minion_accounts);
    return result;
}

static inline int ProcessChangesForMasterAndMinions(CFMutableDictionaryRef changes, SOSAccountRef master_account, CFArrayRef minion_accounts) {
    int result = 0;
    bool new_data = false;
    do {
        new_data = ProcessChangesOnceMasterMinions(changes, master_account, minion_accounts);
        ++result;
    } while (new_data);
    return result;
}

static bool MakeTheBigCircle(CFMutableDictionaryRef changes, SOSAccountRef master_account, CFArrayRef minion_accounts, CFErrorRef *error) {
    bool retval = SOSAccountResetToOffering_wTxn(master_account, error);
    require_quiet(retval, errOut);
    ProcessChangesForMasterAndMinions(changes, master_account, minion_accounts);
    retval = SOSArrayForEachAccount(minion_accounts, ^bool(SOSAccountRef account) {
        bool localret = SOSAccountJoinCircles_wTxn(account, error);
        ProcessChangesForMasterAndMinions(changes, master_account, minion_accounts);
        return localret;
    });
    require_quiet(retval, errOut);
    CFArrayRef applicants = SOSAccountCopyApplicants(master_account, error);
    retval = SOSAccountAcceptApplicants(master_account , applicants, error);
    ProcessChangesForMasterAndMinions(changes, master_account, minion_accounts);
errOut:
    return retval;
}


static CFArrayRef CreateManyAccountsForLocalChanges(CFStringRef namefmt, CFStringRef data_source_name, size_t howmany) {
    CFMutableArrayRef accounts = CFArrayCreateMutable(kCFAllocatorDefault, howmany, &kCFTypeArrayCallBacks);
    
    for(size_t i = 0; i < howmany; i++) {
        CFStringRef tmpname = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, namefmt, i);
        SOSAccountRef tmp = CreateAccountForLocalChanges(tmpname, CFSTR("TestSource"));
        CFArraySetValueAtIndex(accounts, i, tmp);
        CFReleaseNull(tmpname);
        CFReleaseNull(tmp);
    }
    return accounts;
}

static bool AssertAllCredentialsAndUpdate(CFMutableDictionaryRef changes, SOSAccountRef master_account, CFArrayRef minion_accounts, CFStringRef user_account, CFDataRef user_password, CFErrorRef *error) {
    __block bool retval = SOSAccountAssertUserCredentialsAndUpdate(master_account, user_account, user_password, error);
    ProcessChangesForMasterAndMinions(changes, master_account, minion_accounts);
    retval &= SOSArrayForEachAccount(minion_accounts, ^bool(SOSAccountRef account) {
        CFReleaseNull(*error);
        return SOSAccountAssertUserCredentialsAndUpdate(account, user_account, user_password, error);
    });
    CFReleaseNull(*error);

    return retval;
}

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountRef master_account = CreateAccountForLocalChanges(CFSTR("master"), CFSTR("TestSource"));
    CFArrayRef minion_accounts = CreateManyAccountsForLocalChanges(CFSTR("minion%d"), CFSTR("TestSource"), HOW_MANY_MINIONS);
    
    ok(AssertAllCredentialsAndUpdate(changes, master_account, minion_accounts, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    secLogEnable();
    SOSAccountLogState(master_account);
    secLogDisable();

    ok(MakeTheBigCircle(changes, master_account, minion_accounts, &error), "Get Everyone into the circle %@", error);
    
    diag("WHAT?");
    secLogEnable();
    SOSAccountLogState(master_account);
    SOSAccountLogViewState(master_account);
    secLogDisable();
    
    CFDataRef acctData = SOSAccountCopyEncodedData(master_account, kCFAllocatorDefault, &error);
    diag("Account DER Size is %d for %d peers", CFDataGetLength(acctData), HOW_MANY_MINIONS+1);
    CFReleaseNull(acctData);
    CFReleaseNull(error);
   
    CFDataRef circleData = SOSCircleCopyEncodedData(master_account->trusted_circle, kCFAllocatorDefault, &error);
    diag("Circle DER Size is %d for %d peers", CFDataGetLength(circleData), HOW_MANY_MINIONS+1);
    CFReleaseNull(circleData);
    CFReleaseNull(error);

    CFDataRef peerData = SOSPeerInfoCopyEncodedData(SOSAccountGetMyPeerInfo(master_account), kCFAllocatorDefault, &error);
    diag("Peer DER Size is %d", CFDataGetLength(peerData));
    CFReleaseNull(peerData);
    CFReleaseNull(error);

    CFReleaseNull(error);
    CFReleaseNull(master_account);
    CFReleaseNull(minion_accounts);
    
    SOSTestCleanup();
    
}

int secd_200_logstate(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
