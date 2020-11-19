/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
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

#include "pmconfigd.h"

#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
#include <reboot2.h>

#ifndef kIOPMMessageRequestSystemShutdown
#define kIOPMMessageRequestSystemShutdown \
        iokit_family_msg(sub_iokit_powermanagement, 0x470)
#endif
#endif /* (TARGET_OS_OSX && TARGET_CPU_ARM64) */

/* load
 *
 * configd entry point
 */
// Global keys
static CFStringRef              gTZNotificationNameString           = NULL;

IOPMNotificationHandle          gESPreferences                      = 0;

static io_connect_t             _pm_ack_port                        = 0;

static int                      gCPUPowerNotificationToken          = 0;
static bool                     gExpectingWakeFromSleepClockResync  = false;
static CFAbsoluteTime           *gLastWakeTime                      = NULL;
static CFTimeInterval           *gLastSMCS3S0WakeInterval           = NULL;
static CFStringRef              gCachedNextSleepWakeUUIDString      = NULL;
#if !TARGET_OS_IPHONE
static int                      gLastWakeTimeToken                  = -1;
static int                      gLastSMCS3S0WakeIntervalToken       = -1;
#endif

static LoginWindowNotifyTokens  lwNotify = {0,0,0,0,0};

static CFStringRef              gConsoleNotifyKey                   = NULL;
#if !(TARGET_OS_OSX && TARGET_CPU_ARM64)
static bool                     gDisplayIsAsleep = false;
#endif
static struct timeval           gLastSleepTime                      = {0, 0};

static mach_port_t              serverPort                          = MACH_PORT_NULL;
static dispatch_mach_t          gListener;
#if !TARGET_OS_IPHONE
static bool                     gSMCSupportsWakeupTimer             = true;
static int                      _darkWakeThermalEventCount          = 0;
static bool                     gEvaluateDWThermalEmergency = false;
#endif

static natural_t                lastSleepWakeMsg                    = 0;


// foward declarations
static void initializeESPrefsNotification(void);
static void initializeInterestNotifications(void);
static void initializeTimezoneChangeNotifications(void);
static void initializeCalendarResyncNotification(void);
static void initializeShutdownNotifications(void);
static void initializeRootDomainInterestNotifications(void);
#if !TARGET_OS_IPHONE
static void initializeUserNotifications(void);
static void enableSleepWakeWdog(void);
#if !(TARGET_OS_OSX && TARGET_CPU_ARM64)
static void displayMatched(void *, io_iterator_t);
static void displayPowerStateChange(
                                    void *ref,
                                    io_service_t service,
                                    natural_t messageType,
                                    void *arg);
#endif


#endif
static void initializeSleepWakeNotifications(void);

static void SleepWakeCallback(void *,io_service_t, natural_t, void *);
static void ESPrefsHaveChanged(void);
static void RootDomainInterest(void *, io_service_t, natural_t, void *);
static void broadcastGMTOffset(void);

static void pushNewSleepWakeUUID(void);

static void calendarRTCDidResyncHandler(
                                        void *context,
                                        dispatch_mach_reason_t reason,
                                        dispatch_mach_msg_t msg,
                                        mach_error_t error);

static void timeZoneChangedCallBack(
                                    CFNotificationCenterRef center,
                                    void *observer,
                                    CFStringRef notificationName,
                                    const void *object,
                                    CFDictionaryRef userInfo);

static void incoming_XPC_connection(xpc_connection_t);
static void xpc_register(void);

static dispatch_mach_t gListener;


#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static void
pmListenerHandler(void *context, dispatch_mach_reason_t reason,
                  dispatch_mach_msg_t msg, mach_error_t error)
{
    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED) {
        static const struct mig_subsystem *const subsystems[] = {
            (mig_subsystem_t)&_powermanagement_subsystem,
        };
        if (!dispatch_mach_mig_demux(NULL, subsystems, 1, msg)) {
            mach_msg_destroy(dispatch_mach_msg_get_msg(msg, NULL));
        }
    }
}

static void
powerd_init(void *__unused context)
{
    kern_return_t           kern_result = 0;

    kern_result = bootstrap_check_in(
                            bootstrap_port,
                            kIOPMServerBootstrapName,
                            &serverPort);

    xpc_register();

    if (BOOTSTRAP_SUCCESS != kern_result) {
        syslog(LOG_ERR, "PM configd: bootstrap_register \"%s\" error = %d\n",
                                kIOPMServerBootstrapName, kern_result);
    }

    if (MACH_PORT_NULL != serverPort)
    {
        gListener = dispatch_mach_create_f("PowerManagement", _getPMMainQueue(), NULL, pmListenerHandler);
        dispatch_mach_connect(gListener, serverPort, MACH_PORT_NULL, NULL);
    }

    PMStoreLoad();

    initializeESPrefsNotification();
    initializeInterestNotifications();
    initializeTimezoneChangeNotifications();
    initializeCalendarResyncNotification();
    initializeShutdownNotifications();
    initializeRootDomainInterestNotifications();

#if !TARGET_OS_IPHONE
    initializeUserNotifications();
    _oneOffHacksSetup();
#endif

    PMConnection_prime();
    initializeSleepWakeNotifications();

    // Prime the messagetracer UUID pump
    pushNewSleepWakeUUID();

    BatteryTimeRemaining_prime();
    PMSettings_prime();
    AutoWake_prime();
    PMAssertions_prime();
    PMSystemEvents_prime();
    SystemLoad_prime();

#if !TARGET_OS_IPHONE
    UPSLowPower_prime();
    TTYKeepAwake_prime();
    ExternalMedia_prime();

    createOnBootAssertions();
    enableSleepWakeWdog();
    ads_prime();
    standbyTimer_prime();
#endif

    _unclamp_silent_running(false);
    notify_post(kIOUserAssertionReSync);
    logASLMessagePMStart();

#if TARGET_OS_IPHONE
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED,0);
    pthread_set_fixedpriority_self();
    initializeAggdDailyReport();

#endif

    BatteryTimeRemaining_finish();
}

int main(int argc __unused, char *argv[] __unused)
{
    /*
     * This dispatch_sync() ensures that no event can be received concurrently
     * to initializing services, and allow for launch at priority
     * to function properly by checking in on the main thread.
     */
    dispatch_sync_f(_getPMMainQueue(), NULL, powerd_init);
    dispatch_main();
    return 0;
}




__private_extern__ void
ClockSleepWakeNotification(IOPMSystemPowerStateCapabilities old_cap,
                           IOPMSystemPowerStateCapabilities new_cap,
                           uint32_t changeFlags)
{
    // Act on the notification before the dark wake to sleep transition
    if (CAPABILITY_BIT_CHANGED(new_cap, old_cap, kIOPMSystemPowerStateCapabilityCPU) &&
        BIT_IS_SET(old_cap, kIOPMSystemPowerStateCapabilityCPU) &&
        BIT_IS_SET(changeFlags, kIOPMSystemCapabilityWillChange))
    {

        // Stash the last sleep time to ignore clock changes before
        // this sleep is completed.
        size_t len = sizeof(gLastSleepTime);
        if (sysctlbyname("kern.sleeptime", &gLastSleepTime, &len, NULL, 0)) {
            gLastSleepTime.tv_sec = 0;
            gLastSleepTime.tv_usec = 0;
        }

        // The next clock resync occuring on wake from sleep shall be marked
        // as the wake time.
        gExpectingWakeFromSleepClockResync = true;

        // tell clients what our timezone offset is
        broadcastGMTOffset();
    }
}

#if !TARGET_OS_IPHONE

static char* const spindump_args[] =
{ "/usr/sbin/spindump", "kextd","30", "400", "-sampleWithoutTarget",
   "-file", kSpindumpIOKitDir"/iokitwaitquiet-spindump.txt", 0 };

static void takeSpindump()
{

    static int debug_arg = -1;
    int rc;
    static int spindump_pid = -1;
    const char * dir;
    struct stat sb;

    // Take a spindump if:
    // "debug" value is set in nvram boot-args parameter or this is a seed build
    // and kIOPMDebugEnableSpindumpOnFullwake is set in gDebugFlags

    if ((gDebugFlags & kIOPMDebugEnableSpindumpOnFullwake) == 0) {
        // Flag to disable this mechainsm, if required
        return;
    }

    if (debug_arg == -1) {
#if RC_SEED_BUILD
        debug_arg = 1;
#else
        char boot_args[256];
        boot_args[0] = 0;
        debug_arg = 0;

        if ((getNvramArgStr("boot-args", boot_args, sizeof(boot_args)) == kIOReturnSuccess) &&
            (strnstr(boot_args, "debug=", sizeof(boot_args)) != NULL)) {
            debug_arg = 1;
        }
#endif
    }

    if (debug_arg <= 0) {
        return;
    }

    if (spindump_pid != -1) {
        /* If the previously spawned process is alive, kill it */
        struct proc_bsdinfo pbsd;
        rc = proc_pidinfo(spindump_pid, PROC_PIDTBSDINFO, (uint64_t)0, &pbsd, sizeof(struct proc_bsdinfo));
        if ((rc == sizeof(pbsd)) && (pbsd.pbi_ppid == getpid())) {
            kill(spindump_pid, SIGKILL);
            // Don't start another spindump if one is running
            return;
        }
    }

    dir  = kSpindumpIOKitDir;
    rc = stat(dir, &sb);
    if ((rc == -1) && (errno == ENOENT)) {
        rc = mkdir(dir, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    }
    if (rc != 0) {
        return;
    }

    spindump_pid = pluginExecCommand("/usr/sbin/spindump", spindump_args, NULL, NULL);
}
#endif

void sendSleepNotificationResponse(void *acknowledgementToken, bool allow)
{
    if (allow) {
        IOAllowPowerChange(_pm_ack_port, (long)acknowledgementToken);
    }
    else {
        os_log_debug(OS_LOG_DEFAULT, "Cancelling sleep due to async assertions\n");
        IOCancelPowerChange(_pm_ack_port, (long)acknowledgementToken);
    }

}

void handleSoftwareSleep()
{
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    if (lastSleepWakeMsg == kIOMessageCanSystemSleep) {
        DEBUG_LOG("This is an idle sleep. Doing nothing");
        return;
    }

    CFStringRef sleepReason = _getSleepReason();
    if (!isDisplayAsleep()) {
        // turn off display if sleep reason is Software Sleep
        if (CFEqual(sleepReason, CFSTR(kIOPMSoftwareSleepKey))) {
            INFO_LOG("Turning off display for Software Sleep");
            blankDisplay();
        } else if (CFEqual(sleepReason, CFSTR(kIOPMClamshellSleepKey))) {
            INFO_LOG("Turning off display for Clamshell Sleep");
            blankDisplay();
        } else if (IS_EMERGENCY_SLEEP(sleepReason)) {
            INFO_LOG("Turning off display for emergency sleep");
            blankDisplay();
        }
    }
#endif
}

/*
 *
 * Receives notifications on system sleep and system wake.
 * This callback is not called for maintenance sleep/wake.
 */
static void
SleepWakeCallback(
    void            *port,
    io_service_t    rootdomainservice,
    natural_t       messageType,
    void            *acknowledgementToken)
{
    CFStringRef uuid = IOPMSleepWakeCopyUUID();

    // Log Message to MessageTracer

    // Acknowledge message
    switch ( messageType ) {
        case kIOMessageSystemWillSleep:
            INFO_LOG("Received kIOMessageSystemWillSleep. UUID: %{public}@\n", uuid);
            if (userActiveRootDomain()) {
                userActiveHandleSleep();
            }
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
            // disable clamshell sleep
            INFO_LOG("Disable clamshell sleep on kIOMessageSystemWillSleep");
            disableClamshellSleepState();
#endif
            IOAllowPowerChange(_pm_ack_port, (long)acknowledgementToken);
            handleSoftwareSleep();
            break;

        case kIOMessageCanSystemSleep:
            INFO_LOG("Received kIOMessageCanSystemSleep. UUID: %{public}@\n", uuid);
            checkForAsyncAssertions(acknowledgementToken);
            // Response is sent thru sendSleepNotificationResponse()
            break;

        case kIOMessageSystemWillPowerOn:
#if !TARGET_OS_IPHONE
            setVMDarkwakeMode(false);
            /* fallthrough */
#endif
        case kIOMessageSystemWillNotSleep:
            INFO_LOG("Received %{public}s. UUID: %{public}@\n",
                    (messageType == kIOMessageSystemWillPowerOn) ? "kIOMessageSystemWillPowerOn" :
                        "kIOMessageSystemWillNotSleep", uuid);
            _set_sleep_revert(true);
            checkPendingWakeReqs(ALLOW_PURGING);
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kClamshellEvaluateDelay * NSEC_PER_SEC), _getPMMainQueue(), ^{
                INFO_LOG("EvaluateClamshellSleepState after wake");
                evaluateClamshellSleepState();
            });
#endif
            break;

        case kIOMessageSystemHasPoweredOn:
            INFO_LOG("Received kIOMessageSystemHasPoweredOn. UUID: %{public}@\n", uuid);
            break;

        default:
            break;
    }

    if (uuid) {
        CFRelease(uuid);
    }
    lastSleepWakeMsg = messageType;
}

/* ESPrefsHaveChanged
 *
 * Is the handler that configd calls when someone "applies" new Energy Saver
 * Preferences. Since the preferences have probably changed, we re-read them
 * from disk and transmit the new settings to the kernel.
 */
static void
ESPrefsHaveChanged(void)
{
    // Tell ES Prefs listeners that the prefs have changed
    PMSettingsPrefsHaveChanged();
    mt2EvaluateSystemSupport();
#if !TARGET_OS_IPHONE
    UPSLowPowerPrefsHaveChanged();
    TTYKeepAwakePrefsHaveChanged();
#if !(TARGET_OS_OSX && TARGET_CPU_ARM64)
    evalProximityPrefsChange();
#endif
#endif
    SystemLoadPrefsHaveChanged();

    return;
}

/* timeZoneChangedCallback
 *
 * When our timezone offset changes, tell interested drivers.
 */
static void
timeZoneChangedCallBack(
    CFNotificationCenterRef center,
    void *observer,
    CFStringRef notificationName,
    const void *object,
    CFDictionaryRef userInfo)
{
    if( CFEqual(notificationName, gTZNotificationNameString) )
    {
        broadcastGMTOffset();
    }
}


/* broadcastGMTOffset
 *
 * Tell the timezone clients what the seconds offset from GMT is. This info
 * is delivered via the kernel PMSettings interface.
 *
 * Notifications are sent:
 *    - at boot time
 *    - when timezone changes
 *    - at sleep time*
 *    - at display sleep time*
 *
 *    * PM configd does not receive a notification when daylight savings time
 *      changes. In case the system has entered daylight savings time since
 *      boot, we re-broadcast the tz offset at sleep and display sleep.
 */
static void
broadcastGMTOffset(void)
{
    CFTimeZoneRef               tzr = NULL;
    CFNumberRef                 n = NULL;
    int                         secondsOffset = 0;

    CFTimeZoneResetSystem();
    tzr = CFTimeZoneCopySystem();
    if(!tzr) {
        goto exit;
    }

    secondsOffset = (int)CFTimeZoneGetSecondsFromGMT(tzr, CFAbsoluteTimeGetCurrent());
    n = CFNumberCreate(0, kCFNumberIntType, &secondsOffset);
    if(!n) {
        goto exit;
    }

    // Tell the root domain what our timezone's offset from GMT is.
    // IOPMrootdomain will relay the message on to interested PMSetting clients.
    _setRootDomainProperty(CFSTR("TimeZoneOffsetSeconds"), n);

exit:
    if(tzr) CFRelease(tzr);
    if(n) CFRelease(n);
    return;
}

/* lwShutdownCallback
 *
 *
 *
 * loginwindow shutdown handler
 *
 */
static void lwShutdownCallback(mach_msg_header_t *header)
{
    CFNumberRef n = NULL;
    static bool amidst_shutdown = false;
    static int consoleShutdownState = kIOPMStateConsoleShutdownNone;
    static int lastConsoleShutdownState = 0;

    if (header->msgh_id == gCPUPowerNotificationToken)
    {
        // System CPU power status has changed
        SystemLoadCPUPowerHasChanged(NULL);
    } else if (header->msgh_id == lwNotify.su)
    {
        // Loginwindow is logging out to begin a several-minute
        // software update. We'll suppress the immediately next shoutdown
        // and logout messages.
        consoleShutdownState = kIOPMStateConsoleSULogoutInitiated;

    } else if (header->msgh_id == lwNotify.shutdown)
    {
        // Loginwindow put a shutdown confirm panel up on screen
        // The user has not necessarily even clicked on it yet
        amidst_shutdown = true;
        consoleShutdownState = kIOPMStateConsoleShutdownPossible;

    } else if (header->msgh_id == lwNotify.restart)
    {
        // Loginwindow put a restart confirm panel up on screen
        // The user has not necessarily even clicked on it yet
        amidst_shutdown = true;
        consoleShutdownState = kIOPMStateConsoleShutdownPossible;

    } else if (header->msgh_id == lwNotify.cancel)
    {
        amidst_shutdown = false;
        consoleShutdownState = kIOPMStateConsoleShutdownNone;

    } else if (amidst_shutdown
            && (header->msgh_id == lwNotify.pointofnoreturn))
    {
        // Whatever shutdown or restart that was in progress has succeeded.
        // All apps are quit, there's no more user input required. We will
        // hereby disable sleep for the remainder of time spent shutting down
        // this machine.

        _setRootDomainProperty(CFSTR("System Shutdown"), kCFBooleanTrue);
        consoleShutdownState = kIOPMStateConsoleShutdownCertain;
    }

    // Tell interested kernel drivers where we are in the GUI shutdown.
    if (lastConsoleShutdownState != consoleShutdownState) {

        n = CFNumberCreate(0, kCFNumberIntType, &consoleShutdownState);
        if (n) {
            _setRootDomainProperty( CFSTR(kIOPMStateConsoleShutdown), n) ;
            CFRelease(n);
        }

        lastConsoleShutdownState = consoleShutdownState;
    }


    return;
}

static void
lwShutdownHandler(void *context, dispatch_mach_reason_t reason,
                  dispatch_mach_msg_t msg, mach_error_t err)
{
    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED) {
        mach_msg_header_t *hdr = dispatch_mach_msg_get_msg(msg, NULL);
        lwShutdownCallback(hdr);
        mach_msg_destroy(hdr);
    }
}


#if !TARGET_OS_IPHONE && !(TARGET_OS_OSX && TARGET_CPU_ARM64)
/* displayPowerStateChange
 *
 * displayPowerStateChange gets notified when the display changes power state.
 * Power state changes look like this:
 * (1) Full power -> dim
 * (2) dim -> display sleep
 * (3) display sleep -> display sleep
 *
 * We're interested in state transition 2. On transition to state 2 we
 * broadcast the system clock's offset from GMT.
 */
static void
displayPowerStateChange(void *ref, io_service_t service, natural_t messageType, void *arg)
{
    IOPowerStateChangeNotification *params = (IOPowerStateChangeNotification*) arg;
    bool prevState = gDisplayIsAsleep;

    switch (messageType) {
            // Display Wrangler power stateNumber values
            // 4 Display ON
            // 3 Display Dim
            // 2 Display Sleep
            // 1 Not visible to user
            // 0 Not visible to user

        case kIOMessageDeviceWillPowerOff:
            if ( params->stateNumber != kWranglerPowerStateMax )
            {
                gDisplayIsAsleep = true;
            }

            if ( params->stateNumber <= kWranglerPowerStateSleep)
            {
               // Notify a SystemLoad state change when display is completely off
                SystemLoadDisplayPowerStateHasChanged(gDisplayIsAsleep);
                // Display is transition from dim to full sleep.
                broadcastGMTOffset();
            }
            break;

        case kIOMessageDeviceHasPoweredOn:
            if ( params->stateNumber == kWranglerPowerStateMax )
            {
                gDisplayIsAsleep = false;
                SystemLoadDisplayPowerStateHasChanged(gDisplayIsAsleep);
            }

            break;
    }

    if (prevState != gDisplayIsAsleep) {
        logASLDisplayStateChange();
    }
}
#endif

__private_extern__ bool isDisplayAsleep(void)
{
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    return !(skylightDisplayOn());
#else
    return gDisplayIsAsleep;
#endif
}

/* initializeESPrefsNotification
 *
 * Registers a handler that configd calls when someone changes com.apple.PowerManagement.xml
 */
static void
initializeESPrefsNotification(void)
{
    gESPreferences = IOPMRegisterPrefsChangeNotification(_getPMMainQueue(),
                                                         ^(void) {
                                                             ESPrefsHaveChanged();
                                                         });

    return;
}


/* pushNewSleepWakeUUID
 *
 * Called (1) At boot, and (2) When kernel PM uses its current UUID.
 * We pre-allocate the UUID here and send it to the kernel, but the kernel
 * will not activate it until the _next_ sleep/wake session begins.
 *
 * Global gCachedNextSleepWakeUUIDString will always reflect the next
 * upcoming sleep/wake session UUID; not the current UUID.
 */
static void pushNewSleepWakeUUID(void)
{
    uuid_t              new_uuid;
    uuid_string_t       new_uuid_string;

    io_registry_entry_t root_domain = getRootDomain();

    if (IO_OBJECT_NULL == root_domain) {
        return;
    }

    uuid_generate(new_uuid);
    uuid_unparse_upper(new_uuid, new_uuid_string);

    if (gCachedNextSleepWakeUUIDString)
    {
        CFRelease(gCachedNextSleepWakeUUIDString);

        gCachedNextSleepWakeUUIDString = NULL;
    }

    if ((gCachedNextSleepWakeUUIDString = CFStringCreateWithCString(0, new_uuid_string, kCFStringEncodingUTF8)))
    {
        IORegistryEntrySetCFProperty(root_domain, CFSTR(kIOPMSleepWakeUUIDKey), gCachedNextSleepWakeUUIDString);
    }
    return;
}

static void handle_xpc_error(xpc_connection_t peer, xpc_object_t error)
{
    const char *errStr = xpc_dictionary_get_string(error, XPC_ERROR_KEY_DESCRIPTION);
    os_log_debug(OS_LOG_DEFAULT, "Received error \"%s\" on peer %p(pid %d)\n",
            errStr, peer, xpc_connection_get_pid(peer));

    deRegisterUserActivityClient(peer);
    releaseConnectionAssertions(peer);
}
static void incoming_XPC_connection(xpc_connection_t peer)
{
    xpc_connection_set_target_queue(peer, _getPMMainQueue());

    xpc_connection_set_event_handler(peer,
             ^(xpc_object_t event) {


                 if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {

                     xpc_object_t inEvent;

                     if((inEvent = xpc_dictionary_get_value(event, kUserActivityRegister))) {
                        registerUserActivityClient(peer, inEvent);
                     }
                     else if((inEvent = xpc_dictionary_get_value(event, kUserActivityTimeoutUpdate))) {
                        updateUserActivityTimeout(peer, inEvent);
                     }
                     else if ((inEvent = xpc_dictionary_get_value(event, kClaimSystemWakeEvent))) {
                        appClaimWakeReason(peer, inEvent);
                     }
                     else if (xpc_dictionary_get_value(event, kAssertionCreateMsg)) {
                        asyncAssertionCreate(peer, event);
                     }
                     else if (xpc_dictionary_get_value(event, kAssertionReleaseMsg)) {
                        asyncAssertionRelease(peer, event);
                     }
                     else if (xpc_dictionary_get_value(event, kAssertionPropertiesMsg)) {
                        asyncAssertionProperties(peer, event);
                     }
                     else if (xpc_dictionary_get_value(event, kPSAdapterDetails)) {
                         sendAdapterDetails(peer, event);
                     }
#if TARGET_OS_OSX
                     else if (xpc_dictionary_get_value(event, "readBatteryHealthPersistentData")) {
                         getBatteryHealthPersistentData(peer, event);
                     }
#endif  // TARGET_OS_OSX
                     else if (xpc_dictionary_get_value(event, kCustomBatteryProps)) {
                         setCustomBatteryProps(peer, event);
                     }
                     else if (xpc_dictionary_get_value(event, kResetCustomBatteryProps)) {
                         resetCustomBatteryProps(peer, event);
                     }
                     else if (xpc_dictionary_get_value(event, kAssertionSetStateMsg)) {
                         processSetAssertionState(peer, event);
                     }
                     else if (xpc_dictionary_get_value(event, kIOPMPowerEventDataKey)) {
                        getScheduledWake(peer, event);
                     }
#if TARGET_OS_IPHONE && !TARGET_OS_BRIDGE
                     else if (xpc_dictionary_get_value(event, kBatteryHeatMapData)) {
                         sendHeatMapData(peer, event);
                     }
                     else if (xpc_dictionary_get_value(event, kBatteryCycleCountData)) {
                         sendCycleCountData(peer, event);
                     }
#endif // TARGET_OS_IPHONE && !TARGET_OS_BRIDGE
#if TARGET_OS_IOS || TARGET_OS_WATCH || TARGET_OS_OSX
                     else if (xpc_dictionary_get_value(event, kSetBHUpdateTimeDelta)) {
                         setBHUpdateTimeDelta(peer, event);
                     }
#endif // TARGET_OS_IOS || TARGET_OS_WATCH || TARGET_OS_OSX
#if !TARGET_OS_IPHONE
                     else if (xpc_dictionary_get_value(event, kInactivityWindowKey)) {
                         setInactivityWindow(peer, event);
                     }
#endif // !TARGET_OS_IPHONE
#if TARGET_OS_IOS || TARGET_OS_WATCH
                     else if (xpc_dictionary_get_value(event, kBatteryKioskModeData)) {
                         sendKioskModeData(peer, event);
                     }
#endif // TARGET_OS_IOS || TARGET_OS_WATCH
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
                     else if ((inEvent = xpc_dictionary_get_value(event, kSkylightCheckInKey))) {
                         skylightCheckIn(peer, event);
                     }
                     else if ((inEvent = xpc_dictionary_get_value(event, kDesktopModeKey))) {
                         updateDesktopMode(peer, event);
                     }
#endif
                     else {
                        os_log_error(OS_LOG_DEFAULT, "Unexpected xpc dictionary\n");
                     }
                 }
                 else if (xpc_get_type(event) == XPC_TYPE_ERROR) {
                    handle_xpc_error(peer, event);
                 }
                 else {
                    os_log_error(OS_LOG_DEFAULT, "Unexpected xpc type\n");
                 }


             });

    xpc_connection_resume(peer);
    return;
}

static void xpc_register(void)
{
    xpc_connection_t        connection;

    connection = xpc_connection_create_mach_service(
                                                    "com.apple.iokit.powerdxpc",
                                                    _getPMMainQueue(),
                                                    XPC_CONNECTION_MACH_SERVICE_LISTENER);

    xpc_connection_set_target_queue(connection, _getPMMainQueue());

    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
            xpc_type_t type = xpc_get_type(event);
            if (type == XPC_TYPE_CONNECTION) {
                incoming_XPC_connection((xpc_connection_t)event);
            }
        });

    xpc_connection_resume(connection);
}


/* dynamicStoreNotifyCallBack
 *
 * Changed Keys in dynamic store
 *
 */
__private_extern__
void dynamicStoreNotifyCallBack(
    SCDynamicStoreRef   store,
    CFArrayRef          changedKeys,
    void                *info)
{
    CFRange range = CFRangeMake(0,
                CFArrayGetCount(changedKeys));

    // Check for Console user change
    if (gConsoleNotifyKey
        && CFArrayContainsValue(changedKeys,
                                range,
                                gConsoleNotifyKey))
    {
#if !TARGET_OS_IPHONE
        SystemLoadUserStateHasChanged();
#endif
    }

    return;
}

kern_return_t _io_pm_set_value_int(
    mach_port_t   server,
    audit_token_t token,
    int           selector,
    int           inValue,
    int           *result)
{
    uid_t   callerUID;
    pid_t   callerPID;
    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, 0, &callerPID, NULL, NULL);

    *result = kIOReturnSuccess;
    switch (selector) {
    case kIOPMSetAssertionActivityLog:
        setAssertionActivityLog(inValue);
        break;

    case kIOPMSetAssertionActivityAggregate:
        setAssertionActivityAggregate(callerPID, inValue);
        break;

    case kIOPMSetReservePowerMode:
        if (!auditTokenHasEntitlement(token, kIOPMReservePwrCtrlEntitlement))
            *result = kIOReturnNotPrivileged;
        else
            *result = setReservePwrMode(inValue);
        break;

#if !TARGET_OS_IPHONE
    case kIOPMPushConnectionActive:
        setPushConnectionState(inValue ? true:false);
        break;

    case kIOPMTCPKeepAliveExpirationOverride:
        setTCPKeepAliveOverrideSec(inValue);
        break;
#endif

    default:
        break;
    }
    return KERN_SUCCESS;
}


kern_return_t _io_pm_get_value_int(
    mach_port_t   server,
    audit_token_t token,
    int           selector,
    int           *outValue)
{
    uid_t   callerUID;
    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, 0, 0, NULL, NULL);

    *outValue = 0;

    switch(selector)
    {
#if !TARGET_OS_IPHONE
      case kIOPMGetSilentRunningInfo:
         if ( smcSilentRunningSupport( ))
            *outValue = 1;
         else
             *outValue = 0;
         break;

     case kIOPMMT2Bookmark:
            if (0 == callerUID)
            {
                mt2PublishReports();
                *outValue = 0;
            } else {
                *outValue = 1;
            }
          break;
    case kIOPMDarkWakeThermalEventCount:
            *outValue = _darkWakeThermalEventCount;
        break;
    case kIOPMTCPKeepAliveExpirationOverride:
            *outValue = (int)getTCPKeepAliveOverrideSec();
        break;

    case kIOPMTCPKeepAliveIsActive:
            *outValue = (getTCPKeepAliveState(NULL, 0) == kActive) ? true : false;
            break;
    case kIOPMWakeOnLanIsActive:
            *outValue = getWakeOnLanState( );
            break;

    case kIOPMPushConnectionActive:
        *outValue = getPushConnectionState();
        break;
#endif

      default:
         *outValue = 0;
         break;

    }
    return KERN_SUCCESS;
}



kern_return_t _io_pm_force_active_settings(
    mach_port_t                 server,
    audit_token_t               token,
    vm_offset_t                 settings_ptr,
    mach_msg_type_number_t      settings_len,
    int                         *result)
{
    void                    *settings_buf = (void *)settings_ptr;
    CFDictionaryRef         force_settings = NULL;
    uid_t                   callerUID;

    audit_token_to_au32(token, NULL, &callerUID, NULL, NULL, NULL, NULL, NULL, NULL);

    if (0 != callerUID) {
        // Caller must be root
        *result = kIOReturnNotPrivileged;
    } else {
        force_settings = (CFDictionaryRef)IOCFUnserialize(settings_buf, 0, 0, 0);

        if(isA_CFDictionary(force_settings))
        {
            *result = _activateForcedSettings(force_settings);
        } else {
            *result = kIOReturnBadArgument;
        }

        if(force_settings)
        {
            CFRelease(force_settings);
        }
    }

    // deallocate client's memory
    vm_deallocate(mach_task_self(), (vm_address_t)settings_ptr, settings_len);

    return KERN_SUCCESS;
}

/* initializeInteresteNotifications
 *
 * Sets up the notification of general interest from the RootDomain
 */
static void
initializeInterestNotifications()
{
    IONotificationPortRef       notify_port = 0;
    io_iterator_t               battery_iter = 0;
#if !TARGET_OS_IPHONE && !(TARGET_OS_OSX && TARGET_CPU_ARM64)
    io_iterator_t               display_iter = 0;
#endif

    kern_return_t               kr;

    /* Notifier */
    notify_port = IONotificationPortCreate(0);


#if !TARGET_OS_IPHONE  && !(TARGET_OS_OSX && TARGET_CPU_ARM64)
    kr = IOServiceAddMatchingNotification(
                                notify_port,
                                kIOFirstMatchNotification,
                                IOServiceMatching("IODisplayWrangler"),
                                displayMatched,
                                (void *)notify_port,
                                &display_iter);
    if(KERN_SUCCESS == kr)
    {
        // Install notifications on existing instances.
        displayMatched((void *)notify_port, display_iter);
    }
    else {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "Failed to match DisplayWrangler(0x%x)\n", kr);
    }
#elif !TARGET_OS_OSX
    int token;
    uint64_t displayState;
    notify_register_dispatch( kIOHIDEventSystemDisplayStatusNotifyKey,
                              &token, _getPMMainQueue(),
                              ^(int token) {
                                    uint64_t displayState;
                                    notify_get_state(token, &displayState);
                                    gDisplayIsAsleep = displayState ? 0 : 1;
                                    SystemLoadDisplayPowerStateHasChanged(gDisplayIsAsleep);
                                    logASLDisplayStateChange();
                              });
    // Set initial state
    notify_get_state(token, &displayState);
    gDisplayIsAsleep = displayState ? 0 : 1;


#endif

    IONotificationPortSetDispatchQueue(notify_port, _getPMMainQueue());
}

/* initializeTimezoneChangeNotifications
 *
 * Sets up the tz notifications that we re-broadcast to all interested
 * kernel clients listening via PMSettings
 */
static void
initializeTimezoneChangeNotifications(void)
{
    CFNotificationCenterRef         distNoteCenter = NULL;

    gTZNotificationNameString = CFStringCreateWithCString(
                        kCFAllocatorDefault,
                        "NSSystemTimeZoneDidChangeDistributedNotification",
                        kCFStringEncodingMacRoman);

#if TARGET_OS_IPHONE
    distNoteCenter = CFNotificationCenterGetDarwinNotifyCenter();
#else
    distNoteCenter = CFNotificationCenterGetDistributedCenter();
#endif
    if(distNoteCenter)
    {
        CFNotificationCenterAddObserver(
                       distNoteCenter,
                       NULL,
                       timeZoneChangedCallBack,
                       gTZNotificationNameString,
                       NULL,
                       CFNotificationSuspensionBehaviorDeliverImmediately);
    }

    // Boot time - tell clients what our timezone offset is
    broadcastGMTOffset();
}

static void initializeCalendarResyncNotification(void)
{
    static dispatch_mach_t      calendarResyncChannel = NULL;
    mach_port_t                 tport;
    kern_return_t               result;

    result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &tport);
    if (result != KERN_SUCCESS) {
        goto exit;
    }
    result = host_request_notification(mach_host_self(), HOST_NOTIFY_CALENDAR_CHANGE, tport);
    if (result != KERN_SUCCESS) {
        goto exit;
    }

    calendarResyncChannel = dispatch_mach_create_f("PowerManagement/calendarResync", _getPMMainQueue(), NULL, calendarRTCDidResyncHandler);
    dispatch_mach_connect(calendarResyncChannel, tport, MACH_PORT_NULL, NULL);

exit:
    return;
}

static bool calendarRTCDidResync_getSMCWakeInterval(void)
{
    CFAbsoluteTime      lastWakeTime;
    struct timeval      lastSleepTime;
    size_t              len = sizeof(struct timeval);

    // Capture this as early as possible
    lastWakeTime = CFAbsoluteTimeGetCurrent();

    if (!gExpectingWakeFromSleepClockResync) {
        // This is a non-wake-from-sleep clock resync, so we'll ignore it.
        return false;
    }

    if (sysctlbyname("kern.sleeptime", &lastSleepTime, &len, NULL, 0) ||
        ((gLastSleepTime.tv_sec  == lastSleepTime.tv_sec) &&
         (gLastSleepTime.tv_usec == lastSleepTime.tv_usec)))
    {
        // This is a clock resync after sleep has started but before
        // platform sleep.
        return false;
    }

    // if needed, init standalone memory for last wake time data
    if (!gLastSMCS3S0WakeInterval) {
        size_t bufSize;

        bufSize = sizeof(*gLastWakeTime) + sizeof(*gLastSMCS3S0WakeInterval);
        if (0 != vm_allocate(mach_task_self(), (void*)&gLastWakeTime,
                             bufSize, VM_FLAGS_ANYWHERE)) {
            goto exit;
        }
        gLastSMCS3S0WakeInterval = gLastWakeTime + 1;
    } else {
        // validate pointers allocated earlier
        if (!gLastWakeTime || !gLastSMCS3S0WakeInterval)
            goto exit;
    }

    // This is a wake-from-sleep resync, so commit the last wake time
    *gLastWakeTime = lastWakeTime;
    *gLastSMCS3S0WakeInterval = 0;
    gExpectingWakeFromSleepClockResync = false;

exit:
    return true;
}

static void
calendarRTCDidResyncHandler(void *context, dispatch_mach_reason_t reason,
                            dispatch_mach_msg_t msg, mach_error_t error)
{
    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED) {
        mach_msg_header_t   *header = dispatch_mach_msg_get_msg(msg, NULL);

        if (HOST_CALENDAR_CHANGED_REPLYID == header->msgh_id) {
            // renew our request for calendar change notification
            (void) host_request_notification(mach_host_self(), HOST_NOTIFY_CALENDAR_CHANGE,
                                             header->msgh_local_port);

            bool did_update = calendarRTCDidResync_getSMCWakeInterval();
            AutoWakeCalendarChange();

            // calendar has resync. Safe to get timestamps now
            if (did_update) {
                CFAbsoluteTime wakeStart;
                CFTimeInterval smcAdjustment;
                IOReturn rc = IOPMGetLastWakeTime(&wakeStart, &smcAdjustment);
                if (rc == kIOReturnSuccess) {
                    uint64_t wakeStartTimestamp = CFAbsoluteTimeToMachAbsoluteTime(wakeStart);
                    DEBUG_LOG("WakeTime: Calendar resynced\n");
                    updateCurrentWakeStart(wakeStartTimestamp);
                    updateWakeTime();
                }
            }
        }
        mach_msg_destroy(header);
    }
}

/* MIG CALL
 * Returns last wake time to a querulous process
 */
kern_return_t _io_pm_last_wake_time(
    mach_port_t             server,
    vm_offset_t             *out_wake_data,
    mach_msg_type_number_t  *out_wake_len,
    vm_offset_t             *out_delta_data,
    mach_msg_type_number_t  *out_delta_len,
    int                     *return_val)
{
    *out_wake_data = 0;
    *out_wake_len = 0;
    *out_delta_data = 0;
    *out_delta_len = 0;
    *return_val = kIOReturnInvalid;

    if (gExpectingWakeFromSleepClockResync) {
        *return_val = kIOReturnNotReady;
        return KERN_SUCCESS;
    }

#if !TARGET_OS_IPHONE
    if (!gSMCSupportsWakeupTimer) {
        *return_val = kIOReturnNotFound;
        return KERN_SUCCESS;
    };
#endif

    *out_wake_data = (vm_offset_t)gLastWakeTime;
    *out_wake_len = sizeof(*gLastWakeTime);
    *out_delta_data = (vm_offset_t)gLastSMCS3S0WakeInterval;
    *out_delta_len = sizeof(*gLastSMCS3S0WakeInterval);

    *return_val = kIOReturnSuccess;

    return KERN_SUCCESS;
}


#if !TARGET_OS_IPHONE && !(TARGET_OS_OSX && TARGET_CPU_ARM64)
/* displayMatched
 *
 * Notification fires when IODisplayWranger object is created in the IORegistry.
 *
 */
static void displayMatched(
    void *note_port_in,
    io_iterator_t iter)
{
    IONotificationPortRef       note_port = (IONotificationPortRef)note_port_in;
    io_service_t                wrangler = MACH_PORT_NULL;
    io_object_t                 dimming_notification_object = MACH_PORT_NULL;
    CFDictionaryRef             pmState = NULL;

    if((wrangler = (io_registry_entry_t)IOIteratorNext(iter)))
    {
        IOServiceAddInterestNotification(
                    note_port,
                    wrangler,
                    kIOGeneralInterest,
                    displayPowerStateChange,
                    NULL,
                    &dimming_notification_object);

        // Get initial display state
        pmState = IORegistryEntryCreateCFProperty(
                                                  wrangler,
                                                  CFSTR("IOPowerManagement"),
                                                  kCFAllocatorDefault,
                                                  kNilOptions);
        if (isA_CFDictionary(pmState)) {
            CFNumberRef cfvalue = CFDictionaryGetValue(pmState, CFSTR("CurrentPowerState"));
            int value = -1;
            if (isA_CFNumber(cfvalue)) {
                CFNumberGetValue(cfvalue, kCFNumberIntType, &value);
            }
            if (value != -1) {
                if (value == kWranglerPowerStateMax) {
                    gDisplayIsAsleep = false;
                }
                else {
                    gDisplayIsAsleep = true;
                }
            }

        }
        if (pmState) {
            CFRelease(pmState);
        }


        IOObjectRelease(wrangler);
    }

}

#endif

static void
initializeShutdownNotifications(void)
{
    static dispatch_mach_t shutdownNotifChannel = NULL;
    mach_port_t         our_port = MACH_PORT_NULL;

    // Tell the kernel that we are NOT shutting down at the moment, since
    // configd is just launching now.
    // Why: if configd crashed with "System Shutdown" == kCFbooleanTrue, reset
    // it now as the situation may no longer apply.
    _setRootDomainProperty(CFSTR("System Shutdown"), kCFBooleanFalse);

    /* * * * * * * * * * * * * */

    // Sneak in our registration for CPU power notifications here; to piggy-back
    // with the other mach port registrations for LW.
    notify_register_mach_port(
                        kIOPMCPUPowerNotificationKey,
                        &our_port,
                        0, /* flags */
                        &gCPUPowerNotificationToken);


    notify_register_mach_port(
                        kLWShutdowntInitiated,
                        &our_port,
                        NOTIFY_REUSE, /* flags */
                        &lwNotify.shutdown);

    notify_register_mach_port(
                        kLWRestartInitiated,
                        &our_port,
                        NOTIFY_REUSE, /* flags */
                        &lwNotify.restart);

    notify_register_mach_port(
                        kLWLogoutCancelled,
                        &our_port,
                        NOTIFY_REUSE, /* flags */
                        &lwNotify.cancel);

    notify_register_mach_port(
                        kLWLogoutPointOfNoReturn,
                        &our_port,
                        NOTIFY_REUSE, /* flags */
                        &lwNotify.pointofnoreturn);
    notify_register_mach_port(
                        kLWSULogoutInitiated,
                        &our_port,
                        NOTIFY_REUSE, /* flags */
                        &lwNotify.su);

    /* * * * * * * * * * * * * */

    shutdownNotifChannel = dispatch_mach_create_f("PowerManagement/shutdown",
                                                  _getPMMainQueue(), NULL,
                                                  lwShutdownHandler);
    dispatch_mach_connect(shutdownNotifChannel, our_port, MACH_PORT_NULL, NULL);
}

#if !TARGET_OS_IPHONE
static void handleDWThermalMsg(CFStringRef wakeType)
{
    CFMutableDictionaryRef options = NULL;

    if (wakeType == NULL)
        getPlatformWakeReason(NULL, &wakeType);

    INFO_LOG("DarkWake Thermal Emergency message is received. BTWake: %d ssWake:%d ProxWake:%d NotificationWake:%d\n",
            isA_BTMtnceWake(), isA_SleepSrvcWake(), checkForAppWakeReason(CFSTR(kProximityWakeReason)), isA_NotificationDisplayWake());
#if !(TARGET_OS_OSX && TARGET_CPU_ARM64)
    gateProximityDarkWakeState(kPMAllowSleep);
#endif
    if ( (isA_BTMtnceWake() || isA_SleepSrvcWake() || checkForAppWakeReason(CFSTR(kProximityWakeReason))) &&
            (!isA_NotificationDisplayWake()) && (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) ||
                        CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService))
            && !((getTCPKeepAliveState(NULL, 0) == kActive) && checkForActivesByType(kInteractivePushServiceType)) ) {
        // If system woke up for PowerNap and system is in a power nap wake, without any notifications
        // being displayed, then let system go to sleep
        options = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
        if (options) {
            CFDictionarySetValue(options, CFSTR("Sleep Reason"), CFSTR(kIOPMDarkWakeThermalEmergencyKey));
        }
        IOPMSleepSystemWithOptions(getRootDomainConnect(), options);
        if (options) CFRelease(options);
    }
    else {
        // For all other cases, let system run in non-silent running mode
        _unclamp_silent_running(true);

        // Cancel Dark Wake capabilities timer
        cancelDarkWakeCapabilitiesTimer();

        logASLMessageIgnoredDWTEmergency();
    }
}

__private_extern__ void evaluateDWThermalMsg(void)
{
    if (gEvaluateDWThermalEmergency) {
        gEvaluateDWThermalEmergency = false;
        handleDWThermalMsg(NULL);
    }
}
#endif


static void
RootDomainInterest(
    void *refcon,
    io_service_t root_domain,
    natural_t messageType,
    void *messageArgument)
{
    static CFStringRef  _uuidString = NULL;
#if !TARGET_OS_IPHONE
    CFStringRef wakeReason = NULL, wakeType = NULL;
#endif

    if (messageType == kIOPMMessageDriverAssertionsChanged)
    {
        CFNumberRef     driverAssertions = 0;
        uint32_t        _driverAssertions = 0;
        CFArrayRef      kernelAssertionsArray = NULL;

        // Read driver assertion status
        driverAssertions = IORegistryEntryCreateCFProperty(getRootDomain(), CFSTR(kIOPMAssertionsDriverKey), 0, 0);

        if (driverAssertions) {
            CFNumberGetValue(driverAssertions, kCFNumberIntType, &_driverAssertions);
            _PMAssertionsDriverAssertionsHaveChanged(_driverAssertions);
            kernelAssertionsArray = IORegistryEntryCreateCFProperty(getRootDomain(), CFSTR(kIOPMAssertionsDriverDetailedKey), 0, 0);
            if (kernelAssertionsArray) {
                logKernelAssertions(driverAssertions, kernelAssertionsArray);
                CFRelease(kernelAssertionsArray);
            }
            CFRelease(driverAssertions);
        }
    }

    if (messageType == kIOPMMessageSystemPowerEventOccurred)
    {
        // Let System Events know about just-occurred thermal state change

        PMSystemEventsRootDomainInterest();
    }

#if !TARGET_OS_IPHONE
    if(messageType == kIOPMMessageDarkWakeThermalEmergency)
    {
        mt2RecordThermalEvent(kThermalStateSleepRequest);
        _darkWakeThermalEventCount++;

        getPlatformWakeReason(&wakeReason, &wakeType);
        if ((CFEqual(wakeReason, CFSTR("")) && CFEqual(wakeType, CFSTR(""))) || !isCapabilityChangeDone())
        {
            // Thermal emergency msg is received too early before wake type is
            // determined. Delay the handler until capability changes are done
            //
            gEvaluateDWThermalEmergency = true;

        }
        else
        {
            handleDWThermalMsg(wakeType);
            dispatch_async(_getPMMainQueue(), ^{
                logASLAssertionTypeSummary(kInteractivePushServiceType);
            });
        }

    }

    if (messageType == kIOPMMessageLaunchBootSpinDump)
    {
        dispatch_async(_getPMMainQueue(), ^{ takeSpindump(); });
    }
#endif

    if (messageType == kIOPMMessageFeatureChange)
    {
        // Let PMSettings code know that some settings may have been
        // added or removed.

        PMSettingsSupportedPrefsListHasChanged();
    }

    if (messageType == kIOPMMessageSleepWakeUUIDChange)
    {
        if (kIOPMMessageSleepWakeUUIDSet == messageArgument)
        {
            // We keep a copy of the newly published UUID string
            _uuidString = IOPMSleepWakeCopyUUID();

            // xnu kernel PM has just published a sleep/Wake UUID.
            // We must replenish it with a new one (which we generate in user space)
            // Kernel PM will use the UUID we provide here on the next sleep/wake event.
            pushNewSleepWakeUUID();

        } else
        if (kIOPMMessageSleepWakeUUIDCleared == messageArgument)
        {
            // UUID cleared
            if (_uuidString) {
                // We're ready to parse out the newly acquired Power log and
                // package events that pertain to the current UUID
                CFRelease(_uuidString);
                _uuidString = NULL;
            }

            // The kernel will have begun using its cached UUID (the one that we just stored)
            // for its power events log. We need to replenish its (now empty) cache with another
            // UUID, which in turn will get used when the current one expires

            pushNewSleepWakeUUID();

        }
    }

    if (messageType == kIOPMMessageClamshellStateChange)
    {
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
        INFO_LOG("Clamshell state changed\n");
        dispatch_async(_getPMMainQueue(), ^{
            updateClamshellState(messageArgument);
        });
#endif
    }

#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
    if (messageType == kIOPMMessageRequestSystemShutdown)
    {
        INFO_LOG("Received system shutdown request\n");
        dispatch_async(_getPMMainQueue(), ^{
            int ret = reboot3(RB_HALT);
            if (ret) {
                INFO_LOG("Failed to shutdown system: %d (%s)", errno, strerror(errno));
            }
        });
    }
#endif
}

static void
initializeRootDomainInterestNotifications(void)
{
    IONotificationPortRef       note_port = MACH_PORT_NULL;
    io_service_t                root_domain = MACH_PORT_NULL;
    io_object_t                 notification_object = MACH_PORT_NULL;
    IOReturn                    ret;

    root_domain = IORegistryEntryFromPath(kIOMasterPortDefault,
                            kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");

    if(!root_domain) return;

    note_port = IONotificationPortCreate(MACH_PORT_NULL);
    if(!note_port) goto exit;

    ret = IOServiceAddInterestNotification(note_port, root_domain,
                kIOGeneralInterest, RootDomainInterest,
                NULL, &notification_object);
    if (ret != kIOReturnSuccess) goto exit;

    IONotificationPortSetDispatchQueue(note_port, _getPMMainQueue());

exit:
    // Do not release notification_object, would uninstall notification
    // Do not release runLoopSrc because it's 'owned' by note_port, and both
    // must be kept around to receive this notification
    if(MACH_PORT_NULL != root_domain) IOObjectRelease(root_domain);
}

#if !TARGET_OS_IPHONE
static void initializeUserNotifications(void)
{
    SCDynamicStoreRef   localStore = _getSharedPMDynamicStore();
    CFArrayRef          keys = NULL;

    gConsoleNotifyKey = SCDynamicStoreKeyCreateConsoleUser(NULL);
    if (gConsoleNotifyKey)
    {
        keys = CFArrayCreate(NULL, (const void **)&gConsoleNotifyKey,
                                1, &kCFTypeArrayCallBacks);

        if (keys) {
            SCDynamicStoreSetNotificationKeys(localStore, keys, NULL);
            CFRelease(keys);
        }
    }

    SystemLoadUserStateHasChanged();
}

static void enableSleepWakeWdog()
{
    io_service_t                rootDomainService = IO_OBJECT_NULL;
    io_connect_t                rotDomainConnect = IO_OBJECT_NULL;
    kern_return_t               kr = 0;
    IOReturn                    ret;

    // Check if system supports NTS
    if (IONoteToSelfSupported() == false)
        return;

    // Find it
    rootDomainService = getRootDomain();
    if (IO_OBJECT_NULL == rootDomainService) {
        goto exit;
    }

    // Open it
    kr = IOServiceOpen(rootDomainService, mach_task_self(), 0, &rotDomainConnect);
    if (KERN_SUCCESS != kr) {
        goto exit;
    }

    ret = IOConnectCallMethod(rotDomainConnect, kPMSleepWakeWatchdogEnable,
                    NULL, 0,
                    NULL, 0, NULL,
                    NULL, NULL, NULL);

    if (kIOReturnSuccess != ret)
    {
        goto exit;
    }

exit:
    if (IO_OBJECT_NULL != rotDomainConnect)
        IOServiceClose(rotDomainConnect);

}

#endif
static void initializeSleepWakeNotifications(void)
{
    IONotificationPortRef           notify;
    io_object_t                     anIterator;

    _pm_ack_port = IORegisterForSystemPower(0, &notify,
                                    SleepWakeCallback, &anIterator);

    if ( _pm_ack_port != MACH_PORT_NULL && notify ) {
        IONotificationPortSetDispatchQueue(notify, _getPMMainQueue());
    }
}
