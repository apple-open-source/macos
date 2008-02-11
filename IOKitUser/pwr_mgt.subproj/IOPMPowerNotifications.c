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
 
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <IOKit/IOReturn.h>
#include "IOPMLibPrivate.h"

#include <notify.h>


#define kMySCIdentity           CFSTR("IOKit Power")


static CFStringRef scKeyForIOKitString(CFStringRef str)
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

static const char * notifyKeyForIOKitString(CFStringRef str)
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

IOReturn IOPMCopyCPUPowerStatus(CFDictionaryRef *cpuPowerStatus)
{
    SCDynamicStoreRef   store = NULL;
    CFStringRef         cpu_power_key = NULL;
    IOReturn            ret = kIOReturnError;
    
    if (!cpuPowerStatus) {
        ret = kIOReturnBadArgument;
        goto exit;
    }
    
    // Open connection to SCDynamicStore
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                kMySCIdentity, NULL, NULL);
    if (!store) {
        goto exit;
     }

    cpu_power_key = scKeyForIOKitString(CFSTR(kIOPMCPUPowerLimitsKey));
    if (!cpu_power_key) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    *cpuPowerStatus = isA_CFDictionary(SCDynamicStoreCopyValue(store, cpu_power_key));
    if (*cpuPowerStatus)
        ret = kIOReturnSuccess;
    else
        ret = kIOReturnNotFound;

exit:
    if (cpu_power_key)
        CFRelease(cpu_power_key);
    if (store)
        CFRelease(store);

    // Caller to release
    return ret;
}


IOReturn IOPMGetThermalWarningLevel(uint32_t *thermalLevel)
{
    SCDynamicStoreRef   store = NULL;
    CFStringRef         thermal_key = NULL;
    CFNumberRef         warning_level_num = NULL;
    IOReturn            ret = kIOReturnError;
    
    if (!thermalLevel) {
        ret =  kIOReturnBadArgument;
        goto exit;
    }
    
    // Open connection to SCDynamicStore
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                kMySCIdentity, NULL, NULL);
    if (!store) {
        goto exit;
     }

    thermal_key = scKeyForIOKitString(CFSTR(kIOPMThermalLevelWarningKey));
    if (!thermal_key) {
        ret = kIOReturnInternalError;
        goto exit;
    }
    
    warning_level_num = isA_CFNumber(SCDynamicStoreCopyValue(store, thermal_key));
        
    if (!warning_level_num) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    CFNumberGetValue(warning_level_num, kCFNumberIntType, thermalLevel);
    ret = kIOReturnSuccess;

exit:
    if (warning_level_num) 
        CFRelease(warning_level_num);
    if (thermal_key)
        CFRelease(thermal_key);
    if (store)
        CFRelease(store);
    return ret;
}



IOReturn IOPMSystemPowerEventOccurred(
    CFStringRef typeString, 
    CFTypeRef eventValue)
{
    IOReturn            ret = kIOReturnError;
    SCDynamicStoreRef   store = NULL;
    CFStringRef         writeToKey = NULL;
    CFDictionaryRef         cpu_data = NULL;
    CFMutableDictionaryRef  adjusted_cpu_data = NULL;
    
    const char  *notify3Key = NULL;
    
    if( !typeString 
        || !eventValue)
    {
        ret = kIOReturnBadArgument;
        goto exit;
    }
    
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                kMySCIdentity, NULL, NULL);
    if(!store) {
        goto exit;
     }

    // Find appropriate key into SCDynamicStore
    writeToKey = scKeyForIOKitString(typeString);

    // Write value into SCDynamicStore
    if(!SCDynamicStoreSetValue(store, writeToKey, eventValue)) 
    {
        if(kSCStatusAccessError == SCError())
            ret = kIOReturnNotPrivileged;
        else
            ret = kIOReturnInternalError;
        goto exit;
    }
    
    // Find appropriate BSD notify(3) key
    notify3Key = notifyKeyForIOKitString(typeString);

    ret = kIOReturnSuccess;
        
exit:
    if (store)
        CFRelease(store);
    if (writeToKey)
        CFRelease(writeToKey);
    if (cpu_data)
        CFRelease(cpu_data);
    if (adjusted_cpu_data)
        CFRelease(adjusted_cpu_data);
    
    // And generate the BSD notify(3) notification!
    if (notify3Key) 
        notify_post(notify3Key);
    
    return ret;
}
