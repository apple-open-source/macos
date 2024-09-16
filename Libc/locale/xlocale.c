/*
 * Copyright (c) 2005, 2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "xlocale_private.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include "collate.h"
#include "ldpart.h"
#include "lmonetary.h"
#include "lnumeric.h"
#include "mblocal.h"

extern struct xlocale_collate __xlocale_C_collate;

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

#define NMBSTATET	10
#define C_LOCALE_INITIALIZER	{	\
	{ 1, NULL },			\
	{}, {}, {}, {}, {},		\
	{}, {}, {}, {}, {},		\
	OS_UNFAIR_LOCK_INIT,		\
	XMAGIC,				\
	0, 0, 0, 0, 1, 1, 0,	\
	{				\
		[XLC_COLLATE] = (void *)&__xlocale_C_collate,	\
		[XLC_CTYPE] = (void *)&_DefaultRuneXLocale,	\
	},				\
}

#define	LDPART_BUFFER(x)	\
    (((struct xlocale_ldpart *)(x))->buffer)

static char C[] = "C";
static struct _xlocale __c_locale = C_LOCALE_INITIALIZER;
const locale_t _c_locale = (const locale_t)&__c_locale;
struct _xlocale __global_locale = C_LOCALE_INITIALIZER;
pthread_key_t __locale_key = (pthread_key_t)-1;

extern int __collate_load_tables(const char *, locale_t);
extern int __detect_path_locale(void);
extern const char *__get_locale_env(int);
extern int __messages_load_locale(const char *, locale_t);
extern int __monetary_load_locale(const char *, locale_t);
extern int __numeric_load_locale(const char *, locale_t);
extern int __setrunelocale(const char *, locale_t);
extern int __time_load_locale(const char *, locale_t);

static void destruct_locale(void *v);

/*
 * check that the encoding is the right size, isn't . or .. and doesn't
 * contain any slashes
 */
static inline __attribute__((always_inline)) int
_checkencoding(const char *encoding)
{
	return (encoding && (strlen(encoding) > ENCODING_LEN
	  || (encoding[0] == '.' && (encoding[1] == 0
	  || (encoding[1] == '.' && encoding[2] == 0)))
	  || strchr(encoding, '/') != NULL)) ? -1 : 0;
}

/*
 * check that the locale has the right magic number
 */
static inline __attribute__((always_inline)) int
_checklocale(const locale_t loc)
{
	if (!loc)
		return 0;
	return (loc == LC_GLOBAL_LOCALE || loc->__magic == XMAGIC) ? 0 : -1;
}

/*
 * copy a locale_t except anything before the magic value
 */
static inline __attribute__((always_inline)) void
_copylocale(locale_t dst, const locale_t src)
{
	memcpy(&dst->__magic, &src->__magic, sizeof(*dst) - offsetof(struct _xlocale, __magic));
}

/*
 * Make a copy of a locale_t, locking/unlocking the source.
 * A NULL locale_t means to make a copy of the current
 * locale while LC_GLOBAL_LOCALE means to copy the global locale.  If
 * &__c_locale is passed (meaning a C locale is desired), just make
 * a copy.
 */
static locale_t
_duplocale(locale_t loc)
{
	locale_t new;

	if ((new = (locale_t)malloc(sizeof(struct _xlocale))) == NULL)
		return NULL;
	new->header.retain_count = 1;
	new->header.destructor = destruct_locale;
	new->__lock = OS_UNFAIR_LOCK_INIT;
	if (loc == NULL)
		loc = __current_locale();
	else if (loc == LC_GLOBAL_LOCALE)
		loc = &__global_locale;
	else if (loc == &__c_locale) {
		*new = __c_locale;
		new->header.retain_count = 1;
		new->header.destructor = destruct_locale;
		new->__lock = OS_UNFAIR_LOCK_INIT;
		return new;
	}
	XL_LOCK(loc);
	_copylocale(new, loc);
	XL_UNLOCK(loc);
	/* __mbs_mblen is the first of NMBSTATET mbstate_t buffers */
	bzero(&new->__mbs_mblen, offsetof(struct _xlocale, __magic)
	    - offsetof(struct _xlocale, __mbs_mblen));

	for (int i = XLC_COLLATE; i < XLC_LAST; i++) {
		xlocale_retain(new->components[i]);
	}

	xlocale_retain(new->__lc_numeric_loc);

	return new;
}

/*
 * Modify a locale_t, setting the parts specified in the mask
 * to the locale specified by the string.  If the string is NULL, the C
 * locale is used.  If the string is empty, the value is determined from
 * the environment.  -1 is returned on error, and loc is in a partial state.
 */
static int
_modifylocale(locale_t loc, int mask, __const char *locale)
{
	int m, ret;
	const char *enc = NULL;
	char *oenc;

	if (!locale)
		locale = C;

	ret = __detect_path_locale();
	if (ret) {
		errno = ret;
		return -1;
	}

	if (*locale)
		enc = locale;
	for(m = 1; m <= _LC_LAST_MASK; m <<= 1) {
		if (m & mask) {
			switch(m) {
			case LC_COLLATE_MASK:
				if (!*locale) {
					enc = __get_locale_env(LC_COLLATE);
					if (_checkencoding(enc) < 0) {
						errno = EINVAL;
						return -1;
					}
				}
				oenc = (XLOCALE_COLLATE(loc)->__collate_load_error ? C :
				    loc->components[XLC_COLLATE]->locale);
				if (strcmp(enc, oenc) != 0 && __collate_load_tables(enc, loc) == _LDP_ERROR)
					return -1;
				xlocale_fill_name(loc->components[XLC_COLLATE],
				    enc);
				break;
			case LC_CTYPE_MASK:
				if (!*locale) {
					enc = __get_locale_env(LC_CTYPE);
					if (_checkencoding(enc) < 0) {
						errno = EINVAL;
						return -1;
					}
				}
				if (strcmp(enc, loc->components[XLC_CTYPE]->locale) != 0) {
					if ((ret = __setrunelocale(enc, loc)) != 0) {
						errno = ret;
						return -1;
					}
					xlocale_fill_name(loc->components[XLC_CTYPE],
					    enc);
					if (loc->__numeric_fp_cvt == LC_NUMERIC_FP_SAME_LOCALE)
						loc->__numeric_fp_cvt = LC_NUMERIC_FP_UNINITIALIZED;
				}
				break;
			case LC_MESSAGES_MASK:
				if (!*locale) {
					enc = __get_locale_env(LC_MESSAGES);
					if (_checkencoding(enc) < 0) {
						errno = EINVAL;
						return -1;
					}
				}
				oenc = (loc->_messages_using_locale ?
				    LDPART_BUFFER(XLOCALE_MESSAGES(loc)) : C);
				if (strcmp(enc, oenc) != 0 && __messages_load_locale(enc, loc) == _LDP_ERROR)
					return -1;
				xlocale_fill_name(loc->components[XLC_MESSAGES],
				    enc);
				break;
			case LC_MONETARY_MASK:
				if (!*locale) {
					enc = __get_locale_env(LC_MONETARY);
					if (_checkencoding(enc) < 0) {
						errno = EINVAL;
						return -1;
					}
				}
				oenc = (loc->_monetary_using_locale ?
				    LDPART_BUFFER(XLOCALE_MONETARY(loc)) : C);
				if (strcmp(enc, oenc) != 0 && __monetary_load_locale(enc, loc) == _LDP_ERROR)
					return -1;
				xlocale_fill_name(loc->components[XLC_MONETARY],
				    enc);
				break;
			case LC_NUMERIC_MASK:
				if (!*locale) {
					enc = __get_locale_env(LC_NUMERIC);
					if (_checkencoding(enc) < 0) {
						errno = EINVAL;
						return -1;
					}
				}
				oenc = (loc->_numeric_using_locale ?
				    LDPART_BUFFER(XLOCALE_NUMERIC(loc)) : C);
				if (strcmp(enc, oenc) != 0) {
					if (__numeric_load_locale(enc, loc) == _LDP_ERROR)
						return -1;
					xlocale_fill_name(loc->components[XLC_NUMERIC],
					    enc);
					loc->__numeric_fp_cvt = LC_NUMERIC_FP_UNINITIALIZED;
					xlocale_release(loc->__lc_numeric_loc);
					loc->__lc_numeric_loc = NULL;
				}
				break;
			case LC_TIME_MASK:
				if (!*locale) {
					enc = __get_locale_env(LC_TIME);
					if (_checkencoding(enc) < 0) {
						errno = EINVAL;
						return -1;
					}
				}
				oenc = (loc->_time_using_locale ?
				    LDPART_BUFFER(XLOCALE_TIME(loc)) : C);
				if (strcmp(enc, oenc) != 0 && __time_load_locale(enc, loc) == _LDP_ERROR)
					return -1;
				xlocale_fill_name(loc->components[XLC_TIME],
				    enc);
				break;
			}
		}
	}
	return 0;
}

/*
 * release all the memory objects (the memory will be freed when the refcount
 * becomes zero)
 */
static void
destruct_locale(void *v)
{
	locale_t loc = v;

	for (int i = XLC_COLLATE; i < XLC_LAST; i++)
		xlocale_release(loc->components[i]);
	xlocale_release(loc->__lc_numeric_loc);
	free(loc);
}

/*
 * EXTERNAL: Duplicate a (non-NULL) locale_t.  LC_GLOBAL_LOCALE means the
 * global locale, while NULL means the current locale.  NULL is returned
 * on error.
 */
locale_t
duplocale(locale_t loc)
{
	if (_checklocale(loc) < 0) {
		errno = EINVAL;
		return NULL;
	}
	return _duplocale(loc);
}

/*
 * EXTERNAL: Free a locale_t, releasing all memory objects.  Don't free
 * illegal locale_t's or the global locale.
 */
int
freelocale(locale_t loc)
{
	if (!loc || _checklocale(loc) < 0 || loc == &__global_locale
	    || loc == LC_GLOBAL_LOCALE || loc == &__c_locale) {
		errno = EINVAL;
		return -1;
	}
	xlocale_release(loc);
	return 0;
}

/*
 * EXTERNAL: Create a new locale_t, based on the base locale_t, and modified
 * by the mask and locale string.  If the base is NULL, the current locale
 * is used as the base.  If locale is NULL, changes are made from the C locale
 * for categories set in mask.
 */
locale_t
newlocale(int mask, __const char *locale, locale_t base)
{
	locale_t new;
	int lcmask = (mask & LC_ALL_MASK);

	if (_checkencoding(locale) < 0) {
		errno = EINVAL;
		return NULL;
	}
	if (lcmask == LC_ALL_MASK)
		base = (locale_t)&__c_locale;
	else if (_checklocale(base) < 0) {
		errno = EINVAL;
		return NULL;
	}
	new = _duplocale(base);
	if (new == NULL)
		return NULL;
	if (lcmask == 0 || (lcmask == LC_ALL_MASK && locale == NULL))
		return new;
	if (_modifylocale(new, lcmask, locale) < 0) {
		freelocale(new);
		return NULL;
	}
	return new;
}

/*
 * PRIVATE EXTERNAL: Returns the locale that can be used by wcstod and
 * family, to convert the wide character string to a multi-byte string
 * (the LC_NUMERIC and LC_CTYPE locales may be different).
 */
__private_extern__ locale_t
__numeric_ctype(locale_t loc)
{
	switch(loc->__numeric_fp_cvt) {
	case LC_NUMERIC_FP_UNINITIALIZED: {
		const char *ctype = loc->components[XLC_CTYPE]->locale;
		const char *numeric = (loc->_numeric_using_locale ?
		    LDPART_BUFFER(XLOCALE_NUMERIC(loc)) : C);
		if (strcmp(ctype, numeric) == 0) {
			loc->__numeric_fp_cvt = LC_NUMERIC_FP_SAME_LOCALE;
			return loc;
		} else {
			loc->__lc_numeric_loc = newlocale(LC_CTYPE_MASK, numeric, (locale_t)&__c_locale);
			if (loc->__lc_numeric_loc) {
				loc->__numeric_fp_cvt = LC_NUMERIC_FP_USE_LOCALE;
				return loc->__lc_numeric_loc;
			} else { /* shouldn't happen, but just use the same locale */
				loc->__numeric_fp_cvt = LC_NUMERIC_FP_SAME_LOCALE;
				return loc;
			}
		}
	}
	case LC_NUMERIC_FP_SAME_LOCALE:
		return loc;
	case LC_NUMERIC_FP_USE_LOCALE:
		return loc->__lc_numeric_loc;
	}
	return loc;	/* shouldn't happen */
}

/*
 * Darwin's querylocale(3) mask's assigned bits are in alphabetical order of the
 * category name, so map these back to XLC_* constants.
 */
static int querylocale_mapping[] = {
	[0] = XLC_COLLATE,
	[1] = XLC_CTYPE,
	[2] = XLC_MESSAGES,
	[3] = XLC_MONETARY,
	[4] = XLC_NUMERIC,
	[5] = XLC_TIME,
};

/*
 * EXTERNAL: Returns the locale string for the part specified in mask.  The
 * least significant bit is used.  If loc is NULL, the current per-thread
 * locale is used.
 */
const char *
querylocale(int mask, locale_t loc)
{
	int type;
	const char *ret;

	if (_checklocale(loc) < 0 || (mask & LC_ALL_MASK) == 0) {
		errno = EINVAL;
		return NULL;
	}
	DEFAULT_CURRENT_LOCALE(loc);

	/* XXX VERSION_MASK */
	type = ffs(mask) - 1;
	if (type >= nitems(querylocale_mapping)) {
		errno = EINVAL;
		return NULL;
	}

	type = querylocale_mapping[type];
	assert(type < XLC_LAST);

	/*
	 * We don't need to consult _numeric_using_locale, et al., here, because
	 * if its components[type] is set and we're not using it, it should be
	 * populated as the C locale and we'll still return C.
	 */
	XL_LOCK(loc);
	if (loc->components[type] != NULL)
		ret = loc->components[type]->locale;
	else
		ret = C;
	XL_UNLOCK(loc);
	return ret;
}

/*
 * EXTERNAL: Set the thread-specific locale.  The previous locale is returned.
 * Use LC_GLOBAL_LOCALE to set the global locale.  LC_GLOBAL_LOCALE
 * may also be returned if there was no previous thread-specific locale in
 * effect.  If loc is NULL, the current locale is returned, but no locale
 * chance is made.  NULL is returned on error.
 */
locale_t
uselocale(locale_t loc)
{
	locale_t orig;

	if (loc == NULL)
		orig = (locale_t)pthread_getspecific(__locale_key);
	else {
		if (_checklocale(loc) < 0) {
			errno = EINVAL;
			return NULL;
		}
		if (loc == LC_GLOBAL_LOCALE ||
		    loc == &__global_locale)	/* should never happen */
			loc = NULL;
		xlocale_retain(loc);
		orig = pthread_getspecific(__locale_key);
		pthread_setspecific(__locale_key, loc);
		xlocale_release(orig);
	}
	return (orig ? orig : LC_GLOBAL_LOCALE);
}

/*
 * EXTERNAL: Used by the MB_CUR_MAX macro to determine the thread-specific
 * value.
 */
int
___mb_cur_max(void)
{
	return XLOCALE_CTYPE(__current_locale())->__mb_cur_max;
}

/*
 * EXTERNAL: Used by the MB_CUR_MAX_L macro to determine the thread-specific
 * value, from the given locale_t.
 */
int
___mb_cur_max_l(locale_t loc)
{
	return XLOCALE_CTYPE(__locale_ptr(loc))->__mb_cur_max;
}

static void
__xlocale_release(void *loc)
{
	locale_t l = loc;
	xlocale_release(l);
}

/*
 * Called from the Libc initializer to setup the thread-specific key.
 */
__private_extern__ void
__xlocale_init(void)
{
	if (__locale_key == (pthread_key_t)-1) {
		__locale_key = __LIBC_PTHREAD_KEY_XLOCALE;
		pthread_key_init_np(__locale_key, __xlocale_release);
	}
}

