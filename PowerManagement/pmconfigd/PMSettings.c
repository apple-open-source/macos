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
 * 29-Aug-02 ebold created
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/IOMessage.h>

#include "PMSettings.h"
#include "PrivateLib.h"
 
/* Power Management profile bits */
enum {
    kPMForceLowSpeedProfile         = (1<<0),
    kPMREMSleepProfile              = (1<<1)
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

static io_connect_t         gPowerManager;

/* Tracking sleeping state */
static unsigned long        deferredPSChangeNotify = 0;
static unsigned long        _pmcfgd_impendingSleep = 0;

__private_extern__ void 
PMSettingsSleepWakeNotification(natural_t messageType)
{
    // note: The sleepwake handler in pmconfigd.c does all the dirty work like
    // acknowledging this sleep notification with IOAllowPowerChange(). That's
    // why we don't make that call here.

    switch (messageType) {
        case kIOMessageSystemWillSleep:
            _pmcfgd_impendingSleep = 1;
            break;
            
        case kIOMessageSystemHasPoweredOn:
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
            break;
    }
    
    return;
}

__private_extern__ CFDictionaryRef
PMSettings_CopyPMSettings(void)
{
    CFDictionaryRef         copy_all_settings;
    CFDictionaryRef         energySettings;
    CFDictionaryRef         return_val;

    copy_all_settings = IOPMCopyPMPreferences();
    if(!copy_all_settings) return NULL;
    energySettings = isA_CFDictionary(CFDictionaryGetValue(copy_all_settings,currentPowerSource));
    if(energySettings) 
        return_val = CFDictionaryCreateCopy(kCFAllocatorDefault, energySettings);
    else 
        return_val = NULL;

    CFRelease(copy_all_settings);
    return return_val;
}


 /* activate_profiles
 *
 * A wrapper for IOPMActivatePMPreference. We get a chance here to apply "profiles"
 * to the Energy Saver settings before sending them to the kernel.
 * Profiles (like LidClosed or ForceLowSpeed) have affects like accelerating idle
 * times or forcing ReduceProcessorSpeed on.
 */
static IOReturn 
activate_profiles(CFMutableDictionaryRef d, CFStringRef s)
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


__private_extern__ void 
PMSettings_prime(void)
{
    mach_port_t                         masterPort;
    CFTypeRef                           ps_blob;

    // Open a connection to the Power Manager.
    IOMasterPort(bootstrap_port, &masterPort);
    gPowerManager = IOPMFindPowerManagement(masterPort);
    if (gPowerManager == 0) return;
    IOObjectRelease(masterPort);
    
    /*
     * determine current power source for separate Battery/AC settings
     */
    ps_blob = IOPSCopyPowerSourcesInfo();
    if(ps_blob) 
    {
        currentPowerSource = IOPSGetProvidingPowerSourceType(ps_blob);
        CFRelease(ps_blob);
    } else currentPowerSource = CFSTR(kIOPMACPowerKey);
    
    // load the initial configuration from the database
    systemEnergySettings = IOPMCopyPMPreferences();

    // send the initial configuration to the kernel
    activate_profiles(systemEnergySettings, currentPowerSource);

    // send initial power source info to drivers
    if(CFEqual(currentPowerSource, CFSTR(kIOPMACPowerKey)))
         IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMExternalPower);
    else IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMInternalPower);
}
 
/* ESPrefsHaveChanged
 *
 * Is the handler that configd calls when someone "applies" new Energy Saver
 * Preferences. Since the preferences have probably changed, we re-read them
 * from disk and transmit the new settings to the kernel.
 */
__private_extern__ void 
PMSettingsPrefsHaveChanged(void) 
{
    // re-read preferences into memory
    CFRelease(systemEnergySettings);
    systemEnergySettings = (CFMutableDictionaryRef)isA_CFDictionary(IOPMCopyPMPreferences());

    // push new preferences out to the kernel
    //syslog(LOG_INFO, "PMConfigd: activating new preferences");
    if(systemEnergySettings) activate_profiles(systemEnergySettings, currentPowerSource);
    
    return;
}


/* PMSettingsPSChange
 *
 * A power source has changed. Has the current power provider changed?
 * If so, get new settings down to the kernel.
 */
__private_extern__ void PMSettingsPSChange(CFTypeRef ps_blob)
{
    CFStringRef     newPowerSource;
    
    newPowerSource = IOPSGetProvidingPowerSourceType(ps_blob);

    if(!CFEqual(currentPowerSource, newPowerSource))
    {
        currentPowerSource = newPowerSource;

        // Are we in the middle of a sleep?
        if(!_pmcfgd_impendingSleep)
        {
            // If not, tell drivers that the power source changed
            if(CFEqual(CFSTR(kIOPMACPowerKey), currentPowerSource))
            {
                // Running off of external power
                IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMExternalPower);
            } else {
                // This is either battery power or UPS power, "internal power"
                IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMInternalPower);
            }     
        } else {
            // If we WERE in the middle of a sleep, delay notification until we're awake.
            deferredPSChangeNotify = 1;
        }
        
        activate_profiles(systemEnergySettings, currentPowerSource);
    }

}

/* BatteryPollingTimer
 * 
 */
__private_extern__ void
PMSettingsBatteriesHaveChanged(CFArrayRef battery_info)
{
    CFBooleanRef			        rem_bool;
    int					        flags;
    int					        changed_flags;
    int                                 	settings_changed = 0;
    static int				        old_flags = 0;
    io_iterator_t			        it;
    static io_registry_entry_t			root_domain = 0;

    if(!root_domain) 
    {
        IOServiceGetMatchingServices(0, IOServiceNameMatching("IOPMrootDomain"), &it);
        // note: we won't release root_domain because it's static
        root_domain = IOIteratorNext(it);
        IOObjectRelease(it);
    }

    // YUCK YUCK YUCK
    // The very fact that these bits are delivered via the PMU's battery info struct
    // is a hack that slowly evolved to a place we don't want it to be.
    // As part of the disappearance of the PMU, we'll be moving these off into their
    // own out-of-band non-battery "environmental bits" data channel created in rdar://problem/3200532.

    // decide if power source has changed
    CFNumberGetValue(CFDictionaryGetValue(
                CFArrayGetValueAtIndex((CFArrayRef)battery_info,0),
                CFSTR("Flags")), kCFNumberSInt32Type,&flags);

    changed_flags = flags ^ old_flags;
    old_flags = flags;
    
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
    
}
