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
 *  BLGetOpenFirmwareBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLGetOpenFirmwareBootDevice.c,v 1.30 2006/02/20 22:49:57 ssen Exp $
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>

#import <mach/mach_error.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOMediaBSDClient.h>
#import <IOKit/storage/IOPartitionScheme.h>

#import <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

int getPNameAndPType(BLContextPtr context,
                            char * target,
			    char * pname,
			    char * ptype);

int getExternalBooter(BLContextPtr context,
                      mach_port_t iokitPort,
					  io_service_t dataPartition,
 					 io_service_t *booterPartition);

/*
 * Get OF string for device
 * If it's a non-whole device
 *   Look for an external booter
 *     If there is, replace the partition number in OF string
 * For new world, add a tbxi
 */

int BLGetOpenFirmwareBootDevice(BLContextPtr context, const char * mntfrm, char * ofstring) {

    int err;

    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    io_service_t            service;

	int32_t					needsBooter	= 0;
	int32_t					isBooter	= 0;
		
    io_string_t				ofpath;
    char			device[MAXPATHLEN];
	char *split = ofpath;
	char tbxi[5];
    
    CFTypeRef               bootData = NULL;

    if(!mntfrm || 0 != strncmp(mntfrm, "/dev/", 5)) return 1;

	strlcpy(device, mntfrm, sizeof device);
    
    // Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
      return 2;
    }

#if SUPPORT_RAID
    err = BLGetRAIDBootDataForDevice(context, mntfrm, &bootData);
    if(err) {
        contextprintf(context, kBLLogLevelError,  "Error while determining if %s is a RAID\n", mntfrm );
        return 3;
    }
#endif
    
    if(bootData) {
        CFDictionaryRef primary = NULL;
        CFStringRef bootpath = NULL;
		CFStringRef name = NULL;
        io_string_t iostring;
        
        // update name with the primary partition
        if(CFGetTypeID(bootData) == CFArrayGetTypeID() ) {
			if(CFArrayGetCount(bootData) == 0) {
				contextprintf(context, kBLLogLevelError,  "RAID set has no bootable members\n" );
				return 3;				
			}
			
            primary = CFArrayGetValueAtIndex(bootData,0);
            CFRetain(primary);
        } else if(CFGetTypeID(bootData) == CFDictionaryGetTypeID()) {
            primary = bootData;
            CFRetain(primary);
        }
        
        bootpath = CFDictionaryGetValue(primary, CFSTR(kIOBootDevicePathKey));
        if(bootpath == NULL || CFGetTypeID(bootpath) != CFStringGetTypeID()) {
            CFRelease(primary);
            CFRelease(bootData);
            contextprintf(context, kBLLogLevelError,  "Could not find boot path entry for %s\n" , mntfrm);
            return 4;            
        }
        
        if(!CFStringGetCString(bootpath,iostring,sizeof(iostring),kCFStringEncodingUTF8)) {
            CFRelease(primary);
            CFRelease(bootData);
            contextprintf(context, kBLLogLevelError,  "Invalid UTF8 for path entry for %s\n" , mntfrm);
            return 4;                        
        }

		contextprintf(context, kBLLogLevelVerbose,  "Primary OF boot path is %s\n" , iostring);
        
        service = IORegistryEntryFromPath(ourIOKitPort, iostring );
        if(service == 0) {
            CFRelease(primary);
            CFRelease(bootData);
            contextprintf(context, kBLLogLevelError,  "Could not find IOKit entry for %s\n" , iostring);
            return 4;                                    
        }

        CFRelease(primary);
        CFRelease(bootData);
		
		name = IORegistryEntryCreateCFProperty( service, CFSTR(kIOBSDNameKey),
												kCFAllocatorDefault, 0);

		if(name == NULL || CFStringGetTypeID() != CFGetTypeID(name)) {
			IOObjectRelease(service);
            contextprintf(context, kBLLogLevelError,  "Could not find bsd name for %s\n" , iostring);
			return 5;
		}

		IOObjectRelease(service); service = 0;
		
		if(!CFStringGetCString(name,device+5,sizeof(iostring)-5,kCFStringEncodingUTF8)) {
			CFRelease(name);
            contextprintf(context, kBLLogLevelError,  "Could not find bsd name for %s\n" , iostring);
			return 5;
		}
		
		CFRelease(name);
    }
    
    // by this point, "service" should point at the data partition, or potentially
    // a RAID member. We'll need to map it to a booter partition if necessary

	err = BLDeviceNeedsBooter(context, device,
							  &needsBooter,
							  &isBooter,
							  &service);
	if(err) {
		contextprintf(context, kBLLogLevelError,  "Could not determine if partition needs booter\n" );		
		return 10;
	}
	
	if(!needsBooter && !isBooter) {
		err = BLGetIOServiceForDeviceName(context, (char *)device + 5, &service);
		if(err) {
			contextprintf(context, kBLLogLevelError,  "Can't find IOService for %s\n", device + 5 );
			return 10;		
		}
		
	}
	
	kret = IORegistryEntryGetPath(service, kIODeviceTreePlane, ofpath);
	if(kret != KERN_SUCCESS) {
		contextprintf(context, kBLLogLevelError,  "Could not get path in device plane for service\n" );
		IOObjectRelease(service);
		return 11;
	}

	IOObjectRelease(service);

	split = ofpath;

	strsep(&split, ":");
	
	if(split == NULL) {
		contextprintf(context, kBLLogLevelError,  "Bad path in device plane for service\n" );
		IOObjectRelease(service);
		return 11;		
	}
	
	sprintf(ofstring, "%s,\\\\:%s", split, blostype2string(kBL_OSTYPE_PPC_TYPE_BOOTX, tbxi));
	
    return 0;
}



int getPNameAndPType(BLContextPtr context,
                     char * target,
		     char * pname,
		     char * ptype) {

  kern_return_t       status;
  mach_port_t         ourIOKitPort;
  io_iterator_t       services;
  io_registry_entry_t obj;
    CFStringRef temp = NULL;

  pname[0] = '\0';
  ptype[0] = '\0';

  // Obtain the I/O Kit communication handle.
  status = IOMasterPort(bootstrap_port, &ourIOKitPort);
  if (status != KERN_SUCCESS) {
    return 1;
  }

  // Obtain our list of one object
  // +5 to skip over "/dev/"
  status = IOServiceGetMatchingServices(ourIOKitPort,
					IOBSDNameMatching(ourIOKitPort,
							  0,
							  target),
					&services);
  if (status != KERN_SUCCESS) {
    return 2;
  }

  // Should only be one IOKit object for this volume. (And we only want one.)
  obj = IOIteratorNext(services);
  if (!obj) {
    return 3;
  }

    temp = (CFStringRef)IORegistryEntryCreateCFProperty(
	obj,
	CFSTR(kIOMediaContentKey),
        kCFAllocatorDefault,
	0);

  if (temp == NULL) {
    return 4;
  }

    if(!CFStringGetCString(temp, ptype, MAXPATHLEN, kCFStringEncodingMacRoman)) {
        CFRelease(temp);
        return 4;
    }

    CFRelease(temp);
    
    status = IORegistryEntryGetName(obj,pname);
  if (status != KERN_SUCCESS) {
    return 5;
  }


  
  IOObjectRelease(obj);
  obj = 0;

  IOObjectRelease(services);
  services = 0;

    contextprintf(context, kBLLogLevelVerbose,  "looking at partition %s, type %s, name %s\n", target, ptype, pname );

  return 0;

}

int getExternalBooter(BLContextPtr context,
                      mach_port_t iokitPort,
					  io_service_t dataPartition,
					  io_service_t *booterPartition)
{
	CFStringRef name = NULL;
	CFStringRef content = NULL;
	char cname[MAXPATHLEN];
	char *spos = NULL;
	io_service_t booter = 0;
	int partnum = 0;
	int errnum;
	
	name = (CFStringRef)IORegistryEntryCreateCFProperty(
														dataPartition,
														CFSTR(kIOBSDNameKey),
														kCFAllocatorDefault,
														0);
	
	if(!CFStringGetCString(name, cname, sizeof(cname), kCFStringEncodingUTF8)) {
        CFRelease(name);
        return 1;
    }

	CFRelease(name);
	
	spos = strrchr(cname, 's');
	if(spos == NULL || spos == &cname[2]) {
		contextprintf(context, kBLLogLevelError,  "Can't determine partition for %s\n", cname );
		return 2;
	}
	
	partnum = atoi(spos+1);
	sprintf(spos, "s%d", partnum-1);
	
	errnum = BLGetIOServiceForDeviceName(context, cname, &booter);
	if(errnum) {
		contextprintf(context, kBLLogLevelError,  "Could not find IOKit entry for %s\n" , cname);
		return 4;
	}
	
	content = (CFStringRef)IORegistryEntryCreateCFProperty(
														booter,
														CFSTR(kIOMediaContentKey),
														kCFAllocatorDefault,
														0);
	if(content == NULL || CFGetTypeID(content) != CFStringGetTypeID()) {
		contextprintf(context, kBLLogLevelError,  "Invalid content type for %s\n" , cname);
		IOObjectRelease(booter);
		return 5;		
	}
	
	if(!CFEqual(CFSTR("Apple_Boot"), content)) {
		contextprintf(context, kBLLogLevelError,  "Booter partition %s is not Apple_Boot\n" , cname);
		IOObjectRelease(booter);
		return 6;
	}
	
	*booterPartition = booter;
	return 0;
}
