/*
 *  setOF.c
 *  bless
 *
 *  Created by ssen on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/wait.h>

#import <mach/mach_error.h>

#include "bless.h"

#define NVRAM "/usr/sbin/nvram"

int setOpenFirmware(unsigned char mountpoint[]) {
  char ofString[1024];
  int sysVerMajor = 10;
  int isNewWorld = getNewWorld();
  int err;
  
  char * OFSettings[5];
  
  char bootdevice[1024];
  char bootfile[1024];
  char bootcommand[1024];
  
  OFSettings[0] = NVRAM;
  if(err = getOFInfo(mountpoint, ofString)) {
    errorprintf("Can't get Open Firmwaire information\n");
    return 1;
  } else {
    verboseprintf("Got OF string %s\n", ofString);
  }

  
    if (!isNewWorld && sysVerMajor < 10)
    {
	// set them up
        sprintf(bootdevice, "boot-device=/AAPL,ROM");
        sprintf(bootfile, "boot-file=");
        sprintf(bootcommand, "boot-command=boot");

    } else if (isNewWorld) {
        // set them up
        sprintf(bootdevice, "boot-device=%s,\\\\:tbxi", ofString);
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
        
    verboseprintf("OF Setings:\n");    
    verboseprintf("\t\tprogram: %s\n", OFSettings[0]);    
    verboseprintf("\t\t%s\n", OFSettings[1]);    
    verboseprintf("\t\t%s\n", OFSettings[2]);    
    verboseprintf("\t\t%s\n", OFSettings[3]);    


    if(!config.debug) {
        pid_t p;
        int status;
        p = fork();
        if (p == 0) {
            int ret = execv(NVRAM, OFSettings);
            if(ret == -1) {
                errorprintf("Could not exec %s\n", NVRAM);
            }
            _exit(1);
        }
        
        wait(&status);
        if(status) {
            errorprintf("%s returned non-0 exit status\n", NVRAM);
            return 3;
        }
    
    }
   
    return 0;
}
