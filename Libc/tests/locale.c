#include <TargetConditionals.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <xlocale.h>
#include <os/lock.h>
#include <sys/wait.h>

#include <darwintest.h>

#if TARGET_OS_OSX
T_DECL(locale_PR_23679075, "converts a cyrillic a to uppercase")
{
	locale_t loc = newlocale(LC_COLLATE_MASK|LC_CTYPE_MASK, "ru_RU", 0);
	T_ASSERT_NOTNULL(loc, "newlocale(LC_COLLATE_MASK|LC_CTYPE_MASK, \"ru_RU\", 0) should return a locale");

	T_ASSERT_EQ(towupper_l(0x0430, loc), 0x0410, NULL);
	freelocale(loc);
}

T_DECL(locale_PR_24165555, "swprintf with Russian chars")
{
    setlocale(LC_ALL, "ru_RU.UTF-8");

    wchar_t buffer[256];
    T_EXPECT_POSIX_SUCCESS(swprintf(buffer, 256, L"%ls", L"English: Hello World"), "English");
    T_EXPECT_POSIX_SUCCESS(swprintf(buffer, 256, L"%ls", L"Russian: ру́сский язы́к"), "Russian");

    setlocale(LC_ALL, "");
}

T_DECL(locale_PR_28774201, "return code on bad locale")
{
    T_EXPECT_NULL(newlocale(LC_COLLATE_MASK | LC_CTYPE_MASK, "foobar", NULL), NULL);
    T_EXPECT_EQ(errno, ENOENT, NULL);
}

static void *hammer_snprintf(void *hammer)
{
	while(*(bool *)hammer) {
		char buf[3];
		snprintf(buf, sizeof(buf), "%s%s", "a", "b");
	}

	return NULL;
}

#define HOW_MANY_FORKS 10000

T_DECL(locale_PR_91564505, "locale lock should be reset on fork")
{
	bool hammer = true;
	pthread_t thread;
	pthread_create(&thread, NULL, hammer_snprintf, &hammer);

	int i;
	for (i = 0; i < HOW_MANY_FORKS; i++) {
		pid_t pid = fork();
		if (pid < 0) {
			T_FAIL("fork() returned %d", pid);
		}

		if (pid) {
			int stat_loc;
			waitpid(pid, &stat_loc, 0);
			if(WIFSIGNALED(stat_loc)) {
				T_FAIL("child crashed post-fork pre-exec");
				break;
			}
		} else {
			exit(0);
		}
	}

	hammer = false;
	pthread_join(thread, NULL);

	T_EXPECT_EQ(i, HOW_MANY_FORKS, "We made it through %d forks without a post-fork pre-exec crash", i);
}

static void
test_one_wc(wchar_t wc)
{
	char buf[MB_LEN_MAX];
	mbstate_t mbs = { 0 };
	size_t ret;

	/* Always using a fresh mbstate */
	ret = wcrtomb(&buf[0], wc, &mbs);

	T_QUIET;
	T_ASSERT_LE(ret, MB_CUR_MAX, "wc %x bad", wc);
}

static void
test_one_packed(unsigned char upper, unsigned char lower)
{
	wchar_t wc = (upper << 8) | lower;
	wchar_t rev;
	size_t off;
	int exp, ret;
	char buf[3];

	off = 0;
	if (upper != 0)
		buf[off++] = upper;
	buf[off++] = lower;
	buf[off] = 0;

	test_one_wc(wc);

	if (upper == 0 && lower == 0)
		exp = 0;
	else
		exp = off;

	ret = mbtowc(&rev, &buf[0], off);

	T_QUIET;
	T_ASSERT_EQ(ret, exp, NULL);
}

static void
test_packed(void)
{
	wchar_t wc;

	/* G0 (US-ASCII or ISO646-JP) */
	for (unsigned char l = 0x00; l <= 0x7f; l++) {
		test_one_packed(0, l);
	}

	/* G1 (JISX0208 / JISC6226) */
	for (unsigned char u = 0xa1; u <= 0xfe; u++) {
		for (unsigned char l = 0xa1; l <= 0xfe; l++) {
			test_one_packed(u, l);
		}
	}

	/* G3 (JISX0212) */
	for (unsigned char u = 0xa1; u <= 0xfe; u++) {
		for (unsigned char l = 0x21; l <= 0x7e; l++) {
			test_one_packed(u, l);
		}
	}
}

/* locale/FreeBSD/mblocal.h */
#define	SS2	0x8e
#define	SS3	0x8f

/*
 * Test SS2, SS3 under the current locale.
 */
static void
test_cjk_cntrl(unsigned int cntrl, size_t bytes)
{
	char buf[MB_LEN_MAX];
	wchar_t end, rev, wc;
	unsigned int shift;
	int exp, ret;

	T_QUIET;
	T_ASSERT_EQ(cntrl & ~0xff, 0, NULL);
	T_QUIET;
	T_ASSERT_GT(bytes, 1, NULL);
	T_QUIET;
	T_ASSERT_LT(bytes, 5, NULL);

	shift = (8 * (bytes - 1));
	wc = (cntrl << shift);
	end = (1 << shift);

	for (wchar_t next = 0; next < end; next++) {
		wc = (wc & ~(end - 1)) | next;

		test_one_wc(wc);

		for (size_t i = 0; i < bytes; i++) {
			buf[i] = (wc >> (8 * (bytes - (i + 1)))) & 0xff;
		}

		if (wc == 0)
			exp = 0;
		else
			exp = bytes;

		ret = mbtowc(&rev, &buf[0], bytes);

		T_QUIET;
		T_ASSERT_EQ(ret, exp, NULL);
		T_QUIET;
		T_ASSERT_EQ(rev, wc, NULL);
	}
}

/*
 * euc-cw, euc-kr == base, first byte >= 0xa1 == 2 byte
 * euc-jp == base + SS2, 2 bytes; SS3, 3 bytes
 * euc-tw == base + SS2, 4 bytes
 */
T_DECL(locale_cjk, "Test each codeset that should be usable in eucCN, eucJP, eucKR")
{
	const char *locales[] = { "zh_CN.eucCN", "ja_JP.eucJP", "ko_KR.eucKR" };
	const char *cloc;

	for (size_t i = 0; i < sizeof(locales) / sizeof(locales[0]); i++) {
		cloc = locales[i];

		T_ASSERT_EQ_STR(cloc, setlocale(LC_CTYPE, cloc),
		    "setlocale(LC_CTYPE, %s)", cloc);

		T_LOG("Basic packed tests");
		test_packed();

		/* Avoid the legacy check & noise; no extended tests. */
		if (MB_CUR_MAX == 2)
			continue;

		T_LOG("Running extended tests");

		if (strstr(cloc, "eucTW") != NULL) {
			/*
			 * We don't have any eucTW yet, but for completeness
			 * it's included here.
			 */
			test_cjk_cntrl(SS2, 4);
		} else if (strstr(cloc, "eucJP") != NULL) {
			test_cjk_cntrl(SS2, 2);
			test_cjk_cntrl(SS3, 3);
		}
	}
}
#endif
