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
 *  BLSetOpenFirmwareBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetOpenFirmwareBootDevice.c,v 1.9 2003/07/22 15:58:36 ssen Exp $
 *
 *  $Log: BLSetOpenFirmwareBootDevice.c,v $
 *  Revision 1.9  2003/07/22 15:58:36  ssen
 *  APSL 2.0
 *
 *  Revision 1.8  2003/04/19 00:11:14  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.7  2003/04/16 23:57:35  ssen
 *  Update Copyrights
 *
 *  Revision 1.6  2002/08/22 04:29:03  ssen
 *  zero out boot-args for Jim...
 *
 *  Revision 1.5  2002/06/11 00:50:51  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.4  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.3  2002/04/25 07:27:30  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.2  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.10  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.8  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "bless.h"
#include "bless_private.h"

#define NVRAM "/usr/sbin/nvram"


int BLSetOpenFirmwareBootDevice(BLContextPtr context, unsigned char mntfrm[]) {
  char ofString[1024];
  int err;
  
  char * OFSettings[6];
  
  char bootdevice[1024];
  char bootfile[1024];
  char bootcommand[1024];
  char bootargs[1024]; // always zero out bootargs
  
  int isNewWorld = BLIsNewWorld(context);
  pid_t p;
  int status;
  
  OFSettings[0] = NVRAM;
  err = BLGetOpenFirmwareBootDevice(context, mntfrm, ofString);
  if(err) {
    contextprintf(context, kBLLogLevelError,  "Can't get Open Firmware information\n" );
    return 1;
  } else {
    contextprintf(context, kBLLogLevelVerbose,  "Got OF string %s\n", ofString );
  }

  
  if (isNewWorld) {
    // set them up
    sprintf(bootdevice, "boot-device=%s", ofString);
    sprintf(bootfile, "boot-file=");
    sprintf(bootcommand, "boot-command=mac-boot");
    sprintf(bootargs, "boot-args=");    
  } else {
    // set them up
    sprintf(bootdevice, "boot-device=%s", ofString);
    sprintf(bootfile, "boot-file=");
    sprintf(bootcommand, "boot-command=0 bootr");
    sprintf(bootargs, "boot-args=");    
  }
	    
    OFSettings[1] = bootdevice;
    OFSettings[2] = bootfile;
    OFSettings[3] = bootcommand;
    OFSettings[4] = bootargs;
    OFSettings[5] = NULL;
        
    contextprintf(context, kBLLogLevelVerbose,  "OF Setings:\n" );    
    contextprintf(context, kBLLogLevelVerbose,  "\t\tprogram: %s\n", OFSettings[0] );
    contextprintf(context, kBLLogLevelVerbose,  "\t\t%s\n", OFSettings[1] );
    contextprintf(context, kBLLogLevelVerbose,  "\t\t%s\n", OFSettings[2] );
    contextprintf(context, kBLLogLevelVerbose,  "\t\t%s\n", OFSettings[3] );
    contextprintf(context, kBLLogLevelVerbose,  "\t\t%s\n", OFSettings[4] );

    p = fork();
    if (p == 0) {
      int ret = execv(NVRAM, OFSettings);
      if(ret == -1) {
	contextprintf(context, kBLLogLevelError,  "Could not exec %s\n", NVRAM );
      }
      _exit(1);
    }
    
    wait(&status);
    if(status) {
      contextprintf(context, kBLLogLevelError,  "%s returned non-0 exit status\n", NVRAM );
      return 3;
    }
    
    return 0;
}
