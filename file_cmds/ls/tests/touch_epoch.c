 /*
 *
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of
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

#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
	struct timeval times[2];
	unsigned long long val;

	if (argc < 2)
		errx(1, "usage: %s file [file ...]", getprogname());

	/* Overflows localtime() - used for testing ls(1) resilience */
	val = 67768036191705600;

	times[0].tv_sec = (time_t)val;
	times[0].tv_usec = 0;
	times[1] = times[0];

	for (int i = 1; i < argc; i++) {
		if (utimes(argv[i], times) != 0)
			err(1, "utimes(%s)", argv[i]);
	}

	return (0);
}
