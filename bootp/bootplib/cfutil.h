/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _S_CFUTIL_H
#define _S_CFUTIL_H

#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>

void
my_CFRelease(void * t);

CFPropertyListRef 
my_CFPropertyListCreateFromFile(const char * filename);

int
my_CFPropertyListWriteFile(CFPropertyListRef plist, const char * filename);

Boolean
my_CFStringArrayToCStringArray(CFArrayRef arr, char * buffer, int * buffer_size,
			       int * ret_count);
Boolean
my_CFStringArrayToEtherArray(CFArrayRef array, char * buffer, int * buffer_size,
			     int * ret_count);
bool
my_CFStringToIPAddress(CFStringRef str, struct in_addr * ret_ip);

int
my_CFStringToCStringAndLengthExt(CFStringRef cfstr, char * str, int len,
				 boolean_t is_external);
static __inline__ int
my_CFStringToCStringAndLength(CFStringRef cfstr, char * str, int len)
{
    return (my_CFStringToCStringAndLengthExt(cfstr, str, len, FALSE));
}

bool
my_CFTypeToNumber(CFTypeRef element, uint32_t * l_p);

bool
my_CFStringToNumber(CFStringRef str, uint32_t * ret_val);

#endif _S_CFUTIL_H
