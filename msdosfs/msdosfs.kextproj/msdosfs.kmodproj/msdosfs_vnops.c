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
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>
#include <sys/ubc.h>
#include <sys/utfconv.h>
#include <sys/attr.h>
#include <sys/namei.h>
#include <sys/md5.h>

#include <machine/spl.h>

#include "bpb.h"
#include "direntry.h"
#include "denode.h"
#include "msdosfsmount.h"
#include "fat.h"
#include "msdosfs_lockf.h"

#define	DOS_FILESIZE_MAX	0xffffffff

#define	GENERIC_DIRSIZ(dp) \
    ((sizeof (struct dirent) - (MAXNAMLEN+1)) + (((dp)->d_namlen+1 + 3) &~ 3))

extern int groupmember(gid_t gid, ucred_t cred);

/*
 * Prototypes for MSDOSFS vnode operations
 */
static int msdosfs_create __P((struct vnop_create_args *));
static int msdosfs_mknod __P((struct vnop_mknod_args *));
static int msdosfs_close __P((struct vnop_close_args *));
static int msdosfs_getattr __P((struct vnop_getattr_args *));
static int msdosfs_setattr __P((struct vnop_setattr_args *));
static int msdosfs_read __P((struct vnop_read_args *));
static int msdosfs_write __P((struct vnop_write_args *));
static int msdosfs_pagein __P((struct vnop_pagein_args *));
static int msdosfs_fsync __P((struct vnop_fsync_args *));
static int msdosfs_remove __P((struct vnop_remove_args *));
static int msdosfs_rename __P((struct vnop_rename_args *));
static int msdosfs_mkdir __P((struct vnop_mkdir_args *));
static int msdosfs_rmdir __P((struct vnop_rmdir_args *));
static int msdosfs_readdir __P((struct vnop_readdir_args *));
static int msdosfs_strategy __P((struct vnop_strategy_args *));
static int msdosfs_pathconf __P((struct vnop_pathconf_args *ap));
static int msdosfs_advlock(struct vnop_advlock_args *ap);

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
		Symbolic Links for FAT

FAT does not have native support for symbolic links (symlinks).  We
implement them using ordinary files with a particular format.  Our
symlink file format is modeled after the SMB for Mac OS X implementation.

Symlink files are ordinary text files that look like:

XSym
1234
00112233445566778899AABBCCDDEEFF
/the/sym/link/path

The lines of the file are separated by ASCII newline (0x0A).  The first
line is a "magic" value to help identify the file.  The second line is
the length of the symlink itself; it is four decimal digits, with leading
zeroes.  The third line is the MD5 checksum of the symlink as 16
hexadecimal bytes.  The fourth line is the symlink, up to 1024 bytes long.
If the symlink is less than 1024 bytes, then it is padded with a single
newline character and as many spaces as needed to occupy 1024 bytes.

The file size is exactly 1067 (= 4 + 1 + 4 + 1 + 32 + 1 + 1024) bytes.
When we encounter an ordinary file whose length is 1067, we must read
it to verify that the header (including length and MD5 checksum) is
correct.

Since the file size is constant, we use the de_FileSize field in the
denode to store the actual length of the symlink.  That way, we only
check and parse the header once at vnode creation time.

*/

static const char symlink_magic[5] = "XSym\n";

#define SYMLINK_LINK_MAX 1024

struct symlink {
	char magic[5];		/* == symlink_magic */
	char length[4];		/* four decimal digits */
	char newline1;		/* '\n' */
	char md5[32];		/* MD5 hex digest of "length" bytes of "link" field */
	char newline2;		/* '\n' */
	char link[SYMLINK_LINK_MAX]; /* "length" bytes, padded by '\n' and spaces */
};


/*
 * Return the owning user ID for a given volume.  If the volume was mounted
 * with "unknown permissions", then the user ID making the request is the
 * owner.  This is the "everyone is an owner" model.
 */
__private_extern__ uid_t
get_pmuid(struct msdosfsmount *pmp, uid_t current_user)
{
	if (vfs_flags(pmp->pm_mountp) & MNT_UNKNOWNPERMISSIONS)
		return current_user;
	else
		return pmp->pm_uid;
}

/*
 * Create a regular file.
 */
static int
msdosfs_create(ap)
	struct vnop_create_args /* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t dvp = ap->a_dvp;
	struct denode *pdep = VTODE(dvp);
	struct componentname *cnp = ap->a_cnp;
	vfs_context_t context = ap->a_context;
	struct denode ndirent;
	struct denode *dep;
	struct timespec ts;
	int error;
	u_long va_flags;
	u_long offset;		/* byte offset in directory for new entry */
	u_long long_count;	/* number of long name entries needed */

	/*
	 * Find space in the directory to place the new name.
	 */
	error = findslots(pdep, cnp, &ndirent.de_LowerCase, &offset, &long_count, context);
	if (error)
		goto exit;

	/*
	 * If this is the root directory and there is no space left we
	 * can't do anything.  This is because the root directory can not
	 * change size.  (FAT12 and FAT16 only)
	 *
	 * On FAT32, we can grow the root directory, and de_StartCluster
	 * will be the actual cluster number of the root directory.
	 */
	if (pdep->de_StartCluster == MSDOSFSROOT
	    && offset >= pdep->de_FileSize)
	{
		error = ENOSPC;
		goto exit;
	}

	/*
	 * Create a directory entry for the file, then call createde() to
	 * have it installed. NOTE: DOS files are always executable.
	 * The supplied mode is ignored (DOS doesn't store mode, so
	 * all files on a volume have a constant mode).  The immutable
	 * flag is used to set DOS's read-only attribute.
	 */
	bzero(&ndirent, sizeof(ndirent));
	error = uniqdosname(pdep, cnp, ndirent.de_Name, &ndirent.de_LowerCase, context);
	if (error)
	{
		goto exit;
	}

	// Set read-only attribute if one of the immutable bits is set.
	// Always set the "needs archive" attribute on newly created files.
	va_flags = ap->a_vap->va_flags;
	if (va_flags != VNOVAL && (va_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) != 0)
		ndirent.de_Attributes = ATTR_ARCHIVE | ATTR_READONLY;
	else
		ndirent.de_Attributes = ATTR_ARCHIVE;

	/*
	 * If the file name starts with ".", make it invisible on Windows.
	 */
	if (cnp->cn_nameptr[0] == '.')
		ndirent.de_Attributes |= ATTR_HIDDEN;

	ndirent.de_StartCluster = 0;
	ndirent.de_FileSize = 0;
	ndirent.de_dev = pdep->de_dev;
	ndirent.de_devvp = pdep->de_devvp;
	ndirent.de_pmp = pdep->de_pmp;
	ndirent.de_flag = DE_ACCESS | DE_CREATE | DE_UPDATE;
	getnanotime(&ts);
	DETIMES(&ndirent, &ts, &ts, &ts);
	error = createde(&ndirent, pdep, &dep, cnp, offset, long_count, context);
	if (error == 0)
	{
		*ap->a_vpp = DETOV(dep);
		cache_purge_negatives(dvp);
	}

exit:
	msdosfs_meta_flush(pdep->de_pmp);
	return (error);
}

static int
msdosfs_mknod(ap)
	struct vnop_mknod_args /* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	/* We don't support special files */
	return EINVAL;
}

static int
msdosfs_open(ap)
	struct vnop_open_args /* {
		vnode_t a_vp;
		int a_mode;
		vfs_context_t a_context;
	} */ *ap;
{
	return 0;
}

static int
msdosfs_close(ap)
	struct vnop_close_args /* {
		vnode_t a_vp;
		int a_fflag;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct denode *dep = VTODE(vp);

	cluster_push(vp, IO_CLOSE);
	deupdat(dep, 0, ap->a_context);
	msdosfs_meta_flush(dep->de_pmp);

	return 0;
}

static int
msdosfs_getattr(ap)
	struct vnop_getattr_args /* {
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	struct vnode_attr *vap = ap->a_vap;

	VATTR_RETURN(vap, va_rdev, 0);
	VATTR_RETURN(vap, va_nlink, 1);
	VATTR_RETURN(vap, va_total_size, dep->de_FileSize);
	VATTR_RETURN(vap, va_total_alloc, (dep->de_FileSize + pmp->pm_crbomask) & ~pmp->pm_crbomask);
	VATTR_RETURN(vap, va_data_size, dep->de_FileSize);
	VATTR_RETURN(vap, va_data_alloc, vap->va_total_alloc);
	VATTR_RETURN(vap, va_uid, 99);
	VATTR_RETURN(vap, va_gid, 99);
	VATTR_RETURN(vap, va_mode, ALLPERMS & pmp->pm_mask);
	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		vap->va_flags = 0;
		if ((dep->de_Attributes & ATTR_ARCHIVE) == 0)	// DOS: flag set means "needs to be archived"
			vap->va_flags |= SF_ARCHIVED;				// BSD: flag set means "has been archived"
		if (dep->de_Attributes & ATTR_READONLY)			// DOS read-only becomes user immutable
			vap->va_flags |= UF_IMMUTABLE;
		VATTR_SET_SUPPORTED(vap, va_flags);
	}
	
	/* FAT doesn't support extended security data */
	
	if (vap->va_active & (VNODE_ATTR_va_create_time |
		VNODE_ATTR_va_access_time | VNODE_ATTR_va_modify_time |
		VNODE_ATTR_va_change_time))
	{
		struct timespec ts;
		getnanotime(&ts);
		DETIMES(dep, &ts, &ts, &ts);
		
		dos2unixtime(dep->de_CDate, dep->de_CTime, 0, &vap->va_create_time);
		dos2unixtime(dep->de_ADate, 0, 0, &vap->va_access_time);
		dos2unixtime(dep->de_MDate, dep->de_MTime, 0, &vap->va_modify_time);
		vap->va_change_time = vap->va_modify_time;
		/* FAT doesn't have a backup date/time */
		
		vap->va_supported |= VNODE_ATTR_va_create_time |
			VNODE_ATTR_va_access_time |
			VNODE_ATTR_va_modify_time |
			VNODE_ATTR_va_change_time;
	}
	
	if (VATTR_IS_ACTIVE(vap, va_fileid))
		VATTR_RETURN(vap, va_fileid, defileid(dep));
	/* FAT has no va_linkid, and no easy access to va_parentid */

	VATTR_RETURN(vap, va_fsid, dep->de_dev);
	VATTR_RETURN(vap, va_filerev, dep->de_modrev);
	VATTR_RETURN(vap, va_gen, 0);
	
	return 0;
}

static int
msdosfs_setattr(struct vnop_setattr_args *ap)
	/* {
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */
{
	struct denode *dep = VTODE(ap->a_vp);
	struct vnode_attr *vap = ap->a_vap;
	int error = 0;

	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
		if (dep->de_FileSize != vap->va_data_size) {
			if (dep->de_Attributes & ATTR_DIRECTORY)
				return EPERM;	/* Cannot change size of a directory! */
			error = detrunc(dep, vap->va_data_size, 0, ap->a_context);
			if (error)
				goto exit;
		}
		VATTR_SET_SUPPORTED(vap, va_data_size);
	}
	
	/* FAT does not support setting uid, gid or mode */
	
	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		/*
		 * Here we are strict, stricter than ufs in not allowing
		 * users to attempt to set SF_SETTABLE bits or anyone to
		 * set unsupported bits.  However, we ignore attempts to
		 * set ATTR_ARCHIVE for directories `cp -pr' from a more
		 * sensible file system attempts it a lot.
		 */
        
		if (vap->va_flags & ~(SF_ARCHIVED | SF_IMMUTABLE | UF_IMMUTABLE))
		{
			error = EINVAL;
			goto exit;
		}
            
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
		VATTR_SET_SUPPORTED(vap, va_flags);
	}

	/*
	 * Update times.  Since we don't explicitly store a change time, we
	 * don't let you set it here.  (An alternative behavior would be to
	 * set the denode's mod time to the greater of va_modify_time and
	 * va_change_time.)
	 */
	if (vap->va_active & (VNODE_ATTR_va_create_time | VNODE_ATTR_va_access_time |
		VNODE_ATTR_va_modify_time))
	{
		if (VATTR_IS_ACTIVE(vap, va_create_time)) {
			unix2dostime(&vap->va_create_time, &dep->de_CDate, &dep->de_CTime, NULL);
			VATTR_SET_SUPPORTED(vap, va_create_time);
		}
		if (VATTR_IS_ACTIVE(vap, va_access_time)) {
			unix2dostime(&vap->va_access_time, &dep->de_ADate, NULL, NULL);
			VATTR_SET_SUPPORTED(vap, va_access_time);
		}
		if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
			unix2dostime(&vap->va_modify_time, &dep->de_MDate, &dep->de_MTime, NULL);
			VATTR_SET_SUPPORTED(vap, va_modify_time);
		}
		dep->de_Attributes |= ATTR_ARCHIVE;
		dep->de_flag |= DE_MODIFIED;
	}

	error = deupdat(dep, 1, ap->a_context);
	msdosfs_meta_flush(dep->de_pmp);

exit:
	return error;
}


static int
msdosfs_read(ap)
	struct vnop_read_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_ioflag;
		vfs_context_t a_context;
	} */ *ap;
{
	int error = 0;
	int orig_resid;
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	vfs_context_t context = ap->a_context;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;

	if (uio_offset(uio) < 0)
		return EINVAL;

	if (uio_offset(uio) > DOS_FILESIZE_MAX)
		return 0;

	/* If they didn't ask for any data, then we are done. */
	orig_resid = uio_resid(uio);
	if (orig_resid <= 0)
		return 0;

	if (vnode_isreg(vp)) {
		error = cluster_read(vp, uio, (off_t)dep->de_FileSize, 0);
		if (error == 0)
			dep->de_flag |= DE_ACCESS;
	}
	else
	{
		u_long blsize;
		u_int n;
		u_long diff;
		u_long on;
		daddr64_t lbn;
		buf_t bp;

		/* The following code is only used for reading directories */
		
		do {
			if (uio_offset(uio) >= dep->de_FileSize)
				break;
			lbn = de_cluster(pmp, uio_offset(uio));
			/*
			 * If we are operating on a directory file then be sure to
			 * do i/o with the vnode for the filesystem instead of the
			 * vnode for the directory.
			 */
			/* convert cluster # to block # */
			error = pcbmap(dep, lbn, 1, &lbn, NULL, &blsize, context);
			if (error == E2BIG) {
				error = EINVAL;
				break;
			} else if (error)
				break;
			error = (int)buf_meta_bread(pmp->pm_devvp, lbn, blsize, vfs_context_ucred(context), &bp);
			if (error) {
				buf_brelse(bp);
				break;
			}
			on = uio_offset(uio) & pmp->pm_crbomask;
			diff = pmp->pm_bpcluster - on;
			n = diff > uio_resid(uio) ? uio_resid(uio) : diff;
			diff = dep->de_FileSize - uio_offset(uio);
			if (diff < n)
				n = diff;
			diff = blsize - buf_resid(bp);
			if (diff < n)
				n = diff;
			error = uiomove((char *)buf_dataptr(bp) + on, (int) n, uio);
			buf_brelse(bp);
		} while (error == 0 && uio_resid(uio) > 0 && n != 0);
	}

	return error;
}

/*
 * Write data to a file or directory.
 */
static int
msdosfs_write(ap)
	struct vnop_write_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_ioflag;
		vfs_context_t a_context;
	} */ *ap;
{
	int error;
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int ioflag = ap->a_ioflag;
	vfs_context_t context = ap->a_context;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	off_t zero_off;
	u_int32_t original_size;
	u_int32_t count;
	u_int32_t lastcn;
	u_int32_t filesize;
	int   lflag;
	user_ssize_t original_resid;
	off_t original_offset;
	off_t offset;

	switch (vnode_vtype(vp)) {
	case VREG:
		break;
	case VDIR:
		return EISDIR;
	default:
		panic("msdosfs_write: bad file type");
		return EINVAL;
	}

	/*
	 * Remember some values in case the write fails.
	 */
	original_resid = uio_resid(uio);
	original_size = dep->de_FileSize;
	original_offset = uio_offset(uio);
	offset = original_offset;
	
	if (ioflag & IO_APPEND) {
		uio_setoffset(uio, dep->de_FileSize);
		offset = dep->de_FileSize;
	}

	if (offset < 0)
		return EFBIG;

	if (original_resid == 0)
		return 0;

	if (offset + original_resid > DOS_FILESIZE_MAX)
		return EFBIG;

	/*
	 * If the offset we are starting the write at is beyond the end of
	 * the file, then they've done a seek.  Unix filesystems allow
	 * files with holes in them, DOS doesn't so we must fill the hole
	 * with zeroed blocks.
	 */

	/*
	 * If we write beyond the end of the file, extend it to its ultimate
	 * size ahead of the time to hopefully get a contiguous area.
	 */
    if (offset + original_resid > original_size) {
        count = de_clcount(pmp, offset + original_resid) -
        		de_clcount(pmp, original_size);
        error = extendfile(dep, count, context);
        if (error &&  (error != ENOSPC || (ioflag & IO_UNIT)))
            goto errexit;
        lastcn = dep->de_fc[FC_LASTFC].fc_frcn;
		filesize = offset + original_resid;
    } else {
		lastcn = de_clcount(pmp, original_size) - 1;
		filesize = original_size;
	}
	
	lflag = (ioflag & IO_SYNC);

	if (offset > original_size) {
		zero_off = original_size;
		lflag   |= IO_HEADZEROFILL;
	} else
		zero_off = 0;
	
	/*
	 * if the write starts beyond the current EOF then we'll
	 * zero fill from the current EOF to where the write begins
	 */
	error = cluster_write(vp, uio, (off_t)original_size, (off_t)filesize,
				(off_t)zero_off,
				(off_t)0, lflag);
	
	if (uio_offset(uio) > dep->de_FileSize) {
		dep->de_FileSize = uio_offset(uio);
		ubc_setsize(vp, (off_t)dep->de_FileSize);
	}

	if (original_resid > uio_resid(uio))
		dep->de_flag |= DE_UPDATE;
	
	/*
	 * If the write failed and they want us to, truncate the file back
	 * to the size it was before the write was attempted.
	 */
errexit:
	if (error) {
		if (ioflag & IO_UNIT) {
			detrunc(dep, original_size, ioflag & IO_SYNC, context);
			uio_setoffset(uio, original_offset);
			uio_setresid(uio, original_resid);
		} else {
			detrunc(dep, dep->de_FileSize, ioflag & IO_SYNC, context);
			if (uio_resid(uio) != original_resid)
				error = 0;
		}
	} else if (ioflag & IO_SYNC)
		error = deupdat(dep, 1, context);
	msdosfs_meta_flush(pmp);

	return error;
}

static int
msdosfs_pagein(ap)
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
	vnode_t vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	int error;
	
	error = cluster_pagein(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
				ap->a_size, (off_t)dep->de_FileSize,
				ap->a_flags);
	
	return error;
}

static int
msdosfs_pageout(ap)
	struct vnop_pageout_args /* {
		vnode_t a_vp;
		upl_t a_pl;
		vm_offset_t a_pl_offset;
		off_t a_f_offset;
		size_t a_size;
		int a_flags;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	int error;

	error = cluster_pageout(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
				ap->a_size, (off_t)dep->de_FileSize,
				ap->a_flags);

	return error;
}

__private_extern__ int
msdosfs_fsync_internal(vnode_t vp, int sync, int do_dirs, vfs_context_t context)
{
	/*
	 * First of all, write out any clusters.
	 */
	cluster_push(vp, 0);

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
	buf_flushdirtyblks(vp, sync, 0, (char *)"msdosfs_fsync_internal");

	if (do_dirs && vnode_isdir(vp))
		(void) msdosfs_dir_flush(VTODE(vp), sync, context);

	return deupdat(VTODE(vp), sync, context);
}

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
msdosfs_fsync(ap)
	struct vnop_fsync_args /* {
		vnode_t a_vp;
		int a_waitfor;
		vfs_context_t a_context;
	} */ *ap;
{
	int error;
	vnode_t vp = ap->a_vp;
	
	error = msdosfs_fsync_internal(vp, ap->a_waitfor == MNT_WAIT, 1, ap->a_context);

	msdosfs_meta_flush(VTODE(vp)->de_pmp);

	return error;
}

/*
 * Remove (unlink) a file.
 *
 * On entry, vp has been suspended, so there are no pending
 * calls using it.
 *
 */
static int
msdosfs_remove(ap)
	struct vnop_remove_args /* {
		vnode_t a_dvp;
		vnode_t a_vp;
		struct componentname *a_cnp;
		int a_flags;
		vfs_context_t a_context;
	} */ *ap;
{
    vnode_t vp = ap->a_vp;
    vnode_t dvp = ap->a_dvp;
    struct denode *dep = VTODE(vp);
    struct denode *ddep = VTODE(dvp);
    int error;

    /* Make sure the file isn't read-only */
    /* Note that this is an additional imposition over the
       normal deletability rules... */
    if (dep->de_Attributes & ATTR_READONLY)
    {
        error = EPERM;
        goto exit;
    }

	/* Don't allow deletes of busy files (option used by Carbon) */
	if ((ap->a_flags & VNODE_REMOVE_NODELETEBUSY) && vnode_isinuse(vp, 0))
	{
		error = EBUSY;
		goto exit;
	}

	cache_purge(vp);
	dep->de_refcnt--;
    error = removede(ddep, dep->de_diroffset, ap->a_context);

exit:
	msdosfs_meta_flush(ddep->de_pmp);

	return error;
}

/*
 * Renames on files require moving the denode to a new hash queue since the
 * denode's parent is used to compute which hash queue to put the file
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
 * Locking:
 *	On entry, VFS has locked all of the vnodes in an
 *	arbitrary, but consistent, order.  For example,
 *	it may lock the parent with the smallest vnode pointer,
 *	then its child, then the parent with the larger vnode
 *	pointer, then its child.  These locks prevent either
 *	the source or destination (if any) from changing, and
 *	prevents changes to either the source or destination
 *	parent directories.
 *
 *	Traditionally, one of the hardest parts about rename's
 *	locking is when the source is a directory, and we
 *	have to verify that the destination parent isn't a
 *	descendant of the source.  We need to walk up the
 *	directory hierarchy from the destination parent up to
 *	the root.  If any of those directories is the source,
 *	the operation is invalid.  But walking up the hierarchy
 *	violates the top-down lock order; to avoid deadlock, we
 *	must not have two nodes locked at the same time.
 *
 *	But there is a solution: a mutex on rename operations for
 *	the entire volume (that is, only one rename at a time
 *	per volume).  During the walk up the hierarchy, we must
 *	prevent any change to the hierarchy that could cause the
 *	destination parent to become or stop being an ancestor of
 *	the source.  Any operation on a file can't affect the
 *	ancestry of directories.  Directory create operations can't
 *	affect the ancestry of pre-existing directories.
 *	Directory delete operations outside the ancestry path
 *	don't matter, and deletes within the ancestry path will
 *	fail as long as the directories remain locked (the delete
 *	will fail because the directory is locked, or not empty, or
 *	both).  The only operation that can affect the ancestry is
 *	other rename operations on the same volume (and even then,
 *	only if the source parent is not the destination parent).
 *
 *	It is sufficient if the rename mutex is taken only
 *	when the source parent is not the destination parent.
 */
static int
msdosfs_rename(ap)
	struct vnop_rename_args /* {
		vnode_t a_fdvp;
		vnode_t a_fvp;
		struct componentname *a_fcnp;
		vnode_t a_tdvp;
		vnode_t a_tvp;
		struct componentname *a_tcnp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t tdvp = ap->a_tdvp;
	vnode_t fvp = ap->a_fvp;
	vnode_t fdvp = ap->a_fdvp;
	vnode_t tvp = ap->a_tvp;
	struct componentname *tcnp = ap->a_tcnp;
	vfs_context_t context = ap->a_context;
	u_char toname[11], oldname[11];
	u_long to_diroffset;
	u_long to_long_count;
	u_int32_t from_offset;
	u_int8_t new_deLowerCase;	/* deLowerCase corresponding to toname */
	int doingdirectory = 0, newparent = 0;
    int change_case;
	int error;
	u_long cn;
	daddr64_t bn = 0;
	struct denode *fddep;	/* from file's parent directory	 */
	struct denode *fdep;	/* from file or directory	 */
	struct denode *tddep;	/* to file's parent directory	 */
	struct denode *tdep;	/* to file or directory		 */
	struct msdosfsmount *pmp;
	struct buf *bp;
	
	fddep = VTODE(fdvp);
	fdep = VTODE(fvp);
	tddep = VTODE(tdvp);
	tdep = tvp ? VTODE(tvp) : NULL;
	pmp = fddep->de_pmp;

	/*
	 * If source and dest are the same, then it is a rename
	 * that may be changing the upper/lower case of the name.
	 */
    change_case = 0;
	if (tvp == fvp) {
        /* Pretend the destination doesn't exist. */
        tvp = NULL;
        
        /* Remember we're changing case, so we can skip uniqdosname() */
        change_case = 1;
	}

	/*
	 * Figure out where to put the new directory entry.
	 */
	error = findslots(tddep, tcnp, &new_deLowerCase, &to_diroffset, &to_long_count, context);
	if (error)
		goto exit;

	/* Make sure the destination vnode (if any) can be deleted */
	/* Note that these readonly checks are additional impositions
	   over and above the usual deletability checks */
	if (tdep && (tdep->de_Attributes & ATTR_READONLY))
	{
		error = EPERM;
		goto exit;
	}
    
	/* Make sure the source vnode can be changed */
	if (fdep->de_Attributes & ATTR_READONLY)
	{
		error = EPERM;
		goto exit;
	}

	if (fdep->de_Attributes & ATTR_DIRECTORY)
		doingdirectory = 1;
	
	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory heirarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..".
	 */
	if (fddep->de_StartCluster != tddep->de_StartCluster)
		newparent = 1;
	if (doingdirectory && newparent) {
		error = doscheckpath(fdep, tddep, context);
		if (error) goto exit;
	}

	/*
	 * Remember the offset of fdep within fddep.
	 */
	if (doingdirectory)
	{
		error = msdosfs_lookupdir(fddep, fdep, &from_offset, context);
		if (error) goto exit;
	}
	else
		from_offset = fdep->de_diroffset;

	if (tvp != NULL) {
		uint32_t offset;	/* Of the pre-existing entry being removed */
		
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if (tdep->de_Attributes & ATTR_DIRECTORY) {
			if (!dosdirempty(tdep, context)) {
				error = ENOTEMPTY;
				goto exit;
			}
			if (!doingdirectory) {
				error = ENOTDIR;
				goto exit;
			}
			error = msdosfs_lookupdir(tddep, tdep, &offset, context);
			if (error) goto exit;
		} else {
			if (doingdirectory) {
				error = EISDIR;
				goto exit;
			}
			offset = tdep->de_diroffset;
		}
		cache_purge(tvp);		/* Purge tvp before we delete it on disk */
		tdep->de_refcnt--;
		error = removede(tddep, offset, context);
		if (error) goto exit;
	}

	/*
	 * Convert the filename in tcnp into a dos filename. We copy this
	 * into the denode and directory entry for the destination
	 * file/directory.
     *
     * NOTE: uniqdosname also makes sure that the short name does not
     * already exist in the destination directory.  When changing case,
     * that short name already exists (for the source object), so we
     * have to skip the call to uniqdosname.  If we're changing case,
     * we keep the short name as-is, and the call to findslots()
     * set up new_deLowerCase for the new dir entry.
	 */
	if (!change_case) {
        error = uniqdosname(tddep, tcnp, toname, &new_deLowerCase, context);
        /*¥ What if we get an error and we already deleted the target? */
        if (error) goto exit;
    }

    if (change_case) {
    	/*
         * Since we're just changing case, the short name should stay
         * the same, and we couldn't call uniqdosname() above to set
         * up "toname".  So, set it up now.
         *
         * For a file, the short name is already in the denode, and
         * we can just copy it.  But for a directory, the denode actually
         * points at the "." entry in the directory, and the name stored
         * is ".".  So, for a directory, we have to go read in the
         * short name entry as it exists in the parent directory.
         */
        if (doingdirectory)
        {
        	u_long blsize;
        	struct dosdirentry *direntp;
        	
        	/* Read in fdep's entry in fddep */
        	error = pcbmap(fddep, de_cluster(pmp, from_offset), 1, &bn, NULL, &blsize, context);
        	if (error) goto exit;
        	/*¥ On error, we had already removed tdep */
        	
        	error = (int)buf_meta_bread(pmp->pm_devvp, bn, blsize, vfs_context_ucred(context), &bp);
        	if (error) {
        		buf_brelse(bp);
        		/*¥ On error, we had already removed tdep */
        		goto exit;
        	}
        	
        	/* Copy the name from the entry in fddep */
        	direntp = bptoep(pmp, bp, from_offset);
        	bcopy(direntp->deName, toname, 11);
        	buf_brelse(bp);
        }
        else
        {
			bcopy(fdep->de_Name, toname, 11);
		}
	}

	/*
	 * First write a new entry in the destination
	 * directory and mark the entry in the source directory
	 * as deleted.  Then move the denode to the correct hash
	 * chain for its new location in the filesystem.  And, if
	 * we moved a directory, then update its .. entry to point
	 * to the new parent directory.
	 *
	 * The name in the denode is updated for files.  For
	 * directories, the denode points at the "." entry in
	 * the directory, so temporarily change the name in
	 * the denode, and restore it; otherwise the "." entry
	 * may be overwritten.
	 */
	cache_purge(fvp);
	bcopy(fdep->de_Name, oldname, 11);
	bcopy(toname, fdep->de_Name, 11);	/* update denode */
	fdep->de_LowerCase = new_deLowerCase;
	
	/*
	 * If the new name starts with ".", make it invisible on Windows.
	 * Otherwise, make it visible.
	 */
	if (tcnp->cn_nameptr[0] == '.')
		fdep->de_Attributes |= ATTR_HIDDEN;
	else
		fdep->de_Attributes &= ~ATTR_HIDDEN;

	error = createde(fdep, tddep, NULL, tcnp, to_diroffset, to_long_count, context);
	if (error)
	{
		bcopy(oldname, fdep->de_Name, 11);
		/*¥ What if we already deleted the target? */
		goto exit;
	}
	else
	{
		cache_purge_negatives(tdvp);
	}
	
	/* For directories, restore the name to "." */
	if (doingdirectory)
		bcopy(oldname, fdep->de_Name, 11);	/* Change it back to "." */
	error = removede(fddep, from_offset, context);
	if (error) {
		/* XXX should really panic here, fs is corrupt */
		goto exit;
	}
	if (!doingdirectory) {
		/*
		 * Fix fdep's de_dirclust and de_diroffset to reflect
		 * its new location in the destination directory.
		 */
		error = pcbmap(tddep, de_cluster(pmp, to_diroffset), 1,
					NULL, &fdep->de_dirclust, NULL, context);
		if (error) {
			/* XXX should really panic here, fs is corrupt */
			goto exit;
		}
		fdep->de_diroffset = to_diroffset;
	}
	reinsert(fdep);

	/*
	 * If we moved a directory to a new parent directory, then we must
	 * fixup the ".." entry in the moved directory.
	 */
	if (doingdirectory && newparent) {
		struct dosdirentry *dotdotp;

		/* Read in the first cluster of the directory and point to ".." entry */
		cn = fdep->de_StartCluster;
		bn = cntobn(pmp, cn);
		error = (int)buf_meta_bread(pmp->pm_devvp, bn, pmp->pm_bpcluster, vfs_context_ucred(context), &bp);
		if (error) {
			/* XXX should really panic here, fs is corrupt */
			buf_brelse(bp);
			goto exit;
		}
		dotdotp = (struct dosdirentry *)buf_dataptr(bp) + 1;

		/* Update the starting cluster of ".." */
		putushort(dotdotp->deStartCluster, tddep->de_StartCluster);
		if (FAT32(pmp))
			putushort(dotdotp->deHighClust, tddep->de_StartCluster >> 16);

		error = (int)buf_bdwrite(bp);
		if (error) {
			/* XXX should really panic here, fs is corrupt */
			goto exit;
		}
	}

exit:
	msdosfs_meta_flush(pmp);
	return (error);

}

static struct {
	struct dosdirentry dot;
	struct dosdirentry dotdot;
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
	struct vnop_mkdir_args /* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t dvp = ap->a_dvp;
	struct denode *pdep = VTODE(dvp);
	struct componentname *cnp = ap->a_cnp;
	vfs_context_t context = ap->a_context;
	struct denode *dep;
	struct dosdirentry *denp;
	struct msdosfsmount *pmp = pdep->de_pmp;
	struct buf *bp;
	u_long newcluster, pcl;
	daddr64_t bn;
	int error;
	struct denode ndirent;
	struct timespec ts;
	char *bdata;
	u_long offset;
	u_long long_count;

	/*
	 * Find space in the directory to place the new name.
	 */
	error = findslots(pdep, cnp, &ndirent.de_LowerCase, &offset, &long_count, context);
	if (error)
		goto exit;

	/*
	 * If this is the root directory and there is no space left we
	 * can't do anything.  This is because the root directory can not
	 * change size.
	 */
	if (pdep->de_StartCluster == MSDOSFSROOT
		&& offset >= pdep->de_FileSize)
	{
		error = ENOSPC;
		goto exit;
	}

	/*
	 * Allocate a cluster to hold the about to be created directory.
	 */
	error = clusteralloc(pmp, 0, 1, CLUST_EOFE, &newcluster, NULL, context);
	if (error)
		goto exit;

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
	bp = buf_getblk(pmp->pm_devvp, bn, pmp->pm_bpcluster, 0, 0, BLK_META);
	bdata = (char *)buf_dataptr(bp);

	bzero(bdata, pmp->pm_bpcluster);
	bcopy(&dosdirtemplate, bdata, sizeof dosdirtemplate);
	denp = (struct dosdirentry *)bdata;
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

	error = (int)buf_bdwrite(bp);
	if (error)
		goto exit;

	/*
	 * Now build up a directory entry pointing to the newly allocated
	 * cluster.  This will be written to an empty slot in the parent
	 * directory.
	 */
	error = uniqdosname(pdep, cnp, ndirent.de_Name, &ndirent.de_LowerCase, context);
	if (error)
		goto exit;

	ndirent.de_Attributes = ATTR_DIRECTORY;
	ndirent.de_StartCluster = newcluster;
	ndirent.de_FileSize = 0;
	ndirent.de_dev = pdep->de_dev;
	ndirent.de_devvp = pdep->de_devvp;

	/*
	 * If the file name starts with ".", make it invisible on Windows.
	 */
	if (cnp->cn_nameptr[0] == '.')
		ndirent.de_Attributes |= ATTR_HIDDEN;

	error = createde(&ndirent, pdep, &dep, cnp, offset, long_count, context);
	if (error)
		clusterfree(pmp, newcluster, NULL, context);
	else
	{
		*ap->a_vpp = DETOV(dep);
		cache_purge_negatives(dvp);
	}

exit:
	msdosfs_meta_flush(pmp);
	return error;
}

/*
 * Remove a directory.
 *
 * On entry, vp has been suspended, so there are no pending
 * create or lookup operations happening using it as the
 * parent directory.
 *
 * VFS has already checked that dvp != vp.
 * 
 */
static int
msdosfs_rmdir(ap)
	struct vnop_rmdir_args /* {
		vnode_t a_dvp;
		vnode_t a_vp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	vnode_t dvp = ap->a_dvp;
	vfs_context_t context = ap->a_context;
	struct denode *ip, *dp;
	uint32_t offset;
	int error;
	
	/* No need to lock vp since it has been suspended */
	
	ip = VTODE(vp);
	dp = VTODE(dvp);

	/*
	 * No rmdir "." please.
	 *
	 * VFS already checks this in rmdir(), so do we
	 * need to check again?  (It would only be useful if
	 * some other entity called our VNOP directly.)
	 */
	if (dp == ip) {
		return EINVAL;
	}

	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	error = 0;
	if (!dosdirempty(ip, context)) {
		error = ENOTEMPTY;
		goto exit;
	}

	/* Make sure the directory isn't read-only */
	/* Note that this is an additional imposition beyond the usual
	   deletability rules */
	if (ip->de_Attributes & ATTR_READONLY)
	{
		error = EPERM;
		goto exit;
	}
    
	/*
	 * Delete the entry from the directory.  For dos filesystems this
	 * gets rid of the directory entry on disk, the in memory copy
	 * still exists but the de_refcnt is <= 0.  This prevents it from
	 * being found by deget().  When the vput() on dep is done we give
	 * up access and eventually msdosfs_reclaim() will be called which
	 * will remove it from the denode cache.
	 */
	error = msdosfs_lookupdir(dp, ip, &offset, context);
	if (error) goto exit;
	ip->de_refcnt--;
	error = removede(dp, offset, context);
	if (error) goto exit;

	/*
	 * Invalidate the directory's contents.  If directory I/O went through
	 * the directory's vnode, this wouldn't be needed; the invalidation
	 * done in detrunc would be sufficient.
	 */
	error = msdosfs_dir_invalidate(ip, context);
	if (error) goto exit;

	/*
	 * Truncate the directory that is being deleted.
	 */
	error = detrunc(ip, (u_long)0, IO_SYNC, context);
	cache_purge(vp);

exit:
	msdosfs_meta_flush(dp->de_pmp);
	return (error);
}

static int
msdosfs_readdir(ap)
	struct vnop_readdir_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_flags;
		int *a_eofflag;
		int *a_numdirent;
		vfs_context_t a_context;
	} */ *ap;
{
	int error = 0;
	vnode_t vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct denode *dep = VTODE(vp);
	struct msdosfsmount *pmp = dep->de_pmp;
	vfs_context_t context = ap->a_context;
	int diff;
	long n;
	u_long blsize;
	long on;
	u_long cn;
	u_long fileno;
	long bias = 0;
	daddr64_t bn, lbn;
	struct buf *bp;
	struct dosdirentry *dentp;
	struct dirent dirbuf;
	off_t offset;			/* Current offset within directory */
	off_t long_name_offset;	/* Offset to start of long name */
	int chksum = -1;
	u_int16_t ucfn[WIN_MAXLEN + 1];
	u_int16_t unichars;
	size_t outbytes;
	char *bdata;

	if (ap->a_numdirent)
		*ap->a_numdirent = 0;

	/* Assume we won't hit end of directory */
	if (ap->a_eofflag)
		*ap->a_eofflag = 0;

	if (ap->a_flags & (VNODE_READDIR_EXTENDED | VNODE_READDIR_REQSEEKOFF))
		return (EINVAL);

	/*
	 * msdosfs_readdir() won't operate properly on regular files since
	 * it does i/o only with the device vnode, and hence can
	 * retrieve the wrong block from the buffer cache for a plain file.
	 * So, fail attempts to readdir() on a plain file.
	 */
	if (!vnode_isdir(vp))
		return ENOTDIR;

	/*
	 * To be safe, initialize dirbuf
	 */
	bzero(dirbuf.d_name, sizeof(dirbuf.d_name));

	/*
	 * If the file offset is not a multiple of the size of a
	 * directory entry, then we fail the read.  The remaining
	 * space in the buffer will be checked before each uiomove.
	 */
	long_name_offset = offset = uio_offset(uio);
	if (offset & (sizeof(struct dosdirentry) - 1))
		return EINVAL;

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
		bias = 2 * sizeof(struct dosdirentry);
		if (offset < bias) {
			for (n = (int)offset / sizeof(struct dosdirentry);
			     n < 2; n++) {
                dirbuf.d_fileno = defileid(dep);
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
				if (uio_resid(uio) < dirbuf.d_reclen)
					goto out;
				error = uiomove((caddr_t) &dirbuf,
						dirbuf.d_reclen, uio);
				if (error)
					goto out;
				if (ap->a_numdirent)
					++(*ap->a_numdirent);
				offset += sizeof(struct dosdirentry);
			}
		}
	}

	while (uio_resid(uio) > 0) {
		lbn = de_cluster(pmp, offset - bias);
		on = (offset - bias) & pmp->pm_crbomask;
		n = min(pmp->pm_bpcluster - on, uio_resid(uio));
		diff = dep->de_FileSize - (offset - bias);
		if (diff <= 0) {
			if (ap->a_eofflag)
				*ap->a_eofflag = 1;	/* Hit end of directory */
			break;
		}
		n = min(n, diff);
		error = pcbmap(dep, lbn, 1, &bn, &cn, &blsize, context);
		if (error)
			break;
		error = (int)buf_meta_bread(pmp->pm_devvp, bn, blsize, vfs_context_ucred(context), &bp);
		if (error) {
			buf_brelse(bp);
			return (error);
		}
		n = min(n, blsize - buf_resid(bp));

		bdata = (char *)buf_dataptr(bp);
		/*
		 * Convert from dos directory entries to fs-independent
		 * directory entries.
		 */
		for (dentp = (struct dosdirentry *)(bdata + on);
		     (char *)dentp < bdata + on + n;
		     dentp++, offset += sizeof(struct dosdirentry)) {
#ifdef DEBUG
			printf("rd: dentp %08x prev %08x crnt %08x deName %02x attr %02x\n",
			    dentp, prev, crnt, dentp->deName[0], dentp->deAttributes);
#endif
			/*
			 * If this is an unused entry, we can stop.
			 */
			if (dentp->deName[0] == SLOT_EMPTY) {
				buf_brelse(bp);
				if (ap->a_eofflag)
					*ap->a_eofflag = 1;	/* Hit end of directory */
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
				if (dentp->deName[0] & WIN_LAST)
					long_name_offset = offset;
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
			 * the computation in defileid().
             */
            fileno = getushort(dentp->deStartCluster);
            if (FAT32(pmp))
                fileno |= getushort(dentp->deHighClust) << 16;
			if (dentp->deAttributes & ATTR_DIRECTORY) {
            	if (fileno == MSDOSFSROOT) {
                    /* if this is the root directory */
                    if (FAT32(pmp))
                        fileno = pmp->pm_rootdirblk;
                    else
                        fileno = FILENO_ROOT;
                }
				dirbuf.d_type = DT_DIR;
			} else {
                if (fileno == 0)
                    fileno = FILENO_EMPTY;	/* constant for empty files */
				if (getulong(dentp->deFileSize) == sizeof(struct symlink))
					dirbuf.d_type = DT_UNKNOWN;	/* Might be a symlink */
				else
					dirbuf.d_type = DT_REG;
			}
            dirbuf.d_fileno = fileno;
			if (chksum != winChksum(dentp->deName)) {
				chksum = -1;
				unichars = dos2unicodefn(dentp->deName, ucfn,
				    dentp->deLowerCase);
			}
			
			/* Convert SFM Unicode to Mac Unicode */
			sfm2macfn(ucfn, unichars);

			/* translate the name in ucfn into UTF-8 */
			(void) utf8_encodestr(ucfn, unichars * 2,
					dirbuf.d_name, &outbytes,
					sizeof(dirbuf.d_name), 0, UTF_DECOMPOSED);
			dirbuf.d_namlen = outbytes;
			dirbuf.d_reclen = GENERIC_DIRSIZ(&dirbuf);

			if (uio_resid(uio) < dirbuf.d_reclen) {
				if (chksum != -1)
					offset = long_name_offset;	/* Resume with start of long name */
				buf_brelse(bp);
				goto out;
			}
			error = uiomove((caddr_t) &dirbuf,
					dirbuf.d_reclen, uio);
			if (error) {
				buf_brelse(bp);
				goto out;
			}
			chksum = -1;
			if (ap->a_numdirent)
				++(*ap->a_numdirent);
		}
		buf_brelse(bp);
	}

out:
	/* Update the current position within the directory */
	uio_setoffset(uio, offset);

	return error;
}

/* blktooff converts a logical block number to a file offset */
__private_extern__ int
msdosfs_blktooff(ap)
	struct vnop_blktooff_args /* {
		vnode_t a_vp;
		daddr64_t a_lblkno;
		off_t *a_offset;    
	} */ *ap;
{
	if (ap->a_vp == NULL)
		return EINVAL;

	*ap->a_offset = ap->a_lblkno * PAGE_SIZE_64;

	return 0;
}

/* offtoblk converts a file offset to a logical block number */
__private_extern__ int
msdosfs_offtoblk(ap)
	struct vnop_offtoblk_args /* {
		vnode_t a_vp;
		off_t a_offset;    
		daddr64_t *a_lblkno;
	} */ *ap;
{
	if (ap->a_vp == NULL)
		return EINVAL;
	
	*ap->a_lblkno = ap->a_offset / PAGE_SIZE_64;
	
	return 0;
}


__private_extern__ int
msdosfs_blockmap(ap)
	struct vnop_blockmap_args /* {
		vnode_t a_vp;
		off_t a_foffset;
		size_t a_size;
		daddr64_t *a_bpn;
		size_t *a_run;
		void *a_poff;
		int a_flags;
		vfs_context_t a_context;
	} */ *ap;
{
	int error;
	vnode_t vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
    struct msdosfsmount *pmp = dep->de_pmp;
	vfs_context_t context = ap->a_context;
	u_long runsize;
    u_long		cn;
    u_long		numclusters;
    daddr64_t	bn;

	if (ap->a_bpn == NULL)
		return (0);

    if (ap->a_size == 0)
        panic("msdosfs_blockmap: a_size == 0");

    /* Find the cluster that contains the given file offset */
    cn = de_cluster(pmp, ap->a_foffset);
    
    /* Determine number of clusters occupied by the given range */
    numclusters = de_cluster(pmp, ap->a_foffset + ap->a_size - 1) - cn + 1;
    
    /* Find the physical (device) block where that cluster starts */
    error = pcbmap(dep, cn, numclusters, &bn, NULL, &runsize, context);
#ifdef DEBUG
    printf("msdosfs_blockmap: off 0x%lx bn1 0x%lx",
           (u_long)ap->a_foffset, bn);
#endif

    /* Add the offset in physical (device) blocks from the start of the cluster */
    bn += (((u_long)ap->a_foffset - de_cn2off(pmp, cn)) >> pmp->pm_bnshift);
    runsize -= ((u_long)ap->a_foffset - (de_cn2off(pmp, cn)));
    
    *ap->a_bpn = bn;
	if (error == 0 && ap->a_run) {
		if (runsize > ap->a_size)
			* ap->a_run = ap->a_size;
		else
			* ap->a_run = runsize;
	}
	if (ap->a_poff)
		*(int *)ap->a_poff = 0;

#ifdef DEBUG
    printf(" bn 2 0x%lx run 0x%lx\n", bn, *ap->a_run);
#endif
 return (error);
}

/*
 * prepare and issue the I/O
 * buf_strategy knows how to deal
 * with requests that require 
 * fragmented I/Os
 */
static int
msdosfs_strategy(ap)
	struct vnop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	buf_t	bp = ap->a_bp;
	vnode_t	vp = buf_vnode(bp);
	struct denode *dep = VTODE(vp);
	
	return buf_strategy(dep->de_devvp, ap);
}

static int
msdosfs_pathconf(ap)
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
		*ap->a_retval = WIN_MAXLEN;
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


static int msdosfs_advlock(struct vnop_advlock_args /* {
	vnode_t a_vp;
	caddr_t a_id;
	int a_op;
	struct flock *a_fl;
	int a_flags;
	vfs_context_t a_context;
	} */ *ap)
{
	vnode_t vp = ap->a_vp;
	struct flock *fl = ap->a_fl;
	struct msdosfs_lockf *lock;
	struct denode *dep;
	off_t start, end;
	int error;
    
	/* Only regular files can have locks */
	if ( !vnode_isreg(vp))
		return EISDIR;

    dep = VTODE(vp);
    
	/*
	 * Avoid the common case of unlocking when inode has no locks.
	 */
	if (dep->de_lockf == (struct msdosfs_lockf *)0) {
		if (ap->a_op != F_SETLK) {
			fl->l_type = F_UNLCK;
			error = 0;
			goto exit;
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
		error = EINVAL;
		goto exit;
	}

	if (start < 0)
	{
		error = EINVAL;
		goto exit;
	}
	
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
		error = msdosfs_setlock(lock);
		break;
	case F_UNLCK:
		error = msdosfs_clearlock(lock);
		FREE(lock, M_LOCKF);
		break;
	case F_GETLK:
		error = msdosfs_getlock(lock, fl);
		FREE(lock, M_LOCKF);
		break;
	default:
		error = EINVAL;
		_FREE(lock, M_LOCKF);
            break;
	}

exit:
	return error;
}


/*
 * Given a chunk of memory, compute the md5 digest as a string of 32
 * hex digits followed by a NUL character.
 */
static void msdosfs_md5_digest(void *text, unsigned length, char digest[33])
{
	int i;
	MD5_CTX context;
	unsigned char digest_raw[16];

	MD5Init(&context);
	MD5Update(&context, text, length);
	MD5Final(digest_raw, &context);
	
	for (i=0; i<16; ++i)
	{
		(void) sprintf(digest, "%02x", digest_raw[i]);
		digest += 2;
	}
}


/*
 * Determine whether the given denode refers to a symlink or an ordinary
 * file.  This is called during vnode creation, so the de_vnode field
 * has not been set up.  Returns the vnode type to create (either
 * VREG or VLNK).
 *
 * In order to tell whether the file is an ordinary file or a symlink,
 * we need to read from it.  The easiest way to do this is to create a
 * temporary vnode of type VREG, so we can use the buffer cache.  We will
 * terminate the vnode before returning.  deget() will create the real
 * vnode to be returned.
 */
__private_extern__ enum vtype msdosfs_check_link(struct denode *dep, vfs_context_t context)
{
	int error;
	int i;
	unsigned length;
	char c;
	enum vtype result;
	struct msdosfsmount *pmp;
	vnode_t vp = NULL;
    buf_t bp = NULL;
	struct symlink *link;
	char digest[33];
	struct vnode_fsparam vfsp;

	if (dep->de_FileSize != sizeof(struct symlink))
		return VREG;
	
	/*
	 * The file is the magic symlink size.  We need to read it in so we
	 * can validate the header and update the size to reflect the
	 * length of the symlink.
	 */
	result = VREG;		/* Assume it's not a symlink, until we verify it. */
	pmp = dep->de_pmp;
	
	vfsp.vnfs_mp = pmp->pm_mountp;
	vfsp.vnfs_vtype = VREG;
	vfsp.vnfs_str = "msdosfs";
	vfsp.vnfs_dvp = NULL;
	vfsp.vnfs_fsnode = dep;
	vfsp.vnfs_cnp = NULL;
	vfsp.vnfs_vops = msdosfs_vnodeop_p;
	vfsp.vnfs_rdev = 0;
	vfsp.vnfs_filesize = dep->de_FileSize;
	vfsp.vnfs_flags = VNFS_NOCACHE;
	vfsp.vnfs_markroot = 0;
	vfsp.vnfs_marksystem = 0;
	
	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &dep->de_vnode);
	vp = dep->de_vnode;
	if (error) goto exit;

    error = buf_meta_bread(vp, 0, roundup(sizeof(*link),pmp->pm_bpcluster),
        vfs_context_ucred(context), &bp);
    if (error) goto exit;
    link = (struct symlink *) buf_dataptr(bp);

	/* Verify the magic */
	if (strncmp(link->magic, symlink_magic, 5) != 0)
		goto exit;

	/* Parse and sanity check the length field */
	length = 0;
	for (i=0; i<4; ++i)
	{
		c = link->length[i];
		if (c < '0' || c > '9')
			goto exit;		/* Length is non-decimal */
		length = 10 * length + c - '0';
	}
	if (length > SYMLINK_LINK_MAX)
		goto exit;			/* Length is too big */

	/* Verify the MD5 digest */
	msdosfs_md5_digest(link->link, length, digest);
	if (strncmp(digest, link->md5, 32) != 0)
		goto exit;

	/* It passed all the checks; must be a symlink */
	result = VLNK;
	dep->de_FileSize = length;

exit:
    if (bp)
    {
        /*
         * We're going to be getting rid of the vnode, so we might as well
         * mark the buffer invalid so that it will get reused right away.
         */
        buf_markinvalid(bp);
        buf_brelse(bp);
    }
    
	if (vp)
	{
		(void) vnode_clearfsnode(vp);	/* So we won't free dep */
		(void) vnode_put(vp);			/* to balance vnode_create */
		(void) vnode_recycle(vp);		/* get rid of the vnode now */
	}

	return result;
}


/*
 * Create a symbolic link.
 *
 * The idea is to write the symlink file content to disk and
 * create a new directory entry pointing at the symlink content.
 * Then createde will automatically create the correct type of
 * vnode.
 */
static int msdosfs_symlink(struct vnop_symlink_args /* {
	vnode_t a_dvp;
	vnode_t *a_vpp;
	struct componentname *a_cnp;
	struct vnode_vattr *a_vap;
	char *a_target;
	vfs_context_t a_context;
	} */ *ap)
{
	int error;
	vnode_t dvp = ap->a_dvp;
	struct denode *dep = VTODE(dvp);
    struct msdosfsmount *pmp = dep->de_pmp;
    struct componentname *cnp = ap->a_cnp;
	char *target = ap->a_target;
	vfs_context_t context = ap->a_context;
	unsigned length;		/* length of target path */
	struct symlink *link = NULL;
	u_long cn = 0;			/* first cluster of symlink */
	u_long clusters, got;	/* count of clusters needed, actually allocated */
	buf_t bp = NULL;
	struct denode ndirent;
	u_long va_flags;
	struct denode *new_dep;
	struct timespec ts;
	u_long offset;
	u_long long_count;

	/*
	 * Find space in the directory to place the new name.
	 */
	error = findslots(dep, cnp, &ndirent.de_LowerCase, &offset, &long_count, context);
	if (error)
		goto exit;

	/*
	 * If this is the root directory and there is no space left we
	 * can't do anything.  This is because the root directory can not
	 * change size.  (FAT12 and FAT16 only)
	 *
	 * On FAT32, we can grow the root directory, and de_StartCluster
	 * will be the actual cluster number of the root directory.
	 */
	if (dep->de_StartCluster == MSDOSFSROOT
	    && offset >= dep->de_FileSize)
	{
		error = ENOSPC;
		goto exit;
	}

	length = strlen(target);
	if (length > SYMLINK_LINK_MAX)
		return ENAMETOOLONG;

	/*
	 * Allocate (contiguous) space to store the symlink.  In an ideal world,
	 * we should support creating a non-contiguous symlink file, but that
	 * would be much more complicated (creating a temporary denode and vnode
	 * just so that we can allocate and write to the link, then removing that
	 * vnode and creating a real one with the correct vtype).
	 */
	clusters = de_clcount(pmp, sizeof(*link));
	error = clusteralloc(pmp, 0, clusters, CLUST_EOFE, &cn, &got, context);
	if (error) goto exit;
	if (got < clusters)
	{
		error = ENOSPC;
		goto exit;
	}

	/* Get a buffer to hold the symlink */
	error = buf_meta_bread(pmp->pm_devvp, cntobn(pmp, cn),
		roundup(sizeof(*link),pmp->pm_bpcluster),
        vfs_context_ucred(context), &bp);
	if (error) goto exit;
	buf_clear(bp);
	link = (struct symlink *) buf_dataptr(bp);

	/*
	 * Fill in each of the symlink fields.  We have to do this in the same
	 * order as the fields appear in the structure because some of the
	 * operations clobber one byte past the end of their field (with a
	 * NUL character that is a string terminator).
	 */
	bcopy(symlink_magic, link->magic, sizeof(symlink_magic));
	sprintf(link->length, "%04d\n", length);
	msdosfs_md5_digest(target, length, link->md5);
	link->newline2 = '\n';
	bcopy(target, link->link, length);

	/* Pad with newline if there is room */
	if (length < SYMLINK_LINK_MAX)
		link->link[length++] = '\n';

	/* Pad with spaces if there is room */
	if (length < SYMLINK_LINK_MAX)
		memset(&link->link[length], ' ', SYMLINK_LINK_MAX-length);

	/* Write out the symlink */
	error = buf_bwrite(bp);
	if (error) goto exit;
	buf_markinvalid(bp);
	bp = NULL;

	/* Start setting up new directory entry */
	bzero(&ndirent, sizeof(ndirent));
	error = uniqdosname(dep, cnp, ndirent.de_Name, &ndirent.de_LowerCase, context);
	if (error)
		goto exit;

	/*
	 * Set read-only attribute if one of the immutable bits is set.
	 * Always set the "needs archive" attribute on newly created files.
	 */
	va_flags = ap->a_vap->va_flags;
	if (va_flags != VNOVAL && (va_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) != 0)
		ndirent.de_Attributes = ATTR_ARCHIVE | ATTR_READONLY;
	else
		ndirent.de_Attributes = ATTR_ARCHIVE;
        
	ndirent.de_StartCluster = cn;
	ndirent.de_FileSize = sizeof(*link);
	ndirent.de_dev = dep->de_dev;
	ndirent.de_devvp = dep->de_devvp;
	ndirent.de_pmp = dep->de_pmp;
	ndirent.de_flag = DE_ACCESS | DE_CREATE | DE_UPDATE;
	getnanotime(&ts);
	DETIMES(&ndirent, &ts, &ts, &ts);
	
	/* Create a new directory entry pointing at the newly allocated clusters */
	error = createde(&ndirent, dep, &new_dep, cnp, offset, long_count, context);
	if (error == 0)
	{
		*ap->a_vpp = DETOV(new_dep);
		cache_purge_negatives(dvp);
	}

exit:
	if (bp)
	{
		/*
		 * After the symlink is created, we should access the contents via the
		 * symlink's vnode, not via the device.  Marking it invalid here
		 * prevents double caching (and the inconsistencies which can result).
		 */
		buf_markinvalid(bp);
		buf_brelse(bp);
	}
	if (error != 0 && cn != 0)
		(void) freeclusterchain(pmp, cn, context);

	msdosfs_meta_flush(pmp);
	return error;
}


static int msdosfs_readlink(struct vnop_readlink_args /* {
	vnode_t a_vp;
	struct uio *a_uio;
	vfs_context_t a_context;
	} */ *ap)
{
	int error;
	vnode_t vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
    struct msdosfsmount *pmp = dep->de_pmp;
    buf_t bp = NULL;
	struct symlink *link;
	
	if (vnode_vtype(vp) != VLNK)
		return EINVAL;

	/*
	 * If the vnode was created of type VLNK, then we assume the symlink
	 * file has been checked for consistency.  So we skip it here.
	 */
    error = buf_meta_bread(vp, 0, roundup(sizeof(*link),pmp->pm_bpcluster),
        vfs_context_ucred(ap->a_context), &bp);
    if (error) goto exit;
    link = (struct symlink *) buf_dataptr(bp);

	/*
	 * Return the link.  Assumes de_FileSize was set to the length
	 * of the symlink.
	 */
	error = uiomove(link->link, dep->de_FileSize, ap->a_uio);

exit:
    if (bp)
        buf_brelse(bp);

	return error;
}


/* Global vfs data structures for msdosfs */

typedef int     vnop_t __P((void *));

int (**msdosfs_vnodeop_p)(void *);
static struct vnodeopv_entry_desc msdosfs_vnodeop_entries[] = {
	{ &vnop_default_desc,		(vnop_t *) vn_default_error },
	{ &vnop_lookup_desc,		(vnop_t *) msdosfs_lookup },
	{ &vnop_create_desc,		(vnop_t *) msdosfs_create },
	{ &vnop_mknod_desc,			(vnop_t *) msdosfs_mknod },
	{ &vnop_open_desc,			(vnop_t *) msdosfs_open },
	{ &vnop_close_desc,			(vnop_t *) msdosfs_close },
	{ &vnop_getattr_desc,		(vnop_t *) msdosfs_getattr },
	{ &vnop_setattr_desc,		(vnop_t *) msdosfs_setattr },
	{ &vnop_read_desc,			(vnop_t *) msdosfs_read },
	{ &vnop_write_desc,			(vnop_t *) msdosfs_write },
	{ &vnop_fsync_desc,			(vnop_t *) msdosfs_fsync },
	{ &vnop_remove_desc,		(vnop_t *) msdosfs_remove },
	{ &vnop_rename_desc,		(vnop_t *) msdosfs_rename },
	{ &vnop_mkdir_desc,			(vnop_t *) msdosfs_mkdir },
	{ &vnop_rmdir_desc,			(vnop_t *) msdosfs_rmdir },
	{ &vnop_readdir_desc,		(vnop_t *) msdosfs_readdir },
	{ &vnop_inactive_desc,		(vnop_t *) msdosfs_inactive },
	{ &vnop_reclaim_desc,		(vnop_t *) msdosfs_reclaim },
	{ &vnop_pathconf_desc,		(vnop_t *) msdosfs_pathconf },
	{ &vnop_advlock_desc,		(vnop_t *) msdosfs_advlock },
	{ &vnop_pagein_desc,		(vnop_t *) msdosfs_pagein },
	{ &vnop_pageout_desc,		(vnop_t *) msdosfs_pageout },
	{ &vnop_blktooff_desc,		(vnop_t *) msdosfs_blktooff },
	{ &vnop_offtoblk_desc,		(vnop_t *) msdosfs_offtoblk },
  	{ &vnop_blockmap_desc,		(vnop_t *) msdosfs_blockmap },
	{ &vnop_strategy_desc,		(vnop_t *) msdosfs_strategy },
	{ &vnop_symlink_desc,		(vnop_t *) msdosfs_symlink },
	{ &vnop_readlink_desc,		(vnop_t *) msdosfs_readlink },
	{ NULL, NULL }
};

struct vnodeopv_desc msdosfs_vnodeop_opv_desc =
	{ &msdosfs_vnodeop_p, msdosfs_vnodeop_entries };
