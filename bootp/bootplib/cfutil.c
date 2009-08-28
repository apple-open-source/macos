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
#include <arpa/inet.h>
#include <net/ethernet.h>
#include "cfutil.h"

#include <SystemConfiguration/SCValidation.h>
#include <CoreFoundation/CFData.h>

__private_extern__ void
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
read_file(const char * filename, size_t * data_length)
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
write_file(const char * filename, const void * data, size_t data_length)
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

__private_extern__ CFPropertyListRef 
my_CFPropertyListCreateFromFile(const char * filename)
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

__private_extern__ int
my_CFPropertyListWriteFile(CFPropertyListRef plist, const char * filename)
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
		     (const void *)CFDataGetBytePtr(data), CFDataGetLength(data));
    CFRelease(data);
    return (ret);
}

__private_extern__ int
my_CFStringToCStringAndLengthExt(CFStringRef cfstr, char * str, int len,
				 boolean_t is_external)
{
    CFIndex		ret_len = 0;

    CFStringGetBytes(cfstr, CFRangeMake(0, CFStringGetLength(cfstr)),
		     kCFStringEncodingUTF8, 0, is_external,
		     (UInt8 *)str, len - 1, &ret_len);
    if (str != NULL) {
	str[ret_len] = '\0';
    }
    return (ret_len + 1); /* leave 1 byte for nul-termination */
}

__private_extern__ Boolean
my_CFStringArrayToCStringArray(CFArrayRef arr, char * buffer, int * buffer_size,
			       int * ret_count)
{
    int		count = CFArrayGetCount(arr);
    int 	i;
    char *	offset = NULL;	
    int		space;
    char * *	strlist = NULL;

    space = count * sizeof(char *);
    if (buffer != NULL) {
	if (*buffer_size < space) {
	    /* not enough space for even the pointer list */
	    return (FALSE);
	}
	strlist = (char * *)buffer;
	offset = buffer + space; /* the start of the 1st string */
    }
    for (i = 0; i < count; i++) {
	CFIndex		len = 0;
	CFStringRef	str;

	str = CFArrayGetValueAtIndex(arr, i);
	if (isA_CFString(str) == NULL) {
	    return (FALSE);
	}
	if (buffer != NULL) {
	    len = *buffer_size - space;
	    if (len < 0) {
		return (FALSE);
	    }
	}
	len = my_CFStringToCStringAndLength(str, offset, len);
	if (buffer != NULL) {
	    strlist[i] = offset;
	    offset += len;
	}
	space += len;
    }
    *buffer_size = roundup(space, sizeof(char *));
    *ret_count = count;
    return (TRUE);
}

__private_extern__ Boolean
my_CFStringArrayToEtherArray(CFArrayRef array, char * buffer, int * buffer_size,
			     int * ret_count)
{
    int			count = CFArrayGetCount(array);
    int 		i;
    struct ether_addr * list = NULL;
    int			space;

    space = roundup(count * sizeof(*list), sizeof(char *));
    if (buffer != NULL) {
	if (*buffer_size < space) {
	    /* not enough space for all elements */
	    return (FALSE);
	}
	list = (struct ether_addr *)buffer;
    }
    for (i = 0; i < count; i++) {
	struct ether_addr * 	eaddr;
	CFStringRef		str = CFArrayGetValueAtIndex(array, i);
	char			val[64];

	if (isA_CFString(str) == NULL) {
	    return (FALSE);
	}
	if (CFStringGetCString(str, val, sizeof(val), kCFStringEncodingASCII)
	    == FALSE) {
	    return (FALSE);
	}
	eaddr = ether_aton((char *)val);
	if (eaddr == NULL) {
	    return (FALSE);
	}
	if (list != NULL) {
	    list[i] = *eaddr;
	}
    }
    *buffer_size = space;
    *ret_count = count;
    return (TRUE);
}

__private_extern__ bool
my_CFStringToIPAddress(CFStringRef str, struct in_addr * ret_ip)
{
    char		buf[64];
    struct in_addr	ip;

    if (CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingASCII)
	== FALSE) {
	return (FALSE);
    }
    if (inet_aton(buf, &ip) == 1) {
	*ret_ip = ip;
	return (TRUE);
    }
    return (FALSE);
}

__private_extern__ bool
my_CFStringToNumber(CFStringRef str, uint32_t * ret_val)
{
    char		buf[64];
    unsigned long	val;

    my_CFStringToCStringAndLength(str, buf, sizeof(buf));
    val = strtoul(buf, NULL, 0);
    if (val != ULONG_MAX && errno != ERANGE) {
	*ret_val = (uint32_t)val;
	return (TRUE);
    }
    return (FALSE);
}

__private_extern__ bool
my_CFTypeToNumber(CFTypeRef element, uint32_t * l_p)
{
    if (isA_CFString(element) != NULL) {
	if (my_CFStringToNumber(element, l_p) == FALSE) {
	    return (FALSE);
	}
    }
    else if (isA_CFBoolean(element) != NULL) {
	*l_p = CFBooleanGetValue(element);
    }
    else if (isA_CFNumber(element) != NULL) {
	if (CFNumberGetValue(element, kCFNumberSInt32Type, l_p) 
	    == FALSE) {
	    return (FALSE);
	}
    }
    else {
	return (FALSE);
    }
    return (TRUE);
}

