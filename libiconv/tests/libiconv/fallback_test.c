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

#include <sys/cdefs.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <TargetConditionals.h>

/*
 * On !macOS, we'll make this a stub that just writes to stderr and exits with
 * a failure.
 */
#if TARGET_OS_OSX

#include <iconv.h>

#include <atf-c.h>

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

static void
mb_to_uc_fail(const char *a __unused, size_t b __unused,
    void (*c)(const unsigned int *, size_t, void *) __unused,
    void *d __unused, void *e __unused)
{

	atf_tc_fail("fallback called but no fallback expected");
}

static void
uc_to_mb_fail(unsigned int a __unused, void (*b)(const char *, size_t, void *) __unused,
    void *c __unused, void *d __unused)
{

	atf_tc_fail("fallback called but no fallback expected");
}

static void
mb_to_wc_fail(const char *a __unused, size_t b __unused,
    void (*c)(const wchar_t *, size_t, void *) __unused, void *d __unused,
    void *e __unused)
{

	atf_tc_fail("fallback called but no fallback expected");
}

static void
wc_to_mb_fail(wchar_t a __unused, void (*b)(const char *, size_t, void *) __unused,
    void *c __unused, void *d __unused)
{

	atf_tc_fail("fallback called but no fallback expected");
}

ATF_TC_WITHOUT_HEAD(test_fallback_none);
ATF_TC_BODY(test_fallback_none, tc)
{
	iconv_t cd;
	struct iconv_fallbacks ivf = {
	    .mb_to_uc_fallback = &mb_to_uc_fail,
	    .uc_to_mb_fallback = &uc_to_mb_fail,
	    .mb_to_wc_fallback = &mb_to_wc_fail,
	    .wc_to_mb_fallback = &wc_to_mb_fail,
	};
	char inbuf[16], outbuf[16];
	char *inptr, *outptr;
	size_t insz, outsz, ret;

	ATF_REQUIRE_STREQ("UTF-8", setlocale(LC_CTYPE, "UTF-8"));

	cd = iconv_open("Shift_JISX0213", "CP932");
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, &ivf) == 0);

	insz = snprintf(inbuf, sizeof(inbuf), "\xa3");
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(ret == 0);

	iconv_close(cd);

	cd = iconv_open("Shift_JISX0213", "WCHAR_T");
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, &ivf) == 0);

	*(wchar_t *)inbuf = L'A';
	insz = sizeof(wchar_t);
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(ret == 0);

	iconv_close(cd);

#ifdef CITRUS
	cd = iconv_open("WCHAR_T", "CP932");
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, &ivf) == 0);

	insz = snprintf(inbuf, sizeof(inbuf), "A");
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(ret == 0);

	iconv_close(cd);
#endif
}

static void
uc_to_mb_euro(unsigned int uc, void (*write_out)(const char *, size_t, void *),
    void *icdata, void *ourdata)
{

	ATF_REQUIRE(!*(bool *)ourdata);
	ATF_REQUIRE(uc == 0x20ac /* Euro symbol */);
	write_out("@!", 2, icdata);
}

static void
wc_to_mb_euro(wchar_t wc, void (*write_out)(const char *, size_t, void *),
    void *icdata, void *ourdata)
{

	ATF_REQUIRE(!*(bool *)ourdata);
	ATF_REQUIRE(wc == 0xffffff /* Bogus */);
	write_out("@!", 2, icdata);
}

#define	CONFIGSTR(x)	x, (sizeof(x) - 1)
struct test_config {
	struct iconv_fallbacks	ivf;
	const char	*from;
	const char	*to;
	size_t		 inval;

	const char	*input;
	size_t		 inputsz;

	const char	*plain;
	size_t		 plainsz;

	const char	*translit;
	size_t		 translitsz;

	const char	*drop;
	size_t		 dropsz;

	/*
	 * Input for transliteration and later.
	 */
	const char	*input2;
	size_t		 input2sz;
};

struct test_config uc_mb_test_config = {
	{
		.uc_to_mb_fallback = &uc_to_mb_euro,
		.wc_to_mb_fallback = &wc_to_mb_euro,
	},
	"UTF-8", "ASCII", 1,
	CONFIGSTR("50\xe2\x82\xacK"),	/* input */
	CONFIGSTR("50@!K"),		/* plain */
	CONFIGSTR("50EURK"),		/* translit */
	CONFIGSTR("50K"),		/* drop */
};

/*
 * Goes through the uc_to_mb fallback, since 0x20ac is a valid wchar_t
 * in the src locale.
 */
wchar_t wc_mb_wc1[] = { L'5', L'0', 0x20ac, L'K' };
struct test_config wc_mb_test_config1 = {
	{
		.uc_to_mb_fallback = &uc_to_mb_euro,
		.wc_to_mb_fallback = &wc_to_mb_euro,
	},
	"WCHAR_T", "ASCII", 1,
	(const char *)&wc_mb_wc1, sizeof(wc_mb_wc1),	/* input */
	CONFIGSTR("50@!K"),				/* plain */
	CONFIGSTR("50EURK"),				/* translit */
	CONFIGSTR("50K"),				/* drop */
};

wchar_t wc_mb_wc2[] = { L'5', L'0', 0xffffff, L'K' };
struct test_config wc_mb_test_config2 = {
	{
		.uc_to_mb_fallback = &uc_to_mb_euro,
		.wc_to_mb_fallback = &wc_to_mb_euro,
	},
	"WCHAR_T", "ASCII", 1,
	(const char *)&wc_mb_wc2, sizeof(wc_mb_wc2),	/* input */
	CONFIGSTR("50@!K"),				/* plain */
	CONFIGSTR("50EURK"),				/* translit */
	CONFIGSTR("50K"),				/* drop */
	(const char *)&wc_mb_wc1, sizeof(wc_mb_wc1),	/* input2 */
};

static void
test_one(struct test_config *tc)
{
	iconv_t cd;
	char inbuf[32], outbuf[32];
	char *inptr, *outptr;
	size_t insz, outsz, ret;
	int off = 0, on = 1;
	bool fallback_disabled = false;

	ATF_REQUIRE(sizeof(inbuf) >= tc->inputsz);

	tc->ivf.data = &fallback_disabled;
	cd = iconv_open(tc->to, tc->from);
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, &tc->ivf) == 0);

	/*
	 * No transliteration, so this should fail.
	 */
	memcpy(inbuf, tc->input, tc->inputsz);
	insz = tc->inputsz;
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(ret == tc->inval);
	ATF_REQUIRE(insz == 0);
	ATF_REQUIRE(sizeof(outbuf) - outsz == tc->plainsz);
	ATF_REQUIRE(strncmp(outbuf, tc->plain, tc->plainsz) == 0);

	/* Now try again with transliteration enabled */
	(void)iconv(cd, NULL, NULL, NULL, NULL);
	iconvctl(cd, ICONV_SET_TRANSLITERATE, &on);
	if (tc->input2 != NULL) {
		memcpy(inbuf, tc->input2, tc->input2sz);
		insz = tc->input2sz;
	} else {
		insz = inptr - &inbuf[0];
	}
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	fallback_disabled = true;
	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	fallback_disabled = false;
	/* Still an invalid character, but successfully transliterated. */
	ATF_REQUIRE(ret == tc->inval);
	ATF_REQUIRE(insz == 0);
	ATF_REQUIRE(sizeof(outbuf) - outsz == tc->translitsz);
	ATF_REQUIRE(strncmp(outbuf, tc->translit, tc->translitsz) == 0);

	/*
	 * Finally, turn transliteration back off and make sure we drop the character
	 * entirely with //IGNORE.
	 */
	(void)iconv(cd, NULL, NULL, NULL, NULL);
	iconvctl(cd, ICONV_SET_TRANSLITERATE, &off);
	iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, &on);

	insz = inptr - &inbuf[0];
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	fallback_disabled = true;
	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	fallback_disabled = false;
	ATF_REQUIRE(ret == tc->inval);
	ATF_REQUIRE(insz == 0);
	ATF_REQUIRE(sizeof(outbuf) - outsz == tc->dropsz);	/* Leading + Trailing characters */
	ATF_REQUIRE(strncmp(outbuf, tc->drop, tc->dropsz) == 0);

	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(test_fallback_to_mb);
ATF_TC_BODY(test_fallback_to_mb, tc)
{

	ATF_REQUIRE_STREQ("UTF-8", setlocale(LC_CTYPE, "UTF-8"));
	test_one(&uc_mb_test_config);
	test_one(&wc_mb_test_config1);
	test_one(&wc_mb_test_config2);
}

static void
mb_to_uc_badeuro(const char *mb, size_t mbsz,
    void (*write_one)(const unsigned int *, size_t, void *),
    void *icdata, void *ourdata)
{
	unsigned int replacement[] = { '$' };

	ATF_REQUIRE(!*(bool *)ourdata);
	ATF_REQUIRE(mbsz == 1);
	ATF_REQUIRE(*mb == '\xe2' || *mb == '\x82');
	write_one(&replacement[0], nitems(replacement), icdata);
}

static void
mb_to_uc_shiftjis(const char *mb, size_t mbsz,
    void (*write_one)(const unsigned int *, size_t, void *),
    void *icdata, void *ourdata)
{
	unsigned int replacement[] = { '$' };

	ATF_REQUIRE(!*(bool *)ourdata);
	ATF_REQUIRE(mbsz == 1);
	ATF_REQUIRE(*mb == '\x80');
	write_one(&replacement[0], nitems(replacement), icdata);
}

/* Tests mbtocsx failure */
struct test_config mb_uc_test_config1 = {
	{
		.mb_to_uc_fallback = &mb_to_uc_badeuro,
	},
	"UTF-8", "ASCII", 2,
	CONFIGSTR("50\xe2\x82K"),	/* input */
	CONFIGSTR("50$$K"),		/* plain */
	CONFIGSTR(""),			/* translit (unused) */
	CONFIGSTR("50K"),		/* drop */
};
/* do_conv failure */
struct test_config mb_uc_test_config2 = {
	{
		.mb_to_uc_fallback = &mb_to_uc_shiftjis,
	},
	"Shift_JISX0213", "ASCII", 1,
	CONFIGSTR("50\x80K"),		/* input */
	CONFIGSTR("50$K"),		/* plain */
	CONFIGSTR(""),			/* translit (unused) */
	CONFIGSTR("50K"),		/* drop */
};

static void
test_one_mbfail(struct test_config *tc)
{
	iconv_t cd;
	char inbuf[32], outbuf[32];
	char *inptr, *outptr;
	size_t insz, outsz, ret;
	int off = 0, on = 1;
	bool fallback_disabled = false;

	ATF_REQUIRE(sizeof(inbuf) >= tc->inputsz);

	tc->ivf.data = &fallback_disabled;
	cd = iconv_open(tc->to, tc->from);
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, &tc->ivf) == 0);

	/*
	 * No transliteration, so this should fail.
	 */
	memcpy(inbuf, tc->input, tc->inputsz);
	insz = tc->inputsz;
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(ret == tc->inval);
	ATF_REQUIRE(insz == 0);
	ATF_REQUIRE(sizeof(outbuf) - outsz == tc->plainsz);
	ATF_REQUIRE(strncmp(outbuf, tc->plain, tc->plainsz) == 0);

	/*
	 * Finally, turn transliteration back off and make sure we drop the character
	 * entirely with //IGNORE.
	 */
	(void)iconv(cd, NULL, NULL, NULL, NULL);
	iconvctl(cd, ICONV_SET_TRANSLITERATE, &off);
	iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, &on);

	insz = inptr - &inbuf[0];
	inptr = &inbuf[0];
	outsz = sizeof(outbuf);
	outptr = &outbuf[0];

	fallback_disabled = true;
	ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
	fallback_disabled = false;
	ATF_REQUIRE(ret == 0);
	ATF_REQUIRE(insz == 0);
	ATF_REQUIRE(sizeof(outbuf) - outsz == tc->dropsz);	/* Leading + Trailing characters */
	ATF_REQUIRE(strncmp(outbuf, tc->drop, tc->dropsz) == 0);

	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(test_fallback_from_mb);
ATF_TC_BODY(test_fallback_from_mb, tc)
{

	ATF_REQUIRE_STREQ("UTF-8", setlocale(LC_CTYPE, "UTF-8"));
	test_one_mbfail(&mb_uc_test_config1);
	test_one_mbfail(&mb_uc_test_config2);
}

ATF_TC_WITHOUT_HEAD(test_fallback_clear);
ATF_TC_BODY(test_fallback_clear, tc)
{
	struct iconv_fallbacks ivf = {
		.uc_to_mb_fallback = &uc_to_mb_euro,
		.wc_to_mb_fallback = &wc_to_mb_euro,
	};
	iconv_t cd;
	char outbuf[32];
	char *inptr, *outptr;
	size_t insz, outsz;
	bool fallback_disabled;

	fallback_disabled = false;

	cd = iconv_open("ASCII", "WCHAR_T");
	ATF_REQUIRE(cd != (iconv_t)-1);

	/* No harm in clearing it right off the bat. */
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, NULL) == 0);

	ivf.data = &fallback_disabled;
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, &ivf) == 0);

	inptr = (void *)&wc_mb_wc2[0];
	insz = sizeof(wc_mb_wc2);
	outptr = &outbuf[0];
	outsz = sizeof(outbuf);
	/* Should invoke fallback */
	ATF_REQUIRE(iconv(cd, &inptr, &insz, &outptr, &outsz) == 1);
	ATF_REQUIRE(strncmp(outbuf, "50@!K", sizeof("50@!K") - 1) == 0);

	/* Clear the fallback and try again.  Should fail. */
	ATF_REQUIRE(iconvctl(cd, ICONV_SET_FALLBACKS, NULL) == 0);
	inptr = (void *)&wc_mb_wc2[0];
	insz = sizeof(wc_mb_wc2);
	outptr = &outbuf[0];
	outsz = sizeof(outbuf);
	memset(outptr, 0, outsz);

	/*
	 * We set fallback_disabled to detect if it didn't clear; the fallback
	 * would get invoked and fail when it tests ivf.data.
	 */
	fallback_disabled = true;
	ATF_REQUIRE(iconv(cd, &inptr, &insz, &outptr, &outsz) == -1);
	ATF_REQUIRE_EQ(EILSEQ, errno);
	/* It should have halted output right before we would've written @!. */
	ATF_REQUIRE(strncmp(outbuf, "50", 3) == 0);
	iconv_close(cd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, test_fallback_none);
	ATF_TP_ADD_TC(tp, test_fallback_to_mb);
	ATF_TP_ADD_TC(tp, test_fallback_from_mb);
	ATF_TP_ADD_TC(tp, test_fallback_clear);
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
