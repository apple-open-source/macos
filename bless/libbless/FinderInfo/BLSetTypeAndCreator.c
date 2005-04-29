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
 *  BLSetTypeAndCreator.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetTypeAndCreator.c,v 1.12 2005/02/03 00:42:25 ssen Exp $
 *
 *  $Log: BLSetTypeAndCreator.c,v $
 *  Revision 1.12  2005/02/03 00:42:25  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.11  2004/04/20 21:40:41  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.10  2003/10/16 23:50:05  ssen
 *  Partially finish cleanup of headers to add "const" to char[] arguments
 *  that won't be modified.
 *
 *  Revision 1.9  2003/07/22 15:58:30  ssen
 *  APSL 2.0
 *
 *  Revision 1.8  2003/04/19 00:11:05  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.7  2003/04/16 23:57:30  ssen
 *  Update Copyrights
 *
 *  Revision 1.6  2003/03/20 03:40:53  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.5.2.1  2003/03/20 02:10:49  ssen
 *  swap integers to BE for on-disk representation
 *
 *  Revision 1.5  2003/03/19 22:56:58  ssen
 *  C99 types
 *
 *  Revision 1.4  2002/06/11 00:50:40  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.3  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2001/11/17 05:44:02  ssen
 *  fileinfobuf is 8 words
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

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/attr.h>

#include "bless.h"
#include "bless_private.h"

struct fileinfobuf {
  uint32_t info_length;
  uint32_t finderinfo[8];
}; 


int BLSetTypeAndCreator(BLContextPtr context, const unsigned char path[], uint32_t type, uint32_t creator) {
    struct attrlist		alist;
    struct fileinfobuf          finfo;
    int err;

	
    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = 0;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;

    err = getattrlist(path, &alist, &finfo, sizeof(finfo), 0);
    if(err) return 1;

    finfo.finderinfo[0] = CFSwapInt32HostToBig(type);
    finfo.finderinfo[1] = CFSwapInt32HostToBig(creator);

    err = setattrlist(path, &alist, &finfo.finderinfo, sizeof(finfo.finderinfo), 0);
    if(err) return 2;

    return 0;
}




