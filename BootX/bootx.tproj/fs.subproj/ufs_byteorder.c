/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */
/*
 *  ufs_byteorder.c - Functions for endian swapping UFS disk structures.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <architecture/byte_order.h>

#include "ufs_byteorder.h"

#define	swapBigLongToHost(thing) ((thing) = NXSwapBigLongToHost(thing))
#define	swapBigShortToHost(thing) ((thing) = NXSwapBigShortToHost(thing))

void
swapBigIntsToHost(int *array, int count)
{
	register int	i;

	for (i = 0;  i < count;  i++)
		swapBigLongToHost(array[i]);
}


void
swapBigShortToHosts(short *array, int count)
{
	register int	i;

	for (i = 0;  i < count;  i++)
		swapBigShortToHost(array[i]);
}


void
byte_swap_superblock(struct fs *sb)
{
#ifdef notyet
	swapBigIntsToHost(((int *) sb) + 2, 50);
	
	swapBigLongToHost(sb->fs_cgrotor);
	swapBigLongToHost(sb->fs_cpc);

	swapBigShortToHosts((short *) sb->fs_postbl, MAXCPG * NRPOS); 

	swapBigLongToHost(sb->fs_magic);
#endif
}


void
byte_swap_inode_in(struct dinode *dc, struct dinode *ic)
{
#ifdef notyet
	register int		i;

	ic->ic_mode = NXSwapBigShortToHost(dc->ic_mode);
	ic->ic_nlink = NXSwapBigShortToHost(dc->ic_nlink);
//	ic->ic_uid = NXSwapBigShortToHost(dc->ic_uid);
//	ic->ic_gid = NXSwapBigShortToHost(dc->ic_gid);

	ic->ic_size.val[0] = NXSwapBigLongToHost(dc->ic_size.val[1]);
	ic->ic_size.val[1] = NXSwapBigLongToHost(dc->ic_size.val[0]);

//	ic->ic_atime = NXSwapBigLongToHost(dc->ic_atime);
//	ic->ic_mtime = NXSwapBigLongToHost(dc->ic_mtime);
//	ic->ic_ctime = NXSwapBigLongToHost(dc->ic_ctime);
//	ic->ic_atspare = NXSwapBigLongToHost(dc->ic_atspare);
//	ic->ic_mtspare = NXSwapBigLongToHost(dc->ic_mtspare);
//	ic->ic_ctspare = NXSwapBigLongToHost(dc->ic_ctspare);

	ic->ic_flags = NXSwapBigLongToHost(dc->ic_flags);

	if ((ic->ic_flags & IC_FASTLINK) == 0) { /* not a fast symlink */

		for (i=0; i < NDADDR; i++)	/* direct blocks */
			ic->ic_db[i] = NXSwapBigLongToHost(dc->ic_db[i]);
	
		for (i=0; i < NIADDR; i++)	/* indirect blocks */
			ic->ic_ib[i] = NXSwapBigLongToHost(dc->ic_ib[i]);
	}
	else
		bcopy(dc->ic_symlink, ic->ic_symlink, sizeof(dc->ic_symlink));

	ic->ic_blocks = NXSwapBigLongToHost(dc->ic_blocks);
	ic->ic_gen = NXSwapBigLongToHost(dc->ic_gen);
	for (i=0; i < sizeof(ic->ic_spare) / sizeof(int); i++)
		ic->ic_spare[i] = NXSwapBigLongToHost(dc->ic_spare[i]);
#else
	*ic = *dc;
#endif
}


void
byte_swap_dir_block_in(char *addr, int count)
{
#ifdef notyet
	register struct direct	*ep = (struct direct *) addr;
	register int		entryoffsetinblk = 0;

	while (entryoffsetinblk < count) {
		ep = (struct direct *) (entryoffsetinblk + addr);
		swapBigLongToHost(ep->d_ino);
		swapBigShortToHost(ep->d_reclen);
		swapBigShortToHost(ep->d_namlen);
		entryoffsetinblk += ep->d_reclen;
		if (ep->d_reclen < 12)		/* handle garbage in dirs */
			break;
	}
#endif
}
