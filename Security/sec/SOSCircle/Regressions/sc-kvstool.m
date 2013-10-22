//
//  sc-kvstool.c
//  sec
//
//  Created by John Hurley 11/01/12.
//  Copyright 2012 Apple Inc. All rights reserved.
//


// Run on a device:
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_kvstool -v --  --dump
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_kvstool -v --  --clear
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_kvstool -v --  --putcircle
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_kvstool -v -- --direct  --dump
//      /AppleInternal/Applications/SecurityTests.app/SecurityTests sc_kvstool -v -- --direct --putcircle

#include <Foundation/Foundation.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSCloudCircle.h>

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

#include <notify.h>
#include <dispatch/dispatch.h>

#include "SOSCircle_regressions.h"
#include "CKDLocalKeyValueStore.h"
#include "SOSRegressionUtilities.h"
#include "SOSTestDataSource.h"
#include "SOSTestTransport.h"
#include "SOSCloudKeychainClient.h"

#import "SOSDirectCloudTransport.h"

// MARK: ----- Constants -----

static CFStringRef circleKey = CFSTR("Circle");

// MARK: ----- start of all tests -----

static void putCircleInCloud(SOSCircleRef circle, dispatch_queue_t work_queue, dispatch_group_t work_group)
{
    CFErrorRef error = NULL;
    CFDataRef newCloudCircleEncoded = SOSCircleCopyEncodedData(circle, kCFAllocatorDefault, &error);
    ok(newCloudCircleEncoded, "Encoded as: %@ [%@]", newCloudCircleEncoded, error);

    // Send the circle with our application request back to cloud
    testPutObjectInCloud(circleKey, newCloudCircleEncoded, &error, work_group, work_queue);
}

static void createAndPutInitialCircle(void)
{
    dispatch_queue_t work_queue = dispatch_queue_create("capic", DISPATCH_QUEUE_CONCURRENT);
    dispatch_group_t work_group = dispatch_group_create();

    CFErrorRef error = NULL;
    CFStringRef cflabel = CFSTR("TEST_USERKEY");
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    SOSCCRegisterUserCredentials(cflabel, cfpassword, &error);

    CFErrorRef localError = NULL;
    
    SOSDataSourceFactoryRef our_data_source_factory = SOSTestDataSourceFactoryCreate();
    SOSDataSourceRef our_data_source = SOSTestDataSourceCreate();
    SOSTestDataSourceFactoryAddDataSource(our_data_source_factory, circleKey, our_data_source);

    CFDictionaryRef gestalt = SOSCreatePeerGestaltFromName(CFSTR("Alice"));

    SOSAccountRef our_account = SOSAccountCreate(kCFAllocatorDefault, gestalt, our_data_source_factory,
        ^(CFArrayRef keys)
        {
            pass("SOSAccountKeyInterestBlock");
        },
        ^ bool (CFDictionaryRef keys, CFErrorRef *error)
        {
            pass("SOSAccountDataUpdateBlock");
            return false;
        },
        NULL);
    SOSAccountEnsureCircle(our_account, circleKey);

    SOSFullPeerInfoRef our_full_peer_info = SOSAccountGetMyFullPeerInCircleNamed(our_account, circleKey, &error);
    SOSPeerInfoRef our_peer_info = SOSFullPeerInfoGetPeerInfo(our_full_peer_info);
    CFRetain(our_peer_info);

    SOSCircleRef circle = SOSAccountFindCircle(our_account, circleKey);
    CFRetain(circle);
    
//    SecKeyRef user_privkey = SOSUserGetPrivKey(&localError);
    SecKeyRef user_privkey = NULL;  // TODO: this will not work
    ok(SOSCircleRequestAdmission(circle, user_privkey, our_full_peer_info, &localError), "Requested admission (%@)", our_peer_info);
    ok(SOSCircleAcceptRequests(circle, user_privkey, our_full_peer_info, &localError), "Accepted self");
    
    putCircleInCloud(circle, work_queue, work_group);
    pass("Put circle in cloud: (%@)", circle);
    
    CFRelease(circle);
}

static void postCircleChangeNotification()
{
    NSArray *keys = [NSArray arrayWithObjects:(id)circleKey, nil];
    NSDictionary* userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:
        keys, NSUbiquitousKeyValueStoreChangedKeysKey,
        [NSNumber numberWithInt:NSUbiquitousKeyValueStoreServerChange], NSUbiquitousKeyValueStoreChangeReasonKey,
        nil];
    [[NSNotificationCenter defaultCenter] postNotificationName:NSUbiquitousKeyValueStoreDidChangeExternallyNotification object:NULL userInfo:userInfo];
    [userInfo release];
}

static void requestSynchronization(dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    testSynchronize(processQueue, dgroup);
}

static void displayCircles(CFTypeRef objects)
{
    // SOSCCCopyApplicantPeerInfo doesn't display all info, e.g. in the case where we are not in circle
    CFDictionaryForEach(objects, ^(const void *key, const void *value)
    {
        if (SOSKVSKeyGetKeyType(key) == kCircleKey)
        {
            CFErrorRef localError = NULL;
            if (isData(value))
            {
                SOSCircleRef circle = SOSCircleCreateFromData(NULL, (CFDataRef) value, &localError);
                pass("circle: %@ %@", key, circle);
                CFReleaseSafe(circle);
            }
            else
                pass("non-circle: %@ %@", key, value);
        }
    });
}


static void dumpCircleInfo()
{
    CFErrorRef error = NULL;
    CFArrayRef applicantPeerInfos = NULL;
    CFArrayRef peerInfos = NULL;
    int idx;
    
    NSArray *ccmsgs = @[@"ParamErr", @"Error", @"InCircle", @"NotInCircle", @"RequestPending", @"CircleAbsent"  ];

    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    pass("ccstatus: %d, error: %@", ccstatus, error);
    idx = ccstatus-kSOSCCParamErr;
    if (0<=idx && idx<(int)[ccmsgs count])
        pass("ccstatus: %d (%@)", ccstatus, ccmsgs[idx]);

    // Now look at current applicants
    applicantPeerInfos = SOSCCCopyApplicantPeerInfo(&error);
    if (applicantPeerInfos)
    {
        pass("Applicants: %ld, error: %@", (long)CFArrayGetCount(applicantPeerInfos), error);
        CFArrayForEach(applicantPeerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            pass("Applicant: %@", peerName);
        });
    }
    else
        pass("No applicants, error: %@", error);
    
    
    peerInfos = SOSCCCopyPeerPeerInfo(&error);
    if (peerInfos)
    {
        pass("Peers: %ld, error: %@", (long)CFArrayGetCount(applicantPeerInfos), error);
        CFArrayForEach(peerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            pass("Peer: %@", peerName);
        });
    }
    else
        pass("No peers, error: %@", error);
}

// define the options table for the command line
static const struct option options[] =
{
	{ "verbose",		optional_argument,	NULL, 'v' },
	{ "dump",           optional_argument,	NULL, 'd' },
	{ "clear",          optional_argument,	NULL, 'C' },
	{ "putcircle",      optional_argument,	NULL, 'p' },
	{ "direct",         optional_argument,	NULL, 'D' },
	{ "notify",         optional_argument,	NULL, 'n' },
	{ "sync",           optional_argument,	NULL, 's' },
	{ "info",           optional_argument,	NULL, 'i' },
	{ }
};

static int kTestCount = 10;

static void usage(void)
{
    printf("Usage:\n");
    printf("    --dump [itemName]   Dump the contents of the kvs store (through proxy)\n");
    printf("    --clear             Clear the contents of the kvs store (through proxy)\n");
    printf("    --putcircle         Put a new circle into the kvs store (through proxy)\n");
    printf("    --direct            Go directly to KVS (bypass proxy)\n");
    printf("    --notify            Post a notification that the circle key has changed\n");
    printf("    --sync              Post a notification that the circle key has changed\n");
    printf("    --info              Dump info about circle and peer status\n");
}

enum kvscommands
{
    kCommandClear = 1,
    kCommandDump = 2,
    kCommandPutCircle = 3,
    kCommandNotify = 4,
    kCommandSynchronize = 5,
    kCommandInfo = 6,
    kCommandHelp
};

static int command = kCommandHelp;
static bool useDirect = false;

static void tests(const char *itemName)
{
    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);
    dispatch_group_t work_group = dispatch_group_create();

#if 0
    if (useDirect)
    {
        SOSCloudKeychainServerInit();
    }
#endif
    SOSCloudKeychainSetCallbackMethodXPC(); // call this first

    pass("Command: %d", command);
    switch (command)
    {
        case kCommandClear:
            ok(testClearAll(generalq, work_group), "test Clear All");
            break;
        case kCommandDump:
            {
                CFArrayRef keysToGet = NULL;
                if (itemName)
                {
                    CFStringRef itemStr = CFStringCreateWithCString(kCFAllocatorDefault, itemName, kCFStringEncodingUTF8);
                    pass("Retrieving   : %@", itemStr);
                    keysToGet = CFArrayCreateForCFTypes(kCFAllocatorDefault, itemStr);
                    CFReleaseSafe(itemStr);
                }
                CFTypeRef objects = testGetObjectsFromCloud(keysToGet, generalq, work_group);
                CFReleaseSafe(keysToGet);
                pass("   : %@", objects);
                displayCircles(objects);
            }
            break;
        case kCommandPutCircle:
            createAndPutInitialCircle();
            break;
        case kCommandNotify:
            postCircleChangeNotification();
            break;
        case kCommandSynchronize:
            requestSynchronization(generalq, work_group);
            break;
        case kCommandInfo:
            dumpCircleInfo();
            break;
        default:
        case kCommandHelp:
            usage();
            break;
    }
}

int sc_kvstool(int argc, char *const *argv)
{
    char *itemName = NULL;
//  extern int optind;
    extern char *optarg;
    int arg, argSlot;

    while (argSlot = -1, (arg = getopt_long(argc, (char * const *)argv, "ivChpdns", options, &argSlot)) != -1)
        switch (arg)
        {
        case 'd':
            itemName = (char *)(optarg);
            command = kCommandDump;
            break;
        case 'C':   // should set up to call testClearAll
            command = kCommandClear;
            break;
        case 'p':
            command = kCommandPutCircle;
            break;
        case 'n':
            command = kCommandNotify;
            break;
        case 's':
            command = kCommandSynchronize;
            break;
        case 'i':
            command = kCommandInfo;
            break;
        case 'D':
            useDirect = true;
            printf("Using direct calls to KVS\n");
            break;
        default:
            secerror("arg: %s", optarg);
            break;
        }

    plan_tests(kTestCount);

    secerror("Command: %d", command);
    printf("Command: %d\n", command);

	tests(itemName);

	return 0;
}
