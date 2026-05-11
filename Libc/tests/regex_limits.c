#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <darwintest.h>
#include <darwintest_utils.h>

/* see ${SRCTOP}/regex/TRE/lib/tre-internal.h */
#define TRE_MAX_RE	65536U
#define TRE_MAX_STRING	0x7fffffffUL

#define MIN_SHIFT 2
#define MAX_SHIFT 32

T_DECL(regex_limits, "regcomp() and regexec() segfault when input exceeds INT_MAX",
    T_META_ENABLED(TARGET_OS_OSX), /* macOS only, consumes large amounts of memory */
    T_META_NAMESPACE("Libc.regression"))
{
	regmatch_t pm[9];
	regex_t preg;
	size_t npm = sizeof(pm) / sizeof(pm[0]);
	size_t size;
	char *buf;
	int error, expect, shift;

	T_SETUPBEGIN;
	setlocale(LC_NUMERIC, "en_US.UTF-8");
	size = 1ULL << MAX_SHIFT;
	T_ASSERT_NE(0, size, NULL);
	T_ASSERT_NOTNULL(buf = malloc(size + 1), NULL);
	memset(buf, 'a', size);
	buf[size] = '\0';
	T_SETUPEND;

	for (shift = MIN_SHIFT; shift <= MAX_SHIFT; shift++) {
		size = 1ULL << shift;
		T_LOG("2^%d == %'zu", shift, size);
		buf[0] = buf[size - 1] = 'x';
		buf[size] = '\0';

		/* expected outcome of regcomp() for a regex this size */
		expect = size <= TRE_MAX_RE ? 0 : REG_ESPACE;
		T_LOG("expecting %d from regcomp()", expect);

		/* compile a BRE size characters long */
		T_EXPECT_EQ_INT(expect, error = regcomp(&preg, buf, REG_BASIC), NULL);
		if (error == 0)
			regfree(&preg);

		/* compile an ERE size characters long */
		T_EXPECT_EQ_INT(expect, error = regcomp(&preg, buf, REG_EXTENDED), NULL);
		if (error == 0)
			regfree(&preg);

		/* expected outcome of regexec() for a string this size */
		expect = size <= TRE_MAX_STRING ? 0 : REG_NOMATCH;
		T_LOG("expecting %d from regexec()", expect);

		/* match a BRE with a string size characters long */
		T_SETUPBEGIN;
		T_ASSERT_EQ_INT(0, regcomp(&preg, "x\\(aa*\\)x", REG_BASIC), NULL);
		T_SETUPEND;
		T_EXPECT_EQ_INT(expect, error = regexec(&preg, buf, npm, pm, 0), NULL);
		if (error == 0) {
			T_EXPECT_EQ(0, pm[0].rm_so, NULL);
			T_EXPECT_EQ(size, (size_t)pm[0].rm_eo, NULL);
			T_EXPECT_EQ(1, pm[1].rm_so, NULL);
			T_EXPECT_EQ(size - 1, (size_t)pm[1].rm_eo, NULL);
		}
		regfree(&preg);

		/* match an ERE with a string size characters long */
		T_SETUPBEGIN;
		T_ASSERT_EQ_INT(0, regcomp(&preg, "x(a+)x", REG_EXTENDED), NULL);
		T_SETUPEND;
		T_EXPECT_EQ_INT(expect, error = regexec(&preg, buf, npm, pm, 0), NULL);
		if (error == 0) {
			T_EXPECT_EQ(0, pm[0].rm_so, NULL);
			T_EXPECT_EQ(size, (size_t)pm[0].rm_eo, NULL);
			T_EXPECT_EQ(1, pm[1].rm_so, NULL);
			T_EXPECT_EQ(size - 1, (size_t)pm[1].rm_eo, NULL);
		}
		regfree(&preg);

		buf[0] = buf[size - 1] = buf[size] = 'a';
	}

	free(buf);
}
