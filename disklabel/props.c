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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "util.h"

void
PrintValue(const void *key, const void *value, void *context) {
	int key_len, value_len;
	const CFStringRef kStr = key;
	const CFTypeRef kVal = value;
	char *buf = NULL;
	int32_t val;
	char *intFmt = "%d\n";

	if (CFGetTypeID(kStr) != CFStringGetTypeID()) {
		warnx("PrintDictionary:  key type is not a string");
		return;
	}

	if (CFStringCompare(kStr, CFSTR("owner-mode"), 0) == kCFCompareEqualTo) {
		intFmt = "0%o\n";
	} else if (!gVerbose && (CFStringCompare(kStr, CFSTR("Base"), 0) == kCFCompareEqualTo ||
		CFStringCompare(kStr, CFSTR("Size"), 0) == kCFCompareEqualTo)) {
		return;
	}

	key_len = CFStringGetLength(kStr);
	buf = malloc(key_len * 2);
	if (!buf) {
		warnx("PrintDictionary:  unable to allocate buffer for key");
		goto out;
	}
	if (!CFStringGetCString(kStr, buf, key_len * 2, kCFStringEncodingASCII)) {
		warnx("PrintDictionary:  unable to get key as C string");
		goto out;
	}
	printf("\t%s = ", buf);
	free(buf); buf = NULL;

	if (CFGetTypeID(kVal) == CFStringGetTypeID()) {
		value_len = CFStringGetLength(kVal);
		buf = malloc(value_len * 2);
		if (buf == NULL) {
			warnx("PrintDictionary:  unable to allocate buffer for value");
			printf("* * * ERROR * * *\n");
			goto out;
		}
		if (!CFStringGetCString(kVal, buf, value_len * 2, kCFStringEncodingASCII)) {
			warnx("PrintDictionary: unable to get value as C String");
			printf("* * * ERROR * * *\n");
			goto out;
		}
		printf("\"%s\"\n", buf);
		free(buf); buf = NULL;
	} else if (CFGetTypeID(kVal) == CFNumberGetTypeID()) {
		if (!CFNumberGetValue(kVal, kCFNumberSInt32Type, &val)) {
			warnx("PrintDictionary: unable to get value as 32-bit number");
			printf("* * * ERROR * * *\n");
			goto out;
		}
		printf(intFmt, val);
	}

out:
	if (buf)
		free(buf);
	return;
}

void
doProps(const char *dev, char **args) {
	CFMutableDictionaryRef md = nil;
	CFDictionaryRef old;
	CFStringRef cfStr = nil;
	CFNumberRef cfNum = nil;
	int changes = 0;

	int i;

	old = ReadMetadata(dev);
	if (old == nil) {
		warnx("doProps:  cannot get metadata for device %s", dev);
		goto out;
	}

	md = CFDictionaryCreateMutableCopy(nil, 0, old);

	if (md == nil) {
		warnx("cannot create dictionary in doProps");
		goto out;
	}

	if (args[0] == NULL) {
		CFDictionaryApplyFunction(old, &PrintValue, NULL);
		goto out;
	}

	for (i = 0; args[i]; i++) {
		char *arg = args[i];
		CFTypeRef v;
		CFStringRef k;

		if (parseProperty(arg, &k, &v) > 0) {
			if (v == nil) {
				/* No value, so just print it out */
				v = CFDictionaryGetValue(old, k);
				if (v == nil) {
					warnx("Property `%s' does not exist in metadata", arg);
				} else {
					PrintValue(k, v, NULL);
				}
				CFRelease(k);
				continue;
			}
			// We have a key and a value, so we set them
			CFDictionarySetValue(md, k, v);
			printf("\tProperty %s\n", arg);
			CFRelease(k);
			CFRelease(v);
			changes++;
		}
	}

	if (gDebug) {
		CFDataRef data;
		int len;
		char *cp;
		data = CFPropertyListCreateXMLData(nil, (CFPropertyListRef)md);
		len = CFDataGetLength(data);
		cp = (char*)CFDataGetBytePtr(data);
		write (2, cp, len);
		CFRelease(data);

	}
	if (changes) {
		if (WriteMetadata(dev, md) != 1) {
			errx(3, "doProps:  cannot write metadata");
		}
	}
out:
	if (old)
		CFRelease(old);
	if (md)
		CFRelease(md);
	if (cfStr)
		CFRelease(cfStr);
	if (cfNum)
		CFRelease(cfNum);

	return;
}

