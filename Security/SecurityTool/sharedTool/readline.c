/*
 * Copyright (c) 2003-2004,2006-2010,2013-2014 Apple Inc. All Rights Reserved.
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

/* Inspects a file's existence and size.  Returns a file handle or -1 on failure */

int inspect_file(const char* name, off_t *out_off_end) {
    int fd = -1;
    off_t off_end;

    do {
        fd = open(name, O_RDONLY, 0);
    } while (fd == -1 && errno == EINTR);

    if (fd == -1) {
        fprintf(stderr, "open %s: %s", name, strerror(errno));
        return -1;
    }

    off_end = lseek(fd, 0, SEEK_END);
    if (off_end == -1) {
        fprintf(stderr, "lseek %s, SEEK_END: %s", name, strerror(errno));
        return -1;
    }

    if (out_off_end) {
        *out_off_end = off_end;
    }

    return fd;
}

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

CFDataRef copyFileContents(const char *path) {
    CFMutableDataRef data = NULL;
    int fd = open(path, O_RDONLY, 0666);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s", path, strerror(errno));
        goto badFile;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    if (fsize == (off_t)-1) {
        fprintf(stderr, "lseek %s, 0, SEEK_END: %s", path, strerror(errno));
        goto badFile;
    }

	if (fsize > (off_t)INT32_MAX) {
		fprintf(stderr, "file %s too large %llu bytes", path, fsize);
		goto badFile;
	}

    data = CFDataCreateMutable(kCFAllocatorDefault, (CFIndex)fsize);
    CFDataSetLength(data, (CFIndex)fsize);
    void *buf = CFDataGetMutableBytePtr(data);
    off_t total_read = 0;
    while (total_read < fsize) {
        ssize_t bytes_read;

        bytes_read = pread(fd, buf, (size_t)(fsize - total_read), total_read);
        if (bytes_read == -1) {
            fprintf(stderr, "read %s: %s", path, strerror(errno));
            goto badFile;
        }
        if (bytes_read == 0) {
            fprintf(stderr, "read %s: unexpected end of file", path);
            goto badFile;
        }
        total_read += bytes_read;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "close %s: %s", path, strerror(errno));
        /* Failure to close the file isn't fatal. */
    }

    return data;
badFile:
    if (fd != -1) {
        if (close(fd) == -1) {
            fprintf(stderr, "close %s: %s", path, strerror(errno));
        }
    }
    if (data)
        CFRelease(data);
    return NULL;
}


bool writeFileContents(const char *path, CFDataRef data) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s", path, strerror(errno));
        goto badFile;
    }

    const void *buf = CFDataGetBytePtr(data);
    off_t fsize = CFDataGetLength(data);

    off_t total_write = 0;
    while (total_write < fsize) {
        ssize_t bytes_write;

        bytes_write = pwrite(fd, buf, (size_t)(fsize - total_write), total_write);
        if (bytes_write == -1) {
            fprintf(stderr, "write %s: %s", path, strerror(errno));
            goto badFile;
        }
        if (bytes_write == 0) {
            fprintf(stderr, "write %s: unexpected end of file", path);
            goto badFile;
        }
        total_write += bytes_write;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "close %s: %s", path, strerror(errno));
        /* Failure to close the file isn't fatal. */
    }

    return true;
badFile:
    if (fd != -1) {
        if (close(fd) == -1) {
            fprintf(stderr, "close %s: %s", path, strerror(errno));
        }
    }
    if (data)
        CFRelease(data);
    return false;
}
