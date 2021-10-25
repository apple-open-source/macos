/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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
#include <CoreFoundation/CFPriv.h>
#include <mach/mach.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include "IOSystemConfiguration.h"
#include <sys/time.h>
#include <notify.h>
#include <execinfo.h>
#include <asl.h>
#include <mach/mach_time.h>
#if !TARGET_OS_SIMULATOR
#include <energytrace.h>
#endif
#include <os/log.h>
#include <xpc/xpc.h>
#define POWERD_XPC_ID   "com.apple.iokit.powerdxpc"

#include "powermanagement_mig.h"
#include "powermanagement.h"

#include <servers/bootstrap.h>

#if !defined(_OPEN_SOURCE_) && !TARGET_OS_IPHONE
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dlfcn.h>
Boolean _CSCheckFix(CFStringRef str);
#endif        // !_OPEN_SOURCE_ && !TARGET_OS_IPHONE

#define kAssertionsArraySize        5
#define NUM_BT_FRAMES               8

static os_log_t    assertions_log = NULL;
#define LOG_STREAM  assertions_log
#define DEBUG 1
#if DEBUG
#define DEBUG_LOG(fmt, args...) \
{ \
    if (assertions_log) \
    os_log_debug(LOG_STREAM, fmt, ##args); \
    else \
    os_log_debug(OS_LOG_DEFAULT, fmt, ##args);\
}
#else
#define DEBUG_LOG(fmt, args...) {}
#endif

#define ERROR_LOG(fmt, args...) \
{  \
    if (assertions_log) \
    os_log_error(LOG_STREAM, fmt, ##args); \
    else\
    os_log_error(OS_LOG_DEFAULT, fmt, ##args); \
}

#define INFO_LOG(fmt, args...) \
{  \
    if (assertions_log) \
    os_log(LOG_STREAM, fmt, ##args); \
    else \
    os_log(OS_LOG_DEFAULT, fmt, ##args); \
}

static uint64_t  collectBackTrace = 0;

IOReturn _pm_connect(mach_port_t *newConnection);
IOReturn _pm_disconnect(mach_port_t connection);
__private_extern__ IOReturn _copyPMServerObject(int selector, int assertionID,
                                                CFTypeRef selectorData, CFTypeRef *objectOut);
__private_extern__ io_registry_entry_t getPMRootDomainRef(void);
dispatch_queue_t  getPMQueue();

static IOReturn pm_connect_init(mach_port_t *newConnection)
{
#if !TARGET_OS_IPHONE
    static int              disableAppSleepToken = 0;
    static int              enableAppSleepToken = 0;
#endif
    static int              collectBackTraceToken = 0;

#if !TARGET_OS_IPHONE
    if ( !disableAppSleepToken ) {
        char notify_str[128];

        snprintf(notify_str, sizeof(notify_str), "%s.%d", 
                 kIOPMDisableAppSleepPrefix, getpid());

        notify_register_dispatch(
                                 notify_str,
                                 &disableAppSleepToken,
                                 dispatch_get_main_queue(),
                                 ^(int t __unused){
                                 __CFRunLoopSetOptionsReason(__CFRunLoopOptionsTakeAssertion, 
                                                             CFSTR("App is holding power assertion."));
                                 });
    }

    if ( !enableAppSleepToken ) {
        char notify_str[128];

        snprintf(notify_str, sizeof(notify_str), "%s.%d", 
                 kIOPMEnableAppSleepPrefix, getpid());

        notify_register_dispatch(
                                 notify_str,
                                 &enableAppSleepToken,
                                 dispatch_get_main_queue(),
                                 ^(int t __unused){
                                 __CFRunLoopSetOptionsReason(__CFRunLoopOptionsDropAssertion, 
                                                             CFSTR("App released all power assertions."));
                                 });
    }
#endif

    if (!collectBackTraceToken) {
        notify_register_dispatch(
                                 kIOPMAssertionsCollectBTString,
                                 &collectBackTraceToken,
                                 dispatch_get_main_queue(),
                                 ^(int t){
                                     notify_get_state(t, &collectBackTrace);
                                 });
        notify_get_state(collectBackTraceToken, &collectBackTrace);

    }

    return _pm_connect(newConnection);
}

static IOReturn pm_connect_close(mach_port_t connection)
{
    return _pm_disconnect(connection);
}

static inline void saveBackTrace(CFMutableDictionaryRef props)
{

    void *              bt[NUM_BT_FRAMES];
    char                **syms = NULL;
    int                 i;
    CFStringRef         frame_cf = NULL;
    CFMutableArrayRef   syms_cf = NULL;


    int nframes = backtrace((void**)(&bt), NUM_BT_FRAMES);

    syms = backtrace_symbols(bt, nframes);
    syms_cf = CFArrayCreateMutable(0, nframes, &kCFTypeArrayCallBacks);   
    if (syms && syms_cf) {
        for (i = 0; i < nframes; i++) {
            frame_cf = NULL;
            frame_cf = CFStringCreateWithCString(NULL, syms[i], 
                                                 kCFStringEncodingMacRoman);
            if (frame_cf) {
                CFArrayInsertValueAtIndex(syms_cf, i, frame_cf);
                CFRelease(frame_cf);
            }
            else {
                CFArrayInsertValueAtIndex(syms_cf, i, CFSTR(" "));
            }
        }
        CFDictionarySetValue(props, kIOPMAssertionCreatorBacktrace, syms_cf);
    }

    if (syms_cf) CFRelease(syms_cf);
    if (syms) free(syms);
}


#if !defined(_OPEN_SOURCE_) && !TARGET_OS_IPHONE

static void * __loadCarbonCore(void) 
{
    static void *image = NULL;
    if (NULL == image) {
        const char  *framework = "/System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/CarbonCore";
        struct stat statbuf;
        const char  *suffix     = getenv("DYLD_IMAGE_SUFFIX");
        char    path[MAXPATHLEN];

        strlcpy(path, framework, sizeof(path));
        if (suffix) strlcat(path, suffix, sizeof(path));
        if (0 <= stat(path, &statbuf)) {
            image = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
        } else {
            image = dlopen(framework, RTLD_LAZY | RTLD_LOCAL);
        }
    }
    return (void *)image;
}


static Boolean __CSCheckFix(CFStringRef str)
{
    static typeof (_CSCheckFix) *dyfunc = NULL;
    if (!dyfunc) {
        void *image = __loadCarbonCore();
        if (image) dyfunc = dlsym(image, "_CSCheckFix");
    }
    return dyfunc ? dyfunc(str) : TRUE;
}
#define _CSCheckFix __CSCheckFix

#endif  // !_OPEN_SOURCE_ && !TARGET_OS_IPHONE

/*************************************************************************************************************
 Async PowerAssertions
 *************************************************************************************************************/
#define kMaxAsyncAssertions         128 // Max number of assertions at a time per process
#define kMaxAsyncAssertionsLog      16 // Max number of assertions sent to powerd during activity update

// Async assertions uses upper 16-bits to store the idx.
// Sync assertions uses lower-16 bits to store idx.
#define ASYNC_ID_FROM_IDX(idx)  (((idx & 0x7fff) | 0x8000) << 16)
#define ASYNC_IDX_FROM_ID(id)   (((id) >> 16) & 0x7fff)
#define IS_ASYNC_ID(id) ((id) & (0xffff << 16))
#define MAKE_UNIQUE_AID(time, id, pid) (((time & 0xffffffff) << 32 ) | ((pid & 0xffff) << 16) | ((id & 0xffff0000) >> 16))


static dispatch_source_t            gAssertionsOffloader = 0;
static CFMutableDictionaryRef       gAssertionsDict = NULL;
static CFMutableDictionaryRef       gActiveAssertionsDict = NULL; // active - turned on
static CFMutableDictionaryRef       gInactiveAssertionsDict = NULL; // assertions which have been turned off but not released
static CFMutableArrayRef            gReleasedAssertionsList = NULL; // released assertions. Kept around for logging
static CFMutableArrayRef            gTimedAssertionsList = NULL;
static dispatch_source_t            gAssertionTimer = 0;
static uint64_t                     nextOffload_ts;
static xpc_connection_t             gAssertionConnection;

// current local assertion id sent to powerd
static IOPMAssertionID              gCurrentAssertion = 0;

// current remote assertion id got from powerd. Powerd sends timeout messages with this id
static IOPMAssertionID              gCurrentRemoteAssertion = 0;
bool                                gAsyncMode = false;

bool                                gAsyncModeSetupDone = false;
// create assertion
bool createAsyncAssertion(CFDictionaryRef AssertionProperties, IOPMAssertionID *id);

// release assertion
IOReturn releaseAsyncAssertion(IOPMAssertionID id);
IOReturn _releaseAsycnAssertion(IOPMAssertionID id, bool removeTimed);

// set property
IOReturn setAsyncAssertionProperties(CFStringRef theProperty, CFTypeRef theValue, IOPMAssertionID AssertionID);

// inital setup
void initialSetup(void);

// xpc messages to powerd
IOPMAssertionID sendAsyncAssertionMsg(bool create, CFDictionaryRef dict, IOReturn *rc, CFArrayRef toLog);
void sendAsyncReleaseMsg(IOPMAssertionID remoteID, bool log);
void offloadAssertions(bool forced);

// xpc callbacks
void processRemoteMsg(xpc_object_t msg);
void processAssertionTimeout(xpc_object_t msg);
void processCheckAssertionsMsg(xpc_object_t msg);

// assertion timeouts
void handleAssertionTimeout(void);

// assertion level change
IOReturn handleAssertionLevel(CFTypeRef value, IOPMAssertionID assertionID);

// helper functions
void insertIntoTimedList(CFMutableDictionaryRef props);
void removeFromTimedList(IOPMAssertionID id);
bool activeAssertions();

void checkFeatureEnabled()
{
#if TARGET_OS_IOS
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        dispatch_sync(getPMQueue(), ^{
            xpc_object_t connection = xpc_connection_create_mach_service(POWERD_XPC_ID, dispatch_get_main_queue(), 0);
            if (!connection) {
                ERROR_LOG("Failed to create gAssertionConnection\n");
            }
            else {
                xpc_connection_set_target_queue(connection, getPMQueue());
                xpc_connection_set_event_handler(connection,
                                                 ^(xpc_object_t e __unused ) {});
                xpc_connection_resume(connection);

                // send a message to check feature
                xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);
                xpc_dictionary_set_bool(msg, kAssertionFeatureSupportKey, true);
                xpc_connection_send_message_with_reply(connection, msg, getPMQueue(), ^(xpc_object_t resp){
                    if (xpc_get_type(resp) == XPC_TYPE_DICTIONARY) {
                        bool feature = xpc_dictionary_get_bool(resp, kAssertionFeatureSupportKey);
                        INFO_LOG("Assertion feature: setting gAsyncMode to %d", feature);
                        gAsyncMode = feature;
                    }
                });
                if (msg) {
                    xpc_release(msg);
                }
                if (connection) {
                    xpc_release(connection);
                }
            }
        });
    });
#endif
}

IOReturn IOPMEnableAsyncAssertions()
{
    gAsyncMode = true;

    return kIOReturnSuccess;
}

IOReturn IOPMDisableAsyncAssertions()
{
    gAsyncMode = false;

    return kIOReturnSuccess;
}

uint64_t getMonotonicTime( )
{
    static mach_timebase_info_data_t    timebaseInfo;

    if (timebaseInfo.denom == 0)
        mach_timebase_info(&timebaseInfo);

    return ( (mach_absolute_time( ) * timebaseInfo.numer) / (timebaseInfo.denom * NSEC_PER_SEC));
}

void setupLogging()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        assertions_log = os_log_create("com.apple.iokit", "assertions");
    });
}

void initialSetup( )
{
    if (gAsyncModeSetupDone) {
        return;
    }
    setupLogging();
    // Keys into this array integers and doesn't need retain/release
    // Values are CFDictionary
    if (!gAssertionsDict) {
        gAssertionsDict = CFDictionaryCreateMutable(NULL, kMaxAsyncAssertions,
                NULL, &kCFTypeDictionaryValueCallBacks);
        if (!gAssertionsDict) {
            ERROR_LOG("Failed to create gAssertionsDict");
            goto error_exit;
        }
    }

    if (!gReleasedAssertionsList) {
        gReleasedAssertionsList = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    }
    if (!gTimedAssertionsList) {
        gTimedAssertionsList = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    }
    if (!gActiveAssertionsDict) {
        gActiveAssertionsDict = CFDictionaryCreateMutable(kCFAllocatorDefault, kMaxAsyncAssertions, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    if (!gInactiveAssertionsDict) {
        gInactiveAssertionsDict = CFDictionaryCreateMutable(kCFAllocatorDefault, kMaxAsyncAssertions, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    if (!gAssertionConnection) {
        gAssertionConnection = xpc_connection_create_mach_service(POWERD_XPC_ID, dispatch_get_main_queue(), 0);
        if (!gAssertionConnection) {
            ERROR_LOG("Failed to create gAssertionConnection\n");
            goto error_exit;
        }
        else {
            xpc_connection_set_target_queue(gAssertionConnection, getPMQueue());
            xpc_connection_set_event_handler(gAssertionConnection,
                                             ^(xpc_object_t msg ) {processRemoteMsg(msg); });
            xpc_connection_resume(gAssertionConnection);

            // send initial msg
            xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);
            if (msg) {
                xpc_dictionary_set_bool(msg, kAssertionInitialConnKey, true);
                xpc_connection_send_message(gAssertionConnection, msg);
                INFO_LOG("Sending initial message to powerd for async assertions");
                xpc_release(msg);
            } else {
                goto error_exit;
            }
        }
    }

    if (!gAssertionsOffloader) {
        gAssertionsOffloader = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, getPMQueue());
    }

    if (gAssertionsOffloader) {
        dispatch_source_set_event_handler(gAssertionsOffloader, ^{ offloadAssertions(false); });
        dispatch_source_set_cancel_handler(gAssertionsOffloader, ^{
                dispatch_release(gAssertionsOffloader);
                gAssertionsOffloader = NULL;
                });
    }
    gAsyncModeSetupDone = true;
    return;

error_exit:
    gAsyncModeSetupDone = false;
    if (gAssertionsDict) {
        CFRelease(gAssertionsDict);
        gAssertionsDict = NULL;
    }
    if (gReleasedAssertionsList) {
        CFRelease(gReleasedAssertionsList);
        gReleasedAssertionsList = NULL;
    }
    if (gAssertionsOffloader) {
        dispatch_resume(gAssertionsOffloader);
        dispatch_cancel(gAssertionsOffloader);
    }
    if (gInactiveAssertionsDict) {
        CFRelease(gInactiveAssertionsDict);
        gInactiveAssertionsDict = NULL;
    }
    if (gActiveAssertionsDict) {
        CFRelease(gActiveAssertionsDict);
        gActiveAssertionsDict = NULL;
    }
    if (gAssertionConnection) {
        xpc_connection_cancel(gAssertionConnection);
    }
}

void activateAsyncAssertion(IOPMAssertionID id)
{
    uint32_t  idx;
    uint64_t  timeout_secs = 0;
    uint64_t  timeout_ts = 0;
    CFNumberRef numRef;

    idx = ASYNC_IDX_FROM_ID(id);
    CFMutableDictionaryRef assertion = (CFMutableDictionaryRef)CFDictionaryGetValue(gAssertionsDict, idx);
    if (!assertion) {
        ERROR_LOG("Assertion 0x%x not found to activate", id);
        return;
    }
    // insert into active assertions
    CFDictionarySetValue(gActiveAssertionsDict, (uintptr_t)idx, (const void *)assertion);

    // timeout
    numRef = CFDictionaryGetValue(assertion, kIOPMAssertionTimeoutKey);
    if (isA_CFNumber(numRef))  {
        CFNumberGetValue(numRef, kCFNumberSInt64Type, &timeout_secs);
    }
    if (timeout_secs) {
        // There is timeout.
        timeout_ts = timeout_secs + getMonotonicTime();
        CFNumberRef cf_timeout_ts = CFNumberCreate(NULL, kCFNumberSInt64Type, &timeout_ts);
        if (cf_timeout_ts) {
            CFDictionarySetValue(assertion, kIOPMAsyncAssertionTimeoutTimestamp, cf_timeout_ts);
            CFRelease(cf_timeout_ts);
        }
        insertIntoTimedList(assertion);
    }
    if (!timeout_secs || (timeout_secs > kAsyncAssertionsDefaultOffloadDelay)) {
        timeout_secs = kAsyncAssertionsDefaultOffloadDelay;
        timeout_ts = getMonotonicTime() + timeout_secs;
    }
    if (!gCurrentAssertion && (!nextOffload_ts || (timeout_ts != 0 && timeout_ts < nextOffload_ts))) {
        INFO_LOG("Setting gAssertionsOffloader timeout to %llu\n", timeout_secs);
        dispatch_source_set_timer(gAssertionsOffloader,
                                  dispatch_time(DISPATCH_TIME_NOW, timeout_secs*NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
        if (!nextOffload_ts) {
            dispatch_resume(gAssertionsOffloader);
        }
        nextOffload_ts = getMonotonicTime() + timeout_secs;
    }
}

bool createAsyncAssertion(CFDictionaryRef AssertionProperties, IOPMAssertionID *id)
{
    __block bool created = true;
    static uint32_t gNextAssertionIdx = 0;
    CFStringRef     assertionTypeString;

    if (!gAsyncMode) {
        checkFeatureEnabled();
        return false;
    }

    assertionTypeString = CFDictionaryGetValue(AssertionProperties, kIOPMAssertionTypeKey);

    if (!isA_CFString(assertionTypeString) ||
            !(CFEqual(assertionTypeString, kIOPMAssertionTypePreventUserIdleSystemSleep) ||
              CFEqual(assertionTypeString, kIOPMAssertionTypeNoIdleSleep)) ) {
        return false;
    }
    dispatch_queue_t pmQ = getPMQueue();
    if (!pmQ) {
        return false;
    }

    dispatch_sync(pmQ, ^(void){

        uint32_t  idx;
        CFMutableDictionaryRef  mutableProps;
        CFNumberRef numRef;

        // Initial setup
        initialSetup();

        // Generate an id
        for (idx=gNextAssertionIdx; CFDictionaryContainsKey(gAssertionsDict, (uintptr_t)idx) == true;) {
            idx = (idx+1) % kMaxAsyncAssertions;
            if (idx == gNextAssertionIdx) break;
        }

        if (CFDictionaryContainsKey(gAssertionsDict, (uintptr_t)idx) == true) {
            created = false;
            ERROR_LOG("idx already exists. Returning false 0x%x", idx);
            return;
        }

        mutableProps = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, AssertionProperties);
        if (!mutableProps) {
            created = false;
            return;
        }

        // start date
        CFDateRef start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
        if (!start_date) {
            created = false;
            return;
        }
        CFDictionarySetValue(mutableProps, kIOPMAssertionCreateDateKey, start_date);
        CFRelease(start_date);

        // add assertion to dictionary
        CFDictionarySetValue(gAssertionsDict, (uintptr_t)idx, (const void *)mutableProps);

        // next assertion id
        gNextAssertionIdx = (idx+1) % kMaxAsyncAssertions;

        // return assertion id to client
        *id = ASYNC_ID_FROM_IDX(idx);
    
        numRef = CFNumberCreate(0, kCFNumberSInt32Type, id);
        DEBUG_LOG("Setting async client assertion id to 0x%x", *id);
        if (numRef) {
            CFDictionarySetValue(mutableProps, kIOPMAsyncClientAssertionIdKey, numRef);
            CFRelease(numRef);
        }

        // set global unique id
        uint64_t unique_id = MAKE_UNIQUE_AID(getMonotonicTime(), *id, getpid());
        CFNumberRef unique_id_cf = CFNumberCreate(0, kCFNumberSInt64Type, &unique_id);
        if (unique_id_cf) {
            CFDictionarySetValue(mutableProps, kIOPMAssertionGlobalUniqueIDKey, unique_id_cf);
            CFRelease(unique_id_cf);
        }

        // is a level set ? Defaults to On
        uint32_t level = kIOPMAssertionLevelOn;
        numRef = CFDictionaryGetValue(mutableProps, kIOPMAssertionLevelKey);
        if (isA_CFNumber(numRef)) {
            CFNumberGetValue(numRef, kCFNumberSInt32Type, &level);
        }
        if (level == kIOPMAssertionLevelOff) {
            // created assertion with level off
            // add to gTurnedOffAssertionsList and exit
            DEBUG_LOG("Initial level is off for 0x%x: %@", *id, mutableProps);
            CFDictionarySetValue(gInactiveAssertionsDict, (uintptr_t)idx, (const void *)mutableProps);
        } else {
            activateAsyncAssertion(*id);
        }


        DEBUG_LOG("Async assertion created with 0x%x, length %ld, %@\n", *id, (long)CFDictionaryGetCount(gActiveAssertionsDict), mutableProps);
        CFRelease(mutableProps);

    });
    return created;
}

IOReturn _releaseAsycnAssertion(IOPMAssertionID AssertionID, bool removeTimed)
{
    int idx = ASYNC_IDX_FROM_ID(AssertionID);
    CFTypeRef   value;

    if (!CFDictionaryGetValueIfPresent(gAssertionsDict, (uintptr_t)idx, (const void **)&value)) {
        ERROR_LOG("_releaseAsyncAssertion: Failed to get the assertion details for id:0x%x\n", AssertionID);
        return kIOReturnInvalid;
    }

    CFDateRef release_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    if (release_date) {
        CFDictionarySetValue((CFMutableDictionaryRef)value, kIOPMAssertionReleaseDateKey, release_date);
        CFRelease(release_date);
    }

    // remove from active or turned off list
    if (CFDictionaryContainsKey(gActiveAssertionsDict, (uintptr_t)idx)) {
        CFDictionaryRemoveValue(gActiveAssertionsDict, (uintptr_t)idx);
        // add to released list
        if (CFArrayGetCount(gReleasedAssertionsList) < kMaxAsyncAssertions) {
            CFArrayAppendValue(gReleasedAssertionsList, value);
        }
    } else if (CFDictionaryContainsKey(gInactiveAssertionsDict, (uintptr_t)idx)) {
        CFDictionaryRemoveValue(gInactiveAssertionsDict, (uintptr_t)idx);
        // not adding to released list because we would have logged the turn off event as
        // a release
        // if this assertion was timed out by powerd, let's send a release
        if (CFDictionaryContainsKey(value, kIOPMAsyncRemoteAssertionIdKey)) {

            CFNumberRef cf_remote_id = CFDictionaryGetValue(value, kIOPMAsyncRemoteAssertionIdKey);
            int remote_id = 0;
            CFNumberGetValue(cf_remote_id, kCFNumberSInt32Type, &remote_id);
            INFO_LOG("Powerd turned off this assertion. let's send a release for %u", (uint32_t)remote_id);
            sendAsyncReleaseMsg(remote_id, false);
        }
    }

    // remove from gAssertionDict
    CFDictionaryRemoveValue(gAssertionsDict, (uintptr_t)idx);

    if (removeTimed) {
        // remove from timed list
        removeFromTimedList(AssertionID);
    }

    /* If this the last assertion to be released, send the full dictionary
     */
    if (!activeAssertions() && gCurrentAssertion) {
        // No active assertions . Let's send this release to powerd
        sendAsyncReleaseMsg(gCurrentRemoteAssertion, true);
        gCurrentAssertion = 0;
        gCurrentRemoteAssertion = 0;
        DEBUG_LOG("Released assertion id gCurrentRemoteAssertion: 0x%x: 0x%x.Logging everything to powerd %@ %ld", gCurrentRemoteAssertion, AssertionID, gReleasedAssertionsList, CFArrayGetCount(gReleasedAssertionsList));

    } else {
        // we haven't sent this to powerd yet
        DEBUG_LOG("Releasing assertion locally id 0x%x", AssertionID);
    }
    return kIOReturnSuccess;
}

IOReturn releaseAsyncAssertion(IOPMAssertionID AssertionID)
{

    __block IOReturn ret = kIOReturnSuccess;
    dispatch_queue_t pmQ = getPMQueue();
    if (!pmQ) {
        ret = kIOReturnInternalError;
    }

    dispatch_sync(pmQ, ^{
        ret = _releaseAsycnAssertion(AssertionID, true);

    });
    return  ret;

}


IOReturn setAsyncAssertionProperties(CFStringRef theProperty, CFTypeRef theValue, IOPMAssertionID AssertionID)
{

    __block IOReturn ret = kIOReturnSuccess;
    dispatch_queue_t pmQ = getPMQueue();
    if (!pmQ) {
        ret = kIOReturnInternalError;
        return ret;
    }
    DEBUG_LOG("setAsyncAssertionProperties for 0x%x %@:%@", AssertionID, theProperty, theValue);
    dispatch_sync(pmQ, ^{
        int idx = ASYNC_IDX_FROM_ID(AssertionID);
        CFTypeRef   value;
        IOReturn rc = kIOReturnSuccess;
        bool level_change = false;
        if (!CFDictionaryGetValueIfPresent(gAssertionsDict, (uintptr_t)idx, (const void **)&value)) {
            ERROR_LOG("setAsyncAssertionProperties: Failed to get the assertion details for id:0x%x\n", AssertionID);
            ret = kIOReturnInvalid;
        }
        else if (isA_CFDictionary(value)) {
            CFMutableDictionaryRef props = (CFMutableDictionaryRef)value;
            CFDictionarySetValue(props, theProperty, theValue);
            if (CFStringCompare((CFStringRef)theProperty, kIOPMAssertionTimeoutKey, 0) == kCFCompareEqualTo) {
                // is this assertion active
                if (CFDictionaryContainsKey(gActiveAssertionsDict, (uintptr_t)idx)) {
                    // timeout was modified
                    removeFromTimedList(AssertionID);
                    insertIntoTimedList(props);
                }
            } else if (CFStringCompare((CFStringRef)theProperty, kIOPMAssertionLevelKey, 0) == kCFCompareEqualTo) {
                // assertion level was modified
                ret = handleAssertionLevel(theValue, AssertionID);
                level_change = true;
            }
            /*
             Level changes are handled in IOKitUser. Powerd is only sent create and release
             */
            if (CFDictionaryContainsKey(props, kIOPMAsyncRemoteAssertionIdKey) && !level_change) {
                // this assertion has been sent to powerd. Let's send an update
                CFMutableDictionaryRef updates = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(updates, theProperty, theValue);
                CFTypeRef remote_id = CFDictionaryGetValue(props, kIOPMAsyncRemoteAssertionIdKey);
                CFDictionarySetValue(updates, kIOPMAsyncRemoteAssertionIdKey, remote_id);
                sendAsyncAssertionMsg(false, updates, &rc, NULL);
                CFRelease(updates);
            }
        }
        else {
            ERROR_LOG("Unexpected data type in gAssertionsDict for id:0x%x\n", AssertionID);
            ret = kIOReturnInternalError;
        }
    });
    return ret;
}



/*
 * Send AssertionCreate msg to powerd
 * Send property update msg to powerd
 * Send logging information to powerd
*/

IOPMAssertionID sendAsyncAssertionMsg(bool create, CFDictionaryRef dict, IOReturn *rc, CFArrayRef toLog)
{
    xpc_object_t            desc = NULL;
    xpc_object_t            msg = NULL;
    xpc_object_t            resp = NULL;
    IOPMAssertionID         remoteID = kIOPMNullAssertionID;
    xpc_object_t            data = NULL;

    if (!gAssertionConnection) {
        initialSetup();
    }

    msg = xpc_dictionary_create(NULL, NULL, 0);
    if (!msg) {
        ERROR_LOG("Failed to create xpc msg object\n");
        return remoteID;
    }

    if (!dict && !toLog) {
        ERROR_LOG("No assertion dictionary or logging data to send");
        return remoteID;
    }
    if (dict) {
        desc = _CFXPCCreateXPCMessageWithCFObject(dict);
        if (!desc) {
            ERROR_LOG("Failed to convert CF dictionary to xpc object\n");
            return remoteID;
        }
    }
    if (toLog != NULL && CFArrayGetCount(toLog)!= 0) {
        DEBUG_LOG("Logging all %ld activity to powerd\n", CFArrayGetCount(toLog));
        data = _CFXPCCreateXPCObjectFromCFObject(toLog);
        xpc_dictionary_set_value(msg, kAssertionActivityLogKey, data);
    }

    if (create) {
        xpc_dictionary_set_value(msg, kAssertionCreateMsg, desc);

        resp = xpc_connection_send_message_with_reply_sync(gAssertionConnection, msg);
        if (xpc_get_type(resp) == XPC_TYPE_DICTIONARY) {
            remoteID = xpc_dictionary_get_uint64(resp, kAssertionIdKey);
            if (rc) {
                *rc = xpc_dictionary_get_uint64(resp, kMsgReturnCode);
            }
        } else if (xpc_get_type(resp) == XPC_TYPE_ERROR) {
            if (resp == XPC_ERROR_CONNECTION_INTERRUPTED) {
                ERROR_LOG("sendAsyncAssertionMsg failed: powerd xpc connection interrupted");
            } else if (resp == XPC_ERROR_CONNECTION_INVALID) {
                ERROR_LOG("sendAsyncAssertionMsg failed: powerd xpc connection invalid");
            }
        }

    }
    else {
        if (dict) {
            xpc_dictionary_set_value(msg, kAssertionPropertiesMsg, desc);
        }
        xpc_connection_send_message(gAssertionConnection, msg);
    }
    if (desc) {
        xpc_release(desc);
    }
    if (data) {
        xpc_release(data);
    }
    if (resp) {
        xpc_release(resp);
    }
    xpc_release(msg);


    return remoteID;
}

/*
 * Only called when the last assertion is released
 */
void sendAsyncReleaseMsg(IOPMAssertionID remoteID, bool log)
{
    xpc_object_t            msg = NULL;
    xpc_object_t            data = NULL;

    msg = xpc_dictionary_create(NULL, NULL, 0);
    if (!msg) {
        ERROR_LOG("Failed to create xpc msg object\n");
        goto exit;
    }

    if (!gAssertionConnection) {
        ERROR_LOG("No connection to powerd");
        goto exit;
    }

    DEBUG_LOG("Sending Assertion release message for assertion Id 0x%x\n", remoteID);
    xpc_dictionary_set_uint64(msg, kAssertionReleaseMsg, remoteID);
    if (log) {
        if (gReleasedAssertionsList && CFArrayGetCount(gReleasedAssertionsList) != 0) {
            DEBUG_LOG("Logging all %ld activity to powerd\n", CFArrayGetCount(gReleasedAssertionsList));
            data = _CFXPCCreateXPCObjectFromCFObject(gReleasedAssertionsList);
            xpc_dictionary_set_value(msg, kAssertionActivityLogKey, data);
        }
    }
    xpc_connection_send_message(gAssertionConnection, msg);

    if (log) {
        // remove assertions from inactive list
        CFArrayRemoveAllValues(gReleasedAssertionsList);
    }
exit:
    if (data) {
        xpc_release(data);
    }
    if (msg) {
        xpc_release(msg);
    }

    return;
}

/*
 * If an active assertion exits - send it to powerd
 * Send any Inactive assertions for logging
 */
void offloadAssertions(bool forced)
{
    if (!gAsyncMode) {
        return;
    }

    int a_id;
    IOPMAssertionID remoteID;
    CFNumberRef remoteIDCf;
    IOReturn rc;

    DEBUG_LOG("offloadAssertions fired");
    if (!gCurrentAssertion && !activeAssertions()) {
        // no current assertion. Send all activity till now
        if (CFArrayGetCount(gReleasedAssertionsList) > 0) {
            DEBUG_LOG("No current assertion. Sending all activity till now");
            sendAsyncAssertionMsg(false, NULL, &rc, gReleasedAssertionsList);
            CFArrayRemoveAllValues(gReleasedAssertionsList);
            goto exit;
        }
    }

    if (gCurrentRemoteAssertion) {
        ERROR_LOG("offloadAssertions called with gCurrentRemoteAssertion 0x%x", gCurrentRemoteAssertion);
        goto exit;
    }
    for (int idx = 0; idx < kMaxAsyncAssertions; idx++) {
        CFMutableDictionaryRef assertion;
        if (!CFDictionaryGetValueIfPresent(gActiveAssertionsDict, (uintptr_t)idx, (const void **)&assertion)) {
            continue;
        }
        if (!isA_CFDictionary(assertion)) {
            ERROR_LOG("Not a dictinary for 0x%x", idx);
            continue;
        }
        // first active assertion. Let's send this to powerd
        rc = kIOReturnSuccess;
        CFNumberRef assertion_id = CFDictionaryGetValue(assertion, kIOPMAsyncClientAssertionIdKey);
        CFNumberGetValue(assertion_id, kCFNumberSInt32Type, &a_id);
        DEBUG_LOG("Sending assertion create msg for id 0x%x:%@\n",a_id, assertion);
        remoteID = sendAsyncAssertionMsg(true, assertion, &rc, gReleasedAssertionsList);
        CFArrayRemoveAllValues(gReleasedAssertionsList);

        if (rc != kIOReturnSuccess || remoteID == kIOPMNullAssertionID) {
            ERROR_LOG("powerd returned err 0x%x to create assertion %@. Dropping the assertion\n",
                      rc, assertion);
            CFDictionaryRemoveValue(gActiveAssertionsDict, (uintptr_t)idx);
            CFDictionaryRemoveValue(gAssertionsDict, (uintptr_t)idx);
            continue;
        } else {
            // keep track of the remoteID received from powerd
            remoteIDCf = CFNumberCreate(0, kCFNumberIntType, &remoteID);
            if (!remoteIDCf) {
                ERROR_LOG("Failed to create the remoteID to CF for id %@\n", assertion);
                continue;
            }
            INFO_LOG("powerd returned assertion id 0x%x for async id %@\n",
                      remoteID, assertion);
            gCurrentAssertion = a_id;
            gCurrentRemoteAssertion = remoteID;
            CFDictionarySetValue(assertion, kIOPMAsyncRemoteAssertionIdKey , remoteIDCf);
            CFRelease(remoteIDCf);
            break;
        }
    }
exit:
    if (!forced) {
        nextOffload_ts = 0;
        dispatch_suspend(gAssertionsOffloader);
    }
}

void processCheckAssertionsMsg(xpc_object_t msg)
{
    uint32_t blockers = 0;
    uint64_t    token = 0;
    if (activeAssertions()) {
        blockers = 1;
        dispatch_async(getPMQueue(), ^{
            if (gCurrentRemoteAssertion) {
                ERROR_LOG("Received processCheckAssertionMsg with active assertion 0x%x", gCurrentRemoteAssertion);
            }
            offloadAssertions(true);
        });
    }

    xpc_object_t reply = xpc_dictionary_create_reply(msg);
    if (!reply) {
        ERROR_LOG("Failed to create xpc reply object\n");
        return;
    }

    // Take the token from incoming message and put it in reply
    token = xpc_dictionary_get_uint64(msg, kAssertionCheckTokenKey);
    xpc_dictionary_set_uint64(reply, kAssertionCheckTokenKey, token);
    xpc_dictionary_set_uint64(reply, kAssertionCheckCountKey, blockers);

    INFO_LOG("Replying to assertion check message with count %d token:%llu\n", blockers, token);
    xpc_connection_t remote = xpc_dictionary_get_remote_connection(msg);
    xpc_connection_send_message(remote, reply);
    xpc_release(reply);

}

void processAssertionTimeout(xpc_object_t msg)
{
    IOPMAssertionID id;
    int idx = -1;

    id = xpc_dictionary_get_uint64(msg, kAssertionTimeoutMsg);

    if (id == gCurrentRemoteAssertion) {
        INFO_LOG("Current assertion has been released by powerd gCurrentRemoteAssertion 0x%x gCurrentAssertion 0x%x", id, gCurrentAssertion);
        idx = ASYNC_IDX_FROM_ID(gCurrentAssertion);
        // remove from dict
        if (idx != -1) {
            // has this assertion already been released ?
            CFTypeRef assertion = CFDictionaryGetValue(gAssertionsDict, (uintptr_t)idx);
            if (!isA_CFDictionary(assertion)) {
                DEBUG_LOG("Assertion 0x%x has already been released", gCurrentAssertion);
                // send a release to powerd
                INFO_LOG("Assertion %u has already been released. Sending a release to powerd", gCurrentRemoteAssertion);
                sendAsyncReleaseMsg(gCurrentRemoteAssertion, false);
            } else {
                // remove from timed list
                removeFromTimedList(gCurrentAssertion);

                // remove from active list
                if (CFDictionaryContainsKey(gActiveAssertionsDict, (uintptr_t)idx)) {
                    DEBUG_LOG("processAssertionTimeout: Removing from gActiveAssertionsDict: 0x%x", id);
                    CFDictionaryRemoveValue(gActiveAssertionsDict, (uintptr_t)idx);
                } else {
                    DEBUG_LOG("processAssertionTimeout: Not found in gActiveAssertionsDict: 0x%x", id);
                }

                // check assertion timeout action
                CFStringRef action = kIOPMAssertionTimeoutActionTurnOff;
                if (CFDictionaryContainsValue(assertion, kIOPMAssertionTimeoutActionKey)) {
                    action = CFDictionaryGetValue(assertion, kIOPMAssertionTimeoutActionKey);
                }
                if (CFStringCompare(action, kIOPMAssertionTimeoutActionRelease, 0) == kCFCompareEqualTo) {
                    DEBUG_LOG("processAssertionTimeout: Removing idx:%d id:0x%x", idx, id);
                    CFDictionaryRemoveValue(gAssertionsDict, (uintptr_t)idx);

                } else {
                    DEBUG_LOG("processAssertionTimeout: Turning off assertion idx:%d id:0x%x", idx, id);
                    CFDictionarySetValue(gInactiveAssertionsDict, (uintptr_t)idx, assertion);
                }
            }
            gCurrentAssertion = 0;
            gCurrentRemoteAssertion = 0;
        }
    } else {
        ERROR_LOG("Received a release not for gCurrentRemoteAssertion:0x%x. Received 0x%x", gCurrentRemoteAssertion, id)
    }

    // offload any assertions
    dispatch_async(getPMQueue(), ^{
        offloadAssertions(true);
    });
}

void processAssertionUpdateActivity(xpc_object_t msg)
{
    INFO_LOG("Powerd has requested assertion activity update");
    // powerd has requested an update
    if (!gReleasedAssertionsList && !gActiveAssertionsDict) {
        // no assertions to update
        return;
    }
    if (!gAssertionConnection) {
        ERROR_LOG("processAssertionUpdateActivity: No gAssertionConnection");
        return;
    }

    xpc_object_t reply = xpc_dictionary_create_reply(msg);
    xpc_object_t data;
    if (!reply) {
        ERROR_LOG("Failed to create xpc reply object");
        return;
    }

    CFMutableArrayRef toLog = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    int count = 0;

    // gActiveAssertionDict
    for (int idx = 0; idx < kMaxAsyncAssertions; idx++) {
        if (count == kMaxAsyncAssertionsLog) {
            break;
        }
        CFMutableDictionaryRef assertion;
        if (!CFDictionaryGetValueIfPresent(gActiveAssertionsDict, (uintptr_t)idx, (const void **)&assertion)) {
            continue;
        }
        if (!isA_CFDictionary(assertion)) {
            ERROR_LOG("Not a dictiaonry for 0x%x", idx);
            continue;
        }
        if (!CFDictionaryContainsKey(assertion, kIOPMAsyncRemoteAssertionIdKey)) {
            CFArrayAppendValue(toLog, assertion);
            count++;
        } else {
            DEBUG_LOG("Skipping relogging of active assertion which has a remote id %@", assertion);
        }
    }
    if (count < kMaxAsyncAssertionsLog && isA_CFArray(gReleasedAssertionsList)) {
        // add gReleasedAssertionsList
        for (int i = 0; i < CFArrayGetCount(gReleasedAssertionsList); i++) {
            if (count == kMaxAsyncAssertionsLog) {
                break;
            }
            CFMutableDictionaryRef assertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(gReleasedAssertionsList, i);
            if (isA_CFDictionary(assertion)) {
                CFArrayAppendValue(toLog, assertion);
                count++;
            }
        }
    }
    DEBUG_LOG("processAssertionUpdateActivity count %ld and toLog %@", CFArrayGetCount(toLog), toLog);
    if (toLog && CFArrayGetCount(toLog) != 0) {
        data = _CFXPCCreateXPCObjectFromCFObject(toLog);
        xpc_dictionary_set_value(reply, kAssertionActivityLogKey, data);
        if (count == kMaxAsyncAssertionsLog) {
            // set overflow = true.
            xpc_dictionary_set_bool(reply, kAssertionCopyActivityUpdateOverflowKey, true);
        }
    } else {
        ERROR_LOG("No assertion activity to update");
        if (toLog) {
            CFRelease(toLog);
        }
        xpc_release(reply);
        return;
    }

    xpc_connection_send_message(gAssertionConnection, reply);
    if (data) {
        xpc_release(data);
    }
    if (reply) {
        xpc_release(reply);
    }

    // mark everything in toLog as LoggedCreate
    uint32_t value = 1;
    CFNumberRef cf_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);

    for (int i = 0; i < CFArrayGetCount(toLog); i++) {
        CFMutableDictionaryRef assertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(toLog, i);
        CFDictionarySetValue(assertion, kIOPMAsyncAssertionLoggedCreate, cf_value);
    }

    if (cf_value) {
        CFRelease(cf_value);
    }
    // remove gReleasedAssertionsList
    CFArrayRemoveAllValues(gReleasedAssertionsList);
    if (toLog) {
        CFRelease(toLog);
    }
}

void processCurrentActiveAssertions(xpc_object_t msg)
{
    /*
     powerd has requested all active async assertions
     */
    if (!gAssertionConnection) {
        ERROR_LOG("processAssertionUpdateActivity: No gAssertionConnection");
        return;
    }
    if (gActiveAssertionsDict && CFDictionaryGetCount(gActiveAssertionsDict) > 0) {
        xpc_object_t reply = xpc_dictionary_create_reply(msg);
        if (!reply) {
            ERROR_LOG("Failed to create xpc reply object");
            return;
        }
        CFMutableArrayRef toLog = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        for (int idx = 0; idx < kMaxAsyncAssertions; idx++) {
            CFMutableDictionaryRef assertion;
            if (!CFDictionaryGetValueIfPresent(gActiveAssertionsDict, (uintptr_t)idx, (const void **)&assertion)) {
                continue;
            }
            if (!isA_CFDictionary(assertion)) {
                ERROR_LOG("Not a dictianary for 0x%x", idx);
                continue;
            }
            CFArrayAppendValue(toLog, assertion);
        }
        xpc_object_t data = _CFXPCCreateXPCObjectFromCFObject(toLog);
        xpc_dictionary_set_value(reply, kAssertionUpdateActivesMsg, data);
        xpc_connection_send_message(gAssertionConnection, reply);
        if (data) {
            xpc_release(data);
        }
        if (reply) {
            xpc_release(reply);
        }
        if (toLog) {
            CFRelease(toLog);
        }
    }
}

void processRemoteMsg(xpc_object_t msg)
{
    xpc_type_t type = xpc_get_type(msg);

    if (type == XPC_TYPE_DICTIONARY) {
        if ((xpc_dictionary_get_value(msg, kAssertionCheckMsg))) {
            processCheckAssertionsMsg(msg);
        }
        else if ((xpc_dictionary_get_value(msg, kAssertionTimeoutMsg))) {
            processAssertionTimeout(msg);
        }
        else if ((xpc_dictionary_get_value(msg, kAssertionUpdateActivityMsg))) {
            processAssertionUpdateActivity(msg);
        }
        else if ((xpc_dictionary_get_value(msg, kAssertionUpdateActivesMsg))) {
            processCurrentActiveAssertions(msg);
        }
        else {
            ERROR_LOG("Unexpected message from async assertions connections\n");
        }
    }
    else if (type == XPC_TYPE_ERROR) {
        if (msg == XPC_ERROR_CONNECTION_INTERRUPTED) {
            // powerd must have crashed
            ERROR_LOG("powerd released our connection");
            if (gAssertionConnection) {
                xpc_release(gAssertionConnection);
                gAssertionConnection = NULL;
                gAsyncModeSetupDone = false;
                gCurrentRemoteAssertion = 0;
                gCurrentAssertion = 0;
            }
            // resend assertions
            offloadAssertions(true);
        }
        else if (msg == XPC_ERROR_CONNECTION_INVALID) {
            // powerd released our connection
            ERROR_LOG("powerd released our connection");
            if (gAssertionConnection) {
                xpc_release(gAssertionConnection);
                gAssertionConnection = NULL;
                gAsyncModeSetupDone = false;
            }
        }
        else {
            ERROR_LOG("Irrecoverable error for assertion creation\n");
            IOPMDisableAsyncAssertions();
        }
    }
}


// Assertion timeouts
void handleAssertionTimeout(void)
{
    // look through list and release assertion
    uint64_t now = getMonotonicTime();
    uint64_t timeout_ts = 0;
    int i = 0;
    for (i = 0; i < CFArrayGetCount(gTimedAssertionsList); i++) {
        CFMutableDictionaryRef assertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(gTimedAssertionsList, i);
        CFNumberRef cf_timeout = (CFNumberRef)CFDictionaryGetValue(assertion, kIOPMAsyncAssertionTimeoutTimestamp);
        CFNumberGetValue(cf_timeout, kCFNumberSInt64Type, &timeout_ts);
        CFNumberRef a_id = CFDictionaryGetValue(assertion, kIOPMAsyncClientAssertionIdKey);
        uint32_t id = 0;
        CFNumberGetValue(a_id, kCFNumberSInt32Type, &id);
        if (timeout_ts <= now) {
            if (CFDictionaryContainsKey(assertion, kIOPMAsyncRemoteAssertionIdKey)) {
                CFNumberRef cf_id = CFDictionaryGetValue(assertion, kIOPMAsyncRemoteAssertionIdKey);
                uint32_t value = 0;
                CFNumberGetValue(cf_id, kCFNumberSInt32Type, &value);
                ERROR_LOG("Powerd knows about this assertion 0x%0x. let powerd time it out", value);
                continue;
            }
            DEBUG_LOG("Timeout: assertion id 0x%x with time %llu", id, timeout_ts);
            /*
             Check kIOPMAssertionTimeoutActionKey
             */
            CFStringRef action = kIOPMAssertionTimeoutActionTurnOff;
            if (CFDictionaryContainsValue(assertion, kIOPMAssertionTimeoutActionKey)) {
                action = CFDictionaryGetValue(assertion, kIOPMAssertionTimeoutActionKey);
            }
            if (CFStringCompare(action, kIOPMAssertionTimeoutActionRelease, 0) == kCFCompareEqualTo) {
                IOReturn rc = _releaseAsycnAssertion(id, false);
                if (rc != kIOReturnSuccess) {
                    ERROR_LOG("Failed to release assertion 0x%x on timeout", id);
                }
            } else {
                DEBUG_LOG("Turning off assertion 0x%x: %@", id, assertion);
                // turn off assertion
                uint32_t level = kIOPMAssertionLevelOff;
                CFNumberRef cf_level = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &level);
                handleAssertionLevel(cf_level, id);
                CFRelease(cf_level);
            }

        } else {
            break;
        }
    }
    for (int j = 0; j < i; j++) {
        CFArrayRemoveValueAtIndex(gTimedAssertionsList, 0);
    }

    dispatch_suspend(gAssertionTimer);
    // find the next timeout and arm timer
    if (CFArrayGetCount(gTimedAssertionsList) != 0) {
        CFMutableDictionaryRef earliest_assertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(gTimedAssertionsList, 0);
        CFNumberRef nextTimeout = (CFNumberRef)CFDictionaryGetValue(earliest_assertion, kIOPMAsyncAssertionTimeoutTimestamp);
        CFNumberGetValue(nextTimeout, kCFNumberSInt64Type, &timeout_ts);
        uint64_t delta = timeout_ts - now;
        dispatch_source_set_timer(gAssertionTimer, dispatch_time(DISPATCH_TIME_NOW, delta * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
        dispatch_resume(gAssertionTimer);
        DEBUG_LOG("handleAssertionTimeout: Setting assertion timeout to fire in %llu secs", delta);
    }
}

static CFComparisonResult compare_assertion(const void *val1, const void *val2, __unused void *context)
{
    CFDictionaryRef props1 = (CFDictionaryRef)val1;
    CFDictionaryRef props2 = (CFDictionaryRef)val2;
    CFNumberRef t1 = (CFNumberRef)CFDictionaryGetValue(props1, kIOPMAsyncAssertionTimeoutTimestamp);
    CFNumberRef t2 = (CFNumberRef)CFDictionaryGetValue(props2, kIOPMAsyncAssertionTimeoutTimestamp);
    return CFNumberCompare(t1, t2, NULL);
}

void insertIntoTimedList(CFMutableDictionaryRef props)
{
    CFArrayAppendValue(gTimedAssertionsList, props);
    CFArraySortValues(gTimedAssertionsList, CFRangeMake(0, CFArrayGetCount(gTimedAssertionsList)), compare_assertion, NULL);

    // arm a timer
    if (!gAssertionTimer) {
        gAssertionTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, getPMQueue());
        if (gAssertionTimer) {
            dispatch_source_set_event_handler(gAssertionTimer, ^{
                handleAssertionTimeout();
            });
            dispatch_source_set_cancel_handler(gAssertionTimer, ^{
                dispatch_release(gAssertionTimer);
                gAssertionTimer = NULL;
            });
        }
    } else {
        // suspend the current timer and arm the new one
        dispatch_suspend(gAssertionTimer);
    }
    uint64_t now = getMonotonicTime();
    uint64_t earliest_timeout = 0;
    CFMutableDictionaryRef earliestAssertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(gTimedAssertionsList, 0);
    CFNumberRef cf_earliest_timeout = CFDictionaryGetValue(earliestAssertion, kIOPMAsyncAssertionTimeoutTimestamp);
    CFNumberGetValue(cf_earliest_timeout, kCFNumberSInt64Type, &earliest_timeout);
    int  delta = earliest_timeout - now;
    if (delta <= 0) {
        handleAssertionTimeout();
    } else {
        if (gAssertionTimer) {
            DEBUG_LOG("Setting assertion timeout to fire in %d secs", delta);
            dispatch_source_set_timer(gAssertionTimer, dispatch_time(DISPATCH_TIME_NOW, delta * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
            dispatch_resume(gAssertionTimer);
        }
    }
}

void removeFromTimedList(IOPMAssertionID id)
{
    int idx = 0;
    int found_id = -1;
    for (idx = 0; idx < CFArrayGetCount(gTimedAssertionsList); idx++) {
        CFMutableDictionaryRef assertion = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(gTimedAssertionsList, idx);
        CFNumberRef cf_id;
        cf_id = (CFNumberRef)CFDictionaryGetValue(assertion, kIOPMAsyncClientAssertionIdKey);
        uint32_t a_id;
        CFNumberGetValue(cf_id, kCFNumberSInt32Type, &a_id);
        if (a_id == id) {
            found_id = idx;
            break;
        }
    }
    if (found_id != -1) {
        DEBUG_LOG("Removing 0x%x from TimedList", (uint32_t)id);
        CFArrayRemoveValueAtIndex(gTimedAssertionsList, found_id);
    }
}

IOReturn handleAssertionLevel(CFTypeRef value, IOPMAssertionID assertionID)
{
    uint32_t level;
    CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &level);

    uint32_t idx = ASYNC_IDX_FROM_ID(assertionID);
    CFMutableDictionaryRef assertion =  (CFMutableDictionaryRef)CFDictionaryGetValue(gAssertionsDict, (uintptr_t)idx);
    if (!assertion) {
        ERROR_LOG("handleAssertionLevel: unable to find assertion with id 0x%x", assertionID);
        return kIOReturnBadArgument;
    }
    bool is_active = CFDictionaryContainsKey(gActiveAssertionsDict, (uintptr_t)idx);
    bool is_turned_off = CFDictionaryContainsKey(gInactiveAssertionsDict, (uintptr_t)idx);
    if (!is_active && !is_turned_off) {
        // this should never happen
        ERROR_LOG("Assertion 0x%x is neither active nor turned off", assertionID);
        return kIOReturnError;
    }

    if (level == kIOPMAssertionLevelOn) {
        if (is_turned_off) {
            // turn on assertion from turned off list
            // update create time
            CFDateRef start_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
            if (start_date) {
                CFDictionarySetValue(assertion, kIOPMAssertionCreateDateKey, start_date);
                CFRelease(start_date);
            }

            // clear previous release date
            if (CFDictionaryContainsKey(assertion, kIOPMAssertionReleaseDateKey)) {
                CFDictionaryRemoveValue(assertion, kIOPMAssertionReleaseDateKey);
            }
            CFDictionaryRemoveValue(gInactiveAssertionsDict, (uintptr_t)idx);

            // activate the assertion
            activateAsyncAssertion(assertionID);
        }
    } else if (level == kIOPMAssertionLevelOff) {
        if (is_active) {
            // turn off active
            CFDateRef turn_off_date = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
            if (turn_off_date) {
                CFDictionarySetValue(assertion, kIOPMAssertionReleaseDateKey, turn_off_date);
                CFRelease(turn_off_date);
            }

            // make an entry in gReleasedAssertionsList
            CFArrayAppendValue(gReleasedAssertionsList, (const void *)assertion);

            // move to turned off list
            CFDictionarySetValue(gInactiveAssertionsDict, (uintptr_t)idx, assertion);
            CFDictionaryRemoveValue(gActiveAssertionsDict, (uintptr_t)idx);

            // check if this was the last assertion turned off
            if (!activeAssertions() && gCurrentAssertion) {
                DEBUG_LOG("handleAssertionTurnOff: Released assertion id 0x%x", gCurrentRemoteAssertion);
                sendAsyncReleaseMsg(gCurrentRemoteAssertion, true);
                gCurrentAssertion = 0;
                gCurrentRemoteAssertion = 0;

            }
        }
    }
    DEBUG_LOG("handleAssertionLevel for 0x%x to %d: %@", assertionID, level, assertion);
    return kIOReturnSuccess;
}

bool activeAssertions()
{
    int length = (int)CFDictionaryGetCount(gActiveAssertionsDict);
    if (length > 0) {
        return true;
    }
    return false;
}

CFDictionaryRef IOPMGetCurrentAsyncActiveAssertions(void)
{
    if (gAssertionsDict && CFDictionaryGetCount(gActiveAssertionsDict) > 0) {
        return gActiveAssertionsDict;
    }
    return NULL;
}

CFArrayRef IOPMGetCurrentAsyncReleasedAssertions(void)
{
    if (gReleasedAssertionsList && CFArrayGetCount(gReleasedAssertionsList) > 0) {
        return gReleasedAssertionsList;
    }
    return NULL;
}

CFDictionaryRef IOPMGetCurrentAsyncInactiveAssertions(void)
{
    if (gInactiveAssertionsDict && CFDictionaryGetCount(gInactiveAssertionsDict) > 0) {
        return gInactiveAssertionsDict;
    }
    return NULL;
}

CFArrayRef IOPMGetCurrentAsyncTimedAssertions(void)
{
    if (gTimedAssertionsList && CFArrayGetCount(gTimedAssertionsList) > 0) {
        return gTimedAssertionsList;
    }
    return NULL;
}

CFDictionaryRef IOPMGetCurrentAsycnRemoteAssertion(void)
{
    if (gCurrentAssertion && gCurrentAssertion != kIOPMNullAssertionID) {
        int idx = ASYNC_IDX_FROM_ID(gCurrentAssertion);
        CFDictionaryRef props = CFDictionaryGetValue(gAssertionsDict, idx);
        if (props) {
            return props;
        }
    }
    return NULL;
}

/*
 IOPMCopyActiveAsyncAssertionsByProcess. Used by pmset
 Send a message to powerd to fetch all active assertions from
 all processes. This is a blocking call. Intended for use only by pmset
 */
CFDictionaryRef IOPMCopyActiveAsyncAssertionsByProcess()
{
#if !TARGET_OS_IOS
    return NULL;
#endif
    xpc_object_t connection = NULL;
    xpc_object_t msg = NULL;
    connection = xpc_connection_create_mach_service(POWERD_XPC_ID, dispatch_get_main_queue(), 0);
    if (!connection) {
        ERROR_LOG("Failed to create connection\n");
        return NULL;
    }
    xpc_connection_set_target_queue(connection, getPMQueue());
    xpc_connection_set_event_handler(connection,
                                     ^(xpc_object_t e __unused ) { });
    xpc_connection_resume(connection);
    msg = xpc_dictionary_create(NULL, NULL, 0);
    if (!msg) {
        ERROR_LOG("Failed to create dictionary to send message");
        return NULL;
    }
    xpc_dictionary_set_bool(msg, kAssertionCopyActiveAsyncAssertionsKey, true);
    CFDictionaryRef assertions = NULL;
    xpc_object_t reply = xpc_connection_send_message_with_reply_sync(connection, msg);
    if (xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
        xpc_object_t data = xpc_dictionary_get_value(reply, kAssertionCopyActiveAsyncAssertionsKey);
        if (data) {
            assertions = _CFXPCCreateCFObjectFromXPCObject(data);
            INFO_LOG("Received active assertions from powerd %@", assertions);
        } else {
            ERROR_LOG("No assertions by process");
        }
    } else {
        ERROR_LOG("Received an error in response to IOPMCopyActiveAsyncAssertionsByProcess");
    }
    if (msg) {
        xpc_release(msg);
    }
    if (reply) {
        xpc_release(reply);
    }
    return assertions;
}

/********************************************************************
 End Async PowerAssertions
 ***********************************************************************/

/******************************************************************************
 * IOPMAssertionCreate
 *
 * Deprecated but still supported wrapper for IOPMAssertionCreateWithProperties
 ******************************************************************************/
IOReturn IOPMAssertionCreate(
                             CFStringRef             AssertionType,
                             IOPMAssertionLevel      AssertionLevel,
                             IOPMAssertionID         *AssertionID)
{
    return IOPMAssertionCreateWithName(AssertionType, AssertionLevel,
                                       CFSTR("Nameless (via IOPMAssertionCreate)"), AssertionID);
}

/******************************************************************************
 * IOPMAssertionCreateWithName
 *
 * Deprecated but still supported wrapper for IOPMAssertionCreateWithProperties
 ******************************************************************************/
IOReturn IOPMAssertionCreateWithName(
                                     CFStringRef          AssertionType, 
                                     IOPMAssertionLevel   AssertionLevel,
                                     CFStringRef          AssertionName,
                                     IOPMAssertionID      *AssertionID)
{
    CFMutableDictionaryRef      properties = NULL;
    IOReturn                    result = kIOReturnError;
    CFNumberRef                 numRef;

    if (!AssertionName || !AssertionID || !AssertionType)
        return kIOReturnBadArgument;

    numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &AssertionLevel);
    if (!isA_CFNumber(numRef)) {
        return result;
    }
    properties = CFDictionaryCreateMutable(0, 3, &kCFTypeDictionaryKeyCallBacks, 
                                           &kCFTypeDictionaryValueCallBacks);

    if (properties)
    {

        CFDictionarySetValue(properties, kIOPMAssertionTypeKey, AssertionType);

        CFDictionarySetValue(properties, kIOPMAssertionNameKey, AssertionName);

        CFDictionarySetValue(properties, kIOPMAssertionLevelKey, numRef);
        CFRelease(numRef);

        result = IOPMAssertionCreateWithProperties(properties, AssertionID);

        CFRelease(properties);
    }    

    return result;
}

/******************************************************************************
 * IOPMAssertionCreateWithDescription
 *
 ******************************************************************************/

static CFMutableDictionaryRef createAssertionDescription(
                                       CFStringRef  AssertionType,
                                       CFStringRef  Name,
                                       CFStringRef  Details,
                                       CFStringRef  HumanReadableReason,
                                       CFStringRef  LocalizationBundlePath,
                                       CFTimeInterval   Timeout,
                                       CFStringRef  TimeoutAction)
{
    CFMutableDictionaryRef  descriptor = NULL;

    descriptor = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!descriptor) {
        goto exit;
    }

    CFDictionarySetValue(descriptor, kIOPMAssertionNameKey, Name);

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
    if (TimeoutAction) {
        CFDictionarySetValue(descriptor, kIOPMAssertionTimeoutActionKey, TimeoutAction);
    }

exit:
    return descriptor;

}

IOReturn    IOPMAssertionCreateWithDescription(
                                               CFStringRef  AssertionType,
                                               CFStringRef  Name,
                                               CFStringRef  Details,
                                               CFStringRef  HumanReadableReason,
                                               CFStringRef  LocalizationBundlePath,
                                               CFTimeInterval   Timeout,
                                               CFStringRef  TimeoutAction,
                                               IOPMAssertionID  *AssertionID)
{
    CFMutableDictionaryRef  descriptor = NULL;
    IOReturn ret = kIOReturnError;

    if (!AssertionType || !Name || !AssertionID) {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    descriptor = createAssertionDescription(AssertionType, Name, Details, HumanReadableReason,
                                            LocalizationBundlePath, Timeout, TimeoutAction);
    if (!descriptor) {
        goto exit;
    }

    ret = IOPMAssertionCreateWithProperties(descriptor, AssertionID);

    CFRelease(descriptor);

exit:
    return ret;
}


IOReturn    IOPMAssertionCreateWithAutoTimeout(
                                               CFStringRef  AssertionType,
                                               CFStringRef  Name,
                                               CFStringRef  Details,
                                               CFStringRef  HumanReadableReason,
                                               CFStringRef  LocalizationBundlePath,
                                               CFTimeInterval   Timeout,
                                               CFStringRef  TimeoutAction,
                                               IOPMAssertionID  *AssertionID)
{
    CFMutableDictionaryRef  descriptor = NULL;
    IOReturn ret = kIOReturnError;

    if (!AssertionType || !Name || !AssertionID) {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    descriptor = createAssertionDescription(AssertionType, Name, Details, HumanReadableReason,
                                            LocalizationBundlePath, Timeout, TimeoutAction);
    if (!descriptor) {
        goto exit;
    }

    CFDictionarySetValue(descriptor, kIOPMAssertionAutoTimesout, kCFBooleanTrue);

    ret = IOPMAssertionCreateWithProperties(descriptor, AssertionID);

    CFRelease(descriptor);

exit:
    return ret;
}

IOReturn    IOPMAssertionCreateWithResourceList(
                                               CFStringRef  AssertionType,
                                               CFStringRef  Name,
                                               CFStringRef  Details,
                                               CFStringRef  HumanReadableReason,
                                               CFStringRef  LocalizationBundlePath,
                                               CFTimeInterval   Timeout,
                                               CFStringRef  TimeoutAction,
                                               CFArrayRef   resources,
                                               IOPMAssertionID  *AssertionID)
{
    CFMutableDictionaryRef  descriptor = NULL;
    IOReturn ret = kIOReturnError;

    if (!AssertionType || !Name || !AssertionID || !isA_CFArray(resources)) {
        ret = kIOReturnBadArgument;
        goto exit;
    }

    descriptor = createAssertionDescription(AssertionType, Name, Details, HumanReadableReason,
                                            LocalizationBundlePath, Timeout, TimeoutAction);
    if (!descriptor) {
        goto exit;
    }

    CFDictionarySetValue(descriptor, kIOPMAssertionResourcesUsed, resources);
    ret = IOPMAssertionCreateWithProperties(descriptor, AssertionID);

    CFRelease(descriptor);

exit:
    return ret;
}

/******************************************************************************
 * IOPMAssertionCreateWithProperties
 *
 ******************************************************************************/
IOReturn IOPMAssertionCreateWithProperties(
                                           CFDictionaryRef         AssertionProperties,
                                           IOPMAssertionID         *AssertionID)
{
    IOReturn                return_code     = kIOReturnError;
    kern_return_t           kern_result     = KERN_SUCCESS;
    mach_port_t             pm_server       = MACH_PORT_NULL;
    IOReturn                err;
    CFDataRef               flattenedProps  = NULL;
    CFStringRef             assertionTypeString = NULL;
    CFStringRef             name = NULL;
    CFMutableDictionaryRef  mutableProps = NULL;
    int                     disableAppSleep = 0;
    int                     enTrIntensity = -1;
    bool                    assertionEnabled = true;
    bool                    asyncMode = false;
#if TARGET_OS_IPHONE
    static int              resyncToken = 0;
    static CFMutableDictionaryRef  resyncCopy = NULL;
#endif

    if (!AssertionProperties || !AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    assertionTypeString = CFDictionaryGetValue(AssertionProperties, kIOPMAssertionTypeKey);
    name = CFDictionaryGetValue(AssertionProperties, kIOPMAssertionNameKey);

    if (!isA_CFString(assertionTypeString) || !isA_CFString(name)) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }



#if !defined(_OPEN_SOURCE_) && !TARGET_OS_IPHONE

    if ( isA_CFString(assertionTypeString) && 
            ( CFEqual(assertionTypeString, kIOPMAssertionTypePreventUserIdleDisplaySleep) ||
              CFEqual(assertionTypeString, kIOPMAssertionTypeNoDisplaySleep)) ) {

        if (!_CSCheckFix(CFSTR("11136371") )) {

            if (!mutableProps) {
                mutableProps = CFDictionaryCreateMutableCopy(NULL, 0, AssertionProperties);
                if (!mutableProps) {
                    return_code = kIOReturnInternalError;
                    goto exit;
                }
            }


            CFDictionarySetValue(mutableProps, kIOPMAssertionTypeKey, kIOPMAssertionUserIsActive);
        }
    }

#endif  // !_OPEN_SOURCE_ && !TARGET_OS_IPHONE

#if TARGET_OS_IPHONE

    if (isA_CFString(assertionTypeString) && 
        CFEqual(assertionTypeString, kIOPMAssertionTypeEnableIdleSleep) && !resyncToken) {

        resyncCopy = CFDictionaryCreateMutableCopy(NULL, 0, AssertionProperties);
        notify_register_dispatch( kIOUserAssertionReSync, 
                                  &resyncToken, dispatch_get_main_queue(),
                                  ^(int t __unused) {
                                  IOPMAssertionID id;
                                  IOPMAssertionCreateWithProperties(resyncCopy, &id);
                                  });
    }
#endif    

    CFNumberRef numRef = CFDictionaryGetValue(AssertionProperties, kIOPMAssertionLevelKey);
    if (isA_CFNumber(numRef)) {
        int level = 0;
        CFNumberGetValue(numRef, kCFNumberIntType, &level);
        if (level == kIOPMAssertionLevelOff) {
            assertionEnabled = false;
        }
    }

    if (collectBackTrace && assertionEnabled) {
        if (!mutableProps) {
            mutableProps = CFDictionaryCreateMutableCopy(NULL, 0, AssertionProperties);
            if (!mutableProps) {
                return_code = kIOReturnInternalError;
                goto exit;
            }
        }
        saveBackTrace(mutableProps);
    }

    flattenedProps = CFPropertyListCreateData(0, (mutableProps != NULL) ? mutableProps : AssertionProperties, 
                                              kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    if (!flattenedProps) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }


    asyncMode = createAsyncAssertion(
            AssertionProperties, (IOPMAssertionID *)AssertionID);
    if (!asyncMode) {
        kern_result = io_pm_assertion_create( pm_server,
                                              (vm_offset_t)CFDataGetBytePtr(flattenedProps),
                                              CFDataGetLength(flattenedProps),
                                              (int *)AssertionID,
                                              &disableAppSleep,
                                              &enTrIntensity,
                                              &return_code);


        if(KERN_SUCCESS != kern_result) {
            return_code = kIOReturnInternalError;
            goto exit;
        }
    }
    else {
            return_code = kIOReturnSuccess;
#if !TARGET_OS_SIMULATOR
            enTrIntensity = kEnTrQualSPKeepSystemAwake;
#endif
    }

#if !TARGET_OS_IPHONE
    if (disableAppSleep) {
        CFStringRef assertionName = NULL;
        CFStringRef appSleepString = NULL;

        assertionName = CFDictionaryGetValue(AssertionProperties, kIOPMAssertionNameKey);
        appSleepString = CFStringCreateWithFormat(NULL, NULL, 
                                                  CFSTR("App is holding power assertion %u with name \'%@\' "),
                                                  *AssertionID, assertionName);

        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsTakeAssertion,  appSleepString);

        CFRelease(appSleepString);
    }
#endif


#if !TARGET_OS_SIMULATOR
        if ((enTrIntensity != kEnTrQualNone) && assertionEnabled) {
            entr_act_begin(kEnTrCompSysPower, kEnTrActSPPMAssertion, *AssertionID,
                                    enTrIntensity, kEnTrValNone);
        }
#endif

exit:
    if (flattenedProps) {
        CFRelease(flattenedProps);
    }

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }

    // Release mutableProps if allocated in this function
    if (mutableProps)
        CFRelease(mutableProps);

    return return_code;
}

/******************************************************************************
 * IOPMPerformBlockWithAssertion
 *
 ******************************************************************************/
IOReturn IOPMPerformBlockWithAssertion(
                                       CFDictionaryRef assertion_properties,
                                       dispatch_block_t the_block)
{
    IOPMAssertionID _id = kIOPMNullAssertionID;
    IOReturn rc;
    
    if (!assertion_properties || !the_block) {
        return kIOReturnBadArgument;
    }
    
    rc = IOPMAssertionCreateWithProperties(assertion_properties, &_id);
    if (rc != kIOReturnSuccess) {
        return rc;
    }
    
    the_block();
    
    if (kIOPMNullAssertionID != _id) {
        IOPMAssertionRelease(_id);
    }
    
    return kIOReturnSuccess;
}

/******************************************************************************
 * IOPMAssertionsRetain
 *
 ******************************************************************************/
void IOPMAssertionRetain(IOPMAssertionID theAssertion)
{
    IOReturn                return_code     = kIOReturnError;
    kern_return_t           kern_result     = KERN_SUCCESS;
    mach_port_t             pm_server       = MACH_PORT_NULL;
    IOReturn                err;
    int                     disableAppSleep = 0;
    int                     enableAppSleep = 0;
    int                     retainCnt = 0;

    if (!theAssertion) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    if (gAssertionsDict && IS_ASYNC_ID(theAssertion)) {
        ERROR_LOG("Assertion retain not supported in async mode\n");
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_assertion_retain_release( pm_server, 
                                                  (int)theAssertion,
                                                  kIOPMAssertionMIGDoRetain,
                                                  &retainCnt,
                                                  &disableAppSleep,
                                                  &enableAppSleep,
                                                  &return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    } else {
#if !TARGET_OS_IPHONE
        if (disableAppSleep) {
            CFStringRef appSleepString = NULL;

            appSleepString = CFStringCreateWithFormat(NULL, NULL, CFSTR("App is holding power assertion %u"),
                                                      theAssertion);
            __CFRunLoopSetOptionsReason(__CFRunLoopOptionsTakeAssertion,  appSleepString);

            CFRelease(appSleepString);
        }
#endif
#if !TARGET_OS_SIMULATOR
        entr_act_modify(kEnTrCompSysPower, kEnTrActSPPMAssertion,
                                (int)theAssertion, kEnTrModSPRetain, kEnTrValNone); 
#endif
    }


exit:
    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }
    return;
}


/******************************************************************************
 * IOPMAssertionsRelease
 *
 ******************************************************************************/
IOReturn IOPMAssertionRelease(IOPMAssertionID AssertionID)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    mach_port_t             pm_server = MACH_PORT_NULL;
    IOReturn                err;
    int                     disableAppSleep = 0;
    int                     enableAppSleep = 0;
    int                     retainCnt = 0;
    bool                    asyncMode = false;

    if (!AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    if (gAssertionsDict && IS_ASYNC_ID(AssertionID)) {
        asyncMode = true;
        return_code = releaseAsyncAssertion(AssertionID);
        goto exit;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    if (!asyncMode) {
            kern_result = io_pm_assertion_retain_release( pm_server,
                                                          (int)AssertionID,
                                                          kIOPMAssertionMIGDoRelease,
                                                          &retainCnt,
                                                          &disableAppSleep,
                                                          &enableAppSleep,
                                                          &return_code);

        if(KERN_SUCCESS != kern_result) {
            return_code = kIOReturnInternalError;
        }
    }
    else {
        return_code = kIOReturnSuccess;
    }
#if !TARGET_OS_IPHONE
    if (enableAppSleep) {
        CFStringRef appSleepString = NULL;

        appSleepString = CFStringCreateWithFormat(NULL, NULL, CFSTR("App released its last power assertion %u"),
                                                  AssertionID);
        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsDropAssertion, appSleepString);

        CFRelease(appSleepString);
    }
#endif

#if !TARGET_OS_SIMULATOR
    if (retainCnt)
        entr_act_modify(kEnTrCompSysPower, kEnTrActSPPMAssertion,
                                (int)AssertionID, kEnTrModSPRelease, kEnTrValNone);
    else
        entr_act_end(kEnTrCompSysPower, kEnTrActSPPMAssertion,
                                (int)AssertionID, kEnTrQualNone, kEnTrValNone);
#endif

    pm_connect_close(pm_server);
exit:
    return return_code;
}


/******************************************************************************
 * IOPMAssertionSetProperty
 *
 ******************************************************************************/
IOReturn IOPMAssertionSetProperty(IOPMAssertionID theAssertion, CFStringRef theProperty, CFTypeRef theValue)
{
    IOReturn                return_code     = kIOReturnError;
    kern_return_t           kern_result     = KERN_SUCCESS;
    mach_port_t             pm_server       = MACH_PORT_NULL;
    CFDataRef               sendData        = NULL;
    CFMutableDictionaryRef  sendDict        = NULL;
    int                     disableAppSleep = 0;
    int                     enableAppSleep = 0;
    int                     enTrIntensity = -1;
    bool                    assertionEnabled = false;
    bool                    assertionDisabled = false;
    bool                    asyncMode = false;

    if (!theAssertion) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    if (gAssertionsDict && IS_ASYNC_ID(theAssertion)) {
        asyncMode = true;
        return_code = setAsyncAssertionProperties(theProperty, theValue, theAssertion);
        goto exit;
    }

    return_code = pm_connect_init(&pm_server);

    if(kIOReturnSuccess != return_code) {
        goto exit;
    }

    sendDict = CFDictionaryCreateMutable(0, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (!sendDict) {
        return_code = kIOReturnNoMemory;
        goto exit;
    }

    CFDictionarySetValue(sendDict, theProperty, theValue);
    if ((CFStringCompare(theProperty, kIOPMAssertionLevelKey, 0) == kCFCompareEqualTo) && (isA_CFNumber(theValue))) {
        int level = 0;
        CFNumberGetValue(theValue, kCFNumberIntType, &level);
        if (level == kIOPMAssertionLevelOff) {
            assertionDisabled = true;
        }
        else if (level == kIOPMAssertionLevelOn) {
            assertionEnabled = true;
        }
    }

    if (collectBackTrace && assertionEnabled) {
        saveBackTrace(sendDict);
    }

    sendData = CFPropertyListCreateData(0, sendDict, kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    CFRelease(sendDict);



    if (!asyncMode) {
        kern_result = io_pm_assertion_set_properties(pm_server,
                                                     (int)theAssertion,
                                                     (vm_offset_t)CFDataGetBytePtr(sendData),
                                                     CFDataGetLength(sendData),
                                                     &disableAppSleep,
                                                     &enableAppSleep,
                                                     &enTrIntensity,
                                                     (int *)&return_code);

        if(KERN_SUCCESS != kern_result) {
            return_code = kIOReturnInternalError;
            goto exit;
        }
    }
    else {
        return_code = kIOReturnSuccess;
    }

#if !TARGET_OS_IPHONE
    if (disableAppSleep) {
        CFStringRef appSleepString = NULL;

        appSleepString = CFStringCreateWithFormat(NULL, NULL, CFSTR("App is holding power assertion %u"),
                                                  theAssertion);
        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsTakeAssertion,  appSleepString);

        CFRelease(appSleepString);
    }
    else if (enableAppSleep) {
        CFStringRef appSleepString = NULL;

        appSleepString = CFStringCreateWithFormat(NULL, NULL, CFSTR("App released its last power assertion %u"),
                                                  theAssertion);
        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsDropAssertion, appSleepString);

        CFRelease(appSleepString);
    }
#endif


#if !TARGET_OS_SIMULATOR
    if (enTrIntensity != kEnTrQualNone) {
        if (assertionEnabled) {
            entr_act_begin(kEnTrCompSysPower, kEnTrActSPPMAssertion, theAssertion,
                           enTrIntensity, kEnTrValNone);
        }
        else if (assertionDisabled) {
            entr_act_end(kEnTrCompSysPower, kEnTrActSPPMAssertion, theAssertion,
                         kEnTrQualNone, kEnTrValNone);
        }
    }
#endif


exit:
    if (sendData)
        CFRelease(sendData);

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }
    return return_code;    
}
/******************************************************************************
 * IOPMAssertionSetProcessState
 *
 ******************************************************************************/
IOReturn IOPMAssertionSetProcessState(pid_t pid, IOPMAssertionProcessStateType state)
{
    xpc_connection_t        connection = NULL;
    xpc_object_t            msg = NULL;

    connection = xpc_connection_create_mach_service(POWERD_XPC_ID,
                                                    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), 0);

    if (!connection) {
        return kIOReturnError;
    }

    xpc_connection_set_target_queue(connection,
                                    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));

    xpc_connection_set_event_handler(connection,
                                     ^(xpc_object_t e __unused) { });

    msg = xpc_dictionary_create(NULL, NULL, 0);
    if(!msg) {
        xpc_release(connection);
        os_log_error(OS_LOG_DEFAULT, "Failed to create xpc msg object\n");
        return kIOReturnError;
    }

    os_log_debug(OS_LOG_DEFAULT, "Setting Assertion State for PID %d to %d\n", pid, state);

    xpc_dictionary_set_uint64(msg, "pid", pid);
    xpc_dictionary_set_uint64(msg, kAssertionSetStateMsg, state);
    xpc_connection_resume(connection);
    xpc_connection_send_message(connection, msg);

    xpc_release(msg);
    xpc_release(connection);
    return kIOReturnSuccess;
}

/******************************************************************************
 * IOPMAssertionSetTimeout
 *
 ******************************************************************************/
IOReturn IOPMAssertionSetTimeout(IOPMAssertionID whichAssertion, 
                                 CFTimeInterval timeoutInterval)
{
    IOReturn            return_code = kIOReturnError;
    CFNumberRef         intervalNum = NULL;
    int                 timeoutSecs = (int)timeoutInterval;

    intervalNum = CFNumberCreate(0, kCFNumberIntType, &timeoutSecs);

    if (intervalNum) 
    {
        return_code = IOPMAssertionSetProperty(whichAssertion, kIOPMAssertionTimeoutKey, intervalNum);

        CFRelease(intervalNum);
    }

    return return_code;
}

/******************************************************************************
 * IOPMAssertionDeclareNotificationEvent
 *
 ******************************************************************************/
 IOReturn IOPMAssertionDeclareNotificationEvent(
                        CFStringRef          notificationName,
                        CFTimeInterval       secondsToDisplay,
                        IOPMAssertionID      *AssertionID)
{
    IOPMAssertionID     id = kIOPMNullAssertionID;
    IOReturn            ret = kIOReturnSuccess;
    io_registry_entry_t rootdomain = getPMRootDomainRef();
    CFBooleanRef        lidIsClosed = NULL;
    CFBooleanRef        desktopMode = NULL;

    if (rootdomain == MACH_PORT_NULL) 
        return kIOReturnInternalError;

    desktopMode = IORegistryEntryCreateCFProperty(rootdomain, 
                                                  CFSTR("DesktopMode"), kCFAllocatorDefault, 0);
    lidIsClosed = IORegistryEntryCreateCFProperty(rootdomain, 
                                                  CFSTR(kAppleClamshellStateKey), kCFAllocatorDefault, 0);

    if ((kCFBooleanTrue == lidIsClosed) && (kCFBooleanFalse == desktopMode)) {
        ret = kIOReturnNotReady;
        goto exit;
    }

    ret = IOPMAssertionCreateWithDescription(
                                             kIOPMAssertDisplayWake,
                                             notificationName, NULL, NULL, NULL,
                                             secondsToDisplay, kIOPMAssertionTimeoutActionRelease,
                                             &id);
    if (AssertionID)
        *AssertionID = id;

exit:
    if (lidIsClosed) CFRelease(lidIsClosed);
    if (desktopMode) CFRelease(desktopMode);

    return ret;
}

/******************************************************************************
 * IOPMAssertionDeclareSystemActivityWithProperties
 *
 ******************************************************************************/
IOReturn IOPMAssertionDeclareSystemActivityWithProperties(
    CFMutableDictionaryRef         assertionProperties,
    IOPMAssertionID                *AssertionID,
    IOPMSystemState                *SystemState)
{
    IOReturn        err;
    IOReturn        return_code = kIOReturnError;
    mach_port_t     pm_server   = MACH_PORT_NULL;
    kern_return_t   kern_result = KERN_SUCCESS;

    CFDataRef   flattenedProps  = NULL;
    CFStringRef assertionTypeString = NULL;
    CFStringRef name = NULL;

    if (!assertionProperties || !AssertionID || !SystemState) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    name = CFDictionaryGetValue(assertionProperties, kIOPMAssertionNameKey);
    if (!isA_CFString(name)) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    assertionTypeString = CFDictionaryGetValue(assertionProperties, kIOPMAssertionTypeKey);
    /* Caller is not allowed to specify the AssertionType */
    if (isA_CFString(assertionTypeString)) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    if (collectBackTrace) {
        saveBackTrace(assertionProperties);
    }

    flattenedProps = CFPropertyListCreateData(0, assertionProperties,
                                              kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    if (!flattenedProps) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    kern_result = io_pm_declare_system_active(
                                              pm_server,
                                              (int *)SystemState,
                                              (vm_offset_t)CFDataGetBytePtr(flattenedProps),
                                              CFDataGetLength(flattenedProps),
                                              (int *)AssertionID,
                                              &return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
exit:
    if (flattenedProps)
        CFRelease(flattenedProps);

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }

    return return_code;
}

/******************************************************************************
 * IOPMAssertionDeclareSystemActivity
 *
 ******************************************************************************/
IOReturn IOPMAssertionDeclareSystemActivity(
                                            CFStringRef             AssertionName,
                                            IOPMAssertionID         *AssertionID,
                                            IOPMSystemState         *SystemState)
{
    IOReturn                return_code = kIOReturnError;
    CFMutableDictionaryRef  properties  = NULL;

    if (!AssertionName || !AssertionID || !SystemState) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    properties = CFDictionaryCreateMutable(0, 1, &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(properties, kIOPMAssertionNameKey, AssertionName);

    return_code = IOPMAssertionDeclareSystemActivityWithProperties(properties,
                                                                   AssertionID,
                                                                   SystemState);

exit:
    if (properties)
        CFRelease(properties);

    return return_code;

}
/******************************************************************************
 * IOPMAssertionDeclareUserActivity
 *
 ******************************************************************************/
IOReturn IOPMAssertionDeclareUserActivity(
                                          CFStringRef          AssertionName,
                                          IOPMUserActiveType   userType,
                                          IOPMAssertionID      *AssertionID)
{

    IOReturn        return_code = kIOReturnError;
    mach_port_t     pm_server = MACH_PORT_NULL;
    kern_return_t   kern_result = KERN_SUCCESS;
    IOReturn        err;
    int             disableAppSleep = 0;


    CFMutableDictionaryRef  properties = NULL;
    CFDataRef               flattenedProps  = NULL;

    if (!AssertionName || !AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    
    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    properties = CFDictionaryCreateMutable(0, 1, &kCFTypeDictionaryKeyCallBacks, 
                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(properties, kIOPMAssertionNameKey, AssertionName);

    if (collectBackTrace) {
        saveBackTrace(properties);
    }

    flattenedProps = CFPropertyListCreateData(0, properties, 
                                              kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    if (!flattenedProps) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    kern_result = io_pm_declare_user_active( 
                                            pm_server, 
                                            userType,
                                            (vm_offset_t)CFDataGetBytePtr(flattenedProps),
                                            CFDataGetLength(flattenedProps),
                                            (int *)AssertionID,
                                            &disableAppSleep,
                                            &return_code);


    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
#if !TARGET_OS_IPHONE
    else if (disableAppSleep) {
        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsTakeAssertion,
                                    CFSTR("App is holding 'DeclareUserActivity' power assertion"));
    }
#endif


exit:
    if (flattenedProps)
        CFRelease(flattenedProps);

    if (properties)
        CFRelease(properties);

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }

    return return_code;
}


/******************************************************************************
 * IOPMDeclareNetworkClientActivity
 *
 ******************************************************************************/
IOReturn IOPMDeclareNetworkClientActivity(
                                          CFStringRef          AssertionName,
                                          IOPMAssertionID      *AssertionID)
{

    IOReturn        return_code = kIOReturnError;
    mach_port_t     pm_server = MACH_PORT_NULL;
    kern_return_t   kern_result = KERN_SUCCESS;
    IOReturn        err;
    int             disableAppSleep = 0;


    CFMutableDictionaryRef  properties = NULL;
    CFDataRef               flattenedProps  = NULL;

    if (!AssertionName || !AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    properties = CFDictionaryCreateMutable(0, 1, &kCFTypeDictionaryKeyCallBacks, 
                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(properties, kIOPMAssertionNameKey, AssertionName);

    if (collectBackTrace) {
        saveBackTrace(properties);
    }

    flattenedProps = CFPropertyListCreateData(0, properties, 
                                              kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    if (!flattenedProps) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    kern_result = io_pm_declare_network_client_active(
                                                      pm_server, 
                                                      (vm_offset_t)CFDataGetBytePtr(flattenedProps),
                                                      CFDataGetLength(flattenedProps),
                                                      (int *)AssertionID,
                                                      &disableAppSleep,
                                                      &return_code);


    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
#if !TARGET_OS_IPHONE
    else if (disableAppSleep) {
        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsTakeAssertion,
                                    CFSTR("App is holding 'DeclareNetworkClientActivity' power assertion"));
    }
#endif


exit:
    if (flattenedProps)
        CFRelease(flattenedProps);

    if (properties)
        CFRelease(properties);

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }

    return return_code;
}

/*****************************************************************************/
IOReturn IOPMSetReservePowerMode(bool enable)
{

    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t           kern_result = KERN_SUCCESS;
    IOReturn                return_code;
    int                     rc = kIOReturnSuccess;

    return_code = _pm_connect(&pm_server);
    if (return_code != kIOReturnSuccess)
        return return_code;

    if(pm_server == MACH_PORT_NULL)
        return kIOReturnNotReady; 


    kern_result = io_pm_set_value_int( pm_server, kIOPMSetReservePowerMode, enable ? 1 : 0, &rc); 
    _pm_disconnect(pm_server);

    if (rc != kIOReturnSuccess)
        return rc;

    return kern_result;
}

/******************************************************************************
 * IOPMCopyAssertionsByProcessWithAllocator
 *
 ******************************************************************************/
static IOReturn _copyAssertionsByProcess(int operator, CFDictionaryRef *AssertionsByPid, CFAllocatorRef allocator)
{
    IOReturn                return_code     = kIOReturnError;
    CFArrayRef              flattenedDictionary = NULL;
    int                     flattenedArrayCount = 0;
    CFNumberRef             *newDictKeys = NULL;
    CFArrayRef              *newDictValues = NULL;

    if (!AssertionsByPid)
        return kIOReturnBadArgument;

    if ((operator != kIOPMAssertionMIGCopyAll) && (operator != kIOPMAssertionMIGCopyInactive)) {
        return kIOReturnBadArgument;
    }
    return_code = _copyPMServerObject(operator, 0, NULL, (CFTypeRef *)&flattenedDictionary);

    if (kIOReturnSuccess != return_code)
        goto exit;

    /*
     * This API returns a dictionary whose keys are process ID's.
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


    if (flattenedDictionary) {
        flattenedArrayCount = CFArrayGetCount(flattenedDictionary);
    }

    if (0 == flattenedArrayCount) {
        goto exit;
    }

    newDictKeys = (CFNumberRef *)malloc(sizeof(CFNumberRef) * flattenedArrayCount);
    newDictValues = (CFArrayRef *)malloc(sizeof(CFArrayRef) * flattenedArrayCount);

    if (!newDictKeys || !newDictValues)
        goto exit;

    for (int i=0; i < flattenedArrayCount; i++)
    {
        CFDictionaryRef dictionaryAtIndex = NULL;

        if ((dictionaryAtIndex = CFArrayGetValueAtIndex(flattenedDictionary, i)))
        {

            newDictKeys[i]      = CFDictionaryGetValue(dictionaryAtIndex, kIOPMAssertionPIDKey);
            newDictValues[i]    = CFDictionaryGetValue(dictionaryAtIndex, CFSTR("PerTaskAssertions"));
        }
    }

    *AssertionsByPid = CFDictionaryCreate(allocator,
                                          (const void **)newDictKeys, (const void **)newDictValues, flattenedArrayCount,
                                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    return_code = kIOReturnSuccess;

exit:
    if (newDictKeys)
        free(newDictKeys);
    if (newDictValues)
        free(newDictValues);
    if (flattenedDictionary)
        CFRelease(flattenedDictionary);
    return return_code;

}
IOReturn IOPMCopyAssertionsByProcessWithAllocator(CFDictionaryRef *AssertionsByPid, CFAllocatorRef allocator)
{
    return _copyAssertionsByProcess(kIOPMAssertionMIGCopyAll, AssertionsByPid, allocator);
}

/******************************************************************************
 * IOPMCopyAssertionsByProcess
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsByProcess(CFDictionaryRef         *AssertionsByPid)
{
    return IOPMCopyAssertionsByProcessWithAllocator(AssertionsByPid, kCFAllocatorDefault);
}

/******************************************************************************
 * IOPMCopyAssertionsByType
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsByType(CFStringRef type, CFArrayRef *assertionsArray)
{
    if (!assertionsArray) {
        return kIOReturnBadArgument;
    }

    return _copyPMServerObject(kIOPMAssertionMIGCopyByType, 0, type, (CFTypeRef *)assertionsArray);
}

/******************************************************************************
 * IOPMCopyAssertionsByType
 *
 ******************************************************************************/
IOReturn IOPMCopyInactiveAssertionsByProcess(CFDictionaryRef  *inactiveAssertions)
{
    return _copyAssertionsByProcess(kIOPMAssertionMIGCopyInactive, inactiveAssertions, kCFAllocatorDefault);
}

/******************************************************************************
 * IOPMAssertionCopyProperties
 *
 ******************************************************************************/
CFDictionaryRef IOPMAssertionCopyProperties(IOPMAssertionID theAssertion)
{
    CFDictionaryRef         theResult       = NULL;    

    _copyPMServerObject(kIOPMAssertionMIGCopyOneAssertionProperties, theAssertion, NULL, (CFTypeRef *)&theResult);

    return theResult;
}

/******************************************************************************
 * IOPMCopyAssertionsStatus
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsStatus(CFDictionaryRef *AssertionsStatus)
{
    if (!AssertionsStatus)
        return kIOReturnBadArgument;

    return _copyPMServerObject(kIOPMAssertionMIGCopyStatus, 0, NULL, (CFTypeRef *)AssertionsStatus);
}

/******************************************************************************
 * IOPMCopyDeviceRestartPreventers
 *
 ******************************************************************************/
IOReturn IOPMCopyDeviceRestartPreventers(CFArrayRef *preventers)
{
    IOReturn            ret             = kIOReturnError;
    CFMutableArrayRef   assertionArr    = NULL;
    CFStringRef         types[]         = { kIOPMAssertPreventUserIdleSystemSleep,
                                            kIOPMAssertionTypeSystemIsActive };
    int                 numTypes        = sizeof(types)/sizeof(types[0]);
    
    for (int i = 0; i < numTypes; i++) {
        CFArrayRef array = NULL;
        
        ret = IOPMCopyAssertionsByType(types[i], &array);
        if (ret != kIOReturnSuccess) {
            goto exit;
        }
        
        if (array) {
            for (CFIndex j = 0; j < CFArrayGetCount(array); j++) {
                CFDictionaryRef assertion       = CFArrayGetValueAtIndex(array, j);
                CFBooleanRef    allowsRestart   = NULL;
                
                /* skip assertions that allow restart */
                allowsRestart = CFDictionaryGetValue(assertion, kIOPMAssertionAllowsDeviceRestart);
                if (allowsRestart && CFBooleanGetValue(allowsRestart)) {
                    continue;
                }
                
                if (!assertionArr) {
                    assertionArr = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
                    if (!assertionArr) {
                        CFRelease(array);
                        goto exit;
                    }
                }
                
                CFArrayAppendValue(assertionArr, assertion);
            }
            CFRelease(array);
        }
    }
    
exit:
    *preventers = assertionArr;
    return ret;
}


/******************************************************************************
 * _copyPMServerObject
 *
 ******************************************************************************/
__private_extern__ IOReturn _copyPMServerObject(int selector, int assertionID,
                                                CFTypeRef selectorData, CFTypeRef *objectOut)
{
    IOReturn                return_code     = kIOReturnError;
    kern_return_t           kern_result     = KERN_SUCCESS;
    mach_port_t             pm_server       = MACH_PORT_NULL;
    vm_offset_t             theResultsPtr   = 0;
    mach_msg_type_number_t  theResultsCnt   = 0;
    CFDataRef               theResultData   = NULL;
    CFDataRef               inData          = NULL;
    vm_offset_t             inDataPtr       = NULL;
    CFIndex                 inDataLen       = 0;

    *objectOut = NULL;

    if(kIOReturnSuccess != (return_code = pm_connect_init(&pm_server))) {
        return kIOReturnNotFound;
    }

    if (selectorData) {
        inData = CFPropertyListCreateData(0, selectorData, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
        if (inData) {
            inDataPtr = (vm_offset_t)CFDataGetBytePtr(inData);
            inDataLen = CFDataGetLength(inData);
        }
    }
    kern_result = io_pm_assertion_copy_details(pm_server, assertionID, selector,
                                               inDataPtr, inDataLen,
                                               &theResultsPtr, &theResultsCnt, &return_code);

    if(KERN_SUCCESS != kern_result) {
        goto exit;
    }

    if (return_code != kIOReturnSuccess) {
        goto exit;
    }

    if ((theResultData = CFDataCreate(0, (const UInt8 *)theResultsPtr, (CFIndex)theResultsCnt)))
    {
        *objectOut = CFPropertyListCreateWithData(0, theResultData, kCFPropertyListImmutable, NULL, NULL);
        CFRelease(theResultData);
    }

    if (theResultsPtr && 0 != theResultsCnt) {
        vm_deallocate(mach_task_self(), theResultsPtr, theResultsCnt);
    }

exit:
    if (inData) {
        CFRelease(inData);
    }

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }

    return kIOReturnSuccess;
}

/******************************************************************************
 * IOPMCopyAssertionActivityLog
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionActivityLogWithAllocator(CFArrayRef *activityLog, bool *overflow, CFAllocatorRef allocator)
{
    static uint32_t refCnt = UINT_MAX;
    
    return IOPMCopyAssertionActivityUpdateWithAllocator(activityLog, overflow, &refCnt, allocator);
    
}

/******************************************************************************
 * IOPMCopyAssertionActivityLog
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionActivityLog(CFArrayRef *activityLog, bool *overflow)
{
    static uint32_t refCnt = UINT_MAX;

    return IOPMCopyAssertionActivityUpdate(activityLog, overflow, &refCnt);

}

IOReturn IOPMCopyAssertionActivityUpdateWithAllocator(CFArrayRef *logUpdates, bool *overflow, uint32_t *refCnt, CFAllocatorRef allocator)
{
    uint32_t                of;
    IOReturn                rc = kIOReturnInternalError;
    CFDataRef               unfolder = NULL;
    vm_offset_t             logPtr = NULL;
    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t           kern_result;
    mach_msg_type_number_t  logSize = 0;

    *logUpdates = NULL;
    _pm_connect(&pm_server);

    if(pm_server == MACH_PORT_NULL)
        return NULL;

    kern_result = io_pm_assertion_activity_log(pm_server,
                                               &logPtr, &logSize,
                                               refCnt, &of, &rc);

    if ((KERN_SUCCESS != kern_result) || (rc != kIOReturnSuccess)) {
        goto exit;
    }

    if (logSize == 0) {
        goto exit;
    }

    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)logPtr, (CFIndex)logSize, kCFAllocatorNull);
    if (unfolder)
    {
        *logUpdates = CFPropertyListCreateWithData(allocator, unfolder,
                                                   kCFPropertyListMutableContainers,
                                                   NULL, NULL);
        CFRelease(unfolder);
    }

    if (overflow) {
        *overflow = of ? true : false;
    }

exit:

    if (logPtr && logSize)  {
        vm_deallocate(mach_task_self(), logPtr, logSize);
    }

    if (MACH_PORT_NULL != pm_server)
        pm_connect_close(pm_server);
    
    return rc;
    
}
IOReturn IOPMCopyAssertionActivityUpdate(CFArrayRef *logUpdates, bool *overflow, uint32_t *refCnt)
{
    return IOPMCopyAssertionActivityUpdateWithAllocator(logUpdates, overflow,  refCnt, kCFAllocatorDefault);
}

IOReturn IOPMCopyAssertionActivityUpdateWithCallback(uint32_t *prevRefCnt, dispatch_queue_t queue, void(^inblock)(CFArrayRef logUpdates, bool overflow, CFArrayRef processList))
{
    IOReturn rc = kIOReturnSuccess;
    xpc_object_t connection = NULL;
    xpc_object_t msg = NULL;
    if (!prevRefCnt || !queue || !inblock) {
        rc = kIOReturnBadArgument;
        goto exit;
    }

    connection = xpc_connection_create_mach_service(POWERD_XPC_ID, dispatch_get_main_queue(), 0);
    if (!connection) {
        ERROR_LOG("Failed to create connection\n");
        rc = kIOReturnError;
        goto exit;
    }
    xpc_connection_set_target_queue(connection, getPMQueue());
    xpc_connection_set_event_handler(connection,
                                     ^(xpc_object_t e __unused ) { });
    xpc_connection_resume(connection);
    msg = xpc_dictionary_create(NULL, NULL, 0);
    if (!msg) {
        ERROR_LOG("Failed to create dictionary to send message");
        rc = kIOReturnError;
        goto exit;
    }
    xpc_dictionary_set_bool(msg, kAssertionCopyActivityUpdateMsg, true);
    xpc_dictionary_set_uint64(msg, kAssertionCopyActivityUpdateRefCntKey, *prevRefCnt);


    xpc_connection_send_message_with_reply(connection, msg, getPMQueue(), ^(xpc_object_t reply){
        if (xpc_get_type(reply) == XPC_TYPE_DICTIONARY) {
            xpc_object_t data = xpc_dictionary_get_value(reply, kAssertionCopyActivityUpdateDataKey);
            uint32_t ref_cnt = xpc_dictionary_get_uint64(reply, kAssertionCopyActivityUpdateRefCntKey);
            bool overflow_value = xpc_dictionary_get_bool(reply, kAssertionCopyActivityUpdateOverflowKey);
            xpc_object_t processes = xpc_dictionary_get_value(reply, kAssertionCopyActivityUpdateProcessListKey);
            CFArrayRef data_array = NULL;
            CFArrayRef process_array = NULL;
            DEBUG_LOG("Received assertion activity update refcnt %u overflow %d", ref_cnt, overflow_value);
            if (data) {
                data_array = _CFXPCCreateCFObjectFromXPCObject(data);
            } else {
                DEBUG_LOG("No AssertionActivity in xpc reply");
            }
            if (processes) {
                process_array = _CFXPCCreateCFObjectFromXPCObject(processes);
            }
            *prevRefCnt = ref_cnt;
            dispatch_async(queue, ^{
                inblock(data_array, overflow_value, process_array);
                if (data_array) {
                    CFRelease(data_array);
                }
                if (process_array) {
                    CFRelease(process_array);
                }
            });
        } else {
            ERROR_LOG("Received an error in response to copy assertion activity");
        }
        xpc_connection_cancel(connection);
    });

exit:
    if (msg) {
        xpc_release(msg);
    }
    if (connection) {
        xpc_release(connection);
    }

    return rc;

}
/*****************************************************************************/
IOReturn IOPMSetAssertionActivityLog(bool enable)
{

    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t           kern_result = KERN_SUCCESS;
    IOReturn                return_code;
    int                     rc = kIOReturnSuccess;

    return_code = _pm_connect(&pm_server);
    if(return_code != kIOReturnSuccess) {
        return return_code;
    }

    if(pm_server == MACH_PORT_NULL)
        return kIOReturnNotReady; 


    kern_result = io_pm_set_value_int( pm_server, kIOPMSetAssertionActivityLog, enable ? 1 : 0, &rc); 
    _pm_disconnect(pm_server);

    return kern_result;
}
/*****************************************************************************/
IOReturn IOPMSetAssertionActivityAggregate(bool enable)
{

    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t           kern_result = KERN_SUCCESS;
    IOReturn                return_code;
    int                     rc = kIOReturnSuccess;

    return_code = _pm_connect(&pm_server);
    if(return_code != kIOReturnSuccess) {
        return return_code;
    }

    if(pm_server == MACH_PORT_NULL)
        return kIOReturnNotReady; 


    kern_result = io_pm_set_value_int( pm_server, kIOPMSetAssertionActivityAggregate, enable ? 1 : 0, &rc); 
    _pm_disconnect(pm_server);

    return kern_result;
}


/*****************************************************************************/
CFDictionaryRef IOPMCopyAssertionActivityAggregateWithAllocator(CFAllocatorRef allocator)
{
    IOReturn                rc = kIOReturnInternalError;
    CFDataRef               unfolder = NULL;
    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t           kern_result;
    vm_offset_t             addr = NULL;
    mach_msg_type_number_t  size = 0;
    CFDictionaryRef  statsData = NULL;


    _pm_connect(&pm_server);

    if(pm_server == MACH_PORT_NULL)
        return NULL;

    kern_result = io_pm_assertion_activity_aggregate(pm_server,
                                                     &addr,  &size,
                                                     &rc);

    if ((KERN_SUCCESS != kern_result) || (rc != kIOReturnSuccess)) {
        goto exit;
    }

    unfolder = CFDataCreateWithBytesNoCopy(0, (const UInt8 *)addr, (CFIndex)size, kCFAllocatorNull);
    if (unfolder)
    {
        statsData = CFPropertyListCreateWithData(allocator, unfolder,
                                                 kCFPropertyListMutableContainers, NULL, NULL);
        CFRelease(unfolder);
    }

exit:

    if (addr && size)
        vm_deallocate(mach_task_self(), addr, size);


    if (MACH_PORT_NULL != pm_server)
        pm_connect_close(pm_server);

    return statsData;
}

CFDictionaryRef IOPMCopyAssertionActivityAggregate( )
{
    return IOPMCopyAssertionActivityAggregateWithAllocator(kCFAllocatorDefault);
}
/*****************************************************************************/
void IOPMAssertionSetBTCollection(bool enable)
{
    int collectBackTraceToken = 0;

    notify_register_check(kIOPMAssertionsCollectBTString, &collectBackTraceToken);
    notify_set_state(collectBackTraceToken, enable ? 1 : 0);
    notify_post(kIOPMAssertionsCollectBTString);
    notify_cancel(collectBackTraceToken);
}

/*****************************************************************************/
IOReturn IOPMSetAssertionExceptionLimits(CFDictionaryRef procLimits)
{
    IOReturn                return_code     = kIOReturnError;
    kern_return_t           kern_result     = KERN_SUCCESS;
    mach_port_t             pm_server       = MACH_PORT_NULL;
    IOReturn                err;
    CFDataRef               flattenedData  = NULL;

    if (!isA_CFDictionary(procLimits)) {
        return kIOReturnBadArgument;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    flattenedData = CFPropertyListCreateData(0, procLimits, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    if (!flattenedData) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    kern_result = io_pm_set_exception_limits(pm_server,
                                             (vm_offset_t)CFDataGetBytePtr(flattenedData),
                                             CFDataGetLength(flattenedData),
                                             &return_code);
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

exit:
    if (flattenedData) {
        CFRelease(flattenedData);
    }
    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }
    return return_code;
}
/*****************************************************************************/
typedef struct {
    int     token;
} _IOPMAssertionExceptionHandle;

IOPMNotificationHandle IOPMScheduleAssertionExceptionNotification(dispatch_queue_t queue,
                                                                  void (^excHandler)(IOPMAssertionException, pid_t))
{
    _IOPMAssertionExceptionHandle *hdl = NULL;
    uint32_t status;

    hdl = calloc(1, sizeof(_IOPMAssertionExceptionHandle));
    if (!hdl) {
        return NULL;
    }

    notify_handler_t calloutBlock = ^(int token) {
        uint32_t sts;
        uint64_t data;
        IOPMAssertionException exc;
        pid_t pid;

        sts = notify_get_state(token, &data);
        if (sts == NOTIFY_STATUS_OK) {
            pid = data & 0xffffffff;
            exc = (data >> 32);

            excHandler(exc, pid);
        }
    };
    status = notify_register_dispatch(kIOPMAssertionExceptionNotifyName,
                                      &hdl->token, queue, calloutBlock);

    if (status != NOTIFY_STATUS_OK) {
        free(hdl);
        hdl = NULL;
    }

    return hdl;
}

void IOPMUnregisterExceptionNotification(IOPMNotificationHandle handle)
{
    _IOPMAssertionExceptionHandle *excHandle = (_IOPMAssertionExceptionHandle *)handle;
    if (excHandle) {
        if (excHandle->token) {
            notify_cancel(excHandle->token);
        }
        bzero(excHandle, sizeof(*excHandle));
        free(excHandle);
    }
}
