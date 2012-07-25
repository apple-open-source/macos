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

#ifndef _IOPMLibPrivate_h_
#define _IOPMLibPrivate_h_

#include <TargetConditionals.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CFArray.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/*!
 * @constant    kIOPBundlePath
 * @abstract    c-string path to powerd.bundle
 */
#define kIOPMBundlePath             "/System/Library/CoreServices/powerd.bundle"

/*!
 * @constant    kIOPMSystemPowerStateNotify
 * @abstract    Notify(3) string that PM fires every time the system begins a sleep, wake, or a maintenance wake.
 * @discussion  The notification fires at the "system will" notificatin phase; e.g. at the beginning of the sleep or wake.
 *              Unless you have a strong need for this asynchronous sleep/wake notification, you 
 *              should really be using IOPMConnectionCreate().
 */
#define kIOPMSystemPowerStateNotify "com.apple.powermanagement.systempowerstate"

/*!
 * @constant    kIOPMSystemPowerCapabilitiesKeySuffix
 * @abstract    SCDynamicStoreKey location where the system state capability can be found.
 * @discussion  This state is always updated immediately prior to when PM delivers 
 *              the notify (3) notification kIOPMSystemPowerStateNotify
 *              The System power capabilities are defined by the 
 *              enum <code>@link IOPMSystemPowerStateCapabilities@/link</code> below.
 */
#define kIOPMSystemPowerCapabilitiesKeySuffix   "/IOKit/SystemPowerCapabilities"

/*!
 * @constant    kIOPMServerBootstrapName
 * @abstract    Do not use. There's no reason for any code outside of PowerManagement to use this.
 * @discussion  The PM system server registers via this key with launchd.
 */
#define kIOPMServerBootstrapName    "com.apple.PowerManagement.control"

/*!
 * @group       AutoWake API
 * @abstract    For internal use communicating between IOKitUser and PM configd
 */

/*!
 * @constant    kIOPMAutoWakePresPath
 * @abstract    Filename of the scheduled power events file.
 */
#define kIOPMAutoWakePrefsPath      "com.apple.AutoWake.xml"

/*
 * @constant    kIOPMAutoWakeScheduleImmediate
 * @abstract    Internal only argument to <code>@link IOPMSchedulePowerEvent@/link</code> 
 *              for use only by debugging tools.
 * @discussion  Once scheduled, these types are not cancellable, nor will they appear in any lists of
 *              scheduled arguments from IOPMCopyScheduledEvents. These are not acceptable
 *              types to pass as Repeating events.
 *
 *              The 'date' argument to IOPMSchedulePowerEvent is an absolute one for 
 *              'AutoWakeScheduleImmediate' and 'AutoPowerScheduleImmediate'. The effect
 *              of these WakeImmediate and PowerOnImmediate types is to schedule the
 *              wake/power event directly with the wake/power controller, ignoring all OS
 *              queueing and management. This will override a previously scheduled wake event
 *              by another application, should one exist. Recommended for testing only. 
 */
#define kIOPMAutoWakeScheduleImmediate      "WakeImmediate"

/*
 * @constant    kIOPMAutoPowerScheduleImmediate
 * @abstract    Internal only argument to <code>@link IOPMSchedulePowerEvent@/link</code> 
 *              for use only by debugging tools.
 * @discussion  Once scheduled, these types are not cancellable, nor will they appear in any lists of
 *              scheduled arguments from IOPMCopyScheduledEvents. These are not acceptable
 *              types to pass as Repeating events.
 *
 *              The 'date' argument to IOPMSchedulePowerEvent is an absolute one for 
 *              'AutoWakeScheduleImmediate' and 'AutoPowerScheduleImmediate'. The effect
 *              of these WakeImmediate and PowerOnImmediate types is to schedule the
 *              wake/power event directly with the wake/power controller, ignoring all OS
 *              queueing and management. This will override a previously scheduled wake event
 *              by another application, should one exist. Recommended for testing only.
 *
 *
 */
#define kIOPMAutoPowerScheduleImmediate     "PowerOnImmediate"

/*!
 * @constant    kIOPMSchedulePowerEventNotification
 * @abstract    Notification posted when IOPMSchedulePowerEvent successfully schedules a power event
 * @discussion  i.e. this notify(3) notification fires every time the list of scheduled power events changes.
 */
#define kIOPMSchedulePowerEventNotification "com.apple.system.IOPMSchedulePowerEventNotification"

/*
 * @constant    kIOPMAutoWakeRelativeSeconds
 * @abstract    Internal only argument to <code>@link IOPMSchedulePowerEvent@/link</code> 
 *              for use only by debugging tools.
 * @discussion  The 'date' argument to IOPMSchedulePowerEvent is an relative one to "right now," 
 *              when passing type for 'kIOPMSettingDebugWakeRelativeKey' or 'kIOPMAutoPowerRelativeSeconds'
 *
 *              e.g. In this case, we're setting the system to wake from sleep exactly 10
 *              seconds after the system completes going to sleep. We're passing in a date
 *              10 seconds past "right now", but the wakeup controller interprets this as 
 *              relative to sleep time.
 * 
 *              d = CFDateCreate(0, CFAbsoluteGetCurrent() + 10.0);
 *              IOPMSchedulePowerEvent(d, CFSTR("SleepCycler"), CFSTR(kIOPMAutoWakeRelativeSeconds) ); 
 */
#define kIOPMAutoWakeRelativeSeconds        kIOPMSettingDebugWakeRelativeKey
/*
 * @constant    kIOPMAutoPowerRelativeSeconds
 * @abstract    Internal only argument to <code>@link IOPMSchedulePowerEvent@/link</code> 
 *              for use only by debugging tools.
 * @discussion  The 'date' argument to IOPMSchedulePowerEvent is an relative one to "right now," 
 *              when passing type for 'kIOPMSettingDebugWakeRelativeKey' or 'kIOPMAutoPowerRelativeSeconds'
 *
 *              e.g. In this case, we're setting the system to wake from sleep exactly 10
 *              seconds after the system completes going to sleep. We're passing in a date
 *              10 seconds past "right now", but the wakeup controller interprets this as 
 *              relative to sleep time.
 * 
 *              d = CFDateCreate(0, CFAbsoluteGetCurrent() + 10.0);
 *              IOPMSchedulePowerEvent(d, CFSTR("SleepCycler"), CFSTR(kIOPMAutoWakeRelativeSeconds) ); 
 */
#define kIOPMAutoPowerRelativeSeconds       kIOPMSettingDebugPowerRelativeKey

/**************************************************
*
* Private assertions
*
**************************************************/
/*!
 * @functiongroup Private Assertions
 */

/*! 
 * @constant    kIOPMAssertionTypeApplePushServiceTask
 *
 * @discussion  When active during a SleepServices wake, will keep the system awake, subject to
 *              the sleep services time cap.
 *
 *              Except for running during SleepServices time, while a system is on battery power
 *              a ApplePushServiceTask assertion cannot keep the machine in a higher power state.
 *              e.g. An ApplePushServiceTask cannot prevent sleep if the system is on battery.
 *
 *              Awake Behavior on AC power: this assertion will force the system to stay awake. 
 *
 *              DarkWake Behavior on AC power: this assertion will force the system not to sleep;
 *                  e.g. the system will stay in DarkWake as long as this assertion is held.
 */
#define kIOPMAssertionTypeApplePushServiceTask        CFSTR("ApplePushServiceTask")

/*! 
 * @constant    kIOPMAssertionTypeBackgroundTask
 *
 * @discussion  This assertion should be created and held by applications while performing
 *              any system maintenance tasks, e.g. work not initiated by a user.
 *
 *              Example: Periodic system data backups, spotlight indexing etc.
 *
 *              Behavior of this assertion on Systems that support Silent Running: 
 *               On AC: Prevents sytem sleep. If system is idle or if user requests sleep(by 
 *                      lid close or by selecting sleep from apple menu), system runs with display 
 *                      off in silent running mode. Holding this assertion during darkwake results 
 *                      in extending dark wake in silent running mode.
 *               On Battery: This assertion is not honoured.
 *
 *              Behavior on Systems that don't support Silent Running:
 *               On AC: Prevents system idle sleep. Display may get turned off. If user requests
 *                      sleep(by lid close or apple menu sleep), then system goes into sleep.
 *                      If the assertion is held during dark wake, it is not honored.
 *               On Battery: This assertion is not honoured.
 *
 */
#define kIOPMAssertionTypeBackgroundTask            CFSTR("BackgroundTask")

/*! A caller should assert kIOPMAssertionTypeDenySystemSleep to keep the
 * system on (in dark wake mode) when it would otherwise go to sleep.
 * This is a synonym for the public assertion type <code>@link kIOPMAssertionTypePreventSystemSleep@/link</code>,
 * Please prefer to use the public API instead of this one.
 */
#define kIOPMAssertionTypeDenySystemSleep       CFSTR("DenySystemSleep")

/* Assertion 'kIOPMAssertionUserIsActive' powers on the display and 
 * prevents display from going dark.
 */
#define kIOPMAssertionUserIsActive            CFSTR("UserIsActive")

#ifndef _OPEN_SORCE
// When set; PM configd will ignore physical batteries, and instead
// will track software-generated fake batteries. 
// For debug & test support - the suffix "_Debug" reflects that this is for 
// Debug use only, and is not to be called from shipping code.
// Apple test tools are the only supported clients of this API.
#define kIOPMAssertionTypeDisableRealPowerSources_Debug	CFSTR("NoRealPowerSources_debug")
#endif

// Disables AC Power Inflow (requires root to initiate)
#define kIOPMAssertionTypeDisableInflow         CFSTR("DisableInflow")
#define kIOPMInflowDisableAssertion             kIOPMAssertionTypeDisableInflow

// Disables battery charging (requires root to initiate)
#define kIOPMAssertionTypeInhibitCharging       CFSTR("ChargeInhibit")
#define kIOPMChargeInhibitAssertion             kIOPMAssertionTypeInhibitCharging

// Disables low power battery warnings
#define kIOPMAssertionTypeDisableLowBatteryWarnings     CFSTR("DisableLowPowerBatteryWarnings")
#define kIOPMDisableLowBatteryWarningsAssertion         kIOPMAssertionTypeDisableLowBatteryWarnings

// Once initially asserted, the machine may only idle sleep while this assertion
// is asserted. For embedded use only.
#define kIOPMAssertionTypeEnableIdleSleep           CFSTR("EnableIdleSleep")


// Needs CPU Assertions - DEPRECATED
// Only have meaning on PowerPC machines.
#define kIOPMAssertionTypeNeedsCPU              CFSTR("CPUBoundAssertion")
#define kIOPMCPUBoundAssertion                  kIOPMAssertionTypeNeedsCPU


/*
 * Private Assertion Dictionary Keys
 */

#define kIOPMAssertionUsedDeprecatedCreateAPIKey              CFSTR("UsedDeprecatedCreateAPI")

/*! @constant kIOPMAssertionCreateDateKey
 *  @abstract Records the time at which the assertion was created.
 */
#define kIOPMAssertionCreateDateKey                 CFSTR("AssertStartWhen")

/*! @constant kIOPMAssertionReleaseDateKey
 *  @abstract Records the time that the assertion was released.
 *  @discussion The catch is that we only record the release time for assertions that have
 *  already timed out. In the normal create/release lifecycle of an assertion, we won't record
 *  the release time because we'll destroy the assertion object upon its release.
 */
#define kIOPMAssertionReleaseDateKey                CFSTR("AssertReleaseWhen")

/*! @constant kIOPMAssertionTimedOutDateKey
 *  @abstract Records the time that an assertion timed out.
 *  @discussion An assertion times out when its specified timeout interval has elapsed.
 *  This value only exists once the assertion has timedout. The presence of this 
 *  key/value pair in a dictionary indicates the assertion has timed-out.
 */
#define kIOPMAssertionTimedOutDateKey               CFSTR("AssertTimedOutWhen")

/*! @constant kIOPMAssertionTimeOutIntervalKey
 *  @abstract The owner-specified timeout for this assertion.
 */
#define kIOPMAssertionTimeOutIntervalKey            CFSTR("AssertTimeOutInterval")

/*! @constant kIOPMAssertionPIDKey
 *  @abstract The owning process's PID.
 */
#define kIOPMAssertionPIDKey                        CFSTR("AssertPID")

/*! @constant   kIOPMAssertionGlobalIDKey
 *  @abstract   A uint64_t integer that can uniuely identify an assertion system-wide.
 *
 *  @discussion kIOPMAssertionGlobalIDKey differs from the assertion ID (of type IOPMAssertionID) 
 *              returned by IOPMAssertionCreate* API. kIOPMAssertionGlobalIDKey can refer to 
 *              an assertion owned by any process. The 32-bit IOPMAssertionID only refers to 
 *              assertions within the process that created the assertion.
 *              You can use the API <code>@link IOPMCopyAssertionsByProcess@/link</code>
 *              to see assertion data for all processes.
 */
#define kIOPMAssertionGlobalUniqueIDKey             CFSTR("GlobalUniqueID")

/*!
 * @define          kIOPMAssertionAppliesToLimitedPowerKey
 *
 * @abstract        The CFDictionary key for assertion power limits in an assertion info dictionary.
 *
 * @discussion      The value for this key will be a CFBooleanRef, with value
 *                  <code>kCFBooleanTrue</code> or <code>kCFBooleanFalse</code>. A value of 
 *                  kCFBooleanTrue means that the assertion is applied even when system is running
 *                  on limited power source like battery. A value of kCFBooleanFalse means that
 *                  the assertion is applied only when the system is running on unlimited power
 *                  source like AC. 
 *
 *                  This property is valid only for assertion <code>@link kIOPMAssertionTypePreventSystemSleep @/link</code>. 
 *                  By default, this assertion is applied only when system is running on unlimited
 *                  power source. This behavior can be changed using this property.
 */

#define kIOPMAssertionAppliesToLimitedPowerKey  CFSTR("AppliesToLimitedPower")



/*!
 * @constant        kIOPMAssertionTimedOutNotifyString
 * @discussion      Assertion notify(3) string
 *                  Fires when an assertion times out.
 *                  Call IOPMCopyTimedOutAssertions() for a list of timed-out assertions.
 */
#define kIOPMAssertionTimedOutNotifyString          "com.apple.system.powermanagement.assertions.timeout"

/*!
 * @constant        kIOPMAssertionsAnyChangedNotifyString
 * @discussion      Assertion notify(3) string
 *                  Fires when any individual assertion is created, released, or modified.
 */
#define kIOPMAssertionsAnyChangedNotifyString         "com.apple.system.powermanagement.assertions.anychange"


/*!
 * @constant        kIOPMAssertionsChangedNotifyString
 * @discussion      Assertion notify(3) string
 *                  Fires when global assertion levels change. This notification doesn't necessarily fire
 *                  when any individual assertion is created, released, or modified.
 */
#define kIOPMAssertionsChangedNotifyString          "com.apple.system.powermanagement.assertions"

/*! 
 * @define          kIOPMAssertionTimeoutActionKillProcess
 *
 * @discussion      When a timeout expires with this action, Power Management will log the timeout event,
 *                  and will kill the process that created the assertion. Signal SIGTERM is issued to 
 *                  that process.
 */
#define kIOPMAssertionTimeoutActionKillProcess      CFSTR("TimeoutActionKillProcess")



/*! @function IOPMAssertionSetTimeout
 *  @abstract Set a timeout for the given assertion.
 *  @discussion When the timeout fires, the assertion will be logged and a general notification
 *  will be delivered to all interested processes. The notification will not identify the
 *  timed-out assertion. The assertion will remain valid & in effect until it's released (if ever).
 *  Timeouts are bad! Timeouts are a hardcore last chance for debugging hard to find assertion
 *  leaks. They are not meant to fire under any normal circumstances.
 */
IOReturn IOPMAssertionSetTimeout(IOPMAssertionID whichAssertion, CFTimeInterval timeoutInterval);

/*! @function IOPMCopyTimedOutAssertions
 *  @abstract Returns a CFArray of assertions (as CFDictionary's) that have timed out.
 *  @discussion Only the 5 most recent assertions are recorded.
 */
IOReturn IOPMCopyTimedOutAssertions(CFArrayRef *timedOutAssertions);

CFStringRef IOPMAssertionCreateTimeOutKey(void);
CFStringRef IOPMAssertionCreatePIDMappingKey(void);
CFStringRef IOPMAssertionCreateAggregateAssertionKey(void);

/*
 * Deprecated assertion constants
 */
#if TARGET_OS_EMBEDDED
    // RY: Look's like some embedded clients are still dependent on the following
    #define kIOPMPreventIdleSleepAssertion              kIOPMAssertionTypeNoIdleSleep
    #define kIOPMEnableIdleSleepAssertion               kIOPMAssertionTypeEnableIdleSleep
    enum {
        kIOPMAssertionDisable                           = kIOPMAssertionLevelOff,
        kIOPMAssertionEnable                            = kIOPMAssertionLevelOn,
        kIOPMAssertionIDInvalid                         = kIOPMNullAssertionID
     };
    #define kIOPMAssertionValueKey                      kIOPMAssertionLevelKey
#endif /* TARGET_OS_EMBEDDED */

/**************************************************
*
* Energy Saver Preferences - Constants
*
**************************************************/
/*!
 * @functiongroup PM Preferences
 */

#define kIOPMDynamicStoreSettingsKey                    "State:/IOKit/PowerManagement/CurrentSettings"
#define kIOPMDynamicStoreUserOverridesKey               "State:/IOKit/PowerManagement/UserOverrides"
#define kIOPMDefaultPreferencesKey                      "Defaults"

#define kIOPMUPSPowerKey                                "UPS Power"
#define kIOPMBatteryPowerKey                            "Battery Power"
#define kIOPMACPowerKey                                 "AC Power"

// units - CFNumber in minutes
#define kIOPMDisplaySleepKey                            "Display Sleep Timer"
// units - CFNumber in minutes
#define kIOPMDiskSleepKey                               "Disk Sleep Timer"
// units - CFNumber in minutes
#define kIOPMSystemSleepKey                             "System Sleep Timer"	

// units - CFNumber 0/1
#define kIOPMWakeOnLANKey                               "Wake On LAN"
// units - CFNumber 0/1
#define kIOPMWakeOnRingKey                              "Wake On Modem Ring"
// units - CFNumber 0/1
#define kIOPMRestartOnPowerLossKey                      "Automatic Restart On Power Loss"
// units - CFNumber 0/1
#define kIOPMWakeOnACChangeKey                          "Wake On AC Change"
// units - CFNumber 0/1
#define kIOPMSleepOnPowerButtonKey                      "Sleep On Power Button"
// units - CFNumber 0/1
#define kIOPMWakeOnClamshellKey                         "Wake On Clamshell Open"
// units - CFNumber 0/1
#define kIOPMReduceBrightnessKey                        "ReduceBrightness"
// units - CFNumber 0/1
#define kIOPMDisplaySleepUsesDimKey                     "Display Sleep Uses Dim"
// units - CFNumber 0/1
#define kIOPMMobileMotionModuleKey                      "Mobile Motion Module"
// units - CFNumber 0/1
#define kIOPMTTYSPreventSleepKey                        "TTYSPreventSleep"
// units - CFNumber 0/1
#define kIOPMGPUSwitchKey                               "GPUSwitch"
// units - CFNumber 0/1
#define kIOPMDarkWakeBackgroundTaskKey                  "DarkWakeBackgroundTasks"
// units - CFNumber 0/1
#define kIOPMSilentRunningKey                           "SilentRunning"
// units - CFNumber 0/1
#define kIOPMSleepServicesKey                           "SleepServices"

// Restart on Kernel panic
// Deprecated in 10.8. Do not use.
#define kIOPMRestartOnKernelPanicKey                    "RestartAfterKernelPanic"

// See xnu/iokit/IOKit/pwr_mgt/IOPM.h for other PM Settings keys:
//  kIOPMDeepSleepEnabledKey 
//  kIOPMDeepSleepDelayKey 

/*!
 * kIOPMPrioritizeNetworkReachabilityOverSleepKey
 *
 * mDNSResponder will transfer responsibility for publishing network services
 * to a Bonjour sleep proxy during sleep. 
 *      
 * This PM Feature has a value of:
 *      kCFBooleanTrue = If no sleep proxy is available, mDNSResponder will 
 *          prevent system sleep with a PM assertion to maintain network connections.
 *
 *      kCFBooleanFalse = If there is no sleep proxy available, the system will 
 *          fall asleep and prevent network service accessibility.
 *
 * mDNSResponder is responsible for acting upon this key.
 * value is false/true as a CFNumber 0/1
 */
#define kIOPMPrioritizeNetworkReachabilityOverSleepKey  "PrioritizeNetworkReachabilityOverSleep"


// kIOPMReduceSpeedKey
// Deprecated; do not use. Only relevant to PowerPC systems.
#define kIOPMReduceSpeedKey                             "Reduce Processor Speed"

// kIOPMDynamicPowerStepKey
// Deprecated; do not use. Only relevant to PowerPC systems.
#define kIOPMDynamicPowerStepKey                        "Dynamic Power Step"

/**************************************************
 *
 * Energy Saver Preferences - Functions
 *
 **************************************************/

typedef void (*IOPMPrefsCallbackType)(void *context);

/*!
 * @function    IOPMPrefsNotificationCreateRunLoopSource
 * @abstract    Returns a CFRunLoopSourceRef that notifies the caller when any PM preferences
 *              file changes - including Energy Saver or UPS changes.
 * @param       callback A function to be called whenever a prefs file changes.
 * @param       context Any user-defined pointer, passed to the IOPowerSource callback.
 * @result      Returns NULL if an error was encountered, otherwise a CFRunLoopSource. Caller must
 *              release the CFRunLoopSource.
 */
CFRunLoopSourceRef IOPMPrefsNotificationCreateRunLoopSource(IOPMPrefsCallbackType callback, void *context);

/* Structure of a "pm preferences" dictionary, as referenced below.
 *
 * PMPreferencesDictionary = 
 * {
 *      "AC Power" = PowerSourcePreferencesDictionary
 *      "Battery Power" = PowerSourcePreferencesDictionary
 *      "UPS Power" = PowerSourcePreferencesDictionary
 *      kIOPMDefaultPreferencesKey = true/false
 * }
 *
 * PowerSourcePreferencesDictionary = 
 * {
 *      kIOPMDiskSleepKey = 0 and up
 *      kIOPMSystemSleepKey = 0 and up
 *      kIOPMDisplaySleepKey = 0 and up
 *
 *      kIOPMWakeOnLANKey = 0/1
 *      kIOPMWakeOnRingKey = 0/1
 *      kIOPMWakeOnACChangeKey = 0/1
 *      ... etc
 * }
 *
 *
 * Notes: 
 * * If battery power or UPS power are not present on a system, they will
 *   be absent from the PM Preferences dictionary.
 * * If a given setting, like ReduceBrightness is unsupported on a given
 *   system, it will not appear in the PowerSourcePreferencesDictionary.
 * * If the property kIOPMDefaultPreferencesKey is present and set to true,
 *   then the user has not specified any custom PM preferences. These are all
 *   "hardcoded" default properties. As such, the settings in a default custom
 *   dict do NOT factor in power profile settings.
 *
 * * See IOPMCopyActivePMPreferences() for settings that account for PM profile
 *   selections.
 */

    /*!
@function IOPMCopyCustomPMPreferences
@abstract Returns a dictionary of user-selected custom PM preferences. 
    The CFString key kIOPMDefaultPreferencesKey will be present in the top-level 
    dictionary if the returned settings are defaults, as in the case of a clean 
    install
@result Returns a CFDictionaryRef or NULL. Caller must CFRelease the dictionary.
     */
CFDictionaryRef     IOPMCopyCustomPMPreferences(void);

/*!
@function IOPMCopyPMPreferences
@abstract Identical to IOPMCopyCustomPMPreferences; Renamed to 
    IOPMCopyCustomPMPreferences for clarity.
 */
CFMutableDictionaryRef IOPMCopyPMPreferences(void);


/*!
@function IOPMCopyActivePMPreferences
@abstract Returns the _actual_ PM settings _currently_ in use by the system.
@discussion Incorporates the active power profiles as well as custom settings.
        Does not return any settings unsupported on the running computer.
 */
CFDictionaryRef     IOPMCopyActivePMPreferences(void);

/*!
@function IOPMCopyUnabridgedActivePMPreferences
@abstract Returns the _actual_ PM settings _currently_ in use by the system.
@discussion Behaves identically to IOPMCopyActivePMPreferences, except 
        IOPMCopyUnabridgedActivePMPreferences returns all settings, including
        those settings unsupported on the running machine.
        i.e. the returned dictionary will include a setting for WakeOnRing
        on a computer without a modem.
 */
CFDictionaryRef     IOPMCopyUnabridgedActivePMPreferences(void);


    /*!
@function IOPMSetCustomPMPreferences
@abstract Writes a PM preferences dictionary to disk.
    Also activates these preferences and sends notifications to interested 
    applications via SystemConfiguration.
@param ESPrefs  Dictionary of Power Management preferences to write out to disk.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOPMSetCustomPMPreferences(CFDictionaryRef ESPrefs);

/*!
@function IOPMSetPMPreferences
@abstract Identical to IOPMSetCustomPMPreferences. Renamed to 
    IOPMSetCustomPMPreferencs for clarity.
 */
IOReturn IOPMSetPMPreferences(CFDictionaryRef ESPrefs);

    /*!
@function IOPMActivatePMPreference
@abstract Activates the set of preferences called "name." Sets idle spin down timers and other
    Energy Saver settings according to profile name.
@param SystemProfiles  The dictionary of preferences from IOPMCopyPMPreferences
@param profile The name of the set of preferences defined in ESPrefs to activate.
@param removeUnsupportedSettings Specifies whether to send settings to kernel for
settings that are currently unsupported (i.e. WakeOnRing with no modem present). True
means do not activate unsupported settings, false means DO activate unsupported settings.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOPMActivatePMPreference(
    CFDictionaryRef SystemProfiles, 
    CFStringRef profile,
    bool removeUnsupportedSettings);

/*!
 * @function        IOPMFeatureIsAvailable
 * @abstract        Identifies whether a PM feature is supported on this platform.
 * @param           feature The CFString feature to check for availability.
 * @param           power_source CFSTR(kIOPMACPowerKey) or CFSTR(kIOPMBatteryPowerKef). Doesn't 
 *                  matter for most features, but a few are power source dependent.
 * @result          Returns true if supported, false otherwise. 
 */
bool IOPMFeatureIsAvailable(CFStringRef feature, CFStringRef power_source);


/*!
 * @function        IOPMFeatureIsAvailablewithSupportedTable
 * @abstract        Like <code>@link IOPMFeatureIsAvailable@/link </code> but lets caller pass in a 
 *                  cached supported-features array.
 * @discussion      Provides an optimized path if you'll be making repeated calls to IOPMFeatureIsAvailable.
 *                  The "Supported features table" does not change often - it may change during boot-up, and it
 *                  may change when hardware is added or removed to the system.
 *
 *                  By providing a cached copy here, IOPMFeatureIsAvailable can avoid re-reading the feature support
 *                  table on every invocation.
 *
 *                  If you are only occasionally invoking IOPMFeatureIsAvailable for 1-2 settings, you don't need
 *                  to bother with IOPMFeatureIsAvailableWithSupportedTable. Look into it for a performance optimization
 *                  if you do make multiple calls.
 *
 * @param           feature The CFString feature to check for availability.
 *
 * @param           power_source CFSTR(kIOPMACPowerKey) or CFSTR(kIOPMBatteryPowerKef). Doesn't 
 *                  matter for most features, but a few are power source dependent.
 *
 * @param           supportedFeatures Pass in a CFDictionary describing available features, and which power sources
 *                  each supports. Obtain this dictionary from IOPMrootDomain in the IORegistry with code like this:
 *
 *                  io_registry_entry_t _rootDomain = 
 *                                   IORegistryEntryFromPath( kIOMasterPortDefault, 
 *                                   kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
 *                  CFDictionaryRef cachedSupportedFeatures =  
 *                                  IORegistryEntryCreateCFProperty(_rootDomain, 
 *                                      CFSTR("Supported Features"), 
 *                                      kCFAllocatorDefault, kNilOptions);
 *
 * @result          Returns true on if supported, false otherwise.
 */
bool IOPMFeatureIsAvailableWithSupportedTable(
     CFStringRef                PMFeature, 
     CFStringRef                power_source,
     CFDictionaryRef            supportedFeatures);

/**************************************************/
/*!
 * @function IOPMOverrideDefaultPMPreferences
 *
 * @discussion
 * Caller specifies a set of default energy saver preferences for this machine.
 * The system will use these settings as defaults, until the user selets a custom preference
 * via pmset, Enengy Saver, or elsewhere IOPMSetPMPreferences API.
 *
 * Use this API to specify a default setting value at runtime for the platform you're running on.
 * For instance, hibernatemode = 0 on dekstops, and hibernatemode = 3 on portables. That distinction
 * is made by IOPlatformExpert in the kernel, in an analogue to this API.
 *
 * If multiple calls to IOPMOverrideDefaultPMPreferences affect the same preference, behavior is undefined.
 *
 * @param overrideSettings
 * Caller may specify the new defaults one of two ways. PM excepts both of these styles of dictionaries:
 *
 * (1) Specify a setting that applies to all power sources.
 *
 * dictionary = {
 *      Setting" = Object
 * }
 *
 * (2) Specify a setting that may have different falues for each of AC, Battery, or UPS.
 *
 *  dictionary = {
 *      "AC" = {
 *          "Setting" = "Object"
 *      },
 *      "Battery" = {
 *          "Setting" = "Object"
 *      },
 *      "UPS" = {
 *          "Setting" = "Object"
 *      }		
 *  }
 *
 */
void IOPMOverrideDefaultPMPreferences(CFDictionaryRef overrideSettings);
/**************************************************/


    /*!
    @function IOPMSleepSystemWithOptions
    @abstract Request that the system initiate sleep.
    @discussion For security purposes, caller must be root or the console user.
    @param userClient  Port used to communicate to the kernel,  from IOPMFindPowerManagement.
    @param sleepOptions a dictionary defining optional arguments to sleep.
    @result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOPMSleepSystemWithOptions ( io_connect_t userClient, CFDictionaryRef sleepOptions );

/**************************************************/

/* 
 * kIOPMSystemSleepAvailableAtAll
 *      System Power Setting (not power source specific)
 *      value = true/false
 */
#define	kIOPMSleepDisabledKey	CFSTR("SleepDisabled")

    /*!
@function IOPMCopySystemPowerSettings
@abstract Returns System power settings. 
          System-wide power settings are not power source dependent.
     */
CFDictionaryRef IOPMCopySystemPowerSettings( void );

    /*
@function IOPMSetSystemPowerSetting
@abstract Set a system-wide power management setting
@param key Setting name
@param value Setting value
@result kIOReturnSuccess; or IOReturn error otherwise
     */
IOReturn IOPMSetSystemPowerSetting( CFStringRef key, CFTypeRef value );

    /*!
@function IOPMActivateSystemPowerSettings
@abstract Re-send system power settings to kernel.
@result   kIOReturnSuccess on success; IOReturn error otherwise
     */
IOReturn IOPMActivateSystemPowerSettings( void );


/**************************************************
*
* Power Profiles (use in combination with Energy Saver Preferences above)
*
**************************************************/
/*!
 * @functiongroup Power Profiles
 */

#define kIOPMCustomPowerProfile         -1
#define kIOPMNumPowerProfiles           5

/*! @function IOPMCopyPowerProfiles
    @abstract Returns all power profiles and their corresponding Energy Settings.
    @discussion The array return value contains 5 dictionaries.
        - Entry 0 represents the "highest power savings", and entry 4 represents "Highest performance"
        - Each entry in the array is a "Power Profile" dictionary, which in turn contains individual
        dictionaries for AC Power, Battery Power, and UPS Power. Each of these per-power source
        dictionaries contains a mapping of Energy Settings to their values.
        Intended clients: Energy Saver Prefs, Battery Monitor, and pmset.
        - Use IOPMCopyPMPreferences() to read the custom profile.
        - Unsupported features and unsupported power sources will not be present in the returned data.
    @result NULL on error, a CFArrayRef on success.
        Caller must CFRelease() the return value when done.
*/
CFArrayRef          IOPMCopyPowerProfiles(void);

/*! @function IOPMCopyActivePowerProfile
    @abstract Returns the index of the currently active profile, or -1 if Custom preferences are active.
    @result A CFDictionary containing 1 or more entries mapping Power Sources to the currently
            selected profiles for each power source. kIOPMUPSPowerKey, kIOPMACPowerKey, kIOPMBatteryPowerKey
            With corresponding CFNumber value specifying an index falling somewhere within the 
            IOPMCopyPowerProfiles() array
            OR integer value kIOPMCustomPowerProfile.
*/
CFDictionaryRef     IOPMCopyActivePowerProfiles(void);

/*! @function IOPMSetActivePowerProfile
    @abstract Activates the specified power profile, or the user's custom defined profile.
    @discussion Caller must have root, or admin privileges, or be the console user.
        Selects and activates a system power profile per power-source.
        - Use IOPMSetPMPreferences() to re-program the custom profile.
        - This call triggers notifications to all clients of IOPMPrefsNotificationCreateRunLoopSource()
    @param which_profile A CFDictionary specifying which profile to use for each power source.
        Keys should be CFSTR: kIOPMUPSPowerKey, kIOPMACPowerKey, kIOPMBatteryPowerKey
        Value should be a CFNumber (IntType) with value -1 to 4
    @result kIOReturnNotPrivileged if caller does not have permission.
        Caller must CFRelease() the return value when done accessing it.
*/
IOReturn            IOPMSetActivePowerProfiles(CFDictionaryRef which_profile);


/**************************************************
*
* Repeating Sleep/Wake/Shutdown/Restart API
*
**************************************************/
/*!
 * @functiongroup Repeating power events
 */

// Keys to index into CFDictionary returned by IOPSCopyRepeatingPowerEvents()
#define     kIOPMRepeatingPowerOnKey        "RepeatingPowerOn"
#define     kIOPMRepeatingPowerOffKey       "RepeatingPowerOff"

#define     kIOPMAutoSleep                  "sleep"
#define     kIOPMAutoShutdown               "shutdown"

// Keys to "days of week" bitfield for IOPMScheduleRepeatingPowerEvent()
enum {
    kIOPMMonday         = 1 << 0,
    kIOPMTuesday        = 1 << 1,
    kIOPMWednesday      = 1 << 2,
    kIOPMThursday       = 1 << 3,
    kIOPMFriday         = 1 << 4,
    kIOPMSaturday       = 1 << 5,
    kIOPMSunday         = 1 << 6
};

// Keys to index into sub-dictionaries of the dictionary returned by IOPSCopyRepeatingPowerEvents
// Absolute time to schedule power on (stored as a CFNumberRef, type = kCFNumberIntType)
//#define kIOPMPowerOnTimeKey                 "time"
// Bitmask of days to schedule a wakeup for (CFNumberRef, type = kCFNumberIntType)
#define kIOPMDaysOfWeekKey                  "weekdays"
// Type of power on event (CFStringRef)
//#define kIOPMTypeOfPowerOnKey               "typeofwake"

/*  @function IOPMScheduleRepeatingPowerEvent
 *  @abstract Schedules a repeating sleep, wake, shutdown, or restart
 *  @discussion Private API to only be used by Energy Saver preferences panel. Note that repeating sleep & wakeup events are valid together, 
 *              and shutdown & power on events are valid together, but you cannot mix sleep & power on, or shutdown & wakeup events.
 *              Every time you call IOPMSchedueRepeatingPowerEvent, we will cancel all previously scheduled repeating events of that type, and any
 *              scheduled repeating events of "incompatible" types, as I just described.
 *  @param  events A CFDictionary containing two CFDictionaries at keys "RepeatingPowerOn" and "RepeatingPowerOff".
                Each of those dictionaries contains keys for the type of sleep, the days_of_week, and the time_of_day. These arguments specify the
                time, days, and type of power events.
 *  @result kIOReturnSuccess on success, kIOReturnError or kIOReturnNotPrivileged otherwise.
 */
IOReturn IOPMScheduleRepeatingPowerEvent(CFDictionaryRef events);

/*  @function IOPMCopyRepeatingPowerEvents
 *  @abstract Gets the system
 *  @discussion Private API to only be used by Energy Saver preferences panel. Copies the system's current repeating power on
                and power off events.
                The returned CFDictionary contains two CFDictionaries at keys "RepeatingPowerOn" and "RepeatingPowerOff".
                Each of those dictionaries contains keys for the type of sleep, the days_of_week, and the time_of_day.
 *  @result NULL on failure, CFDictionary (possibly empty) otherwise.
 */
 CFDictionaryRef IOPMCopyRepeatingPowerEvents(void);

/*  @function IOPMScheduleRepeatingPowerEvent
 *  @abstract Cancels all repeating power events
 *  @result kIOReturnSuccess on success, kIOReturnError or kIOReturnNotPrivileged otherwise.
 */ 
IOReturn IOPMCancelAllRepeatingPowerEvents(void);



/**************************************************
*
* Power API - For userspace power & thermal drivers
*
**************************************************/

/* @function IOPMSystemPowerEventOccurred
    @abstract Called to alert the system of a power or thermal event
    @param eventType A CFStringRef defining the type of event
    @param eventObject The CF payload describing the event
    @result kIOReturnSuccess; kIOReturnNotPrivileged, kIOReturnInternalError, or kIOReturnError on error.
*/
IOReturn IOPMSystemPowerEventOccurred(
        CFStringRef typeString, 
        CFTypeRef eventValue);

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************
*
* Sleep/Wake Bookkeeping API
*
* Returns timing details about system level power events.
*
******************************************************************************/
/*!
 * @functiongroup IOPM UUID Bookkeeping
 */

/*! 
 *  @function   IOPMGetLastWakeTime 
*
 *  @abstract   returns the timestamp of the last system wake.
 *
 *  @discussion If possible, the wakeup time is adjusted backward using hardware
 *              timers, so that whenWake refers to the time of the physical event
 *              that woke the system.
 *
 *  @param      whenWoke On successful return, whenWoke will contain the CFAbsoluteTime,
 *              in the CF time scale, that the system awoke. This time is already adjusted
 *              with the SMC wake delta, when available.
 *
 *  @param      adjustedForPhysicalWake Returns the interval (in CF time units of seconds) that
 *              PM adjusted the wakeup time by. If the system doesn't support the HW timers
 *              required to adjust the time, this value will be zero.
 *
 *  @result     Returns kIOReturnSuccess; all others returns indicate a failure; the returned
 *              time values should not be used upon a failure return value.
 */
IOReturn IOPMGetLastWakeTime(
        CFAbsoluteTime      *whenWoke,
        CFTimeInterval      *adjustedForPhysicalWake);

/*! IOPMSleepWakeSetUUID
 *  Sets the upcoming sleep/wake UUID. The kernel will cache
 *  this and activate it the next time the system goes to sleep.
 *  Pass a CFStringRef to set the upcoming UUID.
 *  Pass NULL to indicate that the current UUID should be cancelled.
 *  Only should be called by PM configd & loginwindow.
 */
IOReturn IOPMSleepWakeSetUUID(CFStringRef newUUID);
 
 
/*!
 *
 * kIOPMSleepWakeUUID
 * Argument to <code>@link IOPMGetUUID @/link</code>
 *
 * kIOPMSleepServicesUUID
 * Argument to <code>@link IOPMGetUUID @/link</code>
 *
 */
enum {
    kIOPMSleepWakeUUID      = 1000,
    kIOPMSleepServicesUUID  = 1001
};

/*! 
 *  IOPMGetUUID
 *  
 *  Returns a C string that uniquely describes the "sleep interval"
 *  that the system is in right now.
 *
 *  @param whichUUID      
 *  The argument may be <code>kIOPMSleepWakeUUID</code>, to get the sleep/wake UUID,
 *  or <code>kIOPMSleepServicesUUID</code> to get the sleep/wake UUID.
 *
 *  OS X creates a new SleepWakeUUID when the system goes to sleep. OS X clears
 *  it several minutes after the system wakes from sleep. 
 *  Every time OS X clears the SleepWakeUUID, it creates a new UUID. So there
 *  are always valid SleepWake UUID's defined, even when the machine is awake.
 *  Calls to <code>IOPMGetUUID()</code> for type <code>kIOPMSleepWakeUUID</code> 
 *  should in all non-error cases return true, and return a valid UUID string 
 *  in <code>putTheUUIDHere</code>. 
 *
 *  OS X creates a new SleepServiceUUID at the moment that process 
 *  <code>SleepServiceD</code> decides that a given wakeup shall be bound by 
 *  SleepService cap timers.
 *  OS X clears the SleepServiceUUID when the system exits SleepServices.
 *  At many points during a system's runtime, calling <code>IOPMGetUUID</code>
 *  for <code>kIOPMSleepServicesUUID</code> may result in a return value
 *  of false, and no string being copied into the passed-in buffer.
 *
 *  @param putTheUUIDHere
 *  Caller provides a pointer to a buffer of sufficient size to receive 
 *  a C string UUID. I recommend 100 bytes, because that's about 3x what
 *  you should need. 
 *
 *  @param sizeOfBuffer
 *  Caller must specify the size of the buffer they pass in as argument
 *  <code>putTheUUIDHere</code>
 *
 *  @result
 *  Returns true if successfully copied the UUID into the provided buffer.
 *  Returns false if there was an error, or if there was no buffer.
 *
 *  @discussion
 *  For example:
 *          char my_buffer[100];
 *
 *          if(IOPMGetUUID(kIOPMSleepServicesUUID, my_buffer, sizeof(my_buffer)))
 *          {
 *              printf("my UUID is = %s", my_buffer);
 *           } else {
 *              printf("There is no such UUID defined right now.");
 *          }
 *  
 */

bool IOPMGetUUID(int whichUUID, char *putTheUUIDHere, int sizeOfBuffer);
 
/*! IOPMSleepWakeCopyUUID
 *  Returns the active sleep wake UUID, if it exists.
 *  The returned UUID is only useful for logging events that occurred during
 *  the given sleep/wake.
 *  Returns a CFString containing the UUID. May return NULL if there is no UUID.
 *  Unless the result is NULL, caller must release the result.
 */
CFStringRef IOPMSleepWakeCopyUUID(void);

/*
 * IOPMSetSleepServicesWakeTimeCap
 *
 * Only SleepServicesD should call this SPI. If you are not writing code for
 * SleepServicesD, do not call this SPI.
 *
 * This SPI allows changing the Sleep services wake duration time cap. 
 * On receiving this request, powerd ignores the existing time cap and
 * sets the time cap to specified duration starting from point when
 * the request is received.
 *
 * The initial sleep services time cap still need to be sent to powerd 
 * thru the IOPMConnectionAcknowledgeEventWithOptions() API. This SPI
 * IOPMSetSleepServicesWakeTimeCap() should be used only to change the
 * existing time cap.
 *
 * The request is ignored if issued when there is no existing time cap
 * already set or if issued when system is not in sleep services wake.
 *
 * @param cap
 * This specifies the new cap time in milliseconds.
 *
 *  @result
 *  Returns kIOReturnSuccess if new time cap is set successfully.
 */
IOReturn IOPMSetSleepServicesWakeTimeCap(CFTimeInterval cap);
// Poweranagement's 1-byte status code
#define kIOPMSleepWakeFailureKey            "PMFailurePhase"

// PCI Device failure name
// Will indicate which driver-subtree it occured in 
// If the failure took place during a driver sleep/wake phase,
#define kIOPMSleepWakeFailureDriverTreeKey  "PCIDeviceFailure"

// LoginWindow's 1-byte status code
// Will indicate LoginWindow's role in putting up the security panel 
//  if the failure took place during sleep phase:
//  kIOPMTracePointSystemLoginwindowPhase = 0x30
#define kIOPMSleepWakeFailureLoginKey       "LWFailurePhase"

// The UUID of the sleep that failed
#define kIOPMSleepWakeFailureUUIDKey        "UUID"

// The date of the attempted sleep that resulted in failure.
#define kIOPMSleepWakeFailureDateKey        "Date"

// PowerManagement's 8-byte failure descriptor.
// Do not reference this key, instead references the decoded data
// stored under other keys in this dictionary.
#define kIOPMSleepWakeFailureCodeKey        "PMStatusCode"


/*
Ê* For use by LoginWindow only.
Ê* facility - Pass CFSTR(kIOPMLoginWindowSecurityDebugKey) for the facility
Ê* data - Pass a pointer to a variable containing one byte of data.
Ê* dataCount - Pass the integer 1.
Ê*/
IOReturn IOPMDebugTracePoint(
        CFStringRef     facility, 
        uint8_t         *data, 
        int             dataCount);


/*
Ê* Returns data describing the sleep failure (if any) that occured prior to the system booting.
Ê*
Ê* This routine will return the same CFDictionary over the lifetime of a given boot - it does not return
Ê* dynamic information after each sleep/wake. It only returns information pertaining to the last
Ê* failed sleep/wakeÊbefore booting up.
Ê*Ê
Ê* If NULL, then the last sleep/wake was successful, or we were unable to determine whether
Ê* there was a problem.
Ê* If non-NULL, Caller must release the returned dictionary.
Ê*
Ê* kIOPMSleepWakeFailureLoginKey points to a CFNumber containing LW's 8-bit code.
Ê* kIOPMSleepWakeFailureUUIDKey points to a CFStringRef containing the UUID associated with the failed sleep.
Ê* kIOPMSleepWakeFailureDateKey points to the CFDate that the failed sleep was initiated.
Ê*/
CFDictionaryRef IOPMCopySleepWakeFailure(void);


/**************************************************
*
* SW-generated HID events history
*
* OS PM records software generated HID events by process, for
* later associating HID events that are preventing display sleep
* and system sleep with the processes that created them.
*
**************************************************/

typedef struct {
    CFAbsoluteTime  eventWindowStart;
    uint32_t        hidEventCount;  // MOUSE, KEYBOARD, or other NON-NULL event
    uint32_t        nullEventCount; // NX_NULL_EVENT
} IOPMHIDPostEventActivityWindow;

/*!
 * Key into process HID activity dictionary. 
 * The value is the path to the caller's
 * executable path.
 */
#define kIOPMHIDAppPathKey        CFSTR("AppPathString")

/*!
 * Key into process HID activity dictionary.
 * The value is a CFNumber integer - the caller's pid.
 */
#define kIOPMHIDAppPIDKey       CFSTR("AppPID")

/*!
 * Key into process HID activity dictionary.
 * The value is a CFArray of recent HID activity. The CFArray contains
 * CFData blocks, each with size = sizeof(IOHIDPostEventActivityWindow)
 * Each block summarizing a 5 minute window of HID activity.
 */
#define kIOPMHIDHistoryArrayKey   CFSTR("HIDHistoryArray")

/*!
 * IOPMCopyHIDPostEventHistory
 * The returned CFArray contains one dictionary per HID-posting process.
 *      CFDictionary contains
 *          key = AppPID
 *          CFNumberRef for pid
 *          key = AppPathString
 *          CFStringRef path to pid's executable (program name)
 *          key = HIDHistoryArray
 *          CFArray of CFData
 *              Each CFData represents a "bucket" of IOHPMIDPostEventActivityWindow, 
 *                  representing a 5 minute window of HID activity.
 * @param outDictionary On success, returns a valid CFDictionaryRef. The caller must release this array.
 *      Will return NULL otherwise.
 * @result kIOReturnSuccess on success; kIOReturnNotFound if HID event logging is disabled;
 *      kIOReturnError for all others errors.
 */
IOReturn IOPMCopyHIDPostEventHistory(CFArrayRef *outDictionary);

/**************************************************
*
* IOPM Power Details log
*
* Logs fine-grained details about per-driver sleep/wake activity
* into a in-memory buffer. PowerManagement translates that buffer
* into a CFType-based representation of all driver power activity
* during sleep/wake.
*
* These calls may take upwards of 1 second to execute. Do not call
* any of these "PowerDetails" calls from a UI thread.
*
**************************************************/
#define kIOPMPowerHistoryEventTypeKey                   "EventType"
#define kIOPMPowerHistoryEventReasonKey                 "EventReason"
#define kIOPMPowerHistoryEventResultKey                 "EventResult"
#define kIOPMPowerHistoryDeviceNameKey                  "DeviceName"
#define kIOPMPowerHistoryUUIDKey                        "UUID"
#define kIOPMPowerHistoryInterestedDeviceNameKey        "InterestedDeviceName"
#define kIOPMPowerHistoryTimestampKey                   "Timestamp"
#define kIOPMPowerHistoryDeviceUniqueIDKey              "DevinceUnique"
#define kIOPMPowerHistoryElapsedTimeUSKey               "ElapsedTimeUS"
#define kIOPMPowerHistoryOldStateKey                    "OldPowerState"
#define kIOPMPowerHistoryNewStateKey                    "NewPowerState"
#define kIOPMPowerHistoryTimestampCompletedKey          "ClearTime"
#define kIOPMPowerHistoryEventArrayKey                  "EventArray"

/*
 *  The timestamps that IOPMCopyPowerHistory returns are CFStrings that need
 *  to be interpreted in this Unicode date format pattern. They can be 
 *  converted to CFAbsoluteTime or otherwise, using this format string
 */
#define kIOPMPowerHistoryTimestampFormat             "MM,dd,yy HH:mm:ss zzz"

/*
 * Returns a list of major power events (sleep, wake) in chronological order.
 * On failure, IOReturn will reflect the type of failure.
 * On success, caller must release returned array.
 *
 * Input: A CFArrayRef into which the series of power events is copied into
 * 
 * Output: A filled CFArrayRef with details of all major power events. Caller
 *         is responsible for releasing the filled array at some later point
 *
 * Returns kIOReturnSuccess on success, error code of type IOReturn otherwise
 *
 * NOTE: The CFArray returned consists of individual CFDictionaries. Each
 * CFDictionary within the array has the following structure
 *
 * CFDictionary {
 *   {
 *     key: kIOPMPowerHistoryUUIDKey 
 *     value: UUID of power event cluster (CFString)
 *   }
 *   {
 *     key: kIOPMPowerHistoryTimestampKey
 *     value: Time marking the beginning of this power event cluster (CFString)
 *   }
 *   {
 *     key: kIOPMPowerHistoryTimestampCompletedKey
 *     value: Timae marking the end of this power event cluster (CFString)
 *   }
 * }
 *
 * The timestamps returned as CFStrings can be interpreted/parsed using 
 * kIOPMPowerHistoryTimestampFormat as defined above
 */
IOReturn IOPMCopyPowerHistory(CFArrayRef *outArray);

/*
 * When available, returns details of driver activity that occurred 
 * during a larger power event.
 * 
 * Input: A UUID representing a power event. 
 *        Obtained from IOPMCopyPowerHistory.
 * 
 * Output: A CFDictionaryRef of detailed power events that occurred during 
 *         that UUID. Possibly NULL. 
 *         On success, caller should release the returned array.
 * 
 * Returns: kIOReturnSuccess on success; error code otherwise.
 *
 * NOTE: The CFDictionary returned by this function has the following
 *        structure:
 *  
 * CFDictionary {
 *   {
 *     key: kIOPMPowerHistoryUUIDKey
 *     value: UUID identifier for this cluster of power events (CFString)
 *   }
 *   {
 *     key: kIOPMPowerHistoryTimestampKey
 *     value: Time marking the beginning of the power event cluster 
 *            Note that timestamps are returned as CFAbsoluteTime in this
 *            method (unlike in IOPMCopyPowerHistory) to avoid loss of 
 *            precision (CFAbsoluteTime).
 *   }
 *   {
 *     key: kIOPMPowerHistoryTimestampCompletedKey
 *     value: Time marking the end of the power event cluster
 *            Note that timestamps are returned as CFAbsoluteTime in this
 *            method (unlike in IOPMCopyPowerHistory) to avoid loss of 
 *            precision (CFAbsoluteTime).
 *   }
 *   {
 *     key: kIOPMPowerHistoryEventArrayKey
 *     value: An array containing details of individual power events within
 *            the larger cluster (CFArray). The details of each fine-grained
 *            power event is placed in the form of a CFDictionary inside 
 *            this array. The structure of this CFDictionary is explained 
 *            below
 *   }
 * }
 *
 * 
 * Each CFDictionary (which describes a fine-grained power state change) 
 * within the Power events array is structured as:
 *
 * CFDictionary {
 *   {
 *     key: kIOPMPowerHistoryTimestampKey
 *     value: Time at which the given state change took place (CFAbsoluteTime)
 *   }
 *   {
 *     key: kIOPMPowerHistoryEventReasonKey
 *     value: Reason for the state change, if any (CFNumber)
 *   }
 *   {
 *     key: kIOPMPowerHistoryEventResultKey
 *     value: Result of the given state change, if any
 *   }
 *   {
 *     key: kIOPMPowerHistoryDeviceNameKey
 *     value: Name of the device that's changing state (CFString)
 *   }
 *   {
 *     key: kIOPMPowerHistoryUUIDKey
 *     value: UUID of the larger power event cluster that this state change
 *            belongs to (CFString)
 *   }
 *   {
 *     key: kIOPMPowerHistoryInterestedDeviceNameKey
 *     value: Name of any other device that's interested in watching this 
 *            power state change (CFStringRef)
 *   }
 *   {
 *     key: kIOPMPowerHistoryOldStateKey
 *     value: The old power state from which the device is changing out of
 *            (CFNumber)
 *   }
 *   {
 *     key: kIOPMPowerHistoryNewStateKey
 *     value: The new power state the device is changing into (CFNumber)
 *   }
 *   {
 *     key: kIOPMPowerHistoryTimeElapsedUSKey
 *     value: The total time taken for the power state transition to
 *             complete, measured in microseconds (CFNumber)  
 *   }
 * }
 */
IOReturn IOPMCopyPowerHistoryDetailed(CFStringRef UUID, CFDictionaryRef *outDict);

/*! Set a Power event bookmark from the command line
 *
 * This method flushes out the current UUID cluster of power events on demand
 * and bookmarks the beginning of a new UUID cluster
 * The caller is returned the UUID associated with the brand new UUID cluster
 *      
 *
 * Input: A string that is returned with the new UUID in use. this string has
 *        to have its memory pre-allocated. Recommended length for such a 
 *        string that is passed in is 255 characters or higher
 *
 * Output: Returns the new UUID identifier which has been put to use
 *
 * Result: Returns kIOReturnSuccess on success, an error code otherwise
 */
IOReturn IOPMSetPowerHistoryBookmark(char  *uuid);

/**************************************************
*
* IOPMConnection API
*
* IOPMConnection API lets user processes receive
* power management notifications.
*
**************************************************/
/*!
 * @functiongroup IOPMConnection
 */
 
 
/*! 
 * IOPMConnection is the basic connection type between a user process
 * and system power management.
 * IOPMConnections are backed by mach ports, and are automatically cleaned up
 * on process death.
 */
typedef const struct __IOPMConnection * IOPMConnection;

/*! 
 * A unique IOPMConnectionMessageToken accompanies each PM message received from
 * an IOPMConnection notification. This message token should be passed as an argument
 * to IOPMConnectionAcknowledgeEvent().
 */
typedef uint32_t IOPMConnectionMessageToken;


/*****************************************************************************/
/*****************************************************************************/

/*! @enum IOPMSystemPowerStateCapabilities
 *
 * Bits define capabilities in IOPMSystemPowerStateCapabilities type.
 *
 * These bits describe the capabilities of a system power state.
 * Each bit describes whether a capability is supported; it does not
 * guarantee that the described feature is available. Even if a feature
 * is supported, the client must still verify that it's accessible
 * before attempting to use it, and be prepared for an error if the
 * functionality is not accessible.
 *
 * Please use these bits to:
 *      - Specify the capabilities you're interested in in calls to
 *          IOPMConnectionCreate()
 *      - Interpret the power states passed to you in your IOPMEventHandlerType
 *          notification.
 */
enum 
{
    /*! @constant kIOPMSystemPowerStateCapabilityCPU
     *  If set, indicates that in this power state the CPU is running. If this bit is clear,
     *  then the system is going into a low power state such as sleep or hibernation. 
     *  Checking this bit 
     *
     *  The CPU capability bit must be set for any other capability bits (video, audio, 
     *  network, disk, etc.) to be available as well.
     */
    kIOPMSystemPowerStateCapabilityCPU          = 0x1,

    /*! kIOPMSystemPowerStateCapabilityVideo
     * If set, indicates that in this power state, graphic output to displays are supported.
     */
    kIOPMSystemPowerStateCapabilityVideo        = 0x2,

    /*! kIOPMSystemPowerStateCapabilityAudio
     * If set, indicates that in this power state, audio output is supported.
     */
    kIOPMSystemPowerStateCapabilityAudio        = 0x4,

    /*! kIOPMSystemPowerStateCapabilityNetwork
     * If set, indicates that in this power state, network connections are supported.
     */
    kIOPMSystemPowerStateCapabilityNetwork      = 0x8,

    /*! kIOPMSystemPowerStateCapabilityDisk
     * If set, indicates that in this power state, internal disk and storage device access is supported.
     */
    kIOPMSystemPowerStateCapabilityDisk         = 0x10,

    /*! kIOPMSystemPowerStateCapabiliesMask
     *  Should be used as a mask to check for states; this value should not be 
     *  passed as an an argument to IOPMConnectionCreate. 
     *  Passing this as an interest argument will produce undefined behavior.
     *
     *  Any PowerStateCapability bits that are not included in this mask are 
     *  reserved for future use.
     */
    kIOPMSytemPowerStateCapabilitiesMask        = 0x1F
};

/*! IOPMSystemPowerStateCapabilities
 *  Should be a bitfield with a subset of the kIOPMSystemPowerStateCapabilityBits.
 */
typedef uint32_t IOPMSystemPowerStateCapabilities;



/*!
 * IOPMEventHandlerType is the generic function type to handle a
 *   notification generated from the power management system. All clients of
 *   IOPMConnection that wish to listen for notifications must provide a handler
 *   when they call IOPMConnectionCreate.
 * @param param Pointer to user-chosen data.
 * @param connection The IOPMConnection associated with this notification.
 * @param token Uniquely identifies this message invocation; should be passed
 *          to IOPMConnectAcknowledgeEvent().
 * @param eventDescriptor Provides a bitfield describing the new system power state.
 *          See IOPMSystemPowerStateCapabilities below.
 */
typedef void (*IOPMEventHandlerType)(
                void *param, 
                IOPMConnection connection, 
                IOPMConnectionMessageToken token, 
                IOPMSystemPowerStateCapabilities eventDescriptor);


/*****************************************************************************/
/*****************************************************************************/

/*! kIOPMAckNetworkMaintenanceWakeDate
 *
 *  This string can be used as a key in the 'options' dictionary
 *  argument to IOPMConnectionAcknowledgeEventWithOptions. Caller
 *  should push a CFDate value to match this key.
 *
 *  System network daemons should populate this option to specify the
 *  next wakeup date.
 *  Passing this "Date" option lets a caller request a maintenance wake
 *  from the system at a later date. If possible, the system will wake
 *  to the requested power state at the requested time.
 *
 *  This acknowledgement option is only valid when the system is transitioning into a
 *  sleep state i.e. entering a state with 0 capabilities. 
 *
 *  *** Limitation
 *  This acknowledgement argument may only be successfully used with
 *  the requirements bitfield 
 *      kIOPMSystemPowerStateCapabilityDisk | kIOPMSystemPowerStateCapabilityNetwork
 * 
 */
#define kIOPMAckNetworkMaintenanceWakeDate    CFSTR("NetworkMaintenanceWakeDate")

/*! kIOPMAckTimerPluginWakeDate
 *
 *  This string can be used as a key in the 'options' dictionary
 *  argument to IOPMConnectionAcknowledgeEventWithOptions. 
 *  - Caller should set a CFDate value to match this key.
 *  - Caller should set the kIOPMAcknowledgmentOptionSystemCapabilityRequirements key too.
 *
 *  The System timer plugin should populate this option to specify the next wakeup
 *  date.
 *
 *  Passing this "Date" option lets a caller request a maintenance wake
 *  from the system at a later date. If possible, the system will wake
 *  to the requested power state at the requested time.
 *
 *  This acknowledgement option is only valid when the system is transitioning into a
 *  sleep state i.e. entering a state with 0 capabilities. 
 *
 *  *** Limitation
 *  This acknowledgement argument may only be successfully used with
 *  the requirements bitfield 
 *      kIOPMSystemPowerStateCapabilityDisk | kIOPMSystemPowerStateCapabilityNetwork
 * 
 */
#define kIOPMAckTimerPluginWakeDate    CFSTR("TimerPluginWakeDate")


/*! kIOPMAcknowledgmentOptionDate
 *
 *  Callers should prefer to use a more specific key instead of this key:
 *      kIOPMAckNetworkMaintenanceWakeDate
 *      kIOPMAckSleepTimerWakeDate
 *
 *  This string can be used as a key in the 'options' dictionary
 *  argument to IOPMConnectionAcknowledgeEventWithOptions.
 *
 *  Passing this "Date" option lets a caller request a maintenance wake
 *  from the system at a later date. If possible, the system will wake
 *  to the requested power state at the requested time.
 *
 *  This acknowledgement option is only valid when the system is transitioning into a
 *  sleep state i.e. entering a state with 0 capabilities. 
 *
 *  *** Limitation
 *  This acknowledgement argument may only be successfully used with
 *  the requirements bitfield 
 *      kIOPMSystemPowerStateCapabilityDisk | kIOPMSystemPowerStateCapabilityNetwork
 * 
 */
#define kIOPMAcknowledgmentOptionWakeDate           CFSTR("WakeDate")
#define kIOPMAckWakeDate                            kIOPMAcknowledgmentOptionWakeDate

/*! kIOPMAcknowledgmentOptionSystemCapabilityRequirements
 *
 *  This string can be used as a key in the 'options' dictionary
 *  argument to IOPMConnectionAcknowledgeEventWithOptions, or as
 *  an argument to IOPMSystemPowerStateSupportsAcknowledgementOption().
 *
 *  This string should only be specified if the key
 *  kIOPMAcknowledgementOptionDate is also specified.
 *
 *  In the 'options' acknowledgement dictionary, specify a CFNumber that you
 *  contains a bitfield describing the capabilities your process requires at the next
 *  maintenance wake.
 *
 *  This acknowledgement option is only valid when the system is transitioning into a
 *  sleep state i.e. entering a state with 0 capabilities. 
 *
 *  *** Limitation
 *  This acknowledgement argument may only be successfully used with
 *  the requirements bitfield 
 *      kIOPMSystemPowerStateCapabilityDisk | kIOPMSystemPowerStateCapabilityNetwork
 * 
 */
#define kIOPMAcknowledgmentOptionSystemCapabilityRequirements   CFSTR("Requirements")
#define kIOPMAckSystemCapabilityRequirements                    kIOPMAcknowledgmentOptionSystemCapabilityRequirements

/*!
 * @constant kIOPMAcknowledgementOptionSleepServiceDate
 *
 *  This string can be used as a key in the 'options' dictionary
 *  argument to IOPMConnectionAcknowledgeEventWithOptions, or as
 *  an argument to IOPMSystemPowerStateSupportsAcknowledgementOption().
 *
 * Only sleepservicesd should pass this argument. Behavior if passed from any other process
 * is undefined.
 *
 *  Passing this "Date" option lets a caller request a SleepServices wake
 *  from the system at a later date. If possible, the system will wake
 *  and perform SleepServices activity at the specified time.
 *
 *  This acknowledgement option is only valid when the system is transitioning into a
 *  sleep state i.e. entering a state with 0 capabilities. 
 *
 */
#define kIOPMAcknowledgementOptionSleepServiceDate              CFSTR("SleepServiceDate")
#define kIOPMAckSleepServiceDate                                kIOPMAcknowledgementOptionSleepServiceDate

/*!
 * @constant kIOPMAckSleepServiceCapTimeout
 *
 *  This string can be used as a key in the 'options' dictionary
 *  argument to IOPMConnectionAcknowledgeEventWithOptions, or as
 *  an argument to IOPMSystemPowerStateSupportsAcknowledgementOption().
 *
 * Please pass a CFNumber kCFNumberIntType, with a value in milliseconds. 
 * e.g. Pass 120000 to specify a 2 minute cap.
 *
 * Only sleepservicesd should pass this argument. Behavior if passed from any other process
 * is undefined.
 *
 *  Passing this "cap" option lets a caller specify how long a SleepServices wakeup
 *  may last. PM will enforce that the system returns to sleep after this many milliseconds have elapsed.
 *
 *  This acknowledgement option is only valid when the system is transitioning into a
 *  sleep state i.e. entering a state with 0 capabilities. 
 *
 */
#define kIOPMAcknowledgeOptionSleepServiceCapTimeout            CFSTR("SleepServiceTimeout")
#define kIOPMAckSleepServiceCapTimeout                          kIOPMAcknowledgeOptionSleepServiceCapTimeout

/*
 * @constant kIOPMAckClientInfo
 *
 * Optional key for IOPMConnectionAcknoweledgeEventWithOptions() dictionary.
 *
 * The caller may set this key to a CFString with a reason or cause for this 
 * DarkWake request.
 *
 * The string should be a reason, process name, pid, or strings that's useful 
 * for debugging. The value will be logged into "pmset -g log" along with
 * the name of the calling process.
 */
#define kIOPMAckClientInfoKey                                   CFSTR("ClientInfo")

/*****************************************************************************/
/*****************************************************************************/


/*!
 * @function        IOPMConnectionGetSystemCapabilities
 *
 * @abstract        Gets the instantaneous system power capabilities.
 *  
 * @discussion      Checking the system capabilities can help distinguish whether the system is in DarkWake,
 *                  UserActive wake, or if it's on the way to sleep.
 *
 * @result          A bitfield describing whether the system is asleep, awake, or in dark wake,
 *                  using kIOPMSystemPowerStateCapability bits.
 */

IOPMSystemPowerStateCapabilities IOPMConnectionGetSystemCapabilities(void);


/*!
 * IOPMConnectionCreate() opens an IOPMConnection.
 *
 * IOPMConnections provide access to power management notifications. They are a
 * replacement for the existing IORegisterForSystemPower() API, and provide
 * newer functionality and logging as well.
 *
 * Clients are expected to also call IOPMConnectionSetNotification() and
 * IOPMConnectionScheduleWithRunLoop() in order to receive notifications.
 *
 * @param myName Caller should provide a CFStringRef describing its identity, 
 *      for logging.
 * @param interests A bitfield of IOPMSystemPowerStateCapabilities defining
 *      which capabilites the caller is interested in. Caller will only be notified
 *      of changes to the bits specified here.
 * @param newConnection Upon success this will be populated with a fresh IOPMConnection.
 *      The caller must release this with a call to IOPMReleaseConnection.
 * @result Returns kIOReturnSuccess; otherwise on failure.
 */
IOReturn IOPMConnectionCreate(
            CFStringRef myName, 
            IOPMSystemPowerStateCapabilities interests, 
            IOPMConnection *newConnection
            ) __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_NA);

/*! 
 * IOPMConnectionSetNotification associates a notificiation handler with an IOPMConnection
 * @discussion Caller must also call IOPMConnectionScheduleWithRunLoop to setup the notification.
 * @param param User-supplied pointer will be passed to all invocations of handler.  
 *      This call does not retain the *param struct; caller must ensure that the pointer is 
 *      retained.
 * @param myConnection The IOPMConnection created by calling <code>@link IOPMConnectionCreate @/link<code>
 * @param handler Ths function pointer will be invoked every time an event of interest
 *      occurs.
 */
IOReturn IOPMConnectionSetNotification(
            IOPMConnection myConnection, 
            void *param, 
            IOPMEventHandlerType handler
            ) __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_NA);

/*!
 * IOPMConnectionScheduleWithRunLoop schedules a notification on the specified runloop.
 * @discussion The connection must be unscheduled by calling 
 *      IOPMConnectionUnscheduleFromRunLoop.
 * @param myConnection The IOPMConnection created by calling <code>@link IOPMConnectionCreate @/link</code>
 * @param theRunLoop A pointer to the run loop that the caller wants the callback 
 *     scheduled on. Invoking IOPMConnectionRelease will remove the notification from the 
 *     notification from the specified runloop. 
 * @result Returns kIOReturnSuccess; otherwise on failure.
 */
IOReturn IOPMConnectionScheduleWithRunLoop(
            IOPMConnection myConnection, 
            CFRunLoopRef theRunLoop,
            CFStringRef runLoopMode
            ) __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_NA);

 
/*!
 * IOPMConnectionUnscheduleFromRunLoop removes a previously scheduled run loop source.
 * @param myConnection The IOPMConnection created by calling <code>@link IOPMConnectionCreate @/link</code>
 * @param theRunLoop A pointer to the run loop that the caller has previously scheduled
 *      a connection upon. 
 * @result Returns kIOReturnSuccess; otherwise on failure.
 */
IOReturn IOPMConnectionUnscheduleFromRunLoop(
            IOPMConnection myConnection, 
            CFRunLoopRef theRunLoop,
            CFStringRef  runLoopMode
            ) __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_NA);

/*!
 * @function    IOPMConnectionSetDispatchQueue
 * @abstract    Schedule or un-schedule IOPMConnection callbacks on a dispatch queue.
 * @discussion  The caller should set a callback via <code>@link IOPMConnectionSetNotification @/link</code>
 *              before setting the dispatch queue.
 * @param myConnection The IOPMConnection created by a call to <code>@link IOPMConnectionCreate @/link</code>
 * @param myQueue   The dispatch queue to schedule notifications on.
 *                  Pass NULL to cancel dispatch queue notifications.
 *                  If the caller passes in a concurrent dispatch_queue_t, sometimes the notification
 *                  handler can be invoked concurrently (e.g. if the caller fails to respond with
 *                  <code>@link IOPMConnectionAcknowledgePowerEvent @/link</code> before the notification times out).
 */
void IOPMConnectionSetDispatchQueue(
            IOPMConnection myConnection,
            dispatch_queue_t myQueue)
            __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA);

/*!
 * IOPMConnectionRelease cleans up state and notifications for a given
 *      PM connection. This will not remove any scheduled run loop sources - it is the caller's 
 *      responsibility to remove all scheduled run loop sources.
 * @param connection Connection to release.
 * @result Returns kIOReturnSuccess; otherwise on failure.
 */
IOReturn IOPMConnectionRelease(
            IOPMConnection connection
            ) __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_NA);


/*! Acknowledge a power management event.
 * 
 * All IOPMConnection notifications must be acknowledged by calling IOPMConnectAcknowledgeEvent,
 *  or IOPMConnectAcknowledgeEventWithOptions. The caller may invoke IOPMConnectAcknowledgeEvent
 * immediately from within its own notify handler, or the caller may invoke acknowledge
 * the notification from another context.
 *
 * @param connect A valid IOPMConnection object
 * @param token A unique token identifying the message this acknowledgement refers to. This token
 *      should have been received as an argument to the IOPMEventHandlerType handler.
 */
IOReturn IOPMConnectionAcknowledgeEvent(
            IOPMConnection connect, 
            IOPMConnectionMessageToken token
            ) __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_NA);


/*! Acknowledge a power management event with additional options.
 *
 * To request a maintenance wake while acknowledging a system sleep event, 
 * the caller should create an 'options' dictionary with values set for keys
 *      kIOPMAcknowledgmentOptionWakeDate
 *      and kIOPMAcknowledgmentOptionSystemCapabilityRequirements
 *
 * Note that requesting a maintenance wake is only meaningful if the system is transitioning
 * into a sleep state; that is, only if the capabilities flags are all 0.
 *
 * @param connect A valid IOPMConnection object
 * @param token A unique token identifying the message this acknowledgement refers to.
 * @param options If the response type requires additonal arguments, they may be placed
 *          as objects in the "options" dictionary using 
 *          any of the kIOPMAcknowledgementOption* strings as dictionary keys.
 *          Passing NULL for options is equivalent to calling 
 *          IOPMConnectionAcknowledgeEvent()
 * @result Returns kIOReturnSuccess; otherwise on failure.
 */
IOReturn IOPMConnectionAcknowledgeEventWithOptions(
            IOPMConnection connect, 
            IOPMConnectionMessageToken token, 
            CFDictionaryRef options
            ) __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_NA);


/*! 
 * kIOPMSleepServiceActiveNotifyName
 *
 * This API is optional; use <code>IOPMGetSleepServicesActive()</code> for thee easiest way
 * to query this information.
 * Listen for changes to the SleepServices state from 0 to 1 and from 1 to 0 on this notify bit.
 * Powerd calls this notification when it enters and exits a SleepService waiting state.
 */
#define kIOPMSleepServiceActiveNotifyName   "com.apple.powermanagement.sleepservices"

/*! 
 * kIOPMSleepServiceActiveNotifyBit
 * When set, this bit indicates the system is handling SleepServices work.
 *
 * You should probably use <code>IOPMGetSleepServicesActive</code> instead of this
 * for the easiest way to query this state.
 */
#define kIOPMSleepServiceActiveNotifyBit    (1 << 0)

/*!
 * @function        IOPMGetSleepServicesActive
 *
 * @abstract        Checks whether we're staying awake to do SleepServices work at this instant.
 *
 * @result          Returns true starting after SleepServicesd communicates a SleepService Wakeup Cap time;
 *                  and ending when that cap time expires.
 */

bool IOPMGetSleepServicesActive(void);


#define kIOPMDebugEnableAssertionLogging    0x1
#define kIOPMDebugLogAssertionSynchronous   0x2
#define kIOPMDebugLogCallbacks              0x4

IOReturn IOPMSetDebugFlags(uint32_t newFlags, uint32_t *oldFlags);
IOReturn IOPMSetBTWakeInterval(uint32_t newInterval, uint32_t *oldInterval);


__END_DECLS



#endif // _IOPMLibPrivate_h_

