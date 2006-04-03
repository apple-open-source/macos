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
 *  BLCreateFile.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Tue Apr 17 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLCreateFile.c,v 1.25 2005/10/27 21:06:52 ssen Exp $
 *
 *  $Log: BLCreateFile.c,v $
 *  Revision 1.25  2005/10/27 21:06:52  ssen
 *  Simplify BLCreateFile() a lot, since we no longer care
 *  about writing to the resource fork of a file, which also means
 *  we don't have to pass in the parent directory.
 *
 *  From bless.h, remove old wrapper routines.
 *
 *  Revision 1.24  2005/08/22 20:49:24  ssen
 *  Change functions to take "char *foo" instead of "char foo[]".
 *  It should be semantically identical, and be more consistent with
 *  other system APIs
 *
 *  Revision 1.23  2005/06/24 16:39:51  ssen
 *  Don't use "unsigned char[]" for paths. If regular char*s are
 *  good enough for the BSD system calls, they're good enough for
 *  bless.
 *
 *  Revision 1.22  2005/02/04 20:30:43  ssen
 *  try to set type/creator even if not HFS+. UFS supports this now
 *
 *  Revision 1.21  2005/02/03 00:42:27  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.20  2005/01/08 07:13:48  ssen
 *  <rdar://problem/3036819> Any unpriv'd user can make Mac OS X unbootable (hardlink BootX)
 *  Add support to setting the "uchg" or UF_IMMUTABLE bit on
 *  BootX when creating or updating it. Note that if we encounter
 *  an existing file with this bit, we need to remove it before
 *  doing stuff.
 *
 *  Revision 1.19  2004/04/20 21:40:44  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.18  2003/10/17 00:10:39  ssen
 *  add more const
 *
 *  Revision 1.17  2003/07/22 15:58:34  ssen
 *  APSL 2.0
 *
 *  Revision 1.16  2003/05/20 14:43:58  ssen
 *  pass isHFS status explicitly to avoid trying to preallocate on UFS
 *
 *  Revision 1.15  2003/04/23 00:07:58  ssen
 *  Use blostype2string for OSTypes
 *
 *  Revision 1.14  2003/04/19 00:11:12  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.13  2003/04/16 23:57:33  ssen
 *  Update Copyrights
 *
 *  Revision 1.12  2003/03/20 04:07:25  ssen
 *  Use _PATH_RSRCFORKSPEC from sys/paths.h
 *
 *  Revision 1.11  2003/03/20 03:41:03  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.10.2.2  2003/03/20 03:18:52  ssen
 *  typo
 *
 *  Revision 1.10.2.1  2003/03/20 03:18:23  ssen
 *  swap type/creator for display
 *
 *  Revision 1.10  2003/03/19 22:57:06  ssen
 *  C99 types
 *
 *  Revision 1.8  2002/06/11 00:50:49  ssen
 *  All function prototypes need to use BLContextPtr. This is really
 *  a minor change in all of the files.
 *
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
#include <sys/paths.h>

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
	
	if(setImmutable) {
		contextprintf(context, kBLLogLevelVerbose, "Setting UF_IMMUTABLE on %s\n",
					  rsrcpath);					
		err = chflags(rsrcpath, UF_IMMUTABLE);
		if(err) {
			contextprintf(context, kBLLogLevelError, 
						  "Can't set UF_IMMUTABLE on %s: %s\n", rsrcpath,
						  strerror(errno));		
			return 5;				
		}
	}
	
    return 0;
}
