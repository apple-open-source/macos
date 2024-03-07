/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.

 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <TargetConditionals.h>

/*
 * On !macOS, we'll make this a stub that just writes to stderr and exits with
 * a failure.
 */
#if TARGET_OS_OSX

#include <atf-c.h>
#include <iconv.h>

#ifndef nitems
#define nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

static const char filler[] = "A";
static const size_t filler_chars = sizeof(filler) - 1;

struct mbconv_tc_desc {
	size_t		 tcd_offset;
	const char	*tcd_inject;
	int		 tcd_errno;

	/* Must be filled out for UCS-4 injection, may be omitted for ASCII */
	const char	*tcd_expect_ucs4;
};

struct mbconv_tc_params {
	const char	*tcp_src;
	const char	*tcp_dst;

	/*
	 * Adjusted length for the output buffer; some tests may want a shorter
	 * output buffer to force a conversion over multiple calls.
	 */
	ssize_t		 tcp_adjlen;

	char		**tcp_ostr;
	size_t		*tcp_osz;
	size_t		 tcp_oscale;
	int		*tcp_oerr;
	iconv_t		*tcp_ocd;
	bool		 tcp_badilseq;

	/* Internal accounting */
	size_t		 tcp_offset;
	size_t		 tcp_resid;
};

static void
do_conv(struct mbconv_tc_params *tcp, char *inbuf, size_t insz)
{
	char *inptr;
	char *outbuf, *outptr;
	iconv_t cd;
	size_t bufsz, consumed, inleft, outsz, ret;
	int err;

	if (tcp->tcp_oscale == 0)
		tcp->tcp_oscale = 4;
	bufsz = outsz = (insz * tcp->tcp_oscale) + tcp->tcp_adjlen;
	outbuf = calloc(1, outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	/*
	 * Some tests may try to do multiple conversions with the same handle to
	 * test that there's not some bogus state between trials.
	 */
	if (tcp->tcp_ocd != NULL && *tcp->tcp_ocd != (iconv_t)-1) {
		cd = *tcp->tcp_ocd;
	} else {
		cd = iconv_open(tcp->tcp_dst, tcp->tcp_src);

		if (tcp->tcp_badilseq) {
			int on;

			on = 1;
			(void)iconvctl(cd, ICONV_SET_ILSEQ_INVALID, &on);
		}

		tcp->tcp_offset = 0;
		tcp->tcp_resid = insz;
	}

	ATF_REQUIRE(cd != (iconv_t)-1);

	inptr = &inbuf[0];
	outptr = &outbuf[0];

	ATF_REQUIRE(tcp->tcp_offset <= insz);
	inptr += tcp->tcp_offset;
	insz -= tcp->tcp_offset;

	inleft = insz;
	errno = 0;
	ret = iconv(cd, &inptr, &inleft, &outptr, &outsz);
	err = errno;
	ATF_REQUIRE(tcp->tcp_oerr != NULL || ret != (size_t)-1);

	if (tcp->tcp_oerr != NULL)
		*tcp->tcp_oerr = err;

	tcp->tcp_offset = insz - inleft;
	tcp->tcp_resid = inleft;

	if (tcp->tcp_ocd != NULL) {
		*tcp->tcp_ocd = cd;
	} else {
		iconv_close(cd);
	}

	consumed = bufsz - outsz;
	if (tcp->tcp_ostr != NULL) {
		assert(tcp->tcp_osz != NULL);
		*tcp->tcp_osz = consumed;

		if (consumed != 0) {
			*tcp->tcp_ostr = calloc(1, consumed);
			ATF_REQUIRE(*tcp->tcp_ostr != NULL);

			memcpy(*tcp->tcp_ostr, &outbuf[0], consumed);
		} else {
			*tcp->tcp_ostr = NULL;
		}
	}
}


/*
 * Test one variation with an arbitrary string inserted at an arbitrary offset.
 */
static void
test_one(struct mbconv_tc_params *tcp, size_t offset, const char *inject,
    size_t len)
{
	char *instr;
	size_t total;

	total = offset + len;
	instr = malloc(total);
	ATF_REQUIRE(instr != NULL);

	for (size_t i = 0; i < offset; i++) {
		instr[i] = filler[i % filler_chars];
	}

	memcpy(&instr[offset], inject, len);

	do_conv(tcp, instr, total);
}

static char *
mbconv_expected(size_t offset, const char *inject, size_t ilen, bool in_ucs4,
    size_t *osz)
{
	char *expected;
	size_t i;

	if (ilen == (size_t)-1)
		ilen = strlen(inject);

	for (i = 0; i < ilen; i++) {
		if (!in_ucs4)
			ATF_REQUIRE(inject[i] < 0x7f);
	}

	*osz = offset * 4;
	if (in_ucs4)
		*osz += ilen;
	else
		*osz += ilen * 4;

	expected = calloc(1, *osz + 1);
	ATF_REQUIRE(expected != NULL);

	i = 0;
	for (size_t fpos = 0; i < offset * 4; i++, fpos++) {
		memset(&expected[i], 0, 3 /* 0 Padding the filler */);
		i += 3;
		expected[i] = filler[fpos % filler_chars];
	}

	if (in_ucs4) {
		memcpy(&expected[i], inject, ilen);
	} else {
		/* Padded ASCII */
		for (size_t j = 0; j < ilen; j++) {
			memset(&expected[i], 0, 3);
			i += 3;
			expected[i++] = inject[j];
		}
	}

	return (expected);
}

#ifndef _ICONV_STD_PERCVT
/* Traditionally from citrus_iconv_std_local.h, not exported */
#define	_ICONV_STD_PERCVT	16
#endif

static struct mbconv_tc_desc mbconv_tc_simple[] = {
	/* Trivial three-character string to test the most basic bits. */
	{ 0, "The", 0, NULL },
	/* Push our trivial string over the _ICONV_STD_PERCVT boundary. */
	{ 14, "The", 0, NULL },
	/* Now do a multibyte sequence that goes over the boundary. */
	{ 15, "\xf0\x9f\xa5\x91", 0, "\x00\x01\xf9\x51" },

	/*
	 * Less trivial, add a broken multibyte sequence at the boundary.  The
	 * important part is mostly that it's not at the beginning of the
	 * _ICONV_STD_PERCVT chunk; our test is specifically that we get back an
	 * EINVAL and iconv(3) still processed everything leading up to the
	 * invalid sequence.
	 */
	{ 15, "\xf0\x7f\xa5\x91", EILSEQ, NULL },
};

ATF_TC_WITHOUT_HEAD(mbconv_simple);
ATF_TC_BODY(mbconv_simple, tc)
{
	struct mbconv_tc_desc *tcd;
	const char *tcd_expect;
	char *expected, *str;
	size_t expectedsz, outsz;
	ssize_t injectsz;
	int err;
	struct mbconv_tc_params tcp = {
		.tcp_src = "UTF-8",
		.tcp_dst = "UCS-4BE",
		.tcp_ostr = &str,
		.tcp_osz = &outsz,
	};

	err = 0;
	expectedsz = 0;
	for (size_t i = 0; i < nitems(mbconv_tc_simple); i++) {
		tcd = &mbconv_tc_simple[i];

		if ((tcd_expect = tcd->tcd_expect_ucs4) == NULL) {
			tcd_expect = tcd->tcd_inject;
			injectsz = (size_t)-1;
		} else {
			injectsz = 4;
		}

		expected = NULL;
		if (tcd->tcd_errno == 0) {
			expected = mbconv_expected(tcd->tcd_offset, tcd_expect,
			    injectsz, tcd_expect != tcd->tcd_inject,
			    &expectedsz);
			ATF_REQUIRE(expected != NULL);
			tcp.tcp_oerr = NULL;
		} else {
			tcp.tcp_oerr = &err;
		}

		str = NULL;
		test_one(&tcp, tcd->tcd_offset, tcd->tcd_inject,
		    strlen(tcd->tcd_inject));

		if (tcd->tcd_errno == 0) {
			ATF_REQUIRE_MSG(memcmp(expected, str, expectedsz) == 0,
			    "Failed on %s", tcd->tcd_inject);
		} else {
			ATF_REQUIRE_EQ(tcd->tcd_errno, err);

			/*
			 * We should have at least gotten the filler preamble...
			 */
			ATF_REQUIRE_EQ(tcd->tcd_offset * 4, outsz);

			/* XXX Actually verify contents */
		}

		free(expected);
		free(str);
	}
}

ATF_TC_WITHOUT_HEAD(mbconv_badcs);
ATF_TC_BODY(mbconv_badcs, tc)
{
	char *str;
	size_t outsz;
	int err;
#define	MBC_BADCS_OFS	4
	const char short_inject[] = "\xf0\x9f\xa5\x91";
	struct mbconv_tc_params tcp = {
		.tcp_src = "UTF-8",
		.tcp_dst = "ASCII",
		.tcp_badilseq = true,
		.tcp_ostr = &str,
		.tcp_osz = &outsz,
		.tcp_oerr = &err,
	};

	test_one(&tcp, MBC_BADCS_OFS, short_inject, strlen(short_inject));
	/* Configured to fail for a csmapper conversion. */
	ATF_REQUIRE_EQ(EILSEQ, err);
	/* Should have left exactly short_inject in the buffer. */
	ATF_REQUIRE_EQ(sizeof(short_inject) - 1, tcp.tcp_resid);

	/*
	 * Finally, it should have written out everything before the sequence
	 * that can't be represented in ASCII.
	 */
	ATF_REQUIRE_EQ(MBC_BADCS_OFS, outsz);

	for (int i = 0; i < MBC_BADCS_OFS; i++) {
		ATF_REQUIRE_EQ(filler[i % filler_chars], str[i]);
	}

	free(str);
}

ATF_TC_WITHOUT_HEAD(mbconv_shortbuf);
ATF_TC_BODY(mbconv_shortbuf, tc)
{
	char *expected, *str;
	size_t expectedsz, outsz;
	int err;
#define	MBC_SHORTBUF_OFS	4	/* Arbitrary */
	const char short_inject[] = "\xf0\x9f\xa5\x91";
	struct mbconv_tc_params tcp = {
		.tcp_src = "UTF-8",
		.tcp_dst = "UCS-4BE",

		/*
		 * The math for buffer size in do_conv() is a bit off, hence the
		 * extra here -- it doesn't account for the fact that we're
		 * inserting a multibyte sequence, so it multiplies its length
		 * by 4.  It's debatable whether it's worth trying to plumb
		 * through the knowledge that it's a multi-byte string just for
		 * this purpose; the excess is generally fine everywhere else.
		 */
		.tcp_adjlen = -((strlen(short_inject)  * 4)),
		.tcp_ostr = &str,
		.tcp_osz = &outsz,
		.tcp_oerr = &err,
	};

	expected = mbconv_expected(MBC_SHORTBUF_OFS, "", 0, false,
	    &expectedsz);

	test_one(&tcp, MBC_SHORTBUF_OFS, short_inject, strlen(short_inject));

	ATF_REQUIRE_EQ(E2BIG, err);
	ATF_REQUIRE_EQ_MSG(strlen(short_inject), tcp.tcp_resid,
	    "Expected %zu, ended up with %zu\n", strlen(short_inject),
	    tcp.tcp_resid);
	ATF_REQUIRE_EQ_MSG(expectedsz, outsz, "Expected %zu, got %zu",
	    expectedsz, outsz);
	ATF_REQUIRE(memcmp(expected, str, expectedsz) == 0);

	free(expected);
	free(str);
}

static void
try_nulterm(bool utf7)
{
	iconv_t cd;
	char str[16], outstr[17];
	char *inptr, *outptr;
	size_t insz, n, outsz;

	memset(outstr, '\0', sizeof(outstr));

	insz = 14;
	outsz = sizeof(outstr);
	inptr = &str[0];
	outptr = &outstr[0];
	str[0] = '\xe5';
	str[1] = '\xae';
	str[2] = '\xa2';
	memset(&str[3], 'a', 11);
	str[14] = '\0';
	str[15] = '\xc0';

	cd = iconv_open(utf7 ? "UTF-7//IGNORE" : "UTF-8//IGNORE", "UTF-8");
	ATF_REQUIRE(cd != (iconv_t)-1);

	n = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(n != -1);
	ATF_REQUIRE(outptr - &outstr[0] <= sizeof(outstr));

	if (utf7) {
		for (size_t i = sizeof(str) - 1; i > 4; i--) {
			str[i] = str[i - 2];
		}

		str[0] = '\x2b';
		str[1] = '\x57';
		str[2] = '\x36';
		str[3] = '\x49';
		str[4] = '\x2d';
	}

	if (utf7) {
		ATF_REQUIRE(memcmp(&str[0], &outstr[0], sizeof(str)) == 0);
	} else {
		ATF_REQUIRE(memcmp(&str[0], &outstr[0], sizeof(str) - 1) == 0);

		/*
		 * We end up with two extra bytes because we leave outbuf the
		 * right size to handle both the UTF-8 and UTF-7 cases.  We want
		 * To make sure that we didn't end up with garbage in any of it.
		 */
		ATF_REQUIRE_EQ(0, outstr[sizeof(outstr) - 3]);
		ATF_REQUIRE_EQ(0, outstr[sizeof(outstr) - 2]);
	}

	ATF_REQUIRE_EQ(0, outstr[sizeof(outstr) - 1]);
	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(mbconv_nulterm);
ATF_TC_BODY(mbconv_nulterm, tc)
{
	try_nulterm(false);
	try_nulterm(true);
}

static void
mbconv_parallel_cnt_one(const char *src, const char *dst, const char *inject,
    size_t inlen, const char *expected, size_t explen)
{
	char *str;
	size_t outsz;
	struct mbconv_tc_params tcp = {
		.tcp_src = src,
		.tcp_dst = dst,
		.tcp_ostr = &str,
		.tcp_osz = &outsz,
	};

	test_one(&tcp, 0, inject, inlen);

	ATF_REQUIRE(memcmp(expected, str, explen) == 0);
	free(str);
}

ATF_TC_WITHOUT_HEAD(mbconv_parallel_cnt);
ATF_TC_BODY(mbconv_parallel_cnt, tc)
{
	const char gbk_inject[] = "\xa1\x40";
	const char gbk_expected[] = "\x00\x00\xfe\xff\x00\x00\xe4\xc6";
	const char gb18030_inject[] = "\x30\xe5\x88\x86";
	const char gb18030_expected[] = "\x30\xb7\xd6";

	mbconv_parallel_cnt_one("GBK", "UTF-32", gbk_inject,
	    sizeof(gbk_inject) - 1, gbk_expected, sizeof(gbk_expected) - 1);

	mbconv_parallel_cnt_one("UTF-8", "GB18030", gb18030_inject,
	    sizeof(gb18030_inject) - 1, gb18030_expected,
	    sizeof(gb18030_expected) - 1);
}

ATF_TC_WITHOUT_HEAD(mbconv_serial_cnt);
ATF_TC_BODY(mbconv_serial_cnt, tc)
{
	const char ctext_inject[] = "\xc2\xbd\x00\x01";
	const char ctext_expected[] = "\xbd\x00\x01";

	mbconv_parallel_cnt_one("UTF-8", "CTEXT", ctext_inject,
	    sizeof(ctext_inject) - 1, ctext_expected,
	    sizeof(ctext_expected) - 1);
}

ATF_TC_WITHOUT_HEAD(mbconv_lenreset);
ATF_TC_BODY(mbconv_lenreset, tc)
{
	const char short_inject[] = "\x00\x80";
	char *str;
	size_t outsz;
	int err;
	struct mbconv_tc_params tcp = {
		.tcp_src = "SHIFT_JIS",
		.tcp_dst = "UTF-32",
		.tcp_oerr = &err,
		.tcp_ostr = &str,
		.tcp_osz = &outsz,
		/* Includes BOM */
		.tcp_oscale = 8,
	};

	test_one(&tcp, 0, short_inject, sizeof(short_inject) - 1);
	ATF_REQUIRE_EQ(EILSEQ, err);

	/* BOM + nul byte, \x80 was invalid */
	ATF_REQUIRE_EQ(8, outsz);
	free(str);
}

ATF_TC_WITHOUT_HEAD(mbconv_multics);
ATF_TC_BODY(mbconv_multics, tc)
{
	const char cp932_inject[] = "\"Apple\",\"\xb1\xaf\xcc\xdf\xd9\"";
	const char cp932_expected[] =
	    "\"Apple\",\"\xef\xbd\xb1\xef\xbd\xaf\xef\xbe\x8c\xef\xbe\x9f\xef\xbe\x99\"";

	mbconv_parallel_cnt_one("CP932", "UTF-8", cp932_inject,
	    sizeof(cp932_inject) - 1, cp932_expected,
	    sizeof(cp932_expected) - 1);
}

ATF_TC_WITHOUT_HEAD(mbconv_outerr);
ATF_TC_BODY(mbconv_outerr, tc)
{
	const char short_inject[] = "\xa3\x00\x00\x00";
	struct mbconv_tc_params tcp = {
		.tcp_src = "UTF-32LE",
		.tcp_dst = "GBK//TRANSLIT",
		.tcp_oscale = MB_LEN_MAX,
	};

	test_one(&tcp, 0, short_inject, sizeof(short_inject) - 1);
}


ATF_TC_WITHOUT_HEAD(mbconv_partial);
ATF_TC_BODY(mbconv_partial, tc)
{
	int err;
	const char utf_short_inject[] = "\xe0\xa5\x8d\xe0\xa4";
	const char gbk_short_inject[] = "Y\xa1";
	struct mbconv_tc_params tcp = {
		.tcp_src = "UTF-8",
		.tcp_dst = "UTF-16LE",
		.tcp_oerr = &err,
		.tcp_oscale = MB_LEN_MAX,
	};

	/* First with UTF-8, which is an "accelerated" encoding (mbtocsn) */
	test_one(&tcp, 0, utf_short_inject, sizeof(utf_short_inject) - 1);
	ATF_REQUIRE_EQ(EINVAL, err);

	/* Then with GBK, which uses the fallback path. */
	err = 0;
	tcp.tcp_src = "GBK";
	test_one(&tcp, 0, gbk_short_inject, sizeof(gbk_short_inject) - 1);
	ATF_REQUIRE_EQ(EINVAL, err);
}

ATF_TC_WITHOUT_HEAD(mbconv_serial_clobber);
ATF_TC_BODY(mbconv_serial_clobber, tc)
{
	const char short_inject[] = "\x83N";
	const char expected[] = "\xe3\x82\xaf\x0a";
	char *str;
	size_t outsz;
	struct mbconv_tc_params tcp = {
		.tcp_src = "SHIFT_JISX0213",
		.tcp_dst = "UTF-8",
		.tcp_ostr = &str,
		.tcp_osz = &outsz,
	};

	test_one(&tcp, 0, short_inject, sizeof(short_inject) - 1);

	ATF_REQUIRE(memcmp(expected, str, outsz) == 0);
	free(str);
}

ATF_TC_WITHOUT_HEAD(mbconv_statereset);
ATF_TC_BODY(mbconv_statereset, tc)
{
	const char inject[] = "Copyright \x83 ";
	const char expected[] = "Copyright ";
	char *str;
	size_t outsz;
	int err;
	struct mbconv_tc_params tcp = {
		.tcp_src = "CP949",
		.tcp_dst = "UTF-8",
		.tcp_oerr = &err,
		.tcp_ostr = &str,
		.tcp_osz = &outsz,
	};

	test_one(&tcp, 0, inject, sizeof(inject) - 1);
	ATF_REQUIRE_EQ(EILSEQ, err);

	ATF_REQUIRE(memcmp(expected, str, outsz) == 0);
	free(str);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, mbconv_simple);
	ATF_TP_ADD_TC(tp, mbconv_badcs);
	ATF_TP_ADD_TC(tp, mbconv_shortbuf);
	ATF_TP_ADD_TC(tp, mbconv_nulterm);
	ATF_TP_ADD_TC(tp, mbconv_parallel_cnt);
	ATF_TP_ADD_TC(tp, mbconv_serial_cnt);
	ATF_TP_ADD_TC(tp, mbconv_lenreset);
	ATF_TP_ADD_TC(tp, mbconv_multics);
	ATF_TP_ADD_TC(tp, mbconv_outerr);
	ATF_TP_ADD_TC(tp, mbconv_partial);
	ATF_TP_ADD_TC(tp, mbconv_serial_clobber);
	ATF_TP_ADD_TC(tp, mbconv_statereset);

	return (atf_no_error());
}

#else

int
main(int argc, char *argv[])
{

	/*
	 * Make sure that we're not accidentally running stubs on a platform
	 * where we really want to test libiconv.
	 */
	fprintf(stderr, "%s: built without test support\n", argv[0]);
	return (1);
}

#endif
