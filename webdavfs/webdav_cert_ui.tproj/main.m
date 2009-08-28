/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <Security/oidscert.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/certextensions.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFArray.h>
#include <SystemConfiguration/SCValidation.h>
#include <string.h>
#include <AssertMacros.h>

static void *
read_fd(int fd, size_t * ret_size)
{
	void *buf;
	size_t offset;
	size_t remaining;
	size_t size;

	size = 4096;
	buf = malloc(size);
	require(buf != NULL, malloc);

	offset = 0;
	remaining = size - offset;
	while ( TRUE )
	{
		size_t read_count;

		if (remaining == 0)
		{
			size *= 2;
			buf = reallocf(buf, size);
			require(buf != NULL, reallocf);
			
			remaining = size - offset;
		}
		
		read_count = read(fd, buf + offset, remaining);
		require(read_count >= 0, read);
		
		if (read_count == 0)
		{
			/* EOF */
			break;
		}
		
		offset += read_count;
		remaining -= read_count;
	}
	
	require(offset != 0, no_input);
	
	*ret_size = offset;
	return ( buf );


	/* error cases handled here */

no_input:
read:

	free(buf);

reallocf:	
malloc:

	*ret_size = 0;
	return ( NULL );
}

CFPropertyListRef 
my_CFPropertyListCreateFromFileDescriptor(int fd)
{
	void *buf;
	size_t bufsize;
	CFDataRef data;
	CFPropertyListRef plist;

	plist = NULL;

	buf = read_fd(fd, &bufsize);
	require(buf != NULL, read_fd);

	data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buf, bufsize, kCFAllocatorNull);
	require(data != NULL, CFDataCreateWithBytesNoCopy);

	plist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL);

	CFRelease(data);

CFDataCreateWithBytesNoCopy:

	free(buf);

read_fd:

	return (plist);
}

CFDictionaryRef the_dict;

/*
 * This program exits with one of the following values:
 * 0 = Continue button selected
 * 1 = Cancel button selected
 * 2 = an unexpected error was encountered
 */
int main(int argc, char *argv[])
{
	the_dict = my_CFPropertyListCreateFromFileDescriptor(STDIN_FILENO);
	if (isA_CFDictionary(the_dict) == NULL)
	{
		return ( 2 );
	}
	else
	{
		return ( NSApplicationMain(argc, (const char **) argv) );
	}
}
