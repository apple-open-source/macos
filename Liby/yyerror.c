/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)yyerror.c	8.1 (Berkeley) 6/4/93
 */


#include <stdio.h>

int yyerror(msg)
char *msg;
{
	(void)fprintf(stderr, "%s\n", msg);
	return(0);
}
