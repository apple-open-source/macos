/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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
#include <mach/boolean.h>
#include <libkern/OSBase.h>
#include <libkern/OSAtomic.h>
#include <kern/thread_call.h>
#include <libkern/OSMalloc.h>
#include <IOKit/IOTypes.h>

/*
 * msdosfs include files.
 */
#include "bpb.h"
#include "msdosfsmount.h"
#include "direntry.h"
#include "denode.h"
#include "fat.h"

uint32_t msdosfs_meta_delay = 50;	/* in milliseconds */

#ifndef DEBUG
#define DEBUG 0
#endif

/*
 * The fs private data used with FAT vnodes.
 */
struct msdosfs_fat_node {
    struct msdosfsmount *pmp;
    u_int32_t start_block;	/* device block number of first sector */
    u_int32_t file_size;	/* number of bytes covered by this vnode */
};

/*
 * Internal routines
 */
static int msdosfs_fat_cache_flush(struct msdosfsmount *pmp, int ioflags);
static int clusterfree_internal(struct msdosfsmount *pmp,
				uint32_t cluster, uint32_t *oldcnp);
static int fatentry_internal(int function, struct msdosfsmount *pmp,
				uint32_t cn, uint32_t *oldcontents, uint32_t newcontents);
static int clusteralloc_internal(struct msdosfsmount *pmp,
				uint32_t start, uint32_t count, uint32_t fillwith,
				uint32_t *retcluster, uint32_t *got);
static int	chainalloc __P((struct msdosfsmount *pmp, uint32_t start,
				uint32_t count, uint32_t fillwith,
				uint32_t *retcluster, uint32_t *got));
static int	chainlength __P((struct msdosfsmount *pmp, uint32_t start,
				uint32_t count));
static int	fatchain __P((struct msdosfsmount *pmp, uint32_t start,
				uint32_t count, uint32_t fillwith));
static int fillinusemap(struct msdosfsmount *pmp);
static __inline void
		usemap_alloc __P((struct msdosfsmount *pmp, uint32_t cn));
static __inline void
		usemap_free __P((struct msdosfsmount *pmp, uint32_t cn));

/*
 * vnode operations for the FAT vnode
 */
typedef int     vnop_t __P((void *));
static int msdosfs_fat_pagein(struct vnop_pagein_args *ap)
{
    struct msdosfs_fat_node *node = vnode_fsnode(ap->a_vp);
    
    return cluster_pagein(ap->a_vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
	ap->a_size, node->file_size, ap->a_flags);
}

static int msdosfs_fat_pageout(struct vnop_pageout_args *ap)
{
    struct msdosfs_fat_node *node = vnode_fsnode(ap->a_vp);
    
    return cluster_pageout(ap->a_vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
	ap->a_size, node->file_size, ap->a_flags);
}

static int msdosfs_fat_strategy(struct vnop_strategy_args *ap)
{
    struct msdosfs_fat_node *node = vnode_fsnode(buf_vnode(ap->a_bp));
    
    return buf_strategy(node->pmp->pm_devvp, ap);
}

static int msdosfs_fat_blockmap(struct vnop_blockmap_args *ap)
{
    struct msdosfs_fat_node *node = vnode_fsnode(ap->a_vp);
    
    if (ap->a_bpn == NULL)
	return 0;

    /*
     * Return the physical sector number corresponding to ap->a_foffset,
     * where a_foffset is relative to the start of the first copy of the FAT.
     */
    *ap->a_bpn = node->start_block + (ap->a_foffset / node->pmp->pm_BlockSize);
    
    /* Return the number of contiguous bytes, up to ap->a_size */
    if (ap->a_run)
    {
	if (ap->a_foffset + ap->a_size > node->file_size)
	    *ap->a_run = node->file_size - ap->a_foffset;
	else
	    *ap->a_run = ap->a_size;
    }
    
    return 0;
}

static int msdosfs_fat_fsync(struct vnop_fsync_args *ap)
{
    int error = 0;
    int ioflags = 0;
    vnode_t vp = ap->a_vp;
    struct msdosfs_fat_node *node = vnode_fsnode(ap->a_vp);
    struct msdosfsmount *pmp = node->pmp;
    
    if (ap->a_waitfor == MNT_WAIT)
	ioflags = IO_SYNC;

    if (pmp->pm_fat_flags & FAT_CACHE_DIRTY)
    {
	lck_mtx_lock(pmp->pm_fat_lock);
	error = msdosfs_fat_cache_flush(pmp, ioflags);
 	lck_mtx_unlock(pmp->pm_fat_lock);
    }
    if (!error)
	cluster_push(vp, ioflags);

    return error;
}

static int msdosfs_fat_inactive(struct vnop_inactive_args *ap)
{
    vnode_recycle(ap->a_vp);
    return 0;
}

static int msdosfs_fat_reclaim(struct vnop_reclaim_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct msdosfs_fat_node *node = vnode_fsnode(vp);

    vnode_clearfsnode(vp);
    OSFree(node, sizeof(*node), msdosfs_malloc_tag);
    return 0;
}

static int (**msdosfs_fat_vnodeop_p)(void *);
static struct vnodeopv_entry_desc msdosfs_fat_vnodeop_entries[] = {
    { &vnop_default_desc,   (vnop_t *) vn_default_error },
    { &vnop_pagein_desc,    (vnop_t *) msdosfs_fat_pagein },
    { &vnop_pageout_desc,   (vnop_t *) msdosfs_fat_pageout },
    { &vnop_strategy_desc,  (vnop_t *) msdosfs_fat_strategy },
    { &vnop_blockmap_desc,  (vnop_t *) msdosfs_fat_blockmap },
    { &vnop_blktooff_desc,  (vnop_t *) msdosfs_blktooff },
    { &vnop_offtoblk_desc,  (vnop_t *) msdosfs_offtoblk },
    { &vnop_fsync_desc,	    (vnop_t *) msdosfs_fat_fsync },
    { &vnop_inactive_desc,  (vnop_t *) msdosfs_fat_inactive },
    { &vnop_reclaim_desc,   (vnop_t *) msdosfs_fat_reclaim },
    { &vnop_bwrite_desc,    (vnop_t *) vn_bwrite },
    { NULL, NULL }
};

__private_extern__
struct vnodeopv_desc msdosfs_fat_vnodeop_opv_desc =
	{ &msdosfs_fat_vnodeop_p, msdosfs_fat_vnodeop_entries };

__private_extern__ int
msdosfs_fat_init_vol(struct msdosfsmount *pmp)
{	
    errno_t error;
    struct vnode_fsparam vfsp;
    struct msdosfs_fat_node *node;

    lck_mtx_lock(pmp->pm_fat_lock);
    
    /*
     * Create a vnode to use to access the active FAT.
     */
    node = OSMalloc(sizeof(*node), msdosfs_malloc_tag);
    if (node == NULL)
    {
    	error = ENOMEM;
	goto exit;
    }
    node->pmp = pmp;
    node->start_block = pmp->pm_ResSectors * pmp->pm_BlocksPerSec;
    node->file_size = pmp->pm_fat_bytes;
    vfsp.vnfs_mp = pmp->pm_mountp;
    vfsp.vnfs_vtype = VREG;
    vfsp.vnfs_str = "msdosfs";
    vfsp.vnfs_dvp = NULL;
    vfsp.vnfs_fsnode = node;
    vfsp.vnfs_vops = msdosfs_fat_vnodeop_p;
    vfsp.vnfs_markroot = 0;
    vfsp.vnfs_marksystem = 1;
    vfsp.vnfs_rdev = 0;
    vfsp.vnfs_filesize = pmp->pm_fat_bytes;
    vfsp.vnfs_cnp = NULL;
    vfsp.vnfs_flags = VNFS_CANTCACHE;
    error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &pmp->pm_fat_active_vp);
    if (error)
    {
        OSFree(node, sizeof(*node), msdosfs_malloc_tag);
	goto exit;
    }
    vnode_ref(pmp->pm_fat_active_vp);
    vnode_put(pmp->pm_fat_active_vp);
    
    if (pmp->pm_flags & MSDOSFS_FATMIRROR)
    {
	/*
	 * Create a vnode to use to access the alternate FAT.
	 */
        node = OSMalloc(sizeof(*node), msdosfs_malloc_tag);
        if (node == NULL)
	{
	    error = ENOMEM;
	    goto exit;
	}
        node->pmp = pmp;
        node->start_block = pmp->pm_ResSectors * pmp->pm_BlocksPerSec + pmp->pm_fat_bytes / pmp->pm_BlockSize;
        node->file_size = (pmp->pm_FATs - 1) * pmp->pm_fat_bytes;
        vfsp.vnfs_mp = pmp->pm_mountp;
        vfsp.vnfs_vtype = VREG;
        vfsp.vnfs_str = "msdosfs";
        vfsp.vnfs_dvp = NULL;
        vfsp.vnfs_fsnode = node;
        vfsp.vnfs_vops = msdosfs_fat_vnodeop_p;
        vfsp.vnfs_markroot = 0;
        vfsp.vnfs_marksystem = 1;
        vfsp.vnfs_rdev = 0;
        vfsp.vnfs_filesize = node->file_size;
        vfsp.vnfs_cnp = NULL;
        vfsp.vnfs_flags = VNFS_CANTCACHE;
        error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &pmp->pm_fat_mirror_vp);
        if (error)
        {
	    OSFree(node, sizeof(*node), msdosfs_malloc_tag);
	    goto exit;
        }
        vnode_ref(pmp->pm_fat_mirror_vp);
        vnode_put(pmp->pm_fat_mirror_vp);
    }
    
    /*
     * Initialize the cache for most recently used FAT block.
     */
    pmp->pm_fat_cache = OSMalloc(pmp->pm_fatblocksize, msdosfs_malloc_tag);
    if (pmp->pm_fat_cache == NULL)
    {
    	error = ENOMEM;
	goto exit;
    }
    pmp->pm_fat_cache_offset = -1;	/* invalid; no cached content */
    pmp->pm_fat_flags = 0;		/* Nothing is dirty yet */
    
    /*
     * Allocate memory for the bitmap of allocated clusters, and then
     * fill it in.
     *
     * Note: pm_maxcluster is the maximum valid cluster number, so the
     * bitmap is actually at least pm_maxcluster+1 bits.  That's why you
     * don't see the typical round-up form:
     *     (x + FACTOR - 1) / FACTOR
     * Instead, (x + FACTOR) / FACTOR simplifies to (x / FACTOR) + 1.
     */
    MALLOC(pmp->pm_inusemap, u_int *,
       ((pmp->pm_maxcluster / N_INUSEBITS) + 1) *
	sizeof(u_int), M_TEMP, M_WAITOK);

    if (pmp->pm_inusemap == NULL)
    {
	error = ENOMEM;	/* Locks are cleaned up in msdosfs_fat_uninit_vol */
	goto exit;
    }

    error = fillinusemap(pmp);

exit:
    /*
     * NOTE: All memory allocated above gets freed in msdosfs_fat_uninit_vol.
     */
    lck_mtx_unlock(pmp->pm_fat_lock);
    return error;
}

__private_extern__ void
msdosfs_fat_uninit_vol(struct msdosfsmount *pmp)
{
    /*
     * Try one last time to flush the FAT before we get rid of the FAT vnodes.
     *
     * During a force unmount, the FAT may be dirty, and the device already
     * gone.  In that case, the flush will fail, but we must mark the FAT as
     * no longer dirty.  If we don't, releasing the vnodes below will again
     * try to flush the FAT, and will panic due to a NULL vnode pointer.
     */
    if (pmp->pm_fat_flags & FAT_CACHE_DIRTY)
    {
	int error;
	lck_mtx_lock(pmp->pm_fat_lock);
	error = msdosfs_fat_cache_flush(pmp, IO_SYNC);
	if (error)
	    printf("msdosfs_fat_uninit_vol: error %d from msdosfs_fat_cache_flush\n", error);
	pmp->pm_fat_flags = 0;	/* Ignore anything dirty or in need of update */
 	lck_mtx_unlock(pmp->pm_fat_lock);
    }

    if (pmp->pm_fat_active_vp)
    {
	/* Would it be better to push async, then wait for writes to complete? */
	cluster_push(pmp->pm_fat_active_vp, IO_SYNC);
	vnode_recycle(pmp->pm_fat_active_vp);
	vnode_rele(pmp->pm_fat_active_vp);
	pmp->pm_fat_active_vp = NULL;
    }
    
    if (pmp->pm_fat_mirror_vp)
    {
	cluster_push(pmp->pm_fat_mirror_vp, IO_SYNC);
	vnode_recycle(pmp->pm_fat_mirror_vp);
	vnode_rele(pmp->pm_fat_mirror_vp);
	pmp->pm_fat_mirror_vp = NULL;
    }
    
    if (pmp->pm_fat_cache)
    {
	OSFree(pmp->pm_fat_cache, pmp->pm_fatblocksize, msdosfs_malloc_tag);
	pmp->pm_fat_cache = NULL;
    }
	
    if (pmp->pm_inusemap)
    {
	FREE(pmp->pm_inusemap, M_TEMP);
	pmp->pm_inusemap = NULL;
    }
}



/*
 * Update the free cluster count and next free cluster number in the FSInfo
 * sector of a FAT32 volume.  This is called during VFS_SYNC.
 */
__private_extern__
int msdosfs_update_fsinfo(struct msdosfsmount *pmp, int waitfor, vfs_context_t context)
{
    int error;
    buf_t bp;
    struct fsinfo *fp;

    /*
     * If we don't have an FSInfo sector, or if there has been no change
     * to the FAT since the last FSInfo update, then there's nothing to do.
     */
    if (pmp->pm_fsinfo_size == 0 || !(pmp->pm_fat_flags & FSINFO_DIRTY))
	return 0;

    error = buf_meta_bread(pmp->pm_devvp, pmp->pm_fsinfo_sector, pmp->pm_fsinfo_size,
		vfs_context_ucred(context), &bp);
    if (error)
    {
	/*
	 * Turn off FSInfo update for the future.
	 */
	printf("msdosfs_update_fsinfo: error %d reading FSInfo\n", error);
	pmp->pm_fsinfo_size = 0;
	buf_brelse(bp);
    }
    else
    {
	fp = (struct fsinfo *) (buf_dataptr(bp) + pmp->pm_fsinfo_offset);
	
	putuint32(fp->fsinfree, pmp->pm_freeclustercount);
	/* If we ever start using pmp->pm_nxtfree, then we should update it on disk: */
	/* putuint32(fp->fsinxtfree, pmp->pm_nxtfree); */
	if (waitfor || pmp->pm_flags & MSDOSFSMNT_WAITONFAT)
	    error = buf_bwrite(bp);
	else
	    error = buf_bawrite(bp);
    }
    
    pmp->pm_fat_flags &= ~FSINFO_DIRTY;
    return error;
}


/*
 * If the FAT cache is dirty, then write it back via Cluster I/O.
 *
 * NOTE: Cluster I/O will typically cache this write in UBC, so the write
 * will not necessarily go to disk right away.  You must explicitly fsync
 * the vnode(s) to force changes to the media.
 */
static int
msdosfs_fat_cache_flush(struct msdosfsmount *pmp, int ioflags)
{
    int error;
    u_int32_t fat_file_size;
    u_int32_t block_size;
    uio_t uio;
    
    if (DEBUG) lck_mtx_assert(pmp->pm_fat_lock, LCK_MTX_ASSERT_OWNED);

    if ((pmp->pm_fat_flags & FAT_CACHE_DIRTY) == 0)
	return 0;

    fat_file_size = pmp->pm_fat_bytes;
    block_size = pmp->pm_fatblocksize;
    if (pmp->pm_fat_cache_offset + block_size > pmp->pm_fat_bytes)
    	block_size = pmp->pm_fat_bytes - pmp->pm_fat_cache_offset;
    
    uio = uio_create(1, pmp->pm_fat_cache_offset, UIO_SYSSPACE, UIO_WRITE);
    if (uio == NULL)
	return ENOMEM;
    uio_addiov(uio, CAST_USER_ADDR_T(pmp->pm_fat_cache), block_size);
    error = cluster_write(pmp->pm_fat_active_vp, uio, fat_file_size, fat_file_size, 0, 0, ioflags);
    if (!error)
	pmp->pm_fat_flags &= ~FAT_CACHE_DIRTY;

    /*
     * If we're mirroring the FAT, then write to all of the other copies now.
     * By definition, mirroring means that pmp->pm_curfat == 0, so we just
     * need to loop over 1 .. maximum here.
     */
    if (pmp->pm_flags & MSDOSFS_FATMIRROR)
    {
	int i;
	
	fat_file_size = (pmp->pm_FATs - 1) * pmp->pm_fat_bytes;
	for (i=0; i<pmp->pm_FATs-1; ++i)
	{
	    uio_reset(uio, pmp->pm_fat_cache_offset + i * pmp->pm_fat_bytes, UIO_SYSSPACE, UIO_WRITE);
	    uio_addiov(uio, CAST_USER_ADDR_T(pmp->pm_fat_cache), block_size);
	    error = cluster_write(pmp->pm_fat_mirror_vp, uio, fat_file_size, fat_file_size, 0, 0, ioflags);
	}
    }
    
    /* We're finally done with the uio */
    uio_free(uio);
    
    return error;
}


/*
 * Given a cluster number, bring the corresponding block of the FAT into
 * the pm_fat_cache (after writing a dirty cache block, if needed).  Returns
 * a pointer to the given cluster's entry.
 *
 * Inputs:
 *	pmp
 *	cn
 *
 * Outputs (optional):
 *	offp	Offset of entryp from start of FAT block, in bytes
 *	sizep	Number of valid bytes in this block.  For the last FAT block,
 *		this may be less than the block size (it will be the offset
 *		following the entry for the last cluster).
 *
 * Return value: Pointer to cluster cn's entry in the FAT block
 */
static void *
msdosfs_fat_map(struct msdosfsmount *pmp, u_int32_t cn, u_int32_t *offp, u_int32_t *sizep, int *error_out)
{
    int error = 0;
    u_int32_t offset;
    u_int32_t block_offset;
    u_int32_t block_size;
    
    if (DEBUG) lck_mtx_assert(pmp->pm_fat_lock, LCK_MTX_ASSERT_OWNED);
    
    offset = cn * pmp->pm_fatmult / pmp->pm_fatdiv;
    block_offset = offset / pmp->pm_fatblocksize * pmp->pm_fatblocksize;	/* Round to start of page */
    block_size = pmp->pm_fatblocksize;
    if (block_offset + block_size > pmp->pm_fat_bytes)
    	block_size = pmp->pm_fat_bytes - block_offset;

    if (pmp->pm_fat_cache_offset != block_offset)
    {
	uio_t uio;
	
    	if (pmp->pm_fat_flags & FAT_CACHE_DIRTY)
	    error = msdosfs_fat_cache_flush(pmp, 0);
	
	if (!error)
	{
	    pmp->pm_fat_cache_offset = block_offset;
	    uio = uio_create(1, block_offset, UIO_SYSSPACE, UIO_READ);
	    if (uio == NULL)
	    {
		error = ENOMEM;
	    }
	    else
	    {
		uio_addiov(uio, CAST_USER_ADDR_T(pmp->pm_fat_cache), block_size);
		error = cluster_read(pmp->pm_fat_active_vp, uio, pmp->pm_fat_bytes, 0);
		uio_free(uio);
	    }
	}
    }
    
    offset -= block_offset;
    
    if (offp)
	*offp = offset;

    if (sizep)
    {
	u_int32_t last_offset;
	
	/*
	 * If this is the last block of the FAT, then constrain the size to the
	 * end of the entry for the last cluster.
	 */
	last_offset = (pmp->pm_maxcluster + 1) * pmp->pm_fatmult / pmp->pm_fatdiv;
	if (last_offset - block_offset < block_size)
	    block_size = last_offset - block_offset;
    	*sizep = block_size;
    }

    if (error_out)
    	*error_out = error;

    if (error)
    {
	/* Invalidate the state of the FAT cache */
	pmp->pm_fat_cache_offset = -1;
	pmp->pm_fat_flags &= ~FAT_CACHE_DIRTY;
	return NULL;
    }

    return (char *) pmp->pm_fat_cache + offset;
}


/*
 * Flush the primary/active copy of the FAT, and all directory blocks.
 * This skips the boot sector, FSInfo sector, and non-active copies
 * of the FAT.
 * 
 * If sync is non-zero, the data is flushed syncrhonously.
 */
static void msdosfs_meta_flush_internal(struct msdosfsmount *pmp, int sync)
{
    int error;
    
    /*
     * Push out any changes to the FAT.  First, any dirty data in the FAT
     * cache gets written to the FAT vnodes, then the vnodes get pushed to
     * disk.
     *
     * We take the FAT lock here to delay other threads that may be trying
     * to use the FAT, and thus delay them from activating the flush timer.
     * We also pass IO_SYNC to cluster_push to make sure the push has completed
     * before we return.
     *
     * Otherwise, we get into a vicious cycle where one change is being flushed,
     * another change starts the flush timer, and a third change is blocked
     * waiting for the first change's writes to complete.  Meanwhile, the
     * flush timer fires, starting a new flush.  At this point, every change
     * results in its own flush, effectively becoming synchronous.
     */
    lck_mtx_lock(pmp->pm_fat_lock);
    if (pmp->pm_fat_flags & FAT_CACHE_DIRTY)
    {
    	error = msdosfs_fat_cache_flush(pmp, 0);
    	if (error)
    	{
	    printf("msdosfs_meta_flush_internal: error %d flushing FAT cache!\n", error);
	    pmp->pm_fat_flags &= ~FAT_CACHE_DIRTY;
	}
    }
    cluster_push(pmp->pm_fat_active_vp, IO_SYNC);
    if (pmp->pm_fat_mirror_vp)
	cluster_push(pmp->pm_fat_mirror_vp, IO_SYNC);
    lck_mtx_unlock(pmp->pm_fat_lock);
    
    /*
     * Push out any changes to directory blocks.
     */
    buf_flushdirtyblks(pmp->pm_devvp, sync, 0, "msdosfs_meta_flush");
}

__private_extern__ void
msdosfs_meta_flush(struct msdosfsmount *pmp, int sync)
{
    if (sync) goto flush_now;

    /*
     * If the volume was mounted async, and we're not being told to make
     * this flush synchronously, then ignore it.
     */
    if (vfs_flags(pmp->pm_mountp) & MNT_ASYNC)
	return;

    /*
     * If we have a timer, but it has not been scheduled yet, then schedule
     * it now.  It is possible that some or all of the metadata changed by the
     * caller has already been written out.  We're merely guaranteeing that
     * there will be a future metadata flush.
     *
     * If multiple threads are racing into this routine, they could all end
     * up (re)scheduling the timer.  The first thread to schedule the timer
     * ends up incrementing both pm_sync_scheduled and pm_sync_incomplete.
     * Any other thread will both increment and decrement pm_sync_scheduled,
     * leaving it unchanged in the end (but greater than 1 for a brief time).
     *
     * NOTE: We can't rely on just a single counter (pm_sync_incomplete) because
     * it doesn't get decremented until the callback has completed, so it
     * could be non-zero if the flush has already begun and the iterator has
     * already passed the newly dirtied blocks.  (And unmount definitely needs
     * to know when the last callback is *complete*, not just started.)
     */
    if (pmp->pm_sync_timer)
    {
    	if (pmp->pm_sync_scheduled == 0)
    	{
	    AbsoluteTime deadline;

	    clock_interval_to_deadline(msdosfs_meta_delay, kMillisecondScale, &deadline);

	    /*
	     * Increment pm_sync_scheduled on the assumption that we're the
	     * first thread to schedule the timer.  If some other thread beat
	     * us, then we'll decrement it.  If we *were* the first to
	     * schedule the timer, then we need to keep track that the
	     * callback is waiting to complete.
	     */
	    OSIncrementAtomic(&pmp->pm_sync_scheduled);
	    if (thread_call_enter_delayed(pmp->pm_sync_timer, deadline))
		OSDecrementAtomic(&pmp->pm_sync_scheduled);
	    else
		OSIncrementAtomic(&pmp->pm_sync_incomplete);
    	}

	return;
    }

flush_now:
    msdosfs_meta_flush_internal(pmp, sync);
}


__private_extern__ void msdosfs_meta_sync_callback(void *arg0, void *unused)
{
#pragma unused(unused)
    struct msdosfsmount *pmp = arg0;
    
    OSDecrementAtomic(&pmp->pm_sync_scheduled);
    msdosfs_meta_flush_internal(pmp, 0);
    OSDecrementAtomic(&pmp->pm_sync_incomplete);
    wakeup(&pmp->pm_sync_incomplete);
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
 * sp	  - address of where to place the number of contiguous bytes.
 *		If this pointer is null then don't return this quantity.
 *
 * NOTE: Either bnp or cnp must be non-null.
 *
 * This function has one side effect.  If the requested file relative cluster
 * is beyond the end of file, then the actual number of clusters in the file
 * is returned in *cnp, and the file's last cluster number is returned in *sp.
 * This is useful for determining how long a directory is, or for initializing
 * the de_LastCluster field.
 */
__private_extern__ int
pcbmap_internal(
    struct denode *dep,
    uint32_t findcn,	    /* file relative cluster to get */
    uint32_t numclusters,	    /* maximum number of contiguous clusters to map */
    daddr64_t *bnp,	    /* returned filesys relative blk number	 */
    uint32_t *cnp,	    /* returned cluster number */
    uint32_t *sp)		    /* number of contiguous bytes */
{
    int error=0;
    uint32_t i;			/* A temporary */
    uint32_t cn;			/* The current cluster number being examined */
    uint32_t prevcn=0;		/* The cluster previous to cn */
    uint32_t cluster_logical;	/* First file-relative cluster in extent */
    uint32_t cluster_physical;	/* First volume-relative cluster in extent */
    uint32_t cluster_count;	/* Number of clusters in extent */
    struct msdosfsmount *pmp = dep->de_pmp;
    void *entry;
    
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
     * If the requested position is within the currently cached extent,
     * we can return all of the information without walking the cluster
     * chain.
     */
    if (findcn >= dep->de_cluster_logical &&
	findcn < (dep->de_cluster_logical + dep->de_cluster_count))
    {
	i = findcn - dep->de_cluster_logical;	/* # clusters into cached extent */
	cn = dep->de_cluster_physical + i;	/* Starting cluster number */
	
	if (bnp)
	    *bnp = cntobn(pmp, cn);
	if (cnp)
	    *cnp = cn;
	if (sp)
	    *sp = min(dep->de_cluster_count - i, numclusters) * pmp->pm_bpcluster;
	
	goto exit;
    }
    
    /* Default to scanning from the beginning of the chain. */
    cluster_logical = 0;
    cluster_physical = 0;
    cluster_count = 0;
    /* From above: cn = dep->de_StartCluster */

    /*
     * If the requested position is after the cached extent, then start scanning
     * from the last cluster of the cached extent.  We set up the state
     * (cluster_xxx and cn) as if the loop below had just finished scanning
     * the cached extent, with cn being the first cluster of the next extent.
     */
    if (dep->de_cluster_count && findcn > dep->de_cluster_logical)
    {
    	cluster_logical = dep->de_cluster_logical;
    	cluster_physical = dep->de_cluster_physical;
    	cluster_count = dep->de_cluster_count;
    	prevcn = dep->de_cluster_physical+dep->de_cluster_count-1;
    	error = fatentry_internal(FAT_GET, pmp, prevcn, &cn, 0);
	if (error)
	    goto exit;
    }

    /*
     * Iterate through the extents (runs of contiguous clusters) in the file
     * until we find the extent containing findcn, or we hit end of file.
     */
    while ((cluster_logical + cluster_count) <= findcn)
    {
	/*
	 * Stop with all reserved clusters, not just with EOF.
	 */
	if ((cn | ~pmp->pm_fatmask) >= CLUST_RSRVD)
	    goto hiteof;
	
	/*
	 * Detect infinite cycles in the cluster chain.  (Generally only useful
	 * for directories, where findcn is (-1).)
	 */
	if (cluster_logical > pmp->pm_maxcluster)
	{
	    printf("msdosfs: pcbmap: Corrupt cluster chain detected\n");
	    pmp->pm_flags |= MSDOSFS_CORRUPT;
	    error = EIO;
	    goto exit;
	}
	
	/*
	 * Stop and return an error if we hit an out-of-range cluster number.
	 */
	if (cn < CLUST_FIRST || cn > pmp->pm_maxcluster)
	{
	    if (DEBUG)
		panic("pcbmap_internal: invalid cluster: cn=%u, name='%11.11s'", cn, dep->de_Name);
	    return EIO;
	}
	
	/*
	 * Keep track of the start of the extent.
	 *
	 * The count is initialized to zero because it will be incremented to
	 * account for cluster "cn" at the end of the do ... while loop.
	 */
	cluster_logical += cluster_count;   /* Skip past previous extent */
	cluster_physical = cn;
	cluster_count = 0;
	
	/*
	 * Loop over contiguous clusters starting with #cn.
	 */
	do {
	    /*
	     * Make sure we have the block containing the cn'th entry in the FAT.
	     */
	    entry = msdosfs_fat_map(pmp, cn, NULL, NULL, &error);
	    if (!entry)
		goto exit;
	    
	    /*
	     * Fetch the next cluster in the chain.
	     */
	    prevcn = cn;
	    if (FAT32(pmp))
		cn = getuint32(entry);
	    else
		cn = getuint16(entry);
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
	    
	    cluster_count++;
	} while (cn == prevcn + 1);
	
	/*
	 * Falling out of the do..while loop means that cn is the start of a
	 * new extent (or a reserved/EOF value).
	 */
    }
    
    /*
     * We have found the extent containing findcn.  Remember it, and return
     * the mapping information.
     */
    dep->de_cluster_logical = cluster_logical;
    dep->de_cluster_physical = cluster_physical;
    dep->de_cluster_count = cluster_count;
    
    i = findcn - cluster_logical;	/* # of clusters into found extent */
    cn = cluster_physical + i;	/* findcn'th physical (volume-relative) cluster */
    
    if (bnp)
	*bnp = cntobn(pmp, cn);
    if (cnp)
	*cnp = cn;
    if (sp)
	*sp = min(cluster_count-i, numclusters) * pmp->pm_bpcluster;

exit:
    return error;
    
hiteof:
    if (cnp)
	*cnp = cluster_logical + cluster_count;
    if (sp)
	*sp = prevcn;
    
    error = E2BIG;
    goto exit;
}


__private_extern__ int
pcbmap(
    struct denode *dep,
    uint32_t findcn,	    /* file relative cluster to get */
    uint32_t numclusters,	    /* maximum number of contiguous clusters to map */
    daddr64_t *bnp,	    /* returned filesys relative blk number	 */
    uint32_t *cnp,	    /* returned cluster number */
    uint32_t *sp)		    /* number of contiguous bytes */
{
    int error;
    
    lck_mtx_lock(dep->de_cluster_lock);
    lck_mtx_lock(dep->de_pmp->pm_fat_lock);
    error = pcbmap_internal(dep, findcn, numclusters, bnp, cnp, sp);
    lck_mtx_unlock(dep->de_pmp->pm_fat_lock);
    lck_mtx_unlock(dep->de_cluster_lock);
    
    return error;
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
	uint32_t cn;
{

	pmp->pm_inusemap[cn / N_INUSEBITS] |= 1 << (cn % N_INUSEBITS);
	pmp->pm_freeclustercount--;
}

static __inline void
usemap_free(pmp, cn)
	struct msdosfsmount *pmp;
	uint32_t cn;
{

	pmp->pm_freeclustercount++;
	pmp->pm_inusemap[cn / N_INUSEBITS] &= ~(1 << (cn % N_INUSEBITS));
}

static int
clusterfree_internal(pmp, cluster, oldcnp)
	struct msdosfsmount *pmp;
	uint32_t cluster;
	uint32_t *oldcnp;
{
	int error;
	uint32_t oldcn;

	usemap_free(pmp, cluster);
	error = fatentry_internal(FAT_GET_AND_SET, pmp, cluster, &oldcn, MSDOSFSFREE);
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
clusterfree(pmp, cluster, oldcnp)
	struct msdosfsmount *pmp;
	uint32_t cluster;
	uint32_t *oldcnp;
{
	int error;
	
	lck_mtx_lock(pmp->pm_fat_lock);
	error = clusterfree_internal(pmp, cluster, oldcnp);
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
fatentry_internal(function, pmp, cn, oldcontents, newcontents)
	int function;
	struct msdosfsmount *pmp;
	uint32_t cn;
	uint32_t *oldcontents;
	uint32_t newcontents;
{
	int error = 0;
	uint32_t readcn;
	void *entry;
	
	/*
	 * Be sure the requested cluster is in the filesystem.
	 */
	if (cn < CLUST_FIRST || cn > pmp->pm_maxcluster)
	{
	    if (DEBUG)
	    {
		printf("msdosfs: fatentry_internal: cn=%u, function=%d, new=%u\n", cn, function, newcontents);
	    }
	    return (EIO);
	}

	entry = msdosfs_fat_map(pmp, cn, NULL, NULL, &error);
	if (!entry)
	    return error;
	
	if (function & FAT_GET) {
		if (FAT32(pmp))
			readcn = getuint32(entry);
		else
			readcn = getuint16(entry);
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
			readcn = getuint16(entry);
			if (cn & 1) {
				readcn &= 0x000f;
				readcn |= newcontents << 4;
			} else {
				readcn &= 0xf000;
				readcn |= newcontents & 0xfff;
			}
			putuint16(entry, readcn);
			break;
		case FAT16_MASK:
			putuint16(entry, newcontents);
			break;
		case FAT32_MASK:
			/*
			 * According to spec we have to retain the
			 * high order bits of the fat entry.
			 */
			readcn = getuint32(entry);
			readcn &= ~FAT32_MASK;
			readcn |= newcontents & FAT32_MASK;
			putuint32(entry, readcn);
			break;
		}
		pmp->pm_fat_flags |= FAT_CACHE_DIRTY | FSINFO_DIRTY;
	}
	return 0;
}

__private_extern__ int
fatentry(function, pmp, cn, oldcontents, newcontents)
	int function;
	struct msdosfsmount *pmp;
	uint32_t cn;
	uint32_t *oldcontents;
	uint32_t newcontents;
{
	int error;
	
	lck_mtx_lock(pmp->pm_fat_lock);
	error = fatentry_internal(function, pmp, cn, oldcontents, newcontents);
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
fatchain(pmp, start, count, fillwith)
	struct msdosfsmount *pmp;
	uint32_t start;
	uint32_t count;
	uint32_t fillwith;
{
	int error = 0;
	u_int32_t bo, bsize;
	uint32_t readcn, newc;
	char *entry;
	char *block_end;

	/*
	 * Be sure the clusters are in the filesystem.
	 */
	if (start < CLUST_FIRST || start + count - 1 > pmp->pm_maxcluster)
	{
	    printf("msdosfs: fatchain: start=%u, count=%u, fill=%u\n", start, count, fillwith);
	    return (EIO);
	}

	/* Loop over all clusters in the chain */
	while (count > 0) {
		entry = msdosfs_fat_map(pmp, start, &bo, &bsize, &error);
		if (!entry)
		    break;
		
		block_end = entry - bo + bsize;
		
		/* Loop over all clusters in this FAT block */
		while (count > 0) {
			start++;
			newc = --count > 0 ? start : fillwith;
			switch (pmp->pm_fatmask) {
			case FAT12_MASK:
				readcn = getuint16(entry);
				if (start & 1) {
					readcn &= 0xf000;
					readcn |= newc & 0xfff;
				} else {
					readcn &= 0x000f;
					readcn |= newc << 4;
				}
				putuint16(entry, readcn);
				entry++;
				if (!(start & 1))
					entry++;
				break;
			case FAT16_MASK:
				putuint16(entry, newc);
				entry += 2;
				break;
			case FAT32_MASK:
				readcn = getuint32(entry);
				readcn &= ~pmp->pm_fatmask;
				readcn |= newc & pmp->pm_fatmask;
				putuint32(entry, readcn);
				entry += 4;
				break;
			}
			if (entry >= block_end)
				break;
		}
		pmp->pm_fat_flags |= FAT_CACHE_DIRTY | FSINFO_DIRTY;
	}
	return error;
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
	uint32_t start;
	uint32_t count;
{
	uint32_t idx, max_idx;
	u_int map;
	uint32_t len;

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
chainalloc(pmp, start, count, fillwith, retcluster, got)
	struct msdosfsmount *pmp;
	uint32_t start;
	uint32_t count;
	uint32_t fillwith;
	uint32_t *retcluster;
	uint32_t *got;
{
	int error;
	uint32_t cl, n;

	for (cl = start, n = count; n-- > 0;)
		usemap_alloc(pmp, cl++);

	error = fatchain(pmp, start, count, fillwith);
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
clusteralloc_internal(pmp, start, count, fillwith, retcluster, got)
	struct msdosfsmount *pmp;
	uint32_t start;
	uint32_t count;
	uint32_t fillwith;
	uint32_t *retcluster;
	uint32_t *got;
{
	uint32_t idx;
	uint32_t len, foundl, cn, l;
	uint32_t foundcn;
	u_int map;

	if (start) {
		/*
		 * If the caller had a specific starting cluster, then look there
		 * first.  (This happens when extending an existing file or directory.)
		 *
		 * If there are enough contiguous free clusters at "start", just use them.
		 */
		if ((len = chainlength(pmp, start, count)) >= count)
			return (chainalloc(pmp, start, count, fillwith, retcluster, got));
		
		/*
		 * If we fall through here, "len" = the number of contiguous free clusters
		 * starting at "start".
		 */
	} else 
		len = 0;

	/*
	 * "foundl" is the largest number of contiguous free clusters found so far.
	 * "foundcn" is the corresponding starting cluster.
	 */
	foundl = 0;
	foundcn = 0;
	
	/*
	 * Scan through the FAT (actually, the in-use map) for contiguous free
	 * clusters.
	 */
	for (cn = 0; cn <= pmp->pm_maxcluster;) {
		/*
		 * find the in-use map entry corresponding to cluster #cn
		 */
		idx = cn / N_INUSEBITS;
		map = pmp->pm_inusemap[idx];
		/* pretend prior clusters are in use */
		map |= (1 << (cn % N_INUSEBITS)) - 1;
		
		/* If there are free clusters in this use map entry, then ... */
		if (map != (u_int)-1) {
			/* Figure out the cluster number of the first free cluster. */
			cn = idx * N_INUSEBITS + ffs(map^(u_int)-1) - 1;
			
			/* See how many contiguous free clusters are there. */
			if ((l = chainlength(pmp, cn, count)) >= count)
				return (chainalloc(pmp, cn, count, fillwith, retcluster, got));
			
			/*
			 * If we get here, there were some free clusters, but not as many
			 * as we'd ideally like.  So keep track of the largest chain seen.
			 * If we never find a contiguous chain long enough, we'll at least
			 * return the largest chain we found.
			 */
			if (l > foundl) {
				foundcn = cn;
				foundl = l;
			}
			
			/* Skip past this free chain to look for the next free chain. */
			cn += l + 1;
			continue;
		}
		
		/*
		 * If we get here, the rest of the clusters in the current use map entry
		 * were all in use.  So just skip ahead to the first cluster of the
		 * next use map entry.
		 */
		cn += N_INUSEBITS - cn % N_INUSEBITS;
	}

	/*
	 * If we get here, there was no single contiguous chain as long as we
	 * wanted.  Check to see if we found *any* free chains.
	 */
	if (!foundl)
		return (ENOSPC);

	/*
	 * There was no single contiguous chain long enough.  If the caller passed
	 * a specific starting cluster, and there were free clusters there, then
	 * return that chain (under the assumption they're at least contiguous
	 * with the previous bit of the file -- which might result in fewer
	 * total extents).
	 *
	 * Otherwise, just return the largest free chain we found.
	 */
	if (len)
		return (chainalloc(pmp, start, len, fillwith, retcluster, got));
	else
		return (chainalloc(pmp, foundcn, foundl, fillwith, retcluster, got));
}

__private_extern__ int
clusteralloc(pmp, start, count, fillwith, retcluster, got)
	struct msdosfsmount *pmp;
	uint32_t start;
	uint32_t count;
	uint32_t fillwith;
	uint32_t *retcluster;
	uint32_t *got;
{
	int error;
	
	lck_mtx_lock(pmp->pm_fat_lock);
	error = clusteralloc_internal(pmp, start, count, fillwith, retcluster, got);
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
freeclusterchain(pmp, cluster)
	struct msdosfsmount *pmp;
	uint32_t cluster;
{
	int error=0;
	uint32_t readcn;
	void *entry;

	lck_mtx_lock(pmp->pm_fat_lock);

	while (cluster >= CLUST_FIRST && cluster <= pmp->pm_maxcluster) {
		entry = msdosfs_fat_map(pmp, cluster, NULL, NULL, &error);
		if (!entry)
		    break;
		
		usemap_free(pmp, cluster);
		switch (pmp->pm_fatmask) {
		case FAT12_MASK:
			readcn = getuint16(entry);
			if (cluster & 1) {
				cluster = readcn >> 4;
				readcn &= 0x000f;
				readcn |= MSDOSFSFREE << 4;
			} else {
				cluster = readcn;
				readcn &= 0xf000;
				readcn |= MSDOSFSFREE & 0xfff;
			}
			putuint16(entry, readcn);
			break;
		case FAT16_MASK:
			cluster = getuint16(entry);
			putuint16(entry, MSDOSFSFREE);
			break;
		case FAT32_MASK:
			cluster = getuint32(entry);
			putuint32(entry,
				 (MSDOSFSFREE & FAT32_MASK) | (cluster & ~FAT32_MASK));
			break;
		}
		pmp->pm_fat_flags |= FAT_CACHE_DIRTY | FSINFO_DIRTY;
		cluster &= pmp->pm_fatmask;
		if ((cluster | ~pmp->pm_fatmask) >= CLUST_RSRVD)
			cluster |= pmp->pm_fatmask;
	}
	lck_mtx_unlock(pmp->pm_fat_lock);
	return error;
}

/*
 * Read in fat blocks looking for free clusters. For every free cluster
 * found turn off its corresponding bit in the pm_inusemap.
 */
static int
fillinusemap(struct msdosfsmount *pmp)
{
    uint32_t cn, readcn=0;
    int error = 0;
    char *entry, *last_entry;
    u_int32_t offset, block_size;
    
    /*
     * We're going to be reading the entire FAT, so advise Cluster I/O
     * that it should begin the read now.
     */
    (void) advisory_read(pmp->pm_fat_active_vp, pmp->pm_fat_bytes,
	0, pmp->pm_fat_bytes);

    /*
     * Mark all clusters in use.  We mark the free ones in the fat scan
     * loop further down.
     *
     * Note: pm_maxcluster is the maximum valid cluster number, thus the
     * maximum index into the bitmap is pm_maxcluster / N_INUSEBITS.  The
     * third (length) parameter to memset is the same as the value used
     * to MALLOC the in-use map.
     */
    memset(pmp->pm_inusemap, 0xFF, ((pmp->pm_maxcluster / N_INUSEBITS) + 1) * sizeof(u_int));

    /*
     * Figure how many free clusters are in the filesystem by ripping
     * through the fat counting the number of entries whose content is
     * zero.  These represent free clusters.
     */
    pmp->pm_freeclustercount = 0;
    for (cn = CLUST_FIRST; cn <= pmp->pm_maxcluster; ) {
	entry = msdosfs_fat_map(pmp, cn, &offset, &block_size, &error);
	if (!entry)
	    break;
	
	/* Get a pointer past the last valid entry in the block */
	last_entry = entry - offset + block_size;
	
	while (entry < last_entry && cn <= pmp->pm_maxcluster)
	{
	    switch (pmp->pm_fatmask)
	    {
	    case FAT12_MASK:
		readcn = getuint16(entry);
		if (cn & 1)
		{
		    readcn >>= 4;
		    entry += 2;
		}
		else
		{
		    readcn &= FAT12_MASK;
		    entry++;
		}
		break;
	    case FAT16_MASK:
		readcn = getuint16(entry);
		entry += 2;
		break;
	    case FAT32_MASK:
		readcn = getuint32(entry);
		entry += 4;
		readcn &= FAT32_MASK;
		break;
	    }
    
	    if (readcn == 0)
		usemap_free(pmp, cn);
	    
	    cn++;
	}
    }
    
    return error;
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
extendfile(dep, count)
	struct denode *dep;
	uint32_t count;
{
    int error=0;
    uint32_t cn, got, reqcnt;
    struct msdosfsmount *pmp = dep->de_pmp;
    struct buf *bp = NULL;

    lck_mtx_lock(dep->de_cluster_lock);
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
     * If the "file's last cluster" is uninitialized, and the file
     * is not empty, then calculate the last cluster.
     */
    if (dep->de_LastCluster == 0 &&
        dep->de_StartCluster != 0)
    {
        error = pcbmap_internal(dep, 0xFFFFFFFF, 1, NULL, NULL, &dep->de_LastCluster);
        /* we expect it to return E2BIG */
        if (error != E2BIG)
            goto exit;
	error = 0;
	
	if (dep->de_LastCluster == 0)
	{
	    printf("msdosfs: extendfile: dep->de_LastCluster == 0!\n");
	    error = EIO;
	    goto exit;
	}
    }

    reqcnt = count;
    while (count > 0) {
        /*
         * Allocate a new cluster chain and cat onto the end of the
         * file.  If the file is empty we make de_StartCluster point
         * to the new block.  Note that de_StartCluster being 0 is
         * sufficient to be sure the file is empty since we exclude
         * attempts to extend the root directory above, and the root
         * dir is the only file with a startcluster of 0 that has
         * blocks allocated (sort of).
         */
        if (dep->de_StartCluster == 0)
            cn = 0;
        else
            cn = dep->de_LastCluster + 1;
        error = clusteralloc_internal(pmp, cn, count, CLUST_EOFE, &cn, &got);
        if (error)
            goto exit;

	/*
	 * If the first chain we allocated was contiguous with the previous end
	 * of file, or this is the file's first chain, then update the cluster
	 * extent cache.
	 */
	if (reqcnt == count)
	{
	    if (dep->de_StartCluster == 0)
	    {
		/*
		 * The file's first/only extent, so cache it.
		 */
		dep->de_cluster_logical = 0;
		dep->de_cluster_physical = cn;
		dep->de_cluster_count = got;
	    }
	    else if (cn == (dep->de_LastCluster+1) &&
		    cn == (dep->de_cluster_physical + dep->de_cluster_count))
	    {
		/*
		 * We extended the file's last extent, and it was cached, so
		 * update its length to include the new allocation.
		 */
		dep->de_cluster_count += got;
	    }
	}
	
        count -= got;

        if (dep->de_StartCluster == 0) {
            dep->de_StartCluster = cn;
        } else {
            error = fatentry_internal(FAT_SET, pmp,
                             dep->de_LastCluster,
                             0, cn);
            if (error) {
                clusterfree_internal(pmp, cn, NULL);
                goto exit;
            }
        }

        /*
         * Update the "last cluster of the file" entry in the denode.
         */
	dep->de_LastCluster = cn + got - 1;

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
		buf_bdwrite(bp);
            }
        }

    }

exit:
    lck_mtx_unlock(pmp->pm_fat_lock);
    lck_mtx_unlock(dep->de_cluster_lock);
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
__private_extern__ int markvoldirty(struct msdosfsmount *pmp, int dirty)
{
    int error=0;
    uint32_t fatval;
    void *entry;

    /* FAT12 does not support a "clean" bit, so don't do anything */
    if (FAT12(pmp))
        return 0;

    /* Can't change the bit on a read-only filesystem */
    if (pmp->pm_flags & MSDOSFSMNT_RONLY)
        return EROFS;

    lck_mtx_lock(pmp->pm_fat_lock);

    /* Fetch the block containing the FAT entry */
    entry = msdosfs_fat_map(pmp, 1, NULL, NULL, &error);
    if (!entry)
	goto done;
    
    /* Get the current value of the FAT entry and set/clear the high bit */
    if (FAT32(pmp)) {
        /* FAT32 uses bit 27 */
        fatval = getuint32(entry);
        if (dirty)
            fatval &= 0xF7FFFFFF;	/* dirty means clear the "clean" bit */
        else
            fatval |= 0x08000000;	/* clean means set the "clean" bit */
        putuint32(entry, fatval);
    }
    else {
        /* Must be FAT16; use bit 15 */
        fatval = getuint16(entry);
        if (dirty)
            fatval &= 0x7FFF;		/* dirty means clear the "clean" bit */
        else
            fatval |= 0x8000;		/* clean means set the "clean" bit */
        putuint16(entry, fatval);
    }
    
    /* Write out the modified FAT block immediately */
    pmp->pm_fat_flags |= FAT_CACHE_DIRTY;
    error = msdosfs_fat_cache_flush(pmp, IO_SYNC);

done:
    lck_mtx_unlock(pmp->pm_fat_lock);
    return error;
}

