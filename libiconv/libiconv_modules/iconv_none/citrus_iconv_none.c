/* $FreeBSD$ */
/*	$NetBSD: citrus_iconv_none.c,v 1.3 2011/05/23 14:45:44 joerg Exp $	*/

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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "citrus_types.h"
#include "citrus_module.h"
#include "citrus_hash.h"
#include "citrus_iconv.h"
#include "citrus_iconv_none.h"
#ifdef __APPLE__
#include "citrus_mapper.h"
#endif

/* ---------------------------------------------------------------------- */

_CITRUS_ICONV_DECLS(iconv_none);
_CITRUS_ICONV_DEF_OPS(iconv_none);


/* ---------------------------------------------------------------------- */

int
_citrus_iconv_none_iconv_getops(struct _citrus_iconv_ops *ops)
{

	memcpy(ops, &_citrus_iconv_none_iconv_ops,
	       sizeof(_citrus_iconv_none_iconv_ops));

	return (0);
}

static int
/*ARGSUSED*/
_citrus_iconv_none_iconv_init_shared(
    struct _citrus_iconv_shared * __restrict ci,
    const char * __restrict in __unused, const char * __restrict out __unused)
{

	ci->ci_closure = NULL;
	return (0);
}

static void
/*ARGSUSED*/
_citrus_iconv_none_iconv_uninit_shared(struct _citrus_iconv_shared *ci __unused)
{

}

static int
/*ARGSUSED*/
_citrus_iconv_none_iconv_init_context(struct _citrus_iconv *cv)
{

	cv->cv_closure = NULL;
	return (0);
}

static void
/*ARGSUSED*/
_citrus_iconv_none_iconv_uninit_context(struct _citrus_iconv *cv __unused)
{

}

static int
/*ARGSUSED*/
#ifdef __APPLE__
_citrus_iconv_none_iconv_convert(struct _citrus_iconv * __restrict ci,
#else
_citrus_iconv_none_iconv_convert(struct _citrus_iconv * __restrict ci __unused,
#endif
    char * __restrict * __restrict in, size_t * __restrict inbytes,
    char * __restrict * __restrict out, size_t * __restrict outbytes,
    uint32_t flags __unused, size_t * __restrict invalids)
{
#ifdef __APPLE__
	struct iconv_hooks *hooks = ci->cv_shared->ci_hooks;
#endif
	size_t len;
	int e2big;

	if ((in == NULL) || (out == NULL) || (inbytes == NULL))
		return (0);
	if ((*in == NULL) || (*out == NULL) || (*inbytes == 0) || (*outbytes == 0))
		return (0);
	len = *inbytes;
#ifdef __APPLE__
	/*
	 * Floor it to the nearest wchar_t because we could have been passed an
	 * array with some excess, and we shouldn't E2BIG that.
	 */
	if (ci->cv_wchar_dir == MDIR_UCS_BOTH)
		len = (len / sizeof(wchar_t)) * sizeof(wchar_t);
#endif
	e2big = 0;
	if (*outbytes<len) {
		e2big = 1;
		len = *outbytes;
#ifdef __APPLE__
		/* Floor it again. */
		if (ci->cv_wchar_dir == MDIR_UCS_BOTH)
			len = (len / sizeof(wchar_t)) * sizeof(wchar_t);
#endif
	}
#ifdef __APPLE__
	/*
	 * We'll just memcpy the whole array if we don't need to call any hooks.
	 */
	if (ci->cv_wchar_dir == MDIR_UCS_BOTH && hooks != NULL &&
	    hooks->wc_hook != NULL) {
		wchar_t *iwc = (wchar_t *)*in, *owc = (wchar_t *)*out;
		size_t nwchar = len / sizeof(*iwc);

		for (size_t i = 0; i < nwchar; i++, iwc++, owc++) {
			*owc = *iwc;

			hooks->wc_hook(*owc, hooks->data);
		}
	} else {
		memcpy(*out, *in, len);
	}
#else
	memcpy(*out, *in, len);
#endif
	in += len;
	*inbytes -= len;
	out += len;
	*outbytes -= len;
	*invalids = 0;
	if (e2big)
		return (E2BIG);

	return (0);
}
