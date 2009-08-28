/*-
 * Copyright (c) 2008 Apple, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 *
 */
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "amt.h"

#define MAX_PERCENT	10
#define MIN_PERCENT	 5 

static uint64_t
get_memsize(void)
{
	uint64_t physmem;
	size_t len = sizeof(physmem);

	if (sysctlbyname("hw.memsize", &physmem, &len, NULL, 0))
		return (0);

	return (physmem);
}

/*
 * Test memory by allocating MAX_PERCENT% (MIN_PERCENT% min) and writing
 * random values then reading them back.  Returns:
 *
 *	-2  Failure in setting up test.
 *	 0  Test not run because the rlimit was set too low.
 *	 1  Test Passed.
 */
int
amt_memory(int verbose)
{
	uint64_t memsize, allocsize;
	u_int *memptr;
	size_t i, maxidx;
	struct rlimit rl;
	u_int seed;

	/* 
	 * Get physical memory size.
	 */
	if (verbose)
		printf("\tDetermining physical memory size: ");
	memsize = get_memsize();
	if (!memsize) {
		if (verbose)
			printf("Fail.\n");
		return (-2);
	}	
	if (verbose)
		printf("Pass.\n");
	
	/*
	 * Try to allocate up to MAX_PERCENT% of the physical memory.  It may
	 * allocate less if restricted by resource limits.  Must be able to
	 * allocate at least MIN_PERCENT% of the physical memory to run this
	 * test.
	 */
	if (getrlimit(RLIMIT_DATA, &rl) == -1) {
		printf("Fail. (getrlimit: %s)\n", strerror(errno));
		return (-2);
	}
	rl.rlim_cur = rl.rlim_max;
	if (setrlimit(RLIMIT_DATA, (const struct rlimit *)&rl) == -1) {
		printf("Fail. (setrlimit: %s)\n", strerror(errno));
		return (-2);
	}
	allocsize = MIN((memsize / (100/MAX_PERCENT)), rl.rlim_max);
	if (allocsize < (memsize / (100/MIN_PERCENT))) {
		if (verbose)
			printf("\tLimited by rlimit. "
			    "Not running memory test.\n");
		return (0);
	}
	if (verbose)
		printf("\tAllocating %2.1f%% of physical memory (%llu MB): ", 
		    ((float)allocsize/(float)memsize) * 100.0, 
		    allocsize / (1024 * 1024));
	memptr = malloc(allocsize);
	if (memptr == NULL) {
		if (verbose)
			printf("Fail.\n");
		return (-2);
	}
	if (verbose)
		printf("Pass.\n");

	
	if (verbose)
		printf("\tWriting random values to memory: ");
	maxidx = allocsize / sizeof(memptr);
	seed = (u_int) time(NULL);
	srandom(seed);
	for (i=0; i < maxidx; i++)
		memptr[i] = random();
	if (verbose)
		printf("Pass.\n");

	if (verbose)
		printf("\tVerifying memory values: ");
	srandom(seed);
	for (i=0; i < maxidx; i++)
		if (memptr[i] != random()) {
			if (verbose)
				printf("Fail.\n");
			return (-1);
		}
	if (verbose)
		printf("Pass.\n");
	
	return (1);
}
