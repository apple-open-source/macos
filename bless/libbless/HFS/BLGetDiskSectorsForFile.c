/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetDiskSectorsForFile.c,v 1.10 2003/07/22 15:58:31 ssen Exp $
 *
 *  $Log: BLGetDiskSectorsForFile.c,v $
 *  Revision 1.10  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.9  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.8  2003/04/16 23:57:31  ssen
 *  Update Copyrights
 *
 *  Revision 1.7  2003/03/20 03:40:57  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.6.2.1  2003/03/20 03:29:52  ssen
 *  swap MDB structures
 *
 *  Revision 1.6  2003/03/19 22:57:02  ssen
 *  C99 types
 *
 *  Revision 1.5  2002/06/11 00:50:42  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.4  2002/06/09 13:14:03  ssen
 *  finish code to get extents of xcoff file in filesystem. Figure out
 *  pdisk invocation. Still doesn't work quite yet because volume is
 *  mounted at this point, so you can't repartition.
 *
 *  Revision 1.3  2002/06/01 20:45:15  ssen
 *  Get extents and allocation block size for file. All that's left
 *  is getting the offset to the embedded volume and adding a pdisk
 *  incantation to write the pmap entries
 *
 *  Revision 1.2  2002/06/01 17:54:30  ssen
 *  Uh. make compile
 *
 *  Revision 1.1  2002/06/01 17:52:45  ssen
 *  Add function to map files to disk sectors
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/fcntl.h>
#include <sys/attr.h>
#include <unistd.h>
#include <sys/stat.h>
#include <hfs/hfs_format.h>
#include <string.h>
#include <stdio.h>

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

extern int errno;

/*
 * First determine the device and the extents on the mounted volume
 * Then parse the device to see if an offset needs to be added
 */

int BLGetDiskSectorsForFile(BLContextPtr context, unsigned char path[], off_t extents[8][2],
                            unsigned char device[]) {

    struct statfs sb;
    struct extinfo info;
    struct allocinfo ainfo;
    struct attrlist alist, blist;
    unsigned char buffer[512];
    HFSMasterDirectoryBlock *mdb = (HFSMasterDirectoryBlock  *) buffer;
    off_t sectorsPerBlock, offset;
    unsigned char rawdev[MNAMELEN];
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
