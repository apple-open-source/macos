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

#include "keychain/SecureObjectSync/SOSAccount.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"

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

static int kTestTestCount = 111;

#if FIX_ICLOUD_IDENTITY_AS_SET_CRED_SIDE_EFFECT

static bool purgeICloudIdentity(SOSAccount* account) {
    bool retval = false;
    SOSFullPeerInfoRef icfpi = SOSCircleCopyiCloudFullPeerInfoRef([account.trust getCircle:NULL], NULL);
    if(!icfpi) return false;
    retval = SOSFullPeerInfoPurgePersistentKey(icfpi, NULL);
    CFReleaseNull(icfpi);
    return retval;
}

#endif

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
    
    /*----- normal join after restore -----*/
    
    ok(SOSAccountJoinCirclesAfterRestore_wTxn(carole_account, &error), "Carole cloud identity joins (%@)", error);
    CFReleaseNull(error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 4, "updates");

    is(countApplicants(alice_account), 0, "See no applicants");
    
    is(countPeers(carole_account), 3, "Carole sees 3 valid peers after sliding in");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
    
    ok([carole_account.trust leaveCircle:carole_account err:&error], "Carol Leaves again");
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    /*----- join - join after restore -----*/
    
    ok(SOSAccountJoinCircles_wTxn(carole_account, &error), "Carole normally joins (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");

    is(countApplicants(alice_account), 1, "See one applicant");
    
    ok(SOSAccountJoinCirclesAfterRestore_wTxn(carole_account, &error), "Carole cloud identity joins (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 4, "updates");


    is(countApplicants(alice_account), 0, "See no applicants");
    
    is(countPeers(carole_account), 3, "Carole sees 3 valid peers after sliding in");

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);

#if FIX_ICLOUD_IDENTITY_AS_SET_CRED_SIDE_EFFECT
    /* Break iCloud identity FPI in all peers */
    
    ok(purgeICloudIdentity(alice_account), "remove iCloud private key");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");

    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 4, "updates");
    
    ok([carole_account.trust leaveCircle:carole_account err:&error], "Carol Leaves again");
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");
    
    /*----- join - join after restore -----*/
    
    ok(SOSAccountJoinCircles_wTxn(carole_account, &error), "Carole normally joins (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 2, "updates");
    
    is(countApplicants(alice_account), 1, "See one applicant");
    
    ok(SOSAccountJoinCirclesAfterRestore_wTxn(carole_account, &error), "Carole cloud identity joins (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 4, "updates");
    
    
    is(countApplicants(alice_account), 0, "See no applicants");

    is(countPeers(carole_account), 3, "Carole sees 3 valid peers after sliding in");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, carole_account, NULL), 1, "updates");
    
    accounts_agree_internal("Carole's in", bob_account, alice_account, false);
    accounts_agree_internal("Carole's in - 2", bob_account, carole_account, false);
#endif
    //join after piggybacking the icloud identity??
    CFMutableArrayRef identityArray = SOSAccountCopyiCloudIdentities(alice_account);

    NSMutableArray *encodedIdenities = [NSMutableArray array];
    CFIndex i, count = CFArrayGetCount(identityArray);
    for (i = 0; i < count; i++) {
        SOSPeerInfoRef fpi = (SOSPeerInfoRef)CFArrayGetValueAtIndex(identityArray, i);
        NSData *data = CFBridgingRelease(SOSPeerInfoCopyData(fpi, NULL));
        if (data)
            [encodedIdenities addObject:data];
    }

    //store in keychain as the piggy icloud
    [encodedIdenities enumerateObjectsUsingBlock:^(NSData *v_data, NSUInteger idx, BOOL *stop) {

        NSMutableDictionary* query = [@{
                                        (id)kSecClass : (id)kSecClassKey,
                                        (id)kSecUseDataProtectionKeychain : @YES,
                                        (id)kSecAttrAccessGroup: @"com.apple.security.sos",
                                        (id)kSecAttrLabel : @"Cloud Identity - piggy",
                                        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                        (id)kSecUseTombstones : (id)kCFBooleanTrue,
                                        (id)kSecValueData : v_data,
                                        } mutableCopy];

        OSStatus status = SecItemAdd((__bridge CFDictionaryRef) query, NULL);

        if(status == errSecDuplicateItem) {
            // Sure, okay, fine, we'll update.
            NSMutableDictionary* update = [@{
                                             (id)kSecValueData: v_data,
                                             } mutableCopy];
            query[(id)kSecValueData] = nil;

            status = SecItemUpdate((__bridge CFDictionaryRef) query, (__bridge CFDictionaryRef)update);
            ok(status == 0, "Grabbed icloud identity from the keychain %@", error);
        }
    }];


    //now grab this grom the keychain
    NSMutableDictionary* query2 = [@{
                                    (id)kSecClass : (id)kSecClassKey,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccessGroup: @"com.apple.security.sos",
                                    (id)kSecAttrLabel : @"Cloud Identity - piggy",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecUseTombstones : (id)kCFBooleanTrue,
                                    (id)kSecReturnData : (id)kCFBooleanTrue,
                                    } mutableCopy];
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef) query2, &result);
    ok(status == 0, "Grabbed icloud identity from the keychain %@", error);
    ok(result != NULL, "result from sec item copy matching query %@", error);

    NSDictionary *keyAttributes = @{
                                    (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
                                    (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
                                    };


    SecKeyRef privKey = SecKeyCreateWithData(result, (__bridge CFDictionaryRef)keyAttributes, NULL);

    ok(privKey != NULL, "Private key is NULL");
    SecKeyRef publicKey = SecKeyCreatePublicFromPrivate(privKey);
    ok(publicKey != NULL, "Private key is NULL");

    CFDataRef public_key_hash = SecKeyCopyPublicKeyHash(publicKey);
    ok(public_key_hash != NULL, "hash is not null");

    CFReleaseNull(publicKey);

    SOSAccount* margaret_account = CreateAccountForLocalChanges(CFSTR("margaret"), CFSTR("TestSource"));

    ok(SOSAccountAssertUserCredentialsAndUpdate(margaret_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    ok(SOSAccountJoinCirclesAfterRestore_wTxn(margaret_account, &error), "Carole cloud identity joins (%@)", error);

    CFReleaseNull(identityArray);
    CFReleaseNull(changes);
    CFReleaseNull(error);
    CFReleaseNull(public_key_hash);
    CFReleaseNull(cfpassword);
    CFReleaseNull(privKey);

    alice_account = nil;
    bob_account = nil;
    SOSTestCleanup();
}

int secd_60_account_cloud_identity(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
