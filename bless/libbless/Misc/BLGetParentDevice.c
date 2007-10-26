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
 *  BLGetParentDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Jun 25 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLGetParentDevice.c,v 1.19 2006/02/20 22:49:56 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

#import <mach/mach_error.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOPartitionScheme.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

int BLGetParentDevice(BLContextPtr context,  const char * partitionDev,
		      char * parentDev,
		      uint32_t *partitionNum) {

    return BLGetParentDeviceAndPartitionType(context, partitionDev, parentDev, partitionNum, NULL);
}

    
int BLGetParentDeviceAndPartitionType(BLContextPtr context,   const char * partitionDev,
			 char * parentDev,
			 uint32_t *partitionNum,
			BLPartitionType *partitionType) {

    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    io_iterator_t           services;
    io_iterator_t           parents;
    io_registry_entry_t service;
    io_iterator_t           grandparents;
    io_registry_entry_t service2;
    io_object_t             obj;

    char par[MNAMELEN];

    parentDev[0] = '\0';

    // Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
      return 2;
    }

    kret = IOServiceGetMatchingServices(ourIOKitPort,
					IOBSDNameMatching(ourIOKitPort,
							  0,
							  (char *)partitionDev + 5),
					&services);
    if (kret != KERN_SUCCESS) {
      return 3;
    }

    // Should only be one IOKit object for this volume. (And we only want one.)
    obj = IOIteratorNext(services);
    if (!obj) {
      return 4;
    }

    // we have the IOMedia for the partition.

    {
      CFNumberRef pn = NULL;
      pn = (CFNumberRef)IORegistryEntryCreateCFProperty(obj, CFSTR(kIOMediaPartitionIDKey),
					   kCFAllocatorDefault, 0);

      if(pn == NULL) {
	return 4;
      }

      if(CFGetTypeID(pn) != CFNumberGetTypeID()) {
	CFRelease(pn);
	return 5;
      }

      CFNumberGetValue(pn, kCFNumberSInt32Type, partitionNum);
      CFRelease(pn);
    }

    kret = IORegistryEntryGetParentIterator (obj, kIOServicePlane,
					       &parents);
    if (kret) {
      return 6;
      /* We'll never loop forever. */
    }

    while ( (service = IOIteratorNext(parents)) != 0 ) {

        kret = IORegistryEntryGetParentIterator (service, kIOServicePlane,
                                                &grandparents);
        if (kret) {
        return 6;
        /* We'll never loop forever. */
        }

        while ( (service2 = IOIteratorNext(grandparents)) != 0 ) {
        
            CFStringRef content = NULL;
        
            if (!IOObjectConformsTo(service2, "IOMedia")) {
                continue;
            }
        
            content = (CFStringRef)
                IORegistryEntryCreateCFProperty(service2,
                                                CFSTR(kIOMediaContentKey),
                                                kCFAllocatorDefault, 0);
            
            
            if(CFGetTypeID(content) != CFStringGetTypeID()) {
                CFRelease(content);
                return 2;
            }
            
            if(CFStringCompare(content, CFSTR("Apple_partition_scheme"), 0)
               == kCFCompareEqualTo) {
                if(partitionType) *partitionType = kBLPartitionType_APM;
            } else if(CFStringCompare(content, CFSTR("FDisk_partition_scheme"), 0)
                      == kCFCompareEqualTo) {
                if(partitionType) *partitionType = kBLPartitionType_MBR;
            } else if(CFStringCompare(content, CFSTR("GUID_partition_scheme"), 0)
                      == kCFCompareEqualTo) {
                if(partitionType) *partitionType = kBLPartitionType_GPT;
            } else {
                CFRelease(content);
                continue;
            }
            
            CFRelease(content);
        
            content = IORegistryEntryCreateCFProperty(service2, CFSTR(kIOBSDNameKey),
                                                        kCFAllocatorDefault, 0);
        
        
            if(CFGetTypeID(content) != CFStringGetTypeID()) {
                CFRelease(content);
                return 3;
            }
        
            if(!CFStringGetCString(content, par, MNAMELEN, kCFStringEncodingASCII)) {
                CFRelease(content);
                return 4;
            }

	    CFRelease(content);
    
            sprintf(parentDev, "/dev/%s",par);
            break;
        }

	if(parentDev[0] == '\0') {
	    break;
	}
    }

    if(parentDev[0] == '\0') {
      // nothing found
      return 8;
    }

    return 0;
}
