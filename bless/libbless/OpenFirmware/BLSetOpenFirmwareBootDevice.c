/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLSetOpenFirmwareBootDevice.c,v 1.18 2006/02/20 22:49:57 ssen Exp $
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

int BLSetOpenFirmwareBootDevice(BLContextPtr context, const char * mntfrm) {
    char ofString[1024];
    int err;
    
    char * OFSettings[6];
    
    char bootdevice[1024];
    char bootfile[1024];
    char bootcommand[1024];
    char bootargs[1024]; // always zero out bootargs
    
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
    
    char oldbootargs[1024];
    char *restargs;
    FILE *pop;
    
    oldbootargs[0] = '\0';
    
    pop = popen("/usr/sbin/nvram boot-args", "r");
    if(pop) {
        
        if(NULL == fgets(oldbootargs, (int)sizeof(oldbootargs), pop)) {
            contextprintf(context, kBLLogLevelVerbose,  "Could not parse output from /usr/sbin/nvram\n" );
        }
        pclose(pop);
        
        restargs = oldbootargs;
        if(NULL != strsep(&restargs, "\t")) { // nvram must separate the name from the value with a tab
            restargs[strlen(restargs)-1] = '\0'; // remove \n
            
            err = BLPreserveBootArgs(context, restargs, bootargs+strlen(bootargs));
        }
    }
    
    // set them up
    sprintf(bootdevice, "boot-device=%s", ofString);
    sprintf(bootfile, "boot-file=");
    sprintf(bootcommand, "boot-command=mac-boot");
	// bootargs initialized above, and append-to later
    
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
    
    do {
        p = wait(&status);
    } while (p == -1 && errno == EINTR);
    
    if(p == -1 || status) {
        contextprintf(context, kBLLogLevelError,  "%s returned non-0 exit status\n", NVRAM );
        return 3;
    }
    
    return 0;
}
