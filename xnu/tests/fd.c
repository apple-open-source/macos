#include <darwintest.h>
#include <darwintest_utils.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/fileport.h>
#include <sys/fcntl.h>
#include <mach/mach.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.fd"),
	T_META_RUN_CONCURRENTLY(true));

static void *
fd_select_close_helper(void *ctx)
{
	int fd = *(int *)ctx;

	// wait for the thread to enter select
	usleep(500000);
	close(fd);

	return NULL;
}

T_DECL(fd_select_close, "Test for 54795873: make sure close breaks out of select", T_META_TAG_VM_PREFERRED)
{
	fd_set read_fd;
	int pair[2], rc;
	pthread_t th;

	rc = socketpair(PF_LOCAL, SOCK_STREAM, 0, pair);
	T_ASSERT_POSIX_SUCCESS(rc, "socketpair");

	pthread_create(&th, NULL, fd_select_close_helper, pair);

	FD_ZERO(&read_fd);
	FD_SET(pair[0], &read_fd);

	rc = select(pair[0] + 1, &read_fd, NULL, NULL, NULL);
	T_EXPECT_POSIX_FAILURE(rc, EBADF, "select broke out with EBADF");
}

T_DECL(fd_select_ebadf, "Test that select on closed fd returns EBADF", T_META_TAG_VM_PREFERRED)
{
	fd_set read_fd;
	int pair[2], rc;

	rc = socketpair(PF_LOCAL, SOCK_STREAM, 0, pair);
	T_ASSERT_POSIX_SUCCESS(rc, "socketpair");

	FD_ZERO(&read_fd);
	FD_SET(pair[0], &read_fd);

	T_ASSERT_POSIX_SUCCESS(close(pair[0]), "close(%d)", pair[0]);

	rc = select(pair[0] + 1, &read_fd, NULL, NULL, NULL);
	T_EXPECT_POSIX_FAILURE(rc, EBADF, "select returned with EBADF");
}

static void *
fd_stress_dup2_close_fun(void *ctx)
{
	int thno = (int)(long)ctx;
	int count = 10000, rc;

	for (int i = 1; i <= count; i++) {
		rc = dup2(STDIN_FILENO, 42);
		T_QUIET; T_EXPECT_POSIX_SUCCESS(rc, "dup2(%d, 42)", STDIN_FILENO);
		if (thno == 3) {
			rc = close(42);
			if (rc == -1) {
				T_QUIET; T_EXPECT_POSIX_FAILURE(rc, EBADF, "close(42)");
			}
		}
		if (i % 1000 == 0) {
			T_LOG("thread %d: %d/%d dups\n", thno, i, count);
		}
	}

	return NULL;
}

T_DECL(fd_stress_dup2_close, "Stress test races between dup2 and close", T_META_TAG_VM_PREFERRED)
{
	pthread_t th[4];
	int rc;

	for (int i = 0; i < 4; i++) {
		rc = pthread_create(&th[i], NULL,
		    fd_stress_dup2_close_fun, (void *)(long)i);
		T_ASSERT_POSIX_ZERO(rc, "pthread_create");
	}

	for (int i = 0; i < 4; i++) {
		pthread_join(th[i], NULL);
	}
}

T_DECL(fd_dup2_erase_clofork_58446996,
    "Make sure dup2() doesn't inherit flags from an old fd", T_META_TAG_VM_PREFERRED)
{
	int fd1, fd2;

	fd1 = open("/dev/null", O_RDONLY | O_CLOEXEC);
	T_ASSERT_POSIX_SUCCESS(fd1, "open(/dev/null)");

	fd2 = open("/dev/null", O_RDONLY | O_CLOEXEC);
	T_ASSERT_POSIX_SUCCESS(fd2, "open(/dev/null)");

	T_ASSERT_POSIX_SUCCESS(dup2(fd1, fd2), "dup2(fd1, fd2)");
	T_EXPECT_EQ(fcntl(fd2, F_GETFD, 0), 0,
	    "neither FD_CLOEXEC nor FD_CLOFORK should be set");
}

struct confined_race_state {
	int fd;
	bool made;
};

static void *
confine_thread(void *data)
{
	volatile int *fdp = data;

	for (;;) {
		fcntl(*fdp, F_SETCONFINED, 1);
	}

	return NULL;
}

T_DECL(confined_fileport_race, "test for rdar://69922255", T_META_TAG_VM_PREFERRED)
{
	int fd = -1;
	pthread_t t;
	mach_port_t p = MACH_PORT_NULL;

	T_ASSERT_POSIX_SUCCESS(pthread_create(&t, NULL, confine_thread, &fd),
	    "pthread_create");

	for (int i = 0; i < 100 * 1000; i++) {
		fd = open("/dev/null", O_RDONLY | 0x08000000 /* O_CLOFORK */);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(fd, "open(/dev/null)");
		if (fileport_makeport(fd, &p) == 0) {
			T_QUIET; T_ASSERT_EQ(fcntl(fd, F_GETCONFINED), 0,
			    "should never get a confined fd: %d", fd);
			mach_port_deallocate(mach_task_self(), p);
		}

		close(fd);
	}
}
