/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <ufs/ufs/dir.h>
#include <libkern/OSByteOrder.h>
#include "ufs_byteorder.h"
#include "libsaio.h"

#define	swapBigLongToHost(thing) ((thing) = OSSwapBigToHostInt32(thing))
#define	swapBigShortToHost(thing) ((thing) = OSSwapBigToHostInt16(thing))
#define	byte_swap_longlong(thing) ((thing) = OSSwapBigToHostInt64(thing))
#define	byte_swap_int(thing) ((thing) = OSSwapBigToHostInt32(thing))
#define	byte_swap_short(thing) ((thing) = OSSwapBigToHostInt16(thing))

#if UNUSED
void
byte_swap_longlongs(unsigned long long *array, int count)
{
	register unsigned long long	i;

	for (i = 0;  i < (unsigned long long)count;  i++)
		byte_swap_longlong(array[i]);
}
#endif

void
byte_swap_ints(int *array, int count)
{
	register int	i;

	for (i = 0;  i < count;  i++)
		byte_swap_int(array[i]);
}

void
byte_swap_shorts(short *array, int count)
{
	register int	i;

	for (i = 0;  i < count;  i++)
		byte_swap_short(array[i]);
}

#if UNUSED
static void
swapBigIntsToHost(int *array, int count)
{
	register int	i;

	for (i = 0;  i < count;  i++)
		swapBigLongToHost(array[i]);
}

static void
swapBigShortToHosts(short *array, int count)
{
	register int	i;

	for (i = 0;  i < count;  i++)
		swapBigShortToHost(array[i]);
}
#endif

void
byte_swap_superblock(struct fs *sb)
{
	u_int16_t *   usptr;
	unsigned long size;

	byte_swap_ints(((int32_t *)&sb->fs_firstfield), 52);
	byte_swap_int(sb->fs_cgrotor);
	byte_swap_int(sb->fs_cpc);
	byte_swap_shorts((int16_t *)sb->fs_opostbl, 16 * 8); 
	byte_swap_ints((int32_t *)sb->fs_sparecon, 50);
	byte_swap_ints((int32_t *)&sb->fs_contigsumsize, 3);
#if UNUSED
	byte_swap_longlongs((u_int64_t *)&sb->fs_maxfilesize,3);
#endif
	byte_swap_ints((int32_t *)&sb->fs_state, 6);

	/* Got these magic numbers from mkfs.c in newfs */
	if (sb->fs_nrpos != 8 || sb->fs_cpc > 16) {
		usptr = (u_int16_t *)((u_int8_t *)(sb) + (sb)->fs_postbloff);
		size = sb->fs_cpc * sb->fs_nrpos;
		byte_swap_shorts(usptr,size);	/* fs_postbloff */
	}
}


/* This value should correspond to the value set in the ffs_mounts */

#define RESYMLNKLEN 60

void
byte_swap_dinode_in(struct dinode *di)
{
	int i;

	di->di_mode = OSSwapInt16(di->di_mode);
	di->di_nlink = OSSwapInt16(di->di_nlink);
#ifdef LFS
	di->di_u.inumber = OSSwapInt32(di->di_u.inumber);
#else
	di->di_u.oldids[0] = OSSwapInt16(di->di_u.oldids[0]);
	di->di_u.oldids[1] = OSSwapInt16(di->di_u.oldids[1]);
#endif
	di->di_size = OSSwapInt64(di->di_size);
	di->di_atime = OSSwapInt32(di->di_atime);
	di->di_atimensec = OSSwapInt32(di->di_atimensec);
	di->di_mtime = OSSwapInt32(di->di_mtime);
	di->di_mtimensec = OSSwapInt32(di->di_mtimensec);
	di->di_ctime = OSSwapInt32(di->di_ctime);
	di->di_ctimensec = OSSwapInt32(di->di_ctimensec);
	if (((di->di_mode & IFMT) != IFLNK ) || (di->di_size > RESYMLNKLEN)) {
		for (i=0; i < NDADDR; i++)	/* direct blocks */
			di->di_db[i] = OSSwapInt32(di->di_db[i]);
		for (i=0; i < NIADDR; i++)	/* indirect blocks */
			di->di_ib[i] = OSSwapInt32(di->di_ib[i]);
	}
	di->di_flags = OSSwapInt32(di->di_flags);
	di->di_blocks = OSSwapInt32(di->di_blocks);
	di->di_gen = OSSwapInt32(di->di_gen);
	di->di_uid = OSSwapInt32(di->di_uid);
	di->di_gid = OSSwapInt32(di->di_gid);
	di->di_spare[0] = OSSwapInt32(di->di_spare[0]);
	di->di_spare[1] = OSSwapInt32(di->di_spare[1]);
}

void
byte_swap_dir_block_in(char *addr, int count)
{
	register struct direct * ep = (struct direct *) addr;
	register int		     entryoffsetinblk = 0;

	while (entryoffsetinblk < count) {
		ep = (struct direct *) (entryoffsetinblk + addr);
		swapBigLongToHost(ep->d_ino);
		swapBigShortToHost(ep->d_reclen);
		entryoffsetinblk += ep->d_reclen;
		if (ep->d_reclen < 12)		/* handle garbage in dirs */
			break;
	}
}
