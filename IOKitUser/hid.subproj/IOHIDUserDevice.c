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

#include <pthread.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFBase.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDResourceUserClient.h>
#include "IOHIDUserDevice.h"

static IOHIDUserDeviceRef   __IOHIDUserDeviceCreate(
                                    CFAllocatorRef          allocator, 
                                    CFAllocatorContext *    context __unused);
static void                 __IOHIDUserDeviceRelease( CFTypeRef object );
static void                 __IOHIDUserDeviceRegister(void);

typedef struct __IOHIDUserDevice
{
    CFRuntimeBase                   cfBase;   // base CFType information

    io_service_t                    service;
    io_connect_t                    connect;
    CFDictionaryRef                 properties;

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
    
    do {
        if ( !properties )
            break;
            
        device = __IOHIDUserDeviceCreate(allocator, NULL);
        if ( !device )
            break;

        device->service = IOServiceGetMatchingService(__masterPort, IOServiceMatching( "IOHIDResource" ));
        if ( device->service == MACH_PORT_NULL )
            return NULL;
            
        kr = IOServiceOpen(device->service, mach_task_self(), kIOHIDResourceUserClientTypeDevice, &device->connect);
        if ( kr != KERN_SUCCESS )
            break;
            
        data = IOCFSerialize(properties, 0);
        if ( !data )
            break;
            
        kr = IOConnectCallStructMethod(device->connect, kIOHIDResourceDeviceUserClientMethodCreate, CFDataGetBytePtr(data), CFDataGetLength(data), NULL, NULL);
        CFRelease(data);

        if ( kr != KERN_SUCCESS )
            break;
        
        return device;
    } while ( FALSE );
    
    if ( device )
        CFRelease(device);

    return device;
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

