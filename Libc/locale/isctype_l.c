/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)isctype.c	8.3 (Berkeley) 2/24/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/locale/isctype.c,v 1.9 2002/08/17 20:03:44 ache Exp $");

#include "xlocale_private.h"
#include <ctype.h>

#undef digittoint_l
int
digittoint_l(c, l)
	int c;
	locale_t l;
{
	return (__maskrune_l(c, 0xFF, l));
}

#undef isalnum_l
int
isalnum_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_A|_CTYPE_D, l));
}

#undef isalpha_l
int
isalpha_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_A, l));
}

#undef isblank_l
int
isblank_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_B, l));
}

#undef iscntrl_l
int
iscntrl_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_C, l));
}

#undef isdigit_l
int
isdigit_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_D, l));
}

#undef isgraph_l
int
isgraph_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_G, l));
}

#undef ishexnumber_l
int
ishexnumber_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_X, l));
}

#undef isideogram_l
int
isideogram_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_I, l));
}

#undef islower_l
int
islower_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_L, l));
}

#undef isnumber_l
int
isnumber_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_D, l));
}

#undef isphonogram_l
int
isphonogram_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_Q, l));
}

#undef isprint_l
int
isprint_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_R, l));
}

#undef ispunct_l
int
ispunct_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_P, l));
}

#undef isrune_l
int
isrune_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, 0xFFFFFF00L, l));
}

#undef isspace_l
int
isspace_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_S, l));
}

#undef isspecial_l
int
isspecial_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_T, l));
}

#undef isupper_l
int
isupper_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_U, l));
}

#undef isxdigit_l
int
isxdigit_l(c, l)
	int c;
	locale_t l;
{
	return (__istype_l(c, _CTYPE_X, l));
}

#undef tolower_l
int
tolower_l(c, l)
	int c;
	locale_t l;
{
        return (__tolower_l(c, l));
}

#undef toupper_l
int
toupper_l(c, l)
	int c;
	locale_t l;
{
        return (__toupper_l(c, l));
}

