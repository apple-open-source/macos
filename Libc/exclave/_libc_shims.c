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

#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include "_libc_init.h"

/*
 * Helper macros
 */

#define SHIM_INIT(ver, src, target, func) \
	do { \
		if ((src) != NULL && \
			(src)->version >= (ver) && \
			(src)->func != NULL) { \
			(target)->func = (src)->func; \
		} \
	} while (0)

#define SHIM_INIT_V1(func) SHIM_INIT(1, funcs, &_libc_funcs, func)

#define SHIM_NOT_SUPPORTED(ret) \
	do { \
		errno = ENOSYS; \
        return (ret); \
	} while (0)

#define SHIM_STUB \
	{ SHIM_NOT_SUPPORTED(-1); }

#define SHIM_RET_STUB \
	{ SHIM_NOT_SUPPORTED(NULL); }

#define SHIM_FORWARD(ret, impl, func, ...) \
	do { \
		if ((impl)->func != NULL) { \
			return ((impl)->func)(__VA_ARGS__); \
		} \
		SHIM_NOT_SUPPORTED(ret); \
	} while (0)

#define SHIM(func, ...) \
	{ SHIM_FORWARD(-1, &_libc_funcs, func, ##__VA_ARGS__); }

#define SHIM_RET(func, ...) \
	{ SHIM_FORWARD(NULL, &_libc_funcs, func, ##__VA_ARGS__); }

/*
 * Initializer
 */

static struct _libc_functions _libc_funcs;

__private_extern__
void _libc_shims_init(const struct _libc_functions *funcs)
{
	SHIM_INIT_V1(access);
	SHIM_INIT_V1(close);
	SHIM_INIT_V1(read);
	SHIM_INIT_V1(closedir);
	SHIM_INIT_V1(opendir);
	SHIM_INIT_V1(readdir);
	SHIM_INIT_V1(readdir_r);
	SHIM_INIT_V1(open);
	SHIM_INIT_V1(fcntl);
	SHIM_INIT_V1(fstat);
	SHIM_INIT_V1(lstat);
	SHIM_INIT_V1(stat);
}

/*
 * Storage shims
 */

/* unistd.h */
int	 access(const char *arg1, int arg2) SHIM(access, arg1, arg2);
int	 close(int arg) SHIM(close, arg);
ssize_t read(int arg1, void *arg2, size_t arg3) SHIM(read, arg1, arg2, arg3);

/* dirent.h */
int closedir(DIR *arg) SHIM(closedir, arg);
DIR *opendir(const char *arg) SHIM_RET(opendir, arg);
struct dirent *readdir(DIR *arg) SHIM_RET(readdir, arg);
int readdir_r(DIR *arg1, struct dirent *arg2, struct dirent **arg3) SHIM(readdir_r, arg1, arg2, arg3);

/* fnctl.h */
int open(const char *arg1, int arg2, ...)
{
	mode_t arg3 = 0;
	if (arg2 & O_CREAT) {
		va_list ap;
		va_start(ap, arg2);
		arg3 = va_arg(ap, int);
		va_end(ap);
	}

	SHIM(open, arg1, arg2, arg3);
}

int fcntl(int arg1, int arg2, ...)
{
	void *arg3 = NULL;
	va_list ap;
	va_start(ap, arg2);
	arg3 = va_arg(ap, void *);
	va_end(ap);

	SHIM(fcntl, arg1, arg2, arg3);
}

/* sys/stat.h */
int     fstat(int arg1, struct stat *arg2) SHIM(fstat, arg1, arg2);
int     lstat(const char *arg1, struct stat *arg2) SHIM(lstat, arg1, arg2);
int     stat(const char *arg1, struct stat *arg2) SHIM(stat, arg1, arg2);

/*
 * No-op shims
 */

/* unistd.h */
char *getcwd(char *arg1, size_t arg2) SHIM_RET_STUB;
int	 chdir(const char *arg) SHIM_STUB;
int	 chown(const char *arg1, uid_t arg2, gid_t arg3) SHIM_STUB;
int	 rmdir(const char *arg) SHIM_STUB;
int	 unlink(const char *arg) SHIM_STUB;
ssize_t	 write(int __fd, const void * __buf, size_t __nbyte) SHIM_STUB;
int	 lchown(const char *arg1, uid_t arg2, gid_t arg3) SHIM_STUB;
int	 fchown(int arg1, uid_t arg2, gid_t arg3) SHIM_STUB;
ssize_t  readlink(const char * __restrict arg1, char * __restrict arg2, size_t arg3) SHIM_STUB;
int	 symlink(const char *arg1, const char *arg2) SHIM_STUB;
char	*mktemp(char *arg) SHIM_RET_STUB;
int	 mkstemp(char *arg) SHIM_STUB;
int	 fsync(int arg) SHIM_STUB;

/* sys/stat.h */
int     chmod(const char *arg1, mode_t arg2) SHIM_STUB;
int     fchmod(int arg1, mode_t arg2) SHIM_STUB;
int     mkdir(const char *arg1, mode_t arg2) SHIM_STUB;
int     mkfifo(const char *arg1, mode_t arg2) SHIM_STUB;
int     mknod(const char *arg1, mode_t arg2, dev_t arg3) SHIM_STUB;
