/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/attr.h>
#include <sys/ubc.h>
#include <sys/utfconv.h>
#include <vfs/vfs_support.h>
#include <miscfs/specfs/specdev.h>

#include <sys/sysctl.h>

/*#define NTFS_DEBUG 1*/
#include "ntfs.h"
#include "ntfs_inode.h"
#include "ntfs_subr.h"
#include "ntfs_vfsops.h"

#include <sys/unistd.h> /* for pathconf(2) constants */

static int	ntfs_read(struct vnop_read_args *);
static int	ntfs_write(struct vnop_write_args *ap);
static int	ntfs_getattr(struct vnop_getattr_args *ap);
static int	ntfs_inactive(struct vnop_inactive_args *ap);
static int	ntfs_reclaim(struct vnop_reclaim_args *ap);
static int	ntfs_strategy(struct vnop_strategy_args *ap);
static int	ntfs_open(struct vnop_open_args *ap);
static int	ntfs_close(struct vnop_close_args *ap);
static int	ntfs_readdir(struct vnop_readdir_args *ap);
static int	ntfs_lookup(vnode_t dvp, vnode_t *vpp, struct componentname *cnp, proc_t p);
static int	ntfs_cache_lookup(struct vnop_lookup_args *ap);
static int	ntfs_fsync(struct vnop_fsync_args *ap);
static int	ntfs_pathconf(struct vnop_pathconf_args *ap);

static int
ntfs_read(ap)
	struct vnop_read_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_ioflag;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	struct buf *bp;
	daddr64_t cn;
	off_t off, toread;
	size_t bsize;	/* Best size for each buffer cache block of this file */
	int error;
	int io_resid;

	dprintf(("ntfs_read: ino: %d, off: %lld resid: %d\n",ip->i_number,uio_offset(uio),uio_resid(uio)));

	dprintf(("ntfs_read: filesize: %d",(u_int32_t)fp->f_size));

	/* exit quickly if there is nothing to read */
	if (uio_resid(uio) == 0)
		return 0;

	/* don't allow reading after end of file */
	if (uio_offset(uio) >= fp->f_size)
		return (0);

	/* If this is a non-compressed non-resident file, use Cluster I/O */
	if (vnode_isreg(vp) && (fp->f_flag & FN_NONRESIDENT) && (fp->f_compsize == 0))
	{
		return cluster_read(vp, uio, fp->f_size, 0);
	}
	
	if (fp->f_compsize != 0)
		bsize = fp->f_compsize;		/* Read compressed files one compression unit at a time */
	else
		bsize = ntfs_cntob(1);		/* Otherwise, read one cluster at a time */

	error = 0;
	while (uio_resid(uio) > 0 && uio_offset(uio) < fp->f_size) {
		/*
		 * See if there is any cached data available to copy
		 */
		io_resid = uio_resid(uio);				/* Amount caller wants */
		off = fp->f_size - uio_offset(uio);		/* Amount until EOF */
		if (off < io_resid)
			io_resid = off;						/* Pin io_resid to EOF */
#if 0	/* because cluster_copy_ubc_data is not public right now */
		if (io_resid > 0) {
/*¥			error = cluster_copy_ubc_data(vp, uio, &io_resid, 0); */
			if (error)
				return error;
		}
		if (uio_resid(uio) <= 0 || uio_offset(uio) >= fp->f_size)
			return 0;
#endif

		/*
		 * Consider doing read-ahead here.  The thing is, we'd have to read
		 * ahead from the uncompressed stream, into a spot where ntfs_readattr
		 * could find it.
		 *
		 * One possibility: have a separate vnode for the uncompressed stream.
		 * Read ahead on that stream, and invalidate buffers once we
		 * do the decompression.  Perhaps merely reading that stream with
		 * cluster_read would be enough.
		 *
		 * Another possibility: map the physical blocks and read ahead
		 * from the disk device (again, probably need to invalidate them
		 * once the decompression is done).
		 */

		/*
		 * Data was not already in UBC, so read it in now.
		 */
		cn = uio_offset(uio) / bsize;
		off = uio_offset(uio) % bsize;

		toread = MIN(bsize - off, io_resid);

		error = (int)buf_bread(vp, cn, bsize, vfs_context_ucred(ap->a_context), &bp);
		if (error) {
			buf_brelse(bp);
			break;
		}

		/*
		 *¥ It might perform faster to loop back to cluster_copy_ubc_data
		 * instead of doing uiomove here.  But that would only work if
		 * cluster_copy_ubc_data works with a bp in use.
		 */
		error = uiomove((char *)buf_dataptr(bp) + off, toread, uio);
		buf_brelse(bp);
		if(error) {
			break;
		}
	}

	return (error);
}

static int
ntfs_getattr(ap)
	struct vnop_getattr_args /* {
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct vnode_attr *vap = ap->a_vap;

	VATTR_RETURN(vap, va_rdev, 0);
	VATTR_RETURN(vap, va_nlink, ip->i_nlink);
	VATTR_RETURN(vap, va_total_size, fp->f_size);
	VATTR_RETURN(vap, va_total_alloc, fp->f_allocated);
	VATTR_RETURN(vap, va_data_size, fp->f_size);
	VATTR_RETURN(vap, va_data_alloc, fp->f_allocated);
	VATTR_RETURN(vap, va_iosize, ip->i_mp->ntm_spc * ip->i_mp->ntm_bps);
	
	/* No va_uid, va_gid */
	VATTR_RETURN(vap, va_mode, ip->i_mp->ntm_mode);
	VATTR_RETURN(vap, va_flags, 0);
	/* No va_filesec */
	
	if (VATTR_WANTED(vap, va_create_time))
		VATTR_RETURN(vap, va_create_time, ntfs_nttimetounix(fp->f_times.t_create));
	if (VATTR_WANTED(vap, va_access_time))
		VATTR_RETURN(vap, va_access_time, ntfs_nttimetounix(fp->f_times.t_access));
	if (VATTR_WANTED(vap, va_modify_time))
		VATTR_RETURN(vap, va_modify_time, ntfs_nttimetounix(fp->f_times.t_write));
	if (VATTR_WANTED(vap, va_change_time))
		VATTR_RETURN(vap, va_change_time, ntfs_nttimetounix(fp->f_times.t_mftwrite));
	/* No va_backup_time */
	
	VATTR_RETURN(vap, va_fileid, ip->i_number);
	/* No va_linkid */
	/* No va_parentid */
	VATTR_RETURN(vap, va_fsid, ip->i_dev);
	/* No va_filerev */
	/* No va_gen */
	/* No va_encoding */
	
	return 0;
}

/*
 * Last reference to an ntnode.  If necessary, write or delete it.
 */
int
ntfs_inactive(ap)
	struct vnop_inactive_args /* {
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
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
	struct vnop_reclaim_args /* {
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	int error;

	dprintf(("ntfs_reclaim: vnode: %p, ntnode: %d\n", vp, ip->i_number));

	if ((error = ntfs_ntget(ip)) != 0)
		return (error);
	
	/* Purge old data structures associated with the inode. */
	cache_purge(vp);

	ntfs_frele(fp);
	ntfs_ntput(ip);
	vnode_clearfsnode(vp);
	vnode_removefsref(vp);

	return (0);
}

static int
ntfs_blockmap(struct vnop_blockmap_args /* {
	vnode_t a_vp;
	off_t a_foffset;    Starting (logical) offset in the file
	size_t a_size;		Number of bytes to map (maximum)
	daddr64_t *a_bpn;	Physical (device) block number containing a_foffset, or -1 for sparse run
	size_t *a_run;		Number of bytes contiguous starting at that block # (no larger than a_size)
	void *a_poff;		Offset into physical (device) block
	int a_flags;
	vfs_context_t context;
	} */ *ap)
{
    register struct fnode *fp = VTOF(ap->a_vp);
    register struct ntnode *ip = FTONT(fp);
    register struct ntfsmount *ntmp = ip->i_mp;
	struct ntvattr *vap;
	int error;
	off_t off;
	off_t size;
	cn_t cn, cl;	/* Cluster number and length of a run */
	daddr_t bn;		/* Disk block (sector) number */
	int cnt;		/* Index into run list */
	
	off = ap->a_foffset;
	
	error = ntfs_ntvattrget(ntmp, ip, fp->f_attrtype, fp->f_attrname, ntfs_btocn(off), vfs_context_proc(ap->a_context), &vap);
	if (error)
		return error;
	
	/*
	 * The attribute must be non-resident and non-compressed in order to map it.
	 * Unfortunately, we can get called from inside buf_bread if there was no
	 * buffer header, but there was a valid page in VM.  This can happen if
	 * the file was left open while sufficient I/O was done (enough to recycle
	 * the buffer headers, but not enough to evict all resident pages).
	 *
	 * If we get called with a resident or compressed attribute, then just
	 * set the physical block equal to the logical block (indicating no
	 * valid mapping done yet) and return.
	 */
	if ((vap->va_flag & NTFS_AF_INRUN) == 0 ||		/* resident? */
		(vap->va_compression && vap->va_compressalg) != 0)	/* compressed? */
	{
		size_t bsize;	/* Size of one logical block */
		
		if (fp->f_compsize != 0)
			bsize = fp->f_compsize;		/* Compressed file: block == compression unit */
		else
			bsize = ntfs_cntob(1);		/* Others: block == cluster */

		bn = off / bsize;				/* indicate no valid mapping */
		size = MIN(ap->a_size, bsize);	/* "map" at most one cluster */
		goto done;						/* Note: error = 0 from ntfs_ntvattrget() call above */
	}

	/* Need to check whether offset or offset+size is past end of file? */
	
	/*
	 * Adjust offset and size to be relative to this attribute's start/end.
	 * ¥Is that right?  What do va_cnstart and va_cnend really mean?
	 */
	size = MIN(ap->a_size, ntfs_cntob(vap->va_vcnend+1) - off);
	off -= ntfs_cntob(vap->va_vcnstart);
	
	/*
	 * Loop over all of the runs for this attribute, ignoring runs
	 * containing data before the start of the requested range.
	 */
	cnt = 0;
	cn = 0;
	cl = 0;
	bn = 0;
	while (cnt < vap->va_vruncnt)
	{
		cn = vap->va_vruncn[cnt];	/* Start of this run */
		cl = vap->va_vruncl[cnt];	/* Length of this run */

		if (ntfs_cntob(cl) <= off)
		{
			/* current run is entirely before start of range; skip it */
			off -= ntfs_cntob(cl);
			cnt++;
			continue;
		}
		
		/* We've found the run containing the start of the range */
		cl -= ntfs_btocn(off);		/* Skip over clusters before start of range */
		if (cn || ip->i_number == NTFS_BOOTINO)
		{
			/* Current run is not sparse */
			cn += ntfs_btocn(off);		/* Cluster containing start of range */
			bn = ntfs_cntobn(cn);
		}
		else
		{
			/* Current run is sparse */
			bn = -1;
		}
		off = ntfs_btocnoff(off);	/* Offset of range with cluster #cn */
		size = MIN(size, ntfs_cntob(cl) - off);	/* Limit size to end of run */
		break;
	}
	if (cnt >= vap->va_vruncnt)
		panic("ntfs_cmap: tried to map past end of attribute");

done:
	ntfs_ntvattrrele(vap);
	
	if (ap->a_bpn)
		*ap->a_bpn = bn;
	if (ap->a_run)
		*ap->a_run = size;
	if (ap->a_poff)
		*(int *)ap->a_poff = 0;
	
	return error;
}


/* blktooff converts a logical block number to a file offset */
static int
ntfs_blktooff(ap)
	struct vnop_blktooff_args /* {
		vnode_t a_vp;
		daddr_t a_lblkno;
		off_t *a_offset;    
	} */ *ap;
{
	vnode_t vp;
	struct fnode *fp;
	struct ntnode *ip;
	struct ntfsmount *ntmp;
	size_t bsize;	/* Block size for this file (cluster, or compression unit) */

	vp = ap->a_vp;
	if (vp == NULL)
		return (EINVAL);
	fp = VTOF(vp);
	ip = FTONT(fp);
	ntmp = ip->i_mp;
	
	if (fp->f_compsize != 0)
		bsize = fp->f_compsize;		/* Compressed file: block == compression unit */
	else
		bsize = ntfs_cntob(1);		/* Others: block == cluster */

	*ap->a_offset = (off_t)ap->a_lblkno * bsize;

	return(0);
}

/* offtoblk converts a file offset to a logical block number */
static int
ntfs_offtoblk(ap)
struct vnop_offtoblk_args /* {
	vnode_t a_vp;
	off_t a_offset;    
	daddr_t *a_lblkno;
	} */ *ap;
{
	vnode_t vp;
	struct fnode *fp;
	struct ntnode *ip;
	struct ntfsmount *ntmp;
	size_t bsize;	/* Block size for this file (cluster, or compression unit) */

	vp = ap->a_vp;
	if (vp == NULL)
		return (EINVAL);
	fp = VTOF(vp);
	ip = FTONT(fp);
	ntmp = ip->i_mp;

	if (fp->f_compsize != 0)
		bsize = fp->f_compsize;		/* Compressed file: block == compression unit */
	else
		bsize = ntfs_cntob(1);		/* Others: block == cluster */

	*ap->a_lblkno = ap->a_offset / bsize;

	return(0);
}


/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ntfs_strategy(ap)
	struct vnop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	buf_t bp = ap->a_bp;
	vnode_t vp = buf_vnode(bp);
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct ntfsmount *ntmp = ip->i_mp;
	off_t offset;
	size_t bsize;
	int error=0;
	int bflags;

	dprintf(("ntfs_strategy: blkno: %d, lblkno: %d\n",
		(u_int32_t)buf_blkno(bp),
		(u_int32_t)buf_lblkno(bp)));

	dprintf(("strategy: bcount: %d flags: 0x%lx\n", 
		(u_int32_t)buf_count(bp), buf_flags(bp)));

	bflags = buf_flags(bp);
	/*
	 * If we're being called with a cluster based bp, then just
	 * pass the call through to the device.  This happens when
	 * cluster_read calls us, with everything mapped.
	 */
	if ((bflags & B_CLUSTER))
	{
		return buf_strategy(ip->i_devvp, ap);
	}

	if (fp->f_compsize != 0)
		bsize = fp->f_compsize;		/* Compressed files: one block = one compression unit */
	else
		bsize = ntfs_cntob(1);		/* Otherwise, one block = one cluster */
	offset = (off_t) buf_blkno(bp) * (off_t) bsize;

	if ((bflags & B_READ)) {
		u_int32_t toread;

		if (offset >= fp->f_size) {
			buf_clear(bp);
			error = 0;
		} else {
			toread = MIN(buf_count(bp),
				 fp->f_size - offset);
			dprintf(("ntfs_strategy: toread: %d, fsize: %d\n",
				toread,(u_int32_t)fp->f_size));

			error = ntfs_readattr(ntmp, ip, fp->f_attrtype,
				fp->f_attrname, offset,
				toread, (char *)buf_dataptr(bp), NULL, current_proc());

			if (error) {
				printf("ntfs_strategy: ntfs_readattr failed\n");
				buf_seterror(bp, error);
			}

			bzero((char *)buf_dataptr(bp) + toread, buf_count(bp) - toread);
		}
	} else {
#if NTFS_WRITE
		size_t tmp;
		u_int32_t towrite;

		if (offset + buf_count(bp) >= fp->f_size) {
			printf("ntfs_strategy: CAN'T EXTEND FILE\n");
			buf_seterror(bp, EFBIG);
		} else {
			towrite = MIN(buf_count(bp),
				fp->f_size - offset);
			dprintf(("ntfs_strategy: towrite: %d, fsize: %d\n",
				towrite,(u_int32_t)fp->f_size));

			error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,	
				fp->f_attrname, offset, towrite,
				(char *)buf_dataptr(bp), &tmp, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_writeattr fail\n");
				buf_seterror(bp, error);
			}
		}
#else
		panic("ntfs_strategy: write not implemented");
#endif
	}
	buf_biodone(bp);

	return (error);
}

static int
ntfs_write(ap)
	struct vnop_write_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_ioflag;
		vfs_context_t a_context;
	} */ *ap;
{
#if NTFS_WRITE
	vnode_t vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	off_t towrite;
	size_t written;
	int error;

	dprintf(("ntfs_write: ino: %d, off: %lld resid: %d\n",ip->i_number,uio_offset(uio),uio_resid(uio)));
	dprintf(("ntfs_write: filesize: %d",(u_int32_t)fp->f_size));

	if (uio_resid(uio) + uio_offset(uio) > fp->f_size) {
		printf("ntfs_write: CAN'T WRITE BEYOND END OF FILE\n");
		return (EFBIG);
	}

	towrite = MIN(uio_resid(uio), fp->f_size - uio_offset(uio));

	dprintf((", towrite: %d\n",(u_int32_t)towrite));

	error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,
		fp->f_attrname, uio_offset(uio), towrite, NULL, &written, uio);
#ifdef NTFS_DEBUG
	if (error)
		printf("ntfs_write: ntfs_writeattr failed: %d\n", error);
#endif

	return (error);
#else /* NTFS_WRITE */
	return EROFS;
#endif /* NTFS_WRITE */
}


/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
static int
ntfs_open(ap)
	struct vnop_open_args /* {
		vnode_t a_vp;
		int  a_mode;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	
	/*
	 * When opening files, set up the non-resident flag amd the
	 * compression unit size.  This makes those values available
	 * for read and page-in.  However, this breaks execution of
	 * compressed files because exec() never calls VOP_OPEN
	 * (though I think it should).
	 */
	if (vnode_isreg(vp)) {
		register struct fnode *fp = VTOF(vp);
		register struct ntnode *ip = FTONT(fp);
		struct ntfsmount *ntmp = ip->i_mp;
		struct ntvattr *vap;
		int error;
	
		fp->f_compsize = 0;		/* Assume not compressed */
		
		error = ntfs_ntvattrget(ntmp, ip, fp->f_attrtype, fp->f_attrname, 0, vfs_context_proc(ap->a_context), &vap);
		if (error)
			return error;
		
		if (vap->va_flag & NTFS_AF_INRUN) {
			fp->f_flag |= FN_NONRESIDENT;
			if ((vap->va_compression != 0) && (vap->va_compressalg != 0))
				fp->f_compsize = ntfs_cntob(1) << vap->va_compressalg;
		} else {
			fp->f_flag &= ~FN_NONRESIDENT;
		}
		
		ntfs_ntvattrrele(vap);
	}
	
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
	struct vnop_close_args /* {
		vnode_t a_vp;
		int  a_fflag;
		vfs_context_t a_context;
	} */ *ap;
{
#if NTFS_DEBUG
	vnode_t vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);

	printf("ntfs_close: %d\n",ip->i_number);
#endif

	return (0);
}

int
ntfs_readdir(ap)
	struct vnop_readdir_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_flags;
		int *a_eofflag;
		int *a_numdirent;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	int error = 0;
	u_int32_t faked = 0, num;
	struct dirent cde;
	off_t off;
	size_t namelen;

	if (ap->a_numdirent)
		*ap->a_numdirent = 0;
	
	if (ap->a_flags & (VNODE_READDIR_EXTENDED | VNODE_READDIR_REQSEEKOFF))
		return (EINVAL);

	dprintf(("ntfs_readdir %d off: %lld resid: %d\n",ip->i_number,uio_offset(uio),uio_resid(uio)));

	off = uio_offset(uio);

	/* Simulate . in every dir except ROOT */
	if( ip->i_number != NTFS_ROOTINO ) {
		struct dirent dot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 1, "." };

		if( uio_offset(uio) < sizeof(struct dirent) ) {
			dot.d_fileno = ip->i_number;
			error = uiomove((char *)&dot,sizeof(struct dirent),uio);
			if(error)
				return (error);
			if (ap->a_numdirent)
				++(*ap->a_numdirent);
		}
	}

	/* Simulate .. in every dir including ROOT */
	if( uio_offset(uio) < 2 * sizeof(struct dirent) ) {
		struct dirent dotdot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 2, ".." };

		/*¥ Don't we need to set dotdot.d_fileno? */
		error = uiomove((char *)&dotdot,sizeof(struct dirent),uio);
		if(error)
			return (error);
		if (ap->a_numdirent)
			++(*ap->a_numdirent);
	}

	faked = (ip->i_number == NTFS_ROOTINO) ? 1 : 2;
	num = uio_offset(uio) / sizeof(struct dirent) - faked;

	while( uio_resid(uio) >= sizeof(struct dirent) ) {
		struct attr_indexentry *iep;

		error = ntfs_ntreaddir(ntmp, fp, num, &iep, vfs_context_proc(ap->a_context));

		if(error)
			return (error);

		if( NULL == iep )
			break;

		for(; !(le32toh(iep->ie_flag) & NTFS_IEFLAG_LAST) &&
                            (uio_resid(uio) >= sizeof(struct dirent));
                        iep = NTFS_NEXTREC(iep, struct attr_indexentry *))
		{
			cde.d_fileno = le32toh(iep->ie_number);

			/* Hide system files. */
			if (cde.d_fileno != NTFS_ROOTINO && cde.d_fileno < 24)
					continue;
			if(!ntfs_isnamepermitted(ntmp,iep))
				continue;

			error = utf8_encodestr(iep->ie_fname, iep->ie_fnamelen * sizeof(u_int16_t),
				cde.d_name, &namelen, sizeof(cde.d_name), 0, BYTE_ORDER == BIG_ENDIAN ? UTF_REVERSE_ENDIAN : 0);
			if (error) {
				printf("ntfs_readdir: invalid name: directory %u, index %u", ip->i_number, num);	
				continue;	/* Skip over names which can't be converted */
			}
			dprintf(("ntfs_readdir: elem: %d, fname:[%s] type: %d, flag: %d, ",
				num, cde.d_name, iep->ie_fnametype,
				iep->ie_flag));
			cde.d_namlen = namelen;
			cde.d_type = (le32toh(iep->ie_fflag) & NTFS_FFLAG_DIR) ? DT_DIR : DT_REG;
			cde.d_reclen = sizeof(struct dirent);
			dprintf(("%s\n", (cde.d_type == DT_DIR) ? "dir":"reg"));

			error = uiomove((char *)&cde, sizeof(struct dirent), uio);
			if(error)
				return (error);
			if (ap->a_numdirent)
				++(*ap->a_numdirent);
			num++;
		}
	}

	dprintf(("ntfs_readdir: off: %lld resid: %d\n",
		uio_offset(uio),uio_resid(uio)));

	if (ap->a_eofflag)
	    *ap->a_eofflag = fp->f_size <= uio_offset(uio);

	return (error);
}

static int
ntfs_lookup(vnode_t dvp, vnode_t *vpp, struct componentname *cnp, proc_t p)
{
	struct ntnode *dip = VTONT(dvp);
	struct ntfsmount *ntmp = dip->i_mp;
	int error;

	dprintf(("ntfs_lookup: \"%.*s\" (%ld bytes) in %d, lp: %d, wp: %d \n",
		(int)cnp->cn_namelen, cnp->cn_nameptr, cnp->cn_namelen,
		dip->i_number, lockparent, wantparent));

	if ((cnp->cn_flags & ISLASTCN) &&
	    vnode_vfsisrdonly(dvp) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
	{
		return (EROFS);
	}

	if(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		dprintf(("ntfs_lookup: faking . directory in %d\n",
			dip->i_number));

		vnode_get(dvp);
		*vpp = dvp;
		error = 0;
	} else if (cnp->cn_flags & ISDOTDOT) {
		struct ntvattr *vap;
                ino_t parent_ino;
                
		dprintf(("ntfs_lookup: faking .. directory in %d\n",
			 dip->i_number));

		error = ntfs_ntvattrget(ntmp, dip, NTFS_A_NAME, NULL, 0, p, &vap);
		if(error)
			return (error);

		parent_ino = le32toh(vap->va_a_name->n_pnumber);
		dprintf(("ntfs_lookup: parentdir: %d\n",
			 parent_ino));
		error = ntfs_vgetex(ntmp->ntm_mountp, parent_ino, NULLVP, NULL, VNON, NTFS_A_DATA, NULL, 0, p, vpp);
		ntfs_ntvattrrele(vap);
	} else {
		error = ntfs_ntlookupfile(ntmp, dvp, cnp, p, vpp);
		if (error) {
			dprintf(("ntfs_ntlookupfile: returned %d\n", error));
			return (error);
		}

		dprintf(("ntfs_lookup: found ino: %d\n", 
			VTONT((*vpp))->i_number));
	}

	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, *vpp, cnp);
	return (error);
}


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
	struct vnop_lookup_args /* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t dvp;
	int lockparent;
	int error;
	vnode_t *vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	int flags = cnp->cn_flags;

	*vpp = NULL;
	dvp = ap->a_dvp;
	lockparent = flags & LOCKPARENT;

	if (!vnode_isdir(dvp))
		return (ENOTDIR);

	if ((flags & ISLASTCN) && vnode_vfsisrdonly(dvp) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
	{
		return (EROFS);
	}

	error = cache_lookup(dvp, vpp, cnp);

	if (error)
	{
		/* We found a cache entry, positive or negative, so return it. */
		if (error == -1)
			error = 0;		/* No error on positive match */
		return error;
	}
	
	return ntfs_lookup(dvp, vpp, cnp, vfs_context_proc(ap->a_context));
}

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
ntfs_fsync(ap)
	struct vnop_fsync_args /* {
		vnode_t a_vp;
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
ntfs_pathconf(ap)
	struct vnop_pathconf_args /* {
		vnode_t a_vp;
		int a_name;
		register_t *a_retval;
		vfs_context_t a_context;
	} */ *ap;
{
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

static int ntfs_pagein(ap)
	struct vnop_pagein_args /* {
		vnode_t a_vp;
		upl_t a_pl;
		vm_offset_t a_pl_offset;
		off_t a_f_offset;
		size_t a_size;
		int a_flags;
		vfs_context_t a_context;
	} */ *ap;
{
    int error;
    kern_return_t kret;
    vm_offset_t ioaddr;
    vnode_t vp = ap->a_vp;
    upl_t pl = ap->a_pl;
    vm_offset_t pl_offset = ap->a_pl_offset;
    off_t f_offset = ap->a_f_offset;
    size_t size = ap->a_size;
    int flags = ap->a_flags;
    struct fnode *fp = VTOF(vp);
    struct ntnode *ip = FTONT(fp);
    struct ntfsmount *ntmp = ip->i_mp;
    
	/*
	 * Determine whether we can use Cluster I/O on this attribute.  We can
	 * use it if the attribute is not MFT resident, and if it is not compressed.
	 */
    if ((fp->f_flag & FN_NONRESIDENT) && (fp->f_compsize == 0))
    {
        error = cluster_pagein(vp, pl, pl_offset, f_offset, size,
                fp->f_size, flags);
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
                size, (caddr_t)ioaddr, NULL, vfs_context_proc(ap->a_context));
        
        if (error)
        {
            panic("ntfs_pagein: read error = %d", error);
            size = 0;
        }
        
        /* Zero fill part of page past EOF */
        if (ap->a_size > size)
            bzero((caddr_t)ioaddr+size, ap->a_size-size);
        
        /*¥ If mounted read/write, we'd update the access time here. */
        
        kret = ubc_upl_unmap(pl);
        if (kret != KERN_SUCCESS)
            panic("ntfs_pagein: ubc_upl_unmap error = %d", kret);
        
        /* Commit/abort pages unless requested not to */
        if ((flags & UPL_NOCOMMIT) == 0)
        {
            if (error)
                ubc_upl_abort_range(pl, pl_offset, ap->a_size, UPL_ABORT_FREE_ON_EMPTY);
            else
                ubc_upl_commit_range(pl, pl_offset, ap->a_size, UPL_COMMIT_FREE_ON_EMPTY);
        }
    }
    
    if (error)
        panic("ntfs_pagein error = %d", error);
        
    return error;
}


/*
 * The following vnode operations aren't supported because we are a
 * read-only file system.
 */

static int
ntfs_create(struct vnop_create_args *ap)
{
	return EROFS;
}

static int
ntfs_mknod(struct vnop_mknod_args *ap)
{
	return EROFS;
}

static int
ntfs_setattr(struct vnop_setattr_args *ap)
{
	return EROFS;
}

static int
ntfs_remove(struct vnop_remove_args *ap)
{
	return EROFS;
}

static int
ntfs_link(struct vnop_link_args *ap)
{
	return EROFS;
}

static int
ntfs_rename(struct vnop_rename_args *ap)
{
	return EROFS;
}

static int
ntfs_mkdir(struct vnop_mkdir_args *ap)
{
	return EROFS;
}

static int
ntfs_rmdir(struct vnop_rmdir_args *ap)
{
	return EROFS;
}

static int
ntfs_allocate(struct vnop_allocate_args *ap)
{
	return EROFS;
}

/*
 * Global vfs data structures
 */
typedef int     vnop_t __P((void *));

vnop_t **ntfs_vnodeop_p;
static struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
	{ &vnop_default_desc, (vnop_t *)vn_default_error },
	{ &vnop_getattr_desc, (vnop_t *)ntfs_getattr },
	{ &vnop_inactive_desc, (vnop_t *)ntfs_inactive },
	{ &vnop_reclaim_desc, (vnop_t *)ntfs_reclaim },
	{ &vnop_pathconf_desc, (vnop_t *)ntfs_pathconf },
	{ &vnop_lookup_desc, (vnop_t *)ntfs_cache_lookup },
	{ &vnop_close_desc, (vnop_t *)ntfs_close },
	{ &vnop_open_desc, (vnop_t *)ntfs_open },
	{ &vnop_readdir_desc, (vnop_t *)ntfs_readdir },
	{ &vnop_fsync_desc, (vnop_t *)ntfs_fsync },
	{ &vnop_blockmap_desc, (vnop_t *) ntfs_blockmap },
	{ &vnop_blktooff_desc, (vnop_t *)ntfs_blktooff },
	{ &vnop_offtoblk_desc, (vnop_t *)ntfs_offtoblk },
	{ &vnop_strategy_desc, (vnop_t *)ntfs_strategy },
	{ &vnop_read_desc, (vnop_t *)ntfs_read },
	{ &vnop_write_desc, (vnop_t *)ntfs_write },
	{ &vnop_pagein_desc, (vnop_t *) ntfs_pagein },
/*	{ &vnop_advlock_desc, (vnop_t *) ntfs_advlock },		Needed for Carbon to open files for writing */
	{ &vnop_create_desc, (vnop_t *) ntfs_create },
	{ &vnop_mknod_desc, (vnop_t *) ntfs_mknod },
	{ &vnop_setattr_desc, (vnop_t *) ntfs_setattr },
	{ &vnop_remove_desc, (vnop_t *) ntfs_remove },
	{ &vnop_link_desc, (vnop_t *) ntfs_link },
	{ &vnop_rename_desc, (vnop_t *) ntfs_rename },
	{ &vnop_mkdir_desc, (vnop_t *) ntfs_mkdir },
	{ &vnop_rmdir_desc, (vnop_t *) ntfs_rmdir },
	{ &vnop_allocate_desc, (vnop_t *) ntfs_allocate },
	{ NULL, NULL }
};

struct vnodeopv_desc ntfs_vnodeop_opv_desc =
	{ &ntfs_vnodeop_p, ntfs_vnodeop_entries };
