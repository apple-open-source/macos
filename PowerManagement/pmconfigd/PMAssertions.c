/*
 * Copyright (c) 2005-2013 Apple Computer, Inc. All rights reserved.
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

#include <CoreFoundation/CFXPCBridge.h>
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
#include <IOReport.h>
#include <IOKit/IOReportTypes.h>
#include <Security/SecTask.h>
#if !TARGET_OS_SIMULATOR
#include <energytrace.h>
#endif
#include <xpc/private.h>
#include <xpc/xpc.h>
#include <os/state_private.h>



#include "PMConnection.h"
#include "PrivateLib.h"
#include "PMSettings.h"
#include "PMAssertions.h"
#include "BatteryTimeRemaining.h"
#include "PMStore.h"
#include "powermanagementServer.h"
#include "SystemLoad.h"
#include "Platform.h"



os_log_t    assertions_log = NULL;
#undef   LOG_STREAM
#define  LOG_STREAM   assertions_log

#define FMMD_WIPE_BOOT_ARG          "fmm-wipe-system-status"

/*! @constant kIOPMAssertionTimeoutOnResumeKey
 *  @abstract Records number of seconds left when the assertion was suspended
 */
#define kPMAssertionTimeoutOnResumeKey CFSTR("_AssertTimeoutOnResume")



// CAST_PID_TO_KEY casts a mach_port_t into a void * for CF containers
#define CAST_PID_TO_KEY(x)          ((void *)(uintptr_t)(x))

#define LEVEL_FOR_BIT(idx)          (getAssertionLevel(idx))



/*
 * Maximum delay allowed(in Mins) for turning off the display after the
 * PreventDisplayIdleSleep assertion is released
 */
#define kPMMaxDisplayTurnOffDelay  (5)

#define kKernelAssertionType "Kernel Assertion"
#define kKernelIdleSleepPreventer "Idle Sleep Preventer"
#define kKernelSystemSleepPreventer "System Sleep Preventer"

// Globals

uint32_t gSAAssertionBehaviorFlags = kIOPMSystemActivityAssertionEnabled;

CFArrayRef copyScheduledPowerEvents(void);
CFDictionaryRef copyRepeatPowerEvents(void);

// forward

static void                         sendSmartBatteryCommand(uint32_t which, uint32_t level);
static void                         sendUserAssertionsToKernel(uint32_t user_assertions);
static void                         evaluateForPSChange(void);
static void                         HandleProcessExit(pid_t deadPID);

static bool                         callerIsEntitledToAssertion(audit_token_t token,
                                                                CFDictionaryRef newAssertionProperties);
__private_extern__ void             logASLAssertionsAggregate(void);


STATIC IOReturn                     lookupAssertion(pid_t pid, IOPMAssertionID id, assertion_t **assertion);
STATIC bool                         propertiesDictRequiresRoot(CFDictionaryRef   props);
STATIC IOReturn                     doRetain(pid_t pid, IOPMAssertionID id, int *retainCnt);
STATIC IOReturn                     doRelease(pid_t pid, IOPMAssertionID id, int *retainCnt);
STATIC IOReturn                     doSetProperties(pid_t pid,
                                                    IOPMAssertionID id, 
                                                    CFDictionaryRef props,
                                                    int *enTrIntensity);

STATIC CFArrayRef                   copyPIDAssertionDictionaryFlattened(int state);
static CFArrayRef                   copyAssertionsByType(CFStringRef type);
static CFDictionaryRef              copyAggregateValuesDictionary(void);

STATIC IOReturn                     doCreate(pid_t pid, CFMutableDictionaryRef newProperties,
                                             IOPMAssertionID *assertion_id, ProcessInfo **pinfo,
                                             int *enTrIntensity);
STATIC IOReturn                     copyAssertionForID(pid_t inPID, int inID,
                                                       CFMutableDictionaryRef  *outAssertion);

static ProcessInfo*                 processInfoCreate(pid_t p);
static ProcessInfo*                 processInfoRetain(pid_t p);
STATIC void                         processInfoRelease(pid_t p);
static ProcessInfo*                 processInfoGet(pid_t p);
static void                         setClamshellSleepState(void);
static int                          getAssertionTypeIndex(CFStringRef type);

STATIC void                         handleAssertionTimeout(assertionType_t *assertType);
static void                         resetGlobalTimer(assertionType_t *assertType, uint64_t timer);
static IOReturn                     raiseAssertion(assertion_t *assertion);
static void                         allocStatsBuf(ProcessInfo *pinfo);
static void                         releaseStatsBufByPid(pid_t p);

__private_extern__ bool             isDisplayAsleep(void);
__private_extern__ void             logASLMessageSleepServiceTerminated(int forcedTimeoutCnt);

// globals
static uint32_t                     kerAssertionBits = 0;
static int                          aggregate_assertions;
static CFStringRef                  assertion_types_arr[kIOPMNumAssertionTypes];

static CFMutableDictionaryRef       gAssertionsArray = NULL;
static CFMutableDictionaryRef       gKernelAssertionsArray =  NULL;
static uint32_t                     gKernelAssertions = 0;
static CFMutableDictionaryRef       gUserAssertionTypesDict = NULL;
CFMutableDictionaryRef              gProcessDict = NULL;
assertionType_t                     gAssertionTypes[kIOPMNumAssertionTypes];
assertionEffect_t                   gAssertionEffects[kMaxAssertionEffects];
uint32_t                            gDisplaySleepTimer = 0;      /* Display Sleep timer value in mins */
unsigned long                       gIdleSleepTimer = 0;         /* Idle Sleep timer value in mins */


extern uint32_t                     gDebugFlags;

/* Number of procs interested in kIOPMAssertionsAnyChangedNotifyString notification */
static  uint32_t                    gAnyChange = 0;
/* Number of procs interested in kIOPMAssertionsChangedNotifyString notification */
static  uint32_t                    gAggChange = 0;
/* Number of procs interested in kIOPMAssertionTimedOutNotifyString notification */
static  uint32_t                    gTimeoutChange = 0;

uint32_t                            gActivityAggCnt = 0; // Number of requests received to enable activity aggregation

CFDictionaryRef                     gProcAssertionLimits = NULL;
dispatch_source_t                   gProcAggregateMonitor = NULL;
uint64_t                            gProcMonitorFrequency = (2 *3600LL * NSEC_PER_SEC);  // Once every two hours
CFDictionaryRef                     gProcAggregateBasis = NULL;

dispatch_source_t                   gAggCleanupDispatch = NULL;  // Dispatch to release statsbuf of dead procs
uint64_t                            gAggCleanupFrequency = (15 * NSEC_PER_SEC);  // Once every 4 hours
sysQualifier_t                      gSysQualifier;

static CFMutableSetRef              gPendingResponses = NULL;
static long                         gPendingAckToken = 0;
static uint32_t                     gSleepBlockers = 0;
__private_extern__ void             sendSleepNotificationResponse(void *acknowledgementToken, bool allow);

// list for storing idle and system sleep preventers
LIST_HEAD(, assertion) gIdleSleepPreventersList = LIST_HEAD_INITIALIZER(gIdleSleepPreventersList);
LIST_HEAD(, assertion) gSystemSleepPreventersList = LIST_HEAD_INITIALIZER(gSystemSleepPreventersList);

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#pragma mark -
#pragma mark XPC
/******************************************************************************
  * XPC Handlers
 *****************************************************************************/
void asyncAssertionCreate(xpc_object_t remoteConnection, xpc_object_t msg)
{

    audit_token_t       token;
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;
    int                 enTrIntensity = -1;
    IOPMAssertionID     assertionId = kIOPMNullAssertionID;
    ProcessInfo         *pinfo = NULL;
    IOReturn            return_code;
    xpc_object_t        msgDictionary;
    IOPMAssertionID     remoteId = kIOPMNullAssertionID;

    CFNumberRef     numRef = NULL;
    CFDictionaryRef props = NULL;
    CFMutableDictionaryRef mutableProps = NULL;


    msgDictionary = xpc_dictionary_get_value(msg, kAssertionCreateMsg);
    if (!msgDictionary) {
        ERROR_LOG("Failed to retrieve dictionary from Create  message\n");
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    props = (CFMutableDictionaryRef)_CFXPCCreateCFObjectFromXPCMessage(msgDictionary);
    if (!isA_CFDictionary(props)) {
        ERROR_LOG("Received unexpected data type for assertion creation\n");
        return_code = kIOReturnBadArgument;
        goto exit;
    }

#ifndef XCTEST
    xpc_connection_get_audit_token(remoteConnection, &token);
    callerPID = xpc_connection_get_pid(remoteConnection);
    callerUID = xpc_connection_get_euid(remoteConnection);
    callerGID = xpc_connection_get_egid(remoteConnection);
#else
    callerPID=(pid_t) XCTEST_PID;
#endif
    mutableProps = CFDictionaryCreateMutableCopy(NULL, 0, props);
    numRef = CFDictionaryGetValue(mutableProps, kIOPMAsyncClientAssertionIdKey);
    if (isA_CFNumber(numRef))  {
        CFNumberGetValue(numRef, kCFNumberSInt32Type, &remoteId);
    }
#ifndef XCTEST
    if (!callerIsEntitledToAssertion(token, mutableProps))
    {
        ERROR_LOG("Process %d is not entitled to create assertion(remoteId:0x%x)\n", 
                callerPID, remoteId);
        return_code = kIOReturnNotPrivileged;
        goto exit;
    }
#endif

    // Check for privileges if the assertion requires it.
    if (propertiesDictRequiresRoot(mutableProps)
        && ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID))))
    {
        return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    return_code = doCreate(callerPID, mutableProps, &assertionId, &pinfo, &enTrIntensity);
#ifndef XCTEST
    pinfo->remoteConnection = xpc_retain(remoteConnection);
#endif
    DEBUG_LOG("Created assertion with id 0x%x for remote id 0x%x from pid %d\n",
            assertionId, remoteId, callerPID);
exit:

    if (props) {
        CFRelease(props);
    }
    if (mutableProps) {
        CFRelease(mutableProps);
    }
#ifndef XCTEST
    xpc_object_t reply = xpc_dictionary_create_reply(msg);
    if (!reply) {
        // This is fatal...
        ERROR_LOG("Failed to create the xpc object to send response\n");
        return;
    }

    xpc_dictionary_set_uint64(reply, kMsgReturnCode, return_code);
    xpc_dictionary_set_uint64(reply, kAssertionIdKey, assertionId);
    xpc_dictionary_set_uint64(reply, kAssertionEnTrIntensityKey, enTrIntensity);
    xpc_connection_send_message(remoteConnection, reply);
#else
    xpc_dictionary_set_uint64(msg, kAssertionIdKey, assertionId);
    xpc_dictionary_set_uint64(msg, kMsgReturnCode, return_code);
#endif
}

void asyncAssertionRelease(xpc_object_t remoteConnection, xpc_object_t msg)
{
    pid_t               callerPID = -1;
    IOPMAssertionID assertionId;
    IOReturn rc;
    int retainCnt;

    assertionId = (IOPMAssertionID)xpc_dictionary_get_uint64(msg, kAssertionReleaseMsg);
#ifndef XCTEST
    callerPID = xpc_connection_get_pid(remoteConnection);
#else
    callerPID=(pid_t) XCTEST_PID;
#endif
    
    rc = doRelease(callerPID, assertionId, &retainCnt);
    if (rc != kIOReturnSuccess) {
        ERROR_LOG("Failed to release assertion id 0x%x (rc:0x%x)\n", assertionId, rc);
    }
    else {
        DEBUG_LOG("Released assertion 0x%x\n", assertionId);
    }
#if XCTEST
    xpc_dictionary_set_uint64(msg, kMsgReturnCode, rc);
#endif
}

void asyncAssertionProperties(xpc_object_t remoteConnection, xpc_object_t msg)
{

    pid_t               callerPID = -1;
    int                 enTrIntensity = -1;
    IOPMAssertionID     assertionId = kIOPMNullAssertionID;
    IOReturn            rc;
    CFTypeRef           cfObj;
    xpc_object_t        msgDictionary;
    CFDictionaryRef     newAssertionProperties = NULL;
    audit_token_t       token;

    msgDictionary = xpc_dictionary_get_value(msg, kAssertionPropertiesMsg);
    if (!msgDictionary) {
        ERROR_LOG("Failed to retrieve dictionary from Properties message\n");
        rc = kIOReturnBadArgument;
        goto exit;
    }
    newAssertionProperties = _CFXPCCreateCFObjectFromXPCMessage(msgDictionary);
    if (!isA_CFDictionary(newAssertionProperties)) {
        ERROR_LOG("Received unexpected data type for assertion creation\n");
        rc = kIOReturnBadArgument;
        goto exit;
    }
#ifndef XCTEST
    xpc_connection_get_audit_token(remoteConnection, &token);
    if (!callerIsEntitledToAssertion(token, newAssertionProperties)) {
        rc = kIOReturnNotPrivileged;
        goto exit;
    }
#endif

    cfObj = CFDictionaryGetValue(newAssertionProperties, kIOPMAssertionIdKey);
    if (isA_CFNumber(cfObj)) {
        CFNumberGetValue(cfObj, kCFNumberIntType, &assertionId);
    }
    else {
        ERROR_LOG("Failed to retrieve assertion Id from Properties message\n");
        rc = kIOReturnBadArgument;
        goto exit;
    }
#ifndef XCTEST
    callerPID = xpc_connection_get_pid(remoteConnection);
#else
    callerPID = XCTEST_PID;
#endif
    rc = doSetProperties(callerPID, assertionId, newAssertionProperties, &enTrIntensity);

    DEBUG_LOG("Updated properties for assertion id 0x%x(rc:0x%x)\n", assertionId, rc);

exit:

    if (newAssertionProperties) {
        CFRelease(newAssertionProperties);
    }

    if (rc != kIOReturnSuccess) {
        ERROR_LOG("Failed to change properties for assertion id 0x%x (rc:0x%x)\n", assertionId, rc);
    }
#if XCTEST
    xpc_dictionary_set_uint64(msg, kMsgReturnCode, rc);
#endif
}

void processSetAssertionState(xpc_connection_t peer, xpc_object_t msg)
{
    pid_t pid = -1;
    IOPMAssertionProcessStateType processState = kIOPMAssertionProcessStateSuspend;

    SecTaskRef secTask = NULL;
    CFTypeRef  entitled_SuspendResume = NULL;
    audit_token_t token;

    xpc_connection_get_audit_token(peer, &token);

    secTask = SecTaskCreateWithAuditToken(kCFAllocatorDefault, token);

    if (secTask) {
        entitled_SuspendResume = SecTaskCopyValueForEntitlement(
            secTask, kIOPMAssertionSuspendResumeEntitlement, NULL);
    }

    if (!entitled_SuspendResume) {
        os_log_error(OS_LOG_DEFAULT, "Assertion Suspend/Resume request from peer %p(pid %d)"
                     " does not have appropriate entitlement\n", peer, xpc_connection_get_pid(peer));
        goto exit;
    }

    pid = (pid_t)xpc_dictionary_get_uint64(msg, "pid");
    processState = (IOPMAssertionProcessStateType)xpc_dictionary_get_uint64(msg, kAssertionSetStateMsg);

    if (processState == kIOPMAssertionProcessStateSuspend)
        handleAssertionSuspend(pid);
    else
        handleAssertionResume(pid);

exit:
    if (secTask) {
        CFRelease(secTask);
    }
    if (entitled_SuspendResume) {
        CFRelease(entitled_SuspendResume);
    }
}

void releaseConnectionAssertions(xpc_object_t remoteConnection)
{
    pid_t deadPID = xpc_connection_get_pid(remoteConnection);
    ProcessInfo         *pinfo = NULL;

    DEBUG_LOG("Clearing up assertions from pid %d\n", deadPID);
    pinfo = processInfoGet(deadPID);
    if (pinfo && pinfo->remoteConnection) {
        HandleProcessExit(deadPID);
    }
}

void sendAssertionTimeoutMsg(assertion_t *assertion)
{
    xpc_object_t    msg = NULL;
    ProcessInfo     *pinfo = assertion->pinfo;
    CFNumberRef     numRef = NULL;
    IOPMAssertionID asyncClientID = kIOPMNullAssertionID;

    if (!pinfo->remoteConnection) {
        return;
    }

    msg = xpc_dictionary_create(NULL, NULL, 0);
    if (!msg) {
        ERROR_LOG("Failed to create xpc msg object for pid %d to notify assertion 0x%x timeout\n", 
                  pinfo->pid, assertion->assertionId);
        return;
    }
    numRef = CFDictionaryGetValue(assertion->props, kIOPMAsyncClientAssertionIdKey);
    if (isA_CFNumber(numRef))  {
        CFNumberGetValue(numRef, kCFNumberSInt32Type, &asyncClientID);
    }
    else {
        ERROR_LOG("Async client id is not found for id 0x%x\n", assertion->assertionId);
        goto exit;
    }
    xpc_dictionary_set_uint64(msg, kAssertionTimeoutMsg, asyncClientID);

    DEBUG_LOG("Sending assertion timeout message for id 0x%x\n", assertion->assertionId);

    xpc_connection_send_message(pinfo->remoteConnection, msg);

exit:
    xpc_release(msg);
}

void handleAssertionCheckTimeout(const void *value, void *context)
{
    ProcessInfo *pinfo = (ProcessInfo *)value;

    if (!pinfo->proc_exited) {
        ERROR_LOG("Timed out waiting for assertion check response from pid %d\n",
                pinfo->pid);
    }
    processInfoRelease(pinfo->pid);
}


void sendCheckAssertionsMsg(ProcessInfo *pinfo, void(^responseHandler)(xpc_object_t))
{
    xpc_object_t            msg = NULL;


    msg = xpc_dictionary_create(NULL, NULL, 0);
    if (!msg) {
        ERROR_LOG("Failed to create xpc msg object for pid %d to check assertions\n", pinfo->pid);
        return;
    }
    DEBUG_LOG("Sending assertion check message to pid %d\n", pinfo->pid);
    xpc_dictionary_set_uint64(msg, kAssertionCheckMsg, 0);
    xpc_dictionary_set_uint64(msg, kAssertionCheckTokenKey,gPendingAckToken);

    xpc_connection_send_message_with_reply(pinfo->remoteConnection, msg, _getPMMainQueue(), responseHandler);
    xpc_release(msg);


}

void processAssertionCheckResp(xpc_object_t reply, pid_t pid)
{
    long        token;
    uint32_t    blockers;
    ProcessInfo *pinfo;

    token = (long)xpc_dictionary_get_uint64(reply, kAssertionCheckTokenKey);
    blockers = (uint32_t)xpc_dictionary_get_uint64(reply, kAssertionCheckCountKey);

    if (token != gPendingAckToken) {
        ERROR_LOG("Unexpected assertion check response from pid %d. token: %ld expected: %ld\n", 
                  pid, token, gPendingAckToken);
        return;
    }


    pinfo = processInfoGet(pid);
    if (!pinfo) {
        ERROR_LOG("Process for pid %d not found. token: %ld\n", pid, token);
        return;
    }

    DEBUG_LOG("Received assertion check response from pid %d with assertion cnt %d\n", pid, blockers);

    CFSetRemoveValue(gPendingResponses, pinfo);
    processInfoRelease(pinfo->pid);

    long cnt = CFSetGetCount(gPendingResponses);
    if (cnt == 0) {
        sendSleepNotificationResponse((void *)gPendingAckToken, gSleepBlockers ? false : true);
        gPendingAckToken = 0;
        gSleepBlockers = 0;
    }
    else {
        gSleepBlockers += blockers;
        DEBUG_LOG("Still waiting for assertion check response from %ld procs\n", cnt);
    }
}


void checkForAsyncAssertions(void *acknowledgementToken)
{
    static dispatch_source_t timer = 0;
    ProcessInfo **procs = NULL;
    ProcessInfo *pinfo = NULL;


    if (gPendingResponses == NULL) {
        gPendingResponses = CFSetCreateMutable(0, 0, NULL);
        if (!gPendingResponses) {
            ERROR_LOG("Failed to create array for pending responses\n");
            sendSleepNotificationResponse(acknowledgementToken, true);
            goto exit;
        }
    }

    if ((gPendingAckToken != 0) || (CFSetGetCount(gPendingResponses) != 0)) {
        ERROR_LOG("Call to check async assertions while previous check is not complete\n");
        sendSleepNotificationResponse(acknowledgementToken, true);
        goto exit;
    }

    long cnt = CFDictionaryGetCount(gProcessDict);
    procs = malloc(cnt*(sizeof(ProcessInfo *)));
    if (!procs) {
        sendSleepNotificationResponse(acknowledgementToken, true);
        goto exit;
    }

    gPendingAckToken = (uintptr_t)acknowledgementToken;
    gSleepBlockers = 0;
    CFDictionaryGetKeysAndValues(gProcessDict, NULL, (const void **)procs);
    for (long j = 0; (j < cnt) && (procs[j] != NULL); j++) {
        pinfo = procs[j];
        if ((pinfo->remoteConnection == 0) || (pinfo->proc_exited == 1)) {
            continue;
        }

        processInfoRetain(pinfo->pid);
        CFSetAddValue(gPendingResponses, (const void*)pinfo);
        sendCheckAssertionsMsg(pinfo, ^(xpc_object_t reply) { processAssertionCheckResp(reply, pinfo->pid); });
    }

    if (CFSetGetCount(gPendingResponses) == 0) {
        // There are no processes with xpc connection
        sendSleepNotificationResponse(acknowledgementToken, true);
        gPendingAckToken = 0;
        gSleepBlockers = 0;
    }

    if (timer == 0) {

        timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());
        dispatch_source_set_event_handler(timer, ^{
            if (CFSetGetCount(gPendingResponses) == 0) {
                return;
            }
            CFSetApplyFunction(gPendingResponses, handleAssertionCheckTimeout, NULL);
            CFSetRemoveAllValues(gPendingResponses);

            sendSleepNotificationResponse(acknowledgementToken, true);
            gPendingAckToken = 0;
            gSleepBlockers = 0;

        });

        dispatch_source_set_cancel_handler(timer, ^{
            dispatch_release(timer);
            timer = NULL;
        });


        dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC),
                DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(timer);
    }


    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC),
            DISPATCH_TIME_FOREVER, 0);

exit:
    if (procs) {
        free(procs);
    }

    return;
}

#pragma mark -
#pragma mark MIG
/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 * MIG Handlers
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/
void updateAppSleepStates(ProcessInfo *pinfo, int *disableAppSleep, int *enableAppSleep)
{
    if (!pinfo) return;

    if ((disableAppSleep) && (pinfo->disableAS_pend == true)) {
        *disableAppSleep = 1;
        pinfo->disableAS_pend = false;
    }
    if ((enableAppSleep) && (pinfo->enableAS_pend == true)) {
        *enableAppSleep = 1;
        pinfo->enableAS_pend = false;
    }
}

kern_return_t _io_pm_assertion_create (
                                       mach_port_t         server __unused,
                                       audit_token_t       token,
                                       vm_offset_t         props,
                                       mach_msg_type_number_t  propsCnt,
                                       int                 *assertion_id,
                                       int                 *disableAppSleep,
                                       int                 *enTrIntensity,
                                       int                 *return_code)
{
    CFMutableDictionaryRef     newAssertionProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;
    ProcessInfo         *pinfo = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    

    *disableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        newAssertionProperties = (CFMutableDictionaryRef)
                                    CFPropertyListCreateWithData( 0, unfolder, 
                                                                  kCFPropertyListMutableContainersAndLeaves, 
                                                                  NULL, NULL);
        CFRelease(unfolder);
    }

    if (!newAssertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }


    if (!callerIsEntitledToAssertion(token, newAssertionProperties))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    // Check for privileges if the assertion requires it.
    if (propertiesDictRequiresRoot(newAssertionProperties)
        && ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID))))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    *return_code = doCreate(callerPID, newAssertionProperties, (IOPMAssertionID *)assertion_id, &pinfo, enTrIntensity);

    if ((*return_code == kIOReturnSuccess) && (pinfo != NULL) ) {
        updateAppSleepStates(pinfo, disableAppSleep, NULL);
    }

exit:
    if (newAssertionProperties) {
        CFRelease(newAssertionProperties);
    }

    vm_deallocate(mach_task_self(), props, propsCnt);

    return KERN_SUCCESS;
}


/*****************************************************************************/

kern_return_t _io_pm_assertion_set_properties (
                                               mach_port_t         server __unused,
                                               audit_token_t       token,
                                               int                 assertion_id,
                                               vm_offset_t         props,
                                               mach_msg_type_number_t propsCnt,
                                               int                 *disableAppSleep,
                                               int                 *enableAppSleep,
                                               int                 *enTrIntensity,
                                               int                 *return_code) 
{
    CFDictionaryRef     setProperties = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    *disableAppSleep = 0;
    *enableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        setProperties = (CFDictionaryRef)CFPropertyListCreateWithData(0, unfolder, 0, NULL, NULL);
        CFRelease(unfolder);
    }

    if (!setProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }
    if (!callerIsEntitledToAssertion(token, setProperties))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    *return_code = doSetProperties(callerPID, assertion_id, setProperties, enTrIntensity);
    if (*return_code == kIOReturnSuccess) {
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, enableAppSleep);
    }


exit:
    if (setProperties) {
        CFRelease(setProperties);
    }
    vm_deallocate(mach_task_self(), props, propsCnt);

    return KERN_SUCCESS;

}

/*****************************************************************************/
kern_return_t _io_pm_assertion_retain_release (
                                               mach_port_t         server __unused,
                                               audit_token_t       token,
                                               int                 assertion_id,
                                               int                 action,
                                               int                 *retainCnt,
                                               int                 *disableAppSleep,
                                               int                 *enableAppSleep,
                                               int                 *return_code) 
{
    pid_t               callerPID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    *disableAppSleep = 0;
    *enableAppSleep = 0;
    if (kIOPMAssertionMIGDoRetain == action) {
        *return_code = doRetain(callerPID, assertion_id, retainCnt);
    } else {
        *return_code = doRelease(callerPID, assertion_id, retainCnt);
    }
    if (*return_code == kIOReturnSuccess) {
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, enableAppSleep);
    }
    return KERN_SUCCESS;
}

/*****************************************************************************/
kern_return_t _io_pm_assertion_copy_details (
                                             mach_port_t         server,
                                             audit_token_t       token,
                                             int                 assertion_id,
                                             int                 whichData,
                                             vm_offset_t         props,
                                             mach_msg_type_number_t propsCnt,
                                             vm_offset_t         *assertions,
                                             mach_msg_type_number_t  *assertionsCnt,
                                             int                 *return_val) 
{
    CFTypeRef           theCollection = NULL;
    CFDataRef           serializedDetails = NULL;
    pid_t               callerPID = -1;


    *return_val = kIOReturnNotFound;

    if (kIOPMAssertionMIGCopyAll == whichData)
    {
        theCollection = copyPIDAssertionDictionaryFlattened(kIOPMActiveAssertions);

    } else if (kIOPMAssertionMIGCopyInactive == whichData)
    {

        theCollection = copyPIDAssertionDictionaryFlattened(kIOPMInactiveAssertions);

    } else if (kIOPMAssertionMIGCopyOneAssertionProperties == whichData) 
    {
        audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

        *return_val = copyAssertionForID(callerPID, assertion_id,  
                                         (CFMutableDictionaryRef *)&theCollection);

    } else if (kIOPMAssertionMIGCopyStatus == whichData)
    {
        theCollection = copyAggregateValuesDictionary();

    } else if (kIOPMPowerEventsMIGCopyScheduledEvents == whichData)
    {
        theCollection = copyScheduledPowerEvents();
    }
    else if (kIOPMPowerEventsMIGCopyRepeatEvents == whichData)
    {
        theCollection = copyRepeatPowerEvents();
    }
    else if (kIOPMAssertionMIGCopyByType == whichData)
    {
        CFStringRef  assertionType = NULL;

        CFDataRef unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
        if (unfolder) {
            assertionType = (CFStringRef)CFPropertyListCreateWithData(0, unfolder, 0, NULL, NULL);
            CFRelease(unfolder);
        }
        theCollection = copyAssertionsByType(assertionType);

        if (assertionType) {
            CFRelease(assertionType);
        }
    }

    if (!theCollection) {
        *assertionsCnt = 0;
        *assertions = 0;
        *return_val = kIOReturnSuccess;
        goto exit;
    }


    serializedDetails = CFPropertyListCreateData(0, theCollection, 
                                                 kCFPropertyListBinaryFormat_v1_0, 0, NULL);            

    CFRelease(theCollection);        

    if (serializedDetails) 
    {
        *assertionsCnt = (mach_msg_type_number_t)CFDataGetLength(serializedDetails);

        vm_allocate(mach_task_self(), (vm_address_t *)assertions, *assertionsCnt, TRUE);

        memcpy((void *)*assertions, CFDataGetBytePtr(serializedDetails), *assertionsCnt);

        CFRelease(serializedDetails);

        *return_val = kIOReturnSuccess;
    } else {
        *return_val = kIOReturnInternalError;
    }

exit:

    if (props && propsCnt)
    {
        vm_deallocate(mach_task_self(), props, propsCnt);
    }
    return KERN_SUCCESS;
}

kern_return_t _io_pm_ctl_assertion_type (
                                         mach_port_t         server,
                                         audit_token_t       token,
                                         string_t            type, 
                                         int                 op,
                                         int                 *return_code)
{
    CFStringRef typeRef = NULL;
    uid_t       callerEUID;
    int         idx;

    *return_code = kIOReturnError;
    audit_token_to_au32(token, NULL, &callerEUID, NULL, NULL, NULL, NULL, NULL, NULL);
    if (callerEUID != 0) {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (type && strlen(type)) {
        typeRef = CFStringCreateWithCString(0, type, kCFStringEncodingUTF8);
    }
    if (!isA_CFString(typeRef))
        goto exit;

    if ( (idx = getAssertionTypeIndex(typeRef)) < 0 ) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    *return_code = kIOReturnSuccess;    
    if (op == kIOPMDisableAssertionType) {
        disableAssertionType(idx);
    }
    else if (op == kIOPMEnableAssertionType) {
        enableAssertionType(idx);
    }
    else
        *return_code = kIOReturnBadArgument;
exit:
    if (typeRef) CFRelease(typeRef);

    return KERN_SUCCESS;

}

// Changes clamshell sleep state
static void setClamshellSleepState( )
{
    io_connect_t        connect = IO_OBJECT_NULL;
    uint64_t            in;
    static int          prevState = -1;
    int                 newState = 0;
    __block int         lidSleepCount = 0;

    // Check lid sleep preventers on kDeclareUserActivityType and kTicklessDisplayWakeType
    applyToAssertionsSync(&gAssertionTypes[kDeclareUserActivityType], kSelectActive,
                          ^(assertion_t *assertion) {
                              if (assertion->state & kAssertionLidStateModifier) {
                                  lidSleepCount++;
                              }
                          });
    applyToAssertionsSync(&gAssertionTypes[kTicklessDisplayWakeType], kSelectActive,
                          ^(assertion_t *assertion) {
                              if (assertion->state & kAssertionLidStateModifier) {
                                  lidSleepCount++;
                              }
                          });



    if (lidSleepCount || prevState) {
        DEBUG_LOG("lidClose sleep preventers: %d prevState: %d\n", lidSleepCount, prevState);
    }
    if (lidSleepCount && (prevState < 1)) {
        newState = 1;
    }
    else if (lidSleepCount == 0) {
        if (prevState == 0) {
            return;
        }
        newState = 0;
    }
    else {
        return;
    }

    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL) {
        ERROR_LOG("Failed to open connection to RootDomain\n");
        return;
    }

    in = newState ? 1 : 0;
    INFO_LOG("Setting ClamshellSleepState to %lld\n", in);

    IOConnectCallMethod(connect, kPMSetClamshellSleepState, 
                        &in, 1, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);
    prevState = newState;
    return;
}


__private_extern__ void sendActivityTickle ()
{
    io_connect_t                connect = IO_OBJECT_NULL;
    kern_return_t               rc;
    static uint64_t             lastTickle_ts = 0;
    uint64_t                    currTime = getMonotonicTime();

    SystemLoadUserActiveAssertions(true);

    if (((currTime - lastTickle_ts) < kDisplayTickleDelay) &&
            (!isDisplayAsleep()) && (userActiveRootDomain())){
        DEBUG_LOG("Avoiding display tickle. cTime:%lld lTime:%lld Display:%d rd:%d\n",
                currTime, lastTickle_ts, isDisplayAsleep(), userActiveRootDomain());
        return;
    }


    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL) {
        ERROR_LOG("Failed to open connection to rootDomain\n");
        return;
    }

    rc = IOConnectCallMethod(connect, kPMActivityTickle,
                        NULL, 0, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);
    if (rc != kIOReturnSuccess) {
        ERROR_LOG("Activity tickle failed with error 0x%x\n", rc);
    }
    else {
        DEBUG_LOG("Activity tickle\n");
        lastTickle_ts = currTime;
    }

    return;

}
static void sendUserAssertionsToKernel(uint32_t user_assertions)
{
    io_connect_t                connect = IO_OBJECT_NULL;
    const uint64_t              in = (uint64_t)user_assertions;

    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL)
        return;

    IOConnectCallMethod(connect, kPMSetUserAssertionLevels, 
                        &in, 1, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);

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

    configAssertionType(kBackgroundTaskType, false);
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
                                              (which == kSBUCChargeInhibit) ? 
                                                    CFSTR(kIOPMPSIsChargingKey) : CFSTR(kIOPMPSExternalConnectedKey), 
                                              level ? kCFBooleanFalse : kCFBooleanTrue);
            IOObjectRelease(next);
        }
        IOObjectRelease(iter);
    }
    while (false);
    return;
}

#endif /* HAVE_SMART_BATTERY */

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

        if (assertionDescription)
        {
            /* This assertion should be applied even on battery power */
            CFDictionarySetValue(assertionDescription, 
                                 kIOPMAssertionAppliesToLimitedPowerKey, (CFBooleanRef)kCFBooleanTrue);
            InternalCreateAssertion(assertionDescription, NULL);

            CFRelease(assertionDescription);
        }
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
        int tmp_bit = getAssertionLevel(i);

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

__private_extern__ void _PMAssertionsDriverAssertionsHaveChanged(uint32_t changedDriverAssertions)
{
    if (gAggChange)
        notify_post( kIOPMAssertionsChangedNotifyString );
}


#define     kDarkWakeNetworkHoldForSeconds          30
#define     kDeviceEnumerationHoldForSeconds        (45LL)

#pragma mark -
#pragma mark powerd-Internal Use Only


__private_extern__ CFMutableDictionaryRef _IOPMAssertionDescriptionCreate(
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

    dispatch_async(_getPMMainQueue(), ^{
        if (outID == NULL) {
            /* Some don't care for assertionId */
            IOPMAssertionID   assertionID = kIOPMNullAssertionID;
            doCreate(getpid(), properties, &assertionID, NULL, NULL);
        }
        else if ( *outID == kIOPMNullAssertionID )
            doCreate(getpid(), properties, outID, NULL, NULL);

        CFRelease(properties);
    });

    return kIOReturnSuccess;
}

__private_extern__ void InternalReleaseAssertion(
                                                 IOPMAssertionID *outID)
{
    dispatch_async(_getPMMainQueue(), ^{
        if ( *outID != kIOPMNullAssertionID ) {
            doRelease(getpid(), *outID, NULL);
        }
        *outID = kIOPMNullAssertionID;
    });
}

__private_extern__ void InternalEvaluateAssertions(void)
{
    dispatch_async(_getPMMainQueue(), ^{
        evaluateForPSChange();
    });
}

__private_extern__ IOReturn 
InternalCreateAssertionWithTimeout(CFStringRef type, CFStringRef name, int timerSecs, IOPMAssertionID *outID)
{
    CFMutableDictionaryRef          dict = NULL;

    if (!type || !name || !outID) {
        return kIOReturnBadArgument;
    }

    if ( (dict = _IOPMAssertionDescriptionCreate(type, name, NULL, NULL, NULL,
                                                 (CFTimeInterval)timerSecs, kIOPMAssertionTimeoutActionRelease)) )
    {
        doCreate(getpid(), dict, outID, NULL, NULL);
        CFRelease(dict);
    }

    return kIOReturnSuccess;

}
__private_extern__ IOReturn InternalReleaseAssertionSync(IOPMAssertionID outID)
{
    IOReturn ret = kIOReturnError;

    if (outID != kIOPMNullAssertionID) {
        ret = doRelease(getpid(), outID, NULL);
    }

    return ret;
}

__private_extern__ IOReturn InternalSetAssertionTimeout(IOPMAssertionID id, CFTimeInterval timeout)
{
    CFMutableDictionaryRef          dict = NULL;
    IOReturn                        rc = kIOReturnError;

    if (!id)
        return kIOReturnBadArgument;

    CFNumberRef timeout_num = CFNumberCreate(0, kCFNumberDoubleType, &timeout);
    if (!timeout_num) {
        return kIOReturnError;
    }

    if ((dict = CFDictionaryCreateMutable(0, 0,
                                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
    {
        CFDictionarySetValue(dict, kIOPMAssertionTimeoutKey, timeout_num);
        rc = doSetProperties(getpid(), id, dict, NULL);
        CFRelease(dict);
    }
    CFRelease(timeout_num);

    return rc;

}

static IOReturn _localCreateAssertion(CFStringRef type, CFStringRef name, IOPMAssertionID *outID)
{
    return InternalCreateAssertionWithTimeout(type, name, 0, outID);
}

STATIC IOReturn _enableAssertionForLimitedPower(pid_t pid, IOPMAssertionID id)
{
    CFMutableDictionaryRef          dict = NULL;
    IOReturn                        rc = kIOReturnError;

    if (!id)
        return kIOReturnBadArgument;

    if ((dict = CFDictionaryCreateMutable(0, 0, 
                                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
    {
        CFDictionarySetValue(dict, kIOPMAssertionAppliesToLimitedPowerKey, kCFBooleanTrue);

        rc = doSetProperties(pid, id, dict, NULL);

        CFRelease(dict);
    }

    return rc;
}

/* _DarkWakeHandleNetworkWake runs when PM enters dark wake via network packet.
 * It creates a temporary assertion that keeps the system awake hopefully long enough
 * for a remote client to connect, and for the server to create an assertion
 * keeping the system awak.
 */
static void  _DarkWakeHandleNetworkWake(void)
{
    IOReturn rc;
    IOPMAssertionID gDarkWakeNetworkAssertion = kIOPMNullAssertionID;

    rc = InternalCreateAssertionWithTimeout(kIOPMAssertInternalPreventSleep, 
                                        CFSTR("Network wake delay proxy assertion"),
                                        30, &gDarkWakeNetworkAssertion);

    if (rc != kIOReturnSuccess)
        return;

    /* Enable this assertion on battery power also */
    _enableAssertionForLimitedPower(getpid(), gDarkWakeNetworkAssertion);

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
    if (obj)
        IOObjectRelease(obj);

    if (notifyInfo)
    {
        if (notifyInfo->handle)
            IOObjectRelease(notifyInfo->handle);
        if (notifyInfo->port)
            IONotificationPortDestroy(notifyInfo->port);
        free(notifyInfo);
    }

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
        doRelease(getpid(), deInfo->assertId, NULL);
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
                                              0, _getPMMainQueue());

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


static void _AssertForDeviceEnumeration(CFStringRef wakeType)
{
    IOPMAssertionID     deviceEnumerationAssertion = kIOPMNullAssertionID;
    notifyRegInfo_st    *notifyInfo = NULL;
    devEnumInfo_st *deInfo = NULL;
    IOReturn rc;
    dispatch_source_t dispSrc;
    
    if (isA_CFString(wakeType) && (
        CFEqual(wakeType, kIOPMRootDomainWakeTypeAlarm) ||
        CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepTimer) ||
        CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService) ||
        CFEqual(wakeType, kIOPMrootDomainWakeTypeLowBattery) ||
        CFEqual(wakeType, kIOPMRootDomainWakeTypeNotification)
       )) {

        /* No need to take proxy for device enumeration */
        return;
    }

    rc = _localCreateAssertion(kIOPMAssertInternalPreventSleep, 
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
                                     0, _getPMMainQueue());
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


    IONotificationPortSetDispatchQueue(notifyInfo->port, _getPMMainQueue());

    return;

exit:
    if (deInfo) 
        free(deInfo);

    if (notifyInfo)
        _DeregisterForNotification(notifyInfo);

    doRelease(getpid(), deviceEnumerationAssertion, NULL); 
}

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


    if ( IOPMIsADarkWake(capArgs->toCapabilities) &&
         IOPMIsASleep(capArgs->fromCapabilities) )
    {
        wakeType = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeTypeKey));
        wakeReason = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeReasonKey));

        if (isA_CFString(wakeReason) && CFEqual(wakeReason, kIORootDomainWakeReasonDarkPME))
        {
            if (isA_CFString(wakeType) && CFEqual(wakeType, kIOPMRootDomainWakeTypeNetwork))
                _DarkWakeHandleNetworkWake( );
            else
                _AssertForDeviceEnumeration(wakeType);
        }
        else if (isA_CFString(wakeType))
        {
            if (CFEqual(wakeType, kIOPMRootDomainWakeTypeNetwork))
                _DarkWakeHandleNetworkWake( );
            else if (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) &&
                     CFEqual(wakeReason, kIOPMRootDomainWakeReasonRTC))
            {
                InternalCreateAssertionWithTimeout(kIOPMAssertionTypeBackgroundTask, 
                                               CFSTR("Powerd - Wait for client BackgroundTask assertions"), 10, &pushSvcAssert);
            }
            else {
                _AssertForDeviceEnumeration(wakeType);
            }
        }
        else 
        {
            /* 
             * Wake type is not set for some cases. Only wake reason is set.
             * Plugging a new USB device while sleeping is one such case
             */
            _AssertForDeviceEnumeration(wakeType);
        }

        if (wakeType) {
            CFRelease(wakeType);
        }
        if (wakeReason)
            CFRelease(wakeReason);
    }

}

/*
 * Takes an assertion to keep display on for up to
 * a max of 'kPMMaxDisplayTurnOffDelay' minutes
 */
void delayDisplayTurnOff( )
{

    CFNumberRef  levelNum = NULL;
    CFNumberRef  Timeout_num = NULL;
    CFTimeInterval   delay = 0;

    IOPMAssertionLevel        level = kIOPMAssertionLevelOn;
    static IOPMAssertionID    id = kIOPMNullAssertionID; 
    CFMutableDictionaryRef    dict = NULL;

    if (gDisplaySleepTimer)
        delay = gDisplaySleepTimer > kPMMaxDisplayTurnOffDelay ?
            kPMMaxDisplayTurnOffDelay : gDisplaySleepTimer;
    else 
        delay = kPMMaxDisplayTurnOffDelay;

    delay *= 60;

    if (id == kIOPMNullAssertionID) 
    {
        dict = _IOPMAssertionDescriptionCreate(
                                               kIOPMAssertInternalPreventDisplaySleep,
                                               CFSTR("com.apple.powermanagement.delayDisplayOff"),
                                               NULL, CFSTR("Proxy to delay display off"), NULL, 
                                               (CFTimeInterval)delay, kIOPMAssertionTimeoutActionTurnOff);

        if (dict) {
            doCreate(getpid(), dict, &id, NULL, NULL);
            CFRelease(dict);
        }
    }
    else 
    {
        dict = CFDictionaryCreateMutable(0, 0, 
                                         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        levelNum = CFNumberCreate(0, kCFNumberIntType, &level);
        Timeout_num = CFNumberCreate(0, kCFNumberDoubleType, &delay);

        if (dict && levelNum && Timeout_num)
        {
            CFDictionarySetValue(dict, kIOPMAssertionLevelKey, levelNum);
            CFDictionarySetValue(dict, kIOPMAssertionTimeoutKey, Timeout_num);
            doSetProperties(getpid(), id, dict, NULL);
        }
        if (dict) CFRelease(dict);
        if (levelNum) CFRelease(levelNum);
        if (Timeout_num) CFRelease(Timeout_num);
    }


}

STATIC bool propertiesDictRequiresRoot(CFDictionaryRef props)
{
    if ( CFDictionaryGetValue(props, kIOPMInflowDisableAssertion)
         || CFDictionaryGetValue(props, kIOPMChargeInhibitAssertion) )
    {
        return true;
    } else {
        return false;
    }
}



static bool callerIsEntitledToAssertion(
                                        audit_token_t token,
                                        CFDictionaryRef newAssertionProperties)
{
    CFStringRef         assert_type = NULL;
    bool                caller_is_allowed = true;
    int                 idx = -1;
    CFTypeRef           value = NULL;
    pid_t               callerPID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    // Check entitlement for assertion type
    assert_type = CFDictionaryGetValue(newAssertionProperties, kIOPMAssertionTypeKey);
    idx = getAssertionTypeIndex(assert_type);
    if ((idx >= 0) && (gAssertionTypes[idx].entitlement != NULL))  {
        caller_is_allowed = auditTokenHasEntitlement(token, gAssertionTypes[idx].entitlement);

        if (!caller_is_allowed) {
            ERROR_LOG("Pid %d is not privileged to create assertion type %@\n", callerPID, assert_type);
            return false;
        }
    }

    // Check entitlement for assertion properties
    value = CFDictionaryGetValue(newAssertionProperties, kIOPMAssertionAppliesToLimitedPowerKey);
    if (isA_CFBoolean(value) && (value == kCFBooleanTrue)) {
        caller_is_allowed = auditTokenHasEntitlement(token, kIOPMAssertOnBatteryEntitlement);

        if (!caller_is_allowed) {
            ERROR_LOG("Pid %d is not privileged to set property %@\n", callerPID, kIOPMAssertionAppliesToLimitedPowerKey);
            return false;
        }
    }

    value = CFDictionaryGetValue(newAssertionProperties, kIOPMAssertionAppliesOnLidClose);
    if (isA_CFBoolean(value) && (value == kCFBooleanTrue)) {
        caller_is_allowed = auditTokenHasEntitlement(token, kIOPMAssertOnLidCloseEntitlement);

        if (!caller_is_allowed) {
            ERROR_LOG("Pid %d is not privileged to set property %@\n", callerPID, kIOPMAssertionAppliesOnLidClose);
            return false;
        }
    }
    return caller_is_allowed;
}

static void checkProcAggregates( )
{
    CFDictionaryRef update = NULL;
    IOReportSampleRef   delta = NULL;

    if (kBatteryPowered != _getPowerSource()) {
        // Nothing to do when device is on external power source
        return;
    }
    if (gProcAggregateBasis == NULL) {
        copyAssertionActivityAggregate(&gProcAggregateBasis);
        return;
    }

    copyAssertionActivityAggregate(&update);
    if (!update) {
        goto exit;
    }

    delta = IOReportCreateSamplesDelta(gProcAggregateBasis, update, NULL);
    if (!delta) {
        goto exit;
    }

    IOReportIterate(delta, ^(IOReportChannelRef ch) {
            int64_t     eff1, eff2, eff3;
            uint64_t pid;
            ProcessInfo *pinfo = NULL;

            pid = IOReportChannelGetChannelID(ch);
            eff1 = IOReportArrayGetValueAtIndex(ch, 1); // Idle Sleep
            eff2 = IOReportArrayGetValueAtIndex(ch, 2); // Demand Sleep
            eff3 = IOReportArrayGetValueAtIndex(ch, 3); // Display Sleep


            pinfo = processInfoGet((pid_t)pid);
            if (!pinfo) {
                return kIOReportIterOk;
            }

            if (pinfo->aggAssertLength == 0) {
                return kIOReportIterOk;
            }
            if (
                (eff1 >= pinfo->aggAssertLength)
                || (eff2 >= pinfo->aggAssertLength) || (eff3 >= pinfo->aggAssertLength)
                ) {

                int token;
                uint32_t  status = notify_register_check(kIOPMAssertionExceptionNotifyName, &token);
                if (status == NOTIFY_STATUS_OK) {
                    notify_set_state(token, (((uint64_t)kIOPMAssertionAggregateException << 32)) | pid);
                    notify_post(kIOPMAssertionExceptionNotifyName);
                    notify_cancel(token);
                    INFO_LOG("Aggregate assertion exception on pid %llu.\n", pid);
                }

           }

            return kIOReportIterOk;
    });
exit:
    if (gProcAggregateBasis) {
        CFRelease(gProcAggregateBasis);
    }
    gProcAggregateBasis = update;

    if (delta) {
        CFRelease(delta);
    }

}

static void setProcessAssertionLimits(ProcessInfo *pinfo)
{
    CFDictionaryRef defaultLimits, procLimit;
    CFNumberRef cf_defaultAssertLimit, cf_defaultAggLimit;
    CFNumberRef cf_assertionLimit, cf_aggAssertionLimit;
    uint32_t defaultAssertLimit, defaultAggLimit;

    if (!isA_CFDictionary(gProcAssertionLimits) || !isA_CFString(pinfo->name)) {
        return;
    }

    defaultAssertLimit = 0;
    defaultAggLimit = 0;

    defaultLimits = CFDictionaryGetValue(gProcAssertionLimits, kIOPMDefaultLimtsKey);
    if (isA_CFDictionary(defaultLimits)) {
        cf_defaultAssertLimit = CFDictionaryGetValue(defaultLimits, kIOPMAssertionDurationLimit);
        cf_defaultAggLimit = CFDictionaryGetValue(defaultLimits, kIOPMAggregateAssertionLimit);
        if (isA_CFNumber(cf_defaultAssertLimit)) {
            CFNumberGetValue(cf_defaultAssertLimit, kCFNumberIntType, &defaultAssertLimit);
        }
        if (isA_CFNumber(cf_defaultAggLimit)) {
            CFNumberGetValue(cf_defaultAggLimit, kCFNumberIntType, &defaultAggLimit);
        }
    }

    procLimit = CFDictionaryGetValue(gProcAssertionLimits, pinfo->name);
    if (isA_CFDictionary(procLimit)) {
        cf_assertionLimit = CFDictionaryGetValue(procLimit, kIOPMAssertionDurationLimit);
        cf_aggAssertionLimit = CFDictionaryGetValue(procLimit, kIOPMAggregateAssertionLimit);

        if (isA_CFNumber(cf_assertionLimit)) {
            CFNumberGetValue(cf_assertionLimit, kCFNumberIntType, &pinfo->maxAssertLength);
        }

        if (isA_CFNumber(cf_aggAssertionLimit)) {
            CFNumberGetValue(cf_aggAssertionLimit, kCFNumberIntType, &pinfo->aggAssertLength);
        }
    }
    else {
        // Set to default values. This will also over-write any previous values
        pinfo->maxAssertLength = defaultAssertLimit;
        pinfo->aggAssertLength = defaultAggLimit;
    }
}
#ifdef XCTEST
ProcessInfo* processInfoCreateForTest(pid_t p, CFStringRef name)
{
    
    
    ProcessInfo             *proc =  processInfoCreate(p);
    proc->name = name;
    return proc;
}
#endif

static ProcessInfo* processInfoCreate(pid_t p)
{
    ProcessInfo             *proc = NULL;
    char                    name[kProcNameBufLen];
    static  uint32_t        create_seq = 0;
    int64_t                 offset;

#ifndef XCTEST
    if (proc_name(p, name, sizeof(name)) == 0) {
        ERROR_LOG("processInfoCreate: proc_name look-up unsuccessful for pid %d\n", p);
        return NULL;
    }
#else
    name[0]='\0';
#endif
    proc = calloc(1, sizeof(ProcessInfo));
    if (!proc) {
        ERROR_LOG("processInfoCreate: calloc failed to allocate ProcessInfo struct for pid %d\n", p);
        return NULL;
    }


    if (p != 0) {
        proc->disp_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, p, 
                                                DISPATCH_PROC_EXIT, _getPMMainQueue());
        if (proc->disp_src == NULL) {
            free(proc);
            ERROR_LOG("processInfoCreate: dispatch_source_create failed for pid %d\n", p);
            return NULL;
        }

        offset = 60;
#ifndef XCTEST
        dispatch_source_set_event_handler(proc->disp_src, ^{
                                          HandleProcessExit(p);
                                          // 21904354, clean up any assertions they may have been
                                          // created after receiving the PROC_EXIT notification.
                                          dispatch_after(dispatch_time(DISPATCH_TIME_NOW, offset * NSEC_PER_SEC),
                                                         _getPMMainQueue(),
                                                         ^{ HandleProcessExit(p);
                                                            // On OSX, release the stats buf 60secs later. Any stats
                                                            // queries within 60secs will get the dead pid's stats also.
                                                            // On iOS, this is released after powerlog queries the stats.
                                                            releaseStatsBufByPid(p);
                                                            });
                                          });

        dispatch_source_set_cancel_handler(proc->disp_src, ^{
                                           dispatch_release(proc->disp_src);
                                           });

        dispatch_resume(proc->disp_src);
#endif
    }
    proc->name = CFStringCreateWithCString(0, name, kCFStringEncodingUTF8);
    if (!isA_CFString(proc->name)) {
        ERROR_LOG("Failed to create cfstring for pid %d name: %s\n", p, name);
    }
    proc->pid = p;
    proc->retain_cnt++;
    proc->create_seq = create_seq++;

    CFDictionarySetValue(gProcessDict, (const void *)(uintptr_t)p, (const void *)proc);

    setProcessAssertionLimits(proc);

    return proc;
}

/* Wrap a pointer to a non-CF object inside a CFData, so that 
 * we can place it an a CF container.
 */

static ProcessInfo* processInfoRetain(pid_t p)
{    
    ProcessInfo       *proc = NULL;
    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (const void *)(uintptr_t)p);

    if (proc) {
        if (proc->retain_cnt != UINT_MAX) proc->retain_cnt++;
        return proc;
    }

    return NULL;

}
static ProcessInfo* processInfoGet(pid_t p)
{    
    ProcessInfo       *proc = NULL;
    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (const void *)(uintptr_t)p);

    if (proc) {
        return proc;
    }

    return NULL;
}

CFStringRef processInfoGetName(pid_t p)
{
    ProcessInfo         *proc = NULL;
    CFStringRef         retString = NULL;
    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (const void *)(uintptr_t)p);

    if (proc) {
        retString = proc->name;
    }

    return retString;
}

STATIC void processInfoRelease(pid_t p)
{
    ProcessInfo   *proc = NULL;

    proc = (ProcessInfo *)CFDictionaryGetValue(gProcessDict, (const void *)(uintptr_t)p);

    if (!proc) return;


#ifndef XCTEST
    if (proc->retain_cnt == 1) {
        if (proc->disp_src) {
            dispatch_release(proc->disp_src);
        }
        if (proc->name) CFRelease(proc->name);
        if (proc->assertionExceptionAggdKey) CFRelease(proc->assertionExceptionAggdKey);
        if (proc->aggregateExceptionAggdKey) CFRelease(proc->aggregateExceptionAggdKey);

        CFDictionaryRemoveValue(gProcessDict, (const void *)(uintptr_t)p);
        memset(proc, 0, sizeof(*proc));
        free(proc);
    }
    else {
        proc->retain_cnt--;
    }
#endif

    return ;
}

void handleProcAssertionTimeout(pid_t pid, IOPMAssertionID id)
{
    uint32_t status;
    int token;
    assertion_t *assertion = NULL;

    ProcessInfo *pinfo = processInfoGet(pid);
    if (!pinfo || pinfo->proc_exited) {
        // Make sure the process still exists
        return;
    }

    lookupAssertion(pid, id, &assertion);
    if (!assertion) {
        return;
    }
    status = notify_register_check(kIOPMAssertionExceptionNotifyName, &token);
    if (status == NOTIFY_STATUS_OK) {
        notify_set_state(token, (((uint64_t)kIOPMAssertionDurationException << 32)) | pid);
        notify_post(kIOPMAssertionExceptionNotifyName);
        notify_cancel(token);
        INFO_LOG("Single assertion exception on pid %d. Assertion details: %@\n", pid, assertion->props);
    }


}


void updateSystemQualifiers(assertion_t *assertion, assertionOps op)
{
    int audioExists = (gSysQualifier.audioin+gSysQualifier.audioout) ? 1 : 0;
    int newAudioExists;

    void (^create)(void) = ^{
        CFBooleanRef value;
        CFArrayRef resources = CFDictionaryGetValue(assertion->props, kIOPMAssertionResourcesUsed);

        if (CFDictionaryGetValueIfPresent(assertion->props, kIOPMAssertionAllowsDeviceRestart, (const void **)&value)
            && (value == kCFBooleanTrue)) {
            assertion->allowsDeviceRestart = 1;
        }

        if (isA_CFArray(resources)) {
            CFIndex count = CFArrayGetCount(resources);
            for(int i=0; i<count; i++) {
                CFStringRef name = CFArrayGetValueAtIndex(resources, i);
                if (isA_CFString(name)) {
                    if (CFEqual(name, kIOPMAssertionResourceAudioIn) && (assertion->allowsDeviceRestart == 0)) {
                        assertion->audioin = 1;
                        gSysQualifier.audioin++;
                    }
                    else if (CFEqual(name, kIOPMAssertionResourceAudioOut)) {
                        assertion->audioout = 1;
                        gSysQualifier.audioout++;
                    }
                    else if (CFEqual(name, kIOPMAssertionResourceGPS)) {
                        assertion->gps = 1;
                        gSysQualifier.gps++;
                    }
                    else if (CFEqual(name, kIOPMAssertionResourceBaseband)) {
                        assertion->baseband = 1;
                        gSysQualifier.baseband++;
                    }
                    else if (CFEqual(name, kIOPMAssertionResourceBluetooth)) {
                        assertion->bluetooth = 1;
                        gSysQualifier.bluetooth++;
                    }
                }
            }
        }



        if (CFDictionaryGetValueIfPresent(assertion->props, kIOPMAssertionActivityBudgeted, (const void **)&value)
            && (value == kCFBooleanTrue)) {
            assertion->budgetedActivity = 1;
            gSysQualifier.budgetedActivity++;
        }
    };

    void (^release)(void) = ^{
        if (assertion->audioin) {
            assertion->audioin = 0;
            if (gSysQualifier.audioin) {
                gSysQualifier.audioin--;
            }
        }
        if (assertion->audioout) {
            assertion->audioout = 0;
            if (gSysQualifier.audioout) {
                gSysQualifier.audioout--;
            }
        }
        if (assertion->gps) {
            assertion->gps = 0;
            if (gSysQualifier.gps) {
                gSysQualifier.gps--;
            }
        }
        if (assertion->baseband) {
            assertion->baseband = 0;
            if (gSysQualifier.baseband) {
                gSysQualifier.baseband--;
            }
        }
        if (assertion->bluetooth) {
            assertion->bluetooth = 0;
            if (gSysQualifier.bluetooth) {
                gSysQualifier.bluetooth--;
            }
        }
        if (assertion->allowsDeviceRestart) {
            assertion->allowsDeviceRestart = 0;
        }
        if (assertion->budgetedActivity) {
            assertion->budgetedActivity = 0;
            if (gSysQualifier.budgetedActivity) {
                gSysQualifier.budgetedActivity--;
            }
        }

    };
    if (op == kAssertionOpRaise) {
        create();
    }
    else if (op == kAssertionOpRelease){
        release();
    }
    else if (op == kAssertionOpEval) {
        release();
        create();
    }

    newAudioExists = (gSysQualifier.audioin+gSysQualifier.audioout) ? 1 : 0;
    if (audioExists ^ newAudioExists) {
        dispatch_async(_getPMMainQueue(), ^{ userActiveHandlePowerAssertionsChanged();});
    }
}

void stopProcTimer(assertion_t *assertion)
{

    if (assertion->procTimer && (assertion->state & kAssertionProcTimerActive)) {
        dispatch_suspend(assertion->procTimer);
        assertion->state &= ~kAssertionProcTimerActive;
    }
}


void startProcTimer(assertion_t *assertion)
{
    assertionType_t     *assertType = NULL;
    ProcessInfo *pinfo = NULL;
    dispatch_source_t disp;

    assertType = &gAssertionTypes[assertion->kassert];
    if (assertType->effectIdx == kNoEffect) {
        return;
    }

    // Account the timers based on the pid creating the assertions
    pinfo = assertion->pinfo;
    if (!pinfo || pinfo->maxAssertLength == 0) {
        return;
    }

    if (_getPowerSource() != kBatteryPowered) {
        // Time based assertion validation is done only on battery power
        return;
    }
    if (assertion->timeout) {

        uint64_t currTime = getMonotonicTime();
        uint64_t deltaSecs = assertion->timeout - currTime;
        if (deltaSecs <= pinfo->maxAssertLength) {
            // If assertion timeout is smaller than proc's maxAssertionLength, no need to activate the timer
            return;
        }
    }

    stopProcTimer(assertion);

    if (assertion->procTimer == 0) {
        // Saving the dispatch source to local variable 'disp', to make sure the value is
        // copied into the cancel_handler block. That block gets executed after 'assertion'
        // is released.
        disp = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());
        dispatch_source_set_event_handler(disp, ^{ handleProcAssertionTimeout(pinfo->pid, assertion->assertionId);});
        dispatch_source_set_cancel_handler(disp, ^{ dispatch_release(disp);});
        assertion->procTimer = disp;
        dispatch_source_set_timer(assertion->procTimer, dispatch_time(DISPATCH_TIME_NOW, pinfo->maxAssertLength * NSEC_PER_SEC),
                              DISPATCH_TIME_FOREVER, 0);
    }
    dispatch_resume(assertion->procTimer);
    assertion->state |= kAssertionProcTimerActive;

}


static void disableAppSleep(ProcessInfo *pinfo)
{
    char notify_str[128];

    if (pinfo->disableAS_pend == false)
        return;

    pinfo->disableAS_pend = false;
    snprintf(notify_str, sizeof(notify_str), "%s.%d", 
             kIOPMDisableAppSleepPrefix,pinfo->pid);
    notify_post(notify_str);

}

static void enableAppSleep(ProcessInfo *pinfo)
{
    char notify_str[128];

    if (pinfo->enableAS_pend == false)
        return;

    pinfo->enableAS_pend = false;
    snprintf(notify_str, sizeof(notify_str), "%s.%d", 
             kIOPMEnableAppSleepPrefix, pinfo->pid);
    notify_post(notify_str);
}


void schedDisableAppSleep(assertion_t *assertion)
{
    assertionType_t     *assertType = NULL;
    ProcessInfo         *pinfo = NULL;
    uint32_t            agg;

    assertType = &gAssertionTypes[assertion->kassert]; 

    if ( !(assertType->flags & kAssertionTypePreventAppSleep)) return;

    pinfo = assertion->pinfo;
    if (pinfo->pid == getpid()) return;


    if (pinfo->assert_cnt[assertion->kassert] == UCHAR_MAX) return;
    pinfo->assert_cnt[assertion->kassert]++;

    agg = pinfo->aggTypes;
    pinfo->aggTypes |= ( 1 << assertion->kassert );
    if (agg == 0) {
        processInfoRetain(pinfo->pid);
        pinfo->disableAS_pend = true;
        dispatch_async(_getPMMainQueue(), ^{
            disableAppSleep(pinfo);
            processInfoRelease(pinfo->pid);
        });
    }
}


void schedEnableAppSleep(assertion_t *assertion)
{
    assertionType_t     *assertType = NULL;
    ProcessInfo         *pinfo = NULL;

    assertType = &gAssertionTypes[assertion->kassert]; 
    if ( !(assertType->flags & kAssertionTypePreventAppSleep)) return;

    pinfo = assertion->pinfo;
    if (pinfo->pid == getpid()) return;


    if (--pinfo->assert_cnt[assertion->kassert] == 0) 
    {
        pinfo->aggTypes &= ~( 1 << assertion->kassert );

        if (pinfo->aggTypes == 0) {
            processInfoRetain(pinfo->pid);
            pinfo->enableAS_pend = true;
            dispatch_async(_getPMMainQueue(), ^{
                enableAppSleep(pinfo);
                processInfoRelease(pinfo->pid);
            });
        }
    }
}


void updateAppStats(assertion_t *assertion, assertionOps op)
{
    assertionType_t     *assertType = NULL;
    ProcessInfo         *pinfo = NULL;
    effectStats_t       *stats = NULL;
    uint64_t            duration = 0;

    assertType = &gAssertionTypes[assertion->kassert]; 
    
    if (gActivityAggCnt && (assertType->effectIdx < kMaxEffectStats)) {

        if (assertion->causingPid) {
            // If kIOPMAssertionOnBehalfOfPID is set, stats are collected on that process structure
            if (assertion->causingPinfo == NULL) {
                if (!(assertion->causingPinfo = processInfoRetain(assertion->causingPid))) {
                    assertion->causingPinfo = processInfoCreate(assertion->causingPid);

                    // If allocation failed, use the process creating the assertion itself
                    if (!assertion->causingPinfo)
                        assertion->causingPinfo = processInfoRetain(assertion->pinfo->pid);
                }
            }

            pinfo = assertion->causingPinfo;
        }
        if (!pinfo) {
            pinfo = assertion->pinfo;
        }

        if (!pinfo->reportBuf) allocStatsBuf(pinfo);

        if (pinfo->reportBuf)
            stats = &pinfo->stats[assertType->effectIdx];
    }

    switch (op) {

    case kAssertionOpRaise:
        if ((assertType->flags & kAssertionTypeNotValidOnBatt) && 
                (!(assertion->state & kAssertionStateValidOnBatt)) && (_getPowerSource() == kBatteryPowered)) {
            /*
             * If this assertion type is not allowed on battery and this assertion doesn't have override flag,
             * then don't let this assertion add to stats
             */
            stats = NULL;
        }
        if (stats && !(assertion->state & kAssertionStateAddsToProcStats)) {
            if (stats->cnt++ == 0) {
                stats->startTime = getMonotonicTime();
            }
            assertion->state |= kAssertionStateAddsToProcStats;
        }
        break;

    case kAssertionOpRelease:
        if (stats && (stats->cnt) && (assertion->state & kAssertionStateAddsToProcStats)) {
            if (--stats->cnt == 0) {
                duration = (getMonotonicTime() - stats->startTime);
                SIMPLEARRAY_INCREMENTVALUE(pinfo->reportBuf, assertType->effectIdx, duration);
            }
            assertion->state &= ~kAssertionStateAddsToProcStats;
        }
        break;

    default:
        break;
    }

}

STATIC IOReturn lookupAssertion(pid_t pid, IOPMAssertionID id, assertion_t **assertion)
{
    unsigned int idx = INDEX_FROM_ID(id);
    assertion_t  *tmp_a = NULL;

    if (idx >= kMaxAssertions)
        return kIOReturnBadArgument;

    if( CFDictionaryGetValueIfPresent(gAssertionsArray, 
                                      (const void *)(uintptr_t)idx, (const void **)&tmp_a) == false)
        return kIOReturnBadArgument;

    if (tmp_a->pinfo->pid != pid)
        return kIOReturnNotPermitted;

    *assertion = tmp_a;
    return kIOReturnSuccess;

}

kern_return_t _io_pm_change_sa_assertion_behavior (
                                                   mach_port_t             server  __unused,
                                                   audit_token_t           token,
                                                   uint32_t                newFlags,
                                                   uint32_t                *oldFlags,
                                                   int                     *return_code)
{
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
    if (!callerIsRoot(callerUID))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (oldFlags)
        *oldFlags = gSAAssertionBehaviorFlags;
    gSAAssertionBehaviorFlags = newFlags;

    *return_code = kIOReturnSuccess;

exit:
    return KERN_SUCCESS;
}
kern_return_t _io_pm_declare_system_active (
                                            mach_port_t             server  __unused,
                                            audit_token_t           token,
                                            int                     *system_state,
                                            vm_offset_t             props,
                                            mach_msg_type_number_t  propsCnt,
                                            int                     *assertion_id,
                                            int                     *return_code)
{
    pid_t                   callerPID = -1;
    CFDataRef               unfolder  = NULL;
    CFMutableDictionaryRef  assertionProperties = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);

    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        assertionProperties = (CFMutableDictionaryRef)CFPropertyListCreateWithData
                                    (0, unfolder, kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
        CFRelease(unfolder);
    }

    if (!assertionProperties) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    if (!callerIsEntitledToAssertion(token, assertionProperties))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }
    *system_state = kIOPMSystemSleepNotReverted;
    *return_code = kIOReturnSuccess;


    if(_can_revert_sleep()) {
        *system_state = kIOPMSystemSleepReverted;
    }
    else {
        CFStringRef sleepReason = _getSleepReason();
        if (CFStringCompare(sleepReason, CFSTR(kIOPMIdleSleepKey), 0) == kCFCompareEqualTo) {
            // Schedule an immediate wake if system is going for an idle sleep
            int d = 1;
            CFNumberRef sleepDuration = CFNumberCreate(kCFAllocatorDefault,
                                                       kCFNumberSInt32Type,
                                                       &d);
            if (sleepDuration) {
                _setRootDomainProperty( CFSTR(kIOPMSettingDebugWakeRelativeKey),
                                        sleepDuration);
                CFRelease(sleepDuration);
            }
        }
    }

    // If set via a backdoor in pmset, this makes
    // IOPMAssertionDeclareSystemActivity() behave identically to a
    // PreventUserIdleSystemSleep assertion. It negates the side-effect
    // behaviors associated with the call
    if(gSAAssertionBehaviorFlags != kIOPMSystemActivityAssertionEnabled)
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionTypePreventUserIdleSystemSleep);
    else
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionTypeSystemIsActive);

    if(kIOReturnSuccess != doCreate(callerPID, assertionProperties, (IOPMAssertionID *)assertion_id, NULL, NULL))
        *return_code = kIOReturnInternalError;

exit:

    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);

    return KERN_SUCCESS;
}

kern_return_t  _io_pm_declare_user_active (   
                                           mach_port_t             server  __unused,
                                           audit_token_t           token,
                                           int                     user_type,
                                           vm_offset_t             props,
                                           mach_msg_type_number_t  propsCnt,
                                           int                     *assertion_id,
                                           int                     *disableAppSleep,
                                           int                     *return_code)
{

    CFMutableDictionaryRef      assertionProperties = NULL;
    assertion_t      *assertion = NULL;
    CFDataRef           unfolder = NULL;
    pid_t               callerPID = -1;
    IOReturn            ret;
    bool                create_new = true;
    CFTimeInterval      displaySleepTimerSecs;
    CFNumberRef         CFdisplaySleepTimer = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);    

    *disableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        assertionProperties = (CFMutableDictionaryRef)
                                CFPropertyListCreateWithData(0, unfolder, 
                                                             kCFPropertyListMutableContainersAndLeaves, 
                                                             NULL, NULL);
        CFRelease(unfolder);
    }

    if ((!assertionProperties) || (assertion_id == NULL)) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    if (!callerIsEntitledToAssertion(token, assertionProperties))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }
    /* Set the assertion timeout value to display sleep timer value, if it is not 0 */

    displaySleepTimerSecs = gDisplaySleepTimer * 60; /* Convert to secs */
    CFdisplaySleepTimer = CFNumberCreate(0, kCFNumberDoubleType, &displaySleepTimerSecs);
    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutKey, CFdisplaySleepTimer);
    CFRelease(CFdisplaySleepTimer);

    CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);

    /* Check if this is a repeat call on previously returned assertion id */
    do {
        if (*assertion_id == kIOPMNullAssertionID)
            break;

        ret = lookupAssertion(callerPID, *assertion_id, &assertion);
        if ((kIOReturnSuccess != ret) || !assertion) 
            break;

        if (assertion->kassert != kDeclareUserActivityType)
            break;

        /* Extend the timeout timer of this assertion by display sleep timer value */

        /* First set the assertion level to ON */
        int k = kIOPMAssertionLevelOn;
        CFNumberRef useLevelOnNum = CFNumberCreate(0, kCFNumberIntType, &k);
        CFDictionarySetValue(assertion->props, kIOPMAssertionLevelKey, useLevelOnNum);
        CFRelease(useLevelOnNum);

        *return_code = doSetProperties(callerPID, *assertion_id, assertionProperties, NULL);
        create_new = false;
    } while (false);

    if (create_new) {
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionUserIsActive);

        *return_code = doCreate(callerPID, assertionProperties, (IOPMAssertionID *)assertion_id, NULL, NULL);
        if ((*return_code == kIOReturnSuccess) && 
            (lookupAssertion(callerPID, *assertion_id, &assertion) == kIOReturnSuccess)) {
            assertion->state |= kAssertionTimeoutIsSystemTimer;
        }

    }

    if (*return_code == kIOReturnSuccess)
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, NULL);

exit:


    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);
    return KERN_SUCCESS;
}

kern_return_t  _io_pm_declare_network_client_active (   
                                                     mach_port_t             server  __unused,
                                                     audit_token_t           token,
                                                     vm_offset_t             props,
                                                     mach_msg_type_number_t  propsCnt,
                                                     int                     *assertion_id,
                                                     int                     *disableAppSleep,
                                                     int                     *return_code)
{

    bool                create_new = true;
    pid_t               callerPID = -1;
    CFTimeInterval      idleSleepTimerSecs = 0;
    IOReturn            ret;
    CFDataRef           unfolder = NULL;
    CFNumberRef         idleSleepTimerCF = NULL;
    assertion_t *       assertion = NULL;

    CFMutableDictionaryRef      assertionProperties = NULL;

    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);    

    *disableAppSleep = 0;
    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        assertionProperties = (CFMutableDictionaryRef)
                                CFPropertyListCreateWithData(0, unfolder, 
                                                             kCFPropertyListMutableContainersAndLeaves, 
                                                             NULL, NULL);
        CFRelease(unfolder);
    }

    if ((!assertionProperties) || (assertion_id == NULL))  {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    if (!callerIsEntitledToAssertion(token, assertionProperties))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }
    /* Set the assertion timeout value to idle sleep timer value, if it is not 0 */

    idleSleepTimerSecs = gIdleSleepTimer * 60; /* Convert to secs */

    if (idleSleepTimerSecs) {
        idleSleepTimerCF = CFNumberCreate(0, kCFNumberDoubleType, &idleSleepTimerSecs);
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutKey, idleSleepTimerCF);
        CFRelease(idleSleepTimerCF);

        CFDictionarySetValue(assertionProperties, kIOPMAssertionTimeoutActionKey, kIOPMAssertionTimeoutActionRelease);
    }

    /* Check if this is a repeat call on previously returned assertion id */
    do {
        if (*assertion_id == kIOPMNullAssertionID)
            break;

        ret = lookupAssertion(callerPID, *assertion_id, &assertion);
        if ((kIOReturnSuccess != ret) || !assertion) 
            break;

        if (assertion->kassert != kNetworkAccessType)
            break;

        /* Extend the timeout timer of this assertion by idle sleep timer value */

        /* First set the assertion level to ON */
        int k = kIOPMAssertionLevelOn;
        CFNumberRef useLevelOnNum = CFNumberCreate(0, kCFNumberIntType, &k);
        CFDictionarySetValue(assertion->props, kIOPMAssertionLevelKey, useLevelOnNum);
        CFRelease(useLevelOnNum);

        *return_code = doSetProperties(callerPID, *assertion_id, assertionProperties, NULL);
        create_new = false;
    } while (false);

    if (create_new) {
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertNetworkClientActive);

        *return_code = doCreate(callerPID, assertionProperties, (IOPMAssertionID *)assertion_id, NULL, NULL);
        if ((*return_code == kIOReturnSuccess) && 
            (lookupAssertion(callerPID, *assertion_id, &assertion) == kIOReturnSuccess)) {
            assertion->state |= kAssertionTimeoutIsSystemTimer;
        }

    }

    if (*return_code == kIOReturnSuccess)
        updateAppSleepStates(processInfoGet(callerPID), disableAppSleep, NULL);

exit:
    if (assertionProperties)
        CFRelease(assertionProperties);

    vm_deallocate(mach_task_self(), props, propsCnt);
    return KERN_SUCCESS;
}



int do_assertion_notify(pid_t callerPID, string_t name, int req_type)
{

    ProcessInfo         *pinfo = NULL;
    bool                mod = false;
    int                 return_code = kIOReturnSuccess;

    if (req_type == kIOPMNotifyRegister)
    {
        // Create a dispatch handler for process exit, if there isn't one
        if ( !(pinfo = processInfoRetain(callerPID)) ) {
            pinfo = processInfoCreate(callerPID);
            if (!pinfo) {
                return_code = kIOReturnNoMemory;
                goto exit;
            }
        }
    }
    else {
        if ( !(pinfo = processInfoGet(callerPID)) ) {
            return_code = kIOReturnBadArgument;
            goto exit;
        }
    }

    if (!strncmp(name, kIOPMAssertionsAnyChangedNotifyString, sizeof(kIOPMAssertionsAnyChangedNotifyString)))
    {
        if (req_type == kIOPMNotifyRegister && pinfo->anychange == false) {
            pinfo->anychange = true; 
            mod = true;
            gAnyChange++;
        }
        else if (req_type == kIOPMNotifyDeRegister && pinfo->anychange == true) {
            pinfo->anychange = false; 
            gAnyChange--;
            processInfoRelease(callerPID);
        }
    }
    else if (!strncmp(name, kIOPMAssertionsChangedNotifyString, sizeof(kIOPMAssertionsChangedNotifyString))) 
    {
        if (req_type == kIOPMNotifyRegister && pinfo->aggchange == false) {
            pinfo->aggchange = true; 
            mod = true;
            gAggChange++;
        }
        else if (req_type == kIOPMNotifyDeRegister && pinfo->aggchange == true) {
            pinfo->aggchange = false; 
            gAggChange--;
            processInfoRelease(callerPID);
        }
    }
    else if (!strncmp(name, kIOPMAssertionTimedOutNotifyString, sizeof(kIOPMAssertionTimedOutNotifyString)))
    {
        if (req_type == kIOPMNotifyRegister && pinfo->timeoutchange == false) {
            pinfo->timeoutchange = true; 
            mod = true;
            gTimeoutChange++;
        }
        else if (req_type == kIOPMNotifyDeRegister && pinfo->timeoutchange == true) {
            pinfo->timeoutchange = false; 
            gTimeoutChange--;
            processInfoRelease(callerPID);
        }
    }
    else {
        return_code = kIOReturnBadArgument;
    }

    if (!mod && (req_type == kIOPMNotifyRegister)) 
        processInfoRelease(callerPID);

exit:
    return return_code;

}

static void releaseStatsBuf(ProcessInfo *pinfo)
{

    if (pinfo->reportBuf == NULL) return;

    memset(pinfo->stats, 0, sizeof(pinfo->stats));
    free(pinfo->reportBuf);
    pinfo->reportBuf = NULL;
    processInfoRelease(pinfo->pid);

}

static void releaseStatsBufByPid(pid_t p)
{
    ProcessInfo *pinfo = processInfoGet(p);
    if (pinfo == NULL) {
        return;
    }

    releaseStatsBuf(pinfo);
}

void releaseStatsBufForDeadProcs( )
{
    ProcessInfo **procs = NULL;
    CFIndex i, cnt;

    if (gAggCleanupDispatch) {
        dispatch_source_set_timer(gAggCleanupDispatch,
                dispatch_time(DISPATCH_TIME_NOW, gAggCleanupFrequency),
                DISPATCH_TIME_FOREVER, 0);
    }
    cnt = CFDictionaryGetCount(gProcessDict);
    procs = malloc(cnt*(sizeof(ProcessInfo *)));
    if (!procs) {
        return;
    }
    memset(procs, 0, cnt*(sizeof(procs)));
    CFDictionaryGetKeysAndValues(gProcessDict, NULL, (const void **)procs);

    for (i = 0; i < cnt && procs[i] != NULL; i++) {
        if (procs[i]->proc_exited) {
            releaseStatsBuf(procs[i]);
        }
    }
    free(procs);
}

static void allocStatsBuf(ProcessInfo *pinfo)
{
    kerAssertionEffect i;

    if (gActivityAggCnt == 0) return;
    if (pinfo->reportBuf) return;

    size_t nbytes = SIMPLEARRAY_BUFSIZE(kMaxEffectStats);
    pinfo->reportBuf = malloc(nbytes);

    if (pinfo->reportBuf) {
        SIMPLEARRAY_INIT(kMaxEffectStats, pinfo->reportBuf, nbytes, getpid(),
                         pinfo->pid, /* Channel ID */
                         kIOReportCategoryPower);
        for (i=0; i < kMaxEffectStats; i++) {
            SIMPLEARRAY_SETVALUE(pinfo->reportBuf, i, 0);
        }
        memset(pinfo->stats, 0, sizeof(pinfo->stats));
        processInfoRetain(pinfo->pid);
    }
}

void setAssertionActivityAggregate(pid_t callerPID, int value)
{
    kerAssertionType i;
    assertionType_t *assertType = NULL;
    CFIndex j, cnt;
    ProcessInfo **procs = NULL;
    ProcessInfo *pinfo = NULL;

    if (value) {
        // Hold a reference to callerPID, so that we can reduce gActivityAggCnt when that process dies.
        if ( !(pinfo = processInfoRetain(callerPID)) ) {
            pinfo = processInfoCreate(callerPID);
            if (!pinfo) {
                return;
            }
        }
        if (pinfo->aggactivity) {
            // Already subscribed
            processInfoRelease(callerPID);
            return;
        }
        if (gActivityAggCnt++ == 0) {
            cnt = CFDictionaryGetCount(gProcessDict);
            procs = malloc(cnt*(sizeof(ProcessInfo *)));
            if (!procs) {
                processInfoRelease(callerPID);
                return;
            }

            memset(procs, 0, cnt*(sizeof(procs)));
            CFDictionaryGetKeysAndValues(gProcessDict, NULL, (const void **)procs);
            for (j = 0; (j < cnt) && (procs[j] != NULL); j++) {
                allocStatsBuf(procs[j]);
            }
            free(procs);

            for (i=0; i < kIOPMNumAssertionTypes; i++)
            {
                assertType = &gAssertionTypes[i]; 

                if (assertType->effectIdx >= kMaxEffectStats)
                    continue;
                applyToAssertionsSync(assertType, kSelectActive, ^(assertion_t *assertion)
                                      {
                                          updateAppStats(assertion, kAssertionOpRaise);
                                      });

            }
            // Set up a timer to frequently clean up the dead procs
            if (gAggCleanupDispatch == NULL) {
                gAggCleanupDispatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());
                dispatch_source_set_event_handler(gAggCleanupDispatch, ^{ releaseStatsBufForDeadProcs(); });
                dispatch_source_set_cancel_handler(gAggCleanupDispatch, ^{
                                                   dispatch_release(gAggCleanupDispatch);
                                                   gAggCleanupDispatch = NULL;});
                dispatch_source_set_timer(gAggCleanupDispatch,
                                          dispatch_time(DISPATCH_TIME_NOW, gAggCleanupFrequency), DISPATCH_TIME_FOREVER, 0);
                dispatch_resume(gAggCleanupDispatch);
            }
        }
        pinfo->aggactivity = true;
    }
    else if (gActivityAggCnt) {
        if ( (!(pinfo = processInfoGet(callerPID))) || (pinfo->aggactivity == false) ) {
            return;
        }
        if (--gActivityAggCnt == 0) {
            cnt = CFDictionaryGetCount(gProcessDict);
            procs = malloc(cnt*(sizeof(ProcessInfo *)));
            if (!procs) return;

            memset(procs, 0, cnt*(sizeof(procs)));
            CFDictionaryGetKeysAndValues(gProcessDict, NULL, (const void **)procs);
            for (j = 0; (j < cnt) && (procs[j] != NULL); j++) {
                releaseStatsBuf(procs[j]);
            }
            free(procs);

            for (i=0; i < kIOPMNumAssertionTypes; i++)
            {
                assertType = &gAssertionTypes[i];

                if (assertType->effectIdx >= kMaxEffectStats)
                    continue;
                applyToAssertionsSync(assertType, kSelectAll, ^(assertion_t *assertion)
                                      {
                                          assertion->state &= ~kAssertionStateAddsToProcStats;
                                      });
            }

            if (gAggCleanupDispatch) {
                dispatch_source_cancel(gAggCleanupDispatch);
            }
        }
        pinfo->aggactivity = false;
        processInfoRelease(callerPID);
    }

}


kern_return_t  _io_pm_assertion_notify (   
                                        mach_port_t             server  __unused,
                                        audit_token_t           token,
                                        string_t                name, 
                                        int                     req_type,
                                        int                     *return_code)
{
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    *return_code = kIOReturnSuccess;
    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
    if ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID)))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (req_type != kIOPMNotifyRegister && req_type != kIOPMNotifyDeRegister)
    {
        *return_code = kIOReturnNoMemory;
        goto exit;
    }

    *return_code = do_assertion_notify(callerPID, name, req_type);

exit:
    return KERN_SUCCESS;
}


kern_return_t  _io_pm_set_exception_limits (   
                                            mach_port_t             server  __unused,
                                            audit_token_t           token,
                                            vm_offset_t             props,
                                            mach_msg_type_number_t  propsCnt,
                                            int                     *return_code)
{
    CFDataRef           unfolder = NULL;

    CFDictionaryRef     dict = NULL;
    ProcessInfo **procs = NULL;
    CFIndex  cnt;

    // Check the caller's entitlement 
    if (!auditTokenHasEntitlement(token, CFSTR("com.apple.private.iokit.powerlogging")))  {
        *return_code =  kIOReturnNotPrivileged;
        goto exit;
    }

    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)props, propsCnt, kCFAllocatorNull);
    if (unfolder) {
        dict = (CFDictionaryRef)CFPropertyListCreateWithData(0, unfolder,
                                                             kCFPropertyListMutableContainersAndLeaves,
                                                             NULL, NULL);
        CFRelease(unfolder);
    }

    if (!isA_CFDictionary(dict)) {
        *return_code = kIOReturnBadArgument;
        goto exit;
    }

    cnt = CFDictionaryGetCount(gProcessDict);
    procs = malloc(cnt*(sizeof(ProcessInfo *)));
    if (!procs) {
        goto exit;
    }

    if (gProcAssertionLimits) {
        CFRelease(gProcAssertionLimits);
    }

    gProcAssertionLimits = dict;
    dict = NULL;

    memset(procs, 0, cnt*(sizeof(procs)));
    CFDictionaryGetKeysAndValues(gProcessDict, NULL, (const void **)procs);
    for (int j = 0; (j < cnt) && (procs[j] != NULL); j++) {
        setProcessAssertionLimits(procs[j]);
    }

    if (CFDictionaryGetCount(gProcAssertionLimits)) {
        if (gProcAggregateMonitor == NULL) {
            gProcAggregateMonitor = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());

            dispatch_source_set_event_handler(gProcAggregateMonitor, ^{ checkProcAggregates(); });

            dispatch_source_set_cancel_handler(gProcAggregateMonitor, ^{
                    dispatch_release(gProcAggregateMonitor);
                    gProcAggregateMonitor = NULL;
                    });

            if (_getPowerSource() == kBatteryPowered) {
                dispatch_source_set_timer(gProcAggregateMonitor,
                                          dispatch_time(DISPATCH_TIME_NOW, 0), gProcMonitorFrequency, 0);
            }
            else {
                // No need to check aggregate stats periodically when external power source is created
                dispatch_source_set_timer(gProcAggregateMonitor,
                                          dispatch_time(DISPATCH_TIME_FOREVER, 0), DISPATCH_TIME_FOREVER, 0);
            }
            dispatch_resume(gProcAggregateMonitor);

            // Enable process level assertion aggregate stats
            setAssertionActivityAggregate(getpid(), 1);
        }

    }
    else {
        // Empty gProcAssertionLimits means cancel all monitoring
        if (gProcAggregateMonitor) {
            dispatch_source_cancel(gProcAggregateMonitor);
            setAssertionActivityAggregate(getpid(), 0);
            if (gProcAggregateBasis) {
                CFRelease(gProcAggregateBasis);
                gProcAggregateBasis = NULL;
            }
        }
    }

    *return_code = kIOReturnSuccess;
exit:
    if (procs) { free(procs); }
    if (dict) { CFRelease(dict); }
    vm_deallocate(mach_task_self(), props, propsCnt);
    return KERN_SUCCESS;
}


uint8_t getAssertionLevel(kerAssertionType idx)
{
    return ((aggregate_assertions & (1 << idx)) ? 1:0);
}

void setAggregateLevel(kerAssertionType idx, uint8_t val)
{
    if (val)
        aggregate_assertions |= (1 << idx);
    else
        aggregate_assertions &= ~(1<<idx);
}

uint32_t getKerAssertionBits( )
{
    return kerAssertionBits;
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

void insertActiveAssertion(assertion_t *assertion, assertionType_t *assertType, bool updates)
{
    LIST_INSERT_HEAD(&assertType->active, assertion, link);
    assertion->state &= ~(kAssertionStateTimed|kAssertionStateInactive|kAssertionSkipLogging);

    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) &&
         (assertion->state & kAssertionStateValidOnBatt) )
        assertType->validOnBattCount++;

    if (updates) {
        updateAppStats(assertion, kAssertionOpRaise);
        schedDisableAppSleep(assertion);

        updateSystemQualifiers(assertion, kAssertionOpRaise);
    }
    startProcTimer(assertion);

}

void removeActiveAssertion(assertion_t *assertion, assertionType_t *assertType, bool updates)
{
    LIST_REMOVE(assertion, link);

    if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
        assertType->validOnBattCount--;

    if (updates) {
        updateAppStats(assertion, kAssertionOpRelease);
        schedEnableAppSleep(assertion);

        updateSystemQualifiers(assertion, kAssertionOpRelease);
    }
    stopProcTimer(assertion);

}

void resetAssertionTimer(assertionType_t *assertType)
{
    uint64_t currTime ;
    assertion_t *nextAssertion = NULL;

    nextAssertion = LIST_FIRST(&assertType->activeTimed);
    if (!nextAssertion) return;

    currTime = getMonotonicTime();

    if (nextAssertion->timeout <= currTime) {
        dispatch_async(_getPMMainQueue(), ^{
            handleAssertionTimeout(assertType);
        });
    }
    else {

        dispatch_source_set_timer(assertType->timer, 
                                  dispatch_time(DISPATCH_TIME_NOW, (nextAssertion->timeout-currTime)*NSEC_PER_SEC), 
                                  DISPATCH_TIME_FOREVER, 0);
    }

}


static void releaseAssertionMemory(assertion_t *assertion, assertLogAction logAction)
{
    int idx = INDEX_FROM_ID(assertion->assertionId);
    assertion_t *tmp_a = NULL;

    if ( (idx < 0) || (idx >= kMaxAssertions) || 
         (CFDictionaryGetValueIfPresent(gAssertionsArray, 
                                        (const void *)(uintptr_t)idx, (const void **)&tmp_a) == false) || (tmp_a != assertion) ) {
#ifdef DEBUG
        abort();
#endif
        return; // This is an error
    }

    assertion->retainCnt = 0;
    logAssertionEvent(logAction, assertion);
    CFDictionaryRemoveValue(gAssertionsArray, (const void *)(uintptr_t)idx);
    if (assertion->props) CFRelease(assertion->props);


    processInfoRelease(assertion->pinfo->pid);
    if (assertion->causingPinfo) {
        processInfoRelease(assertion->causingPinfo->pid);
    }

    if (assertion->procTimer) {
        if ((assertion->state & kAssertionProcTimerActive) == 0) {
            // If procTimer is suspended, resume it to allow its cancellation
            assertion->state |= kAssertionProcTimerActive;
            dispatch_resume(assertion->procTimer);
        }
        dispatch_source_cancel(assertion->procTimer);
    }
    memset(assertion, 0, sizeof(assertion_t));
    free(assertion);
}

void handleAssertionTimeout(assertionType_t *assertType)
{
    assertion_t     *assertion;
    CFDateRef       dateNow = NULL;
    uint64_t        currtime = getMonotonicTime( );
    uint32_t        timedoutCnt = 0;
    CFStringRef     timeoutAction = NULL;
    bool            displayProxy = false;

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

        updateAppStats(assertion, kAssertionOpRelease);
        schedEnableAppSleep( assertion );
        stopProcTimer(assertion);
        updateSystemQualifiers(assertion, kAssertionOpRelease);
#if !TARGET_OS_SIMULATOR
        entr_act_end(kEnTrCompSysPower, kEnTrActSPPMAssertion,
                     assertion->assertionId, kEnTrQualTimedOut, kEnTrValNone);
#endif

        if ((dateNow = CFDateCreate(0, CFAbsoluteTimeGetCurrent()))) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionTimedOutDateKey, dateNow);            
            CFRelease(dateNow);
        }


        if ( (assertion->kassert == kPreventDisplaySleepType) && 
             (assertion->pinfo->pid != getpid()))
            displayProxy = true;

        timeoutAction = CFDictionaryGetValue(assertion->props, kIOPMAssertionTimeoutActionKey);
        if (isA_CFString(timeoutAction) && CFEqual(kIOPMAssertionTimeoutActionRelease, timeoutAction))
        { 
            if (assertion->pinfo->remoteConnection) {
                sendAssertionTimeoutMsg(assertion);
            }
            releaseAssertionMemory(assertion, kATimeoutLog);
        }
        else /* Default timeout action is to turn off */
        {
            logAssertionEvent(kATimeoutLog, assertion);
            assertion->state |= kAssertionSkipLogging;

            // Leave this in the inactive assertions list
            insertInactiveAssertion(assertion, assertType);
            if (isA_CFString(timeoutAction) && CFEqual(kIOPMAssertionTimeoutActionKillProcess, timeoutAction))
            {
                kill(assertion->pinfo->pid, SIGTERM);
            }

        }

    }

    if ( !timedoutCnt ) return;

    resetAssertionTimer(assertType);

    if (displayProxy) delayDisplayTurnOff( );

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);

    logASLAssertionsAggregate();
    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

}

void removeTimedAssertion(assertion_t *assertion, assertionType_t *assertType, bool updateTimer, bool updates)
{
    bool isTheFirstOne = false;

    CFDictionaryRemoveValue(assertion->props, kIOPMAssertionTimeoutTimeLeftKey);
    if (LIST_FIRST(&assertType->activeTimed) == assertion) {
        isTheFirstOne = true;
    }
    LIST_REMOVE(assertion, link);
    assertion->state &= ~kAssertionStateTimed;

    if ( (assertion->state & kAssertionStateValidOnBatt) && assertType->validOnBattCount)
        assertType->validOnBattCount--;

    if (updates) {
        updateAppStats(assertion, kAssertionOpRelease);
        schedEnableAppSleep(assertion);
        updateSystemQualifiers(assertion, kAssertionOpRelease);
    }
    stopProcTimer(assertion);
    if (isTheFirstOne && updateTimer) resetAssertionTimer(assertType);

}

void updateAssertionTimer(assertionType_t *assertType)
{
    uint64_t    currTime;
    assertion_t *assertion = NULL;

    if ((assertion = LIST_FIRST(&assertType->activeTimed)) == NULL) return;

    /* Update/create the dispatch timer.  */
    if (assertType->timer == NULL) {
        assertType->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());

        dispatch_source_set_event_handler(assertType->timer, ^{
                                          handleAssertionTimeout(assertType);
                                          });

        dispatch_source_set_cancel_handler(assertType->timer, ^{
                                           dispatch_release(assertType->timer);
                                           });

        dispatch_resume(assertType->timer);
    }

    currTime = getMonotonicTime();


    if (assertion->timeout <= currTime) {
        /* This has already timed out. */
        dispatch_async(_getPMMainQueue(), ^{
            handleAssertionTimeout(assertType);
        });
    }
    else {
        dispatch_source_set_timer(assertType->timer, 
                                  dispatch_time(DISPATCH_TIME_NOW, (assertion->timeout-currTime)*NSEC_PER_SEC), 
                                  DISPATCH_TIME_FOREVER, 0);
    }


}

/* Inserts assertion into activeTimed list, sorted by timeout */
static void insertByTimeout(assertion_t *assertion, assertionType_t *assertType)
{
    assertion_t *a, *prev = NULL;
    CFNumberRef         timeLeftCF = NULL;
    uint64_t            currTime, timeLeft;
    CFDateRef           updateDate = NULL;

    currTime = getMonotonicTime();
    if (assertion->timeout > currTime) {
        /* Update timeout time left property */
        timeLeft = assertion->timeout - currTime;
        timeLeftCF = CFNumberCreate(0, kCFNumberLongType, &timeLeft);

        if (timeLeftCF) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutTimeLeftKey, timeLeftCF);
            CFRelease(timeLeftCF);
        }

        updateDate = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        if (updateDate) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutUpdateTimeKey, updateDate);
            CFRelease(updateDate);
        }
    }

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

}

void insertTimedAssertion(assertion_t *assertion, assertionType_t *assertType, bool updateTimer, bool updates)
{
    insertByTimeout(assertion, assertType);

    assertion->state |= kAssertionStateTimed;
    assertion->state &= ~kAssertionSkipLogging;

    if ( (assertType->flags & kAssertionTypeNotValidOnBatt) &&
         (assertion->state & kAssertionStateValidOnBatt) )
        assertType->validOnBattCount++;

    if (updates) {
        updateAppStats(assertion, kAssertionOpRaise);
        schedDisableAppSleep( assertion );
        updateSystemQualifiers(assertion, kAssertionOpRaise);
    }
    startProcTimer(assertion);
    /*  
     * If this assertion is not the one with earliest timeout,
     * there is nothing to do.
     */
    if (LIST_FIRST(&assertType->activeTimed) != assertion)
        return;

    if (updateTimer) updateAssertionTimer(assertType);

    return;
}

static void resumeAssertion(assertion_t *assertion)
{
    assertion->state &= ~kAssertionStateSuspended;

    CFDictionarySetValue(assertion->props, kIOPMAssertionIsStateSuspendedKey,
                         (CFBooleanRef)kCFBooleanFalse);

    uint64_t currTime = getMonotonicTime();
    assertionType_t *assertType = &gAssertionTypes[assertion->kassert];

    /* Is level set to 0 */
    int level = 0;
    CFNumberRef numCF = CFDictionaryGetValue(assertion->props, kIOPMAssertionLevelKey);
    if (isA_CFNumber(numCF)) {
        CFNumberGetValue(numCF, kCFNumberIntType, &level);
        if (level == kIOPMAssertionLevelOff) {
            /* Dump this assertion in inactive list */
            insertInactiveAssertion(assertion, assertType);
            return;
        }
    }

    /* Check if a timed Assertion is being resumed */
    CFTimeInterval timeRemaining = 0;
    CFNumberRef timeRemainingCF = CFDictionaryGetValue(assertion->props, kPMAssertionTimeoutOnResumeKey);
    if (isA_CFNumber(timeRemainingCF)) {
        CFNumberGetValue(timeRemainingCF, kCFNumberDoubleType, &timeRemaining);
        CFDictionaryRemoveValue(assertion->props, kPMAssertionTimeoutOnResumeKey);
    }

    if (timeRemaining) {
        // Absolute time at which assertion expires
        assertion->timeout = (uint64_t)timeRemaining + currTime;
        insertTimedAssertion(assertion, assertType, true, false);
    }
    else {
        /* Insert into active assertion list */
        insertActiveAssertion(assertion, assertType, false);
    }

    logAssertionEvent(kAStateResume, assertion);
}

static void releaseAssertion(assertion_t *assertion, bool callHandler)
{
    assertionType_t     *assertType;
    bool active = false;

    assertType = &gAssertionTypes[assertion->kassert]; 

    if (assertion->state & kAssertionStateTimed) {
        removeTimedAssertion(assertion, assertType, true, true);
        active = true;
    }
    else if (assertion->state & kAssertionStateInactive) {
        removeInactiveAssertion(assertion, assertType);
    }
    else {
        removeActiveAssertion(assertion, assertType, true);
        active = true;
    }

    if (active && (assertion->kassert == kPreventDisplaySleepType) &&
         (assertion->pinfo->pid != getpid())) {
        delayDisplayTurnOff( );
    }

    if (!callHandler) return;

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);


}

static void suspendAssertion(assertion_t *assertion)
{
    /* timeout is valid only for Timed Assertions */
    if (assertion->timeout){
        uint64_t currTime = getMonotonicTime();
        if (assertion->timeout > currTime) {
            uint64_t timeRemaining = assertion->timeout - currTime;
            CFNumberRef timeRemainingCF = CFNumberCreate(0, kCFNumberLongType, &timeRemaining);
            CFDictionarySetValue(assertion->props, kPMAssertionTimeoutOnResumeKey, timeRemainingCF);
            CFRelease(timeRemainingCF);
        }
    }

    releaseAssertion(assertion, false);

    assertion->state |= kAssertionStateSuspended;

    CFDictionarySetValue(assertion->props, kIOPMAssertionIsStateSuspendedKey,
                         (CFBooleanRef)kCFBooleanTrue);

    logAssertionEvent(kAStateSuspend, assertion);

}

STATIC IOReturn doRelease(pid_t pid, IOPMAssertionID id, int *retainCnt)
{
    IOReturn                    ret;

    assertion_t    *assertion = NULL;
    ret = lookupAssertion(pid, id, &assertion);
    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    if (assertion->retainCnt)
        assertion->retainCnt--;

    if (retainCnt)
        *retainCnt = assertion->retainCnt;

    if (assertion->retainCnt) {
        return kIOReturnSuccess;
    }

    releaseAssertion(assertion, true);
    releaseAssertionMemory(assertion, kAReleaseLog);

    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

    return kIOReturnSuccess;
}

__private_extern__ void applyToAssertionsSync(assertionType_t *assertType,
                                              listSelectType_t assertionListSelect,
                                              void (^performOnAssertion)(assertion_t *))
{
    assertion_t* list[kIndexMaxCount];

    list[kIndexActiveTimed] = LIST_FIRST(&assertType->activeTimed);
    list[kIndexActive] = LIST_FIRST(&assertType->active);
    list[kIndexInactive] = LIST_FIRST(&assertType->inactive);
    list[kIndexSuspended] = LIST_FIRST(&assertType->suspended);

    for (listIndexType_t idx = 0; idx < kIndexMaxCount; idx++) {

        if ((SELECT_MASK(idx) & assertionListSelect) == 0)
            continue;

        assertion_t * assertion = list[idx];
        while (assertion)
        {
            assertion_t* nextAssertion = LIST_NEXT(assertion, link);
            performOnAssertion(assertion);
            assertion = nextAssertion;
        }
    }
}

__private_extern__ void HandleProcessExit(pid_t deadPID)
{
    int i;
    assertionType_t *assertType = NULL;
    assertion_t     *assertion = NULL;
    assertion_t     *nextAssertion = NULL;
    __block LIST_HEAD(, assertion) list  = LIST_HEAD_INITIALIZER(list);     /* list of assertions released */
    ProcessInfo         *pinfo = NULL;

    if ( (pinfo = processInfoGet(deadPID)) ) {
        pinfo->proc_exited = 1;
        if (pinfo->remoteConnection) {
            xpc_release(pinfo->remoteConnection);
            pinfo->remoteConnection = 0;
        }
    }

    do_assertion_notify(deadPID, kIOPMAssertionsAnyChangedNotifyString, kIOPMNotifyDeRegister);
    do_assertion_notify(deadPID, kIOPMAssertionTimedOutNotifyString, kIOPMNotifyDeRegister);
    do_assertion_notify(deadPID, kIOPMAssertionsChangedNotifyString, kIOPMNotifyDeRegister);

    setAssertionActivityAggregate(deadPID, 0);

    /* Go thru each assertion type and release all assertion from its lists */
    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertType = &gAssertionTypes[i]; 

        applyToAssertionsSync(assertType, kSelectAll, ^(assertion_t *assertion)
                              {
                                  if (assertion->pinfo->pid == deadPID) {
                                      releaseAssertion(assertion, false);
                                      LIST_INSERT_HEAD(&list, assertion, link);
                                  }
                              });

        if (assertType->handler)
            (*assertType->handler)(assertType, kAssertionOpRelease);

        /* Release memory after calling the handler to get proper aggregate_assertions value into log */
        assertion = LIST_FIRST(&list);
        while (assertion != NULL)
        {
#if !TARGET_OS_SIMULATOR
            entr_act_end(kEnTrCompSysPower, kEnTrActSPPMAssertion,
                                    assertion->assertionId, kEnTrQualNone, kEnTrValNone);
#endif
            LIST_REMOVE(assertion, link);
            nextAssertion = LIST_FIRST(&list);
            releaseAssertionMemory(assertion, kAClientDeathLog);
            assertion = nextAssertion;
        }

    }
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );


}

void handleAssertionSuspend(pid_t pid)
{
    ProcessInfo *pinfo = processInfoGet(pid);

    if (!pinfo) {
        ERROR_LOG("handleAssertionSuspend: Process with pid %d not found.\n", pid);
        return;
    }
    else {
        // A suspended process cannot be suspended again
        if (pinfo->isSuspended) {
            ERROR_LOG("handleAssertionSuspend: Process with pid %d is already Suspended.\n", pid);
            return;
        }
    }

    /* Go thru each assertion type and suspend all assertions matching pid */
    for (int i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertionType_t *assertType = &gAssertionTypes[i];

        applyToAssertionsSync(assertType, kSelectAllButSuspended,
                              ^(assertion_t *assertion)
                              {
                                  if (assertion->pinfo->pid == pid ||
                                      assertion->causingPid == pid) {
                                      assertionType_t *assertType = &gAssertionTypes[assertion->kassert];
                                      suspendAssertion(assertion);
                                      LIST_INSERT_HEAD(&assertType->suspended, assertion, link);
                                  }
                              });

        if (assertType->handler)
            (*assertType->handler)(assertType, kAssertionOpEval);

    }

    pinfo->isSuspended = 1;

    if (gAnyChange)
        notify_post( kIOPMAssertionsAnyChangedNotifyString );
}

void handleAssertionResume(pid_t pid)
{
    ProcessInfo *pinfo = processInfoGet(pid);

    if (!pinfo || !pinfo->isSuspended){
        ERROR_LOG("handleAssertionResume: Process with pid %d not found or not Suspended.\n", pid);
        return;
    }

    /* Go thru each assertion type and resume all suspended assertions matching pid */
    for (int i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertionType_t *assertType = &gAssertionTypes[i];

        applyToAssertionsSync(assertType, kSelectSuspended,
                              ^(assertion_t *assertion)
                              {
                                  if (assertion->pinfo->pid == pid ||
                                      assertion->causingPid == pid) {
                                      LIST_REMOVE(assertion, link);
                                      resumeAssertion(assertion);
                                  }
                              });
        if (assertType->handler)
            (*assertType->handler)(assertType, kAssertionOpEval);
    }

    pinfo->isSuspended = 0;

    if (gAnyChange)
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
    CFTimeInterval      timeout = 0;
    int level;

    if (!isA_CFString(key))
        return; /* Key has to be a string */


    assertType = &gAssertionTypes[assertion->kassert];
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
        CFNumberGetValue(value, kCFNumberDoubleType, &timeout);

        if (assertType->flags & kAssertionTypeAutoTimed) {
            /* Restrict timeout to a max value of 'autoTimeout' */
            if (!timeout || (timeout > assertType->autoTimeout))
                timeout = assertType->autoTimeout;
        }

        if (timeout) {
            assertion->timeout = (uint64_t)timeout + getMonotonicTime(); // Absolute time at which assertion expires
        }
        else  {
            assertion->timeout = 0;
        }

        /* Setting a timeout makes an inactive assertion active again */
        if (assertion->state & kAssertionStateInactive) {
            assertion->state &= ~kAssertionStateInactive;
            assertion->mods |= kAssertionModLevel;
        }
        else {
            assertion->mods |= kAssertionModTimer;
        }
    }
    else if (CFEqual(key, kIOPMAssertionAppliesToLimitedPowerKey)) {
        if (!isA_CFBoolean(value)) return;
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
    else if (CFEqual(key, kIOPMAssertionAppliesOnLidClose)) {
        if (!isA_CFBoolean(value)) return;
        if ((value == kCFBooleanTrue) && !(assertion->state & kAssertionLidStateModifier)) {
            assertion->state |= kAssertionLidStateModifier;
            assertion->mods |= kAssertionModLidState;
        }
        else if((value == kCFBooleanFalse) && (assertion->state & kAssertionLidStateModifier)) {
            assertion->state &= ~kAssertionLidStateModifier;
            assertion->mods |= kAssertionModLidState;
        }
    }
    else if (CFEqual(key, kIOPMAssertionExitSilentRunning)) {
        if (!isA_CFBoolean(value)) return;
        if ((value == kCFBooleanTrue) && !(assertion->state & kAssertionExitSilentRunningMode)) {
            assertion->state |= kAssertionExitSilentRunningMode;
            assertion->mods |= kAssertionModSilentRunning;
        }
    }
    else if (CFEqual(key, kIOPMAssertionOnBehalfOfPID)) {
        if (!isA_CFNumber(value)) return;
        assertion->mods |= kAssertionModCausingPid;
    }
    else if (CFEqual(key, kIOPMAssertionTypeKey)) {
        /* Assertion type can't be modified */
        return;
    }
    else if (CFEqual(key, kIOPMAssertionNameKey)) {
        assertion->mods |= kAssertionModName;
    }
    else if (CFEqual(key, kIOPMAssertionResourcesUsed) ||
             CFEqual(key, kIOPMAssertionAllowsDeviceRestart))  {
        assertion->mods |= kAssertionModResources;
    }

    CFDictionarySetValue(assertion->props, key, value);


}

STATIC IOReturn doSetProperties(pid_t pid,
                                IOPMAssertionID id, 
                                CFDictionaryRef inProps,
                                int *enTrIntensity)
{
    assertion_t                 *assertion = NULL;
    assertionType_t             *assertType;
    uint32_t                    oldState;
    ProcessInfo                 *pinfo = NULL;
    ProcessInfo                 *causingPinfo = NULL;

    IOReturn                    ret;

    // doSetProperties doesn't handle retain()/release() count. 
    // Callers should use IOPMAssertionRetain() or IOPMAssertionRelease().

    ret = lookupAssertion(pid, id, &assertion);

    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    // Assertions created by suspended pids are not allowed to be manipulated
    pinfo = processInfoGet(pid);
    if ( pinfo == NULL ) {
        return kIOReturnBadArgument;
    }

    if ( pinfo->isSuspended ) {
        return kIOReturnNotPermitted;
    }

    // Assertions created on behalf of suspended pids are not allowed to be manipulated
    causingPinfo = processInfoGet(assertion->causingPid);
    if ( causingPinfo && causingPinfo->isSuspended) {
        return kIOReturnNotPermitted;
    }

    assertion->mods = 0;
    assertType = &gAssertionTypes[assertion->kassert];
    oldState = assertion->state;
    CFDictionaryApplyFunction(inProps, forwardPropertiesToAssertion,
                              assertion);

    if (assertion->mods & kAssertionModCausingPid) {
        //TODO This cannot be enabled, until RunningBoard moves to new API
        //return kIOReturnNotPermitted;
    }

    if (enTrIntensity) {
        *enTrIntensity = assertType->enTrQuality;
    }

    if (assertion->mods & kAssertionModName) {
        logAssertionEvent(kANameChangeLog, assertion);
    }

    if (assertion->mods & kAssertionModLevel) 
    {
        if (assertion->state & kAssertionStateInactive) 
        {
            /* Remove from active or activeTimed list */
            if (oldState & kAssertionStateTimed) 
                removeTimedAssertion(assertion, assertType, true, true);
            else 
                removeActiveAssertion(assertion, assertType, true);

            /* Add to inActive list */
            insertInactiveAssertion(assertion, assertType);

            if ( (assertion->kassert == kPreventDisplaySleepType) && 
                 (assertion->pinfo->pid != getpid()))
                delayDisplayTurnOff( );
            if (assertType->handler)
                (*assertType->handler)(assertType, kAssertionOpRelease);

            logAssertionEvent(kATurnOffLog, assertion);
        }
        else 
        {
            /* An inactive assertion is made active now */
            removeInactiveAssertion(assertion, assertType);
            CFDictionaryRemoveValue(assertion->props, kIOPMAssertionTimedOutDateKey);            
            CFDictionaryRemoveValue(assertion->props, kIOPMAssertionCreateDateKey);
            raiseAssertion(assertion);
            logAssertionEvent(kATurnOnLog, assertion);
        }
        if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
        return kIOReturnSuccess;
    }

    if (assertion->state & kAssertionStateInactive)
        return kIOReturnSuccess;    

    if  (assertion->mods & kAssertionModTimer) 
    {
        /* Remove from active or activeTimed list */
        /* Assertion timeout is being updated. Removal and
         * insertion should not call updates
         */
        if (oldState & kAssertionStateTimed)
            removeTimedAssertion(assertion, assertType, true, false);
        else
            removeActiveAssertion(assertion, assertType, false);

        assertion->createTime = getMonotonicTime();
        CFDateRef start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        if (start_date) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionCreateDateKey, start_date);
            CFRelease(start_date);
        }
        if (assertion->timeout != 0) {
            insertTimedAssertion(assertion, assertType, true, false);
        }
        else {
            insertActiveAssertion(assertion, assertType, false);
        }

        if (assertion->kassert == kDeclareUserActivityType) {
            bool userActive = userActiveRootDomain();
            (*assertType->handler)(assertType, kAssertionOpRaise);
            if (!userActive) {
                // Log the assertion changing the user activity state
                logAssertionEvent(kATurnOnLog, assertion);
            }
        }

        if (assertType->handler) 
            (*assertType->handler)(assertType, kAssertionOpEval);
    }


    if ( (assertion->mods & kAssertionModPowerConstraint) &&
         (assertType->handler) )
    {
        if (assertion->state & kAssertionStateValidOnBatt) {
            (*assertType->handler)(assertType, kAssertionOpRaise);
            updateAppStats(assertion, kAssertionOpRaise);
        }
        else {
            (*assertType->handler)(assertType, kAssertionOpRelease);
            updateAppStats(assertion, kAssertionOpRelease);
        }

    }

    if ( (assertion->mods & kAssertionModLidState) &&
         (assertType->handler) )
    {
        if (assertion->state & kAssertionLidStateModifier)
            (*assertType->handler)(assertType, kAssertionOpRaise);
        else
            (*assertType->handler)(assertType, kAssertionOpRelease);

    }

    if ( (assertion->mods & kAssertionModSilentRunning) &&
         (assertType->handler) )
    {
        (*assertType->handler)(assertType, kAssertionOpEval);

    }
    if (assertion->mods & kAssertionModResources) {
        updateSystemQualifiers(assertion, kAssertionOpEval);
    }

    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
    return kIOReturnSuccess;    
}

__private_extern__ bool checkForActivesByEffect(kerAssertionEffect effectIdx)
{
    bool                typeActive = false;
    assertionType_t     *type = NULL;
    assertionEffect_t   *effect = NULL;

    if (effectIdx == kNoEffect)
        return false;

    effect = &gAssertionEffects[effectIdx];

    LIST_FOREACH(type, &effect->assertTypes, link) {

        /*
         * Check for active assertions in this assertionType's 'active' & 'activeTimed' lists
         */
        if ( (type->flags & kAssertionTypeNotValidOnBatt) && ( _getPowerSource() == kBatteryPowered) )
            typeActive = (type->validOnBattCount > 0);
        else
            typeActive = (LIST_FIRST(&type->active) || LIST_FIRST(&type->activeTimed));

        if (typeActive)
            return true;
    }

    return false;

}

/*
 * Check for active assertions of the specified type and also for active
 * assertions of linked types.
 * Returns true if active assertions exist either in the specified type or
 * in the linked  types to the specified type.
 */
bool checkForActives(assertionType_t *assertType, bool *existsInThisType )
{
    bool                typeActive = false;
    bool                effectActive = false;
    assertionType_t     *type = NULL;
    assertionEffect_t   *effect = NULL;

    if (existsInThisType) 
        *existsInThisType = false;
    if (assertType->effectIdx == kNoEffect)
        return false;

    effect = &gAssertionEffects[assertType->effectIdx];

    LIST_FOREACH(type, &effect->assertTypes, link) {

        /* 
         * Check for active assertions in this assertionType's 'active' & 'activeTimed' lists 
         */
        if ( (type->flags & kAssertionTypeNotValidOnBatt) && ( _getPowerSource() == kBatteryPowered) )
            typeActive = (type->validOnBattCount > 0);
        else
            typeActive = (LIST_FIRST(&type->active) || LIST_FIRST(&type->activeTimed));

        effectActive |= typeActive; 
        if (existsInThisType && (type == assertType) && typeActive) { 
            *existsInThisType = true;
            return true;
        }
        if (typeActive && !existsInThisType) 
            return true;

    }

    return effectActive;
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
    return checkForActives(&gAssertionTypes[kPreventSleepType], NULL);
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

/*
 * Returns true if there are assertions using audio resources
 */
__private_extern__ bool checkForAudioType( )
{
    return (gSysQualifier.audioin+gSysQualifier.audioout) ? 1 : 0;
}

/* Disable the specified assertion type */
__private_extern__ void disableAssertionType(kerAssertionType type)
{
    gAssertionTypes[type].disableCnt++;
    configAssertionType(type, false);

}

/* Enablee the specified assertion type */
__private_extern__ void enableAssertionType(kerAssertionType type)
{
    if (gAssertionTypes[type].disableCnt)
        gAssertionTypes[type].disableCnt--;

    configAssertionType(type, false);

}

__private_extern__ kern_return_t setReservePwrMode(int enable)
{
    return kIOReturnError;
}

static inline void updateAggregates(assertionType_t *assertType, bool activesForTheType)
{
    if (activesForTheType) {
        setAggregateLevel(assertType->kassert,  1 );
    }
    else  {
        if (getAssertionLevel(assertType->kassert) && assertType->globalTimeout) {
            resetGlobalTimer(assertType, 0);
        }
        setAggregateLevel(assertType->kassert,  0 );
    }

    if((assertType->kassert == kDeclareUserActivityType) || (assertType->kassert == kPreventDisplaySleepType) || (assertType->kassert == kNetworkAccessType) || (assertType->kassert == kTicklessDisplayWakeType)) {
        userActiveHandlePowerAssertionsChanged();
    }
}


static void modifySettings(assertionType_t *assertType, assertionOps op)
{
    bool    activeExists;
    bool    activesForTheType = false;
    int     opVal;
    bool    assertionLevel = 0;

    if (assertType->effectIdx == kNoEffect)
        return;

    assertionLevel = getAssertionLevel(assertType->kassert);

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
    case kHighPerfType:
        overrideSetting( kPMForceHighSpeed, opVal);
        break;

    case kDeclareSystemActivityType:
        // Behaves identical to a PreventIdleSleep
        // assertion, except it also backs out of
        // idle if possible.
    case kBackgroundTaskType:
    case kPreventIdleType:
    case kNetworkAccessType:
    case kInteractivePushServiceType:
    case kReservePwrPreventIdleType:
        overrideSetting(kPMPreventIdleSleep, opVal);
        break;

    case kPreventDiskSleepType:
        overrideSetting(kPMPreventDiskSleep, opVal);
        break;

    default:
        return;
    }

    activateSettingOverrides();
    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
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
        if ( getAssertionLevel(assertType->kassert) || !activeExists )
            return;
        setAggregateLevel(assertType->kassert, 1);
    }
    else {
        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !getAssertionLevel(assertType->kassert) || activeExists)
            return;

        setAggregateLevel(assertType->kassert, 0);
    }

    switch(assertType->kassert) {
    case kDisableInflowType:
        sendSmartBatteryCommand( kSBUCInflowDisable, 
                                 op == kAssertionOpRaise ? 1 : 0);
        break;

    case kInhibitChargeType:
        sendSmartBatteryCommand( kSBUCChargeInhibit, 
                                 op == kAssertionOpRaise ? 1 : 0);
        break;

    case kDisableWarningsType:
        _setRootDomainProperty( CFSTR("BatteryWarningsDisabled"), kCFBooleanTrue);
        break;

    default:
        break;
    }

    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
    return;
}

void setKernelAssertions(assertionType_t *assertType, assertionOps op)
{
    uint32_t    assertBit = 0;
    bool    activeExists, activesForTheType;

    if (assertType->effectIdx == kNoEffect)
        return;

    /* 
     * active assertions exists if one of the active list is not empty and
     * the assertion is not globally disabled.
     */
    activeExists = checkForActives(assertType, &activesForTheType);

    switch(assertType->kassert) {

    case kPushServiceTaskType:
#if LOG_SLEEPSERVICES
        if ( !activesForTheType && getAssertionLevel(assertType->kassert) )
            logASLMessageSleepServiceTerminated(assertType->forceTimedoutCnt);
#endif
        assertBit = kIOPMDriverAssertionCPUBit;
        break;

    case kInteractivePushServiceType:
    case kPreventSleepType:
    case kBackgroundTaskType:
    case kSRPreventSleepType:
    case kNetworkAccessType:
        assertBit = kIOPMDriverAssertionCPUBit;
        break;

    case kDeclareUserActivityType:
    case kPreventDisplaySleepType:
    case kIntPreventDisplaySleepType:
        assertBit = kIOPMDriverAssertionPreventDisplaySleepBit;
        break;

    case kExternalMediaType:
        assertBit = kIOPMDriverAssertionExternalMediaMountedBit;
        break;

    default:
        return;
    }

    updateAggregates(assertType, activesForTheType);

    if (op == kAssertionOpRaise)  {

        if ((assertType->kassert == kDeclareUserActivityType) && activesForTheType) {
            setClamshellSleepState();
            sendActivityTickle();
            _unclamp_silent_running(true);
        }

        /*
         * If server-mode power assertion is being raised anew, then we need
         * to unclamp SilentRunning through AppleSMC
         */
        if ( ((assertType->kassert == kPreventSleepType) || (assertType->kassert == kNetworkAccessType))
             && activesForTheType) {
            _unclamp_silent_running(true);
            setVMDarkwakeMode(false);
        }
        /*
         * if already raised with kernel or if there are no active ones,
         * nothing to do
         */
        if ( (kerAssertionBits & assertBit) || !activeExists )
            return;

    }
    else if (op == kAssertionOpRelease)  {
        if (assertType->kassert == kDeclareUserActivityType) {
            setClamshellSleepState();
        }

        /*
         * If this assertionType is not raised with kernel
         * or if there are active ones, nothing to do
         */
        if ( !(kerAssertionBits & assertBit) || activeExists)
            return;

    }
    else { // op == kAssertionOpEval

        if ( (assertType->kassert == kPreventSleepType) && activesForTheType)
            _unclamp_silent_running(true);

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
    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );
}

static void enableIdleHandler(assertionType_t *assertType, assertionOps op)
{
}

static void displayWakeHandler(assertionType_t *assertType, assertionOps op)
{
    bool            activesForTheType = false;
    bool            activeExists;
    uint64_t        level = 0;
    bool            assertionLevel = 0;
    io_connect_t    connect = IO_OBJECT_NULL;

    if (assertType->kassert != kTicklessDisplayWakeType)
        return;

    assertionLevel = getAssertionLevel(assertType->kassert);
    activeExists = checkForActives(assertType, &activesForTheType);

    if (op == kAssertionOpRaise) {
        setClamshellSleepState();
        if ( !activeExists ) return;
        level = 1;
    }
    else if (op == kAssertionOpRelease) {
        setClamshellSleepState();
        if ( activeExists ) return;
        level = 0;
    }
    else { // op == kAssertionOpEval
        if (activeExists) {
            level = 1;
            if (assertionLevel)
                goto check_silentRunning;
        }
        else {
            if (!assertionLevel)
                return;
            level = 0;
        }
    }

    if ( (connect = getRootDomainConnect()) == IO_OBJECT_NULL)
        return;

    if (level) { 
        if ((isA_DarkWakeState() || isA_SleepState())) {
            set_NotificationDisplayWake( );
        }
    }
    else {
        cancel_NotificationDisplayWake( );
    }

    // updateAggregates after set_NotificationDisplayWake() to
    // avoid 'kIOPMUserPresentPassive' level when display is waking
    // to display notification
    updateAggregates(assertType, activesForTheType);
    IOConnectCallMethod(connect, kPMSetDisplayPowerOn, 
                        &level, 1, 
                        NULL, 0, NULL, 
                        NULL, NULL, NULL);

    if (gAggChange) notify_post( kIOPMAssertionsChangedNotifyString );

check_silentRunning:
    if (level && isInSilentRunningMode()) {
        __block bool userActive = false;
        applyToAssertionsSync(assertType, kSelectActive, ^(assertion_t *assertion){
                if (assertion->state & kAssertionExitSilentRunningMode) {
                    INFO_LOG("Assertion with id:%lld has ExitSilentRunning mode flag set. Exiting silent running mode.\n",
                         (((uint64_t)assertion->kassert) << 32) | (assertion->assertionId));
                userActive = true;
                return;
            }
        });
        if (userActive) {
            _unclamp_silent_running(true);
        }
    }
}

void enforceAssertionTypeTimeCap(assertionType_t *assertType)
{
    assertion_t *assertion = NULL;

#ifndef XCTEST
    if ((assertType->flags & kAssertionTypeGloballyTimed) == 0)
        return;
#endif
    assertType->forceTimedoutCnt = 0;
    /* Move all active assertions to inactive mode */
    while( (assertion = LIST_FIRST(&assertType->active)) )
    {
        removeActiveAssertion(assertion, assertType, true);
        insertInactiveAssertion(assertion, assertType);
        assertType->forceTimedoutCnt++;
        logAssertionEvent(kACapExpiryLog, assertion);
        mt2RecordAssertionEvent(kAssertionOpGlobalTimeout, assertion);
    }

    /* Timeout all timed assertions */
    while( (assertion = LIST_FIRST(&assertType->activeTimed)) )
    {
        LIST_REMOVE(assertion, link);
        assertion->state &= ~kAssertionStateTimed;

        updateAppStats(assertion, kAssertionOpRelease);
        schedEnableAppSleep( assertion );
        stopProcTimer(assertion);
        updateSystemQualifiers(assertion, kAssertionOpRelease);

        insertInactiveAssertion(assertion, assertType);
        assertType->forceTimedoutCnt++;
        logAssertionEvent(kACapExpiryLog, assertion);
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

    if ((assertType->flags & kAssertionTypeGloballyTimed) == 0)
        return;
    assertType->globalTimeout = timeout;
    if (assertType->globalTimeout == 0) {
        if ( assertType->globalTimer)
            dispatch_source_cancel(assertType->globalTimer);
        return;
    }

    if (assertType->globalTimer == NULL) {
        assertType->globalTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());

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
    uint64_t            currTime = getMonotonicTime();
    uint32_t            levelInt = 0;
    CFDateRef           start_date = NULL;
    CFStringRef         assertionTypeRef;
    CFNumberRef         numRef = NULL;
    CFNumberRef         levelNum = NULL;
    CFTimeInterval      timeout = 0;
    assertionType_t     *assertType;
    uint64_t            assertion_id_64;
    CFBooleanRef        val = NULL;
    CFNumberRef         pidCF = NULL;


    assertionTypeRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionTypeKey);

    /* Find index for this assertion type */
    idx = getAssertionTypeIndex(assertionTypeRef);

    if (idx < 0 )
        return kIOReturnBadArgument;

    assertType = &gAssertionTypes[idx];
    assertion->kassert = idx;

    assertion_id_64 = MAKE_UNIQAID(currTime, idx, assertion->assertionId);
    CFNumberRef uniqueAID = CFNumberCreate(0, kCFNumberSInt64Type, &assertion_id_64);

    if (uniqueAID) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionGlobalUniqueIDKey, uniqueAID);
        CFRelease(uniqueAID);
    }
    numRef = CFNumberCreate(0, kCFNumberSInt32Type, &assertion->assertionId);
    if (numRef) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionIdKey, numRef);
        CFRelease(numRef);
    }
    if (assertion->pinfo->name) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionProcessNameKey, assertion->pinfo->name);
    }
    pidCF = CFNumberCreate(0, kCFNumberIntType, &assertion->pinfo->pid);
    if (pidCF) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionPIDKey, pidCF);
        CFRelease(pidCF);
    }

    assertion->createTime = 0;
    if (CFDictionaryGetValueIfPresent(assertion->props, kIOPMAssertionCreateDateKey, (const void **)&start_date) &&
        isA_CFDate(start_date)) {
        CFTimeInterval delta = CFAbsoluteTimeGetCurrent() - CFDateGetAbsoluteTime(start_date);
        if (delta > 0) {
            assertion->createTime = currTime - delta;
        }
    }
    if (!assertion->createTime) {
        /* Attach the Create Time */
        start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        if (start_date) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionCreateDateKey, start_date);
            CFRelease(start_date);
        }
        assertion->createTime = currTime;
    }


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
        val = CFDictionaryGetValue(assertion->props, kIOPMAssertionAppliesToLimitedPowerKey);
        if (isA_CFBoolean(val) && (val == kCFBooleanTrue)) {
            assertion->state |= kAssertionStateValidOnBatt;
        }
    }

    if (assertion->kassert == kTicklessDisplayWakeType) {
        CFBooleanRef userActive = kCFBooleanFalse;
        if (CFDictionaryGetValueIfPresent(assertion->props, kIOPMAssertionExitSilentRunning, (const void **)&userActive)
                && (userActive == kCFBooleanTrue)) {
            assertion->state |= kAssertionExitSilentRunningMode;
        }
    }

    numRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionOnBehalfOfPID);
    if (isA_CFNumber(numRef)) {
        CFNumberGetValue(numRef, kCFNumberIntType, &assertion->causingPid);
    }

    val = CFDictionaryGetValue(assertion->props, kIOPMAssertionAppliesOnLidClose);
    if (isA_CFBoolean(val) && (val == kCFBooleanTrue)) {
        assertion->state |= kAssertionLidStateModifier;
    }

    /* Is this timed */
    numRef = CFDictionaryGetValue(assertion->props, kIOPMAssertionTimeoutKey);
    if (isA_CFNumber(numRef)) 
        CFNumberGetValue(numRef, kCFNumberDoubleType, &timeout);

    if (assertType->flags & kAssertionTypeAutoTimed) {
        /* Restrict timeout to a max value of 'autoTimeout' */
        if (!timeout || (timeout > assertType->autoTimeout))
            timeout = assertType->autoTimeout;
    }
    if (timeout) {
        assertion->timeout = (uint64_t)timeout+assertion->createTime; // Absolute time at which assertion expires
        insertTimedAssertion(assertion, assertType, true, true);
    }
    else {
        /* Insert into active assertion list */
        insertActiveAssertion(assertion, assertType, true);
    }


    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRaise);

    mt2RecordAssertionEvent(kAssertionOpRaise, assertion);

exit:
    return kIOReturnSuccess;

}


STATIC IOReturn doCreate(
                  pid_t                   pid,
                  CFMutableDictionaryRef  newProperties,
                  IOPMAssertionID         *assertion_id,
                  ProcessInfo             **procInfo,
                  int                     *enTrIntensity
                 ) 
{
    int                     i;
    assertion_t             *assertion = NULL;
    assertion_t             *tmp_a = NULL;
    IOReturn                result = kIOReturnSuccess;
    ProcessInfo             *pinfo = NULL;
    assertionType_t         *assertType = NULL;
    static uint32_t         gNextAssertionIdx = 0;

    // assertion_id will be set to kIOPMNullAssertionID on failure.
    *assertion_id = kIOPMNullAssertionID;

    // Create a dispatch handler for process exit, if there isn't one
    if ( !(pinfo = processInfoRetain(pid)) ) {
        pinfo = processInfoCreate(pid);
        if (!pinfo || pinfo->proc_exited) return kIOReturnNoMemory;

        // procInfo is only set for first call to doCreate by a pid
        if (procInfo) *procInfo = pinfo;
    }

    // Suspended pids are not allowed to manipulate assertions
    if (pinfo->isSuspended) {
        return kIOReturnInternalError;
    }

    // Generate an id
    for (i=gNextAssertionIdx; CFDictionaryGetValueIfPresent(gAssertionsArray, 
                                                            (const void *)(uintptr_t)i, (const void **)&tmp_a) == true; ) {
        i = (i+1) % kMaxAssertions;
        if (i == gNextAssertionIdx) break;
    }
    if (CFDictionaryGetValueIfPresent(gAssertionsArray, (const void *)(uintptr_t)i, (const void **)&tmp_a) == true) {
        processInfoRelease(pid);
        return kIOReturnNoMemory;
    }

    assertion = calloc(1, sizeof(assertion_t));
    if (assertion == NULL) {
        processInfoRelease(pid);
        return kIOReturnNoMemory;
    }
    assertion->props = newProperties;
    CFRetain(newProperties);
    assertion->retainCnt = 1;
    assertion->pinfo = pinfo;

    assertion->assertionId = ID_FROM_INDEX(i);
    CFDictionarySetValue(gAssertionsArray, (const void *)(uintptr_t)i, (const void *)assertion);
    gNextAssertionIdx = (i+1) % kMaxAssertions;

    result = raiseAssertion(assertion);

    if (result != kIOReturnSuccess) {
        processInfoRelease(pid);
        CFDictionaryRemoveValue(gAssertionsArray, (const void *)(uintptr_t)i);
        CFRelease(assertion->props);
        free(assertion);

        return result;
    }

    assertType = &gAssertionTypes[assertion->kassert];
    if (!(assertion->state & kAssertionStateInactive))
        logAssertionEvent(kACreateLog, assertion);
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

    *assertion_id = assertion->assertionId;
    if (enTrIntensity)
        *enTrIntensity = assertType->enTrQuality;

    return result;
}

static void copyAssertion(assertion_t *assertion, CFMutableDictionaryRef assertionsDict)
{
    bool                    created = false;
    CFNumberRef             pidCF = NULL;
    CFMutableDictionaryRef  processDict = NULL;
    CFMutableArrayRef       pidAssertionsArr = NULL;

    pidCF = CFNumberCreate(0, kCFNumberIntType, &assertion->pinfo->pid);

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

    if (assertion->kassert < kIOPMNumAssertionTypes) {
        CFDictionarySetValue(assertion->props, kIOPMAssertionTrueTypeKey, assertion_types_arr[assertion->kassert]);
    }

    CFArrayAppendValue(pidAssertionsArr, assertion->props);
    CFRelease(pidCF);

    if (created) {
        CFRelease(pidAssertionsArr);
        CFRelease(processDict);
    }
}

static CFArrayRef copyAssertionsByType(CFStringRef assertionType)
{
    __block CFMutableArrayRef       returnArray = NULL;
    assertionType_t         *assertType = NULL;
    int idx;

    idx = getAssertionTypeIndex(assertionType);
    if (idx == -1) {
        return NULL;
    }
    assertType = &gAssertionTypes[idx];
    applyToAssertionsSync(assertType, kSelectActive, ^(assertion_t *assertion)
                          {
                              if (returnArray == NULL) {
                                  returnArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
                              }
                              if (assertion->kassert < kIOPMNumAssertionTypes) {
                                  CFDictionarySetValue(assertion->props,
                                                       kIOPMAssertionTrueTypeKey,
                                                       assertion_types_arr[assertion->kassert]);
                              }
                              CFArrayAppendValue(returnArray, assertion->props);
                          });

    return returnArray;

}

STATIC CFArrayRef copyPIDAssertionDictionaryFlattened(int state)
{
    CFMutableDictionaryRef  assertionsDict = NULL;
    CFMutableArrayRef       returnArray = NULL;
    CFDictionaryRef         *assertionsDictArr = NULL;
    assertionType_t         *assertType = NULL;
    CFIndex                 i, count;



    assertionsDict = CFDictionaryCreateMutable(
                                               kCFAllocatorDefault, 0, 
                                               &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);

    /* Go thru each assertion type and copy assertion props */
    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        if (i == kEnableIdleType) continue;
        assertType = &gAssertionTypes[i]; 
        applyToAssertionsSync(assertType, kSelectAll, ^(assertion_t *assertion)
                              {
                                  if (((assertion->state & kAssertionStateInactive) && (state == kIOPMInactiveAssertions)) ||
                                      ((!(assertion->state & kAssertionStateInactive)) && (state == kIOPMActiveAssertions))) {
                                      copyAssertion(assertion, assertionsDict);
                                  }
                              });
    }

    count = CFDictionaryGetCount(assertionsDict);
    assertionsDictArr = (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef)*count); 
    if (!assertionsDictArr) {
        goto exit;
    }
    CFDictionaryGetKeysAndValues(assertionsDict,
                                 NULL, (const void **)assertionsDictArr);

    returnArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    for (i=0; i < count; i++) {
        CFArrayAppendValue(returnArray, assertionsDictArr[i]);
    }

exit:
    if (assertionsDictArr) {
        free(assertionsDictArr);
    }
    if (assertionsDict) {
        CFRelease(assertionsDict);
    }

    return returnArray;
}

STATIC IOReturn copyAssertionForID(
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

STATIC IOReturn doRetain(pid_t pid, IOPMAssertionID id, int *retainCnt)
{
    IOReturn        ret;
    assertion_t     *assertion = NULL;

    ret = lookupAssertion(pid, id, &assertion);

    if ((kIOReturnSuccess != ret)) {
        return ret;
    }

    assertion->retainCnt++;

    if (retainCnt)
        *retainCnt = assertion->retainCnt;

    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );

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
    LIST_HEAD(, assertion) list  = LIST_HEAD_INITIALIZER(list);  // local list to hold assertions for which timeout is changed

    if (gDisplaySleepTimer == (int)dispSlpTimer)
        return;

    changeInSecs = ((int)dispSlpTimer - gDisplaySleepTimer) * 60;
    gDisplaySleepTimer = dispSlpTimer;

    assertType = &gAssertionTypes[kDeclareUserActivityType];

    assertion = LIST_FIRST(&assertType->activeTimed);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        if (gDisplaySleepTimer) {
            LIST_REMOVE(assertion, link); // Remove from timed list

            if (assertion->timeout + changeInSecs < currTime)
                assertion->timeout = currTime;
            else
                assertion->timeout += changeInSecs;

            LIST_INSERT_HEAD(&list, assertion, link); // add to local list
        }
        else {
            removeTimedAssertion(assertion, assertType, false, true);
            assertion->timeout = 0;
            insertActiveAssertion(assertion, assertType, true);
        }
        assertion = nextAssertion;
    }

    // Walk thru local list and add them back to activeTimed list
    while( (assertion = LIST_FIRST(&list)) )
    {
        LIST_REMOVE(assertion, link);
        insertByTimeout(assertion, assertType);
    }

    assertion = LIST_FIRST(&assertType->active);
    while(assertion && gDisplaySleepTimer)
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        assertion->timeout = currTime + (gDisplaySleepTimer * 60); 

        removeActiveAssertion(assertion, assertType, true);
        insertTimedAssertion(assertion, assertType, false, true);
        assertion = nextAssertion;
    }
    updateAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);


    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
}


/* 
 * Called when Idle Sleep Timer setting is changed. The timeout value
 * for assertions of type kIOPMAssertNetworkClientActive created by
 * API _io_pm_declare_network_client_active() are  modified
 * to reflect the new idle Sleep timer value.
 */
__private_extern__ void evalAllNetworkAccessAssertions()
{

    assertionType_t     *assertType;
    unsigned long       changeInSecs;
    assertion_t         *assertion, *nextAssertion;
    uint64_t            currTime = getMonotonicTime();
    unsigned long       idleSleepTimer = gIdleSleepTimer;
    LIST_HEAD(, assertion) list  = LIST_HEAD_INITIALIZER(list);  // local list to hold assertions for which timeout is changed

    getIdleSleepTimer(&idleSleepTimer);
    changeInSecs = (idleSleepTimer - gIdleSleepTimer) * 60UL;
    gIdleSleepTimer = idleSleepTimer;

    if (changeInSecs == 0) return;

    assertType = &gAssertionTypes[kNetworkAccessType];

    assertion = LIST_FIRST(&assertType->activeTimed);
    while( assertion )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        if (gIdleSleepTimer) {
            LIST_REMOVE(assertion, link); // Remove from timed list

            if (assertion->timeout + changeInSecs < currTime)
                assertion->timeout = currTime;
            else
                assertion->timeout += changeInSecs;

            LIST_INSERT_HEAD(&list, assertion, link); // add to local list
        }
        else {
            removeTimedAssertion(assertion, assertType, false, true);
            assertion->timeout = 0;
            insertActiveAssertion(assertion, assertType, true);
        }
        assertion = nextAssertion;
    }

    // Walk thru local list and add them back to activeTimed list
    while( (assertion = LIST_FIRST(&list)) )
    {
        LIST_REMOVE(assertion, link);
        insertByTimeout(assertion, assertType);
    }

    assertion = LIST_FIRST(&assertType->active);
    while( assertion && gIdleSleepTimer )
    {
        nextAssertion = LIST_NEXT(assertion, link);
        if (!(assertion->state & kAssertionTimeoutIsSystemTimer)) {
            assertion = nextAssertion;
            continue;
        }

        assertion->timeout = currTime + (gIdleSleepTimer * 60); 

        removeActiveAssertion(assertion, assertType, true);
        insertTimedAssertion(assertion, assertType, false, true);
        assertion = nextAssertion;
    }
    updateAssertionTimer(assertType);

    if (assertType->handler)
        (*assertType->handler)(assertType, kAssertionOpRelease);

    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
}

__private_extern__ void evalAllInteractivePushAssertions()
{
    assertionType_t     *assertType;
    uint64_t            newTimeout;

    assertType = &gAssertionTypes[kInteractivePushServiceType];
    newTimeout = assertType->autoTimeout + getMonotonicTime();

    applyToAssertionsSync(assertType, kSelectActive, ^(assertion_t *assertion)
                          {
                              CFNumberRef  timeLeftCF = NULL;
                              CFDateRef    updateDate = NULL;

                              if (assertion->timeout > newTimeout) {
                                  assertion->timeout = newTimeout;

                                  timeLeftCF = CFNumberCreate(0, kCFNumberLongType, &assertType->autoTimeout);
                                  if (timeLeftCF) {
                                      CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutTimeLeftKey, timeLeftCF);
                                      CFRelease(timeLeftCF);
                                  }

                                  updateDate = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
                                  if (updateDate) {
                                      CFDictionarySetValue(assertion->props, kIOPMAssertionTimeoutUpdateTimeKey, updateDate);
                                      CFRelease(updateDate);
                                  }
                              }
                          });

    updateAssertionTimer(assertType);

    if (gTimeoutChange) notify_post( kIOPMAssertionTimedOutNotifyString );
    if (gAnyChange) notify_post( kIOPMAssertionsAnyChangedNotifyString );
}



static void   evaluateForPSChange(void)
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

    // re-configure assertions that change with power source 
    configAssertionType(kBackgroundTaskType, false);
    configAssertionType(kNetworkAccessType, false);
    configAssertionType(kInteractivePushServiceType, false);

    /* Timeout for Interactive push assertions changes with power source change */
    evalAllInteractivePushAssertions( );
    cancelPowerNapStates( );

    for (i=0; i < kIOPMNumAssertionTypes; i++)
    {
        assertType = &gAssertionTypes[i];
        applyToAssertionsSync(assertType, kSelectActive, ^(assertion_t *assertion)
        {
            if (pwrSrc == kBatteryPowered) {
                if ((!(assertion->state & kAssertionStateValidOnBatt)) && (assertType->flags & kAssertionTypeNotValidOnBatt)) {
                    updateAppStats(assertion, kAssertionOpRelease);
                }
                startProcTimer(assertion);
            }
            else if (pwrSrc != kBatteryPowered) {
                if (assertType->flags & kAssertionTypeNotValidOnBatt) {
                    updateAppStats(assertion, kAssertionOpRaise);
                }
                stopProcTimer(assertion);
            }
        });
    }
    if (gProcAggregateMonitor) {
        if (pwrSrc == kBatteryPowered) {
            dispatch_source_set_timer(gProcAggregateMonitor,
                    dispatch_time(DISPATCH_TIME_NOW, 0), gProcMonitorFrequency, 0);
        }
        else {
            // On external power source, clear any accumulated proc aggregate assertion data
            // and set the timer not to fire
            if (gProcAggregateBasis) {
                CFRelease(gProcAggregateBasis);
                gProcAggregateBasis = NULL;
            }
            dispatch_source_set_timer(gProcAggregateMonitor,
                    dispatch_time(DISPATCH_TIME_FOREVER, 0), DISPATCH_TIME_FOREVER, 0);
        }
    }
    logASLAssertionsAggregate();

}

__private_extern__ void setSleepServicesTimeCap(uint32_t  timeoutInMS)
{
    assertionType_t *assertType;

    assertType = &gAssertionTypes[kPushServiceTaskType];

    // Avoid duplicate resets to 0
    if ( (timeoutInMS == 0) && (assertType->globalTimeout == 0) )
        return;

    resetGlobalTimer(assertType, timeoutInMS/1000);
    if (timeoutInMS == 0) {
        dispatch_async(_getPMMainQueue(), ^{
            enforceAssertionTypeTimeCap(assertType);
        });
    }
}


static void configAssertionEffect(kerAssertionEffect idx)
{
    gAssertionEffects[idx].effectIdx = idx;
    LIST_INIT(&gAssertionEffects[idx].assertTypes);
}


__private_extern__ void configAssertionType(kerAssertionType idx, bool initialConfig)
{
    assertionHandler_f   oldHandler = NULL;
    CFNumberRef idxRef = NULL;
    uint32_t    oldFlags, flags;
    static bool prevBTdisable = false;
    kerAssertionType  altIdx;
    assertionType_t *assertType;
    kerAssertionEffect  prevEffect, newEffect;

    // This can get called before gUserAssertionTypesDict is initialized
    if ( !gUserAssertionTypesDict)
        return;

    oldFlags = 0;
    prevEffect = kNoEffect;
    assertType = &gAssertionTypes[idx];
    if (!initialConfig) {
        oldHandler = assertType->handler;
        oldFlags = assertType->flags;
        prevEffect = assertType->effectIdx;
    }

    assertType->kassert = idx;
    switch(idx) 
    {
    case kHighPerfType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNeedsCPU, idxRef);
        assertType->handler = modifySettings;
        assertType->enTrQuality = kEnTrQualNone;
        newEffect = kHighPerfEffect;
        break;

    case kPreventIdleType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventUserIdleSystemSleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNoIdleSleep, idxRef);
        assertType->handler = modifySettings;
        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake;

        newEffect = kPrevIdleSlpEffect;
        assertType->flags |= kAssertionTypePreventAppSleep;
        break;

    case kDisableInflowType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDisableInflow, idxRef);
        assertType->handler = handleBatteryAssertions;
        assertType->enTrQuality = kEnTrQualNone;
        newEffect = kDisableInflowEffect;
        break;

    case kInhibitChargeType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeInhibitCharging, idxRef);
        assertType->handler = handleBatteryAssertions;
        assertType->enTrQuality = kEnTrQualNone;
        newEffect = kInhibitChargeEffect;
        break;

    case kDisableWarningsType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDisableLowBatteryWarnings, idxRef);
        assertType->handler = handleBatteryAssertions;
        assertType->enTrQuality = kEnTrQualNone;
        newEffect = kDisableWarningsEffect;
        break;

    case kPreventDisplaySleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventUserIdleDisplaySleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeNoDisplaySleep, idxRef);
        assertType->handler = setKernelAssertions;
        assertType->enTrQuality = kEnTrQualSPKeepDisplayAwake;

        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        newEffect = kPrevDisplaySlpEffect;
        break;

    case kEnableIdleType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeEnableIdleSleep, idxRef);
        assertType->handler = enableIdleHandler;
        assertType->enTrQuality = kEnTrQualNone;
        newEffect = kEnableIdleEffect;
        break;

    case kPreventSleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypePreventSystemSleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeDenySystemSleep, idxRef);
        assertType->flags |= kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep 
            | kAssertionTypeLogOnCreate;
        assertType->handler = setKernelAssertions;
        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake | kEnTrQualSPPreventSleepSystem;

        newEffect = kPrevDemandSlpEffect;

        break;

    case kSRPreventSleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertInternalPreventSleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertMaintenanceActivity, idxRef);
        assertType->flags |= kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        assertType->handler = setKernelAssertions;
        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake | kEnTrQualSPPreventSleepSystem;


        newEffect = kPrevDemandSlpEffect;
        break;

    case kPreventDiskSleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertPreventDiskIdle, idxRef);
        assertType->handler = modifySettings;
        assertType->enTrQuality = kEnTrQualNone;
        newEffect = kPreventDiskSleepEffect;
        break;

    case kExternalMediaType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, _kIOPMAssertionTypeExternalMedia, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, _kIOPMAssertionTypePreventStandby, idxRef);
        assertType->handler = setKernelAssertions;
        assertType->enTrQuality = kEnTrQualNone;
        assertType->flags |= kAssertionTypeLogOnCreate;
        newEffect = kExternalMediaEffect;
        break;

    case kDeclareUserActivityType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionUserIsActive, idxRef);
        assertType->handler = setKernelAssertions;
        assertType->enTrQuality = kEnTrQualSPKeepDisplayAwake | kEnTrQualSPWakeDisplay;

        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        newEffect = kPrevDisplaySlpEffect;
        break;

    case kDeclareSystemActivityType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeSystemIsActive, idxRef);
        assertType->handler = modifySettings;
        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake;

        newEffect = kPrevIdleSlpEffect;
        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        break;

    case kPushServiceTaskType:
        if ( isA_SleepSrvcWake() && _SS_allowed() ) {
            idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
            CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeApplePushServiceTask, idxRef);
            newEffect = kPrevDemandSlpEffect;
        }
        else {
            /* Set this as an alias to BackgroundTask assertion for non-sleep srvc wakes */
            altIdx = kBackgroundTaskType;
            idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
            CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeApplePushServiceTask, idxRef);
            newEffect = kNoEffect;
        }
        assertType->flags |= kAssertionTypeGloballyTimed | kAssertionTypePreventAppSleep;
        assertType->handler = setKernelAssertions;

        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake;

        break;

    case kBackgroundTaskType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertionTypeBackgroundTask, idxRef);
        assertType->flags |= kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep;
        if (_DWBT_enabled()) {
            assertType->handler = setKernelAssertions;

            if ( !isA_BTMtnceWake() ) {
                if (!prevBTdisable) {
                    assertType->disableCnt++;
                    prevBTdisable = true;
                }
                assertType->flags &= ~kAssertionTypeLogOnCreate;
            }
            else {
                if (prevBTdisable)  {
                    if (assertType->disableCnt)
                        assertType->disableCnt--;
                    prevBTdisable = false;
                }
                assertType->flags &= ~kAssertionTypeLogOnCreate;
            }
            newEffect = kPrevDemandSlpEffect;
        }
        else  {
            assertType->handler = modifySettings;
            newEffect = kPrevIdleSlpEffect;
            if (prevBTdisable) {
                if (assertType->disableCnt) 
                    assertType->disableCnt--;
                prevBTdisable = false;
            }
        }
        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake;

        break;

    case kTicklessDisplayWakeType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertDisplayWake, idxRef);
        assertType->handler = displayWakeHandler;
        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        newEffect = kTicklessDisplayWakeEffect;
        assertType->entitlement = kIOPMDarkWakeControlEntitlement;
        assertType->enTrQuality = kEnTrQualNone;
        break;

    case kIntPreventDisplaySleepType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertInternalPreventDisplaySleep, idxRef);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertRequiresDisplayAudio, idxRef);
        assertType->handler = setKernelAssertions;
        assertType->flags |= kAssertionTypeLogOnCreate;
        assertType->enTrQuality = kEnTrQualSPKeepDisplayAwake;

        newEffect = kPrevDisplaySlpEffect;
        break;

    case kNetworkAccessType:
        idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertNetworkClientActive, idxRef);
        assertType->flags |= kAssertionTypePreventAppSleep | kAssertionTypeLogOnCreate;
        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake | kEnTrQualSPPreventSleepSystem;

        if (kACPowered == _getPowerSource()) {
            assertType->handler = setKernelAssertions;
            newEffect = kPrevDemandSlpEffect;
        }
        else {
            assertType->handler = modifySettings;
            newEffect = kPrevIdleSlpEffect;
        }
        break;

    case kInteractivePushServiceType:
        newEffect = kNoEffect;

        if (getTCPKeepAliveState(NULL, 0) == kActive) {
            /* If keep alives are allowed */
            idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);

            assertType->handler = setKernelAssertions;
            assertType->flags = kAssertionTypePreventAppSleep | kAssertionTypeAutoTimed;
            assertType->autoTimeout = getCurrentSleepServiceCapTimeout()/1000;

            newEffect = kPrevDemandSlpEffect;
        }
        else if ( isA_SleepSrvcWake() && _SS_allowed() ) {
            /* else if in a sleep service window, set this as an alias to ApplePushServiceTask  */

            altIdx = kPushServiceTaskType;
            idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
        }
        else {
            /* else make this behave same as BackgroundTask assertion when PowerNap is disabled */
            idxRef = CFNumberCreate(0, kCFNumberIntType, &idx);
            assertType->flags = kAssertionTypeNotValidOnBatt | kAssertionTypePreventAppSleep;
            assertType->flags |= kAssertionTypeAutoTimed;
            assertType->autoTimeout = getCurrentSleepServiceCapTimeout()/1000;
            assertType->handler = modifySettings;

            newEffect = kPrevIdleSlpEffect;

        }
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertInteractivePushServiceTask, idxRef);
        assertType->entitlement = kIOPMInteractivePushEntitlement;

        assertType->enTrQuality = kEnTrQualSPKeepSystemAwake;


        break;

    case kReservePwrPreventIdleType:
        altIdx = kPreventIdleType;
        idxRef = CFNumberCreate(0, kCFNumberIntType, &altIdx);
        CFDictionarySetValue(gUserAssertionTypesDict, kIOPMAssertAwakeReservePower, idxRef);
        assertType->flags |= kAssertionTypePreventAppSleep ;
        assertType->handler = modifySettings;
        newEffect = kPrevIdleSlpEffect;
        assertType->entitlement = kIOPMReservePwrCtrlEntitlement;
        assertType->enTrQuality = kEnTrQualSPPreventSleepSystem;

        break;


    default:
        return;
    }
    if (idxRef)
        CFRelease(idxRef);

    if (assertType->disableCnt) {
        newEffect = kNoEffect;
    }

    if (initialConfig) {
        assertType->effectIdx = newEffect;
        LIST_INSERT_HEAD(&gAssertionEffects[newEffect].assertTypes, assertType, link);
    }
    else if ((oldHandler != assertType->handler)  || (prevEffect != newEffect)){
        // Temporarily disable the assertion type and call the old handler.
        flags = assertType->flags;
        LIST_REMOVE(assertType, link);

        oldHandler(assertType, kAssertionOpEval);
        assertType->flags = flags;
        if (gActivityAggCnt && (prevEffect != newEffect)) {
            applyToAssertionsSync(assertType, kSelectActive, ^(assertion_t *assertion)
                                  {
                                      updateAppStats(assertion, kAssertionOpRelease);
                                  });
        }

        assertType->effectIdx = newEffect;

        LIST_INSERT_HEAD(&gAssertionEffects[newEffect].assertTypes, assertType, link);

        // Call the new handler
        if (newEffect != kNoEffect)
            assertType->handler(assertType, kAssertionOpEval);

        if (gActivityAggCnt && (prevEffect != newEffect)) {
            applyToAssertionsSync(assertType, kSelectActive, ^(assertion_t *assertion)
                                  {
                                      updateAppStats(assertion, kAssertionOpRaise);
                                  });
        }

    }
    else if (oldFlags != assertType->flags) {
        if (assertType->handler)
            assertType->handler(assertType, kAssertionOpEval);
    }

}

assertion_t * createKernelAssertion(CFDictionaryRef kAssertion)
{
    // Create an assertion_t struct with kernel assertion details
    assertion_t *assertion = NULL;
    CFNumberRef id = NULL;
    CFNumberRef asserted = NULL;
    CFStringRef owner_string = NULL;
    CFNumberRef owner_n = NULL;
    
    assertion = calloc(1, sizeof(assertion_t));
    if (assertion != NULL) {
        assertion->props = CFDictionaryCreateMutable(NULL, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!assertion->props) {
            // No memory
            return NULL;
        }

        // ID
        id = CFDictionaryGetValue(kAssertion, CFSTR(kIOPMDriverAssertionIDKey));
        if (id == NULL) {
            return NULL;
        }
        CFDictionarySetValue(assertion->props, kIOPMAssertionGlobalUniqueIDKey, id);

        // assertion name
        asserted = CFDictionaryGetValue(kAssertion, CFSTR(kIOPMDriverAssertionAssertedKey));
        if (asserted) {
            uint32_t n_asserted = 0;
            CFNumberGetValue(asserted, kCFNumberSInt32Type, &n_asserted);
            const char *assertionName = descriptiveKernelAssertions(n_asserted);
            CFStringRef name = CFStringCreateWithCString(0, assertionName, kCFStringEncodingUTF8);
            if (name) {
                CFDictionarySetValue(assertion->props, kIOPMAssertionNameKey, name);
                CFRelease(name);
            }
        }

        // assertion type
        CFDictionarySetValue(assertion->props, kIOPMAssertionTypeKey, CFSTR(kKernelAssertionType));

        // owner service key
        owner_n = CFDictionaryGetValue(kAssertion, CFSTR(kIOPMDriverAssertionOwnerServiceKey));
        if (owner_n) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionOnBehalfOfPID, owner_n);
        }
        
        // owner bundle ID
        owner_string = CFDictionaryGetValue(kAssertion, CFSTR(kIOPMDriverAssertionOwnerStringKey));
        if(owner_string) {
            CFDictionarySetValue(assertion->props, kIOPMAssertionOnBehalfOfBundleID, owner_string);
        }

        // process info - kernel
        if(!(assertion->pinfo = processInfoRetain(0))){
            assertion->pinfo = processInfoCreate(0);
        }

        // details to log
        int n_id;
        CFNumberGetValue(id, kCFNumberIntType, &n_id);
        char ownerBuf[100];
        CFStringRef ownerString = CFDictionaryGetValue(kAssertion, CFSTR(kIOPMDriverAssertionOwnerStringKey));
        if (ownerString) {
            CFStringGetCString(ownerString, ownerBuf, sizeof(ownerBuf), kCFStringEncodingUTF8);
        }
        INFO_LOG("inserted kernel assertion id %d %s \n",n_id, ownerBuf);
    }
    return assertion;
}

assertion_t* createSleepPreventerAssertion(CFStringRef kextName, CFNumberRef id, int preventerType)
{
    assertion_t *assertion = NULL;

    assertion = calloc(1, sizeof(assertion_t));
    if (assertion != NULL) {
        assertion->props = CFDictionaryCreateMutable(NULL, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!assertion->props) {
            ERROR_LOG("Unable to create assertion properties dictionary\n");
            free(assertion);
            return NULL;
        }
    } else {
        ERROR_LOG("Unable to calloc assertion struct\n");
        return NULL;
    }
    /*
     * Type - Kernel idle sleep preventer/kernel system sleep preventer
     * Name - kext name
     * pinfo - kernel
     */

    // ID
    CFDictionarySetValue(assertion->props, kIOPMAssertionGlobalUniqueIDKey, id);

    // Name
    CFDictionarySetValue(assertion->props, kIOPMAssertionNameKey, kextName);

    // Type
    CFDictionarySetValue(assertion->props, kIOPMAssertionTypeKey, ((preventerType == kIOPMIdleSleepPreventers) ? CFSTR(kKernelIdleSleepPreventer): CFSTR(kKernelSystemSleepPreventer)));

    // process info - kernel
    if(!(assertion->pinfo = processInfoRetain(0))){
        assertion->pinfo = processInfoCreate(0);
    }

    if (preventerType == kIOPMIdleSleepPreventers) {
        LIST_INSERT_HEAD(&gIdleSleepPreventersList, assertion, link);
    } else {
        LIST_INSERT_HEAD(&gSystemSleepPreventersList, assertion, link);
    }
    return assertion;

}

void logChangedSleepPreventers(int preventerType)
{
    int i = 0;
    CFStringRef new_kext = NULL;
    CFNumberRef old_id = NULL;
    CFNumberRef new_id = NULL;
    CFArrayRef preventers = NULL;
    CFIndex new_count = 0;
    CFDictionaryRef temp_preventer = NULL;
    IOReturn ret = IOPMCopySleepPreventersListWithID(preventerType, &preventers);
    if (ret != kIOReturnSuccess) {
        INFO_LOG("Could not read sleep preventers\n");
        return;
    }

    if (isA_CFArray(preventers)) {
        new_count = CFArrayGetCount(preventers);
    }

    assertion_t *first_a;

    // Get the right list based on preventer type
    if (preventerType == kIOPMIdleSleepPreventers) {
        first_a = LIST_FIRST(&gIdleSleepPreventersList);
    } else {
        first_a = LIST_FIRST(&gSystemSleepPreventersList);
    }

    assertion_t *temp_a = NULL;
    assertion_t *temp_b = NULL;

    int found = 0;
    for (i = 0; i < new_count; i++) {

        // check for new kexts preventing sleep
        found = 0 ;
        temp_a = first_a;
        temp_preventer = isA_CFDictionary(CFArrayGetValueAtIndex(preventers, i));
        new_id = CFDictionaryGetValue(temp_preventer, CFSTR(kIOPMDriverAssertionRegistryEntryIDKey));
        while(temp_a) {
            if (temp_a->props) {
                old_id = CFDictionaryGetValue(temp_a->props, kIOPMAssertionGlobalUniqueIDKey);
                if (CFNumberCompare(old_id, new_id, 0) == kCFCompareEqualTo) {
                    // still preventing sleep
                    found = 1;
                    break;
                }
            }
            temp_a = LIST_NEXT(temp_a, link);
        }
        if (found == 0) {
            // new sleep preventer
            // add this
            new_kext = CFDictionaryGetValue(temp_preventer, CFSTR(kIOPMDriverAssertionOwnerStringKey));
            assertion_t *new_a = createSleepPreventerAssertion(new_kext, new_id, preventerType);
            logAssertionEvent(kACreateLog, new_a);
        }
    }

    // check which kexts have been removed
    temp_a = first_a;
    temp_b = temp_a;
    while(temp_a) {
        if (temp_a->props) {
            old_id = CFDictionaryGetValue(temp_a->props, kIOPMAssertionGlobalUniqueIDKey);
            found = 0;
            for (i = 0; i < new_count; i++) {
                temp_preventer = isA_CFDictionary(CFArrayGetValueAtIndex(preventers, i));
                new_id = CFDictionaryGetValue(temp_preventer, CFSTR(kIOPMDriverAssertionRegistryEntryIDKey));
                if (CFNumberCompare(old_id, new_id, 0) == kCFCompareEqualTo) {
                    // still preventing sleep
                    found = 1;
                    break;
                }
            }
        }
        if (found == 0) {
            // remove from list
            temp_b = LIST_NEXT(temp_a, link);
            LIST_REMOVE(temp_a, link);
            logAssertionEvent(kAReleaseLog, temp_a);
            CFRelease(temp_a->props);
            processInfoRelease(0);
            free(temp_a);
            temp_a = temp_b;
        } else {
            temp_a = LIST_NEXT(temp_a, link);
        }
    }

    if (preventers) {
        CFRelease(preventers);
    }
}

void logChangedKernelAssertions(CFNumberRef driverAssertions, CFArrayRef kernelAssertionsArray)
{
    // check if kernel assertions have changed
    uint32_t driver_assertions = 0;
    CFNumberGetValue(driverAssertions, kCFNumberIntType, &driver_assertions);

    if (driver_assertions != gKernelAssertions) {
        INFO_LOG("Kernel driver assertions bit mask has changed old 0x%x new 0x%x\n", gKernelAssertions, driver_assertions);
        gKernelAssertions = driver_assertions;
    }

    CFIndex new_count;
    CFIndex old_count = 0;
    int i = 0;
    CFNumberRef *old_ids = NULL;

    // IDs of assertions from kernelAssertionsArray
    int *new_ids = NULL;
    int changed = 0;
    if (kernelAssertionsArray) {
        CFDictionaryRef k_assertion = NULL;
        CFNumberRef id = NULL;
        new_count = CFArrayGetCount(kernelAssertionsArray);
        new_ids = calloc(new_count, sizeof(int));

        // check for newly created assertions and released assertions in gKernelAssertions
        for (i = 0; i < new_count; i++) {
            k_assertion = isA_CFDictionary(CFArrayGetValueAtIndex(kernelAssertionsArray, i));
            if (!k_assertion) {
                continue;
            }
            id = CFDictionaryGetValue(k_assertion, CFSTR(kIOPMDriverAssertionIDKey));
            int n_id;
            CFNumberGetValue(id, kCFNumberIntType, &n_id);
            new_ids[i] = n_id;

            // get assertion level
            CFNumberRef assertion_level = CFDictionaryGetValue(k_assertion, CFSTR(kIOPMDriverAssertionLevelKey));
            uint32_t level = 0;
            CFNumberGetValue(assertion_level, kCFNumberSInt32Type, &level);
            if (CFDictionaryContainsKey(gKernelAssertionsArray, (const void *)id) == true) {
                if (level == kIOPMAssertionLevelOff) {

                    // If assertion is now turned off and is present in gKernelAssertions, release it
                    INFO_LOG("assertion %d is turned off. Let's remove it\n", n_id);
                    assertion_t *tempA = (assertion_t *)CFDictionaryGetValue(gKernelAssertionsArray, (const void *)id);
                    logAssertionEvent(kAReleaseLog, tempA);
                    CFDictionaryRemoveValue(gKernelAssertionsArray, (const void *)id);
                    CFRelease(tempA->props);
                    processInfoRelease(0);
                    free(tempA);
                    changed = 1;
                }
            } else {
                    if (level != kIOPMAssertionLevelOff) {

                        // New assertion
                        INFO_LOG("New kernel assertion %d\n", n_id);
                        changed = 1;
                        assertion_t *assertion = createKernelAssertion(k_assertion);
                        if (assertion) {
                            CFDictionarySetValue(gKernelAssertionsArray, (const void *)id, (const void *)assertion);
                            logAssertionEvent(kACreateLog, assertion);
                        }
                    }
            }
        }

        // Now check if any assertion present in gKernelAssertions is not in present in kernelAssertionsArray
        // If so, log a release for those assertions

        old_count = CFDictionaryGetCount(gKernelAssertionsArray);
        old_ids = calloc(old_count, sizeof(CFNumberRef));
        CFDictionaryGetKeysAndValues(gKernelAssertionsArray,(const void **)old_ids ,NULL);
        for (i = 0; i < old_count; i++){
           int released = 1;
           int id;
           CFNumberGetValue(old_ids[i], kCFNumberIntType, &id);
           for (int j = 0; j < new_count; j++) {
                if (new_ids[j] == id) {
                    released = 0;
                }
           }
           if (released == 1) {
               INFO_LOG("Kernel assertion %d has been released\n", id);
               assertion_t *tempA = (assertion_t *)CFDictionaryGetValue(gKernelAssertionsArray, (const void *)old_ids[i]);
               logAssertionEvent(kAReleaseLog, tempA);
               CFDictionaryRemoveValue(gKernelAssertionsArray, (const void *)old_ids[i]);
               CFRelease(tempA->props);
               processInfoRelease(0);
               free(tempA);
               changed = 1;
           }
        }
        free(old_ids);
        free(new_ids);
    }

    if (changed != 0 ) {
        INFO_LOG("Kernel assertions changed\n");
    }
}

__private_extern__ void logInitialKernelAssertions(CFNumberRef driverAssertions, CFArrayRef kernelAssertionsArray)
{
    CFNumberGetValue(driverAssertions, kCFNumberIntType, &gKernelAssertions);
    INFO_LOG("Kernel assertions 0x%u\n", gKernelAssertions);
    CFIndex count;
    int i = 0;
    if (kernelAssertionsArray) {
        CFDictionaryRef k_assertion = NULL;
        CFNumberRef id = NULL;
        count = CFArrayGetCount(kernelAssertionsArray);
        for (i = 0; i < count; i++) {
            k_assertion = isA_CFDictionary(CFArrayGetValueAtIndex(kernelAssertionsArray, i));
            if (!k_assertion) {
                continue;
            }
            id = CFDictionaryGetValue(k_assertion, CFSTR(kIOPMDriverAssertionIDKey));
            int n_id;
            CFNumberGetValue(id, kCFNumberIntType, &n_id);
            CFNumberRef assertion_level = CFDictionaryGetValue(k_assertion, CFSTR(kIOPMDriverAssertionLevelKey));
            uint32_t level = 0;
            CFNumberGetValue(assertion_level, kCFNumberSInt32Type, &level);
            if (level != kIOPMAssertionLevelOff) {
                assertion_t *assertion = createKernelAssertion(k_assertion);
                if (assertion) {
                    DEBUG_LOG("inserting kernel assertion %d\n", n_id);
                    CFDictionarySetValue(gKernelAssertionsArray, (const void *)id, (const void *)assertion);
                    logAssertionEvent(kACreateLog, assertion);
                }
            }
        }
    }
}

__private_extern__ void logKernelAssertions(CFNumberRef driverAssertions, CFArrayRef kernelAssertionsArray)
{
    if (!gKernelAssertionsArray) {
        gKernelAssertionsArray = CFDictionaryCreateMutable(NULL, kMaxAssertions, &kCFTypeDictionaryKeyCallBacks, NULL);

        // log initial assertions
        logInitialKernelAssertions(driverAssertions, kernelAssertionsArray);
    } else {
        logChangedKernelAssertions(driverAssertions, kernelAssertionsArray);
    }
}
__private_extern__ void PMAssertions_prime(void)
{

    kerAssertionType  idx = 0;
    kerAssertionEffect  effctIdx = 0;
    int token;

    assertions_log = os_log_create(PM_LOG_SYSTEM, ASSERTIONS_LOG);
    gAssertionsArray = CFDictionaryCreateMutable(NULL, kMaxAssertions, NULL, NULL); 
    gProcessDict = CFDictionaryCreateMutable(0, 0, NULL, NULL);

    gUserAssertionTypesDict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


    assertion_types_arr[kHighPerfType]             = kIOPMAssertionTypeNeedsCPU; 
    assertion_types_arr[kPreventIdleType]          = kIOPMAssertionTypePreventUserIdleSystemSleep;
    assertion_types_arr[kPreventSleepType]         = kIOPMAssertionTypePreventSystemSleep;
    assertion_types_arr[kDisableInflowType]        = kIOPMAssertionTypeDisableInflow; 
    assertion_types_arr[kInhibitChargeType]        = kIOPMAssertionTypeInhibitCharging;
    assertion_types_arr[kDisableWarningsType]      = kIOPMAssertionTypeDisableLowBatteryWarnings;
    assertion_types_arr[kPreventDisplaySleepType]  = kIOPMAssertionTypePreventUserIdleDisplaySleep;
    assertion_types_arr[kEnableIdleType]           = kIOPMAssertionTypeEnableIdleSleep;
    assertion_types_arr[kExternalMediaType]             = _kIOPMAssertionTypeExternalMedia;
    assertion_types_arr[kDeclareUserActivityType]       = kIOPMAssertionUserIsActive;
    assertion_types_arr[kPushServiceTaskType]      = kIOPMAssertionTypeApplePushServiceTask;
    assertion_types_arr[kBackgroundTaskType]       = kIOPMAssertionTypeBackgroundTask;
    assertion_types_arr[kDeclareSystemActivityType]     = kIOPMAssertionTypeSystemIsActive;
    assertion_types_arr[kSRPreventSleepType]       = kIOPMAssertInternalPreventSleep;
    assertion_types_arr[kTicklessDisplayWakeType]       = kIOPMAssertDisplayWake;
    assertion_types_arr[kPreventDiskSleepType]     = kIOPMAssertPreventDiskIdle;
    assertion_types_arr[kNetworkAccessType]        = kIOPMAssertNetworkClientActive;
    assertion_types_arr[kIntPreventDisplaySleepType] = kIOPMAssertInternalPreventDisplaySleep;
    assertion_types_arr[kInteractivePushServiceType] = kIOPMAssertInteractivePushServiceTask;
    assertion_types_arr[kReservePwrPreventIdleType]  = kIOPMAssertAwakeReservePower;

    for (effctIdx = 0; effctIdx < kMaxAssertionEffects; effctIdx++)
        configAssertionEffect(effctIdx);

    for (idx = 0; idx < kIOPMNumAssertionTypes; idx++)
        configAssertionType(idx, true);

    getDisplaySleepTimer(&gDisplaySleepTimer); 
    getIdleSleepTimer(&gIdleSleepTimer); 

    // Reset kernel assertions to clear out old values from prior to powerd's crash
    sendUserAssertionsToKernel(0);
    setClamshellSleepState();

    setAggregateLevel(kEnableIdleType, 1); /* Idle sleep is enabled by default */

    notify_register_dispatch("com.apple.notificationcenter.pushdnd", &token,
                             _getPMMainQueue(),
                             ^(int t) { 
                             configAssertionType(kInteractivePushServiceType, false); });
    notify_register_dispatch(kIOPMAssertionsCollectBTString, &token,
                             _getPMMainQueue(),
                             ^(int t) { /*Dummy registration to keep the key/value valid with notifyd */ });

    os_state_add_handler(_getPMMainQueue(), ^os_state_data_t(os_state_hints_t hints) {
            logASLAllAssertions(); return NULL; });
    return;
}



