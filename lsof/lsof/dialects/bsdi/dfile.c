/*
 * dfile.c - BSDI file processing functions for lsof
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dfile.c,v 1.7 2001/08/14 12:36:32 abe Exp $";
#endif


#include "lsof.h"


/*
 * print_dev() - print device
 */

char *
print_dev(lf, dev)
	struct lfile *lf;		/* file whose device is to be printed */
	dev_t *dev;			/* device to be printed */
{
	static char buf[128];

	(void) snpf(buf, sizeof(buf), "%d,%d,%d", GET_MAJ_DEV(*dev),
	    dv_unit(*dev), dv_subunit(*dev));
	return(buf);
}
