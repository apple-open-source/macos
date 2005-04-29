/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
 *  BLSetOpenFirmwareBootDevice.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLSetOpenFirmwareBootDevice.c,v 1.14 2005/02/03 00:42:29 ssen Exp $
 *
 *  $Log: BLSetOpenFirmwareBootDevice.c,v $
 *  Revision 1.14  2005/02/03 00:42:29  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.13  2005/01/08 13:05:34  ssen
 *  <rdar://problem/3942261> need a way to avoid hard-coding clean boot-arg keys
 *  Use new code to not hardcode boot-args to preserve
 *
 *  Revision 1.12  2004/06/16 00:34:46  ssen
 *  <rdar://problem/2950473> ER: Installer carry over bootargs debug values
 *  Treat debug= as a special value, and preserve it
 *  Bug #: 2950473
 *
 *  Revision 1.11  2004/04/20 21:40:45  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.10  2003/10/17 00:10:39  ssen
 *  add more const
 *
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

#include "preserve_bootargs.h"

int BLSetOpenFirmwareBootDevice(BLContextPtr context, const unsigned char mntfrm[]) {
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

  sprintf(bootargs, "boot-args=");
  
  if (isNewWorld) {
      char oldbootargs[1024];
      char *token, *restargs;
	  int firstarg=1;
      FILE *pop;
      
      oldbootargs[0] = '\0';
      
      pop = popen("/usr/sbin/nvram boot-args", "r");
      if(pop) {
          
          if(NULL == fgets(oldbootargs, sizeof(oldbootargs), pop)) {
              contextprintf(context, kBLLogLevelVerbose,  "Could not parse output from /usr/sbin/nvram\n" );
          }
          pclose(pop);

          restargs = oldbootargs;
          if(NULL != strsep(&restargs, "\t")) { // nvram must separate the name from the value with a tab
              restargs[strlen(restargs)-1] = '\0'; // remove \n
              memmove(oldbootargs, restargs, strlen(restargs)+1);
              
              contextprintf(context, kBLLogLevelVerbose,  "Old boot-args: %s\n", oldbootargs);
              
			  restargs = oldbootargs;
			  while((token = strsep(&restargs, " ")) != NULL) {
				  int shouldbesaved = 0, i;
				  contextprintf(context, kBLLogLevelVerbose, "\tGot token: %s\n", token);
				  for(i=0; i < sizeof(preserve_boot_args)/sizeof(preserve_boot_args[0]); i++) {
					// see if it's something we want
					  if(preserve_boot_args[i][0] == '-') {
						  // -v style
						  if(strcmp(preserve_boot_args[i], token) == 0) {
							  shouldbesaved = 1;
							  break;
						  }
					  } else {
						// debug= style
						  int keylen = strlen(preserve_boot_args[i]);
						  if(strlen(token) >= keylen+1
							 && strncmp(preserve_boot_args[i], token, keylen) == 0
							 && token[keylen] == '=') {
							  shouldbesaved = 1;
							  break;
						  }
					  }
				  }
				  
				  if(shouldbesaved) {
					// append to bootargs if it should be preserved
					  if(firstarg) {
						  firstarg = 0;
					  } else {
						  strcat(bootargs, " ");
					  }
					  
					  contextprintf(context, kBLLogLevelVerbose,  "\tPreserving: %s\n", token);
					  strcat(bootargs, token);
				  }
			  }
			  
          }
    }
      
      // set them up
    sprintf(bootdevice, "boot-device=%s", ofString);
    sprintf(bootfile, "boot-file=");
    sprintf(bootcommand, "boot-command=mac-boot");
	// bootargs initialized above, and append-to later
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
