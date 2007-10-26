/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
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
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <asl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "PrivateLib.h"
#include "PMSettings.h"
#include "SetActive.h"
#include "powermanagementServer.h"

#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath        "com.apple.PowerManagement.xml"

#define kMaxTaskAssertions          64
#define kIOPMTaskPortKey            CFSTR("task")
#define kIOPMAssertionsKey          CFSTR("assertions")


#define kIOPMNumAssertionTypes      6
enum {
    kHighPerfIndex          = 0,
    kPreventIdleIndex       = 1,
    kDisableInflowIndex     = 2,
    kInhibitChargeIndex     = 3,
    kDisableWarningsIndex   = 4,
    kPreventDisplaySleepIndex = 5,
    kEnableIdleIndex = 6
};

// Selectors for AppleSmartBatteryManagerUserClient
enum {
    kSBUCInflowDisable = 0,
    kSBUCChargeInhibit = 1
};


extern CFMachPortRef            serverMachPort;

// forward
__private_extern__ void cleanupAssertions(mach_port_t dead_port);
static void evaluateAssertions(void);
static void calculateAggregates(void);
static void sendSmartBatteryCommand(uint32_t which, uint32_t level);

// globals
static CFMutableDictionaryRef assertionsDict = NULL;
static int aggregate_assertions[kIOPMNumAssertionTypes];
static int last_aggregate_assertions[kIOPMNumAssertionTypes];
static CFStringRef assertion_types_arr[kIOPMNumAssertionTypes];

static bool idle_enable_assumed = true;

/***********************************
 * Static Profiles
 ***********************************/

static void
calculateAggregates(void)
{
    CFDictionaryRef         *process_assertions = NULL;
    int                     process_count = 0;
    int                     i, j;
    
    // Clear out the aggregate assertion values. We are about to re-calculate
    // these values in the big nasty loop below.
    bzero( aggregate_assertions, sizeof(aggregate_assertions) );
    
    // Initialize kEnableIdleIndex to idle_enable_assumed
    aggregate_assertions[kEnableIdleIndex] = idle_enable_assumed;
    
    process_count = CFDictionaryGetCount(assertionsDict);
    process_assertions = malloc(sizeof(CFDictionaryRef) * process_count);
    CFDictionaryGetKeysAndValues(assertionsDict, NULL, (const void **)process_assertions);
    for(i=0; i<process_count; i++)
    {
        CFArrayRef          asst_arr = NULL;
        int                 asst_arr_count = 0;
        if(!isA_CFDictionary(process_assertions[i])) continue;

        asst_arr = isA_CFArray(
            CFDictionaryGetValue(process_assertions[i], kIOPMAssertionsKey));
        if(!asst_arr) continue;
        
        asst_arr_count = CFArrayGetCount(asst_arr);
        for(j=0; j<asst_arr_count; j++)
        {
            CFDictionaryRef     this_assertion = NULL;
            CFStringRef         asst_type = NULL;
            CFNumberRef         asst_val = NULL;
            int                 val = -1;
            
            this_assertion = CFArrayGetValueAtIndex(asst_arr, j);
            if(isA_CFDictionary(this_assertion))
            {
                asst_type = isA_CFString(
                    CFDictionaryGetValue(this_assertion, kIOPMAssertionTypeKey));
                asst_val = isA_CFNumber(
                    CFDictionaryGetValue(this_assertion, kIOPMAssertionLevelKey));
                if(asst_type && asst_val) {
                    CFNumberGetValue(asst_val, kCFNumberIntType, &val);
                    if(kIOPMAssertionLevelOn == val)
                    {
                        if (CFEqual(asst_type, kIOPMAssertionTypeNeedsCPU))
                        {
                            aggregate_assertions[kHighPerfIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeNoIdleSleep))
                        {
                            aggregate_assertions[kPreventIdleIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeEnableIdleSleep))
                        {
                            aggregate_assertions[kEnableIdleIndex] = 1;
                            
                            // Once idle_enable_assumed != true, the system will not idle sleep
                            // unless kIOPMAssertionTypeEnableIdleSleep is asserted.
                            idle_enable_assumed = false;

                        } else if (CFEqual(asst_type, kIOPMAssertionTypeDisableInflow))
                        {
                            aggregate_assertions[kDisableInflowIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeInhibitCharging))
                        {
                            aggregate_assertions[kInhibitChargeIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeDisableLowBatteryWarnings))
                        {
                            aggregate_assertions[kDisableWarningsIndex] = 1;
                        } else if (CFEqual(asst_type, kIOPMAssertionTypeNoDisplaySleep))
                        {
                            aggregate_assertions[kPreventDisplaySleepIndex] = 1;
                        }
                    }
                }
           }
        }
    }
    free(process_assertions);
    
    // At this point we have iterated through the entire nested data structure
    // and have calculaed what the total settings are for each of the profiles
    // and have stored them in the global array aggregate_assertions
    
}


static void
evaluateAssertions(void)
{
    calculateAggregates(); // fills results into aggregate_assertions global

    // Override PM settings
    overrideSetting( kPMForceHighSpeed, 
                    aggregate_assertions[kHighPerfIndex]);
    overrideSetting( kPMPreventDisplaySleep, 
                    aggregate_assertions[kPreventDisplaySleepIndex]);
    if(  aggregate_assertions[kPreventIdleIndex]
     || !aggregate_assertions[kEnableIdleIndex]) {
        overrideSetting(kPMPreventIdleSleep, 1);
    } else {
        overrideSetting(kPMPreventIdleSleep, 0);
    }
    
    // Perform kDisableInflowIndex
    if( aggregate_assertions[kDisableInflowIndex] 
        != last_aggregate_assertions[kDisableInflowIndex]) 
    {
        asl_log(NULL, NULL, ASL_LEVEL_INFO, "%s DisableInflow",
            aggregate_assertions[kDisableInflowIndex]? "Activating":"Clearing");
        sendSmartBatteryCommand( kSBUCInflowDisable,
                        aggregate_assertions[kDisableInflowIndex]);
    }
    
    // Perform kInhibitChargeIndex
    if( aggregate_assertions[kInhibitChargeIndex] 
        != last_aggregate_assertions[kInhibitChargeIndex]) 
    {
        asl_log(NULL, NULL, ASL_LEVEL_INFO, "%s InhibitCharge",
            aggregate_assertions[kInhibitChargeIndex]? "Activating":"Clearing");
        sendSmartBatteryCommand( kSBUCChargeInhibit, 
                        aggregate_assertions[kInhibitChargeIndex]);
    }
    
    // Perform low battery warning
    if( aggregate_assertions[kDisableWarningsIndex] 
        != last_aggregate_assertions[kDisableWarningsIndex]) 
    {
        _setRootDomainProperty( CFSTR("BatteryWarningsDisabled"),
                                (aggregate_assertions[kDisableWarningsIndex]
                                    ? kCFBooleanTrue : kCFBooleanFalse));
    }
    
    bcopy(aggregate_assertions, last_aggregate_assertions, 
                            sizeof(aggregate_assertions));
    
    activateSettingOverrides();
}


#if HAVE_SMART_BATTERY
static void
sendSmartBatteryCommand(uint32_t which, uint32_t level)
{
    io_service_t    sbmanager = MACH_PORT_NULL;
    io_connect_t    sbconnection = MACH_PORT_NULL;
    kern_return_t   kret;
    uint32_t        output_count = 1;
    uint64_t        uc_return = kIOReturnError;
    uint64_t        level_64 = level;

    // Find SmartBattery manager
    sbmanager = IOServiceGetMatchingService(MACH_PORT_NULL,
                    IOServiceMatching("AppleSmartBatteryManager"));
                    
    if(MACH_PORT_NULL == sbmanager) {
        goto bail;
    }

    kret = IOServiceOpen( sbmanager, mach_task_self(), 0, &sbconnection);
    if(kIOReturnSuccess != kret) {
        goto bail;
    }

    kret = IOConnectCallMethod(
                    sbconnection, // connection
                    which,      // selector
                    &level_64,  // uint64_t *input
                    1,          // input Count
                    NULL,       // input struct count
                    0,          // input struct count
                    &uc_return, // output
                    &output_count,  // output count
                    NULL,       // output struct
                    0);         // output struct count

bail:

    if (MACH_PORT_NULL != sbconnection) {
        IOServiceClose(sbconnection);
    }

    if (MACH_PORT_NULL != sbmanager) {
        IOObjectRelease(sbmanager);
    }

    return;
}
#else /* HAVE_SMART_BATTERY */

static void
sendSmartBatteryCommand(uint32_t which, uint32_t level)
{
    kern_return_t       kr;
    io_iterator_t       iter;
    io_registry_entry_t next;

    do
    {
    if (kSBUCChargeInhibit != which)
        break;
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, 
        IOServiceMatching("IOPMPowerSource"), &iter);
    if (kIOReturnSuccess != kr)
        break;
    if (MACH_PORT_NULL == iter)
        break;
    while ((next = IOIteratorNext(iter)))
    {
        kr = IORegistryEntrySetCFProperty(next, CFSTR(kIOPMPSIsChargingKey), 
                          level ? kCFBooleanFalse : kCFBooleanTrue);
        IOObjectRelease(next);
    }
    IOObjectRelease(iter);
    }
    while (false);
    return;
}

#endif /* HAVE_SMART_BATTERY */


__private_extern__ IOReturn  
_IOPMSetActivePowerProfilesRequiresRoot
(
    CFDictionaryRef which_profile, 
    int uid, 
    int gid
)
{
    IOReturn                    ret = kIOReturnError;
    SCPreferencesRef            energyPrefs = NULL;
    
    /* Private call for Power Management use only.
       PM's configd plugin (our daemon-lite) is the only intended caller
       of _IOPMSetActivePowerProfilesRequiresRoot. configd will call this
       only when a running process (like BatteryMonitor) calls 
       IOPMSetActivePowerProfiles() as adimn, root, or console user.
       configd does the security check there, and then calls _RequiresRoot
       under configd's own root privileges. Writing out the preferences
       file using SCPreferences requires root privileges.       
      */

    if( (!callerIsRoot(uid, gid) &&
        !callerIsAdmin(uid, gid) &&
        !callerIsConsole(uid, gid)) || 
        ( (-1 == uid) || (-1 == gid) ))
    {
        ret = kIOReturnNotPrivileged;
        goto exit;    
    }

    if(!which_profile) {
        // We leave most of the input argument vetting for IOPMSetActivePowerProfiles,
        // which checks the contents of the dictionary. At this point we assume it
        // is well formed (and that it exists).
        ret = kIOReturnBadArgument;
        goto exit;
    }
    
    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, CFSTR(kIOPMAppName), CFSTR(kIOPMPrefsPath) );
    if(!energyPrefs) {
        goto exit;
    }

    if(!SCPreferencesLock(energyPrefs, true))
    {  
        ret = kIOReturnInternalError;
        goto exit;
    }
    
    if(!SCPreferencesSetValue(energyPrefs, CFSTR("ActivePowerProfiles"), which_profile)) {
        goto exit;
    }

    if(!SCPreferencesCommitChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    if(!SCPreferencesApplyChanges(energyPrefs))
    {
        // handle error
        if(kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;        
        goto exit;
    }

    ret = kIOReturnSuccess;
exit:
    if(energyPrefs) {
        SCPreferencesUnlock(energyPrefs);
        CFRelease(energyPrefs);
    }
    return ret;
}

 
/***********************************
 * Dynamic Assertions
 ***********************************/
#define ID_FROM_INDEX(idx)  (idx + 300)
#define INDEX_FROM_ID(id)   (id - 300)

__private_extern__ void
PMAssertions_prime(void)
{
    assertion_types_arr[kHighPerfIndex]             = kIOPMAssertionTypeNeedsCPU; 
    assertion_types_arr[kPreventIdleIndex]          = kIOPMAssertionTypeNoIdleSleep;
    assertion_types_arr[kDisableInflowIndex]        = kIOPMAssertionTypeDisableInflow; 
    assertion_types_arr[kInhibitChargeIndex]        = kIOPMAssertionTypeInhibitCharging;
    assertion_types_arr[kDisableWarningsIndex]      = kIOPMAssertionTypeDisableLowBatteryWarnings;
    assertion_types_arr[kPreventDisplaySleepIndex]  = kIOPMAssertionTypeNoDisplaySleep;
    assertion_types_arr[kEnableIdleIndex]           = kIOPMAssertionTypeEnableIdleSleep;

    return;
}

IOReturn _IOPMAssertionCreateRequiresRoot
(
    mach_port_t         task,
    CFStringRef         assertionString,
    int                 level,
    int                 *assertion_id
)
{
    IOReturn                result = kIOReturnInternalError;
    CFMachPortRef           cf_port_for_task = NULL;
    CFDictionaryRef         tmp_task = NULL;
    CFMutableDictionaryRef  this_task = NULL;
    CFArrayRef              tmp_assertions = NULL;
    CFMutableArrayRef       assertions = NULL;
    mach_port_t             oldNotify = MACH_PORT_NULL;
    kern_return_t           err = KERN_SUCCESS;
    int                     i;

    // assertion_id will be set to kIOPMNullAssertionID on failure, 
    // unless we succeed here and it gets a valid value below.
    *assertion_id = kIOPMNullAssertionID;

    cf_port_for_task = CFMachPortCreateWithPort(0, task, NULL, NULL, 0);
    if(!cf_port_for_task) {
        result = kIOReturnNoMemory;
        goto exit;
    }
    
    if(assertionsDict &&
       (tmp_task = CFDictionaryGetValue(assertionsDict, cf_port_for_task)) )
    {
        // There is an existing dictionary tracking this process's assertions.
        mach_port_deallocate(mach_task_self(), task);
        this_task = CFDictionaryCreateMutableCopy(0, 0, tmp_task);
        CFDictionarySetValue(assertionsDict, cf_port_for_task, this_task);
    } else {
        // This is the first assertion for this process, so we'll create a new
        // assertion datastructure.
        this_task = CFDictionaryCreateMutable(0, 0, 
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
        if(!this_task){
            mach_port_deallocate(mach_task_self(), task);
            result = kIOReturnNoMemory;
            goto exit;
        }
        CFDictionarySetValue(this_task, kIOPMTaskPortKey, cf_port_for_task);    

        // Register for a dead name notification on this task_t
        err = mach_port_request_notification(
                    mach_task_self(),       // task
                    task,                   // port that will die
                    MACH_NOTIFY_DEAD_NAME,   // msgid
                    1,                      // make-send count
                    CFMachPortGetPort(serverMachPort),       // notify port
                    MACH_MSG_TYPE_MAKE_SEND_ONCE, // notifyPoly
                    &oldNotify);            // previous
        if(KERN_SUCCESS != err)
        {
            syslog(LOG_ERR, "mach port request notification error %s(%08x)\n",
                mach_error_string(err), err);
            mach_port_deallocate(mach_task_self(), task);
            result = err;
            goto exit;
        }
                    
        if (oldNotify != MACH_PORT_NULL) {
            mach_port_deallocate(mach_task_self(), oldNotify);
        }

        // assertionsDict is the global dictionary that maps a process's task_t
        // to all power management assertions it has created.
        if(!assertionsDict) {
            assertionsDict = CFDictionaryCreateMutable(0, 0, 
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }
        CFDictionarySetValue(assertionsDict, cf_port_for_task, this_task);
    }

    tmp_assertions = CFDictionaryGetValue(this_task, kIOPMAssertionsKey);
    if(!tmp_assertions) {
        assertions = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    } else {
        assertions = CFArrayCreateMutableCopy(0, 0, tmp_assertions);
    }
    CFDictionarySetValue(this_task, kIOPMAssertionsKey, assertions);

    // TODO: Check for existing assertions of the same type in this process.
    // refcount them if they exist

    // find empty slot
    CFIndex     arrayIndex;
    int         asst_count;
    asst_count = CFArrayGetCount(assertions);
    for (arrayIndex=0; arrayIndex<asst_count; arrayIndex++) 
    {
        // find the first empty element in the array
        // empty elements are denoted by the value kCFBooleanFalse
        if(kCFBooleanFalse == CFArrayGetValueAtIndex(assertions, arrayIndex)) break;
    }
    if (arrayIndex >= kMaxTaskAssertions) 
    {
        // ERROR! out of space in array!
        result = kIOReturnNoMemory;
        goto exit;
    }

    CFMutableDictionaryRef          new_assertion_dict = NULL;
    CFNumberRef                     cf_assertion_val = NULL;
    
    *assertion_id = ID_FROM_INDEX(arrayIndex);
    new_assertion_dict = CFDictionaryCreateMutable(0, 2,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
    cf_assertion_val = CFNumberCreate(0, kCFNumberIntType, &level);
    CFDictionarySetValue(new_assertion_dict, 
                            kIOPMAssertionTypeKey, assertionString);
    CFDictionarySetValue(new_assertion_dict, 
                            kIOPMAssertionLevelKey, cf_assertion_val);
    CFRelease(cf_assertion_val);
    CFArraySetValueAtIndex(assertions, arrayIndex, new_assertion_dict);
    CFRelease(new_assertion_dict);

    evaluateAssertions();

    result = kIOReturnSuccess;
exit:
    if(this_task) CFRelease(this_task);
    if(assertions) CFRelease(assertions);
    if(cf_port_for_task) CFRelease(cf_port_for_task);

    return result;
}


kern_return_t _io_pm_assertion_release
(
    mach_port_t server,
    mach_port_t task,
    int assertion_id,
    int *return_code
)
{
    CFMachPortRef           cf_port_for_task = NULL;
    CFDictionaryRef         calling_task = NULL;
    CFMutableArrayRef       assertions = NULL;              
    CFTypeRef               assertion_to_release = NULL;
    int                     i;
    int                     n;
    Boolean                 releaseTask;

    cf_port_for_task = CFMachPortCreateWithPort(0, task, NULL, NULL, 0);
    if(!cf_port_for_task) {
        *return_code = kIOReturnNoMemory;
        goto exit;
    }

    if (assertionsDict) {
        calling_task = CFDictionaryGetValue(assertionsDict, cf_port_for_task);
    }
    if(!calling_task) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    
    // Retrieve assertions array for the calling task
    assertions = (CFMutableArrayRef)CFDictionaryGetValue(calling_task, kIOPMAssertionsKey);
    if(!assertions) {
        *return_code = kIOReturnInternalError;
        goto exit;
    }
    
    // Look up assertion at assertion_id and make sure it exists
    CFIndex arrayIndex = INDEX_FROM_ID(assertion_id);
    if ((arrayIndex < 0) || (arrayIndex >= CFArrayGetCount(assertions))) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    assertion_to_release = CFArrayGetValueAtIndex(assertions, arrayIndex);
    if(!assertion_to_release || !isA_CFDictionary(assertion_to_release)) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    
    // Release it
    CFArraySetValueAtIndex(assertions, arrayIndex, kCFBooleanFalse);

    // Check for last reference and cleanup
    releaseTask = TRUE;
    n = CFArrayGetCount(assertions);
    for (i =0; i < n; i++) {
        CFTypeRef    assertion;

        assertion = CFArrayGetValueAtIndex(assertions, i);
        if (!CFEqual(assertion, kCFBooleanFalse)) {
            releaseTask = FALSE;
            break;
        }
    }
    if (releaseTask) {
        CFDictionaryRemoveValue(assertionsDict, cf_port_for_task);
        mach_port_deallocate(mach_task_self(), task);
    }

    // Re-evaluate
    evaluateAssertions();
    
    *return_code = kIOReturnSuccess;    
exit:
    if(cf_port_for_task) CFRelease(cf_port_for_task);
    mach_port_deallocate(mach_task_self(), task);
    return KERN_SUCCESS;   
}

// Returns a CFDictionary of PM Assertions
// * The keys of which are CFNumber (IntType) process ids (pids)
// * The values are CFArrays of assertions created by that process
//     - Each entry in the array is a CFDictionary with:
//          - key = CFSTR("assert_type"); value = CFStringRef
//          - key = CFSTR("assert_value"); value = CFNumberRef
kern_return_t _io_pm_copy_active_assertions
(
    mach_port_t             server,
    vm_offset_t             *profiles,
    mach_msg_type_number_t  *profilesCnt
)
{
    CFDataRef               serialized_object = NULL;
    CFMachPortRef           *task_id_arr = NULL;
    CFDictionaryRef         *task_assertion_dict_arr = NULL;
    CFMutableDictionaryRef  ret_dict = 0;
    int                     dict_count;
    int                     i;

    *profiles = 0;
    *profilesCnt = 0;
    
    if(!assertionsDict) goto exit;

    dict_count = CFDictionaryGetCount(assertionsDict);
    if(0 == dict_count) goto exit;

    ret_dict = CFDictionaryCreateMutable(0, 0, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    // Iterate our master list of assertions and map them to pids
    task_id_arr = (CFMachPortRef *)malloc(sizeof(CFMachPortRef)*dict_count);
    task_assertion_dict_arr = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef)*dict_count);
    if(!task_id_arr || !task_assertion_dict_arr) goto exit;
    CFDictionaryGetKeysAndValues(assertionsDict, 
        (const void **)task_id_arr, (const void **)task_assertion_dict_arr);
    for(i=0; i<dict_count; i++)
    {
        int                 ppid;
        CFStringRef         cf_ppid;
        CFArrayRef          cf_process_assertions_arr;
        char                pid_str[10];
        
        if(!task_id_arr[i] || !task_assertion_dict_arr[i]) continue;
        pid_for_task(CFMachPortGetPort(task_id_arr[i]), &ppid);
        // hackaround: IOCFSerialize() crashes on dictionaries with
        // non-string keys. Put the pid number inside a CFStringRef
        // and unroll it on the other side of the mig in IOKitUser.
        snprintf(pid_str, 9, "%d", ppid);
        cf_ppid = CFStringCreateWithCString(0, pid_str, kCFStringEncodingMacRoman);        
        cf_process_assertions_arr = CFDictionaryGetValue(
            task_assertion_dict_arr[i], kIOPMAssertionsKey);

        CFDictionaryAddValue(ret_dict, cf_ppid, cf_process_assertions_arr);
        CFRelease(cf_ppid);
    }
    free(task_id_arr);
    free(task_assertion_dict_arr);

    // Serialize and pass the buffer back over to the client
    serialized_object = (CFDataRef)IOCFSerialize(ret_dict, 0);
    if(serialized_object)
    {
        kern_return_t       status = KERN_SUCCESS;
        void                *tmp_buf = NULL;
        int                 len = CFDataGetLength(serialized_object);
        status = vm_allocate(mach_task_self(), (void *)&tmp_buf, len, TRUE);
        if(KERN_SUCCESS != status) goto exit;

        bcopy((void *)CFDataGetBytePtr(serialized_object), tmp_buf, len);
        CFRelease(serialized_object);
        
        *profiles = (vm_offset_t)tmp_buf;
        *profilesCnt = len;
    }
    CFRelease(ret_dict);
exit:
    return KERN_SUCCESS;
}

kern_return_t _io_pm_copy_assertions_status
(
    mach_port_t server,
    vm_offset_t *profiles,
    mach_msg_type_number_t *profilesCnt
)
{
    CFDictionaryRef                 assertions_info = NULL;
    CFDataRef                       serialized_object = NULL;
    CFNumberRef                     cf_agg_vals[kIOPMNumAssertionTypes];
    int                             i;
    
    // Massage int values into CFNumbers for CFDictionaryCreate
    for(i=0; i<kIOPMNumAssertionTypes; i++)
    {
        cf_agg_vals[i] = CFNumberCreate(
            0, 
            kCFNumberIntType,
            &aggregate_assertions[i]);
    }

    // We assume that aggregate_assertions is up-to-date
    // and just return whatever's in there as the
    // current assertion values packed into a CFDictionary.
    assertions_info = CFDictionaryCreate(
        0,
        (const void **)assertion_types_arr,     // type: CFStringRef
        (const void **)cf_agg_vals,   // value: CFNumberRef
        kIOPMNumAssertionTypes,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    // Release CFNumbers
    for(i=0; i<kIOPMNumAssertionTypes; i++)
    {
        CFRelease(cf_agg_vals[i]);
    }

    serialized_object = (CFDataRef)IOCFSerialize((CFTypeRef)assertions_info, 0);
    CFRelease(assertions_info);
    
    if(serialized_object)
    {
        kern_return_t       status = KERN_SUCCESS;
        void                *tmp_buf = NULL;
        int                 len = CFDataGetLength(serialized_object);
        status = vm_allocate(mach_task_self(), (void *)&tmp_buf, len, TRUE);
        if(KERN_SUCCESS != status) goto exit;

        bcopy((void *)CFDataGetBytePtr(serialized_object), tmp_buf, len);
        CFRelease(serialized_object);
        
        *profiles = (vm_offset_t)tmp_buf;
        *profilesCnt = len;
    } else {
        *profiles = 0;
        *profilesCnt = 0;
    }

exit:
    return KERN_SUCCESS;
}

__private_extern__ void
cleanupAssertions(
    mach_port_t dead_port
)
{
    CFMachPortRef               cf_task_port = NULL;
    
    if(!assertionsDict) {
        return;
    }
    
    // Clean up after this dead process
    cf_task_port = CFMachPortCreateWithPort(0, dead_port, NULL, NULL, 0);
    if(!cf_task_port) return;

    if (CFDictionaryContainsKey(assertionsDict, cf_task_port)) {
        // Remove the process's tracking data
        CFDictionaryRemoveValue(assertionsDict, cf_task_port);
        mach_port_deallocate(mach_task_self(), dead_port);
        evaluateAssertions();
    }

    CFRelease(cf_task_port);
    return;
}

