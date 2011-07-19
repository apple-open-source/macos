/*
 * Copyright (c) 2001 - 2010 Apple Inc. All rights reserved.
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
#if !defined(__CHARSETS_H__)
#define __CHARSETS_H__ 1

#include <CoreFoundation/CoreFoundation.h>

void str_upper(char *dst, size_t maxDstLen, CFStringRef srcRef);
extern char *convert_wincs_to_utf8(const char *windows_string, CFStringEncoding codePage);
extern char *convert_utf8_to_wincs(const char *utf8_string, CFStringEncoding codePage, int uppercase);
extern char *convert_leunicode_to_utf8(unsigned short *windows_string, size_t maxLen);
extern char *convert_unicode_to_utf8(const uint16_t *unicode_string, size_t maxLen);
extern unsigned short *convert_utf8_to_leunicode(const char *utf8_string);
#endif /* !__CHARSETS_H__ */
