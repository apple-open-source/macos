/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOPMLibPrivate_h_
#define _IOPMLibPrivate_h_

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CFArray.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define kIOPMServerBootstrapName    "com.apple.PowerManagement.control"

// AutoWake API
// For internal use communicating between IOKitUser and PM configd
// The preferences file
#define kIOPMAutoWakePrefsPath      "com.apple.AutoWake.xml"

/**************************************************
*
* Energy Saver Preferences
*
**************************************************/
#define kIOPMDynamicStoreSettingsKey    "State:/IOKit/PowerManagement/CurrentSettings"
#define kIOPMDefaultPreferencesKey      "Defaults"

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
#define kIOPMReduceSpeedKey                             "Reduce Processor Speed"
// units - CFNumber 0/1
#define kIOPMDynamicPowerStepKey                        "Dynamic Power Step"
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

typedef void (*IOPMPrefsCallbackType)(void *context);

    /*!
@function IOPMPrefsNotificationCreateRunLoopSource
@abstract  Returns a CFRunLoopSourceRef that notifies the caller when any PM preferences
    file changes - including Energy Saver or UPS changes.
@param callback A function to be called whenever a prefs file changes.
@param context Any user-defined pointer, passed to the IOPowerSource callback.
@result Returns NULL if an error was encountered, otherwise a CFRunLoopSource. Caller must
    release the CFRunLoopSource.
    */
CFRunLoopSourceRef IOPMPrefsNotificationCreateRunLoopSource(IOPMPrefsCallbackType, void *);


    /*!
@function IOPMCopyPMPreferences.
@abstract Returns a CFDictionary of Power Management preferences. A preference is a CFDictionary
    of Energy Saver settings. They are indexed within the dictionary by CFStrings. ("Battery Power", "AC Power")
@discussion The CFString key kIOPMDefaultPreferencesKey will be present in the top-level dictionary
    if the returned value is default (as in the case of a first boot after clean install) rather 
    than a user-selected set of preferences.
@result Returns a CFDictionary or NULL if request failed. It's the caller's responsibility to CFRelease the dictionary.
     */
CFMutableDictionaryRef IOPMCopyPMPreferences(void);

    /*!
@function IOPMSetPMPreferences.
@abstract Writes a dictionary of (name, preference) pairs back to the preferences file on disk.
    Also activates these preferences and sends notifications to "interested" applications. An
    application can be notified of changes to these prefs through SystemConfiguration.
@param ESPrefs  Dictionary of Power Management preferences to write out to disk.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOPMSetPMPreferences(CFDictionaryRef ESPrefs);

    /*!
@function IOPMActivatePMPreference.
@abstract Activates the set of preferences called "name." Sets idle spin down timers and other
    Energy Saver settings according to profile name.
@param SystemProfiles  The dictionary of preferences from IOPMCopyPMPreferences
@param profile The name of the set of preferences defined in ESPrefs to activate.
@result Returns kIOReturnSuccess or an error condition if request failed.
     */
IOReturn IOPMActivatePMPreference(CFDictionaryRef SystemProfiles, CFStringRef profile);

    /*!
@function IOPMFetaureIsAvailable
@abstract Identifies whether a PM feature is supported on this platform.
@param feature The CFSTR() feature to check availability of
@param power_source CFSTR(kIOPMACPowerKey) or CFSTR(kIOPMBatteryPowerKef). Doesn't 
    matter for most features, but a few are power source dependent.
@result Returns true if supported, false otherwise.
     */
bool IOPMFeatureIsAvailable(CFStringRef feature, CFStringRef power_source);

/**************************************************
*
* Dynamic Power Assertions
* Allows any application to dynamically request "Highest Performance"
*
**************************************************/
// Keeps the CPU at its highest level
#define kIOPMCPUBoundAssertion                    CFSTR("CPUBoundAssertion")

// UNSUPPORTED: kIOPMPreventIdleSleepAssertion is UNSUPPORTED in 10.4
#define kIOPMPreventIdleSleepAssertion            CFSTR("NoIdleSleepAssertion")

enum {
    kIOPMAssertionDisable = 0,
    kIOPMAssertionEnable  = 255
 };

typedef int IOPMAssertionID;

    /*!
@function IOPMAssertionCreate
@abstract Dynamically requests a system behavior from the power management system.
@discussion No special privileges necessary to make this call - any process may
        activate a power profile.
@param assertion The CFString profile to request from the PM system.
@param level Pass kIOPMProfileEnable or kIOPMProfileDisable.
@param assertion_id On success, a unique id will be returned in this parameter.
@result Returns kIOReturnSuccess on success, any other return indicates
        PM could not successfully activate the specified profile.
     */
IOReturn IOPMAssertionCreate(CFStringRef  assertion, 
                           int level,
                           IOPMAssertionID *assertion_id);                           
                           
    /*!
@function IOPMAssertionRelease
@abstract Releases the behavior requested in IOPMAssertionCreate
@discussion All calls to IOPMAssertionCreate must be paired with calls to  
        IOPMAssertionRelease.
@param assertion_id The assertion_id, returned from IOPMAssertionCreate, to cancel.
@result Returns kIOReturnSuccess on success
     */
IOReturn IOPMAssertionRelease(IOPMAssertionID assertion_id);

// Use these keys to examine assertion dictionaries returned
// in IOPMCopyAssertionsByProcess() return value.
#define kIOPMAssertionTypeKey       CFSTR("assert_type")
#define kIOPMAssertionValueKey      CFSTR("assert_value")

    /*!
@function IOPMCopyAssertionsByProcess
@abstract Returns a dictionary mapping active profiles to the processes that activated them (by pid).
@discussion Notes: One process may have multiple profiles asserted. Several processes may
            have asserted the same profile to different levels.
@param assertions_by_pid On success, this returns a terribly complicated nested data structure 
        of assertions per process.
        At the top level, keys to the CFDictionary are pids stored as CFNumbers (kCFNumberIntType).
        The value associated with each CFNumber pid is a CFArray of active assertions.
        Each entry in the CFArray is an assertion represented as a CFDictionary. See the keys
            kIOPMAssertionTypeKey and kIOPMAssertionValueKey           
@result Returns kIOReturnSuccess on success.
     */
IOReturn IOPMCopyAssertionsByProcess(CFDictionaryRef *assertions_by_pid);

    /*!
@function IOPMCopyAssertionsStatus
a@bstract Returns a list of available profiles and their currently aggregated state.
@discussion Notes: One process may have multiple profiles asserted. Several processes may
            have asserted the same profile to different levels.
@param assertions_status On success, this returns a CFDictionary of all profiles currently available.
       The keys in the dictionary are the profile names, and the value of each is a CFNumber that
       represents the aggregate level for that profile.  Caller must CFRelease() this dictionary when done.
@result Returns kIOReturnSuccess on success.
     */
IOReturn IOPMCopyAssertionsStatus(CFDictionaryRef *assertions_status);



/**************************************************
*
* Power Profiles (use in combination with Energy Saver Preferences above)
*
**************************************************/
#define kIOPMCustomPowerProfile         -1
#define kIOPMNumPowerProfiles           5

/*! @function IOPMCopyPowerProfilesInfo
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



__END_DECLS

#endif // _IOPMLibPrivate_h_

