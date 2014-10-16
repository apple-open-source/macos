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


#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>

#include "SOSCircle_regressions.h"

#include <corecrypto/ccsha2.h>

#include <utilities/SecCFWrappers.h>

#include <stdint.h>

#include <AssertMacros.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CoreFoundation/CFDate.h>

#include <utilities/SecCFWrappers.h>

#include <Security/SecKey.h>

#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>
#include <SecureObjectSync/SOSPeerCoder.h>
#include <SecureObjectSync/SOSTransportMessageKVS.h>
#include <SecureObjectSync/SOSTransportMessage.h>

#include <SecureObjectSync/SOSAccountPriv.h>

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"
#include "SOSTestDataSource.h"

#include <securityd/Regressions/SOSAccountTesting.h>
#include <securityd/Regressions/SOSTransportTestTransports.h>

#ifndef SEC_CONST_DECL
#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));
#endif

#include <securityd/SOSCloudCircleServer.h>


// MARK: ----- Constants -----

static CFStringRef circleKey = CFSTR("Circle");

static int kTestTestCount = 68;

static bool withEngine(SOSCircleRef circle, SOSDataSourceFactoryRef factory, bool readOnly, CFErrorRef *error, bool (^action)(SOSEngineRef engine, CFErrorRef *block_error)) {
    bool success = false;
    SOSDataSourceRef ds = NULL;
    SOSEngineRef engine = NULL;
    
    ds = factory->create_datasource(factory, SOSCircleGetName(circle), error);
    require_quiet(ds, exit);
    
    engine = SOSEngineCreate(ds, error); // Hand off DS to engine.
    ds = NULL;
    require_quiet(engine, exit);
    
    success = action(engine, error);
    
exit:
    if (engine)
        SOSEngineDispose(engine);
    
    return success;
}

static bool SOSCircleSyncWithPeer(SOSAccountRef account ,SOSFullPeerInfoRef myRef, SOSCircleRef circle, SOSDataSourceFactoryRef factory,
                           SOSPeerInfoRef peerInfo, CFErrorRef *error)
{
    return withEngine(circle, factory, true, error, ^bool(SOSEngineRef engine, CFErrorRef *block_error) {
        SOSPeerRef peer = SOSPeerCreate(engine, peerInfo, block_error);
        if (!peer) return false;
        
        CFMutableDictionaryRef circleToPeerIDs = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableArrayRef peer_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFArrayAppendValue(peer_ids, SOSPeerGetID(peer));
        CFDictionaryAddValue(circleToPeerIDs, SOSCircleGetName(circle), peer_ids);
       
        SOSTransportMessageRef transport = (SOSTransportMessageRef)CFDictionaryGetValue(SOSAccountGetMessageTransports(account), SOSCircleGetName(circle));
        
        bool result = SOSTransportMessageSyncWithPeers(transport, circleToPeerIDs, error);
        CFReleaseSafe(peer);
        return result;
    });
}

static bool SOSCircleHandlePeerMessage(SOSAccountRef account, SOSCircleRef circle, SOSFullPeerInfoRef myRef, SOSDataSourceFactoryRef factory,
                            SOSPeerInfoRef peerInfo, CFDataRef message, CFErrorRef *error) {
    
    return withEngine(circle, factory, true, error, ^bool(SOSEngineRef engine, CFErrorRef *block_error) {
        CFDataRef decodedMessage = NULL;
        SOSPeerRef peer = SOSPeerCreate(engine, peerInfo, block_error);
        if (!peer) return false;
        CFDictionaryRef message_transports = (CFDictionaryRef)SOSAccountGetMessageTransports(account);
        SOSTransportMessageRef transport = (SOSTransportMessageRef)CFDictionaryGetValue(message_transports, SOSCircleGetName(circle));
        bool result = SOSTransportMessageHandlePeerMessage(transport, SOSPeerGetID(peer), message, error);
        
        CFReleaseSafe(peer);
        CFReleaseNull(decodedMessage);
        return result;
    });
}



static void tests()
{
    CFErrorRef error = NULL;
    
    CFStringRef aliceID = CFSTR("Alice");
    CFStringRef bobID = CFSTR("Bob");    // not really remote, just another client on same machine
    
    SecKeyRef alice_key = NULL;
    SecKeyRef bob_key = NULL;
    
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    
    CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
    ok(parameters, "No parameters!");
    ok(error == NULL, "Error: (%@)", error);
    CFReleaseNull(error);
    
    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, &error);
    CFReleaseNull(parameters);
    CFReleaseSafe(cfpassword);
    
    CFStringRef circleName = CFSTR("Woot Circle");
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), circleName);
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), circleName);
    
    
    SOSFullPeerInfoRef alice_full_peer_info = SOSCreateFullPeerInfoFromName(aliceID, &alice_key, &error);
    SOSPeerInfoRef alice_peer_info = SOSFullPeerInfoGetPeerInfo(alice_full_peer_info);
    
    SOSFullPeerInfoRef bob_full_peer_info = SOSCreateFullPeerInfoFromName(bobID, &bob_key, &error);
    SOSPeerInfoRef bob_peer_info = SOSFullPeerInfoGetPeerInfo(bob_full_peer_info);
    
    SOSCircleRef aliceCircle = SOSCircleCreate(kCFAllocatorDefault, circleName, &error);
    
    ok(SOSCircleRequestAdmission(aliceCircle, user_privkey, alice_full_peer_info, &error));
    ok(SOSCircleAcceptRequests(aliceCircle, user_privkey, alice_full_peer_info, NULL));
    ok(SOSCircleRequestAdmission(aliceCircle, user_privkey, bob_full_peer_info, &error), "requested admission");
    ok(SOSCircleAcceptRequests(aliceCircle, user_privkey, bob_full_peer_info, &error), "accepted them all!");
    
    alice_peer_info = SOSFullPeerInfoGetPeerInfo(alice_full_peer_info);
    bob_peer_info = SOSFullPeerInfoGetPeerInfo(bob_full_peer_info);
    
    CFDataRef aliceCircleEncoded;
    ok(aliceCircleEncoded = SOSCircleCopyEncodedData(aliceCircle, kCFAllocatorDefault, &error), "encode alice circle: %@", error);
    CFReleaseNull(error);
    SOSCircleRef bobCircle;
    ok(bobCircle = SOSCircleCreateFromData(0, aliceCircleEncoded, &error), "decode bobCircle: %@", error);
    CFReleaseNull(aliceCircleEncoded);
    CFReleaseNull(error);
    
    SOSTransportMessageRef alice_message_transport = (SOSTransportMessageRef)CFDictionaryGetValue(SOSAccountGetMessageTransports(alice_account), circleName);
    SOSTransportMessageRef bob_message_transport = (SOSTransportMessageRef)CFDictionaryGetValue(bob_account->message_transports, circleName);
    
    ok(SOSPeerCoderInitializeForPeer(alice_message_transport, alice_full_peer_info, bob_peer_info, NULL));
    ok(SOSPeerCoderInitializeForPeer(bob_message_transport, bob_full_peer_info, alice_peer_info, NULL));
       
    SOSDataSourceFactoryRef aliceDsf = alice_account->factory;
    SOSDataSourceFactoryRef bobDsf = bob_account->factory;
    
    /* Test passing peer messages to the engine. */
    CFDataRef message;

    ok(SOSCircleSyncWithPeer(alice_account, alice_full_peer_info, aliceCircle, aliceDsf, bob_peer_info, &error), "Start sync [error %@]", error);
    CFReleaseNull(error);

    ok(CFDictionaryGetCount(SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)alice_message_transport)) != 0, "Alice sent message");
    CFDictionaryRef changes = SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)alice_message_transport);
    CFDictionaryRef peer_dict = CFDictionaryGetValue(changes, circleName);
    message = CFDictionaryGetValue(peer_dict, SOSPeerInfoGetPeerID(bob_peer_info));
    is(SOSCircleHandlePeerMessage(bob_account, bobCircle, bob_full_peer_info, bobDsf, alice_peer_info, message, &error), true,
        "Bob accepted message: %@", error);
    
    is(SOSCircleSyncWithPeer(bob_account, bob_full_peer_info, bobCircle, bobDsf, alice_peer_info, &error), true, "Bob sent response");
    CFReleaseNull(error);
    
#if 1
    ok(CFDictionaryGetCount(SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)bob_message_transport)) != 0, "we got a message from Bob");
    changes = SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)bob_message_transport);
    peer_dict = CFDictionaryGetValue(changes, circleName);
    message = CFDictionaryGetValue(peer_dict, SOSPeerInfoGetPeerID(alice_peer_info));
    
    ok(SOSCircleHandlePeerMessage(alice_account, aliceCircle, alice_full_peer_info, aliceDsf, bob_peer_info, message, &error),
       "Alice accepted message: %@", error);
    
    ok(CFDictionaryGetCount(SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)alice_message_transport)) != 0, "we got a reply from Alice");
    changes = SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)alice_message_transport);
    peer_dict = CFDictionaryGetValue(changes, circleName);
    message = CFDictionaryGetValue(peer_dict, SOSPeerInfoGetPeerID(bob_peer_info));
    ok(SOSCircleHandlePeerMessage(bob_account, bobCircle, bob_full_peer_info, bobDsf, alice_peer_info, message, &error),
       "Bob accepted message: %@", error);
#endif
    
#if 0
    SOSDataSourceRef aliceDs = aliceDsf->create_datasource(aliceDsf, circleName, NULL);
    ok(CFDictionaryGetCount(SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)bob_message_transport)) == 0, "we got no message from Bob");

    SOSObjectRef object = SOSDataSourceCreateGenericItem(aliceDs, CFSTR("75_circle_engine_account"), CFSTR("test service"));
    ok(SOSTestDataSourceAddObject(aliceDs, object, &error), "add empty object to datasource: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(object);

    ok(SOSCircleSyncWithPeer(alice_account, alice_full_peer_info, aliceCircle, aliceDsf, bob_peer_info, &error), "Restart sync [error %@]", error);
    CFReleaseNull(error);
    
    ok(CFDictionaryGetCount(SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)alice_message_transport)) != 0, "Alice started again");
    changes = SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)alice_message_transport);
    peer_dict = CFDictionaryGetValue(changes, circleName);
    message = CFDictionaryGetValue(peer_dict, SOSPeerInfoGetPeerID(bob_peer_info));
    
    is(SOSCircleHandlePeerMessage(bob_account, bobCircle, bob_full_peer_info, bobDsf, alice_peer_info, message, &error), true,
       "bob accepted %@", message);
    CFReleaseNull(error);
#endif

#if 1
    bool alice = true;
    int max_loops = 50;
    changes = SOSTransportMessageTestGetChanges((SOSTransportMessageTestRef)alice_message_transport);
    while (max_loops-- && (CFDictionaryGetCount(changes) != 0)) {
        peer_dict = CFDictionaryGetValue(changes, circleName);
        message = CFDictionaryGetValue(peer_dict, SOSPeerInfoGetPeerID(bob_peer_info));
        if (alice) {
            ok(SOSCircleHandlePeerMessage(alice_account, aliceCircle, alice_full_peer_info, aliceDsf, bob_peer_info, message, &error),
               "alice accepted %@: %@", message, error);
        } else {
            ok(SOSCircleHandlePeerMessage(bob_account, bobCircle, bob_full_peer_info, bobDsf, alice_peer_info, message, &error),
               "bob accepted %@: %@", message, error);
        }
        alice = !alice;
    }
#endif
    
    CFReleaseNull(aliceCircle);
    CFReleaseNull(bobCircle);

    CFReleaseNull(alice_peer_info);
    CFReleaseNull(bob_peer_info);
    
    aliceDsf->release(aliceDsf);
    bobDsf->release(bobDsf);
    
    SOSUnregisterAllTransportMessages();
    SOSUnregisterAllTransportCircles();
    SOSUnregisterAllTransportKeyParameters();
    CFArrayRemoveAllValues(key_transports);
    CFArrayRemoveAllValues(circle_transports);
    CFArrayRemoveAllValues(message_transports);
}

// MARK: ----- start of all tests -----

int sc_75_circle_engine(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
