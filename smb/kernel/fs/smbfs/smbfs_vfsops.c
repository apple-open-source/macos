/*
 * Copyright (c) 2000-2001, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $Id: smbfs_vfsops.c,v 1.18 2002/06/29 21:15:25 lindak Exp $
 */
#ifndef APPLE
#include "opt_nsmb.h"
#endif
#ifndef NETSMB
#error "SMBFS requires option NETSMB"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>


#ifdef APPLE
#include <sys/syslog.h>
#include <sys/smb_apple.h>
#include <sys/iconv.h>
#endif

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

#include <sys/buf.h>

int smbfs_debuglevel = 0;

static int smbfs_version = SMBFS_VERSION;

#ifdef SMBFS_USEZONE
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

vm_zone_t smbfsmount_zone;
#endif

#ifdef APPLE
#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_smb);
#endif
SYSCTL_NODE(_net_smb, OID_AUTO, fs, CTLFLAG_RW, 0, "SMB/CIFS file system");
SYSCTL_INT(_net_smb_fs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, debuglevel, CTLFLAG_RW,
	   &smbfs_debuglevel, 0, "");
extern struct sysctl_oid sysctl__net_smb;
extern struct sysctl_oid sysctl__net_smb_fs_iconv;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_add;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_cslist;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_drvlist;
#else /* APPLE */
#ifdef SYSCTL_DECL
SYSCTL_DECL(_vfs_smbfs);
#endif
SYSCTL_NODE(_vfs, OID_AUTO, smbfs, CTLFLAG_RW, 0, "SMB/CIFS file system");
SYSCTL_INT(_vfs_smbfs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_vfs_smbfs, OID_AUTO, debuglevel, CTLFLAG_RW, &smbfs_debuglevel, 0, "");
#endif /* APPLE */

static MALLOC_DEFINE(M_SMBFSHASH, "SMBFS hash", "SMBFS hash table");


static int smbfs_mount(struct mount *, char *, caddr_t,
			struct nameidata *, struct proc *);
static int smbfs_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
static int smbfs_root(struct mount *, struct vnode **);
static int smbfs_start(struct mount *, int, struct proc *);
static int smbfs_statfs(struct mount *, struct statfs *, struct proc *);
static int smbfs_sync(struct mount *, int, struct ucred *, struct proc *);
static int smbfs_unmount(struct mount *, int, struct proc *);
static int smbfs_init(struct vfsconf *vfsp);
#ifdef APPLE
static int smbfs_sysctl(int *, u_int, void*, size_t *, void *, size_t, struct proc *);
#else
static int smbfs_uninit(struct vfsconf *vfsp);
#endif /* APPLE */

#if defined(APPLE) || __FreeBSD_version < 400009
static int smbfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp);
static int smbfs_fhtovp(struct mount *, struct fid *,
#ifdef APPLE
			struct mbuf *, struct vnode **, int *,
#else
			struct sockaddr *, struct vnode **, int *,
#endif /* APPLE */
			struct ucred **);
static int smbfs_vptofh(struct vnode *, struct fid *);
#endif

static struct vfsops smbfs_vfsops = {
	smbfs_mount,
	smbfs_start,
	smbfs_unmount,
	smbfs_root,
	smbfs_quotactl,
	smbfs_statfs,
	smbfs_sync,
#if __FreeBSD_version > 400008
	vfs_stdvget,
	vfs_stdfhtovp,		/* shouldn't happen */
	vfs_stdcheckexp,
	vfs_stdvptofh,		/* shouldn't happen */
#else
	smbfs_vget,
	smbfs_fhtovp,
	smbfs_vptofh,
#endif
	smbfs_init,
#ifdef APPLE
	smbfs_sysctl
#else
	smbfs_uninit,
#ifndef FB_RELENG3
	vfs_stdextattrctl,
#else
#define	M_USE_RESERVE	M_KERNEL
	&sysctl___vfs_smbfs
#endif
#endif /* APPLE */
};


VFS_SET(smbfs_vfsops, smbfs, VFCF_NETWORK);

#ifdef MODULE_DEPEND
MODULE_DEPEND(smbfs, netsmb, NSMB_VERSION, NSMB_VERSION, NSMB_VERSION);
MODULE_DEPEND(smbfs, libiconv, 1, 1, 1);
#endif

int smbfs_pbuf_freecnt = -1;	/* start out unlimited */

static int
smbfs_mount(struct mount *mp, char *path, caddr_t data, 
	struct nameidata *ndp, struct proc *p)
{
	struct smbfs_args args;		/* will hold data from mount request */
	struct smbmount *smp = NULL;
	struct smb_vc *vcp;
	struct smb_share *ssp = NULL;
	struct vnode *vp;
	struct smb_cred scred;
	size_t size;
	int error;
	char *pc, *pe;
	struct mount *mp2;

	if (data == NULL) {
		printf("missing data argument\n");
		return EINVAL;
	}
	if (mp->mnt_flag & MNT_UPDATE) {
		printf("MNT_UPDATE not implemented");
		return EOPNOTSUPP;
	}
	error = copyin(data, (caddr_t)&args, sizeof(struct smbfs_args));
	if (error)
		return error;
	if (args.version != SMBFS_VERSION) {
		printf("mount version mismatch: kernel=%d, mount=%d\n",
		    SMBFS_VERSION, args.version);
		return EINVAL;
	}
	smb_makescred(&scred, p, p->p_ucred);
	error = smb_dev2share(args.dev, SMBM_EXEC, &scred, &ssp);
	if (error) {
		printf("invalid device handle %d (%d)\n", args.dev, error);
		return error;
	}
	vcp = SSTOVC(ssp);
	smb_share_unlock(ssp, 0, p);
#ifdef APPLE
	/* Give the Finder et al a better hint */
	mp->mnt_stat.f_iosize = 128 * 1024;
#else
	mp->mnt_stat.f_iosize = SSTOVC(ssp)->vc_txmax;
#endif /* APPLE */

#ifdef SMBFS_USEZONE
	smp = zalloc(smbfsmount_zone);
#else
	MALLOC(smp, struct smbmount*, sizeof(*smp), M_SMBFSDATA, M_USE_RESERVE);
#endif
	if (smp == NULL) {
		printf("could not alloc smbmount\n");
		error = ENOMEM;
		goto bad;
	}
	bzero(smp, sizeof(*smp));
	mp->mnt_data = (qaddr_t)smp;
	smp->sm_hash = hashinit(desiredvnodes, M_SMBFSHASH, &smp->sm_hashlen);
	if (smp->sm_hash == NULL)
		goto bad;
	lockinit(&smp->sm_hashlock, PVFS, "smbfsh", 0, 0);
	smp->sm_share = ssp;
	smp->sm_root = NULL;
	smp->sm_args = args;
	smp->sm_caseopt = args.caseopt;
	smp->sm_args.file_mode = (smp->sm_args.file_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFREG;
	smp->sm_args.dir_mode  = (smp->sm_args.dir_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFDIR;

/*	simple_lock_init(&smp->sm_npslock);*/
	error = copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	if (error)
		goto bad;
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	pc = mp->mnt_stat.f_mntfromname;
	pe = pc + sizeof(mp->mnt_stat.f_mntfromname);
	bzero(pc, MNAMELEN);
	*pc++ = '/';
	*pc++ = '/';
	pc=index(strncpy(pc, vcp->vc_username, pe - pc - 2), 0);
	if (pc < pe-1) {
		*(pc++) = '@';
		pc = index(strncpy(pc, vcp->vc_srvname, pe - pc - 2), 0);
		if (pc < pe - 1) {
			*(pc++) = '/';
			strncpy(pc, ssp->ss_name, pe - pc - 2);
		}
	}
	/*
	 * XXX
	 * This circumvents the "unique disk id" design flaw by disallowing
	 * multiple mounts of the same filesystem as the same user.  The flaw
	 * is that URLMount, DiskArb, FileManager, Finder added new semantics
	 * to f_mntfromname, requiring that field to be a unique id for a
	 * given mount.  (Another flaw added is to require that that field
	 * have sufficient information to remount.  That is not solved here.)
	 * This is XXX because it cripples multiple mounting, a traditional
	 * unix feature useful in multiuser and chroot-ed environments.  This
	 * limitation can often be (manually) avoided by altering the remote
	 * login name - even a difference in case is sufficient, and
	 * should authenticate as if there were no difference.
	 *
	 * Details:
	 * "funnel" keeps us from having to lock mountlist during scan.
	 * The scan could be more cpu-efficient, but mounts are not a hot spot.
	 * Scanning mountlist is more robust than using smb_vclist.
	 */
	for (mp2 = mountlist.cqh_first; mp2 != (void *)&mountlist;
	     mp2 = mp2->mnt_list.cqe_next)
		if (strncmp(mp2->mnt_stat.f_mntfromname,
			    mp->mnt_stat.f_mntfromname, MNAMELEN) == 0) {
			error = EBUSY;
			goto bad;
		}
	/* protect against invalid mount points */
	smp->sm_args.mount_point[sizeof(smp->sm_args.mount_point) - 1] = '\0';
	vfs_getnewfsid(mp);
	error = smbfs_root(mp, &vp);
	if (error)
		goto bad;
	VOP_UNLOCK(vp, 0, p);
	SMBVDEBUG("root.v_usecount = %d\n", vp->v_usecount);

#ifdef DIAGNOSTICS
	SMBERROR("mp=%p\n", mp);
#endif
	return error;
bad:
	if (smp) {
		mp->mnt_data = (qaddr_t)0;
		if (smp->sm_hash)
			free(smp->sm_hash, M_SMBFSHASH);
		lockdestroy(&smp->sm_hashlock);
#ifdef SMBFS_USEZONE
		zfree(smbfsmount_zone, smp);
#else
		free(smp, M_SMBFSDATA);
#endif
	}
	if (ssp)
		smb_share_put(ssp, &scred);
	return error;
}

/* Unmount the filesystem described by mp. */
static int
smbfs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct vnode *vp;
	struct smb_cred scred;
	int error, flags;

	SMBVDEBUG("smbfs_unmount: flags=%04x\n", mntflags);
	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	error = VFS_ROOT(mp, &vp);
	if (error)
		return (error);
	if (vp->v_usecount > 2) {
		printf("smbfs_unmount: usecnt=%d\n", (int)vp->v_usecount);
		vput(vp);
		return EBUSY;
	}
	error = vflush(mp, vp, flags);
	if (error) {
		vput(vp);
		return error;
	}
	vput(vp);
	vrele(vp);
	vgone(vp);
	smb_makescred(&scred, p, p->p_ucred);
	smb_share_put(smp->sm_share, &scred);
	mp->mnt_data = (qaddr_t)0;

	if (smp->sm_hash)
		free(smp->sm_hash, M_SMBFSHASH);
	lockdestroy(&smp->sm_hashlock);
#ifdef SMBFS_USEZONE
	zfree(smbfsmount_zone, smp);
#else
	free(smp, M_SMBFSDATA);
#endif
	mp->mnt_flag &= ~MNT_LOCAL;
	return error;
}

/* 
 * Return locked root vnode of a filesystem
 */
static int
smbfs_root(struct mount *mp, struct vnode **vpp)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct vnode *vp;
	struct smbnode *np;
	struct smbfattr fattr;
	struct proc *p = curproc;
	struct ucred *cred = p->p_ucred;
	struct smb_cred scred;
	int error;

	if (smp == NULL) {
		SMBERROR("smp == NULL (bug in umount)\n");
		return EINVAL;
	}
	if (smp->sm_root) {
		*vpp = SMBTOV(smp->sm_root);
		return vget(*vpp, LK_EXCLUSIVE | LK_RETRY, p);
	}
	smb_makescred(&scred, p, cred);
	error = smbfs_smb_lookup(NULL, NULL, NULL, &fattr, &scred);
	if (error)
		return error;
	error = smbfs_nget(mp, NULL, "TheRooT", 7, &fattr, &vp);
	if (error)
		return error;
	vp->v_flag |= VROOT;
	np = VTOSMB(vp);
	smp->sm_root = np;
	*vpp = vp;
	return 0;
}

/*
 * Vfs start routine, a no-op.
 */
/* ARGSUSED */
static int
smbfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	SMBVDEBUG("flags=%04x\n", flags);
	return 0;
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
static int
smbfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	SMBVDEBUG("return EOPNOTSUPP\n");
	return EOPNOTSUPP;
}

/*ARGSUSED*/
int
smbfs_init(struct vfsconf *vfsp)
{
	smb_checksmp();

#ifdef SMBFS_USEZONE
	smbfsmount_zone = zinit("SMBFSMOUNT", sizeof(struct smbmount), 0, 0, 1);
#endif
#ifndef APPLE
	smbfs_pbuf_freecnt = nswbuf / 2 + 1;
#endif
	SMBVDEBUG("done.\n");
	return 0;
}

#ifndef APPLE
/*ARGSUSED*/
int
smbfs_uninit(struct vfsconf *vfsp)
{

	SMBVDEBUG("done.\n");
	return 0;
}
#endif

/*
 * smbfs_statfs call
 */
int
smbfs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode *np = smp->sm_root;
	struct smb_share *ssp = smp->sm_share;
	struct smb_cred scred;
	int error = 0;

	if (np == NULL)
		return EINVAL;
#ifdef APPLE
	/* Give the Finder et al a better hint */
	sbp->f_iosize = 128 * 1024;	/* optimal transfer block size */
#else
	sbp->f_iosize = SSTOVC(ssp)->vc_txmax;	/* optimal transfer block size */
	sbp->f_spare2 = 0;			/* placeholder */
#endif /* APPLE */
	smb_makescred(&scred, p, p->p_ucred);

	if (SMB_DIALECT(SSTOVC(ssp)) >= SMB_DIALECT_LANMAN2_0)
		error = smbfs_smb_statfs2(ssp, sbp, &scred);
	else
		error = smbfs_smb_statfs(ssp, sbp, &scred);
	if (error)
		return error;
	sbp->f_flags = 0;		/* copy of mount exported flags */
	if (sbp != &mp->mnt_stat) {
		sbp->f_fsid = mp->mnt_stat.f_fsid;	/* file system id */
		sbp->f_owner = mp->mnt_stat.f_owner;	/* user that mounted the filesystem */
		sbp->f_type = mp->mnt_vfc->vfc_typenum;	/* type of filesystem */
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return 0;
}

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
static int
smbfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	struct vnode *vp;
	int error, allerror = 0;
#ifdef APPLE
	int didhold = 0;
#endif
	/*
	 * Force stale buffer cache information to be flushed.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first;
	     vp != NULL;
	     vp = vp->v_mntvnodes.le_next) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
#ifdef APPLE
		if (VOP_ISLOCKED(vp) || vp->v_dirtyblkhd.lh_first == NULL)
#else
#ifndef FB_RELENG3
		if (VOP_ISLOCKED(vp, NULL) || TAILQ_EMPTY(&vp->v_dirtyblkhd) ||
#else
		if (VOP_ISLOCKED(vp) || TAILQ_EMPTY(&vp->v_dirtyblkhd) ||
#endif
		    waitfor == MNT_LAZY)
#endif /* APPLE */
			continue;
		if (vget(vp, LK_EXCLUSIVE, p))
			goto loop;
#ifdef APPLE
		didhold = ubc_hold(vp);
#endif
		error = VOP_FSYNC(vp, cred, waitfor, p);
		if (error)
			allerror = error;
#ifdef APPLE
		VOP_UNLOCK(vp, 0, p);
		if (didhold)
			(void)ubc_rele(vp);
		vrele(vp);
#else
		vput(vp);
#endif
	}
	return (allerror);
}

#if defined(APPLE) || __FreeBSD_version < 400009
/*
 * smbfs flat namespace lookup. Unsupported.
 */
/* ARGSUSED */
static int smbfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

/* ARGSUSED */
static int smbfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	struct mount *mp;
	struct fid *fhp;
#ifdef APPLE
	struct mbuf *nam;
#else
	struct sockaddr *nam;
#endif
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	return (EINVAL);
}

/*
 * Vnode pointer to File handle, should never happen either
 */
/* ARGSUSED */
static int
smbfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	return (EINVAL);
}

#endif /* __FreeBSD_version < 400009 */


#ifdef APPLE
static int
smbfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
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

static char smbfs_name[MFSNAMELEN] = "smbfs";

kmod_info_t *smbfs_kmod_infop;

typedef int (*PFI)();

extern int vfs_opv_numops;

extern struct vnodeopv_desc smbfs_vnodeop_opv_desc;

extern int version_major;
extern int version_minor;


__private_extern__ int
smbfs_module_start(kmod_info_t *ki, void *data)
{
#pragma unused(data)
	boolean_t funnel_state;
	struct vfsconf  *newvfsconf = NULL;
	int     j;
	int     (***opv_desc_vector_p)(void *);
	int     (**opv_desc_vector)(void *);
	struct vnodeopv_entry_desc      *opve_descp;
	int     error = 0;

#if 0
	/* instead of this just set breakpoint on kmod_start_or_stop */
	Debugger("smb module start");
#endif
	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	smbfs_kmod_infop = ki;
	/*
	 * This routine is responsible for all the initialization that would
	 * ordinarily be done as part of the system startup;
	 */
	MALLOC(newvfsconf, void *, sizeof(struct vfsconf), M_TEMP,
	       M_WAITOK);
	bzero(newvfsconf, sizeof(struct vfsconf));
	newvfsconf->vfc_vfsops = &smbfs_vfsops;
	strncpy(&newvfsconf->vfc_name[0], smbfs_name, MFSNAMELEN);
	newvfsconf->vfc_typenum = maxvfsconf++;
	newvfsconf->vfc_refcount = 0;
	newvfsconf->vfc_flags = 0;
	newvfsconf->vfc_mountroot = NULL;
	newvfsconf->vfc_next = NULL;

	opv_desc_vector_p = smbfs_vnodeop_opv_desc.opv_desc_vector_p;
	/*
	 * Allocate and init the vector.
	 * Also handle backwards compatibility.
	 */
	/* XXX - shouldn't be M_TEMP */
	MALLOC(*opv_desc_vector_p, PFI *, vfs_opv_numops * sizeof(PFI),
	       M_TEMP, M_WAITOK);
	bzero(*opv_desc_vector_p, vfs_opv_numops*sizeof(PFI));

	opv_desc_vector = *opv_desc_vector_p;

	for (j = 0; smbfs_vnodeop_opv_desc.opv_desc_ops[j].opve_op; j++) {
		opve_descp = &(smbfs_vnodeop_opv_desc.opv_desc_ops[j]);
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
			printf("smbfs_module_start: op %s not listed in %s.\n",
			       opve_descp->opve_op->vdesc_name,
			       "vfs_op_descs");
			panic("smbfs_module_start: bad operation");
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
	opv_desc_vector_p = smbfs_vnodeop_opv_desc.opv_desc_vector_p;
	/*
	 * Force every operations vector to have a default routine.
	 */
	opv_desc_vector = *opv_desc_vector_p;
	if (opv_desc_vector[VOFFSET(vop_default)] == NULL)
	    panic("smbfs_module_start: op vector without default routine.");
	for (j = 0; j < vfs_opv_numops; j++)
		if (opv_desc_vector[j] == NULL)
			opv_desc_vector[j] =
			    opv_desc_vector[VOFFSET(vop_default)];
	/* Ok, vnode vectors are set up, vfs vectors are set up, add it in */
	error = vfsconf_add(newvfsconf);
	if (newvfsconf)
		free(newvfsconf, M_TEMP);

	if (!error) {
		SEND_EVENT(iconv, MOD_LOAD);
		SEND_EVENT(iconv_ces, MOD_LOAD);
#ifdef XXX /* apparently unused */
		SEND_EVENT(iconv_xlat_?, MOD_LOAD); /* iconv_xlatmod_handler */
		SEND_EVENT(iconv_ces_table, MOD_LOAD);
#endif
		SEND_EVENT(iconv_xlat, MOD_LOAD);
		/* Bring up UTF-8 converter */
		SEND_EVENT(iconv_utf8, MOD_LOAD);
		iconv_add("utf8", "utf-8", "ucs-2");
		iconv_add("utf8", "ucs-2", "utf-8");
		/* Bring up default codepage converter */
		SEND_EVENT(iconv_codepage, MOD_LOAD);
		iconv_add("codepage", "utf-8", "cp437");
		iconv_add("codepage", "cp437", "utf-8");
		SEND_EVENT(dev_netsmb, MOD_LOAD);

		sysctl_register_oid(&sysctl__net_smb);
		sysctl_register_oid(&sysctl__net_smb_fs);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv_add);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv_cslist);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv_drvlist);
	}
	thread_funnel_set(kernel_flock, funnel_state);
	return (error);
}


__private_extern__ int
smbfs_module_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki)
#pragma unused(data)
	boolean_t funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_drvlist);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_cslist);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_add);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv);
	sysctl_unregister_oid(&sysctl__net_smb_fs);
	sysctl_unregister_oid(&sysctl__net_smb);

	vfsconf_del(smbfs_name);

	SEND_EVENT(dev_netsmb, MOD_UNLOAD);
	SEND_EVENT(iconv_xlat, MOD_UNLOAD);
	SEND_EVENT(iconv_utf8, MOD_UNLOAD);
#ifdef XXX /* apparently unused */
	SEND_EVENT(iconv_ces_table, MOD_UNLOAD);
	SEND_EVENT(iconv_xlat_?, MOD_UNLOAD); /* iconv_xlatmod_handler */
#endif
	SEND_EVENT(iconv_ces, MOD_UNLOAD);
	SEND_EVENT(iconv, MOD_UNLOAD);

	free(*smbfs_vnodeop_opv_desc.opv_desc_vector_p, M_TEMP);
	thread_funnel_set(kernel_flock, funnel_state);
	return (0);
}
#endif /* APPLE */
