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
 *  BLUpdateHFSWrapper.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Jun 26 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLUpdateHFSWrapper.c,v 1.6 2002/05/21 17:36:09 ssen Exp $
 *
 *  $Log: BLUpdateHFSWrapper.c,v $
 *  Revision 1.6  2002/05/21 17:36:09  ssen
 *  wrong path
 *
 *  Revision 1.5  2002/05/03 04:23:55  ssen
 *  Consolidate APIs, and update bless to use it
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
 *  Revision 1.8  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "bless.h"

extern int errno;

#define OSXBOOTFILE "OSXBoot!"
#define kNameLocked   0x1000
#define kIsInvisible  0x4000

static int makeMarker(BLContext context, unsigned char mountpt[], unsigned char file[]);

int BLUpdateHFSWrapper(BLContext context, unsigned char mountpt[], unsigned char system[]) {

    unsigned char newsystem[MAXPATHLEN];
    CFDataRef fileData = NULL;
    int err;
    
        
    snprintf(newsystem, MAXPATHLEN, "%s/System", mountpt);
       
    err = makeMarker(context, mountpt, OSXBOOTFILE);
    if(err) {
        return 2;
    }

    BLSetFinderFlag(context, newsystem, kNameLocked, 0);

    err = BLLoadFile(context, system, 0, (void **)&fileData);
    if(err) {
        contextprintf(context, kBLLogLevelError,  "Error while loading %s\n", system );
        return 3;
    }

    err = BLCreateFile(context, (void *)fileData, mountpt, "System", 1, 'zsys', 'MACS');

    if(err) {
        contextprintf(context, kBLLogLevelError,  "Error while copying %s to %s\n", system, newsystem );
        return 3;
    }
    
    BLSetFinderFlag(context, newsystem, kNameLocked, 1);
    return 0;
}


static int makeMarker(BLContext context, unsigned char mountpt[], unsigned char file[]) {
    int err;
    unsigned char marker[MAXPATHLEN];

    snprintf(marker, MAXPATHLEN, "%s/%s", mountpt, file);

    err = BLCreateFile(context, NULL, mountpt, file, 0, 'boot', 'jonb');


    err = BLSetFinderFlag(context, marker, kIsInvisible, 1);
    if (err) {
      contextprintf(context, kBLLogLevelError,  "Couldn't make special marker file invisible\n" );
      return 3;
    }


    return 0;
}

