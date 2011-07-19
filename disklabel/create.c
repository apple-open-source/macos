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

static uint64_t
ParseSize(char *str) {
	uint64_t size, retval;
	char *endptr;
	uint64_t mult = 1;

	fprintf(stderr, "ParseSize(%s)\n", str);

	errno = 0;
	size = strtoq(str, &endptr, 10);
	if (errno != 0) {
		warn("cannot parse `%s': ", str);
		return -1;
	}

	if (endptr && *endptr != 0) {
		if (strlen(endptr) != 1) {
			warnx("cannot parse `%s': unknown suffix `%s'",
				str, endptr);
			return -1;
		}
		switch (*endptr) {
			case 'b': case 'B':
				mult = 512; break;
			case 'k': case 'K':
				mult = 1024; break;
			case 'm': case 'M':
				mult = 1024 * 1024; break;
			case 'g': case 'G':
				mult = 1024 * 1024 * 1024; break;
			default:
				warnx("cannot parse `%s': unknown suffix `%s'",
					str, endptr);
				return -1;
		}
	}
	retval = size * mult;
	return retval;
}

void
doCreate(const char *dev, char **args) {
	uint64_t size = 128 * 1024;	// Default size of metadata area
	CFMutableDictionaryRef md = nil;
	CFStringRef cfStr = nil;
	CFNumberRef cfNum = nil;
	int blocksize = GetBlockSize(dev);
	uint64_t disksize = GetDiskSize(dev);

	int i;

	if (blocksize == 0) {
		errx(4, "doCreate:  Cannot get valid blocksize for device %s", dev);
	}
	if (disksize == 0) {
		errx(5, "doCreate:  Cannot get valid disk size for device %s", dev);
	}


	md = CFDictionaryCreateMutable(nil, 0, nil, nil);
	if (md == nil) {
		warnx("cannot create dictionary in doCreate");
		goto out;
	}

	for (i = 0; args[i]; i++) {
		char *arg = args[i];
		CFTypeRef v;
		CFStringRef k;

		if (parseProperty(arg, &k, &v) > 0) {
			if (v == nil) {
				warnx("cannot parse `%s': tag must have a value", arg);
bad:
				CFRelease(k);
				continue;
			}
			if (CFStringCompare(k, CFSTR("-msize"), 0)
				== kCFCompareEqualTo) {
				if (CFGetTypeID(v) == CFStringGetTypeID()) {
					int len = CFStringGetLength(v);
					char buf[len*2];
					
					memset(buf, 0, sizeof(buf));
					if (CFStringGetCString(v, buf, sizeof(buf), kCFStringEncodingASCII) == FALSE) {
						warnx("cannot convert msize to string!");
						goto bad;
					}
					size = ParseSize(buf);
				} else if (CFGetTypeID(v) != CFNumberGetTypeID()) {
					warnx("-msize value must be a number");
					goto bad;
				} else {
					CFNumberGetValue(v, kCFNumberSInt64Type, &size);
				}
				CFRelease(k);
				CFRelease(v);
				continue;
			}
			CFDictionaryAddValue(md, k, v);
		}
	}

	if (size > 0) {
		disksize = disksize - size;
		cfNum = CFNumberCreate(NULL, kCFNumberSInt64Type, &size);
		if (cfNum == NULL) {
			errx(6, "doCreate:  cannot create base number");
		}
		CFDictionaryAddValue(md, CFSTR("Base"), cfNum);
		CFRelease(cfNum);
		cfNum = CFNumberCreate(NULL, kCFNumberSInt64Type, &disksize);
		if (cfNum == NULL) {
			errx(7, "doCreate:  cannot create size number");
		}
		CFDictionaryAddValue(md, CFSTR("Size"), cfNum);
		if (gDebug) {
			CFDataRef data;
			int len;
			char *cp;
			data = CFPropertyListCreateXMLData(nil, (CFPropertyListRef)md);
			len = CFDataGetLength(data);
			cp = (char*)CFDataGetBytePtr(data);
			fprintf(stderr, "Size = %qu bytes\n", size);
			write (2, cp, len);
			CFRelease(data);

		}
		// Now need to put the metadata in the partition
		if (InitialMetadata(dev, md, size) == -1) {
			errx(3, "Cannot write initial metadata to device %s", dev);
		}
	}
out:
	if (md)
		CFRelease(md);
	if (cfStr)
		CFRelease(cfStr);
	if (cfNum)
		CFRelease(cfNum);

	return;
}

