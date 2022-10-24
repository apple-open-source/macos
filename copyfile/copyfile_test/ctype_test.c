//
//  ctype_test.c
//  copyfile_test
//

#include <stdbool.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "../copyfile.h"

#include "ctype_test.h"
#include "test_utils.h"

#define COMPRESSED_FILE_NAME	"aria"
#define DESTINATION_FILE_NAME	"lieder"

static bool verify_compressed_type(const char *test_directory, const char *type,
	bool copy_stat) {
	char src_name[BSIZE_B], dst_name[BSIZE_B];
	struct stat sb = {0};
	struct timespec orig_mtime, orig_atime;
	copyfile_flags_t copy_flags = COPYFILE_DATA | COPYFILE_XATTR;
	int src_fd, dst_fd;
	bool success = true;

	// Here we verify that copyfile(COPYFILE_DATA|COPYFILE_XATTR) can
	// preserve the compressed status of a file compressed with the type provided.
	// (This is an issue for copyfile(3) since it allow-lists compressed file types.)

	// Create path names.
	assert_with_errno(snprintf(src_name, BSIZE_B, "%s/" COMPRESSED_FILE_NAME, test_directory) > 0);
	assert_with_errno(snprintf(dst_name, BSIZE_B, "%s/" DESTINATION_FILE_NAME, test_directory) > 0);

	// Create our source file.
	src_fd = open(src_name, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(src_fd >= 0);

	// Write some compressible data to the test file.
	write_compressible_data(src_fd);
	assert_no_err(close(src_fd));

	// Compress the file.
	compress_file(src_name, type);

	// Verify that it is compressed.
	assert_no_err(stat(src_name, &sb));
	success = success && verify_st_flags(&sb, UF_COMPRESSED);

	if (copy_stat) {
		// Save the {m, a}times of this file.
		orig_mtime = sb.st_mtimespec;
		orig_atime = sb.st_atimespec;

		copy_flags |= COPYFILE_STAT;
	}

	// Verify that copyfile(COPYFILE_DATA | COPYFILE_XATTR {| COPYFILE_STAT})
	// creates a compressed file.
	assert_no_err(copyfile(src_name, dst_name, NULL, copy_flags));
	assert_no_err(stat(dst_name, &sb));
	success = success && verify_st_flags(&sb, UF_COMPRESSED);

	if (copy_stat) {
		// Verify that the mtime & atime of the copy matches the original.
		success = success && verify_times("mtime", &orig_mtime, &sb.st_mtimespec);
		success = success && verify_times("atime", &orig_atime, &sb.st_atimespec);
	}

	// Verify that the contents are identical.
	success = success && verify_copy_contents(src_name, dst_name);

	// Verify that the extended attributes are identical.
	// (We need to re-open the source file to do so.)
	src_fd = open(src_name, O_RDONLY);
	assert_with_errno(src_fd >= 0);

	dst_fd = open(dst_name, O_RDONLY);
	assert_with_errno(dst_fd >= 0);

	success = success && verify_fd_xattr_contents(src_fd, dst_fd);

	// Post-test cleanup.
	assert_no_err(close(dst_fd));
	assert_no_err(close(src_fd));
	(void)removefile(dst_name, NULL, 0);
	(void)removefile(src_name, NULL, 0);

	return success;
}

bool do_compressed_type_test(const char *apfs_test_directory, __unused size_t block_size) {
	char test_dir[BSIZE_B] = {0};
	int test_folder_id;
	bool success = true;

	printf("START [compressed_type]\n");

	// Get ready for the test.
	test_folder_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(apfs_test_directory, "compressed_type", test_folder_id, test_dir);
	assert_no_err(mkdir(test_dir, DEFAULT_MKDIR_PERM));

	// Test with both the copyfile_data() and copyfile_stat() paths.
	success = verify_compressed_type(test_dir, "14", false);
	success = verify_compressed_type(test_dir, "14", true);

	if (success) {
		printf("PASS  [compressed_type]\n");
	} else {
		printf("FAIL  [compressed_type]\n");
	}

	(void)removefile(apfs_test_directory, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
