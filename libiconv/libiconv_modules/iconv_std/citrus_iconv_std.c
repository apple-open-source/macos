/* $FreeBSD$ */
/*	$NetBSD: citrus_iconv_std.c,v 1.16 2012/02/12 13:51:29 wiz Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c)2003 Citrus Project,
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
#ifdef __APPLE__
#include <sys/param.h>
#else
#include <sys/endian.h>
#endif /* __APPLE__ */
#include <sys/queue.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_module.h"
#include "citrus_region.h"
#include "citrus_mmap.h"
#include "citrus_hash.h"
#include "citrus_iconv.h"
#include "citrus_stdenc.h"
#include "citrus_mapper.h"
#include "citrus_csmapper.h"
#include "citrus_memstream.h"
#include "citrus_iconv_std.h"
#include "citrus_esdb.h"

#ifdef __APPLE__
#ifndef nitems
#define nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif
#endif /* __APPLE__ */

/* ---------------------------------------------------------------------- */

_CITRUS_ICONV_DECLS(iconv_std);
_CITRUS_ICONV_DEF_OPS(iconv_std);


/* ---------------------------------------------------------------------- */

int
_citrus_iconv_std_iconv_getops(struct _citrus_iconv_ops *ops)
{

	memcpy(ops, &_citrus_iconv_std_iconv_ops,
	    sizeof(_citrus_iconv_std_iconv_ops));

	return (0);
}

/* ---------------------------------------------------------------------- */

/*
 * convenience routines for stdenc.
 */
static __inline void
save_encoding_state(struct _citrus_iconv_std_encoding *se)
{

	if (se->se_ps)
		memcpy(se->se_pssaved, se->se_ps,
		    _stdenc_get_state_size(se->se_handle));
}

static __inline void
restore_encoding_state(struct _citrus_iconv_std_encoding *se)
{

	if (se->se_ps)
		memcpy(se->se_ps, se->se_pssaved,
		    _stdenc_get_state_size(se->se_handle));
}

static __inline void
init_encoding_state(struct _citrus_iconv_std_encoding *se)
{

	if (se->se_ps)
		_stdenc_init_state(se->se_handle, se->se_ps);
}

#ifdef __APPLE__
static __inline int
iconv_std_wctomb(struct _citrus_iconv * __restrict cv, unsigned short *delta,
    int cnt, char **s, size_t n, size_t *wcbufsz)
{
	wchar_t *win;
	struct _citrus_iconv_std_context *sc = cv->cv_closure;
	mbstate_t mbstate;
	size_t mbsz, szrin;
	int ret, tmpcnt;

	/*
	 * For conversions from WCHAR_T, we need to convert to
	 * the anticipated multibyte sequence before we can
	 * proceed.  The upper layer set our actual src encoding
	 * to UCS-4-INTERNAL so that everything just works.
	 */
	ret = tmpcnt = 0;
	*wcbufsz = szrin = 0;
	win = (wchar_t *)*s;
	mbstate = sc->sc_mbstate;
	while (n >= sizeof(*win) &&
	    szrin <= sizeof(sc->sc_wcbuf) - MB_CUR_MAX &&
	    tmpcnt < cnt) {
		mbsz = wcrtomb(&sc->sc_wcbuf[szrin], *win, &mbstate);
		if (mbsz == (size_t)-1) {
			if (cv->cv_shared->ci_discard_ilseq) {
				/* Drop it, try again, reset state. */
				memset(&mbstate, 0, sizeof(mbstate));
				goto nextwc;
			} else if (szrin > 0) {
				/*
				 * If we completed anything, go ahead and run
				 * those through the conversion machinery.
				 */
				ret = EILSEQ;
				goto out;
			}

			/* XXX Fallback */
			return (EILSEQ);
		}

		szrin += mbsz;
		sc->sc_mbstate = mbstate;
nextwc:
		/*
		 * delta after a successful iconv_std_wctomb is a reverse lookup
		 * table to get us back to the original input string if we need
		 * it.
		 */
		delta[tmpcnt++] = szrin;

		win++;
		n -= sizeof(*win);
		*s += sizeof(*win);
	}

out:
	*wcbufsz = szrin;
	return (ret);
}

static void
iconv_std_delta_remap(int cnt, int sz, const unsigned short *wcdelta,
    unsigned short *delta)
{
	size_t curmb;
	int i, j;

	for (i = 0; i < cnt; i++) {
		/*
		 * delta[i] describes how many multibyte characters we consumed
		 * for the widechar at position i.
		 *
		 * wcdelta[j] describes how many multibyte characters were
		 * produced by the wchar_t at position j.
		 *
		 * delta and wcdelta should be a roughly 1:1 match, but the
		 * original wchar_t array may have actually had some holes in
		 * it. wcdelta will account for the holes, so we just need to
		 * fast forward past those and map delta[i] back to
		 * (idx + 1) * sizeof(wchar_t).
		 */
		curmb = delta[i];

		assert(wcdelta[i] <= curmb);
		for (j = i; j < sz - 1; j++) {
			if (wcdelta[j + 1] > curmb)
				break;
		}

		assert(wcdelta[j] <= curmb);
		delta[i] = (j + 1) * sizeof(wchar_t);
	}
}
#endif /* __APPLE__ */

static __inline int
#ifdef __APPLE__
mbtocsx(struct _citrus_iconv_std_encoding *se,
    _csid_t *csid, _index_t *idx, unsigned short *delta, int *cnt, char **s,
    size_t n, size_t *nresult, struct iconv_hooks *hooks)
#else
mbtocsx(struct _citrus_iconv_std_encoding *se,
    _csid_t *csid, _index_t *idx, char **s, size_t n, size_t *nresult,
    struct iconv_hooks *hooks)
#endif
{
#ifdef __APPLE__
	int ret;

	ret = _stdenc_mbtocsn(se->se_handle, csid, idx, delta, cnt, s, n,
	    se->se_ps, nresult, hooks, (void (*)(void *))&save_encoding_state,
	    se);

	if (ret == EOPNOTSUPP) {
		size_t accum;
		char *first, *last;
		int i;

		*nresult = 0;
		accum = 0;
		first = *s;
		for (i = 0; i < *cnt && n > 0; i++) {
			save_encoding_state(se);

			last = *s;
			ret = _stdenc_mbtocs(se->se_handle, &csid[i], &idx[i],
			    s, n, se->se_ps, &accum, hooks);
			if (ret != 0)
				break;
			if (accum == (size_t)-2) {
				*nresult = accum;
				break;
			}

			*nresult += accum;
			assert(*s >= last);
			n -= *s - last;
			delta[i] = *s - first;
		}

		if (i < *cnt)
			*cnt = i;
	}

	return (ret);
#else
	return (_stdenc_mbtocs(se->se_handle, csid, idx, s, n, se->se_ps,
			      nresult, hooks));
#endif
}

static __inline int
#ifdef __APPLE__
cstombx(struct _citrus_iconv_std_encoding *se,
    char *s, size_t n, _csid_t *csid, _index_t *idx, int *cnt, size_t *nresult,
    struct iconv_hooks *hooks)
#else
cstombx(struct _citrus_iconv_std_encoding *se,
    char *s, size_t n, _csid_t csid, _index_t idx, size_t *nresult,
    struct iconv_hooks *hooks)
#endif
{
#ifdef __APPLE__
	int ret;

	ret = _stdenc_cstombn(se->se_handle, s, n, csid, idx, cnt, se->se_ps,
	    nresult, hooks, (void (*)(void *))&save_encoding_state, se);
	if (ret == EOPNOTSUPP) {
		size_t acc, tmp;

		acc = 0;
		for (int i = 0; i < *cnt; i++) {
			save_encoding_state(se);
			ret = _stdenc_cstomb(se->se_handle, s, n, csid[i],
			    idx[i], se->se_ps, &tmp, hooks);
			if (ret != 0) {
				/*
				 * Error hit after 'i' characters.
				 */
				*cnt = i;
				break;
			}

			acc += tmp;
			s += tmp;
			n -= tmp;

			if (n == 0 && i < *cnt - 1) {
				/* Truncated */
				*cnt = i + 1;
				break;
			}
		}

		if (ret == 0)
			*nresult = acc;
	}

	return (ret);
#else
	return (_stdenc_cstomb(se->se_handle, s, n, csid, idx, se->se_ps,
			      nresult, hooks));
#endif
}

#ifdef __APPLE__
static __inline int
iconv_std_mbtowc(struct _citrus_iconv * __restrict cv,
    char *s, size_t n, _csid_t *csid, _index_t *idx, int *cnt, size_t *nresult)
{
	const struct _citrus_iconv_std_shared *is = cv->cv_shared->ci_closure;
	struct _citrus_iconv_std_context *sc = cv->cv_closure;
	mbstate_t mbstate;
	wchar_t *wcbuf, wc;
	size_t cur_min, tmpoff, tmpsz, wcsz;
	int cntoff, out, ret, tmpcnt, total;

	/*
	 * We don't need a persistent state because the only state tracked in
	 * mbrtowc() will be for things that we've already accounted for --
	 * unless something bad has happened, cstombx() shouldn't be giving us
	 * incomplete sequences -- we don't use NULL, though, because we don't
	 * want to clobber global state.
	 */
	memset(&mbstate, 0, sizeof(mbstate));
	cur_min = _stdenc_get_mb_cur_min(is->is_dst_encoding);
	assert(cur_min < MB_LEN_MAX);

	ret = 0;
	wcbuf = (wchar_t *)s;
	cntoff = out = 0;
	total = *cnt;
	tmpsz = 0;
	while (cntoff < total) {
		tmpcnt = total - cntoff;
		ret = cstombx(&sc->sc_dst_encoding,
		    &sc->sc_wcbuf[0], sizeof(sc->sc_wcbuf),
		    &csid[cntoff], &idx[cntoff], &tmpcnt,
		    &tmpsz, cv->cv_shared->ci_hooks);

		/* Return with what we've done so far. */
		if (ret != 0 && tmpcnt == 0)
			break;

		/*
		 * Otherwise, wcbuf has tmpsz bytes encoded into it that we need
		 * to convert.  We should be able to get all of them cleanly
		 * out.
		 */
		tmpoff = 0;
		while (tmpsz != 0 && n != 0) {
			size_t consume;

			if (n < sizeof(wchar_t))
				break;

			wcsz = mbrtowc(&wc,
			    &sc->sc_wcbuf[tmpoff], tmpsz, &mbstate);

			assert(wcsz != (size_t)-2);

			if (wcsz == (size_t)-1) {
				ret = EILSEQ;
				goto out;
			}

			assert(wcsz <= tmpsz);

			/*
			 * csmapper sometimes does map some input to NUL bytes,
			 * so we should handle that here.
			 */
			consume = MIN(MAX(wcsz, cur_min), tmpsz);
			tmpsz -= consume;
			tmpoff += consume;

			if (wcsz == 0)
				continue;

			wcbuf[out++] = wc;
			n -= sizeof(wchar_t);
		}

		cntoff += tmpcnt;
	}

out:
	/*
	 * cntoff is how many we actually processed, out is how many wchar_t we
	 * actually sent out (could be different due to, e.g., NUL bytes).
	 */
	*cnt = cntoff;
	if (cntoff > 0)
		*nresult = out * sizeof(wchar_t);

	return (ret);
}
#endif /* __APPLE__ */


static __inline int
wctombx(struct _citrus_iconv_std_encoding *se,
    char *s, size_t n, _wc_t wc, size_t *nresult,
    struct iconv_hooks *hooks)
{

	return (_stdenc_wctomb(se->se_handle, s, n, wc, se->se_ps, nresult,
			     hooks));
}

static __inline int
put_state_resetx(struct _citrus_iconv_std_encoding *se, char *s, size_t n,
    size_t *nresult)
{

	return (_stdenc_put_state_reset(se->se_handle, s, n, se->se_ps, nresult));
}

static __inline int
get_state_desc_gen(struct _citrus_iconv_std_encoding *se, int *rstate)
{
	struct _stdenc_state_desc ssd;
	int ret;

	ret = _stdenc_get_state_desc(se->se_handle, se->se_ps,
	    _STDENC_SDID_GENERIC, &ssd);
	if (!ret)
		*rstate = ssd.u.generic.state;

	return (ret);
}

/*
 * init encoding context
 */
static int
init_encoding(struct _citrus_iconv_std_encoding *se, struct _stdenc *cs,
    void *ps1, void *ps2)
{
	int ret = -1;

	se->se_handle = cs;
	se->se_ps = ps1;
	se->se_pssaved = ps2;

#ifdef __APPLE__
	assert((se->se_ps == NULL && se->se_pssaved == NULL) ||
	    (se->se_ps != NULL && se->se_pssaved != NULL));
#endif
	if (se->se_ps)
		ret = _stdenc_init_state(cs, se->se_ps);
	if (!ret && se->se_pssaved)
#ifdef __APPLE__
		save_encoding_state(se);
#else
		ret = _stdenc_init_state(cs, se->se_pssaved);
#endif

	return (ret);
}

static int
#ifdef __APPLE__
open_csmapper(struct _csmapper **rcm, const char *src, const char *dst,
    unsigned long *rnorm, bool *idmap)
#else
open_csmapper(struct _csmapper **rcm, const char *src, const char *dst,
    unsigned long *rnorm)
#endif
{
	struct _csmapper *cm;
	int ret;

#ifdef __APPLE__
	ret = _csmapper_open(&cm, src, dst, 0, rnorm, idmap);
#else
	ret = _csmapper_open(&cm, src, dst, 0, rnorm);
#endif
	if (ret)
		return (ret);
	if (_csmapper_get_src_max(cm) != 1 || _csmapper_get_dst_max(cm) != 1 ||
	    _csmapper_get_state_size(cm) != 0) {
		_csmapper_close(cm);
		return (EINVAL);
	}

	*rcm = cm;

	return (0);
}

static void
close_dsts(struct _citrus_iconv_std_dst_list *dl)
{
	struct _citrus_iconv_std_dst *sd;

	while ((sd = TAILQ_FIRST(dl)) != NULL) {
		TAILQ_REMOVE(dl, sd, sd_entry);
		_csmapper_close(sd->sd_mapper);
		free(sd);
	}
}

static int
open_dsts(struct _citrus_iconv_std_dst_list *dl,
    const struct _esdb_charset *ec, const struct _esdb *dbdst)
{
	struct _citrus_iconv_std_dst *sd, *sdtmp;
	unsigned long norm;
	int i, ret;
#ifdef __APPLE__
	bool idmap;
#endif

	sd = malloc(sizeof(*sd));
	if (sd == NULL)
		return (errno);

	for (i = 0; i < dbdst->db_num_charsets; i++) {
#ifdef __APPLE__
		ret = open_csmapper(&sd->sd_mapper, ec->ec_csname,
		    dbdst->db_charsets[i].ec_csname, &norm, &idmap);
#else
		ret = open_csmapper(&sd->sd_mapper, ec->ec_csname,
		    dbdst->db_charsets[i].ec_csname, &norm);
#endif
		if (ret == 0) {
			sd->sd_csid = dbdst->db_charsets[i].ec_csid;
			sd->sd_norm = norm;
#ifdef __APPLE__
			sd->sd_idmap = idmap;
#endif
			/* insert this mapper by sorted order. */
			TAILQ_FOREACH(sdtmp, dl, sd_entry) {
				if (sdtmp->sd_norm > norm) {
					TAILQ_INSERT_BEFORE(sdtmp, sd,
					    sd_entry);
					sd = NULL;
					break;
				}
			}
			if (sd)
				TAILQ_INSERT_TAIL(dl, sd, sd_entry);
			sd = malloc(sizeof(*sd));
			if (sd == NULL) {
				ret = errno;
				close_dsts(dl);
				return (ret);
			}
		} else if (ret != ENOENT) {
			close_dsts(dl);
			free(sd);
			return (ret);
		}
	}
	free(sd);
	return (0);
}

static void
close_srcs(struct _citrus_iconv_std_src_list *sl)
{
	struct _citrus_iconv_std_src *ss;

	while ((ss = TAILQ_FIRST(sl)) != NULL) {
		TAILQ_REMOVE(sl, ss, ss_entry);
		close_dsts(&ss->ss_dsts);
		free(ss);
	}
}

static int
#ifdef __APPLE__
open_srcs(struct _citrus_iconv_std_src_list *sl,
    const struct _esdb *dbsrc, const struct _esdb *dbdst, int *ocount)
#else
open_srcs(struct _citrus_iconv_std_src_list *sl,
    const struct _esdb *dbsrc, const struct _esdb *dbdst)
#endif
{
	struct _citrus_iconv_std_src *ss;
	int count = 0, i, ret;

	ss = malloc(sizeof(*ss));
	if (ss == NULL)
		return (errno);

	TAILQ_INIT(&ss->ss_dsts);

	for (i = 0; i < dbsrc->db_num_charsets; i++) {
		ret = open_dsts(&ss->ss_dsts, &dbsrc->db_charsets[i], dbdst);
		if (ret)
			goto err;
		if (!TAILQ_EMPTY(&ss->ss_dsts)) {
			ss->ss_csid = dbsrc->db_charsets[i].ec_csid;
			TAILQ_INSERT_TAIL(sl, ss, ss_entry);
			ss = malloc(sizeof(*ss));
			if (ss == NULL) {
				ret = errno;
				goto err;
			}
			count++;
			TAILQ_INIT(&ss->ss_dsts);
		}
	}
	free(ss);

#ifdef __APPLE__
	if (count && ocount != NULL)
		*ocount = count;
#endif
	return (count ? 0 : ENOENT);

err:
	free(ss);
	close_srcs(sl);
	return (ret);
}

#ifdef __APPLE__
static __inline int
do_conv_map_one(struct _citrus_iconv_std_dst *sd, _csid_t *csid, _index_t *idx,
    int *cnt, _index_t *tentative_entry)
{
	_index_t tmpidx[_ICONV_STD_PERCVT];
	int last, ret;

	if (sd->sd_idmap) {
		/*
		 * With identity mapping (mapper_none), *idx just remains
		 * untouched and we succeed quietly.
		 */
		for (int i = 0; i < *cnt; i++) {
			csid[i] = sd->sd_csid;
		}
		return (0);
	}

	ret = _csmapper_convert(sd->sd_mapper, &tmpidx[0], idx, cnt, NULL);

	/*
	 * *cnt needs to reflect the total including our tentative entry, but
	 * we need to make sure we don't clobber csid or idx for it in case we
	 * have a later destination set with an exact match.
	 */
	last = *cnt;
	if (ret == _MAPPER_CONVERT_TRANSLIT) {
		last--;

		*tentative_entry = tmpidx[last];
		tmpidx[last] = idx[last];
	}

	/*
	 * The mo_convert() implementation may fail part-way through the array,
	 * we should still update csid/idx for the characters that *did*
	 * succeed to match the behavior of the upstream implementation.
	 */
	for (int i = 0; i < last; i++) {
		csid[i] = sd->sd_csid;
		idx[i] = tmpidx[i];
	}

	switch (ret) {
	case _MAPPER_CONVERT_TRANSLIT:
		return (EAGAIN);
	case _MAPPER_CONVERT_SUCCESS:
		return (0);
	case _MAPPER_CONVERT_NONIDENTICAL:
		break;
	case _MAPPER_CONVERT_SRC_MORE:
		/*FALLTHROUGH*/
	case _MAPPER_CONVERT_DST_MORE:
		/*FALLTHROUGH*/
	case _MAPPER_CONVERT_ILSEQ:
		return (EILSEQ);
	case _MAPPER_CONVERT_FATAL:
		return (EINVAL);
	}

	return (ENOENT);
}
#endif

/* do convert a series of characters */
#define E_NO_CORRESPONDING_CHAR ENOENT /* XXX */
static int
/*ARGSUSED*/
#ifdef __APPLE__
do_conv(const struct _citrus_iconv * __restrict cv,
	const struct _citrus_iconv_std_shared *is,
	_csid_t *csid, _index_t *idx, int *cnt, size_t *invalp)
#else
do_conv(const struct _citrus_iconv_std_shared *is,
	_csid_t *csid, _index_t *idx)
#endif
{
	struct _citrus_iconv_std_dst *sd;
	struct _citrus_iconv_std_src *ss;
	_index_t tmpidx;
#ifdef __APPLE__
	int off = 0, tmpcnt = *cnt, total = 0;
#endif
	int ret;

#ifdef __APPLE__
	tmpidx = 0;
	if (is->is_lone_dst != NULL) {
		for (int i = 0; i < tmpcnt; i++) {
			if (csid[i] != is->is_lone_dst_csid) {
				*cnt = i;
				if (i == 0)
					return (E_NO_CORRESPONDING_CHAR);
				break;
			}
		}

		while (total < *cnt) {
			ret = do_conv_map_one(is->is_lone_dst, &csid[off],
			    &idx[off], &tmpcnt, &tmpidx);

			if (ret == 0)
				assert(tmpcnt + total == *cnt);
			else if (ret == EAGAIN)
				assert(tmpcnt > 0);
			else
				assert(tmpcnt + total < *cnt);

			if (ret == EAGAIN && !cv->cv_shared->ci_translit) {
				ret = ENOENT;
				tmpcnt--;
			}

			if (ret != 0 && ret != EAGAIN) {
				*cnt = total + tmpcnt;
				if (ret == ENOENT)
					break;
				return (ret);
			} else if (ret == 0) {
				return (ret);
			} else if (ret == EAGAIN) {
				/* Transliteration */
				total += tmpcnt;
				idx[total - 1] = tmpidx;

				tmpcnt = *cnt - total;
				off += total;
				(*invalp)++;
			}
		}
	} else {
		_csid_t checkid, tmpcsid;
		int elen = 0, len = 0;
		bool tentative;

next:
		if (tmpcnt == 0)
			return (0);

		/*
		 * First grab a contiguous block; in the common case, the whole
		 * block is of the same csid.
		 */
		tmpcsid = checkid = csid[off];
		len = 0;
		for (int i = off; i < off + tmpcnt; i++) {
			if (csid[i] == checkid)
				len++;
			else
				break;
		}

		tentative = false;
		TAILQ_FOREACH(ss, &is->is_srcs, ss_entry) {
			if (ss->ss_csid == csid[off]) {
				TAILQ_FOREACH(sd, &ss->ss_dsts, sd_entry) {
					elen = len;
					ret = do_conv_map_one(sd, &csid[off],
					    &idx[off], &elen, &tmpidx);
					if (ret != 0 && ret != ENOENT &&
					    ret != EAGAIN) {
						*cnt = total + elen;
						return (ret);
					}

					/*
					 * If we succeeded, we *have* to have
					 * processed all of them.
					 *
					 * If we failed, we must not have;
					 * hitting this particular assertion
					 * will indicate that we failed to
					 * update *cnt in a _csmapper_convert
					 * somewhere.
					 *
					 * In the EAGAIN case, we must have
					 * reflected the translit mapping in the
					 * *cnt so we can't tell anything about
					 * it relative to `len`, but we know it
					 * can't be 0.
					 */
					if (ret == 0)
						assert(elen == len);
					else if (ret != EAGAIN)
						assert(elen < len);
					else
						assert(elen > 0);

					if (ret == EAGAIN) {
						/*
						 * If we hit a translit mapping,
						 * we'll drop the last entry and
						 * keep going, but note that we
						 * do have a tentative entry for
						 * the next character.
						 */
						tentative = true;
						tmpcsid = checkid;
						elen--;
					} else if (ret == 0) {
						/* Reset once we succeed. */
						tentative = false;
					}

					/*
					 * If we could convert at least one
					 * character, we should advance that
					 * many then start over.  We'll end up
					 * hitting an ENOENT on the next
					 * character *again*, but it's not worth
					 * the complexity to skip just the
					 * one dst.
					 */
					if (elen > 0) {
						total += elen;
						tmpcnt -= elen;
						off += elen;
						goto next;
					}
				}

				if (tentative)
					continue;

				break;
			}
		}

		/*
		 * Commit the tentative mapping if we have transliteration
		 * enabled.
		 */
		if (tentative && cv->cv_shared->ci_translit) {
			idx[off] = tmpidx;
			csid[off] = tmpcsid;
			total++;
			tmpcnt--;
			off++;
			(*invalp)++;
			goto next;
		}

		*cnt = total;
	}
#else
	TAILQ_FOREACH(ss, &is->is_srcs, ss_entry) {
		if (ss->ss_csid == *csid) {
			TAILQ_FOREACH(sd, &ss->ss_dsts, sd_entry) {
				ret = _csmapper_convert(sd->sd_mapper,
				    &tmpidx, *idx, NULL);
				switch (ret) {
				case _MAPPER_CONVERT_SUCCESS:
					*csid = sd->sd_csid;
					*idx = tmpidx;
					return (0);
				case _MAPPER_CONVERT_NONIDENTICAL:
					break;
				case _MAPPER_CONVERT_SRC_MORE:
					/*FALLTHROUGH*/
				case _MAPPER_CONVERT_DST_MORE:
					/*FALLTHROUGH*/
				case _MAPPER_CONVERT_ILSEQ:
					return (EILSEQ);
				case _MAPPER_CONVERT_FATAL:
					return (EINVAL);
				}
			}
			break;
		}
	}
#endif

	return (E_NO_CORRESPONDING_CHAR);
}
/* ---------------------------------------------------------------------- */

static int
/*ARGSUSED*/
_citrus_iconv_std_iconv_init_shared(struct _citrus_iconv_shared *ci,
    const char * __restrict src, const char * __restrict dst)
{
	struct _citrus_esdb esdbdst, esdbsrc;
	struct _citrus_iconv_std_shared *is;
#ifdef __APPLE__
	int count;
#endif
	int ret;

	is = malloc(sizeof(*is));
	if (is == NULL) {
		ret = errno;
		goto err0;
	}
	ret = _citrus_esdb_open(&esdbsrc, src);
	if (ret)
		goto err1;
	ret = _citrus_esdb_open(&esdbdst, dst);
	if (ret)
		goto err2;
	ret = _stdenc_open(&is->is_src_encoding, esdbsrc.db_encname,
	    esdbsrc.db_variable, esdbsrc.db_len_variable);
	if (ret)
		goto err3;
	ret = _stdenc_open(&is->is_dst_encoding, esdbdst.db_encname,
	    esdbdst.db_variable, esdbdst.db_len_variable);
	if (ret)
		goto err4;
	is->is_use_invalid = esdbdst.db_use_invalid;
	is->is_invalid = esdbdst.db_invalid;

#ifdef __APPLE__
	is->is_lone_dst = NULL;
	is->is_lone_dst_csid = -1;
#endif

	TAILQ_INIT(&is->is_srcs);
#ifdef __APPLE__
	ret = open_srcs(&is->is_srcs, &esdbsrc, &esdbdst, &count);
#else
	ret = open_srcs(&is->is_srcs, &esdbsrc, &esdbdst);
#endif
	if (ret)
		goto err5;

#ifdef __APPLE__
	if (count == 1) {
		struct _citrus_iconv_std_dst *sd;
		struct _citrus_iconv_std_src *ss;

		count = 0;

		ss = TAILQ_FIRST(&is->is_srcs);

		/* Do we only have one dst? */
		TAILQ_FOREACH(sd, &ss->ss_dsts, sd_entry) {
			count++;
		}

		if (count == 1) {
			is->is_lone_dst = TAILQ_FIRST(&ss->ss_dsts);
			is->is_lone_dst_csid = ss->ss_csid;
		}
	}
#endif

	_esdb_close(&esdbsrc);
	_esdb_close(&esdbdst);
	ci->ci_closure = is;

	return (0);

err5:
	_stdenc_close(is->is_dst_encoding);
err4:
	_stdenc_close(is->is_src_encoding);
err3:
	_esdb_close(&esdbdst);
err2:
	_esdb_close(&esdbsrc);
err1:
	free(is);
err0:
	return (ret);
}

static void
_citrus_iconv_std_iconv_uninit_shared(struct _citrus_iconv_shared *ci)
{
	struct _citrus_iconv_std_shared *is = ci->ci_closure;

	if (is == NULL)
		return;

	_stdenc_close(is->is_src_encoding);
	_stdenc_close(is->is_dst_encoding);
	close_srcs(&is->is_srcs);
	free(is);
}

static int
_citrus_iconv_std_iconv_init_context(struct _citrus_iconv *cv)
{
	const struct _citrus_iconv_std_shared *is = cv->cv_shared->ci_closure;
	struct _citrus_iconv_std_context *sc;
	char *ptr;
	size_t sz, szpsdst, szpssrc;

	szpssrc = _stdenc_get_state_size(is->is_src_encoding);
	szpsdst = _stdenc_get_state_size(is->is_dst_encoding);

	sz = (szpssrc + szpsdst)*2 + sizeof(struct _citrus_iconv_std_context);
	sc = malloc(sz);
	if (sc == NULL)
		return (errno);

#ifdef __APPLE__
	memset(&sc->sc_mbstate, 0, sizeof(sc->sc_mbstate));
#endif

	ptr = (char *)&sc[1];
	if (szpssrc > 0)
		init_encoding(&sc->sc_src_encoding, is->is_src_encoding,
		    ptr, ptr+szpssrc);
	else
		init_encoding(&sc->sc_src_encoding, is->is_src_encoding,
		    NULL, NULL);
	ptr += szpssrc*2;
	if (szpsdst > 0)
		init_encoding(&sc->sc_dst_encoding, is->is_dst_encoding,
		    ptr, ptr+szpsdst);
	else
		init_encoding(&sc->sc_dst_encoding, is->is_dst_encoding,
		    NULL, NULL);

	cv->cv_closure = (void *)sc;

	return (0);
}

static void
_citrus_iconv_std_iconv_uninit_context(struct _citrus_iconv *cv)
{

	free(cv->cv_closure);
}

static int
_citrus_iconv_std_iconv_convert(struct _citrus_iconv * __restrict cv,
    char * __restrict * __restrict in, size_t * __restrict inbytes,
    char * __restrict * __restrict out, size_t * __restrict outbytes,
    uint32_t flags, size_t * __restrict invalids)
{
#ifdef __APPLE__
	_csid_t csid[_ICONV_STD_PERCVT];
	_index_t idx[_ICONV_STD_PERCVT];
	unsigned short delta[_ICONV_STD_PERCVT];	/* Cumulative */
#endif
	const struct _citrus_iconv_std_shared *is = cv->cv_shared->ci_closure;
	struct _citrus_iconv_std_context *sc = cv->cv_closure;
#ifndef __APPLE__
	_csid_t csid;
	_index_t idx;
#endif
	char *tmpin;
	size_t inval, in_mb_cur_min, szrin, szrout;
	int ret, state = 0;
#ifdef __APPLE__
	int cnt, tmpcnt;
#endif

	inval = 0;
	if (in == NULL || *in == NULL) {
		/* special cases */
		if (out != NULL && *out != NULL) {
			/* init output state and store the shift sequence */
			save_encoding_state(&sc->sc_src_encoding);
			save_encoding_state(&sc->sc_dst_encoding);
			szrout = 0;

			ret = put_state_resetx(&sc->sc_dst_encoding,
			    *out, *outbytes, &szrout);
			if (ret)
				goto err;

			if (szrout == (size_t)-2) {
				/* too small to store the character */
				ret = EINVAL;
				goto err;
			}
			*out += szrout;
			*outbytes -= szrout;
		} else
			/* otherwise, discard the shift sequence */
			init_encoding_state(&sc->sc_dst_encoding);
		init_encoding_state(&sc->sc_src_encoding);
		*invalids = 0;
		return (0);
	}

	in_mb_cur_min = _stdenc_get_mb_cur_min(is->is_src_encoding);

	/* normal case */
	for (;;) {
		if (*inbytes == 0) {
			ret = get_state_desc_gen(&sc->sc_src_encoding, &state);
			if (state == _STDENC_SDGEN_INITIAL ||
			    state == _STDENC_SDGEN_STABLE)
				break;
		}

#ifndef __APPLE__
		/*
		 * In Darwin, this is pushed into mbtocsx/cstombx because we
		 * batch up characters to process.
		 */
		/* save the encoding states for the error recovery */
		save_encoding_state(&sc->sc_src_encoding);
		save_encoding_state(&sc->sc_dst_encoding);
#endif

		/* mb -> csid/index */
		tmpin = *in;
		szrin = szrout = 0;

#ifdef __APPLE__
		tmpcnt = cnt = nitems(csid);
		if ((cv->cv_wchar_dir & MDIR_UCS_SRC) != 0) {
			char *tmpwin;
			size_t szwin;

			memset(&sc->sc_wcdelta, 0, sizeof(sc->sc_wcdelta));
			ret = iconv_std_wctomb(cv, &sc->sc_wcdelta[0], tmpcnt,
			    &tmpin, *inbytes, &szwin);

			/*
			 * We can thus anticipate an error on the next run of
			 * iconv_std_wctomb(), but we don't actually do anything
			 * with that fact just yet.
			 */
			if (ret != 0 && szwin != 0)
				ret = 0;
			if (ret != 0)
				goto err;

			/*
			 * When wctomb is feeding mbtocsx, we just need to
			 * carefully divert to our contexts' wcbuf.
			 */
			tmpwin = &sc->sc_wcbuf[0];
			ret = mbtocsx(&sc->sc_src_encoding, &csid[0], &idx[0],
			    &delta[0], &tmpcnt, &tmpwin, szwin, &szrin,
			    cv->cv_shared->ci_hooks);
			if (tmpcnt > 0) {
				/*
				 * Before we go any further, map delta back to
				 * actual offsets into the original buffer.  At
				 * the moment, it maps to offsets into our
				 * intermediate buffer.
				 */
				iconv_std_delta_remap(tmpcnt, cnt,
				    &sc->sc_wcdelta[0], &delta[0]);
			}
		} else {
			ret = mbtocsx(&sc->sc_src_encoding, &csid[0], &idx[0],
			    &delta[0], &tmpcnt, &tmpin, *inbytes, &szrin,
			    cv->cv_shared->ci_hooks);
		}

		/*
		 * Record how many characters we started out with, so we can
		 * figure out near the end if we lost something.
		 */
		cnt = tmpcnt;

		if (szrin == (size_t)-2 && tmpcnt > 0) {
			/*
			 * We had a partial conversion, but it ended in a
			 * sequence that's incomplete.  We can wipe out the
			 * encoding state and tmpcnt/tmpin reflect the part of
			 * the buffer that wasn't problematic; this loop will
			 * restart because we still have more left, then we'll
			 * hit the restart condition again with *no* characters
			 * converted.
			 *
			 * If we don't reset the encoding state here, then the
			 * encoding module will still think we're in the middle
			 * of a multibyte sequence and likely error out with an
			 * EILSEQ instead.
			 */
			init_encoding_state(&sc->sc_src_encoding);

			szrin = delta[tmpcnt - 1];
			tmpin = *in + szrin;
		}

		if (ret == EILSEQ && cv->cv_shared->ci_discard_ilseq) {
			/*
			 * If //IGNORE was specified, we'll just keep crunching
			 * through invalid characters.
			 */
			tmpin += in_mb_cur_min;

			/* Discard the src shift state, we're starting over. */
			init_encoding_state(&sc->sc_src_encoding);

			/*
			 * If there weren't any previous characters, we need to
			 * re-mbtocsx() it now that we've potentially advanced
			 * past the invalid sequence.  If there were previous
			 * characters, we'll still process those.
			 */
			if (tmpcnt == 0) {
				*inbytes -= tmpin - *in;
				*in = tmpin;
				continue;
			}
		}

		/*
		 * If we hit an error without converting any characters, we'll
		 * bail out.  Otherwise, we still need to attempt output of what
		 * we have and do our out-pointer accounting.
		 */
		if (ret != 0 && tmpcnt == 0) {
			goto err;
		} else if (ret != 0) {
			restore_encoding_state(&sc->sc_src_encoding);
		}
#else
		ret = mbtocsx(&sc->sc_src_encoding, &csid, &idx, &tmpin,
		    *inbytes, &szrin, cv->cv_shared->ci_hooks);
		if (ret != 0 && (ret != EILSEQ ||
		    !cv->cv_shared->ci_discard_ilseq)) {
			goto err;
		} else if (ret == EILSEQ) {
			/*
			 * If //IGNORE was specified, we'll just keep crunching
			 * through invalid characters.
			 */
			*in += in_mb_cur_min;
			*inbytes -= in_mb_cur_min;
			restore_encoding_state(&sc->sc_src_encoding);
			restore_encoding_state(&sc->sc_dst_encoding);
			continue;
		}
#endif

		if (szrin == (size_t)-2) {
			/* incompleted character */
			ret = get_state_desc_gen(&sc->sc_src_encoding, &state);
			if (ret) {
				ret = EINVAL;
				goto err;
			}
			switch (state) {
			case _STDENC_SDGEN_INITIAL:
			case _STDENC_SDGEN_STABLE:
				/* fetch shift sequences only. */
				goto next;
			}
			ret = EINVAL;
			goto err;
		}
		/* convert the character */
#ifdef __APPLE__
		ret = do_conv(cv, is, &csid[0], &idx[0], &tmpcnt, &inval);
		if (ret && tmpcnt != 0) {
			/*
			 * Rewind tmpin so that we hit the invalid seq again in
			 * the next iteration.  Simplifies our error handling...
			 */
			tmpin = *in + delta[tmpcnt - 1];
			assert(tmpin > *in);
		} else
#else
		ret = do_conv(is, &csid, &idx);
#endif
		if (ret) {
			if (ret == E_NO_CORRESPONDING_CHAR) {
				/*
				 * GNU iconv returns EILSEQ when no
				 * corresponding character in the output.
				 * Some software depends on this behavior
				 * though this is against POSIX specification.
				 */
				if (cv->cv_shared->ci_ilseq_invalid != 0) {
					ret = EILSEQ;
#ifdef __APPLE__
					if (cv->cv_shared->ci_discard_ilseq) {
						/*
						 * This will all be audited as
						 * part of a different problem,
						 * but for now the combination
						 * of GNU behavior + //IGNORE
						 * just moves us on past.
						 */
						restore_encoding_state(&sc->sc_dst_encoding);
						inval++;
						ret = 0;

						/*
						 * Advance just past the invalid
						 * character.  We wouldn't have
						 * made it this far if delta[0]
						 * wasn't valid; tmpcnt > 0
						 * after the previous mbtocsx().
						 */
						tmpin = *in + delta[0];
						goto next;
					}
					goto converr;
#else
					goto err;
#endif
				}
				inval++;
				szrout = 0;
				if ((((flags & _CITRUS_ICONV_F_HIDE_INVALID) == 0) &&
				    !cv->cv_shared->ci_discard_ilseq) &&
				    is->is_use_invalid) {
					ret = wctombx(&sc->sc_dst_encoding,
					    *out, *outbytes, is->is_invalid,
					    &szrout, cv->cv_shared->ci_hooks);
					if (ret)
#ifdef __APPLE__
						goto converr;
#else
						goto err;
#endif
				}

				goto next;
			} else
#ifdef __APPLE__
				goto converr;
#else
				goto err;
#endif
		}

		/* csid/index -> mb */
#ifdef __APPLE__
		if ((cv->cv_wchar_dir & MDIR_UCS_DST) != 0) {

			/*
			 * For conversions -to- wchar_t, we just to convert back
			 * out into the buffer in our context instead of
			 * directly into *out, then we'll mbrtowc() into *out.
			 */
			ret = iconv_std_mbtowc(cv, *out, *outbytes, &csid[0],
			    &idx[0], &tmpcnt, &szrout);
		} else {
			ret = cstombx(&sc->sc_dst_encoding,
			    *out, *outbytes, &csid[0], &idx[0], &tmpcnt,
			    &szrout, cv->cv_shared->ci_hooks);
		}

		/*
		 * If we got an EILSEQ, replace that one character with the
		 * invalid byte.
		 */
		if (ret == EILSEQ && is->is_use_invalid) {
			size_t tmpout;

			/*
			 * Wipe out the encoding state, because cstombx() may
			 * have left us somewhere bogus when we failed that
			 * would cause bogus errors from the below wctombx().
			 */
			init_encoding_state(&sc->sc_dst_encoding);

			tmpout = 0;
			ret = wctombx(&sc->sc_dst_encoding,
			    *out + szrout, *outbytes - szrout, is->is_invalid,
			    &tmpout, cv->cv_shared->ci_hooks);

			if (ret == 0) {
				/*
				 * Here we want to eat the invalid character so
				 * that we don't re-encounter it, then adjust
				 * szrout for how much we wrote total.
				 */
				tmpcnt++;
				szrout += tmpout;
			} else {
				/*
				 * We shouldn't[0] be able to get EILSEQ here
				 * because the invalid character is a property
				 * of the destination encoding.  If we did get
				 * it, then the esdb definition is inherently
				 * broken and we need to fix that.
				 *
				 * [0] We actually can, so we have to relax this
				 *     a little bit -- those using the
				 *     citrus_none encoding may specify a bogus
				 *     invalid character that sets more than
				 *     just the lower 16 bits.  Barring a way to
				 *     scope this to just citrus_none, we just
				 *     make it a little more lenient.
				 */
				assert(ret == E2BIG || ret == EILSEQ);
			}
		}

		/*
		 * If we were able to fit *some* of the input
		 * characters, we need to update our output pointer
		 * accounting to accurately reflect the situation.
		 */
		if (ret && tmpcnt == 0)
			goto converr;
		if (tmpcnt < cnt) {
			/*
			 * Rewind in here because we shouldn't have eaten the
			 * excess that doesn't fit in the buffer.
			 */
			tmpin = *in + delta[tmpcnt - 1];
		}

		assert(tmpin > *in);
#else
		ret = cstombx(&sc->sc_dst_encoding,
		    *out, *outbytes, csid, idx, &szrout,
		    cv->cv_shared->ci_hooks);
		if (ret)
			goto err;
#endif
next:
#ifdef __APPLE__
		assert(tmpin - *in <= *inbytes);
		assert(szrout <= *outbytes);
#endif
		*inbytes -= tmpin-*in; /* szrin is insufficient on \0. */
		*in = tmpin;
		*outbytes -= szrout;
		*out += szrout;
#ifdef __APPLE__
		if (ret != 0)
			goto err;
		continue;
converr:

		/*
		 * Error out, but update our accounting first.  If we did any
		 * output, we shouldn't have gotten to this label. and we should
		 * instead be updating pointers above in the `next` label
		 * instead.
		 */
		if (tmpcnt != 0) {
			unsigned short diff;

			/*
			 * delta[n] is cumulative; it describes how many bytes
			 * we need to skip in the input string at each
			 * conversion.  So, if we managed to successfully
			 * convert 5 characters, delta[4] represents the number
			 * of input bytes that covers the first 5.
			 */
			diff = delta[tmpcnt - 1];
			assert((signed short)diff > 0);
			*inbytes -= diff;
			*in += diff;
		}

		goto err;
#endif
	}
	*invalids = inval;

	return (0);

err:
	restore_encoding_state(&sc->sc_src_encoding);
	restore_encoding_state(&sc->sc_dst_encoding);
	*invalids = inval;

	return (ret);
}
