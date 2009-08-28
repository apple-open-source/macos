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

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/wait.h>

/*
 * Handle the fault, if any, caused by fault_test() below.
 */
static void
fault_handler(int sig)
{

	/* Simply exit and return the type of fault. */
	_exit(sig);
}

/*
 * fault_test is a fixture for testing code that is expected to cause a fault.
 * The arguments are as follows:
 *  verbose:  	set to 1 if verbose output is desired, otherwise 0. 
 *  test_func:	the code (function ptr) that is expected to cause the fault.
 *  arg:	an argument, if any, to test_func().
 *  expected:	the expected fault type.
 */
int
fault_test(int verbose, void (*test_func)(void *), void *arg, u_char expected)
{
	pid_t pid, wpid;
	struct sigaction sig;
	struct sigaction o_sigsegv, o_sigill, o_sigiot, o_sigio, o_sigint;
	struct sigaction o_sigabrt, o_sigterm, o_sigquit, o_sigbus;
	int retval, stat;
	u_char fault;

	/*
	 * Set the fault handler and save the old signal handler state.
	 */
	sig.sa_handler = fault_handler;
	sig.sa_flags = 0;
	sigemptyset(&sig.sa_mask);
	sigaction(SIGSEGV, &sig, &o_sigsegv);
	sigaction(SIGILL, &sig, &o_sigill);
	sigaction(SIGIOT, &sig, &o_sigiot);
	sigaction(SIGIO, &sig, &o_sigio);
	sigaction(SIGINT, &sig, &o_sigint);
	sigaction(SIGABRT, &sig, &o_sigabrt);
	sigaction(SIGTERM, &sig, &o_sigterm);
	sigaction(SIGQUIT, &sig, &o_sigquit);
	sigaction(SIGBUS, &sig, &o_sigbus);

	/*
	 * Run the test code in another process.
	 */
	pid = fork();
	if (pid == 0) {
		/* child */

		(*test_func)(arg);
		_exit(-1);
	} else if (pid == -1) {
		/* fork() failed */
		if (verbose)
			printf("Fail. fork() failed.\n");
		retval = -2;
	} else {
		/* parent */
		wpid = wait(&stat);
		if (WIFEXITED(stat)) {
			fault = WEXITSTATUS(stat); 
			if (fault == expected) {
				if (verbose)
					printf("Pass.\n");
				retval = 1;
			} else if (fault == 255) {			
				if (verbose)
					printf("Fail. No fault.\n");
				retval = 0;
			} else {
				if (verbose)
					printf("Fail. Unexpected fault"
					   " (%d).\n", fault);
				retval = -1;
			}
		} else {
			if (verbose)
				printf("Fail. Test exited adnormally.\n");
			retval = -2; 
		}
	}

	/* 
	 * Restore the old signal handlers. 
	 */
	sigaction(SIGSEGV, &o_sigsegv, NULL);
	sigaction(SIGILL, &o_sigill, NULL);
	sigaction(SIGIOT, &o_sigiot, NULL);
	sigaction(SIGIO,  &o_sigio, NULL);
	sigaction(SIGINT, &o_sigint, NULL);
	sigaction(SIGABRT, &o_sigabrt, NULL);
	sigaction(SIGTERM, &o_sigterm, NULL);
	sigaction(SIGQUIT, &o_sigquit, NULL);
	sigaction(SIGBUS, &o_sigbus, NULL);

	return (retval);
}
