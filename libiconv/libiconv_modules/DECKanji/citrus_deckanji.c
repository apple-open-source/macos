/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Apple Computer, Inc.
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
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef __APPLE__
#include <stdbool.h>
#endif /* __APPLE__ */

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_bcs.h"
#include "citrus_module.h"
#include "citrus_stdenc.h"
#include "citrus_deckanji.h"

/* ----------------------------------------------------------------------
 * private stuffs used by templates
 */

typedef struct {
	size_t	 chlen;
	char	 ch[2];
} _DECKanjiState;

typedef struct {
	int	 dummy;
} _DECKanjiEncodingInfo;

#define _CEI_TO_EI(_cei_)		(&(_cei_)->ei)
#define _CEI_TO_STATE(_cei_, _func_)	(_cei_)->states.__CONCAT(s_,_func_)

#define _FUNCNAME(m)			__CONCAT(_citrus_DECKanji_,m)
#define _ENCODING_INFO			_DECKanjiEncodingInfo
#define _ENCODING_STATE			_DECKanjiState
#define _ENCODING_MB_CUR_MAX(_ei_)		2
#define _ENCODING_IS_STATE_DEPENDENT		0
#define _STATE_NEEDS_EXPLICIT_INIT(_ps_)	0

static __inline void
/*ARGSUSED*/
_citrus_DECKanji_init_state(_DECKanjiEncodingInfo * __restrict ei __unused,
    _DECKanjiState * __restrict psenc)
{

	psenc->chlen = 0;
}

static void
/*ARGSUSED*/
_citrus_DECKanji_encoding_module_uninit(_DECKanjiEncodingInfo *ei __unused)
{

	/* ei may be null */
}

static int
/*ARGSUSED*/
_citrus_DECKanji_encoding_module_init(_DECKanjiEncodingInfo * __restrict ei __unused,
    const void * __restrict var __unused, size_t lenvar __unused)
{

	/* ei may be null */
	return (0);
}

static __inline bool
is_singlebyte(int c)
{

	return (c <= 0x7F);
}

static __inline bool
is_leadbyte(int c)
{

	return (c & 0x80) != 0;
}

static int
/*ARGSUSED*/
_citrus_DECKanji_mbrtowc_priv(_DECKanjiEncodingInfo * __restrict ei,
    wchar_t * __restrict pwc, char ** __restrict s, size_t n,
    _DECKanjiState * __restrict psenc, size_t * __restrict nresult)
{
	char *s0;
	wchar_t wc;
	int ch, len, olen;

	if (*s == NULL) {
		_citrus_DECKanji_init_state(ei, psenc);
		*nresult = _ENCODING_IS_STATE_DEPENDENT;
		return (0);
	}
	s0 = *s;

	olen = psenc->chlen;
	wc = (wchar_t)0;
	switch (psenc->chlen) {
	case 0:
		if (n-- < 1)
			goto restart;
		ch = *s0++ & 0xFF;
		if (!is_singlebyte(ch) && !is_leadbyte(ch))
			goto ilseq;
		psenc->ch[psenc->chlen++] = ch;
		break;
	case 1:
		ch = psenc->ch[0] & 0xFF;
		break;
	default:
		return (EINVAL);
	}

	len = is_singlebyte(ch) ? 1 : 2;
	while (psenc->chlen < len) {
		if (n-- < 1)
			goto restart;

		psenc->ch[psenc->chlen++] = *s0++;
	}

	*s = s0;

	switch (len) {
	case 1:
		wc = psenc->ch[0] & 0xff;
		break;
	case 2:
		wc = ((psenc->ch[0] & 0xff) << 8) | (psenc->ch[1] & 0xff);
		break;
	default:
		/* Illegal state */
		goto ilseq;
	}

	psenc->chlen = 0;
	if (pwc != NULL)
		*pwc = wc;

	*nresult = wc ? len - olen : 0;
	return (0);

restart:
	*nresult = (size_t)-2;
	*s = s0;
	return (0);

ilseq:
	*nresult = (size_t)-1;
	return (EILSEQ);
}

static int
/*ARGSUSED*/
_citrus_DECKanji_wcrtomb_priv(_DECKanjiEncodingInfo * __restrict ei __unused,
    char * __restrict s, size_t n, wchar_t wc,
    _DECKanjiState * __restrict psenc, size_t * __restrict nresult)
{
	int ch;

	if (psenc->chlen != 0)
		return (EINVAL);

	/* XXX: assume wchar_t as int */
	if ((uint32_t)wc < 0x100) {
		/* ASCII, JISX0201 Roman, JISX0201 Kana*/
		ch = wc & 0xFF;
	} else {
		ch = (wc >> 8) & 0xFF;
		if (!is_leadbyte(ch))
			goto ilseq;
		psenc->ch[psenc->chlen++] = ch;
		ch = wc & 0xFF;
	}
	psenc->ch[psenc->chlen++] = ch;
	if (n < psenc->chlen) {
		*nresult = (size_t)-1;
		return (E2BIG);
	}
	memcpy(s, psenc->ch, psenc->chlen);
	*nresult = psenc->chlen;
	psenc->chlen = 0;

	return (0);

ilseq:
	*nresult = (size_t)-1;
	return (EILSEQ);
}

static __inline int
/*ARGSUSED*/
_citrus_DECKanji_stdenc_wctocs(_DECKanjiEncodingInfo * __restrict ei __unused,
    _csid_t * __restrict csid, _index_t * __restrict idx, wchar_t wc)
{
	_index_t mask;

	mask = 0x7f;
	if ((_wc_t)wc < 0x80) {
		/* ISO-646-JP / JISX0201 Roman */
		*csid = 0;
	} else if ((_wc_t)wc < 0x100) {
		/* JISX0201-KANA */
		*csid = 1;
	} else {
		if (((_wc_t)wc & 0x80) != 0)
			/* JISX0208 */
			*csid = 2;
		else
			/* UDA */
			*csid = 3;

		/* First byte also needs the high bit masked. */
		mask |= ((_wc_t)0x7f << 8);
	}

	*idx = (_index_t)(wc & mask);

	return (0);
}

static __inline int
/*ARGSUSED*/
_citrus_DECKanji_stdenc_cstowc(_DECKanjiEncodingInfo * __restrict ei __unused,
    wchar_t * __restrict wc, _csid_t csid, _index_t idx)
{
	wchar_t mask;

	mask = 0;
	switch (csid) {
	case 1:
		/* JISX0201-KANA */
		mask |= 0x80;
		/* FALLTHROUGH */
	case 0:
		/* ISO646 */
		if (idx >= 0x80)
			return (EILSEQ);
		break;
	case 2:
		/* JISX0208 */
		mask |= 0x80;
		/* FALLTHROUGH */
	case 3:
		/* UDA */
		mask |= 0x8000;
		break;
	default:
		return (EILSEQ);
	}

	*wc = mask | (wchar_t)idx;
	return (0);
}

static __inline int
/*ARGSUSED*/
_citrus_DECKanji_stdenc_get_state_desc_generic(
    _DECKanjiEncodingInfo * __restrict ei __unused,
    _DECKanjiState * __restrict psenc, int * __restrict rstate)
{

	*rstate = (psenc->chlen == 0)
	    ? _STDENC_SDGEN_INITIAL
	    : _STDENC_SDGEN_INCOMPLETE_CHAR;
	return (0);
}

/* ----------------------------------------------------------------------
 * public interface for stdenc
 */

_CITRUS_STDENC_DECLS(DECKanji);
_CITRUS_STDENC_DEF_OPS(DECKanji);

#include "citrus_stdenc_template.h"
