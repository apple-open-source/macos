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
#ifdef APPLE
#include <sys/ubc.h>
#else
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#ifdef APPLE
#include <miscfs/specfs/specdev.h>
#include <string.h>
#else
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#endif

/*#define NTFS_DEBUG 1*/
#ifdef APPLE
#include "ntfs.h"
#include "ntfs_inode.h"
#include "ntfs_subr.h"
#include "ntfs_vfsops.h"
#include "ntfs_ihash.h"
#include "ntfsmount.h"
#else
#include <fs/ntfs/ntfs.h>
#include <fs/ntfs/ntfs_inode.h>
#include <fs/ntfs/ntfs_subr.h>
#include <fs/ntfs/ntfs_vfsops.h>
#include <fs/ntfs/ntfs_ihash.h>
#include <fs/ntfs/ntfsmount.h>
#endif

MALLOC_DEFINE(M_NTFSMNT, "NTFS mount", "NTFS mount structure");
MALLOC_DEFINE(M_NTFSNTNODE,"NTFS ntnode",  "NTFS ntnode information");
MALLOC_DEFINE(M_NTFSFNODE,"NTFS fnode",  "NTFS fnode information");
MALLOC_DEFINE(M_NTFSDIR,"NTFS dir",  "NTFS dir buffer");

struct sockaddr;

static int	ntfs_root(struct mount *, struct vnode **);
#ifdef APPLE
static int	ntfs_statfs(struct mount *, struct statfs *, struct proc *);
static int	ntfs_unmount(struct mount *, int, struct proc *);
static int	ntfs_vget(struct mount *mp, void *ino,
			       struct vnode **vpp);
static int	ntfs_mountfs(register struct vnode *, struct mount *, 
				  struct ntfs_args *, struct proc *);
static int	ntfs_vptofh(struct vnode *, struct fid *);
static int	ntfs_fhtovp(struct mount *, struct fid *, struct mbuf *, struct vnode **, int *, struct ucred **);
static int	ntfs_mount(struct mount *, char *, caddr_t,
				struct nameidata *, struct proc *);
#else
static int	ntfs_statfs(struct mount *, struct statfs *, struct thread *);
static int	ntfs_unmount(struct mount *, int, struct thread *);
static int	ntfs_vget(struct mount *mp, ino_t ino, int lkflags,
			       struct vnode **vpp);
static int	ntfs_mountfs(register struct vnode *, struct mount *, 
				  struct ntfs_args *, struct thread *);
static int	ntfs_vptofh(struct vnode *, struct fid *);
static int	ntfs_fhtovp(struct mount *, struct fid *, struct vnode **);
static int	ntfs_mount(struct mount *, char *, caddr_t,
				struct nameidata *, struct thread *);
#endif
static int	ntfs_init(struct vfsconf *);

static int
ntfs_init (
	struct vfsconf *vcp )
{
	ntfs_nthashinit();
	ntfs_toupper_init();
	return 0;
}

#ifndef APPLE
static int
ntfs_uninit (
	struct vfsconf *vcp )
{
	ntfs_toupper_destroy();
	ntfs_nthashdestroy();
	return 0;
}
#endif

static int
ntfs_mount ( 
	struct mount *mp,
	char *path,
	caddr_t data,
	struct nameidata *ndp,
#ifdef APPLE
	struct proc *td )
#else
	struct thread *td )
#endif
{
	size_t		size;
	int		err = 0;
	struct vnode	*devvp;
	struct ntfs_args args;

	/*
	 * Use NULL path to flag a root mount
	 */
	if( path == NULL) {
#ifdef APPLE
		/*
		 * I don't know if it make sense for FreeBSD to mount NTFS
		 * as a root filesystem.  It doesn't make sense for Darwin,
		 * so don't even try.
		 */
		 
		 err = EINVAL;
		 goto error_1;
#else
		/*
		 ***
		 * Mounting root filesystem
		 ***
		 */
	
		/* Get vnode for root device*/
		if( bdevvp( rootdev, &rootvp))
			panic("ntfs_mountroot: can't setup bdevvp for root");

		/*
		 * FS specific handling
		 */
		mp->mnt_flag |= MNT_RDONLY;	/* XXX globally applicable?*/

		/*
		 * Attempt mount
		 */
		if( ( err = ntfs_mountfs(rootvp, mp, &args, td)) != 0) {
			/* fs specific cleanup (if any)*/
			goto error_1;
		}

		goto dostatfs;		/* success*/
#endif
	}

	/*
	 ***
	 * Mounting non-root filesystem or updating a filesystem
	 ***
	 */

	/* copy in user arguments*/
	err = copyin(data, (caddr_t)&args, sizeof (struct ntfs_args));
	if (err)
		goto error_1;		/* can't get arguments*/

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		/* if not updating name...*/
		if (args.fspec == 0) {
			/*
			 * Process export requests.  Jumping to "success"
			 * will return the vfs_export() error code.
			 */
#ifdef APPLE
			struct ntfsmount *pmp = VFSTONTFS(mp);
			err = vfs_export(mp, &pmp->ntm_export, &args.export);
#else
			err = vfs_export(mp, &args.export);
#endif
			goto success;
		}

		printf("ntfs_mount(): MNT_UPDATE not supported\n");
		err = EINVAL;
		goto error_1;
	}

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, td);
	err = namei(ndp);
	if (err) {
		/* can't get devvp!*/
		goto error_1;
	}
#ifndef APPLE
	/*ее Other APPLE filesystems don't call NDFREE here.  Why not? */
	NDFREE(ndp, NDF_ONLY_PNBUF);
#endif
	devvp = ndp->ni_vp;

#ifdef APPLE
	if (devvp->v_type != VBLK) {
                err = ENOTBLK;
		goto error_2;
	}
	if (major(devvp->v_rdev) >= nblkdev) {
                err = ENXIO;
                goto error_2;
	}
#else
	if (!vn_isdisk(devvp, &err)) 
		goto error_2;
#endif
	if (mp->mnt_flag & MNT_UPDATE) {
#if 0
		/*
		 ********************
		 * UPDATE
		 ********************
		 */

		if (devvp != ntmp->um_devvp)
			err = EINVAL;	/* needs translation */
		else
			vrele(devvp);
		/*
		 * Update device name only on success
		 */
		if( !err) {
			/* Save "mounted from" info for mount point (NULL pad)*/
                        /*ее Check for errors.  Do this earlier, in case parameters are bad? */
			copyinstr(	args.fspec,
					mp->mnt_stat.f_mntfromname,
					MNAMELEN - 1,
					&size);
			bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
		}
#endif
	} else {
		/*
		 ********************
		 * NEW MOUNT
		 ********************
		 */

		/*
		 * Since this is a new mount, we want the names for
		 * the device and the mount point copied in.  If an
		 * error occurs, the mountpoint is discarded by the
		 * upper level code.  Note that vfs_mount() handles
		 * copying the mountpoint f_mntonname for us, so we
		 * don't have to do it here unless we want to set it
		 * to something other than "path" for some rason.
		 */
		/* Save "mounted from" info for mount point (NULL pad)*/
		copyinstr(	args.fspec,			/* device name*/
				mp->mnt_stat.f_mntfromname,	/* save area*/
				MNAMELEN - 1,			/* max size*/
				&size);				/* real size*/
		bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
#ifdef APPLE
                copyinstr(	path,
                                mp->mnt_stat.f_mntonname,
                                MNAMELEN - 1,
                                &size);
                bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);

		mp->mnt_flag |= MNT_RDONLY;	/* Force read-only for Darwin */
#endif
		err = ntfs_mountfs(devvp, mp, &args, td);
	}
	if (err) {
		goto error_2;
	}

dostatfs:
	/*
	 * Initialize FS stat information in mount struct; uses both
	 * mp->mnt_stat.f_mntonname and mp->mnt_stat.f_mntfromname
	 *
	 * This code is common to root and non-root mounts
	 */
	(void)VFS_STATFS(mp, &mp->mnt_stat, td);

	goto success;


error_2:	/* error with devvp held*/

	/* release devvp before failing*/
	vrele(devvp);

error_1:	/* no state to back out*/

success:
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
ntfs_mountfs(devvp, mp, argsp, td)
	register struct vnode *devvp;
	struct mount *mp;
	struct ntfs_args *argsp;
#ifdef APPLE
	struct proc *td;
#else
	struct thread *td;
#endif
{
	struct buf *bp;
	struct ntfsmount *ntmp;
	dev_t dev = devvp->v_rdev;
	int error, ronly, ncount, i;
	struct vnode *vp;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	ncount = vcount(devvp);
#ifndef APPLE
	if (devvp->v_object)
		ncount -= 1;
#endif
	if (ncount > 1 && devvp != rootvp)
		return (EBUSY);
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef APPLE
	error = vinvalbuf(devvp, V_SAVE, td->p_ucred, td, 0, 0);
#else
	error = vinvalbuf(devvp, V_SAVE, td->td_ucred, td, 0, 0);
#endif
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, td);
	VOP_UNLOCK(devvp, 0, td);
	if (error)
		return (error);

	bp = NULL;

	error = bread(devvp, BBLOCK, BBSIZE, NOCRED, &bp);
	if (error)
		goto out;
#ifdef APPLE
	MALLOC(ntmp, struct ntfsmount *, sizeof *ntmp, M_NTFSMNT, M_WAITOK);
	bzero(ntmp, sizeof *ntmp);
#else
	ntmp = malloc( sizeof *ntmp, M_NTFSMNT, M_WAITOK | M_ZERO);
#endif
	bootfile_to_host((struct bootfile *) bp->b_data, &ntmp->ntm_bootfile);
	brelse( bp );
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

	/* Copy in the 8-bit to Unicode conversion table */
	if (argsp->flag & NTFSMNT_U2WTABLE) {
		ntfs_82u_init(ntmp, argsp->u2w);
	} else {
		ntfs_82u_init(ntmp, NULL);
	}

	/* Initialize Unicode to 8-bit table from 8toU table */
	ntfs_u28_init(ntmp, ntmp->ntm_82u);

	mp->mnt_data = (qaddr_t)ntmp;

	dprintf(("ntfs_mountfs(): case-%s,%s uid: %d, gid: %d, mode: %o\n",
		(ntmp->ntm_flag & NTFS_MFLAG_CASEINS)?"insens.":"sens.",
		(ntmp->ntm_flag & NTFS_MFLAG_ALLNAMES)?" allnames,":"",
		ntmp->ntm_uid, ntmp->ntm_gid, ntmp->ntm_mode));

	/*
	 * We read in some system nodes to do not allow 
	 * reclaim them and to have everytime access to them.
	 */ 
	{
		ino_t pi[3] = { NTFS_MFTINO, NTFS_ROOTINO, NTFS_BITMAPINO };
		for (i=0; i<3; i++) {
#ifdef APPLE
			error = VFS_VGET(mp, &pi[i],
					 &(ntmp->ntm_sysvn[pi[i]]));
#else
			error = VFS_VGET(mp, pi[i], LK_EXCLUSIVE,
					 &(ntmp->ntm_sysvn[pi[i]]));
#endif
			if(error)
				goto out1;
#ifdef APPLE
			ntmp->ntm_sysvn[pi[i]]->v_flag |= VSYSTEM;
#else
			ntmp->ntm_sysvn[pi[i]]->v_vflag |= VV_SYSTEM;
#endif
			VREF(ntmp->ntm_sysvn[pi[i]]);
			vput(ntmp->ntm_sysvn[pi[i]]);
		}
	}

	/* read the Unicode lowercase --> uppercase translation table,
	 * if necessary */
	if ((error = ntfs_toupper_use(mp, ntmp)))
		goto out1;

	/*
	 * Scan $BitMap and count free clusters
	 */
	error = ntfs_calccfree(ntmp, &ntmp->ntm_cfree);
	if(error)
		goto out1;

	/*
	 * Read and translate to internal format attribute
	 * definition file. 
	 */
	{
		int num,j;
		struct attrdef ad;
#ifdef APPLE
                ino_t attrdef_ino = NTFS_ATTRDEFINO;
#endif
                
		/* Open $AttrDef */
#ifdef APPLE
		error = VFS_VGET(mp, &attrdef_ino, &vp );
#else
		error = VFS_VGET(mp, NTFS_ATTRDEFINO, LK_EXCLUSIVE, &vp );
#endif
		if(error) 
			goto out1;

		/* Count valid entries */
		for(num=0;;num++) {
			error = ntfs_readattr(ntmp, VTONT(vp),
					NTFS_A_DATA, NULL,
					num * sizeof(ad), sizeof(ad),
					&ad, NULL);
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
					&ad, NULL);
			if (error)
				goto out1;
			j = 0;
			do {
				ntmp->ntm_ad[i].ad_name[j] = le16toh(ad.ad_name[j]);
			} while(ad.ad_name[j++]);
			ntmp->ntm_ad[i].ad_namelen = j - 1;
			ntmp->ntm_ad[i].ad_type = le32toh(ad.ad_type);
		}

		vput(vp);
	}

#ifdef APPLE
	mp->mnt_stat.f_fsid.val[0] = (long) dev;
#else
	mp->mnt_stat.f_fsid.val[0] = dev2udev(dev);
#endif
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_maxsymlinklen = 0;
	mp->mnt_flag |= MNT_LOCAL;
#ifdef APPLE
	devvp->v_specflags |= SI_MOUNTEDON;
#else
	devvp->v_rdev->si_mountpoint = mp;
#endif
	return (0);

out1:
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		if(ntmp->ntm_sysvn[i]) vrele(ntmp->ntm_sysvn[i]);

	if (vflush(mp, 0, 0))
		dprintf(("ntfs_mountfs: vflush failed\n"));

out:
#ifdef APPLE
	devvp->v_specflags &= ~SI_MOUNTEDON;
#else
	devvp->v_rdev->si_mountpoint = NULL;
#endif
	if (bp)
		brelse(bp);

	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, td);
	if (ntmp) {
            if (ntmp->ntm_ad)
                FREE(ntmp->ntm_ad, M_NTFSMNT);
            if (ntmp->ntm_82u)
                FREE(ntmp->ntm_82u, M_TEMP);
            if (ntmp->ntm_u28)
                FREE(ntmp->ntm_u28, M_TEMP);
            FREE(ntmp, M_NTFSMNT);
            mp->mnt_data = (qaddr_t) 0;
        }
	return (error);
}

static int
ntfs_unmount( 
	struct mount *mp,
	int mntflags,
#ifdef APPLE
	struct proc *td)
#else
	struct thread *td)
#endif
{
	register struct ntfsmount *ntmp;
	int error, ronly = 0, flags, i;

	dprintf(("ntfs_unmount: unmounting...\n"));
	ntmp = VFSTONTFS(mp);

	flags = 0;
	if(mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	dprintf(("ntfs_unmount: vflushing...\n"));
	error = vflush(mp, 0, flags | SKIPSYSTEM);
	if (error) {
		printf("ntfs_unmount: vflush failed: %d\n",error);
		return (error);
	}

	/* Check if only system vnodes are rest */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if((ntmp->ntm_sysvn[i]) && 
		    (ntmp->ntm_sysvn[i]->v_usecount > 1)) return (EBUSY);

	/* Dereference all system vnodes */
	for(i=0;i<NTFS_SYSNODESNUM;i++)
		 if(ntmp->ntm_sysvn[i]) vrele(ntmp->ntm_sysvn[i]);

	/* vflush system vnodes */
	error = vflush(mp, 0, flags);
	if (error)
		printf("ntfs_unmount: vflush failed(sysnodes): %d\n",error);

	/* Check if the type of device node isn't VBAD before
	 * touching v_specinfo.  If the device vnode is revoked, the
	 * field is NULL and touching it causes null pointer derefercence.
	 */
	if (ntmp->ntm_devvp->v_type != VBAD)
#ifdef APPLE
                ntmp->ntm_devvp->v_specflags &= ~SI_MOUNTEDON;
#else
		ntmp->ntm_devvp->v_rdev->si_mountpoint = NULL;
#endif

	vinvalbuf(ntmp->ntm_devvp, V_SAVE, NOCRED, td, 0, 0);

	error = VOP_CLOSE(ntmp->ntm_devvp, ronly ? FREAD : FREAD|FWRITE,
		NOCRED, td);

	vrele(ntmp->ntm_devvp);

	/* free the toupper table, if this has been last mounted ntfs volume */
	ntfs_toupper_unuse();

	dprintf(("ntfs_umount: freeing memory...\n"));
	ntfs_u28_uninit(ntmp);
	ntfs_82u_uninit(ntmp);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	FREE(ntmp->ntm_ad, M_NTFSMNT);
	FREE(ntmp, M_NTFSMNT);
	return (error);
}

static int
ntfs_root(
	struct mount *mp,
	struct vnode **vpp )
{
	struct vnode *nvp;
	int error = 0;
#ifdef APPLE
        ino_t root_ino = NTFS_ROOTINO;
#endif
        
	dprintf(("ntfs_root(): sysvn: %p\n",
		VFSTONTFS(mp)->ntm_sysvn[NTFS_ROOTINO]));
#ifdef APPLE
	error = VFS_VGET(mp, &root_ino, &nvp);
#else
	error = VFS_VGET(mp, (ino_t)NTFS_ROOTINO, LK_EXCLUSIVE, &nvp);
#endif
	if(error) {
		printf("ntfs_root: VFS_VGET failed: %d\n",error);
		return (error);
	}

	*vpp = nvp;
	return (0);
}

int
ntfs_calccfree(
	struct ntfsmount *ntmp,
	cn_t *cfreep)
{
	struct vnode *vp;
	u_int8_t *tmp;
	int j, error;
	long cfree = 0;
	size_t bmsize, i;

	vp = ntmp->ntm_sysvn[NTFS_BITMAPINO];

	bmsize = VTOF(vp)->f_size;

	MALLOC(tmp, u_int8_t *, bmsize, M_TEMP, M_WAITOK);

	error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
			       0, bmsize, tmp, NULL);
	if (error)
		goto out;

	for(i=0;i<bmsize;i++)
		for(j=0;j<8;j++)
			if(~tmp[i] & (1 << j)) cfree++;
	*cfreep = cfree;

    out:
	FREE(tmp, M_TEMP);
	return(error);
}

static int
ntfs_statfs(
	struct mount *mp,
	struct statfs *sbp,
#ifdef APPLE
	struct proc *td)
#else
	struct thread *td)
#endif
{
	struct ntfsmount *ntmp = VFSTONTFS(mp);
	u_int64_t mftsize,mftallocated;

	dprintf(("ntfs_statfs():\n"));

	mftsize = VTOF(ntmp->ntm_sysvn[NTFS_MFTINO])->f_size;
	mftallocated = VTOF(ntmp->ntm_sysvn[NTFS_MFTINO])->f_allocated;

	sbp->f_type = mp->mnt_vfc->vfc_typenum;
	sbp->f_bsize = ntmp->ntm_bps;
	sbp->f_iosize = ntmp->ntm_bps * ntmp->ntm_spc;
	sbp->f_blocks = ntmp->ntm_bootfile.bf_spv;
	sbp->f_bfree = sbp->f_bavail = ntfs_cntobn(ntmp->ntm_cfree);
	sbp->f_ffree = sbp->f_bfree / ntmp->ntm_bpmftrec;
	sbp->f_files = mftallocated / ntfs_bntob(ntmp->ntm_bpmftrec) +
		       sbp->f_ffree;
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	sbp->f_flags = mp->mnt_flag;

	return (0);
}

/*ARGSUSED*/
static int
ntfs_fhtovp(
	struct mount *mp,
	struct fid *fhp,
#ifdef APPLE
	struct mbuf *nam,
	struct vnode **vpp,
	int *exflagsp,
	struct ucred **credanonp)
#else
	struct vnode **vpp)
#endif
{
	struct vnode *nvp;
	struct ntfid *ntfhp = (struct ntfid *)fhp;
	int error;

	ddprintf(("ntfs_fhtovp(): %d\n", ntfhp->ntfid_ino));

#ifdef APPLE
	if ((error = VFS_VGET(mp, &ntfhp->ntfid_ino, &nvp)) != 0) {
#else
	if ((error = VFS_VGET(mp, ntfhp->ntfid_ino, LK_EXCLUSIVE, &nvp)) != 0) {
#endif
		*vpp = NULLVP;
		return (error);
	}
	/* XXX as unlink/rmdir/mkdir/creat are not currently possible
	 * with NTFS, we don't need to check anything else for now */
	*vpp = nvp;

	return (0);
}

static int
ntfs_vptofh(
	struct vnode *vp,
	struct fid *fhp)
{
	register struct ntnode *ntp;
	register struct ntfid *ntfhp;

	ddprintf(("ntfs_fhtovp(): %p\n", vp));

	ntp = VTONT(vp);
	ntfhp = (struct ntfid *)fhp;
	ntfhp->ntfid_len = sizeof(struct ntfid);
	ntfhp->ntfid_ino = ntp->i_number;
	/* ntfhp->ntfid_gen = ntp->i_gen; */
	return (0);
}

int
ntfs_vgetex(
	struct mount *mp,
	ino_t ino,
	u_int32_t attrtype,
	char *attrname,
	u_long lkflags,
	u_long flags,
#ifdef APPLE
	struct proc *td,
#else
	struct thread *td,
#endif
	struct vnode **vpp) 
{
	int error;
	register struct ntfsmount *ntmp;
	struct ntnode *ip;
	struct fnode *fp;
	struct vnode *vp;
	enum vtype f_type;

	dprintf(("ntfs_vgetex: ino: %d, attr: 0x%x:%s, lkf: 0x%lx, f: 0x%lx\n",
		ino, attrtype, attrname?attrname:"", (u_long)lkflags,
		(u_long)flags ));

	ntmp = VFSTONTFS(mp);
	*vpp = NULL;

	/* Get ntnode */
	error = ntfs_ntlookup(ntmp, ino, &ip);
	if (error) {
		printf("ntfs_vget: ntfs_ntget failed\n");
		return (error);
	}

	/* It may be not initialized fully, so force load it */
	if (!(flags & VG_DONTLOADIN) && !(ip->i_flag & IN_LOADED)) {
		error = ntfs_loadntnode(ntmp, ip);
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
	f_type = VNON;
	if (!(flags & VG_DONTVALIDFN) && !(fp->f_flag & FN_VALID)) {
		if ((ip->i_frflag & NTFS_FRFLAG_DIR) &&
		    (fp->f_attrtype == NTFS_A_DATA && fp->f_attrname == NULL)) {
			f_type = VDIR;
		} else if (flags & VG_EXT) {
			f_type = VNON;
			fp->f_size = fp->f_allocated = 0;
		} else {
			f_type = VREG;	

			error = ntfs_filesize(ntmp, fp, 
					      &fp->f_size, &fp->f_allocated);
			if (error) {
				ntfs_ntput(ip);
				return (error);
			}
		}

		fp->f_flag |= FN_VALID;
	}

        /* See if we already have a vnode for the file */
	if (FTOV(fp)) {
		vget(FTOV(fp), lkflags, td);
		*vpp = FTOV(fp);
		ntfs_ntput(ip);
#ifdef APPLE
		UBCINFOCHECK("ntfs_vgetex", *vpp);
#endif
		return (0);
	}

        /* Must create a new vnode */
	error = getnewvnode(VT_NTFS, ntmp->ntm_mountp, ntfs_vnodeop_p, &vp);
	if(error) {
		ntfs_frele(fp);
		ntfs_ntput(ip);
		return (error);
	}
	dprintf(("ntfs_vget: vnode: %p for ntnode: %d\n", vp,ino));

#ifdef APPLE
	lockinit(&fp->f_lock, PINOD, "fnode", 0, 0);
#else
	lockinit(&fp->f_lock, PINOD, "fnode", VLKTIMEOUT, 0);
#endif
	fp->f_vp = vp;
	vp->v_data = fp;
	vp->v_type = f_type;

#ifdef APPLE
	if (ino == NTFS_ROOTINO)
		vp->v_flag |= VROOT;
        if (ino < 24)	/*ее use NTFS_SYSNODENUM instead? */
                vp->v_flag |= VSYSTEM;
#else
	if (ino == NTFS_ROOTINO)
		vp->v_vflag |= VV_ROOT;
#endif

	ntfs_ntput(ip);

	if (lkflags & LK_TYPE_MASK) {
		error = vn_lock(vp, lkflags, td);
		if (error) {
			vput(vp);
			return (error);
		}
	}

	*vpp = vp;
#ifdef APPLE
	if (vp->v_type == VREG && (UBCINFOMISSING(vp) || UBCINFORECLAIMED(vp)))
		ubc_info_init(vp);
#endif
	return (0);
	
}

#ifdef APPLE
static int
ntfs_vget(
	struct mount *mp,
	void *ino,
	struct vnode **vpp) 
{
	return ntfs_vgetex(mp, *(ino_t*)ino, NTFS_A_DATA, NULL, LK_EXCLUSIVE, 0,
	    current_proc(), vpp);
}
#else
static int
ntfs_vget(
	struct mount *mp,
	ino_t ino,
	int lkflags,
	struct vnode **vpp) 
{
	return ntfs_vgetex(mp, ino, NTFS_A_DATA, NULL, lkflags, 0,
	    curthread, vpp);
}
#endif

#ifdef APPLE
/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
static int
ntfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return 0;
}


/*
 * Do operations associated with quotas
 */
int
ntfs_quotactl(mp, cmds, uid, arg, p)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
    return EOPNOTSUPP;
}


int	
ntfs_sync (mp, waitfor, cred, proc)
	struct mount *mp;
	int waitfor;
	struct ucred *cred; 
	struct proc *proc;
{
	return 0;
}


/*
 * Fast-FileSystem only?
 */
static int
ntfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
     int * name;
     u_int namelen;
     void* oldp;
     size_t * oldlenp;
     void * newp;
     size_t newlen;
     struct proc * p;
{
     return (EOPNOTSUPP);
}
#endif /* APPLE */

#ifdef APPLE
static struct vfsops ntfs_vfsops = {
	ntfs_mount,
	ntfs_start,
	ntfs_unmount,
	ntfs_root,
	ntfs_quotactl,
	ntfs_statfs,
	ntfs_sync,
	ntfs_vget,
	ntfs_fhtovp,
	ntfs_vptofh,
	ntfs_init,
	ntfs_sysctl,
};
#else
static struct vfsops ntfs_vfsops = {
	ntfs_mount,
	vfs_stdstart,
	ntfs_unmount,
	ntfs_root,
	vfs_stdquotactl,
	ntfs_statfs,
	vfs_stdsync,
	ntfs_vget,
	ntfs_fhtovp,
	vfs_stdcheckexp,
	ntfs_vptofh,
	ntfs_init,
	ntfs_uninit,
	vfs_stdextattrctl,
};
VFS_SET(ntfs_vfsops, ntfs, 0);
#endif /* APPLE */


#ifdef APPLE
static char ntfs_name[MFSNAMELEN] = "ntfs";

struct kmod_info_t *ntfs_kmod_infop;

typedef int (*PFI)();

extern int vfs_opv_numops;

extern struct vnodeopv_desc ntfs_vnodeop_opv_desc;


__private_extern__ int
ntfs_kext_start(struct kmod_info_t *ki, void *data)
{
#pragma unused(data)
	struct vfsconf	*newvfsconf = NULL;
	int	j;
	int	(***opv_desc_vector_p)(void *);
	int	(**opv_desc_vector)(void *);
	struct vnodeopv_entry_desc	*opve_descp; 
	int	error = 0; 
	boolean_t funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);
	ntfs_kmod_infop = ki;
	/*
	 * This routine is responsible for all the initialization that would
	 * ordinarily be done as part of the system startup;
	 */

	MALLOC(newvfsconf, void *, sizeof(struct vfsconf), M_TEMP,
	       M_WAITOK);
	bzero(newvfsconf, sizeof(struct vfsconf));
	newvfsconf->vfc_vfsops = &ntfs_vfsops;
	strncpy(&newvfsconf->vfc_name[0], ntfs_name, MFSNAMELEN);
	newvfsconf->vfc_typenum = maxvfsconf++;
	newvfsconf->vfc_refcount = 0;
	newvfsconf->vfc_flags = MNT_LOCAL;
	newvfsconf->vfc_mountroot = NULL;
	newvfsconf->vfc_next = NULL;

	opv_desc_vector_p = ntfs_vnodeop_opv_desc.opv_desc_vector_p;

	/*
	 * Allocate and init the vector.
	 * Also handle backwards compatibility.
	 */
	/* XXX - shouldn't be M_TEMP */

	MALLOC(*opv_desc_vector_p, PFI *, vfs_opv_numops * sizeof(PFI),
	       M_TEMP, M_WAITOK);
	bzero(*opv_desc_vector_p, vfs_opv_numops*sizeof(PFI));

	opv_desc_vector = *opv_desc_vector_p;

	for (j = 0; ntfs_vnodeop_opv_desc.opv_desc_ops[j].opve_op; j++) {
		opve_descp = &(ntfs_vnodeop_opv_desc.opv_desc_ops[j]);

		/*
		 * Sanity check:  is this operation listed
		 * in the list of operations?  We check this
		 * by seeing if its offest is zero.  Since
		 * the default routine should always be listed
		 * first, it should be the only one with a zero
		 * offset.  Any other operation with a zero
		 * offset is probably not listed in
		 * vfs_op_descs, and so is probably an error.
		 *
		 * A panic here means the layer programmer
		 * has committed the all-too common bug
		 * of adding a new operation to the layer's
		 * list of vnode operations but
		 * not adding the operation to the system-wide
		 * list of supported operations.
		 */
		if (opve_descp->opve_op->vdesc_offset == 0 &&
		    opve_descp->opve_op->vdesc_offset != VOFFSET(vop_default)) {
			printf("load_ntfs: operation %s not listed in %s.\n",
			       opve_descp->opve_op->vdesc_name,
			       "vfs_op_descs");
			panic("load_ntfs: bad operation");
		}
		/*
		 * Fill in this entry.
		 */
		opv_desc_vector[opve_descp->opve_op->vdesc_offset] =
		    opve_descp->opve_impl;
	}

	/*  
	 * Finally, go back and replace unfilled routines
	 * with their default.  (Sigh, an O(n^3) algorithm.  I
	 * could make it better, but that'd be work, and n is small.)
	 */  
	opv_desc_vector_p = ntfs_vnodeop_opv_desc.opv_desc_vector_p;

	/*   
	 * Force every operations vector to have a default routine.
	 */  
	opv_desc_vector = *opv_desc_vector_p;
	if (opv_desc_vector[VOFFSET(vop_default)] == NULL)
	    panic("ntfs_kext_start: operation vector without default routine.");
	for (j = 0; j < vfs_opv_numops; j++)  
		if (opv_desc_vector[j] == NULL)
			opv_desc_vector[j] =
			    opv_desc_vector[VOFFSET(vop_default)];

	/* Ok, vnode vectors are set up, vfs vectors are set up, add it in */
	error = vfsconf_add(newvfsconf);

	if (newvfsconf)
		FREE(newvfsconf, M_TEMP);

	thread_funnel_set(kernel_flock, funnel_state);
	return (error == 0 ? KERN_SUCCESS : KERN_FAILURE);
}
     

__private_extern__ int  
ntfs_kext_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki)
#pragma unused(data)
	boolean_t funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	vfsconf_del(ntfs_name);
	FREE(*ntfs_vnodeop_opv_desc.opv_desc_vector_p, M_TEMP);  

	thread_funnel_set(kernel_flock, funnel_state);
	return (KERN_SUCCESS);
}   
#endif APPLE
