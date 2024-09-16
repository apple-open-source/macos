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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/locale/collate.c,v 1.35 2005/02/27 20:31:13 ru Exp $");

#include "xlocale_private.h"

#include "namespace.h"

#include <sys/stat.h>
#include <sys/mman.h>

#include <arpa/inet.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>
#include <ctype.h>
#include "un-namespace.h"

#include "collate.h"
#include "setlocale.h"
#include "ldpart.h"

#include "libc_private.h"

/* assumes the locale_t variable is named loc */
#define collate_chain_pri_table	(XLOCALE_COLLATE(loc)->chain_pri_table)
#define collate_char_pri_table	(XLOCALE_COLLATE(loc)->char_pri_table)
#define collate_info		(XLOCALE_COLLATE(loc)->info)
#define collate_large_pri_table	(XLOCALE_COLLATE(loc)->large_pri_table)
#define collate_subst_table	(XLOCALE_COLLATE(loc)->subst_table)

#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
static void wntohl(wchar_t *, int);
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */
void __collate_err(int ex, const char *f) __dead2;

/*
 * Normally, the __collate_* routines should all be __private_extern__,
 * but grep is using them (3715846).  Until we can provide an alternative,
 * we leave them public, and provide a read-only __collate_load_error variable
 */
#undef __collate_load_error
int __collate_load_error = 1;

struct xlocale_collate __xlocale_C_collate = {
	{ { 1, NULL }, "C", }, 1, NULL, 0, NULL, { NULL }, NULL, NULL, NULL
};

static void
destruct_collate(void *t)
{
	struct xlocale_collate *table = t;

	/*
	 * With the modern collation format, table->info points into the mmap'd
	 * region; in the legacy format, table->info points into a separately
	 * allocated region since we have to do some translations into the
	 * current day internal representation.
	 */
	if ((table->info->flags & COLLATE_LEGACY) != 0) {
		free(table->info);
	}

	if (table->map && (table->maplen > 0)) {
		(void) munmap(table->map, table->maplen);
	}

	free(table);
}

static void
__collate_fill_info(struct xlocale_collate *TMP,
    collate_legacy_info_t *info)
{
	collate_info_t *dinfo = TMP->info;

	/*
	 * Some fields may be larger in the new data structure; zero it to take
	 * care of the difference.  Sprinkle in some assertions to make sure
	 * we're not writing off the end of these arrays, though
	 * COLL_WEIGHTS_MAX should only go up from here.
	 */
	memset(dinfo, 0, sizeof(*dinfo));
	assert(sizeof(info->directive) <= sizeof(dinfo->directive));
	assert(sizeof(info->subst_count) <= sizeof(dinfo->subst_count));
	assert(sizeof(info->undef_pri) <= sizeof(dinfo->undef_pri));

	/* Copy plain scalar bits first */
	dinfo->directive_count = info->directive_count;
	dinfo->chain_max_len = info->chain_max_len;
	dinfo->flags = info->flags | COLLATE_LEGACY;
	dinfo->chain_count = info->chain_count;
	dinfo->large_count = info->large_pri_count;

	/* Now fill in arrays */
	memcpy(dinfo->directive, info->directive, sizeof(info->directive));
	/* No analog for dinfo->pri_count */
	memcpy(dinfo->subst_count, info->subst_count,
	    sizeof(info->subst_count));
	memcpy(dinfo->undef_pri, info->undef_pri, sizeof(info->undef_pri));
}

static int
__collate_load_tables_legacy(const char *encoding, locale_t loc, char *TMP,
    char *map, struct stat *sbuf, struct xlocale_collate **cachep)
{
	collate_legacy_info_t *info;
	struct xlocale_collate *table;
	collate_legacy_char_t *lchar_table;
	collate_legacy_chain_t *lchain_table;
	collate_legacy_large_t *llarge_table;
	collate_legacy_subst_t *lsubst_table[2];
	char *XTMP;
	int i, chains, z;

	TMP += LEGACY_COLLATE_STR_LEN;

	info = (void *)TMP;
	TMP += sizeof(*info);

#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
	for(z = 0; z < info->directive_count; z++) {
		info->undef_pri[z] = ntohl(info->undef_pri[z]);
		info->subst_count[z] = ntohl(info->subst_count[z]);
	}
	info->chain_count = ntohl(info->chain_count);
	info->large_pri_count = ntohl(info->large_pri_count);
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */
	if ((chains = info->chain_count) < 0) {
		(void) munmap(map, sbuf->st_size);
		/* XXX FreeBSD returns EINVAL here */
		errno = EFTYPE;
		return (_LDP_ERROR);
	}

	i = (sizeof (collate_legacy_char_t) * (UCHAR_MAX + 1)) +
	    (sizeof (collate_legacy_chain_t) * chains) +
	    (sizeof (collate_legacy_large_t) * info->large_pri_count);
	for(z = 0; z < info->directive_count; z++) {
		i += sizeof (collate_legacy_subst_t) * info->subst_count[z];
	}
	if (i != (sbuf->st_size - (TMP - map))) {
		(void) munmap(map, sbuf->st_size);
		errno = EINVAL;
		return (_LDP_ERROR);
	}

	if ((table = malloc(sizeof(*table))) == NULL) {
		(void) munmap(map, sbuf->st_size);
		errno = ENOMEM;
		return (_LDP_ERROR);
	}

	table->map = map;
	table->maplen = sbuf->st_size;

	/*
	 * The legacy format has a lower COLL_WEIGHTS_MAX, so we can't map it in
	 * directly.  We could alter the logic at every point that uses these
	 * tables to check if it's a legacy info for these that are arrays of
	 * elements, but instead we choose to take a modest hit in memory usage
	 * for legacy locales rather than sprinkling around more code to audit.
	 */
	i = sizeof(*table->info) + (sizeof (collate_char_t) * (UCHAR_MAX + 1)) +
	    (sizeof (collate_chain_t) * chains) +
	    (sizeof (collate_large_t) * info->large_pri_count);
	for(z = 0; z < info->directive_count; z++) {
		i += sizeof(collate_subst_t) * info->subst_count[z];
	}
	if ((table->info = calloc(1, i)) == NULL) {
		(void) munmap(map, sbuf->st_size);
		free(table);
		errno = ENOMEM;
		return (_LDP_ERROR);
	}

	XTMP = (void *)(table->info + 1);

	/* one for the locale, one for the cache */
	table->header.header.retain_count = 2;
	table->header.header.destructor = destruct_collate;

	lchar_table = (void *)TMP;
	table->char_pri_table = (void *)XTMP;

	TMP += sizeof (collate_legacy_char_t) * (UCHAR_MAX + 1);
	XTMP += sizeof (collate_char_t) * (UCHAR_MAX + 1);

	/* the COLLATE_SUBST_DUP optimization relies on COLL_WEIGHTS_MAX == 2 */
	if (info->subst_count[0] > 0) {
		lsubst_table[0] = (void *)TMP;
		table->subst_table[0] = (collate_subst_t *)XTMP;
		TMP += info->subst_count[0] * sizeof(collate_legacy_subst_t);
		XTMP += info->subst_count[0] * sizeof(collate_subst_t);
	} else
		table->subst_table[0] = NULL;
	if (info->flags & COLLATE_SUBST_DUP)
		table->subst_table[1] = table->subst_table[0];
	else if (info->subst_count[1] > 0) {
		lsubst_table[1] = (void *)TMP;
		table->subst_table[1] = (collate_subst_t *)XTMP;
		TMP += info->subst_count[1] * sizeof(collate_legacy_subst_t);
		XTMP += info->subst_count[1] * sizeof(collate_subst_t);
	} else
		table->subst_table[1] = NULL;

	if (chains > 0) {
		lchain_table = (void *)TMP;
		table->chain_pri_table = (collate_chain_t *)XTMP;
		TMP += chains * sizeof(collate_legacy_chain_t);
		XTMP += chains * sizeof(collate_chain_t);
	} else {
		lchain_table = NULL;
		table->chain_pri_table = NULL;
	}

	if (info->large_pri_count > 0) {
		llarge_table = (void *)TMP;
		table->large_pri_table = (collate_large_t *)XTMP;
	} else {
		llarge_table = NULL;
		table->large_pri_table = NULL;
	}

	{
		collate_char_t *p = table->char_pri_table;
		collate_legacy_char_t *lp = lchar_table;
		for(i = UCHAR_MAX + 1; i-- > 0; p++, lp++) {
			for(z = 0; z < info->directive_count; z++)
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
				p->pri[z] = ntohl(lp->pri[z]);
#else
				p->pri[z] = lp->pri[z];
#endif
		}
	}

	for(z = 0; z < info->directive_count; z++) {
		if (info->subst_count[z] > 0) {
			collate_legacy_subst_t *lp = lsubst_table[z];
			collate_subst_t *p = table->subst_table[z];
			for(i = info->subst_count[z]; i-- > 0; lp++, p++) {
				memcpy(&p->str[0], &lp->str[0],
				    sizeof(lp->str));
				p->val = lp->val;
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
				p->val = ntohl(p->val);
				wntohl(p->str, LEGACY_COLLATE_STR_LEN);
			}
		}
#endif
	}

	{
		collate_chain_t *p = table->chain_pri_table;
		collate_legacy_chain_t *lp = lchain_table;
		for(i = chains; i-- > 0; p++, lp++) {
			memcpy(&p->str[0], &lp->str[0], sizeof(lp->str));
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
			wntohl(p->str, LEGACY_COLLATE_STR_LEN);
#endif
			for(z = 0; z < info->directive_count; z++)
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
				p->pri[z] = ntohl(lp->pri[z]);
#else
				p->pri[z] = lp->pri[z];
#endif
		}
	}
	if (info->large_pri_count > 0) {
		collate_large_t *p = table->large_pri_table;
		collate_legacy_large_t *lp = llarge_table;
		for(i = info->large_pri_count; i-- > 0; p++, lp++) {
			p->val = lp->val;
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
			p->val = ntohl(p->val);
#endif
			for(z = 0; z < info->directive_count; z++)
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
				p->pri.pri[z] = ntohl(lp->pri.pri[z]);
#else
				p->pri.pri[z] = lp->pri.pri[z];
#endif
		}
	}
	(void)strcpy(table->header.locale, encoding);
	__collate_fill_info(table, info);
	xlocale_release(*cachep);
	*cachep = table;
	xlocale_release(loc->components[XLC_COLLATE]);
	loc->components[XLC_COLLATE] = (void *)table;
	/* no need to retain, since we set retain_count to 2 above */

	table->__collate_load_error = 0;
	if (loc == &__global_locale)
		__collate_load_error = 0;

	return (_LDP_LOADED);
}

__private_extern__ int
__collate_load_tables(const char *encoding, locale_t loc)
{
	int fd;
	int i, chains, z;
	char buf[PATH_MAX];
	struct xlocale_collate *table;
	char *TMP;
	char *map;
	static struct xlocale_collate *cache = NULL;
	collate_info_t *info;
	struct stat sbuf;

	/* 'encoding' must be already checked. */
	if (strcmp(encoding, "C") == 0 || strcmp(encoding, "POSIX") == 0 ||
	    strncmp(encoding, "C.", 2) == 0) {
		if (loc == &__global_locale)
			__collate_load_error = 1;
		xlocale_release(loc->components[XLC_COLLATE]);
		loc->components[XLC_COLLATE] = (void *)&__xlocale_C_collate;
		xlocale_retain(&__xlocale_C_collate);
		return (_LDP_CACHE);
	}

	/*
	 * If the locale name is the same as our cache, use the cache.
	 */
	if (cache && strcmp(encoding, cache->header.locale) == 0) {
		if (loc == &__global_locale)
			__collate_load_error = 0;
		xlocale_release(loc->components[XLC_COLLATE]);
		loc->components[XLC_COLLATE] = (void *)cache;
		xlocale_retain(cache);
		return (_LDP_CACHE);
	}

	/*
	 * Slurp the locale file into the cache.
	 */

	/* 'PathLocale' must be already set & checked. */
	/* Range checking not needed, encoding has fixed size */
	(void)strcpy(buf, encoding);
	(void)strcat(buf, "/LC_COLLATE");
	fd = __open_path_locale(buf);
	if (fd == -1) {
		return (_LDP_ERROR);
	}

	if (_fstat(fd, &sbuf) < 0) {
		(void) _close(fd);
		return (_LDP_ERROR);
	}

	if (sbuf.st_size < COLLATE_STR_LEN + sizeof(*info)) {
		(void) _close(fd);
		errno = EINVAL;
		return (_LDP_ERROR);
	}

	/*
	 * Legacy needs PROT_WRITE for byteswapping, the new format will not.
	 */
	map = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
	    0);
	(void) _close(fd);
	if ((TMP = map) == MAP_FAILED) {
		return (_LDP_ERROR);
	}

	chains = 1;
	if (strncmp(TMP, COLLATE_VERSION1_1A, COLLATE_FMT_VERSION_LEN) == 0) {
		return (__collate_load_tables_legacy(encoding, loc, TMP, map,
		    &sbuf, &cache));
	}

	if (strncmp(TMP, COLLATE_FMT_VERSION, COLLATE_FMT_VERSION_LEN) != 0) {
		(void) munmap(map, sbuf.st_size);
		errno = EFTYPE;
		return (_LDP_ERROR);
	}

	TMP += COLLATE_FMT_VERSION_LEN;
	/* XXX Grab header version */
	TMP += XLOCALE_DEF_VERSION_LEN;

	info = (void *)TMP;
	TMP += sizeof(*info);

	if ((chains = info->chain_count) < 0) {
		(void) munmap(map, sbuf.st_size);
		/* XXX FreeBSD returns EINVAL here */
		errno = EFTYPE;
		return (_LDP_ERROR);
	}

	/*
	 * We relied on COLL_WEIGHT_MAX == 2 for this; the flag doesn't make any
	 * sense in the new format.
	 */
	if (info->flags & COLLATE_SUBST_DUP) {
		(void) munmap(map, sbuf.st_size);
		errno = EINVAL;
		return (_LDP_ERROR);
	}

	i = (sizeof (collate_char_t) * (UCHAR_MAX + 1)) +
	    (sizeof (collate_chain_t) * chains) +
	    (sizeof (collate_large_t) * info->large_count);
	for(z = 0; z < info->directive_count; z++) {
		i += sizeof (collate_subst_t) * info->subst_count[z];
	}
	if (i != (sbuf.st_size - (TMP - map))) {
		(void) munmap(map, sbuf.st_size);
		errno = EINVAL;
		return (_LDP_ERROR);
	}

	if ((table = malloc(sizeof(*table))) == NULL) {
		(void) munmap(map, sbuf.st_size);
		errno = ENOMEM;
		return (_LDP_ERROR);
	}

	table->map = map;
	table->maplen = sbuf.st_size;

	/* one for the locale, one for the cache */
	table->header.header.retain_count = 2;
	table->header.header.destructor = destruct_collate;

	table->char_pri_table = (void *)TMP;
	TMP += sizeof (collate_char_t) * (UCHAR_MAX + 1);

	for(z = 0; z < info->directive_count; z++) {
		if (info->subst_count[z] > 0) {
			table->subst_table[z] = (collate_subst_t *)TMP;
			TMP += info->subst_count[z] * sizeof (collate_subst_t);
		} else {
			table->subst_table[z] = NULL;
		}
	}

	if (chains > 0) {
		table->chain_pri_table = (collate_chain_t *)TMP;
		TMP += chains * sizeof(collate_chain_t);
	} else
		table->chain_pri_table = NULL;
	if (info->large_count > 0)
		table->large_pri_table = (collate_large_t *)TMP;
	else
		table->large_pri_table = NULL;

	(void)strcpy(table->header.locale, encoding);
	table->info = info;
	xlocale_release(cache);
	cache = table;
	xlocale_release(loc->components[XLC_COLLATE]);
	loc->components[XLC_COLLATE] = (void *)cache;
	/* no need to retain, since we set retain_count to 2 above */

	table->__collate_load_error = 0;
	if (loc == &__global_locale)
		__collate_load_error = 0;

	return (_LDP_LOADED);
}

static int
__collate_wcsnlen(const wchar_t *s, int len)
{
	int n = 0;
	while (*s && n < len) {
		s++;
		n++;
	}
	return n;
}

static collate_subst_t *
substsearch(const wchar_t key, collate_subst_t *tab, int n)
{
	int low = 0;
	int high = n - 1;
	int next, compar;
	collate_subst_t *p;

	while (low <= high) {
		next = (low + high) / 2;
		p = tab + next;
		compar = key - p->val;
		if (compar == 0)
			return p;
		if (compar > 0)
			low = next + 1;
		else
			high = next - 1;
	}
	return NULL;
}

__private_extern__ wchar_t *
__collate_substitute(const wchar_t *s, int which, locale_t loc)
{
	int dest_len, len, nlen;
	int n, delta, nsubst;
	wchar_t *dest_str = NULL;
	const wchar_t *fp;
	collate_subst_t *subst, *match;

	if (s == NULL || *s == '\0')
		return (__collate_wcsdup(L""));
	dest_len = wcslen(s);
	nsubst = collate_info->subst_count[which];
	if (nsubst <= 0)
		return __collate_wcsdup(s);
	subst = collate_subst_table[which];
	delta = dest_len / 4;
	if (delta < 2)
		delta = 2;
	dest_str = (wchar_t *)malloc((dest_len += delta) * sizeof(wchar_t));
	if (dest_str == NULL)
		__collate_err(EX_OSERR, __func__);
	len = 0;
	while (*s) {
		if ((match = substsearch(*s, subst, nsubst)) != NULL) {
			fp = match->str;
			n = __collate_wcsnlen(fp, COLLATE_STR_LEN);
		} else {
			fp = s;
			n = 1;
		}
		nlen = len + n;
		if (dest_len <= nlen) {
			dest_str = reallocf(dest_str, (dest_len = nlen + delta) * sizeof(wchar_t));
			if (dest_str == NULL)
				__collate_err(EX_OSERR, __func__);
		}
		wcsncpy(dest_str + len, fp, n);
		len += n;
		s++;
	}
	dest_str[len] = 0;
	return (dest_str);
}

static const int32_t *
lookup_substsearch(struct xlocale_collate *table, const wchar_t key, int pass)
{
	const collate_subst_t *p;
	int n = table->info->subst_count[pass];

	if (n == 0)
		return (NULL);

	if (pass >= table->info->directive_count)
		return (NULL);

	if (!(key & COLLATE_SUBST_PRIORITY))
		return (NULL);

	p = table->subst_table[pass] + (key & ~COLLATE_SUBST_PRIORITY);
	assert(p->val == key);

	return (p->str);
}


static collate_chain_t *
chainsearch(const wchar_t *key, int *len, locale_t loc)
{
	int low = 0;
	int high = collate_info->chain_count - 1;
	int next, compar, l;
	collate_chain_t *p;
	collate_chain_t *tab = collate_chain_pri_table;

	while (low <= high) {
		next = (low + high) / 2;
		p = tab + next;
		compar = *key - *p->str;
		if (compar == 0) {
			l = __collate_wcsnlen(p->str, COLLATE_STR_LEN);
			compar = wcsncmp(key, p->str, l);
			if (compar == 0) {
				*len = l;
				return p;
			}
		}
		if (compar > 0)
			low = next + 1;
		else
			high = next - 1;
	}
	return NULL;
}

static collate_large_t *
largesearch(const wchar_t key, locale_t loc)
{
	int low = 0;
	int high = collate_info->large_count - 1;
	int next, compar;
	collate_large_t *p;
	collate_large_t *tab = collate_large_pri_table;

	while (low <= high) {
		next = (low + high) / 2;
		p = tab + next;
		compar = key - p->val;
		if (compar == 0)
			return p;
		if (compar > 0)
			low = next + 1;
		else
			high = next - 1;
	}
	return NULL;
}

/*
* This is provided for programs (like grep) that are calling this
* private function.  This is also used by wcscoll()
*/
void
__collate_lookup_l(const wchar_t *t, int *len, int *prim, int *sec, locale_t loc)
{
	collate_chain_t *p2;
	collate_large_t *match;
	struct xlocale_collate *table;
	const int *sptr;
	int l;

	if (!*t) {
		*len = 0;
		*prim = 0;
		*sec = 0;
		return;
	}

	NORMALIZE_LOCALE(loc);
	table = XLOCALE_COLLATE(loc);
	if (table->__collate_load_error) {
		*len = 1;
		*prim = *t;
		*sec = 0;
		return;
	}

	/* No active substitutions */
	*len = 1;

	/*
	 * Check for composites such as diphthongs that collate as a
	 * single element (aka chains or collating-elements).
	 */
	if (((p2 = chainsearch(t, &l, loc)) != NULL &&
	    p2->pri[0] >= 0)) {

		*len = l;
		*prim = p2->pri[0];
		*sec = p2->pri[1];

	} else if (*t <= UCHAR_MAX) {

		/*
		 * Character is a small (8-bit) character.
		 * We just look these up directly for speed.
		 */
		*prim = collate_char_pri_table[*t].pri[0];
		*sec = collate_char_pri_table[*t].pri[1];

	} else if (collate_info->large_count > 0 &&
	    ((match = largesearch(*t, loc)) != NULL)) {

		/*
		 * Character was found in the extended table.
		 */
		*prim = match->pri.pri[0];
		*sec = match->pri.pri[1];

	} else {
		/*
		 * Character lacks a specific definition.
		 */
		*prim = (l = collate_info->undef_pri[0]) >= 0 ? l : *t - l;
		*sec = (l = collate_info->undef_pri[1]) >= 0 ? l : *t - l;

		/* No substitutions for undefined characters! */
		return;
	}

	/*
	 * Try substituting (expanding) the character.  We are
	 * currently doing this *after* the chain compression.  I
	 * think it should not matter, but this way might be slightly
	 * faster.
	 *
	 * We do this after the priority search, as this will help us
	 * to identify a single key value.  In order for this to work,
	 * its important that the priority assigned to a given element
	 * to be substituted be unique for that level.  The localedef
	 * code ensures this for us.
	 */
	if (*prim >= 0 && (sptr = lookup_substsearch(table, *prim, 0)) != NULL) {
#ifdef __APPLE__
		*prim = *sptr;
#else
		if ((*pri = *sptr) > 0) {
			sptr++;
			*state = *sptr ? sptr : NULL;
		}
#endif
	}
	if (*sec >= 0 && (sptr = lookup_substsearch(table, *sec, 1)) != NULL) {
#ifdef __APPLE__
		*sec = sptr[1];
#else
		if ((*pri = *sptr) > 0) {
			sptr++;
			*state = *sptr ? sptr : NULL;
		}
#endif
	}
}

/*
 * This is also provided for programs (like grep) that are calling this
 * private function - that do not perform their own multi-byte handling.
 * This will go away eventually.
 */
void
__collate_lookup(const unsigned char *t, int *len, int *prim, int *sec)
{
	locale_t loc = __current_locale();
	wchar_t *w = NULL;
	int sverrno;

	if (!*t) {
		*len = 0;
		*prim = 0;
		*sec = 0;
		return;
	}

	if (XLOCALE_COLLATE(loc)->__collate_load_error ||
	    (w = __collate_mbstowcs((const char *)t, loc)) == NULL) {
		*len = 1;
		*prim = (int)*t;
		*sec = 0;

		sverrno = errno;
		free((void*)w);
		errno = sverrno;
		return;
	}

	__collate_lookup_l(w, len, prim, sec, loc);
	sverrno = errno;
	free(w);
	errno = sverrno;
}

__private_extern__ void
__collate_lookup_which(const wchar_t *t, int *len, int *pri, int which, locale_t loc)
{
	collate_chain_t *p2;
	collate_large_t *match;
	const int *sptr;
	struct xlocale_collate *table;
	int p, l;

	table = XLOCALE_COLLATE(loc);

	/* No active substitutions */
	*len = 1;

	if (((p2 = chainsearch(t, &l, loc)) != NULL &&
	    (p = p2->pri[which]) >= 0)) {

		*len = l;
		*pri = p;

	} else if (*t <= UCHAR_MAX) {

		/*
		 * Character is a small (8-bit) character.
		 * We just look these up directly for speed.
		 */
		*pri = collate_char_pri_table[*t].pri[which];

	} else if ((collate_info->large_count) > 0 &&
	    (match = largesearch(*t, loc)) != NULL) {

		/*
		 * Character was found in the extended table.
		 */
		*pri = match->pri.pri[which];

	} else {
		/*
		 * Character lacks a specific definitino.
		 */
		if (collate_info->directive[which] & DIRECTIVE_UNDEFINED) {
			*pri = (*t & COLLATE_MAX_PRIORITY);
		} else {
			*pri = collate_info->undef_pri[which];
		}
		/* No substitutions for undefined characters! */
		return;
	}

	/*
	 * Try substituting (expanding) the character.  We are
	 * currently doing this *after* the chain compression.  I
	 * think it should not matter, but this way might be slightly
	 * faster.
	 *
	 * We do this after the priority search, as this will help us
	 * to identify a single key value.  In order for this to work,
	 * its important that the priority assigned to a given element
	 * to be substituted be unique for that level.  The localedef
	 * code ensures this for us.
	 */
	if ((sptr = lookup_substsearch(table, *pri, 0)) != NULL) {
#ifdef __APPLE__
		*pri = *sptr;
#else
		if ((*pri = *sptr) > 0) {
			sptr++;
			*state = *sptr ? sptr : NULL;
		}
#endif
	}
}

__private_extern__ wchar_t *
__collate_mbstowcs(const char *s, locale_t loc)
{
	static const mbstate_t initial;
	mbstate_t st;
	size_t len;
	const char *ss;
	wchar_t *wcs;

	ss = s;
	st = initial;
	if ((len = mbsrtowcs_l(NULL, &ss, 0, &st, loc)) == (size_t)-1)
		return NULL;
	if ((wcs = (wchar_t *)malloc((len + 1) * sizeof(wchar_t))) == NULL)
		__collate_err(EX_OSERR, __func__);
	st = initial;
	mbsrtowcs_l(wcs, &s, len, &st, loc);
	wcs[len] = 0;

	return (wcs);
}

__private_extern__ wchar_t *
__collate_wcsdup(const wchar_t *s)
{
	size_t len = wcslen(s) + 1;
	wchar_t *wcs;

	if ((wcs = (wchar_t *)malloc(len * sizeof(wchar_t))) == NULL)
		__collate_err(EX_OSERR, __func__);
	wcscpy(wcs, s);
	return (wcs);
}

__private_extern__ void
__collate_xfrm(const wchar_t *src, wchar_t **xf, locale_t loc)
{
	int pri, len;
	size_t slen;
	const wchar_t *t;
	wchar_t *tt = NULL, *tr = NULL;
	int direc, pass;
	wchar_t *xfp;
	collate_info_t *info = collate_info;
	int sverrno;

	for(pass = 0; pass < COLL_WEIGHTS_MAX; pass++)
		xf[pass] = NULL;
	for(pass = 0; pass < info->directive_count; pass++) {
		direc = info->directive[pass];
		if (pass == 0 || !(info->flags & COLLATE_SUBST_DUP)) {
			sverrno = errno;
			free(tt);
			errno = sverrno;
			tt = __collate_substitute(src, pass, loc);
		}
		if (direc & DIRECTIVE_BACKWARD) {
			wchar_t *bp, *fp, c;
			sverrno = errno;
			free(tr);
			errno = sverrno;
			tr = __collate_wcsdup(tt ? tt : src);
			bp = tr;
			fp = tr + wcslen(tr) - 1;
			while(bp < fp) {
				c = *bp;
				*bp++ = *fp;
				*fp-- = c;
			}
			t = (const wchar_t *)tr;
		} else if (tt)
			t = (const wchar_t *)tt;
		else
			t = (const wchar_t *)src;
		sverrno = errno;
		if ((xf[pass] = (wchar_t *)malloc(sizeof(wchar_t) * (wcslen(t) + 1))) == NULL) {
			errno = sverrno;
			slen = 0;
			goto end;
		}
		errno = sverrno;
		xfp = xf[pass];
		if (direc & DIRECTIVE_POSITION) {
			while(*t) {
				__collate_lookup_which(t, &len, &pri, pass, loc);
				t += len;
				if (pri <= 0) {
					if (pri < 0) {
						errno = EINVAL;
						slen = 0;
						goto end;
					}
					pri = COLLATE_MAX_PRIORITY;
				}
				*xfp++ = pri;
			}
		} else {
			while(*t) {
				__collate_lookup_which(t, &len, &pri, pass, loc);
				t += len;
				if (pri <= 0) {
					if (pri < 0) {
						errno = EINVAL;
						slen = 0;
						goto end;
					}
					continue;
				}
				*xfp++ = pri;
			}
 		}
		*xfp = 0;
	}
  end:
	sverrno = errno;
	free(tt);
	free(tr);
	errno = sverrno;
}

__private_extern__ void
__collate_err(int ex, const char *f)
{
	const char *s;
	int serrno = errno;

	s = _getprogname();
	_write(STDERR_FILENO, s, strlen(s));
	_write(STDERR_FILENO, ": ", 2);
	s = f;
	_write(STDERR_FILENO, s, strlen(s));
	_write(STDERR_FILENO, ": ", 2);
	s = strerror(serrno);
	_write(STDERR_FILENO, s, strlen(s));
	_write(STDERR_FILENO, "\n", 1);
	exit(ex);
}

/*
 * __collate_collating_symbol takes the multibyte string specified by
 * src and slen, and using ps, converts that to a wide character.  Then
 * it is checked to verify it is a collating symbol, and then copies
 * it to the wide character string specified by dst and dlen (the
 * results are not null terminated).  The length of the wide characters
 * copied to dst is returned if successful.  Zero is returned if no such
 * collating symbol exists.  (size_t)-1 is returned if there are wide-character
 * conversion errors, if the length of the converted string is greater that
 * COLLATE_STR_LEN or if dlen is too small.  It is up to the calling routine to
 * preserve the mbstate_t structure as needed.
 */
__private_extern__ size_t
__collate_collating_symbol(wchar_t *dst, size_t dlen, const char *src, size_t slen, mbstate_t *ps, locale_t loc)
{
	wchar_t wname[COLLATE_STR_LEN];
	wchar_t w, *wp;
	size_t len, l;

	/* POSIX locale */
	if (XLOCALE_COLLATE(loc)->__collate_load_error) {
		if (dlen < 1)
			return (size_t)-1;
		if (slen != 1 || !isascii(*src))
			return 0;
		*dst = *src;
		return 1;
	}
	for(wp = wname, len = 0; slen > 0; len++) {
		l = mbrtowc_l(&w, src, slen, ps, loc);
		if (l == (size_t)-1 || l == (size_t)-2)
			return (size_t)-1;
		if (l == 0)
			break;
		if (len >= COLLATE_STR_LEN)
			return -1;
		*wp++ = w;
		src += l;
		slen = (long)slen - (long)l;
	}
	if (len == 0 || len > dlen)
		return (size_t)-1;
	if (len == 1) {
		if (*wname <= UCHAR_MAX) {
			if (collate_char_pri_table[*wname].pri[0] >= 0) {
				if (dlen > 0)
					*dst = *wname;
				return 1;
			}
			return 0;
		} else if (collate_info->large_count > 0) {
			collate_large_t *match;
			match = largesearch(*wname, loc);
			if (match && match->pri.pri[0] >= 0) {
				if (dlen > 0)
					*dst = *wname;
				return 1;
			}
		}
		return 0;
	}
	*wp = 0;
	if (collate_info->chain_count > 0) {
		collate_chain_t *match;
		int ll;
		match = chainsearch(wname, &ll, loc);
		if (match) {
			if (ll < dlen)
				dlen = ll;
			wcsncpy(dst, wname, dlen);
			return ll;
		}
	}
	return 0;
}

/*
 * __collate_equiv_class returns the equivalence class number for the symbol
 * specified by src and slen, using ps to convert from multi-byte to wide
 * character.  Zero is returned if the symbol is not in an equivalence
 * class.  -1 is returned if there are wide character conversion error,
 * if there are any greater-than-8-bit characters or if a multi-byte symbol
 * is greater or equal to COLLATE_STR_LEN in length.  It is up to the calling
 * routine to preserve the mbstate_t structure as needed.
 */
__private_extern__ int
__collate_equiv_class(const char *src, size_t slen, mbstate_t *ps, locale_t loc)
{
	wchar_t wname[COLLATE_STR_LEN];
	wchar_t w, *wp;
	size_t len, l;
	int e;

	/* POSIX locale */
	if (XLOCALE_COLLATE(loc)->__collate_load_error)
		return 0;
	for(wp = wname, len = 0; slen > 0; len++) {
		l = mbrtowc_l(&w, src, slen, ps, loc);
		if (l == (size_t)-1 || l == (size_t)-2)
			return -1;
		if (l == 0)
			break;
		if (len >= COLLATE_STR_LEN)
			return -1;
		*wp++ = w;
		src += l;
		slen = (long)slen - (long)l;
	}
	if (len == 0)
		return -1;
	if (len == 1) {
		e = -1;
		if (*wname <= UCHAR_MAX)
			e = collate_char_pri_table[*wname].pri[0];
		else if (collate_info->large_count > 0) {
			collate_large_t *match;
			match = largesearch(*wname, loc);
			if (match)
				e = match->pri.pri[0];
		}
		if (e == 0)
			return IGNORE_EQUIV_CLASS;
		return e > 0 ? e : 0;
	}
	*wp = 0;
	if (collate_info->chain_count > 0) {
		collate_chain_t *match;
		int ll;
		match = chainsearch(wname, &ll, loc);
		if (match) {
			e = match->pri[0];
			if (e == 0)
				return IGNORE_EQUIV_CLASS;
			return e < 0 ? -e : e;
		}
	}
	return 0;
}

/*
 * __collate_equiv_match tries to match any single or multi-character symbol
 * in equivalence class equiv_class in the multi-byte string specified by src
 * and slen.  If start is non-zero, it is taken to be the first (pre-converted)
 * wide character.  Subsequence wide characters, if needed, will use ps in
 * the conversion.  On a successful match, the length of the matched string
 * is returned (including the start character).  If dst is non-NULL, the
 * matched wide-character string is copied to dst, a wide character array of
 * length dlen (the results are not zero-terminated).  If rlen is non-NULL,
 * the number of character in src actually used is returned.  Zero is
 * returned by __collate_equiv_match if there is no match.  (size_t)-1 is
 * returned on error: if there were conversion errors or if dlen is too small
 * to accept the results.  On no match or error, ps is restored to its incoming
 * state.
 */
size_t
__collate_equiv_match(int equiv_class, wchar_t *dst, size_t dlen, wchar_t start, const char *src, size_t slen, mbstate_t *ps, size_t *rlen, locale_t loc)
{
	wchar_t w;
	size_t len, l, clen;
	int i;
	wchar_t buf[COLLATE_STR_LEN], *wp;
	mbstate_t save;
	const char *s = src;
	size_t sl = slen;
	collate_chain_t *ch = NULL;

	/* POSIX locale */
	if (XLOCALE_COLLATE(loc)->__collate_load_error)
		return (size_t)-1;
	if (equiv_class == IGNORE_EQUIV_CLASS)
		equiv_class = 0;
	if (ps)
		save = *ps;
	wp = buf;
	len = clen = 0;
	if (start) {
		*wp++ = start;
		len = 1;
	}
	/* convert up to the max chain length */
	while(sl > 0 && len < collate_info->chain_max_len) {
		l = mbrtowc_l(&w, s, sl, ps, loc);
		if (l == (size_t)-1 || l == (size_t)-2 || l == 0)
			break;
		*wp++ = w;
		s += l;
		clen += l;
		sl -= l;
		len++;
	}
	*wp = 0;
	if (len > 1 && (ch = chainsearch(buf, &i, loc)) != NULL) {
		int e = ch->pri[0];
		if (e < 0)
			e = -e;
		if (e == equiv_class)
			goto found;
	}
	/* try single character */
	i = 1;
	if (*buf <= UCHAR_MAX) {
		if (equiv_class == collate_char_pri_table[*buf].pri[0])
			goto found;
	} else if (collate_info->large_count > 0) {
		collate_large_t *match;
		match = largesearch(*buf, loc);
		if (match && equiv_class == match->pri.pri[0])
			goto found;
	}
	/* no match */
	if (ps)
		*ps = save;
	return 0;
found:
	/* if we converted more than we used, restore to initial and reconvert
	 * up to what did match */
	if (i < len) {
		len = i;
		if (ps)
			*ps = save;
		if (start)
			i--;
		clen = 0;
		while(i-- > 0) {
			l = mbrtowc_l(&w, src, slen, ps, loc);
			src += l;
			clen += l;
			slen -= l;
		}
	}
	if (dst) {
		if (dlen < len) {
			if (ps)
				*ps = save;
			return (size_t)-1;
		}
		for(wp = buf; len > 0; len--)
		    *dst++ = *wp++;
	}
	if (rlen)
		*rlen = clen;
	return len;
}

/*
 * __collate_equiv_value returns the primary collation value for the given
 * collating symbol specified by str and len.  Zero or negative is return
 * if the collating symbol was not found.  (Use by the bracket code in TRE.)
 */
__private_extern__ int
__collate_equiv_value(locale_t loc, const wchar_t *str, size_t len)
{
	int e;

	if (len < 1 || len >= COLLATE_STR_LEN)
		return -1;

	/* POSIX locale */
	if (XLOCALE_COLLATE(loc)->__collate_load_error)
		return (len == 1 && *str <= UCHAR_MAX) ? *str : -1;

	if (len == 1) {
		e = -1;
		if (*str <= UCHAR_MAX)
			e = collate_char_pri_table[*str].pri[0];
		else if (collate_info->large_count > 0) {
			collate_large_t *match;
			match = largesearch(*str, loc);
			if (match)
				e = match->pri.pri[0];
		}
		if (e == 0)
			return IGNORE_EQUIV_CLASS;
		return e > 0 ? e : 0;
	}
	if (collate_info->chain_count > 0) {
		wchar_t name[COLLATE_STR_LEN];
		collate_chain_t *match;
		int ll;

		wcsncpy(name, str, len);
		name[len] = 0;
		match = chainsearch(name, &ll, loc);
		if (match) {
			e = match->pri[0];
			if (e == 0)
				return IGNORE_EQUIV_CLASS;
			return e < 0 ? -e : e;
		}
	}
	return 0;
}

#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
static void
wntohl(wchar_t *str, int len)
{
	for(; *str && len > 0; str++, len--)
		*str = ntohl(*str);
}
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */

#ifdef COLLATE_DEBUG
static char *
show(int c)
{
	static char buf[5];

	if (c >=32 && c <= 126)
		sprintf(buf, "'%c' ", c);
	else
		sprintf(buf, "\\x{%02x}", c);
	return buf;
}

static char *
showwcs(const wchar_t *t, int len)
{
	static char buf[64];
	char *cp = buf;

	for(; *t && len > 0; len--, t++) {
		if (*t >=32 && *t <= 126)
			*cp++ = *t;
		else {
			sprintf(cp, "\\x{%02x}", *t);
			cp += strlen(cp);
		}
	}
	*cp = 0;
	return buf;
}

void
__collate_print_tables()
{
	int i, z;
	locale_t loc = __current_locale();

	printf("Info: p=%d s=%d f=0x%02x m=%d dc=%d up=%d us=%d pc=%d sc=%d cc=%d lc=%d\n",
	    collate_info->directive[0], collate_info->directive[1],
	    collate_info->flags, collate_info->chain_max_len,
	    collate_info->directive_count,
	    collate_info->undef_pri[0], collate_info->undef_pri[1],
	    collate_info->subst_count[0], collate_info->subst_count[1],
	    collate_info->chain_count, collate_info->large_count);
	for(z = 0; z < collate_info->directive_count; z++) {
		if (collate_info->subst_count[z] > 0) {
			collate_subst_t *p2 = collate_subst_table[z];
			if (z == 0 && (collate_info->flags & COLLATE_SUBST_DUP))
				printf("Both substitute tables:\n");
			else
				printf("Substitute table %d:\n", z);
			for (i = collate_info->subst_count[z]; i-- > 0; p2++)
				printf("\t%s --> \"%s\"\n",
					show(p2->val),
					showwcs(p2->str, COLLATE_STR_LEN));
		}
	}
	if (collate_info->chain_count > 0) {
		printf("Chain priority table:\n");
		collate_chain_t *p2 = collate_chain_pri_table;
		for (i = collate_info->chain_count; i-- > 0; p2++) {
			printf("\t\"%s\" :", showwcs(p2->str, COLLATE_STR_LEN));
			for(z = 0; z < collate_info->directive_count; z++)
				printf(" %d", p2->pri[z]);
			putchar('\n');
		}
	}
	printf("Char priority table:\n");
	{
		collate_char_t *p2 = collate_char_pri_table;
		for (i = 0; i < UCHAR_MAX + 1; i++, p2++) {
			printf("\t%s :", show(i));
			for(z = 0; z < collate_info->directive_count; z++)
				printf(" %d", p2->pri[z]);
			putchar('\n');
		}
	}
	if (collate_info->large_count > 0) {
		collate_large_t *p2 = collate_large_pri_table;
		printf("Large priority table:\n");
		for (i = collate_info->large_count; i-- > 0; p2++) {
			printf("\t%s :", show(p2->val));
			for(z = 0; z < collate_info->directive_count; z++)
				printf(" %d", p2->pri.pri[z]);
			putchar('\n');
		}
	}
}
#endif
