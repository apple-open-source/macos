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
 *  BLLoadFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 30 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLLoadFile.c,v 1.1 2002/05/03 04:23:55 ssen Exp $
 *
 *  $Log: BLLoadFile.c,v $
 *  Revision 1.1  2002/05/03 04:23:55  ssen
 *  Consolidate APIs, and update bless to use it
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <libc.h>

#include "bless.h"


int BLLoadFile(BLContext context, unsigned char src[], int useRsrcFork,
    void ** /* CFDataRef* */ data) {

    int err = 0;
    int isHFS = 0;
    CFDataRef                output = NULL;
    CFURLRef                 loadSrc;
    unsigned char rsrcpath[MAXPATHLEN];


    if(useRsrcFork) {
        err = BLIsMountHFS(context, src, &isHFS);
        if(err) return 2;

        if(isHFS) {
            snprintf(rsrcpath, MAXPATHLEN-1, "%s/..namedfork/rsrc", src);
        } else {
            strncpy(rsrcpath, src, MAXPATHLEN-1);
        }    
    } else {
        strncpy(rsrcpath, src, MAXPATHLEN-1);    
    }

    rsrcpath[MAXPATHLEN-1] = '\0';

    loadSrc = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, rsrcpath,
                    strlen(rsrcpath), 0);

    if(loadSrc == NULL) {
        return 1;
    }
    
    if(!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
                                            loadSrc,
                                            &output,
                                            NULL,
                                            NULL,
                                            NULL)) {
        contextprintf(context, kBLLogLevelError,  "Can't load %s\n", rsrcpath );
        CFRelease(loadSrc);
        return 2;
    }

    CFRelease(loadSrc); loadSrc = NULL;

    *data = (void *)output;
    
    return 0;
}