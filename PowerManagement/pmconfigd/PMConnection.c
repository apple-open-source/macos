/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include "powermanagementServer.h" // mig generated

#include <asl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <notify.h>
#include <bsm/libbsm.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <libproc.h>

#include "PrivateLib.h"
#include "PMConnection.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "PMAssertions.h"
#include "PMStore.h"
#include "SystemLoad.h"
#if !TARGET_OS_EMBEDDED
#include "TTYKeepAwake.h"
#include "PMSettings.h"
#endif

/************************************************************************************/

// Bits for gPowerState
#define kSleepState              0x01          
#define kDarkWakeState           0x02
#define kDarkWakeForBTState      0x04
#define kDarkWakeForSSState      0x08
#define kDarkWakeForMntceState   0x10
#define kDarkWakeForServerState  0x20
#define kFullWakeState           0x40

#define kPowerStateMask  (kSleepState|kDarkWakeState|kDarkWakeForBTState| \
            kDarkWakeForSSState|kDarkWakeForMntceState|kDarkWakeForServerState|kFullWakeState)

#ifndef kIOPMMaintenanceScheduleImmediate
#define kIOPMMaintenanceScheduleImmediate               "MaintenanceImmediate"
#endif

/* Bookkeeping data types */

enum {
    kSleepWakeInterestBit   = 1,
    kMaintenanceInterestBit = 2
};

enum {
    _kSleepStateBits = 0x0000,
    _kOnStateBits = 0xFFFF
};

/* Array indices & for PMChooseScheduledEvent */
enum {
    kChooseFullWake         = 0,
    kChooseMaintenance      = 1,
    kChooseSleepServiceWake = 2,
    kChooseTimerPlugin      = 3,
    kChooseDWBTInterval     = 4,
    kChooseWakeTypeCount    = 5
};

enum {
    kSilentRunningOff = 0,
    kSilentRunningOn  = 1
};

static int const kMaxConnectionIDCount = 1000*1000*1000;
static int const kConnectionOffset = 1000;
static double const  kPMConnectionNotifyTimeoutDefault = 25.0;
#if !TARGET_OS_EMBEDDED
static int kPMSleepDurationForBT = (30*60); // Defaults to 30 mins
#if LOG_SLEEPSERVICES
static int                      gNotifySleepServiceToken = 0;
#endif
#endif
// gWakeForDWBTInterval - When set, a maintenance wake is scheduled
// when system is going to sleep from S0 (value is in seconds)
static int                      gWakeForDWBTInterval = 0;

// gPowerState - Tracks various wake types the system is currently in
// by setting appropriate bits. 
static uint32_t                 gPowerState;

static io_service_t             rootDomainService = IO_OBJECT_NULL;

extern CFMachPortRef            pmServerMachPort;

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

/* Bookkeeping structs */

/* PMResponseWrangler
 * While we have an outstanding notification, we have one of these guys sitting around
 *  waiting to handle the incoming responses.
 * We assert that only one PMResponseWrangler shall exist at a time - e.g. no more
 *  than one system state transition shall occur simultaneously.
 */
typedef struct {
    CFMutableArrayRef       awaitingResponses;
    CFRunLoopTimerRef       awaitingResponsesTimeout;
    CFAbsoluteTime          allRepliedTime;
    long                    kernelAcknowledgementID;
    int                     notificationType;
    int                     awaitingResponsesCount;
    int                     awaitResponsesTimeoutSeconds;
    int                     completedStatus;    // status after timed out or, all acked
    bool                    completed;
    bool                    nextIsValid;
    long                    nextKernelAcknowledgementID;
    int                     nextInterestBits;
} PMResponseWrangler;


/* PMConnection - one tracker corresponds to one PMConnection
 * in an application.
 *
 * responseHandler - Should be NULL unless this connection has outstanding
 *      notifications to reply to.
 */
typedef struct {
    mach_port_t             notifyPort;
    PMResponseWrangler      *responseHandler;
    CFStringRef             callerName;
    uint32_t                uniqueID;
    int                     callerPID;
    IOPMSystemPowerStateCapabilities    interestsBits;
    bool                    notifyEnable;
} PMConnection;


/* PMResponse 
 * represents one outstanding notification acknowledgement
 */
typedef struct {
    PMConnection            *connection;
    PMResponseWrangler      *myResponseWrangler;
    IOPMConnectionMessageToken  token;
    CFAbsoluteTime          repliedWhen;
    CFAbsoluteTime          notifiedWhen;
    CFAbsoluteTime          maintenanceRequested;
    CFAbsoluteTime          timerPluginRequested;
    CFAbsoluteTime          sleepServiceRequested;
    CFStringRef             clientInfoString;
    int                     sleepServiceCapTimeoutMS;
    int                     notificationType;
    bool                    replied;
    bool                    timedout;
} PMResponse;


/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

/* Internal methods */


#define VALID_DATE(x) (x!=0.0)
/* 
 * PMScheduleWakeEventChooseBest
 *
 * Expected to be called ONCE at each system sleep by PMConnection.c.
 * Compares maintenance, sleepservice, and autowake requests, and schedules the earliest event with the RTC.
 */
static IOReturn createConnectionWithID(
                    PMConnection **);

static PMConnection *connectionForID(
                    uint32_t findMe);

static CFArrayRef createArrayOfConnectionsWithInterest(
                    int interestBitsNotify);

static PMResponseWrangler *connectionFireNotification(
                    int notificationType,
                    long kernelAcknowledgementID);

static void _sendMachMessage(
                    mach_port_t port, 
                    mach_msg_id_t msg_id,
                    uint32_t payload_bits,
                    uint32_t payload_messagetoken);

static void checkResponses(PMResponseWrangler *wrangler);

static void     PMScheduleWakeEventChooseBest(CFAbsoluteTime *pick);

static void responsesTimedOut(CFRunLoopTimerRef timer, void * info);

static void cleanupConnection(PMConnection *reap);

static void cleanupResponseWrangler(PMResponseWrangler *reap);

static void setSystemSleepStateTracking(IOPMSystemPowerStateCapabilities);

#if !TARGET_OS_EMBEDDED
static void scheduleSleepServiceCapTimerEnforcer(uint32_t cap_ms);
#endif

/* Hide SleepServices code for public OS seeds. 
 * We plan to re-enable this code for shipment.
 * ETB 1/24/12
 */

#if LOG_SLEEPSERVICES
static void logASLMessageSleepServiceBegins(long withCapTime);
__private_extern__ void logASLMessageSleepServiceTerminated(int forcedTimeoutCnt);
#endif

static void PMConnectionPowerCallBack(
    void            *port,
    io_service_t    rootdomainservice,
    natural_t       messageType,
    void            *messageData);

__private_extern__ void ClockSleepWakeNotification(IOPMSystemPowerStateCapabilities b, 
                                                   IOPMSystemPowerStateCapabilities c);

bool smcSilentRunningSupport( );


/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

/* CFArrayRef support structures */

static Boolean _CFArrayConnectionEquality(const void *value1, const void *value2) 
{
    const PMConnection *v1 = (const PMConnection *)value1;
    const PMConnection *v2 = (const PMConnection *)value2;   
    return (v1->uniqueID == v2->uniqueID);
}
static CFArrayCallBacks _CFArrayConnectionCallBacks = 
                        { 0, NULL, NULL, NULL, _CFArrayConnectionEquality };
static CFArrayCallBacks _CFArrayVanillaCallBacks =
                        { 0, NULL, NULL, NULL, NULL };

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

/* Globals */

static CFMutableArrayRef        gConnections = NULL;

static uint32_t                 globalConnectionIDTally = 0;

static io_connect_t             gRootDomainConnect = IO_OBJECT_NULL;

static PMResponseWrangler *     gLastResponseWrangler = NULL;

SleepServiceStruct              gSleepService;

uint32_t                        gDebugFlags = 0;

uint32_t                        gCurrentSilentRunningState = kSilentRunningOff;

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

#define IS_DARK_CAPABILITIES(x) \
                    (BIT_IS_SET(x, kIOPMSystemCapabilityCPU) \
                    && BIT_IS_NOT_SET(x, kIOPMSystemCapabilityGraphics|kIOPMSystemCapabilityAudio))

#define SYSTEM_WILL_WAKE(x) \
                    ( BIT_IS_SET(capArgs->changeFlags, kIOPMSystemCapabilityWillChange) \
                    && IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityCPU) )

#define SYSTEM_DID_WAKE(x) \
                    ( BIT_IS_SET(capArgs->changeFlags, kIOPMSystemCapabilityDidChange)  \
                        && (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityCPU) \
                        || IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityGraphics) ) )

#define SYSTEM_WILL_SLEEP_TO_S0(x) \
                    ((CAPABILITY_BIT_CHANGED(x->fromCapabilities, x->toCapabilities, kIOPMSystemCapabilityCPU)) \
                    && BIT_IS_SET(x->changeFlags, kIOPMSystemCapabilityWillChange) \
                    && BIT_IS_NOT_SET(x->toCapabilities, kIOPMSystemCapabilityCPU))

#define SYSTEM_WILL_SLEEP_TO_S0DARK(x) \
                    ((CHANGED_CAP_BITS(x->fromCapabilities, x->toCapabilities) == (kIOPMSystemCapabilityGraphics|kIOPMSystemCapabilityAudio)) \
                    && BIT_IS_SET(x->changeFlags, kIOPMSystemCapabilityWillChange) \
                    && BIT_IS_NOT_SET(x->toCapabilities, kIOPMSystemCapabilityGraphics|kIOPMSystemCapabilityAudio))
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

/*
 * PMConnection_prime
 */
__private_extern__
void PMConnection_prime()
{
    io_object_t                 sleepWakeCallbackHandle = IO_OBJECT_NULL;
    IONotificationPortRef       notify = NULL;
    kern_return_t               kr = 0;
    char                        *errorString = NULL;
    
    bzero(&gSleepService, sizeof(gSleepService));

    gConnections = CFArrayCreateMutable(kCFAllocatorDefault, 100, &_CFArrayConnectionCallBacks);
                                        
    // Find it
    rootDomainService = getRootDomain();
    if (IO_OBJECT_NULL == rootDomainService) {
        errorString = "Could not find IOPMrootDomain";
        goto error;
    }

    // Open it
    kr = IOServiceOpen(rootDomainService, mach_task_self(), 0, &gRootDomainConnect);    
    if (KERN_SUCCESS != kr) {
        errorString = "Could not open IOPMrootDomain";
        goto error;    
    }

    notify = IONotificationPortCreate(MACH_PORT_NULL);
    if (!notify) {
        errorString = "Could not create IONotificationPort";
        goto error;
    }

    // Register for sleep wake notifications
    kr = IOServiceAddInterestNotification(notify, rootDomainService, "IOPMSystemCapabilityInterest",
                                          (IOServiceInterestCallback) PMConnectionPowerCallBack, NULL,
                                          &sleepWakeCallbackHandle);
    if (KERN_SUCCESS != kr) {
        errorString = "Could not add interest notification kIOPMAppPowerStateInterest";
        goto error;
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(),
            IONotificationPortGetRunLoopSource(notify),
            kCFRunLoopDefaultMode);

    return;

error:
    // ASL_LOG: KEEP
    asl_log(NULL, NULL, ASL_LEVEL_ERR,
                    "PowerManagement: unable to register with kernel power management. %s %s",
                    errorString ? "Reason = : ":"", errorString ? errorString:"unknown");
    return;
}    


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/* M I G */
/* M I G */
/* M I G */
/* M I G */
/* M I G */

#pragma mark -
#pragma mark MIG

/* 
 * Create a PM Connection
 *   - assign a globally unique ID
 *   - find the remote mach port
 *   - get an invalidation notification on the remote callback
 *
 */
kern_return_t _io_pm_connection_create
(
    mach_port_t server,
    mach_port_t task_in,
    string_t name,
    int interests,
    uint32_t *connection_id,
    int *return_code
)
{
    PMConnection         *newConnection = NULL;
    int                 task_pid;

    // Allocate: create a new PMConnection type
    createConnectionWithID(&newConnection);
    if (!newConnection) {
        *return_code = kIOReturnError;
        goto exit;
    }
    
    if (KERN_SUCCESS == pid_for_task(task_in, &task_pid)) {
        newConnection->callerPID = task_pid;
    }

    // Save caller name for logging
    if (name && strlen(name)) {
        newConnection->callerName = CFStringCreateWithCString(0, name, kCFStringEncodingUTF8);
    }

    // TODO: for now, all clients are listening for all notifications
    newConnection->interestsBits = 0xFF;

    *connection_id = newConnection->uniqueID;
    *return_code = kIOReturnSuccess;

exit:

    if (MACH_PORT_NULL != task_in)
    {
        // Release the send right on task_in that we received as an argument
        // to this method.
        __MACH_PORT_DEBUG(true, "_io_pm_connection_create dropping send right", task_in);
        mach_port_deallocate(mach_task_self(), task_in);
    }

    return KERN_SUCCESS;
}


/*****************************************************************************/
/*****************************************************************************/

 kern_return_t _io_pm_connection_schedule_notification(
     mach_port_t     server,
    uint32_t        connection_id,
    mach_port_t     notify_port_in,
    int             disable,
    int             *return_code)
{
    PMConnection         *connection = NULL;
    mach_port_t                 oldNotify;

    if (MACH_PORT_NULL == notify_port_in || NULL == return_code) {
        if (return_code) *return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    __MACH_PORT_DEBUG(true, "_io_pm_connection_schedule_notification notify_port", notify_port_in);
    
    *return_code = kIOReturnError;
    
    connection = connectionForID(connection_id);
    if (!connection) {
        return kIOReturnNotFound;
    }

    connection->notifyEnable = (disable == 0);

    if (!disable && (MACH_PORT_NULL == connection->notifyPort)) {
        connection->notifyPort = notify_port_in;

        mach_port_request_notification(
                    mach_task_self(),           // task
                    notify_port_in,                 // port that will die
                    MACH_NOTIFY_DEAD_NAME,      // msgid
                    1,                          // make-send count
                    CFMachPortGetPort(pmServerMachPort),        // notify port
                    MACH_MSG_TYPE_MAKE_SEND_ONCE,               // notifyPoly
                    &oldNotify);                                // previous
    
        __MACH_PORT_DEBUG(true, "Registered dead name notification on notifyPort", notify_port_in);
    } else {
        mach_port_deallocate(mach_task_self(), notify_port_in);
    }

    *return_code = kIOReturnSuccess;
exit:
    return KERN_SUCCESS;
}

/*****************************************************************************/
/*****************************************************************************/

kern_return_t _io_pm_connection_release
(
    mach_port_t server,
    uint32_t connection_id,
    int *return_code
)
{
    PMConnection        *cleanMeUp = NULL;

    cleanMeUp = connectionForID(connection_id);
    
    if (cleanMeUp) {
        cleanupConnection(cleanMeUp);
        if (return_code)
            *return_code = kIOReturnSuccess;
    } else {
        if (return_code)
            *return_code = kIOReturnNotFound;
    }
    
    return KERN_SUCCESS;
}

/*****************************************************************************/
/*****************************************************************************/

/* 
 * _io_pm_acknowledge_event_findOutstandingResponseForToken
 * Helper function to improve readability of connection_acknowledge_event
 */

static PMResponse *_io_pm_acknowledge_event_findOutstandingResponseForToken(PMConnection *connection, int token)
{
    CFMutableArrayRef   responsesTrackingList = NULL;
    int                 responsesCount = 0;
    PMResponse          *checkResponse = NULL;
    PMResponse          *foundResponse = NULL;
    int                 i;
    
    
    if (!connection
        || !connection->responseHandler
        || !(responsesTrackingList = connection->responseHandler->awaitingResponses)) 
    {
        return NULL;
    }
    
    responsesCount = CFArrayGetCount(responsesTrackingList);
    
    for (i=0; i<responsesCount; i++)
    {
        checkResponse = (PMResponse *)CFArrayGetValueAtIndex(responsesTrackingList, i);
        if (checkResponse && (token == checkResponse->token)) {
            foundResponse = checkResponse;
            break;
        }
    }
    
    return foundResponse;
}

/* 
 * _io_pm_connection_acknowledge_event_unpack_payload
 * Helper function to improve readability of connection_acknowledge_event
 */
static CFDictionaryRef _io_pm_connection_acknowledge_event_unpack_payload(
    vm_offset_t         ptr,
    mach_msg_type_number_t   len)
{
    CFDataRef           optionsAsData = NULL;
    CFPropertyListRef   optionsUnzipped = NULL;
    CFDictionaryRef     ackOptionsDict = NULL;
    
    if ( ptr != 0 && len != 0)
    {
        optionsAsData = CFDataCreateWithBytesNoCopy(0, (const uint8_t *)ptr, len, kCFAllocatorNull);
        if (optionsAsData)
        {
            optionsUnzipped = CFPropertyListCreateWithData(0, optionsAsData, kCFPropertyListImmutable, NULL, NULL);
            ackOptionsDict = (CFDictionaryRef) isA_CFDictionary(optionsUnzipped);
            CFRelease(optionsAsData);
        }
    }
    
    return ackOptionsDict;
}

kern_return_t _io_pm_connection_acknowledge_event
(
 mach_port_t server,
 uint32_t connection_id,
 int messageToken,
 vm_offset_t options_ptr,
 mach_msg_type_number_t options_len,
 int *return_code
 )
{
#if TARGET_OS_EMBEDDED
	return KERN_FAILURE;
#else
    PMConnection        *connection = connectionForID(connection_id);
    int                 timeIntervalMS;
    PMResponse          *foundResponse = NULL;
    
    // Check options dictionary
    CFDictionaryRef     ackOptionsDict = NULL;
    CFDateRef           requestDate = NULL;
    CFNumberRef         sleepServiceCapTimer = NULL;
    
    if (!(foundResponse = _io_pm_acknowledge_event_findOutstandingResponseForToken(connection, messageToken))) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    
    foundResponse->repliedWhen = CFAbsoluteTimeGetCurrent();
    foundResponse->replied = true;
    
    // Log if response time exceeds kAppResponseLogThresholdMS.
    timeIntervalMS = (int)((foundResponse->repliedWhen - foundResponse->notifiedWhen) * 1000);
    if (timeIntervalMS > kAppResponseLogThresholdMS) 
    {
        CFNumberRef timeIntervalNumber = CFNumberCreate(NULL, kCFNumberIntType, &timeIntervalMS);
        logASLMessageApplicationResponse(kAppResponseLogSourcePMConnection, connection->callerName,
                                         CFSTR(kIOPMStatsResponseSlow), timeIntervalNumber, foundResponse->notificationType);
        
        CFRelease(timeIntervalNumber);
    }
    else if (gDebugFlags & kIOPMDebugLogCallbacks)
    {
        logASLMessageApplicationResponse(kAppResponseLogSourcePMConnection, connection->callerName,
                                         CFSTR(kMsgTracerDomainAppResponse), 0, foundResponse->notificationType);
    }
    
    // Unpack the passed-in options data structure
    if ((ackOptionsDict = _io_pm_connection_acknowledge_event_unpack_payload(options_ptr, options_len)))
    {
        
        foundResponse->clientInfoString = CFDictionaryGetValue(ackOptionsDict, kIOPMAckClientInfoKey);
        if (foundResponse->clientInfoString) {
            CFRetain(foundResponse->clientInfoString);
        }
        
        /*
         * Caller requests a maintenance wake
         *
         * kIOPMAckWakeDate
         *      - Schedules on AC only
         * kIOPMAckNetworkMaintenanceWakeDate
         *      - Schedules on AC only
         */
        requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckWakeDate));
        if (!requestDate) {
            requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckNetworkMaintenanceWakeDate));
        }
                
        if (requestDate 
            && (kACPowered == _getPowerSource()))
        {
            foundResponse->maintenanceRequested = CFDateGetAbsoluteTime(requestDate);
        }
        
        /* kIOPMAckTimerPluginWakeDate
         *      - Timer plugin uses this to schedule DWBT timed work.
         *      - Schedules on AC only
         *      - Schedules on SilentRunning machines only
         */
        requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckTimerPluginWakeDate));
        
        if (requestDate && _DWBT_allowed()) 
        {
            foundResponse->timerPluginRequested = CFDateGetAbsoluteTime(requestDate);
            if (foundResponse->timerPluginRequested < (CFAbsoluteTimeGetCurrent() + kPMSleepDurationForBT) )
                foundResponse->timerPluginRequested = (CFAbsoluteTimeGetCurrent() + kPMSleepDurationForBT);
        }
        
        /* kIOPMAckSleepServiceDate
         *      - SleepServiceD uses this to schedule SleepService work.
         *      - Schedules on AC & Battery
         *      - Schedules on SilentRunning machines only
         */
        requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckSleepServiceDate));
        
        if (requestDate && _SS_allowed())
        {
            foundResponse->sleepServiceRequested = CFDateGetAbsoluteTime(requestDate);
        }
        
        /*
         * Caller specifies a maximum amount of time that the system should stay awake 
         * when it wakes for SleepServices.
         *       - SleepServiceD calls this. Nobody else.
         */
        sleepServiceCapTimer = isA_CFNumber(CFDictionaryGetValue(ackOptionsDict,
                                                                 kIOPMAcknowledgeOptionSleepServiceCapTimeout));
        if ( (sleepServiceCapTimer) && _SS_allowed() )
        {
            CFNumberGetValue(sleepServiceCapTimer, kCFNumberIntType, &foundResponse->sleepServiceCapTimeoutMS);
            scheduleSleepServiceCapTimerEnforcer(foundResponse->sleepServiceCapTimeoutMS);
        }
        
        CFRelease(ackOptionsDict);
    }
    
    checkResponses(connection->responseHandler);
    
    *return_code = kIOReturnSuccess;
    
exit:    
    vm_deallocate(mach_task_self(), options_ptr, options_len);
    
    return KERN_SUCCESS;
#endif
}

/*****************************************************************************/
/*****************************************************************************/

kern_return_t _io_pm_connection_copy_status
(
    mach_port_t server,
    int status_index,
    vm_offset_t *status_data,
    mach_msg_type_number_t *status_dataCnt,
    int *return_val
)
{
    return KERN_SUCCESS;
}

kern_return_t _io_pm_set_debug_flags(
        mach_port_t     server,
        audit_token_t   token,
        uint32_t    newFlags,
        uint32_t    *oldFlags,
        int         *return_code )
{
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
    if ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID) ))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (oldFlags) 
        *oldFlags = gDebugFlags;
    gDebugFlags = newFlags;

    *return_code = kIOReturnSuccess;

exit:


    return KERN_SUCCESS;
}

kern_return_t _io_pm_set_bt_wake_interval
(
    mach_port_t server,
    audit_token_t   token,
    uint32_t    newInterval,
    uint32_t    *oldInterval,
    int         *return_code
)
{
#if !TARGET_OS_EMBEDDED
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
    if ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID) ))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (oldInterval)
       *oldInterval = kPMSleepDurationForBT;
    kPMSleepDurationForBT = newInterval;
    if ( newInterval ) {
       /*
        * Backdoor to enable SR Capability to allow others
        * to play  with this Background tasks.
        */
       _Set_SR_override(kPMSROverrideEnable);
    }
    else {
       _Set_SR_override(kPMSROverrideDisable);
    }
#endif

    *return_code = kIOReturnSuccess;

exit:
    return KERN_SUCCESS;
}



/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

#pragma mark -
#pragma mark Connection

static void cleanupConnection(PMConnection *reap)
{
    PMResponseWrangler      *responseWrangler = NULL;
    PMResponse              *openResponse = NULL;
    int                     allResponsesCount = 0;
    int                     i;
    int                     index;
    CFRange                 connectionsRange =
                                CFRangeMake(0, CFArrayGetCount(gConnections));

    if (MACH_PORT_NULL != reap->notifyPort) 
    {
        // Release the send right on reap->notifyPort that we obtained 
        // when we received it as an argument to _io_pm_connection_schedule_notification.
        __MACH_PORT_DEBUG(true, "IOPMConnection cleanupConnection drop notifyPort", reap->notifyPort);
        mach_port_deallocate(mach_task_self(), reap->notifyPort);
        reap->notifyPort = MACH_PORT_NULL;
    }

    if (reap->callerName) {
        CFRelease(reap->callerName);
        reap->callerName = NULL;
    }

    responseWrangler = reap->responseHandler;
    if (responseWrangler && responseWrangler->awaitingResponses)
    {
        allResponsesCount = CFArrayGetCount(responseWrangler->awaitingResponses);

        for (i=0; i<allResponsesCount; i++)
        {
            openResponse = (PMResponse *)CFArrayGetValueAtIndex(    
                                    responseWrangler->awaitingResponses, i);

            if (openResponse && (openResponse->connection == reap)) {
                openResponse->connection    = NULL;
                openResponse->replied       = true;
                openResponse->timedout      = true;
                break;
            }
        }    

        // Let the response wrangler finish handling the outstanding event
        // now that we've zeroed out any of our pending responses for this dead client.
        checkResponses(responseWrangler);
    }
       
    // Remove our struct from gConnections
    index = CFArrayGetFirstIndexOfValue(gConnections, connectionsRange, (void *)reap);

    if (kCFNotFound != index) {
        CFArrayRemoveValueAtIndex(gConnections, index);
    }
    
    free(reap);

    return;
}

/*****************************************************************************/
/*****************************************************************************/

static void cleanupResponseWrangler(PMResponseWrangler *reap)
{
    PMConnection    *one_connection;
    long nextAcknowledgementID;
    int  responseCount;
    int  connectionsCount;
    int  i;
    int  nextInterestBits;
    bool nextIsValid;

    if (!reap) 
        return;
        
    if (!gConnections)
        return;

    // Cache the next response fields.
    nextInterestBits      = reap->nextInterestBits;
    nextAcknowledgementID = reap->nextKernelAcknowledgementID;
    nextIsValid           = reap->nextIsValid;

    // Loop through all connections. If any connections are referring to
    // responseWrangler in their tracking structs, zero that out.
    connectionsCount = CFArrayGetCount(gConnections);
    for (i=0; i<connectionsCount; i++) 
    {
        one_connection = (PMConnection *)CFArrayGetValueAtIndex(gConnections, i);
        if (reap == one_connection->responseHandler) 
        {
            // Zero out this reference before it points to a free'd pointer
            one_connection->responseHandler = NULL;
        }
    }
        

    // Loop responses, destroy responses
    if (reap->awaitingResponses)
    {
        responseCount = CFArrayGetCount(reap->awaitingResponses);
        if (responseCount > 0) 
        {
            for (i=0; i<responseCount; i++) 
            {
                PMResponse  *purgeMe = (PMResponse *)CFArrayGetValueAtIndex(reap->awaitingResponses, i);
                
                if (purgeMe->clientInfoString)
                    CFRelease(purgeMe->clientInfoString);
                
                free(purgeMe);
            }
        }
        CFRelease(reap->awaitingResponses);
        
        reap->awaitingResponses = NULL;
    }

    // Invalidate the pointer to the in-flight response wrangler.
    if (gLastResponseWrangler == reap) {
        gLastResponseWrangler = NULL;
    }

    free(reap);

    // Create a new response wrangler if the reaped wrangler has a queued
    // notification stored. If new wrangler is NULL, make sure to ack any
    // sleep message.
    if (nextIsValid)
    {
        PMResponseWrangler * resp;
        resp = connectionFireNotification(nextInterestBits, nextAcknowledgementID);
        if (!resp)
        {
            if (nextAcknowledgementID)
                IOAllowPowerChange(gRootDomainConnect, nextAcknowledgementID);
        }
    }
}

/*****************************************************************************/
/*****************************************************************************/

__private_extern__ bool PMConnectionHandleDeadName(mach_port_t deadPort)
{
    PMConnection    *one_connection = NULL;;
    PMConnection    *the_connection = NULL;
    int             connectionsCount = 0;
    int             i;
    
    if (!gConnections)
        return false;
    
    connectionsCount = CFArrayGetCount(gConnections);
    
    // Find the PMConnection that owns this mach port
    for (i=0; i<connectionsCount; i++) 
    {
        one_connection = (PMConnection *)CFArrayGetValueAtIndex(gConnections, i);
    
        if (one_connection && (deadPort == one_connection->notifyPort))
        {
            the_connection = one_connection;
            break;
        }
    }

    if (the_connection) {
        cleanupConnection(the_connection);
        return true;
    } else {
        return false;
    }
}

/*****************************************************************************/
/*****************************************************************************/

static void setSystemSleepStateTracking(IOPMSystemPowerStateCapabilities capables)
{
	SCDynamicStoreRef   store = _getSharedPMDynamicStore();
	CFStringRef			key = NULL;
	CFNumberRef			capablesNum = NULL;

	if (!store)
		return;

	key = SCDynamicStoreKeyCreate(0, CFSTR("%@%@"),
                        kSCDynamicStoreDomainState,
                        CFSTR(kIOPMSystemPowerCapabilitiesKeySuffix));
	if (!key)
		return;
		
	capablesNum = CFNumberCreate(0, kCFNumberIntType, &capables);

    if (capablesNum) {
        PMStoreSetValue(key, capablesNum);     
        CFRelease(capablesNum);
    }

    CFRelease(key);
}

#define IS_CAP_GAIN(c, f)       \
        ((((c)->fromCapabilities & (f)) == 0) && \
         (((c)->toCapabilities & (f)) != 0))

static bool PMConnectionPowerCallBack_HandleSleepForSSH(natural_t inMessageType, void *messageData)
{
    bool allow_sleep = true;
    
    if (kIOMessageCanSystemSleep == inMessageType)
    {
    #if !TARGET_OS_EMBEDDED
        // Re-examine TTY activeness right before system sleep
        allow_sleep = TTYKeepAwakeConsiderAssertion( );
    #endif        

        if (allow_sleep)
           IOAllowPowerChange(gRootDomainConnect, (long)messageData);                
        else
           IOCancelPowerChange(gRootDomainConnect, (long)messageData);                

        return true;
    }

    return false;
}
/*
static bool wakeReasonWas(CFStringRef c)
{
    bool                match = false;
    CFStringRef         wakeReasonProp = NULL;
    
    wakeReasonProp = IORegistryEntryCreateCFProperty(rootDomainService, CFSTR("Wake Reason"), 0, 0);
    if (wakeReasonProp) 
    {
        if (CFEqual(wakeReasonProp, c)) 
        {
            match = true;
        }
        CFRelease(wakeReasonProp);
    }
    return match;
}
*/

#pragma mark -
#pragma mark SleepServices

kern_return_t _io_pm_get_uuid(
    mach_port_t         server __unused,
    int                 selector,
    string_t            out_uuid,
    int                 *return_code)
{
    if (kIOPMSleepServicesUUID == selector)
    {
        if (gSleepService.uuid) 
        {
            if (CFStringGetCString(gSleepService.uuid, out_uuid, kPMMIGStringLength, kCFStringEncodingUTF8))
            {
                *return_code = kIOReturnSuccess;
                return KERN_SUCCESS;
            }
        }
    }
    out_uuid[0] = '\0';
    *return_code = kIOReturnNotFound;
    return KERN_SUCCESS;
}

kern_return_t _io_pm_set_sleepservice_wake_time_cap(
    mach_port_t         server __unused,
    audit_token_t       token,
    int                 cap_ms,
    int                 *return_code)
{
#if !TARGET_OS_EMBEDDED
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);    
    if ( !callerIsRoot(callerUID) )
    {
        *return_code = kIOReturnNotPrivileged;
        return KERN_SUCCESS;
    }

    if (gSleepService.capTime == 0 ||
          !isA_SleepSrvcWake() )
    {
        *return_code = kIOReturnError;
        return KERN_SUCCESS;
    }
    gSleepService.capTime = cap_ms;
   setSleepServicesTimeCap(cap_ms);
#endif
   *return_code = kIOReturnSuccess;
   return KERN_SUCCESS;
}

#if !TARGET_OS_EMBEDDED
static void scheduleSleepServiceCapTimerEnforcer(uint32_t cap_ms) 
{
    CFMutableDictionaryRef assertionDescription = NULL;
    
    gSleepService.capTime = (long)cap_ms;
    
    setSleepServicesTimeCap(cap_ms);
    /*
     * Publish a SleepService UUID
     */
    
    if (gSleepService.uuid) {
        CFRelease(gSleepService.uuid);
    }
    CFUUIDRef   ssuuid = NULL;
    ssuuid = CFUUIDCreate(0);
    if (ssuuid) {
        gSleepService.uuid = CFUUIDCreateString(0, ssuuid);
        CFRelease(ssuuid);
    }
    
    gPowerState |= kDarkWakeForSSState;
    configAssertionType(kPushServiceTaskIndex, false);
    assertionDescription = _IOPMAssertionDescriptionCreate(
                    kIOPMAssertionTypeApplePushServiceTask,
                    CFSTR("Powerd - Wait for client pushService assertions"), 
                    NULL, NULL, NULL,
                    10, kIOPMAssertionTimeoutActionRelease);

    InternalCreateAssertion(assertionDescription, NULL);

    CFRelease(assertionDescription);

    /*
     * Publish SleepService notify state under "com.apple.powermanagement.sleepservices"
     */

    if (!gSleepService.notifyToken) {
        int status;
        status = notify_register_check(kIOPMSleepServiceActiveNotifyName, &gSleepService.notifyToken);
        
        if (NOTIFY_STATUS_OK != status) {
            gSleepService.notifyToken = 0;            
        }
    }
    
    if (gSleepService.notifyToken) 
    {
        /* Interested clients will know that PM is in a SleepServices wake, as dictated by SleepServiceD */
        notify_set_state(gSleepService.notifyToken, kIOPMSleepServiceActiveNotifyBit);
        notify_post(kIOPMSleepServiceActiveNotifyName);
    }
    
    /*
     *  Announce to ASL & MessageTracer
     */
    logASLMessageSleepServiceBegins(gSleepService.capTime);


    if (!gNotifySleepServiceToken) {
        int status;
        status = notify_register_check(kIOPMSleepServiceActiveNotifyName, &gNotifySleepServiceToken);
        
        if (NOTIFY_STATUS_OK != status) {
            gNotifySleepServiceToken = 0;            
        }
    }
    
    if (gNotifySleepServiceToken) 
    {
        /* Interested clients will know that PM is in a SleepServices wake, as dictated by SleepServiceD */
        notify_set_state(gNotifySleepServiceToken, kIOPMSleepServiceActiveNotifyBit);
        notify_post(kIOPMSleepServiceActiveNotifyName);
    }
}
#endif


#if LOG_SLEEPSERVICES

/*****************************************************************************/
/* LOG ASL MESSAGE SLEEPSERVICE - Begins */
/*****************************************************************************/

static void logASLMessageSleepServiceBegins(long withCapTime)
{
    aslmsg      beginMsg;
    char        strbuf[125];
    
    beginMsg = asl_new(ASL_TYPE_MSG);
    
    asl_set(beginMsg, kMsgTracerDomainKey, kMsgTracerDomainSleepServiceStarted);
    
    /* com.apple.message.uuid = <SleepWake UUID>
     */
    if (_getUUIDString(strbuf, sizeof(strbuf))) {
        asl_set(beginMsg, kMsgTracerUUIDKey, strbuf);
    }
    
    /* com.apple.message.uuid2 = <SleepServices UUID>
     */
    if (gSleepService.uuid
        && CFStringGetCString(gSleepService.uuid, strbuf, sizeof(strbuf), kCFStringEncodingUTF8))
    {
        asl_set(beginMsg, kMsgTracerUUID2Key, strbuf);
    }
    
    snprintf(strbuf, sizeof(strbuf), "%ld", withCapTime);
    asl_set(beginMsg, kMsgTracerValueKey, strbuf);
    
    snprintf(strbuf, sizeof(strbuf), "SleepService: window begins with cap time=%ld secs", withCapTime/1000);
    asl_set(beginMsg, ASL_KEY_MSG, strbuf);
    
    //   If you set kPMASLMessageKey == kPMASLMessageLogValue, then "pmset -g log" will pick 
    //   this message up display.
    asl_set(beginMsg, kPMASLMessageKey, kPMASLMessageLogValue);
    asl_set(beginMsg, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    
    asl_set(beginMsg, ASL_KEY_FACILITY, "internal");
    asl_send(NULL, beginMsg);
    asl_free(beginMsg);
}

/*****************************************************************************/
/* LOG ASL MESSAGE SLEEPSERVICE - Terminated*/
/*****************************************************************************/

__private_extern__ void logASLMessageSleepServiceTerminated(int forcedTimeoutCnt)
{
    aslmsg      endMsg;
    char        strUUID[100];
    char        strUUID2[100];
    char        valStr[30];
    
    if ( (gPowerState & kDarkWakeForSSState) == 0)
       return;

    gPowerState &= ~kDarkWakeForSSState;
    configAssertionType(kPushServiceTaskIndex, false);

    endMsg = asl_new(ASL_TYPE_MSG);
    
    asl_set(endMsg, kMsgTracerDomainKey, kMsgTracerDomainSleepServiceTerminated);
    
    /* com.apple.message.uuid = <SleepWake UUID>
     */
    if (_getUUIDString(strUUID, sizeof(strUUID))) {
        asl_set(endMsg, kMsgTracerUUIDKey, strUUID);
    }
    
    /* com.apple.message.uuid2 = <SleepServices UUID>
     */
    if (gSleepService.uuid
        && CFStringGetCString(gSleepService.uuid, strUUID2, sizeof(strUUID2), kCFStringEncodingUTF8))
    {
        asl_set(endMsg, kMsgTracerUUID2Key, strUUID2);
    }
    
    /* value = # of clients whose assertions had to be timed out.
     */
    snprintf(valStr, sizeof(valStr), "%d", forcedTimeoutCnt);
    asl_set(endMsg, kMsgTracerValueKey, valStr);
        

    asl_set(endMsg, ASL_KEY_MSG, "SleepService: window has terminated.");

    /* Signature for how many clients timed out
     */    
    if (forcedTimeoutCnt == 0)
    {
        asl_set(endMsg, kMsgTracerSignatureKey, kMsgTracerSigSleepServiceExitClean);
    } else {
        asl_set(endMsg, kMsgTracerSignatureKey, kMsgTracerSigSleepServiceTimedOut);
    }
    
    //   If you set kPMASLMessageKey == kPMASLMessageLogValue, then "pmset -g log" will pick 
    //   this message up display.
    asl_set(endMsg, kPMASLMessageKey, kPMASLMessageLogValue);
    asl_set(endMsg, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    
    asl_set(endMsg, ASL_KEY_FACILITY, "internal");
    asl_send(NULL, endMsg);
    asl_free(endMsg);
    
    /* Messages describes the next state - S3, S0Dark, S0
     */
/*
    IOACPISystemPowerLevel minSystemPower;

    minSystemPower = getAssertionsMinimumSystemPowerLevel();

    if (kS0Dark == minSystemPower) {
        asl_set(endMsg, ASL_KEY_MSG, "SleepService window terminated. Elevated to DarkWake.");            
    } else 
    if (kS0Full == minSystemPower) {
        asl_set(endMsg, ASL_KEY_MSG, "SleepService window terminated. Elevated to FullWake.");    
    } else
    if (kS3 == minSystemPower) {
        asl_set(endMsg, ASL_KEY_MSG, "SleepService window terminated. Returning to S3/S4.");    
    } else {
        
        asl_set(endMsg, ASL_KEY_MSG, "SleepService window terminated. AssertionsMinimumSystemPowerLevel = Unknown.");
    }
*/
}
#endif

/*****************************************************************************/
/* Getters for System State */
/*****************************************************************************/
__private_extern__ bool isA_DarkWakeState()
{
   if (gPowerState & kDarkWakeState)
      return true;

   return false;

}
__private_extern__ bool isA_BTMtnceWake()
{

   if (gPowerState & kDarkWakeForBTState)
      return true;

   return false;
}

__private_extern__ void set_SleepSrvcWake()
{
   gPowerState |= kDarkWakeForSSState;
}
__private_extern__ bool isA_SleepSrvcWake()
{

   if (gPowerState & kDarkWakeForSSState)
      return true;

   return false;
}


/*****************************************************************************/
/* PMConnectionPowerCallBack */
/*****************************************************************************/

#pragma mark -
#pragma mark Sleep/Wake

static void PMConnectionPowerCallBack(
    void            *port,
    io_service_t    rootdomainservice,
    natural_t       inMessageType,
    void            *messageData)
{
    PMResponseWrangler                      *responseController = NULL;
    IOPMSystemPowerStateCapabilities        deliverCapabilityBits = 0;
    const struct        IOPMSystemCapabilityChangeParameters *capArgs;
#if !TARGET_OS_EMBEDDED
    static bool         scheduleWakeForBT = true;
    CFStringRef         wakeType = NULL;
#endif
    
    if (PMConnectionPowerCallBack_HandleSleepForSSH(inMessageType, messageData) ||
            (kIOMessageSystemCapabilityChange != inMessageType) ) 
    {
        return;
    }

    capArgs = (const struct IOPMSystemCapabilityChangeParameters *)messageData;

    AutoWakeCapabilitiesNotification(capArgs->fromCapabilities, capArgs->toCapabilities);
    ClockSleepWakeNotification(capArgs->fromCapabilities, capArgs->toCapabilities);

    if (IS_DARK_CAPABILITIES(capArgs->fromCapabilities)
        && !IS_DARK_CAPABILITIES(capArgs->toCapabilities))
    {
        /* MessageTracer only records assertion events that occurred during DarkWake. 
         * This cues MT2 to stop logging assertions.
         */
        mt2DarkWakeEnded();
    }
    
    if (SYSTEM_WILL_SLEEP_TO_S0(capArgs))
    {
        // Send an async notify out - clients include SCNetworkReachability API's; no ack expected
        setSystemSleepStateTracking(0);
        notify_post(kIOPMSystemPowerStateNotify);

#if !TARGET_OS_EMBEDDED
        if (scheduleWakeForBT) {
          enableAssertionType(kBackgroundTaskIndex);
          scheduleWakeForBT = false;
        }
#endif
        // Clear gPowerState and set it to kSleepState
        // We will clear kDarkWakeForSSState bit later when the SS session is closed.
        gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
        gPowerState |= kSleepState;

       if (isA_SleepSrvcWake()) {
           gSleepService.capTime = 0;
           setSleepServicesTimeCap(0);
       }

        /* We must acknowledge this sleep event within 30 second timeout, 
         *      once clients ack via IOPMConnectionAcknowledgeEvent().
         * Our processing will pick-up again in our handler
         *      _io_pm_connection_acknowledge_event
         */
        logASLMessageSleep(kMsgTracerSigSuccess, NULL, NULL, kIsS0Sleep);
        logASLMessageKernelApplicationResponses();

        responseController = connectionFireNotification(_kSleepStateBits, (long)capArgs->notifyRef);

        if (!responseController) {
            // We have zero clients. Acknowledge immediately.            

            PMScheduleWakeEventChooseBest(NULL);
            IOAllowPowerChange(gRootDomainConnect, (long)capArgs->notifyRef);                
        }


        return;
#if !TARGET_OS_EMBEDDED
    } else if (SYSTEM_WILL_SLEEP_TO_S0DARK(capArgs))
    {

       if (_DWBT_allowed() && checkForActivesByType(kBackgroundTaskIndex)) {
          disableAssertionType(kBackgroundTaskIndex);
          scheduleWakeForBT = true;
          gWakeForDWBTInterval = kPMSleepDurationForBT;
       }
       /* 
        * This notification is issued before every sleep. Log to asl only if
        * there are assertion that can prevent system from going into S0
        */
       if (systemBlockedInS0Dark()) {
          logASLMessageSleep(kMsgTracerSigSuccess, NULL, NULL, kIsDarkWake);
       }

        // Clear gPowerState and set it to kDarkWakeState
        // We will clear kDarkWakeForSSState bit later when the SS session is closed.
       gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
       gPowerState |= kDarkWakeState;
       if (isA_SleepSrvcWake()) {
           setSleepServicesTimeCap(0);
           gSleepService.capTime = 0;
       }
#endif
    } else if (SYSTEM_DID_WAKE(capArgs))
    {
       // On a SilentRunningMachine, the assumption is that every wake is
       // Silent until powerd unclamps SilentRunning or unforeseen thermal
       // constraints arise
       if(smcSilentRunningSupport() )
           gCurrentSilentRunningState = kSilentRunningOn;

       //If system is sleep services wake, cancel the sleep services
       if (isA_SleepSrvcWake()) {
           setSleepServicesTimeCap(0);
           gSleepService.capTime = 0;
       }
        if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityGraphics))
        {
            // e.g. System has powered into full wake
            // Unclamp SilentRunning ASAP
            _unclamp_silent_running();

            deliverCapabilityBits =
                    kIOPMSystemPowerStateCapabilityCPU
                    | kIOPMSystemPowerStateCapabilityDisk 
                    | kIOPMSystemPowerStateCapabilityNetwork
                    | kIOPMSystemPowerStateCapabilityAudio 
                    | kIOPMSystemPowerStateCapabilityVideo;

            if (BIT_IS_SET(capArgs->fromCapabilities, kIOPMSystemCapabilityCPU)) {
               logASLMessageWake(kMsgTracerSigSuccess, NULL,  NULL, kIsDarkToFullWake);
            } else {
               logASLMessageWake(kMsgTracerSigSuccess, NULL,  NULL, kIsFullWake);
                mt2RecordWakeEvent(kWakeStateFull);
            }

            // kDarkWakeForSSState bit is removed when the SS session is closed.
            gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
            gPowerState |= kFullWakeState;
        } else if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityCPU))
        {
            // e.g. System has moved into dark wake
            deliverCapabilityBits = 
                    kIOPMSystemPowerStateCapabilityCPU
                    | kIOPMSystemPowerStateCapabilityDisk 
                    | kIOPMSystemPowerStateCapabilityNetwork;
            
            logASLMessageWake(kMsgTracerSigSuccess, NULL,  NULL, kIsDarkWake);
            mt2RecordWakeEvent(kWakeStateDark);

            // kDarkWakeForSSState bit is removed when the SS session is closed.
            gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
            gPowerState |= kDarkWakeState;
            
            _ProxyAssertions(capArgs);
#if !TARGET_OS_EMBEDDED

           wakeType = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeTypeKey));
           if (isA_CFString(wakeType) && CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) && 
                        ( _getPowerSource() != kBatteryPowered)) {
              gWakeForDWBTInterval = 0;
              gPowerState |= kDarkWakeForMntceState|kDarkWakeForBTState;

              /* Log all background task assertions once again */
              CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, 
                         ^{ logASLAssertionTypeSummary(kBackgroundTaskIndex); });
              CFRunLoopWakeUp(_getPMRunLoop());
           }
           if (wakeType) CFRelease(wakeType);
#endif
        } 
        // Send an async notify out - clients include SCNetworkReachability API's; no ack expected
        setSystemSleepStateTracking(deliverCapabilityBits);

        notify_post(kIOPMSystemPowerStateNotify);

        responseController = connectionFireNotification(deliverCapabilityBits, (long)capArgs->notifyRef);
    
        /* We must acknowledge this sleep event within 30 second timeout, 
         *      once clients ack via IOPMConnectionAcknowledgeEvent().
         * Our processing will pick-up again in our handler
         *      _io_pm_connection_acknowledge_event
         */
        if (!responseController) {
            // We have zero clients. Acknowledge immediately.            
            IOAllowPowerChange(gRootDomainConnect, (long)capArgs->notifyRef);                
        }
#if !TARGET_OS_EMBEDDED
        SystemLoadSystemPowerStateHasChanged( );
#endif
        return;
    }

    if (capArgs->notifyRef)
        IOAllowPowerChange(gRootDomainConnect, capArgs->notifyRef);

}

/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/
/************************************************************************************/

#pragma mark -
#pragma mark Responses

static PMResponseWrangler *connectionFireNotification(
    int interestBitsNotify,
    long kernelAcknowledgementID)
{
    static int              lastInterestBits = 0xFFFFFFFF;
    int                     affectedBits = 0;
    CFArrayRef              interested = NULL;
    PMConnection            *connection = NULL;
    int                     interestedCount = 0;
    uint32_t                messageToken = 0;
    uint16_t                calloutCount = 0;
    
    PMResponseWrangler      *responseWrangler = NULL;
    PMResponse              *awaitThis = NULL;

    /*
     * If a response wrangler is active, store the new notification on the
     * active wrangler. Then wait for its completion before firing the new
     * notification. Only the last notification is stored.
     */
    if (gLastResponseWrangler)
    {
        gLastResponseWrangler->nextIsValid = true;
        gLastResponseWrangler->nextInterestBits = interestBitsNotify;
        gLastResponseWrangler->nextKernelAcknowledgementID = kernelAcknowledgementID;
        return gLastResponseWrangler;
    }

    // We only send state change notifications out to entities interested in the changing
    // bits, or interested in a subset of the changing bits.
    // Any client who is interested in a superset of the changing bits shall not receive
    // a notification.
    //      affectedBits & InterestedBits != 0
    //  and affectedBits & InterestedBits == InterestedBits

    affectedBits = interestBitsNotify ^ lastInterestBits;

    lastInterestBits = interestBitsNotify;

    interested = createArrayOfConnectionsWithInterest(affectedBits);
    if (!interested) {
        goto exit;
    }
    interestedCount = CFArrayGetCount(interested);
    if (0 == interestedCount) {
        goto exit;
    }

    /* Allocate the response wrangler
     *
     * This object will be allocated & valid for the duration of 
     *   sending out notifications & awaiting responses.
     */
    responseWrangler = calloc(1, sizeof(PMResponseWrangler));
    if (!responseWrangler) {
        goto exit;
    }
    responseWrangler->notificationType = interestBitsNotify;
    responseWrangler->awaitResponsesTimeoutSeconds = (int)kPMConnectionNotifyTimeoutDefault;
    responseWrangler->kernelAcknowledgementID = kernelAcknowledgementID;

    
    /*
     * We will track each notification we're sending out with an individual response.
     * Record that response in the "active response array" so we can group them
     * all later when they acknowledge, or fail to acknowledge.
     */
    responseWrangler->awaitingResponses = 
                    CFArrayCreateMutable(kCFAllocatorDefault, interestedCount, &_CFArrayVanillaCallBacks);
    responseWrangler->awaitingResponsesCount = interestedCount;

    for (calloutCount=0; calloutCount<interestedCount; calloutCount++) 
    {
        connection = (PMConnection *)CFArrayGetValueAtIndex(interested, calloutCount);
    
        if ((MACH_PORT_NULL == connection->notifyPort) ||
            (false == connection->notifyEnable)) {
            continue;
        }
        
        /* We generate a messagetoken here, which the notifiee must pass 
         * back into us when the client acknowledges. 
         * We note the token in the PMResponse struct.
         */
        messageToken = (interestBitsNotify << 16)
                            | calloutCount;

        // Mark this connection with the responseWrangler that's awaiting its responses
        connection->responseHandler = responseWrangler;

        _sendMachMessage(connection->notifyPort, 
                            0,
                            interestBitsNotify, 
                            messageToken);

        /* 
         * Track the response!
         */
        awaitThis = calloc(1, sizeof(PMResponse));
        if (!awaitThis) {
            goto exit;
        }

        awaitThis->token = messageToken;
        awaitThis->connection = connection;
        awaitThis->notificationType = interestBitsNotify;
        awaitThis->myResponseWrangler = responseWrangler;
        awaitThis->notifiedWhen = CFAbsoluteTimeGetCurrent();

        CFArrayAppendValue(responseWrangler->awaitingResponses, awaitThis);

        if (gDebugFlags & kIOPMDebugLogCallbacks)
           logASLMessageAppNotify(awaitThis->connection->callerName, interestBitsNotify );
         
    }

    // TODO: Set off a timer to fire in xx30xx seconds 
    CFRunLoopTimerContext   responseTimerContext = 
        { 0, (void *)responseWrangler, NULL, NULL, NULL };
    responseWrangler->awaitingResponsesTimeout = 
            CFRunLoopTimerCreate(0, 
                    CFAbsoluteTimeGetCurrent() + responseWrangler->awaitResponsesTimeoutSeconds, 
                    0.0, 0, 0, responsesTimedOut, &responseTimerContext);

    if (responseWrangler->awaitingResponsesTimeout)
    {
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), 
                            responseWrangler->awaitingResponsesTimeout, 
                            kCFRunLoopDefaultMode);
                            
        CFRelease(responseWrangler->awaitingResponsesTimeout);
    }

exit:
    if (interested) 
        CFRelease(interested);

    // Record the active wrangler in a global, then clear when reaped.
    if (responseWrangler)
        gLastResponseWrangler = responseWrangler;

    return responseWrangler;
}

/*****************************************************************************/
/*****************************************************************************/

static void responsesTimedOut(CFRunLoopTimerRef timer, void * info)
{
    PMResponseWrangler  *responseWrangler = (PMResponseWrangler *)info;
    PMResponse          *one_response = NULL;

    int             responsesCount = 0;
    int             i;
    int             tardyCount = 0;

    if (!responseWrangler)
        return;

    // Mark the timer as NULL since it's just fired and autoreleased.
    responseWrangler->awaitingResponsesTimeout = NULL;

    // Iterate list of awaiting responses, and tattle on anyone who hasn't 
    // acknowledged yet.
    // Artificially mark them as "replied", with their reason being "timed out"
    responsesCount = CFArrayGetCount(responseWrangler->awaitingResponses);
    for (i=0; i<responsesCount; i++)
    {
        one_response = (PMResponse *)CFArrayGetValueAtIndex(
                                        responseWrangler->awaitingResponses, i);
        if (!one_response)
            continue;
        if (one_response->replied)
            continue;

        // Caught a tardy reply
        tardyCount++;
        one_response->replied = true;
        one_response->timedout = true;
        one_response->repliedWhen = CFAbsoluteTimeGetCurrent();
        
        int timeIntervalMS = (int)((one_response->repliedWhen - one_response->notifiedWhen) * 1000);
        CFNumberRef timeIntervalNumber = CFNumberCreate(NULL, kCFNumberIntType, &timeIntervalMS);

        logASLMessageApplicationResponse(
            kAppResponseLogSourcePMConnection, 
            one_response->connection->callerName, 
            CFSTR(kIOPMStatsResponseTimedOut), 
            timeIntervalNumber,
            one_response->notificationType);

        if (timeIntervalNumber)
            CFRelease(timeIntervalNumber);
    }

    checkResponses(responseWrangler);
}


/*****************************************************************************/
/*****************************************************************************/

#define kMsgPayloadCount    2

typedef struct {
    mach_msg_header_t   header;
    mach_msg_body_t     body;
    uint32_t            payload[kMsgPayloadCount];
} IOPMMessageStructure;
 
static void _sendMachMessage(
    mach_port_t         port, 
    mach_msg_id_t       msg_id,
    uint32_t            payload_bits,
    uint32_t            payload_messagetoken)
{
    kern_return_t        status;
    IOPMMessageStructure msg;
    
    bzero(&msg, sizeof(msg));
    
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_id = msg_id;
    
    msg.body.msgh_descriptor_count = 0;
    
    msg.payload[0] = payload_bits;
    msg.payload[1] = payload_messagetoken;

    status = mach_msg(&msg.header,                  /* msg */
              MACH_SEND_MSG | MACH_SEND_TIMEOUT,    /* options */
              msg.header.msgh_size,                 /* send_size */
              0,                                    /* rcv_size */
              MACH_PORT_NULL,                       /* rcv_name */
              MACH_MSG_TIMEOUT_NONE,                /* timeout */
              MACH_PORT_NULL);                      /* notify */

    if (status == MACH_SEND_TIMED_OUT) {
        mach_msg_destroy(&msg.header);
    }
    
    if (status != MACH_MSG_SUCCESS)
    {
        // Pray for the lost message.
    }

    return;
}

static void describeWakeEvent(
    PMConnection            *inConnection,
    CFMutableStringRef      allWakeEvents,
    CFStringRef             describeType, 
    CFAbsoluteTime          requestedTime,
    CFStringRef             clientInfoString)
{
    CFStringRef             descriptionString = NULL;
    char                    name[32];
    pid_t                   use_pid = 0;
    
    if (!allWakeEvents)
        return;                    
    
    use_pid = inConnection ? inConnection->callerPID : 0;
    if (0 == use_pid) {
        use_pid = getpid();
    }
    
    if ((0 == use_pid) 
        || !proc_name(use_pid, name, sizeof(name))) {
        name[0] = '\0';
    }
    
    descriptionString = CFStringCreateWithFormat(0, 0, CFSTR("[proc=%s request=%@ inDelta=%.0f%@%@] "),
                        name, describeType,
                        (requestedTime - CFAbsoluteTimeGetCurrent()),
                        clientInfoString ? CFSTR(" info="):CFSTR(""),
                        clientInfoString ? clientInfoString:CFSTR(""));
    if (descriptionString) {
        CFStringAppend(allWakeEvents, descriptionString);
        CFRelease(descriptionString);
    }
}

#pragma mark -
#pragma mark CheckResponses

static bool checkResponses_ScheduleWakeEvents(PMResponseWrangler *wrangler)
{
    int                     i = 0;
    int                     responsesCount          = 0;
    bool                    complete                = true;
    PMResponse              *oneResponse            = NULL;
    CFMutableStringRef      allWakeEventsString     = NULL;
    CFAbsoluteTime          pick[kChooseWakeTypeCount];

    bzero(pick, sizeof(pick));
    
    if (PMDebugEnabled(kLogWakeEvents)) {
        allWakeEventsString = CFStringCreateMutable(0, 0);
    }
    
    if (_DWBT_allowed() && gWakeForDWBTInterval) {
        describeWakeEvent(NULL, 
                          allWakeEventsString, CFSTR("BTIntervalWaitToDarkWake"), 
                          gWakeForDWBTInterval + CFAbsoluteTimeGetCurrent(), NULL);
    }
    responsesCount = CFArrayGetCount(wrangler->awaitingResponses);    
    
    for (i=0; i<responsesCount; i++)
    {
        oneResponse = (PMResponse *)CFArrayGetValueAtIndex(wrangler->awaitingResponses, i);
        
        if (!oneResponse->replied) {
            complete = false;
            break;
        }
        
        if (VALID_DATE(oneResponse->maintenanceRequested)) 
        {
            describeWakeEvent(oneResponse->connection, 
                              allWakeEventsString, CFSTR("Maintenance"), 
                              oneResponse->maintenanceRequested, 
                              oneResponse->clientInfoString);

        }
        if (VALID_DATE(oneResponse->sleepServiceRequested)) 
        {
            describeWakeEvent(oneResponse->connection, 
                              allWakeEventsString, CFSTR("SleepService"), 
                              oneResponse->sleepServiceRequested,
                              oneResponse->clientInfoString);
        }
        if (VALID_DATE(oneResponse->timerPluginRequested))
        {
            describeWakeEvent(oneResponse->connection, 
                              allWakeEventsString, CFSTR("TimerPlugin"), 
                              oneResponse->timerPluginRequested,
                              oneResponse->clientInfoString);
        }
        
        if (!VALID_DATE(pick[kChooseMaintenance])
            || (VALID_DATE(oneResponse->maintenanceRequested) && (oneResponse->maintenanceRequested < pick[kChooseMaintenance])))
        {
            pick[kChooseMaintenance] = oneResponse->maintenanceRequested;
        }
        
        if (!VALID_DATE(pick[kChooseSleepServiceWake])
            || (VALID_DATE(oneResponse->sleepServiceRequested) && (oneResponse->sleepServiceRequested < pick[kChooseSleepServiceWake])))
        {
            pick[kChooseSleepServiceWake] = oneResponse->sleepServiceRequested;
        }

        if (!VALID_DATE(pick[kChooseTimerPlugin])
            || (VALID_DATE(oneResponse->timerPluginRequested) && (oneResponse->timerPluginRequested < pick[kChooseTimerPlugin])))
        {
            pick[kChooseTimerPlugin] = oneResponse->timerPluginRequested;
        }
    }
    
    if (!complete) {
        // Some clients have not responded yet.
        // We await more responses, or more deaths, or a timeout.
        goto exit;
    }

    if (!BIT_IS_SET(wrangler->notificationType, kIOPMSystemCapabilityCPU)) 
    {
        // Only schedule wakeup events if we're going to sleep.

        logASLMessagePMConnectionScheduledWakeEvents(allWakeEventsString);        
        
        PMScheduleWakeEventChooseBest(pick);
    } 
    
exit:
    if (allWakeEventsString){
        CFRelease(allWakeEventsString);
    }

    return complete;
}

static void checkResponses(PMResponseWrangler *wrangler)
{
    if (!checkResponses_ScheduleWakeEvents(wrangler)) {
        // Not all clients acknowledged.
        return;
    }

    // Completion: all clients have acknowledged.
    if (wrangler->awaitingResponsesTimeout) {
        CFRunLoopTimerInvalidate(wrangler->awaitingResponsesTimeout);
        wrangler->awaitingResponsesTimeout = NULL;
    }
    
    // Handle PowerManagement acknowledgements
    if (wrangler->kernelAcknowledgementID) 
    {
        IOAllowPowerChange(gRootDomainConnect, wrangler->kernelAcknowledgementID);
    }
    
    cleanupResponseWrangler(wrangler);
    
    return;
}
            
            
/* PMScheduleWakeEventChooseBest
* Handles the decision-making leading up to system sleep time, about which
* wake event we want to schedule with the RTC.
*
* The system might have several clients requesting a Maintenance wake...
* Or a SleepServices wake...
* And it might have a few IOPMSchedulePowerEvent() requests to power the system
* over an RTC wake.
*
* This code identifies the first upcoming event, among those options, and
* schedules it with the RTC.
*/

#define IS_EARLIEST_EVENT(x, y, z) (VALID_DATE(x) && ((y==0.0)||(x<y)) && ((z==0.0)||(x<z)))

static bool pickEarliestEvent(CFAbsoluteTime *inArray, int *outIndex, CFAbsoluteTime *outTime)
{
    int                 i = 0;
    CFAbsoluteTime      lowest = kCFAbsoluteTimeIntervalSince1904;

    if (outIndex) *outIndex = 0;
    if (outTime) *outTime = 0.0;
    
    if (!inArray || !outIndex || !outTime)
        return false;
    
    for (i=0; i<kChooseWakeTypeCount; i++) 
    {
        if ((0.0 != inArray[i]) && (inArray[i] < lowest)) {
            *outIndex = i;
            *outTime = lowest = inArray[i];
        }
    }
    
    return (0.0 != *outTime);
}
            
            
__private_extern__ void PMScheduleWakeEventChooseBest(CFAbsoluteTime *pickWakeEvent)
{
    CFStringRef     scheduleWakeType                    = NULL;
    CFAbsoluteTime  scheduleTime                        = 0.0;
    CFAbsoluteTime  dummy[kChooseWakeTypeCount];
    int             idx;
    
    if (!pickWakeEvent) {
        bzero(dummy, sizeof(dummy));
        pickWakeEvent = dummy;
    }
    
    pickWakeEvent[kChooseFullWake]          = getEarliestRequestAutoWake();
    if (_DWBT_allowed() && gWakeForDWBTInterval) {
        pickWakeEvent[kChooseDWBTInterval]      = CFAbsoluteTimeGetCurrent() + gWakeForDWBTInterval;
    }
 
    if (!pickEarliestEvent(pickWakeEvent, &idx, &scheduleTime))
    {
        // Nothing to schedule. bail.
        return;
    }
    
    // INVARIANT: At least one of the WakeTimes we're evaluating has a valid date
    if ((kChooseMaintenance == idx) || (kChooseDWBTInterval == idx) 
            || (kChooseTimerPlugin == idx))
    {
        scheduleWakeType = CFSTR(kIOPMMaintenanceScheduleImmediate);
    } else if (kChooseFullWake == idx)
    {
        scheduleWakeType = CFSTR(kIOPMAutoWakeScheduleImmediate);
    } else  if (kChooseSleepServiceWake == idx)
    {
        scheduleWakeType = CFSTR(kIOPMSleepServiceScheduleImmediate);
    }

    if (VALID_DATE(scheduleTime) && scheduleWakeType)
    {
        CFDateRef theChosenDate = NULL;

        if ((theChosenDate = CFDateCreate(0, scheduleTime)))
        {
            /* Tell the RTC when PM wants to be woken up */
            IOPMSchedulePowerEvent(theChosenDate, NULL, scheduleWakeType);            
            CFRelease(theChosenDate);
            
        }
        
        if (PMDebugEnabled(kLogWakeEvents)) 
        {
            /* Record to pmset -g log the event that we chose */
            CFMutableStringRef      finalPublish = CFStringCreateMutable(0, 0);
            if (finalPublish)
            {
                CFStringAppend(finalPublish, scheduleWakeType);
                CFStringAppendFormat(finalPublish, NULL, CFSTR(" inDelta=%.2lf"), (scheduleTime - CFAbsoluteTimeGetCurrent()));
                logASLMessageExecutedWakeupEvent(finalPublish);
                CFRelease(finalPublish);
            }
        }
    }

    return ;
}
            
            
/*****************************************************************************/
/*****************************************************************************/

static CFArrayRef createArrayOfConnectionsWithInterest(
    int interestBits)
{
    CFMutableArrayRef       arrayFoundInterests = NULL;
    PMConnection            *lookee;
    int                     gConnectionsCount;
    int                     i;
    
    
    if (0 == interestBits)
        return NULL;

    // *********        
    
    arrayFoundInterests = CFArrayCreateMutable(kCFAllocatorDefault, 0, &_CFArrayConnectionCallBacks);

    gConnectionsCount = CFArrayGetCount(gConnections);

    for (i=0; i<gConnectionsCount; i++)
    {
        lookee = (PMConnection *)CFArrayGetValueAtIndex(gConnections, i);

        // Matching interest in this connection
        if (interestBits & lookee->interestsBits) {
            CFArrayAppendValue(arrayFoundInterests, lookee);
        }
    }
    
    return arrayFoundInterests;
    
}

/*****************************************************************************/
/*****************************************************************************/

static IOReturn createConnectionWithID(PMConnection **out)
{
    static bool     hasLoggedTooManyConnections = false;

    if ((globalConnectionIDTally > kMaxConnectionIDCount)
        && !hasLoggedTooManyConnections) 
    {
    // ASL_LOG: KEEP FOR NOW
//        asl_log(NULL, NULL, ASL_LEVEL_ERR, "PM configd connections: connection count exceeded %d",
//                                                    kMaxConnectionIDCount);
        return kIOReturnNoSpace;
    }

    *out = (PMConnection *)calloc(1, sizeof(PMConnection));
    
    if (!*out)
        return kIOReturnNoMemory;
    
    ((PMConnection *)*out)->uniqueID = kConnectionOffset + globalConnectionIDTally++;

    // Add new connection to the global tracking array
    CFArrayAppendValue(gConnections, *out);
    
    return kIOReturnSuccess;
}

/*****************************************************************************/
/*****************************************************************************/

static PMConnection *connectionForID(uint32_t findMe)
{
    CFIndex     where = 0;
    CFRange     theRange = CFRangeMake(0, CFArrayGetCount(gConnections));
    PMConnection     dummy;
    
    // Our gConnections CFArray equality callback only checks the "uniqueID" field
    // of the passed in PMConnection type. Searching for the value "dummy"
    // will return the first instance with connection ID "findMe".

    dummy.uniqueID = findMe;
    where = CFArrayGetFirstIndexOfValue(gConnections, theRange, &dummy);

    if (kCFNotFound == where) {
        return NULL;
    } else {
        return (PMConnection *)CFArrayGetValueAtIndex(gConnections, where);
    }
}

// Unclamps machine from SilentRunning if the machine is currently clamped.
// Does so by going through rootDomain's setProperties implementation.
// RootDomain informs all interested drivers (including AppleSMC) that
// SilentRunning has been turned off
__private_extern__ IOReturn _unclamp_silent_running(void)
{
    // Nothing to do. It's already unclamped
    if(gCurrentSilentRunningState == kSilentRunningOff)
        return kIOReturnSuccess;

    // Nothing to do. SMC doesn't support SR
    if(!smcSilentRunningSupport())
        return kIOReturnUnsupported;

    int newSRCap = kSilentRunningOff;
    CFNumberRef num = CFNumberCreate(0, kCFNumberIntType, &newSRCap);
    if (num)
    {
        if(rootDomainService)
        {
            IORegistryEntrySetCFProperty(rootDomainService,
                                         CFSTR(kIOPMSilentRunningKey),
                                         num);
            gCurrentSilentRunningState = kSilentRunningOff;
        }
        else
        {
            CFRelease(num);
            return kIOReturnInternalError;
        }
        CFRelease(num);
        return kIOReturnSuccess;
    }
    else
    {
        return kIOReturnInternalError;
    }
}
