/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <err.h>
#include <sys/errno.h>
#include <stdbool.h>
#include <sysexits.h>
#include <mach/mach.h>
#include <mach/task_policy.h>

#define QOS_PARAMETER_LATENCY 0
#define QOS_PARAMETER_THROUGHPUT 1

static void usage(void);
static int parse_disk_policy(const char *strpolicy);
static int parse_qos_tier(const char *strpolicy, int parameter);

int main(int argc, char * argv[])
{
	int ch, ret;
	bool flagx = false, flagX = false, flagb = false;
	int flagd = -1, flagg = -1;
	struct task_qos_policy qosinfo = { LATENCY_QOS_TIER_UNSPECIFIED, THROUGHPUT_QOS_TIER_UNSPECIFIED };
	
	while ((ch = getopt(argc, argv, "xXbd:g:t:l:")) != -1) {
		switch (ch) {
			case 'x':
				flagx = true;
				break;
			case 'X':
				flagX = true;
				break;
			case 'b':
				flagb = true;
				break;
			case 'd':
				flagd = parse_disk_policy(optarg);
				if (flagd == -1) {
					warnx("Could not parse '%s' as a disk policy", optarg);
					usage();
				}
				break;
			case 'g':
				flagg = parse_disk_policy(optarg);
				if (flagg == -1) {
					warnx("Could not parse '%s' as a disk policy", optarg);
					usage();
				}
				break;
			case 't':
				qosinfo.task_throughput_qos_tier = parse_qos_tier(optarg, QOS_PARAMETER_THROUGHPUT);
				if (qosinfo.task_throughput_qos_tier == -1) {
					warnx("Could not parse '%s' as a qos tier", optarg);
					usage();
				}
				break;
			case 'l':
				qosinfo.task_latency_qos_tier = parse_qos_tier(optarg, QOS_PARAMETER_LATENCY);
				if (qosinfo.task_latency_qos_tier == -1) {
					warnx("Could not parse '%s' as a qos tier", optarg);
					usage();
				}
				break;
			case '?':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
	}
	
	if (flagx && flagX){
		warnx("Incompatible options -x, -X");
		usage();
	}

	if (flagx) {
		ret = setiopolicy_np(IOPOL_TYPE_VFS_HFS_CASE_SENSITIVITY, IOPOL_SCOPE_PROCESS, IOPOL_VFS_HFS_CASE_SENSITIVITY_FORCE_CASE_SENSITIVE);
		if (ret == -1) {
			err(EX_SOFTWARE, "setiopolicy_np(IOPOL_TYPE_VFS_HFS_CASE_SENSITIVITY...)");
		}
	}

	if (flagX) {
		ret = setiopolicy_np(IOPOL_TYPE_VFS_HFS_CASE_SENSITIVITY, IOPOL_SCOPE_PROCESS, IOPOL_VFS_HFS_CASE_SENSITIVITY_DEFAULT);
		if (ret == -1) {
			err(EX_SOFTWARE, "setiopolicy_np(IOPOL_TYPE_VFS_HFS_CASE_SENSITIVITY...)");
		}
	}

	if (flagb) {
		ret = setpriority(PRIO_DARWIN_PROCESS, 0, PRIO_DARWIN_BG);
		if (ret == -1) {
			err(EX_SOFTWARE, "setpriority()");
		}
	}

	if (flagd >= 0) {
		ret = setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, flagd);
		if (ret == -1) {
			err(EX_SOFTWARE, "setiopolicy_np(...IOPOL_SCOPE_PROCESS...)");
		}
	}

	if (flagg >= 0){
		ret = setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_DARWIN_BG, flagg);
		if (ret == -1) {
			err(EX_SOFTWARE, "setiopolicy_np(...IOPOL_SCOPE_DARWIN_BG...)");
		}
	}

	if (qosinfo.task_latency_qos_tier != LATENCY_QOS_TIER_UNSPECIFIED ||
	    qosinfo.task_throughput_qos_tier != THROUGHPUT_QOS_TIER_UNSPECIFIED){
		ret = task_policy_set(mach_task_self(), TASK_OVERRIDE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
		if (ret != KERN_SUCCESS){
			err(EX_SOFTWARE, "task_policy_set(...TASK_OVERRIDE_QOS_POLICY...)");
		}
	}
	
	ret = execvp(argv[0], argv);
	if (ret == -1) {
		err(EX_NOINPUT, "Could not execute %s", argv[0]);
	}
	
	return EX_OSERR;
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-x|-X] [-d <policy>] [-g policy] [-b] [-t <tier>] [-l <tier>] <program> [<pargs> [...]]\n", getprogname());
	exit(EX_USAGE);
}

static int parse_disk_policy(const char *strpolicy)
{
	long policy;
	char *endptr = NULL;
	
	/* first try as an integer */
	policy = strtol(strpolicy, &endptr, 0);
	if (endptr && (endptr[0] == '\0') && (strpolicy[0] != '\0')) {
		/* parsed complete string as a number */
		return (int)policy;
	}
	
	if (0 == strcasecmp(strpolicy, "DEFAULT") ) {
		return IOPOL_DEFAULT;
	} else if (0 == strcasecmp(strpolicy, "IMPORTANT")) {
		return IOPOL_IMPORTANT;
	} else if (0 == strcasecmp(strpolicy, "PASSIVE")) {
		return IOPOL_PASSIVE;
	} else if (0 == strcasecmp(strpolicy, "THROTTLE")) {
		return IOPOL_THROTTLE;
	} else if (0 == strcasecmp(strpolicy, "UTILITY")) {
		return IOPOL_UTILITY;
	} else if (0 == strcasecmp(strpolicy, "STANDARD")) {
		return IOPOL_STANDARD;
	} else {
		return -1;
	}
}

static int parse_qos_tier(const char *strtier, int parameter){
	long policy;
	char *endptr = NULL;
	
	/* first try as an integer */
	policy = strtol(strtier, &endptr, 0);
	if (endptr && (endptr[0] == '\0') && (strtier[0] != '\0')) {
		switch (policy) {
			case 0:
				return parameter ? THROUGHPUT_QOS_TIER_0 : LATENCY_QOS_TIER_0;
				break;
			case 1:
				return parameter ? THROUGHPUT_QOS_TIER_1 : LATENCY_QOS_TIER_1;
				break;
			case 2:
				return parameter ? THROUGHPUT_QOS_TIER_2 : LATENCY_QOS_TIER_2;
				break;
			case 3:
				return parameter ? THROUGHPUT_QOS_TIER_3 : LATENCY_QOS_TIER_3;
				break;
			case 4:
				return parameter ? THROUGHPUT_QOS_TIER_4 : LATENCY_QOS_TIER_4;
				break;
			case 5:
				return parameter ? THROUGHPUT_QOS_TIER_5 : LATENCY_QOS_TIER_5;
				break;
			default:
				return -1;
				break;
		}
	}

	return -1;
}
