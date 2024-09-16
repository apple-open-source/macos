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
__FBSDID("$FreeBSD: src/lib/libc/locale/lnumeric.c,v 1.16 2003/06/26 10:46:16 phantom Exp $");

#include "xlocale_private.h"

#include <limits.h>
#include <string.h>

#include "ldpart.h"
#include "lnumeric.h"

extern const char *__fix_locale_grouping_str(const char *);

#define LCNUMERIC_SIZE (sizeof(struct lc_numeric_T) / sizeof(char *))

static const struct lc_numeric_T _C_numeric_locale = {
	".",     	/* decimal_point */
	"",     	/* thousands_sep */
	""		/* grouping [C99 7.11.2.1]*/
};

__private_extern__ int
__numeric_load_locale(const char *name, locale_t loc)
{
	int ret;
	struct xlocale_numeric *xp;
	static struct xlocale_numeric *cache = NULL;

	/* 'name' must be already checked. */
	if (strcmp(name, "C") == 0 || strcmp(name, "POSIX") == 0 ||
	    strncmp(name, "C.", 2) == 0) {
		if (!loc->_numeric_using_locale)
			return (_LDP_CACHE);
		loc->_numeric_using_locale = 0;
		xlocale_release(loc->components[XLC_NUMERIC]);
		loc->components[XLC_NUMERIC] = NULL;
		loc->__nlocale_changed = 1;
		return (_LDP_CACHE);
	}

	if (loc->_numeric_using_locale && strcmp(name, XLOCALE_NUMERIC(loc)->buffer) == 0)
		return (_LDP_CACHE);
	/*
	 * If the locale name is the same as our cache, use the cache.
	 */
	if (cache && cache->buffer && strcmp(name, cache->buffer) == 0) {
		loc->_numeric_using_locale = 1;
		xlocale_release(loc->components[XLC_NUMERIC]);
		loc->components[XLC_NUMERIC] = (void *)cache;
		xlocale_retain(cache);
		loc->__nlocale_changed = 1;
		return (_LDP_CACHE);
	}
	if ((xp = (struct xlocale_numeric *)malloc(sizeof(*xp))) == NULL)
		return _LDP_ERROR;
	xp->header.header.retain_count = 1;
	xp->header.header.destructor = destruct_ldpart;
	xp->buffer = NULL;

	ret = __part_load_locale(name, &loc->_numeric_using_locale,
		&xp->buffer, "LC_NUMERIC",
		LCNUMERIC_SIZE, LCNUMERIC_SIZE,
		(const char **)&xp->locale);
	if (ret != _LDP_ERROR)
		loc->__nlocale_changed = 1;
	else
		free(xp);
	if (ret == _LDP_LOADED) {
		/* Can't be empty according to C99 */
		if (*xp->locale.decimal_point == '\0')
			xp->locale.decimal_point =
			    _C_numeric_locale.decimal_point;
		xp->locale.grouping =
		    __fix_locale_grouping_str(xp->locale.grouping);
		xlocale_release(loc->components[XLC_NUMERIC]);
		loc->components[XLC_NUMERIC] = (void *)xp;
		xlocale_release(cache);
		cache = xp;
		xlocale_retain(cache);
	}
	return (ret);
}

__private_extern__ struct lc_numeric_T *
__get_current_numeric_locale(locale_t loc)
{
	return (loc->_numeric_using_locale
		? &XLOCALE_NUMERIC(loc)->locale
		: (struct lc_numeric_T *)&_C_numeric_locale);
}

#ifdef LOCALE_DEBUG
void
numericdebug(void) {
locale_t loc = __current_locale();
printf(	"decimal_point = %s\n"
	"thousands_sep = %s\n"
	"grouping = %s\n",
	XLOCALE_NUMERIC(loc)->locale.decimal_point,
	XLOCALE_NUMERIC(loc)->locale.thousands_sep,
	XLOCALE_NUMERIC(loc)->locale.grouping
);
}
#endif /* LOCALE_DEBUG */
