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
#else
#include <SystemConfiguration/SCPrivate.h>
#endif

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


#if !TARGET_OS_EMBEDDED
  #define HAVE_CF_USER_NOTIFICATION     1
  #define HAVE_HID_SYSTEM               1
  #define HAVE_SMART_BATTERY            1
#endif


/*****************************************************************************/

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


struct IOPMBattery {
    io_registry_entry_t     me;
    io_object_t             msg_port;
    CFMutableDictionaryRef  properties;
    bool                    externalConnected:1;
    bool                    externalChargeCapable:1;
    bool                    isCharging:1;
    bool                    isPresent:1;
    bool                    markedDeclining:1;
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





/* ASL Keys
 */
#define kMsgTracerDomainKey         	        "com.apple.message.domain"
#define kMsgTracerSignatureKey         	        "com.apple.message.signature"
#define kMsgTracerUUIDKey            	        "com.apple.message.uuid"
#define kMsgTracerValueKey            	        "com.apple.message.value"
#define kMsgTracerValue2Key            	        "com.apple.message.value2"
#define kMsgTracerValue3Key            	        "com.apple.message.value3"
#define kMsgTracerResultKey                     "com.apple.message.result"
#define kMsgTracerResultSuccess                 "Success"
#define kMsgTracerResultFailure                 "Failure"
#define kMsgTracerResultNoop                    "Noop"

#define kMsgTracerDomainPMSleep           	  	"com.apple.powermanagement.sleep"
#define kMsgTracerDomainPMMaintenance           "com.apple.powermanagement.maintenancewake"
#define kMsgTracerDomainPMWake                  "com.apple.powermanagement.wake"
#define kMsgTracerDomainHibernateStatistics     "com.apple.powermanagement.hibernatestats"
#define kMsgTracerDomainFilteredFailure         "com.apple.powermanagement.filteredfailure"
#define kMsgTracerDomainAppResponse             "com.apple.powermanagement.applicationresponse"
#define kMsgTracerDomainAppResponseCancel       kMsgTracerDomainAppResponse ".cancelled"
#define kMsgTracerDomainAppResponseSlow         kMsgTracerDomainAppResponse ".slowresponse"
#define kMsgTracerDomainAppResponseTimedOut     kMsgTracerDomainAppResponse ".timedout"

#define kMsgTracerSigSuccess                    kMsgTracerResultSuccess
#define kMsgTracerSigEarlyFailure               "Early Failure"
#define kMsgTracerSigAppsFailure                "Apps Failure"
#define kMsgTracerSigDriversFailure             "Drivers Failure"
#define kMsgTracerSigHibernateFailure           "Hibernate Failure"
#define kMsgTracerSigPlatformFailure            "Platform Failure"
#define kMsgTracerSigLoginwindowAuthFailure     "Loginwindow Authorization Failure"

#define kMsgTracerSigResponseTimedOut           "Timed Out"
#define kMsgTracerSigResponseCancel             "Cancelled"
#define kMsgTracerSigResponseSlow               "Slow Response"

#define kMsgTracerValueUndefined                "undefined"

// MY_CAST_INT_POINTER casts a mach_port_t into a void * for CF containers
#define MY_CAST_INT_POINTER(x)  ((void *)(uintptr_t)(x))


__private_extern__ void logASLMessageSleep(
                            const char *sig, 
                            const char *uuidStr, 
                            CFAbsoluteTime date,
                            const char *failureStr);
__private_extern__ void logASLMessageWake(
                            const char *sig, 
                            const char *uuidStr, 
                            CFAbsoluteTime date, 
                            const char *failureStr);
__private_extern__ void logASLMessageFilteredFailure(
                            uint32_t pmFailureStage,
                            const char *pmFailureString,
                            const char *uuidStr, 
                            int shutdowncode);
__private_extern__ void logASLMessageHibernateStatistics(void);
__private_extern__ void logASLMessageApplicationResponse(
                            CFStringRef logSourceString,
                            CFStringRef appNameString,
                            CFStringRef responseTypeString,
                            CFNumberRef responseTime);
__private_extern__ void logASLMessageKernelApplicationResponses(void);
__private_extern__ void logASLMessageMaintenanceWake(void);

#define kAppResponseLogSourceKernel             CFSTR("Kernel")
#define kAppResponseLogSourcePMConnection       CFSTR("PMConnection")

#define kAppResponseLogThresholdMS              100

// Dictionary lives as a setting in com.apple.PowerManagement.plist
// The keys to this dictionary are for Date & for UUID
#define kPMSettingsCachedUUIDKey                "LastSleepUUID"
#define kPMSettingsDictionaryDateKey            "Date"
#define kPMSettingsDictionaryUUIDKey            "UUID"

__private_extern__ IOPMBattery **_batteries(void);
__private_extern__ IOPMBattery *_newBatteryFound(io_registry_entry_t);
__private_extern__ void _batteryChanged(IOPMBattery *);
__private_extern__ bool _batteryHas(IOPMBattery *, CFStringRef);
__private_extern__ int  _batteryCount(void);
__private_extern__ void  _removeBattery(io_registry_entry_t);

// Returns 10.0 - 10.4 style IOPMCopyBatteryInfo dictionary, when possible.
__private_extern__ CFArrayRef _copyLegacyBatteryInfo(void);

__private_extern__ void _askNicelyThenShutdownSystem(void);
__private_extern__ void _askNicelyThenRestartSystem(void);
__private_extern__ void _askNicelyThenSleepSystem(void);

__private_extern__ IOReturn _getSystemManagementKeyInt32(uint32_t key, uint32_t *val);
__private_extern__ IOReturn _getACAdapterInfo(uint64_t *acBits);

__private_extern__ IOReturn _smcWakeTimerPrimer(void);
__private_extern__ IOReturn _smcWakeTimerGetResults(uint16_t *mSec);

#if !TARGET_OS_EMBEDDED
__private_extern__ CFUserNotificationRef _showUPSWarning(void);
#endif

__private_extern__ SCDynamicStoreRef _getSharedPMDynamicStore(void);


// _PortInvalidatedCallout is implemented in pmconfigd.c
__private_extern__ void _PortInvalidatedCallout(CFMachPortRef port, void *info);


// getUUIDString copies the UUID string into the provided buffer
// returns true on success; or false if the copy failed, or the UUID does not exist
__private_extern__ bool _getUUIDString(char *buf, int buflen);

__private_extern__ bool _getSleepReason(char *buf, int buflen);
__private_extern__ bool _getWakeReason(char *buf, int buflen);

__private_extern__ io_registry_entry_t getRootDomain(void);
__private_extern__ IOReturn _setRootDomainProperty(
                                    CFStringRef     key,
                                    CFTypeRef       val);
__private_extern__ CFTypeRef _copyRootDomainProperty(
                                    CFStringRef     key);


__private_extern__ int callerIsRoot(int uid, int gid);
__private_extern__ int callerIsAdmin(int uid, int gid);
__private_extern__ int callerIsConsole(int uid, int gid);

__private_extern__ const char *stringForLWCode(uint8_t code);
__private_extern__ const char *stringForPMCode(uint8_t code);


enum {
    kChooseMaintenance      = 1,
    kChooseFullWake         = 2,
    kChooseReset            = 3
};
// _pm_scheduledevent_choose_best_wake_event
// Expected to be called TWICE at each system sleep
//  - once from AutoWakeScheduler.c when a system wakeup time is chosen
//  - once from PMConnection.c when a system maintenance time is chosen
//  - _choose_best_wake_event will select the earlier of the two, and activate
//      that event.
__private_extern__ IOReturn _pm_scheduledevent_choose_best_wake_event(
            int                 selector,
            CFAbsoluteTime      chosenTime);


__private_extern__ void _oneOffHacksSetup(void);

#endif

