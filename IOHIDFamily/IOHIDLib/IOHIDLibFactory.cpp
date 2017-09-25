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
#include "IOHIDDeviceClass.h"
#include "IOHIDUPSClass.h"
#include "IOHIDEventServiceClass.h"
#include "IOHIDEventServiceFastPathClass.h"

extern "C" void *IOHIDLibFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeID);

void *IOHIDLibFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeID)
{
    if (CFEqual(typeID, kIOHIDDeviceUserClientTypeID))
        return (void *) IOHIDObsoleteDeviceClass::alloc();
    else if (CFEqual(typeID, kIOHIDDeviceTypeID))
        return (void *) IOHIDDeviceClass::alloc();
    else if (CFEqual(typeID, kIOHIDServicePlugInTypeID))
        return (void *) IOHIDEventServiceClass::alloc();
    else if (CFEqual(typeID, kIOUPSPlugInTypeID))
        return (void *) IOHIDUPSClass::alloc();
    else if (CFEqual(typeID, kIOHIDServiceFastPathPlugInTypeID))
        return (void *) IOHIDEventServiceFastPathClass::alloc();
    else
        return NULL;
}
