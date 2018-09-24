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

int test_fs_encryption_status(const char *bsdname) {
	int res = 0;
	bool encryption_status;
	fs_media_encryption_details_t encryption_details;

	// Make sure we return an error when passed invalid arguments.
	res = GetFSEncryptionStatus(bsdname, NULL, false, NULL);
	if (res == 0) {
		return -1;
	}

	// Make sure that we don't return an error when just asking for
	// encryption status.
	res = GetFSEncryptionStatus(bsdname, &encryption_status, false, NULL);
	if (res != 0) {
		return res;
	}

	res = GetFSEncryptionStatus(bsdname, &encryption_status, true, NULL);
	if (res != 0) {
		return res;
	}

	// Make sure we also return success when asking for extended information.
	res = GetFSEncryptionStatus(bsdname, &encryption_status, false,
								&encryption_details);
	if (res != 0) {
		return res;
	}

	res = GetFSEncryptionStatus(bsdname, &encryption_status, true, &encryption_details);
	if (res != 0) {
		return res;
	}

	printf("For %s, FS encrypted: %d details: %u\n", bsdname,
		   encryption_status, encryption_details);
	return 0;
}

int test_di_encryption_status(const char *bsdname) {
	int res = 0;
	bool encryption_status;

	// Make sure we return an error when passed invalid arguments.
	res = GetDiskImageEncryptionStatus(bsdname, NULL);
	if (res == 0) {
		return -1;
	}

	// Make sure that we don't return an error when just asking for
	// encryption status.
	res = GetDiskImageEncryptionStatus(bsdname, &encryption_status);
	if (res != 0) {
		return res;
	}

	printf("For %s, DI encrypted: %d\n", bsdname, encryption_status);
	return 0;
}

int test_encryption_status(const char *bsdname) {
	bool encryption_status = false;
	fs_media_encryption_details_t encryption_detail = 0;
	CFStringRef bsdname_cfstring = CFStringCreateWithCString(kCFAllocatorDefault, bsdname, kCFStringEncodingASCII);
	errno_t error;

	error = _FSGetMediaEncryptionStatus(bsdname_cfstring, &encryption_status, &encryption_detail);
	if (error != 0) {
		return error;
	}

	printf("For %s, encrypted: %d total encryption status: %u\n", bsdname, encryption_status, encryption_detail);
	CFRelease(bsdname_cfstring);
	return 0;
}
