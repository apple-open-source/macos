/*
 * Copyright (c) 2003-2010 Apple Inc. All rights reserved.
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

#include "symbol_scope.h"

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

void
my_CFDictionarySetTypeAsArrayValue(CFMutableDictionaryRef dict,
				   CFStringRef prop, CFTypeRef val);
void
my_CFDictionarySetIPAddressAsArrayValue(CFMutableDictionaryRef dict,
					CFStringRef prop,
					struct in_addr ip_addr);
void
my_CFArrayAppendUniqueValue(CFMutableArrayRef arr, CFTypeRef new);

CFStringRef
my_CFStringCopyComponent(CFStringRef path, CFStringRef separator, 
			 CFIndex component_index);
CFStringRef
my_CFStringCreateWithIPAddress(const struct in_addr ip);

CFStringRef
my_CFStringCreateWithIPv6Address(const struct in6_addr * ip6_addr);

void
my_CFStringAppendBytesAsHex(CFMutableStringRef str, const uint8_t * bytes,
			    int length, char sep);
char *
my_CFStringToCString(CFStringRef cfstr, CFStringEncoding encoding);

Boolean
my_CFEqual(CFTypeRef val1, CFTypeRef val2);

#endif _S_CFUTIL_H
