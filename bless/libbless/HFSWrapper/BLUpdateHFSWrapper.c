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
 *  BLUpdateHFSWrapper.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Jun 26 2001.
 *  Copyright (c) 2001-2003 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLUpdateHFSWrapper.c,v 1.12 2003/07/22 15:58:33 ssen Exp $
 *
 *  $Log: BLUpdateHFSWrapper.c,v $
 *  Revision 1.12  2003/07/22 15:58:33  ssen
 *  APSL 2.0
 *
 *  Revision 1.11  2003/04/19 00:11:11  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.10  2003/04/16 23:57:32  ssen
 *  Update Copyrights
 *
 *  Revision 1.9  2003/03/20 03:41:01  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.8.2.1  2003/03/20 02:22:30  ssen
 *  Include CoreServices.h and use Finder Flag constants from Finder.h
 *
 *  Revision 1.8  2003/03/19 20:27:53  ssen
 *  #include <CF/CF.h> and use full CFData/CFDictionary pointers instead of
 *  void *. Eww, what in the world was I thinking.
 *
 *  Revision 1.7  2002/06/11 00:50:48  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
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
#include <CoreServices/CoreServices.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "bless.h"
#include "bless_private.h"

extern int errno;

#define OSXBOOTFILE "OSXBoot!"

static int makeMarker(BLContextPtr context, unsigned char mountpt[], unsigned char file[]);

int BLUpdateHFSWrapper(BLContextPtr context, unsigned char mountpt[], unsigned char system[]) {

    unsigned char newsystem[MAXPATHLEN];
    CFDataRef fileData = NULL;
    int err;
    
        
    snprintf(newsystem, MAXPATHLEN, "%s/System", mountpt);
       
    err = makeMarker(context, mountpt, OSXBOOTFILE);
    if(err) {
        return 2;
    }

    BLSetFinderFlag(context, newsystem, kNameLocked, 0);

    err = BLLoadFile(context, system, 0, &fileData);
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


static int makeMarker(BLContextPtr context, unsigned char mountpt[], unsigned char file[]) {
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

