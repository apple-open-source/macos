/*
 *  blessDir.c
 *  bless
 *
 *  Created by ssen on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/attr.h>
#include <sys/stat.h>

#include "bless.h"


int blessDir(unsigned char mountpoint[], UInt32 dirX, UInt32 dir9) {

    int err;
    UInt32 finderinfo[8];
    
    err = getFinderInfo(mountpoint, finderinfo);
    if(err) {
        errorprintf("Can't get Finder info fields for volume mounted at %s\n", mountpoint);
        return 1;
    }

    /* If either directory was not specified, the dirID
     * variables will be 0, so we can use that to initialize
     * the FI fields */

    /* Set Finder info words 3 & 5 */
    finderinfo[3] = dir9;
    finderinfo[5] = dirX;

    if(!dirX || (dirX && dir9 && config.use9)) {
      /* The 9 folder is what we really want */
      finderinfo[0] = dir9;
    } else {
      /* X */
      finderinfo[0] = dirX;
    }

    if(config.debug) {
      verboseprintf("finderinfo[0] = %d\n", finderinfo[0]);
      verboseprintf("finderinfo[3] = %d\n", finderinfo[3]);
      verboseprintf("finderinfo[5] = %d\n", finderinfo[5]);
    } else {
      if(err = setFinderInfo(mountpoint, finderinfo)) {
        errorprintf("Can't set Finder info fields for volume mounted at %s\n", mountpoint);
        return 2;
      } else {
	verboseprintf("Finder info fields set for volume mounted at %s\n", mountpoint);
      }
    }

    return 0;
}
