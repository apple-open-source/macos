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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o volfs_chroot volfs_chroot.c -g -Weverything */

#include <darwintest.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <TargetConditionals.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ENABLED(TARGET_OS_OSX),
	T_META_ASROOT(true),
	T_META_CHECK_LEAKS(false));

T_DECL(volfs_chroot,
    "Check for and fail if the volfs path is not under the chroot")
{
#if TARGET_OS_OSX
	int fd;
	char root_volfs[MAXPATHLEN];
	const char *root_path = "/", *private_path = "/private";
	struct stat root_stat, root_stat2, private_stat, fd_stat;

	T_SETUPBEGIN;

	T_ASSERT_POSIX_SUCCESS(stat(root_path, &root_stat),
	    "Setup: Calling stat() on %s",
	    root_path);

	T_ASSERT_POSIX_SUCCESS(snprintf(root_volfs, sizeof(root_volfs), "/.vol/%d/2", root_stat.st_dev),
	    "Setup: Creating root_volfs path");

	T_ASSERT_POSIX_SUCCESS(stat(root_volfs, &root_stat2),
	    "Setup: Calling stat() on %s",
	    root_volfs);

	T_ASSERT_POSIX_SUCCESS(stat(private_path, &private_stat),
	    "Setup: Calling stat() on %s",
	    private_path);

	T_ASSERT_POSIX_SUCCESS(chroot(private_path),
	    "Setup: Calling chroot() on %s",
	    private_path);

	T_SETUPEND;

	T_ASSERT_EQ(root_stat.st_ino, root_stat2.st_ino, "Verifing %s and %s are the same file", root_path, root_volfs);
	T_ASSERT_POSIX_SUCCESS((fd = open(root_path, 0)), "Opening the updated root path");
	T_ASSERT_POSIX_SUCCESS((fstat(fd, &fd_stat)), "Calling stat on the updated root path");
	T_ASSERT_EQ(fd_stat.st_ino, private_stat.st_ino, "Verifing %s was opened", private_path);
	T_ASSERT_POSIX_FAILURE(open(root_volfs, 0), ENOENT, "Verifing %s can not be opened because path is not under the chroot", root_volfs);
#else
	T_SKIP("Not macOS");
#endif
}
