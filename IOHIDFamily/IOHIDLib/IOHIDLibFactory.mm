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

#include <TargetConditionals.h>

#include <IOKit/hid/IOHIDDevicePlugIn.h>
#include <IOKit/hid/IOHIDServicePlugIn.h>
#include "IOHIDIUnknown.h"
#import "IOHIDDeviceClass.h"
#import "IOHIDObsoleteDeviceClass.h"
#import "IOHIDUPSClass.h"
#include "IOHIDEventServiceFastPathClass.h"

extern "C" void *IOHIDLibFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeID);

void *IOHIDLibFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeID)
{
    IOCFPlugInInterface **interface = NULL;
    
    if (CFEqual(typeID, kIOHIDDeviceUserClientTypeID)) {
        IOHIDObsoleteDeviceClass *device = [[IOHIDObsoleteDeviceClass alloc] init];
        [device queryInterface:CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID)
               outInterface:(LPVOID *)&interface];
    } else if (CFEqual(typeID, kIOHIDDeviceTypeID)) {
        IOHIDDeviceClass *device = [[IOHIDDeviceClass alloc] init];
        [device queryInterface:CFUUIDGetUUIDBytes(kIOHIDDeviceDeviceInterfaceID)
                  outInterface:(LPVOID *)&interface];
    } else if (CFEqual(typeID, kIOUPSPlugInTypeID)) {
        IOHIDUPSClass *ups = [[IOHIDUPSClass alloc] init];
        [ups queryInterface:CFUUIDGetUUIDBytes(kIOCFPlugInInterfaceID)
               outInterface:(LPVOID *)&interface];
    } else if (CFEqual(typeID, kIOHIDServiceFastPathPlugInTypeID)) {
        interface = IOHIDEventServiceFastPathClass::alloc();
    }
    
    return (void *)interface;
}
