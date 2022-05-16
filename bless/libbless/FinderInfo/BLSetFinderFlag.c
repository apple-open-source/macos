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
 *  getFinderFlag.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jul 05 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLSetFinderFlag.c,v 1.17 2006/02/20 22:49:54 ssen Exp $
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/attr.h>
#include <unistd.h>

#include "bless.h"
#include "bless_private.h"

struct TwoUInt16 {
    uint16_t first;
    uint16_t second;
};

struct fileinfobuf {
  uint32_t info_length;
  uint32_t finderinfo[8];
}; 


int BLSetFinderFlag(BLContextPtr context, const char * path, uint16_t flag, int setval) {
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

    if(setval) {
        /* we want to set the bit. so OR with the flag  */
        twoUint->first |= CFSwapInt16HostToBig(flag);
    } else {
        /* AND with a mask  that excludes flag*/
        uint16_t mask = CFSwapInt16HostToBig(flag);;
	mask = ~mask;
        twoUint->first &= mask;
    }
    
	err = setattrlist(path, &alist, &finfo.finderinfo, sizeof(finfo.finderinfo), 0);
    if(err) {
        contextprintf(context, kBLLogLevelError,  "Error while setting file information for %s\n", path );
        return 2;
    }

    return 0;
}
