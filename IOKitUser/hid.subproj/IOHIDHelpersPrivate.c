/*
 * Copyright (c) 2025 Apple Computer, Inc.  All Rights Reserved.
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

#include "IOHIDHelpersPrivate.h"
#include <AssertMacros.h>

bool _IOHIDTraverseDevicesFromParentService(io_service_t parentService, IOHIDTraverseDevicesBlock handler) {
    kern_return_t ret = kIOReturnError;
    mach_timespec_t wait;
    io_iterator_t iterator = IO_OBJECT_NULL;
    io_iterator_t service = IO_OBJECT_NULL;
    bool result = false;
    
    require_action(parentService != IO_OBJECT_NULL, exit, result = false);

    wait.tv_sec = 1; // Max wait 1 second for service busy state to clear
    wait.tv_nsec = 0;
    ret = IOServiceWaitQuiet(parentService, &wait);
    require_action(ret == kIOReturnSuccess, exit, result = false);
    
    ret = IORegistryEntryCreateIterator(parentService, kIOServicePlane, kIORegistryIterateRecursively, &iterator);
    require_action(ret == kIOReturnSuccess && iterator != IO_OBJECT_NULL, exit, result = false);
    
    service = IOIteratorNext(iterator);
    while(service != IO_OBJECT_NULL) {
        ret = IOServiceWaitQuiet(service, &wait);
        require_action(ret == kIOReturnSuccess, exit, result = false);
        
        if(IOObjectConformsTo(service, "IOHIDDevice")) {
            handler(service);
        }
        
        if(service != IO_OBJECT_NULL) {
            IOObjectRelease(service);
            service = IO_OBJECT_NULL;
        }
        service = IOIteratorNext(iterator);
    }
    
    result = true;
    
exit:
    if(service != IO_OBJECT_NULL) {
        IOObjectRelease(service);
        service = IO_OBJECT_NULL;
    }
    if(iterator != IO_OBJECT_NULL) {
        IOObjectRelease(iterator);
        iterator = IO_OBJECT_NULL;
    }
    return result;
}
