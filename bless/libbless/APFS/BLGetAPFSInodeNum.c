/*
 * Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
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
 *  BLGetAPFSInodeNum.c
 *  bless
 *
 *  Created by Jon Becker on 9/21/16.
 *  Copyright (c) 2001-2016 Apple Inc. All Rights Reserved.
 *
 */

#include <unistd.h>
#include <sys/attr.h>

#include "bless.h"
#include "bless_private.h"


int BLGetAPFSInodeNum(BLContextPtr context, const char *path, uint64_t *inum)
{
    int err;
    struct attrlist blist;
    struct objectinfobuf {
        uint32_t info_length;
        uint64_t fileID;
    } __attribute__((aligned(4), packed)) attrbuf;
    
    
    // Get System Folder dirID
    blist.bitmapcount = 5;
    blist.reserved = 0;
    blist.commonattr = ATTR_CMN_FILEID;
    blist.volattr = 0;
    blist.dirattr = 0;
    blist.fileattr = 0;
    blist.forkattr = 0;
    
    err = getattrlist(path, &blist, &attrbuf, sizeof(attrbuf), 0);
    if (err) {
        return errno;
    };
    *inum = attrbuf.fileID;
    return 0;
}
