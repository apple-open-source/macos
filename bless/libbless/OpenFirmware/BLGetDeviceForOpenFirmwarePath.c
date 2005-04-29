/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetDeviceForOpenFirmwarePath.c,v 1.18 2005/02/07 21:22:38 ssen Exp $
 *
 *  $Log: BLGetDeviceForOpenFirmwarePath.c,v $
 *  Revision 1.18  2005/02/07 21:22:38  ssen
 *  Refact lookupServiceForName and code for BLDeviceNeedsBooter
 *
 *  Revision 1.17  2005/02/03 00:42:29  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.16  2005/01/25 19:38:38  ssen
 *  Fix -getBoot functionality so that we can find the RAID for
 *  an Apple_Boot_RAID
 *
 *  Revision 1.15  2005/01/16 00:10:12  ssen
 *  <rdar://problem/3861859> bless needs to try getProperty(kIOBootDeviceKey)
 *  Implement -getBoot and -info functionality. If boot-device is
 *  set to the Apple_Boot for one of the RAID members, we map
 *  this back to the top-level RAID device and print that out. This
 *  enables support in Startup Disk
 *
 *  Revision 1.14  2004/04/20 21:40:45  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.13  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.12  2003/07/25 01:16:25  ssen
 *  When mapping OF -> device, if we found an Apple_Boot, try to
 *  find the corresponding partition that is the real root filesystem
 *
 *  Revision 1.11  2003/07/22 15:58:36  ssen
 *  APSL 2.0
 *
 *  Revision 1.10  2003/04/19 00:11:14  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.9  2003/04/16 23:57:35  ssen
 *  Update Copyrights
 *
 *  Revision 1.8  2002/08/22 04:18:07  ssen
 *  typo
 *
 *  Revision 1.7  2002/08/22 00:38:42  ssen
 *  Gah. Search for ",\\:tbxi" from the end of the OF path
 *  instead of the beginning. For SCSI cards that use commas
 *  in the OF path, the search was causing a mis-parse.
 *
 *  Revision 1.6  2002/06/11 00:50:51  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.5  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.4  2002/04/25 07:27:30  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.3  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2002/02/03 17:25:22  ssen
 *  Fix "bless -info" usage to determine current device
 *
 *  Revision 1.1  2002/02/03 17:02:35  ssen
 *  Add of -> dev function
 *
 */
 
#import <mach/mach_error.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/RAID/AppleRAIDUserLib.h>


#import <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/mount.h>

#include <sys/stat.h>
 
#include "bless.h"
#include "bless_private.h"

extern int getPNameAndPType(BLContextPtr context,
			    unsigned char target[],
			    unsigned char pname[],
			    unsigned char ptype[]);

static int findRAIDForMember(BLContextPtr context, mach_port_t iokitPort, unsigned char mntfrm[]);
static int isRAIDPath(BLContextPtr context, mach_port_t iokitPort, io_service_t member,
					  CFDictionaryRef raidEntry);

int BLGetDeviceForOpenFirmwarePath(BLContextPtr context, const char ofstring[], unsigned char mntfrm[]) {
	
    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    char newof[1024];
    unsigned char targetPName[MAXPATHLEN];
    unsigned char targetPType[MAXPATHLEN];
    unsigned char parentDev[MNAMELEN];
    unsigned long slice = 0;
    int ret;
    
    BLPartitionType partitionType = 0;
    
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
    
    if(strcmp("Apple_Boot", targetPType) != 0 && strcmp("Apple_Boot_RAID", targetPType) != 0) {
		contextprintf(context, kBLLogLevelVerbose,  "No external booter needed\n");
        return 0;
        
    }
    
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
        sprintf(mntfrm, "%ss%lu", parentDev, slice);

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

    if(strcmp("Apple_RAID", targetPType) == 0 || strcmp("Apple_Boot_RAID", targetPType) == 0) {
        contextprintf(context, kBLLogLevelVerbose,  "%s is an Apple_RAID. Looking for exported volume\n", mntfrm);
        ret = findRAIDForMember(context, ourIOKitPort, mntfrm);
        if(ret) {
            contextprintf(context, kBLLogLevelError,  "Couldn't find appropriate RAID volume for %s\n", mntfrm);
            return 4;
        }				
    }
    
    return 0;
	
}

static int findRAIDForMember(BLContextPtr context, mach_port_t iokitPort, unsigned char mntfrm[])
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
