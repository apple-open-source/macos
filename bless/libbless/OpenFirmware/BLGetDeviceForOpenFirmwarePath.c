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
 *  BLGetDeviceForOpenFirmwarePath.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Jan 24 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLGetDeviceForOpenFirmwarePath.c,v 1.5 2002/04/27 17:55:00 ssen Exp $
 *
 *  $Log: BLGetDeviceForOpenFirmwarePath.c,v $
 *  Revision 1.5  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.4  2002/04/25 07:27:30  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.3  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.2  2002/02/03 17:25:22  ssen
 *  Fix "bless -info" usage to determine current device
 *
 *  Revision 1.1  2002/02/03 17:02:35  ssen
 *  Add of -> dev function
 *
 */
 
#import <mach/mach_error.h>
#import <IOKit/IOKitLib.h>

#import <CoreFoundation/CoreFoundation.h>

#include <string.h>
 
#include "bless.h"

int BLGetDeviceForOpenFirmwarePath(BLContext context, char ofstring[], unsigned char mntfrm[]) {
 
    kern_return_t           kret;
    mach_port_t             ourIOKitPort;
    char newof[1024];
    char *beg = newof;

    strcpy(newof, ofstring);

    if(strsep(&beg, ",") == NULL) {
        return 1;
    }

     // Obtain the I/O Kit communication handle.
    if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
      return 2;
    }

    strcpy(mntfrm, "/dev/");

    kret = IOServiceOFPathToBSDName(ourIOKitPort,
                         newof,
                         mntfrm + 5);

    contextprintf(context, kBLLogLevelVerbose,  "bsd name is is %s\n", mntfrm );

    return 0;

 }
