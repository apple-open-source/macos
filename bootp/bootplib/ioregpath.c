/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include "ioregpath.h"
#include <IOKit/IOKitLib.h>

CFDictionaryRef
myIORegistryEntryCopyValue(const char * path)
{
    mach_port_t         	master;
    io_registry_entry_t 	service;
    kern_return_t       	status;
    CFMutableDictionaryRef	properties = NULL;

    status = IOMasterPort(bootstrap_port, &master);
    if (status != KERN_SUCCESS) {
	return (NULL);
    }
    service = IORegistryEntryFromPath(master, path);
    if (status != KERN_SUCCESS) {
	return (NULL);
    }
    status = IORegistryEntryCreateCFProperties(service,
					       &properties,
					       kCFAllocatorDefault,
					       kNilOptions);
    if (status != KERN_SUCCESS) {
	properties = NULL;
    }
    IOObjectRelease(service);
    return (properties);
}
