//
//  xdev_test.c
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
#include <fts.h>

#include "../copyfile.h"
#include "test_utils.h"

#if TARGET_OS_OSX

REGISTER_TEST(xdev, false, TIMEOUT_MIN(1));

#define DISK_IMAGE_SIZE_MB	4
#define EXTERIOR_DIR_NAME 	"xdev_dir"
#define EXTERIOR_FILE_NAME	"exterior_regular_file"
#define XDEV_DMG_MNT      	"xdev_mount"
#define INTERIOR_FILE_NAME	"interior_regular_file"
#define DST_DIR_NAME      	"xdev_dst"

typedef struct xdev_callback_ctx {
	dev_t xc_dev;
	bool  xc_should_alter_ftsent;
} xdev_callback_ctx_t;

__unused static int xdev_callback(int what, int stage, copyfile_state_t state,
	__unused const char *src, __unused const char *dst, __unused void *_ctx)
{
	xdev_callback_ctx_t *ctx = (xdev_callback_ctx_t *)_ctx;
	const FTSENT *entry;

	if (!(what == COPYFILE_RECURSE_DIR || what == COPYFILE_RECURSE_FILE ||
		what == COPYFILE_RECURSE_DIR_CLEANUP)) {
		return 0;
	}

	// Make sure that the FTS entry is present, is for a directory or regular file,
	// has stat information, and has the correct device number.
	assert_no_err(copyfile_state_get(state, COPYFILE_STATE_RECURSIVE_SRC_FTSENT, &entry));
	assert(entry);

	if (ctx->xc_should_alter_ftsent && what == COPYFILE_RECURSE_DIR &&
		stage == COPYFILE_ERR) {
		// ...with the exception that if we're seeing an error,
		// it should be because we modified the FTS entry in a previous iteration.
		assert_equal_int(entry->fts_info, FTS_SL);
		return COPYFILE_QUIT;
	}

	assert(entry->fts_info == FTS_D || entry->fts_info == FTS_F || entry->fts_info == FTS_DP);
	assert(entry->fts_statp);

	if (entry->fts_info == FTS_F) {
		// We only check that the device name is entirely equal
		// for regular files, since FTS will still return mountpoints
		// (but not their contents).
		assert_equal_int(entry->fts_statp->st_dev, ctx->xc_dev);
	}

	assert_call_fail(copyfile_state_set(state, COPYFILE_STATE_RECURSIVE_SRC_FTSENT, &entry), EINVAL);

	if (ctx->xc_should_alter_ftsent) {
		// Change the file type and our caller should check that this copy fails.
		// (Since we do not copy any symlinks here, we can use the symlink type
		// as our 'file changed' type.)
		((FTSENT *)entry)->fts_info = FTS_SL;
	}

	return 0;
}

bool do_xdev_test(__unused const char *apfs_test_directory, __unused size_t block_size) {
	bool success = true;

	int exterior_file_fd, file_in_dmg_fd, test_file_id;
	char exterior_dir_src[BSIZE_B] = {0}, dmg_mount_dir[BSIZE_B] = {0};
	char dst_dir[BSIZE_B] = {0}, dst_dmg_mount_dir[BSIZE_B];
	char exterior_file_name[BSIZE_B], file_in_dmg_name[BSIZE_B];
	char exterior_copied_file_name[BSIZE_B], file_in_dmg_copied_name[BSIZE_B];
	struct stat exterior_file_sb;

	test_file_id = rand() % DEFAULT_NAME_MOD;

	// Create the exterior directory and an interior disk image mount directory.
	assert_with_errno(snprintf(exterior_dir_src, BSIZE_B, "%s/%s", apfs_test_directory, EXTERIOR_DIR_NAME) > 0);
	assert_no_err(mkdir(exterior_dir_src, DEFAULT_MKDIR_PERM));
	assert_with_errno(snprintf(dmg_mount_dir, BSIZE_B, "%s/%s", exterior_dir_src, XDEV_DMG_MNT) > 0);
	assert_no_err(mkdir(dmg_mount_dir, DEFAULT_MKDIR_PERM));

	// Set up the buffers for these directories inside the destination (after the copy).
	assert_with_errno(snprintf(dst_dir, BSIZE_B, "%s/%s", apfs_test_directory, DST_DIR_NAME) > 0);
	assert_with_errno(snprintf(dst_dmg_mount_dir, BSIZE_B, "%s/%s", dst_dir, XDEV_DMG_MNT) > 0);

	// Create a file in the exterior directory.
	create_test_file_name(exterior_dir_src, EXTERIOR_FILE_NAME, test_file_id, exterior_file_name);
	exterior_file_fd = open(exterior_file_name, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(exterior_file_fd >= 0);
	write_compressible_data(exterior_file_fd);
	assert_no_err(fstat(exterior_file_fd, &exterior_file_sb));
	assert_no_err(close(exterior_file_fd));
	exterior_file_fd = -1;

	// Create a disk image and mount it on the interior disk image directory.
	disk_image_create(APFS_FSTYPE, dmg_mount_dir, DISK_IMAGE_SIZE_MB);

	// Add a file inside the internal disk image directory.
	create_test_file_name(dmg_mount_dir, INTERIOR_FILE_NAME, test_file_id, file_in_dmg_name);
	file_in_dmg_fd = open(file_in_dmg_name, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(file_in_dmg_fd >= 0);
	write_compressible_data(file_in_dmg_fd);
	assert_no_err(close(file_in_dmg_fd));
	file_in_dmg_fd = -1;

	//
	// Copy the exterior directory (not crossing mounts) to the destination directory -
	// at this point, the source looks like:
	//
	// apfs_test_directory
	//   exterior_dir
	//     exterior_regular_file
	//     disk_image_mount_directory [DISK IMAGE]
	//       interior_regular_file
	//
	//
	// After the copy, the layout should look like:
	//
	// apfs_test_directory
	//   exterior_dir
	//     ...
	//   destination_directory
	//     exterior_regular_file
	//   disk_image_mount_directory (no disk image)
	//
	// We will assert that the file inside the disk image does *not* get copied.
	//
	copyfile_state_t state = copyfile_state_alloc();
	xdev_callback_ctx_t cb_ctx = { .xc_dev = exterior_file_sb.st_dev };
	bool forbid_xmnt = true;
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_FORBID_CROSS_MOUNT, &forbid_xmnt));
	assert_no_err(copyfile_state_get(state, COPYFILE_STATE_FORBID_CROSS_MOUNT, &forbid_xmnt));
	assert(forbid_xmnt);
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, &xdev_callback));
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_STATUS_CTX, (void *)&cb_ctx));
	assert_no_err(copyfile(exterior_dir_src, dst_dir, state, COPYFILE_RECURSIVE|COPYFILE_ALL));

	// Make sure that the exterior file was copied and matches,
	// but that the file interior to the disk image is not present.
	create_test_file_name(dst_dir, EXTERIOR_FILE_NAME, test_file_id, exterior_copied_file_name);
	create_test_file_name(dst_dmg_mount_dir, INTERIOR_FILE_NAME, test_file_id, file_in_dmg_copied_name);

	success = success && verify_copy_contents(exterior_file_name, exterior_copied_file_name);
	assert_call_fail(open(file_in_dmg_copied_name, O_RDONLY), ENOENT);

	// Repeat the test, but now attempt to modify the FTSENT pointer in the state
	// and make sure that we detect the modification (and fail the copy).
	cb_ctx.xc_should_alter_ftsent = true;
	assert_call_fail(copyfile(exterior_dir_src, dst_dir, state, COPYFILE_RECURSIVE|COPYFILE_ALL), EBADF);

	assert_no_err(copyfile_state_free(state));
	state = NULL;

	// We're done!
	disk_image_destroy(dmg_mount_dir, false);

	(void)removefile(exterior_dir_src, NULL, REMOVEFILE_RECURSIVE);
	(void)removefile(dst_dir, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif // TARGET_OS_OSX
