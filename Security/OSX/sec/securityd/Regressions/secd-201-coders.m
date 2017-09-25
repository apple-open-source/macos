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
//  secd_201_coders
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
#include <Security/SecureObjectSync/SOSEngine.h>
#import <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>
#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"
#include "SOSTestDevice.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static bool SOSAccountIsThisPeerIDMe(SOSAccount* account, CFStringRef peerID) {
    SOSAccountTrustClassic*trust = account.trust;
    SOSPeerInfoRef mypi = trust.peerInfo;
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(mypi);

    return myPeerID && CFEqualSafe(myPeerID, peerID);
}

static void compareCoders(CFMutableDictionaryRef beforeCoders, CFMutableDictionaryRef afterCoderState)
{
    CFDictionaryForEach(beforeCoders, ^(const void *key, const void *value) {
        CFStringRef beforePeerid = (CFStringRef)key;
        SOSCoderRef beforeCoderData = (SOSCoderRef)value;
        SOSCoderRef afterCoderData = (SOSCoderRef)CFDictionaryGetValue(afterCoderState, beforePeerid);
        ok(CFEqual(beforeCoderData,afterCoderData));
    });
}

static void ids_test_sync(SOSAccount* alice_account, SOSAccount* bob_account){

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    __block bool SyncingCompletedOverIDS = false;
    __block CFErrorRef localError = NULL;
    __block bool done = false;
    SOSAccountTrustClassic *aliceTrust = alice_account.trust;
    SOSAccountTrustClassic *bobTrust = bob_account.trust;

    do{
        SOSCircleForEachValidPeer(aliceTrust.trustedCircle, alice_account.accountKey, ^(SOSPeerInfoRef peer) {
            if (!SOSAccountIsThisPeerIDMe(alice_account, SOSPeerInfoGetPeerID(peer))) {
                if(SOSPeerInfoShouldUseIDSTransport(aliceTrust.peerInfo, peer) &&
                   SOSPeerInfoShouldUseIDSMessageFragmentation(aliceTrust.peerInfo, peer)){
                    secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                    CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                    CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                    CFTypeRef alice_engine = [(SOSMessageIDSTest*)alice_account.ids_message_transport SOSTransportMessageGetEngine];

                    //testing loading and saving coders
                    ok(TestSOSEngineGetCoders(alice_engine));
                    CFMutableDictionaryRef beforeCoders = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(TestSOSEngineGetCoders(alice_engine)), TestSOSEngineGetCoders(alice_engine));
                    TestSOSEngineDoTxnOnQueue(alice_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders((SOSEngineRef)[(SOSMessageIDSTest*)alice_account.ids_message_transport SOSTransportMessageGetEngine], txn, &localError));
                    });

                    ok(TestSOSEngineGetCoders(alice_engine));

                    TestSOSEngineDoTxnOnQueue(alice_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(SOSTestEngineSaveCoders(alice_engine, txn, &localError));
                    });

                    compareCoders(beforeCoders, TestSOSEngineGetCoders(alice_engine));

                    //syncing with all peers
                    SyncingCompletedOverIDS = [(SOSMessageIDSTest*)alice_account.ids_message_transport SOSTransportMessageSyncWithPeers:(SOSMessageIDSTest*)alice_account.ids_message_transport p:ids err:&localError];
                    //testing load after sync with all peers
                    CFMutableDictionaryRef codersAfterSyncBeforeLoad = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(TestSOSEngineGetCoders(alice_engine)), TestSOSEngineGetCoders(alice_engine));
                    TestSOSEngineDoTxnOnQueue(alice_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders((SOSEngineRef)[(SOSMessageIDSTest*)alice_account.ids_message_transport SOSTransportMessageGetEngine], txn, &localError));
                    });
                    compareCoders(codersAfterSyncBeforeLoad, TestSOSEngineGetCoders(alice_engine));

                    CFReleaseNull(codersAfterSyncBeforeLoad);
                    CFReleaseNull(beforeCoders);
                    CFReleaseNull(ids);
                }
            }
        });

        ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);

        SOSCircleForEachValidPeer(bobTrust.trustedCircle, bob_account.accountKey, ^(SOSPeerInfoRef peer) {
            if (!SOSAccountIsThisPeerIDMe(bob_account, SOSPeerInfoGetPeerID(peer))) {
                if(SOSPeerInfoShouldUseIDSTransport(bobTrust.peerInfo, peer) &&
                   SOSPeerInfoShouldUseIDSMessageFragmentation(bobTrust.peerInfo, peer)){
                    secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                    CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                    CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                    SOSEngineRef bob_engine = (SOSEngineRef)[(SOSMessageIDSTest*)bob_account.ids_message_transport SOSTransportMessageGetEngine];

                    //testing loading and saving coders
                    ok(TestSOSEngineGetCoders(bob_engine));
                    CFMutableDictionaryRef beforeCoders = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(TestSOSEngineGetCoders(bob_engine)), TestSOSEngineGetCoders(bob_engine));
                    TestSOSEngineDoTxnOnQueue(bob_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders((SOSEngineRef)[(SOSMessageIDSTest*)bob_account.ids_message_transport SOSTransportMessageGetEngine], txn, &localError));
                    });

                    ok((SOSEngineRef)TestSOSEngineGetCoders(bob_engine));

                    TestSOSEngineDoTxnOnQueue(bob_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(SOSTestEngineSaveCoders(bob_engine, txn, &localError));
                    });

                    compareCoders(beforeCoders, TestSOSEngineGetCoders(bob_engine));

                    SyncingCompletedOverIDS &= [(SOSMessageIDSTest*)bob_account.ids_message_transport SOSTransportMessageSyncWithPeers:(SOSMessageIDSTest*)bob_account.ids_message_transport p:ids err:&localError];

                    //testing load after sync with all peers
                    CFMutableDictionaryRef codersAfterSyncBeforeLoad = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(TestSOSEngineGetCoders(bob_engine)), TestSOSEngineGetCoders(bob_engine));
                    TestSOSEngineDoTxnOnQueue(bob_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders((SOSEngineRef)[(SOSMessageIDSTest*)bob_account.ids_message_transport SOSTransportMessageGetEngine], txn, &localError));
                    });
                    compareCoders(codersAfterSyncBeforeLoad, TestSOSEngineGetCoders(bob_engine));
                    CFReleaseNull(codersAfterSyncBeforeLoad);
                    CFReleaseNull(beforeCoders);
                    CFReleaseNull(ids);
                }
            }
        });
        if(!SyncingCompletedOverIDS)
            return;

        if(CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges((SOSMessageIDSTest*)alice_account.ids_message_transport)) == 0 && CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges((SOSMessageIDSTest*)bob_account.ids_message_transport)) == 0){
            done = true;
            break;
        }

        ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);

    }while(done == false);
    CFReleaseNull(changes);

    ok(SyncingCompletedOverIDS, "synced items over IDS");

}

static void tests(void)
{

    __block CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFDataRef cfwrong_password = CFDataCreate(NULL, (uint8_t *) "NotFooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));

    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
    CFReleaseNull(cfpassword);

    CFReleaseNull(error);
    ok(!SOSAccountTryUserCredentials(alice_account, cfaccount, cfwrong_password, &error), "Credential failing (%@)", error);
    CFReleaseNull(cfwrong_password);
    is(error ? CFErrorGetCode(error) : 0, kSOSErrorWrongPassword, "Expected SOSErrorWrongPassword");
    CFReleaseNull(error);

    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    ok(SOSAccountHasCompletedInitialSync(alice_account), "Alice thinks she's completed initial sync");

    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);

        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicant %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Alice accepts (%@)", error);
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");

    accounts_agree("bob&alice pair", bob_account, alice_account);

    CFArrayRef peers = SOSAccountCopyPeers(alice_account, &error);
    ok(peers && CFArrayGetCount(peers) == 2, "See two peers %@ (%@)", peers, error);
    CFReleaseNull(peers);

    //creating test devices
    CFIndex version = 0;

    // Optionally prefix each peer with name to make them more unique.
    CFArrayRef deviceIDs = CFArrayCreateForCFTypes(kCFAllocatorDefault,alice_account.peerID, bob_account.peerID, NULL);
    CFSetRef views = SOSViewsCopyTestV2Default();
    CFMutableArrayRef peerMetas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFStringRef deviceID;
    CFArrayForEachC(deviceIDs, deviceID) {
        SOSPeerMetaRef peerMeta = SOSPeerMetaCreateWithComponents(deviceID, views, NULL);
        CFArrayAppendValue(peerMetas, peerMeta);
        CFReleaseNull(peerMeta);
    }

    CFReleaseNull(views);
    CFArrayForEachC(deviceIDs, deviceID) {
        SOSTestDeviceRef device = SOSTestDeviceCreateWithDbNamed(kCFAllocatorDefault, deviceID, deviceID);
        SOSTestDeviceSetPeerIDs(device, peerMetas, version, NULL);

        if([alice_account.peerID isEqual: (__bridge id) deviceID]){
            alice_account.factory = device->dsf;
            SOSTestDeviceAddGenericItem(device, CFSTR("Alice"), CFSTR("Alice-add"));
        }
        else{
            bob_account.factory = device->dsf;
            SOSTestDeviceAddGenericItem(device, CFSTR("Bob"), CFSTR("Bob-add"));
        }

        CFReleaseNull(device);
    }
    CFReleaseNull(deviceIDs);
    CFReleaseNull(peerMetas);

    SOSUnregisterAllTransportMessages();
    CFArrayRemoveAllValues(message_transports);

    SOSAccountTrustClassic *aliceTrust = alice_account.trust;
    SOSAccountTrustClassic *bobTrust = bob_account.trust;

    alice_account.ids_message_transport = (SOSMessageIDS*)[[SOSMessageIDSTest alloc] initWithAccount:alice_account andAccountName:CFSTR("Alice") andCircleName:CFSTR("TestSource") err:&error];

    bob_account.ids_message_transport = (SOSMessageIDS*)[[SOSMessageIDSTest alloc] initWithAccount:bob_account andAccountName:CFSTR("Bob") andCircleName:CFSTR("TestSource") err:&error];

    bool result = [alice_account.trust modifyCircle:alice_account.circle_transport err:&error action:^(SOSCircleRef circle) {

        CFErrorRef localError = NULL;

        SOSFullPeerInfoUpdateTransportType(aliceTrust.fullPeerInfo, SOSTransportMessageTypeIDSV2, &localError);
        SOSFullPeerInfoUpdateTransportPreference(aliceTrust.fullPeerInfo, kCFBooleanFalse, &localError);
        SOSFullPeerInfoUpdateTransportFragmentationPreference(aliceTrust.fullPeerInfo, kCFBooleanTrue, &localError);
        SOSFullPeerInfoUpdateTransportAckModelPreference(aliceTrust.fullPeerInfo, kCFBooleanTrue, &localError);

        return SOSCircleHasPeer(circle, aliceTrust.peerInfo, NULL);
    }];

    ok(result, "Alice account update circle with transport type");

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    result = [bob_account.trust modifyCircle:bob_account.circle_transport err:&error action:^(SOSCircleRef circle) {
        CFErrorRef localError = NULL;

        SOSFullPeerInfoUpdateTransportType(bobTrust.fullPeerInfo, SOSTransportMessageTypeIDSV2, &localError);
        SOSFullPeerInfoUpdateTransportPreference(bobTrust.fullPeerInfo, kCFBooleanFalse, &localError);
        SOSFullPeerInfoUpdateTransportFragmentationPreference(bobTrust.fullPeerInfo, kCFBooleanTrue, &localError);
        SOSFullPeerInfoUpdateTransportAckModelPreference(bobTrust.fullPeerInfo, kCFBooleanTrue, &localError);

        return SOSCircleHasPeer(circle, bobTrust.peerInfo, NULL);
    }];

    ok(result, "Bob account update circle with transport type");
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    CFStringRef alice_transportType =SOSPeerInfoCopyTransportType(alice_account.peerInfo);
    CFStringRef bob_accountTransportType = SOSPeerInfoCopyTransportType(bob_account.peerInfo);
    ok(CFEqualSafe(alice_transportType, CFSTR("IDS2.0")), "Alice transport type not IDS");
    ok(CFEqualSafe(bob_accountTransportType, CFSTR("IDS2.0")), "Bob transport type not IDS");

    CFReleaseNull(alice_transportType);
    CFReleaseNull(bob_accountTransportType);

    SOSTransportMessageIDSTestSetName((SOSMessageIDSTest*)alice_account.ids_message_transport, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSTestGetName((SOSMessageIDSTest*)alice_account.ids_message_transport) != NULL, "retrieved getting account name");
    ok(SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(alice_account, &error) != false, "device ID from KeychainSyncingOverIDSProxy");

    SOSTransportMessageIDSTestSetName((SOSMessageIDSTest*)bob_account.ids_message_transport, CFSTR("Bob Account"));
    ok(SOSTransportMessageIDSTestGetName((SOSMessageIDSTest*)bob_account.ids_message_transport) != NULL, "retrieved getting account name");
    ok(SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(bob_account, &error) != false, "device ID from KeychainSyncingOverIDSProxy");


    ok(SOSAccountSetMyDSID_wTxn(alice_account, CFSTR("Alice"),&error), "Setting IDS device ID");
    CFStringRef alice_dsid = SOSAccountCopyDeviceID(alice_account, &error);
    ok(CFEqualSafe(alice_dsid, CFSTR("Alice")), "Getting IDS device ID");
    CFReleaseNull(alice_dsid);

    ok(SOSAccountSetMyDSID_wTxn(bob_account, CFSTR("Bob"),&error), "Setting IDS device ID");
    CFStringRef bob_dsid = SOSAccountCopyDeviceID(bob_account, &error);
    ok(CFEqualSafe(bob_dsid, CFSTR("Bob")), "Getting IDS device ID");
    CFReleaseNull(bob_dsid);

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");

    ok(SOSAccountEnsurePeerRegistration(alice_account, NULL), "ensure peer registration - alice");
    ok(SOSAccountEnsurePeerRegistration(bob_account, NULL), "ensure peer registration - bob");

    ids_test_sync(alice_account, bob_account);

    alice_account = nil;
    bob_account = nil;
    
    SOSTestCleanup();


    CFReleaseNull(changes);
}

int secd_201_coders(int argc, char *const *argv)
{
    plan_tests(166);

    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
