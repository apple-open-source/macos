/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/re/re.h,v 1.1 2002/09/08 02:29:50 paulo Exp $ */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef _re_h
#define _re_h

/*
 * Defines
 */

	/* Compile flags options */
#define REG_BASIC		0000	/* Not used */
#define REG_EXTENDED		0001	/* Not used, only extended supported */

#define RE_ICASE		0002
#define RE_NOSUB		0004
#define RE_NEWLINE		0010
#define RE_NOSPEC		0020
#define RE_PEND			0040
#define RE_DUMP			0200



	/* Execute flag options */
#define RE_NOTBOL		1
#define RE_NOTEOL		2
#define RE_STARTEND		4
#define RE_TRACE		00400	/* Not used/supported */
#define RE_LARGE		01000	/* Not used/supported */
#define RE_BACKR		02000	/* Not used/supported */

	/* Value returned by reexec when match fails */
#define RE_NOMATCH		1
	/* Compile error values */
#define RE_BADPAT		2
#define RE_ECOLLATE		3
#define RE_ECTYPE		4
#define RE_EESCAPE		5
#define RE_ESUBREG		6
#define RE_EBRACK		7
#define RE_EPAREN		8
#define RE_EBRACE		9
#define RE_EBADBR		10
#define RE_ERANGE		11
#define RE_ESPACE		12
#define RE_BADRPT		13
#define RE_EMPTY		14
#define RE_ASSERT		15
#define RE_INVARG		16
#define RE_ATOI			255	/* Not used/supported */
#define RE_ITOA			0400	/* Not used/supported */


/*
 * Types
 */

/* (re)gular expression (mat)ch result */
typedef struct _re_mat {
    long rm_so;
    long rm_eo;
} re_mat;

/* (re)gular expression (cod)e */
typedef struct _re_cod {
    unsigned char *cod;
    int re_nsub;		/* Public member */
    const char *re_endp;	/* Support for RE_PEND */
} re_cod;


/*
 * Prototypes
 */
	/* compile the given pattern string
	 * returns 0 on success, error code otherwise */
int recomp(re_cod *preg, const char *pattern, int flags);

	/* execute the compiled pattern on the string.
	 * returns 0 if matched, RE_NOMATCH if failed, error code otherwise */
int reexec(const re_cod *preg, const char *string,
	   int nmat, re_mat pmat[], int flags);

	/* formats an error message for the given code in ebuffer */
int reerror(int ecode, const re_cod *preg, char *ebuffer, int ebuffer_size);

	/* frees the given parameter */
void refree(re_cod *preg);


#endif /* _re_h */
