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
#include <IOKit/hid/IOHIDEventSystem.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <libkern/OSThermalNotification.h>

#include "PrivateLib.h"
#include "SystemLoad.h"
#include "PMStore.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMConnection.h"
#include "Platform.h"
#include "powermanagementServer.h" // mig generated
#include "adaptiveDisplay.h"
#include "StandbyTimer.h"

os_log_t    sysLoad_log = NULL;
#undef   LOG_STREAM
#define  LOG_STREAM   sysLoad_log

static int minOfThree(int a, int b, int c);

extern uint32_t                     gDebugFlags;
// Forwards
const bool  kNoNotify  = false;
const bool  kYesNotify = true;
static void shareTheSystemLoad(bool shouldNotify);

static char *sysload_qname = "com.apple.powermanagement.systemload";
static dispatch_queue_t  sysloadQ;

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

static UserActiveStruct gUserActive;

__private_extern__ bool isDisplayAsleep(void);
/************************* ****************************** ********************/

static uint32_t updateUserActivityLevels(void);

#ifdef XCTEST
uint32_t xctUserInactiveDuration = 0;

void xctSetUserInactiveDuration(uint32_t value) {
    xctUserInactiveDuration = value;
}

void xctSetUserActiveRootDomain(bool active) {
    gUserActive.rootDomain = active;
}

bool xctGetUserActiveRootDomain() {
    return gUserActive.rootDomain;
}

uint64_t xctGetUserActivityPostedLevels() {
    return gUserActive.postedLevels;
}

void xctUserActive_prime() {
    bzero(&gUserActive, sizeof(UserActiveStruct));

    gUserActive.postedLevels = kIOPMUserPresentActive;
    xctSetUserActiveRootDomain(true);
}
#endif

static void userActive_prime(void) {
    bzero(&gUserActive, sizeof(UserActiveStruct));

    gUserActive.postedLevels = 0xFFFF; // bogus value
    userActiveHandleRootDomainActivity(true);

}

bool userActiveRootDomain(void)
{
    // returns true when display is on except for
    // notification wake
    return gUserActive.rootDomain;
}
void userActiveHandleSleep(void)
{
    if (gUserActive.rootDomain) {
        gUserActive.sleepFromUserWakeTime = CFAbsoluteTimeGetCurrent();
    }
    gUserActive.rootDomain = false;
}

void userActiveHandlePowerAssertionsChanged()
{
    updateUserActivityLevels();
}

__private_extern__ void resetSessionUserActivity()
{
    gUserActive.sessionUserActivity = false;
    gUserActive.sessionActivityLevels = 0;
}

__private_extern__ uint32_t getSystemThermalState()
{
    return thermalState;
}

/*
 * getSessionUserActivity - returns rootDomain's activity state since 
 * wake. Also returns all activityLevels set in this wake.
 */
__private_extern__ bool getSessionUserActivity(uint64_t *sessionLevels)
{
    if (sessionLevels) {
        *sessionLevels = gUserActive.sessionActivityLevels;
    }
    return gUserActive.sessionUserActivity;
}

void userActiveHandleRootDomainActivity(bool active)
{
    if (active == gUserActive.rootDomain) {
        DEBUG_LOG("No change in userActive rootDomain %d\n", gUserActive.rootDomain);
        return;
    }
    if (active == true) {
        _unclamp_silent_running(true);
        cancel_NotificationDisplayWake();
        cancelPowerNapStates();
        cancelDarkWakeCapabilitiesTimer();
        enableTCPKeepAlive();
        gUserActive.sessionUserActivity = gUserActive.rootDomain = true;

        // Consider this equivalent to an user active assertion.
        // This is because, when the system is woken up by a key press
        // without any more HID activity, lastHid_ts won't change to true
        SystemLoadUserActiveAssertions(true);
    }
    else {
        gUserActive.rootDomain = false;
    }

    INFO_LOG("rootDomain's user activity state:%d\n", gUserActive.rootDomain);
    if (gUserActive.sessionActivityLevels == 0) {
        evaluateADS();
    }
    setVMDarkwakeMode(false);
    evaluateAdaptiveStandby();
}

static uint32_t getUserInactiveDuration()
{
#if XCTEST
    return xctUserInactiveDuration;
#endif
    uint64_t now = getMonotonicContinuousTime();

    uint64_t hidInactivityDuration = 0;
    uint64_t assertionInactivityDuration = UINT64_MAX;

    DEBUG_LOG("getUserInactiveDuration: rdActive:%d hidActive:%d assertionActivityValid:%d now:0x%llx  hid_ts:0x%llx assertion_ts:0x%llx\n",
            gUserActive.rootDomain, gUserActive.hidActive, gUserActive.assertionActivityValid,
            now, gUserActive.lastHid_ts, gUserActive.lastAssertion_ts);

    if (gUserActive.rootDomain) {
        // Hid activity can be checked only if rootDomain declares system is active.
        // For cases like notification wake, hid activity shouldn't be checked. In those cases,
        // rootDomain declares system to be inactive
        if (gUserActive.hidActive) {
            hidInactivityDuration = 0;
        }
        else {
            hidInactivityDuration = now - gUserActive.lastHid_ts;
        }
    }
    else {
        hidInactivityDuration = UINT64_MAX;
    }

    if (gUserActive.assertionActivityValid) {
        assertionInactivityDuration = now - gUserActive.lastAssertion_ts;
    }
    if (hidInactivityDuration < assertionInactivityDuration) {
        return ((uint32_t)hidInactivityDuration);
    }
    else {
        return ((uint32_t)assertionInactivityDuration);
    }
}

uint32_t updateUserActivityLevels(void)
{
    uint64_t            levels = 0;
    bool                presentActive = false;
    uint32_t            nextIdleTimeout = 0;
    uint32_t            inactiveDuration = 0;
    clientInfo_t        *client;
    static int          token = 0;


    if (!displayIsOff && gUserActive.userActive) {
        levels |= kIOPMUserPresentActive;
    }
    else {
        if (checkForActivesByType(kPreventDisplaySleepType) && !isA_NotificationDisplayWake()) {
            levels |= kIOPMUserPresentPassive;
        }

    }
    if (checkForActivesByType(kNetworkAccessType)) {
        levels |= kIOPMUserRemoteClientActive;
    }
    if (checkForActivesByType(kTicklessDisplayWakeType)) {
        levels |= kIOPMUserNotificationActive;
    }
    DEBUG_LOG("Global levels set to 0x%llx\n", levels);

    // Set system activity Level based on the default HID activity timeout(kIOPMDefaultUserActivityTimeout)
    if (0 == token) {
        notify_register_check("com.apple.system.powermanagement.useractivity2",
                              &token);
    }
    if (gUserActive.postedLevels != levels) {
        notify_set_state(token, levels);
        notify_post("com.apple.system.powermanagement.useractivity2");

        INFO_LOG("Activity changes from 0x%llx to 0x%llx. UseActiveState:%d\n",
            gUserActive.postedLevels, levels, gUserActive.userActive);
        INFO_LOG("hidActive:%d displayOff:%d assertionActivityValid:%d now:0x%llx  hid_ts:0x%llx assertion_ts:0x%llx\n",
                gUserActive.hidActive, displayIsOff, gUserActive.assertionActivityValid,
                getMonotonicContinuousTime(), gUserActive.lastHid_ts, gUserActive.lastAssertion_ts);

        if (((gUserActive.postedLevels & kIOPMUserNotificationActive) == 0) &&
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

        gUserActive.postedLevels = levels;
        gUserActive.sessionActivityLevels |= levels;
    }


    inactiveDuration = getUserInactiveDuration();

    // Now, process per client activity level
    LIST_FOREACH(client, &gUserActive.clientList, link)  {

        // Unset kIOPMUserPresentActive bit to evalute it per client
        levels = (gUserActive.postedLevels & ~kIOPMUserPresentActive);
        presentActive = false;


        // User is active if display is on AND  either there are active assertions or hid idleness is
        // less than 'idleTimeout'
        if (!displayIsOff && (inactiveDuration < client->idleTimeout)) {
            presentActive = true;
        }
        if (presentActive) {
            levels &= ~(kIOPMUserPresentPassive|kIOPMUserPresentPassiveWithDisplay|kIOPMUserPresentPassiveWithoutDisplay);
            levels |= kIOPMUserPresentActive;
        }
        else {
            if (!displayIsOff) {
                bool displayAssertionsExist, audioAssertionsExist;
                // kIOPMUserPresentPassiveWithDisplay is set if display is on due to
                // some app requesting display to be on. Display on just due to user settings
                // that prevent display sleep should not set kIOPMUserPresentPassiveWithDisplay.
                displayAssertionsExist = checkForActivesByType(kPreventDisplaySleepType);
                audioAssertionsExist = checkForAudioType();
                if (displayAssertionsExist) {
                    levels |= (kIOPMUserPresentPassive|kIOPMUserPresentPassiveWithDisplay);
                }
                else if (audioAssertionsExist) {
                    levels |= (kIOPMUserPresentPassive|kIOPMUserPresentPassiveWithoutDisplay);
                }
            }
            else if (checkForAudioType() && !isA_DarkWakeState()) {
                levels |= (kIOPMUserPresentPassive|kIOPMUserPresentPassiveWithoutDisplay);
            }
        }

        if (inactiveDuration && (inactiveDuration < client->idleTimeout)) {
            // If HID's idle notification is received, then remeber the 'idleTimeout'
            // of the next client to get notification
            if (!nextIdleTimeout) {
                nextIdleTimeout = client->idleTimeout;
            }
        }


        if (client->postedLevels != levels) {
            xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);

            DEBUG_LOG("Sending new activity levels(0x%llx) to client %p(pid %d)\n",
                    levels, client->connection, xpc_connection_get_pid(client->connection));

            xpc_dictionary_set_uint64(msg, kUserActivityLevels, levels);
            xpc_connection_send_message(client->connection, msg);
            xpc_release(msg);

            client->postedLevels = levels;
        }
        else {
            DEBUG_LOG("Client %p(pid %d) activity level is already at 0x%llx\n",
                    client->connection, xpc_connection_get_pid(client->connection), levels);
        }

    }

    return nextIdleTimeout;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*! PresentActive user detector
 */
static void updateUserActivityState(int state)
{

    if (state != gUserActive.presentActive) {
       if (state) {
           /* new PresentActive == true */
           notify_set_state(gUserActive.token, (uint64_t)kIOUserIsActive);
       }
       else  {
           /* new PresentActive == false */
           notify_set_state(gUserActive.token, (uint64_t)kIOUserIsIdle);
       }

       notify_post(kIOUserActivityNotifyName);

       DEBUG_LOG("PresentActive changes from %d to %d. UserActivityState:%d displayIsOff:%d\n",
               gUserActive.presentActive, state, gUserActive.userActive, displayIsOff);
       gUserActive.presentActive = state;

       updateUserActivityLevels();
    }


}

CFAbsoluteTime get_SleepFromUserWakeTime(void)
{
    return gUserActive.sleepFromUserWakeTime;
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
        else if (thermalPressureLevel == kOSThermalPressureLevelModerate)
        {
            powerLevel = kIOSystemLoadAdvisoryLevelOK;
        }
        else {
            // heavy or trapping or sleeping
            powerLevel = kIOSystemLoadAdvisoryLevelBad;
        }
    }

    // TODO: Use seconds since last UI activity as an indicator of
    // userLevel. Basing this data on display dimming is a crutch,
    // and may be invalid on systems with display dimming disabled.
    if (gUserActive.loggedIn) {
        if (displayIsOff) {
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





static void evaluateHidIdleNotification()
{

    static dispatch_source_t hidIdleEval = 0;
    static bool hidIdleEvalSuspended = true;
    uint32_t    nextIdleTimeout;
    uint32_t inactiveDuration = 0;
    uint32_t legacyNextIdleTimeout = 0;

    inactiveDuration = getUserInactiveDuration();

    DEBUG_LOG("inactiveDuration:%d gUserActive.userActive:%d\n",
            inactiveDuration, gUserActive.userActive);

    if (displayIsOff || (inactiveDuration >= kIOPMDefaultUserActivityTimeout)) {
        gUserActive.userActive = false;
        updateUserActivityState(kIOUserIsIdle);
    }
    else {
        gUserActive.userActive = true;
        updateUserActivityState(kIOUserIsActive);
        legacyNextIdleTimeout = kIOPMDefaultUserActivityTimeout;
    }

    if (!hidIdleEval) {
        hidIdleEval = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());

        dispatch_source_set_event_handler(hidIdleEval, ^{ evaluateHidIdleNotification(); });

        dispatch_source_set_cancel_handler(hidIdleEval, ^{
                dispatch_release(hidIdleEval);
                hidIdleEval = 0;
                });
    }

    nextIdleTimeout = updateUserActivityLevels();
    DEBUG_LOG("nextIdleTimeout: %d legacyNextIdleTimeout:%d\n", nextIdleTimeout, legacyNextIdleTimeout);
    if (nextIdleTimeout || legacyNextIdleTimeout) {
        dispatch_time_t delta2NextIdleTimeout;

        if ( !nextIdleTimeout || (legacyNextIdleTimeout && (nextIdleTimeout > legacyNextIdleTimeout))) {
            nextIdleTimeout = legacyNextIdleTimeout;
        }
        if (inactiveDuration > nextIdleTimeout) {
            ERROR_LOG("Unexpected values. inactiveDuration:%d nextIdleTimeout:%d\n",
                    inactiveDuration, nextIdleTimeout);
            return;
        }

        delta2NextIdleTimeout = dispatch_time(DISPATCH_TIME_NOW, (nextIdleTimeout-inactiveDuration)*NSEC_PER_SEC);

        dispatch_source_set_timer(hidIdleEval, delta2NextIdleTimeout, DISPATCH_TIME_FOREVER, 0);
        if (hidIdleEvalSuspended) {
            dispatch_resume(hidIdleEval);
            hidIdleEvalSuspended = false;
        }
    }
    else if (!hidIdleEvalSuspended) {
        dispatch_suspend(hidIdleEval);
        hidIdleEvalSuspended = true;
    }

}

CFDictionaryRef __nonReceivingEventMatching()
{

  CFNumberRef keyValue;
  uint32_t    value;
  CFMutableDictionaryRef matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  value = 0xffff;
  keyValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  if (keyValue) {
    CFDictionaryAddValue(matching, CFSTR(kIOHIDServiceDeviceUsageKey), keyValue);
    CFDictionaryAddValue(matching, CFSTR(kIOHIDServiceDeviceUsagePageKey), keyValue);
    CFRelease(keyValue);
  }
  return matching;
}

// This function should be called on 'sysloadQ'.
uint64_t __IOHIDEventSystemClientCopyIntegerProperty(CFStringRef key)
{
    uint64_t value = -1;

    dispatch_assert_queue(sysloadQ);
    if (gUserActive.hidClient == 0) {
        return value;
    }
    CFNumberRef property = (CFNumberRef)IOHIDEventSystemClientCopyProperty(gUserActive.hidClient, key);
    if (property) {
        CFNumberGetValue(property, kCFNumberSInt64Type, &value);
        CFRelease(property);
    }
    return value;
}

// This function should be called on 'sysloadQ'.
void __IOHIDEventSystemClientSetIntegerProperty(CFStringRef key, uint64_t value)
{

    dispatch_assert_queue(sysloadQ);
    if (gUserActive.hidClient == 0) {
        return;
    }
    CFNumberRef property = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
    if (property) {
        IOHIDEventSystemClientSetProperty(gUserActive.hidClient, key, property);
        CFRelease(property);
    }
}

// This function gets called on 'sysloadQ'.
void hidPropertyCallback( void * target, void * context, CFStringRef property, CFTypeRef value)
{
    dispatch_assert_queue(sysloadQ);

    int state = (int)__IOHIDEventSystemClientCopyIntegerProperty(CFSTR(kIOHIDActivityStateKey));
    uint64_t ts = 0;

    DEBUG_LOG("HID activity callback thru property callback. state:%d\n", state);
    if (state == 1) {
        ts = getMonotonicContinuousTime();
    }
    else if (state == 0) {
        ts = monotonicTS2Secs(__IOHIDEventSystemClientCopyIntegerProperty(CFSTR(kIOHIDLastActivityTimestampKey)));
    }

    // Evaluate on main queue
    dispatch_async(_getPMMainQueue(), ^{
        if (state == 1) {
            gUserActive.hidActive = true;
        }
        else {
            gUserActive.hidActive = false;
        }
        gUserActive.lastHid_ts = ts;
        evaluateHidIdleNotification();
    });
}

static void registerForHidActivity()
{

    dispatch_async(sysloadQ, ^{
        gUserActive.hidClient = IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
        if (gUserActive.hidClient == NULL) {
            ERROR_LOG("Failed to create hid client\n");
            return;
        }
        CFDictionaryRef matching = __nonReceivingEventMatching ();

        IOHIDEventSystemClientSetMatching (gUserActive.hidClient, matching);


        IOHIDEventSystemClientRegisterPropertyChangedCallback (gUserActive.hidClient, CFSTR(kIOHIDActivityStateKey), hidPropertyCallback, NULL, NULL);
        IOHIDEventSystemClientScheduleWithDispatchQueue (gUserActive.hidClient, sysloadQ);

        __IOHIDEventSystemClientSetIntegerProperty (CFSTR(kIOHIDIdleNotificationTimeKey), gUserActive.idleTimeout);

        hidPropertyCallback(NULL, NULL, NULL, NULL);
    });
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__private_extern__ void SystemLoad_prime(void)
{
    IONotificationPortRef       notify_port = 0;
    int                         token;

    sysLoad_log = os_log_create(PM_LOG_SYSTEM, SYSLOAD_LOG);
    sysloadQ = dispatch_queue_create(sysload_qname, DISPATCH_QUEUE_SERIAL);
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

    notify_register_check(kIOUserActivityNotifyName, &gUserActive.token);
    notify_set_state(gUserActive.token, (uint64_t)kIOUserIsActive);

    // If this is a desktop, then we won't get any battery notifications.
    // Let's prime the battery pump right here with an initial coll.
    int count = _batteryCount();
    dispatch_sync(BatteryTimeRemaining_getQ(), ^() {
        SystemLoadBatteriesHaveChanged(count);
    });

    SystemLoadPrefsHaveChanged();

    SystemLoadUserStateHasChanged();

    SystemLoadCPUPowerHasChanged(NULL);

    notify_port = IONotificationPortCreate(0);
    IONotificationPortSetDispatchQueue(notify_port, _getPMMainQueue());

    gUserActive.idleTimeout = kIOPMDefaultUserActivityTimeout;

    gUserActive.lastHid_ts = getMonotonicContinuousTime();
    gUserActive.hidActive = true;

    gUserActive.lastAssertion_ts = 0;
    gUserActive.assertionActivityValid = false;

    registerForHidActivity();
    displayIsOff = isDisplayAsleep();



    notify_register_dispatch( kOSThermalNotificationPressureLevelName,
                              &token, _getPMMainQueue(),
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
        // Force set assertionActivityValid to false to ignore
        // assertions created prior to display sleep
        gUserActive.assertionActivityValid = 0;
        userActiveHandleRootDomainActivity(false);
    } else {
        // display is on. Update UserActiveRootDomain
        if (!isA_NotificationDisplayWake()){
            DEBUG_LOG("Display is on: Not a notification wake. Updating user active rootdomain");
            userActiveHandleRootDomainActivity(true);
        } else {
            DEBUG_LOG("Display is on: Is a notification wake");
        }
    }
    INFO_LOG("Display state: %s NotificationWake : %d\n", (displayIsOff ? "Off" : "On"), isA_NotificationDisplayWake());

    shareTheSystemLoad(kYesNotify);
    evaluateHidIdleNotification();
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
__private_extern__ void SystemLoadBatteriesHaveChanged(int count)
{
    dispatch_assert_queue(BatteryTimeRemaining_getQ());

    static const int kBatThreshold = 40;
    int sumMax = 0;
    int sumCurrent = 0;
    int i;

    bool local_onACPower = false;
    bool local_onBatteryPower = false;
    bool local_batteryBelowThreshold = false;

    if (0 == count) {
        local_onACPower = true;
        goto exit;
    }

    IOPMBattery **batt_stats = _batteries();

    for (i=0; i<count; i++) {
        if( batt_stats[i]->externalConnected ) {
            local_onACPower = true;
        }
        sumCurrent += batt_stats[i]->currentCap;
        sumMax += batt_stats[i]->maxCap;
    }

    if (!local_onACPower) {
        local_onBatteryPower = true;
    }

    if (sumMax && (kBatThreshold > (100*sumCurrent)/sumMax)) {
        local_batteryBelowThreshold = true;
    }

exit:
    dispatch_async(_getPMMainQueue(), ^() {
        onACPower = local_onACPower;
        onBatteryPower = local_onBatteryPower;
        batteryBelowThreshold = local_batteryBelowThreshold;
        shareTheSystemLoad(kYesNotify);
    });
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

/* @function SystemLoadUserStateHasChanged
 * Populates:
 *  gUserActive.loggedIn
 *  loggedInUserIdle
 *  switchedOutUsers
 *  remoteConnections
 */
__private_extern__ void SystemLoadUserStateHasChanged(void)
{
    CFStringRef         loggedInUserName;

    gUserActive.loggedIn = false;

    loggedInUserName = SCDynamicStoreCopyConsoleUser(_getSharedPMDynamicStore(),
                                                    NULL,  // uid
                                                    NULL); // gid
    if (loggedInUserName) {
        gUserActive.loggedIn = true;
        CFRelease(loggedInUserName);
    }

    shareTheSystemLoad(kYesNotify);
}

__private_extern__ void SystemLoadSystemPowerStateHasChanged(void)
{
    shareTheSystemLoad(kYesNotify);
}

/*! SystemLoadUserActiveAssertions
 *  Called when new user-active assertion is created
 */
static void setAssertionIdleNotificationTimer()
{
    static dispatch_source_t assertionEval = 0;
    static bool assertionEvalSuspended = true;
    uint64_t now = getMonotonicContinuousTime();

    if (!gUserActive.lastAssertion_ts) {
        // Timer is not required if there are no assertions
        return;
    }
    if (!assertionEval) {
        assertionEval = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());

        dispatch_source_set_event_handler(assertionEval, ^{ SystemLoadUserActiveAssertions( false ); });

        dispatch_source_set_cancel_handler(assertionEval, ^{
                dispatch_release(assertionEval);
                assertionEval = 0;
                });
    }

    if (gUserActive.idleTimeout > (now - gUserActive.lastAssertion_ts)) {
        dispatch_time_t delta = dispatch_time(DISPATCH_TIME_NOW,
                              (gUserActive.idleTimeout - ( now - gUserActive.lastAssertion_ts))*NSEC_PER_SEC);
        dispatch_source_set_timer(assertionEval, delta, DISPATCH_TIME_FOREVER, 0);
        if (assertionEvalSuspended) {
            dispatch_resume(assertionEval);
            assertionEvalSuspended = false;
        }
    }
}

__private_extern__ void SystemLoadUserActiveAssertions(bool active)
{

    if (active) {

        // As there is no notification when assertion is released, we need to
        // post a timer to reset the assertion state
        gUserActive.lastAssertion_ts = getMonotonicContinuousTime();
        setAssertionIdleNotificationTimer();
        gUserActive.assertionActivityValid = true;
    }

    evaluateHidIdleNotification( );

}


static int minOfThree(int a, int b, int c)
{
    int result = a;

    if (b < result) result = b;
        if (c < result) result = c;

        return result;
}

static void insertClient(clientInfo_t *client)
{
    clientInfo_t *iter, *prev = NULL;

    if (client->idleTimeout < kMinIdleTimeout) {
        ERROR_LOG("Invalid idleTimeout value %d\n", client->idleTimeout);
        client->idleTimeout = kMinIdleTimeout;
    }

    if (client->idleTimeout < gUserActive.idleTimeout) {
        gUserActive.idleTimeout = client->idleTimeout;

        DEBUG_LOG("Changing idle activity timeout notification to %d secs\n", gUserActive.idleTimeout);
        dispatch_async(sysloadQ, ^{
            __IOHIDEventSystemClientSetIntegerProperty (CFSTR(kIOHIDIdleNotificationTimeKey), gUserActive.idleTimeout);
        });
        setAssertionIdleNotificationTimer();
    }


    if (LIST_EMPTY(&gUserActive.clientList) ) {
        LIST_INSERT_HEAD(&gUserActive.clientList, client, link);
    }
    else {
        LIST_FOREACH(iter, &gUserActive.clientList, link)
        {
            prev = iter;
            if (iter->idleTimeout > client->idleTimeout)
                break;
        }
        if (iter)
            LIST_INSERT_BEFORE(iter, client, link);
        else
            LIST_INSERT_AFTER(prev, client, link);
    }


    // force a re-evaluation with existing user activity status
    client->postedLevels = ULONG_MAX;
    evaluateHidIdleNotification( );

}

__private_extern__ void registerUserActivityClient(xpc_object_t connection, xpc_object_t msg)
{

    if (!connection || !msg) {
        ERROR_LOG("Invalid args for UserActivity client registration(%p, %p)\n",
                connection, msg);
        return;
    }

    clientInfo_t *client = calloc(1, sizeof(clientInfo_t));
    if (!client) {
        ERROR_LOG("Failed allocate memory\n");
        return;
    }

    client->connection = xpc_retain(connection);
    client->idleTimeout = (uint32_t)xpc_dictionary_get_uint64(msg, kUserActivityTimeoutKey);
    insertClient(client);

    DEBUG_LOG("Registered user inactivity client %p(pid %d) with timeout(%d)\n",
                 connection, xpc_connection_get_pid(connection), client->idleTimeout);
}

void deRegisterUserActivityClient(xpc_object_t connection)
{
    clientInfo_t *client, *nextClient;

    if (!connection) {
        ERROR_LOG("Invalid args for UserActivity client deregistration(%p)\n",
                connection);
        return;
    }
    client = LIST_FIRST(&gUserActive.clientList);
    while( client )
    {
        nextClient = LIST_NEXT(client, link);
        if (client->connection == connection) {
            LIST_REMOVE(client, link);
            break;
        }
        client = nextClient;
    }

    if (!client) {
        return;
    }

    xpc_release(client->connection);
    free(client);

    DEBUG_LOG("Deregistered user inactivity client %p(pid %d)\n",
                 connection, xpc_connection_get_pid(connection));
}


void updateUserActivityTimeout(xpc_object_t connection, xpc_object_t msg)
{
    clientInfo_t *client, *nextClient;

    if (!connection || !msg) {
        ERROR_LOG("Invalid args UserActivity client timeout update(%p, %p)\n",
                connection, msg);
        return;
    }

    // Remove the client from the list
    client = LIST_FIRST(&gUserActive.clientList);
    while( client )
    {
        nextClient = LIST_NEXT(client, link);
        if (client->connection == connection) {
            LIST_REMOVE(client, link);
            break;
        }
        client = nextClient;
    }

    if (!client) {
        ERROR_LOG("Update request from unexpected connection(%p)(pid:%d)\n",
                connection, xpc_connection_get_pid(connection));
        return;
    }

    // Insert back with new values
    client->idleTimeout = (uint32_t)xpc_dictionary_get_uint64(msg, kUserActivityTimeoutKey);
    insertClient(client);

    DEBUG_LOG("Updated idleTimeout to %d for  user inactivity client %p(pid %d)\n",
                 client->idleTimeout, client->connection, xpc_connection_get_pid(connection));
}

__private_extern__ uint32_t getTimeSinceLastTickle( )
{
    __block uint64_t  hidActivity_ts = 0;

#if 0
    // NOTE: This section is required on iOS to get last tickle timestamp from backboardd.
    // Hid activity tickle time stamp is not required on macOS as an
    // assertion update is guaranteed for each HID activity.
    dispatch_assert_queue_not(_getPMMainQueue());
    dispatch_sync(sysloadQ, ^{
        hidActivity_ts = monotonicTS2Secs(__IOHIDEventSystemClientCopyIntegerProperty(
                CFSTR(kIOHIDLastActivityTimestampKey)));
    });
#endif
    uint64_t  currTime = getMonotonicContinuousTime();

    if (hidActivity_ts > gUserActive.lastAssertion_ts) {
        return (currTime > hidActivity_ts) ? (uint32_t)(currTime - hidActivity_ts) : 0;
    }
    else {
        return (currTime > gUserActive.lastAssertion_ts) ? (uint32_t)(currTime - gUserActive.lastAssertion_ts) : 0;
    }


}
