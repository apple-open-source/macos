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
 *  BLSetOpenFirmwareBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetOpenFirmwareBootDevice.c,v 1.4 2002/04/27 17:55:00 ssen Exp $
 *
 *  $Log: BLSetOpenFirmwareBootDevice.c,v $
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

#define NVRAM "/usr/sbin/nvram"


int BLSetOpenFirmwareBootDevice(BLContext context, unsigned char mntfrm[]) {
  char ofString[1024];
  int err;
  
  char * OFSettings[5];
  
  char bootdevice[1024];
  char bootfile[1024];
  char bootcommand[1024];
  
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
    
  } else {
    // set them up
    sprintf(bootdevice, "boot-device=%s", ofString);
    sprintf(bootfile, "boot-file=");
    sprintf(bootcommand, "boot-command=0 bootr");
    
  }
	    
    OFSettings[1] = bootdevice;
    OFSettings[2] = bootfile;
    OFSettings[3] = bootcommand;
    OFSettings[4] = NULL;
        
    contextprintf(context, kBLLogLevelVerbose,  "OF Setings:\n" );    
    contextprintf(context, kBLLogLevelVerbose,  "\t\tprogram: %s\n", OFSettings[0] );    
    contextprintf(context, kBLLogLevelVerbose,  "\t\t%s\n", OFSettings[1] );    
    contextprintf(context, kBLLogLevelVerbose,  "\t\t%s\n", OFSettings[2] );    
    contextprintf(context, kBLLogLevelVerbose,  "\t\t%s\n", OFSettings[3] );    


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
