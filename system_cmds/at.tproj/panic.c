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
 * panic.c - terminate fast in case of error
 * Copyright (c) 1993 by Thomas Koenig
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* System Headers */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Local headers */

#include "panic.h"
#include "at.h"

/* File scope variables */

static char rcsid[] = "$Id: panic.c,v 1.1.1.2 2000/01/11 02:10:05 wsanchez Exp $";

/* External variables */

/* Global functions */

#ifdef __APPLE__
__private_extern__
#endif
void
panic(a)
	char *a;
{
/* Something fatal has happened, print error message and exit.
 */
	fprintf(stderr, "%s: %s\n", namep, a);
	if (fcreated)
		unlink(atfile);

	exit(EXIT_FAILURE);
}

void
perr(a)
	char *a;
{
/* Some operating system error; print error message and exit.
 */
	perror(a);
	if (fcreated)
		unlink(atfile);

	exit(EXIT_FAILURE);
}

void 
perr2(a, b)
	char *a, *b;
{
	fprintf(stderr, "%s", a);
	perr(b);
}

void
usage(void)
{
/* Print usage and exit.
*/
	fprintf(stderr, "Usage: at [-q x] [-f file] [-m] time\n"
	    "       atq [-q x] [-v]\n"
	    "       atrm [-q x] job ...\n"
	    "       batch [-f file] [-m]\n");
	exit(EXIT_FAILURE);
}
