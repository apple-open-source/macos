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
#include <libkern/OSThermalNotification.h>

#include "PrivateLib.h"
#include "SystemLoad.h"
#include "PMStore.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMConnection.h"
#include "Platform.h"

#ifndef  kIOHIDSystemUserHidActivity
#define kIOHIDSystemUserHidActivity    iokit_family_msg(sub_iokit_hidsystem, 6)
#endif

#define IDLE_HID_ACTIVITY_SECS ((uint64_t)(5*60))
static int minOfThree(int a, int b, int c);

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

static bool   displayIsOff                = FALSE;
static bool   displaySleepEnabled       = FALSE;

static int    gNotifyToken              = 0;

static uint32_t thermalState            = kIOPMThermalWarningLevelNormal;
static uint64_t thermalPressureLevel    = kOSThermalPressureLevelNominal;
static int      tpl_supported           = 0;


/*! UserActiveStruct records the many data sources that affect
 *  our concept of user-is-active; and the user's activity level.
 *
 *  Track every aspect of "user is active on the system" in this struct
 *  right here.
 *  Anyplace in powerd that we consider "is the user active"; that
 *  data needs to go through UserActiveStruct and the functions that
 *  operate on it.
 */
typedef struct {
    int     token;
    bool    assertionsActive;
    dispatch_source_t timer;

    /*! presentActive: user has been present and active within 5 minutes
     */
    bool    presentActive;

    /*! hidActive returns true when HID has seen received a HID packet
     *  within < 5 minutes.
     *  IOHIDFamily/IOHIDSystem.cpp implements this 5 minute timeout
     *  using constant IDLE_HID_ACTIVITY_NSECS */
    bool    hidActive;

    /*! loggedIn is true if there's a console user logged in to
     *  loginWindow. Also returns true for ScreenShared users.
     */
    bool    loggedIn;

    /*! rootDomain tracks the IOPMrootDomain's concept of user activity, as
     *  decided by root domain's policy surrounding kStimulusUserIsActive
     *  and kStimulusUserIsInactive.
     *  By definition, rootDomain is true when the system is in S0 notification
     *  wake, and set to false on asleep.
     */
    bool    rootDomain;

    /*! sessionUserActivity tracks if user was ever active since last full wake.
     * This is reset when system enters dark wake state.
     */
    bool    sessionUserActivity;

    /*! sleepFromUserWakeTime is a timestamp tracking the last time the system
     *  was in full S0 user wake, and it went to sleep.
     *  We will not update this timestamp on maintenance wakes.
     */
    CFAbsoluteTime  sleepFromUserWakeTime;

    /*! postedLevels is the last set of user activity levels we've published.
     *  This corresponds to the currently available return value to
     *  processes calling IOPMGetUserActivityLevel().
     */
    uint64_t postedLevels;
} UserActiveStruct;

static UserActiveStruct userActive;

/************************* ****************************** ********************/

static void updateUserActivityLevels(void);

static void userActive_prime(void) {
    bzero(&userActive, sizeof(UserActiveStruct));

    userActive.postedLevels = 0xFFFF; // bogus value
    userActiveHandleRootDomainActivity();
}

bool userActiveRootDomain(void)
{
    return userActive.rootDomain;
}
void userActiveHandleSleep(void)
{
    if (userActive.rootDomain) {
        userActive.sleepFromUserWakeTime = CFAbsoluteTimeGetCurrent();
    }
    userActive.rootDomain = false;
}

void userActiveHandlePowerAssertionsChanged()
{
    updateUserActivityLevels();
}

__private_extern__ void resetSessionUserActivity()
{
    userActive.sessionUserActivity = false;
}

__private_extern__ uint32_t getSystemThermalState()
{
    return thermalState;
}

__private_extern__ bool getSessionUserActivity()
{
    return userActive.sessionUserActivity;
}

void userActiveHandleRootDomainActivity(void)
{
    CFBooleanRef    userIsActive = NULL;

    userIsActive = IORegistryEntryCreateCFProperty(getRootDomain(),
                                                   CFSTR(kIOPMUserIsActiveKey),
                                                   0, 0);
    if (userIsActive == kCFBooleanTrue) {
        _unclamp_silent_running(true);
        cancel_NotificationDisplayWake();
        cancelPowerNapStates();
#if TCPKEEPALIVE
        enableTCPKeepAlive();
#endif
        userActive.sessionUserActivity = userActive.rootDomain = true;

        // Consider this equivalent to an user active assertion.
        // This is because, when the system is woken up by a key press
        // without any more HID activity, hidActive won't change to true
        SystemLoadUserActiveAssertions(true);
        updateUserActivityLevels( );
    }

    if (userIsActive) {
        CFRelease(userIsActive);
    }
}

void updateUserActivityLevels(void)
{
    static int          token = 0;
    uint64_t            levels = 0;


    if (userActive.presentActive) {
        levels |= kIOPMUserPresentActive;
    }
    if (checkForActivesByType(kPreventDisplaySleepType)) {
        levels |= kIOPMUserPresentPassive;
    }
    if (checkForActivesByType(kNetworkAccessType)) {
        levels |= kIOPMUserRemoteClientActive;
    }
    if (checkForActivesByType(kTicklessDisplayWakeType)) {
        levels |= kIOPMUserNotificationActive;
    }

    if (0 == token) {
        notify_register_check("com.apple.system.powermanagement.useractivity2",
                              &token);
    }
    if (userActive.postedLevels != levels) {
        notify_set_state(token, levels);
        notify_post("com.apple.system.powermanagement.useractivity2");

        if (((userActive.postedLevels & kIOPMUserNotificationActive) == 0) &&
            (levels & kIOPMUserNotificationActive)) {
            /*
             * kIOPMUserNotificationActive is being set. Make sure this notification 
             * is visible before proceeding. This notification has to reach the clients
             * before display gets turned on(rdar://problem/18344363).
             */
            int attempts = 0;
            uint64_t newstate = 0;
            while (attempts++ < 10) {
                notify_get_state(token, &newstate);
                if (newstate == levels)
                    break;
                usleep(1000); // 10ms delay
            }

        }
        userActive.postedLevels = levels;
    }
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*! PresentActive user detector 
 */
static void updateUserPresentActive( )
{
#if !TARGET_OS_EMBEDDED
   bool presentActive = false;

    if (userActive.assertionsActive
        || userActive.hidActive)
    {
        presentActive = true;
    }

    if (!userActive.loggedIn || displayIsOff) {
        presentActive = false;
    }

    if (presentActive != userActive.presentActive) {
       if (presentActive) {
           /* new PresentActive == true */
           notify_set_state(userActive.token, (uint64_t)kIOUserIsActive);
       }
       else  {
           /* new PresentActive == false */
           notify_set_state(userActive.token, (uint64_t)kIOUserIsIdle);
       }

       notify_post(kIOUserActivityNotifyName);

       userActive.presentActive = presentActive;

       updateUserActivityLevels();
    }

#endif

}

CFAbsoluteTime get_SleepFromUserWakeTime(void)
{
    return userActive.sleepFromUserWakeTime;
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
    
    if (!tpl_supported) {
        // Check plimits and GFI only if pressure levels are not
        // published for the platform.
        // Pressure levels, if published, takes hysterisis of
        // plimits into account and reflects more accurate state.
        if (plimitBelowThreshold) {
            powerLevel = kIOSystemLoadAdvisoryLevelOK;
        }
        if (coresConstrained || forcedIdle || thermalWarningLevel) {
            powerLevel = kIOSystemLoadAdvisoryLevelBad;
        }
    }
    else {
        if (thermalPressureLevel == kOSThermalPressureLevelNominal) {
            powerLevel = kIOSystemLoadAdvisoryLevelGreat;
        }
        else if ((thermalPressureLevel == kOSThermalPressureLevelModerate) ||
            (thermalPressureLevel == kOSThermalPressureLevelHeavy))
        {
            powerLevel = kIOSystemLoadAdvisoryLevelOK;
        }
        else {
            // trapping or sleeping
            powerLevel = kIOSystemLoadAdvisoryLevelBad;
        }
    }

    // TODO: Use seconds since last UI activity as an indicator of
    // userLevel. Basing this data on display dimming is a crutch,
    // and may be invalid on systems with display dimming disabled.
    if (userActive.loggedIn) {
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


   userActive.hidActive = ((uint64_t)arg) ? false : true;

    updateUserPresentActive();
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
            bool hidIdle;
            IOHIDGetActivityState(connect, &hidIdle);
            userActive.hidActive = !hidIdle;

            IOServiceClose(connect);
            updateUserPresentActive();
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
    int                         token;

    userActive_prime();

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

    notify_register_check(kIOUserActivityNotifyName, &userActive.token);
    notify_set_state(userActive.token, (uint64_t)kIOUserIsActive);

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
    
    notify_register_dispatch( kOSThermalNotificationPressureLevelName, 
                              &token, dispatch_get_main_queue(),
                              ^(int token) {
                                    notify_get_state(token, &thermalPressureLevel);
                                    tpl_supported = 1;
                                    shareTheSystemLoad(kYesNotify);
                              });

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
    if (displayIsOff) {
        // Force set user active assertions to false
        userActive.assertionsActive = false;
        dispatch_suspend(userActive.timer);
    }
    
    shareTheSystemLoad(kYesNotify);
    updateUserPresentActive();
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
    ret = IOPMGetThermalWarningLevel(&thermalState);
    if ( (kIOReturnSuccess == ret)
        && (kIOPMThermalWarningLevelNormal != thermalState)
        && (kIOPMThermalLevelUnknown != thermalState))
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
 *  userActive.loggedIn
 *  loggedInUserIdle
 *  switchedOutUsers
 *  remoteConnections
 */
__private_extern__ void SystemLoadUserStateHasChanged(void)
{
    CFStringRef         loggedInUserName;

    userActive.loggedIn = false;

    loggedInUserName = SCDynamicStoreCopyConsoleUser(_getSharedPMDynamicStore(),
                                                    NULL,  // uid
                                                    NULL); // gid
    if (loggedInUserName) {
        userActive.loggedIn = true;
        CFRelease(loggedInUserName);
    }

    shareTheSystemLoad(kYesNotify);

    updateUserPresentActive( );
}

__private_extern__ void SystemLoadSystemPowerStateHasChanged(void)
{
    shareTheSystemLoad(kYesNotify);
}
#endif /* !TARGET_OS_EMBEDDED */

/*! SystemLoadUserActiveAssertions
 *  This timer fires 5 minutes after UserActive assertion was created,
 *  e.g. after last user input.
 */
__private_extern__ void SystemLoadUserActiveAssertions(bool _userActiveAssertions)
{
#if !TARGET_OS_EMBEDDED
    static uint64_t    userActive_ts = 0;

    if (userActive.timer == 0) {
        userActive.timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0,
                            0, dispatch_get_main_queue());

        dispatch_source_set_event_handler(userActive.timer, ^{
                SystemLoadUserActiveAssertions(false);
                });

        dispatch_source_set_cancel_handler(userActive.timer, ^{
            dispatch_release(userActive.timer);
            userActive.timer = 0;
        });
    }

    /* Determine whether 5 minutes have elapsed since
     * UserActive assertion was created.
     */
    if (_userActiveAssertions == false) {
        uint64_t curTime = getMonotonicTime();
        dispatch_suspend(userActive.timer);
        if ( (curTime - userActive_ts) < IDLE_HID_ACTIVITY_SECS) {
            dispatch_source_set_timer(userActive.timer,
                    dispatch_time(DISPATCH_TIME_NOW, 
                        (IDLE_HID_ACTIVITY_SECS+userActive_ts-curTime)*NSEC_PER_SEC), 
                    DISPATCH_TIME_FOREVER, 0);
            dispatch_resume(userActive.timer);
            return;
        }
    }
    else {
        userActive_ts = getMonotonicTime();
    }

    if (userActive.assertionsActive == _userActiveAssertions)
       return;

    /* UserActive assertion either just raised, or just released.
     * We'll update the User PresentActive level accordingly.
     */

    userActive.assertionsActive = _userActiveAssertions;

    updateUserPresentActive();


    /* TODO: Can we remove this timer polling, since we're getting a timeout
     * above?
     */
    if (userActive.assertionsActive == true) {
        dispatch_source_set_timer(userActive.timer,
                dispatch_time(DISPATCH_TIME_NOW, IDLE_HID_ACTIVITY_SECS * NSEC_PER_SEC),
                DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(userActive.timer);
    }

#endif /* !TARGET_OS_EMBEDDED */
}


static int minOfThree(int a, int b, int c)
{
    int result = a;

    if (b < result) result = b;
        if (c < result) result = c;

        return result;
}
