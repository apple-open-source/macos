/*
 * Copyright (c) 1999-2008 Apple Computer, Inc.  All Rights Reserved.
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

#define DEBUG_ASSERT_COMPONENT_NAME_STRING IOHIDDevice

#include <pthread.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBase.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <asl.h>
#include <AssertMacros.h>
#include "IOHIDDevicePlugIn.h"
#include "IOHIDDevice.h"
#include "IOHIDQueue.h"
#include "IOHIDElement.h"
#include "IOHIDTransaction.h"
#include "IOHIDLibPrivate.h"
#include "IOHIDManagerPersistentProperties.h"

//------------------------------------------------------------------------------
static IOHIDDeviceRef   __IOHIDDeviceCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused);
static void             __IOHIDDeviceRelease( CFTypeRef object );
static void             __IOHIDDeviceRegister(void);
static CFArrayRef       __IOHIDDeviceCopyMatchingInputElements(
                                    IOHIDDeviceRef          device, 
                                    CFArrayRef              multiple);
static void             __IOHIDDeviceRegisterMatchingInputElements(
                                    IOHIDDeviceRef          device, 
                                    IOHIDQueueRef           queue,
                                    CFArrayRef              mutlipleMatch);
static void             __IOHIDDeviceInputElementValueCallback(
                                    void *                  context,
                                    IOReturn                result, 
                                    void *                  sender);
static void             __IOHIDDeviceNotification(
                                    IOHIDDeviceRef          service,
                                    io_service_t            ioservice,
                                    natural_t               messageType,
                                    void *                  messageArgument );
static void             __IOHIDDeviceValueCallback(
                                    void *                  context, 
                                    IOReturn                result, 
                                    void *                  sender, 
                                    IOHIDValueRef           value);
static void             __IOHIDDeviceReportCallbackOnce(
                                    void *                  context, 
                                    IOReturn                result, 
                                    void *                  sender, 
                                    IOHIDReportType         type, 
                                    uint32_t                reportID, 
                                    uint8_t *               report, 
                                    CFIndex                 reportLength);
static void             __IOHIDDeviceReportCallbackRegistered(
                                    void *                  context, 
                                    IOReturn                result, 
                                    void *                  sender, 
                                    IOHIDReportType         type, 
                                    uint32_t                reportID, 
                                    uint8_t *               report, 
                                    CFIndex                 reportLength);
static void             __IOHIDDeviceTransactionCallback(
                                    void *                  context, 
                                    IOReturn                result, 
                                    void *                  sender);

//------------------------------------------------------------------------------
typedef struct __IOHIDDeviceCallbackInfo
{
    void *          context;
    void *          callback;
    IOHIDDeviceRef  device;
} IOHIDDeviceCallbackInfo;

typedef struct __IOHIDDeviceTransactionCallbackInfo
{
    void *          context;
    void *          callback;
    IOHIDDeviceRef  device;
    CFArrayRef      elements;
} IOHIDDeviceTransactionCallbackInfo;

typedef struct __IOHIDDeviceInputElementValueCallbackInfo {
    void *              context;
    IOHIDValueCallback  callback;
} IOHIDDeviceInputElementValueCallbackInfo;

typedef struct __IOHIDDeviceRemovalCallbackInfo {
    void *          context;
    IOHIDCallback   callback;
} IOHIDDeviceRemovalCallbackInfo;

typedef struct __IOHIDDeviceReportCallbackInfo
{
    void *              context;
    IOHIDReportCallback callback;
    IOHIDDeviceRef      device;
} IOHIDDeviceReportCallbackInfo;

typedef struct __IOHIDDevice
{
    CFRuntimeBase                   cfBase;   // base CFType information

    io_service_t                    service;
    IOHIDDeviceDeviceInterface**    deviceInterface;
    IOCFPlugInInterface **          plugInInterface;
    CFMutableDictionaryRef          properties;
    CFMutableSetRef                 elements;
    CFStringRef                     rootKey;
    CFStringRef                     UUIDKey;
    IONotificationPortRef           notificationPort;
    io_object_t                     notification;
    CFTypeRef                       asyncEventSource;
    CFRunLoopRef                    runLoop;
    CFStringRef                     runLoopMode;
    
    IOHIDQueueRef                   queue;
    CFArrayRef                      inputMatchingMultiple;
    Boolean                         loadProperties;
    Boolean                         isDirty;
    
    // For thread safety reasons, to add or remove an  element, first make a copy, 
    // then modify that copy, then replace the original
    CFMutableArrayRef               removalCallbackArray;
    CFMutableArrayRef               reportCallbackArray;
    CFMutableArrayRef               inputCallbackArray;
} __IOHIDDevice, *__IOHIDDeviceRef;

static const CFRuntimeClass __IOHIDDeviceClass = {
    0,                      // version
    "IOHIDDevice",          // className
    NULL,                   // init
    NULL,                   // copy
    __IOHIDDeviceRelease,   // finalize
    NULL,                   // equal
    NULL,                   // hash
    NULL,                   // copyFormattingDesc
    NULL,                   // copyDebugDesc
    NULL,                   // reclaim
    NULL
};

static pthread_once_t __deviceTypeInit = PTHREAD_ONCE_INIT;
static CFTypeID __kIOHIDDeviceTypeID = _kCFRuntimeNotATypeID;

//------------------------------------------------------------------------------
// __IOHIDDeviceRegister
//------------------------------------------------------------------------------
void __IOHIDDeviceRegister(void)
{
    __kIOHIDDeviceTypeID = _CFRuntimeRegisterClass(&__IOHIDDeviceClass);
}

//------------------------------------------------------------------------------
// __IOHIDDeviceCreate
//------------------------------------------------------------------------------
IOHIDDeviceRef __IOHIDDeviceCreate(   
                                CFAllocatorRef              allocator, 
                                CFAllocatorContext *        context __unused)
{
    IOHIDDeviceRef  device = NULL;
    void *          offset  = NULL;
    uint32_t        size;

    /* allocate service */
    size  = sizeof(__IOHIDDevice) - sizeof(CFRuntimeBase);
    device = (IOHIDDeviceRef)_CFRuntimeCreateInstance(allocator, IOHIDDeviceGetTypeID(), size, NULL);
    
    if (!device)
        return NULL;

    offset = device;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
    return device;
}

//------------------------------------------------------------------------------
// __IOHIDDeviceRelease
//------------------------------------------------------------------------------
void __IOHIDDeviceRelease( CFTypeRef object )
{
    IOHIDDeviceRef device = (IOHIDDeviceRef)object;
    
    if ( device->inputMatchingMultiple ) {
        CFRelease(device->inputMatchingMultiple);
        device->inputMatchingMultiple = NULL;
    }
    
    if ( device->queue ) {
        CFRelease(device->queue);
        device->queue = NULL;
    }
    
    CFRELEASE_IF_NOT_NULL(device->properties);
    CFRELEASE_IF_NOT_NULL(device->elements);
    CFRELEASE_IF_NOT_NULL(device->rootKey);
    
    CFRELEASE_IF_NOT_NULL(device->removalCallbackArray);
    CFRELEASE_IF_NOT_NULL(device->inputCallbackArray);
    CFRELEASE_IF_NOT_NULL(device->reportCallbackArray);
    
    if ( device->deviceInterface ) {
        (*device->deviceInterface)->Release(device->deviceInterface);
        device->deviceInterface = NULL;
    }

    if ( device->plugInInterface ) {
        IODestroyPlugInInterface(device->plugInInterface);
        device->plugInInterface = NULL;
    }
    
    if ( device->service ) {
        IOObjectRelease(device->service);
        device->service = 0;
    }
   
    if ( device->notification ) {
        IOObjectRelease(device->notification);
        device->notification = 0;
    }
    
    if (device->notificationPort) {
        IONotificationPortDestroy(device->notificationPort);
        device->notificationPort = NULL;
    }
}

//------------------------------------------------------------------------------
// __IOHIDDeviceNotification
//------------------------------------------------------------------------------
void __IOHIDDeviceNotification(             
                            IOHIDDeviceRef          device,
                            io_service_t            ioservice __unused,
                            natural_t               messageType,
                            void *                  messageArgument __unused)
{
    if ( !device || (messageType != kIOMessageServiceIsTerminated) || !device->removalCallbackArray )
        return;
    
    CFRetain(device);
    
    CFIndex index = 0;
    CFIndex count = CFArrayGetCount(device->removalCallbackArray);
    while ((index < count) && (count == CFArrayGetCount(device->removalCallbackArray))) {
        CFDataRef infoRef = (CFDataRef)CFArrayGetValueAtIndex(device->removalCallbackArray, index);
        IOHIDDeviceRemovalCallbackInfo *info = (IOHIDDeviceRemovalCallbackInfo *)CFDataGetBytePtr(infoRef);
        if (info->callback)
            info->callback(info->context, kIOReturnSuccess, device);
        index++;
    }
    
    if (count != CFArrayGetCount(device->removalCallbackArray))
        _IOHIDLog(ASL_LEVEL_ERR, "%s removal callbacks altered while in use by IOHIDDeviceRef %p\n", __func__, device);

    CFRelease(device);
}

//------------------------------------------------------------------------------
// _IOHIDDeviceGetIOCFPlugInInterface
//------------------------------------------------------------------------------
IOCFPlugInInterface ** _IOHIDDeviceGetIOCFPlugInInterface( 
                                IOHIDDeviceRef                  device)
{
    return device->plugInInterface;
}

//------------------------------------------------------------------------------
// IOHIDDeviceGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOHIDDeviceGetTypeID(void) 
{
    if ( _kCFRuntimeNotATypeID == __kIOHIDDeviceTypeID )
        pthread_once(&__deviceTypeInit, __IOHIDDeviceRegister);
        
    return __kIOHIDDeviceTypeID;
}

//------------------------------------------------------------------------------
// IOHIDDeviceCreate
//------------------------------------------------------------------------------
IOHIDDeviceRef IOHIDDeviceCreate(
                                CFAllocatorRef                  allocator, 
                                io_service_t                    service)
{
    IOCFPlugInInterface **          plugInInterface     = NULL;
    IOHIDDeviceDeviceInterface **   deviceInterface     = NULL;
    IOHIDDeviceRef                  device              = NULL;
    kern_return_t                   kr                  = kIOReturnSuccess;
    HRESULT                         result              = S_FALSE;
    SInt32                          score               = 0;

    kr = IOObjectRetain(service);
    
    if ( kr != kIOReturnSuccess )
        return NULL;
        
    kr = IOCreatePlugInInterfaceForService(service, kIOHIDDeviceTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
                
    if ( kr != kIOReturnSuccess ) 
	return NULL;

    result = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceDeviceInterfaceID), (LPVOID)&deviceInterface);
        
    if ( result != S_OK || !deviceInterface ) {
        IODestroyPlugInInterface(plugInInterface);
        return NULL;
    }                                                                                                                                                                                
    
    device = __IOHIDDeviceCreate(allocator, NULL);

    if ( !device ) {
        (*deviceInterface)->Release(deviceInterface);
        IODestroyPlugInInterface(plugInInterface);
        return NULL;
    }
        
    device->plugInInterface = plugInInterface;
    device->deviceInterface = deviceInterface;
    device->service         = service;

    return device;
}

//------------------------------------------------------------------------------
// IOHIDDeviceGetService
//------------------------------------------------------------------------------
io_service_t IOHIDDeviceGetService(
                                IOHIDDeviceRef                  device)
{
    return device->service;
}


//------------------------------------------------------------------------------
// IOHIDDeviceOpen
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceOpen(          
                                IOHIDDeviceRef                  device, 
                                IOOptionBits                    options)
{
    return (*device->deviceInterface)->open(device->deviceInterface, options);
}

//------------------------------------------------------------------------------
// IOHIDDeviceClose
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceClose(
                                IOHIDDeviceRef                  device, 
                                IOOptionBits                    options)
{
    CFRELEASE_IF_NOT_NULL(device->removalCallbackArray);
    CFRELEASE_IF_NOT_NULL(device->inputCallbackArray);
    CFRELEASE_IF_NOT_NULL(device->reportCallbackArray);

    return (*device->deviceInterface)->close(device->deviceInterface, options);
}

//------------------------------------------------------------------------------
// IOHIDDeviceConformsTo
//------------------------------------------------------------------------------
Boolean IOHIDDeviceConformsTo(          
                                IOHIDDeviceRef                  device, 
                                uint32_t                        usagePage,
                                uint32_t                        usage)
{
    Boolean                 doesConform = FALSE;
    CFMutableDictionaryRef  matching    = NULL;
    
    matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (matching) {
        CFArrayRef  elements    = NULL;
        CFNumberRef number      = NULL;
        uint32_t    value;
    
        value   = kIOHIDElementTypeCollection;
        number  = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
        CFDictionarySetValue(matching, CFSTR(kIOHIDElementTypeKey), number);
        CFRelease(number);
        
        value   = usagePage;
        number  = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
        CFDictionarySetValue(matching, CFSTR(kIOHIDElementUsagePageKey), number);
        CFRelease(number);

        value   = usage;
        number  = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
        CFDictionarySetValue(matching, CFSTR(kIOHIDElementUsageKey), number);
        CFRelease(number);
        
        elements = IOHIDDeviceCopyMatchingElements(device, matching, 0);
        if ( elements ) {
            CFIndex         count, index;
            IOHIDElementRef element;
            
            count = CFArrayGetCount(elements);
            for (index=0; index<count; index++) {
                element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, index);
                
                if ( IOHIDElementGetCollectionType(element) == kIOHIDElementCollectionTypePhysical ||
                     IOHIDElementGetCollectionType(element) == kIOHIDElementCollectionTypeApplication ) {
                     
                     doesConform = TRUE;
                     break;
                }
            
            }
            CFRelease(elements);
        }
        CFRelease(matching);
    }
    
    return doesConform;
}

//------------------------------------------------------------------------------
// IOHIDDeviceGetProperty
//------------------------------------------------------------------------------
CFTypeRef IOHIDDeviceGetProperty(
                                IOHIDDeviceRef                  device, 
                                CFStringRef                     key)
{
    CFTypeRef   property = NULL;
    IOReturn    ret;
    
    ret = (*device->deviceInterface)->getProperty(
                                            device->deviceInterface,
                                            key, 
                                            &property);
    
    if ((ret != kIOReturnSuccess) || !property) {
        if (device->properties) {
            property = CFDictionaryGetValue(device->properties, key);
        }
        else {
            property = NULL;
        }
    }

    return property;
}
                                
//------------------------------------------------------------------------------
// IOHIDDeviceSetProperty
//------------------------------------------------------------------------------
Boolean IOHIDDeviceSetProperty(
                                IOHIDDeviceRef                  device,
                                CFStringRef                     key,
                                CFTypeRef                       property)
{
    if (!device->properties) {
        device->properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 
                                                        0,
                                                        &kCFTypeDictionaryKeyCallBacks,
                                                        &kCFTypeDictionaryValueCallBacks);
        if (!device->properties)
            return FALSE;
    }
    
    device->isDirty = TRUE;
    CFDictionarySetValue(device->properties, key, property);

    // A device does not apply its properties to its elements.

    return (*device->deviceInterface)->setProperty(device->deviceInterface, 
                                                   key, 
                                                   property) == kIOReturnSuccess;
}

//------------------------------------------------------------------------------
// IOHIDDeviceCopyMatchingElements
//------------------------------------------------------------------------------
CFArrayRef IOHIDDeviceCopyMatchingElements(
                                IOHIDDeviceRef                  device, 
                                CFDictionaryRef                 matching, 
                                IOOptionBits                    options)
{
    CFArrayRef  elements = NULL;
    IOReturn    ret;
    
    ret = (*device->deviceInterface)->copyMatchingElements(
                                        device->deviceInterface, 
                                        matching,
                                        &elements,
                                        options);
                                        
    if ( ret != kIOReturnSuccess && elements ) {
        CFRelease(elements);
        elements = NULL;
    }
    
    if ( elements ) {
        CFIndex         count, index;
        IOHIDElementRef element;
        
        count = CFArrayGetCount(elements);
        for (index=0; index<count; index++) {
            element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, index);
            _IOHIDElementSetDevice(element, device);
            
            if (device->elements || 
                (device->elements = CFSetCreateMutable(kCFAllocatorDefault, 
                                                        0,
                                                        &kCFTypeSetCallBacks))) {
                if (!CFSetContainsValue(device->elements, element)) {
                    CFSetSetValue(device->elements, element);
                    if (device->loadProperties) {
                        __IOHIDElementLoadProperties(element);
                    }
                }
            }
        }
    }
    
    return elements;
}

//------------------------------------------------------------------------------
// IOHIDDeviceScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOHIDDeviceScheduleWithRunLoop(
                                IOHIDDeviceRef                  device, 
                                CFRunLoopRef                    runLoop, 
                                CFStringRef                     runLoopMode)
{
    device->runLoop     = runLoop;
    device->runLoopMode = runLoopMode;


    if ( !device->asyncEventSource) {
        IOReturn ret;
        
        ret = (*device->deviceInterface)->getAsyncEventSource(
                                                    device->deviceInterface,
                                                    &device->asyncEventSource);
        
        if (ret != kIOReturnSuccess || !device->asyncEventSource)
            return;
    }

    if (CFGetTypeID(device->asyncEventSource) == CFRunLoopSourceGetTypeID())
        CFRunLoopAddSource( device->runLoop, 
                            (CFRunLoopSourceRef)device->asyncEventSource, 
                            device->runLoopMode);
    else if (CFGetTypeID(device->asyncEventSource) == CFRunLoopTimerGetTypeID())
        CFRunLoopAddTimer(  device->runLoop, 
                            (CFRunLoopTimerRef)device->asyncEventSource, 
                            device->runLoopMode);

    if ( device->notificationPort )
        CFRunLoopAddSource(
                device->runLoop, 
                IONotificationPortGetRunLoopSource(device->notificationPort), 
                device->runLoopMode);
                
    // Default queue has already been created, so go ahead and schedule it
    if ( device->queue ) {
        IOHIDQueueScheduleWithRunLoop(  device->queue, 
                                        device->runLoop, 
                                        device->runLoopMode);
     
        IOHIDQueueStart(device->queue);
    }

}

//------------------------------------------------------------------------------
// IOHIDDeviceUnscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOHIDDeviceUnscheduleFromRunLoop(  
                                IOHIDDeviceRef                  device, 
                                CFRunLoopRef                    runLoop, 
                                CFStringRef                     runLoopMode)
{
    if ( CFEqual(device->runLoop, runLoop) && 
            CFEqual(device->runLoopMode, runLoopMode)) {
            
        if ( device->notificationPort ) {        
            CFRunLoopRemoveSource(
                device->runLoop, 
                IONotificationPortGetRunLoopSource(device->notificationPort), 
                device->runLoopMode);
        }
        
        if (device->asyncEventSource) {
            if (CFGetTypeID(device->asyncEventSource) == CFRunLoopSourceGetTypeID())
                CFRunLoopRemoveSource( device->runLoop, 
                                    (CFRunLoopSourceRef)device->asyncEventSource, 
                                    device->runLoopMode);
            else if (CFGetTypeID(device->asyncEventSource) == CFRunLoopTimerGetTypeID())
                CFRunLoopAddTimer(  device->runLoop, 
                                    (CFRunLoopTimerRef)device->asyncEventSource, 
                                    device->runLoopMode);
        
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDDeviceRegisterRemovalCallback
//------------------------------------------------------------------------------
void IOHIDDeviceRegisterRemovalCallback( 
                                IOHIDDeviceRef                  device, 
                                IOHIDCallback                   callback, 
                                void *                          context)
{    
    CFDataRef infoRef = NULL;
    CFRetain(device);   
    if (!context)
        _IOHIDLog(ASL_LEVEL_WARNING, "%s called with a null context\n", __func__);
    
    if (!device->removalCallbackArray) {
        device->removalCallbackArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    require(device->removalCallbackArray, cleanup);
    
    if (callback) {
        // adding a callback
        IOHIDDeviceRemovalCallbackInfo info = { context, callback };
        infoRef = CFDataCreate(NULL, (const UInt8 *) &info, sizeof(info));
        require(infoRef, cleanup);
        
        CFArrayAppendValue(device->removalCallbackArray, infoRef);
        
        if ( !device->notificationPort ) {
            kern_return_t kret = 0;
            device->notificationPort = IONotificationPortCreate(kIOMasterPortDefault);
            require(device->notificationPort, cleanup);
            require(device->service, cleanup);
            
            if ( device->runLoop ) {
                CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(device->notificationPort);
                if (source) {
                    CFRunLoopAddSource(device->runLoop, source, device->runLoopMode);
                }
            }
            
            kret = IOServiceAddInterestNotification(device->notificationPort,   // notifyPort
                                                    device->service,            // service
                                                    kIOGeneralInterest,         // interestType
                                                    (IOServiceInterestCallback)__IOHIDDeviceNotification, // callback
                                                    device,                     // refCon
                                                    &(device->notification)     // notification
                                                    );
            require_noerr(kret, cleanup);
        }
    }
    else {
        // removing a callback
        CFIndex index = 0;
        while (index < CFArrayGetCount(device->removalCallbackArray)) {
            CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(device->removalCallbackArray, index);
            IOHIDDeviceRemovalCallbackInfo *info = (IOHIDDeviceRemovalCallbackInfo *)CFDataGetBytePtr(data);
            if (info->context == context) {
                CFArrayRemoveValueAtIndex(device->removalCallbackArray, index);
            }
            else {
                index++;
            }
        }
    }
    
cleanup:
    CFRELEASE_IF_NOT_NULL(infoRef);
    CFRelease(device);
}

//******************************************************************************
// HID ELEMENT SUPPORT
//******************************************************************************

//------------------------------------------------------------------------------
// __IOHIDDeviceCopyMatchingInputElements
//------------------------------------------------------------------------------
CFArrayRef __IOHIDDeviceCopyMatchingInputElements(IOHIDDeviceRef device, CFArrayRef multiple)
{
    CFMutableArrayRef       inputElements   = NULL;
    CFArrayRef              elements        = NULL;
    CFIndex                 index, count;
    
    // Grab the matching multiple.  If one has not already been specified,
    // fallback to the default
    if ( multiple ) {
        CFRetain(multiple);
    } else {
        uint32_t inputTypes[] = { kIOHIDElementTypeInput_Misc, kIOHIDElementTypeInput_Button, kIOHIDElementTypeInput_Axis, kIOHIDElementTypeInput_ScanCodes};
        
        count = sizeof(inputTypes) / sizeof(uint32_t);
        
        CFDictionaryRef matching[count];
        
        bzero(matching, sizeof(CFDictionaryRef) * count);
        
        CFStringRef key = CFSTR(kIOHIDElementTypeKey);
        for ( index=0; index<count; index++ ) {            
            CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &inputTypes[index]);
            matching[index] = CFDictionaryCreate(kCFAllocatorDefault, 
                                                 (const void **)&key,
                                                 (const void **)&number,
                                                 1, 
                                                 &kCFCopyStringDictionaryKeyCallBacks, 
                                                 &kCFTypeDictionaryValueCallBacks);
            CFRelease(number);
        }
        
        multiple = CFArrayCreate(kCFAllocatorDefault, (const void **)matching, count, &kCFTypeArrayCallBacks);
        
        for ( index=0; index<count; index++ )
            CFRelease(matching[index]);
    }
    
    if ( !multiple )
        return NULL;
        
    count = CFArrayGetCount( multiple );
   
    for ( index=0; index<count; index++) {
        elements = IOHIDDeviceCopyMatchingElements( device,
                                                    CFArrayGetValueAtIndex(multiple, index),
                                                    0);
        if ( !elements ) 
            continue;
            
        if ( !inputElements )
            inputElements = CFArrayCreateMutableCopy(   kCFAllocatorDefault, 
                                                        0, 
                                                        elements);
        else
            CFArrayAppendArray( inputElements, 
                                elements, 
                                CFRangeMake(0, CFArrayGetCount(elements)));
     
        CFRelease(elements);
    }
   
    CFRelease(multiple);
    
    return inputElements;
}

//------------------------------------------------------------------------------
// __IOHIDDeviceRegisterMatchingInputElements
//------------------------------------------------------------------------------
void __IOHIDDeviceRegisterMatchingInputElements(
                                IOHIDDeviceRef                  device, 
                                IOHIDQueueRef                   queue,
                                CFArrayRef                      mutlipleMatch)
{
    CFArrayRef elements = __IOHIDDeviceCopyMatchingInputElements(device, mutlipleMatch);
    
    if ( !elements )
        return;
        
    CFIndex         i, count;
    IOHIDElementRef element;
    
    count = CFArrayGetCount(elements);
    
    for (i=0; i<count; i++) {
        element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
        
        if ( !element || 
            (IOHIDElementGetReportSize(element) > (sizeof(CFIndex) << 3)))
            continue;
                     
        IOHIDQueueAddElement(queue, element);
    }
    
    CFRelease(elements);
}

//------------------------------------------------------------------------------
// IOHIDDeviceRegisterInputValueCallback
//------------------------------------------------------------------------------
void IOHIDDeviceRegisterInputValueCallback(
                                IOHIDDeviceRef                  device, 
                                IOHIDValueCallback              callback, 
                                void *                          context)
{
    CFDataRef infoRef = NULL;
    CFRetain(device);
    if (!context)
        _IOHIDLog(ASL_LEVEL_WARNING, "%s called with a null context\n", __func__);
    
    if (!device->inputCallbackArray) {
        device->inputCallbackArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    require(device->inputCallbackArray, cleanup);

    if (callback) {
        // adding a callback
        if ( !device->queue ) {
            device->queue = IOHIDQueueCreate(kCFAllocatorDefault, device, 20, 0);
            require(device->queue, cleanup);
            
            __IOHIDDeviceRegisterMatchingInputElements(device, device->queue, device->inputMatchingMultiple);
            
            // If a run loop has been already set, go ahead and schedule the queues
            if ( device->runLoop ) {
                IOHIDQueueScheduleWithRunLoop(  device->queue, 
                                              device->runLoop, 
                                              device->runLoopMode);
                
                IOHIDQueueStart(device->queue);
            }
            
        }
        
        IOHIDDeviceInputElementValueCallbackInfo info = { context, callback };
        infoRef = CFDataCreate(NULL, (const UInt8 *) &info, sizeof(info));
        require(infoRef, cleanup);
        
        CFArrayAppendValue(device->inputCallbackArray, infoRef);
    }
    else {
        // removing a callback
        CFIndex index = 0;
        while (index < CFArrayGetCount(device->inputCallbackArray)) {
            CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(device->inputCallbackArray, index);
            const IOHIDDeviceInputElementValueCallbackInfo *info = 
                    (const IOHIDDeviceInputElementValueCallbackInfo *)CFDataGetBytePtr(data);
            if (info->context == context) {
                CFArrayRemoveValueAtIndex(device->inputCallbackArray, index);
            }
            else {
                index++;
            }
        }
    }
    
    if (device && device->queue)
        IOHIDQueueRegisterValueAvailableCallback(device->queue, 
                                                 __IOHIDDeviceInputElementValueCallback, 
                                                 device);

cleanup:
    CFRELEASE_IF_NOT_NULL(infoRef);
    CFRelease(device);
}

//------------------------------------------------------------------------------
// IOHIDDeviceSetInputValueMatching
//------------------------------------------------------------------------------
void IOHIDDeviceSetInputValueMatching(
                                IOHIDDeviceRef                  device, 
                                CFDictionaryRef                 matching)
{
    if ( matching ) {
        CFArrayRef multiple = CFArrayCreate(kCFAllocatorDefault, (const void **)&matching, 1, &kCFTypeArrayCallBacks);
        
        IOHIDDeviceSetInputValueMatchingMultiple(device, multiple);
        
        CFRelease(multiple);
    } else {
        IOHIDDeviceSetInputValueMatchingMultiple(device, NULL);
    }
}

//------------------------------------------------------------------------------
// IOHIDDeviceSetInputValueMatchingMultiple
//------------------------------------------------------------------------------
void IOHIDDeviceSetInputValueMatchingMultiple(
                                IOHIDDeviceRef                  device, 
                                CFArrayRef                      multiple)
{
    if ( device->queue ) {
        IOHIDValueRef   value;
        CFArrayRef      elements;
        
        // stop the queue
        IOHIDQueueStop(device->queue);
        
        // drain the queue
        while ( (value = IOHIDQueueCopyNextValue(device->queue)) )
            CFRelease(value);

        // clear the exising elements from the queue            
        elements = _IOHIDQueueCopyElements(device->queue);
        if ( elements ) {
            CFIndex index, count;
        
            count = CFArrayGetCount(elements);
            for (index=0; index<count; index++)
                IOHIDQueueRemoveElement(device->queue, (IOHIDElementRef)CFArrayGetValueAtIndex(elements, index));
                
            CFRelease(elements);
        }
        
        // reload
        __IOHIDDeviceRegisterMatchingInputElements(device, device->queue, multiple);
        
        // start it back up
        IOHIDQueueStart(device->queue);
    }

    if ( device->inputMatchingMultiple ) 
        CFRelease(device->inputMatchingMultiple);
    
    device->inputMatchingMultiple = multiple ? (CFArrayRef)CFRetain(multiple) : NULL;
    
}

//------------------------------------------------------------------------------
// IOHIDDeviceSetValue
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceSetValue(
                                IOHIDDeviceRef                  device, 
                                IOHIDElementRef                 element, 
                                IOHIDValueRef                   value)
{
    return (*device->deviceInterface)->setValue(
                                                device->deviceInterface,
                                                element,
                                                value,
                                                0,
                                                NULL,
                                                NULL,
                                                0);
}

//------------------------------------------------------------------------------
// IOHIDDeviceSetValueMultiple
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceSetValueMultiple(
                                IOHIDDeviceRef                  device, 
                                CFDictionaryRef                 multiple)
{
    return IOHIDDeviceSetValueMultipleWithCallback(device, multiple, 0, NULL, NULL);
}

//------------------------------------------------------------------------------
// IOHIDDeviceSetValueWithCallback
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceSetValueWithCallback(
                                IOHIDDeviceRef                  device, 
                                IOHIDElementRef                 element, 
                                IOHIDValueRef                   value, 
                                CFTimeInterval                  timeout,
                                IOHIDValueCallback              callback, 
                                void *                          context)
{
    IOHIDDeviceCallbackInfo * elementInfo = 
            (IOHIDDeviceCallbackInfo *)malloc(sizeof(IOHIDDeviceCallbackInfo));
    
    if ( !elementInfo )
        return kIOReturnNoMemory;

    elementInfo->device     = device;
    elementInfo->callback   = callback;
    elementInfo->context    = context;

    uint32_t timeoutMS = timeout * 1000;
    
    IOReturn ret = (*device->deviceInterface)->setValue(
                                                device->deviceInterface,
                                                element,
                                                value,
                                                timeoutMS,
                                                __IOHIDDeviceValueCallback,
                                                elementInfo,
                                                0);
                                                
    if ( ret != kIOReturnSuccess )
        free(elementInfo);
    
    return ret;
}

//------------------------------------------------------------------------------
// IOHIDDeviceSetValueMultipleWithCallback
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceSetValueMultipleWithCallback(
                                IOHIDDeviceRef                  device, 
                                CFDictionaryRef                 multiple,
                                CFTimeInterval                  timeout,
                                IOHIDValueMultipleCallback      callback, 
                                void *                          context)
{
    IOReturn            ret             = kIOReturnError;
    IOHIDElementRef *   elements        = NULL;
    IOHIDValueRef *     values          = NULL;
    IOHIDTransactionRef transaction     = NULL;
    CFIndex             index, count;
    
    
    if ( !multiple || ((count = CFDictionaryGetCount(multiple)) == 0))
        return kIOReturnBadArgument;
           
    do {
        transaction = IOHIDTransactionCreate(   kCFAllocatorDefault, 
                                                device,
                                                kIOHIDTransactionDirectionTypeOutput,
                                                0);
        if ( !transaction ) {
            ret = kIOReturnNoMemory;
            break;
         }

        elements = (IOHIDElementRef *)malloc(count * sizeof(IOHIDElementRef));
        if ( !elements ) { 
            ret = kIOReturnNoMemory;
            break;
        }
            
        values  = (IOHIDValueRef *)malloc(count * sizeof(IOHIDValueRef));
        if ( !values ) { 
            ret = kIOReturnNoMemory;
            break;
        }
        
        CFDictionaryGetKeysAndValues(multiple, (const void **)elements, (const void **)values);
        
        for ( index=0; index<count; index++ ) {
            IOHIDTransactionAddElement(transaction, elements[index]);
            IOHIDTransactionSetValue(transaction, elements[index], values[index], 0);
        }

        if ( callback ) {  // Async
            IOHIDDeviceTransactionCallbackInfo * elementInfo = 
                    (IOHIDDeviceTransactionCallbackInfo *)malloc(sizeof(IOHIDDeviceTransactionCallbackInfo));
            
            if ( !elementInfo ) { 
                ret = kIOReturnNoMemory;
                break;
            }

            elementInfo->device     = device;
            elementInfo->callback   = callback;
            elementInfo->context    = context;
            elementInfo->elements   = CFArrayCreate(kCFAllocatorDefault, (const void **)elements, count, &kCFTypeArrayCallBacks);
        
            ret = IOHIDTransactionCommitWithCallback(transaction, timeout, __IOHIDDeviceTransactionCallback, elementInfo);
            
            if ( ret != kIOReturnSuccess )
                free(elementInfo);
                
        } else { // Sync
            ret = IOHIDTransactionCommit(transaction);
        }
        
    } while ( FALSE );
    
    if ( transaction )
        CFRelease(transaction);
        
    if ( elements )
        free(elements);

    if ( values )
        free(values);
    
    return ret;
}

//------------------------------------------------------------------------------
// IOHIDDeviceGetValue
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceGetValue(
                                IOHIDDeviceRef                  device, 
                                IOHIDElementRef                 element, 
                                IOHIDValueRef *                 pValue)
{
    return (*device->deviceInterface)->getValue(
                                                device->deviceInterface,
                                                element,
                                                pValue,
                                                0,
                                                NULL,
                                                NULL,
                                                0);
}

//------------------------------------------------------------------------------
// IOHIDDeviceCopyValueMultiple
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceCopyValueMultiple(
                                IOHIDDeviceRef                  device, 
                                CFArrayRef                      elements, 
                                CFDictionaryRef *               pMultiple)
{
    return IOHIDDeviceCopyValueMultipleWithCallback(device,
                                                    elements,
                                                    pMultiple,
                                                    0,
                                                    NULL,
                                                    NULL);
}

//------------------------------------------------------------------------------
// IOHIDDeviceGetValueWithCallback
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceGetValueWithCallback(
                                IOHIDDeviceRef                  device, 
                                IOHIDElementRef                 element, 
                                IOHIDValueRef *                 pValue,
                                CFTimeInterval                  timeout,
                                IOHIDValueCallback              callback, 
                                void *                          context)
{
    IOHIDDeviceCallbackInfo * elementInfo = 
            (IOHIDDeviceCallbackInfo *)malloc(sizeof(IOHIDDeviceCallbackInfo));
    
    if ( !elementInfo )
        return kIOReturnNoMemory;

    elementInfo->device     = device;
    elementInfo->callback   = callback;
    elementInfo->context    = context;

    uint32_t timeoutMS = timeout * 1000;
    
    IOReturn ret = (*device->deviceInterface)->getValue(
                                                device->deviceInterface,
                                                element,
                                                pValue,
                                                timeoutMS,
                                                __IOHIDDeviceValueCallback,
                                                elementInfo,
                                                0);
                                                
    if ( ret != kIOReturnSuccess )
        free(elementInfo);
        
    return ret;
}

//------------------------------------------------------------------------------
// IOHIDDeviceCopyValueMultipleWithCallback
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceCopyValueMultipleWithCallback(
                                IOHIDDeviceRef                  device, 
                                CFArrayRef                      elements, 
                                CFDictionaryRef *               pMultiple,
                                CFTimeInterval                  timeout,
                                IOHIDValueMultipleCallback      callback, 
                                void *                          context)
{
    IOReturn            ret             = kIOReturnError;
    IOHIDTransactionRef transaction     = NULL;
    IOHIDValueRef       value           = NULL;
    CFIndex             index, count;
    
    
    if ( !pMultiple || !elements || ((count = CFArrayGetCount(elements)) == 0))
        return kIOReturnBadArgument;
           
    do {
        transaction = IOHIDTransactionCreate(   kCFAllocatorDefault, 
                                                device,
                                                kIOHIDTransactionDirectionTypeInput,
                                                0);
        if ( !transaction ) {
            ret = kIOReturnNoMemory;
            break;
         }

        // Add the elements to the transaction
        for ( index=0; index<count; index++ )
            IOHIDTransactionAddElement(transaction, (IOHIDElementRef)CFArrayGetValueAtIndex(elements, index));

        if ( callback ) {  // Async
            IOHIDDeviceTransactionCallbackInfo * elementInfo = 
                    (IOHIDDeviceTransactionCallbackInfo *)malloc(sizeof(IOHIDDeviceTransactionCallbackInfo));
            
            if ( !elementInfo ) { 
                ret = kIOReturnNoMemory;
                break;
            }

            elementInfo->device     = device;
            elementInfo->callback   = callback;
            elementInfo->context    = context;
            elementInfo->elements   = CFArrayCreateCopy(kCFAllocatorDefault, elements);
        
            ret = IOHIDTransactionCommitWithCallback(transaction, timeout, __IOHIDDeviceTransactionCallback, elementInfo);
            
            if ( ret != kIOReturnSuccess )
                free(elementInfo);
                
        } else { // Sync
        
            ret = IOHIDTransactionCommit(transaction);
            
            if ( ret != kIOReturnSuccess )
                break;
                
            CFMutableDictionaryRef multiple = CFDictionaryCreateMutable(
                                                    kCFAllocatorDefault, 
                                                    count, 
                                                    &kCFCopyStringDictionaryKeyCallBacks, 
                                                    &kCFTypeDictionaryValueCallBacks);
            
            if ( !multiple ) {
                ret = kIOReturnNoMemory;
                break;
            }
            
            for ( index = 0; index<count; index++ ) {
                IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, index);
                value = IOHIDTransactionGetValue(transaction, element, 0);
                if ( !value )
                    continue;
                     
                CFDictionarySetValue(multiple, element, value); 
            }
            
            if ( CFDictionaryGetCount(multiple) == 0 ) {
                CFRelease(multiple);
                multiple = NULL;
            }
            
            *pMultiple = multiple;
        }
        
    } while ( FALSE );
    
    if ( transaction )
        CFRelease(transaction);
                
    return ret;
}

//------------------------------------------------------------------------------
// __IOHIDDeviceValueCallback
//------------------------------------------------------------------------------
void __IOHIDDeviceValueCallback(
                                    void *                  context, 
                                    IOReturn                result, 
                                    void *                  sender __unused, 
                                    IOHIDValueRef           value)
{
    IOHIDDeviceCallbackInfo *   elementInfo = (IOHIDDeviceCallbackInfo *)context;
    IOHIDDeviceRef              device      = elementInfo->device;
    
    if ( elementInfo->callback ) {
        IOHIDValueCallback callback = elementInfo->callback;
        
        (*callback)(elementInfo->context,
                    result, 
                    device,
                    value);
    }
    
    free(elementInfo);
}

//------------------------------------------------------------------------------
// __IOHIDDeviceInputElementValueCallback
//------------------------------------------------------------------------------
void __IOHIDDeviceInputElementValueCallback(
                                    void *                  context,
                                    IOReturn                result, 
                                    void *                  sender)
{
    IOHIDDeviceRef  device  = (IOHIDDeviceRef)context;
    IOHIDQueueRef   queue   = (IOHIDQueueRef)sender;
    IOHIDValueRef   value   = NULL;
    
    if ( (queue != device->queue) || (kIOReturnSuccess != result) )
        return;
    
    CFRetain(device);

    // Drain the queue and dispatch the values
    while ( (value = IOHIDQueueCopyNextValue(queue)) ) {
        if ( device->inputCallbackArray ) {
            CFIndex index = 0;
            CFIndex count = CFArrayGetCount(device->inputCallbackArray);
            while ((index < count) && (count == CFArrayGetCount(device->inputCallbackArray))) {
                CFDataRef infoRef = (CFDataRef)CFArrayGetValueAtIndex(device->inputCallbackArray, index);
                IOHIDDeviceInputElementValueCallbackInfo *info = (IOHIDDeviceInputElementValueCallbackInfo *)CFDataGetBytePtr(infoRef);
                if (info->callback)
                    info->callback(info->context, kIOReturnSuccess, sender, value);
                index++;
            }
            
            if (count != CFArrayGetCount(device->inputCallbackArray))
                _IOHIDLog(ASL_LEVEL_ERR, "%s input value callbacks altered while in use for IOHIDDeviceRef %p\n", __func__, device);
        }
        CFRelease(value);
    }

    CFRelease(device);
}

//------------------------------------------------------------------------------
// __IOHIDDeviceTransactionCallback
//------------------------------------------------------------------------------
void __IOHIDDeviceTransactionCallback(
                                    void *                  context,
                                    IOReturn                result, 
                                    void *                  sender)
{
    IOHIDTransactionRef                 transaction     = (IOHIDTransactionRef)sender;
    IOHIDDeviceTransactionCallbackInfo  *elementInfo    = (IOHIDDeviceTransactionCallbackInfo *)context;
    IOHIDDeviceRef                      device          = elementInfo->device;
    IOHIDValueMultipleCallback          callback        = elementInfo->callback;
    CFMutableDictionaryRef              values          = NULL;
    
    do {
        if ( !transaction )
            break;
            
        if ( !callback )
            break;
            
        if ( result == kIOReturnSuccess ) {
            
            if ( !elementInfo->elements )
                break;
                
            CFIndex         count = CFArrayGetCount(elementInfo->elements);
            CFIndex         index = 0;
            IOHIDElementRef element = NULL;
            IOHIDValueRef   value = NULL;
            
            if ( count )
                values = CFDictionaryCreateMutable(
                                        kCFAllocatorDefault, 
                                        count, 
                                        &kCFCopyStringDictionaryKeyCallBacks, 
                                        &kCFTypeDictionaryValueCallBacks);
            
            if ( !values )
                break;
            
            for ( index = 0; index<count; index++ ) {
                element = (IOHIDElementRef)CFArrayGetValueAtIndex(elementInfo->elements, index);
                if ( !element )
                    continue;

                /* FIX ME: provide an option to get transaction value before it is cleared */
                value = IOHIDTransactionGetValue(transaction, element, 0);
                if ( !value )
                    continue;
                    
                CFDictionarySetValue(values, element, value); 
            }

            if ( CFDictionaryGetCount(values) == 0 ) {
                CFRelease(values);
                values = NULL;
            }
        }

        (*callback)(elementInfo->context, result, device, values);
            
    } while ( FALSE );
    
    if ( values )
        CFRelease(values);

    if ( elementInfo->elements )
        CFRelease(elementInfo->elements);
        
    free(elementInfo);
}

//******************************************************************************
// HID REPORT SUPPORT
//******************************************************************************
//------------------------------------------------------------------------------
// __IOHIDDeviceReportCallbackOnce
//------------------------------------------------------------------------------
void __IOHIDDeviceReportCallbackOnce(void *                  context, 
                                     IOReturn                result, 
                                     void *                  sender, 
                                     IOHIDReportType         type, 
                                     uint32_t                reportID, 
                                     uint8_t *               report, 
                                     CFIndex                 reportLength)
{
    IOHIDDeviceReportCallbackInfo   *info   = (IOHIDDeviceReportCallbackInfo *)context;
    IOHIDDeviceRef                  device      = info->device;
    
    CFRetain(device);
    if ( info->callback && sender == device->deviceInterface ) {
            info->callback(info->context, 
                           result, 
                           device,
                           type,
                           reportID,
                           report,
                           reportLength);
    }
    
    free(context);
    CFRelease(device);
}

//------------------------------------------------------------------------------
// __IOHIDDeviceReportCallbackRegistered
//------------------------------------------------------------------------------
void __IOHIDDeviceReportCallbackRegistered(void *                  context, 
                                           IOReturn                result, 
                                           void *                  sender __unused, 
                                           IOHIDReportType         type, 
                                           uint32_t                reportID, 
                                           uint8_t *               report, 
                                           CFIndex                 reportLength)
{
    CFDataRef                       infoRef = (CFDataRef)context;
    IOHIDDeviceReportCallbackInfo   *info   = (IOHIDDeviceReportCallbackInfo *)CFDataGetBytePtr(infoRef);
    IOHIDDeviceRef                  device  = info->device;
    
    if (!device || !device->reportCallbackArray)
        return;
    
    CFRetain(device);

    CFIndex index = 0;
    CFIndex count = CFArrayGetCount(device->reportCallbackArray);
    while ((index < count) && (count == CFArrayGetCount(device->reportCallbackArray))) {
        infoRef = (CFDataRef)CFArrayGetValueAtIndex(device->reportCallbackArray, index);
        info = (IOHIDDeviceReportCallbackInfo *)CFDataGetBytePtr(infoRef);
        if (info->callback)
            info->callback(info->context, result, device, type, reportID, report, reportLength);
        index++;
    }
    
    if (count != CFArrayGetCount(device->reportCallbackArray))
        _IOHIDLog(ASL_LEVEL_ERR, "%s report callbacks altered while in use for IOHIDDeviceRef %p\n", __func__, device);

    CFRelease(device);
}

//------------------------------------------------------------------------------
// IOHIDDeviceRegisterInputReportCallback
//------------------------------------------------------------------------------
void IOHIDDeviceRegisterInputReportCallback( 
                                            IOHIDDeviceRef                  device, 
                                            uint8_t *                       report, 
                                            CFIndex                         reportLength,
                                            IOHIDReportCallback             callback, 
                                            void *                          context)
{    
    CFDataRef infoRef = NULL;
    CFRetain(device);   
    if (!context)
        _IOHIDLog(ASL_LEVEL_WARNING, "%s called with a null context\n", __func__);
    
    if (!device->reportCallbackArray) {
        device->reportCallbackArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    require(device->reportCallbackArray, cleanup);
    
    if (callback) {
        // adding or modifying a callback
        IOHIDDeviceReportCallbackInfo info = {
            context,
            callback,
            device
        };
        infoRef = CFDataCreate(NULL, (const UInt8 *) &info, sizeof(info));
        require(infoRef, cleanup);
        
        CFArrayAppendValue(device->reportCallbackArray, infoRef);
        
        (*device->deviceInterface)->setInputReportCallback(device->deviceInterface, 
                                                           report,
                                                           reportLength,
                                                           __IOHIDDeviceReportCallbackRegistered,
                                                           (void *) infoRef,
                                                           0);
    }
    else {
        // removing a callback
        CFIndex index = 0;
        while (index < CFArrayGetCount(device->reportCallbackArray)) {
            CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(device->reportCallbackArray, index);
            const IOHIDDeviceInputElementValueCallbackInfo *info = 
                    (const IOHIDDeviceInputElementValueCallbackInfo *)CFDataGetBytePtr(data);
            if (info->context == context) {
                CFArrayRemoveValueAtIndex(device->reportCallbackArray, index);
            }
            else {
                index++;
            }
        }
    }
    
cleanup:
    CFRELEASE_IF_NOT_NULL(infoRef);
    CFRelease(device);
}

//------------------------------------------------------------------------------
// IOHIDDeviceSetReport
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceSetReport(
                                IOHIDDeviceRef                  device,
                                IOHIDReportType                 reportType,
                                CFIndex                         reportID,
                                const uint8_t *                 report,
                                CFIndex                         reportLength)
{
    return (*device->deviceInterface)->setReport(
                                                device->deviceInterface,
                                                reportType,
                                                reportID,
                                                report,
                                                reportLength,
                                                0,
                                                NULL,
                                                NULL,
                                                0);
}
                                
//------------------------------------------------------------------------------
// IOHIDDeviceSetReportWithCallback
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceSetReportWithCallback(
                                IOHIDDeviceRef                  device,
                                IOHIDReportType                 reportType,
                                CFIndex                         reportID,
                                const uint8_t *                 report,
                                CFIndex                         reportLength,
                                CFTimeInterval                  timeout,
                                IOHIDReportCallback             callback,
                                void *                          context)
{
    IOHIDDeviceReportCallbackInfo *info_ptr = (IOHIDDeviceReportCallbackInfo*)malloc(sizeof(IOHIDDeviceReportCallbackInfo));

    if ( !info_ptr )
        return kIOReturnNoMemory;

    info_ptr->context = context;
    info_ptr->callback = callback;
    info_ptr->device = device;
    
    uint32_t timeoutMS = timeout * 1000;
    
    IOReturn result = (*device->deviceInterface)->setReport(device->deviceInterface,
                                                             reportType,
                                                             reportID,
                                                             report,
                                                             reportLength,
                                                             timeoutMS,
                                                             __IOHIDDeviceReportCallbackOnce,
                                                             info_ptr,
                                                             0);
    if (result)
        free(info_ptr);
    return result;
}

//------------------------------------------------------------------------------
// IOHIDDeviceGetReport
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceGetReport(
                                IOHIDDeviceRef                  device,
                                IOHIDReportType                 reportType,
                                CFIndex                         reportID,
                                uint8_t *                       report,
                                CFIndex *                       pReportLength)
{
    return (*device->deviceInterface)->getReport(
                                                device->deviceInterface,
                                                reportType,
                                                reportID,
                                                report,
                                                pReportLength,
                                                0,
                                                NULL,
                                                NULL,
                                                0);
}

//------------------------------------------------------------------------------
// IOHIDDeviceGetReportWithCallback
//------------------------------------------------------------------------------
IOReturn IOHIDDeviceGetReportWithCallback(
                                IOHIDDeviceRef                  device,
                                IOHIDReportType                 reportType,
                                CFIndex                         reportID,
                                uint8_t *                       report,
                                CFIndex *                       pReportLength,
                                CFTimeInterval                  timeout,
                                IOHIDReportCallback             callback,
                                void *                          context)
{
    IOHIDDeviceReportCallbackInfo info = {
        context,
        callback,
        device
    };
    void *info_ptr = malloc(sizeof(info));
    
    if ( !info_ptr )
        return kIOReturnNoMemory;
    
    memcpy(info_ptr, &info, sizeof(info));
    
    uint32_t timeoutMS = timeout * 1000;
    
    IOReturn result = (*device->deviceInterface)->getReport(device->deviceInterface,
                                                            reportType,
                                                            reportID,
                                                            report,
                                                            pReportLength,
                                                            timeoutMS,
                                                            __IOHIDDeviceReportCallbackOnce,
                                                            (void *) info_ptr,
                                                            0);
    if (result)
        free(info_ptr);
    return result;
}

//------------------------------------------------------------------------------
CFStringRef __IOHIDDeviceGetRootKey(IOHIDDeviceRef device) 
{
    if (!device->rootKey) {
        // Device Root Key
        // All *required* matching information
        CFStringRef manager = __IOHIDManagerGetRootKey();
        CFStringRef transport = (CFStringRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDTransportKey));
        CFNumberRef vendor = (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
        CFNumberRef product = (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
        CFStringRef serial = (CFStringRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDSerialNumberKey));
        if (!transport || (CFGetTypeID(transport) != CFStringGetTypeID())) {
            transport = CFSTR("unknown");
        }
        if (!vendor || (CFGetTypeID(vendor) != CFNumberGetTypeID())) {
            vendor = kCFNumberNaN;
        }
        if (!product || (CFGetTypeID(product) != CFNumberGetTypeID())) {
            product = kCFNumberNaN;
        }
        if (!serial || (CFGetTypeID(serial) != CFStringGetTypeID())) {
            serial = CFSTR("none");
        }
        
        device->rootKey = CFStringCreateWithFormat(NULL, NULL, 
                                                   CFSTR("%@#%@#%@#%@#%@"), 
                                                   manager,
                                                   transport,
                                                   vendor,
                                                   product,
                                                   serial);
    }
    
    return device->rootKey;
}

//------------------------------------------------------------------------------
CFStringRef __IOHIDDeviceGetUUIDString(IOHIDDeviceRef device)
{
    CFStringRef uuidStr = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDManagerUUIDKey));
    if (!uuidStr || (CFGetTypeID(uuidStr) != CFStringGetTypeID())) {
        CFUUIDRef uuid = CFUUIDCreate(NULL);
        uuidStr = CFUUIDCreateString(NULL, uuid);
        IOHIDDeviceSetProperty(device, CFSTR(kIOHIDManagerUUIDKey), uuidStr);
        CFRelease(uuid);
        CFRelease(uuidStr);
    }
    return uuidStr;
}

//------------------------------------------------------------------------------
CFStringRef __IOHIDDeviceGetUUIDKey(IOHIDDeviceRef device) 
{
    if (!device->UUIDKey) {
        CFStringRef manager = __IOHIDManagerGetRootKey();
        CFStringRef uuidStr = __IOHIDDeviceGetUUIDString(device);

        device->UUIDKey = CFStringCreateWithFormat(NULL, NULL, 
                                                   CFSTR("%@#%@"), 
                                                   manager,
                                                   uuidStr);
    }
    
    return device->UUIDKey;
}

//------------------------------------------------------------------------------
void __IOHIDDeviceSaveProperties(IOHIDDeviceRef device, __IOHIDPropertyContext *context)
{
    if (device->isDirty && device->properties) {
        CFStringRef uuidStr = __IOHIDDeviceGetUUIDString(device);
        CFArrayRef uuids = (CFArrayRef)CFPreferencesCopyAppValue(__IOHIDDeviceGetRootKey(device), kCFPreferencesCurrentApplication);
        CFArrayRef uuidsToWrite = NULL;
        if (uuids && (CFGetTypeID(uuids) == CFArrayGetTypeID())) {
            CFRange range = { 0, CFArrayGetCount(uuids) };
            if (!CFArrayContainsValue(uuids, range, uuidStr)) {
                uuidsToWrite = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(uuids) + 1, uuids);
                CFArrayAppendValue((CFMutableArrayRef)uuidsToWrite, uuidStr);
            }
        }
        else {
            uuidsToWrite = CFArrayCreate(NULL, (const void**)&uuidStr, 1, &kCFTypeArrayCallBacks);
        }
        if (uuidsToWrite) {
            __IOHIDPropertySaveWithContext(__IOHIDDeviceGetRootKey(device), uuidsToWrite, context);
        }
        
        CFRELEASE_IF_NOT_NULL(uuids);
        CFRELEASE_IF_NOT_NULL(uuidsToWrite);
        
        __IOHIDPropertySaveToKeyWithSpecialKeys(device->properties, 
                                               __IOHIDDeviceGetUUIDKey(device), 
                                               NULL, 
                                               context);
        device->isDirty = FALSE;
    }

    if (device->elements)
        CFSetApplyFunction(device->elements, __IOHIDSaveElementSet, context);
}

//------------------------------------------------------------------------------
void __IOHIDDeviceLoadProperties(IOHIDDeviceRef device)
{
    CFStringRef uuidStr = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDManagerUUIDKey));
    device->loadProperties = TRUE;
    
    // Have we already generated a key?
    if (!device->UUIDKey) {
        // Is there a UUID in the device properties?
        if (!uuidStr || (CFGetTypeID(uuidStr) != CFStringGetTypeID())) {
            // Are there any UUIDs for this device?
            CFArrayRef uuids = (CFArrayRef)CFPreferencesCopyAppValue(__IOHIDDeviceGetRootKey(device), kCFPreferencesCurrentApplication);
            if (uuids && (CFGetTypeID(uuids) == CFArrayGetTypeID()) && CFArrayGetCount(uuids)) {
                // VTN3 ¥ TODO: Add optional matching based on location ID and anything else you can think of
                uuidStr = (CFStringRef)CFArrayGetValueAtIndex(uuids, 0);
            }
        }
        if (!uuidStr || (CFGetTypeID(uuidStr) != CFStringGetTypeID())) {
            CFUUIDRef uuid = CFUUIDCreate(NULL);
            uuidStr = CFUUIDCreateString(NULL, uuid);
            CFRelease(uuid);
        }
        IOHIDDeviceSetProperty(device, CFSTR(kIOHIDManagerUUIDKey), uuidStr);
    }
    
    // Convert to __IOHIDPropertyLoadFromKeyWithSpecialKeys if we identify special keys
    CFMutableDictionaryRef properties = __IOHIDPropertyLoadDictionaryFromKey(__IOHIDDeviceGetUUIDKey(device));
    
    if (properties) {
        CFRELEASE_IF_NOT_NULL(device->properties);
        device->properties = properties;
        device->isDirty = FALSE;
    }
    
    IOHIDDeviceSetProperty(device, CFSTR(kIOHIDManagerUUIDKey), uuidStr);
    
    // A device does not apply its properties to its elements.
    if (device->elements)
        CFSetApplyFunction(device->elements, __IOHIDLoadElementSet, NULL);
}

//------------------------------------------------------------------------------
void __IOHIDApplyPropertyToDeviceSet(const void *value, void *context) {
    IOHIDDeviceRef device = (IOHIDDeviceRef)value;
    __IOHIDApplyPropertyToSetContext *data = (__IOHIDApplyPropertyToSetContext*)context;
    if (device && data) {
        IOHIDDeviceSetProperty(device, data->key, data->property);
    }
}

//------------------------------------------------------------------------------
void __IOHIDApplyPropertiesToDeviceFromDictionary(const void *key, const void *value, void *context) {
    IOHIDDeviceRef device = (IOHIDDeviceRef)context;
    if (device && key && value) {
        IOHIDDeviceSetProperty(device, (CFStringRef)key, (CFTypeRef)value);
    }
}

//------------------------------------------------------------------------------
void __IOHIDSaveDeviceSet(const void *value, void *context) {
    IOHIDDeviceRef device = (IOHIDDeviceRef)value;
    if (device)
        __IOHIDDeviceSaveProperties(device, (__IOHIDPropertyContext*)context);
}

//------------------------------------------------------------------------------
