//
//  cprotect_test.c
//  copyfile_test
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "test_utils.h"

#if TARGET_OS_IPHONE

REGISTER_TEST(cprotect_ignore, false, TIMEOUT_MIN(1));

bool do_cprotect_ignore_test(const char *apfs_test_directory, size_t block_size) {
	copyfile_state_t cpf_state = copyfile_state_alloc();
	uint32_t dst_class;
	char file_src[BSIZE_B] = {0}, file_dst[BSIZE_B] = {0};
	bool success = true;

	// Create a source file with a protection class on disk.
	assert_with_errno(snprintf(file_src, BSIZE_B, "%s/cprotect_src", apfs_test_directory) > 0);
	assert_no_err(close(open_dprotected_np(file_src, DEFAULT_OPEN_FLAGS, PROTECTION_CLASS_D, 0, DEFAULT_OPEN_PERM)));
	assert_no_err(truncate(file_src, block_size));

	// Create a destination file with a different destination class on disk.
	assert_with_errno(snprintf(file_dst, BSIZE_B, "%s/cprotect_dst", apfs_test_directory) > 0);
	assert_no_err(close(open_dprotected_np(file_dst, DEFAULT_OPEN_FLAGS, PROTECTION_CLASS_C, 0, DEFAULT_OPEN_PERM)));
	assert_no_err(truncate(file_dst, block_size));

	// Opt-in to not changing the destination protection class.
	uint32_t enable_nocprotect = 1;
	assert_no_err(copyfile_state_set(cpf_state, COPYFILE_STATE_NOCPROTECT, &enable_nocprotect));
	enable_nocprotect = 0;
	assert_no_err(copyfile_state_get(cpf_state, COPYFILE_STATE_NOCPROTECT, &enable_nocprotect));
	assert_equal(enable_nocprotect, 1, "%u");

	// Make sure that copying the former over the latter does not change the protection class.
	assert_no_err(copyfile(file_src, file_dst, cpf_state, 0));
	dst_class = getclass(file_dst);
	if (dst_class != PROTECTION_CLASS_C) {
		success = false;
		printf("Destination class was not C after initial copy (was %d instead)\n", dst_class);
	}

	// Repeat the copy without our cust state object, and make sure the class now changes.
	assert_no_err(copyfile(file_src, file_dst, NULL, 0));
	dst_class = getclass(file_dst);
	if (dst_class != PROTECTION_CLASS_D) {
		success = false;
		printf("Destination class was not D after final copy (was %d instead)\n", dst_class);
	}
	assert_no_err(copyfile_state_free(cpf_state));
	(void)unlink(file_src);
	(void)unlink(file_dst);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif // TARGET_OS_IPHONE
