/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific written prior permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 * RCS: @(#) tmpnam.c,v 1.5 2003/01/21 19:39:57 hunt Exp
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>

/*
 * Use /tmp instead of /usr/tmp, because L_tmpname is only 14 chars
 * on some machines (like NeXT machines) and /usr/tmp will cause
 * buffer overflows.
 */

#ifdef P_tmpdir
#   undef P_tmpdir
#endif
#define	P_tmpdir	"/tmp"

char *
tmpnam(s)
	char *s;
{
	static char name[50];
	char *mktemp();

	if (!s)
		s = name;
	(void)sprintf(s, "%s/XXXXXX", P_tmpdir);
	return(mktemp(s));
}
