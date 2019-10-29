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

//
//  secd60-account-cloud-exposure.c
//  sec
//



#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <CoreFoundation/CFDictionary.h>

#include "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Identity.h"

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include "keychain/securityd/SOSCloudCircleServer.h"

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static bool SOSAccountResetCircleToNastyOffering(SOSAccount* account, SecKeyRef userPriv, SOSPeerInfoRef pi, CFErrorRef *error) {
    bool result = false;
    SecKeyRef userPub = SecKeyCreatePublicFromPrivate(userPriv);
    SOSAccountTrustClassic *trust = account.trust;
    if(!SOSAccountHasCircle(account, error)){
        CFReleaseNull(userPub);
        return result;
    }
    if(![account.trust ensureFullPeerAvailable:(__bridge CFDictionaryRef)(account.gestalt) deviceID:(__bridge CFStringRef)(account.deviceID) backupKey:(__bridge CFDataRef)(account.backup_key) err:error]){
        CFReleaseNull(userPub);
        return result;
    }
    (void) [account.trust resetAllRings:account err:error];
    
    [account.trust modifyCircle:account.circle_transport err:error action:^(SOSCircleRef circle) {
        bool result = false;
        CFErrorRef localError = NULL;
        SOSFullPeerInfoRef iCloudfpi = NULL;
        
        //sleep(10);
        require_quiet(SOSCircleResetToEmpty(circle, error), err_out);
        require_quiet([account.trust addiCloudIdentity:circle key:userPriv err:error], err_out);
        require_quiet(iCloudfpi = SOSCircleCopyiCloudFullPeerInfoRef(circle, error), err_out);
        
        /* Add the defenders peerInfo to circle */
        require_quiet(SOSCircleRequestReadmission(circle, userPub, pi, error), err_out);
        require_quiet(SOSCircleAcceptRequest(circle, userPriv, iCloudfpi, pi, error), err_out);
        
        [trust setDepartureCode:kSOSNeverLeftCircle];
        result = true;
        SOSCircleRef copiedCircle = SOSCircleCopyCircle(kCFAllocatorDefault, circle, error); // I don't think this copy is necessary, but...
        [trust setTrustedCircle:copiedCircle];
        CFReleaseNull(copiedCircle);
        SOSAccountPublishCloudParameters(account, NULL);
        trust.fullPeerInfo = nil;

    err_out:
        if (result == false) {
            secerror("error resetting circle (%@) to offering: %@", circle, localError);
        }
        if (localError && error && *error == NULL) {
            *error = localError;
            localError = NULL;
        }

        CFReleaseNull(iCloudfpi);
        CFReleaseNull(localError);
        return result;
    }];
    
    result = true;
    
    return result;
}

static bool SOSAccountResetToNastyOffering(SOSAccount* account, SOSPeerInfoRef pi, CFErrorRef* error) {
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;

    SOSAccountTrustClassic* trust = account.trust;
    trust.fullPeerInfo = nil;

    return user_key && SOSAccountResetCircleToNastyOffering(account, user_key, pi, error);
}

static bool performiCloudIdentityAttack(SOSAccount* attacker, SOSAccount* defender, SOSAccount* accomplice, CFMutableDictionaryRef changes) {
    CFErrorRef error = NULL;
    bool retval = false;  // false means the attack succeeds
    CFArrayRef applicants = NULL;
    SOSAccountTrustClassic* defenderTrust = defender.trust;

    /*----- Carole makes bogus circle with fake iCloud identity and Alice's peerInfo but only signed with fake iCloud identity -----*/
    
    require_action_quiet(SOSAccountResetToNastyOffering(attacker, defenderTrust.peerInfo, &error), testDone, retval = true);
    CFReleaseNull(error);
    
    ProcessChangesUntilNoChange(changes, defender, accomplice, attacker, NULL);
    
    /*----- Now use our fake iCloud identity to get in to the circle for real -----*/
    require_action_quiet(SOSAccountJoinCirclesAfterRestore_wTxn(attacker, &error), testDone, retval = true);
    CFReleaseNull(error);
    require_action_quiet(countPeers(attacker) == 2, testDone, retval = true);
    
    /*----- Let's see if carole can get bob into the circle and have alice believe it -----*/
    require_action_quiet(SOSAccountJoinCircles_wTxn(accomplice, &error), testDone, retval = true);
    CFReleaseNull(error);
    
    ProcessChangesUntilNoChange(changes, defender, accomplice, attacker, NULL);
    
    applicants = SOSAccountCopyApplicants(attacker, &error);
    CFReleaseNull(error);
    
    if(CFArrayGetCount(applicants) > 0) {
        require_action_quiet(SOSAccountAcceptApplicants(attacker, applicants, &error), testDone, retval = true);
    }
    
    ProcessChangesUntilNoChange(changes, defender, accomplice, attacker, NULL);
    
    require_action_quiet(countPeers(defender) == 3, testDone, retval = true);
    require_action_quiet(countPeers(accomplice) == 3, testDone, retval = true);
    require_action_quiet(countPeers(attacker) == 3, testDone, retval = true);

testDone:
    CFReleaseNull(applicants);
    CFReleaseNull(error);
    return retval;
}

static void tests(void)
{
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccount* alice_account = CreateAccountForLocalChanges( CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    SOSAccount* carole_account = CreateAccountForLocalChanges(CFSTR("Carole"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(carole_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");
    
    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
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

    
    ok(performiCloudIdentityAttack(carole_account, alice_account, bob_account, changes), "Attack is defeated");
    
    CFReleaseNull(cfpassword);
    alice_account = nil;
    bob_account = nil;
    SOSTestCleanup();
}

int secd_60_account_cloud_exposure(int argc, char *const *argv)
{
    plan_tests(41);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
