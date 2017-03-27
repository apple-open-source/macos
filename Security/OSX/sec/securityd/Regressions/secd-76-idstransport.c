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
#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>

#include <securityd/SOSCloudCircleServer.h>
#include "SecdTestKeychainUtilities.h"
#include "SOSAccountTesting.h"
#include "SOSTransportTestTransports.h"
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainConstants.h>
#include "SOSTestDevice.h"

static int kTestTestCount = 92;

static void tests()
{
    CFErrorRef error = NULL;
        
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("ak"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("ak"));
    
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
    
    alice_account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSTestCreate(alice_account, CFSTR("Alice"), SOSCircleGetName(alice_account->trusted_circle), &error);
    bob_account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSTestCreate(bob_account, CFSTR("Bob"), SOSCircleGetName(bob_account->trusted_circle), &error);
    
    ok(alice_account->ids_message_transport != NULL, "Alice Account, Created IDS Test Transport");
    ok(bob_account->ids_message_transport != NULL, "Bob Account, Created IDS Test Transport");
    
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
    
    SOSTransportMessageIDSTestSetName(alice_account->ids_message_transport, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSTestGetName(alice_account->ids_message_transport) != NULL, "retrieved getting account name");
    ok(SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(alice_account, &error) != false, "device ID from KeychainSyncingOverIDSProxy");
    
    ok(SOSAccountSetMyDSID_wTxn(alice_account, CFSTR("DSID"),&error), "Setting IDS device ID");
    CFStringRef dsid = SOSAccountCopyDeviceID(alice_account, &error);
    ok(CFEqualSafe(dsid, CFSTR("DSID")), "Getting IDS device ID");
    CFReleaseNull(dsid);
    
    ok(SOSAccountStartPingTest(alice_account, CFSTR("hai there!"), &error), "Ping test");
    ok(CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges(alice_account->ids_message_transport)) != 0, "ping message made it to transport");
    SOSTransportMessageIDSTestClearChanges(alice_account->ids_message_transport);
    
    ok(SOSAccountSendIDSTestMessage(alice_account, CFSTR("hai again!"), &error), "Send Test Message");
    ok(CFDictionaryGetCount(SOSTransportMessageIDSTestGetChanges(alice_account->ids_message_transport)) != 0, "ping message made it to transport");

    CFStringRef dataKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyIDSDataMessage, kCFStringEncodingASCII);
    CFStringRef deviceIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyDeviceID, kCFStringEncodingASCII);
    CFStringRef sendersPeerIDKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeySendersPeerID, kCFStringEncodingASCII);

    //test IDS message handling
    CFMutableDictionaryRef messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending empty message dictionary");
    
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending device ID only");

    CFReleaseNull(messageDict);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending peer ID only");
    
    CFReleaseNull(messageDict);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending data only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending data and peerid only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending data and deviceid only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending peerid and deviceid only");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);
    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFDictionaryAddValue(messageDict, deviceIDKey, CFSTR("Alice Account"));
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, SOSPeerInfoGetPeerID(SOSAccountGetMyPeerInfo(bob_account)));
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending peerid and deviceid and data");
    
    CFReleaseNull(messageDict);
    CFReleaseNull(data);

    messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    data = CFDataCreate(kCFAllocatorDefault, 0, 0);
    CFDictionaryAddValue(messageDict, dataKey, data);
    CFStringRef BobDeviceID = SOSPeerInfoCopyDeviceID(SOSAccountGetMyPeerInfo(bob_account));
    CFDictionaryAddValue(messageDict, deviceIDKey, BobDeviceID);
    CFReleaseNull(BobDeviceID);
    CFDictionaryAddValue(messageDict, sendersPeerIDKey, CFSTR("Alice Account"));
    ok(SOSTransportMessageIDSHandleMessage(alice_account, messageDict, &error) == kHandleIDSMessageDontHandle, "sending peerid and deviceid and data");
    
    CFReleaseNull(data);
    CFReleaseNull(dataKey);
    CFReleaseNull(deviceIDKey);
    CFReleaseNull(sendersPeerIDKey);

    CFReleaseNull(alice_account);
    CFReleaseNull(bob_account);
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
