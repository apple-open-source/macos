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

#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CFArray.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>

#ifndef _IOPMLibPrivate_h_
#define _IOPMLibPrivate_h_

#ifdef __cplusplus
extern "C" {
#endif

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



#ifdef __cplusplus
}
#endif

#endif // _IOPMLibPrivate_h_

