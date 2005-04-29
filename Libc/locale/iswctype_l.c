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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/locale/iswctype.c,v 1.6 2002/08/17 20:30:34 ache Exp $");

#include "xlocale_private.h"
#include <wctype.h>

#undef iswalnum_l
int
iswalnum_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_A|_CTYPE_D, l));
}

#undef iswalpha_l
int
iswalpha_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_A, l));
}

#undef iswblank_l
int
iswblank_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_B, l));
}

#undef iswcntrl_l
int
iswcntrl_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_C, l));
}

#undef iswdigit_l
int
iswdigit_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_D, l));
}

#undef iswgraph_l
int
iswgraph_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_G, l));
}

#undef iswhexnumber_l
int
iswhexnumber_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_X, l));
}

#undef iswideogram_l
int
iswideogram_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_I, l));
}

#undef iswlower_l
int
iswlower_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_L, l));
}

#undef iswnumber_l
int
iswnumber_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_D, l));
}

#undef iswphonogram_l
int
iswphonogram_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_Q, l));
}

#undef iswprint_l
int
iswprint_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_R, l));
}

#undef iswpunct_l
int
iswpunct_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_P, l));
}

#undef iswrune_l
int
iswrune_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, 0xFFFFFF00L, l));
}

#undef iswspace_l
int
iswspace_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_S, l));
}

#undef iswspecial_l
int
iswspecial_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_T, l));
}

#undef iswupper_l
int
iswupper_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_U, l));
}

#undef iswxdigit_l
int
iswxdigit_l(wc, l)
	wint_t wc;
	locale_t l;
{
	return (__istype_l(wc, _CTYPE_X, l));
}

#undef towlower_l
wint_t
towlower_l(wc, l)
	wint_t wc;
	locale_t l;
{
        return (__tolower_l(wc, l));
}

#undef towupper_l
wint_t
towupper_l(wc, l)
	wint_t wc;
	locale_t l;
{
        return (__toupper_l(wc, l));
}

