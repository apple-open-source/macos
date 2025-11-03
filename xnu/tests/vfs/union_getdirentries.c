/*
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/* compile: xcrun -sdk macosx.internal clang -arch arm64e -arch x86_64 -ldarwintest -o union_getdirentries union_getdirentries.c */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <darwintest.h>
#include <darwintest/utils.h>

#define RUN_TEST     TARGET_OS_OSX

#define FSTYPE_DEVFS "devfs"
#define FSTYPE_TMPFS "tmpfs"

#define TESTDIR   "testdir"
#define DIRECTORY "dir"
#define FILE      "dir/file"

static char template[MAXPATHLEN];
static char *testdir = NULL;
static int testdir_fd = -1;

extern ssize_t __getdirentries64(int, void *, size_t, off_t *);

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(true),
	T_META_ENABLED(RUN_TEST),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
	unmount(DIRECTORY, MNT_FORCE);

	if (testdir_fd != -1) {
		unlinkat(testdir_fd, FILE, 0);
		unlinkat(testdir_fd, DIRECTORY, AT_REMOVEDIR);
		unlinkat(testdir_fd, TESTDIR, AT_REMOVEDIR);
		close(testdir_fd);
		rmdir(testdir);
	}
}

T_DECL(union_getdirentries_open_lifecycle,
    "Ensure getdirentries64 maintains proper open/close lifecycle management with multiple union mounts")
{
	int fd;
	struct dirent *buf;
	off_t offset = 0;
	ssize_t result;
	char zero_path[MAXPATHLEN];
	char dir_path[MAXPATHLEN];
	char mount_tmpfs_cmd[1000];

#if (!RUN_TEST)
	T_SKIP("Not macOS");
#endif

	if (geteuid() != 0) {
		T_SKIP("Test should run as root");
	}

	T_ATEND(cleanup);

	T_SETUPBEGIN;

	T_ASSERT_POSIX_NOTNULL((buf = malloc(4096)), "Allocating data buffer");

	snprintf(template, sizeof(template), "%s/union_getdirentries_open_lifecycle-XXXXXX", dt_tmpdir());
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root dir");
	T_ASSERT_POSIX_SUCCESS((testdir_fd = open(testdir, O_SEARCH, 0777)), "Opening test root directory %s", testdir);

	/* Create base directory structure */
	T_ASSERT_POSIX_SUCCESS(mkdirat(testdir_fd, DIRECTORY, 0777), "Creating %s/%s", testdir, DIRECTORY);

	/* Create directories path */
	snprintf(dir_path, sizeof(dir_path), "%s/%s", testdir, DIRECTORY);
	snprintf(zero_path, sizeof(zero_path), "%s/%s/zero", testdir, DIRECTORY);

	/* Create multi-layer union mount structure */
	/* Layer 1: Mount devfs with union flag on the base directory */
	T_ASSERT_POSIX_SUCCESS(mount(FSTYPE_DEVFS, dir_path, MNT_UNION, NULL), "Mounting devfs layer 1 at %s", dir_path);

	/* Layer 2: Mount tmpfs on devfs directory using mount_tmpfs command with union flag */
	snprintf(mount_tmpfs_cmd, sizeof(mount_tmpfs_cmd), "/sbin/mount_tmpfs -o union -s 10m %s", dir_path);
	T_ASSERT_POSIX_SUCCESS(system(mount_tmpfs_cmd), "Mounting tmpfs with union flag at %s", dir_path);

	/* Create the zero directory in the tmpfs mount */
	T_ASSERT_POSIX_SUCCESS(mkdir(zero_path, 0777), "Creating %s", zero_path);

	T_SETUPEND;

	/* Open the multi-layer union mount entry - test proper lifecycle management */
	T_EXPECT_POSIX_SUCCESS((fd = open(zero_path, O_RDONLY)), "Opening multi-layer union mount entry");
	if (fd >= 0) {
		/*
		 * This tests that proper VNOP_OPEN/VNOP_CLOSE lifecycle
		 * is maintained when switching vnodes through multiple union mount layers
		 * (devfs + tmpfs), preventing filesystem state corruption.
		 */
		while (1) {
			result = __getdirentries64(fd, buf, 4096, &offset);
			if (result <= 0) {
				break;
			}
		}

		close(fd);
		T_PASS("getdirentries64 completed with proper open/close lifecycle management through multiple filesystem layers");
	}

	/* Clean up mounts in reverse order */
	unmount(dir_path, MNT_FORCE);  /* Remove union mount */
	unmount(dir_path, MNT_FORCE); /* Remove union mount */

	/* Clean up directories */
	rmdir(zero_path);
	rmdir(dir_path);
	rmdir(testdir);

	free(buf);
}
