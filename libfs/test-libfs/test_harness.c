#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "test_harness.h"
#include "FSFormatName.h"

// All tests must use a buffer of this size or less.
#define TEST_BUFFER_SIZE 1024

int test(size_t block_size, off_t start_addr, size_t length, char *buf) {
	int testfd = open("readdisk_testfile.txt", O_RDONLY);
	if (testfd < 0) {
		perror("open");
		return -1;
	}

	if (readdisk(testfd, start_addr, length, block_size, buf) < block_size) {
		fprintf(stderr, "readdisk fail\n");
		return -2;
	}

	close(testfd);
	return 0;
}

int maketest(char *answer, size_t block_size, off_t start_addr, size_t length) {
	char buf[TEST_BUFFER_SIZE];
	int res;

	res = test(block_size, start_addr, length, buf);
	if (res < 0) {
		return res;
	}

	if (memcmp(buf, answer, length) != 0) {
		res = -3;

		// Null terminate the bad string so we can print it.
		if (length < TEST_BUFFER_SIZE) {
			buf[length] = 0;
		} else {
			buf[TEST_BUFFER_SIZE - 1] = 0;
		}

		fprintf(stderr, "Expected %s, got %s\n",
				answer, buf);
	}

	return res;
}
