/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc/locale/collate.h,v 1.15 2005/02/27 20:31:13 ru Exp $
 */

#ifndef _COLLATE_H_
#define	_COLLATE_H_

#include <sys/cdefs.h>
#ifndef __LIBC__
#include <sys/types.h>
#endif /* !__LIBC__ */
#include <limits.h>
#include "xlocale_private.h"

#define COLLATE_STR_LEN 24
#define TABLE_SIZE 100
#define COLLATE_VERSION    "1.0\n"
#define COLLATE_VERSION1_1 "1.1\n"
#define COLLATE_VERSION1_1A "1.1A\n"
#define COLLATE_VERSION1_2 "1.2\n"

#define	COLLATE_FMT_VERSION_LEN	12
#define	COLLATE_FMT_VERSION	"DARWIN 1.0\n"

/* XXX */
#ifdef __APPLE__
#if COLL_WEIGHTS_MAX < 10
#undef COLL_WEIGHTS_MAX
#define	COLL_WEIGHTS_MAX	10
#endif
#endif

/* see discussion in string/FreeBSD/strxfrm for this value */
#define COLLATE_MAX_PRIORITY ((1 << 24) - 1)
#define COLLATE_SUBST_PRIORITY (0x40000000)	/* bit indicates subst table */

#define DIRECTIVE_UNDEF 0x00
#define DIRECTIVE_FORWARD 0x01
#define DIRECTIVE_BACKWARD 0x02
#define DIRECTIVE_POSITION 0x04
#define DIRECTIVE_UNDEFINED 0x08	/* special last weight for UNDEFINED */

#define DIRECTIVE_DIRECTION_MASK (DIRECTIVE_FORWARD | DIRECTIVE_BACKWARD)

#define COLLATE_SUBST_DUP 0x0001
#define COLLATE_LEGACY 0x0002

#define IGNORE_EQUIV_CLASS 1

/* __collate_st_info */
typedef struct collate_info {
	__uint8_t directive_count;
	__uint8_t directive[COLL_WEIGHTS_MAX];
#ifdef __APPLE__
	__uint8_t chain_max_len;	/* In padding */
#endif
	__int32_t pri_count[COLL_WEIGHTS_MAX];
	__int32_t flags;
	__int32_t chain_count;
	__int32_t large_count;
	__int32_t subst_count[COLL_WEIGHTS_MAX];
	__int32_t undef_pri[COLL_WEIGHTS_MAX];
} collate_info_t;

/*
 * Pin COLL_WEIGHTS_MAX to 2 for the legacy format; it's the last value used
 * before supporting higher values for newer unicode data.
 */
#define	LEGACY_COLL_WEIGHTS_MAX	2
#define	LEGACY_COLLATE_STR_LEN 10

typedef struct collate_legacy_info {
	__uint8_t directive[LEGACY_COLL_WEIGHTS_MAX];
	__uint8_t flags;
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
	__uint8_t directive_count:4;
	__uint8_t chain_max_len:4;
#else
	__uint8_t chain_max_len:4;
	__uint8_t directive_count:4;
#endif
	__int32_t undef_pri[LEGACY_COLL_WEIGHTS_MAX];
	__int32_t subst_count[LEGACY_COLL_WEIGHTS_MAX];
	__int32_t chain_count;
	__int32_t large_pri_count;
} collate_legacy_info_t;

/* __collate_st_char_pri */
typedef struct collate_char {
	__int32_t pri[COLL_WEIGHTS_MAX];
} collate_char_t;
typedef struct collate_legacy_char {
	__int32_t pri[LEGACY_COLL_WEIGHTS_MAX];
} collate_legacy_char_t;
/* __collate_st_chain_pri */
typedef struct collate_chain {
	__darwin_wchar_t str[COLLATE_STR_LEN];
	__int32_t pri[COLL_WEIGHTS_MAX];
} collate_chain_t;
typedef struct collate_legacy_chain {
	__darwin_wchar_t str[LEGACY_COLLATE_STR_LEN];
	__int32_t pri[LEGACY_COLL_WEIGHTS_MAX];
} collate_legacy_chain_t;
/* __collate_st_large_char_pri */
typedef struct collate_large {
	__int32_t val;
	collate_char_t pri;
} collate_large_t;
typedef struct collate_legacy_large {
	__int32_t val;
	collate_legacy_char_t pri;
} collate_legacy_large_t;
/* __collate_st_subst */
typedef struct collate_subst {
	__int32_t val;
	__darwin_wchar_t str[COLLATE_STR_LEN];
} collate_subst_t;
typedef struct collate_legacy_subst {
	__int32_t val;
	__darwin_wchar_t str[LEGACY_COLLATE_STR_LEN];
} collate_legacy_subst_t;

struct xlocale_collate {
	struct xlocale_component header;
	unsigned char __collate_load_error;
	char *map;
	size_t maplen;

	collate_info_t *info;
	collate_subst_t *subst_table[COLL_WEIGHTS_MAX];
	collate_chain_t *chain_pri_table;
	collate_large_t *large_pri_table;
	collate_char_t *char_pri_table;
};

#ifndef __LIBC__
extern int __collate_load_error;
#define __collate_char_pri_table (*__collate_char_pri_table_ptr)
extern collate_char_t __collate_char_pri_table[UCHAR_MAX + 1];
extern collate_chain_t *__collate_chain_pri_table;
extern __int32_t *__collate_chain_equiv_table;
extern collate_info_t __collate_info;
#endif /* !__LIBC__ */

__BEGIN_DECLS
#ifdef __LIBC__
__darwin_wchar_t	*__collate_mbstowcs(const char *, locale_t);
__darwin_wchar_t	*__collate_wcsdup(const __darwin_wchar_t *);
__darwin_wchar_t	*__collate_substitute(const __darwin_wchar_t *, int, locale_t);
int	__collate_load_tables(const char *, locale_t);
void	__collate_lookup_l(const __darwin_wchar_t *, int *, int *, int *, locale_t);
void	__collate_lookup_which(const __darwin_wchar_t *, int *, int *, int, locale_t);
void	__collate_xfrm(const __darwin_wchar_t *, __darwin_wchar_t **, locale_t);
int	__collate_range_cmp(__darwin_wchar_t, __darwin_wchar_t, locale_t);
size_t	__collate_collating_symbol(__darwin_wchar_t *, size_t, const char *, size_t, __darwin_mbstate_t *, locale_t);
int	__collate_equiv_class(const char *, size_t, __darwin_mbstate_t *, locale_t);
size_t	__collate_equiv_match(int, __darwin_wchar_t *, size_t, __darwin_wchar_t, const char *, size_t, __darwin_mbstate_t *, size_t *, locale_t);
#else /* !__LIBC__ */
void	__collate_lookup(const unsigned char *, int *, int *, int *);
#endif /* __LIBC__ */
#ifdef COLLATE_DEBUG
void	__collate_print_tables(void);
#endif
__END_DECLS

#endif /* !_COLLATE_H_ */
