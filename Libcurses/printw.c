/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
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
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "curses.h"

/*
 * printw and friends.
 *
 * These routines make nonportable assumptions about varargs if __STDC__
 * is not in effect.
 */

static int __winwrite __P((void *, const char *, int));

/*
 * printw --
 *	Printf on the standard screen.
 */
int
#ifdef __STDC__
printw(const char *fmt, ...)
#else
printw(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = vwprintw(stdscr, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 * wprintw --
 *	Printf on the given window.
 */
int
#ifdef __STDC__
wprintw(WINDOW * win, const char *fmt, ...)
#else
wprintw(win, fmt, va_alist)
	WINDOW *win;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = vwprintw(win, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 * mvprintw, mvwprintw --
 *	Implement the mvprintw commands.  Due to the variable number of
 *	arguments, they cannot be macros.  Sigh....
 */
int
#ifdef __STDC__
mvprintw(register int y, register int x, const char *fmt, ...)
#else
mvprintw(y, x, fmt, va_alist)
	register int y, x;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (move(y, x) != OK)
		return (ERR);
	ret = vwprintw(stdscr, fmt, ap);
	va_end(ap);
	return (ret);
}

int
#ifdef __STDC__
mvwprintw(register WINDOW * win, register int y, register int x,
    const char *fmt, ...)
#else
mvwprintw(win, y, x, fmt, va_alist)
	register WINDOW *win;
	register int y, x;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (wmove(win, y, x) != OK)
		return (ERR);

	ret = vwprintw(win, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 * Internal write-buffer-to-window function.
 */
static int
__winwrite(cookie, buf, n)
	void *cookie;
	register const char *buf;
	int n;
{
	register WINDOW *win;
	register int c;

	for (c = n, win = cookie; --c >= 0;)
		if (waddch(win, *buf++) == ERR)
			return (-1);
	return (n);
}

/*
 * vwprintw --
 *	This routine actually executes the printf and adds it to the window.
 */
int
vwprintw(win, fmt, ap)
	WINDOW *win;
	const char *fmt;
	va_list ap;
{
	FILE *f;

	if ((f = funopen(win, NULL, __winwrite, NULL, NULL)) == NULL)
		return (ERR);
	(void)vfprintf(f, fmt, ap);
	return (fclose(f) ? ERR : OK);
}
