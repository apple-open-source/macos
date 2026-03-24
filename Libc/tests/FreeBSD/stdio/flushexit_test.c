/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Regression test for a race in stdio, where _cleanup() flushes all
 * streams without locking them.  This can lead to all sorts of corruption
 * if one thread calls exit() (which calls _cleanup()) and another
 * attempts a stream operation at roughly the same time.
 *
 * This test forks a child in which one thread calls exit(0) at the same
 * time as another closes a stream which had previously been created with
 * funopen().  If libc properly locks streams while flushing them on exit,
 * the exit() call will complete and the child will terminate.  If a race
 * is detected, the child will raise SIGABRT.  If any other issue arises,
 * the child will terminate with a non-zero exit code which corresponds to
 * the line number of the check that failed.
 */

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

static pthread_t close_thr, exit_thr, block_thr;
static int baton[2];

#define REQUIRE(cond) do { if (!(cond)) _Exit(__LINE__); } while (0)

static int
writefn(void *arg, const char *buf __unused, int len __unused)
{
	bool *isopen = arg;

	/*
	 * We should only be called from the exit thread.
	 */
	REQUIRE(pthread_self() == exit_thr);

	/*
	 * The exit thread has called exit(), which has called stdio's
	 * cleanup routine, which is flushing all streams.  Signal the
	 * close thread to close the stream.
	 */
	close(baton[1]);

	/*
	 * The close thread will now attempt to close the stream.  If libc
	 * is behaving correctly, this will block until after we've
	 * returned, so any attempt to synchronize with the close thread
	 * here would deadlock.  The best we can do is sleep a bit.
	 */
	usleep(100000);

	/*
	 * If the closer thread was not blocked, the stream is now closed.
	 */
	if (!*isopen)
		abort();

	return (0);
}

static int
closefn(void *arg)
{
	bool *isopen = arg;

	/*
	 * We should only be called from the close thread.
	 */
	REQUIRE(pthread_self() == close_thr);

	/*
	 * The stream should still be open.
	 */
	REQUIRE(*isopen);

	/*
	 * Mark the stream closed.
	 */
	*isopen = false;

	return (0);
}

static void *
close_main(void *arg)
{
	int tmp;

	/*
	 * Wait for the exit thread to start flushing streams.  When this
	 * happens, our write function will close the other end of the
	 * pipe and this read() call will return.
	 */
	REQUIRE(read(baton[0], &tmp, sizeof(tmp)) == 0);

	/*
	 * Now close the stream.  If libc is behaving correctly, this will
	 * block until the exit thread is done flushing streams.
	 */
	fclose(arg);

	/*
	 * Sleep a good long while; otherwise, we might beat the exit
	 * thread to the finish line.
	 */
	usleep(1000000);

	REQUIRE(false);
}

static void *
exit_main(void *arg __unused)
{
	exit(0);
}

static void *
block_main(void *arg __unused)
{
	FILE *f;
	int p[2], dummy;

	REQUIRE(pipe(p) == 0);
	REQUIRE((f = fdopen(p[0], "r")) != NULL);
	/* this should block forever */
	(void)fread(&dummy, sizeof(dummy), 1, f);
	REQUIRE(false);
}

static int __dead2
flushexit(void)
{
	static char buf[64];
	static char *msg = "Hello, world!\n";
	FILE *f;
	void *ret;
	bool isopen = true;

	/* create block thread and give it a moment */
	REQUIRE(pthread_create(&block_thr, NULL, block_main, NULL) == 0);
	usleep(1000000);

	/* create a stream and ensure that it needs to be flushed on exit */
	REQUIRE((f = funopen(&isopen, NULL, writefn, NULL, closefn)) != NULL);
	REQUIRE(setvbuf(f, buf, _IOFBF, sizeof(buf)) == 0);
	REQUIRE(fputs(msg, f) != EOF);

	/* create our synchronization pipe */
	REQUIRE(pipe(baton) == 0);

	/* create close and exit threads */
	REQUIRE(pthread_create(&close_thr, NULL, close_main, f) == 0);
	REQUIRE(pthread_create(&exit_thr, NULL, exit_main, NULL) == 0);
	(void)pthread_join(close_thr, &ret);
	(void)pthread_join(exit_thr, &ret);
	(void)pthread_join(block_thr, &ret);
	REQUIRE(false);
}

ATF_TC(flushexit);
ATF_TC_HEAD(flushexit, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test that flush-on-exit does not "
	    "flush locked streams or deadlock");
	atf_tc_set_md_var(tc, "timeout", "30");
}
ATF_TC_BODY(flushexit, tc)
{
	pid_t pid;

	if ((pid = atf_utils_fork()) == 0) {
		/* child */
		flushexit();
		REQUIRE(false);
	}
	atf_utils_wait(pid, 0, "", "");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, flushexit);

	return (atf_no_error());
}
