/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#include <libkern/OSMalloc.h>

OSMallocTag  msdosfs_node_tag;

static struct denode **dehashtbl;
static u_long dehash;			/* size of hash table - 1 */
#define	DEHASH(dev, dcl, doff)	(dehashtbl[(minor(dev) + (dcl) + (doff) / 	\
				sizeof(struct dosdirentry)) & dehash])

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

static struct denode *
		msdosfs_hashget __P((dev_t dev, u_long dirclust,
				     u_long diroff));
static void	msdosfs_hashins __P((struct denode *dep));
static void	msdosfs_hashrem __P((struct denode *dep));

__private_extern__ void
msdosfs_hash_init(void)
{
    msdosfs_node_tag = OSMalloc_Tagalloc("msdosfs denode", OSMT_DEFAULT);
    dehashtbl = hashinit(desiredvnodes, M_TEMP, &dehash);
}

__private_extern__ void
msdosfs_hash_uninit(void)
{
	OSMalloc_Tagfree(msdosfs_node_tag);
}

static struct denode *
msdosfs_hashget(dev, dirclust, diroff)
	dev_t dev;
	u_long dirclust;
	u_long diroff;
{
	struct denode *dep;
	vnode_t vp;
	u_int32_t vid;

loop:
	for (dep = DEHASH(dev, dirclust, diroff); dep; dep = dep->de_next)
	{
		if (dirclust == dep->de_dirclust
		    && diroff == dep->de_diroffset
		    && dev == dep->de_dev
		    && dep->de_refcnt != 0)
		{
			if (ISSET(dep->de_flag, DE_INIT))
			{
				/* Denode is being initialized.  Wait for it to complete. */
				SET(dep->de_flag, DE_WAITINIT);
				msleep(dep, NULL, PINOD, "msdosfs_hashget", 0);
				goto loop;
			}
			vp = DETOV(dep);
			vid = vnode_vid(vp);
			if (vnode_getwithvid(vp, vid))
				goto loop;
			return dep;
		}
	}
	return NULL;
}

static void
msdosfs_hashins(dep)
	struct denode *dep;
{
	struct denode **depp, *deq;

	depp = &DEHASH(dep->de_dev, dep->de_dirclust, dep->de_diroffset);
	deq = *depp;
	if (deq)
		deq->de_prev = &dep->de_next;
	dep->de_next = deq;
	dep->de_prev = depp;
	*depp = dep;
}

static void
msdosfs_hashrem(dep)
	struct denode *dep;
{
	struct denode *deq;

	deq = dep->de_next;
	if (deq)
		deq->de_prev = dep->de_prev;
	*dep->de_prev = deq;
}

/*
 * If deget() succeeds it returns with an io_count reference on the denode's
 * corresponding vnode.  If not returning that vnode to VFS, then be sure
 * to vnode_put it!
 *
 * pmp	     - address of msdosfsmount structure of the filesystem containing
 *	       the denode of interest.  The pm_dev field and the address of
 *	       the msdosfsmount structure are used.
 * dirclust  - which cluster bp contains, if dirclust is 0 (root directory)
 *	       diroffset is relative to the beginning of the root directory,
 *	       otherwise it is cluster relative.
 * diroffset - offset from start of parent directory to the directory entry we want
 * dvp		 - parent directory
 * cnp		 - name used to look up the denode
 * depp	     - returns the address of the gotten denode.
 */
__private_extern__ int
deget(pmp, dirclust, diroffset, dvp, cnp, depp, context)
	struct msdosfsmount *pmp;	/* so we know the maj/min number */
	u_long dirclust;			/* cluster this dir entry came from */
	u_long diroffset;			/* index of entry within the cluster */
	vnode_t dvp;				/* parent directory */
	struct componentname *cnp;	/* name used to find this node */
	struct denode **depp;		/* returns the addr of the gotten denode */
	vfs_context_t context;
{
	int error;
	dev_t dev = pmp->pm_dev;
	struct mount *mntp = pmp->pm_mountp;
	struct denode *new_dep = NULL;
	struct denode *found_dep;
	struct dosdirentry *direntptr;
	struct buf *bp = NULL;
	struct timeval tv;
	struct vnode_fsparam vfsp;
	enum vtype vtype;
	vnode_t vp;

	/*
	 * On FAT32 filesystems, root is a (more or less) normal
	 * directory
	 */
	if (FAT32(pmp) && dirclust == MSDOSFSROOT)
		dirclust = pmp->pm_rootdirblk;

	/*
	 * Allocate a denode before looking in the hash.  This is
	 * because the allocation could block, during which time
	 * a denode could be added to or removed from the hash.
	 */
    new_dep = OSMalloc(sizeof(struct denode), msdosfs_node_tag);

	/*
	 * See if the denode is already in our hash.  If so,
	 * just return it (and free up the one we just allocated).
	 */
	found_dep = msdosfs_hashget(dev, dirclust, diroffset);
	if (found_dep != NULL) {
		*depp = found_dep;
		if (dvp && cnp && (cnp->cn_flags & MAKEENTRY))
			cache_enter(dvp, DETOV(found_dep), cnp);
		OSFree(new_dep, sizeof(struct denode), msdosfs_node_tag);
		return (0);
	}

	/*
	 * There was nothing in the hash.  Before we block on I/O,
	 * we need to insert a matching denode marked as being
	 * initialized.  Any other deget() will block until we're
	 * finished here, and either find the fully initialized
	 * denode, or none at all.
	 */
	bzero(new_dep, sizeof *new_dep);
	new_dep->de_pmp = pmp;
	new_dep->de_devvp = pmp->pm_devvp;
	vnode_ref(new_dep->de_devvp);
	new_dep->de_dev = dev;
	new_dep->de_dirclust = dirclust;
	new_dep->de_diroffset = diroffset;
	new_dep->de_refcnt = 1;
	SET(new_dep->de_flag, DE_INIT);
	msdosfs_hashins(new_dep);
	
	vfsp.vnfs_markroot = 0;	/* Assume not the root */

	fc_purge(new_dep, 0);	/* init the fat cache for this denode */
	
	/*
	 * Copy the directory entry into the denode
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
		vfsp.vnfs_markroot = 1;

		new_dep->de_Attributes = ATTR_DIRECTORY;
		new_dep->de_LowerCase = 0;
		if (FAT32(pmp))
			new_dep->de_StartCluster = pmp->pm_rootdirblk;
			/* de_FileSize will be filled in further down */
		else {
			new_dep->de_StartCluster = MSDOSFSROOT;
			new_dep->de_FileSize = pmp->pm_rootdirsize * pmp->pm_BlockSize;
		}
		/*
		 * fill in time and date so that dos2unixtime() doesn't
		 * spit up when called from msdosfs_getattr() with root
		 * denode
		 */
		new_dep->de_CHun = 0;
		new_dep->de_CTime = 0x0000;	/* 00:00:00	 */
		new_dep->de_CDate = (0 << DD_YEAR_SHIFT) | (1 << DD_MONTH_SHIFT)
		    | (1 << DD_DAY_SHIFT);
		/* Jan 1, 1980	 */
		new_dep->de_ADate = new_dep->de_CDate;
		new_dep->de_MTime = new_dep->de_CTime;
		new_dep->de_MDate = new_dep->de_CDate;
		/* leave the other fields as garbage */
                
		/*
		 * If there is a volume label entry, then grab the times from it instead.
		 */
		if (pmp->pm_label_cluster != CLUST_EOFE) {
			error = readep(pmp, pmp->pm_label_cluster, pmp->pm_label_offset,
					&bp, &direntptr, context);
			if (!error) {
				new_dep->de_CHun = direntptr->deCHundredth;
				new_dep->de_CTime = getushort(direntptr->deCTime);
				new_dep->de_CDate = getushort(direntptr->deCDate);
				new_dep->de_ADate = getushort(direntptr->deADate);
				new_dep->de_MTime = getushort(direntptr->deMTime);
				new_dep->de_MDate = getushort(direntptr->deMDate);
				buf_brelse(bp);
				bp = NULL;
			}
		}
	} else {
		/* Not the root directory */
		error = readep(pmp, dirclust, diroffset, &bp, &direntptr, context);
		if (error)
			goto fail;
		DE_INTERNALIZE(new_dep, direntptr);
		buf_brelse(bp);
		bp = NULL;
	}

	/*
	 * Determine initial values for vnode fields, and finish
	 * populating the denode.
	 */
	if (new_dep->de_Attributes & ATTR_DIRECTORY) {
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
		if ((diroffset == 0) && (new_dep->de_StartCluster != dirclust))
			new_dep->de_StartCluster = dirclust;
                
		vtype = VDIR;
		if (new_dep->de_StartCluster != MSDOSFSROOT) {
			error = pcbmap(new_dep, 0xffff, 1, NULL, &size, NULL, context);
			if (error == E2BIG) {
				new_dep->de_FileSize = de_cn2off(pmp, size);
				error = 0;
			}
		}
	} else {
		/*
		 * We found a regular file.  See if it is really a symlink.
		 */
		vtype = msdosfs_check_link(new_dep, context);
	}
	getmicrouptime(&tv);
	SETHIGH(new_dep->de_modrev, tv.tv_sec);
	SETLOW(new_dep->de_modrev, tv.tv_usec * 4294);
	
	/*
	 * Create the vnode
	 */
	vfsp.vnfs_mp = mntp;
	vfsp.vnfs_vtype = vtype;
	vfsp.vnfs_str = "msdosfs";
	vfsp.vnfs_dvp = dvp;
	vfsp.vnfs_fsnode = new_dep;
	vfsp.vnfs_cnp = cnp;
	vfsp.vnfs_vops = msdosfs_vnodeop_p;
	vfsp.vnfs_rdev = 0;		/* msdosfs doesn't support block devices */
	vfsp.vnfs_filesize = new_dep->de_FileSize;
	if (dvp && cnp && (cnp->cn_flags & MAKEENTRY))
		vfsp.vnfs_flags = 0;
	else
		vfsp.vnfs_flags = VNFS_NOCACHE;
	/* vfsp.vnfs_markroot was set or cleared above */
	vfsp.vnfs_marksystem = 0;	/* msdosfs has no "system" vnodes */
	
	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &vp);
	if (error)
		goto fail;

	/*
	 * Make the denode reference the new vnode, and
	 * take a reference on it (on behalf of the denode).
	 */
	new_dep->de_vnode = vp;
	vnode_addfsref(vp);
	
	/*
	 * Return it.  We're done.
	 */
	*depp = new_dep;

	CLR(new_dep->de_flag, DE_INIT);
	if (ISSET(new_dep->de_flag, DE_WAITINIT))
		wakeup(new_dep);
	
	return 0;

fail:
	if (bp)
		buf_brelse(bp);

	msdosfs_hashrem(new_dep);
	
	if (ISSET(new_dep->de_flag, DE_WAITINIT))
		wakeup(new_dep);
	
	OSFree(new_dep, sizeof(struct denode), msdosfs_node_tag);
	
	return error;
}

__private_extern__ int
deupdat(dep, waitfor, context)
	struct denode *dep;
	int waitfor;
	vfs_context_t context;
{
	int error;
	struct buf *bp;
	struct dosdirentry *dirp;
	struct timespec ts;

	if (vnode_vfsisrdonly(DETOV(dep)))
		return (0);
	getnanotime(&ts);
	DETIMES(dep, &ts, &ts, &ts);
	if ((dep->de_flag & DE_MODIFIED) == 0)
		return (0);
	dep->de_flag &= ~DE_MODIFIED;
	if ((dep->de_Attributes & ATTR_DIRECTORY) &&
            (vnode_isvroot(DETOV(dep))) &&
            (dep->de_pmp->pm_label_cluster == CLUST_EOFE))
		return (0);	/* There is no volume label entry to update. */
	if (dep->de_refcnt <= 0)
		return (0);
	error = readde(dep, &bp, &dirp, context);
	if (error)
		return (error);
        if (vnode_isvroot(DETOV(dep)))
            DE_EXTERNALIZE_ROOT(dirp, dep);
        else
            DE_EXTERNALIZE(dirp, dep);
	return ((int)buf_bdwrite(bp));
}

/*
 * Truncate the file described by dep to the length specified by length.
 */
__private_extern__ int
detrunc(dep, length, flags, context)
	struct denode *dep;
	u_long length;
	int flags;
	vfs_context_t context;
{
    int error;
    int allerror;
    u_long eofentry;
    u_long chaintofree;
    daddr64_t bn;
    int boff;
    int isadir = dep->de_Attributes & ATTR_DIRECTORY;
    struct buf *bp;
    struct msdosfsmount *pmp = dep->de_pmp;
    vnode_t vp = DETOV(dep);

    /*
     * Disallow attempts to truncate the root directory since it is of
     * fixed size.  That's just the way dos filesystems are.  We use
     * the VROOT bit in the vnode because checking for the directory
     * bit and a startcluster of 0 in the denode is not adequate to
     * recognize the root directory at this point in a file or
     * directory's life.
     */
    if (vnode_isvroot(vp) && !FAT32(pmp)) {
        printf("detrunc(): can't truncate root directory, clust %ld, offset %ld\n",
               dep->de_dirclust, dep->de_diroffset);
        return (EINVAL);
    }

    if (dep->de_FileSize < length) {
        return deextend(dep, length, flags, context);
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
                       &eofentry, NULL, context);
        if (error)
            return error;
    }

    fc_purge(dep, de_clcount(pmp, length));

	ubc_setsize(vp, (off_t)length); /* XXX check errors */

	allerror = buf_invalidateblks(vp, ((length > 0) ? BUF_WRITE_DATA : 0), 0, 0);
	
    dep->de_FileSize = length;

    /*
     * Write out the updated directory entry.  Even if the update fails
     * we free the trailing clusters.
     */
    if (!isadir)
        dep->de_flag |= DE_UPDATE|DE_MODIFIED;

    error = deupdat(dep, 1, context);
    if (error && (allerror == 0))
        allerror = error;

    /*
     * If we need to break the cluster chain for the file then do it
     * now.
     */
    if (eofentry != ~0) {
        error = fatentry(FAT_GET_AND_SET, pmp, eofentry,
                         &chaintofree, CLUST_EOFE, context);
        if (error)
            return error;

        fc_setcache(dep, FC_LASTFC, de_cluster(pmp, length - 1),
                    eofentry);
    }

    /*
     * Now free the clusters removed from the file because of the
     * truncation.
     */
    if (chaintofree != 0 && !MSDOSFSEOF(pmp, chaintofree))
        freeclusterchain(pmp, chaintofree, context);

   return (allerror);
}

/*
 * Extend the file described by dep to length specified by length.
 */
__private_extern__ int
deextend(dep, length, flags, context)
	struct denode *dep;
	u_long length;
	int flags;
	vfs_context_t context;
{
    struct msdosfsmount *pmp = dep->de_pmp;
    u_long count;
    int error;

    /*
     * The root of a DOS filesystem cannot be extended.
     */
    if (vnode_isvroot(DETOV(dep)) && !FAT32(pmp))
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
        error = extendfile(dep, count, context);
        if (error) {
            /* truncate the added clusters away again */
            (void) detrunc(dep, dep->de_FileSize, 0, context);
            return (error);
        }
    }

	error = cluster_write(DETOV(dep), (struct uio *) 0,
						  (off_t)dep->de_FileSize, (off_t)(length),
						  (off_t)dep->de_FileSize, (off_t)0,
						  ((flags & DE_SYNC) | IO_HEADZEROFILL));
	if (error)
		return (error);

	ubc_setsize(DETOV(dep), (off_t)length); /* XXX check errors */

    dep->de_FileSize = length;

    dep->de_flag |= DE_UPDATE|DE_MODIFIED;
    return (deupdat(dep, 1, context));
}

/*
 * Move a denode to its correct hash queue after the file it represents has
 * been moved to a new directory.
 */
__private_extern__ void
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

__private_extern__ int
msdosfs_reclaim(ap)
	struct vnop_reclaim_args /* {
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct denode *dep = VTODE(vp);

	/*
	 * Skip everything if there was no denode.  This can happen
	 * If the vnode was temporarily created in msdosfs_check_link.
	 */
	if (dep == NULL)
		return 0;

	/*
	 * Remove the denode from its hash chain.
	 */
	msdosfs_hashrem(dep);
	/*
	 * Purge old data structures associated with the denode.
	 */
	cache_purge(vp);
	vnode_rele(dep->de_devvp);
	
	OSFree(dep, sizeof(struct denode), msdosfs_node_tag);
	vnode_clearfsnode(vp);
	vnode_removefsref(vp);

	return (0);
}

__private_extern__ int
msdosfs_inactive(ap)
	struct vnop_inactive_args /* {
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	vfs_context_t context = ap->a_context;
	struct denode *dep = VTODE(vp);
	int error = 0;
	int needs_flush = 0;

	/*
	 * Skip everything if there was no denode.  This can happen
	 * If the vnode was temporarily created in msdosfs_check_link.
	 */
	if (dep == NULL)
		return 0;

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
	if (dep->de_refcnt <= 0 && ( !vnode_vfsisrdonly(vp))) {
		error = detrunc(dep, (u_long) 0, 0, context);
		needs_flush = 1;
		dep->de_flag |= DE_UPDATE;
		dep->de_Name[0] = SLOT_DELETED;
	}
	deupdat(dep, 1, context);

out:
	if (needs_flush)
		msdosfs_meta_flush(dep->de_pmp);

	/*
	 * If we are done with the denode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (dep->de_Name[0] == SLOT_DELETED)
		vnode_recycle(vp);

	return (error);
}


/*
 * defileid -- Return the file ID (inode number) for a given denode.  This routine
 * is used by msdosfs_getattr and msdosfs_readdir to ensure a consistent file
 * ID space.
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
__private_extern__ u_long defileid(struct denode *dep)
{
    u_long fileid;
    
    fileid = dep->de_StartCluster;
    
    if ((dep->de_Attributes & ATTR_DIRECTORY) && (dep->de_StartCluster == MSDOSFSROOT))
        fileid = FILENO_ROOT;		/* root of FAT12 or FAT16 */
    
    if (fileid == 0)			/* empty? */
        fileid = FILENO_EMPTY;		/* use an out-of-range cluster number */
        
    return fileid;
}
