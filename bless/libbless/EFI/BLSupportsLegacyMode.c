/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
/*
 *  BLSupportsLegacyMode.c
 *  bless
 *
 *  Created by Shantonu Sen on 2/10/06.
 *  Copyright 2006 Apple Computer. All Rights Reserved.
 *
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

#include <sys/stat.h>
#include "bless.h"
#include "bless_private.h"

// Check if a system supports CSM legacy mode

#define kBL_APPLE_VENDOR_NVRAM_GUID "4D1EDE05-38C7-4A6A-9CC6-4BCCA8B38C14"
static bool _getFeatureFlags(BLContextPtr context, uint32_t *featureMask,
                        uint32_t *featureFlags);

bool BLSupportsLegacyMode(BLContextPtr context)
{
    
	uint32_t		featureMask;
	uint32_t		featureFlags;
    
    if(!_getFeatureFlags(context, &featureMask, &featureFlags)) {
        return false;
    }

	if((featureMask & 0x00000001)
	   && (featureFlags & 0x00000001)) {
        contextprintf(context, kBLLogLevelVerbose,  "Legacy mode suppported\n");
		return true;
	}
	
    contextprintf(context, kBLLogLevelVerbose,  "Legacy mode NOT suppported\n");
    return false;
}

static bool _getFeatureFlags(BLContextPtr context, uint32_t *featureMask,
                        uint32_t *featureFlags)
{
    
    io_registry_entry_t optionsNode = IO_OBJECT_NULL;
    CFDataRef		dataRef;
    
    optionsNode = IORegistryEntryFromPath(kIOMasterPortDefault, kIODeviceTreePlane ":/options");
    
    if(IO_OBJECT_NULL == optionsNode) {
        contextprintf(context, kBLLogLevelVerbose,  "Could not find " kIODeviceTreePlane ":/options\n");
        return false;
    }
    
    dataRef = IORegistryEntryCreateCFProperty(optionsNode,
											 CFSTR(kBL_APPLE_VENDOR_NVRAM_GUID ":FirmwareFeaturesMask"),
											 kCFAllocatorDefault, 0);

	if(dataRef != NULL
	   && CFGetTypeID(dataRef) == CFDataGetTypeID()
	   && CFDataGetLength(dataRef) == sizeof(uint32_t)) {
		const UInt8	*bytes = CFDataGetBytePtr(dataRef);
		
		*featureMask = CFSwapInt32LittleToHost(*(uint32_t *)bytes);
	} else {
		*featureMask = 0x000003FF;
	}
    
	if(dataRef) CFRelease(dataRef);
	
    dataRef = IORegistryEntryCreateCFProperty(optionsNode,
											  CFSTR(kBL_APPLE_VENDOR_NVRAM_GUID ":FirmwareFeatures"),
											  kCFAllocatorDefault, 0);
	
	if(dataRef != NULL
	   && CFGetTypeID(dataRef) == CFDataGetTypeID()
	   && CFDataGetLength(dataRef) == sizeof(uint32_t)) {
		const UInt8	*bytes = CFDataGetBytePtr(dataRef);
		
		*featureFlags = CFSwapInt32LittleToHost(*(uint32_t *)bytes);
	} else {
		*featureFlags = 0x00000014;
	}
    
	if(dataRef) CFRelease(dataRef);
		
	IOObjectRelease(optionsNode);
    
	contextprintf(context, kBLLogLevelVerbose,  "Firmware feature mask: 0x%08X\n", *featureMask);
	contextprintf(context, kBLLogLevelVerbose,  "Firmware features: 0x%08X\n", *featureFlags);
	
    return true;
}
