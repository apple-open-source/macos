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
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include "PMTestLib.h"


int main(int argc, char **argv)
{
    int _pid = 0;
    int wait_status = 0;
    
    PMTestInitialize("Test One - Child Creates Assertion and Crashes", "com.apple.iokit.powermanagement.assertion_one_crash");

    _pid = fork();
    if (0 == _pid) {
        IOPMAssertionID     _id = 0;
        IOReturn            ret = 0;
        CFDictionaryRef     _props = NULL;

        CFStringRef         keys[10];
        CFTypeRef           vals[10];
        int                 val = 0;
        
        keys[0] =       kIOPMAssertionTypeKey;
        vals[0] =       kIOPMAssertionTypePreventUserIdleSystemSleep;

        keys[1] =       kIOPMAssertionHumanReadableReasonKey;
        vals[1] =       CFSTR("I did this because I had to.");
        
        val =           500; // seconds
        keys[2] =       kIOPMAssertionTimeoutKey;
        vals[2] =       CFNumberCreate(0, kCFNumberIntType, &val);

        keys[3] =       kIOPMAssertionLocalizationBundlePathKey;
        vals[3] =       CFSTR("com.apple.powermanagement");

        _props =        CFDictionaryCreate(0, (const void **)keys, (const void **)vals, 4, 
                                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                
        ret = IOPMAssertionCreateWithProperties(_props, &_id);
        
        CFRelease(_props);
        CFRelease(vals[0]);
        CFRelease(vals[2]);        
        
        if (ret != kIOReturnSuccess) {
            PMTestFail("IOPMAssertionCreate() failed, error = 0x%08x", ret);
            exit(1);
        }
        exit(0);

    }
    
    if (-1 == _pid) {
        PMTestFail("Fork failed (errno=%d)\n", errno);
        return 0;
    }
    
    
    _pid = wait4(_pid, &wait_status, 0, NULL);
    if ((_pid == -1) ||
        !WIFEXITED(wait_status) ||
        (WEXITSTATUS(wait_status) != 0)) {
        PMTestFail("child process did not exit cleanly");
        return 0;
    }
    
    PMTestPass("Child exited cleanly; however this test does not verify that PM reaped child's assertion.\n");

    return 0;
}
