/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
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
 *  setboot.c
 *  bless
 *
 *  Created by Shantonu Sen on 1/14/05.
 *  Copyright 2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: setboot.c,v 1.6 2005/02/23 19:52:49 ssen Exp $
 *
 *  $Log: setboot.c,v $
 *  Revision 1.6  2005/02/23 19:52:49  ssen
 *  start work on -firmware mode
 *
 *  Revision 1.5  2005/02/09 00:17:56  ssen
 *  If doing a setboot on UFS, HFSX, or RAID, and a label
 *  was not explicitly specified,  query DiskArb and render to label
 *
 *  Revision 1.4  2005/02/08 00:18:45  ssen
 *  Implement support for offline updating of BootX and OF labels
 *  in Apple_Boot partitions, and also for RAIDs. Only works
 *  in --device mode so far
 *
 *  Revision 1.3  2005/02/03 00:42:22  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.2  2005/01/16 02:10:43  ssen
 *  Move updating of RAID booters into common setboot() routine
 *
 *  Revision 1.1  2005/01/14 22:28:22  ssen
 *  <rdar://problem/3861859> bless needs to try getProperty(kIOBootDeviceKey)
 *  Consolidate code to set OF or active BIOS partition
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>

#include <IOKit/IOBSD.h>
#include <DiskArbitration/DiskArbitration.h>
#include <CoreFoundation/CoreFoundation.h>

#include "enums.h"

#include "bless.h"

extern int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...);

static int updateAppleBootIfPresent(BLContextPtr context, char *device, CFDataRef bootxData,
							 CFDataRef labelData);

int setboot(BLContextPtr context, char *device, CFDataRef bootxData,
				   CFDataRef labelData)
{
	int err;	
	CFTypeRef bootData = NULL;
	
	err = BLGetRAIDBootDataForDevice(context, device, &bootData);
	if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Error while determining if %s is a RAID\n", device );
	    return 3;
	}
	
	if(bootData) {
		// might be either an array or a dictionary
		err = BLUpdateRAIDBooters(context, device, bootData, bootxData, labelData);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Error while updating RAID booters for %s\n", device );			
			// we keep going, since BootX may be able to reconstruct the RAID
		}
		CFRelease(bootData);
	} else {
		err = updateAppleBootIfPresent(context, device, bootxData, labelData);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Error while updating booter for %s\n", device );			
		}		
	}
		
	if(BLIsOpenFirmwarePresent(context)) {
		err = BLSetOpenFirmwareBootDevice(context, device);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
		}
	} else {
		err = BLSetActiveBIOSBootDevice(context, device);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set active boot partition as %s\n", device);
			return 4;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "%s set as active boot partition\n" , device);
		}
	}
	
	return 0;	
}


static int updateAppleBootIfPresent(BLContextPtr context, char *device, CFDataRef bootxData,
									CFDataRef labelData)
{
	unsigned char booterDev[MAXPATHLEN];
	io_service_t            service = 0;
	CFStringRef				name = NULL;
	int32_t					needsBooter	= 0;
	int32_t					isBooter	= 0;
	BLUpdateBooterFileSpec	*spec = NULL;
	int32_t					specCount = 0, currentCount = 0;
	
	int ret;
	
	strcpy(booterDev, "/dev/");
	
	ret = BLDeviceNeedsBooter(context, device,
							  &needsBooter,
							  &isBooter,
							  &service);
	if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine if partition needs booter\n" );		
		return 1;
	}
	
	if(!(needsBooter || isBooter))
		return 0;
	
	for(;;) {
		DADiskRef disk = NULL;
		DASessionRef session = NULL;
		CFDictionaryRef props = NULL;
		CFStringRef	daName = NULL;
		char label[MAXPATHLEN];

		
		if(labelData) break; // no need to generate
		
		session = DASessionCreate(kCFAllocatorDefault);
		if(session == NULL) {
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't connect to DiskArb\n");
			break;
		}
		
		disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, device+5);
		if(disk == NULL) {
			CFRelease(session);
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't create DADisk for %s\n",
						  device + 5);
			break;
		}
		
		props = DADiskCopyDescription(disk);
		if(props == NULL) {
			CFRelease(session);
			CFRelease(disk);
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't get properties for %s\n",
						  device + 5);
			break;
		}
		
		daName = CFDictionaryGetValue(props, kDADiskDescriptionVolumeNameKey);
		if(daName == NULL) {
			CFRelease(props);
			CFRelease(disk);
			CFRelease(session);	
			blesscontextprintf(context, kBLLogLevelVerbose, "Can't get properties for %s\n",
							   device + 5);
			break;			
		}
		
		
		
		if(!CFStringGetCString(daName, label, sizeof(label),
							   kCFStringEncodingUTF8)) {
			CFRelease(props);
			CFRelease(disk);
			CFRelease(session);	
			break;
		}

		CFRelease(props);
		CFRelease(disk);
		CFRelease(session);	
		
		ret = BLGenerateOFLabel(context, label, &labelData);
		if(ret)
			labelData = NULL;
		
		break;
	}
	
	if(!(bootxData || labelData)) {
		IOObjectRelease(service);
		return 0;
	}
	
	name = IORegistryEntryCreateCFProperty( service, CFSTR(kIOBSDNameKey),
											kCFAllocatorDefault, 0);
	
	if(name == NULL || CFStringGetTypeID() != CFGetTypeID(name)) {
		IOObjectRelease(service);
		blesscontextprintf(context, kBLLogLevelError,  "Could not find bsd name for %x\n" , service);
		return 2;
	}
	
	IOObjectRelease(service); service = 0;
	
	if(!CFStringGetCString(name,booterDev+5,sizeof(booterDev)-5,kCFStringEncodingUTF8)) {
		CFRelease(name);
		blesscontextprintf(context, kBLLogLevelError,  "Could not find bsd name for %x\n" , service);
		return 3;
	}
	
	CFRelease(name);
	
	if(labelData) specCount += 2;
	if(bootxData) specCount += 1;
	
	spec = calloc(specCount, sizeof(spec[0]));
	
	if(labelData) {
	
		spec[currentCount+0].version = 0;
		spec[currentCount+0].reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
		spec[currentCount+0].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		spec[currentCount+0].reqFilename = NULL;
		spec[currentCount+0].payloadData = labelData;
		spec[currentCount+0].postType = 0; // no type
		spec[currentCount+0].postCreator = 0; // no type
		spec[currentCount+0].foundFile = 0;
		spec[currentCount+0].updatedFile = 0;
		
		spec[currentCount+1].version = 0;
		spec[currentCount+1].reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER;
		spec[currentCount+1].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		spec[currentCount+1].reqFilename = NULL;
		spec[currentCount+1].payloadData = labelData;
		spec[currentCount+1].postType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
		spec[currentCount+1].postCreator = 0; // no type
		spec[currentCount+1].foundFile = 0;
		spec[currentCount+1].updatedFile = 0;
		
		currentCount += 2;
	}
	
	if(bootxData) {
		spec[currentCount+0].version = 0;
		spec[currentCount+0].reqType = kBL_OSTYPE_PPC_TYPE_BOOTX;
		spec[currentCount+0].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		spec[currentCount+0].reqFilename = NULL;
		spec[currentCount+0].payloadData = bootxData;
		spec[currentCount+0].postType = 0; // no type
		spec[currentCount+0].postCreator = 0; // no type
		spec[currentCount+0].foundFile = 0;
		spec[currentCount+0].updatedFile = 0;
	}
	
	ret = BLUpdateBooter(context, booterDev, spec, specCount);
	if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Error enumerating HFS+ volume\n");		
		return 1;
	}
	
	if(bootxData) {
		if(!(spec[currentCount].foundFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "No pre-existing BootX found in HFS+ volume\n");
			return 2;
		}			

		if(!(spec[currentCount].updatedFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "BootX was not updated\n");
			return 3;
		}			
	}
	
    if(labelData) {
		if(!(spec[0].foundFile || spec[1].foundFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "No pre-existing OF label found in HFS+ volume\n");
			return 2;
		}
		if(!(spec[0].updatedFile || spec[1].updatedFile)) {
			blesscontextprintf(context, kBLLogLevelError,  "OF label was not updated\n");
			return 3;
		}
	}

	free(spec);
	
	return 0;
}

