/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
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
 * cfutil.c
 * - CF utility functions
 */

/* 
 * Modification History
 *
 * February 15, 2002 	Dieter Siegmund (dieter@apple.com)
 * - broken out of ipconfigd.c
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/param.h>
#include "cfutil.h"

#include <CoreFoundation/CFData.h>

void
my_CFRelease(void * t)
{
    void * * obj = (void * *)t;
    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

static void *
read_file(char * filename, size_t * data_length)
{
    void *		data = NULL;
    size_t		len = 0;
    int			fd = -1;
    struct stat		sb;

    *data_length = 0;
    if (stat(filename, &sb) < 0)
	goto done;
    len = sb.st_size;
    if (len == 0)
	goto done;

    data = malloc(len);
    if (data == NULL)
	goto done;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
	goto done;

    if (read(fd, data, len) != len) {
	goto done;
    }
 done:
    if (fd >= 0)
	close(fd);
    if (data) {
	*data_length = len;
    }
    return (data);
}

static int
write_file(char * filename, void * data, size_t data_length)
{
    char		path[MAXPATHLEN];
    int			fd = -1;
    int			ret = 0;

    snprintf(path, sizeof(path), "%s-", filename);
    fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0) {
	ret = -1;
	goto done;
    }

    if (write(fd, data, data_length) != data_length) {
	ret = -1;
	goto done;
    }
    rename(path, filename);
 done:
    if (fd >= 0) {
	close(fd);
    }
    return (ret);
}

CFPropertyListRef 
my_CFPropertyListCreateFromFile(char * filename)
{
    void *		buf;
    size_t		bufsize;
    CFDataRef		data = NULL;
    CFPropertyListRef	plist = NULL;

    buf = read_file(filename, &bufsize);
    if (buf == NULL) {
	return (NULL);
    }
    data = CFDataCreateWithBytesNoCopy(NULL, buf, bufsize, kCFAllocatorNull);
    if (data == NULL) {
	goto done;
    }
    plist = CFPropertyListCreateFromXMLData(NULL, data, 
					    kCFPropertyListImmutable,
					    NULL);
 done:
    if (data)
	CFRelease(data);
    if (buf)
	free(buf);
    return (plist);
}

int
my_CFPropertyListWriteFile(CFPropertyListRef plist, char * filename)
{
    CFDataRef	data;
    int		ret;

    if (plist == NULL)
	return (0);

    data = CFPropertyListCreateXMLData(NULL, plist);
    if (data == NULL) {
	return (0);
    }
    ret = write_file(filename, 
		     (void *)CFDataGetBytePtr(data), CFDataGetLength(data));
    CFRelease(data);
    return (ret);
}

