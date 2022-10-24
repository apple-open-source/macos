//
//  revert_writable_test.c
//  copyfile_test
//

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include "../copyfile.h"
#include "revert_writable_test.h"
#include "test_utils.h"

#define SOURCE_FILE_NAME     	"revert_writable_source"
#define DESTINATION_FILE_NAME	"revert_writable_destination"
#define NONEXISTENT_FILE_PATH   "/tmp/dongero"

/*
 * Set up a source and destination file for testing copying, and return
 * the destination's stat(2) info.
 * The destination file will not be owner-writable and will be owned by a nondefault group.
 */
static void set_up_files(const char *src_path, const char *dst_path, struct stat *dst_sb) {
	int src_fd, dst_fd;

	// Create a source file.
	src_fd = open(src_path, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(src_fd >= 0);

	// Write some data into the source file, so that copyfile_data() will do work.
	write_compressible_data(src_fd);

	assert_no_err(close(src_fd));

	// Create an empty destination file.
	dst_fd = open(dst_path, DEFAULT_OPEN_FLAGS, S_IRWXU);
	assert_with_errno(dst_fd >= 0);

	// Make sure the file is not owner-writable and has a non-standard group.
	assert_no_err(fchown(dst_fd, getuid(), getgid() + 1));
	assert_no_err(fchmod(dst_fd, S_IRUSR | S_IXUSR)); // not writable

	// Get stat(2) information.
	assert_no_err(fstat(dst_fd, dst_sb));

	assert_no_err(close(dst_fd));
}

// Test copying functions.

typedef enum {
	COPYFILE = 0,
	FCOPYFILE,
} copyfile_copy_func_t;

// Each take a path to the source file and destination file, as set up above,
// and a copy function (copyfile() or fcopyfile()).
typedef void (*copy_func)(const char *, const char *, copyfile_copy_func_t);

#define open_test_file(PATH, FD, mode) \
	do { \
		FD = open(PATH, mode); \
		assert_with_errno(FD >= 0); \
	} while (0)

static void do_copy_nonexistent(__unused const char *src_path, const char *dst_path,
	copyfile_copy_func_t copy_func) {
	// We attempt to copy a non-existent file - `src_path` is ignored here.

	if (copy_func == FCOPYFILE) {
		int src_fd = -1, dst_fd;
		open_test_file(dst_path, dst_fd, O_RDWR);

		assert_with_errno(fcopyfile(src_fd, dst_fd, NULL, COPYFILE_DATA) == -1 &&
			(errno == EINVAL));

		assert_no_err(close(dst_fd));
	} else {
		assert_with_errno(copyfile(NONEXISTENT_FILE_PATH, dst_path, NULL, COPYFILE_DATA) == -1 &&
			(errno == ENOENT));
	}
}

static void do_copy_stat(const char *src_path, const char *dst_path,
	copyfile_copy_func_t copy_func) {
	// Negative test: we perform a copy that includes COPYFILE_STAT,
	// and make sure it copies ownership and permissions.

	if (copy_func == FCOPYFILE) {
		int src_fd, dst_fd;
		open_test_file(src_path, src_fd, O_RDONLY);
		open_test_file(dst_path, dst_fd, O_RDWR);

		assert_no_err(fcopyfile(src_fd, dst_fd, NULL, COPYFILE_ALL));

		assert_no_err(close(src_fd));
		assert_no_err(close(dst_fd));
	} else {
		assert_no_err(copyfile(src_path, dst_path, NULL, COPYFILE_ALL));
	}
}

static void do_copy_data(const char *src_path, const char *dst_path,
	copyfile_copy_func_t copy_func) {
	// Negative test: perform a regular data copy and see that ownership and permissions
	// are not altered on the destination.

	if (copy_func == FCOPYFILE) {
		int src_fd, dst_fd;
		open_test_file(src_path, src_fd, O_RDONLY);
		open_test_file(dst_path, dst_fd, O_RDWR);

		assert_no_err(fcopyfile(src_fd, dst_fd, NULL, COPYFILE_DATA));

		assert_no_err(close(src_fd));
		assert_no_err(close(dst_fd));
	} else {
		assert_no_err(copyfile(src_path, dst_path, NULL, COPYFILE_DATA));
	}
}

static void do_copy_unpack(const char *src_path, const char *dst_path,
	copyfile_copy_func_t copy_func) {
	// Test that a copy that fails midway (in our case, by passing COPYFILE_UNPACK
	// on a file without a valid AppleDouble header)
	// rolls back ownership and permissions on the destination.

	if (copy_func == FCOPYFILE) {
		int src_fd, dst_fd;
		open_test_file(src_path, src_fd, O_RDONLY);
		open_test_file(dst_path, dst_fd, O_RDWR);

		assert_with_errno(fcopyfile(src_fd, dst_fd, NULL, COPYFILE_UNPACK) == -1);

		assert_no_err(close(src_fd));
		assert_no_err(close(dst_fd));
	} else {
		assert_with_errno(copyfile(src_path, dst_path, NULL, COPYFILE_UNPACK) == -1);
	}
}

static void do_copy_check(const char *src_path, const char *dst_path,
	copyfile_copy_func_t copy_func) {
	// Make sure that COPYFILE_CHECK does not alter ownership or permissions.

	if (copy_func == FCOPYFILE) {
		// no COPYFILE_CHECK for fcopyfile()
		return;
	} else {
		// Since we pass ALL (and the file has no xattrs) COPYFILE_CHECK should return 0.
		assert_no_err(copyfile(src_path, dst_path, NULL, COPYFILE_CHECK|COPYFILE_ALL));
	}
}

typedef struct {
	copy_func func;     // pointer to function to do the 'copy'
	const char * name;  // null terminated string describing the test
	bool alters_perms;  // if true, permissions are expected to not match 
} revert_writable_test_func;

#define NUM_TEST_FUNCTIONS 5
revert_writable_test_func revert_writable_test_functions[NUM_TEST_FUNCTIONS] = {
	{ do_copy_nonexistent,      "copy_nonexistent", false },
	{ do_copy_stat,             "copy_stat",        true  },
	{ do_copy_data,             "copy_data",        false },
	{ do_copy_unpack,           "copy_unpack",      false },
	{ do_copy_check,            "copy_check",       false },
};

bool do_revert_writable_test(const char *apfs_test_directory, __unused size_t block_size) {
	char test_dir[BSIZE_B] = {0}, test_src[BSIZE_B], test_dst[BSIZE_B];
	int test_folder_id;
	bool success = true, sub_test_success;

	if (geteuid() != 0) {
		printf("Skipping revert_writable tests, because we are not root.\n");
		return EXIT_SUCCESS;
	}

	test_folder_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(apfs_test_directory, "revert_writable", test_folder_id, test_dir);
	assert_no_err(mkdir(test_dir, DEFAULT_MKDIR_PERM));
	create_test_file_name(test_dir, SOURCE_FILE_NAME, test_folder_id, test_src);
	create_test_file_name(test_dir, DESTINATION_FILE_NAME, test_folder_id, test_dst);

	// perform tests in order.
	for (int sub_test = 0; sub_test < NUM_TEST_FUNCTIONS; sub_test++) {
		printf("START [revert_writable_%s]\n", revert_writable_test_functions[sub_test].name);
		sub_test_success = true;

		for (copyfile_copy_func_t copy_func = COPYFILE; copy_func <= FCOPYFILE; copy_func++) {
			// Set up test files.
			struct stat orig_dst_sb, end_dst_sb;
			set_up_files(test_src, test_dst, &orig_dst_sb);

			// Run the copy using the current copy function.
			revert_writable_test_functions[sub_test].func(test_src, test_dst, copy_func);

			// Verify that `dst's stat info is correct.
			assert_no_err(stat(test_dst, &end_dst_sb));
			if (revert_writable_test_functions[sub_test].alters_perms) {
				sub_test_success = sub_test_success &&
					memcmp(&orig_dst_sb, &end_dst_sb, sizeof(struct stat));
			} else {
				sub_test_success = sub_test_success &&
					verify_st_ids_and_mode(&orig_dst_sb, &end_dst_sb);
			}

			if (!sub_test_success) {
				printf("FAIL  [revert_writable_%s (copy_func %d)]\n",
					revert_writable_test_functions[sub_test].name, copy_func);
				success = false;
			}

			// Clean up test files.
			(void)removefile(test_src, NULL, 0);
			(void)removefile(test_dst, NULL, 0);
		}

		if (sub_test_success) {
			printf("PASS  [revert_writable_%s]\n", revert_writable_test_functions[sub_test].name);
		}
	}

	(void)removefile(test_dir, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
