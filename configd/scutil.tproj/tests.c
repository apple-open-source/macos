/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "scutil.h"
#include "SCDPrivate.h"

void
do_snapshot(int argc, char **argv)
{
	SCDStatus	status;

	status = SCDSnapshot(session);
	if (status != SCD_OK) {
		printf("SCDSnapshot: %s\n", SCDError(status));
	}
	return;
}


#ifdef	DEBUG

void
test_openCloseLeak(int argc, char **argv)
{
	SCDStatus	status;
	int		i, loopCnt;
	SCDSessionRef	*sessions;

	if ((argc == 0) || (sscanf(argv[0], "%d", &loopCnt) != 1)) {
		loopCnt = 100;
	}

	sessions = malloc(loopCnt * sizeof(SCDSessionRef));

	/* open, close, open, close, open, close, ... */
	for (i=0; i<loopCnt; i++) {
		status = SCDOpen(&sessions[i], CFSTR("sc"));
		if (status != SCD_OK) {
			printf("SCDOpen: %s\n", SCDError(status));
			break;
		}

		status = SCDClose(&sessions[i]);
		if (status != SCD_OK) {
			printf("SCDClose: %s\n", SCDError(status));
			break;
		}
	}

	/* open, open, open, close, close, close, ... */
	for (i=0; i<loopCnt; i++) {
		status = SCDOpen(&sessions[i], CFSTR("sc"));
		if (status != SCD_OK) {
			printf("SCDOpen: %s\n", SCDError(status));
			break;
		}
	}
	for (i=0; i<loopCnt; i++) {
		status = SCDClose(&sessions[i]);
		if (status != SCD_OK) {
			printf("SCDClose: %s\n", SCDError(status));
			break;
		}
	}

	return;
}
#endif	/* DEBUG */
