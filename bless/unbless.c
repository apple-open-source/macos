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
 *  unbless.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Sun Mar 6, 2005.
 *  Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: unbless.c,v 1.2 2005/09/12 22:09:06 ssen Exp $
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <err.h>

#include "bless.h"

int unbless(char *mountpoint);

int main(int argc, char *argv[]) {

  char *mntpnt = NULL;
  struct statfs sb;
  int ret;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s /Volumes/foo\n", getprogname());
    exit(1);
  }

  mntpnt = argv[1];

  ret = statfs(mntpnt, &sb);
  if(ret)
    err(1, "statfs(%s)", mntpnt);

  if(0 != strcmp(mntpnt, sb.f_mntonname))
    errx(1, "Path is not a mount point");

  ret = unbless(mntpnt);

  return ret;
}


int unbless(char *mountpoint) {
	
    int ret;
    int isHFS;
    uint32_t oldwords[8];
		
    ret = BLIsMountHFS(NULL, mountpoint, &isHFS);
    if(ret)
      errx(1, "Could not determine filesystem of %s", mountpoint);

    if(!isHFS)
      errx(1, "%s is not HFS+", mountpoint);
    
    ret = BLGetVolumeFinderInfo(NULL, mountpoint, oldwords);
    if(ret)
      err(1, "Could not get finder info for %s", mountpoint);
		
    oldwords[0] = 0;
    oldwords[1] = 0;
    oldwords[2] = 0;
    oldwords[3] = 0;
    oldwords[5] = 0;
		
    /* bless! bless */
    
    ret = BLSetVolumeFinderInfo(NULL,  mountpoint, oldwords);
    if(ret)
      err(1, "Can't set finder info for %s", mountpoint);
	
    return 0;
}

