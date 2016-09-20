//
//  external-jnl.c
//  hfs
//
//  Created by Chris Suter on 8/11/15.
//
//

#include <TargetConditionals.h>

#if !TARGET_OS_EMBEDDED

#include <stdio.h>
#include <sys/mount.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "hfs-tests.h"
#include "disk-image.h"
#include "systemx.h"
#include "../core/hfs_format.h"
#include "test-utils.h"

#define DISK_IMAGE_1		"/tmp/external-jnl1.sparseimage"
#define DISK_IMAGE_2		"/tmp/external-jnl2.sparseimage"

TEST(external_jnl)

int run_external_jnl(__unused test_ctx_t *ctx)
{
	unlink(DISK_IMAGE_1);
	unlink(DISK_IMAGE_2);

	disk_image_t *di1 = disk_image_create(DISK_IMAGE_1,
										  &(disk_image_opts_t){
											  .size = 64 * 1024 * 1024
										  });
	disk_image_t *di2
		= disk_image_create(DISK_IMAGE_2,
							&(disk_image_opts_t){
								.partition_type = EXTJNL_CONTENT_TYPE_UUID,
								.size = 8 * 1024 * 1024
							});

	unmount(di1->mount_point, 0);

	assert(!systemx("/sbin/newfs_hfs", SYSTEMX_QUIET, "-J", "-D", di2->disk, di1->disk, NULL));

	assert(!systemx("/usr/sbin/diskutil", SYSTEMX_QUIET, "mount", di1->disk, NULL));

	free((char *)di1->mount_point);
	di1->mount_point = NULL;

	struct statfs *mntbuf;
	int i, n = getmntinfo(&mntbuf, 0);
	for (i = 0; i < n; ++i) {
		if (!strcmp(mntbuf[i].f_mntfromname, di1->disk)) {
			di1->mount_point = strdup(mntbuf[i].f_mntonname);
			break;
		}
	}

	assert(i < n);

	char *path;
	asprintf(&path, "%s/test", di1->mount_point);
	int fd = open(path, O_RDWR | O_CREAT, 0666);
	assert_with_errno(fd >= 0);
	assert_no_err(close(fd));
	assert_no_err(unlink(path));
	free(path);

	return 0;
}

#endif
