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

#include <bsm/audit.h>
#include <bsm/audit_session.h>

#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	auditinfo_addr_t auinfo = {
		.ai_termid = { .at_type = AU_IPv4 },
		.ai_asid = AU_ASSIGN_ASID,
		.ai_auid = 1 /* daemon */,
		.ai_flags = 0,
	};

	if (getuid() != 0) {
		fprintf(stderr, "must be run as root\n");
		return (1);
	}

	if (argc < 2) {
		fprintf(stderr, "usage: %s [command ...]\n", getprogname());
		return (1);
	}

	/* Skip our argv[0], invoke argv[1]+ */
	argc -= 1;
	argv += 1;

	if (setaudit_addr(&auinfo, sizeof(auinfo)) != 0)
		err(1, "setaudit_addr");

	execvp(argv[0], argv);
	err(1, "execv");
}
