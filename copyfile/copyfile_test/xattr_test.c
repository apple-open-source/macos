//
//  xattr_test.c
//  copyfile_test
//

#include <unistd.h>
#include <removefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "xattr_flags.h"

#include <sys/xattr.h>

#include "xattr_test.h"
#include "test_utils.h"

#define SRC_FILE_NAME		"src_file"
#define DST_FILE_NAME		"dst_file"
#define SMALL_XATTR_NAME	"small_xattr"
#define SMALL_XATTR_DATA	"drell"
#define BIG_XATTR_NAME		"big_xattr"
#define BIG_XATTR_SIZE		(20 * 1024 * 1024) // 20MiB

#define DEFAULT_CHAR_MOD	256

#define BACKUP_XF_POSTFIX	"#B"

#define METADATA_XATTR_PREFIX			"com.apple.metadata:"
#define SPOTLIGHT_COLLABORATION_XATTR	METADATA_XATTR_PREFIX "kMDItemCollaborationIdentifier"
#define SPOTLIGHT_SHARED_XATTR			METADATA_XATTR_PREFIX "kMDItemIsShared"
#define SPOTLIGHT_USERROLE_XATTR		METADATA_XATTR_PREFIX "kMDItemSharedItemCurrentUserRole"
#define SPOTLIGHT_OWNER_XATTR			METADATA_XATTR_PREFIX "kMDItemOwnerName"
#define SPOTLIGHT_FAVORITE_XATTR		METADATA_XATTR_PREFIX "kMDItemFavoriteRank"
#define OTHER_METADATA_XATTR			METADATA_XATTR_PREFIX "puzzlebub"

bool do_xattr_flags_test(__unused const char *apfs_test_directory, __unused size_t block_size) {
	// The idea here is to verify that the xattr_preserve_for_intent(3) API family returns sane results.
	printf("START [xattr_flags]\n");

	// The resource fork should be preserved on copies, but not safe saves.
	assert_equal_int(xattr_preserve_for_intent(XATTR_RESOURCEFORK_NAME, XATTR_OPERATION_INTENT_COPY), 1);
	assert_equal_int(xattr_preserve_for_intent(XATTR_RESOURCEFORK_NAME, XATTR_OPERATION_INTENT_SAVE), 0);

	// Spotlight xattrs should not be preserved for copy or share.
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_COLLABORATION_XATTR, XATTR_OPERATION_INTENT_COPY), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_SHARED_XATTR, XATTR_OPERATION_INTENT_COPY), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_USERROLE_XATTR, XATTR_OPERATION_INTENT_COPY), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_OWNER_XATTR, XATTR_OPERATION_INTENT_COPY), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_FAVORITE_XATTR, XATTR_OPERATION_INTENT_COPY), 0);

	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_COLLABORATION_XATTR, XATTR_OPERATION_INTENT_SHARE), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_SHARED_XATTR, XATTR_OPERATION_INTENT_SHARE), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_USERROLE_XATTR, XATTR_OPERATION_INTENT_SHARE), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_OWNER_XATTR, XATTR_OPERATION_INTENT_SHARE), 0);
	assert_equal_int(xattr_preserve_for_intent(SPOTLIGHT_FAVORITE_XATTR, XATTR_OPERATION_INTENT_SHARE), 0);

	// However, other "com.apple.metadata" xattrs should be preserved for copy and safe save.
	assert_equal_int(xattr_preserve_for_intent(OTHER_METADATA_XATTR, XATTR_OPERATION_INTENT_COPY), 1);
	assert_equal_int(xattr_preserve_for_intent(OTHER_METADATA_XATTR, XATTR_OPERATION_INTENT_SAVE), 1);

	// Verify that xattr_flags_from_name()/xattr_name_with_flags() recognize the backup xattr flag.
	assert_equal_ll(xattr_flags_from_name(SMALL_XATTR_NAME BACKUP_XF_POSTFIX), XATTR_FLAG_ONLY_BACKUP);
	assert_equal_str(xattr_name_with_flags(SMALL_XATTR_NAME, XATTR_FLAG_ONLY_BACKUP), SMALL_XATTR_NAME BACKUP_XF_POSTFIX);

	// Verify that backup xattrs are not copied, saved, shared, or synced...
	for (unsigned int intent = XATTR_OPERATION_INTENT_COPY; intent < XATTR_OPERATION_INTENT_BACKUP; intent++) {
		assert_equal_int(xattr_preserve_for_intent(SMALL_XATTR_NAME BACKUP_XF_POSTFIX, intent), 0);
	}

	// ...but they are backed-up.
	assert_equal_int(xattr_preserve_for_intent(SMALL_XATTR_NAME BACKUP_XF_POSTFIX, XATTR_OPERATION_INTENT_BACKUP), 1);

	printf("PASS  [xattr_flags]\n");
	return EXIT_SUCCESS;
}

static bool copy_and_verify_xattr_contents(const char *src_file, const char *dst_file, int src_file_fd, int dst_file_fd) {
	assert_no_err(copyfile(src_file, dst_file, NULL, COPYFILE_XATTR));

	return verify_fd_xattr_contents(src_file_fd, dst_file_fd);
}

bool do_xattr_test(const char *apfs_test_directory, __unused size_t block_size) {
	char test_dir[BSIZE_B] = {0};
	char src_file[BSIZE_B] = {0}, dst_file[BSIZE_B] = {0};
	char *big_xattr_data = NULL, buf[4096] = {0};
	int test_folder_id;
	int src_file_fd, dst_file_fd;
	bool success = true;

	printf("START [xattr]\n");

	// Get ready for the test.
	test_folder_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(apfs_test_directory, "xattr", test_folder_id, test_dir);
	assert_no_err(mkdir(test_dir, DEFAULT_MKDIR_PERM));

	// Create path names.
	assert_with_errno(snprintf(src_file, BSIZE_B, "%s/" SRC_FILE_NAME, test_dir) > 0);
	assert_with_errno(snprintf(dst_file, BSIZE_B, "%s/" DST_FILE_NAME, test_dir) > 0);

	// Create our files.
	src_file_fd = open(src_file, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(src_file_fd >= 0);
	dst_file_fd = open(dst_file, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(dst_file_fd >= 0);

	// Sanity check - empty copy
	success = success && copy_and_verify_xattr_contents(src_file, dst_file, src_file_fd, dst_file_fd);

	// Write a small xattr to the source file.
	assert_no_err(fsetxattr(src_file_fd, SMALL_XATTR_NAME, SMALL_XATTR_DATA, sizeof(SMALL_XATTR_DATA), 0, XATTR_CREATE));
	success = success && copy_and_verify_xattr_contents(src_file, dst_file, src_file_fd, dst_file_fd);

	// Create big xattr data
	assert_with_errno(big_xattr_data = malloc(BIG_XATTR_SIZE));
	for (int i = 0; i * sizeof(buf) < BIG_XATTR_SIZE; i++) {
		memset(buf, rand() % DEFAULT_CHAR_MOD, sizeof(buf));
		memcpy(big_xattr_data + (i * sizeof(buf)), buf, sizeof(buf));
	}

	// Write a big xattr to the source file.
	assert_no_err(fsetxattr(src_file_fd, BIG_XATTR_NAME, big_xattr_data, BIG_XATTR_SIZE, 0, XATTR_CREATE));
	success = success && copy_and_verify_xattr_contents(src_file, dst_file, src_file_fd, dst_file_fd);

	if (success) {
		printf("PASS  [xattr]\n");
	} else {
		printf("FAIL  [xattr]\n");
	}

	free(big_xattr_data);
	(void)removefile(test_dir, NULL, REMOVEFILE_RECURSIVE);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

