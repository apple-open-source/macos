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
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "bless.h"

int BLSetBootBlocks(BLContext context, unsigned char mountpoint[], unsigned char bbPtr[]) {
  /* If a Classic system folder was specified, and we need to set the boot blocks
   * for the volume, read it from the boot 0 resource of the System file, and write it */
  fbootstraptransfer_t        bbr;
  int                         fd;
  int err;
  
  
  fd = open(mountpoint, O_RDONLY);
  if (fd == -1) {
    contextprintf(context, kBLLogLevelError,  "Can't open volume mount point for %s\n", mountpoint );
    return 2;
  }
  
  bbr.fbt_offset = 0;
  bbr.fbt_length = 1024;
  bbr.fbt_buffer = (unsigned char *)bbPtr;
  
  err = fcntl(fd, F_WRITEBOOTSTRAP, &bbr);
  if (err) {
    contextprintf(context, kBLLogLevelError,  "Can't write boot blocks\n" );
    close(fd);
    return 3;
  } else {
    contextprintf(context, kBLLogLevelVerbose,  "Boot blocks written successfully\n" );
  }
  close(fd);
  
  return 0;
}
