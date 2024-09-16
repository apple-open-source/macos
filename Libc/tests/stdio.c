#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <sys/sysctl.h>

#include <darwintest.h>

#define FILE_LIMIT 15

/*
 * Validate the maximum number of fds allowed open per process
 * implemented by the kernel matches what sysconf expects:
 *  32 bit: OPEN_MAX
 *  64 bit: RLIM_INFINITY
 */
T_DECL(stdio_PR_63187147_SC_STREAM_MAX, "_SC_STREAM_MAX test")
{
	struct rlimit rlim;
	long stream_max, saved_stream_max;
	int maxfilesperproc, err;
	size_t maxfilesperproc_size = sizeof(maxfilesperproc);

	T_SETUPBEGIN;

	saved_stream_max = sysconf(_SC_STREAM_MAX);
	T_LOG("Initial stream_max %ld", saved_stream_max);

	/*
	 * Determine the maximum number of fds allowed by the kernel
	 */
	err = sysctlbyname("kern.maxfilesperproc", &maxfilesperproc, &maxfilesperproc_size, NULL, 0);
	T_EXPECT_POSIX_SUCCESS(err, "sysctlbyname(\"kern.maxfilesperproc\") returned %d", err);
	T_LOG("kern.maxfilesperproc %d", maxfilesperproc);

	/*
	 * Raise RLIMIT_NOFILE to RLIM_INFINITY
	 */
	err = getrlimit(RLIMIT_NOFILE, &rlim);
	T_EXPECT_POSIX_SUCCESS(err, "getrlimit(RLIMIT_NOFILE)");
	T_LOG("Initial RLIMIT_NOFILE rlim.cur: 0x%llx", rlim.rlim_cur);
	rlim.rlim_cur = RLIM_INFINITY;
	err = setrlimit(RLIMIT_NOFILE, &rlim);
	T_EXPECT_POSIX_SUCCESS(err, "setrlimit(RLIMIT_NOFILE) to RLIM_INFINITY");
	err = getrlimit(RLIMIT_NOFILE, &rlim);
	T_EXPECT_POSIX_SUCCESS(err, "New RLIMIT_NOFILE rlim_cur: 0x%llx", rlim.rlim_cur);

	T_SETUPEND;

	/*
	 * The largest value sysconf returns for _SC_STREAM_MAX is
	 * OPEN_MAX (32 bit) or RLIM_INFINITY (64 bit)
	 */
	stream_max = sysconf(_SC_STREAM_MAX);
	T_EXPECT_NE_LONG((long)-1, stream_max, "stream_max %ld", stream_max);
#if __LP64__
	T_EXPECT_EQ((long)RLIM_INFINITY, stream_max, "sysconf returned 0x%lx", stream_max);
#else
	T_EXPECT_EQ((long)OPEN_MAX, stream_max, "sysconf returned 0x%lx", stream_max);
#endif
}

/*
 * Verify that a) {STREAM_MAX} reflects RLIMIT_NOFILE, b) we cannot open
 * more than {STREAM_MAX} streams (taking into account the pre-existing
 * stdin, stdout, stderr) and c) raising RLIMIT_NOFILE after hitting the
 * limit immediately lets us open additional streams up to the new limit.
 */
T_DECL(stdio_PR_22813396, "STREAM_MAX is affected by changes to RLIMIT_NOFILE")
{
	struct rlimit rlim;
	long i, stream_max;
	FILE *f;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_NOTNULL(f = fdopen(dup(0), "r"), "opening initial stream");
	T_ASSERT_POSIX_ZERO(getrlimit(RLIMIT_NOFILE, &rlim), "getting file limit");
	T_ASSERT_POSIX_SUCCESS(stream_max = sysconf(_SC_STREAM_MAX),
	    "getting stream limit");
	T_ASSERT_EQ_ULLONG((unsigned long long)stream_max, rlim.rlim_cur,
	    "stream limit equals file limit");
	rlim.rlim_cur = FILE_LIMIT;
	T_ASSERT_POSIX_ZERO(setrlimit(RLIMIT_NOFILE, &rlim), "setting file limit");
	T_ASSERT_POSIX_SUCCESS(stream_max = sysconf(_SC_STREAM_MAX),
	    "refreshing stream limit");
	T_ASSERT_EQ_LONG(stream_max, (long)FILE_LIMIT,
	    "stream limit equals file limit");
	T_SETUPEND;
	for (i = 4; i < stream_max; i++)
		T_EXPECT_POSIX_NOTNULL(fdopen(0, "r"), "opening stream within limit");
	T_EXPECT_NULL(fdopen(0, "r"), "opening stream beyond limit");
	T_SETUPBEGIN;
	rlim.rlim_cur = FILE_LIMIT + 1;
	T_ASSERT_POSIX_ZERO(setrlimit(RLIMIT_NOFILE, &rlim), "raising file limit");
	T_SETUPEND;
	T_EXPECT_POSIX_NOTNULL(fdopen(0, "r"), "opening stream after raising limit");
	T_EXPECT_NULL(fdopen(0, "r"), "opening stream beyond raised limit");
}

/*
 * Verify that a) {STREAM_MAX} reflects RLIMIT_NOFILE, b) we cannot open
 * more than {STREAM_MAX} streams (taking into account the pre-existing
 * stdin, stdout, stderr) and c) closing a stream after hitting the limit
 * immediately lets us open exactly one other.
 */
T_DECL(stdio_PR_22813396_close, "STREAM_MAX is enforced properly after fclose")
{
	struct rlimit rlim;
	long i, stream_max;
	FILE *f;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_NOTNULL(f = fdopen(dup(0), "r"), "opening initial stream");
	T_ASSERT_POSIX_ZERO(getrlimit(RLIMIT_NOFILE, &rlim), "getting file limit");
	T_ASSERT_POSIX_SUCCESS(stream_max = sysconf(_SC_STREAM_MAX),
	    "getting stream limit");
	T_ASSERT_EQ_ULLONG((unsigned long long)stream_max, rlim.rlim_cur,
	    "stream limit equals file limit");
	rlim.rlim_cur = FILE_LIMIT;
	T_ASSERT_POSIX_ZERO(setrlimit(RLIMIT_NOFILE, &rlim), "setting file limit");
	T_ASSERT_POSIX_SUCCESS(stream_max = sysconf(_SC_STREAM_MAX),
	    "refreshing stream limit");
	T_ASSERT_EQ_LONG(stream_max, (long)FILE_LIMIT,
	    "stream limit equals file limit");
	T_SETUPEND;
	for (i = 4; i < stream_max; i++)
		T_EXPECT_POSIX_NOTNULL(fdopen(0, "r"), "opening stream within limit");
	T_EXPECT_NULL(fdopen(0, "r"), "opening stream beyond limit");
	T_EXPECT_POSIX_ZERO(fclose(f), "closing a stream");
	T_EXPECT_POSIX_NOTNULL(fdopen(0, "r"), "opening stream after closing another");
	T_EXPECT_NULL(fdopen(0, "r"), "opening second stream after closing only one");
}
