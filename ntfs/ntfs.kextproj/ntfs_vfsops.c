/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2004 Apple Computer, Inc.  All Rights Reserved.
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
/*	$NetBSD: ntfs_vfsops.c,v 1.23 1999/11/15 19:38:14 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/ntfs/ntfs_vfsops.c,v 1.44 2002/08/04 10:29:29 jeff Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/ubc.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <libkern/OSMalloc.h>
#include <miscfs/specfs/specdev.h>
#include <string.h>
#include <mach/memory_object_types.h>
#include <mach/machine/vm_param.h>

/*
 * The following used to be defined in <mach/memory_object_types.h>,
 * but was recently changed to be defined only for xnu itself.
 * We define it here as a temporary work-around.
 */
#ifndef MAX_UPL_TRANSFER
#define MAX_UPL_TRANSFER 256
#endif

/*#define NTFS_DEBUG 1*/
#include "ntfs.h"
#include "ntfs_inode.h"
#include "ntfs_subr.h"
#include "ntfs_vfsops.h"
#include "ntfs_ihash.h"
#include "ntfsmount.h"

MALLOC_DEFINE(M_NTFSMNT, "NTFS mount", "NTFS mount structure");
MALLOC_DEFINE(M_NTFSDIR,"NTFS dir",  "NTFS dir buffer");

extern int (**ntfs_vnodeop_p)(void *);		/* vnode dispatch table */

static int	ntfs_root(mount_t mp, vnode_t *vpp, vfs_context_t context);
static int	ntfs_statfs(mount_t mp, struct vfsstatfs *sbp, vfs_context_t context);
static int	ntfs_unmount(mount_t mp, int mntflags, vfs_context_t context);
static int	ntfs_vget(mount_t mp, ino64_t  ino,
			       vnode_t *vpp, vfs_context_t context);
static int	ntfs_mountfs(register vnode_t, mount_t mp, 
				  struct ntfs_args *, vfs_context_t context);
static int	ntfs_vptofh(vnode_t, int *, unsigned char *, vfs_context_t context);
static int	ntfs_fhtovp(mount_t mp, int, unsigned char *fhp, vnode_t *vpp, vfs_context_t context);
static int	ntfs_mount(mount_t mp, vnode_t devvp, user_addr_t data, vfs_context_t context);
static int	ntfs_init(struct vfsconf *);

static int ntfs_calccfree(struct ntfsmount *, proc_t, cn_t *);

OSMallocTag ntfs_malloc_tag;

static int
ntfs_init (
	struct vfsconf *vcp )
{
	ntfs_nthashinit();
	ntfs_toupper_init();
	
	ntfs_lck_grp_attr = lck_grp_attr_alloc_init();
	ntfs_lck_grp = lck_grp_alloc_init("ntfs ntnode", ntfs_lck_grp_attr);

	return 0;
}

static int
ntfs_mount ( 
	mount_t mp,
	vnode_t devvp,
	user_addr_t data,
	vfs_context_t context )
{
	int		err = 0;
	struct ntfs_args args;

	/* copy in user arguments*/
	err = copyin(data, &args, sizeof (struct ntfs_args));
	if (err)
		goto exit;		/* can't get arguments*/

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (vfs_isupdate(mp)) {
		printf("ntfs_mount(): MNT_UPDATE not supported\n");
		err = EINVAL;
		goto exit;
	}

	if (!vfs_isupdate(mp)) {
		vfs_setflags(mp, MNT_RDONLY);	/* Force read-only for Darwin */
		err = ntfs_mountfs(devvp, mp, &args, context);
	}
	if (err)
		goto exit;

	vfs_setflags(mp, MNT_IGNORE_OWNERSHIP);

	/*
	 * Initialize FS stat information in mount struct
	 *
	 * This code is common to root and non-root mounts
	 */
	(void) ntfs_statfs(mp, vfs_statfs(mp), context);

exit:
	return(err);
}



/*
 * Copy the fields of a struct bootfile from on-disk (little endian)
 * to memory in native endianness.
 */
static void bootfile_to_host(struct bootfile *src, struct bootfile *dest)
{
    bcopy(src, dest, sizeof(struct bootfile));	/* Copy the unordered or byte sized fields */
    dest->bf_bps = le16toh(src->bf_bps);
    dest->bf_spt = le16toh(src->bf_spt);
    dest->bf_heads = le16toh(src->bf_heads);
    dest->bf_spv = le64toh(src->bf_spv);
    dest->bf_mftcn = le64toh(src->bf_mftcn);
    dest->bf_mftmirrcn = le64toh(src->bf_mftmirrcn);
    dest->bf_ibsz = le32toh(src->bf_ibsz);
    dest->bf_volsn = le32toh(src->bf_volsn);
}



/*
 * Common code for mount and mountroot
 */
int
ntfs_mountfs(devvp, mp, argsp, context)
	vnode_t devvp;
	mount_t mp;
	struct ntfs_args *argsp;
	vfs_context_t context;
{
	struct buf *bp;
	struct ntfsmount *ntmp=NULL;
	dev_t dev = vnode_specrdev(devvp);
	int error, ronly, i;
	vnode_t vp;
	proc_t p = vfs_context_proc(context);
	struct vfsstatfs *sbp;

	error = buf_invalidateblks(devvp, BUF_WRITE_DATA, 0, 0);
	if (error)
		return (error);

	ronly = vfs_isrdonly(mp);

	bp = NULL;

	error = (int)buf_meta_bread(devvp, BBLOCK, BBSIZE, vfs_context_ucred(context), &bp);
	if (error)
		goto out;
	ntmp = OSMalloc(sizeof(struct ntfsmount), ntfs_malloc_tag);
	bzero(ntmp, sizeof *ntmp);
	bootfile_to_host((struct bootfile *) buf_dataptr(bp), &ntmp->ntm_bootfile);
	buf_markaged(bp);
	buf_brelse(bp);
	bp = NULL;

	if (strncmp(ntmp->ntm_bootfile.bf_sysid, NTFS_BBID, NTFS_BBIDLEN)) {
		error = EINVAL;
		dprintf(("ntfs_mountfs: invalid boot block\n"));
		goto out;
	}

	{
		/* Calculate number of sectors per MFT record */
		int8_t cpr = ntmp->ntm_mftrecsz;
		if( cpr > 0 )
			ntmp->ntm_bpmftrec = ntmp->ntm_spc * cpr;
		else
			ntmp->ntm_bpmftrec = (1 << (-cpr)) / ntmp->ntm_bps;
	}
	dprintf(("ntfs_mountfs(): bps: %d, spc: %d, media: %x, mftrecsz: %d (%d sects)\n",
		ntmp->ntm_bps,ntmp->ntm_spc,ntmp->ntm_bootfile.bf_media,
		ntmp->ntm_mftrecsz,ntmp->ntm_bpmftrec));
	dprintf(("ntfs_mountfs(): mftcn: 0x%x|0x%x\n",
		(u_int32_t)ntmp->ntm_mftcn,(u_int32_t)ntmp->ntm_mftmirrcn));

	ntmp->ntm_mountp = mp;
	ntmp->ntm_dev = dev;
	ntmp->ntm_devvp = devvp;
	ntmp->ntm_uid = argsp->uid;
	ntmp->ntm_gid = argsp->gid;
	ntmp->ntm_mode = argsp->mode;
	ntmp->ntm_flag = argsp->flag;

	vfs_setfsprivate(mp, ntmp);

	dprintf(("ntfs_mountfs(): case-%s,%s uid: %d, gid: %d, mode: %o\n",
		(ntmp->ntm_flag & NTFS_MFLAG_CASE_SENSITIVE)?"sens.":"insens.",
		(ntmp->ntm_flag & NTFS_MFLAG_ALLNAMES)?" allnames,":"",
		ntmp->ntm_uid, ntmp->ntm_gid, ntmp->ntm_mode));


	/*
	 * We read in some system nodes to prevent reclaiming
	 * them and to have convenient access to them.
	 */
	{
		ino_t pi[3] = { NTFS_MFTINO, NTFS_ROOTINO, NTFS_BITMAPINO };
		for (i=0; i<3; i++) {
			error = ntfs_vgetex(mp, pi[i], NULLVP, NULL, VNON, NTFS_A_DATA, NULL, 0, p, &ntmp->ntm_sysvn[pi[i]]);
			if(error)
				goto out1;
			vnode_ref(ntmp->ntm_sysvn[pi[i]]);
			vnode_put(ntmp->ntm_sysvn[pi[i]]);
		}
	}

	/* read the Unicode lowercase --> uppercase translation table,
	 * if necessary */
	if ((error = ntfs_toupper_use(mp, ntmp, p)))
		goto out1;

	/*
	 * Scan $BitMap and count free clusters
	 */
	error = ntfs_calccfree(ntmp, p, &ntmp->ntm_cfree);
	if(error)
		goto out1;

	/*
	 * Read and translate to internal format attribute
	 * definition file. 
	 */
	{
		int num,j;
		struct attrdef ad;
                
		/* Open $AttrDef */
		error = ntfs_vgetex(mp, NTFS_ATTRDEFINO, NULLVP, NULL, VNON, NTFS_A_DATA, NULL, 0, p, &vp);
		if(error) 
			goto out1;

		/* Count valid entries */
		for(num=0;;num++) {
			error = ntfs_readattr(ntmp, VTONT(vp),
					NTFS_A_DATA, NULL,
					num * sizeof(ad), sizeof(ad),
					&ad, NULL, p);
			if (error)
				goto out1;
			if (ad.ad_name[0] == 0)
				break;
		}

		/* Alloc memory for attribute definitions */
		MALLOC(ntmp->ntm_ad, struct ntvattrdef *,
			num * sizeof(struct ntvattrdef),
			M_NTFSMNT, M_WAITOK);

		ntmp->ntm_adnum = num;

		/* Read them and translate */
		for(i=0;i<num;i++){
			error = ntfs_readattr(ntmp, VTONT(vp),
					NTFS_A_DATA, NULL,
					i * sizeof(ad), sizeof(ad),
					&ad, NULL, p);
			if (error)
				goto out1;
			j = 0;
			do {
				ntmp->ntm_ad[i].ad_name[j] = le16toh(ad.ad_name[j]);
			} while(ad.ad_name[j++]);
			ntmp->ntm_ad[i].ad_namelen = j - 1;
			ntmp->ntm_ad[i].ad_type = le32toh(ad.ad_type);
		}

		vnode_put(vp);
	}

	sbp = vfs_statfs(mp);
	sbp->f_bsize = ntmp->ntm_spc * ntmp->ntm_bps;
	sbp->f_iosize = MAX_UPL_TRANSFER * PAGE_SIZE_64;
	sbp->f_blocks = ntmp->ntm_bootfile.bf_spv / ntmp->ntm_spc;
	sbp->f_fsid.val[0] = (long) dev;
	sbp->f_fsid.val[1] = vfs_typenum(mp);
	vfs_setmaxsymlen(mp, 0);
	vfs_setflags(mp, MNT_LOCAL);
	return (0);

out1:
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		if(ntmp->ntm_sysvn[i])
			vnode_rele(ntmp->ntm_sysvn[i]);

out:
	if (bp)
		buf_brelse(bp);

	if (ntmp) {
            if (ntmp->ntm_ad)
                FREE(ntmp->ntm_ad, M_NTFSMNT);
	    OSFree(ntmp, sizeof(struct ntfsmount), ntfs_malloc_tag);
            vfs_setfsprivate(mp, NULL);
        }
	return (error);
}

static int
ntfs_unmount( 
	mount_t mp,
	int mntflags,
	vfs_context_t context)
{
	register struct ntfsmount *ntmp;
	int error, flags, i;

	dprintf(("ntfs_unmount: unmounting...\n"));
	ntmp = VFSTONTFS(mp);

	flags = 0;
	if(mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	dprintf(("ntfs_unmount: vflushing...\n"));
	error = vflush(mp, ntmp->ntm_sysvn[NTFS_ROOTINO], flags | SKIPSYSTEM);
	if (error) {
		printf("ntfs_unmount: vflush failed: %d\n",error);
		return (error);
	}

	/* Check if any system vnodes are busy */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if((ntmp->ntm_sysvn[i]) && vnode_isinuse(ntmp->ntm_sysvn[i], 1))
		 	return (EBUSY);

	/* Release all system vnodes */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if(ntmp->ntm_sysvn[i]) vnode_rele(ntmp->ntm_sysvn[i]);

	/* vflush system vnodes */
	error = vflush(mp, NULLVP, flags);
	if (error)
		printf("ntfs_unmount: vflush failed(sysnodes): %d\n",error);

	error = buf_invalidateblks(ntmp->ntm_devvp, BUF_WRITE_DATA, 0, 0);

	/* free the toupper table, if this has been last mounted ntfs volume */
	ntfs_toupper_unuse();

	dprintf(("ntfs_umount: freeing memory...\n"));
	FREE(ntmp->ntm_ad, M_NTFSMNT);
	OSFree(ntmp, sizeof(struct ntfsmount), ntfs_malloc_tag);
	vfs_setfsprivate(mp, NULL);
	return (error);
}

static int
ntfs_root(
	mount_t mp,
	vnode_t *vpp,
	vfs_context_t context)
{
	vnode_t nvp;
	int error = 0;
        
	dprintf(("ntfs_root(): sysvn: %p\n",
		VFSTONTFS(mp)->ntm_sysvn[NTFS_ROOTINO]));
	error = ntfs_vget(mp, NTFS_ROOTINO, &nvp, context);
	if (error) {
		printf("ntfs_root: VFS_VGET failed: %d\n",error);
		return (error);
	}

	*vpp = nvp;
	return (0);
}

static int
ntfs_calccfree(
	struct ntfsmount *ntmp,
	proc_t p,
	cn_t *cfreep)
{
	vnode_t vp;
	u_int8_t *tmp;
	int j, error=0;
	long cfree = 0;
	size_t bmsize, i, bmbase, readsize;

	vp = ntmp->ntm_sysvn[NTFS_BITMAPINO];

	bmsize = VTOF(vp)->f_size;

	tmp = OSMalloc(MAXBSIZE, ntfs_malloc_tag);

	for (bmbase = 0; bmbase < bmsize; bmbase += MAXBSIZE) {
		readsize = MIN(bmsize - bmbase, MAXBSIZE);

		error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
					   bmbase, readsize, tmp, NULL, p);
		if (error)
			goto out;
	
		for(i=0;i<readsize;i++)
			for(j=0;j<8;j++)
				if(~tmp[i] & (1 << j)) cfree++;
	}

	*cfreep = cfree;

out:
	OSFree(tmp, MAXBSIZE, ntfs_malloc_tag);
	return(error);
}

static int
ntfs_statfs(
	mount_t mp,
	struct vfsstatfs *sbp,
	vfs_context_t context)
{
	struct ntfsmount *ntmp = VFSTONTFS(mp);
	u_int64_t mftallocated;
	u_int32_t bytes_per_mft;
	
	dprintf(("ntfs_statfs():\n"));

	bytes_per_mft = ntfs_bntob(ntmp->ntm_bpmftrec);
	mftallocated = VTOF(ntmp->ntm_sysvn[NTFS_MFTINO])->f_allocated;

	sbp->f_bfree = sbp->f_bavail = ntmp->ntm_cfree;
	sbp->f_bused = sbp->f_blocks - sbp->f_bfree;
	sbp->f_ffree = ntfs_cntob(sbp->f_bfree) / bytes_per_mft;
	sbp->f_files = mftallocated / bytes_per_mft + sbp->f_ffree;
	vfs_name(mp, sbp->f_fstypename);
	
	return (0);
}

static int
ntfs_vfs_getattr(
	struct mount *mp,
	struct vfs_attr *attr,
	vfs_context_t context)
{
	struct vfsstatfs *stats = vfs_statfs(mp);

	/* We don't return object counts */
	
	VFSATTR_RETURN(attr, f_bsize, stats->f_bsize);
	VFSATTR_RETURN(attr, f_iosize, stats->f_iosize);
	VFSATTR_RETURN(attr, f_blocks, stats->f_blocks);
	VFSATTR_RETURN(attr, f_bfree, stats->f_bfree);
	VFSATTR_RETURN(attr, f_bavail, stats->f_bavail);
	VFSATTR_RETURN(attr, f_bused, stats->f_bused);
	VFSATTR_RETURN(attr, f_files, stats->f_files);
	VFSATTR_RETURN(attr, f_ffree, stats->f_ffree);
	VFSATTR_RETURN(attr, f_fsid, stats->f_fsid);
	VFSATTR_RETURN(attr, f_owner, stats->f_owner);
	if (VFSATTR_IS_ACTIVE(attr, f_capabilities))
	{
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_HARDLINKS |
			VOL_CAP_FMT_SPARSE_FILES |
			VOL_CAP_FMT_CASE_PRESERVING |
			VOL_CAP_FMT_FAST_STATFS ;
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] = 0;
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		attr->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
	
		attr->f_capabilities.valid[VOL_CAPABILITIES_FORMAT] =
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
		attr->f_capabilities.valid[VOL_CAPABILITIES_INTERFACES] =
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
		attr->f_capabilities.valid[VOL_CAPABILITIES_RESERVED1] = 0;
		attr->f_capabilities.valid[VOL_CAPABILITIES_RESERVED2] = 0;

		VFSATTR_SET_SUPPORTED(attr, f_capabilities);
	}
	
	/* Should we return f_attributes? */
	
	/* We don't return volume times */
	
	/* f_fstypename, f_mntonname, f_mntfromname are set by VFS */

	/* Should we return f_volname? */
	
	return 0;
}


/*ARGSUSED*/
static int
ntfs_fhtovp(
	mount_t mp,
	int fhlen,
	unsigned char *fhp,
	vnode_t *vpp,
	vfs_context_t context)
{
	struct ntfid *ntfhp = (struct ntfid *)fhp;
	int error;

	if (fhlen < sizeof(struct ntfid))
		return (EINVAL);

	ddprintf(("ntfs_fhtovp(): %d\n", ntfhp->ntfid_ino));

	error = ntfs_vgetex(mp, ntfhp->ntfid_ino, NULLVP, NULL, VNON, NTFS_A_DATA, NULL, 0, vfs_context_proc(context), vpp);
	if (error != 0)
		*vpp = NULLVP;

	/* XXX as unlink/rmdir/mkdir/creat are not currently possible
	 * with NTFS, we don't need to check anything else for now
	 * (like generation numbers or GUIDs -- to verify the inode
	 * number still belongs to the same object)*/

	return (error);
}

static int
ntfs_vptofh(
	vnode_t vp,
	int *fhlenp,
	unsigned char *fhp,
	vfs_context_t context)
{
	register struct ntnode *ntp;
	register struct ntfid *ntfhp;

	ddprintf(("ntfs_vptofh(): %p\n", vp));

	if (*fhlenp < sizeof(struct ntfid))
		return (EOVERFLOW);
	ntp = VTONT(vp);
	ntfhp = (struct ntfid *)fhp;
	ntfhp->ntfid_len = sizeof(struct ntfid);
	ntfhp->ntfid_pad = 0;
	ntfhp->ntfid_ino = ntp->i_number;
	/* ntfhp->ntfid_gen = ntp->i_gen; */
	ntfhp->ntfid_gen = 0;
	*fhlenp = sizeof(struct ntfid);
	return (0);
}

__private_extern__
int
ntfs_vgetex(
	mount_t mp,
	ino_t ino,
	vnode_t dvp,
	struct componentname *cnp,
	enum vtype vtype,
	u_int32_t attrtype,
	char *attrname,
	u_long flags,
	proc_t p,
	vnode_t *vpp) 
{
	int error;
	struct ntfsmount *ntmp;
	struct ntnode *ip;
	struct fnode *fp;
	vnode_t	vp;
	uint32_t vid;
	struct vnode_fsparam vfsp;

	dprintf(("ntfs_vgetex: ino: %d, attr: 0x%x:%s, f: 0x%lx\n",
		ino, attrtype, attrname?attrname:"", (u_long)flags ));

	ntmp = VFSTONTFS(mp);
	*vpp = NULL;

again:
	/* Get ntnode */
	error = ntfs_ntlookup(ntmp, ino, &ip);
	if (error) {
		printf("ntfs_vget: ntfs_ntget failed\n");
		return (error);
	}

	/* It may be not initialized fully, so force load it */
	if (!(flags & VG_DONTLOADIN) && !(ip->i_flag & IN_LOADED)) {
		error = ntfs_loadntnode(ntmp, ip, p);
		if(error) {
			printf("ntfs_vget: CAN'T LOAD ATTRIBUTES FOR INO: %d\n",
			       ip->i_number);
			ntfs_ntput(ip);
			return (error);
		}
	}

	/* Find or create an fnode for desired attribute */
	error = ntfs_fget(ntmp, ip, attrtype, attrname, &fp);
	if (error) {
		printf("ntfs_vget: ntfs_fget failed\n");
		ntfs_ntput(ip);
		return (error);
	}

	/* Make sure the fnode's sizes are initialized */
	if (!(flags & VG_DONTVALIDFN) && !(fp->f_flag & FN_VALID)) {
		if ((ip->i_frflag & NTFS_FRFLAG_DIR) &&
		    (fp->f_attrtype == NTFS_A_DATA && fp->f_attrname == NULL)) {
			vtype = VDIR;
		} else if (flags & VG_EXT) {
			vtype = VNON;
			fp->f_size = fp->f_allocated = 0;
		} else {
			vtype = VREG;	

			error = ntfs_filesize(ntmp, fp, p,
					      &fp->f_size, &fp->f_allocated);
			if (error) {
				ntfs_ntput(ip);
				return (error);
			}
		}

		fp->f_flag |= FN_VALID;
	}
	/* else, caller must have passed vtype */
	
	/* See if we already have a vnode for the file */
	vp = FTOV(fp);
	if (vp) {
		vid = vnode_vid(vp);
		error = vnode_getwithvid(vp, vid);
		ntfs_ntput(ip);
		if (error)
			goto again;
		*vpp = vp;
		return 0;
	}

	/* Must create a new vnode */
	vfsp.vnfs_mp = mp;
	vfsp.vnfs_vtype = vtype;
	vfsp.vnfs_str = "ntfs";
	vfsp.vnfs_dvp = dvp;	/* Parent vnode, if we know it */
	vfsp.vnfs_fsnode = fp;
	vfsp.vnfs_vops = ntfs_vnodeop_p;
	vfsp.vnfs_markroot = (ino == NTFS_ROOTINO);
	vfsp.vnfs_marksystem = ((ino < 12) && (ino != NTFS_ROOTINO));
	vfsp.vnfs_rdev = 0;		/* Set only for block or char devices */
	vfsp.vnfs_filesize = fp->f_size;	/*¥ Assumes the fnode is valid! */
	vfsp.vnfs_cnp = cnp;	/* Component name, if we know it */
	if (dvp && cnp && (cnp->cn_flags & MAKEENTRY))
		vfsp.vnfs_flags = 0;
	else
		vfsp.vnfs_flags = VNFS_NOCACHE;

	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &vp);
	if(error) {
		ntfs_frele(fp);
		ntfs_ntput(ip);
		return (error);
	}
	dprintf(("ntfs_vget: vnode: %p for ntnode: %d\n", vp,ino));

	fp->f_vp = vp;
	vnode_addfsref(vp);
	ntfs_ntput(ip);

	*vpp = vp;
	return (0);
}

static int
ntfs_vget(
	mount_t mp,
	ino64_t ino,
	vnode_t *vpp,
	vfs_context_t context) 
{
	return ntfs_vgetex(mp, (ino_t)ino, NULLVP, NULL, VNON, NTFS_A_DATA, NULL,
	    0, vfs_context_proc(context), vpp);
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
static int
ntfs_start(mp, flags, context)
	mount_t mp;
	int flags;
	vfs_context_t context;
{
	return 0;
}


static int	
ntfs_sync (mp, waitfor, context)
	mount_t mp;
	int waitfor;
	vfs_context_t context;
{
	return 0;
}


static struct vfsops ntfs_vfsops = {
	ntfs_mount,
	ntfs_start,
	ntfs_unmount,
	ntfs_root,
	NULL, /* ntfs_quotactl */
	ntfs_vfs_getattr,
	ntfs_sync,
	ntfs_vget,
	ntfs_fhtovp,
	ntfs_vptofh,
	ntfs_init,
	NULL /* ntfs_sysctl */
};


extern struct vnodeopv_desc ntfs_vnodeop_opv_desc;
static struct vnodeopv_desc *ntfs_vnodeop_opv_desc_list[1] =
{
	&ntfs_vnodeop_opv_desc
};


static vfstable_t ntfs_vfsconf;

__private_extern__
int
ntfs_kext_start(struct kmod_info_t *ki, void *data)
{
#pragma unused(data)
	errno_t error;
	struct vfs_fsentry vfe;

	ntfs_malloc_tag = OSMalloc_Tagalloc("ntfs", OSMT_DEFAULT);

	vfe.vfe_vfsops = &ntfs_vfsops;
	vfe.vfe_vopcnt = 1;		/* We just have vnode operations for regular files and directories */
	vfe.vfe_opvdescs = ntfs_vnodeop_opv_desc_list;
	strcpy(vfe.vfe_fsname, "ntfs");
	vfe.vfe_flags =  VFS_TBLNOTYPENUM | VFS_TBLLOCALVOL | VFS_TBL64BITREADY;
	vfe.vfe_reserv[0] = 0;
	vfe.vfe_reserv[1] = 0;
	
	error = vfs_fsadd(&vfe, &ntfs_vfsconf);
	if (error)
		OSMalloc_Tagfree(ntfs_malloc_tag);

	return error ? KERN_FAILURE : KERN_SUCCESS;
}
     

__private_extern__
int  
ntfs_kext_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki)
#pragma unused(data)
	errno_t error;

	error = vfs_fsremove(ntfs_vfsconf);
	if (error == 0)
		OSMalloc_Tagfree(ntfs_malloc_tag);

	return error ? KERN_FAILURE : KERN_SUCCESS;
}   
