/*
 * ptti.c -- BSD style print_tcptpi() function for lsof library
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
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


#include "../machine.h"

#if	defined(USE_LIB_PRINT_TCPTPI)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: ptti.c,v 1.1 97/09/23 04:07:36 abe Exp $";
# endif	/* !defined(lint) */

#define	TCPSTATES			/* activate tcpstates[] */
#include "../lsof.h"


/*
 * print_tcptpi() - print TCP/TPI info
 */

void
print_tcptpi(nl)
	int nl;				/* 1 == '\n' required */
{
	int ps = 0;
	int s;

	if ((Ftcptpi & TCPTPI_STATE) && Lf->lts.type == 0) {
	    if (Ffield)
		(void) printf("%cST=", LSOF_FID_TCPTPI);
	    else
		putchar('(');
	    if ((s = Lf->lts.state.i) < 0 || s >= TCP_NSTATES)
		(void) printf("UNKNOWN_TCP_STATE_%d", s);
	    else
		(void) fputs(tcpstates[s], stdout);
	    ps++;
	    if (Ffield)
		putchar(Terminator);
	}

#if	defined(HASTCPTPIQ)
	if (Ftcptpi & TCPTPI_QUEUES) {
	    if (Lf->lts.rqs) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("QR=%lu", Lf->lts.rq);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	    if (Lf->lts.sqs) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("QS=%lu", Lf->lts.sq);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	}
#endif	/* defined(HASTCPTPIQ) */

#if	defined(HASTCPTPIW)
	if (Ftcptpi & TCPTPI_WINDOWS) {
	    if (Lf->lts.rws) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("WR=%lu", Lf->lts.rw);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	    if (Lf->lts.wws) {
		if (Ffield)
		    putchar(LSOF_FID_TCPTPI);
		else {
		    if (ps)
			putchar(' ');
		    else
			putchar('(');
		}
		(void) printf("WW=%lu", Lf->lts.ww);
		if (Ffield)
		    putchar(Terminator);
		ps++;
	    }
	}
#endif	/* defined(HASTCPTPIW) */

	if (ps && !Ffield)
	    putchar(')');
	if (nl)
	    putchar('\n');
}
#else	/* !defined(USE_LIB_PRINT_TCPTPI) */
static char d1[] = "d"; static char *d2 = d1;
#endif	/* defined(USE_LIB_PRINT_TCPTPI) */
