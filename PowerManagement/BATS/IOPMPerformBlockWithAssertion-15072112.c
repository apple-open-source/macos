/*
 * Copyright (c) 2014
  Apple Computer, Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <dispatch/dispatch.h>
#include <stdio.h>


/*

cc -o IOPMPerformBlockWithAssertion-15072112 \
        IOPMPerformBlockWithAssertion-15072112.c \
        -framework IOKit -framework CoreFoundation

Inspired by:
<rdar://problem/15072112> Sub-TLF: Investigating IOPMAssertion block API

*/



enum {
    kExpectOn,
    kExpectOff,
    kExpectNA
};
static CFStringRef kAType = kIOPMAssertNetworkClientActive;


static bool globalAssertionLevelFor(CFStringRef type, int expected, const char *print);
static void asyncPerformBlockWith(CFStringRef type, int assertionTimeoutSec, int blockDurationSec);

static void block10s_execTest(void);
//static void blockTimeoutExceedsBlockLifespanTest(void);

#define kNoTimeout  0

int main(int argc, char *argv[])
{
    globalAssertionLevelFor(kAType, kExpectNA, "Baseline");

    block10s_execTest();

    return 0;
}


static void block10s_execTest(void)
{
    asyncPerformBlockWith(kAType, kNoTimeout, 10);
    sleep(1);
    globalAssertionLevelFor(kAType, kExpectOn, "#1 block is holding assertion");
    sleep(3);
    globalAssertionLevelFor(kAType, kExpectNA, "#1 block should have exited by now");
    sleep(10);
    globalAssertionLevelFor(kAType, kExpectNA, "#1 block should have exited by now EXTRA EXTRA EXIT");
}


//static void blockTimeoutExceedsBlockLifespanTest(void)
//{
//    asyncPerformBlockWith(kAType, 7, 3);
//    sleep(1);
//    globalAssertionLevelFor(kAType, kExpectOn, "#2 block is holding assertion");
//    sleep(3);
//    globalAssertionLevelFor(kAType, kExpectOff, "#2 block exited; timeout still going");
//    sleep(5);
//    globalAssertionLevelFor(kAType, kExpectOff, "#2 block exited; timeout exited");
//}

static void asyncPerformBlockWith(CFStringRef type,
                                  int assertionTimeoutSec,
                                  int blockDurationSec)
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^() {
        IOReturn ret;
        CFMutableDictionaryRef          properties = NULL;

        properties = CFDictionaryCreateMutable(0, 3,
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);

        if (properties) {
            CFDictionarySetValue(properties,
                kIOPMAssertionNameKey,
                CFSTR("IOPMPerformBlockWithAssertion-15072112"));

            CFDictionarySetValue(properties,
                kIOPMAssertionTypeKey,
                type);

            if (assertionTimeoutSec)
            {
                CFTimeInterval timeint = (CFTimeInterval)assertionTimeoutSec;
                CFNumberRef timer;
                timer = CFNumberCreate(0, kCFNumberDoubleType, &timeint);
                CFDictionarySetValue(properties,
                                     kIOPMAssertionTimeoutKey,
                                     timer);
                CFRelease(timer);
            }

            int levelOn = kIOPMAssertionLevelOn;

            CFNumberRef numOn = CFNumberCreate(0, kCFNumberIntType, &levelOn);

            CFDictionarySetValue(properties,
                    kIOPMAssertionLevelKey,
                    numOn);
            CFRelease(numOn);
        }

        IOPMAssertionID aid;
        IOReturn r2 = IOPMAssertionCreateWithProperties(properties, &aid);
        printf("r2=0x%08x\n",r2);

        ret = IOPMPerformBlockWithAssertion(properties,
            ^(){
                globalAssertionLevelFor(type, kExpectOn, "Within performed block");
                sleep(blockDurationSec);
            });
        
        if (kIOReturnSuccess != ret) {
            printf ("[FAIL] IOPMPerformBlockWithAssertion returns 0x%08x\n", ret);
            exit(1);
        }
        CFRelease(properties);
    });
}


static bool globalAssertionLevelFor(CFStringRef type, int expected, const char *print)
{
    CFDictionaryRef         out = NULL;
    CFNumberRef             level = NULL;
    bool                    actual = false;
    int                     level_int = 0;

    IOPMCopyAssertionsStatus(&out);

    if (!out) {
        printf("[FAIL] NULL return from IOPMCopyAssertionsStatus");
        return false;
    }

    level = CFDictionaryGetValue(out, type);

    CFNumberGetValue(level, kCFNumberIntType, &level_int);

    actual = (1==level_int);

    if (print) {
        const char *expectedStr;
        const char *actualStr;
        if (kExpectOn == expected) {
            expectedStr = "On";
        } else if (kExpectOff == expected) {
            expectedStr = "Off";
        } else {
            expectedStr = "N/A";
        }
        actualStr = actual? "On":"Off";

        if ((kExpectNA == expected)
            || (actual && (kExpectOn == expected))
            || (!actual && (kExpectOff == expected)))
        {
            printf("[PASS] \"%s\" level is %s, expected to be %s\n",print, actualStr, expectedStr);
        } else {
            printf("[FAIL] \"%s\" level is %s, should be %s\n",print, actualStr, expectedStr);
        }
        fflush(stdout);
    }

    CFRelease(out);

    return (kIOPMAssertionLevelOn > 0);
}

