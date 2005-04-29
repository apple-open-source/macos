/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * Copyright (c) 1988 Carnegie-Mellon University
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)prf.c	7.1 (Berkeley) 6/5/86
 */

#include <sys/param.h>

#define SPACE	1
#define ZERO	2
#define UCASE   16

/*
 * Scaled down version of C Library printf.
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are
 * suspended.
 *
 */

/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static void
printn(n, b, flag, minwidth, putfn_p, putfn_arg)
	u_long n;
	int b, flag, minwidth;
	void (*putfn_p)();
	void *putfn_arg;
{
	char prbuf[11];
	register char *cp;
	int width = 0, neg = 0;

	if (b == 10 && (int)n < 0) {
		neg = 1;
		n = (unsigned)(-(int)n);
	}
	cp = prbuf;
	do {
                *cp++ = "0123456789abcdef0123456789ABCDEF"[(flag & UCASE) + n%b];
		n /= b;
		width++;
	} while (n);
	
	if (neg) {
		(*putfn_p)('-', putfn_arg);
		width++;
	}
	while (width++ < minwidth)
		(*putfn_p)( (flag & ZERO) ? '0' : ' ', putfn_arg);
		
	do
		(*putfn_p)(*--cp, putfn_arg);
	while (cp > prbuf);
}

void prf(
	char *fmt,
	unsigned int *adx,
	void (*putfn_p)(),
	void *putfn_arg
)
{
	int b, c;
	char *s;
	int flag = 0, width = 0;
        int minwidth;

loop:
	while ((c = *fmt++) != '%') {
		if(c == '\0')
			return;
		(*putfn_p)(c, putfn_arg);
	}
        minwidth = 0;
again:
	c = *fmt++;
	switch (c) {
	case 'l':
		goto again;
	case ' ':
		flag |= SPACE;
		goto again;
	case '0':
		if (minwidth == 0) {
		    /* this is a flag */
		    flag |= ZERO;
		    goto again;
		} /* fall through */
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		minwidth *= 10;
		minwidth += c - '0';
		goto again;
        case 'X':
                flag |= UCASE;
                /* fall through */
	case 'x':
		b = 16;
		goto number;
	case 'd':
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		printn((u_long)*adx, b, flag, minwidth, putfn_p, putfn_arg);
		break;
	case 's':
		s = (char *)*adx;
		while (c = *s++) {
			(*putfn_p)(c, putfn_arg);
			width++;
		}
		while (width++ < minwidth)
		    (*putfn_p)(' ', putfn_arg);
		break;
	case 'c':
		(*putfn_p)((char)*adx, putfn_arg);
		break;
	}
	adx++;
	goto loop;
}
