/*
 *  getMountPoint.c
 *  bless
 *
 *  Created by ssen on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "bless.h"

int getMountPoint(unsigned char f1[], unsigned char f2[], unsigned char mountp[]) {

    struct statfs fsinfo;
    int err;
    unsigned char f2mount[MNAMELEN];

    if(f1[0] != '\0') {
      if(err = statfs(f1, &fsinfo)) {
        errorprintf("No mount point for %s\n", f1);
        return 1;
      } else {
	strncpy(mountp, fsinfo.f_mntonname, MNAMELEN-1);
	mountp[MNAMELEN-1] = '\0';
	verboseprintf("Mount point for %s is %s\n", f1, mountp);
      }
    }

    if(f2[0] != '\0') {
      if(err = statfs(f2, &fsinfo)) {
        errorprintf("No mount point for %s\n", f2);
        return 2;
      } else {
	strncpy(f2mount, fsinfo.f_mntonname, MNAMELEN-1);
	f2mount[MNAMELEN-1] = '\0';
	verboseprintf("Mount point for %s is %s\n", f2, f2mount);
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

    errorprintf("No folders specified");
    return 4;
}
