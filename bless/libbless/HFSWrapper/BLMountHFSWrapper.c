/*
 * Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  BLMountHFSWrapper.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Jun 26 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLMountHFSWrapper.c,v 1.10 2003/07/22 15:58:33 ssen Exp $
 *
 *  $Log: BLMountHFSWrapper.c,v $
 *  Revision 1.10  2003/07/22 15:58:33  ssen
 *  APSL 2.0
 *
 *  Revision 1.9  2003/04/23 00:03:41  ssen
 *  If running on 10.3, use mount_hfs -w to mount wrapper, although
 *  mount_hfs started supporting it in 10.2
 *
 *  Revision 1.8  2003/04/19 00:11:11  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.7  2003/04/16 23:57:32  ssen
 *  Update Copyrights
 *
 *  Revision 1.6  2003/03/20 03:41:01  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.5.4.1  2003/03/20 03:30:38  ssen
 *  add TODO swap
 *
 *  Revision 1.5  2002/06/11 00:50:47  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
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

/* XXX 1020 mode is not endian safe XXX */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <hfs/hfs_format.h>

#include "bless.h"
#include "bless_private.h"

extern int errno;

#define SECTORSIZE 512

int BLMountHFSWrapper(BLContextPtr context, unsigned char device[], unsigned char mountpt[]) {

  unsigned char commandline[1024];
  int ret;
  char *mountwrapperopt;

#if OSX_TARGET >= 1030
  mountwrapperopt = "-w";

#else // OSX_TARGET

  int				mfd;
  char                     sector[SECTORSIZE];
  HFSPlusVolumeHeader     *vh = (HFSPlusVolumeHeader *)sector;
  HFSMasterDirectoryBlock *mdb = (HFSMasterDirectoryBlock *)sector;

  mountwrapperopt = "";

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
#endif // OSX_TARGET

  snprintf(commandline, 1024, "/sbin/mount_hfs %s %s %s", mountwrapperopt, device, mountpt);
  contextprintf(context, kBLLogLevelVerbose,  "Executing `%s'\n", commandline );

  ret = system(commandline);

   if(ret) {
        contextprintf(context, kBLLogLevelError,  "Can't mount %s on %s\n", device, mountpt );
        return 7;
   }


  return 0;
}
