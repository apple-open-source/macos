/*
 * dnexus.c -- Darwin nexus processing functions for libproc-based lsof
 */

/*
 * Portions Copyright 2016 Apple Computer, Inc.  All rights reserved.
 *
 * Copyright 2005 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Allan Nathanson, Apple Computer, Inc., and Victor A.
 * Abell, Purdue University.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors, nor Apple Computer, Inc. nor Purdue University
 *    are responsible for any consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either
 *    by explicit claim or by omission.  Credit to the authors, Apple
 *    Computer, Inc. and Purdue University must appear in documentation
 *    and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


#ifndef lint
static char copyright[] =
"@(#) Copyright 2016 Apple Computer, Inc. and Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dnexus.c,v 1.7 2012/04/10 16:41:04 abe Exp $";
#endif


#include "lsof.h"


/*
 * process_nexus() -- process nexus file
 */

void
process_nexus(pid, fd)
	int pid;			/* PID */
	int32_t fd;			/* FD */
{
	(void) snpf(Lf->type, sizeof(Lf->type), "NEXUS");
	Lf->inp_ty = 2;
}
