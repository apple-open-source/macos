//
//  ctype_test.c
//  copyfile_test
//
#include <System/sys/decmpfs.h>
#include <System/sys/fsctl.h>

#include <paths.h>
#include <removefile.h>
#include <stdbool.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#include "../copyfile.h"

#include "ctype_test.h"
#include "test_utils.h"

#define COMPRESSED_FILE_NAME	"aria"
#define DESTINATION_FILE_NAME	"lieder"

static int _delete_decmpfs_callback(int what, int stage, copyfile_state_t state, __unused const char *src, __unused const char *dst, __unused void *ctx) {
	if (what == COPYFILE_COPY_XATTR && stage == COPYFILE_FINISH) {
		// delete DECMPFS_XATTR_NAME to invalidate compression
		const char *name = NULL;
		int dst_fd = -1;
		if (copyfile_state_get(state, COPYFILE_STATE_XATTRNAME, &name) == 0 &&
			copyfile_state_get(state, COPYFILE_STATE_DST_FD, &dst_fd) == 0 &&
			name && strcmp(name, DECMPFS_XATTR_NAME) == 0 &&
			dst_fd != -1)
		{
			(void)fremovexattr(dst_fd, name, XATTR_SHOWCOMPRESSION);
		}
	}
	return COPYFILE_CONTINUE;
}

static errno_t safe_fchflags(int fd, const uint32_t new_flags, uint32_t preserve_flags) {
	struct fsioc_cas_bsdflags cas = {};
	struct stat sb = {};
	if (fstat(fd, &sb) != 0) {
		return errno;
	}

	cas.expected_flags = sb.st_flags;
	cas.new_flags = new_flags | (sb.st_flags & preserve_flags);
	cas.actual_flags = ~0;
	if (ffsctl(fd, FSIOC_CAS_BSDFLAGS, &cas, 0) != 0) {
		return errno;
	} else if (sb.st_flags != cas.actual_flags) {
		errno = EAGAIN;
		return EAGAIN;
	}
	return 0;
}

static bool verify_compressed_type(const char *test_directory, const char *type,
	bool copy_stat) {
	char src_name[BSIZE_B] = "", dst_name[BSIZE_B] = "";
	struct stat sb = {0};
	struct timespec orig_mtime = {0}, orig_atime = {0};
	off_t orig_size = 0;
	ssize_t orig_rsrc_size = 0;
	const copyfile_flags_t copy_flags = COPYFILE_DATA | COPYFILE_XATTR | (copy_stat ? COPYFILE_STAT : 0);
	int src_fd = -1, dst_fd = -1;
	bool success = true;

	// Here we verify that copyfile(COPYFILE_DATA|COPYFILE_XATTR) can
	// preserve the compressed status of a file compressed with the type provided.
	// (This is an issue for copyfile(3) since it allow-lists compressed file types.)

	// Create path names.
	assert_with_errno(snprintf(src_name, BSIZE_B, "%s/" COMPRESSED_FILE_NAME, test_directory) > 0);
	assert_with_errno(snprintf(dst_name, BSIZE_B, "%s/" DESTINATION_FILE_NAME, test_directory) > 0);

	// Create our source file.
	assert_fd(src_fd = open(src_name, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM));

	// Write some compressible data to the test file.
	write_compressible_data(src_fd);

	// Close the file, compress it, and open it readonly.
	assert_no_err(close(src_fd));
	compress_file(src_name, type);
	assert_fd(src_fd = open(src_name, O_RDONLY));

	// Verify that it is compressed.
	assert_no_err(fstat(src_fd, &sb));
	assert_true(verify_st_flags(&sb, UF_COMPRESSED, UF_COMPRESSED));

	// Latch attributes of the source file.
	orig_size = sb.st_size;
	orig_mtime = sb.st_mtimespec;
	orig_atime = sb.st_atimespec;
	orig_rsrc_size = fgetxattr(src_fd, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, XATTR_SHOWCOMPRESSION);

	// Verify that copyfile(COPYFILE_DATA | COPYFILE_XATTR {| COPYFILE_STAT})
	// creates a compressed file.
	assert_no_err(copyfile(src_name, dst_name, NULL, copy_flags));
	assert_no_err(stat(dst_name, &sb));
	success = success && verify_st_flags(&sb, UF_COMPRESSED, UF_COMPRESSED);

	if (copy_stat) {
		// Verify that the mtime & atime of the copy matches the original.
		success = success && verify_times("mtime", &orig_mtime, &sb.st_mtimespec);
		success = success && verify_times("atime", &orig_atime, &sb.st_atimespec);
	}

	// Verify that the contents are identical.
	success = success && verify_copy_contents(src_name, dst_name);

	// Verify that the extended attributes are identical.
	assert_fd(dst_fd = open(dst_name, O_RDONLY));
	success = success && verify_fd_xattr_contents(src_fd, dst_fd);
	assert_no_err(close(dst_fd));

	// Verify that a standalone COPYFILE_STAT *does* strip UF_COMPRESSED.
	if (copy_stat) {
		assert_no_err(copyfile(src_name, dst_name, NULL, copy_flags));
		assert_no_err(copyfile(_PATH_DEVNULL, dst_name, NULL, COPYFILE_STAT));
		assert_no_err(stat(dst_name, &sb));
		success = success && verify_st_flags(&sb, UF_COMPRESSED, 0);
	}

	// Verify that st_flags on dst are handled correctly. COPYFILE_STAT
	// should replace them; otherwise they should be unchanged. We want
	// to test this in *all* flows that might set st_flags, so compute
	// the expected flags here and use them in our test cases below.
	const uint32_t flags2set = UF_HIDDEN;
	const uint32_t flags2expect = (copy_stat) ? 0 : flags2set;

	// Copy src_fd to dst_fd using fcopyfile().
	//
	// NOTE: dst_fd is intentionally O_RDONLY here so fcopyfile() will fail
	// if we regress rdar://91336242 and don't early-exit from copyfile_data().
	(void)unlink(dst_name);
	assert_fd(dst_fd = open(dst_name, O_RDONLY|O_CREAT|O_EXCL, DEFAULT_OPEN_PERM));
	assert_no_err(safe_fchflags(dst_fd, flags2set, 0));
	assert_with_errno(lseek(src_fd, 0, SEEK_SET) == 0);
	assert_no_err(fcopyfile(src_fd, dst_fd, NULL, copy_flags));
	assert_no_err(fstat(dst_fd, &sb));
	success = success && verify_st_flags(&sb, UF_COMPRESSED|flags2set, UF_COMPRESSED|flags2expect);
	success = success && verify_copy_contents(src_name, dst_name);
	success = success && verify_fd_contents(src_fd, 0, dst_fd, 0, orig_size);
	success = success && verify_fd_xattr_contents(src_fd, dst_fd);
	assert_no_err(close(dst_fd));

	// Confirm our fallback to copyfile_data() works when UF_COMPRESSED
	// cannot be set. We force this to fail by deleting DECMPFS_XATTR_NAME
	// immediately after copyfile_xattr() copies it, before copyfile_data().
	copyfile_state_t badxattr_state = copyfile_state_alloc();
	(void)unlink(dst_name);
	assert_fd(dst_fd = open(dst_name, O_RDWR|O_CREAT|O_EXCL, DEFAULT_OPEN_PERM));
	assert_no_err(safe_fchflags(dst_fd, flags2set, 0));
	assert_no_err(copyfile_state_set(badxattr_state, COPYFILE_STATE_STATUS_CB, &_delete_decmpfs_callback));
	assert_no_err(copyfile(src_name, dst_name, badxattr_state, copy_flags));
	assert_no_err(fstat(dst_fd, &sb));
	success = success && verify_st_flags(&sb, UF_COMPRESSED|flags2set, flags2expect);
	success = success && verify_copy_contents(src_name, dst_name);
	success = success && verify_fd_contents(src_fd, 0, dst_fd, 0, orig_size);
	if (orig_rsrc_size >= 0) {
		// Confirm that our fallback removed the unused resource fork, which can be very large.
		success = success && fgetxattr(dst_fd, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, XATTR_SHOWCOMPRESSION) == -1 && errno == ENOATTR;
	}
	assert_no_err(close(dst_fd));
	copyfile_state_free(badxattr_state);

	// Try fcopyfile() with non-zero src offset.
	assert_fd(dst_fd = open(dst_name, O_RDWR|O_TRUNC));
	assert_with_errno(lseek(src_fd, 1, SEEK_SET) == 1);
	assert_with_errno(lseek(dst_fd, 0, SEEK_SET) == 0);
	assert_no_err(fcopyfile(src_fd, dst_fd, NULL, copy_flags));
	assert_no_err(fstat(dst_fd, &sb));
	success = success && verify_st_flags(&sb, UF_COMPRESSED, 0);
	success = success && verify_fd_contents(src_fd, 1, dst_fd, 0, orig_size - 1);
	success = success && (sb.st_size == (orig_size - 1));
	assert_no_err(close(dst_fd));

	// Try fcopyfile() with non-zero dst offset.
	assert_fd(dst_fd = open(dst_name, O_RDWR|O_TRUNC));
	assert_with_errno(lseek(src_fd, 0, SEEK_SET) == 0);
	assert_with_errno(lseek(dst_fd, 1, SEEK_SET) == 1);
	assert_no_err(fcopyfile(src_fd, dst_fd, NULL, copy_flags));
	assert_no_err(fstat(dst_fd, &sb));
	success = success && verify_st_flags(&sb, UF_COMPRESSED, 0);
	success = success && verify_fd_contents(src_fd, 0, dst_fd, 1, orig_size - 1);
	success = success && (sb.st_size == orig_size); // not (orig_size+1) due to fcopyfile() not honoring the initial offset
	assert_no_err(close(dst_fd));

	// Post-test cleanup.
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

	(void)removefile(test_dir, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
