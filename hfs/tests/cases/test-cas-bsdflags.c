#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/mount.h>
#include <sys/param.h>

#include <System/sys/fsctl.h>

#include "hfs-tests.h"
#include "test-utils.h"
#include "disk-image.h"
#include "systemx.h"

#define AFSCUTIL       "/usr/local/bin/afscutil"

TEST(cas_bsdflags)

static bool
cas_bsd_flags(int fd, uint32_t expected_flags, uint32_t new_flags, int expected_error)
{
	struct fsioc_cas_bsdflags cas;

	cas.expected_flags = expected_flags;
	cas.new_flags      = new_flags;
	cas.actual_flags   = ~0;		/* poison */

	if (expected_error != 0) {
		// no assert_call_fail() in test_hfs
		assert(ffsctl(fd, FSIOC_CAS_BSDFLAGS, &cas, 0) == -1);
		assert(errno == EPERM);
		return true; // as expected - flags were not changed
	} else {
		assert_no_err(ffsctl(fd, FSIOC_CAS_BSDFLAGS, &cas, 0));
	}

	return (cas.expected_flags == cas.actual_flags);
}

static void
write_compressible_data(int fd)
{
	// adapted from test_clonefile in apfs
	char dbuf[4096];

	// write some easily compressable data
	memset(dbuf + 0*(sizeof(dbuf)/4), 'A', sizeof(dbuf)/4);
	memset(dbuf + 1*(sizeof(dbuf)/4), 'B', sizeof(dbuf)/4);
	memset(dbuf + 2*(sizeof(dbuf)/4), 'C', sizeof(dbuf)/4);
	memset(dbuf + 3*(sizeof(dbuf)/4), 'D', sizeof(dbuf)/4);
	for (int idx = 0; idx < 32; idx++) {
		check_io(write(fd, dbuf, sizeof(dbuf)), sizeof(dbuf));
	}
}

int run_cas_bsdflags(__unused test_ctx_t *ctx)
{
	disk_image_t *di = disk_image_get();
	struct stat sb;
	int fd;

	char *file;
	asprintf(&file, "%s/cas_bsdflags.data", di->mount_point);

	assert_with_errno((fd = open(file,
								 O_CREAT | O_RDWR | O_TRUNC, 0666)) >= 0);

	assert_no_err(fchflags(fd, UF_HIDDEN));
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_HIDDEN);

	assert(cas_bsd_flags(fd, 0, UF_NODUMP, 0) == false);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_HIDDEN);

	assert(cas_bsd_flags(fd, UF_HIDDEN, UF_NODUMP, 0) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_NODUMP);

	assert(cas_bsd_flags(fd, UF_NODUMP, 0, 0) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, 0);

	// Add some data to our (non-compressed) file,
	// mark it with UF_COMPRESSED,
	// and check that UF_COMPRESSED is *not* set -
	// as there is no decmpfs xattr present.
	check_io(write(fd, "J", 1), 1);
	assert_no_err(fstat(fd, &sb));
	assert(sb.st_size > 0);

	assert(cas_bsd_flags(fd, 0, UF_COMPRESSED, EPERM) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, 0);

	// Now, add some compressible data to the file and compress it using afscutil.
	write_compressible_data(fd);
	assert(!systemx(AFSCUTIL, "-c", file, NULL));
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_COMPRESSED);

	// Now, remove UF_COMPRESSED from our file and
	// check that the file is 0-length.
	assert(cas_bsd_flags(fd, UF_COMPRESSED, 0, 0) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_ll(sb.st_size, 0);

	close(fd);
	assert_no_err(unlink(file));
	free(file);

	return 0;
}

