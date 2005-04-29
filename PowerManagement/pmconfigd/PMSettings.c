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

#include <syslog.h>
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
#include <sys/syslog.h>

#include "PMSettings.h"
#include "PrivateLib.h"
 
/* Global - systemEnergySettings
 * Keeps track of current Energy Saver settings.
 */
static CFDictionaryRef                  systemEnergySettings = NULL;

/* Global - currentPowerSource
 * Keeps track of current power - battery or AC
 */
static CFStringRef                      currentPowerSource = NULL;

/* g_overrides
 * Tracks active PM usage profiles
 */
static unsigned long                    g_overrides = 0;

static io_connect_t                     gPowerManager;

/* Tracking sleeping state */
static unsigned long                    deferredPSChangeNotify = 0;
static unsigned long                    _pmcfgd_impendingSleep = 0;

// Forward Declarations
static CFDictionaryRef _copyPMSettings(void);
static IOReturn activate_profiles(CFDictionaryRef d, CFStringRef s);

// Must be followed by a call to activateSettingOverrides
__private_extern__ void
overrideSetting
(
    int             bit,
    int             val
)
{
    if(val) {
        g_overrides |= bit;
    } else {
        g_overrides &= ~bit;
    }
}


// Providing activateSettingsOverrides to SetActive.c
// So that it may set multiple assertions without triggering a prefs
// re-evaluate each time. SetActive.c can call overrideSetting() n times
// and only call activateSettingsOverrides once.
__private_extern__ void
activateSettingOverrides(void)
{
    if(systemEnergySettings) {
        activate_profiles(systemEnergySettings, currentPowerSource);
    }
}

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
PMSettings_CopyActivePMSettings(void)
{
    CFDictionaryRef         copy_all_settings;
    CFDictionaryRef         energySettings;
    CFDictionaryRef         return_val;

    copy_all_settings = _copyPMSettings();
    if(!copy_all_settings) return NULL;
    energySettings = isA_CFDictionary(CFDictionaryGetValue(copy_all_settings,currentPowerSource));
    if(energySettings) 
        return_val = CFDictionaryCreateCopy(kCFAllocatorDefault, energySettings);
    else 
        return_val = NULL;

    CFRelease(copy_all_settings);
    return return_val;
}

__private_extern__ void 
PMSettingsConsoleUserHasChanged(void)
{
    static int first_login = 1;
    
    if(first_login) {
        // Broadcast settings to drivers a second time during boot-up, since
        // some drivers may not have been loaded at the time configd launched,
        // particularly IOBacklightDisplay (3956697).
        activate_profiles(systemEnergySettings, currentPowerSource);
        first_login = 0;
    }
}

/* _copyPMSettings
 * Synthesizes a dictionary of "user-selected profiles" per-power source. 
 The returned dictionary represents the "currently selected" per-power source settings.
 */
static CFDictionaryRef
_copyPMSettings(void)
{
    CFDictionaryRef                 active_profiles = NULL;
    CFArrayRef                      system_profiles = NULL;
    CFDictionaryRef                 custom_settings = NULL;
    CFMutableDictionaryRef          return_val = 0;
    CFStringRef                     *active_profile_dict_keys = NULL;
    CFNumberRef                     *active_profile_dict_values = NULL;
    int                             ps_count;
    int                             i;

    active_profiles = IOPMCopyActivePowerProfiles();
    system_profiles = IOPMCopyPowerProfiles();    
    custom_settings = IOPMCopyPMPreferences();
    
    if(!active_profiles || !system_profiles || !custom_settings) goto exit;

    return_val = CFDictionaryCreateMutable(0, 0, 
            &kCFTypeDictionaryKeyCallBacks, 
            &kCFTypeDictionaryValueCallBacks);

    ps_count = CFDictionaryGetCount(active_profiles);
    active_profile_dict_keys = (CFStringRef *)malloc(ps_count*sizeof(CFStringRef));
    active_profile_dict_values = (CFNumberRef *)malloc(ps_count*sizeof(CFNumberRef));
    if(!active_profile_dict_keys || !active_profile_dict_values) goto exit;
    CFDictionaryGetKeysAndValues(
        active_profiles, 
        (const void **)active_profile_dict_keys, 
        (const void **)active_profile_dict_values);

    for(i=0; i<ps_count; i++)
    {
        int                         profile_index;
        CFDictionaryRef             settings_per_power_source;
        CFDictionaryRef             sys_profile;
        CFDictionaryRef             tmp_settings;

        if(!CFNumberGetValue(active_profile_dict_values[i], 
            kCFNumberIntType, &profile_index)) continue;
        if(-1 == profile_index) {
            // Custom profile for this power source
            settings_per_power_source = 
                CFDictionaryGetValue(custom_settings, active_profile_dict_keys[i]);
        } else {
            // user has selected a system defined profile for this source
            if( (profile_index < 0) || (profile_index > 4) ) continue;
            sys_profile = isA_CFDictionary(
                CFArrayGetValueAtIndex(system_profiles, profile_index));
            if(!sys_profile) continue;
            settings_per_power_source = CFDictionaryGetValue(sys_profile, active_profile_dict_keys[i]);
        }
        syslog(LOG_INFO, "PM configd:_copyPMSettings(): using profile #%d for power source %s\n", 
            profile_index, CFStringGetCStringPtr(active_profile_dict_keys[i], kCFStringEncodingMacRoman));
        if(!settings_per_power_source) {
            syslog(LOG_INFO, "ERROR ERROR ERROR no settings_per_power_source available!\n");
            continue;
        }
        tmp_settings = CFDictionaryCreateCopy(kCFAllocatorDefault, settings_per_power_source);
        if(!tmp_settings) continue;
        CFDictionarySetValue(return_val, active_profile_dict_keys[i], tmp_settings);
        CFRelease(tmp_settings);
    }


exit:
    if(active_profile_dict_keys) free(active_profile_dict_keys);
    if(active_profile_dict_values) free(active_profile_dict_values);
    if(active_profiles) CFRelease(active_profiles);
    if(system_profiles) CFRelease(system_profiles);
    if(custom_settings) CFRelease(custom_settings);
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
activate_profiles(CFDictionaryRef d, CFStringRef s)
{
    CFDictionaryRef                     energy_settings;
    CFMutableDictionaryRef              profiles_activated;
    CFMutableDictionaryRef              tmp;
    IOReturn                            ret;
    CFNumberRef                         n1, n0;
    int                                 one = 1;
    int                                 zero = 0;
    
    //syslog(LOG_INFO, "g_overrides = %ld", g_overrides);
    
    if(g_overrides)
    {
        energy_settings = (CFDictionaryRef)isA_CFDictionary(CFDictionaryGetValue(d, s));
        if(!energy_settings) return kIOReturnError;

        profiles_activated = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 
            CFDictionaryGetCount(energy_settings), energy_settings);
        if(!profiles_activated) return kIOReturnError;
        
        n1 = CFNumberCreate(0, kCFNumberIntType, &one);
        n0 = CFNumberCreate(0, kCFNumberIntType, &zero);
        // If the "force low speed" profile is set, flip the ReduceSpeed bit on
        if(g_overrides & kPMForceLowSpeedProfile)
        {
            if(n1) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMReduceSpeedKey), n1);
            //syslog(LOG_INFO, "ForceLowSpeed activated");
        }
        
        if(g_overrides & kPMForceHighSpeed)
        {
            if(n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMReduceSpeedKey), n0);
            if(n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMDynamicPowerStepKey), n0);
        }
        
        CFRelease(n0);
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
    systemEnergySettings = _copyPMSettings();

    // send the initial configuration to the kernel
    if(systemEnergySettings) activate_profiles(systemEnergySettings, currentPowerSource);

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
    if(systemEnergySettings) CFRelease(systemEnergySettings);
    systemEnergySettings = (CFDictionaryRef)isA_CFDictionary(_copyPMSettings());

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
        
        if(systemEnergySettings) activate_profiles(systemEnergySettings, currentPowerSource);
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
    
        settings_changed = 1;
        if(flags & kForceLowSpeed)
            g_overrides |= kPMForceLowSpeedProfile;
        else g_overrides &= ~kPMForceLowSpeedProfile;
    }

    if(settings_changed)
    {
        if(systemEnergySettings) activate_profiles(systemEnergySettings, currentPowerSource);
    }
    
}
