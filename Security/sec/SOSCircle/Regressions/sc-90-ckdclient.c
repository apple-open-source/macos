//
//  sc-90-ckdclient.c
//  sec
//
//  Created by John Hurley on 9/6/12.
//
//

#include <AssertMacros.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xpc/xpc.h>

#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <SecureObjectSync/SOSAccount.h>
#include <CKBridge/SOSCloudKeychainClient.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include "SOSCircle_regressions.h"
#include "SOSRegressionUtilities.h"


static bool verboseCKDClientDebugging = false;

static CFStringRef kTestKeyIDTimestamp = CFSTR("IDTimestamp");
static dispatch_group_t sDispatchGroup = NULL;
static dispatch_queue_t xpc_queue = NULL;

// MARK: ----- Test Data -----

static CFStringRef kTestKeyString = CFSTR("teststring");
static CFStringRef kTestKeyData = CFSTR("testdata");
static const UInt8 tdata[] = {0x01, 0x02, 0x03, 0x04, 'a', 'b', 'c'};
static CFStringRef kTestKeyArray = CFSTR("testarray");
static const CFStringRef adata[] = {CFSTR("A"), CFSTR("b"), CFSTR("C"), CFSTR("D")};
static const CFStringRef circleKeyStrings[] = {CFSTR("circleA"), CFSTR("circleB"), CFSTR("circleC"), CFSTR("circleD")};
static const CFStringRef keysWhenUnlockedKeyStrings[] = {CFSTR("foo"), CFSTR("bar"), CFSTR("baz")};

static CFDataRef testData = NULL;
static CFArrayRef testArray = NULL;
static CFArrayRef circleKeys = NULL;
static CFArrayRef keysWhenUnlocked = NULL;

static void initializeTestData(void)
{
    testData = CFDataCreate(kCFAllocatorDefault, tdata, sizeof(tdata)/sizeof(UInt8));
    testArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&adata, sizeof(adata)/sizeof(CFStringRef), &kCFTypeArrayCallBacks);
    // Register keys
    circleKeys = CFArrayCreate(kCFAllocatorDefault, (const void **)&circleKeyStrings, sizeof(circleKeyStrings)/sizeof(CFStringRef), &kCFTypeArrayCallBacks);
    keysWhenUnlocked = CFArrayCreate(kCFAllocatorDefault, (const void **)&keysWhenUnlockedKeyStrings, sizeof(keysWhenUnlockedKeyStrings)/sizeof(CFStringRef),
        &kCFTypeArrayCallBacks);
}

// MARK: ----- utilities -----

static void printTimeNow(const char *msg)
{
    if (verboseCKDClientDebugging)
    {
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        const char *nowstr = cfabsoluteTimeToString(now);
        if (nowstr)
        {
                printf("%s %s\n", nowstr, msg);
            free((void *)nowstr);
        }
    }
}

// MARK: ----- basicKVSTests -----

static bool testPostGet(CFStringRef key, CFTypeRef cfobj, dispatch_queue_t processQueue, dispatch_group_t dgroup)
{
    CFErrorRef error = NULL;
    bool result = false;
    CFTypeRef cfv = NULL;
    
    testPutObjectInCloud(key, cfobj, &error, dgroup, processQueue);
    CFTypeRef cfvalue = testGetObjectFromCloud(key, processQueue, dgroup);
    printTimeNow("finished getObjectFromCloud");
    if (!cfvalue)
        return false;
    if (CFGetTypeID(cfvalue)==CFDictionaryGetTypeID())
        cfv = CFDictionaryGetValue(cfvalue, key);
    else
        cfv = cfvalue;
    result = CFEqual(cfobj, cfv);
    return result;
}

static bool postIDTimestamp(dispatch_queue_t theq, dispatch_group_t dgroup)
{
    bool result = false;
    CFStringRef macaddr = myMacAddress();
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    const char *nowstr = cfabsoluteTimeToStringLocal(now);
    CFStringRef cfidstr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@: %@  %s"), kTestKeyIDTimestamp, macaddr ,nowstr);
    secerror("Setting %@ key: %@", kTestKeyIDTimestamp, cfidstr);
    result = testPostGet(kTestKeyIDTimestamp, cfidstr, theq, dgroup);

    if (nowstr)
        free((void *)nowstr);
    CFReleaseSafe(cfidstr);
    return result;
}


static const int kbasicKVSTestsCount = 10;
static void basicKVSTests(dispatch_group_t dgroup)
{
    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);

    printTimeNow("Start tests [basicKVSTests]");
 //   dispatch_group_enter(dgroup);
    
    // synchronize first to make sure we see cloud values
    ok(testSynchronize(generalq, dgroup), "test synchronize");

    // Next, get the TimeNow value, since this is the only one that differs from test to test (i.e. sc-90-ckdclient)
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    const char *nowstr = cfabsoluteTimeToStringLocal(now);
    CFStringRef cfstrtime = CFStringCreateWithCString(kCFAllocatorDefault, nowstr, kCFStringEncodingUTF8);
    ok(testGetObjectFromCloud(kTestKeyIDTimestamp, generalq, dgroup) != nil, "testGet for %@", kTestKeyIDTimestamp);
    
    ok(postIDTimestamp(generalq, dgroup), "testPostGet for %@", kTestKeyIDTimestamp);

    ok(testPostGet(kTestKeyString, CFSTR("test string"), generalq, dgroup), "testPostGet for CFStringRef");

    // Now the fixed values
    ok(testPostGet(kTestKeyString, CFSTR("test string"), generalq, dgroup), "testPostGet for CFStringRef");
    ok(testPostGet(kTestKeyData, testData, generalq, dgroup), "testPostGet for CFDataRef");
    ok(testPostGet(kTestKeyArray, testArray, generalq, dgroup), "testPostGet for CFDataRef");
    ok(testRegisterKeys(circleKeys, generalq, dgroup), "test register keys");
    
    ok(postIDTimestamp(generalq, dgroup), "testPostGet for %@", kTestKeyIDTimestamp);

    // Synchronize one more time before exit
    ok(testSynchronize(generalq, dgroup), "test synchronize");
/**/
    printTimeNow("End tests [basicKVSTests]");

//    dispatch_group_leave(dgroup);
    
    // Release test data
    CFRelease(testData);
    CFRelease(testArray);
    CFRelease(circleKeys);
    CFRelease(keysWhenUnlocked);
    CFRelease(cfstrtime);
    if (nowstr)
        free((void *)nowstr);
}

// MARK: ----- start of all tests -----

static int kTestTestCount = kbasicKVSTestsCount;
static void tests(void)
{
    SKIP: {
        skip("Skipping ckdclient tests because CloudKeychainProxy.xpc is not installed", kTestTestCount, XPCServiceInstalled());
 //       dispatch_queue_t dqueue = dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
            xpc_queue = dispatch_queue_create("sc_90_ckdclient", DISPATCH_QUEUE_SERIAL);

        sDispatchGroup = dispatch_group_create();
        
        initializeTestData();
        basicKVSTests(sDispatchGroup);
    }
}

int sc_90_ckdclient(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();

	return 0;
}


