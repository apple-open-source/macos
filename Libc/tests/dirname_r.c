#include <sys/param.h>

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <darwintest.h>
#include <darwintest_utils.h>

#define TOOLONG (MAXPATHLEN + 16)

T_DECL(dirname_r_null, "Test dirname_r(3) NULL handling")
{
	char *ret, buf[32];
	pid_t pid;
	int exitcode, sig;

	/* NULL argument handling  */
	ret = dirname_r(NULL, buf);
	T_ASSERT_EQ_STR(".", ret, "dirname_r(NULL, buf)");

	sig = 0;
	pid = fork();
	if (pid == 0) {
		ret = dirname_r(NULL, NULL);
		_Exit(0);
	}
	dt_waitpid(pid, &exitcode, &sig, 5);
	T_ASSERT_EQ(sig, SIGSEGV, "dirname_r(NULL, NULL)");

	sig = 0;
	pid = fork();
	if (pid == 0) {
		ret = dirname_r("./foo/bar", NULL);
		_Exit(0);
	}
	dt_waitpid(pid, &exitcode, &sig, 5);
	T_ASSERT_EQ(sig, SIGSEGV, "dirname_r('./foo/bar', NULL)");
}

T_DECL(dirname_r_pathlen, "Test dirname_r(3) MAXPATHLEN handling")
{
	char buf[TOOLONG + 1] = { 0 };
	char dst[TOOLONG + 1] = { 0 };

	/* MAXPATHLEN handling */
	memset(buf, 'a', TOOLONG);
	buf[TOOLONG - 2] = '/';
	T_ASSERT_NULL(dirname_r(buf, dst), NULL);
	buf[TOOLONG - 2] = 'a';

	buf[MAXPATHLEN] = '/';
	T_ASSERT_NULL(dirname_r(buf, dst), NULL);
	T_ASSERT_EQ(errno, ENAMETOOLONG, NULL);

	buf[MAXPATHLEN - 1] = '/';
	T_ASSERT_NOTNULL(dirname_r(buf, dst), NULL);
}
