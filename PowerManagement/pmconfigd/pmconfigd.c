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

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <syslog.h>
#include <unistd.h>

#include "batteryconfigd.h"

#define kIOPMAppName		"Power Management configd plugin"
#define kIOPMPrefsPath		"com.apple.PowerManagement.xml"

#ifndef kIOPSUPSManagementClaimed
#define kIOPSUPSManagementClaimed "State:/IOKit/UPSPowerManagementClaimed"
#endif

/* Power Management profile bits */
enum {
    kPMForceLowSpeedProfile = (1<<0),
    kPMREMSleepProfile      = (1<<1)
};

/* Global - systemEnergySettings
 * Keeps track of current Energy Saver settings.
 */
static CFMutableDictionaryRef			systemEnergySettings = NULL;

/* Global - currentPowerSource
 * Keeps track of current power - battery or AC
 */
static CFStringRef				currentPowerSource = NULL;

/* g_profiles
 * Tracks active PM usage profiles
 */
static unsigned long				g_profiles = 0;

// Global keys
static CFStringRef	   	EnergyPrefsKey;

static SCDynamicStoreRef   	energyDS;
static io_connect_t         gPowerManager;

/* Tracking sleeping state */
static unsigned long        deferredPSChangeNotify = 0;
static unsigned long        _pmcfgd_impendingSleep = 0;

/* activate_profiles
 *
 * A wrapper for IOPMActivatePMPreference. We get a chance here to apply "profiles"
 * to the Energy Saver settings before sending them to the kernel.
 * Profiles (like LidClosed or ForceLowSpeed) have affects like accelerating idle
 * times or forcing ReduceProcessorSpeed on.
 */
static IOReturn activate_profiles(CFMutableDictionaryRef d, CFStringRef s)
{
    CFMutableDictionaryRef              energy_settings;
    CFMutableDictionaryRef              profiles_activated;
    CFMutableDictionaryRef              tmp;
    IOReturn                            ret;
    CFNumberRef                         n1;
    int                                 one = 1;
    
    //syslog(LOG_INFO, "g_profiles = %ld", g_profiles);
    
    if(g_profiles)
    {
        energy_settings = (CFMutableDictionaryRef)isA_CFDictionary(CFDictionaryGetValue(d, s));
        if(!energy_settings) return kIOReturnError;

        profiles_activated = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 
            CFDictionaryGetCount(energy_settings), energy_settings);
        if(!profiles_activated) return kIOReturnError;
        
        n1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &one);
        // If the "force low speed" profile is set, flip the ReduceSpeed bit on
        if(g_profiles & kPMForceLowSpeedProfile)
        {
            if(n1) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMReduceSpeedKey), n1);
            //syslog(LOG_INFO, "ForceLowSpeed activated");
        }

        if(g_profiles & kPMREMSleepProfile)
        {
            if(n1) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMDiskSleepKey), n1);
            if(n1) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMSystemSleepKey), n1);
            if(n1) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMDisplaySleepKey), n1);
            //syslog(LOG_INFO, "REMSleepProfile activated");
        }
        CFRelease(n1);

        // Package the new, modified settings, in a way that 
        // IOPMActivatePMPreferences will read them
        tmp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(tmp, s, profiles_activated);

        ret = IOPMActivatePMPreference(tmp, s);

        CFRelease(profiles_activated);
        CFRelease(tmp);
    } else {
        ret = IOPMActivatePMPreference(d, s);
    }
        
    return ret;
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
    CFRange array_range = CFRangeMake(0, CFArrayGetCount(changedKeys));

    // If Power Management Preferences file has changed
    if(CFArrayContainsValue(changedKeys, array_range, EnergyPrefsKey))
    {
        // re-read preferences into memory
        CFRelease(systemEnergySettings);
        systemEnergySettings = (CFMutableDictionaryRef)isA_CFDictionary(IOPMCopyPMPreferences());

        // push new preferences out to the kernel
        //syslog(LOG_INFO, "PMConfigd: activating new preferences");
        if(systemEnergySettings) activate_profiles(systemEnergySettings, currentPowerSource);
    }

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
    if(temp = SCDynamicStoreCopyValue(energyDS, ups_claimed)) {
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
static void
PowerSourcesHaveChanged(void *info) 
{
    CFTypeRef			ps_blob;
    CFTypeRef			ups = NULL;
    CFTypeRef           batt0 = NULL;
    CFDictionaryRef		ups_info;
    int				t1, t2;
    CFNumberRef			n1, n2;
    double			percent_remaining;
    double			shutdown_threshold;
    CFBooleanRef		isPresent;
    CFStringRef			ups_power_source;
    
    ps_blob = isA_CFDictionary(IOPSCopyPowerSourcesInfo());
    if(!ps_blob) return;
    
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
    if(ps_blob) CFRelease(ps_blob);
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

void _pmcfgd_goingToSleep()
{
    _pmcfgd_impendingSleep = 1;
}

void _pmcfgd_wakeFromSleep()
{
    _pmcfgd_impendingSleep = 0;
    if(deferredPSChangeNotify)
    {
        deferredPSChangeNotify = 0;
        _pmcfgd_impendingSleep = 0;    

        if(currentPowerSource && CFEqual(currentPowerSource, CFSTR(kIOPMACPowerKey)))
        {
            // ac power
            IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMExternalPower);
        } else {
            // battery power
            IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMInternalPower);            
        }
    }

}

/* BatteryPollingTimer
 *
 * typedef void (*CFRunLoopTimerCallBack)(CFRunLoopTimerRef timer, void *info);
 * 
 */
static void
BatteryPollingTimer(CFRunLoopTimerRef timer, void *info) {
    CFArrayRef			                battery_info = NULL;
    CFStringRef				        oldPowerSource;
    CFBooleanRef			        rem_bool;
    int					        flags;
    int					        changed_flags;
    int                                 	settings_changed = 0;
    static int				        old_flags = 0;
    mach_port_t				        masterPort;
    io_iterator_t			        it;
    static io_registry_entry_t			root_domain = 0;

    IOMasterPort(bootstrap_port, &masterPort);

    // get battery info
    battery_info = copyBatteryInfo();
    if(!battery_info) return;

    // Pass control over to PMUBattery for battery calculation
    PMUBatteryPollingTimer(battery_info);

    // get a static pointer to IOPMrootDomain registry entry
    if(!root_domain) 
    {
        IOServiceGetMatchingServices(masterPort, IOServiceNameMatching("IOPMrootDomain"), &it);
        // note: we won't release root_domain because it's static in a system level daemon
        root_domain = IOIteratorNext(it);
        IOObjectRelease(it);
    }
    
    // decide if power source has changed
    CFNumberGetValue(CFDictionaryGetValue(
                CFArrayGetValueAtIndex((CFArrayRef)battery_info,0),
                CFSTR("Flags")), kCFNumberSInt32Type,&flags);

    changed_flags = flags ^ old_flags;
    old_flags = flags;

    // Handle charger plug/unplug event
    if(changed_flags & kIOBatteryChargerConnect)
    {
        oldPowerSource = CFStringCreateCopy(kCFAllocatorDefault, currentPowerSource);
        
        // Keep track of current power source
        if(flags & kIOBatteryChargerConnect) 
        {
            currentPowerSource = CFSTR(kIOPMACPowerKey); // AC
        } else 
        {
            currentPowerSource = CFSTR(kIOPMBatteryPowerKey); // battery
        }
        
        // Are we in the middle of a sleep?
        if(!_pmcfgd_impendingSleep)
        {
            // If not, tell drivers that the power source changed
            if(flags & kIOBatteryChargerConnect) 
            {
                IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMExternalPower);
            } else 
            {
                IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMInternalPower);
            }
        } else {
            // If so, delay the notification until we're awake from sleep
            deferredPSChangeNotify = 1;
        }
        
        if(0 != CFStringCompare(oldPowerSource, currentPowerSource, NULL))
        {
            //PowerSourceHasChanged(oldPowerSource, currentPowerSource);
            settings_changed = 1;
        }
        CFRelease(oldPowerSource);
    }
    
    // Do we need to override the low processor speed setting?
    if(changed_flags & kForceLowSpeed)
    {
        //syslog(LOG_INFO, "kForceLowSpeed bit changed to %08x", (flags & kForceLowSpeed));
    
        settings_changed = 1;
        if(flags & kForceLowSpeed)
            g_profiles |= kPMForceLowSpeedProfile;
        else g_profiles &= ~kPMForceLowSpeedProfile;
    }

    // activate/de-activate REMSleep profile
    if(flags & kIOPMClosedClamshell)
    {
        //syslog(LOG_INFO, "kIOPMClosedClamshell changed to %08x", flags & kIOPMClosedClamshell);
        // Clamshell is closed
        rem_bool = IORegistryEntryCreateCFProperty(root_domain, 
                    CFSTR("REMSleepEnabled"), kCFAllocatorDefault, NULL);
        if(rem_bool)
        {
            if(true == CFBooleanGetValue(rem_bool) )
            {
                // Clamshell is closed and REMSleep is enabled, so activate the
                // REMSleep profile. (only if it's not already active)
                if(!(g_profiles & kPMREMSleepProfile)) 
                {
                    // kPMREMSleepProfile ON
                    g_profiles |= kPMREMSleepProfile;
                    settings_changed = 1;
                }
            } else {
                // REMSleepEnabled is set to false. If we have the profile activated, disable it.
                if(g_profiles & kPMREMSleepProfile) 
                {
                    // kPMREMSleepProfile OFF
                    g_profiles &= ~kPMREMSleepProfile;
                    settings_changed = 1;
                }
            }
            CFRelease(rem_bool);
        }            
    } else {
        // If the lid is open and REMSleepProfile is on, disable it
        if(g_profiles & kPMREMSleepProfile) 
        {
            // kPMREMSleepProfile OFF
            g_profiles &= ~kPMREMSleepProfile;
            settings_changed = 1;
        }
    }
    if(settings_changed)
    {
        activate_profiles(systemEnergySettings, currentPowerSource);
    }
    
    IOObjectRelease(masterPort);
    CFRelease(battery_info);
}

/* initializeESPrefsDynamicStore
 *
 * Registers a handler that configd calls when someone changes com.apple.PowerManagement.xml
 */
static SCDynamicStoreRef
initializeESPrefsDynamicStore(void)
{
    CFRunLoopSourceRef 		CFrls;
    SCDynamicStoreContext	*SCDcontext;
    
    SCDcontext = NULL;
        
    energyDS = SCDynamicStoreCreate(NULL, CFSTR(kIOPMAppName), &ESPrefsHaveChanged, SCDcontext);

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


    return energyDS;
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
    PMUBattery_prime();

    // load the initial configuration from the database
    systemEnergySettings = IOPMCopyPMPreferences();

    // send the initial configuration to the kernel
    activate_profiles(systemEnergySettings, currentPowerSource);

    // send initial power source info to drivers
    if(CFEqual(currentPowerSource, CFSTR(kIOPMACPowerKey)))
         IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMExternalPower);
    else IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMInternalPower);
    
    return;
}

void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    CFArrayRef			            battery_info;
    int				                battery_count;
    int				                flags;
    mach_port_t				        masterPort;

    // Open a connection to the Power Manager.
    IOMasterPort(bootstrap_port, &masterPort);
    gPowerManager = IOPMFindPowerManagement(masterPort);
    if (gPowerManager == 0) return;
    IOObjectRelease(masterPort);
    
    // Install notification on Power Source changes
    initializePowerSourceChangeNotification();

    // Install notification when the preferences file changes on disk
    initializeESPrefsDynamicStore();

    // Initialize battery averaging code
    PMUBattery_load(bundle, bundleVerbose);

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
        
        // Find out what the current power source is
        CFNumberGetValue(CFDictionaryGetValue(
                    CFArrayGetValueAtIndex((CFArrayRef)battery_info,0),
                    CFSTR("Flags")), kCFNumberSInt32Type,&flags);

        if(flags & kIOBatteryChargerConnect) {
            currentPowerSource = CFSTR(kIOPMACPowerKey); // AC
        } else {
            currentPowerSource = CFSTR(kIOPMBatteryPowerKey); // battery
        }
            
        CFRelease(battery_info);
    } else {
        // If no batteries are found, set currentPowerSource as AC
        currentPowerSource = CFSTR(kIOPMACPowerKey);
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
