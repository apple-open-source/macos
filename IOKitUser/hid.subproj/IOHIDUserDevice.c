/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBase.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDResourceUserClient.h>
#include <IOKit/IODataQueueClient.h>
#include "IOHIDUserDevice.h"

static IOHIDUserDeviceRef   __IOHIDUserDeviceCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused);
static void                 __IOHIDUserDeviceRelease( CFTypeRef object );
static void                 __IOHIDUserDeviceRegister(void);
static void                 __IOHIDUserDeviceQueueCallback(CFMachPortRef port, void *msg, CFIndex size, void *info);

typedef struct __IOHIDUserDevice
{
    CFRuntimeBase                   cfBase;   // base CFType information

    io_service_t                    service;
    io_connect_t                    connect;
    CFDictionaryRef                 properties;
    
    CFRunLoopRef                    runLoop;
    CFStringRef                     runLoopMode;
    
    struct {
        CFMachPortRef               port;
        CFRunLoopSourceRef          source;
        IODataQueueMemory *         data;
    } queue;
    
    struct {
        IOHIDUserDeviceReportCallback   callback;
        void *                          refcon;
    } setReport, getReport;

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
    NULL
};

static pthread_once_t   __deviceTypeInit            = PTHREAD_ONCE_INIT;
static CFTypeID         __kIOHIDUserDeviceTypeID    = _kCFRuntimeNotATypeID;
static mach_port_t      __masterPort                = MACH_PORT_NULL;

//------------------------------------------------------------------------------
// __IOHIDUserDeviceRegister
//------------------------------------------------------------------------------
void __IOHIDUserDeviceRegister(void)
{
    __kIOHIDUserDeviceTypeID = _CFRuntimeRegisterClass(&__IOHIDUserDeviceClass);
    IOMasterPort(bootstrap_port, &__masterPort);
}

//------------------------------------------------------------------------------
// __IOHIDUserDeviceCreate
//------------------------------------------------------------------------------
IOHIDUserDeviceRef __IOHIDUserDeviceCreate(   
                                CFAllocatorRef              allocator, 
                                CFAllocatorContext *        context __unused)
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
    
    if ( device->queue.source ) {
        CFRelease(device->queue.source);
        device->queue.source = NULL;
    }

    if ( device->queue.port ) {
        CFRelease(device->queue.port);
        device->queue.port = NULL;
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

//------------------------------------------------------------------------------
// IOHIDUserDeviceCreate
//------------------------------------------------------------------------------
IOHIDUserDeviceRef IOHIDUserDeviceCreate(
                                CFAllocatorRef                  allocator, 
                                CFDictionaryRef                 properties)
{
    IOHIDUserDeviceRef  device;
    CFDataRef           data;
    kern_return_t       kr;
    
    require(properties, error);
        
    device = __IOHIDUserDeviceCreate(allocator, NULL);
    require(device, error);

    device->service = IOServiceGetMatchingService(__masterPort, IOServiceMatching( "IOHIDResource" ));
    require(device->service, error);
        
    kr = IOServiceOpen(device->service, mach_task_self(), kIOHIDResourceUserClientTypeDevice, &device->connect);
    require_noerr(kr, error);
        
    data = IOCFSerialize(properties, 0);
    require(data, error);
        
    kr = IOConnectCallStructMethod(device->connect, kIOHIDResourceDeviceUserClientMethodCreate, CFDataGetBytePtr(data), CFDataGetLength(data), NULL, NULL);
    CFRelease(data);

    require_noerr(kr, error);
    
    return device;

error:    
    if ( device )
        CFRelease(device);

    return NULL;
}

//------------------------------------------------------------------------------
// IOHIDUserDeviceScheduleWithRunLoop
//------------------------------------------------------------------------------
void IOHIDUserDeviceScheduleWithRunLoop(IOHIDUserDeviceRef device, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    if ( !device->queue.data ) {
        IOReturn ret;
#if !__LP64__
        vm_address_t        address = 0;
        vm_size_t           size    = 0;
#else
        mach_vm_address_t   address = 0;
        mach_vm_size_t      size    = 0;
#endif
        ret = IOConnectMapMemory (	device->connect, 
                                    0, 
                                    mach_task_self(), 
                                    &address, 
                                    &size, 
                                    kIOMapAnywhere	);
        if (ret != kIOReturnSuccess) 
            return;
        
        device->queue.data = (IODataQueueMemory *) address;
    }

    if ( !device->queue.port ) {
        mach_port_t port = IODataQueueAllocateNotificationPort();
        
        if ( port != MACH_PORT_NULL ) {
            CFMachPortContext context = {0, device, NULL, NULL, NULL};
            
            device->queue.port = CFMachPortCreateWithPort(kCFAllocatorDefault, port, __IOHIDUserDeviceQueueCallback, &context, FALSE);
        }
    }
    
    if ( !device->queue.source ) {
        
        if ( device->queue.port ) {
            device->queue.source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, device->queue.port, 0);
        }
    }

    CFRunLoopAddSource(runLoop, device->queue.source, runLoopMode);
    IOConnectSetNotificationPort(device->connect, 0, CFMachPortGetPort(device->queue.port), (uintptr_t)NULL);
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
        uint32_t                        responseLength  = 0;
                 
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

            if ( device->setReport.callback )
                response[kIOHIDResourceUserClientResponseIndexResult] = (*device->getReport.callback)(device->getReport.refcon, header->type, header->reportID, responseReport, responseLength);
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
// IOHIDUserDeviceHandleReport
//------------------------------------------------------------------------------
IOReturn IOHIDUserDeviceHandleReport(
                                IOHIDUserDeviceRef              device, 
                                uint8_t *                       report, 
                                CFIndex                         reportLength)
{
    return IOConnectCallStructMethod(device->connect, kIOHIDResourceDeviceUserClientMethodHandleReport, report, reportLength, NULL, NULL);
}

