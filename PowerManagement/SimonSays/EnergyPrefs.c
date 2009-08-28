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
 *  TestMissingEnergyPreferences.c
 *  SULeoGaia Verification
 *
 *  Created by Ethan Bold on 7/25/08.
 *  Copyright 2008 Apple. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOReturn.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/ps/IOPowerSources.h>
#include <XILog/XILog.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static bool gMachineSupportsBattery = false;
static bool gMachineSupportsUPS = false;

// Global XI log ref
static XILogRef logRef = NULL;

/* verifyPowerSourceDictionary
 * expects: caller will have initiated a test with XILogBeginTestCase()
 * checks: 
 *  - if machine supports battery; checks for battery dictionary
 *  - if machine supports AC; checks for AC dictionary
 */
static void verifyPowerSourceDictionary(CFDictionaryRef);

#define kExpectEnergySettingsCount      5
#define kExpectPowerProfilesArrayCount  3

/* checkForBatteries
 * populates the global variables gMachineSupportsBattery & gMachineSupports UPS
 */
static void checkForBatteries(void);

/* verifyEnergySettingsDictionary
 * expects: caller will have initiated a test with XILogBeginTestCase()
 * checks: 
 *  - that the energy settings dictionary contains at least kExpectEnergySettingsCount
 *      settings. This is just a simple sanity check; 5 is an arbitrary number and we expect
 *      all macs support at least that many.
 */
static void verifyEnergySettingsDictionary(CFDictionaryRef);

/* verifyPowerProfilesArray
 * expects: caller will have initiated a test with XILogBeginTestCase()
 * checks: 
 *  - that the power profiles array contains at least 3 entries, and that the dictonaries
 *      within it are well-defined.
*/
static void verifyPowerProfiles(CFArrayRef);

/* Read the dictionary of profile choices from IOPMCopyActivePowerProfiles
 * checks: that it returns a CFDictionary       
 * checks that it contains one CFNumber per supported power source
 */
static void verifyActivePowerProfiles(CFDictionaryRef);

int main(int argc, char *argv[])
{
    /* XILog */
    char *XIconfig      = NULL;
    int XIecho          = true; 
    int XIxml           = false;
    char *XILogPath     = NULL; 
    
    /* Aggressiveness Test */
    io_connect_t pmUserClientConnect = MACH_PORT_NULL;
    IOReturn            aggRet = 0;
    unsigned long       aggType = 0;
    unsigned long       aggVal = 0;
    

    /* Custom log file may be defined on command line */
    if (argc > 2) {
        XILogPath = argv[1];
    }

    // 
    // XILog Initialization
    //
     logRef = XILogOpenLog(XILogPath, "IOPMEnergyPreferencesValidity", "com.apple.iokit.ethan", XIconfig, XIxml, XIecho);
     if(logRef == NULL)
     {
         fprintf(stderr,"Couldn't create log: %s", XILogPath ? XILogPath : "(NULL)");
         exit(-1);
     }
     
     // Discover system attributes to test against
     checkForBatteries();

     // Check IOPMGetAggressiveness
     XILogBeginTestCase(logRef, "Check IOPMGetAggressiveness()", "Check return values of a few calls to IOPMGetAggressiveness()");
     
    pmUserClientConnect = IOPMFindPowerManagement(kIOMasterPortDefault);
    if (MACH_PORT_NULL == pmUserClientConnect) {
        XILogErr("No Power Management connection found - fatal.");
    }
    
    aggType = kPMMinutesToSpinDown;
    aggRet = IOPMGetAggressiveness(pmUserClientConnect, aggType, &aggVal);
    XILogMsg("IOPMGetAggressiveness type=%d out val=%d; error return = 0x%08x", aggType, aggVal, aggRet);
    if (kIOReturnSuccess == aggRet) {
        XILogMsg("Good: input=%d(kPMMinutesToSpindown) expected return = kIOReturnSuccess", aggType);
    } else {
        XILogErr("Bad: IOPMGetAggressiveness returned 0x%08x on input=%d(kPMMinutesToSpindown); should return success.", aggRet, aggType);
    }

    aggType = kPMPowerSource;
    aggRet = IOPMGetAggressiveness(pmUserClientConnect, aggType, &aggVal);
    XILogMsg("IOPMGetAggressiveness type=%d out val=%d; error return = 0x%08x", aggType, aggVal, aggRet);
    if (kIOReturnSuccess == aggRet) {
        XILogMsg("Good: input=%d(kPMPowerSource) expected return = kIOReturnSuccess", aggType);
    } else {
        XILogErr("Bad: IOPMGetAggressiveness returned 0x%08x on input=%d(kPMPowerSource); should return success.", aggRet, aggType);
    }

    aggType = kPMLastAggressivenessType;
    aggRet = IOPMGetAggressiveness(pmUserClientConnect, aggType, &aggVal);
    XILogMsg("IOPMGetAggressiveness type=%d out val=%d; error return = 0x%08x", aggType, aggVal, aggRet);
    if (kIOReturnSuccess != aggRet) {
        XILogMsg("Good: input=%d(kPMLastAggressivenessType) expected error=0x%08x", aggType, aggRet);
    } else {
        XILogErr("Bad: IOPMGetAggressiveness returned kIOReturnSuccess on input=%d(kPMLastAggressivenessType); should return success.", aggType);
    }

#ifndef kIOFBLowPowerAggressiveness
#define kIOFBLowPowerAggressiveness	iokit_family_err(sub_iokit_graphics, 1)
#endif
    
    aggType = kIOFBLowPowerAggressiveness;
    aggRet = IOPMGetAggressiveness(pmUserClientConnect, aggType, &aggVal);
    XILogMsg("IOPMGetAggressiveness type=0x%08x out val=%d; error return = 0x%08x", aggType, aggVal, aggRet);
    if (kIOReturnSuccess != aggRet) {
        XILogMsg("Good: input=0x%08x(kIOFBLowPowerAggressiveness) expected error=0x%08x", aggType, aggRet);
    } else {
        XILogErr("Bad: IOPMGetAggressiveness returned kIOReturnSuccess on input=0x%08x(kIOFBLowPowerAggressiveness); should return success.", aggType);
    }
    
    
    IOServiceClose(pmUserClientConnect);

     XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);

     
     // Read the current PM preferences
        // Assert that it contains one dictionary per supported power source
        // Assert that each dictionary contains more than 5 settings
     XILogBeginTestCase(logRef, "Check IOPMCopyPMPreferences()", "Check validity of dictionaries returned from IOPMCopyPMPreferences()");
     
     CFDictionaryRef prefs = NULL;
     
     prefs = IOPMCopyPMPreferences();
     if (!prefs) {
        XILogErr("NULL return from IOPMCopyPMPreferences()");
     } else {
        verifyEnergySettingsDictionary(prefs);
        CFRelease(prefs);
     }
     
     XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);
     
     // Read the array of profiles from IOPMCopyPowerProfiles
        // Assert it contains several profiles (>= 3)
        // Assert that it contains one dictionary per supported power source
        // Assert that each dictionary contains more than 5 settings
     XILogBeginTestCase(logRef, "Check IOPMCopyPowerProfiles()", "Check validity of power profiles from IOPMCopyPowerProfiles()");
     
     CFArrayRef systemProfiles = NULL;
     
     systemProfiles = IOPMCopyPowerProfiles();
     if (!systemProfiles) {
        XILogErr("NULL return from IOPMCopyPowerProfiles()");
     } else {
        verifyPowerProfiles(systemProfiles);
        CFRelease(systemProfiles);
     }
     
     XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);
     

     // Read the dictionary of profile choices from IOPMCopyActivePowerProfiles
        // Assert it returns a CFDictionary
        // Assert that it contains one CFNumber per supported power source
     XILogBeginTestCase(logRef, "Check IOPMCopyActivePowerProfiles()", "Check validity of IOPMCopyActivePowerProfiles()");
     
     CFDictionaryRef activeProfiles = NULL;
     
     activeProfiles = IOPMCopyActivePowerProfiles();
     if (!activeProfiles) {
        XILogErr("NULL return from IOPMCopyPowerProfiles()");
     } else {
        verifyActivePowerProfiles(activeProfiles);
        CFRelease(activeProfiles);
     }
     
     XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);

GiveUp:
     
    //
    // XILog Cleanup
    // 
    XILogCloseLog(logRef);

    return 0;
}


/* verifyPowerSourceDictionary
 * expects: caller will have initiated a test with XILogBeginTestCase()
 * checks: 
 *  - if machine supports battery; checks for battery dictionary
 *  - if machine supports AC; checks for AC dictionary
 */
static void verifyEnergySettingsDictionary(CFDictionaryRef settings)
{
    CFDictionaryRef     acDict = NULL;
    CFDictionaryRef     batteryDict = NULL;
    CFDictionaryRef     upsDict = NULL;
    
    
    if (!settings) {
        XILogErr("Fatal error - NULL energy settings dictionary.");
        return;
    }
    
    acDict = CFDictionaryGetValue(settings, CFSTR(kIOPMACPowerKey));
    if (!acDict) {
        XILogErr("No AC dictionary in energy settings dictionary.");
    }

    batteryDict = CFDictionaryGetValue(settings, CFSTR(kIOPMBatteryPowerKey));
    if (gMachineSupportsBattery && !batteryDict)
    {
        XILogErr("Machine supports battery; Energy prefs dictionary doesn't contain battery settings.");
    }

    upsDict = CFDictionaryGetValue(settings, CFSTR(kIOPMUPSPowerKey));
    if (gMachineSupportsUPS && !upsDict)
    {
        XILogErr("Machine supports UPS; Energy prefs dictionary doesn't contain UPS settings.");
    }

    /* Check size AC */
    if (acDict && (CFDictionaryGetCount(acDict) < kExpectEnergySettingsCount))
    {
        XILogErr("AC dictionary too small: %d < expected %d", 
                CFDictionaryGetCount(acDict), kExpectEnergySettingsCount);
    }

    /* Check size Battery */
    if (batteryDict && (CFDictionaryGetCount(batteryDict) < kExpectEnergySettingsCount))
    {
        XILogErr("Battery dictionary too small: %d < expected %d", 
                CFDictionaryGetCount(batteryDict), kExpectEnergySettingsCount);
    }

    /* Check size UPS */
    if (upsDict && (CFDictionaryGetCount(upsDict) < kExpectEnergySettingsCount))
    {
        XILogErr("UPS dictionary too small: %d < expected %d", 
                CFDictionaryGetCount(upsDict), kExpectEnergySettingsCount);
    }

}

/* verifyPowerProfilesArray
 * expects: caller will have initiated a test with XILogBeginTestCase()
 * checks: 
 *  - that the power profiles array contains at least 3 entries, and that the dictonaries
 *      within it are well-defined.
*/
static void verifyPowerProfiles(CFArrayRef profiles)
{
    int i;
    CFDictionaryRef     settings;

    if (!profiles) {
        XILogErr("Fatal error - NULL power profiles array.");
        return;
    }

    if (CFArrayGetCount(profiles) < kExpectPowerProfilesArrayCount) {
        XILogErr("Power profiles array is too small - %d < expected %d",
            CFArrayGetCount(profiles), kExpectPowerProfilesArrayCount);
    }

    for (i = 0; i < CFArrayGetCount(profiles); i++)
    {
        settings = CFArrayGetValueAtIndex(profiles, i);
        XILogMsg("Checking profile dictionary %d of %d", i, CFArrayGetCount(profiles));
        verifyEnergySettingsDictionary(settings);
    }

    return;
}

/* Read the dictionary of profile choices from IOPMCopyActivePowerProfiles
 * checks: that it returns a CFDictionary       
 * checks that it contains one CFNumber per supported power source
 */
static void verifyActivePowerProfiles(CFDictionaryRef active)
{
    CFNumberRef     acnum = NULL;
    CFNumberRef     battnum = NULL;
    CFNumberRef     upsnum = NULL;

    if (!isA_CFDictionary(active))
    {
        XILogErr("Fatal Error - NULL active power profiles.");
    }

    acnum = CFDictionaryGetValue(active, CFSTR(kIOPMACPowerKey));
    battnum = CFDictionaryGetValue(active, CFSTR(kIOPMBatteryPowerKey));
    upsnum = CFDictionaryGetValue(active, CFSTR(kIOPMUPSPowerKey));
    
    if (!acnum || !isA_CFNumber(acnum)) {
        XILogErr("AC profile selection is missing or malformed.");
    }

    if (gMachineSupportsBattery 
        && (!battnum || !isA_CFNumber(battnum))) {
        XILogErr("batt profile selection is missing or malformed.");
    }

    if (gMachineSupportsUPS
        && (!upsnum || !isA_CFNumber(upsnum))) {
        XILogErr("ups profile selection is missing or malformed.");
    }

}

static void checkForBatteries(void)
{

    CFTypeRef powerblob = NULL;
    
    XILogBeginTestCase(logRef, "Check IOPowerSources response", "Check return value from IOPSCopyPowerSourcesInfo");
    
    powerblob = IOPSCopyPowerSourcesInfo();
    if (!powerblob) {
        XILogErr("NULL return from IOPSCopyPowerSourcesInfo() - error.");
    }
    gMachineSupportsBattery = false;
    if (kCFBooleanTrue == IOPSPowerSourceSupported(powerblob, CFSTR(kIOPMBatteryPowerKey))) {
        XILogMsg("Machine supports battery.");
        gMachineSupportsBattery = true;
    }

    gMachineSupportsUPS = false;
    if (kCFBooleanTrue == IOPSPowerSourceSupported(powerblob, CFSTR(kIOPMUPSPowerKey))) {
        XILogMsg("Machine supports UPS.");
        gMachineSupportsUPS = true;
    }
    
    CFRelease(powerblob);

    XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);
}