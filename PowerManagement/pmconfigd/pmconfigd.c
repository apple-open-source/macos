/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOMessage.h>

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <syslog.h>
#include <unistd.h>

#include "PMSettings.h"
#include "PSLowPower.h"
#include "BatteryTimeRemaining.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "PrivateLib.h"

#define kIOPMAppName		"Power Management configd plugin"
#define kIOPMPrefsPath		"com.apple.PowerManagement.xml"

// Global keys
static CFStringRef	   	EnergyPrefsKey;
static CFStringRef      	AutoWakePrefsKey;
static CFStringRef      	ConsoleUserKey;
static SCDynamicStoreRef   	energyDS;

static io_service_t		gIOResourceService;
static io_connect_t     	_pm_ack_port = 0;


/* PMUInterestNotification
 *
 * Receives and distributes messages from the PMU driver
 * These include legacy AutoWake requests and battery change notifications.
 */
static void 
PMUInterestNotification(void *refcon, io_service_t service, natural_t messageType, void *arg)
{    
    // Tell the AutoWake handler
    if((kIOPMUMessageLegacyAutoWake == messageType) ||
       (kIOPMUMessageLegacyAutoPower == messageType) )
        AutoWakePMUInterestNotification(messageType, (UInt32)arg);
}

/* RootDomainInterestNotification
 *
 * Receives and distributes messages from the IOPMrootDomain
 */
static void 
RootDomainInterestNotification(void *refcon, io_service_t service, natural_t messageType, void *arg)
{
    CFArrayRef          battery_info;

    // Tell battery calculation code that battery status has changed
    if(kIOPMMessageBatteryStatusHasChanged == messageType)
    {
        // get battery info
        battery_info = isA_CFArray(_copyBatteryInfo());
        if(!battery_info) return;

        // Pass control over to PMSettings
        PMSettingsBatteriesHaveChanged(battery_info);
        // Pass control over to PMUBattery for battery calculation
        BatteryTimeRemainingBatteriesHaveChanged(battery_info);
        
        CFRelease(battery_info);
    }
}

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
    
    // Notify AutoWake
    AutoWakeSleepWakeNotification(messageType);
    RepeatingAutoWakeSleepWakeNotification(messageType);

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
    CFRange   key_range = CFRangeMake(0, CFArrayGetCount(changedKeys));

    if(CFArrayContainsValue(changedKeys, key_range, EnergyPrefsKey))
    {
        // Tell PMSettings that the prefs file has changed
        PMSettingsPrefsHaveChanged();
    }

    if(CFArrayContainsValue(changedKeys, key_range, AutoWakePrefsKey))
    {
        // Tell AutoWake that the prefs file has changed
        AutoWakePrefsHaveChanged();
        RepeatingAutoWakePrefsHaveChanged();
    }

    if(CFArrayContainsValue(changedKeys, key_range, ConsoleUserKey))
    {
	CFArrayRef sessionList = SCDynamicStoreCopyConsoleInformation(energyDS);
	if (!sessionList)
	    sessionList = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);

	if (sessionList)
	{
	    IORegistryEntrySetCFProperty(gIOResourceService, CFSTR(kIOConsoleUsersKey), sessionList);
	    CFRelease(sessionList);
	}
    }

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

    // Setup notification for changes in AutoWake prefences
    AutoWakePrefsKey = SCDynamicStoreKeyCreatePreferences(NULL, CFSTR(kIOPMAutoWakePrefsPath), kSCPreferencesKeyCommit);
    if(AutoWakePrefsKey) 
        SCDynamicStoreAddWatchedKey(energyDS, AutoWakePrefsKey, FALSE);

    gIOResourceService = IORegistryEntryFromPath(NULL, kIOServicePlane ":/" kIOResourcesClass);
    ConsoleUserKey = SCDynamicStoreKeyCreateConsoleUser( NULL /* CFAllocator */ );
    if(ConsoleUserKey && gIOResourceService) 
        SCDynamicStoreAddWatchedKey(energyDS, ConsoleUserKey, FALSE);

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

/* initializeInteresteNotifications
 *
 * Sets up the notification of general interest from the PMU & RootDomain
 */
static void
initializeInterestNotifications()
{
    IONotificationPortRef       notify_port = 0;
    IONotificationPortRef       r_notify_port = 0;
    io_object_t                 notification_ref = 0;
    io_service_t                pmu_service_ref = 0;
    io_service_t                root_domain_ref = 0;
    CFRunLoopSourceRef          rlser = 0;
    CFRunLoopSourceRef          r_rlser = 0;
    IOReturn                    ret;

    // PMU
    pmu_service_ref = IOServiceGetMatchingService(0, IOServiceNameMatching("ApplePMU"));
    if(!pmu_service_ref) goto root_domain;

    notify_port = IONotificationPortCreate(0);
    ret = IOServiceAddInterestNotification(notify_port, pmu_service_ref, 
                                kIOGeneralInterest, PMUInterestNotification,
                                0, &notification_ref);
    if(kIOReturnSuccess != ret) goto root_domain;

    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(!rlser) goto root_domain;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);
    
    
    // ROOT_DOMAIN
root_domain:
    root_domain_ref = IOServiceGetMatchingService(0, IOServiceNameMatching("IOPMrootDomain"));
    if(!root_domain_ref) goto finish;

    r_notify_port = IONotificationPortCreate(0);
    ret = IOServiceAddInterestNotification(r_notify_port, root_domain_ref, 
                                kIOGeneralInterest, RootDomainInterestNotification,
                                0, &notification_ref);
    if(kIOReturnSuccess != ret) goto finish;

    r_rlser = IONotificationPortGetRunLoopSource(r_notify_port);
    if(!r_rlser) goto finish;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), r_rlser, kCFRunLoopDefaultMode);

finish:
    if(rlser) CFRelease(rlser);
    if(r_rlser) CFRelease(r_rlser);
    if(notify_port) IOObjectRelease((io_object_t)notify_port);
    if(r_notify_port) IOObjectRelease((io_object_t)r_notify_port);
    if(pmu_service_ref) IOObjectRelease(pmu_service_ref);
    if(root_domain_ref) IOObjectRelease(root_domain_ref);
    return;
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

    // Initialzie AutoWake code
    AutoWake_prime();
    RepeatingAutoWake_prime();
    
    return;
}

void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    IONotificationPortRef           notify;	
    io_object_t                     anIterator;
    
    // Install notification on Power Source changes
    initializePowerSourceChangeNotification();

    // Install notification when the preferences file changes on disk
    initializeESPrefsDynamicStore();

    // Install notification on ApplePMU&IOPMrootDomain general interest messages
    initializeInterestNotifications();

    // Register for SystemPower notifications
    _pm_ack_port = IORegisterForSystemPower (0, &notify, SleepWakeCallback, &anIterator);
    if ( _pm_ack_port != NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }

}

// cc -o pm -g pmconfigd.c PSLowPower.c PMSettings.c BatteryTimeRemaining.c AutoWakeScheduler.c RepeatingAutoWake.c PrivateLib.c -framework IOKit -framework CoreFoundation -framework SystemConfiguration -framework AppKit
//#define STANDALONE

#ifdef  STANDALONE
int
main(int argc, char **argv)
{
    openlog("pmcfgd", LOG_PID | LOG_NDELAY, LOG_USER);

	load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);

	prime();

	CFRunLoopRun();

	/* not reached */
	exit(0);
	return 0;
}
#endif
