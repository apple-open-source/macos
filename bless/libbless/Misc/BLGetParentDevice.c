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

    int                     result = 0;
    kern_return_t           kret;
    io_iterator_t           services = MACH_PORT_NULL;
    io_iterator_t           parents = MACH_PORT_NULL;
    io_registry_entry_t     service = MACH_PORT_NULL;
    io_iterator_t           grandparents = MACH_PORT_NULL;
    io_registry_entry_t     service2 = MACH_PORT_NULL;
    io_object_t             obj = MACH_PORT_NULL;
    CFNumberRef             pn = NULL;
    CFStringRef             content = NULL;

    char par[MNAMELEN];

    parentDev[0] = '\0';

    kret = IOServiceGetMatchingServices(kIOMasterPortDefault,
					IOBSDNameMatching(kIOMasterPortDefault,
							  0,
							  (char *)partitionDev + 5),
					&services);
    if (kret != KERN_SUCCESS) {
      result = 3;
      goto finish;
    }

    // Should only be one IOKit object for this volume. (And we only want one.)
    obj = IOIteratorNext(services);
    if (!obj) {
        result = 4;
        goto finish;
    }  

    // we have the IOMedia for the partition.

    pn = (CFNumberRef)IORegistryEntryCreateCFProperty(obj, CFSTR(kIOMediaPartitionIDKey),
        kCFAllocatorDefault, 0);
    
    if(pn == NULL) {
        result = 4;
        goto finish;
    }
    
    if (CFGetTypeID(pn) != CFNumberGetTypeID()) {
        result = 5;
        goto finish;
    }
    
    CFNumberGetValue(pn, kCFNumberSInt32Type, partitionNum);
    
    kret = IORegistryEntryGetParentIterator (obj, kIOServicePlane,
					       &parents);
    if (kret) {
      result = 6;
      goto finish;
      /* We'll never loop forever. */
    }

    while ( (service = IOIteratorNext(parents)) != 0 ) {

        kret = IORegistryEntryGetParentIterator (service, kIOServicePlane,
                                                &grandparents);
        IOObjectRelease(service);
        service = MACH_PORT_NULL;

        if (kret) {
            result = 6;
            goto finish;
            /* We'll never loop forever. */
        }

        while ( (service2 = IOIteratorNext(grandparents)) != 0 ) {
        
            if (content) {
                CFRelease(content);
                content = NULL;
            }

            if (!IOObjectConformsTo(service2, "IOMedia")) {
                IOObjectRelease(service2);
                service2 = MACH_PORT_NULL;
                continue;
            }
        
            content = (CFStringRef)
                IORegistryEntryCreateCFProperty(service2,
                                                CFSTR(kIOMediaContentKey),
                                                kCFAllocatorDefault, 0);
            
            
            if(CFGetTypeID(content) != CFStringGetTypeID()) {
                result = 2;
                goto finish;
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
                IOObjectRelease(service2);
                service2 = MACH_PORT_NULL;
                continue;
            }
            
            content = IORegistryEntryCreateCFProperty(service2, CFSTR(kIOBSDNameKey),
                                                        kCFAllocatorDefault, 0);
        
            if(CFGetTypeID(content) != CFStringGetTypeID()) {
                result = 3;
                goto finish;
            }
        
            if(!CFStringGetCString(content, par, MNAMELEN, kCFStringEncodingASCII)) {
                result = 4;
                goto finish;
            }

            sprintf(parentDev, "/dev/%s",par);
            break;
        }

        if(parentDev[0] == '\0') {
            break;
        }
    }

    if(parentDev[0] == '\0') {
      // nothing found
      result = 8;
      goto finish;
    }

finish:
    if (services != MACH_PORT_NULL)     IOObjectRelease(services);
    if (parents != MACH_PORT_NULL)      IOObjectRelease(parents);
    if (service != MACH_PORT_NULL)      IOObjectRelease(service);
    if (grandparents != MACH_PORT_NULL) IOObjectRelease(grandparents);
    if (service2 != MACH_PORT_NULL)     IOObjectRelease(service2);
    if (obj != MACH_PORT_NULL)          IOObjectRelease(obj);
    if (pn)                             CFRelease(pn);
    if (content)                        CFRelease(content);
    
    return result;
}
