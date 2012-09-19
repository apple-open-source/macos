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
 *  BLUpdateRAIDBooters.c
 *  bless
 *
 *  Created by Shantonu Sen on 1/15/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLUpdateRAIDBooters.c,v 1.12 2006/02/20 22:49:58 ssen Exp $
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>

#include <mach/mach_error.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/storage/IOMedia.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

#if SUPPORT_RAID

#include <IOKit/storage/RAID/AppleRAIDUserLib.h>

#if USE_DISKARBITRATION
#include <DiskArbitration/DiskArbitration.h>
#endif

#define kBootPlistName "com.apple.Boot.plist"


int updateRAIDMember(BLContextPtr context, mach_port_t iokitPort, 
					 CFDictionaryRef raidEntry, CFDataRef opaqueData,
					 CFDataRef bootxData, CFDataRef labelData);
int getExternalBooter(BLContextPtr context,
                      mach_port_t iokitPort,
					  io_service_t dataPartition,
					  io_service_t *booterPartition);

int updateAppleBoot(BLContextPtr context, const char *devname, CFDataRef opaqueData,
					CFDataRef bootxData, CFDataRef labelData);

CFDataRef _createLabel(BLContextPtr context, CFStringRef name, int index);

int BLUpdateRAIDBooters(BLContextPtr context, const char * device,
						CFTypeRef bootData,
						CFDataRef bootxData, CFDataRef labelData)
{
	int ret = 0;
	int anyFailed = 0;
	CFDataRef xmlData = NULL;
	CFStringRef		daName = NULL;
	
	kern_return_t           kret;
	mach_port_t             ourIOKitPort;
	
	
	xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault, bootData);
	if(xmlData == NULL) {
		contextprintf(context, kBLLogLevelError,  "Error generating XML data\n");
		return 1;
	}

	contextprintf(context, kBLLogLevelVerbose,  "RAID XML data is:\n%s\n",
				  CFDataGetBytePtr(xmlData));

	
	// Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
		return 2;
    }
	
	for(;;) {
		// try to get a symbolic name from DA, if possible
		if(labelData == NULL) {
#if USE_DISKARBITRATION
			DADiskRef disk = NULL;
			DASessionRef session = NULL;
			CFDictionaryRef props = NULL;
			
			session = DASessionCreate(kCFAllocatorDefault);
			if(session == NULL) {
				contextprintf(context, kBLLogLevelVerbose, "Can't connect to DiskArb\n");
				break;
			}
			
			disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, device+5);
			if(disk == NULL) {
				CFRelease(session);
				contextprintf(context, kBLLogLevelVerbose, "Can't create DADisk for %s\n",
							  device + 5);
				break;
			}
			
			props = DADiskCopyDescription(disk);
			if(props == NULL) {
				CFRelease(session);
				CFRelease(disk);
				contextprintf(context, kBLLogLevelVerbose, "Can't get properties for %s\n",
							  device + 5);
				break;
			}
			
			daName = CFDictionaryGetValue(props, kDADiskDescriptionVolumeNameKey);
			if(daName) CFRetain(daName);
			
			CFRelease(props);
			CFRelease(disk);
			CFRelease(session);			
#else // !USE_DISKARBITRATION
			daName = CFSTR("RAID");
#endif // !USE_DISKARBITRATION
		}
		break;
	}
	
	
	if(CFGetTypeID(bootData) == CFArrayGetTypeID()) {
		CFIndex i, count = CFArrayGetCount(bootData);
		
		for(i=0; i < count; i++) {
			CFDictionaryRef dict = CFArrayGetValueAtIndex(bootData,i);
			CFDataRef	tempLabel = NULL;
			
			if(!labelData && daName) {
				tempLabel = _createLabel(context, daName, i+1);
			}
			
			ret = updateRAIDMember(context, ourIOKitPort, dict, xmlData,
								   bootxData, labelData ? labelData : tempLabel);
			if(ret) {
				anyFailed = 1;
				// keep going to update other booters
			}
			
			if(tempLabel) CFRelease(tempLabel);
		}
	} else {
		CFDataRef	tempLabel = NULL;
		
		if(!labelData && daName) {
			tempLabel = _createLabel(context, daName, 1);
		}

		ret = updateRAIDMember(context, ourIOKitPort,
							   (CFDictionaryRef)bootData, xmlData,
							   bootxData, labelData ? labelData : tempLabel);
		if(ret) {
			anyFailed = 1;
			// keep going to update other booters
		}		

		if(tempLabel) CFRelease(tempLabel);
}
	CFRelease(xmlData);

	if(anyFailed) {
		return 1;
	} else {
		return 0;
	}
}

int updateRAIDMember(BLContextPtr context, mach_port_t iokitPort,
					 CFDictionaryRef raidEntry, CFDataRef opaqueData,
					 CFDataRef bootxData, CFDataRef labelData)
{
    int ret;
    CFStringRef path;
    io_string_t	cpath;
    io_service_t	service;
    io_service_t	booter;
    
    CFStringRef name = NULL;
    CFStringRef content = NULL;
    char cname[MAXPATHLEN];
    
    
    path = CFDictionaryGetValue(raidEntry, CFSTR(kIOBootDevicePathKey));
    if(path == NULL) return 1;
    
    if(!CFStringGetCString(path,cpath,sizeof(cpath),kCFStringEncodingUTF8))
        return 2;
    
    contextprintf(context, kBLLogLevelVerbose,  "Updating booter data for %s\n", cpath);
    
    service = IORegistryEntryFromPath(iokitPort, cpath);
    if(service == 0) {
        contextprintf(context, kBLLogLevelVerbose,  "Could not find service for \"%s\"\n", cpath);
        return 3;
    }

    content = (CFStringRef)IORegistryEntryCreateCFProperty(
                                                        service,
                                                        CFSTR(kIOMediaContentKey),
                                                        kCFAllocatorDefault,
                                                        0);
    
    if(content) {
        if(CFStringGetTypeID() == CFGetTypeID(content)
           && CFEqual(content, CFSTR("Apple_Boot_RAID"))) {
         
            contextprintf(context, kBLLogLevelVerbose,  "Member at \"%s\" is RAIDv1 Apple_Boot_RAID partition. Ignoring...\n", cpath);
            
            CFRelease(content);
            IOObjectRelease(service);
            return 0;
        }
        
        CFRelease(content);
    }
    
    ret = getExternalBooter(context, iokitPort, service, &booter);
    if(ret) {
        IOObjectRelease(service);
        return 4;
    }
    
    name = (CFStringRef)IORegistryEntryCreateCFProperty(
                                                        booter,
                                                        CFSTR(kIOBSDNameKey),
                                                        kCFAllocatorDefault,
                                                        0);
    
    if(!CFStringGetCString(name, cname, sizeof(cname), kCFStringEncodingUTF8)) {
        CFRelease(name);
        IOObjectRelease(booter);
        IOObjectRelease(service);
        return 5;
    }
    
    CFRelease(name);
    
    contextprintf(context, kBLLogLevelVerbose,  "Booter partition is %s\n", cname);	
    ret = updateAppleBoot(context, cname, opaqueData,
						  bootxData, labelData);
    if(ret) {
        IOObjectRelease(booter);
        IOObjectRelease(service);
        return 6;
    }
    
    IOObjectRelease(booter);
    IOObjectRelease(service);
    
    return 0;
}

int updateAppleBoot(BLContextPtr context, const char *devname, CFDataRef opaqueData,
					CFDataRef bootxData, CFDataRef labelData)
{
	
    int         status;
	char			device[MAXPATHLEN];
	BLUpdateBooterFileSpec	*spec = NULL;
	BLUpdateBooterFileSpec	*pspec, *xspec, *lspec1, *lspec2;
	int32_t					specCount = 0, currentCount = 0;
	
	pspec = xspec = lspec1 = lspec2 = NULL;
	
	sprintf(device, "/dev/%s", devname);

	specCount = 1; // plist
	if(labelData) specCount += 2;
	if(bootxData) specCount += 1;

	spec = calloc(specCount, sizeof(spec[0]));
	
	
	pspec = &spec[0];
	
	pspec->version = 0;
	pspec->reqType = 0; // no type
	pspec->reqCreator = 0; // no type
	pspec->reqParentDir = 0;
	pspec->reqFilename = kBootPlistName;
	pspec->payloadData = opaqueData;
	pspec->postType = 0; // no type
	pspec->postCreator = 0; // no type
	pspec->foundFile = 0;
	pspec->updatedFile = 0;

	currentCount++;
	
	if(labelData) {
		lspec1 = &spec[currentCount];
		lspec2 = &spec[currentCount+1];
		
		lspec1->version = 0;
		lspec1->reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
		lspec1->reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		lspec1->reqParentDir = 0;
		lspec1->reqFilename = NULL;
		lspec1->payloadData = labelData;
		lspec1->postType = 0; // no type
		lspec1->postCreator = 0; // no type
		lspec1->foundFile = 0;
		lspec1->updatedFile = 0;
		
		lspec2->version = 0;
		lspec2->reqType = kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER;
		lspec2->reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		lspec2->reqParentDir = 0;
		lspec2->reqFilename = NULL;
		lspec2->payloadData = labelData;
		lspec2->postType = kBL_OSTYPE_PPC_TYPE_OFLABEL;
		lspec2->postCreator = 0; // no type
		lspec2->foundFile = 0;
		lspec2->updatedFile = 0;
		
		
		currentCount += 2;
	}
	
	if(bootxData) {
		xspec = &spec[currentCount];
		
		xspec->version = 0;
		xspec->reqType = kBL_OSTYPE_PPC_TYPE_BOOTX;
		xspec->reqCreator = kBL_OSTYPE_PPC_CREATOR_CHRP;
		xspec->reqParentDir = 0;
		xspec->reqFilename = NULL;
		xspec->payloadData = bootxData;
		xspec->postType = 0; // no type
		xspec->postCreator = 0; // no type
		xspec->foundFile = 0;
		xspec->updatedFile = 0;
		
		currentCount++;		
	}
	
	status = BLUpdateBooter(context, device, spec, specCount);
	if(status) {
		contextprintf(context, kBLLogLevelError,  "Error enumerating HFS+ volume\n");		
		return 1;
	}
	
	if(bootxData) {
		if(!(xspec->foundFile)) {
			contextprintf(context, kBLLogLevelError,  "No pre-existing BootX found in HFS+ volume\n");
			return 2;
		}			
		
		if(!(xspec->updatedFile)) {
			contextprintf(context, kBLLogLevelError,  "BootX was not updated\n");
			return 3;
		}			
	}
	
    if(labelData) {
		if(!(lspec1->foundFile || lspec2->foundFile)) {
			contextprintf(context, kBLLogLevelError,  "No pre-existing OF label found in HFS+ volume\n");
			return 2;
		}
		if(!(lspec1->updatedFile || lspec2->updatedFile)) {
			contextprintf(context, kBLLogLevelError,  "OF label was not updated\n");
			return 3;
		}
	}

    if(!pspec->foundFile) {
		contextprintf(context, kBLLogLevelError,  "No pre-existing Plist found in HFS+ volume\n");
		return 1;
    }
    if(!pspec->updatedFile) {
		contextprintf(context, kBLLogLevelError,  "Plist was not updated\n");
		return 2;
    }

    return 0;
}

CFDataRef _createLabel(BLContextPtr context, CFStringRef name, int index)
{
	CFDataRef newLabel = NULL;
	char label[MAXPATHLEN];
	int ret;
	CFStringRef nameWithNum = NULL;
	
	nameWithNum = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,
										   CFSTR("%@ %d"), name, index);
	if(nameWithNum == NULL)
	   return NULL;
	
	if(!CFStringGetCString(nameWithNum, label, sizeof(label),
						   kCFStringEncodingUTF8)) {
		CFRelease(nameWithNum);
		return NULL;
	}
	
	CFRelease(nameWithNum);

	ret = BLGenerateLabelData(context, label, kBitmapScale_1x, &newLabel);
	
	if(ret) {
		return NULL;
	} else {
		return newLabel;
	}
}

#endif // SUPPORT_RAID
