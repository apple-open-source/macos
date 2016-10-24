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

#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <unistd.h>
#include <dlfcn.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#if !TARGET_OS_EMBEDDED
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#endif
#include <IOKit/IOHibernatePrivate.h>
#include <pthread.h>

#include "PMSettings.h"
#include "BatteryTimeRemaining.h"
#include "PrivateLib.h"
#include "PMStore.h"
#include "PMAssertions.h"


#define kIOPMSCPrefsPath    CFSTR("com.apple.PowerManagement.xml")
#define kIOPMAppName        CFSTR("PowerManagement configd")
#define kIOPMSCPrefsFile    "/Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist"


/* Arguments to CopyPMSettings functions */
enum {
    kIOPMUnabridgedSettings = false,
    kIOPMRemoveUnsupportedSettings = true
};

/* Global - energySettings
 * Keeps track of current Energy Saver settings.
 */
static CFDictionaryRef                  energySettings = NULL;

/*
 * Cached settings
 * These are cached at the start of powerd to set the
 * values after the features are published. Without this caching,
 * any call to set preferences will remove values for features
 * that are not yet published by the kexts. This results in loss
 * of user set values.
 */
static CFMutableDictionaryRef           bootTimePrefs = NULL;

/* Global - currentPowerSource
 * Keeps track of current power - battery or AC
 */
static CFStringRef                      currentPowerSource = NULL;

/* g_overrides
 * Tracks active PM usage profiles
 */
static unsigned long                    g_overrides = 0;
static unsigned long                    gLastOverrideState = 0;
static unsigned long                    gSleepSetting = -1;

static io_connect_t                     gPowerManager;

/* Tracking sleeping state */
static unsigned long                    deferredPSChangeNotify = 0;
static unsigned long                    _pmcfgd_impendingSleep = 0;


/* Forward Declarations */
static IOReturn activate_profiles(
        CFDictionaryRef                 d, 
        CFStringRef                     s, 
        bool                            removeUnsupported);
#if !TARGET_OS_EMBEDDED
static CFMutableDictionaryRef  copyBootTimePrefs();
static void mergeBootTimePrefs(void);
static void updatePowerNapSetting(CFMutableDictionaryRef prefs);
#endif


/* overrideSetting
 * Must be followed by a call to activateSettingOverrides
 */
__private_extern__ void overrideSetting
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


__private_extern__ bool
GetPMSettingBool(CFStringRef which)
{
    CFDictionaryRef     current_settings; 
    CFNumberRef         n;
    int                 nint = 0;
    CFStringRef         pwrSrc;
    
    if (!energySettings || !which) 
        return false;

    
    if (_getPowerSource() == kBatteryPowered)
       pwrSrc = CFSTR(kIOPMBatteryPowerKey);
    else
       pwrSrc = CFSTR(kIOPMACPowerKey);
    // Don't use 'currentPowerSource' here as that gets updated
    // little slowly after this function is called to get a setting
    // on new power source.
    current_settings = (CFDictionaryRef)isA_CFDictionary(
                         CFDictionaryGetValue(energySettings, pwrSrc));

    if (current_settings) {
        n = CFDictionaryGetValue(current_settings, which);
        if (n) {
            CFNumberGetValue(n, kCFNumberIntType, &nint);
        }
        return (0 != nint);
    }
    return false;
}



__private_extern__ IOReturn
GetPMSettingNumber(CFStringRef which, int64_t *value)
{
    CFDictionaryRef     current_settings; 
    CFNumberRef         n;
    CFStringRef         pwrSrc;
    
    if (!energySettings || !which) 
        return kIOReturnBadArgument;

    if (_getPowerSource() == kBatteryPowered)
       pwrSrc = CFSTR(kIOPMBatteryPowerKey);
    else
       pwrSrc = CFSTR(kIOPMACPowerKey);
    // Don't use 'currentPowerSource' here as that gets updated
    // little slowly after this function is called to get a setting
    // on new power source.
    current_settings = (CFDictionaryRef)isA_CFDictionary(
                         CFDictionaryGetValue(energySettings, pwrSrc));

    if (current_settings) {
        n = CFDictionaryGetValue(current_settings, which);
        if (isA_CFNumber(n)) {
            CFNumberGetValue(n, kCFNumberSInt64Type, value);
            return kIOReturnSuccess;
        }
    }
    return kIOReturnError;
}

/* Returns Display sleep time in minutes */
__private_extern__ IOReturn
getDisplaySleepTimer(uint32_t *displaySleepTimer)
{
    CFDictionaryRef     current_settings; 

    if (!energySettings || !displaySleepTimer) 
        return kIOReturnError;

    current_settings = (CFDictionaryRef)isA_CFDictionary(
                            CFDictionaryGetValue(energySettings, currentPowerSource));
    if (getAggressivenessValue(current_settings, CFSTR(kIOPMDisplaySleepKey), 
                                    kCFNumberSInt32Type, displaySleepTimer) ) {
        return kIOReturnSuccess;
    }

    return kIOReturnError;
}

/* Returns Idle sleep time in minutes */
__private_extern__ IOReturn
getIdleSleepTimer(unsigned long *idleSleepTimer)
{
    CFDictionaryRef     current_settings; 

    if (!energySettings || !idleSleepTimer) 
        return kIOReturnError;

    if (gSleepSetting != -1) {
        *idleSleepTimer = gSleepSetting;
        return kIOReturnSuccess;
    }

    current_settings = (CFDictionaryRef)isA_CFDictionary(
                            CFDictionaryGetValue(energySettings, currentPowerSource));
    if (getAggressivenessValue(current_settings, CFSTR(kIOPMSystemSleepKey), 
                                    kCFNumberSInt32Type, (uint32_t *)idleSleepTimer) ) {
        return kIOReturnSuccess;
    }

    return kIOReturnError;
}

// Providing activateSettingsOverrides to PMAssertions.c
// So that it may set multiple assertions without triggering a prefs
// re-evaluate each time. PMAssertions.c can call overrideSetting() n times
// and only call activateSettingsOverrides once.
__private_extern__ void
activateSettingOverrides(void)
{
    if (!energySettings) 
        return;

    if (gLastOverrideState != g_overrides)
    {
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

        gLastOverrideState = g_overrides;
        activate_profiles( energySettings, 
                            currentPowerSource, 
                            kIOPMRemoveUnsupportedSettings);
    }
}

__private_extern__ void 
PMSettingsCapabilityChangeNotification(const struct IOPMSystemCapabilityChangeParameters * p)
{
    if (CAPABILITY_BIT_CHANGED(p->fromCapabilities, p->toCapabilities, kIOPMSystemPowerStateCapabilityCPU))
    {
        if (BIT_IS_SET(p->toCapabilities, kIOPMSystemPowerStateCapabilityCPU) &&
            BIT_IS_SET(p->changeFlags, kIOPMSystemCapabilityDidChange))
        {
            // did wake
            _pmcfgd_impendingSleep = 0;
            if (deferredPSChangeNotify)
            {
                bool hasAcPower = (currentPowerSource && CFEqual(currentPowerSource, CFSTR(kIOPMACPowerKey)));
                deferredPSChangeNotify = 0;
                _pmcfgd_impendingSleep = 0;
                IOPMSetAggressiveness(gPowerManager, kPMPowerSource, hasAcPower ? kIOPMExternalPower : kIOPMInternalPower);
            }
        }
        else if (BIT_IS_NOT_SET(p->toCapabilities, kIOPMSystemPowerStateCapabilityCPU) &&
            BIT_IS_SET(p->changeFlags, kIOPMSystemCapabilityWillChange))
        {
            // will sleep
            _pmcfgd_impendingSleep = 1;
        }
    }
}

__private_extern__ CFDictionaryRef
PMSettings_CopyActivePMSettings(void)
{
    CFDictionaryRef         copy_all_settings;
    CFDictionaryRef         energySettings;
    CFDictionaryRef         return_val;

    copy_all_settings = IOPMCopyActivePMPreferences();
    if(!copy_all_settings) return NULL;
    energySettings = isA_CFDictionary(CFDictionaryGetValue(copy_all_settings,currentPowerSource));
    if(energySettings) 
        return_val = CFDictionaryCreateCopy(kCFAllocatorDefault, energySettings);
    else 
        return_val = NULL;

    CFRelease(copy_all_settings);
    return return_val;
}


/* _DWBT_enabled() returns true if the system supports DWBT and if user has opted in */
__private_extern__ bool _DWBT_enabled(void)
{
   CFDictionaryRef     current_settings; 
   CFNumberRef         n;
   int                 nint = 0;


   if (!energySettings) 
       return false;

    current_settings = (CFDictionaryRef)isA_CFDictionary(
                         CFDictionaryGetValue(energySettings, CFSTR(kIOPMACPowerKey)));
    if (current_settings) {
        n = CFDictionaryGetValue(current_settings, CFSTR(kIOPMDarkWakeBackgroundTaskKey));
        if (n) {
            CFNumberGetValue(n, kCFNumberIntType, &nint);
        }
        return (0 != nint);
    }

    return false;
}

/* _DWBT_allowed() tells if a DWBT wake can be scheduled at this moment */
__private_extern__ bool
_DWBT_allowed(void)
{
#if TARGET_OS_EMBEDDED
    return false;
#else
    return ( (GetPMSettingBool(CFSTR(kIOPMDarkWakeBackgroundTaskKey))) &&
             (kACPowered == _getPowerSource()) );
#endif

}

/* Is Sleep Services allowed */
__private_extern__ bool _SS_allowed(void)
{
#if TARGET_OS_EMBEDDED
    return false;
#else
    if (_DWBT_allowed())
        return true;

    return ( (GetPMSettingBool(CFSTR(kIOPMDarkWakeBackgroundTaskKey))) &&
             (kBatteryPowered == _getPowerSource()) );
#endif

}

/**************************************************/

 /* activate_profiles
 *
 * A wrapper for ActivatePMSettings. We get a chance here to apply modifications
 * to the Energy Saver settings before sending them to the kernel.
 * Profiles (like LidClosed or ForceLowSpeed) have affects like accelerating idle
 * times or forcing ReduceProcessorSpeed on.
 */
static IOReturn 
activate_profiles(CFDictionaryRef d, CFStringRef s, bool removeUnsupported)
{
    CFDictionaryRef                     energy_settings;
    CFDictionaryRef                     activePMPrefs = NULL;
    CFMutableDictionaryRef              profiles_activated;
    IOReturn                            ret;
    CFNumberRef                         n1, n0;
    CFNumberRef                         sleepSetting;
    int                                 one = 1;
    int                                 zero = 0;
    
    if(NULL == d) {
        return kIOReturnBadArgument;
    }
    
    if(NULL == s) {
        s = CFSTR(kIOPMACPowerKey);
    }
    
    energy_settings = (CFDictionaryRef)isA_CFDictionary(CFDictionaryGetValue(d, s));
    if (!energy_settings) {
        return kIOReturnError;
    }


    sleepSetting = (CFNumberRef)isA_CFNumber(CFDictionaryGetValue(energy_settings, CFSTR(kIOPMSystemSleepKey)));
    if (sleepSetting) {
        CFNumberGetValue(sleepSetting, kCFNumberLongType, &gSleepSetting);
    }

    if(g_overrides)
    {
        profiles_activated = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 
            CFDictionaryGetCount(energy_settings), energy_settings);
        if(!profiles_activated) 
            return kIOReturnError;
        
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
        if (g_overrides & kPMPreventDiskSleep)
        {
            if (n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMDiskSleepKey), n0);
        }
        if (g_overrides & kPMPreventWakeOnLan)
        {
            if (n0) CFDictionarySetValue(profiles_activated, CFSTR(kIOPMWakeOnLANKey), n0);
        }

        
        if (n0)
            CFRelease(n0);
        if (n1)
            CFRelease(n1);
        
        ret = ActivatePMSettings(profiles_activated, removeUnsupported);

        CFRelease(profiles_activated);
    } else {
        ret = ActivatePMSettings(energy_settings, removeUnsupported);
    }
        
    activePMPrefs = SCDynamicStoreCopyValue(_getSharedPMDynamicStore(), 
                                            CFSTR(kIOPMDynamicStoreSettingsKey));
    
    // If there isn't currently a value for kIOPMDynamicStoreSettingsKey,
    //   or the current value is different than the new value,
    // Put the new settings in the SCDynamicStore for interested apps.
    
    if( !isA_CFDictionary(activePMPrefs) || !CFEqual(activePMPrefs, energy_settings) )
    {
        PMStoreSetValue(CFSTR(kIOPMDynamicStoreSettingsKey), energy_settings);
    }

    if (activePMPrefs)
        CFRelease(activePMPrefs);

    return ret;
}


__private_extern__ void PMSettings_prime(void)
{

#if !TARGET_OS_EMBEDDED
    bootTimePrefs = copyBootTimePrefs();
    updatePowerNapSetting(bootTimePrefs);
#endif

    // Open a connection to the Power Manager.
    gPowerManager = IOPMFindPowerManagement(MACH_PORT_NULL);
    if (gPowerManager == 0) return;

    // Activate non-power source specific, PM settings
    // namely disable sleep, where appropriate
    IOPMActivateSystemPowerSettings();

    /*
     * determine current power source for separate Battery/AC settings
     */
    int powersource = getActivePSType();
    if (kIOPSProvidedByExternalBattery == powersource) {
        currentPowerSource = CFSTR(kIOPMUPSPowerKey);
    } else if (kIOPSProvidedByBattery == powersource) {
        currentPowerSource = CFSTR(kIOPMBatteryPowerKey);
    } else {
        currentPowerSource = CFSTR(kIOPMACPowerKey);
    }

#if !TARGET_OS_EMBEDDED
    // Merge bootTimePrefs to any current settings
    mergeBootTimePrefs();
#endif

    // load the initial configuration from the database
    energySettings = IOPMCopyActivePMPreferences();

    // send the initial configuration to the kernel
    if(energySettings) {
        activate_profiles( energySettings, 
                            currentPowerSource, 
                            kIOPMRemoveUnsupportedSettings);
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
    // First check if cached settings can be applied to newly discovered features

#if !TARGET_OS_EMBEDDED
    mergeBootTimePrefs();
#endif
    PMSettingsPrefsHaveChanged();    
}

#if !TARGET_OS_EMBEDDED
static void updatePowerNapSetting(CFMutableDictionaryRef prefs)
{
    char     *model = NULL;
    uint32_t majorRev;
    uint32_t minorRev;
    IOReturn rc;

    CFDictionaryRef systemSettings = NULL; 
    CFMutableDictionaryRef prefsForSrc;

    if (!prefs) {
        INFO_LOG("No prefs to update\n");
        return;
    }
    prefsForSrc = (CFMutableDictionaryRef)CFDictionaryGetValue(prefs, CFSTR(kIOPMACPowerKey));
    if (!isA_CFDictionary(prefsForSrc)) {
        INFO_LOG("Invalid prefs found\n");
        goto exit;
    }

    rc = IOCopyModel(&model, &majorRev, &minorRev);
    if (rc != kIOReturnSuccess) {
        INFO_LOG("Failed to get the model name\n");
        return;
    }

    systemSettings = IOPMCopySystemPowerSettings();
    if (isA_CFDictionary(systemSettings) && CFDictionaryContainsKey(systemSettings, CFSTR(kIOPMUpdateDarkWakeBGSettingKey))) {
        // key exists. That means, powernap setting is already updated once
        INFO_LOG("UpdateDarkWakeBGSetting key already exists\n");
        goto exit;
    }

    rc = IOPMSetSystemPowerSetting(CFSTR(kIOPMUpdateDarkWakeBGSettingKey), kCFBooleanTrue);
    if (rc != kIOReturnSuccess) {
        ERROR_LOG("Failed to set system setting 'UpdateDarkWakeBGSetting'. rc=0x%x\n", rc);
        goto exit;
    }

    // Remove any autopower off delay settings
    IOPMSetPMPreference(CFSTR(kIOPMAutoPowerOffDelayKey), NULL, NULL);
    CFDictionaryRemoveValue(prefsForSrc, CFSTR(kIOPMAutoPowerOffDelayKey));
    CFDictionarySetValue(prefs, CFSTR(kIOPMACPowerKey), prefsForSrc);


    if ( (!strncmp(model, "iMac", sizeof("iMac")) && ((majorRev > 13) && (majorRev <= 17))) ||
            (!strncmp(model, "Macmini", sizeof("Macmini")) && (majorRev == 7))) {

        int one = 1;
        CFNumberRef n1 = CFNumberCreate(0, kCFNumberIntType, &one);
        if (n1) {
            CFDictionarySetValue(prefsForSrc, CFSTR(kIOPMDarkWakeBackgroundTaskKey), n1);
            CFRelease(n1);

            CFDictionarySetValue(prefs, CFSTR(kIOPMACPowerKey), prefsForSrc);

            INFO_LOG("Changed powernap setting\n");
        }
    }
    else {
        INFO_LOG("Powernap setting need not be changed\n");
    }

    INFO_LOG("Updated prefs: %@\n", prefs);

exit:
    if (systemSettings) {
        CFRelease(systemSettings);
    }
    if (model != NULL) {
        free(model);
    }
    return;
}

bool mergePrefsForSrc(CFMutableDictionaryRef current, CFMutableDictionaryRef cachedPrefs)
{
    bool modified = false;
    int i;

    CFIndex cnt = CFDictionaryGetCount(cachedPrefs);
    if (cnt == 0) {
        return false;
    }

    CFStringRef *keys = (CFStringRef *)malloc(sizeof(CFStringRef) * cnt);
    CFTypeRef   *values = (CFTypeRef *)malloc(sizeof(CFTypeRef) * cnt);
    if (!keys || !values) {
        goto exit;
    }

    CFDictionaryGetKeysAndValues(cachedPrefs,
                    (const void **)keys, (const void **)values);
    for(i=0; i<cnt; i++)
    {
        if (!CFDictionaryContainsKey(current, keys[i])) {
            continue;
        }

        // Set the cached value for this key and remove it
        // from the cache
        CFDictionarySetValue(current, keys[i], values[i]);
        CFDictionaryRemoveValue(cachedPrefs, keys[i]);
        modified = true;
    }

exit:
    if (keys) {
        free(keys);
    }
    if (values) {
        free(values);
    }

    return modified;
}

// Merge existing settings with any cached at boot.
// These cached settings may have values for features that are
// not yet published or just published
void mergeBootTimePrefs(void)
{
    CFMutableDictionaryRef currentForSrc, cachedForSrc;
    CFMutableDictionaryRef currentPrefs = NULL;
    bool modified = false;
    bool prefsUpdated = false;

    if (!bootTimePrefs) {
        return;
    }

    currentPrefs = IOPMCopyPMPreferences();
    INFO_LOG("Saved prefs: %@\n", currentPrefs);

    if (!isA_CFDictionary(currentPrefs)) {
        return;
    }

    CFStringRef pwrSrc[] = {CFSTR(kIOPMACPowerKey), CFSTR(kIOPMBatteryPowerKey), CFSTR(kIOPMUPSPowerKey)};

    for (int i = 0; i < sizeof(pwrSrc)/sizeof(pwrSrc[0]); i++) {

        currentForSrc = (CFMutableDictionaryRef)CFDictionaryGetValue(currentPrefs, pwrSrc[i]);
        cachedForSrc = (CFMutableDictionaryRef)CFDictionaryGetValue(bootTimePrefs, pwrSrc[i]);

        if (isA_CFDictionary(currentForSrc) && isA_CFDictionary(cachedForSrc)) {
            modified = false;
            modified = mergePrefsForSrc(currentForSrc, cachedForSrc);

            INFO_LOG("Merging cached prefs for src %@ to: %@\n", pwrSrc[i], currentForSrc);
            if(modified) {
                CFDictionarySetValue(currentPrefs, pwrSrc[i], currentForSrc);
                prefsUpdated = true;
            }

            if (CFDictionaryGetCount(cachedForSrc) == 0) {
                CFDictionaryRemoveValue(bootTimePrefs, pwrSrc[i]);
            }
        }
    }


    if (CFDictionaryGetCount(bootTimePrefs) == 0) {
        CFRelease(bootTimePrefs);
        bootTimePrefs = NULL;
    }

    if (prefsUpdated) {
        IOPMRemoveIrrelevantProperties(currentPrefs);
        IOPMSetPMPreferences(currentPrefs);
    }

    CFRelease(currentPrefs);
    return;
}
#endif

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

    energySettings = IOPMCopyActivePMPreferences();

    // push new preferences out to the kernel
    if(isA_CFDictionary(energySettings)) {
        activate_profiles(energySettings, 
                            currentPowerSource,
                            kIOPMRemoveUnsupportedSettings);
    } else {
        if (energySettings) {
            CFRelease(energySettings);
        }
        energySettings = NULL;
    }
    PMAssertions_SettingsHaveChanged();
    
    return;
}


/* PMSettingsPSChange
 *
 * A power source has changed. Has the current power provider changed?
 * If so, get new settings down to the kernel.
 */
__private_extern__ void PMSettingsPSChange(void)
{
    CFStringRef     newPowerSource;
    
    int powersource = getActivePSType();
    if (kIOPSProvidedByExternalBattery == powersource) {
        newPowerSource = CFSTR(kIOPMUPSPowerKey);
    } else if (kIOPSProvidedByBattery == powersource) {
        newPowerSource = CFSTR(kIOPMBatteryPowerKey);
    } else {
        newPowerSource = CFSTR(kIOPMACPowerKey);
    }

    if(!currentPowerSource
       || !CFEqual(currentPowerSource, newPowerSource))
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

/* activateForcedSettings
 * 
 */
__private_extern__ IOReturn 
_activateForcedSettings(CFDictionaryRef forceSettings)
{
    // Calls to "pmset force" end up here
    return activate_profiles( forceSettings, 
                        currentPowerSource,
                        kIOPMRemoveUnsupportedSettings);
}

#if !TARGET_OS_EMBEDDED

/****************** SCPrefs to CFPrefs conversion **********************/
/*
 * List of keys that appear in the energy saver preferences
 * panel. These values should carry over from the host
 * machine to the target machine. All other preferences
 * should remain default values, since some values do not
 * make sense to migrate over (e.g. standby key).
 */
CFStringRef energyPrefsKeys[] = {
    CFSTR(kIOPMDarkWakeBackgroundTaskKey),
    CFSTR(kIOPMDiskSleepKey),
    CFSTR(kIOPMDisplaySleepKey),
    CFSTR(kIOPMDisplaySleepUsesDimKey),
    CFSTR(kIOPMReduceBrightnessKey),
    CFSTR(kIOPMRestartOnPowerLossKey),
    CFSTR(kIOPMSystemSleepKey),
    CFSTR(kIOPMWakeOnLANKey)
};

CFStringRef systemSettingKeys[] = {
    kIOPMSleepDisabledKey,
    CFSTR(kIOPMDestroyFVKeyOnStandbyKey)
};


static void mergeOldPrefsForSrc(CFDictionaryRef oldPrefs,
                                CFMutableDictionaryRef newPrefs,
                                CFStringRef pwrSrc)
{
    CFDictionaryRef         newDict = NULL;
    CFDictionaryRef         oldDict = NULL;
    int                     i;

    CFMutableDictionaryRef  mergedDict = NULL;


    const int kEnergyPrefsCount = sizeof(energyPrefsKeys)/sizeof(energyPrefsKeys[0]);

    oldDict = CFDictionaryGetValue(oldPrefs, pwrSrc);
    newDict = CFDictionaryGetValue(newPrefs, pwrSrc);

    if (newDict) {
        mergedDict = CFDictionaryCreateMutableCopy(0, CFDictionaryGetCount(newDict), newDict);
    }
    else {
        mergedDict  = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    }

    if (!mergedDict) {
        return;
    }

    if (oldDict) {
        for (i = 0; i < kEnergyPrefsCount; i++) {
            CFNumberRef num = CFDictionaryGetValue(oldDict, energyPrefsKeys[i]);
            if (num) {
                CFDictionarySetValue(mergedDict, energyPrefsKeys[i], num);
            }
        }
    }

    CFDictionarySetValue(newPrefs, pwrSrc, mergedDict);
    CFRelease(mergedDict);

    return;
}



/*
 * The "Custom Profile" dictionary may still exist in the SCPrefs
 * even though the user is using default preferences. We have to
 * check the ActivePowerProfiles dictionary to see if custom
 * settings are being used (a value of -1 in the dictionary).
 */
static bool usingDefaults(CFDictionaryRef profiles, CFStringRef pwrSrc)
{
    CFNumberRef numRef = NULL;
    int val = 0;

    numRef = CFDictionaryGetValue(profiles, pwrSrc);
    if (numRef) {
        CFNumberGetValue(numRef, kCFNumberIntType, &val);
        if (val == -1) {
            return false;
        }
    }

    return true;
}

// Check /Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist
// for old power management settings.
static CFDictionaryRef copyOldPrefs(void)
{
    SCPreferencesRef prefsRef       = NULL;
    CFDictionaryRef  activeProfs    = NULL;
    CFDictionaryRef  custom         = NULL;
    CFDictionaryRef  systemSettings = NULL;

    bool                    forceDelete         = false;;
    CFDictionaryRef         upsShutdownSettings = NULL;
    CFMutableDictionaryRef  prefs               = NULL;


    CFMutableDictionaryRef  options = NULL;


    options = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(options, kSCPreferencesOptionRemoveWhenEmpty, kCFBooleanTrue);

    prefsRef = SCPreferencesCreateWithOptions(0, kIOPMAppName, kIOPMSCPrefsPath, NULL, options);
    if (!prefsRef) {
        INFO_LOG("Couldn't read prefs from system configuration\n");
        goto exit;
    }

    activeProfs = SCPreferencesGetValue(prefsRef, CFSTR("ActivePowerProfiles"));
    if (!activeProfs) {
        INFO_LOG("Active profiles information is not found\n");
    }

    custom = SCPreferencesGetValue(prefsRef, CFSTR("Custom Profile"));
    if (custom) {
        // Create a copy, since custom will be destroyed when we release PrefsRef
        prefs = CFDictionaryCreateMutableCopy(0, 0, custom);
    }
    else {
        INFO_LOG("No custom preferences found in system configuration.\n");
        prefs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    if (!prefs) {
        INFO_LOG("Failed to create a mutable copy of custom preferences\n");
        goto exit;
    }

    if (activeProfs) {
        if (usingDefaults(activeProfs, CFSTR(kIOPMACPowerKey))) {
            CFDictionaryRemoveValue(prefs, CFSTR(kIOPMACPowerKey));
        }
        if (usingDefaults(activeProfs, CFSTR(kIOPMBatteryPowerKey))) {
            CFDictionaryRemoveValue(prefs, CFSTR(kIOPMBatteryPowerKey));
        }
        if (usingDefaults(activeProfs, CFSTR(kIOPMUPSPowerKey))) {
            CFDictionaryRemoveValue(prefs, CFSTR(kIOPMUPSPowerKey));
        }
    }

    systemSettings = SCPreferencesGetValue(prefsRef, CFSTR(kIOPMSystemPowerSettingsKey));
    if (systemSettings) {
        CFDictionarySetValue(prefs, CFSTR(kIOPMSystemPowerSettingsKey), systemSettings);
    }

    upsShutdownSettings = SCPreferencesGetValue(prefsRef, CFSTR(kIOPMDefaultUPSThresholds));
    if (upsShutdownSettings) {
        CFDictionarySetValue(prefs, CFSTR(kIOPMDefaultUPSThresholds), upsShutdownSettings);
    }


exit:
    if (!SCPreferencesRemoveAllValues(prefsRef) || !SCPreferencesCommitChanges(prefsRef)) {
        ERROR_LOG("Failed to remove or commit SC prefs file\n");

        // Force delete the prefs file
        forceDelete = true;
    }

    if (prefsRef)   CFRelease(prefsRef);
    if (options)    CFRelease(options);

    if (forceDelete) {
        if (unlink(kIOPMSCPrefsFile)) {
            ERROR_LOG("Failed to delete SC prefs file: %d\n", errno);
        }
    }
    return prefs;
}



/*
 * This function merges old SC based prefs on disk and new CF based prefs
 * on disk and returns on single dictionary
 */
CFMutableDictionaryRef  copyBootTimePrefs()
{
    CFDictionaryRef         oldPrefs = NULL;
    CFMutableDictionaryRef  mergedPrefs = NULL;
    IOReturn                ret      = kIOReturnError;
    struct stat             info;
    CFDictionaryRef         upsShutdownSettings = NULL;
    CFDictionaryRef         systemSettings    = NULL;

    // Check for old SCPreferences
    if ((lstat(kIOPMSCPrefsFile, &info) != 0) || !S_ISREG(info.st_mode)) {
        INFO_LOG("No SC based prefs file found\n");
    }
    else {
        oldPrefs = copyOldPrefs();
        if (!oldPrefs) {
            INFO_LOG("No SC Preferences to migrate\n");
        }
    }


    mergedPrefs = IOPMCopyPreferencesOnFile();
    if (!mergedPrefs) {
        INFO_LOG("No CF based prefs found on file\n");
        if (oldPrefs) {
            mergedPrefs = CFDictionaryCreateMutableCopy(0, 0, oldPrefs);
            CFRelease(oldPrefs);
            oldPrefs = NULL;
        }
    }


    if (!oldPrefs) {
        goto exit;
    }

    INFO_LOG("Old preferences found. Saving to new preferences.\n");
    mergeOldPrefsForSrc(oldPrefs, mergedPrefs, CFSTR(kIOPMACPowerKey));
    mergeOldPrefsForSrc(oldPrefs, mergedPrefs, CFSTR(kIOPMBatteryPowerKey));
    mergeOldPrefsForSrc(oldPrefs, mergedPrefs, CFSTR(kIOPMUPSPowerKey));


    upsShutdownSettings = CFDictionaryGetValue(oldPrefs, CFSTR(kIOPMDefaultUPSThresholds));
    if (upsShutdownSettings) {
        ret = IOPMSetUPSShutdownLevels(CFSTR(kIOPMDefaultUPSThresholds), upsShutdownSettings);
        if (ret == kIOReturnSuccess) {
            INFO_LOG("UPS shutdown levels migrated successfully!\n");
        } else {
            ERROR_LOG("[ERROR] Failed to migrate UPS shutdown levels(0x%x).\n", ret);
        }
    }

    systemSettings = CFDictionaryGetValue(oldPrefs, CFSTR(kIOPMSystemPowerSettingsKey));
    const int kSystemSettingCount = sizeof(systemSettingKeys)/sizeof(systemSettingKeys[0]);
    if (systemSettings) {
        for (int i = 0; i < kSystemSettingCount; i++) {
            CFTypeRef val;
            if (CFDictionaryGetValueIfPresent(systemSettings, systemSettingKeys[i], &val)) {
                ret = IOPMSetSystemPowerSetting(systemSettingKeys[i],val);
                if (ret != kIOReturnSuccess) {
                    ERROR_LOG("[ERROR] Failed to migrate system settings key %@\n", systemSettingKeys[i]);
                }
            }
        }
    }

    INFO_LOG("Merged prefs on start: %@\n", mergedPrefs);

exit:
    if (oldPrefs)   CFRelease(oldPrefs);

    return mergedPrefs;

}


/***********************************************************************/

#endif
