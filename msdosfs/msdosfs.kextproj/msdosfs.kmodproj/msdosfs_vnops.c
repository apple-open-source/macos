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
/* $FreeBSD: src/sys/msdosfs/msdosfs_vnops.c,v 1.99 2000/05/05 09:58:36 phk Exp $ */
/*	$NetBSD: msdosfs_vnops.c,v 1.68 1998/02/10 14:10:04 mrg Exp $	*/

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
#include <sys/vnode.h>
#include <sys/resourcevar.h>	/* defines plimit structure in proc struct */
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>
#include <sys/ubc.h>
#include <sys/utfconv.h>
#include <sys/attr.h>

#include <machine/spl.h>

#include "bpb.h"
#include "direntry.h"
#include "denode.h"
#include "msdosfsmount.h"
#include "fat.h"
#include "msdosfs_lockf.h"

extern uid_t console_user;

#define	DOS_FILESIZE_MAX	0xffffffff

#define	GENERIC_DIRSIZ(dp) \
    ((sizeof (struct dirent) - (MAXNAMLEN+1)) + (((dp)->d_namlen+1 + 3) &~ 3))

extern int groupmember(gid_t gid, struct ucred *cred);
extern int lockmgr_printinfo(struct lock__bsd__ *lkp);

/*
 * Prototypes for MSDOSFS vnode operations
 */
static int msdosfs_create __P((struct vop_create_args *));
static int msdosfs_mknod __P((struct vop_mknod_args *));
static int msdosfs_close __P((struct vop_close_args *));
static int msdosfs_access __P((struct vop_access_args *));
static int msdosfs_getattr __P((struct vop_getattr_args *));
static int msdosfs_setattr __P((struct vop_setattr_args *));
static int msdosfs_read __P((struct vop_read_args *));
static int msdosfs_write __P((struct vop_write_args *));
static int msdosfs_pagein __P((struct vop_pagein_args *));
static int msdosfs_fsync __P((struct vop_fsync_args *));
static int msdosfs_remove __P((struct vop_remove_args *));
static int msdosfs_rename __P((struct vop_rename_args *));
static int msdosfs_mkdir __P((struct vop_mkdir_args *));
static int msdosfs_rmdir __P((struct vop_rmdir_args *));
static int msdosfs_readdir __P((struct vop_readdir_args *));
static int msdosfs_bmap __P((struct vop_bmap_args *));
static int msdosfs_strategy __P((struct vop_strategy_args *));
static int msdosfs_lock __P((struct vop_lock_args *));
static int msdosfs_unlock __P((struct vop_unlock_args *));
static int msdosfs_islocked __P((struct vop_islocked_args *));
static int msdosfs_print __P((struct vop_print_args *));
static int msdosfs_pathconf __P((struct vop_pathconf_args *ap));
static int msdosfs_advlock(struct vop_advlock_args *ap);

int msdosfs_getattrlist(struct vop_getattrlist_args *ap);	/* In msdosfs_attrlist.c */

/*
 * Some general notes:
 *
 * In the ufs filesystem the inodes, superblocks, and indirect blocks are
 * read/written using the vnode for the filesystem. Blocks that represent
 * the contents of a file are read/written using the vnode for the file
 * (including directories when they are read/written as files). This
 * presents problems for the dos filesystem because data that should be in
 * an inode (if dos had them) resides in the directory itself.  Since we
 * must update directory entries without the benefit of having the vnode
 * for the directory we must use the vnode for the filesystem.  This means
 * that when a directory is actually read/written (via read, write, or
 * readdir, or seek) we must use the vnode for the filesystem instead of
 * the vnode for the directory as would happen in ufs. This is to insure we
 * retreive the correct block from the buffer cache since the hash value is
 * based upon the vnode address and the desired block number.
 */

/*
 * Create a regular file. On entry the directory to contain the file being
 * created is locked.  We must release before we return. We must also free
 * the pathname buffer pointed at by cnp->cn_pnbuf, always on error, or
 * only if the SAVESTART bit in cn_flags is clear on success.
 */
static int
msdosfs_create(ap)
	struct vop_create_args /* {        I O E
		struct vnode *a_dvp;       L U U
		struct vnode **a_vpp;      - L -
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct denode ndirent;
	struct denode *dep;
	struct denode *pdep = VTODE(ap->a_dvp);
	struct timespec ts;
	int error;
        u_long va_flags;
        
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_create(cnp %p, vap %p\n", cnp, ap->a_vap);
#endif

	/*
	 * If this is the root directory and there is no space left we
	 * can't do anything.  This is because the root directory can not
	 * change size.
	 */
	if (pdep->de_StartCluster == MSDOSFSROOT
	    && pdep->de_fndoffset >= pdep->de_FileSize) {
		error = ENOSPC;
		goto bad;
	}

	/*
	 * Create a directory entry for the file, then call createde() to
	 * have it installed. NOTE: DOS files are always executable.
         * The supplied mode is ignored (DOS doesn't store mode, so
         * all files on a volume have a constant mode).  The immutable
         * flag is used to set DOS's read-only attribute.
	 */
#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("msdosfs_create: no name");
#endif
	bzero(&ndirent, sizeof(ndirent));
	error = uniqdosname(pdep, cnp, ndirent.de_Name);
	if (error)
		goto bad;

        // Set read-only attribute if one of the immutable bits is set.
        // Always set the "needs archive" attribute on newly created files.
        va_flags = ap->a_vap->va_flags;
        if (va_flags != VNOVAL && (va_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) != 0)
            ndirent.de_Attributes = ATTR_ARCHIVE | ATTR_READONLY;
        else
            ndirent.de_Attributes = ATTR_ARCHIVE;
        
	ndirent.de_LowerCase = 0;
	ndirent.de_StartCluster = 0;
	ndirent.de_FileSize = 0;
	ndirent.de_dev = pdep->de_dev;
	ndirent.de_devvp = pdep->de_devvp;
	ndirent.de_pmp = pdep->de_pmp;
	ndirent.de_flag = DE_ACCESS | DE_CREATE | DE_UPDATE;
	getnanotime(&ts);
	DETIMES(&ndirent, &ts, &ts, &ts);
	error = createde(&ndirent, pdep, &dep, cnp);
	if (error)
		goto bad;
    if ((cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF) {
        FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    };
	*ap->a_vpp = DETOV(dep);
	vput(ap->a_dvp);
	return (0);

bad:
    	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (error);
}

static int
msdosfs_mknod(ap)
	struct vop_mknod_args /* {        I O E
		struct vnode *a_dvp;      L U U
		struct vnode **a_vpp;     - X -
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	int error;

	switch (ap->a_vap->va_type) {
	case VDIR:
		/* msdosfs_mkdir has same lock policy as msdosfs_mknod */
		return (msdosfs_mkdir((struct vop_mkdir_args *)ap));
		break;

	case VREG:
		/* msdosfs_create dvp lock policy differs */
		error = msdosfs_create((struct vop_create_args *)ap);
		vput(ap->a_dvp);
		return (error);
		break;

	default:
		vput(ap->a_dvp);
		return (EINVAL);
	}
	/* NOTREACHED */
}

static int
msdosfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}

static int
msdosfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	struct timespec ts;

	simple_lock(&vp->v_interlock);
	if (vp->v_usecount > (UBCINFOEXISTS(vp) ? 2 : 1)) {
		getnanotime(&ts);
		DETIMES(dep, &ts, &ts, &ts);
	}
	simple_unlock(&vp->v_interlock);
	return 0;
}

static int
msdosfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(ap->a_vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct ucred *cred = ap->a_cred;
	mode_t mask, file_mode, mode = ap->a_mode;
	register gid_t *gp;
	int i;

	file_mode = ALLPERMS & pmp->pm_mask;
    set_pmuid(pmp);
    
	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		default:
			break;
		}
	}

    /* If ATTR_READONLY (immutable) is set, nobody gets write access. */
    if ((mode & VWRITE) && (dep->de_Attributes & ATTR_READONLY))
        return EPERM;
        
	/* User id 0 always gets access. */
	if (cred->cr_uid == 0)
		return 0;

	mask = 0;

	/* Otherwise, check the owner. */
	/* And allow for console */
	if (cred->cr_uid == pmp->pm_uid) {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		return (file_mode & mask) == mask ? 0 : EACCES;
	}

	/* Otherwise, check the groups. */
	for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++)
		if (pmp->pm_gid == *gp) {
			if (mode & VEXEC)
				mask |= S_IXGRP;
			if (mode & VREAD)
				mask |= S_IRGRP;
			if (mode & VWRITE)
				mask |= S_IWGRP;
			return (file_mode & mask) == mask ? 0 : EACCES;
		}

	/* Otherwise, check everyone else. */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;
	return (file_mode & mask) == mask ? 0 : EACCES;
}

static int
msdosfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct denode *dep = VTODE(ap->a_vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct vattr *vap = ap->a_vap;
	struct timespec ts;
	u_long dirsperblk = pmp->pm_BytesPerSec / sizeof(struct direntry);
	u_long fileid;

	getnanotime(&ts);
	DETIMES(dep, &ts, &ts, &ts);
	vap->va_fsid = dep->de_dev;
    set_pmuid(pmp);
	/*
	 * The following computation of the fileid must be the same as that
	 * used in msdosfs_readdir() to compute d_fileno. If not, pwd
	 * doesn't work.
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY) {
		fileid = cntobn(pmp, dep->de_StartCluster) * dirsperblk;
		if (dep->de_StartCluster == MSDOSFSROOT)
			fileid = 1;
	} else {
		fileid = cntobn(pmp, dep->de_dirclust) * dirsperblk;
		if (dep->de_dirclust == MSDOSFSROOT)
			fileid = roottobn(pmp, 0) * dirsperblk;
		fileid += dep->de_diroffset / sizeof(struct direntry);
	}
	vap->va_fileid = fileid;
	vap->va_mode = ALLPERMS & pmp->pm_mask;
	vap->va_uid = pmp->pm_uid;
	vap->va_gid = pmp->pm_gid;
	vap->va_nlink = 1;
	vap->va_rdev = 0;
	vap->va_size = dep->de_FileSize;
	dos2unixtime(dep->de_MDate, dep->de_MTime, 0, &vap->va_mtime);
	if (pmp->pm_flags & MSDOSFSMNT_LONGNAME) {
		dos2unixtime(dep->de_ADate, 0, 0, &vap->va_atime);
		dos2unixtime(dep->de_CDate, dep->de_CTime, dep->de_CHun, &vap->va_ctime);
	} else {
		vap->va_atime = vap->va_mtime;
		vap->va_ctime = vap->va_mtime;
	}
	vap->va_flags = 0;
	if ((dep->de_Attributes & ATTR_ARCHIVE) == 0)	// DOS: flag set means "needs to be archived"
		vap->va_flags |= SF_ARCHIVED;				// BSD: flag set means "has been archived"
    if (dep->de_Attributes & ATTR_READONLY)			// DOS read-only becomes user immutable
        vap->va_flags |= UF_IMMUTABLE;
	vap->va_gen = 0;
	vap->va_blocksize = pmp->pm_bpcluster;
	vap->va_bytes =
	    (dep->de_FileSize + pmp->pm_crbomask) & ~pmp->pm_crbomask;
	vap->va_type = ap->a_vp->v_type;
	vap->va_filerev = dep->de_modrev;
	return (0);
}

static int
msdosfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(ap->a_vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct vattr *vap = ap->a_vap;
	struct ucred *cred = ap->a_cred;
	struct proc *p = ap->a_p;
	int error = 0;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_setattr(): vp %p, vap %p, cred %p, p %p\n",
	    ap->a_vp, vap, cred, ap->a_p);
#endif

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    (vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
#ifdef MSDOSFS_DEBUG
		printf("msdosfs_setattr(): returning EINVAL\n");
		printf("    va_type %d, va_nlink %x, va_fsid %lx, va_fileid %lx\n",
		    vap->va_type, vap->va_nlink, vap->va_fsid, vap->va_fileid);
		printf("    va_blocksize %lx, va_rdev %x, va_bytes %qx, va_gen %lx\n",
		    vap->va_blocksize, vap->va_rdev, vap->va_bytes, vap->va_gen);
		printf("    va_uid %x, va_gid %x\n",
		    vap->va_uid, vap->va_gid);
#endif
		return (EINVAL);
	}

    set_pmuid(pmp);

    if (vap->va_flags != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != pmp->pm_uid &&
		    (error = suser(cred, &p->p_acflag)))
			return (error);
		/*
		 * We are very inconsistent about handling unsupported
		 * attributes.  We ignored the access time and the
		 * mode bits.  We were strict for the other attributes.
		 *
		 * Here we are strict, stricter than ufs in not allowing
		 * users to attempt to set SF_SETTABLE bits or anyone to
		 * set unsupported bits.  However, we ignore attempts to
		 * set ATTR_ARCHIVE for directories `cp -pr' from a more
		 * sensible file system attempts it a lot.
		 */
		if (cred->cr_uid != 0) {
			if (vap->va_flags & SF_SETTABLE)
				return EPERM;
		}
        
		if (vap->va_flags & ~(SF_ARCHIVED | SF_IMMUTABLE | UF_IMMUTABLE))
			return EOPNOTSUPP;
            
		if (vap->va_flags & SF_ARCHIVED)
			dep->de_Attributes &= ~ATTR_ARCHIVE;
		else if (!(dep->de_Attributes & ATTR_DIRECTORY))
			dep->de_Attributes |= ATTR_ARCHIVE;

        /* For files, copy the immutable flag to read-only attribute. */
        /* Ignore immutable bit for directories. */
        if (!(dep->de_Attributes & ATTR_DIRECTORY))
        {
            if (vap->va_flags & (SF_IMMUTABLE | UF_IMMUTABLE))
                dep->de_Attributes |= ATTR_READONLY;
            else
                dep->de_Attributes &= ~ATTR_READONLY;
        }
        
		dep->de_flag |= DE_MODIFIED;
	}

	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		uid_t uid;
		gid_t gid;
		
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		uid = vap->va_uid;
		if (uid == (uid_t)VNOVAL)
			uid = pmp->pm_uid;
		gid = vap->va_gid;
		if (gid == (gid_t)VNOVAL)
			gid = pmp->pm_gid;
		if ((cred->cr_uid != pmp->pm_uid || uid != pmp->pm_uid ||
		    (gid != pmp->pm_gid && !groupmember(gid, cred))) &&
		    (error = suser(cred, &p->p_acflag)))
			return error;
		if (uid != pmp->pm_uid || gid != pmp->pm_gid)
			return EINVAL;
	}

	if (vap->va_size != VNOVAL) {
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the file system.
		 */
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
			/* NOT REACHED */
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		default:
			break;
		}
		error = detrunc(dep, vap->va_size, 0, cred, ap->a_p);
		if (error)
			return error;
	}
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != pmp->pm_uid &&
		    (error = suser(cred, &p->p_acflag)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 ||
		    (error = VOP_ACCESS(ap->a_vp, VWRITE, cred, ap->a_p))))
			return (error);
		if (vp->v_type != VDIR) {
			if ((pmp->pm_flags & MSDOSFSMNT_NOWIN95) == 0 &&
			    vap->va_atime.tv_sec != VNOVAL)
				unix2dostime(&vap->va_atime, &dep->de_ADate, NULL, NULL);
			if (vap->va_mtime.tv_sec != VNOVAL)
				unix2dostime(&vap->va_mtime, &dep->de_MDate, &dep->de_MTime, NULL);
			dep->de_Attributes |= ATTR_ARCHIVE;
			dep->de_flag |= DE_MODIFIED;
		}
	}
	/*
	 * DOS files only have the ability to have their writability
	 * attribute set, so we use the owner write bit to set the readonly
	 * attribute.
	 */
    /*
     * DOS doesn't store mode flags.  The code below only serves to
     * check for some error conditions where you wouldn't be able
     * to set the mode anyway.
     */
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != pmp->pm_uid &&
		    (error = suser(cred, &p->p_acflag)))
			return (error);
        
        /*
         * DOS can't store modes.  Should we return an error (which one?)
         * if the requested mode is different from the volume's default mode?
         */
	}
	return (deupdat(dep, 1));
}

static int
msdosfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error = 0;
	int blsize;
	int isadir;
	int orig_resid;
	u_int n;
	u_long diff;
	u_long on;
	daddr_t lbn;
	daddr_t rablock;
	int rasize;
	int seqcount;
	struct buf *bp;
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct uio *uio = ap->a_uio;
	int devBlockSize = 0;

	if (uio->uio_offset < 0)
		return (EINVAL);

	if ((u_int64_t)uio->uio_offset > DOS_FILESIZE_MAX)
                return (0);
	/*
	 * If they didn't ask for any data, then we are done.
	 */
	orig_resid = uio->uio_resid;
	if (orig_resid <= 0)
		return (0);

	if (UBCISVALID(vp)) {
		VOP_DEVBLOCKSIZE(dep->de_devvp, &devBlockSize);
		error = cluster_read(vp, uio, (off_t)dep->de_FileSize, devBlockSize, 0);
		if (error == 0)
			dep->de_flag |= DE_ACCESS;
		return (error);
	}

	seqcount = ap->a_ioflag >> 16;

	isadir = dep->de_Attributes & ATTR_DIRECTORY;
	do {
		if (uio->uio_offset >= dep->de_FileSize)
			break;
		lbn = de_cluster(pmp, uio->uio_offset);
		/*
		 * If we are operating on a directory file then be sure to
		 * do i/o with the vnode for the filesystem instead of the
		 * vnode for the directory.
		 */
		if (isadir) {
			/* convert cluster # to block # */
			error = pcbmap(dep, lbn, &lbn, 0, &blsize);
			if (error == E2BIG) {
				error = EINVAL;
				break;
			} else if (error)
				break;
                        error = meta_bread(pmp->pm_devvp, lbn, blsize, NOCRED, &bp);
		} else {
			blsize = pmp->pm_bpcluster;
			rablock = lbn + 1;
			if (seqcount > 1 &&
			    de_cn2off(pmp, rablock) < dep->de_FileSize) {
				rasize = pmp->pm_bpcluster;
				error = breadn(vp, lbn, blsize,
				    &rablock, &rasize, 1, NOCRED, &bp); 
			} else {
				error = bread(vp, lbn, blsize, NOCRED, &bp);
			}
		}
		if (error) {
			brelse(bp);
			break;
		}
		on = uio->uio_offset & pmp->pm_crbomask;
		diff = pmp->pm_bpcluster - on;
		n = diff > uio->uio_resid ? uio->uio_resid : diff;
		diff = dep->de_FileSize - uio->uio_offset;
		if (diff < n)
			n = diff;
		diff = blsize - bp->b_resid;
		if (diff < n)
			n = diff;
		error = uiomove(bp->b_data + on, (int) n, uio);
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	if (!isadir && (error == 0 || uio->uio_resid != orig_resid) /* &&
	    (vp->v_mount->mnt_flag & MNT_NOATIME) == 0 */)
		dep->de_flag |= DE_ACCESS;
	return (error);
}

/*
 * Write data to a file or directory.
 */
static int
msdosfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int n;
	int croffset;
	int resid;
	u_long osize;
	int error = 0;
	u_long count;
	daddr_t bn, lastcn;
	struct buf *bp;
	int ioflag = ap->a_ioflag;
	struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct vnode *vp = ap->a_vp;
	struct vnode *thisvp = NULL;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct ucred *cred = ap->a_cred;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_write(vp %p, uio %p, ioflag %x, cred %p\n",
	    vp, uio, ioflag, cred);
	printf("msdosfs_write(): diroff %lu, dirclust %lu, startcluster %lu\n",
	    dep->de_diroffset, dep->de_dirclust, dep->de_StartCluster);
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = dep->de_FileSize;
		thisvp = vp;
		break;
	case VDIR:
		return EISDIR;
	default:
		panic("msdosfs_write(): bad file type");
	}

	if (uio->uio_offset < 0)
		return (EFBIG);

	if (uio->uio_resid == 0)
		return (0);

	/*
	 * If they've exceeded their filesize limit, tell them about it.
	 */
	if (p &&
	    ((u_int64_t)uio->uio_offset + uio->uio_resid >
	    p->p_rlimit[RLIMIT_FSIZE].rlim_cur)) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}

	if ((u_int64_t)uio->uio_offset + uio->uio_resid > DOS_FILESIZE_MAX)
                return (EFBIG);

	/*
	 * If the offset we are starting the write at is beyond the end of
	 * the file, then they've done a seek.  Unix filesystems allow
	 * files with holes in them, DOS doesn't so we must fill the hole
	 * with zeroed blocks.
	 */
	/*
	 * Remember some values in case the write fails.
	 */
	resid = uio->uio_resid;
	osize = dep->de_FileSize;

	/*
	 * If we write beyond the end of the file, extend it to its ultimate
	 * size ahead of the time to hopefully get a contiguous area.
	 */
    if (uio->uio_offset + resid > osize) {
        count = de_clcount(pmp, uio->uio_offset + resid) -
        		de_clcount(pmp, osize);
        error = extendfile(dep, count);
        if (error &&  (error != ENOSPC || (ioflag & IO_UNIT)))
            goto errexit;
        lastcn = dep->de_fc[FC_LASTFC].fc_frcn;
    } else
		lastcn = de_clcount(pmp, osize) - 1;

	if (UBCISVALID(vp)) {
    	u_long filesize;
		off_t zero_off;
		int   lflag;
		int devBlockSize = 0;

        if (uio->uio_offset + resid > osize)
        	filesize = uio->uio_offset + resid;
        else
        	filesize = osize;

		lflag = (ioflag & IO_SYNC);

        if (uio->uio_offset > osize) {
        	zero_off = osize;
			lflag   |= IO_HEADZEROFILL;
		} else
			zero_off = 0;
		
		VOP_DEVBLOCKSIZE(dep->de_devvp, &devBlockSize);

		/*
		* if the write starts beyond the current EOF then
		* we we'll zero fill from the current EOF to where the write begins
		*/
#ifdef MSDOSFS_DEBUG
        if (lflag & IO_HEADZEROFILL)
       		printf("msdosfs write...zeroing from 0x%lx to 0x%lx\n",
                osize, filesize);
#endif
        error = cluster_write(vp, uio, (off_t)osize, (off_t)filesize,
					(off_t)zero_off,
					(off_t)0, devBlockSize, lflag);
		
		if (uio->uio_offset > dep->de_FileSize) {
			dep->de_FileSize = uio->uio_offset;
			ubc_setsize(vp, (off_t)dep->de_FileSize);
		}

        if (resid > uio->uio_resid)
			dep->de_flag |= DE_UPDATE;
	
		goto errexit;
	}
    
    
	do {
		if (de_cluster(pmp, uio->uio_offset) > lastcn) {
			error = ENOSPC;
			break;
		}

		croffset = uio->uio_offset & pmp->pm_crbomask;
		n = min(uio->uio_resid, pmp->pm_bpcluster - croffset);
		if (uio->uio_offset + n > dep->de_FileSize) {
			dep->de_FileSize = uio->uio_offset + n;
		}

		bn = de_cluster(pmp, uio->uio_offset);
		if ((uio->uio_offset & pmp->pm_crbomask) == 0
		    && (de_cluster(pmp, uio->uio_offset + uio->uio_resid) 
		        > de_cluster(pmp, uio->uio_offset)
			|| uio->uio_offset + uio->uio_resid >= dep->de_FileSize)) {
			/*
			 * If either the whole cluster gets written,
			 * or we write the cluster from its start beyond EOF,
			 * then no need to read data from disk.
			 */
			bp = getblk(thisvp, bn, pmp->pm_bpcluster, 0, 0, BLK_META);
			clrbuf(bp);
			/*
			 * Do the bmap now, since pcbmap needs buffers
			 * for the fat table. (see msdosfs_strategy)
			 */
			if (bp->b_blkno == bp->b_lblkno) {
				error = pcbmap(dep, bp->b_lblkno, &bp->b_blkno, 
				     0, 0);
				if (error)
					bp->b_blkno = -1;
			}
			if (bp->b_blkno == -1) {
				brelse(bp);
				if (!error)
					error = EIO;		/* XXX */
				break;
			}
		} else {
			/*
			 * The block we need to write into exists, so read it in.
			 */
			error = bread(thisvp, bn, pmp->pm_bpcluster, cred, &bp);
			if (error) {
				brelse(bp);
				break;
			}
		}

		/*
		 * Should these vnode_pager_* functions be done on dir
		 * files?
		 */

		/*
		 * Copy the data from user space into the buf header.
		 */
		error = uiomove(bp->b_data + croffset, n, uio);
		if (error) {
			brelse(bp);
			break;
		}

		/*
		 * If they want this synchronous then write it and wait for
		 * it.  Otherwise, if on a cluster boundary write it
		 * asynchronously so we can move on to the next block
		 * without delay.  Otherwise do a delayed write because we
		 * may want to write somemore into the block later.
		 */
		if (ioflag & IO_SYNC)
			(void) bwrite(bp);
		else if (n + croffset == pmp->pm_bpcluster)
			bawrite(bp);
		else
			bdwrite(bp);
		dep->de_flag |= DE_UPDATE;
	} while (error == 0 && uio->uio_resid > 0);

	/*
	 * If the write failed and they want us to, truncate the file back
	 * to the size it was before the write was attempted.
	 */
errexit:
	if (error) {
		if (ioflag & IO_UNIT) {
			detrunc(dep, osize, ioflag & IO_SYNC, NOCRED, NULL);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		} else {
			detrunc(dep, dep->de_FileSize, ioflag & IO_SYNC, NOCRED, NULL);
			if (uio->uio_resid != resid)
				error = 0;
		}
	} else if (ioflag & IO_SYNC)
		error = deupdat(dep, 1);
	return (error);
}

static int
msdosfs_pagein(ap)
	struct vop_pagein_args /* {
	   	struct vnode 	*a_vp,
	   	upl_t		a_pl,
		vm_offset_t	a_pl_offset,
		off_t		a_f_offset,
		size_t		a_size,
		struct ucred	*a_cred,
		int		a_flags
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep;
	int devBlockSize = 0;
	int error;

	dep = VTODE(vp);

	if  (UBCINVALID(vp))
		panic("msdosfs_pagein: Not a  VREG");
	UBCINFOCHECK("msdosfs_pagein", vp);

	VOP_DEVBLOCKSIZE(dep->de_devvp, &devBlockSize);

  	error = cluster_pagein(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
  				ap->a_size, (off_t)dep->de_FileSize, devBlockSize,
  				ap->a_flags);
	return (error);
}

static int
msdosfs_pageout(ap)
	struct vop_pageout_args /* {
	   struct vnode *a_vp,
	   upl_t         a_pl,
	   vm_offset_t   a_pl_offset,
	   off_t         a_f_offset,
	   size_t        a_size,
	   struct ucred *a_cred,
	   int           a_flags
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep;
	int devBlockSize = 0;
	int error;

	dep = VTODE(vp);

	if (UBCINVALID(vp))
		panic("msdosfs_pageout: Not a  VREG: vp=%x", vp);

	VOP_DEVBLOCKSIZE(dep->de_devvp, &devBlockSize);

	error = cluster_pageout(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
				ap->a_size, (off_t)dep->de_FileSize, devBlockSize,
				ap->a_flags);

	return (error);
}

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
msdosfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int s;
	struct buf *bp, *nbp;

	/*
	 * First of all, write out any clusters.
	 */
	cluster_push(vp);

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if ((bp->b_flags & B_BUSY))
			continue;
	//	if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT))
	//		continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("msdosfs_fsync: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		(void) bwrite(bp);
		goto loop;
	}
	if (vp->v_flag & VHASDIRTY)
		ubc_pushdirty(vp);

	while (vp->v_numoutput) {
		vp->v_flag |= VBWAIT;
		(void) tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "msdosfsn", 0);
	}
#ifdef DIAGNOSTIC
	if (vp->v_dirtyblkhd.lh_first || (vp->v_flag & VHASDIRTY)) {
		vprint("msdosfs_fsync: dirty", vp);
		splx(s);
		goto loop;
	}
#endif
	splx(s);
	return (deupdat(VTODE(vp), ap->a_waitfor == MNT_WAIT));
}

static int
msdosfs_remove(ap)
	struct vop_remove_args /* {        I O E
		struct vnode *a_dvp;       L U U
		struct vnode *a_vp;        L U U
		struct componentname *a_cnp;
	} */ *ap;
{
    struct denode *dep = VTODE(ap->a_vp);
    struct denode *ddep = VTODE(ap->a_dvp);
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;
    int error;

    if (ap->a_vp->v_type == VDIR)
    {
        error = EPERM;
        goto out;
    }

    /* [2877546] Make sure the file isn't read-only */
    if (dep->de_Attributes & ATTR_READONLY)
    {
        error = EPERM;
        goto out;
    }

    error = removede(ddep, dep);

out:
	/* No matter what, unlock everything */
    if (dvp == vp)
        vrele(vp);
    else
        vput(vp);
    vput(dvp);

    /* Set from removede above */
    if (error)
        return (error);

    if (UBCINFOEXISTS(vp)) {
        (void) ubc_uncache(vp);
        /* WARNING vp may not be valid after this */
    }

    return (error);
}

/*
 * Renames on files require moving the denode to a new hash queue since the
 * denode's location is used to compute which hash queue to put the file
 * in. Unless it is a rename in place.  For example "mv a b".
 *
 * What follows is the basic algorithm:
 *
 * if (file move) {
 *	if (dest file exists) {
 *		remove dest file
 *	}
 *	if (dest and src in same directory) {
 *		rewrite name in existing directory slot
 *	} else {
 *		write new entry in dest directory
 *		update offset and dirclust in denode
 *		move denode to new hash chain
 *		clear old directory entry
 *	}
 * } else {
 *	directory move
 *	if (dest directory exists) {
 *		if (dest is not empty) {
 *			return ENOTEMPTY
 *		}
 *		remove dest directory
 *	}
 *	if (dest and src in same directory) {
 *		rewrite name in existing entry
 *	} else {
 *		be sure dest is not a child of src directory
 *		write entry in dest directory
 *		update "." and ".." in moved directory
 *		clear old directory entry for moved directory
 *	}
 * }
 *
 * On entry:
 *	source's parent directory is unlocked
 *	source file or directory is unlocked
 *	destination's parent directory is locked
 *	destination file or directory is locked if it exists
 *
 * On exit:
 *	all denodes should be released
 *
 * Notes:
 * I'm not sure how the memory containing the pathnames pointed at by the
 * componentname structures is freed, there may be some memory bleeding
 * for each rename done.
 */
static int
msdosfs_rename(ap)
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tvp = ap->a_tvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct proc *p = fcnp->cn_proc;
	struct denode *ip, *xp, *dp, *zp;
	u_char toname[11], oldname[11];
	u_long from_diroffset, to_diroffset;
	u_char to_count;
	int doingdirectory = 0, newparent = 0;
	int error;
	u_long cn;
	daddr_t bn = 0;
	struct denode *fddep;	/* from file's parent directory	 */
	struct denode *fdep;	/* from file or directory	 */
	struct denode *tddep;	/* to file's parent directory	 */
	struct denode *tdep;	/* to file or directory		 */
	struct msdosfsmount *pmp;
	struct direntry *dotdotp;
	struct buf *bp;

	fddep = VTODE(ap->a_fdvp);
	fdep = VTODE(ap->a_fvp);
	tddep = VTODE(ap->a_tdvp);
	tdep = tvp ? VTODE(tvp) : NULL;
	pmp = fddep->de_pmp;

	pmp = VFSTOMSDOSFS(fdvp->v_mount);

#ifdef DIAGNOSTIC
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("msdosfs_rename: no name");
#endif
	/*
	 * Check for cross-device rename.
	 */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
abortit:
        VOP_ABORTOP(ap->a_tdvp, ap->a_tcnp);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(ap->a_fdvp, ap->a_fcnp);
		vrele(fdvp);
		vrele(fvp);
		return (error);
	}

	/*
	 * If source and dest are the same, do nothing.
	 */
	if (tvp == fvp) {
		error = 0;
		goto abortit;
	}

    /* [2877546] Make sure the destination vnode (if any) can be deleted */
    if (tvp && (tdep->de_Attributes & ATTR_READONLY))
    {
        error = EPERM;
        goto abortit;
    }
    
	error = vn_lock(fvp, LK_EXCLUSIVE, p);
	if (error)
		goto abortit;
	dp = VTODE(fdvp);
	ip = VTODE(fvp);

    /* [2877546] Make sure the source vnode can be changed */
    if (ip->de_Attributes & ATTR_READONLY)
    {
        VOP_UNLOCK(fvp, 0, p);
        error = EPERM;
        goto abortit;
    }
    
	/*
	 * Be sure we are not renaming ".", "..", or an alias of ".". This
	 * leads to a crippled directory tree.  It's pretty tough to do a
	 * "ls" or "pwd" with the "." directory entry missing, and "cd .."
	 * doesn't work if the ".." entry is missing.
	 */
	if (ip->de_Attributes & ATTR_DIRECTORY) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    dp == ip ||
		    (fcnp->cn_flags & ISDOTDOT) ||
		    (tcnp->cn_flags & ISDOTDOT) ||
		    (ip->de_flag & DE_RENAME)) {
			VOP_UNLOCK(fvp, 0, p);
			error = EINVAL;
			goto abortit;
		}
		ip->de_flag |= DE_RENAME;
		doingdirectory++;
	}

	/*
	 * When the target exists, both the directory
	 * and target vnodes are returned locked.
	 */
	dp = VTODE(tdvp);
	xp = tvp ? VTODE(tvp) : NULL;
	/*
	 * Remember direntry place to use for destination
	 */
	to_diroffset = dp->de_fndoffset;
	to_count = dp->de_fndcnt;

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory heirarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call
	 * to namei, as the parent directory is unlocked by the
	 * call to doscheckpath().
	 */
	error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_proc);
	VOP_UNLOCK(fvp, 0, p);
	if (VTODE(fdvp)->de_StartCluster != VTODE(tdvp)->de_StartCluster)
		newparent = 1;
	if (doingdirectory && newparent) {
		if (error)	/* write access check above */
			goto bad;
		if (xp != NULL)
			vput(tvp);
		/*
		 * doscheckpath() vput()'s dp,
		 * so we have to do a relookup afterwards
		 */
		error = doscheckpath(ip, dp);
		if (error)
			goto out;
		if ((tcnp->cn_flags & SAVESTART) == 0)
			panic("msdosfs_rename: lost to startdir");
		error = relookup(tdvp, &tvp, tcnp);
		if (error)
			goto out;
		dp = VTODE(tdvp);
		xp = tvp ? VTODE(tvp) : NULL;
	}

	if (xp != NULL) {
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if (xp->de_Attributes & ATTR_DIRECTORY) {
			if (!dosdirempty(xp)) {
				error = ENOTEMPTY;
				goto bad;
			}
			if (!doingdirectory) {
				error = ENOTDIR;
				goto bad;
			}
			cache_purge(tdvp);
		} else if (doingdirectory) {
			error = EISDIR;
			goto bad;
		}
		error = removede(dp, xp);
		if (error)
			goto bad;
		vput(tvp);
		xp = NULL;
	}

	/*
	 * Convert the filename in tcnp into a dos filename. We copy this
	 * into the denode and directory entry for the destination
	 * file/directory.
	 */
	error = uniqdosname(VTODE(tdvp), tcnp, toname);
	if (error)
		goto abortit;

	/*
	 * Since from wasn't locked at various places above,
	 * have to do a relookup here.
	 */
	fcnp->cn_flags &= ~MODMASK;
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
	if ((fcnp->cn_flags & SAVESTART) == 0)
		panic("msdosfs_rename: lost from startdir");
	if (!newparent)
		VOP_UNLOCK(tdvp, 0, p);
    if (relookup(fdvp, &fvp, fcnp) == 0)
        vrele(fdvp);
	if (fvp == NULL) {
		/*
		 * From name has disappeared.
		 */
		if (doingdirectory)
			panic("rename: lost dir entry");
		vrele(ap->a_fvp);
		if (newparent)
			VOP_UNLOCK(tdvp, 0, p);
		vrele(tdvp);
		return 0;
	}
	xp = VTODE(fvp);
	zp = VTODE(fdvp);
	from_diroffset = zp->de_fndoffset;

	/*
	 * Ensure that the directory entry still exists and has not
	 * changed till now. If the source is a file the entry may
	 * have been unlinked or renamed. In either case there is
	 * no further work to be done. If the source is a directory
	 * then it cannot have been rmdir'ed or renamed; this is
	 * prohibited by the DE_RENAME flag.
	 */
	if (xp != ip) {
		if (doingdirectory)
			panic("rename: lost dir entry");
		vrele(ap->a_fvp);
		VOP_UNLOCK(fvp, 0, p);
		if (newparent)
			VOP_UNLOCK(fdvp, 0, p);
		xp = NULL;
	} else {
		vrele(fvp);
		xp = NULL;

		/*
		 * First write a new entry in the destination
		 * directory and mark the entry in the source directory
		 * as deleted.  Then move the denode to the correct hash
		 * chain for its new location in the filesystem.  And, if
		 * we moved a directory, then update its .. entry to point
		 * to the new parent directory.
		 */
		bcopy(ip->de_Name, oldname, 11);
		bcopy(toname, ip->de_Name, 11);	/* update denode */
		dp->de_fndoffset = to_diroffset;
		dp->de_fndcnt = to_count;
		error = createde(ip, dp, (struct denode **)0, tcnp);
		if (error) {
			bcopy(oldname, ip->de_Name, 11);
			if (newparent)
				VOP_UNLOCK(fdvp, 0, p);
			VOP_UNLOCK(fvp, 0, p);
			goto bad;
		}
		ip->de_refcnt++;
		zp->de_fndoffset = from_diroffset;
		error = removede(zp, ip);
		if (error) {
			/* XXX should really panic here, fs is corrupt */
			if (newparent)
				VOP_UNLOCK(fdvp, 0, p);
			VOP_UNLOCK(fvp, 0, p);
			goto bad;
		}
		if (!doingdirectory) {
			error = pcbmap(dp, de_cluster(pmp, to_diroffset), 0,
				       &ip->de_dirclust, 0);
			if (error) {
				/* XXX should really panic here, fs is corrupt */
				if (newparent)
					VOP_UNLOCK(fdvp, 0, p);
				VOP_UNLOCK(fvp, 0, p);
				goto bad;
			}
			if (ip->de_dirclust == MSDOSFSROOT)
				ip->de_diroffset = to_diroffset;
			else
				ip->de_diroffset = to_diroffset & pmp->pm_crbomask;
		}
		reinsert(ip);
		if (newparent)
			VOP_UNLOCK(fdvp, 0, p);
	}

	/*
	 * If we moved a directory to a new parent directory, then we must
	 * fixup the ".." entry in the moved directory.
	 */
	if (doingdirectory && newparent) {
		cn = ip->de_StartCluster;
		if (cn == MSDOSFSROOT) {
			/* this should never happen */
			panic("msdosfs_rename(): updating .. in root directory?");
		} else
			bn = cntobn(pmp, cn);
                error = meta_bread(pmp->pm_devvp, bn, pmp->pm_bpcluster,
			      NOCRED, &bp);
		if (error) {
			/* XXX should really panic here, fs is corrupt */
			brelse(bp);
			VOP_UNLOCK(fvp, 0, p);
			goto bad;
		}
		dotdotp = (struct direntry *)bp->b_data + 1;
		putushort(dotdotp->deStartCluster, dp->de_StartCluster);
		if (FAT32(pmp))
			putushort(dotdotp->deHighClust, dp->de_StartCluster >> 16);
		error = bwrite(bp);
		if (error) {
			/* XXX should really panic here, fs is corrupt */
			VOP_UNLOCK(fvp, 0, p);
			goto bad;
		}
	}

	VOP_UNLOCK(fvp, 0, p);
bad:
	if (xp)
		vput(tvp);
	vput(tdvp);
out:
	ip->de_flag &= ~DE_RENAME;
	vrele(fdvp);
	vrele(fvp);
	return (error);

}

static struct {
	struct direntry dot;
	struct direntry dotdot;
} dosdirtemplate = {
	{	".       ", "   ",			/* the . entry */
		ATTR_DIRECTORY,				/* file attribute */
		0,	 				/* reserved */
		0, { 0, 0 }, { 0, 0 },			/* create time & date */
		{ 0, 0 },				/* access date */
		{ 0, 0 },				/* high bits of start cluster */
		{ 210, 4 }, { 210, 4 },			/* modify time & date */
		{ 0, 0 },				/* startcluster */
		{ 0, 0, 0, 0 } 				/* filesize */
	},
	{	"..      ", "   ",			/* the .. entry */
		ATTR_DIRECTORY,				/* file attribute */
		0,	 				/* reserved */
		0, { 0, 0 }, { 0, 0 },			/* create time & date */
		{ 0, 0 },				/* access date */
		{ 0, 0 },				/* high bits of start cluster */
		{ 210, 4 }, { 210, 4 },			/* modify time & date */
		{ 0, 0 },				/* startcluster */
		{ 0, 0, 0, 0 }				/* filesize */
	}
};

static int
msdosfs_mkdir(ap)
	struct vop_mkdir_args /* {        I O E
		struct vnode *a_dvp;      L U U
		struvt vnode **a_vpp;     - L -
		struvt componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct denode *dep;
	struct denode *pdep = VTODE(ap->a_dvp);
	struct direntry *denp;
	struct msdosfsmount *pmp = pdep->de_pmp;
	struct buf *bp;
	u_long newcluster, pcl;
	int bn;
	int error;
	struct denode ndirent;
	struct timespec ts;

	/*
	 * If this is the root directory and there is no space left we
	 * can't do anything.  This is because the root directory can not
	 * change size.
	 */
	if (pdep->de_StartCluster == MSDOSFSROOT
	    && pdep->de_fndoffset >= pdep->de_FileSize) {
		error = ENOSPC;
		goto bad2;
	}

	/*
	 * Allocate a cluster to hold the about to be created directory.
	 */
	error = clusteralloc(pmp, 0, 1, CLUST_EOFE, &newcluster, NULL);
	if (error)
		goto bad2;

	bzero(&ndirent, sizeof(ndirent));
	ndirent.de_pmp = pmp;
	ndirent.de_flag = DE_ACCESS | DE_CREATE | DE_UPDATE;
	getnanotime(&ts);
	DETIMES(&ndirent, &ts, &ts, &ts);

	/*
	 * Now fill the cluster with the "." and ".." entries. And write
	 * the cluster to disk.  This way it is there for the parent
	 * directory to be pointing at if there were a crash.
	 */
	bn = cntobn(pmp, newcluster);
	/* always succeeds */
	bp = getblk(pmp->pm_devvp, bn, pmp->pm_bpcluster, 0, 0, BLK_META);
	bzero(bp->b_data, pmp->pm_bpcluster);
	bcopy(&dosdirtemplate, bp->b_data, sizeof dosdirtemplate);
	denp = (struct direntry *)bp->b_data;
	putushort(denp[0].deStartCluster, newcluster);
	putushort(denp[0].deCDate, ndirent.de_CDate);
	putushort(denp[0].deCTime, ndirent.de_CTime);
	denp[0].deCHundredth = ndirent.de_CHun;
	putushort(denp[0].deADate, ndirent.de_ADate);
	putushort(denp[0].deMDate, ndirent.de_MDate);
	putushort(denp[0].deMTime, ndirent.de_MTime);
	pcl = pdep->de_StartCluster;
	if (FAT32(pmp) && pcl == pmp->pm_rootdirblk)
		pcl = 0;
	putushort(denp[1].deStartCluster, pcl);
	putushort(denp[1].deCDate, ndirent.de_CDate);
	putushort(denp[1].deCTime, ndirent.de_CTime);
	denp[1].deCHundredth = ndirent.de_CHun;
	putushort(denp[1].deADate, ndirent.de_ADate);
	putushort(denp[1].deMDate, ndirent.de_MDate);
	putushort(denp[1].deMTime, ndirent.de_MTime);
	if (FAT32(pmp)) {
		putushort(denp[0].deHighClust, newcluster >> 16);
		putushort(denp[1].deHighClust, pdep->de_StartCluster >> 16);
	}

    if (pmp->pm_flags & MSDOSFSMNT_WAITONFAT)
        error = bwrite(bp);
    else
        bdwrite(bp);	// error = 0 from above
	if (error)
		goto bad;

	/*
	 * Now build up a directory entry pointing to the newly allocated
	 * cluster.  This will be written to an empty slot in the parent
	 * directory.
	 */
#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("msdosfs_mkdir: no name");
#endif
	error = uniqdosname(pdep, cnp, ndirent.de_Name);
	if (error)
		goto bad;

	ndirent.de_Attributes = ATTR_DIRECTORY;
	ndirent.de_LowerCase = 0;
	ndirent.de_StartCluster = newcluster;
	ndirent.de_FileSize = 0;
	ndirent.de_dev = pdep->de_dev;
	ndirent.de_devvp = pdep->de_devvp;
	error = createde(&ndirent, pdep, &dep, cnp);
	if (error)
		goto bad;
    if ((cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF) {
        FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    };
	*ap->a_vpp = DETOV(dep);
	vput(ap->a_dvp);
	return (0);

bad:
	clusterfree(pmp, newcluster, NULL);
bad2:
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (error);
}

static int
msdosfs_rmdir(ap)
	struct vop_rmdir_args /* {        I O E
		struct vnode *a_dvp;      L U U
		struct vnode *a_vp;       L U U
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct vnode *dvp = ap->a_dvp;
	register struct componentname *cnp = ap->a_cnp;
	register struct denode *ip, *dp;
	struct proc *p = cnp->cn_proc;
	int error;
	
	ip = VTODE(vp);
	dp = VTODE(dvp);
	/*
	 * No rmdir "." please.
	 */
	if (dp == ip) {
		vrele(dvp);
		vput(vp);
		return (EINVAL);
	}

	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	error = 0;
	if (!dosdirempty(ip) || ip->de_flag & DE_RENAME) {
		error = ENOTEMPTY;
		goto out;
	}

    /* [2877546] Make sure the directory isn't read-only */
    if (ip->de_Attributes & ATTR_READONLY)
    {
        error = EPERM;
        goto out;
    }
    
	/*
	 * Delete the entry from the directory.  For dos filesystems this
	 * gets rid of the directory entry on disk, the in memory copy
	 * still exists but the de_refcnt is <= 0.  This prevents it from
	 * being found by deget().  When the vput() on dep is done we give
	 * up access and eventually msdosfs_reclaim() will be called which
	 * will remove it from the denode cache.
	 */
	error = removede(dp, ip);
	if (error)
		goto out;
	/*
	 * This is where we decrement the link count in the parent
	 * directory.  Since dos filesystems don't do this we just purge
	 * the name cache.
	 */
	cache_purge(dvp);
	VOP_UNLOCK(dvp, 0, p);
	/*
	 * Truncate the directory that is being deleted.
	 */
	error = detrunc(ip, (u_long)0, IO_SYNC, cnp->cn_cred, p);
	cache_purge(vp);

	vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, p);
out:
	if (dvp)
		vput(dvp);
	vput(vp);
	return (error);
}

static int
msdosfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	int error = 0;
	int diff;
	long n;
	int blsize;
	long on;
	u_long cn;
	u_long fileno;
	u_long dirsperblk;
	long bias = 0;
	daddr_t bn, lbn;
	struct buf *bp;
	struct denode *dep = VTODE(ap->a_vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;
	struct dirent dirbuf;
	struct uio *uio = ap->a_uio;
	u_long *cookies = NULL;
	int ncookies = 0;
	off_t offset, off;
	int chksum = -1;
	u_int16_t ucfn[WIN_MAXLEN + 1];
	u_int16_t unichars;
	size_t outbytes;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_readdir(): vp %p, uio %p, cred %p, eofflagp %p\n",
	    ap->a_vp, uio, ap->a_cred, ap->a_eofflag);
#endif

	/*
	 * msdosfs_readdir() won't operate properly on regular files since
	 * it does i/o only with the the filesystem vnode, and hence can
	 * retrieve the wrong block from the buffer cache for a plain file.
	 * So, fail attempts to readdir() on a plain file.
	 */
	if ((dep->de_Attributes & ATTR_DIRECTORY) == 0)
		return (ENOTDIR);

	/*
	 * To be safe, initialize dirbuf
	 */
	bzero(dirbuf.d_name, sizeof(dirbuf.d_name));

	/*
	 * If the user buffer is smaller than the size of one dos directory
	 * entry or the file offset is not a multiple of the size of a
	 * directory entry, then we fail the read.
	 */
	off = offset = uio->uio_offset;
	if (uio->uio_resid < sizeof(struct direntry) ||
	    (offset & (sizeof(struct direntry) - 1)))
		return (EINVAL);

	if (ap->a_ncookies) {
		ncookies = uio->uio_resid / 16;
		MALLOC(cookies, u_long *, ncookies * sizeof(u_long), M_TEMP,
		       M_WAITOK);
		*ap->a_cookies = cookies;
		*ap->a_ncookies = ncookies;
	}

	dirsperblk = pmp->pm_BytesPerSec / sizeof(struct direntry);

	/*
	 * If they are reading from the root directory then, we simulate
	 * the . and .. entries since these don't exist in the root
	 * directory.  We also set the offset bias to make up for having to
	 * simulate these entries. By this I mean that at file offset 64 we
	 * read the first entry in the root directory that lives on disk.
	 */
	if (dep->de_StartCluster == MSDOSFSROOT
	    || (FAT32(pmp) && dep->de_StartCluster == pmp->pm_rootdirblk)) {
#ifdef DEBUG
		printf("msdosfs_readdir(): going after . or .. in root dir, offset %d\n",
		    offset);
#endif
		bias = 2 * sizeof(struct direntry);
		if (offset < bias) {
			for (n = (int)offset / sizeof(struct direntry);
			     n < 2; n++) {
				if (FAT32(pmp))
					dirbuf.d_fileno = cntobn(pmp,
								 pmp->pm_rootdirblk)
							  * dirsperblk;
				else
					dirbuf.d_fileno = 1;
				dirbuf.d_type = DT_DIR;
				switch (n) {
				case 0:
					dirbuf.d_namlen = 1;
					strcpy(dirbuf.d_name, ".");
					break;
				case 1:
					dirbuf.d_namlen = 2;
					strcpy(dirbuf.d_name, "..");
					break;
				}
				dirbuf.d_reclen = GENERIC_DIRSIZ(&dirbuf);
				if (uio->uio_resid < dirbuf.d_reclen)
					goto out;
				error = uiomove((caddr_t) &dirbuf,
						dirbuf.d_reclen, uio);
				if (error)
					goto out;
				offset += sizeof(struct direntry);
				off = offset;
				if (cookies) {
					*cookies++ = offset;
					if (--ncookies <= 0)
						goto out;
				}
			}
		}
	}

	off = offset;
	while (uio->uio_resid > 0) {
		lbn = de_cluster(pmp, offset - bias);
		on = (offset - bias) & pmp->pm_crbomask;
		n = min(pmp->pm_bpcluster - on, uio->uio_resid);
		diff = dep->de_FileSize - (offset - bias);
		if (diff <= 0)
			break;
		n = min(n, diff);
		error = pcbmap(dep, lbn, &bn, &cn, &blsize);
		if (error)
			break;
		error = meta_bread(pmp->pm_devvp, bn, blsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}
		n = min(n, blsize - bp->b_resid);

		/*
		 * Convert from dos directory entries to fs-independent
		 * directory entries.
		 */
		for (dentp = (struct direntry *)(bp->b_data + on);
		     (char *)dentp < bp->b_data + on + n;
		     dentp++, offset += sizeof(struct direntry)) {
#ifdef DEBUG
			printf("rd: dentp %08x prev %08x crnt %08x deName %02x attr %02x\n",
			    dentp, prev, crnt, dentp->deName[0], dentp->deAttributes);
#endif
			/*
			 * If this is an unused entry, we can stop.
			 */
			if (dentp->deName[0] == SLOT_EMPTY) {
				brelse(bp);
				goto out;
			}
			/*
			 * Skip deleted entries.
			 */
			if (dentp->deName[0] == SLOT_DELETED) {
				chksum = -1;
				continue;
			}

			/*
			 * Handle Win95 long directory entries
			 */
			if (dentp->deAttributes == ATTR_WIN95) {
				if (pmp->pm_flags & MSDOSFSMNT_SHORTNAME)
					continue;
				chksum = getunicodefn((struct winentry *)dentp,
						ucfn, &unichars, chksum);
				continue;
			}

			/*
			 * Skip volume labels
			 */
			if (dentp->deAttributes & ATTR_VOLUME) {
				chksum = -1;
				continue;
			}
			/*
			 * This computation of d_fileno must match
			 * the computation of va_fileid in
			 * msdosfs_getattr.
			 */
			if (dentp->deAttributes & ATTR_DIRECTORY) {
				fileno = getushort(dentp->deStartCluster);
				if (FAT32(pmp))
					fileno |= getushort(dentp->deHighClust) << 16;
				/* if this is the root directory */
				if (fileno == MSDOSFSROOT)
					if (FAT32(pmp))
						fileno = cntobn(pmp,
								pmp->pm_rootdirblk)
							 * dirsperblk;
					else
						fileno = 1;
				else
					fileno = cntobn(pmp, fileno) * dirsperblk;
				dirbuf.d_fileno = fileno;
				dirbuf.d_type = DT_DIR;
			} else {
				dirbuf.d_fileno = offset / sizeof(struct direntry);
				dirbuf.d_type = DT_REG;
			}
			if (chksum != winChksum(dentp->deName)) {
				unichars = dos2unicodefn(dentp->deName, ucfn,
				    dentp->deLowerCase |
					((pmp->pm_flags & MSDOSFSMNT_SHORTNAME) ?
					(LCASE_BASE | LCASE_EXT) : 0),
				    pmp->pm_flags & MSDOSFSMNT_U2WTABLE,
				    pmp->pm_d2u,
				    pmp->pm_flags & MSDOSFSMNT_ULTABLE,
				    pmp->pm_ul);
			}
			
			/* translate the name in ucfn into UTF-8 */
			(void) utf8_encodestr(ucfn, unichars * 2,
					dirbuf.d_name, &outbytes,
					sizeof(dirbuf.d_name), 0, UTF_DECOMPOSED);
			dirbuf.d_namlen = outbytes;
			chksum = -1;
			dirbuf.d_reclen = GENERIC_DIRSIZ(&dirbuf);

			if (uio->uio_resid < dirbuf.d_reclen) {
				brelse(bp);
				goto out;
			}
			error = uiomove((caddr_t) &dirbuf,
					dirbuf.d_reclen, uio);
			if (error) {
				brelse(bp);
				goto out;
			}
			if (cookies) {
				*cookies++ = offset + sizeof(struct direntry);
				if (--ncookies <= 0) {
					brelse(bp);
					goto out;
				}
			}
			off = offset + sizeof(struct direntry);
		}
		brelse(bp);
	}
out:
	/* Subtract unused cookies */
	if (ap->a_ncookies)
		*ap->a_ncookies -= ncookies;

	uio->uio_offset = off;

	/*
	 * Set the eofflag (NFS uses it)
	 */
	if (ap->a_eofflag) {
		if (dep->de_FileSize - (offset - bias) <= 0)
			*ap->a_eofflag = 1;
		else
			*ap->a_eofflag = 0;
	}
	return (error);
}

/*
 * vp  - address of vnode file the file
 * bn  - which cluster we are interested in mapping to a filesystem block number.
 * vpp - returns the vnode for the block special file holding the filesystem
 *	 containing the file of interest
 * bnp - address of where to return the filesystem relative block number
 */
static int
msdosfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap;
{
	struct denode *dep = VTODE(ap->a_vp);

	if (ap->a_vpp != NULL)
		*ap->a_vpp = dep->de_devvp;
	if (ap->a_bnp == NULL)
		return (0);
	if (ap->a_runp) {
		/*
		 * Sequential clusters should be counted here.
		 */
		*ap->a_runp = 0;
	}

	return (pcbmap(dep, ap->a_bn, ap->a_bnp, 0, 0));
}

/* blktooff converts a logical block number to a file offset */
int
msdosfs_blktooff(ap)
	struct vop_blktooff_args /* {
		struct vnode *a_vp;
		daddr_t a_lblkno;
		off_t *a_offset;    
	} */ *ap;
{
	if (ap->a_vp == NULL)
		return (EINVAL);
	*ap->a_offset = (off_t)ap->a_lblkno * PAGE_SIZE_64;

	return(0);
}

/* offtoblk converts a file offset to a logical block number */
int
msdosfs_offtoblk(ap)
struct vop_offtoblk_args /* {
	struct vnode *a_vp;
	off_t a_offset;    
	daddr_t *a_lblkno;
	} */ *ap;
{

    if (ap->a_vp == NULL)
        return (EINVAL);
    *ap->a_lblkno = ap->a_offset / PAGE_SIZE_64;

    return(0);
}

int
msdosfs_cmap(ap)
struct vop_cmap_args /* {
	struct vnode *a_vp;
	off_t a_foffset;    
	size_t a_size;
	daddr_t *a_bpn;
	size_t *a_run;
	void *a_poff;
} */ *ap;
{
	int blksiz;
	int error;
	struct denode *dep = VTODE(ap->a_vp);
    struct msdosfsmount *pmp = dep->de_pmp;
    u_long		cn;
    daddr_t		bn;
    
	if (ap->a_bpn == NULL)
		return (0);

    /* Find the cluster that contains the given file offset */
    cn = de_cluster(pmp, ap->a_foffset);
    
    /* Find the physical (device) block where that cluster starts */
    error = pcbmap(dep, cn, &bn, 0, &blksiz);
#ifdef DEBUG
    printf("msdosfs cmap...off 0x%lx bn1 0x%lx",
           (u_long)ap->a_foffset, bn);
#endif

    /* Add the offset in physical (device) blocks from the start of the cluster */
    bn += (((u_long)ap->a_foffset - de_cn2off(pmp, cn)) >> pmp->pm_bnshift);
    blksiz -= ((u_long)ap->a_foffset - (de_cn2off(pmp, cn)));
    
    *ap->a_bpn = bn;
	if (error == 0 && ap->a_run) {
		if (blksiz > ap->a_size)
			* ap->a_run = ap->a_size;
		else
			* ap->a_run = blksiz;
	}
	if (ap->a_poff)
		*(int *)ap->a_poff = 0;

#ifdef DEBUG
    printf(" bn 2 0x%lx run 0x%lx\n", bn, *ap->a_run);
#endif
 return (error);
}

static int
msdosfs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	struct vnode *vp = bp->b_vp;
	struct denode *dep;
	int error = 0;

	dep = VTODE(vp);
	if ( !(bp->b_flags & B_VECTORLIST)) {

		if (vp->v_type == VBLK || vp->v_type == VCHR)
			panic("msdosfs_strategy: device vnode passed!");

		if (bp->b_flags & B_PAGELIST) {
			/*
			* if we have a page list associated with this bp,
			* then go through cluste_bp since it knows how to 
			* deal with a page request that might span non-contiguous
			* physical blocks on the disk...
			*/
			error = cluster_bp(bp);
			vp = dep->de_devvp;
			bp->b_dev = vp->v_rdev;
			return (error);
		}
	/*
	 * If we don't already know the filesystem relative block number
		* then get it using VOP_BMAP().  If VOP_BMAP() returns the block
		* number as -1 then we've got a hole in the file.  HFS filesystems
	 * don't allow files with holes, so we shouldn't ever see this.
	 */
	if (bp->b_blkno == bp->b_lblkno) {
			if ((error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL))) {
			bp->b_error = error;
				bp->b_flags |= B_ERROR;
				biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
				clrbuf(bp);
	}
		if ((long)bp->b_blkno == -1) {
			biodone(bp);
		return (0);
	}
		if (bp->b_validend == 0) {
			/* Record the exact size of the I/O transfer about to be made: */
			bp->b_validend = bp->b_bcount;
		};
	}
	/*
	 * Read/write the block from/to the disk that contains the desired
	 * file block.
	 */
	vp = dep->de_devvp;
	bp->b_dev = vp->v_rdev;
	return VOCALL (vp->v_op, VOFFSET(vop_strategy), ap);
}

/*
 * Lock an node.
 */
static int
msdosfs_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (VTODE(vp) == (struct denode *) NULL)
		panic ("msdosfs_lock: null node");
	return (lockmgr(&VTODE(vp)->de_lock, ap->a_flags, &vp->v_interlock,ap->a_p));
}

static int
msdosfs_abortop(ap)
struct vop_abortop_args /* {
    struct vnode *a_dvp;
    struct componentname *a_cnp;
} */ *ap;
{

    if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
        FREE_ZONE(ap->a_cnp->cn_pnbuf, ap->a_cnp->cn_pnlen, M_NAMEI);

    return (0);
}

/*
 * Unlock an node.
 */
static int
msdosfs_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTODE(vp)->de_lock, ap->a_flags | LK_RELEASE, &vp->v_interlock,ap->a_p));
}

/*
 * Check for a locked node.
 */
static int
msdosfs_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return (lockstatus(&VTODE(ap->a_vp)->de_lock));
}

static int
msdosfs_print(ap)
	struct vop_print_args /* {
		struct vnode *vp;
	} */ *ap;
{
	struct denode *dep = VTODE(ap->a_vp);

	printf(
	    "tag VT_MSDOSFS, startcluster %lu, dircluster %lu, diroffset %lu ",
	       dep->de_StartCluster, dep->de_dirclust, dep->de_diroffset);
	printf(" dev %d, %d", major(dep->de_dev), minor(dep->de_dev));
	(void) lockmgr_printinfo(&dep->de_lock);
	printf("\n");
	return (0);
}

static int
msdosfs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{
	struct msdosfsmount *pmp = VTODE(ap->a_vp)->de_pmp;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = pmp->pm_flags & MSDOSFSMNT_LONGNAME ? WIN_MAXLEN : 12;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}


/*
#
#% advlock	vp	U U U
#
vop_advlock {
	IN struct vnode *vp;
	IN caddr_t id;
	IN int op;
	IN struct flock *fl;
	IN int flags;
};
*/
static int msdosfs_advlock(struct vop_advlock_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct flock *fl = ap->a_fl;
	struct msdosfs_lockf *lock;
	struct denode *dep;
	off_t start, end;
	int retval;
    
	/* Only regular files can have locks */
	if (vp->v_type != VREG)
		return (EISDIR);

    dep = VTODE(vp);
    
	/*
	 * Avoid the common case of unlocking when inode has no locks.
	 */
	if (dep->de_lockf == (struct msdosfs_lockf *)0) {
		if (ap->a_op != F_SETLK) {
			fl->l_type = F_UNLCK;
			return (0);
		}
	}

	/*
	 * Convert the flock structure into a start and end.
	 */
	start = 0;
	switch (fl->l_whence) {
	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		start = fl->l_start;
		break;
	case SEEK_END:
		start = dep->de_FileSize + fl->l_start;
		break;
	default:
		return (EINVAL);
	}

	if (start < 0)
		return (EINVAL);
	if (fl->l_len == 0)
 		end = -1;
	else
		end = start + fl->l_len - 1;

	/*
	 * Create the lockf structure
	 */
	MALLOC(lock, struct msdosfs_lockf *, sizeof *lock, M_LOCKF, M_WAITOK);
	lock->lf_start = start;
	lock->lf_end = end;
	lock->lf_id = ap->a_id;
	lock->lf_denode = dep;
	lock->lf_type = fl->l_type;
	lock->lf_next = (struct msdosfs_lockf *)0;
	TAILQ_INIT(&lock->lf_blkhd);
	lock->lf_flags = ap->a_flags;

	/*
	 * Do the requested operation.
	 */
	switch(ap->a_op) {
	case F_SETLK:
		retval = msdosfs_setlock(lock);
		break;
	case F_UNLCK:
		retval = msdosfs_clearlock(lock);
		FREE(lock, M_LOCKF);
		break;
	case F_GETLK:
		retval = msdosfs_getlock(lock, fl);
		FREE(lock, M_LOCKF);
		break;
	default:
		retval = EINVAL;
		_FREE(lock, M_LOCKF);
            break;
	}

	return (retval);
}


/* Global vfs data structures for msdosfs */

typedef int     vop_t __P((void *));

vop_t **msdosfs_vnodeop_p;
static struct vnodeopv_entry_desc msdosfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vn_default_error },
	{ &vop_lookup_desc,		(vop_t *) msdosfs_cache_lookup },
	{ &vop_create_desc,		(vop_t *) msdosfs_create },
	{ &vop_mknod_desc,		(vop_t *) msdosfs_mknod },
	{ &vop_open_desc,		(vop_t *) msdosfs_open },
	{ &vop_close_desc,		(vop_t *) msdosfs_close },
	{ &vop_access_desc,		(vop_t *) msdosfs_access },
	{ &vop_getattr_desc,		(vop_t *) msdosfs_getattr },
	{ &vop_setattr_desc,		(vop_t *) msdosfs_setattr },
	{ &vop_read_desc,		(vop_t *) msdosfs_read },
	{ &vop_write_desc,		(vop_t *) msdosfs_write },
//	{ &vop_lease_desc,		(vop_t *) msdosfs_lease_check },
//	{ &vop_ioctl_desc,		(vop_t *) msdosfs_ioctl },
//	{ &vop_select_desc,		(vop_t *) msdosfs_select },
//	{ &vop_mmap_desc,		(vop_t *) msdosfs_mmap },
	{ &vop_fsync_desc,		(vop_t *) msdosfs_fsync },
//	{ &vop_seek_desc,		(vop_t *) msdosfs_seek },
	{ &vop_remove_desc,		(vop_t *) msdosfs_remove },
	{ &vop_link_desc,		(vop_t *) err_link },   /* not supported */
	{ &vop_rename_desc,		(vop_t *) msdosfs_rename },
	{ &vop_mkdir_desc,		(vop_t *) msdosfs_mkdir },
	{ &vop_rmdir_desc,		(vop_t *) msdosfs_rmdir },
	{ &vop_symlink_desc,		(vop_t *) err_symlink },   /* not supported */
	{ &vop_readdir_desc,		(vop_t *) msdosfs_readdir },
//	{ &vop_readlink_desc,		(vop_t *) msdosfs_readlink },
	{ &vop_abortop_desc,		(vop_t *) msdosfs_abortop },
	{ &vop_inactive_desc,		(vop_t *) msdosfs_inactive },
	{ &vop_reclaim_desc,		(vop_t *) msdosfs_reclaim },
	{ &vop_lock_desc,			(vop_t *) msdosfs_lock },
	{ &vop_unlock_desc,			(vop_t *) msdosfs_unlock },
	{ &vop_bmap_desc,			(vop_t *) msdosfs_bmap },
	{ &vop_strategy_desc,		(vop_t *) msdosfs_strategy },
	{ &vop_print_desc,			(vop_t *) msdosfs_print },
	{ &vop_islocked_desc,		(vop_t *) msdosfs_islocked },
	{ &vop_pathconf_desc,		(vop_t *) msdosfs_pathconf },
	{ &vop_advlock_desc,		(vop_t *) msdosfs_advlock },
//	{ &vop_truncate_desc,		(vop_t *) msdosfs_truncate },
//	{ &vop_update_desc,			(vop_t *) msdosfs_update },
	{ &vop_pagein_desc,			(vop_t *) msdosfs_pagein },
	{ &vop_pageout_desc,		(vop_t *) msdosfs_pageout },
	{ &vop_blktooff_desc,		(vop_t *) msdosfs_blktooff },
	{ &vop_offtoblk_desc,		(vop_t *) msdosfs_offtoblk },
  	{ &vop_cmap_desc,			(vop_t *) msdosfs_cmap },
    { &vop_getattrlist_desc,	(vop_t *) msdosfs_getattrlist },  /* getattrlist */
//  { &vop_setattrlist_desc,	(vop_t *) msdosfs_setattrlist },  /* setattrlist */
	{ NULL, NULL }
};

struct vnodeopv_desc msdosfs_vnodeop_opv_desc =
	{ &msdosfs_vnodeop_p, msdosfs_vnodeop_entries };

// VNODEOP_SET(msdosfs_vnodeop_opv_desc);
