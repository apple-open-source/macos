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
#include <fcntl.h>
#include <err.h>
#include <unistd.h>

#include "util.h"

void
doDestroy(const char *dev) {
	int bs;
	if (gVerbose) {
		fprintf(stderr, "Destroying device %s", dev);
	}

	if (!IsAppleLabel(dev)) {
		errx(4,"doDestroy:  device %s is not an Apple Label device", dev);
	}

	bs = GetBlockSize(dev);
	if (bs != 0) {
		int fd;
		char bz[bs];
		memset(bz, 0, bs);

		fd = open(dev, O_WRONLY);
		if (fd == -1) {
			err(1, "doDestroy:  cannot open device %s for writing", dev);
		}
		if (write(fd, bz, bs) != bs) {
			err(2, "doDestroy:  cannot write %d bytes onto device %s", bs, dev);
		}
		close(fd);
	} else {
		errx(3, "doDestroy:  cannot get blocksize for device %s", dev);
	}
}
