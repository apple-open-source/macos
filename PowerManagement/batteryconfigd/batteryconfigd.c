/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 4-Apr-02 ebold created
 * configd plugin for tracking battery usage
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/ps/IOPSKeys.h>
#include "battery.h"

/**** PMUBattery configd plugin
 This plugin ontinuously reads battery status from the IORegistry via IOPMCopyBatteryInfo(). It 
 calculates remaining battery time and re-packages relevant battery information in a CFDictionary.
 Then the plugin publishes this CFDictionary of battery information in the SCDynamicStore under the keys
 "State:/IOKit/PowerSources/PMUBattery-0" and "State:/IOKit/PowerSources/PMUBattery-1"
 
 By publishing its state in the SCDynamicStore, that information is now available to the IOKit PowerSource
 API. The IOPS API presents an abstract interface to all power sources on the system - internal battery and
 UPS.
 
 Future work:
 - ApplePMU.kext should send notifications when the battery info has changed. This plugin should only
   check the battery on each of those notifications. Right now the plugin checks the battery every second
   so that we can observe AC plug/unplug events in "real time" and present them to the UI (in Battery Monitor).
   
 - This plugin should also compare its new battery info to the most recently published battery info to avoid
   unnecessarily duplicated work of clients. Note that whenever we publish a new battery state, every 
   "interested client" of the PowerSource API receives a notification.
****/

static CFStringRef PMUBatteryDynamicStore[MAX_BATTERY_NUM];

static CFArrayRef
copyBatteryInfo(void) 
{
    static mach_port_t 		master_device_port = 0;
    kern_return_t       	kr;
    int				ret;
    
    CFArrayRef			battery_info = NULL;
    
    if(!master_device_port) kr = IOMasterPort(bootstrap_port,&master_device_port);
    
    // PMCopyBatteryInfo
    ret = IOPMCopyBatteryInfo(master_device_port, &battery_info);
    if(ret != kIOReturnSuccess || !battery_info)
    {
        return NULL;
    }
    
    return battery_info;
}

void
PMUBatteryPollingTimer(CFArrayRef batteryInfo) 
{
    CFDictionaryRef	result[MAX_BATTERY_NUM];
    int			i;
    static SCDynamicStoreRef	store = NULL;
    static CFDictionaryRef	old_battery[MAX_BATTERY_NUM] = {NULL, NULL};
    
    if(!batteryInfo) return;
    
    if(!store) 
        store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("PMUBattery configd plugin"), NULL, NULL);

    bzero(result, MAX_BATTERY_NUM*sizeof(CFDictionaryRef));
    _IOPMCalculateBatteryInfo(batteryInfo, result);
    
    // Publish the results of calculation in the SCDynamicStore
    for(i=0; i<MAX_BATTERY_NUM; i++) {
        if(result[i]) {   
            // Determine if CFDictionary is new or has changed...
            // Only do SCDynamicStoreSetValue if the dictionary is different
            if(!old_battery[i]) {
                SCDynamicStoreSetValue(store, PMUBatteryDynamicStore[i], result[i]);
            } else {
                if(!CFEqual(old_battery[i], result[i])) {
                    SCDynamicStoreSetValue(store, PMUBatteryDynamicStore[i], result[i]);
                }
                CFRelease(old_battery[i]);
            }
            old_battery[i] = result[i];

        }
    }
}

static void
initializeBatteryPollingTimer()
{
    CFRunLoopTimerRef		batteryTimer;

    batteryTimer = CFRunLoopTimerCreate(NULL, 
                            CFAbsoluteTimeGetCurrent(), 	// fire date
                            (CFTimeInterval)1.0,		// interval
                            NULL, 0, PMUBatteryPollingTimer, NULL);

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), batteryTimer, kCFRunLoopDefaultMode);
    
    CFRelease(batteryTimer);
}

void
PMUBattery_prime()
{
    // nada
    return;
}

void
PMUBattery_load(CFBundleRef bundle, Boolean bundleVerbose)
{
    CFArrayRef			battery_info;
    int				battery_count;
    
    // setup battery calculation global variables
    _IOPMCalculateBatterySetup();
    
    // Initialize SCDynamicStore battery key names
    PMUBatteryDynamicStore[0] = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
            kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), CFSTR("InternalBattery-0"));
    PMUBatteryDynamicStore[1] = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@/%@"),
            kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), CFSTR("InternalBattery-1"));
}

//#define MAIN

#ifdef  MAIN
int
main(int argc, char **argv) {
        printf("load\n"); fflush(stdout);
	load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
        printf("prime\n"); fflush(stdout);
	prime();
        printf("run\n"); fflush(stdout);
	CFRunLoopRun();
	/* not reached */
        exit(0);
	return 0;
}
#endif
