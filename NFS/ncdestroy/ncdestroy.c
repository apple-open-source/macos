/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#define _DARWIN_FEATURE_64_BIT_INODE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <sys/attr.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <System/sys/fsctl.h>
#include <nfs/nfs_ioctl.h>

int
nfs_cred_destroy(const char *path, uint32_t flags)
{
	int error;

	if (path == NULL)
		return (EINVAL);

	error = fsctl(path, NFS_IOC_DESTROY_CRED, NULL, flags);
	return (error);
}


int
main(int argc, char *argv[])
{
	int count, i, opt;
	int return_status = 0;
	unsigned int flags = 0;
	struct statfs *mounts;
	struct statfs fsbuf;
	char path_storage[PATH_MAX];
	char *path;
	int verbose = 0;
	int error;

	while ((opt = getopt(argc, argv, "Pv")) != -1) {
		switch (opt) {
		case 'P':
			flags = FSOPT_NOFOLLOW;
			break;
		case 'v':
			verbose = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	for (i = 0; i < argc; i++) {
		path = (flags & FSOPT_NOFOLLOW) ? argv[i] : realpath(argv[i], path_storage);

		error = statfs(argv[i], &fsbuf);
		if (error && errno == ENOENT) {
			warn("%s", argv[i]);
			return_status = 1;
			continue;
		}
		if (verbose) {
			printf("Flushing credentials associated with %s ", path);
			if (!error)
				printf("Mounted on %s", fsbuf.f_mntonname);
			printf("\n");
		}
		error = nfs_cred_destroy(argv[i], flags);
		if (verbose && error )
			warn("fsctl for %s", path);
	}

	if (i == 0) {
		count = getmntinfo(&mounts, MNT_NOWAIT);
		if (count == 0)
			err(1, "getmntinfo failed");

		for (i = 0; i < count; i++) {
			if (strcmp(mounts[i].f_fstypename, "nfs") == 0) {
				if (verbose)
					printf("Flushing credentials from %s\n", mounts[i].f_mntonname);
				error = nfs_cred_destroy(mounts[i].f_mntonname, 0);
				if (error && verbose)
					warn("fsctl %s", mounts[i].f_mntonname);
			}
		}
	}

	return (return_status);
}
