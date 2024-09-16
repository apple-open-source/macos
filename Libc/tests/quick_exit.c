#include <sys/wait.h>

#include <stdlib.h>
#include <unistd.h>

#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

static void func_a(void)
{
	if (write(STDOUT_FILENO, "a", 1) != 1)
		_Exit(1);
}

static void func_b(void)
{
	if (write(STDOUT_FILENO, "b", 1) != 1)
		_Exit(1);
}

static void func_c(void)
{
	if (write(STDOUT_FILENO, "c", 1) != 1)
		_Exit(1);
}

static void child(void)
{
	// this will be received by the parent
	printf("hello, ");
	fflush(stdout);
	// this won't, because quick_exit() does not flush
	printf("world");
	// these will be called in reverse order, producing "abc"
	if (at_quick_exit(func_c) != 0 ||
	    at_quick_exit(func_b) != 0 ||
	    at_quick_exit(func_a) != 0)
		_Exit(1);
	quick_exit(0);
}

T_DECL(quick_exit, "Test quick_exit and at_quick_exit")
{
	char buf[100] = "";
	ssize_t len;
	int p[2], wstatus = 0;
	pid_t pid;

	T_ASSERT_POSIX_SUCCESS(pipe(p), NULL);
	pid = fork();
	if (pid == 0) {
		if (dup2(p[1], STDOUT_FILENO) < 0)
			_Exit(1);
		(void)close(p[1]);
		(void)close(p[0]);
		child();
		_Exit(1);
	}
	T_ASSERT_POSIX_SUCCESS(pid, "expect fork() to succeed");
	T_EXPECT_EQ(waitpid(pid, &wstatus, 0), pid, "expect to collect child process");
	T_EXPECT_EQ(wstatus, 0, "expect child to exit cleanly");
	T_EXPECT_POSIX_SUCCESS(len = read(p[0], buf, sizeof(buf)), NULL);
	T_EXPECT_EQ_STR(buf, "hello, abc", NULL);
}
