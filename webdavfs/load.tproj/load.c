
/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*      @(#)load.c      *
 *      (c) 2000   Apple Computer, Inc.  All Rights Reserved
 *
 *
 *      load.c -- WebDAV setuid load command
 *
 *      MODIFICATION HISTORY:
 *              10-APR-2000     Clark Warner      File Creation
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

/*****************************************************************************/

/* Local Definitions */

#define LOAD_COMMAND "/sbin/kextload"
#define WEBDAV_MODULE_PATH "/System/Library/Extensions/webdav_fs.kext"

/*****************************************************************************/

int main(int argc, const char *argv[])
{
	int pid;
	int result = -1;
	union wait status;
	
	pid = fork();
	if (pid == 0)
	{
		result = execl(LOAD_COMMAND, LOAD_COMMAND, WEBDAV_MODULE_PATH, NULL);
		/* We can only get here if the exec failed */
		goto Return;
	}
	
	if (pid == -1)
	{
		result = errno;
		goto Return;
	}
	
	/* Success! */
	if ((wait4(pid, (int *) & status, 0, NULL) == pid) && (WIFEXITED(status)))
	{
		result = status.w_retcode;
	}
	else
	{
		result = -1;
	}
	
Return:
	return result;
}

/*****************************************************************************/
