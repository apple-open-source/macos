/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  BLGetDeviceForOpenFirmwarePath.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jan 24 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLGetDeviceForOpenFirmwarePath.c,v 1.22 2006/02/20 22:49:57 ssen Exp $
 *
 */
 
#import <mach/mach_error.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/storage/IOMedia.h>

#import <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <sys/stat.h>
 
#include "bless.h"
#include "bless_private.h"

#undef SUPPORT_RAID
#define SUPPORT_RAID 0

#if SUPPORT_RAID
#import <IOKit/storage/RAID/AppleRAIDUserLib.h>

static int findRAIDForMember(BLContextPtr context, mach_port_t iokitPort, char * mntfrm);
static int isRAIDPath(BLContextPtr context, mach_port_t iokitPort, io_service_t member,
					  CFDictionaryRef raidEntry);
#endif

extern int getPNameAndPType(BLContextPtr context,
			    char * target,
			    char * pname,
			    char * ptype);


int BLGetDeviceForOpenFirmwarePath(BLContextPtr context, const char * ofstring, char * mntfrm) {
	
    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    char newof[1024];
    char targetPName[MAXPATHLEN];
    char targetPType[MAXPATHLEN];
    int ret;
    
    char *comma = NULL;;
	
    strcpy(newof, ofstring);
	
    comma = strrchr(newof, ',');
    if(comma == NULL) { // there should be a ,\\:tbxi
        return 1;
    }
	
    // strip off the booter path
    *comma = '\0';
	
	// Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
		return 2;
    }
	
    strcpy(mntfrm, "/dev/");
	
    kret = IOServiceOFPathToBSDName(ourIOKitPort,
									newof,
									mntfrm + 5);
	
    contextprintf(context, kBLLogLevelVerbose,  "bsd name is %s\n", mntfrm );
	
    ret = getPNameAndPType(context, mntfrm+5, targetPName, targetPType);
    if(ret) {
		contextprintf(context, kBLLogLevelVerbose,  "Could not get partition type for %s\n", mntfrm );
		
		return 3;
    }
    
    contextprintf(context, kBLLogLevelVerbose,  "Partition name is %s. Partition type is %s\n",  targetPName, targetPType);
    
#if 0
    
    if(strcmp("Apple_Boot", targetPType) != 0 && strcmp("Apple_Boot_RAID", targetPType) != 0) {
		contextprintf(context, kBLLogLevelVerbose,  "No external booter needed\n");
        return 0;
        
    }
    
    char parentDev[MNAMELEN];
    uint32_t slice = 0;
    
    BLPartitionType partitionType = 0;
    
    // this is a auxiliary booter partition
    contextprintf(context, kBLLogLevelVerbose,  "Looking for root partition\n");
    
    
    if(strcmp("Apple_Boot", targetPType) == 0) {
        ret = BLGetParentDeviceAndPartitionType(context,mntfrm,
                                                parentDev,
                                                &slice,
                                                &partitionType);
        
        if(ret) {
            contextprintf(context, kBLLogLevelVerbose,  "Could not get information about partition map for %s\n", mntfrm);
            return 3;
            
        }
        
        slice += 1; // n+1 for "real" root partition
        sprintf(mntfrm, "%ss%u", parentDev, slice);

        contextprintf(context, kBLLogLevelVerbose,  "Now looking at %s\n", mntfrm);
    }
    
    ret = getPNameAndPType(context, mntfrm+5, targetPName, targetPType);
    if(ret) {
        contextprintf(context, kBLLogLevelVerbose,  "Could not get partition type for %s\n", mntfrm );
        
        return 3;
    }
    
    contextprintf(context, kBLLogLevelVerbose,  "Partition name is %s. Partition type is %s\n",  targetPName, targetPType);
    
    if(strcmp("Apple_Boot", targetPType) == 0) {
        contextprintf(context, kBLLogLevelError,  "Apple_Boot followed by another Apple_Boot\n");
        return 4;
    }
    
    if(strcmp("Apple_HFS", targetPType) == 0) {
        contextprintf(context, kBLLogLevelError,  "Apple_HFS does not require an Apple_Boot\n");
        return 4;
    }

#if SUPPORT_RAID
    if(strcmp("Apple_RAID", targetPType) == 0 || strcmp("Apple_Boot_RAID", targetPType) == 0) {
        contextprintf(context, kBLLogLevelVerbose,  "%s is an Apple_RAID. Looking for exported volume\n", mntfrm);
        ret = findRAIDForMember(context, ourIOKitPort, mntfrm);
        if(ret) {
            contextprintf(context, kBLLogLevelError,  "Couldn't find appropriate RAID volume for %s\n", mntfrm);
            return 4;
        }				
    }
#endif
    
#endif // 0
    
    return 0;
	
}

#if SUPPORT_RAID

static int findRAIDForMember(BLContextPtr context, mach_port_t iokitPort, char * mntfrm)
{
	kern_return_t           kret;
	io_service_t			member;
	io_iterator_t			raiditer;
	io_service_t			raidservice;
	CFMutableDictionaryRef	matching = NULL;
	CFMutableDictionaryRef	props = NULL;
	int						foundRAID = 0;
	int						errnum;

	errnum = BLGetIOServiceForDeviceName(context, mntfrm+5, &member);
	if(errnum) {
		contextprintf(context, kBLLogLevelError,  "Could not find IOKit entry for %s\n" , mntfrm+5);
		return 4;
	}
	
	contextprintf(context, kBLLogLevelVerbose,  "Found service for %s\n", mntfrm+5);
	
	matching = IOServiceMatching(kIOMediaClass);
	
	props = CFDictionaryCreateMutable(kCFAllocatorDefault,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(props, CFSTR(kAppleRAIDIsRAIDKey), kCFBooleanTrue);
	CFDictionaryAddValue(props, CFSTR(kIOMediaLeafKey), kCFBooleanTrue);
	
	CFDictionaryAddValue(matching, CFSTR(kIOPropertyMatchKey), props);
	CFRelease(props);
	
	kret = IOServiceGetMatchingServices(iokitPort, matching, &raiditer);
	if(kret != KERN_SUCCESS) {
		contextprintf(context, kBLLogLevelVerbose,  "Could find any RAID devices on the system\n");
		return 2;
	}
	
	while((raidservice = IOIteratorNext(raiditer))) {
//		CFMutableDictionaryRef rprops = NULL;
		CFTypeRef			data = NULL;

		/*
		kret = IORegistryEntryCreateCFProperties(raidservice, &rprops, kCFAllocatorDefault, 0);
		if(kret == KERN_SUCCESS) {
			CFShow(rprops);
			CFRelease(rprops);
		}
*/
		contextprintf(context, kBLLogLevelVerbose,  "Checking if member is part of RAID %x\n", raidservice);
		
		// we know this IOService is a RAID member. Now we need to get the boot data
		data = IORegistryEntrySearchCFProperty( raidservice,
												kIOServicePlane,
												CFSTR(kIOBootDeviceKey),
												kCFAllocatorDefault,
												kIORegistryIterateRecursively|
												kIORegistryIterateParents);
		if(data == NULL) {
			// it's an error for a RAID not to have this information
			IOObjectRelease(raidservice);
			raidservice = 0;
			continue;
		}
		
		if(CFGetTypeID(data) == CFArrayGetTypeID()) {
			CFIndex i, count = CFArrayGetCount(data);
			for(i=0; i < count && !foundRAID; i++) {
				CFDictionaryRef ent = CFArrayGetValueAtIndex((CFArrayRef)data,i);
				if(isRAIDPath(context, iokitPort, member, ent)) {
					foundRAID = 1;
					CFRelease(data);
					break;
				}
			}
			if(foundRAID) break;
			
		} else if(CFGetTypeID(data) == CFDictionaryGetTypeID()) {
			if(isRAIDPath(context, iokitPort, member, (CFDictionaryRef)data)) {
				foundRAID = 1;
				CFRelease(data);
				break;
			}
		} else {
			contextprintf(context, kBLLogLevelError,  "Invalid RAID boot data\n" );
			CFRelease(data);
			IOObjectRelease(raidservice);
			raidservice = 0;
			continue;
		}
		
		
		IOObjectRelease(raidservice);
	}
	
	IOObjectRelease(raiditer);
	
	if(foundRAID) {
		// sweet. We found the RAID device corresponding to this member.
		// replace the dev node with it
		CFStringRef name = (CFStringRef)IORegistryEntryCreateCFProperty(
															raidservice,
															CFSTR(kIOBSDNameKey),
															kCFAllocatorDefault,
															0);
		
		if(!CFStringGetCString(name, mntfrm+5, MAXPATHLEN-5, kCFStringEncodingUTF8)) {
			CFRelease(name);
			IOObjectRelease(raidservice);
			return 1;
		}

		CFRelease(name);
		IOObjectRelease(raidservice);

		contextprintf(context, kBLLogLevelVerbose,  "RAID device is %s\n", mntfrm );

		return 0;
	}
	
	// no RAID found
	return 5;
}

static int isRAIDPath(BLContextPtr context, mach_port_t iokitPort, io_service_t member,
					  CFDictionaryRef raidEntry)
{
	CFStringRef path;
	io_string_t	cpath;
	io_service_t	service;
	
	path = CFDictionaryGetValue(raidEntry, CFSTR(kIOBootDevicePathKey));
	if(path == NULL) return 0;

	if(!CFStringGetCString(path,cpath,sizeof(cpath),kCFStringEncodingUTF8))
		return 0;

	contextprintf(context, kBLLogLevelVerbose,  "Comparing member to %s", cpath);
	
	service = IORegistryEntryFromPath(iokitPort, cpath);
	if(service == 0) {
		contextprintf(context, kBLLogLevelVerbose,  "\nCould not find service\n");
		return 0;
	}
	
	if(IOObjectIsEqualTo(service, member)) {
		contextprintf(context, kBLLogLevelVerbose,  "\tEQUAL\n");
		IOObjectRelease(service);
		return 1;
	} else {
		contextprintf(context, kBLLogLevelVerbose,  "\tNOT EQUAL\n");		
	}
	
	IOObjectRelease(service);	
	
	return 0;
}

#endif // SUPPORT_RAID
