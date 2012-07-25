/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
#include <IOKit/IOReturn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#import <IOKit/pwr_mgt/IOPMKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <stdlib.h>
#include <stdio.h>
#include "PMTestLib.h"


/***
    cc -g -o ./AssertTimeouts-TurnOff-9892470 AssertTimeouts-TurnOff-9892470.c -arch i386 -framework IOKit -framework CoreFoundation

xcrun -sdk iphoneos cc -g -o ./AssertTimeouts-TurnOff-9892470 AssertTimeouts-TurnOff-9892470.c -arch armv7 -framework IOKit -framework CoreFoundation \
    -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.Internal.sdk

    
    This test assumes it is the only code holding kIOPMAssertionTypePreventUserIdleSystemSleep assertions.

    <rdar://problem/9892470> Assertions with level 0 still keep the device awake
    Test originally written by Ziv W

***/



static CFStringRef  kUseType = kIOPMAssertionTypePreventUserIdleSystemSleep;


IOPMAssertionLevel getAssertionLevelForType(CFStringRef _type)
{
    CFDictionaryRef     assertionsStatus = NULL;
    CFNumberRef         levelNum;
    IOPMAssertionLevel  returnLevel = kIOPMAssertionLevelOff;
    IOReturn            ret = 0;
    
    if (!_type)
        return kIOPMAssertionLevelOff;
    
    ret = IOPMCopyAssertionsStatus(&assertionsStatus);
        
    if (assertionsStatus) {
        levelNum = CFDictionaryGetValue(assertionsStatus, _type);
        if (levelNum) {
            CFNumberGetValue(levelNum, kCFNumberIntType, &returnLevel);
        }
        CFRelease(assertionsStatus);
    }
        
    return returnLevel ? kIOPMAssertionLevelOn:kIOPMAssertionLevelOff;
}



int main (int argc, const char * argv[])
{
    int i=0;
    IOPMAssertionID assertions[30];
    // insert code here...
    char buf[100];
    CFStringGetCString(kUseType, buf, sizeof(buf), kCFStringEncodingUTF8);

    PMTestInitialize("Creating a gaggle of assertions with timeouts. Expect all assertions to timeout, and return assertion state to zero.\n", "com.apple.iokit.powermanagement");

    if (kIOPMAssertionLevelOn == getAssertionLevelForType(kUseType))
    {
        PMTestFail("*****PowerManagement expects to be the only process holding assertion %s. \nIt appears another process is holding this assertion. This may invalidate the results.", buf);
    } else {
        PMTestPass("Prerequisite met: Current value of %s is OFF.", buf);
    }

    PMTestLog("Creating %dx assertions. Please wait %d seconds.", 11, 45);

    for(i=0;i<10;i++){
        IOPMAssertionCreateWithDescription(kUseType, 
                    CFSTR("com.apple.PowerAssertionTaker-off"), NULL, NULL, NULL, 
                    (CFTimeInterval)i, kIOPMAssertionTimeoutActionTurnOff, &assertions[i]);
    }
    
    IOPMAssertionCreateWithDescription(kUseType, 
                    CFSTR("com.apple.PowerAssertionTaker-off-20secs"), NULL, NULL, NULL, 
                    (CFTimeInterval)30, kIOPMAssertionTimeoutActionTurnOff, &assertions[i++]);
    
    sleep(5);

    if (kIOPMAssertionLevelOn == getAssertionLevelForType(kUseType))
    {
        PMTestPass("Expected: Current value of %s is ON.", buf);
    } else {
        PMTestFail("Expected %s to be held ON before timeouts.", buf);
    }

    sleep(15);
    
    PMTestLog("10 timers should have timed out by now.");
    
    for(i=0;i<5;i++){
        IOPMAssertionRelease(assertions[i]);
    }

    PMTestLog("Just Released 5 of the timed-out assertions. 6 remain.");
    
    sleep(25);
    
    PMTestLog("45 seconds have passed. Expect assertion level to be Off.");
    
    if (kIOPMAssertionLevelOn != getAssertionLevelForType(kUseType))
    {
        PMTestPass("We created timeouts. We released some, we let some timeout. Current value of %s is OFF. Success.", buf);
    } else {
        PMTestFail("OOPS - the global assertion state for %s is ON!!!!!", buf);
    }

    return 0;
        // 
        // for(i=10;i<20;i++){
        //     IOPMAssertionCreateWithDescription(kUseType, 
        //                 CFSTR("com.apple.PowerAssertionTaker-release"), NULL, NULL, NULL, 
        //                 (CFTimeInterval)i-10, kIOPMAssertionTimeoutActionRelease, &assertions[i]);
        // }
        // 
        // sleep(20);
        // for(i=10;i<15;i++){
        //     IOPMAssertionRelease(assertions[i]);
        // 
        // }
        // 
        // for(i=20;i<30;i++){
        //     IOPMAssertionCreateWithDescription(kUseType, 
        //                 CFSTR("com.apple.PowerAssertionTaker-log"), NULL, NULL, NULL, 
        //                 (CFTimeInterval)i-20, kIOPMAssertionTimeoutActionLog, &assertions[i]);
        // }
        // 
        // sleep(20);
        // for(i=20;i<30;i++){
        //     IOPMAssertionRelease(assertions[i]);
        // 
        // }
        // 
        // sleep(1000);
}

