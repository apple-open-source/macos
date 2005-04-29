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
#include <grp.h>
#include <pwd.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "PMSettings.h"
#include "SetActive.h"
#include "powermanagementServer.h"

#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath        "com.apple.PowerManagement.xml"

#define kAssertionsArraySize        5
#define kIOPMTaskPortKey            CFSTR("task")
#define kIOPMAssertionsKey          CFSTR("assertions")

#define kIOPMNumAssertionTypes      2
#define kHighPerfIndex              0
#define kPreventIdleIndex           1

static int callerIsRoot(int uid, int gid);
static int callerIsAdmin(int uid, int gid);
static int callerIsConsole(int uid, int gid);

extern CFMachPortRef            serverMachPort;

// external
__private_extern__ void mig_server_callback(CFMachPortRef port, void *msg, CFIndex size, void *info);

// forward
__private_extern__ void     cleanupAssertions(mach_port_t dead_port);
static void                 evaluateAssertions(void);
static void                 calculateAggregates(void);

// globals
static CFMutableDictionaryRef           assertionsDict = NULL;
static int                              aggregate_assertions[kIOPMNumAssertionTypes];
static CFStringRef                      assertion_types_arr[kIOPMNumAssertionTypes];

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
    for(i=0; i<kIOPMNumAssertionTypes; i++)
    {
        aggregate_assertions[i] = 0;
    }
    
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
                    CFDictionaryGetValue(this_assertion, kIOPMAssertionValueKey));
                if(asst_type && asst_val) {
                    CFNumberGetValue(asst_val, kCFNumberIntType, &val);
                    if(kIOPMAssertionEnable == val)
                    {
                        if(kCFCompareEqualTo ==
                            CFStringCompare(asst_type, kIOPMCPUBoundAssertion, 0))
                        {
                            aggregate_assertions[kHighPerfIndex] = 1;                        
                        } else if(kCFCompareEqualTo ==
                            CFStringCompare(asst_type, kIOPMPreventIdleSleepAssertion, 0))
                        {
                            aggregate_assertions[kPreventIdleIndex] = 1;                        
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

    if(aggregate_assertions[kHighPerfIndex]) {
        overrideSetting(kPMForceHighSpeed, 1);
    } else {
        overrideSetting(kPMForceHighSpeed, 0);
    }
    
    activateSettingOverrides();
}

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

    if( !callerIsRoot(uid, gid) &&
        !callerIsAdmin(uid, gid) &&
        !callerIsConsole(uid, gid) || 
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


static int
callerIsRoot(
    int uid,
    int gid
)
{
    return (0 == uid);
}

static int
callerIsAdmin(
    int uid,
    int gid
)
{
    int         ngroups = NGROUPS_MAX+1;
    int         group_list[NGROUPS_MAX+1];
    int         i;
    struct group    *adminGroup;
    struct passwd   *pw;
        
    
    pw = getpwuid(uid);
    if(!pw) return false;
    
    getgrouplist(pw->pw_name, pw->pw_gid, group_list, &ngroups);

    adminGroup = getgrnam("admin");
    if (adminGroup != NULL) {
        gid_t    adminGid = adminGroup->gr_gid;
        for(i=0; i<ngroups; i++)
        {
            if (group_list[i] == adminGid) {
                return TRUE;    // if a member of group "admin"
            }
        }
    }
    return false;
}

static int
callerIsConsole(
    int uid,
    int gid)
{
    CFStringRef                 user_name = 0;
    uid_t                       console_uid;
    gid_t                       console_gid;
    
    user_name = SCDynamicStoreCopyConsoleUser(
            NULL, &console_uid, &console_gid);
    if(user_name) CFRelease(user_name);
    else return false;

    return ((uid == console_uid) && (gid == console_gid));
}
 
/***********************************
 * Dynamic Profiles
 ***********************************/

__private_extern__ void
PMAssertions_prime(void)
{
    assertion_types_arr[kHighPerfIndex] = kIOPMCPUBoundAssertion; 
    assertion_types_arr[kPreventIdleIndex] = kIOPMPreventIdleSleepAssertion;
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
    CFMachPortRef           cf_port_for_task = NULL;
    mach_port_name_t        rcv_right = MACH_PORT_NULL;
    kern_return_t           kern_result = KERN_SUCCESS;
    CFDictionaryRef         tmp_task = NULL;
    CFMutableDictionaryRef  this_task = NULL;
    CFArrayRef              tmp_assertions = NULL;
    CFMutableArrayRef       assertions = NULL;
    mach_port_t             oldNotify = MACH_PORT_NULL;
    kern_return_t           err = KERN_SUCCESS;
    int                     i;

    // assertion_id will be set to -1 on failure, unless we succeed here
    // and it gets a valid [0, (kAssertionsArraySize-1)] value below.
    *assertion_id = -1;

    cf_port_for_task = CFMachPortCreateWithPort(0, task, mig_server_callback, 0, 0);
    if(!cf_port_for_task) {
        *result = kIOReturnNoMemory;
        goto exit;
    }
    
    if(assertionsDict &&
       (tmp_task = CFDictionaryGetValue(assertionsDict, cf_port_for_task)) )
    {
        // There is an existing dictionary tracking this process's assertions.
        this_task = CFDictionaryCreateMutableCopy(0, 0, tmp_task);
        CFDictionarySetValue(assertionsDict, cf_port_for_task, this_task);
    } else {
        // This is the first assertion for this process, so we'll create a new
        // assertion datastructure.
        this_task = CFDictionaryCreateMutable(0, 0, 
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
        if(!this_task){
            *result = kIOReturnNoMemory;
            goto exit;
        }
        CFDictionarySetValue(this_task, CFSTR("task"), cf_port_for_task);    

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
            *result = err;
            goto exit;
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
        assertions = CFArrayCreateMutable(0, kAssertionsArraySize, &kCFTypeArrayCallBacks);
        for(i=0; i<kAssertionsArraySize; i++) {
            CFArraySetValueAtIndex(assertions, i, kCFBooleanFalse);
        }
    } else {
        assertions = CFArrayCreateMutableCopy(0, 0, tmp_assertions);
    }
    CFDictionarySetValue(this_task, kIOPMAssertionsKey, assertions);

    // TODO: Check for existing assertions of the same type in this process.
    // refcount them if they exist

    // find empty slot
    int         index = -1;
    int         asst_count;
    asst_count = CFArrayGetCount(assertions);
    if(0 == asst_count) {
        index = 0;
    } else {
        for(i=0; i<asst_count; i++) {
            // find the first empty element in the array
            // empty elements are denoted by the value kCFBooleanFalse
            if(kCFBooleanFalse == CFArrayGetValueAtIndex(assertions, i)) break;
        }
        if(kAssertionsArraySize == i) {
            // ERROR! out of space in array!
            *result = kIOReturnNoMemory;
            goto exit;
        } else index = i;
    }

    CFMutableDictionaryRef          new_assertion_dict = NULL;
    CFStringRef                     cf_assertion_str = NULL;
    CFNumberRef                     cf_assertion_val = NULL;
    
    *assertion_id = index;
    new_assertion_dict = CFDictionaryCreateMutable(0, 2,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
    cf_assertion_str = CFStringCreateWithCString(0, profile, kCFStringEncodingMacRoman);
    cf_assertion_val = CFNumberCreate(0, kCFNumberIntType, &level);
    CFDictionarySetValue(new_assertion_dict, kIOPMAssertionTypeKey, cf_assertion_str);
    CFDictionarySetValue(new_assertion_dict, kIOPMAssertionValueKey, cf_assertion_val);
    CFRelease(cf_assertion_str);
    CFRelease(cf_assertion_val);
    CFArraySetValueAtIndex(assertions, index, new_assertion_dict);
    CFRelease(new_assertion_dict);

    evaluateAssertions();

    *result = kIOReturnSuccess;
exit:
    if(this_task) CFRelease(this_task);
    if(assertions) CFRelease(assertions);
    if(cf_port_for_task) CFRelease(cf_port_for_task);
    return KERN_SUCCESS;
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

    if((assertion_id < 0) || (assertion_id >= kAssertionsArraySize)) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    cf_port_for_task = CFMachPortCreateWithPort(0, task, mig_server_callback, 0, 0);
    if(!cf_port_for_task) {
        *return_code = kIOReturnNoMemory;
        goto exit;
    }

    calling_task = CFDictionaryGetValue(assertionsDict, cf_port_for_task);
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
    assertion_to_release = CFArrayGetValueAtIndex(assertions, assertion_id);
    if(!assertion_to_release || !isA_CFDictionary(assertion_to_release)) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    
    // Release it
    CFArraySetValueAtIndex(assertions, assertion_id, kCFBooleanFalse);
    
    evaluateAssertions();
    
    *return_code = kIOReturnSuccess;    
exit:
    if(cf_port_for_task) CFRelease(cf_port_for_task);
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
    int                         dead_pid = -1;
    
    if(!assertionsDict) {
        return;
    }
    
    // Clean up after this dead process
    cf_task_port = CFMachPortCreateWithPort(0, dead_port, mig_server_callback, 0, 0);
    if(!cf_task_port) return;

    // Log a message on this exceptional circumstance.
    pid_for_task(dead_port, &dead_pid);
//    syslog(LOG_ERR, "Power Management cleaning up assertions for pid %d (port %d).\n", \
        dead_pid, dead_port);
    
    // Remove the process's tracking data
    // Deletes the entire dictionary that tracks this process.
    CFDictionaryRemoveValue(assertionsDict, cf_task_port);

    evaluateAssertions();
exit:
    return;
}

