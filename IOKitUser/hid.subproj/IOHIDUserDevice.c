/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2012 Apple Computer, Inc.  All Rights Reserved.
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

#include <AssertMacros.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBase.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDResourceUserClient.h>
#include <IOKit/IODataQueueClient.h>
#include "IOHIDUserDevice.h"
#include <IOKit/IOKitLibPrivate.h>

static IOHIDUserDeviceRef   __IOHIDUserDeviceCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused,
                                    IOOptionBits            options);
static void                 __IOHIDUserDeviceRelease( CFTypeRef object );
static void                 __IOHIDUserDeviceRegister(void);
static void                 __IOHIDUserDeviceQueueCallback(CFMachPortRef port, void *msg, CFIndex size, void *info);
static void                 __IOHIDUserDeviceHandleReportAsyncCallback(void *refcon, IOReturn result);
static Boolean              __IOHIDUserDeviceSetupAsyncSupport(IOHIDUserDeviceRef device);
static IOReturn             __IOHIDUserDeviceStartDevice(IOHIDUserDeviceRef device, IOOptionBits options);


typedef struct __IOHIDUserDevice
{
    CFRuntimeBase                   cfBase;   // base CFType information

    io_service_t                    service;
    io_connect_t                    connect;
    CFDictionaryRef                 properties;
    IOOptionBits                    options;
    
    CFRunLoopRef                    runLoop;
    CFStringRef                     runLoopMode;
    
    dispatch_queue_t                dispatchQueue;
    
    struct {
        CFMachPortRef               port;
        CFRunLoopSourceRef          source;
        dispatch_source_t           dispatchSource;
        IODataQueueMemory *         data;
    } queue;
    
    struct {
        IONotificationPortRef       port;
        CFRunLoopSourceRef          source;
        IODataQueueMemory *         data;
    } async;
    
    struct {
        IOHIDUserDeviceReportCallback   callback;
        void *                          refcon;
    } setReport, getReport;

    struct {
        IOHIDUserDeviceReportWithReturnLengthCallback   callback;
        void *                                          refcon;
    } getReportWithReturnLength;

} __IOHIDUserDevice, *__IOHIDUserDeviceRef;

static const CFRuntimeClass __IOHIDUserDeviceClass = {
    0,                          // version
    "IOHIDUserDevice",          // className
    NULL,                       // init
    NULL,                       // copy
    __IOHIDUserDeviceRelease,   // finalize
    NULL,                       // equal
    NULL,                       // hash
    NULL,                       // copyFormattingDesc
    NULL,
    NULL,
    NULL
};

static pthread_once_t   __deviceTypeInit            = PTHREAD_ONCE_INIT;
static CFTypeID         __kIOHIDUserDeviceTypeID    = _kCFRuntimeNotATypeID;
static mach_port_t      __masterPort                = MACH_PORT_NULL;


typedef struct __IOHIDDeviceHandleReportAsyncContext {
    IOHIDUserDeviceHandleReportAsyncCallback   callback;
    void *                          refcon;
} IOHIDDeviceHandleReportAsyncContext;


//------------------------------------------------------------------------------
// __IOHIDUserDeviceRegister
//------------------------------------------------------------------------------
void __IOHIDUserDeviceRegister(void)
{
    IOMasterPort(bootstrap_port, &__masterPort);
    __kIOHIDUserDeviceTypeID = _CFRuntimeRegisterClass(&__IOHIDUserDeviceClass);
}

//------------------------------------------------------------------------------
// __IOHIDUserDeviceCreate
//------------------------------------------------------------------------------
IOHIDUserDeviceRef __IOHIDUserDeviceCreate(   
                                CFAllocatorRef              allocator, 
                                CFAllocatorContext *        context __unused,
                                IOOptionBits                options)
{
    IOHIDUserDeviceRef  device = NULL;
    void *          offset  = NULL;
    uint32_t        size;

    /* allocate service */
    size  = sizeof(__IOHIDUserDevice) - sizeof(CFRuntimeBase);
    device = (IOHIDUserDeviceRef)_CFRuntimeCreateInstance(allocator, IOHIDUserDeviceGetTypeID(), size, NULL);
    
    if (!device)
        return NULL;

    offset = device;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
    device->options = options;
    
    return device;
}

//------------------------------------------------------------------------------
// __IOHIDUserDeviceRelease
//------------------------------------------------------------------------------
void __IOHIDUserDeviceRelease( CFTypeRef object )
{
    IOHIDUserDeviceRef device = (IOHIDUserDeviceRef)object;
    
    if ( device->queue.data )
    {
#if !__LP64__
        vm_address_t        mappedMem = (vm_address_t)device->queue.data;
#else
        mach_vm_address_t   mappedMem = (mach_vm_address_t)device->queue.data;
#endif
        IOConnectUnmapMemory (  device->connect, 
                                0, 
                                mach_task_self(), 
                                mappedMem);
        device->queue.data = NULL;
    }
    
    if ( device->queue.dispatchSource ) {
        dispatch_release(device->queue.dispatchSource);
        device->queue.dispatchSource = NULL;
    }
    
    if ( device->queue.source ) {
        CFRelease(device->queue.source);
        device->queue.source = NULL;
    }

    if ( device->queue.port ) {
        mach_port_t port = CFMachPortGetPort(device->queue.port);
        
        CFMachPortInvalidate(device->queue.port);
        CFRelease(device->queue.port);

        mach_port_mod_refs(mach_task_self(),
                   port,
                   MACH_PORT_RIGHT_RECEIVE,
                   -1);

        device->queue.port = NULL;
    }
    
    if ( device->async.port ) {
        IONotificationPortDestroy(device->async.port);
        device->async.port = NULL;
    }
    
    if ( device->properties ) {
        CFRelease(device->properties);
        device->properties = NULL;
    }
    
    if ( device->connect ) {
        IOObjectRelease(device->connect);
        device->connect = 0;
    }
    
    if ( device->service ) {
        IOObjectRelease(device->service);
        device->service = 0;
    }
   
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOHIDUserDeviceGetTypeID(void) 
{
    if ( _kCFRuntimeNotATypeID == __kIOHIDUserDeviceTypeID )
        pthread_once(&__deviceTypeInit, __IOHIDUserDeviceRegister);
        
    return __kIOHIDUserDeviceTypeID;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDUserDeviceStartDevice
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOReturn __IOHIDUserDeviceStartDevice(IOHIDUserDeviceRef device, IOOptionBits options)
{
    CFDataRef   data = NULL;
    IOReturn    kr;
    uint64_t    input = options;
    
    data = IOCFSerialize(device->properties, 0);
    require_action(data, error, kr=kIOReturnNoMemory);
    
    kr = IOConnectCallMethod(device->connect, kIOHIDResourceDeviceUserClientMethodCreate, &input, 1, CFDataGetBytePtr(data), CFDataGetLength(data), NULL, NULL, NULL, NULL);
    require_noerr(kr, error);
    
error:
    if ( data )
        CFRelease(data);
    
    return kr;
    
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceCreate
//------------------------------------------------------------------------------
IOHIDUserDeviceRef IOHIDUserDeviceCreate(
                                CFAllocatorRef                  allocator, 
                                CFDictionaryRef                 properties)
{
    return IOHIDUserDeviceCreateWithOptions(allocator, properties, 0);
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceCreateWithOptions
//------------------------------------------------------------------------------
IOHIDUserDeviceRef IOHIDUserDeviceCreateWithOptions(CFAllocatorRef allocator, CFDictionaryRef properties, IOOptionBits options)
{
    IOHIDUserDeviceRef  device = NULL;
    IOHIDUserDeviceRef  result = NULL;
    kern_return_t       kr;
    
    require(properties, error);
        
    device = __IOHIDUserDeviceCreate(allocator, NULL, options);
    require(device, error);
    
    device->properties = CFDictionaryCreateCopy(allocator, properties);
    require(device->properties, error);

    device->service = IOServiceGetMatchingService(__masterPort, IOServiceMatching("IOHIDResource"));
    require(device->service, error);
        
    kr = IOServiceOpen(device->service, mach_task_self(), kIOHIDResourceUserClientTypeDevice, &device->connect);
    require_noerr(kr, error);
    
    if ( (device->options & kIOHIDUserDeviceCreateOptionStartWhenScheduled) == 0 ) {
        kr = __IOHIDUserDeviceStartDevice(device, device->options);
        require_noerr(kr, error);
    }
    
    result = device;
    CFRetain(result);

error:

    if ( device )
        CFRelease(device);

    return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDUserDeviceSetupAsyncSupport
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Boolean __IOHIDUserDeviceSetupAsyncSupport(IOHIDUserDeviceRef device)
{
    Boolean result;
    
    if ( !device->queue.data ) {
        IOReturn ret;
    #if !__LP64__
        vm_address_t        address = 0;
        vm_size_t           size    = 0;
    #else
        mach_vm_address_t   address = 0;
        mach_vm_size_t      size    = 0;
    #endif
        
        ret = IOConnectMapMemory(device->connect, 0, mach_task_self(), &address, &size, kIOMapAnywhere);
        require_noerr_action(ret, exit, result=false);
        
        device->queue.data =(IODataQueueMemory * )address;
    }

    if ( !device->queue.port ) {
        mach_port_t port = IODataQueueAllocateNotificationPort();
        
        if ( port != MACH_PORT_NULL ) {
            CFMachPortContext context = {0, device, NULL, NULL, NULL};
            
            device->queue.port = CFMachPortCreateWithPort(CFGetAllocator(device), port, __IOHIDUserDeviceQueueCallback, &context, FALSE);
        }
    }
    require_action(device->queue.port, exit, result=false);

    if ( !device->async.port ) {
        device->async.port = IONotificationPortCreate(kIOMasterPortDefault);
    }

    require_action(device->async.port, exit, result=false);

    result = true;
    
exit:
    
    return true;
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOHIDUserDeviceScheduleWithRunLoop(IOHIDUserDeviceRef device, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    if ( !__IOHIDUserDeviceSetupAsyncSupport(device) )
        return;
    
    if ( !device->queue.source ) {
        device->queue.source = CFMachPortCreateRunLoopSource(CFGetAllocator(device), device->queue.port, 0);
        if ( !device->queue.source )
            return;
    }
    
    if ( !device->async.source ) {
        device->async.source = IONotificationPortGetRunLoopSource(device->async.port);
        if ( !device->async.source )
            return;
    }
    
    CFRunLoopAddSource(runLoop, device->async.source, runLoopMode);
    CFRunLoopAddSource(runLoop, device->queue.source, runLoopMode);
    IOConnectSetNotificationPort(device->connect, 0, CFMachPortGetPort(device->queue.port), (uintptr_t)NULL);
    
    if ( device->options & kIOHIDUserDeviceCreateOptionStartWhenScheduled ) {
        __IOHIDUserDeviceStartDevice(device, device->options);
    }
    
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceUnscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOHIDUserDeviceUnscheduleFromRunLoop(IOHIDUserDeviceRef device, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    if ( !device->queue.port )
        return;
        
    IOConnectSetNotificationPort(device->connect, 0, MACH_PORT_NULL, (uintptr_t)NULL);
    CFRunLoopRemoveSource(runLoop, device->queue.source, runLoopMode);
    CFRunLoopRemoveSource(runLoop, device->async.source, runLoopMode);
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceScheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDUserDeviceScheduleWithDispatchQueue(IOHIDUserDeviceRef device, dispatch_queue_t queue)
{
    if ( !__IOHIDUserDeviceSetupAsyncSupport(device) )
        return;
    
    if ( !device->queue.dispatchSource ) {
        device->queue.dispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, CFMachPortGetPort(device->queue.port), 0, queue);

        if ( !device->queue.dispatchSource )
            return;

        dispatch_source_set_event_handler(device->queue.dispatchSource, ^{
            CFRetain(device);
            mach_msg_size_t size = sizeof(mach_msg_header_t) + MAX_TRAILER_SIZE;
            mach_msg_header_t *msg = (mach_msg_header_t *)CFAllocatorAllocate(CFGetAllocator(device), size, 0);
            msg->msgh_size = size;
            for (;;) {
                msg->msgh_bits = 0;
                msg->msgh_local_port = CFMachPortGetPort(device->queue.port);
                msg->msgh_remote_port = MACH_PORT_NULL;
                msg->msgh_id = 0;
                kern_return_t ret = mach_msg(msg, MACH_RCV_MSG|MACH_RCV_LARGE|MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)|MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AV), 0, msg->msgh_size, CFMachPortGetPort(device->queue.port), 0, MACH_PORT_NULL);
                if (MACH_MSG_SUCCESS == ret) break;
                if (MACH_RCV_TOO_LARGE != ret) goto inner_exit;
                uint32_t newSize = round_msg(msg->msgh_size + MAX_TRAILER_SIZE);
                msg = CFAllocatorReallocate(CFGetAllocator(device), msg, newSize, 0);
                msg->msgh_size = newSize;
            }
            
            __IOHIDUserDeviceQueueCallback(device->queue.port, msg, msg->msgh_size, device);
            
        inner_exit:
            CFAllocatorDeallocate(kCFAllocatorSystemDefault, msg);
            CFRelease(device);
        });
    }
    
    IONotificationPortSetDispatchQueue(device->async.port, queue);
    
    dispatch_resume(device->queue.dispatchSource);
    IOConnectSetNotificationPort(device->connect, 0, CFMachPortGetPort(device->queue.port), (uintptr_t)NULL);
    
    device->dispatchQueue = queue;
    
    if ( device->options & kIOHIDUserDeviceCreateOptionStartWhenScheduled ) {
        __IOHIDUserDeviceStartDevice(device, device->options);
    }
    
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceUnscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDUserDeviceUnscheduleFromDispatchQueue(IOHIDUserDeviceRef device, dispatch_queue_t queue)
{
    if ( !device->queue.port || device->dispatchQueue != queue)
        return;
    
    IOConnectSetNotificationPort(device->connect, 0, MACH_PORT_NULL, (uintptr_t)NULL);
    
    if ( device->queue.dispatchSource ) {
        dispatch_release(device->queue.dispatchSource);
        device->queue.dispatchSource = NULL;
    }
    
    if ( device->async.port ) {
        IONotificationPortDestroy(device->async.port);
        device->async.port = NULL;
    }    
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceRegisterGetReportCallback
//------------------------------------------------------------------------------
void IOHIDUserDeviceRegisterGetReportCallback(IOHIDUserDeviceRef device, IOHIDUserDeviceReportCallback callback, void * refcon)
{
    device->getReport.callback  = callback;
    device->getReport.refcon    = refcon;
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceRegisterGetReportCallback
//------------------------------------------------------------------------------
void IOHIDUserDeviceRegisterGetReportWithReturnLengthCallback(IOHIDUserDeviceRef device, IOHIDUserDeviceReportWithReturnLengthCallback callback, void * refcon)
{
    device->getReportWithReturnLength.callback  = callback;
    device->getReportWithReturnLength.refcon    = refcon;
}


//------------------------------------------------------------------------------
// IOHIDUserDeviceRegisterSetReportCallback
//------------------------------------------------------------------------------
void IOHIDUserDeviceRegisterSetReportCallback(IOHIDUserDeviceRef device, IOHIDUserDeviceReportCallback callback, void * refcon)
{
    device->setReport.callback  = callback;
    device->setReport.refcon    = refcon;
}

#ifndef min
#define min(a, b) \
    ((a < b) ? a:b)
#endif
//------------------------------------------------------------------------------
// __IOHIDUserDeviceQueueCallback
//------------------------------------------------------------------------------
void __IOHIDUserDeviceQueueCallback(CFMachPortRef port __unused, void *msg __unused, CFIndex size __unused, void *info)
{
    IOHIDUserDeviceRef device = (IOHIDUserDeviceRef)info;
    
    if ( !device->queue.data )
        return;

    // check entry size
    IODataQueueEntry *  nextEntry;
    uint32_t            dataSize;

    // if queue empty, then stop
    while ((nextEntry = IODataQueuePeek(device->queue.data))) {
    
        IOHIDResourceDataQueueHeader *  header                                                  = (IOHIDResourceDataQueueHeader*)&(nextEntry->data);
        uint64_t                        response[kIOHIDResourceUserClientResponseIndexCount]    = {kIOReturnUnsupported,header->token};
        uint8_t *                       responseReport  = NULL;
        CFIndex                         responseLength  = 0;
                 
        // set report
        if ( header->direction == kIOHIDResourceReportDirectionOut ) {
            CFIndex     reportLength    = min(header->length, (nextEntry->size - sizeof(IOHIDResourceDataQueueHeader)));
            uint8_t *   report          = ((uint8_t*)header)+sizeof(IOHIDResourceDataQueueHeader);
            
            if ( device->setReport.callback )
                response[kIOHIDResourceUserClientResponseIndexResult] = (*device->setReport.callback)(device->setReport.refcon, header->type, header->reportID, report, reportLength);
                
        } 
        else if ( header->direction == kIOHIDResourceReportDirectionIn ) {
            // RY: malloc our own data that we'll send back to the kernel.
            // I thought about mapping the mem dec from the caller in kernel,  
            // but given the typical usage, it is so not worth it
            responseReport = (uint8_t *)malloc(header->length);
            responseLength = header->length;

            if ( device->getReport.callback )
                response[kIOHIDResourceUserClientResponseIndexResult] = (*device->getReport.callback)(device->getReport.refcon, header->type, header->reportID, responseReport, responseLength);
            
            if ( device->getReportWithReturnLength.callback )
                response[kIOHIDResourceUserClientResponseIndexResult] = (*device->getReportWithReturnLength.callback)(device->getReportWithReturnLength.refcon, header->type, header->reportID, responseReport, &responseLength);
        }

        // post the response
        IOConnectCallMethod(device->connect, kIOHIDResourceDeviceUserClientMethodPostReportResponse, response, sizeof(response)/sizeof(uint64_t), responseReport, responseLength, NULL, NULL, NULL, NULL);

        if ( responseReport )
            free(responseReport);
    
        // dequeue the item
        dataSize = 0;
        IODataQueueDequeue(device->queue.data, NULL, &dataSize);
    }
}


//------------------------------------------------------------------------------
// __IOHIDUserDeviceHandleReportAsyncCallback
//------------------------------------------------------------------------------
void __IOHIDUserDeviceHandleReportAsyncCallback(void *refcon, IOReturn result)
{
    IOHIDDeviceHandleReportAsyncContext *pContext = (IOHIDDeviceHandleReportAsyncContext *)refcon;
    
    if (pContext->callback)
        pContext->callback(pContext->refcon, result);

    free(pContext);
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceHandleReportAsync
//------------------------------------------------------------------------------
IOReturn IOHIDUserDeviceHandleReportAsyncWithTimeStamp(IOHIDUserDeviceRef device, uint64_t timestamp, uint8_t *report, CFIndex reportLength, IOHIDUserDeviceHandleReportAsyncCallback callback, void * refcon)
{
    IOHIDDeviceHandleReportAsyncContext *pContext = malloc(sizeof(IOHIDDeviceHandleReportAsyncContext));
    
    if (!pContext)
        return kIOReturnNoMemory;

    pContext->callback = callback;
    pContext->refcon = refcon;
    
    mach_port_t wakePort = MACH_PORT_NULL;
    uint64_t asyncRef[kOSAsyncRef64Count];
    
    wakePort = IONotificationPortGetMachPort(device->async.port);
    
    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)(uintptr_t)__IOHIDUserDeviceHandleReportAsyncCallback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)(uintptr_t)pContext;

    return IOConnectCallAsyncMethod(device->connect, kIOHIDResourceDeviceUserClientMethodHandleReport, wakePort, asyncRef, kOSAsyncRef64Count, &timestamp, 1, report, reportLength, NULL, NULL, NULL, NULL);
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceHandleReportWithTimeStamp
//------------------------------------------------------------------------------
IOReturn IOHIDUserDeviceHandleReportWithTimeStamp(IOHIDUserDeviceRef device, uint64_t timestamp, uint8_t * report, CFIndex reportLength)
{
    return IOConnectCallMethod(device->connect, kIOHIDResourceDeviceUserClientMethodHandleReport, &timestamp, 1, report, reportLength, NULL, NULL, NULL, NULL);
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceHandleReport
//------------------------------------------------------------------------------
IOReturn IOHIDUserDeviceHandleReport(IOHIDUserDeviceRef device, uint8_t * report, CFIndex reportLength)
{
    return IOHIDUserDeviceHandleReportWithTimeStamp(device, mach_absolute_time(), report, reportLength);
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceHandleReportAsync
//------------------------------------------------------------------------------
IOReturn IOHIDUserDeviceHandleReportAsync(IOHIDUserDeviceRef device, uint8_t * report, CFIndex reportLength, IOHIDUserDeviceHandleReportAsyncCallback callback, void * refcon)
{
    return IOHIDUserDeviceHandleReportAsyncWithTimeStamp(device, mach_absolute_time(), report, reportLength, callback, refcon);
}

