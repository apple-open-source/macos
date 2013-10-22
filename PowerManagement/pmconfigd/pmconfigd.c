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
 
#include <CoreFoundation/CoreFoundation.h>

#include <syslog.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <notify.h> 
#include <asl.h>
#include <uuid/uuid.h>
#include <pthread.h>
#include <bsm/libbsm.h>
#include <sys/sysctl.h>
#if !TARGET_OS_EMBEDDED
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#endif

#include "powermanagementServer.h" // mig generated

#include "PMStore.h"
#include "PMSettings.h"
#include "PSLowPower.h"
#include "BatteryTimeRemaining.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "PMAssertions.h"
#include "PrivateLib.h"
#include "TTYKeepAwake.h"
#include "PMSystemEvents.h"
#include "SystemLoad.h"
#include "PMConnection.h"
#include "ExternalMedia.h"
//#include "Platform.h"

// To support importance donation across IPCs
#include <libproc_internal.h>

#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath      "com.apple.PowerManagement.xml"
#define pwrLogDirName       "/System/Library/PowerEvents"

#ifndef kIOUPSDeviceKey
// Also defined in ioupsd/IOUPSPrivate.h
#define kIOUPSDeviceKey                 "UPSDevice" 
#endif

/*
 * BSD notifications from loginwindow indicating shutdown
 */
// kLWShutdownInitiated
//   User clicked shutdown: may be aborted later
#define kLWShutdowntInitiated    "com.apple.system.loginwindow.shutdownInitiated"

// kLWRestartInitiated
//   User clicked restart: may be aborted later
#define kLWRestartInitiated     "com.apple.system.loginwindow.restartinitiated"

// kLWLogoutCancelled
//   A previously initiated shutdown, restart, or logout, has been cancelled.
#define kLWLogoutCancelled      "com.apple.system.loginwindow.logoutcancelled"

// kLWLogoutPointOfNoReturn
//   A previously initiated shutdown, restart, or logout has succeeded, and is 
//   no longer abortable by anyone. Point of no return!
#define kLWLogoutPointOfNoReturn    "com.apple.system.loginwindow.logoutNoReturn"


#define kDWTMsgHandlerDelay         10  // Time(in secs) for which DW Thermal msg handler is delayed

#define LogObjectRetainCount(x, y) do {} while(0)
/* #define LogObjectRetainCount(x, y) do { \
    asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: kernel retain = %d, user retain = %d\n", \
        x, IOObjectGetKernelRetainCount(y), IOObjectGetUserRetainCount(y)); } while(0)
*/


// Global keys
static CFStringRef              gTZNotificationNameString           = NULL;

static SCPreferencesRef         gESPreferences                      = NULL;

static io_connect_t             _pm_ack_port                        = 0;
static io_iterator_t            _ups_added_noteref                  = 0;
static int                      _alreadyRunningIOUPSD               = 0;
static int                      _darkWakeThermalEventCount          = 0;

static int                      gCPUPowerNotificationToken          = 0;
static bool                     gExpectingWakeFromSleepClockResync  = false;
static CFAbsoluteTime           *gLastWakeTime                      = NULL;
static CFTimeInterval           *gLastSMCS3S0WakeInterval           = NULL;
static CFStringRef              gCachedNextSleepWakeUUIDString      = NULL;
static int                      gLWShutdownNotificationToken        = 0;
static int                      gLWRestartNotificationToken         = 0;
static int                      gLWLogoutCancelNotificationToken    = 0;
static int                      gLWLogoutPointOfNoReturnNotificationToken = 0;
static CFStringRef              gConsoleNotifyKey                   = NULL;
static bool                     gDisplayIsAsleep = true;
static bool                     gUserWasActive = true;
static CFAbsoluteTime           gSleepFromUserWakeTime = 0;
static struct timeval           gLastSleepTime                      = {0, 0};
static dispatch_source_t        gDWTMsgDispatch; /* Darkwake thermal emergency message handler dispatch */

static mach_port_t              serverPort                          = MACH_PORT_NULL;
__private_extern__ CFMachPortRef pmServerMachPort                   = NULL;
#if !TARGET_OS_EMBEDDED
static bool                     gSMCSupportsWakeupTimer             = true;
#endif

#if TCPKEEPALIVE
extern TCPKeepAliveStruct           *gTCPKeepAlive;
#endif

// defined by MiG
extern boolean_t powermanagement_server(mach_msg_header_t *, mach_msg_header_t *);


// foward declarations
static void initializeESPrefsDynamicStore(void);
static void initializePowerSourceChangeNotification(void);
static void initializeInterestNotifications(void);
static void initializeTimezoneChangeNotifications(void);
static void initializeCalendarResyncNotification(void);
static void initializeShutdownNotifications(void);
static void initializeRootDomainInterestNotifications(void);
#if !TARGET_OS_EMBEDDED
static void initializeUserNotifications(void);
static void enableSleepWakeWdog();
#endif
static void initializeSleepWakeNotifications(void);

static void SleepWakeCallback(void *,io_service_t, natural_t, void *);
static void ESPrefsHaveChanged(
                SCPreferencesRef prefs,
                SCPreferencesNotification notificationType,
                void *info);
static void _ioupsd_exited(pid_t, int, struct rusage *, void *);
static void UPSDeviceAdded(void *, io_iterator_t);
static void BatteryMatch(void *, io_iterator_t);
static void BatteryInterest(void *, io_service_t, natural_t, void *);
static void RootDomainInterest(void *, io_service_t, natural_t, void *);
extern void PowerSourcesHaveChanged(void *info);
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

static void displayMatched(void *, io_iterator_t);
static void displayPowerStateChange(
                void *ref, 
                io_service_t service, 
                natural_t messageType, 
                void *arg);

static boolean_t pm_mig_demux(
                mach_msg_header_t * request,
                mach_msg_header_t * reply);

static void mig_server_callback(
                CFMachPortRef port, 
                void *msg, 
                CFIndex size, 
                void *info);

kern_return_t _io_pm_set_active_profile(
                mach_port_t         server,
                audit_token_t       token,
                vm_offset_t         profiles_ptr,
                mach_msg_type_number_t    profiles_len,
                int                 *result);

kern_return_t _io_pm_last_wake_time(
                mach_port_t             server,
                vm_offset_t             *out_wake_data,
                mach_msg_type_number_t  *out_wake_len,
                vm_offset_t             *out_delta_data,
                mach_msg_type_number_t  *out_delta_len,
                int                     *return_val);


static CFStringRef              
serverMPCopyDescription(const void *info)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<IOKit Power Management MIG server>"));
}

// Callback is registered in PrivateLib.c
__private_extern__ void dynamicStoreNotifyCallBack(
                SCDynamicStoreRef   store,
                CFArrayRef          changedKeys,
                void                *info);


/* load
 *
 * configd entry point
 */


int main(int argc __unused, char *argv[] __unused)
{
    CFRunLoopSourceRef      cfmp_rls = 0;
    CFMachPortContext       context  = { 0, (void *)1, NULL, NULL, serverMPCopyDescription };
    kern_return_t           kern_result = 0;

    kern_result = bootstrap_check_in(
                            bootstrap_port, 
                            kIOPMServerBootstrapName,
                            &serverPort);

#if TARGET_OS_EMBEDDED
    if (BOOTSTRAP_SUCCESS != kern_result) {
        kern_result = mach_port_allocate(
                                mach_task_self(), 
                                MACH_PORT_RIGHT_RECEIVE, 
                                &serverPort);

        if (KERN_SUCCESS == kern_result) {
            kern_result = mach_port_insert_right(
                                mach_task_self(), 
                                serverPort, serverPort, 
                                MACH_MSG_TYPE_MAKE_SEND);
        }
    
        if (KERN_SUCCESS == kern_result) {
            kern_result = bootstrap_register(
                                bootstrap_port, 
                                kIOPMServerBootstrapName, 
                                serverPort);
        }
    }
#endif

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
    
    initializePowerSourceChangeNotification();
    initializeESPrefsDynamicStore();
    initializeInterestNotifications();
    initializeTimezoneChangeNotifications();
    initializeCalendarResyncNotification();    
    initializeShutdownNotifications();
    initializeRootDomainInterestNotifications();
    
#if !TARGET_OS_EMBEDDED
    initializeUserNotifications();
    _oneOffHacksSetup();
#endif
    
    initializeSleepWakeNotifications();

    // Prime the messagetracer UUID pump
    pushNewSleepWakeUUID();
    
#if TCPKEEPALIVE
    Platform_prime();
#endif
    BatteryTimeRemaining_prime();
    PMSettings_prime();
    AutoWake_prime();
    PMAssertions_prime();
    PMSystemEvents_prime();
    SystemLoad_prime();
    PMConnection_prime();

#if !TARGET_OS_EMBEDDED
    PSLowPower_prime();
    TTYKeepAwake_prime();
    ExternalMedia_prime();

    createOnBootAssertions();
    enableSleepWakeWdog();
#endif

    _unclamp_silent_running(false);
    notify_post(kIOUserAssertionReSync);
    logASLMessagePMStart();
    CFRunLoopRun();
    return 0;
}




static void BatteryMatch(
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
                            kIOGeneralInterest, BatteryInterest,
                            (void *)tracking, &notification_ref);

        LogObjectRetainCount("PM::BatteryMatch(M1) me", battery);
        LogObjectRetainCount("PM::BatteryMatch(M1) msg_port", notification_ref);

        tracking->msg_port = notification_ref;
        IOObjectRelease(battery);
    }
    InternalEvaluateAssertions();
    InternalEvalConnections();
}


static void BatteryInterest(
    void *refcon, 
    io_service_t batt, 
    natural_t messageType, 
    void *messageArgument)
{
    IOPMBattery         *changed_batt = (IOPMBattery *)refcon;
    IOPMBattery         **batt_stats;

    if(kIOPMMessageBatteryStatusHasChanged == messageType)
    {
        // Update the arbiter
        changed_batt->me = (io_registry_entry_t)batt;
        _batteryChanged(changed_batt);

        LogObjectRetainCount("PM:BatteryInterest(B0) msg_port", changed_batt->msg_port);
        LogObjectRetainCount("PM:BatteryInterest(B1) msg_port", changed_batt->me);

        batt_stats = _batteries();        
        BatteryTimeRemainingBatteriesHaveChanged(batt_stats);
        SystemLoadBatteriesHaveChanged(batt_stats);
        InternalEvaluateAssertions();
        InternalEvalConnections();
    }

    if (kIOMessageServiceIsTerminated == messageType
        && changed_batt && IO_OBJECT_NULL != changed_batt->msg_port)
    {
        LogObjectRetainCount("PM:BatteryInterest(T0) msg_port", changed_batt->msg_port);
        LogObjectRetainCount("PM:BatteryInterest(T0) me", changed_batt->me);

// TODO: Clean up left over objects upon kext removal
//        IOObjectRelease(changed_batt->msg_port);
//        IOObjectRelease(changed_batt->me);

        LogObjectRetainCount("PM:BatteryInterest(T1) msg_port", changed_batt->msg_port);
        LogObjectRetainCount("PM:BatteryInterest(T1) me", changed_batt->me);
    }

    return;
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
#if !TARGET_OS_EMBEDDED
        // write SMC Key to re-enable SMC timer
        _smcWakeTimerPrimer();
#endif

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
    PMSettingsSleepWakeNotification(messageType);

    // Log Message to MessageTracer

    // Acknowledge message
    switch ( messageType ) {
        case kIOMessageSystemWillSleep:
            if (gUserWasActive) {
                gSleepFromUserWakeTime = CFAbsoluteTimeGetCurrent();
                gUserWasActive = false;
            }
            // Fall thru
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange(_pm_ack_port, (long)acknowledgementToken);
            break;
        case kIOMessageSystemHasPoweredOn:
        default:
            break;
    }
}

/* ESPrefsHaveChanged
 *
 * Is the handler that configd calls when someone "applies" new Energy Saver
 * Preferences. Since the preferences have probably changed, we re-read them
 * from disk and transmit the new settings to the kernel.
 */
static void 
ESPrefsHaveChanged(
    SCPreferencesRef prefs,
    SCPreferencesNotification notificationType,
    void *info)
{
    if ((kSCPreferencesNotificationCommit & notificationType) == 0)
        return;

    if (gESPreferences == prefs)
    {
        // Tell ES Prefs listeners that the prefs have changed
        PMSettingsPrefsHaveChanged();
        mt2EvaluateSystemSupport();
#if !TARGET_OS_EMBEDDED
        PSLowPowerPrefsHaveChanged();
        TTYKeepAwakePrefsHaveChanged();
#endif
        SystemLoadPrefsHaveChanged();
    }

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
    if(0 != status)
    {
        // ioupsd didn't exit cleanly.
        // Intentionally leave _alreadyRunningIOUPSD set so that we don't 
        // re-launch it.
        syslog(
            LOG_ERR, 
           "PowerManagement: /usr/libexec/ioupsd(%d) has exited with status %d\n", 
           pid, 
           status);
    } else {
        _alreadyRunningIOUPSD = 0;
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


/* PowerSourcesHaveChanged
 *
 * Is the handler that gets notified when power source (battery or UPS)
 * state changes. We might respond to this by posting a user notification
 * or performing emergency sleep/shutdown.
 */
extern void
PowerSourcesHaveChanged(void *info) 
{
    CFTypeRef            ps_blob;
    
    ps_blob = IOPSCopyPowerSourcesInfo();
    if(!ps_blob)
        return;

#if !TARGET_OS_EMBEDDED
    // Notifiy PSLowPower of power sources change
    PSLowPowerPSChange(ps_blob);
#endif
    
    // Notify PMSettings
    PMSettingsPSChange(ps_blob);
    
    CFRelease(ps_blob);
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
    if(!tzr) return;

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
    } else if (header->msgh_id == gLWShutdownNotificationToken) 
    {
        // Loginwindow put a shutdown confirm panel up on screen
        // The user has not necessarily even clicked on it yet
        amidst_shutdown = true;        
        consoleShutdownState = kIOPMStateConsoleShutdownPossible;

    } else if (header->msgh_id == gLWRestartNotificationToken) 
    {
        // Loginwindow put a restart confirm panel up on screen
        // The user has not necessarily even clicked on it yet
        amidst_shutdown = true;
        consoleShutdownState = kIOPMStateConsoleShutdownPossible;

    } else if (header->msgh_id == gLWLogoutCancelNotificationToken) 
    {
        // Whatever shutdown, restart, or logout that was in progress has been cancelled.
        amidst_shutdown = false;
        consoleShutdownState = kIOPMStateConsoleShutdownNone;

    } else if (amidst_shutdown 
            && (header->msgh_id == gLWLogoutPointOfNoReturnNotificationToken))
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

    switch (messageType)
    {
            // Display Wrangler power stateNumber values
            // 4 Display ON
            // 3 Display Dim
            // 2 Display Sleep
            // 1 Not visible to user
            // 0 Not visible to user

        case kIOMessageDeviceWillPowerOff:
            if ( params->stateNumber != 4 )
            {
                gDisplayIsAsleep = true;
            }

            if ( params->stateNumber <= 1)
            {
               // Notify a SystemLoad state change when display is completely off
                SystemLoadDisplayPowerStateHasChanged(gDisplayIsAsleep);
                // Display is transition from dim to full sleep.
                broadcastGMTOffset();            
            }
            break;
            
        case kIOMessageDeviceHasPoweredOn:
            if ( params->stateNumber == 4 )
            {
                gDisplayIsAsleep = false;            
                SystemLoadDisplayPowerStateHasChanged(gDisplayIsAsleep);
            }

            break;
    }
}

__private_extern__ bool isDisplayAsleep( )
{
    return gDisplayIsAsleep;
}

/* initializeESPrefsDynamicStore
 *
 * Registers a handler that configd calls when someone changes com.apple.PowerManagement.xml
 */
static void
initializeESPrefsDynamicStore(void)
{
    gESPreferences = SCPreferencesCreate(
                    kCFAllocatorDefault,
                    CFSTR("com.apple.configd.powermanagement"),
                    CFSTR(kIOPMPrefsPath));

    if (gESPreferences)
    {
        SCPreferencesSetCallback(
                    gESPreferences,
                    (SCPreferencesCallBack)ESPrefsHaveChanged,
                    (SCPreferencesContext *)NULL);

        SCPreferencesScheduleWithRunLoop(
                    gESPreferences,
                    CFRunLoopGetCurrent(),
                    kCFRunLoopDefaultMode);
    }
    
    return;
}

/* initializePowerSourceChanges
 *
 * Registers a handler that gets called on power source (battery or UPS) changes
 */
static void
initializePowerSourceChangeNotification(void)
{
    CFRunLoopSourceRef         CFrls;
        
    // Create and add RunLoopSource
    CFrls = IOPSNotificationCreateRunLoopSource(PowerSourcesHaveChanged, NULL);
    if(CFrls) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), CFrls, kCFRunLoopDefaultMode);    
        CFRelease(CFrls);
    }
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

static boolean_t 
pm_mig_demux(
    mach_msg_header_t * request,
    mach_msg_header_t * reply)
{
    mach_dead_name_notification_t *deadRequest = 
                    (mach_dead_name_notification_t *)request;
    boolean_t processed = FALSE;

    processed = powermanagement_server(request, reply);

    if (processed) 
        return true;
    
    if (MACH_NOTIFY_DEAD_NAME == request->msgh_id) 
    {
        bool handled = false;

        __MACH_PORT_DEBUG(true, "pm_mig_demux: Dead name port should have 1+ send right(s)", deadRequest->not_port);

        handled = BatteryHandleDeadName(deadRequest->not_port);
        if (!handled) {
            PMConnectionHandleDeadName(deadRequest->not_port);
        }
        
        __MACH_PORT_DEBUG(true, "pm_mig_demux: Deallocating dead name port", deadRequest->not_port);
        mach_port_deallocate(mach_task_self(), deadRequest->not_port);
        
        reply->msgh_bits            = 0;
        reply->msgh_remote_port     = MACH_PORT_NULL;

        return TRUE;
    }

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
    int                 ret = 0;
    uint64_t            token;

    __MACH_PORT_DEBUG(true, "mig_server_callback", serverPort);
    
    ret = proc_importance_assertion_begin_with_msg(&bufRequest->Head, NULL, &token);
    
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
    if (ret == 0)
        proc_importance_assertion_complete(token);
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
#if !TARGET_OS_EMBEDDED
        SystemLoadUserStateHasChanged();
#endif
    }
    
    return;
}

kern_return_t _io_pm_set_value_int(
    mach_port_t   server,
    audit_token_t token,
    int           selector,
    int           inValue)
{
    uid_t   callerUID;
    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, 0, 0, NULL, NULL);
    
    if (kIOPMSetNoPoll == selector)
    {
        BatterySetNoPoll(inValue ? true:false);
    }
#if TCPKEEPALIVE
    if ((kIOPMTCPKeepAliveExpirationOverride == selector)
        && auditTokenHasEntitlement(token, kIOPMDarkWakeControlEntitlement))
    {
        if (gTCPKeepAlive) {
            gTCPKeepAlive->overrideSec = inValue;
        }
    }
    else if ((kIOPMTCPWakeQuotaInterval == selector)
        && auditTokenHasEntitlement(token, kIOPMDarkWakeControlEntitlement))
    {
        TCPKeepAlive_adjustQuota(0, (CFTimeInterval)inValue);
    }
    else if ((kIOPMTCPWakeQuota == selector)
        && auditTokenHasEntitlement(token, kIOPMDarkWakeControlEntitlement))
    {
        TCPKeepAlive_adjustQuota(inValue, 0.0);
    }

#endif
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
#if !TARGET_OS_EMBEDDED
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
            if (gTCPKeepAlive) {
                *outValue = gTCPKeepAlive->overrideSec;
            }
        break;
            
    case kIOPMTCPKeepAliveIsActive:
            if (gTCPKeepAlive) {
                *outValue = getTCPKeepAliveIsActive(NULL, 0);
            }
            break;
            
    case kIOPMTCPWakeQuotaInterval:
            if (gTCPKeepAlive) {
                *outValue = gTCPKeepAlive->wakeQuotaInterval;
            }
            break;

    case kIOPMTCPWakeQuota:
            if (gTCPKeepAlive) {
                *outValue = gTCPKeepAlive->wakeQuota;
            }
            break;
#endif
            
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

kern_return_t _io_pm_set_active_profile(
    mach_port_t         server,
    audit_token_t       token,
    vm_offset_t         profiles_ptr,
    mach_msg_type_number_t    profiles_len,
    int                 *result)
{
    void                *profiles_buf = (void *)profiles_ptr;
    CFDictionaryRef     power_profiles = NULL;
    uid_t               callerUID;
    gid_t               callerGID;
    
    audit_token_to_au32(token, NULL, &callerUID, &callerGID, NULL, NULL, NULL, NULL, NULL);
    
    power_profiles = (CFDictionaryRef)IOCFUnserialize(profiles_buf, 0, 0, 0);
    if(isA_CFDictionary(power_profiles)) {
        *result = _IOPMSetActivePowerProfilesRequiresRoot(power_profiles, callerUID, callerGID);
        CFRelease(power_profiles);
    } else if(power_profiles) {
        CFRelease(power_profiles);
    }

    // deallocate client's memory
    vm_deallocate(mach_task_self(), (vm_address_t)profiles_ptr, profiles_len);

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
    
    CFMutableDictionaryRef      matchingDict = 0;
    CFMutableDictionaryRef      propertyDict = 0;
    kern_return_t               kr;
    
    /* Notifier */
    notify_port = IONotificationPortCreate(0);
    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(!rlser) goto finish;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);


    kr = IOServiceAddMatchingNotification(
                                notify_port,
                                kIOFirstMatchNotification,
                                IOServiceMatching("IOPMPowerSource"),
                                BatteryMatch,
                                (void *)notify_port,
                                &battery_iter);
    if(KERN_SUCCESS == kr)
    {
        // Install notifications on existing instances.
        BatteryMatch((void *)notify_port, battery_iter);
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
    

    matchingDict = IOServiceMatching(kIOServiceClass); 
    if (!matchingDict) goto finish;
            
    propertyDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
                    0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!propertyDict) goto finish;        

    // We are only interested in devices that have kIOUPSDeviceKey property set
    CFDictionarySetValue(propertyDict, CFSTR(kIOUPSDeviceKey), kCFBooleanTrue);    
    CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyDict);
    
    // Now set up a notification to be called when a device is first matched by I/O Kit.
    // Note that this will not catch any devices that were already plugged in so we take
    // care of those later.
    kr = IOServiceAddMatchingNotification(
                              notify_port, 
                              kIOFirstMatchNotification, 
                              matchingDict,
                              UPSDeviceAdded,
                              NULL,
                              &_ups_added_noteref);

    matchingDict = 0; // reference consumed by AddMatchingNotification
    if ( kr == kIOReturnSuccess )
    {
        // Check for existing matching devices and launch ioupsd if present.
        UPSDeviceAdded( NULL, _ups_added_noteref);        
    }
    
finish:   
    if(matchingDict) CFRelease(matchingDict);
    if(propertyDict) CFRelease(propertyDict);
    return;
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

#if TARGET_OS_EMBEDDED                                      
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
#if !TARGET_OS_EMBEDDED
    uint16_t            wakeup_smc_result = 0;
    IOReturn            ret = kIOReturnSuccess;
#endif
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
    
#if !TARGET_OS_EMBEDDED
    if (!gSMCSupportsWakeupTimer) {
        // This system's SMC doesn't support a wakeup time, so we're done
        goto exit;
    }
    
    // Read SMC key for precise timing between when the wake event physically occurred
    // and now (i.e. the moment we read the key).
    // - SMC key returns the delta in tens of milliseconds
    ret = _smcWakeTimerGetResults(&wakeup_smc_result);
    if ((ret != kIOReturnSuccess) || (wakeup_smc_result == 0))
    {
        if (kIOReturnNotFound == ret) {
            gSMCSupportsWakeupTimer = false;
        }
        goto exit;
    }
    // re-sample the current time closer to the SMC key read
    *gLastWakeTime = CFAbsoluteTimeGetCurrent();
    
    // convert 10x msecs to (double)seconds
    *gLastSMCS3S0WakeInterval = ((double)wakeup_smc_result / 100.0);
    
    // And we adjust backwards to determine the real time of physical wake.
    *gLastWakeTime -= *gLastSMCS3S0WakeInterval;
#endif
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

#if !TARGET_OS_EMBEDDED
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
    
    if((wrangler = (io_registry_entry_t)IOIteratorNext(iter))) 
    {        
        IOServiceAddInterestNotification(
                    note_port, 
                    wrangler, 
                    kIOGeneralInterest, 
                    displayPowerStateChange,
                    NULL, 
                    &dimming_notification_object);

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
                        &gLWShutdownNotificationToken);

    notify_register_mach_port( 
                        kLWRestartInitiated, 
                        &our_port, 
                        NOTIFY_REUSE, /* flags */ 
                        &gLWRestartNotificationToken);

    notify_register_mach_port( 
                        kLWLogoutCancelled, 
                        &our_port, 
                        NOTIFY_REUSE, /* flags */ 
                        &gLWLogoutCancelNotificationToken);

    notify_register_mach_port( 
                        kLWLogoutPointOfNoReturn, 
                        &our_port, 
                        NOTIFY_REUSE, /* flags */ 
                        &gLWLogoutPointOfNoReturnNotificationToken);

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
        getCFWakeReason(NULL, &wakeType);

    if ( (isA_BTMtnceWake() || isA_SleepSrvcWake()) && !isA_NotificationDisplayWake() && 
               (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) || 
                        CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService))
#if TCPKEEPALIVE
            && !(getTCPKeepAliveIsActive(NULL, 0) && checkForActivesByType(kInteractivePushServiceIndex)) ) {
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
    CFBooleanRef    userIsActive = NULL;
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

#if !TARGET_OS_EMBEDDED
    if(messageType == kIOPMMessageDarkWakeThermalEmergency)
    {
        mt2RecordThermalEvent(kThermalStateSleepRequest);
        _darkWakeThermalEventCount++;

        getCFWakeReason(&wakeReason, &wakeType);
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
                        ^{ logASLAssertionTypeSummary(kInteractivePushServiceIndex);});
            CFRunLoopWakeUp(_getPMRunLoop());
        }

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

    if (messageType == kIOPMMessageUserIsActiveChanged)
    {
        userIsActive = IORegistryEntryCreateCFProperty(getRootDomain(), CFSTR(kIOPMUserIsActiveKey), 0, 0);
        if (userIsActive == kCFBooleanTrue) {
            _unclamp_silent_running(true);
            cancel_NotificationDisplayWake();
            cancelPowerNapStates();
#if TCPKEEPALIVE
            TCPWakeQuotaRecordWake(kIsUserWake);
#endif
            gUserWasActive = true;
        }
        if (userIsActive) {
            CFRelease(userIsActive);
        }
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

#if !TARGET_OS_EMBEDDED
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
    if (IOAuthenticatedRestartSupported() == false) 
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
 
    if ( _pm_ack_port != MACH_PORT_NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }
}


__private_extern__ bool isA_userSession()
{
    return gUserWasActive ? true : false;
}

__private_extern__ CFAbsoluteTime get_SleepFromUserWakeTime()
{
    return gSleepFromUserWakeTime;
}
