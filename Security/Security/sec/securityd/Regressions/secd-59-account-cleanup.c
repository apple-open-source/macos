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

#include <CoreFoundation/CFDictionary.h>

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <SecureObjectSync/SOSTransport.h>

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

static int kTestTestCount = 136;

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    CFStringRef circle_name = CFSTR("TestSource");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), circle_name);
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), circle_name);
    SOSAccountRef carole_account = CreateAccountForLocalChanges(CFSTR("Carole"), circle_name);
    
    ok(SOSAccountAssertUserCredentials(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountAssertUserCredentials(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountResetToOffering(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    ok(SOSAccountJoinCircles(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 3, "updates");

    accounts_agree("bob&alice pair", bob_account, alice_account);
    
    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);
    
    SOSFullPeerInfoRef fpiAlice = SOSAccountGetMyFullPeerInCircleNamedIfPresent(alice_account, circle_name, NULL);
    CFStringRef alice_id = CFStringCreateCopy(NULL, SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(fpiAlice)));
    
    ok(SOSAccountLeaveCircles(alice_account, &error), "Alice Leaves (%@)", error);
    CFReleaseNull(error);
    CFReleaseNull(cfpassword);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 3, "updates");

    accounts_agree("Alice bails", bob_account, alice_account);
    accounts_agree("Alice bails", bob_account, carole_account);
    
    SOSAccountCleanupRetirementTickets(bob_account, 0, &error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    //is(CFDictionaryGetCountOfValue(BobChanges, kCFNull),0, "0 Keys Nulled Out");
    
    ok(SOSAccountJoinCircles(carole_account, &error), "Carole Applies (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(bob_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(bob_account, applicants, &error), "Bob accepts Carole (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    
    // Bob should not yet cleanup Alice's retirment here on his own since it hasn't been long enough
    // by default.
    //is(CFDictionaryGetCountOfValue(BobChanges, kCFNull),0, "0 Keys Nulled Out");

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 3, "updates");

    accounts_agree("Carole joins", bob_account, carole_account);
    
    SOSAccountCleanupRetirementTickets(bob_account, 0, &error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    is(countPeers(bob_account), 2, "Active peers after forced cleanup");
    is(countActivePeers(bob_account), 3, "Inactive peers after forced cleanup");
    
    //is(CFDictionaryGetCountOfValue(BobChanges, kCFNull), 1, "1 Keys Nulled Out");
    
//    CFDictionaryForEach(BobChanges, ^(const void *key, const void *value) {
//        if(isNull(value)) {
//            CFStringRef circle_name = NULL, retiree = NULL;
//            SOSKVSKeyType keytype = SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, &retiree, NULL);
//            is(keytype, kRetirementKey, "Expect only a retirement key");
//            ok(CFEqualSafe(alice_id, retiree), "Alice (%@) is retiree (%@)", alice_id, retiree);
//            CFReleaseNull(circle_name);
//            CFReleaseNull(retiree);
//        }
//    });

    CFReleaseNull(alice_id);
    CFReleaseNull(carole_account);
    CFReleaseNull(bob_account);
    CFReleaseNull(alice_account);
    
    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
    
}

int secd_59_account_cleanup(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();
    
    return 0;
}
