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
#include <CoreFoundation/CFPriv.h>
#include <mach/mach.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include "IOSystemConfiguration.h"
#include <sys/time.h>
#include <notify.h>
#include <execinfo.h>
#include <asl.h>

#include "powermanagement_mig.h"
#include "powermanagement.h"

#include <servers/bootstrap.h>


#define kAssertionsArraySize        5
#define NUM_BT_FRAMES               8

static uint64_t  collectBackTrace = 0;

IOReturn _pm_connect(mach_port_t *newConnection);
IOReturn _pm_disconnect(mach_port_t connection);
__private_extern__ IOReturn _copyPMServerObject(int selector, int assertionID, CFTypeRef *objectOut);
__private_extern__ io_registry_entry_t getPMRootDomainRef(void);

static IOReturn pm_connect_init(mach_port_t *newConnection)
{
#if __i386__ || __x86_64__
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
    size_t              btsize = 0;
    char                **syms = NULL;
    int                 i;
    CFStringRef         frame_cf = NULL;
    CFMutableArrayRef   syms_cf = NULL;


    int nframes = backtrace((void**)(&bt), NUM_BT_FRAMES);
    btsize = nframes * sizeof(bt[0]);

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
                                     IOPMAssertionLevel   AssertionLevel __unused,
                                     CFStringRef          AssertionName,
                                     IOPMAssertionID      *AssertionID)
{
    CFMutableDictionaryRef      properties = NULL;
    IOReturn                    result = kIOReturnError;

    if (!AssertionName || !AssertionID || !AssertionType)
        return kIOReturnBadArgument;

    properties = CFDictionaryCreateMutable(0, 3, &kCFTypeDictionaryKeyCallBacks, 
                                           &kCFTypeDictionaryValueCallBacks);

    if (properties)
    {

        CFDictionarySetValue(properties, kIOPMAssertionTypeKey, AssertionType);

        CFDictionarySetValue(properties, kIOPMAssertionNameKey, AssertionName);

        CFDictionarySetValue(properties, kIOPMAssertionUsedDeprecatedCreateAPIKey, kCFBooleanTrue);

        result = IOPMAssertionCreateWithProperties(properties, AssertionID);

        CFRelease(properties);
    }    

    return result;
}

/******************************************************************************
 * IOPMAssertionCreateWithDescription
 *
 ******************************************************************************/

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
    CFMutableDictionaryRef  mutableProps = NULL;
    int                     disableAppSleep = 0;
#if TARGET_OS_IPHONE
    static int              resyncToken = 0;
    static CFMutableDictionaryRef  resyncCopy = NULL;
#endif

    if (!AssertionProperties || !AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }


    assertionTypeString = CFDictionaryGetValue(AssertionProperties, kIOPMAssertionTypeKey);


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

    if (collectBackTrace) {
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


    kern_result = io_pm_assertion_create( pm_server, 
                                          (vm_offset_t)CFDataGetBytePtr(flattenedProps),
                                          CFDataGetLength(flattenedProps),
                                          (int *)AssertionID, 
                                          &disableAppSleep,
                                          &return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }
#if !TARGET_OS_IPHONE
    else if (disableAppSleep) {
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
    
    if (!assertion_properties || !the_block) {
        return kIOReturnBadArgument;
    }
    
    IOPMAssertionCreateWithProperties(assertion_properties, _id);
    
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

    if (!theAssertion) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_assertion_retain_release( pm_server, 
                                                  (int)theAssertion,
                                                  kIOPMAssertionMIGDoRetain,
                                                  &disableAppSleep,
                                                  &enableAppSleep,
                                                  &return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }
#if !TARGET_OS_IPHONE
    else if (disableAppSleep) {
        CFStringRef appSleepString = NULL;

        appSleepString = CFStringCreateWithFormat(NULL, NULL, CFSTR("App is holding power assertion %u"),
                                                  theAssertion);
        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsTakeAssertion,  appSleepString);

        CFRelease(appSleepString);
    }
#endif


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

    if (!AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    err = pm_connect_init(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_assertion_retain_release( pm_server, 
                                                  (int)AssertionID,
                                                  kIOPMAssertionMIGDoRelease,
                                                  &disableAppSleep,
                                                  &enableAppSleep,
                                                  &return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }
#if !TARGET_OS_IPHONE
    else if (enableAppSleep) {
        CFStringRef appSleepString = NULL;

        appSleepString = CFStringCreateWithFormat(NULL, NULL, CFSTR("App released its last power assertion %u"),
                                                  AssertionID);
        __CFRunLoopSetOptionsReason(__CFRunLoopOptionsDropAssertion, appSleepString);

        CFRelease(appSleepString);
    }
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
    CFDictionaryRef         sendDict        = NULL;
    int                     disableAppSleep = 0;
    int                     enableAppSleep = 0;

    if (!theAssertion) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    return_code = pm_connect_init(&pm_server);

    if(kIOReturnSuccess != return_code) {
        goto exit;
    }

    sendDict = CFDictionaryCreate(0, (const void **)&theProperty, (const void **)&theValue, 1, 
                                  &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (sendDict) {
        sendData = CFPropertyListCreateData(0, sendDict, kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
        CFRelease(sendDict);
    }


    kern_result = io_pm_assertion_set_properties(pm_server, 
                                                 (int)theAssertion,
                                                 (vm_offset_t)CFDataGetBytePtr(sendData),
                                                 CFDataGetLength(sendData),
                                                 &disableAppSleep,
                                                 &enableAppSleep,
                                                 (int *)&return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
#if !TARGET_OS_IPHONE
    else if (disableAppSleep) {
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


exit:
    if (sendData)
        CFRelease(sendData);

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }
    return return_code;    
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
#define TCPKEEPALIVE 1
#if TCPKEEPALIVE
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
#else
    if (AssertionID) {
        *AssertionID = kIOPMNullAssertionID;
    }
    return kIOReturnUnsupported;
#endif
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
    IOReturn        err;
    IOReturn        return_code = kIOReturnError;
    mach_port_t     pm_server   = MACH_PORT_NULL;
    kern_return_t   kern_result = KERN_SUCCESS;

    CFMutableDictionaryRef  properties      = NULL;
    CFDataRef               flattenedProps  = NULL;

    if (!AssertionName || !AssertionID || !SystemState) {
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

    if (properties)
        CFRelease(properties);

    if (MACH_PORT_NULL != pm_server) {
        pm_connect_close(pm_server);
    }

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
    static struct   timeval prev_ts = {0,0};
    struct timeval  ts;
    int             disableAppSleep = 0;


    CFMutableDictionaryRef  properties = NULL;
    CFDataRef               flattenedProps  = NULL;

    if (!AssertionName || !AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }

    gettimeofday(&ts, NULL);
    if (ts.tv_sec - prev_ts.tv_sec <= 5) {
        if ( *AssertionID == kIOPMNullAssertionID )
            *AssertionID = 0xabcd; /* Give a dummy id */
        return_code = kIOReturnSuccess;
        goto exit;
    }
    prev_ts = ts;

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

    if(pm_server == MACH_PORT_NULL)
        return kIOReturnNotReady; 


    kern_result = io_pm_set_value_int( pm_server, kIOPMSetReservePowerMode, enable ? 1 : 0, &rc); 
    _pm_disconnect(pm_server);

    if (rc != kIOReturnSuccess)
        return rc;

    return kern_result;
}


/******************************************************************************
 * IOPMCopyAssertionsByProcess
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsByProcess(CFDictionaryRef         *AssertionsByPid)
{
    IOReturn                return_code     = kIOReturnError;
    CFArrayRef              flattenedDictionary = NULL;
    int                     flattenedArrayCount = 0;
    CFNumberRef             *newDictKeys = NULL;
    CFArrayRef              *newDictValues = NULL;

    if (!AssertionsByPid)
        return kIOReturnBadArgument;

    return_code = _copyPMServerObject(kIOPMAssertionMIGCopyAll, 0, (CFTypeRef *)&flattenedDictionary);

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

    newDictKeys = (CFNumberRef *)malloc(sizeof(CFTypeRef) * flattenedArrayCount);
    newDictValues = (CFArrayRef *)malloc(sizeof(CFTypeRef) * flattenedArrayCount);

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

    *AssertionsByPid = CFDictionaryCreate(kCFAllocatorDefault,
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

/******************************************************************************
 * IOPMAssertionCopyProperties
 *
 ******************************************************************************/
CFDictionaryRef IOPMAssertionCopyProperties(IOPMAssertionID theAssertion)
{
    CFDictionaryRef         theResult       = NULL;    

    _copyPMServerObject(kIOPMAssertionMIGCopyOneAssertionProperties, theAssertion, (CFTypeRef *)&theResult);

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

    return _copyPMServerObject(kIOPMAssertionMIGCopyStatus, 0, (CFTypeRef *)AssertionsStatus);
}

/******************************************************************************
 * _copyPMServerObject
 *
 ******************************************************************************/
__private_extern__ IOReturn _copyPMServerObject(int selector, int assertionID, CFTypeRef *objectOut)
{
    IOReturn                return_code     = kIOReturnError;
    kern_return_t           kern_result     = KERN_SUCCESS;
    mach_port_t             pm_server       = MACH_PORT_NULL;
    vm_offset_t             theResultsPtr   = 0;
    mach_msg_type_number_t  theResultsCnt   = 0;
    CFDataRef               theResultData   = NULL;

    *objectOut = NULL;

    if(kIOReturnSuccess != (return_code = pm_connect_init(&pm_server))) {
        return kIOReturnNotFound;
    }

    kern_result = io_pm_assertion_copy_details(pm_server, assertionID, selector,
                                               &theResultsPtr, &theResultsCnt, &return_code);

    if(KERN_SUCCESS != kern_result) {
        return kIOReturnInternalError;
    }

    if (return_code != kIOReturnSuccess)
        return return_code;

    if ((theResultData = CFDataCreate(0, (const UInt8 *)theResultsPtr, (CFIndex)theResultsCnt)))
    {
        *objectOut = CFPropertyListCreateWithData(0, theResultData, kCFPropertyListImmutable, NULL, NULL);
        CFRelease(theResultData);
    }

    if (theResultsPtr && 0 != theResultsCnt) {
        vm_deallocate(mach_task_self(), theResultsPtr, theResultsCnt);
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
IOReturn IOPMCopyAssertionActivityLog(CFArrayRef *activityLog, bool *overflow)
{
    static uint32_t refCnt = UINT_MAX;

    return IOPMCopyAssertionActivityUpdate(activityLog, overflow, &refCnt);

}

IOReturn IOPMCopyAssertionActivityUpdate(CFArrayRef *logUpdates, bool *overflow, uint32_t *refCnt)
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
        *logUpdates = CFPropertyListCreateWithData(0, unfolder, 
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

/*****************************************************************************/
IOReturn IOPMSetAssertionActivityLog(bool enable)
{

    mach_port_t             pm_server = MACH_PORT_NULL;
    kern_return_t           kern_result = KERN_SUCCESS;
    IOReturn                return_code;
    int                     rc = kIOReturnSuccess;

    return_code = _pm_connect(&pm_server);

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

    if(pm_server == MACH_PORT_NULL)
        return kIOReturnNotReady; 


    kern_result = io_pm_set_value_int( pm_server, kIOPMSetAssertionActivityAggregate, enable ? 1 : 0, &rc); 
    _pm_disconnect(pm_server);

    return kern_result;
}


/*****************************************************************************/
CFDictionaryRef IOPMCopyAssertionActivityAggregate( )
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
        statsData = CFPropertyListCreateWithData(0, unfolder, 
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
/*****************************************************************************/
void IOPMAssertionSetBTCollection(bool enable)
{
    int collectBackTraceToken = 0;

    notify_register_check(kIOPMAssertionsCollectBTString, &collectBackTraceToken);
    notify_set_state(collectBackTraceToken, enable ? 1 : 0);
    notify_post(kIOPMAssertionsCollectBTString);
    notify_cancel(collectBackTraceToken);
}
