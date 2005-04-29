/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2001 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 18-Dec-01 ebold created
 *
 */
 
#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
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
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "powermanagementServer.h" // mig generated

#include "PMSettings.h"
#include "PSLowPower.h"
#include "BatteryTimeRemaining.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "SetActive.h"
#include "PrivateLib.h"

#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath        "com.apple.PowerManagement.xml"

#ifndef kIOUPSDeviceKey
// Also defined in ioupsd/IOUPSPrivate.h
#define kIOUPSDeviceKey                 "UPSDevice" 
#endif

// Global keys
static CFStringRef              EnergyPrefsKey              = NULL;
static CFStringRef              AutoWakePrefsKey            = NULL;
static CFStringRef              ConsoleUserKey              = NULL;

static io_service_t             gIOResourceService          = 0;
static io_connect_t             _pm_ack_port                = 0;
static io_iterator_t            _ups_added_noteref          = 0;
static int                      _alreadyRunningIOUPSD       = 0;
static int                      gClientUID                  = -1;
static int                      gClientGID                  = -1;

CFMachPortRef            serverMachPort              = NULL;


// defined by MiG
extern boolean_t powermanagement_server(mach_msg_header_t *, mach_msg_header_t *);

// external
__private_extern__ void cleanupAssertions(mach_port_t dead_port);

// foward declarations
static void tellSMU_GMTOffset(void);


/* PMUInterestNotification
 *
 * Receives and distributes messages from the PMU driver
 * These include legacy AutoWake requests and battery change notifications.
 */
static void 
PMUInterestNotification(void *refcon, io_service_t service, natural_t messageType, void *arg)
{    
    // Tell the AutoWake handler
    if((kIOPMUMessageLegacyAutoWake == messageType) ||
       (kIOPMUMessageLegacyAutoPower == messageType) )
        AutoWakePMUInterestNotification(messageType, (UInt32)arg);
}

/* RootDomainInterestNotification
 *
 * Receives and distributes messages from the IOPMrootDomain
 */
static void 
RootDomainInterestNotification(void *refcon, io_service_t service, natural_t messageType, void *arg)
{
    CFArrayRef          battery_info;

    // Tell battery calculation code that battery status has changed
    if(kIOPMMessageBatteryStatusHasChanged == messageType)
    {
        // get battery info
        battery_info = isA_CFArray(_copyBatteryInfo());
        if(!battery_info) return;

        // Pass control over to PMSettings
        PMSettingsBatteriesHaveChanged(battery_info);
        // Pass control over to PMUBattery for battery calculation
        BatteryTimeRemainingBatteriesHaveChanged(battery_info);
        
        CFRelease(battery_info);
    }
}

/* SleepWakeCallback
 * 
 * Receives notifications on system sleep and system wake.
 */
static void
SleepWakeCallback(void * port,io_service_t y,natural_t messageType,void * messageArgument)
{
    // Notify BatteryTimeRemaining
    BatteryTimeRemainingSleepWakeNotification(messageType);

    // Notify PMSettings
    PMSettingsSleepWakeNotification(messageType);
    
    // Notify AutoWake
    AutoWakeSleepWakeNotification(messageType);
    RepeatingAutoWakeSleepWakeNotification(messageType);

    switch ( messageType ) {
    case kIOMessageSystemWillSleep:
        tellSMU_GMTOffset(); // tell SMU what our timezone offset is
    case kIOMessageCanSystemSleep:
        IOAllowPowerChange(_pm_ack_port, (long)messageArgument);
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
ESPrefsHaveChanged(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info) 
{
    CFRange   key_range = CFRangeMake(0, CFArrayGetCount(changedKeys));

    if(CFArrayContainsValue(changedKeys, key_range, EnergyPrefsKey))
    {
        // Tell PMSettings that the prefs file has changed
        PMSettingsPrefsHaveChanged();
        PSLowPowerPrefsHaveChanged();
    }

    if(CFArrayContainsValue(changedKeys, key_range, AutoWakePrefsKey))
    {
        // Tell AutoWake that the prefs file has changed
        AutoWakePrefsHaveChanged();
        RepeatingAutoWakePrefsHaveChanged();
    }

    if(CFArrayContainsValue(changedKeys, key_range, ConsoleUserKey))
    {
        PMSettingsConsoleUserHasChanged();
        
        CFArrayRef sessionList = SCDynamicStoreCopyConsoleInformation(store);
        if (!sessionList)
            sessionList = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
    
        if (sessionList)
        {
            IORegistryEntrySetCFProperty(gIOResourceService, CFSTR(kIOConsoleUsersKey), sessionList);
            CFRelease(sessionList);
        }
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
        // intentionally leave _alreadyRunningIOUPSD set so that we don't re-launch it.
        syslog(LOG_ERR, "PowerManagement: /usr/libexec/ioupsd(%d) has exited with status %d\n", pid, status);
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
        
    while ( upsDevice = IOIteratorNext(iterator) )
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


/* tellSMU_GMTOffset
 *
 * Tell the SMU what the seconds offset from GMT is.
 * Why does power management care which timezone we're in?
 * We don't, really. The SMU firmware needs to know for
 * a feature which shall remain nameless. Timezone info
 * is really only conveniently accessible from up here in 
 * user space, so we just keep track of it and tell PMU/SMU
 * whenever it changes. And this PM plugin was a vaguely
 * convenient place for this code to live.
 */
static void 
tellSMU_GMTOffset(void)
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
    // IOPMrootdomain will relay the message on to AppleSMU.kext or
    // ApplePMU.kext as appropriate for the system.
    _setRootDomainProperty(CFSTR("TimeZoneOffsetSeconds"), n);
exit:
    if(tzr) CFRelease(tzr);
    if(n) CFRelease(n);
}

/* displayPowerStateChange
 *
 * displayPowerStateChange gets notified when the display changes power state.
 * Power state changes look like this:
 * (1) Full power -> dim
 * (2) dim -> display sleep
 * (3) display sleep -> display sleep
 * 
 * We're interested in state transition 2. When that occurs on an SMU system
 * we'll tell the SMU what the system clock's offset from GMT is.
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
                tellSMU_GMTOffset();            
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
    CFRunLoopSourceRef          CFrls = NULL;
    CFMutableArrayRef           watched_keys = NULL;
    SCDynamicStoreRef           energyDS;

    watched_keys = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    if(!watched_keys) return;
    
    energyDS = SCDynamicStoreCreate(NULL, CFSTR(kIOPMAppName), &ESPrefsHaveChanged, NULL);
    if(!energyDS) return;

    // Setup notification for changes in Energy Saver prefences
    EnergyPrefsKey = SCDynamicStoreKeyCreatePreferences(
                                NULL, 
                                CFSTR(kIOPMPrefsPath), 
                                kSCPreferencesKeyApply);
    if(EnergyPrefsKey) 
        CFArrayAppendValue(watched_keys, EnergyPrefsKey);

    // Setup notification for changes in AutoWake prefences
    AutoWakePrefsKey = SCDynamicStoreKeyCreatePreferences(
                NULL, 
                CFSTR(kIOPMAutoWakePrefsPath), 
                kSCPreferencesKeyCommit);
    if(AutoWakePrefsKey) 
        CFArrayAppendValue(watched_keys, AutoWakePrefsKey);

    gIOResourceService = IORegistryEntryFromPath(
                MACH_PORT_NULL, 
                kIOServicePlane ":/" kIOResourcesClass);
    ConsoleUserKey = SCDynamicStoreKeyCreateConsoleUser( NULL /* CFAllocator */ );
    if(ConsoleUserKey && gIOResourceService) 
        CFArrayAppendValue(watched_keys, ConsoleUserKey);

    SCDynamicStoreSetNotificationKeys(energyDS, watched_keys, NULL);

    // Create and add RunLoopSource
    CFrls = SCDynamicStoreCreateRunLoopSource(NULL, energyDS, 0);
    if(CFrls) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), CFrls, kCFRunLoopDefaultMode);    
        CFRelease(CFrls);
    }

    CFRelease(watched_keys);
    CFRelease(energyDS);
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
    
    // Check for no more senders and clean-up PM assertions from
    // dead processes
    if(MACH_NOTIFY_DEAD_NAME == request->msgh_id) 
    {
        //syslog(LOG_ERR, "Power Management: A client (port = %d) has unexpectedly died; \
        //                cleaning up its outstanding assertions.\n", deadRequest->not_port); 
        cleanupAssertions(deadRequest->not_port);

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



__private_extern__ void
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


static void
initializeMIGServer(void)
{
    kern_return_t           kern_result = 0;
    CFMachPortRef           cf_mach_port = 0;
    CFRunLoopSourceRef      cfmp_rls = 0;
    mach_port_t             our_port;

    cf_mach_port = CFMachPortCreate(0, mig_server_callback, 0, 0);
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
        //syslog(LOG_INFO, "pmconfigd exit: undefined mig error");
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
    IONotificationPortRef       r_notify_port = 0;
    IONotificationPortRef       ups_notify_port = 0;
    io_object_t                 notification_ref = 0;
    io_service_t                pmu_service_ref = 0;
    io_service_t                root_domain_ref = 0;
    CFRunLoopSourceRef          rlser = 0;
    CFRunLoopSourceRef          r_rlser = 0;
    CFRunLoopSourceRef          ups_rlser = 0;
    IOReturn                    ret;
    
    CFMutableDictionaryRef      matchingDict = 0;
    CFMutableDictionaryRef      propertyDict = 0;
    kern_return_t               kr;
    

    // PMU
    pmu_service_ref = IOServiceGetMatchingService(0, IOServiceNameMatching("ApplePMU"));
    if(!pmu_service_ref) goto root_domain;

    notify_port = IONotificationPortCreate(0);
    ret = IOServiceAddInterestNotification(notify_port, pmu_service_ref, 
                                kIOGeneralInterest, PMUInterestNotification,
                                0, &notification_ref);
    if(kIOReturnSuccess != ret) goto root_domain;

    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(!rlser) goto root_domain;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);
    
    
    // ROOT_DOMAIN
root_domain:
    root_domain_ref = IOServiceGetMatchingService(0, IOServiceNameMatching("IOPMrootDomain"));
    if(!root_domain_ref) goto ups;

    r_notify_port = IONotificationPortCreate(0);
    ret = IOServiceAddInterestNotification(r_notify_port, root_domain_ref, 
                                kIOGeneralInterest, RootDomainInterestNotification,
                                0, &notification_ref);
    if(kIOReturnSuccess != ret) goto ups;

    r_rlser = IONotificationPortGetRunLoopSource(r_notify_port);
    if(!r_rlser) goto ups;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), r_rlser, kCFRunLoopDefaultMode);
    
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
    ups_notify_port = IONotificationPortCreate(0);
    kr = IOServiceAddMatchingNotification(ups_notify_port,            // notifyPort
                                          kIOFirstMatchNotification,    // notificationType
                                          matchingDict,            // matching
                                          UPSDeviceAdded,        // callback
                                          NULL,                // refCon
                                          &_ups_added_noteref            // notification
                                          );
    matchingDict = 0; // reference consumed by AddMatchingNotification
    if ( kr != kIOReturnSuccess ) goto finish;

    // Check for existing matching devices and launch ioupsd if present.
    UPSDeviceAdded( NULL, _ups_added_noteref );

    ups_rlser = IONotificationPortGetRunLoopSource(ups_notify_port);
    if(!ups_rlser) goto finish;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), ups_rlser, kCFRunLoopDefaultMode);
        

finish:
    if(rlser) CFRelease(rlser);
    if(r_rlser) CFRelease(r_rlser);
    if(ups_rlser) CFRelease(ups_rlser);
    if(notify_port) IOObjectRelease((io_object_t)notify_port);
    if(r_notify_port) IOObjectRelease((io_object_t)r_notify_port);
    if(ups_notify_port) IOObjectRelease((io_object_t)ups_notify_port);
    if(pmu_service_ref) IOObjectRelease(pmu_service_ref);
    if(root_domain_ref) IOObjectRelease(root_domain_ref);
    if(matchingDict) CFRelease(matchingDict);
    if(propertyDict) CFRelease(propertyDict);
    return;
}

static bool
systemHasSMU(void)
{
    static io_registry_entry_t              smuRegEntry = MACH_PORT_NULL;
    static bool                             known = false;

    if(known) return (smuRegEntry?true:false);

    smuRegEntry = (io_registry_entry_t)IOServiceGetMatchingService(0,
                        IOServiceNameMatching("AppleSMU"));
    if(MACH_PORT_NULL == smuRegEntry)
    {
        // SMU not supported on this platform, no need to install tz handler
        known = true;
        return false;
    }
    IOObjectRelease(smuRegEntry);
    known = true;
    return true;
}

/* intializeDisplaySleepNotifications
 *
 * Notifications on display sleep. Our only purpose for listening to these
 * is to tell the SMU what our timezone offset is when display sleep kicks
 * in. As such, we only install the notifications on machines with an SMU.
 */
static void
intializeDisplaySleepNotifications(void)
{
    IONotificationPortRef       note_port = MACH_PORT_NULL;
    CFRunLoopSourceRef          dimSrc = NULL;
    io_service_t                display_wrangler = MACH_PORT_NULL;
    io_object_t                 dimming_notification_object = MACH_PORT_NULL;
    IOReturn                    ret;

    if(!systemHasSMU()) return;

    display_wrangler = IOServiceGetMatchingService(NULL, IOServiceNameMatching("IODisplayWrangler"));
    if(!display_wrangler) return;
    
    note_port = IONotificationPortCreate(NULL);
    if(!note_port) goto exit;
    
    ret = IOServiceAddInterestNotification(note_port, display_wrangler, 
                kIOGeneralInterest, displayPowerStateChange,
                NULL, &dimming_notification_object);
    if(ret != kIOReturnSuccess) goto exit;
    
    dimSrc = IONotificationPortGetRunLoopSource(note_port);
    
    if(dimSrc)
    {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), dimSrc, kCFRunLoopDefaultMode);
    }
    
exit:
    // Do not release dimming_notification_object, would uninstall notification
    // Do not release dimSrc because it's 'owned' by note_port, and both
    // must be kept around to receive this notification
    if(MACH_PORT_NULL != display_wrangler) IOObjectRelease(display_wrangler);
}

void
prime()
{    
    // Initialize battery averaging code
    BatteryTimeRemaining_prime();
    
    // Initialize PMSettings code
    PMSettings_prime();
    
    // Initialize PSLowPower code
    PSLowPower_prime();

    // Initialzie AutoWake code
    AutoWake_prime();
    RepeatingAutoWake_prime();
    
    // initialize Assertions code
    PMAssertions_prime();
        
    return;
}

void
load(CFBundleRef bundle, Boolean bundleVerbose)
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
    
    // Register for display dim/undim notifications
    intializeDisplaySleepNotifications();
    
    // Register for SystemPower notifications
    _pm_ack_port = IORegisterForSystemPower (0, &notify, SleepWakeCallback, &anIterator);
    if ( _pm_ack_port != MACH_PORT_NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }

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

