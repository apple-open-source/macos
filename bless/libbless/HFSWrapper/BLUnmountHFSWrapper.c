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
 *  BLUnmountHFSWrapper.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Jun 26 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLUnmountHFSWrapper.c,v 1.9 2003/07/22 15:58:33 ssen Exp $
 *
 *  $Log: BLUnmountHFSWrapper.c,v $
 *  Revision 1.9  2003/07/22 15:58:33  ssen
 *  APSL 2.0
 *
 *  Revision 1.8  2003/04/23 00:03:41  ssen
 *  If running on 10.3, use mount_hfs -w to mount wrapper, although
 *  mount_hfs started supporting it in 10.2
 *
 *  Revision 1.7  2003/04/19 00:11:11  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.6  2003/04/16 23:57:32  ssen
 *  Update Copyrights
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
 *  Revision 1.2  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.9  2001/11/11 06:19:08  ssen
 *  revert to -pre-libbless
 *
 *  Revision 1.7  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */


#include <sys/param.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <string.h>
#include <hfs/hfs_format.h>

#include "bless.h"
#include "bless_private.h"

#define SECTORSIZE 512

int BLUnmountHFSWrapper(BLContextPtr context, unsigned char device[], unsigned char mountpt[]) {

  unsigned char commandline[1024];
  int ret;

#if OSX_TARGET < 1030
  int				mfd;
  char                     sector[SECTORSIZE];
  HFSMasterDirectoryBlock *mdb = (HFSMasterDirectoryBlock *)sector;
#endif
  
    snprintf(commandline, 1024, "/sbin/umount %s", mountpt);
    contextprintf(context, kBLLogLevelVerbose,  "Executing command `%s'\n", commandline );

    ret = system(commandline);
    if(ret) {
        contextprintf(context, kBLLogLevelError,  "Can't unmount %s from %s\n", device, mountpt );
        return 7;
    }

#if OSX_TARGET < 1030
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

  contextprintf(context, kBLLogLevelVerbose,  "Switching to HFS+ embedded signature\n" );

  mdb->drEmbedSigWord = kHFSPlusSigWord;

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
    contextprintf(context, kBLLogLevelError,  "Error %d from close()", ret );
    return 6;
  }
#endif // OSX_TARGET
  
  return 0;
}
