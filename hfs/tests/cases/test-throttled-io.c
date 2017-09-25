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

static disk_image_t *gDI;
static char *gFile1, *gFile2, *gFile3;

static pid_t gPID = 0;
static void *gBuf;
static const size_t gBuf_size = 64 * 1024 * 1024;

static void start_background_io(void)
{
	char *of;
	asprintf(&of, "of=%s", gFile1);
	
	assert_no_err(posix_spawn(&gPID, "/bin/dd", NULL, NULL,
							  (char *[]){ "/bin/dd", "if=/dev/random",
								  of, NULL },
							  NULL));
}

static void end_background_io(void)
{
	if ( gPID != 0 )
	{
		kill(gPID, SIGKILL);
		int stat;
		wait(&stat);
		gPID = 0;
	}
}

static int run_test1(void)
{

	// Kick off another process to ensure we get throttled
	start_background_io();

	assert_no_err(setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, 
								 IOPOL_THROTTLE));

	int fd, fd2;
	assert_with_errno((fd = open("/dev/random", O_RDONLY)) >= 0);
	assert_with_errno((fd2 = open(gFile2, 
								  O_RDWR | O_CREAT | O_TRUNC, 0666)) >= 0);

	assert_no_err(fcntl(fd2, F_SINGLE_WRITER, 1));
	assert_no_err(fcntl(fd2, F_NOCACHE, 1));

	gBuf = valloc(gBuf_size);
	CC_SHA1_CTX ctx;
	CC_SHA1_Init(&ctx);

	ssize_t res = check_io(read(fd, gBuf, gBuf_size), gBuf_size);

	CC_SHA1_Update(&ctx, gBuf, (CC_LONG)res);

	res = check_io(write(fd2, gBuf, res), res);

	bzero(gBuf, gBuf_size);

	CC_SHA1_CTX ctx2;
	CC_SHA1_Init(&ctx2);

	lseek(fd2, 0, SEEK_SET);

	res = check_io(read(fd2, gBuf, gBuf_size), gBuf_size);

	CC_SHA1_Update(&ctx2, gBuf, (CC_LONG)res);

	uint8_t digest1[CC_SHA1_DIGEST_LENGTH], digest2[CC_SHA1_DIGEST_LENGTH];
	CC_SHA1_Final(digest1, &ctx);
	CC_SHA1_Final(digest2, &ctx2);

	assert(!memcmp(digest1, digest2, CC_SHA1_DIGEST_LENGTH));

	assert_no_err (close(fd));
	assert_no_err (close(fd2));
	
	end_background_io();

	return 0;
}

static volatile uint64_t written;
static volatile bool done;

static void test2_thread(void)
{
	int fd = open(gFile3, O_RDONLY);
	assert(fd >= 0);

	void *b = gBuf + gBuf_size / 2;
	uLong seq = crc32(0, Z_NULL, 0);
	uint32_t offset = 0;

	do {
		ssize_t res;

		do {
			res = check_io(pread(fd, b, gBuf_size / 2, offset), -1);
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
	
	assert_no_err (close(fd));
}

static int run_test2(void)
{
	start_background_io();
	
	int fd = open(gFile3, O_RDWR | O_CREAT | O_TRUNC, 0666);
	assert(fd >= 0);

	assert_no_err(fcntl(fd, F_SINGLE_WRITER, 1));
	assert_no_err(fcntl(fd, F_NOCACHE, 1));

	pthread_t thread;
	pthread_create(&thread, NULL, (void *(*)(void *))test2_thread, NULL);
	uLong seq = crc32(0, Z_NULL, 0);

	for (int i = 0; i < 4; ++i) {
		uLong *p = gBuf;
		for (unsigned i = 0; i < gBuf_size / 2 / sizeof(uLong); ++i) {
			seq = crc32(Z_NULL, (void *)&seq, 4);
			p[i] = seq;
		}

		ssize_t res = check_io(write(fd, gBuf, gBuf_size / 2), gBuf_size / 2);

		written += res;
	}

	OSMemoryBarrier();

	done = true;

	pthread_join(thread, NULL);

	assert_no_err (close(fd));
	
	end_background_io();

	return 0;
}

static bool clean_up(void)
{
	end_background_io();

	unlink(gFile1);
	unlink(gFile2);
	unlink(gFile3);
	
	free(gFile1);
	free(gFile2);
	free(gFile3);
	
	return true;
}

int run_throttled_io(__unused test_ctx_t *ctx)
{
	
	gDI = disk_image_get();
	
	asprintf(&gFile1, "%s/throttled_io.1", gDI->mount_point);
	asprintf(&gFile2, "%s/throttled_io.2", gDI->mount_point);
	asprintf(&gFile3, "%s/throttled_io.3", gDI->mount_point);
	
	test_cleanup(^ bool {
		return clean_up();
	});

	int res = run_test1();
	if (res)
		return res;

	res = run_test2();
	if (res)
		return res;
	
	return res;
}
