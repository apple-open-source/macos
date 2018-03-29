//
//  secd-155-otrnegotiationmonitor.m
//  secdtests_ios
//
//  Created by Michelle Auricchio on 6/5/17.
//

#import <Foundation/Foundation.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSAccountTrustClassic+Expansion.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSPeerOTRTimer.h>

#import "SOSAccountTesting.h"
#import "SOSTransportTestTransports.h"

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include "SecRecoveryKey.h"

#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>
#include "SecdTestKeychainUtilities.h"
#include "SOSAccountTesting.h"
#import "SOSTransportTestTransports.h"
#include "SOSTestDevice.h"
#include "SOSTestDataSource.h"
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>
static int kTestTestCount = 114;

static bool SOSAccountIsThisPeerIDMe(SOSAccount* account, CFStringRef peerID) {
    SOSAccountTrustClassic* trust = account.trust;
    SOSPeerInfoRef mypi = trust.peerInfo;
    ok(mypi);
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(mypi);
    ok(myPeerID);
    return myPeerID && CFEqualSafe(myPeerID, peerID);
}

__unused static void ids_test_sync(SOSAccount* alice_account, SOSAccount* bob_account){

    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    __block bool SyncingCompletedOverIDS = false;
    __block CFErrorRef localError = NULL;

    SOSAccountTrustClassic* aliceTrust = alice_account.trust;
    SOSAccountTrustClassic* bobTrust = bob_account.trust;
    SOSDataSourceRef bob_ds = SOSDataSourceFactoryCreateDataSource(bob_account.factory, CFSTR("ak"), NULL);
    SOSEngineRef bob_engine = bob_ds ? SOSDataSourceGetSharedEngine(bob_ds, NULL) : (SOSEngineRef) NULL;
    SOSDataSourceRef alice_ds = SOSDataSourceFactoryCreateDataSource(alice_account.factory, CFSTR("ak"), NULL);
    SOSEngineRef alice_engine = alice_ds ? SOSDataSourceGetSharedEngine(alice_ds, NULL) : (SOSEngineRef) NULL;

    /* new routines to test */
    __block NSString *bobID = bob_account.peerID;
    __block NSString *aliceID = alice_account.peerID;


    ///////////////////////////
    //second exchange//
    ///////////////////////////

    SOSCircleForEachValidPeer(aliceTrust.trustedCircle, alice_account.accountKey, ^(SOSPeerInfoRef peer) {
        if (!SOSAccountIsThisPeerIDMe(alice_account, SOSPeerInfoGetPeerID(peer))) {
            if(SOSPeerInfoShouldUseIDSTransport(aliceTrust.peerInfo, peer) &&
               SOSPeerInfoShouldUseIDSMessageFragmentation(aliceTrust.peerInfo, peer)){
                secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                SyncingCompletedOverIDS = [alice_account.ids_message_transport SOSTransportMessageSyncWithPeers:alice_account.ids_message_transport p:ids err:&localError];
                CFReleaseNull(ids);
            }
        }
    });

    SOSEngineWithPeerID(alice_engine, (__bridge CFStringRef)bobID, NULL, ^(SOSPeerRef bob_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(alice_account , bobID));
        ok(false == SOSPeerTimerForPeerExist(bob_peer));
        ok(false == SOSPeerOTRTimerHaveAnRTTAvailable(alice_account, bobID));
        ok(true == SOSCoderIsCoderInAwaitingState(coder));
    });

    ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);

    SOSCircleForEachValidPeer(bobTrust.trustedCircle, bob_account.accountKey, ^(SOSPeerInfoRef peer) {
        if (!SOSAccountIsThisPeerIDMe(bob_account, SOSPeerInfoGetPeerID(peer))) {
            if(SOSPeerInfoShouldUseIDSTransport(bobTrust.peerInfo, peer) &&
               SOSPeerInfoShouldUseIDSMessageFragmentation(bobTrust.peerInfo, peer)){
                secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                SyncingCompletedOverIDS = [(SOSMessageIDSTest*)bob_account.ids_message_transport SOSTransportMessageSyncWithPeers:(SOSMessageIDSTest*)bob_account.ids_message_transport p:ids err:&localError];
                CFReleaseNull(ids);
            }
        }
    });

    ok(SyncingCompletedOverIDS, "synced items over IDS");

    SOSEngineWithPeerID(bob_engine, (__bridge CFStringRef)aliceID, NULL, ^(SOSPeerRef alice_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(bob_account , aliceID));
        ok(false == SOSPeerTimerForPeerExist(alice_peer));
        ok(false == SOSPeerOTRTimerHaveAnRTTAvailable(bob_account, aliceID));
    });

    ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);

    SOSEngineWithPeerID(alice_engine, (__bridge CFStringRef)bobID, NULL, ^(SOSPeerRef bob_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(alice_account , bobID));
        ok(false == SOSPeerTimerForPeerExist(bob_peer));
        ok(false == SOSPeerOTRTimerHaveAnRTTAvailable(alice_account, bobID));
        ok(true == SOSCoderIsCoderInAwaitingState(coder));
    });

    ///////////////////////////
    //second sync exchange //
    ///////////////////////////

    SOSCircleForEachValidPeer(aliceTrust.trustedCircle, alice_account.accountKey, ^(SOSPeerInfoRef peer) {
        if (!SOSAccountIsThisPeerIDMe(alice_account, SOSPeerInfoGetPeerID(peer))) {
            if(SOSPeerInfoShouldUseIDSTransport(aliceTrust.peerInfo, peer) &&
               SOSPeerInfoShouldUseIDSMessageFragmentation(aliceTrust.peerInfo, peer)){
                secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                SyncingCompletedOverIDS = [alice_account.ids_message_transport SOSTransportMessageSyncWithPeers:alice_account.ids_message_transport p:ids err:&localError];
                CFReleaseNull(ids);
            }
        }
    });

    SOSEngineWithPeerID(alice_engine, (__bridge CFStringRef)bobID, NULL, ^(SOSPeerRef bob_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(alice_account , bobID));
        ok(false == SOSPeerTimerForPeerExist(bob_peer));
        ok(false == SOSPeerOTRTimerHaveAnRTTAvailable(alice_account, bobID));
        ok(true == SOSCoderIsCoderInAwaitingState(coder));
    });

    ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);

    SOSCircleForEachValidPeer(bobTrust.trustedCircle, bob_account.accountKey, ^(SOSPeerInfoRef peer) {
        if (!SOSAccountIsThisPeerIDMe(bob_account, SOSPeerInfoGetPeerID(peer))) {
            if(SOSPeerInfoShouldUseIDSTransport(bobTrust.peerInfo, peer) &&
               SOSPeerInfoShouldUseIDSMessageFragmentation(bobTrust.peerInfo, peer)){
                secnotice("IDS Transport","Syncing with IDS capable peers using IDS!");

                CFMutableSetRef ids = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                CFSetAddValue(ids, SOSPeerInfoGetPeerID(peer));

                SyncingCompletedOverIDS = [(SOSMessageIDSTest*)bob_account.ids_message_transport SOSTransportMessageSyncWithPeers:(SOSMessageIDSTest*)bob_account.ids_message_transport p:ids err:&localError];
                CFReleaseNull(ids);
            }
        }
    });

    ok(SyncingCompletedOverIDS, "synced items over IDS");

    SOSEngineWithPeerID(bob_engine, (__bridge CFStringRef)aliceID, NULL, ^(SOSPeerRef alice_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(bob_account , aliceID));
        ok(false == SOSPeerTimerForPeerExist(alice_peer));
        ok(false == SOSPeerOTRTimerHaveAnRTTAvailable(bob_account, aliceID));
    });

    ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL);

    SOSEngineWithPeerID(alice_engine, (__bridge CFStringRef)bobID, NULL, ^(SOSPeerRef bob_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(alice_account , bobID));
        ok(false == SOSPeerTimerForPeerExist(bob_peer));
        ok(true == SOSPeerOTRTimerHaveAnRTTAvailable(alice_account, bobID));
        ok(false == SOSCoderIsCoderInAwaitingState(coder));
    });

    CFReleaseNull(changes);
}


static void tests(void)
{
    CFErrorRef error = NULL;
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("ak"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("ak"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(cfpassword);
    CFReleaseNull(error);
    
    ok(NULL != alice_account, "Alice Created");
    ok(NULL != bob_account, "Bob Created");
    
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    
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
    
    SOSDataSourceRef alice_ds = SOSDataSourceFactoryCreateDataSource(alice_account.factory, CFSTR("ak"), NULL);
    SOSEngineRef alice_engine = alice_ds ? SOSDataSourceGetSharedEngine(alice_ds, NULL) : (SOSEngineRef) NULL;
    
    ok(false == SOSEngineHandleCodedMessage(alice_account, alice_engine, (__bridge CFStringRef)bob_account.peerID, NULL, NULL));
    
    SOSDataSourceRef bob_ds = SOSDataSourceFactoryCreateDataSource(bob_account.factory, CFSTR("ak"), NULL);
    SOSEngineRef bob_engine = bob_ds ? SOSDataSourceGetSharedEngine(bob_ds, NULL) : (SOSEngineRef) NULL;
    
    ok(false == SOSEngineHandleCodedMessage(bob_account, bob_engine, (__bridge CFStringRef)alice_account.peerID, NULL, NULL));
    
    /* new routines to test */
    __block NSString *bobID = bob_account.peerID;
    __block NSString *aliceID = alice_account.peerID;
    
    SOSEngineWithPeerID(bob_engine, (__bridge CFStringRef)aliceID, NULL, ^(SOSPeerRef alice_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(bob_account , aliceID));
        ok(false == SOSPeerTimerForPeerExist(alice_peer));
        ok(false == SOSPeerOTRTimerHaveAnRTTAvailable(bob_account, aliceID));
    });
    
    SOSEngineWithPeerID(alice_engine, (__bridge CFStringRef)bobID, NULL, ^(SOSPeerRef bob_peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok(false == SOSPeerOTRTimerHaveReachedMaxRetryAllowance(alice_account , bobID));
        ok(false == SOSPeerTimerForPeerExist(bob_peer));
        ok(false == SOSPeerOTRTimerHaveAnRTTAvailable(alice_account, bobID));
    });

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

        if([alice_account.peerID isEqual: (__bridge id)(deviceID)]){
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

    SOSAccountTrustClassic* aliceTrust = alice_account.trust;
    SOSAccountTrustClassic* bobTrust = bob_account.trust;

    alice_account.ids_message_transport = (SOSMessageIDS*)[[SOSMessageIDSTest alloc] initWithAccount:alice_account andAccountName:CFSTR("Alice") andCircleName:SOSCircleGetName(aliceTrust.trustedCircle) err:&error ];

    bob_account.ids_message_transport = (SOSMessageIDS*)[[SOSMessageIDSTest alloc] initWithAccount:bob_account andAccountName:CFSTR("Bob") andCircleName:SOSCircleGetName(bobTrust.trustedCircle) err:&error];

    ok(alice_account.ids_message_transport != NULL, "Alice Account, Created IDS Test Transport");
    ok(bob_account.ids_message_transport != NULL, "Bob Account, Created IDS Test Transport");

    bool result = [alice_account.trust modifyCircle:alice_account.circle_transport err:&error action:^bool(SOSCircleRef circle) {
        CFErrorRef localError = NULL;

        SOSFullPeerInfoUpdateTransportType(aliceTrust.fullPeerInfo, SOSTransportMessageTypeIDSV2, &localError);
        SOSFullPeerInfoUpdateTransportPreference(aliceTrust.fullPeerInfo, kCFBooleanFalse, &localError);
        SOSFullPeerInfoUpdateTransportFragmentationPreference(aliceTrust.fullPeerInfo, kCFBooleanTrue, &localError);
        SOSFullPeerInfoUpdateTransportAckModelPreference(aliceTrust.fullPeerInfo, kCFBooleanTrue, &localError);

        return SOSCircleHasPeer(circle, aliceTrust.peerInfo, NULL);
    }];

    ok(result, "Alice account update circle with transport type");

    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    result = [bob_account.trust modifyCircle:bob_account.circle_transport err:&error action:^bool(SOSCircleRef circle) {
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

   // ids_test_sync(alice_account, bob_account);
}

int secd_155_otr_negotiation_monitor(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
