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
/* load
 *
 * configd entry point
 */
// Global keys
static CFStringRef              gTZNotificationNameString           = NULL;

IOPMNotificationHandle          gESPreferences                      = 0;

static io_connect_t             _pm_ack_port                        = 0;
static io_iterator_t            _ups_added_noteref                  = 0;
static int                      _alreadyRunningIOUPSD               = 0;

static int                      gCPUPowerNotificationToken          = 0;
static bool                     gExpectingWakeFromSleepClockResync  = false;
static CFAbsoluteTime           *gLastWakeTime                      = NULL;
static CFTimeInterval           *gLastSMCS3S0WakeInterval           = NULL;
static CFStringRef              gCachedNextSleepWakeUUIDString      = NULL;
static int                      gLastWakeTimeToken                  = -1;
static int                      gLastSMCS3S0WakeIntervalToken       = -1;

static LoginWindowNotifyTokens  lwNotify = {0,0,0,0,0};

static CFStringRef              gConsoleNotifyKey                   = NULL;
static bool                     gDisplayIsAsleep = false;
static struct timeval           gLastSleepTime                      = {0, 0};

static mach_port_t              serverPort                          = MACH_PORT_NULL;
#ifndef POWERD_TEST
__private_extern__ CFMachPortRef pmServerMachPort                   = NULL;
#endif
static bool                     gSMCSupportsWakeupTimer             = true;
static int                      _darkWakeThermalEventCount          = 0;
static dispatch_source_t        gDWTMsgDispatch; /* Darkwake thermal emergency message handler dispatch */



// foward declarations
static void initializeESPrefsNotification(void);
static void initializeInterestNotifications(void);
static void initializeHIDInterestNotifications(IONotificationPortRef notify_port);
static void initializeTimezoneChangeNotifications(void);
static void initializeCalendarResyncNotification(void);
static void initializeShutdownNotifications(void);
static void initializeRootDomainInterestNotifications(void);
static void initializeUserNotifications(void);
static void enableSleepWakeWdog();
static void displayMatched(void *, io_iterator_t);
static void displayPowerStateChange(
                                    void *ref,
                                    io_service_t service,
                                    natural_t messageType,
                                    void *arg);


static void initializeSleepWakeNotifications(void);

static void SleepWakeCallback(void *,io_service_t, natural_t, void *);
static void ESPrefsHaveChanged(void);
static CFMutableDictionaryRef copyUPSMatchingDict( );
static void _ioupsd_exited(pid_t, int, struct rusage *, void *);
static void UPSDeviceAdded(void *, io_iterator_t);
static void ioregBatteryMatch(void *, io_iterator_t);
static void ioregBatteryInterest(void *, io_service_t, natural_t, void *);
static void RootDomainInterest(void *, io_service_t, natural_t, void *);
static void broadcastGMTOffset(void);

static void pushNewSleepWakeUUID(void);

static void calendarRTCDidResync(
                                 CFMachPortRef port,
                                 void *msg,
                                 CFIndex size,
                                 void *info);

static void lwShutdownCallback(
                               CFMachPortRef port,
                               void *msg,
                               CFIndex size,
                               void *info);

static void timeZoneChangedCallBack(
                                    CFNotificationCenterRef center,
                                    void *observer,
                                    CFStringRef notificationName,
                                    const void *object,
                                    CFDictionaryRef userInfo);

static boolean_t pm_mig_demux(
                              mach_msg_header_t * request,
                              mach_msg_header_t * reply);

static void mig_server_callback(
                                CFMachPortRef port,
                                void *msg,
                                CFIndex size,
                                void *info);

static void incoming_XPC_connection(xpc_connection_t);
static void xpc_register(void);

static CFStringRef
serverMPCopyDescription(const void *info)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<IOKit Power Management MIG server>"));
}



#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


int main(int argc __unused, char *argv[] __unused)
{
    CFRunLoopSourceRef      cfmp_rls = 0;
    CFMachPortContext       context  = { 0, (void *)1, NULL, NULL, serverMPCopyDescription };
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
        // Finish setting up mig handler callback on pmServerMachPort
        pmServerMachPort = _SC_CFMachPortCreateWithPort(
                                "PowerManagement",
                                serverPort, 
                                mig_server_callback, 
                                &context);
    }
    if (pmServerMachPort) {
        cfmp_rls = CFMachPortCreateRunLoopSource(0, pmServerMachPort, 0);
        if (cfmp_rls) {
            CFRunLoopAddSource(CFRunLoopGetCurrent(), cfmp_rls, kCFRunLoopDefaultMode);
            CFRelease(cfmp_rls);
        }
    }

    _getPMRunLoop();
    
    PMStoreLoad();
    
    initializeESPrefsNotification();
    initializeInterestNotifications();
    initializeTimezoneChangeNotifications();
    initializeCalendarResyncNotification();    
    initializeShutdownNotifications();
    initializeRootDomainInterestNotifications();
    
    initializeUserNotifications();
    _oneOffHacksSetup();
    
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

    UPSLowPower_prime();
    TTYKeepAwake_prime();
    ExternalMedia_prime();

    createOnBootAssertions();
    enableSleepWakeWdog();
    ads_prime();
    standbyTimer_prime();

    _unclamp_silent_running(false);
    notify_post(kIOUserAssertionReSync);
    logASLMessagePMStart();


    BatteryTimeRemaining_finish();

    CFRunLoopRun();
    return 0;
}




static void ioregBatteryMatch(
    void *refcon, 
    io_iterator_t b_iter) 
{
    IOPMBattery                 *tracking;
    IONotificationPortRef       notify = (IONotificationPortRef)refcon;
    io_registry_entry_t         battery;
    io_object_t                 notification_ref;
    
    while((battery = (io_registry_entry_t)IOIteratorNext(b_iter))) 
    {
        // Add battery to our list of batteries
        tracking = _newBatteryFound(battery);
        
        LogObjectRetainCount("PM::BatteryMatch(M0) me", battery);
        
        // And install an interest notification on it
        IOServiceAddInterestNotification(notify, battery, 
                            kIOGeneralInterest, ioregBatteryInterest,
                            (void *)tracking, &notification_ref);

        LogObjectRetainCount("PM::BatteryMatch(M1) me", battery);
        LogObjectRetainCount("PM::BatteryMatch(M1) msg_port", notification_ref);

        tracking->msg_port = notification_ref;
        IOObjectRelease(battery);
    }
    InternalEvaluateAssertions();
    InternalEvalConnections();
    evalTcpkaForPSChange();
}

__private_extern__ void ioregBatteryProcess(IOPMBattery *changed_batt,
                                            io_service_t batt)
{
    if (!changed_batt) {
        return;
    }

    PowerSources oldPS = _getPowerSource();

    // Update the arbiter
    changed_batt->me = (io_registry_entry_t)batt;
    _batteryChanged(changed_batt);

    if (changed_batt->properties == NULL) {
        // Nothing to do
        return;
    }

    LogObjectRetainCount("PM:BatteryInterest(B0) msg_port", changed_batt->msg_port);
    LogObjectRetainCount("PM:BatteryInterest(B1) msg_port", changed_batt->me);

    IOPMBattery **batt_stats = _batteries();
    kernelPowerSourcesDidChange(changed_batt);

    SystemLoadBatteriesHaveChanged(batt_stats);
    InternalEvaluateAssertions();
    InternalEvalConnections();
    if (_getPowerSource() != oldPS) {
        evalTcpkaForPSChange();
        evalProxForPSChange();
    }

    return;
}

static void ioregBatteryInterest(
    void *refcon,
    io_service_t batt,
    natural_t messageType,
    void *messageArgument)
{
    if (kIOPMMessageBatteryStatusHasChanged != messageType) {
        return;
    }

    IOPMBattery *changed_batt = (IOPMBattery *)refcon;

    ioregBatteryProcess(changed_batt, batt);
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

    spindump_pid = _SCDPluginExecCommand(NULL, NULL, 0, 0, "/usr/sbin/spindump", spindump_args);
}

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

    BatteryTimeRemainingSleepWakeNotification(messageType);
    CFStringRef uuid = IOPMSleepWakeCopyUUID();

    // Log Message to MessageTracer

    // Acknowledge message
    switch ( messageType ) {
        case kIOMessageSystemWillSleep:
            INFO_LOG("Received kIOMessageSystemWillSleep. UUID: %{public}@\n", uuid);
            if (userActiveRootDomain()) {
                userActiveHandleSleep();
            }
            IOAllowPowerChange(_pm_ack_port, (long)acknowledgementToken);
             break;

        case kIOMessageCanSystemSleep:
            INFO_LOG("Received kIOMessageCanSystemSleep. UUID: %{public}@\n", uuid);
            checkForAsyncAssertions(acknowledgementToken);
            // Response is sent thru sendSleepNotificationResponse()
            break;

        case kIOMessageSystemWillPowerOn:
        case kIOMessageSystemWillNotSleep:
            INFO_LOG("Received %{public}s. UUID: %{public}@\n",
                    (messageType == kIOMessageSystemWillPowerOn) ? "kIOMessageSystemWillPowerOn" :
                        "kIOMessageSystemWillNotSleep", uuid);
            _set_sleep_revert(true);
            checkPendingWakeReqs(ALLOW_PURGING);
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
    UPSLowPowerPrefsHaveChanged();
    TTYKeepAwakePrefsHaveChanged();
    evalProximityPrefsChange();
    SystemLoadPrefsHaveChanged();
    
    return;
}


/* _ioupsd_exited
 *
 * Gets called (by configd) when /usr/libexec/ioupsd exits
 */
static void _ioupsd_exited(
    pid_t           pid,
    int             status,
    struct rusage   *rusage,
    void            *context)
{
    asl_log(0,0,ASL_LEVEL_ERR, "ioupsd exited with status %d\n", status);
    if(0 != status)
    {
        // ioupsd didn't exit cleanly.
        syslog(
            LOG_ERR, 
           "PowerManagement: /usr/libexec/ioupsd(%d) has exited with status %d\n", 
           pid, 
           status);
        
        // relaunch
        char *argv[2] = {"/usr/libexec/ioupsd", NULL};
        _SCDPluginExecCommand(&_ioupsd_exited, 0, 0, 0,
                              "/usr/libexec/ioupsd", argv);
    } else {
        _alreadyRunningIOUPSD = 0;

        // Re-scan the registry for any objects of interest that might have
        // been published before _ioupsd_exited() is called.
        io_iterator_t   iter;
        CFDictionaryRef matchingDict = copyUPSMatchingDict();
        if (matchingDict) {
            kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iter);
            matchingDict = 0; // reference consumed by IOServiceGetMatchingServices

            if ((kr == kIOReturnSuccess) && (iter != MACH_PORT_NULL)) {
                UPSDeviceAdded(NULL, iter);
                IOObjectRelease(iter);
            }
        }

    }
}


/* UPSDeviceAdded
 *
 * A UPS has been detected running on the system.
 * 
 */
static void UPSDeviceAdded(void *refCon, io_iterator_t iterator)
{
    io_object_t                 upsDevice = MACH_PORT_NULL;
        
    asl_log(0,0,ASL_LEVEL_ERR, "UPSDeviceAdded. _alreadyRunningIOUPSD:%d\n", _alreadyRunningIOUPSD);
    while ( (upsDevice = IOIteratorNext(iterator)) )
    {
        // If not running, launch the management process ioupsd now.
        if(!_alreadyRunningIOUPSD) {
            char *argv[2] = {"/usr/libexec/ioupsd", NULL};

            _alreadyRunningIOUPSD = 1;
            _SCDPluginExecCommand(&_ioupsd_exited, 0, 0, 0,
                "/usr/libexec/ioupsd", argv);
        }
        IOObjectRelease(upsDevice);
    }
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
static void lwShutdownCallback( 
    CFMachPortRef port,
    void *msg,
    CFIndex size,
    void *info)
{
    mach_msg_header_t   *header = (mach_msg_header_t *)msg;
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

    switch (messageType)
    {
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

            if ( params->stateNumber < kWranglerPowerStateSleep)
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

__private_extern__ bool isDisplayAsleep( )
{
    return gDisplayIsAsleep;
}

/* initializeESPrefsNotification
 *
 * Registers a handler that configd calls when someone changes com.apple.PowerManagement.xml
 */
static void
initializeESPrefsNotification(void)
{
    gESPreferences = IOPMRegisterPrefsChangeNotification(dispatch_get_main_queue(),
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
    xpc_connection_set_target_queue(peer, dispatch_get_main_queue());

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
                     else if ((inEvent = xpc_dictionary_get_value(event, kAssertionCreateMsg))) {
                        asyncAssertionCreate(peer, event);
                     }
                     else if ((inEvent = xpc_dictionary_get_value(event, kAssertionReleaseMsg))) {
                        asyncAssertionRelease(peer, event);
                     }
                     else if ((inEvent = xpc_dictionary_get_value(event, kAssertionPropertiesMsg))) {
                        asyncAssertionProperties(peer, event);
                     }
                     else if ((inEvent = xpc_dictionary_get_value(event, kPSAdapterDetails))) {
                         sendAdapterDetails(peer, event);
                     }
                     else if ((inEvent = xpc_dictionary_get_value(event, kInactivityWindowKey))) {
                         setInactivityWindow(peer, event);
                     }
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
                                                    dispatch_get_main_queue(),
                                                    XPC_CONNECTION_MACH_SERVICE_LISTENER);

    xpc_connection_set_target_queue(connection, dispatch_get_main_queue());

    xpc_connection_set_event_handler(connection, ^(xpc_object_t event) {
            xpc_type_t type = xpc_get_type(event);
            if (type == XPC_TYPE_CONNECTION) {
                incoming_XPC_connection((xpc_connection_t)event);
            }
        });
    
    xpc_connection_resume(connection);
}


static boolean_t 
pm_mig_demux(
    mach_msg_header_t * request,
    mach_msg_header_t * reply)
{
    boolean_t processed = FALSE;

    processed = powermanagement_server(request, reply);

    if (processed) 
        return true;
    
    // mig request is not in our subsystem range!
    // generate error reply packet
    reply->msgh_bits        = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request->msgh_bits), 0);
    reply->msgh_remote_port = request->msgh_remote_port;
    reply->msgh_size        = sizeof(mig_reply_error_t);    /* Minimal size */
    reply->msgh_local_port  = MACH_PORT_NULL;
    reply->msgh_id          = request->msgh_id + 100;
    ((mig_reply_error_t *)reply)->NDR = NDR_record;
    ((mig_reply_error_t *)reply)->RetCode = MIG_BAD_ID;
    
    return processed;
}



static void
mig_server_callback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mig_reply_error_t * bufRequest = msg;
    mig_reply_error_t * bufReply = CFAllocatorAllocate(
        NULL, _powermanagement_subsystem.maxsize, 0);
    mach_msg_return_t   mr;
    int                 options;

    __MACH_PORT_DEBUG(true, "mig_server_callback", serverPort);
    
    /* we have a request message */
    (void) pm_mig_demux(&bufRequest->Head, &bufReply->Head);

    if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
         (bufReply->RetCode != KERN_SUCCESS)) {

        if (bufReply->RetCode == MIG_NO_REPLY) {
            /*
             * This return code is a little tricky -- it appears that the
             * demux routine found an error of some sort, but since that
             * error would not normally get returned either to the local
             * user or the remote one, we pretend it's ok.
             */
            goto out;
            
        }

        /*
         * destroy any out-of-line data in the request buffer but don't destroy
         * the reply port right (since we need that to send an error message).
         */
        bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
        mach_msg_destroy(&bufRequest->Head);
    }

    if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
        /* no reply port, so destroy the reply */
        if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
            mach_msg_destroy(&bufReply->Head);
        }
        goto out;
    }

    /*
     * send reply.
     *
     * We don't want to block indefinitely because the client
     * isn't receiving messages from the reply port.
     * If we have a send-once right for the reply port, then
     * this isn't a concern because the send won't block.
     * If we have a send right, we need to use MACH_SEND_TIMEOUT.
     * To avoid falling off the kernel's fast RPC path unnecessarily,
     * we only supply MACH_SEND_TIMEOUT when absolutely necessary.
     */

    options = MACH_SEND_MSG;
    if (MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) != MACH_MSG_TYPE_MOVE_SEND_ONCE) {
        options |= MACH_SEND_TIMEOUT;
    }
    mr = mach_msg(&bufReply->Head,          /* msg */
              options,                      /* option */
              bufReply->Head.msgh_size,     /* send_size */
              0,                            /* rcv_size */
              MACH_PORT_NULL,               /* rcv_name */
              MACH_MSG_TIMEOUT_NONE,        /* timeout */
              MACH_PORT_NULL);              /* notify */


    /* Has a message error occurred? */
    switch (mr) {
        case MACH_SEND_INVALID_DEST:
        case MACH_SEND_TIMED_OUT:
            /* the reply can't be delivered, so destroy it */
            mach_msg_destroy(&bufReply->Head);
            break;

        default :
            /* Includes success case.  */
            break;
    }


out:
    CFAllocatorDeallocate(NULL, bufReply);
    return;

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
        SystemLoadUserStateHasChanged();
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
    case kIOPMSetNoPoll:
        BatterySetNoPoll(inValue ? true:false);
        break;

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

    case kIOPMPushConnectionActive:
        setPushConnectionState(inValue ? true:false);
        break;

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
#if TCPKEEPALIVE
    case kIOPMTCPKeepAliveExpirationOverride:
            *outValue = (int)getTCPKeepAliveOverrideSec();
        break;
            
    case kIOPMTCPKeepAliveIsActive:
            *outValue = (getTCPKeepAliveState(NULL, 0) == kActive) ? true : false;
            break;
#endif
    case kIOPMWakeOnLanIsActive:
            *outValue = getWakeOnLanState( );
            break;

    case kIOPMPushConnectionActive:
        *outValue = getPushConnectionState();
        break;

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
    io_iterator_t               display_iter = 0;
    CFRunLoopSourceRef          rlser = 0;
    
    kern_return_t               kr;
    
    /* Notifier */
    notify_port = IONotificationPortCreate(0);
    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(!rlser) return;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);


    kr = IOServiceAddMatchingNotification(
                                notify_port,
                                kIOFirstMatchNotification,
                                IOServiceMatching("IOPMPowerSource"),
                                ioregBatteryMatch,
                                (void *)notify_port,
                                &battery_iter);
    if(KERN_SUCCESS == kr)
    {
        // Install notifications on existing instances.
        ioregBatteryMatch((void *)notify_port, battery_iter);
    }

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

    // Listen for Power devices and Battery Systems to start ioupsd
    initializeHIDInterestNotifications(notify_port);
}

static CFMutableDictionaryRef
copyUPSMatchingDict( )
{
    CFMutableArrayRef devicePairs = NULL;
    CFMutableDictionaryRef matchingDict = NULL;
    CFNumberRef cfUsageKey = NULL;
    bool dictCreated = false;
    CFMutableDictionaryRef pair  = NULL;
    CFNumberRef  cfUsagePageKey = NULL;
    int i, count;

    int usagePages[]    = {kIOPowerDeviceUsageKey,  kIOBatterySystemUsageKey,   kHIDPage_AppleVendor,                   kHIDPage_PowerDevice};
    int usages[]        = {0,                       0,                          kHIDUsage_AppleVendor_AccessoryBattery, kHIDUsage_PD_PeripheralDevice};

    matchingDict = IOServiceMatching(kIOHIDDeviceKey);
    if (!matchingDict) {
        goto exit;
    }

    devicePairs = CFArrayCreateMutable(kCFAllocatorDefault, 4, &kCFTypeArrayCallBacks);
    if (!devicePairs) {
        goto exit;
    }

    count = sizeof(usagePages)/sizeof(usagePages[0]);
    for (i = 0; i < count; i++) {
        if (!usagePages[i]) {
            continue;
        }

        pair = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                &kCFTypeDictionaryValueCallBacks);
        if (!pair) {
            goto exit;
        }

        // We need to box the Usage Page value up into a CFNumber... sorry bout that
        cfUsagePageKey = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePages[i]);
        if (!cfUsagePageKey) {
            goto exit;
        }

        CFDictionarySetValue(pair, CFSTR(kIOHIDDeviceUsagePageKey), cfUsagePageKey);
        CFRelease(cfUsagePageKey);
        cfUsagePageKey = NULL;

        if (usages[i]) {
            cfUsageKey = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usages[i]);
            if (!cfUsageKey) {
                goto exit;
            }

            CFDictionarySetValue(pair, CFSTR(kIOHIDDeviceUsageKey), cfUsageKey);
            CFRelease(cfUsageKey);
            cfUsageKey = NULL;
        }

        CFArrayAppendValue(devicePairs, pair);
        CFRelease(pair);
        pair = NULL;
    }

    CFDictionarySetValue(matchingDict, CFSTR(kIOHIDDeviceUsagePairsKey), devicePairs);
    CFRelease(devicePairs);
    devicePairs = 0;
    dictCreated = true;

exit:

    if (devicePairs) {
        CFRelease(devicePairs);
    }
    if (pair) {
        CFRelease(pair);
    }
    if (cfUsagePageKey) {
        CFRelease(cfUsagePageKey);
    }
    if (cfUsageKey) {
        CFRelease(cfUsageKey);
    }
    if (!dictCreated && matchingDict) {
        CFRelease(matchingDict);
        matchingDict = NULL;
    }

    return matchingDict;
}

static void
initializeHIDInterestNotifications(IONotificationPortRef notify_port)
{
    CFMutableDictionaryRef matchingDict = NULL;
    asl_log(0,0,ASL_LEVEL_ERR, "Registering for UPS devices\n");
#if 0
    matchingDict = IOServiceMatching(kIOHIDDeviceKey);
    if (!matchingDict) {
        return;
    }

    // We need to box the Usage Page value up into a CFNumber... sorry bout that
    CFNumberRef cfUsagePageKey = CFNumberCreate(kCFAllocatorDefault,
                                                kCFNumberIntType,
                                                &usagePage);
    if (!cfUsagePageKey) {
        CFRelease(matchingDict);
        return;
    }

    CFDictionarySetValue(matchingDict,
                         CFSTR(kIOHIDDeviceUsagePageKey),
                         cfUsagePageKey);
    CFRelease(cfUsagePageKey);

    if (usage) {
        CFNumberRef cfUsageKey = CFNumberCreate(kCFAllocatorDefault,
                                                    kCFNumberIntType,
                                                    &usage);
        if (!cfUsageKey) {
            CFRelease(matchingDict);
            return;
        }

        CFDictionarySetValue(matchingDict,
                             CFSTR(kIOHIDDeviceUsageKey),
                             cfUsageKey);
        CFRelease(cfUsageKey);
    }
#else
    matchingDict = copyUPSMatchingDict();
#endif
    
    // Now set up a notification to be called when a device is first matched by
    // I/O Kit. Note that this will not catch any devices that were already
    // plugged in so we take care of those later.
    kern_return_t kr =
            IOServiceAddMatchingNotification(notify_port,
                                             kIOFirstMatchNotification,
                                             matchingDict,
                                             UPSDeviceAdded,
                                             NULL,
                                             &_ups_added_noteref);

    matchingDict = 0; // reference consumed by AddMatchingNotification
    if ( kr == kIOReturnSuccess ) {
        // Check for existing matching devices and launch ioupsd if present.
        UPSDeviceAdded( NULL, _ups_added_noteref);
    }
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

    distNoteCenter = CFNotificationCenterGetDistributedCenter();
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
    CFMachPortRef               mpref = NULL;
    CFRunLoopSourceRef          mpsrc = NULL;
    mach_port_t                 nport, tport;
    kern_return_t               result;

    result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &nport);
    if (result != KERN_SUCCESS) {
        goto exit;
    }
    result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &tport);
    if (result != KERN_SUCCESS) {
        goto exit;
    }
    result = mach_port_move_member(mach_task_self(), tport, nport);
    if (result != KERN_SUCCESS) {
        goto exit;
    }
    result = host_request_notification(mach_host_self(), HOST_NOTIFY_CALENDAR_CHANGE, tport);
    if (result != KERN_SUCCESS) {
        goto exit;
    }

    mpref = _SC_CFMachPortCreateWithPort("PowerManagement/calendarResync", tport, calendarRTCDidResync, NULL);
    if (mpref) {
        mpsrc = CFMachPortCreateRunLoopSource(0, mpref, 0);
        if (mpsrc) {
            CFRunLoopAddSource(CFRunLoopGetCurrent(), mpsrc, kCFRunLoopDefaultMode);
            CFRelease(mpsrc);
        }
        CFRelease(mpref);
    }

exit:
    return;
}

static void calendarRTCDidResync_getSMCWakeInterval(void)
{
    CFAbsoluteTime      lastWakeTime;
    struct timeval      lastSleepTime;
    size_t              len = sizeof(struct timeval);

    // Capture this as early as possible
    lastWakeTime = CFAbsoluteTimeGetCurrent();
        
    if (!gExpectingWakeFromSleepClockResync) {
        // This is a non-wake-from-sleep clock resync, so we'll ignore it.
        goto exit;
    }

    if (sysctlbyname("kern.sleeptime", &lastSleepTime, &len, NULL, 0) ||
        ((gLastSleepTime.tv_sec  == lastSleepTime.tv_sec) &&
         (gLastSleepTime.tv_usec == lastSleepTime.tv_usec)))
    {
        // This is a clock resync after sleep has started but before
        // platform sleep.
        goto exit;
    }

    // if needed, init standalone memory for last wake time data
    if (!gLastSMCS3S0WakeInterval) {
        size_t bufSize;
        
        bufSize = sizeof(*gLastWakeTime) + sizeof(*gLastSMCS3S0WakeInterval);
        if (0 != vm_allocate(mach_task_self(), (void*)&gLastWakeTime,
                             bufSize, VM_FLAGS_ANYWHERE)) {
            return;
        }
        gLastSMCS3S0WakeInterval = gLastWakeTime + 1;
    } else {
        // validate pointers allocated earlier
        if (!gLastWakeTime || !gLastSMCS3S0WakeInterval)
            return;
    }
    
    // This is a wake-from-sleep resync, so commit the last wake time
    *gLastWakeTime = lastWakeTime;
    *gLastSMCS3S0WakeInterval = 0;
    gExpectingWakeFromSleepClockResync = false;
    
    // Re-enable battery time remaining calculations
    (void) BatteryTimeRemainingRTCDidResync();
    
exit:
    return;
}

static void calendarRTCDidResync(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_header_t   *header = (mach_msg_header_t *)msg;

    if (!header || HOST_CALENDAR_CHANGED_REPLYID != header->msgh_id) {
        return;
    }
    
    // renew our request for calendar change notification
    (void) host_request_notification(mach_host_self(), HOST_NOTIFY_CALENDAR_CHANGE,
                                     header->msgh_local_port);

    calendarRTCDidResync_getSMCWakeInterval();
    AutoWakeCalendarChange();

    // calendar has resync. Safe to get timestamps now
    CFAbsoluteTime wakeStart;
    CFTimeInterval smcAdjustment;
    IOReturn rc = IOPMGetLastWakeTime(&wakeStart, &smcAdjustment);
    if (rc == kIOReturnSuccess) {
        uint64_t wakeStartTimestamp = CFAbsoluteTimeToMachAbsoluteTime(wakeStart);
        DEBUG_LOG("WakeTime: Calendar resynced\n");
        updateCurrentWakeStart(wakeStartTimestamp);
        updateWakeTime();
    }
    return;
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
    *out_wake_len = 0;
    *out_delta_len = 0;
    *return_val = kIOReturnInvalid;

    if (gExpectingWakeFromSleepClockResync) {
        *return_val = kIOReturnNotReady;
        return KERN_SUCCESS;
    }

    if (!gSMCSupportsWakeupTimer) {
        *return_val = kIOReturnNotFound;
        return KERN_SUCCESS;
    };

    *out_wake_data = (vm_offset_t)gLastWakeTime;
    *out_wake_len = sizeof(*gLastWakeTime);
    *out_delta_data = (vm_offset_t)gLastSMCS3S0WakeInterval;
    *out_delta_len = sizeof(*gLastSMCS3S0WakeInterval);

    *return_val = kIOReturnSuccess;

    return KERN_SUCCESS;
}


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


static void 
initializeShutdownNotifications(void)
{
    CFMachPortRef       gNotifyMachPort = NULL;
    CFRunLoopSourceRef  gNotifyMachPortRLS = NULL;
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

    gNotifyMachPort = _SC_CFMachPortCreateWithPort(
                                "PowerManagement/shutdown",
                                our_port,
                                lwShutdownCallback,
                                NULL);

    if (gNotifyMachPort) {
        gNotifyMachPortRLS = CFMachPortCreateRunLoopSource(0, gNotifyMachPort, 0);
        if (gNotifyMachPortRLS) {        
            CFRunLoopAddSource(CFRunLoopGetCurrent(), gNotifyMachPortRLS, kCFRunLoopDefaultMode);
            CFRelease(gNotifyMachPortRLS);
        }
        CFRelease(gNotifyMachPort);
    }
}

static void handleDWThermalMsg(CFStringRef wakeType)
{
    CFMutableDictionaryRef options = NULL;

    if (wakeType == NULL)
        getPlatformWakeReason(NULL, &wakeType);

    INFO_LOG("DarkWake Thermal Emergency message is received. BTWake: %d ssWake:%d ProxWake:%d NotificationWake:%d\n",
            isA_BTMtnceWake(), isA_SleepSrvcWake(), checkForAppWakeReason(CFSTR(kProximityWakeReason)), isA_NotificationDisplayWake());
    gateProximityDarkWakeState(kPMAllowSleep);
    if ( (isA_BTMtnceWake() || isA_SleepSrvcWake() || checkForAppWakeReason(CFSTR(kProximityWakeReason))) &&
            (!isA_NotificationDisplayWake()) && (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) ||
                        CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService))
#if TCPKEEPALIVE
            && !((getTCPKeepAliveState(NULL, 0) == kActive) && checkForActivesByType(kInteractivePushServiceType)) ) {
#else
        ) {
#endif
        
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


static void 
RootDomainInterest(
    void *refcon, 
    io_service_t root_domain, 
    natural_t messageType, 
    void *messageArgument)
{
    static CFStringRef  _uuidString = NULL;
    CFStringRef wakeReason = NULL, wakeType = NULL;
    
    if (messageType == kIOPMMessageDriverAssertionsChanged)
    {
        CFNumberRef     driverAssertions = 0;
        uint32_t        _driverAssertions = 0;

        // Read driver assertion status
        driverAssertions = IORegistryEntryCreateCFProperty(getRootDomain(), CFSTR(kIOPMAssertionsDriverKey), 0, 0);
        
        if (driverAssertions) {
            CFNumberGetValue(driverAssertions, kCFNumberIntType, &_driverAssertions);
            _PMAssertionsDriverAssertionsHaveChanged(_driverAssertions);
            CFRelease(driverAssertions);
        }
    }
    
    if (messageType == kIOPMMessageSystemPowerEventOccurred)
    {
        // Let System Events know about just-occurred thermal state change
        
        PMSystemEventsRootDomainInterest();
    }

    if(messageType == kIOPMMessageDarkWakeThermalEmergency)
    {
        mt2RecordThermalEvent(kThermalStateSleepRequest);
        _darkWakeThermalEventCount++;

        getPlatformWakeReason(&wakeReason, &wakeType);
        if (CFEqual(wakeReason, CFSTR("")) && CFEqual(wakeType, CFSTR("")))
        {
            // Thermal emergency msg is received too early before wake type is 
            // determined. Delay the handler for a short handler until we know
            // the wake type
            if (gDWTMsgDispatch)
                dispatch_suspend(gDWTMsgDispatch);
            else {
                gDWTMsgDispatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0,
                        0, dispatch_get_main_queue()); 
                dispatch_source_set_event_handler(gDWTMsgDispatch, ^{
                    handleDWThermalMsg(NULL);
                });
                    
                dispatch_source_set_cancel_handler(gDWTMsgDispatch, ^{
                    if (gDWTMsgDispatch) {
                        dispatch_release(gDWTMsgDispatch);
                        gDWTMsgDispatch = 0;
                    }
                }); 
            }

            dispatch_source_set_timer(gDWTMsgDispatch, 
                    dispatch_walltime(NULL, kDWTMsgHandlerDelay * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
            dispatch_resume(gDWTMsgDispatch);
        }
        else
        {
            handleDWThermalMsg(wakeType);
            CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode,
                        ^{ logASLAssertionTypeSummary(kInteractivePushServiceType);});
            CFRunLoopWakeUp(_getPMRunLoop());
        }

    }

    if (messageType == kIOPMMessageLaunchBootSpinDump)
    {
        dispatch_async(dispatch_get_main_queue(), ^{ takeSpindump(); });
    }

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

    if (messageType == kIOPMMessageUserIsActiveChanged)
    {
        userActiveHandleRootDomainActivity();
    }
}

static void 
initializeRootDomainInterestNotifications(void)
{
    IONotificationPortRef       note_port = MACH_PORT_NULL;
    CFRunLoopSourceRef          runLoopSrc = NULL;
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
    
    runLoopSrc = IONotificationPortGetRunLoopSource(note_port);
    
    if (runLoopSrc)
    {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSrc, kCFRunLoopDefaultMode);
    }
    
exit:
    // Do not release notification_object, would uninstall notification
    // Do not release runLoopSrc because it's 'owned' by note_port, and both
    // must be kept around to receive this notification
    if(MACH_PORT_NULL != root_domain) IOObjectRelease(root_domain);
}

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

static void initializeSleepWakeNotifications(void)
{
    IONotificationPortRef           notify;
    io_object_t                     anIterator;

    _pm_ack_port = IORegisterForSystemPower(0, &notify, 
                                    SleepWakeCallback, &anIterator);
 
    if ( _pm_ack_port != MACH_PORT_NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }
}
