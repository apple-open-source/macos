/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/xedit/lisp/lsp.c,v 1.7 2002/11/23 08:26:49 paulo Exp $ */

#include <stdio.h>
#include <string.h>
#include "lisp.h"

#ifdef NEED_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
#ifdef NEED_REALPATH
#include <sys/param.h>
#if defined(ISC)
#ifndef MAXPATHLEN
#define MAXPATHLEN      1024
#endif
#endif
char *realpath(const char *pathname, char resolvedname[MAXPATHLEN]);
#endif

int
main(int argc, char *argv[])
{
    int i;

    LispBegin();

    i = 1;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
	LispDebug(1);
	++i;
    }

    if (i < argc) {
	char buffer[2048];

	for (; i < argc; i++) {
	    snprintf(buffer, sizeof(buffer),
		     "(load \"%s\" :if-does-not-exist :error)",
		     argv[i]);
	    LispExecute(buffer);
	}
    }
    else
	LispMachine();

    LispEnd();

    return (0);
}
