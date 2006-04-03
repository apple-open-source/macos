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
 *  BLLoadFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 30 2002.
 *  Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLLoadFile.c,v 1.14 2005/08/22 20:49:24 ssen Exp $
 *
 *  $Log: BLLoadFile.c,v $
 *  Revision 1.14  2005/08/22 20:49:24  ssen
 *  Change functions to take "char *foo" instead of "char foo[]".
 *  It should be semantically identical, and be more consistent with
 *  other system APIs
 *
 *  Revision 1.13  2005/06/24 16:39:51  ssen
 *  Don't use "unsigned char[]" for paths. If regular char*s are
 *  good enough for the BSD system calls, they're good enough for
 *  bless.
 *
 *  Revision 1.12  2005/02/03 00:42:27  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.11  2004/04/20 21:40:44  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.10  2004/03/21 18:10:04  ssen
 *  Update includes
 *
 *  Revision 1.9  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.8  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.7  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.6  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.5  2003/03/20 04:07:25  ssen
 *  Use _PATH_RSRCFORKSPEC from sys/paths.h
 *
 *  Revision 1.4  2003/03/19 20:27:56  ssen
 *  #include <CF/CF.h> and use full CFData/CFDictionary pointers instead of
 *  void *. Eww, what in the world was I thinking.
 *
 *  Revision 1.3  2002/12/04 05:02:57  ssen
 *  add some newlines at the end of the files
 *
 *  Revision 1.2  2002/06/11 00:50:50  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
 *  Revision 1.1  2002/05/03 04:23:55  ssen
 *  Consolidate APIs, and update bless to use it
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

    *data = (void *)output;
    
    return 0;
}
