/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		entropyFileUnix.c

	Contains:	Module to maintain MacYarrow's entropy file, UNIX version.

	Written by:	Doug Mitchell

	Copyright: (c) 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		05/22/00	dpm		Created.
 
*/

#include "entropyFile.h"
#include "debug.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/*
 * For now we use the same file location for all builds. Generally for
 * debugging - when this code is not running as root - you need to do
 * the following once per system before using this code:
 *
 * > su to root
 * # touch /var/db/SystemEntropyCache
 * # chmod 666 /var/db/SystemEntropyCache
 */
#define DEFAULT_ENTROPY_FILE_PATH		"/var/db/SystemEntropyCache"

/* NULL ==> use default, else use caller-specified path */
static char *entropyFilePath = NULL;

static OSErr errNoToOSErr(int err)
{
	switch(err) {
		case ENOENT:
			return fnfErr;
		case EPERM:
			return permErr;
		/* anything else interesting? */
		default:
			return ioErr;
	}
}

static char *getEntropyFilePath()
{
	if(entropyFilePath) {
		return entropyFilePath;
	}
	else {
		return DEFAULT_ENTROPY_FILE_PATH;
	}
}

/*
 * Specify optional entropy file path. If this is never called,
 * this module will use its own default path. 
 */
OSErr setEntropyFilePath(
	const char *path)
{
	unsigned len;
	
	if(entropyFilePath) {
		free(entropyFilePath);
		entropyFilePath = NULL;
	}
	if(path == NULL) {
		return noErr;
	}
	len = strlen(path);
	if(len > 255) {
		/* no can do */
		return bdNamErr;
	}
	entropyFilePath = malloc(len + 1);
	if(entropyFilePath == NULL) {
		return memFullErr;
	}
	memmove(entropyFilePath, path, len + 1);
	return noErr;
}

/*
 * Write specified data to entropy file. A new file will be created
 * if none exists. Existing file's data is replaced with caller's data.
 */
OSErr writeEntropyFile(
	UInt8		*bytes,
	UInt32		numBytes)
{
	int		rtn;
	int 	fd;
	OSErr	ortn;
	
	fd = open(getEntropyFilePath(), O_RDWR | O_CREAT | O_TRUNC, 0600);
	if(fd <= 0) {
		rtn = errno;
		errorLog1("writeEntropyFile: open returned %d\n", rtn);
		return errNoToOSErr(rtn);
	}
	rtn = lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		rtn = errno;
		errorLog1("writeEntropyFile: lseek returned %d\n", rtn);
		return errNoToOSErr(rtn);
	}
	rtn = write(fd, bytes, (size_t)numBytes);
	if(rtn != (int)numBytes) {
		if(rtn < 0) {
			errorLog1("writeEntropyFile: write() returned %d\n", rtn);
			ortn = errNoToOSErr(errno);
		}
		else {
			errorLog0("writeEntropyFile(): short write\n");
			ortn = ioErr;
		}
	}
	else {
		ortn = noErr;
	}
	close(fd);
	return ortn;
}
	
/*
 * Read data from entropy file.
 */
OSErr readEntropyFile(
	UInt8		*bytes,
	UInt32		numBytes,		// max # of bytes to read
	UInt32		*actualBytes)	// RETURNED - number of bytes actually read
{
	int rtn;
	int fd;
	OSErr ortn;
	
	*actualBytes = 0;
	fd = open(getEntropyFilePath(), O_RDONLY, 0);
	if(fd <= 0) {
		rtn = errno;
		errorLog1("readEntropyFile: open returned %d\n", rtn);
		return errNoToOSErr(rtn);
	}
	rtn = lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		rtn = errno;
		errorLog1("readEntropyFile: lseek returned %d\n", rtn);
		return errNoToOSErr(rtn);
	}
	rtn = read(fd, bytes, (size_t)numBytes);
	if(rtn < 0) {
		errorLog1("readEntropyFile: read() returned %d\n", rtn);
		ortn = errNoToOSErr(errno);
	}
	else {
		*actualBytes = (UInt32)rtn;
		ortn = noErr;
	}
	close(fd);
	return ortn;
}
