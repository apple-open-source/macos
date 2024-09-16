#ifdef T_NAMESPACE
#undef T_NAMESPACE
#endif /* T_NAMESPACE */

#include <Block.h>
#include <darwintest.h>
#include <dispatch/dispatch.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <util.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.kevent"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("kevent"),
	T_META_CHECK_LEAKS(false),
	T_META_RUN_CONCURRENTLY(true));

#define TIMEOUT_SECS 10

static int child_ready[2];

static void
child_tty_client(void)
{
	dispatch_source_t src;
	char buf[16] = "";
	ssize_t bytes_wr;

	src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
	    (uintptr_t)STDIN_FILENO, 0, NULL);
	if (!src) {
		exit(1);
	}
	dispatch_source_set_event_handler(src, ^{});

	dispatch_activate(src);

	close(child_ready[0]);
	snprintf(buf, sizeof(buf), "%ds", getpid());
	bytes_wr = write(child_ready[1], buf, strlen(buf));
	if (bytes_wr < 0) {
		err(1, "failed to write on child ready pipe");
	}

	dispatch_main();
}

static void
pty_master(void)
{
	pid_t child_pid;
	int ret;

	child_pid = fork();
	if (child_pid == 0) {
		child_tty_client();
	}
	ret = setpgid(child_pid, child_pid);
	if (ret < 0) {
		exit(1);
	}
	ret = tcsetpgrp(STDIN_FILENO, child_pid);
	if (ret < 0) {
		exit(1);
	}

	sleep(TIMEOUT_SECS);
	exit(1);
}

T_DECL(pty_master_teardown,
    "try removing a TTY master out from under a PTY slave holding a kevent",
    T_META_ASROOT(true), T_META_TAG_VM_PREFERRED)
{
	__block pid_t master_pid;
	char buf[16] = "";
	char *end;
	ssize_t bytes_rd;
	size_t buf_len = 0;
	unsigned long slave_pid;
	int master_fd;
	char pty_filename[PATH_MAX];
	int status;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(pipe(child_ready), NULL);

	master_pid = forkpty(&master_fd, pty_filename, NULL, NULL);
	if (master_pid == 0) {
		pty_master();
		__builtin_unreachable();
	}
	T_ASSERT_POSIX_SUCCESS(master_pid,
	    "forked child master PTY with pid %d, at pty %s", master_pid,
	    pty_filename);

	close(child_ready[1]);

	end = buf;
	do {
		bytes_rd = read(child_ready[0], end, sizeof(buf) - buf_len);
		T_ASSERT_POSIX_SUCCESS(bytes_rd, "read on pipe between master and runner");
		buf_len += (size_t)bytes_rd;
		T_LOG("runner read %zd bytes", bytes_rd);
		end += bytes_rd;
	} while (bytes_rd != 0 && *(end - 1) != 's');

	slave_pid = strtoul(buf, &end, 0);
	if (buf == end) {
		T_ASSERT_FAIL("could not parse child PID from master pipe");
	}

	T_LOG("got pid %lu for slave process from master", slave_pid);
	T_SETUPEND;

	T_LOG("sending fatal signal to master");
	T_ASSERT_POSIX_SUCCESS(kill(master_pid, SIGKILL), NULL);

	T_LOG("sending fatal signal to slave");
	(void)kill((int)slave_pid, SIGKILL);

	T_ASSERT_POSIX_SUCCESS(waitpid(master_pid, &status, 0), NULL);
	T_ASSERT_TRUE(WIFSIGNALED(status), "master PID was signaled");
	(void)waitpid((int)slave_pid, &status, 0);
}

volatile static bool writing = true;

static void *
reader_thread(void *arg)
{
	int fd = (int)arg;
	char c;

	T_SETUPBEGIN;
	T_QUIET;
	T_ASSERT_GT(fd, 0, "reader thread received valid fd");
	T_SETUPEND;

	for (;;) {
		ssize_t rdsize = read(fd, &c, sizeof(c));
		if (rdsize == -1) {
			if (errno == EINTR) {
				continue;
			} else if (errno == EBADF) {
				T_LOG("reader got an error (%s), shutting down",
				    strerror(errno));
				return NULL;
			} else {
				T_ASSERT_POSIX_SUCCESS(rdsize, "read on PTY");
			}
		} else if (rdsize == 0) {
			return NULL;
		}
	}

	return NULL;
}

static void *
writer_thread(void *arg)
{
	int fd = (int)arg;
	char c[4096];
	memset(c, 'a', sizeof(c));

	T_SETUPBEGIN;
	T_QUIET;
	T_ASSERT_GT(fd, 0, "writer thread received valid fd");
	T_SETUPEND;

	while (writing) {
		ssize_t wrsize = write(fd, c, sizeof(c));
		if (wrsize == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				T_LOG("writer got an error (%s), shutting down",
				    strerror(errno));
				return NULL;
			}
		}
	}

	return NULL;
}

#define ATTACH_ITERATIONS 10000

static int attach_master, attach_slave;
static pthread_t reader, writer;

static void
redispatch(dispatch_group_t grp, dispatch_source_type_t type, int fd)
{
	__block int iters = 0;

	__block void (^redispatch_blk)(void) = Block_copy(^{
		if (iters++ > ATTACH_ITERATIONS) {
		        return;
		} else if (iters == ATTACH_ITERATIONS) {
		        dispatch_group_leave(grp);
		        T_PASS("created %d %s sources on busy PTY", iters,
		        type == DISPATCH_SOURCE_TYPE_READ ? "read" : "write");
		}

		dispatch_source_t src = dispatch_source_create(
			type, (uintptr_t)fd, 0,
			dispatch_get_main_queue());

		dispatch_source_set_event_handler(src, ^{
			dispatch_cancel(src);
		});

		dispatch_source_set_cancel_handler(src, redispatch_blk);

		dispatch_activate(src);
	});

	dispatch_group_enter(grp);
	dispatch_async(dispatch_get_main_queue(), redispatch_blk);
}

T_DECL(attach_while_tty_wakeups,
    "try to attach knotes while a TTY is getting wakeups", T_META_TAG_VM_PREFERRED)
{
	dispatch_group_t grp = dispatch_group_create();

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(openpty(&attach_master, &attach_slave, NULL, NULL,
	    NULL), NULL);

	T_ASSERT_POSIX_ZERO(pthread_create(&reader, NULL, reader_thread,
	    (void *)(uintptr_t)attach_master), NULL);
	T_ASSERT_POSIX_ZERO(pthread_create(&writer, NULL, writer_thread,
	    (void *)(uintptr_t)attach_slave), NULL);
	T_SETUPEND;

	redispatch(grp, DISPATCH_SOURCE_TYPE_READ, attach_master);
	redispatch(grp, DISPATCH_SOURCE_TYPE_WRITE, attach_slave);

	dispatch_group_notify(grp, dispatch_get_main_queue(), ^{
		T_LOG("both reader and writer sources cleaned up");
		T_END;
	});

	dispatch_main();
}

T_DECL(master_read_data_set,
    "check that the data is set on read sources of master fds", T_META_TAG_VM_PREFERRED)
{
	int master = -1, slave = -1;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(openpty(&master, &slave, NULL, NULL, NULL), NULL);
	T_QUIET; T_ASSERT_GE(master, 0, "master fd is valid");
	T_QUIET; T_ASSERT_GE(slave, 0, "slave fd is valid");

	dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
	    (uintptr_t)master, 0, dispatch_get_main_queue());

	dispatch_source_set_event_handler(src, ^{
		unsigned long len = dispatch_source_get_data(src);
		T_EXPECT_GT(len, (unsigned long)0,
		"the amount of data to read was set for the master source");
		dispatch_cancel(src);
	});

	dispatch_source_set_cancel_handler(src, ^{
		dispatch_release(src);
		T_END;
	});

	dispatch_activate(src);
	T_SETUPEND;

	// Let's not fill up the TTY's buffer, otherwise write(2) will block.
	char buf[512] = "";

	int ret = 0;
	while ((ret = write(slave, buf, sizeof(buf)) == -1 && errno == EAGAIN)) {
		;
	}
	T_ASSERT_POSIX_SUCCESS(ret, "slave wrote data");

	dispatch_main();
}
