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
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mach/vm_param.h>  /* for PAGE_SIZE */

#include "amt.h"

#define USLEEP_TIME 50 

struct memsep_test {
	int	(*mt_fp)(int);
	char	*mt_desc;
};

static int text_mem(int verbose);
static int readonly_mem(int verbose);
static int stack_not_shared(int verbose);
static int malloc_not_shared(int verbose);

static struct memsep_test memsep_tests[] = {
	{ stack_not_shared, "Memory Separation (stack not shared)" },
	{ malloc_not_shared, "Memory Separation (heap not shared)" },
	{ text_mem, "Memory Protection (text segment memory)" },
	{ readonly_mem, "Memory Protection (read-only memory)" },
	{ NULL, NULL }	
};

static int
changemem_test(int verbose, int *vptr, int oldval, int newval)
{
	pid_t pid;
	int retval, stat;
	
	pid = fork();
	if (pid == 0) {
		/* child */
		
		/*
		 * Change variable pointed by vptr to new value.
		 */
		*vptr = newval;

		/*
		 * Sleep so parent can test value before exiting.
		 */
		(void) usleep(2 * USLEEP_TIME);
		_exit(0);
	} else if (pid == -1) {
		/* fork() failed */
		if (verbose)
			printf("Fail. fork() failed.\n");
		retval = -2;		
	} else {
		/* parent */
		
		/*
		 * Sleep so child can change value before testing.
		 */
		(void) usleep(USLEEP_TIME);
		if (*vptr == oldval) {
			/* success */
			if (verbose)
				printf("Pass.\n");
			retval = 1;
		} else {
			/* failure */
			if (verbose)
				printf("Fail.\n");
			retval = -1;
		}
	}
	(void) wait(&stat);
	return (retval);
}

static int
stack_not_shared(int verbose)
{
	int variable, rval;
	
	variable = 1234;
	rval = changemem_test(verbose, &variable, 1234, 5678);

	return (rval);
}

static int
malloc_not_shared(int verbose)
{
	int *iptr;
	int rval;
	
	iptr = malloc(sizeof(int));
	if (iptr == NULL) {
		
		return (-2);
	}
	*iptr = 1234;
	rval = changemem_test(verbose, iptr, 1234, 5678);
	free(iptr);

	return (rval);
}

static void
write_mem(void *arg)
{
	int *iptr = (int *)arg;

	*iptr = 5678;
	_exit(-1);
}

static int
text_mem(int verbose)
{
	int retval;


	retval = fault_test(verbose, write_mem, (void *)write_mem, SIGBUS);

	return (retval);
}


static int
readonly_mem(int verbose)
{
	int *iptr;
	int retval;
	
	/*
	 * Map a read-only page of memory.
	 */
	iptr = (int *) mmap((caddr_t)0, PAGE_SIZE, PROT_READ, 
	    MAP_ANON|MAP_PRIVATE, -1, 0);
	if (iptr == MAP_FAILED) {

		return (-2);
	}
	
	retval = fault_test(verbose, write_mem, (void *)iptr, SIGBUS);

	/*
	 * Unmap page.
	 */
	if (munmap(iptr, PAGE_SIZE) == -1) 
		retval = -2;

	return (retval);
}

int
amt_memsep(int verbose)
{
	struct memsep_test *mtp;
	int tcnt;

	for (tcnt = 0, mtp = &memsep_tests[0]; mtp->mt_fp != NULL; mtp++) {
		if (verbose)
			printf("\tSubtest %d. %s: ", tcnt + 1, mtp->mt_desc);
		if ((*mtp->mt_fp)(verbose) == 1)
			tcnt++;
		else
			/* This test failed.  No need to test more. */
			return (-1);
	}
	
	return (tcnt);
}
