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
 *  BLSetTypeAndCreator.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetTypeAndCreator.c,v 1.3 2002/02/23 04:13:05 ssen Exp $
 *
 *  $Log: BLSetTypeAndCreator.c,v $
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
#include <sys/types.h>
#include <unistd.h>
#include <sys/attr.h>

#include "bless.h"

struct fileinfobuf {
  u_int32_t info_length;
  u_int32_t finderinfo[8];
}; 


int BLSetTypeAndCreator(BLContext context, unsigned char path[], u_int32_t type, u_int32_t creator) {
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

    finfo.finderinfo[0] = type;
    finfo.finderinfo[1] = creator;

    err = setattrlist(path, &alist, &finfo.finderinfo, sizeof(finfo.finderinfo), 0);
    if(err) return 2;

    return 0;
}




