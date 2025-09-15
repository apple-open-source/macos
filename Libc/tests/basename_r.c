#include <sys/param.h>

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <darwintest.h>
#include <darwintest_utils.h>

#define TOOLONG (MAXPATHLEN + 16)

T_DECL(basename_r_null,  "Test basename_r(3) NULL handling")
{
	char *ret, buf[32];
	pid_t pid;
	int exitcode, sig;

	/* NULL argument handling  */
	ret = basename_r(NULL, buf);
	T_EXPECT_EQ_STR(".", ret, "basename_r(NULL, buf)");

	sig = 0;
	pid = fork();
	if (pid == 0) {
		ret = basename_r(NULL, NULL);
		_Exit(0);
	}
	dt_waitpid(pid, &exitcode, &sig, 5);
	T_ASSERT_EQ(sig, SIGSEGV, "basename_r(NULL, NULL)");

	sig = 0;
	pid = fork();
	if (pid == 0) {
		ret = dirname_r("./foo/bar", NULL);
		_Exit(0);
	}
	dt_waitpid(pid, &exitcode, &sig, 5);
	T_ASSERT_EQ(sig, SIGSEGV, "basename_r('./foo/bar', NULL)");
}

T_DECL(basename_r_pathlen, "Test basename_r(3) MAXPATHLEN handling")
{
	char buf[TOOLONG + 1] = {0};
	char dst[TOOLONG + 1] = {0};

	/* MAXPATHLEN handling */
	memset(buf, 'a', TOOLONG);
	T_ASSERT_NULL(basename_r(buf, dst), NULL);
	T_ASSERT_EQ(errno, ENAMETOOLONG, NULL);

	buf[TOOLONG - MAXPATHLEN - 1] = '/';
	T_ASSERT_NULL(basename_r(buf, dst), NULL);
	T_ASSERT_EQ(errno, ENAMETOOLONG, NULL);
	buf[TOOLONG - MAXPATHLEN - 1] = 'a';

	buf[TOOLONG - MAXPATHLEN] = '/';
	T_ASSERT_NOTNULL(basename_r(buf, dst), NULL);
}
