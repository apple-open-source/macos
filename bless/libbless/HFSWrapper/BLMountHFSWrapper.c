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
 *  BLMountHFSWrapper.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Jun 26 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLMountHFSWrapper.c,v 1.4 2002/04/27 17:54:59 ssen Exp $
 *
 *  $Log: BLMountHFSWrapper.c,v $
 *  Revision 1.4  2002/04/27 17:54:59  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.3  2002/04/25 07:27:28  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.2  2002/02/23 04:13:05  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.10  2001/11/11 06:19:08  ssen
 *  revert to -pre-libbless
 *
 *  Revision 1.8  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <hfs/hfs_format.h>

#include "bless.h"

extern int errno;

#define SECTORSIZE 512

int BLMountHFSWrapper(BLContext context, unsigned char device[], unsigned char mountpt[]) {

  int				mfd;
  char                     sector[SECTORSIZE];
  HFSPlusVolumeHeader     *vh = (HFSPlusVolumeHeader *)sector;
  HFSMasterDirectoryBlock *mdb = (HFSMasterDirectoryBlock *)sector;
  unsigned char commandline[1024];

  int ret;

    mfd = open(device, O_RDWR);
  
  if(mfd == -1) {
    contextprintf(context, kBLLogLevelError,  "Cannot open device %s\n", device );
    return 1;
  }

    if((2*SECTORSIZE) != lseek(mfd, (2*SECTORSIZE), SEEK_SET)) {
        contextprintf(context, kBLLogLevelError,  "Could not seed to offset %d of device %s\n", (2*SECTORSIZE), device );
        close(mfd);
        return 1;
    }

    if(SECTORSIZE != read(mfd, sector, SECTORSIZE)) {
        contextprintf(context, kBLLogLevelError,  "Could not read %d bytes from offset %d of device %s\n", SECTORSIZE, (2*SECTORSIZE), device );
        close(mfd);
        return 1;    
    }

  if (vh->signature == kHFSPlusSigWord) {
    contextprintf(context, kBLLogLevelVerbose,  "%s: Pure HFS+, no wrapper\n", device );
        close(mfd);
    return 3;
  }

  if (vh->signature == kHFSSigWord) {
    if (mdb->drEmbedSigWord == kHFSPlusSigWord) {
      contextprintf(context, kBLLogLevelVerbose,  "%s: Embedded HFS+ in HFS-\n", device );
    } else {
      contextprintf(context, kBLLogLevelError,  "%s: Pure HFS-, no wrapper\n", device );
        close(mfd);
      return 4;
    }
  }
      
  contextprintf(context, kBLLogLevelVerbose,  "Switching to HFS- signature\n" );

  mdb->drEmbedSigWord = kHFSSigWord;

    if((2*SECTORSIZE) != lseek(mfd, (2*SECTORSIZE), SEEK_SET)) {
        contextprintf(context, kBLLogLevelError,  "Could not seed to offset %d of device %s\n", (2*SECTORSIZE), device );
        close(mfd);
        return 5;
    }

    if(SECTORSIZE != write(mfd, sector, SECTORSIZE)) {
        contextprintf(context, kBLLogLevelError,  "Could not write %d bytes from offset %d of device %s\n", SECTORSIZE, (2*SECTORSIZE), device );
        close(mfd);
        return 5;    
    }



  contextprintf(context, kBLLogLevelVerbose,  "Closing volume\n" );
  ret = close(mfd);
  if (ret == -1) {
    contextprintf(context, kBLLogLevelError,  "Error from close(): %s\n", strerror(errno) );
    return 6;
  }


  snprintf(commandline, 1024, "/sbin/mount_hfs %s %s", device, mountpt);
  ret = system(commandline);

   if(ret) {
        contextprintf(context, kBLLogLevelError,  "Can't mount %s on %s\n", device, mountpt );
        return 7;
   }


  return 0;
}
