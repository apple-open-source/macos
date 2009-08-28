/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/* 
 * Copyright (C) 1991 by NeXT Computer, Inc.
 * All Rights Reserved.
 */

/*
 * Swapping File System mount command
 *
 * HISTORY
 * 24-Jul-91  Bradley Taylor (btaylor) at NeXT, Inc.
 *	Created. 
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <errno.h>

typedef struct swapfs_mountopts {
	int real_fd;
} swapfs_mountopts_t;


static void
usage(char *cmdname)
{
	fprintf(stderr, "usage: %s filename frontname swapfs opts\n", 
		cmdname);
	exit(EINVAL);
}

/*
 * Given: dev dir type opts
 */
void
main(int argc, char **argv)
{
	swapfs_mountopts_t opts;
	char *cmdname;
	int fd;
	char *storage_fname;
	char *mount_fname;

	cmdname = argv[0];

	if (argc != 5) {
		usage(cmdname);
	}
	storage_fname = argv[1];
	mount_fname = argv[2];

	fd = open(storage_fname, O_RDWR|O_NO_MFS, 0);
	if (fd < 0) {
		fd = open(storage_fname, O_RDWR|O_CREAT|O_NO_MFS, 01600);
	}
	
	if (fd < 0) {
		perror(storage_fname);
		exit(errno);
	}
	opts.real_fd = fd;
	if (mount("swapfs", mount_fname, M_NEWTYPE, &opts) < 0) {
		perror("mount");
		exit(errno);
	}
	exit(0);
}


