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
 *  BLCreateFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLCreateFile.c,v 1.7 2002/05/21 17:34:57 ssen Exp $
 *
 *  $Log: BLCreateFile.c,v $
 *  Revision 1.7  2002/05/21 17:34:57  ssen
 *  dont try to write 0 bytes. CFData is null in that case
 *
 *  Revision 1.6  2002/05/03 04:23:55  ssen
 *  Consolidate APIs, and update bless to use it
 *
 *  Revision 1.5  2002/04/27 17:55:00  ssen
 *  Rewrite output logic to format the string before sending of to logger
 *
 *  Revision 1.4  2002/04/25 07:27:29  ssen
 *  Go back to using errorprint and verboseprint inside library
 *
 *  Revision 1.3  2002/03/04 22:22:50  ssen
 *  O_CREAT the file for both the hfs and non-hfs case
 *
 *  Revision 1.2  2002/02/23 04:13:06  ssen
 *  Update to context-based API
 *
 *  Revision 1.1  2002/02/04 00:49:19  ssen
 *  Add -systemfile option to create a 9 system file
 *
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include "bless.h"
#include "bless_private.h"


int BLCreateFile(BLContext context, void * /* CFDataRef */ data,  unsigned char dest[],
		 unsigned char file[],
		 int useRsrcFork, u_int32_t type, u_int32_t creator) {


    int err;
    int isHFS = 0;
    unsigned char rsrcpath[MAXPATHLEN];
    int mainfd;
        
    err = BLIsMountHFS(context, dest, &isHFS);
    if(err) return 2;


    snprintf(rsrcpath, MAXPATHLEN-1, "%s/%s", dest, file);
    rsrcpath[MAXPATHLEN-1] = '\0';
    mainfd = open(rsrcpath, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(mainfd < 0) {
      contextprintf(context, kBLLogLevelError,  "Could not touch %s\n", rsrcpath );
      return 2;
    }
    close(mainfd);
    
    if(data != NULL) {
      if(isHFS && useRsrcFork) {
	snprintf(rsrcpath, MAXPATHLEN-1, "%s/%s/..namedfork/rsrc", dest, file);
	rsrcpath[MAXPATHLEN-1] = '\0';
	
	err = BLCopyFileFromCFData(context, data, rsrcpath);
	if(err) return 3;
	
	snprintf(rsrcpath, MAXPATHLEN-1, "%s/%s", dest, file);
	rsrcpath[MAXPATHLEN-1] = '\0';
	
      } else {
	err = BLCopyFileFromCFData(context, data, rsrcpath);
	if(err) return 3;
      }
    }


    if(isHFS) {
      err = BLSetTypeAndCreator(context, rsrcpath, type, creator);
      if(err) {
	contextprintf(context, kBLLogLevelError,  "Error while setting type/creator for %s\n", rsrcpath );
	return 4;
      } else {
	contextprintf(context, kBLLogLevelVerbose, "Type/creator set to %4.4s/%4.4s for %s\n",
		      (char *)&type, (char *)&creator, rsrcpath );
      }
    } else {
      contextprintf(context, kBLLogLevelVerbose,  "%s not on HFS file system, type/creator not set\n", rsrcpath );
    }
    return 0;
}
