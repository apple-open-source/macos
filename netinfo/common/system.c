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

/*
 * System routines
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <NetInfo/config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/file.h>
#include <syslog.h>
#include <stdarg.h>

#include <NetInfo/system.h>

#ifdef NeXT
#include <libc.h>
#endif

char *
sys_hostname(void)
{

	static char myhostname[MAXHOSTNAMELEN + 1] = "";

	if (myhostname[0] == 0) gethostname(myhostname, sizeof(myhostname));
	return (myhostname);
}
	
int
sys_spawn(const char *fname, ...)
{
	va_list ap;
	char *args[10]; /* XXX */
	int i;
	int pid;
	
	va_start(ap, (char *)fname);
	args[0] = (char *)fname;
	for (i = 1; (args[i] = va_arg(ap, char *)) != NULL; i++) {}
	va_end(ap);

	switch (pid = fork())
	{
		case -1: return -1;
		case 0:
			execv(args[0], args);
			_exit(-1);
		default: return pid;
	}
}
