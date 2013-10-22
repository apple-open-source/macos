/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include <sys/cdefs.h>
#include <TargetConditionals.h>

#include "IOSystemConfiguration.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOHibernatePrivate.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>
#include <sys/syslog.h>
#include "IOPMLib.h"
#include "IOPMLibPrivate.h"
#include "powermanagement.h"

#include <asl.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>

#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <fcntl.h>
//#include <pthread.h>
//#include <sys/stat.h>
//#include <sys/sysctl.h>
//#include <sys/mount.h>

#define kSecondsIn5Years            157680000

typedef struct {
    const char *keyName;
    uint32_t    defaultValueAC;
    uint32_t    defaultValueBattery;
} PMSettingDescriptorStruct;
#ifndef kIOPMAutoPowerOffEnabledKey
#define kIOPMAutoPowerOffEnabledKey "poweroffenabled"
#define kIOPMAutoPowerOffDelayKey "poweroffdelay"
#endif
PMSettingDescriptorStruct defaultSettings[] =
{   /* Setting Name                             AC - Battery */
    {kIOPMDisplaySleepKey,                              5,  5},
    {kIOPMDiskSleepKey,                                 10, 5},
    {kIOPMSystemSleepKey,                               10, 5},
    {kIOPMWakeOnLANKey,                                 0,  0},
    {kIOPMWakeOnRingKey,                                0,  0},
    {kIOPMRestartOnPowerLossKey,                        0,  0},
    {kIOPMWakeOnACChangeKey,                            0,  0},
    {kIOPMSleepOnPowerButtonKey,                        1,  0},
    {kIOPMWakeOnClamshellKey,                           1,  1},
    {kIOPMReduceBrightnessKey,                          0,  1},
    {kIOPMDisplaySleepUsesDimKey,                       1,  1},
    {kIOPMMobileMotionModuleKey,                        1,  1},
    {kIOPMTTYSPreventSleepKey,                          1,  1},
    {kIOPMGPUSwitchKey,                                 0,  1},
    {kIOPMPrioritizeNetworkReachabilityOverSleepKey,    0,  0},
    {kIOPMDeepSleepEnabledKey,                          0,  0},
    {kIOPMDeepSleepDelayKey,                            0,  0},
    {kIOPMDarkWakeBackgroundTaskKey,                    1,  0},
    {kIOPMAutoPowerOffEnabledKey,                       0,  0},
    {kIOPMAutoPowerOffDelayKey,                         0,  0}
};

static const int kPMSettingsCount = sizeof(defaultSettings)/sizeof(PMSettingDescriptorStruct);

/* com.apple.PowerManagement.plist general keys 
 */
#define kIOPMPrefsPath                                  CFSTR("com.apple.PowerManagement.xml")
#define kIOPMAppName                                    CFSTR("PowerManagement configd")

#define kIOPMSystemPowerKey                             CFSTR("SystemPowerSettings")
#define kIOPMCustomProfileKey                           CFSTR("Custom Profile")

#define kIOHibernateDefaultFile                         "/var/vm/sleepimage"

#define kIOPMPrefsPath                                  CFSTR("com.apple.PowerManagement.xml")
#define kIOPMAppName                                    CFSTR("PowerManagement configd")

/* IOPMRootDomain property keys for default settings
 */
#define kIOPMSystemDefaultProfilesKey                   "SystemPowerProfiles"
#define kIOPMSystemDefaultOverrideKey                   "SystemPowerProfileOverrideDict"

/* Keys for Cheetah Energy Settings shim
 */
#define kCheetahDimKey                                  CFSTR("MinutesUntilDisplaySleeps")
#define kCheetahDiskKey                                 CFSTR("MinutesUntilHardDiskSleeps")
#define kCheetahSleepKey                                CFSTR("MinutesUntilSystemSleeps")
#define kCheetahRestartOnPowerLossKey                   CFSTR("RestartOnPowerLoss")
#define kCheetahWakeForNetworkAccessKey                 CFSTR("WakeForNetworkAdministrativeAccess")
#define kCheetahWakeOnRingKey                           CFSTR("WakeOnRing")

// Supported Feature bitfields for IOPMrootDomain Supported Features
enum {
    kIOPMSupportedOnAC              = 1<<0,
    kIOPMSupportedOnBatt            = 1<<1,
    kIOPMSupportedOnUPS             = 1<<2
};

/* Power sources
 *
 */
#define kPowerSourcesCount          3
#define kPowerProfilesCount         5

enum {
    kIOPMKeepUnsupportedPreferences = false,
    kIOPMRemoveUnsupportedPreferences = true
};


/* IOPMPrefsNotificationCreateRunLoopSource
 * user_callback_context supports IOPMPrefsCallback
 */
typedef struct {
    IOPMPrefsCallbackType       callback;
    void                        *context;
} user_callback_context;

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
// Forwards

static CFStringRef getPowerSourceString(int i);

static void mergeUserDefaultOverriddenSettings(
               CFMutableDictionaryRef es);
static int addDefaultEnergySettings( 
                CFMutableDictionaryRef sys );
static void addSystemProfileEnergySettings(
                CFDictionaryRef user_profile_selections,
                CFMutableDictionaryRef passed_in);
__private_extern__ io_registry_entry_t getPMRootDomainRef(void);
static CFStringRef supportedNameForPMName( CFStringRef pm_name );
static bool featureSupportsPowerSource(
                CFTypeRef                       featureDetails, 
                CFStringRef                     power_source);
static void IOPMRemoveIrrelevantProperties(
                CFMutableDictionaryRef energyPrefs);
static int getCheetahPumaEnergySettings(
                CFMutableDictionaryRef energyPrefs);

#define  kOverWriteDuplicates       true
#define  kKeepOriginalValues        false
static void mergeDictIntoMutable(
                CFMutableDictionaryRef          target,
                CFDictionaryRef                 overrides,
                bool                            overwrite);
static CFDictionaryRef  _copyActivePMPreferences(
                bool    removeUnsupportedSettings);
static CFArrayRef _copySystemProvidedProfiles(void);
static CFArrayRef _createDefaultSystemProfiles(void);
static CFDictionaryRef _createDefaultProfileSelections(void);
static CFDictionaryRef _createAllCustomProfileSelections(void);
static int _isActiveProfileDictValid( CFDictionaryRef p );
static void _purgeUnsupportedPowerSources( CFMutableDictionaryRef p );
static void ioCallout( 
                SCDynamicStoreRef store __unused, 
                CFArrayRef keys __unused, 
                void *ctxt);

static IOReturn readAllPMPlistSettings(
                bool    removeUnsupportedSettings,
                CFMutableDictionaryRef *customSettings, 
                CFDictionaryRef *profileSelections,
                bool    *defaultSettings);

IOReturn _pm_connect(mach_port_t *newConnection);
IOReturn _pm_disconnect(mach_port_t connection);
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/


/**************************************************
*
* Energy Saver Preferences
*
**************************************************/

CFDictionaryRef IOPMCopyCustomPMPreferences(void)
{
    CFMutableDictionaryRef      copiedPMPrefs = NULL;
    CFDictionaryRef         immutableRetPrefs = NULL;
    
    /* IOPMCopyCustomPMPreferences returns the same thing IOPMCopyPMPreferences
     * does.  This is the recommended version to use; for clarity.
     */
    
    copiedPMPrefs = IOPMCopyPMPreferences();
    if( copiedPMPrefs ) {
        // Turn it from mutable to immutable
        immutableRetPrefs = CFDictionaryCreateCopy( 
                        kCFAllocatorDefault, (CFDictionaryRef)copiedPMPrefs );
        CFRelease(copiedPMPrefs); 
        return immutableRetPrefs;
    } else {
        return NULL;
    }        
}

/**************************************************/

CFMutableDictionaryRef IOPMCopyPMPreferences(void)
{
    IOReturn                ret;
    CFMutableDictionaryRef  settings = NULL;

    ret = readAllPMPlistSettings( 
                        kIOPMRemoveUnsupportedPreferences, 
                        &settings, 
                        NULL, NULL );
    
    if(kIOReturnSuccess == ret) {
        return settings;
    } else {
        return NULL;
    }
}

/**************************************************/
#if !TARGET_OS_IPHONE

IOReturn IOPMCopyPMSetting(
    CFStringRef key,
    CFStringRef power_source,
    CFTypeRef *outValue)
{
    CFDictionaryRef ActiveSettings  = NULL;
    CFDictionaryRef perPS           = NULL;
    CFStringRef     usePowerSource  = power_source;
    CFTypeRef       psblob          = NULL;
    bool            supported       = false;

    IOReturn        ret;

    if (!key || !outValue) {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    *outValue = 0;

    if (!usePowerSource) {
        psblob = IOPSCopyPowerSourcesInfo();
        if (psblob) {
            usePowerSource = IOPSGetProvidingPowerSourceType(psblob);
        } else {
            usePowerSource = CFSTR(kIOPMACPowerKey);
        }
    }

    supported = IOPMFeatureIsAvailable(key, usePowerSource);
    
    if (!supported) {
        ret = kIOReturnUnsupported;
        goto exit;
    }
    
    ActiveSettings = _copyActivePMPreferences(true);
    if (ActiveSettings) {
        perPS = (CFDictionaryRef)CFDictionaryGetValue(
                          ActiveSettings, usePowerSource);
    
        if (perPS) {
            *outValue = CFDictionaryGetValue(perPS, key);
        } 
    }
    
    if (*outValue) {
        CFRetain(*outValue);
        ret = kIOReturnSuccess;
    } else {
        ret = kIOReturnNotFound;
    }
    
    if (ActiveSettings) {
        CFRelease(ActiveSettings);
    }
    
exit:
    if (psblob) {
        CFRelease(psblob);
    }
    return ret;
}

#endif
/**************************************************/

CFDictionaryRef IOPMCopyActivePMPreferences(void)
{
    return _copyActivePMPreferences(kIOPMRemoveUnsupportedPreferences);    
}

CFDictionaryRef IOPMCopyUnabridgedActivePMPreferences(void)
{
    return _copyActivePMPreferences(kIOPMKeepUnsupportedPreferences);    
}

static CFDictionaryRef  _copyActivePMPreferences(
    bool                            removeUnsupportedSettings) 
{
    CFDictionaryRef                 active_profiles = NULL;
    CFArrayRef                      system_profiles = NULL;
    CFMutableDictionaryRef          custom_settings = NULL;
    CFMutableDictionaryRef          return_val = NULL;
    CFStringRef                     *active_profile_dict_keys = NULL;
    CFNumberRef                     *active_profile_dict_values = NULL;
    IOReturn                        ret;
    int                             ps_count;
    int                             i;

    system_profiles = IOPMCopyPowerProfiles();    
    if(!system_profiles) {
        return NULL;
    }
    
    /* Depending on removeUnsupportedSettings argument, 
     * the returned custom_settings dictionary may have
     * removed the settings lacking hardware support
     * on this machine
     */
    ret = readAllPMPlistSettings( removeUnsupportedSettings, 
                                    &custom_settings, 
                                    &active_profiles, NULL );
    if( kIOReturnSuccess != ret ) 
    {
        goto exit;
    }

    if(!active_profiles || !system_profiles || !custom_settings) goto exit;

    return_val = CFDictionaryCreateMutable(0, 0, 
            &kCFTypeDictionaryKeyCallBacks, 
            &kCFTypeDictionaryValueCallBacks);

    ps_count = CFDictionaryGetCount(active_profiles);
    active_profile_dict_keys = 
                    (CFStringRef *)malloc(ps_count*sizeof(CFStringRef));
    active_profile_dict_values = 
                    (CFNumberRef *)malloc(ps_count*sizeof(CFNumberRef));
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
            settings_per_power_source = CFDictionaryGetValue(
                                custom_settings, active_profile_dict_keys[i]);
        } else {
            // user has selected a system defined profile for this source
            if( (profile_index < 0) || (profile_index > 4) ) continue;
            sys_profile = isA_CFDictionary(
                CFArrayGetValueAtIndex(system_profiles, profile_index));
            if(!sys_profile) continue;
            settings_per_power_source = CFDictionaryGetValue(sys_profile, 
                                            active_profile_dict_keys[i]);
        }
        if(!settings_per_power_source) {
            continue;
        }
        tmp_settings = CFDictionaryCreateCopy(kCFAllocatorDefault, 
                                                settings_per_power_source);
        if(!tmp_settings) continue;
        CFDictionarySetValue(return_val, active_profile_dict_keys[i], 
                                         tmp_settings);
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

/**************************************************/


// A duplicate function for IOPMSetPMPreferences
// Provided as "IOPMSetCustomPMPreferences" for clarity
IOReturn IOPMSetCustomPMPreferences(CFDictionaryRef ESPrefs)
{
    return IOPMSetPMPreferences(ESPrefs);
}
/**************************************************/

IOReturn IOPMRevertPMPreferences(CFArrayRef keys_arr)
{
    IOReturn                            ret = kIOReturnInternalError;
    int                                 keys_i = 0;
    int                                 keys_count = 0;
    CFStringRef                         revert_this_setting = NULL;
    bool                                these_are_defaults = false;
    int                                 csi = 0;
    CFMutableDictionaryRef              customSettings = NULL;
    int                                 customSettingsCount = 0;
    CFDictionaryRef                    *customSettingsDicts = NULL;
    CFStringRef                        *customSettingsKeys = NULL;
    
    readAllPMPlistSettings(kIOPMRemoveUnsupportedPreferences, &customSettings, NULL, &these_are_defaults);
    if (these_are_defaults) {
        /* If the system has default settings, then there's nothing
         * for us to revert to default. Good bye.
         */
        ret = kIOReturnInvalid;
        goto exit_and_free;
    }

    if (!keys_arr || (0 == (keys_count = CFArrayGetCount(keys_arr)))) {
        /* Revert to OS Defaults
         * PM tracks your current settings as indices into a now-defunct array of settings, called profiles.
         * We "revert to defaults" by selecting profiles AC=2, Batt=1, UPS=1
         * Note that "custom settings" are AC=-1, Batt=-1, UPS=-1
         */
        
        CFDictionaryRef twooneoneprofiles = NULL;
        
        CFStringRef     tooKeys[kPowerSourcesCount] = { CFSTR(kIOPMACPowerKey), CFSTR(kIOPMBatteryPowerKey), CFSTR(kIOPMUPSPowerKey) };
        CFNumberRef     tooVals[kPowerSourcesCount];
        int             setVal = 0;
        
        setVal = 2;
        tooVals[0] = CFNumberCreate(0, kCFNumberIntType, &setVal);
        setVal = 1;
        tooVals[1] = CFNumberCreate(0, kCFNumberIntType, &setVal);
        tooVals[2] = CFNumberCreate(0, kCFNumberIntType, &setVal);
        
        twooneoneprofiles = CFDictionaryCreate(0, (const void **)tooKeys, (const void **)tooVals, kPowerSourcesCount,
                                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        if (twooneoneprofiles) {
            IOPMSetActivePowerProfiles(twooneoneprofiles);
            CFRelease(twooneoneprofiles);
        }
        
        CFRelease(tooVals[0]);
        CFRelease(tooVals[1]);
        CFRelease(tooVals[2]);
        
        ret = kIOReturnSuccess;
        goto exit_and_free;
    }
            
    customSettingsCount = CFDictionaryGetCount(customSettings);
    customSettingsKeys = (CFStringRef *)malloc(customSettingsCount * sizeof(CFStringRef));
    customSettingsDicts = (CFDictionaryRef *)malloc(customSettingsCount * sizeof(CFDictionaryRef));
    CFDictionaryGetKeysAndValues(customSettings, (const void **)customSettingsKeys, (const void **)customSettingsDicts);
        
    for (csi=0; csi<customSettingsCount; csi++) {
        CFMutableDictionaryRef      mutablePerPowerSourceCopy = NULL;
        mutablePerPowerSourceCopy = CFDictionaryCreateMutableCopy(0, 0, customSettingsDicts[csi]);
        if (!mutablePerPowerSourceCopy) {
            continue;
        }
        
        // Remove each key in the caller's array from the PM Prefs dictionary
        for (keys_i = 0; keys_i < keys_count; keys_i++) {
            revert_this_setting = CFArrayGetValueAtIndex(keys_arr, keys_i);
            if (isA_CFString(revert_this_setting)) {                
                CFDictionaryRemoveValue(mutablePerPowerSourceCopy, revert_this_setting);
            }
        }
        
        CFDictionarySetValue(customSettings, customSettingsKeys[csi], mutablePerPowerSourceCopy);
        CFRelease(mutablePerPowerSourceCopy);
    }
        
    ret = IOPMSetPMPreferences(customSettings);
    
exit_and_free:
    if (customSettings) {
        CFRelease(customSettings);
    }
    if (customSettingsKeys) {
        free(customSettingsKeys);
    }
    if (customSettingsDicts) {
        free(customSettingsDicts);
    }
    return ret;
}

/**************************************************/

static IOReturn _doTouchEnergyPrefs(SCDynamicStoreRef ds)
{
    CFStringRef                 energyPrefsKey = NULL;
    IOReturn                    ret;
    
    energyPrefsKey = SCDynamicStoreKeyCreatePreferences(NULL, kIOPMPrefsPath, kSCPreferencesKeyCommit);

    if (!energyPrefsKey) {
        return kIOReturnInternalError;
    }

    // Trigger a notification that the prefs have changed
    
    if (!SCDynamicStoreNotifyValue(ds, energyPrefsKey)) 
    {
        if(kSCStatusAccessError == SCError()) {
            ret = kIOReturnNotPrivileged;
        } else {
            ret = kIOReturnError;
        }
    } else {
        ret = kIOReturnSuccess;
    }    

    if (energyPrefsKey) {
        CFRelease(energyPrefsKey);
    }
    
    return ret;
}

// Sets (and activates) Custom power profile
IOReturn IOPMSetPMPreferences(CFDictionaryRef ESPrefs)
{
    IOReturn                    ret = kIOReturnError;
    SCPreferencesRef            energyPrefs = NULL;

    if(NULL == ESPrefs)
    {
        SCDynamicStoreRef ds = NULL;
        ds = SCDynamicStoreCreate(0, CFSTR("IOKit User Library - Touch"), NULL, NULL);
        if(!ds) {
            ret = kIOReturnInternalError;
        } else {
            ret = _doTouchEnergyPrefs(ds);
            CFRelease(ds);
        }
        return ret;
    }
        
    energyPrefs = SCPreferencesCreate(0, kIOPMAppName, kIOPMPrefsPath);
    if(!energyPrefs) {
        return kIOReturnError;
    }
    
    if(!SCPreferencesLock(energyPrefs, true))
    {  
        // handle error
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }

    if(!SCPreferencesSetValue(energyPrefs, CFSTR("Custom Profile"), ESPrefs))
    {
        ret = kIOReturnError;
        goto exit;
    }

    // If older profiles exist, remove them in favor of the new format
    SCPreferencesRemoveValue(energyPrefs, CFSTR(kIOPMACPowerKey));
    SCPreferencesRemoveValue(energyPrefs, CFSTR(kIOPMBatteryPowerKey));
    SCPreferencesRemoveValue(energyPrefs, CFSTR(kIOPMUPSPowerKey));

    if(!SCPreferencesCommitChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    ret = kIOReturnSuccess;
exit:
    if(energyPrefs) {
        SCPreferencesUnlock(energyPrefs);
        CFRelease(energyPrefs);
    }
    return ret;
}

/**************************************************/

/*
 * IOPMFeatureIsAvailable
 (
 * @param PMFeature - Name of a PM feature (like "WakeOnRing" or "Reduce Processor Speed")
 * @param power_source - The current power source (like "AC Power" or "Battery Power")
 * @result true if the given PM feature is supported on the given power source,
 *      false if the feature is unsupported.
 */
bool IOPMFeatureIsAvailable(CFStringRef PMFeature, CFStringRef power_source)
{
    CFDictionaryRef             supportedFeatures = NULL;
    io_registry_entry_t         registry_entry = MACH_PORT_NULL;
    int                         return_this_value = 0;

    if(!(registry_entry = getPMRootDomainRef())) 
        return false;

    supportedFeatures = IORegistryEntryCreateCFProperty(registry_entry, CFSTR("Supported Features"),
                                                        kCFAllocatorDefault, kNilOptions);

    if (!supportedFeatures)
        return false;

    if( CFEqual(PMFeature, CFSTR(kIOPMDarkWakeBackgroundTaskKey)) )
    {

#if TARGET_OS_IPHONE
        goto exit;
#else
        return_this_value = 0;

        if (!power_source)
            power_source = CFSTR(kIOPMACPowerKey);

        // On new machines (late 2012 and beyond), IOPPF publishes PowerNap
        // support using the kIOPMPowerNapSupportedKey
        CFTypeRef pnDetails = CFDictionaryGetValue( supportedFeatures,
                                CFSTR(kIOPMPowerNapSupportedKey));
        if(featureSupportsPowerSource(pnDetails, power_source))
        {
            return_this_value = 1;
            goto exit;
        }


        CFTypeRef btdetails = CFDictionaryGetValue( supportedFeatures,
                                CFSTR(kIOPMDarkWakeBackgroundTaskKey));
        CFTypeRef ssdetails = CFDictionaryGetValue( supportedFeatures,
                                CFSTR(kIOPMSleepServicesKey));
        if(featureSupportsPowerSource(btdetails, power_source) ||
           featureSupportsPowerSource(ssdetails, power_source))
        {
            return_this_value = 1;
        }
#endif
    }
    else
    {
       return_this_value = IOPMFeatureIsAvailableWithSupportedTable(
                               PMFeature,
                               power_source,
                               supportedFeatures);
    }

exit:
    CFRelease(supportedFeatures);

    return (return_this_value ? true:false);
}


bool IOPMFeatureIsAvailableWithSupportedTable(
     CFStringRef                PMFeature,
     CFStringRef                power_source,
     CFDictionaryRef            supportedFeatures)
{
    CFStringRef                 supportedString = NULL;
    CFTypeRef                   featureDetails = NULL;
    bool                        ret = false;
    
    if (!power_source)
        power_source = CFSTR(kIOPMACPowerKey);
    
    if (!supportedFeatures)
        return false;
    
    /* Basic sleep timer settings are always available *
     * TTY connection ability to prevent sleep is always available */
    if (CFEqual(PMFeature, CFSTR(kIOPMDisplaySleepKey))
        || CFEqual(PMFeature, CFSTR(kIOPMSystemSleepKey))
        || CFEqual(PMFeature, CFSTR(kIOPMDiskSleepKey))
        || CFEqual(PMFeature, CFSTR(kIOPMTTYSPreventSleepKey)))
    {    
        ret = true;
        goto exit;
    }

    /* deprecated - kIOPMRestartOnKernelPanicKey
     * 11195840 Remove "restart automatically if computer freezes" check-box
     */     
    if (CFEqual(PMFeature, CFSTR(kIOPMRestartOnKernelPanicKey))) { 
        ret = false;
        goto exit;
    }
    
    // *********************************
    // Special case for PowerButtonSleep    
    
    if(CFEqual(PMFeature, CFSTR(kIOPMSleepOnPowerButtonKey)))
    {
#if TARGET_OS_IPHONE
        ret = false;
#else
        CFArrayRef  tmp_array = NULL;
        // Pressing the power button only causes sleep on desktop PowerMacs, 
        // cubes, and iMacs.
        // Therefore this feature is not supported on portables.
        // We'll use the presence of a battery (or the capability for a battery) 
        // as evidence whether this is a portable or not.
        IOReturn r = IOPMCopyBatteryInfo(kIOMasterPortDefault, &tmp_array);
        if((r == kIOReturnSuccess) && tmp_array) 
        {
            CFRelease(tmp_array);
            ret = false;
        } else ret = true;
#endif
        goto exit;
    }
    
    // *********************************
    // Special case for ReduceBrightness
    
    if ( CFEqual(PMFeature, CFSTR(kIOPMReduceBrightnessKey)) )
    {
        // ReduceBrightness feature is only supported on laptops
        // and on desktops with UPS with brightness-adjustable LCD displays.
        // These machines report a "DisplayDims" property in the 
        // supportedFeatures dictionary.
        // ReduceBrightness is never supported on AC Power.
        CFTypeRef ps = IOPSCopyPowerSourcesInfo();
        if ( ps 
            && ( IOPSGetActiveBattery(ps) || IOPSGetActiveUPS(ps) ) 
            && supportedFeatures
            && CFDictionaryGetValue(supportedFeatures, CFSTR("DisplayDims"))
            && !CFEqual(power_source, CFSTR(kIOPMACPowerKey)) )
        {
            ret = true;
        } else {
            ret = false;
        }
        
        if (ps) 
            CFRelease(ps);
        goto exit;
    }

    // ***********************************
    // Generic code for all other settings    
    
    supportedString = supportedNameForPMName( PMFeature );
    if(!supportedString) {
        ret = false;
        goto exit;
    }
    
    featureDetails = CFDictionaryGetValue(supportedFeatures, supportedString);
    if(!featureDetails) {
        ret = false;
        goto exit;
    }
    
    if(featureSupportsPowerSource(featureDetails, power_source))
    {
        ret = true;    
    }
    
    
exit:
    return ret;
}

/**************************************************/

// How we determine what ActivePowerProfiles to return:
// (1) If the user has specified any profiles in the
//     PM prefs file, we'll return those.
// (2) If the user hasn't explicitly specified any
//     profiles, we'll look to their Custom settings:
// (3) If, in the past, a user has specified any PM settings at
//     all (say from before an upgrade install), we'll return
//     (-1, -1, -1) and respect those settings as custom settings.
// (4) If there are no specified profiles and no previous specified
//     custom settings, then we'll return (2, 1, 1) as specified
//     in PowerManagement.bundle's com.apple.SystemPowerProfileDefaults.plist
//
// Also, note that in the case of (1), we'll re-populate any settings not
// specified for a particular power source (UPS's are easy to plug/unplug)
// with the defaults obtained as in (4).
//
// For all steps above, we'll strip any irrelevant settings before
// returning the dictionary (i.e. battery or UPS settings will not be 
// returned where unsupported)

CFDictionaryRef     IOPMCopyActivePowerProfiles(void)
{
    IOReturn                ret;
    CFDictionaryRef         activeProfiles;

    ret = readAllPMPlistSettings(
                        kIOPMRemoveUnsupportedPreferences, 
                        NULL, 
                        &activeProfiles, NULL);

    if(kIOReturnSuccess == ret) {
        return activeProfiles;
    } else {
        return NULL;
    }
}

/**************************************************/

IOReturn IOPMSetActivePowerProfiles(CFDictionaryRef which_profile)
{
    CFDataRef           profiles_data;
    vm_address_t        profiles_buffer;
    IOByteCount         buffer_len;
    kern_return_t       kern_result;
    IOReturn            return_val = kIOReturnError;
    mach_port_t         server_port = MACH_PORT_NULL;

    if(!_isActiveProfileDictValid(which_profile)) {
        return kIOReturnBadArgument;
    }
    
    // open reference to PM configd
    kern_result = bootstrap_look_up2(bootstrap_port, 
                                     kIOPMServerBootstrapName, 
                                     &server_port, 
                                     0, 
                                     BOOTSTRAP_PRIVILEGED_SERVER);    
    if(KERN_SUCCESS != kern_result) {
        return kIOReturnError;
    }

    profiles_data = IOCFSerialize(which_profile, 0);
    profiles_buffer = (vm_address_t) CFDataGetBytePtr(profiles_data);
    buffer_len = CFDataGetLength(profiles_data);

    // toss dictionary over the wall to conigd via mig-generated interface
    // configd will perform a permissions check. If the caller is root,
    // admin, or console, configd will write the prefs file from its
    // root context.
    kern_result = io_pm_set_active_profile(server_port, 
                            profiles_buffer, buffer_len, 
                            &return_val);

    mach_port_deallocate(mach_task_self(), server_port);
    CFRelease(profiles_data);

    if(KERN_SUCCESS == kern_result) {
        return return_val;
    } else {
        return kIOReturnInternalError;
    }
}

/**************************************************/

/***
 Returns a CFRunLoopSourceRef that notifies the caller when power source
 information changes.
 Arguments:
    IOPowerSourceCallbackType callback - A function to be called whenever 
        ES prefs file on disk changes
    void *context - Any user-defined pointer, passed to the callback.
 Returns NULL if there were any problems.
 Caller must CFRelease() the returned value.
***/
CFRunLoopSourceRef IOPMPrefsNotificationCreateRunLoopSource(
    IOPMPrefsCallbackType callback, 
    void *context) 
{
    SCDynamicStoreRef           store = NULL;
    CFStringRef                 EnergyPrefsKey = NULL;
    CFRunLoopSourceRef          SCDrls = NULL;
    user_callback_context       *ioContext = NULL;
    SCDynamicStoreContext       scContext = {0, NULL, CFRetain, CFRelease, NULL};

    if(!callback) return NULL;

    scContext.info = CFDataCreateMutable(NULL, sizeof(user_callback_context));
    CFDataSetLength(scContext.info, sizeof(user_callback_context));
    ioContext = (user_callback_context *)CFDataGetBytePtr(scContext.info); 
    ioContext->context = context;
    ioContext->callback = callback;
        
    // Open connection to SCDynamicStore. User's callback as context.
    store = SCDynamicStoreCreate(kCFAllocatorDefault, 
                CFSTR("IOKit Preferences Copy"), ioCallout, (void *)&scContext);
    if(!store) return NULL;
     
    // Setup notification for changes in Energy Saver prefences
    EnergyPrefsKey = SCDynamicStoreKeyCreatePreferences(
                    NULL, 
                    kIOPMPrefsPath, 
                    kSCPreferencesKeyApply);
    if(EnergyPrefsKey) {
        SCDynamicStoreAddWatchedKey(store, EnergyPrefsKey, FALSE);
        CFRelease(EnergyPrefsKey);
    }

    // Obtain the CFRunLoopSourceRef from this SCDynamicStoreRef session
    SCDrls = SCDynamicStoreCreateRunLoopSource(kCFAllocatorDefault, store, 0);
    CFRelease(store);

    return SCDrls;
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
// Internals

static void mergeUserDefaultOverriddenSettings(CFMutableDictionaryRef es)
{
    CFMutableDictionaryRef  batt        = NULL;
    CFMutableDictionaryRef  ac          = NULL;
    CFMutableDictionaryRef  ups         = NULL;
    CFDictionaryRef         acOver      = NULL;
    CFDictionaryRef         battOver    = NULL;
    CFDictionaryRef         upsOver     = NULL;
    SCDynamicStoreRef       dynamic_store = NULL;
    CFDictionaryRef         userOverrides = NULL;
    
    /*
     * There might be a CFDictionary in the SCDynamicStore at
     *     "State:/IOKit/PowerManagement/UserOverrides"
     *      == kIOPMDynamicStoreUserOverridesKey
     * If there is, then we will prefer to use its settings
     * as default settings, rather than our hard-coded defaults,
     * for unspecified setting values.
     */    
    dynamic_store = SCDynamicStoreCreate(0, CFSTR("IOKit User Library - PM defaults"), NULL, NULL);
    
    if (dynamic_store) 
    {
        userOverrides = SCDynamicStoreCopyValue(dynamic_store, CFSTR(kIOPMDynamicStoreUserOverridesKey));
        CFRelease(dynamic_store);
    }
    
    if (NULL == userOverrides)
        return;
    
    if (!isA_CFDictionary(userOverrides)) {
        CFRelease(userOverrides);
        return;
    }
    
    acOver     = CFDictionaryGetValue(userOverrides, CFSTR(kIOPMACPowerKey));
    battOver   = CFDictionaryGetValue(userOverrides, CFSTR(kIOPMBatteryPowerKey));
    upsOver    = CFDictionaryGetValue(userOverrides, CFSTR(kIOPMUPSPowerKey));
    
    // Overrides either contains 3 dictionaries of PM Settings keyed as
    // AC, Battery, and UPS Power, or it is itself a dictionary of PM Settings.
    if(acOver && battOver && upsOver) 
    {
        // Good. All 3 power source settings types are represented.
        // Do nothing here.
    } else if(!acOver && !battOver && !upsOver) 
    {
        // The dictionary didn't specify any per-power source overrides, which
        // means that it's a flat dictionary strictly of PM settings.
        // We duplicate it 3 ways, as each overridden setting in this dictionary
        // will be applied to each power source's settings.
        acOver = battOver = upsOver = userOverrides;
    } else {
        return;
    }
    
    ac      = (CFMutableDictionaryRef)CFDictionaryGetValue(es, CFSTR(kIOPMACPowerKey));
    batt    = (CFMutableDictionaryRef)CFDictionaryGetValue(es, CFSTR(kIOPMBatteryPowerKey));
    ups     = (CFMutableDictionaryRef)CFDictionaryGetValue(es, CFSTR(kIOPMUPSPowerKey));
    
    if (ac) {
        mergeDictIntoMutable(ac,    acOver,     kKeepOriginalValues);
    }
    
    if (batt) {
        mergeDictIntoMutable(batt,  battOver,   kKeepOriginalValues);
    }

    if (ups) {
        mergeDictIntoMutable(ups,   upsOver,    kKeepOriginalValues);
    }
        
    CFRelease (userOverrides);
    
    return;
}

static int addDefaultEnergySettings(CFMutableDictionaryRef sys)
{
    CFMutableDictionaryRef  batt = NULL;
    CFMutableDictionaryRef  ac = NULL;
    CFMutableDictionaryRef  ups = NULL;
    int             i;
    CFNumberRef     val;
    CFStringRef     key;


    batt    = (CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMBatteryPowerKey));
    ac      = (CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMACPowerKey));
    ups     = (CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMUPSPowerKey));

    /*
     * Note that in the following "poplulation" loops, we're using 
     * CFDictionaryAddValue rather than CFDictionarySetValue. If a value is 
     * already present AddValue will not replace it.
     */
    
    /* 
     * Populate default battery dictionary 
     */
    
    if (batt) {
        for (i=0; i<kPMSettingsCount; i++)
        {
            key = CFStringCreateWithCString(0, defaultSettings[i].keyName, kCFStringEncodingMacRoman);
            val = CFNumberCreate(0, kCFNumberSInt32Type, &defaultSettings[i].defaultValueBattery);

            CFDictionaryAddValue(batt, key, val);
            
            CFRelease(key);
            CFRelease(val);
        }
        CFDictionaryAddValue(batt, CFSTR(kIOHibernateFileKey), CFSTR(kIOHibernateDefaultFile));
    }

    /* 
     * Populate default AC dictionary 
     */
    if (ac)
    {
        for (i=0; i<kPMSettingsCount; i++)
        {
            key = CFStringCreateWithCString(0, defaultSettings[i].keyName, kCFStringEncodingMacRoman);
            val = CFNumberCreate(0, kCFNumberSInt32Type, &defaultSettings[i].defaultValueAC);
            
            CFDictionaryAddValue(ac, key, val);
            
            CFRelease(key);
            CFRelease(val);
        }

        CFDictionaryAddValue(ac, CFSTR(kIOHibernateFileKey), CFSTR(kIOHibernateDefaultFile));
    }
    
    /* 
     * Populate default UPS dictionary 
     */
    if (ups) {
        for (i=0; i<kPMSettingsCount; i++)
        {
            key = CFStringCreateWithCString(0, defaultSettings[i].keyName, kCFStringEncodingMacRoman);
            val = CFNumberCreate(0, kCFNumberSInt32Type, &defaultSettings[i].defaultValueAC);
            
            CFDictionaryAddValue(ups, key, val);
            
            CFRelease(key);
            CFRelease(val);
        }

        CFDictionaryAddValue(ups, CFSTR(kIOHibernateFileKey), CFSTR(kIOHibernateDefaultFile));
    }
    
    return 0;
}

static void addSystemProfileEnergySettings(
    CFDictionaryRef user_profile_selections,
    CFMutableDictionaryRef passed_in)
{
    CFArrayRef                  system_profiles = NULL;
    CFDictionaryRef             profile_at_idx = NULL;
    CFDictionaryRef             finally_actual_settings = NULL;    

    CFNumberRef                 this_profile_choice = NULL;
    CFNumberRef                 default_profile_choice = NULL;
    CFDictionaryRef             default_profile_selections = NULL;
    int                         int_profile_choice = 0;
    
    CFMutableDictionaryRef      ps_passed_in = NULL;
    CFStringRef                 this_power_source = NULL;
    int                         i;

    // Does the user currently have any profiles selected?
    // If not, IOPMCopyActivePowerProfiles returns a (2,1,1) default
    if(!user_profile_selections) return;

    default_profile_selections = _createDefaultProfileSelections();
    if(!isA_CFDictionary(default_profile_selections))
        return;

    // Copy the full CFArray of system profiles
    system_profiles = IOPMCopyPowerProfiles();
    if(!system_profiles) goto exit_cleanup;
    
    // For each power source,
    for(i = 0; i<kPowerSourcesCount; i++)
    {
        this_power_source = getPowerSourceString(i);
        if(!this_power_source) continue;
        
        // Grab the profile number from user_profile_selections
        this_profile_choice = isA_CFNumber( CFDictionaryGetValue( 
                                user_profile_selections, this_power_source));
        if(this_profile_choice) {
            CFNumberGetValue( this_profile_choice, 
                                kCFNumberIntType, &int_profile_choice);
        }
        
        /* -1 == Custom settings in use?
           Then we need to merge in the default settings [from profiles 2,1,1]
           for this power source.
         */
        if( -1 == int_profile_choice ) {
            default_profile_choice = isA_CFNumber( CFDictionaryGetValue(
                                default_profile_selections, this_power_source));
            if(default_profile_choice) {
                CFNumberGetValue( default_profile_choice, 
                                    kCFNumberIntType, &int_profile_choice);
            }
        }
        
        // And if the integer profile index is outside the acceptable range,
        // we give it a sane value
        if ( (int_profile_choice < 0) 
            || (int_profile_choice > kPowerProfilesCount) ) 
        {
            int_profile_choice = 2;
        }
        
        // Index into systemProfiles at profileNumber
        profile_at_idx = CFArrayGetValueAtIndex( system_profiles, 
                                                 int_profile_choice);
        if(!profile_at_idx) continue;
        
        // From the profile's CFDictionary, extract the final actual
        // settings CFDictionary for "this profile number" and for
        // "this power source"
        finally_actual_settings = CFDictionaryGetValue(
                                    profile_at_idx, this_power_source);

        // Gently merge this settings CFDictionary into the passed-in
        // settings dictionary's appropiate power-source specific dictionary
        
        ps_passed_in = (CFMutableDictionaryRef)
                            CFDictionaryGetValue(passed_in, this_power_source);
        
        if(!finally_actual_settings || !ps_passed_in) continue;

        mergeDictIntoMutable( ps_passed_in, 
                              finally_actual_settings,
                              kKeepOriginalValues);
    }

exit_cleanup:
//    if(user_profile_selections) CFRelease(user_profile_selections);
    if(default_profile_selections) CFRelease(default_profile_selections);
    if(system_profiles) CFRelease(system_profiles);

    return;
}


/* getPMRootDomainRef
 *
 * Caller should not release the returned io_registry_entry_t
 */
__private_extern__ io_registry_entry_t  getPMRootDomainRef(void)
{
    static io_registry_entry_t cached_root_domain = MACH_PORT_NULL;
    
    if( MACH_PORT_NULL == cached_root_domain ) {
        cached_root_domain = IORegistryEntryFromPath( kIOMasterPortDefault, 
                        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    }
    return cached_root_domain;
}


/* Maps a PowerManagement string constant
 *   -> to its corresponding Supported Feature in IOPMrootDomain
 */
static CFStringRef
supportedNameForPMName( CFStringRef pm_name )
{
#if TARGET_OS_IPHONE
    if( CFEqual(pm_name, CFSTR(kIOPMReduceBrightnessKey))
        || CFEqual(pm_name, CFSTR(kIOPMDisplaySleepUsesDimKey)) )
#else
    if(CFEqual(pm_name, CFSTR(kIOPMDisplaySleepUsesDimKey)))
#endif /* TARGET_OS_IPHONE */
    {
        return CFSTR("DisplayDims");
    }

    if(CFEqual(pm_name, CFSTR(kIOPMWakeOnLANKey))
       || CFEqual(pm_name, CFSTR(kIOPMPrioritizeNetworkReachabilityOverSleepKey)))
    {
        return CFSTR("WakeOnMagicPacket");
    }

    if(CFEqual(pm_name, CFSTR(kIOPMMobileMotionModuleKey)))
    {
        return CFSTR("MobileMotionModule");
    }

    if( CFEqual(pm_name, CFSTR(kIOHibernateModeKey))
        || CFEqual(pm_name, CFSTR(kIOHibernateFreeRatioKey))
        || CFEqual(pm_name, CFSTR(kIOHibernateFreeTimeKey))
        || CFEqual(pm_name, CFSTR(kIOHibernateFileKey))
        || CFEqual(pm_name, CFSTR(kIOHibernateFeatureKey)))
    {
        return CFSTR(kIOHibernateFeatureKey);
    }
    
    if (CFEqual(pm_name, CFSTR(kIOPMDeepSleepEnabledKey))
        || CFEqual(pm_name, CFSTR(kIOPMDeepSleepDelayKey)))
    {
        return CFSTR("DeepSleep");
    }

    if (CFEqual(pm_name, CFSTR(kIOPMAutoPowerOffEnabledKey))
        || CFEqual(pm_name, CFSTR(kIOPMAutoPowerOffDelayKey)))
    {
        return CFSTR("AutoPowerOff");
    }

    return pm_name;
}

// Helper for IOPMFeatureIsAvailable
static bool
featureSupportsPowerSource(CFTypeRef featureDetails, CFStringRef power_source)
{
    CFNumberRef         featureNum   = NULL;
    CFNumberRef         tempNum      = NULL;
    CFArrayRef          featureArr   = NULL;
    uint32_t            ps_support   = 0;
    uint32_t            tmp;
    unsigned int        i;
        
    if( (featureNum = isA_CFNumber(featureDetails)) )
    {
        CFNumberGetValue(featureNum, kCFNumberSInt32Type, &ps_support);
    } else if( (featureArr = isA_CFArray(featureDetails)) )
    {
        // If several entitites are asserting a given feature, we OR
        // together their supported power sources.

        unsigned int arrayCount = CFArrayGetCount(featureArr);
        for(i = 0; i<arrayCount; i++)
        {
            tempNum = isA_CFNumber(CFArrayGetValueAtIndex(featureArr, i));
            if(tempNum) {
                CFNumberGetValue(tempNum, kCFNumberSInt32Type, &tmp);
                ps_support |= tmp;
            }
        }
    }

    if(!power_source) {

        // Lack of a defined power source just gets a "true" return
        // if the setting is supported on ANY power source.

        return (ps_support ? true : false);
    }

    if(CFEqual(CFSTR(kIOPMACPowerKey), power_source) )
    {
        return (ps_support & kIOPMSupportedOnAC) ? true : false;
    } else if(CFEqual(CFSTR(kIOPMBatteryPowerKey), power_source) )
    {
        return (ps_support & kIOPMSupportedOnBatt) ? true : false;
    } else if(CFEqual(CFSTR(kIOPMUPSPowerKey), power_source) )
    {
        return (ps_support & kIOPMSupportedOnUPS) ? true : false;
    } else {
        // unexpected power source argument
        return false;
    }

}


/***
 * removeIrrelevantPMProperties
 *
 * Prunes unsupported properties from the energy dictionary.
 * e.g. If your machine doesn't have a modem, this removes the Wake On Ring property.
 ***/
static void IOPMRemoveIrrelevantProperties(CFMutableDictionaryRef energyPrefs)
{
    int                         profile_count = 0;
    int                         dict_count    = 0;
    CFStringRef                 *profile_keys = NULL;
    CFDictionaryRef             *profile_vals = NULL;
    CFStringRef                 *dict_keys    = NULL;
    CFDictionaryRef             *dict_vals    = NULL;
    CFMutableDictionaryRef      this_profile  = NULL;
    CFTypeRef                   ps_snapshot      = NULL;
    CFDictionaryRef                _supportedCached = NULL;
    io_registry_entry_t         _rootDomain   = IO_OBJECT_NULL;
    
    ps_snapshot = IOPSCopyPowerSourcesInfo();
    
    // Grab a copy of RootDomain's supported energy saver settings
    _rootDomain = getPMRootDomainRef();
    if (IO_OBJECT_NULL != _rootDomain) 
    {
        _supportedCached = IORegistryEntryCreateCFProperty(_rootDomain, 
                                                           CFSTR("Supported Features"), 
                                                           kCFAllocatorDefault, kNilOptions);
    }
    
    /*
     * Remove features when not supported - 
     *      Wake On Administrative Access, Dynamic Speed Step, etc.
     */
    profile_count = CFDictionaryGetCount(energyPrefs);
    profile_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * profile_count);
    profile_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * profile_count);
    if (!profile_keys || !profile_vals) 
        goto exit;
    
    CFDictionaryGetKeysAndValues(energyPrefs, (const void **)profile_keys, 
                                 (const void **)profile_vals);
    // For each CFDictionary at the top level (battery, AC)
    while(--profile_count >= 0)
    {
        if(kCFBooleanTrue != IOPSPowerSourceSupported(ps_snapshot, profile_keys[profile_count]))
        {
            // Remove dictionary if the whole power source isn't supported on this machine.
            CFDictionaryRemoveValue(energyPrefs, profile_keys[profile_count]);        
        } else {
            
            // Make a mutable copy of the prefs dictionary
            
            this_profile = (CFMutableDictionaryRef)isA_CFDictionary(
                                                                    CFDictionaryGetValue(energyPrefs, profile_keys[profile_count]));
            if(!this_profile) 
                continue;
            
            this_profile = CFDictionaryCreateMutableCopy(NULL, 0, this_profile);
            if(!this_profile) 
                continue;
            
            CFDictionarySetValue(energyPrefs, profile_keys[profile_count], this_profile);
            CFRelease(this_profile);
            
            // And prune unneeded settings from our new mutable property            
            
            dict_count = CFDictionaryGetCount(this_profile);
            dict_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * dict_count);
            dict_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * dict_count);
            if (!dict_keys || !dict_vals) 
                continue;
            CFDictionaryGetKeysAndValues(this_profile, 
                                         (const void **)dict_keys, (const void **)dict_vals);
            // For each specific property within each dictionary
            while(--dict_count >= 0)
            {
                if( CFEqual((CFStringRef)dict_keys[dict_count], CFSTR(kIOPMDarkWakeBackgroundTaskKey)) ) 
                {
#if !TARGET_OS_IPHONE
                    // We conditionalize PowerNap support on kIOPMPowerNapSupportedKey for all
                    // machines late 2012 and beyond. The presence of this key is a sufficient
                    // condition to support PowerNap
                    bool supportsNewPNKey = false;
                    if(IOPMFeatureIsAvailableWithSupportedTable( CFSTR(kIOPMPowerNapSupportedKey),
                           (CFStringRef)profile_keys[profile_count], _supportedCached)) {
                            supportsNewPNKey = true;
                    }

                    // For legacy machines, we look for either the kIOPMDarkWakeBackgroundTaskKey or the
                    // kIOPMSleepServicesKey
                    if ( ((!IOPMFeatureIsAvailableWithSupportedTable( CFSTR(kIOPMDarkWakeBackgroundTaskKey), 
                            (CFStringRef)profile_keys[profile_count], _supportedCached)  &&
                        !IOPMFeatureIsAvailableWithSupportedTable( CFSTR(kIOPMSleepServicesKey), 
                            (CFStringRef)profile_keys[profile_count], _supportedCached)) 
                         && !supportsNewPNKey
                      ) 
                   {
                       CFDictionaryRemoveValue(this_profile, (CFStringRef)dict_keys[dict_count]);
                   }
#endif /* !TARGET_OS_IPHONE */
                }
                else if( !IOPMFeatureIsAvailableWithSupportedTable((CFStringRef)dict_keys[dict_count], 
                                    (CFStringRef)profile_keys[profile_count], _supportedCached) )
                {
                    // If the property isn't supported, remove it
                    CFDictionaryRemoveValue(this_profile, 
                                            (CFStringRef)dict_keys[dict_count]);    
                }
            }
            free(dict_keys);
            free(dict_vals);
        }
    }
    
exit:
    if (profile_keys)
        free(profile_keys);
    if (profile_vals)
        free(profile_vals);
    if (ps_snapshot) 
        CFRelease(ps_snapshot);
    if (_supportedCached)
        CFRelease(_supportedCached);
    return;
}

/***
 * getCheetahPumaEnergySettings
 *
 * Reads the old Energy Saver preferences file from /Library/Preferences/com.apple.PowerManagement.xml
 *
 ***/
static int getCheetahPumaEnergySettings(CFMutableDictionaryRef energyPrefs)
{
   SCPreferencesRef             CheetahPrefs = NULL;
   CFMutableDictionaryRef       s = NULL;
   CFNumberRef                  n;
   CFBooleanRef                 b;
   
   if(!energyPrefs) return 0;
   CheetahPrefs = SCPreferencesCreate (kCFAllocatorDefault, 
                CFSTR("I/O Kit PM Library"),
                CFSTR("/Library/Preferences/com.apple.PowerManagement.plist"));
    
    if(!CheetahPrefs) return 0;
    
    s = (CFMutableDictionaryRef)CFDictionaryGetValue( energyPrefs, 
                                            CFSTR(kIOPMBatteryPowerKey));
    if(!s)
    {
        CFRelease(CheetahPrefs);
        return 0;
    }
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDimKey);
    if(n) CFDictionaryAddValue(s, CFSTR(kIOPMDisplaySleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDiskKey);
    if(n) CFDictionaryAddValue(s, CFSTR(kIOPMDiskSleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahSleepKey);
    if(n) CFDictionaryAddValue(s, CFSTR(kIOPMSystemSleepKey), n);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahRestartOnPowerLossKey);
    if(b) CFDictionaryAddValue(s, CFSTR(kIOPMRestartOnPowerLossKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeForNetworkAccessKey);
    if(b) CFDictionaryAddValue(s, CFSTR(kIOPMWakeOnLANKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeOnRingKey);
    if(b) CFDictionaryAddValue(s, CFSTR(kIOPMWakeOnRingKey), b);
                    

    s = (CFMutableDictionaryRef)CFDictionaryGetValue(energyPrefs, CFSTR(kIOPMACPowerKey));
    if(!s)
    {
        CFRelease(CheetahPrefs);
        return 0;
    }
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDimKey);
    if(n) CFDictionaryAddValue(s, CFSTR(kIOPMDisplaySleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahDiskKey);
    if(n) CFDictionaryAddValue(s, CFSTR(kIOPMDiskSleepKey), n);
    n = (CFNumberRef)SCPreferencesGetValue(CheetahPrefs, kCheetahSleepKey);
    if(n) CFDictionaryAddValue(s, CFSTR(kIOPMSystemSleepKey), n);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahRestartOnPowerLossKey);
    if(b) CFDictionaryAddValue(s, CFSTR(kIOPMRestartOnPowerLossKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeForNetworkAccessKey);
    if(b) CFDictionaryAddValue(s, CFSTR(kIOPMWakeOnLANKey), b);
    b = (CFBooleanRef)SCPreferencesGetValue(CheetahPrefs, kCheetahWakeOnRingKey);
    if(b) CFDictionaryAddValue(s, CFSTR(kIOPMWakeOnRingKey), b);


    CFRelease(CheetahPrefs);

     return 1; // success
}


/**************************************************
*
* System Power Settings
*
**************************************************/

IOReturn IOPMActivateSystemPowerSettings( void )
{
    io_registry_entry_t         rootdomain = MACH_PORT_NULL;
    CFDictionaryRef             settings = NULL;
    IOReturn                    ret = kIOReturnSuccess;
    bool                        disable_sleep = false;

    settings = IOPMCopySystemPowerSettings();
    if(!settings) {
        goto exit;
    }

    // Disable Sleep?
    disable_sleep = (kCFBooleanTrue == 
                        CFDictionaryGetValue( settings, kIOPMSleepDisabledKey ));

    rootdomain = getPMRootDomainRef();
    ret = IORegistryEntrySetCFProperty( rootdomain, kIOPMSleepDisabledKey, 
                        (disable_sleep ? kCFBooleanTrue : kCFBooleanFalse));

#if !TARGET_OS_IPHONE
    bool    avoid_keyStore = false; 
    // Disable FDE Key Store on SMC
    avoid_keyStore = (kCFBooleanTrue == 
                        CFDictionaryGetValue( settings, CFSTR(kIOPMDestroyFVKeyOnStandbyKey) ));
    ret = IORegistryEntrySetCFProperty( rootdomain, CFSTR(kIOPMDestroyFVKeyOnStandbyKey), 
                        (avoid_keyStore ? kCFBooleanTrue : kCFBooleanFalse));

#endif

exit:
    if(settings) CFRelease( settings );
    return ret;
}

CFDictionaryRef IOPMCopySystemPowerSettings(void)
{
    CFDictionaryRef                         systemPowerDictionary = NULL;
    CFDictionaryRef                         tmp_dict = NULL;
    SCPreferencesRef                        energyPrefs = NULL;

    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, 
                                        kIOPMAppName, kIOPMPrefsPath );
    if(!energyPrefs) {
        return NULL;
    }
    
    tmp_dict = isA_CFDictionary(SCPreferencesGetValue( energyPrefs, 
                                                CFSTR("SystemPowerSettings")));

    if(!tmp_dict) {
        goto exit;
    }
    
    systemPowerDictionary = CFDictionaryCreateCopy(0, tmp_dict); 
    
exit:
    if(energyPrefs) CFRelease(energyPrefs);
    return systemPowerDictionary;
}

IOReturn IOPMSetSystemPowerSetting( CFStringRef key, CFTypeRef value)
{
    IOReturn                    ret = kIOReturnError;
    SCPreferencesRef            energyPrefs = NULL;
    
    CFMutableDictionaryRef      systemPowerDictionary = NULL;
    CFDictionaryRef             tmp_dict = NULL;
    
    energyPrefs = SCPreferencesCreate( 0, kIOPMAppName, kIOPMPrefsPath );
    if(!energyPrefs) 
    {
        if(kSCStatusAccessError == SCError()) 
           return kIOReturnNotPrivileged;
        else 
           return kIOReturnError;
    }

    /* We lock energyPrefs here to prevent multiple simultaneous writes
     * modifying the same data. Must lock energyPrefs before reading
     * systemPowerSettings in IOPMCopySystemPowerSettings.
     */    
    if(!SCPreferencesLock(energyPrefs, true))
    {  
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    tmp_dict = IOPMCopySystemPowerSettings();
    if(tmp_dict) {
        systemPowerDictionary = CFDictionaryCreateMutableCopy(0, 0, tmp_dict);
        CFRelease(tmp_dict);
        tmp_dict = NULL;
    } else {
        systemPowerDictionary = CFDictionaryCreateMutable(0, 0, 
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks);
    }
    
    if(!systemPowerDictionary) {
        goto exit;
    }
    
    CFDictionarySetValue( systemPowerDictionary, key, value);

    ret = kIOReturnSuccess;

    if(!SCPreferencesSetValue( energyPrefs, 
                                CFSTR("SystemPowerSettings"), 
                                systemPowerDictionary))
    {
        ret = kIOReturnError;
        goto exit;
    }

    if(!SCPreferencesCommitChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    ret = kIOReturnSuccess;
exit:
    if(energyPrefs) {
        SCPreferencesUnlock(energyPrefs);
        CFRelease(energyPrefs);
    }
    return ret;

}


void IOPMOverrideDefaultPMPreferences(CFDictionaryRef overrideSettings)
{
    SCDynamicStoreRef               dynamic_store = NULL;
    
    if(0 == isA_CFDictionary(overrideSettings)) {
        return;
    }
    
    dynamic_store = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("IOKit User Library - Override"),
                                         NULL, NULL);
    if(dynamic_store == NULL) 
        return;
    
    // This is the simplest possible implementation of IOPMOverrideDefaultPMPreferences.
    // This assumes only one caller. If called multiple times, the last caller will
    // overwrite all previous callers values.
    SCDynamicStoreSetValue(dynamic_store, CFSTR(kIOPMDynamicStoreUserOverridesKey), overrideSettings);
    
    // And send a notification to anyone watching the preferences file.
    _doTouchEnergyPrefs(dynamic_store);
    
    CFRelease(dynamic_store);
    return;
}




/**************************************************
*
* Power Profiles
*
*
**************************************************/

static void mergeDictIntoMutable(
    CFMutableDictionaryRef  target,
    CFDictionaryRef         overrides,
    bool                    overwrite)
{
    const CFStringRef         *keys;
    const CFTypeRef           *objs;
    int                 count;
    int                 i;
    
    count = CFDictionaryGetCount(overrides);
    if(0 == count) return;

    keys = (CFStringRef *)malloc(sizeof(CFStringRef) * count);
    objs = (CFTypeRef *)malloc(sizeof(CFTypeRef) * count);
    if(!keys || !objs) return;

    CFDictionaryGetKeysAndValues(overrides, 
                    (const void **)keys, (const void **)objs);
    for(i=0; i<count; i++)
    {
        if(overwrite) {
            CFDictionarySetValue(target, keys[i], objs[i]);    
        } else {
            // no value added if key already present
            CFDictionaryAddValue(target, keys[i], objs[i]);    
        }
    }
    free((void *)keys);
    free((void *)objs);    
}

/* _copySystemProvidedProfiles()
 *
 * The PlatformExpert kext on the system may conditionally override the Energy
 * Saver's profiles. Only the PlatformExpert should be setting these properties.
 *
 * We use two supported properties - kIOPMSystemDefaultOverrideKey & 
 * kIOPMSystemDefaultProfilesKey. We check first for 
 * kIOPMSystemDefaultOverrideKey (a partial settings defaults substitution), 
 * and if it's not present we'll use kIOPMSystemDefaultProfilesKey 
 * (a complete settings defaults substitution).
 *
 * Overrides are a single dictionary of PM settings merged into the 
 * default PM profiles found on the root volume, under the PM bundle, as
 * com.apple.SystemPowerProfileDefaults.plist
 *
 * Alternatively, Overrides are a 3 dictionary set, each dictionary
 * being a proper PM settings dictionary. The 3 keys must be
 * "AC Power", "Battery Power" and "UPS Power" respectively. Each
 * dictionary under those keys should contain only PM settings.
 * 
 * DefaultProfiles is a CFArray of size 5, containing CFDictionaries
 * which each contain 3 dictionaries in turn
 */
static CFArrayRef      _copySystemProvidedProfiles()
{
    io_registry_entry_t         registry_entry = MACH_PORT_NULL;
    CFTypeRef                   cftype_total_prof_override = NULL;
    CFTypeRef                   cftype_overrides = NULL;
    CFArrayRef                  retArray = NULL;
    CFDictionaryRef             overrides = NULL;
    CFDictionaryRef             ac_over = NULL;
    CFDictionaryRef             batt_over = NULL;
    CFDictionaryRef             ups_over = NULL;

    CFArrayRef                  sysPowerProfiles = NULL;
    CFMutableArrayRef           mArrProfs = NULL;
    int                         count = 0;
    int                         i = 0;
    
    registry_entry = getPMRootDomainRef();
    if(MACH_PORT_NULL == registry_entry) return NULL;
    
    /* O v e r r i d e */

    cftype_overrides = IORegistryEntryCreateCFProperty(registry_entry, 
                        CFSTR(kIOPMSystemDefaultOverrideKey),
                        kCFAllocatorDefault, 0);
    if( !(overrides = isA_CFDictionary(cftype_overrides)) ) {
        // Expect overrides to be a CFDictionary. If not, skip.
        if(cftype_overrides) {
            CFRelease(cftype_overrides); cftype_overrides = NULL;
        }
        goto TrySystemDefaultProfiles;
    }
    
    ac_over = CFDictionaryGetValue(overrides, CFSTR(kIOPMACPowerKey));
    batt_over = CFDictionaryGetValue(overrides, CFSTR(kIOPMBatteryPowerKey));
    ups_over = CFDictionaryGetValue(overrides, CFSTR(kIOPMUPSPowerKey));

    // Overrides either contains 3 dictionaries of PM Settings keyed as
    // AC, Battery, and UPS Power, or it is itself a dictionary of PM Settings.
    if(ac_over && batt_over && ups_over) 
    {
        // Good. All 3 power source settings types are represented.
        // Do nothing here.
    } else if(!ac_over && !batt_over && !ups_over) 
    {
        // The dictionary didn't specify any per-power source overrides, which
        // means that it's a flat dictionary strictly of PM settings.
        // We duplicate it 3 ways, as each overridden setting in this dictionary
        // will be applied to each power source's settings.
        ac_over = batt_over = ups_over = overrides;
    } else {
        // Bad form for overrides dictionary.
        goto TrySystemDefaultProfiles;
    }
    
    // ac_over, batt_over, ups_over now contain the PM settings to be merged
    // into the system's default profiles. The settings defined in ac_over,
    // batt_over, and ups_over, will override the system's defaults from file:
    //
    // com.apple.SystemPowerProfileDefaults.plist in PowerManagement.bundle
    
    
    sysPowerProfiles = _createDefaultSystemProfiles();
    if(!sysPowerProfiles) goto exit;
    count = CFArrayGetCount(sysPowerProfiles);

    mArrProfs = CFArrayCreateMutable(0, count, &kCFTypeArrayCallBacks);
    for(i=0; i<count; i++)
    {
        CFMutableDictionaryRef      mSettingsAC;
        CFMutableDictionaryRef      mSettingsBatt;
        CFMutableDictionaryRef      mSettingsUPS;
        CFMutableDictionaryRef      mProfile;
        CFDictionaryRef             _profile;
        CFDictionaryRef             tmp;

        _profile = (CFDictionaryRef)CFArrayGetValueAtIndex(sysPowerProfiles, i);
        if(!_profile) continue;

        // Create a new mutable profile to modify & override selected settings        
        mProfile = CFDictionaryCreateMutable(0, 
                        CFDictionaryGetCount(_profile), 
                        &kCFTypeDictionaryKeyCallBacks, 
                        &kCFTypeDictionaryValueCallBacks);
                        
        if(!mProfile) continue;        
        // Add new mutable profile to new mutable array of profiles
        CFArraySetValueAtIndex(mArrProfs, i, mProfile);
        CFRelease(mProfile);

        tmp = (CFDictionaryRef)CFDictionaryGetValue(_profile, 
                        CFSTR(kIOPMACPowerKey));
        if(!tmp) continue;
        mSettingsAC = CFDictionaryCreateMutableCopy(0, 
                        CFDictionaryGetCount(tmp), tmp);
                        

        tmp = (CFDictionaryRef)CFDictionaryGetValue(_profile, 
                        CFSTR(kIOPMBatteryPowerKey));
        if(!tmp) continue;
        mSettingsBatt = CFDictionaryCreateMutableCopy(0, 
                        CFDictionaryGetCount(tmp), tmp);

        tmp = (CFDictionaryRef)CFDictionaryGetValue(_profile, 
                        CFSTR(kIOPMUPSPowerKey));
        if(!tmp) continue;
        mSettingsUPS = CFDictionaryCreateMutableCopy(0, 
                        CFDictionaryGetCount(tmp), tmp);
        
        if( !(mSettingsAC && mSettingsBatt && mSettingsUPS) ) {
            if(sysPowerProfiles) { 
                CFRelease(sysPowerProfiles); sysPowerProfiles = NULL;
            }
            if(mSettingsAC) {
                CFRelease(mSettingsAC); mSettingsAC = NULL;
            }
            if(mSettingsBatt) {
                CFRelease(mSettingsBatt); mSettingsBatt = NULL;
            }
            if(mSettingsUPS) {
                CFRelease(mSettingsUPS); mSettingsUPS = NULL;
            }
            if(mArrProfs) {
                CFRelease(mArrProfs); mArrProfs = NULL;
            }
            goto TrySystemDefaultProfiles;
        }

        // Add these new mutable dictionaries to our new mutable profile
        
        CFDictionarySetValue(mProfile, 
                            CFSTR(kIOPMACPowerKey), 
                            mSettingsAC);
        
        CFDictionarySetValue(mProfile, 
                            CFSTR(kIOPMBatteryPowerKey), 
                            mSettingsBatt);

        CFDictionarySetValue(mProfile, 
                            CFSTR(kIOPMUPSPowerKey), 
                            mSettingsUPS);

        // And now... what we've all been waiting for... merge in the system
        // platform expert's provided default profiles.
        // true == overwrite existing settings
        mergeDictIntoMutable(mSettingsAC, ac_over, kOverWriteDuplicates);
        mergeDictIntoMutable(mSettingsBatt, batt_over, kOverWriteDuplicates);
        mergeDictIntoMutable(mSettingsUPS, ups_over, kOverWriteDuplicates);

        // And release...

        CFRelease(mSettingsAC); mSettingsAC = NULL;
        CFRelease(mSettingsBatt); mSettingsBatt = NULL;
        CFRelease(mSettingsUPS); mSettingsUPS = NULL;
    }

    // Currently holding one retain on mArrProfs
    retArray = (CFArrayRef)mArrProfs;
    
    goto exit;

TrySystemDefaultProfiles:

    /* D e f a u l t   P r o f i l e s */

    // If there were no override PM settings, we check for a complete
    // power profiles definition instead. If so, return the CFArray
    // it contains wholesale.

    cftype_total_prof_override = IORegistryEntryCreateCFProperty(registry_entry, 
                        CFSTR(kIOPMSystemDefaultProfilesKey),
                        kCFAllocatorDefault, 0);
    if( isA_CFArray(cftype_total_prof_override) ) {
        retArray = (CFArrayRef)cftype_total_prof_override;
        goto exit;
    } else {
        if(cftype_total_prof_override) {
            CFRelease(cftype_total_prof_override);
            cftype_total_prof_override = NULL;
        }
    }

exit:
    if(sysPowerProfiles) { 
        CFRelease(sysPowerProfiles); sysPowerProfiles = NULL; 
    }
    if(cftype_overrides) {
        CFRelease(cftype_overrides); cftype_overrides = NULL;
    }
    return retArray;
}

static CFArrayRef       _createDefaultSystemProfiles()
{
    CFURLRef                pm_bundle_url = 0;
    CFBundleRef             pm_bundle = 0;
    CFURLRef                profiles_url = 0;
    CFStringRef             profiles_path = 0;
    CFArrayRef              system_default_profiles = 0;
    CFArrayRef              return_array = 0;
    SCPreferencesRef        open_file = 0;
    
    pm_bundle_url = CFURLCreateWithFileSystemPath(0, CFSTR(kIOPMBundlePath), kCFURLPOSIXPathStyle, true);
    if(!pm_bundle_url) {
        goto exit;
    }

    pm_bundle = CFBundleCreate(kCFAllocatorDefault, pm_bundle_url);
    if(!pm_bundle) {
        goto exit;
    }

    profiles_url = CFBundleCopyResourceURL(pm_bundle,
        CFSTR("com.apple.SystemPowerProfileDefaults.plist"), NULL, NULL);
    if(!profiles_url) {
        goto exit;
    }
    profiles_path = CFURLCopyPath(profiles_url);
    
    open_file = SCPreferencesCreate(0, CFSTR("PowerManagementPreferences"), profiles_path);
    if(!open_file) {
        goto exit;
    }    

    system_default_profiles = SCPreferencesGetValue(open_file, CFSTR("SystemProfileDefaults"));
    if(!isA_CFArray(system_default_profiles)) {
        goto exit;
    }
    
    return_array = CFArrayCreateCopy(0, system_default_profiles);
    
exit:
    if(pm_bundle_url)       CFRelease(pm_bundle_url);
    if(pm_bundle)           CFRelease(pm_bundle);
    if(profiles_url)        CFRelease(profiles_url);
    if(profiles_path)       CFRelease(profiles_path);
    if(open_file)           CFRelease(open_file);
    
    return return_array;
}

static CFDictionaryRef      _createDefaultProfileSelections(void)
{
    CFURLRef                pm_bundle_url = 0;
    CFBundleRef             pm_bundle = 0;
    CFURLRef                profiles_url = 0;
    CFStringRef             profiles_path = 0;
    CFDictionaryRef         default_profiles_selection = 0;
    CFDictionaryRef         return_dict = 0;
    SCPreferencesRef        open_file = 0;
    
    pm_bundle_url = CFURLCreateWithFileSystemPath(0, CFSTR(kIOPMBundlePath), kCFURLPOSIXPathStyle, true);
    if(!pm_bundle_url) {
        goto exit;
    }

    pm_bundle = CFBundleCreate(
        kCFAllocatorDefault, 
        pm_bundle_url);
    if(!pm_bundle) {
        goto exit;
    }

    profiles_url = CFBundleCopyResourceURL(
        pm_bundle,
        CFSTR("com.apple.SystemPowerProfileDefaults.plist"),
        NULL,
        NULL);
    if(!profiles_url) {
        //syslog(LOG_INFO, "Can't find path to profiles\n");
        goto exit;
    }
    profiles_path = CFURLCopyPath(profiles_url);
    
    open_file = SCPreferencesCreate(
        kCFAllocatorDefault,
        CFSTR("PowerManagementPreferences"),
        profiles_path);
    if(!open_file) {
        //syslog(LOG_INFO, "PM could not open System Profile defaults\n");
        goto exit;
    }    

    default_profiles_selection = SCPreferencesGetValue(
        open_file,
        CFSTR("DefaultProfileChoices"));
    if(!isA_CFDictionary(default_profiles_selection)) {
        goto exit;
    }
    
    return_dict = CFDictionaryCreateCopy(kCFAllocatorDefault, default_profiles_selection);
    
exit:
    if(pm_bundle_url)       CFRelease(pm_bundle_url);
    if(pm_bundle)           CFRelease(pm_bundle);
    if(profiles_url)        CFRelease(profiles_url);
    if(profiles_path)       CFRelease(profiles_path);
    if(open_file)           CFRelease(open_file);
    
    return return_dict;
}

static CFDictionaryRef _createAllCustomProfileSelections(void)
{
    int                         j = -1;
    CFNumberRef                 n;
    CFMutableDictionaryRef      custom_dict = NULL;

    custom_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 3, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &j);
    if(!custom_dict || !n) return NULL;
    
    CFDictionarySetValue(custom_dict, CFSTR(kIOPMACPowerKey), n);
    CFDictionarySetValue(custom_dict, CFSTR(kIOPMBatteryPowerKey), n);
    CFDictionarySetValue(custom_dict, CFSTR(kIOPMUPSPowerKey), n);

    CFRelease(n);
    return custom_dict;
}


CFArrayRef          IOPMCopyPowerProfiles(void)
{
    CFArrayRef                      power_profiles = 0;
    CFMutableArrayRef               mutable_power_profiles = 0;
    CFDictionaryRef                 tmp;
    CFMutableDictionaryRef          mutable_profile;
    int                             i, p_count;

    // Provide the platform expert driver a chance to define better default
    // power settings for the machine this code is running on.
    power_profiles = _copySystemProvidedProfiles();
    if(!power_profiles) {
        power_profiles = _createDefaultSystemProfiles();
    }
    if(!power_profiles) return NULL;

    mutable_power_profiles = CFArrayCreateMutableCopy(0, 0, power_profiles);
    if(!mutable_power_profiles) goto exit;
    
    // Prune unsupported power supplies and unsupported
    // settings
    p_count = CFArrayGetCount(mutable_power_profiles);
    for(i=0; i<p_count; i++)
    {
        tmp = CFArrayGetValueAtIndex(power_profiles, i);
        if(!tmp)
            continue;
        
        mutable_profile = CFDictionaryCreateMutableCopy(0, 0, tmp);
        if(!mutable_profile) 
            continue;
        
        mergeUserDefaultOverriddenSettings(mutable_profile);

        addDefaultEnergySettings(mutable_profile);
        
        IOPMRemoveIrrelevantProperties(mutable_profile);
        
        CFArraySetValueAtIndex(mutable_power_profiles, i, mutable_profile);
        CFRelease(mutable_profile);
    }
exit:
    if(power_profiles) CFRelease(power_profiles);    
    return mutable_power_profiles;
}

static int _isActiveProfileDictValid(CFDictionaryRef p)
{
    CFNumberRef     val;
    int             j;
    
    if(!p) return 0;
    
    // AC value required
    val = CFDictionaryGetValue(p, CFSTR(kIOPMACPowerKey));
    if(!val) return 0;
    CFNumberGetValue(val, kCFNumberIntType, &j);
    if(j<-1 || j>= kIOPMNumPowerProfiles) return 0;

    // Battery value optional
    val = CFDictionaryGetValue(p, CFSTR(kIOPMBatteryPowerKey));
    if(val) {
        CFNumberGetValue(val, kCFNumberIntType, &j);
        if(j<-1 || j>= kIOPMNumPowerProfiles) return 0;
    }
    
    // UPS value optional
    val = CFDictionaryGetValue(p, CFSTR(kIOPMUPSPowerKey));
    if(val) {
        CFNumberGetValue(val, kCFNumberIntType, &j);
        if(j<-1 || j>= kIOPMNumPowerProfiles) return 0;
    }
    
    return 1;    
}

static void _purgeUnsupportedPowerSources(CFMutableDictionaryRef p)
{
    CFStringRef                     *ps_names = NULL;
    CFTypeRef                       ps_snap = NULL;
    int                             count;
    int                             i;

    ps_snap = IOPSCopyPowerSourcesInfo();
    if(!ps_snap) return;
    count = CFDictionaryGetCount(p);
    ps_names = (CFStringRef *)malloc(count*sizeof(CFStringRef));
    if(!ps_names) goto exit;
    CFDictionaryGetKeysAndValues(p, (CFTypeRef *)ps_names, NULL);
    for(i=0; i<count; i++)
    {
        if(kCFBooleanTrue != IOPSPowerSourceSupported(ps_snap, ps_names[i])) {
            CFDictionaryRemoveValue(p, ps_names[i]);
        }    
    }
exit:
    if(ps_snap) CFRelease(ps_snap);
    if(ps_names) free(ps_names);
}

static CFStringRef getPowerSourceString(int i)
{
    if(i == 1) return CFSTR(kIOPMBatteryPowerKey);
    if(i == 2) return CFSTR(kIOPMUPSPowerKey);

    return CFSTR(kIOPMACPowerKey);
}

/**************************************************
*
* Support for IOPMPrefsNotificationCreateRunLoopSource
*
**************************************************/

/* SCDynamicStoreCallback */
static void ioCallout(
    SCDynamicStoreRef store __unused, 
    CFArrayRef keys __unused, 
    void *ctxt) 
{
    user_callback_context   *c; 
    IOPowerSourceCallbackType cb;

    c = (user_callback_context *)CFDataGetBytePtr((CFDataRef)ctxt);
    if(!c) return;
    cb = c->callback;
    if(!cb) return;
    
    // Execute callback
    (*cb)(c->context);
}


/*******************************************************************************
****
**** readAllPMPlistSettings
****
*******************************************************************************/

IOReturn readAllPMPlistSettings(
    bool                        removeUnsupportedSettings,
    CFMutableDictionaryRef      *customSettings, 
    CFDictionaryRef             *profileSelections,
    bool                        *returnDefaultSettings)
{
    /*
     * Read Custom Settings
     */
    CFMutableDictionaryRef                  energyDict = NULL;
    CFDictionaryRef                         tmp_dict = NULL;
    SCPreferencesRef                        energyPrefs = NULL;
    CFMutableDictionaryRef                  batterySettings = NULL;
    CFMutableDictionaryRef                  ACSettings = NULL;
    CFMutableDictionaryRef                  UPSSettings = NULL;
    bool                                    usingDefaults = true;
    bool                                    prefsSuccess = false;

    /* 
     * Read profiles
     */
    CFDictionaryRef                         tmp = NULL;
    CFDictionaryRef                         defaultProfiles = NULL;
    CFStringRef                             *profileKeys = NULL;
    CFNumberRef                             *profileValues = NULL;
    bool                                    activeProfilesSpecified = false;
    int                                     profileCount;
    int                                     i;
    bool                                    profilesSuccess = false;
    CFMutableDictionaryRef                  acquiredProfiles = NULL;


    energyPrefs = SCPreferencesCreate( 0, kIOPMAppName, kIOPMPrefsPath );

    if(!energyPrefs) {
        // Utter failure!!!
        if(customSettings) *customSettings = NULL;
        if(profileSelections) *profileSelections = NULL;
        return kIOReturnNotOpen;
    }

/*******************************************************************************
**** Read raw profiles com.apple.PowerManagement.plist
*******************************************************************************/
    
    tmp = SCPreferencesGetValue(energyPrefs, CFSTR("ActivePowerProfiles"));
    if(tmp && _isActiveProfileDictValid(tmp)) {
        acquiredProfiles = CFDictionaryCreateMutableCopy(0, 0, tmp);
        activeProfilesSpecified = true;
    } else {
        acquiredProfiles = NULL;
    }
    
/*******************************************************************************
**** Read custom settings dictionary from com.apple.PowerManagement.plist
*******************************************************************************/

    // Attempt to read battery & AC settings
    tmp_dict = isA_CFDictionary(SCPreferencesGetValue(energyPrefs, 
                                        CFSTR("Custom Profile")));
    
    // If com.apple.PowerManagement.xml opened correctly, read data from it
    if(tmp_dict)
    {
        usingDefaults = false;
        
        // Tiger preferences file format
        energyDict = CFDictionaryCreateMutableCopy(
            kCFAllocatorDefault,
            0,
            tmp_dict);
        if(!energyDict) goto prefsExit;
    } else {
        // Try Panther/Jaguar  prefs formats

        batterySettings = (CFMutableDictionaryRef)isA_CFDictionary(
            SCPreferencesGetValue(energyPrefs, CFSTR(kIOPMBatteryPowerKey)));
        ACSettings = (CFMutableDictionaryRef)isA_CFDictionary(
            SCPreferencesGetValue(energyPrefs, CFSTR(kIOPMACPowerKey)));
        UPSSettings = (CFMutableDictionaryRef)isA_CFDictionary(
            SCPreferencesGetValue(energyPrefs, CFSTR(kIOPMUPSPowerKey)));

        if ( batterySettings || ACSettings || UPSSettings ) 
            usingDefaults = false;

        energyDict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 
            0, 
            &kCFTypeDictionaryKeyCallBacks, 
            &kCFTypeDictionaryValueCallBacks);
        if(!energyDict) goto prefsExit;
        
        if(batterySettings) {
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMBatteryPowerKey),
                                                batterySettings);
        }
        if(ACSettings) {
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMACPowerKey), 
                                                ACSettings);
        }
        if(UPSSettings) {
            CFDictionaryAddValue(energyDict, CFSTR(kIOPMUPSPowerKey), 
                                                UPSSettings);
        }
    }
    
    // Make sure that the enclosed dictionaries are all mutable
    tmp = isA_CFDictionary(CFDictionaryGetValue(
                                energyDict, CFSTR(kIOPMBatteryPowerKey)));
    if(tmp) {
        batterySettings = CFDictionaryCreateMutableCopy(0, 0, tmp);
    } else { 
        batterySettings = CFDictionaryCreateMutable(0, 0, 
                                    &kCFTypeDictionaryKeyCallBacks, 
                                    &kCFTypeDictionaryValueCallBacks);
    }
    if(batterySettings)
    {
        CFDictionarySetValue(energyDict, CFSTR(kIOPMBatteryPowerKey), 
                                         batterySettings);
        CFRelease(batterySettings);
    } else goto prefsExit;

    tmp = isA_CFDictionary(CFDictionaryGetValue(
                                energyDict, CFSTR(kIOPMACPowerKey)));
    if(tmp) {
        ACSettings = CFDictionaryCreateMutableCopy(0, 0, tmp);
    } else {
        ACSettings = CFDictionaryCreateMutable(0, 0, 
                                    &kCFTypeDictionaryKeyCallBacks, 
                                    &kCFTypeDictionaryValueCallBacks);
    }
    if(ACSettings) {
        CFDictionarySetValue(energyDict, CFSTR(kIOPMACPowerKey), ACSettings);
        CFRelease(ACSettings);
    } else goto prefsExit;

    tmp = isA_CFDictionary(CFDictionaryGetValue(
                                energyDict, CFSTR(kIOPMUPSPowerKey)));
    if(tmp) {
        UPSSettings = CFDictionaryCreateMutableCopy(0, 0, tmp);        
    } else {
        UPSSettings = CFDictionaryCreateMutable(0, 0, 
                                    &kCFTypeDictionaryKeyCallBacks, 
                                    &kCFTypeDictionaryValueCallBacks);
    }
    if(UPSSettings) {
        CFDictionarySetValue(energyDict, CFSTR(kIOPMUPSPowerKey), UPSSettings);
        CFRelease(UPSSettings);
    } else goto prefsExit;
    
    // INVARIANT: At this point we want a mutable dictionary energyDict
    // containing 3 mutable preferences dictionaries that are either 
    // empty or contain some settings.

    // Check for existence of Puma/Cheetah prefs format
    // And add settings defined there if present
    getCheetahPumaEnergySettings(energyDict);

    // Fill in any undefined settings from the system's provided defaults
    // IOPlatformExpert may specify different profile defaults, so we must
    // respect those when present.
    addSystemProfileEnergySettings(acquiredProfiles, energyDict);

    // Fill in any settings overriden by a user process.
    mergeUserDefaultOverriddenSettings(energyDict);
    
    // Fill in any undefined settings with our defaults
    // If no current or legacy prefs files exist, addDefaultEnergySettings()
    // completely populates the default EnergySaver preferences.
    addDefaultEnergySettings(energyDict);

    if (removeUnsupportedSettings)
    {
        // Remove any unsupported key/value pairs (including some of 
        // those we just added in getDefaultEnergySettings)    
        IOPMRemoveIrrelevantProperties(energyDict);
    }
    
    if(usingDefaults) {
        // If we couldn't find any user-specified settings on disk, 
        // tag this dictionary as "Defaults" so that BatteryMonitor and 
        // EnergySaver can tell whether these are user-selected
        // values or just the system defaults.
        CFDictionarySetValue(energyDict, CFSTR(kIOPMDefaultPreferencesKey),
                                         kCFBooleanTrue);
    }

    prefsSuccess = true;
    if( customSettings ) {
        *customSettings = energyDict;
    }

prefsExit:

    if ( !prefsSuccess && customSettings) {
        *customSettings = NULL;
    }

/*******************************************************************************
 **** Read custom profiles dictionary from com.apple.PowerManagement.plist
 ******************************************************************************/

    if( !profileSelections ) {
        profilesSuccess = true;
    } else {
        // acquiredProfiles already contains the pre-read profiles data
        if(!acquiredProfiles) {
            acquiredProfiles = CFDictionaryCreateMutable(0, 3, 
                        &kCFTypeDictionaryKeyCallBacks, 
                        &kCFTypeDictionaryValueCallBacks);
        }
        
        
        if(!activeProfilesSpecified && !usingDefaults)
        {
            defaultProfiles = _createAllCustomProfileSelections();        
        } else {
            defaultProfiles = _createDefaultProfileSelections();
        }
        
        // Merge default profiles into active profiles
        // If the user has any specified Profiles, we'll merge in the defaults
        // for any power sources that aren't specified. If there weren't any
        // profile choices, we'll just merge in (-1, -1, -1) for all custom or
        // (2, 1, 1) to the empty dictionary, as described in comments above.
        
        if(isA_CFDictionary(defaultProfiles))
        {
            profileCount = CFDictionaryGetCount(defaultProfiles);
            profileKeys = malloc(sizeof(CFStringRef)*profileCount);
            profileValues = malloc(sizeof(CFNumberRef)*profileCount);
            if ( !profileKeys || !profileValues ) goto profilesExit;
            CFDictionaryGetKeysAndValues(defaultProfiles, 
                    (const void **)profileKeys, (const void **)profileValues);

            for(i=0; i<profileCount; i++)
            {
                if( isA_CFString(profileKeys[i]) &&
                    isA_CFNumber(profileValues[i]) )
                {
                    // use the softer AddValue that won't replace any existing
                    // settings in the existing chosen profiles dictionary.
                    CFDictionaryAddValue(acquiredProfiles, profileKeys[i], 
                                                         profileValues[i]);            
                }
            }
            free(profileKeys);
            free(profileValues);
        }
        
        /* Note: removeUnsupportedSettings argument does not affect the pruning
         * of unsupported power sources; we prune these regardless. 
         * And remove all the unsupported profiles that we just added.
         */
        _purgeUnsupportedPowerSources(acquiredProfiles);
      
        *profileSelections = acquiredProfiles;
        profilesSuccess = true;
      
profilesExit:
        if(defaultProfiles) CFRelease(defaultProfiles);
        if(!profilesSuccess) *profileSelections = NULL;
    }

// common exit

    if( energyPrefs ) CFRelease(energyPrefs);

    if( !customSettings && energyDict ) 
    {
        // If the caller did not request a copy of the PM settings, we 
        // release them here. Otherwise it's the caller's responsibility
        // to CFRelease(*customSettings);
    
        CFRelease(energyDict);        
    }
    
    if( !profileSelections && acquiredProfiles )
    {
        // If the caller did not request a copy of the profiles, we 
        // release them here. Otherwise it's the caller's responsibility
        // to CFRelease(*profileSelections);
    
        CFRelease(acquiredProfiles);
    }
    
    if (returnDefaultSettings) {
        *returnDefaultSettings = usingDefaults;
    }
    
    if( prefsSuccess && profilesSuccess) {
        return kIOReturnSuccess;
    } else {
        return kIOReturnError;
    }
}



