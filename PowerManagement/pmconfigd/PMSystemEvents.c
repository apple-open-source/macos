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

#include <notify.h>


#define kRootDomainThermalStatusKey "Power Status"

#define kMySCIdentity           CFSTR("IOKit Power")


static CFStringRef createSCKeyForIOKitString(CFStringRef str)
{
    CFStringRef     keyForString = NULL;

    if (CFEqual(str, CFSTR(kIOPMThermalLevelWarningKey))) 
    {
        keyForString = CFSTR("ThermalWarning");
    } else if (CFEqual(str, CFSTR(kIOPMCPUPowerLimitsKey))) {
        keyForString = CFSTR("CPUPower");
    }

    if (!keyForString)
        return NULL;

    return SCDynamicStoreKeyCreate(kCFAllocatorDefault, 
                        CFSTR("%@%@/%@"),
                        kSCDynamicStoreDomainState, 
                        CFSTR("/IOKit/Power"),
                        keyForString);
}

static const char * getNotifyKeyForIOKitString(CFStringRef str)
{
    if (CFEqual(str, CFSTR(kIOPMThermalLevelWarningKey))) 
    {
        return kIOPMThermalWarningNotificationKey;
    } else if (CFEqual(str, CFSTR(kIOPMCPUPowerLimitsKey))) 
    {
        return kIOPMCPUPowerNotificationKey;
    }
    return NULL;
}

    
__private_extern__ void 
PMSystemEvents_prime(void)
{
    // Publish default settings
    PMSystemEventsRootDomainInterest();    
}

__private_extern__ void 
PMSystemEventsRootDomainInterest(void)
{
    CFDictionaryRef         thermalStatus;
    CFMutableDictionaryRef  setTheseDSKeys = NULL;
    CFStringRef             *keys = NULL;
    CFNumberRef             *vals = NULL;
    SCDynamicStoreRef       store = NULL;
    int                     count = 0;
    int                     i;

    // Read dictionary from IORegistry
    thermalStatus = IORegistryEntryCreateCFProperty(
                            getRootDomain(),
                            CFSTR(kRootDomainThermalStatusKey),
                            kCFAllocatorDefault,
                            kNilOptions);

    if (!thermalStatus
        || !(count = CFDictionaryGetCount(thermalStatus)))
    {
        goto exit;
    }
    
    // Publish dictionary in SCDynamicStore
    keys = (CFStringRef *)malloc(count*sizeof(CFStringRef));
    vals = (CFNumberRef *)malloc(count*sizeof(CFNumberRef));
    if (!keys||!vals) 
        goto exit;
        
    CFDictionaryGetKeysAndValues(thermalStatus, 
                    (const void **)keys, (const void **)vals);
    
    setTheseDSKeys = CFDictionaryCreateMutable(0, count, 
                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!setTheseDSKeys)
        goto exit;
    
    for (i=0; i<count; i++) 
    {
        CFStringRef writeToKey = createSCKeyForIOKitString(keys[i]);
        if (writeToKey) {
            CFDictionarySetValue(setTheseDSKeys, writeToKey, vals[i]);
            CFRelease(writeToKey);
        }
    }

    store = SCDynamicStoreCreate(0, kMySCIdentity, NULL, NULL);
    if (!store)
        goto exit;

    SCDynamicStoreSetMultiple(store, setTheseDSKeys, NULL, NULL);

    for (i=0; i<count; i++)
    {
        const char *notify3Key = getNotifyKeyForIOKitString(keys[i]);
        if (notify3Key) 
            notify_post(notify3Key);
    }

exit:    
    if (keys)
        free(keys);
    if (vals)
        free(vals);
    if (setTheseDSKeys)
        CFRelease(setTheseDSKeys);
    if (store)
        CFRelease(store);
    if (thermalStatus)
        CFRelease(thermalStatus);
    return;
}


#endif //_PMSystemEvents_h_
