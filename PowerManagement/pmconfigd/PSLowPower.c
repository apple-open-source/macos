/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 29-Aug-02 ebold created
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDPlugin.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/IOMessage.h>
#include "PSLowPower.h"

// Keep one open handle to the SCDynamicStore
static SCDynamicStoreRef		store;
 
__private_extern__ void PSLowPower_prime(void)
{
    store = SCDynamicStoreCreate(NULL, CFSTR("PM configd plugin"), NULL, NULL);
    return; 
}
 
 /* isUPSPresent
 *
 * Argument: 
 *	CFTypeRef power_sources: The return value from IOPSCoyPowerSourcesInfo()
 * Return value:
 * 	CFTypeRef: handle for the system-power-managing UPS
 *			NULL if no UPS is present
 */
static CFTypeRef
isUPSPresent(CFTypeRef power_sources)
{
    CFArrayRef			array = isA_CFArray(IOPSCopyPowerSourcesList(power_sources));
    CFTypeRef			name = NULL;
    CFTypeRef           name_ret = NULL;
    CFDictionaryRef		ps;
    CFStringRef			transport_type;
    int				i, count;

    if(!array) return NULL;
    count = CFArrayGetCount(array);
    name = NULL;

    // Iterate through power_sources
    for(i=0; i<count; i++)
    {
        name = CFArrayGetValueAtIndex(array, i);
        ps = isA_CFDictionary(IOPSGetPowerSourceDescription(power_sources, name));
        if(ps) {
            // Return the first power source that's not an internal battery
            // This assumes that available power sources are "Internal" battery or "UPS" "Serial" or "Network"
            // If not an internal battery it must be a UPS
            transport_type = isA_CFString(CFDictionaryGetValue(ps, CFSTR(kIOPSTransportTypeKey)));
            if(transport_type && !CFEqual(transport_type, CFSTR(kIOPSInternalType)))
            {
                name_ret = name;
                CFRetain(name_ret);
                break; // out of for loop
            }
        }
    }
    CFRelease(array);
    return name_ret;
}

/* isInternalBatteryPresent
 *
 * Argument: 
 *	CFTypeRef power_sources: The return value from IOPSCoyPowerSourcesInfo()
 * Return value:
 * 	CFTypeRef: handle for the (or one of several) internal battery
 *			NULL if no UPS is present
 */
 static CFTypeRef
isInternalBatteryPresent(CFTypeRef power_sources)
{
    CFArrayRef			array = isA_CFArray(IOPSCopyPowerSourcesList(power_sources));
    CFTypeRef			name = NULL;
    CFTypeRef           name_ret = NULL;
    CFDictionaryRef		ps;
    CFStringRef			transport_type;
    int				i, count;

    if(!array) return NULL;
    count = CFArrayGetCount(array);
    name = NULL;

    // Iterate through power_sources
    for(i=0; i<count; i++)
    {
        name = CFArrayGetValueAtIndex(array, i);
        ps = isA_CFDictionary(IOPSGetPowerSourceDescription(power_sources, name));
        if(ps) {
            // Return the first power source that is an internal battery
            transport_type = isA_CFString(CFDictionaryGetValue(ps, CFSTR(kIOPSTransportTypeKey)));
            if(transport_type && CFEqual(transport_type, CFSTR(kIOPSInternalType)))
            {
                name_ret = name;
                CFRetain(name_ret);
                break; // out of for loop
            }
        }
    }
    CFRelease(array);
    return name_ret;
}


/* weManageUPSPower
 *
 * Determines whether X Power Management should do the emergency shutdown when low on UPS power.
 * OS X should NOT manage low power situations if another third party application has already claimed
 * that emergency shutdown responsibility.
 *
 * Return value:
 * 	CFTypeRef: handle for the system-power-managing UPS
 *			NULL if no UPS is present
 */
static bool
weManageUPSPower(void)
{
    static CFStringRef  ups_claimed = NULL;
    CFTypeRef		temp;

    if(!ups_claimed) {
        ups_claimed = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@"), kSCDynamicStoreDomainState, CFSTR(kIOPSUPSManagementClaimed));
    }

    // Check for existence of "UPS Management claimed" key in SCDynamicStore
    if(temp = SCDynamicStoreCopyValue(store, ups_claimed)) {
    	// Someone else has claimed it. We don't manage UPS power.
        if(isA_CFBoolean(temp)) CFRelease(temp);
        return false;
    }
    // Yes, we manage
    return true;
}

static void 
doPowerEmergencyShutdown(void)
{
    int ret;
    char *null_args[2];
    
    syslog(LOG_INFO, "Performing emergency UPS low power shutdown now");

    null_args[0] = (char *)"";
    null_args[1] = NULL;
    ret = _SCDPluginExecCommand(0, 0, 0, 0, "/sbin/halt", null_args);

}

/* PowerSourcesHaveChanged
 *
 * Is the handler that gets notified when power source (battery or UPS)
 * state changes. We might respond to this by posting a user notification
 * or performing emergency sleep/shutdown.
 */
__private_extern__ void
PSLowPowerPSChange(CFTypeRef ps_blob) 
{
    CFTypeRef			ups = NULL;
    CFTypeRef           batt0 = NULL;
    CFDictionaryRef		ups_info;
    int				t1, t2;
    CFNumberRef			n1, n2;
    double			percent_remaining;
    double			shutdown_threshold;
    CFBooleanRef		isPresent;
    CFStringRef			ups_power_source;
    
    //syslog(LOG_INFO, "PMCFGD: PowerSource state has changed");
    // *** Inspect battery power levels

        // Compare time remaining/power level to warning threshold
        // NOT IMPLEMENTED
    
    // Should Power Management handle UPS warnings and emergency shutdown?
    if(weManageUPSPower())
    {
        //syslog(LOG_INFO, "We manage UPS power");

        // *** Inspect UPS power levels
        // We assume we're only dealing with 1 UPS
        if(ups = isUPSPresent(ps_blob))
        {
            //syslog(LOG_INFO, "Detected UPS %s", CFStringGetCStringPtr((CFStringRef)ups, kCFStringEncodingMacRoman));
            
            // Is an internal battery present?
            if(batt0 = isInternalBatteryPresent(ps_blob))
            {
                // Do not do UPS shutdown if internal battery is present.
                // Internal battery may still be providing power. 
                // Don't do any further UPS shutdown processing.
                // PMU will cause an emergency sleep when the battery runs out - we fall back on that
                // in the battery case.
                //syslog(LOG_INFO, "bail 0");
                goto _exit_PowerSourcesHaveChanged_;                
            }
            
            ups_info = isA_CFDictionary(IOPSGetPowerSourceDescription(ps_blob, ups));
            if(!ups_info) goto _exit_PowerSourcesHaveChanged_;
            
            // Check UPS "Is Present" key
            isPresent = isA_CFBoolean(CFDictionaryGetValue(ups_info, CFSTR(kIOPSIsPresentKey)));
            if(!isPresent || !CFBooleanGetValue(isPresent))
            {
                // If UPS isn't active or connected we shouldn't base policy decisions on it
                //syslog(LOG_INFO, "bail 1");
                goto _exit_PowerSourcesHaveChanged_;
            }
            
            // Check Power Source
            ups_power_source = isA_CFString(CFDictionaryGetValue(ups_info, CFSTR(kIOPSPowerSourceStateKey)));
            if(!ups_power_source || !CFEqual(ups_power_source, CFSTR(kIOPSBatteryPowerValue)))
            {
                // we have to be draining the internal battery to do a shutdown
                //syslog(LOG_INFO, "bail 2");
                goto _exit_PowerSourcesHaveChanged_;
            }
            
            // Calculate battery percentage remaining
            n1 = isA_CFNumber(CFDictionaryGetValue(ups_info, CFSTR(kIOPSCurrentCapacityKey)));
            n2 = isA_CFNumber(CFDictionaryGetValue(ups_info, CFSTR(kIOPSMaxCapacityKey)));
            if(!n1 || !n2)
            {
                // We couldn't read one of the keys we determine percent with
                //syslog(LOG_INFO, "bail 3");
                goto _exit_PowerSourcesHaveChanged_;
            }

            if(!CFNumberGetValue(n1, kCFNumberIntType, &t1)) {
                //syslog(LOG_INFO, "bail 4");
                goto _exit_PowerSourcesHaveChanged_;
            }
    
            if(!CFNumberGetValue(n2, kCFNumberIntType, &t2)) {
                //syslog(LOG_INFO, "bail 5");            
                goto _exit_PowerSourcesHaveChanged_;
            }
            
            // percent = battery level / maximum capacity
            percent_remaining = (double)( ((double)t1) / ((double)t2) );
            
            //syslog(LOG_INFO, "CurrentCapacity = %d; MaxCapacity = %d; Percent = %f", t1, t2, percent_remaining);
            
            // Compare percent remaining to warning threshold
            // NOT IMPLEMENTED
            
            // Shutdown threshold is hard-wired to 20%
            shutdown_threshold = 0.2;

            // Compare percent remaining to shutdown threshold
            if(percent_remaining < shutdown_threshold)
            {
                // Do emergency low power shutdown
                //syslog(LOG_INFO, "emergency low power shutdown ACTIVATED");
                doPowerEmergencyShutdown();
            }
        }
    }
    
    // exit point
    _exit_PowerSourcesHaveChanged_:
    if(ups) CFRelease(ups);
    if(batt0) CFRelease(batt0);
}
