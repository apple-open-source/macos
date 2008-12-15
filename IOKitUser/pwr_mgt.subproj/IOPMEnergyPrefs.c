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

#include <sys/cdefs.h>
#include <TargetConditionals.h>

#include "IOSystemConfiguration.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOHibernatePrivate.h>
#include <servers/bootstrap.h>
#include <sys/syslog.h>
#include "IOPMLib.h"
#include "IOPMLibPrivate.h"
#include "powermanagement.h"

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>

#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mount.h>



/* com.apple.PowerManagement.plist general keys 
 *
 *
 */
#define kIOPMPrefsPath              CFSTR("com.apple.PowerManagement.xml")
#define kIOPMAppName                CFSTR("PowerManagement configd")

#define kIOPMSystemPowerKey         CFSTR("SystemPowerSettings")
#define kIOPMCustomProfileKey       CFSTR("Custom Profile")

/* Default Energy Saver settings for IOPMCopyPMPreferences
 * 
 *      AC
 */
#define kACMinutesToDim                 5
#define kACMinutesToSpin                10
#define kACMinutesToSleep               10
#define kACWakeOnRing                   0
#define kACAutomaticRestart             0
#define kACWakeOnLAN                    1
#define kACReduceProcessorSpeed         0
#define kACDynamicPowerStep             1
#define kACSleepOnPowerButton           1
#define kACWakeOnClamshell              1
#define kACWakeOnACChange               0
#define kACReduceBrightness             0
#define kACDisplaySleepUsesDim          1
#define kACMobileMotionModule           1
#define kACGPUSavings                   0

/*
 *      Battery
 */
#define kBatteryMinutesToDim            5
#define kBatteryMinutesToSpin           5
#define kBatteryMinutesToSleep          5
#define kBatteryWakeOnRing              0
#define kBatteryAutomaticRestart        0
#define kBatteryWakeOnLAN               0
#define kBatteryReduceProcessorSpeed    0
#define kBatteryDynamicPowerStep        1
#define kBatterySleepOnPowerButton      0
#define kBatteryWakeOnClamshell         1
#define kBatteryWakeOnACChange          0
#define kBatteryReduceBrightness        1
#define kBatteryDisplaySleepUsesDim     1
#define kBatteryMobileMotionModule      1
#define kBatteryGPUSavings              1

/*
 *      UPS
 */
#define kUPSMinutesToDim                 kACMinutesToDim
#define kUPSMinutesToSpin                kACMinutesToSpin
#define kUPSMinutesToSleep               kACMinutesToSleep
#define kUPSWakeOnRing                   kACWakeOnRing
#define kUPSAutomaticRestart             kACAutomaticRestart
#define kUPSWakeOnLAN                    kACWakeOnLAN
#define kUPSReduceProcessorSpeed         kACReduceProcessorSpeed
#define kUPSDynamicPowerStep             kACDynamicPowerStep
#define kUPSSleepOnPowerButton           kACSleepOnPowerButton
#define kUPSWakeOnClamshell              kACWakeOnClamshell
#define kUPSWakeOnACChange               kACWakeOnACChange
#define kUPSReduceBrightness             kACReduceBrightness
#define kUPSDisplaySleepUsesDim          kACDisplaySleepUsesDim
#define kUPSMobileMotionModule           kACMobileMotionModule
#define kUPSGPUSavings                   kACGPUSavings

/*
 * Settings with same default value across all power sources
 */
#define kTTYSPreventSleepDefault        1


#define kIOHibernateDefaultFile     "/var/vm/sleepimage"
enum { kIOHibernateMinFreeSpace     = 750*1024ULL*1024ULL }; /* 750Mb */

#define kIOPMNumPMFeatures      16

static char *energy_features_array[kIOPMNumPMFeatures] = {
    kIOPMDisplaySleepKey, 
    kIOPMDiskSleepKey,
    kIOPMSystemSleepKey,
    kIOPMWakeOnRingKey,
    kIOPMRestartOnPowerLossKey,
    kIOPMWakeOnLANKey,
    kIOPMReduceSpeedKey,
    kIOPMDynamicPowerStepKey,
    kIOPMSleepOnPowerButtonKey,
    kIOPMWakeOnClamshellKey,
    kIOPMWakeOnACChangeKey,
    kIOPMReduceBrightnessKey,
    kIOPMDisplaySleepUsesDimKey,
    kIOPMMobileMotionModuleKey,
    kIOHibernateModeKey,
    kIOPMTTYSPreventSleepKey
    kIOPMGPUSwitchKey
};

static const unsigned int battery_defaults_array[] = {
    kBatteryMinutesToDim,
    kBatteryMinutesToSpin,
    kBatteryMinutesToSleep,
    kBatteryWakeOnRing,
    kBatteryAutomaticRestart,
    kBatteryWakeOnLAN,
    kBatteryReduceProcessorSpeed,
    kBatteryDynamicPowerStep,
    kBatterySleepOnPowerButton,
    kBatteryWakeOnClamshell,
    kBatteryWakeOnACChange,
    kBatteryReduceBrightness,
    kBatteryDisplaySleepUsesDim,
    kBatteryMobileMotionModule,
    kIOHibernateModeOn | kIOHibernateModeSleep,  /* safe sleep mode */
    kTTYSPreventSleepDefault,
    kBatteryGPUSavings
};

static const unsigned int ac_defaults_array[] = {
    kACMinutesToDim,
    kACMinutesToSpin,
    kACMinutesToSleep,
    kACWakeOnRing,
    kACAutomaticRestart,
    kACWakeOnLAN,
    kACReduceProcessorSpeed,
    kACDynamicPowerStep,
    kACSleepOnPowerButton,
    kACWakeOnClamshell,
    kACWakeOnACChange,
    kACReduceBrightness,
    kACDisplaySleepUsesDim,
    kACMobileMotionModule,
    kIOHibernateModeOn | kIOHibernateModeSleep,  /* safe sleep mode */
    kTTYSPreventSleepDefault,
    kACGPUSavings
};

static const unsigned int ups_defaults_array[] = {
    kUPSMinutesToDim,
    kUPSMinutesToSpin,
    kUPSMinutesToSleep,
    kUPSWakeOnRing,
    kUPSAutomaticRestart,
    kUPSWakeOnLAN,
    kUPSReduceProcessorSpeed,
    kUPSDynamicPowerStep,
    kUPSSleepOnPowerButton,
    kUPSWakeOnClamshell,
    kUPSWakeOnACChange,
    kUPSReduceBrightness,
    kUPSDisplaySleepUsesDim,
    kUPSMobileMotionModule,
    kIOHibernateModeOn | kIOHibernateModeSleep,  /* safe sleep mode */
    kTTYSPreventSleepDefault,
    kUPSGPUSavings
};

#define kIOPMPrefsPath              CFSTR("com.apple.PowerManagement.xml")
#define kIOPMAppName                CFSTR("PowerManagement configd")

/* IOPMRootDomain property keys for default settings
 */
#define kIOPMSystemDefaultProfilesKey "SystemPowerProfiles"
#define kIOPMSystemDefaultOverrideKey "SystemPowerProfileOverrideDict"

/* Keys for Cheetah Energy Settings shim
 */
#define kCheetahDimKey                          CFSTR("MinutesUntilDisplaySleeps")
#define kCheetahDiskKey                         CFSTR("MinutesUntilHardDiskSleeps")
#define kCheetahSleepKey                        CFSTR("MinutesUntilSystemSleeps")
#define kCheetahRestartOnPowerLossKey           CFSTR("RestartOnPowerLoss")
#define kCheetahWakeForNetworkAccessKey         CFSTR("WakeForNetworkAdministrativeAccess")
#define kCheetahWakeOnRingKey                   CFSTR("WakeOnRing")

// Supported Feature bitfields for IOPMrootDomain Supported Features
enum {
    kIOPMSupportedOnAC      = 1<<0,
    kIOPMSupportedOnBatt    = 1<<1,
    kIOPMSupportedOnUPS     = 1<<2
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

/* IOPMAggressivenessFactors
 *
 * The form of data that the kernel understands.
 */
typedef struct {
    unsigned int        fMinutesToDim;
    unsigned int        fMinutesToSpin;
    unsigned int        fMinutesToSleep;

    unsigned int        fWakeOnLAN;
    unsigned int        fWakeOnRing;
    unsigned int        fAutomaticRestart;
    unsigned int        fSleepOnPowerButton;
    unsigned int        fWakeOnClamshell;
    unsigned int        fWakeOnACChange;
    unsigned int        fDisplaySleepUsesDimming;
    unsigned int        fMobileMotionModule;
    unsigned int        fGPU;
} IOPMAggressivenessFactors;


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
static int addDefaultEnergySettings( 
                CFMutableDictionaryRef sys );
static void addSystemProfileEnergySettings(
                CFDictionaryRef user_profile_selections,
                CFMutableDictionaryRef passed_in);
static io_registry_entry_t getPMRootDomainRef(void);
static int ProcessHibernateSettings(
                CFDictionaryRef                 dict, 
                io_registry_entry_t             rootDomain);
static int sendEnergySettingsToKernel(
                CFDictionaryRef                 System, 
                CFStringRef                     prof, 
                bool                            removeUnsupportedSettings,
                IOPMAggressivenessFactors       *p);
static bool getAggressivenessValue(
                CFDictionaryRef                 dict,
                CFStringRef                     key,
                CFNumberType                    type,
                uint32_t                        *ret);
static int getAggressivenessFactorsFromProfile(
                CFDictionaryRef System, 
                CFStringRef prof, 
                IOPMAggressivenessFactors *agg);
static CFStringRef supportedNameForPMName( CFStringRef pm_name );
static bool featureSupportsPowerSource(
                CFTypeRef                       featureDetails, 
                CFStringRef                     power_source);
static void IOPMRemoveIrrelevantProperties(
                CFMutableDictionaryRef energyPrefs);
static int getCheetahPumaEnergySettings(
                CFMutableDictionaryRef energyPrefs);
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
                CFDictionaryRef *profileSelections);


Boolean _IOReadBytesFromFile(
                CFAllocatorRef alloc, 
                const char *path, 
                void **bytes,
                CFIndex *length, 
                CFIndex maxLength);


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
                        NULL );
    
    if(kIOReturnSuccess == ret) {
        return settings;
    } else {
        return NULL;
    }
}

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
                                    &active_profiles );
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

IOReturn IOPMActivatePMPreference(
    CFDictionaryRef SystemProfiles, 
    CFStringRef profile,
    bool removeUnsupportedSettings)
{
    IOPMAggressivenessFactors       *agg = NULL;
    CFDictionaryRef                 activePMPrefs = NULL;
    CFDictionaryRef                 newPMPrefs = NULL;
    SCDynamicStoreRef               dynamic_store = NULL;

    if(0 == isA_CFDictionary(SystemProfiles) || 0 == isA_CFString(profile)) {
        return kIOReturnBadArgument;
    }

    // Activate settings by sending them to the kernel
    agg = (IOPMAggressivenessFactors *)malloc(sizeof(IOPMAggressivenessFactors));
    getAggressivenessFactorsFromProfile(SystemProfiles, profile, agg);
    sendEnergySettingsToKernel( SystemProfiles, profile, 
                                removeUnsupportedSettings, agg);
    free(agg);
    
    // Put the new settings in the SCDynamicStore for interested apps
    dynamic_store = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("IOKit User Library"),
                                         NULL, NULL);
    if(dynamic_store == NULL) return kIOReturnError;

    activePMPrefs = isA_CFDictionary(SCDynamicStoreCopyValue(dynamic_store,  
                                         CFSTR(kIOPMDynamicStoreSettingsKey)));

    newPMPrefs = isA_CFDictionary(CFDictionaryGetValue(SystemProfiles, profile));

    // If there isn't currently a value for kIOPMDynamicStoreSettingsKey
    //    or the current value is different than the new value
    if( !activePMPrefs || (newPMPrefs && !CFEqual(activePMPrefs, newPMPrefs)) )
    {
        // Then set the kIOPMDynamicStoreSettingsKey to the new value
        SCDynamicStoreSetValue(dynamic_store,  
                               CFSTR(kIOPMDynamicStoreSettingsKey),
                               newPMPrefs);
    }
    
    if(activePMPrefs) CFRelease(activePMPrefs);
    CFRelease(dynamic_store);
    return kIOReturnSuccess;
}

/**************************************************/

// A duplicate function for IOPMSetPMPreferences
// Provided as "IOPMSetCustomPMPreferences" for clarity
IOReturn IOPMSetCustomPMPreferences(CFDictionaryRef ESPrefs)
{
    return IOPMSetPMPreferences(ESPrefs);
}

/**************************************************/

// Sets (and activates) Custom power profile
IOReturn IOPMSetPMPreferences(CFDictionaryRef ESPrefs)
{
    IOReturn                    ret = kIOReturnError;
    SCPreferencesRef            energyPrefs = NULL;

    // for touch
    CFStringRef                 energyPrefsKey = NULL;
    SCDynamicStoreRef           ds = NULL;

    if(NULL == ESPrefs)
    {        
        /* TOUCH */
        // Trigger a notification that the prefs have changed
        // Called by pmset touch
        ds = SCDynamicStoreCreate( kCFAllocatorDefault,
                                     CFSTR("IOKit User Library"),
                                     NULL, NULL);
        if(!ds) ret = kIOReturnInternalError;

        // Setup notification for changes in Energy Saver prefences
        energyPrefsKey = SCDynamicStoreKeyCreatePreferences(
                                NULL, kIOPMPrefsPath, 
                                kSCPreferencesKeyCommit);
        if(!energyPrefsKey) ret = kIOReturnInternalError;

        if(!SCDynamicStoreNotifyValue(ds, energyPrefsKey)) {
            if(kSCStatusAccessError == SCError()) {
                ret = kIOReturnNotPrivileged;
            } else {
            ret = kIOReturnError;
            }
            goto commandTouchExit;
        }
        
        ret = kIOReturnSuccess;
                            
commandTouchExit:
        if(ds) CFRelease(ds);
        if(energyPrefsKey) CFRelease(energyPrefsKey);
        return ret;
    }
    
    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, 
                                       kIOPMAppName, kIOPMPrefsPath );
    if(!energyPrefs) return kIOReturnError;
    
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

/*** IOPMFeatureIsAvailable
     Arguments-
        CFStringRef PMFeature - Name of a PM feature
                (like "WakeOnRing" or "Reduce Processor Speed")
        CFStringRef power_source - The current power source 
                  (like "AC Power" or "Battery Power")
     Return value-
        true if the given PM feature is supported on the given power source
        false if the feature is unsupported
 ***/
#if TARGET_OS_EMBEDDED
#define CACHE_FEATURES	1
#else
#define CACHE_FEATURES	0
#endif /* TARGET_OS_EMBEDDED */

bool IOPMFeatureIsAvailable(CFStringRef PMFeature, CFStringRef power_source)
{
#if CACHE_FEATURES
    static
#endif
    CFDictionaryRef             supportedFeatures = NULL;
    CFStringRef                 supportedString = NULL;
    CFTypeRef                   featureDetails = NULL;
    CFArrayRef                  tmp_array = NULL;

    io_registry_entry_t         registry_entry = MACH_PORT_NULL;
    bool                        ret = false;

    if(!power_source) power_source = CFSTR(kIOPMACPowerKey);

    /* Basic sleep timer settings are always available */    
    if(CFEqual(PMFeature, CFSTR(kIOPMDisplaySleepKey))
        || CFEqual(PMFeature, CFSTR(kIOPMSystemSleepKey))
        || CFEqual(PMFeature, CFSTR(kIOPMDiskSleepKey)))
    {
        ret = true;
        goto exit;
    }

    /* TTY connection ability to prevent sleep is always available */
    if( CFEqual(PMFeature, CFSTR(kIOPMTTYSPreventSleepKey)) )
    {
        ret = true;
        goto exit;
    }

    if (!supportedFeatures)
    {
        registry_entry = getPMRootDomainRef();
        if(!registry_entry) goto exit;
        supportedFeatures = IORegistryEntryCreateCFProperty(
                registry_entry, CFSTR("Supported Features"),
                kCFAllocatorDefault, kNilOptions);
    }

// *********************************
// Special case for PowerButtonSleep    

    if(CFEqual(PMFeature, CFSTR(kIOPMSleepOnPowerButtonKey)))
    {
#if CACHE_FEATURES
        ret = false;
#else
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
        if( ps 
            && ( IOPSGetActiveBattery(ps) || IOPSGetActiveUPS(ps) ) 
            && supportedFeatures
            && CFDictionaryGetValue(supportedFeatures, CFSTR("DisplayDims"))
            && !CFEqual(power_source, CFSTR(kIOPMACPowerKey)) )
        {
            ret = true;
        } else {
            ret = false;
        }

        if(ps) CFRelease(ps);
        goto exit;
    }

// ***********************************
// Generic code for all other settings    
    
    if(!supportedFeatures) {
        ret = false;
        goto exit;
    }
    
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
#if !CACHE_FEATURES
    if(supportedFeatures) CFRelease(supportedFeatures);
#endif
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
                        &activeProfiles);

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
    kern_result = bootstrap_look_up(bootstrap_port, 
            kIOPMServerBootstrapName, &server_port);    
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

    mach_port_destroy(mach_task_self(), server_port);
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

static int addDefaultEnergySettings(CFMutableDictionaryRef sys)
{
    CFMutableDictionaryRef  batt = NULL;
    CFMutableDictionaryRef  ac = NULL;
    CFMutableDictionaryRef  ups = NULL;
    int             i;
    CFNumberRef     val;
    CFStringRef     key;


    batt=(CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMBatteryPowerKey));
    ac=(CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMACPowerKey));
    ups=(CFMutableDictionaryRef)CFDictionaryGetValue(sys, CFSTR(kIOPMUPSPowerKey));

    /*
     * Note that in the following "poplulation" loops, we're using 
     * CFDictionaryAddValue rather than CFDictionarySetValue. If a value is 
     * already present AddValue will not replace it.
     */
    
    /* 
     * Populate default battery dictionary 
     */
    
    if(batt) {
        for(i=0; i<kIOPMNumPMFeatures; i++)
        {
            key = CFStringCreateWithCString(kCFAllocatorDefault, 
                        energy_features_array[i], kCFStringEncodingMacRoman);
            val = CFNumberCreate(kCFAllocatorDefault, 
                        kCFNumberSInt32Type, &battery_defaults_array[i]);
            CFDictionaryAddValue(batt, key, val);
            CFRelease(key);
            CFRelease(val);
        }
        CFDictionaryAddValue(batt, CFSTR(kIOHibernateFileKey), 
                        CFSTR(kIOHibernateDefaultFile));
        CFDictionarySetValue(sys, CFSTR(kIOPMBatteryPowerKey), batt);
    }

    /* 
     * Populate default AC dictionary 
     */
    if(ac)
    {
        for(i=0; i<kIOPMNumPMFeatures; i++)
        {
            key = CFStringCreateWithCString(kCFAllocatorDefault, energy_features_array[i], kCFStringEncodingMacRoman);
            val = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ac_defaults_array[i]);
            CFDictionaryAddValue(ac, key, val);
            CFRelease(key);
            CFRelease(val);
        }
        CFDictionaryAddValue(ac, CFSTR(kIOHibernateFileKey), 
                        CFSTR(kIOHibernateDefaultFile));
        CFDictionarySetValue(sys, CFSTR(kIOPMACPowerKey), ac);
    }
    
    /* 
     * Populate default UPS dictionary 
     */
    if(ups) {
        for(i=0; i<kIOPMNumPMFeatures; i++)
        {
            key = CFStringCreateWithCString(kCFAllocatorDefault, 
                        energy_features_array[i], kCFStringEncodingMacRoman);
            val = CFNumberCreate(kCFAllocatorDefault, 
                        kCFNumberSInt32Type, &ups_defaults_array[i]);
            CFDictionaryAddValue(ups, key, val);
            CFRelease(key);
            CFRelease(val);
        }
        CFDictionaryAddValue(ups, CFSTR(kIOHibernateFileKey), 
                        CFSTR(kIOHibernateDefaultFile));
        CFDictionarySetValue(sys, CFSTR(kIOPMUPSPowerKey), ups);
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
                              false); /* false == no overwrites */
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
static io_registry_entry_t  getPMRootDomainRef(void)
{
    static io_registry_entry_t cached_root_domain = MACH_PORT_NULL;
    
    if( MACH_PORT_NULL == cached_root_domain ) {
        cached_root_domain = IORegistryEntryFromPath( kIOMasterPortDefault, 
                        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    }
    return cached_root_domain;
}

typedef struct 
{
    int      fd;
    uint64_t size;
} CleanHibernateFileArgs;

static void *
CleanHibernateFile(void * args)
{
    char *   buf;
    size_t   size, bufSize = 128 * 1024;
    int      fd = ((CleanHibernateFileArgs *) args)->fd;
    uint64_t fileSize = ((CleanHibernateFileArgs *) args)->size;

    (void) fcntl(fd, F_NOCACHE, 1);
    lseek(fd, 0ULL, SEEK_SET);
    buf = calloc(bufSize, sizeof(char));
    if (!buf)
	return (NULL);
	
    size = bufSize;
    while (fileSize)
    {
	if (fileSize < size)
	    size = fileSize;
	if (size != (size_t) write(fd, buf, size))
	    break;
	fileSize -= size;
    }
    close(fd);
    free(buf);

    return (NULL);
}

#define VM_PREFS_PLIST		    "/Library/Preferences/com.apple.virtualMemory.plist"
#define VM_PREFS_ENCRYPT_SWAP_KEY   "UseEncryptedSwap"

__private_extern__ CFMutableDictionaryRef
readPlist(const char * path, UInt32 key);

static boolean_t
EncryptedSwap(void)
{
    CFMutableDictionaryRef propertyList;
    boolean_t              result = FALSE;
    
    propertyList = readPlist(VM_PREFS_PLIST, 0);
    if (propertyList)
    {
	result = ((CFDictionaryGetTypeID() == CFGetTypeID(propertyList))
		&& (kCFBooleanTrue == CFDictionaryGetValue(propertyList, CFSTR(VM_PREFS_ENCRYPT_SWAP_KEY))));
	CFRelease(propertyList);
    }

    return (result);
}

static int 
ProcessHibernateSettings(CFDictionaryRef dict, io_registry_entry_t rootDomain)
{
    IOReturn    ret;
    CFTypeRef   obj;
    CFNumberRef modeNum;
    SInt32      modeValue = 0;
    CFURLRef    url = NULL;
    Boolean createFile = false;
    Boolean haveFile = false;
    struct stat statBuf;
    char	path[MAXPATHLEN];
    int		fd;
    long long	size;
    size_t	len;
    fstore_t	prealloc;
    off_t	filesize;


    if ( !IOPMFeatureIsAvailable( CFSTR(kIOHibernateFeatureKey), NULL ) )
    {
        // Hibernation is not supported; return before we touch anything.
        return 0;
    }
    

    if ((modeNum = CFDictionaryGetValue(dict, CFSTR(kIOHibernateModeKey)))
      && isA_CFNumber(modeNum))
	CFNumberGetValue(modeNum, kCFNumberSInt32Type, &modeValue);
    else
	modeNum = NULL;

    if (modeValue
      && (obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFileKey)))
      && isA_CFString(obj))
    do
    {
	url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
	    obj, kCFURLPOSIXPathStyle, true);

	if (!url || !CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *) path, MAXPATHLEN))
	    break;

	len = sizeof(size);
	if (sysctlbyname("hw.memsize", &size, &len, NULL, 0))
	    break;
	filesize = size;

	if (0 == stat(path, &statBuf))
	{
	    if ((S_IFBLK == (S_IFMT & statBuf.st_mode)) 
                || (S_IFCHR == (S_IFMT & statBuf.st_mode)))
	    {
                haveFile = true;
	    }
	    else if (S_IFREG == (S_IFMT & statBuf.st_mode))
            {
                if (statBuf.st_size >= filesize)
                    haveFile = true;
                else
                    createFile = true;
            }
	    else
		break;
	}
	else
	    createFile = true;

	if (createFile)
	{
            do
            {
                char *    patchpath, save = 0;
		struct    statfs sfs;
                u_int64_t fsfree;

                fd = -1;

		/*
		 * get rid of the filename at the end of the file specification
		 * we only want the portion of the pathname that should already exist
		 */
		if ((patchpath = strrchr(path, '/')))
                {
                    save = *patchpath;
                    *patchpath = 0;
                }

	        if (-1 == statfs(path, &sfs))
                    break;

                fsfree = ((u_int64_t)sfs.f_bfree * (u_int64_t)sfs.f_bsize);
                if ((fsfree - filesize) < kIOHibernateMinFreeSpace)
                    break;

                if (patchpath)
                    *patchpath = save;
                fd = open(path, O_CREAT | O_TRUNC | O_RDWR);
                if (-1 == fd)
                    break;
                if (-1 == fchmod(fd, 01600))
                    break;
        
                prealloc.fst_flags = F_ALLOCATEALL; // F_ALLOCATECONTIG
                prealloc.fst_posmode = F_PEOFPOSMODE;
                prealloc.fst_offset = 0;
                prealloc.fst_length = filesize;
                if (((-1 == fcntl(fd, F_PREALLOCATE, &prealloc))
                    || (-1 == fcntl(fd, F_SETSIZE, &prealloc.fst_length)))
                && (-1 == ftruncate(fd, prealloc.fst_length)))
                    break;

                haveFile = true;
            }
            while (false);
            if (-1 != fd)
            {
                close(fd);
                if (!haveFile)
                    unlink(path);
            }
	}

        if (!haveFile)
            break;

	if (EncryptedSwap() && !createFile)
	{
	    // encryption on - check existing file to see if it has unencrypted content
	    fd = open(path, O_RDWR);
	    if (-1 != fd) do
	    {
		static CleanHibernateFileArgs args;
		IOHibernateImageHeader        header;
		pthread_attr_t                attr;
		pthread_t                     tid;

		len = read(fd, &header, sizeof(IOHibernateImageHeader));
		if (len != sizeof(IOHibernateImageHeader))
		    break;
		if ((kIOHibernateHeaderSignature != header.signature)
		 && (kIOHibernateHeaderInvalidSignature != header.signature))
		    break;
		if (header.encryptStart)
		    break;

		// if so, clean it off the configd thread
		args.fd = fd;
		args.size = header.imageSize;
		if (pthread_attr_init(&attr))
		    break;
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
		    break;
		if (pthread_create(&tid, &attr, &CleanHibernateFile, &args))
		    break;
		pthread_attr_destroy(&attr);
		fd = -1;
	    }
	    while (false);
	    if (-1 != fd)
		close(fd);
	}

#if defined (__i386__) || defined(__x86_64__) 
#define kBootXPath		"/System/Library/CoreServices/boot.efi"
#define kBootXSignaturePath	"/System/Library/Caches/com.apple.bootefisignature"
#else
#define kBootXPath		"/System/Library/CoreServices/BootX"
#define kBootXSignaturePath	"/System/Library/Caches/com.apple.bootxsignature"
#endif
#define kCachesPath		"/System/Library/Caches"
#define	kGenSignatureCommand	"/bin/cat " kBootXPath " | /usr/bin/openssl dgst -sha1 -hex -out " kBootXSignaturePath


        struct stat bootx_stat_buf;
        struct stat bootsignature_stat_buf;
    
        if (0 != stat(kBootXPath, &bootx_stat_buf))
            break;

        if ((0 != stat(kBootXSignaturePath, &bootsignature_stat_buf))
         || (bootsignature_stat_buf.st_mtime != bootx_stat_buf.st_mtime))
        {
	    if (-1 == stat(kCachesPath, &bootsignature_stat_buf))
	    {
		mkdir(kCachesPath, 0777);
		chmod(kCachesPath, 0777);
	    }

            // generate signature file
            if (0 != system(kGenSignatureCommand))
               break;

            // set mod time to that of source
            struct timeval fileTimes[2];
	    TIMESPEC_TO_TIMEVAL(&fileTimes[0], &bootx_stat_buf.st_atimespec);
	    TIMESPEC_TO_TIMEVAL(&fileTimes[1], &bootx_stat_buf.st_mtimespec);
            if ((0 != utimes(kBootXSignaturePath, fileTimes)))
                break;
        }


        // send signature to kernel
	CFAllocatorRef alloc;
        void *         sigBytes;
        CFIndex        sigLen;

	alloc = CFRetain(CFAllocatorGetDefault());
        if (_IOReadBytesFromFile(alloc, kBootXSignaturePath, &sigBytes, &sigLen, 0))
            ret = sysctlbyname("kern.bootsignature", NULL, NULL, sigBytes, sigLen);
        else
            ret = -1;
        CFAllocatorDeallocate(alloc, sigBytes);
	CFRelease(alloc);
        if (0 != ret)
            break;

        ret = IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileKey), obj);
    }
    while (false);

    if (modeNum)
	ret = IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateModeKey), modeNum);

    if ((obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFreeRatioKey)))
      && isA_CFNumber(obj))
    {
	ret = IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFreeRatioKey), obj);
    }
    if ((obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFreeTimeKey)))
      && isA_CFNumber(obj))
    {
	ret = IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFreeTimeKey), obj);
    }

    if (url)
	CFRelease(url);

    return (0);
}

static int sendEnergySettingsToKernel(
    CFDictionaryRef                 System, 
    CFStringRef                     prof, 
    bool                            removeUnsupportedSettings,
    IOPMAggressivenessFactors       *p
)
{
    io_registry_entry_t             PMRootDomain = MACH_PORT_NULL;
    io_connect_t        		    PM_connection = MACH_PORT_NULL;
    CFTypeRef                       power_source_info = NULL;
    CFStringRef                     providing_power = NULL;
    IOReturn    	                err;
    IOReturn                        ret;
    CFNumberRef                     number1;
    CFNumberRef                     number0;
    CFNumberRef                     num;
    int                             type;
    uint32_t                        i;
    
    i = 1;
    number1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
    i = 0;
    number0 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
    if(!number0 || !number1) return -1;
    
    PMRootDomain = getPMRootDomainRef();
    if(!PMRootDomain) return -1;

    PM_connection = IOPMFindPowerManagement(0);
    if ( !PM_connection ) return -1;

    // Determine type of power source
    power_source_info = IOPSCopyPowerSourcesInfo();
    if(power_source_info) {
        providing_power = IOPSGetProvidingPowerSourceType(power_source_info);
    }
    

    // System sleep - setAggressiveness seconds
    IOPMSetAggressiveness(PM_connection, kPMMinutesToSleep, p->fMinutesToSleep);
    // Disk sleep - setAggressiveness seconds
    IOPMSetAggressiveness(PM_connection, kPMMinutesToSpinDown, p->fMinutesToSpin);
    // Display sleep - setAggressiveness seconds
    IOPMSetAggressiveness(PM_connection, kPMMinutesToDim, p->fMinutesToDim);


    // Wake on LAN
    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnLANKey), providing_power))
    {
        type = kPMEthernetWakeOnLANSettings;
        err = IOPMSetAggressiveness(PM_connection, type, p->fWakeOnLAN);
    } else {
        // Even if WakeOnLAN is reported as not supported, broadcast 0 as 
        // value. We may be on a supported machine, just on battery power.
        // Wake on LAN is not supported on battery power on PPC hardware.
        type = kPMEthernetWakeOnLANSettings;
        err = IOPMSetAggressiveness(PM_connection, type, 0);
    }
    
    // Display Sleep Uses Dim
    if ( !removeUnsupportedSettings
         || IOPMFeatureIsAvailable(CFSTR(kIOPMDisplaySleepUsesDimKey), providing_power))
    {
        ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                                    CFSTR(kIOPMSettingDisplaySleepUsesDimKey), 
                                    (p->fDisplaySleepUsesDimming?number1:number0));
    }    
    
    // Wake On Ring
    if( !removeUnsupportedSettings
         || IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnRingKey), providing_power))
    {
        ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                                    CFSTR(kIOPMSettingWakeOnRingKey), 
                                    (p->fWakeOnRing?number1:number0));
    }
    
    // Automatic Restart On Power Loss, aka FileServer mode
    if( !removeUnsupportedSettings
         || IOPMFeatureIsAvailable(CFSTR(kIOPMRestartOnPowerLossKey), providing_power))
    {
        ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                                    CFSTR(kIOPMSettingRestartOnPowerLossKey), 
                                    (p->fAutomaticRestart?number1:number0));
    }
    
    // Wake on change of AC state -- battery to AC or vice versa
    if( !removeUnsupportedSettings
         || IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnACChangeKey), providing_power))
    {
        ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                                    CFSTR(kIOPMSettingWakeOnACChangeKey), 
                                    (p->fWakeOnACChange?number1:number0));
    }
    
    // Disable power button sleep on PowerMacs, Cubes, and iMacs
    // Default is false == power button causes sleep
    if( !removeUnsupportedSettings
         || IOPMFeatureIsAvailable(CFSTR(kIOPMSleepOnPowerButtonKey), providing_power))
    {
        ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                    CFSTR(kIOPMSettingSleepOnPowerButtonKey), 
                    (p->fSleepOnPowerButton?kCFBooleanFalse:kCFBooleanTrue));
    }    
    
    // Wakeup on clamshell open
    // Default is true == wakeup when the clamshell opens
    if( !removeUnsupportedSettings
         || IOPMFeatureIsAvailable(CFSTR(kIOPMWakeOnClamshellKey), providing_power))
    {
        ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                                    CFSTR(kIOPMSettingWakeOnClamshellKey), 
                                    (p->fWakeOnClamshell?number1:number0));            
    }

    // Mobile Motion Module
    // Defaults to on
    if( !removeUnsupportedSettings
         || IOPMFeatureIsAvailable(CFSTR(kIOPMMobileMotionModuleKey), providing_power))
    {
        ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                                    CFSTR(kIOPMSettingMobileMotionModuleKey), 
                                    (p->fMobileMotionModule?number1:number0));            
    }
    
    /*
     * GPU
     */
    if(true == IOPMFeatureIsAvailable(CFSTR(kIOPMGPUSwitchKey), providing_power))
    {
        num = CFNumberCreate(0, kCFNumberIntType, &p->fGPU);
        if (num) {
            ret = IORegistryEntrySetCFProperty(PMRootDomain, 
                                        CFSTR(kIOPMGPUSwitchKey),
                                        num);            
            CFRelease(num);
        }
    }

    CFDictionaryRef dict = NULL;
    if((dict = CFDictionaryGetValue(System, prof)) )
    {
        ProcessHibernateSettings(dict, PMRootDomain);
    }

    /* PowerStep and Reduce Processor Speed are handled by a separate configd 
       plugin that's watching the SCDynamicStore key 
       State:/IOKit/PowerManagement/CurrentSettings. Changes to the settings 
       notify the configd plugin, which then activates th processor speed 
       settings. Note that IOPMActivatePMPreference updates that key in the 
       SCDynamicStore when we activate new settings. 
       See DynamicPowerStep configd plugin.

       A separate display manager process handles activating the 
       Reduce Brightness key through the same mechanism desribed above for 
       Reduce Process & Dynamic Power Step.
    */
    CFRelease(number0);
    CFRelease(number1);
    if(power_source_info) CFRelease(power_source_info);
    IOServiceClose(PM_connection);
    return 0;
}

/* getAggressivenessValue
 *
 * returns true if the setting existed in the dictionary
 */
static bool getAggressivenessValue(
    CFDictionaryRef     dict,
    CFStringRef         key,
    CFNumberType        type,
    uint32_t           *ret)
{
    CFTypeRef           obj = CFDictionaryGetValue(dict, key);

    *ret = 0;
    if (isA_CFNumber(obj))
    {            
        CFNumberGetValue(obj, type, ret);
        return true;
    } 
    else if (isA_CFBoolean(obj))
    {
        *ret = CFBooleanGetValue(obj);
        return true;
    }
    return false;
}

/* For internal use only */
static int getAggressivenessFactorsFromProfile(
    CFDictionaryRef System, 
    CFStringRef prof, 
    IOPMAggressivenessFactors *agg)
{
    CFDictionaryRef p = NULL;

    if( !(p = CFDictionaryGetValue(System, prof)) )
    {
        return -1;
    }

    if(!agg) return -1;
    
    /*
     * Extract battery settings into s->battery
     */
    
    // dim
    getAggressivenessValue(p, CFSTR(kIOPMDisplaySleepKey),
                           kCFNumberSInt32Type, &agg->fMinutesToDim);
    
    // spin down
    getAggressivenessValue(p, CFSTR(kIOPMDiskSleepKey),
                           kCFNumberSInt32Type, &agg->fMinutesToSpin);

    // sleep
    getAggressivenessValue(p, CFSTR(kIOPMSystemSleepKey),
                           kCFNumberSInt32Type, &agg->fMinutesToSleep);

    // Wake On Magic Packet
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnLANKey),
                           kCFNumberSInt32Type, &agg->fWakeOnLAN);

    // Wake On Ring
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnRingKey),
                           kCFNumberSInt32Type, &agg->fWakeOnRing);

    // AutomaticRestartOnPowerLoss
    getAggressivenessValue(p, CFSTR(kIOPMRestartOnPowerLossKey),
                           kCFNumberSInt32Type, &agg->fAutomaticRestart);
    
    // Disable Power Button Sleep
    getAggressivenessValue(p, CFSTR(kIOPMSleepOnPowerButtonKey),
                           kCFNumberSInt32Type, &agg->fSleepOnPowerButton);    

    // Disable Clamshell Wakeup
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnClamshellKey),
                           kCFNumberSInt32Type, &agg->fWakeOnClamshell);    

    // Wake on AC Change
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnACChangeKey),
                           kCFNumberSInt32Type, &agg->fWakeOnACChange);    

    // Disable intermediate dimming stage for display sleep
    getAggressivenessValue(p, CFSTR(kIOPMDisplaySleepUsesDimKey),
                           kCFNumberSInt32Type, &agg->fDisplaySleepUsesDimming);    

    // MMM
    getAggressivenessValue(p, CFSTR(kIOPMMobileMotionModuleKey),
                           kCFNumberSInt32Type, &agg->fMobileMotionModule);    

    // GPU
    getAggressivenessValue(p, CFSTR(kIOPMGPUSwitchKey),
                           kCFNumberSInt32Type, &agg->fGPU);
    return 0;
}

/* Maps a PowerManagement string constant
 *   -> to its corresponding Supported Feature in IOPMrootDomain
 */
static CFStringRef
supportedNameForPMName( CFStringRef pm_name )
{
#if TARGET_OS_EMBEDDED
    if( CFEqual(pm_name, CFSTR(kIOPMReduceBrightnessKey))
        || CFEqual(pm_name, CFSTR(kIOPMDisplaySleepUsesDimKey)) )
#else
    if(CFEqual(pm_name, CFSTR(kIOPMDisplaySleepUsesDimKey)))
#endif /* TARGET_OS_EMBEDDED */
    {
        return CFSTR("DisplayDims");
    }

    if(CFEqual(pm_name, CFSTR(kIOPMWakeOnLANKey)))
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
    int                         dict_count = 0;
    CFStringRef                 *profile_keys = NULL;
    CFDictionaryRef             *profile_vals = NULL;
    CFStringRef                 *dict_keys    = NULL;
    CFDictionaryRef             *dict_vals    = NULL;
    CFMutableDictionaryRef      this_profile;
    CFTypeRef                   ps_snapshot;
    
    ps_snapshot = IOPSCopyPowerSourcesInfo();
    
    /*
     * Remove features when not supported - 
     *      Wake On Administrative Access, Dynamic Speed Step, etc.
     */
    profile_count = CFDictionaryGetCount(energyPrefs);
    profile_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * profile_count);
    profile_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * profile_count);
    if(!profile_keys || !profile_vals) return;
    
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
            if(!this_profile) continue;

            this_profile = CFDictionaryCreateMutableCopy(NULL, 0, this_profile);
            if(!this_profile) continue;

            CFDictionarySetValue(energyPrefs, profile_keys[profile_count], this_profile);
            CFRelease(this_profile);

            // And prune unneeded settings from our new mutable property            

            dict_count = CFDictionaryGetCount(this_profile);
            dict_keys = (CFStringRef *)malloc(sizeof(CFStringRef) * dict_count);
            dict_vals = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * dict_count);
            if(!dict_keys || !dict_vals) continue;
            CFDictionaryGetKeysAndValues(this_profile, 
                        (const void **)dict_keys, (const void **)dict_vals);
            // For each specific property within each dictionary
            while(--dict_count >= 0)
            {
                if( !IOPMFeatureIsAvailable((CFStringRef)dict_keys[dict_count], 
                                    (CFStringRef)profile_keys[profile_count]) )
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

    free(profile_keys);
    free(profile_vals);
    if(ps_snapshot) CFRelease(ps_snapshot);
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
    if(!energyPrefs) return kIOReturnError;

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
        mergeDictIntoMutable(mSettingsAC, ac_over, true);
        mergeDictIntoMutable(mSettingsBatt, batt_over, true);
        mergeDictIntoMutable(mSettingsUPS, ups_over, true);

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
    
    pm_bundle_url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/SystemConfiguration/PowerManagement.bundle"),
        kCFURLPOSIXPathStyle,
        1);
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

    system_default_profiles = SCPreferencesGetValue(
        open_file,
        CFSTR("SystemProfileDefaults"));
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
    
    if(!return_array) {
        syslog(LOG_INFO, "Power Management error: unable to load default System Power Profiles.\n");
    }    
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
    
    pm_bundle_url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/SystemConfiguration/PowerManagement.bundle"),
        kCFURLPOSIXPathStyle,
        1);
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
    
    if(!return_dict) {
        syslog(LOG_INFO, "Power Management error: unable to load default profiles selections.\n");
    }    
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
        if(!tmp) continue;
        mutable_profile = CFDictionaryCreateMutableCopy(
            kCFAllocatorDefault, 
            0, 
            tmp);
        if(!mutable_profile) continue;
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
    CFDictionaryRef             *profileSelections)
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

    // File in any undefined settings from the system's provided defaults
    // IOPlatformExpert may specify different profile defaults, so we must
    // respect those when present.
    addSystemProfileEnergySettings(acquiredProfiles, energyDict);

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
    
    if( prefsSuccess && profilesSuccess) {
        return kIOReturnSuccess;
    } else {
        return kIOReturnError;
    }
}


