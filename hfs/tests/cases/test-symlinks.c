/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * <rdar://problem/65693863>  UNIX Conformance | hfs_vnop_symlink should not validate empty path.
 */
#include <unistd.h>
#include <sys/stat.h>


#include "hfs-tests.h"
#include "test-utils.h"
#include "disk-image.h"

#define SYMPLINK_TEST_DIR "symlink.testdir"
#define SYMLINK_EMPTYSTR "symlink.emptystr"
TEST(symlinks)

int run_symlinks(__unused test_ctx_t *ctx)
{
	disk_image_t *di;
	struct stat statb;
	char *parent_dir, *slink;
	char buf;

	di = disk_image_get();

	//
	// Create a parent directory to host our test.
	//
	asprintf(&parent_dir, "%s/"SYMPLINK_TEST_DIR, di->mount_point);
	assert(!mkdir(parent_dir, 0777) || errno == EEXIST);

	//
	// Now check to make sure we support creating a symlink with an empty
	// target required for UNIX Conformance.
	//
	asprintf(&slink, "%s/"SYMLINK_EMPTYSTR, parent_dir);
	assert_no_err(symlink("", slink));

	//
	// Test that symlink has "l" as the S_ISLNK flag using lstat
	//
	memset(&statb, 0, sizeof(statb));
	assert(!(lstat(slink, &statb) < 0 ));
	assert(S_ISLNK(statb.st_mode));

	//
	// Test that readlink returns zero.
	//
	assert(!readlink(slink, &buf, 1));

	//
	// Delete test symlink, test directory and release all resources.
	//
	unlink(slink);
	unlink(parent_dir);
	free(slink);
	free(parent_dir);
	return 0;
}
