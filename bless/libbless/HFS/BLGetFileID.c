/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  BLGetFileID.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetFileID.c,v 1.2 2002/02/23 04:13:05 ssen Exp $
 *
 *  $Log: BLGetFileID.c,v $
 *  Revision 1.2  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.6  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.4  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#include <unistd.h> 
#include <sys/attr.h>

#include "bless.h"

int BLGetFileID(BLContext context, unsigned char path[], u_int32_t *folderID) {

    int err;

    struct attrlist blist;
    struct objectinfobuf {
        u_int32_t info_length;
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
        return 1;
    };

    
    *folderID = attrbuf.dirid.fid_objno;
    return 0;
}

