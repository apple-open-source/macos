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
 *  BLCreateFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLCreateFile.c,v 1.26 2006/02/20 22:49:56 ssen Exp $
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/paths.h>
#include <errno.h>

#include "bless.h"
#include "bless_private.h"
#include "bless_private.h"


int BLCreateFile(BLContextPtr context, const CFDataRef data,
                 const char * file, int setImmutable,
				 uint32_t type, uint32_t creator) {


    int err;
    int mainfd;
	struct stat sb;
    const char *rsrcpath = file;

	
	err = lstat(rsrcpath, &sb);
	if(err == 0) {
		if(!S_ISREG(sb.st_mode)) {
			contextprintf(context, kBLLogLevelError, "%s is not a regular file\n",
						  rsrcpath);					
			return 1;
		}
		
		if(sb.st_flags & UF_IMMUTABLE) {
			uint32_t newflags = sb.st_flags & ~UF_IMMUTABLE;

			contextprintf(context, kBLLogLevelVerbose, "Removing UF_IMMUTABLE from %s\n",
						  rsrcpath);					
			err = chflags(rsrcpath, newflags);
			if(err) {
				contextprintf(context, kBLLogLevelError, 
							  "Can't remove UF_IMMUTABLE from %s: %s\n", rsrcpath,
							  strerror(errno));		
				return 1;				
			}
			
		}
		
		
		contextprintf(context, kBLLogLevelVerbose, "Deleting old %s\n",
					  rsrcpath);					
		err = unlink(rsrcpath);
		if(err) {
			contextprintf(context, kBLLogLevelError, 
						  "Can't delete %s: %s\n", rsrcpath,
						  strerror(errno));		
			return 1;				
		}
		
	} else if(errno != ENOENT) {
		contextprintf(context, kBLLogLevelError,  "Can't access %s: %s\n", rsrcpath,
					  strerror(errno));		
		return 1;
	} else {
		// ENOENT is OK, we'll create the file now
	}
	
    mainfd = open(rsrcpath, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(mainfd < 0) {
      contextprintf(context, kBLLogLevelError,  "Could not touch %s\n", rsrcpath );
      return 2;
    }
    close(mainfd);
    
    if(data != NULL) {
        err = BLCopyFileFromCFData(context, data, rsrcpath, 1);
        if(err) return 3;
    }

	if (type || creator) {
		err = BLSetTypeAndCreator(context, rsrcpath, type, creator);
		if(err) {
			contextprintf(context, kBLLogLevelError,
						  "Error while setting type/creator for %s\n", rsrcpath );
			return 4;
		} else {
			char printType[5], printCreator[5];
			
			contextprintf(context, kBLLogLevelVerbose, "Type/creator set to %4.4s/%4.4s for %s\n",
						  blostype2string(type, printType),
						  blostype2string(creator, printCreator), rsrcpath );
		}
	}
	
	if(setImmutable) {
		contextprintf(context, kBLLogLevelVerbose, "Setting UF_IMMUTABLE on %s\n",
					  rsrcpath);					
		err = chflags(rsrcpath, UF_IMMUTABLE);
		if(err && errno != ENOTSUP) {
			contextprintf(context, kBLLogLevelError, 
						  "Can't set UF_IMMUTABLE on %s: %s\n", rsrcpath,
						  strerror(errno));		
			return 5;				
		}
	}
	
    return 0;
}
