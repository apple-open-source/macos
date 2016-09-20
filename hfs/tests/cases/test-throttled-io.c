#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <CommonCrypto/CommonDigest.h>
#include <stdbool.h>
#include <stdio.h>
#include <spawn.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/errno.h>
#include <libkern/OSAtomic.h>
#include <zlib.h>
#include <pthread.h>

#include "hfs-tests.h"
#include "test-utils.h"
#include "disk-image.h"

TEST(throttled_io)

static disk_image_t *di;
static char *file1, *file2, *file3;

static pid_t pid;
static void *buf;
static const size_t buf_size = 64 * 1024 * 1024;

static int run_test1(void)
{
	char *of;
	asprintf(&of, "of=%s", file1);
	
	// Kick off another process to ensure we get throttled
	assert_no_err(posix_spawn(&pid, "/bin/dd", NULL, NULL, 
							  (char *[]){ "/bin/dd", "if=/dev/random", 
									  of, NULL },
			NULL));

	assert_no_err(setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, 
								 IOPOL_THROTTLE));

	int fd, fd2;
	assert_with_errno((fd = open("/dev/random", O_RDONLY)) >= 0);
	assert_with_errno((fd2 = open(file2, 
								  O_RDWR | O_CREAT | O_TRUNC, 0666)) >= 0);

	assert_no_err(fcntl(fd2, F_SINGLE_WRITER, 1));
	assert_no_err(fcntl(fd2, F_NOCACHE, 1));

	buf = valloc(buf_size);
	CC_SHA1_CTX ctx;
	CC_SHA1_Init(&ctx);

	ssize_t res = check_io(read(fd, buf, buf_size), buf_size);

	CC_SHA1_Update(&ctx, buf, (CC_LONG)res);

	res = check_io(write(fd2, buf, res), res);

	bzero(buf, buf_size);

	CC_SHA1_CTX ctx2;
	CC_SHA1_Init(&ctx2);

	lseek(fd2, 0, SEEK_SET);

	res = check_io(read(fd2, buf, buf_size), buf_size);

	CC_SHA1_Update(&ctx2, buf, (CC_LONG)res);

	uint8_t digest1[CC_SHA1_DIGEST_LENGTH], digest2[CC_SHA1_DIGEST_LENGTH];
	CC_SHA1_Final(digest1, &ctx);
	CC_SHA1_Final(digest2, &ctx2);

	assert(!memcmp(digest1, digest2, CC_SHA1_DIGEST_LENGTH));

	return 0;
}

static volatile uint64_t written;
static volatile bool done;

static void test2_thread(void)
{
	int fd = open(file3, O_RDONLY);
	assert(fd >= 0);

	void *b = buf + buf_size / 2;
	uLong seq = crc32(0, Z_NULL, 0);
	uint32_t offset = 0;

	do {
		ssize_t res;

		do {
			res = check_io(pread(fd, b, buf_size / 2, offset), -1);
		} while (res == 0 && !done);

		assert (res % 4 == 0);

		offset += res;

		for (uLong *p = b; res; ++p, res -= sizeof(uLong)) {
			seq = crc32(Z_NULL, (void *)&seq, 4);
			assert(*p == seq);
		}

		if (offset < written)
			continue;
		OSMemoryBarrier();
	} while (!done);
}

static int run_test2(void)
{
	int fd = open(file3, O_RDWR | O_CREAT | O_TRUNC, 0666);
	assert(fd >= 0);

	assert_no_err(fcntl(fd, F_SINGLE_WRITER, 1));
	assert_no_err(fcntl(fd, F_NOCACHE, 1));

	pthread_t thread;
	pthread_create(&thread, NULL, (void *(*)(void *))test2_thread, NULL);
	uLong seq = crc32(0, Z_NULL, 0);

	for (int i = 0; i < 4; ++i) {
		uLong *p = buf;
		for (unsigned i = 0; i < buf_size / 2 / sizeof(uLong); ++i) {
			seq = crc32(Z_NULL, (void *)&seq, 4);
			p[i] = seq;
		}

		ssize_t res = check_io(write(fd, buf, buf_size / 2), buf_size / 2);

		written += res;
	}

	OSMemoryBarrier();

	done = true;

	pthread_join(thread, NULL);

	return 0;
}

static bool clean_up(void)
{
	kill(pid, SIGKILL);
	int stat;
	wait(&stat);

	unlink(file1);
	unlink(file2);
	unlink(file3);
	
	return true;
}

int run_throttled_io(__unused test_ctx_t *ctx)
{
	test_cleanup(^ bool {
		return clean_up();
	});
	
	di = disk_image_get();
	asprintf(&file1, "%s/throttled_io.1", di->mount_point);
	asprintf(&file2, "%s/throttled_io.2", di->mount_point);
	asprintf(&file3, "%s/throttled_io.3", di->mount_point);

	int res = run_test1();
	if (res)
		goto out;

	res = run_test2();
	if (res)
		goto out;

out:
	free(file1);
	free(file2);
	free(file3);
	
	return res;
}
