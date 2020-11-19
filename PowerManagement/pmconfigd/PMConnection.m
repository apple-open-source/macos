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
#include <mach/mach_time.h>
#if !TARGET_OS_IPHONE
#include <libspindump_priv.h>
#endif
#include <CoreFoundation/CFXPCBridge.h>

#include "PrivateLib.h"
#include "PMConnection.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "PMAssertions.h"
#include "PMStore.h"
#include "SystemLoad.h"
#if !TARGET_OS_IPHONE
#include "TTYKeepAwake.h"
#include "adaptiveDisplay.h"
#endif
#include "PMSettings.h"
#include "Platform.h"
#include "Platform.h"
#include "StandbyTimer.h"
#include "pmconfigd.h"

/************************************************************************************/


#ifndef kIOPMMaintenanceScheduleImmediate
#define kIOPMMaintenanceScheduleImmediate               "MaintenanceImmediate"
#endif

#ifndef kIOPMSystemCapabilitiesKey
#define kIOPMSystemCapabilitiesKey  "System Capabilities"
#endif

// Number of seconds before auto power off timer we let system go to sleep
#define kAutoPowerOffSleepAhead     (0)

os_log_t    sleepwake_log = NULL;
#undef   LOG_STREAM
#define  LOG_STREAM   sleepwake_log


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
typedef enum {
    kChooseFullWake         = 0,
    kChooseMaintenance      = 1,
    kChooseSleepServiceWake = 2,
    kChooseTimerPlugin      = 3,
    kChooseWakeTypeCount    = 4
} wakeType_e;

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
#if !TARGET_OS_IPHONE
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
static int kPMSleepDurationForBT = (15*60); // Defaults to 15 mins
static int kPMDarkWakeLingerDuration = 5; // Defaults to 5 secs
static int kPMAlwaysOn = 1;
#else
static int kPMSleepDurationForBT = (30*60); // Defaults to 30 mins
static int kPMDarkWakeLingerDuration = 15;
static int kPMAlwaysOn = 0;
#endif // (TARGET_OS_OSX && TARGET_CPU_ARM64)
static int kPMACWakeLingerDuration = 45; // Defaults to 45 secs
#if LOG_SLEEPSERVICES
static int                      gNotifySleepServiceToken = 0;
#endif
// Time at which system can wake for next PowerNap
static CFAbsoluteTime      ts_nextPowerNap = 0;
// Timer to fire kPMSleepDurationForBT after sleep to update capabilities if needed
static dispatch_source_t gDarkWakeCapabilitiesCheckTimer = 0;
#endif
static int gIdleSleepPreventersToken = 0;
static int gSystemSleepPreventersToken = 0;
uint64_t lastcall_ts = 0;

// Wake time bookkeeping
static uint64_t gCurrentWakeTime = 0; 
static uint64_t gCurrentWakeStart = 0; // read from SMC on calendar re sync notification
static uint64_t gCurrentWakeEnd = 0; // updated on capability change callback
bool gIsDarkWake = false;
bool gWakeFromDarkWake = false;
bool gCapabilityChangeDone = false;

// gPowerState - Tracks various wake types the system is currently in
// by setting appropriate bits. 
static uint32_t                 gPowerState;

// scheduled wake logging

CFMutableDictionaryRef gPendingScheduledWakeLog;
CFMutableDictionaryRef gCurrentScheduledWakeLog;
CFAbsoluteTime          leeway = 5.0;

static io_service_t             rootDomainService = IO_OBJECT_NULL;
static IOPMCapabilityBits       gCurrentCapabilityBits = kIOPMCapabilityCPU | kIOPMCapabilityDisk 
                                    | kIOPMCapabilityNetwork | kIOPMCapabilityAudio | kIOPMCapabilityVideo;


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
    CFMutableArrayRef       responseStats;
    dispatch_source_t       awaitingResponsesTimeout;
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
    pid_t                   callerPID;
    IOPMCapabilityBits      interestsBits;
    bool                    notifyEnable;
    int                     timeoutCnt;
    dispatch_source_t       procExit;
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
    CFStringRef             clientInfoStringBGTask;
    CFStringRef             clientInfoStringAppRefresh;
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

static void PMScheduleWakeEventChooseBest(CFAbsoluteTime scheduleTime, wakeType_e type);

static void responsesTimedOut(PMResponseWrangler * info);

static void cleanupConnection(PMConnection *reap);

static void cleanupResponseWrangler(PMResponseWrangler *reap);

static void setSystemSleepStateTracking(IOPMCapabilityBits);

#if !TARGET_OS_IPHONE
static void scheduleSleepServiceCapTimerEnforcer(uint32_t cap_ms);
#endif

/* Hide SleepServices code for public OS seeds. 
 * We plan to re-enable this code for shipment.
 * ETB 1/24/12
 */

#if LOG_SLEEPSERVICES
#if !TARGET_OS_IPHONE
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

__private_extern__ bool isDisplayAsleep(void);
void setAutoPowerOffTimer(bool initialCall, CFAbsoluteTime postpone);
static void sendNoRespNotification( int interestBitsNotify );
static void sendNoRespNotificationToInterestedClients( int interestBitsNotify );
void cancelAutoPowerOffTimer(void);
static void setInitialSleepPreventersCount(int type);
static bool PMConnectionHandleDeadName(uint32_t connection_id);
static bool checkResponses_ScheduleWakeEvents(PMResponseWrangler *wrangler);

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

uint32_t                        gDebugFlags = kIOPMDebugAssertionASLLog|
                                                kIOPMDebugEnableSpindumpOnFullwake|
                                                kIOPMDebugLogAssertionActivity;

uint32_t                        gCurrentSilentRunningState = kSilentRunningOff;

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

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

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
    CFMutableDictionaryRef      rdProps = NULL;
    CFNumberRef                 caps_cf = NULL;
    
    sleepwake_log = os_log_create(PM_LOG_SYSTEM, SLEEPWAKE_LOG);
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

    IONotificationPortSetDispatchQueue(notify, _getPMMainQueue());

    dispatch_async(_getPMMainQueue(), ^{
        setInitialSleepPreventersCount(kIOPMIdleSleepPreventers);
        setInitialSleepPreventersCount(kIOPMSystemSleepPreventers);
    });

    IORegistryEntryCreateCFProperties(rootDomainService, &rdProps, 0, 0);
    if (isA_CFDictionary(rdProps)) {
        caps_cf = CFDictionaryGetValue(rdProps, CFSTR(kIOPMSystemCapabilitiesKey));
        if (isA_CFNumber(caps_cf)) {
            uint32_t caps;

            CFNumberGetValue(caps_cf, kCFNumberIntType, &caps);
            if (caps & (kIOPMSystemCapabilityGraphics|kIOPMSystemCapabilityAudio)) {
                gPowerState = kFullWakeState;
            }
            else if (caps & kIOPMSystemCapabilityCPU) {
                gPowerState = kDarkWakeState;
            }
            else {
                gPowerState = kSleepState;
            }
        }
    }
    if (rdProps) {
        CFRelease(rdProps);
    }
#if !TARGET_OS_IPHONE
    // Make sure we don't get stuck with the VM dark mode enabled.
    setVMDarkwakeMode(false);
#endif

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
kern_return_t _io_pm_connection_create (
    mach_port_t     server __unused,
    audit_token_t   token,
    string_t        name,
    int             interests,
    uint32_t        *connection_id,
    int             *return_code
)
{
    PMConnection         *newConnection = NULL;
    *connection_id = 0;
    // Allocate: create a new PMConnection type
    createConnectionWithID(&newConnection);
    if (!newConnection) {
        *return_code = kIOReturnError;
        goto exit;
    }
    
    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &newConnection->callerPID, NULL, NULL);

    // Save caller name for logging
    if (name && strlen(name)) {
        newConnection->callerName = CFStringCreateWithCString(0, name, kCFStringEncodingUTF8);
    }

    newConnection->interestsBits = interests;
    *connection_id = newConnection->uniqueID;
    *return_code = kIOReturnSuccess;

exit:

    return KERN_SUCCESS;
}


/*****************************************************************************/
/*****************************************************************************/

 kern_return_t _io_pm_connection_schedule_notification(
    mach_port_t     server __unused,
    audit_token_t   token,
    uint32_t        connection_id,
    mach_port_t     notify_port_in,
    int             disable,
    int             *return_code)
{
    PMConnection        *connection = NULL;
    pid_t               callerPID = -1;

    if (MACH_PORT_NULL == notify_port_in || NULL == return_code) {
        if (return_code) *return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    __MACH_PORT_DEBUG(true, "_io_pm_connection_schedule_notification notify_port", notify_port_in);
    
    *return_code = kIOReturnError;
    
    audit_token_to_au32(token, NULL, NULL, NULL, NULL, NULL, &callerPID, NULL, NULL);
    connection = connectionForID(connection_id);
    if (!connection) {
        ERROR_LOG("Failed to find the connection for ID %d from pid %d\n", connection_id, callerPID);
        return kIOReturnNotFound;
    }
    if (connection->callerPID != callerPID) {
        ERROR_LOG("Notification schedule request from unexepcted pid %d for connection %d. Expected pid:%d\n",
                callerPID, connection_id, connection->callerPID);

        return kIOReturnNotFound;
    }

    connection->notifyEnable = (disable == 0);

    if (!disable && (MACH_PORT_NULL == connection->notifyPort)) {
        dispatch_source_t src;
        connection->notifyPort = notify_port_in;

        src = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, callerPID,
                                            DISPATCH_PROC_EXIT, _getPMMainQueue());
        if (src != NULL) {
            dispatch_source_set_event_handler(src, ^{
                    PMConnectionHandleDeadName(connection_id);
                    });
            dispatch_source_set_cancel_handler(src, ^{dispatch_release(src);});
            dispatch_resume(src);

            // Create dispatch source in local variable 'src' to force the cancel handler
            // block to make a copy of the address
            connection->procExit = src;
        }
        else {
            ERROR_LOG("Failed to create dispatch src to cleanup connection %d from pid %d\n", connection_id, callerPID);
        }
    

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

#if !TARGET_OS_IPHONE
static PMResponse *_io_pm_acknowledge_event_findOutstandingResponseForToken(PMConnection *connection, int token)
{
    CFMutableArrayRef   responsesTrackingList = NULL;
    long                responsesCount = 0;
    PMResponse          *checkResponse = NULL;
    PMResponse          *foundResponse = NULL;
    long                i;
    
    
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
    
    if (foundResponse && !foundResponse->connection)
    {
        ERROR_LOG("Trying to acknowledge event with a NULL connection. ClientInfo %@, Replied: %d\n", foundResponse->clientInfoString, foundResponse->replied);
        return NULL;
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

static void cacheResponseStats(PMResponse *resp)
{
    PMResponseWrangler *respWrangler = resp->myResponseWrangler;
    CFMutableDictionaryRef stats = NULL;
    uint32_t   timeIntervalMS;
    CFStringRef     respType = NULL;

    if (respWrangler->responseStats == NULL)
        return;

    timeIntervalMS = (resp->repliedWhen - resp->notifiedWhen) * 1000;

    if (resp->timedout) {
        respType = CFSTR(kIOPMStatsResponseTimedOut);
    }
    else if (timeIntervalMS > kAppResponseLogThresholdMS) {
        respType = CFSTR(kIOPMStatsResponseSlow);
    }
    else if (gDebugFlags & kIOPMDebugLogCallbacks) {
        respType = CFSTR(kIOPMStatsResponsePrompt);
    }
    else return;

    stats = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (stats == NULL)
        return;

    CFDictionarySetValue(stats, CFSTR(kIOPMStatsApplicationResponseTypeKey), respType);
    CFDictionarySetValue(stats, CFSTR(kIOPMStatsNameKey), resp->connection->callerName);

    CFNumberRef capsNum = CFNumberCreate(NULL, kCFNumberIntType, &resp->notificationType);
    if (capsNum) {
        CFDictionarySetValue(stats, CFSTR(kIOPMStatsPowerCapabilityKey), capsNum);
        CFRelease(capsNum);
    }

    CFNumberRef timeIntervalNumber = CFNumberCreate(NULL, kCFNumberIntType, &timeIntervalMS);
    if (timeIntervalNumber) {
        CFDictionarySetValue(stats, CFSTR(kIOPMStatsTimeMSKey), timeIntervalNumber);
        CFRelease(timeIntervalNumber);
    }

    if (gPowerState & kSleepState) {
        CFDictionarySetValue(stats, CFSTR(kIOPMStatsSystemTransitionKey), CFSTR("Sleep"));
    }
    else if (gPowerState & kDarkWakeState) {
        CFDictionarySetValue(stats, CFSTR(kIOPMStatsSystemTransitionKey), CFSTR("DarkWake"));
    }
    else if (gPowerState & kFullWakeState) {
        CFDictionarySetValue(stats, CFSTR(kIOPMStatsSystemTransitionKey), CFSTR("Wake"));
    }

    CFArrayAppendValue(respWrangler->responseStats, stats);

    CFRelease(stats);
}

kern_return_t _io_pm_connection_acknowledge_event
(
 mach_port_t server,
 audit_token_t   token,
 uint32_t connection_id,
 int messageToken,
 vm_offset_t options_ptr,
 mach_msg_type_number_t options_len,
 int *return_code
 )
{
#if TARGET_OS_IPHONE
	return KERN_FAILURE;
#else
    PMConnection        *connection = connectionForID(connection_id);
    PMResponse          *foundResponse = NULL;
    
    // Check options dictionary
    CFDictionaryRef     ackOptionsDict = NULL;
    CFDateRef           requestDate = NULL;

    if (messageToken == 0)
    {
       /* 
        * This response is for a message for which response is not expected.
        * There is no record mainatained for these messages.
        */
       *return_code = kIOReturnSuccess;
       goto exit;
    }
    
    if (!(foundResponse = _io_pm_acknowledge_event_findOutstandingResponseForToken(connection, messageToken))) {
        *return_code = kIOReturnNotFound;
        goto exit;
    }
    
    *return_code = kIOReturnSuccess;
    foundResponse->repliedWhen = CFAbsoluteTimeGetCurrent();
    foundResponse->replied = true;
    
    cacheResponseStats(foundResponse);
    
    // Unpack the passed-in options data structure
    if ((ackOptionsDict = _io_pm_connection_acknowledge_event_unpack_payload(options_ptr, options_len)))
    {
        if (!CFDictionaryContainsKey(ackOptionsDict, kIOPMAckClientInfoKey) &&
            !CFDictionaryContainsKey(ackOptionsDict, kIOPMAckClientInfoBGTaskKey) &&
            !CFDictionaryContainsKey(ackOptionsDict, kIOPMAckClientInfoAppRefreshKey)) {
            if (foundResponse->connection->callerPID && foundResponse->connection->callerName) {
                ERROR_LOG("IOPMConnectionAcknowledgeEventWithOptions: No client info key present for %d:%@",
                    foundResponse->connection->callerPID, foundResponse->connection->callerName);
            } else {
                ERROR_LOG("IOPMConnectionAcknowledgeEventWithOptions: No client info");
            }
            *return_code = kIOReturnBadArgument;
            goto skip;
        }

        foundResponse->clientInfoString = CFDictionaryGetValue(ackOptionsDict, kIOPMAckClientInfoKey);
        if (foundResponse->clientInfoString) {
            CFRetain(foundResponse->clientInfoString);
        }

        /*
         * mDNSResponder requests a maintenance wake for DHCP renewal.
         *  - Schedules on AC
         *  - Schedules on battery only if TCPKA is active
         *
         * kIOPMAckDHCPRenewWakeDate
         */
#if !TARGET_OS_IPHONE
        requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckDHCPRenewWakeDate));
        CFDateRef  requestBeaconDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckLimitedPowerWakeDate));

        if (requestDate) {
            if ((kACPowered == _getPowerSource())
                || (getTCPKeepAliveState(NULL, 0) == kActive))
            {
                foundResponse->maintenanceRequested = CFDateGetAbsoluteTime(requestDate);
            }
        } else if (requestBeaconDate) {
            /*
             * Bluetooth requests a maintenance wake for Beacon
             *  - Schedules on battery only if appropriate entitlement is present
             */
            if (auditTokenHasEntitlement(token, kIOPMLimitedPowerWakeRequestEntitlement))
            {
                foundResponse->maintenanceRequested = CFDateGetAbsoluteTime(requestBeaconDate);
            }
        }
        else {
#endif
            /*
             * Caller requests a maintenance wake. These schedule on AC only.
             *
             * kIOPMAckWakeDate
             * kIOPMAckNetworkMaintenanceWakeDate
             * kIOPMAckSUWakeDate
             */
            requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckWakeDate));
            if (!requestDate) {
                requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckNetworkMaintenanceWakeDate));
            }

            if (!requestDate) {
                requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckSUWakeDate));
            }

            if (requestDate
                && (kACPowered == _getPowerSource()))
            {
                foundResponse->maintenanceRequested = CFDateGetAbsoluteTime(requestDate);
            }
#if !TARGET_OS_IPHONE
        }
#endif

        /* kIOPMAckBackgroundTaskWakeDate
         *      - Timer plugin uses this to schedule DWBT timed work.
         *      - Schedules on AC only
         *      - Schedules on SilentRunning machines only
         */
        requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckBackgroundTaskWakeDate));
        
        if (requestDate && _DWBT_allowed()) 
        {
            foundResponse->clientInfoStringBGTask = CFDictionaryGetValue(ackOptionsDict, kIOPMAckClientInfoBGTaskKey);
            if (foundResponse->clientInfoStringBGTask) {
                CFRetain(foundResponse->clientInfoStringBGTask);
            }

            foundResponse->timerPluginRequested = CFDateGetAbsoluteTime(requestDate);
#if !TARGET_OS_IPHONE
            if ( foundResponse->timerPluginRequested < ts_nextPowerNap )
                foundResponse->timerPluginRequested = ts_nextPowerNap;
#endif
        }
        
        /* kIOPMAckAppRefreshWakeDate
         *      - CTS uses this to schedule SleepService work.
         *      - Schedules on AC & Battery
         *      - Schedules on SilentRunning/PowerNap supported machines only
         */
        requestDate = isA_CFDate(CFDictionaryGetValue(ackOptionsDict, kIOPMAckAppRefreshWakeDate));
        
        if (requestDate && _SS_allowed())
        {
            foundResponse->clientInfoStringAppRefresh = CFDictionaryGetValue(ackOptionsDict, kIOPMAckClientInfoAppRefreshKey);
            if (foundResponse->clientInfoStringAppRefresh) {
                CFRetain(foundResponse->clientInfoStringAppRefresh);
            }

            foundResponse->sleepServiceRequested = CFDateGetAbsoluteTime(requestDate);
#if !TARGET_OS_IPHONE
            if ( foundResponse->sleepServiceRequested < ts_nextPowerNap )
                foundResponse->sleepServiceRequested = ts_nextPowerNap;
#endif
        }
skip:
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

kern_return_t _io_pm_set_debug_flags(
        mach_port_t     server,
        audit_token_t   token,
        uint32_t    newFlags,
        uint32_t    setMode,
        uint32_t    *oldFlags,
        int         *return_code )
{
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);
    *oldFlags = 0;
    if ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID) ))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }

    if (oldFlags) 
        *oldFlags = gDebugFlags;

    switch (setMode) {
    case kIOPMDebugFlagsSetBits:
        gDebugFlags |= newFlags;
        break;

    case kIOPMDebugFlagsResetBits:
        gDebugFlags &= ~newFlags;
        break;

    case kIOPMDebugFlagsSetValue:
        gDebugFlags = newFlags;
        break;

    default:
        break;
    }

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
    *oldInterval = 0;
#if !TARGET_OS_IPHONE
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

#if !TARGET_OS_IPHONE
exit:
#endif
    return KERN_SUCCESS;
}

#if !TARGET_OS_IPHONE
int getBTWakeInterval()
{
    return kPMSleepDurationForBT;
}

void setDwlInterval(uint32_t interval)
{

    if (interval > 60) {
        // Ignore ridiculously large values
        interval = 60;
    }
    kPMDarkWakeLingerDuration = interval;
}
#endif

kern_return_t _io_pm_set_dw_linger_interval
(
    mach_port_t server,
    audit_token_t   token,
    uint32_t    newInterval,
    int         *return_code
)
{
#if !TARGET_OS_IPHONE
    pid_t               callerPID = -1;
    uid_t               callerUID = -1;
    gid_t               callerGID = -1;

    audit_token_to_au32(token, NULL, NULL, NULL, &callerUID, &callerGID, &callerPID, NULL, NULL);
    if ( !(callerIsRoot(callerUID) || callerIsAdmin(callerUID, callerGID) ))
    {
        *return_code = kIOReturnNotPrivileged;
        goto exit;
    }
    CFNumberRef numRef = CFNumberCreate(0, kCFNumberIntType, &newInterval);
    if (!numRef) {
        *return_code = kIOReturnError;
    }
    else {
        // Set the value to system settings, to persist across reboot
        *return_code = IOPMSetSystemPowerSetting(CFSTR(kIOPMDarkWakeLingerDurationKey), numRef);
        CFRelease(numRef);
    }
exit:
#else
    *return_code = kIOReturnUnsupported;
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
    CFIndex                 i, allResponsesCount = 0;
    CFIndex                 index;
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

    if (reap->procExit) {
        dispatch_source_cancel(reap->procExit);
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
    CFIndex         i,  connectionsCount;
    CFIndex         responseCount;

    long nextAcknowledgementID;
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
                if (purgeMe->clientInfoStringBGTask)
                    CFRelease(purgeMe->clientInfoStringBGTask);
                if (purgeMe->clientInfoStringAppRefresh)
                    CFRelease(purgeMe->clientInfoStringAppRefresh);
                
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

bool PMConnectionHandleDeadName(uint32_t connection_id)
{
    PMConnection    *the_connection = NULL;
    
    the_connection = connectionForID(connection_id);
    
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
#if !TARGET_OS_IPHONE
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

#if !TARGET_OS_IPHONE
static void scheduleSleepServiceCapTimerEnforcer(uint32_t cap_ms) 
{
    if(!cap_ms)
        return;

    
    // Don't allow SS sessions when 'PreventSystemSleep' assertions are held
    if (checkForActivesByType(kPreventSleepType)) 
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
    configAssertionType(kPushServiceTaskType, false);
    configAssertionType(kInteractivePushServiceType, false);

    // create assertion in sync
    int assertion_timeout = (kPMAlwaysOn) ? 2 : 10;
    IOPMAssertionID assertionID = kIOPMNullAssertionID;
    InternalCreateAssertionWithTimeout(kIOPMAssertionTypeApplePushServiceTask,
                    CFSTR("Powerd - Wait for client pushService assertions"), assertion_timeout, &assertionID);


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

static void updateCapabilitiesToAllowBackgroundTasks()
{
    /* Its been kPMSleepDurationForBT since sleep. We can allow background task if not already
     * allowed in Dark Wake
     */
    IOPMCapabilityBits deliverCapabilityBits;
    if ((gCurrentCapabilityBits & kIOPMSystemCapabilityCPU) &&
        !(gCurrentCapabilityBits & (kIOPMSystemCapabilityGraphics| kIOPMSystemCapabilityAudio))){
        // in Dark Wake
        if (_DWBT_allowed() && gCurrentSilentRunningState) {
            if (!(gCurrentCapabilityBits & kIOPMCapabilityBackgroundTask)){
                deliverCapabilityBits = gCurrentCapabilityBits | kIOPMCapabilityBackgroundTask;
                // deliver capabilities notification
                sendNoRespNotificationToInterestedClients(deliverCapabilityBits);
                INFO_LOG("Capabilities changed to include BackgroundTask\n");
            }
        }
    }
}

void cancelDarkWakeCapabilitiesTimer()
{
    // cancel update capabilities timer once in full wake
    if (gDarkWakeCapabilitiesCheckTimer) {
        INFO_LOG("Cancelling dark wake update capabilities timer\n");
        dispatch_source_cancel(gDarkWakeCapabilitiesCheckTimer);
    }

}
#endif


#if LOG_SLEEPSERVICES

/*****************************************************************************/
/* LOG ASL MESSAGE SLEEPSERVICE - Begins */
/*****************************************************************************/

#if !TARGET_OS_IPHONE

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
    asl_release(m);
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
    configAssertionType(kPushServiceTaskType, false);
    configAssertionType(kInteractivePushServiceType, false);

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
    asl_release(m);
    
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
#ifdef XCTEST
void xctSetPowerState(uint32_t powerState)
{
    gPowerState = powerState;
}
#endif

__private_extern__ bool isA_SleepState()
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

__private_extern__ bool isA_FullWake()
{
   if (gPowerState & kFullWakeState)
      return true;

   return false;

}

__private_extern__ bool isCapabilityChangeDone()
{
    return gCapabilityChangeDone;
}

__private_extern__ void cancelPowerNapStates( )
{

#if !TARGET_OS_IPHONE
    if (isA_SleepSrvcWake()) {
        gSleepService.capTime = 0;
        setSleepServicesTimeCap(0);
    }

    if (isA_BTMtnceWake() ) {
        gPowerState &= ~kDarkWakeForBTState;
        configAssertionType(kBackgroundTaskType, false);
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
__private_extern__ kern_return_t
getPlatformSleepType(uint32_t *sleepType, uint32_t *standbyTimer)
{

    IOReturn                    ret;
    uint32_t        outputScalarCnt = 2;
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

    if (kIOReturnSuccess == ret) {
        if (sleepType) {
            *sleepType = (uint32_t)outs[0];
        }
        if (standbyTimer) {
            *standbyTimer = (uint32_t)outs[1];
        }
    }
        

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
    getPlatformSleepType( &sleepType, NULL );
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
     * If there are any assertions of type 'kPreventSleepType', then system is in
     * server mode, which again make system not auto-power off capable.
     *
     * So, disabling 'kBackgroundTaskType' & 'kPushServiceTaskType' is good enough
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
                0, _getPMMainQueue());
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

#if !TARGET_OS_IPHONE
    if (gSleepService.capTime != 0 &&
        isA_SleepSrvcWake()) {
        int capTimeout = getCurrentSleepServiceCapTimeout();
        gSleepService.capTime = capTimeout;
        setSleepServicesTimeCap(capTimeout);
    }

    if (pwrSrc == kBatteryPowered)
        return;

    if ((pwrSrc == kACPowered) && gApoDispatch ) {
        setAutoPowerOffTimer(false, 0);
    }
#endif
}

__private_extern__ void InternalEvalConnections(void)
{
    dispatch_async(_getPMMainQueue(), ^{
        evalForPSChange();
    });
}

static void handleLastCallMsg(void *messageData, CFStringRef sleepReason)
{
    bool tcpka_active = false;
    bool sys_active = false;
    bool pending_wakes = false;

#if !TARGET_OS_IPHONE
    tcpka_active = ((getTCPKeepAliveState(NULL, 0) == kActive) && checkForActivesByType(kInteractivePushServiceType));
#endif

    sys_active = checkForActivesByType(kDeclareSystemActivityType);
    pending_wakes = (CFStringCompare(sleepReason, CFSTR(kIOPMIdleSleepKey), 0) == kCFCompareEqualTo) &&
                    checkPendingWakeReqs(CHECK_UPCOMING|CHECK_EXPIRED|ALLOW_PURGING);

    if( tcpka_active || sys_active || pending_wakes)
    {
        logASLMessageSleepCanceledAtLastCall(tcpka_active, sys_active, pending_wakes);
#if !TARGET_OS_IPHONE
        /* Log all assertions that could have canceled sleep */
        dispatch_async(_getPMMainQueue(), ^{
            logASLAssertionTypeSummary(kInteractivePushServiceType);
            logASLAssertionTypeSummary(kDeclareSystemActivityType);
        });
#endif

        IOCancelPowerChange(gRootDomainConnect, (long)messageData);
    }
    else
    {
        IOAllowPowerChange(gRootDomainConnect, (long)messageData);
        gMachineStateRevertible = false;
        lastcall_ts = mach_absolute_time();
    }

}

static void handleCanSleepMsg(void *messageData, CFStringRef sleepReason)
{
    bool allow_sleep = true;
    bool idle_sleep = false;

    idle_sleep = (CFStringCompare(sleepReason, CFSTR(kIOPMIdleSleepKey), 0) == kCFCompareEqualTo);
#if !TARGET_OS_IPHONE
    /* Check for active TTYs due to ssh sessiosn */
    allow_sleep = TTYKeepAwakeConsiderAssertion( );
#endif        
    if (allow_sleep && idle_sleep) {
        if (!isA_DarkWakeState()) {
            // If idle sleep and transition is from full wake -> darkwake
            // check if sleep has to be prevented for upcoming sleep request
            allow_sleep = (checkPendingWakeReqs(CHECK_UPCOMING|PREVENT_PURGING) ? false : true);
        }
        allow_sleep &= checkForActivesByType(kDeclareSystemActivityType) ? false : true;
    }

    if (allow_sleep) {
        lastcall_ts = mach_absolute_time();
        gMachineStateRevertible = false;
        IOAllowPowerChange(gRootDomainConnect, (long)messageData);                
    }
    else
        IOCancelPowerChange(gRootDomainConnect, (long)messageData);                
}

static void setInitialSleepPreventersCount(int type)
{
    char        *notify_str;
    int         *token = NULL;
    uint32_t    status;
    CFArrayRef  preventers;
    IOReturn    ret;
    long        count;

    if (type == kIOPMIdleSleepPreventers)
    {
        notify_str = kIOPMIdleSleepPreventersNotifyName;
        token = &gIdleSleepPreventersToken;
    }
    else if (type == kIOPMSystemSleepPreventers)
    {
        notify_str = kIOPMSystemSleepPreventersNotifyName;
        token = &gSystemSleepPreventersToken;
    }
    else
    {
        return;
    }
    if (*token == 0)
    {
        status = notify_register_check(notify_str, token);
        if ((status != NOTIFY_STATUS_OK) || (*token == 0))
        {
            *token = 0;
            return;
        }
    }

    ret = IOPMCopySleepPreventersList(type, &preventers);
    if (ret != kIOReturnSuccess)
    {
       return;
    }
    if (!isA_CFArray(preventers))
    {
        count = 0;
    }
    else
    {
        count = CFArrayGetCount(preventers);
        CFRelease(preventers);
    }

    notify_set_state(*token, count);
    notify_post(notify_str);

}


static void handleSleepPreventersMsg(natural_t msg, void *messageData)
{
    char        *notify_str;
    int         *token = NULL;
    uint32_t    status;
    bool        idleChange = false;
    bool        systemChange = false;

    static  unsigned int idleCount = 0;
    static  unsigned int systemCount = 0;

    if (msg == kIOPMMessageIdleSleepPreventers)
    {
        notify_str = kIOPMIdleSleepPreventersNotifyName;
        token = &gIdleSleepPreventersToken;
        if (idleCount != ((unsigned int )messageData)) {
            idleChange = true;
        }
        idleCount = ((unsigned int )messageData);
    }
    else if (msg == kIOPMMessageSystemSleepPreventers)
    {
        notify_str = kIOPMSystemSleepPreventersNotifyName;
        token = &gSystemSleepPreventersToken;
        if (systemCount != ((unsigned int )messageData)) {
            systemChange = true;
        }
        systemCount = ((unsigned int )messageData);
    }
    else
    {
        return;
    }

    if (*token == 0)
    {
        status = notify_register_check(notify_str, token);
        if (status != NOTIFY_STATUS_OK)
        {
            *token = 0;
        }
    }
    if (*token)
    {
        notify_set_state(*token, ((unsigned int )messageData));
        notify_post(notify_str);
    }

    if (idleChange)
    {
        logASLSleepPreventers(kIOPMIdleSleepPreventers);
        logChangedSleepPreventers(kIOPMIdleSleepPreventers);
    }
    if (systemChange)
    {
        logASLSleepPreventers(kIOPMSystemSleepPreventers);
        logChangedSleepPreventers(kIOPMSystemSleepPreventers);
    }
}

#if !TARGET_OS_IPHONE
static bool dwLinger(CFStringRef sleepReason)
{

    // Dark Wake Linger: After going from FullWake to DarkWake during a
    // demand sleep, linger in darkwake for a certain amount of seconds
    //
    // Power Nap machines will linger on every FullWake --> DarkWake
    // transition except emergency sleeps.
    // Non-Power Nap machines will linger on every FullWake --> DarkWake
    // transition except emergency sleeps, and clamshell close sleeps


    bool isBTCapable = IOPMFeatureIsAvailable(CFSTR(kIOPMDarkWakeBackgroundTaskKey), NULL);

    bool isEmergencySleep = IS_EMERGENCY_SLEEP(sleepReason);
    bool isClamshellSleep = CFEqual(sleepReason, CFSTR(kIOPMClamshellSleepKey))?true:false;

    if ((getSessionUserActivity(NULL) == false) ||
        isEmergencySleep ||
        (kPMDarkWakeLingerDuration == 0) ) {
        // Don't linger if:
        //      user wasn't active in last full wake, or
        //      this is an emrgency sleep, or
        //      user has set linger duration to 0
        return false;
    }

    if((isBTCapable || (!isBTCapable && !isClamshellSleep)))
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

            return true;
        }
    }

    return false;
}

void setVMDarkwakeMode(bool darkwakeMode)
{
    int newValue = darkwakeMode ? 1 : 0;
    int oldValue;
    size_t oldValueSize = sizeof(oldValue);
    sysctlbyname("vm.darkwake_mode", &oldValue, &oldValueSize, &newValue, sizeof(newValue));
    INFO_LOG("vm.darkwake_mode: %i -> %i", oldValue, newValue);
}

#endif

void updateWakeTime()
{
    if (gCapabilityChangeDone == true) {
        if (gWakeFromDarkWake && gCurrentWakeTime == 0) {
            CFStringRef wakeType = NULL;
            getPlatformWakeReason(NULL, &wakeType);
            if (wakeType && CFEqual(wakeType, kIOPMRootDomainWakeTypeNotification)) {
                return;
            }
            uint64_t useractive = 0;
            size_t size = sizeof(useractive);
            sysctlbyname("kern.useractive_abs_time", &useractive, &size, NULL, 0);
            DEBUG_LOG("WakeTime: useractive %llu\n", useractive);

            gCurrentWakeTime = intervalInNanoseconds(useractive, gCurrentWakeEnd);
            logASLMessageWakeTime(gCurrentWakeTime, kIsDarkToFullWake);
        } else {
            if (gCurrentWakeStart != 0 && gCurrentWakeTime == 0) {
                // Calendar re sync is done
                gCurrentWakeTime = intervalInNanoseconds(gCurrentWakeStart, gCurrentWakeEnd);
                if (gIsDarkWake)
                    logASLMessageWakeTime(gCurrentWakeTime, kIsDarkWake);
                else
                    logASLMessageWakeTime(gCurrentWakeTime, kIsFullWake);
            } else {
                DEBUG_LOG("WakeTime: calendar hasn't resynced yet\n");
            }
        }
    } else {
        DEBUG_LOG("WakeTime: capabilityChange isn't done yet\n");
    }
}


void updateCurrentWakeStart(uint64_t timestamp)
{
    INFO_LOG("Updating wake start timestamp to %llu\n", timestamp);
    gCurrentWakeStart = timestamp;
}

void updateCurrentWakeEnd(uint64_t timestamp)
{
    INFO_LOG("Updating wake end timestamp to %llu\n", timestamp);
    gCurrentWakeEnd = timestamp;
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
    CFStringRef sleepReason = _getSleepReason();
    static bool slp_logd = false;
    static bool s02dw_logd = false;
    static bool slp2dw_logd = false;
#if !TARGET_OS_IPHONE
    static IOPMAssertionID requestUserActiveID = kIOPMNullAssertionID;
    CFArrayRef appStats = NULL;
#endif
    CFStringRef wakeType = CFSTR("");
    CFStringRef wakeReason = CFSTR("");

    if(inMessageType == kIOPMMessageLastCallBeforeSleep)
    {
        handleLastCallMsg(messageData, sleepReason);
        return;
    }
    else if (inMessageType == kIOMessageCanSystemSleep)
    {
        sleepReason = _updateSleepReason();
        handleCanSleepMsg(messageData, sleepReason);
        return;
    }
    else if (inMessageType == kIOMessageSystemWillNotSleep)
    {
        gMachineStateRevertible = true;
        checkPendingWakeReqs(ALLOW_PURGING);
        return;
    }
    else if ((inMessageType == kIOPMMessageIdleSleepPreventers) ||
        (inMessageType == kIOPMMessageSystemSleepPreventers))
    {
        dispatch_async(_getPMMainQueue(),
                       ^{handleSleepPreventersMsg(inMessageType, messageData);});
        return;
    }
    else if (inMessageType == kIOPMMessageProModeStateChange)
    {
		if ((int)messageData)
			notify_post(kIOPMSystemProModeEngaged);
		else
			notify_post(kIOPMSystemProModeDisengaged);

        INFO_LOG("Notified clients of ProMode state change to %d\n", (int)messageData);

        return;
    }
    else if (inMessageType == kIOPMMessageRequestUserActive)
    {
        char *reason = (char *)messageData;
        INFO_LOG("Request user active from kernel driver %s", reason);
#if !TARGET_OS_IPHONE
        CFStringRef description = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("com.apple.powermanagement.kernel.useractive %s"), reason);
        if (description) {
            InternalDeclareUserActive(description, &requestUserActiveID);
            CFRelease(description);
        }
#endif // !TARGET_OS_IPHONE
        return;
    }
    else if (kIOMessageSystemCapabilityChange != inMessageType)
    {
        ERROR_LOG("Unhandled inMessageType (%x)\n", inMessageType);
        return;
    }

    capArgs = (typeof(capArgs)) messageData;

    AutoWakeCapabilitiesNotification(capArgs->fromCapabilities, capArgs->toCapabilities);
    ClockSleepWakeNotification(capArgs->fromCapabilities, capArgs->toCapabilities,
                               capArgs->changeFlags);
    PMSettingsCapabilityChangeNotification(capArgs);

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
        // clear all wake time bookkeeping
        gIsDarkWake = false;
        gWakeFromDarkWake = false;
        gCurrentWakeStart = 0;
        gCurrentWakeEnd = 0;
        gCapabilityChangeDone = false;
        gCurrentWakeTime = 0;

        // Send an async notify out - clients include SCNetworkReachability API's; no ack expected
        setSystemSleepStateTracking(0);
        notify_post(kIOPMSystemPowerStateNotify);

        _resetWakeReason();
#if !TARGET_OS_IPHONE
        // If this is a non-SR wake, set next earliest PowerNap wake point to
        // after 'kPMSleepDurationForBT' secs.
        // For all other wakes, don't move the next PowerNap wake point.
        if ( ( gCurrentSilentRunningState == kSilentRunningOff ) ||
              CFEqual(sleepReason, CFSTR(kIOPMDarkWakeThermalEmergencyKey))){
           ts_nextPowerNap = CFAbsoluteTimeGetCurrent() + kPMSleepDurationForBT;

        }
        cancelPowerNapStates();

        // set up timer for kPMSleepDurationForBT to update capabilities if needed
        if (!gDarkWakeCapabilitiesCheckTimer) {
            gDarkWakeCapabilitiesCheckTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());
            dispatch_source_set_event_handler(gDarkWakeCapabilitiesCheckTimer, ^{
                updateCapabilitiesToAllowBackgroundTasks();
            });
            dispatch_source_set_cancel_handler(gDarkWakeCapabilitiesCheckTimer, ^{
                dispatch_release(gDarkWakeCapabilitiesCheckTimer);
                gDarkWakeCapabilitiesCheckTimer = NULL;
            });
            dispatch_resume(gDarkWakeCapabilitiesCheckTimer);
        }
        dispatch_source_set_timer(gDarkWakeCapabilitiesCheckTimer,
                                    dispatch_walltime(DISPATCH_TIME_NOW, kPMSleepDurationForBT*NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);


        if (IS_EMERGENCY_SLEEP(sleepReason))
            disableTCPKeepAlive();

        if ((getSessionUserActivity(NULL) == true) && (_getPowerSource() == kBatteryPowered))
            startTCPKeepAliveExpTimer();

        setStandbyTimer();
        resetSessionUserActivity();
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
        slp_logd = true;

        // Tell flight data recorder we're going to sleep
        recordFDREvent(kFDRBattEventAsync, false);
        recordFDREvent(kFDRSleepEvent, false);

#if !TARGET_OS_IPHONE
        evaluateADS();
#endif
        if(smcSilentRunningSupport() )
           gCurrentSilentRunningState = kSilentRunningOn;

        responseController = connectionFireNotification(_kSleepStateBits, (long)capArgs->notifyRef);




        if (!responseController) {
            // We have zero clients. Acknowledge immediately.            

            checkResponses_ScheduleWakeEvents(NULL);
            IOAllowPowerChange(gRootDomainConnect, (long)capArgs->notifyRef);                
        }

#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
        // disable clamshell sleep as a last resort safety net
        DEBUG_LOG("Disabling clamshell sleep while entering sleep from dark wake");
        disableClamshellSleepState();

        // reset display state
        resetDisplayState();
#endif
        incrementSleepCnt();
        return;
    } else if (SYSTEM_WILL_SLEEP_TO_S0DARK(capArgs))
    {
        // clear all wake time bookkeeping
        gIsDarkWake = false;
        gWakeFromDarkWake = false;
        gCurrentWakeStart = 0;
        gCurrentWakeEnd = 0;
        gCapabilityChangeDone = false;
        gCurrentWakeTime = 0;


        gPowerState |= kDarkWakeState;
        gPowerState &= ~(kNotificationDisplayWakeState|kFullWakeState);

#if !TARGET_OS_IPHONE

        bool linger = false;
        bool blockedInS0 = systemBlockedInS0Dark();
        setAutoPowerOffTimer(true, 0);
        linger = dwLinger(sleepReason);

        /*
         * This notification is issued before every sleep. Log to asl only if
         * there are assertion that can prevent system from going into S3/S4
         */

        // Take off Audio & Video capabilities. Leave other capabilities as is.
        deliverCapabilityBits = (gCurrentCapabilityBits & ~(kIOPMCapabilityAudio|kIOPMCapabilityVideo));

        if (blockedInS0) {
            logASLMessageSleep(kPMASLSigSuccess, NULL, NULL, kIsDarkWake);
            recordFDREvent(kFDRDarkWakeEvent, false);
            s02dw_logd = true;

            if (_DWBT_allowed( ))
                deliverCapabilityBits |= kIOPMCapabilityBackgroundTask;
        }


        // Send a notification with no expectation for response
        gCurrentCapabilityBits = deliverCapabilityBits;
        sendNoRespNotification(deliverCapabilityBits);
		SPNotifyLeavingFullWake();

#endif
    } else if (SYSTEM_DID_WAKE(capArgs))
    {
        _updateWakeReason(&wakeReason, &wakeType);

        // Update wake end timestamp
        updateCurrentWakeEnd(mach_absolute_time());

        // if wake reason is rtc, track responsible process
        if ((CFStringFind(wakeReason, CFSTR("RTC"), 0).location != kCFNotFound) || (CFStringFind(wakeReason, CFSTR("rtc"), 0).location != kCFNotFound)) {
            if (gPendingScheduledWakeLog) {

                CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
                CFNumberRef cf_wake = isA_CFNumber(CFDictionaryGetValue(gPendingScheduledWakeLog, CFSTR(kIOPMPowerEventTimeKey)));
                CFAbsoluteTime wake_time = 0;
                CFNumberGetValue(cf_wake,kCFNumberDoubleType, &wake_time);
                if ((wake_time > now) && (wake_time - now) >= leeway) {
                    DEBUG_LOG("Not the current wake %f > now %f", wake_time, now);
                } else {
                    if (!gCurrentScheduledWakeLog) {
                        gCurrentScheduledWakeLog = CFDictionaryCreateMutableCopy(NULL, 0, gPendingScheduledWakeLog);
                        // add in the current sleep wake uuid
                        CFStringRef uuid = IOPMSleepWakeCopyUUID();
                        if (uuid) {
                            CFDictionarySetValue(gCurrentScheduledWakeLog, CFSTR(kIOPMSleepWakeUUIDKey), uuid);
                            CFRelease(uuid);
                        }
                    }
                }
            }
        }

        // reset on wake
        if (gPendingScheduledWakeLog) {
            CFRelease(gPendingScheduledWakeLog);
            gPendingScheduledWakeLog = NULL;
        }

        // On a SilentRunningMachine, the assumption is that every wake is
        // Silent until powerd unclamps SilentRunning or unforeseen thermal
        // constraints arise

        if(smcSilentRunningSupport() )
           gCurrentSilentRunningState = kSilentRunningOn;

        gMachineStateRevertible = true;

        BatteryTimeRemainingWakeNotification();

        if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityGraphics))
        {

            // e.g. System has powered into full wake
#if !TARGET_OS_IPHONE

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
                if (slp_logd) {
                    /* This can happen if sleep is cancelled after powerd logs system entering S3 */
                    logASLMessageWake(kPMASLSigSuccess, NULL,  NULL, deliverCapabilityBits, kIsFullWake);
                }

                else if (s02dw_logd || slp2dw_logd) {
                    gWakeFromDarkWake = true;
                    logASLMessageWake(kPMASLSigSuccess, NULL,  NULL, deliverCapabilityBits, kIsDarkToFullWake);
                }
                recordFDREvent(kFDRUserWakeEvent, false);
            } else {
                if (slp_logd) {
                    logASLMessageWake(kPMASLSigSuccess, NULL,  NULL, deliverCapabilityBits, kIsFullWake);
                }
                mt2RecordWakeEvent(kWakeStateFull);
                recordFDREvent(kFDRUserWakeEvent, true);
            }
            slp_logd = s02dw_logd = slp2dw_logd = false;

            // update wake time
            gCapabilityChangeDone = true;
            gCurrentWakeTime = 0;
            updateWakeTime();
#if !TARGET_OS_IPHONE
            evaluateDWThermalMsg();
#endif

        } else if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityCPU))
        {
            gIsDarkWake = true;
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
            
#if !TARGET_OS_IPHONE
            setVMDarkwakeMode(true);

            /* True powernap wake is either Maintenance or SleepService wake
             * caused by RTC
             */
            if ( (cur_time >= ts_nextPowerNap) && (kPMAlwaysOn || 
                    ((CFEqual(wakeType, kIOPMRootDomainWakeTypeMaintenance) || 
                            CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepService)) && (CFStringFind(wakeReason, CFSTR("RTC"), 0).location != kCFNotFound))) ) {
                  if ( _DWBT_allowed() ) {
                      gPowerState |= kDarkWakeForBTState;
                      configAssertionType(kBackgroundTaskType, false);
                      /* Log all background task assertions once again */
                      dispatch_async(_getPMMainQueue(), ^{
                          logASLAssertionTypeSummary(kBackgroundTaskType);
                      });

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
                /* Wake from network activity
                 * Do not set SilentRunning. Let DAS check thermal levels
                 * Allow Background tasks only if cur_time >= ts_nextPowerNap
                 */
                if (_DWBT_allowed()) {
                      if (cur_time >= ts_nextPowerNap) {
                          deliverCapabilityBits |= (kIOPMCapabilityBackgroundTask  |
                                                kIOPMCapabilityPushServiceTask);
                      } else {
                            deliverCapabilityBits |= kIOPMCapabilityPushServiceTask;
                      }
                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());

                }

                else if (_SS_allowed() ) {
                        deliverCapabilityBits |=  kIOPMCapabilityPushServiceTask;

                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());
                }
            }
            else {
               if ( !CFEqual(wakeType, kIOPMrootDomainWakeTypeLowBattery) &&
                    !CFEqual(wakeType, kIOPMRootDomainWakeTypeSleepTimer) ) {

                    if (_SS_allowed() ) {
                        deliverCapabilityBits |=  kIOPMCapabilityPushServiceTask;

                        scheduleSleepServiceCapTimerEnforcer(
                                getCurrentSleepServiceCapTimeout());
                    }
               }
            }
            if (checkForActivesByType(kPreventSleepType)) {
                deliverCapabilityBits &= ~kIOPMCapabilitySilentRunning;
               _unclamp_silent_running(false);
            }

            evaluateADS();
#endif
            if (gApoDispatch && ts_apo && (cur_time > ts_apo - kAutoPowerOffSleepAhead) ) {
                /* Check for auto-power off if required */
                dispatch_suspend(gApoDispatch);
                dispatch_source_set_timer(gApoDispatch, 
                        DISPATCH_TIME_NOW, DISPATCH_TIME_FOREVER, 0);
                dispatch_resume(gApoDispatch);
            }
            if (slp_logd) {
                logASLMessageWake(kPMASLSigSuccess, NULL,  NULL, deliverCapabilityBits, kIsDarkWake);
                slp2dw_logd = true;
                slp_logd = false;
            }
            mt2RecordWakeEvent(kWakeStateDark);
            recordFDREvent(kFDRDarkWakeEvent, true);
            gCapabilityChangeDone = true;
            gCurrentWakeTime = 0;
            updateWakeTime();
#if !TARGET_OS_IPHONE
            evaluateDWThermalMsg();
#endif

        }

#if !TARGET_OS_IPHONE
        if ((kACPowered == _getPowerSource()) && (kPMDarkWakeLingerDuration != 0)
            && (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityCPU))) {
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

        // Send an async notify out - clients include SCNetworkReachability API's; no ack expected
        setSystemSleepStateTracking(deliverCapabilityBits);

        notify_post(kIOPMSystemPowerStateNotify);

        gCurrentCapabilityBits = deliverCapabilityBits;
        sendNoRespNotification(deliverCapabilityBits);

        if (capArgs->notifyRef)
            IOAllowPowerChange(gRootDomainConnect, (long)capArgs->notifyRef);     
                   
#if !TARGET_OS_IPHONE
        SystemLoadSystemPowerStateHasChanged( );

        // Get stats on delays while waking up. This includes delays from previous sleep
        // and the current wake
        appStats = (CFArrayRef)_copyRootDomainProperty(CFSTR(kIOPMSleepStatisticsAppsKey));
        if (appStats) {
            logASLMessageAppStats(appStats, kPMASLDomainKernelClientStats);
            CFRelease(appStats);
        }
#endif
        return;
    }
    else if (SYSTEM_WILL_WAKE(capArgs) )
    {
#if !TARGET_OS_IPHONE
        // reset delta to standby
        resetDeltaToStandby();
        refreshStandbyInactivityPrediction();
#endif
        gMachineStateRevertible = true;

        gPowerState = kDarkWakeState;
        dispatch_async(_getPMMainQueue(), ^{
            configAssertionType(kInteractivePushServiceType, false);
        });

        deliverCapabilityBits = kIOPMEarlyWakeNotification |
              kIOPMCapabilityCPU | kIOPMCapabilityDisk | kIOPMCapabilityNetwork ;

        if (IS_CAP_GAIN(capArgs, kIOPMSystemCapabilityGraphics))
           deliverCapabilityBits |= kIOPMCapabilityVideo | kIOPMCapabilityAudio;

        sendNoRespNotification( deliverCapabilityBits );
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
    int                     affectedBits = 0;
    CFArrayRef              interested = NULL;
    PMConnection            *connection = NULL;
    int                     interestedCount = 0;
    uint32_t                messageToken = 0;
    int                     calloutCount = 0;
    
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

    INFO_LOG("connectionFireNotification: 0x%x\n", interestBitsNotify);
    // We only send state change notifications out to entities interested in the changing
    // bits, or interested in a subset of the changing bits.
    // Any client who is interested in a superset of the changing bits shall not receive
    // a notification.
    //      affectedBits & InterestedBits != 0
    //  and affectedBits & InterestedBits == InterestedBits

    affectedBits = interestBitsNotify ^ gCurrentCapabilityBits;

    gCurrentCapabilityBits = interestBitsNotify;

    interested = createArrayOfConnectionsWithInterest(affectedBits);
    if (!interested) {
        goto exit;
    }
    interestedCount = (int)CFArrayGetCount(interested);
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

    responseWrangler->responseStats =
                    CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
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
         *
         * callCount is incremented by 1 to make sure messageToken is  not NULL
         */
        messageToken = (interestBitsNotify << 16)
                            | (calloutCount+1);

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
    dispatch_source_t   timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());
    dispatch_time_t     when = dispatch_time(DISPATCH_TIME_NOW, responseWrangler->awaitResponsesTimeoutSeconds * NSEC_PER_SEC);
    dispatch_source_set_timer(timer, when, DISPATCH_TIME_FOREVER, 0);
    dispatch_source_set_event_handler(timer, ^{
        responsesTimedOut(responseWrangler);
    });
    responseWrangler->awaitingResponsesTimeout = timer;
    dispatch_activate(timer);

exit:
    if (interested) 
        CFRelease(interested);

    // Record the active wrangler in a global, then clear when reaped.
    if (responseWrangler)
        gLastResponseWrangler = responseWrangler;

    return responseWrangler;
}

/*****************************************************************************/
static void sendNoRespNotification( int interestBitsNotify )
{

    CFIndex                 i, count = 0;
    // Set messageToken to 0, to indicate that we are not interested in response
    uint32_t                messageToken = 0;
    PMConnection            *connection = NULL;

    INFO_LOG("sendNoRespNotification: 0x%x\n", interestBitsNotify);
    count = CFArrayGetCount(gConnections);

    for (i=0; i<count; i++)
    {
        connection = (PMConnection *)CFArrayGetValueAtIndex(gConnections, i);

        if ((connection->interestsBits & interestBitsNotify) == 0) 
           continue;

        if ((MACH_PORT_NULL == connection->notifyPort) ||
            (false == connection->notifyEnable)) {
            continue;
        }

        _sendMachMessage(connection->notifyPort, 
                            0,
                            interestBitsNotify, 
                            messageToken);


        if (gDebugFlags & kIOPMDebugLogCallbacks)
           logASLPMConnectionNotify(connection->callerName, interestBitsNotify );
         
    }


}

static void sendNoRespNotificationToInterestedClients( int interestBitsNotify )
{

    CFIndex                 i, count = 0;
    // Set messageToken to 0, to indicate that we are not interested in response
    uint32_t                messageToken = 0;
    PMConnection            *connection = NULL;
    int                     affectedBits = 0;


    INFO_LOG("sendNoRespNotificationToInterestedClients: 0x%x\n", interestBitsNotify);
    count = CFArrayGetCount(gConnections);

    affectedBits = interestBitsNotify ^ gCurrentCapabilityBits;
    gCurrentCapabilityBits = interestBitsNotify;
    for (i=0; i<count; i++)
    {
        connection = (PMConnection *)CFArrayGetValueAtIndex(gConnections, i);

        if ((connection->interestsBits & affectedBits) == 0) 
           continue;

        if ((MACH_PORT_NULL == connection->notifyPort) ||
            (false == connection->notifyEnable)) {
            continue;
        }

        _sendMachMessage(connection->notifyPort, 
                            0,
                            interestBitsNotify, 
                            messageToken);


        if (gDebugFlags & kIOPMDebugLogCallbacks)
           logASLPMConnectionNotify(connection->callerName, interestBitsNotify );
         
    }


}
/*****************************************************************************/
/*****************************************************************************/

static void responsesTimedOut(PMResponseWrangler  *responseWrangler)
{
    PMResponse          *one_response = NULL;

    CFIndex         i, responsesCount = 0;
#if !TARGET_OS_IPHONE
    char            appName[32];
    char            pidbuf[64];

    pidbuf[0] = 0;
#endif

    if (!responseWrangler)
        return;

    dispatch_source_cancel(responseWrangler->awaitingResponsesTimeout);
    dispatch_release(responseWrangler->awaitingResponsesTimeout);
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
        
        cacheResponseStats(one_response);

#if !TARGET_OS_IPHONE
        if (isA_CFString(one_response->connection->callerName) && 
                CFStringGetCString(one_response->connection->callerName, appName, sizeof(appName), kCFStringEncodingUTF8))
            snprintf(pidbuf, sizeof(pidbuf), "%s %s(%d)",pidbuf, appName, one_response->connection->callerPID);
        else
            snprintf(pidbuf, sizeof(pidbuf), "%s (%d)",pidbuf, one_response->connection->callerPID );
#endif
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

static aslmsg describeWakeRequest(
    aslmsg                  m,
    pid_t                   pid,
    char                    *describeType,
    CFAbsoluteTime          requestedTime,
    CFStringRef             clientInfoString)
{
    static int cnt = 0;
    char    key[50];
    char    value[512];
    int     year, month, day, hour, minute, second;

    if (m == NULL) {
        m = new_msg_pmset_log();
        asl_set(m, kPMASLDomainKey, kPMASLDomainClientWakeRequests);
        cnt = 0;
    }

    key[0] = value[0] = 0;
    proc_name(pid, value, sizeof(value));
    snprintf(key, sizeof(key), "%s%d", KPMASLWakeReqAppNamePrefix, cnt);
    asl_set(m, key, value);

    snprintf(key, sizeof(key), "%s%d", kPMASLWakeReqTypePrefix, cnt);
    asl_set(m, key, describeType);

    snprintf(key, sizeof(key), "%s%d", kPMASLWakeReqTimeDeltaPrefix, cnt);
    snprintf(value, sizeof(value), "%.0f", requestedTime - CFAbsoluteTimeGetCurrent());
    asl_set(m, key, value);

    CFCalendarDecomposeAbsoluteTime(_gregorian(), requestedTime, "yMdHms", &year, &month, &day, &hour, &minute, &second);
    snprintf(key, sizeof(key), "%s%d", kPMASLWakeReqTimePrefix, cnt);
    snprintf(value, sizeof(value), "%d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    asl_set(m, key, value);

    if (isA_CFString(clientInfoString) && 
        (CFStringGetCString(clientInfoString, value, sizeof(value), kCFStringEncodingUTF8))) {
        snprintf(key, sizeof(key), "%s%d", kPMASLWakeReqClientInfoPrefix, cnt);
        asl_set(m, key, value);
    }
    cnt++;

    return m;
}

#pragma mark -
#pragma mark CheckResponses

bool validScheduledDate(CFAbsoluteTime event, int pid)
{
    if (event == 0) {
        return false;
    }
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (event < now) {
        ERROR_LOG("Process %d scheduled a wake in the past %f\n", pid, (event-now));
        return false;
    }
    return true;
}

static bool checkResponses_ScheduleWakeEvents(PMResponseWrangler *wrangler)
{
    CFIndex                 i = 0;
    CFIndex                 responsesCount          = 0;
    bool                    complete                = true;
    PMResponse              *oneResponse            = NULL;
    aslmsg                  m = NULL;
    int                     chosenReq = -1;
    CFAbsoluteTime          userWake = 0.0; 
    CFAbsoluteTime          earliestWake = kCFAbsoluteTimeIntervalSince1904; // Invalid value
    wakeType_e              type = kChooseWakeTypeCount; // Invalid value
    CFBooleanRef            scheduleEvent = kCFBooleanFalse;
    bool                    userWakeReq = false, ssWakeReq = false, disableWakeReq = false;
    int                     reqCnt = 0;


    // for loggging
    CFNumberRef             chosen_cf_pid = NULL;
    int                     chosen_pid = getpid();
    char                    chosen_name[128];
    char                    chosen_info[128];
    CFStringRef             chosen_cf_name = NULL;
    CFStringRef             chosen_type = NULL;
    CFStringRef             chosen_cf_info = NULL;
    PMResponse              *chosen_darkwake = NULL;

    if (wrangler) {
        responsesCount = CFArrayGetCount(wrangler->awaitingResponses);
    }

    if (!gPendingScheduledWakeLog) {
        gPendingScheduledWakeLog = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    for (i=0; i<responsesCount; i++)
    {
        oneResponse = (PMResponse *)CFArrayGetValueAtIndex(wrangler->awaitingResponses, i);
        
        if (!oneResponse->replied) {
            complete = false;
            break;
        }
        
        if (BIT_IS_SET(wrangler->notificationType, kIOPMSystemCapabilityCPU)) {
            // Don't need to look at wake requests if system is not going to sleep
            continue;
        }

        if (oneResponse->connection == NULL) {
            // Requesting process has died. Ignore its request
            continue;
        }

        if (validScheduledDate(oneResponse->maintenanceRequested, oneResponse->connection->callerPID))
        {
            m = describeWakeRequest(m, oneResponse->connection->callerPID,
                                    "Maintenance",
                                    oneResponse->maintenanceRequested,
                                    oneResponse->clientInfoString);
            if (oneResponse->maintenanceRequested < earliestWake)
            {
                earliestWake = oneResponse->maintenanceRequested;
                type = kChooseMaintenance;
                chosenReq = reqCnt;
                chosen_darkwake = oneResponse;
            }
            reqCnt++;
        }

        if (validScheduledDate(oneResponse->sleepServiceRequested, oneResponse->connection->callerPID))
        {
            if (isA_CFString(oneResponse->clientInfoStringAppRefresh)) {
                m = describeWakeRequest(m, oneResponse->connection->callerPID,
                                        "SleepService",
                                        oneResponse->sleepServiceRequested,
                                        oneResponse->clientInfoStringAppRefresh);
            } else {
                m = describeWakeRequest(m, oneResponse->connection->callerPID,
                                        "SleepService",
                                        oneResponse->sleepServiceRequested,
                                        oneResponse->clientInfoString);
            }

            if (oneResponse->sleepServiceRequested < earliestWake)
            {
                earliestWake = oneResponse->sleepServiceRequested;
                type = kChooseSleepServiceWake;
                chosenReq = reqCnt;
                chosen_darkwake = oneResponse;
            }
            ssWakeReq = true;
            reqCnt++;
        }

        if (validScheduledDate(oneResponse->timerPluginRequested, oneResponse->connection->callerPID))
        {
            if (isA_CFString(oneResponse->clientInfoStringBGTask)) {
                m = describeWakeRequest(m, oneResponse->connection->callerPID,
                                        "TimerPlugin",
                                        oneResponse->timerPluginRequested,
                                        oneResponse->clientInfoStringBGTask);
            } else {
                m = describeWakeRequest(m, oneResponse->connection->callerPID,
                                        "TimerPlugin",
                                        oneResponse->timerPluginRequested,
                                        oneResponse->clientInfoString);
            }

            if (oneResponse->timerPluginRequested < earliestWake)
            {
                earliestWake = oneResponse->timerPluginRequested;
                type = kChooseTimerPlugin;
                chosenReq = reqCnt;
                chosen_darkwake = oneResponse;
            }
            reqCnt++;
        }
    }
    
    if ( !complete || (wrangler && BIT_IS_SET(wrangler->notificationType, kIOPMSystemCapabilityCPU)))
    {
        // Either some clients haven't responded yet, or
        // system is not going to sleep. So, no need to schedule wake yet
        if (gPendingScheduledWakeLog) {
            CFRelease(gPendingScheduledWakeLog);
            gPendingScheduledWakeLog = NULL;
        }
        goto exit;
    } else {
        if (chosen_darkwake) {
            // we have the selected dark wake from clients
            chosen_pid = chosen_darkwake->connection->callerPID;
            CFStringGetCString(chosen_darkwake->connection->callerName, chosen_name, sizeof(chosen_name), kCFStringEncodingUTF8);
            if (chosen_darkwake->clientInfoString) {
                CFStringGetCString(chosen_darkwake->clientInfoString, chosen_info, sizeof(chosen_info), kCFStringEncodingUTF8);
            } else if (chosen_darkwake->clientInfoStringBGTask) {
                CFStringGetCString(chosen_darkwake->clientInfoStringBGTask, chosen_info, sizeof(chosen_info), kCFStringEncodingUTF8);
            } else if (chosen_darkwake->clientInfoStringAppRefresh) {
                CFStringGetCString(chosen_darkwake->clientInfoStringAppRefresh, chosen_info, sizeof(chosen_info), kCFStringEncodingUTF8);
            }
        }
    }

#if !TARGET_OS_IPHONE && !(TARGET_OS_OSX && TARGET_CPU_ARM64)
    CFAbsoluteTime standbyWakeTime = 0, proxSupportWakeTime = 0;
    if ((standbyWakeTime = getWakeFromStandbyTime())) {
        if (standbyWakeTime < ts_nextPowerNap) {
            standbyWakeTime = ts_nextPowerNap;
        }

        INFO_LOG("Adaptive standby wake request after %f secs\n", standbyWakeTime - CFAbsoluteTimeGetCurrent());
        m = describeWakeRequest(m, getpid(), "AdaptiveWake", standbyWakeTime, NULL);
        if (standbyWakeTime < earliestWake) {
            earliestWake = standbyWakeTime;
            type = kChooseSleepServiceWake;
            chosenReq = reqCnt;
            chosen_pid = getpid();
            snprintf(chosen_name, sizeof(chosen_name), "powerd");
            snprintf(chosen_info, sizeof(chosen_info), "AdaptiveWake");
        }
        reqCnt++;
    }
    if ((proxSupportWakeTime = getNextWakeForProximitySupport())) {
        if (proxSupportWakeTime < ts_nextPowerNap) {
            proxSupportWakeTime = ts_nextPowerNap;
        }

        INFO_LOG("Prox support wake request after %f secs\n", proxSupportWakeTime - CFAbsoluteTimeGetCurrent());
        m = describeWakeRequest(m, getpid(), "ProxWakeSupport", proxSupportWakeTime, NULL);
        if (proxSupportWakeTime < earliestWake) {
            earliestWake = proxSupportWakeTime;
            type = kChooseSleepServiceWake;
            chosenReq = reqCnt;
            chosen_pid = getpid();
            snprintf(chosen_name, sizeof(chosen_name), "powerd");
            snprintf(chosen_info, sizeof(chosen_info), "ProxWakeSupport");
        }
        reqCnt++;
    }


#endif

    if (earliestWake < (CFAbsoluteTimeGetCurrent()+60)) {
        // Make sure that wake request is at least 1 min from now
        earliestWake = (CFAbsoluteTimeGetCurrent()+60);
    }
#if !TARGET_OS_IPHONE

    CFAbsoluteTime ts_tcpka_turnoff = getTcpkaTurnOffTime();
    if (validScheduledDate(ts_tcpka_turnoff, getpid())) {
        m = describeWakeRequest(m, getpid(), "TCPKATurnOff", ts_tcpka_turnoff, NULL);
        if (ts_tcpka_turnoff <= earliestWake) {
            earliestWake = ts_tcpka_turnoff;
            type = kChooseMaintenance;
            chosenReq = reqCnt;
            chosen_pid = getpid();
            snprintf(chosen_name, sizeof(chosen_name), "powerd");
            snprintf(chosen_info, sizeof(chosen_info), "TCPKATurnOff");
            if (earliestWake < (CFAbsoluteTimeGetCurrent()+60)) {
                // Make sure that wake request is at least 1 min from now
                earliestWake = (CFAbsoluteTimeGetCurrent()+60);
            }
        }
        reqCnt++;

    }


    // Disable all dark wake requests if system is going to standby and kIOPMDestroyFVKeyOnStandbyKey
    // is set. This is done by resetting earliestWake to kCFAbsoluteTimeIntervalSince1904
#if !(TARGET_OS_OSX && TARGET_CPU_ARM64)
    bool destroyFVKey = GetSystemPowerSettingBool(CFSTR(kIOPMDestroyFVKeyOnStandbyKey));
    if (getDeltaToStandby() == 0 && destroyFVKey) {
        INFO_LOG("Entering standby and kIOPMDestroyFVKeyOnStandbyKey is set. Disabling dark wakes");
        disableWakeReq = true;
        earliestWake = kCFAbsoluteTimeIntervalSince1904;
        ssWakeReq = false;
        chosenReq = -1;
    }
#endif
#endif

    /* Check for user wake requests in the end to give highest priority, in case of conflict */
    CFDictionaryRef event = copyEarliestRequestAutoWakeEvent();
    if (event)
    {
        userWake = getWakeScheduleTime(event);
    
        if (validScheduledDate(userWake, getpid())) {
            CFStringRef appName;
            CFNumberRef appPID;
            CFMutableStringRef appInfo=NULL;
            appName = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey));
            appPID = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppPIDKey));
            if (isA_CFString(appName) && isA_CFNumber(appPID))
            {
                appInfo = CFStringCreateMutable(NULL, MAXPATHLEN);
                CFStringAppendFormat(appInfo, NULL, CFSTR("%@,%@"),appName, appPID);
            }
            m = describeWakeRequest(m, getpid(), "UserWake", userWake, appInfo);
            
            if (userWake <= earliestWake) {
                earliestWake = userWake;
#if TARGET_OS_OSX
                if (appInfo && CFStringFind(appInfo, CFSTR("com.apple.alarm"), 0).location != kCFNotFound) {
                    INFO_LOG("Wake scheduled by com.apple.alarm");
                    type = kChooseMaintenance;
                } else {
                    type = kChooseFullWake;
                }
#else
                type = kChooseFullWake;
#endif
                chosenReq = reqCnt;
                if (appPID) {
                    CFNumberGetValue(appPID, kCFNumberIntType, &chosen_pid);
                } else {
                    chosen_pid = getpid();
                }
                if (appName) {
                    CFStringGetCString(appName, chosen_name, sizeof(chosen_name), kCFStringEncodingUTF8);
                } else {
                    snprintf(chosen_name, sizeof(chosen_name), "powerd");
                }
                if (appInfo) {
                    CFStringGetCString(appInfo, chosen_info, sizeof(chosen_info), kCFStringEncodingUTF8);
                }
                chosen_type = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTypeKey));
            }
            if (appInfo)
            {
                CFRelease(appInfo);
            }
            userWakeReq = true;
            reqCnt++;
        }
        CFRelease(event);
    }
    
    /* check if there is any shutdown or restart request */
    event = copyEarliestShutdownRestartEvent();
    if (event && !disableWakeReq)
    {
        //wake up a minute before shutdown
        userWake = getWakeScheduleTime(event);
        
        if (validScheduledDate(userWake, getpid())) {
            //schedule a wake for shutdown/restart event
            CFStringRef appName;
            CFNumberRef appPID;
            CFMutableStringRef appInfo=NULL;
            appName = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppNameKey));
            appPID = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventAppPIDKey));
            if (isA_CFString(appName) && isA_CFNumber(appPID))
            {
                appInfo = CFStringCreateMutable(NULL, MAXPATHLEN);
                CFStringAppendFormat(appInfo, NULL, CFSTR("%@,%@"),appName, appPID);
            }
            
            if (userWake <= earliestWake) {
                earliestWake = userWake;
                type = kChooseMaintenance;
                chosenReq = reqCnt;
                m = describeWakeRequest(m, getpid(), "Shutdown/Restart", userWake, appInfo);
                chosen_pid = getpid();
                snprintf(chosen_name, sizeof(chosen_name), "powerd");
                if (appInfo) {
                    CFStringGetCString(appInfo, chosen_info, sizeof(chosen_info), kCFStringEncodingUTF8);
                }
                chosen_type = CFDictionaryGetValue(event, CFSTR(kIOPMPowerEventTypeKey));
            }
            if (appInfo)
            {
                CFRelease(appInfo);
            }
            userWakeReq = true;
            reqCnt++;
        }
        CFRelease(event);
    }

#ifndef TARGET_OS_IPHONE
    uint32_t thermalLevel = getSystemThermalLevel();
    if ((thermalLevel == kIOPMThermalLevelWarning) || (thermalLevel == kIOPMThermalLevelTrap)) {
        // Ignore darkwake requests and user wake requests when thermal state is warning/trap
        earliestWake = kCFAbsoluteTimeIntervalSince1904;
        userWakeReq = ssWakeReq = false;
        if (gPendingScheduledWakeLog) {
            CFRelease(gPendingScheduledWakeLog);
            gPendingScheduledWakeLog = NULL;
        }
    }
#endif

    if (ts_apo != 0) {
        // Report existence of user wake request or SS request to IOPPF(thru rootDomain)
        // This is used in figuring out if Auto Power Off should be scheduled
        if (userWakeReq || ssWakeReq) {
            scheduleEvent = kCFBooleanTrue;
        }
        _setRootDomainProperty(CFSTR(kIOPMUserWakeAlarmScheduledKey), scheduleEvent);
    }

    if (m != NULL) {
        char chosenStr[5];
        snprintf(chosenStr, sizeof(chosenStr), "%d", chosenReq);
        asl_set(m, kPMASLWakeReqChosenIdx, chosenStr);
        asl_send(NULL, m);
    }

    if (earliestWake != kCFAbsoluteTimeIntervalSince1904) {
        // time
        CFNumberRef cf_time = CFNumberCreate(NULL, kCFNumberDoubleType, &earliestWake);
        CFDictionarySetValue(gPendingScheduledWakeLog, CFSTR(kIOPMPowerEventTimeKey), cf_time);
        // pid
        chosen_cf_pid = CFNumberCreate(NULL, kCFNumberIntType, &chosen_pid);
        CFDictionarySetValue(gPendingScheduledWakeLog, CFSTR(kIOPMPowerEventAppPIDKey), chosen_cf_pid);
        //name
        chosen_cf_name= CFStringCreateWithCString(0, chosen_name, kCFStringEncodingUTF8);
        if (chosen_cf_name) {
            CFDictionarySetValue(gPendingScheduledWakeLog, CFSTR(kIOPMPowerEventAppNameKey), chosen_cf_name);
            CFRelease(chosen_cf_name);
        }
        // type
        if (chosen_type) {
            CFDictionarySetValue(gPendingScheduledWakeLog, CFSTR(kIOPMPowerEventTypeKey), chosen_type);
        } else {
            CFDictionarySetValue(gPendingScheduledWakeLog, CFSTR(kIOPMPowerEventTypeKey), CFSTR(kIOPMAutoWake));
        }
        //info
        chosen_cf_info = CFStringCreateWithCString(0, chosen_info, kCFStringEncodingUTF8);
        if (chosen_cf_info) {
            CFDictionarySetValue(gPendingScheduledWakeLog, CFSTR(kIOPMPowerEventInfoKey), chosen_cf_info);
            CFRelease(chosen_cf_info);
        }

        if (cf_time) {
            CFRelease(cf_time);
        }
        if (chosen_cf_pid) {
            CFRelease(chosen_cf_pid);
        }
    } else {
        // not scheduling any wake
        if (gPendingScheduledWakeLog) {
            CFRelease(gPendingScheduledWakeLog);
            gPendingScheduledWakeLog = NULL;
        }
    }

    // clear current entry on sleep
    if (gCurrentScheduledWakeLog){
        CFRelease(gCurrentScheduledWakeLog);
        gCurrentScheduledWakeLog = NULL;
    }

    PMScheduleWakeEventChooseBest(earliestWake, type);

exit:

    if (m != NULL) {
        asl_release(m);
    }
    return complete;
}

static void checkResponses(PMResponseWrangler *wrangler)
{


    if (!checkResponses_ScheduleWakeEvents(wrangler)) {
        // Not all clients acknowledged.
        return;
    }

#if !TARGET_OS_WATCH
    if (wrangler->responseStats) {
        logASLMessageAppStats(wrangler->responseStats, kPMASLDomainPMClientStats);
        CFRelease(wrangler->responseStats);
        wrangler->responseStats = NULL;
    }
#endif

    // Completion: all clients have acknowledged.
    if (wrangler->awaitingResponsesTimeout) {
        dispatch_source_cancel(wrangler->awaitingResponsesTimeout);
        dispatch_release(wrangler->awaitingResponsesTimeout);
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
            
            
static void PMScheduleWakeEventChooseBest(CFAbsoluteTime scheduleTime, wakeType_e type)
{
    CFStringRef     scheduleWakeType                    = NULL;
    CFNumberRef     diff_secs = NULL;
    uint64_t        secs_to_apo = ULONG_MAX;
    CFAbsoluteTime  cur_time = 0.0;
    uint32_t        sleepType = kIOPMSleepTypeInvalid;
    bool            skip_scheduling = false;
    CFBooleanRef    scheduleEvent = kCFBooleanFalse;
    

    if (!VALID_DATE(scheduleTime))
    {
        // Set a huge number for following comaprisions with power off timer
        scheduleTime = kCFAbsoluteTimeIntervalSince1904;
    }


    // Always set the Auto Power off timer
    if ( ts_apo != 0 ) 
    {
        if (ts_apo < scheduleTime+kAutoPowerOffSleepAhead) {

           getPlatformSleepType( &sleepType, NULL );
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
             asl_log(0,0,ASL_LEVEL_ERR, "sleepType:%d APO timer:%lld secs\n", 
                     sleepType, secs_to_apo);
    }

    if ( scheduleTime == kCFAbsoluteTimeIntervalSince1904)
    {
       // Nothing to schedule. bail..
       return;
    }

    // INVARIANT: At least one of the WakeTimes we're evaluating has a valid date
    if ((kChooseMaintenance == type) || (kChooseTimerPlugin == type))
    {
        scheduleWakeType = CFSTR(kIOPMMaintenanceScheduleImmediate);
    } else if (kChooseFullWake == type)
    {
        scheduleWakeType = CFSTR(kIOPMAutoWakeScheduleImmediate);
    } else  if (kChooseSleepServiceWake == type)
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
    CFIndex                 connectionsCount;
    CFIndex                 i;
    
    
    if (0 == interestBits)
        return NULL;

    // *********        
    
    arrayFoundInterests = CFArrayCreateMutable(kCFAllocatorDefault, 0, &_CFArrayConnectionCallBacks);

    connectionsCount = CFArrayGetCount(gConnections);

    for (i=0; i<connectionsCount; i++)
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
        sendNoRespNotificationToInterestedClients(deliverCapabilityBits);
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

__private_extern__ bool isInSilentRunningMode()
{
    return (gCurrentSilentRunningState == kSilentRunningOn);
}

__private_extern__ void _set_sleep_revert(bool state)
{
    gMachineStateRevertible = state;
}

__private_extern__ bool _can_revert_sleep(void)
{
    uint64_t wakeabs_ts = 0;
    size_t size = sizeof(wakeabs_ts);
    int ret;
    bool revertible = gMachineStateRevertible;

    if (!gMachineStateRevertible) {
        /*
         * Checking if this call is made when system is going to sleep or
         * when system is waking up. gMachineStateRevertible will be false
         * when sytem is waking up, until "WillPowerOn" is received and machine
         * sleep state is revertible in that duration.
         */

        ret = sysctlbyname("kern.wake_abs_time", &wakeabs_ts, &size, NULL, 0);
        if (ret) {
            ERROR_LOG("Failed to read sysctl 'kern.wake_abs_time'\n");
            return gMachineStateRevertible;
        }

        if (wakeabs_ts > lastcall_ts) {
            // system is waking up
            revertible = true;
        }
        INFO_LOG("wake_ts:0x%llx lastcall_ts:0x%llx\n", wakeabs_ts, lastcall_ts);
    }
    INFO_LOG("Sleep revert state: %d\n", revertible);
    return revertible;
}


__private_extern__ io_connect_t getRootDomainConnect()
{
   return gRootDomainConnect;
}

/*
 * Returns the per-platform sleep service cap timeout for the current power
 * source in milliseconds
 */
#if !TARGET_OS_IPHONE
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

__private_extern__ void getScheduledWake(xpc_object_t remote, xpc_object_t msg) {
    if (!remote || !msg) {
        ERROR_LOG("Invalid parameters. remote: %@ msg: %@", remote, msg);
        return;
    }

    xpc_object_t reply_msg = xpc_dictionary_create_reply(msg);
    if (!reply_msg) {
        ERROR_LOG("Cannot create reply dictionary");
        return;
    }

    if (gCurrentScheduledWakeLog) {
        xpc_dictionary_set_int64(reply_msg, kMsgReturnCode, kIOReturnSuccess);
        xpc_object_t data = NULL;
        data = _CFXPCCreateXPCObjectFromCFObject(gCurrentScheduledWakeLog);
        xpc_dictionary_set_value(reply_msg,  kIOPMPowerEventDataKey, data);
        if (data) {
            xpc_release(data);
        }

    } else {
        xpc_dictionary_set_int64(reply_msg, kMsgReturnCode, kIOReturnError);
    }
    xpc_connection_send_message(remote, reply_msg);

    if (reply_msg) {
        xpc_release(reply_msg);
    }
}
