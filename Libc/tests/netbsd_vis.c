/*	$NetBSD: t_vis.c,v 1.7 2014/09/08 19:01:03 christos Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code was contributed to The NetBSD Foundation by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <vis.h>

#include <darwintest.h>
#include <locale.h>

#include <TargetConditionals.h>

static int styles[] = {
	VIS_OCTAL,
	VIS_CSTYLE,
	VIS_SP,
	VIS_TAB,
	VIS_NL,
	VIS_WHITE,
	VIS_SAFE,
#if 0	/* Not reversible */
	VIS_NOSLASH,
#endif
	VIS_HTTP1808,
	VIS_MIMESTYLE,
#if 0	/* Not supported by vis(3) */
	VIS_HTTP1866,
#endif
};

#define SIZE	256

T_DECL(strvis_basic, "strvis(3)")
{
	char *srcbuf, *dstbuf, *visbuf;
	unsigned int i, j;

	T_ASSERT_NOTNULL((srcbuf = malloc(SIZE)), NULL);
	T_ASSERT_NOTNULL((dstbuf = malloc(SIZE + 1)), NULL);
	T_ASSERT_NOTNULL((visbuf = malloc(SIZE * 4 + 1)), NULL);

	for (i = 0; i < SIZE; i++)
		srcbuf[i] = (char)i;

	for (i = 0; i < sizeof(styles)/sizeof(styles[0]); i++) {
		T_ASSERT_GT(strsvisx(visbuf, srcbuf, SIZE, styles[i], ""), 0, NULL);
		memset(dstbuf, 0, SIZE);
		T_ASSERT_GT(strunvisx(dstbuf, visbuf, 
		    styles[i] & (VIS_HTTP1808|VIS_MIMESTYLE)), 0, NULL);
		for (j = 0; j < SIZE; j++)
			if (dstbuf[j] != (char)j)
				T_FAIL("Failed for style %x, char %d [%d]", styles[i], j, dstbuf[j]);
		if (dstbuf[SIZE] != '\0')
			T_FAIL("Failed for style %x, the result must be null-terminated [%d]", styles[i], dstbuf[SIZE]);
	}
	free(dstbuf);
	free(srcbuf);
	free(visbuf);
}

T_DECL(strvis_null, "strvis(3) NULL")
{
	char dst[] = "fail";
	strvis(dst, NULL, VIS_SAFE);
	T_ASSERT_EQ(dst[0], '\0', NULL);
	T_ASSERT_EQ(dst[1], 'a', NULL);
}

T_DECL(strvis_empty, "strvis(3) empty")
{
	char dst[] = "fail";
	strvis(dst, "", VIS_SAFE);
	T_ASSERT_EQ(dst[0], '\0', NULL);
	T_ASSERT_EQ(dst[1], 'a', NULL);
}

#if TARGET_OS_OSX
/* rdar://problem/108684957 - multibyte spaces should also be passed through */
T_DECL(strvis_cstyle_mbspace, "strvis(3) multibyte space")
{
	const char str[] = "\xe2\x80\xaf";
	char dst[(sizeof(str) - 1) * 4];
	int ret;

	(void)setlocale(LC_CTYPE, "en_US.UTF-8");

	ret = strsnvisx(dst, sizeof(dst), str, sizeof(str) - 1,
	    VIS_CSTYLE | VIS_NOSLASH, "\\\"\b\f\n\r\t");
	T_ASSERT_EQ(ret, sizeof(str) - 1, NULL);
	T_ASSERT_EQ_STR(dst, str, NULL);

	(void)setlocale(LC_CTYPE, "C");
}
#endif

T_DECL(strunvis_hex, "strunvis(3) \\Xxx")
{
	static const struct {
		const char *e;
		const char *d;
		int error;
	} ed[] = {
		{ "\\xff", "\xff", 1 },
		{ "\\x1", "\x1", 1 },
		{ "\\x1\\x02", "\x1\x2", 2 },
		{ "\\x1x", "\x1x", 2 },
		{ "\\xx", "", -1 },
	};
	char uv[10];

	for (size_t i = 0; i < sizeof(ed)/sizeof(ed[0]); i++) {
		T_ASSERT_EQ(strunvis(uv, ed[i].e), ed[i].error, NULL);
		if (ed[i].error > 0)
			T_ASSERT_EQ(memcmp(ed[i].d, uv, (unsigned long)ed[i].error), 0, NULL);
	}
}

#define	STRVIS_OVERFLOW_MARKER	0xff	/* Arbitrary */

#if TARGET_OS_OSX
T_DECL(strvis_overflow_mb, "Test strvis(3) multi-byte overflow")
{
	const char src[] = "\xf0\x9f\xa5\x91";
	/* Extra byte to detect overflow */
	char dst[sizeof(src) + 1];
	int n;

	setlocale(LC_CTYPE, "en_US.UTF-8");

	/* Arbitrary */
	memset(dst, STRVIS_OVERFLOW_MARKER, sizeof(dst));

	/*
	 * If we only provide four bytes of buffer, we shouldn't be able encode
	 * a full 4-byte sequence.
	 */
	n = strnvis(dst, 4, src, VIS_SAFE);
	T_ASSERT_EQ_CHAR((unsigned char)dst[4], STRVIS_OVERFLOW_MARKER, NULL);
	T_ASSERT_EQ(n, -1, NULL);

	n = strnvis(dst, sizeof(src), src, VIS_SAFE);
	T_ASSERT_EQ(n, sizeof(src) - 1, NULL);
}
#endif

T_DECL(strvis_overflow_c, "Test strvis(3) C locale overflow")
{
	const char src[] = "AAAA";
	/* Extra byte to detect overflow */
	char dst[sizeof(src) + 1];
	int n;

	/* Arbitrary */
	memset(dst, STRVIS_OVERFLOW_MARKER, sizeof(dst));

	/*
	 * If we only provide four bytes of buffer, we shouldn't be able encode
	 * 4 bytes of input.
	 */
	n = strnvis(dst, 4, src, VIS_SAFE | VIS_NOLOCALE);
	T_ASSERT_EQ_CHAR((unsigned char)dst[4], STRVIS_OVERFLOW_MARKER, NULL);
	T_ASSERT_EQ(n, -1, NULL);

	n = strnvis(dst, sizeof(src), src, VIS_SAFE | VIS_NOLOCALE);
	T_ASSERT_EQ(n, sizeof(src) - 1, NULL);
}
