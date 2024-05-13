//
//  recursive_test.c
//  copyfile_test
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <fts.h>

#include "test_utils.h"

REGISTER_TEST(recursive_symlink_overwrite, false, TIMEOUT_MIN(1));
REGISTER_TEST(recursive_with_symlink, false, TIMEOUT_MIN(1));
REGISTER_TEST(recursive_symlink_root, false, TIMEOUT_MIN(1));

bool do_recursive_symlink_overwrite_test(const char *apfs_test_directory, __unused size_t block_size) {
	int file_src_fd, test_file_id;
	char exterior_dir_src[BSIZE_B] = {0}, interior_dir_src[BSIZE_B] = {0};
	char exterior_dir_dst[BSIZE_B] = {0};
	char file_src[BSIZE_B] = {0};
	char file_dst[BSIZE_B] = {0}, link_dst[BSIZE_B] = {0};
	copyfile_state_t state = copyfile_state_alloc();

	// Construct our source layout:
	//
	// apfs_test_directory
	//   recursive_symlink_src
	//     interior (directory)
	//     testfile-NUM.file_with_data
	//
	assert_with_errno(snprintf(exterior_dir_src, BSIZE_B, "%s/recursive_symlink_src/", apfs_test_directory) > 0);
	assert_with_errno(snprintf(interior_dir_src, BSIZE_B, "%s/interior", exterior_dir_src) > 0);
	assert_with_errno(snprintf(exterior_dir_dst, BSIZE_B, "%s/recursive_symlink_dst", apfs_test_directory) > 0);

	assert_no_err(mkdir(exterior_dir_src, DEFAULT_MKDIR_PERM));
	assert_no_err(mkdir(interior_dir_src, DEFAULT_MKDIR_PERM));
	assert_no_err(mkdir(exterior_dir_dst, DEFAULT_MKDIR_PERM));

	test_file_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(exterior_dir_src, "file_with_data", test_file_id, file_src);
	create_test_file_name(exterior_dir_dst, "file_with_data", test_file_id, file_dst);

	file_src_fd = open(file_src, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(file_src_fd >= 0);
	assert_with_errno(write(file_src_fd, "a", 1) == 1);

	// Create a symlink in the destination directory at the same path
	// as a directory in the source.
	// At this point in time, our destination layout will look like:
	//
	// apfs_test_directory
	//   recursive_symlink_dst
	//     interior (symlink)
	//
	assert_with_errno(snprintf(link_dst, BSIZE_B, "%s/interior", exterior_dir_dst) > 0);
	assert_no_err(symlink(file_dst, link_dst)); // (The destination file doesn't currently exist.)

	// Now, try to copy the file over the symlink,
	// setting our new strict mode, and see that it fails.
	uint32_t forbid_existing_symlinks = 1;
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_FORBID_DST_EXISTING_SYMLINKS, &forbid_existing_symlinks));
	assert_call_fail(copyfile(exterior_dir_src, link_dst, state, COPYFILE_ALL), EBADF);

	// Repeat with the directory as a whole - because of the layout we've constructed,
	// we'll fail to overwrite "interior" (which is already a symlink in the destination already).
	assert_call_fail(copyfile(exterior_dir_src, exterior_dir_dst, state, COPYFILE_ALL|COPYFILE_RECURSIVE), EBADF);

	assert_no_err(copyfile_state_free(state));
	state = NULL;

	// We're done!
	assert_no_err(removefile(exterior_dir_src, NULL, REMOVEFILE_RECURSIVE));
	assert_no_err(removefile(exterior_dir_dst, NULL, REMOVEFILE_RECURSIVE));

	return EXIT_SUCCESS;
}


typedef struct rws_callback_ctx {
	const bool has_interior_dir;
	const bool has_link;
	bool rc_file_found;
	unsigned rc_dirs_found;
	bool rc_link_found;
} rws_callback_ctx_t;

static int recursive_with_symlink_callback(int what, __unused int stage, copyfile_state_t state,
	__unused const char *src, __unused const char *dst, __unused void *_ctx) {
	rws_callback_ctx_t *ctx = (rws_callback_ctx_t *)_ctx;
	const unsigned int num_directories = ctx->has_interior_dir ? 2 : 1;
	const FTSENT *entry;

	if (!(what == COPYFILE_RECURSE_DIR || what == COPYFILE_RECURSE_FILE ||
		what == COPYFILE_RECURSE_DIR_CLEANUP)) {
		// Only inspect recurse dir / file (which also catches symlinks).
		return COPYFILE_CONTINUE;
	} else if (stage == COPYFILE_FINISH) {
		// Ignore "finish", since we only care about one event per file type.
		return COPYFILE_CONTINUE;
	}

	assert_no_err(copyfile_state_get(state, COPYFILE_STATE_RECURSIVE_SRC_FTSENT, &entry));
	assert(entry);

	assert(entry->fts_info == FTS_D || entry->fts_info == FTS_F ||
		entry->fts_info == FTS_SL || entry->fts_info == FTS_DP);
	assert(entry->fts_statp);

	if (entry->fts_info == FTS_D) {
		// Make sure that we only see one or two directories (depending upon the test).
		assert(ctx->rc_dirs_found < num_directories);
		ctx->rc_dirs_found++;
	} else if (entry->fts_info == FTS_F) {
		// Make sure we only see one file.
		assert(!ctx->rc_file_found);
		ctx->rc_file_found = true;
	} else if (entry->fts_info == FTS_SL) {
		// Make sure we only see the link after we've seen the (one or two) directories
		// and the one file (and only see a link if we're expecting one).
		assert(ctx->has_link);
		assert((ctx->rc_dirs_found == num_directories) && ctx->rc_file_found && !ctx->rc_link_found);
		ctx->rc_link_found = true;
	} else if (entry->fts_info == FTS_DP) {
		// Make sure we only see this after we've seen a directory and not the link
		// (since there are up to two directories, we'll see this once for each directory).
		assert(ctx->rc_dirs_found && !ctx->rc_link_found);
	}

	return COPYFILE_CONTINUE;
}

bool do_recursive_with_symlink_test(const char *apfs_test_directory, __unused size_t block_size) {
	int exterior_file_src_fd, test_file_id;
	char exterior_dir_src[BSIZE_B] = {0}, interior_dir_src[BSIZE_B] = {0};
	char exterior_dir_dst[BSIZE_B] = {0}, interior_dir_dst[BSIZE_B] = {0};
	char exterior_file_src[BSIZE_B] = {0}, exterior_link_src[BSIZE_B] = {0};
	char exterior_file_dst[BSIZE_B] = {0}, exterior_link_dst[BSIZE_B] = {0};
	char exterior_link_dst_path[BSIZE_B] = {0};
	struct stat exterior_file_src_sb, exterior_file_dst_sb, interior_dir_dst_sb, exterior_link_dst_sb;
	copyfile_state_t state = copyfile_state_alloc();
	rws_callback_ctx_t cb_ctx = { .has_interior_dir = true, .has_link = true, };
	bool success = true;

	// Construct our source layout:
	//
	// apfs_test_directory
	//   recursive_copy_src
	//     interior (directory)
	//     testfile-NUM.file_with_data
	//     testfile-NUM.symlink -> testfile-NUM.file_with_data
	//
	assert_with_errno(snprintf(exterior_dir_src, BSIZE_B, "%s/recursive_copy_src", apfs_test_directory) > 0);
	assert_with_errno(snprintf(interior_dir_src, BSIZE_B, "%s/interior", exterior_dir_src) > 0);
	assert_with_errno(snprintf(exterior_dir_dst, BSIZE_B, "%s/recursive_copy_dst", apfs_test_directory) > 0);
	assert_with_errno(snprintf(interior_dir_dst, BSIZE_B, "%s/interior", exterior_dir_dst) > 0);

	assert_no_err(mkdir(exterior_dir_src, DEFAULT_MKDIR_PERM));
	assert_no_err(mkdir(interior_dir_src, DEFAULT_MKDIR_PERM));

	test_file_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(exterior_dir_src, "file_with_data", test_file_id, exterior_file_src);
	create_test_file_name(exterior_dir_dst, "file_with_data", test_file_id, exterior_file_dst);

	exterior_file_src_fd = open(exterior_file_src, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(exterior_file_src_fd >= 0);
	assert_with_errno(write(exterior_file_src_fd, "a", 1) == 1);

	create_test_file_name(exterior_dir_src, "symlink", test_file_id, exterior_link_src);
	create_test_file_name(exterior_dir_dst, "symlink", test_file_id, exterior_link_dst);
	assert_no_err(symlink(exterior_file_src, exterior_link_src));

	// Now, recursively copy our folder using COPYFILE_RECURSIVE.
	// (We use COPYFILE_CLONE here so that we clone the actual link,
	// instead of creating a destination file that matches the original.)
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, &recursive_with_symlink_callback));
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CTX, (void *)&cb_ctx));
	assert_no_err(copyfile(exterior_dir_src, exterior_dir_dst, state, COPYFILE_ALL|COPYFILE_CLONE|COPYFILE_RECURSIVE));

	// The files were (hopefully) copied. Now, we must verify:
	// 1. The regular file copied over is identical.
	// 2. The directory copied over looks sane.
	// 3. The symlink was copied, and it was done after the other files.
	//
	// The target layout we're aiming for is:
	//
	// apfs_test_directory
	//   recursive_copy_dst
	//     interior (directory)
	//     testfile-NUM.file_with_data
	//     testfile-NUM.symlink -> testfile-NUM.file_with_data
	//

	// 1. The regular file copied over is identical.
	assert(cb_ctx.rc_file_found);
	assert_no_err(fstat(exterior_file_src_fd, &exterior_file_src_sb));
	assert_no_err(stat(exterior_file_dst, &exterior_file_dst_sb));

	success = success && verify_copy_sizes(&exterior_file_src_sb, &exterior_file_dst_sb, NULL, true, 0);
	success = success && verify_copy_contents(exterior_file_src, exterior_file_dst);

	assert_no_err(close(exterior_file_src_fd));
	exterior_file_src_fd = -1;

	// 2. The directory copied over looks sane.
	assert(cb_ctx.rc_dirs_found == 2);
	assert_no_err(stat(interior_dir_dst, &interior_dir_dst_sb));
	success = success && S_ISDIR(interior_dir_dst_sb.st_mode);

	// 3. The symlink was copied, and it was done after the other files.
	// (This last detail is enforced by our copy callback.)
	assert(cb_ctx.rc_link_found);
	assert_no_err(lstat(exterior_link_dst, &exterior_link_dst_sb));
	success = success && S_ISLNK(exterior_link_dst_sb.st_mode);
	assert_with_errno(readlink(exterior_link_dst, exterior_link_dst_path, sizeof(exterior_link_dst_path)) > 0);
	assert(!strcmp(exterior_file_src, exterior_link_dst_path));

	assert_no_err(copyfile_state_free(state));
	state = NULL;

	assert_no_err(removefile(exterior_dir_dst, NULL, REMOVEFILE_RECURSIVE));

	// Repeat the test without COPYFILE_CLONE (to make sure the copy doesn't error out).
	// In this case, we will actually copy the target of the symlink itself,
	// so we need to verify that the file's contents match.
	// The target layout we're aiming for is:
	//
	// apfs_test_directory
	//   recursive_copy_dst
	//     interior (directory)
	//     testfile-NUM.file_with_data
	//     testfile-NUM.symlink (regular file - same contents as testfile-NUM.file_with_data)
	//
	assert_no_err(copyfile(exterior_dir_src, exterior_dir_dst, NULL, COPYFILE_ALL|COPYFILE_RECURSIVE));

	assert_no_err(lstat(exterior_link_dst, &exterior_link_dst_sb)); // Should be a regular file now.
	success = success && S_ISREG(exterior_link_dst_sb.st_mode);
	success = success && verify_copy_sizes(&exterior_file_src_sb, &exterior_link_dst_sb, NULL, true, 0);
	success = success && verify_copy_contents(exterior_file_src, exterior_link_dst);

	// We're done!
	(void)removefile(exterior_dir_src, NULL, REMOVEFILE_RECURSIVE);
	(void)removefile(exterior_dir_dst, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

bool do_recursive_symlink_root_test(const char *apfs_test_directory, __unused size_t block_size) {
	int exterior_file_src_fd, test_file_id;
	char exterior_src[BSIZE_B] = {0}; // a symbolic link this time
	char exterior_real_src[BSIZE_B] = {0}; // a real directory
	char exterior_dst[BSIZE_B] = {0};
	char exterior_file_src[BSIZE_B] = {0}, exterior_file_dst[BSIZE_B] = {0};
	char exterior_dst_path[BSIZE_B] = {0};
	struct stat exterior_file_src_sb, exterior_file_dst_sb, dst_sb;
	copyfile_state_t state = copyfile_state_alloc();
	rws_callback_ctx_t cb_ctx = { .has_interior_dir = false, .has_link = false, };
	bool success = true;

	// Construct our source layout:
	//
	// apfs_test_directory
	//   symlink_root_real_src
	//     testfile-NUM.file_with_data
	//   symlink_root_src -> symlink_real_src
	//
	// We will copy `src` (which is a symlink) recursively -
	// this should copy the entire hierarchy without COPYFILE_NOFOLLOW_SRC,
	// and only src as a symlink with COPYFILE_NOFOLLOW_SRC.
	//
	assert_with_errno(snprintf(exterior_src, BSIZE_B, "%s/symlink_root_src", apfs_test_directory) > 0);
	assert_with_errno(snprintf(exterior_real_src, BSIZE_B, "%s/symlink_root_real_src", apfs_test_directory) > 0);
	assert_with_errno(snprintf(exterior_dst, BSIZE_B, "%s/symlink_root_dst", apfs_test_directory) > 0);

	assert_no_err(mkdir(exterior_real_src, DEFAULT_MKDIR_PERM));
	assert_no_err(symlink(exterior_real_src, exterior_src));

	test_file_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(exterior_real_src, "file_with_data", test_file_id, exterior_file_src);
	create_test_file_name(exterior_dst, "file_with_data", test_file_id, exterior_file_dst);

	exterior_file_src_fd = open(exterior_file_src, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(exterior_file_src_fd >= 0);
	assert_with_errno(write(exterior_file_src_fd, "a", 1) == 1);

	// Now, recursively copy our folder using COPYFILE_RECURSIVE.
	// (We can't pass COPYFILE_CLONE here because it will prevent FTS_COMFOLLOW
	// during COPYFILE_RECURSIVE, and we are trying to trigger that situation.)
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, &recursive_with_symlink_callback));
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CTX, (void *)&cb_ctx));
	assert_no_err(copyfile(exterior_src, exterior_dst, state, COPYFILE_ALL|COPYFILE_RECURSIVE));

	// The files were (hopefully) copied, and we returned no error.
	// We need to verify that the regular file copied over is identical.
	//
	// The target layout we're aiming for is:
	//
	// apfs_test_directory
	//   symlink_root_dst
	//     testfile-NUM.file_with_data
	//

	// Verify the regular file copied over is identical, and that's all we saw.
	assert(cb_ctx.rc_file_found);
	assert(!cb_ctx.rc_link_found);
	assert(cb_ctx.rc_dirs_found == 1);
	assert_no_err(fstat(exterior_file_src_fd, &exterior_file_src_sb));
	assert_no_err(stat(exterior_file_dst, &exterior_file_dst_sb));

	success = success && verify_copy_sizes(&exterior_file_src_sb, &exterior_file_dst_sb, NULL, true, 0);
	success = success && verify_copy_contents(exterior_file_src, exterior_file_dst);

	assert_no_err(close(exterior_file_src_fd));
	exterior_file_src_fd = -1;

	assert_no_err(copyfile_state_free(state));
	state = NULL;

	assert_no_err(removefile(exterior_dst, NULL, REMOVEFILE_RECURSIVE));

	// Now, repeat the test with COPYFILE_NOFOLLOW_SRC, and verify that we only created a symlink.
	assert_no_err(copyfile(exterior_src, exterior_dst, NULL, COPYFILE_ALL|COPYFILE_RECURSIVE|COPYFILE_NOFOLLOW_SRC));

	// Verify that the destination is actually a symlink and points to exterior_real_src.
	assert_no_err(lstat(exterior_dst, &dst_sb));
	success = success && S_ISLNK(dst_sb.st_mode);
	assert_with_errno(readlink(exterior_dst, exterior_dst_path, sizeof(exterior_dst_path)) > 0);
	assert(!strcmp(exterior_real_src, exterior_dst_path));

	// We're done!
	(void)removefile(exterior_src, NULL, REMOVEFILE_RECURSIVE);
	(void)removefile(exterior_real_src, NULL, REMOVEFILE_RECURSIVE);
	(void)removefile(exterior_dst, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
