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

#include "../copyfile.h"
#include "xdev_test.h"
#include "test_utils.h"

#define DISK_IMAGE_SIZE_MB	4
#define EXTERIOR_DIR_NAME 	"xdev_dir"
#define EXTERIOR_FILE_NAME	"exterior_regular_file"
#define XDEV_DMG_MNT      	"xdev_mount"
#define INTERIOR_FILE_NAME	"interior_regular_file"
#define DST_DIR_NAME      	"xdev_dst"

bool do_xdev_test(const char *apfs_test_directory, __unused size_t block_size) {
	bool success = true;

#if TARGET_OS_OSX
	int exterior_file_fd, file_in_dmg_fd, test_file_id;
	char exterior_dir_src[BSIZE_B] = {0}, dmg_mount_dir[BSIZE_B] = {0};
	char dst_dir[BSIZE_B] = {0}, dst_dmg_mount_dir[BSIZE_B];
	char exterior_file_name[BSIZE_B], file_in_dmg_name[BSIZE_B];
	char exterior_copied_file_name[BSIZE_B], file_in_dmg_copied_name[BSIZE_B];

	printf("START [xdev]\n");

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
	bool forbid_xmnt = true;
	assert_no_err(copyfile_state_set(state, COPYFILE_STATE_FORBID_CROSS_MOUNT, &forbid_xmnt));
	assert_no_err(copyfile(exterior_dir_src, dst_dir, state, COPYFILE_RECURSIVE|COPYFILE_ALL));
	copyfile_state_free(state);
	state = NULL;

	// Make sure that the exterior file was copied and matches,
	// but that the file interior to the disk image is not present.
	create_test_file_name(dst_dir, EXTERIOR_FILE_NAME, test_file_id, exterior_copied_file_name);
	create_test_file_name(dst_dmg_mount_dir, INTERIOR_FILE_NAME, test_file_id, file_in_dmg_copied_name);

	success = success && verify_copy_contents(exterior_file_name, exterior_copied_file_name);
	assert_call_fail(open(file_in_dmg_copied_name, O_RDONLY), ENOENT);

	if (success) {
		printf("PASS  [xdev]\n");
	} else {
		printf("FAIL  [xdev]\n");
	}

	disk_image_destroy(dmg_mount_dir, false);

	(void)removefile(exterior_dir_src, NULL, REMOVEFILE_RECURSIVE);
	(void)removefile(dst_dir, NULL, REMOVEFILE_RECURSIVE);

#else
	printf("Skipping xdev tests as we are not on macOS.\n");
#endif

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
