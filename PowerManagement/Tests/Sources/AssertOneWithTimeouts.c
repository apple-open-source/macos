/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <stdlib.h>
#include <stdio.h>
#include "PMTestLib.h"

/***
    cc -g -o ./assert_settimeout assert_settimeout.c -arch i386 -framework IOKit -framework CoreFoundation
    
    This tool exercises the settimeout machinery in PM configd by creating/scheduling and removing timers.
    The tool exercises both assertion release cleanup, and dead client process.
    
    This tool detects configd crashes or incorrect return values.

***/

#define DO_ITERATIONS   10

int main()
{    
    IOReturn                ret = kIOReturnSuccess; 
    IOPMAssertionID         assertion_id[DO_ITERATIONS] = {kIOPMNullAssertionID};
    int                     didIterations = 0;
    int                     didFork = 0;


    int i=0;
    int failureLine = 0;

    PMTestInitialize("AssertOneWithTimeout: Create an assertion, set a timeout, let it expire. Wait 10 seconds.", "com.apple.iokit.powermanagement");
    PMTestLog("Performing %d assert, settimeout, release cycles.", DO_ITERATIONS);

    for (i=0; i<DO_ITERATIONS; i++)
    {
        ret = IOPMAssertionCreate(  kIOPMAssertionTypeNoDisplaySleep, 
                                    kIOPMAssertionLevelOn, 
                                    &assertion_id[i]);

        if(kIOReturnSuccess != ret) {
            PMTestFail("Error 0x%08x from IOPMAssertionCreate()\n", ret);
            failureLine = __LINE__;
            break;
        }

        ret = IOPMAssertionSetTimeout(assertion_id[i], 5.0);

        if(kIOReturnSuccess != ret) {
            PMTestFail("Error 0x%08x from IOPMAssertionSetTimeout()\n", ret);
            failureLine = __LINE__;
            break;
        }
        
    }
    
    sleep(10);

    /* 
     * 
     * WAIT FOR ASSERTIONS TO TIMEOUT 
     *
     */

    for (i=0; i<DO_ITERATIONS; i++)
    {
        IOPMAssertionRelease(assertion_id[i]);
    }

    didIterations = i;

    PMTestLog("IOPMAssertionSetTimeout stress test: Did %d iterations, %d of which were forked children.", didIterations, didFork);
    
    if (kIOReturnSuccess != ret) {
        PMTestFail("Failure - IOReturn value 0x%08x returned at %s:%d", ret, __FILE__, failureLine);
    } else {
        PMTestPass("settimeout stress test succeeded.");
    }
    
    // TODO: Test did configd crash?

    return 0;
}
