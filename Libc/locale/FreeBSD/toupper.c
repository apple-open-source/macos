/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
__FBSDID("$FreeBSD: src/lib/libc/locale/toupper.c,v 1.13 2007/01/09 00:28:01 imp Exp $");
  
#include "xlocale_private.h"

#include <ctype.h>
#include <stdio.h>
#include <runetype.h>

__ct_rune_t
___toupper_l(__ct_rune_t c, locale_t loc)
{
	size_t lim;
	_RuneRange *rr;
	_RuneEntry *base, *re;

	if (c < 0 || c == EOF)
		return(c);

	NORMALIZE_LOCALE(loc);
	/*
	 * the following is not used by toupper(), but can be used by
	 * toupper_l().  This provides the oppurtunity to optimize toupper()
	 * when compatibility for Panther and lower is no longer needed
	 */
	if (c < _CACHED_RUNES)
		return XLOCALE_CTYPE(loc)->_CurrentRuneLocale->__mapupper[c];
	rr = &XLOCALE_CTYPE(loc)->_CurrentRuneLocale->__mapupper_ext;
	/* Binary search -- see bsearch.c for explanation. */
	base = rr->__ranges;
	for (lim = rr->__nranges; lim != 0; lim >>= 1) {
		re = base + (lim >> 1);
		if (re->__min <= c && c <= re->__max)
			return (re->__map + c - re->__min);
		else if (c > re->__max) {
			base = re + 1;
			lim--;
		}
	}

	return(c);
}

__ct_rune_t
___toupper(__ct_rune_t c)
{
	return ___toupper_l(c, __current_locale());
}
