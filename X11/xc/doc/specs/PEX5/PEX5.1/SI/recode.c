/* $Xorg: recode.c,v 1.3 2000/08/17 19:42:14 cpqbld Exp $ */

/***********************************************************

Copyright (c) 1990, 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

Copyright (c) 1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Sun Microsystems,
and the X Consortium, not be used in advertising or publicity 
pertaining to distribution of the software without specific, written 
prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/



/*
 * recode.escapes --
 * changes troff's funny control codes back into \x sequences,
 * where the control codes and their translations are as defined
 * in the switch statement below.
 *	Written by Henry McGilton.  April 1984
 */
#include	<stdio.h>

main ()
{
	int	c;

	while ((c = getchar()) != EOF)
		switch (c) {
		case '\000':
			putchar('\\');
			putchar('0');
			break;
		case '\001':
			putchar('\\');
			putchar('a');
			break;
		case '\020':
			putchar('\\');
			putchar('{');
			break;
		case '\021':
			putchar('\\');
			putchar('}');
			break;
		case '\024':
			putchar('\\');
			putchar('%');
			break;
		case '\025':
			putchar('\\');
			putchar('c');
			break;
		case '\026':
			putchar('\\');
			putchar('e');
			break;
		case '\027':
			putchar('\\');
			putchar(' ');
			break;
		case '\030':
			putchar('\\');
			putchar('!');
			break;
		case '\022':
			putchar('\\');
			putchar('&');
			break;
		case ('\327' & 0377):
			putchar('\\');
			putchar('^');
			break;
		case ('\332' & 0377):
			putchar('\\');
			putchar('|');
			break;
		case ('\326' & 0377):
			putchar('\\');
			putchar('-');
			break;
		case ('\334' & 0377):
			putchar('\\');
			putchar('\'');
			break;
		case ('\003'):
			putchar('\\');
			putchar('`');
			break;
		default:
			putchar(c);
			break;
		}
	return(0);
}
