/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#ifndef __LIBC_INIT_H__
#define __LIBC_INIT_H__

#include <sys/cdefs.h>
#include <sys/stat.h>
#include <dirent.h>

__BEGIN_DECLS

#if defined(ENABLE_EXCLAVE_STORAGE)
struct _libc_functions {
	unsigned long version;

	// version 1
	int (*access)(const char *path, int mode);
	int (*close)(int fildes);
	ssize_t (*read)(int fildes, void *buf, size_t nbyte);
	int (*closedir)(DIR *dirp);
	DIR *(*opendir)(const char *filename);
	struct dirent *(*readdir)(DIR *dirp);
	int (*readdir_r)(DIR *dirp, struct dirent *entry, struct dirent **result);
	int (*open)(const char *path, int oflag, int mode);
	int (*fcntl)(int fildes, int cmd, void *buffer);
	int (*fstat)(int fildes, struct stat *buf);
	int (*lstat)(const char *restrict path, struct stat *restrict buf);
	int (*stat)(const char *restrict path, struct stat *restrict buf);

	// version 2
};
#else
struct _libc_functions;
#endif /* ENABLE_EXCLAVE_STORAGE */

struct ProgramVars; // forward reference

void
_libc_initializer(const struct _libc_functions *funcs,
    const char *envp[],
    const char *apple[],
    const struct ProgramVars *vars);

__END_DECLS

#endif /* __LIBC_INIT_H__ */
