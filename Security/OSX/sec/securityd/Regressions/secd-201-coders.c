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
#include <Security/SecureObjectSync/SOSEnginePriv.h>

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

static int kTestTestCount = 124;

static void TestSOSEngineDoOnQueue(SOSEngineRef engine, dispatch_block_t action)
{
    dispatch_sync(engine->queue, action);
}

static bool SOSAccountIsThisPeerIDMe(SOSAccountRef account, CFStringRef peerID) {
    SOSPeerInfoRef mypi = SOSFullPeerInfoGetPeerInfo(account->my_identity);
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(mypi);

    return myPeerID && CFEqualSafe(myPeerID, peerID);
}

static bool TestSOSEngineDoTxnOnQueue(SOSEngineRef engine, CFErrorRef *error, void(^transaction)(SOSTransactionRef txn, bool *commit))
{
    return SOSDataSourceWithCommitQueue(engine->dataSource, error, ^(SOSTransactionRef txn, bool *commit) {
        TestSOSEngineDoOnQueue(engine, ^{ transaction(txn, commit); });
    });
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

static void ids_test_sync(SOSAccountRef alice_account, SOSAccountRef bob_account){

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    __block bool SyncingCompletedOverIDS = false;
    __block CFErrorRef localError = NULL;
    __block bool done = false;
    do{
        SOSCircleForEachValidPeer(alice_account->trusted_circle, alice_account->user_public, ^(SOSPeerInfoRef peer) {
            if (!SOSAccountIsThisPeerIDMe(alice_account, SOSPeerInfoGetPeerID(peer))) {
                if(SOSPeerInfoShouldUseIDSTransport(SOSFullPeerInfoGetPeerInfo(alice_account->my_identity), peer) &&
                   SOSPeerInfoShouldUseIDSMessageFragmentation(SOSFullPeerInfoGetPeerInfo(alice_account->my_identity), peer)){
                    secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                    CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                    CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                    SOSEngineRef alice_engine = SOSTransportMessageGetEngine(alice_account->ids_message_transport);

                    //testing loading and saving coders
                    ok(alice_engine->coders);
                    CFMutableDictionaryRef beforeCoders = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(alice_engine->coders), alice_engine->coders);
                    TestSOSEngineDoTxnOnQueue(alice_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders(SOSTransportMessageGetEngine(alice_account->ids_message_transport), txn, &localError));
                    });

                    ok(alice_engine->coders);

                    TestSOSEngineDoTxnOnQueue(alice_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(SOSTestEngineSaveCoders(alice_engine, txn, &localError));
                    });

                    compareCoders(beforeCoders, alice_engine->coders);

                    //syncing with all peers
                    SyncingCompletedOverIDS = SOSTransportMessageSyncWithPeers(alice_account->ids_message_transport, ids, &localError);

                    //testing load after sync with all peers
                    CFMutableDictionaryRef codersAfterSyncBeforeLoad = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(alice_engine->coders), alice_engine->coders);
                    TestSOSEngineDoTxnOnQueue(alice_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders(SOSTransportMessageGetEngine(alice_account->ids_message_transport), txn, &localError));
                    });
                    compareCoders(codersAfterSyncBeforeLoad, alice_engine->coders);

                    CFReleaseNull(codersAfterSyncBeforeLoad);
                    CFReleaseNull(beforeCoders);
                    CFReleaseNull(ids);
                }
            }
        });

        ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);

        SOSCircleForEachValidPeer(bob_account->trusted_circle, bob_account->user_public, ^(SOSPeerInfoRef peer) {
            if (!SOSAccountIsThisPeerIDMe(bob_account, SOSPeerInfoGetPeerID(peer))) {
                if(SOSPeerInfoShouldUseIDSTransport(SOSFullPeerInfoGetPeerInfo(bob_account->my_identity), peer) &&
                   SOSPeerInfoShouldUseIDSMessageFragmentation(SOSFullPeerInfoGetPeerInfo(bob_account->my_identity), peer)){
                    secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                    CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                    CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                    SOSEngineRef bob_engine = SOSTransportMessageGetEngine(bob_account->ids_message_transport);

                    //testing loading and saving coders
                    ok(bob_engine->coders);
                    CFMutableDictionaryRef beforeCoders = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(bob_engine->coders), bob_engine->coders);
                    TestSOSEngineDoTxnOnQueue(bob_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders(SOSTransportMessageGetEngine(bob_account->ids_message_transport), txn, &localError));
                    });

                    ok(bob_engine->coders);

                    TestSOSEngineDoTxnOnQueue(bob_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(SOSTestEngineSaveCoders(bob_engine, txn, &localError));
                    });

                    compareCoders(beforeCoders, bob_engine->coders);

                    SyncingCompletedOverIDS &= SOSTransportMessageSyncWithPeers(bob_account->ids_message_transport, ids, &localError);

                    //testing load after sync with all peers
                    CFMutableDictionaryRef codersAfterSyncBeforeLoad = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(bob_engine->coders), bob_engine->coders);
                    TestSOSEngineDoTxnOnQueue(bob_engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
                        ok(TestSOSEngineLoadCoders(SOSTransportMessageGetEngine(bob_account->ids_message_transport), txn, &localError));
                    });
                    compareCoders(codersAfterSyncBeforeLoad, bob_engine->coders);
                    CFReleaseNull(codersAfterSyncBeforeLoad);
                    CFReleaseNull(beforeCoders);
                    CFReleaseNull(ids);
                }
            }
        });

        if(CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges(alice_account->ids_message_transport)) == 0 && CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges(bob_account->ids_message_transport)) == 0){
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
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));

    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);

    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");

    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    ok(SOSAccountTryUserCredentials(alice_account, cfaccount, cfpassword, &error), "Credential trying (%@)", error);
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
    CFArrayRef deviceIDs = CFArrayCreateForCFTypes(kCFAllocatorDefault,SOSAccountGetMyPeerID(alice_account), SOSAccountGetMyPeerID(bob_account), NULL);
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

        if(CFEqualSafe(deviceID, SOSAccountGetMyPeerID(alice_account))){
            alice_account->factory = device->dsf;
            SOSTestDeviceAddGenericItem(device, CFSTR("Alice"), CFSTR("Alice-add"));
        }
        else{
            bob_account->factory = device->dsf;
            SOSTestDeviceAddGenericItem(device, CFSTR("Bob"), CFSTR("Bob-add"));
        }

        CFReleaseNull(device);
    }
    CFReleaseNull(deviceIDs);
    CFReleaseNull(peerMetas);

    SOSUnregisterAllTransportMessages();
    CFArrayRemoveAllValues(message_transports);
    
    alice_account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSTestCreate(alice_account, CFSTR("Alice"), CFSTR("TestSource"), &error);
    bob_account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSTestCreate(bob_account, CFSTR("Bob"), CFSTR("TestSource"), &error);

    bool result = SOSAccountModifyCircle(alice_account, &error, ^bool(SOSCircleRef circle) {
        CFErrorRef localError = NULL;

        SOSFullPeerInfoUpdateTransportType(alice_account->my_identity, SOSTransportMessageTypeIDSV2, &localError);
        SOSFullPeerInfoUpdateTransportPreference(alice_account->my_identity, kCFBooleanFalse, &localError);
        SOSFullPeerInfoUpdateTransportFragmentationPreference(alice_account->my_identity, kCFBooleanTrue, &localError);
        SOSFullPeerInfoUpdateTransportAckModelPreference(alice_account->my_identity, kCFBooleanTrue, &localError);

        return SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(alice_account->my_identity), NULL);
    });

    ok(result, "Alice account update circle with transport type");

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    result = SOSAccountModifyCircle(bob_account, &error, ^bool(SOSCircleRef circle) {
        CFErrorRef localError = NULL;

        SOSFullPeerInfoUpdateTransportType(bob_account->my_identity, SOSTransportMessageTypeIDSV2, &localError);
        SOSFullPeerInfoUpdateTransportPreference(bob_account->my_identity, kCFBooleanFalse, &localError);
        SOSFullPeerInfoUpdateTransportFragmentationPreference(bob_account->my_identity, kCFBooleanTrue, &localError);
        SOSFullPeerInfoUpdateTransportAckModelPreference(bob_account->my_identity, kCFBooleanTrue, &localError);

        return SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(bob_account->my_identity), NULL);
    });

    ok(result, "Bob account update circle with transport type");
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    CFStringRef alice_transportType =SOSPeerInfoCopyTransportType(SOSAccountGetMyPeerInfo(alice_account));
    CFStringRef bob_accountTransportType = SOSPeerInfoCopyTransportType(SOSAccountGetMyPeerInfo(bob_account));
    ok(CFEqualSafe(alice_transportType, CFSTR("IDS2.0")), "Alice transport type not IDS");
    ok(CFEqualSafe(bob_accountTransportType, CFSTR("IDS2.0")), "Bob transport type not IDS");

    CFReleaseNull(alice_transportType);
    CFReleaseNull(bob_accountTransportType);

    SOSTransportMessageIDSTestSetName(alice_account->ids_message_transport, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSTestGetName(alice_account->ids_message_transport) != NULL, "retrieved getting account name");
    ok(SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(alice_account, &error) != false, "device ID from KeychainSyncingOverIDSProxy");

    SOSTransportMessageIDSTestSetName(bob_account->ids_message_transport, CFSTR("Bob Account"));
    ok(SOSTransportMessageIDSTestGetName(bob_account->ids_message_transport) != NULL, "retrieved getting account name");
    ok(SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(bob_account, &error) != false, "device ID from KeychainSyncingOverIDSProxy");


    ok(SOSAccountSetMyDSID_wTxn(alice_account, CFSTR("Alice"),&error), "Setting IDS device ID");
    CFStringRef alice_dsid = SOSAccountCopyDeviceID(alice_account, &error);
    ok(CFEqualSafe(alice_dsid, CFSTR("Alice")), "Getting IDS device ID");

    ok(SOSAccountSetMyDSID_wTxn(bob_account, CFSTR("Bob"),&error), "Setting IDS device ID");
    CFStringRef bob_dsid = SOSAccountCopyDeviceID(bob_account, &error);
    ok(CFEqualSafe(bob_dsid, CFSTR("Bob")), "Getting IDS device ID");

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");

    ok(SOSAccountEnsurePeerRegistration(alice_account, NULL), "ensure peer registration - alice");
    ok(SOSAccountEnsurePeerRegistration(bob_account, NULL), "ensure peer registration - bob");

    ids_test_sync(alice_account, bob_account);

    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
    CFReleaseNull(alice_account);
    CFReleaseNull(bob_account);
    CFReleaseNull(changes);
}

int secd_201_coders(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
