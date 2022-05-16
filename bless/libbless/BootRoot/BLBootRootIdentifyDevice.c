/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
 *  BLBootRootIdentifyDevice.c
 *  bless
 *
 *  Created by Shantonu Sen on 7/16/07.
 *  Copyright 2007 Apple Inc. All rights reserved.
 *
 */
#if 0
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/storage/IOMedia.h>

#include <CoreFoundation/CoreFoundation.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include "bless.h"
#include "bless_private.h"

// classify the role of a partition

int _verifyIsPhysicalPartition(BLContextPtr context,
                               io_service_t partition,
                               io_service_t *parentWholeMedia,
                               BLPartitionType *partType);

int BLBootRootIdentifyDevice(BLContextPtr context,
                             const char *bsdName,
                             BLBootRootRole *role)
{
    int ret;
    io_service_t service = IO_OBJECT_NULL, parentWholeMedia = IO_OBJECT_NULL;
    BLPartitionType partType = kBLPartitionType_None;

    if (bsdName == NULL || role == NULL) {
        return EINVAL;
    }
    
    *role = kBLBootRootRole_Unknown;
    
    ret = BLGetIOServiceForDeviceName(context, bsdName, &service);
    if (ret) {
        contextprintf(context, kBLLogLevelError, "No IOMedia for %s\n", bsdName);
        return ENOENT;
    }
    
    ret = _verifyIsPhysicalPartition(context, service, &parentWholeMedia, &partType);
    if (ret) {
        if (service != IO_OBJECT_NULL) IOObjectRelease(service);
        if (parentWholeMedia != IO_OBJECT_NULL) IOObjectRelease(parentWholeMedia);
        
        contextprintf(context, kBLLogLevelError, "Can't determine topology for IOMedia %s\n", bsdName);
        return EINVAL;        
    }

    if (parentWholeMedia != IO_OBJECT_NULL) {
        // XXX
    }
    
    return 0;
}

// do a topology check to make sure this partition is a leaf, its
// grandparent is a whole partition, the GP doesn't have an IOMedia
// ancestor
int _verifyIsPhysicalPartition(BLContextPtr context,
                               io_service_t partition,
                               io_service_t *parentWholeMedia,
                               BLPartitionType *partitionType)
{
    CFBooleanRef isLeaf, isWhole;
    CFStringRef content;
    io_service_t parent = IO_OBJECT_NULL, grandparent = IO_OBJECT_NULL;
    kern_return_t kret;
    
    isLeaf = IORegistryEntryCreateCFProperty(partition, CFSTR(kIOMediaLeafKey), kCFAllocatorDefault, 0);
    if (!isLeaf || CFGetTypeID(isLeaf) != CFBooleanGetTypeID()) {
        if (isLeaf) CFRelease(isLeaf);
        return ENOATTR;
    }
    
    if (!CFEqual(isLeaf, kCFBooleanTrue)) {
        CFRelease(isLeaf);
        return 0; // not failure, just not a physical partition
    }

    CFRelease(isLeaf);

    kret = IORegistryEntryGetParentEntry(partition, kIOServicePlane, &parent);
    if (kret) {
        return ENOENT;
    }
    
    kret = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &grandparent);
    if (kret) {
        IOObjectRelease(parent);
        return ENOENT;
    }

    IOObjectRelease(parent);

    if (!IOObjectConformsTo(grandparent, kIOMediaClass)) {
        IOObjectRelease(grandparent);
        return 0; // not a failure, just not a physical partition
    }
    
    isWhole = IORegistryEntryCreateCFProperty(grandparent, CFSTR(kIOMediaWholeKey), kCFAllocatorDefault, 0);
    if (!isWhole || CFGetTypeID(isWhole) != CFBooleanGetTypeID()) {
        if (isWhole) CFRelease(isWhole);
        IOObjectRelease(grandparent);
        return ENOATTR;
    }
    
    if (!CFEqual(isWhole, kCFBooleanTrue)) {
        CFRelease(isWhole);
        IOObjectRelease(grandparent);
        return 0; // not failure, just not a physical partition
    }

    CFRelease(isWhole);

    *parentWholeMedia = grandparent;
    
    content = IORegistryEntryCreateCFProperty(grandparent,
                                    CFSTR(kIOMediaContentKey),
                                    kCFAllocatorDefault, 0);
    
    
    if(!content || CFGetTypeID(content) != CFStringGetTypeID()) {
        if (content) CFRelease(content);
        return ENOATTR;
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
        if(partitionType) *partitionType = kBLPartitionType_None;
    }
    
    return 0;
}
#endif
