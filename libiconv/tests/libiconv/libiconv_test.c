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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <TargetConditionals.h>

/*
 * On !macOS, we'll make this a stub that just writes to stderr and exits with
 * a failure.
 */
#if TARGET_OS_OSX

#include <sys/param.h>
#include <sys/queue.h>
#include <errno.h>

#include <atf-c.h>
#include <iconv.h>
#include <libcharset.h>
#include <locale.h>

/*
 * rdar://problem/89097907 - Stripping invalid UTF-8 sequences is failing.
 */
ATF_TC_WITHOUT_HEAD(test_sanitize);
ATF_TC_BODY(test_sanitize, tc)
{
	iconv_t cd;
	char str[] = "Tes\xe2\x28\xa1t";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, res;

	outsz = insz = sizeof(str) - 1;
	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	cd = iconv_open("UTF-8//IGNORE", "UTF-8");
	ATF_REQUIRE(cd != (iconv_t)-1);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)(iconv_t)-1);

	/*
	 * This conversion specifically should have stripped \xe2 and \xa1,
	 * leaving us with "Tes(t".
	 */
	ATF_REQUIRE_INTEQ(2, outsz);
	ATF_REQUIRE_STREQ("Tes(t", outbuf);

	iconv_close(cd);
}

/*
 * rdar://problem/89363266 - Test conversion from UTF-16 to UTF-8.
 */
ATF_TC_WITHOUT_HEAD(test_utf16);
ATF_TC_BODY(test_utf16, tc)
{
	iconv_t cd;
	char str[] = "\xfe\xff\x00T\x00\x65\x00s\x00t";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, res;

	insz = sizeof(str) - 1;
	outsz = 4;	/* "Test" */
	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	cd = iconv_open("UTF-8", "UTF-16");
	ATF_REQUIRE(cd != (iconv_t)-1);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)(iconv_t)-1);

	ATF_REQUIRE_INTEQ(0, outsz);
	ATF_REQUIRE_STREQ("Test", outbuf);

	iconv_close(cd);
}

/*
 * rdar://problem/120338891 - Test BOM consumption
 */
ATF_TC_WITHOUT_HEAD(test_utf16_bom);
ATF_TC_BODY(test_utf16_bom, tc)
{
	iconv_t cd;
	char outbuf[16];
	char str16be[] = "\xfe\xff\x00T\x00o\xff\xfe\x00s\x00s";
	char str16[] = "\xfe\xff\x00T\x00o\xff\xfes\x00s\x00";
	char *inptr, *outptr;
	size_t insz, outsz, res;

	/*
	 * BOMs in a UTF-16BE data stream should be interpreted literally.
	 */
	cd = iconv_open("UTF-8", "UTF-16BE");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inptr = &str16be[0];
	insz = sizeof(str16be) - 1;
	outptr = &outbuf[0];
	outsz = sizeof(outbuf);
	res = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)-1);
	ATF_REQUIRE(memcmp("\xef\xbb\xbfTo\xef\xbf\xbess", outbuf,
	    sizeof(outbuf) - outsz) == 0);

	iconv_close(cd);

	/*
	 * Make sure we didn't regress the UTF-16 case; a BOM may be included
	 * internally to flip the byte order in the remainder of the string.
	 */
	cd = iconv_open("UTF-8", "UTF-16");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inptr = &str16[0];
	insz = sizeof(str16) - 1;
	outptr = &outbuf[0];
	outsz = sizeof(outbuf);
	res = iconv(cd, &inptr, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)-1);
	ATF_REQUIRE(memcmp("Toss", outbuf,
	    sizeof(outbuf) - outsz) == 0);

	iconv_close(cd);
}

/*
 * rdar://problem/89485968 - mutt appears to want a UTF-8 -> UTF-8 conversion
 * and it thinks that output buffer shouldn't be touched with NULL input string
 * information.
 */
ATF_TC_WITHOUT_HEAD(test_mutt);
ATF_TC_BODY(test_mutt, tc)
{
	iconv_t cd;
	char outbuf[10], *outptr = outbuf;
	size_t insz, outsz, res;

	insz = 0;
	outsz = sizeof(outbuf) - 1;

	cd = iconv_open("UTF-8", "UTF-8");
	ATF_REQUIRE(cd != (iconv_t)-1);

	res = iconv(cd, NULL, 0, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)(iconv_t)-1);

	ATF_REQUIRE_INTEQ(sizeof(outbuf) - 1, outsz);
	ATF_REQUIRE_EQ(outptr, outbuf);

	iconv_close(cd);
}

/*
 * rdar://problem/89343731 - Test conversion from UCS-4-Internal to UTF-8
 */
ATF_TC_WITHOUT_HEAD(test_ucs4int);
ATF_TC_BODY(test_ucs4int, tc)
{
	iconv_t cd;
	char str[] =
	    "T\x00\x00\x00\x65\x00\x00\x00s\x00\x00\x00t\x00\x00\x00";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, res;

	insz = sizeof(str) - 1;
	outsz = 4;	/* "Test" */
	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	cd = iconv_open("UTF-8", "UCS-4-INTERNAL");
	ATF_REQUIRE(cd != (iconv_t)-1);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)(iconv_t)-1);

	ATF_REQUIRE_INTEQ(0, outsz);
	ATF_REQUIRE_STREQ("Test", outbuf);

	iconv_close(cd);
}

/*
 * rdar://problem/89321372 - Test conversion from UCS-2-Internal to UTF-8
 */
ATF_TC_WITHOUT_HEAD(test_ucs2int);
ATF_TC_BODY(test_ucs2int, tc)
{
	iconv_t cd;
	char str[] = "T\x00\x65\x00s\x00t\x00";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, res;

	insz = sizeof(str) - 1;
	outsz = 4;	/* "Test" */
	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	cd = iconv_open("UTF-8", "UCS-2-INTERNAL");
	ATF_REQUIRE(cd != (iconv_t)-1);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)(iconv_t)-1);

	ATF_REQUIRE_INTEQ(0, outsz);
	ATF_REQUIRE_STREQ("Test", outbuf);

	iconv_close(cd);
}

/*
 * rdar://problem/104482977 - libcharset should be exposed via libiconv
 */
ATF_TC_WITHOUT_HEAD(test_libcharset);
ATF_TC_BODY(test_libcharset, tc)
{
	const char *cs;

	/*
	 * This test is basically just a placeholder to force dyld to crash if
	 * we can't resolve this libcharset symbol against libiconv alone, or
	 * the linker to fail if libcharset isn't being re-exported by libiconv.
	 */
	cs = locale_charset();
	ATF_REQUIRE(cs != NULL);
	ATF_REQUIRE(*cs != '\0');
}

/*
 * rdar://problem/104607349 - wchar_t was not previously implemented, but it
 * should effectively be an alias for the currently set codeset, and the
 * respective buffers are instead treated as wchar_t arrays.
 *
 * rdar://problem/10633939 is also tested here, where wchar_t was implemented in
 * GNU libiconv but our build of it meant that the implementation would provide
 * incorrect output.
 */
ATF_TC_WITHOUT_HEAD(test_wchar_t);
ATF_TC_BODY(test_wchar_t, tc)
{
	const char utf8buf[] = "T\xe2\x82\xacT";
	const wchar_t wstr[] = { L'T', 0x20ac, L'T' };
	char tmpbuf[sizeof(wstr)];
	iconv_t cd;
	char *inbuf, *outbuf;
	size_t insz, outsz, res;

	setlocale(LC_ALL, "en_US.UTF-8");

	cd = iconv_open("WCHAR_T", "UTF-8");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inbuf = (char *)&utf8buf[0];
	insz = sizeof(utf8buf) - 1;
	outbuf = &tmpbuf[0];
	outsz = sizeof(tmpbuf);

	res = iconv(cd, &inbuf, &insz, &outbuf, &outsz);
	ATF_REQUIRE(res != (size_t)-1);

	ATF_REQUIRE_INTEQ(0, outsz);
	ATF_REQUIRE(memcmp(wstr, tmpbuf, sizeof(tmpbuf)) == 0);

	iconv_close(cd);

	/* Test the other way around now */
	cd = iconv_open("UTF-8", "WCHAR_T");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inbuf = (char *)&wstr[0];
	insz = sizeof(wstr);
	outbuf = &tmpbuf[0];
	outsz = sizeof(tmpbuf);

	res = iconv(cd, &inbuf, &insz, &outbuf, &outsz);
	ATF_REQUIRE(res != (size_t)-1);

	ATF_REQUIRE_INTEQ(sizeof(tmpbuf) - (sizeof(utf8buf) - 1), outsz);
	ATF_REQUIRE(memcmp(utf8buf, tmpbuf, sizeof(tmpbuf) - outsz) == 0);

	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(test_wchar_t_gb2312);
ATF_TC_BODY(test_wchar_t_gb2312, tc)
{
	const char gb2312buf[] = "T\xb5\xa2T";
	const wchar_t wstr[] = { L'T', 0xb5a2, L'T' };
	char tmpbuf[sizeof(wstr)];
	iconv_t cd;
	char *inbuf, *outbuf;
	size_t insz, outsz, res;

	setlocale(LC_ALL, "zh_CN.GB2312");

	cd = iconv_open("WCHAR_T", "GB2312");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inbuf = (char *)&gb2312buf[0];
	insz = sizeof(gb2312buf) - 1;
	outbuf = &tmpbuf[0];
	outsz = sizeof(tmpbuf);

	res = iconv(cd, &inbuf, &insz, &outbuf, &outsz);
	ATF_REQUIRE_MSG(res != (size_t)-1, "errno %d", errno);

	ATF_REQUIRE_INTEQ(0, outsz);
	ATF_REQUIRE(memcmp(wstr, tmpbuf, sizeof(tmpbuf)) == 0);

	iconv_close(cd);

	/* Test the other way around now */
	cd = iconv_open("GB2312", "WCHAR_T");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inbuf = (char *)&wstr[0];
	insz = sizeof(wstr);
	outbuf = &tmpbuf[0];
	outsz = sizeof(tmpbuf);

	res = iconv(cd, &inbuf, &insz, &outbuf, &outsz);
	ATF_REQUIRE(res != (size_t)-1);

	ATF_REQUIRE_INTEQ(sizeof(tmpbuf) - (sizeof(gb2312buf) - 1), outsz);
	ATF_REQUIRE(memcmp(gb2312buf, tmpbuf, sizeof(tmpbuf) - outsz) == 0);

	iconv_close(cd);
}

struct test_wchar_1t1_data {
	const wchar_t	*wdata;
	size_t		 idx;
	size_t		 maxidx;
};

static void
test_wchar_1t1_wchook(wchar_t wc, void *data)
{
	struct test_wchar_1t1_data *d1 = data;

	ATF_REQUIRE(d1->idx < d1->maxidx);
	ATF_REQUIRE_EQ(d1->wdata[d1->idx++], wc);
}

/*
 * rdar://problem/117243934 - wchar_t can just iconv_none and wc_hook should
 * still be invoked on every character.
 */
ATF_TC_WITHOUT_HEAD(test_wchar_1t1);
ATF_TC_BODY(test_wchar_1t1, tc)
{
	iconv_t cd;
	wchar_t wstr[] = { L'T', 0x20ac, L'T' };
	wchar_t outbuf[sizeof(wstr) / sizeof(wchar_t)];
	char *inbuf, *outptr;
	struct test_wchar_1t1_data d1 = {
	    .wdata = &wstr[0],
	    .idx = 0,
	    .maxidx = sizeof(wstr) / sizeof(wstr[0]),
	};
	struct iconv_hooks hooks = {
	    .wc_hook = &test_wchar_1t1_wchook,
	    .data = &d1,
	};
	size_t insz, outsz, res;

	setlocale(LC_ALL, "en_US.UTF-8");

	cd = iconv_open("WCHAR_T", "WCHAR_T");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inbuf = (char *)&wstr[0];
	insz = sizeof(wstr);
	outptr = (char *)&outbuf[0];
	outsz = sizeof(outbuf);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)-1);

	ATF_REQUIRE_INTEQ(0, outsz);
	ATF_REQUIRE(memcmp(wstr, outbuf, sizeof(outbuf)) == 0);

	/*
	 * We take a slightly different path through iconv_none when hooks are
	 * set, so let's make sure that's still correct and that the hooks are
	 * getting called when we expect and on the correct data.
	 */
	iconvctl(cd, ICONV_SET_HOOKS, &hooks);

	inbuf = (char *)&wstr[0];
	insz = sizeof(wstr);
	outptr = (char *)&outbuf[0];
	outsz = sizeof(outbuf);

	memset(&outbuf[0], 0, sizeof(outbuf));

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)-1);

	ATF_REQUIRE_INTEQ(0, outsz);
	ATF_REQUIRE(memcmp(wstr, outbuf, sizeof(outbuf)) == 0);

	ATF_REQUIRE_EQ(d1.maxidx, d1.idx);

	iconv_close(cd);
}

/*
 * Noticed while working on wchar_t, "char" should be case insensitive.  While
 * we're here, it should also yield UTF-8 by default.
 */
ATF_TC_WITHOUT_HEAD(test_char);
ATF_TC_BODY(test_char, tc)
{
	iconv_t cd;
	char str[] = "\xe4";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, res;

	insz = sizeof(str) - 1;
	outsz = 4;	/* UTF-8's MB_CUR_MAX */
	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	cd = iconv_open("CHAR", "L1");
	ATF_REQUIRE(cd != (iconv_t)-1);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res != (size_t)(iconv_t)-1);

	ATF_REQUIRE_INTEQ(2, outsz);
	ATF_REQUIRE_STREQ("\xc3\xa4", outbuf);

	iconv_close(cd);
}

/*
 * rdar://problem/104448984 - bad conversions shouldn't cause a program abort,
 * they should simply fail.
 */
ATF_TC_WITHOUT_HEAD(test_utf8mac);
ATF_TC_BODY(test_utf8mac, tc)
{
	iconv_t cd;
	char str[] = "\xf0\x9f\xa5\x91";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, res;

	insz = sizeof(str) - 1;
	outsz = 16;	/* Arbitrary; this will error out. */
	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	cd = iconv_open("UTF-8", "UTF-8-MAC");
	ATF_REQUIRE(cd != (iconv_t)-1);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == (size_t)(iconv_t)-1);

	iconv_close(cd);
}

#define	SHORT_BUF_SIZE	8

/*
 * rdar://problem/105248919 - Providing a too short buffer for UTF-8-MAC should
 * result in E2BIG.
 */
ATF_TC_WITHOUT_HEAD(test_utf8mac_short);
ATF_TC_BODY(test_utf8mac_short, tc)
{
	iconv_t cd;
	char str[] = "/path/to/something/important";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, res;

	insz = sizeof(str) - 1;
	outsz = SHORT_BUF_SIZE;
	_Static_assert(SHORT_BUF_SIZE < sizeof(str) - 1,
	    "Short buffer too large");

	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	errno = 0;
	cd = iconv_open("UTF-8-MAC", "UTF-8");
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(errno == 0);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == (size_t)(iconv_t)-1);
	ATF_REQUIRE(errno == E2BIG);

	iconv_close(cd);
}

/*
 * rdar://problem/45790933 - Conversions ending in the middle of a multi-byte
 * sequence should also result in an EINVAL, which will be retried.
 */
ATF_TC_WITHOUT_HEAD(test_utf8mac_midshort);
ATF_TC_BODY(test_utf8mac_midshort, tc)
{
	iconv_t cd;
	char str[] = "Test\xe2\x80\x94String";
	char *inbuf = str, *outbuf, *outptr;
	size_t insz, outsz, origoutsz, res, seqend;

	origoutsz = outsz = sizeof(str) * 2;

	outptr = outbuf = malloc(outsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	errno = 0;
	cd = iconv_open("UTF-8", "UTF-8-MAC");
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(errno == 0);

	/* First pass an insz that lands us just after \xe2 */
	insz = strchr(str, '\xe2') - str + 1;
	seqend = strchr(str, '\x94') - str + 1;
	for (size_t next = insz; next < seqend; next++) {
		inbuf = str;
		insz = next;
		outptr = outbuf;
		outsz = origoutsz;

		iconv(cd, NULL, NULL, NULL, NULL);
		res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
		ATF_REQUIRE(res == (size_t)(iconv_t)-1);
		ATF_REQUIRE(errno == EINVAL);
	}

	/*
	 * Prepare for one last conversion, includes the end of the sequence we
	 * had broken up so it should succeed now.
	 */
	inbuf = str;
	insz = seqend;
	outptr = outbuf;
	outsz = origoutsz;

	iconv(cd, NULL, NULL, NULL, NULL);
	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == 0);
	ATF_REQUIRE(memcmp(outbuf, str, origoutsz - outsz) == 0);
	iconv_close(cd);
}

struct enc {
	LIST_ENTRY(enc)		 entries;
	const char		*name;
};

struct enclist {
	LIST_HEAD(, enc)	 head;
	size_t			 nenc;
};

static int
test_open_all_collect_encodings(unsigned int count, const char * const *names,
    void *data)
{
	struct enc *next;
	struct enclist *encl;

	if (names == NULL)
		return (0);

	encl = (struct enclist *)data;
	next = calloc(1, sizeof(*next));
	ATF_REQUIRE(next != NULL);

	next->name = strdup(names[0]);
	ATF_REQUIRE(next->name != NULL);

	LIST_INSERT_HEAD(&encl->head, next, entries);
	++encl->nenc;

	return (0);
}

ATF_TC_WITHOUT_HEAD(test_open_all);
ATF_TC_BODY(test_open_all, tc)
{
	struct enclist encl;
	struct enc *n1, *n2;
	iconv_t cd;
	int success;

	memset(&encl, '\0', sizeof(encl));
	LIST_INIT(&encl.head);

	iconvlist(&test_open_all_collect_encodings, &encl);

	/* Test each one against all of the others. */
	LIST_FOREACH(n1, &encl.head, entries) {
		success = 0;
		LIST_FOREACH(n2, &encl.head, entries) {
			/*
			 * We want to try each encoding as both the src and
			 * dest, to be sure, so do it.
			 */
			errno = 0;
			cd = iconv_open(n2->name, n1->name);
			if (cd == (iconv_t)-1) {
				/*
				 * We should audit
				 */
				continue;
			}

			success++;
			ATF_REQUIRE(errno == 0);
			ATF_REQUIRE(iconv_close(cd) == 0);
		}

		ATF_REQUIRE_MSG(success != 0, "%s failed", n1->name);
	}
}

/*
 * rdar://problem/113828035 - We may encounter a sequence that's valid in the
 * src encoding, but not in the dst encoding.  GNU libiconv seems to replace
 * these with the "invalid" character regardless of //IGNORE, so make sure we do
 * the same.
 */
ATF_TC_WITHOUT_HEAD(test_eilseq_out);
ATF_TC_BODY(test_eilseq_out, tc)
{
	iconv_t cd;
	char first_str[] = "\xc2\xff\xff\xff";
	char second_str[] = "X\x00\x00\x00\xc2\xff\xff\xffY\x00\x00\x00";
	char *inbuf, *outbuf, *outptr;
	size_t insz, outbufsz, outsz, res;

	outbufsz = 7;	/* Largest possible UTF-8 sequence + 1 ("X") */
	outptr = outbuf = malloc(outbufsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	cd = iconv_open("UTF-8", "UCS-4LE");
	ATF_REQUIRE(cd != (iconv_t)-1);

	/*
	 * First try it with the invalid sequence being at the beginning of our
	 * conversion.
	 */
	inbuf = first_str;
	insz = sizeof(first_str) - 1;
	outptr = outbuf;
	outsz = outbufsz;
	memset(outptr, '\0', outsz);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == 0);
	/* Should have consumed the entire input buffer. */
	ATF_REQUIRE_INTEQ(0, insz);

	/*
	 * At the moment, our invalid sequence character for UTF-8 is a simple
	 * "?", so we should have just used up one character.
	 */
	ATF_REQUIRE_INTEQ(outbufsz - 1, outsz);
	ATF_REQUIRE_STREQ("?", outbuf);

	/* Now we try it with valid characters around it. */
	inbuf = second_str;
	insz = sizeof(second_str) - 1;
	outptr = outbuf;
	outsz = outbufsz;
	memset(outptr, '\0', outsz);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == 0);
	ATF_REQUIRE_INTEQ(0, insz);

	ATF_REQUIRE_INTEQ(outbufsz - 3, outsz);
	ATF_REQUIRE_STREQ("X?Y", outbuf);

	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(test_eilseq_out_all);
ATF_TC_BODY(test_eilseq_out_all, tc)
{
	struct enclist encl;
	struct enc *n1;
	char str[] = "\xc2\xff\xff\xff";
	iconv_t cd;
	char *inptr, *outbuf, *outptr;
	size_t insz, outbufsz, outsz, res;

	outbufsz = sizeof(str) * MB_LEN_MAX;
	outbuf = malloc(outbufsz + 1);
	ATF_REQUIRE(outbuf != NULL);

	memset(&encl, '\0', sizeof(encl));
	LIST_INIT(&encl.head);

	iconvlist(&test_open_all_collect_encodings, &encl);

	/* Test each one against all of the others. */
	LIST_FOREACH(n1, &encl.head, entries) {
		errno = 0;
		cd = iconv_open(n1->name, "UCS-4LE");
		if (cd == (iconv_t)-1) {
			/*
			 * We should audit
			 */
			continue;
		}

		inptr = &str[0];
		outptr = &outbuf[0];
		insz = sizeof(str) - 1;
		outsz = outbufsz;
		res = iconv(cd, &inptr, &insz, &outptr, &outsz);

		/*
		 * This should either succeed, yield 1 invalid character, or
		 * error.
		 *
		 * In testing, an assertion was hit in some scenarios.
		 */
		ATF_REQUIRE((ssize_t)res == -1 || res <= 1);
		if ((ssize_t)res == -1) {
			/*
			 * We really just want ENOENT for all of the encodings
			 * that don't have a designated invalid character to
			 * replace the invalid character with, but some will use
			 * citrus_none with a character that's way out of range;
			 * these are likely bogus in some fashion, but for now
			 * we'll just accept it.
			 *
			 * In testing, EINVAL was observed in some scenarios.
			 */
			ATF_REQUIRE(errno == ENOENT || errno == EILSEQ);
		}

		ATF_REQUIRE(iconv_close(cd) == 0);
	}
}

/*
 * rdar://problem/115429361 - gettext and others broken because this libiconv
 * defaults to transliteration (euro symbol -> "EUR")
 */
ATF_TC_WITHOUT_HEAD(test_autoconf);
ATF_TC_BODY(test_autoconf, tc)
{
	iconv_t cd;
	char str[] = "\xe2\x82\xac";
	char *inbuf = str, outbuf[20], *outptr = outbuf;
	size_t insz, outsz, res;
	int on;

	insz = sizeof(str) - 1;
	outsz = sizeof(outbuf) - 1;

	errno = 0;
	cd = iconv_open("ISO-8859-1", "UTF-8");
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(errno == 0);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == (size_t)-1);
	ATF_REQUIRE(errno == EILSEQ);

	/* Reset and try with transliteration. */
	iconv(cd, NULL, NULL, NULL, NULL);
	on = 1;
	iconvctl(cd, ICONV_SET_TRANSLITERATE, &on);

	insz = sizeof(str) - 1;
	inbuf = str;
	outsz = sizeof(outbuf) - 1;
	outptr = &outbuf[0];
	memset(outbuf, '\0', sizeof(outbuf));
	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == 1);
	ATF_REQUIRE(strcmp(outbuf, "EUR") == 0);
	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(test_translit);
ATF_TC_BODY(test_translit, tc)
{
	iconv_t cd;
	char instr[] = "\x00\x00\x00y\x00\x00\x00\xa0\x00\x00\x00\xa9";
	char outbuf[sizeof(instr) + 4];
	const char expectedbuf[] = "y (c)";
	char *inbuf, *outptr;
	size_t insz, outbufsz, outsz, res;

	outbufsz = sizeof(outbuf) - 1;
	outbuf[outbufsz] = '\0';

	/* Try it with no transliteration first. */
	cd = iconv_open("ASCII", "UTF-32BE");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inbuf = instr;
	insz = sizeof(instr) - 1;
	outptr = outbuf;
	outsz = outbufsz;
	memset(outptr, '\0', outsz);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == -1);
	ATF_REQUIRE_INTEQ(EILSEQ, errno);

	iconv_close(cd);

	/*
	 * Next, try it with transliteration.  Return value should reflect
	 * two invalid conversions to match our two sequences requiring
	 * transliteration.
	 */
	cd = iconv_open("ASCII//TRANSLIT", "UTF-32BE");
	ATF_REQUIRE(cd != (iconv_t)-1);

	inbuf = instr;
	insz = sizeof(instr) - 1;
	outptr = outbuf;
	outsz = outbufsz;
	memset(outptr, '\0', outsz);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == 2);
	/* Should have consumed the whole buffer. */
	ATF_REQUIRE_INTEQ(0, insz);

	ATF_REQUIRE_STREQ(expectedbuf, outbuf);

	iconv_close(cd);
}

/*
 * rdar://problem/115458746 - the "GNU behavior" that libiconv implements should
 * still increment inval count if we hit a character that we can't map into the
 * destination encoding.
 */
ATF_TC_WITHOUT_HEAD(test_invalid_ignore);
ATF_TC_BODY(test_invalid_ignore, tc)
{
	iconv_t cd;
	char str[] = "\xe2\x82\xac Z";
	char *inbuf = str, outbuf[20], *outptr = outbuf;
	size_t insz, outsz, res;
	int on;

	insz = sizeof(str) - 1;
	outsz = sizeof(outbuf) - 1;
	memset(outbuf, '\0', sizeof(outbuf));

	errno = 0;
	cd = iconv_open("ISO-8859-1//IGNORE", "UTF-8");
	ATF_REQUIRE(cd != (iconv_t)-1);
	ATF_REQUIRE(errno == 0);

	/* This is the default now, but to be sure... */
	on = 1;
	iconvctl(cd, ICONV_SET_ILSEQ_INVALID, &on);

	res = iconv(cd, &inbuf, &insz, &outptr, &outsz);
	ATF_REQUIRE(res == 1);

	ATF_REQUIRE(strcmp(outbuf, " Z") == 0);

	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(test_utf16_unpaired_surrogates);
ATF_TC_BODY(test_utf16_unpaired_surrogates, tc)
{
	const char *inpairs[] = {
	    "a\x00\x00\xdcg\x00",	/* Low surrogate with no prior high. */
	    "a\x00\x00\xd8g\x00",	/* High surrogate with no low. */
	    NULL,
	};
	char outbuf[16];
	size_t inpairsz = 6;	/* 3 UTF-16 characters */
	char *inptr, *outptr;
	size_t insz, outsz, ret;
	iconv_t cd;

	cd = iconv_open("UTF-8", "UTF-16LE");
	ATF_REQUIRE(cd != (iconv_t)-1);

	for (const char **buf = &inpairs[0]; *buf != NULL; buf++) {
		/* A little ugly, but we won't be changing it. */
		inptr = (char *)(uintptr_t)*buf;
		insz = inpairsz;
		outptr = &outbuf[0];
		outsz = sizeof(outbuf);

		iconv(cd, NULL, NULL, NULL, NULL);

		ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
		ATF_REQUIRE(ret == (size_t)-1);
		ATF_REQUIRE(errno == EILSEQ);

		/* We only processed 'a' */
		ATF_REQUIRE(inpairsz - insz == 2);
		ATF_REQUIRE(sizeof(outbuf) - outsz == 1);
		ATF_REQUIRE(strncmp(outbuf, "a", 1) == 0);
	}

	iconv_close(cd);
}

ATF_TC_WITHOUT_HEAD(test_utf7_ascii);
ATF_TC_BODY(test_utf7_ascii, tc)
{
	static const char direct_excl[] = "~\\+ ";
	static const char utf7_spaces[] = " \r\n\t";
	iconv_t cd;
	char *inptr, *outptr;
	size_t insz, outsz, ret;
	char str[2] = {0, 0};
	char outbuf[2];

	cd = iconv_open("UTF-8", "UTF-7");
	ATF_REQUIRE(cd != (iconv_t)-1);

	for (size_t testsz = 1; testsz <= 2; testsz++) {
		for (int i = 0; i < 0x80; i++) {
			str[0] = i;
			str[1] = 0;

			iconv(cd, NULL, NULL, NULL, NULL);

			inptr = &str[0];
			insz = testsz;
			outptr = &outbuf[0];
			outsz = sizeof(outbuf);

			ret = iconv(cd, &inptr, &insz, &outptr, &outsz);
			if (testsz == 1) {
				if (isalnum(i)) {
					/* Directly convertible */
					ATF_REQUIRE(ret != (size_t)-1);
					ATF_REQUIRE(insz == 0);
				} else if (isprint(i) &&
				    strchr(direct_excl, i) == NULL) {
					/* Optional direct */
					ATF_REQUIRE(ret != (size_t)-1);
					ATF_REQUIRE(insz == 0);
				} else if (i > 0 &&
				    strchr(utf7_spaces, i) != NULL) {
					ATF_REQUIRE(ret != (size_t)-1);
					ATF_REQUIRE(insz == 0);
				} else {
					ATF_REQUIRE(ret == (size_t)-1);
				}
			} else {
				ATF_REQUIRE(ret == (size_t)-1);
			}

			/* We should never overflow anything. */
			ATF_REQUIRE(inptr - &str[0] <= sizeof(str));
			ATF_REQUIRE(outptr - &outbuf[0] <= sizeof(outbuf));
		}
	}

	iconv_close(cd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, test_sanitize);
	ATF_TP_ADD_TC(tp, test_utf16);
	ATF_TP_ADD_TC(tp, test_utf16_bom);
	ATF_TP_ADD_TC(tp, test_mutt);
	ATF_TP_ADD_TC(tp, test_ucs4int);
	ATF_TP_ADD_TC(tp, test_ucs2int);
	ATF_TP_ADD_TC(tp, test_libcharset);
	ATF_TP_ADD_TC(tp, test_wchar_t);
	ATF_TP_ADD_TC(tp, test_wchar_t_gb2312);
	ATF_TP_ADD_TC(tp, test_wchar_1t1);
	ATF_TP_ADD_TC(tp, test_char);
	ATF_TP_ADD_TC(tp, test_utf8mac);
	ATF_TP_ADD_TC(tp, test_utf8mac_short);
	ATF_TP_ADD_TC(tp, test_utf8mac_midshort);
	ATF_TP_ADD_TC(tp, test_open_all);
	ATF_TP_ADD_TC(tp, test_eilseq_out);
	ATF_TP_ADD_TC(tp, test_eilseq_out_all);
	ATF_TP_ADD_TC(tp, test_autoconf);
	ATF_TP_ADD_TC(tp, test_translit);
	ATF_TP_ADD_TC(tp, test_invalid_ignore);
	ATF_TP_ADD_TC(tp, test_utf16_unpaired_surrogates);
	ATF_TP_ADD_TC(tp, test_utf7_ascii);
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
