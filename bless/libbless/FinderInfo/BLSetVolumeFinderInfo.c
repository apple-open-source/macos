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
 *  BLSetVolumeFinderInfo.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLSetVolumeFinderInfo.c,v 1.17 2006/02/20 22:49:54 ssen Exp $
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/attr.h>

#include "bless.h"
#include "bless_private.h"

struct volinfobuf {
  uint32_t info_length;
  uint32_t finderinfo[8];
}; 


int BLSetVolumeFinderInfo(BLContextPtr context, const char *mountpoint, uint32_t *words) {

    struct attrlist alist;
    struct volinfobuf vinfo;
    int err, i;

    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = ATTR_VOL_INFO;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;

    err = getattrlist(mountpoint, &alist, &vinfo, sizeof(vinfo), 0);
    if (err) {
		int rval = errno;
		contextprintf(context, kBLLogLevelError,  "Can't volume information for %s\n", mountpoint );
		return rval;
    }

    // only let the user overwrite words 0-5 (i.e. not VSDB)
    for (i=0; i<6; i++) {
        vinfo.finderinfo[i] = CFSwapInt32HostToBig(words[i]);
    }

    err = setattrlist(mountpoint, &alist, &vinfo.finderinfo, sizeof(vinfo.finderinfo), 0);
    if (err) {
		int rval = errno;
		contextprintf(context, kBLLogLevelError,  "Error while setting volume information for %s\n", mountpoint );
		return rval;
    }

    return 0;
}

