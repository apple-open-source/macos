/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2001 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 18-Dec-01 ebold created
 *
 */
 
#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOMessage.h>

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <syslog.h>
#include <unistd.h>

#include "PMSettings.h"
#include "PSLowPower.h"
#include "BatteryTimeRemaining.h"

#define kIOPMAppName		"Power Management configd plugin"
#define kIOPMPrefsPath		"com.apple.PowerManagement.xml"

// Global keys
static CFStringRef	   	EnergyPrefsKey;

static SCDynamicStoreRef   	energyDS;

static io_connect_t     _pm_ack_port = 0;


/* SleepWakeCallback
 * 
 * Receives notifications on system sleep and system wake.
 */
static void
SleepWakeCallback(void * port,io_service_t y,natural_t messageType,void * messageArgument)
{
    // Notify BatteryTimeRemaining
    BatteryTimeRemainingSleepWakeNotification(messageType);

    // Notify PMSettings
    PMSettingsSleepWakeNotification(messageType);

    switch ( messageType ) {
    case kIOMessageSystemWillSleep:
    case kIOMessageCanSystemSleep:
        IOAllowPowerChange(_pm_ack_port, (long)messageArgument);
        break;
        
    case kIOMessageSystemHasPoweredOn:
        break;
    }
}

/* ESPrefsHaveChanged
 *
 * Is the handler that configd calls when someone "applies" new Energy Saver
 * Preferences. Since the preferences have probably changed, we re-read them
 * from disk and transmit the new settings to the kernel.
 */
static void 
ESPrefsHaveChanged(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info) 
{
    // Tell PMSettings that the prefs file has changed
    PMSettingsPrefsHaveChanged(store, changedKeys, EnergyPrefsKey);

    return;
}


/* PowerSourcesHaveChanged
 *
 * Is the handler that gets notified when power source (battery or UPS)
 * state changes. We might respond to this by posting a user notification
 * or performing emergency sleep/shutdown.
 */
extern void
PowerSourcesHaveChanged(void *info) 
{
    CFTypeRef			ps_blob;
    
    ps_blob = isA_CFDictionary(IOPSCopyPowerSourcesInfo());
    if(!ps_blob) return;
    
    // Notifiy PSLowPower of power sources change
    PSLowPowerPSChange(ps_blob);
    
    CFRelease(ps_blob);
}

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

/* BatteryPollingTimer
 *
 * typedef void (*CFRunLoopTimerCallBack)(CFRunLoopTimerRef timer, void *info);
 * 
 */
static void
BatteryPollingTimer(CFRunLoopTimerRef timer, void *info) {
    CFArrayRef			                battery_info = NULL;

    // get battery info
    battery_info = isA_CFArray(copyBatteryInfo());
    if(!battery_info) return;

    // Pass control over to PMUBattery for battery calculation
    BatteryTimeRemainingBatteryPollingTimer(battery_info);

    // Pass control over to PMSettings
    PMSettingsBatteryPollingTimer(battery_info);
    
    CFRelease(battery_info);
}

/* initializeESPrefsDynamicStore
 *
 * Registers a handler that configd calls when someone changes com.apple.PowerManagement.xml
 */
static void
initializeESPrefsDynamicStore(void)
{
    CFRunLoopSourceRef 		CFrls;
    
    energyDS = SCDynamicStoreCreate(NULL, CFSTR(kIOPMAppName), &ESPrefsHaveChanged, NULL);

    // Setup notification for changes in Energy Saver prefences
    EnergyPrefsKey = SCDynamicStoreKeyCreatePreferences(NULL, CFSTR(kIOPMPrefsPath), kSCPreferencesKeyApply);
    if(EnergyPrefsKey) 
        SCDynamicStoreAddWatchedKey(energyDS, EnergyPrefsKey, FALSE);

    // Create and add RunLoopSource
    CFrls = SCDynamicStoreCreateRunLoopSource(NULL, energyDS, 0);
    if(CFrls) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), CFrls, kCFRunLoopDefaultMode);    
        CFRelease(CFrls);
    }

    return;
}

/* initializePowerSourceChanges
 *
 * Registers a handler that gets called on power source (battery or UPS) changes
 */
static void
initializePowerSourceChangeNotification(void)
{
    CFRunLoopSourceRef 		CFrls;
        
    // Create and add RunLoopSource
    CFrls = IOPSNotificationCreateRunLoopSource(PowerSourcesHaveChanged, NULL);
    if(CFrls) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), CFrls, kCFRunLoopDefaultMode);    
        CFRelease(CFrls);
    }
}

/* initializeBatteryPollingTimer
 *
 * Sets up the CFTimer that will read battery settings every x seconds
 */
static void
initializeBatteryPollingTimer()
{
    CFRunLoopTimerRef		batteryTimer;

    batteryTimer = CFRunLoopTimerCreate(NULL, 
                            CFAbsoluteTimeGetCurrent(), 	// fire date
                            (CFTimeInterval)1.0,					// interval
                            NULL, 0, BatteryPollingTimer, NULL);

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), batteryTimer, kCFRunLoopDefaultMode);
    
    CFRelease(batteryTimer);
}

void
prime()
{
    // Initialize battery averaging code
    BatteryTimeRemaining_prime();
    
    // Initialize PMSettings code
    PMSettings_prime();
    
    // Initialize PSLowPower code
    PSLowPower_prime();

    
    return;
}

void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    IONotificationPortRef			notify;	
    io_object_t					anIterator;
    CFArrayRef					battery_info;
    int				                battery_count;

    
    // Install notification on Power Source changes
    initializePowerSourceChangeNotification();

    // Install notification when the preferences file changes on disk
    initializeESPrefsDynamicStore();

    // Register for SystemPower notifications
    _pm_ack_port = IORegisterForSystemPower (0, &notify, SleepWakeCallback, &anIterator);
    if ( _pm_ack_port != NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }

    /*
     * determine presence of battery/UPS for separate Battery/AC settings
     * If system has battery capability, install a timer to poll for battery
     * level and changes in the power source.
     */
    battery_info = copyBatteryInfo();
    if(battery_info)
    {
        battery_count = CFArrayGetCount(battery_info);
        if(battery_count > 0)
        {
            // Setup battery polling timer
            initializeBatteryPollingTimer();
        }
        CFRelease(battery_info);
    }
}

//#define MAIN

#ifdef  MAIN
int
main(int argc, char **argv)
{
	load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);

	prime();

	CFRunLoopRun();

	/* not reached */
	exit(0);
	return 0;
}
#endif
