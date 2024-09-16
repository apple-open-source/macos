//
//  acl_test.c
//  copyfile_test
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <removefile.h>
#include <sys/acl.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "../copyfile_private.h"
#include "../xattr_properties.h"

#include "test_utils.h"

REGISTER_TEST(acl, false, 30);

bool do_acl_test(const char *test_directory, __unused size_t block_size) {
	char exterior_dir[BSIZE_B] = {0};
	char file_src[BSIZE_B] = {0}, file_dst[BSIZE_B] = {0};
	CopyOperationIntent_t intent = CopyOperationIntentShare;
	copyfile_state_t cpf_state = NULL;
	acl_t src_acl = NULL, dst_acl = NULL;
	acl_entry_t acl_entry = NULL;
	acl_permset_t src_permset = NULL;
	int test_folder_id;
	int src_file_fd = -1;
	bool success = true;

	// Get read for the test.
	assert_with_errno((src_acl = acl_init(1)) != NULL);
	test_folder_id = rand() % DEFAULT_NAME_MOD;
	create_test_file_name(test_directory, "acl", test_folder_id, exterior_dir);
	assert_no_err(mkdir(exterior_dir, DEFAULT_MKDIR_PERM));

	// Create path names.
	assert_with_errno(snprintf(file_src, BSIZE_B, "%s/src", exterior_dir) > 0);
	assert_with_errno(snprintf(file_dst, BSIZE_B, "%s/dst", exterior_dir) > 0);

	// Create a source file, with an ACL.
	// (Do not free it until the end of the test,
	// as we need to persist the permission set.)
	src_file_fd = open(file_src, DEFAULT_OPEN_FLAGS, DEFAULT_OPEN_PERM);
	assert_with_errno(src_file_fd >= 0);
	assert_no_err(acl_create_entry(&src_acl, &acl_entry));
	assert_no_err(acl_get_permset(acl_entry, &src_permset));
	assert_no_err(acl_add_perm(src_permset, ACL_SEARCH));
	assert_no_err(acl_set_fd(src_file_fd, src_acl));

	// COPYFILE_CLONE the source file and make sure that the ACL is not preserved.
	assert_no_err(copyfile(file_src, file_dst, NULL, COPYFILE_CLONE));
	dst_acl = acl_get_file(file_dst, ACL_TYPE_EXTENDED);
	success = success && (!dst_acl);
	if (dst_acl) {
		acl_free(dst_acl);
		dst_acl = NULL;
	}

	assert_no_err(unlink(file_dst));

	// Repeat, but also pass COPYFILE_ACL and make sure cloning preserves the ACL.
	assert_no_err(copyfile(file_src, file_dst, NULL, COPYFILE_CLONE | COPYFILE_ACL));
	dst_acl = acl_get_file(file_dst, ACL_TYPE_EXTENDED);
	success = success && (dst_acl);
	if (dst_acl) {
		acl_permset_t dst_permset;

		// Make sure the ACL permission sets match.
		assert_no_err(acl_get_entry(dst_acl, ACL_FIRST_ENTRY, &acl_entry));
		assert_no_err(acl_get_permset(acl_entry, &dst_permset));
		assert_equal_int(acl_compare_permset_np(src_permset, dst_permset), 1);

		acl_free(dst_acl);
		dst_acl = NULL;
	}

	assert_no_err(unlink(file_dst));

	// Repeat the test but use COPYFILE_CLONE with a copy intent so that it fails,
	// and verify that we don't copy the ACL upon failed COPYFILE_CLONE.
	assert_with_errno((cpf_state = copyfile_state_alloc()) != NULL);
	assert_no_err(copyfile_state_set(cpf_state, COPYFILE_STATE_INTENT, &intent));
	assert_no_err(copyfile(file_src, file_dst, cpf_state, COPYFILE_CLONE));
	dst_acl = acl_get_file(file_dst, ACL_TYPE_EXTENDED);
	success = success && (!dst_acl);
	if (dst_acl) {
		acl_free(dst_acl);
		dst_acl = NULL;
	}
	assert_no_err(copyfile_state_free(cpf_state));
	cpf_state = NULL;

	assert_no_err(unlink(file_dst));

	// Finally, repeat the copy intent test, passing COPYFILE_ACL as well,
	// and verify that we *do* preserve the ACL.
	assert_with_errno((cpf_state = copyfile_state_alloc()) != NULL);
	assert_no_err(copyfile_state_set(cpf_state, COPYFILE_STATE_INTENT, &intent));
	assert_no_err(copyfile(file_src, file_dst, cpf_state, COPYFILE_CLONE | COPYFILE_ACL));
	dst_acl = acl_get_file(file_dst, ACL_TYPE_EXTENDED);
	success = success && dst_acl;
	if (dst_acl) {
		acl_permset_t dst_permset;

		// Make sure the ACL permission sets match.
		assert_no_err(acl_get_entry(dst_acl, ACL_FIRST_ENTRY, &acl_entry));
		assert_no_err(acl_get_permset(acl_entry, &dst_permset));
		assert_equal_int(acl_compare_permset_np(src_permset, dst_permset), 1);

		acl_free(dst_acl);
		dst_acl = NULL;
	}
	assert_no_err(copyfile_state_free(cpf_state));
	cpf_state = NULL;

	assert_no_err(acl_free(src_acl));
	src_acl = NULL;

	assert_no_err(removefile(exterior_dir, NULL, REMOVEFILE_RECURSIVE));

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
