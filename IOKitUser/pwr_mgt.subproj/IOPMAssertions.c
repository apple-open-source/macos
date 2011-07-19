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
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include "IOSystemConfiguration.h"

#include "powermanagement_mig.h"
#include "powermanagement.h"

#include <servers/bootstrap.h>

#define kAssertionsArraySize        5

static const int kMaxNameLength = 128;


static IOReturn _pm_connect(mach_port_t *newConnection)
{
    kern_return_t       kern_result = KERN_SUCCESS;
    
    if(!newConnection) return kIOReturnBadArgument;
    
    // open reference to PM configd
    kern_result = bootstrap_look_up(bootstrap_port, 
                                    kIOPMServerBootstrapName, newConnection);
    if(KERN_SUCCESS != kern_result) {
        *newConnection = MACH_PORT_NULL;
        return kIOReturnError;
    }
    return kIOReturnSuccess;
}

static IOReturn _pm_disconnect(mach_port_t connection)
{
    if(!connection) return kIOReturnBadArgument;
    mach_port_deallocate(mach_task_self(), connection);
    return kIOReturnSuccess;
}

/*
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
            || CFEqual(assertion, kIOPMAssertionTypeDisableRealPowerSources_Debug)
            || CFEqual(assertion, kIOPMAssertionTypeDisableLowBatteryWarnings) 
            || CFEqual(assertion, kIOPMAssertionTypeNeedsCPU) 
            || CFEqual(assertion, kIOPMAssertionTypeDenySystemSleep) );
}
*/

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

    if (!AssertionProperties || !AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    flattenedProps = CFPropertyListCreateData(0, AssertionProperties, 
                          kCFPropertyListBinaryFormat_v1_0, 0, NULL /* error */);
    if (!flattenedProps) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    kern_result = io_pm_assertion_create( pm_server, 
                                          (vm_offset_t)CFDataGetBytePtr(flattenedProps),
                                          CFDataGetLength(flattenedProps),
                                          (int *)AssertionID, 
                                          &return_code);
    
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }

exit:
	if (flattenedProps) {
		CFRelease(flattenedProps);
	}

    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }
    
    return return_code;
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
    
    if (!theAssertion) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    kern_result = io_pm_assertion_retain_release( pm_server, 
                                         (int)theAssertion,
                                         kIOPMAssertionMIGDoRetain,
                                         &return_code);
    
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }
    
exit:
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
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
    
    if (!AssertionID) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    err = _pm_connect(&pm_server);
    if(kIOReturnSuccess != err) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    kern_result = io_pm_assertion_retain_release( pm_server, 
                                                 (int)AssertionID,
                                                 kIOPMAssertionMIGDoRelease,
                                                 &return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
    }
    
    _pm_disconnect(pm_server);
exit:
    return return_code;
}

/******************************************************************************
 * IOPMAssertionCopyProperties
 *
 ******************************************************************************/
CFDictionaryRef IOPMAssertionCopyProperties(IOPMAssertionID theAssertion)
{
    IOReturn                return_code     = kIOReturnError;
    kern_return_t           kern_result     = KERN_SUCCESS;
    mach_port_t             pm_server       = MACH_PORT_NULL;
    vm_offset_t             theResultsPtr   = 0;
    mach_msg_type_number_t  theResultsCnt   = 0;
    CFDictionaryRef         theResult       = NULL;
    
    if (!theAssertion) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    return_code = _pm_connect(&pm_server);

    if(kIOReturnSuccess != return_code) {
        goto exit;
    }
    
    kern_result = io_pm_assertion_copy_details(pm_server, 
                                               (int)theAssertion,
                                               kIOPMAssertionMIGCopyOneAssertionProperties,
                                               &theResultsPtr,
                                               &theResultsCnt,
                                               &return_code);

    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
    if (kIOReturnSuccess == return_code) 
    {
        CFDataRef   flattenedResult = NULL;

        flattenedResult = CFDataCreate(0, (const UInt8 *)theResultsPtr, (CFIndex)theResultsCnt);

        if (flattenedResult) {
            theResult = (CFDictionaryRef)CFPropertyListCreateWithData(0, flattenedResult, 
                              kCFPropertyListImmutable, NULL /* format */, NULL /* error */);

            CFRelease(flattenedResult);
        }

    }

exit:
    if (theResultsPtr && 0 != theResultsCnt) {
        vm_deallocate(mach_task_self(), theResultsPtr, theResultsCnt);
    }
    
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }
    return theResult;
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
    
    if (!theAssertion) {
        return_code = kIOReturnBadArgument;
        goto exit;
    }
    
    return_code = _pm_connect(&pm_server);
    
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
                                               (int *)&return_code);
    
    if(KERN_SUCCESS != kern_result) {
        return_code = kIOReturnInternalError;
        goto exit;
    }
    
exit:
    if (sendData)
        CFRelease(sendData);
    
    if (MACH_PORT_NULL != pm_server) {
        _pm_disconnect(pm_server);
    }
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
                                   _io_kSCDynamicStoreDomainState, 
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
                                   _io_kSCDynamicStoreDomainState, 
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
                                   _io_kSCDynamicStoreDomainState, 
                                   CFSTR("/IOKit/PowerManagement/Assertions"));
}

