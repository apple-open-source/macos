/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
print_time(struct timespec *tv)
{

	printf("%ld", tv->tv_sec);
	if (tv->tv_nsec != 0)
		printf(".%ld", tv->tv_nsec);
	printf("\n");
}

int
main(int argc, char *argv[])
{
	struct stat sb;
	const char *file;

	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", getprogname());
		return (1);
	}

	file = argv[1];
	if (stat(file, &sb) != 0)
		err(1, "stat");

	/* atime, mtime */
	print_time(&sb.st_atimespec);
	print_time(&sb.st_mtimespec);
}
