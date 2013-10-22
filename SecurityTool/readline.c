/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 * readline.c
 */

#include "readline.h"
#include "security.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Read a line from stdin into buffer as a null terminated string.  If buffer is
   non NULL use at most buffer_size bytes and return a pointer to buffer.  Otherwise
   return a newly malloced buffer.
   if EOF is read this function returns NULL.  */
char *
readline(char *buffer, int buffer_size)
{
	int ix = 0, bytes_malloced = 0;

	if (!buffer)
	{
		bytes_malloced = 64;
		buffer = (char *)malloc(bytes_malloced);
		buffer_size = bytes_malloced;
	}

	for (;;++ix)
	{
		int ch;

		if (ix == buffer_size - 1)
		{
			if (!bytes_malloced)
				break;
			bytes_malloced += bytes_malloced;
			buffer = (char *)realloc(buffer, bytes_malloced);
			buffer_size = bytes_malloced;
		}

		ch = getchar();
		if (ch == EOF)
		{
			if (bytes_malloced)
				free(buffer);
			return NULL;
		}
		if (ch == '\n')
			break;
		buffer[ix] = ch;
	}

	/* 0 terminate buffer. */
	buffer[ix] = '\0';

	return buffer;
}

/* Read the file name into buffer.  On return buffer contains a newly
   malloced buffer or length buffer_size. Return 0 on success and -1 on failure.  */
int
read_file(const char *name, CSSM_DATA *outData)
{
	int fd, result;
	char *buffer = NULL;
	off_t length;
	ssize_t bytes_read;

	do {
		fd = open(name, O_RDONLY, 0);
	} while (fd == -1 && errno == EINTR);

	if (fd == -1)
	{
		sec_error("open %s: %s", name, strerror(errno));
		result = -1;
		goto loser;
	}

	length = lseek(fd, 0, SEEK_END);
	if (length == -1)
	{
		sec_error("lseek %s, SEEK_END: %s", name, strerror(errno));
		result = -1;
		goto loser;
	}

	buffer = malloc(length);

	do {
		bytes_read = pread(fd, buffer, length, 0);
	} while (bytes_read == -1 && errno == EINTR);

	if (bytes_read == -1)
	{
		sec_error("pread %s: %s", name, strerror(errno));
		result = -1;
		goto loser;
	}
	if (bytes_read != (ssize_t)length)
	{
		sec_error("read %s: only read %d of %qu bytes", name, bytes_read, length);
		result = -1;
		goto loser;
	}

	do {
		result = close(fd);
	} while (result == -1 && errno == EINTR);
	if (result == -1)
	{
		sec_error("close %s: %s", name, strerror(errno));
		goto loser;
	}

	outData->Data = (uint8 *)buffer;
	outData->Length = (uint32)length;

	return result;

loser:
	if (buffer)
		free(buffer);

	return result;
}
