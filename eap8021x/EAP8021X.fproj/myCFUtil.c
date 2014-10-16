/*
 * Copyright (c) 2001-2014 Apple Inc. All rights reserved.
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
#include <CoreFoundation/CFUUID.h>
#include <CoreFoundation/CFNumber.h>
#include <SystemConfiguration/SCValidation.h>
#include "myCFUtil.h"

static const CFStringEncoding	S_encodings[] = {
    kCFStringEncodingUTF8,
    kCFStringEncodingMacRoman
};
static const int		S_encodings_count = (sizeof(S_encodings)
						     / sizeof(S_encodings[0]));

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

static char *
S_copy_cstring(CFStringRef cfstr, CFRange range, CFStringEncoding encoding)
{
    CFIndex		l;
    uint8_t *		str;

    CFStringGetBytes(cfstr, range, encoding, 0, FALSE, NULL, 0, &l);
    if (l <= 0) {
	return (NULL);
    }
    str = (uint8_t *)malloc(l + 1);
    CFStringGetBytes(cfstr, range, encoding, 0, FALSE, str, l, &l);
    str[l] = '\0';
    return ((char *)str);
}

char *
my_CFStringToCString(CFStringRef cfstr, CFStringEncoding encoding)
{
    int			i;
    CFRange		range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    if (encoding != kCFStringEncodingInvalidId) {
	return (S_copy_cstring(cfstr, range, encoding));
    }

    /* try each encoding in turn, and return the first non-NULL value */
    for (i = 0; i < S_encodings_count; i++) {
	char *		str;

	str = S_copy_cstring(cfstr, range, S_encodings[i]);
	if (str != NULL) {
	    return (str);
	}
    }
    return (NULL);
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

CFPropertyListRef 
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
    data = CFDataCreate(NULL, buf, bufsize);
    if (data == NULL) {
	goto done;
    }
    plist = CFPropertyListCreateWithData(NULL, data, 
					 kCFPropertyListImmutable,
					 NULL, NULL);
 done:
    if (data)
	CFRelease(data);
    if (buf)
	free(buf);
    return (plist);
}

int
my_CFPropertyListWriteFile(CFPropertyListRef plist, const char * filename)
{
    CFDataRef	data;
    int		ret;

    if (plist == NULL)
	return (0);

    data = CFPropertyListCreateData(NULL, plist,
				    kCFPropertyListXMLFormat_v1_0,
				    0, NULL);
    if (data == NULL) {
	return (0);
    }
    ret = write_file(filename, 
		     (const void *)CFDataGetBytePtr(data),
		     CFDataGetLength(data));
    CFRelease(data);
    return (ret);
}

CFPropertyListRef
my_CFPropertyListCreateWithBytePtrAndLength(const void * data, int data_len)
{
    CFPropertyListRef	plist;
    CFDataRef		xml_data;

    xml_data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)data,
					   data_len,
					   kCFAllocatorNull);
    if (xml_data == NULL) {
	return (NULL);
    }
    plist = CFPropertyListCreateWithData(NULL, xml_data,
					 kCFPropertyListImmutable,
					 NULL, NULL);
    CFRelease(xml_data);
    return (plist);
}

Boolean
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

CFStringRef
my_CFUUIDStringCreate(CFAllocatorRef alloc)
{
    CFUUIDRef 	uuid;
    CFStringRef	uuid_str;

    uuid = CFUUIDCreate(alloc);
    uuid_str = CFUUIDCreateString(alloc, uuid);
    CFRelease(uuid);
    return (uuid_str);
}

CFStringRef
my_CFStringCreateWithData(CFDataRef data)
{
    CFAllocatorRef 	allocator = CFGetAllocator(data);
    int			i;
    CFStringRef		str;

    for (i = 0; i < S_encodings_count; i++) {
	str = CFStringCreateWithBytes(allocator,
				      CFDataGetBytePtr(data),
				      CFDataGetLength(data),
				      S_encodings[i],
				      FALSE);
	if (str != NULL) {
	    return (str);
	}
    }
    return (NULL);
}

CFStringRef
my_CFStringCreateWithCString(const char * cstr)
{
    int			i;
    CFStringRef		str;

    for (i = 0; i < S_encodings_count; i++) {
	str = CFStringCreateWithCString(NULL, cstr,
					S_encodings[i]);
	if (str != NULL) {
	    return (str);
	}
    }
    return (NULL);
}

CFDataRef
my_CFDataCreateWithString(CFStringRef str)
{
    CFDataRef		data;
    int			i;

    for (i = 0; i < S_encodings_count; i++) {
	data = CFStringCreateExternalRepresentation(NULL, str, 
						    S_encodings[i], 0);
	if (data != NULL) {
	    return (data);
	}
    }
    return (NULL);
}

void
my_FieldSetRetainedCFType(void * field_p, const void * v)
{
    CFTypeRef	existing = *((CFTypeRef *)field_p);
    CFTypeRef	val = (CFTypeRef)v;

    if (val != NULL) {
	CFRetain(val);
    }
    if (existing != NULL) {
	CFRelease(existing);
    }
    *((CFTypeRef *)field_p) = val;
    return;
}

CFStringRef
my_CFPropertyListCopyAsXMLString(CFPropertyListRef plist)
{
    CFDataRef	data;
    CFStringRef	str;

    data = CFPropertyListCreateData(NULL, plist,
				    kCFPropertyListXMLFormat_v1_0, 0, NULL);
    if (data == NULL) {
	return (NULL);
    }
    str = CFStringCreateWithBytes(NULL, 
				  CFDataGetBytePtr(data),
				  CFDataGetLength(data),
				  kCFStringEncodingUTF8,
				  FALSE);
    CFRelease(data);
    return (str);
}

vm_address_t
my_CFPropertyListCreateVMData(CFPropertyListRef plist,
			      mach_msg_type_number_t * 	ret_data_len)
{
    vm_address_t	data;
    int			data_len;
    kern_return_t	status;
    CFDataRef		xml_data;

    data = 0;
    *ret_data_len = 0;
    xml_data = CFPropertyListCreateData(NULL, plist,
					kCFPropertyListBinaryFormat_v1_0,
					0, NULL);
    if (xml_data == NULL) {
	goto done;
    }
    data_len = (int)CFDataGetLength(xml_data);
    status = vm_allocate(mach_task_self(), &data, data_len, TRUE);
    if (status != KERN_SUCCESS) {
	goto done;
    }
    bcopy((char *)CFDataGetBytePtr(xml_data), (char *)data, data_len);
    *ret_data_len = data_len;

 done:
    my_CFRelease(&xml_data);
    return (data);
}

/* 
 * Function: my_CFStringCopyComponent
 * Purpose:
 *    Separates the given string using the given separator, and returns
 *    the component at the specified index.
 * Returns:
 *    NULL if no such component exists, non-NULL component otherwise
 */
CFStringRef
my_CFStringCopyComponent(CFStringRef path, CFStringRef separator, 
			 CFIndex component_index)
{
    CFArrayRef		arr;
    CFStringRef		component = NULL;

    arr = CFStringCreateArrayBySeparatingStrings(NULL, path, separator);
    if (arr == NULL) {
	goto done;
    }
    if (CFArrayGetCount(arr) <= component_index) {
	goto done;
    }
    component = CFRetain(CFArrayGetValueAtIndex(arr, component_index));

 done:
    my_CFRelease(&arr);
    return (component);

}

