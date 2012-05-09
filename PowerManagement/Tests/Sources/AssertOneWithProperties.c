/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "PMTestLib.h"


/*
 * IOPMAssertions test case.
 * Assert each known assertion once, then release it.
 * Then assert all assertions together, and release them.
 *
 */


int main()
{
    IOReturn            ret = 0;
    IOPMAssertionID     _id = 0;
    CFDictionaryRef     _props = 0;
    CFStringRef         keys[10];
    CFTypeRef           vals[10];
    int                 val = 0;
    
    ret = PMTestInitialize("PMAssertions", "com.apple.iokit.powertesting");
    if(kIOReturnSuccess != ret)
    {
        fprintf(stderr,"PMTestInitialize failed with IOReturn error code 0x%08x\n", ret);
        exit(-1);
    }
    
    
    val =           kIOPMAssertionLevelOn;
    keys[0] =       kIOPMAssertionTypeApplePushServiceTask;
    vals[0] =       CFNumberCreate(0, kCFNumberIntType, &val);
    
    keys[1] =       kIOPMAssertionHumanReadableReasonKey;
    vals[1] =       CFSTR("I did this because I had to.");
    
    val =           500; // seconds
    keys[2] =       kIOPMAssertionTimeoutKey;
    vals[2] =       CFNumberCreate(0, kCFNumberIntType, &val);
    
    keys[3] =       kIOPMAssertionLocalizationBundlePathKey;
    vals[3] =       CFSTR("com.apple.powermanagement");
    
    _props = CFDictionaryCreate(0, (const void **)keys, (const void **)vals, 4, 
                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    ret = IOPMAssertionCreateWithProperties(_props, &_id);
    
    CFRelease(_props);
    CFRelease(vals[0]);
    CFRelease(vals[2]);

    if (kIOReturnSuccess != ret) {
        PMTestFail("IOPMAssertionCreateWithProperties returns non-success 0x%08x\n", ret);
        exit(1);
    }

    ret = IOPMAssertionRelease(_id);
    
    if (kIOReturnSuccess != ret) {
        PMTestFail("IOPMAssertionRelease returns non-success 0x%08x\n", ret);
        exit(1);
    }
    
    PMTestPass("Successfully created and released via IOPMAssertionCrateWithProperties\n");
    
    return 0;
}
