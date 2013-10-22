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
#include <sys/syscall.h>
#include <Kernel/kern/debug.h>

#include "PrivateLib.h"
#include "PMConnection.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "PMAssertions.h"
#include "PMStore.h"
#include "SystemLoad.h"
#if !TARGET_OS_EMBEDDED
#include "TTYKeepAwake.h"
#endif
#include "PMSettings.h"
//#include "Platform.h"

/************************************************************************************/

// Bits for gPowerState
#define kSleepState                     0x01          
#define kDarkWakeState                  0x02
#define kDarkWakeForBTState             0x04
#define kDarkWakeForSSState             0x08
#define kDarkWakeForMntceState          0x10
#define kDarkWakeForServerState         0x20
#define kFullWakeState                  0x40
#define kNotificationDisplayWakeState   0x80
#define kPowerStateMask                 0xff

#ifndef kIOPMMaintenanceScheduleImmediate
#define kIOPMMaintenanceScheduleImmediate               "MaintenanceImmediate"
#endif

// Number of seconds before auto power off timer we let system go to sleep
#define kAutoPowerOffSleepAhead     (0)

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
    kChooseWakeTypeCount    = 4
};

enum {
    kSilentRunningOff = 0,
    kSilentRunningOn  = 1
};

/* Auto Poweroff info */
static dispatch_source_t   gApoDispatch;
CFAbsoluteTime              ts_apo = 0; // Time at which system should go for auto power off

static int const kMaxConnectionIDCount = 1000*1000*1000;
static int const kConnectionOffset = 1000;
static double const  kPMConnectionNotifyTimeoutDefault = 28.0;
#if !TARGET_OS_EMBEDDED
static int kPMSleepDurationForBT = (30*60); // Defaults to 30 mins
static int kPMDarkWakeLingerDuration = 15; // Defaults to 15 secs
static int kPMACWakeLingerDuration = 45; // Defaults to 15 secs
#if LOG_SLEEPSERVICES
static int                      gNotifySleepServiceToken = 0;
#endif
// Time at which system can wake for next PowerNap
static CFAbsoluteTime      ts_nextPowerNap = 0;
#endif


// gPowerState - Tracks various wake types the system is currently in
// by setting appropriate bits. 
static uint32_t                 gPowerState;

static io_service_t             rootDomainService = IO_OBJECT_NULL;
static IOPMCapabilityBits       gCurrentCapabilityBits = kIOPMCapabilityCPU | kIOPMCapabilityDisk 
                                    | kIOPMCapabilityNetwork | kIOPMCapabilityAudio | kIOPMCapabilityVideo;
extern CFMachPortRef            pmServerMachPort;


static void  *stackshotBuf = NULL;
static uint32_t   stackshotOffset = 0;
static uint32_t   stackshotSize = 0;
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
    IOPMCapabilityBits      interestsBits;
    bool                    notifyEnable;
    int                     timeoutCnt;
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

static void setSystemSleepStateTracking(IOPMCapabilityBits);
static void saveAppResponseStackshots(); 

#if !TARGET_OS_EMBEDDED
static void scheduleSleepServiceCapTimerEnforcer(uint32_t cap_ms);
#endif

/* Hide SleepServices code for public OS seeds. 
 * We plan to re-enable this code for shipment.
 * ETB 1/24/12
 */

#if LOG_SLEEPSERVICES
#if !TARGET_OS_EMBEDDED
static void logASLMessageSleepServiceBegins(long withCapTime);
#endif
__private_extern__ void logASLMessageSleepServiceTerminated(int forcedTimeoutCnt);
#endif

static void PMConnectionPowerCallBack(
    void            *port,
    io_service_t    rootdomainservice,
    natural_t       messageType,
    void            *messageData);

__private_extern__ void ClockSleepWakeNotification(IOPMCapabilityBits b, 
                                                   IOPMCapabilityBits c,
                                                   uint32_t changeFlags);


void setAutoPowerOffTimer(bool initialCall, CFAbsoluteTime postpone);
static void sendEarlyNotification( int interestBitsNotify );
void cancelAutoPowerOffTimer();

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

#if !TARGET_OS_EMBEDDED
static bool                     gForceDWL = false;
#endif

bool                            gMachineStateRevertible = true;

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

    newConnection->interestsBits = interests;
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

#if !TARGET_OS_EMBEDDED
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
            if (optionsUnzipped && !ackOptionsDict) {
                CFRelease(optionsUnzipped);
            }
            CFRelease(optionsAsData);
        }
    }
    
    return ackOptionsDict;
}
#endif

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

    if (messageToken & (kIOPMEarlyWakeNotification << 16) )
    {
       /* 
        * This response is for a early wake notification. Just ignore the response.
        * There is no record mainatained for these early notification messages.
        */
       if (gDebugFlags & kIOPMDebugLogCallbacks)
       {
           logASLMessagePMConnectionResponse(kAppResponseLogSourcePMConnection, NULL,
                                         CFSTR(kPMASLDomainAppResponse), 0, messageToken >> 16);
       }
       *return_code = kIOReturnSuccess;
       return KERN_SUCCESS;
    }
    
    if (!(foundResponse = _io_pm_acknowledge_event_findOutstandingResponseForToken(connection, messageToken))) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    
    *return_code = kIOReturnSuccess;
    foundResponse->repliedWhen = CFAbsoluteTimeGetCurrent();
    foundResponse->replied = true;
    
    // Log if response time exceeds kAppResponseLogThresholdMS.
    timeIntervalMS = (int)((foundResponse->repliedWhen - foundResponse->notifiedWhen) * 1000);
    if (timeIntervalMS > kAppResponseLogThresholdMS) 
    {
        CFNumberRef timeIntervalNumber = CFNumberCreate(NULL, kCFNumberIntType, &timeIntervalMS);
        logASLMessagePMConnectionResponse(kAppResponseLogSourcePMConnection, connection->callerName,
                                         CFSTR(kIOPMStatsResponseSlow), timeIntervalNumber, foundResponse->notificationType);
        
        CFRelease(timeIntervalNumber);
    }
    else if (gDebugFlags & kIOPMDebugLogCallbacks)
    {
        logASLMessagePMConnectionResponse(kAppResponseLogSourcePMConnection, connection->callerName,
                                         CFSTR(kPMASLDomainAppResponse), 0, foundResponse->notificationType);
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
#if !TARGET_OS_EMBEDDED
            if ( foundResponse->timerPluginRequested < ts_nextPowerNap )
                foundResponse->timerPluginRequested = ts_nextPowerNap;
#endif
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
#if !TARGET_OS_EMBEDDED
            if ( foundResponse->sleepServiceRequested < ts_nextPowerNap )
                foundResponse->sleepServiceRequested = ts_nextPowerNap;
#endif
        }
        
        CFRelease(ackOptionsDict);
    }
    
    checkResponses(connection->responseHandler);
    
    
exit:    
    vm_deallocate(mach_task_self(), options_ptr, options_len);
    
    return KERN_SUCCESS;
#endif
}

kern_return_t _io_pm_get_capability_bits(
        mach_port_t     server,
        audit_token_t   token,
        uint32_t    *capBits,
        int         *return_code )
{

    *capBits = gCurrentCapabilityBits;
    *return_code = kIOReturnSuccess;
    return KERN_SUCCESS;
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
#endif

    *return_code = kIOReturnSuccess;

#if !TARGET_OS_EMBEDDED
exit:
#endif
    return KERN_SUCCESS;
}


kern_return_t _io_pm_set_dw_linger_interval
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
       *oldInterval = kPMDarkWakeLingerDuration;
    kPMDarkWakeLingerDuration = newInterval;
    // Force fake sleep even on unsupported platforms
    gForceDWL = true;
#endif

    *return_code = kIOReturnSuccess;

#if !TARGET_OS_EMBEDDED
exit:
#endif
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

static void setSystemSleepStateTracking(IOPMCapabilityBits capables)
{
    SCDynamicStoreRef   store = _getSharedPMDynamicStore();
    CFStringRef         key = NULL;
    CFNumberRef         capablesNum = NULL;

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
    if(!cap_ms)
        return;

    CFMutableDictionaryRef assertionDescription = NULL;
    
    // Don't allow SS sessions when 'PreventSystemSleep' assertions are held
    if (checkForActivesByType(kPreventSleepIndex)) 
        return;

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
    configAssertionType(kInteractivePushServiceIndex, false);
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

#if !TARGET_OS_EMBEDDED

static void logASLMessageSleepServiceBegins(long withCapTime)
{
    aslmsg      m;
    char        strbuf[125];
    
    m = new_msg_pmset_log();
    
    asl_set(m, kPMASLDomainKey, kPMASLDomainSleepServiceStarted);
    
    /* com.apple.message.uuid = <SleepWake UUID>
     */
    if (_getUUIDString(strbuf, sizeof(strbuf))) {
        asl_set(m, kPMASLUUIDKey, strbuf);
    }
    
    /* com.apple.message.uuid2 = <SleepServices UUID>
     */
    if (gSleepService.uuid
        && CFStringGetCString(gSleepService.uuid, strbuf, sizeof(strbuf), kCFStringEncodingUTF8))
    {
        asl_set(m, kPMASLUUID2Key, strbuf);
    }
    
    snprintf(strbuf, sizeof(strbuf), "%ld", withCapTime);
    asl_set(m, kPMASLValueKey, strbuf);
    
    snprintf(strbuf, sizeof(strbuf), "SleepService: window begins with cap time=%ld secs", withCapTime/1000);
    asl_set(m, ASL_KEY_MSG, strbuf);
    
    asl_send(NULL, m);
    asl_free(m);
}
#endif

/*****************************************************************************/
/* LOG ASL MESSAGE SLEEPSERVICE - Terminated*/
/*****************************************************************************/

__private_extern__ void logASLMessageSleepServiceTerminated(int forcedTimeoutCnt)
{
    aslmsg      m;
    char        strUUID[100];
    char        strUUID2[100];
    char        valStr[30];
    
    if ( (gPowerState & kDarkWakeForSSState) == 0)
       return;

    gPowerState &= ~kDarkWakeForSSState;
    configAssertionType(kPushServiceTaskIndex, false);
    configAssertionType(kInteractivePushServiceIndex, false);

    m = new_msg_pmset_log();
    
    asl_set(m, kPMASLDomainKey, kPMASLDomainSleepServiceTerminated);
    
    /* com.apple.message.uuid = <SleepWake UUID>
     */
    if (_getUUIDString(strUUID, sizeof(strUUID))) {
        asl_set(m, kPMASLUUIDKey, strUUID);
    }
    
    /* com.apple.message.uuid2 = <SleepServices UUID>
     */
    if (gSleepService.uuid
        && CFStringGetCString(gSleepService.uuid, strUUID2, sizeof(strUUID2), kCFStringEncodingUTF8))
    {
        asl_set(m, kPMASLUUID2Key, strUUID2);
    }
    
    /* value = # of clients whose assertions had to be timed out.
     */
    snprintf(valStr, sizeof(valStr), "%d", forcedTimeoutCnt);
    asl_set(m, kPMASLValueKey, valStr);
        

    asl_set(m, ASL_KEY_MSG, "SleepService: window has terminated.");

    /* Signature for how many clients timed out
     */    
    if (forcedTimeoutCnt == 0)
    {
        asl_set(m, kPMASLSignatureKey, kPMASLSigSleepServiceExitClean);
    } else {
        asl_set(m, kPMASLSignatureKey, kPMASLSigSleepServiceTimedOut);
    }
    
    asl_send(NULL, m);
    asl_free(m);
    
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
bool isA_SleepState()
{
   if (gPowerState & kSleepState)
       return true;
   return false;
}
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

__private_extern__ void set_NotificationDisplayWake()
{
   gPowerState |= kNotificationDisplayWakeState;
}

__private_extern__ void cancel_NotificationDisplayWake()
{
   gPowerState &= ~kNotificationDisplayWakeState;
}

__private_extern__ bool isA_NotificationDisplayWake()
{

   if (gPowerState & kNotificationDisplayWakeState)
      return true;

   return false;
}

__private_extern__ bool isA_SleepSrvcWake()
{

   if (gPowerState & kDarkWakeForSSState)
      return true;

   return false;
}

__private_extern__ void cancelPowerNapStates( )
{

#if !TARGET_OS_EMBEDDED
    if (isA_SleepSrvcWake()) {
        gSleepService.capTime = 0;
        setSleepServicesTimeCap(0);
    }

    if (isA_BTMtnceWake() ) {
        gPowerState &= ~kDarkWakeForBTState;
        configAssertionType(kBackgroundTaskIndex, false);
    }

    SystemLoadSystemPowerStateHasChanged( );

#endif
}

/*****************************************************************************/
/* Auto Power Off Timers */
/*****************************************************************************/

/*
 * Function to query IOPPF(thru rootDomain) the next possible sleep type 
 */
static kern_return_t
getPlatformSleepType( uint32_t *sleepType )
{

    IOReturn                    ret;
    uint32_t        outputScalarCnt = 1;
    size_t          outputStructSize = 0;
    uint64_t        outs[2];

    
    ret = IOConnectCallMethod(
                gRootDomainConnect,                   // connect
                kPMGetSystemSleepType,  // selector
                NULL,                   // input
                0,                      // input count
                NULL,                   // input struct
                0,                      // input struct count
                outs,                   // output scalar
                &outputScalarCnt,       // output scalar count
                NULL,                   // output struct
                &outputStructSize);     // output struct size

    if (kIOReturnSuccess == ret)
        *sleepType = (uint32_t)outs[0];
        

    return ret;

}

static void startAutoPowerOffSleep( )
{
    uint32_t sleepType = kIOPMSleepTypeInvalid;
    CFTimeInterval nextAutoWake = 0.0;

    if (isA_SleepState()) {
        /* 
         * IOPPF can't return proper sleep type when system is still not
         * completely up. Check again after system wake is completed.
         */
        return;
    }
    /* 
     * Check with IOPPF(thru root domain) if system is in a state to
     * go into auto power off state. Also, make sure that there are 
     * no user scheduled wake requests
     */
    getPlatformSleepType( &sleepType );
    nextAutoWake = getEarliestRequestAutoWake();
    if ( (sleepType != kIOPMSleepTypePowerOff) || (nextAutoWake != 0) ) {

        if (gDebugFlags & kIOPMDebugLogCallbacks)
            asl_log(0,0,ASL_LEVEL_ERR, "Resetting APO timer. sleepType:%d nextAutoWake:%f\n",
                    sleepType, nextAutoWake);
        setAutoPowerOffTimer(false, 0);
        return;
    }

    if (gDebugFlags & kIOPMDebugLogCallbacks)
       asl_log(0,0,ASL_LEVEL_ERR,  "Cancelling assertions for AutoPower Off\n");
    cancelAutoPowerOffTimer( );
    /*
     * We will be here only if the system is  in dark wake. In that
     * case, 'kPreventIdleIndex' assertion type is not honoured any more(except if it
     * is a network wake, in which case system is not auto-Power off capable).
     * If there are any assertions of type 'kPreventSleepIndex', then system is in
     * server mode, which again make system not auto-power off capable.
     *
     * So, disabling 'kBackgroundTaskIndex' & 'kPushServiceTaskIndex' is good enough
     */
    cancelPowerNapStates( );
}

void cancelAutoPowerOffTimer()
{

    if (gApoDispatch)
        dispatch_source_cancel(gApoDispatch);
}

/*
 * setAutoPowerOffTimer: starts a timer to fire a little before the point when system 
 *                       need to go into auto power off mode. Also sets the variable ts_apo, 
 *                       indicating absolute time at which system need to go into APO mode.
 * Params:initialCall -  set to true if this is called when system is going fom S0->S3
 *        postponeAfter - When set to non-zero, timer is postponed by 'autopoweroffdelay' secs
 *                        after the specified 'postponeAfter' value. Other wise, timer is set to 
 *                        fire after 'autopowerofdelay' secs from current time.
 */
void setAutoPowerOffTimer(bool initialCall, CFAbsoluteTime  postponeAfter)
{
    int64_t apo_enable, apo_delay;
    int64_t timer_fire;
    CFAbsoluteTime  cur_time = 0.0;

    if ( (GetPMSettingNumber(CFSTR(kIOPMAutoPowerOffEnabledKey), &apo_enable) != kIOReturnSuccess) ||
            (apo_enable != 1) ) {
        if (gDebugFlags & kIOPMDebugLogCallbacks)
           asl_log(0,0,ASL_LEVEL_ERR, "Failed to get APO enabled key\n");
        ts_apo = 0;
        return;
    }

    if ( (GetPMSettingNumber(CFSTR(kIOPMAutoPowerOffDelayKey), &apo_delay) != kIOReturnSuccess) ) {
        if (gDebugFlags & kIOPMDebugLogCallbacks)
           asl_log(0,0,ASL_LEVEL_ERR, "Failed to get APO delay timer \n");
        ts_apo = 0;
        return;
    }

    cur_time = CFAbsoluteTimeGetCurrent();
    if ( postponeAfter > cur_time )
       ts_apo = postponeAfter + apo_delay;
    else
        ts_apo = cur_time + apo_delay;

    if (apo_delay > kAutoPowerOffSleepAhead) {
        /*
         * Let the timer fire little ahead of actual auto power off time, to allow system
         * go to sleep and wake up for auto power off.
         */
        timer_fire = (ts_apo - cur_time) - kAutoPowerOffSleepAhead;
    }
    else if ( !initialCall ) {
        /* 
         * For repeat checks, give at least 'apo_delay' secs 
         * before checking the sleep state
         */
        if ( postponeAfter > cur_time )
           timer_fire = (ts_apo - cur_time) - apo_delay;
        else
           timer_fire = apo_delay;
    }
    else {
       /* No need of timer. Let the system go to sleep if Auto-power off is allowed */
        startAutoPowerOffSleep();

        return;
    }


    if (gApoDispatch)
        dispatch_suspend(gApoDispatch);
    else {
        gApoDispatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0,
                0, dispatch_get_main_queue()); 
        dispatch_source_set_event_handler(gApoDispatch, ^{
            startAutoPowerOffSleep();
        });
            
        dispatch_source_set_cancel_handler(gApoDispatch, ^{
            if (gApoDispatch) {
                dispatch_release(gApoDispatch);
                gApoDispatch = 0;
            }
        }); 

    }

    if (gDebugFlags & kIOPMDebugLogCallbacks)
        asl_log(0,0,ASL_LEVEL_ERR,
                "Set auto power off timer to fire in %lld secs\n", timer_fire);
    dispatch_source_set_timer(gApoDispatch, 
            dispatch_walltime(NULL, timer_fire * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    dispatch_resume(gApoDispatch);

}

/* Evaulate for Power source change */
void evalForPSChange( )
{
    int         pwrSrc;
    static int  prevPwrSrc = -1;

    pwrSrc = _getPowerSource();
    if (pwrSrc == prevPwrSrc)
        return; // If power source hasn't changed, there is nothing to do

    prevPwrSrc = pwrSrc;

#if !TARGET_OS_EMBEDDED
    if (gSleepService.capTime != 0 &&
        isA_SleepSrvcWake()) {
        int capTimeout = getCurrentSleepServiceCapTimeout();
        gSleepService.capTime = capTimeout;
        setSleepServicesTimeCap(capTimeout);
    }
#endif

    if (pwrSrc == kBatteryPowered)
        return;

    if ((pwrSrc == kACPowered) && gApoDispatch ) {
        setAutoPowerOffTimer(false, 0);
    }
}

__private_extern__ void InternalEvalConnections(void)
{
    CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, ^{
        evalForPSChange();
    });
    CFRunLoopWakeUp(_getPMRunLoop());
    
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
    PMResponseWrangler        *responseController = NULL;
    IOPMCapabilityBits        deliverCapabilityBits = 0;
    const struct        IOPMSystemCapabilityChangeParameters *capArgs;
    CFAbsoluteTime  cur_time = 0.0;
#if !TARGET_OS_EMBEDDED
    CFStringRef sleepReason = CFSTR("");
    CFStringRef wakeType = CFSTR("");
#endif

    if(inMessageType == kIOPMMessageLastCallBeforeSleep)
    {
        if( 
#if TCPKEEPALIVE
            (getTCPKeepAliveIsActive(NULL, 0) && checkForActivesByType(kInteractivePushServiceIndex)) ||
#endif
                checkForActivesByType(kDeclareSystemActivity) )
        {
            logASLMessageSleepCanceledAtLastCall();
            /* Log all assertions that could have canceled sleep */
            CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode,
                        ^{ logASLAssertionTypeSummary(kInteractivePushServiceIndex);
                           logASLAssertionTypeSummary(kDeclareSystemActivity);});
            CFRunLoopWakeUp(_getPMRunLoop());

            IOCancelPowerChange(gRootDomainConnect, (long)messageData);
        }
        else
        {
            IOAllowPowerChange(gRootDomainConnect, (long)messageData);
            gMachineStateRevertible = false;
        }
        return;
    }

    if (PMConnectionPowerCallBack_HandleSleepForSSH(inMessageType, messageData) ||
            (kIOMessageSystemCapabilityChange != inMessageType) ) 
    {
        return;
    }

    capArgs = (const struct IOPMSystemCapabilityChangeParameters *)messageData;

    AutoWakeCapabilitiesNotification(capArgs->fromCapabilities, capArgs->toCapabilities);
    ClockSleepWakeNotification(capArgs->fromCapabilities, capArgs->toCapabilities,
                               capArgs->changeFlags);

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
        _resetWakeReason();
        sleepReason = _updateSleepReason();
        // If this is a non-SR wake, set next earliest PowerNap wake point to 
        // after 'kPMSleepDurationForBT' secs.
        // For all other wakes, don't move the next PowerNap wake point.
        if ( ( gCurrentSilentRunningState == kSilentRunningOff ) ||
              CFEqual(sleepReason, CFSTR(kIOPMDarkWakeThermalEmergencyKey))){
           ts_nextPowerNap = CFAbsoluteTimeGetCurrent() + kPMSleepDurationForBT;

        }
        cancelPowerNapStates();
#endif

        // Clear gPowerState and set it to kSleepState
        // We will clear kDarkWakeForSSState bit later when the SS session is closed.
        gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
        gPowerState |= kSleepState;


        /* We must acknowledge this sleep event within 30 second timeout, 
         *      once clients ack via IOPMConnectionAcknowledgeEvent().
         * Our processing will pick-up again in our handler
         *      _io_pm_connection_acknowledge_event
         */
        logASLMessageSleep(kPMASLSigSuccess, NULL, NULL, kIsS0Sleep);

        // Tell flight data recorder we're going to sleep
        recordFDREvent(kFDRBattEventAsync, false, _batteries());
        recordFDREvent(kFDRSleepEvent, false, NULL);

        if(smcSilentRunningSupport() )
           gCurrentSilentRunningState = kSilentRunningOn;

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
        setAutoPowerOffTimer(true, 0);

        gMachineStateRevertible = true;

        sleepReason = _updateSleepReason();

        gPowerState |= kDarkWakeState;
        gPowerState &= ~kNotificationDisplayWakeState;
        /*
         * This notification is issued before every sleep. Log to asl only if
         * there are assertion that can prevent system from going into S3/S4
         */
        if (systemBlockedInS0Dark()) {
            logASLMessageSleep(kPMASLSigSuccess, NULL, NULL, kIsDarkWake);
            recordFDREvent(kFDRDarkWakeEvent, false, NULL);


            // Take off Audio & Video capabilities. Leave other capabilities as is.
            deliverCapabilityBits = (gCurrentCapabilityBits & ~(kIOPMCapabilityAudio|kIOPMCapabilityVideo));
            if (_DWBT_allowed( ))
                deliverCapabilityBits |= kIOPMCapabilityBackgroundTask;
            // Send a notification with no expectation for response
            connectionFireNotification(deliverCapabilityBits, 0);
        }

        // Dark Wake Linger: After going from FullWake to DarkWake during a
        // demand sleep, linger in darkwake for a certain amount of seconds
        //
        // Power Nap machines will linger on every FullWake --> DarkWake
        // transition except emergency sleeps.
        // Non-Power Nap machines will linger on every FullWake --> DarkWake
        // transition except emergency sleeps, and clamshell close sleeps
        bool isBTCapable = (IOPMFeatureIsAvailable(CFSTR(kIOPMDarkWakeBackgroundTaskKey), NULL) ||
                            gForceDWL)?true:false;

        bool isEmergencySleep = (CFEqual(sleepReason, CFSTR(kIOPMLowPowerSleepKey)) ||
                                 CFEqual(sleepReason, CFSTR(kIOPMThermalEmergencySleepKey)))?true:false;
        bool isClamshellSleep = CFEqual(sleepReason, CFSTR(kIOPMClamshellSleepKey))?true:false;

        if(((isBTCapable && !isEmergencySleep) ||
            (!isBTCapable && !isEmergencySleep && !isClamshellSleep)) &&
           (kPMDarkWakeLingerDuration != 0))
        {

            CFMutableDictionaryRef assertionDescription = NULL;
            assertionDescription = _IOPMAssertionDescriptionCreate(
                                        kIOPMAssertInternalPreventSleep,
                                        CFSTR("com.apple.powermanagement.darkwakelinger"),
                                        NULL, CFSTR("Proxy assertion to linger in darkwake"),
                                        NULL, kPMDarkWakeLingerDuration,
                                        kIOPMAssertionTimeoutActionRelease);

            if (assertionDescription)
            {
                //This assertion should be applied even on battery power
                CFDictionarySetValue(assertionDescription,
                                     kIOPMAssertionAppliesToLimitedPowerKey,
                                     (CFBooleanRef)kCFBooleanTrue);
                InternalCreateAssertion(assertionDescription, NULL);

                CFRelease(assertionDescription);
            }
        }

#endif
    } else if (SYSTEM_DID_WAKE(capArgs))
    {
#if !TARGET_OS_EMBEDDED
        _updateWakeReason(NULL, &wakeType);
        // On a SilentRunningMachine, the assumption is that every wake is
        // Silent until powerd unclamps SilentRunning or unforeseen thermal
        // constraints arise
#endif
        if(smcSilentRunningSupport() )
           gCurrentSilentRunningState = kSilentRunningOn;

        gMachineStateRevertible = true;

        if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityGraphics))
        {
            // e.g. System has powered into full wake
#if !TARGET_OS_EMBEDDED
            // Unclamp SilentRunning if this full wake is not due to Notification display
            if (CFEqual(wakeType, kIOPMRootDomainWakeTypeNotification)) {
                // For notification display wake, keep the 'BPS' capability bits if they
                // are currently set
                deliverCapabilityBits = (gCurrentCapabilityBits & (
                    (kIOPMCapabilityPushServiceTask|kIOPMCapabilityBackgroundTask|kIOPMCapabilitySilentRunning)));

                gPowerState |= kFullWakeState;
            }
            else {
                cancelPowerNapStates();
                _unclamp_silent_running(false);

                // kDarkWakeForSSState bit is removed when the SS session is closed.
                gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
                gPowerState |= kFullWakeState;

#if TCPKEEPALIVE
                // Reset the wake quota only for FullWakes that are not notification wakes
                TCPWakeQuotaRecordWake(kIsUserWake);
#endif
            }
            cancelAutoPowerOffTimer();
#else
            gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
            gPowerState |= kFullWakeState;
#endif
            deliverCapabilityBits |=
                    kIOPMCapabilityCPU | kIOPMCapabilityDisk 
                    | kIOPMCapabilityNetwork | kIOPMCapabilityAudio | kIOPMCapabilityVideo;

            if (BIT_IS_SET(capArgs->fromCapabilities, kIOPMSystemCapabilityCPU)) {
                logASLMessageWake(kPMASLSigSuccess, NULL,  NULL, deliverCapabilityBits, kIsDarkToFullWake);
                recordFDREvent(kFDRUserWakeEvent, false, NULL);
            } else {
                logASLMessageWake(kPMASLSigSuccess, NULL,  NULL, deliverCapabilityBits, kIsFullWake);
                mt2RecordWakeEvent(kWakeStateFull);
                recordFDREvent(kFDRUserWakeEvent, true, NULL);
            }

        } else if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityCPU))
        {
            //If system is in any power nap state, cancel those states
            cancelPowerNapStates();
            
            // e.g. System has moved into dark wake
            deliverCapabilityBits = 
                    kIOPMCapabilityCPU | kIOPMCapabilityDisk | kIOPMCapabilityNetwork;
            

            // kDarkWakeForSSState bit is removed when the SS session is closed.
            gPowerState &= ~(kPowerStateMask ^ kDarkWakeForSSState);
            gPowerState |= kDarkWakeState;
            
            _ProxyAssertions(capArgs);
            cur_time = CFAbsoluteTimeGetCurrent();
            
#if !TARGET_OS_EMBEDDED
            if ( (cur_time >= ts_nextPowerNap) &&  
                    (CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) || 
                            CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService)) ) {

                  if ( _DWBT_allowed() ) {
                      gPowerState |= kDarkWakeForBTState;
                      configAssertionType(kBackgroundTaskIndex, false);
                      /* Log all background task assertions once again */
                      CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, 
                                 ^{ logASLAssertionTypeSummary(kBackgroundTaskIndex); });
                      CFRunLoopWakeUp(_getPMRunLoop());

                      deliverCapabilityBits |= (kIOPMCapabilityBackgroundTask  |
                                                kIOPMCapabilityPushServiceTask |
                                                kIOPMCapabilitySilentRunning);
                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());
                  }
                  else if (_SS_allowed() ) {
                      deliverCapabilityBits |=  (kIOPMCapabilityPushServiceTask |
                                                 kIOPMCapabilitySilentRunning);
                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());
                  }
            }
            else if (CFEqual(wakeType, kIOPMRootDomainWakeTypeNetwork)) {
                if (_DWBT_allowed()) {
                      deliverCapabilityBits |= (kIOPMCapabilityBackgroundTask  |
                                                kIOPMCapabilityPushServiceTask |
                                                kIOPMCapabilitySilentRunning);
                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());

                }
                else if (_SS_allowed() ) {
                        deliverCapabilityBits |=  (kIOPMCapabilityPushServiceTask |
                                                   kIOPMCapabilitySilentRunning);

                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());
                }
            }
            else {
               if ( !CFEqual(wakeType, kIOPMrootDomainWakeTypeLowBattery) &&
                    !CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepTimer) ) {

                    if (_SS_allowed() ) {
                        deliverCapabilityBits |=  (kIOPMCapabilityPushServiceTask |
                                                   kIOPMCapabilitySilentRunning);

                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());
                    }
               }
            }
            if (checkForActivesByType(kPreventSleepIndex)) {
                deliverCapabilityBits &= ~kIOPMCapabilitySilentRunning;
               _unclamp_silent_running(false);
            }

            if ((kACPowered == _getPowerSource()) && (kPMDarkWakeLingerDuration != 0)) {
                CFMutableDictionaryRef assertionDescription = NULL;
                assertionDescription = _IOPMAssertionDescriptionCreate(
                                kIOPMAssertInternalPreventSleep,
                                CFSTR("com.apple.powermanagement.acwakelinger"),
                                NULL, CFSTR("Proxy assertion to linger on darkwake with ac"),
                                NULL, kPMACWakeLingerDuration,
                                kIOPMAssertionTimeoutActionRelease);

                InternalCreateAssertion(assertionDescription, NULL);
                CFRelease(assertionDescription);
            }
#endif
            if (gApoDispatch && ts_apo && (cur_time > ts_apo - kAutoPowerOffSleepAhead) ) {
                /* Check for auto-power off if required */
                dispatch_suspend(gApoDispatch);
                dispatch_source_set_timer(gApoDispatch, 
                        DISPATCH_TIME_NOW, DISPATCH_TIME_FOREVER, 0);
                dispatch_resume(gApoDispatch);
            }
            logASLMessageWake(kPMASLSigSuccess, NULL,  NULL, deliverCapabilityBits, kIsDarkWake);
            mt2RecordWakeEvent(kWakeStateDark);
            recordFDREvent(kFDRDarkWakeEvent, true, NULL);
#if TCPKEEPALIVE
            TCPWakeQuotaRecordWake(kIsDarkWake);
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
        logASLMessageIORegisterForSystemPowerResponses();
        saveAppResponseStackshots();
        return;
    }
    else if (SYSTEM_WILL_WAKE(capArgs) )
    {
        gMachineStateRevertible = true;

        CFRunLoopPerformBlock(_getPMRunLoop(), kCFRunLoopDefaultMode, 
                ^{ configAssertionType(kInteractivePushServiceIndex, false); });
        CFRunLoopWakeUp(_getPMRunLoop());

        deliverCapabilityBits = kIOPMEarlyWakeNotification |
              kIOPMCapabilityCPU | kIOPMCapabilityDisk | kIOPMCapabilityNetwork ;

        if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityGraphics))
           deliverCapabilityBits |= kIOPMCapabilityVideo | kIOPMCapabilityAudio;

        sendEarlyNotification( deliverCapabilityBits );
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

    gCurrentCapabilityBits = interestBitsNotify;

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
           logASLPMConnectionNotify(awaitThis->connection->callerName, interestBitsNotify );
         
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
static void sendEarlyNotification( int interestBitsNotify )
{

    int                     i, count = 0;
    uint32_t                messageToken = 0;
    uint16_t                calloutCount = 0;
    PMConnection            *connection = NULL;

    interestBitsNotify |= kIOPMEarlyWakeNotification;

    count = CFArrayGetCount(gConnections);

    for (i=0; i<count; i++)
    {
        connection = (PMConnection *)CFArrayGetValueAtIndex(gConnections, i);

        if ( ((connection->interestsBits & kIOPMEarlyWakeNotification) == 0) ||
                ((connection->interestsBits & interestBitsNotify) == kIOPMEarlyWakeNotification))
           continue;

        if ((MACH_PORT_NULL == connection->notifyPort) ||
            (false == connection->notifyEnable)) {
            continue;
        }
        
        /* We generate a messagetoken here, which the notifiee must pass 
         * back into us when the client acknowledges. 
         */
        messageToken = (interestBitsNotify << 16)
                            | calloutCount;


        _sendMachMessage(connection->notifyPort, 
                            0,
                            interestBitsNotify, 
                            messageToken);


        if (gDebugFlags & kIOPMDebugLogCallbacks)
           logASLPMConnectionNotify(connection->callerName, interestBitsNotify );
         
        calloutCount++;
    }


}
/*****************************************************************************/
/*****************************************************************************/

static void responsesTimedOut(CFRunLoopTimerRef timer, void * info)
{
    PMResponseWrangler  *responseWrangler = (PMResponseWrangler *)info;
    PMResponse          *one_response = NULL;

    int             responsesCount = 0;
    int             i;
    bool            take_stackshot = false;
#if !TARGET_OS_EMBEDDED
    char            uuid[64];
    char            appName[32];
    char            pidbuf[64];
    uint32_t        bytesRemaining;
    char            *ptr = NULL;
    int             n;

    pidbuf[0] = 0;
#endif

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
        one_response->replied = true;
        one_response->timedout = true;
        one_response->repliedWhen = CFAbsoluteTimeGetCurrent();
        if ((one_response->connection->timeoutCnt++ == 0) && (gDebugFlags & kIOPMDebugAppTimeoutStackshot))
            take_stackshot = true;
        
        int timeIntervalMS = (int)((one_response->repliedWhen - one_response->notifiedWhen) * 1000);
        CFNumberRef timeIntervalNumber = CFNumberCreate(NULL, kCFNumberIntType, &timeIntervalMS);

        logASLMessagePMConnectionResponse(
            kAppResponseLogSourcePMConnection, 
            one_response->connection->callerName, 
            CFSTR(kIOPMStatsResponseTimedOut), 
            timeIntervalNumber,
            one_response->notificationType);

        if (timeIntervalNumber)
            CFRelease(timeIntervalNumber);
#if !TARGET_OS_EMBEDDED
        if (isA_CFString(one_response->connection->callerName) && 
                CFStringGetCString(one_response->connection->callerName, appName, sizeof(appName), kCFStringEncodingUTF8))
            snprintf(pidbuf, sizeof(pidbuf), "%s %s(%d)",pidbuf, appName, one_response->connection->callerPID);
        else
            snprintf(pidbuf, sizeof(pidbuf), "%s (%d)",pidbuf, one_response->connection->callerPID );
#endif
    }

#if !TARGET_OS_EMBEDDED
    if (stackshotBuf || (take_stackshot == false)) {
        goto done; // Allow only one stackshot per sleep/wake cycle
    }
    stackshotSize = bytesRemaining = 256*1024;
    stackshotBuf = malloc(stackshotSize);
    if (!stackshotBuf) goto done;

    ptr = (char *)stackshotBuf;
    if (_getUUIDString(uuid, sizeof(uuid))) {
        n = snprintf(ptr, bytesRemaining, "UUID: %s\n", uuid)+1;
        if (n > 0) {
            bytesRemaining -= n;
            ptr += n;
        }
    }
    n = sprintf(ptr, "caps: %d\n", one_response ? one_response->notificationType: -1)+1;
    if (n > 0) {
        bytesRemaining -= n;
        ptr += n;
    }
    n = sprintf(ptr, "Process: %s\n", pidbuf)+1;
    if (n > 0) {
        bytesRemaining -= n;
        ptr += n;
    }
    n = sprintf(ptr, "Stackshot reason: PM Connection client timeout\n")+1;
    if (n > 0) {
        bytesRemaining -= n;
        ptr += n;
    }
    stackshotOffset = stackshotSize-bytesRemaining;

    stackshotSize = syscall(SYS_stack_snapshot, -1, ptr, bytesRemaining, 
                    STACKSHOT_SAVE_LOADINFO | STACKSHOT_SAVE_KEXT_LOADINFO, 0);
    if (stackshotSize == -1) {
        free(stackshotBuf);
        stackshotBuf = 0;
    }

done:
#endif
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
    
#if 0
    if (ts_apo) {
        CFAbsoluteTime t = CFAbsoluteTimeGetCurrent();
        if (ts_apo > t) t = ts_apo;
        describeWakeEvent(NULL, 
                          allWakeEventsString, CFSTR("AutoPowerOffTimer"), t,
                          NULL);
    }
#endif
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
            // Make sure that wake request is at least 1 min from now
            if (VALID_DATE(pick[kChooseMaintenance]) && pick[kChooseMaintenance] < (CFAbsoluteTimeGetCurrent()+60))
                pick[kChooseMaintenance] = CFAbsoluteTimeGetCurrent()+60;
        }
        
        if (!VALID_DATE(pick[kChooseSleepServiceWake])
            || (VALID_DATE(oneResponse->sleepServiceRequested) && (oneResponse->sleepServiceRequested < pick[kChooseSleepServiceWake])))
        {
            pick[kChooseSleepServiceWake] = oneResponse->sleepServiceRequested;
            // Make sure that wake request is at least 1 min from now
            if (VALID_DATE(pick[kChooseSleepServiceWake]) && pick[kChooseSleepServiceWake] < (CFAbsoluteTimeGetCurrent()+60)) 
                pick[kChooseSleepServiceWake] = CFAbsoluteTimeGetCurrent()+60;
        }

        if (!VALID_DATE(pick[kChooseTimerPlugin])
            || (VALID_DATE(oneResponse->timerPluginRequested) && (oneResponse->timerPluginRequested < pick[kChooseTimerPlugin])))
        {
            pick[kChooseTimerPlugin] = oneResponse->timerPluginRequested;
            // Make sure that wake request is at least 1 min from now
            if (VALID_DATE(pick[kChooseTimerPlugin]) && pick[kChooseTimerPlugin] < (CFAbsoluteTimeGetCurrent()+60))
                pick[kChooseTimerPlugin] = CFAbsoluteTimeGetCurrent()+60;
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
    CFNumberRef     diff_secs = NULL;
    int             idx;
    uint64_t        secs_to_apo = ULONG_MAX;
    CFAbsoluteTime  cur_time = 0.0;
    uint32_t        sleepType = kIOPMSleepTypeInvalid;
    bool            skip_scheduling = false;
    CFBooleanRef    scheduleEvent = kCFBooleanFalse;
    
    if (!pickWakeEvent) {
        bzero(dummy, sizeof(dummy));
        pickWakeEvent = dummy;
    }
    
    pickWakeEvent[kChooseFullWake]          = getEarliestRequestAutoWake();

    if ( !pickEarliestEvent(pickWakeEvent, &idx, &scheduleTime) )
    {
        // Set a huge number for following comaprisions with power off timer
        scheduleTime = kCFAbsoluteTimeIntervalSince1904;
    }


    // Always set the Auto Power off timer
    if ( ts_apo != 0 ) 
    {
        // Using 'kIOPMUserWakeAlarmScheduledKey' property to report both user requsted
        // wakes and Sleep service related wakes, as there is no other way to report
        // sleep service related schedules to rootDomain
        if (pickWakeEvent[kChooseFullWake]  || pickWakeEvent[kChooseSleepServiceWake]) {

            scheduleEvent = kCFBooleanTrue;
            _setRootDomainProperty(CFSTR(kIOPMUserWakeAlarmScheduledKey), scheduleEvent);
        }
        else {
            scheduleEvent = kCFBooleanFalse;
            _setRootDomainProperty(CFSTR(kIOPMUserWakeAlarmScheduledKey), scheduleEvent);
        }

        if (ts_apo < scheduleTime+kAutoPowerOffSleepAhead) {

           getPlatformSleepType( &sleepType );
           if ((scheduleEvent == kCFBooleanFalse) && (sleepType == kIOPMSleepTypePowerOff))
               skip_scheduling = true;

        }

        cur_time = CFAbsoluteTimeGetCurrent();
        if (cur_time >= ts_apo)
            secs_to_apo = 0;
        else
            secs_to_apo = ts_apo - cur_time;

        diff_secs = CFNumberCreate(0, kCFNumberLongType, &secs_to_apo);
        if (diff_secs) {
           _setRootDomainProperty( CFSTR(kIOPMAutoPowerOffTimerKey), (CFTypeRef)diff_secs );
           CFRelease(diff_secs);
        }
 
       if ( skip_scheduling ) {
             // Auto Power off timer is the earliest one and system is capable of
             // entering ErpLot6. Don't schedule any other wakes.
             if (gDebugFlags & kIOPMDebugLogCallbacks)
                asl_log(0,0,ASL_LEVEL_ERR, 
                     "Not scheduling other wakes to allow AutoPower off. APO timer:%lld\n", 
                     secs_to_apo);
             return;
       }
       if (gDebugFlags & kIOPMDebugLogCallbacks)
             asl_log(0,0,ASL_LEVEL_ERR, "SS_wake:%d Alarm_wake:%d sleepType:%d APO timer:%lld secs\n", 
                     (pickWakeEvent[kChooseSleepServiceWake] != 0),
                     (pickWakeEvent[kChooseFullWake] != 0),
                     sleepType, secs_to_apo);
    }

    if ( scheduleTime == kCFAbsoluteTimeIntervalSince1904) 
    {
       // Nothing to schedule. bail..
       return;
    }

    // INVARIANT: At least one of the WakeTimes we're evaluating has a valid date
    if ((kChooseMaintenance == idx) || (kChooseTimerPlugin == idx))
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
__private_extern__ IOReturn _unclamp_silent_running(bool sendNewCapBits)
{
    IOPMCapabilityBits deliverCapabilityBits;

    // Nothing to do. It's already unclamped
    if(gCurrentSilentRunningState == kSilentRunningOff)
        return kIOReturnSuccess;

    // Nothing to do. SMC doesn't support SR
    if(!smcSilentRunningSupport())
        return kIOReturnUnsupported;

    // Unset background, push and silent bits only if they are set in current capabilities. Inform PMConnection clients
    if ((sendNewCapBits) && (gCurrentCapabilityBits & 
                (kIOPMCapabilityPushServiceTask|kIOPMCapabilityBackgroundTask|kIOPMCapabilitySilentRunning)))
    {
        deliverCapabilityBits = gCurrentCapabilityBits & ~(kIOPMCapabilitySilentRunning);
        connectionFireNotification(deliverCapabilityBits, 0);
    }

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

__private_extern__ bool _can_back_out_of_idle(void)
{
    return gMachineStateRevertible;
}


__private_extern__ io_connect_t getRootDomainConnect()
{
   return gRootDomainConnect;
}

/*
 * Returns the per-platform sleep service cap timeout for the current power
 * source in milliseconds
 */
#if !TARGET_OS_EMBEDDED
__private_extern__ int getCurrentSleepServiceCapTimeout()
{
    static int acCapTimeout     = -1;
    static int battCapTimeout   = -1;
    int currentCap              = 3*60*1000;  // Set this to a default value of 3mins
    int pwrSrc                  = _getPowerSource();

    if(kACPowered == pwrSrc && acCapTimeout != (-1))
        return acCapTimeout;
    else if(kBatteryPowered == pwrSrc && battCapTimeout != (-1))
        return battCapTimeout;

    CFDictionaryRef dwServicesDict      = NULL;
    CFDictionaryRef ssModesDict         = NULL;
    CFDictionaryRef modeADict           = NULL;
    CFDictionaryRef baseIntervalsDict   = NULL;
    CFDictionaryRef powerSourceDict     = NULL;
    CFNumberRef     baseWakeCap         = NULL;

    dwServicesDict = (CFDictionaryRef)_copyRootDomainProperty(
                                        CFSTR("DarkWakeServices"));
    if(dwServicesDict) {
        ssModesDict = CFDictionaryGetValue(
                        dwServicesDict,
                        CFSTR("SleepServicesModes"));
        if(ssModesDict) {
            modeADict = CFDictionaryGetValue(
                            ssModesDict,
                            CFSTR("ModeA"));
            if(modeADict) {
                baseIntervalsDict = CFDictionaryGetValue(
                                        modeADict,
                                        CFSTR("BaseIntervals"));
                if(baseIntervalsDict) {
                    if(kACPowered == pwrSrc) {
                        powerSourceDict = CFDictionaryGetValue(
                                            baseIntervalsDict,
                                            CFSTR("AC"));
                        if(powerSourceDict)
                            baseWakeCap = CFDictionaryGetValue(
                                            powerSourceDict,
                                            CFSTR("BaseWakeCapInterval"));
                        if(baseWakeCap) {
                            CFNumberGetValue(
                                baseWakeCap,
                                kCFNumberIntType,
                                &acCapTimeout);
                            // (Time in secs) * 1000
                            acCapTimeout *= 1000;
                            currentCap    = acCapTimeout;
                        } else {
                            acCapTimeout = battCapTimeout = -1;
                        }
                    } else if (kBatteryPowered == pwrSrc) {
                        powerSourceDict = CFDictionaryGetValue(
                                            baseIntervalsDict,
                                            CFSTR("Battery"));
                        if(powerSourceDict)
                            baseWakeCap = CFDictionaryGetValue(
                                            powerSourceDict,
                                            CFSTR("BaseWakeCapInterval"));
                        if(baseWakeCap) {
                            CFNumberGetValue(
                                baseWakeCap,
                                kCFNumberIntType,
                                &battCapTimeout);
                            // (Time in secs) * 1000
                            battCapTimeout *= 1000;
                            currentCap      = battCapTimeout;
                        } else {
                            acCapTimeout = battCapTimeout = -1;
                        }
                    }
                } else {
                    acCapTimeout = battCapTimeout = -1;
                }
            } else {
                acCapTimeout = battCapTimeout = -1;
            }
        } else {
            acCapTimeout = battCapTimeout = -1;
        }
        CFRelease(dwServicesDict);
    } else {
        acCapTimeout = battCapTimeout = -1;
    }

    return currentCap;
}
#endif

static void saveAppResponseStackshots() 
{
    int fd;

    if (stackshotBuf == NULL)
        return;

    if ((fd = open("/var/tmp/PMSleepWakeStacks.dump", O_CREAT|O_RDWR|O_NOFOLLOW, S_IRUSR|S_IWUSR|S_IRGRP)) < 0) {
        goto exit;
    }
    write(fd, (char *)stackshotBuf+stackshotOffset, stackshotSize);
    close(fd);

    if ((fd = open("/var/tmp/PMSleepWakeLog.dump", O_CREAT|O_RDWR|O_NOFOLLOW, S_IRUSR|S_IWUSR|S_IRGRP)) < 0) {
        goto exit;
    }
    write(fd, (char *)stackshotBuf, stackshotOffset);
    close(fd);


exit:
    if (fd > 0)
        close(fd);
    free(stackshotBuf);
    stackshotBuf = NULL;

}
