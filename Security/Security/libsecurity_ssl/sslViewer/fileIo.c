/*
 * Copyright (c) 2001-2003,2006-2012,2014 Apple Inc. All Rights Reserved.
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
 * fileIo.c - simple file read/write utilities
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fileIo.h"

int writeFile(
	const char			*fileName,
	const unsigned char	*bytes,
	unsigned			numBytes)
{
	int		rtn;
	int 	fd;

	fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if(fd < 0) {
		return errno;
	}
	rtn = lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		return errno;
	}
	rtn = write(fd, bytes, (size_t)numBytes);
	if(rtn != (int)numBytes) {
		if(rtn >= 0) {
			printf("writeFile: short write\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
	}
	close(fd);
	return rtn;
}

/*
 * Read entire file.
 */
int readFile(
	const char		*fileName,
	unsigned char	**bytes,		// mallocd and returned
	unsigned		*numBytes)		// returned
{
	int rtn;
	int fd;
	unsigned char *buf;
	struct stat	sb;
	unsigned size;

	*numBytes = 0;
	*bytes = NULL;
	fd = open(fileName, O_RDONLY, 0);
	if(fd < 0) {
		return errno;
	}
	rtn = fstat(fd, &sb);
	if(rtn) {
		goto errOut;
	}
	size = sb.st_size;
	buf = malloc(size);
	if(buf == NULL) {
		rtn = ENOMEM;
		goto errOut;
	}
	rtn = lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		goto errOut;
	}
	rtn = read(fd, buf, (size_t)size);
	if(rtn != (int)size) {
		if(rtn >= 0) {
			printf("readFile: short read\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
		*bytes = buf;
		*numBytes = size;
	}
errOut:
	close(fd);
	return rtn;
}
