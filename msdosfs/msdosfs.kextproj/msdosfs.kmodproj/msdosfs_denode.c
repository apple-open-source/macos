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
/* $FreeBSD: src/sys/msdosfs/msdosfs_denode.c,v 1.48 2000/05/05 09:58:34 phk Exp $ */
/*	$NetBSD: msdosfs_denode.c,v 1.28 1998/02/10 14:10:00 mrg Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/ubc.h>
#include <sys/namei.h>

#include "bpb.h"
#include "msdosfsmount.h"
#include "direntry.h"
#include "denode.h"
#include "fat.h"
#include <kern/zalloc.h>

zone_t  msdosfs_node_zone;

static struct denode **dehashtbl;
static u_long dehash;			/* size of hash table - 1 */
#define	DEHASH(dev, dcl, doff)	(dehashtbl[(minor(dev) + (dcl) + (doff) / 	\
				sizeof(struct direntry)) & dehash])
#ifndef NULL_SIMPLELOCKS
static simple_lock_data_t dehash_slock;
#endif

union _qcvt {
	quad_t qcvt;
	long val[2];
};
#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

extern int (**msdosfs_vnodeop_p)(void *);

static struct denode *
		msdosfs_hashget __P((dev_t dev, u_long dirclust,
				     u_long diroff));
static void	msdosfs_hashins __P((struct denode *dep));
static void	msdosfs_hashrem __P((struct denode *dep));

/*ARGSUSED*/
int 
msdosfs_init(vfsp)
	struct vfsconf *vfsp;
{

    msdosfs_node_zone = zinit (sizeof(struct denode), desiredvnodes/2 *sizeof(struct denode), 0, "msdos node zone");
    dehashtbl = hashinit(desiredvnodes/2, M_CACHE, &dehash);
	simple_lock_init(&dehash_slock);
	return (0);
}

static struct denode *
msdosfs_hashget(dev, dirclust, diroff)
	dev_t dev;
	u_long dirclust;
	u_long diroff;
{
	struct proc *p = current_proc();	/* XXX */
	struct denode *dep;
	struct vnode *vp;

loop:
	simple_lock(&dehash_slock);
	for (dep = DEHASH(dev, dirclust, diroff); dep; dep = dep->de_next) {
		if (dirclust == dep->de_dirclust
		    && diroff == dep->de_diroffset
		    && dev == dep->de_dev
		    && dep->de_refcnt != 0) {
			vp = DETOV(dep);
			simple_lock(&vp->v_interlock);
			simple_unlock(&dehash_slock);
			if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, p))
				goto loop;
			return (dep);
		}
	}
	simple_unlock(&dehash_slock);
	return (NULL);
}

static void
msdosfs_hashins(dep)
	struct denode *dep;
{
	struct denode **depp, *deq;

	simple_lock(&dehash_slock);
	depp = &DEHASH(dep->de_dev, dep->de_dirclust, dep->de_diroffset);
	deq = *depp;
	if (deq)
		deq->de_prev = &dep->de_next;
	dep->de_next = deq;
	dep->de_prev = depp;
	*depp = dep;
	simple_unlock(&dehash_slock);
}

static void
msdosfs_hashrem(dep)
	struct denode *dep;
{
	struct denode *deq;

	simple_lock(&dehash_slock);
	deq = dep->de_next;
	if (deq)
		deq->de_prev = dep->de_prev;
	*dep->de_prev = deq;
#ifdef DIAGNOSTIC
	dep->de_next = NULL;
	dep->de_prev = NULL;
#endif
	simple_unlock(&dehash_slock);
}

/*
 * If deget() succeeds it returns with the gotten denode locked().
 *
 * pmp	     - address of msdosfsmount structure of the filesystem containing
 *	       the denode of interest.  The pm_dev field and the address of
 *	       the msdosfsmount structure are used.
 * dirclust  - which cluster bp contains, if dirclust is 0 (root directory)
 *	       diroffset is relative to the beginning of the root directory,
 *	       otherwise it is cluster relative.
 * diroffset - offset past begin of cluster of denode we want
 * depp	     - returns the address of the gotten denode.
 */
int
deget(pmp, dirclust, diroffset, depp)
	struct msdosfsmount *pmp;	/* so we know the maj/min number */
	u_long dirclust;		/* cluster this dir entry came from */
	u_long diroffset;		/* index of entry within the cluster */
	struct denode **depp;		/* returns the addr of the gotten denode */
{
	int error;
	dev_t dev = pmp->pm_dev;
	struct mount *mntp = pmp->pm_mountp;
	struct direntry *direntptr;
	struct denode *ldep;
	struct vnode *nvp;
	struct buf *bp;
	struct proc *p = current_proc();	/* XXX */
	struct timeval tv;

#ifdef MSDOSFS_DEBUG
	printf("deget(pmp %p, dirclust %lu, diroffset %lx, depp %p)\n",
	    pmp, dirclust, diroffset, depp);
#endif

	/*
	 * On FAT32 filesystems, root is a (more or less) normal
	 * directory
	 */
	if (FAT32(pmp) && dirclust == MSDOSFSROOT)
		dirclust = pmp->pm_rootdirblk;

	/*
	 * See if the denode is in the denode cache. Use the location of
	 * the directory entry to compute the hash value. For subdir use
	 * address of "." entry. For root dir (if not FAT32) use cluster
	 * MSDOSFSROOT, offset MSDOSFSROOT_OFS
	 *
	 * NOTE: The check for de_refcnt > 0 below insures the denode being
	 * examined does not represent an unlinked but still open file.
	 * These files are not to be accessible even when the directory
	 * entry that represented the file happens to be reused while the
	 * deleted file is still open.
	 */
	ldep = msdosfs_hashget(dev, dirclust, diroffset);
	if (ldep) {
		*depp = ldep;
		return (0);
	}

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
    ldep = (struct denode *) zalloc(msdosfs_node_zone);

	/*
	 * Directory entry was not in cache, have to create a vnode and
	 * copy it from the passed disk buffer.
	 */
	/* getnewvnode() does a VREF() on the vnode */
	error = getnewvnode(VT_MSDOSFS, mntp, msdosfs_vnodeop_p, &nvp);
	if (error) {
		*depp = NULL;
        zfree(msdosfs_node_zone, (vm_offset_t) ldep);
		return error;
	}
	bzero((caddr_t)ldep, sizeof *ldep);
	lockinit(&ldep->de_lock, PINOD, "denode", 0, 0);
	nvp->v_data = ldep;
	ldep->de_vnode = nvp;
	ldep->de_flag = 0;
	ldep->de_devvp = 0;
	ldep->de_dev = dev;
	ldep->de_dirclust = dirclust;
	ldep->de_diroffset = diroffset;
	fc_purge(ldep, 0);	/* init the fat cache for this denode */

	/*
	 * Lock the denode so that it can't be accessed until we've read
	 * it in and have done what we need to it.  Do this here instead
	 * of at the start of msdosfs_hashins() so that reinsert() can
	 * call msdosfs_hashins() with a locked denode.
	 */
	if (lockmgr(&ldep->de_lock, LK_EXCLUSIVE, (simple_lock_data_t *)0, p))
		panic("deget: unexpected lock failure");

	/*
	 * Insert the denode into the hash queue.
	 */
	msdosfs_hashins(ldep);

	ldep->de_pmp = pmp;
	ldep->de_refcnt = 1;
	/*
	 * Copy the directory entry into the denode area of the vnode.
	 */
	if ((dirclust == MSDOSFSROOT
	     || (FAT32(pmp) && dirclust == pmp->pm_rootdirblk))
	    && diroffset == MSDOSFSROOT_OFS) {
		/*
		 * Directory entry for the root directory. There isn't one,
		 * so we manufacture one. We should probably rummage
		 * through the root directory and find a label entry (if it
		 * exists), and then use the time and date from that entry
		 * as the time and date for the root denode.
		 */
		nvp->v_flag |= VROOT; /* should be further down		XXX */

		ldep->de_Attributes = ATTR_DIRECTORY;
		ldep->de_LowerCase = 0;
		if (FAT32(pmp))
			ldep->de_StartCluster = pmp->pm_rootdirblk;
			/* de_FileSize will be filled in further down */
		else {
			ldep->de_StartCluster = MSDOSFSROOT;
			ldep->de_FileSize = pmp->pm_rootdirsize * pmp->pm_BlockSize;
		}
		/*
		 * fill in time and date so that dos2unixtime() doesn't
		 * spit up when called from msdosfs_getattr() with root
		 * denode
		 */
		ldep->de_CHun = 0;
		ldep->de_CTime = 0x0000;	/* 00:00:00	 */
		ldep->de_CDate = (0 << DD_YEAR_SHIFT) | (1 << DD_MONTH_SHIFT)
		    | (1 << DD_DAY_SHIFT);
		/* Jan 1, 1980	 */
		ldep->de_ADate = ldep->de_CDate;
		ldep->de_MTime = ldep->de_CTime;
		ldep->de_MDate = ldep->de_CDate;
		/* leave the other fields as garbage */
                
                /*
                 * If there is a volume label entry, then grab the times from it instead.
                 */
                if (pmp->pm_label_cluster != CLUST_EOFE) {
                    error = readep(pmp, pmp->pm_label_cluster, pmp->pm_label_offset,
                            &bp, &direntptr);
                    if (!error) {
                        ldep->de_CHun = direntptr->deCHundredth;
                        ldep->de_CTime = getushort(direntptr->deCTime);
                        ldep->de_CDate = getushort(direntptr->deCDate);
                        ldep->de_ADate = getushort(direntptr->deADate);
                        ldep->de_MTime = getushort(direntptr->deMTime);
                        ldep->de_MDate = getushort(direntptr->deMDate);
                        brelse(bp);
                    }
                }
	} else {
		error = readep(pmp, dirclust, diroffset, &bp, &direntptr);
		if (error) {
			/*
			 * The denode does not contain anything useful, so
			 * it would be wrong to leave it on its hash chain.
			 * Arrange for vput() to just forget about it.
			 */
			ldep->de_Name[0] = SLOT_DELETED;

			vput(nvp);
			*depp = NULL;
			return (error);
		}
		DE_INTERNALIZE(ldep, direntptr);
		brelse(bp);
	}

	/*
	 * Fill in a few fields of the vnode and finish filling in the
	 * denode.  Then return the address of the found denode.
	 */
	if (ldep->de_Attributes & ATTR_DIRECTORY) {
		/*
		 * Since DOS directory entries that describe directories
		 * have 0 in the filesize field, we take this opportunity
		 * to find out the length of the directory and plug it into
		 * the denode structure.
		 */
		u_long size;

                /*
                 * On some disks, the cluster number in the "." entry is zero,
                 * or otherwise damaged.  If it's inconsistent, we'll use the
                 * correct value.
                 */
                if ((diroffset == 0) && (ldep->de_StartCluster != dirclust))
                    ldep->de_StartCluster = dirclust;
                
		nvp->v_type = VDIR;
		if (ldep->de_StartCluster != MSDOSFSROOT) {
			error = pcbmap(ldep, 0xffff, 1, NULL, &size, NULL);
			if (error == E2BIG) {
				ldep->de_FileSize = de_cn2off(pmp, size);
				error = 0;
			} else
				printf("deget(): pcbmap returned %d\n", error);
		}
	} else {
		nvp->v_type = VREG;
		(void) ubc_info_init(nvp);
	}
	getmicrouptime(&tv);
	SETHIGH(ldep->de_modrev, tv.tv_sec);
	SETLOW(ldep->de_modrev, tv.tv_usec * 4294);
	ldep->de_devvp = pmp->pm_devvp;
	VREF(ldep->de_devvp);
	*depp = ldep;
	return (0);
}

int
deupdat(dep, waitfor)
	struct denode *dep;
	int waitfor;
{
	int error;
	struct buf *bp;
	struct direntry *dirp;
	struct timespec ts;

	if (DETOV(dep)->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	getnanotime(&ts);
	DETIMES(dep, &ts, &ts, &ts);
	if ((dep->de_flag & DE_MODIFIED) == 0)
		return (0);
	dep->de_flag &= ~DE_MODIFIED;
	if ((dep->de_Attributes & ATTR_DIRECTORY) &&
            (DETOV(dep)->v_flag & VROOT) &&
            (dep->de_pmp->pm_label_cluster == CLUST_EOFE))
		return (0);	/* There is no volume label entry to update. */
	if (dep->de_refcnt <= 0)
		return (0);
	error = readde(dep, &bp, &dirp);
	if (error)
		return (error);
        if (DETOV(dep)->v_flag & VROOT)
            DE_EXTERNALIZE_ROOT(dirp, dep);
        else
            DE_EXTERNALIZE(dirp, dep);
	if (waitfor)
		return (bwrite(bp));
	else {
		bdwrite(bp);
		return (0);
	}
}

/*
 * Truncate the file described by dep to the length specified by length.
 */
int
detrunc(dep, length, flags, cred, p)
	struct denode *dep;
	u_long length;
	int flags;
	struct ucred *cred;
	struct proc *p;
{
    int error;
    int allerror;
    u_long eofentry;
    u_long chaintofree;
    daddr_t bn;
    int boff;
    int isadir = dep->de_Attributes & ATTR_DIRECTORY;
    struct buf *bp;
    struct msdosfsmount *pmp = dep->de_pmp;
    struct vnode *vp = DETOV(dep);

#ifdef MSDOSFS_DEBUG
    printf("detrunc(): file %s, length %lu, flags %x\n", dep->de_Name, length, flags);
#endif

    /*
     * Disallow attempts to truncate the root directory since it is of
     * fixed size.  That's just the way dos filesystems are.  We use
     * the VROOT bit in the vnode because checking for the directory
     * bit and a startcluster of 0 in the denode is not adequate to
     * recognize the root directory at this point in a file or
     * directory's life.
     */
    if ((vp->v_flag & VROOT) && !FAT32(pmp)) {
        printf("detrunc(): can't truncate root directory, clust %ld, offset %ld\n",
               dep->de_dirclust, dep->de_diroffset);
        return (EINVAL);
    }

    if (dep->de_FileSize < length) {
        return deextend(dep, length, flags, cred);
    }

    /*
     * If the desired length is 0 then remember the starting cluster of
     * the file and set the StartCluster field in the directory entry
     * to 0.  If the desired length is not zero, then get the number of
     * the last cluster in the shortened file.  Then get the number of
     * the first cluster in the part of the file that is to be freed.
     * Then set the next cluster pointer in the last cluster of the
     * file to CLUST_EOFE.
     */
    if (length == 0) {
        chaintofree = dep->de_StartCluster;
        dep->de_StartCluster = 0;
        eofentry = ~0;
    } else {
        error = pcbmap(dep, de_clcount(pmp, length) - 1, 1, NULL,
                       &eofentry, NULL);
        if (error) {
#ifdef MSDOSFS_DEBUG
            printf("detrunc(): pcbmap fails %d\n", error);
#endif
            return (error);
        }
    }

    fc_purge(dep, de_clcount(pmp, length));

    if (UBCISVALID(vp))
        ubc_setsize(vp, (off_t)length); /* XXX check errors */

    allerror = vinvalbuf(vp, ((length > 0) ? V_SAVE : 0), cred, p, 0, 0);
#ifdef MSDOSFS_DEBUG
    if (allerror)
        printf("detrunc(): vtruncbuf error %d\n", allerror);
#endif

    dep->de_FileSize = length;
   /*
     * If the new length is not a multiple of the cluster size then we
     * must zero the tail end of the new last cluster in case it
     * becomes part of the file again because of a seek.
     */
    if ((isadir) && (boff = length & pmp->pm_crbomask) != 0) {
        bn = cntobn(pmp, eofentry);
        error = meta_bread(pmp->pm_devvp, bn, pmp->pm_bpcluster,
                           NOCRED, &bp);
        if (error) {
            brelse(bp);
#ifdef MSDOSFS_DEBUG
            printf("detrunc(): bread fails %d\n", error);
#endif
            return (error);
        }
        /*
         * is this the right place for it?
         */
        bzero(bp->b_data + boff, pmp->pm_bpcluster - boff);
        if (flags & IO_SYNC)
            bwrite(bp);
        else
            bdwrite(bp);
    }

    /*
     * Write out the updated directory entry.  Even if the update fails
     * we free the trailing clusters.
     */
    if (!isadir)
        dep->de_flag |= DE_UPDATE|DE_MODIFIED;

    error = deupdat(dep, 1);
    if (error && (allerror == 0))
        allerror = error;
#ifdef MSDOSFS_DEBUG
    printf("detrunc(): allerror %d, eofentry %lu\n",
           allerror, eofentry);
#endif

    /*
     * If we need to break the cluster chain for the file then do it
     * now.
     */
    if (eofentry != ~0) {
        error = fatentry(FAT_GET_AND_SET, pmp, eofentry,
                         &chaintofree, CLUST_EOFE);
        if (error) {
#ifdef MSDOSFS_DEBUG
            printf("detrunc(): fatentry errors %d\n", error);
#endif
            return (error);
        }
        fc_setcache(dep, FC_LASTFC, de_cluster(pmp, length - 1),
                    eofentry);
    }

    /*
     * Now free the clusters removed from the file because of the
     * truncation.
     */
    if (chaintofree != 0 && !MSDOSFSEOF(pmp, chaintofree))
        freeclusterchain(pmp, chaintofree);

   return (allerror);
}

/*
 * Extend the file described by dep to length specified by length.
 */
int
deextend(dep, length, flags, cred)
	struct denode *dep;
	u_long length;
	int flags;
	struct ucred *cred;
{
    struct msdosfsmount *pmp = dep->de_pmp;
    u_long count;
    int error;

    /*
     * The root of a DOS filesystem cannot be extended.
     */
    if ((DETOV(dep)->v_flag & VROOT) && !FAT32(pmp))
        return (EINVAL);

    /*
     * Directories cannot be extended.
     */
    if (dep->de_Attributes & ATTR_DIRECTORY)
        return (EISDIR);

    if (length <= dep->de_FileSize)
        panic("deextend: file too large");

    /*
     * Compute the number of clusters to allocate.
     */
    count = de_clcount(pmp, length) - de_clcount(pmp, dep->de_FileSize);
    if (count > 0) {
        if (count > pmp->pm_freeclustercount)
            return (ENOSPC);
        error = extendfile(dep, count);
        if (error) {
            /* truncate the added clusters away again */
            (void) detrunc(dep, dep->de_FileSize, 0, cred, NULL);
            return (error);
        }
    }

    if (UBCISVALID(DETOV(dep))) {
        register_t    devBlockSize;

        VOP_DEVBLOCKSIZE(pmp->pm_devvp, &devBlockSize);

#ifdef MSDOSFS_DEBUG
        printf("msdosfs: deextend...zeroing from 0x%lx to 0x%lx\n",
               dep->de_FileSize, length);
#endif
        error = cluster_write(DETOV(dep), (struct uio *) 0,
                              (off_t)dep->de_FileSize, (off_t)(length),
                              (off_t)dep->de_FileSize, (off_t)0,
                              devBlockSize,
                              ((flags & DE_SYNC) | IO_HEADZEROFILL));
        if (error)
            return (error);

        ubc_setsize(DETOV(dep), (off_t)length); /* XXX check errors */
    }

    dep->de_FileSize = length;

    dep->de_flag |= DE_UPDATE|DE_MODIFIED;
    return (deupdat(dep, 1));
}

/*
 * Move a denode to its correct hash queue after the file it represents has
 * been moved to a new directory.
 */
void
reinsert(dep)
	struct denode *dep;
{
	/*
	 * Fix up the denode cache.  If the denode is for a directory,
	 * there is nothing to do since the hash is based on the starting
	 * cluster of the directory file and that hasn't changed.  If for a
	 * file the hash is based on the location of the directory entry,
	 * so we must remove it from the cache and re-enter it with the
	 * hash based on the new location of the directory entry.
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY)
		return;
	msdosfs_hashrem(dep);
	msdosfs_hashins(dep);
}

int
msdosfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);

	extern int prtactive;
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_reclaim(): dep %p, file %s, refcnt %ld\n",
	    dep, dep->de_Name, dep->de_refcnt);
#endif

	if (prtactive && vp->v_usecount != 0)
		vprint("msdosfs_reclaim(): pushing active", vp);
	/*
	 * Remove the denode from its hash chain.
	 */
	msdosfs_hashrem(dep);
	/*
	 * Purge old data structures associated with the denode.
	 */
	cache_purge(vp);
	if (dep->de_devvp) {
		vrele(dep->de_devvp);
		dep->de_devvp = 0;
	}
#if 0 /* XXX */
	dep->de_flag = 0;
#endif

    zfree(msdosfs_node_zone, (vm_offset_t) dep);
	vp->v_data = NULL;

	return (0);
}

int
msdosfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	struct proc *p = ap->a_p;
	int error = 0;

	extern int prtactive;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, de_Name[0] %x\n", dep, dep->de_Name[0]);
#endif

	if (prtactive && vp->v_usecount != 0)
		vprint("msdosfs_inactive(): pushing active", vp);

	/*
	 * Ignore denodes related to stale file handles.
	 */
	if (dep->de_Name[0] == SLOT_DELETED)
		goto out;

	/*
	 * If the file has been deleted and it is on a read/write
	 * filesystem, then truncate the file, and mark the directory slot
	 * as empty.  (This may not be necessary for the dos filesystem.)
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, refcnt %ld, mntflag %x, MNT_RDONLY %x\n",
	       dep, dep->de_refcnt, vp->v_mount->mnt_flag, MNT_RDONLY);
#endif
	if (dep->de_refcnt <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		error = detrunc(dep, (u_long) 0, 0, NOCRED, p);
		dep->de_flag |= DE_UPDATE;
		dep->de_Name[0] = SLOT_DELETED;
	}
	deupdat(dep, 0);

out:
	VOP_UNLOCK(vp, 0, p);
	/*
	 * If we are done with the denode, reclaim it
	 * so that it can be reused immediately.
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): v_usecount %d, de_Name[0] %x\n", vp->v_usecount,
	       dep->de_Name[0]);
#endif
	if (dep->de_Name[0] == SLOT_DELETED)
		vrecycle(vp, (simple_lock_data_t *)0, p);
	return (error);
}


/*
 * defileid -- Return the file ID (inode number) for a given denode.  This routine
 * is used by msdosfs_getattr, msdosfs_readdir, and msdosfs_getattrlist to ensure
 * a consistent file ID space.
 *
 * In older versions, the file ID was based on the location of the directory entry
 * on disk (essentially the byte offset of the entry divided by the size of an entry).
 * The file ID of a directory was the ID of the "." entry (which is the first entry
 * in the directory).  The file ID of the root of a FAT12 or FAT16 volume (whose root
 * directory is not in an allocated cluster) is 1.
 *
 * We now use the starting cluster number of the file or directory, or 1 for the root
 * of a FAT12 or FAT16 volume.  Note that empty files have no starting cluster number,
 * and their file ID is a constant that is out of range for any FAT volume (since
 * FAT32 really only uses 28 bits for cluster number).  Directories (other than the root)
 * always contain at least one cluster for their "." and ".." entries.
 */
u_long defileid(struct denode *dep)
{
    u_long fileid;
    
    fileid = dep->de_StartCluster;
    
    if ((dep->de_Attributes & ATTR_DIRECTORY) && (dep->de_StartCluster == MSDOSFSROOT))
        fileid = FILENO_ROOT;		/* root of FAT12 or FAT16 */
    
    if (fileid == 0)			/* empty? */
        fileid = FILENO_EMPTY;		/* use an out-of-range cluster number */
        
    return fileid;
}
