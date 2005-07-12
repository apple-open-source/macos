/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/* $FreeBSD: src/sys/msdosfs/msdosfs_fat.c,v 1.24 2000/05/05 09:58:35 phk Exp $ */
/*	$NetBSD: msdosfs_fat.c,v 1.28 1997/11/17 15:36:49 ws Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

/*
 * kernel include files.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/mount.h>		/* to define statfs structure */
#include <sys/vnode.h>		/* to define vattr structure */
#include <sys/ubc.h>

/*
 * msdosfs include files.
 */
#include "bpb.h"
#include "msdosfsmount.h"
#include "direntry.h"
#include "denode.h"
#include "fat.h"

/*
 * Fat cache stats.
 *
 * These are all protected by the funnel.
 */
static int fc_fileextends;	/* # of file extends */
static int fc_lfcempty;		/* # of time last file cluster cache entry was empty */
static int fc_bmapcalls;	/* # of times pcbmap was called */

/*
 * Counters for how far off the last cluster mapped entry was.
 */
#define	LMMAX	20
static int fc_lmdistance[LMMAX];	/* off by small amount */
static int fc_largedistance;		/* off by more than LMMAX */

/*
 * Internal routines
 */
static int pcbmap_internal(struct denode *dep,
				u_long findcn, u_long numclusters,
				daddr64_t *bnp, u_long *cnp, u_long *sp, vfs_context_t context);
static int clusterfree_internal(struct msdosfsmount *pmp,
				u_long cluster, u_long *oldcnp, vfs_context_t context);
static int fatentry_internal(int function, struct msdosfsmount *pmp,
				u_long cn, u_long *oldcontents, u_long newcontents, vfs_context_t context);
static int clusteralloc_internal(struct msdosfsmount *pmp,
				u_long start, u_long count, u_long fillwith,
				u_long *retcluster, u_long *got, vfs_context_t context);
static int	chainalloc __P((struct msdosfsmount *pmp, u_long start,
				u_long count, u_long fillwith,
				u_long *retcluster, u_long *got, vfs_context_t context));
static int	chainlength __P((struct msdosfsmount *pmp, u_long start,
				u_long count));
static void	fatblock __P((struct msdosfsmount *pmp, u_long ofs,
				daddr64_t *bnp, u_long *sizep, u_long *bop));
static int	fatchain __P((struct msdosfsmount *pmp, u_long start,
				u_long count, u_long fillwith, vfs_context_t context));
static void	fc_lookup __P((struct denode *dep, u_long findcn,
				u_long *frcnp, u_long *fsrcnp));
static void	updatefats __P((struct msdosfsmount *pmp, struct buf *bp,
				daddr64_t fatbn, vfs_context_t context));
static int fillinusemap(struct msdosfsmount *pmp, vfs_context_t context);
static __inline void
		usemap_alloc __P((struct msdosfsmount *pmp, u_long cn));
static __inline void
		usemap_free __P((struct msdosfsmount *pmp, u_long cn));

static lck_grp_attr_t *fat_lock_group_attr;
static lck_grp_t *fat_lock_group;

__private_extern__ void
msdosfs_fat_init(void)
{
	fat_lock_group_attr = lck_grp_attr_alloc_init();
	fat_lock_group = lck_grp_alloc_init("msdosfs fat", fat_lock_group_attr);
}

__private_extern__ void
msdosfs_fat_uninit(void)
{
	lck_grp_free(fat_lock_group);
	lck_grp_attr_free(fat_lock_group_attr);
}

__private_extern__ int
msdosfs_fat_init_vol(struct msdosfsmount *pmp, vfs_context_t context)
{
	pmp->pm_fat_lock_attr = lck_attr_alloc_init();
	pmp->pm_fat_lock = lck_mtx_alloc_init(fat_lock_group, pmp->pm_fat_lock_attr);
	
	/*
	 * Allocate memory for the bitmap of allocated clusters, and then
	 * fill it in.
	 */
	MALLOC(pmp->pm_inusemap, u_int *,
	       ((pmp->pm_maxcluster + N_INUSEBITS - 1) / N_INUSEBITS) *
	        sizeof(*pmp->pm_inusemap), M_TEMP, M_WAITOK);

	if (pmp->pm_inusemap == NULL)
		return ENOMEM;	/* Locks are cleaned up in msdosfs_fat_uninit_vol */

	return fillinusemap(pmp, context);
}

__private_extern__ void
msdosfs_fat_uninit_vol(struct msdosfsmount *pmp)
{
	if (pmp->pm_inusemap)
		FREE(pmp->pm_inusemap, M_TEMP);
	if (pmp->pm_fat_lock)
		lck_mtx_free(pmp->pm_fat_lock, fat_lock_group);
	if (pmp->pm_fat_lock_attr)
		lck_attr_free(pmp->pm_fat_lock_attr);
}



/*
 * Given a byte offset from the start of the FAT, return the
 * FAT block that contains that offset.
 *
 * Inputs:
 *	pmp	mount point
 *	ofs	offset from start of the FAT
 *
 * Outputs:
 *	bnp	physical (device) block number containing FAT block
 *	sizep	size of the entire FAT block, in bytes
 *	bop	byte offset of "ofs" from start of FAT block
 */
static void
fatblock(pmp, ofs, bnp, sizep, bop)
	struct msdosfsmount *pmp;
	u_long ofs;
	daddr64_t *bnp;
	u_long *sizep;
	u_long *bop;
{
	u_long bn, size;
        
        /* Compute offset from start of FAT, in sectors */
	bn = ofs / pmp->pm_fatblocksize * pmp->pm_fatblocksec;
        
        /*
         * Compute the size of this FAT block, in bytes.
         * The total size of the FAT may not be a multiple
         * of pm_fatblocksize, so the last FAT block may be
         * shorter than the rest.
         */
	size = min(pmp->pm_fatblocksec, pmp->pm_FATsecs - bn)
	    * pmp->pm_BytesPerSec;

        /* Compute offset from start of volume, in sectors */
	bn += pmp->pm_ResSectors + pmp->pm_curfat * pmp->pm_FATsecs;

        /* Convert sectors to physical (device) blocks */
        bn *= pmp->pm_BlocksPerSec;
        
	if (bnp)
		*bnp = bn;
	if (sizep)
		*sizep = size;
	if (bop)
		*bop = ofs % pmp->pm_fatblocksize;
}

/*
 * Map the logical cluster number of a file into a physical disk sector
 * that is filesystem relative.
 *
 * dep	  - address of denode representing the file of interest
 * findcn - file relative cluster whose filesystem relative cluster number
 *	    and/or block number are/is to be found
 * bnp	  - address of where to place the file system relative block number.
 *	    If this pointer is null then don't return this quantity.
 * cnp	  - address of where to place the file system relative cluster number.
 *	    If this pointer is null then don't return this quantity.
 *
 * NOTE: Either bnp or cnp must be non-null.
 * This function has one side effect.  If the requested file relative cluster
 * is beyond the end of file, then the actual number of clusters in the file
 * is returned in *cnp.  This is useful for determining how long a directory is.
 *  If cnp is null, nothing is returned.
 */
static int
pcbmap_internal(dep, findcn, numclusters, bnp, cnp, sp, context)
	struct denode *dep;
	u_long findcn;		/* file relative cluster to get */
    u_long numclusters;	/* maximum number of contiguous clusters to map */
	daddr64_t *bnp;		/* returned filesys relative blk number	 */
	u_long *cnp;		/* returned cluster number */
	u_long *sp;			/* returned block size */
	vfs_context_t context;
{
	int error=0;
	u_long i;
	u_long cn;
	u_long prevcn = 0; /* XXX: prevcn could be used unititialized */
	u_long byteoffset;
	daddr64_t bn;
	u_long bo;
	struct buf *bp = NULL;
	daddr64_t bp_bn = -1;
	struct msdosfsmount *pmp = dep->de_pmp;
	u_long bsize;
	char *bdata;

	fc_bmapcalls++;

	if (numclusters == 0)
		panic("pcbmap: numclusters == 0");

	/*
	 * If they don't give us someplace to return a value then don't
	 * bother doing anything.
	 */
	if (bnp == NULL && cnp == NULL && sp == NULL)
		goto exit;

	cn = dep->de_StartCluster;
	/*
	 * The "file" that makes up the root directory is contiguous,
	 * permanently allocated, of fixed size, and is not made up of
	 * clusters.  If the cluster number is beyond the end of the root
	 * directory, then return the number of clusters in the file.
	 */
	if (cn == MSDOSFSROOT) {
		if (dep->de_Attributes & ATTR_DIRECTORY) {
			if (de_cn2off(pmp, findcn) >= dep->de_FileSize) {
				if (cnp)
					*cnp = de_bn2cn(pmp, pmp->pm_rootdirsize);
				error = E2BIG;
				goto exit;
			}
			if (bnp)
				*bnp = pmp->pm_rootdirblk + de_cn2bn(pmp, findcn);
			if (cnp)
				*cnp = MSDOSFSROOT;
			if (sp)
				*sp = min(pmp->pm_bpcluster,
				    dep->de_FileSize - de_cn2off(pmp, findcn));
			goto exit;
		} else {		/* just an empty file */
			if (cnp)
				*cnp = 0;
			error = E2BIG;
			goto exit;
		}
	}

	/*
	 * All other files do I/O in cluster sized blocks
	 */
	if (sp)
		*sp = pmp->pm_bpcluster;

	/*
	 * Rummage around in the fat cache, maybe we can avoid tromping
	 * thru every fat entry for the file. And, keep track of how far
	 * off the cache was from where we wanted to be.
	 */
	i = 0;
	fc_lookup(dep, findcn, &i, &cn);
	if ((bn = findcn - i) >= LMMAX)
		fc_largedistance++;
	else
		fc_lmdistance[bn]++;

	/*
	 * Handle all other files or directories the normal way.
	 */
	for (; i < findcn; i++) {
		/*
		 * Stop with all reserved clusters, not just with EOF.
		 */
		if ((cn | ~pmp->pm_fatmask) >= CLUST_RSRVD)
			goto hiteof;
		byteoffset = FATOFS(pmp, cn);
		fatblock(pmp, byteoffset, &bn, &bsize, &bo);
		if (bn != bp_bn) {
			if (bp)
				buf_brelse(bp);
			error = (int)buf_meta_bread(pmp->pm_devvp, bn, bsize, vfs_context_ucred(context), &bp);
			if (error) {
				buf_brelse(bp);
				goto exit;
			}
			bp_bn = bn;
		}
		bdata = (char *)buf_dataptr(bp);
		prevcn = cn;

		if (FAT32(pmp))
			cn = getulong(&bdata[bo]);
		else
			cn = getushort(&bdata[bo]);
		if (FAT12(pmp) && (prevcn & 1))
			cn >>= 4;
		cn &= pmp->pm_fatmask;

		/*
		 * Force the special cluster numbers
		 * to be the same for all cluster sizes
		 * to let the rest of msdosfs handle
		 * all cases the same.
		 */
		if ((cn | ~pmp->pm_fatmask) >= CLUST_RSRVD)
			cn |= ~pmp->pm_fatmask;
	}
        
	if ((cn | ~pmp->pm_fatmask) >= CLUST_RSRVD)
		goto hiteof;

	/*
	 * Return the block and cluster of the start of the range.
	 */
	if (bnp)
			*bnp = cntobn(pmp, cn);
	if (cnp)
			*cnp = cn;

	/*
	* See how many clusters are contiguous, up to numclusters.
	*/
	for ( ; i < findcn + numclusters - 1; ++i) {
		byteoffset = FATOFS(pmp, cn);
		fatblock(pmp, byteoffset, &bn, &bsize, &bo);
		if (bn != bp_bn) {
			if (bp)
				buf_brelse(bp);
			error = (int)buf_meta_bread(pmp->pm_devvp, bn, bsize, vfs_context_ucred(context), &bp);
			if (error) {
				buf_brelse(bp);
				goto exit;
			}
			bp_bn = bn;
		}
		bdata = (char *)buf_dataptr(bp);
		prevcn = cn;

		if (FAT32(pmp))
			cn = getulong(&bdata[bo]);
		else
			cn = getushort(&bdata[bo]);
		if (FAT12(pmp) && (prevcn & 1))
			cn >>= 4;
		cn &= pmp->pm_fatmask;
		
		if (cn != prevcn+1) {
			cn = prevcn;
			break;
		}
	}
	
	if (sp)
		*sp = (i - findcn + 1) * pmp->pm_bpcluster;

	fc_setcache(dep, FC_LASTMAP, i, cn);
	
	if (bp)
		buf_brelse(bp);
exit:
	return error;

hiteof:
	if (cnp)
		*cnp = i;
	if (bp)
		buf_brelse(bp);
	/* update last file cluster entry in the fat cache */
	fc_setcache(dep, FC_LASTFC, i - 1, prevcn);
	return E2BIG;
}

__private_extern__ int
pcbmap(dep, findcn, numclusters, bnp, cnp, sp, context)
	struct denode *dep;
	u_long findcn;
    u_long numclusters;
	daddr64_t *bnp;
	u_long *cnp;
	u_long *sp;
	vfs_context_t context;
{
	int error;
	
	lck_mtx_lock(dep->de_pmp->pm_fat_lock);
	error = pcbmap_internal(dep, findcn, numclusters, bnp, cnp, sp, context);
	lck_mtx_unlock(dep->de_pmp->pm_fat_lock);
	
	return error;
}

/*
 * Find the closest entry in the fat cache to the cluster we are looking
 * for.
 */
static void
fc_lookup(dep, findcn, frcnp, fsrcnp)
	struct denode *dep;
	u_long findcn;
	u_long *frcnp;
	u_long *fsrcnp;
{
	int i;
	u_long cn;
	struct fatcache *closest = 0;

	for (i = 0; i < FC_SIZE; i++) {
		cn = dep->de_fc[i].fc_frcn;
		if (cn != FCE_EMPTY && cn <= findcn) {
			if (closest == 0 || cn > closest->fc_frcn)
				closest = &dep->de_fc[i];
		}
	}
	if (closest) {
		*frcnp = closest->fc_frcn;
		*fsrcnp = closest->fc_fsrcn;
	}
}

/*
 * Purge the fat cache in denode dep of all entries relating to file
 * relative cluster frcn and beyond.
 *
 * Note: This routine doesn't do any FAT locking because it only
 * manipulates structures within a single denode.  Therefore, the
 * denode's lock is sufficient.  (In fact, since nothing here blocks,
 * the msdosfs funnel is also sufficient.)
 */
__private_extern__ void
fc_purge(dep, frcn)
	struct denode *dep;
	u_int frcn;
{
	int i;
	struct fatcache *fcp;

	fcp = dep->de_fc;
	for (i = 0; i < FC_SIZE; i++, fcp++) {
		if (fcp->fc_frcn >= frcn)
			fcp->fc_frcn = FCE_EMPTY;
	}
}

/*
 * Update the fat.
 * If mirroring the fat, update all copies, with the first copy as last.
 * Else update only the current fat (ignoring the others).
 *
 * pmp	 - msdosfsmount structure for filesystem to update
 * bp	 - addr of modified fat block
 * fatbn - block number relative to begin of filesystem of the modified fat block.
 */
static void
updatefats(pmp, bp, fatbn, context)
	struct msdosfsmount *pmp;
	struct buf *bp;
	daddr64_t fatbn;
	vfs_context_t context;
{
	int i;
	struct buf *bpn;

	/*
	 * If we have an FSInfo block, update it.
	 */
	if (pmp->pm_fsinfo) {
		/*
		 * [2734381] The FSInfo sector occupies pm_BytesPerSec bytes on disk,
		 * but only 512 of those have meaningful contents.  There's no point
		 * in reading all pm_BytesPerSec bytes if the device block size is
		 * smaller.  So just use the device block size here.
		 */
		if (buf_meta_bread(pmp->pm_devvp, pmp->pm_fsinfo, pmp->pm_BlockSize, 
			vfs_context_ucred(context), &bpn) != 0) {
			/*
			 * Ignore the error, but turn off FSInfo update for the future.
			 */
			pmp->pm_fsinfo = 0;
			buf_brelse(bpn);
		} else {
			struct fsinfo *fp = (struct fsinfo *)buf_dataptr(bpn);

			putulong(fp->fsinfree, pmp->pm_freeclustercount);
                        /* If we ever start using pmp->pm_nxtfree, then we should update it on disk: */
			/* putulong(fp->fsinxtfree, pmp->pm_nxtfree); */
			if (pmp->pm_flags & MSDOSFSMNT_WAITONFAT)
				(void)buf_bwrite(bpn);
			else
				buf_bawrite(bpn);
		}
	}

	if (pmp->pm_flags & MSDOSFS_FATMIRROR) {
		/*
		 * Now copy the block(s) of the modified fat to the other copies of
		 * the fat and write them out.  This is faster than reading in the
		 * other fats and then writing them back out.  This could tie up
		 * the fat for quite a while. Preventing others from accessing it.
		 * To prevent us from going after the fat quite so much we use
		 * delayed writes, unless they specfied "synchronous" when the
		 * filesystem was mounted.  If synch is asked for then use
		 * buf_bwrite()'s and really slow things down.
		 */
		for (i = 1; i < pmp->pm_FATs; i++) {
			fatbn += pmp->pm_FATsecs;
			/* buf_getblk() never fails */
			bpn = buf_getblk(pmp->pm_devvp, fatbn, buf_count(bp), 0, 0, BLK_META);
			bcopy((char *)buf_dataptr(bp), (char *)buf_dataptr(bpn), buf_count(bp));
			if (pmp->pm_flags & MSDOSFSMNT_WAITONFAT)
				(void)buf_bwrite(bpn);
			else
				buf_bawrite(bpn);
		}
	}

	/*
	 * Write out the first (or current) fat last.
	 */
	if (pmp->pm_flags & MSDOSFSMNT_WAITONFAT)
		(void)buf_bwrite(bp);
	else
		buf_bawrite(bp);
	/*
	 * Maybe update fsinfo sector here?
	 */
}

/*
 * Updating entries in 12 bit fats is a pain in the butt.
 *
 * The following picture shows where nibbles go when moving from a 12 bit
 * cluster number into the appropriate bytes in the FAT.
 *
 *	byte m        byte m+1      byte m+2
 *	+----+----+   +----+----+   +----+----+
 *	|  0    1 |   |  2    3 |   |  4    5 |   FAT bytes
 *	+----+----+   +----+----+   +----+----+
 *
 *	+----+----+----+   +----+----+----+
 *	|  3    0    1 |   |  4    5    2 |
 *	+----+----+----+   +----+----+----+
 *	cluster n  	   cluster n+1
 *
 * Where n is even. m = n + (n >> 2)
 *
 */
static __inline void
usemap_alloc(pmp, cn)
	struct msdosfsmount *pmp;
	u_long cn;
{

	pmp->pm_inusemap[cn / N_INUSEBITS] |= 1 << (cn % N_INUSEBITS);
	pmp->pm_freeclustercount--;
}

static __inline void
usemap_free(pmp, cn)
	struct msdosfsmount *pmp;
	u_long cn;
{

	pmp->pm_freeclustercount++;
	pmp->pm_inusemap[cn / N_INUSEBITS] &= ~(1 << (cn % N_INUSEBITS));
}

static int
clusterfree_internal(pmp, cluster, oldcnp, context)
	struct msdosfsmount *pmp;
	u_long cluster;
	u_long *oldcnp;
	vfs_context_t context;
{
	int error;
	u_long oldcn;

	usemap_free(pmp, cluster);
	error = fatentry_internal(FAT_GET_AND_SET, pmp, cluster, &oldcn, MSDOSFSFREE, context);
	if (error) {
		usemap_alloc(pmp, cluster);
		return (error);
	}
	/*
	 * If the cluster was successfully marked free, then update
	 * the count of free clusters, and turn off the "allocated"
	 * bit in the "in use" cluster bit map.
	 */
	if (oldcnp)
		*oldcnp = oldcn;
	return (0);
}

__private_extern__ int
clusterfree(pmp, cluster, oldcnp, context)
	struct msdosfsmount *pmp;
	u_long cluster;
	u_long *oldcnp;
	vfs_context_t context;
{
	int error;
	
	lck_mtx_lock(pmp->pm_fat_lock);
	error = clusterfree_internal(pmp, cluster, oldcnp, context);
	lck_mtx_unlock(pmp->pm_fat_lock);
	
	return error;
}


/*
 * Get or Set or 'Get and Set' the cluster'th entry in the fat.
 *
 * function	- whether to get or set a fat entry
 * pmp		- address of the msdosfsmount structure for the filesystem
 *		  whose fat is to be manipulated.
 * cn		- which cluster is of interest
 * oldcontents	- address of a word that is to receive the contents of the
 *		  cluster'th entry if this is a get function
 * newcontents	- the new value to be written into the cluster'th element of
 *		  the fat if this is a set function.
 *
 * This function can also be used to free a cluster by setting the fat entry
 * for a cluster to 0.
 *
 * All copies of the fat are updated if this is a set function. NOTE: If
 * fatentry() marks a cluster as free it does not update the inusemap in
 * the msdosfsmount structure. This is left to the caller.
 */
static int
fatentry_internal(function, pmp, cn, oldcontents, newcontents, context)
	int function;
	struct msdosfsmount *pmp;
	u_long cn;
	u_long *oldcontents;
	u_long newcontents;
	vfs_context_t context;
{
	int error;
	u_long readcn;
	daddr64_t bn;
	u_long bo, bsize, byteoffset;
	struct buf *bp;
	char *bdata;

	/*
	 * Be sure the requested cluster is in the filesystem.
	 */
	if (cn < CLUST_FIRST || cn > pmp->pm_maxcluster)
		return (EINVAL);

	byteoffset = FATOFS(pmp, cn);
	fatblock(pmp, byteoffset, &bn, &bsize, &bo);
        error = (int)buf_meta_bread(pmp->pm_devvp, bn, bsize, vfs_context_ucred(context), &bp);
	if (error) {
		buf_brelse(bp);
		return (error);
	}
	bdata = (char *)buf_dataptr(bp);

	if (function & FAT_GET) {
		if (FAT32(pmp))
			readcn = getulong(&bdata[bo]);
		else
			readcn = getushort(&bdata[bo]);
		if (FAT12(pmp) & (cn & 1))
			readcn >>= 4;
		readcn &= pmp->pm_fatmask;
		/* map reserved fat entries to same values for all fats */
		if ((readcn | ~pmp->pm_fatmask) >= CLUST_RSRVD)
			readcn |= ~pmp->pm_fatmask;
		*oldcontents = readcn;
	}
	if (function & FAT_SET) {
		switch (pmp->pm_fatmask) {
		case FAT12_MASK:
			readcn = getushort(&bdata[bo]);
			if (cn & 1) {
				readcn &= 0x000f;
				readcn |= newcontents << 4;
			} else {
				readcn &= 0xf000;
				readcn |= newcontents & 0xfff;
			}
			putushort(&bdata[bo], readcn);
			break;
		case FAT16_MASK:
			putushort(&bdata[bo], newcontents);
			break;
		case FAT32_MASK:
			/*
			 * According to spec we have to retain the
			 * high order bits of the fat entry.
			 */
			readcn = getulong(&bdata[bo]);
			readcn &= ~FAT32_MASK;
			readcn |= newcontents & FAT32_MASK;
			putulong(&bdata[bo], readcn);
			break;
		}
		updatefats(pmp, bp, bn, context);
		bp = NULL;
		pmp->pm_fmod = 1;
	}
	if (bp)
		buf_brelse(bp);
	return (0);
}

__private_extern__ int
fatentry(function, pmp, cn, oldcontents, newcontents, context)
	int function;
	struct msdosfsmount *pmp;
	u_long cn;
	u_long *oldcontents;
	u_long newcontents;
	vfs_context_t context;
{
	int error;
	
	lck_mtx_lock(pmp->pm_fat_lock);
	error = fatentry_internal(function, pmp, cn, oldcontents, newcontents, context);
	lck_mtx_unlock(pmp->pm_fat_lock);
	
	return error;
}


/*
 * Update a contiguous cluster chain
 *
 * pmp	    - mount point
 * start    - first cluster of chain
 * count    - number of clusters in chain
 * fillwith - what to write into fat entry of last cluster
 */
static int
fatchain(pmp, start, count, fillwith, context)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
	u_long fillwith;
	vfs_context_t context;
{
	int error;
	daddr64_t bn;
	u_long bo, bsize, byteoffset, readcn, newc;
	struct buf *bp;
	char *bdata;

	/*
	 * Be sure the clusters are in the filesystem.
	 */
	if (start < CLUST_FIRST || start + count - 1 > pmp->pm_maxcluster)
		return (EINVAL);

	while (count > 0) {
		byteoffset = FATOFS(pmp, start);
		fatblock(pmp, byteoffset, &bn, &bsize, &bo);
                error = (int)buf_meta_bread(pmp->pm_devvp, bn, bsize, vfs_context_ucred(context), &bp);
		if (error) {
			buf_brelse(bp);
			return (error);
		}
		bdata = (char *)buf_dataptr(bp);

		while (count > 0) {
			start++;
			newc = --count > 0 ? start : fillwith;
			switch (pmp->pm_fatmask) {
			case FAT12_MASK:
				readcn = getushort(&bdata[bo]);
				if (start & 1) {
					readcn &= 0xf000;
					readcn |= newc & 0xfff;
				} else {
					readcn &= 0x000f;
					readcn |= newc << 4;
				}
				putushort(&bdata[bo], readcn);
				bo++;
				if (!(start & 1))
					bo++;
				break;
			case FAT16_MASK:
				putushort(&bdata[bo], newc);
				bo += 2;
				break;
			case FAT32_MASK:
				readcn = getulong(&bdata[bo]);
				readcn &= ~pmp->pm_fatmask;
				readcn |= newc & pmp->pm_fatmask;
				putulong(&bdata[bo], readcn);
				bo += 4;
				break;
			}
			if (bo >= bsize)
				break;
		}
		updatefats(pmp, bp, bn, context);
	}
	pmp->pm_fmod = 1;
	return (0);
}

/*
 * Check the length of a free cluster chain starting at start.
 *
 * pmp	 - mount point
 * start - start of chain
 * count - maximum interesting length
 */
static int
chainlength(pmp, start, count)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
{
	u_long idx, max_idx;
	u_int map;
	u_long len;

	max_idx = pmp->pm_maxcluster / N_INUSEBITS;
	idx = start / N_INUSEBITS;
	start %= N_INUSEBITS;
	map = pmp->pm_inusemap[idx];
	map &= ~((1 << start) - 1);
	if (map) {
		len = ffs(map) - 1 - start;
		return (len > count ? count : len);
	}
	len = N_INUSEBITS - start;
	if (len >= count)
		return (count);
	while (++idx <= max_idx) {
		if (len >= count)
			break;
		map = pmp->pm_inusemap[idx];
		if (map) {
			len +=  ffs(map) - 1;
			break;
		}
		len += N_INUSEBITS;
	}
	return (len > count ? count : len);
}

/*
 * Allocate contigous free clusters.
 *
 * pmp	      - mount point.
 * start      - start of cluster chain.
 * count      - number of clusters to allocate.
 * fillwith   - put this value into the fat entry for the
 *		last allocated cluster.
 * retcluster - put the first allocated cluster's number here.
 * got	      - how many clusters were actually allocated.
 */
static int
chainalloc(pmp, start, count, fillwith, retcluster, got, context)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
	u_long fillwith;
	u_long *retcluster;
	u_long *got;
	vfs_context_t context;
{
	int error;
	u_long cl, n;

	for (cl = start, n = count; n-- > 0;)
		usemap_alloc(pmp, cl++);

	error = fatchain(pmp, start, count, fillwith, context);
	if (error != 0)
		return (error);
	if (retcluster)
		*retcluster = start;
	if (got)
		*got = count;
	return (0);
}

/*
 * Allocate contiguous free clusters.
 *
 * pmp	      - mount point.
 * start      - preferred start of cluster chain.
 * count      - number of clusters requested.
 * fillwith   - put this value into the fat entry for the
 *		last allocated cluster.
 * retcluster - put the first allocated cluster's number here.
 * got	      - how many clusters were actually allocated.
 */
static int
clusteralloc_internal(pmp, start, count, fillwith, retcluster, got, context)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
	u_long fillwith;
	u_long *retcluster;
	u_long *got;
	vfs_context_t context;
{
	u_long idx;
	u_long len, foundl, cn, l;
	u_long foundcn = 0; /* XXX: foundcn could be used unititialized */
	u_int map;

	if (start) {
		if ((len = chainlength(pmp, start, count)) >= count)
			return (chainalloc(pmp, start, count, fillwith, retcluster, got, context));
	} else 
		len = 0;

	foundl = 0;

	for (cn = 0; cn <= pmp->pm_maxcluster;) {
		idx = cn / N_INUSEBITS;
		map = pmp->pm_inusemap[idx];
		map |= (1 << (cn % N_INUSEBITS)) - 1;
		if (map != (u_int)-1) {
			cn = idx * N_INUSEBITS + ffs(map^(u_int)-1) - 1;
			if ((l = chainlength(pmp, cn, count)) >= count)
				return (chainalloc(pmp, cn, count, fillwith, retcluster, got, context));
			if (l > foundl) {
				foundcn = cn;
				foundl = l;
			}
			cn += l + 1;
			continue;
		}
		cn += N_INUSEBITS - cn % N_INUSEBITS;
	}

	if (!foundl)
		return (ENOSPC);

	if (len)
		return (chainalloc(pmp, start, len, fillwith, retcluster, got, context));
	else
		return (chainalloc(pmp, foundcn, foundl, fillwith, retcluster, got, context));
}

__private_extern__ int
clusteralloc(pmp, start, count, fillwith, retcluster, got, context)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
	u_long fillwith;
	u_long *retcluster;
	u_long *got;
	vfs_context_t context;
{
	int error;
	
	lck_mtx_lock(pmp->pm_fat_lock);
	error = clusteralloc_internal(pmp, start, count, fillwith, retcluster, got, context);
	lck_mtx_unlock(pmp->pm_fat_lock);
	
	return error;
}


/*
 * Free a chain of clusters.
 *
 * pmp		- address of the msdosfs mount structure for the filesystem
 *		  containing the cluster chain to be freed.
 * startcluster - number of the 1st cluster in the chain of clusters to be
 *		  freed.
 */
__private_extern__ int
freeclusterchain(pmp, cluster, context)
	struct msdosfsmount *pmp;
	u_long cluster;
	vfs_context_t context;
{
	int error=0;
	struct buf *bp = NULL;
	daddr64_t bn;
	daddr64_t lbn = -1;
	u_long bo, bsize, byteoffset;
	u_long readcn;
	char *bdata;

	lck_mtx_lock(pmp->pm_fat_lock);

	while (cluster >= CLUST_FIRST && cluster <= pmp->pm_maxcluster) {
		byteoffset = FATOFS(pmp, cluster);
		fatblock(pmp, byteoffset, &bn, &bsize, &bo);
		if (lbn != bn) {
			if (bp)
				updatefats(pmp, bp, lbn, context);
			error = (int)buf_meta_bread(pmp->pm_devvp, bn, bsize, vfs_context_ucred(context), &bp);
			if (error) {
				buf_brelse(bp);
				goto exit;
			}
			lbn = bn;
		}
		bdata = (char *)buf_dataptr(bp);

		usemap_free(pmp, cluster);
		switch (pmp->pm_fatmask) {
		case FAT12_MASK:
			readcn = getushort(&bdata[bo]);
			if (cluster & 1) {
				cluster = readcn >> 4;
				readcn &= 0x000f;
				readcn |= MSDOSFSFREE << 4;
			} else {
				cluster = readcn;
				readcn &= 0xf000;
				readcn |= MSDOSFSFREE & 0xfff;
			}
			putushort(&bdata[bo], readcn);
			break;
		case FAT16_MASK:
			cluster = getushort(&bdata[bo]);
			putushort(&bdata[bo], MSDOSFSFREE);
			break;
		case FAT32_MASK:
			cluster = getulong(&bdata[bo]);
			putulong(&bdata[bo],
				 (MSDOSFSFREE & FAT32_MASK) | (cluster & ~FAT32_MASK));
			break;
		}
		cluster &= pmp->pm_fatmask;
		if ((cluster | ~pmp->pm_fatmask) >= CLUST_RSRVD)
			cluster |= pmp->pm_fatmask;
	}
	if (bp)
		updatefats(pmp, bp, bn, context);
exit:
	lck_mtx_unlock(pmp->pm_fat_lock);
	return error;
}

/*
 * Read in fat blocks looking for free clusters. For every free cluster
 * found turn off its corresponding bit in the pm_inusemap.
 */
static int
fillinusemap(pmp, context)
	struct msdosfsmount *pmp;
	vfs_context_t context;
{
	struct buf *bp = NULL;
	u_long cn, readcn;
	int error;
	daddr64_t bn;
	u_long bo, bsize, byteoffset;
	char *bdata;

	/*
	 * Mark all clusters in use, we mark the free ones in the fat scan
	 * loop further down.
	 */
	for (cn = 0; cn < (pmp->pm_maxcluster + N_INUSEBITS - 1) / N_INUSEBITS; cn++)
		pmp->pm_inusemap[cn] = (u_int)-1;

	/*
	 * Figure how many free clusters are in the filesystem by ripping
	 * through the fat counting the number of entries whose content is
	 * zero.  These represent free clusters.
	 */
	pmp->pm_freeclustercount = 0;
	for (cn = CLUST_FIRST; cn <= pmp->pm_maxcluster; cn++) {
		byteoffset = FATOFS(pmp, cn);
		bo = byteoffset % pmp->pm_fatblocksize;
		if (!bo || !bp) {
			/* Read new FAT block */
			if (bp)
				buf_brelse(bp);
			fatblock(pmp, byteoffset, &bn, &bsize, NULL);
				error = (int)buf_meta_bread(pmp->pm_devvp, bn, bsize, vfs_context_ucred(context), &bp);
			if (error) {
				buf_brelse(bp);
				return (error);
			}
		}
		bdata = (char *)buf_dataptr(bp);

		if (FAT32(pmp))
			readcn = getulong(&bdata[bo]);
		else
			readcn = getushort(&bdata[bo]);
		if (FAT12(pmp) && (cn & 1))
			readcn >>= 4;
		readcn &= pmp->pm_fatmask;

		if (readcn == 0)
			usemap_free(pmp, cn);
	}
	buf_brelse(bp);
	return (0);
}

/*
 * Allocate a new cluster and chain it onto the end of the file.
 *
 * dep	 - the file to extend
 * count - number of clusters to allocate
 *
 * NOTE: This function is not responsible for turning on the DE_UPDATE bit of
 * the de_flag field of the denode and it does not change the de_FileSize
 * field.  This is left for the caller to do.
 */
__private_extern__ int
extendfile(dep, count, context)
	struct denode *dep;
	u_long count;
	vfs_context_t context;
{
    int error=0;
    u_long frcn;
    u_long cn, got, reqcnt;
    struct msdosfsmount *pmp = dep->de_pmp;
    struct buf *bp = NULL;

	lck_mtx_lock(pmp->pm_fat_lock);

    /*
     * Don't try to extend the root directory
     */
    if (dep->de_StartCluster == MSDOSFSROOT
        && (dep->de_Attributes & ATTR_DIRECTORY))
    {
    	error = ENOSPC;
    	goto exit;
    }

    /*
     * If the "file's last cluster" cache entry is empty, and the file
     * is not empty, then fill the cache entry by calling pcbmap().
     */
    fc_fileextends++;
    if (dep->de_fc[FC_LASTFC].fc_frcn == FCE_EMPTY &&
        dep->de_StartCluster != 0)
    {
        fc_lfcempty++;
        error = pcbmap_internal(dep, 0xffff, 1, NULL, &cn, NULL, context);
        /* we expect it to return E2BIG */
        if (error != E2BIG)
            goto exit;
		error = 0;
    }

    reqcnt = count;
    while (count > 0) {
        /*
         * Allocate a new cluster chain and cat onto the end of the
         * file.  * If the file is empty we make de_StartCluster point
         * to the new block.  Note that de_StartCluster being 0 is
         * sufficient to be sure the file is empty since we exclude
         * attempts to extend the root directory above, and the root
         * dir is the only file with a startcluster of 0 that has
         * blocks allocated (sort of).
         */
        if (dep->de_StartCluster == 0)
            cn = 0;
        else
            cn = dep->de_fc[FC_LASTFC].fc_fsrcn + 1;
        error = clusteralloc_internal(pmp, cn, count, CLUST_EOFE, &cn, &got, context);
        if (error)
            goto exit;

        count -= got;

        if (dep->de_StartCluster == 0) {
            dep->de_StartCluster = cn;
            frcn = 0;
        } else {
            error = fatentry_internal(FAT_SET, pmp,
                             dep->de_fc[FC_LASTFC].fc_fsrcn,
                             0, cn, context);
            if (error) {
                clusterfree(pmp, cn, NULL, context);
                goto exit;
            }
            frcn = dep->de_fc[FC_LASTFC].fc_frcn + 1;
        }

        /*
         * Update the "last cluster of the file" entry in the denode's fat
         * cache.
         */
        fc_setcache(dep, FC_LASTFC, frcn + got - 1, cn + got - 1);

        /*
         * Clear directory clusters here, file clusters are cleared by the caller
         */
        if (dep->de_Attributes & ATTR_DIRECTORY) {
            while (got-- > 0) {
                /*
                 * Get the buf header for the new block of the file.
                 */
                bp = buf_getblk(pmp->pm_devvp, cntobn(pmp, cn++),
					pmp->pm_bpcluster, 0, 0, BLK_META);
                buf_clear(bp);
				buf_bwrite(bp);
            }
        }

    }

exit:
	lck_mtx_unlock(pmp->pm_fat_lock);
    return error;
}



/* [2753891]
 * Routine to mark a FAT16 or FAT32 volume as "clean" or "dirty" by manipulating the upper bit
 * of the FAT entry for cluster 1.  Note that this bit is not defined for FAT12 volumes, which
 * are always assumed to be dirty.
 *
 * The fatentry() routine only works on cluster numbers that a file could occupy, so it won't
 * manipulate the entry for cluster 1.  So we have to do it here.  The code is ripped from
 * fatentry(), and tailored for cluster 1.
 * 
 * Inputs:
 *	pmp	The MS-DOS volume to mark
 *	dirty	Non-zero if the volume should be marked dirty; zero if it should be marked clean.
 *
 * Result:
 *	0	Success
 *	EROFS	Volume is read-only
 *	?	(other errors from called routines)
 */
__private_extern__ int markvoldirty(struct msdosfsmount *pmp, int dirty, vfs_context_t context)
{
    int error=0;
    daddr64_t bn;
    u_long bo, bsize, byteoffset;
    u_long fatval;
    struct buf *bp=NULL;
    char *bdata;

    /* FAT12 does not support a "clean" bit, so don't do anything */
    if (FAT12(pmp))
        return 0;

    /* Can't change the bit on a read-only filesystem */
    if (pmp->pm_flags & MSDOSFSMNT_RONLY)
        return EROFS;

	lck_mtx_lock(pmp->pm_fat_lock);
	
    /* Fetch the block containing the FAT entry */
    byteoffset = FATOFS(pmp, 1);	/* Find the location of cluster 1 */
    fatblock(pmp, byteoffset, &bn, &bsize, &bo);
    error = (int)buf_meta_bread(pmp->pm_devvp, bn, bsize, vfs_context_ucred(context), &bp);
    if (error) {
            buf_brelse(bp);
            goto exit;
    }
    bdata = (char *)buf_dataptr(bp);

    /* Get the current value of the FAT entry and set/clear the high bit */
    if (FAT32(pmp)) {
        /* FAT32 uses bit 27 */
        fatval = getulong(&bdata[bo]);
        if (dirty)
            fatval &= 0xF7FFFFFF;	/* dirty means clear the "clean" bit */
        else
            fatval |= 0x08000000;	/* clean means set the "clean" bit */
        putulong(&bdata[bo], fatval);
    }
    else {
        /* Must be FAT16; use bit 15 */
        fatval = getushort(&bdata[bo]);
        if (dirty)
            fatval &= 0x7FFF;		/* dirty means clear the "clean" bit */
        else
            fatval |= 0x8000;		/* clean means set the "clean" bit */
        putushort(&bdata[bo], fatval);
    }
    
    /* Write out the modified FAT block immediately */
    error = buf_bwrite(bp);

exit:
	lck_mtx_unlock(pmp->pm_fat_lock);
	return error;
}

