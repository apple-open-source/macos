/*	$NetBSD: ntfs_vnops.c,v 1.23 1999/10/31 19:45:27 jdolecek Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/ntfs/ntfs_vnops.c,v 1.31 2002/07/31 00:42:57 semenu Exp $
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#ifndef APPLE
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/dirent.h>
#ifdef APPLE
#include <sys/attr.h>
#include <sys/ubc.h>
#include <vfs/vfs_support.h>
#else
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>
#endif

#include <sys/sysctl.h>

/*#define NTFS_DEBUG 1*/
#ifdef APPLE
#include "ntfs.h"
#include "ntfs_inode.h"
#include "ntfs_subr.h"
#else
#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>
#include <fs/ntfs/ntfs_subr.h>
#endif

#include <sys/unistd.h> /* for pathconf(2) constants */

static int	ntfs_read(struct vop_read_args *);
static int	ntfs_write(struct vop_write_args *ap);
static int	ntfs_getattr(struct vop_getattr_args *ap);
static int	ntfs_inactive(struct vop_inactive_args *ap);
static int	ntfs_print(struct vop_print_args *ap);
static int	ntfs_reclaim(struct vop_reclaim_args *ap);
static int	ntfs_strategy(struct vop_strategy_args *ap);
static int	ntfs_access(struct vop_access_args *ap);
static int	ntfs_open(struct vop_open_args *ap);
static int	ntfs_close(struct vop_close_args *ap);
static int	ntfs_readdir(struct vop_readdir_args *ap);
#ifdef APPLE
static int	ntfs_lookup(struct vnode *dvp,
                            struct vnode **vpp,
                            struct componentname *cnp);
static int	ntfs_cache_lookup(struct vop_lookup_args *ap);
#else
static int	ntfs_lookup(struct vop_lookup_args *ap);
#endif
static int	ntfs_fsync(struct vop_fsync_args *ap);
static int	ntfs_pathconf(void *);

int	ntfs_prtactive = 0;	/* 1 => print out reclaim of active vnodes */

static int
ntfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	struct buf *bp;
	daddr_t cn;
	int resid, off, toread;
	int error;

	dprintf(("ntfs_read: ino: %d, off: %d resid: %d, segflg: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid,uio->uio_segflg));

	dprintf(("ntfs_read: filesize: %d",(u_int32_t)fp->f_size));

	/* don't allow reading after end of file */
	if (uio->uio_offset > fp->f_size)
		return (0);

	resid = min(uio->uio_resid, fp->f_size - uio->uio_offset);

	dprintf((", resid: %d\n", resid));

	error = 0;
	while (resid) {
		cn = ntfs_btocn(uio->uio_offset);
		off = ntfs_btocnoff(uio->uio_offset);

		toread = min(off + resid, ntfs_cntob(1));

		error = bread(vp, cn, ntfs_cntob(1), NOCRED, &bp);
		if (error) {
			brelse(bp);
			break;
		}

		error = uiomove(bp->b_data + off, toread - off, uio);
		if(error) {
			brelse(bp);
			break;
		}
		brelse(bp);

		resid -= toread - off;
	}

	return (error);
}

static int
ntfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	register struct vattr *vap = ap->a_vap;

	dprintf(("ntfs_getattr: %d, flags: %d\n",ip->i_number,ip->i_flag));

#ifdef APPLE
	vap->va_fsid = ip->i_dev;
#else
	vap->va_fsid = dev2udev(ip->i_dev);
#endif
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mp->ntm_mode;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_mp->ntm_uid;
	vap->va_gid = ip->i_mp->ntm_gid;
	vap->va_rdev = 0;				/* XXX UNODEV ? */
	vap->va_size = fp->f_size;
	vap->va_bytes = fp->f_allocated;
	vap->va_atime = ntfs_nttimetounix(fp->f_times.t_access);
	vap->va_mtime = ntfs_nttimetounix(fp->f_times.t_write);
	vap->va_ctime = ntfs_nttimetounix(fp->f_times.t_create);
	vap->va_flags = ip->i_flag;
	vap->va_gen = 0;
	vap->va_blocksize = ip->i_mp->ntm_spc * ip->i_mp->ntm_bps;
	vap->va_type = vp->v_type;
	vap->va_filerev = 0;
	return (0);
}


/*
 * Last reference to an ntnode.  If necessary, write or delete it.
 */
int
ntfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
#ifdef NTFS_DEBUG
	register struct ntnode *ip = VTONT(vp);
#endif

	dprintf(("ntfs_inactive: vnode: %p, ntnode: %d\n", vp, ip->i_number));

	if (ntfs_prtactive && vp->v_usecount != 0)
		vprint("ntfs_inactive: pushing active", vp);

#ifdef APPLE
	VOP_UNLOCK(vp, 0, ap->a_p);
#else
	VOP_UNLOCK(vp, 0, ap->a_td);
#endif

	/* XXX since we don't support any filesystem changes
	 * right now, nothing more needs to be done
	 */
	return (0);
}

/*
 * Reclaim an fnode/ntnode so that it can be used for other purposes.
 */
int
ntfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	int error;

	dprintf(("ntfs_reclaim: vnode: %p, ntnode: %d\n", vp, ip->i_number));

	if (ntfs_prtactive && vp->v_usecount != 0)
		vprint("ntfs_reclaim: pushing active", vp);

	if ((error = ntfs_ntget(ip)) != 0)
		return (error);
	
	/* Purge old data structures associated with the inode. */
	cache_purge(vp);

	ntfs_frele(fp);
	ntfs_ntput(ip);
	vp->v_data = NULL;

	return (0);
}

static int
ntfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	return (0);
}


#ifdef APPLE
static int
ntfs_bmap(ap)
    struct vop_bmap_args /* {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	daddr_t a_bn;
	struct vnode **a_vpp;
	daddr_t *a_bnp;
	int *a_runp;
    } */ *ap;
{
    register struct fnode *fp = VTOF(ap->a_vp);
    register struct ntnode *ip = FTONT(fp);

    if (ap->a_vpp != NULL)
        *ap->a_vpp = ip->i_devvp;

    /*¥¥ Should I check whether ap->a_bn is within EOF? */
    
    /*
     * Since we never assume data is aligned to block boundaries,
     * and the buffer cache/UBC will sometimes call VOP_BMAP,
     * we need a dummy routine.  We just return a bogus block
     * number, and zero for the run.
     */

    if (ap->a_bnp != NULL)
        *ap->a_bnp = -99;

    if (ap->a_runp != NULL)
        ap->a_runp = 0;
    
    return 0;
}


/* blktooff converts a logical block number to a file offset */
static int
ntfs_blktooff(ap)
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
static int
ntfs_offtoblk(ap)
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
#endif


/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ntfs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	register struct vnode *vp = bp->b_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct ntfsmount *ntmp = ip->i_mp;
	int error;

#ifdef APPLE
	dprintf(("ntfs_strategy: blkno: %d, lblkno: %d\n",
		(u_int32_t)bp->b_blkno,
		(u_int32_t)bp->b_lblkno));
#else
	dprintf(("ntfs_strategy: offset: %d, blkno: %d, lblkno: %d\n",
		(u_int32_t)bp->b_offset,(u_int32_t)bp->b_blkno,
		(u_int32_t)bp->b_lblkno));
#endif

	dprintf(("strategy: bcount: %d flags: 0x%lx\n", 
		(u_int32_t)bp->b_bcount,bp->b_flags));

#ifdef APPLE
	if (bp->b_flags & B_READ) {
#else
	if (bp->b_iocmd == BIO_READ) {
#endif
		u_int32_t toread;

		if (ntfs_cntob(bp->b_blkno) >= fp->f_size) {
			clrbuf(bp);
			error = 0;
		} else {
			toread = min(bp->b_bcount,
				 fp->f_size-ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: toread: %d, fsize: %d\n",
				toread,(u_int32_t)fp->f_size));

			error = ntfs_readattr(ntmp, ip, fp->f_attrtype,
				fp->f_attrname, ntfs_cntob(bp->b_blkno),
				toread, bp->b_data, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_readattr failed\n");
				bp->b_error = error;
#ifdef APPLE
				bp->b_flags |= B_ERROR;
#else
				bp->b_ioflags |= BIO_ERROR;
#endif
			}

			bzero(bp->b_data + toread, bp->b_bcount - toread);
		}
	} else {
		size_t tmp;
		u_int32_t towrite;

		if (ntfs_cntob(bp->b_blkno) + bp->b_bcount >= fp->f_size) {
			printf("ntfs_strategy: CAN'T EXTEND FILE\n");
			bp->b_error = error = EFBIG;
#ifdef APPLE
			bp->b_flags |= B_ERROR;
#else
			bp->b_ioflags |= BIO_ERROR;
#endif
		} else {
			towrite = min(bp->b_bcount,
				fp->f_size-ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: towrite: %d, fsize: %d\n",
				towrite,(u_int32_t)fp->f_size));

			error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,	
				fp->f_attrname, ntfs_cntob(bp->b_blkno),towrite,
				bp->b_data, &tmp, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_writeattr fail\n");
				bp->b_error = error;
#ifdef APPLE
				bp->b_flags |= B_ERROR;
#else
				bp->b_ioflags |= BIO_ERROR;
#endif
			}
		}
	}
#ifdef APPLE
	biodone(bp);
#else
	bufdone(bp);
#endif
	return (error);
}

static int
ntfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	u_int64_t towrite;
	size_t written;
	int error;

	dprintf(("ntfs_write: ino: %d, off: %d resid: %d, segflg: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid,uio->uio_segflg));
	dprintf(("ntfs_write: filesize: %d",(u_int32_t)fp->f_size));

	if (uio->uio_resid + uio->uio_offset > fp->f_size) {
		printf("ntfs_write: CAN'T WRITE BEYOND END OF FILE\n");
		return (EFBIG);
	}

	towrite = min(uio->uio_resid, fp->f_size - uio->uio_offset);

	dprintf((", towrite: %d\n",(u_int32_t)towrite));

	error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,
		fp->f_attrname, uio->uio_offset, towrite, NULL, &written, uio);
#ifdef NTFS_DEBUG
	if (error)
		printf("ntfs_write: ntfs_writeattr failed: %d\n", error);
#endif

	return (error);
}

int
ntfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);
	mode_t mode = ap->a_mode;
#ifdef APPLE
	mode_t mask;
	mode_t file_mode;
	struct ucred *cred = ap->a_cred;
	register gid_t *gp;
	int i;
#endif
#ifdef QUOTA
	int error;
#endif

	dprintf(("ntfs_access: %d\n",ip->i_number));

	/*
	 * Disallow write attempts on read-only filesystems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the filesystem.
	 */
	if (mode & VWRITE) {
		switch ((int)vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#ifdef QUOTA
			if (error = getinoquota(ip))
				return (error);
#endif
			break;
		}
	}

#ifdef APPLE
        /* We currently have vaccess in our headers, but no kernel implementation. */
        /*¥¥ÊShould probably check the file's DOS-style read-only bit */

	/* User id 0 always gets access. */
	if (cred->cr_uid == 0)
		return 0;

	mask = 0;
        file_mode = ip->i_mp->ntm_mode;
        
	/* Otherwise, check the owner. */
	/* And allow for console */
	if (cred->cr_uid == ip->i_mp->ntm_uid) {
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
		if (ip->i_mp->ntm_gid == *gp) {
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
#else
	return (vaccess(ip->i_mp->ntm_mode, ip->i_mp->ntm_uid,
	    ip->i_mp->ntm_gid, ap->a_mode, ap->a_cred));
#endif
} 

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
static int
ntfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
#if NTFS_DEBUG
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	printf("ntfs_open: %d\n",ip->i_number);
#endif

	/*
	 * Files marked append-only must be opened for appending.
	 */

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
static int
ntfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
#if NTFS_DEBUG
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	printf("ntfs_close: %d\n",ip->i_number);
#endif

	return (0);
}

int
ntfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_ncookies;
		u_int **cookies;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fnode *fp = VTOF(vp);
	register struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	int i, error = 0;
	u_int32_t faked = 0, num;
	int ncookies = 0;
	struct dirent cde;
	off_t off;

	dprintf(("ntfs_readdir %d off: %d resid: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid));

	off = uio->uio_offset;

	/* Simulate . in every dir except ROOT */
	if( ip->i_number != NTFS_ROOTINO ) {
		struct dirent dot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 1, "." };

		if( uio->uio_offset < sizeof(struct dirent) ) {
			dot.d_fileno = ip->i_number;
			error = uiomove((char *)&dot,sizeof(struct dirent),uio);
			if(error)
				return (error);

			ncookies ++;
		}
	}

	/* Simulate .. in every dir including ROOT */
	if( uio->uio_offset < 2 * sizeof(struct dirent) ) {
		struct dirent dotdot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 2, ".." };

                /*¥¥ Don't we need to set dotdot.d_fileno? */
		error = uiomove((char *)&dotdot,sizeof(struct dirent),uio);
		if(error)
			return (error);

		ncookies ++;
	}

	faked = (ip->i_number == NTFS_ROOTINO) ? 1 : 2;
	num = uio->uio_offset / sizeof(struct dirent) - faked;

	while( uio->uio_resid >= sizeof(struct dirent) ) {
		struct attr_indexentry *iep;

		error = ntfs_ntreaddir(ntmp, fp, num, &iep);

		if(error)
			return (error);

		if( NULL == iep )
			break;

		for(; !(le32toh(iep->ie_flag) & NTFS_IEFLAG_LAST) &&
                            (uio->uio_resid >= sizeof(struct dirent));
                        iep = NTFS_NEXTREC(iep, struct attr_indexentry *))
		{
			cde.d_fileno = le32toh(iep->ie_number);

#ifdef APPLE
			/* Hide system files. */
                        if (cde.d_fileno != NTFS_ROOTINO && cde.d_fileno < 24)
                                continue;
#endif
			if(!ntfs_isnamepermitted(ntmp,iep))
				continue;

			for(i=0; i<iep->ie_fnamelen; i++) {
				cde.d_name[i] = NTFS_U28(le16toh(iep->ie_fname[i]));
			}
			cde.d_name[i] = '\0';
			dprintf(("ntfs_readdir: elem: %d, fname:[%s] type: %d, flag: %d, ",
				num, cde.d_name, iep->ie_fnametype,
				iep->ie_flag));
			cde.d_namlen = iep->ie_fnamelen;
			cde.d_type = (le32toh(iep->ie_fflag) & NTFS_FFLAG_DIR) ? DT_DIR : DT_REG;
			cde.d_reclen = sizeof(struct dirent);
			dprintf(("%s\n", (cde.d_type == DT_DIR) ? "dir":"reg"));

			error = uiomove((char *)&cde, sizeof(struct dirent), uio);
			if(error)
				return (error);

			ncookies++;
			num++;
		}
	}

	dprintf(("ntfs_readdir: %d entries (%d bytes) read\n",
		ncookies,(u_int)(uio->uio_offset - off)));
	dprintf(("ntfs_readdir: off: %d resid: %d\n",
		(u_int32_t)uio->uio_offset,uio->uio_resid));

	if (!error && ap->a_ncookies != NULL) {
		struct dirent* dpStart;
		struct dirent* dp;
		u_long *cookies;
		u_long *cookiep;

		ddprintf(("ntfs_readdir: %d cookies\n",ncookies));
		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("ntfs_readdir: unexpected uio from NFS server");
		dpStart = (struct dirent *)
		     ((caddr_t)uio->uio_iov->iov_base -
			 (uio->uio_offset - off));
		MALLOC(cookies, u_long *, ncookies * sizeof(u_long),
		       M_TEMP, M_WAITOK);
		for (dp = dpStart, cookiep = cookies, i=0;
		     i < ncookies;
		     dp = (struct dirent *)((caddr_t) dp + dp->d_reclen), i++) {
			off += dp->d_reclen;
			*cookiep++ = (u_int) off;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}
/*
	if (ap->a_eofflag)
	    *ap->a_eofflag = VTONT(ap->a_vp)->i_size <= uio->uio_offset;
*/
	return (error);
}

/*
 * FreeBSD has a cn_flags flag named PDIRUNLOCK.  When set, vfs_lookup does a
 * vrele() instead of a vput() on the directory vnode inside its loop.
 * Darwin has no such flag and always does vput().  FreeBSD's smbfs uses the
 * same trick of defining it to zero if not previously defined.  I hope this
 * means the locking is OK.
 */
#ifndef PDIRUNLOCK
#define PDIRUNLOCK 0
#endif

#ifdef APPLE
static int
ntfs_lookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp)
{
#else
int
ntfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct vnode *dvp = ap->a_dvp;
#endif
	register struct ntnode *dip = VTONT(dvp);
	struct ntfsmount *ntmp = dip->i_mp;
#ifndef APPLE
	struct componentname *cnp = ap->a_cnp;
#endif
	struct ucred *cred = cnp->cn_cred;
	int error;
	int lockparent = cnp->cn_flags & LOCKPARENT;
#if NTFS_DEBUG
	int wantparent = cnp->cn_flags & (LOCKPARENT|WANTPARENT);
#endif
	dprintf(("ntfs_lookup: \"%.*s\" (%ld bytes) in %d, lp: %d, wp: %d \n",
		(int)cnp->cn_namelen, cnp->cn_nameptr, cnp->cn_namelen,
		dip->i_number, lockparent, wantparent));

#ifdef APPLE
	error = VOP_ACCESS(dvp, VEXEC, cred, cnp->cn_proc);
#else
	error = VOP_ACCESS(dvp, VEXEC, cred, cnp->cn_thread);
#endif
	if(error)
		return (error);

	if ((cnp->cn_flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	if(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		dprintf(("ntfs_lookup: faking . directory in %d\n",
			dip->i_number));

		VREF(dvp);
#ifdef APPLE
		*vpp = dvp;
#else
		*ap->a_vpp = dvp;
#endif
		error = 0;
	} else if (cnp->cn_flags & ISDOTDOT) {
		struct ntvattr *vap;
                ino_t parent_ino;
                
		dprintf(("ntfs_lookup: faking .. directory in %d\n",
			 dip->i_number));

		error = ntfs_ntvattrget(ntmp, dip, NTFS_A_NAME, NULL, 0, &vap);
		if(error)
			return (error);

#ifdef APPLE
		VOP_UNLOCK(dvp,0,cnp->cn_proc);
#else
		VOP_UNLOCK(dvp,0,cnp->cn_thread);
#endif
		cnp->cn_flags |= PDIRUNLOCK;

                parent_ino = le32toh(vap->va_a_name->n_pnumber);
		dprintf(("ntfs_lookup: parentdir: %d\n",
			 parent_ino));
#ifdef APPLE
		error = VFS_VGET(ntmp->ntm_mountp, &parent_ino, vpp);
#else
		error = VFS_VGET(ntmp->ntm_mountp, vap->va_a_name->n_pnumber,
				 LK_EXCLUSIVE, ap->a_vpp); 
#endif
		ntfs_ntvattrrele(vap);
		if (error) {
#ifdef APPLE
			if (vn_lock(dvp,LK_EXCLUSIVE|LK_RETRY,cnp->cn_proc)==0)
#else
			if (vn_lock(dvp,LK_EXCLUSIVE|LK_RETRY,cnp->cn_thread)==0)
#endif
				cnp->cn_flags &= ~PDIRUNLOCK;
			return (error);
		}

		if (lockparent && (cnp->cn_flags & ISLASTCN)) {
#ifdef APPLE
			error = vn_lock(dvp, LK_EXCLUSIVE, cnp->cn_proc);
#else
			error = vn_lock(dvp, LK_EXCLUSIVE, cnp->cn_thread);
#endif
			if (error) {
#ifdef APPLE
				vput( *vpp );
#else
				vput( *(ap->a_vpp) );
#endif
				return (error);
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}
	} else {
#ifdef APPLE
		error = ntfs_ntlookupfile(ntmp, dvp, cnp, vpp);
#else
		error = ntfs_ntlookupfile(ntmp, dvp, cnp, ap->a_vpp);
#endif
		if (error) {
			dprintf(("ntfs_ntlookupfile: returned %d\n", error));
			return (error);
		}

#ifdef APPLE
		dprintf(("ntfs_lookup: found ino: %d\n", 
			VTONT((*vpp))->i_number));
#else
		dprintf(("ntfs_lookup: found ino: %d\n", 
			VTONT(*ap->a_vpp)->i_number));
#endif
		if(!lockparent || !(cnp->cn_flags & ISLASTCN))
#ifdef APPLE
			VOP_UNLOCK(dvp, 0, cnp->cn_proc);
#else
			VOP_UNLOCK(dvp, 0, cnp->cn_thread);
#endif
	}

	if (cnp->cn_flags & MAKEENTRY)
#ifdef APPLE
		cache_enter(dvp, *vpp, cnp);
#else
		cache_enter(dvp, *ap->a_vpp, cnp);
#endif
	return (error);
}


#ifdef APPLE
/*
 * This does a lookup through the name cache.  If nothing is found
 * in the cache, then it calls ntfs_lookup to scan the directory
 * on disk.
 *
 * This is essentially FreeBSD's vfs_cache_lookup routine, without
 * the shared lock support.  Darwin doesn't have a similar generic
 * routine nor VOP call to do the on-disk search.
 */

/* msd: big endian OK */
static int
ntfs_cache_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *dvp, *vp;
	int lockparent;
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	struct proc *proc = cnp->cn_proc;
	u_long vpid;	/* capability number of vnode */

	*vpp = NULL;
	dvp = ap->a_dvp;
	lockparent = flags & LOCKPARENT;

	if (dvp->v_type != VDIR)
                return (ENOTDIR);

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	error = VOP_ACCESS(dvp, VEXEC, cred, proc);

	if (error)
		return (error);

	error = cache_lookup(dvp, vpp, cnp);

	if (!error) 
		return ntfs_lookup(dvp, vpp, cnp);

	if (error == ENOENT)
		return (error);

	vp = *vpp;
	vpid = vp->v_id;
	if (dvp == vp) {   /* lookup on "." */
		VREF(vp);
		error = 0;
	} else if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, proc);
		error = vget(vp, LK_EXCLUSIVE, proc);
		if (!error && lockparent && (flags & ISLASTCN)) {
			error = vn_lock(dvp, LK_EXCLUSIVE, proc);
		}
	} else {
		error = vget(vp, LK_EXCLUSIVE, proc);
		if (!lockparent || error || !(flags & ISLASTCN))
			VOP_UNLOCK(dvp, 0, proc);
	}
	/*
	 * Check that the capability number did not change
	 * while we were waiting for the lock.
	 */
	if (!error) {
		if (vpid == vp->v_id)
			return (0);
		vput(vp);
		if (lockparent && dvp != vp && (flags & ISLASTCN))
			VOP_UNLOCK(dvp, 0, proc);
	}
        error = vn_lock(dvp, LK_EXCLUSIVE, proc);
        if (error)
                return (error);
	return ntfs_lookup(dvp, vpp, cnp);
}

static int
ntfs_abortop(ap)
struct vop_abortop_args /* {
    struct vnode *a_dvp;
    struct componentname *a_cnp;
} */ *ap;
{

    if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
        FREE_ZONE(ap->a_cnp->cn_pnbuf, ap->a_cnp->cn_pnlen, M_NAMEI);

    return (0);
}
#endif

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
ntfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap;
{
	return (0);
}

/*
 * Return POSIX pathconf information applicable to NTFS filesystem
 */
int
ntfs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NTFS_MAXFILENAME;
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

#ifdef APPLE
/*
 * Darwin's Unified Buffer Cache requires you to support pagein and pageout.
 * There is no way for a filesystem to prevent memory mapping of regular files.
 */
static int ntfs_pageout(struct vop_pageout_args *args)
{
    panic("ntfs_pageout not supported");
    return EOPNOTSUPP;
}

/*
#
#% pagein	vp	= = =
#

struct vop_pagein_args {
	struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	upl_t a_pl;
	vm_offset_t a_pl_offset;
	off_t a_f_offset;
	size_t a_size;
	struct ucred *a_cred;
	int a_flags;
};
*/
static int ntfs_pagein(struct vop_pagein_args *args)
{
    int error;
    kern_return_t kret;
    vm_offset_t ioaddr;
    struct vnode *vp = args->a_vp;
    upl_t pl = args->a_pl;
    vm_offset_t pl_offset = args->a_pl_offset;
    off_t f_offset = args->a_f_offset;
    size_t size = args->a_size;
    int flags = args->a_flags;
    struct fnode *fp = VTOF(vp);
    struct ntnode *ip = FTONT(fp);
    struct ntfsmount *ntmp = ip->i_mp;
    
    if (UBCINVALID(vp))
        panic("ntfs_pagein: ubc invalid vp=0x%x", vp);
    if (UBCINFOMISSING(vp))
        panic("ntfs_pagein: ubc missing vp=%x", vp);

    /*
     * We don't currently support cluster I/O.  In the future, we
     * could support it for uncompressed runs in non-resident
     * attributes.
     */
    if (0)
    {
        int dev_block_size;
        
        VOP_DEVBLOCKSIZE(ntmp->ntm_devvp, &dev_block_size);
        error = cluster_pagein(vp, pl, pl_offset, f_offset, size,
                fp->f_size, dev_block_size, flags);
    }
    else
    {
        if (size <= 0)
            panic("ntfs_pagein: size = %x", size);
        
        kret = ubc_upl_map(pl, &ioaddr);
        if (kret != KERN_SUCCESS) {
                panic("ntfs_pagein: ubc_upl_map error = %d", kret);
                return (EPERM);
        }
        
        ioaddr += pl_offset;
        
        /* Make sure pagein doesn't extend beyond EOF */
        if (f_offset + size > fp->f_size)
            size = fp->f_size - f_offset;	/* pin size to EOF */
        
        /* Read from vp, file offset=f_offset, length=size, buffer=ioaddr */
        error = ntfs_readattr(ntmp, ip, fp->f_attrtype, fp->f_attrname, f_offset,
                size, (caddr_t)ioaddr, NULL);
        
        if (error)
        {
            panic("ntfs_pagein: read error = %d", error);
            size = 0;
        }
        
        /* Zero fill part of page past EOF */
        if (args->a_size > size)
            bzero((caddr_t)ioaddr+size, args->a_size-size);
        
        /*¥¥ If mounted read/write, we'd update the access time here. */
        
        kret = ubc_upl_unmap(pl);
        if (kret != KERN_SUCCESS)
            panic("ntfs_pagein: ubc_upl_unmap error = %d", kret);
        
        /* Commit/abort pages unless requested not to */
        if ((flags & UPL_NOCOMMIT) == 0)
        {
            if (error)
                ubc_upl_abort_range(pl, pl_offset, args->a_size, UPL_ABORT_FREE_ON_EMPTY);
            else
                ubc_upl_commit_range(pl, pl_offset, args->a_size, UPL_COMMIT_FREE_ON_EMPTY);
        }
    }
    
    if (error)
        panic("ntfs_pagein error = %d", error);
        
    return error;
}


/*
 * Vnode (fnode) locking: I think FreeBSD has a default locking implementation that
 * assumes the first field of the structure pointed to by vp->v_data (in our case, the
 * struct fnode) is a BSD lock, and provides a default implementation of xx_lock, xx_unlock
 * and xx_islocked using that lock field.
 *
 * Darwin doesn't have that, so here are explicit routines to use that field for vnode locking.
 */

/*
 * Check for a locked vnode.
 */
static int
ntfs_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	return (lockstatus(&VTOF(ap->a_vp)->f_lock));
}


/*
 * Lock a vnode.
 */
static int
ntfs_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (VTOF(vp) == (struct fnode *) NULL)
		panic ("ntfs_lock: null node");
	return (lockmgr(&VTOF(vp)->f_lock, ap->a_flags, &vp->v_interlock, ap->a_p));
}


/*
 * Unlock a vnode.
 */
static int
ntfs_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (VTOF(vp) == (struct fnode *) NULL)
		panic ("ntfs_unlock: null node");
	return (lockmgr(&VTOF(vp)->f_lock, ap->a_flags | LK_RELEASE, &vp->v_interlock, ap->a_p));
}

/*
 * getattrlist -- Return attributes about files, directories, and volumes.
 * This is a minimal implementation that only returns volume capabilities
 * so clients (like Carbon) can tell which interfaces and features are
 * supported by the volume.
 *
 * #
 * #% getattrlist	vp	= = =
 * #
 * vop_getattrlist {
 *	IN struct vnode *vp;
 *	IN struct attrlist *alist;
 *	INOUT struct uio *uio;
 *	IN struct ucred *cred;
 *	IN struct proc *p;
 * };
 */
static int
ntfs_getattrlist(ap)
        struct vop_getattrlist_args /* {
                struct vnode *a_vp;
                struct attrlist *a_alist
                struct uio *a_uio;
                struct ucred *a_cred;
                struct proc *a_p;
        } */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct attrlist *alist = ap->a_alist;
        size_t attrbufsize;	/* Actual buffer size */
        int error;
        struct {
                u_long buffer_size;
                vol_capabilities_attr_t capabilities;
        } results;

        /*
         * Reject requests that ask for anything other than volume
         * capabilities, or has an invalid bitmap count (indicating
         * a change in the headers that this code isn't prepared
         * to handle).
         *
         * NOTE: we don't use ATTR_BIT_MAP_COUNT, because that could
         * change in the header without this code changing.
         */
        if ((alist->bitmapcount != 5) ||
            (alist->commonattr != 0) ||
            (alist->volattr != (ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES)) ||
            (alist->dirattr != 0) ||
            (alist->fileattr != 0) ||
            (alist->forkattr != 0)) {
                return EINVAL;
        }
        
        /*
         * Volume requests, including volume capabilities, requires using
         * the volume's root vnode.  Since we only handle volume requests,
         * this is always required.
         */
        if ((vp->v_flag & VROOT) == 0)
                return EINVAL;

        /*
         * A general implementation would calculate the maximum size of
         * all requested attributes, allocate a buffer to hold them,
         * and then pack them all in bitmap order.  Since we support
         * just one attribute, this trivially uses a local structure.
         */
        attrbufsize = MIN(ap->a_uio->uio_resid, sizeof results);
        results.buffer_size = attrbufsize;
        
        /* The capabilities[] array defines what this volume supports. */
        results.capabilities.capabilities[VOL_CAPABILITIES_FORMAT] =
            VOL_CAP_FMT_HARDLINKS |
	    VOL_CAP_FMT_SPARSE_FILES |
	    VOL_CAP_FMT_CASE_PRESERVING |
	    VOL_CAP_FMT_FAST_STATFS ;
        results.capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] =
            0;	/* None of the optional interfaces are implemented. */
        results.capabilities.capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
        results.capabilities.capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

        /*
         * The valid[] array defines which bits this code understands
         * the meaning of (whether the volume has that capability or not).
         * Any zero bits here means "I don't know what you're asking about"
         * and the caller cannot tell whether that capability is
         * present or not.
         */
        results.capabilities.valid[VOL_CAPABILITIES_FORMAT] =
            VOL_CAP_FMT_PERSISTENTOBJECTIDS |
            VOL_CAP_FMT_SYMBOLICLINKS |
            VOL_CAP_FMT_HARDLINKS |
	    VOL_CAP_FMT_JOURNAL |
	    VOL_CAP_FMT_JOURNAL_ACTIVE |
	    VOL_CAP_FMT_NO_ROOT_TIMES |
	    VOL_CAP_FMT_SPARSE_FILES |
	    VOL_CAP_FMT_ZERO_RUNS |
	    VOL_CAP_FMT_CASE_SENSITIVE |
	    VOL_CAP_FMT_CASE_PRESERVING |
	    VOL_CAP_FMT_FAST_STATFS ;
        results.capabilities.valid[VOL_CAPABILITIES_INTERFACES] =
            VOL_CAP_INT_SEARCHFS |
            VOL_CAP_INT_ATTRLIST |
            VOL_CAP_INT_NFSEXPORT |
            VOL_CAP_INT_READDIRATTR |
            VOL_CAP_INT_EXCHANGEDATA |
            VOL_CAP_INT_COPYFILE |
            VOL_CAP_INT_ALLOCATE |
            VOL_CAP_INT_VOL_RENAME |
            VOL_CAP_INT_ADVLOCK |
            VOL_CAP_INT_FLOCK ;
        results.capabilities.valid[VOL_CAPABILITIES_RESERVED1] = 0;
        results.capabilities.valid[VOL_CAPABILITIES_RESERVED2] = 0;

        /* Copy the results to the caller. */
        error = uiomove((caddr_t) &results, attrbufsize, ap->a_uio);
        
        return error;
}

/*
 * The following vnode operations aren't supported.  Many require stubs
 * to return EROFS instead of EOPNOTSUPP.  When EOPNOTSUPP is sufficient,
 * but we need to do additional cleanup, just use the default error routine.
 */

static int
ntfs_create(struct vop_create_args *ap)
{
	(void) nop_create(ap);
	return EROFS;
}

static int
ntfs_mknod(struct vop_mknod_args *ap)
{
	(void) nop_mknod(ap);
	return EROFS;
}

static int
ntfs_mkcomplex(struct vop_mkcomplex_args *ap)
{
	(void) nop_mkcomplex(ap);
	return EROFS;
}

static int
ntfs_setattr(struct vop_setattr_args *ap)
{
	(void) nop_setattr(ap);
	return EROFS;
}

static int
ntfs_remove(struct vop_remove_args *ap)
{
	(void) nop_remove(ap);
	return EROFS;
}

static int
ntfs_link(struct vop_link_args *ap)
{
	(void) nop_link(ap);
	return EROFS;
}

static int
ntfs_rename(struct vop_rename_args *ap)
{
	(void) nop_rename(ap);
	return EROFS;
}

static int
ntfs_mkdir(struct vop_mkdir_args *ap)
{
	(void) nop_mkdir(ap);
	return EROFS;
}

static int
ntfs_rmdir(struct vop_rmdir_args *ap)
{
	(void) nop_rmdir(ap);
	return EROFS;
}

/* Symbolic links aren't supported at all, so return EOPNOTSUPP */
#define ntfs_symlink err_symlink

/* The xxxdirattr calls aren't supported, so return EOPNOTSUPP */
#define ntfs_readdirattr err_readdirattr

static int
ntfs_allocate(struct vop_allocate_args *ap)
{
	(void) nop_allocate(ap);
	return EROFS;
}

#define ntfs_blkatoff err_blkatoff
#define ntfs_valloc err_valloc
#define ntfs_devblocksize err_devblocksize
#define ntfs_searchfs err_searchfs

static int
ntfs_copyfile(struct vop_copyfile_args *ap)
{
	(void) nop_copyfile(ap);
	return EROFS;
}
#endif /* APPLE */

/*
 * Global vfs data structures
 */
vop_t **ntfs_vnodeop_p;
static
struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
#ifdef APPLE
	{ &vop_default_desc, (vop_t *)vn_default_error },
#else
	{ &vop_default_desc, (vop_t *)vop_defaultop },
#endif

	{ &vop_getattr_desc, (vop_t *)ntfs_getattr },
	{ &vop_inactive_desc, (vop_t *)ntfs_inactive },
	{ &vop_reclaim_desc, (vop_t *)ntfs_reclaim },
	{ &vop_print_desc, (vop_t *)ntfs_print },
	{ &vop_pathconf_desc, ntfs_pathconf },

#ifdef APPLE
	{ &vop_islocked_desc, (vop_t *) ntfs_islocked },
	{ &vop_unlock_desc, (vop_t *) ntfs_unlock },
	{ &vop_lock_desc, (vop_t *) ntfs_lock },
	{ &vop_lookup_desc, (vop_t *)ntfs_cache_lookup },
#else
	{ &vop_islocked_desc, (vop_t *)vop_stdislocked },
	{ &vop_unlock_desc, (vop_t *)vop_stdunlock },
	{ &vop_lock_desc, (vop_t *)vop_stdlock },
	{ &vop_cachedlookup_desc, (vop_t *)ntfs_lookup },
	{ &vop_lookup_desc, (vop_t *)vfs_cache_lookup },
#endif

	{ &vop_access_desc, (vop_t *)ntfs_access },
	{ &vop_close_desc, (vop_t *)ntfs_close },
	{ &vop_open_desc, (vop_t *)ntfs_open },
	{ &vop_readdir_desc, (vop_t *)ntfs_readdir },
	{ &vop_fsync_desc, (vop_t *)ntfs_fsync },

#ifdef APPLE
	{ &vop_bmap_desc, (vop_t *)ntfs_bmap },
/*	{ &vop_cmap_desc, (vop_t *) ntfs_cmap },		Needed if we do cluster I/O */
	{ &vop_blktooff_desc, (vop_t *)ntfs_blktooff },
	{ &vop_offtoblk_desc, (vop_t *)ntfs_offtoblk },
#endif
	{ &vop_strategy_desc, (vop_t *)ntfs_strategy },
	{ &vop_read_desc, (vop_t *)ntfs_read },
	{ &vop_write_desc, (vop_t *)ntfs_write },

#ifdef APPLE
	{ &vop_pagein_desc, (vop_t *) ntfs_pagein },
	{ &vop_pageout_desc, (vop_t *) ntfs_pageout },
	{ &vop_abortop_desc, (vop_t *) ntfs_abortop },
/*	{ &vop_advlock_desc, (vop_t *) ntfs_advlock },		Needed for Carbon to open files for writing */

	/*
	 * The following operations are not implemented, but require
	 * extra work to be consistent with the locking and pathname
	 * allocation policies.
	 */
	{ &vop_create_desc, (vop_t *) ntfs_create },
	/* whiteout -- vn_default_error */
	{ &vop_mknod_desc, (vop_t *) ntfs_mknod },
	{ &vop_mkcomplex_desc, (vop_t *) ntfs_mkcomplex },
	{ &vop_setattr_desc, (vop_t *) ntfs_setattr },
        { &vop_getattrlist_desc, (vop_t *) ntfs_getattrlist },
	/* setattrlist -- vn_default_error */
	/* lease -- vn_default_error */
	/* ioctl -- vn_default_error */
	/* select -- vn_default_error */
	/* exchange -- vn_default_error */
	/* revoke -- vn_default_error */
	/* mmap -- vn_default_error */
	/* seek -- vn_default_error */
	{ &vop_remove_desc, (vop_t *) ntfs_remove },
	{ &vop_link_desc, (vop_t *) ntfs_link },
	{ &vop_rename_desc, (vop_t *) ntfs_rename },
	{ &vop_mkdir_desc, (vop_t *) ntfs_mkdir },
	{ &vop_rmdir_desc, (vop_t *) ntfs_rmdir },
	{ &vop_symlink_desc, (vop_t *) ntfs_symlink },
	{ &vop_readdirattr_desc, (vop_t *) ntfs_readdirattr },
	/* readlink -- vn_default_error */
	{ &vop_blkatoff_desc, (vop_t *) ntfs_blkatoff },
	{ &vop_valloc_desc, (vop_t *) ntfs_valloc },
	/* reallocblks -- vn_default_error */
	/* vfree -- vn_default_error */
	/* truncate -- vn_default_error */
	{ &vop_allocate_desc, (vop_t *) ntfs_allocate },
	/* update -- vn_default_error */
	/* pgrd -- vn_default_error */
	/* pgwr -- vn_default_error */
	{ &vop_devblocksize_desc, (vop_t *) ntfs_devblocksize },
	{ &vop_searchfs_desc, (vop_t *) ntfs_searchfs },
	{ &vop_copyfile_desc, (vop_t *) ntfs_copyfile },
#endif
	{ NULL, NULL }
};

#ifndef APPLE
static
#endif
struct vnodeopv_desc ntfs_vnodeop_opv_desc =
	{ &ntfs_vnodeop_p, ntfs_vnodeop_entries };

#ifndef APPLE
VNODEOP_SET(ntfs_vnodeop_opv_desc);
#endif
