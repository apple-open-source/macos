/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include "PrivateLib.h"
#include "PMSystemEvents.h"

#ifndef _PMSystemEvents_h_
#define _PMSystemEvents_h_

#define kRootDomainThermalStatusKey "Power Status"
    
__private_extern__ void 
PMSystemEvents_prime(void)
{
    // Publish default settings
    PMSystemEventsRootDomainInterest();    
}

__private_extern__ void 
PMSystemEventsRootDomainInterest(void)
{
    IOReturn                ret;
    CFDictionaryRef         thermalStatus;
    CFStringRef             *keys = NULL;
    CFNumberRef             *vals = NULL;
    int                     count = 0;
    int                     i;

    // Read dictionary from IORegistry
    thermalStatus = IORegistryEntryCreateCFProperty(
                            getRootDomain(),
                            CFSTR(kRootDomainThermalStatusKey),
                            kCFAllocatorDefault,
                            kNilOptions);

    if(!thermalStatus)
        goto exit;
    
    // Publish dictionary in SCDynamicStore
    count = CFDictionaryGetCount(thermalStatus);
    keys = (CFStringRef *)malloc(count*sizeof(CFStringRef));
    vals = (CFNumberRef *)malloc(count*sizeof(CFNumberRef));
    if (!keys||!vals) 
        goto exit;
        
    CFDictionaryGetKeysAndValues(thermalStatus, 
                    (const void **)keys, (const void **)vals);
    
    for(i=0; i<count; i++) {

        ret = IOPMSystemPowerEventOccurred(keys[i], vals[i]);
        
        // IOPMSystemPowerEventOccurred may fail if configd is down
        // but we ignore those failure cases.
    }

exit:    
    if (keys)
        free(keys);
    if (vals)
        free(vals);
    if (thermalStatus)
        CFRelease(thermalStatus);
    return;
}


#endif _PMSystemEvents_h_
