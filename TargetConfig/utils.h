/*
 * Copyright (c) 2005-2008 Apple Inc. All rights reserved.
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
// Kevin Van Vechten <kvv@apple.com>

#include <CoreFoundation/CoreFoundation.h>

#define stack_cfstrdup(_str) ({ \
        char* _result = NULL; \
        if (_str != NULL) { \
                CFIndex _length = CFStringGetLength(_str); \
                CFIndex _size = CFStringGetMaximumSizeForEncoding(_length, kCFStringEncodingUTF8); \
                _result = alloca(_size+1); \
                if (_result != NULL) { \
                        _length = CFStringGetBytes(_str, CFRangeMake(0, _length), kCFStringEncodingUTF8, '?', 0, (UInt8*)_result, _size, NULL); \
                        _result[_length] = 0; \
                } \
        } \
        _result; \
})

#define stack_pathconcat(x, y) ({ \
	int _lenx = strlen(x); \
	int _leny = strlen(y); \
	char *_str = (char *)alloca(_lenx + _leny + 2); \
	if (_str != NULL) { \
		strcpy(_str, (x)); \
		if ((x)[_lenx - 1] != '/') strcat(_str, "/"); \
		strcat(_str, (y)); \
	} \
	_str; \
})

#define stack_strdup(x) ({ \
	int _len = strlen(x); \
	char *_str = alloca(_len + 1); \
	if (_str != NULL) strcpy(_str, (x)); \
	_str; \
})

char* cfstrdup(CFStringRef str);
CFStringRef cfstr(const char* str);
CFPropertyListRef read_plist(const char* path, const char **errstr);
int write_plist(const char *path, CFPropertyListRef plist);
int write_plist_fd(int fd, CFPropertyListRef plist);
int cfprintf(FILE* file, const char* format, ...);
CFArrayRef dictionaryGetSortedKeys(CFDictionaryRef dictionary);
void dictionaryApplyFunctionSorted(CFDictionaryRef dict, CFDictionaryApplierFunction applier, void* context);
#ifdef notdef
int writePlist(FILE* f, CFPropertyListRef p, int tabs);
#endif /* notdef */
CFArrayRef tokenizeString(CFStringRef str);
void arrayAppendArrayDistinct(CFMutableArrayRef array, CFArrayRef other);
int is_file(const char* path);
void upper_ident(char *str);
const char *lockfilebylink(const char *lockfile);
void unlockfilebylink(const char *linkfile);
