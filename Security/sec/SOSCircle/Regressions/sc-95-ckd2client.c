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

static bool postIDTimestamp(dispatch_queue_t theq)
{
    __block bool result = false;
    CFStringRef macaddr = myMacAddress();
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    const char *nowstr = cfabsoluteTimeToStringLocal(now);
    CFStringRef cfidstr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@: %@  %s"), kTestKeyIDTimestamp, macaddr ,nowstr);
    secerror("Setting %@ key: %@", kTestKeyIDTimestamp, cfidstr);

    // create a dictionary with one key/value pair
    CFDictionaryRef objects = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kTestKeyIDTimestamp, cfidstr, NULL);
    SOSCloudKeychainPutObjectsInCloud(objects, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                                      ^(CFDictionaryRef returnedValues, CFErrorRef error) {
                                          if (error) {
                                              fail("Error putting: %@", error);
                                              CFReleaseSafe(error);
                                          }
                                      });

    CFMutableArrayRef keysToGet = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(keysToGet, kTestKeyIDTimestamp);
    SOSCloudKeychainGetObjectsFromCloud(keysToGet, theq, ^ (CFDictionaryRef returnedValues, CFErrorRef error)
        {
            CFStringRef returnedTimestamp = (CFStringRef)CFDictionaryGetValue(returnedValues, kTestKeyIDTimestamp);
            if (returnedTimestamp)
                result = CFEqual(returnedTimestamp, cfidstr);
        });

    if (nowstr)
        free((void *)nowstr);
    return result;
}

static const int kbasicKVSTestsCount = 1;
static void basicKVSTests2()
{
    dispatch_queue_t generalq = dispatch_queue_create("general", DISPATCH_QUEUE_SERIAL);

    const UInt8 tdata[] = {0x01, 0x02, 0x03, 0x04, 'a', 'b', 'c'};
    CFDataRef testData = CFDataCreate(kCFAllocatorDefault, tdata, sizeof(tdata)/sizeof(UInt8));
   
    const CFStringRef adata[] = {CFSTR("A"), CFSTR("b"), CFSTR("C"), CFSTR("D")};
    CFArrayRef testArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&adata, sizeof(adata)/sizeof(CFStringRef), &kCFTypeArrayCallBacks);

    // Register keys
    const CFStringRef circleKeyStrings[] = {CFSTR("circleA"), CFSTR("circleB"), CFSTR("circleC"), CFSTR("circleD")};
    CFArrayRef circleKeys = CFArrayCreate(kCFAllocatorDefault, (const void **)&circleKeyStrings, sizeof(circleKeyStrings)/sizeof(CFStringRef), &kCFTypeArrayCallBacks);

    const CFStringRef keysWhenUnlockedKeyStrings[] = {CFSTR("foo"), CFSTR("bar"), CFSTR("baz")};
    CFArrayRef keysWhenUnlocked = CFArrayCreate(kCFAllocatorDefault, (const void **)&keysWhenUnlockedKeyStrings, sizeof(keysWhenUnlockedKeyStrings)/sizeof(CFStringRef), &kCFTypeArrayCallBacks);

    printTimeNow("Start tests [basicKVSTests]");
    
    ok(postIDTimestamp(generalq), "testPostGet for %@", kTestKeyIDTimestamp);

    printTimeNow("Start tests [basicKVSTests2]");

    // Release test data
    CFRelease(testData);
    CFRelease(testArray);
    CFRelease(circleKeys);
    CFRelease(keysWhenUnlocked);
}

// MARK: ----- start of all tests -----

static const int kCloudTransportTestsCount = 0;
static int kTestTestCount = kbasicKVSTestsCount + kCloudTransportTestsCount;
static void tests(void)
{
    SKIP: {
        skip("Skipping ckdclient tests because CloudKeychainProxy.xpc is not installed", kTestTestCount, XPCServiceInstalled());
        basicKVSTests2();
  //      cloudTransportTests();
  sleep(15);    // on iOS, we need to hang around long enough for syncdefaultsd to see us
    }
}

int sc_95_ckd2client(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    tests();

	return 0;
}


