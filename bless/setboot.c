/*
 * Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
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
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: setboot.c,v 1.30 2006/07/19 00:15:36 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOCFSerialize.h>
#include <CoreFoundation/CoreFoundation.h>

#include "enums.h"
#include "bless.h"
#include "bless_private.h"
#include "protos.h"

#if USE_DISKARBITRATION
#include <DiskArbitration/DiskArbitration.h>
#endif

#if SUPPORT_RAID
static int updateAppleBootIfPresent(BLContextPtr context, char *device, CFDataRef bootxData,
							 CFDataRef labelData);
#endif

int setboot(BLContextPtr context, char *device, CFDataRef bootxData,
				   CFDataRef labelData)
{
	int err;	
	BLPreBootEnvType	preboot;
	
	err = BLGetPreBootEnvironmentType(context, &preboot);
	if(err) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine preboot environment\n");
		return 1;
	}
	
#if SUPPORT_RAID
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
#endif // SUPPORT_RAID
		
	if(preboot == kBLPreBootEnvType_OpenFirmware) {
		err = BLSetOpenFirmwareBootDevice(context, device);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set Open Firmware\n" );
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "Open Firmware set successfully\n" );
		}
	} else if(preboot == kBLPreBootEnvType_EFI) {
        err = setefidevice(context, device + 5, 0, 0, NULL, NULL, false);
		if(err) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set EFI\n" );
			return 1;
		} else {
			blesscontextprintf(context, kBLLogLevelVerbose,  "EFI set successfully\n" );
		}
	} else {
        blesscontextprintf(context, kBLLogLevelError,  "Unknown system type\n");
        return 1;
    }
	
	return 0;	
}

#if SUPPORT_RAID
static int updateAppleBootIfPresent(BLContextPtr context, char *device, CFDataRef bootxData,
									CFDataRef labelData)
{
	char booterDev[MAXPATHLEN];
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
		char label[MAXPATHLEN];
#if USE_DISKARBITRATION
		DADiskRef disk = NULL;
		DASessionRef session = NULL;
		CFDictionaryRef props = NULL;
		CFStringRef	daName = NULL;

		
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
#else // !USE_DISKARBITRATION
		strlcpy(label, "Unknown", sizeof(label));
#endif // !USE_DISKARBITRATION
		
		ret = BLGenerateLabelData(context, label, kBitmapScale_1x, &labelData);
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
		spec[currentCount+0].reqParentDir = 0;
		spec[currentCount+0].reqFilename = NULL;
		spec[currentCount+0].payloadData = labelData;
		spec[currentCount+0].postType = 0; // no type
		spec[currentCount+0].postCreator = 0; // no type
		spec[currentCount+0].foundFile = 0;
		spec[currentCount+0].updatedFile = 0;
		
		spec[currentCount+1].version = 0;
		spec[currentCount+1].reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER;
		spec[currentCount+1].reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		spec[currentCount+1].reqParentDir = 0;
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
		spec[currentCount+0].reqParentDir = 0;
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
#endif // SUPPORT_RAID

int setefilegacypath(BLContextPtr context, const char * path, int bootNext,
                            const char *legacyHint, const char *optionalData)
{
    CFStringRef xmlString = NULL;
    const char *bootString = NULL;
    int ret;
    struct statfs sb;
    if(0 != blsustatfs(path, &sb)) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           path);
        return 1;           
    }
    
    if(legacyHint) {
        ret = BLCreateEFIXMLRepresentationForDevice(context,
                                                    legacyHint+5,
                                                    NULL,
                                                    &xmlString,
                                                    false);
        
        if(ret) {
            return 1;
        }
        
        ret = setit(context, kIOMasterPortDefault, "efi-legacy-drive-hint", xmlString);    
        if(ret) return ret;
        
        ret = _forwardNVRAM(context, CFSTR("efi-legacy-drive-hint-data"), CFSTR("BootCampHD"));
        if(ret) return ret;     
        
        ret = setit(context, kIOMasterPortDefault, kIONVRAMDeletePropertyKey, CFSTR("efi-legacy-drive-hint"));    
        if(ret) return ret;
        
    }
    
    
    ret = BLCreateEFIXMLRepresentationForLegacyDevice(context,
                                                      sb.f_mntfromname + 5,
                                                      &xmlString);
    if(ret) {
        return 1;
    }
    
    if(bootNext) {
        bootString = "efi-boot-next";
    } else {
        bootString = "efi-boot-device";
    }
    
    ret = setit(context, kIOMasterPortDefault, bootString, xmlString);
    CFRelease(xmlString);
    if(ret) {
        return 2;
    }        
    
	ret = efinvramcleanup(context);
	if(ret) return ret;
    
    return 0;
}

