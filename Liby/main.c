/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)main.c	8.1 (Berkeley) 6/4/93
 */

extern int yyparse(void);

void main()
{
	exit(yyparse());
}
