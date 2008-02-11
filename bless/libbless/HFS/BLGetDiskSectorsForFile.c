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
 *  BLGetDiskSectorsForFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Sat Jun 01 2002.
 *  Copyright (c) 2002-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLGetDiskSectorsForFile.c,v 1.17 2006/02/20 22:49:55 ssen Exp $
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/fcntl.h>
#include <sys/attr.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <hfs/hfs_format.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "bless.h"
#include "bless_private.h"

struct extinfo {
    uint32_t length;
    HFSPlusExtentRecord extents;
};

struct allocinfo {
    uint32_t length;
    off_t allocationSize;
};

/*
 * First determine the device and the extents on the mounted volume
 * Then parse the device to see if an offset needs to be added
 */

int BLGetDiskSectorsForFile(BLContextPtr context, const char * path, off_t extents[8][2],
                            char * device) {

    struct statfs sb;
    struct extinfo info;
    struct allocinfo ainfo;
    struct attrlist alist, blist;
    char buffer[512];
    HFSMasterDirectoryBlock *mdb = (HFSMasterDirectoryBlock  *) buffer;
    off_t sectorsPerBlock, offset;
    char rawdev[MNAMELEN];
    int i,  fd;
    int ret;
    
    ret = statfs(path, &sb);
    if(ret) {
        contextprintf(context, kBLLogLevelError,  "Can't get information for %s\n", path );
        return 1;
    }

    strcpy(device, sb.f_mntfromname);

    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = 0;
    alist.volattr =  0;
    alist.dirattr = 0;
    alist.fileattr = ATTR_FILE_DATAEXTENTS;
    alist.forkattr = 0;
    
    ret = getattrlist(path, &alist, &info, sizeof(info), 1);
    if(ret) {
        contextprintf(context, kBLLogLevelError,  "Could not get extents for %s: %d\n", path, errno);
        return 1;
    }

    blist.bitmapcount = 5;
    blist.reserved = 0;
    blist.commonattr = 0;
    blist.volattr =  ATTR_VOL_MINALLOCATION|ATTR_VOL_INFO;
    blist.dirattr = 0;
    blist.fileattr = 0;
    blist.forkattr = 0;
    
    ret = getattrlist(sb.f_mntonname, &blist, &ainfo, sizeof(ainfo), 1);
    if(ret) {
        contextprintf(context, kBLLogLevelError, "Could not get allocation block size for %s: %d\n", sb.f_mntonname, errno);
        return 1;
    }
    
    sectorsPerBlock = ainfo.allocationSize / 512;

    sprintf(rawdev, "/dev/r%s", device+5);

    fd = open(rawdev, O_RDONLY, 0);
    if(fd == -1) {
            contextprintf(context, kBLLogLevelError,  "Could not open device to read Master Directory Block: %d\n", errno);
            return 3;
    }
    
    lseek(fd, 1024, SEEK_SET);
    
    if(512 != read(fd, buffer, 512)) {
            contextprintf(context, kBLLogLevelError,  "Failed to read Master Directory Block\n");
            return 3;    
    }
   
   
    offset = 0;
    if(CFSwapInt16BigToHost(mdb->drSigWord) == kHFSPlusSigWord) {
        // pure HFS+
        offset = 0;
    } else if ((CFSwapInt16BigToHost(mdb->drSigWord) == kHFSSigWord)
            && (CFSwapInt16BigToHost(mdb->drEmbedSigWord) == kHFSPlusSigWord)) {
        // HFS+ embedded
        offset = CFSwapInt16BigToHost(mdb->drAlBlSt)
            + CFSwapInt16BigToHost(mdb->drEmbedExtent.startBlock)*
	               (CFSwapInt32BigToHost(mdb->drAlBlkSiz) >> 9);
    } else {
        // pure HFS
        offset = CFSwapInt16BigToHost(mdb->drAlBlSt);
    }
   
    close(fd);

    for(i=0; i<8; i++) {
        extents[i][0] = info.extents[i].startBlock*sectorsPerBlock+offset;
        extents[i][1] = info.extents[i].blockCount*sectorsPerBlock;
    }

    return 0;
}
