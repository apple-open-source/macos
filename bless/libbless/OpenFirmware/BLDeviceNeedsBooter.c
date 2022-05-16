/*
 * Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
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
 *  BLDeviceNeedsBooter.c
 *  bless
 *
 *  Created by Shantonu Sen on 2/7/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLDeviceNeedsBooter.c,v 1.8 2006/02/20 22:49:57 ssen Exp $
 *
 */

#include <stdlib.h>
#include <unistd.h>

#include <mach/mach_error.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/storage/IOMedia.h>

#include <CoreFoundation/CoreFoundation.h>

#include <sys/param.h>

#include "bless.h"
#include "bless_private.h"

// based on the partition type, deduce whether OpenFirmware needs
// an auxiliary HFS+ filesystem on an Apple_Boot partition. Doesn't
// apply to RAID or non-OF systems
int BLDeviceNeedsBooter(BLContextPtr context, const char * device,
						int32_t *needsBooter,
						int32_t *isBooter,
						io_service_t *booterPartition)
{
	char			wholename[MAXPATHLEN], bootername[MAXPATHLEN];
	io_service_t			booter = 0, maindev = 0;
	uint32_t				partnum;
	int						ret;
	CFStringRef				content = NULL;
	CFBooleanRef			isWhole = NULL;


	*needsBooter = 0;
	*isBooter = 0;
	*booterPartition = 0;
	
	ret = BLGetIOServiceForDeviceName(context, (char *)device + 5, &maindev);
	if(ret) {
        contextprintf(context, kBLLogLevelError,  "Can't find IOService for %s\n", device + 5 );
        return 3;		
	}
	
	isWhole = IORegistryEntryCreateCFProperty( maindev, CFSTR(kIOMediaWholeKey),
											   kCFAllocatorDefault, 0);
	
	if(isWhole == NULL || CFGetTypeID(isWhole) !=CFBooleanGetTypeID()) {
		contextprintf(context, kBLLogLevelError,  "Wrong type of IOKit entry for kIOMediaWholeKey\n" );
		if(isWhole) CFRelease(isWhole);
		IOObjectRelease(maindev);
		return 4;
	}
	
	if(CFEqual(isWhole, kCFBooleanTrue)) {
		contextprintf(context, kBLLogLevelVerbose,  "Device IS whole\n" );
		CFRelease(isWhole);
		IOObjectRelease(maindev);
		return 0;
	} else {
		contextprintf(context, kBLLogLevelVerbose,  "Device IS NOT whole\n" );			
		CFRelease(isWhole);
	}
	
	// Okay, it's partitioned. There might be helper partitions, though...
	content = IORegistryEntryCreateCFProperty(maindev, CFSTR(kIOMediaContentKey),
											kCFAllocatorDefault, 0);
	
	if(content == NULL || CFGetTypeID(content) != CFStringGetTypeID()) {
		contextprintf(context, kBLLogLevelError,  "Wrong type of IOKit entry for kIOMediaContentKey\n" );
		if(content) CFRelease(content);
		IOObjectRelease(maindev);
		return 4;
	}
	
	
	if(CFStringCompare(content, CFSTR("Apple_HFS"), 0) == kCFCompareEqualTo
       || CFStringCompare(content, CFSTR("48465300-0000-11AA-AA11-00306543ECAC"), 0) == kCFCompareEqualTo) {
		contextprintf(context, kBLLogLevelVerbose,  "Apple_HFS partition. No external loader\n" );
		// it's an HFS partition. no loader needed
		CFRelease(content);
		IOObjectRelease(maindev);
		return 0;
	}
	
	if(CFStringCompare(content, CFSTR("Apple_Boot_RAID"), 0)
	   == kCFCompareEqualTo) {
		contextprintf(context, kBLLogLevelVerbose,  "Apple_Boot_RAID partition. No external loader\n" );
		// it's an old style RAID partition. no loader needed
		CFRelease(content);
		IOObjectRelease(maindev);
		return 0;
	}
	
	if(CFStringCompare(content, CFSTR("Apple_Boot"), 0) == kCFCompareEqualTo) {
		contextprintf(context, kBLLogLevelVerbose,  "Apple_Boot partition is an external loader\n" );
		// it's an loader itself
		CFRelease(content);
		*isBooter = 1;
		*booterPartition = maindev;
		
		return 0;
	}
			
	IOObjectRelease(maindev);
	CFRelease(content);
	
	// if we got this far, it's partitioned media that needs a booter. check for it
	contextprintf(context, kBLLogLevelVerbose,  "NOT Apple_HFS, Apple_Boot_RAID, or Apple_Boot partition.\n");
	
	// check if partition is Apple_HFS
	ret = BLGetParentDevice(context, device, wholename, &partnum);
	if(ret) {
		contextprintf(context, kBLLogLevelError,  "Could not determine partition for %s\n", device);	
		return 2;
	}
		
    if(partnum == 1) {
        // partition is of the form disk1s1. No booter at disk1s0
        contextprintf(context, kBLLogLevelVerbose,  "Skipping search for booter for %s\n", device );	    
        return 0;
    }
    
	// devname now points to "disk1s3"
	sprintf(bootername, "%ss%u", wholename + 5, partnum - 1);

	contextprintf(context, kBLLogLevelVerbose,  "Looking for external loader at %s\n", bootername );	

	ret = BLGetIOServiceForDeviceName(context, bootername, &booter);
	if(ret) {
        contextprintf(context, kBLLogLevelError,  "Can't find IOService for %s\n", bootername );
        return 3;		
	}
	
	content = IORegistryEntryCreateCFProperty(booter, CFSTR(kIOMediaContentKey),
											  kCFAllocatorDefault, 0);
	
	if(content == NULL || CFGetTypeID(content) != CFStringGetTypeID()) {
		contextprintf(context, kBLLogLevelError,  "Wrong type of IOKit entry for kIOMediaContentKey\n" );
		if(content) CFRelease(content);
		IOObjectRelease(booter);
		return 4;
	}
	
	
	if(CFStringCompare(content, CFSTR("Apple_Boot"), 0)
	   == kCFCompareEqualTo) {
		contextprintf(context, kBLLogLevelVerbose,  "Found Apple_Boot partition\n" );
		// it's an HFS partition. no loader needed
		CFRelease(content);
		*needsBooter = 1;
		*booterPartition = booter;
		return 0;
	}

	IOObjectRelease(booter);
	CFRelease(content);

	// we needed a partition, but we couldn't find it
	
	return 6;
}
