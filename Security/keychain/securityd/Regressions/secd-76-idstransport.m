//
//  secd-76-idstransport.c
//  sec
//
//

/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#include <stdio.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h>
#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>

#include <securityd/SOSCloudCircleServer.h>
#include "SecdTestKeychainUtilities.h"
#import "SOSAccountTesting.h"
#import "SOSTransportTestTransports.h"
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainConstants.h>
#include "SOSTestDevice.h"



static int kTestTestCount = 73;

static void tests()
{
    CFErrorRef error = NULL;
        
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    SOSAccount* alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("ak"));
    SOSAccount* bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("ak"));
    SOSAccountTrustClassic *aliceTrust = alice_account.trust;
    SOSAccountTrustClassic *bobTrust = bob_account.trust;

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
        
        if(CFEqualSafe(deviceID, (__bridge CFTypeRef)(alice_account.peerID))){
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

    alice_account.ids_message_transport =  (SOSMessageIDS*)[[SOSMessageIDSTest alloc] initWithAccount:alice_account andAccountName:CFSTR("Alice") andCircleName:SOSCircleGetName(aliceTrust.trustedCircle) err:&error];


    bob_account.ids_message_transport = (SOSMessageIDS*)[[SOSMessageIDSTest alloc] initWithAccount:bob_account andAccountName:CFSTR("Bob") andCircleName:SOSCircleGetName(bobTrust.trustedCircle) err:&error];
    ok(alice_account.ids_message_transport != NULL, "Alice Account, Created IDS Test Transport");
    ok(bob_account.ids_message_transport != NULL, "Bob Account, Created IDS Test Transport");
    
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
    
    result &= [bob_account.trust modifyCircle:bob_account.circle_transport err:&error action:^(SOSCircleRef circle) {
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
    
    ok(SOSAccountSetMyDSID_wTxn(bob_account, CFSTR("Bob"),&error), "Setting IDS device ID");
    CFStringRef bob_dsid = SOSAccountCopyDeviceID(bob_account, &error);
    ok(CFEqualSafe(bob_dsid, CFSTR("Bob")), "Getting IDS device ID");
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");
    
    SOSTransportMessageIDSTestSetName((SOSMessageIDSTest*)alice_account.ids_message_transport, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSTestGetName((SOSMessageIDSTest*)alice_account.ids_message_transport) != NULL, "retrieved getting account name");
    ok(SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(alice_account, &error) != false, "device ID from KeychainSyncingOverIDSProxy");
    
    ok(SOSAccountSetMyDSID_wTxn(alice_account, CFSTR("DSID"),&error), "Setting IDS device ID");
    CFStringRef dsid = SOSAccountCopyDeviceID(alice_account, &error);
    ok(CFEqualSafe(dsid, CFSTR("DSID")), "Getting IDS device ID");
    CFReleaseNull(dsid);
    
    ok(SOSAccountStartPingTest(alice_account, CFSTR("hai there!"), &error), "Ping test");
    ok(CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges((SOSMessageIDSTest*)alice_account.ids_message_transport)) != 0, "ping message made it to transport");
    SOSTransportMessageIDSTestClearChanges((SOSMessageIDSTest*)alice_account.ids_message_transport);
    
    ok(SOSAccountSendIDSTestMessage(alice_account, CFSTR("hai again!"), &error), "Send Test Message");
    ok(CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges((SOSMessageIDSTest*)alice_account.ids_message_transport)) != 0, "ping message made it to transport");

    CFStringRef dataKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyIDSDataMessage, kCFStringEncodingASCII);
    CFStringRef deviceIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyDeviceID, kCFStringEncodingASCII);
    CFStringRef sendersPeerIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeySendersPeerID, kCFStringEncodingASCII);

    //test IDS message handling
    CFMutableDictionaryRef messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    ok([alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error]
       == kHandleIDSMessageDontHandle, "sending empty message dictionary");
    
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    ok([alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error]  == kHandleIDSMessageDontHandle, "sending device ID only");

    CFReleaseNull(messageDict);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok([alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error]   == kHandleIDSMessageDontHandle, "sending peer ID only");

    CFReleaseNull(messageDict);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    ok( [alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error] == kHandleIDSMessageDontHandle, "sending data only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok([(SOSMessageIDS*)alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error]== kHandleIDSMessageDontHandle, "sending data and peerid only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    ok([(SOSMessageIDS*)alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error] == kHandleIDSMessageDontHandle, "sending data and deviceid only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok([(SOSMessageIDS*)alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error] == kHandleIDSMessageDontHandle, "sending peerid and deviceid only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, SOSPeerInfoGetPeerID(bob_account.peerInfo));
    ok([(SOSMessageIDS*)alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error]== kHandleIDSMessageDontHandle, "sending peerid and deviceid and data");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);

    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFStringRef BobDeviceID = SOSPeerInfoCopyDeviceID(bob_account.peerInfo);
    CFDictionaryAddValue(messageDict, deviceIDKey, BobDeviceID);
    CFReleaseNull(BobDeviceID);
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok([(SOSMessageIDS*)alice_account.ids_message_transport  SOSTransportMessageIDSHandleMessage:alice_account m:messageDict err:&error]== kHandleIDSMessageDontHandle, "sending peerid and deviceid and data");
    
    CFReleaseNull(data);
    CFReleaseNull(dataKey);
    CFReleaseNull(deviceIDKey);
    CFReleaseNull(sendersPeerIDKey);

    CFReleaseNull(alice_dsid);
    CFReleaseNull(bob_dsid);
    CFReleaseNull(changes);

    SOSTestCleanup();
}
int secd_76_idstransport(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    
    tests();
    
    return 0;
}
