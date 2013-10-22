//
//  sc-100-devicecircle.c
//  sec
//
//  Created by John Hurley 10/16/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//

/*
    This test is a combination of sc-75, sc_93 and sc_94 that can
    be run on two devices.
    
    The test will look for a circle in kvs
    - if none exists, it will create one
    - if one exists, it will try to join
    
    Whenever you confirm a new peer, must start sync
    
    Test sc-98 can be run before this to clear out kvs
*/

// Run on 2 devices:
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_100_devicecircle -v -- -i alice
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_100_devicecircle -v -- -i bob

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>

#include "SOSCircle_regressions.h"

#include <corecrypto/ccsha2.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include <stdint.h>

#include <AssertMacros.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CoreFoundation/CFDate.h>
#include <getopt.h>


#include <Security/SecKey.h>

#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"
#include "SOSTestDataSource.h"
#include "SOSTestTransport.h"
#include "SOSCloudKeychainClient.h"

#include <securityd/SOSCloudCircleServer.h>

#ifndef SEC_CONST_DECL
#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));
#endif

//static     dispatch_queue_t wait_queue = NULL;

#define VALUECFNULLCHECK(msg) if (msg == NULL || CFGetTypeID(msg) == CFNullGetTypeID()) { pass("CFNull message"); return 0; }

// TODO _SecServerKeychainSyncUpdate

// MARK: ----- Constants -----

static CFStringRef circleKey = CFSTR("Circle");
//static CFStringRef messageKey = CFSTR("Message");
static CFStringRef messageFromAliceToBobKey = CFSTR("AliceToBob");
static CFStringRef messageFromBobToAliceKey = CFSTR("BobToAlice");

#if USE_BOB_GO
static CFStringRef messageGoBobGoKey = CFSTR("GoBobGo");
#endif

struct SOSKVSTransport {
    struct SOSTransport t;
    CFStringRef messageKey;
};

#include <notify.h>
#include <dispatch/dispatch.h>

static bool kvsTransportSend(CFStringRef key, CFDataRef message) {
    __block bool success = true;
    __block dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    
    CFDictionaryRef objects = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, key, message, NULL);

    CFStringRef desc = SOSMessageCopyDescription(message);
    pass("kvsTransportSend: %@ %@", key, desc);
    CFReleaseSafe(desc);
    
    SOSCloudKeychainPutObjectsInCloud(objects, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            ok(error == NULL, "Error from SOSCloudKeychainPutObjectsInCloud %@:", error);
            if (error)
                success = false;
            dispatch_semaphore_signal(waitSemaphore);
        });

    dispatch_release(waitSemaphore);

    return success;
}

static void putCircleInCloud(SOSCircleRef circle, dispatch_queue_t work_queue, dispatch_group_t work_group)
{
    CFErrorRef error = NULL;
    CFDataRef newCloudCircleEncoded = SOSCircleCopyEncodedData(circle, kCFAllocatorDefault, &error);
    ok(newCloudCircleEncoded, "Encoded as: %@ [%@]", newCloudCircleEncoded, error);

    // Send the circle with our application request back to cloud
    testPutObjectInCloud(circleKey, newCloudCircleEncoded, &error, work_group, work_queue);
    CFReleaseSafe(newCloudCircleEncoded);
}

static void mutate_inflated_circle(SOSCircleRef newCircle,
                                   dispatch_queue_t work_queue, dispatch_group_t work_group,
                          void (^action)(SOSCircleRef circle))
{
//JCH    action(newCircle);
        
    putCircleInCloud(newCircle, work_queue, work_group);
    pass("Put circle in cloud: (%@)", newCircle);
}

static void mutate_account_circle(SOSAccountRef account,
                                  CFDataRef encodedCircle,
                                  dispatch_queue_t work_queue, dispatch_group_t work_group,
                                  void (^action)(SOSCircleRef circle))
{
    bool mutated = false;

    CFErrorRef error = NULL;
    SKIP:
    {
        skip("Must be CFData!", 3, encodedCircle && (CFDataGetTypeID() == CFGetTypeID(encodedCircle)));
        SOSCircleRef newCircle = SOSCircleCreateFromData(kCFAllocatorDefault, encodedCircle, &error);
        ok(newCircle, "Decoded data version of circle: %@", newCircle);
        
        SOSCircleRef accountCircle = SOSAccountFindCircle(account, SOSCircleGetName(newCircle), NULL);
        ok(accountCircle, "Found our circle in account");
        
        if (!CFEqual(newCircle, accountCircle)) {
            pass("New circle and accountCircle not equal");
            
            // JCH JCH
                action(newCircle);

            bool updated = SOSAccountUpdateCircle(account, newCircle, &error);
            if (updated) {
                pass("Updated account with new circle");
                mutated = true;
                mutate_inflated_circle(newCircle, work_queue, work_group, action);
            }
        }
    }
    pass("mutate_account_circle exit");
}

static void SOSCloudKeychainRegisterKeysAndGetWithNotification(CFArrayRef keysToRegister, dispatch_queue_t processQueue, CloudKeychainReplyBlock replyBlock)
{
    SOSCloudKeychainRegisterKeysAndGet(keysToRegister, processQueue, replyBlock,
                                       ^(CFDictionaryRef dict) { replyBlock(dict, NULL);});
}

#define kAlicePeerID CFSTR("alice-peer-id")
#define kBobPeerID CFSTR("bob-peer-id")

static void runTests(bool Alice, dispatch_queue_t work_queue, dispatch_group_t work_group)
{
    SecKeyRef user_key = NULL;
    SecKeyRef public_key = NULL;
    CFErrorRef error = NULL;
    CFStringRef cflabel = CFSTR("TEST_USERKEY");
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);

    GenerateECPair(256, &public_key, &user_key);
    SOSCCRegisterUserCredentials(cflabel, cfpassword, &error);
    CFReleaseNull(public_key);
    CFReleaseNull(cfpassword);

    SOSDataSourceFactoryRef our_data_source_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef our_data_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactoryAddDataSource(our_data_source_factory, circleKey, our_data_source);

    CFDictionaryRef gestalt = SOSCreatePeerGestaltFromName(Alice? kAlicePeerID: kBobPeerID);

    SOSAccountRef our_account = SOSAccountCreate(kCFAllocatorDefault, gestalt, our_data_source_factory, NULL, NULL);
    SOSAccountEnsureCircle(our_account, circleKey, NULL);

    SOSFullPeerInfoRef our_full_peer_info = SOSAccountGetMyFullPeerInCircleNamed(our_account, circleKey, &error);
    __block SOSPeerInfoRef our_peer_info = SOSFullPeerInfoGetPeerInfo(our_full_peer_info);
    CFRetain(our_peer_info);
    pass("We are %s (%@)", Alice?"Alice":"Bob", our_peer_info);

    ok(our_peer_info, "Peer Info: %@ [error: %@]", our_peer_info, error);

    /// Setup ck notifications.
    
    CFArrayRef keysToRegister = NULL;
    
    CFStringRef message_key;
    
    if (Alice)
    {
        keysToRegister = CFArrayCreateForCFTypes(kCFAllocatorDefault, circleKey, messageFromBobToAliceKey, NULL);
        message_key = messageFromAliceToBobKey;
    }
    else
    {
#if USE_BOB_GO
        keysToRegister = CFArrayCreateForCFTypes(kCFAllocatorDefault, circleKey, messageFromAliceToBobKey, messageGoBobGoKey, NULL);
#else
        keysToRegister = CFArrayCreateForCFTypes(kCFAllocatorDefault, circleKey, messageFromAliceToBobKey, NULL);
#endif
        message_key = messageFromBobToAliceKey;
    }

    SOSPeerSendBlock transport = ^bool (CFDataRef message, CFErrorRef *error) {
        kvsTransportSend(message_key, message);
        return true;
    };


    __block CFIndex current = 0;
    typedef CFIndex (^TestStateBlock) (CFDictionaryRef returnedValues, CFErrorRef error);

    //------------------------------------------------------------------------
    //              ALICE
    //------------------------------------------------------------------------
    CFArrayRef aliceWorkToDo =
    CFArrayCreateForCFTypes(kCFAllocatorDefault,
        ^ CFIndex (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            /*
                We get here as the return from registerkeys.
                Here we create a fresh circle, add and accept ourselves
                Then we post to the cloud and wait for a Circle changed notification
            */
            
            (void) returnedValues; (void) error;
            
            testClearAll(work_queue, work_group);

            CFErrorRef localError = NULL;
            
#if USE_BOB_GO
            testPutObjectInCloud(messageGoBobGoKey, CFSTR("Go Bob, Go!"), &localError, work_group, work_queue);
#endif

            SOSCircleRef circle = SOSAccountFindCircle(our_account, circleKey, NULL);
            CFRetain(circle);
            
            ok(SOSCircleRequestAdmission(circle, SOSAccountGetPrivateCredential(our_account, &error), our_full_peer_info, &localError), "Requested admission (%@)", our_peer_info);
            ok(SOSCircleAcceptRequests(circle, SOSAccountGetPrivateCredential(our_account, &error), our_full_peer_info, &localError), "Accepted self");
            
            our_peer_info = SOSFullPeerInfoGetPeerInfo(our_full_peer_info);

            putCircleInCloud(circle, work_queue, work_group);
            pass("Put circle in cloud: (%@)", circle);
            
            CFRelease(circle);
            
            return +1;
        },
        ^ CFIndex (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            __block CFIndex incrementAmount = 0; // Don't increment unless we find stuff

            /*
                When we get here, it should only be because Bob has retrieved
                our circle and requested admission to our circle.
                If we don't find a circleKey entry, our test setup is wrong.
            */
            SKIP:
            {
                __block CFErrorRef localError = NULL;
                CFDataRef value = CFDictionaryGetValue(returnedValues, circleKey);
                VALUECFNULLCHECK(value);
                skip("cloudCircle NULL!", 5, value);
                ok(value, "Found circle");
                
                mutate_account_circle(our_account, value, work_queue, work_group,
                    ^ (SOSCircleRef circle)
                    {
                        ok(SOSCircleHasPeer(circle, our_peer_info, &localError), "We're a peer [error: %@]", localError);
                        CFReleaseNull(localError);
                        
                        is(SOSCircleCountPeers(circle), 1, "One peer, woot");
                        is(SOSCircleCountApplicants(circle), 1, "One applicant, hope it's BOB");
                        
                        CFErrorRef pkerr;
                        ok(SOSCircleAcceptRequests(circle, SOSAccountGetPrivateCredential(our_account, &pkerr), our_full_peer_info, &localError), "Accepted peers [error: %@]", localError);
                        CFReleaseNull(localError);

                        ok(SOSCircleSyncWithPeer(our_full_peer_info, circle, our_data_source_factory, transport, kBobPeerID, &localError), "Started sync: [error: %@]", localError);
                        CFReleaseNull(localError);
                        
                        incrementAmount = 1;
                    });

                CFReleaseSafe(localError);
            }
            
            return incrementAmount;
        },
        ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            CFErrorRef localError = NULL;
            SOSCircleRef circle = SOSAccountFindCircle(our_account, circleKey, NULL);
            CFDataRef message = CFDictionaryGetValue(returnedValues, messageFromBobToAliceKey);
            VALUECFNULLCHECK(message);

            ok(message, "Saw response to manifest message from Bob");
            ok(SOSCircleHandlePeerMessage(circle, our_full_peer_info, our_data_source_factory, transport, kBobPeerID, message, &localError), "handle message from bob: [error: %@]", localError);
            
#if TEST_EMPTY_ADD
            // Sync again after adding an empty object
            CFDictionaryRef object = CFDictionaryCreate(0, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            ok(SOSTestDataSourceAddObject(our_data_source, object, &error), "add empty object to datasource [error: %@]", error);

            ok(ourEngine = SOSCircleCopyEngine(circle, our_data_source_factory, &localError), "get ourEngine [error: %@]", localError);

            // Start syncing
            SOSPeerRef bobPeer = SOSCircleCopyPeer(circle, transport, kBobPeerID, &localError);
            ok(bobPeer, "Got bob: [error: %@]", error);
            ok(SOSEngineSyncWithPeer(ourEngine, bobPeer, false, &localError), "tell Alice sync with peer Bob");
            SOSPeerDispose(bobPeer);

            CFReleaseNull(localError);
#endif
            return 1;
        },
#if TEST_EMPTY_ADD
        ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            CFErrorRef localError = NULL;
            SOSCircleRef circle = SOSAccountFindCircle(our_account, circleKey);
            CFDataRef message = CFDictionaryGetValue(returnedValues, messageFromBobToAliceKey);
            VALUECFNULLCHECK(message);
            ok(message, "Saw response to manifest message from Bob - 2");
            ok(SOSCircleHandlePeerMessage(circle, our_data_source_factory, transport, kBobPeerID, message, &localError), "handle message from bob: [error: %@]", localError);
            return 1;
        },
#endif
        NULL);
    
    //------------------------------------------------------------------------
    //              BOB
    //------------------------------------------------------------------------

    CFArrayRef bobWorkToDo =
    CFArrayCreateForCFTypes(kCFAllocatorDefault,

        ^ CFIndex (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            __block CFIndex incrementAmount = 0; // Don't increment unless we find stuff
            __block CFErrorRef localError = NULL;
            
#if USE_BOB_GO
            CFTypeRef goMessage = CFDictionaryGetValue(returnedValues, messageGoBobGoKey);
            if (!goMessage) // We can discard any changes we see here; they are stale
                return 0;
            
            SOSCloudKeychainRemoveObjectForKey(messageGoBobGoKey, work_queue,
                ^ (CFDictionaryRef returnedValues, CFErrorRef error)
                {
                    pass("ACK from messageGoBobGoKey");
                });
#endif

            CFTypeRef value = CFDictionaryGetValue(returnedValues, circleKey);
            VALUECFNULLCHECK(value);

            ok(value, "Found circle %@", value);

            mutate_account_circle(our_account, value, work_queue, work_group,
                ^ (SOSCircleRef circle)
                {
                    int peerCount = SOSCircleCountPeers(circle);
                    ok(peerCount == 1, "One peer, hope it's Alice");
                    if (peerCount != 1)
                        printf("NOT One peer, hope it's Alice: saw %d peers\n", peerCount);
                    ok(SOSCircleCountApplicants(circle) == 0, "No applicants");
                    CFErrorRef pkerr;

                    ok(SOSCircleRequestAdmission(circle, SOSAccountGetPrivateCredential(our_account, &pkerr), our_full_peer_info, &localError), "Requested admission");
                    our_peer_info = SOSFullPeerInfoGetPeerInfo(our_full_peer_info);

                    incrementAmount = 1;
                });

            CFReleaseSafe(localError);
            
            return incrementAmount;
        },
        ^ CFIndex (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            __block CFIndex incrementAmount = 0; // Don't increment unless we find stuff
            __block CFErrorRef localError = NULL;

            CFDataRef value = CFDictionaryGetValue(returnedValues, circleKey);
            VALUECFNULLCHECK(value);
            ok(value, "Found circle");

            mutate_account_circle(our_account, value, work_queue, work_group,
                ^ (SOSCircleRef circle)
                {
                    ok(SOSCircleHasPeer(circle, our_peer_info, &localError), "We're a peer");
                    ok(SOSCircleCountPeers(circle) == 2, "Both peers!");
                    ok(SOSCircleCountApplicants(circle) == 0, "No applicants!");

                    CFDataRef message;
                    ok(message = CFDictionaryGetValue(returnedValues, messageFromAliceToBobKey), "got message from alice");
               //     VALUECFNULLCHECK(message);
               if (message == NULL || CFGetTypeID(message) == CFNullGetTypeID()) { pass("CFNull message"); return; }
                    ok(SOSCircleHandlePeerMessage(circle, our_full_peer_info, our_data_source_factory, transport, kAlicePeerID, message, &localError), "handle message from alice: %@", localError);
                    
                    incrementAmount = 1;
                });

            CFReleaseSafe(localError);
            return incrementAmount;

        },
#if TEST_EMPTY_ADD
/**/
        // this next block would normally be looking at the reply to the initial sync message.
        // Since that one was the same we sent out, we won't get a notification for that.
        // This block looks at the sync message of the empty object datasource
        ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            CFErrorRef localError = NULL;
            SOSCircleRef circle = SOSAccountFindCircle(our_account, circleKey);
            CFDataRef message = CFDictionaryGetValue(returnedValues, messageFromBobToAliceKey);
            VALUECFNULLCHECK(message);
            ok(message, "Saw response to manifest message from Alice 2");
            ok(SOSCircleHandlePeerMessage(circle, our_data_source_factory, transport, kAlicePeerID, message, &localError), "handle message from Alice 2: %@", localError);
            return 1;
        },
#endif            

         NULL);
    
    //------------------------------------------------------------------------
    //              START
    //------------------------------------------------------------------------
    
    CFArrayRef ourWork = Alice ? aliceWorkToDo : bobWorkToDo;

    SOSCloudKeychainRegisterKeysAndGetWithNotification(keysToRegister, work_queue,
        ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            pass("Got key changes: %@ [error: %@]", returnedValues, error);

            CFReleaseSafe(error);

            TestStateBlock thingToDo = CFArrayGetValueAtIndex(ourWork, current);
            
            if (thingToDo)
            {
                pass("%s stage %d rv: %@ [error: %@]", Alice?"Alice":"Bob", (int)current, returnedValues, error);
                current += thingToDo(returnedValues, error);
            }

            if (current < 0 || current >= CFArrayGetCount(ourWork))
                dispatch_group_leave(work_group);
        });

    dispatch_group_wait(work_group, DISPATCH_TIME_FOREVER);

    // We probably never get here since the program exits..

    CFReleaseNull(aliceWorkToDo);
    CFReleaseNull(bobWorkToDo);
    CFReleaseNull(gestalt);
    CFReleaseNull(our_account);
    CFReleaseNull(our_peer_info);
    CFReleaseNull(user_key);
    CFReleaseNull(public_key); // Should be NULL but just in case..

#if 0

    if (Alice)
    {
        CFDictionaryRef object = CFDictionaryCreate(0, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        ok(SOSTestDataSourceAddObject(our_data_source, object, &error), "add empty object to datasource (error: %@)", error);
        CFReleaseNull(error);
        CFReleaseNull(object);

        sendManifestDigest(transportX, pqrEngine, our_circle, our_peer_info);
        ok(waitForSemaphore(), "Got ACK for manifest with object");
        ok(handleCloudMessage(our_circle, our_data_source, t), "Got ACK for manifest with object");
    }
#endif
}

// MARK: ----- start of all tests -----
static void tests(bool Alice)
{
    dispatch_queue_t work_queue = dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_t work_group = dispatch_group_create();

    // Queue the work we want to do.
    runTests(Alice, work_queue, work_group);

}

// define the options table for the command line
static const struct option options[] =
{
	{ "verbose",		optional_argument,	NULL, 'v' },
	{ "identity",		optional_argument,	NULL, 'i' },
	{ "clear",          optional_argument,	NULL, 'C' },
	{ }
};

static int kAliceTestCount = 22;
static int kBobTestCount = 20;

int sc_100_devicecircle(int argc, char *const *argv)
{
    char *identity = NULL;
//    extern int optind;
    extern char *optarg;
    int arg, argSlot;
    bool Alice = false;

    while (argSlot = -1, (arg = getopt_long(argc, (char * const *)argv, "i:vC", options, &argSlot)) != -1)
        switch (arg)
        {
        case 'i':
            identity = (char *)(optarg);
            break;
        case 'C':   // should set up to call testClearAll
            break;
        default:
            secerror("arg: %s", optarg);
            break;
        }
    
    if (identity)
    {
        secerror("We are %s",identity);
        if (!strcmp(identity, "alice"))
            Alice = true;
    }

    plan_tests(Alice?kAliceTestCount:kBobTestCount);
    tests(Alice);

	return 0;
}
