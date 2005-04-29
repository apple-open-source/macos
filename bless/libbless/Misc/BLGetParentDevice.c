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
 *  BLGetParentDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Jun 25 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetParentDevice.c,v 1.14 2005/02/03 00:42:27 ssen Exp $
 *
 *  $Log: BLGetParentDevice.c,v $
 *  Revision 1.14  2005/02/03 00:42:27  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.13  2004/04/20 21:40:44  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.12  2003/10/24 22:46:35  ssen
 *  don't try to compare IOKit objects to NULL, since they are really mach port integers
 *
 *  Revision 1.11  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.10  2003/07/22 22:35:04  ssen
 *  Add function to get pmap type as well as parent device
 *
 *  Revision 1.9  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.8  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.7  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.6  2002/09/24 21:05:46  ssen
 *  Eliminate use of deprecated constants
 *
 *  Revision 1.5  2002/06/11 00:50:49  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.4  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.3  2002/02/04 04:21:28  ssen
 *  Dont freak out with unpartitioned volumes
 *
 *  Revision 1.2  2001/11/17 05:45:02  ssen
 *  fix parent lookup
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.7  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.5  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
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

int BLGetParentDevice(BLContextPtr context,  const unsigned char partitionDev[],
		      unsigned char parentDev[],
		      unsigned long *partitionNum) {

    return BLGetParentDeviceAndPartitionType(context, partitionDev, parentDev, partitionNum, NULL);
}

    
int BLGetParentDeviceAndPartitionType(BLContextPtr context,   const unsigned char partitionDev[],
			 unsigned char parentDev[],
			 unsigned long *partitionNum,
			BLPartitionType *partitionType) {

    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    io_iterator_t           services;
    io_iterator_t           parents;
    io_registry_entry_t service;
    io_iterator_t           grandparents;
    io_registry_entry_t service2;
    io_object_t             obj;

    unsigned char par[MNAMELEN];

    parentDev[0] = '\0';

    // Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
      return 2;
    }

    kret = IOServiceGetMatchingServices(ourIOKitPort,
					IOBSDNameMatching(ourIOKitPort,
							  0,
							  (unsigned char *)partitionDev + 5),
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
      long ppn = 0;
      pn = (CFNumberRef)IORegistryEntryCreateCFProperty(obj, CFSTR(kIOMediaPartitionIDKey),
					   kCFAllocatorDefault, 0);

      if(pn == NULL) {
	return 4;
      }

      if(CFGetTypeID(pn) != CFNumberGetTypeID()) {
	CFRelease(pn);
	return 5;
      }

      CFNumberGetValue(pn, kCFNumberLongType, (void *)&ppn);
      *partitionNum = (unsigned long)ppn;
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
