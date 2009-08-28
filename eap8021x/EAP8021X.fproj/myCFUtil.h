/*
 * Copyright (c) 2001-2008 Apple Inc. All rights reserved.
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

#ifndef _S_MYCFUTIL_H
#define _S_MYCFUTIL_H

/* 
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#include <CoreFoundation/CFString.h>

Boolean
my_CFEqual(CFTypeRef val1, CFTypeRef val2);

void 
my_CFRelease(void * t);

char *
my_CFStringToCString(CFStringRef cfstr, CFStringEncoding encoding);

CFPropertyListRef 
my_CFPropertyListCreateFromFile(const char * filename);

int
my_CFPropertyListWriteFile(CFPropertyListRef plist, const char * filename);

Boolean
my_CFDictionaryGetBooleanValue(CFDictionaryRef properties, CFStringRef propname,
			       Boolean def_value);

CFPropertyListRef
my_CFPropertyListCreateWithBytePtrAndLength(const void * data, int data_len);

#endif _S_MYCFUTIL_H
