/*
 *  sc-01-create.c
 *
 *  Created by Mitch Adler on 1/25/121.
 *  Copyright 2012 Apple Inc. All rights reserved.
 *
 */

//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_120_cloudcircle -v -- -i alice
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_120_cloudcircle -v -- -i bob

#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#include <Security/SecRandom.h>
#endif

#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSInternal.h>
#include "SOSTestDataSource.h"
#include <CKBridge/SOSCloudKeychainClient.h>

#include <utilities/debugging.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/iOSforOSX.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <notify.h>

#include "SOSRegressionUtilities.h"

#include "SOSCircle_regressions.h"
#include "SecureObjectSync/Imported/SOSCloudCircleServer.h"

#ifdef NO_SERVER
#if TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR
#include <securityd/SOSCloudCircleServer.h>
#else
#warning "NO_SERVER doesn't really work on OSX"
#endif
#endif

static CFStringRef kCircleName = CFSTR("Global Circle");

static CFStringRef sAliceReady = CFSTR("Alice-Ready");
static CFStringRef sBobReady = CFSTR("Bob-Ready");

static void SOSCloudKeychainClearAllSync()
{
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    SOSCloudKeychainClearAll(dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                             ^(CFDictionaryRef returnedValues __unused, CFErrorRef error)
                             {
                                 CFReleaseSafe(error);
                                 dispatch_semaphore_signal(waitSemaphore);
                             });
    dispatch_semaphore_wait(waitSemaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(waitSemaphore);
}

static void ClearAndSynchronize()
{
    dispatch_queue_t global_queue = dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_semaphore_t bob_ready_semaphore = dispatch_semaphore_create(0);
    
    const CFIndex nonceByteCount = 10;
    CFMutableDataRef nonce = CFDataCreateMutable(kCFAllocatorDefault, nonceByteCount);
    CFDataSetLength(nonce, nonceByteCount);
    SecRandomCopyBytes(kSecRandomDefault, CFDataGetLength(nonce), CFDataGetMutableBytePtr(nonce));
    
    dispatch_queue_t notification_queue = dispatch_queue_create("sync notification queue", DISPATCH_QUEUE_SERIAL);
    
    CloudItemsChangedBlock notification_block = ^ (CFDictionaryRef returnedValues)
    {
        CFRetain(returnedValues);
        dispatch_async(notification_queue, ^{
            CFTypeRef bobReadyValue = CFDictionaryGetValue(returnedValues, sBobReady);
            if (isData(bobReadyValue) && CFEqual(bobReadyValue, nonce)) {
                SOSCloudKeychainClearAllSync();

                pass("signalling");
                dispatch_semaphore_signal(bob_ready_semaphore);
            } else {
                pass("Ignoring change: %@", returnedValues);
            }
            CFReleaseSafe(returnedValues);
        });
    };
    
    CloudKeychainReplyBlock reply_block = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        if (CFDictionaryGetCount(returnedValues) != 0)
            notification_block(returnedValues);
    };
    
    pass("Clearing");
    SOSCloudKeychainClearAllSync();
    
    CFArrayRef bobKey = CFArrayCreateForCFTypes(kCFAllocatorDefault, sBobReady, NULL);
    SOSCloudKeychainRegisterKeysAndGet(bobKey, global_queue, reply_block, notification_block);
    
    CFStringRef description = SOSInterestListCopyDescription(bobKey);
    pass("%@", description);
    
    CFReleaseNull(description);
    CFReleaseNull(bobKey);
    
    CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, sAliceReady, nonce, NULL);
    SOSCloudKeychainPutObjectsInCloud(changes, global_queue, NULL);
    
    description = SOSChangesCopyDescription(changes, true);
    pass("%@", description);
    CFReleaseNull(description);
    
    pass("Waiting");
    dispatch_semaphore_wait(bob_ready_semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(bob_ready_semaphore);

    pass("Continuing");

    CFErrorRef error = NULL;
    CFArrayRef no_keys = CFArrayCreateForCFTypes(kCFAllocatorDefault, NULL);
    SOSCloudKeychainUpdateKeys(false, no_keys, no_keys, no_keys, &error);

    SOSCloudKeychainSetItemsChangedBlock(NULL);
    CFReleaseSafe(no_keys);
    CFReleaseSafe(error);

    
    CFReleaseNull(changes);
}

static void WaitForSynchronization()
{
    dispatch_queue_t global_queue = dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_semaphore_t alice_ready_semaphore = dispatch_semaphore_create(0);
    dispatch_queue_t notification_queue = dispatch_queue_create("sync notification queue", DISPATCH_QUEUE_SERIAL);
    
    __block CFDataRef foundNonce = NULL;
    CloudItemsChangedBlock notification_block = ^ (CFDictionaryRef returnedValues)
    {
        CFRetain(returnedValues);
        dispatch_async(notification_queue, ^{
            CFTypeRef aliceReadyValue = CFDictionaryGetValue(returnedValues, sAliceReady);
            if (isData(aliceReadyValue)) {
                foundNonce = (CFDataRef) aliceReadyValue;
                CFRetain(foundNonce);
                pass("signalling for: %@", foundNonce);
                
                dispatch_semaphore_signal(alice_ready_semaphore);
            }
            CFReleaseSafe(returnedValues);
        });
    };
    
    CloudKeychainReplyBlock reply_block = ^ (CFDictionaryRef returnedValues, CFErrorRef error)
    {
        if (CFDictionaryGetCount(returnedValues) != 0)
            notification_block(returnedValues);
    };
    
    CFArrayRef aliceKey = CFArrayCreateForCFTypes(kCFAllocatorDefault, sAliceReady, NULL);
    SOSCloudKeychainRegisterKeysAndGet(aliceKey, global_queue, reply_block, notification_block);
    
    CFStringRef description = SOSInterestListCopyDescription(aliceKey);
    pass("%@", description);
    CFReleaseNull(description);
    
    pass("Waiting");
    dispatch_semaphore_wait(alice_ready_semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(alice_ready_semaphore);
    pass("Continuing");

    CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, sBobReady, foundNonce, NULL);
    SOSCloudKeychainPutObjectsInCloud(changes, global_queue, NULL);
    CFReleaseSafe(changes);
    
    CFReleaseSafe(foundNonce);
    CFReleaseSafe(aliceKey);
}

static CFStringRef circleKey = CFSTR("Circle");

#ifdef NO_SERVER
static SOSDataSourceRef SetupTestFactory()
{
    static SOSDataSourceRef our_data_source;

    our_data_source = SOSTestDataSourceCreate();

    SOSKeychainAccountSetFactoryForAccount(^ {
        SOSDataSourceFactoryRef our_data_source_factory = SOSTestDataSourceFactoryCreate();
        SOSTestDataSourceFactoryAddDataSource(our_data_source_factory, circleKey, our_data_source);
        return our_data_source_factory;
    });
    
    return our_data_source;
}
#endif

static void PurgeAndReload()
{
#if 0
    CFErrorRef error = NULL;
    ok(SOSKeychainSaveAccountDataAndPurge(&error), "Purged");
    CFReleaseNull(error);
    
    ok(SOSKeychainAccountGetSharedAccount(), "Got Shared: %@", SOSKeychainAccountGetSharedAccount());
#endif
}

#ifndef NO_SERVER
#define SOSKeychainAccountGetSharedAccount() NULL
#endif

#define kAliceTestCount 100
static void AliceTests()
{
    plan_tests(kAliceTestCount);
    
    dispatch_semaphore_t notification_signal = dispatch_semaphore_create(0);
    int token;
    notify_register_dispatch(kSOSCCCircleChangedNotification, &token,
                             dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                             ^(int token) {
                                 dispatch_semaphore_signal(notification_signal);
                             });

    SOSCloudKeychainSetCallbackMethodXPC(); // call this first
    ClearAndSynchronize();

#ifdef NO_SERVER
    SOSDataSourceRef our_test_data_source = SetupTestFactory();
#endif
    CFErrorRef error = NULL;
    
    ok(SOSCCResetToOffering(&error), "Reset: %@ [%@]", SOSKeychainAccountGetSharedAccount(), error);
    CFReleaseNull(error);
    
    PurgeAndReload();
    
    CFArrayRef applicants = NULL;
    do {
        dispatch_semaphore_wait(notification_signal, DISPATCH_TIME_FOREVER);
        applicants = SOSCCCopyApplicantPeerInfo(&error);
        CFReleaseNull(error);
    } while (CFArrayGetCount(applicants) == 0);
    pass("Waited long enough");
    
    is(CFArrayGetCount(applicants), 1, "Bob should be applying");
    CFReleaseNull(error);

    ok(SOSCCAcceptApplicants(applicants, &error), "Accepted all applicants: %@", error);
    CFReleaseNull(error);
    CFReleaseSafe(applicants);
    
    PurgeAndReload();

    sleep(180);
    pass("Waited long enough again");

#ifdef NO_SERVER
    CFDictionaryRef data = SOSTestDataSourceGetDatabase(our_test_data_source);
    is(CFDictionaryGetCount(data), 1, "Should have gotten bob's one");
#endif
}

#define kBobTestCount 100
static void BobTests()
{
    plan_tests(kBobTestCount);
    
    dispatch_semaphore_t notification_signal = dispatch_semaphore_create(0);
    int token;
    notify_register_dispatch(kSOSCCCircleChangedNotification, &token,
                             dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                             ^(int token) {
                                 dispatch_semaphore_signal(notification_signal);
                             });

    SOSCloudKeychainSetCallbackMethodXPC(); // call this first
    WaitForSynchronization();
    
#ifdef NO_SERVER
    SOSDataSourceRef our_test_data_source = SetupTestFactory();
#endif

    CFErrorRef error = NULL;

#ifdef NO_SERVER
    SOSObjectRef firstObject = SOSDataSourceCreateGenericItem(our_test_data_source, CFSTR("1234"), CFSTR("service"));
    SOSTestDataSourceAddObject(our_test_data_source, firstObject, &error);
#endif

    CFReleaseNull(error);

    is(SOSCCThisDeviceIsInCircle(&error), kSOSCCCircleAbsent, "Shouldn't be a circle: %@", error);
    CFReleaseNull(error);
    
    PurgeAndReload();

    CFArrayRef peers = NULL;
    do {
        dispatch_semaphore_wait(notification_signal, DISPATCH_TIME_FOREVER);
        peers = SOSCCCopyPeerPeerInfo(&error);
        CFReleaseNull(error);
    } while (CFArrayGetCount(peers) == 0);

    pass("Peer arrived: %@", SOSKeychainAccountGetSharedAccount());

    is(CFArrayGetCount(peers), 1, "Only Alice should be there");
    CFReleaseSafe(peers);
    
    ok(SOSCCRequestToJoinCircle(&error), "Join circles: %@ [%@]", SOSKeychainAccountGetSharedAccount(), error);
    CFReleaseNull(error);
   
    PurgeAndReload();
    
    do {
        dispatch_semaphore_wait(notification_signal, DISPATCH_TIME_FOREVER);

    } while (SOSCCThisDeviceIsInCircle(&error) != kSOSCCInCircle);
    pass("Was admitted: %@ [%@]", SOSKeychainAccountGetSharedAccount(), error);
    CFReleaseNull(error);
    
    is(SOSCCThisDeviceIsInCircle(&error), kSOSCCInCircle, "Should be in circle: %@", error);
    CFReleaseNull(error);
    
    PurgeAndReload();
    
    // Sync
    sleep(120);
    pass("Waited long enough again: %@", SOSKeychainAccountGetSharedAccount());
    
#ifdef NO_SERVER
    CFDictionaryRef data = SOSTestDataSourceGetDatabase(our_test_data_source);
    is(CFDictionaryGetCount(data), 1, "Only have one");
#endif
}


static const struct option options[] =
{
	{ "identity",		optional_argument,	NULL, 'i' },
	{ }
};


int sc_120_cloudcircle(int argc, char *const *argv)
{
    char *identity = NULL;
    extern char *optarg;
    int arg, argSlot;
    
    bool isAlice = false;
    
    while (argSlot = -1, (arg = getopt_long(argc, (char * const *)argv, "i:vC", options, &argSlot)) != -1)
        switch (arg)
    {
        case 'i':
            identity = (char *)(optarg);
            break;
        default:
            fail("arg: %s", optarg);
            break;
    }
    
    if (identity)
    {
        if (!strcmp(identity, "alice"))
            isAlice = true;
    }

    if (isAlice)
        AliceTests();
    else
        BobTests();

	return 0;
    
}
