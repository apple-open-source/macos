/*
 * Copyright (c) 2010-2012,2014 Apple Inc. All Rights Reserved.
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


#include "misc.h"
#include "SecCFRelease.h"

// NOTE: the return may or allocate a fair bit more space then it needs.
// Use it for short lived conversions (or strdup the result).
char *utf8(CFStringRef s) {
	CFIndex sz = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s), kCFStringEncodingUTF8) + 1;
	CFIndex used = 0;
	UInt8 *buf = (UInt8 *)malloc(sz);
	if (!buf) {
		return NULL;
	}
	CFStringGetBytes(s, CFRangeMake(0, CFStringGetLength(s)), kCFStringEncodingUTF8, '?', FALSE, buf, sz, &used);
	buf[used] = 0;
	
	return (char*)buf;
}

void CFfprintf(FILE *f, CFStringRef format, ...) {
	va_list ap;
	va_start(ap, format);
	
	CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, format, ap);
	va_end(ap);
	
	CFIndex sz = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
	sz += 1;
	CFIndex used = 0;
	unsigned char *buf;
	bool needs_free = false;
	if (sz < 1024) {
		buf = alloca(sz);
	} else {
		buf = malloc(sz);
		needs_free = true;
	}
	if (buf) {
		CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)), kCFStringEncodingUTF8, '?', FALSE, buf, sz, &used);
	} else {
		buf = (unsigned char *)"malloc failue during CFfprintf\n";
        needs_free = false;
	}
	
	fwrite(buf, 1, used, f);
	if (needs_free) {
		free(buf);
	}
	
	CFReleaseNull(str);
}

CFErrorRef fancy_error(CFStringRef domain, CFIndex code, CFStringRef description) {
	const void *v_ekey = kCFErrorDescriptionKey;
	const void *v_description = description;
	CFErrorRef err = CFErrorCreateWithUserInfoKeysAndValues(NULL, domain, code, &v_ekey, &v_description, 1);
    CFAutorelease(err);
	
	return err;
}

void CFSafeRelease(CFTypeRef object) {
	if (object) {
		CFReleaseNull(object);
	}
}

