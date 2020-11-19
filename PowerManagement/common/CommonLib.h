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


#ifndef PowerManagement_CommonLib_h
#define PowerManagement_CommonLib_h

#include <asl.h>

/*
 * Power Management's ASL keys
 */
#define kPMASLDomainKey         	        "com.apple.iokit.domain"
#define kPMASLSignatureKey         	        "signature"
#define kPMASLUUIDKey            	        "uuid"
#define kPMASLUUID2Key            	        "uuid2"
#define kPMASLDelayKey                      "delay"
#define kPMASLValueKey            	        "value"
#define kPMASLTCPKeepAlive                  "tcpkeepaliveplatform"
#define kPMASLTCPKeepAliveExpired           "tcpkeepaliveexpired"
#define kPMASLPowerSourceKey                "powersource"
#define kPMASLBatteryPercentageKey          "batterypercentage"
#define kPMASLSleepCntSinceBoot             "sleepcntsinceboot"
#define kPMASLSleepCntSinceFailure          "sleepcntsincefailure16"

#define kPMASLValueSupported                "supported"
#define kPMASLValueExpired                  "expired"
#define kPMASLValueActive                   "active"

#define kPMASLClaimedEventKey               "claimedEvents"


/*
 * Signatures
 */
#define kPMASLSigSuccess                    "success"
#define kPMASLSigEarlyFailure               "Sleep has been initiated"
#define kPMASLSigAppsFailure                "Waiting for responses from notified applications"
#define kPMASLSigPriorityFailure            "Informing clients about upcoming changes"
#define kPMASLSigInterestFailure            "Informing rootDomain's clients about state changes"
#define kPMASLSigCapabilityFailure          "Informing clients about completed changes to system caps"
#define kPMASLSigNotificationFailure        "Sending kIOMessageSystemWillPowerOn to kernel and userspace"
#define kPMASLSigDriversFailure             "Some drivers failed to handle setPowerState"
#define kPMASLSigHibernateFailure           "Hibernate Failure"
#define kPMASLSigPlatformActionFailure      "Some drivers failed to handle Quiesce/Sleep action"
#define kPMASLSigPlatformDriverFailure      "Failure while halting non-boot CPUs"
#define kPMASLSigCpusFailure                "Failure while halting boot CPU"
#define kPMASLSigEFIFailure                 "EFI/Bootrom Failure after last point of entry to sleep"
#define kPMASLSigLoginwindowFailure         "Loginwindow Failure"
#define kPMASLSigResponseTimedOut           "Timed Out"
#define kPMASLSigResponseCancel             "Cancelled"
#define kPMASLSigResponseSlow               "Slow Response"
#define kPMASLSigDarkWakeEnterFailure       "Darkwake Entry Failure"
#define kPMASLSigDarkWakeExitFailure        "Darkwake Exit Failure"

/*
 * Failure Types
 */
#define kPMASLSleepFailureType              "Sleep"
#define kPMASLWakeFailureType               "Wake"
/*
 * SleepService Domains
 */
#define kPMASLDomainSleepServiceStarted     "com.apple.sleepservices.sessionStarted"
#define kPMASLDomainSleepServiceTerminated  "com.apple.sleepservices.sessionTerminated"
#define kPMASLDomainSleepServiceCapApp      "com.apple.sleepservices.clientCapTimeout"
#define kPMASLPrefixSleepServices           "com.apple.sleepservices."
#define kPMASLPrefixPM                      "com.apple.powermanagement."

/*
 * SleepService Signatures
 */
#define kPMASLSigSleepServiceExitClean      "com.apple.sleepserviced.ExitCleanAssertions"
#define kPMASLSigSleepServiceTimedOut       "com.apple.sleepserviced.ExitTimeOutAssertions"
#define kPMASLSigSleepServiceElevatedFull   "com.apple.sleepserviced.ElevatedToFullWake"
#define kPMASLSigSleepServiceElevatedDark   "com.apple.sleepserviced.ElevatedToDarkWake"

#define kPMFacility                             "com.apple.iokit.power"
#define kPMASLActionKey                         "Action"
#define kPMASLPIDKey                            "Process"
#define kPMASLProcessNameKey                    "ProcessName"
#define kPMASLAssertionNameKey                  "AssertName"
#define kPMASLAssertionTypeKey                  "AssertType"
#define kPMASLAssertionAgeKey                   "AssertAge"
#define kPMASLAssertionIdKey                    "AssertId"

/*
 * Power Management Domains
 */
#define kPMASLDomainPMStart                 "Start"
#define kPMASLDomainPMSleep                 "Sleep"
#define kPMASLDomainPMMaintenance           "MaintenanceWake"
#define kPMASLDomainPMWake                  "Wake"
#define kPMASLDomainPMDarkWake              "DarkWake"
#define kPMASLDomainPMAssertions            "Assertions"
#define kPMASLDomainPMWakeRequests          "WakeRequests"
#define kPMASLDomainHibernateStatistics     "HibernateStats"
#define kPMASLDomainFilteredFailure         "FilteredFailure"
#define kPMASLDomainAppNotify               "Notification"
#define kPMASLDomainSWFailure               "Failure"
#define kPMASLDomainWakeTime                "WakeTime"
#define kPMASLDomainSMCShutdownCause        "ShutdownCause"

#define kPMASLDomainThermalEvent            "ThermalEvent"
#define kPMASLDomainPerformanceEvent        "PerformanceEvent"
#define kPMASLDomainSleepRevert             "SleepAborted"

#define kPMASLDomainBattery                 "BatteryHealth"

#define kPMASLDomainSummaryPrefix           "Summary."
#define kPMASLDomainSummaryActive       "Summary.Active"
#define kPMASLDomainSummary             "Summary.Historical"

#define kPMASLDomainAppResponse             "ApplicationResponse"
#define kPMASLDomainAppResponseReceived     "Response.Received"
#define kPMASLDomainAppResponseCancel       "Response.Cancelled"
#define kPMASLDomainAppResponseSlow         "Response.SlowResponse"
#define kPMASLDomainAppResponseTimedOut     "Response.Timedout"

#define kPMASLDomainKernelClientStats       "KernelClientStats"
#define kPMASLDomainPMClientStats           "PMClientStats"

/*
 * Below three definitions are prefixes. Actual ASL keys will be
 * AppName0, DelayTypeApp0, DelayFromApp0
 * AppName1, DelayTypeApp1, DelayFromApp1  etc...
 */
#define kPMASLResponseAppNamePrefix         "AppName"
#define kPMASLResponseRespTypePrefix        "DelayTypeApp"
#define kPMASLResponseDelayPrefix           "DelayFromApp"
#define kPMASLResponsePSCapsPrefix          "PowerStateCaps"
#define kPMASLResponseMessagePrefix         "Message"

#define kPMASLResponseSystemTransition      "SystemTransition"


#define kPMASLDomainClientWakeRequests      "ClientWakeRequests"

#define KPMASLWakeReqAppNamePrefix          "WakeAppName"
#define kPMASLWakeReqTimePrefix             "WakeTime"
#define kPMASLWakeReqTimeDeltaPrefix        "WakeTimeDelta"
#define kPMASLWakeReqTypePrefix             "WakeType"
#define kPMASLWakeReqClientInfoPrefix       "WakeClientInfo"
#define kPMASLWakeReqChosenIdx              "WakeRequestChosen"

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

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define kPMASLStorePath                 "/var/log/powermanagement"

// SMC shutdown causes
typedef struct {
    const char *shutdownCauseString;
    const int shutdownCause;
} SMCShutdownCause;

__private_extern__ const char *smcShutdownCauseString(int shutdownCause);

__private_extern__ io_registry_entry_t getRootDomain(void);
__private_extern__ io_registry_entry_t getIOPMPowerSource(void);

__private_extern__ const char           *stringForLWCode(uint8_t code);
__private_extern__ const char           *stringForPMCode(uint8_t code);


/* PM Kernel shares times with user space in a packed 64-bit integer.
 * Seconds since 1970 in the lower 32, microseconds in the upper 32.
 */
__private_extern__ CFAbsoluteTime       _CFAbsoluteTimeFromPMEventTimeStamp(uint64_t kernelPackedTime);

__private_extern__ bool getAggressivenessValue(CFDictionaryRef     dict,
                                               CFStringRef         key,
                                               CFNumberType        type,
                                               uint32_t           *ret);


__private_extern__ bool                 platformPluginLoaded(void);

__private_extern__ IOReturn ActivatePMSettings(
                                               CFDictionaryRef                 useSettings,
                                               bool                            removeUnsupportedSettings);

__private_extern__ CFCalendarRef        _gregorian(void);

__private_extern__  asl_object_t open_pm_asl_store(char *);

__private_extern__ uint64_t CFAbsoluteTimeToMachAbsoluteTime(CFAbsoluteTime absoluteTime);

__private_extern__ uint64_t intervalInNanoseconds(uint64_t start, uint64_t end);

// This doesn't seem to be defined in the header file
extern kern_return_t mach_get_times(uint64_t* absolute_time, uint64_t* continuous_time, struct timespec *tp);

// Kernel assertions
const char * descriptiveKernelAssertions(uint32_t val);
#endif
