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
 *  BLBlessDir.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLBlessDir.c,v 1.15 2006/02/20 22:49:55 ssen Exp $
 *
 */

#include <sys/types.h>

#include "bless.h"
#include "bless_private.h"


int BLBlessDir(BLContextPtr context, const char * mountpoint,
                uint32_t dirX, uint32_t dir9, int useX) {

    int err;
    uint32_t finderinfo[8];
    
    err = BLGetVolumeFinderInfo(context, mountpoint, finderinfo);
    if(err) {
        contextprintf(context, kBLLogLevelError,  "Can't get Finder info fields for volume mounted at %s\n", mountpoint );
        return 1;
    }

    /* If either directory was not specified, the dirID
     * variables will be 0, so we can use that to initialize
     * the FI fields */

    /* Set Finder info words 3 & 5 */
    finderinfo[3] = dir9;
    finderinfo[5] = dirX;

    if(!dirX || !useX) {
      /* The 9 folder is what we really want */
      finderinfo[0] = dir9;
    } else {
      /* X */
      finderinfo[0] = dirX;
    }

    contextprintf(context, kBLLogLevelVerbose,  "finderinfo[0] = %d\n", finderinfo[0] );
    contextprintf(context, kBLLogLevelVerbose,  "finderinfo[3] = %d\n", finderinfo[3] );
    contextprintf(context, kBLLogLevelVerbose,  "finderinfo[5] = %d\n", finderinfo[5] );
    
    err = BLSetVolumeFinderInfo(context, mountpoint, finderinfo);
    if(err) {
      contextprintf(context, kBLLogLevelError,  "Can't set Finder info fields for volume mounted at %s\n", mountpoint );
      return 2;
    }

    return 0;
}

