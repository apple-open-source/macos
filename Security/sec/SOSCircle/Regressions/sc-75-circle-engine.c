//
//  sc-75-circle-engine.c
//  sec
//
//  Created by Michael Brouwer on 9/24/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//

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

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"
#include "SOSTestDataSource.h"

#ifndef SEC_CONST_DECL
#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));
#endif

#include <securityd/SOSCloudCircleServer.h>


// MARK: ----- Constants -----

static CFStringRef circleKey = CFSTR("Circle");

static int kTestTestCount = 22;

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

    /* Transport. */
    __block CFDataRef queued_message = NULL;
    
    SOSPeerSendBlock enqueueMessage =  ^bool (CFDataRef message, CFErrorRef *error) {
        if (queued_message)
            fail("We already had an unproccessed message");
        
        queued_message = (CFDataRef) CFRetain(message);
        return true;
    };
    
    CFDataRef (^dequeueMessage)() = ^CFDataRef () {
        CFDataRef result = queued_message;
        queued_message = NULL;
        
        return result;
    };

    /* DataSource */
    SOSDataSourceRef aliceDs = SOSTestDataSourceCreate();
    SOSDataSourceRef bobDs = SOSTestDataSourceCreate();
    
    SOSDataSourceFactoryRef aliceDsf = SOSTestDataSourceFactoryCreate();
    SOSTestDataSourceFactoryAddDataSource(aliceDsf, circleName, aliceDs);

    SOSDataSourceFactoryRef bobDsf = SOSTestDataSourceFactoryCreate();
    SOSTestDataSourceFactoryAddDataSource(bobDsf, circleName, bobDs);
    
    /* Test passing peer messages to the engine. */
    CFDataRef message;

    CFStringRef bob_peer_id = SOSPeerInfoGetPeerID(bob_peer_info);

    /* Hand an empty message to the engine for handeling. */
    message = CFDataCreate(NULL, NULL, 0);
    is(SOSCircleHandlePeerMessage(aliceCircle, alice_full_peer_info, aliceDsf, enqueueMessage, bob_peer_id, message, &error), false,
       "empty message rejected, %@", error);

    CFReleaseNull(error);
    CFReleaseNull(message);

    ok(SOSCircleSyncWithPeer(alice_full_peer_info, aliceCircle, aliceDsf, enqueueMessage, bob_peer_id, &error), "Start sync [error %@]", error);
    CFReleaseNull(error);

    ok(message = dequeueMessage(), "Alice sent message");
    CFStringRef alice_peer_id = SOSPeerInfoGetPeerID(alice_peer_info);
    is(SOSCircleHandlePeerMessage(bobCircle, bob_full_peer_info, bobDsf, enqueueMessage, alice_peer_id, message, &error), true,
       "Bob accepted message: %@", error);
    CFReleaseNull(message);

#if 1
    CFStringRef desc = NULL;
    ok(message = dequeueMessage(), "we got a message from Bob %@", desc = SOSMessageCopyDescription(message));
    ok(SOSCircleHandlePeerMessage(aliceCircle, alice_full_peer_info, aliceDsf, enqueueMessage, bob_peer_id, message, &error),
       "Alice accepted message: %@", error);
    CFReleaseNull(message);
    CFReleaseNull(desc);

    ok(message = dequeueMessage(), "we got a reply from Alice %@", desc = SOSMessageCopyDescription(message));
    ok(SOSCircleHandlePeerMessage(bobCircle, bob_full_peer_info, bobDsf, enqueueMessage, alice_peer_id, message, &error),
       "Bob accepted message: %@", error);
    CFReleaseNull(message);
    CFReleaseNull(desc);
#endif

#if 0
    message = dequeueMessage();
    ok(NULL == message, "we got no message from Bob %@", desc = SOSMessageCopyDescription(message));

    SOSObjectRef object = SOSDataSourceCreateGenericItem(aliceDs, CFSTR("75_circle_engine_account"), CFSTR("test service"));
    ok(SOSTestDataSourceAddObject(aliceDs, object, &error), "add empty object to datasource: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(object);

    ok(SOSCircleSyncWithPeer(alice_full_peer_info, aliceCircle, aliceDsf, enqueueMessage, bob_peer_id, &error), "Restart sync [error %@]", error);
    CFReleaseNull(error);
    
    ok(message = dequeueMessage(), "Alice started again %@", desc = SOSMessageCopyDescription(message));
    is(SOSCircleHandlePeerMessage(bobCircle, bob_full_peer_info, bobDsf, enqueueMessage, alice_peer_id, message, &error), true,
       "bob accepted %@: %@", SOSMessageCopyDescription(message), error);
    CFReleaseNull(error);
    CFReleaseNull(message);
#endif

#if 1
    bool alice = true;
    int max_loops = 50;
    while (max_loops-- && NULL != (message = dequeueMessage())) {
        if (alice) {
            ok(SOSCircleHandlePeerMessage(aliceCircle, alice_full_peer_info, aliceDsf, enqueueMessage, bob_peer_id, message, &error),
               "alice accepted %@: %@", desc = SOSMessageCopyDescription(message), error);
        } else {
            ok(SOSCircleHandlePeerMessage(bobCircle, bob_full_peer_info, bobDsf, enqueueMessage, alice_peer_id, message, &error),
               "bob accepted %@: %@", desc = SOSMessageCopyDescription(message), error);
        }
        alice = !alice;
        CFRelease(message);
        CFReleaseNull(desc);
    }
#endif

    CFReleaseNull(aliceCircle);
    CFReleaseNull(bobCircle);

    CFReleaseNull(alice_peer_info);
    CFReleaseNull(bob_peer_info);

    CFReleaseNull(alice_key);
    CFReleaseNull(bob_key);
    aliceDsf->release(aliceDsf);
    bobDsf->release(bobDsf);
}

// MARK: ----- start of all tests -----

int sc_75_circle_engine(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);

    tests();

	return 0;
}
