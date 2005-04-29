/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#include <stdio.h>
#include <aghdr.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static char *Message[] = {
	"",							/* 0 is not assigned   */
	"%s",						/* AGERROR_SYNTAX == 1 */
	"out of memory",			/* AGERROR_MEMORY == 2 */
	"unimplemented feature: %s",/* AGERROR_UNIMPL == 3 */
	"move_to_front lock %s",	/* AGERROR_MTFLOCK== 4 */
	"compound graph error %s",	/* AGERROR_CMPND  == 5 */
	"bad object pointer %s",	/* AGERROR_BADOBJ == 6 */
	"object ID overflow",		/* AGERROR_IDOVFL == 7 */
	"flat lock violation",		/* AGERROR_MTFLOCK== 8 */
	"object and graph disagree" /* AGERROR_WRONGGRAPH==9 */
};

/* default error handler */
void agerror(int code, char *str)
{
	/* fprintf(stderr,"libgraph runtime error: "); */
	fprintf(stderr,Message[code],str);
	fprintf(stderr,"\n");

	if (code != AGERROR_SYNTAX) exit(1);
}
