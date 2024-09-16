//
//  bsize_test.c
//  copyfile_test
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/mount.h>

#include "test_utils.h"

REGISTER_TEST(bsize, false, TIMEOUT_MIN(1));

#define SOURCE_FILE_NAME     	"bsize_source"
#define DESTINATION_FILE_NAME	"bsize_destination"

#define FILE_SIZE	(8 * MB)

typedef struct copyfile_cb_ctx {
	uint32_t progress_cb_calls;
} copyfile_cb_ctx_t;

typedef enum bsize_selection {
	bs_both = 0,
	bs_src = 1,
	bs_dst = 2,
} bsize_selection_t;

static int copyfile_cb(int what, int stage, __unused copyfile_state_t state,
	__unused const char *src, __unused const char *dst, void *ctxp) {
	copyfile_cb_ctx_t *ctx = (copyfile_cb_ctx_t *)ctxp;

	assert_equal_int(what, COPYFILE_COPY_DATA);
	assert_equal_int(stage, COPYFILE_PROGRESS);

	ctx->progress_cb_calls++;

	return 0;
}

static bool verify_bsize_behavior(const char *src, const char *dst,
	size_t bsize, uint32_t num_calls, bsize_selection_t selection) {
	off_t bytes_copied = 0;
	copyfile_cb_ctx_t callback_ctx = {0};
	copyfile_state_t state;
	bool success = true;

	// The idea here is to copy src into dst,
	// setting the copy blocksize for either the source, the destination,
	// or both, and verify that the correct blocksize was used.
	// To do so, we rely on the caller knowing the number of times
	// the progress callback should be called (since it is called after
	// one blocksize-sized copy of data).

	// Set up the copyfile state, with the callback and blocksize.
	assert_with_errno((state = copyfile_state_alloc()));
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, &copyfile_cb));
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CTX, &callback_ctx));
	if (selection == bs_both) {
		assert_no_err(copyfile_state_set(state, COPYFILE_STATE_BSIZE, &bsize));
	} else if (selection == bs_src) {
		assert_no_err(copyfile_state_set(state, COPYFILE_STATE_SRC_BSIZE, &bsize));
	} else if (selection == bs_dst) {
		assert_no_err(copyfile_state_set(state, COPYFILE_STATE_DST_BSIZE, &bsize));
	}

	// Perform the copy.
	assert_no_err(copyfile(src, dst, state, COPYFILE_DATA|COPYFILE_EXCL));

	// Verify copy contents.
	success = verify_copy_contents(src, dst);

	// Verify the number of times the progress callback was called.
	if (num_calls != callback_ctx.progress_cb_calls) {
		printf("expected %u calls (bsize %lu), actually found %u\n", num_calls, bsize,
			callback_ctx.progress_cb_calls);
		success = false;
	}

	// Verify that COPYFILE_STATE_COPIED is set correctly each time.
	assert_no_err(copyfile_state_get(state, COPYFILE_STATE_COPIED, &bytes_copied));
	if (bytes_copied != FILE_SIZE) {
		printf("expected %lu bytes copied (bsize %lu), actually found %lld\n",
			FILE_SIZE, bsize, bytes_copied);
		success = false;
	}

	// Post-test cleanup.
	assert_no_err(removefile(dst, NULL, 0));
	assert_no_err(copyfile_state_free(state));

	return success;
}

bool do_bsize_test(const char *apfs_test_directory, size_t block_size) {
	char test_src[BSIZE_B] = {0}, test_dst[BSIZE_B];
	int test_file_id, src_fd;
	struct statfs sfs; // for f_iosize
	bool success = true;

	// Get ready for the test.
	test_file_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(apfs_test_directory, SOURCE_FILE_NAME, test_file_id, test_src);
	create_test_file_name(apfs_test_directory, DESTINATION_FILE_NAME, test_file_id, test_dst);
	assert_no_err(statfs(apfs_test_directory, &sfs));

	// Create the test file and write our test content into it.
	src_fd = open(test_src, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(src_fd >= 0);
	// Write into the end of the file.
	assert_with_errno(pwrite(src_fd, "j", 1, FILE_SIZE - 1) == 1);
	assert_no_err(ftruncate(src_fd, FILE_SIZE));
	assert_no_err(close(src_fd));

	// Copy this file into our destination, using each blocksize, and validate its contents
	// and the number of times our callback was called.
	success = success && verify_bsize_behavior(test_src, test_dst, block_size,
		FILE_SIZE / block_size, bs_both);
	success = success && verify_bsize_behavior(test_src, test_dst, (size_t) sfs.f_iosize,
		FILE_SIZE / sfs.f_iosize, bs_both);
	success = success && verify_bsize_behavior(test_src, test_dst, MB, FILE_SIZE / MB, bs_both);
	success = success && verify_bsize_behavior(test_src, test_dst, FILE_SIZE, 1, bs_both);
	success = success && verify_bsize_behavior(test_src, test_dst, SIZE_MAX, 1, bs_both);
	success = success && verify_bsize_behavior(test_src, test_dst, 0,
		FILE_SIZE / sfs.f_iosize, bs_both); // 0 means let copyfile() choose

	// Check that setting just the source (or destination) blocksize still results
	// in the copy size being capped by the destination (or source) filesystem's copy blocksize.
	success = success && verify_bsize_behavior(test_src, test_dst, MB,
		FILE_SIZE / sfs.f_iosize, bs_src);
	success = success && verify_bsize_behavior(test_src, test_dst, MB,
		FILE_SIZE / sfs.f_iosize, bs_dst);

	(void)removefile(test_src, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
