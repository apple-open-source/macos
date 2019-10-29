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
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#include <IOKit/IOHibernatePrivate.h>
#include <pthread.h>
#include <notify.h>

#include "PMSettings.h"
#include "BatteryTimeRemaining.h"
#include "PrivateLib.h"
#include "PMStore.h"
#include "PMAssertions.h"
#include "PMConnection.h"
#include "StandbyTimer.h"
#include "adaptiveDisplay.h"


#define kIOPMSCPrefsPath    CFSTR("com.apple.PowerManagement.xml")
#define kIOPMAppName        CFSTR("PowerManagement configd")
#define kIOPMSCPrefsFile    "/Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist"

#define kAlarmDataKey        CFSTR("AlarmData")
os_log_t pmSettings_log = NULL;
#undef   LOG_STREAM
#define  LOG_STREAM   pmSettings_log

enum
{
    // 2GB
    kStandbyDesktopHibernateFileSize = 2ULL*1024*1024*1024,
    // 1GB
    kStandbyPortableHibernateFileSize = 1ULL*1024*1024*1024
};


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
static unsigned long                    gSleepSetting = -1;

static io_connect_t                     gPowerManager;

/* Tracking sleeping state */
static unsigned long                    deferredPSChangeNotify = 0;
static unsigned long                    _pmcfgd_impendingSleep = 0;

static uint32_t gDisplaySleepFactor = 1;

/* Forward Declarations */
static IOReturn activate_profiles(
        CFDictionaryRef                 d, 
        CFStringRef                     s, 
        bool                            removeUnsupported);
static IOReturn PMActivateSystemPowerSettings( void );

static void  migrateSCPrefs(void);
static void updatePowerNapSetting(void);



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

__private_extern__ bool GetSystemPowerSettingBool(CFStringRef which)
{
    CFDictionaryRef system_power_settings = NULL;
    CFBooleanRef value = kCFBooleanFalse;
    system_power_settings = IOPMCopySystemPowerSettings();
    if (!system_power_settings || !which)
        return false;
    if (system_power_settings) {
        value = CFDictionaryGetValue(system_power_settings, which);
        CFRelease(system_power_settings);
    }
    return (value == kCFBooleanTrue) ? true : false;
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


__private_extern__  void
setDisplayToDimTimer(io_connect_t connection, unsigned int minutesToDim)
{
    io_connect_t tmpConnection = MACH_PORT_NULL;

    if (connection == MACH_PORT_NULL) {
        tmpConnection = IOPMFindPowerManagement(0);
        if (!tmpConnection) {
            ERROR_LOG("Failed to open connection to rootDomain\n");
            return;
        }
        connection = tmpConnection;
    }

    IOPMSetAggressiveness(connection, kPMMinutesToDim, minutesToDim * gDisplaySleepFactor);
    if (tmpConnection) {
        IOServiceClose(tmpConnection);
    }

}

__private_extern__ void
setDisplaySleepFactor(unsigned int factor)
{
    uint32_t displaySleepTimer;
    IOReturn rc;

    rc = getDisplaySleepTimer(&displaySleepTimer);
    if (rc != kIOReturnSuccess) {
        ERROR_LOG("Failed to get display sleep timer. rc:0x%x\n", rc);
        return;
    }

    gDisplaySleepFactor = factor;

    setDisplayToDimTimer(IO_OBJECT_NULL, displaySleepTimer);
}

__private_extern__ void saveAlarmInfo(CFDictionaryRef info)
{
    CFPreferencesSetValue(kAlarmDataKey, info, CFSTR(kIOPMCFPrefsPath), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(CFSTR(kIOPMCFPrefsPath), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
}

__private_extern__ CFDictionaryRef copyAlarmInfo()
{
    return CFPreferencesCopyValue(kAlarmDataKey, CFSTR(kIOPMCFPrefsPath), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
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

#ifdef XCTEST
bool xctAllowOnAC;
bool xctAllowOnBatt;

void xctSetPowerNapState(bool allowOnAC, bool allowOnBatt) {
    xctAllowOnAC =  allowOnAC;
    xctAllowOnBatt = allowOnBatt;
}
bool _DWBT_allowed(void) {
    if (_getPowerSource() == kACPowered)
        return xctAllowOnAC;
    return false;
}
bool _SS_allowed(void) {
    if (_DWBT_allowed()) {
        return true;
    }
    return (xctAllowOnBatt && (_getPowerSource() == kBatteryPowered));
}

void xctSetEnergySettings(CFDictionaryRef settings) {
    if (energySettings) {
        CFRelease(energySettings);
    }
    energySettings = CFRetain(settings);
}

#else
/* _DWBT_allowed() tells if a DWBT wake can be scheduled at this moment */
__private_extern__ bool
_DWBT_allowed(void)
{
    return ( (GetPMSettingBool(CFSTR(kIOPMDarkWakeBackgroundTaskKey))) &&
             (kACPowered == _getPowerSource()) );

}

/* Is Sleep Services(aka PowerNap) allowed */
__private_extern__ bool _SS_allowed(void)
{
    if (_DWBT_allowed())
        return true;

    return ( (GetPMSettingBool(CFSTR(kIOPMDarkWakeBackgroundTaskKey))) &&
             (kBatteryPowered == _getPowerSource()) );

}
#endif
/* getAggressivenessValue
 *
 * returns true if the setting existed in the dictionary
 */
__private_extern__ bool getAggressivenessValue(
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
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepEnabledKey), kCFNumberSInt32Type, &agg->fDeepSleepEnable);
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepDelayKey), kCFNumberSInt32Type, &agg->fDeepSleepDelay);
    getAggressivenessValue(p, CFSTR(kIOPMAutoPowerOffEnabledKey), kCFNumberSInt32Type, &agg->fAutoPowerOffEnable);
    getAggressivenessValue(p, CFSTR(kIOPMAutoPowerOffDelayKey), kCFNumberSInt32Type, &agg->fAutoPowerOffDelay);

    return 0;
}

#define kIOPMSystemDefaultOverrideKey    "SystemPowerProfileOverrideDict"

__private_extern__ bool platformPluginLoaded(void)
{
    static bool         gPlatformPluginLoaded = false;
    io_registry_entry_t rootDomain;
    CFTypeRef           prop;

    if (gPlatformPluginLoaded) return (true);

    rootDomain = getRootDomain();
    if (MACH_PORT_NULL == rootDomain) return (false);
    prop = IORegistryEntryCreateCFProperty(rootDomain, CFSTR(kIOPMSystemDefaultOverrideKey),
                                           kCFAllocatorDefault, kNilOptions);
    if (prop)
    {
        gPlatformPluginLoaded = true;
        CFRelease(prop);
    }
    return (gPlatformPluginLoaded);
}

static int ProcessHibernateSettings(CFDictionaryRef dict, bool standby, bool isDesktop, io_registry_entry_t rootDomain)
{
    CFTypeRef   obj = NULL;
    CFNumberRef modeNum;
    CFNumberRef num;
    SInt32      modeValue = 0;
    CFURLRef    url = NULL;
    Boolean createFile = false;
    Boolean haveFile = false;
	Boolean deleteFile = false;
    struct stat statBuf;
    char    path[MAXPATHLEN];
    int        fd;
    long long    size;
    size_t    len;
    fstore_t    prealloc;
    off_t    filesize = 0;
    off_t    minFileSize = 0;
    off_t    maxFileSize = 0;
    bool     apo_available = false;
    SInt32   apo_enabled = 0;
    CFNumberRef apo_enabled_cf = NULL;

    if (!platformPluginLoaded()) return (0);

    if ( !IOPMFeatureIsAvailable( CFSTR(kIOHibernateFeatureKey), NULL ) )
    {
        // Hibernation is not supported; return before we touch anything.
        return 0;
    }

    if ((modeNum = CFDictionaryGetValue(dict, CFSTR(kIOHibernateModeKey))) && isA_CFNumber(modeNum))
        CFNumberGetValue(modeNum, kCFNumberSInt32Type, &modeValue);
    else
        modeNum = NULL;

    apo_available = IOPMFeatureIsAvailable(CFSTR(kIOPMAutoPowerOffEnabledKey), NULL);
    if (apo_available && (apo_enabled_cf = CFDictionaryGetValue(dict, CFSTR(kIOPMAutoPowerOffEnabledKey ))) && isA_CFNumber(apo_enabled_cf))
    {
        CFNumberGetValue(apo_enabled_cf, kCFNumberSInt32Type, &apo_enabled);
    }

    if ((obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFileKey))) && isA_CFString(obj))
        do
        {

            len = sizeof(size);
            if (sysctlbyname("hw.memsize", &size, &len, NULL, 0))
                break;

            filesize = (size >> 1);
            if (isDesktop)
            {
                if (standby && (filesize > kStandbyDesktopHibernateFileSize)) filesize = kStandbyDesktopHibernateFileSize;
            }
            else
            {
                if (standby && (filesize > kStandbyPortableHibernateFileSize)) filesize = kStandbyPortableHibernateFileSize;
            }
            minFileSize = filesize;
            maxFileSize = 0;

            url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, obj, kCFURLPOSIXPathStyle, true);

            if (!url || !CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *) path, MAXPATHLEN))
                break;

            if (0 != stat(path, &statBuf)) {
				createFile = true;
			} else {
                if ((S_IFBLK == (S_IFMT & statBuf.st_mode))
                    || (S_IFCHR == (S_IFMT & statBuf.st_mode))) {
                    haveFile = true;
                } else if (S_IFREG == (S_IFMT & statBuf.st_mode)) {
                    if ((statBuf.st_size == filesize) || (kIOHibernateModeFileResize & modeValue))
                        haveFile = true;
                    else
                        createFile = true;
                } else {
                    break;
				}
            }

			if (!(modeValue || (apo_available && apo_enabled))) {
				INFO_LOG("sleepimage file flagged for deletion\n");
				deleteFile = true;
				break;
			}

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
                    fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 01600);
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

            if (haveFile) {
                IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileKey), obj);
            }
        } while (false);

	if (haveFile && deleteFile) {
			unlink(path);
	}

    if (modeNum)
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateModeKey), modeNum);


    if ((obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFreeRatioKey)))
        && isA_CFNumber(obj))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFreeRatioKey), obj);
    }
    if ((obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFreeTimeKey)))
        && isA_CFNumber(obj))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFreeTimeKey), obj);
    }
    if (minFileSize && (num = CFNumberCreate(NULL, kCFNumberLongLongType, &minFileSize)))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileMinSizeKey), num);
        CFRelease(num);
    }
    if (maxFileSize && (num = CFNumberCreate(NULL, kCFNumberLongLongType, &maxFileSize)))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileMaxSizeKey), num);
        CFRelease(num);
    }

    if (url)
        CFRelease(url);

    return (0);
}



static void sendEnergySettingsToKernel(
                                       CFDictionaryRef                 useSettings,
                                       bool                            removeUnsupportedSettings,
                                       IOPMAggressivenessFactors       *p)
{
    io_registry_entry_t             PMRootDomain = getRootDomain();
    io_connect_t                    PM_connection = MACH_PORT_NULL;
    CFDictionaryRef                 _supportedCached = NULL;
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
    int powersource = getActivePSType();
    if (kIOPSProvidedByExternalBattery == powersource) {
        providing_power = CFSTR(kIOPMUPSPowerKey);
    } else if (kIOPSProvidedByBattery == powersource) {
        providing_power = CFSTR(kIOPMBatteryPowerKey);
    } else {
        providing_power = CFSTR(kIOPMACPowerKey);
    }

    // Grab a copy of RootDomain's supported energy saver settings
    _supportedCached = IORegistryEntryCreateCFProperty(PMRootDomain, CFSTR("Supported Features"), kCFAllocatorDefault, kNilOptions);

    IOPMSetAggressiveness(PM_connection, kPMMinutesToSleep, p->fMinutesToSleep);
    IOPMSetAggressiveness(PM_connection, kPMMinutesToSpinDown, p->fMinutesToSpin);
    setDisplayToDimTimer(PM_connection, p->fMinutesToDim);


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

    // AutoPowerOffEnable
    // Defaults to on
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMAutoPowerOffEnabledKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMAutoPowerOffEnabledKey),
                                     (p->fAutoPowerOffEnable?kCFBooleanTrue:kCFBooleanFalse));
    }

    // AutoPowerOffDelay
    // In seconds
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMAutoPowerOffDelayKey), providing_power, _supportedCached))
    {
        num = CFNumberCreate(0, kCFNumberIntType, &p->fAutoPowerOffDelay);
        if (num) {
            IORegistryEntrySetCFProperty(PMRootDomain,
                                         CFSTR(kIOPMAutoPowerOffDelayKey),
                                         num);
            CFRelease(num);
        }
    }

    // ProModeControl
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMFeatureProModeKey), providing_power, _supportedCached))
    {
		CFNumberRef modeNum;
		if ((modeNum = CFDictionaryGetValue(useSettings, CFSTR(kIOPMProModeKey))) && isA_CFNumber(modeNum)) {
				IORegistryEntrySetCFProperty(PMRootDomain, CFSTR(kIOPMSettingProModeControl), modeNum);
			}
	}

    if ( !_platformSleepServiceSupport && !_platformBackgroundTaskSupport)
    {
        bool ssupdate, btupdate, pnupdate;

        // On legacy systems, IOPPF publishes PowerNap support using
        // the kIOPMDarkWakeBackgroundTaskKey  and/or
        // kIOPMSleepServicesKey
        btupdate = IOPMFeatureIsAvailableWithSupportedTable(
                                                            CFSTR(kIOPMDarkWakeBackgroundTaskKey),
                                                            providing_power, _supportedCached);
        ssupdate = IOPMFeatureIsAvailableWithSupportedTable(
                                                            CFSTR(kIOPMSleepServicesKey),
                                                            providing_power, _supportedCached);

        // But going forward (late 2012 machines and beyond), IOPPF will publish
        // PowerNap support as a PM feature using the kIOPMPowerNapSupportedKey
        pnupdate = IOPMFeatureIsAvailableWithSupportedTable(
                                                            CFSTR(kIOPMPowerNapSupportedKey),
                                                            providing_power, _supportedCached);

        // We have to check for one of either 'legacy' or 'modern' PowerNap
        // support and configure BT assertion and other powerd-internal PowerNap
        // settings accordingly
        if (ssupdate || btupdate || pnupdate) {
            _platformSleepServiceSupport = ssupdate;
            _platformBackgroundTaskSupport = btupdate;
            configAssertionType(kBackgroundTaskType, false);
            mt2EvaluateSystemSupport();
        }
    }

    if (useSettings)
    {
        bool isDesktop = (0 == _batteryCount());
        ProcessHibernateSettings(useSettings, p->fDeepSleepEnable, isDesktop, PMRootDomain);
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
    if (_supportedCached) {
        CFRelease(_supportedCached);
    }
    return;
}

static void sendEnergySettingsToIOPMPowerSource(CFDictionaryRef useSettings) { }

__private_extern__ IOReturn ActivatePMSettings(
    CFDictionaryRef                 useSettings,
    bool                            removeUnsupportedSettings)
{
    IOPMAggressivenessFactors       theFactors;

    if(!isA_CFDictionary(useSettings))
    {
        return kIOReturnBadArgument;
    }

    // Activate settings by sending them to the multiple owning drivers kernel
    getAggressivenessFactorsFromProfile(useSettings, &theFactors);

    sendEnergySettingsToKernel(useSettings, removeUnsupportedSettings, &theFactors);
    sendEnergySettingsToIOPMPowerSource(useSettings);

    evalAllUserActivityAssertions(theFactors.fMinutesToDim);
    evalAllNetworkAccessAssertions();

    return kIOReturnSuccess;
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

    pmSettings_log = os_log_create(PM_LOG_SYSTEM, PMSETTINGS_LOG);
    migrateSCPrefs();
    updatePowerNapSetting();

    // Open a connection to the Power Manager.
    gPowerManager = IOPMFindPowerManagement(MACH_PORT_NULL);
    if (gPowerManager == 0) return;

    // Activate non-power source specific, PM settings
    // namely disable sleep, where appropriate
    PMActivateSystemPowerSettings();

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
    // and removing support. Force trigger prefernces re-evaluation

    notify_post(kIOPMPrefsChangeNotify);
}

static void updatePowerNapSetting( )
{
    char     *model = NULL;
    uint32_t majorRev;
    uint32_t minorRev;
    IOReturn rc;

    CFMutableDictionaryRef prefs = NULL;
    CFDictionaryRef systemSettings = NULL; 
    CFMutableDictionaryRef prefsForSrc;

    rc = IOCopyModel(&model, &majorRev, &minorRev);
    if (rc != kIOReturnSuccess) {
        INFO_LOG("Failed to get the model name\n");
        goto exit;
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

    prefs = IOPMCopyPreferencesOnFile();
    if (!prefs) {
        INFO_LOG("No prefs to update\n");
        goto exit;
    }

    prefsForSrc = (CFMutableDictionaryRef)CFDictionaryGetValue(prefs, CFSTR(kIOPMACPowerKey));
    if (!isA_CFDictionary(prefsForSrc)) {
        INFO_LOG("Invalid prefs found\n");
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
    if (prefs) {
        IOPMSetPMPreferences(prefs);
        CFRelease(prefs);
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


static IOReturn PMActivateSystemPowerSettings( void )
{
    io_registry_entry_t         rootdomain = MACH_PORT_NULL;
    CFDictionaryRef             settings = NULL;
    bool                        disable_sleep = false;

    settings = IOPMCopySystemPowerSettings();
    if(!settings) {
        goto exit;
    }

    // Disable Sleep?
    disable_sleep = (kCFBooleanTrue ==
                        CFDictionaryGetValue( settings, kIOPMSleepDisabledKey ));

    rootdomain = getRootDomain();
    IORegistryEntrySetCFProperty( rootdomain, kIOPMSleepDisabledKey,
                                       (disable_sleep ? kCFBooleanTrue : kCFBooleanFalse));

    bool    avoid_keyStore = false;
    CFNumberRef numRef;
    uint32_t value = 0xffff;

    // Disable FDE Key Store on SMC
    avoid_keyStore = (kCFBooleanTrue ==
                        CFDictionaryGetValue( settings, CFSTR(kIOPMDestroyFVKeyOnStandbyKey) ));
    IORegistryEntrySetCFProperty( rootdomain, CFSTR(kIOPMDestroyFVKeyOnStandbyKey),
                        (avoid_keyStore ? kCFBooleanTrue : kCFBooleanFalse));

    if (CFDictionaryGetValueIfPresent(settings, CFSTR(kIOPMDarkWakeLingerDurationKey), (const void **)&numRef)) {
        CFNumberGetValue(numRef, kCFNumberIntType, &value);
        setDwlInterval(value);
    }

exit:
    if(settings) CFRelease( settings );
    return kIOReturnSuccess;
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
    PMActivateSystemPowerSettings();

    // re-read preferences into memory
    if(energySettings) CFRelease(energySettings);

    energySettings = IOPMCopyPMPreferences();

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
 * on disk and saves them to disk
 */
void migrateSCPrefs()
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
        goto exit;
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

    IOPMSetPMPreferences(mergedPrefs);
exit:
    if (oldPrefs)   CFRelease(oldPrefs);
    if (mergedPrefs) CFRelease(mergedPrefs);

    return;

}


/***********************************************************************/

