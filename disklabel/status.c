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
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <CoreFoundation/CoreFoundation.h>

#include "util.h"

static char *
findRealDevice(char *dev) {
	struct dirent *dp;
	DIR *dir;
	struct stat sbuf;
	ino_t mine;
	char *dName;
	char *retval = NULL;
	
	if (stat(dev, &sbuf) == -1) {
		warn("cannot stat device file %s", dev);
		return NULL;
	}
	if (sbuf.st_nlink == 1) {   // assume this is the real device
		return NULL;
	}
#define BEGINS(x, y) (!strncmp((x), (y), strlen((y))))
	if (BEGINS(dev, "/dev/rdisk") || BEGINS(dev, "rdisk") ||
		BEGINS(dev, "/dev/disk") || BEGINS(dev, "disk")) {
		return NULL;
	}
	
	if (BEGINS(dev, "/dev/")) {
		dName = dev + 4;
	} else {
		dName = dev;
	}

#undef BEGINS
	mine = sbuf.st_ino;
	
	dir = opendir("/dev");
	while ((dp = readdir(dir))) {
		char *tmp = dp->d_name;
		char dbuf[6 + strlen(tmp)];
		memcpy(dbuf, "/dev/", 6);
		memcpy(dbuf + 5, tmp, sizeof(dbuf) - 5);
		if (!strcmp(dbuf, dName))
			continue;
		tmp = strrchr(dbuf, 's');
		if (!tmp)
			continue;
		if (dp->d_fileno == mine) {
			retval = strndup(dbuf, tmp - dbuf);
			break;
		}
	}
	closedir(dir);
	return retval;
}

void
doStatus(const char *dev) {
	char *realDev = findRealDevice((char*)dev);
	printf("Device %s\n", dev);
	if (realDev) {
		printf("\tReal device is %s\n", realDev);
	} else {
		realDev = (char*)dev;
	}
	dev = realDev;
	// XXX -- need to ensure this is an AppleLabel partition
	if (IsAppleLabel(dev) != 1) {
		printf("\t* * * NOT A VALID LABEL * * *\n");
		return;
	}
	printf("Metadata size = %u\n", GetMetadataSize(dev));
	if (VerifyChecksum(dev) == 0)
		printf("Metadata checksum is good\n");
	else
		printf("\t* * * Checksum is bad * * *\n");

	return;
}
