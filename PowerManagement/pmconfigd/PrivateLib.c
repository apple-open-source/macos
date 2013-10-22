/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOHibernatePrivate.h>
#if !TARGET_OS_EMBEDDED
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#endif
#include <Security/SecTask.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>
#include <asl.h>
#include <membership.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <unistd.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <notify.h>

//#include "Platform.h"
#include "PrivateLib.h"
#include "BatteryTimeRemaining.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMAssertions.h"

#define kIntegerStringLen               15

#if !TARGET_OS_EMBEDDED
#include <IOKit/smc/SMCUserClient.h>
#include <systemstats/systemstats.h>
#endif /* TARGET_OS_EMBEDDED */

#ifndef kIOHIDIdleTimeKy
#define kIOHIDIdleTimeKey                               "HIDIdleTime"
#endif

#ifndef kIOPMMaintenanceScheduleImmediate
#define kIOPMMaintenanceScheduleImmediate               "MaintenanceImmediate"
#endif

enum
{
    PowerManagerScheduledShutdown = 1,
    PowerManagerScheduledSleep,
    PowerManagerScheduledRestart
};

/* If the battery doesn't specify an alternative time, we wait 16 seconds
   of ignoring the battery's (or our own) time remaining estimate.
*/
enum
{
    kInvalidWakeSecsDefault = 16
};

enum
{
    // 2GB
    kStandbyDesktopHibernateFileSize = 2ULL*1024*1024*1024,
    // 1GB
    kStandbyPortableHibernateFileSize = 1ULL*1024*1024*1024
};

#define kPowerManagerActionNotificationName "com.apple.powermanager.action"
#define kPowerManagerActionKey "action"
#define kPowerManagerValueKey "value"

// Tracks system battery state
CFMutableSetRef             _publishedBatteryKeysSet = NULL;
// Track real batteries
static CFMutableSetRef     physicalBatteriesSet = NULL;
static int                 physicalBatteriesCount = 0;
static IOPMBattery         **physicalBatteriesArray = NULL;

#ifndef __I_AM_PMSET__
// Track simulated debug batteries
extern int                  _showWhichBatteries;
static CFMutableSetRef     simulatedBatteriesSet = NULL;
static int                 simulatedBatteriesCount = 0;
static IOPMBattery         **simulatedBatteriesArray = NULL;
#endif

// Frequency with which to write out FDR records, in secs
#define kFDRRegularInterval (10*60)
// How long to wait after a power event to write out first FDR record, in secs
// Power events include sleep, wake, AC change etc.
#define kFDRIntervalAfterPE (1*60)

#if !TARGET_OS_EMBEDDED
#ifndef __I_AM_PMSET__
static uint64_t nextFDREventDue = 0;
#endif
#endif

static CFStringRef     gSleepReasonString = CFSTR("");
static CFStringRef     gWakeReasonString = CFSTR("");
static CFStringRef     gWakeTypeString = CFSTR("");

/******
 * Do not remove DUMMY macros
 *
 * The following DUMMY macros aren't used in the source code, they're
 * a placeholder for the 'genstrings' tool to read the strings we're using.
 * 'genstrings' will encode these strings in a Localizable.strings file.
 * Note: if you add to or modify these strings, you must re-run genstrings and
 * replace this project's Localizable.strings file with the output.
 ******/

#define DUMMY_UPS_HEADER(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("WARNING!"), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("Warning!"), \
            NULL);

#define DUMMY_UPS_BODY(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("YOUR COMPUTER IS NOW RUNNING ON UPS BACKUP BATTERY. SAVE YOUR DOCUMENTS AND SHUTDOWN SOON."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("Your computer is now running on UPS backup battery power. Save your documents and shut down soon."), \
            NULL);


#define DUMMY_ASSERTION_STRING_TTY(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("A remote user is connected. That prevents system sleep."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("A remote user is connected. That prevents system sleep."), \
            NULL);

#define DUMMY_CAFFEINATE_REASON_STRING(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("THE CAFFEINATE TOOL IS PREVENTING SLEEP."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("The caffeinate tool is preventing sleep."), \
            NULL);


static int getAggressivenessFactorsFromProfile(
                                               CFDictionaryRef                 p,
                                               IOPMAggressivenessFactors       *agg);
static int ProcessHibernateSettings(
                                    CFDictionaryRef                 dict,
                                    bool                            standby,
                                    bool                            desktop,
                                    io_registry_entry_t             rootDomain);


#ifndef __I_AM_PMSET__
// dynamicStoreNotifyCallBack is defined in pmconfigd.c
// is not defined in pmset! so we don't compile this code in pmset.

extern SCDynamicStoreRef                gSCDynamicStore;

__private_extern__ SCDynamicStoreRef _getSharedPMDynamicStore(void)
{
    return gSCDynamicStore;
}
#endif

__private_extern__ CFRunLoopRef         _getPMRunLoop(void)
{
    static CFRunLoopRef     pmRLS = NULL;

    if (!pmRLS) {
        pmRLS = CFRunLoopGetCurrent();
    }

    return pmRLS;
}

__private_extern__ dispatch_queue_t     _getPMDispatchQueue(void)
{
    static dispatch_queue_t pmQ = NULL;

    if (!pmQ) {
        pmQ = dispatch_queue_create("Power Management configd queue", NULL);
    }

    return pmQ;
}

#ifndef __I_AM_PMSET__
#if !TARGET_OS_EMBEDDED
__private_extern__ bool auditTokenHasEntitlement(
                                     audit_token_t token,
                                     CFStringRef entitlement)
{
    SecTaskRef task = NULL;
    CFTypeRef val = NULL;
    bool caller_is_allowed = false;
    CFErrorRef      errorp = NULL;
    
    task = SecTaskCreateWithAuditToken(kCFAllocatorDefault, token);
    if (task) {
        val = SecTaskCopyValueForEntitlement(task, entitlement, &errorp);
        CFRelease(task);
        
        if (kCFBooleanTrue == val) {
            caller_is_allowed = true;
        }
        if (val) {
            CFRelease(val);
        }
    }
    return caller_is_allowed;
    
}
#endif
#endif

__private_extern__ io_registry_entry_t getRootDomain(void)
{
    static io_registry_entry_t gRoot = MACH_PORT_NULL;

    if (MACH_PORT_NULL == gRoot)
        gRoot = IORegistryEntryFromPath( kIOMasterPortDefault,
                kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    return gRoot;
}

__private_extern__ IOReturn
_setRootDomainProperty(
    CFStringRef                 key,
    CFTypeRef                   val)
{
    return IORegistryEntrySetCFProperty(getRootDomain(), key, val);
}

__private_extern__ CFTypeRef
_copyRootDomainProperty(
    CFStringRef                 key)
{
    return IORegistryEntryCreateCFProperty(getRootDomain(), key, kCFAllocatorDefault, 0);
}


__private_extern__ bool
_getUUIDString(
    char *buf,
    int buflen)
{
    bool            ret = false;
    CFStringRef     uuidString = NULL;

    uuidString = IOPMSleepWakeCopyUUID();

    if (uuidString) {
        if (!CFStringGetCString(uuidString, buf, buflen,
                                kCFStringEncodingUTF8))
        {
            goto exit;
        }

        ret = true;
    }
exit:
    if (uuidString) CFRelease(uuidString);
    return ret;
}

__private_extern__ CFStringRef _updateSleepReason( )
{
    io_service_t    iopm_rootdomain_ref = getRootDomain();

    if (gSleepReasonString) CFRelease(gSleepReasonString);

    gSleepReasonString = IORegistryEntryCreateCFProperty(
                            iopm_rootdomain_ref,
                            CFSTR("Last Sleep Reason"),
                            kCFAllocatorDefault, 0);

    if (!isA_CFString(gSleepReasonString))
        gSleepReasonString = CFSTR("");

    return gSleepReasonString;

}

__private_extern__ bool
_getSleepReason(
    char *buf,
    int buflen)
{
    bool            ret = false;
    io_service_t    iopm_rootdomain_ref = getRootDomain();
    CFNumberRef     sleepPID = NULL;
    char            reasonBuf[50];
    int             spid = -1;
    
    sleepPID = (CFNumberRef)IORegistryEntryCreateCFProperty(
                             iopm_rootdomain_ref,
                             CFSTR("SleepRequestedByPID"),
                             kCFAllocatorDefault, 0);
    if (sleepPID && isA_CFNumber(sleepPID)) {
        CFNumberGetValue(sleepPID, kCFNumberIntType, &spid);
    }
    if (gSleepReasonString && isA_CFString(gSleepReasonString))
    {
        if (CFStringGetCString(gSleepReasonString, reasonBuf, sizeof(reasonBuf), kCFStringEncodingUTF8))
        {
            if (!strncmp(kIOPMSoftwareSleepKey, reasonBuf, strlen(kIOPMSoftwareSleepKey)))
            {
                snprintf(buf, buflen, "%s pid=%d", reasonBuf, spid);
            } else {
                snprintf(buf, buflen, "%s", reasonBuf);
            }
            ret = true;
        }
    }
    if (sleepPID) CFRelease(sleepPID);
    return ret;
}

__private_extern__ void _resetWakeReason( )
{

    if (gWakeReasonString) CFRelease(gWakeReasonString);
    if (gWakeTypeString) CFRelease(gWakeTypeString);
    gWakeReasonString = CFSTR("");
    gWakeTypeString = CFSTR("");
}

__private_extern__ void _updateWakeReason
    (CFStringRef *wakeReason, CFStringRef *wakeType)
{
    io_service_t    iopm_rootdomain_ref = getRootDomain();

    if (gWakeReasonString) CFRelease(gWakeReasonString);
    if (gWakeTypeString) CFRelease(gWakeTypeString);

    // This property may not exist on all platforms.
    gWakeReasonString = IORegistryEntryCreateCFProperty( iopm_rootdomain_ref,
                              CFSTR(kIOPMRootDomainWakeReasonKey), kCFAllocatorDefault, 0);

    if (!isA_CFString(gWakeReasonString))
        gWakeReasonString = CFSTR("");

    gWakeTypeString = IORegistryEntryCreateCFProperty(iopm_rootdomain_ref,
                              CFSTR(kIOPMRootDomainWakeTypeKey), kCFAllocatorDefault, 0);

    if (!isA_CFString(gWakeTypeString))
        gWakeTypeString = CFSTR("");

    if (wakeReason) *wakeReason = gWakeReasonString;
    if (wakeType) *wakeType = gWakeTypeString;
    return ;


}

__private_extern__ void getCFWakeReason
    (CFStringRef *wakeReason, CFStringRef *wakeType)
{

    if (!isA_CFString(gWakeReasonString))
        gWakeReasonString = CFSTR("");

    if (!isA_CFString(gWakeTypeString))
        gWakeTypeString = CFSTR("");

    if (wakeReason) *wakeReason = gWakeReasonString;
    if (wakeType) *wakeType = gWakeTypeString;
    return ;

}
    


__private_extern__ bool
_getWakeReason(
    char *buf,
    int buflen)
{
    bool            ret = false;
    char            wr[25], wt[25];

    wr[0] = wt[0] = 0;

    if (gWakeReasonString) {
       if (CFStringGetCString(gWakeReasonString, wr, sizeof(wr),
                   kCFStringEncodingUTF8) && wr[0]) {
           ret = true;
        }
    }

    if (gWakeTypeString) {
       if (CFStringGetCString(gWakeTypeString, wt, sizeof(wt),
                   kCFStringEncodingUTF8) && wt[0]) {
           ret = true;
       }
    }

  snprintf(buf, buflen, "%s/%s", wr, wt);
    return ret;
}

__private_extern__ bool
_getHibernateState(
    uint32_t *hibernateState)
{
    bool            ret = false;
    io_service_t    rootDomain = getRootDomain();
    CFDataRef       hibStateData = NULL;
    uint32_t        *hibStatePtr;

    // This property may not exist on all platforms.
    hibStateData = IORegistryEntryCreateCFProperty(
                        rootDomain,
                        CFSTR(kIOHibernateStateKey),
                        kCFAllocatorDefault, 0);

    if (isA_CFData(hibStateData) &&
        (CFDataGetLength(hibStateData) == sizeof(uint32_t)) &&
        (hibStatePtr = (uint32_t *)CFDataGetBytePtr(hibStateData)))
    {
        *hibernateState = *hibStatePtr;
        ret = true;
    }

    if (hibStateData) CFRelease(hibStateData);
    return ret;
}

__private_extern__
const char * getSleepTypeString(void)
{
    const char      *string = NULL;
#if !TARGET_OS_EMBEDDED
    io_service_t    rootDomain = getRootDomain();
    bool            isHibWake = false;
    CFNumberRef     sleepTypeNum;
    uint32_t        hibState;

    sleepTypeNum = IORegistryEntryCreateCFProperty(
                        rootDomain,
                        CFSTR(kIOPMSystemSleepTypeKey),
                        kCFAllocatorDefault, 0);

    if (_getHibernateState(&hibState) &&
        (hibState & kIOHibernateStateWakingFromHibernate))
    {
        isHibWake = true;
    }

    if (isA_CFNumber(sleepTypeNum))
    {
        int sleepType = kIOPMSleepTypeInvalid;

        CFNumberGetValue(sleepTypeNum, kCFNumberIntType, &sleepType);
        if (isHibWake)
        {
            // Hibernation types
            switch (sleepType)
            {
                case kIOPMSleepTypeSafeSleep:
                    string = "Safe Sleep";
                    break;
                case kIOPMSleepTypeHibernate:
                    string = "Hibernate";
                    break;
                case kIOPMSleepTypeStandby:
                    string = "Standby";
                    break;
                case kIOPMSleepTypePowerOff:
                    string = "AutoPowerOff";
                    break;
            }
        }
    }
    if (sleepTypeNum)
        CFRelease(sleepTypeNum);

#endif /* !TARGET_OS_EMBEDDED */
    return string;
}

__private_extern__
const char *stringForLWCode(uint8_t code)
{
    const char *string;
    switch (code)
    {
        default:
            string = "OK";
    }
    return string;
}

__private_extern__
const char *stringForPMCode(uint8_t code)
{
    const char *string = "";

    switch (code)
    {
        case kIOPMTracePointSystemUp:
            string = "On";
            break;
        case kIOPMTracePointSleepStarted:
            string = "SleepStarted";
            break;
        case kIOPMTracePointSleepApplications:
            string = "SleepApps";
            break;
        case kIOPMTracePointSleepPriorityClients:
            string = "SleepPriority";
            break;
        case kIOPMTracePointSleepWillChangeInterests:
            string = "SleepWillChangeInterests";
            break;
        case kIOPMTracePointSleepPowerPlaneDrivers:
            string = "SleepDrivers";
            break;
        case kIOPMTracePointSleepDidChangeInterests:
            string = "SleepDidChangeInterests";
            break;
        case kIOPMTracePointSleepCapabilityClients:
            string = "SleepCapabilityClients";
            break;
        case kIOPMTracePointSleepPlatformActions:
            string = "SleepPlatformActions";
            break;
        case kIOPMTracePointSleepCPUs:
            string = "SleepCPUs";
            break;
        case kIOPMTracePointSleepPlatformDriver:
            string = "SleepPlatformDriver";
            break;
        case kIOPMTracePointSystemSleep:
            string = "SleepPlatform";
            break;
        case kIOPMTracePointHibernate:
            string = "Hibernate";
            break;
        case kIOPMTracePointWakePlatformDriver:
            string = "WakePlatformDriver";
            break;
        case kIOPMTracePointWakePlatformActions:
            string = "WakePlatformActions";
            break;
        case kIOPMTracePointWakeCPUs:
            string = "WakeCPUs";
            break;
        case kIOPMTracePointWakeWillPowerOnClients:
            string = "WakeWillPowerOnClients";
            break;
        case kIOPMTracePointWakeWillChangeInterests:
            string = "WakeWillChangeInterests";
            break;
        case kIOPMTracePointWakeDidChangeInterests:
            string = "WakeDidChangeInterests";
            break;
        case kIOPMTracePointWakePowerPlaneDrivers:
            string = "WakeDrivers";
            break;
        case kIOPMTracePointWakeCapabilityClients:
            string = "WakeCapabilityClients";
            break;
        case kIOPMTracePointWakeApplications:
            string = "WakeApps";
            break;
        case kIOPMTracePointSystemLoginwindowPhase:
            string = "WakeLoginWindow";
            break;
        case kIOPMTracePointDarkWakeEntry:
            string = "DarkWakeEntry";
            break;
        case kIOPMTracePointDarkWakeExit:
            string = "DarkWakeExit";
            break;
    }
    return string;
}


static void sendNotification(int command)
{
#if !TARGET_OS_EMBEDDED
    CFMutableDictionaryRef   dict = NULL;
    int numberOfSeconds = 600;

    CFNumberRef secondsValue = CFNumberCreate( NULL, kCFNumberIntType, &numberOfSeconds );
    CFNumberRef commandValue = CFNumberCreate( NULL, kCFNumberIntType, &command );

    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2,
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(dict, CFSTR(kPowerManagerActionKey), commandValue);
    CFDictionarySetValue(dict, CFSTR(kPowerManagerValueKey), secondsValue);

    CFNotificationCenterPostNotificationWithOptions (
                        CFNotificationCenterGetDistributedCenter(),
                                            CFSTR(kPowerManagerActionNotificationName),
                                            NULL, dict,
                                            (kCFNotificationPostToAllSessions | kCFNotificationDeliverImmediately));
    CFRelease(dict);
    CFRelease(secondsValue);
    CFRelease(commandValue);
#endif
}


__private_extern__ void _askNicelyThenShutdownSystem(void)
{
    sendNotification( PowerManagerScheduledShutdown );
}

__private_extern__ void _askNicelyThenSleepSystem(void)
{
    sendNotification( PowerManagerScheduledSleep );
}

__private_extern__ void _askNicelyThenRestartSystem(void)
{
    sendNotification( PowerManagerScheduledRestart );
}

__private_extern__ CFAbsoluteTime _CFAbsoluteTimeFromPMEventTimeStamp(uint64_t kernelPackedTime)
{
    uint32_t    cal_sec = (uint32_t)(kernelPackedTime >> 32);
    uint32_t    cal_micro = (uint32_t)(kernelPackedTime & 0xFFFFFFFF);
    CFAbsoluteTime timeKernelEpoch = (CFAbsoluteTime)(double)cal_sec + (double)cal_micro/1000.0;

    // Adjust from kernel 1970 epoch to CF 2001 epoch
    timeKernelEpoch -= kCFAbsoluteTimeIntervalSince1970;

    return timeKernelEpoch;
}

#ifndef __I_AM_PMSET__
static bool                     _platformBackgroundTaskSupport = false;
static bool                     _platformSleepServiceSupport = false;
#endif


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

#ifndef __I_AM_PMSET__
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
            configAssertionType(kBackgroundTaskIndex, false);
            mt2EvaluateSystemSupport();
        }
    }
#endif

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
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepEnabledKey), kCFNumberSInt32Type, &agg->fDeepSleepEnable);
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepDelayKey), kCFNumberSInt32Type, &agg->fDeepSleepDelay);
    getAggressivenessValue(p, CFSTR(kIOPMAutoPowerOffEnabledKey), kCFNumberSInt32Type, &agg->fAutoPowerOffEnable);
    getAggressivenessValue(p, CFSTR(kIOPMAutoPowerOffDelayKey), kCFNumberSInt32Type, &agg->fAutoPowerOffDelay);

    return 0;
}

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

#ifndef __I_AM_PMSET__
    evalAllUserActivityAssertions(theFactors.fMinutesToDim);
    evalAllNetworkAccessAssertions();
#endif

    return kIOReturnSuccess;
}



static void _unpackBatteryState(IOPMBattery *b, CFDictionaryRef prop)
{
    CFBooleanRef    boo;
    CFNumberRef     n;

    if(!isA_CFDictionary(prop)) return;

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalConnectedKey));
    b->externalConnected = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalChargeCapableKey));
    b->externalChargeCapable = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryInstalledKey));
    b->isPresent = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSIsChargingKey));
    b->isCharging = (kCFBooleanTrue == boo);

    b->failureDetected = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSErrorConditionKey));

    b->batterySerialNumber = (CFStringRef)CFDictionaryGetValue(prop, CFSTR("BatterySerialNumber"));

    b->chargeStatus = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryChargeStatusKey));

    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSVoltageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->voltage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCurrentCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->currentCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSDesignCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->designCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSTimeRemainingKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->hwAverageTR);
    }


    n = CFDictionaryGetValue(prop, CFSTR("InstantAmperage"));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->instantAmperage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSAmperageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->avgAmperage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxErrKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxerr);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCycleCountKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->cycleCount);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSLocationKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->location);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSInvalidWakeSecondsKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->invalidWakeSecs);
    } else {
        b->invalidWakeSecs = kInvalidWakeSecsDefault;
    }
    n = CFDictionaryGetValue(prop, CFSTR("PermanentFailureStatus"));
    if (n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->pfStatus);
    } else {
        b->pfStatus = 0;
    }

    return;
}

/*
 * _batteries
 */
__private_extern__ IOPMBattery **_batteries(void)
{
#ifndef __I_AM_PMSET__
    if (kBatteryShowFake == _showWhichBatteries)
        return simulatedBatteriesArray;
    else
#endif
        return physicalBatteriesArray;
}

/*
 * _batteryCount
 */
__private_extern__ int  _batteryCount(void)
{
#ifndef __I_AM_PMSET__
    if (kBatteryShowFake == _showWhichBatteries)
        return simulatedBatteriesCount;
    else
#endif
        return physicalBatteriesCount;
}

__private_extern__ IOPMBattery *_newBatteryFound(io_registry_entry_t where)
{
    IOPMBattery *new_battery = NULL;
    static int new_battery_index = 0;
    // Populate new battery in array
    new_battery = calloc(1, sizeof(IOPMBattery));
    new_battery->me = where;
    new_battery->name = CFStringCreateWithFormat(
                            kCFAllocatorDefault,
                            NULL,
                            CFSTR("InternalBattery-%d"),
                            new_battery_index);
    new_battery->dynamicStoreKey = SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault,
                            CFSTR("%@%@/InternalBattery-%d"),
                            kSCDynamicStoreDomainState,
                            CFSTR(kIOPSDynamicStorePath),
                            new_battery_index);

    if (new_battery->dynamicStoreKey) {
        if (!_publishedBatteryKeysSet) {
            _publishedBatteryKeysSet = CFSetCreateMutable(0, 1, &kCFTypeSetCallBacks);
        }
        if (_publishedBatteryKeysSet) {
            CFSetAddValue(_publishedBatteryKeysSet, new_battery->dynamicStoreKey);
        }
    }

    new_battery_index++;
    _batteryChanged(new_battery);
    // Check whether new_battery is a software simulated battery,
    // or a real physical battery.
    if (new_battery->properties
        && CFDictionaryGetValue(new_battery->properties, CFSTR("AppleSoftwareSimulatedBattery")))
    {
        /* Software simulated battery. Not a real battery. */
#ifndef __I_AM_PMSET__
        if (!simulatedBatteriesSet) {
            simulatedBatteriesSet = CFSetCreateMutable(0, 1, NULL);
        }
        CFSetAddValue(simulatedBatteriesSet, new_battery);
        simulatedBatteriesCount = CFSetGetCount(simulatedBatteriesSet);
        if (simulatedBatteriesArray) {
            free(simulatedBatteriesArray);
            simulatedBatteriesArray = NULL;
        }
        simulatedBatteriesArray = (IOPMBattery **)calloc(simulatedBatteriesCount, sizeof(IOPMBattery *));
        CFSetGetValues(simulatedBatteriesSet, (const void **)simulatedBatteriesArray);
#endif
    } else {
        /* Real, physical battery found */
        if (!physicalBatteriesSet) {
            physicalBatteriesSet = CFSetCreateMutable(0, 1, NULL);
        }
        CFSetAddValue(physicalBatteriesSet, new_battery);
        physicalBatteriesCount = CFSetGetCount(physicalBatteriesSet);
        if (physicalBatteriesArray) {
            free(physicalBatteriesArray);
            physicalBatteriesArray = NULL;
        }
        physicalBatteriesArray = (IOPMBattery **)calloc(physicalBatteriesCount, sizeof(IOPMBattery *));
        CFSetGetValues(physicalBatteriesSet, (const void **)physicalBatteriesArray);
    }

    notify_post(kIOPSNotifyAttach);
    
    return new_battery;
}


__private_extern__ void _batteryChanged(IOPMBattery *changed_battery)
{
    kern_return_t       kr;

    if(!changed_battery) {
        // This is unexpected; we're not tracking this battery
        return;
    }

    // Free the last set of properties
    if(changed_battery->properties) {
        CFRelease(changed_battery->properties);
        changed_battery->properties = NULL;
    }

    kr = IORegistryEntryCreateCFProperties(
                            changed_battery->me,
                            &(changed_battery->properties),
                            kCFAllocatorDefault, 0);
    if(KERN_SUCCESS != kr) {
        changed_battery->properties = NULL;
        goto exit;
    }

    _unpackBatteryState(changed_battery, changed_battery->properties);
exit:
    return;
}

__private_extern__ bool _batteryHas(IOPMBattery *b, CFStringRef property)
{
    if(!property || !b->properties) return false;

    // If the battery's descriptior dictionary has an entry at all for the
    // given 'property' it is supported, i.e. the battery 'has' it.
    return CFDictionaryGetValue(b->properties, property) ? true : false;
}


#if HAVE_CF_USER_NOTIFICATION

__private_extern__ CFUserNotificationRef _copyUPSWarning(void)
{
    CFMutableDictionaryRef      alert_dict;
    SInt32                      error;
    CFUserNotificationRef       note_ref;
    CFBundleRef                 myBundle;
    CFStringRef                 header_unlocalized;
    CFStringRef                 message_unlocalized;
    CFURLRef                    bundle_url;

    myBundle = CFBundleGetBundleWithIdentifier(kPowerdBundleIdentifier);
    if (!myBundle)
        return NULL;

    alert_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!alert_dict)
        return NULL;

    bundle_url = CFBundleCopyBundleURL(myBundle);
    CFDictionarySetValue(alert_dict, kCFUserNotificationLocalizationURLKey, bundle_url);
    CFRelease(bundle_url);

    header_unlocalized = CFSTR("WARNING!");
    message_unlocalized = CFSTR("YOUR COMPUTER IS NOW RUNNING ON UPS BACKUP BATTERY. SAVE YOUR DOCUMENTS AND SHUTDOWN SOON.");

    CFDictionaryAddValue(alert_dict, kCFUserNotificationAlertHeaderKey, header_unlocalized);
    CFDictionaryAddValue(alert_dict, kCFUserNotificationAlertMessageKey, message_unlocalized);

    note_ref = CFUserNotificationCreate(kCFAllocatorDefault, 0, 0, &error, alert_dict);
    CFRelease(alert_dict);

    asl_log(0, 0, ASL_LEVEL_ERR, "PowerManagement: UPS low power warning\n");

    return note_ref;
}

#endif

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*      AAAAAAAAAAAAAA      SSSSSSSSSSSSSS       LLLLLLLLLLLLLLLL          */
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

static bool powerString(char *powerBuf, int bufSize)
{
    IOPMBattery                                 **batteries;
    int                                         batteryCount = 0;
    uint32_t                                    capPercent = 0;
    int                                         i;
    batteryCount = _batteryCount();
    if (0 < batteryCount) {
        batteries = _batteries();
        for (i=0; i< batteryCount; i++) {
            if (batteries[i]->isPresent
                && (0 != batteries[i]->maxCap)) {
                capPercent += (batteries[i]->currentCap * 100) / batteries[i]->maxCap;
            }
        }
        snprintf(powerBuf, bufSize, "%s \(Charge:%d%%)",
               batteries[0]->externalConnected ? "Using AC":"Using BATT", capPercent);
        return true;
    } else {
        snprintf(powerBuf, bufSize, "Using AC");
        return false;
    }
}

static void printCapabilitiesToBuf(char *buf, int buf_size, IOPMCapabilityBits in_caps)
{
    uint64_t caps = (uint64_t)in_caps;

//    snprintf(buf, buf_size, "%s:%s%s%s%s%s%s%s",
//                             on_sleep_dark,
    snprintf(buf, buf_size, " [%s%s%s%s%s%s%s]",
                             (caps & kIOPMCapabilityCPU) ? "C":"<off> ",
                             (caps & kIOPMCapabilityDisk) ? "D":"",
                             (caps & kIOPMCapabilityNetwork) ? "N":"",
                             (caps & kIOPMCapabilityVideo) ? "V":"",
                             (caps & kIOPMCapabilityAudio) ? "A":"",
                             (caps & kIOPMCapabilityPushServiceTask) ? "P":"",
                             (caps & kIOPMCapabilityBackgroundTask) ? "B":"");
}


__private_extern__ aslmsg new_msg_pmset_log(void)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);

    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    asl_set(m, ASL_KEY_FACILITY, kPMFacility);

    return m;
}


__private_extern__ void logASLMessagePMStart(void)
{
    aslmsg                  m;
    char                    uuidString[150];

    m = new_msg_pmset_log();

    if (_getUUIDString(uuidString, sizeof(uuidString))) {
        asl_set(m, kPMASLUUIDKey, uuidString);
    }
    asl_set(m, kPMASLDomainKey, kPMASLDomainPMStart);
    asl_set(m, ASL_KEY_MSG, "powerd process is started\n");
    asl_send(NULL, m);
    asl_free(m);
}

#if TCPKEEPALIVE
#ifndef __I_AM_PMSET__

static void attachTCPKeepAliveKeys(
                                   aslmsg m,
                                   char *tcpString,
                                   unsigned int tcpStringLen)

{
    CFTypeRef           platformSupport = NULL;
    char                keepAliveString[100];
    
    IOPlatformCopyFeatureDefault(kIOPlatformTCPKeepAliveDuringSleep, &platformSupport);
    if (kCFBooleanTrue == platformSupport)
    {
        asl_set(m, kPMASLTCPKeepAlive, "supported");
        
        getTCPKeepAliveIsActive(keepAliveString, sizeof(keepAliveString));

        asl_set(m, kPMASLTCPKeepAliveExpired, keepAliveString);
        snprintf(tcpString, tcpStringLen, "TCPKeepAlive=%s", keepAliveString);
    }

    if (platformSupport){
        CFRelease(platformSupport);
    }
}

#else

static void attachTCPKeepAliveKeys(
                                   aslmsg m __unused,
                                   char *tcpString __unused,
                                   unsigned int tcpStringLen __unused)
{
    return;
}

#endif
#endif

__private_extern__ void logASLMessageSleep(
    const char *sig,
    const char *uuidStr,
    const char *failureStr,
    int   sleepType
)
{
    static int              sleepCyclesCount = 0;
    aslmsg                  m;
    char                    uuidString[150];
    char                    powerLevelBuf[50];
    char                    numbuf[15];
    bool                    success = true;
    char                    messageString[200];
    char                    detailString[100];
    char                    tcpKeepAliveString[50];

    m = new_msg_pmset_log();

    bzero(messageString, sizeof(messageString));
    bzero(detailString, sizeof(detailString));
    bzero(tcpKeepAliveString, sizeof(tcpKeepAliveString));
    
    if(!_getSleepReason(messageString, sizeof(messageString))) {
        messageString[0] = '\0';
    }
    
    if (!strncmp(sig, kPMASLSigSuccess, sizeof(kPMASLSigSuccess)))
    {
        success = true;
        if (sleepType == kIsS0Sleep) {
            if (0 == strlen(messageString)) {
                snprintf(detailString, sizeof(detailString), " Sleep");
            }
#if !TARGET_OS_EMBEDDED && TCPKEEPALIVE
            attachTCPKeepAliveKeys(m, tcpKeepAliveString, sizeof(tcpKeepAliveString));
#endif
        } else {
           snprintf(detailString, sizeof(detailString), " to DarkWake");
        }
        
    } else {
        success = false;
        snprintf(detailString, sizeof(detailString), " Sleep (Failure code:%s)",
                failureStr);
    }

    if (success) {
        // Value == Sleep Cycles Count
        // Note: unknown on the failure case, so we won't publish the sleep count
        // unless sig == success
        snprintf(numbuf, 10, "%d", ++sleepCyclesCount);
        asl_set(m, kPMASLValueKey, numbuf);
    }

    // UUID
    if (uuidStr) {
        asl_set(m, kPMASLUUIDKey, uuidStr);  // Caller Provided
    } else if (_getUUIDString(uuidString, sizeof(uuidString))) {
        asl_set(m, kPMASLUUIDKey, uuidString);
    }
    
    powerString(powerLevelBuf, sizeof(powerLevelBuf));

    snprintf(messageString, sizeof(messageString), "%s%s: %s %s\n",
            messageString,  detailString, powerLevelBuf, tcpKeepAliveString);

    asl_set(m, kPMASLDomainKey, kPMASLDomainPMSleep);
    asl_set(m, kPMASLSignatureKey, sig);
    asl_set(m, ASL_KEY_MSG, messageString);
    asl_send(NULL, m);
    asl_free(m);
}

/*****************************************************************************/
#pragma mark ASL

__private_extern__ void logASLMessageWake(
    const char *sig,
    const char *uuidStr,
    const char *failureStr,
    IOPMCapabilityBits in_capabilities,
    WakeTypeEnum dark_wake
)
{
    aslmsg                  m;
    char                    buf[200];
    char                    powerLevelBuf[50];
    char                    wakeReasonBuf[50];
    char                    cBuf[50];
    const char *            detailString = NULL;
    static int              darkWakeCnt = 0;
    char                    numbuf[15];
    static char             prev_uuid[50];
    CFStringRef             wakeType = NULL;
    const char *            sleepTypeString;

    m = new_msg_pmset_log();

    asl_set(m, kPMASLSignatureKey, sig);
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
        if (strncmp(buf, prev_uuid, sizeof(prev_uuid))) {
              // New sleep/wake cycle.
              snprintf(prev_uuid, sizeof(prev_uuid), "%s", buf);
              darkWakeCnt = 0;
        }
    }

    buf[0] = 0;
    if (!strncmp(sig, kPMASLSigSuccess, sizeof(kPMASLSigSuccess)))
    {
        if (_getWakeReason(wakeReasonBuf, sizeof(wakeReasonBuf)))
            detailString = wakeReasonBuf;
    } else {
        detailString = failureStr;

        snprintf(buf, sizeof(buf), "%s during wake", sig);
    }

    powerString(powerLevelBuf, sizeof(powerLevelBuf));

    if (dark_wake == kIsDarkWake)
    {
       asl_set(m, kPMASLDomainKey, kPMASLDomainPMDarkWake);
       snprintf(buf, sizeof(buf), "%s", "DarkWake");
       darkWakeCnt++;
       snprintf(numbuf, sizeof(numbuf), "%d", darkWakeCnt);
       asl_set(m, kPMASLValueKey, numbuf);
    }
    else if (dark_wake == kIsDarkToFullWake)
    {
       wakeType = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeTypeKey));
       if (isA_CFString(wakeType)) {
           CFStringGetCString(wakeType, wakeReasonBuf, sizeof(wakeReasonBuf), kCFStringEncodingUTF8);
           CFRelease(wakeType);
       }
       asl_set(m, kPMASLDomainKey, kPMASLDomainPMWake);
       snprintf(buf, sizeof(buf), "%s", "DarkWake to FullWake");
    }
    else
    {
       asl_set(m, kPMASLDomainKey, kPMASLDomainPMWake);
       snprintf(buf, sizeof(buf), "%s", "Wake");
    }

    if ((sleepTypeString = getSleepTypeString()))
    {
        snprintf(buf, sizeof(buf), "%s from %s", buf, sleepTypeString);
    }

    printCapabilitiesToBuf(cBuf, sizeof(cBuf), in_capabilities);
    strncat(buf, cBuf, sizeof(buf)-strlen(buf)-1);

    snprintf(buf, sizeof(buf), "%s %s %s: %s\n", buf,
          detailString ? "due to" : "",
          detailString ? detailString : "",
          powerLevelBuf);

    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_free(m);

    logASLMessageHibernateStatistics( );
}


/*****************************************************************************/

__private_extern__ void logASLMessageHibernateStatistics(void)
{
    aslmsg                  m;
    CFDataRef               statsData = NULL;
    PMStatsStruct           *stats = NULL;
    uint64_t                readHIBImageMS = 0;
    uint64_t                writeHIBImageMS = 0;
    CFNumberRef             hibernateModeNum = NULL;
    CFNumberRef             hibernateDelayNum = NULL;
    int                     hibernateMode = 0;
    char                    valuestring[25];
    int                     hibernateDelay = 0;
    char                    buf[100];
    char                    uuidString[150];

    hibernateModeNum = (CFNumberRef)_copyRootDomainProperty(CFSTR(kIOHibernateModeKey));
    if (!hibernateModeNum)
        goto exit;
    CFNumberGetValue(hibernateModeNum, kCFNumberIntType, &hibernateMode);
    CFRelease(hibernateModeNum);

    hibernateDelayNum= (CFNumberRef)_copyRootDomainProperty(CFSTR(kIOPMDeepSleepDelayKey));
    if (!hibernateDelayNum)
        goto exit;
    CFNumberGetValue(hibernateDelayNum, kCFNumberIntType, &hibernateDelay);
    CFRelease(hibernateDelayNum);

    statsData = (CFDataRef)_copyRootDomainProperty(CFSTR(kIOPMSleepStatisticsKey));
    if (!statsData || !(stats = (PMStatsStruct *)CFDataGetBytePtr(statsData)))
    {
        goto exit;
    } else {
        writeHIBImageMS = (stats->hibWrite.stop - stats->hibWrite.start)/1000000UL;

        readHIBImageMS =(stats->hibRead.stop - stats->hibRead.start)/1000000UL;

        /* Hibernate image is not generated on every sleep for some h/w */
        if ( !writeHIBImageMS && !readHIBImageMS)
            goto exit;
    }

    m = new_msg_pmset_log();

    asl_set(m, kPMASLDomainKey, kPMASLDomainHibernateStatistics);

    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);

    if (_getUUIDString(uuidString, sizeof(uuidString))) {
        asl_set(m, kPMASLUUIDKey, uuidString);
    }

    snprintf(valuestring, sizeof(valuestring), "hibernatemode=%d", hibernateMode);
    asl_set(m, kPMASLSignatureKey, valuestring);
    // If readHibImageMS == zero, that means we woke from the contents of memory
    // and did not read the hibernate image.
    if (writeHIBImageMS)
        snprintf(buf, sizeof(buf), "wr=%qd ms ", writeHIBImageMS);

    if (readHIBImageMS)
        snprintf(buf, sizeof(buf), "rd=%qd ms", readHIBImageMS);
    asl_set(m, kPMASLDelayKey, buf);

    snprintf(buf, sizeof(buf), "hibmode=%d standbydelay=%d", hibernateMode, hibernateDelay);

    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_free(m);
exit:
    if(statsData)
        CFRelease(statsData);
    return;
}

/*****************************************************************************/

__private_extern__ void logASLPMConnectionNotify(
    CFStringRef     appNameString,
    int             notificationBits
    )
{

    aslmsg m;
    char buf[128];
    char appName[100];


    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainAppNotify);


    if (!CFStringGetCString(appNameString, appName, sizeof(appName), kCFStringEncodingUTF8))
       snprintf(appName, sizeof(appName), "Unknown app");


    asl_set(m, kPMASLSignatureKey, appName);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

   snprintf(buf, sizeof(buf), "Notification sent to %s (powercaps:0x%x)",
         appName,notificationBits );

    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_free(m);
}

__private_extern__ void logASLMessagePMConnectionResponse(
    CFStringRef     logSourceString,
    CFStringRef     appNameString,
    CFStringRef     responseTypeString,
    CFNumberRef     responseTime,
    int             notificationBits
)
{
    aslmsg                  m;
    char                    appName[128];
    char                    *appNamePtr = NULL;
    int                     time = 0;
    char                    buf[128];
    bool                    fromKernel = false;
    char                    qualifier[30];
    bool                    timeout = false;

    // String identifying the source of the log is required.
    if (!logSourceString)
        return;
    if (CFEqual(logSourceString, kAppResponseLogSourceKernel))
        fromKernel = true;

    m = new_msg_pmset_log();

    if (responseTypeString && CFEqual(responseTypeString, CFSTR(kIOPMStatsResponseTimedOut)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseTimedOut);
        snprintf(qualifier, sizeof(qualifier), "timed out");
        timeout = true;
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kIOPMStatsResponseCancel)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseCancel);
        snprintf(qualifier, sizeof(qualifier), "is to cancel state change");
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kIOPMStatsResponseSlow)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseSlow);
        snprintf(qualifier, sizeof(qualifier), "is slow");
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kPMASLDomainSleepServiceCapApp)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainSleepServiceCapApp);
        snprintf(qualifier, sizeof(qualifier), "exceeded SleepService cap");
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kPMASLDomainAppResponse)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseReceived);
        snprintf(qualifier, sizeof(qualifier), "received");
    } else {
        asl_free(m);
        return;
    }

    // Message = Failing process name
    if (appNameString)
    {
        if (CFStringGetCString(appNameString, appName, sizeof(appName), kCFStringEncodingUTF8))
        {
                appNamePtr = &appName[0];
        }
    }
    if (!appNamePtr) {
        appNamePtr = "AppNameUnknown";
    }

    asl_set(m, kPMASLSignatureKey, appNamePtr);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

    // Value == Time
    if (responseTime) {
        if (CFNumberGetValue(responseTime, kCFNumberIntType, &time)) {
            snprintf(buf, sizeof(buf), "%d", time);
            asl_set(m, kPMASLValueKey, buf);
        }
    }

    if (CFStringGetCString(logSourceString, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        snprintf(buf, sizeof(buf), "%s: Response from %s %s",
              buf, appNamePtr, qualifier);
    } else {
        snprintf(buf, sizeof(buf), "Response from %s %s",
              appNamePtr, qualifier);
    }

    if (notificationBits != -1)
       snprintf(buf, sizeof(buf), "%s (powercaps:0x%x)", buf, notificationBits);

    asl_set(m, ASL_KEY_MSG, buf);

    if (time != 0) {
       snprintf(buf, sizeof(buf), "%d ms", time);
       asl_set(m, kPMASLDelayKey, buf);
    }

    asl_send(NULL, m);
    asl_free(m);

#ifndef __I_AM_PMSET__
    if (timeout) {
        mt2RecordAppTimeouts(gSleepReasonString, appNameString);
    }
#endif
}

/*****************************************************************************/

/* logASLMessageIORegisterForSystemPowerResponses
 *
 * Logs one ASL message for each errant application notification received.
 *
 */
__private_extern__ void logASLMessageIORegisterForSystemPowerResponses(void)
{
    CFArrayRef              appFailuresArray = NULL;
    CFDictionaryRef         *appFailures = NULL;
    CFStringRef             appNameString = NULL;
    CFNumberRef             timeNum = NULL;
    CFNumberRef             powerCaps_cf = NULL;
    CFStringRef             responseTypeString = NULL;
    int                     appFailuresCount = 0;
    int                     i = 0;
    int                     powerCaps = -1;

    appFailuresArray = (CFArrayRef)_copyRootDomainProperty(CFSTR(kIOPMSleepStatisticsAppsKey));

    if (!appFailuresArray
        || (0 == (appFailuresCount = CFArrayGetCount(appFailuresArray))))
    {
        goto exit;
    }

    appFailures = (CFDictionaryRef *)calloc(appFailuresCount, sizeof(CFDictionaryRef));

    CFArrayGetValues(appFailuresArray, CFRangeMake(0, appFailuresCount),
                        (const void **)appFailures);

    for (i=0; i<appFailuresCount; i++)
    {
        if (!isA_CFDictionary(appFailures[i])) {
            continue;
        }

        appNameString       = CFDictionaryGetValue(appFailures[i], CFSTR(kIOPMStatsNameKey));
        timeNum             = CFDictionaryGetValue(appFailures[i], CFSTR(kIOPMStatsTimeMSKey));
        responseTypeString  = CFDictionaryGetValue(appFailures[i], CFSTR(kIOPMStatsApplicationResponseTypeKey));
        powerCaps_cf        = CFDictionaryGetValue(appFailures[i], CFSTR(kIOPMStatsPowerCapabilityKey));

        if (powerCaps_cf)
            CFNumberGetValue(powerCaps_cf, kCFNumberIntType, &powerCaps);

        logASLMessagePMConnectionResponse(
            kAppResponseLogSourceKernel,
            appNameString,
            responseTypeString,
            timeNum, powerCaps);

    }

exit:
    if (appFailuresArray)
        CFRelease(appFailuresArray);

    if (appFailures)
        free(appFailures);
}


__private_extern__ void logASLMessagePMConnectionScheduledWakeEvents(CFStringRef requestedMaintenancesString)
{
    aslmsg                  m;
    char                    buf[100];
    char                    requestors[500];
    CFMutableStringRef      messageString = NULL;

    messageString = CFStringCreateMutable(0, 0);
    if (!messageString)
        return;

    m = new_msg_pmset_log();

    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

    CFStringAppendCString(messageString, "Clients requested wake events: ", kCFStringEncodingUTF8);
    if (requestedMaintenancesString && (0 < CFStringGetLength(requestedMaintenancesString))) {
        CFStringAppend(messageString, requestedMaintenancesString);
    } else {
        CFStringAppend(messageString, CFSTR("None"));
    }

    CFStringGetCString(messageString, requestors, sizeof(requestors), kCFStringEncodingUTF8);

    asl_set(m, kPMASLDomainKey, kPMASLDomainPMWakeRequests);
    asl_set(m, ASL_KEY_MSG, requestors);
    asl_send(NULL, m);
    asl_free(m);
    CFRelease(messageString);

}

__private_extern__ void logASLMessageExecutedWakeupEvent(CFStringRef requestedMaintenancesString)
{
    aslmsg                  m;
    char                    buf[100];
    char                    requestors[500];
    CFMutableStringRef      messageString = CFStringCreateMutable(0, 0);

    if (!messageString)
        return;

    m = new_msg_pmset_log();

    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

    CFStringAppendCString(messageString, "PM scheduled RTC wake event: ", kCFStringEncodingUTF8);
    CFStringAppend(messageString, requestedMaintenancesString);

    CFStringGetCString(messageString, requestors, sizeof(requestors), kCFStringEncodingUTF8);

    asl_set(m, kPMASLDomainKey, kPMASLDomainPMWakeRequests);
    asl_set(m, ASL_KEY_MSG, requestors);
    asl_send(NULL, m);
    asl_free(m);
    CFRelease(messageString);
}

#if !TARGET_OS_EMBEDDED
__private_extern__ void logASLMessageIgnoredDWTEmergency(void)
{
    aslmsg      m;
    char        strbuf[125];
    char        tcpKeepAliveString[50];

    bzero(strbuf, sizeof(strbuf));
    bzero(tcpKeepAliveString, sizeof(tcpKeepAliveString));

    m = new_msg_pmset_log();
#if TCPKEEPALIVE
    attachTCPKeepAliveKeys(m, tcpKeepAliveString, sizeof(tcpKeepAliveString));
#endif
    
    asl_set(m, kPMASLDomainKey, kPMASLDomainDWTEmergency);

    snprintf(
        strbuf,
        sizeof(strbuf),
        "Ignored DarkWake thermal emergency signal %s", tcpKeepAliveString);
    asl_set(m, ASL_KEY_MSG, strbuf);

    asl_send(NULL, m);
    asl_free(m);
}
#endif

__private_extern__ void logASLMessageSleepCanceledAtLastCall(void)
{
    aslmsg      m;
    char        strbuf[125];
    char        tcpKeepAliveString[50];

    bzero(strbuf, sizeof(strbuf));
    bzero(tcpKeepAliveString, sizeof(tcpKeepAliveString));

    m = new_msg_pmset_log();

#if TCPKEEPALIVE
    attachTCPKeepAliveKeys(m, tcpKeepAliveString, sizeof(tcpKeepAliveString));
#endif

    asl_set(m, kPMASLDomainKey, kPMASLDomainSleepRevert);

    snprintf(
        strbuf,
        sizeof(strbuf),
        "Sleep in process aborted due to power assertion %s", tcpKeepAliveString);
    asl_set(m, ASL_KEY_MSG, strbuf);

    asl_send(NULL, m);
    asl_free(m);
}

/*****************************************************************************/
/*****************************************************************************/


/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
#pragma mark MT2 DarkWake
#ifndef __I_AM_PMSET__

#if TARGET_OS_EMBEDDED
/* These are stubs of MT2 functions, so we can build for embedded, without this functionality. */
void initializeMT2Aggregator(void) {};
void mt2DarkWakeEnded(void) {};
void mt2EvaluateSystemSupport(void) {};
void mt2RecordWakeEvent(uint32_t description) {};
void mt2RecordThermalEvent(uint32_t description) {};
void mt2RecordAssertionEvent(assertionOps action, assertion_t *theAssertion) {};
void mt2PublishReports(void) {};
void mt2PublishSleepFailure(const char *failType, const char *pci_string) {};
void mt2PublishWakeFailure(const char *failType, const char *pci_string) {};
void mt2RecordAppTimeouts(CFStringRef sleepReason, CFStringRef procName) {};
#else

/*
 * MessageTracer2 DarkWake Keys
 */

typedef struct {
    CFAbsoluteTime              startedPeriod;
    dispatch_source_t           nextFireSource;

    /* for domain com.apple.darkwake.capable */
    int                         SMCSupport:1;
    int                         PlatformSupport:1;
    int                         checkedforAC:1;
    int                         checkedforBatt:1;
    /* for domain com.apple.darkwake.wakes */
    uint16_t                    wakeEvents[kWakeStateCount];
    /* for domain com.apple.darkwake.thermal */
    uint16_t                    thermalEvents[kThermalStateCount];
    /* for domain com.apple.darkwake.backgroundtasks */
    CFMutableSetRef             alreadyRecordedBackground;
    CFMutableDictionaryRef      tookBackground;
    /* for domain com.apple.darkwake.pushservicetasks */
    CFMutableSetRef             alreadyRecordedPush;
    CFMutableDictionaryRef      tookPush;
    /* for domain com.apple.darkwake.pushservicetasktimeout */
    CFMutableSetRef             alreadyRecordedPushTimeouts;
    CFMutableDictionaryRef      timeoutPush;
    CFMutableDictionaryRef      idleSleepAppTimeouts;
    CFMutableDictionaryRef      demandSleepAppTimeouts;
    CFMutableDictionaryRef      darkwakeSleepAppTimeouts;
} MT2Aggregator;

static const uint64_t   kMT2CheckIntervalTimer = 4ULL*60ULL*60ULL*NSEC_PER_SEC;     /* Check every 4 hours */
static CFAbsoluteTime   kMT2SendReportsAtInterval = 7.0*24.0*60.0*60.0;             /* Publish reports every 7 days */
static const uint64_t   kBigLeeway = (30Ull * NSEC_PER_SEC);

static MT2Aggregator    *mt2 = NULL;

void initializeMT2Aggregator(void)
{
    if (mt2)
    {
        /* Zero out & recycle MT2Aggregator structure */
        if (mt2->nextFireSource) {
            dispatch_release(mt2->nextFireSource);
        }
        CFRelease(mt2->alreadyRecordedBackground);
        CFRelease(mt2->tookBackground);
        CFRelease(mt2->alreadyRecordedPush);
        CFRelease(mt2->tookPush);
        CFRelease(mt2->alreadyRecordedPushTimeouts);
        CFRelease(mt2->timeoutPush);
        CFRelease(mt2->idleSleepAppTimeouts);
        CFRelease(mt2->demandSleepAppTimeouts);
        CFRelease(mt2->darkwakeSleepAppTimeouts);

        bzero(mt2, sizeof(MT2Aggregator));
    } else {
        /* New datastructure */
        mt2 = calloc(1, sizeof(MT2Aggregator));
    }
    mt2->startedPeriod                      = CFAbsoluteTimeGetCurrent();
    mt2->alreadyRecordedBackground          = CFSetCreateMutable(0, 0, &kCFTypeSetCallBacks);
    mt2->alreadyRecordedPush                = CFSetCreateMutable(0, 0, &kCFTypeSetCallBacks);
    mt2->alreadyRecordedPushTimeouts        = CFSetCreateMutable(0, 0, &kCFTypeSetCallBacks);
    mt2->tookBackground                     = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->tookPush                           = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->timeoutPush                        = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->idleSleepAppTimeouts               = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->demandSleepAppTimeouts             = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->darkwakeSleepAppTimeouts           = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);

    mt2->nextFireSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    if (mt2->nextFireSource) {
        dispatch_source_set_event_handler(mt2->nextFireSource, ^(){ mt2PublishReports(); });
        dispatch_source_set_timer(mt2->nextFireSource, dispatch_time(DISPATCH_TIME_NOW, kMT2CheckIntervalTimer),
                                  kMT2CheckIntervalTimer, kBigLeeway);
        dispatch_resume(mt2->nextFireSource);
    }
    mt2EvaluateSystemSupport();
    return;
}



static int mt2PublishDomainCapable(void)
{
#define kMT2DomainDarkWakeCapable       "com.apple.darkwake.capable"
#define kMT2KeySupport                  "com.apple.message.hardware_support"
#define kMT2ValNoSupport                "none"
#define kMT2ValPlatformSupport          "platform_without_smc"
#define kMT2ValSMCSupport               "smc_without_platform"
#define kMT2ValFullDWSupport            "dark_wake_supported"
#define kMT2KeySettings                 "com.apple.message.settings"
#define kMT2ValSettingsNone             "none"
#define kMT2ValSettingsAC               "ac"
#define kMT2ValSettingsBatt             "battery"
#define kMT2ValSettingsACPlusBatt       "ac_and_battery"
    if (!mt2) {
        return 0;
    }

    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainDarkWakeCapable );
    if (mt2->SMCSupport && mt2->PlatformSupport) {
        asl_set(m, kMT2KeySupport, kMT2ValFullDWSupport);
    } else if (mt2->PlatformSupport) {
        asl_set(m, kMT2KeySupport, kMT2ValPlatformSupport);
    } else if (mt2->SMCSupport) {
        asl_set(m, kMT2KeySupport, kMT2ValSMCSupport);
    } else {
        asl_set(m, kMT2KeySupport, kMT2ValNoSupport);
    }

    if (mt2->checkedforAC && mt2->checkedforBatt) {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsACPlusBatt);
    } else if (mt2->checkedforAC) {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsAC);
    } else if (mt2->checkedforBatt) {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsBatt);
    } else {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsNone);
    }

    asl_log(NULL, m, ASL_LEVEL_ERR, "");
    asl_free(m);

    return 1;
}


static int mt2PublishDomainWakes(void)
{
#define kMT2DomainWakes                 "com.apple.darkwake.wakes"
#define kMT2KeyWakeType                 "com.apple.message.waketype"
#define kMT2ValWakeDark                 "dark"
#define kMT2ValWakeFull                 "full"
#define kMT2KeyPowerSource              "com.apple.message.powersource"
#define kMT2ValPowerAC                  "ac"
#define kMT2ValPowerBatt                "battery"
#define kMT2KeyLid                      "com.apple.message.lid"
#define kMT2ValLidOpen                  "open"
#define kMT2ValLidClosed                "closed"

    int     sentCount = 0;
    int     i = 0;
    char    buf[kIntegerStringLen];

    if (!mt2) {
        return 0;
    }
    for (i=0; i<kWakeStateCount; i++)
    {
        if (0 == mt2->wakeEvents[i]) {
            continue;
        }
        aslmsg m = asl_new(ASL_TYPE_MSG);
        asl_set(m, "com.apple.message.domain", kMT2DomainWakes);
        if (i & kWakeStateDark) {
            asl_set(m, kMT2KeyWakeType, kMT2ValWakeDark);
        } else {
            asl_set(m, kMT2KeyWakeType, kMT2ValWakeFull);
        }
        if (i & kWakeStateBattery) {
            asl_set(m, kMT2KeyPowerSource, kMT2ValPowerBatt);
        } else {
            asl_set(m, kMT2KeyPowerSource, kMT2ValPowerAC);
        }
        if (i & kWakeStateLidClosed) {
            asl_set(m, kMT2KeyLid, kMT2ValLidClosed);
        } else {
            asl_set(m, kMT2KeyLid, kMT2ValLidOpen);
        }

        snprintf(buf, sizeof(buf), "%d", mt2->wakeEvents[i]);
        asl_set(m, "com.apple.message.count", buf);
        asl_log(NULL, m, ASL_LEVEL_ERR, "");
        asl_free(m);
        sentCount++;
    }
    return sentCount;
}

static int mt2PublishDomainThermals(void)
{
#define kMT2DomainThermal               "com.apple.darkwake.thermalevent"
#define kMT2KeySleepRequest             "com.apple.message.sleeprequest"
#define kMT2KeyFansSpin                 "com.apple.message.fansspin"
#define kMT2ValTrue                     "true"
#define kMT2ValFalse                    "false"

    int     sentCount = 0;
    int     i = 0;
    char    buf[kIntegerStringLen];

    if (!mt2) {
        return 0;
    }

    for (i=0; i<kThermalStateCount; i++)
    {
        if (0 == mt2->thermalEvents[i]) {
            continue;
        }
        aslmsg m = asl_new(ASL_TYPE_MSG);
        asl_set(m, "com.apple.message.domain", kMT2DomainThermal );
        if (i & kThermalStateSleepRequest) {
            asl_set(m, kMT2KeySleepRequest, kMT2ValTrue);
        } else {
            asl_set(m, kMT2KeySleepRequest, kMT2ValFalse);
        }
        if (i & kThermalStateFansOn) {
            asl_set(m, kMT2KeyFansSpin, kMT2ValTrue);
        } else {
            asl_set(m, kMT2KeyFansSpin, kMT2ValFalse);
        }

        snprintf(buf, sizeof(buf), "%d", mt2->thermalEvents[i]);
        asl_set(m, "com.apple.message.count", buf);
        asl_log(NULL, m, ASL_LEVEL_ERR, "");
        asl_free(m);
        sentCount++;
    }
    return sentCount;
}

static int mt2PublishDomainProcess(const char *appdomain, CFDictionaryRef apps)
{
#define kMT2KeyApp                      "com.apple.message.process"

    CFStringRef         *keys;
    uintptr_t           *counts;
    char                buf[2*kProcNameBufLen];
    int                 sendCount = 0;
    int                 appcount = 0;
    int                 i = 0;

    if (!mt2 || !apps || (0 == (appcount = CFDictionaryGetCount(apps))))
    {
        return 0;
    }

    keys = (CFStringRef *)calloc(sizeof(CFStringRef), appcount);
    counts = (uintptr_t *)calloc(sizeof(uintptr_t), appcount);

    CFDictionaryGetKeysAndValues(apps, (const void **)keys, (const void **)counts);

    for (i=0; i<appcount; i++)
    {
        if (0 == counts[i]) {
            continue;
        }
        aslmsg m = asl_new(ASL_TYPE_MSG);
        asl_set(m, "com.apple.message.domain", appdomain);

        if (!CFStringGetCString(keys[i], buf, sizeof(buf), kCFStringEncodingUTF8)) {
            snprintf(buf, sizeof(buf), "com.apple.message.%s", "Unknown");
        }
        asl_set(m, kMT2KeyApp, buf);

        snprintf(buf, sizeof(buf), "%d", (int)counts[i]);
        asl_set(m, "com.apple.message.count", buf);

        asl_log(NULL, m, ASL_LEVEL_ERR,"");
        asl_free(m);
        sendCount++;

    }

    free(keys);
    free(counts);

    return sendCount;
}

void mt2PublishReports(void)
{
#define kMT2DomainPushTasks         "com.apple.darkwake.pushservicetasks"
#define kMT2DomainPushTimeouts      "com.apple.darkwake.pushservicetimeouts"
#define kMT2DomainBackgroundTasks   "com.apple.darkwake.backgroundtasks"
#define kMT2DomainIdleSlpAckTo      "com.apple.ackto.idlesleep"    /* Idle sleep ack timeouts */
#define kMT2DomainDemandSlpAckTo    "com.apple.ackto.demandsleep"  /* Demand sleep ack timeouts */
#define kMT2DomainDarkWkSlpAckTo    "com.apple.ackto.demandsleep"  /* Dark wake sleep ack timeouts */

    if (!mt2) {
        return;
    }

    if ((mt2->startedPeriod + kMT2SendReportsAtInterval) < CFAbsoluteTimeGetCurrent())
    {
        /* mt2PublishReports should only publish a new batch of ASL no more
         * frequently than once every kMT2SendReportsAtInterval seconds.
         * If it's too soon to publish ASL keys, just return.
         */
        return;
    }

    mt2PublishDomainCapable();

    if (mt2->PlatformSupport && mt2->SMCSupport)
    {
        mt2PublishDomainWakes();
        mt2PublishDomainThermals();
        mt2PublishDomainProcess(kMT2DomainPushTasks, mt2->tookPush);
        mt2PublishDomainProcess(kMT2DomainPushTimeouts, mt2->timeoutPush);
        mt2PublishDomainProcess(kMT2DomainBackgroundTasks, mt2->tookBackground);
        mt2PublishDomainProcess(kMT2DomainIdleSlpAckTo, mt2->idleSleepAppTimeouts);
        mt2PublishDomainProcess(kMT2DomainDemandSlpAckTo, mt2->demandSleepAppTimeouts);
        mt2PublishDomainProcess(kMT2DomainDarkWkSlpAckTo, mt2->darkwakeSleepAppTimeouts);

        // Recyle the data structure for the next reporting.
        initializeMT2Aggregator();
    }

    // If the system lacks (PlatformSupport && SMC Support), this is where we stop scheduling MT2 reports.
    // If the system has PowerNap support, then we'll set a periodic timer in initializeMT2Aggregator() and
    // we'll keep publish messages on a schedule.

    return;
}

void mt2DarkWakeEnded(void)
{
    if (!mt2) {
        return;
    }
    CFSetRemoveAllValues(mt2->alreadyRecordedBackground);
    CFSetRemoveAllValues(mt2->alreadyRecordedPush);
    CFSetRemoveAllValues(mt2->alreadyRecordedPushTimeouts);
}

void mt2EvaluateSystemSupport(void)
{
    CFDictionaryRef     energySettings = NULL;
    CFDictionaryRef     per = NULL;
    CFNumberRef         num = NULL;
    int                 value = 0;

    if (!mt2) {
        return;
    }

    mt2->SMCSupport = smcSilentRunningSupport() ? 1:0;
    mt2->PlatformSupport = (_platformBackgroundTaskSupport || _platformSleepServiceSupport) ? 1:0;

    mt2->checkedforAC = 0;
    mt2->checkedforBatt = 0;
    if ((energySettings = IOPMCopyActivePMPreferences())) {
        per = CFDictionaryGetValue(energySettings, CFSTR(kIOPMACPowerKey));
        if (per) {
            num = CFDictionaryGetValue(per, CFSTR(kIOPMDarkWakeBackgroundTaskKey));
            if (num) {
                CFNumberGetValue(num, kCFNumberIntType, &value);
                mt2->checkedforAC = value ? 1:0;
            }
        }
        per = CFDictionaryGetValue(energySettings, CFSTR(kIOPMBatteryPowerKey));
        if (per) {
            num = CFDictionaryGetValue(per, CFSTR(kIOPMDarkWakeBackgroundTaskKey));
            if (num) {
                CFNumberGetValue(num, kCFNumberIntType, &value);
                mt2->checkedforBatt = value ? 1:0;
            }
        }

        CFRelease(energySettings);
    }
    return;
}

void mt2RecordWakeEvent(uint32_t description)
{
    CFStringRef     lidString = NULL; 
    CFBooleanRef    lidIsClosed = NULL;

    if (!mt2) {
        return;
    }

    if (kWakeStateFull & description) {
        /* The system just woke into FullWake.
         * To make sure that we publish mt2 reports in a timely manner,
         * we'll try now. It'll only actually happen if the PublishInterval has
         * elapsed since the last time we published.
         */
        mt2PublishReports();
    }

    lidString = CFStringCreateWithCString(0, kAppleClamshellStateKey, kCFStringEncodingUTF8);
    if (lidString) {
        lidIsClosed = _copyRootDomainProperty(lidString);
        CFRelease(lidString);
    }

    description |= ((_getPowerSource() == kBatteryPowered) ? kWakeStateBattery : kWakeStateAC)
                 | ((kCFBooleanTrue == lidIsClosed) ? kWakeStateLidClosed : kWakeStateLidOpen);

    if (lidIsClosed) {
        CFRelease(lidIsClosed);
    }

    mt2->wakeEvents[description]++;
    return;
}

void mt2RecordThermalEvent(uint32_t description)
{
    if (!mt2) {
        return;
    }
    description &= (kThermalStateFansOn | kThermalStateSleepRequest);
    mt2->thermalEvents[description]++;
    return;
}

/* PMConnection.c */
bool isA_DarkWakeState();

void mt2RecordAssertionEvent(assertionOps action, assertion_t *theAssertion)
{
    CFStringRef         processName;
    CFStringRef         assertionType;

    if (!mt2) {
        return;
    }

    if (!theAssertion || !theAssertion->props || !isA_DarkWakeState()) {
        return;
    }

    if (!(processName = processInfoGetName(theAssertion->pinfo->pid))) {
        processName = CFSTR("Unknown");
    }

    if (!(assertionType = CFDictionaryGetValue(theAssertion->props, kIOPMAssertionTypeKey))
        || (!CFEqual(assertionType, kIOPMAssertionTypeBackgroundTask)
         && !CFEqual(assertionType, kIOPMAssertionTypeApplePushServiceTask)))
    {
        return;
    }

    if (CFEqual(assertionType, kIOPMAssertionTypeBackgroundTask))
    {
        if (kAssertionOpRaise == action) {
            if (!CFSetContainsValue(mt2->alreadyRecordedBackground, processName)) {
                int x = (int)CFDictionaryGetValue(mt2->tookBackground, processName);
                x++;
                CFDictionarySetValue(mt2->tookBackground, processName, (uintptr_t)x);
                CFSetAddValue(mt2->alreadyRecordedBackground, processName);
            }
        }
    }
    else if (CFEqual(assertionType, kIOPMAssertionTypeApplePushServiceTask))
    {
        if (kAssertionOpRaise == action) {
            if (!CFSetContainsValue(mt2->alreadyRecordedPush, processName)) {
                int x = (int)CFDictionaryGetValue(mt2->tookPush, processName);
                x++;
                CFDictionarySetValue(mt2->tookPush, processName, (uintptr_t)x);
                CFSetAddValue(mt2->alreadyRecordedPush, processName);
            }
        }
        else if (kAssertionOpGlobalTimeout == action) {
            if (!CFSetContainsValue(mt2->alreadyRecordedPushTimeouts, processName)) {
                int x = (int)CFDictionaryGetValue(mt2->timeoutPush, processName);
                x++;
                CFDictionarySetValue(mt2->timeoutPush, (const void *)processName, (uintptr_t)x);
                CFSetAddValue(mt2->alreadyRecordedPushTimeouts, processName);
            }
        }
    }

    return;
}

void mt2RecordAppTimeouts(CFStringRef sleepReason, CFStringRef procName)
{
    CFMutableDictionaryRef dict;

    if (!isA_CFString(procName)) return;

    if (CFStringCompare(sleepReason, CFSTR(kIOPMIdleSleepKey), 0) == kCFCompareEqualTo) {
        dict = mt2->idleSleepAppTimeouts;
    }
    else  if ((CFStringCompare(sleepReason, CFSTR(kIOPMClamshellSleepKey), 0) == kCFCompareEqualTo) ||
            (CFStringCompare(sleepReason, CFSTR(kIOPMPowerButtonSleepKey), 0) == kCFCompareEqualTo) ||
            (CFStringCompare(sleepReason, CFSTR(kIOPMSoftwareSleepKey), 0) == kCFCompareEqualTo)) {
        dict = mt2->demandSleepAppTimeouts;
    }
    else {
        dict = mt2->darkwakeSleepAppTimeouts;
    }

    int x = (int)CFDictionaryGetValue(dict, procName);
    x++;
    CFDictionarySetValue(dict, (const void *)procName, (uintptr_t)x);

}

#define kMT2DomainSleepFailure      "com.apple.sleep.failure"
#define kMT2DomainWakeFailure       "com.apple.wake.failure"
#define kMT2KeyFailType             "com.apple.message.signature"
#define kMT2KeyPCI                  "com.apple.message.signature2"
void mt2PublishSleepFailure(const char *failType, const char *pci_string)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainSleepFailure);
    asl_set(m, kMT2KeyFailType, failType);
    asl_set(m, kMT2KeyPCI, pci_string);
    asl_set(m, "com.apple.message.summarize", "YES");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
    asl_free(m);
}

void mt2PublishWakeFailure(const char *failType, const char *pci_string)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainWakeFailure);
    asl_set(m, kMT2KeyFailType, failType);
    asl_set(m, kMT2KeyPCI, pci_string);
    asl_set(m, "com.apple.message.summarize", "YES");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
    asl_free(m);
}

#endif      /* #endif for iOS */
#endif      /* #endif pmset */

#pragma mark FDR

/*************************
  FDR functionality
 *************************/
void recordFDREvent(int eventType, bool checkStandbyStatus,  IOPMBattery **batteries)
{
#if !TARGET_OS_EMBEDDED
#ifndef __I_AM_PMSET__

    struct systemstats_sleep_s s;
    struct systemstats_wake_s w;
    struct systemstats_power_state_change_s psc;
    IOPMBattery *b;
    char wt[50];
    wt[0] = 0;

    switch(eventType) {
        case kFDRInit:
            systemstats_init(SYSTEMSTATS_WRITER_powerd, NULL);
            break;

        case kFDRACChanged:
            if(!batteries)
                return;

            b = batteries[0];

            bzero(&psc, sizeof(struct systemstats_power_state_change_s));
            psc.now_on_battery = !(b->externalConnected);
            psc.is_fully_charged = isFullyCharged(b);
            systemstats_write_power_state_change(&psc);
            nextFDREventDue = (uint64_t)kFDRIntervalAfterPE + getMonotonicTime();
            break;

        case kFDRSleepEvent:
            bzero(&s, sizeof(struct systemstats_sleep_s));
            s.reason = 0;
            systemstats_write_sleep(&s);
            nextFDREventDue = (uint64_t)kFDRRegularInterval + getMonotonicTime();
            break;

        case kFDRUserWakeEvent:
            bzero(&w, sizeof(struct systemstats_wake_s));
            w.reason = 1;
#if SYSTEMSTATS_API_VERSION >= 20130705
            if(checkStandbyStatus) {
                const char *sleepTypeString = getSleepTypeString();
                if(sleepTypeString &&
                    (!strncmp(sleepTypeString, "Standby", 15) ||
                     !strncmp(sleepTypeString, "AutoPowerOff", 15))) {
                    w.wake_from_standby = true;
                }
            }

            if (gWakeTypeString) {
                if (CFStringGetCString(gWakeTypeString, wt, sizeof(wt),
                    kCFStringEncodingUTF8) && wt[0]) {
                    w.wake_type = wt;
                }
            }
#endif
            systemstats_write_wake(&w);
            nextFDREventDue = (uint64_t)kFDRIntervalAfterPE + getMonotonicTime();
            break;

        case kFDRDarkWakeEvent:
            bzero(&w, sizeof(struct systemstats_wake_s));
            w.reason = 2;
#if SYSTEMSTATS_API_VERSION >= 20130705
            if(checkStandbyStatus) {
                const char *sleepTypeString = getSleepTypeString();
                if(sleepTypeString &&
                    (!strncmp(sleepTypeString, "Standby", 15) ||
                     !strncmp(sleepTypeString, "AutoPowerOff", 15))) {
                    w.wake_from_standby = true;
                }
            }

            if (gWakeTypeString) {
                if (CFStringGetCString(gWakeTypeString, wt, sizeof(wt),
                    kCFStringEncodingUTF8) && wt[0]) {
                    w.wake_type = wt;
                }
            }
#endif
            systemstats_write_wake(&w);
            nextFDREventDue = (uint64_t)kFDRIntervalAfterPE + getMonotonicTime();
            break;

        case kFDRBattEventPeriodic:
            // If last FDR event was < X mins ago, do nothing
            if(nextFDREventDue && getMonotonicTime() <= nextFDREventDue)  {
                break;
            }

            // Fall thru
        case kFDRBattEventAsync:
            if(!batteries)
                return;

            IOPMBattery *b = batteries[0];
            struct systemstats_battery_charge_level_s binfo;
            bzero(&binfo, sizeof(struct systemstats_battery_charge_level_s));

            binfo.charge = b->currentCap;
            binfo.max_charge = b->maxCap;
            binfo.cycle_count = b->cycleCount;
            binfo.instant_amperage = b->instantAmperage;
            binfo.instant_voltage = b->voltage;
            binfo.last_minute_wattage = ((abs(b->voltage))*(abs(b->avgAmperage)))/1000;
            binfo.estimated_time_remaining = b->hwAverageTR;
            binfo.smoothed_estimated_time_remaining = b->swCalculatedTR;
            binfo.is_fully_charged = isFullyCharged(b);

            systemstats_write_battery_charge_level(&binfo);
            nextFDREventDue = (uint64_t)kFDRRegularInterval + getMonotonicTime();
            break;

        default:
            return;
    }

#endif
#endif
}

#pragma mark SMC and Hardware
/************************* One off hack for AppleSMC
 *************************
 ************************* Send AppleSMC a kCFPropertyTrue
 ************************* on time discontinuities.
 *************************/
#if !TARGET_OS_EMBEDDED

static void setSMCProperty(void)
{
    static io_registry_entry_t     _smc = MACH_PORT_NULL;

    if(MACH_PORT_NULL == _smc) {
        _smc = IOServiceGetMatchingService( MACH_PORT_NULL,
                        IOServiceMatching("AppleSMCFamily"));
    }

    if(!_smc) {
        return;
    }

    // And simply AppleSMC with kCFBooleanTrue to let them know time is changed.
    // We don't pass any information down.
    IORegistryEntrySetCFProperty( _smc,
                        CFSTR("TheTimesAreAChangin"),
                        kCFBooleanTrue);
}

static void handleMachCalendarMessage(CFMachPortRef port, void *msg,
                                            CFIndex size, void *info)
{
    kern_return_t  result;
    mach_port_t    mport = CFMachPortGetPort(port);
    mach_port_t    host_port;

    // Re-register for notification
    host_port = mach_host_self();
    result = host_request_notification(host_port, HOST_NOTIFY_CALENDAR_CHANGE, mport);
    if (host_port) {
        mach_port_deallocate(mach_task_self(), host_port);
    }
    if (result != KERN_SUCCESS) {
        return;
    }

    setSMCProperty();
}

static void registerForCalendarChangedNotification(void)
{
    mach_port_t tport;
    mach_port_t host_port;
    kern_return_t result;
    CFRunLoopSourceRef rls;
    static CFMachPortRef        calChangeReceivePort = NULL;

    // allocate the mach port we'll be listening to
    result = mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE, &tport);
    if (result != KERN_SUCCESS) {
        return;
    }

    calChangeReceivePort = CFMachPortCreateWithPort(
            kCFAllocatorDefault,
            tport,
            (CFMachPortCallBack)handleMachCalendarMessage,
            NULL, /* context */
            false); /* shouldFreeInfo */
    if (calChangeReceivePort) {
        rls = CFMachPortCreateRunLoopSource(
                kCFAllocatorDefault,
                calChangeReceivePort,
                0); /* index Order */
        if (rls) {
            CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
            CFRelease(rls);
        }
        CFRelease(calChangeReceivePort);
    }

    // register for notification
    host_port = mach_host_self();
    host_request_notification(host_port,HOST_NOTIFY_CALENDAR_CHANGE, tport);
    if (host_port) {
        mach_port_deallocate(mach_task_self(), host_port);
    }
}
#endif

/* Returns monotonic time in secs */
__private_extern__ uint64_t getMonotonicTime( )
{
    static mach_timebase_info_data_t    timebaseInfo;

    if (timebaseInfo.denom == 0)
        mach_timebase_info(&timebaseInfo);

    return ( (mach_absolute_time( ) * timebaseInfo.numer) / (timebaseInfo.denom * NSEC_PER_SEC));
}

__private_extern__ int callerIsRoot(int uid)
{
    return (0 == uid);
}

__private_extern__ int
callerIsAdmin(
    int uid,
    int gid
)
{
    int         ngroups = NGROUPS_MAX+1;
    int         group_list[NGROUPS_MAX+1];
    int         i;
    struct group    *adminGroup;
    struct passwd   *pw;


    pw = getpwuid(uid);
    if (!pw)
        return false;

    getgrouplist(pw->pw_name, pw->pw_gid, group_list, &ngroups);

    adminGroup = getgrnam("admin");
    if (adminGroup != NULL) {
        gid_t    adminGid = adminGroup->gr_gid;
        for(i=0; i<ngroups; i++)
        {
            if (group_list[i] == adminGid) {
                return TRUE;    // if a member of group "admin"
            }
        }
    }
    return false;

}

__private_extern__ int
callerIsConsole(
    int uid,
    int gid)
{
#if TARGET_OS_EMBEDDED
    return false;
#else
    CFStringRef                 user_name = NULL;
    uid_t                       console_uid;
    gid_t                       console_gid;

    user_name = (CFStringRef)SCDynamicStoreCopyConsoleUser(NULL,
                                    &console_uid, &console_gid);

    if(user_name) {
        CFRelease(user_name);
        return ((uid == console_uid) && (gid == console_gid));
    } else {
        // no data returned re: console user's uid or gid; return "false"
        return false;
    }
#endif /* !TARGET_OS_EMBEDDED */
}


void _oneOffHacksSetup(void)
{
#if !TARGET_OS_EMBEDDED
    registerForCalendarChangedNotification();
#endif
}

static const CFTimeInterval kTimeNSPerSec = 1000000000.0;

CFTimeInterval _getHIDIdleTime(void)
{
    static io_registry_entry_t hidsys = IO_OBJECT_NULL;
    CFNumberRef     hidsys_idlenum = NULL;
    CFTimeInterval  ret_time = 0.0;
    uint64_t        idle_nanos = 0;

    if (IO_OBJECT_NULL == hidsys) {
        hidsys = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOHIDSystem"));
    }
    if (!hidsys)
        goto exit;

    hidsys_idlenum = IORegistryEntryCreateCFProperty(hidsys, CFSTR(kIOHIDIdleTimeKey), 0, 0);

    if (!isA_CFNumber(hidsys_idlenum))
        goto exit;

    if (CFNumberGetValue(hidsys_idlenum, kCFNumberSInt64Type, &idle_nanos))
    {
        ret_time = ((CFTimeInterval)idle_nanos)/kTimeNSPerSec;
    }

exit:
    if (hidsys_idlenum)
        CFRelease(hidsys_idlenum);
    return ret_time;
}

/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/

// Code to read AppleSMC

/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
// Forwards
#if !TARGET_OS_EMBEDDED
static IOReturn _smcWriteKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t outBufMax);
static IOReturn _smcReadKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t *outBufMax);

#endif

/************************************************************************/
__private_extern__ IOReturn _getACAdapterInfo(
    uint64_t *val)
{
#if !TARGET_OS_EMBEDDED
    uint8_t readKeyLen = 8;
    return _smcReadKey('ACID', (void *)val, &readKeyLen);
#else
    return kIOReturnNotReadable;
#endif
}
/************************************************************************/
__private_extern__ PowerSources _getPowerSource(void)
{
#if !TARGET_OS_EMBEDDED
   IOPMBattery      **batteries;

   if (_batteryCount() && (batteries = _batteries())
            && (!batteries[0]->externalConnected) )
      return kBatteryPowered;
   else
      return kACPowered;
#else
    return kBatteryPowered;
#endif
}


#if !TARGET_OS_EMBEDDED
/************************************************************************/
__private_extern__ IOReturn _smcWakeTimerPrimer(void)
{
    uint8_t  buf[2];

    buf[0] = 0;
    buf[1] = 1;
    return _smcWriteKey('CLWK', buf, 2);
}

/************************************************************************/
__private_extern__ IOReturn _smcWakeTimerGetResults(uint16_t *mSec)
{
    uint8_t     size = 2;
    uint8_t     buf[2];
    IOReturn    ret;
    ret = _smcReadKey('CLWK', buf, &size);

    if (kIOReturnSuccess == ret) {
        *mSec = buf[0] | (buf[1] << 8);
    }

    return ret;
}

bool smcSilentRunningSupport(void)
{
    uint8_t     size = 1;
    uint8_t     buf[1];
    static IOReturn    ret = kIOReturnInvalid;

    if (ret != kIOReturnSuccess) {
       ret = _smcReadKey('WKTP', buf, &size);
    }

    if (kIOReturnSuccess == ret) {
        return true;
    }
    return false;
}



/************************************************************************/
/************************************************************************/

static IOReturn callSMCFunction(
    int which,
    SMCParamStruct *inputValues,
    SMCParamStruct *outputValues);

/************************************************************************/
// Methods
static IOReturn _smcWriteKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t outBufMax)
{
    SMCParamStruct  stuffMeIn;
    SMCParamStruct  stuffMeOut;
    IOReturn        ret;
    int             i;

    if (key == 0)
        return kIOReturnCannotWire;

    bzero(&stuffMeIn, sizeof(SMCParamStruct));
    bzero(&stuffMeOut, sizeof(SMCParamStruct));

    // Determine key's data size
    stuffMeIn.data8             = kSMCGetKeyInfo;
    stuffMeIn.key               = key;

    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);
    if (kIOReturnSuccess != ret) {
        goto exit;
    }

    if (stuffMeOut.result == kSMCKeyNotFound) {
        ret = kIOReturnNotFound;
        goto exit;
    } else if (stuffMeOut.result != kSMCSuccess) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    // Write Key
    stuffMeIn.data8             = kSMCWriteKey;
    stuffMeIn.key               = key;
    stuffMeIn.keyInfo.dataSize  = stuffMeOut.keyInfo.dataSize;
    if (outBuf) {
        if (outBufMax > 32) outBufMax = 32;
        for (i=0; i<outBufMax; i++) {
            stuffMeIn.bytes[i] = outBuf[i];
        }
    }
    bzero(&stuffMeOut, sizeof(SMCParamStruct));
    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);

    if (stuffMeOut.result != kSMCSuccess) {
        ret = kIOReturnInternalError;
        goto exit;
    }

exit:
    return ret;
}

static IOReturn _smcReadKey(
    uint32_t key,
    uint8_t *outBuf,
    uint8_t *outBufMax)
{
    SMCParamStruct  stuffMeIn;
    SMCParamStruct  stuffMeOut;
    IOReturn        ret;
    int             i;

    if (key == 0 || outBuf == NULL)
        return kIOReturnCannotWire;

    // Determine key's data size
    bzero(outBuf, *outBufMax);
    bzero(&stuffMeIn, sizeof(SMCParamStruct));
    bzero(&stuffMeOut, sizeof(SMCParamStruct));
    stuffMeIn.data8 = kSMCGetKeyInfo;
    stuffMeIn.key = key;

    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);
    if (kIOReturnSuccess != ret) {
        goto exit;
    }

    if (stuffMeOut.result == kSMCKeyNotFound) {
        ret = kIOReturnNotFound;
        goto exit;
    } else if (stuffMeOut.result != kSMCSuccess) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    // Get Key Value
    stuffMeIn.data8 = kSMCReadKey;
    stuffMeIn.key = key;
    stuffMeIn.keyInfo.dataSize = stuffMeOut.keyInfo.dataSize;
    bzero(&stuffMeOut, sizeof(SMCParamStruct));
    ret = callSMCFunction(kSMCHandleYPCEvent, &stuffMeIn, &stuffMeOut);
    if (stuffMeOut.result == kSMCKeyNotFound) {
        ret = kIOReturnNotFound;
        goto exit;
    } else if (stuffMeOut.result != kSMCSuccess) {
        ret = kIOReturnInternalError;
        goto exit;
    }

    if (*outBufMax > stuffMeIn.keyInfo.dataSize)
        *outBufMax = stuffMeIn.keyInfo.dataSize;

    // Byte-swap data returning from the SMC.
    // The data at key 'ACID' are not provided by the SMC and do
    // NOT need to be byte-swapped.
    for (i=0; i<*outBufMax; i++)
    {
        if ('ACID' == key)
        {
            // Do not byte swap
            outBuf[i] = stuffMeOut.bytes[i];
        } else {
            // Byte swap
            outBuf[i] = stuffMeOut.bytes[*outBufMax - (i + 1)];
        }
    }
exit:
    return ret;
}

static IOReturn callSMCFunction(
    int which,
    SMCParamStruct *inputValues,
    SMCParamStruct *outputValues)
{
    IOReturn result = kIOReturnError;

    size_t         inStructSize = sizeof(SMCParamStruct);
    size_t         outStructSize = sizeof(SMCParamStruct);

    io_connect_t    _SMCConnect = IO_OBJECT_NULL;
    io_service_t    smc = IO_OBJECT_NULL;

    smc = IOServiceGetMatchingService(
        kIOMasterPortDefault,
        IOServiceMatching("AppleSMC"));
    if (IO_OBJECT_NULL == smc) {
        return kIOReturnNotFound;
    }

    result = IOServiceOpen(smc, mach_task_self(), 1, &_SMCConnect);
    if (result != kIOReturnSuccess ||
        IO_OBJECT_NULL == _SMCConnect) {
        _SMCConnect = IO_OBJECT_NULL;
        goto exit;
    }

    result = IOConnectCallMethod(_SMCConnect, kSMCUserClientOpen,
                    NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
    if (result != kIOReturnSuccess) {
        goto exit;
    }

    result = IOConnectCallStructMethod(_SMCConnect, which,
                        inputValues, inStructSize,
                        outputValues, &outStructSize);

exit:
    if (IO_OBJECT_NULL != _SMCConnect) {
        IOConnectCallMethod(_SMCConnect, kSMCUserClientClose,
                    NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);
        IOServiceClose(_SMCConnect);
    }

    return result;
}

#else
bool smcSilentRunningSupport(void)
{
   return false;
}
#endif /* TARGET_OS_EMBEDDED */

/*****************************************************************************/
/*****************************************************************************/

static uint32_t gPMDebug = 0xFFFF;

__private_extern__ bool PMDebugEnabled(uint32_t which)
{
    return (gPMDebug & which);
}
__private_extern__ IOReturn getNvramArgInt(char *key, int *value)
{
    io_registry_entry_t optionsRef;
    IOReturn ret = kIOReturnError;
    CFDataRef   dataRef = NULL;
    int *dataPtr = NULL;
    kern_return_t       kr;
    CFMutableDictionaryRef dict = NULL;
    CFStringRef keyRef = NULL;


    optionsRef = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if (optionsRef == 0)
        return kIOReturnError;

    kr = IORegistryEntryCreateCFProperties(optionsRef, &dict, 0, 0);
    if (kr != KERN_SUCCESS)
        goto exit;

    keyRef = CFStringCreateWithCStringNoCopy(0, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (!isA_CFString(keyRef))
        goto exit;
    dataRef = CFDictionaryGetValue(dict, keyRef);

    if (!dataRef)
        goto exit;

    dataPtr = (int*)CFDataGetBytePtr(dataRef);
    *value = *dataPtr;

    ret = kIOReturnSuccess;

exit:
    if (keyRef) CFRelease(keyRef);
    if (dict) CFRelease(dict);
    IOObjectRelease(optionsRef);
    return ret;
}

/* extern symbol defined in IOKit.framework
 * IOCFURLAccess.c
 */
extern Boolean _IOReadBytesFromFile(CFAllocatorRef alloc, const char *path, void **bytes, CFIndex *length, CFIndex maxLength);

static int ProcessHibernateSettings(CFDictionaryRef dict, bool standby, bool isDesktop, io_registry_entry_t rootDomain)
{
    IOReturn    ret;
    CFTypeRef   obj;
    CFNumberRef modeNum;
    CFNumberRef num;
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
    off_t    minFileSize = 0;
    off_t    maxFileSize = 0;
    bool     apo_available = false;
    SInt32   apo_enabled = 0;
    CFNumberRef apo_enabled_cf = NULL;


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

    apo_available = IOPMFeatureIsAvailable(CFSTR(kIOPMAutoPowerOffEnabledKey), NULL);
    if (apo_available &&
            (apo_enabled_cf = CFDictionaryGetValue(dict, CFSTR(kIOPMAutoPowerOffEnabledKey ))) && 
             isA_CFNumber(apo_enabled_cf)) 
    {
        CFNumberGetValue(apo_enabled_cf, kCFNumberSInt32Type, &apo_enabled);
    }
    
    if ((modeValue || (apo_available && apo_enabled))
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

        if (0 != stat(path, &statBuf)) createFile = true;
        else
        {
            if ((S_IFBLK == (S_IFMT & statBuf.st_mode))
                || (S_IFCHR == (S_IFMT & statBuf.st_mode)))
            {
                haveFile = true;
            }
            else if (S_IFREG == (S_IFMT & statBuf.st_mode))
            {
                if ((statBuf.st_size == filesize) || (kIOHibernateModeFileResize & modeValue))
                    haveFile = true;
                else
                    createFile = true;
            }
            else
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

        if (!haveFile)
            break;

#if defined (__i386__) || defined(__x86_64__)
#define kBootXPath        "/System/Library/CoreServices/boot.efi"
#define kBootXSignaturePath    "/System/Library/Caches/com.apple.bootefisignature"
#else
#define kBootXPath        "/System/Library/CoreServices/BootX"
#define kBootXSignaturePath    "/System/Library/Caches/com.apple.bootxsignature"
#endif
#define kCachesPath        "/System/Library/Caches"
#define kGenSignatureCommand    "/bin/cat " kBootXPath " | /usr/bin/openssl dgst -sha1 -hex -out " kBootXSignaturePath


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

        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileKey), obj);
    }
    while (false);

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

