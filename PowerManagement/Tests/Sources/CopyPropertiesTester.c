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
    IOPMAssertionID     _id[10];
    
    ret = PMTestInitialize("CopyPropertiesExerciser", "com.apple.iokit.powertesting");
    if(kIOReturnSuccess != ret)
    {
        fprintf(stderr,"PMTestInitialize failed with IOReturn error code 0x%08x\n", ret);
        exit(-1);
    }
    
    int i;
    
    for (i=0; i<10; i++)
    {       
        ret = IOPMAssertionCreate(kIOPMAssertionTypePreventUserIdleSystemSleep, kIOPMAssertionLevelOn, &_id[i]);
    
        if (kIOReturnSuccess != ret) {
            PMTestFail("IOPMAssertionCreateWithProperties returns non-success 0x%08x\n", ret);
            exit(1);
        }
    }
    
    CFDictionaryRef     properties = NULL;
    for (i=0; i<10; i++)
    {
        properties = IOPMAssertionCopyProperties(_id[i]);
        
        
        /* Pure sanity check. */
        
        if (!properties) {
            PMTestFail("IOPMAssertionCopyProperties for id %ld return NULL\n", _id[i]);
            exit(1);
        }
        
        if (!CFDictionaryContainsKey(properties, kIOPMAssertionNameKey)
            || !CFDictionaryContainsKey(properties, kIOPMAssertionTypeKey)
            || !CFDictionaryContainsKey(properties, kIOPMAssertionLevelKey))
        {
            CFShow(properties);
            PMTestFail("IOPMAssertionCopyProperties doesn't contain required dictionary keys\n", _id[i]);
        }

        CFRelease(properties);
    }

    for (i=0; i<10; i++)
    {       
        ret = IOPMAssertionRelease(_id[i]);
    
        if (kIOReturnSuccess != ret) {
            PMTestFail("IOPMAssertionRelease returns non-success 0x%08x\n", ret);
            exit(1);
        }
    }
    
    PMTestPass("Created a handful of assertions. Every one of them had properties.\n");
    
    return 0;
}
