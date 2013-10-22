/*
 * ntfs.h - Some generic defines for the NTFS kernel driver.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#ifndef _OSX_NTFS_H
#define _OSX_NTFS_H

#ifdef KERNEL

#include <sys/mount.h>

#include <libkern/OSMalloc.h>

#include <kern/locks.h>

/* The email address of the NTFS developers. */
__private_extern__ const char ntfs_dev_email[];
__private_extern__ const char ntfs_please_email[];

/*
 * Lock group and lock attribute for de-/initialization of locks (defined
 * in ntfs_vfsops.c).
 */
__private_extern__ lck_grp_t *ntfs_lock_grp;
__private_extern__ lck_attr_t *ntfs_lock_attr;

/*
 * A tag for allocation and freeing of memory (defined in ntfs_vfsops.c).
 */
__private_extern__ OSMallocTag ntfs_malloc_tag;

#include "ntfs_volume.h"

/**
 * NTFS_MP - return the NTFS volume given a vfs mount
 * @mp:		VFS mount
 *
 * NTFS_MP() returns the NTFS volume associated with the VFS mount @mp.
 */
static inline ntfs_volume *NTFS_MP(mount_t mp)
{
	return (ntfs_volume*)vfs_fsprivate(mp);
}

__private_extern__ void ntfs_do_postponed_release(ntfs_volume *vol);

#endif /* KERNEL */

#include "ntfs_endian.h"
#include "ntfs_types.h"

/* Some useful constants to do with NTFS. */
enum {
	NTFS_BLOCK_SIZE		= 512,
	NTFS_BLOCK_SIZE_SHIFT	= 9,
	NTFS_MAX_NAME_LEN	= 255,
	NTFS_MAX_ATTR_NAME_LEN	= 255,
	NTFS_MAX_SECTOR_SIZE	= 4096,		/* 4kiB */
	NTFS_MAX_CLUSTER_SIZE	= 64 * 1024,	/* 64kiB */
	NTFS_ALLOC_BLOCK	= 1024,
	NTFS_MAX_HARD_LINKS	= 65535,	/* 2^16 - 1 */
	NTFS_MAX_ATTR_LIST_SIZE	= 256 * 1024,	/* 256kiB, corresponding to the
						   VACB_MAPPING_GRANULARITY on
						   Windows. */
	NTFS_COMPRESSION_UNIT	= 4,
};

/*
 * The maximum attribute size on NTFS is 2^63 - 1 bytes as it is stored in a
 * signed 64 bit type (s64).
 */
#define NTFS_MAX_ATTRIBUTE_SIZE 0x7fffffffffffffffULL

/*
 * The maximum number of MFT records allowed on NTFS is 2^32 as described in
 * various documentation to be found on the Microsoft web site.  This is an
 * imposed limit rather than an inherent NTFS format limit.
 */
#define NTFS_MAX_NR_MFT_RECORDS 0x100000000ULL

// TODO: Constants so ntfs_vfsops.c compiles for now...
enum {
	/* One of these must be present, default is ON_ERRORS_CONTINUE|ON_ERRORS_FAIL_DIRTY. */
	ON_ERRORS_PANIC		= 0x01,
	ON_ERRORS_REMOUNT_RO	= 0x02,
	ON_ERRORS_CONTINUE	= 0x04,
	/* Optional, can be combined with any of the above. */
	ON_ERRORS_RECOVER	= 0x10,
	/* If the volume is dirty, and we attempted to mount read/write, */
	/* return an error rather than force a read-only mount. */
	ON_ERRORS_FAIL_DIRTY    = 0x20,
};

/*
 * The NTFS mount options header passed in from user space.
 */
typedef struct {
#ifndef KERNEL
	char *fspec;	/* Path of device to mount, consumed by mount(2). */
#endif /* !KERNEL */
	u8 major_ver;	/* The major version of the mount options structure. */
	u8 minor_ver;	/* The minor version of the mount options structure. */
} __attribute__((__packed__)) ntfs_mount_options_header;

/*
 * The NTFS mount options passed in from user space.  This follows the
 * ntfs_mount_options_header aligned to an eight byte boundary.
 *
 * This is major version 0, minor version 0, which does not have any options,
 * i.e. is empty.
 */
typedef struct {
	/* Mount options version 0.0 does not have any ntfs options. */
} __attribute__((__packed__)) ntfs_mount_options_0_0;

/*
 * The currently defined flags for the ntfs mount options structure.
 */
enum {
	/* Below flag(s) appeared in mount options version 1.0. */
	NTFS_MNT_OPT_CASE_SENSITIVE = const_cpu_to_le32(0x00000001),
	/* Below flag(s) appeared in mount options version x.y. */
	// TODO: Add NTFS specific mount options flags here.
};

typedef le32 NTFS_MNT_OPTS;

/*
 * The NTFS mount options passed in from user space.  This follows the
 * ntfs_mount_options_header aligned to an eight byte boundary.
 *
 * This is major version 1, minor version 0, which has only one option, a
 * little endian, 32-bit flags option.
 */
typedef struct {
	NTFS_MNT_OPTS flags;
	// TODO: Add NTFS specific mount options here.
} __attribute__((__packed__)) ntfs_mount_options_1_0;

#endif /* !_OSX_NTFS_H */
