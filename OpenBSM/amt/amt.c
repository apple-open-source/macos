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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include <bsm/audit.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>

#include "amt.h"

/*
 * The following is until these events get offically added to audit_uevents.h.
 */
#ifndef AUE_amt_succ
#define AUE_amt_succ	32801
#endif
#ifndef AUE_amt_fail
#define AUE_amt_fail	32802
#endif

/*
 *  The number of test that can be "NOT TESTED" and still pass.
 */
#define MAX_NOT_TESTED		1  

#define	PRIV_INST_MASK		0x00000001
#define MEM_SEP_MASK		0x00000002
#define	MEMORY_MASK		0x00000004
		
#define COMMANDLINE_FLAGS	"mpqs"

struct amt_test {
	int	(*at_fp)(int);
	char 	*at_desc;
	u_int	at_omask;
	char 	at_oflag;
};

static struct amt_test amt_tests[] = {
	{ amt_priv, "Privileged Instructions", PRIV_INST_MASK, 'p' },
	{ amt_memsep, "Memory Protection and Separation", MEM_SEP_MASK, 's' }, 
	{ amt_memory, "Memory Write and Read", MEMORY_MASK, 'm' },
	{ NULL, NULL, 0, '\0' }
};
	

static void
usage(void)
{

	fprintf(stderr, "Usage: amt [-%s]\n", COMMANDLINE_FLAGS);
	exit(-1);
}

static int
run_tests(int verbose, u_int test_mask)
{
	struct amt_test *atp;
	int result;
	int failed = 0;
	int nottested = 0;

	for (atp = &amt_tests[0]; atp->at_fp != NULL; atp++) {
		if (test_mask & atp->at_omask) {
			if (verbose)
				printf("%s test NOT TESTED\n\n", atp->at_desc);
			nottested++;
			continue;
		}
		if (verbose) 
			printf("Testing %s...\n", atp->at_desc);

		result = (*atp->at_fp)(verbose);
		if (result < 0) {
			failed++;
			if (verbose)
				printf("%s test FAILED.\n", atp->at_desc);
		} else if (result == 0) {
			nottested++;			
			if (verbose)
				printf("%s test NOT TESTED\n", atp->at_desc);
		} else if (result > 0) {
			if (verbose)
				printf("%s test PASSED.\n", atp->at_desc);
		}
		if (verbose)
			printf("\n");
	}	

	if (failed)
		return (failed);
	if (nottested > MAX_NOT_TESTED)
		return (nottested); 
	return (0);
}

int
main(int argc, char *argv[])
{
	struct auditinfo ai;
	struct amt_test *atp;
	token_t *tok;
	int aufd, result, logopts;
	int verbose = 1;
	u_int mask = 0;
	int ch, match;

	while ((ch = getopt(argc, argv, COMMANDLINE_FLAGS)) != -1) {
		match = 0;
		if (ch == 'q') {
			verbose = 0;
			continue;
		}
		for (atp = &amt_tests[0]; atp->at_fp != NULL; atp++) {
			if (ch == atp->at_oflag) {
				mask |= atp->at_omask;
				match = 1;
			}
		}
		if (match)
			continue;
		else
			usage();
	} 

	if (geteuid() != 0) {
		fprintf(stderr, "Please run as audit administrator.\n");
		exit(-1);
	}
	logopts = LOG_PID;

#ifdef LOG_SECURITY
	openlog("amt", logopts, LOG_SECURITY);
#else
	openlog("amt", logopts, LOG_AUTH);
#endif

	result = run_tests(verbose, mask);

	/*
	 * Generate an audit record to report the results of the tests.
	 */
	if ((aufd = au_open()) == -1)
		syslog(LOG_ERR, 
		    "Could not create AMT result event (result: %s)", 
		    (result) ? "PASSED" : "FAILED");
	else {
		bzero(&ai, sizeof(ai));
		if ((tok = au_to_subject32(getuid(), geteuid(), getegid(),
		    getuid(), getgid(), getpid(), getpid(), &ai.ai_termid)) 
		    != NULL)
			au_write(aufd, tok);
		if ((tok = au_to_text((result == 0) ? 
		    "amt::Abstract Machine Test: PASSED" :
		    "amt::Abstract Machine Test: FAILED")) != NULL)
			au_write(aufd, tok);
		if ((tok = au_to_return32(0, 0)) != NULL)
			au_write(aufd, tok);
		if (au_close(aufd, 1, (result == 0) ? AUE_amt_succ :
		    AUE_amt_fail) == -1)
			syslog(LOG_ERR,
		    	    "Could not create AMT result event (result: %s)", 
			    (result == 0) ? "PASSED" : "FAILED");
	}

	if (verbose) 
		printf("Abstract Machine Testing %s.\n", 
		    (result == 0) ? "PASSED" : "FAILED");

	syslog(LOG_INFO, "Abstract Machine Testing %s", 
	    (result == 0) ? "PASSED" : "FAILED");
		
	return (result);
}
