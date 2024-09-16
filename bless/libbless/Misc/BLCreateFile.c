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


int BLCreateFileWithOptions(BLContextPtr context, const CFDataRef data,
                            const char * file, int setImmutable,
                            uint32_t type, uint32_t creator, int shouldPreallocate)
{
	int err = 0;
	int mainfd = 0;
	struct stat sb;
	const char *rsrcpath = file;

	// Create file descriptor and don't close till IMMUTABLE FLAG will be cleared to avoid vulnerability
	// Following scenario can cause to code vulnerability
	// 1) Verify if the supplied link is regular file
	// 2) user can chflag and replace file with the symlink to some system file or SIP protected file which has IMMUTABLE flag
	// 3) run chflag clear IMMUTABLE flag and in this case it will try
	//    to clear IMMUTABLE flag from the file symlink is pointing to
	// 4) If it points to SIP protected area it will cause to panic.
	mainfd = open(rsrcpath, O_RDONLY|O_NOFOLLOW);
	if(mainfd < 0) {
		err = errno;
		if (err == ENOENT) {
			err = 0;
			goto file_create;
		}
		contextprintf(context, kBLLogLevelError,  "Could not open file descriptor for %s : %s\n",
					  rsrcpath, strerror(err));
		return err;
	}

	err = fstat(mainfd, &sb);
	if(err) {
		err = errno;
		contextprintf(context, kBLLogLevelError,  "Can't access %s: %s\n", rsrcpath, strerror(err));
		goto exit;
	}

	if(!S_ISREG(sb.st_mode)) {
		contextprintf(context, kBLLogLevelError, "%s is not a regular file\n", rsrcpath);
		err = EINVAL;
		goto exit;
	}

	if(sb.st_flags & UF_IMMUTABLE) {
		uint32_t newflags = sb.st_flags & ~UF_IMMUTABLE;

		contextprintf(context, kBLLogLevelVerbose, "Removing UF_IMMUTABLE from %s\n", rsrcpath);
		err = fchflags(mainfd, newflags);
		if(err) {
			err = errno;
			contextprintf(context, kBLLogLevelError,
						  "Can't remove UF_IMMUTABLE from %s: %s\n", rsrcpath,
						  strerror(err));
			goto exit;
		}
	}
	close(mainfd);

	contextprintf(context, kBLLogLevelVerbose, "Deleting old %s\n", rsrcpath);
	err = unlink(rsrcpath);
	if(err) {
		err = errno;
		contextprintf(context, kBLLogLevelError,
					  "Can't delete %s: %s\n", rsrcpath,
					  strerror(err));
		return err;
	}

file_create:
	// Create new file that will replace the previous one or create from the scratch
	mainfd = open(rsrcpath, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if(mainfd < 0) {
		err = errno;
		contextprintf(context, kBLLogLevelError,  "Could not touch %s %s\n", rsrcpath, strerror(err));
		return err;
	}

	if(data != NULL) {
		err = BLCopyFileFromCFData(context, data, rsrcpath, shouldPreallocate);
		if(err) {
			goto exit;
		}
	}

	if (type || creator) {
		err = BLSetTypeAndCreator(context, rsrcpath, type, creator);
		if(err) {
			contextprintf(context, kBLLogLevelError,
						  "Error while setting type/creator for %s\n", rsrcpath );
			goto exit;
		} else {
			char printType[5], printCreator[5];
			contextprintf(context, kBLLogLevelVerbose, "Type/creator set to %4.4s/%4.4s for %s\n",
						  blostype2string(type, printType, sizeof(printType)),
						  blostype2string(creator, printCreator, sizeof(printCreator)), rsrcpath);
		}
	}

	if(setImmutable) {
		contextprintf(context, kBLLogLevelVerbose, "Setting UF_IMMUTABLE on %s\n", rsrcpath);
		err = fstat(mainfd, &sb);
		if(err) {
			err = errno;
			contextprintf(context, kBLLogLevelError, "Can't stat %s: %s\n", rsrcpath, strerror(errno));
			goto exit;
		}
		err = fchflags(mainfd, sb.st_flags | UF_IMMUTABLE);
		if(err && errno != ENOTSUP) {
			err = errno;
			contextprintf(context, kBLLogLevelError,
						  "Can't set UF_IMMUTABLE on %s: %s\n", rsrcpath,
						  strerror(err));
		}
	}

exit:
	close(mainfd);

	return err;
}



int BLCreateFile(BLContextPtr context, const CFDataRef data,
                 const char * file, int setImmutable,
                 uint32_t type, uint32_t creator)
{
	return BLCreateFileWithOptions(context, data, file, setImmutable, type, creator, kMustPreallocate);
}
