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

#ifndef _privatelib_h_
#define _privatelib_h_

//#define DEBUG_MACH_PORT_ALLOCATIONS 1


#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCDynamicStoreKey.h>



#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>

#include <SystemConfiguration/SCPrivate.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMUPSPrivate.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <os/log.h>


#include <dispatch/dispatch.h>
#include "PMAssertions.h"
#include "CommonLib.h"
#include "adaptiveDisplay.h"

  #define HAVE_CF_USER_NOTIFICATION     1
  #define HAVE_SMART_BATTERY            1


#define kProcNameBufLen                     (2*MAXCOMLEN)

/* System Capability Macros */

#define CAPABILITY_BIT_CHANGED(x, y, b)     ((x ^ y) & b)
#define CHANGED_CAP_BITS(x, y)              ((x) ^ (y))
#define BIT_IS_SET(x,b)                     ((x & b)==b)
#define BIT_IS_NOT_SET(x,b)                 ((x & (b))==0)

/* gDebugFlags bits */
#define kIOPMDebugAssertionASLLog           0x01
#define kIOPMDebugLogAssertionSynchronous   0x02
#define kIOPMDebugLogCallbacks              0x04
#define kIOPMDebugLogWakeRequests           0x08
#define kIOPMDebugLogUserActivity           0x10
#define kIOPMDebugEnableSpindumpOnFullwake  0x20
#define kIOPMDebugLogAssertionActivity      0x40  // Logs assertion data to log archive
#define kIOPMDebugLogAssertionNameChange    0x80


#define PM_LOG_SYSTEM       "powerd"
#define SYSLOAD_LOG         "systemLoad"
#define AGGD_REPORTS_LOG    "aggdReport"
#define BATTERY_LOG         "battery"
#define ASSERTIONS_LOG      "assertions"
#define PMSETTINGS_LOG      "pmSettings"
#define SLEEPWAKE_LOG       "sleepWake"
#define ADAPTIVEDISPLAY_LOG "adaptiveDisplay"
#define WAKEREQUESTS_LOG    "wakeRequests"
#define DISPLAY_LOG         "displayState"
#define PREFS_LOG           "prefs"

#ifndef LOG_STREAM
#define LOG_STREAM OS_LOG_DEFAULT
#endif

#define INFO_LOG(fmt, args...) \
{ \
    os_log(LOG_STREAM, fmt, ##args); \
}
#define DEBUG_LOG(fmt, args...) \
{ \
    os_log_debug(LOG_STREAM, fmt, ##args); \
}

#define ERROR_LOG(fmt, args...) \
{  \
    os_log_error(LOG_STREAM, fmt, ##args); \
}

#define CFDictionaryGetIntValue(dict, key, val) { \
    CFNumberRef n;   \
    if (CFDictionaryGetValueIfPresent((dict), (key), (const void **)&n)) { \
        if (isA_CFNumber(n)) {    \
            CFNumberGetValue((n), kCFNumberIntType, &(val)); \
        } \
    } \
}

#define CFDictionarySetIntValue(dict, key, val) { \
    CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &(val)); \
    if (n) {    \
        CFDictionarySetValue((dict), (key), n);     \
        CFRelease(n);   \
    }   \
}

#define CFDictionarySetInt64Value(dict, key, val) { \
    CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &(val)); \
    if (n) {    \
        CFDictionarySetValue((dict), (key), n);     \
        CFRelease(n);   \
    }   \
}

#define CFDictionaryGetInt64Value(dict, key, val) { \
    CFNumberRef n;   \
    if (CFDictionaryGetValueIfPresent((dict), (key), (const void **)&n)) { \
        if (isA_CFNumber(n)) {    \
            CFNumberGetValue((n), kCFNumberSInt64Type, &(val)); \
        } \
    } \
}

#define CFDictionaryGetDoubleValue(dict, key, val) { \
    CFNumberRef n;   \
    if (CFDictionaryGetValueIfPresent((dict), (key), (const void **)&n)) { \
        if (isA_CFNumber(n)) {    \
            CFNumberGetValue((n), kCFNumberDoubleType, &(val)); \
        } \
    } \
}

#define CFDictionarySetDoubleValue(dict, key, val) { \
    CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &(val)); \
    if (n) {    \
        CFDictionarySetValue((dict), (key), n);     \
        CFRelease(n);   \
    }   \
}

/*****************************************************************************/

enum {
    kLogWakeEvents =                        (1<<0),
    kLogAssertions =                        (1<<1)
};


__private_extern__ bool PMDebugEnabled(uint32_t which);


// Run states (R-state) defined within the ON power state.
enum {
    kRStateNormal = 0,
    kRStateDark,
    kRStateMaintenance,
    kRStateCount
};

// Definitions of PFStatus keys for AppleSmartBattery failures
enum {
    kSmartBattPFExternalInput =             (1<<0),
    kSmartBattPFSafetyOverVoltage =         (1<<1),
    kSmartBattPFChargeSafeOverTemp =        (1<<2),
    kSmartBattPFDischargeSafeOverTemp =     (1<<3),
    kSmartBattPFCellImbalance =             (1<<4),
    kSmartBattPFChargeFETFailure =          (1<<5),
    kSmartBattPFDischargeFETFailure =       (1<<6),
    kSmartBattPFDataFlushFault =            (1<<7),
    kSmartBattPFPermanentAFECommFailure =   (1<<8),
    kSmartBattPFPeriodicAFECommFailure =    (1<<9),
    kSmartBattPFChargeSafetyOverCurrent =   (1<<10),
    kSmartBattPFDischargeSafetyOverCurrent = (1<<11),
    kSmartBattPFOpenThermistor =            (1<<12),
    // reserved 1<<13,
    // reserved 1<<14,
    kSmartBattPFFuseBlown =                 (1<<15)
};

typedef enum {
   kBatteryPowered = 0,
   kACPowered
} PowerSources;

typedef enum {
    kIsFullWake = 0,
    kIsDarkWake = 1,
    kIsDarkToFullWake = 2,
    kIsS0Sleep = 3,
    kIsUserWake = 4 // All FullWakes that are not notification wakes
} WakeTypeEnum;

struct IOPMBattery {
    io_registry_entry_t     me;
    io_object_t             msg_port;
    CFMutableDictionaryRef  properties;
    uint32_t                     externalConnected:1;
    uint32_t                     rawExternalConnected:1;
    uint32_t                     externalChargeCapable:1;
    uint32_t                     isCharging:1;
    uint32_t                     showChargingUI:1;
    uint32_t                     playChargingChime:1;
    uint32_t                     isPresent:1;
    uint32_t                     markedDeclining:1;
    uint32_t                     isTimeRemainingUnknown:1;
    uint32_t                     isCritical:1;
    uint32_t                     isRestricted:1;
    uint32_t                pfStatus;
    int                     currentCap;
    int                     maxCap;
    int                     designCap;
    int                     voltage;
    int                     avgAmperage;
    int                     instantAmperage;
    int                     maxerr;
    int                     cycleCount;
    int                     location;
    int                     hwAverageTR;
    int                     hwInstantTR;
    int                     swCalculatedTR;
    int                     swCalculatedPR;
    int                     invalidWakeSecs;
    CFStringRef             batterySerialNumber;
    CFStringRef             health;
    CFStringRef             failureDetected;
    CFStringRef             name;
    CFStringRef             dynamicStoreKey;
    CFStringRef             chargeStatus;
    time_t                  lowCapRatioSinceTime;
    boolean_t               hasLowCapRatio;
};
typedef struct IOPMBattery IOPMBattery;



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
    unsigned int        fDeepSleepEnable;
    unsigned int        fDeepSleepDelay;
    unsigned int        fAutoPowerOffEnable;
    unsigned int        fAutoPowerOffDelay;
} IOPMAggressivenessFactors;

enum {
    kIOHibernateMinFreeSpace                            = 750*1024ULL*1024ULL  /* 750Mb */
};


#define kPowerManagementBundlePathCString       "/System/Library/CoreServices/powerd.bundle"
#define kPowerdBundleIdentifier                 CFSTR("com.apple.powerd")
#define kPowerManagementBundlePathString        CFSTR(kPowerManagementBundlePathCString)

/***************************************************************
 * FDR
 * Flight Data Recorder SPI calls
 *
 ***************************************************************/

enum {
    kFDRInit                    = (1 << 0),
    kFDRACChanged               = (1 << 1),
    kFDRSleepEvent              = (1 << 2),
    kFDRUserWakeEvent           = (1 << 3),
    kFDRDarkWakeEvent           = (1 << 4),
    kFDRBattEventPeriodic       = (1 << 5),
    kFDRBattEventAsync          = (1 << 6)
};

/* recordFDREvent
 * powerd uses this method to publish battery and system power state info
 * to FDR.
 */
void recordFDREvent(int eventType, bool checkStandbyStatus);

/***************************************************************
 * MT2
 * MessageTracer SPI calls
 *
 ***************************************************************/

/* This is a bitfield. It describes system state at the time of a wakeup. */
enum {
    kWakeStateDark              = (1 << 0),     /* 0 == FullWake. 1 == DarkWake */
    kWakeStateBattery           = (1 << 1),     /* 0 == AC. 1 == Battery */
    kWakeStateLidClosed         = (1 << 2)      /* 0 == LidOpen. 1 == LidCLosed */
};
#define kWakeStateFull          0
#define kWakeStateAC            0
#define kWakeStateLidOpen       0
#define kWakeStateCount         (8)

/* This is a bitfield. It describes the themal state at the time of a "thermal event". */
enum {
    kThermalStateFansOn         = (1 << 0),     /* 0 == Fans off. 1 == Fans On */
    kThermalStateSleepRequest   = (1 << 1)      /* 0 == No Sleep Request. 1 == Received sleep request. */
};
#define kThermalStateFansOff    0
#define kThermalStateNoRequest  0
#define kThermalStateCount      (4)


static bool                     _platformBackgroundTaskSupport = false;
static bool                     _platformSleepServiceSupport = false;

/* initializeMT2Aggregator
 * Call once at powerd launch.
 * After calling this, powerd should let internal MT2Aggregator code decide
 * when to publish reports, at what intervals to publish reports.
 */
void initializeMT2Aggregator(void);

/* mt2DarkWakeEnded
 * powerd must call mt2DarkWakeEnded to stop recording assertions when we exit DarkWake
 */
void mt2DarkWakeEnded(void);

/* mt2EvaluateSystemSupport
 * powerd should call this to report changes to Energy Saver settings.
 * Populates MT2Aggregator fields SMCSupport, PlatformSupport, checkedForAC, checkedForBatt
 */
void mt2EvaluateSystemSupport(void);

/* mt2RecordWakeEvent
 * powerd should call this once upon wakeup from S3/S4 to S0.
 * mt2RecordWakeEvent will populate the battery & lid bits; caller must only supply wake type.
 * Arguments: kWakeStateFull, or kWakeStateDark
 * Records a wake events as DarkWake/FullWake, and records AC/Batt, LidOpen/LidClosed.
 */
void mt2RecordWakeEvent(uint32_t description);

/* mt2RecordThermalEvent
 * powerd should call at most once per DarkWake. These should be rare conditions, and will not occur in all situations.
 * Records a thermal event: Fanson/fansoff, sleeprequest/none.
 */
void mt2RecordThermalEvent(uint32_t description);

/* mt2RecordAsserctionEvent
 * powerd should call to indicate that a process "whichApp" has "action'd" assertion "AssertionType".
 * @arg assertionType is one of PMAssertion.h: kPushServiceTaskIndex, kBackgroundTaskIndex
 * @arg action is one of PMAssertions.h: kOpRaise, kOpGlobalTimeout
 * @arg theAssertion is a valid assertion datastructure (we map this back to process name, then process index)
 * OK to call many times during a DarkWake, upon unique (assertionType, action, whichApp) tuples.
 */
void mt2RecordAssertionEvent(assertionOps action, assertion_t *theAssertion);

/*
 * mt2RecordAppTimeouts
 * powerd should call to report power notification acknowledgement timeouts.
 * @arg sleepReason indicates the type of sleep, system is entering into when ack timeout occurred.
 * @arg procName indicates the name of process which didn't respond to notification.
 */
void mt2RecordAppTimeouts(CFStringRef sleepReason, CFStringRef procName);

/*
 * mt2PublishSleepWakeInfo
 * powerd should call to report sleep wake info
 * @arg wakeType     Wake Type
 * @arg claimedWake  Wake reason string as provided by the driver
 * @arg success      Wake was successful (not a sleep/wake failure)
 */
void mt2PublishSleepWakeInfo(CFStringRef wakeType, CFStringRef claimedWake, bool success);

/* mt2PublishReports
 * Debug routines. Should only be used for debugging to influence the mt2 publishing cycle.
 * Don't call these as part of a normal sleep/wake/darkwake cycle - these are special case calls.
 */
void mt2PublishReports(void);

/* mt2PublishSleepWakeFailure
 * powerd should call to report a sleep/wake failure.
 * @arg failType indicates if it is sleep/wake failure
 * @arg failPhase is the category of the failure
 * @arg pci_string is a list of pci devices
 */
void mt2PublishSleepWakeFailure(const char *failType, const char *failPhase, const char *pci_string);

/* mt2PublishWakeTime
 * powerd should call to report wake time
 * @arg waketime wake time in milliseconds
 * @arg waketype wake type
 */
void mt2PublishWakeTime(double waketime, WakeTypeEnum waketype);

#define kAssertionHumanReadableReasonTTY        CFSTR("A remote user is connected. That prevents system sleep.")

__private_extern__ void                 logASLMessagePMStart(void);
__private_extern__ void                 logASLMessageSleep(const char *sig, const char *uuidStr,
                                                           const char *failureStr, int sleep_type);

__private_extern__ void                 logASLMessageWake(const char *sig, const char *uuidStr,
                                                        const char *failureStr,
                                                        IOPMCapabilityBits in_capabilities, WakeTypeEnum dark_wake);

__private_extern__ void                 logASLMessageWakeTime(uint64_t waketime, WakeTypeEnum waketype);

__private_extern__ void                 logASLMessageSMCShutdownCause(int shutdownCause);

__private_extern__ void                 logASLMessagePMConnectionResponse(CFStringRef logSourceString, CFStringRef appNameString,
                                                         CFStringRef responseTypeString, CFNumberRef responseTime,
                                                         int notificationBits);

__private_extern__ void                 logASLPMConnectionNotify(CFStringRef appNameString, int notificationBits);
__private_extern__ void                 logASLDisplayStateChange(void);
__private_extern__ void                 logASLThermalState(int thermalState);
__private_extern__ void                 logASLPerforamceState(int perfState);
__private_extern__ void                 logASLMessageAppStats(CFArrayRef appStats, char *domain);

__private_extern__ void                 logASLMessagePMConnectionScheduledWakeEvents(CFStringRef requestedMaintenancesString);

__private_extern__ void                 logASLMessageExecutedWakeupEvent(CFStringRef requestedMaintenancesString);

__private_extern__ void                 logASLMessageIgnoredDWTEmergency(void);
__private_extern__ void                 logASLInactivityWindow(inactivityWindowType type, CFDateRef start, CFDateRef end);


__private_extern__ void logASLMessageSleepCanceledAtLastCall( bool tcpka_active,
                                                              bool sys_active,
                                                              bool pending_wakes);

__private_extern__ void                 logASLBatteryHealthChanged(const char *health,
                                                                   const char *oldhealth,
                                                                   const char *reason);
__private_extern__ void                 logASLLowBatteryWarning(IOPSLowBatteryWarningLevel level,
                                                   int time, int ccap);
__private_extern__ void                 logASLSleepPreventers(int preventerType);

#define kAppResponseLogSourceKernel             CFSTR("Kernel")
#define kAppResponseLogSourcePMConnection       CFSTR("PMConnection")
#define kAppResponseLogSourceSleepServiceCap    CFSTR("SleepService")

/*
 * If a PMConnection client doesn't respond to a sleep/wake notification in longer
 * than kAppResponseLogThresholdMS, log it to pmset -g log.
 */
#define kAppResponseLogThresholdMS              250

__private_extern__ aslmsg               new_msg_pmset_log(void);
__private_extern__ bool                 isA_installEnvironment(void);

__private_extern__ void                 _removeBattery(io_registry_entry_t);

__private_extern__ CFUserNotificationRef _copyUPSWarning(void);
__private_extern__ IOReturn              _smcWakeTimerPrimer(void);
__private_extern__ IOReturn              _smcWakeTimerGetResults(double *mSec);
__private_extern__ bool                  smcSilentRunningSupport(void);

__private_extern__ void                 _askNicelyThenShutdownSystem(void);
__private_extern__ void                 _askNicelyThenRestartSystem(void);
__private_extern__ void                 _askNicelyThenSleepSystem(void);


__private_extern__ SCDynamicStoreRef    _getSharedPMDynamicStore(void);


// getUUIDString copies the UUID string into the provided buffer
// returns true on success; or false if the copy failed, or the UUID does not exist
__private_extern__ bool                 _getUUIDString(char *buf, int buflen);
__private_extern__ CFStringRef          _updateSleepReason(void);
__private_extern__ CFStringRef          _getSleepReason(void);
__private_extern__ void                 _resetWakeReason(void);
__private_extern__ void                 _updateWakeReason(CFStringRef *wakeReason, CFStringRef *wakeType);
__private_extern__ void                 getPlatformWakeReason(CFStringRef *wakeReason, CFStringRef *wakeType);
__private_extern__ void                 appClaimWakeReason(xpc_connection_t peer, xpc_object_t claim);
__private_extern__ bool                 checkForAppWakeReason(CFStringRef wakeReason);


__private_extern__ IOReturn             _setRootDomainProperty(CFStringRef key, CFTypeRef val);
__private_extern__ CFTypeRef            _copyRootDomainProperty(CFStringRef key);


__private_extern__ int                  callerIsRoot(int uid);
__private_extern__ int                  callerIsAdmin(int uid, int gid);
__private_extern__ int                  callerIsConsole(int uid, int gid);
__private_extern__ void                 _PortInvalidatedCallout(CFMachPortRef port, void *info);


__private_extern__ CFTimeInterval       _getHIDIdleTime(void);

__private_extern__ dispatch_queue_t     _getPMMainQueue(void);

__private_extern__ bool auditTokenHasEntitlement(
                                                 audit_token_t token,
                                                 CFStringRef entitlement);
__private_extern__ bool xpcConnectionHasEntitlement(xpc_object_t connection,
                                                    CFStringRef entitlement);

__private_extern__ void                 _oneOffHacksSetup(void);

__private_extern__ IOReturn getNvramArgInt(char *key, int *value);
__private_extern__ IOReturn getNvramArgStr(char *key, char *buf, size_t bufSize);
__private_extern__ uint64_t             getMonotonicContinuousTime(void );
__private_extern__ uint64_t             getMonotonicTime(void);
__private_extern__ uint64_t             monotonicTS2Secs(uint64_t tsc);
__private_extern__ void                 incrementSleepCnt(void);
__private_extern__ const char *sleepType2String(int sleepType);
__private_extern__ int getLastSleepType(void);
__private_extern__ IOReturn _smcWriteKey( uint32_t key, uint8_t *outBuf, uint8_t outBufMax);
__private_extern__ IOReturn _smcReadKey( uint32_t key, uint8_t *outBuf, uint8_t *outBufMax, bool byteSwap);
__private_extern__ IOReturn _smcReadKeySwizzle(uint32_t key, uint8_t *outBuf, uint8_t *outBufMax, unsigned int byteSwap);
__private_extern__ bool isSenderEntitled(xpc_object_t remoteConnection, CFStringRef entitlementString, bool requireRoot);
__private_extern__ void setCustomBatteryProps(xpc_object_t remoteConnection, xpc_object_t msg);
__private_extern__ void resetCustomBatteryProps(xpc_object_t remoteConnection, xpc_object_t msg);

__private_extern__ int pluginExecCommand(const char *path, char *const argv[],
		dispatch_queue_t queue, void (*callback)(pid_t pid, int status));

#ifdef XCTEST
void setAppWakeReason(CFStringRef reasonStr);

#endif

#endif

