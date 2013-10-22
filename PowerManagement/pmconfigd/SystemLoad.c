/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <notify.h>
#include <IOKit/hidsystem/IOHIDLib.h>

#include "PrivateLib.h"
#include "SystemLoad.h"
#include "PMStore.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMConnection.h"

#ifndef  kIOHIDSystemUserHidActivity
#define kIOHIDSystemUserHidActivity    iokit_family_msg(sub_iokit_hidsystem, 6)
#endif

#define IDLE_HID_ACTIVITY_SECS ((uint64_t)(5*60))

// Forwards
const bool  kNoNotify  = false;
const bool  kYesNotify = true;
static void shareTheSystemLoad(bool shouldNotify);


// Globals
static CFStringRef  systemLoadKey = NULL;
static CFStringRef  systemLoadDetailedKey = NULL;
//  The following  bool variables are implicit arguments to
//  the function shareTheSystemLoad()
static bool   onACPower                 = FALSE;
static bool   onBatteryPower            = FALSE;
static bool   batteryBelowThreshold     = FALSE;

static bool   coresConstrained          = FALSE;
static bool   forcedIdle                = FALSE;
static bool   plimitBelowThreshold      = FALSE;
static bool   thermalWarningLevel       = FALSE;

static bool   loggedInUser              = FALSE;

static bool   displayIsOff                = FALSE;
static bool   displaySleepEnabled       = FALSE;

static int    gNotifyToken              = 0;

static int    gUserActivityNotifyToken  = 0;
static bool   userActiveAssertions      = FALSE;
dispatch_source_t  ua_AssertionTimer    = NULL;
static bool   hidActivityIdle           = FALSE;

static int minOfThree(int a, int b, int c)
{
    int result = a;
    
    if (b < result) result = b;
    if (c < result) result = c;

    return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static void shareUserIdleness( )
{
#if !TARGET_OS_EMBEDDED
   static bool userIsIdle_saved = false;
   bool userIsIdle = false;

    if  ((userActiveAssertions == false) && ((!loggedInUser) ||  (hidActivityIdle) || (displayIsOff)) )
       userIsIdle = true;

    if (userIsIdle != userIsIdle_saved) {
       if (userIsIdle) {
          notify_set_state(gUserActivityNotifyToken, (uint64_t)kIOUserIsIdle);        
       }
       else  {
          notify_set_state(gUserActivityNotifyToken, (uint64_t)kIOUserIsActive);        
       }

       notify_post(kIOUserActivityNotifyName);

       userIsIdle_saved = userIsIdle;
    }
#endif

}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void shareTheSystemLoad(bool shouldNotify)
{
    static uint64_t         lastSystemLoad  = 0;
    uint64_t                theseSystemLoad = 0;
    int                     userLevel       = kIOSystemLoadAdvisoryLevelGreat;
    int                     batteryLevel    = kIOSystemLoadAdvisoryLevelGreat;
    int                     powerLevel      = kIOSystemLoadAdvisoryLevelGreat;
    int                     combinedLevel   = kIOSystemLoadAdvisoryLevelGreat;

/******************************************
 * Power Level Computation code begins here
 * Edit this block of code to change what
 * defines a "good time" to do work, based on system load.
 */
/******************************************/

    if (onACPower) {
        batteryLevel = kIOSystemLoadAdvisoryLevelGreat;
    } else if (!batteryBelowThreshold) {
        batteryLevel = kIOSystemLoadAdvisoryLevelOK;
    } else {
        batteryLevel = kIOSystemLoadAdvisoryLevelBad;
    }
    
    if (plimitBelowThreshold) {
        powerLevel = kIOSystemLoadAdvisoryLevelOK;
    }
    if (coresConstrained || forcedIdle || thermalWarningLevel) {
        powerLevel = kIOSystemLoadAdvisoryLevelBad;
    }

    // TODO: Use seconds since last UI activity as an indicator of
    // userLevel. Basing this data on display dimming is a crutch,
    // and may be invalid on systems with display dimming disabled.
    if (loggedInUser) {
        if (displayIsOff)
        {
            if (_DWBT_enabled()) {
               // System allows DWBT & user has opted in

               if (isA_BTMtnceWake( ) )
                  userLevel = kIOSystemLoadAdvisoryLevelGreat;
               else
                  userLevel = kIOSystemLoadAdvisoryLevelOK;
            }
            else
               userLevel = kIOSystemLoadAdvisoryLevelGreat;

        } else {
            userLevel = kIOSystemLoadAdvisoryLevelOK;
        }
        // TODO: If user is performing a full screen activity, or
        // is actively producing UI events, time is BAD.
    }

    // The combined level is the lowest/worst level of the contributing factors
    combinedLevel = minOfThree(userLevel, batteryLevel, powerLevel);

/******************************************/
/* Power Level Computation code ends here */
/******************************************/

    theseSystemLoad = combinedLevel
                | (userLevel << 8)
                | (batteryLevel << 16)
                | (powerLevel << 24);

    if (theseSystemLoad != lastSystemLoad) 
    {
        CFMutableDictionaryRef publishDetails = NULL;
        CFNumberRef publishNum = NULL;
    
        lastSystemLoad = theseSystemLoad;

        /* Publish the combinedLevel under our notify key 'kIOSystemLoadAdvisoryNotifyName'
         */
        notify_set_state(gNotifyToken, (uint64_t)combinedLevel);        

        /* Publish the SystemLoad key read by API
         * IOGetSystemLoadAdvisory();
         */
        publishNum = CFNumberCreate(0, kCFNumberSInt64Type, &theseSystemLoad);
        if (publishNum)
        {
            PMStoreSetValue(systemLoadKey, publishNum);
            CFRelease(publishNum);
            publishNum = NULL;
        }
    
        /* Publish the Detailed key read by API
         * CFDictionaryRef IOPMCheckSystemLoadDetailed();
         */
        publishDetails = CFDictionaryCreateMutable(0, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
        if (!publishDetails) return;
        publishNum = CFNumberCreate(0, kCFNumberIntType, &userLevel);
        if (publishNum) {
            CFDictionarySetValue(publishDetails, 
                                    kIOSystemLoadAdvisoryUserLevelKey,
                                    publishNum);
            CFRelease(publishNum);
            publishNum = 0;
        }
        publishNum = CFNumberCreate(0, kCFNumberIntType, &batteryLevel);
        if (publishNum) {
            CFDictionarySetValue(publishDetails, 
                                    kIOSystemLoadAdvisoryBatteryLevelKey,
                                    publishNum);
            CFRelease(publishNum);
            publishNum = 0;
        }
        publishNum = CFNumberCreate(0, kCFNumberIntType, &powerLevel);
        if (publishNum) {
            CFDictionarySetValue(publishDetails, 
                                    kIOSystemLoadAdvisoryThermalLevelKey,
                                    publishNum);
            CFRelease(publishNum);
            publishNum = 0;
        }
        publishNum = CFNumberCreate(0, kCFNumberIntType, &combinedLevel);
        if (publishNum) {
            CFDictionarySetValue(publishDetails, 
                                    kIOSystemLoadAdvisoryCombinedLevelKey,
                                    publishNum);
            CFRelease(publishNum);
            publishNum = 0;
        }

        // Publish SystemLoadDetailed
        PMStoreSetValue(systemLoadDetailedKey, publishDetails);
        CFRelease(publishDetails);

        // post notification
        if (shouldNotify) {
            notify_post(kIOSystemLoadAdvisoryNotifyName);
        }
    }
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static void 
hidActivityStateChange(void *ref, io_service_t service, natural_t messageType, void *arg)
{

   if (messageType != kIOHIDSystemUserHidActivity)
      return;


   hidActivityIdle = ((uint64_t)arg) ? true : false;

    shareUserIdleness();
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static void hidSystemMatched(
    void *note_port_in, 
    io_iterator_t iter)
{
    IONotificationPortRef       note_port = (IONotificationPortRef)note_port_in;
    io_service_t                hidSystem  = MACH_PORT_NULL;
    io_object_t                 notification_object = MACH_PORT_NULL;
    mach_port_t                 connect = MACH_PORT_NULL;
    
    if((hidSystem = (io_registry_entry_t)IOIteratorNext(iter))) 
    {        
        IOServiceAddInterestNotification(
                    note_port, 
                    hidSystem, 
                    kIOGeneralInterest, 
                    hidActivityStateChange,
                    NULL, 
                    &notification_object);

        IOServiceOpen(hidSystem, mach_task_self(), kIOHIDParamConnectType, &connect);
        if (connect) {
            IOHIDGetActivityState(connect, &hidActivityIdle);
            IOServiceClose(connect);
            shareUserIdleness();
        }

        IOObjectRelease(hidSystem);
    }

}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__private_extern__ void SystemLoad_prime(void)
{
    IONotificationPortRef       notify_port = 0;
    io_iterator_t               hid_iter = 0;
    kern_return_t               kr;
    CFRunLoopSourceRef          rlser = 0;

    systemLoadKey = SCDynamicStoreKeyCreate(
                    kCFAllocatorDefault, 
                    CFSTR("%@%@"),
                    kSCDynamicStoreDomainState, 
                    CFSTR("/IOKit/PowerManagement/SystemLoad"));

    systemLoadDetailedKey = SCDynamicStoreKeyCreate(
                    kCFAllocatorDefault, 
                    CFSTR("%@%@"),
                    kSCDynamicStoreDomainState, 
                    CFSTR("/IOKit/PowerManagement/SystemLoad/Detailed"));

    notify_register_check(kIOSystemLoadAdvisoryNotifyName, &gNotifyToken);

    notify_register_check(kIOUserActivityNotifyName, &gUserActivityNotifyToken);
    notify_set_state(gUserActivityNotifyToken, (uint64_t)kIOUserIsActive);        

    // If this is a desktop, then we won't get any battery notifications.
    // Let's prime the battery pump right here with an initial coll.
    SystemLoadBatteriesHaveChanged(_batteries());

    SystemLoadPrefsHaveChanged();
    
#if !TARGET_OS_EMBEDDED
    SystemLoadUserStateHasChanged();
#endif
    
    SystemLoadCPUPowerHasChanged(NULL);

    notify_port = IONotificationPortCreate(0);
    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(rlser) 
       CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);

    kr = IOServiceAddMatchingNotification(
                                notify_port,
                                kIOFirstMatchNotification,
                                IOServiceMatching("IOHIDSystem"),
                                hidSystemMatched,
                                (void *)notify_port,
                                &hid_iter);
    if(KERN_SUCCESS == kr) 
    {
        // Install notifications on existing instances.
        hidSystemMatched((void *)notify_port, hid_iter);
    }
    else {
        asl_log(NULL, NULL, ASL_LEVEL_ERR, 
                "Failed to match HIDSystem(0x%x)\n", kr);
    }
    


}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* @function SystemLoadDisplayPowerStateHasChanged
 * @param displayIsOn is true if the backlight is powered
 * Populates:
 *      displayIsOff
 */
__private_extern__ void SystemLoadDisplayPowerStateHasChanged(bool _displayIsOff)
{
    if (displayIsOff == _displayIsOff) {
        return;
    }

    displayIsOff = _displayIsOff;
    
    shareTheSystemLoad(kYesNotify);
    shareUserIdleness();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* @function SystemLoadPrefsHaveChanged
 * @abstract We check whether DIsplay Sleep Timer == 0
 * Populates:
 *      displaySleepEnabled
 */
__private_extern__ void SystemLoadPrefsHaveChanged(void)
{
    SCDynamicStoreRef   _store       = _getSharedPMDynamicStore();
    CFDictionaryRef     liveSettings = NULL;
    CFNumberRef         displaySleep = NULL;
    CFTypeRef           dwbt = NULL;
    int                 idle, newDWBT;
    static bool         lastDisplaySleep = false;
    static int          lastDWBT = -1;
    bool                notify = false;

    liveSettings = SCDynamicStoreCopyValue(_store, 
                        CFSTR(kIOPMDynamicStoreSettingsKey));
    if (liveSettings) 
    {
        displaySleep = CFDictionaryGetValue(liveSettings, 
                                    CFSTR(kIOPMDisplaySleepKey));
        if (displaySleep) 
        {
            CFNumberGetValue(displaySleep, kCFNumberIntType, &idle);

            displaySleepEnabled = idle ? true:false;                

            if (displaySleepEnabled != lastDisplaySleep)
            {
                lastDisplaySleep = displaySleepEnabled;
                notify = true;
            }
        }
        dwbt = CFDictionaryGetValue(liveSettings, 
                                    CFSTR(kIOPMDarkWakeBackgroundTaskKey));
        if (dwbt)
        {
            newDWBT = CFBooleanGetValue(dwbt) ? 1 : 0;
            if (lastDWBT != newDWBT) 
            {
                lastDWBT = newDWBT;
                notify = true;
            }
        }
        CFRelease(liveSettings);
    }

    if (notify)
        shareTheSystemLoad(kYesNotify);
    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* @function SystemLoadBatteriesHaveChanged
 * Populates:
 *   onACPower
 *   onBatteryPower
 *   batteryBelowThreshold
 */ 
__private_extern__ void SystemLoadBatteriesHaveChanged(IOPMBattery **batt_stats)
{
    static const int kBatThreshold = 40;
    int count = _batteryCount();
    int sumMax = 0;
    int sumCurrent = 0;
    int i;
   
    onACPower = false;
    onBatteryPower = false;
    batteryBelowThreshold = false;
    
    if (0 == count)
    {
        onACPower = true;
        goto exit;
    }
    for (i=0; i<count; i++) 
    {
        if( batt_stats[i]->externalConnected ) {
            onACPower = true; 
        }
        sumCurrent += batt_stats[i]->currentCap;
        sumMax += batt_stats[i]->maxCap;
    }
    if (!onACPower) 
    {
        onBatteryPower = true;
    }
    if (sumMax
        && (kBatThreshold > (100*sumCurrent)/sumMax)) 
    {
        batteryBelowThreshold = true;
    }

exit:    
    shareTheSystemLoad(kYesNotify);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* @function SystemLoadCPUPowerHasChanged
 * Populates:
 *  coresConstrained
 *  forcedIdle
 *  plimitBelowThreshold
 *  thermalWarningLevel
 */
__private_extern__ void SystemLoadCPUPowerHasChanged(CFDictionaryRef newCPU)
{
    CFDictionaryRef ourAllocatedCPU = NULL;
    CFNumberRef     plimitNum = NULL;
    int             plimit = 100;   // defaults to OK value
    CFNumberRef     cpuCountNum = NULL;
    int             cpuCount = 0;
    static int      maxCPUCount = 0;
    CFNumberRef     runnableTimeNum = NULL;
    int             runnableTime = 100; // defaults to OK value
    uint32_t        getlevel = 0;
    IOReturn        ret;

    if (0 == maxCPUCount) {
        int  result;
        size_t len = sizeof(maxCPUCount);
        result = sysctlbyname("hw.ncpu", &maxCPUCount, &len, 0, 0);
        if (-1 == result) { // error
            maxCPUCount = 0;
        }
    }

   // cpuCount defaults to max CPU count
    cpuCount = maxCPUCount;

    /**/    
    ret = IOPMGetThermalWarningLevel(&getlevel);
    if ( (kIOReturnSuccess == ret)
        && (kIOPMThermalWarningLevelNormal != getlevel))
    {
        thermalWarningLevel = true;
    } else {
        thermalWarningLevel = false;
    }

    /**/
    coresConstrained = false;
    forcedIdle = false;
    plimitBelowThreshold = false;

    /**/
    // If caller passed in a NULL CPU dictionary, we'll allocate one here,
    // and release it at exit.
    if (!newCPU) {
        ret = IOPMCopyCPUPowerStatus(&ourAllocatedCPU);
        if (kIOReturnSuccess == ret) {
            newCPU = ourAllocatedCPU;
        } else {
            goto exit;
        }
    }
    
    if (!newCPU)
        goto exit;

    plimitNum = CFDictionaryGetValue(newCPU, CFSTR(kIOPMCPUPowerLimitProcessorSpeedKey));
    if (plimitNum) {
        CFNumberGetValue(plimitNum, kCFNumberIntType, &plimit);
    }

    cpuCountNum = CFDictionaryGetValue(newCPU, CFSTR(kIOPMCPUPowerLimitProcessorCountKey));
    if (cpuCountNum) {
        CFNumberGetValue(cpuCountNum, kCFNumberIntType, &cpuCount);
    }

    runnableTimeNum = CFDictionaryGetValue(newCPU, CFSTR(kIOPMCPUPowerLimitSchedulerTimeKey));
    if (runnableTimeNum) {
        CFNumberGetValue(runnableTimeNum, kCFNumberIntType, &runnableTime);
    }

    // This test only tests the results that are returned.
    // For a platform that doesn't support, say, dropping CPU's, that property
    // may be absent in the CPU Power dictionary.
    if (50 >= plimit) {
        plimitBelowThreshold = true;
    }
    if (maxCPUCount > cpuCount) {
        coresConstrained = true;
    }
    if (100 != runnableTime) {
        forcedIdle = true;
    }

    shareTheSystemLoad(kYesNotify);

exit:
    if (ourAllocatedCPU)
        CFRelease(ourAllocatedCPU);
    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if !TARGET_OS_EMBEDDED
/* @function SystemLoadUserStateHasChanged
 * Populates:
 *  loggedInUser
 *  loggedInUserIdle
 *  switchedOutUsers
 *  remoteConnections
 */
__private_extern__ void SystemLoadUserStateHasChanged(void)
{
    CFStringRef         loggedInUserName;

    loggedInUser = false;

    loggedInUserName = SCDynamicStoreCopyConsoleUser(_getSharedPMDynamicStore(),
                                                    NULL,  // uid
                                                    NULL); // gid
    if (loggedInUserName) {
        loggedInUser = true;
        CFRelease(loggedInUserName);
    }

    shareTheSystemLoad(kYesNotify);
    shareUserIdleness( );
}

__private_extern__ void SystemLoadSystemPowerStateHasChanged(void)
{
    shareTheSystemLoad(kYesNotify);
}
#endif /* !TARGET_OS_EMBEDDED */


__private_extern__ void SystemLoadUserActiveAssertions(bool _userActiveAssertions)
{
#if !TARGET_OS_EMBEDDED
    static uint64_t    userActive_ts = 0;

    if (ua_AssertionTimer == 0) {
        ua_AssertionTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0,
                            0, dispatch_get_main_queue());

        dispatch_source_set_event_handler(ua_AssertionTimer, ^{
                SystemLoadUserActiveAssertions(false);
                });

        dispatch_source_set_cancel_handler(ua_AssertionTimer, ^{
            dispatch_release(ua_AssertionTimer);
            ua_AssertionTimer = 0;
        });
    }

    if (_userActiveAssertions == false) {
        uint64_t curTime = getMonotonicTime();
        dispatch_suspend(ua_AssertionTimer);
        if ( (curTime - userActive_ts) < IDLE_HID_ACTIVITY_SECS) {
            dispatch_source_set_timer(ua_AssertionTimer,
                    dispatch_time(DISPATCH_TIME_NOW, 
                        (IDLE_HID_ACTIVITY_SECS+userActive_ts-curTime)*NSEC_PER_SEC), 
                    DISPATCH_TIME_FOREVER, 0);
            dispatch_resume(ua_AssertionTimer);
            return;
        }
    }
    else {
        userActive_ts = getMonotonicTime();
    }

    if (userActiveAssertions == _userActiveAssertions)
       return;

    userActiveAssertions = _userActiveAssertions;

    if (userActiveAssertions == true) {
        dispatch_source_set_timer(ua_AssertionTimer,
                dispatch_time(DISPATCH_TIME_NOW, IDLE_HID_ACTIVITY_SECS * NSEC_PER_SEC),
                DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(ua_AssertionTimer);
    }


    shareUserIdleness();
#endif /* !TARGET_OS_EMBEDDED */
}
