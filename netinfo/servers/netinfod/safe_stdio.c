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
 * Safe stdio
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * Stdio does not lock its iob slots. We must do the locking
 * ourselves here instead because we use multiple threads which
 * stdio does not expect.
 */
#include <stdio.h>
#include <NetInfo/socket_lock.h>
#include "ni_globals.h"

#ifdef STDIO_DEBUG
#include <sys/stat.h>
#endif

/*
 * fopen() a file thread-safe way.
 */
FILE *
safe_fopen(char *fname, char *mode)
{
	FILE *f;

	socket_lock();
	f = fopen(fname, mode);

#ifdef STDIO_DEBUG
	/*
	 * If a problem occurs because descriptors are not being locked
	 * correctly, this code will often catch the problem (with enough
	 * beating up on the server).
	 */
	if (f != NULL)
	{
		struct stat st;
		extern void system_log(LOG_ERR,const char *);

		if (fstat(fileno(f), &st) < 0)
		{
			fclose(f);
			socket_unlock();
			system_log(LOG_ERR,"fopen returned bogus descriptor");
			return (NULL);
		}
	}
#endif

	socket_unlock();
	return (f);
}

/*
 * fclose() a file in a thread-safe way.
 */
int
safe_fclose(FILE *f)
{
	int res;

	socket_lock();
	res = fclose(f);
	socket_unlock();
	return (res);
}
