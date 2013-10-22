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
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_101_accountsync -v -- -i alice
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_101_accountsync -v -- -i bob

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>

#include "SOSCircle_regressions.h"

#include <corecrypto/ccsha2.h>
#include <Security/SecRandom.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>
#include <utilities/iOSforOSX.h>
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

struct SOSKVSTransport {
    struct SOSTransport t;
    CFStringRef messageKey;
};

#include <notify.h>
#include <dispatch/dispatch.h>

static void putCircleInCloud(SOSCircleRef circle, dispatch_queue_t work_queue, dispatch_group_t work_group)
{
    CFErrorRef error = NULL;
    CFDataRef newCloudCircleEncoded = SOSCircleCopyEncodedData(circle, kCFAllocatorDefault, &error);
    ok(newCloudCircleEncoded, "Encoded as: %@ [%@]", newCloudCircleEncoded, error);

    // Send the circle with our application request back to cloud
    testPutObjectInCloud(circleKey, newCloudCircleEncoded, &error, work_group, work_queue);
    CFReleaseSafe(newCloudCircleEncoded);
}


// artifacts of test harness : , dispatch_queue_t work_queue, dispatch_group_t work_group)
bool SOSAccountEstablishCircle(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error, dispatch_queue_t work_queue, dispatch_group_t work_group);

bool SOSAccountEstablishCircle(SOSAccountRef account, CFStringRef circleName, CFErrorRef *error, dispatch_queue_t work_queue, dispatch_group_t work_group)
{
    CFErrorRef localError = NULL;
    SOSCircleRef circle = SOSAccountEnsureCircle(account, circleName, NULL);
    CFRetain(circle);
    SecKeyRef user_privkey = SOSAccountGetPrivateCredential(account, &localError);

    SOSFullPeerInfoRef our_full_peer_info = SOSAccountGetMyFullPeerInCircleNamed(account, circleName, &localError);
    SOSPeerInfoRef our_peer_info = SOSFullPeerInfoGetPeerInfo(our_full_peer_info);
    CFRetain(our_peer_info);

    SecKeyRef   device_key =   SOSFullPeerInfoCopyDeviceKey(our_full_peer_info, &localError);
    ok(device_key, "Retrieved device_key from full peer info (Error: %@)", localError);
    CFReleaseNull(device_key);
    CFReleaseNull(localError);
    ok(SOSCircleRequestAdmission(circle, user_privkey, our_full_peer_info, &localError), "Requested admission (%@)", our_peer_info);
    ok(SOSCircleAcceptRequests(circle, user_privkey, our_full_peer_info, &localError), "Accepted self");
    
    putCircleInCloud(circle, work_queue, work_group);
    pass("Put (new) circle in cloud: (%@)", circle);

#if 0
    

    SOSCircleRef oldCircle = SOSAccountFindCircle(account, SOSCircleGetName(circle));

    if (!oldCircle)
        return false; // Can't update one we don't have.
    // TODO: Ensure we don't let circles get replayed.

    CFDictionarySetValue(account->circles, SOSCircleGetName(circle), circle);

//----
            SOSCircleRef circle = SOSAccountFindCircle(our_account, circleKey);
            CFRetain(circle);
            
            ok(SOSCircleRequestAdmission(circle, our_full_peer_info, user_key, &localError), "Requested admission (%@)", our_peer_info);
            ok(SOSCircleAcceptRequests(circle, our_full_peer_info, user_key, &localError), "Accepted self");
            
            putCircleInCloud(circle, work_queue, work_group);

//-----
#endif
    return true;
}


static void runTests(bool Alice, dispatch_queue_t work_queue, dispatch_group_t work_group)
{
    CFStringRef our_name =      Alice ? CFSTR("Alice") : CFSTR("Bob");
    CFDictionaryRef our_gestalt = SOSCreatePeerGestaltFromName(our_name);
    
    dispatch_queue_t global_queue = dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    
    CFErrorRef error = NULL;

    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);

    CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
    ok(parameters, "No parameters!");
    ok(error == NULL, "Error: (%@)", error);
    CFReleaseNull(error);

    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, &error);
    CFReleaseNull(parameters);
    CFReleaseNull(cfpassword);

    dispatch_semaphore_t start_semaphore = dispatch_semaphore_create(0);

    CFStringRef sBobReady = CFSTR("Bob-Ready");
    CFStringRef sAliceReady = CFSTR("Alice-Ready");
    __block CFDataRef foundNonce = NULL;
    if (Alice) {
        const CFIndex nonceByteCount = 10;
        CFMutableDataRef nonce = CFDataCreateMutable(kCFAllocatorDefault, nonceByteCount);
        CFDataSetLength(nonce, nonceByteCount);
        SecRandomCopyBytes(kSecRandomDefault, CFDataGetLength(nonce), CFDataGetMutableBytePtr(nonce));

        CloudItemsChangedBlock notification_block = ^ (CFDictionaryRef returnedValues)
        {
            CFTypeRef bobReadyValue = CFDictionaryGetValue(returnedValues, sBobReady);
            if (isData(bobReadyValue) && CFEqual(bobReadyValue, nonce)) {
                CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, sAliceReady, kCFNull, sBobReady, kCFNull, NULL);

                SOSCloudKeychainPutObjectsInCloud(changes, global_queue, NULL);

                pass("signalling");
                dispatch_semaphore_signal(start_semaphore);
                CFReleaseSafe(changes);
            }
            CFReleaseSafe(error);
        };

        CloudKeychainReplyBlock reply_block = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            notification_block(returnedValues);
        };

        pass("Clearing");

        testClearAll(global_queue, work_group);

        CFArrayRef bobKey = CFArrayCreateForCFTypes(kCFAllocatorDefault, sBobReady, NULL);
        SOSCloudKeychainRegisterKeysAndGet(bobKey, work_queue, reply_block, notification_block);
        
        CFStringRef description = SOSInterestListCopyDescription(bobKey);
        pass("%@", description);
        
        CFReleaseNull(description);
        CFReleaseNull(bobKey);

        CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, sAliceReady, nonce, NULL);
        SOSCloudKeychainPutObjectsInCloud(changes, global_queue, NULL);
        
        description = SOSChangesCopyDescription(changes, true);
        pass("%@", description);
        CFReleaseNull(description);
        
        CFReleaseNull(changes);
    } else {
        CloudItemsChangedBlock notification_block = ^ (CFDictionaryRef returnedValues)
        {
            CFTypeRef aliceReadyValue = CFDictionaryGetValue(returnedValues, sAliceReady);
            if (isData(aliceReadyValue)) {
                foundNonce = (CFDataRef) aliceReadyValue;
                CFRetain(foundNonce);

                pass("signalling found: %@", foundNonce);
                dispatch_semaphore_signal(start_semaphore);
            }
            CFReleaseSafe(error);
        };

        CloudKeychainReplyBlock reply_block = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            notification_block(returnedValues);
        };

        CFArrayRef aliceKey = CFArrayCreateForCFTypes(kCFAllocatorDefault, sAliceReady, NULL);
        SOSCloudKeychainRegisterKeysAndGet(aliceKey, work_queue, reply_block, notification_block);
        
        CFStringRef description = SOSInterestListCopyDescription(aliceKey);
        pass("%@", description);
        CFReleaseNull(description);

        CFReleaseSafe(aliceKey);
    }

    pass("Waiting");
    dispatch_semaphore_wait(start_semaphore, DISPATCH_TIME_FOREVER);
    pass("Moving on");


    __block CFArrayRef ourWork = NULL;
    __block CFIndex current = 0;
    __block SOSAccountRef our_account = NULL;
    typedef CFIndex (^TestStateBlock) (SOSAccountRef account, CFErrorRef error);

    SOSDataSourceFactoryRef our_data_source_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef our_data_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactoryAddDataSource(our_data_source_factory, circleKey, our_data_source);
    
    CloudItemsChangedBlock notification_block = ^ (CFDictionaryRef returnedValues)
    {
        CFStringRef changesString = SOSChangesCopyDescription(returnedValues, false);
        pass("Got: %@", changesString);
        CFReleaseNull(changesString);

        CFErrorRef error = NULL;

        SOSAccountHandleUpdates(our_account, returnedValues, &error);

        TestStateBlock thingToDo = CFArrayGetValueAtIndex(ourWork, current);

        if (thingToDo)
        {
            pass("%@ stage %d rv: %@ [error: %@]", our_name, (int)current, returnedValues, error);
            current += thingToDo(our_account, error);
        }

        if (current < 0 || current >= CFArrayGetCount(ourWork))
            dispatch_group_leave(work_group);

        CFReleaseSafe(error);
    };

    CloudKeychainReplyBlock reply_block = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        pass("Reply block");
        notification_block(returnedValues);
    };

    __block bool initialConnection = !Alice;
    SOSAccountKeyInterestBlock updateKVSKeys = ^(bool getNewKeysOnly, CFArrayRef alwaysKeys, CFArrayRef afterFirstUnlockKeys, CFArrayRef unlockedKeys) {
        CFMutableArrayRef keys = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, alwaysKeys);
        CFArrayAppendArray(keys, afterFirstUnlockKeys, CFRangeMake(0, CFArrayGetCount(afterFirstUnlockKeys)));
        CFArrayAppendArray(keys, unlockedKeys, CFRangeMake(0, CFArrayGetCount(unlockedKeys)));

        CFStringRef description = SOSInterestListCopyDescription(keys);

        pass("%@", description);

        CFReleaseNull(description);

        SOSCloudKeychainRegisterKeysAndGet(keys, work_queue,
                                           initialConnection ? reply_block : NULL, notification_block);

        CFReleaseSafe(keys);
        initialConnection = false;
    };
    
    SOSAccountDataUpdateBlock updateKVS = ^ bool (CFDictionaryRef changes, CFErrorRef *error) {
        CFStringRef changesString = SOSChangesCopyDescription(changes, true);
        pass("Pushing: %@", changesString);
        CFReleaseNull(changesString);

        SOSCloudKeychainPutObjectsInCloud(changes, global_queue,
                                          ^ (CFDictionaryRef returnedValues, CFErrorRef error)
                                          {
                                              if (error) {
                                                  fail("testPutObjectInCloud returned: %@", error);
                                                  CFRelease(error);
                                              }
                                          });
        return true;
    };
    

    our_account = SOSAccountCreate(kCFAllocatorDefault, our_gestalt, our_data_source_factory, updateKVSKeys, updateKVS);
    
    SOSFullPeerInfoRef our_full_peer_info = SOSAccountGetMyFullPeerInCircleNamed(our_account, circleKey, &error);
    SOSPeerInfoRef our_peer_info = SOSFullPeerInfoGetPeerInfo(our_full_peer_info);
    CFRetain(our_peer_info);

    SOSAccountAddChangeBlock(our_account, ^(SOSCircleRef circle,
                                            CFArrayRef peer_additions, CFArrayRef peer_removals,
                                            CFArrayRef applicant_additions, CFArrayRef applicant_removals) {
        // Should initiate syncing here!
        bool joined = CFArrayContainsValue(peer_additions, CFRangeMake(0, CFArrayGetCount(peer_additions)), our_peer_info);

        pass("Peers Changed [%s] (Add: %@, Remove: %@)", joined ? "*** I'm in ***" : "Not including me.", peer_additions, peer_removals);

        if (joined) {
            SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer_info)
                       {
                           CFErrorRef error = NULL;

                           if (!CFEqual(peer_info, our_peer_info)) {
                               ok(SOSAccountSyncWithPeer(our_account, circle, peer_info, NULL, &error),
                                  "Initiated sync with %@: [Error %@]", peer_info, error);
                           }
                       });
        }
    });
    
    ok(our_peer_info, "Peer Info: %@ [error: %@]", our_peer_info, error);
        
    //__block SOSEngineRef ourEngine;

    SOSObjectRef firstObject = SOSDataSourceCreateGenericItem(our_data_source, CFSTR("1234"), CFSTR("service"));

    //------------------------------------------------------------------------
    //              ALICE
    //------------------------------------------------------------------------
    CFArrayRef aliceWorkToDo =
    CFArrayCreateForCFTypes(kCFAllocatorDefault,
                           ^ CFIndex (SOSAccountRef account, CFErrorRef error)
                           {
                               /*
                                When we get here, it should only be because Bob has retrieved
                                our circle and requested admission to our circle.
                                If we don't find a circleKey entry, our test setup is wrong.
                                */
                               CFErrorRef modifyError = NULL;
                               SOSAccountModifyCircle(account, circleKey, &modifyError, ^(SOSCircleRef circle) {
                                   CFErrorRef localError = NULL;
                                   
                                   ok(SOSCircleHasPeer(circle, our_peer_info, &localError), "We're a peer [Error: %@]", localError);
                                   is(SOSCircleCountPeers(circle), 1, "One peer, woot");
                                   is(SOSCircleCountApplicants(circle), 1, "One applicant, hope it's BOB");
                                   
                                   ok(SOSCircleAcceptRequests(circle, user_privkey, our_full_peer_info, &localError), "Accepted peers (%@) [Error: %@]", circle, localError);
                                   
                                   CFReleaseSafe(localError);
                               });
                               CFReleaseSafe(modifyError);

                               return +1;
                           },
                           ^ CFIndex (SOSAccountRef account, CFErrorRef error)
                           {
                               // He should be telling us about him and we should be responding.
                               
                               CFMutableDictionaryRef ourDatabase = SOSTestDataSourceGetDatabase(our_data_source);
                               
                               is(CFDictionaryGetCount(ourDatabase), 0, "Database empty, we're synced");

                               pass("1");
                               return +1;
                           },
                           ^ CFIndex (SOSAccountRef account, CFErrorRef error)
                           {
                               CFMutableDictionaryRef ourDatabase = SOSTestDataSourceGetDatabase(our_data_source);
                               
                               is(CFDictionaryGetCount(ourDatabase), 1, "One element!");
                               
                               return +1;
                           },
                           NULL);
    
    //------------------------------------------------------------------------
    //              BOB
    //------------------------------------------------------------------------
    
    CFArrayRef bobWorkToDo =
    CFArrayCreateForCFTypes(kCFAllocatorDefault,
                           ^ CFIndex (SOSAccountRef account, CFErrorRef error)
                           {
                               __block CFIndex increment = 0;
                               CFErrorRef modifyError = NULL;
                               SOSAccountModifyCircle(account, circleKey, &modifyError, ^(SOSCircleRef circle) {
                                   CFErrorRef localError = NULL;

                                   if (SOSCircleCountPeers(circle) == 1) {
                                       is(SOSCircleCountApplicants(circle), 0, "No applicants");
                                       ok(SOSCircleRequestAdmission(circle, user_privkey, our_full_peer_info, &localError), "Requested admission (%@) [Error: %@]", circle, localError);
                                       increment = +1;
                                   }
                                   
                                   CFReleaseSafe(localError);
                               });
                               CFReleaseSafe(modifyError);
                               return increment;
                           },
                           ^ CFIndex (SOSAccountRef account, CFErrorRef error)
                           {
                               CFErrorRef modifyError = NULL;
                               SOSAccountModifyCircle(account, circleKey, &modifyError, ^(SOSCircleRef circle) {
                                   CFErrorRef localError = NULL;

                                   ok(SOSCircleHasPeer(circle, our_peer_info, &localError), "We're a peer (%@) [Error: %@]", circle, localError);
                                   is(SOSCircleCountPeers(circle), 2, "One peer, hope it's Alice");
                                   is(SOSCircleCountApplicants(circle), 0, "No applicants");
                                   
                                   CFReleaseSafe(localError);
                               });
                               CFReleaseSafe(modifyError);

                               return +1;
                           },
                           ^ CFIndex (SOSAccountRef account, CFErrorRef error)
                           {
                               CFErrorRef localError = NULL;
                               CFMutableDictionaryRef ourDatabase = SOSTestDataSourceGetDatabase(our_data_source);
                               is(CFDictionaryGetCount(ourDatabase), 0, "Database empty, we're synced");
                               
                               SOSTestDataSourceAddObject(our_data_source, firstObject, &localError);
                               CFReleaseNull(localError);
                               
                               SOSAccountSyncWithAllPeers(account, &localError);
                               CFReleaseNull(localError);
                               
                               pass("1");
                               return +1;
                           },
                           ^ CFIndex (SOSAccountRef account, CFErrorRef error)
                           {
                               pass("2");
                               
                               CFMutableDictionaryRef ourDatabase = SOSTestDataSourceGetDatabase(our_data_source);
                               is(CFDictionaryGetCount(ourDatabase), 1, "Still one element!");

                               return +1;
                           },
                           NULL);
    
    //------------------------------------------------------------------------
    //              START
    //------------------------------------------------------------------------
    
    ourWork = Alice ? aliceWorkToDo : bobWorkToDo;

    if (Alice) {
        /*
         Here we create a fresh circle, add and accept ourselves
         Then we post to the cloud and wait for a Circle changed notification
         */

        CFErrorRef modifyError = NULL;
        SOSAccountModifyCircle(our_account, circleKey, &modifyError, ^(SOSCircleRef circle) {
            CFErrorRef localError = NULL;

            ok(SOSCircleRequestAdmission(circle, user_privkey, our_full_peer_info, &localError), "Requested admission (%@) [error: %@]", our_peer_info, localError);
            ok(SOSCircleAcceptRequests(circle, user_privkey, our_full_peer_info, &localError), "Accepted self [Error: %@]", localError);

            CFReleaseSafe(localError);
        });
        CFReleaseSafe(modifyError);

    } else {
        // Tell alice we're set to go:
        if (foundNonce) {
            CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, sBobReady, foundNonce, NULL);
            SOSCloudKeychainPutObjectsInCloud(changes, global_queue, NULL);
            CFReleaseSafe(changes);
        } else {
            fail("No none found to start the handshake");
        }
    }
    dispatch_group_wait(work_group, DISPATCH_TIME_FOREVER);
    
    // We probably never get here since the program exits..
    
    CFReleaseNull(aliceWorkToDo);
    CFReleaseNull(bobWorkToDo);
    CFReleaseNull(our_peer_info);
    CFReleaseNull(foundNonce);
    
}

// MARK: ----- start of all tests -----
static void tests(bool Alice)
{
    dispatch_queue_t work_queue = dispatch_queue_create("NotificationQueue", DISPATCH_QUEUE_SERIAL); //;
    dispatch_group_t work_group = dispatch_group_create();

    // Prep the group for exitting the whole shebang.
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

static int kAliceTestCount = 32;
static int kBobTestCount = 30;

int sc_101_accountsync(int argc, char *const *argv)
{
    char *identity = NULL;
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
