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
 *  BLGetFileID.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetFileID.c,v 1.14 2005/08/22 20:49:23 ssen Exp $
 *
 *  $Log: BLGetFileID.c,v $
 *  Revision 1.14  2005/08/22 20:49:23  ssen
 *  Change functions to take "char *foo" instead of "char foo[]".
 *  It should be semantically identical, and be more consistent with
 *  other system APIs
 *
 *  Revision 1.13  2005/06/24 16:39:50  ssen
 *  Don't use "unsigned char[]" for paths. If regular char*s are
 *  good enough for the BSD system calls, they're good enough for
 *  bless.
 *
 *  Revision 1.12  2005/02/03 00:42:25  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.11  2004/04/20 21:40:42  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.10  2003/10/16 23:50:05  ssen
 *  Partially finish cleanup of headers to add "const" to char[] arguments
 *  that won't be modified.
 *
 *  Revision 1.9  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.8  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.7  2003/04/16 23:57:31  ssen
 *  Update Copyrights
 *
 *  Revision 1.6  2003/03/20 18:52:55  ssen
 *  Clarify comments about dirID, and make sure to not overwrite VSDB
 *
 *  Revision 1.5  2003/03/20 03:40:57  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.4.2.1  2003/03/20 02:41:54  ssen
 *  add comment that we don't need swapping
 *
 *  Revision 1.4  2003/03/19 22:57:02  ssen
 *  C99 types
 *
 *  Revision 1.3  2002/06/11 00:50:43  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
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
#include "bless_private.h"

int BLGetFileID(BLContextPtr context, const char * path, uint32_t *folderID) {

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
        return 1;
    };

    /*
     * the OBJID is an attribute stored in the in-core vnode in host
     * endianness. The kernel has already swapped it when loading the
     * Catalog entry from disk, so we don't need to do any swapping
     */
    
    *folderID = attrbuf.dirid.fid_objno;
    return 0;
}

