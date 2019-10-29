/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#include <Security/SecTask.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>
#include <asl.h>
#include <membership.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <unistd.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <notify.h>
#include <xpc/private.h>
#include <IOKit/pwr_mgt/powermanagement_mig.h>
#include <spawn.h>
#include <spawn_private.h>
#include <crt_externs.h>

#include "Platform.h"
#include "PrivateLib.h"
#include "BatteryTimeRemaining.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMAssertions.h"
#include "adaptiveDisplay.h"

#define kIntegerStringLen               15
extern os_log_t    sleepwake_log;
#undef   LOG_STREAM
#define  LOG_STREAM   sleepwake_log

#include <IOKit/smc/SMCUserClient.h>
#include <systemstats/systemstats.h>


#ifndef kIOHIDIdleTimeKy
#define kIOHIDIdleTimeKey                               "HIDIdleTime"
#endif

#ifndef kIOPMMaintenanceScheduleImmediate
#define kIOPMMaintenanceScheduleImmediate               "MaintenanceImmediate"
#endif


#define kIOPMBatteryWarnSettings   CFSTR("BatteryWarn")

// Duplicating the enum from IOServicePMPrivate.h
enum {
    kDriverCallInformPreChange,
    kDriverCallInformPostChange,
    kDriverCallSetPowerState,
    kRootDomainInformPreChange
};
enum
{
    PowerManagerScheduledShutdown = 1,
    PowerManagerScheduledSleep,
    PowerManagerScheduledRestart
};

/* If the battery doesn't specify an alternative time, we wait 16 seconds
   of ignoring the battery's (or our own) time remaining estimate.
*/
enum
{
    kInvalidWakeSecsDefault = 16
};


#define kPowerManagerActionNotificationName "com.apple.powermanager.action"
#define kPowerManagerActionKey "action"
#define kPowerManagerValueKey "value"

// Track real batteries
static CFMutableSetRef     physicalBatteriesSet = NULL;

static IOPMBattery         **physicalBatteriesArray = NULL;
static CFDictionaryRef     customBatteryProps = NULL;

static int sleepCntSinceBoot = 0;
static int sleepCntSinceFailure = -1;

__private_extern__ bool isDisplayAsleep(void);

// Cached data for LowCapRatio
static time_t               cachedLowCapRatioTime = 0;
static bool                 cachedKeyPresence = false;
static bool                 cachedHasLowCap = false;

// Frequency with which to write out FDR records, in secs
#define kFDRRegularInterval (10*60)
// How long to wait after a power event to write out first FDR record, in secs
// Power events include sleep, wake, AC change etc.
#define kFDRIntervalAfterPE (1*60)

static uint64_t nextFDREventDue = 0;
static void logASLMessageHibernateStatistics(void);

static void logASLAppWakeReason(const char * ident, const char * reason);


typedef struct {
    CFStringRef     sleepReason;
    CFStringRef     platformWakeReason;
    CFStringRef     platformWakeType;
    CFArrayRef      claimedWakeEventsArray;
    CFStringRef     claimedWake;
    CFStringRef     interpretedWake;
    CFMutableArrayRef      appWakeReasonArray;
} PowerEventReasons;

static PowerEventReasons     reasons = {
        CFSTR(""),
        CFSTR(""),
        CFSTR(""),
        NULL,
        NULL,
        NULL,
        NULL
        };

/******
 * Do not remove DUMMY macros
 *
 * The following DUMMY macros aren't used in the source code, they're
 * a placeholder for the 'genstrings' tool to read the strings we're using.
 * 'genstrings' will encode these strings in a Localizable.strings file.
 * Note: if you add to or modify these strings, you must re-run genstrings and
 * replace this project's Localizable.strings file with the output.
 ******/

#define DUMMY_UPS_HEADER(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("WARNING!"), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("Warning!"), \
            NULL);

#define DUMMY_UPS_BODY(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("YOUR COMPUTER IS NOW RUNNING ON UPS BACKUP BATTERY. SAVE YOUR DOCUMENTS AND SHUTDOWN SOON."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("Your computer is now running on UPS backup battery power. Save your documents and shut down soon."), \
            NULL);


#define DUMMY_ASSERTION_STRING_TTY(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("A remote user is connected. That prevents system sleep."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("A remote user is connected. That prevents system sleep."), \
            NULL);

#define DUMMY_CAFFEINATE_REASON_STRING(myBundle) CFCopyLocalizedStringWithDefaultValue( \
            CFSTR("THE CAFFEINATE TOOL IS PREVENTING SLEEP."), \
            CFSTR("Localizable"), \
            myBundle, \
            CFSTR("The caffeinate tool is preventing sleep."), \
            NULL);



// dynamicStoreNotifyCallBack is defined in pmconfigd.c
// is not defined in pmset! so we don't compile this code in pmset.

extern SCDynamicStoreRef                gSCDynamicStore;

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


#ifdef XCTEST

PowerSources xctPowerSource = kACPowered;
uint32_t xctCapacity = 80;

void xctSetPowerSource(PowerSources src) {
    xctPowerSource = src;
}

void xctSetCapacity(uint32_t capacity) {
    xctCapacity = capacity;
}

#endif

__private_extern__ SCDynamicStoreRef _getSharedPMDynamicStore(void)
{
    return gSCDynamicStore;
}

__private_extern__ dispatch_queue_t         _getPMMainQueue(void)
{
    static dispatch_queue_t     pmRLS = NULL;

    if (!pmRLS) {
        pmRLS = dispatch_queue_create_with_target("Power Management main queue", NULL, NULL);
    }

    return pmRLS;
}


asl_object_t getSleepCntObject(char *store)
{
    asl_object_t        response = NULL;
    size_t              endMessageID;

    if (!store) {
        return NULL;
    }
    asl_object_t query = asl_new(ASL_TYPE_LIST);
    if (query != NULL)
    {
		asl_object_t cq = asl_new(ASL_TYPE_QUERY);
        if (cq == NULL) {
            goto exit;
        }
        asl_set_query(cq, ASL_KEY_FACILITY, kPMFacility, ASL_QUERY_OP_EQUAL);
        asl_set_query(cq, kPMASLDomainKey, kPMASLDomainHibernateStatistics, ASL_QUERY_OP_EQUAL);
        asl_append(query, cq);
        asl_release(cq);


        asl_object_t pmstore = asl_open_path(store, 0);
        if (pmstore != NULL) {
            // Don't block for more than a second
            response = asl_match(pmstore, query, &endMessageID, SIZE_MAX, 1, 100000, ASL_MATCH_DIRECTION_REVERSE);
            asl_release(pmstore);
        }
    }

exit:
    if (query) {
        asl_release(query);
    }
    return response;
}


static void initSleepCnt()
{
    int cnt = -1; // Init to invalid value
    size_t msgCnt;
    asl_object_t cq = NULL;
    asl_object_t        msg, msgList = NULL;
    const char *str = NULL;

    if (sleepCntSinceFailure != -1) {
        return;
    }

    msgList = getSleepCntObject(kPMASLStorePath);
    if (msgList == NULL) {
        goto exit;
    }
    msgCnt = asl_count(msgList);
    if (msgCnt <= 0) {
        goto exit;
    }

    msg = asl_get_index(msgList, msgCnt-1);
    if ((str = asl_get(msg, kPMASLSleepCntSinceFailure))) {
        cnt = (int)strtol(str, NULL, 0);
    }

exit:
    if (cq) {
        asl_release(cq);
    }
    if (msgList) {
        asl_release(msgList);
    }
    sleepCntSinceFailure = cnt;
}

__private_extern__ void incrementSleepCnt()
{
    sleepCntSinceBoot++;
    if (sleepCntSinceFailure == -1) {
        initSleepCnt();
    }
    if (sleepCntSinceFailure == -1) {
        sleepCntSinceFailure = 0;
    }
    sleepCntSinceFailure++;
}

__private_extern__ bool auditTokenHasEntitlement(
                                     audit_token_t token,
                                     CFStringRef entitlement)
{
    SecTaskRef task = NULL;
    CFTypeRef val = NULL;
    bool caller_is_allowed = false;
    CFErrorRef      errorp = NULL;
    
    task = SecTaskCreateWithAuditToken(kCFAllocatorDefault, token);
    if (task) {
        val = SecTaskCopyValueForEntitlement(task, entitlement, &errorp);
        CFRelease(task);
        
        if (kCFBooleanTrue == val) {
            caller_is_allowed = true;
        }
        if (val) {
            CFRelease(val);
        }
    }
    return caller_is_allowed;
    
}

#define kIOPMSystemDefaultOverrideKey    "SystemPowerProfileOverrideDict"

__private_extern__ IOReturn
_setRootDomainProperty(
    CFStringRef                 key,
    CFTypeRef                   val)
{
    return IORegistryEntrySetCFProperty(getRootDomain(), key, val);
}

__private_extern__ CFTypeRef
_copyRootDomainProperty(
    CFStringRef                 key)
{
    return IORegistryEntryCreateCFProperty(getRootDomain(), key, kCFAllocatorDefault, 0);
}


__private_extern__ bool
_getUUIDString(
    char *buf,
    int buflen)
{
    bool            ret = false;
    CFStringRef     uuidString = NULL;

    uuidString = IOPMSleepWakeCopyUUID();

    if (uuidString) {
        if (!CFStringGetCString(uuidString, buf, buflen,
                                kCFStringEncodingUTF8))
        {
            goto exit;
        }

        ret = true;
    }
exit:
    if (uuidString) CFRelease(uuidString);
    return ret;
}

__private_extern__ CFStringRef _updateSleepReason( )
{
    io_service_t    iopm_rootdomain_ref = getRootDomain();

    if (reasons.sleepReason) CFRelease(reasons.sleepReason);

    reasons.sleepReason = IORegistryEntryCreateCFProperty(
                            iopm_rootdomain_ref,
                            CFSTR("Last Sleep Reason"),
                            kCFAllocatorDefault, 0);

    if (!isA_CFString(reasons.sleepReason))
        reasons.sleepReason = CFSTR("");

    return reasons.sleepReason;

}

__private_extern__ CFStringRef 
_getSleepReason()
{
    return (reasons.sleepReason);
}

static  bool
_getSleepReasonLogStr(
    char *buf,
    int buflen)
{
    bool            ret = false;
    io_service_t    iopm_rootdomain_ref = getRootDomain();
    CFNumberRef     sleepPID = NULL;
    char            reasonBuf[50];
    int             spid = -1;
    
    sleepPID = (CFNumberRef)IORegistryEntryCreateCFProperty(
                             iopm_rootdomain_ref,
                             CFSTR("SleepRequestedByPID"),
                             kCFAllocatorDefault, 0);
    if (sleepPID && isA_CFNumber(sleepPID)) {
        CFNumberGetValue(sleepPID, kCFNumberIntType, &spid);
    }
    if (reasons.sleepReason && isA_CFString(reasons.sleepReason))
    {
        if (CFStringGetCString(reasons.sleepReason, reasonBuf, sizeof(reasonBuf), kCFStringEncodingUTF8))
        {
            if (!strncmp(kIOPMSoftwareSleepKey, reasonBuf, strlen(kIOPMSoftwareSleepKey)))
            {
                snprintf(buf, buflen, "\'%s pid=%d\'", reasonBuf, spid);
            } else {
                snprintf(buf, buflen, "\'%s\'", reasonBuf);
            }
            ret = true;
        }
    }
    if (sleepPID) CFRelease(sleepPID);
    return ret;
}

__private_extern__ void _resetWakeReason( )
{
    if (reasons.platformWakeReason)         CFRelease(reasons.platformWakeReason);
    if (reasons.platformWakeType)           CFRelease(reasons.platformWakeType);
    if (reasons.claimedWake)        CFRelease(reasons.claimedWake);
    if (isA_CFArray(reasons.claimedWakeEventsArray))   CFRelease(reasons.claimedWakeEventsArray);
    if (reasons.appWakeReasonArray)     CFRelease(reasons.appWakeReasonArray);

    reasons.interpretedWake         = NULL;
    reasons.claimedWake             = NULL;
    reasons.claimedWakeEventsArray  = NULL;
    reasons.platformWakeReason              = CFSTR("");
    reasons.platformWakeType                = CFSTR("");
    reasons.appWakeReasonArray              = NULL;
}

#ifndef  kIOPMDriverWakeEventsKey
#define kIOPMDriverWakeEventsKey            "IOPMDriverWakeEvents"
#define kIOPMWakeEventTimeKey               "Time"
#define kIOPMWakeEventFlagsKey              "Flags"
#define kIOPMWakeEventReasonKey             "Reason"
#define kIOPMWakeEventDetailsKey            "Details"
#endif

#define kWakeEventWiFiPrefix          CFSTR("WiFi")
#define kWakeEventEnetPrefix          CFSTR("Enet")

static CFStringRef claimedReasonFromEventsArray(CFArrayRef wakeEvents)
{
    CFDictionaryRef     eachClaim = NULL;
    CFStringRef         eachReason = NULL;
    int i = 0;
    long  count = 0;

    if (!isA_CFArray(wakeEvents) ||
        !(count = CFArrayGetCount(wakeEvents))) {
        return NULL;
    }

    for (i=0; i<count; i++)
    {
        eachClaim = CFArrayGetValueAtIndex(wakeEvents,i);
        if (!isA_CFDictionary(eachClaim))
            continue;
        eachReason = CFDictionaryGetValue(eachClaim, CFSTR(kIOPMWakeEventReasonKey));
        if (!isA_CFString(eachReason))
            continue;
        if (CFStringHasPrefix(eachReason, kWakeEventWiFiPrefix)
            || CFStringHasPrefix(eachReason, kWakeEventEnetPrefix))
        {
            return CFRetain(eachReason);
        }
    }
    return NULL;
}


__private_extern__ void _updateWakeReason
    (CFStringRef *wakeReason, CFStringRef *wakeType)
{
    if (reasons.platformWakeReason) {
        CFRelease(reasons.platformWakeReason);
        reasons.platformWakeReason = CFSTR("");
    }
    if (reasons.platformWakeType) {
        CFRelease(reasons.platformWakeType);
        reasons.platformWakeType = CFSTR("");
    }
    if (reasons.claimedWakeEventsArray) {
        CFRelease(reasons.claimedWakeEventsArray);
        reasons.claimedWakeEventsArray = NULL;
    }
    if (reasons.claimedWake) {
        CFRelease(reasons.claimedWake);
        reasons.claimedWake = NULL;
    }

    // This property may not exist on all platforms.
    reasons.platformWakeReason = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeReasonKey));
    if (!isA_CFString(reasons.platformWakeReason))
        reasons.platformWakeReason = CFSTR("");

    reasons.platformWakeType = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeTypeKey));
    if (!isA_CFString(reasons.platformWakeType))
        reasons.platformWakeType = CFSTR("");

    reasons.claimedWakeEventsArray = _copyRootDomainProperty(CFSTR(kIOPMDriverWakeEventsKey));
    
    reasons.claimedWake = claimedReasonFromEventsArray(reasons.claimedWakeEventsArray);
    if (reasons.claimedWake) {
        reasons.interpretedWake = reasons.claimedWake;
        mt2PublishSleepWakeInfo(reasons.platformWakeType, reasons.claimedWake, true);
    } else {
        reasons.interpretedWake = reasons.platformWakeType;
        mt2PublishSleepWakeInfo(reasons.platformWakeType, reasons.platformWakeType, true);
    }

    getPlatformWakeReason(wakeReason, wakeType);
    return ;
}

// Checks if the specified wakeReason exists in the current claimed app wake reasons
bool checkForAppWakeReason(CFStringRef wakeReason)
{
    CFIndex count;

    if (!isA_CFArray(reasons.appWakeReasonArray) || !isA_CFString(wakeReason)) {
        return false;
    }
    count = CFArrayGetCount(reasons.appWakeReasonArray);

    for (CFIndex i = 0; i < count; i++) {
        CFTypeRef value = CFArrayGetValueAtIndex(reasons.appWakeReasonArray, i);

        if (isA_CFString(value) && (CFStringCompare(wakeReason, value, 0) == kCFCompareEqualTo)) {
            return true;
        }
    }
    return false;
}

STATIC void setAppWakeReason(CFStringRef reasonStr)
{
    if (reasons.appWakeReasonArray == NULL) {
        reasons.appWakeReasonArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    }
    if (!reasons.appWakeReasonArray) {
        ERROR_LOG("Failed to create array to hold app wake reasons\n");
        return;
    }

    CFArrayAppendValue(reasons.appWakeReasonArray, reasonStr);

}
__private_extern__ void appClaimWakeReason(xpc_connection_t peer, xpc_object_t claim)
{
    const char              *id;
    const char              *reason;
    SecTaskRef secTask = NULL;
    CFTypeRef  entitled_DarkWakeControl = NULL;
    audit_token_t token;
    pid_t   pid;
    CFStringRef reasonStr = NULL;

    if (!claim || !peer) {
        return;
    }
    xpc_connection_get_audit_token(peer, &token);
    pid = xpc_connection_get_pid(peer);
    secTask = SecTaskCreateWithAuditToken(kCFAllocatorDefault, token);
    if (secTask) {
        entitled_DarkWakeControl = SecTaskCopyValueForEntitlement(secTask, kIOPMDarkWakeControlEntitlement, NULL);
    }

    if (!entitled_DarkWakeControl) {
        ERROR_LOG("PID %d is not entitled to set wake reason\n", pid);
        goto exit;
    }

    id = xpc_dictionary_get_string(claim, "identity");
    reason = xpc_dictionary_get_string(claim, "reason");

    logASLAppWakeReason(id, reason);
    INFO_LOG("Wake reason: \"%s\"  identity: \"%s\" \n", reason, id);

    reasonStr = CFStringCreateWithCString(NULL, reason, kCFStringEncodingUTF8);
    if (!reasonStr) {
        ERROR_LOG("Failed to create string to hold app wake reason\n");
        goto exit;
    }
    setAppWakeReason(reasonStr);

exit:
    if (secTask) {
        CFRelease(secTask);
    }
    if (entitled_DarkWakeControl) {
        CFRelease(entitled_DarkWakeControl);
    }
    if (reasonStr) {
        CFRelease(reasonStr);
    }
}

__private_extern__ void getPlatformWakeReason
    (CFStringRef *wakeReason, CFStringRef *wakeType)
{

    if (!isA_CFString(reasons.platformWakeReason))
        reasons.platformWakeReason = CFSTR("");

    if (!isA_CFString(reasons.platformWakeType))
        reasons.platformWakeType = CFSTR("");

    if (wakeReason) *wakeReason = reasons.platformWakeReason;
    if (wakeType) *wakeType = reasons.platformWakeType;
    return ;

}
    

__private_extern__ bool
_getHibernateState(
    uint32_t *hibernateState)
{
    bool            ret = false;
    io_service_t    rootDomain = getRootDomain();
    CFDataRef       hibStateData = NULL;
    uint32_t        *hibStatePtr;

    // This property may not exist on all platforms.
    hibStateData = IORegistryEntryCreateCFProperty(
                        rootDomain,
                        CFSTR(kIOHibernateStateKey),
                        kCFAllocatorDefault, 0);

    if (isA_CFData(hibStateData) &&
        (CFDataGetLength(hibStateData) == sizeof(uint32_t)) &&
        (hibStatePtr = (uint32_t *)CFDataGetBytePtr(hibStateData)))
    {
        *hibernateState = *hibStatePtr;
        ret = true;
    }

    if (hibStateData) CFRelease(hibStateData);
    return ret;
}

__private_extern__
const char *sleepType2String(int sleepType)
{
    const char *string;

        switch (sleepType)
        {
            case kIOPMSleepTypeInvalid:
                string = "Invalid";
                break;
            case kIOPMSleepTypeAbortedSleep:
                string = "Aborted Sleep";
                break;
            case kIOPMSleepTypeNormalSleep:
                string = "Normal Sleep";
                break;
            case kIOPMSleepTypeSafeSleep:
                string = "Safe Sleep";
                break;
            case kIOPMSleepTypeHibernate:
                string = "Hibernate";
                break;
            case kIOPMSleepTypeStandby:
                string = "Standby";
                break;
            case kIOPMSleepTypePowerOff:
                string = "AutoPowerOff";
                break;
            case kIOPMSleepTypeDeepIdle:
                string = "Deep Idle";
                break;
            default:
                string = "Unknown";
                break;
        }
        return string;
}


__private_extern__ int getLastSleepType()
{

    io_service_t    rootDomain = getRootDomain();
    CFNumberRef     sleepTypeNum;

    int sleepType = kIOPMSleepTypeInvalid;

    sleepTypeNum = IORegistryEntryCreateCFProperty(
                        rootDomain,
                        CFSTR(kIOPMSystemSleepTypeKey),
                        kCFAllocatorDefault, 0);

    if (isA_CFNumber(sleepTypeNum))
    {
        CFNumberGetValue(sleepTypeNum, kCFNumberIntType, &sleepType);
    }
    if (sleepTypeNum) {
        CFRelease(sleepTypeNum);
    }

    return sleepType;

}


static const char * getSleepTypeString(void)
{
    return sleepType2String(getLastSleepType());
    return NULL;
}


static void sendNotification(int command)
{
    CFMutableDictionaryRef   dict = NULL;
    int numberOfSeconds = 600;

    CFNumberRef secondsValue = CFNumberCreate( NULL, kCFNumberIntType, &numberOfSeconds );
    CFNumberRef commandValue = CFNumberCreate( NULL, kCFNumberIntType, &command );

    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2,
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(dict, CFSTR(kPowerManagerActionKey), commandValue);
    CFDictionarySetValue(dict, CFSTR(kPowerManagerValueKey), secondsValue);

    CFNotificationCenterPostNotificationWithOptions (
                        CFNotificationCenterGetDistributedCenter(),
                                            CFSTR(kPowerManagerActionNotificationName),
                                            NULL, dict,
                                            (kCFNotificationPostToAllSessions | kCFNotificationDeliverImmediately));
    CFRelease(dict);
    CFRelease(secondsValue);
    CFRelease(commandValue);
}


__private_extern__ void _askNicelyThenShutdownSystem(void)
{
    sendNotification( PowerManagerScheduledShutdown );
}

__private_extern__ void _askNicelyThenSleepSystem(void)
{
    sendNotification( PowerManagerScheduledSleep );
}

__private_extern__ void _askNicelyThenRestartSystem(void)
{
    sendNotification( PowerManagerScheduledRestart );
}





IOReturn _getLowCapRatioTime(CFStringRef batterySerialNumber,
                             boolean_t *hasLowCapRatio,
                             time_t *since)
{
    IOReturn                    ret         = kIOReturnError;
    
    CFNumberRef                 num         = NULL;
    CFDictionaryRef             dict        = NULL;

    if (!hasLowCapRatio || !since || !isA_CFString(batterySerialNumber)) {
        return ret;
    }
    
    *hasLowCapRatio = false;
    *since = 0;
    
    if (cachedKeyPresence) {
        *hasLowCapRatio = cachedHasLowCap;
        *since = cachedLowCapRatioTime;
        ret = kIOReturnSuccess;
        goto exit;
    }

    dict = IOPMCopyFromPrefs(NULL, kIOPMBatteryWarnSettings);
    if (!isA_CFDictionary(dict)) {
        goto exit;
    }

    num = CFDictionaryGetValue(dict, batterySerialNumber);
    if (isA_CFNumber(num)) {
        if (!CFNumberGetValue(num, kCFNumberSInt64Type, since)) {
            goto exit;
        }
        *hasLowCapRatio = true;
        cachedHasLowCap = true;
        cachedLowCapRatioTime = *since;
        // set the flag to indicate the file was read once successfully
        cachedKeyPresence = true;
    }

    
    ret = kIOReturnSuccess;
    
exit:
    if (dict) {
        CFRelease(dict);
    }
    
    return ret;
}

IOReturn _setLowCapRatioTime(CFStringRef batterySerialNumber,
                             boolean_t hasLowCapRatio,
                             time_t since)
{
    IOReturn                    ret         = kIOReturnError;
    
    CFMutableDictionaryRef      dict        = NULL; // must release
    CFNumberRef                 num         = NULL; // must release
    
    if (!isA_CFString(batterySerialNumber))
        goto exit;

    // return early if the cached copy indicates the flag is already set
    if (cachedKeyPresence)  {
        if (hasLowCapRatio == cachedHasLowCap) {
            ret = kIOReturnSuccess;
            goto exit;
        }
    }

    if (hasLowCapRatio) {
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                         0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);

        num = CFNumberCreate(kCFAllocatorDefault,
                             kCFNumberSInt64Type,
                             &since);
        CFDictionarySetValue(dict, batterySerialNumber, num);
        CFRelease(num);

    }
    else {
        // This removes the dictionary from prefs
        dict = NULL;
    }
    ret = IOPMWriteToPrefs(kIOPMBatteryWarnSettings, dict, true, false);

    
exit:
    if (dict)           CFRelease(dict);
    
    return ret;
}

static void _unpackBatteryState(IOPMBattery *b, CFDictionaryRef prop)
{
    CFBooleanRef    boo;
    CFNumberRef     n;

    if(!isA_CFDictionary(prop)) return;

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalConnectedKey));
    b->externalConnected = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSExternalChargeCapableKey));
    b->externalChargeCapable = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryInstalledKey));
    b->isPresent = (kCFBooleanTrue == boo);

    boo = CFDictionaryGetValue(prop, CFSTR(kIOPMPSIsChargingKey));
    b->isCharging = (kCFBooleanTrue == boo);


    b->failureDetected = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSErrorConditionKey));

    b->batterySerialNumber = (CFStringRef)CFDictionaryGetValue(prop, CFSTR("BatterySerialNumber"));

    b->chargeStatus = (CFStringRef)CFDictionaryGetValue(prop, CFSTR(kIOPMPSBatteryChargeStatusKey));
    
    _getLowCapRatioTime(b->batterySerialNumber,
                        &(b->hasLowCapRatio),
                        &(b->lowCapRatioSinceTime));
    
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSVoltageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->voltage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCurrentCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->currentCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSDesignCapacityKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->designCap);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSTimeRemainingKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->hwAverageTR);
    }


    n = CFDictionaryGetValue(prop, CFSTR("InstantAmperage"));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->instantAmperage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSAmperageKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->avgAmperage);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSMaxErrKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->maxerr);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSCycleCountKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->cycleCount);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSLocationKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->location);
    }
    n = CFDictionaryGetValue(prop, CFSTR(kIOPMPSInvalidWakeSecondsKey));
    if(n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->invalidWakeSecs);
    } else {
        b->invalidWakeSecs = kInvalidWakeSecsDefault;
    }
    n = CFDictionaryGetValue(prop, CFSTR("PermanentFailureStatus"));
    if (n) {
        CFNumberGetValue(n, kCFNumberIntType, &b->pfStatus);
    } else {
        b->pfStatus = 0;
    }

    return;
}

/*
 * _batteries
 */
__private_extern__ IOPMBattery **_batteries(void)
{
        return physicalBatteriesArray;
}


__private_extern__ IOPMBattery *_newBatteryFound(io_registry_entry_t where)
{
    IOPMBattery *new_battery = NULL;
    static int new_battery_index = 0;
    // Populate new battery in array
    new_battery = calloc(1, sizeof(IOPMBattery));
    new_battery->me = where;
    new_battery->name = CFStringCreateWithFormat(
                            kCFAllocatorDefault,
                            NULL,
                            CFSTR("InternalBattery-%d"),
                            new_battery_index);

    new_battery_index++;

    /* Real, physical battery found */
    if (!physicalBatteriesSet) {
        physicalBatteriesSet = CFSetCreateMutable(0, 1, NULL);
    }
    CFSetAddValue(physicalBatteriesSet, new_battery);
    physicalBatteriesCount = CFSetGetCount(physicalBatteriesSet);
    if (physicalBatteriesArray) {
        free(physicalBatteriesArray);
        physicalBatteriesArray = NULL;
    }
    physicalBatteriesArray = (IOPMBattery **)calloc(physicalBatteriesCount, sizeof(IOPMBattery *));
    CFSetGetValues(physicalBatteriesSet, (const void **)physicalBatteriesArray);

    return new_battery;
}


__private_extern__ void _batteryChanged(IOPMBattery *changed_battery)
{
    kern_return_t       kr;

    CFMutableDictionaryRef props = NULL;
    CFDictionaryRef adapter = NULL;
    CFBooleanRef externalConnected = kCFBooleanFalse;
    CFBooleanRef battInstalled = kCFBooleanFalse;
    bool newBattery = true;

    if(!changed_battery) {
        // This is unexpected; we're not tracking this battery
        return;
    }

    // Free the last set of properties
    if(changed_battery->properties) {
        CFRelease(changed_battery->properties);
        changed_battery->properties = NULL;
        newBattery = false;
    }
    if (isA_CFDictionary(customBatteryProps)) {
        props = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, customBatteryProps);
    }
    else {

        kr = IORegistryEntryCreateCFProperties(
                changed_battery->me,
                &props,
                kCFAllocatorDefault, 0);
        if(KERN_SUCCESS != kr) {
            goto exit;
        }
    }

    if (CFDictionaryGetValueIfPresent(props, CFSTR(kIOPMPSBatteryInstalledKey), (const void **)&battInstalled) &&
            (battInstalled == kCFBooleanTrue)) {

        _unpackBatteryState(changed_battery, props);
        if (newBattery) {
            // New battery
            initializeBatteryCalculations();
        }
    }
    else {
        // There is no battery here. May be just an adapeter
        INFO_LOG("Battery is not installed\n");
        CFDictionaryGetValueIfPresent(props, CFSTR(kIOPMPSExternalConnectedKey), (const void **)&externalConnected);
        CFDictionaryGetValueIfPresent(props, CFSTR(kIOPMPSAdapterDetailsKey), (const void **)&adapter);

        if (isA_CFDictionary(adapter)) {
            readAndPublishACAdapter((externalConnected == kCFBooleanTrue) ? true : false, adapter);
        }
        CFRelease(props);
        props = NULL;
    }

exit:
    changed_battery->properties = props;
    return;
}

__private_extern__ bool _batteryHas(IOPMBattery *b, CFStringRef property)
{
    if(!property || !b->properties) return false;

    // If the battery's descriptior dictionary has an entry at all for the
    // given 'property' it is supported, i.e. the battery 'has' it.
    return CFDictionaryGetValue(b->properties, property) ? true : false;
}

bool isSenderEntitled(xpc_object_t remoteConnection, CFStringRef entitlementString, bool requireRoot)
{
    audit_token_t token;
    CFTypeRef  entitlement = NULL;
    SecTaskRef secTask = NULL;
    bool entitled = false;

    if (requireRoot && !callerIsRoot(xpc_connection_get_euid(remoteConnection))) {
        goto exit;
    }
    xpc_connection_get_audit_token(remoteConnection, &token);
    secTask = SecTaskCreateWithAuditToken(kCFAllocatorDefault, token);
    if (secTask) {
        entitlement = SecTaskCopyValueForEntitlement(secTask, entitlementString, NULL);
    }

    entitled = (entitlement != NULL);

exit:
    if (secTask) {
        CFRelease(secTask);
    }
    if (entitlement) {
        CFRelease(entitlement);
    }

    return entitled;

}

__private_extern__ void resetCustomBatteryProps(xpc_object_t remoteConnection, xpc_object_t msg)
{

    if (!msg) {
        ERROR_LOG("Invalid message\n");
        return;
    }
    
    xpc_object_t respMsg = xpc_dictionary_create_reply(msg);
    if (respMsg == NULL) {
        ERROR_LOG("Failed to create response message\n");
        return;
    }

    if (!isSenderEntitled(remoteConnection, CFSTR("com.apple.private.iokit.batteryTester"), true)) {
        ERROR_LOG("Ignoring request to reset custom battery properties from unprivileged sender\n");
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnNotPrivileged);
        goto exit;
    }

    if (xpc_dictionary_get_value(msg, kResetCustomBatteryProps) && customBatteryProps) {
        CFRelease(customBatteryProps);
        customBatteryProps = NULL;

        BatterySetNoPoll(false);
        BatteryTimeRemaining_finish();
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnSuccess);
        INFO_LOG("System reset to use default battery properties\n");
    }
exit:
    xpc_connection_send_message(remoteConnection, respMsg);
    xpc_release(respMsg);
}

__private_extern__ void setCustomBatteryProps(xpc_object_t remoteConnection, xpc_object_t msg)
{
    xpc_object_t xpcBatteryProps = NULL;
    CFDictionaryRef batteryProps = NULL;

    if (!msg) {
        ERROR_LOG("Invalid message\n");
        return;
    }
    xpc_object_t respMsg = xpc_dictionary_create_reply(msg);
    if (respMsg == NULL) {
        ERROR_LOG("Failed to create response message\n");
        return;
    }

    if (!isSenderEntitled(remoteConnection, CFSTR("com.apple.private.iokit.batteryTester"), true)) {
        ERROR_LOG("Ignoring custom battery properties message from unprivileged sender\n");
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnNotPrivileged);
        goto exit;
    }

    xpcBatteryProps = xpc_dictionary_get_value(msg, kCustomBatteryProps);
    if (xpcBatteryProps) {
        batteryProps = _CFXPCCreateCFObjectFromXPCObject(xpcBatteryProps);
    }
    if (!isA_CFDictionary(batteryProps)) {
        ERROR_LOG("Received invalid data thru custom battery properties message\n");
        xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnBadArgument);
        goto exit;
    }

    if (customBatteryProps) {
        CFRelease(customBatteryProps);
    }
    INFO_LOG("System updated to use custom battery properties\n");
    BatterySetNoPoll(true);
    customBatteryProps = CFRetain(batteryProps);
    BatteryTimeRemaining_finish();
    xpc_dictionary_set_uint64(respMsg, kMsgReturnCode, kIOReturnSuccess);

exit:
    xpc_connection_send_message(remoteConnection, respMsg);
    if (batteryProps) {
        CFRelease(batteryProps);
    }
    xpc_release(respMsg);
}


#if HAVE_CF_USER_NOTIFICATION

__private_extern__ CFUserNotificationRef _copyUPSWarning(void)
{
    CFMutableDictionaryRef      alert_dict;
    SInt32                      error;
    CFUserNotificationRef       note_ref;
    CFBundleRef                 myBundle;
    CFStringRef                 header_unlocalized;
    CFStringRef                 message_unlocalized;
    CFURLRef                    bundle_url;

    myBundle = CFBundleGetBundleWithIdentifier(kPowerdBundleIdentifier);
    if (!myBundle)
        return NULL;

    alert_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if(!alert_dict)
        return NULL;

    bundle_url = CFBundleCopyBundleURL(myBundle);
    CFDictionarySetValue(alert_dict, kCFUserNotificationLocalizationURLKey, bundle_url);
    CFRelease(bundle_url);

    header_unlocalized = CFSTR("WARNING!");
    message_unlocalized = CFSTR("YOUR COMPUTER IS NOW RUNNING ON UPS BACKUP BATTERY. SAVE YOUR DOCUMENTS AND SHUTDOWN SOON.");

    CFDictionaryAddValue(alert_dict, kCFUserNotificationAlertHeaderKey, header_unlocalized);
    CFDictionaryAddValue(alert_dict, kCFUserNotificationAlertMessageKey, message_unlocalized);

    note_ref = CFUserNotificationCreate(kCFAllocatorDefault, 0, 0, &error, alert_dict);
    CFRelease(alert_dict);

    asl_log(0, 0, ASL_LEVEL_ERR, "PowerManagement: UPS low power warning\n");

    return note_ref;
}

#endif

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/*      AAAAAAAAAAAAAA      SSSSSSSSSSSSSS       LLLLLLLLLLLLLLLL          */
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

__private_extern__ bool getPowerState(PowerSources *source, uint32_t *percentage)
{
    IOPMBattery                                 **batteries;
    int                                         batteryCount = 0;
    uint32_t                                    capPercent = 0;
    int                                         i;
    int                                         validBattCount = 0;
    bool                                        ret = false;

#ifdef XCTEST
    *source = xctPowerSource;
    *percentage = xctCapacity;
    return true;
#endif

    *source = kACPowered;
    batteryCount = _batteryCount();
    if (0 < batteryCount) {
        batteries = _batteries();
        for (i=0; i< batteryCount; i++) {
            if (batteries[i]->isPresent == false) {
                continue;
            }
            validBattCount++;
            if (0 != batteries[i]->maxCap) {
                capPercent += (batteries[i]->currentCap * 100) / batteries[i]->maxCap;
            }
        }
        if (validBattCount) {
            *source = batteries[0]->externalConnected ? kACPowered : kBatteryPowered;
            *percentage = capPercent;
            ret = true;
        }
    }

    return ret;
}


static void printCapabilitiesToBuf(char *buf, int buf_size, IOPMCapabilityBits in_caps)
{
    uint64_t caps = (uint64_t)in_caps;

//    snprintf(buf, buf_size, "%s:%s%s%s%s%s%s%s",
//                             on_sleep_dark,
    snprintf(buf, buf_size, " [%s%s%s%s%s%s%s]",
                             (caps & kIOPMCapabilityCPU) ? "C":"<off> ",
                             (caps & kIOPMCapabilityDisk) ? "D":"",
                             (caps & kIOPMCapabilityNetwork) ? "N":"",
                             (caps & kIOPMCapabilityVideo) ? "V":"",
                             (caps & kIOPMCapabilityAudio) ? "A":"",
                             (caps & kIOPMCapabilityPushServiceTask) ? "P":"",
                             (caps & kIOPMCapabilityBackgroundTask) ? "B":"");
}

__private_extern__ bool isA_installEnvironment()
{
    static int installEnv = -1;

    if (installEnv == -1) {
        installEnv = (getenv("__OSINSTALL_ENVIRONMENT") != NULL) ? 1 : 0;
    }

    return (installEnv);
}

__private_extern__ aslmsg new_msg_pmset_log(void)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);

    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    asl_set(m, ASL_KEY_FACILITY, kPMFacility);

    return m;
}


__private_extern__ void logASLMessagePMStart(void)
{
    aslmsg                  m;
    char                    uuidString[150];

    m = new_msg_pmset_log();

    if (_getUUIDString(uuidString, sizeof(uuidString))) {
        asl_set(m, kPMASLUUIDKey, uuidString);
    }
    asl_set(m, kPMASLDomainKey, kPMASLDomainPMStart);
    asl_set(m, ASL_KEY_MSG, "powerd process is started\n");
    asl_send(NULL, m);
    asl_release(m);

    if (isA_installEnvironment()) {
        syslog(LOG_INFO | LOG_INSTALL, "powerd process is started\n");
    }
}

__private_extern__ void logASLMessageSMCShutdownCause(int shutdownCause)
{
    const char *shutdownCauseString = smcShutdownCauseString(shutdownCause);
    char buf[120];

    snprintf(buf, sizeof(buf), "SMC shutdown cause: %d: %s", shutdownCause, shutdownCauseString);
    INFO_LOG("%s\n", buf);

    aslmsg m;
    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainSMCShutdownCause);
    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_release(m);
}


static void attachTCPKeepAliveKeys(
                                   aslmsg m,
                                   char *tcpString,
                                   unsigned int tcpStringLen)

{
    CFTypeRef           platformSupport = NULL;
    char                keepAliveString[100];
    
    IOPlatformCopyFeatureDefault(kIOPlatformTCPKeepAliveDuringSleep, &platformSupport);
    if (kCFBooleanTrue == platformSupport)
    {
        asl_set(m, kPMASLTCPKeepAlive, "supported");
        
        getTCPKeepAliveState(keepAliveString, sizeof(keepAliveString));

        asl_set(m, kPMASLTCPKeepAliveExpired, keepAliveString);
        snprintf(tcpString, tcpStringLen, "TCPKeepAlive=%s", keepAliveString);
    }

    if (platformSupport){
        CFRelease(platformSupport);
    }
}


__private_extern__ void logASLMessageSleep(
    const char *sig,
    const char *uuidStr,
    const char *failureStr,
    int   sleepType
)
{
    aslmsg                  m;
    char                    uuidString[150];
    char                    source[10];
    uint32_t                percentage = 0;
    char                    numbuf[15];
    bool                    success = true;
    char                    messageString[200];
    char                    reasonString[100];
    char                    tcpKeepAliveString[50];
    PowerSources            pwrSrc;

    numbuf[0] = 0;

    reasonString[0] = messageString[0] = tcpKeepAliveString[0] = source[0] =  0;
    
    _getSleepReasonLogStr(reasonString, sizeof(reasonString));
    
    if (!strncmp(sig, kPMASLSigSuccess, sizeof(kPMASLSigSuccess)))
    {
        success = true;
        if (sleepType == kIsS0Sleep) {
            snprintf(messageString, sizeof(messageString), "Entering Sleep state");
        } else {
           snprintf(messageString, sizeof(messageString), "Entering DarkWake state");
        }

        if (reasonString[0] != 0) {
            snprintf(messageString, sizeof(messageString), "%s due to %s", messageString, reasonString);
        }
        
    } else {
        success = false;
        snprintf(messageString, sizeof(messageString), "Failure during sleep: %s : %s", failureStr, (sig) ? sig : "");
    }

    getPowerState(&pwrSrc, &percentage);
    INFO_LOG("%{public}s", messageString);
    m = new_msg_pmset_log();
    if (success) {
        asl_set(m, kPMASLDomainKey, kPMASLDomainPMSleep);

        snprintf(numbuf, 10, "%d", percentage);
        asl_set(m, kPMASLBatteryPercentageKey, numbuf);

        asl_set(m, kPMASLPowerSourceKey, (pwrSrc == kACPowered) ? "AC" : "Batt");

        attachTCPKeepAliveKeys(m, tcpKeepAliveString, sizeof(tcpKeepAliveString));
        snprintf(messageString, sizeof(messageString), "%s:%s",
                messageString, tcpKeepAliveString);
    }
    else {
        asl_set(m, kPMASLDomainKey, kPMASLDomainSWFailure);
    }

    // UUID
    if (uuidStr) {
        asl_set(m, kPMASLUUIDKey, uuidStr);  // Caller Provided
    } else if (_getUUIDString(uuidString, sizeof(uuidString))) {
        asl_set(m, kPMASLUUIDKey, uuidString);
    }
    
    
    asl_set(m, kPMASLSignatureKey, sig);
    asl_set(m, ASL_KEY_MSG, messageString);
    asl_send(NULL, m);
    asl_release(m);

    if (isA_installEnvironment()) {
        syslog(LOG_INFO | LOG_INSTALL, "%s battCap:%s pwrSrc: %s\n",
                messageString, numbuf, (pwrSrc == kACPowered) ? "AC" : "Batt");
    }
}

/*****************************************************************************/
#pragma mark ASL

__private_extern__ void logASLMessageWake(
    const char *sig,
    const char *uuidStr,
    const char *failureStr,
    IOPMCapabilityBits in_capabilities,
    WakeTypeEnum dark_wake
)
{
    aslmsg                  m;
    char                    numbuf[15];
    CFStringRef             tmpStr = NULL;
    char                    buf[200];
    char                    source[10];
    uint32_t                percentage = 0;
    char                    wakeReasonBuf[50];
    char                    cBuf[50];
    const char *            detailString = NULL;
    static int              darkWakeCnt = 0;
    char                    battCap[15];
    static char             prev_uuid[50];
    CFStringRef             wakeType = NULL;
    const char *            sleepTypeString;
    bool                    success = true;
    PowerSources            pwrSrc;

    m = new_msg_pmset_log();
    asl_set(m, kPMASLSignatureKey, sig);

    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
        if (strncmp(buf, prev_uuid, sizeof(prev_uuid))) {
              // New sleep/wake cycle.
              snprintf(prev_uuid, sizeof(prev_uuid), "%s", buf);
              darkWakeCnt = 0;
        }
    }

    buf[0] = source[0] = battCap[0] = 0;
    getPowerState(&pwrSrc, &percentage);
    if (!strncmp(sig, kPMASLSigSuccess, sizeof(kPMASLSigSuccess)))
    {
        char  wakeTypeBuf[50];

        wakeReasonBuf[0] = wakeTypeBuf[0] = 0;
        if (isA_CFString(reasons.platformWakeReason)) {
            CFStringGetCString(reasons.platformWakeReason, wakeReasonBuf, sizeof(wakeReasonBuf), kCFStringEncodingUTF8);
        }
        if (isA_CFString(reasons.platformWakeType)) {
            CFStringGetCString(reasons.platformWakeType, wakeTypeBuf, sizeof(wakeTypeBuf), kCFStringEncodingUTF8);
        }
        snprintf(wakeReasonBuf, sizeof(wakeReasonBuf), "%s/%s", wakeReasonBuf, wakeTypeBuf);
        detailString = wakeReasonBuf;

        snprintf(battCap, 10, "%d", percentage);
        asl_set(m, kPMASLBatteryPercentageKey, battCap);
        asl_set(m, kPMASLPowerSourceKey, (pwrSrc == kACPowered) ? "AC" : "BATT");
    } else {
        snprintf(buf, sizeof(buf), "Failure during wake: %s : %s", 
                 failureStr, (sig) ? sig : "");
        success = false;

    }

    /* populate driver wake reasons */
    if (success && isA_CFArray(reasons.claimedWakeEventsArray))
    {
        int  keyIndex = 0;
        long    claimedCount = CFArrayGetCount(reasons.claimedWakeEventsArray);

        for (int i=0; i<claimedCount; i++) {
            /* Legal requirement: 16513925 & 16544525
             * Limit the length of the reported string to 60 characters.
             */
            const int               kMaxClaimReportLen = 60;

            CFDictionaryRef         claimedEvent = NULL;
            char                    claimedReasonStr[kMaxClaimReportLen];
            char                    claimedDetailsStr[kMaxClaimReportLen];
            char                    claimed[255];
            char                    key[255];

            claimedReasonStr[0] = 0;
            claimedDetailsStr[0] = 0;
            
            claimedEvent = CFArrayGetValueAtIndex(reasons.claimedWakeEventsArray, i);
            if (!isA_CFDictionary(claimedEvent)) continue;
            
            tmpStr = CFDictionaryGetValue(claimedEvent,
                                          CFSTR(kIOPMWakeEventReasonKey));
            if (isA_CFString(tmpStr)) {
                CFStringGetCString(tmpStr, claimedReasonStr, sizeof(claimedReasonStr), kCFStringEncodingUTF8);
            }
            if (!claimedReasonStr[0]) continue;

            tmpStr = CFDictionaryGetValue(claimedEvent,
                                          CFSTR(kIOPMWakeEventDetailsKey));
            if (isA_CFString(tmpStr)) {
                CFStringGetCString(tmpStr, claimedDetailsStr, sizeof(claimedDetailsStr), kCFStringEncodingUTF8);
            }

            snprintf(claimed, sizeof(claimed), "DriverReason:%s - DriverDetails:%s",
                                                claimedReasonStr, claimedDetailsStr);
            
            snprintf(key, sizeof(key), "%s-%d", kPMASLClaimedEventKey, keyIndex);
            
            asl_set(m, key, claimed);
            keyIndex++;
        }
    }

    if (!success)
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainSWFailure);
    }
    else if (dark_wake == kIsDarkWake)
    {
        darkWakeCnt++;
        snprintf(buf, sizeof(buf), "%s", "DarkWake");
        asl_set(m, kPMASLDomainKey, kPMASLDomainPMDarkWake);
        snprintf(numbuf, sizeof(numbuf), "%d", darkWakeCnt);
        asl_set(m, kPMASLValueKey, numbuf);
    }
    else if (dark_wake == kIsDarkToFullWake)
    {
        wakeType = _copyRootDomainProperty(CFSTR(kIOPMRootDomainWakeTypeKey));
        if (isA_CFString(wakeType)) {
            CFStringGetCString(wakeType, wakeReasonBuf, sizeof(wakeReasonBuf), kCFStringEncodingUTF8);
        }
        if (wakeType) {
            CFRelease(wakeType);
        }
        asl_set(m, kPMASLDomainKey, kPMASLDomainPMWake);
        snprintf(buf, sizeof(buf), "%s", "DarkWake to FullWake");
    }
    else
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainPMWake);
        snprintf(buf, sizeof(buf), "%s", "Wake");
    }

    if (success)
    {
        if ((sleepTypeString = getSleepTypeString()))
        {
            snprintf(buf, sizeof(buf), "%s from %s", buf, sleepTypeString);
        }

        printCapabilitiesToBuf(cBuf, sizeof(cBuf), in_capabilities);
        strncat(buf, cBuf, sizeof(buf)-strlen(buf)-1);
    }

    snprintf(buf, sizeof(buf), "%s %s %s\n", buf,
          detailString ? ": due to" : "",
          detailString ? detailString : "");

    INFO_LOG("%{public}s\n", buf);
    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    if (success) {
        logASLMessageHibernateStatistics( );
    }
    asl_release(m);

    if (isA_installEnvironment()) {
        syslog(LOG_INFO | LOG_INSTALL, "%s battCap:%s pwrSrc: %s\n",
                buf, battCap, (pwrSrc == kACPowered) ? "AC" : "Batt");
    }


}

__private_extern__ void logASLMessageWakeTime(uint64_t waketime, WakeTypeEnum waketype)
{
    char buf[128];
    double wakeTime = (double)(waketime)/1000000.0;
    snprintf(buf, sizeof(buf), "WakeTime: %2.3lf sec", wakeTime/1000.0);
    INFO_LOG("%s\n", buf);

    aslmsg m = NULL;
    m = new_msg_pmset_log();

    asl_set(m, kPMASLDomainKey, kPMASLDomainWakeTime);
    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    mt2PublishWakeTime(wakeTime, waketype);
    asl_release(m);
}
/*****************************************************************************/

__private_extern__ void logASLAppWakeReason(
                                            const char * ident,
                                            const char * reason)
{
#define kPMASLDomainAppWakeReason   "AppWakeReason"

    char msg[255];
    snprintf(msg, sizeof(msg), "AppWoke:%s Reason:%s", ident?ident:"--none--", reason?reason:"--none--");
    INFO_LOG("%{public}s\n", msg);

    aslmsg m = new_msg_pmset_log();

    asl_set(m, kPMASLDomainKey, kPMASLDomainAppWakeReason);
    if (ident) {
        asl_set(m, kPMASLSignatureKey, ident);
    }

    asl_set(m, ASL_KEY_MSG, msg);
    asl_send(NULL, m);
    asl_release(m);
}


/*****************************************************************************/

__private_extern__ void logASLPMConnectionNotify(
    CFStringRef     appNameString,
    int             notificationBits
    )
{
    aslmsg m;
    char buf[128];
    char appName[100];


    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainAppNotify);


    if (!CFStringGetCString(appNameString, appName, sizeof(appName), kCFStringEncodingUTF8))
       snprintf(appName, sizeof(appName), "Unknown app");


    asl_set(m, kPMASLSignatureKey, appName);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

   snprintf(buf, sizeof(buf), "Notification sent to %s (powercaps:0x%x)",
         appName,notificationBits );

    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_release(m);
}

__private_extern__ void logASLDisplayStateChange()
{
    bool displayOff = isDisplayAsleep();

    aslmsg m;
    char buf[128];
    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainAppNotify);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

   snprintf(buf, sizeof(buf), "Display is turned %s",
        displayOff ? "off" : "on");

    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_release(m);

    if (isA_installEnvironment()) {
        syslog(LOG_INFO | LOG_INSTALL, "%s\n", buf);
    }

    if (displayOff) {
        /* Log all assertions when display goes off */
        dispatch_async(_getPMMainQueue(), ^{
            logASLAllAssertions();
        });
    }
}


__private_extern__ void logASLInactivityWindow(inactivityWindowType type, CFDateRef start, CFDateRef end)
{

    aslmsg m;
    char buf[128];
    char startCStr[32], endCStr[32];
    CFStringRef startStr, endStr;

    startCStr[0] = endCStr[0] = 0;
    startStr = endStr = NULL;
    CFDateFormatterRef date_format = CFDateFormatterCreate (NULL, NULL, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
    if (date_format) {
        CFDateFormatterSetFormat(date_format, CFSTR("yyyy-MM-dd HH:mm:ss ZZZ"));
        startStr = CFDateFormatterCreateStringWithDate(kCFAllocatorDefault, date_format, start);
        if (startStr) {
            CFStringGetCString(startStr, startCStr, sizeof(startCStr), kCFStringEncodingMacRoman);
        }
        endStr = CFDateFormatterCreateStringWithDate(kCFAllocatorDefault, date_format, end);
        if (endStr) {
            CFStringGetCString(endStr, endCStr, sizeof(endCStr), kCFStringEncodingMacRoman);
        }
        CFRelease(date_format);
    }

    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainAppNotify);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }


    snprintf(buf, sizeof(buf), "Next %s inactivity window start:\'%s\' end:\'%s\'\n",
             (type == kImmediateInactivityWindow) ? "immediate" : "largest", startCStr, endCStr);

    INFO_LOG("%{public}s\n", buf);
    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_release(m);

    if (startStr) {
        CFRelease(startStr);
    }
    if (endStr) {
        CFRelease(endStr);
    }
}


__private_extern__ void logASLPerforamceState(int perfState)
{
    aslmsg m;
    char buf[128];


    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainPerformanceEvent);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

    snprintf(buf, sizeof(buf), "Performance State is %d", perfState);

    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_release(m);
}

__private_extern__ void logASLThermalState(int thermalState)
{
    aslmsg m;
    char buf[128];

    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainThermalEvent);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

   snprintf(buf, sizeof(buf), "Thermal State is %d", thermalState);

    asl_set(m, ASL_KEY_MSG, buf);
    asl_send(NULL, m);
    asl_release(m);
}

__private_extern__ void logASLMessagePMConnectionResponse(
    CFStringRef     logSourceString,
    CFStringRef     appNameString,
    CFStringRef     responseTypeString,
    CFNumberRef     responseTime,
    int             notificationBits
)
{
    aslmsg                  m;
    char                    appName[128];
    char                    *appNamePtr = NULL;
    int                     time = 0;
    char                    buf[128];
    char                    qualifier[30];
    bool                    timeout = false;

    // String identifying the source of the log is required.
    if (!logSourceString)
        return;
    
    m = new_msg_pmset_log();

    if (responseTypeString && CFEqual(responseTypeString, CFSTR(kIOPMStatsResponseTimedOut)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseTimedOut);
        snprintf(qualifier, sizeof(qualifier), "timed out");
        timeout = true;
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kIOPMStatsResponseCancel)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseCancel);
        snprintf(qualifier, sizeof(qualifier), "is to cancel state change");
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kIOPMStatsResponseSlow)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseSlow);
        snprintf(qualifier, sizeof(qualifier), "is slow");
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kPMASLDomainSleepServiceCapApp)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainSleepServiceCapApp);
        snprintf(qualifier, sizeof(qualifier), "exceeded SleepService cap");
    } else
        if (responseTypeString && CFEqual(responseTypeString, CFSTR(kPMASLDomainAppResponse)))
    {
        asl_set(m, kPMASLDomainKey, kPMASLDomainAppResponseReceived);
        snprintf(qualifier, sizeof(qualifier), "received");
    } else {
        asl_release(m);
        return;
    }

    // Message = Failing process name
    if (appNameString)
    {
        if (CFStringGetCString(appNameString, appName, sizeof(appName), kCFStringEncodingUTF8))
        {
                appNamePtr = &appName[0];
        }
    }
    if (!appNamePtr) {
        appNamePtr = "AppNameUnknown";
    }

    asl_set(m, kPMASLSignatureKey, appNamePtr);

    // UUID
    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

    // Value == Time
    if (responseTime) {
        if (CFNumberGetValue(responseTime, kCFNumberIntType, &time)) {
            snprintf(buf, sizeof(buf), "%d", time);
            asl_set(m, kPMASLValueKey, buf);
        }
    }

    if (CFStringGetCString(logSourceString, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        snprintf(buf, sizeof(buf), "%s: Response from %s %s",
              buf, appNamePtr, qualifier);
    } else {
        snprintf(buf, sizeof(buf), "Response from %s %s",
              appNamePtr, qualifier);
    }

    if (notificationBits != -1)
       snprintf(buf, sizeof(buf), "%s (powercaps:0x%x)", buf, notificationBits);

    asl_set(m, ASL_KEY_MSG, buf);

    if (time != 0) {
       snprintf(buf, sizeof(buf), "%d ms", time);
       asl_set(m, kPMASLDelayKey, buf);
    }

    asl_send(NULL, m);
    asl_release(m);

    if (timeout) {
        mt2RecordAppTimeouts(reasons.sleepReason, appNameString);
    }
}

/*****************************************************************************/

/* logASLMessageAppStats
 *
 * Logs ASL message for delays and timeouts in acknowledging power notifications
 *
 */
__private_extern__ void  logASLMessageAppStats(CFArrayRef appFailuresArray, char *domain)
{
    CFDictionaryRef         appFailures = NULL;
    CFStringRef             appNameString = NULL;
    CFStringRef             transString = NULL;
    CFNumberRef             numRef = NULL;
    CFStringRef             responseTypeString = NULL;
    long                    numElems = 0;
    int                     appCnt = 0;
    int                     i = 0;
    aslmsg                  m;
    char                    appName[128];
    char                    responseType[32];
    int                     num = 0;
    char                    key[128];
    char                    numStr[10];


    if (!isA_CFArray(appFailuresArray))
        return;

    numElems = CFArrayGetCount(appFailuresArray);
    if (numElems == 0)
        return;

    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, domain);

    for (i = 0; i < numElems; i++)
    {
        appFailures = CFArrayGetValueAtIndex(appFailuresArray, i);
        if ( !isA_CFDictionary(appFailures)) {
            break;
        }


        appNameString = CFDictionaryGetValue(appFailures, CFSTR(kIOPMStatsNameKey));
        if (!isA_CFString(appNameString) || 
            (!CFStringGetCString(appNameString, appName, sizeof(appName), kCFStringEncodingUTF8))) {
            continue;
        }

        numRef = CFDictionaryGetValue(appFailures, CFSTR(kIOPMStatsTimeMSKey));
        if (!isA_CFNumber(numRef) || (!CFNumberGetValue(numRef, kCFNumberIntType, &num))) {
                continue;
        }
        numStr[0] = 0;
        snprintf(numStr, sizeof(numStr), "%d", num);

        responseTypeString  = CFDictionaryGetValue(appFailures, CFSTR(kIOPMStatsApplicationResponseTypeKey));
        if (!isA_CFString(responseTypeString) || 
            (!CFStringGetCString(responseTypeString, responseType, sizeof(responseType), kCFStringEncodingUTF8))) {
            continue;
        }

        if (!_getUUIDString(key, sizeof(key)))
            continue;
        asl_set(m, kPMASLUUIDKey, key);

        if (transString == NULL) {
            // Transition should be same for all apps listed in appFailuresArray.
            // So, set kPMASLResponseSystemTransition only once
            transString = CFDictionaryGetValue(appFailures, CFSTR(kIOPMStatsSystemTransitionKey));
            if (isA_CFString(transString)  && 
                (CFStringGetCString(transString, key, sizeof(key), kCFStringEncodingUTF8))) {
                    asl_set(m, kPMASLResponseSystemTransition, key);
            }
        }

        snprintf(key, sizeof(key), "%s%d",kPMASLResponseAppNamePrefix, appCnt);
        asl_set(m, key, appName);

        snprintf(key, sizeof(key), "%s%d", kPMASLResponseRespTypePrefix, appCnt);
        asl_set(m, key, responseType);

        snprintf(key, sizeof(key), "%s%d", kPMASLResponseDelayPrefix, appCnt);
        asl_set(m, key, numStr);

        numRef = CFDictionaryGetValue(appFailures, CFSTR(kIOPMStatsMessageTypeKey));
        if (isA_CFNumber(numRef) && (CFNumberGetValue(numRef, kCFNumberIntType, &num))) {

            snprintf(key, sizeof(key), "%s%d", kPMASLResponseMessagePrefix, appCnt);
            if (num == kDriverCallSetPowerState)
                asl_set(m, key, "SetState");
            else if (num == kDriverCallInformPreChange)
                asl_set(m, key, "WillChangeState");
            else 
                asl_set(m, key, "DidChangeState");
        }

        numRef = CFDictionaryGetValue(appFailures, CFSTR(kIOPMStatsPowerCapabilityKey));
        if (isA_CFNumber(numRef) && (CFNumberGetValue(numRef, kCFNumberIntType, &num))) {
            numStr[0] = 0;
            snprintf(numStr, sizeof(numStr), "%d", num);

            snprintf(key, sizeof(key), "%s%d", kPMASLResponsePSCapsPrefix, appCnt);
            asl_set(m, key, numStr);
        }

        appCnt++;

        if (CFEqual(responseTypeString, CFSTR(kIOPMStatsResponseTimedOut))) {
            mt2RecordAppTimeouts(reasons.sleepReason, appNameString);
        }
    }
    asl_send(NULL, m);
    asl_release(m);
}


__private_extern__ void logASLMessagePMConnectionScheduledWakeEvents(CFStringRef requestedMaintenancesString)
{
    aslmsg                  m;
    char                    buf[100];
    char                    requestors[500];
    CFMutableStringRef      messageString = NULL;

    messageString = CFStringCreateMutable(0, 0);
    if (!messageString)
        return;

    m = new_msg_pmset_log();

    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

    CFStringAppendCString(messageString, "Clients requested wake events: ", kCFStringEncodingUTF8);
    if (requestedMaintenancesString && (0 < CFStringGetLength(requestedMaintenancesString))) {
        CFStringAppend(messageString, requestedMaintenancesString);
    } else {
        CFStringAppend(messageString, CFSTR("None"));
    }

    CFStringGetCString(messageString, requestors, sizeof(requestors), kCFStringEncodingUTF8);

    asl_set(m, kPMASLDomainKey, kPMASLDomainPMWakeRequests);
    asl_set(m, ASL_KEY_MSG, requestors);
    asl_send(NULL, m);
    asl_release(m);
    CFRelease(messageString);
}

__private_extern__ void logASLMessageExecutedWakeupEvent(CFStringRef requestedMaintenancesString)
{
    aslmsg                  m;
    char                    buf[100];
    char                    requestors[500];
    CFMutableStringRef      messageString = CFStringCreateMutable(0, 0);

    if (!messageString)
        return;

    m = new_msg_pmset_log();

    if (_getUUIDString(buf, sizeof(buf))) {
        asl_set(m, kPMASLUUIDKey, buf);
    }

    CFStringAppendCString(messageString, "PM scheduled RTC wake event: ", kCFStringEncodingUTF8);
    CFStringAppend(messageString, requestedMaintenancesString);

    CFStringGetCString(messageString, requestors, sizeof(requestors), kCFStringEncodingUTF8);

    asl_set(m, kPMASLDomainKey, kPMASLDomainPMWakeRequests);
    asl_set(m, ASL_KEY_MSG, requestors);
    asl_send(NULL, m);
    asl_release(m);
    CFRelease(messageString);
}

__private_extern__ void logASLMessageIgnoredDWTEmergency(void)
{
    aslmsg      m;
    char        strbuf[125];
    char        tcpKeepAliveString[50];

    bzero(strbuf, sizeof(strbuf));
    bzero(tcpKeepAliveString, sizeof(tcpKeepAliveString));

    m = new_msg_pmset_log();
    attachTCPKeepAliveKeys(m, tcpKeepAliveString, sizeof(tcpKeepAliveString));
    
    asl_set(m, kPMASLDomainKey, kPMASLDomainThermalEvent);

    snprintf(
        strbuf,
        sizeof(strbuf),
        "Ignored DarkWake thermal emergency signal %s", tcpKeepAliveString);
    asl_set(m, ASL_KEY_MSG, strbuf);

    asl_send(NULL, m);
    asl_release(m);
}

__private_extern__ void logASLMessageSleepCanceledAtLastCall(
                               bool tcpka_active,
                               bool sys_active,
                               bool pending_wakes)
{
    aslmsg      m;
    char        strbuf[250];
    char        tcpKeepAliveString[50];

    bzero(strbuf, sizeof(strbuf));
    bzero(tcpKeepAliveString, sizeof(tcpKeepAliveString));

    m = new_msg_pmset_log();

    if (tcpka_active)
        attachTCPKeepAliveKeys(m, tcpKeepAliveString, sizeof(tcpKeepAliveString));

    asl_set(m, kPMASLDomainKey, kPMASLDomainSleepRevert);

    snprintf( strbuf, sizeof(strbuf),
        "Sleep in process aborted due to ");
    if (tcpka_active)
        snprintf(strbuf, sizeof(strbuf), "%s (TCP KeepAlive activity -- %s) ", strbuf, tcpKeepAliveString);
    if (sys_active)
        snprintf(strbuf, sizeof(strbuf), "%s (SystemIsActive assertions)", strbuf);
    if (pending_wakes)
        snprintf(strbuf, sizeof(strbuf), "%s (Pending system wake request)", strbuf);

    asl_set(m, ASL_KEY_MSG, strbuf);

    asl_send(NULL, m);
    asl_release(m);
}

__private_extern__ void logASLBatteryHealthChanged(const char *health,
                                                   const char *oldhealth,
                                                   const char *reason)
{
    aslmsg      m;
    char        strbuf[125];
    
    bzero(strbuf, sizeof(strbuf));
    
    m = new_msg_pmset_log();
    
    asl_set(m, kPMASLDomainKey, kPMASLDomainBattery);
    
    if (!strncmp(oldhealth, "", 5)) {
        snprintf(
                 strbuf,
                 sizeof(strbuf),
                 "Battery health: %s", health);
    } else if (!strncmp(reason, "", 5)){
        snprintf(
                 strbuf,
                 sizeof(strbuf),
                 "Battery health: %s; was: %s", health, oldhealth);
    } else {
        snprintf(
                 strbuf,
                 sizeof(strbuf),
                 "Battery health: %s; was: %s; reason %s", health, oldhealth, reason);
    }
    asl_set(m, ASL_KEY_MSG, strbuf);
    
    asl_send(NULL, m);
    asl_release(m);
}

__private_extern__ void logASLLowBatteryWarning(IOPSLowBatteryWarningLevel level,
                                                   int time, int ccap)
{
    aslmsg      m;
    char        strbuf[125];
    
    bzero(strbuf, sizeof(strbuf));
    
    m = new_msg_pmset_log();
    
    asl_set(m, kPMASLDomainKey, kPMASLDomainBattery);
    
    snprintf(strbuf, sizeof(strbuf), "Warning level: %d time: %d cap: %d\n",
             level, time, ccap);
    asl_set(m, ASL_KEY_MSG, strbuf);
    
    asl_send(NULL, m);
    asl_release(m);
}

__private_extern__ void logASLSleepPreventers(int preventerType)
{
    aslmsg      m;
    char        strbuf[125];
    CFArrayRef  preventers;
    IOReturn    ret;
    char        name[32];
    long        count = 0;
    size_t      len = 0;


    strbuf[0] = 0;

    ret = IOPMCopySleepPreventersList(preventerType, &preventers);
    if (ret != kIOReturnSuccess)
    {
        return;
    }
    if (isA_CFArray(preventers))
    {
        count = CFArrayGetCount(preventers);
    }

    m = new_msg_pmset_log();
    asl_set(m, kPMASLDomainKey, kPMASLDomainPMAssertions);

    snprintf(strbuf, sizeof(strbuf), "Kernel %s sleep preventers: ",
             (preventerType == kIOPMIdleSleepPreventers) ? "Idle" : "System");
    if (count == 0)
    {
        strlcat(strbuf, "-None-", sizeof(strbuf));
    }

    for (int i = 0; i < count; i++)
    {
        CFStringRef cfstr = CFArrayGetValueAtIndex(preventers, i);
        CFStringGetCString(cfstr, name, sizeof(name), kCFStringEncodingUTF8);

        if (i != 0) {
            strlcat(strbuf, ", ", sizeof(strbuf));
        }
        len = strlcat(strbuf, name, sizeof(strbuf));
        if (len >= sizeof(strbuf)) break;
    }
    if (len >= sizeof(strbuf)) {
        // Put '...' to indicate incomplete string
        strbuf[sizeof(strbuf)-4] = strbuf[sizeof(strbuf)-3] = strbuf[sizeof(strbuf)-2] = '.';
        strbuf[sizeof(strbuf)-1] = '\0';
    }
    asl_set(m, ASL_KEY_MSG, strbuf);

    asl_send(NULL, m);
    asl_release(m);

    if (preventers)
    {
        CFRelease(preventers);
    }
}
/*****************************************************************************/
/*****************************************************************************/


/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
#pragma mark MT2 DarkWake


/*
 * MessageTracer2 DarkWake Keys
 */

typedef struct {
    CFAbsoluteTime              startedPeriod;
    dispatch_source_t           nextFireSource;

    /* for domain com.apple.darkwake.capable */
    int                         SMCSupport:1;
    int                         PlatformSupport:1;
    int                         checkedforAC:1;
    int                         checkedforBatt:1;
    /* for domain com.apple.darkwake.wakes */
    uint16_t                    wakeEvents[kWakeStateCount];
    /* for domain com.apple.darkwake.thermal */
    uint16_t                    thermalEvents[kThermalStateCount];
    /* for domain com.apple.darkwake.backgroundtasks */
    CFMutableSetRef             alreadyRecordedBackground;
    CFMutableDictionaryRef      tookBackground;
    /* for domain com.apple.darkwake.pushservicetasks */
    CFMutableSetRef             alreadyRecordedPush;
    CFMutableDictionaryRef      tookPush;
    /* for domain com.apple.darkwake.pushservicetasktimeout */
    CFMutableSetRef             alreadyRecordedPushTimeouts;
    CFMutableDictionaryRef      timeoutPush;
    CFMutableDictionaryRef      idleSleepAppTimeouts;
    CFMutableDictionaryRef      demandSleepAppTimeouts;
    CFMutableDictionaryRef      darkwakeSleepAppTimeouts;
} MT2Aggregator;

static const uint64_t   kMT2CheckIntervalTimer = 4ULL*60ULL*60ULL*NSEC_PER_SEC;     /* Check every 4 hours */
static CFAbsoluteTime   kMT2SendReportsAtInterval = 7.0*24.0*60.0*60.0;             /* Publish reports every 7 days */
static const uint64_t   kBigLeeway = (30Ull * NSEC_PER_SEC);

static MT2Aggregator    *mt2 = NULL;

void initializeMT2Aggregator(void)
{
    if (mt2)
    {
        /* Zero out & recycle MT2Aggregator structure */
        if (mt2->nextFireSource) {
            dispatch_release(mt2->nextFireSource);
        }
        CFRelease(mt2->alreadyRecordedBackground);
        CFRelease(mt2->tookBackground);
        CFRelease(mt2->alreadyRecordedPush);
        CFRelease(mt2->tookPush);
        CFRelease(mt2->alreadyRecordedPushTimeouts);
        CFRelease(mt2->timeoutPush);
        CFRelease(mt2->idleSleepAppTimeouts);
        CFRelease(mt2->demandSleepAppTimeouts);
        CFRelease(mt2->darkwakeSleepAppTimeouts);

        bzero(mt2, sizeof(MT2Aggregator));
    } else {
        /* New datastructure */
        mt2 = calloc(1, sizeof(MT2Aggregator));
    }
    mt2->startedPeriod                      = CFAbsoluteTimeGetCurrent();
    mt2->alreadyRecordedBackground          = CFSetCreateMutable(0, 0, &kCFTypeSetCallBacks);
    mt2->alreadyRecordedPush                = CFSetCreateMutable(0, 0, &kCFTypeSetCallBacks);
    mt2->alreadyRecordedPushTimeouts        = CFSetCreateMutable(0, 0, &kCFTypeSetCallBacks);
    mt2->tookBackground                     = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->tookPush                           = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->timeoutPush                        = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->idleSleepAppTimeouts               = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->demandSleepAppTimeouts             = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    mt2->darkwakeSleepAppTimeouts           = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, NULL);

    mt2->nextFireSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _getPMMainQueue());
    if (mt2->nextFireSource) {
        dispatch_source_set_event_handler(mt2->nextFireSource, ^(){ mt2PublishReports(); });
        dispatch_source_set_timer(mt2->nextFireSource, dispatch_time(DISPATCH_TIME_NOW, kMT2CheckIntervalTimer),
                                  kMT2CheckIntervalTimer, kBigLeeway);
        dispatch_resume(mt2->nextFireSource);
    }
    mt2EvaluateSystemSupport();
    return;
}



static int mt2PublishDomainCapable(void)
{
#define kMT2DomainDarkWakeCapable       "com.apple.darkwake.capable"
#define kMT2KeySupport                  "com.apple.message.hardware_support"
#define kMT2ValNoSupport                "none"
#define kMT2ValPlatformSupport          "platform_without_smc"
#define kMT2ValSMCSupport               "smc_without_platform"
#define kMT2ValFullDWSupport            "dark_wake_supported"
#define kMT2KeySettings                 "com.apple.message.settings"
#define kMT2ValSettingsNone             "none"
#define kMT2ValSettingsAC               "ac"
#define kMT2ValSettingsBatt             "battery"
#define kMT2ValSettingsACPlusBatt       "ac_and_battery"
    if (!mt2) {
        return 0;
    }

    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainDarkWakeCapable );
    if (mt2->SMCSupport && mt2->PlatformSupport) {
        asl_set(m, kMT2KeySupport, kMT2ValFullDWSupport);
    } else if (mt2->PlatformSupport) {
        asl_set(m, kMT2KeySupport, kMT2ValPlatformSupport);
    } else if (mt2->SMCSupport) {
        asl_set(m, kMT2KeySupport, kMT2ValSMCSupport);
    } else {
        asl_set(m, kMT2KeySupport, kMT2ValNoSupport);
    }

    if (mt2->checkedforAC && mt2->checkedforBatt) {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsACPlusBatt);
    } else if (mt2->checkedforAC) {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsAC);
    } else if (mt2->checkedforBatt) {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsBatt);
    } else {
        asl_set(m, kMT2KeySettings, kMT2ValSettingsNone);
    }

    asl_log(NULL, m, ASL_LEVEL_ERR, "");
    asl_release(m);

    return 1;
}


static int mt2PublishDomainWakes(void)
{
#define kMT2DomainWakes                 "com.apple.darkwake.wakes"
#define kMT2KeyWakeType                 "com.apple.message.waketype"
#define kMT2ValWakeDark                 "dark"
#define kMT2ValWakeFull                 "full"
#define kMT2KeyPowerSource              "com.apple.message.powersource"
#define kMT2ValPowerAC                  "ac"
#define kMT2ValPowerBatt                "battery"
#define kMT2KeyLid                      "com.apple.message.lid"
#define kMT2ValLidOpen                  "open"
#define kMT2ValLidClosed                "closed"

    int     sentCount = 0;
    int     i = 0;
    char    buf[kIntegerStringLen];

    if (!mt2) {
        return 0;
    }
    for (i=0; i<kWakeStateCount; i++)
    {
        if (0 == mt2->wakeEvents[i]) {
            continue;
        }
        aslmsg m = asl_new(ASL_TYPE_MSG);
        asl_set(m, "com.apple.message.domain", kMT2DomainWakes);
        if (i & kWakeStateDark) {
            asl_set(m, kMT2KeyWakeType, kMT2ValWakeDark);
        } else {
            asl_set(m, kMT2KeyWakeType, kMT2ValWakeFull);
        }
        if (i & kWakeStateBattery) {
            asl_set(m, kMT2KeyPowerSource, kMT2ValPowerBatt);
        } else {
            asl_set(m, kMT2KeyPowerSource, kMT2ValPowerAC);
        }
        if (i & kWakeStateLidClosed) {
            asl_set(m, kMT2KeyLid, kMT2ValLidClosed);
        } else {
            asl_set(m, kMT2KeyLid, kMT2ValLidOpen);
        }

        snprintf(buf, sizeof(buf), "%d", mt2->wakeEvents[i]);
        asl_set(m, "com.apple.message.count", buf);
        asl_log(NULL, m, ASL_LEVEL_ERR, "");
        asl_release(m);
        sentCount++;
    }
    return sentCount;
}

static int mt2PublishDomainThermals(void)
{
#define kMT2DomainThermal               "com.apple.darkwake.thermalevent"
#define kMT2KeySleepRequest             "com.apple.message.sleeprequest"
#define kMT2KeyFansSpin                 "com.apple.message.fansspin"
#define kMT2ValTrue                     "true"
#define kMT2ValFalse                    "false"

    int     sentCount = 0;
    int     i = 0;
    char    buf[kIntegerStringLen];

    if (!mt2) {
        return 0;
    }

    for (i=0; i<kThermalStateCount; i++)
    {
        if (0 == mt2->thermalEvents[i]) {
            continue;
        }
        aslmsg m = asl_new(ASL_TYPE_MSG);
        asl_set(m, "com.apple.message.domain", kMT2DomainThermal );
        if (i & kThermalStateSleepRequest) {
            asl_set(m, kMT2KeySleepRequest, kMT2ValTrue);
        } else {
            asl_set(m, kMT2KeySleepRequest, kMT2ValFalse);
        }
        if (i & kThermalStateFansOn) {
            asl_set(m, kMT2KeyFansSpin, kMT2ValTrue);
        } else {
            asl_set(m, kMT2KeyFansSpin, kMT2ValFalse);
        }

        snprintf(buf, sizeof(buf), "%d", mt2->thermalEvents[i]);
        asl_set(m, "com.apple.message.count", buf);
        asl_log(NULL, m, ASL_LEVEL_ERR, "");
        asl_release(m);
        sentCount++;
    }
    return sentCount;
}

static int mt2PublishDomainProcess(const char *appdomain, CFDictionaryRef apps)
{
#define kMT2KeyApp                      "com.apple.message.process"

    CFStringRef         *keys;
    uintptr_t           *counts;
    char                buf[2*kProcNameBufLen];
    int                 sendCount = 0;
    long                appcount = 0;
    int                 i = 0;

    if (!mt2 || !apps || (0 == (appcount = CFDictionaryGetCount(apps))))
    {
        return 0;
    }

    keys = (CFStringRef *)calloc(sizeof(CFStringRef), appcount);
    counts = (uintptr_t *)calloc(sizeof(uintptr_t), appcount);

    CFDictionaryGetKeysAndValues(apps, (const void **)keys, (const void **)counts);

    for (i=0; i<appcount; i++)
    {
        if (0 == counts[i]) {
            continue;
        }
        aslmsg m = asl_new(ASL_TYPE_MSG);
        asl_set(m, "com.apple.message.domain", appdomain);

        if (!CFStringGetCString(keys[i], buf, sizeof(buf), kCFStringEncodingUTF8)) {
            snprintf(buf, sizeof(buf), "com.apple.message.%s", "Unknown");
        }
        asl_set(m, kMT2KeyApp, buf);

        snprintf(buf, sizeof(buf), "%d", (int)counts[i]);
        asl_set(m, "com.apple.message.count", buf);

        asl_log(NULL, m, ASL_LEVEL_ERR,"");
        asl_release(m);
        sendCount++;

    }

    free(keys);
    free(counts);

    return sendCount;
}

void mt2PublishReports(void)
{
#define kMT2DomainPushTasks         "com.apple.darkwake.pushservicetasks"
#define kMT2DomainPushTimeouts      "com.apple.darkwake.pushservicetimeouts"
#define kMT2DomainBackgroundTasks   "com.apple.darkwake.backgroundtasks"
#define kMT2DomainIdleSlpAckTo      "com.apple.ackto.idlesleep"    /* Idle sleep ack timeouts */
#define kMT2DomainDemandSlpAckTo    "com.apple.ackto.demandsleep"  /* Demand sleep ack timeouts */
#define kMT2DomainDarkWkSlpAckTo    "com.apple.ackto.demandsleep"  /* Dark wake sleep ack timeouts */
#define kMT2DomainclaimedWakeEvents "com.apple.wake.claimedevents"

    if (!mt2) {
        return;
    }

    if ((mt2->startedPeriod + kMT2SendReportsAtInterval) < CFAbsoluteTimeGetCurrent())
    {
        /* mt2PublishReports should only publish a new batch of ASL no more
         * frequently than once every kMT2SendReportsAtInterval seconds.
         * If it's too soon to publish ASL keys, just return.
         */
        return;
    }

    mt2PublishDomainCapable();

    if (mt2->PlatformSupport && mt2->SMCSupport)
    {
        mt2PublishDomainWakes();
        mt2PublishDomainThermals();
        mt2PublishDomainProcess(kMT2DomainPushTasks, mt2->tookPush);
        mt2PublishDomainProcess(kMT2DomainPushTimeouts, mt2->timeoutPush);
        mt2PublishDomainProcess(kMT2DomainBackgroundTasks, mt2->tookBackground);
        mt2PublishDomainProcess(kMT2DomainIdleSlpAckTo, mt2->idleSleepAppTimeouts);
        mt2PublishDomainProcess(kMT2DomainDemandSlpAckTo, mt2->demandSleepAppTimeouts);
        mt2PublishDomainProcess(kMT2DomainDarkWkSlpAckTo, mt2->darkwakeSleepAppTimeouts);

        // Recyle the data structure for the next reporting.
        initializeMT2Aggregator();
    }

    // If the system lacks (PlatformSupport && SMC Support), this is where we stop scheduling MT2 reports.
    // If the system has PowerNap support, then we'll set a periodic timer in initializeMT2Aggregator() and
    // we'll keep publish messages on a schedule.

    return;
}

void mt2DarkWakeEnded(void)
{
    if (!mt2) {
        return;
    }
    CFSetRemoveAllValues(mt2->alreadyRecordedBackground);
    CFSetRemoveAllValues(mt2->alreadyRecordedPush);
    CFSetRemoveAllValues(mt2->alreadyRecordedPushTimeouts);
}

void mt2EvaluateSystemSupport(void)
{
    CFDictionaryRef     energySettings = NULL;
    CFDictionaryRef     per = NULL;
    CFNumberRef         num = NULL;
    int                 value = 0;

    if (!mt2) {
        return;
    }

    mt2->SMCSupport = smcSilentRunningSupport() ? 1:0;
    mt2->PlatformSupport = (_platformBackgroundTaskSupport || _platformSleepServiceSupport) ? 1:0;

    mt2->checkedforAC = 0;
    mt2->checkedforBatt = 0;
    if ((energySettings = IOPMCopyActivePMPreferences())) {
        per = CFDictionaryGetValue(energySettings, CFSTR(kIOPMACPowerKey));
        if (per) {
            num = CFDictionaryGetValue(per, CFSTR(kIOPMDarkWakeBackgroundTaskKey));
            if (num) {
                CFNumberGetValue(num, kCFNumberIntType, &value);
                mt2->checkedforAC = value ? 1:0;
            }
        }
        per = CFDictionaryGetValue(energySettings, CFSTR(kIOPMBatteryPowerKey));
        if (per) {
            num = CFDictionaryGetValue(per, CFSTR(kIOPMDarkWakeBackgroundTaskKey));
            if (num) {
                CFNumberGetValue(num, kCFNumberIntType, &value);
                mt2->checkedforBatt = value ? 1:0;
            }
        }

        CFRelease(energySettings);
    }
    return;
}

void mt2RecordWakeEvent(uint32_t description)
{
    CFStringRef     lidString = NULL; 
    CFBooleanRef    lidIsClosed = NULL;

    if (!mt2) {
        return;
    }

    if ((kWakeStateDark & description) == 0) {
        /* The system just woke into FullWake.
         * To make sure that we publish mt2 reports in a timely manner,
         * we'll try now. It'll only actually happen if the PublishInterval has
         * elapsed since the last time we published.
         */
        mt2PublishReports();
    }

    lidString = CFStringCreateWithCString(0, kAppleClamshellStateKey, kCFStringEncodingUTF8);
    if (lidString) {
        lidIsClosed = _copyRootDomainProperty(lidString);
        CFRelease(lidString);
    }

    description |= ((_getPowerSource() == kBatteryPowered) ? kWakeStateBattery : kWakeStateAC)
                 | ((kCFBooleanTrue == lidIsClosed) ? kWakeStateLidClosed : kWakeStateLidOpen);

    if (lidIsClosed) {
        CFRelease(lidIsClosed);
    }

    mt2->wakeEvents[description]++;
    return;
}

void mt2RecordThermalEvent(uint32_t description)
{
    if (!mt2) {
        return;
    }
    description &= (kThermalStateFansOn | kThermalStateSleepRequest);
    mt2->thermalEvents[description]++;
    return;
}

/* PMConnection.c */
bool isA_DarkWakeState(void);

void mt2RecordAssertionEvent(assertionOps action, assertion_t *theAssertion)
{
    CFStringRef         processName;
    CFStringRef         assertionType;

    if (!mt2) {
        return;
    }

    if (!theAssertion || !theAssertion->props || !isA_DarkWakeState()) {
        return;
    }

    if (!(processName = processInfoGetName(theAssertion->pinfo->pid))) {
        processName = CFSTR("Unknown");
    }

    if (!(assertionType = CFDictionaryGetValue(theAssertion->props, kIOPMAssertionTypeKey))
        || (!CFEqual(assertionType, kIOPMAssertionTypeBackgroundTask)
         && !CFEqual(assertionType, kIOPMAssertionTypeApplePushServiceTask)))
    {
        return;
    }

    if (CFEqual(assertionType, kIOPMAssertionTypeBackgroundTask))
    {
        if (kAssertionOpRaise == action) {
            if (!CFSetContainsValue(mt2->alreadyRecordedBackground, processName)) {
                int x = (int)CFDictionaryGetValue(mt2->tookBackground, processName);
                x++;
                CFDictionarySetValue(mt2->tookBackground, processName, (void *)(uintptr_t)x);
                CFSetAddValue(mt2->alreadyRecordedBackground, processName);
            }
        }
    }
    else if (CFEqual(assertionType, kIOPMAssertionTypeApplePushServiceTask))
    {
        if (kAssertionOpRaise == action) {
            if (!CFSetContainsValue(mt2->alreadyRecordedPush, processName)) {
                int x = (int)CFDictionaryGetValue(mt2->tookPush, processName);
                x++;
                CFDictionarySetValue(mt2->tookPush, processName, (void *)(uintptr_t)x);
                CFSetAddValue(mt2->alreadyRecordedPush, processName);
            }
        }
        else if (kAssertionOpGlobalTimeout == action) {
            if (!CFSetContainsValue(mt2->alreadyRecordedPushTimeouts, processName)) {
                int x = (int)CFDictionaryGetValue(mt2->timeoutPush, processName);
                x++;
                CFDictionarySetValue(mt2->timeoutPush, (const void *)processName, (void *)(uintptr_t)x);
                CFSetAddValue(mt2->alreadyRecordedPushTimeouts, processName);
            }
        }
    }

    return;
}

void mt2RecordAppTimeouts(CFStringRef sleepReason, CFStringRef procName)
{
    CFMutableDictionaryRef dict;

    if ( !mt2 || !isA_CFString(procName)) return;

    if (CFStringCompare(sleepReason, CFSTR(kIOPMIdleSleepKey), 0) == kCFCompareEqualTo) {
        dict = mt2->idleSleepAppTimeouts;
    }
    else  if ((CFStringCompare(sleepReason, CFSTR(kIOPMClamshellSleepKey), 0) == kCFCompareEqualTo) ||
            (CFStringCompare(sleepReason, CFSTR(kIOPMPowerButtonSleepKey), 0) == kCFCompareEqualTo) ||
            (CFStringCompare(sleepReason, CFSTR(kIOPMSoftwareSleepKey), 0) == kCFCompareEqualTo)) {
        dict = mt2->demandSleepAppTimeouts;
    }
    else {
        dict = mt2->darkwakeSleepAppTimeouts;
    }

    int x = (int)CFDictionaryGetValue(dict, procName);
    x++;
    CFDictionarySetValue(dict, (const void *)procName, (void *)(uintptr_t)x);

}


#define kMT2DomainSleepWakeInfo     "com.apple.sleepwake.type"
#define kMT2DomainWakeTime          "com.apple.sleepwake.waketime"
#define kMT2DomainSleepWakeFailure  "com.apple.sleepwake.failure"
#define kMT2DomainSleepFailure      "com.apple.sleep.failure"
#define kMT2DomainWakeFailure       "com.apple.wake.failure"
#define kMT2KeyFailPhase            "com.apple.message.signature"
#define kMT2KeyPCI                  "com.apple.message.signature2"
#define kMT2KeyFailType             "com.apple.message.signature3"
#define kMT2KeySuccessCount         "com.apple.message.value"
#define kMT2KeyResult               "com.apple.message.result"
#define kMT2ValResultPass           "pass"
#define kMT2ValResultFail           "fail"
#define kMT2ValUnknown              "Unknown"


void mt2PublishSleepWakeInfo(CFStringRef wakeTypeStr, CFStringRef claimedWakeStr, bool success)
{
    char wakeType[64];
    char claimedWake[64];
    char sleepReason[64];
    const char *sleepTypeString;
    
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainSleepWakeInfo);
    
    if ((sleepTypeString = getSleepTypeString())) {
        asl_set(m, "com.apple.message.signature", sleepTypeString);
    } else {
        asl_set(m, "com.apple.message.signature", kMT2ValUnknown);
    }
    
    if (CFStringGetCString(_getSleepReason(), sleepReason, sizeof(sleepReason), kCFStringEncodingUTF8)) {
        asl_set(m, "com.apple.message.signature2", sleepReason);
    } else {
        asl_set(m, "com.apple.message.signature2", kMT2ValUnknown);
    }
    
    if (isA_CFString(wakeTypeStr) &&
        CFStringGetCString(wakeTypeStr, wakeType, sizeof(wakeType), kCFStringEncodingUTF8)) {
        asl_set(m, "com.apple.message.signature3", wakeType);
    } else {
        asl_set(m, "com.apple.message.signature3", kMT2ValUnknown);
    }
    
    if (isA_CFString(claimedWakeStr) &&
        CFStringGetCString(claimedWakeStr, claimedWake, sizeof(claimedWake), kCFStringEncodingUTF8)) {
        asl_set(m, "com.apple.message.signature4", claimedWake);
    } else {
        asl_set(m, "com.apple.message.signature4", kMT2ValUnknown);
    }
    
    if (success) {
        asl_set(m, kMT2KeyResult, kMT2ValResultPass);
    } else {
        asl_set(m, kMT2KeyResult, kMT2ValResultFail);
    }
    
    asl_set(m, "com.apple.message.summarize", "YES");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
    asl_release(m);
}

void mt2PublishWakeTime(double waketime, WakeTypeEnum waketype)
{
    // Publish wake times
    aslmsg m_wake = asl_new(ASL_TYPE_MSG);
    asl_set(m_wake, "com.apple.message.domain", kMT2DomainWakeTime);
    char waketimeBuf[128];
    char waketypeBuf[128];
    const char *sleepTypeString;
    snprintf(waketimeBuf, sizeof(waketimeBuf), "%lf", waketime);
    asl_set(m_wake, "com.apple.message.waketime", waketimeBuf);
    if (waketype == kIsDarkToFullWake)
        snprintf(waketypeBuf, sizeof(waketypeBuf), "%s", "Full Wake from Dark Wake");
    else {
        sleepTypeString = getSleepTypeString();
        if (waketype == kIsDarkWake)
            snprintf(waketypeBuf, sizeof(waketypeBuf), "Dark Wake from %s", sleepTypeString);
        else if (waketype == kIsFullWake)
            snprintf(waketypeBuf, sizeof(waketypeBuf), "Full Wake from %s", sleepTypeString);
    }
    asl_set(m_wake, "com.apple.message.waketype", waketypeBuf);
    asl_log(NULL, m_wake, ASL_LEVEL_NOTICE, "");
    asl_release(m_wake);
}

void mt2PublishSleepFailure(const char *phase, const char *pci_string)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainSleepFailure);
    asl_set(m, kMT2KeyFailPhase, phase);
    asl_set(m, kMT2KeyPCI, pci_string);
    asl_set(m, "com.apple.message.summarize", "YES");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
    asl_release(m);
}

void mt2PublishWakeFailure(const char *phase, const char *pci_string)
{
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainWakeFailure);
    asl_set(m, kMT2KeyFailPhase, phase);
    asl_set(m, kMT2KeyPCI, pci_string);
    asl_set(m, "com.apple.message.summarize", "YES");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
    asl_release(m);
}


void mt2PublishSleepWakeFailure(const char *failType, const char *phase, const char *pci_string)
{
    char buf[8];

    if (sleepCntSinceFailure == -1) {
        initSleepCnt();
    }
    aslmsg m = asl_new(ASL_TYPE_MSG);
    asl_set(m, "com.apple.message.domain", kMT2DomainSleepWakeFailure);
    asl_set(m, kMT2KeyFailType, failType);
    asl_set(m, kMT2KeyFailPhase, phase);
    asl_set(m, kMT2KeyPCI, pci_string);
    snprintf(buf, sizeof(buf), "%d", sleepCntSinceFailure);
    asl_set(m, kMT2KeySuccessCount, buf);
    asl_set(m, "com.apple.message.summarize", "NO");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
    asl_release(m);

    if (!strncmp(failType, kPMASLSleepFailureType, strlen(failType))) {
        mt2PublishSleepFailure(phase, pci_string);
    }
    else if (!strncmp(failType, kPMASLWakeFailureType, strlen(failType))) {
        mt2PublishWakeFailure(phase, pci_string);
    } 

    sleepCntSinceFailure = 0;
    logASLMessageHibernateStatistics();
}

/*****************************************************************************/

static void logASLMessageHibernateStatistics(void)
{
    aslmsg                  m = NULL;
    CFDataRef               statsData = NULL;
    PMStatsStruct           *stats = NULL;
    uint64_t                readHIBImageMS = 0;
    uint64_t                writeHIBImageMS = 0;
    CFNumberRef             hibernateModeNum = NULL;
    CFNumberRef             hibernateDelayNum = NULL;
    int                     hibernateMode = 0;
    char                    valuestring[25];
    int                     hibernateDelay = 0;
    int64_t                 hibernateDelayHigh = 0;
    char                    buf[100];
    char                    uuidString[150];

    if (sleepCntSinceFailure == -1) {
        initSleepCnt();
        if (sleepCntSinceFailure < 0) {
            sleepCntSinceFailure = 0;
        }
    }
    m = new_msg_pmset_log();

    asl_set(m, kPMASLDomainKey, kPMASLDomainHibernateStatistics);

    asl_set(m, ASL_KEY_LEVEL, ASL_STRING_NOTICE);

    if (_getUUIDString(uuidString, sizeof(uuidString))) {
        asl_set(m, kPMASLUUIDKey, uuidString);
    }

    snprintf(buf, sizeof(buf), "%d", sleepCntSinceBoot);
    asl_set(m, kPMASLSleepCntSinceBoot, buf);
    snprintf(buf, sizeof(buf), "%d", sleepCntSinceFailure);
    asl_set(m, kPMASLSleepCntSinceFailure, buf);

    hibernateModeNum = (CFNumberRef)_copyRootDomainProperty(CFSTR(kIOHibernateModeKey));
    if (hibernateModeNum) {
        CFNumberGetValue(hibernateModeNum, kCFNumberIntType, &hibernateMode);
        CFRelease(hibernateModeNum);
    }

    hibernateDelayNum= (CFNumberRef)_copyRootDomainProperty(CFSTR(kIOPMDeepSleepDelayKey));
    if (hibernateDelayNum) {
        CFNumberGetValue(hibernateDelayNum, kCFNumberIntType, &hibernateDelay);
        CFRelease(hibernateDelayNum);
    }

    GetPMSettingNumber(CFSTR(kIOPMDeepSleepDelayHighKey), &hibernateDelayHigh);

    statsData = (CFDataRef)_copyRootDomainProperty(CFSTR(kIOPMSleepStatisticsKey));
    if (statsData && (stats = (PMStatsStruct *)CFDataGetBytePtr(statsData)))
    {
        writeHIBImageMS = (stats->hibWrite.stop - stats->hibWrite.start)/1000000UL;

        readHIBImageMS =(stats->hibRead.stop - stats->hibRead.start)/1000000UL;

    }

    snprintf(valuestring, sizeof(valuestring), "hibernatemode=%d", hibernateMode);
    asl_set(m, kPMASLSignatureKey, valuestring);
    // If readHibImageMS == zero, that means we woke from the contents of memory
    // and did not read the hibernate image.
    if (writeHIBImageMS)
        snprintf(buf, sizeof(buf), "wr=%qd ms ", writeHIBImageMS);

    if (readHIBImageMS)
        snprintf(buf, sizeof(buf), "rd=%qd ms", readHIBImageMS);
    asl_set(m, kPMASLDelayKey, buf);

    snprintf(buf, sizeof(buf), "hibmode=%d standbydelaylow=%d standbydelayhigh=%lld", hibernateMode, hibernateDelay, hibernateDelayHigh);
    asl_set(m, ASL_KEY_MSG, buf);

    if (m) {
        asl_send(NULL, m);
        asl_release(m);
    }
    if(statsData)
        CFRelease(statsData);
    return;
}


#pragma mark FDR

/*************************
  FDR functionality
 *************************/
void recordFDREvent(int eventType, bool checkStandbyStatus,  IOPMBattery **batteries)
{

    struct systemstats_sleep_s s;
    struct systemstats_wake_s w;
    struct systemstats_power_state_change_s psc;
    IOPMBattery *b;
    char wt[50];
    wt[0] = 0;

    switch(eventType) {
        case kFDRInit:
            systemstats_init(SYSTEMSTATS_WRITER_powerd, NULL);
            break;

        case kFDRACChanged:
            if(!batteries)
                return;

            b = batteries[0];

            bzero(&psc, sizeof(struct systemstats_power_state_change_s));
            psc.now_on_battery = !(b->externalConnected);
            psc.is_fully_charged = isFullyCharged(b);
            systemstats_write_power_state_change(&psc);
            nextFDREventDue = (uint64_t)kFDRIntervalAfterPE + getMonotonicTime();
            break;

        case kFDRSleepEvent:
            bzero(&s, sizeof(struct systemstats_sleep_s));
            s.reason = 0;
            systemstats_write_sleep(&s);
            nextFDREventDue = (uint64_t)kFDRRegularInterval + getMonotonicTime();
            break;

        case kFDRUserWakeEvent:
        case kFDRDarkWakeEvent:

            bzero(&w, sizeof(struct systemstats_wake_s));
            if (kFDRUserWakeEvent == eventType) {
                w.reason = 1;
            } else if (kFDRDarkWakeEvent == eventType) {
                w.reason = 2;
            }

            if (checkStandbyStatus) {
                const char *sleepTypeString = getSleepTypeString();
                if(sleepTypeString &&
                    (!strncmp(sleepTypeString, "Standby", 15) ||
                     !strncmp(sleepTypeString, "AutoPowerOff", 15))) {
                    w.wake_from_standby = true;
                }
            }

            if (isA_CFString(reasons.interpretedWake)) {
                if (CFStringGetCString(reasons.interpretedWake, wt, sizeof(wt),
                    kCFStringEncodingUTF8) && wt[0]) {
                    w.wake_type = wt;
                }
            }
            
            systemstats_write_wake(&w);
            nextFDREventDue = (uint64_t)kFDRIntervalAfterPE + getMonotonicTime();
            break;

        case kFDRBattEventPeriodic:
            // If last FDR event was < X mins ago, do nothing
            if(nextFDREventDue && getMonotonicTime() <= nextFDREventDue)  {
                break;
            }

            // Fall thru
        case kFDRBattEventAsync:
            if(!batteries)
                return;

            IOPMBattery *b = batteries[0];
            struct systemstats_battery_charge_level_s binfo;
            bzero(&binfo, sizeof(struct systemstats_battery_charge_level_s));

            binfo.charge = b->currentCap;
            binfo.max_charge = b->maxCap;
            binfo.cycle_count = b->cycleCount;
            binfo.instant_amperage = b->instantAmperage;
            binfo.instant_voltage = b->voltage;
            binfo.last_minute_wattage = ((abs(b->voltage))*(abs(b->avgAmperage)))/1000;
            binfo.estimated_time_remaining = b->hwAverageTR;
            binfo.smoothed_estimated_time_remaining = b->swCalculatedTR;
            binfo.is_fully_charged = isFullyCharged(b);

            systemstats_write_battery_charge_level(&binfo);
            nextFDREventDue = (uint64_t)kFDRRegularInterval + getMonotonicTime();
            break;

        default:
            return;
    }

}

#pragma mark SMC and Hardware
/************************* One off hack for AppleSMC
 *************************
 ************************* Send AppleSMC a kCFPropertyTrue
 ************************* on time discontinuities.
 *************************/

static void setSMCProperty(void)
{
    static io_registry_entry_t     _smc = MACH_PORT_NULL;

    if(MACH_PORT_NULL == _smc) {
        _smc = IOServiceGetMatchingService( MACH_PORT_NULL,
                        IOServiceMatching("AppleSMCFamily"));
    }

    if(!_smc) {
        return;
    }

    // And simply AppleSMC with kCFBooleanTrue to let them know time is changed.
    // We don't pass any information down.
    IORegistryEntrySetCFProperty( _smc,
                        CFSTR("TheTimesAreAChangin"),
                        kCFBooleanTrue);
}

static kern_return_t armMachCalendarNotification(mach_port_t port)
{
    kern_return_t  result;
    mach_port_t    host_port;

    // register for notification
    host_port = mach_host_self();
    result = host_request_notification(host_port,HOST_NOTIFY_CALENDAR_CHANGE, port);
    if (host_port) {
        mach_port_deallocate(mach_task_self(), host_port);
    }

    return result;
}

static void calChangeReceiveChannelHandler(void *context, dispatch_mach_reason_t reason,
                                           dispatch_mach_msg_t message, mach_error_t error)
{
    if (reason == DISPATCH_MACH_MESSAGE_RECEIVED) {
        mach_msg_header_t *hdr = dispatch_mach_msg_get_msg(message, NULL);
        if (armMachCalendarNotification(hdr->msgh_local_port) == KERN_SUCCESS) {
            setSMCProperty();
        }
        mach_msg_destroy(hdr);
    }
}

static void registerForCalendarChangedNotification(void)
{
    static dispatch_mach_t        calChangeReceiveChannel = NULL;
    mach_port_t port;

    // allocate the mach port we'll be listening to
    port = mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE, &port);
    if (port != KERN_SUCCESS) {
        return;
    }

    calChangeReceiveChannel = dispatch_mach_create_f("PowerManagement/calendar", _getPMMainQueue(), NULL, calChangeReceiveChannelHandler);
    dispatch_mach_connect(calChangeReceiveChannel, port, MACH_PORT_NULL, NULL);

    armMachCalendarNotification(port);
}

__private_extern__ uint64_t monotonicTS2Secs(uint64_t tsc)
{
    static mach_timebase_info_data_t    timebaseInfo;

    if (timebaseInfo.denom == 0) {
        mach_timebase_info(&timebaseInfo);
    }

    return ( (tsc * timebaseInfo.numer) / (timebaseInfo.denom * NSEC_PER_SEC));

}
#if !POWERD_IOS_XCTEST
/* Returns monotonic continuous time in secs */
__private_extern__ uint64_t getMonotonicContinuousTime( )
{
    return monotonicTS2Secs(mach_continuous_time());
}
#endif

/* Returns monotonic time in secs */
__private_extern__ uint64_t getMonotonicTime( )
{
    return monotonicTS2Secs(mach_absolute_time());
}

__private_extern__ int callerIsRoot(int uid)
{
    return (0 == uid);
}

__private_extern__ int
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
    if (!pw)
        return false;

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

__private_extern__ int
callerIsConsole(
    int uid,
    int gid)
{
    CFStringRef                 user_name = NULL;
    uid_t                       console_uid;
    gid_t                       console_gid;

    user_name = (CFStringRef)SCDynamicStoreCopyConsoleUser(NULL,
                                    &console_uid, &console_gid);

    if(user_name) {
        CFRelease(user_name);
        return ((uid == console_uid) && (gid == console_gid));
    } else {
        // no data returned re: console user's uid or gid; return "false"
        return false;
    }
}


void _oneOffHacksSetup(void)
{
    registerForCalendarChangedNotification();
}

static const CFTimeInterval kTimeNSPerSec = 1000000000.0;

CFTimeInterval _getHIDIdleTime(void)
{
    static io_registry_entry_t hidsys = IO_OBJECT_NULL;
    CFNumberRef     hidsys_idlenum = NULL;
    CFTimeInterval  ret_time = 0.0;
    uint64_t        idle_nanos = 0;

    if (IO_OBJECT_NULL == hidsys) {
        hidsys = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOHIDSystem"));
    }
    if (!hidsys)
        goto exit;

    hidsys_idlenum = IORegistryEntryCreateCFProperty(hidsys, CFSTR(kIOHIDIdleTimeKey), 0, 0);

    if (!isA_CFNumber(hidsys_idlenum))
        goto exit;

    if (CFNumberGetValue(hidsys_idlenum, kCFNumberSInt64Type, &idle_nanos))
    {
        ret_time = ((CFTimeInterval)idle_nanos)/kTimeNSPerSec;
    }

exit:
    if (hidsys_idlenum)
        CFRelease(hidsys_idlenum);
    return ret_time;
}

/************************************************************************/

// Code to read AppleSMC

/************************************************************************/
__private_extern__ CFDictionaryRef _copyACAdapterInfo(CFDictionaryRef oldACDict)
{
    return NULL;
}
/************************************************************************/
__private_extern__ PowerSources _getPowerSource(void)
{
   IOPMBattery      **batteries;

#ifdef XCTEST
    return xctPowerSource;
#endif

   if (_batteryCount() && (batteries = _batteries())
            && (!batteries[0]->externalConnected) )
      return kBatteryPowered;
   else
      return kACPowered;
}


bool smcSilentRunningSupport(void)
{
   return false;
}



/*****************************************************************************/
/*****************************************************************************/

__private_extern__ IOReturn getNvramArgInt(char *key, int *value)
{
    io_registry_entry_t optionsRef;
    IOReturn ret = kIOReturnError;
    CFDataRef   dataRef = NULL;
    int *dataPtr = NULL;
    kern_return_t       kr;
    CFMutableDictionaryRef dict = NULL;
    CFStringRef keyRef = NULL;


    optionsRef = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if (optionsRef == 0)
        return kIOReturnError;

    kr = IORegistryEntryCreateCFProperties(optionsRef, &dict, 0, 0);
    if (kr != KERN_SUCCESS)
        goto exit;

    keyRef = CFStringCreateWithCStringNoCopy(0, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (!isA_CFString(keyRef))
        goto exit;
    dataRef = CFDictionaryGetValue(dict, keyRef);

    if (!dataRef)
        goto exit;

    dataPtr = (int*)CFDataGetBytePtr(dataRef);
    *value = *dataPtr;

    ret = kIOReturnSuccess;

exit:
    if (keyRef) CFRelease(keyRef);
    if (dict) CFRelease(dict);
    IOObjectRelease(optionsRef);
    return ret;
}

__private_extern__ IOReturn getNvramArgStr(char *key, char *buf, size_t bufSize)
{
    io_registry_entry_t optionsRef;
    IOReturn ret = kIOReturnError;
    CFStringRef   dataRef = NULL;
    kern_return_t       kr;
    CFMutableDictionaryRef dict = NULL;
    CFStringRef keyRef = NULL;


    optionsRef = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if (optionsRef == 0)
        return kIOReturnError;

    kr = IORegistryEntryCreateCFProperties(optionsRef, &dict, 0, 0);
    if (kr != KERN_SUCCESS)
        goto exit;

    keyRef = CFStringCreateWithCStringNoCopy(0, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (!isA_CFString(keyRef))
        goto exit;
    dataRef = CFDictionaryGetValue(dict, keyRef);

    if (!isA_CFString(dataRef))
        goto exit;

    if (CFStringGetCString(dataRef, buf, bufSize, kCFStringEncodingUTF8)) {
        ret = kIOReturnSuccess;
    }
    else {
        ret = kIOReturnNoSpace;
    }


exit:
    if (keyRef) CFRelease(keyRef);
    if (dict) CFRelease(dict);
    IOObjectRelease(optionsRef);
    return ret;
}

int
pluginExecCommand(const char *path, char *const argv[],
		dispatch_queue_t queue,
		void (*callback)(pid_t pid, int status))
{
    posix_spawn_file_actions_t fattr;
    pid_t pid = -1;
    int save_errno, rc;

    posix_spawn_file_actions_init(&fattr);
    posix_spawn_file_actions_addopen(&fattr, STDIN_FILENO,  "/dev/null", O_RDONLY, 0666);
    posix_spawn_file_actions_addopen(&fattr, STDOUT_FILENO, "/dev/null", O_WRONLY, 0666);
    posix_spawn_file_actions_addopen(&fattr, STDERR_FILENO, "/dev/null", O_WRONLY, 0666);

    rc = posix_spawn(&pid, path, &fattr, NULL, argv, *_NSGetEnviron());
    save_errno = errno;

    posix_spawn_file_actions_destroy(&fattr);

    if (rc == -1) {
        errno = save_errno;
        return -1;
    }

    if (callback) {
        dispatch_source_t src;

        src = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
                                     pid, DISPATCH_PROC_EXIT, queue);
        dispatch_source_set_event_handler(src, ^{
            int status;
            waitpid(pid, &status, 0);
            callback(pid, status);
            dispatch_source_cancel(src);
            dispatch_release(src);
        });
        dispatch_activate(src);
    }
    return 0;
}
