//
//  energyprefs.c
//
//  Created by dekom on 12/3/15.
//  Copyright © 2015 Apple. All rights reserved.
//

#include <stdio.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include "PMtests.h"

static void testRegister(void);
static void testCopy(void);
static void testRevert(void);
static void testSet(void);
static void testSystemPowerSettings(void);
static void test_UsingDefaultPrefs(void);

int gPassCnt = 0, gFailCnt = 0;

int main(int argc, const char * argv[]) {
    START_TEST("Executing energyprefs\n");
    CFDictionaryRef savedPrefs = IOPMCopyPMPreferences();
    
    testRegister();
    testCopy();
    testRevert();
    testSet();
    testSystemPowerSettings();
    test_UsingDefaultPrefs();
    
    // Restore preferences
    if (savedPrefs) {
        IOReturn ret = IOPMSetPMPreferences(savedPrefs);
        if (ret != kIOReturnSuccess) {
            FAIL("main - IOPMSetPMPreferences failed (ret = 0x%x)\n", ret);
        }
    }
    else {
        FAIL("main - IOPMCopyPMPreferences failed\n");
    }
    
    SUMMARY("energyprefs");
    return 0;
}

static void pmPrefsCallBack(void)
{
    LOG("Prefs updated\n");
}

static void testRegister(void)
{
    IOPMNotificationHandle prefsHandle = 0;
    
    START_TEST_CASE("Test IOPMRegisterPrefsChangeNotification\n");
    prefsHandle = IOPMRegisterPrefsChangeNotification(
                                dispatch_get_main_queue(),
                                ^(void) {
                                    pmPrefsCallBack();
                                });
    if (!prefsHandle) {
        FAIL("IOPMRegisterPrefsChangeNotification failed\n");
    }
    
    IOPMUnregisterPrefsChangeNotification(prefsHandle);
    
    PASS("testRegister\n");
}

static void testCopy(void)
{
    CFMutableDictionaryRef  mDict    = NULL;
    CFDictionaryRef         dict     = NULL;
    CFNumberRef             numRef   = NULL;
    bool                    result   = false;
    
    START_TEST_CASE("Test Copy/Set Preferences\n");
    mDict = IOPMCopyPMPreferences();
    if (!mDict) {
        FAIL("IOPMCopyPMPreferences failed\n");
    }
    else {
        LOG("IOPMCopyPMPreferences returned:\n");
        CFShow(mDict);
        CFRelease(mDict);
    }
    
    dict = IOPMCopyActivePMPreferences();
    if (!dict) {
        FAIL("IOPMCopyActivePMPreferences failed\n");
    }
    else {
        LOG("IOPMCopyActivePMPreferences returned:\n");
        CFShow(dict);
        CFRelease(dict);
    }
    
    dict = IOPMCopyDefaultPreferences();
    if (!dict) {
        FAIL("IOPMCopyDefaultPreferences failed\n");
    }
    else {
        LOG("IOPMCopyDefaultPreferences returned:\n");
        CFShow(dict);
        CFRelease(dict);
    }
    
    IOPMCopyPMSetting(CFSTR(kIOPMDisplaySleepKey), NULL, (CFTypeRef *)&numRef);
    if (!numRef) {
        FAIL("IOPMCopyPMSetting failed\n");
    }
    else {
        int val = 0;
        CFNumberGetValue(numRef, kCFNumberSInt32Type, &val);
        LOG("IOPMCopyPMSetting returned: %d\n", val);
        CFRelease(numRef);
    }
    
    result = IOPMUsingDefaultPreferences(CFSTR(kIOPMACPowerKey));
    LOG("IOPMUsingDefaultPreferences(CFSTR(kIOPMACPowerKey)) returned: %d\n", result);
    
    result = IOPMUsingDefaultPreferences(NULL);
    LOG("IOPMUsingDefaultPreferences(NULL) returned: %d\n", result);
    
    result = IOPMFeatureIsAvailable(CFSTR(kIOPMPowerNapSupportedKey), CFSTR(kIOPMACPowerKey));
    LOG("IOPMFeatureIsAvailable returned: %d\n", result);
    
    PASS("testCopy\n");
}

static void test_UsingDefaultPrefs()
{
    IOReturn rc;
    bool ret;
    CFNumberRef numRef;

    START_TEST_CASE("Test Revert Preferences and Check for Defaults\n");
    // Set null prefs
    rc = IOPMRevertPMPreferences(NULL);
    if (rc != kIOReturnSuccess) {
        FAIL("IOPMSetPMPreferences returned 0x%x\n", rc) ;
        goto exit;
    }

    ret = IOPMUsingDefaultPreferences(NULL);
    if (ret != true) {
        FAIL("IOPMUsingDefaultPreferences returned false, when true is expected\n");
        goto exit;
    }
    ret = IOPMUsingDefaultPreferences(CFSTR(kIOPMACPowerKey));
    if (ret != true) {
        FAIL("IOPMUsingDefaultPreferences returned false for AC Power source, when true is expected\n");
        goto exit;
    }

    INT_TO_CFNUMBER(numRef, 0);
    IOPMSetPMPreference(CFSTR(kIOPMDisplaySleepKey), numRef, CFSTR(kIOPMBatteryPowerKey));
    CFRelease(numRef);

    ret = IOPMUsingDefaultPreferences(CFSTR(kIOPMACPowerKey));
    if (ret != true) {
        FAIL("IOPMUsingDefaultPreferences returned false for AC Power source, when true is expected\n");
        goto exit;
    }

    ret = IOPMUsingDefaultPreferences(CFSTR(kIOPMBatteryPowerKey));
    if (ret == true) {
        FAIL("IOPMUsingDefaultPreferences returned true for Batt Power source, when false is expected\n");
        goto exit;
    }
    ret = IOPMUsingDefaultPreferences(NULL);
    if (ret == true) {
        FAIL("IOPMUsingDefaultPreferences returned true, when false is expected\n");
        goto exit;
    }

    PASS("Test Revert Preferences and Check for Defaults\n");

exit:
    return;
}


static void testRevert(void)
{
    CFStringRef keys[3];
    CFArrayRef  arr = NULL;
    IOReturn    ret = kIOReturnError;
    
    // Random keys to revert
    keys[0] = CFSTR(kIOPMDisplaySleepKey);
    keys[1] = CFSTR(kIOPMDarkWakeBackgroundTaskKey);
    keys[2] = CFSTR(kIOPMDeepSleepEnabledKey);
    
    arr = CFArrayCreate(0, (const void **)keys, 3, &kCFTypeArrayCallBacks);
    if (arr) {
        ret = IOPMRevertPMPreferences(arr);
        if (ret != kIOReturnSuccess) {
            FAIL("IOPMRevertPMPreferences(arr) failed (ret = 0x%x)\n", ret);
        }
        CFRelease(arr);
    }
    else {
        FAIL("testRevert - CFArrayCreate failed\n");
    }
    
    ret = IOPMRevertPMPreferences(NULL);
    if (ret != kIOReturnSuccess) {
        FAIL("[IOPMRevertPMPreferences(NULL) failed (ret = 0x%x)\n", ret);
    }
    
    PASS("testRevert\n");
}

static void testSet(void)
{
    CFDictionaryRef dict    = IOPMCopyActivePMPreferences();
    IOReturn        ret     = kIOReturnError;
    CFNumberRef     numRef  = NULL;
    int             num     = 1234;
    
    numRef = CFNumberCreate(0, kCFNumberIntType, &num);
    if (numRef) {
        // Set one key/ps
        ret = IOPMSetPMPreference(CFSTR(kIOPMDisplaySleepKey), numRef, CFSTR(kIOPMACPowerKey));
        if (ret != kIOReturnSuccess) {
            FAIL("IOPMSetPMPreference displaysleep/set/ac failed (ret = 0x%x)\n", ret);
        }
        
        // Set one key for all ps's
        ret = IOPMSetPMPreference(CFSTR(kIOPMDisplaySleepKey), numRef, NULL);
        if (ret != kIOReturnSuccess) {
            FAIL("IOPMSetPMPreference displaysleep/set/NULL failed (ret = 0x%x)\n", ret);
        }
        
        // Revert key
        ret = IOPMSetPMPreference(CFSTR(kIOPMDisplaySleepKey), NULL, CFSTR(kIOPMACPowerKey));
        if (ret != kIOReturnSuccess) {
            FAIL("IOPMSetPMPreference displaysleep/NULL/ac failed (ret = 0x%x)\n", ret);
        }
        
        // Revert all keys for a ps
        ret = IOPMSetPMPreference(NULL, NULL, CFSTR(kIOPMACPowerKey));
        if (ret != kIOReturnSuccess) {
            FAIL("IOPMSetPMPreference NULL/NULL/ac failed (ret = 0x%x)\n", ret);
        }
        
        // Revert all keys
        ret = IOPMSetPMPreference(NULL, NULL, NULL);
        if (ret != kIOReturnSuccess) {
            FAIL("IOPMSetPMPreference NULL/NULL/NULL failed (ret = 0x%x)\n", ret);
        }
        
        CFRelease(numRef);
    }
    else {
        FAIL("testSet - CFNumberCreate failed\n");
    }
    
    if (dict) {
        ret = IOPMSetPMPreferences(dict);
        if (ret != kIOReturnSuccess) {
            FAIL("IOPMSetPMPreferences failed (ret = 0x%x)\n", ret);
        }
    }
    else {
        FAIL("testSet - IOPMCopyActivePMPreferences failed\n");
    }
    
    ret = IOPMSetPMPreferences(NULL);
    if (ret != kIOReturnSuccess) {
        FAIL("IOPMSetPMPreferences(NULL) failed (ret = 0x%x)\n", ret);
    }
    
    if (dict) {
        CFRelease(dict);
    }
    PASS("testSet\n");
}

static void testSystemPowerSettings(void)
{
    IOReturn        ret     = kIOReturnError;
    CFDictionaryRef dict    = NULL;
    
    ret = IOPMSetSystemPowerSetting(CFSTR(kIOPMDestroyFVKeyOnStandbyKey), kCFBooleanTrue);
    if (ret != kIOReturnSuccess) {
        FAIL("IOPMSetSystemPowerSetting failed (ret = 0x%x)\n", ret);
    }
    
    dict = IOPMCopySystemPowerSettings();
    if (!dict) {
        FAIL("IOPMCopySystemPowerSettings failed\n");
    }
    else {
        LOG("IOPMCopySystemPowerSettings returned:\n");
        CFShow(dict);
        CFRelease(dict);
    }
    
    PASS("testSystemPowerSettings\n");
}
