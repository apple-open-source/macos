//
//  secd-155-otrnegotiationmonitor.m
//  secdtests_ios
//
//  Created by Michelle Auricchio on 6/5/17.
//

#import <Foundation/Foundation.h>
#include "keychain/SecureObjectSync/SOSAccount.h"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include <Security/SecureObjectSync/SOSViews.h>
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#include "keychain/SecureObjectSync/SOSTransportMessage.h"
#include "keychain/SecureObjectSync/SOSPeerOTRTimer.h"

#import "SOSAccountTesting.h"
#import "SOSTransportTestTransports.h"

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <Security/SecRecoveryKey.h>

#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>
#include "SecdTestKeychainUtilities.h"
#include "SOSAccountTesting.h"
#import "SOSTransportTestTransports.h"
#include "SOSTestDevice.h"
#include "SOSTestDataSource.h"
#include "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"


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

    ok(SOSAccountEnsurePeerRegistration(alice_account, NULL), "ensure peer registration - alice");

    ok(SOSAccountEnsurePeerRegistration(bob_account, NULL), "ensure peer registration - bob");

   // ids_test_sync(alice_account, bob_account);
}

int secd_155_otr_negotiation_monitor(int argc, char *const *argv)
{
    plan_tests(44);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
