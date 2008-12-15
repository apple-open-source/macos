/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>
#include <servers/bootstrap.h>
#include "IOSystemConfiguration.h"
#include "IOPMLibPrivate.h"
#include "powermanagement.h"

#define kAssertionsArraySize        5

static IOReturn _pm_connect(mach_port_t *newConnection)
{
    kern_return_t       kern_result = KERN_SUCCESS;
    
    if(!newConnection) return kIOReturnBadArgument;

    // open reference to PM configd
    kern_result = bootstrap_look_up(bootstrap_port, 
            kIOPMServerBootstrapName, newConnection);
    if(KERN_SUCCESS != kern_result) {
        return kIOReturnError;
    }
    return kIOReturnSuccess;
}

static IOReturn _pm_disconnect(mach_port_t connection)
{
    if(!connection) return kIOReturnBadArgument;
    mach_port_destroy(mach_task_self(), connection);
    return kIOReturnSuccess;
}

static bool _supportedAssertion(CFStringRef assertion)
{
    return (CFEqual(assertion, kIOPMAssertionTypeNoIdleSleep)
        || CFEqual(assertion, kIOPMAssertionTypeNoDisplaySleep)
        || CFEqual(assertion, kIOPMAssertionTypeDisableInflow)
        || CFEqual(assertion, kIOPMAssertionTypeInhibitCharging)

#if TARGET_EMBEDDED_OS
        // kIOPMAssertionTypeEnableIdleSleep is only supported on
        // embedded platforms. IOPMAssertionCreate returns an error
        // when a caller tries to assert it on user OS X.
        || CFEqual(assertion, kIOPMAssertionTypeEnableIdleSleep)
#endif

        || CFEqual(assertion, kIOPMAssertionTypeDisableLowBatteryWarnings) 
        || CFEqual(assertion, kIOPMAssertionTypeNeedsCPU) );
}


/******************************************************************************
 * IOPMAssertionCreate
 *
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
 ******************************************************************************/
IOReturn IOPMAssertionCreateWithName(
    CFStringRef          AssertionType, 
    IOPMAssertionLevel   AssertionLevel,
    CFStringRef          AssertionName,
    IOPMAssertionID      *AssertionID)
{
    static const int        kMaxNameLength = 128;

    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    char                    assertion_str[50];
    int                     assertion_len = 50;
    char                    name_str[kMaxNameLength+1];
    int                     name_len = kMaxNameLength+1;
    mach_port_t             pm_server = MACH_PORT_NULL;
    mach_port_t             task_self = mach_task_self();
    IOReturn                err;

    // Set assertion_id to a known invalid setting. If successful, it will
    // get a valid value later on.
    *AssertionID = -1;

    if(!_supportedAssertion(AssertionType))
    {
        return kIOReturnUnsupported;
    }

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    CFStringGetCString( AssertionType, assertion_str, 
                        assertion_len, kCFStringEncodingMacRoman);
    assertion_len = strlen(assertion_str)+1;
    
    // Check validity of input name string
    if (!AssertionName || (kMaxNameLength < CFStringGetLength(AssertionName))) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    CFStringGetCString( AssertionName, name_str,
                        name_len, kCFStringEncodingMacRoman);
    name_len = strlen(name_str) + 1;

    // io_pm_assertion_create mig's over to configd, and it's configd 
    // that actively tracks and manages the list of active power assertions.
    kern_result = io_pm_assertion_create(
            pm_server, 
            task_self,
            name_str,
            name_len,
            assertion_str,
            assertion_len,
            AssertionLevel, 
            (int *)AssertionID,
            &return_code);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }

exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }
    return return_code;
}

/******************************************************************************
 * IOPMAssertionsRelease
 *
 ******************************************************************************/
IOReturn IOPMAssertionRelease(
    IOPMAssertionID         AssertionID)
{
    IOReturn                return_code = kIOReturnError;
    kern_return_t           kern_result = KERN_SUCCESS;
    mach_port_t             pm_server = MACH_PORT_NULL;
    mach_port_t             task_self = mach_task_self();
    IOReturn                err;

    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    kern_result = io_pm_assertion_release(
            pm_server, 
            task_self,
            AssertionID,
            &return_code);
            
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }

    _pm_disconnect(pm_server);
exit:
    return return_code;
}

/******************************************************************************
 * IOPMCopyAssertionsByProcess
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsByProcess(
    CFDictionaryRef         *AssertionsByPid)
{
    SCDynamicStoreRef   dynamicStore = NULL;
    CFStringRef         dataKey = NULL;
    IOReturn            returnCode = kIOReturnError;
    int                 flattenedArrayCount = 0;
    CFArrayRef          flattenedDictionary = NULL;    
    CFNumberRef         *newDictKeys = NULL;
    CFArrayRef          *newDictValues = NULL;
    
    if (!AssertionsByPid)
        return kIOReturnBadArgument;
        
    *AssertionsByPid = NULL;
    
    dynamicStore = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("PM IOKit User Library"),
                                         NULL, NULL);

    if (NULL == dynamicStore) {
        goto exit;
    }
    
    dataKey = IOPMAssertionCreatePIDMappingKey();

    flattenedDictionary = SCDynamicStoreCopyValue(dynamicStore, dataKey);

    if (!flattenedDictionary) {
        returnCode = kIOReturnSuccess;
        goto exit;
    }

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

    flattenedArrayCount = CFArrayGetCount(flattenedDictionary);
    
    newDictKeys = (CFNumberRef *)malloc(sizeof(CFTypeRef) * flattenedArrayCount);
    newDictValues = (CFArrayRef *)malloc(sizeof(CFTypeRef) * flattenedArrayCount);
    
    if (!newDictKeys || !newDictValues)
        goto exit;
    
    for (int i=0; i < flattenedArrayCount; i++)
    {
        CFDictionaryRef         dictionaryAtIndex = NULL;

        dictionaryAtIndex = CFArrayGetValueAtIndex(flattenedDictionary, i);

        if (!dictionaryAtIndex)
            continue;

        newDictKeys[i] = CFDictionaryGetValue(
                                dictionaryAtIndex,
                                kIOPMAssertionPIDKey);
        newDictValues[i] = CFDictionaryGetValue(
                                dictionaryAtIndex,
                                CFSTR("PerTaskAssertions"));    
    }
    

    *AssertionsByPid = CFDictionaryCreate(
                                kCFAllocatorDefault,
                                (const void **)newDictKeys,
                                (const void **)newDictValues,
                                flattenedArrayCount,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);

    returnCode = kIOReturnSuccess;
exit:
    if (newDictKeys)
        free(newDictKeys);
    if (newDictValues)
        free(newDictValues);
    if (dynamicStore)
        CFRelease(dynamicStore);
    if (flattenedDictionary)
        CFRelease(flattenedDictionary);
    if (dataKey)
        CFRelease(dataKey);
    return returnCode;
}


/******************************************************************************
 * IOPMCopyAssertionsStatus
 *
 ******************************************************************************/
IOReturn IOPMCopyAssertionsStatus(
    CFDictionaryRef         *AssertionsStatus)
{
    SCDynamicStoreRef   dynamicStore = NULL;
    CFStringRef         dataKey = NULL;
    IOReturn            returnCode = kIOReturnError;
    CFDictionaryRef     returnDictionary = NULL;
    
    if (!AssertionsStatus)
        return kIOReturnBadArgument;

    *AssertionsStatus = NULL;
    
    dynamicStore = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("PM IOKit User Library"),
                                         NULL, NULL);

    if (NULL == dynamicStore) {
        goto exit;
    }
    
    dataKey = IOPMAssertionCreateAggregateAssertionKey();

    returnDictionary = SCDynamicStoreCopyValue(dynamicStore, dataKey);
    
    // TODO: check return of SCDynamicStoreCopyVale

    *AssertionsStatus = returnDictionary;

    returnCode = kIOReturnSuccess;
exit:
    if (dynamicStore)
        CFRelease(dynamicStore);
    if (dataKey)
        CFRelease(dataKey);
    return returnCode;
}

/******************************************************************************
 * IOPMAssertionSetTimeout
 *
 ******************************************************************************/
IOReturn IOPMAssertionSetTimeout(
    IOPMAssertionID whichAssertion, 
    CFTimeInterval timeoutInterval)
{
    IOReturn            return_code = kIOReturnError;
    mach_port_t         pm_server = MACH_PORT_NULL;
    int                 seconds = lrint(timeoutInterval);
    kern_return_t       kern_result;
    IOReturn            err;
    
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }

    kern_result = io_pm_assertion_settimeout(
                                    pm_server,
                                    mach_task_self(),
                                    whichAssertion, /* id */
                                    seconds,        /* interval */
                                    &return_code);

    if (KERN_SUCCESS != kern_result) {
        return_code = kern_result;
    }

exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }

    return return_code;
}

/******************************************************************************
 * IOPMCopyTimedOutAssertions
 *
 ******************************************************************************/
IOReturn IOPMCopyTimedOutAssertions(
    CFArrayRef *timedOutAssertions)
{
    SCDynamicStoreRef   dynamicStore = NULL;
    CFStringRef         dataKey = NULL;
    IOReturn            returnCode = kIOReturnError;
    CFArrayRef          returnArray = NULL;
    
    if (!timedOutAssertions)
        return kIOReturnBadArgument;

    *timedOutAssertions = NULL;
    
    dynamicStore = SCDynamicStoreCreate(kCFAllocatorDefault,
                                         CFSTR("PM IOKit User Library"),
                                         NULL, NULL);

    if (NULL == dynamicStore) {
        goto exit;
    }
    
    dataKey = IOPMAssertionCreateTimeOutKey();

    returnArray = SCDynamicStoreCopyValue(dynamicStore, dataKey);
    
    // TODO: check return of SCDynamicStoreCopyVale

    *timedOutAssertions = returnArray;

    returnCode = kIOReturnSuccess;
exit:
    if (dynamicStore)
        CFRelease(dynamicStore);
    if (dataKey)
        CFRelease(dataKey);
    return returnCode;
}

/*
 * State:/IOKit/PowerManagement/Assertions/TimedOut
 */

CFStringRef IOPMAssertionCreateTimeOutKey(void)
{
    return SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@/%@"),
                            kSCDynamicStoreDomainState, 
                            CFSTR("/IOKit/PowerManagement/Assertions"), 
                            CFSTR("TimedOut"));
}

/*
 * State:/IOKit/PowerManagement/Assertions/ByProcess
 */

CFStringRef IOPMAssertionCreatePIDMappingKey(void)
{
    return SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@/%@"),
                            kSCDynamicStoreDomainState, 
                            CFSTR("/IOKit/PowerManagement/Assertions"), 
                            CFSTR("ByProcess"));
}


/*
 * State:/IOKit/PowerManagement/Assertions
 */

CFStringRef IOPMAssertionCreateAggregateAssertionKey(void)
{
    return SCDynamicStoreKeyCreate(
                            kCFAllocatorDefault, 
                            CFSTR("%@%@"),
                            kSCDynamicStoreDomainState, 
                            CFSTR("/IOKit/PowerManagement/Assertions"));
}
