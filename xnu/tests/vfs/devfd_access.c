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

/* compile: xcrun -sdk macosx.internal clang -ldarwintest -o devfd_access devfd_access.c -g -Weverything */
/* sign: codesign --force --sign - --timestamp=none --entitlements devfd_access.entitlements devfd_access */

#include <darwintest.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/param.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vfs"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("vfs"),
	T_META_ASROOT(false),
	T_META_CHECK_LEAKS(false));

static int
docheck(int fd, int perm)
{
	char path[MAXPATHLEN];

	path[0] = '\0';
	snprintf(path, sizeof(path), "/dev/fd/%d", fd);
	errno = 0;

	return access(path, perm);
}

/* The devfs_access test should not run as root */
T_DECL(devfd_access, "Calculate the allowed access based on the open-flags for fdesc vnodes")
{
	const char *path = "/dev/null";
	int fd_rdonly, fd_wronly, fd_evtonly, fd_evtonly_drw;

	if (geteuid() == 0) {
		T_SKIP("Test should NOT run as root");
	}

	T_SETUPBEGIN;

	T_ASSERT_POSIX_SUCCESS(fd_rdonly = open(path, O_RDONLY),
	    "Setup: Opening file with O_RDONLY permissions, fd_rdonly = %d",
	    fd_rdonly);

	T_ASSERT_POSIX_SUCCESS(fd_wronly = open(path, O_WRONLY),
	    "Setup: Opening file with O_WRONLY permissions, fd_wronly = %d",
	    fd_wronly);

	T_ASSERT_POSIX_SUCCESS(fd_evtonly = open(path, O_EVTONLY),
	    "Setup: Opening file with O_EVTONLY permissions, fd_evtonly = %d",
	    fd_evtonly);

	T_ASSERT_POSIX_SUCCESS(setiopolicy_np(IOPOL_TYPE_VFS_DISALLOW_RW_FOR_O_EVTONLY,
	    IOPOL_SCOPE_PROCESS,
	    IOPOL_VFS_DISALLOW_RW_FOR_O_EVTONLY_ON),
	    "Setup: Disallowing RW for O_EVTONLY");

	T_ASSERT_POSIX_SUCCESS(fd_evtonly_drw = open(path, O_EVTONLY),
	    "Setup: Opening file with O_EVTONLY permissions while RW is disabled, fd_evtonly_drw = %d",
	    fd_evtonly_drw);

	T_SETUPEND;

	T_LOG("Test rdonly-fd's access");
	T_EXPECT_POSIX_SUCCESS(docheck(fd_rdonly, R_OK), "Testing R_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_rdonly, W_OK), EACCES, "Testing W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_rdonly, R_OK | W_OK), EACCES, "Testing R_OK | W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_rdonly, X_OK), EACCES, "Testing X_OK permissions");

	T_LOG("Test wronly-fd's access");
	T_EXPECT_POSIX_FAILURE(docheck(fd_wronly, R_OK), EACCES, "Testing R_OK permissions");
	T_EXPECT_POSIX_SUCCESS(docheck(fd_wronly, W_OK), "Testing W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_wronly, R_OK | W_OK), EACCES, "Testing R_OK | W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_wronly, X_OK), EACCES, "Testing X_OK permissions");

	T_LOG("Test evtonly-fd's access");
	T_EXPECT_POSIX_SUCCESS(docheck(fd_evtonly, R_OK), "Testing R_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_evtonly, W_OK), EACCES, "Testing W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_evtonly, R_OK | W_OK), EACCES, "Testing R_OK | W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_evtonly, X_OK), EACCES, "Testing X_OK permissions");

	T_LOG("Test evtonly-drw-fd's access");
	T_EXPECT_POSIX_FAILURE(docheck(fd_evtonly_drw, R_OK), EACCES, "Testing R_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_evtonly_drw, W_OK), EACCES, "Testing W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_evtonly_drw, R_OK | W_OK), EACCES, "Testing R_OK | W_OK permissions");
	T_EXPECT_POSIX_FAILURE(docheck(fd_evtonly_drw, X_OK), EACCES, "Testing X_OK permissions");

	/* Close open file descriptors */
	close(fd_rdonly);
	close(fd_wronly);
	close(fd_evtonly);
	close(fd_evtonly_drw);
}
