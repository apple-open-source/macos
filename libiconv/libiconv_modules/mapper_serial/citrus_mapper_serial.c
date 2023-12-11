/* $FreeBSD$ */
/*	$NetBSD: citrus_mapper_serial.c,v 1.2 2003/07/12 15:39:20 tshiozak Exp $	*/

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
#include <sys/queue.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_bcs.h"
#include "citrus_module.h"
#include "citrus_region.h"
#include "citrus_memstream.h"
#include "citrus_mmap.h"
#include "citrus_hash.h"
#include "citrus_mapper.h"
#include "citrus_mapper_serial.h"

/* ---------------------------------------------------------------------- */

_CITRUS_MAPPER_DECLS(mapper_serial);
_CITRUS_MAPPER_DEF_OPS(mapper_serial);

#define _citrus_mapper_parallel_mapper_init		\
	_citrus_mapper_serial_mapper_init
#define _citrus_mapper_parallel_mapper_uninit		\
	_citrus_mapper_serial_mapper_uninit
#define _citrus_mapper_parallel_mapper_init_state	\
	_citrus_mapper_serial_mapper_init_state
#ifdef __APPLE__
static int	_citrus_mapper_parallel_mapper_convert(
		    struct _citrus_mapper * __restrict, _index_t * __restrict,
		    _index_t * __restrict, int * __restrict, void * __restrict);
#else
static int	_citrus_mapper_parallel_mapper_convert(
		    struct _citrus_mapper * __restrict, _index_t * __restrict,
		    _index_t, void * __restrict);
#endif
_CITRUS_MAPPER_DEF_OPS(mapper_parallel);
#undef _citrus_mapper_parallel_mapper_init
#undef _citrus_mapper_parallel_mapper_uninit
#undef _citrus_mapper_parallel_mapper_init_state


/* ---------------------------------------------------------------------- */

struct maplink {
	STAILQ_ENTRY(maplink)	 ml_entry;
	struct _mapper		*ml_mapper;
};
STAILQ_HEAD(maplist, maplink);

struct _citrus_mapper_serial {
	struct maplist		 sr_mappers;
};

int
_citrus_mapper_serial_mapper_getops(struct _citrus_mapper_ops *ops)
{

	memcpy(ops, &_citrus_mapper_serial_mapper_ops,
	    sizeof(_citrus_mapper_serial_mapper_ops));

	return (0);
}

int
_citrus_mapper_parallel_mapper_getops(struct _citrus_mapper_ops *ops)
{

	memcpy(ops, &_citrus_mapper_parallel_mapper_ops,
	    sizeof(_citrus_mapper_parallel_mapper_ops));

	return (0);
}

static void
uninit(struct _citrus_mapper_serial *sr)
{
	struct maplink *ml;

	while ((ml = STAILQ_FIRST(&sr->sr_mappers)) != NULL) {
		STAILQ_REMOVE_HEAD(&sr->sr_mappers, ml_entry);
		_mapper_close(ml->ml_mapper);
		free(ml);
	}
}

static int
parse_var(struct _citrus_mapper_area *__restrict ma,
    struct _citrus_mapper_serial *sr, struct _memstream *ms)
{
	struct _region r;
	struct maplink *ml;
	char mapname[PATH_MAX];
	int ret;

	STAILQ_INIT(&sr->sr_mappers);
	while (1) {
		/* remove beginning white spaces */
		_memstream_skip_ws(ms);
		if (_memstream_iseof(ms))
			break;
		/* cut down a mapper name */
		_memstream_chr(ms, &r, ',');
		snprintf(mapname, sizeof(mapname), "%.*s",
		    (int)_region_size(&r), (char *)_region_head(&r));
		/* remove trailing white spaces */
		mapname[_bcs_skip_nonws(mapname)-mapname] = '\0';
		/* create a new mapper record */
		ml = malloc(sizeof(*ml));
		if (ml == NULL)
			return (errno);
		ret = _mapper_open(ma, &ml->ml_mapper, mapname);
		if (ret) {
			free(ml);
			return (ret);
		}
		/* support only 1:1 and stateless converter */
		if (_mapper_get_src_max(ml->ml_mapper) != 1 ||
		    _mapper_get_dst_max(ml->ml_mapper) != 1 ||
		    _mapper_get_state_size(ml->ml_mapper) != 0) {
			free(ml);
			return (EINVAL);
		}
		STAILQ_INSERT_TAIL(&sr->sr_mappers, ml, ml_entry);
	}
	return (0);
}

static int
/*ARGSUSED*/
_citrus_mapper_serial_mapper_init(struct _citrus_mapper_area *__restrict ma __unused,
    struct _citrus_mapper * __restrict cm, const char * __restrict dir __unused,
    const void * __restrict var, size_t lenvar,
    struct _citrus_mapper_traits * __restrict mt, size_t lenmt)
{
	struct _citrus_mapper_serial *sr;
	struct _memstream ms;
	struct _region r;

	if (lenmt < sizeof(*mt))
		return (EINVAL);

	sr = malloc(sizeof(*sr));
	if (sr == NULL)
		return (errno);

	_region_init(&r, __DECONST(void *, var), lenvar);
	_memstream_bind(&ms, &r);
	if (parse_var(ma, sr, &ms)) {
		uninit(sr);
		free(sr);
		return (EINVAL);
	}
	cm->cm_closure = sr;
	mt->mt_src_max = mt->mt_dst_max = 1;	/* 1:1 converter */
	mt->mt_state_size = 0;			/* stateless */

	return (0);
}

static void
/*ARGSUSED*/
_citrus_mapper_serial_mapper_uninit(struct _citrus_mapper *cm)
{

	if (cm && cm->cm_closure) {
		uninit(cm->cm_closure);
		free(cm->cm_closure);
	}
}

static int
/*ARGSUSED*/
#ifdef __APPLE__
_citrus_mapper_serial_mapper_convert(struct _citrus_mapper * __restrict cm,
    _index_t * __restrict dst, _index_t * __restrict src,
    int * __restrict cnt, void * __restrict ps __unused)
#else

_citrus_mapper_serial_mapper_convert(struct _citrus_mapper * __restrict cm,
    _index_t * __restrict dst, _index_t src, void * __restrict ps __unused)
#endif
{
	struct _citrus_mapper_serial *sr;
	struct maplink *ml;
	int ret;
#ifdef __APPLE__
	int incnt = *cnt;
	bool tentative;
#endif

	sr = cm->cm_closure;
#ifdef __APPLE__
	tentative = false;

	/*
	 * Upper levels should generally not have a problem
	 * with dst[n] being potentially bogus, and we don't want to
	 * clobber src[n] with invalid mappings because we could
	 * ourselves be chained with another mapper in parallel.
	 */
	memcpy(&dst[0], &src[0], *cnt * sizeof(dst[0]));
#endif
	STAILQ_FOREACH(ml, &sr->sr_mappers, ml_entry) {
#ifdef __APPLE__

		/*
		 * We let the underlying mo_convert() implementation
		 * update *cnt.  Each iteration of this loop is expected
		 * to succeed for the entire *cnt; if it doesn't, we
		 * can just leave *cnt to whatever the first failure
		 * set it to.
		 */
		ret = _mapper_convert(ml->ml_mapper, &dst[0], &dst[0], cnt,
		    NULL);
		if (ret == _MAPPER_CONVERT_TRANSLIT) {
			/*
			 * Note that this is a translit mapping and move on.
			 * Note that one TRANSLIT entry in the chain of mappers
			 * will dirty the whole conversion, meaning the end
			 * result is a transliteration.
			 */
			tentative = true;
			continue;
		}
#else
		ret = _mapper_convert(ml->ml_mapper, &src, src, NULL);
#endif
		if (ret != _MAPPER_CONVERT_SUCCESS) {
#ifdef __APPLE__
			assert(*cnt < incnt);
#endif
			return (ret);
		}
#ifdef __APPLE__
		else {
			assert(*cnt == incnt);
		}
#endif
	}
#ifdef __APPLE__
	if (tentative)
		return (_MAPPER_CONVERT_TRANSLIT);
#else
	*dst = src;
#endif
	return (_MAPPER_CONVERT_SUCCESS);
}

static int
/*ARGSUSED*/
#ifdef __APPLE__
_citrus_mapper_parallel_mapper_convert(struct _citrus_mapper * __restrict cm,
    _index_t * __restrict dst, _index_t * __restrict src, int * __restrict cnt,
    void * __restrict ps __unused)
#else
_citrus_mapper_parallel_mapper_convert(struct _citrus_mapper * __restrict cm,
    _index_t * __restrict dst, _index_t src, void * __restrict ps __unused)
#endif
{
	struct _citrus_mapper_serial *sr;
	struct maplink *ml;
	_index_t tmp;
	int ret;
#ifdef __APPLE__
	int i, incnt, tmpcnt;
	bool tentative = false;
#endif

	sr = cm->cm_closure;
#ifdef __APPLE__
	incnt = *cnt;
	for (i = 0; i < incnt; i++) {
		tentative = false;
		STAILQ_FOREACH(ml, &sr->sr_mappers, ml_entry) {
			/*
			 * Parallel mapper takes a penalty because we don't want
			 * to assume we can keep the # indices constant between
			 * the mapper module and iconv_std.  We would need to
			 * complicate this a bit to allow for a mismatch, so we
			 * just revert to converting one index at a time.
			 */
			tmpcnt = 1;
			ret = _mapper_convert(ml->ml_mapper, &tmp, &src[i],
			     &tmpcnt, NULL);
			if (ret == _MAPPER_CONVERT_SUCCESS) {
				tentative = false;
				dst[i] = tmp;

				goto nextchar;
			} else if (ret == _MAPPER_CONVERT_TRANSLIT) {
				tentative = true;
				dst[i] = tmp;

				/*
				 * Continue checking other mappers in case
				 * another one has a non-tentative match.
				 */
				continue;
			} else if (ret == _MAPPER_CONVERT_ILSEQ) {
				if (tentative) {
					/*
					 * The error can be ignored if we had a
					 * valid transliteration before this; it
					 * is as good as a 'previous success' if
					 * the alternative is pushing into
					 * character sets in which it's invalid.
					 */
					*cnt = i + 1;
					return (_MAPPER_CONVERT_TRANSLIT);
				}

				*cnt = i;
				return (_MAPPER_CONVERT_ILSEQ);
			}
		}

		/*
		 * If we exhausted all of the mapper entries, we must stop now
		 * and report the short *cnt + ENOENT.
		 */
		goto out;
nextchar:
		continue;
	}

out:
	*cnt = i;
	if (tentative) {
		(*cnt)++;
		return (_MAPPER_CONVERT_TRANSLIT);
	}

	if (i == incnt)
		return (_MAPPER_CONVERT_SUCCESS);
#else
	STAILQ_FOREACH(ml, &sr->sr_mappers, ml_entry) {
		ret = _mapper_convert(ml->ml_mapper, &tmp, src, NULL);
		if (ret == _MAPPER_CONVERT_SUCCESS) {
			*dst = tmp;
			return (_MAPPER_CONVERT_SUCCESS);
		} else if (ret == _MAPPER_CONVERT_ILSEQ)
			return (_MAPPER_CONVERT_ILSEQ);
	}
#endif
	return (_MAPPER_CONVERT_NONIDENTICAL);
}

static void
/*ARGSUSED*/
_citrus_mapper_serial_mapper_init_state(void)
{

}
