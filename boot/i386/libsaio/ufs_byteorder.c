/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

void
byte_swap_longlongs(unsigned long long *array, int count)
{
	register unsigned long long	i;

	for (i = 0;  i < (unsigned long long)count;  i++)
		byte_swap_longlong(array[i]);
}

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
	u_int16_t *   usptr;
	unsigned long size;

	byte_swap_ints(((int32_t *)&sb->fs_firstfield), 52);
	byte_swap_int(sb->fs_cgrotor);
	byte_swap_int(sb->fs_cpc);
	byte_swap_shorts((int16_t *)sb->fs_opostbl, 16 * 8); 
	byte_swap_ints((int32_t *)sb->fs_sparecon, 50);
	byte_swap_ints((int32_t *)&sb->fs_contigsumsize, 3);
	byte_swap_longlongs((u_int64_t *)&sb->fs_maxfilesize,3);
	byte_swap_ints((int32_t *)&sb->fs_state, 6);

	/* Got these magic numbers from mkfs.c in newfs */
	if (sb->fs_nrpos != 8 || sb->fs_cpc > 16) {
		usptr = (u_int16_t *)((u_int8_t *)(sb) + (sb)->fs_postbloff);
		size = sb->fs_cpc * sb->fs_nrpos;
		byte_swap_shorts(usptr,size);	/* fs_postbloff */
	}
}

static inline void
byte_swap_disklabel_common(disk_label_t *dl)
{
	swapBigLongToHost(dl->dl_version);	/* ditto */
	swapBigLongToHost(dl->dl_label_blkno);
	swapBigLongToHost(dl->dl_size);
	swapBigLongToHost(dl->dl_flags);
	swapBigLongToHost(dl->dl_tag);
//	swapBigShortToHost(dl->dl_checksum);
//	if (dl->dl_version >= DL_V3)
//		swapBigShortToHost(dl->dl_un.DL_v3_checksum);
//	else
//		swapBigIntsToHost(dl->dl_un.DL_bad, NBAD);
}

void
byte_swap_disklabel_in(disk_label_t *dl)
{
	byte_swap_disklabel_common(dl);
	byte_swap_disktab_in(&dl->dl_dt);
}

static inline void
byte_swap_disktab_common(struct disktab *dt)
{
	register unsigned int	i;

	swapBigLongToHost(dt->d_secsize);
	swapBigLongToHost(dt->d_ntracks);
	swapBigLongToHost(dt->d_nsectors);
	swapBigLongToHost(dt->d_ncylinders);
//	swapBigLongToHost(dt->d_rpm);
	swapBigShortToHost(dt->d_front);
	swapBigShortToHost(dt->d_back);
//	swapBigShortToHost(dt->d_ngroups);
//	swapBigShortToHost(dt->d_ag_size);
//	swapBigShortToHost(dt->d_ag_alts);
//	swapBigShortToHost(dt->d_ag_off);
//	swapBigIntsToHost(dt->d_boot0_blkno, NBOOTS);

	for (i=0; i < NPART; i++)
		byte_swap_partition(&dt->d_partitions[i]);
}

/*
 *  This is particularly grody.  The beginning of the partition array is two
 *  bytes low on the 68 wrt natural alignment rules.  Furthermore, each
 *  element of the partition table is two bytes smaller on 68k due to padding
 *  at the end of the struct.
 */
void
byte_swap_disktab_in(struct disktab *dt)
{
	struct partition * pp;
	int                i;

	/*
	 *  Shift each struct partition up in memory by 2 + 2 * offset bytes.
	 *  Do it backwards so we don't overwrite anything.
	 */
	for (i=NPART - 1; i >=0; i--) {
		struct partition temp;
		pp = &dt->d_partitions[i];
		/* beware: compiler doesn't do overlapping struct assignment */
		temp = *(struct partition *)(((char *) pp) - 2 * (i + 1));
		*pp = temp;
	}

	byte_swap_disktab_common(dt);
}

void
byte_swap_partition(struct partition *part)
{
	swapBigLongToHost(part->p_base);
	swapBigLongToHost(part->p_size);
	swapBigShortToHost(part->p_bsize);
	swapBigShortToHost(part->p_fsize);
	swapBigShortToHost(part->p_cpg);
	swapBigShortToHost(part->p_density);
}

/* This value should correspond to the value set in the ffs_mounts */

#define RESYMLNKLEN 60

void
byte_swap_dinode_in(struct dinode *di)
{
	int i;

	di->di_mode = NXSwapShort(di->di_mode);
	di->di_nlink = NXSwapShort(di->di_nlink);
#ifdef LFS
	di->di_u.inumber = NXSwapLong(di->di_u.inumber);
#else
	di->di_u.oldids[0] = NXSwapShort(di->di_u.oldids[0]);
	di->di_u.oldids[1] = NXSwapShort(di->di_u.oldids[1]);
#endif
	di->di_size = NXSwapLongLong(di->di_size);
	di->di_atime = NXSwapLong(di->di_atime);
	di->di_atimensec = NXSwapLong(di->di_atimensec);
	di->di_mtime = NXSwapLong(di->di_mtime);
	di->di_mtimensec = NXSwapLong(di->di_mtimensec);
	di->di_ctime = NXSwapLong(di->di_ctime);
	di->di_ctimensec = NXSwapLong(di->di_ctimensec);
	if (((di->di_mode & IFMT) != IFLNK ) || (di->di_size > RESYMLNKLEN)) {
		for (i=0; i < NDADDR; i++)	/* direct blocks */
			di->di_db[i] = NXSwapLong(di->di_db[i]);
		for (i=0; i < NIADDR; i++)	/* indirect blocks */
			di->di_ib[i] = NXSwapLong(di->di_ib[i]);
	}
	di->di_flags = NXSwapLong(di->di_flags);
	di->di_blocks = NXSwapLong(di->di_blocks);
	di->di_gen = NXSwapLong(di->di_gen);
	di->di_uid = NXSwapLong(di->di_uid);
	di->di_gid = NXSwapLong(di->di_gid);
	di->di_spare[0] = NXSwapLong(di->di_spare[0]);
	di->di_spare[1] = NXSwapLong(di->di_spare[1]);
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
