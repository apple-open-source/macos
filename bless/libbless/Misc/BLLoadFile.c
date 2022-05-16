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
 *  BLLoadFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 30 2002.
 *  Copyright (c) 2002-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLLoadFile.c,v 1.15 2006/02/20 22:49:56 ssen Exp $
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <string.h>

#include <sys/paths.h>
#include <sys/param.h>

#include "bless.h"
#include "bless_private.h"


int BLLoadFile(BLContextPtr context, const char * src, int useRsrcFork,
    CFDataRef* data) {

    int err = 0;
    int isHFS = 0;
    CFDataRef                output = NULL;
    CFURLRef                 loadSrc;
    char rsrcpath[MAXPATHLEN];

	if (data) *data = NULL;
    if(useRsrcFork) {
        err = BLIsMountHFS(context, src, &isHFS);
        if(err) return 2;

        if(isHFS) {
            snprintf(rsrcpath, MAXPATHLEN-1, "%s"_PATH_RSRCFORKSPEC, src);
        } else {
            strncpy(rsrcpath, src, MAXPATHLEN-1);
        }    
    } else {
        strncpy(rsrcpath, src, MAXPATHLEN-1);    
    }

    rsrcpath[MAXPATHLEN-1] = '\0';

    loadSrc = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
													  (UInt8 *)rsrcpath,
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

    *data = output;
    
    return 0;
}
