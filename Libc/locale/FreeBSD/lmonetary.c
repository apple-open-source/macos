/*
 * Copyright (c) 2000, 2001 Alexey Zelkin <phantom@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/locale/lmonetary.c,v 1.19 2003/06/26 10:46:16 phantom Exp $");

#include "xlocale_private.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ldpart.h"
#include "lmonetary.h"

extern const char * __fix_locale_grouping_str(const char *);

#define LCMONETARY_SIZE_FULL (sizeof(struct lc_monetary_T) / sizeof(char *))
#define LCMONETARY_SIZE_MIN \
		(offsetof(struct lc_monetary_T, int_p_cs_precedes) / \
		    sizeof(char *))

static char	empty[] = "";
static char	numempty[] = { CHAR_MAX, '\0'};

static const struct lc_monetary_T _C_monetary_locale = {
	empty,		/* int_curr_symbol */
	empty,		/* currency_symbol */
	empty,		/* mon_decimal_point */
	empty,		/* mon_thousands_sep */
	empty,		/* mon_grouping [C99 7.11.2.1]*/
	empty,		/* positive_sign */
	empty,		/* negative_sign */
	numempty,	/* int_frac_digits */
	numempty,	/* frac_digits */
	numempty,	/* p_cs_precedes */
	numempty,	/* p_sep_by_space */
	numempty,	/* n_cs_precedes */
	numempty,	/* n_sep_by_space */
	numempty,	/* p_sign_posn */
	numempty,	/* n_sign_posn */
	numempty,	/* int_p_cs_precedes */
	numempty,	/* int_n_cs_precedes */
	numempty,	/* int_p_sep_by_space */
	numempty,	/* int_n_sep_by_space */
	numempty,	/* int_p_sign_posn */
	numempty	/* int_n_sign_posn */
};

static char
cnv(const char *str)
{
	int i = strtol(str, NULL, 10);

	if (i == -1)
		i = CHAR_MAX;
	return ((char)i);
}

__private_extern__ int
__monetary_load_locale(const char *name, locale_t loc)
{
	int ret;
	struct xlocale_monetary *xp;
	static struct xlocale_monetary *cache = NULL;

	/* 'name' must be already checked. */
	if (strcmp(name, "C") == 0 || strcmp(name, "POSIX") == 0 ||
	    strncmp(name, "C.", 2) == 0) {
		if (!loc->_monetary_using_locale)
			return (_LDP_CACHE);
		loc->_monetary_using_locale = 0;
		xlocale_release(loc->components[XLC_MONETARY]);
		loc->components[XLC_MONETARY] = NULL;
		loc->__mlocale_changed = 1;
		return (_LDP_CACHE);
	}

	if (loc->_monetary_using_locale && strcmp(name, XLOCALE_MONETARY(loc)->buffer) == 0)
		return (_LDP_CACHE);
	/*
	 * If the locale name is the same as our cache, use the cache.
	 */
	if (cache && cache->buffer && strcmp(name, cache->buffer) == 0) {
		loc->_monetary_using_locale = 1;
		xlocale_release(loc->components[XLC_MONETARY]);
		loc->components[XLC_MONETARY] = (void *)cache;
		xlocale_retain(cache);
		loc->__mlocale_changed = 1;
		return (_LDP_CACHE);
	}
	if ((xp = (struct xlocale_monetary *)malloc(sizeof(*xp))) == NULL)
		return _LDP_ERROR;
	xp->header.header.retain_count = 1;
	xp->header.header.destructor = destruct_ldpart;
	xp->buffer = NULL;

	ret = __part_load_locale(name, &loc->_monetary_using_locale,
		&xp->buffer, "LC_MONETARY",
		LCMONETARY_SIZE_FULL, LCMONETARY_SIZE_MIN,
		(const char **)&xp->locale);
	if (ret != _LDP_ERROR)
		loc->__mlocale_changed = 1;
	else
		free(xp);
	if (ret == _LDP_LOADED) {
		xp->locale.mon_grouping =
		     __fix_locale_grouping_str(xp->locale.mon_grouping);

#define M_ASSIGN_CHAR(NAME) (((char *)xp->locale.NAME)[0] = \
			     cnv(xp->locale.NAME))

		M_ASSIGN_CHAR(int_frac_digits);
		M_ASSIGN_CHAR(frac_digits);
		M_ASSIGN_CHAR(p_cs_precedes);
		M_ASSIGN_CHAR(p_sep_by_space);
		M_ASSIGN_CHAR(n_cs_precedes);
		M_ASSIGN_CHAR(n_sep_by_space);
		M_ASSIGN_CHAR(p_sign_posn);
		M_ASSIGN_CHAR(n_sign_posn);

		/*
		 * The six additional C99 international monetary formatting
		 * parameters default to the national parameters when
		 * reading FreeBSD LC_MONETARY data files.
		 */
#define	M_ASSIGN_ICHAR(NAME)						\
		do {							\
			if (xp->locale.int_##NAME == NULL)	\
				xp->locale.int_##NAME =	\
				    xp->locale.NAME;		\
			else						\
				M_ASSIGN_CHAR(int_##NAME);		\
		} while (0)

		M_ASSIGN_ICHAR(p_cs_precedes);
		M_ASSIGN_ICHAR(n_cs_precedes);
		M_ASSIGN_ICHAR(p_sep_by_space);
		M_ASSIGN_ICHAR(n_sep_by_space);
		M_ASSIGN_ICHAR(p_sign_posn);
		M_ASSIGN_ICHAR(n_sign_posn);
		xlocale_release(loc->components[XLC_MONETARY]);
		loc->components[XLC_MONETARY] = (void *)xp;
		xlocale_release(cache);
		cache = xp;
		xlocale_retain(cache);
	}
	return (ret);
}

__private_extern__ struct lc_monetary_T *
__get_current_monetary_locale(locale_t loc)
{
	return (loc->_monetary_using_locale
		? &XLOCALE_MONETARY(loc)->locale
		: (struct lc_monetary_T *)&_C_monetary_locale);
}

#ifdef LOCALE_DEBUG
void
monetdebug() {
locale_t loc = __current_locale();
printf(	"int_curr_symbol = %s\n"
	"currency_symbol = %s\n"
	"mon_decimal_point = %s\n"
	"mon_thousands_sep = %s\n"
	"mon_grouping = %s\n"
	"positive_sign = %s\n"
	"negative_sign = %s\n"
	"int_frac_digits = %d\n"
	"frac_digits = %d\n"
	"p_cs_precedes = %d\n"
	"p_sep_by_space = %d\n"
	"n_cs_precedes = %d\n"
	"n_sep_by_space = %d\n"
	"p_sign_posn = %d\n"
	"n_sign_posn = %d\n",
	"int_p_cs_precedes = %d\n"
	"int_p_sep_by_space = %d\n"
	"int_n_cs_precedes = %d\n"
	"int_n_sep_by_space = %d\n"
	"int_p_sign_posn = %d\n"
	"int_n_sign_posn = %d\n",
	XLOCALE_MONETARY(loc)->locale.int_curr_symbol,
	XLOCALE_MONETARY(loc)->locale.currency_symbol,
	XLOCALE_MONETARY(loc)->locale.mon_decimal_point,
	XLOCALE_MONETARY(loc)->locale.mon_thousands_sep,
	XLOCALE_MONETARY(loc)->locale.mon_grouping,
	XLOCALE_MONETARY(loc)->locale.positive_sign,
	XLOCALE_MONETARY(loc)->locale.negative_sign,
	XLOCALE_MONETARY(loc)->locale.int_frac_digits[0],
	XLOCALE_MONETARY(loc)->locale.frac_digits[0],
	XLOCALE_MONETARY(loc)->locale.p_cs_precedes[0],
	XLOCALE_MONETARY(loc)->locale.p_sep_by_space[0],
	XLOCALE_MONETARY(loc)->locale.n_cs_precedes[0],
	XLOCALE_MONETARY(loc)->locale.n_sep_by_space[0],
	XLOCALE_MONETARY(loc)->locale.p_sign_posn[0],
	XLOCALE_MONETARY(loc)->locale.n_sign_posn[0],
	XLOCALE_MONETARY(loc)->locale.int_p_cs_precedes[0],
	XLOCALE_MONETARY(loc)->locale.int_p_sep_by_space[0],
	XLOCALE_MONETARY(loc)->locale.int_n_cs_precedes[0],
	XLOCALE_MONETARY(loc)->locale.int_n_sep_by_space[0],
	XLOCALE_MONETARY(loc)->locale.int_p_sign_posn[0],
	XLOCALE_MONETARY(loc)->locale.int_n_sign_posn[0]
);
}
#endif /* LOCALE_DEBUG */
