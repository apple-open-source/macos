
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

#ifdef __cplusplus
extern "C" {
#endif

#define kIOPMDynamicStoreSettingsKey    "State:/IOKit/PowerManagement/CurrentSettings"

#define kIOPMBatteryPowerKey                            "Battery Power"
#define kIOPMACPowerKey                                 "AC Power"

#define kIOPMDisplaySleepKey                            "Display Sleep Timer"
#define kIOPMDiskSleepKey                               "Disk Sleep Timer"
#define kIOPMSystemSleepKey                             "System Sleep Timer"	

#define kIOPMReduceSpeedKey                             "Reduce Processor Speed"
#define kIOPMDynamicPowerStepKey                        "Dynamic Power Step"
#define kIOPMWakeOnLANKey                               "Wake On LAN"
#define kIOPMWakeOnRingKey                              "Wake On Modem Ring"
#define kIOPMRestartOnPowerLossKey                      "Automatic Restart On Power Loss"


/*!

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

#ifdef __cplusplus
}
#endif
