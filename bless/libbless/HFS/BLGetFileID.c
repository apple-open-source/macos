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
 *  BLGetFileID.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLGetFileID.c,v 1.15 2006/02/20 22:49:55 ssen Exp $
 *
 */

#include <unistd.h> 
#include <errno.h>
#include <sys/attr.h>

#include "bless.h"
#include "bless_private.h"

int BLGetFileID(BLContextPtr context, const char *path, uint32_t *folderID) {

    int err;

    struct attrlist blist;
    struct objectinfobuf {
        uint32_t info_length;
        fsobj_id_t dirid;
    } attrbuf;


    // Get System Folder dirID
    blist.bitmapcount = 5;
    blist.reserved = 0;
    blist.commonattr = ATTR_CMN_OBJID;
    blist.volattr = 0;
    blist.dirattr = 0;
    blist.fileattr = 0;
    blist.forkattr = 0;

    err = getattrlist(path, &blist, &attrbuf, sizeof(attrbuf), 0);
    if (err) {
        return errno;
    };

    /*
     * the OBJID is an attribute stored in the in-core vnode in host
     * endianness. The kernel has already swapped it when loading the
     * Catalog entry from disk, so we don't need to do any swapping
     */
    
    *folderID = attrbuf.dirid.fid_objno;
    return 0;
}

