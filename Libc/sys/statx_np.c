/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>
#include <stdio.h>

#define ACL_MIN_SIZE_HEURISTIC (sizeof(struct kauth_filesec) + 16 * sizeof(struct kauth_ace))

static int	statx_syscall(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size);
static int	fstatx_syscall(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size);
static int	lstatx_syscall(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size);

static int	statx1(void *obj,
		       int (* stat_syscall)(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size),
		       struct stat *sb, filesec_t fsec);

/*
 * Stat interfaces.
 */
int
statx_np(const char *path, struct stat *sb, filesec_t fsec)
{
	if (fsec == NULL)
		return(stat(path, sb));
	return(statx1((void *)&path, statx_syscall, sb, fsec));
}

int
fstatx_np(int fd, struct stat *sb, filesec_t fsec)
{
	if (fsec == NULL)
		return(fstat(fd, sb));
	return(statx1((void *)&fd, fstatx_syscall, sb, fsec));
}

int
lstatx_np(const char *path, struct stat *sb, filesec_t fsec)
{
	if (fsec == NULL)
		return(lstat(path, sb));
	return(statx1((void *)&path, lstatx_syscall, sb, fsec));
}

/*
 * Stat syscalls
 */
static int
statx_syscall(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size)
{
	const char *path = *(const char **)obj;

	return(syscall(SYS_stat_extended, path, sb, fsacl, fsacl_size));
}

static int
fstatx_syscall(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size)
{
	int fd = *(int *)obj;
	return(syscall(SYS_fstat_extended, fd, sb, fsacl, fsacl_size));
}

static int
lstatx_syscall(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size)
{
	const char *path = *(const char **)obj;
	return(syscall(SYS_lstat_extended, path, sb, fsacl, fsacl_size));
}

/*
 * Stat internals
 */
static int
statx1(void *obj,
    int (* stat_syscall)(void *obj, struct stat *sb, void *fsacl, size_t *fsacl_size),
    struct stat *sb, filesec_t fsec)
{
	struct kauth_filesec *fsacl, *ofsacl;
	size_t fsacl_size, buffer_size;
	int error;

	fsacl = NULL;
	error = 0;
	
	/*
	 * Allocate an initial buffer.
	 */
	if ((fsacl = malloc(ACL_MIN_SIZE_HEURISTIC)) == NULL) {
		error = ENOMEM;
		goto out;
	}		
	buffer_size = ACL_MIN_SIZE_HEURISTIC;

	/*
	 * Loop until we have the ACL.
	 */
	for (;;) {
		fsacl_size = buffer_size;
		if ((error = stat_syscall(obj, sb, fsacl, &fsacl_size)) != 0)
			goto out;

		/*
		 * No error, did we get the ACL?
		 */
		if (fsacl_size <= buffer_size)
			break;

		/* no, use supplied buffer size plus some padding */
		ofsacl = fsacl;
		fsacl = realloc(fsacl, fsacl_size + sizeof(struct kauth_ace) * 2);
		if (fsacl == NULL) {
			fsacl = ofsacl;
			errno = ENOMEM;
			goto out;
		}
		buffer_size = fsacl_size;
	}
	
	/* populate filesec with values from stat */
	filesec_set_property(fsec, FILESEC_OWNER, &(sb->st_uid));
	filesec_set_property(fsec, FILESEC_GROUP, &(sb->st_gid));
	filesec_set_property(fsec, FILESEC_MODE, &(sb->st_mode));

	/* if we got a kauth_filesec, take values from there too */
	if (fsacl_size >= sizeof(struct kauth_filesec)) {
		filesec_set_property(fsec, FILESEC_UUID, &fsacl->fsec_owner);
		filesec_set_property(fsec, FILESEC_GRPUUID, &fsacl->fsec_group);
		
		/* check to see whether there's actually an ACL here */
		if (fsacl->fsec_acl.acl_entrycount != KAUTH_FILESEC_NOACL) {
		    filesec_set_property(fsec, FILESEC_ACL_ALLOCSIZE, &fsacl_size);
		    filesec_set_property(fsec, FILESEC_ACL_RAW, &fsacl);
		    fsacl = NULL;	/* avoid freeing it below */
		} else {
		    filesec_set_property(fsec, FILESEC_ACL_ALLOCSIZE, NULL);
		    filesec_set_property(fsec, FILESEC_ACL_RAW, NULL);
		}
	} else {
		filesec_set_property(fsec, FILESEC_UUID, NULL);
		filesec_set_property(fsec, FILESEC_GRPUUID, NULL);
		filesec_set_property(fsec, FILESEC_ACL_ALLOCSIZE, NULL);
		filesec_set_property(fsec, FILESEC_ACL_RAW, NULL);
	}
out:
	if (fsacl != NULL)
		free(fsacl);
	return(error);
}
