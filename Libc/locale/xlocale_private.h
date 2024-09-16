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

#ifndef _XLOCALE_PRIVATE_H_
#define _XLOCALE_PRIVATE_H_

#define __DARWIN_XLOCALE_PRIVATE
#include <sys/cdefs.h>
#include <xlocale.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <libkern/OSAtomic.h>
#include <pthread.h>
#include <pthread/tsd_private.h>
#include <limits.h>
#include <os/lock.h>
#include "setlocale.h"
#include "timelocal.h"
#include <TargetConditionals.h>
#undef __DARWIN_XLOCALE_PRIVATE

#undef MB_CUR_MAX_L
#define MB_CUR_MAX_L(x)	(XLOCALE_CTYPE(x)->__mb_cur_max)
#undef MB_CUR_MAX
#define MB_CUR_MAX	MB_CUR_MAX_L(__current_locale())

typedef void (*__free_extra_t)(void *);

#define XMAGIC		0x786c6f63616c6530LL	/* 'xlocale0' */

/**
 * The XLC_ values are indexes into the components array.  They are defined in
 * the same order as the LC_ values in locale.h, but without the LC_ALL zero
 * value.  Translating from LC_X to XLC_X is done by subtracting one.
 *
 * Any reordering of this enum should ensure that these invariants are not
 * violated.
 */
enum {
	XLC_COLLATE = 0,
	XLC_CTYPE,
	XLC_MONETARY,
	XLC_NUMERIC,
	XLC_TIME,
	XLC_MESSAGES,
	XLC_LAST
};

_Static_assert(XLC_LAST - XLC_COLLATE == 6, "XLC values should be contiguous");
_Static_assert(XLC_COLLATE == LC_COLLATE - 1,
               "XLC_COLLATE doesn't match the LC_COLLATE value.");
_Static_assert(XLC_CTYPE == LC_CTYPE - 1,
               "XLC_CTYPE doesn't match the LC_CTYPE value.");
_Static_assert(XLC_MONETARY == LC_MONETARY - 1,
               "XLC_MONETARY doesn't match the LC_MONETARY value.");
_Static_assert(XLC_NUMERIC == LC_NUMERIC - 1,
               "XLC_NUMERIC doesn't match the LC_NUMERIC value.");
_Static_assert(XLC_TIME == LC_TIME - 1,
               "XLC_TIME doesn't match the LC_TIME value.");
_Static_assert(XLC_MESSAGES == LC_MESSAGES - 1,
               "XLC_MESSAGES doesn't match the LC_MESSAGES value.");

struct xlocale_refcounted {
	/** Number of references to this component. */
	int retain_count;
	/** Function used to destroy this component, if one is required. */
	__free_extra_t destructor;
};

#define XLOCALE_DEF_VERSION_LEN 12

/**
 * Header for a locale component.  All locale components must begin wtih this
 * header.
 */
struct xlocale_component {
	struct xlocale_refcounted header;
	/** Name of the locale used for this component. */
	char locale[ENCODING_LEN+1];
	/** Version of the definition for this component. */
	char version[XLOCALE_DEF_VERSION_LEN];
};

struct xlocale_ldpart {
	struct xlocale_component header;
	char *buffer;
};
/*
 * the next four structures must have the first three fields of the same
 * as the xlocale_ldpart structure above.
 */
struct xlocale_messages;
struct xlocale_monetary;
struct xlocale_numeric;
struct xlocale_time;

#define	XLC_PART_MASKS	((1 << XLC_MESSAGES) | (1 << XLC_MONETARY) | \
    (1 << XLC_NUMERIC) | (1 << XLC_TIME))

/* the extended locale structure */
    /* values for __numeric_fp_cvt */
#define	LC_NUMERIC_FP_UNINITIALIZED	0
#define	LC_NUMERIC_FP_SAME_LOCALE	1
#define	LC_NUMERIC_FP_USE_LOCALE	2

struct _xlocale {
/* The item(s) before __magic are not copied when duplicating locale_t's */
	struct xlocale_refcounted header;
	/* only used for locale_t's in __lc_numeric_loc */
	/* 10 independent mbstate_t buffers! */
	__darwin_mbstate_t __mbs_mblen;
	__darwin_mbstate_t __mbs_mbrlen;
	__darwin_mbstate_t __mbs_mbrtowc;
	__darwin_mbstate_t __mbs_mbsnrtowcs;
	__darwin_mbstate_t __mbs_mbsrtowcs;
	__darwin_mbstate_t __mbs_mbtowc;
	__darwin_mbstate_t __mbs_wcrtomb;
	__darwin_mbstate_t __mbs_wcsnrtombs;
	__darwin_mbstate_t __mbs_wcsrtombs;
	__darwin_mbstate_t __mbs_wctomb;
	os_unfair_lock __lock;
/* magic (Here up to the end is copied when duplicating locale_t's) */
	int64_t __magic;
/* flags */
	unsigned char _messages_using_locale;
	unsigned char _monetary_using_locale;
	unsigned char _numeric_using_locale;
	unsigned char _time_using_locale;
	unsigned char __mlocale_changed;
	unsigned char __nlocale_changed;
	unsigned char __numeric_fp_cvt;
	struct xlocale_component *components[XLC_LAST];
	struct _xlocale *__lc_numeric_loc;
/* localeconv */
	struct lconv __lc_localeconv;
};

#define	XLOCALE_COLLATE(l) \
    ((struct xlocale_collate *)(l)->components[XLC_COLLATE])
#define	XLOCALE_CTYPE(l)	\
    ((struct xlocale_ctype *)(l)->components[XLC_CTYPE])
#define	XLOCALE_MONETARY(l)	\
    ((struct xlocale_monetary *)(l)->components[XLC_MONETARY])
#define	XLOCALE_NUMERIC(l)	\
    ((struct xlocale_numeric *)(l)->components[XLC_NUMERIC])
#define	XLOCALE_TIME(l)	\
    ((struct xlocale_time *)(l)->components[XLC_TIME])
#define	XLOCALE_MESSAGES(l)	\
    ((struct xlocale_messages *)(l)->components[XLC_MESSAGES])

#define DEFAULT_CURRENT_LOCALE(x)	\
				if ((x) == NULL) { \
					(x) = __current_locale(); \
				} else if ((x) == LC_GLOBAL_LOCALE) { \
					(x) = &__global_locale; \
				}

#define NORMALIZE_LOCALE(x)	if ((x) == LC_C_LOCALE) { \
					(x) = _c_locale; \
				} else if ((x) == LC_GLOBAL_LOCALE) { \
					(x) = &__global_locale; \
				}

#define XL_LOCK(x)	os_unfair_lock_lock(&(x)->__lock);
#define XL_UNLOCK(x)	os_unfair_lock_unlock(&(x)->__lock);

static __inline void*
xlocale_retain(void *val)
{
	struct xlocale_refcounted *obj = val;

	if (obj == NULL)
		return (NULL);

	OSAtomicIncrement32Barrier(&obj->retain_count);

	return (val);
}

static __inline void
xlocale_release(void *val)
{
	struct xlocale_refcounted *obj = val;

	if (obj == NULL)
		return;

	/*
	 * FreeBSD has one main difference in refcounting that we may adopt
	 * later:
	 *
	 * retain_count is a signed long, 0 is the minimum for a live object
	 */
	if (OSAtomicDecrement32Barrier(&obj->retain_count) == 0) {
		if (obj->destructor != NULL)
			(*obj->destructor)(val);
	}
}

#if __DARWIN_C_LEVEL >= __DARWIN_C_FULL
/*
 * Some files in Libc want POSIX C, but they won't be needing
 * xlocale_fill_name() anyways.
 */
static __inline void
xlocale_fill_name(struct xlocale_component *comp, const char *name)
{

	if (comp == NULL)
		return;

	(void)strlcpy(comp->locale, name, sizeof(comp->locale));
}
#endif

__attribute__((visibility("hidden")))
extern struct xlocale_ctype _DefaultRuneXLocale;

__attribute__((visibility("hidden")))
extern struct _xlocale	__global_locale;

__attribute__((visibility("hidden")))
extern pthread_key_t	__locale_key;

__BEGIN_DECLS

void	destruct_ldpart(void *);
locale_t __numeric_ctype(locale_t);
void	__xlocale_init(void);

static inline __attribute__((always_inline)) locale_t
__current_locale(void)
{
#if TARGET_OS_SIMULATOR
	/* <rdar://problem/14136256> Crash in _objc_inform for duplicate class name during simulator launch
	 * TODO: Remove after the simulator's libSystem is initialized properly.
	 */
	if (__locale_key == (pthread_key_t)-1) {
		return &__global_locale;
	}
#endif
	void *__thread_locale;
	if (_pthread_has_direct_tsd()) {
		__thread_locale = _pthread_getspecific_direct(__locale_key);
	} else {
		__thread_locale = pthread_getspecific(__locale_key);
	}
	return (__thread_locale ? (locale_t)__thread_locale : &__global_locale);
}

static inline __attribute__((always_inline)) locale_t
__locale_ptr(locale_t __loc)
{
	NORMALIZE_LOCALE(__loc);
	return __loc;
}

__END_DECLS

#endif /* _XLOCALE_PRIVATE_H_ */
