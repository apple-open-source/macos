/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
	int fd;

	if (argc != 3 || argv[1][0] == '\0' || argv[2][0] == '\0') {
		errx(1, "usage: %s swap1 swap2", argv[0]);
	}

	fprintf(stderr, "Swapping %s and %s\n", argv[1], argv[2]);

	fd = open("flag", O_CREAT | O_RDONLY, 0644);
	if (fd < 0) {
		err(1, "flag");
	}

	for (;;) {
		renamex_np(argv[1], argv[2], RENAME_SWAP);
	}

	return 0;
}
