/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 29-Aug-02 ebold created
 *
 */

#include <syslog.h>
#include <sys/syslog.h>

#include "PMSettings.h"
#include "PrivateLib.h"

/* Arguments to CopyPMSettings functions */
enum {
    kIOPMUnabridgedSettings = false,
    kIOPMRemoveUnsupportedSettings = true
};

/* Global - energySettings
 * Keeps track of current Energy Saver settings.
 */
static CFDictionaryRef                  energySettings = NULL;

/* Global - currentPowerSource
 * Keeps track of current power - battery or AC
 */
static CFStringRef                      currentPowerSource = NULL;

/* g_overrides
 * Tracks active PM usage profiles
 */
static unsigned long                    g_overrides = 0;
static unsigned long                    gLastOverrideState = 0;
#if TARGET_OS_EMBEDDED
static long				gSleepSetting = -1;
#endif

static io_connect_t                     gPowerManager;

/* Tracking sleeping state */
static unsigned long                    deferredPSChangeNotify = 0;
static unsigned long                    _pmcfgd_impendingSleep = 0;

// Forward Declarations
static CFDictionaryRef _copyPMSettings(
                bool removeUnsupported);
static IOReturn activate_profiles(
                CFDictionaryRef d, 
                CFStringRef s, 
                bool removeUnsupported);

/* overrideSetting
 * Must be followed by a call to activateSettingOverrides
 */
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
    if (!energySettings) 
        return;

    if (gLastOverrideState != g_overrides)
    {
#if TARGET_OS_EMBEDDED
	if ((kPMPreventIdleSleep == (gLastOverrideState ^ g_overrides))
	 && (-1 != gSleepSetting)) do
	{
	    static io_connect_t gIOPMConnection = MACH_PORT_NULL;
	    IOReturn kr;

	    if (!gIOPMConnection) gIOPMConnection = IOPMFindPowerManagement(0);
	    if (!gIOPMConnection) break;
	    kr = IOPMSetAggressiveness(gIOPMConnection, kPMMinutesToSleep, 
					(kPMPreventIdleSleep & g_overrides) ? 0 : gSleepSetting);
	    if (kIOReturnSuccess != kr)
	    {
		gIOPMConnection = MACH_PORT_NULL;
		break;
	    }
	    gLastOverrideState = g_overrides;
	    return;
	}
	while (false);
#endif
        gLastOverrideState = g_overrides;
        activate_profiles( energySettings, 
                            currentPowerSource, 
                            kIOPMRemoveUnsupportedSettings);
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

    copy_all_settings = _copyPMSettings(kIOPMRemoveUnsupportedSettings);
    if(!copy_all_settings) return NULL;
    energySettings = isA_CFDictionary(CFDictionaryGetValue(copy_all_settings,currentPowerSource));
    if(energySettings) 
        return_val = CFDictionaryCreateCopy(kCFAllocatorDefault, energySettings);
    else 
        return_val = NULL;

    CFRelease(copy_all_settings);
    return return_val;
}

/* _copyPMSettings
 * The returned dictionary represents the "currently selected" 
 * per-power source settings.
 */
static CFDictionaryRef
_copyPMSettings(bool removeUnsupported)
{
    if(removeUnsupported) {
        return IOPMCopyActivePMPreferences();
    } else {
        return IOPMCopyUnabridgedActivePMPreferences();
    }
}


 /* activate_profiles
 *
 * A wrapper for IOPMActivatePMPreference. We get a chance here to apply "profiles"
 * to the Energy Saver settings before sending them to the kernel.
 * Profiles (like LidClosed or ForceLowSpeed) have affects like accelerating idle
 * times or forcing ReduceProcessorSpeed on.
 */
static IOReturn 
activate_profiles(CFDictionaryRef d, CFStringRef s, bool removeUnsupported)
{
    CFDictionaryRef                     energy_settings;
    CFMutableDictionaryRef              profiles_activated;
    CFMutableDictionaryRef              tmp;
    IOReturn                            ret;
    CFNumberRef                         n1, n0;
    int                                 one = 1;
    int                                 zero = 0;
    
    if(NULL == d) return kIOReturnBadArgument;
    
    if(NULL == s) s = CFSTR(kIOPMACPowerKey);

#if TARGET_OS_EMBEDDED
    CFNumberRef                         sleepSetting;

    energy_settings = (CFDictionaryRef)isA_CFDictionary(CFDictionaryGetValue(d, s));
    if(!energy_settings) return kIOReturnError;
    sleepSetting = (CFNumberRef)isA_CFNumber(CFDictionaryGetValue(energy_settings, CFSTR(kIOPMSystemSleepKey)));
    if (sleepSetting)
	CFNumberGetValue(sleepSetting, kCFNumberLongType, &gSleepSetting);
#endif

    if(g_overrides)
    {
#if !TARGET_OS_EMBEDDED
        energy_settings = (CFDictionaryRef)isA_CFDictionary(CFDictionaryGetValue(d, s));
        if(!energy_settings) return kIOReturnError;
#endif

        profiles_activated = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 
            CFDictionaryGetCount(energy_settings), energy_settings);
        if(!profiles_activated) return kIOReturnError;
        
        n1 = CFNumberCreate(0, kCFNumberIntType, &one);
        n0 = CFNumberCreate(0, kCFNumberIntType, &zero);
        // If the "force low speed" profile is set, flip the ReduceSpeed bit on
        if(g_overrides & kPMForceLowSpeedProfile)
        {
            if(n1) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMReduceSpeedKey), n1);
        }
        
        if(g_overrides & kPMForceHighSpeed)
        {
            if(n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMReduceSpeedKey), n0);
            if(n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMDynamicPowerStepKey), n0);
        }
        
        if(g_overrides & kPMPreventIdleSleep)
        {
            if(n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMSystemSleepKey), n0);
        }

        if(g_overrides & kPMPreventDisplaySleep)
        {
            if(n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMDisplaySleepKey), n0);
        }

        
        CFRelease(n0);
        CFRelease(n1);
        
        // Package the new, modified settings, in a way that 
        // IOPMActivatePMPreferences will read them
        tmp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(tmp, s, profiles_activated);

        ret = IOPMActivatePMPreference(tmp, s, removeUnsupported);

        CFRelease(profiles_activated);
        CFRelease(tmp);
    } else {
        ret = IOPMActivatePMPreference(d, s, removeUnsupported);
    }
    
    return ret;
}


__private_extern__ void 
PMSettings_prime(void)
{
    CFTypeRef                           ps_blob;

    // Open a connection to the Power Manager.
    gPowerManager = IOPMFindPowerManagement(MACH_PORT_NULL);
    if (gPowerManager == 0) return;

    // Activate non-power source specific, PM settings
    // namely disable sleep, where appropriate
    IOPMActivateSystemPowerSettings();

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
    energySettings = _copyPMSettings(kIOPMUnabridgedSettings);

    // send the initial configuration to the kernel
    if(energySettings) {
        activate_profiles( energySettings, 
                            currentPowerSource, 
                            kIOPMUnabridgedSettings);
    }

    // send initial power source info to drivers
    if(CFEqual(currentPowerSource, CFSTR(kIOPMACPowerKey)))
         IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMExternalPower);
    else IOPMSetAggressiveness(gPowerManager, kPMPowerSource, kIOPMInternalPower);
}
 
__private_extern__ void 
PMSettingsSupportedPrefsListHasChanged(void)
{
    // The "supported prefs have changed" notification is generated 
    // by a kernel driver annnouncing a new supported feature, or unloading
    // and removing support. Let's re-evaluate our known settings.

    PMSettingsPrefsHaveChanged();    
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
    // re-blast system-wide settings
    IOPMActivateSystemPowerSettings();

    // re-read preferences into memory
    if(energySettings) CFRelease(energySettings);

    energySettings = isA_CFDictionary(_copyPMSettings(
                                        kIOPMRemoveUnsupportedSettings));

    // push new preferences out to the kernel
    if(energySettings) {
        activate_profiles(energySettings, 
                            currentPowerSource,
                            kIOPMRemoveUnsupportedSettings);
    }    
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
        
        if(energySettings) {
            activate_profiles( energySettings, 
                                currentPowerSource,
                                kIOPMRemoveUnsupportedSettings);
        }
    }

}

/* BatteryPollingTimer
 * 
 */
__private_extern__ void
PMSettingsBatteriesHaveChanged(CFArrayRef battery_info)
{
    int                             flags;
    int                             changed_flags;
    int                             settings_changed = 0;
    static int                      old_flags = 0;
    io_iterator_t                   it;
    static io_registry_entry_t      root_domain = 0;

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
    if(changed_flags & kIOPMForceLowSpeed)
    {
    
        settings_changed = 1;
        if(flags & kIOPMForceLowSpeed)
            g_overrides |= kPMForceLowSpeedProfile;
        else g_overrides &= ~kPMForceLowSpeedProfile;
    }

    if(settings_changed)
    {
        if(energySettings) {
            activate_profiles( energySettings, 
                                currentPowerSource,
                                kIOPMRemoveUnsupportedSettings);
        }
    }
    
}

/* activateForcedSettings
 * 
 */
__private_extern__ IOReturn 
_activateForcedSettings(CFDictionaryRef forceSettings)
{
    // Calls to "pmset force" end up here
    energySettings = CFDictionaryCreateCopy(0, forceSettings);
    return activate_profiles( energySettings, 
                        currentPowerSource,
                        kIOPMRemoveUnsupportedSettings);
}
