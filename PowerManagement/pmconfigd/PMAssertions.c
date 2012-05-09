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


CFArrayRef copyScheduledPowerEvents(void);
CFDictionaryRef copyRepeatPowerEvents(void);

// forward
static void                         evaluateAssertions(void);
static bool                         calculateAggregates(void);

static void                         sendSmartBatteryCommand(uint32_t which, uint32_t level);
static void                         sendUserAssertionsToKernel(uint32_t user_assertions);
static bool                         HandleProcessExit(pid_t deadPID);

#if !TARGET_OS_EMBEDDED
static void                         logASLAssertionEvent(const char              *assertionActionCStr,
                                                        CFDictionaryRef         taskDictionary,
                                                        CFDictionaryRef         assertionDictionary);
static void                         logASLAssertionSummary(void);
#else
#define                             logASLAssertionEvent(X1, X2, X3)    
#define                             logASLAssertionSummary()
#endif

static bool                         propertiesDictRequiresRoot(CFDictionaryRef   props);

static IOReturn                     doCreate(pid_t pid, CFDictionaryRef newProperties,
                                                        IOPMAssertionID *assertion_id);
static IOReturn                     doRetain(pid_t pid, IOPMAssertionID id);
static IOReturn                     doRelease(pid_t pid, IOPMAssertionID id);
static IOReturn                     doSetProperties(pid_t pid, 
                                                        IOPMAssertionID id, 
                                                        CFDictionaryRef props);
static IOReturn                     doClearTimer(pid_t pid, IOPMAssertionID id, TimerType type);
static IOReturn                     doResetTimer(pid_t pid, IOPMAssertionID id);

//static int                          indexForAssertionName(CFStringRef assertionName);
static CFArrayRef                   copyPIDAssertionDictionaryFlattened(void);
static CFDictionaryRef              copyAggregateValuesDictionary(void);
static CFArrayRef                   copyTimedOutAssertionsArray(void);
static IOReturn                     copyAssertionForID(pid_t                   inPID,
                                                       int                     inID,
                                                       CFMutableDictionaryRef  *outTask,
                                                       CFMutableArrayRef       *outTaskAssertions,
                                                       CFMutableDictionaryRef  *outAssertion);
static void                         sendActivityTickle ();
static void                         dictionarySetDispatchSource(
                                                     CFMutableDictionaryRef d, 
                                                     CFStringRef k, 
                                                     dispatch_source_t p);
static dispatch_source_t            dictionaryGetDispatchSource(
                                                     CFDictionaryRef d, 
                                                     CFStringRef k);


// globals
static const int                    kMaxCountTimedOut = 5;
extern CFMachPortRef                pmServerMachPort;

static CFMutableDictionaryRef       gAssertionsDict = NULL;
static CFMutableArrayRef            gTimedOutArray = NULL;

static int                          aggregate_assertions;
static int                          last_aggregate_assertions;
static int                          aconly_assertions;
static CFStringRef                  assertion_types_arr[kIOPMNumAssertionTypes];
static int                          _IdleEnableAssertionIsMeaningless = 1;

extern SleepServiceStruct           gSleepService;

static bool                         doDebugASLAssertions = false;
__private_extern__ bool isDisplayAsleep( );

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
) {
    CFDictionaryRef     newAssertionProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;
    
    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
        
    unfolder = CFDataCreate(0, (const UInt8 *)props, propsCnt);
    if (unfolder) {
        newAssertionProperties = (CFDictionaryRef)CFPropertyListCreateWithData(0, unfolder, 0, NULL, NULL);
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
 ) {
    CFDictionaryRef     setProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);
    
    unfolder = CFDataCreate(0, (const UInt8 *)props, propsCnt);
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
 ) {
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
 ) {
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

        *return_val = copyAssertionForID(callerPID, assertion_id,  NULL, NULL, 
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
    CFMutableDictionaryRef      assertion = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    IOReturn            ret;
    bool                create_new = true;
    uint32_t            displaySleepTimer;
    CFNumberRef         CFdisplaySleepTimer = NULL;
    CFStringRef         type = NULL;
    CFDateRef           start_date = NULL;
    

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);    
        
    unfolder = CFDataCreate(0, (const UInt8 *)props, propsCnt);
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
    if (getDisplaySleepTimer(&displaySleepTimer) != kIOReturnSuccess)
        goto exit;

    displaySleepTimer *=  60; /* Convert to secs */
    CFdisplaySleepTimer = CFNumberCreate(0, kCFNumberIntType, &displaySleepTimer);
    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutKey, CFdisplaySleepTimer);
    CFRelease(CFdisplaySleepTimer);

    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);

    /* Check if this is a repeat call on previously returned assertion id */
    do {
        if (assertion_id == NULL || *assertion_id == kIOPMNullAssertionID) 
            break;

        ret = copyAssertionForID(callerPID, *assertion_id, NULL, NULL, &assertion);
        if ((kIOReturnSuccess != ret) || !assertion) 
            break;

        type = CFDictionaryGetValue(assertion, kIOPMAssertionTypeKey);
        if (!CFEqual(type, kIOPMAssertionUserIsActive))
            break;

        /* Extend the timeout timer of this assertion by display sleep timer value */

        /* First set the assertion level to ON */
        int k = kIOPMAssertionLevelOn;
        CFNumberRef useLevelOnNum = CFNumberCreate(0, kCFNumberIntType, &k);
        CFDictionarySetValue(assertionProperties, kIOPMAssertionLevelKey, useLevelOnNum);
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

    if (assertion)
        CFRelease(assertion);

    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);
    return KERN_SUCCESS;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* doTearDownTask
 * Contains destroy code that's common to our "release via API/timeout" and "crash" exit paths
 */
static bool doTearDownTask(pid_t task_teardown_pid, CFMutableDictionaryRef task_dictionary)
{
    dispatch_source_t task_exit_source;

    if ((task_exit_source = dictionaryGetDispatchSource(task_dictionary, kIOPMTaskDispatchSourceKey)))
    {
        dispatch_source_cancel(task_exit_source);
    }

    CFDictionaryRemoveValue(task_dictionary, kIOPMTaskDispatchSourceKey);
    
    CFDictionaryRemoveValue(gAssertionsDict, CAST_PID_TO_KEY(task_teardown_pid));

    return true;
}

#pragma mark -
#pragma mark Release
IOReturn doRelease(pid_t pid, IOPMAssertionID id)
{
    CFMutableDictionaryRef  callerTask          = NULL;
    CFMutableArrayRef       taskAssertions      = NULL;              
    CFMutableDictionaryRef  releaseAssertion    = NULL;
    CFNumberRef             refCountNum         = NULL;
    int                     refCount            = 0;
    int                     i;
    int                     n;
    bool                    releaseTask;
    IOReturn                ret;
        
    ret = copyAssertionForID(pid, id, &callerTask, &taskAssertions, &releaseAssertion);
    
    if (kIOReturnSuccess != ret) {
        goto exit;
    }
    
    refCountNum = CFDictionaryGetValue(releaseAssertion, kIOPMAssertionRetainCountKey);

    if (refCountNum) 
    {    
        CFNumberGetValue(refCountNum, kCFNumberIntType, &refCount);
        
        refCount--;
        
        refCountNum = CFNumberCreate(0, kCFNumberIntType, &refCount);
        if (refCountNum) 
        {
            CFDictionarySetValue(releaseAssertion, kIOPMAssertionRetainCountKey, refCountNum);
            CFRelease(refCountNum);
        }
    }
    
    if (0 < refCount) 
    {
        /*
         * The ref count is greater than zero. Let the assertion live another day.
         * This is an expected exit path.
         */
        ret = kIOReturnSuccess;
        goto exit;
    }


    /*
     * Destroy the assertion and its tracking dictionary.
     */

    doClearTimer(pid, id, kTimerTypeReleased);
    
    // Add a "Released" timestamp if this assertion has already timed out
    // If this assertion has timed-out, debugging humans will want to know if 
    // and when it was released.
    if (CFDictionaryGetValue(releaseAssertion, kIOPMAssertionTimedOutDateKey))
    {
        CFDateRef   dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        CFDictionarySetValue(releaseAssertion, kIOPMAssertionReleaseDateKey, dateNow);
        CFRelease(dateNow);
    }
    
    // Figure out the index into the returned taskAssertions array
    CFIndex arrayIndex = INDEX_FROM_ID(id);
    
    // Release it from its task array
    // * Note that if this assertion has timed-out, it will continue to exist for
    // record-keeping purposes on the gTimedOutAssertions arrray.
    CFArraySetValueAtIndex(taskAssertions, arrayIndex, kCFBooleanFalse);
    

    /*
     * The following code destroys this process's assertion tracking array.
     */
    
    
    // Check whether this is the last assertion in the task's array.
    // If no more assertions tied to the task, clean up the task's bookkeeping.
    releaseTask = TRUE;
    n = CFArrayGetCount(taskAssertions);
    for (i =0; i < n; i++) {
        CFTypeRef    assertion;
        assertion = CFArrayGetValueAtIndex(taskAssertions, i);
        if (!CFEqual(assertion, kCFBooleanFalse)) {
            releaseTask = FALSE;
            break;
        }
    }

    if (releaseTask) {
        doTearDownTask(pid, callerTask);
    } 
    
    evaluateAssertions();
    
    logASLAssertionEvent(kPMASLAssertionActionRelease, callerTask, releaseAssertion);
    ret = kIOReturnSuccess;    
    
    
exit:
    
    if (callerTask) {
        CFRelease(callerTask);
    }
    if (taskAssertions) {
        CFRelease(taskAssertions);
    }
    if (releaseAssertion) {
        CFRelease(releaseAssertion);
    }
    
    return ret;   
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

__private_extern__ bool
HandleProcessExit(pid_t deadPID)
{
    CFMutableDictionaryRef      deadTrackerDict = NULL;
    CFArrayRef                  dead_task_assertions;

    if (!gAssertionsDict 
        || !(deadTrackerDict = (CFMutableDictionaryRef)CFDictionaryGetValue(gAssertionsDict, CAST_PID_TO_KEY(deadPID))) )
    {
        return false;
    }
        
    // Clean up after this dead process
    CFRetain(deadTrackerDict);


    // Mark each assertion in the process as released
    // (useful debugging data for tracking down timed-out assertions)

    dead_task_assertions = CFDictionaryGetValue(deadTrackerDict, kIOPMTaskAssertionsKey);
    
    if (dead_task_assertions)
    {
        int assertions_count = CFArrayGetCount(dead_task_assertions);
        int i;
        
        for (i=0; i<assertions_count; i++)
        {
            CFMutableDictionaryRef releaseAssertion;
            releaseAssertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(dead_task_assertions, i);

            if (!releaseAssertion || !isA_CFDictionary(releaseAssertion)) {
                continue;
            }

            doClearTimer(deadPID, ID_FROM_INDEX(i), kTimerTypeReleased);

            // Add a "Released" timestamp if this assertion has already timed out
            // If this assertion has timed-out, debugging humans will want to know if 
            // and when it was released.
            if (CFDictionaryGetValue(releaseAssertion, kIOPMAssertionTimedOutDateKey))
            {
                CFDateRef   dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
                CFDictionarySetValue(releaseAssertion, kIOPMAssertionReleaseDateKey, dateNow);
                CFRelease(dateNow);
            }
        }
    }


    doTearDownTask(deadPID, deadTrackerDict);
        
    evaluateAssertions();

    logASLAssertionEvent(kPMASLAssertionActionClientDeath, deadTrackerDict, NULL);
    CFRelease(deadTrackerDict);

    // Return true if we destroyed the port.
    return true;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


static CFStringRef preferredNameForAssertion(CFStringRef given)
{
    if (CFEqual(given, kIOPMAssertionTypeNoIdleSleep))
    {
        return assertion_types_arr[kPreventIdleIndex];
    }
    
    if (CFEqual(given, kIOPMAssertionTypeNoDisplaySleep))
    {
        return assertion_types_arr[kPreventDisplaySleepIndex];
    }
    
    if (CFEqual(given, kIOPMAssertionTypeDenySystemSleep))
    {
        return assertion_types_arr[kPreventSleepIndex];
    }
        
    return given;
}

static bool _passItForward(CFDictionaryRef in, CFMutableDictionaryRef out, CFStringRef key, CFTypeID type)
{
    CFTypeRef       t = CFDictionaryGetValue(in, key);
    bool            forwardedProperty = false;
    
    if (t && CFGetTypeID(t) == type) {
        forwardedProperty = true;
        CFDictionarySetValue(out, key, t);
    }
    return forwardedProperty;
}

/* powerd internally tracks each assertion's type with a bitfield
 * bitForAssertionType returns the bitfield for a given assertion type.
 */
uint32_t bitForAssertionType(CFStringRef assertionName)
{
    int i;
        
    if (assertionName) {
        for (i=0; i<kIOPMNumAssertionTypes; i++) 
        {
            if (CFEqual(assertionName, assertion_types_arr[i])) 
            {
                return (1 << i);
            }
        }
    }
    return 0;
}

static void calculateAndSetAssertionBitfieldsFromDictionary(CFMutableDictionaryRef a, uint32_t *levelBits, bool *modified)
{
    CFStringRef                 assertionType;
    CFNumberRef                 assertionSummaryBitsNum = NULL;
    CFNumberRef                 assertionLevelValue = NULL;
    CFNumberRef                 publishBits = NULL;
    uint32_t                    lastLevelBits = 0;
    int                         levelOn = kIOPMAssertionLevelOn;

    *levelBits = 0;
    *modified = false;
    
    // Read the assertions LEVEL key
    if ((assertionLevelValue = CFDictionaryGetValue(a, kIOPMAssertionLevelKey)))
    {
        CFNumberGetValue(assertionLevelValue, kCFNumberIntType, &levelOn);
    }
    
    // Get the corresponding bitfield for this combination of LEVEL & TYPE
    if (kIOPMAssertionLevelOn == levelOn)
    {                
        assertionType = CFDictionaryGetValue(a, kIOPMAssertionTypeKey);
        if (isA_CFString(assertionType)) {
            *levelBits = bitForAssertionType(assertionType);

            if (CFEqual(assertionType, kIOPMAssertionUserIsActive) && isDisplayAsleep( ))
            {
                sendActivityTickle( );
            }
        }
    }

    // Figure out if the assertion state has changed
    if ((assertionSummaryBitsNum = CFDictionaryGetValue(a, kIOPMAssertionLevelsBits)))
    {
        CFNumberGetValue(assertionSummaryBitsNum, kCFNumberIntType, &lastLevelBits);
    }
    
    *modified = (*levelBits != lastLevelBits);

    // Synchronize assertion dictionary LevelBits field with assertions held
    publishBits = CFNumberCreate(0, kCFNumberIntType, levelBits);
    if (publishBits) {
        CFDictionarySetValue(a, kIOPMAssertionLevelsBits, publishBits);
        CFRelease(publishBits);
    }
    
    return;
}

void forwardPropertiesToAssertion(
    CFDictionaryRef             inProps, 
    CFMutableDictionaryRef      assertion,
    uint32_t                    *_assertionLevels,
    bool                        *_modifiedAssertions,
    bool                        *_triggerTimerEval, 
    bool                        *_triggerLevelEval, 
    bool                        *_powerConstraintsChanged)
{
    CFTypeID                    _kStringTypeID = CFStringGetTypeID();
    CFTypeID                    _kNumTypeID = CFNumberGetTypeID();
    CFTypeID                    _kBooleanTypeID = CFBooleanGetTypeID();
    CFTypeID                    _kDateTypeID = CFDateGetTypeID();
    CFStringRef                 assertionTypeString= NULL;
    
    *_assertionLevels = 0;
    *_modifiedAssertions = NULL;
    *_triggerLevelEval = NULL;
    *_triggerTimerEval = NULL;
    *_powerConstraintsChanged = NULL;
    
    _passItForward(inProps, assertion, kIOPMAssertionNameKey, _kStringTypeID);
    _passItForward(inProps, assertion, kIOPMAssertionDetailsKey, _kStringTypeID);
    _passItForward(inProps, assertion, kIOPMAssertionHumanReadableReasonKey, _kStringTypeID);
    _passItForward(inProps, assertion, kIOPMAssertionLocalizationBundlePathKey, _kStringTypeID);
    _passItForward(inProps, assertion, kIOPMAssertionFrameworkIDKey, _kStringTypeID);
    _passItForward(inProps, assertion, kIOPMAssertionPlugInIDKey, _kStringTypeID);
    _passItForward(inProps, assertion, kIOPMAssertionCreateDateKey, _kDateTypeID);
    
    if (_passItForward(inProps, assertion, kIOPMAssertionTimeoutActionKey, _kStringTypeID)) {
        *_triggerTimerEval = true;
    }
    if (_passItForward(inProps, assertion, kIOPMAssertionTimeoutKey, _kNumTypeID)) {
        *_triggerTimerEval = true;
    }
    
    if (_passItForward(inProps, assertion, kIOPMAssertionLevelKey, _kNumTypeID)) {
        *_triggerLevelEval = true;
    }
    
    if (_passItForward(inProps, assertion, kIOPMAssertionAppliesToLimitedPowerKey, _kBooleanTypeID)) {
        *_powerConstraintsChanged = true;
    }
    
    if ((assertionTypeString = isA_CFString(CFDictionaryGetValue(inProps, kIOPMAssertionTypeKey))))
    {
        // Map older Assertion names into their preferred names
        assertionTypeString = preferredNameForAssertion(assertionTypeString);        
        CFDictionarySetValue(assertion, kIOPMAssertionTypeKey, assertionTypeString);
        
        *_triggerLevelEval = true;

        uint32_t levelInt = 0;
        CFNumberRef levelNum = NULL;
        
        // If assertion level isn't specified, default the assertion level to ON
        if (!CFDictionaryGetValue(assertion, kIOPMAssertionLevelKey)) {
            levelInt = kIOPMAssertionLevelOn;
            levelNum = CFNumberCreate(0, kCFNumberIntType, &levelInt);
            CFDictionarySetValue(assertion, kIOPMAssertionLevelKey, levelNum);
            CFRelease(levelNum);
        }
    }
    
    calculateAndSetAssertionBitfieldsFromDictionary(assertion, _assertionLevels, _modifiedAssertions);
 
    return;
}

#pragma mark -
#pragma mark SetProperties

static IOReturn doSetProperties(pid_t pid, 
                                IOPMAssertionID id, 
                                CFDictionaryRef inProps)
{
    CFMutableDictionaryRef      assertion = NULL;
    uint32_t                    _assertionLevels = 0;
    bool                        _modifiedAssertions = false;
    bool                        _triggerLevelEval = false;
    bool                        _triggerTimerEval = false;
    bool                        _powerConstraintsChanged = false;
    IOReturn                    ret;
    
    // doSetProperties doesn't handle retain()/release() count. 
    // Callers should use IOPMAssertionRetain() or IOPMAssertionRelease().
    
    ret = copyAssertionForID(pid, id, NULL, NULL, &assertion);
    
    if ((kIOReturnSuccess != ret)) {
        return ret;
    }
    
    forwardPropertiesToAssertion(inProps, assertion,
                                 &_assertionLevels,
                                 &_modifiedAssertions,
                                 &_triggerTimerEval, 
                                 &_triggerLevelEval, 
                                 &_powerConstraintsChanged);

    CFRelease(assertion);

    if (_triggerLevelEval || _powerConstraintsChanged) {
        evaluateAssertions();
    }
    
    if (_triggerLevelEval && _modifiedAssertions) {
        _triggerTimerEval = true;            
    }

    if (_triggerTimerEval) 
    {
        if (_modifiedAssertions && (0 == _assertionLevels)) 
        {
            // The assertions just turned off; clear the timer.
            doClearTimer(pid, id, kTimerTypeReleased);
        } else if (0 !=_assertionLevels) 
        {
            // Either setProperties just updated a timer property;
            // or setProperties turned an assertion ON.
            doResetTimer(pid, id);
        }

    }

    return kIOReturnSuccess;    
}



/******************************************************************************
 ******************************************************************************
 *****************************************************************************/
 
static void
applyToAllAssertionsSync(void (^performOnAssertion)(CFDictionaryRef, CFDictionaryRef))
{
    CFDictionaryRef         *process_assertions                 = NULL;
    int                     process_count                       = 0;
    int                     i, j;
    
	if (!gAssertionsDict) {
		return;
    }
	
    process_count = CFDictionaryGetCount(gAssertionsDict);
    
    if (0 == process_count) {
        return;
    }
    
    process_assertions = malloc(sizeof(CFDictionaryRef) * process_count);
    CFDictionaryGetKeysAndValues(gAssertionsDict, NULL, (const void **)process_assertions);
    for (i=0; i<process_count; i++)
    {
        CFArrayRef          asst_arr = NULL;
        int                 asst_arr_count = 0;
        
        if (!isA_CFDictionary(process_assertions[i])) 
            continue;
        
        asst_arr = isA_CFArray(CFDictionaryGetValue(process_assertions[i], kIOPMTaskAssertionsKey));
        if (!asst_arr) 
            continue;
        
        asst_arr_count = CFArrayGetCount(asst_arr);
        for (j=0; j<asst_arr_count; j++)
        {
            CFDictionaryRef     this_assertion = NULL;
            
            if ((this_assertion = CFArrayGetValueAtIndex(asst_arr, j))
                && (kCFBooleanFalse != (CFTypeRef)this_assertion))
            {
                // Invoke the block on this assertion
                performOnAssertion(process_assertions[i], this_assertion);
            }
        }
    }
    free(process_assertions);
}

/* calculateAggregates
 * returns true if any assertion totals have changed in this update.
 * Returns false if aggregate assertion totals haven't changed.
 */
static bool
calculateAggregates(void)
{
    PowerSources pwr_src = _getPowerSource();
    
    // Clear out the aggregate assertion values. We are about to re-calculate
    // these values in the big nasty loop below.
    aggregate_assertions = 0;
        
    // 'aconly_assertions' tracks the assertions that have to be applied on
    // AC power only. By default 'PreventSystemSleep' assertion is valid only 
    // on AC power unless this assertion is set with property
    // 'kIOPMAssertionAppliesToLimitedPowerKey'.
    aconly_assertions = (1 << kPreventSleepIndex);

    applyToAllAssertionsSync(^(CFDictionaryRef itsTaskDictionary, CFDictionaryRef thisAssertion) 
            {
                CFNumberRef         assertions_combined = NULL;
                CFBooleanRef        power_limits = NULL;
                int                 val = -1;
 
                assertions_combined = CFDictionaryGetValue(thisAssertion, kIOPMAssertionLevelsBits);
                
                if (assertions_combined) {
                    CFNumberGetValue(assertions_combined, kCFNumberIntType, &val);                

                    if ( (val & aconly_assertions) && (pwr_src == kBatteryPowered) ) {
                        /* 
                         * If this assertion is AC power only assertion by default, 
                         * then apply this assertion on battery power only if it has
                         * override property
                         */
                        if ( (power_limits = CFDictionaryGetValue(thisAssertion, kIOPMAssertionAppliesToLimitedPowerKey))
                                && (kCFBooleanTrue == power_limits) ) {
                            aggregate_assertions |= val;
                        }

                    }
                    else {
                        aggregate_assertions |= val;
                    }
                }

                 
            });

    
    // Once someone asserts EnableIdle for the first time, then we'll start respecting it.
    // Until then we always set it to ON.
    if (LEVEL_FOR_BIT(kEnableIdleIndex)) {
        _IdleEnableAssertionIsMeaningless = 0;
    } else {
        SET_AGGREGATE_LEVEL(kEnableIdleIndex, _IdleEnableAssertionIsMeaningless);
    }
    
    
    // Did anything actually change?
    
    // At this point we have iterated through the entire nested data structure
    // and have calculated what the total settings are for each of the profiles
    // and have stored them in the global array aggregate_assertions
                             
    return (aggregate_assertions != last_aggregate_assertions);
}


static void sendActivityTickle ()
{
    io_service_t                rootDomainService = IO_OBJECT_NULL;
    io_connect_t                gRootDomainConnect = IO_OBJECT_NULL;
    kern_return_t               kr = 0;
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

static void
evaluateAssertions(void)
{
    bool                        aggregatesHaveChanged = false;
    uint32_t                    userAssertionBits = 0;
    static uint32_t             lastUserAssertionBits = 0xFFFF;
    
    aggregatesHaveChanged = calculateAggregates();

    /*
     * Publish the "Anything assertion changed" notification
     */
    notify_post( kIOPMAssertionsAnyChangedNotifyString );

    if (!aggregatesHaveChanged)
    {
        // Assertion levels have not changed; nothing to do.
        // Exit without acting upon any assertions.
        goto exit;
    }
    
    // Override PM settings
    overrideSetting( kPMForceHighSpeed, LEVEL_FOR_BIT(kHighPerfIndex));

    userAssertionBits |= LEVEL_FOR_BIT(kPreventDisplaySleepIndex) ? kIOPMDriverAssertionPreventDisplaySleepBit : 0;
    userAssertionBits |= LEVEL_FOR_BIT(kDeclareUserActivity) ? kIOPMDriverAssertionPreventDisplaySleepBit : 0;


#if SLEEPSERVICES
    userAssertionBits |= LEVEL_FOR_BIT(kPushServiceTaskAssertion) ? kIOPMDriverAssertionCPUBit : 0;
#endif
    if (!LEVEL_FOR_BIT(kPushServiceTaskAssertion) &&
        LAST_LEVEL_FOR_BIT(kPushServiceTaskAssertion))
    {
        /* kPushServiceTaskAssertion is released */
        cancelSleepService(); 
    }

    // Prevent idle sleep if PreventIdle or DenyIdle is asserted
    //   or if EnableIdleSleep isn't asserted.
    if (  LEVEL_FOR_BIT(kPreventIdleIndex)
      ||  LEVEL_FOR_BIT(kPreventSleepIndex)
      || !LEVEL_FOR_BIT(kEnableIdleIndex)) {
        overrideSetting(kPMPreventIdleSleep, 1);
    } else {
        overrideSetting(kPMPreventIdleSleep, 0);
    }
        
    userAssertionBits |= LEVEL_FOR_BIT(kPreventSleepIndex) ? kIOPMDriverAssertionCPUBit : 0;

    userAssertionBits |= LEVEL_FOR_BIT(kExternalMediaIndex) ? kIOPMDriverAssertionExternalMediaMountedBit : 0;

        
    // Perform kDisableInflowIndex
    if ( LEVEL_FOR_BIT(kDisableInflowIndex) 
        != LAST_LEVEL_FOR_BIT(kDisableInflowIndex)) 
    {
        sendSmartBatteryCommand( kSBUCInflowDisable, LEVEL_FOR_BIT(kDisableInflowIndex));
    }
    
    // Perform kInhibitChargeIndex
    if ( LEVEL_FOR_BIT(kInhibitChargeIndex)
        != LAST_LEVEL_FOR_BIT(kInhibitChargeIndex)) 
    {
        sendSmartBatteryCommand( kSBUCChargeInhibit, LEVEL_FOR_BIT(kInhibitChargeIndex));
    }
    
    // Perform low battery warning
    if ( LEVEL_FOR_BIT(kDisableWarningsIndex)
        != LAST_LEVEL_FOR_BIT(kDisableWarningsIndex)) 
    {
        _setRootDomainProperty( CFSTR("BatteryWarningsDisabled"),
                                (LEVEL_FOR_BIT(kDisableWarningsIndex) ? kCFBooleanTrue : kCFBooleanFalse));
    }
    
    // Perform debug "disable real power sources" assertion
    if ( LEVEL_FOR_BIT(kNoRealPowerSourcesDebugIndex)
       != LAST_LEVEL_FOR_BIT(kNoRealPowerSourcesDebugIndex)) 
    {
        if (0 == LEVEL_FOR_BIT(kNoRealPowerSourcesDebugIndex))
        {
            switchActiveBatterySet(kBatteryShowReal);
        } else {
            switchActiveBatterySet(kBatteryShowFake);
        }
        
        // Kick power sources
        BatteryTimeRemainingBatteriesHaveChanged(NULL);
    }

    
    last_aggregate_assertions = aggregate_assertions;

    if (userAssertionBits != lastUserAssertionBits) {
        sendUserAssertionsToKernel(userAssertionBits);
    }
    lastUserAssertionBits = userAssertionBits;

    activateSettingOverrides();
    
    logASLAssertionSummary();

    // Tell the world that an assertion has been created, released,
    // or an interested application has died.
    // The state of active assertions may not have changed as a result
    // of this assertion action - i.e. may be no changes to user.

    /*
     * Publish the "levels changed" notification
     */
    notify_post( kIOPMAssertionsChangedNotifyString );


exit:
    return;
}
#pragma mark -
#pragma mark Act on assertions

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

__private_extern__ void
PMAssertions_prime(void)
{

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
    assertion_types_arr[kPushServiceTaskAssertion]  = kIOPMAssertionTypeApplePushServiceTask;
    evaluateAssertions();
    
    return;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#pragma mark -
#pragma mark Create


IOReturn doCreate(
    pid_t                   pid,
    CFDictionaryRef         newProperties,
    IOPMAssertionID         *assertion_id
) 
{
    IOReturn                result = kIOReturnInternalError;
    CFMutableDictionaryRef  this_task = NULL;
    CFMutableArrayRef       perTaskAssertions = NULL;
    CFNumberRef             task_pid_num = NULL;
    static uint16_t         id_cnt = 0;
    dispatch_source_t       proc_exit_source = NULL;
    
    // assertion_id will be set to kIOPMNullAssertionID on failure.
    *assertion_id = kIOPMNullAssertionID;
    
    task_pid_num = CFNumberCreate(0, kCFNumberIntType, &pid);
    
    /* Get dictionary describing this tasks's open assertions */
    if (gAssertionsDict) {
       this_task = (CFMutableDictionaryRef)CFDictionaryGetValue(gAssertionsDict, CAST_PID_TO_KEY(pid));
    }
    
    if (!this_task)
    {        
        // This is the first assertion for this process, so we'll create a new
        // assertion datastructure.
        this_task = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!this_task){
            result = kIOReturnNoMemory;
            goto exit;
        }

        char                this_proc_name[200];
        CFStringRef         proc_string = NULL;
        proc_name(pid, this_proc_name, sizeof(this_proc_name));
        proc_string = CFStringCreateWithCString(0, this_proc_name, kCFStringEncodingUTF8);
        if (proc_string) {
            CFDictionarySetValue(this_task, kIOPMTaskNameKey, proc_string);
            CFRelease(proc_string);
        }
        
        // Register for a notification when this task dies
        if (( proc_exit_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue()))) 
        {
            dispatch_source_set_event_handler(proc_exit_source, ^{
                HandleProcessExit(pid);
            });
            
            dispatch_source_set_cancel_handler(proc_exit_source, ^{
                dispatch_release(proc_exit_source);
            });
            
            dictionarySetDispatchSource(this_task, kIOPMTaskDispatchSourceKey, (void *)proc_exit_source);
        }

        if (task_pid_num) {
            CFDictionarySetValue(this_task, kIOPMTaskPIDKey, task_pid_num);
        }

        // gAssertionsDict is the global dictionary that maps a processes to their assertions -
        // Lazily created here.
        if (!gAssertionsDict) {
            gAssertionsDict = CFDictionaryCreateMutable(0, 0, NULL, &kCFTypeDictionaryValueCallBacks);
        }
        
        CFDictionarySetValue(gAssertionsDict, CAST_PID_TO_KEY(pid), this_task);
        CFRelease(this_task);
    }

    /* Get Array Listing all this task's open assertions */
    perTaskAssertions = (CFMutableArrayRef)CFDictionaryGetValue(this_task, kIOPMTaskAssertionsKey);
    if (!perTaskAssertions) 
    {
        perTaskAssertions = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
        CFDictionarySetValue(this_task, kIOPMTaskAssertionsKey, perTaskAssertions);
        CFRelease(perTaskAssertions);
    }

    // find empty slot
    CFIndex     arrayIndex;
    int         asst_count;
    asst_count = CFArrayGetCount(perTaskAssertions);
    for (arrayIndex=0; arrayIndex<asst_count; arrayIndex++) 
    {
        // find the first empty element in the array
        // empty elements are denoted by the value kCFBooleanFalse
        if (kCFBooleanFalse == CFArrayGetValueAtIndex(perTaskAssertions, arrayIndex)) 
            break;
    }
    if (arrayIndex >= kMaxTaskAssertions) 
    {
        // ERROR! out of space in array!
        result = kIOReturnNoMemory;
        goto exit;
    }

    /* 
     * Populate our new assertion dictionary 
     */
    CFMutableDictionaryRef          new_assertion_dict = NULL;
    CFDateRef                       start_date = NULL;

    *assertion_id = ID_FROM_INDEX(arrayIndex);
    
    new_assertion_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    /* Attach the Create Time */
    start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    if (start_date) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionCreateDateKey, start_date);
        CFRelease(start_date);
    }
    
    /* Attach the Owner's PID */    
    if (task_pid_num) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionPIDKey, task_pid_num);
    }
    
    /* Attach the globally unique AssertionID */
    uint64_t    assertion_id_64 = (uint64_t)*assertion_id;
    uint64_t    uniqueAIDInt    = MAKE_UNIQAID(pid, assertion_id_64, id_cnt);
    CFNumberRef uniqueAID       = CFNumberCreate(0, kCFNumberSInt64Type, &uniqueAIDInt);
    if (uniqueAID) {
        CFDictionarySetValue(new_assertion_dict, kIOPMAssertionGlobalUniqueIDKey, uniqueAID);
        CFRelease(uniqueAID);
    }
    id_cnt++; // Ignore overflow
    
    CFArraySetValueAtIndex(perTaskAssertions, arrayIndex, new_assertion_dict);

    CFRelease(new_assertion_dict);

    // Attach the rest of the properties to the new assertion dictionary, 
    // act on the newly asserted assertions, and kick off any new timeouts.
    result = doSetProperties(pid, *assertion_id, newProperties);

    // ASL Log assertion
    logASLAssertionEvent(kPMASLAssertionActionCreate, this_task, new_assertion_dict);

    if (proc_exit_source)
        dispatch_resume(proc_exit_source);
    
exit:    
    if (task_pid_num)
        CFRelease(task_pid_num);
    
    return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Creates a dictionary mapping process id's to the assertions that they own.
 *
 * The corresponding IOKit API IOPMCopyAssertionsByProcess returns a dictionary 
 * whose keys are process ID's.
 * This is perfectly acceptable in CoreFoundation, EXCEPT that you cannot
 * serialize a dictionary with CFNumbers for keys using CF or IOKit
 * serialization.
 *
 * To serialize this dictionary and pass it from configd to the caller's process,
 * we re-formatted it as a "flattened" array of dictionaries in configd, 
 * and we will re-constitute with pid's for keys here.
 *
 * Next time around, I will simply not use CFNumberRefs for keys in API.
 */
static CFArrayRef copyPIDAssertionDictionaryFlattened(void)
{
    CFDictionaryRef         *taskDataArray = NULL;
    CFMutableArrayRef       returnArray = 0;
    int                     dict_count;
    int                     task_index;
    
    if (!gAssertionsDict 
     || !(dict_count = CFDictionaryGetCount(gAssertionsDict)))
    {
        goto exit;            
    }

    returnArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    
    taskDataArray = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef)*dict_count);

    if (!taskDataArray) 
        goto exit;

    CFDictionaryGetKeysAndValues(gAssertionsDict, 
                                (const void **)NULL,   
                                (const void **)taskDataArray);

    // Iterate our master list of processes and map them to pids & their assertions
    for (task_index=0; task_index<dict_count; task_index++)
    {
        CFNumberRef             processID = NULL;
        CFArrayRef              processAssertions = NULL;
        CFMutableDictionaryRef  perProcessDictionary = NULL;
        
        if (!taskDataArray[task_index]) 
            continue;
        
        processID = CFDictionaryGetValue(
                                taskDataArray[task_index], 
                                kIOPMTaskPIDKey);

        processAssertions = CFDictionaryGetValue(
                                taskDataArray[task_index], 
                                kIOPMTaskAssertionsKey);

        if (!processID || !processAssertions)
            continue;

        /* Create an array copy of the process's array of tracking assertions.
         * Remove empty entries & remove internal data from each assertion,
         * before passing it out to user space.
         */

        CFMutableArrayRef       newProcessAssertionsArray = CFArrayCreateMutable(
                                            kCFAllocatorDefault, 
                                            CFArrayGetCount(processAssertions), 
                                            &kCFTypeArrayCallBacks);

        int                     arrayCount = CFArrayGetCount(processAssertions);

        int assertion_index;
        for (assertion_index = 0; assertion_index<arrayCount; assertion_index++)
        {
            CFMutableDictionaryRef assertionAtIndex = 
                        (CFMutableDictionaryRef)CFArrayGetValueAtIndex(processAssertions, assertion_index);
            CFMutableDictionaryRef assertionCopy = NULL;
            
            if (isA_CFDictionary(assertionAtIndex))
            {
                assertionCopy = CFDictionaryCreateMutableCopy(
                                            kCFAllocatorDefault,
                                            CFDictionaryGetCount(assertionAtIndex),
                                            assertionAtIndex);

            /* Strip out CFTimerRef from dictionaries. CFTimerRef types are not serializable
             * and can't be stored in the SCDynamicStore; and API clients don't need to see them.
             */

                if (assertionCopy)
                {
                    CFDictionaryRemoveValue(assertionCopy, kIOPMAssertionTimerRefKey);
                
                    CFArrayAppendValue(newProcessAssertionsArray, assertionCopy);

                    CFRelease(assertionCopy);
                }   
            }
        }


        /* Flattening begins */            
        perProcessDictionary = CFDictionaryCreateMutable(
                                kCFAllocatorDefault,
                                2,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);

        CFDictionarySetValue(perProcessDictionary,
                                kIOPMAssertionPIDKey,
                                processID);

        CFDictionarySetValue(perProcessDictionary,
                                CFSTR("PerTaskAssertions"),
                                newProcessAssertionsArray);
                                
        CFRelease(newProcessAssertionsArray);

        /* Flattening complete - pid & assertions are encapsulated within a dictionary now. 
         * IOKitUser framework will re-build the PID (key) to assertions array (value) dictionary
         * mapping. 
         */
        CFArrayAppendValue(returnArray, perProcessDictionary);

        CFRelease(perProcessDictionary);
    }
    
    free(taskDataArray);

exit:
    return returnArray;
}

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


/* copyAssertionForID()
 * Identifies the assertion "inID" for task "inPort".
 * Returns copies of the task's dictionary (immutable),
 * the array of assertions associated with the task (mutable),
 * and the assertion identified by ID (mutable).
 *
 * Caller must CFRelease() returned CF objects.
 *
 * Returns IOReturn code.
 */
static IOReturn copyAssertionForID(
    pid_t                   inPID,
    int                     inID,
    CFMutableDictionaryRef  *outTask,
    CFMutableArrayRef       *outTaskAssertions,
    CFMutableDictionaryRef  *outAssertion)
{
    CFMutableDictionaryRef  localTask = NULL;
    CFMutableArrayRef       localTaskAssertions = NULL;              
    CFMutableDictionaryRef  localAssertion = NULL;
    IOReturn                ret;

    if (outTask) {
        *outTask = NULL;
    }
    if (outTaskAssertions) {
        *outTaskAssertions = NULL;
    }
    if (outAssertion) {
        *outAssertion = NULL;
    }
    if (gAssertionsDict) {
        localTask = (CFMutableDictionaryRef)CFDictionaryGetValue(gAssertionsDict, CAST_PID_TO_KEY(inPID));
    }
    if (!localTask) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    localTaskAssertions = (CFMutableArrayRef)CFDictionaryGetValue(localTask, kIOPMTaskAssertionsKey);
    if (!localTaskAssertions) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    CFIndex arrayIndex = INDEX_FROM_ID(inID);
    if ((arrayIndex < 0) || (arrayIndex >= CFArrayGetCount(localTaskAssertions))) {
        ret = kIOReturnNotFound;
        goto exit;
    }

    localAssertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(localTaskAssertions, arrayIndex);
    if (!localAssertion 
     || !isA_CFDictionary(localAssertion)) {
        ret = kIOReturnNotFound;
        goto exit;
    }
    
    if (outTask) {
        CFRetain(localTask);
        *outTask = localTask;
    }
    if (outTaskAssertions) {
        CFRetain(localTaskAssertions);
        *outTaskAssertions = localTaskAssertions;
    }
    if (outAssertion) {
        CFRetain(localAssertion);
        *outAssertion = localAssertion;
    }

    ret = kIOReturnSuccess;

exit:
    return ret;
}

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

static IOReturn doRetain(pid_t pid, IOPMAssertionID id)
{
    CFMutableDictionaryRef     theAssertion;
    IOReturn            return_code;
    CFNumberRef         refCountNum;
    int                 refCount;
    
    return_code = copyAssertionForID(pid, id, NULL, NULL, &theAssertion);
    
    if (kIOReturnSuccess != return_code) {
        return return_code;
    }
    
    refCountNum = CFDictionaryGetValue(theAssertion, kIOPMAssertionRetainCountKey);
    
    if (refCountNum) {
        
        CFNumberGetValue(refCountNum, kCFNumberIntType, &refCount);
        
        refCount++;
        
        refCountNum = CFNumberCreate(0, kCFNumberIntType, &refCount);
        if (refCountNum) 
        {
            CFDictionarySetValue(theAssertion, kIOPMAssertionRetainCountKey, refCountNum);
            CFRelease(refCountNum);
        }
    }
    
    CFRelease(theAssertion);
    return kIOReturnSuccess;
}

static IOReturn doClearTimer(pid_t pid, IOPMAssertionID id, TimerType type)
{
    CFMutableDictionaryRef      assertion = NULL;
    dispatch_source_t           timerSource = NULL;
    
    /* 
     * Assert: The assertion referencing this timeout still exists and hasn't been released 
     *         at the time we run this code.
     */
    
    if (kIOReturnSuccess != copyAssertionForID(pid, id, NULL, NULL, &assertion))
        return kIOReturnNotFound;

        
    if (kTimerTypeTimedOut == type)
    {
        CFDateRef       dateNow = NULL;
        CFStringRef     timeoutAction = NULL;

        // This is a timed out assertion; mark it invalid with the timestamp.
        // Although marked invalid, we leave this assertion on the owning pid's assertion list
        // until it's released or reset with a valid level.
        
        if ((dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent()))) {
            CFDictionarySetValue(assertion, kIOPMAssertionTimedOutDateKey, dateNow);            
            CFRelease(dateNow);
        }
        
        /*
         * Zero out assertion levels on timeout with TimeoutAction = TurnOff
         */
        if ((timeoutAction = CFDictionaryGetValue(assertion, kIOPMAssertionTimeoutActionKey)))
        {

            // Put a copy of this assertion into our "timeouts" array.        
            appendTimedOutAssertion(assertion);

            if (CFEqual(kIOPMAssertionTimeoutActionTurnOff, timeoutAction)) 
            {         
                int i;
                CFNumberRef     levelOffNum = NULL;
                
                i = kIOPMAssertionLevelOff;
                levelOffNum = CFNumberCreate(0, kCFNumberIntType, &i);
                CFStringRef key = kIOPMAssertionLevelKey;
                
                CFDictionaryRef levelOffDict = CFDictionaryCreate(0, (const void **)&key, 
                               (const void **)&levelOffNum, 1, &kCFTypeDictionaryKeyCallBacks, 
                               &kCFTypeDictionaryValueCallBacks);
                
                doSetProperties(pid, id, levelOffDict);
                
                CFRelease(levelOffDict);
                CFRelease(levelOffNum);
            } else if (CFEqual(kIOPMAssertionTimeoutActionRelease, timeoutAction))
            {
                doRelease(pid, id);
            }
        }
        
        notify_post( kIOPMAssertionTimedOutNotifyString );
    }


    if ((timerSource = dictionaryGetDispatchSource(assertion, kIOPMAssertionTimerRefKey)))
    {
        dispatch_source_cancel(timerSource);

        // Delete our data tracking the timer
        CFDictionaryRemoveValue(assertion, kIOPMAssertionTimerRefKey);


        evaluateAssertions();    
        logASLAssertionEvent(kPMASLAssertionActionTimeOut, NULL, assertion);
    }
        
    CFRelease(assertion);
    
    return kIOReturnSuccess;
}



static IOReturn doResetTimer(pid_t pid, IOPMAssertionID id)
{
    CFMutableDictionaryRef      timeoutAssertion = NULL;
    IOReturn                    ret;
    CFNumberRef                 intervalNum;
    CFTimeInterval              intervalCFTime;
    uint64_t                    intervalInt;
    dispatch_source_t           old_timer = NULL;
    dispatch_source_t           t;
    
    ret = copyAssertionForID(pid, id, NULL, NULL, &timeoutAssertion);
    if (kIOReturnSuccess != ret) {
        ret = kIOReturnNotFound;
        goto exit;
    }
        
    intervalNum = CFDictionaryGetValue(timeoutAssertion, kIOPMAssertionTimeoutKey);
    if (!intervalNum) {
        ret = kIOReturnError;
        goto exit;
    }
    CFNumberGetValue(intervalNum, kCFNumberDoubleType, &intervalCFTime);
    
    intervalInt = lround(intervalCFTime * (CFTimeInterval)1000);
    intervalInt *= 1000*1000;

     if (intervalInt == 0) {
        /* No timeout for 0 intervalInt */
        ret = kIOReturnSuccess;
        goto exit;
    }
   
    if ((old_timer = dictionaryGetDispatchSource(timeoutAssertion, kIOPMAssertionTimerRefKey))) {
        dispatch_source_cancel(old_timer);
    }

    t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    
    dictionarySetDispatchSource(timeoutAssertion, kIOPMAssertionTimerRefKey, t);
    
    dispatch_source_set_timer(t, dispatch_time(DISPATCH_TIME_NOW, intervalInt), DISPATCH_TIME_FOREVER, 0);
    
    dispatch_source_set_event_handler(t, ^{
        doClearTimer(pid, id, kTimerTypeTimedOut); 
    });
    
    dispatch_source_set_cancel_handler(t, ^{
        dispatch_release(t);
    });
    
    dispatch_resume(t);
    
exit:
    if (timeoutAssertion)
        CFRelease(timeoutAssertion);

    return ret;
}        

#pragma mark -
#pragma mark Glue

static void evalUserActivityAssertion(CFMutableDictionaryRef thisAssertion, unsigned int dispSlpTimer)
{
    CFStringRef type;
    CFDateRef           updateDate     = NULL;
    CFTimeInterval      expiredSecs = 0.0;
    CFAbsoluteTime      updateCFTime   = 0.0;
    CFNumberRef         uniqueAidCF = NULL;
    uint64_t            uniqueAid = 0;
    CFNumberRef         newTimeoutCF = NULL;
    CFTimeInterval      newTimeout = 0;

    unsigned int dispSlpTimerSecs = dispSlpTimer * 60;

    uniqueAidCF = CFDictionaryGetValue(thisAssertion, kIOPMAssertionGlobalUniqueIDKey);
    if (uniqueAidCF) {
        CFNumberGetValue(uniqueAidCF, kCFNumberSInt64Type, &uniqueAid);
    }
    if (uniqueAid == 0)
        return;

    type = CFDictionaryGetValue(thisAssertion, kIOPMAssertionTypeKey);
    if ( !isA_CFString(type) || !CFEqual(type, kIOPMAssertionUserIsActive))  {
        return;
    }

    if((updateDate = CFDictionaryGetValue(thisAssertion, kIOPMAssertionCreateDateKey))) 
    {
        /* 
         * If 'TimerUpdateKey' is missing(bcos idle display sleep timer was 0 when assertion was created), 
         * then use assertion create time 
         */
        updateCFTime = CFDateGetAbsoluteTime(updateDate);
    }

    expiredSecs =  CFAbsoluteTimeGetCurrent() - updateCFTime;

    if (dispSlpTimerSecs == 0) 
        newTimeout = 0;
    else
        newTimeout = dispSlpTimerSecs - expiredSecs;


    if (newTimeout >= 0) {

        newTimeoutCF = CFNumberCreate(0, kCFNumberDoubleType, &newTimeout);
        CFDictionarySetValue(thisAssertion, kIOPMAssertionTimeoutKey, newTimeoutCF);
        CFRelease(newTimeoutCF);

        if (newTimeout == 0)
            /* No idle display sleep. Just remove the timer and let assertion stay forever */
            doClearTimer(GET_ASSERTPID(uniqueAid), GET_ASSERTID(uniqueAid), kTimerTypeReleased );
        else
            doResetTimer(GET_ASSERTPID(uniqueAid), GET_ASSERTID(uniqueAid) );

    }
    else if (expiredSecs >= dispSlpTimerSecs) {
        /* Release the assertion */

        doRelease(GET_ASSERTPID(uniqueAid), GET_ASSERTID(uniqueAid) );
    }

}

/* 
 * Called when display Sleep Timer setting is changed. The timeout value
 * for all assertions of type kIOPMAssertionUserIsActive is modified
 * to reflect the new display Sleep timer value.
 */
__private_extern__ void evalAllUserActivityAssertions(unsigned int dispSlpTimer)
{

    static unsigned int prevDispSlpTimer = 0;

    if (prevDispSlpTimer == dispSlpTimer)
        return;

    prevDispSlpTimer = dispSlpTimer;

    applyToAllAssertionsSync(^(CFDictionaryRef itsTaskDictionary, CFDictionaryRef  thisAssertion) 
            {
                evalUserActivityAssertion((CFMutableDictionaryRef)thisAssertion, dispSlpTimer); 
            });


    evaluateAssertions();
}

#pragma mark -
#pragma mark ASL Log




#if !TARGET_OS_EMBEDDED

/* logASLAssertionEvent
 *
 * Logs a message describing an assertion event that just occured.
 * - Action = Created/Released/TimedOut/Died/System
 * 
 * Message = "%s(Action) %d(pid) %s(AssertionLocalizableName)(or just the Name) %s(AssertionsHeld=Level)"
 */
static void logASLAssertionEvent(
    const char      *assertionAction,
    CFDictionaryRef taskDictionary,
    CFDictionaryRef assertionDictionary)
{
    const int       kLongStringLen          = 200;
    const int       kShortStringLen         = 10;
    aslmsg          m;
    int             app_pid                 = -1;
    CFNumberRef     pidNum                  = NULL;
    CFStringRef     foundAssertionType      = NULL;
    CFStringRef     foundAssertionName      = NULL;
    char            pid_buf[kShortStringLen];
    char            assertionTypeCString[kLongStringLen];
    char            assertionNameCString[kLongStringLen];
    char            aslMessageString[kLongStringLen];
    
    if (!doDebugASLAssertions) 
        return;

    m = asl_new(ASL_TYPE_MSG);
    
    asl_set(m, kMsgTracerDomainKey, kMsgTracerDomainPMAssertions);
    pid_buf[0] = 0;
    
    if (taskDictionary) {
        // Try to read pid from Task's dictionary
        pidNum = CFDictionaryGetValue(taskDictionary, kIOPMTaskPIDKey);
    } 
    if (!pidNum && assertionDictionary) {
        // Try to read pid from Assertion's dictionary
        pidNum = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionPIDKey);
    }
    if (pidNum && CFNumberGetValue(pidNum, kCFNumberIntType, &app_pid)) 
    {
        if (0 < snprintf(pid_buf, kShortStringLen, "%d", app_pid)) {
            asl_set(m, kPMASLPIDKey, pid_buf);
        }
    }

    asl_set(m, kPMASLActionKey, assertionAction);
    
    if (assertionDictionary)
    {
        /* When we're logging for client death, we don't have assertionDictionary.
         * 
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
         * Retain count
         */
        CFNumberRef retainCountNum = NULL;
        int retainCount = 0;
        if ( (retainCountNum = CFDictionaryGetValue(assertionDictionary, kIOPMAssertionRetainCountKey)) )
        {
            CFNumberGetValue(retainCountNum, kCFNumberIntType, &retainCount);
            if (0 != retainCount) 
            {
                char    retainCountBuf[kShortStringLen];
                snprintf(retainCountBuf, sizeof(retainCountBuf), "%d", retainCount);
                asl_set(m, "RetainCount", retainCountBuf);            
            }
        }
    }
     
    snprintf(aslMessageString, sizeof(aslMessageString), "PID %s %s %s %s%s%s Aggregate:0x%x", 
                pid_buf, assertionAction, 
                foundAssertionType ? assertionTypeCString:"",
                foundAssertionName?"\"":"", foundAssertionName ? assertionNameCString:"", 
                foundAssertionName?"\"":"",
                aggregate_assertions);

    asl_set(m, ASL_KEY_MSG, aslMessageString);    
    
    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);    

    /* Facility = "internal"
     * is required to make sure this message goes into ASL database, but not
     * into system.log
     */
//    asl_set(m, ASL_KEY_FACILITY, "internal");

    asl_set(m, kPMASLMessageKey, kPMASLMessageLogValue);

    asl_send(NULL, m);

    asl_free(m);
}


static void logASLAssertionSummary(void)
{
    const int       kLongStringLen      = 100;
    int             i;
    int             assertions_logged   = 0;
    aslmsg          m;
    char            assertionLevelString[kLongStringLen];
    char            assertionSummaryString[kLongStringLen * kIOPMNumAssertionTypes + 1];
    char            assertionTypeString[kLongStringLen];

    if (!doDebugASLAssertions) 
        return;

    m = asl_new(ASL_TYPE_MSG);
    asl_set(m, kMsgTracerDomainKey, kMsgTracerDomainPMAssertions);
    
    bzero(assertionSummaryString, sizeof(assertionSummaryString));
    
    /* Log Summary of assertion levels */
    snprintf(assertionSummaryString, sizeof(assertionLevelString), "Assertion Summary: ");
    for (i=0; i<kIOPMNumAssertionTypes; i++) 
    {
        //
        if (LEVEL_FOR_BIT(i)
            && !(i == kEnableIdleIndex && _IdleEnableAssertionIsMeaningless))
        {
            bzero(assertionLevelString, sizeof(assertionLevelString));
            
            assertions_logged++;

            CFStringGetCString(assertion_types_arr[i], assertionTypeString, 
                                    sizeof(assertionTypeString), kCFStringEncodingMacRoman);

            snprintf(assertionLevelString, sizeof(assertionLevelString), "%s=%d ", 
                                    assertionTypeString, LEVEL_FOR_BIT(i));

            strlcat(assertionSummaryString, assertionLevelString, sizeof(assertionSummaryString));
        }
    }

    if (0 == assertions_logged)
    {
        strlcat(assertionSummaryString, "None", sizeof(assertionSummaryString));    
    }
    
    /* Facility = "internal"
     * is required to make sure this message goes into ASL database, but not
     * into system.log
     */
//    asl_set(m, ASL_KEY_FACILITY, "internal");

    asl_set(m, ASL_KEY_MSG, assertionSummaryString);
    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);    
    asl_set(m, kPMASLMessageKey, kPMASLMessageLogValue);

    asl_send(NULL, m);
    asl_free(m);
}
#endif

#ifndef     kIOPMRootDomainWakeTypeKey
#define     kIOPMRootDomainWakeTypeKey              CFSTR("Wake Type")
#endif

#ifndef     kIOPMRootDomainWakeTypeNetwork
#define     kIOPMRootDomainWakeTypeNetwork          CFSTR("Network")
#endif

#ifndef     kIOPMRootDomainWakeTypeAlarm
#define     kIOPMRootDomainWakeTypeAlarm            CFSTR("Alarm")
#endif

#ifndef     kIOPMRootDomainWakeTypeMaintenance  
#define     kIOPMRootDomainWakeTypeMaintenance      CFSTR("Maintenance")
#endif

#ifndef     kIOPMRootDomainWakeTypeSleepTimer
#define     kIOPMRootDomainWakeTypeSleepTimer       CFSTR("SleepTimer")
#endif

#ifndef kIOPMRootDomainWakeTypeSleepService
#define kIOPMRootDomainWakeTypeSleepService         CFSTR("SleepService")
#endif

#define     kDarkWakeNetworkHoldForSeconds          30
#define     kDeviceEnumerationHoldForSeconds        (45LL)

#pragma mark -
#pragma mark powerd-Internal Use Only


__private_extern__ void PMAssertions_TurnOffAssertions_ApplePushServiceTask(void)
{

    asl_log(NULL, NULL, ASL_LEVEL_ERR, "SleepService: Releasing all SleepService assertions.!\n");    

    
    gSleepService.pushAssertionsWereTimedOutCount = 0;
    
    applyToAllAssertionsSync(^(CFDictionaryRef itsTaskDictionary, CFDictionaryRef d){
        CFStringRef t;
        CFNumberRef n;
        uint32_t levelInt = kIOPMAssertionLevelOff;
        CFNumberRef levelNum = NULL;
        CFMutableDictionaryRef  assertion = (CFMutableDictionaryRef)d;

        int    nx = -1;
        if (assertion) {
            t = CFDictionaryGetValue(assertion, kIOPMAssertionTypeKey);
            n = CFDictionaryGetValue(assertion, kIOPMAssertionLevelKey);
            if (n) 
                CFNumberGetValue(n, kCFNumberIntType, &nx);
            
            if (t && CFEqual(t, kIOPMAssertionTypeApplePushServiceTask) 
                    && (kIOPMAssertionLevelOn == nx)) 
            {
                levelNum = CFNumberCreate(0, kCFNumberIntType, &levelInt);
                CFDictionarySetValue(assertion, kIOPMAssertionLevelKey, levelNum);
                CFDictionarySetValue(assertion, kIOPMAssertionLevelsBits, levelNum);
                CFRelease(levelNum);
                
                gSleepService.pushAssertionsWereTimedOutCount++;
                
                {           
                    /* Log timed out Assertions */
                    CFNumberRef pidnum = NULL;
                    CFStringRef pidstring = NULL;
                    pidnum = CFDictionaryGetValue(assertion, kIOPMAssertionPIDKey);
                    if (pidnum) {
                        int pid_int = 0;
                        CFNumberGetValue(pidnum, kCFNumberIntType, &pid_int);
                        pidstring = CFStringCreateWithFormat(0, 0, CFSTR("%d"), pid_int);
                    }
                    pidnum = CFNumberCreate(0, kCFNumberIntType, &gSleepService.capTime);
                    
                    logASLMessageApplicationResponse(kAppResponseLogSourceSleepServiceCap, 
                                                     CFDictionaryGetValue(itsTaskDictionary, kIOPMTaskNameKey),
                                                     CFSTR(kMsgTracerDomainSleepServiceCapApp), 
                                                     pidnum, -1);
                    if (pidnum) {
                        CFRelease(pidnum);
                    }
                    if (pidstring) {
                        CFRelease(pidstring);
                    }

                }
            }
        }
    });

    evaluateAssertions();
}

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
    CFDictionaryRef properties, 
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
        if ( *outID != kIOPMNullAssertionID )
            doRelease(getpid(), *outID);
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
        if (deInfo->dispSrc)
            dispatch_suspend(deInfo->dispSrc);
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
        }
        dispatch_source_set_timer(deInfo->dispSrc, dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC), 
                                                DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(deInfo->dispSrc);
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
    IOPMAssertionID     pushSvcAssert = kIOPMNullAssertionID;

    if ( !(kIOPMSystemCapabilityDidChange & capArgs->changeFlags) )
       return;


    if ( IS_DARK_STATE(capArgs->toCapabilities) &&
          IS_OFF_STATE(capArgs->fromCapabilities) )
    {
        wakeType = _copyRootDomainProperty(kIOPMRootDomainWakeTypeKey);
        if (isA_CFString(wakeType))
        {
            if (CFEqual(wakeType, kIOPMRootDomainWakeTypeNetwork))
                _DarkWakeHandleNetworkWake( );
            else if (CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService) )
            {
                _localCreateAssertionWithTimer(kIOPMAssertionTypeApplePushServiceTask, kIOPMAssertionLevelOn,
                                    CFSTR("Powerd - Wait for client pushService assertions"), 10, &pushSvcAssert);
            }
            else if ((CFEqual(wakeType, kIOPMRootDomainWakeTypeAlarm) == false ) &&
                     (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) == false ) &&
                     (CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepTimer) == false ))
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


/* Unwrap a pointer stored in a CFData, so we can place it an a CF container.
 */
static void dictionarySetDispatchSource(CFMutableDictionaryRef d, CFStringRef k, dispatch_source_t p)
{ 
    CFDataRef tmpData = CFDataCreate(0, (uint8_t *)(&p), sizeof(dispatch_source_t));
    if (tmpData) {
        CFDictionarySetValue(d, k, tmpData);
        CFRelease(tmpData);
    }
}

/* Wrap a pointer to a non-CF object inside a CFData, so that 
 * we can place it an a CF container.
 */
static dispatch_source_t dictionaryGetDispatchSource(CFDictionaryRef d, CFStringRef k)
{
    void *return_val = 0;
    void *databytesptr = NULL;
    
    CFDataRef tmpData = CFDictionaryGetValue(d, k); 
    
    if (tmpData) {
        databytesptr = (void *)CFDataGetBytePtr(tmpData);
        return_val = *((void **)databytesptr);
    }
    
    return (dispatch_source_t)return_val;
}


