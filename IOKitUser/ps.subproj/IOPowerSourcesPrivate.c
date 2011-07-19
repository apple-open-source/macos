/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOCFSerialize.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>
#include <servers/bootstrap.h>

#include "IOSystemConfiguration.h"
#include "IOPowerSources.h"
#include "IOPowerSourcesPrivate.h"
#include "IOPSKeys.h"
#include "powermanagement.h"

/* IOPSCopyInternalBatteriesArray
 *
 * Argument: 
 *	CFTypeRef power_sources: The return value from IOPSCoyPowerSourcesInfo()
 * Return value:
 * 	CFArrayRef: all the batteries we found
 *			NULL if none are found
 */
CFArrayRef
IOPSCopyInternalBatteriesArray(CFTypeRef power_sources)
{
    CFArrayRef			array = isA_CFArray(IOPSCopyPowerSourcesList(power_sources));
    CFMutableArrayRef   ret_arr;
    CFTypeRef			name = NULL;
    CFDictionaryRef		ps;
    CFStringRef			transport_type;
    int				    i, count;

    if(!array) return NULL;
    count = CFArrayGetCount(array);
    name = NULL;

    ret_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(!ret_arr) goto exit;

    for(i=0; i<count; i++) {
        name = CFArrayGetValueAtIndex(array, i);
        ps = isA_CFDictionary(IOPSGetPowerSourceDescription(power_sources, name));
        if(ps) {
            transport_type = isA_CFString(CFDictionaryGetValue(ps, CFSTR(kIOPSTransportTypeKey)));
            if(transport_type && CFEqual(transport_type, CFSTR(kIOPSInternalType)))
            {
                CFArrayAppendValue(ret_arr, name);
            }
        }
    }

    if(0 == CFArrayGetCount(ret_arr)) {
        CFRelease(ret_arr);
        ret_arr = NULL;
    }

exit:
    CFRelease(array);
    return ret_arr;
}


/* IOPSCopyUPSArray
 *
 * Argument: 
 *	CFTypeRef power_sources: The return value from IOPSCoyPowerSourcesInfo()
 * Return value:
 * 	CFArrayRef: all the UPS's we found
 *			NULL if none are found
 */
 CFArrayRef
IOPSCopyUPSArray(CFTypeRef power_sources)
{
    CFArrayRef			array = isA_CFArray(IOPSCopyPowerSourcesList(power_sources));
    CFMutableArrayRef   ret_arr;
    CFTypeRef			name = NULL;
    CFDictionaryRef		ps;
    CFStringRef			transport_type;
    int				    i, count;

    if(!array) return NULL;
    count = CFArrayGetCount(array);
    name = NULL;

    ret_arr = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if(!ret_arr) goto exit;

    // Iterate through power_sources
    for(i=0; i<count; i++) {
        name = CFArrayGetValueAtIndex(array, i);
        ps = isA_CFDictionary(IOPSGetPowerSourceDescription(power_sources, name));
        if(ps) {
            transport_type = isA_CFString(CFDictionaryGetValue(ps, CFSTR(kIOPSTransportTypeKey)));
            if(transport_type && ( CFEqual(transport_type, CFSTR(kIOPSSerialTransportType)) ||
                CFEqual(transport_type, CFSTR(kIOPSUSBTransportType)) ||
                CFEqual(transport_type, CFSTR(kIOPSNetworkTransportType)) ) )
            {
                CFArrayAppendValue(ret_arr, name);
            }
        }
    }

    if(0 == CFArrayGetCount(ret_arr)) {
        CFRelease(ret_arr);
        ret_arr = NULL;
    }

exit:
    CFRelease(array);
    return ret_arr;
}

/* IOPSGetActiveUPS
 *
 * Argument: 
 *	CFTypeRef power_sources: The return value from IOPSCoyPowerSourcesInfo()
 * Return value:
 * 	CFTypeRef (actually a CFStringRef): the UPS we're paying attention to, since we only
 *              support one UPS at a time.
 *			NULL if none are found
 */
CFTypeRef  
IOPSGetActiveUPS(CFTypeRef ps_blob)
{
    CFTypeRef       ret_ups;
    CFArrayRef      ups_arr;
    
    ups_arr = IOPSCopyUPSArray(ps_blob);
    if(!ups_arr)
    {
        return NULL;    
    }

    ret_ups = CFArrayGetValueAtIndex(ups_arr, 0);
    
    CFRelease(ups_arr);

    return ret_ups;
}

/* IOPSGetActiveBattery
 *
 * Argument: 
 *	CFTypeRef power_sources: The return value from IOPSCoyPowerSourcesInfo()
 * Return value:
 * 	CFTypeRef (actually a CFStringRef): the UPS we're paying attention to, since we only
 *              support one UPS at a time.
 *			NULL if none are found
 */
CFTypeRef  
IOPSGetActiveBattery(CFTypeRef ps_blob)
{
    CFTypeRef       ret_ups;
    CFArrayRef      ups_arr;
    
    ups_arr = IOPSCopyInternalBatteriesArray(ps_blob);
    if(!ups_arr)
    {
        return NULL;    
    }

    ret_ups = CFArrayGetValueAtIndex(ups_arr, 0);
    
    CFRelease(ups_arr);

    return ret_ups;
}

static CFStringRef getPowerSourceState(CFTypeRef blob, CFTypeRef id)
{
    CFDictionaryRef the_dict = IOPSGetPowerSourceDescription(blob, id);
    return CFDictionaryGetValue(the_dict, CFSTR(kIOPSPowerSourceStateKey));
}

/* _getProvidingPowerSourceType
 * Argument: 
 *  ps_blob: as returned from IOPSCopyPowerSourcesInfo()
 * Returns: 
 *  The current system power source.
 *  CFSTR("AC Power"), CFSTR("Battery Power"), CFSTR("UPS Power")
 */
CFStringRef IOPSGetProvidingPowerSourceType(CFTypeRef ps_blob)
{
    // CFArrayRef      batt_arr = NULL;
    // CFArrayRef      ups_arr = NULL;
    CFTypeRef       the_ups = NULL;
    CFTypeRef       the_batt = NULL;
    CFStringRef     ps_state = NULL;


    if(kCFBooleanFalse == IOPSPowerSourceSupported(ps_blob, CFSTR(kIOPMBatteryPowerKey)))
    {
        if(kCFBooleanFalse == IOPSPowerSourceSupported(ps_blob, CFSTR(kIOPMUPSPowerKey))) {
            // no batteries, no UPS -> AC Power
            return CFSTR(kIOPMACPowerKey);
        } else {
            // optimization opportunity: needless loops inside IOPSGetActiveUPS
            the_ups = IOPSGetActiveUPS(ps_blob);
            if(!the_ups) return CFSTR(kIOPMACPowerKey);
            ps_state = getPowerSourceState(ps_blob, the_ups);
            if(ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue)))
            {
                // no batteries, yes UPS, UPS is running off of AC power -> AC Power
                return CFSTR(kIOPMACPowerKey);
            } else if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))
            {
                // no batteries, yes UPS, UPS is running off of Battery power -> UPS Power
                return CFSTR(kIOPMUPSPowerKey);
            }
            
        }
        // Error in the data we were passed
        return CFSTR(kIOPMACPowerKey);
    } else {
        // Optimization opportunity: needless loops inside IOPSGetActiveBattery
        the_batt = IOPSGetActiveBattery(ps_blob);
        if(!the_batt) return CFSTR(kIOPMACPowerKey);
        ps_state = getPowerSourceState(ps_blob, the_batt);
        if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))
        {
            // Yes batteries, yes running on battery power -> Battery power
            return CFSTR(kIOPMBatteryPowerKey);
        } else {
            // batteries are on AC power. let's check UPS.
            // optimize.
            if(kCFBooleanFalse == IOPSPowerSourceSupported(ps_blob, CFSTR(kIOPMUPSPowerKey)))
            {
                // yes batteries on AC power, no UPS -> AC Power
                return CFSTR(kIOPMACPowerKey);
            } else {
                the_ups = IOPSGetActiveUPS(ps_blob);
                if(!the_ups) return CFSTR(kIOPMACPowerKey);
                ps_state = getPowerSourceState(ps_blob, the_ups);
                if(ps_state && CFEqual(ps_state, CFSTR(kIOPSBatteryPowerValue)))                
                {
                    // yes batteries on AC power, UPS is on battery power -> UPS Power
                    return CFSTR(kIOPMUPSPowerKey);
                } else if(ps_state && CFEqual(ps_state, CFSTR(kIOPSACPowerValue)))
                {
                    // yes batteries on AC Power, UPS is on AC Power -> AC Power
                    return CFSTR(kIOPMACPowerKey);
                }
            }
        }
        // Error in the data we were passed.
//        syslog(LOG_INFO, "PowerManagement: unexpected point 3 reached in _getProvidingPowerSourceType\n");
    }

    // Should not reach this point. Return something safe.
    return CFSTR(kIOPMACPowerKey);
}

/***
 * int powerSourceSupported(CFStringRef)
 * takes: CFSTR of kIOPMACPowerKey, kIOPMBatteryPowerKey, kIOPMUPSPowerKey
 * returns true if this machine supports (has) that power type.
 */
CFBooleanRef IOPSPowerSourceSupported(CFTypeRef ps_blob, CFStringRef ps_type)
{

    if(!isA_CFString(ps_type)) 
    {
        return kCFBooleanFalse;
    } 
    
    if(CFEqual(ps_type, CFSTR(kIOPMACPowerKey))) 
    {
        return kCFBooleanTrue;
    }
    
#if defined (__i386__) || defined (__x86_64__) 
    if (CFEqual(ps_type, CFSTR(kIOPMBatteryPowerKey))) 
    {
        CFBooleanRef            ret = kCFBooleanFalse;
        io_registry_entry_t     platform = IO_OBJECT_NULL;
        CFDataRef               systemTypeData = NULL;
        int                     *systemType = 0;
        
        platform = IORegistryEntryFromPath(kIOMasterPortDefault, 
                                    kIODeviceTreePlane ":/");
        
        if (IO_OBJECT_NULL == platform) {
            return kCFBooleanFalse;
        }
        
        systemTypeData = (CFDataRef)IORegistryEntryCreateCFProperty(
                                platform, CFSTR("system-type"),
                                kCFAllocatorDefault, kNilOptions);
        if (systemTypeData 
            && (CFDataGetLength(systemTypeData) > 0)
            && (systemType = (int *)CFDataGetBytePtr(systemTypeData))
            && (2 == *systemType))
        {
            ret = kCFBooleanTrue;        
        } else {
            ret = kCFBooleanFalse;
        }
        if (systemTypeData)
            CFRelease(systemTypeData);
        IOObjectRelease(platform);
        return ret;
    }
#else
    if (ps_blob 
       && CFEqual(ps_type, CFSTR(kIOPMBatteryPowerKey))
       && IOPSGetActiveBattery(ps_blob))
    {
        return kCFBooleanTrue;
    }
#endif
    
    if (ps_blob 
       && CFEqual(ps_type, CFSTR(kIOPMUPSPowerKey))
       && IOPSGetActiveUPS(ps_blob))
    {
        return kCFBooleanTrue;
    }

    return kCFBooleanFalse;
}

/******************************************************************************

    IOPSPowerSourceID - publishing a power source from user space

 ******************************************************************************/


static IOReturn _pm_connect(mach_port_t *newConnection)
{
    kern_return_t       kern_result = KERN_SUCCESS;
    
    if(!newConnection) {
        return kIOReturnBadArgument;
    }

    kern_result = bootstrap_look_up(bootstrap_port, kIOPMServerBootstrapName, newConnection);
    if(KERN_SUCCESS != kern_result) {
        return kIOReturnError;
    }
    return kIOReturnSuccess;
}

static IOReturn _pm_disconnect(mach_port_t connection)
{
    if(!connection) {
        return kIOReturnBadArgument;
    }

    mach_port_deallocate(mach_task_self(), connection);

    return kIOReturnSuccess;
}


/* OpaqueIOPSPowerSourceID
 * As typecast in the header:
 * typedef struct OpaqueIOPSPowerSourceID *IOPSPowerSourceID;
 */
struct OpaqueIOPSPowerSourceID {
    CFMachPortRef   configdConnection;
    CFStringRef     scdsKey;
};

#define kMaxPSTypeLength        25
#define kMaxSCDSKeyLength       1024

IOReturn IOPSCreatePowerSource(
    IOPSPowerSourceID *outPS,
    CFStringRef powerSourceType)
{
    IOPSPowerSourceID           newPS = NULL;
    mach_port_t                 pm_server = MACH_PORT_NULL;

    char                        psType[kMaxPSTypeLength];

    char                        scdsKey[kMaxSCDSKeyLength];

    mach_port_t                 local_port = MACH_PORT_NULL;
    int                         return_code = kIOReturnSuccess;
    kern_return_t               kr = KERN_SUCCESS;
    IOReturn                    ret = kIOReturnError;

    if (!powerSourceType || !outPS)
        return kIOReturnBadArgument;

    // newPS - This tracking structure must be freed by IOPSReleasePowerSource()

    newPS = calloc(1, sizeof(struct OpaqueIOPSPowerSourceID));

    if (!newPS)
        return kIOReturnVMError;

    // We allocate a port in our namespace, and pass it to configd. 
    // If this process dies, configd will cleanup  our unattached power sources.

    newPS->configdConnection = CFMachPortCreate(kCFAllocatorDefault, NULL, NULL, NULL);

    if (newPS->configdConnection)
        local_port = CFMachPortGetPort(newPS->configdConnection);

    if (MACH_PORT_NULL == local_port) {
        ret = kIOReturnInternalError;
        goto fail;
    }

    if (!CFStringGetCString(powerSourceType, psType, sizeof(psType), kCFStringEncodingMacRoman)) {
        ret = kIOReturnBadMedia;
        goto fail;
    }
    
    ret = _pm_connect(&pm_server);
    if(kIOReturnSuccess != ret) {
        ret = kIOReturnNotOpen;
        goto fail;
    }
    
    kr = io_pm_new_pspowersource(
            pm_server, 
            local_port,     // in: mach port
            psType,         // in: type name
            scdsKey,        // out: SCDS key
            &return_code);  // out: Return code

    if(KERN_SUCCESS != kr) {
        ret = kIOReturnNotResponding;
        goto fail;
    }

    _pm_disconnect(pm_server);

    newPS->scdsKey = CFStringCreateWithCString(0, scdsKey, kCFStringEncodingUTF8);

// success:
    *outPS = newPS;
    return (IOReturn)return_code;

fail:
    if (newPS)
        free(newPS);
    if (IO_OBJECT_NULL != pm_server)
        _pm_disconnect(pm_server);
    *outPS = NULL;

    return ret;
}

IOReturn IOPSSetPowerSourceDetails(
    IOPSPowerSourceID whichPS, 
    CFDictionaryRef details)
{
    IOReturn                ret = kIOReturnSuccess;
    char                    dskey_str[kMaxSCDSKeyLength];
    CFDataRef               flatDetails;
    mach_port_t             pm_server = MACH_PORT_NULL;

    if (!whichPS || !isA_CFString(whichPS->scdsKey) || !isA_CFDictionary(details))
        return kIOReturnBadArgument;

    CFStringGetCString(whichPS->scdsKey, dskey_str, sizeof(dskey_str), kCFStringEncodingUTF8);

    flatDetails = IOCFSerialize(whichPS, 0);
    if (!flatDetails)
        goto exit;

    ret = _pm_connect(&pm_server);
    if(kIOReturnSuccess != ret) {
        ret = kIOReturnNotOpen;
        goto exit;
    }
    
    // Pass the details off to powerd 
    io_pm_update_pspowersource(pm_server, dskey_str, 
                CFDataGetBytePtr(flatDetails), CFDataGetLength(flatDetails), (int *)&ret);

    _pm_disconnect(pm_server);

exit:
    if (flatDetails)
        CFRelease(flatDetails);
    return ret;
}

IOReturn IOPSReleasePowerSource(
    IOPSPowerSourceID whichPS)
{
    if (!whichPS)
        return kIOReturnBadArgument;

    if (whichPS->configdConnection)
    {
        CFRelease(whichPS->configdConnection);
    }

    if (whichPS->scdsKey)
        CFRelease(whichPS->scdsKey);

    free(whichPS);
    return kIOReturnSuccess;
}
