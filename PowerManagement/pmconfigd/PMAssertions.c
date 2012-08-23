/*
 * Copyright (c) 2005-2006 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include <unistd.h>
#include <stdlib.h>
#include <notify.h>
#include <asl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <dispatch/dispatch.h>
#include <bsm/libbsm.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <libproc.h>



#include "PMConnection.h"
#include "PrivateLib.h"
#include "PMSettings.h"
#include "PMAssertions.h"
#include "BatteryTimeRemaining.h"
#include "PMStore.h"
#include "powermanagementServer.h"

#define kIOPMAppName                "Power Management configd plugin"
#define kIOPMPrefsPath              "com.apple.PowerManagement.xml"
#define FMMD_WIPE_BOOT_ARG          "fmm-wipe-system-status"


#define kMaxAssertions              10240
#define kMaxTaskAssertions          1024
#define kIOPMTaskPortKey            CFSTR("task")
#define kIOPMTaskPIDKey             CFSTR("pid")
#define kIOPMTaskAssertionsKey      CFSTR("assertions")
#define kIOPMTaskDispatchSourceKey  CFSTR("dispatchsource")
#define kIOPMTaskNameKey            CFSTR("name")

// CAST_PID_TO_KEY casts a mach_port_t into a void * for CF containers
#define CAST_PID_TO_KEY(x)          ((void *)(uintptr_t)(x))


/* kIOPMAssertionTimerRefKey
 * For internal use only.
 * Key into Assertion dictionary.
 * Records the CFRunLoopTimerRef (if any) associated with a given assertion.
 */
#define kIOPMAssertionTimerRefKey   CFSTR("AssertTimerRef")

/* kIOPMAssertionLevelsBitfield
 * For PM internal use only.
 * Each assertion contains this int CFNumber property. Its value is
 * a bitfield, with bit indexes defined by the assertion indices
 * defiend below.
 *
 * A value of 1 in bit (1 << kPreventIdleIndex) indicates that
 * this assertion has an 'on' value for assertion type kIOPMAssertionTypeNoIdleSleep.
 *      
 */
#define kIOPMAssertionLevelsBits    CFSTR("LevelsBitfield")


// Selectors for AppleSmartBatteryManagerUserClient
enum {
    kSBUCInflowDisable              = 0,
    kSBUCChargeInhibit              = 1
};

typedef enum {
    kTimerTypeTimedOut              = 0,
    kTimerTypeReleased              = 1
} TimerType;

#define ID_FROM_INDEX(idx)          (idx + 300)
#define INDEX_FROM_ID(id)           (id - 300)
#define MAKE_UNIQAID(pid, assertId, id_cnt) ((uint64_t)(pid) << 32) | ((assertId) & 0xffff) << 16 | ((id_cnt) & 0xffff)
#define GET_ASSERTID(uniqaid)       (((uniqaid) >> 16) & 0xffff)
#define GET_ASSERTPID(uniqaid)      (((uniqaid) >> 32) & 0xffffffff) 

#define SET_AGGREGATE_LEVEL(idx, val)   { if (val)  aggregate_assertions |= (1 << idx); \
                                          else      aggregate_assertions &= ~(1<<idx); }
#define LEVEL_FOR_BIT(idx)          ((aggregate_assertions & (1 << idx)) ? 1:0)
#define LAST_LEVEL_FOR_BIT(idx)     ((last_aggregate_assertions & (1 << idx)) ? 1:0)

#define ASSERTION_LOG_DELAY         (5LL)


CFArrayRef copyScheduledPowerEvents(void);
CFDictionaryRef copyRepeatPowerEvents(void);

// forward

static void                         sendSmartBatteryCommand(uint32_t which, uint32_t level);
static void                         sendUserAssertionsToKernel(uint32_t user_assertions);
static void                         evaluateAssertions(void);
static void                         HandleProcessExit(pid_t deadPID);

#if !TARGET_OS_EMBEDDED
static void logASLAssertionEvent(
    const char      *assertionAction,
    assertion_t     *assertion);
#else
#define                             logASLAssertionEvent(X1, X2)    
#define                             logASLAssertionSummary()
#define                             logASLAssertionsAggregate()
#endif

static bool                         propertiesDictRequiresRoot(CFDictionaryRef   props);

static IOReturn                     doRetain(pid_t pid, IOPMAssertionID id);
static IOReturn                     doRelease(pid_t pid, IOPMAssertionID id);
static IOReturn                     doSetProperties(pid_t pid, 
                                                        IOPMAssertionID id, 
                                                        CFDictionaryRef props);

//static int                          indexForAssertionName(CFStringRef assertionName);
static CFArrayRef                   copyPIDAssertionDictionaryFlattened(void);
static CFDictionaryRef              copyAggregateValuesDictionary(void);
static CFArrayRef                   copyTimedOutAssertionsArray(void);

static IOReturn                     doCreate(pid_t pid, CFMutableDictionaryRef newProperties,
                                                        IOPMAssertionID *assertion_id);
static IOReturn copyAssertionForID(
        pid_t inPID, int inID,
        CFMutableDictionaryRef  *outAssertion);

static void                         processInfoCreate(pid_t p, dispatch_source_t d);
static bool                         processInfoRetain(pid_t p);
static void                         processInfoRelease(pid_t p);
static void                         sendActivityTickle ();


// globals
static const int                    kMaxCountTimedOut = 5;
extern CFMachPortRef                pmServerMachPort;

static CFMutableArrayRef            gTimedOutArray = NULL;

static uint32_t                     kerAssertionBits = 0;
static int                          aggregate_assertions;
static CFStringRef                  assertion_types_arr[kIOPMNumAssertionTypes];

__private_extern__ bool isDisplayAsleep( );
__private_extern__ void logASLMessageSleepServiceTerminated(int forcedTimeoutCnt);

static CFMutableDictionaryRef       gAssertionsArray = NULL;
static CFMutableDictionaryRef       gProcessDict = NULL;
static CFMutableDictionaryRef       gUserAssertionTypesDict = NULL;
assertionType_t     gAssertionTypes[kIOPMNumAssertionTypes];
uint32_t            gDisplaySleepTimer = 0;      /* Display Sleep timer value in mins */


void handleAssertionTimeout(assertionType_t *assertType);
void resetGlobalTimer(assertionType_t *assertType, uint64_t timer);
static IOReturn raiseAssertion(assertion_t *assertion);

dispatch_source_t       logDispatch = NULL;
static uint32_t         gNextAssertionIdx = 0;
extern uint32_t         gDebugFlags;

#pragma mark -
#pragma mark MIG

/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 * MIG Handlers
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/

kern_return_t _io_pm_assertion_create
(
    mach_port_t         server __unused,
    audit_token_t       token,
    vm_offset_t         props,
    mach_msg_type_number_t  propsCnt,
    int                 *assertion_id,
    int                 *return_code
)
{
    CFMutableDictionaryRef     newAssertionProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;
    
    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
        
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        newAssertionProperties = (CFMutableDictionaryRef)CFPropertyListCreateWithData(
                            0, unfolder, kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
        CFRelease(unfolder);
    }
    
    if (!newAssertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    // Check for privileges if the assertion requires it.
    if (propertiesDictRequiresRoot(newAssertionProperties)
        && ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID))))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    *return_code = doCreate(callerPID, newAssertionProperties, (IOPMAssertionID *)assertion_id);
    
exit:
    if (newAssertionProperties) {
        CFRelease(newAssertionProperties);
    }
    
    vm_deallocate(mach_task_self(), props, propsCnt);
    
    return KERN_SUCCESS;
}


/*****************************************************************************/

kern_return_t _io_pm_assertion_set_properties
(
    mach_port_t         server __unused,
    audit_token_t       token,
    int                 assertion_id,
    vm_offset_t         props,
    mach_msg_type_number_t propsCnt,
    int                 *return_code
 ) 
{
    CFDictionaryRef     setProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);
    
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        setProperties = (CFDictionaryRef)CFPropertyListCreateWithData(0, unfolder, 0, NULL, NULL);
        CFRelease(unfolder);
    }
    
    if (!setProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    *return_code = doSetProperties(callerPID, assertion_id, setProperties);
    
    CFRelease(setProperties);
    
exit:
    vm_deallocate(mach_task_self(), props, propsCnt);
    
    return KERN_SUCCESS;
    
}

/*****************************************************************************/
kern_return_t _io_pm_assertion_retain_release
(
    mach_port_t         server __unused,
    audit_token_t       token,
    int                 assertion_id,
    int                 action,
    int                 *return_code
) 
{
    pid_t               callerPID = -1;
    
    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);
    
    if (kIOPMAssertionMIGDoRetain == action) {
        *return_code = doRetain(callerPID, assertion_id);
    } else {
        *return_code = doRelease(callerPID, assertion_id);
    }
    
    return KERN_SUCCESS;
}

/*****************************************************************************/
kern_return_t _io_pm_assertion_copy_details
(
    mach_port_t         server,
    audit_token_t       token,
    int                 assertion_id,
    int                 whichData,
    vm_offset_t         *assertions,
    mach_msg_type_number_t  *assertionsCnt,
    int                 *return_val
) 
{
    CFTypeRef           theCollection = NULL;
    CFDataRef           serializedDetails = NULL;
    pid_t               callerPID = -1;
    

    *return_val = kIOReturnNotFound;
    
    if (kIOPMAssertionMIGCopyAll == whichData)
    {
        theCollection = copyPIDAssertionDictionaryFlattened();
        
    } else if (kIOPMAssertionMIGCopyOneAssertionProperties == whichData) 
    {
        audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

        *return_val = copyAssertionForID(callerPID, assertion_id,  
                                         (CFMutableDictionaryRef *)&theCollection);

    } else if (kIOPMAssertionMIGCopyStatus == whichData)
    {
        theCollection = copyAggregateValuesDictionary();
    
    } else if (kIOPMAssertionMIGCopyTimedOutAssertions == whichData) 
    {
        theCollection = copyTimedOutAssertionsArray();
    
    }
    else if (kIOPMPowerEventsMIGCopyScheduledEvents == whichData)
    {
        theCollection = copyScheduledPowerEvents();
    }
    else if (kIOPMPowerEventsMIGCopyRepeatEvents == whichData)
    {
        theCollection = copyRepeatPowerEvents();
    }
        
    if (!theCollection) {
        *assertionsCnt = 0;
        *assertions = 0;
        *return_val = kIOReturnSuccess;
        return KERN_SUCCESS;
    }
    
        
    serializedDetails = CFPropertyListCreateData(0, theCollection, 
                                                 kCFPropertyListBinaryFormat_v1_0, 0, NULL);            

    CFRelease(theCollection);        
        
    if (serializedDetails) 
    {
        *assertionsCnt = CFDataGetLength(serializedDetails);
        
        vm_allocate(mach_task_self(), (vm_address_t *)assertions, *assertionsCnt, TRUE);
        
        memcpy((void *)*assertions, CFDataGetBytePtr(serializedDetails), *assertionsCnt);

        CFRelease(serializedDetails);

        *return_val = kIOReturnSuccess;
    } else {
        *return_val = kIOReturnInternalError;
    }
    
    return KERN_SUCCESS;
}

static void sendActivityTickle ()
{
    io_service_t                rootDomainService = IO_OBJECT_NULL;
    io_connect_t                gRootDomainConnect = IO_OBJECT_NULL;
    kern_return_t               kr = 0;
    IOReturn                    ret;

    if (!isDisplayAsleep( ))
        return; // Nothing to do when display is on

    // Find it
    rootDomainService = getRootDomain();
    if (IO_OBJECT_NULL == rootDomainService) {
        goto exit;
    }

    // Open it
    kr = IOServiceOpen(rootDomainService, mach_task_self(), 0, &gRootDomainConnect);    
    if (KERN_SUCCESS != kr) {
        goto exit;    
    }
    
    ret = IOConnectCallMethod(gRootDomainConnect, kPMActivityTickle, 
                    NULL, 0, 
                    NULL, 0, NULL, 
                    NULL, NULL, NULL);

    if (kIOReturnSuccess != ret)
    {
        goto exit;
    }

exit:
    if (IO_OBJECT_NULL != gRootDomainConnect)
        IOServiceClose(gRootDomainConnect);
    return;

}
static void sendUserAssertionsToKernel(uint32_t user_assertions)
{
    io_service_t                rootDomainService = IO_OBJECT_NULL;
    io_connect_t                gRootDomainConnect = IO_OBJECT_NULL;
    kern_return_t               kr = 0;
    const uint64_t              in = (uint64_t)user_assertions;
    IOReturn                    ret;

    // Find it
    rootDomainService = getRootDomain();
    if (IO_OBJECT_NULL == rootDomainService) {
        goto exit;
    }

    // Open it
    kr = IOServiceOpen(rootDomainService, mach_task_self(), 0, &gRootDomainConnect);    
    if (KERN_SUCCESS != kr) {
        goto exit;    
    }
    
    ret = IOConnectCallMethod(gRootDomainConnect, kPMSetUserAssertionLevels, 
                    &in, 1, 
                    NULL, 0, NULL, 
                    NULL, NULL, NULL);

    if (kIOReturnSuccess != ret)
    {
        goto exit;
    }

exit:
    if (IO_OBJECT_NULL != gRootDomainConnect)
        IOServiceClose(gRootDomainConnect);
    return;
}

#pragma mark -
#pragma mark Act on assertions

__private_extern__ void PMAssertions_SettingsHaveChanged(void)
{
    static int lastDWBTSetting = -1;
    int newDWBT = GetPMSettingBool(CFSTR(kIOPMDarkWakeBackgroundTaskKey));

    if (newDWBT == lastDWBTSetting) {
        return;
    }
    lastDWBTSetting = newDWBT;

    configAssertionType(kBackgroundTaskIndex, false);
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
                    
    if (MACH_PORT_NULL == sbmanager) {
        goto bail;
    }

    kret = IOServiceOpen( sbmanager, mach_task_self(), 0, &sbconnection);
    if (kIOReturnSuccess != kret) {
        goto bail;
    }

    IOConnectCallMethod(
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
    kr = IOServiceGetMatchingServices(kIOMasterPortDefault, 
        IOServiceMatching("IOPMPowerSource"), &iter);
    if (kIOReturnSuccess != kr)
        break;
    if (MACH_PORT_NULL == iter)
        break;
    while ((next = IOIteratorNext(iter)))
    {
        kr = IORegistryEntrySetCFProperty(next, 
                          (which == kSBUCChargeInhibit) ? CFSTR(kIOPMPSIsChargingKey) : CFSTR(kIOPMPSExternalConnectedKey), 
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

    if ( (!callerIsRoot(uid) &&
        !callerIsAdmin(uid, gid) &&
        !callerIsConsole(uid, gid)) || 
        ( (-1 == uid) || (-1 == gid) ))
    {
        ret = kIOReturnNotPrivileged;
        goto exit;    
    }

    if (!which_profile) {
        // We leave most of the input argument vetting for IOPMSetActivePowerProfiles,
        // which checks the contents of the dictionary. At this point we assume it
        // is well formed (and that it exists).
        ret = kIOReturnBadArgument;
        goto exit;
    }
    
    energyPrefs = SCPreferencesCreate( kCFAllocatorDefault, CFSTR(kIOPMAppName), CFSTR(kIOPMPrefsPath) );
    if (!energyPrefs) {
        goto exit;
    }

    if (!SCPreferencesLock(energyPrefs, true))
    {  
        ret = kIOReturnInternalError;
        goto exit;
    }
    
    if (!SCPreferencesSetValue(energyPrefs, CFSTR("ActivePowerProfiles"), which_profile)) {
        goto exit;
    }

    if (!SCPreferencesCommitChanges(energyPrefs))
    {
        // handle error
        if (kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;
        goto exit;
    }
    
    if (!SCPreferencesApplyChanges(energyPrefs))
    {
        // handle error
        if (kSCStatusAccessError == SCError()) ret = kIOReturnNotPrivileged;
        else ret = kIOReturnError;        
        goto exit;
    }

    ret = kIOReturnSuccess;
exit:
    if (energyPrefs) {
        SCPreferencesUnlock(energyPrefs);
        CFRelease(energyPrefs);
    }
    return ret;
}

/*
 * This functions creates assertions required at boot time.
 */
void createOnBootAssertions( )
{
    CFMutableDictionaryRef assertionDescription = NULL;

    int value;
    IOReturn ret;
    /* 
     * For now only 'preventSystemSleep' assertion is created
     * for FindMyMacd if system is booting in wipe mode.
     */
    ret = getNvramArgInt(FMMD_WIPE_BOOT_ARG, &value);
    if (ret == kIOReturnSuccess && value > 0) {
        /* Create 'PreventSystemSleep' assertion */
        assertionDescription = _IOPMAssertionDescriptionCreate(
                        kIOPMAssertionTypePreventSystemSleep,
                        CFSTR("com.apple.powermanagement.fmmdwipe"),
                        NULL, CFSTR("Proxy Assertion during FMMD system wipe"), NULL,
                        120, kIOPMAssertionTimeoutActionRelease);

        /* This assertion should be applied even on battery power */
        CFDictionarySetValue(assertionDescription, 
                 kIOPMAssertionAppliesToLimitedPowerKey, (CFBooleanRef)kCFBooleanTrue);
        InternalCreateAssertion(assertionDescription, NULL);

        CFRelease(assertionDescription);
    }


}
 
/***********************************
 * Dynamic Assertions
 ***********************************/


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static CFDictionaryRef copyAggregateValuesDictionary(void)
{
    CFDictionaryRef                 assertions_info = NULL;
    CFNumberRef                     cf_agg_vals[kIOPMNumAssertionTypes];
    int                             i;
    
    // Massage int values into CFNumbers for CFDictionaryCreate
    for (i=0; i<kIOPMNumAssertionTypes; i++)
    {
        int tmp_bit = LEVEL_FOR_BIT(i);

        cf_agg_vals[i] = CFNumberCreate(0, kCFNumberIntType, &tmp_bit);
    }

    // We return the contents of aggregate_assertions packed into a CFDictionary.
    assertions_info = CFDictionaryCreate(
        0,
        (const void **)assertion_types_arr,     // type: CFStringRef
        (const void **)cf_agg_vals,   // value: CFNumberRef
        kIOPMNumAssertionTypes,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
        
    // Release CFNumbers
    for (i=0; i<kIOPMNumAssertionTypes; i++)
    {
        CFRelease(cf_agg_vals[i]);
    }

    // TODO: strip unsupported assertions?

    return assertions_info;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static CFArrayRef copyTimedOutAssertionsArray(void)
{
    /* Strip out non-serializable private portions of the timed-out assertions */

    if (gTimedOutArray)
    {    
        int                     arrayCount;
        int                     i;
        CFMutableArrayRef       newTimeOutArray;
        newTimeOutArray = CFArrayCreateMutableCopy(0, 0, gTimedOutArray);
        
        arrayCount = CFArrayGetCount(newTimeOutArray);
        for (i = 0; i<arrayCount; i++)
        {
            CFMutableDictionaryRef  assertionCopy = 
                    CFDictionaryCreateMutableCopy(0, 0, CFArrayGetValueAtIndex(newTimeOutArray, i));
    
            CFDictionaryRemoveValue(assertionCopy, kIOPMAssertionTimerRefKey);
    
            CFArraySetValueAtIndex(newTimeOutArray, i, assertionCopy);
            
            CFRelease(assertionCopy);
        }

        return newTimeOutArray;
    }
    return NULL;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void appendTimedOutAssertion(CFDictionaryRef timedout)
{
    int timedoutcount = 0;

    if (!gTimedOutArray) {
        gTimedOutArray = CFArrayCreateMutable(0, kMaxCountTimedOut, &kCFTypeArrayCallBacks);    
    }

    if (!gTimedOutArray) 
        return;

    timedoutcount = CFArrayGetCount(gTimedOutArray);
    if (kMaxCountTimedOut == timedoutcount) {
        // Ensure that we store no more than the kMaxCountTimedOut latest timeouts
        // If we're over our tracking quota, pull the last entry off the end.
        CFArrayRemoveValueAtIndex(gTimedOutArray, kMaxCountTimedOut - 1);
    }

    // Always add new entries onto the front.
    // The array starts with newest assertions at index 0.
    CFArrayInsertValueAtIndex(gTimedOutArray, 0, timedout);
    
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__private_extern__ void _PMAssertionsDriverAssertionsHaveChanged(uint32_t changedDriverAssertions)
{
    notify_post( kIOPMAssertionsChangedNotifyString );
}

/*
static int indexForAssertionName(CFStringRef assertionName) 
{
    if (CFEqual(assertionName, kIOPMAssertionTypeNeedsCPU))
       return kHighPerfIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeNoIdleSleep))
        return kPreventIdleIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeDenySystemSleep))
        return kPreventSleepIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeEnableIdleSleep))
        return kEnableIdleIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeDisableInflow))
        return kDisableInflowIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeInhibitCharging))
        return kInhibitChargeIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeDisableLowBatteryWarnings))
        return kDisableWarningsIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeNoDisplaySleep))
        return kPreventDisplaySleepIndex;
    else if (CFEqual(assertionName, kIOPMAssertionTypeDisableRealPowerSources_Debug))
        return kNoRealPowerSourcesDebugIndex;
    else
        return 0;
}
*/


#pragma mark -
#pragma mark Retain & Release




#pragma mark -
#pragma mark ASL Log

#if !TARGET_OS_EMBEDDED

__private_extern__ void logASLAssertionTypeSummary( kerAssertionType type)
{

     applyToAllAssertionsSync(&gAssertionTypes[type], false, ^(assertion_t *assertion)
             {
                 logASLAssertionEvent(kPMASLAssertionActionSummary, assertion);                    
             });
}

/* logASLAssertionEvent
 *
 * Logs a message describing an assertion event that just occured.
 * - Action = Created/Released/TimedOut/Died/System
 * 
 * Message = "%s(Action) %d(pid) %s(AssertionLocalizableName)(or just the Name) %s(AssertionsHeld=Level)"
 */
static void logASLAssertionEvent(
    const char      *assertionAction,
    assertion_t     *assertion)
{
    const int       kLongStringLen          = 200;
    const int       kShortStringLen         = 10;
    aslmsg          m;
    CFStringRef     foundAssertionType      = NULL;
    CFStringRef     foundAssertionName      = NULL;
    CFDateRef       foundDate               = NULL;
    CFStringRef     procName                = NULL;
    char            proc_name_buf[kProcNameBufLen];
    char            pid_buf[kShortStringLen];
    char            assertionTypeCString[kLongStringLen];
    char            assertionNameCString[kLongStringLen];
    char            ageString[kShortStringLen];
    char            aslMessageString[kLongStringLen];
    CFMutableDictionaryRef assertionDictionary;
    
    if ((gDebugFlags & kIOPMDebugEnableAssertionLogging) == 0) 
        return;

    if (assertionAction == kPMASLAssertionActionCreate) {
        if (assertion->state & kAssertionStateLogged)
           return;
        assertion->state |= kAssertionStateLogged;
    }
    else if ( (assertion->state & kAssertionStateLogged) == 0) {
        return;
    }

    m = asl_new(ASL_TYPE_MSG);
    
    asl_set(m, kMsgTracerDomainKey, kMsgTracerDomainPMAssertions);
    pid_buf[0] = 0;
    
    if (0 < snprintf(pid_buf, kShortStringLen, "%d", assertion->pid)) {
        asl_set(m, kPMASLPIDKey, pid_buf);
    }

    asl_set(m, kPMASLActionKey, assertionAction);

    
    assertionDictionary = assertion->props;
    if (assertionDictionary)
    {
        /* 
         * Log the assertion type:
         */
        foundAssertionType = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionTypeKey);        
        if (foundAssertionType) {
            CFStringGetCString(foundAssertionType, assertionTypeCString, sizeof(assertionTypeCString), kCFStringEncodingUTF8);
            asl_set(m, kPMASLAssertionNameKey, assertionTypeCString);
        }

        foundAssertionName = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionNameKey);
        if (foundAssertionName) {
            CFStringGetCString(foundAssertionName, assertionNameCString, sizeof(assertionNameCString), kCFStringEncodingUTF8);            
        }

        /*
         * Assertion's age
         */
        if ((foundDate = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionCreateDateKey)))
        {
            CFAbsoluteTime createdCFTime    = CFDateGetAbsoluteTime(foundDate);
            int createdSince                = (int)(CFAbsoluteTimeGetCurrent() - createdCFTime);
            int hours                       = createdSince / 3600;
            int minutes                     = (createdSince / 60) % 60;
            int seconds                     = createdSince % 60;
            snprintf(ageString, sizeof(ageString), "%02d:%02d:%02d ", hours, minutes, seconds);
        }

        /*
         * Retain count
         */
        int retainCount = assertion->retainCnt;
        if (1 != retainCount) 
        {
            char    retainCountBuf[kShortStringLen];
            snprintf(retainCountBuf, sizeof(retainCountBuf), "%d", retainCount);
            asl_set(m, "RetainCount", retainCountBuf);
        }
    }

    if ((procName = processInfoGetName(assertion->pid)))
    {
        CFStringGetCString(procName, proc_name_buf, sizeof(proc_name_buf), kCFStringEncodingUTF8);
    }

    
    snprintf(aslMessageString, sizeof(aslMessageString), "PID %s(%s) %s %s %s%s%s %s id:0x%llx Aggregate:0x%x",
                pid_buf,
                procName ? proc_name_buf:"?",
                assertionAction,
                foundAssertionType ? assertionTypeCString:"",
                foundAssertionName?"\"":"", foundAssertionName ? assertionNameCString:"", 
                foundAssertionName?"\"":"",
                foundDate?ageString:"",
                (((uint64_t)assertion->kassert) << 32) | (assertion->assertionId),
                ((kerAssertionBits & 0xfff) << 16) | (aggregate_assertions & 0xffff));

    asl_set(m, ASL_KEY_MSG, aslMessageString);    
    
    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);    

    /* Facility = "internal"
     * is required to make sure this message goes into ASL database, but not
     * into system.log
     */
    asl_set(m, ASL_KEY_FACILITY, "internal");

    asl_set(m, kPMASLMessageKey, kPMASLMessageLogValue);

    asl_send(NULL, m);

    asl_free(m);
}

static void 
logASLAssertionsAggregate( )
{
    aslmsg m;
    char            aslMessageString[100];

    m = asl_new(ASL_TYPE_MSG);
    asl_set(m, kMsgTracerDomainKey, kMsgTracerDomainPMAssertions);
    asl_set(m, kPMASLActionKey, kPMASLAssertionActionSummary);

    snprintf(aslMessageString, sizeof(aslMessageString), "Summary- Aggregate:0x%x Using %s", 
                ((kerAssertionBits & 0xfff) << 16) | (aggregate_assertions & 0xffff),
                  ( _getPowerSource() == kBatteryPowered) ? "Batt" : "AC");

    asl_set(m, ASL_KEY_MSG, aslMessageString);    
    
    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);    

    /* Facility = "internal"
     * is required to make sure this message goes into ASL database, but not
     * into system.log
     */
    asl_set(m, ASL_KEY_FACILITY, "internal");

    asl_set(m, kPMASLMessageKey, kPMASLMessageLogValue);

    asl_send(NULL, m);

    asl_free(m);

}


#endif

#define     kDarkWakeNetworkHoldForSeconds          30
#define     kDeviceEnumerationHoldForSeconds        (45LL)

#pragma mark -
#pragma mark powerd-Internal Use Only


__private_extern__ CFMutableDictionaryRef	_IOPMAssertionDescriptionCreate(
    CFStringRef AssertionType, 
    CFStringRef Name, 
    CFStringRef Details,
    CFStringRef HumanReadableReason,
    CFStringRef LocalizationBundlePath,
    CFTimeInterval Timeout,
    CFStringRef TimeoutBehavior)
{
    CFMutableDictionaryRef  descriptor = NULL;
    
    if (!AssertionType || !Name) {
        return NULL;
    }
    
    descriptor = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!descriptor) {
        return NULL;
    }

    CFDictionarySetValue(descriptor, kIOPMAssertionNameKey, Name);

    int _on = kIOPMAssertionLevelOn;
    CFNumberRef _on_num = CFNumberCreate(0, kCFNumberIntType, &_on);
    CFDictionarySetValue(descriptor, kIOPMAssertionLevelKey, _on_num);
    CFRelease(_on_num);

    CFDictionarySetValue(descriptor, kIOPMAssertionTypeKey, AssertionType);

    if (Details) {
        CFDictionarySetValue(descriptor, kIOPMAssertionDetailsKey, Details);
    }
    if (HumanReadableReason) {
        CFDictionarySetValue(descriptor, kIOPMAssertionHumanReadableReasonKey, HumanReadableReason);
    }
    if (LocalizationBundlePath) {
        CFDictionarySetValue(descriptor, kIOPMAssertionLocalizationBundlePathKey, LocalizationBundlePath);
    }
    if (Timeout) {
        CFNumberRef Timeout_num = CFNumberCreate(0, kCFNumberDoubleType, &Timeout);
        CFDictionarySetValue(descriptor, kIOPMAssertionTimeoutKey, Timeout_num);
        CFRelease(Timeout_num);
    }
    if (TimeoutBehavior)
    {
        CFDictionarySetValue(descriptor, kIOPMAssertionTimeoutActionKey, TimeoutBehavior);
    }

    return descriptor;
}



__private_extern__ IOReturn InternalCreateAssertion(
    CFMutableDictionaryRef properties, 
    IOPMAssertionID *outID)
{
    if (!properties) 
       return kIOReturnBadArgument;

    CFRetain(properties);

    CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{
        if (outID == NULL) {
            /* Some don't care for assertionId */
            IOPMAssertionID   assertionID = kIOPMNullAssertionID;
            doCreate(getpid(), properties, &assertionID);    
        }
        else if ( *outID == kIOPMNullAssertionID )
            doCreate(getpid(), properties, outID);    

        CFRelease(properties);
    });
    CFRunLoopWakeUp(_getPMRunLoop());
    
    return kIOReturnSuccess;
}

__private_extern__ void InternalReleaseAssertion(
                                                 IOPMAssertionID *outID)
{
    CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{
        if ( *outID != kIOPMNullAssertionID ) {
            doRelease(getpid(), *outID);
        }
        *outID = kIOPMNullAssertionID;
    });
    CFRunLoopWakeUp(_getPMRunLoop());
}

__private_extern__ void InternalEvaluateAssertions(void)
{
    CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{
        evaluateAssertions();
    });
    CFRunLoopWakeUp(_getPMRunLoop());
    
}

static IOReturn _localCreateAssertionWithTimer(
        CFStringRef type, IOPMAssertionLevel level, CFStringRef name, int timerSecs, IOPMAssertionID *outID)
{
    CFMutableDictionaryRef          dict = NULL;
    CFNumberRef                     levelNum = NULL;
    CFNumberRef                     timerNum = NULL;
    
    if (!type || !name || !outID) {
        return kIOReturnBadArgument;
    }
    
    if ((dict = CFDictionaryCreateMutable(0, 0, 
           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
    {
        if ((levelNum = CFNumberCreate(0, kCFNumberIntType, &level)))
        {
            CFDictionarySetValue(dict, kIOPMAssertionLevelKey, levelNum);
            CFRelease(levelNum);
        }

        if (timerSecs && (timerNum = CFNumberCreate(0, kCFNumberIntType, &timerSecs)) )
        {
            CFDictionarySetValue(dict, kIOPMAssertionTimeoutKey, timerNum);
            CFRelease(timerNum);

            CFDictionarySetValue(dict, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);
        }
        
        CFDictionarySetValue(dict, kIOPMAssertionTypeKey, type);
        CFDictionarySetValue(dict, kIOPMAssertionNameKey, name);

        doCreate(getpid(), dict, outID);
        
        CFRelease(dict);
    }

    return kIOReturnSuccess;

}
static IOReturn _localCreateAssertion(CFStringRef type, IOPMAssertionLevel level, CFStringRef name, IOPMAssertionID *outID)
{
    return _localCreateAssertionWithTimer(type, level, name, 0, outID);
}

static IOReturn _enableAssertionForLimitedPower(pid_t pid, IOPMAssertionID id)
{
    CFMutableDictionaryRef          dict = NULL;
    IOReturn                        rc = kIOReturnError;
    
    if (!id)
        return kIOReturnBadArgument;
  
    if ((dict = CFDictionaryCreateMutable(0, 0, 
           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
    {
        CFDictionarySetValue(dict, kIOPMAssertionAppliesToLimitedPowerKey, kCFBooleanTrue);

        rc = doSetProperties(pid, id, dict);

        CFRelease(dict);
    }

    return rc;
}


static void __DarkWakeHandleTeardownCallback(CFRunLoopTimerRef  timer __unused, void *info)
{
    IOPMAssertionID     tearItDown = (int)(uintptr_t)info;

    doRelease(getpid(), tearItDown);
}

/* _DarkWakeHandleNetworkWake runs when PM enters dark wake via network packet.
 * It creates a temporary assertion that keeps the system awake hopefully long enough
 * for a remote client to connect, and for the server to create an assertion
 * keeping the system awak.
 */
static void  _DarkWakeHandleNetworkWake(void)
{
    IOPMAssertionID     darkWakeNetworkAssertion = kIOPMNullAssertionID;
    
    if (kIOReturnSuccess == _localCreateAssertion(kIOPMAssertionTypePreventSystemSleep, kIOPMAssertionLevelOn, 
                                                  CFSTR("PM configd - network wake delay"), &darkWakeNetworkAssertion))
    {

        /* This assertion can only last for 30 seconds. 
         * Let's start a timer and tear it down after that. 
         */
        CFRunLoopTimerContext   timerArgument;
        CFRunLoopTimerRef       timerTime = NULL;
        
        bzero(&timerArgument, sizeof(timerArgument));
        timerArgument.info = (void *)(uintptr_t)darkWakeNetworkAssertion;
        timerTime = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + (double)kDarkWakeNetworkHoldForSeconds, 
                                         0.0, 0, 0, __DarkWakeHandleTeardownCallback, &timerArgument);
        if (timerTime) {
            CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerTime, kCFRunLoopDefaultMode);
            CFRelease(timerTime);
        }
    }

    return ;
}



typedef struct notifyRegInfo {
    IONotificationPortRef port;
    io_object_t handle;
} notifyRegInfo_st;

static void _DeregisterForNotification(
        notifyRegInfo_st *notifyInfo)
{
    if (notifyInfo->handle)
        IOObjectRelease(notifyInfo->handle);

    if (notifyInfo->port)
        IONotificationPortDestroy(notifyInfo->port);

    if (notifyInfo)
        free(notifyInfo);
}

/*
 * Register for specific interestType with service at specified 'path' */
static notifyRegInfo_st * _RegisterForNotification( 
        const io_string_t	path, const io_name_t 	interestType,
        IOServiceInterestCallback callback, void *refCon)
{

    io_service_t	obj = MACH_PORT_NULL;
    notifyRegInfo_st  *notifyInfo = NULL;
    kern_return_t kr;

    notifyInfo = calloc(sizeof(notifyRegInfo_st), 1);
    if ( !notifyInfo )
        goto exit;

    notifyInfo->port = IONotificationPortCreate( kIOMasterPortDefault );
    if ( !notifyInfo->port ) 
        goto exit;
    
    obj = IORegistryEntryFromPath( kIOMasterPortDefault, path);
    if ( !obj )
        goto exit;

    kr = IOServiceAddInterestNotification(
            notifyInfo->port,
            obj, interestType,
            callback, refCon,
            &notifyInfo->handle );

    if (kr !=  KERN_SUCCESS)
        goto exit;


    IOObjectRelease(obj);
    return  notifyInfo;

exit:
    if (notifyInfo->handle)
        IOObjectRelease(notifyInfo->handle);

    if (notifyInfo->port)
        IONotificationPortDestroy(notifyInfo->port);

    if (obj)
        IOObjectRelease(obj);

    if (notifyInfo)
        free(notifyInfo);

    return NULL;
}

typedef struct devEnumInfo {
    dispatch_source_t   dispSrc; /* Dispatched 5sec after IOKit is quiet */
    bool                suspended; /* true if 'dispSrc' is suspended */
    dispatch_source_t   dispSrc2; /* Dispatched 45sec after assertion is created */
    notifyRegInfo_st    *notifyInfo;
    IOPMAssertionID assertId;
}devEnumInfo_st;

/*
 * Function that releases the assertion and cleans up all dispatch queues */
static void devEnumerationDone( devEnumInfo_st *deInfo )
{

    /* First cancel the dispatch sources */
    if (deInfo->dispSrc && (dispatch_source_testcancel(deInfo->dispSrc) == 0)) {
        dispatch_source_cancel(deInfo->dispSrc);
    }

    if (deInfo->dispSrc2 && (dispatch_source_testcancel(deInfo->dispSrc2) == 0)) {
        dispatch_source_cancel(deInfo->dispSrc2);
    }
    /* De-register from IOkit busy state updates */
    if (deInfo->notifyInfo) {
        _DeregisterForNotification(deInfo->notifyInfo);
        deInfo->notifyInfo = 0;
    }

    /* Release the assertion */
    if (deInfo->assertId) {
        doRelease(getpid(), deInfo->assertId);
        deInfo->assertId = 0;
        free(deInfo);
    }
}

void ioKitStateCallback (
	void *			refcon,
	io_service_t		service,
	uint32_t		messageType,
	void *			messageArgument ) 
{
    devEnumInfo_st *deInfo = (devEnumInfo_st *)refcon;
    long state = (long)messageArgument;
    dispatch_source_t dispSrc;


    if (messageType != kIOMessageServiceBusyStateChange)
        return;

    if (state) {
        /* IOKit is busy. Suspend the timer until the Iokit is free */
        if (deInfo->dispSrc && !deInfo->suspended) {
            dispatch_suspend(deInfo->dispSrc);
            deInfo->suspended = true;
        }
    }
    else {
        /* 
         * IOkit is free. Create/extend a timer to dispatch a function 
         * that can release the device enumeration assertion.
         */
        if (deInfo->dispSrc == 0) {
            dispSrc  = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 
                                                        0, dispatch_get_main_queue());

            dispatch_source_set_event_handler(dispSrc, ^{
                devEnumerationDone(deInfo);
            });

            dispatch_source_set_cancel_handler(dispSrc, ^{
                dispatch_release(dispSrc);
            });

            deInfo->dispSrc = dispSrc;
            deInfo->suspended = true;
        }
        if (deInfo->suspended) {
           dispatch_source_set_timer(deInfo->dispSrc, dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC), 
                                                DISPATCH_TIME_FOREVER, 0);
           dispatch_resume(deInfo->dispSrc);
        }

        deInfo->suspended = false;
    }


}


static void _AssertForDeviceEnumeration( )
{
    IOPMAssertionID     deviceEnumerationAssertion = kIOPMNullAssertionID;
    notifyRegInfo_st    *notifyInfo = NULL;
    devEnumInfo_st *deInfo = NULL;
    IOReturn rc;
    dispatch_source_t dispSrc;
        
    rc = _localCreateAssertion(kIOPMAssertionTypePreventSystemSleep, kIOPMAssertionLevelOn, 
                                CFSTR("PM configd - Wait for Device enumeration"), 
                                &deviceEnumerationAssertion);
    if (rc != kIOReturnSuccess)
        return;

    /* Enable this assertion on battery power also */
    _enableAssertionForLimitedPower(getpid(), deviceEnumerationAssertion);

    deInfo = calloc(sizeof(devEnumInfo_st), 1);
    if (!deInfo) {
        goto exit;
    }

    /* Register IOService Busy/free notifications */
    notifyInfo = _RegisterForNotification(kIOServicePlane ":/", 
                        kIOBusyInterest, ioKitStateCallback, (void *)deInfo);
    if ( !notifyInfo ) {
        /* Failed to register for notification. Remove the assertion immediately */
        goto exit;
    }

    deInfo->notifyInfo = notifyInfo;
    deInfo->assertId = deviceEnumerationAssertion;

    /* 
     * Create a higher level timer dispatch, which guarantees that assertion is 
     * released irrespective of the IOkit busy/quiet state.
     */

    dispSrc = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 
                                                0, dispatch_get_main_queue());
    dispatch_source_set_timer(dispSrc, 
                                dispatch_time(DISPATCH_TIME_NOW, kDeviceEnumerationHoldForSeconds * NSEC_PER_SEC), 
                                DISPATCH_TIME_FOREVER, 0);

    dispatch_source_set_event_handler(dispSrc, ^{
        devEnumerationDone(deInfo);
    });

    dispatch_source_set_cancel_handler(dispSrc, ^{
        dispatch_release(dispSrc);
    });

    deInfo->dispSrc2 = dispSrc;
    dispatch_resume(deInfo->dispSrc2);

        
    IONotificationPortSetDispatchQueue(notifyInfo->port, dispatch_get_main_queue());

    return;

exit:
    if (deInfo) 
        free(deInfo);

    if (notifyInfo)
        _DeregisterForNotification(notifyInfo);

    doRelease(getpid(), deviceEnumerationAssertion); 
}

#define IS_DARK_STATE(cap)      ( (cap & kIOPMSystemCapabilityCPU) && !(cap & kIOPMSystemCapabilityGraphics) )

#define IS_OFF_STATE(cap)       ( 0 == (cap & kIOPMSystemCapabilityCPU) )


/*
 * Create assertions for a short period on behalf of other modules/processes. This is to
 * give a chance to those modules to create any required assertions on their own.
 * Otherwise, system may go back to sleep before those modules got a chance to
 * complete their processing.
 */
__private_extern__ void _ProxyAssertions(const struct IOPMSystemCapabilityChangeParameters *capArgs)
{

    CFStringRef         wakeType = NULL;
    CFStringRef         wakeReason = NULL;
    IOPMAssertionID     pushSvcAssert = kIOPMNullAssertionID;

    if ( !(kIOPMSystemCapabilityDidChange & capArgs->changeFlags) )
       return;


    if ( IS_DARK_STATE(capArgs->toCapabilities) &&
          IS_OFF_STATE(capArgs->fromCapabilities) )
    {
        wakeType = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeTypeKey));
        wakeReason = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeReasonKey));

        if (isA_CFString(wakeReason) && CFEqual(wakeReason, kIORootDomainWakeReasonDarkPME))
        {
             _AssertForDeviceEnumeration( );
        }
        else if (isA_CFString(wakeType))
        {
            if (CFEqual(wakeType, kIOPMRootDomainWakeTypeNetwork))
                _DarkWakeHandleNetworkWake( );
            else if (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) &&
                  CFEqual(wakeReason, kIOPMRootDomainWakeReasonRTC))
            {
                _localCreateAssertionWithTimer(kIOPMAssertionTypeBackgroundTask, kIOPMAssertionLevelOn,
                                    CFSTR("Powerd - Wait for client BackgroundTask assertions"), 10, &pushSvcAssert);
            }
            else if ((CFEqual(wakeType, kIOPMRootDomainWakeTypeAlarm) == false ) &&
                     (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) == false ) &&
                     (CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepTimer) == false ) &&
                     (CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService) == false ) &&
                     (CFEqual(wakeType, kIOPMrootDomainWakeTypeLowBattery) == false ) )
            {
                /* 
                 * If this is not a Timer based wake, raise assertion and wait for 
                 * devices to enumerate.
                 */
                _AssertForDeviceEnumeration( );
            }
        }
        else 
        {
            /* 
             * Wake type is not set for some cases. Only wake reason is set.
             * Plugging a new USB device while sleeping is one such case
             */
            _AssertForDeviceEnumeration( );
        }

        if (wakeType) {
            CFRelease(wakeType);
        }
        if (wakeReason)
           CFRelease(wakeReason);
    }

}


static bool propertiesDictRequiresRoot(CFDictionaryRef props)
{
    if ( CFDictionaryGetValue(props, kIOPMInflowDisableAssertion)
      || CFDictionaryGetValue(props, kIOPMChargeInhibitAssertion) )
    {
        return true;
    } else {
        return false;
    }
}


typedef struct {
    CFStringRef             name;
    dispatch_source_t       disp_src;
} ProcessInfoStruct;

/* Unwrap a pointer stored in a CFData, so we can place it an a CF container.
 */
//static void dictionarySetDispatchSource(CFMutableDictionaryRef d, const void *k, dispatch_source_t p)
static void processInfoCreate(pid_t p, dispatch_source_t d)
{
    ProcessInfoStruct       proc;
    CFDataRef               tmpData;
    char                    name[kProcNameBufLen];
    
    proc.disp_src = d;
    proc_name(p, name, sizeof(name));
    proc.name = CFStringCreateWithCString(0, name, kCFStringEncodingUTF8);
    
    tmpData= CFDataCreate(0, (uint8_t *)(&proc), sizeof(ProcessInfoStruct));
    if (tmpData) {
        CFDictionarySetValue(gProcessDict, (const void *)p, tmpData);
        CFRelease(tmpData);
    }
}

/* Wrap a pointer to a non-CF object inside a CFData, so that 
 * we can place it an a CF container.
 */

static bool processInfoRetain(pid_t p)
{    
    CFDataRef tmpData = CFDictionaryGetValue(gProcessDict, (const void *)p);
    
    if (tmpData) {
        CFRetain(tmpData);
        return true;
    }
    
    return false;
}

CFStringRef processInfoGetName(pid_t p)
{
    CFDataRef           tmpData = CFDictionaryGetValue(gProcessDict, (const void *)p);
    ProcessInfoStruct   *proc = NULL;
    CFStringRef         retString = NULL;
    
    if (tmpData && (proc = (ProcessInfoStruct *)CFDataGetBytePtr(tmpData))) {
        retString = proc->name;
    }
    
    return retString;
}

static void processInfoRelease(pid_t p)
{
    ProcessInfoStruct   *proc = NULL;
    CFIndex cnt;
    
    CFDataRef tmpData = CFDictionaryGetValue(gProcessDict, (const void *)p);
    
    if (!tmpData) return;

    cnt = CFGetRetainCount(tmpData);


    if (cnt == 1) {
        proc = (ProcessInfoStruct *)CFDataGetBytePtr(tmpData);

        dispatch_release(proc->disp_src);
        CFRelease(proc->name);
        CFDictionaryRemoveValue(gProcessDict, (const void *)p);
    }
    else {
        CFRelease(tmpData);
    }
    return ;
}


static IOReturn lookupAssertion(pid_t pid, IOPMAssertionID id, assertion_t **assertion)
{
    unsigned int idx = INDEX_FROM_ID(id);
    assertion_t  *tmp_a = NULL;

    if (idx >= kMaxAssertions)
        return kIOReturnBadArgument;

    if( CFDictionaryGetValueIfPresent(gAssertionsArray, 
                  (void *)idx, (const void **)&tmp_a) == false)
        return kIOReturnBadArgument;

    if (tmp_a->pid != pid)
        return kIOReturnNotPermitted;

    *assertion = tmp_a;
    return kIOReturnSuccess;

}


kern_return_t  _io_pm_declare_user_active
(   
    mach_port_t             server  __unused,
    audit_token_t           token,
    int                     user_type,
    vm_offset_t             props,
    mach_msg_type_number_t  propsCnt,
    int                     *assertion_id,
    int                     *return_code
)
{

    CFMutableDictionaryRef      assertionProperties = NULL;
    assertion_t      *assertion = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    IOReturn            ret;
    bool                create_new = true;
    uint32_t            displaySleepTimerSecs;
    CFNumberRef         CFdisplaySleepTimer = NULL;
    CFDateRef           start_date = NULL;
    
    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);    
        
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        assertionProperties = (CFMutableDictionaryRef)CFPropertyListCreateWithData(0, unfolder, 
                                            kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
        CFRelease(unfolder);
    }
    
    if (!assertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    /* Set the assertion timeout value to display sleep timer value, if it is not 0 */

    displaySleepTimerSecs = gDisplaySleepTimer * 60; /* Convert to secs */
    CFdisplaySleepTimer = CFNumberCreate(0, kCFNumberIntType, &displaySleepTimerSecs);
    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutKey, CFdisplaySleepTimer);
    CFRelease(CFdisplaySleepTimer);

    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);

    /* Check if this is a repeat call on previously returned assertion id */
    do {
        if (assertion_id == NULL || *assertion_id == kIOPMNullAssertionID) 
            break;

        ret = lookupAssertion(callerPID, *assertion_id, &assertion);
        if ((kIOReturnSuccess != ret) || !assertion) 
            break;

        if (assertion->kassert != kDeclareUserActivity)
            break;

        /* Extend the timeout timer of this assertion by display sleep timer value */

        /* First set the assertion level to ON */
        int k = kIOPMAssertionLevelOn;
        CFNumberRef useLevelOnNum = CFNumberCreate(0, kCFNumberIntType, &k);
        CFDictionarySetValue(assertion->props, kIOPMAssertionLevelKey, useLevelOnNum);
        CFRelease(useLevelOnNum);

        /* Update the Create Time */
        start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        if (start_date) {
            CFDictionarySetValue(assertionProperties, kIOPMAssertionCreateDateKey, start_date);
            CFRelease(start_date);
        }

        *return_code = doSetProperties(callerPID, *assertion_id, assertionProperties);
        create_new = false;
    } while (false);

    if (create_new) {
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionUserIsActive);

        *return_code = doCreate(callerPID, assertionProperties, (IOPMAssertionID *)assertion_id);
 
    }

exit:


    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);
    return KERN_SUCCESS;
}


static uint64_t getMonotonicTime( )
{
    static mach_timebase_info_data_t    timebaseInfo;

    if (timebaseInfo.denom == 0) 
        mach_timebase_info(&timebaseInfo);

    return ( (mach_absolute_time( ) * timebaseInfo.numer) / (timebaseInfo.denom * NSEC_PER_SEC));
}

void insertInactiveAssertion(assertion_t *assertion, assertionType_t *assertType) 
{
    LIST_INSERT_HEAD(&assertType->inactive, assertion, link);
    assertion->state &= ~kAssertionStateTimed;
    assertion->state |= kAssertionStateInactive;
}

void removeInactiveAssertion(assertion_t *assertion, assertionType_t *assertType)
{
    LIST_REMOVE(assertion, link);
    assertion->state &= ~kAssertionStateInactive;
}

void insertActiveAssertion(assertion_t *assertion, assertionType_t *assertType)
{
    LIST_INSERT_HEAD(&assertType->active, assertion, link);
    assertion->state &= ~(kAssertionStateTimed|kAssertionStateInactive);

    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) &&
            (assertion->state & kAssertionStateValidOnBatt) )
            assertType->validOnBattCount++;

}

void removeActiveAssertion(assertion_t *assertion, assertionType_t *assertType)
{
    LIST_REMOVE(assertion, link);

    if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
            assertType->validOnBattCount--;
}


void resetAssertionTimer(assertionType_t *assertType)
{
    uint64_t currTime, timeout ;
    assertion_t *nextAssertion = NULL;

    nextAssertion = LIST_FIRST(&assertType->activeTimed);
    if (!nextAssertion) return;

    timeout = nextAssertion->timeout;
    currTime = getMonotonicTime();

    if (nextAssertion->timeout <= currTime) {
        CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{ handleAssertionTimeout(assertType); });
        CFRunLoopWakeUp(_getPMRunLoop());
    }
    else {
        dispatch_suspend(assertType->timer);
        
        dispatch_source_set_timer(assertType->timer, 
                dispatch_time(DISPATCH_TIME_NOW, (nextAssertion->timeout-currTime)*NSEC_PER_SEC), 
                DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(assertType->timer);
    }

}


static void releaseAssertionMemory(assertion_t *assertion)
{
    int idx = INDEX_FROM_ID(assertion->assertionId);
    assertion_t *tmp_a = NULL;
    
    if ( (idx < 0) || (idx >= kMaxAssertions) || 
          (CFDictionaryGetValueIfPresent(gAssertionsArray, 
                          (void *)idx, (const void **)&tmp_a) == false) || (tmp_a != assertion) ) {
#ifdef DEBUG
        abort();
#endif
        return; // This is an error
    }

    logASLAssertionEvent(kPMASLAssertionActionRelease, assertion);
    CFDictionaryRemoveValue(gAssertionsArray, (void *)idx);
    if (assertion->props) CFRelease(assertion->props);

    free(assertion);

    processInfoRelease(assertion->pid);
}

void handleAssertionTimeout(assertionType_t *assertType)
{
    assertion_t     *assertion;
    CFDateRef       dateNow = NULL;
    uint64_t        currtime = getMonotonicTime( );
    uint32_t        timedoutCnt = 0;
    CFStringRef     timeoutAction = NULL;

    while( (assertion = LIST_FIRST(&assertType->activeTimed)) )
    {
        if (assertion->timeout > currtime) {
            assertion = NULL;
            break;
        }
        timedoutCnt++;

        LIST_REMOVE(assertion, link);
        assertion->state &= ~kAssertionStateTimed;

        if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
                assertType->validOnBattCount--;


        if ((dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent()))) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionTimedOutDateKey, dateNow);            
            CFRelease(dateNow);
        }

        // Put a copy of this assertion into our "timeouts" array.        
        appendTimedOutAssertion(assertion->props);


        logASLAssertionEvent(kPMASLAssertionActionTimeOut, assertion);

        timeoutAction = CFDictionaryGetValue(assertion->props, kIOPMAssertionTimeoutActionKey);
        if (isA_CFString(timeoutAction) && CFEqual(kIOPMAssertionTimeoutActionRelease, timeoutAction))
        { 
            releaseAssertionMemory(assertion);
        }
        else /* Default timeout action is to turn off */
        {
            // Leave this in the inactive assertions list
            insertInactiveAssertion(assertion, assertType);
            if (isA_CFString(timeoutAction) && CFEqual(kIOPMAssertionTimeoutActionKillProcess, timeoutAction))
            {
                kill(assertion->pid, SIGTERM);
            }

        }

    }

    if ( !timedoutCnt ) return;

    resetAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);

    logASLAssertionsAggregate();
    notify_post( kIOPMAssertionTimedOutNotifyString );
    notify_post( kIOPMAssertionsAnyChangedNotifyString );

}

void removeTimedAssertion(assertion_t *assertion, assertionType_t *assertType)
{
    bool adjustTimer = false;

    if (LIST_FIRST(&assertType->activeTimed) == assertion) {
        adjustTimer = true;
    }
    LIST_REMOVE(assertion, link);
    assertion->state &= ~kAssertionStateTimed;

    if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
            assertType->validOnBattCount--;

    if (adjustTimer) resetAssertionTimer(assertType);

}

void updateAssertionTimer(assertionType_t *assertType)
{
    uint64_t    currTime;
    assertion_t *assertion = NULL;

    if ((assertion = LIST_FIRST(&assertType->activeTimed)) == NULL) return;

    /* Update/create the dispatch timer.  */
    if (assertType->timer == NULL) {
        assertType->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());

        dispatch_source_set_event_handler(assertType->timer, ^{
            handleAssertionTimeout(assertType);
        });
    
        dispatch_source_set_cancel_handler(assertType->timer, ^{
            dispatch_release(assertType->timer);
        });
    
    }
    else {
        dispatch_suspend(assertType->timer);
    }
    currTime = getMonotonicTime();


    if (assertion->timeout <= currTime) {
        /* This has already timed out. */
        dispatch_resume(assertType->timer);
        CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{ handleAssertionTimeout(assertType); });
        CFRunLoopWakeUp(_getPMRunLoop());
    }
    else {
        dispatch_source_set_timer(assertType->timer, 
                dispatch_time(DISPATCH_TIME_NOW, (assertion->timeout-currTime)*NSEC_PER_SEC), 
                DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(assertType->timer);
    }


}

void insertTimedAssertion(assertion_t *assertion, assertionType_t *assertType, bool updateTimer)
{
    assertion_t *a, *prev;

    if (LIST_EMPTY(&assertType->activeTimed) ) {
        LIST_INSERT_HEAD(&assertType->activeTimed, assertion, link);
    }
    else {
        LIST_FOREACH(a, &assertType->activeTimed, link) 
        {
            prev = a;
            if (a->timeout > assertion->timeout)
                break;
        }
        if (a)
            LIST_INSERT_BEFORE(a, assertion, link);
        else
            LIST_INSERT_AFTER(prev, assertion, link);
    }

    assertion->state |= kAssertionStateTimed;
    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) &&
            (assertion->state & kAssertionStateValidOnBatt) )
            assertType->validOnBattCount++;
    /* 
     * If this assertion is not the one with earliest timeout,
     * there is nothing to do.
     */
    if (LIST_FIRST(&assertType->activeTimed) != assertion)
        return;

    if (updateTimer) updateAssertionTimer(assertType);

    return;
}


static void releaseAssertion(assertion_t *assertion, bool callHandler)
{
    assertionType_t     *assertType;

    assertType = &gAssertionTypes[assertion->kassert]; 

    if (assertion->state & kAssertionStateTimed)
        removeTimedAssertion(assertion, assertType);
    else if (assertion->state & kAssertionStateInactive)
        removeInactiveAssertion(assertion, assertType);
    else
        removeActiveAssertion(assertion, assertType);

    if (!callHandler) return;

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);


}

static IOReturn doRelease(pid_t pid, IOPMAssertionID id)
{
    IOReturn                    ret;

    assertion_t    *assertion = NULL;
    ret = lookupAssertion(pid, id, &assertion);
    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    if (assertion->retainCnt)
        assertion->retainCnt--;

    if (assertion->retainCnt) {
        return kIOReturnSuccess;
    }

    releaseAssertion(assertion, true);
    releaseAssertionMemory(assertion);

    notify_post( kIOPMAssertionsAnyChangedNotifyString );

    return kIOReturnSuccess;
}


__private_extern__ void
applyToAllAssertionsSync(assertionType_t *assertType, bool applyToInactives,  void (^performOnAssertion)(assertion_t *))
{
    assertion_t *assertion, *nextAssertion;

    assertion = LIST_FIRST(&assertType->active);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        performOnAssertion(assertion);
        assertion = nextAssertion;
    }

    assertion = LIST_FIRST(&assertType->activeTimed);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        performOnAssertion(assertion);
        assertion = nextAssertion;
    }

    if ( !applyToInactives) return; 

    assertion = LIST_FIRST(&assertType->inactive);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        performOnAssertion(assertion);
        assertion = nextAssertion;
    }

}

__private_extern__ void
HandleProcessExit(pid_t deadPID)
{
    int i;
    assertionType_t *assertType = NULL;
    assertion_t     *assertion = NULL;
    __block LIST_HEAD(, assertion) list  = LIST_HEAD_INITIALIZER(list);     /* list of assertions released */

    /* Go thru each assertion type and release all assertion from its lists */
    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertType = &gAssertionTypes[i]; 

        applyToAllAssertionsSync(assertType, true, ^(assertion_t *assertion)
                {
                    if (assertion->pid == deadPID) {
                        releaseAssertion(assertion, false);
                        LIST_INSERT_HEAD(&list, assertion, link);
                    }
                });

        if (assertType->handler)
            (*assertType->handler)(assertType, kAssertionOpRelease);

        /* Release memory after calling the handler to get proper aggregate_assertions value into log */
        while( (assertion = LIST_FIRST(&list)) )
        {
            LIST_REMOVE(assertion, link);
            releaseAssertionMemory(assertion);
        }

    }
    notify_post( kIOPMAssertionsAnyChangedNotifyString );
}


static int getAssertionTypeIndex(CFStringRef type)
{
    int idx = -1;
    CFNumberRef numRef = NULL;

    if (!isA_CFString(type))
        return -1;

    numRef = CFDictionaryGetValue(gUserAssertionTypesDict, type);
    if (isA_CFNumber(numRef))
        CFNumberGetValue(numRef, kCFNumberIntType, &idx);

    if (idx < 0 || idx >= kIOPMNumAssertionTypes)
        return -1;

    return idx;
}
static void forwardPropertiesToAssertion(const void *key, const void *value, void *context)
{
    assertion_t *assertion = (assertion_t *)context;
    assertionType_t *assertType = NULL;
    int level, idx;

    if (!isA_CFString(key))
        return; /* Key has to be a string */


    if (CFEqual(key, kIOPMAssertionLevelKey)) {
        if (!isA_CFNumber(value)) return;
        CFNumberGetValue(value, kCFNumberIntType, &level);
        if ( (assertion->state & kAssertionStateInactive) && (level == kIOPMAssertionLevelOn) )
        {
            assertion->state &= ~kAssertionStateInactive;
            assertion->mods |= kAssertionModLevel;
        }
        else if ( !(assertion->state & kAssertionStateInactive) && (level == kIOPMAssertionLevelOff) )
        {
            assertion->state |= kAssertionStateInactive;
            assertion->mods |= kAssertionModLevel;
        }
    }
    else if (CFEqual(key, kIOPMAssertionTimeoutKey)) {
        if (!isA_CFNumber(value)) return;
        CFNumberGetValue(value, kCFNumberDoubleType, &assertion->timeout);

        if (assertion->timeout) {
            assertion->timeout += getMonotonicTime(); // Absolute time at which assertion expires
        }

        assertion->mods |= kAssertionModTimer;
    }
    else if (CFEqual(key, kIOPMAssertionAppliesToLimitedPowerKey)) {
        if (!isA_CFBoolean(value)) return;
        assertType = &gAssertionTypes[assertion->kassert];
        if ((assertType->flags & kAssertionTypeNotValidOnBatt) == 0) return;
        if ((value == kCFBooleanTrue) && !(assertion->state & kAssertionStateValidOnBatt))
        {
            assertType->validOnBattCount++;
            assertion->state |= kAssertionStateValidOnBatt;
            assertion->mods |= kAssertionModPowerConstraint;
        }
        else if ((value == kCFBooleanFalse) && (assertion->state & kAssertionStateValidOnBatt) )
        {
            if (assertType->validOnBattCount) assertType->validOnBattCount--;
            assertion->state &= ~kAssertionStateValidOnBatt;
            assertion->mods |= kAssertionModPowerConstraint;
        }

    }
    else if (CFEqual(key, kIOPMAssertionTypeKey)) {
        idx = getAssertionTypeIndex(value);
        if ( (idx < 0) || (idx == assertion->kassert) )return;

        assertion->kassert = idx;
        assertion->state |= kAssertionModType;

    }

    CFDictionarySetValue(assertion->props, key, value);

    
}

static IOReturn doSetProperties(pid_t pid, 
                                IOPMAssertionID id, 
                                CFDictionaryRef inProps)
{
    assertion_t                 *assertion = NULL;
    assertionType_t             *assertType;
    uint32_t                    oldState;

    IOReturn                    ret;
    
    // doSetProperties doesn't handle retain()/release() count. 
    // Callers should use IOPMAssertionRetain() or IOPMAssertionRelease().
    
    ret = lookupAssertion(pid, id, &assertion);
    
    if ((kIOReturnSuccess != ret)) {
        return ret;
    }
    
    assertion->mods = 0;
    assertType = &gAssertionTypes[assertion->kassert];
    oldState = assertion->state;
    CFDictionaryApplyFunction(inProps, forwardPropertiesToAssertion,
                assertion);

    if (assertion->mods & kAssertionModType) 
    {
        /* Release the assertion completely */
        /* And add it again as if this is a new assertion */
        releaseAssertion(assertion, true);
        assertion->state = 0;
        assertion->kassert = 0;
        raiseAssertion(assertion);
        notify_post( kIOPMAssertionsAnyChangedNotifyString );
        return kIOReturnSuccess;
    }

    if (assertion->mods & kAssertionModLevel) 
    {
        if (assertion->state & kAssertionStateInactive) 
        {
            /* Remove from active or activeTimed list */
            if (oldState & kAssertionStateTimed) 
                removeTimedAssertion(assertion, assertType);
            else 
                removeActiveAssertion(assertion, assertType);
            
            /* Add to inActive list */
            insertInactiveAssertion(assertion, assertType);

            if (assertType->handler)
                (*assertType->handler)(assertType, kAssertionOpRelease);

            logASLAssertionEvent(kPMASLAssertionActionTurnOff, assertion);
        }
        else 
        {
            /* An inactive assertion is made active now */
            removeInactiveAssertion(assertion, assertType);
            raiseAssertion(assertion);
            logASLAssertionEvent(kPMASLAssertionActionTurnOn, assertion);
        }
        notify_post( kIOPMAssertionsAnyChangedNotifyString );
        return kIOReturnSuccess;
    }

    if (assertion->state & kAssertionStateInactive)
        return kIOReturnSuccess;    

    if  (assertion->mods & kAssertionModTimer) 
    {
        /* Remove from active or activeTimed list */
        if (oldState & kAssertionStateTimed)
            removeTimedAssertion(assertion, assertType);
        else 
            removeActiveAssertion(assertion, assertType);

        if (assertion->timeout != 0) {
            insertTimedAssertion(assertion, assertType, true);
        }
        else {
            insertActiveAssertion(assertion, assertType);
        }
    }


    if ( (assertion->mods & kAssertionModPowerConstraint) &&
            (assertType->handler) )
    {
        if (assertion->state & kAssertionStateValidOnBatt)
            (*assertType->handler)(assertType, kAssertionOpRaise);
        else
            (*assertType->handler)(assertType, kAssertionOpRelease);

    }

    notify_post( kIOPMAssertionsAnyChangedNotifyString );
    return kIOReturnSuccess;    
}



/*
 * Check for active assertions of the specified type and also for active
 * assertions of linked types.
 * Returns true if active assertions exist either in the specified type or
 * in the linked  types to the specified type.
 */
bool checkForActives(assertionType_t *assertType, bool *existsInThisType )
{
    bool activeExists = false;
    uint32_t idx = 0, linked = assertType->linkedTypes;

    /* 
     * Check for active assertions in this assertionType's 'active' & 'activeTimed' lists 
     * This assertion type shouldn't be disabled using 'kAssertionTypeDisabled' flag
     */
    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) && ( _getPowerSource() == kBatteryPowered) )
        activeExists = (!(assertType->flags & kAssertionTypeDisabled) && (assertType->validOnBattCount > 0));
    else
        activeExists = (LIST_FIRST(&assertType->active) || LIST_FIRST(&assertType->activeTimed)) &&
                        !(assertType->flags & kAssertionTypeDisabled);

    if (existsInThisType) 
        *existsInThisType = activeExists;
    if (activeExists) return true;

    /* Check for active ones in the linked assertion types */
    for ( ; linked !=0; linked >>=1, idx++) {
        if ((linked & 1) == 0) continue;

        assertType = &gAssertionTypes[idx];

    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) && ( _getPowerSource() == kBatteryPowered) )
        activeExists |= (!(assertType->flags & kAssertionTypeDisabled) && (assertType->validOnBattCount > 0));
    else
        activeExists |= ((LIST_FIRST(&assertType->active) || LIST_FIRST(&assertType->activeTimed)) &&
                        !(assertType->flags & kAssertionTypeDisabled) );

        if (activeExists) return true;
    }

    return activeExists;
}

/*
 * Checks if there are assertions preventing system going from S0dark to S3.
 * Returns true if S3 sleep is prevented.
 */
__private_extern__ bool systemBlockedInS0Dark( )
{

   /* PreventSystemSleep assertion and its linked types are the only ones
    * that can keep the system in S0dark.
    */
   return checkForActives(&gAssertionTypes[kPreventSleepIndex], NULL);
}

/*
 * Check for active assertions of the specified type.
 * Returns true if specified type assertions are active
 */
__private_extern__ bool checkForActivesByType(kerAssertionType type)
{
   bool activesForTheType = false;

   checkForActives(&gAssertionTypes[type], &activesForTheType);
   return activesForTheType;
}

/*
 * Check for assertions of the specified type, even if assertion type is disabled.
 * Returns true if there any assertions of specified type raised
 */
__private_extern__ bool checkForEntriesByType(kerAssertionType type)
{
    assertionType_t *assertType = &gAssertionTypes[type];

    if (LIST_FIRST(&assertType->active) || LIST_FIRST(&assertType->activeTimed))
        return true;

    return false;
}

/* Disable the specified assertion type */
__private_extern__ void disableAssertionType(kerAssertionType type)
{
   gAssertionTypes[type].flags |= kAssertionTypeDisabled;
   if (gAssertionTypes[type].handler)
      gAssertionTypes[type].handler(&gAssertionTypes[type], kAssertionOpEval);
}

/* Enablee the specified assertion type */
__private_extern__ void enableAssertionType(kerAssertionType type)
{
   gAssertionTypes[type].flags &= ~kAssertionTypeDisabled;
   if (gAssertionTypes[type].handler)
      gAssertionTypes[type].handler(&gAssertionTypes[type], kAssertionOpEval);
}

static inline void updateAggregates(assertionType_t *assertType, bool activesForTheType)
{
    if (activesForTheType) {
        SET_AGGREGATE_LEVEL(assertType->kassert,  1 );
    }
    else  {
        if (LEVEL_FOR_BIT(assertType->kassert) && assertType->globalTimeout) {
           resetGlobalTimer(assertType, 0);
        }
        SET_AGGREGATE_LEVEL(assertType->kassert,  0 );
    }
}

static void modifySettings(assertionType_t *assertType, assertionOps op)
{
    bool    activeExists;
    bool    activesForTheType = false;
    int     opVal;
    bool    assertionLevel = 0;
    
    assertionLevel = LEVEL_FOR_BIT(assertType->kassert);

    activeExists = checkForActives(assertType, &activesForTheType);
    updateAggregates(assertType, activesForTheType);


    if (op == kAssertionOpRaise) {
        /* 
         * if already raised with kernel or if there are no active ones,
         * nothing to do
         */
        if ( assertionLevel || !activeExists )
            return;
        opVal = 1;
    }
    else if (op == kAssertionOpRelease) {
        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !assertionLevel || activeExists)
            return;
        opVal = 0;

    }
    else { // op == kAssertionOpEval
        if (activeExists) {
            if (assertionLevel)
                return;
            opVal = 1;
        }
        else {
            if (!assertionLevel)
                return;
            opVal = 0;
        }
    }


    switch(assertType->kassert) {
        case kHighPerfIndex:
            overrideSetting( kPMForceHighSpeed, opVal);
            break;

        case kBackgroundTaskIndex:
        case kPreventIdleIndex:
            overrideSetting(kPMPreventIdleSleep, opVal);
            break;

        default:
            return;
    }

    activateSettingOverrides();
    notify_post( kIOPMAssertionsChangedNotifyString );
    return;
}

void handleBatteryAssertions(assertionType_t *assertType, assertionOps op)
{
    bool    activeExists;

    if (op == kAssertionOpEval)
        return; // Nothing to evaluate

    activeExists = checkForActives(assertType, NULL);

    if (op == kAssertionOpRaise) {
        /* 
         * if already raised with kernel or if there are no active ones,
         * nothing to do
         */
        if ( LEVEL_FOR_BIT(assertType->kassert) || !activeExists )
            return;
        SET_AGGREGATE_LEVEL(assertType->kassert, 1);
    }
    else {
        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !LEVEL_FOR_BIT(assertType->kassert) || activeExists)
            return;

        SET_AGGREGATE_LEVEL(assertType->kassert, 0);
    }

    switch(assertType->kassert) {
        case kDisableInflowIndex:
            sendSmartBatteryCommand( kSBUCInflowDisable, 
                    op == kAssertionOpRaise ? 1 : 0);
            break;

        case kInhibitChargeIndex:
            sendSmartBatteryCommand( kSBUCChargeInhibit, 
                    op == kAssertionOpRaise ? 1 : 0);
            break;

        case kDisableWarningsIndex:
            _setRootDomainProperty( CFSTR("BatteryWarningsDisabled"), kCFBooleanTrue);
            break;

        case kNoRealPowerSourcesDebugIndex:
            switchActiveBatterySet(op == kAssertionOpRaise ? kBatteryShowFake : kBatteryShowReal);
            BatteryTimeRemainingBatteriesHaveChanged(NULL);

        default:
            break;
    }

    notify_post( kIOPMAssertionsChangedNotifyString );
    return;
}

void setKernelAssertions(assertionType_t *assertType, assertionOps op)
{
    uint32_t    assertBit = 0;
    bool    activeExists, activesForTheType;

    /* 
     * active assertions exists if one of the active list is not empty and
     * the assertion is not globally disabled.
     */
    activeExists = checkForActives(assertType, &activesForTheType);

    switch(assertType->kassert) {

        case kPushServiceTaskIndex:
#if LOG_SLEEPSERVICES
           if ( !activesForTheType && LEVEL_FOR_BIT(assertType->kassert) )
                 logASLMessageSleepServiceTerminated(assertType->forceTimedoutCnt);
#endif
            assertBit = kIOPMDriverAssertionCPUBit;
            break;

        case kPreventSleepIndex:
        case kBackgroundTaskIndex:
            assertBit = kIOPMDriverAssertionCPUBit;
            break;

        case kDeclareUserActivity:
        case kPreventDisplaySleepIndex:
            assertBit = kIOPMDriverAssertionPreventDisplaySleepBit;
            break;

        case kExternalMediaIndex:
            assertBit = kIOPMDriverAssertionExternalMediaMountedBit;
            break;

        default:
            return;
    }

    updateAggregates(assertType, activesForTheType);

    if (op == kAssertionOpRaise)  {
        if ((assertType->kassert == kDeclareUserActivity) && activesForTheType)
            sendActivityTickle();

        /*
         * If server-mode power assertion is being raised anew, then we need
         * to unclamp SilentRunning through AppleSMC
         */
#if !TARGET_OS_EMBEDDED
        if ( (assertType->kassert == kPreventSleepIndex) && activesForTheType)
            _unclamp_silent_running();
#endif
        /*
         * if already raised with kernel or if there are no active ones,
         * nothing to do
         */
        if ( (kerAssertionBits & assertBit) || !activeExists )
            return;

    }
    else if (op == kAssertionOpRelease)  {

        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !(kerAssertionBits & assertBit) || activeExists)
            return;
    }
    else { // op == kAssertionOpEval

#if !TARGET_OS_EMBEDDED
        if ( (assertType->kassert == kPreventSleepIndex) && activesForTheType)
            _unclamp_silent_running();
#endif
        if (activeExists && (kerAssertionBits & assertBit))
            return;
        else if ( !activeExists && !(kerAssertionBits & assertBit) )
            return;
    }




    if (activeExists) {
        kerAssertionBits |= assertBit;
        sendUserAssertionsToKernel(kerAssertionBits);
    }
    else {
        kerAssertionBits &= ~assertBit;
        sendUserAssertionsToKernel(kerAssertionBits);
    }
    notify_post( kIOPMAssertionsChangedNotifyString );
}

static void enableIdleHandler(assertionType_t *assertType, assertionOps op)
{
    static IOPMAssertionID    enableIdleId = kIOPMNullAssertionID;
    CFMutableDictionaryRef    assertionDescription = NULL;
    bool                      activeExists;

#if !TARGET_OS_EMBEDDED
    return; // Supported for embedded only
#endif

    if (op == kAssertionOpEval)
        return; // Nothing to evaluate

    if (assertType->kassert != kEnableIdleIndex)
        return;

     activeExists = checkForActives(assertType, NULL);

    if (op == kAssertionOpRaise) {
        /* Check if this assertType is already raised */
        if (  !activeExists )
            return;
        SET_AGGREGATE_LEVEL(assertType->kassert, 1);

        InternalReleaseAssertion(&enableIdleId);
    }
    else {

        /* If not the last one or if this assertType is not raised, nothing to do */
        if ( activeExists )
            return;
        SET_AGGREGATE_LEVEL(assertType->kassert, 0);

       assertionDescription = _IOPMAssertionDescriptionCreate(
                       kIOPMAssertionTypePreventUserIdleSystemSleep,
                       CFSTR("com.apple.powermanagement.enableIdle"),
                       NULL, CFSTR("Waiting for Idle Sleep to enabled"), NULL, 0, 0);

       InternalCreateAssertion(assertionDescription, &enableIdleId);

       CFRelease(assertionDescription);



    }

    notify_post( kIOPMAssertionsChangedNotifyString );
}

void enforceAssertionTypeTimeCap(assertionType_t *assertType)
{
    assertion_t *assertion = NULL;

    assertType->forceTimedoutCnt = 0;
    /* Move all active assertions to inactive mode */
    while( (assertion = LIST_FIRST(&assertType->active)) )
    {
        removeActiveAssertion(assertion, assertType);
        insertInactiveAssertion(assertion, assertType);
        assertType->forceTimedoutCnt++;
        logASLAssertionEvent(kPMASLAssertionActionTimeOut, assertion);
        mt2RecordAssertionEvent(kAssertionOpGlobalTimeout, assertion);
    }

    /* Timeout all timed assertions */
    while( (assertion = LIST_FIRST(&assertType->activeTimed)) )
    {
        LIST_REMOVE(assertion, link);
        assertion->state &= ~kAssertionStateTimed;

        insertInactiveAssertion(assertion, assertType);
        assertType->forceTimedoutCnt++;
        logASLAssertionEvent(kPMASLAssertionActionTimeOut, assertion);
        mt2RecordAssertionEvent(kAssertionOpGlobalTimeout, assertion);
    }

    resetAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);

    if (assertType->forceTimedoutCnt == 0)
       logASLMessageSleepServiceTerminated(0);

    logASLAssertionsAggregate();
}

void resetGlobalTimer(assertionType_t *assertType, uint64_t timeout)
{

    assertType->globalTimeout = timeout;
    if (assertType->globalTimeout == 0) {
        if ( assertType->globalTimer)
            dispatch_source_cancel(assertType->globalTimer);
        return;
    }

    if (assertType->globalTimer == NULL) {
        assertType->globalTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());

        dispatch_source_set_event_handler(assertType->globalTimer, ^{
            enforceAssertionTypeTimeCap(assertType);
        });
    
        dispatch_source_set_cancel_handler(assertType->globalTimer, ^{
            dispatch_release(assertType->globalTimer);
            assertType->globalTimer = NULL;
        });
    
    }
    else {
         dispatch_suspend(assertType->globalTimer);
    }

    dispatch_source_set_timer(assertType->globalTimer, 
            dispatch_time(DISPATCH_TIME_NOW, assertType->globalTimeout * NSEC_PER_SEC), 
            DISPATCH_TIME_FOREVER, 0);
    dispatch_resume(assertType->globalTimer);

}


static IOReturn raiseAssertion(assertion_t *assertion)
{
    int                 idx = -1;
    int                 level;
    bool                isFirstOne;
    uint64_t            currTime = getMonotonicTime();
    uint32_t            levelInt = 0;
    CFDateRef           start_date = NULL;
    CFStringRef         assertionTypeRef;
    CFNumberRef         numRef = NULL;
    CFNumberRef         levelNum = NULL;
    CFTimeInterval      timeout = 0;
    assertionType_t     *assertType;
    uint64_t            assertion_id_64;


    assertionTypeRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionTypeKey);

    /* Find index for this assertion type */
    idx = getAssertionTypeIndex(assertionTypeRef);
    if (idx < 0 )
        return kIOReturnBadArgument;
    assertType = &gAssertionTypes[idx];
    assertion->kassert = idx;

    assertion_id_64 = ((uint64_t)idx << 32) | assertion->assertionId;
    CFNumberRef uniqueAID = CFNumberCreate(0, kCFNumberSInt64Type, &assertion_id_64);
    if (uniqueAID) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionGlobalUniqueIDKey, uniqueAID);
        CFRelease(uniqueAID);
    }

    /* Attach the Create Time */
    start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    if (start_date) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionCreateDateKey, start_date);
        CFRelease(start_date);
    }
    assertion->createTime = currTime;


    /* Is level set to 0 */
    numRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionLevelKey);
    if (isA_CFNumber(numRef)) {
        CFNumberGetValue(numRef, kCFNumberIntType, &level);
        if (level == kIOPMAssertionLevelOff) {
            /* Dump this assertion in inactive list */
            insertInactiveAssertion(assertion, assertType);
            goto exit;
        }
    }
    else {
        /* If level is not set, set it to On */
        levelInt = kIOPMAssertionLevelOn;
        levelNum = CFNumberCreate(0, kCFNumberIntType, &levelInt);
        CFDictionarySetValue(assertion->props, kIOPMAssertionLevelKey, levelNum);
        CFRelease(levelNum);
    }

    /* Check if this is appplicable on battery power also */
    if (assertType->flags & kAssertionTypeNotValidOnBatt) {
        CFBooleanRef    val = NULL;
        val = CFDictionaryGetValue(assertion->props, kIOPMAssertionAppliesToLimitedPowerKey);
        if (isA_CFBoolean(val) && (val == kCFBooleanTrue)) {
            assertion->state |= kAssertionStateValidOnBatt;
        }
    }

    isFirstOne = (LIST_FIRST(&assertType->active) == NULL) && 
                        (LIST_FIRST(&assertType->activeTimed) == NULL);
            
    /* Is this timed */
    numRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionTimeoutKey);
    if (isA_CFNumber(numRef)) 
        CFNumberGetValue(numRef, kCFNumberDoubleType, &timeout);
    if (timeout) {

        assertion->timeout = (uint64_t)timeout+currTime; // Absolute time at which assertion expires
        insertTimedAssertion(assertion, assertType, true);
    }
    else {
        /* Insert into active assertion list */
        insertActiveAssertion(assertion, assertType);
    }


    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRaise);

    mt2RecordAssertionEvent(kAssertionOpRaise, assertion);
    
exit:
    return kIOReturnSuccess;

}


IOReturn doCreate(
    pid_t                   pid,
    CFMutableDictionaryRef  newProperties,
    IOPMAssertionID         *assertion_id
) 
{
    int                     i;
    dispatch_source_t       proc_exit_source = NULL;
    assertion_t             *assertion = NULL;
    assertion_t             *tmp_a = NULL;
    IOReturn                result = kIOReturnSuccess;


    // assertion_id will be set to kIOPMNullAssertionID on failure.
    *assertion_id = kIOPMNullAssertionID;


    // Create a dispatch handler for process exit, if there isn't one
    if ( !processInfoRetain(pid) ) {

        if (( proc_exit_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue()))) 
        {
            dispatch_source_set_event_handler(proc_exit_source, ^{
                HandleProcessExit(pid);
            });
            
            dispatch_source_set_cancel_handler(proc_exit_source, ^{
                dispatch_release(proc_exit_source);
            });
            
            processInfoCreate(pid, proc_exit_source);
            dispatch_resume(proc_exit_source);
        }
    }

    // Generate an id
    for (i=gNextAssertionIdx; CFDictionaryGetValueIfPresent(gAssertionsArray, 
                              (void *)i, (const void **)&tmp_a) == true; ) {
        i = (i+1) % kMaxAssertions;
        if (i == gNextAssertionIdx) break;
    }
    if (CFDictionaryGetValueIfPresent(gAssertionsArray, (void *)i, (const void **)&tmp_a) == true) 
        return kIOReturnNoMemory;

    assertion = calloc(1, sizeof(assertion_t));
    if (assertion == NULL) {
        return kIOReturnNoMemory;
    }
    assertion->pid = pid;
    assertion->props = newProperties;
    CFRetain(newProperties);
    assertion->retainCnt = 1;

    assertion->assertionId = ID_FROM_INDEX(i);
    CFDictionarySetValue(gAssertionsArray, (void *)i, (const void *)assertion);
    gNextAssertionIdx = (i+1) % kMaxAssertions;

    result = raiseAssertion(assertion);
    if (result != kIOReturnSuccess) {
        CFDictionaryRemoveValue(gAssertionsArray, (void *)i);
        CFRelease(assertion->props);
        free(assertion);

        return result;
    }

    if ( (gDebugFlags & kIOPMDebugLogAssertionSynchronous) ||
          (assertion->kassert == kDeclareUserActivity) ||
          (assertion->kassert == kPreventSleepIndex) )
        logASLAssertionEvent(kPMASLAssertionActionCreate, assertion);

    notify_post( kIOPMAssertionsAnyChangedNotifyString );

    *assertion_id = assertion->assertionId;

    return result;
}

static void copyAssertion(assertion_t *assertion, CFMutableDictionaryRef assertionsDict)
{
    bool                    created = false;
    CFNumberRef             pidCF = NULL;
    CFMutableDictionaryRef  processDict = NULL;
    CFMutableArrayRef       pidAssertionsArr = NULL;
    CFStringRef             processName = NULL;

    pidCF = CFNumberCreate(0, kCFNumberIntType, &assertion->pid);

    processDict = (CFMutableDictionaryRef)CFDictionaryGetValue(assertionsDict, pidCF);
    if (processDict == NULL) 
    {
        pidAssertionsArr = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

        processDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 2,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(processDict,
                                CFSTR("PerTaskAssertions"),
                                pidAssertionsArr);
        CFDictionarySetValue(processDict,
                                kIOPMAssertionPIDKey,
                                pidCF);


        CFDictionarySetValue(assertionsDict, pidCF, processDict);
        created = true;
    }
    else {
        pidAssertionsArr = (CFMutableArrayRef)CFDictionaryGetValue(processDict, CFSTR("PerTaskAssertions"));
    }

    processName = processInfoGetName(assertion->pid);
    if (processName) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionProcessNameKey, processName);
    }
    CFArrayAppendValue(pidAssertionsArr, assertion->props);
    CFRelease(pidCF);

    if (created) {
        CFRelease(pidAssertionsArr);
        CFRelease(processDict);
    }
}


static CFArrayRef copyPIDAssertionDictionaryFlattened(void)
{
    CFMutableDictionaryRef  assertionsDict = NULL;
    CFMutableArrayRef       returnArray = NULL;
    CFDictionaryRef         *assertionsDictArr;
    assertionType_t         *assertType = NULL;
    int i, count;


    returnArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

    assertionsDict = CFDictionaryCreateMutable(
                        kCFAllocatorDefault, 0, 
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);

    /* Go thru each assertion type and copy assertion props */
    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        if (i == kEnableIdleIndex) continue;
        assertType = &gAssertionTypes[i]; 
        applyToAllAssertionsSync(assertType, false, ^(assertion_t *assertion)
                {
                    copyAssertion(assertion, assertionsDict);                    
                });

    }
    
    count = CFDictionaryGetCount(assertionsDict);
    assertionsDictArr = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef)*count); 
    CFDictionaryGetKeysAndValues(assertionsDict,
                    NULL, (const void **)assertionsDictArr);

    for (i=0; i < count; i++) {
        CFArrayAppendValue(returnArray, assertionsDictArr[i]);
    }

    free(assertionsDictArr);
    CFRelease(assertionsDict);

    return returnArray;
}

static IOReturn copyAssertionForID(
        pid_t inPID, int inID,
        CFMutableDictionaryRef  *outAssertion)
{
    IOReturn        ret = kIOReturnBadArgument;
    assertion_t     *assertion = NULL;

    if (outAssertion) {
        *outAssertion = NULL;
    }
    else goto exit;

    ret = lookupAssertion(inPID, inID, &assertion);
    if ((kIOReturnSuccess != ret)) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    CFRetain(assertion->props);
    *outAssertion = assertion->props;

exit:
    return ret;
}

static IOReturn doRetain(pid_t pid, IOPMAssertionID id)
{
    IOReturn        ret;
    assertion_t     *assertion = NULL;

    ret = lookupAssertion(pid, id, &assertion);
    
    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    assertion->retainCnt++;
    notify_post( kIOPMAssertionsAnyChangedNotifyString );

    return kIOReturnSuccess;
}



/* 
 * Called when display Sleep Timer setting is changed. The timeout value
 * for all assertions of type kIOPMAssertionUserIsActive is modified
 * to reflect the new display Sleep timer value.
 */
__private_extern__ void evalAllUserActivityAssertions(unsigned int dispSlpTimer)
{

    assertionType_t     *assertType;
    int                 changeInSecs;
    assertion_t         *assertion, *nextAssertion;
    uint64_t            currTime = getMonotonicTime();

    if (gDisplaySleepTimer == (int)dispSlpTimer)
        return;

    changeInSecs = ((int)dispSlpTimer - gDisplaySleepTimer) * 60;
    gDisplaySleepTimer = dispSlpTimer;

    assertType = &gAssertionTypes[kDeclareUserActivity];

    assertion = LIST_FIRST(&assertType->activeTimed);
    while( assertion )
    {
        if (assertion->timeout + changeInSecs < currTime)
            assertion->timeout = currTime;
        else
            assertion->timeout += changeInSecs;

        assertion = LIST_NEXT(assertion, link);
    }

    assertion = LIST_FIRST(&assertType->active);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (changeInSecs < 0)
            assertion->timeout = currTime; 
        else
            assertion->timeout = changeInSecs;

        removeActiveAssertion(assertion, assertType);
        insertTimedAssertion(assertion, assertType, false);
        assertion = nextAssertion;
    }
    updateAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);
}

static void   evaluateAssertions(void)
{
   int         i, pwrSrc;
   static int  prevPwrSrc = -1;
   assertionType_t    *assertType;

   pwrSrc = _getPowerSource();
   if (pwrSrc == prevPwrSrc)
      return; // If power source hasn't changed, there is nothing to do

   prevPwrSrc = pwrSrc;
    
    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertType = &gAssertionTypes[i];
        if (assertType->handler) {
            (*assertType->handler)(assertType, kAssertionOpEval);
        }
    }

    // Background task assertion need to re-configured on power source change
    configAssertionType(kBackgroundTaskIndex, false);
    cancelPowerNapStates( );
    
    logASLAssertionsAggregate();
}

__private_extern__ void setSleepServicesTimeCap(uint32_t  timeoutInMS)
{
    assertionType_t *assertType;

    assertType = &gAssertionTypes[kPushServiceTaskIndex];

    // Avoid duplicate resets to 0
    if ( (timeoutInMS == 0) && (assertType->globalTimeout == 0) )
        return;

    resetGlobalTimer(assertType, timeoutInMS/1000);
    if (timeoutInMS == 0) {
       CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, 
                   ^{ enforceAssertionTypeTimeCap(assertType); });
       CFRunLoopWakeUp(_getPMRunLoop());
    }
}


#if !TARGET_OS_EMBEDDED
void assertionLogger( )
{
    static uint32_t  idxToLog = 0;
    uint64_t         currTime = getMonotonicTime();
    uint32_t         delay = ASSERTION_LOG_DELAY;
    assertion_t     *assertion = NULL;


    // If synchronous logging is enabled, we don't
    // have to fo anything here.
    if (gDebugFlags & kIOPMDebugLogAssertionSynchronous)
        idxToLog = gNextAssertionIdx;

    while (idxToLog != gNextAssertionIdx)
    {
        if ( (CFDictionaryGetValueIfPresent(gAssertionsArray, 
                (void *)idxToLog, (const void **)&assertion) == false)
                || (assertion->state & kAssertionStateLogged) )
        {
            idxToLog = (idxToLog+1) % kMaxAssertions;
            continue;
        }
        
        if ((currTime - assertion->createTime) < ASSERTION_LOG_DELAY)
        {
            delay = currTime - assertion->createTime;
            break;
        }
        logASLAssertionEvent(kPMASLAssertionActionCreate, assertion);
        idxToLog = (idxToLog+1) % kMaxAssertions;
    }

    dispatch_source_set_timer(logDispatch,
            dispatch_time(DISPATCH_TIME_NOW, delay*NSEC_PER_SEC), 
            DISPATCH_TIME_FOREVER, 0);

}

#endif

__private_extern__ void configAssertionType(kerAssertionType idx, bool initialConfig)
{
   assertionHandler_f   oldHandler = NULL;
   CFNumberRef idxRef = NULL;
   uint32_t    oldLinks = 0, newLinks = 0;
   uint32_t    oldFlags, flags, i;
   kerAssertionType  altIdx;

   // This can get called before gUserAssertionTypesDict is initialized
   if ( !gUserAssertionTypesDict )
      return;

   if (!initialConfig) {
      oldLinks = gAssertionTypes[idx].linkedTypes;
      oldHandler = gAssertionTypes[idx].handler;
      oldFlags = gAssertionTypes[idx].flags;
   }

   switch(idx) 
   {
      case kHighPerfIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNeedsCPU, idxRef);
         gAssertionTypes[idx].handler = modifySettings;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kPreventIdleIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventUserIdleSystemSleep, idxRef);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNoIdleSleep, idxRef);
         gAssertionTypes[idx].handler = modifySettings;
         gAssertionTypes[idx].kassert = idx;
         if (!_DWBT_enabled()) {
           newLinks = 1 << kBackgroundTaskIndex;
         }
         break;

      case kDisableInflowIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDisableInflow, idxRef);
         gAssertionTypes[idx].handler = handleBatteryAssertions;
         gAssertionTypes[idx].kassert = idx;
         break;


      case kInhibitChargeIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeInhibitCharging, idxRef);
         gAssertionTypes[idx].handler = handleBatteryAssertions;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kDisableWarningsIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDisableLowBatteryWarnings, idxRef);
         gAssertionTypes[idx].handler = handleBatteryAssertions;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kPreventDisplaySleepIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventUserIdleDisplaySleep, idxRef);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNoDisplaySleep, idxRef);
         gAssertionTypes[idx].handler = setKernelAssertions;
         newLinks = 1 << kDeclareUserActivity;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kEnableIdleIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeEnableIdleSleep, idxRef);
         gAssertionTypes[idx].handler = enableIdleHandler;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kNoRealPowerSourcesDebugIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDisableRealPowerSources_Debug, idxRef);
         gAssertionTypes[idx].handler = handleBatteryAssertions;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kPreventSleepIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventSystemSleep, idxRef);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDenySystemSleep, idxRef);
         gAssertionTypes[idx].flags |= kAssertionTypeNotValidOnBatt;
         gAssertionTypes[idx].handler = setKernelAssertions;
         newLinks = 1 << kPushServiceTaskIndex;
         if (_DWBT_enabled())
           newLinks |= 1 << kBackgroundTaskIndex;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kExternalMediaIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, _kIOPMAssertionTypeExternalMedia, idxRef);
         gAssertionTypes[idx].handler = setKernelAssertions;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kDeclareUserActivity:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionUserIsActive, idxRef);
         gAssertionTypes[idx].handler = setKernelAssertions;
         newLinks = 1 << kPreventDisplaySleepIndex;
         gAssertionTypes[idx].kassert = idx;
         break;

      case kPushServiceTaskIndex:
         if ( isA_SleepSrvcWake() && _SS_allowed() ) {
            idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
            CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeApplePushServiceTask, idxRef);
         }
         else {
            /* Set this as an alias to BackgroundTask assertion for non-sleep srvc wakes */
            altIdx = kBackgroundTaskIndex;
            idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
            CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeApplePushServiceTask, idxRef);
         }
         gAssertionTypes[idx].flags |= kAssertionTypeGloballyTimed ;
         gAssertionTypes[idx].handler = setKernelAssertions;
         newLinks = 1 << kPreventSleepIndex;
         if (_DWBT_enabled())
           newLinks |= 1 << kBackgroundTaskIndex;
         gAssertionTypes[idx].kassert = idx;


         break;

      case kBackgroundTaskIndex:
         idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
         CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeBackgroundTask, idxRef);
         gAssertionTypes[idx].flags |= kAssertionTypeNotValidOnBatt;
         gAssertionTypes[idx].flags &= ~kAssertionTypeDisabled;
         if (_DWBT_enabled()) {
           gAssertionTypes[idx].handler = setKernelAssertions;
           newLinks = 1 << kPreventSleepIndex | 1 << kPushServiceTaskIndex;

           if ( !isA_BTMtnceWake() ) {
               // Disable this assertion in non-DWBT wakes
               gAssertionTypes[idx].flags |= kAssertionTypeDisabled;
           }
         }
         else  {
           gAssertionTypes[idx].handler = modifySettings;
           newLinks = 1 << kPreventIdleIndex;
         }
         gAssertionTypes[idx].kassert = idx;
         break;

      default:
         return;
   }
   if (idxRef)
      CFRelease(idxRef);

   if ( (!initialConfig) && (newLinks != oldLinks) ) {
      uint32_t links = newLinks;
      
      // If assertion 'a' is linked to 'b', then assertion 'b'
      // has to be linked to 'a'
      for (i = 0; oldLinks; i++, oldLinks >>= 1) {
         if ( !(oldLinks & 1) ) continue;

         gAssertionTypes[i].linkedTypes &= ~(1 << idx);
      }

      for (i = 0 ; links; i++, links >>= 1) {
         if ( !(links & 1) ) continue;

         gAssertionTypes[i].linkedTypes |= (1 << idx);
      }
   }

   if (initialConfig) {
      gAssertionTypes[idx].linkedTypes = newLinks;
   }
   else if (oldHandler != gAssertionTypes[idx].handler) {
      // Temporarily disable the assertion type and call the old handler.
      flags = gAssertionTypes[idx].flags;
      gAssertionTypes[idx].flags |= kAssertionTypeDisabled;

      oldHandler(&gAssertionTypes[idx], kAssertionOpEval);
      gAssertionTypes[idx].flags = flags;

      gAssertionTypes[idx].linkedTypes = newLinks;

      // Call the new handler
      if (gAssertionTypes[idx].handler)
         gAssertionTypes[idx].handler(&gAssertionTypes[idx], kAssertionOpEval);
   }
   else if (oldFlags != gAssertionTypes[idx].flags) {
      if (gAssertionTypes[idx].handler)
         gAssertionTypes[idx].handler(&gAssertionTypes[idx], kAssertionOpEval);
   }

}

__private_extern__ void
PMAssertions_prime(void)
{

    kerAssertionType  idx = 0;

    gAssertionsArray = CFDictionaryCreateMutable(NULL, kMaxAssertions, NULL, NULL);
    gProcessDict = CFDictionaryCreateMutable(0, 0, NULL, &kCFTypeDictionaryValueCallBacks);

    gUserAssertionTypesDict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


    assertion_types_arr[kHighPerfIndex]             = kIOPMAssertionTypeNeedsCPU; 
    assertion_types_arr[kPreventIdleIndex]          = kIOPMAssertionTypePreventUserIdleSystemSleep;
    assertion_types_arr[kPreventSleepIndex]         = kIOPMAssertionTypePreventSystemSleep;
    assertion_types_arr[kDisableInflowIndex]        = kIOPMAssertionTypeDisableInflow; 
    assertion_types_arr[kInhibitChargeIndex]        = kIOPMAssertionTypeInhibitCharging;
    assertion_types_arr[kDisableWarningsIndex]      = kIOPMAssertionTypeDisableLowBatteryWarnings;
    assertion_types_arr[kPreventDisplaySleepIndex]  = kIOPMAssertionTypePreventUserIdleDisplaySleep;
    assertion_types_arr[kEnableIdleIndex]           = kIOPMAssertionTypeEnableIdleSleep;
    assertion_types_arr[kNoRealPowerSourcesDebugIndex] = kIOPMAssertionTypeDisableRealPowerSources_Debug;
    assertion_types_arr[kExternalMediaIndex]        = _kIOPMAssertionTypeExternalMedia;
    assertion_types_arr[kDeclareUserActivity]       = kIOPMAssertionUserIsActive;
    assertion_types_arr[kPushServiceTaskIndex]      = kIOPMAssertionTypeApplePushServiceTask;
    assertion_types_arr[kBackgroundTaskIndex]       = kIOPMAssertionTypeBackgroundTask;

    for (idx = 0; idx < kIOPMNumAssertionTypes; idx++)
       configAssertionType(idx, true);


    getDisplaySleepTimer(&gDisplaySleepTimer); 

    // Reset kernel assertions to clear out old values from prior to powerd's crash
    sendUserAssertionsToKernel(0);
#if TARGET_OS_EMBEDDED
    /* 
     * Disable Idle Sleep until some one comes and enables the idle sleep
     * by issuing 'kIOPMAssertionTypeEnableIdleSleep' assertion.
     */
    gAssertionTypes[kEnableIdleIndex].handler(
          &gAssertionTypes[kEnableIdleIndex], kAssertionOpRelease);
#else

    SET_AGGREGATE_LEVEL(kEnableIdleIndex, 1); /* Idle sleep is enabled by default */
    gDebugFlags = kIOPMDebugEnableAssertionLogging;

    /* Start a dispatch source to log assertions with a delay */
    logDispatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(logDispatch, ^{
        assertionLogger();
    });

    dispatch_source_set_cancel_handler(logDispatch, ^{
        dispatch_release(logDispatch);
    });

    dispatch_source_set_timer(logDispatch, dispatch_time(DISPATCH_TIME_NOW, ASSERTION_LOG_DELAY * NSEC_PER_SEC), 
                                            DISPATCH_TIME_FOREVER, 0);
    dispatch_resume(logDispatch);
#endif
    return;
}


