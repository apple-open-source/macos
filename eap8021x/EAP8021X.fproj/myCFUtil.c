/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <mach/mach.h>
#include <mach/notify.h>
#include <mach/mach_error.h>
#include <pthread.h>

#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFNumber.h>
#include <SystemConfiguration/SCValidation.h>
#include "myCFUtil.h"

Boolean
my_CFEqual(CFTypeRef val1, CFTypeRef val2)
{
    if (val1 == NULL) {
	if (val2 == NULL) {
	    return (TRUE);
	}
	return (FALSE);
    }
    if (val2 == NULL) {
	return (FALSE);
    }
    return (CFEqual(val1, val2));
}

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

char *
my_CFStringToCString(CFStringRef cfstr, CFStringEncoding encoding)
{
    CFIndex		l;
    CFRange		range;
    uint8_t *		str;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    CFStringGetBytes(cfstr, range, encoding,
		     0, FALSE, NULL, 0, &l);
    if (l <= 0) {
	return (NULL);
    }
    str = (uint8_t *)malloc(l + 1);
    CFStringGetBytes(cfstr, range, encoding, 0, FALSE, str, l, &l);
    str[l] = '\0';
    return ((char *)str);
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
    data = CFDataCreate(NULL, buf, bufsize);
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

Boolean
xmlSerialize(CFPropertyListRef		obj,
	     CFDataRef			*xml,
	     void			**dataRef,
	     CFIndex			*dataLen)
{
    CFDataRef	myXml;

    if (!obj) {
	/* if no object to serialize */
	return FALSE;
    }

    if (!xml && !(dataRef && dataLen)) {
	/* if not keeping track of allocated space */
	return FALSE;
    }

    myXml = CFPropertyListCreateXMLData(NULL, obj);
    if (!myXml) {
	if (xml)	*xml     = NULL;
	if (dataRef)	*dataRef = NULL;
	if (dataLen)	*dataLen = 0;
	return FALSE;
    }

    if (xml) {
	*xml = myXml;
	if (dataRef) {
	    *dataRef = (void *)CFDataGetBytePtr(myXml);
	}
	if (dataLen) {
	    *dataLen = CFDataGetLength(myXml);
	}
    } else {
	kern_return_t	status;

	*dataLen = CFDataGetLength(myXml);
	status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
	if (status != KERN_SUCCESS) {
	    CFRelease(myXml);
	    *dataRef = NULL;
	    *dataLen = 0;
	    return FALSE;
	}

	bcopy((char *)CFDataGetBytePtr(myXml), *dataRef, *dataLen);
	CFRelease(myXml);
    }

    return TRUE;
}

Boolean
xmlUnserialize(CFPropertyListRef	*obj,
	       void			*dataRef,
	       CFIndex			dataLen)
{
    kern_return_t		status;
    CFDataRef		xml;
    CFStringRef		xmlError;

    if (!obj) {
	return FALSE;
    }

    xml = CFDataCreate(NULL, (void *)dataRef, dataLen);
    status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
    if (status != KERN_SUCCESS) {
	/* non-fatal???, proceed */
    }
    *obj = CFPropertyListCreateFromXMLData(NULL,
					   xml,
					   kCFPropertyListImmutable,
					   &xmlError);
    CFRelease(xml);

    if (!obj) {
	if (xmlError) {
	    CFRelease(xmlError);
	}
	return FALSE;
    }
    return TRUE;
}

__private_extern__ Boolean
my_CFDictionaryGetBooleanValue(CFDictionaryRef properties, CFStringRef propname,
			       Boolean def_value)
{
    bool		ret = def_value;

    if (properties != NULL) {
	CFBooleanRef	val;

	val = CFDictionaryGetValue(properties, propname);
	if (isA_CFBoolean(val)) {
	    ret = CFBooleanGetValue(val);
	}
    }
    return (ret);
}
