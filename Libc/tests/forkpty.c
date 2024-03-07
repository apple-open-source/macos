#include <sys/resource.h>
#include <sys/wait.h>

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>

#include <darwintest.h>
#include <TargetConditionals.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

T_DECL(forkpty_forkfail,
    "Check for fd leak when fork() fails",
    T_META_CHECK_LEAKS(false),
    T_META_ENABLED(TARGET_OS_OSX))
{
	struct rlimit orl, nrl;
	struct passwd *pwd;
	pid_t pid;
	int prevfd, fd, pty;

	T_SETUPBEGIN;
	if (geteuid() == 0) {
		/* the setrlimit() trick won't work if we're root */
		T_ASSERT_NOTNULL(pwd = getpwnam("nobody"), NULL);
		T_ASSERT_POSIX_SUCCESS(setgid(pwd->pw_gid), NULL);
		T_ASSERT_POSIX_SUCCESS(setuid(pwd->pw_uid), NULL);
	}
	T_ASSERT_POSIX_SUCCESS(getrlimit(RLIMIT_NPROC, &orl), NULL);
	nrl = orl;
	nrl.rlim_cur = 1;
	T_ASSERT_POSIX_SUCCESS(setrlimit(RLIMIT_NPROC, &nrl), NULL);
	T_ASSERT_POSIX_SUCCESS(fd = dup(0), NULL);
	T_ASSERT_POSIX_SUCCESS(close(fd), NULL);
	T_SETUPEND;
	pid = forkpty(&pty, NULL, NULL, NULL);
	if (pid == 0) {
		/* child - fork() unexpectedly succeeded */
		_exit(0);
	}
	T_EXPECT_POSIX_FAILURE(pid, EAGAIN, "expected fork() to fail");
	if (pid > 0) {
		/* parent - fork() unexpectedly succeeded */
		(void)waitpid(pid, NULL, 0);
	}
	prevfd = fd;
	T_ASSERT_POSIX_SUCCESS(fd = dup(0), NULL);
	T_EXPECT_EQ(fd, prevfd, "expected same fd as previously");
}
