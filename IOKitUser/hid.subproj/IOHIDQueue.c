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

#include <pthread.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/hid/IOHIDDevicePlugIn.h>
#include "IOHIDLibPrivate.h"
#include "IOHIDDevice.h"
#include "IOHIDQueue.h"

static IOHIDQueueRef    __IOHIDQueueCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused);
static void             __IOHIDQueueRelease( CFTypeRef object );
static void             __IOHIDQueueValueAvailableCallback(
                                void *                          context,
                                IOReturn                        result,
                                void *                          sender);


typedef struct __IOHIDQueue
{
    CFRuntimeBase                   cfBase;   // base CFType information

    IOHIDDeviceQueueInterface**     queueInterface;
    
    CFTypeRef                       asyncEventSource;
    CFRunLoopRef                    asyncRunLoop;
    CFStringRef                     asyncRunLoopMode;

    IOHIDDeviceRef                  device;
    void *                          context;
    IOHIDCallback                   callback;
    
    CFMutableSetRef                 elements;
} __IOHIDQueue, *__IOHIDQueueRef;

static const CFRuntimeClass __IOHIDQueueClass = {
    0,                      // version
    "IOHIDQueue",           // className
    NULL,                   // init
    NULL,                   // copy
    __IOHIDQueueRelease,    // finalize
    NULL,                   // equal
    NULL,                   // hash
    NULL,                   // copyFormattingDesc
    NULL
};

static pthread_once_t __queueTypeInit = PTHREAD_ONCE_INIT;
static CFTypeID __kIOHIDQueueTypeID = _kCFRuntimeNotATypeID;

//------------------------------------------------------------------------------
// __IOHIDQueueRegister
//------------------------------------------------------------------------------
void __IOHIDQueueRegister(void)
{
    __kIOHIDQueueTypeID = _CFRuntimeRegisterClass(&__IOHIDQueueClass);
}

//------------------------------------------------------------------------------
// __IOHIDQueueCreate
//------------------------------------------------------------------------------
IOHIDQueueRef __IOHIDQueueCreate(   
                                CFAllocatorRef              allocator, 
                                CFAllocatorContext *        context __unused)
{
    IOHIDQueueRef   queue   = NULL;
    void *          offset  = NULL;
    uint32_t        size;

    /* allocate service */
    size  = sizeof(__IOHIDQueue) - sizeof(CFRuntimeBase);
    queue = (IOHIDQueueRef)_CFRuntimeCreateInstance(allocator, IOHIDQueueGetTypeID(), size, NULL);
    
    if (!queue)
        return NULL;

    offset = queue;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
    return queue;
}

//------------------------------------------------------------------------------
// __IOHIDQueueRelease
//------------------------------------------------------------------------------
void __IOHIDQueueRelease( CFTypeRef object )
{
    IOHIDQueueRef queue = (IOHIDQueueRef)object;
    
    if ( queue->elements ) {
        CFRelease(queue->elements);
        queue->elements = NULL;
    }
    
    if ( queue->queueInterface ) {
        (*queue->queueInterface)->Release(queue->queueInterface);
        queue->queueInterface = NULL;
    }
    
    if ( queue->device ) {
        CFRelease(queue->device);
        queue->device = NULL;
    }

}

//------------------------------------------------------------------------------
// __IOHIDQueueValueAvailableCallback
//------------------------------------------------------------------------------
void __IOHIDQueueValueAvailableCallback(
                                void *                          context,
                                IOReturn                        result,
                                void *                          sender __unused)
{
    IOHIDQueueRef queue = (IOHIDQueueRef)context;

    if ( !queue || !queue->callback)
        return;
        
    (*queue->callback)( queue->context,
                        result,
                        queue);
}

//------------------------------------------------------------------------------
// IOHIDQueueGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOHIDQueueGetTypeID(void) 
{
    if ( _kCFRuntimeNotATypeID == __kIOHIDQueueTypeID )
        pthread_once(&__queueTypeInit, __IOHIDQueueRegister);
        
    return __kIOHIDQueueTypeID;
}

//------------------------------------------------------------------------------
// IOHIDQueueCreate
//------------------------------------------------------------------------------
IOHIDQueueRef IOHIDQueueCreate(
                                CFAllocatorRef                  allocator, 
                                IOHIDDeviceRef                  device,
                                CFIndex                         depth,
                                IOOptionBits                    options)
{
    IOCFPlugInInterface **          deviceInterface = NULL;
    IOHIDDeviceQueueInterface **    queueInterface  = NULL;
    IOHIDQueueRef                   queue           = NULL;
    IOReturn                        ret;
    
    if ( !device )
        return NULL;
        
    deviceInterface = _IOHIDDeviceGetIOCFPlugInInterface(device);
    
    if ( !deviceInterface )
        return NULL;
        
    ret = (*deviceInterface)->QueryInterface(
                            deviceInterface, 
                            CFUUIDGetUUIDBytes(kIOHIDDeviceQueueInterfaceID), 
                            (LPVOID)&queueInterface);
    
    if ( ret != kIOReturnSuccess || !queueInterface )
        return NULL;
        
    queue = __IOHIDQueueCreate(allocator, NULL);
    
    if ( !queue ) {
        (*queueInterface)->Release(queueInterface);
        return NULL;
    }

    queue->queueInterface   = queueInterface;
    queue->device           = (IOHIDDeviceRef)CFRetain(device);
    
    (*queue->queueInterface)->setDepth(queue->queueInterface, depth, options);
    
    return queue;
}

//------------------------------------------------------------------------------
// IOHIDQueueGetDevice
//------------------------------------------------------------------------------
IOHIDDeviceRef IOHIDQueueGetDevice(     
                                IOHIDQueueRef                   queue)
{
    return queue->device;
}

//------------------------------------------------------------------------------
// IOHIDQueueGetDepth
//------------------------------------------------------------------------------
CFIndex IOHIDQueueGetDepth(     
                                IOHIDQueueRef                   queue)
{
    uint32_t depth = 0;
    (*queue->queueInterface)->getDepth(queue->queueInterface, &depth);
    
    return depth;
}

//------------------------------------------------------------------------------
// IOHIDQueueSetDepth
//------------------------------------------------------------------------------
void IOHIDQueueSetDepth(        
                                IOHIDQueueRef                   queue,
                                CFIndex                         depth)
{
    (*queue->queueInterface)->setDepth(queue->queueInterface, depth, 0);
}
                                
//------------------------------------------------------------------------------
// IOHIDQueueAddElement
//------------------------------------------------------------------------------
void IOHIDQueueAddElement(      
                                IOHIDQueueRef                   queue,
                                IOHIDElementRef                 element)
{
    (*queue->queueInterface)->addElement(queue->queueInterface, element, 0);
    
    if ( !queue->elements )
        queue->elements = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
        
    if ( queue->elements )
        CFSetAddValue(queue->elements, element);
}
                                
//------------------------------------------------------------------------------
// IOHIDQueueRemoveElement
//------------------------------------------------------------------------------
void IOHIDQueueRemoveElement(
                                IOHIDQueueRef                   queue,
                                IOHIDElementRef                 element)
{
    (*queue->queueInterface)->removeElement(queue->queueInterface, element, 0);

    if ( queue->elements )
        CFSetRemoveValue(queue->elements, element);
}
                                
//------------------------------------------------------------------------------
// IOHIDQueueContainsElement
//------------------------------------------------------------------------------
Boolean IOHIDQueueContainsElement(
                                IOHIDQueueRef                   queue,
                                IOHIDElementRef                 element)
{
    Boolean hasElement = FALSE;
    
    (*queue->queueInterface)->containsElement(
                                            queue->queueInterface, 
                                            element, 
                                            &hasElement, 
                                            0);
                                            
    return hasElement;
}
                                
//------------------------------------------------------------------------------
// IOHIDQueueStart
//------------------------------------------------------------------------------
void IOHIDQueueStart(           IOHIDQueueRef                   queue)
{
    (*queue->queueInterface)->start(queue->queueInterface, 0);
}

//------------------------------------------------------------------------------
// IOHIDQueueStop
//------------------------------------------------------------------------------
void IOHIDQueueStop(            IOHIDQueueRef                   queue)
{
    (*queue->queueInterface)->stop(queue->queueInterface, 0);
}

//------------------------------------------------------------------------------
// IOHIDQueueScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOHIDQueueScheduleWithRunLoop(
                                IOHIDQueueRef                   queue, 
                                CFRunLoopRef                    runLoop, 
                                CFStringRef                     runLoopMode)
{
    if ( !queue->asyncEventSource) {
        IOReturn ret;
        
        ret = (*queue->queueInterface)->getAsyncEventSource(
                                                    queue->queueInterface,
                                                    &queue->asyncEventSource);
        
        if (ret != kIOReturnSuccess || !queue->asyncEventSource)
            return;
    }

    queue->asyncRunLoop     = runLoop;
    queue->asyncRunLoopMode = runLoopMode;
        
    if (CFGetTypeID(queue->asyncEventSource) == CFRunLoopSourceGetTypeID())
        CFRunLoopAddSource( queue->asyncRunLoop, 
                            (CFRunLoopSourceRef)queue->asyncEventSource, 
                            queue->asyncRunLoopMode);
    else if (CFGetTypeID(queue->asyncEventSource) == CFRunLoopTimerGetTypeID())
        CFRunLoopAddTimer(  queue->asyncRunLoop, 
                            (CFRunLoopTimerRef)queue->asyncEventSource, 
                            queue->asyncRunLoopMode);

}

//------------------------------------------------------------------------------
// IOHIDQueueUnscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOHIDQueueUnscheduleFromRunLoop(  
                                IOHIDQueueRef                   queue, 
                                CFRunLoopRef                    runLoop, 
                                CFStringRef                     runLoopMode)
{
    if ( !queue->asyncEventSource )
        return;
        
    if (CFGetTypeID(queue->asyncEventSource) == CFRunLoopSourceGetTypeID())
        CFRunLoopRemoveSource(  runLoop, 
                                (CFRunLoopSourceRef)queue->asyncEventSource, 
                                runLoopMode);
    else if (CFGetTypeID(queue->asyncEventSource) == CFRunLoopTimerGetTypeID())
        CFRunLoopRemoveTimer(   runLoop, 
                                (CFRunLoopTimerRef)queue->asyncEventSource, 
                                runLoopMode);
                                
    queue->asyncRunLoop     = NULL;
    queue->asyncRunLoopMode = NULL;
}
                                
//------------------------------------------------------------------------------
// IOHIDQueueSetValueCallback
//------------------------------------------------------------------------------
void IOHIDQueueRegisterValueAvailableCallback(
                                IOHIDQueueRef                   queue,
                                IOHIDCallback                   callback,
                                void *                          context)
{
    queue->context  = context;
    queue->callback = callback;
    
    (*queue->queueInterface)->setValueAvailableCallback(
                                queue->queueInterface,
                                __IOHIDQueueValueAvailableCallback,
                                queue);
}

//------------------------------------------------------------------------------
// IOHIDQueueCopyNextValue
//------------------------------------------------------------------------------
IOHIDValueRef IOHIDQueueCopyNextValue(
                                IOHIDQueueRef                   queue)
{    
    return IOHIDQueueCopyNextValueWithTimeout(queue, 0);
}

//------------------------------------------------------------------------------
// IOHIDQueueCopyNextValueWithTimeout
//------------------------------------------------------------------------------
IOHIDValueRef IOHIDQueueCopyNextValueWithTimeout(
                                IOHIDQueueRef                   queue,
                                CFTimeInterval                  timeout)
{
    IOHIDValueRef   value       = NULL;
    uint32_t        timeoutMS   = timeout * 1000;
    
    (*queue->queueInterface)->copyNextValue(queue->queueInterface,
                                            &value,
                                            timeoutMS,
                                            0);
                                            
    return value;
}

//------------------------------------------------------------------------------
// _IOHIDQueueCopyElements
//------------------------------------------------------------------------------
CFArrayRef _IOHIDQueueCopyElements(IOHIDQueueRef queue)
{
    if ( !queue->elements )
        return NULL;
        
    CFIndex count = CFSetGetCount( queue->elements );
    
    if ( !count )
        return NULL;
        
    IOHIDElementRef * elements  = malloc(sizeof(IOHIDElementRef) * count);
    CFArrayRef        ret       = NULL;
    
    bzero(elements, sizeof(IOHIDElementRef) * count);
    
    CFSetGetValues(queue->elements, (const void **)elements);
    
    ret = CFArrayCreate(kCFAllocatorDefault, (const void **)elements, count, &kCFTypeArrayCallBacks);
    
    free(elements);
    
    return ret;
}

