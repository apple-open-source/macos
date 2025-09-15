/* compile: xcrun -sdk macosx.internal clang -arch arm64e -arch x86_64 -ldarwintest -o test_aio aio.c */

#include <darwintest.h>
#include <darwintest_utils.h>
#include <darwintest_multiprocess.h>
#include <aio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/sysctl.h>

#include "test_utils.h"


#ifndef SIGEV_KEVENT
#define SIGEV_KEVENT    4
#endif

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.file_descriptors.aio"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("file descriptors"),
	T_META_CHECK_LEAKS(false),
	T_META_TAG_VM_PREFERRED);


#define AIO_TESTFILE        "aio_testfile"
#define AIO_BUFFER_SIZE     (1024 * 1024)
#define AIO_BUFFER_PATTERN  0x20190912
#define AIO_LIST_MAX        4

static char *g_testfiles[AIO_LIST_MAX];
static int g_fds[AIO_LIST_MAX];
static struct aiocb g_aiocbs[AIO_LIST_MAX];
static char *g_buffers[AIO_LIST_MAX];

/*
 * This unit-test tests AIO (Asynchronous I/O) facility.
 */


static void
exit_cleanup(void)
{
	for (int i = 0; i < AIO_LIST_MAX; i++) {
		if (g_fds[i] > 0) {
			close(g_fds[i]);
		}
		if (g_testfiles[i]) {
			(void)remove(g_testfiles[i]);
		}
		if (g_buffers[i]) {
			free(g_buffers[i]);
		}
	}
}

static void
do_init(int num_files, bool enable_nocache)
{
	const char *tmpdir = dt_tmpdir();
	int i, err;

	T_SETUPBEGIN;

	atexit(exit_cleanup);

	T_QUIET;
	T_ASSERT_LE(num_files, AIO_LIST_MAX, "too many files");

	for (i = 0; i < AIO_LIST_MAX; i++) {
		g_fds[i] = -1;
		g_testfiles[i] = NULL;
		g_buffers[i] = NULL;
	}

	for (i = 0; i < num_files; i++) {
		T_WITH_ERRNO;
		g_testfiles[i] = malloc(MAXPATHLEN);
		T_QUIET;
		T_ASSERT_NE(g_testfiles[i], NULL, "Allocate path buffer %d size %d",
		    i, MAXPATHLEN);

		snprintf(g_testfiles[i], MAXPATHLEN, "%s/%s.%d",
		    tmpdir, AIO_TESTFILE, i);

		T_WITH_ERRNO;
		g_fds[i] = open(g_testfiles[i], O_CREAT | O_RDWR, 0666);
		T_ASSERT_NE(g_fds[i], -1, "Create test fi1e: %s", g_testfiles[i]);

		T_WITH_ERRNO;
		g_buffers[i] = malloc(AIO_BUFFER_SIZE);
		T_QUIET;
		T_ASSERT_NE(g_buffers[i], NULL, "Allocate data buffer %d size %d",
		    i, AIO_BUFFER_SIZE);
		memset(g_buffers[i], AIO_BUFFER_PATTERN, AIO_BUFFER_SIZE);

		if (enable_nocache) {
			T_WITH_ERRNO;
			err = fcntl(g_fds[i], F_NOCACHE, 1);
			T_ASSERT_NE(err, -1, "Set F_NOCACHE: %s", g_testfiles[i]);
		}
	}

	T_SETUPEND;
}

static struct aiocb *
init_aiocb(int idx, off_t offset, int lio_opcode)
{
	struct aiocb *aiocbp;

	aiocbp = &g_aiocbs[idx];
	memset(aiocbp, 0, sizeof(struct aiocb));
	aiocbp->aio_fildes = g_fds[idx];
	aiocbp->aio_offset = offset;
	aiocbp->aio_buf = g_buffers[idx];
	aiocbp->aio_nbytes = AIO_BUFFER_SIZE;
	aiocbp->aio_lio_opcode = lio_opcode;

	return aiocbp;
}

static int
poll_aio_error(struct aiocb *aiocbp)
{
	int err;

	while (1) {
		err = aio_error(aiocbp);
		if (err != EINPROGRESS) {
			break;
		}
		usleep(10000);
	}

	return err;
}

static int
wait_for_kevent(int kq, struct kevent64_s *kevent)
{
	struct timespec timeout = {.tv_sec = 10, .tv_nsec = 0};

	return kevent64(kq, NULL, 0, kevent, 1, 0, &timeout);
}

static int
verify_buffer_data(struct aiocb *aiocbp, uint32_t pattern)
{
	char *buf_to_verify;
	int err = 0;

	buf_to_verify = malloc(aiocbp->aio_nbytes);
	if (!buf_to_verify) {
		err = ENOMEM;
		goto out;
	}
	memset(buf_to_verify, pattern, aiocbp->aio_nbytes);

	err = memcmp((const void *)aiocbp->aio_buf, (const void *)buf_to_verify,
	    aiocbp->aio_nbytes);
	free(buf_to_verify);

out:
	return err;
}

/*
 * Test aio_write() and aio_read().
 * Poll with aio_error() for AIO completion and call aio_return() to retrieve
 * return status of AIO operation.
 */
T_DECL(write_read, "Test aio_write() and aio_read(). Poll for AIO completion")
{
	struct aiocb *aiocbp;
	ssize_t retval;
	int err;

	do_init(1, true);

	/* Setup aiocb for aio_write(). */
	aiocbp = init_aiocb(0, 0, 0);

	T_WITH_ERRNO;
	err = aio_write(aiocbp);
	T_ASSERT_NE(err, -1, "aio_write() for fd %d offset 0x%llx length 0x%zx",
	    aiocbp->aio_fildes, aiocbp->aio_offset, aiocbp->aio_nbytes);

	T_WITH_ERRNO;
	err = poll_aio_error(aiocbp);
	T_ASSERT_NE(err, -1, "aio_error() for aiocbp %p", aiocbp);

	T_WITH_ERRNO;
	retval = aio_return(aiocbp);
	T_ASSERT_EQ((int)retval, AIO_BUFFER_SIZE,
	    "aio_return() for aiocbp %p bytes_written 0x%zx", aiocbp, retval);

	memset((void *)aiocbp->aio_buf, 0, AIO_BUFFER_SIZE);

	T_WITH_ERRNO;
	err = aio_read(aiocbp);
	T_ASSERT_NE(err, -1, "aio_read() for fd %d offset 0x%llx length 0x%zx",
	    aiocbp->aio_fildes, aiocbp->aio_offset, aiocbp->aio_nbytes);

	T_WITH_ERRNO;
	err = poll_aio_error(aiocbp);
	T_ASSERT_NE(err, -1, "aio_error() for aiocbp %p", aiocbp);

	T_WITH_ERRNO;
	retval = aio_return(aiocbp);
	T_ASSERT_EQ((int)retval, AIO_BUFFER_SIZE,
	    "aio_return() for aiocbp %p bytes_read 0x%zx", aiocbp, retval);

	err = verify_buffer_data(aiocbp, AIO_BUFFER_PATTERN);
	T_ASSERT_EQ(err, 0, "verify data returned from aio_read()");
}

/*
 * Test aio_write() and aio_fsync().
 * Poll with aio_error() for AIO completion and call aio_return() to retrieve
 * return status of AIO operation.
 */
T_DECL(write_fsync, "Test aio_write() and aio_fsync(). Poll for AIO completion.")
{
	struct aiocb *aiocbp;
	ssize_t retval;
	int err;

	do_init(1, false);

	/* Setup aiocb for aio_write(). */
	aiocbp = init_aiocb(0, (1024 * 1024), 0);

	T_WITH_ERRNO;
	err = aio_write(aiocbp);
	T_ASSERT_NE(err, -1, "aio_write() for fd %d offset 0x%llx length 0x%zx",
	    aiocbp->aio_fildes, aiocbp->aio_offset, aiocbp->aio_nbytes);

	T_WITH_ERRNO;
	err = poll_aio_error(aiocbp);
	T_ASSERT_NE(err, -1, "aio_error() for aiocbp %p", aiocbp);

	T_WITH_ERRNO;
	retval = aio_return(aiocbp);
	T_ASSERT_EQ((int)retval, AIO_BUFFER_SIZE,
	    "aio_return() for aiocbp %p bytes_written 0x%zx", aiocbp, retval);

	T_WITH_ERRNO;
	err = aio_fsync(O_SYNC, aiocbp);
	T_ASSERT_NE(err, -1, "aio_fsync() for aiocbp %p", aiocbp);

	T_WITH_ERRNO;
	err = poll_aio_error(aiocbp);
	T_ASSERT_NE(err, -1, "aio_error() for aiocbp %p", aiocbp);

	T_WITH_ERRNO;
	err = aio_return(aiocbp);
	T_ASSERT_EQ(err, 0, "aio_return() for aiocbp %p", aiocbp);
}

/*
 * Test aio_write() and aio_suspend().
 * Suspend with aio_suspend() until AIO completion and call aio_return() to
 * retrieve return status of AIO operation.
 */
T_DECL(write_suspend, "Test aio_write() and aio_suspend(). Suspend until AIO completion.")
{
	struct aiocb *aiocbp, *aiocb_list[AIO_LIST_MAX];
	struct timespec timeout;
	ssize_t retval;
	int err;

	do_init(1, false);

	/* Setup aiocb for aio_write(). */
	aiocbp = init_aiocb(0, (128 * 1024), 0);
	aiocb_list[0] = aiocbp;

	T_WITH_ERRNO;
	err = aio_write(aiocbp);
	T_ASSERT_NE(err, -1, "aio_write() for fd %d offset 0x%llx length 0x%zx",
	    aiocbp->aio_fildes, aiocbp->aio_offset, aiocbp->aio_nbytes);

	T_WITH_ERRNO;
	timeout.tv_sec = 1;
	timeout.tv_nsec = 0;
	err = aio_suspend((const struct aiocb *const *)aiocb_list, 1, &timeout);
	T_ASSERT_NE(err, -1, "aio_suspend() with 1 sec timeout");

	T_WITH_ERRNO;
	retval = aio_return(aiocbp);
	T_ASSERT_EQ((int)retval, AIO_BUFFER_SIZE,
	    "aio_return() for aiocbp %p bytes_written 0x%zx", aiocbp, retval);
}

/*
 * Test lio_listio() with LIO_WAIT.
 * Initiate a list of AIO operations and wait for their completions.
 */
T_DECL(lio_listio_wait, "Test lio_listio() with LIO_WAIT.")
{
	struct aiocb *aiocbp, *aiocb_list[AIO_LIST_MAX];
	ssize_t retval;
	int i, err;

	do_init(AIO_LIST_MAX, true);

	/* Setup aiocbs for lio_listio(). */
	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = init_aiocb(i, (i * 1024 * 1024), LIO_WRITE);
		aiocb_list[i] = aiocbp;
	}

	T_WITH_ERRNO;
	err = lio_listio(LIO_WAIT, aiocb_list, AIO_LIST_MAX, NULL);
	T_ASSERT_NE(err, -1, "lio_listio(LIO_WAIT) for %d AIO operations",
	    AIO_LIST_MAX);

	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = aiocb_list[i];

		T_WITH_ERRNO;
		retval = aio_return(aiocbp);
		T_ASSERT_EQ((int)retval, AIO_BUFFER_SIZE,
		    "aio_return() for aiocbp(%d) %p bytes_written 0x%zx",
		    i, aiocbp, retval);
	}
}

/*
 * Test lio_listio() with LIO_NOWAIT.
 * Initiate a list of AIO operations and poll for their completions.
 */
T_DECL(lio_listio_nowait, "Test lio_listio() with LIO_NOWAIT.")
{
	struct aiocb *aiocbp, *aiocb_list[AIO_LIST_MAX];
	ssize_t retval;
	int i, err;

	do_init(AIO_LIST_MAX, true);

	/* Setup aiocbs for lio_listio(). */
	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = init_aiocb(i, (i * 1024 * 1024), LIO_WRITE);
		aiocb_list[i] = aiocbp;
	}

	T_WITH_ERRNO;
	err = lio_listio(LIO_NOWAIT, aiocb_list, AIO_LIST_MAX, NULL);
	T_ASSERT_NE(err, -1, "lio_listio(LIO_NOWAIT) for %d AIO operations",
	    AIO_LIST_MAX);

	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = aiocb_list[i];

		T_WITH_ERRNO;
		err = poll_aio_error(aiocbp);
		T_ASSERT_NE(err, -1, "aio_error() for aiocbp %p", aiocbp);

		T_WITH_ERRNO;
		retval = aio_return(aiocbp);
		T_ASSERT_EQ((int)retval, AIO_BUFFER_SIZE,
		    "aio_return() for aiocbp(%d) %p bytes_written 0x%zx",
		    i, aiocbp, retval);
	}
}

/*
 * Test lio_listio() and aio_cancel().
 * Initiate a list of AIO operations and attempt to cancel them with
 * aio_cancel().
 */
T_DECL(lio_listio_cancel, "Test lio_listio() and aio_cancel().")
{
	struct aiocb *aiocbp, *aiocb_list[AIO_LIST_MAX];
	char *buffer;
	ssize_t retval;
	int i, err;

	do_init(AIO_LIST_MAX, true);

	/* Setup aiocbs for lio_listio(). */
	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = init_aiocb(i, (i * 1024 * 1024), LIO_WRITE);
		aiocb_list[i] = aiocbp;
	}

	T_WITH_ERRNO;
	err = lio_listio(LIO_NOWAIT, aiocb_list, AIO_LIST_MAX, NULL);
	T_ASSERT_NE(err, -1, "lio_listio() for %d AIO operations", AIO_LIST_MAX);

	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = aiocb_list[i];

		T_WITH_ERRNO;
		err = aio_cancel(g_fds[i], aiocbp);
		T_ASSERT_TRUE(((err & (AIO_ALLDONE | AIO_CANCELED | AIO_NOTCANCELED)) != 0),
		    "aio_cancel() for aiocbp(%d) %p err %d", i, aiocbp, err);

		if (err == AIO_NOTCANCELED || err == AIO_ALLDONE) {
			if (err == AIO_NOTCANCELED) {
				T_WITH_ERRNO;
				err = poll_aio_error(aiocbp);
				T_ASSERT_NE(err, -1, "aio_error() for aiocbp %p", aiocbp);
			}
			T_WITH_ERRNO;
			retval = aio_return(aiocbp);
			T_ASSERT_EQ((int)retval, AIO_BUFFER_SIZE,
			    "aio_return() for aiocbp(%d) %p bytes_written 0x%zx",
			    i, aiocbp, retval);
		} else if (err == AIO_CANCELED) {
			T_WITH_ERRNO;
			retval = aio_return(aiocbp);
			T_ASSERT_EQ((int)retval, -1,
			    "aio_return() for aiocbp(%d) %p", i, aiocbp);
		}
	}
}

/*
 * Test aio_write() and aio_read().
 * Use kevent for AIO completion and return status.
 */
T_DECL(write_read_kevent, "Test aio_write() and aio_read(). Use kevent for AIO completion and return status.")
{
	struct aiocb *aiocbp;
	struct kevent64_s kevent;
	void *udata1, *udata2;
	ssize_t retval;
	int err, kq;

	do_init(1, true);

	kq = kqueue();
	T_ASSERT_NE(kq, -1, "Create kqueue");

	/* Setup aiocb for aio_write(). */
	aiocbp = init_aiocb(0, 0, 0);
	aiocbp->aio_sigevent.sigev_notify = SIGEV_KEVENT;
	aiocbp->aio_sigevent.sigev_signo = kq;
	aiocbp->aio_sigevent.sigev_value.sival_ptr = (void *)&udata1;

	T_WITH_ERRNO;
	err = aio_write(aiocbp);
	T_ASSERT_NE(err, -1, "aio_write() for fd %d offset 0x%llx length 0x%zx",
	    aiocbp->aio_fildes, aiocbp->aio_offset, aiocbp->aio_nbytes);

	memset(&kevent, 0, sizeof(kevent));
	err = wait_for_kevent(kq, &kevent);
	T_ASSERT_NE(err, -1, "Listen for AIO completion event on kqueue %d", kq);

	if (err > 0) {
		T_ASSERT_EQ(err, 1, "num event returned %d", err);
		T_ASSERT_EQ((struct aiocb *)kevent.ident, aiocbp, "kevent.ident %p",
		    (struct aiocb *)kevent.ident);
		T_ASSERT_EQ(kevent.filter, EVFILT_AIO, "kevent.filter %d",
		    kevent.filter);
		T_ASSERT_EQ((void **)kevent.udata, &udata1, "kevent.udata %p",
		    (char *)kevent.udata);
		T_ASSERT_EQ((int)kevent.ext[0], 0, "kevent.ext[0] (err %d)",
		    (int)kevent.ext[0]);
		T_ASSERT_EQ((int)kevent.ext[1], AIO_BUFFER_SIZE,
		    "kevent.ext[1] (bytes_written 0x%x)", (int)kevent.ext[1]);
	} else {
		T_FAIL("Timedout listening for AIO completion event on kqueue %d", kq);
	}

	aiocbp->aio_sigevent.sigev_value.sival_ptr = (void *)&udata2;

	T_WITH_ERRNO;
	err = aio_read(aiocbp);
	T_ASSERT_NE(err, -1, "aio_read() for fd %d offset 0x%llx length 0x%zx",
	    aiocbp->aio_fildes, aiocbp->aio_offset, aiocbp->aio_nbytes);

	memset(&kevent, 0, sizeof(kevent));
	err = wait_for_kevent(kq, &kevent);
	T_ASSERT_NE(err, -1, "Listen for AIO completion event on kqueue %d", kq);

	if (err > 0) {
		T_ASSERT_EQ(err, 1, "num event returned %d", err);
		T_ASSERT_EQ((struct aiocb *)kevent.ident, aiocbp, "kevent.ident %p",
		    (struct aiocb *)kevent.ident);
		T_ASSERT_EQ(kevent.filter, EVFILT_AIO, "kevent.filter %d",
		    kevent.filter);
		T_ASSERT_EQ((void **)kevent.udata, &udata2, "kevent.udata %p",
		    (char *)kevent.udata);
		T_ASSERT_EQ((int)kevent.ext[0], 0, "kevent.ext[0] (err %d)",
		    (int)kevent.ext[0]);
		T_ASSERT_EQ((int)kevent.ext[1], AIO_BUFFER_SIZE,
		    "kevent.ext[1] (bytes_read 0x%x)", (int)kevent.ext[1]);
	} else {
		T_FAIL("Timedout listening for AIO completion event on kqueue %d", kq);
	}
}

/*
 * Test lio_listio() with LIO_NOWAIT.
 * Initiate a list of AIO operations and use kevent for their completion
 * notification and status.
 */
T_DECL(lio_listio_kevent, "Test lio_listio() with kevent.")
{
	struct aiocb *aiocbp, *aiocb_list[AIO_LIST_MAX];
	struct kevent64_s kevent;
	ssize_t retval;
	int i, err, kq;

	do_init(AIO_LIST_MAX, true);

	kq = kqueue();
	T_ASSERT_NE(kq, -1, "Create kqueue");

	/* Setup aiocbs for lio_listio(). */
	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = init_aiocb(i, (i * 1024 * 1024), LIO_WRITE);
		aiocbp->aio_sigevent.sigev_notify = SIGEV_KEVENT;
		aiocbp->aio_sigevent.sigev_signo = kq;
		aiocbp->aio_sigevent.sigev_value.sival_ptr = (void *)g_testfiles[i];
		aiocb_list[i] = aiocbp;
	}

	T_WITH_ERRNO;
	err = lio_listio(LIO_NOWAIT, aiocb_list, AIO_LIST_MAX, NULL);
	T_ASSERT_NE(err, -1, "lio_listio(LIO_NOWAIT) for %d AIO operations",
	    AIO_LIST_MAX);

	for (i = 0; i < AIO_LIST_MAX; i++) {
		aiocbp = aiocb_list[i];

		memset(&kevent, 0, sizeof(kevent));
		err = wait_for_kevent(kq, &kevent);
		T_ASSERT_NE(err, -1, "Listen for AIO completion event on kqueue %d", kq);
		if (err > 0) {
			int idx;

			aiocbp = NULL;
			T_ASSERT_EQ(err, 1, "num event returned %d", err);

			for (idx = 0; idx < AIO_LIST_MAX; idx++) {
				if (aiocb_list[idx] == (struct aiocb *)kevent.ident) {
					aiocbp = (struct aiocb *)kevent.ident;
					break;
				}
			}

			T_ASSERT_EQ((struct aiocb *)kevent.ident, aiocbp, "kevent.ident %p",
			    (struct aiocb *)kevent.ident);
			T_ASSERT_EQ(kevent.filter, EVFILT_AIO, "kevent.filter %d",
			    kevent.filter);
			T_ASSERT_EQ((void *)kevent.udata, (void *)g_testfiles[idx],
			    "kevent.udata %p", (char *)kevent.udata);
			T_ASSERT_EQ((int)kevent.ext[0], 0, "kevent.ext[0] (err %d)",
			    (int)kevent.ext[0]);
			T_ASSERT_EQ((int)kevent.ext[1], AIO_BUFFER_SIZE,
			    "kevent.ext[1] (bytes_read 0x%x)", (int)kevent.ext[1]);
		} else {
			T_FAIL("Timedout listening for AIO completion event on kqueue %d", kq);
		}
	}
}
