/* 
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *
 *	@(#)extern.h	8.1 (Berkeley) 5/31/93
 */

int	c_cchars __P((const void *, const void *));
int	c_modes __P((const void *, const void *));
int	csearch __P((char ***, struct info *));
void	checkredirect __P((void));
void	gprint __P((struct termios *, struct winsize *, int));
void	gread __P((struct termios *, char *));
int	ksearch __P((char ***, struct info *));
int	msearch __P((char ***, struct info *));
void	optlist __P((void));
void	print __P((struct termios *, struct winsize *, int, enum FMT));
void	usage __P((void));

extern struct cchar cchars1[], cchars2[];
