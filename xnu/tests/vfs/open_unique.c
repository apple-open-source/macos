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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o open_unique open_unique.c -g -Weverything */

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <darwintest.h>
#include <darwintest/utils.h>

static char template[MAXPATHLEN];
static char testdir_path[MAXPATHLEN + 1];
static char *testdir = NULL;
static int testdir_fd = -1;

#ifndef O_UNIQUE
#define O_UNIQUE         0x00002000
#endif

#define FILE           "file.txt"
#define FILE2          "file2.txt"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_CHECK_LEAKS(false));

static void
cleanup(void)
{
	if (testdir_fd != -1) {
		unlinkat(testdir_fd, FILE, 0);
		unlinkat(testdir_fd, FILE2, 0);

		close(testdir_fd);
		if (rmdir(testdir)) {
			T_FAIL("Unable to remove the test directory (%s)", testdir);
		}
	}
}

T_DECL(open_unique,
    "Validate the functionality of the O_UNIQUE flag in the open/openat syscalls")
{
	int fd;
	char file_path[MAXPATHLEN];
	struct stat statbuf;

	T_SETUPBEGIN;
	T_ATEND(cleanup);

	/* Create test root directory */
	snprintf(template, sizeof(template), "%s/%s-XXXXXX", dt_tmpdir(), "open_unique");
	T_ASSERT_POSIX_NOTNULL((testdir = mkdtemp(template)), "Creating test root directory");
	T_ASSERT_POSIX_SUCCESS((testdir_fd = open(testdir, O_SEARCH, 0777)), "Opening test root directory %s", testdir);
	T_ASSERT_POSIX_SUCCESS(fcntl(testdir_fd, F_GETPATH, testdir_path), "Calling fcntl() to get the path");

	/* Create test file path */
	snprintf(file_path, sizeof(file_path), "%s/%s", testdir_path, FILE);

	T_SETUPEND;

	/* Create the test file */
	T_ASSERT_POSIX_SUCCESS((fd = openat(testdir_fd, FILE, O_CREAT | O_RDWR | O_UNIQUE, 0777)), "Creating %s using openat() with O_UNIQUE -> Should PASS", FILE);
	close(fd);

	/* Validate nlink count equals 1 */
	T_EXPECT_POSIX_SUCCESS((fstatat(testdir_fd, FILE, &statbuf, 0)), "Calling stat() for %s -> Should PASS", FILE);
	T_EXPECT_EQ(statbuf.st_nlink, 1, "Validate nlink equals 1");
	T_EXPECT_POSIX_SUCCESS((fd = openat(testdir_fd, FILE, O_RDONLY | O_UNIQUE, 0)), "Opening %s using O_UNIQUE -> Should PASS", FILE);
	close(fd);

	/* Increase nlink count */
	T_EXPECT_POSIX_SUCCESS(linkat(testdir_fd, FILE, testdir_fd, FILE2, 0), "Calling linkat() for %s, %s -> Should PASS", FILE, FILE2);

	/* Validate nlink count equals 2 */
	T_EXPECT_POSIX_SUCCESS((fstatat(testdir_fd, FILE, &statbuf, 0)), "Calling fstatat() for %s -> Should PASS", FILE);
	T_EXPECT_EQ(statbuf.st_nlink, 2, "Validate nlink equals 2");
	T_EXPECT_POSIX_SUCCESS((fd = openat(testdir_fd, FILE, O_RDONLY, 0)), "Opening %s -> Should PASS", FILE);
	close(fd);

	/* Validate ENOTCAPABLE */
	T_EXPECT_POSIX_FAILURE((fd = open(file_path, O_RDONLY | O_UNIQUE, 0)), ENOTCAPABLE, "Opening using open() with O_UNIQUE -> Should FAIL with ENOTCAPABLE");

	T_EXPECT_POSIX_FAILURE((fd = openat(testdir_fd, FILE, O_WRONLY | O_UNIQUE, 0)), ENOTCAPABLE, "Opening %s using openat() with O_UNIQUE -> Should FAIL with ENOTCAPABLE", FILE);

	T_EXPECT_POSIX_FAILURE((fd = openat(testdir_fd, FILE2, O_CREAT | O_RDWR | O_UNIQUE, 0)), ENOTCAPABLE, "Opening %s using openat() with O_UNIQUE -> Should FAIL with ENOTCAPABLE", FILE2);

	/* Reduce nlink count */
	T_EXPECT_POSIX_SUCCESS(unlinkat(testdir_fd, FILE2, 0), "Calling unlinkat() for %s -> Should PASS", FILE2);

	/* Validate nlink count equals 1 */
	T_EXPECT_POSIX_SUCCESS((fstatat(testdir_fd, FILE, &statbuf, 0)), "Calling fstatat() for %s -> Should PASS", FILE);
	T_EXPECT_EQ(statbuf.st_nlink, 1, "Validate nlink equals 1");
	T_EXPECT_POSIX_SUCCESS((fd = openat(testdir_fd, FILE, O_RDONLY | O_UNIQUE, 0)), "Opening %s -> Should PASS", FILE);
	close(fd);
}
