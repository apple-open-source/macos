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
 *  getFinderFlag.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jul 05 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetFinderFlag.c,v 1.4 2002/04/27 17:54:58 ssen Exp $
 *
 *  $Log: BLGetFinderFlag.c,v $
 *  Revision 1.4  2002/04/27 17:54:58  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.3  2002/04/25 07:27:26  ssen
 *  Go back to using errorprint and verboseprint inside library
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

#include <sys/types.h>
#include <sys/attr.h>
#include <unistd.h>

#include "bless.h"

struct TwoUInt16 {
    u_int16_t first;
    u_int16_t second;
};

struct fileinfobuf {
  u_int32_t info_length;
  u_int32_t finderinfo[4];
}; 

int BLGetFinderFlag(BLContext context, unsigned char path[], u_int16_t flag, int *retval) {
    struct attrlist		alist;
    struct fileinfobuf finfo;
    struct TwoUInt16 *twoUint = (struct TwoUInt16 *)&finfo.finderinfo[2];
    int err;
	
    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = 0;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;

    err = getattrlist(path, &alist, &finfo, sizeof(finfo), 0);
    if(err) {
        contextprintf(context, kBLLogLevelError,  "Can't file information for %s\n", path );
        return 1;
    }

    *retval = ( twoUint->first & flag ? 1 : 0 );

    return 0;
}

