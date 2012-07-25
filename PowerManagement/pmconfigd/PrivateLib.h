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

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>
#if TARGET_OS_EMBEDDED
#define __MACH_PORT_DEBUG(cond, str, port) do {} while(0)
#endif
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

#include <dispatch/dispatch.h>
#include "PMAssertions.h"

#if !TARGET_OS_EMBEDDED
  #define HAVE_CF_USER_NOTIFICATION     1
  #define HAVE_SMART_BATTERY            1
#endif


#define kProcNameBufLen                     (2*MAXCOMLEN)

/* System Capability Macros */

#define CAPABILITY_BIT_CHANGED(x, y, b)     ((x ^ y) & b)
#define CHANGED_CAP_BITS(x, y)              ((x) ^ (y))
#define BIT_IS_SET(x,b)                     ((x & b)==b)
#define BIT_IS_NOT_SET(x,b)                 ((x & (b))==0)

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



struct IOPMBattery {
    io_registry_entry_t     me;
    io_object_t             msg_port;
    CFMutableDictionaryRef  properties;
    int                     externalConnected:1;
    int                     externalChargeCapable:1;
    int                     isCharging:1;
    int                     isPresent:1;
    int                     markedDeclining:1;
    int                     isTimeRemainingUnknown:1;
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
    int                     invalidWakeSecs;
    CFStringRef             batterySerialNumber;
    CFStringRef             health;
    CFStringRef             failureDetected;
    CFStringRef             name;
    CFStringRef             dynamicStoreKey;
    CFStringRef             chargeStatus;
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
} IOPMAggressivenessFactors;

enum { 
    kIOHibernateMinFreeSpace                            = 750*1024ULL*1024ULL  /* 750Mb */
};

__private_extern__ IOReturn ActivatePMSettings(
    CFDictionaryRef                 useSettings,
    bool                            removeUnsupportedSettings);



#define kPowerManagementBundlePathCString       "/System/Library/CoreServices/powerd.bundle"
#define kPowerdBundleIdentifier                 CFSTR("com.apple.powerd")

#define kPowerManagementBundlePathString        CFSTR(kPowerManagementBundlePathCString)


/* 
 * MessageTracer Keys
 */
#define kMsgTracerDomainKey         	        "com.apple.message.domain"
#define kMsgTracerSignatureKey         	        "com.apple.message.signature"
#define kMsgTracerUUIDKey            	        "com.apple.message.uuid"
#define kMsgTracerUUID2Key            	        "com.apple.message.uuid2"
#define kMsgTraceDelayKey                       "com.apple.message.delay"
#define kMsgTracerValueKey            	        "com.apple.message.value"
#define kMsgTracerValue2Key            	        "com.apple.message.value2"
#define kMsgTracerValue3Key            	        "com.apple.message.value3"
#define kMsgTracerResultKey                     "com.apple.message.result"
#define kMsgTracerResultSuccess                 "Success"
#define kMsgTracerResultFailure                 "Failure"
#define kMsgTracerResultNoop                    "Noop"
#define kMsgTracerValueUndefined                "undefined"

/*
 * Power Management Domains
 */
#define kMsgTracerDomainPMSleep                 "com.apple.powermanagement.Sleep"
#define kMsgTracerDomainPMMaintenance           "com.apple.powermanagement.MaintenanceWake"
#define kMsgTracerDomainPMWake                  "com.apple.powermanagement.Wake"
#define kMsgTracerDomainPMDarkWake              "com.apple.powermanagement.DarkWake"
#define kMsgTraceRDomainPMSystemPowerState      "com.apple.powermanagement.SystemPowerState"
#define kMsgTracerDomainPMAssertions            "com.apple.powermanagement.Assertions"
#define kMsgTracerDomainPMWakeRequests          "com.apple.powermanagement.WakeRequests"
#define kMsgTracerDomainHibernateStatistics     "com.apple.powermanagement.HibernateStats"
#define kMsgTracerDomainFilteredFailure         "com.apple.powermanagement.FilteredFailure"
#define kMsgTracerDomainAppNotify               "com.apple.powermanagement.Notification"
#define kMsgTracerDomainAppResponse             "com.apple.powermanagement.ApplicationResponse"
#define kMsgTracerDomainAppResponseReceived     kMsgTracerDomainAppResponse ".Response"
#define kMsgTracerDomainAppResponseCancel       kMsgTracerDomainAppResponse ".Cancelled"
#define kMsgTracerDomainAppResponseSlow         kMsgTracerDomainAppResponse ".SlowResponse"
#define kMsgTracerDomainAppResponseTimedOut     kMsgTracerDomainAppResponse ".Timedout"

/*
 * Signatures
 */
#define kMsgTracerSigSuccess                    kMsgTracerResultSuccess
#define kMsgTracerSigEarlyFailure               "Early Failure"
#define kMsgTracerSigAppsFailure                "Apps Failure"
#define kMsgTracerSigPriorityFailure            "Priority Failure"
#define kMsgTracerSigInterestFailure            "Interest Failure"
#define kMsgTracerSigCapabilityFailure          "Capability Failure"
#define kMsgTracerSigNotificationFailure        "Notification Failure"
#define kMsgTracerSigDriversFailure             "Drivers Failure"
#define kMsgTracerSigHibernateFailure           "Hibernate Failure"
#define kMsgTracerSigPlatformActionFailure      "Platform Action Failure"
#define kMsgTracerSigPlatformDriverFailure      "Platform Driver Failure"
#define kMsgTracerSigCpusFailure                "Cpus Failure"
#define kMsgTracerSigPlatformFailure            "Platform Failure"
#define kMsgTracerSigLoginwindowAuthFailure     "Loginwindow Authorization Failure"
#define kMsgTracerSigResponseTimedOut           "Timed Out"
#define kMsgTracerSigResponseCancel             "Cancelled"
#define kMsgTracerSigResponseSlow               "Slow Response"

/*
 * SleepService Domains
 */
#define kMsgTracerDomainSleepServiceStarted     "com.apple.sleepservices.sessionStarted"
#define kMsgTracerDomainSleepServiceTerminated  "com.apple.sleepservices.sessionTerminated"
#define kMsgTracerDomainSleepServiceCapApp      "com.apple.sleepservices.clientCapTimeout"
#define kMsgTracerPrefixSleepServices           "com.apple.sleepservices."
#define kMsgTracerPrefixPM                      "com.apple.powermanagement."

/*
 * SleepService Signatures
 */
#define kMsgTracerSigSleepServiceExitClean      "com.apple.sleepserviced.ExitCleanAssertions"
#define kMsgTracerSigSleepServiceTimedOut       "com.apple.sleepserviced.ExitTimeOutAssertions"
#define kMsgTracerSigSleepServiceElevatedFull   "com.apple.sleepserviced.ElevatedToFullWake"
#define kMsgTracerSigSleepServiceElevatedDark   "com.apple.sleepserviced.ElevatedToDarkWake"

/*
 * Power Management's ASL keys
 */
#define kPMASLMessageKey                        "com.apple.powermanagement"
#define kPMASLMessageLogValue                   "pmlog"

#define kPMASLActionKey                         "Action"
#define kPMASLPIDKey                            "Process"
#define kPMASLAssertionNameKey                  "AssertionName"
#define kPMASLAssertionActionCreate             "Created"
#define kPMASLAssertionActionRelease            "Released"
#define kPMASLAssertionActionClientDeath        "ClientDied"
#define kPMASLAssertionActionTimeOut            "TimedOut"
#define kPMASLAssertionActionSummary            "Summary"
#define kPMASLAssertionActionTurnOff            "Turned off"
#define kPMASLAssertionActionTurnOn             "Turned on"

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

/* mt2PublishReports
 * Debug routines. Should only be used for debugging to influence the mt2 publishing cycle.
 * Don't call these as part of a normal sleep/wake/darkwake cycle - these are special case calls.
 */
void mt2PublishReports(void);

// kIOPMAssertionProcessNameKey - key to IOPMAssertion dictionary
#ifndef kIOPMAssertionProcessNameKey
#define kIOPMAssertionProcessNameKey            CFSTR("Process Name")
#endif

#ifndef kIOPMRootDomainWakeReasonKey
// As defined in Kernel.framework/IOKit/pwr_mgt/RootDomain.h
#define kIOPMRootDomainWakeReasonKey            "Wake Reason"
#endif

#ifndef kIOPMRootDomainWakeTypeKey
// As defined in Kernel.framework/IOKit/pwr_mgt/RootDomain.h
#define kIOPMRootDomainWakeTypeKey              "Wake Type"
#endif



#define kAssertionHumanReadableReasonTTY        CFSTR("A remote user is connected. That prevents system sleep.")

__private_extern__ void                 logASLMessageSleep(const char *sig, const char *uuidStr, 
                                                           const char *failureStr, int sleep_type);

enum {
    kIsFullWake = 0,
    kIsDarkWake = 1,
    kIsDarkToFullWake = 2,
    kIsS0Sleep
};
__private_extern__ void                 logASLMessageWake(const char *sig, const char *uuidStr, 
                                                          const char *failureStr, int dark_wake);

__private_extern__ void                 logASLMessageFilteredFailure(uint64_t pmFailureStage, const char *pmFailureString,
                                                                     const char *uuidStr, int shutdowncode);

__private_extern__ void                 logASLMessageHibernateStatistics(void);

__private_extern__ void                 logASLMessageApplicationResponse(CFStringRef logSourceString, CFStringRef appNameString,
                                                                         CFStringRef responseTypeString, CFNumberRef responseTime,
                                                                         int notificationBits);

__private_extern__ void                  logASLMessageAppNotify( CFStringRef appNameString, int notificationBits);
__private_extern__ void                 logASLMessageKernelApplicationResponses(void);

__private_extern__ void                 logASLMessageSystemPowerState(bool inS3, int runState);


__private_extern__ void                 logASLMessagePMConnectionScheduledWakeEvents(CFStringRef requestedMaintenancesString);

__private_extern__ void                 logASLMessageExecutedWakeupEvent(CFStringRef requestedMaintenancesString);

#define kAppResponseLogSourceKernel             CFSTR("Kernel")
#define kAppResponseLogSourcePMConnection       CFSTR("PMConnection")
#define kAppResponseLogSourceSleepServiceCap    CFSTR("SleepService")

/*
 * If a PMConnection client doesn't respond to a sleep/wake notification in longer 
 * than kAppResponseLogThresholdMS, log it to pmset -g log.
 */
#define kAppResponseLogThresholdMS              250

// Dictionary lives as a setting in com.apple.PowerManagement.plist
// The keys to this dictionary are for Date & for UUID
#define kPMSettingsCachedUUIDKey                "LastSleepUUID"
#define kPMSettingsDictionaryDateKey            "Date"
#define kPMSettingsDictionaryUUIDKey            "UUID"

/* PM Kernel shares times with user space in a packed 64-bit integer. 
 * Seconds since 1970 in the lower 32, microseconds in the upper 32.
 */
__private_extern__ CFAbsoluteTime       _CFAbsoluteTimeFromPMEventTimeStamp(uint64_t kernelPackedTime);

__private_extern__ IOPMBattery          **_batteries(void);
__private_extern__ IOPMBattery          *_newBatteryFound(io_registry_entry_t);
__private_extern__ void                 _batteryChanged(IOPMBattery *);
__private_extern__ bool                 _batteryHas(IOPMBattery *, CFStringRef);
__private_extern__ int                  _batteryCount(void);
__private_extern__ void                 _removeBattery(io_registry_entry_t);
__private_extern__ IOReturn             _getACAdapterInfo(uint64_t *acBits);
__private_extern__ PowerSources         _getPowerSource(void);

__private_extern__ void                 wakeDozingMachine(void);

#if !TARGET_OS_EMBEDDED
__private_extern__ CFUserNotificationRef _copyUPSWarning(void);
__private_extern__ IOReturn              _smcWakeTimerPrimer(void);
__private_extern__ IOReturn              _smcWakeTimerGetResults(uint16_t *mSec);
__private_extern__ bool                  smcSilentRunningSupport(void);
#endif

__private_extern__ void                 _askNicelyThenShutdownSystem(void);
__private_extern__ void                 _askNicelyThenRestartSystem(void);
__private_extern__ void                 _askNicelyThenSleepSystem(void);


__private_extern__ SCDynamicStoreRef    _getSharedPMDynamicStore(void);


// getUUIDString copies the UUID string into the provided buffer
// returns true on success; or false if the copy failed, or the UUID does not exist
__private_extern__ bool                 _getUUIDString(char *buf, int buflen);
__private_extern__ bool                 _getSleepReason(char *buf, int buflen);
__private_extern__ bool                 _getWakeReason(char *buf, int buflen);

__private_extern__ io_registry_entry_t  getRootDomain(void);
__private_extern__ IOReturn             _setRootDomainProperty(CFStringRef key, CFTypeRef val);
__private_extern__ CFTypeRef            _copyRootDomainProperty(CFStringRef key);


__private_extern__ int                  callerIsRoot(int uid);
__private_extern__ int                  callerIsAdmin(int uid, int gid);
__private_extern__ int                  callerIsConsole(int uid, int gid);
__private_extern__ void                 _PortInvalidatedCallout(CFMachPortRef port, void *info);

__private_extern__ const char           *stringForLWCode(uint8_t code);
__private_extern__ const char           *stringForPMCode(uint8_t code);

__private_extern__ CFTimeInterval       _getHIDIdleTime(void);

__private_extern__ CFRunLoopRef         _getPMRunLoop(void);
__private_extern__ dispatch_queue_t     _getPMDispatchQueue(void);

__private_extern__ bool getAggressivenessValue(CFDictionaryRef     dict,
                                               CFStringRef         key,
                                               CFNumberType        type,
                                               uint32_t           *ret);


__private_extern__ void                 _oneOffHacksSetup(void);

__private_extern__ IOReturn getNvramArgInt(char *key, int *value);
#endif

