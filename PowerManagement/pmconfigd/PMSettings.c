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
#include <IOKit/IOHibernatePrivate.h>

#include "PMSettings.h"
#include "PrivateLib.h"
#include "PMStore.h"

/* Arguments to CopyPMSettings functions */
enum {
    kIOPMUnabridgedSettings = false,
    kIOPMRemoveUnsupportedSettings = true
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
    unsigned int        fPanicRestartSeconds;
    unsigned int        fDeepSleepEnable;
    unsigned int        fDeepSleepDelay;
} IOPMAggressivenessFactors;

enum { 
    kIOHibernateMinFreeSpace                            = 750*1024ULL*1024ULL  /* 750Mb */
};
//
/* extern symbol defined in IOKit.framework
 * IOCFURLAccess.c
 */
extern Boolean _IOReadBytesFromFile(CFAllocatorRef alloc, const char *path, void **bytes, CFIndex *length, CFIndex maxLength);


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
static long                             gSleepSetting = -1;
#endif

static io_connect_t                     gPowerManager;

/* Tracking sleeping state */
static unsigned long                    deferredPSChangeNotify = 0;
static unsigned long                    _pmcfgd_impendingSleep = 0;

/* Forward Declarations */
static CFDictionaryRef _copyPMSettings(bool removeUnsupported);
static IOReturn activate_profiles(
        CFDictionaryRef                 d, 
        CFStringRef                     s, 
        bool                            removeUnsupported);
static IOReturn ActivatePMSettings(
        CFDictionaryRef                 useSettings, 
        bool                            removeUnsupportedSettings);
static void sendEnergySettingsToKernel(
        CFDictionaryRef                 useSettings, 
        bool                            removeUnsupportedSettings,
        IOPMAggressivenessFactors       *p);
static bool getAggressivenessValue(
        CFDictionaryRef                 dict,
        CFStringRef                     key,
        CFNumberType                    type,
        uint32_t                        *ret);
static int getAggressivenessFactorsFromProfile(
        CFDictionaryRef                 p, 
        IOPMAggressivenessFactors       *agg);
static int ProcessHibernateSettings(
        CFDictionaryRef                 dict, 
        io_registry_entry_t             rootDomain);
void HandleRestartOnKernelPanicSeconds(unsigned int panicSeconds);


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

static void sendEnergySettingsToKernel(
    CFDictionaryRef                 useSettings, 
    bool                            removeUnsupportedSettings,
    IOPMAggressivenessFactors       *p)
{
    io_registry_entry_t             PMRootDomain = getRootDomain();
    io_connect_t                    PM_connection = MACH_PORT_NULL;
    CFDictionaryRef                 _supportedCached = NULL;
    CFTypeRef                       power_source_info = NULL;
    CFStringRef                     providing_power = NULL;
    CFNumberRef                     number1 = NULL;
    CFNumberRef                     number0 = NULL;
    CFNumberRef                     num = NULL;
    uint32_t                        i;
    
    i = 1;
    number1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
    i = 0;
    number0 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
    
    if (!number0 || !number1) 
        goto exit;
    
    PM_connection = IOPMFindPowerManagement(0);
    
    if (!PM_connection) 
        goto exit;
    
    // Determine type of power source
    power_source_info = IOPSCopyPowerSourcesInfo();
    if(power_source_info) {
        providing_power = IOPSGetProvidingPowerSourceType(power_source_info);
    }
    
    // Grab a copy of RootDomain's supported energy saver settings
    _supportedCached = IORegistryEntryCreateCFProperty(PMRootDomain, CFSTR("Supported Features"), kCFAllocatorDefault, kNilOptions);
    
    IOPMSetAggressiveness(PM_connection, kPMMinutesToSleep, p->fMinutesToSleep);
    IOPMSetAggressiveness(PM_connection, kPMMinutesToSpinDown, p->fMinutesToSpin);
    IOPMSetAggressiveness(PM_connection, kPMMinutesToDim, p->fMinutesToDim);
    
    
    // Wake on LAN
    if(true == IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnLANKey), providing_power, _supportedCached))
    {
        IOPMSetAggressiveness(PM_connection, kPMEthernetWakeOnLANSettings, p->fWakeOnLAN);
    } else {
        // Even if WakeOnLAN is reported as not supported, broadcast 0 as 
        // value. We may be on a supported machine, just on battery power.
        // Wake on LAN is not supported on battery power on PPC hardware.
        IOPMSetAggressiveness(PM_connection, kPMEthernetWakeOnLANSettings, 0);
    }
    
    // Display Sleep Uses Dim
    if ( !removeUnsupportedSettings
        || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMDisplaySleepUsesDimKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMSettingDisplaySleepUsesDimKey), 
                                           (p->fDisplaySleepUsesDimming?number1:number0));
    }    
    
    // Wake On Ring
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnRingKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMSettingWakeOnRingKey), 
                                           (p->fWakeOnRing?number1:number0));
    }
    
    // Automatic Restart On Power Loss, aka FileServer mode
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMRestartOnPowerLossKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMSettingRestartOnPowerLossKey), 
                                           (p->fAutomaticRestart?number1:number0));
    }
    
    // Wake on change of AC state -- battery to AC or vice versa
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnACChangeKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMSettingWakeOnACChangeKey), 
                                           (p->fWakeOnACChange?number1:number0));
    }
    
    // Disable power button sleep on PowerMacs, Cubes, and iMacs
    // Default is false == power button causes sleep
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMSleepOnPowerButtonKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMSettingSleepOnPowerButtonKey), 
                                           (p->fSleepOnPowerButton?kCFBooleanFalse:kCFBooleanTrue));
    }    
    
    // Wakeup on clamshell open
    // Default is true == wakeup when the clamshell opens
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnClamshellKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMSettingWakeOnClamshellKey), 
                                           (p->fWakeOnClamshell?number1:number0));            
    }
    
    // Mobile Motion Module
    // Defaults to on
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMMobileMotionModuleKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMSettingMobileMotionModuleKey), 
                                           (p->fMobileMotionModule?number1:number0));            
    }
    
    /*
     * GPU
     */
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMGPUSwitchKey), providing_power, _supportedCached))
    {
        num = CFNumberCreate(0, kCFNumberIntType, &p->fGPU);
        if (num) {
            IORegistryEntrySetCFProperty(PMRootDomain, 
                                               CFSTR(kIOPMGPUSwitchKey),
                                               num);            
            CFRelease(num);
        }
    }
    
    /*
     * Restart on kernel panic
     */
    if (IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMRestartOnKernelPanicKey), providing_power, _supportedCached))
    {        
        HandleRestartOnKernelPanicSeconds(p->fPanicRestartSeconds);
    }
    
    // DeepSleepEnable
    // Defaults to on
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMDeepSleepEnabledKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain, 
                                           CFSTR(kIOPMDeepSleepEnabledKey), 
                                           (p->fDeepSleepEnable?kCFBooleanTrue:kCFBooleanFalse));            
    }
    
    // DeepSleepDelay
    // In seconds
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMDeepSleepDelayKey), providing_power, _supportedCached))
    {
        num = CFNumberCreate(0, kCFNumberIntType, &p->fDeepSleepDelay);
        if (num) {
            IORegistryEntrySetCFProperty(PMRootDomain, 
                                               CFSTR(kIOPMDeepSleepDelayKey), 
                                               num);            
            CFRelease(num);
        }
    }
    
    if (useSettings)
    {
        ProcessHibernateSettings(useSettings, PMRootDomain);
    }
    
    
exit:
    if (number0) {
        CFRelease(number0);
    }
    if (number1) {
        CFRelease(number1);
    }
    if (IO_OBJECT_NULL != PM_connection) {
        IOServiceClose(PM_connection);
    }
    if (power_source_info) {
        CFRelease(power_source_info);
    }
    if (_supportedCached) {
        CFRelease(_supportedCached);
    }    
    return;
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
    CFDictionaryRef p, 
    IOPMAggressivenessFactors *agg)
{
    if( !agg || !p ) {
        return -1;
    }
    
    getAggressivenessValue(p, CFSTR(kIOPMDisplaySleepKey), kCFNumberSInt32Type, &agg->fMinutesToDim);
    getAggressivenessValue(p, CFSTR(kIOPMDiskSleepKey), kCFNumberSInt32Type, &agg->fMinutesToSpin);
    getAggressivenessValue(p, CFSTR(kIOPMSystemSleepKey), kCFNumberSInt32Type, &agg->fMinutesToSleep);
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnLANKey), kCFNumberSInt32Type, &agg->fWakeOnLAN);
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnRingKey), kCFNumberSInt32Type, &agg->fWakeOnRing);
    getAggressivenessValue(p, CFSTR(kIOPMRestartOnPowerLossKey), kCFNumberSInt32Type, &agg->fAutomaticRestart);
    getAggressivenessValue(p, CFSTR(kIOPMSleepOnPowerButtonKey), kCFNumberSInt32Type, &agg->fSleepOnPowerButton);    
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnClamshellKey), kCFNumberSInt32Type, &agg->fWakeOnClamshell);    
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnACChangeKey), kCFNumberSInt32Type, &agg->fWakeOnACChange);    
    getAggressivenessValue(p, CFSTR(kIOPMDisplaySleepUsesDimKey), kCFNumberSInt32Type, &agg->fDisplaySleepUsesDimming);    
    getAggressivenessValue(p, CFSTR(kIOPMMobileMotionModuleKey), kCFNumberSInt32Type, &agg->fMobileMotionModule);    
    getAggressivenessValue(p, CFSTR(kIOPMGPUSwitchKey), kCFNumberSInt32Type, &agg->fGPU);
    getAggressivenessValue(p, CFSTR(kIOPMRestartOnKernelPanicKey), kCFNumberSInt32Type, &agg->fPanicRestartSeconds);
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepEnabledKey), kCFNumberSInt32Type, &agg->fDeepSleepEnable);
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepDelayKey), kCFNumberSInt32Type, &agg->fDeepSleepDelay);
    
    return 0;
}

static IOReturn ActivatePMSettings(
    CFDictionaryRef                 useSettings, 
    bool                            removeUnsupportedSettings)
{
    IOPMAggressivenessFactors       theFactors;
    CFDictionaryRef                 activePMPrefs = NULL;
    
    if(!isA_CFDictionary(useSettings))
    {
        return kIOReturnBadArgument;
    }
    
    // Activate settings by sending them to the multiple owning drivers kernel
    getAggressivenessFactorsFromProfile(useSettings, &theFactors);
    
    sendEnergySettingsToKernel(useSettings, removeUnsupportedSettings, &theFactors);
        
    activePMPrefs = SCDynamicStoreCopyValue(_getSharedPMDynamicStore(), 
                                            CFSTR(kIOPMDynamicStoreSettingsKey));
    
    // If there isn't currently a value for kIOPMDynamicStoreSettingsKey,
    //   or the current value is different than the new value,
    // Put the new settings in the SCDynamicStore for interested apps.
    
    if( !isA_CFDictionary(activePMPrefs) || !CFEqual(activePMPrefs, useSettings) )
    {
        PMStoreSetValue(CFSTR(kIOPMDynamicStoreSettingsKey), useSettings);
    }
    

    if (activePMPrefs)
        CFRelease(activePMPrefs);
    return kIOReturnSuccess;
}

#define kPanicSym                   "getPanicImageData"
#define kPanicLibPath               kPowerManagementBundlePathCString "/libPanicImageData.dylib"
#define kSecondsIn5Years            157680000
#define kPanicRestartSecondsDefault kSecondsIn5Years

void HandleRestartOnKernelPanicSeconds(unsigned int panicSeconds)
{
    int mib[3];
    int result;
    static bool customPanicDisplayed = false;
    
    sysctlbyname("machdep.misc.panic_restart_timeout", NULL, NULL, &panicSeconds, sizeof(panicSeconds));

    mib[0] = CTL_KERN;
    mib[1] = KERN_PANICINFO;
    mib[2] = KERN_PANICINFO_IMAGE;
    
    /* Update the panic UI in the kernel
     */
    if (panicSeconds != kPanicRestartSecondsDefault
        && !customPanicDisplayed)
    {
        static void                    *panic_dlg = NULL;
        static unsigned int            panic_len = 0;
                
        if (!panic_dlg || (0==panic_len))
        {
            void                *dlhandle = NULL;
            void                (*panic_func)(void **, unsigned int *) = NULL;

            dlhandle = dlopen("/System/Library/CoreServices/powerd.bundle/libPanicImageData.dylib", RTLD_LAZY);
            
            if (dlhandle) 
            {
                panic_func = dlsym(dlhandle, kPanicSym);
                
                if (panic_func)
                    panic_func(&panic_dlg, &panic_len);    
            } else {
                asl_log(0, 0, ASL_LEVEL_ERR, "PM dlopen failure error: %s\n", dlerror());
            }
            // Do not dlclose our dlhandle. We leave it open
            // to re-read the panic data from it later on.
        }
        
        
        if (panic_dlg && (0!=panic_len))
        {
            result = sysctl(mib, 3, NULL, NULL, panic_dlg, panic_len);
        }
        
        customPanicDisplayed = true;
    }
    
    if (panicSeconds == kPanicRestartSecondsDefault
        && customPanicDisplayed)
    {
        // Restore default panic dialog
        result = sysctl(mib, 3, NULL, NULL, 0, 0);

        customPanicDisplayed = false;
    }
    
    return;
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
    CFMutableDictionaryRef              profiles_activated;
    IOReturn                            ret;
    CFNumberRef                         n1, n0;
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

#if TARGET_OS_EMBEDDED
    CFNumberRef                         sleepSetting;

    sleepSetting = (CFNumberRef)isA_CFNumber(CFDictionaryGetValue(energy_settings, CFSTR(kIOPMSystemSleepKey)));
    if (sleepSetting) {
        CFNumberGetValue(sleepSetting, kCFNumberLongType, &gSleepSetting);
    }
#endif

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

        
        if (n0)
            CFRelease(n0);
        if (n1)
            CFRelease(n1);
        
        ret = ActivatePMSettings(profiles_activated, removeUnsupported);

        CFRelease(profiles_activated);
    } else {
        ret = ActivatePMSettings(energy_settings, removeUnsupported);
    }
    
    return ret;
}


__private_extern__ void PMSettings_prime(void)
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

#define VM_PREFS_PLIST            "/Library/Preferences/com.apple.virtualMemory.plist"
#define VM_PREFS_ENCRYPT_SWAP_KEY   "UseEncryptedSwap"

static CFDictionaryRef readPlist(const char * path)
{
    CFURLRef                    filePath = NULL;
    CFDataRef                   fileData = NULL;
    CFDictionaryRef             outPList = NULL;
    
    filePath = CFURLCreateFromFileSystemRepresentation(0, (const UInt8 *)path, strlen(path), FALSE);
    if (!filePath)
        goto exit;
    CFURLCreateDataAndPropertiesFromResource(0, filePath, &fileData, NULL, NULL, NULL);
    if (!fileData)
        goto exit;
    outPList = CFPropertyListCreateFromXMLData(0, fileData, kCFPropertyListImmutable, NULL);

exit:
    if (filePath) {
        CFRelease(filePath);
    }
    if (fileData) {
        CFRelease(fileData);
    }
    return outPList;
}

static boolean_t
EncryptedSwap(void)
{
    CFDictionaryRef         propertyList;
    boolean_t              result = FALSE;
    
    propertyList = readPlist(VM_PREFS_PLIST);
    if (propertyList)
    {
        result = ((CFDictionaryGetTypeID() == CFGetTypeID(propertyList))
            && (kCFBooleanTrue == CFDictionaryGetValue(propertyList, CFSTR(VM_PREFS_ENCRYPT_SWAP_KEY))));
        CFRelease(propertyList);
    }

    return (result);
}


static int ProcessHibernateSettings(CFDictionaryRef dict, io_registry_entry_t rootDomain)
{
    IOReturn    ret;
    CFTypeRef   obj;
    CFNumberRef modeNum;
    SInt32      modeValue = 0;
    CFURLRef    url = NULL;
    Boolean createFile = false;
    Boolean haveFile = false;
    struct stat statBuf;
    char    path[MAXPATHLEN];
    int        fd;
    long long    size;
    size_t    len;
    fstore_t    prealloc;
    off_t    filesize;


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
    url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, obj, kCFURLPOSIXPathStyle, true);

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
#define kBootXPath        "/System/Library/CoreServices/boot.efi"
#define kBootXSignaturePath    "/System/Library/Caches/com.apple.bootefisignature"
#else
#define kBootXPath        "/System/Library/CoreServices/BootX"
#define kBootXSignaturePath    "/System/Library/Caches/com.apple.bootxsignature"
#endif
#define kCachesPath        "/System/Library/Caches"
#define    kGenSignatureCommand    "/bin/cat " kBootXPath " | /usr/bin/openssl dgst -sha1 -hex -out " kBootXSignaturePath


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
        if (sigBytes)
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

