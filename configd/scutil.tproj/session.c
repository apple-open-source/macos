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

void
do_open(int argc, char **argv)
{
	SCDStatus	status;

	if (session != NULL) {
		status = SCDClose(&session);
		switch (status) {
			case SCD_OK :
			case SCD_NOSESSION :
				/*
				 * if the "close" was successful or if we had an open
				 * session but can no talk to the server
				 */
				break;
			default :
				SCDLog(LOG_INFO, CFSTR("SCDClose: %s"), SCDError(status));
				return;
		}
	}

	status = SCDOpen(&session, CFSTR("sc"));
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDOpen: %s"), SCDError(status));
		return;
	}

	return;
}


void
do_close(int argc, char **argv)
{
	SCDStatus	status;

	status = SCDClose(&session);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDClose: %s"), SCDError(status));
	}
	return;
}


void
do_lock(int argc, char **argv)
{
	SCDStatus	status;

	status = SCDLock(session);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDLock: %s"), SCDError(status));
	}
	return;
}


void
do_unlock(int argc, char **argv)
{
	SCDStatus	status;

	status = SCDUnlock(session);
	if (status != SCD_OK) {
		SCDLog(LOG_INFO, CFSTR("SCDUnlock: %s"), SCDError(status));
	}
	return;
}
