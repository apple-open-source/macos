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
/* $FreeBSD: src/sys/msdosfs/msdosfs_lookup.c,v 1.31 2000/05/05 09:58:35 phk Exp $ */
/*	$NetBSD: msdosfs_lookup.c,v 1.37 1997/11/17 15:36:54 ws Exp $	*/

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
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/utfconv.h>

#include "bpb.h"
#include "direntry.h"
#include "denode.h"
#include "msdosfsmount.h"
#include "fat.h"

static int msdosfs_lookup __P((struct vnode *, struct vnode **, struct componentname *));

/*
 * Perform canonical checks and cache lookup and pass on to
 * msdosfs_lookup only if needed.
 *
 * Based on vn_cache_lookup from FreeBSD4.1
 */
int
msdosfs_cache_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vdp;
	struct vnode *pdp;
	int lockparent;	
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	struct proc *p = cnp->cn_proc;
	u_long vpid;	/* capability number of vnode */

	*vpp = NULL;
	vdp = ap->a_dvp;
	lockparent = flags & LOCKPARENT;

	if (vdp->v_type != VDIR)
                return (ENOTDIR);

	if ((flags & ISLASTCN) && (vdp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	error = VOP_ACCESS(vdp, VEXEC, cred, cnp->cn_proc);

	if (error)
		return (error);

	error = cache_lookup(vdp, vpp, cnp);

	if (!error) 
		return (msdosfs_lookup(ap->a_dvp, ap->a_vpp, ap->a_cnp));

	if (error == ENOENT)
		return (error);

	pdp = vdp;
	vdp = *vpp;
	vpid = vdp->v_id;
	if (pdp == vdp) {   /* lookup on "." */
		VREF(vdp);
		error = 0;
	} else if (flags & ISDOTDOT) {
		VOP_UNLOCK(pdp, 0, p);
		error = vget(vdp, LK_EXCLUSIVE, p);
		if (!error && lockparent && (flags & ISLASTCN))
			error = vn_lock(pdp, LK_EXCLUSIVE, p);
	} else {
		error = vget(vdp, LK_EXCLUSIVE, p);
		if (!lockparent || error || !(flags & ISLASTCN))
			VOP_UNLOCK(pdp, 0, p);
	}
	/*
	 * Check that the capability number did not change
	 * while we were waiting for the lock.
	 */
	if (!error) {
		if (vpid == vdp->v_id)
			return (0);
		vput(vdp);
		if (lockparent && pdp != vdp && (flags & ISLASTCN))
			VOP_UNLOCK(pdp, 0, p);
	}
	error = vn_lock(pdp, LK_EXCLUSIVE, p);
	if (error)
		return (error);
	return (msdosfs_lookup(ap->a_dvp, ap->a_vpp, ap->a_cnp));
}


/*
 * When we search a directory the blocks containing directory entries are
 * read and examined.  The directory entries contain information that would
 * normally be in the inode of a unix filesystem.  This means that some of
 * a directory's contents may also be in memory resident denodes (sort of
 * an inode).  This can cause problems if we are searching while some other
 * process is modifying a directory.  To prevent one process from accessing
 * incompletely modified directory information we depend upon being the
 * sole owner of a directory block.  bread/brelse provide this service.
 * This being the case, when a process modifies a directory it must first
 * acquire the disk block that contains the directory entry to be modified.
 * Then update the disk block and the denode, and then write the disk block
 * out to disk.  This way disk blocks containing directory entries and in
 * memory denode's will be in synch.
 */
static int
msdosfs_lookup(vdp, vpp, cnp)
	struct vnode *vdp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	daddr_t bn;
	int error;
	int lockparent;
	int wantparent;
	int slotcount;
	int slotoffset = 0;
	int frcn;
	u_long cluster;
	int blkoff;
	int diroff;
	u_long blsize;
	int isadir;		/* ~0 if found direntry is a directory	 */
	u_long scn;		/* starting cluster number		 */
	struct vnode *pdp;
	struct denode *dp;
	struct denode *tdp;
	struct msdosfsmount *pmp;
	struct buf *bp = 0;
	struct direntry *dep = NULL;
	u_char dosfilename[12];
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	struct proc *p = cnp->cn_proc;
	int unlen;

	int wincnt = 1;
	int chksum = -1;
	int olddos = 1;

	u_int16_t ucfn[WIN_MAXLEN];
	size_t unichars;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): looking for %s\n", cnp->cn_nameptr);
#endif
	dp = VTODE(vdp);
	pmp = dp->de_pmp;
	*vpp = NULL;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT | WANTPARENT);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): vdp %p, dp %p, Attr %02x\n",
	    vdp, dp, dp->de_Attributes);
#endif

	/*
	 * If they are going after the . or .. entry in the root directory,
	 * they won't find it.  DOS filesystems don't have them in the root
	 * directory.  So, we fake it. deget() is in on this scam too.
	 */
	if ((vdp->v_flag & VROOT) && cnp->cn_nameptr[0] == '.' &&
	    (cnp->cn_namelen == 1 ||
		(cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.'))) {
		isadir = ATTR_DIRECTORY;
		scn = MSDOSFSROOT;
#ifdef MSDOSFS_DEBUG
		printf("msdosfs_lookup(): looking for . or .. in root directory\n");
#endif
		cluster = MSDOSFSROOT;
		blkoff = MSDOSFSROOT_OFS;
		goto foundroot;
	}

	/*
	 * Decode lookup name into UCS-2 (Unicode)
	 */
	(void) utf8_decodestr(cnp->cn_nameptr, cnp->cn_namelen, ucfn, &unichars,
				sizeof(ucfn), 0, UTF_PRECOMPOSED);
	unichars /= 2; /* bytes to chars */
	
	switch (unicode2dosfn(ucfn, dosfilename, unichars, 0)) {
	case 0:
                /*
                 * The name is syntactically invalid.  Normally, we'd return EINVAL,
                 * but ENAMETOOLONG makes it clear that the name is the problem (and
                 * allows Carbon to return a more meaningful error).
                 */
		return (ENAMETOOLONG);
	case 1:
		break;
	case 2:
		wincnt = winSlotCnt(ucfn, unichars) + 1;
		break;
	case 3:
		olddos = 0;
		wincnt = winSlotCnt(ucfn, unichars) + 1;
		break;
	}
	if (pmp->pm_flags & MSDOSFSMNT_SHORTNAME) {
		wincnt = 1;
		olddos = 1;
	}

	unlen = winLenFixup(ucfn, unichars);

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	slotcount = wincnt;
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN))
		slotcount = 0;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): dos version of filename %s, length %ld\n",
	    dosfilename, cnp->cn_namelen);
#endif
	/*
	 * Search the directory pointed at by vdp for the name in ucfn.
	 */
	tdp = NULL;
	/*
	 * The outer loop ranges over the clusters that make up the
	 * directory.  Note that the root directory is different from all
	 * other directories.  It has a fixed number of blocks that are not
	 * part of the pool of allocatable clusters.  So, we treat it a
	 * little differently. The root directory starts at "cluster" 0.
	 */
	diroff = 0;
	for (frcn = 0;; frcn++) {
		error = pcbmap(dp, frcn, 1, &bn, &cluster, &blsize);
		if (error) {
			if (error == E2BIG)
				break;
			return (error);
		}
                error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}
		for (blkoff = 0; blkoff < blsize;
		     blkoff += sizeof(struct direntry),
		     diroff += sizeof(struct direntry)) {
			dep = (struct direntry *)(bp->b_data + blkoff);
			/*
			 * If the slot is empty and we are still looking
			 * for an empty then remember this one.  If the
			 * slot is not empty then check to see if it
			 * matches what we are looking for.  If the slot
			 * has never been filled with anything, then the
			 * remainder of the directory has never been used,
			 * so there is no point in searching it.
			 */
			if (dep->deName[0] == SLOT_EMPTY ||
			    dep->deName[0] == SLOT_DELETED) {
				/*
				 * Drop memory of previous long matches
				 */
				chksum = -1;

				if (slotcount < wincnt) {
					slotcount++;
					slotoffset = diroff;
				}
				if (dep->deName[0] == SLOT_EMPTY) {
					brelse(bp);
					goto notfound;
				}
			} else {
				/*
				 * If there wasn't enough space for our winentries,
				 * forget about the empty space
				 */
				if (slotcount < wincnt)
					slotcount = 0;

				/*
				 * Check for Win95 long filename entry
				 */
				if (dep->deAttributes == ATTR_WIN95) {
					if (pmp->pm_flags & MSDOSFSMNT_SHORTNAME)
						continue;
					
					chksum = winChkName(ucfn,
							    unlen,
							    (struct winentry *)dep,
							    chksum,
							    pmp->pm_flags & MSDOSFSMNT_U2WTABLE,
							    pmp->pm_u2w,
							    pmp->pm_flags & MSDOSFSMNT_ULTABLE,
							    pmp->pm_ul);
					continue;
				}

				/*
				 * Ignore volume labels (anywhere, not just
				 * the root directory).
				 */
				if (dep->deAttributes & ATTR_VOLUME) {
					chksum = -1;
					continue;
				}

				/*
				 * Check for a checksum or name match
				 */
				if (chksum != winChksum(dep->deName)
				    && (!olddos || bcmp(dosfilename, dep->deName, 11))) {
					chksum = -1;
					continue;
				}
#ifdef MSDOSFS_DEBUG
				printf("msdosfs_lookup(): match blkoff %d, diroff %d\n",
				    blkoff, diroff);
#endif
				/*
				 * Remember where this directory
				 * entry came from for whoever did
				 * this lookup.
				 */
				dp->de_fndoffset = diroff;
				dp->de_fndcnt = wincnt - 1;

				goto found;
			}
		}	/* for (blkoff = 0; .... */
		/*
		 * Release the buffer holding the directory cluster just
		 * searched.
		 */
		brelse(bp);
	}	/* for (frcn = 0; ; frcn++) */

notfound:
	/*
	 * We hold no disk buffers at this point.
	 */

	/*
	 * Fixup the slot description to point to the place where
	 * we might put the new DOS direntry (putting the Win95
	 * long name entries before that)
	 */
	if (!slotcount) {
		slotcount = 1;
		slotoffset = diroff;
	}
	if (wincnt > slotcount)
		slotoffset += sizeof(struct direntry) * (wincnt - slotcount);

	/*
	 * If we get here we didn't find the entry we were looking for. But
	 * that's ok if we are creating or renaming and are at the end of
	 * the pathname and the directory hasn't been removed.
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): op %d, refcnt %ld\n",
	    nameiop, dp->de_refcnt);
	printf("               slotcount %d, slotoffset %d\n",
	       slotcount, slotoffset);
#endif
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN) && dp->de_refcnt != 0) {
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		error = VOP_ACCESS(vdp, VWRITE, cnp->cn_cred, cnp->cn_proc);
		if (error)
			return (error);
		/*
		 * Return an indication of where the new directory
		 * entry should be put.
		 */
		dp->de_fndoffset = slotoffset;
		dp->de_fndcnt = wincnt - 1;

		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to do a direnter().
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory inode in ndp->ni_dvp.
		 * The pathname buffer is saved so that the name
		 * can be obtained later.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(vdp, 0, p);
		return (EJUSTRETURN);
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if ((cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
		cache_enter(vdp, *vpp, cnp);
	return (ENOENT);

found:
	/*
	 * NOTE:  We still have the buffer with matched directory entry at
	 * this point.
	 */
	isadir = dep->deAttributes & ATTR_DIRECTORY;
	scn = getushort(dep->deStartCluster);
	if (FAT32(pmp)) {
		scn |= getushort(dep->deHighClust) << 16;
		if (scn == pmp->pm_rootdirblk) {
			/*
			 * There should actually be 0 here.
			 * Just ignore the error.
			 */
			scn = MSDOSFSROOT;
		}
	}

	if (isadir) {
		/*
		 * For directories, the cluster and offset in the denode
		 * actually point at the "." entry in the directory.
		 * The denode for the root directory itself uses a special
		 * constant MSDOSFSROOT_OFS for the offset.
		 */
		cluster = scn;
		if (cluster == MSDOSFSROOT)
			blkoff = MSDOSFSROOT_OFS;
		else
			blkoff = 0;
	} else if (cluster == MSDOSFSROOT)
		blkoff = diroff;

	/*
	 * Now release buf to allow deget to read the entry again.
	 * Reserving it here and giving it to deget could result
	 * in a deadlock.
	 */
	brelse(bp);
	bp = 0;
	
foundroot:
	/*
	 * If we entered at foundroot, then we are looking for the . or ..
	 * entry of the filesystems root directory.  isadir and scn were
	 * setup before jumping here.  And, bp is already null.
	 */
	if (FAT32(pmp) && scn == MSDOSFSROOT)
		scn = pmp->pm_rootdirblk;

	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * If the wantparent flag isn't set, we return only
	 * the directory (in ndp->ni_dvp), otherwise we go
	 * on and lock the inode, being careful with ".".
	 */
	if (nameiop == DELETE && (flags & ISLASTCN)) {
		/*
		 * Don't allow deleting the root.
		 */
		if (blkoff == MSDOSFSROOT_OFS)
			return EROFS;				/* really? XXX */

		/*
		 * Write access to directory required to delete files.
		 */
		error = VOP_ACCESS(vdp, VWRITE, cnp->cn_cred, cnp->cn_proc);
		if (error)
			return (error);

		/*
		 * Return pointer to current entry in dp->i_offset.
		 * Save directory inode pointer in ndp->ni_dvp for dirremove().
		 */
		if (dp->de_StartCluster == scn && isadir) {	/* "." */
			VREF(vdp);
			*vpp = vdp;
			return (0);
		}
		error = deget(pmp, cluster, blkoff, &tdp);
		if (error)
			return (error);
		*vpp = DETOV(tdp);
		if (!lockparent)
			VOP_UNLOCK(vdp, 0, p);
		return (0);
	}

	/*
	 * If rewriting (RENAME), return the inode and the
	 * information required to rewrite the present directory
	 * Must get inode of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (nameiop == RENAME && wantparent &&
	    (flags & ISLASTCN)) {
		if (blkoff == MSDOSFSROOT_OFS)
			return EROFS;				/* really? XXX */

		error = VOP_ACCESS(vdp, VWRITE, cnp->cn_cred, cnp->cn_proc);
		if (error)
			return (error);

		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
		if (dp->de_StartCluster == scn && isadir)
			return (EISDIR);

		if ((error = deget(pmp, cluster, blkoff, &tdp)) != 0)
			return (error);
		*vpp = DETOV(tdp);
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(vdp, 0, p);
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not `vput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the VFS_VGET for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	pdp = vdp;
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(pdp, 0, p);
		error = deget(pmp, cluster, blkoff,  &tdp);
		if (error) {
			vn_lock(pdp, LK_EXCLUSIVE | LK_RETRY, p); 
			return (error);
		}
		if (lockparent && (flags & ISLASTCN) &&
		    (error = vn_lock(pdp, LK_EXCLUSIVE, p))) {
			vput(DETOV(tdp));
			return (error);
		}
		*vpp = DETOV(tdp);
	} else if (dp->de_StartCluster == scn && isadir) {
		VREF(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		if ((error = deget(pmp, cluster, blkoff, &tdp)) != 0)
			return (error);
		if (!lockparent || !(flags & ISLASTCN))
			VOP_UNLOCK(pdp, 0, p);
		*vpp = DETOV(tdp);
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
	return (0);
}

/*
 * dep  - directory entry to copy into the directory
 * ddep - directory to add to
 * depp - return the address of the denode for the created directory entry
 *	  if depp != 0
 * cnp  - componentname needed for Win95 long filenames
 */
int
createde(dep, ddep, depp, cnp)
	struct denode *dep;
	struct denode *ddep;
	struct denode **depp;
	struct componentname *cnp;
{
	int error;
	u_long dirclust, diroffset;
	struct direntry *ndep;
	struct msdosfsmount *pmp = ddep->de_pmp;
	struct buf *bp;
	daddr_t bn;
	u_long blsize;

#ifdef MSDOSFS_DEBUG
	printf("createde(dep %p, ddep %p, depp %p, cnp %p)\n",
	    dep, ddep, depp, cnp);
#endif

	/*
	 * If no space left in the directory then allocate another cluster
	 * and chain it onto the end of the file.  There is one exception
	 * to this.  That is, if the root directory has no more space it
	 * can NOT be expanded.  extendfile() checks for and fails attempts
	 * to extend the root directory.  We just return an error in that
	 * case.
	 */
    if (ddep->de_fndoffset >= ddep->de_FileSize) {
        diroffset = ddep->de_fndoffset + sizeof(struct direntry)
        		- ddep->de_FileSize;
        dirclust = de_clcount(pmp, diroffset);
        error = extendfile(ddep, dirclust);
        if (error) {
            (void)detrunc(ddep, ddep->de_FileSize, 0, NOCRED, NULL);
            return error;
        }

        /*
         * Update the size of the directory
         */
        ddep->de_FileSize += de_cn2off(pmp, dirclust);
    }

	/*
	 * We just read in the cluster with space.  Copy the new directory
	 * entry in.  Then write it to disk. NOTE:  DOS directories
	 * do not get smaller as clusters are emptied.
	 */
	error = pcbmap(ddep, de_cluster(pmp, ddep->de_fndoffset), 1,
		       &bn, &dirclust, &blsize);
	if (error)
		return error;
	diroffset = ddep->de_fndoffset;
	if (dirclust != MSDOSFSROOT)
		diroffset &= pmp->pm_crbomask;
        if ((error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return error;
	}
	ndep = bptoep(pmp, bp, ddep->de_fndoffset);

	DE_EXTERNALIZE(ndep, dep);

	/*
	 * Now write the Win95 long name
	 */
	if (ddep->de_fndcnt > 0) {
		u_int8_t chksum = winChksum(ndep->deName);
		u_int16_t ucfn[WIN_MAXLEN];
		size_t unichars;
		int cnt = 1;
		
		/*
		 * Decode component name into Unicode
		 * NOTE: We should be using a "precompose" flag
		 */
		(void) utf8_decodestr(cnp->cn_nameptr, cnp->cn_namelen, ucfn,
					&unichars, sizeof(ucfn), 0, UTF_PRECOMPOSED);
		unichars /= 2; /* bytes to chars */

		while (--ddep->de_fndcnt >= 0) {
			if (!(ddep->de_fndoffset & pmp->pm_crbomask)) {
				error = bwrite(bp);
				if (error)
					return error;

				ddep->de_fndoffset -= sizeof(struct direntry);
				error = pcbmap(ddep,
					       de_cluster(pmp,
							  ddep->de_fndoffset),
                                               1,
					       &bn, 0, &blsize);
				if (error)
					return error;

				error = meta_bread(pmp->pm_devvp, bn, blsize,
					      NOCRED, &bp);
				if (error) {
					brelse(bp);
					return error;
				}
				ndep = bptoep(pmp, bp, ddep->de_fndoffset);
			} else {
				ndep--;
				ddep->de_fndoffset -= sizeof(struct direntry);
			}
			if (!unicode2winfn(ucfn, unichars, (struct winentry *)ndep,
					cnt++, chksum))
				break;
		}
	}

	error = bwrite(bp);
	if (error)
		return error;

    ddep->de_flag |= DE_UPDATE;
    
	/*
	 * If they want us to return with the denode gotten.
	 */
	if (depp) {
		if (dep->de_Attributes & ATTR_DIRECTORY) {
			dirclust = dep->de_StartCluster;
			if (FAT32(pmp) && dirclust == pmp->pm_rootdirblk)
				dirclust = MSDOSFSROOT;
			if (dirclust == MSDOSFSROOT)
				diroffset = MSDOSFSROOT_OFS;
			else
				diroffset = 0;
		}
		return deget(pmp, dirclust, diroffset, depp);
	}

	return 0;
}

/*
 * Be sure a directory is empty except for "." and "..". Return 1 if empty,
 * return 0 if not empty or error.
 */
int
dosdirempty(dep)
	struct denode *dep;
{
	u_long blsize;
	int error;
	u_long cn;
	daddr_t bn;
	struct buf *bp;
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;

	/*
	 * Since the filesize field in directory entries for a directory is
	 * zero, we just have to feel our way through the directory until
	 * we hit end of file.
	 */
	for (cn = 0;; cn++) {
		if ((error = pcbmap(dep, cn, 1, &bn, NULL, &blsize)) != 0) {
			if (error == E2BIG)
				return (1);	/* it's empty */
			return (0);
		}
        error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (0);
		}
		for (dentp = (struct direntry *)bp->b_data;
		     (char *)dentp < bp->b_data + blsize;
		     dentp++) {
			if (dentp->deName[0] != SLOT_DELETED &&
			    (dentp->deAttributes & ATTR_VOLUME) == 0) {
				/*
				 * In dos directories an entry whose name
				 * starts with SLOT_EMPTY (0) starts the
				 * beginning of the unused part of the
				 * directory, so we can just return that it
				 * is empty.
				 */
				if (dentp->deName[0] == SLOT_EMPTY) {
					brelse(bp);
					return (1);
				}
				/*
				 * Any names other than "." and ".." in a
				 * directory mean it is not empty.
				 */
				if (bcmp(dentp->deName, ".          ", 11) &&
				    bcmp(dentp->deName, "..         ", 11)) {
					brelse(bp);
#ifdef MSDOSFS_DEBUG
					printf("dosdirempty(): entry found %02x, %02x\n",
					    dentp->deName[0], dentp->deName[1]);
#endif
					return (0);	/* not empty */
				}
			}
		}
		brelse(bp);
	}
	/* NOTREACHED */
}

/*
 * Check to see if the directory described by target is in some
 * subdirectory of source.  This prevents something like the following from
 * succeeding and leaving a bunch or files and directories orphaned. mv
 * /a/b/c /a/b/c/d/e/f Where c and f are directories.
 *
 * source - the inode for /a/b/c
 * target - the inode for /a/b/c/d/e/f
 *
 * Returns 0 if target is NOT a subdirectory of source.
 * Otherwise returns a non-zero error number.
 * The target inode is always unlocked on return.
 */
int
doscheckpath(source, target)
	struct denode *source;
	struct denode *target;
{
	daddr_t scn;
	struct msdosfsmount *pmp;
	struct direntry *ep;
	struct denode *dep;
	struct buf *bp = NULL;
	int error = 0;

	dep = target;
	if ((target->de_Attributes & ATTR_DIRECTORY) == 0 ||
	    (source->de_Attributes & ATTR_DIRECTORY) == 0) {
		error = ENOTDIR;
		goto out;
	}
	if (dep->de_StartCluster == source->de_StartCluster) {
		error = EEXIST;
		goto out;
	}
	if (dep->de_StartCluster == MSDOSFSROOT)
		goto out;
	pmp = dep->de_pmp;
#ifdef	DIAGNOSTIC
	if (pmp != source->de_pmp)
		panic("doscheckpath: source and target on different filesystems");
#endif
	if (FAT32(pmp) && dep->de_StartCluster == pmp->pm_rootdirblk)
		goto out;

	for (;;) {
		if ((dep->de_Attributes & ATTR_DIRECTORY) == 0) {
			error = ENOTDIR;
			break;
		}
		scn = dep->de_StartCluster;
        	error = meta_bread(pmp->pm_devvp, cntobn(pmp, scn),
			      pmp->pm_bpcluster, NOCRED, &bp);
		if (error)
			break;

		ep = (struct direntry *) bp->b_data + 1;
		if ((ep->deAttributes & ATTR_DIRECTORY) == 0 ||
		    bcmp(ep->deName, "..         ", 11) != 0) {
			error = ENOTDIR;
			break;
		}
		scn = getushort(ep->deStartCluster);
		if (FAT32(pmp))
			scn |= getushort(ep->deHighClust) << 16;

		if (scn == source->de_StartCluster) {
			error = EINVAL;
			break;
		}
		if (scn == MSDOSFSROOT)
			break;
		if (FAT32(pmp) && scn == pmp->pm_rootdirblk) {
			/*
			 * scn should be 0 in this case,
			 * but we silently ignore the error.
			 */
			break;
		}

		vput(DETOV(dep));
		brelse(bp);
		bp = NULL;
		/* NOTE: deget() clears dep on error */
		if ((error = deget(pmp, scn, 0, &dep)) != 0)
			break;
	}
out:;
	if (bp)
		brelse(bp);
	if (error == ENOTDIR)
		printf("doscheckpath(): .. not a directory?\n");
	if (dep != NULL)
		vput(DETOV(dep));
	return (error);
}

/*
 * Read in the disk block containing the directory entry (dirclu, dirofs)
 * and return the address of the buf header, and the address of the
 * directory entry within the block.
 */
int
readep(pmp, dirclust, diroffset, bpp, epp)
	struct msdosfsmount *pmp;
	u_long dirclust, diroffset;
	struct buf **bpp;
	struct direntry **epp;
{
	int error;
	daddr_t bn;
	int blsize;

        /*
         * Handle the special case of the root directory.  If there is a volume
         * label entry, then get that.  Otherwise, return an error.
         */
	if ((dirclust == MSDOSFSROOT
	     || (FAT32(pmp) && dirclust == pmp->pm_rootdirblk))
	    && diroffset == MSDOSFSROOT_OFS) {
                if (pmp->pm_label_cluster == CLUST_EOFE)
                    return EIO;
                else {
                    dirclust = pmp->pm_label_cluster;
                    diroffset = pmp->pm_label_offset;
                }
        }
        
	blsize = pmp->pm_bpcluster;
	if (dirclust == MSDOSFSROOT
	    && de_blk(pmp, diroffset + blsize) > pmp->pm_rootdirsize)
		blsize = de_bn2off(pmp, pmp->pm_rootdirsize) & pmp->pm_crbomask;
	bn = detobn(pmp, dirclust, diroffset);
        if ((error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, bpp)) != 0) {
		brelse(*bpp);
		*bpp = NULL;
		return (error);
	}
	if (epp)
		*epp = bptoep(pmp, *bpp, diroffset);
	return (0);
}

/*
 * Read in the disk block containing the directory entry dep came from and
 * return the address of the buf header, and the address of the directory
 * entry within the block.
 */
int
readde(dep, bpp, epp)
	struct denode *dep;
	struct buf **bpp;
	struct direntry **epp;
{

	return (readep(dep->de_pmp, dep->de_dirclust, dep->de_diroffset,
	    bpp, epp));
}

/*
 * Remove a directory entry. At this point the file represented by the
 * directory entry to be removed is still full length until noone has it
 * open.  When the file no longer being used msdosfs_inactive() is called
 * and will truncate the file to 0 length.  When the vnode containing the
 * denode is needed for some other purpose by VFS it will call
 * msdosfs_reclaim() which will remove the denode from the denode cache.
 */
int
removede(pdep, dep)
	struct denode *pdep;	/* directory where the entry is removed */
	struct denode *dep;	/* file to be removed */
{
    int error;
    struct direntry *ep;
    struct buf *bp;
    daddr_t bn;
    u_long blsize;
    struct msdosfsmount *pmp = pdep->de_pmp;
    u_long offset = pdep->de_fndoffset;

#ifdef MSDOSFS_DEBUG
    printf("removede(): filename %s, dep %p, offset %08lx\n",
           dep->de_Name, dep, offset);
#endif

    dep->de_refcnt--;
    offset += sizeof(struct direntry);
    do {
        offset -= sizeof(struct direntry);
        error = pcbmap(pdep, de_cluster(pmp, offset), 1, &bn, NULL, &blsize);
        if (error)
            return error;
        error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp);
        if (error) {
            brelse(bp);
            return error;
        }
        ep = bptoep(pmp, bp, offset);
        /*
         * Check whether, if we came here the second time, i.e.
         * when underflowing into the previous block, the last
         * entry in this block is a longfilename entry, too.
         */
        if (ep->deAttributes != ATTR_WIN95
            && offset != pdep->de_fndoffset) {
            brelse(bp);
            break;
        }
        offset += sizeof(struct direntry);
        while (1) {
            /*
             * We are a bit agressive here in that we delete any Win95
             * entries preceding this entry, not just the ones we "own".
             * Since these presumably aren't valid anyway,
             * there should be no harm.
             */
            offset -= sizeof(struct direntry);
            ep--->deName[0] = SLOT_DELETED;
            if ((pmp->pm_flags & MSDOSFSMNT_NOWIN95)
                || !(offset & pmp->pm_crbomask)
                || ep->deAttributes != ATTR_WIN95)
                break;
        }
		error = bwrite(bp);
        if (error)
            return error;
    } while (!(pmp->pm_flags & MSDOSFSMNT_NOWIN95)
             && !(offset & pmp->pm_crbomask)
             && offset);
    pdep->de_flag |= DE_UPDATE;

    return 0;
}

/*
 * Create a unique DOS name in dvp
 */
int
uniqdosname(dep, cnp, cp)
	struct denode *dep;
	struct componentname *cnp;
	u_char *cp;
{
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;
	int gen;
	u_long blsize;
	u_long cn;
	daddr_t bn;
	struct buf *bp;
	int error;
	u_int16_t ucfn[WIN_MAXLEN];
	size_t unichars;

	/*
	 * Decode component name into Unicode
	 */
	(void) utf8_decodestr(cnp->cn_nameptr, cnp->cn_namelen, ucfn,
				&unichars, sizeof(ucfn), 0, UTF_PRECOMPOSED);
	unichars /= 2; /* bytes to chars */
	
	if (pmp->pm_flags & MSDOSFSMNT_SHORTNAME)
		return (unicode2dosfn(ucfn, cp, unichars, 0) ?
		    0 : ENAMETOOLONG);

	for (gen = 1;; gen++) {
		/*
		 * Generate DOS name with generation number
		 */
		if (!unicode2dosfn(ucfn, cp, unichars, gen))
			return gen == 1 ? ENAMETOOLONG : EEXIST;

		/*
		 * Now look for a dir entry with this exact name
		 */
		for (cn = error = 0; !error; cn++) {
			if ((error = pcbmap(dep, cn, 1, &bn, NULL, &blsize)) != 0) {
				if (error == E2BIG)	/* EOF reached and not found */
					return 0;
				return error;
			}
                	error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return error;
			}
			for (dentp = (struct direntry *)bp->b_data;
			     (char *)dentp < bp->b_data + blsize;
			     dentp++) {
				if (dentp->deName[0] == SLOT_EMPTY) {
					/*
					 * Last used entry and not found
					 */
					brelse(bp);
					return 0;
				}
				/*
				 * Ignore volume labels and Win95 entries
				 */
				if (dentp->deAttributes & ATTR_VOLUME)
					continue;
				if (!bcmp(dentp->deName, cp, 11)) {
					error = EEXIST;
					break;
				}
			}
			brelse(bp);
		}
	}
}

/*
 * Find any Win'95 long filename entry in directory dep
 */
int
findwin95(dep)
	struct denode *dep;
{
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;
        u_long blsize;
	int win95;
	u_long cn;
	daddr_t bn;
	struct buf *bp;

	win95 = 1;
	/*
	 * Read through the directory looking for Win'95 entries
	 * Note: Error currently handled just as EOF			XXX
	 */
	for (cn = 0;; cn++) {
		if (pcbmap(dep, cn, 1, &bn, NULL, &blsize))
			return (win95);
        	if (meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp)) {
			brelse(bp);
			return (win95);
		}
		for (dentp = (struct direntry *)bp->b_data;
		     (char *)dentp < bp->b_data + blsize;
		     dentp++) {
			if (dentp->deName[0] == SLOT_EMPTY) {
				/*
				 * Last used entry and not found
				 */
				brelse(bp);
				return (win95);
			}
			if (dentp->deName[0] == SLOT_DELETED) {
				/*
				 * Ignore deleted files
				 * Note: might be an indication of Win'95 anyway	XXX
				 */
				continue;
			}
			if (dentp->deAttributes == ATTR_WIN95) {
				brelse(bp);
				return 1;
			}
			win95 = 0;
		}
		brelse(bp);
	}
}

/*
 * Find room in a directory to create the entries for a given name.
 *
 * dep	- directory to search for free/unused entries
 * cnp	- the name to be created (used to determine number of slots needed).
 */
int
findslots(dep, cnp)
    struct denode *dep;
    struct componentname *cnp;
{
        int error;
	u_char dosfilename[12];
        u_int16_t ucfn[WIN_MAXLEN];
        size_t unichars;
        int wincnt;	/* Number of consecutive entries needed for long name + dir entry */
        int slotcount;	/* Number of consecutive entries found so far */
	int diroff;	/* Byte offset of entry from start of directory */
	int blkoff;	/* Byte offset of entry from start of block */
	int frcn;	/* File (directory) relative cluster number */
	daddr_t bn;	/* Physical disk block number */
	u_long cluster;	/* Physical cluster */
	u_long blsize;	/* Size of directory cluster, in bytes */
        struct direntry *entry;
	struct msdosfsmount *pmp;
	struct buf *bp;

        pmp = dep->de_pmp;

        /*
         * Decode name into UCS-2 (Unicode)
         */
        (void) utf8_decodestr(cnp->cn_nameptr, cnp->cn_namelen, ucfn, &unichars,
                                sizeof(ucfn), 0, UTF_PRECOMPOSED);
        unichars /= 2; /* bytes to chars */

        /*
         * Determine the number of consecutive directory entries we'll need.
         */
        switch (unicode2dosfn(ucfn, dosfilename, unichars, 0)) {
        case 0:
                /*
                 * The name is syntactically invalid.  Normally, we'd return EINVAL,
                 * but ENAMETOOLONG makes it clear that the name is the problem (and
                 * allows Carbon to return a more meaningful error).
                 */
                return (ENAMETOOLONG);
        case 1:
                /*
                 * The name is already a short, DOS name, so no long name entries needed.
                 */
                wincnt = 1;
                break;
        case 2:
        case 3:
                wincnt = winSlotCnt(ucfn, unichars) + 1;
                break;
        }
        if (pmp->pm_flags & MSDOSFSMNT_SHORTNAME) {
                wincnt = 1;
        }

        /*
         * Look for some consecutive unused directory entries.
         */
        slotcount = 0;		/* None found yet. */
        
        /* The outer loop ranges over the clusters in the directory. */
        diroff = 0;
        for (frcn = 0; ; frcn++) {
		error = pcbmap(dep, frcn, 1, &bn, &cluster, &blsize);
		if (error) {
			if (error == E2BIG)
				break;
			return (error);
		}
                error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}

                /* Loop over entries in the cluster. */
		for (blkoff = 0; blkoff < blsize;
		     blkoff += sizeof(struct direntry),
		     diroff += sizeof(struct direntry)) {
			entry = (struct direntry *)(bp->b_data + blkoff);
                        
                        if (entry->deName[0] == SLOT_EMPTY ||
                            entry->deName[0] == SLOT_DELETED) {
                                slotcount++;
                                if (slotcount == wincnt) {
                                        /* Found enough space! */
                                        dep->de_fndoffset = diroff;
                                        goto found;
                                }
                        } else {
                                /* Empty space wasn't big enough, so forget about it. */
                                slotcount = 0;
                        }
                }
                brelse(bp);
                bp = NULL;
        }
        /*
         * Fix up the slot description to point to where we would put the
         * DOS entry (with Win95 long name entries before that).  If we
         * would need to grow the directory, then the offset will be greater
         * than or equal to the size of the directory.
         */
found:        
	if (!slotcount) {
		slotcount = 1;
		dep->de_fndoffset = diroff;
	}
	if (wincnt > slotcount)
		dep->de_fndoffset += sizeof(struct direntry) * (wincnt - slotcount);
	dep->de_fndcnt = wincnt - 1;

	if (bp)
		brelse(bp);

	return 0;
}
