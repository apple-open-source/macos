/*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
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
 
#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCPreferences.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <syslog.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <notify.h> 

#include "powermanagementServer.h" // mig generated

#include "PMSettings.h"
#include "PSLowPower.h"
#include "BatteryTimeRemaining.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "SetActive.h"
#include "PrivateLib.h"
#include "TTYKeepAwake.h"

#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath        "com.apple.PowerManagement.xml"

#ifndef kIOUPSDeviceKey
// Also defined in ioupsd/IOUPSPrivate.h
#define kIOUPSDeviceKey                 "UPSDevice" 
#endif

/*
 * BSD notifications from loginwindow indicating shutdown
 */
// kLWShutdownInitiated
//   User clicked shutdown: may be aborted later
#define kLWShutdowntInitiated    "com.apple.loginwindow.shutdownInitiated"

// kLWRestartInitiated
//   User clicked restart: may be aborted later
#define kLWRestartInitiated     "com.apple.loginwindow.restartinitiated"

// kLWLogoutCancelled
//   A previously initiated shutdown, restart, or logout, has been cancelled.
#define kLWLogoutCancelled      "com.apple.loginwindow.logoutcancelled"

// kLWLogoutPointOfNoReturn
//   A previously initiated shutdown, restart, or logout has succeeded, and is 
//   no longer abortable by anyone. Point of no return!
#define kLWLogoutPointOfNoReturn    "com.apple.loginwindow.logoutNoReturn"


// Global keys
static CFStringRef              gTZNotificationNameString   = NULL;

static SCPreferencesRef         gESPreferences              = NULL;
static SCPreferencesRef         gAutoWakePreferences        = NULL;

static io_connect_t             _pm_ack_port                = 0;
static io_iterator_t            _ups_added_noteref          = 0;
static int                      _alreadyRunningIOUPSD       = 0;
static int                      gClientUID                  = -1;
static int                      gClientGID                  = -1;

static int                      gLWShutdownNotificationToken        = 0;
static int                      gLWRestartNotificationToken         = 0;
static int                      gLWLogoutCancelNotificationToken    = 0;
static int                      gLWLogoutPointOfNoReturnNotificationToken = 0;

CFMachPortRef                   serverMachPort              = NULL;

// defined by MiG
extern boolean_t powermanagement_server(mach_msg_header_t *, mach_msg_header_t *);

// external
__private_extern__ void cleanupAssertions(mach_port_t dead_port);

// foward declarations
static void initializeESPrefsDynamicStore(void);
static void initializePowerSourceChangeNotification(void);
static void initializeMIGServer(void);
static void initializeInterestNotifications(void);
static void initializeTimezoneChangeNotifications(void);
static void initializeDisplaySleepNotifications(void);
static void initializeShutdownNotifications(void);
static void initializeRootDomainInterestNotifications(void);

static void SleepWakeCallback(void *,io_service_t, natural_t, void *);
static void ESPrefsHaveChanged(
                SCPreferencesRef prefs,
                SCPreferencesNotification notificationType,
                void *info);
static void _ioupsd_exited(pid_t, int, struct rusage *, void *);
static void UPSDeviceAdded(void *, io_iterator_t);
static void BatteryMatch(void *, io_iterator_t);
static void BatteryInterest(void *, io_service_t, natural_t, void *);
#if __ppc__
static void PMUInterest(void *, io_service_t, natural_t, void *);
#endif
static void RootDomainInterest(void *, io_service_t, natural_t, void *);
extern void PowerSourcesHaveChanged(void *info);
static void broadcastGMTOffset(void);

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
                vm_offset_t         profiles_ptr,
                mach_msg_type_number_t    profiles_len,
                int                 *result);


/* prime
 *
 * configd entry point
 */
void prime()
{    
    // Initialize everything.
    
    BatteryTimeRemaining_prime();
    PMSettings_prime();
    PSLowPower_prime();
    AutoWake_prime();
    RepeatingAutoWake_prime();
    PMAssertions_prime();
    TTYKeepAwake_prime();
        
    return;
}

/* load
 *
 * configd entry point
 */
void load(CFBundleRef bundle, Boolean bundleVerbose)
{
    IONotificationPortRef           notify;
    io_object_t                     anIterator;

    // Install notification on Power Source changes
    initializePowerSourceChangeNotification();

    // Install notification when the preferences file changes on disk
    initializeESPrefsDynamicStore();

    // Install notification on ApplePMU&IOPMrootDomain general interest messages
    initializeInterestNotifications();

    // Initialize MIG
    initializeMIGServer();
    
    // Initialize timezone changed notifications
    initializeTimezoneChangeNotifications();
    
    // Register for display dim/undim notifications
    initializeDisplaySleepNotifications();
    
    // Register for loginwindow's shutdown/restart panel initiated notifications
    initializeShutdownNotifications();
  
    // Register for interest notifications when PM features are published or removed
    initializeRootDomainInterestNotifications();
    
    // Register for SystemPower notifications
    _pm_ack_port = IORegisterForSystemPower (0, &notify, SleepWakeCallback, &anIterator);
    if ( _pm_ack_port != MACH_PORT_NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }

    // And we'll try to keep all the ugliest hacks off in their own little corner...
    _oneOffHacksSetup();

    return;
}



#if __ppc__
/* PMUInterest
 *
 * Receives and distributes messages from the PMU driver
 * These include legacy AutoWake requests and battery change notifications.
 */
static void 
PMUInterest(void *refcon, io_service_t service, natural_t messageType, void *arg)
{    
    // Tell the AutoWake handler
    if((kIOPMUMessageLegacyAutoWake == messageType) ||
       (kIOPMUMessageLegacyAutoPower == messageType) )
        AutoWakePMUInterestNotification(messageType, (UInt32)arg);
}
#endif

static void BatteryMatch(
    void *refcon, 
    io_iterator_t b_iter) 
{
    IOReturn                    ret;
    IOPMBattery                 *tracking;
    IONotificationPortRef       notify = (IONotificationPortRef)refcon;
    io_registry_entry_t         battery;
    io_object_t                 notification_ref;
    
    while((battery = (io_registry_entry_t)IOIteratorNext(b_iter))) 
    {
        // Add battery to our list of batteries
        tracking = _newBatteryFound(battery);
        
        // And install an interest notification on it
        ret = IOServiceAddInterestNotification(notify, battery, 
                            kIOGeneralInterest, BatteryInterest,
                            (void *)tracking, &notification_ref);

        tracking->msg_port = notification_ref;
        IOObjectRelease(battery);
    }
}


static void BatteryInterest(
    void *refcon, 
    io_service_t batt, 
    natural_t messageType, 
    void *messageArgument)
{
    CFArrayRef          battery_info = NULL;
    IOPMBattery         *changed_batt = (IOPMBattery *)refcon;
    IOPMBattery         **batt_stats;

    if(kIOPMMessageBatteryStatusHasChanged == messageType)
    {
        // Update the arbiter
        changed_batt->me = (io_registry_entry_t)batt;
        _batteryChanged(changed_batt);


        // Pass control over to PMUBattery for battery calculation
        batt_stats = _batteries();        
        BatteryTimeRemainingBatteriesHaveChanged(batt_stats);


        // Get legacy battery info & pass control over to PMSettings
        battery_info = isA_CFArray(_copyLegacyBatteryInfo());
        if(!battery_info) return;
        PMSettingsBatteriesHaveChanged(battery_info);

        CFRelease(battery_info);
    }

    return;
}


/* SleepWakeCallback
 * 
 * Receives notifications on system sleep and system wake.
 */
static void
SleepWakeCallback(void * port,io_service_t y,natural_t messageType,void * messageArgument)
{
    bool cancel_sleep = false;

    // Notify BatteryTimeRemaining
    BatteryTimeRemainingSleepWakeNotification(messageType);

    // Notify PMSettings
    PMSettingsSleepWakeNotification(messageType);
    
    // Notify AutoWake
    AutoWakeSleepWakeNotification(messageType);
    RepeatingAutoWakeSleepWakeNotification(messageType);

    // Notify & potentially cancel sleep by open ssh connections
    cancel_sleep = TTYKeepAwakeSleepWakeNotification(messageType);

    switch ( messageType ) {
    case kIOMessageSystemWillSleep:
        broadcastGMTOffset(); // tell clients what our timezone offset is
        IOAllowPowerChange(_pm_ack_port, (long)messageArgument);
        break;
    case kIOMessageCanSystemSleep:
        if ( cancel_sleep )
        {
            IOCancelPowerChange(_pm_ack_port, (long)messageArgument);
        } else {
            IOAllowPowerChange(_pm_ack_port, (long)messageArgument);
        }
        break;
        
    case kIOMessageSystemHasPoweredOn:
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
        PSLowPowerPrefsHaveChanged();
        TTYKeepAwakePrefsHaveChanged();
    }

    if (gAutoWakePreferences == prefs)
    {
        // Tell AutoWake listeners that the prefs have changed
        AutoWakePrefsHaveChanged();
        RepeatingAutoWakePrefsHaveChanged();
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
            pid_t           _ioupsd_pid;

            _alreadyRunningIOUPSD = 1;
            _ioupsd_pid = _SCDPluginExecCommand(&_ioupsd_exited, 0, 0, 0,
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
    
    ps_blob = isA_CFDictionary(IOPSCopyPowerSourcesInfo());
    if(!ps_blob) return;
    
    // Notifiy PSLowPower of power sources change
    PSLowPowerPSChange(ps_blob);
    
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
    static bool amidst_shutdown = false;

    if (header->msgh_id == gLWShutdownNotificationToken) 
    {
        // Loginwindow put a shutdown confirm panel up on screen
        // The user has not necessarily even clicked on it yet
        amidst_shutdown = true;
    } else if (header->msgh_id == gLWRestartNotificationToken) 
    {
        // Loginwindow put a restart confirm panel up on screen
        // The user has not necessarily even clicked on it yet
        amidst_shutdown = true;
    } else if (header->msgh_id == gLWLogoutCancelNotificationToken) 
    {
        // Whatever shutdown, restart, or logout that was in progress has been cancelled.
        amidst_shutdown = false;
    } else if (amidst_shutdown 
            && (header->msgh_id == gLWLogoutPointOfNoReturnNotificationToken))
    {
        // Whatever shutdown or restart that was in progress has succeeded.
        // All apps are quit, there's no more user input required. We will
        // hereby disable sleep for the remainder of time spent shutting down
        // this machine.

        _setRootDomainProperty(CFSTR("System Shutdown"), kCFBooleanTrue);
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
    static      int level = 0;
    switch (messageType)
    {
        case kIOMessageDeviceWillPowerOff:
            level++;
            if(2 == level) {
                // Display is transition from dim to full sleep.
                broadcastGMTOffset();            
            }
            break;
            
        case kIOMessageDeviceHasPoweredOn:
            level = 0;
            break;
    }
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

    gAutoWakePreferences = SCPreferencesCreate(
                    kCFAllocatorDefault,
                    CFSTR("com.apple.configd.powermanagement"),
                    CFSTR(kIOPMAutoWakePrefsPath));

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
    
    if (gAutoWakePreferences)
    {
        SCPreferencesSetCallback(
                    gAutoWakePreferences,
                    (SCPreferencesCallBack)ESPrefsHaveChanged,
                    (SCPreferencesContext *)NULL);

        SCPreferencesScheduleWithRunLoop(
                    gAutoWakePreferences,
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



static boolean_t 
pm_mig_demux(
    mach_msg_header_t * request,
    mach_msg_header_t * reply)
{
    mach_dead_name_notification_t *deadRequest = 
                    (mach_dead_name_notification_t *)request;
    boolean_t processed = FALSE;

    mach_msg_format_0_trailer_t * trailer;

    if ((request->msgh_id >= _powermanagement_subsystem.start &&
         request->msgh_id < _powermanagement_subsystem.end)) 
     {

        /*
         * Get the caller's credentials (eUID/eGID) from the message trailer.
         */
        trailer = (mach_msg_security_trailer_t *)((vm_offset_t)request +
            round_msg(request->msgh_size));

        if ((trailer->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) &&
           (trailer->msgh_trailer_size >= MACH_MSG_TRAILER_FORMAT_0_SIZE)) {

            gClientUID = trailer->msgh_sender.val[0];
            gClientGID = trailer->msgh_sender.val[1];
        } else {
            //kextd_error_log("caller's credentials not available");
            gClientUID = -1;
            gClientGID = -1;
        }
    }
    
    processed = powermanagement_server(request, reply);
    if(processed) return true;
    
    // Check for tasks which have exited and clean-up PM assertions
    if(MACH_NOTIFY_DEAD_NAME == request->msgh_id) 
    {
        cleanupAssertions(deadRequest->not_port);
        mach_port_deallocate(mach_task_self(), deadRequest->not_port);

        reply->msgh_bits        = 0;
        reply->msgh_remote_port    = MACH_PORT_NULL;
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
            CFAllocatorDeallocate(NULL, bufReply);
            return;
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
        CFAllocatorDeallocate(NULL, bufReply);
        return;
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
    if (MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) == MACH_MSG_TYPE_MOVE_SEND_ONCE) {
        options |= MACH_SEND_TIMEOUT;
    }
    mr = mach_msg(&bufReply->Head,        /* msg */
              options,            /* option */
              bufReply->Head.msgh_size,    /* send_size */
              0,            /* rcv_size */
              MACH_PORT_NULL,        /* rcv_name */
              MACH_MSG_TIMEOUT_NONE,    /* timeout */
              MACH_PORT_NULL);        /* notify */


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

    CFAllocatorDeallocate(NULL, bufReply);
}

kern_return_t _io_pm_force_active_settings(
    mach_port_t                 server,
    vm_offset_t                 settings_ptr,
    mach_msg_type_number_t      settings_len,
    int                         *result)
{
    void                    *settings_buf = (void *)settings_ptr;
    CFDictionaryRef         force_settings = NULL;

    /* requires root */
    if(0 != gClientUID) {
        *result = kIOReturnNotPrivileged;
    }
    
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

    // deallocate client's memory
    vm_deallocate(mach_task_self(), (vm_address_t)settings_ptr, settings_len);

    return KERN_SUCCESS;
}

kern_return_t _io_pm_set_active_profile(
                mach_port_t         server,
                vm_offset_t         profiles_ptr,
                mach_msg_type_number_t    profiles_len,
                int                 *result)
{
    void                    *profiles_buf = (void *)profiles_ptr;
    CFDictionaryRef         power_profiles = NULL;
    
    power_profiles = (CFDictionaryRef)IOCFUnserialize(profiles_buf, 0, 0, 0);
    if(isA_CFDictionary(power_profiles)) {
        *result = _IOPMSetActivePowerProfilesRequiresRoot(power_profiles, gClientUID, gClientGID);
        CFRelease(power_profiles);
    } else if(power_profiles) {
        CFRelease(power_profiles);
    }

    // deallocate client's memory
    vm_deallocate(mach_task_self(), (vm_address_t)profiles_ptr, profiles_len);

    return KERN_SUCCESS;
}


static bool _assertionRequiresRoot(CFStringRef   asst)
{
    if( CFEqual(asst, kIOPMInflowDisableAssertion)
       || CFEqual(asst, kIOPMChargeInhibitAssertion) )
    {
        return true;
    }
    
    return false;
}

kern_return_t _io_pm_assertion_create
(
    mach_port_t         server,
    mach_port_t         task,
    string_t            profile,
    mach_msg_type_number_t   profileCnt,
    int                 level,
    int                 *assertion_id,
    int                 *result
)
{
    CFStringRef     profileString = NULL;

    profileString = CFStringCreateWithCString(0, profile, 
                            kCFStringEncodingMacRoman);

    // Kick them out if this assertion requires root priviliges to run
    if( _assertionRequiresRoot(profileString) )
    {
        // uid & gid set in global vars in pm_mig_demux above
        if( ( !callerIsRoot(gClientUID, gClientGID) 
            && !callerIsAdmin(gClientUID, gClientGID) )
            || (-1 == gClientUID) || (-1 == gClientGID) )
        {
            *result = kIOReturnNotPrivileged;
            goto exit;    
        }
    }

    if(!profileString) goto exit;

    *result = _IOPMAssertionCreateRequiresRoot(task, profileString, 
                                                level, assertion_id);

    CFRelease(profileString); profileString = NULL;

exit:
    if(profileString) CFRelease(profileString);
    return KERN_SUCCESS;
}

static CFStringRef              
serverMPCopyDescription(const void *info)
{
        return CFStringCreateWithFormat(NULL, NULL, CFSTR("<io pm server MP>"));
}
 
static void
initializeMIGServer(void)
{
    kern_return_t           kern_result = 0;
    CFMachPortRef           cf_mach_port = 0;
    CFRunLoopSourceRef      cfmp_rls = 0;
    mach_port_t             our_port;

    CFMachPortContext       context  = { 0, (void *)1, NULL, NULL, serverMPCopyDescription };

    cf_mach_port = CFMachPortCreate(0, mig_server_callback, &context, 0);
    if(!cf_mach_port) {
        goto bail;
    }
    our_port = CFMachPortGetPort(cf_mach_port);
    cfmp_rls = CFMachPortCreateRunLoopSource(0, cf_mach_port, 0);
    if(!cfmp_rls) {
        goto bail;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), cfmp_rls, kCFRunLoopDefaultMode);

    kern_result = bootstrap_register(
                        bootstrap_port, 
                        kIOPMServerBootstrapName, 
                        our_port);

    switch (kern_result) {
      case BOOTSTRAP_SUCCESS:
        break;
      case BOOTSTRAP_NOT_PRIVILEGED:
        break;
      case BOOTSTRAP_SERVICE_ACTIVE:
        break;
      default:
        break;
    }

    serverMachPort = cf_mach_port;

bail:
    if(cfmp_rls) CFRelease(cfmp_rls);
    return;
}

/* initializeInteresteNotifications
 *
 * Sets up the notification of general interest from the PMU & RootDomain
 */
static void
initializeInterestNotifications()
{
    IONotificationPortRef       notify_port = 0;
    io_object_t                 notification_ref = 0;
    io_service_t                pmu_service_ref = 0;
    io_iterator_t               battery_iter = 0;
    CFRunLoopSourceRef          rlser = 0;
    IOReturn                    ret;
    
    CFMutableDictionaryRef      matchingDict = 0;
    CFMutableDictionaryRef      propertyDict = 0;
    kern_return_t               kr;
    
    /* Notifier */
    notify_port = IONotificationPortCreate(0);
    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(!rlser) goto finish;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);

#if __ppc__
    /* PMU */
    pmu_service_ref = IOServiceGetMatchingService(0, IOServiceNameMatching("ApplePMU"));
    if(!pmu_service_ref) goto battery;

    ret = IOServiceAddInterestNotification(notify_port, pmu_service_ref, 
                                kIOGeneralInterest, PMUInterest,
                                0, &notification_ref);
    if(kIOReturnSuccess != ret) goto battery;
#endif /* __ppc__ */

battery:

    // Get match notification on IOPMPowerSource
    kr = IOServiceAddMatchingNotification(
                                notify_port,
                                kIOFirstMatchNotification,
                                IOServiceMatching("IOPMPowerSource"),
                                BatteryMatch,
                                (void *)notify_port,
                                &battery_iter);
    if(KERN_SUCCESS != kr) goto ups;

    // Install notifications on existing instances.
    BatteryMatch((void *)notify_port, battery_iter);
    
ups:

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
    if ( kr != kIOReturnSuccess ) goto finish;

    // Check for existing matching devices and launch ioupsd if present.
    UPSDeviceAdded( NULL, _ups_added_noteref);        

finish:   
    if(rlser) CFRelease(rlser);
    if(pmu_service_ref) IOObjectRelease(pmu_service_ref);
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


/* intializeDisplaySleepNotifications
 *
 * Notifications on display sleep. We tell the tz+dst offset to our timezone 
 * clients when display sleep kicks in. 
 */
static void
initializeDisplaySleepNotifications(void)
{
    IONotificationPortRef       note_port = MACH_PORT_NULL;
    CFRunLoopSourceRef          dimSrc = NULL;
    io_service_t                display_wrangler = MACH_PORT_NULL;
    io_object_t                 dimming_notification_object = MACH_PORT_NULL;
    IOReturn                    ret;

    display_wrangler = IOServiceGetMatchingService(MACH_PORT_NULL, 
                                            IOServiceNameMatching("IODisplayWrangler"));
    if(!display_wrangler) return;
    
    note_port = IONotificationPortCreate(MACH_PORT_NULL);
    if(!note_port) goto exit;
    
    ret = IOServiceAddInterestNotification(note_port, display_wrangler, 
                kIOGeneralInterest, displayPowerStateChange,
                NULL, &dimming_notification_object);
    if (ret != kIOReturnSuccess) goto exit;
    
    dimSrc = IONotificationPortGetRunLoopSource(note_port);
    
    if (dimSrc)
    {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), dimSrc, kCFRunLoopDefaultMode);
    }
    
exit:
    // Do not release dimming_notification_object, would uninstall notification
    // Do not release dimSrc because it's 'owned' by note_port, and both
    // must be kept around to receive this notification
    if(MACH_PORT_NULL != display_wrangler) IOObjectRelease(display_wrangler);
}

static void 
initializeShutdownNotifications(void)
{
    CFMachPortRef       gNotifyMachPort = NULL;
    CFRunLoopSourceRef  gNotifyMachPortRLS = NULL;
    mach_port_t         our_port = MACH_PORT_NULL;
    int                 notify_return = NOTIFY_STATUS_OK;

    // Tell the kernel that we are NOT shutting down at the moment, since
    // configd is just launching now.
    // Why: if configd crashed with "System Shutdown" == kCFbooleanTrue, reset
    // it now as the situation may no longer apply.
    _setRootDomainProperty(CFSTR("System Shutdown"), kCFBooleanFalse);

    /* * * * * * * * * * * * * */
    
    notify_return = notify_register_mach_port( 
                        kLWShutdowntInitiated, 
                        &our_port, 
                        0, /* flags */ 
                        &gLWShutdownNotificationToken);

    notify_return = notify_register_mach_port( 
                        kLWRestartInitiated, 
                        &our_port, 
                        NOTIFY_REUSE, /* flags */ 
                        &gLWRestartNotificationToken);

    notify_return = notify_register_mach_port( 
                        kLWLogoutCancelled, 
                        &our_port, 
                        NOTIFY_REUSE, /* flags */ 
                        &gLWLogoutCancelNotificationToken);

    notify_return = notify_register_mach_port( 
                        kLWLogoutPointOfNoReturn, 
                        &our_port, 
                        NOTIFY_REUSE, /* flags */ 
                        &gLWLogoutPointOfNoReturnNotificationToken);

    /* * * * * * * * * * * * * */

    gNotifyMachPort = CFMachPortCreateWithPort(
                                kCFAllocatorDefault,
                                our_port,
                                lwShutdownCallback,
                                NULL,  /* context */
                                NULL); /* &shouldFreeInfo */
    if (!gNotifyMachPort)
        return;
    
    // Create RLS for mach port
    gNotifyMachPortRLS = CFMachPortCreateRunLoopSource(
                                        kCFAllocatorDefault,
                                        gNotifyMachPort,
                                        0); /* order */
    if (!gNotifyMachPortRLS)
        return;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), 
            gNotifyMachPortRLS, kCFRunLoopDefaultMode);
}

static void 
RootDomainInterest(
    void *refcon, 
    io_service_t root_domain, 
    natural_t messageType, 
    void *messageArgument)
{
    if (messageType == kIOPMMessageFeatureChange)
    {
        // Let PMSettings code know that some settings may have been
        // added or removed.
    
        PMSettingsSupportedPrefsListHasChanged();
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
        CFRelease(runLoopSrc);
    }
    
exit:
    // Do not release notification_object, would uninstall notification
    // Do not release runLoopSrc because it's 'owned' by note_port, and both
    // must be kept around to receive this notification
    if(MACH_PORT_NULL != root_domain) IOObjectRelease(root_domain);
}

// use 'make' to build standalone debuggable executable 'pm'

#ifdef  STANDALONE
int
main(int argc, char **argv)
{
    openlog("pmcfgd", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);

    prime();

    CFRunLoopRun();

    /* not reached */
    exit(0);
    return 0;
}
#endif

