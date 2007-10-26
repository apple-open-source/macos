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
 *  BLGetCommonMountPoint.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLGetCommonMountPoint.c,v 1.14 2006/02/20 22:49:56 ssen Exp $
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "bless.h"
#include "bless_private.h"

int BLGetCommonMountPoint(BLContextPtr context, const char * f1,
    const char * f2, char * mountp) {

    struct statfs fsinfo;
    int err;
    char f2mount[MNAMELEN];

    if(f1[0] != '\0') {
		err = statfs(f1, &fsinfo);
      if(err) {
        contextprintf(context, kBLLogLevelError,  "No mount point for %s\n", f1 );
        return 1;
      } else {
	strncpy(mountp, fsinfo.f_mntonname, MNAMELEN-1);
	mountp[MNAMELEN-1] = '\0';
	contextprintf(context, kBLLogLevelVerbose,  "Mount point for %s is %s\n", f1, mountp );
      }
    }

    if(f2[0] != '\0') {
		err = statfs(f2, &fsinfo);
      if(err) {
        contextprintf(context, kBLLogLevelError,  "No mount point for %s\n", f2 );
        return 2;
      } else {
	strncpy(f2mount, fsinfo.f_mntonname, MNAMELEN-1);
	f2mount[MNAMELEN-1] = '\0';
	contextprintf(context, kBLLogLevelVerbose,  "Mount point for %s is %s\n", f2, f2mount );
      }
    }

    /* Now we have the mount points of any folders that were passed
     * in. We must determine:
     * 1) if f1 && f2, find a common mount point or err
     * 2) if f2 && !f1, copy f2mount -> mountp
     * 3) if f1 && !f2, just return success
     */

    if(f2[0] != '\0') {
      /* Case 1, 2 */
      if(f1[0] != '\0') {
	/* Case 1 */
	if(strcmp(mountp, f2mount)) {
	  /* no common */
	  mountp[0] = '\0';
	  return 3;
	} else {
	  /* yay common */
	  return 0;
	}
      } else {
	/* Case 2 */

	/* We know each buffer is <MNAMELEN and 0-terminated */
	strncpy(mountp, f2mount, MNAMELEN);
	return 0;
      }
    } else {
      /* Case 3 */
      return 0;
    }

    contextprintf(context, kBLLogLevelError,  "No folders specified" );
    return 4;
}
