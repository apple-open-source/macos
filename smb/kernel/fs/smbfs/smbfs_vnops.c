/*
 * Copyright (c) 2000-2001 Boris Popov
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
 * $Id: smbfs_vnops.c,v 1.54.22.1 2004/01/27 22:00:48 lindak Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#ifdef APPLE
#include <sys/vnode.h>
#else
#include <sys/namei.h>
#endif
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#ifdef APPLE
#include <sys/namei.h>
#endif
#include <sys/unistd.h>
#ifndef APPLE
#include <sys/vnode.h>
#endif
#include <sys/lockf.h>

#ifndef APPLE
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>
#endif


#ifdef APPLE
#include <sys/syslog.h>
#include <sys/smb_apple.h>
#include <sys/attr.h>
#endif
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>
#include <fs/smbfs/smbfs_lockf.h>

#include <sys/buf.h>

extern void lockmgr_printinfo(struct lock__bsd__ *lkp);

/*
 * Prototypes for SMBFS vnode operations
 */
#ifdef APPLE
static int smbfs_create0(struct vop_create_args *);
#else
static int smbfs_create(struct vop_create_args *);
#endif
static int smbfs_mknod(struct vop_mknod_args *);
static int smbfs_open(struct vop_open_args *);
static int smbfs_close(struct vop_close_args *);
static int smbfs_access(struct vop_access_args *);
static int smbfs_getattr(struct vop_getattr_args *);
static int smbfs_setattr(struct vop_setattr_args *);
static int smbfs_read(struct vop_read_args *);
static int smbfs_write(struct vop_write_args *);
static int smbfs_fsync(struct vop_fsync_args *);
static int smbfs_remove(struct vop_remove_args *);
static int smbfs_link(struct vop_link_args *);
static int smbfs_lookup(struct vop_lookup_args *);
static int smbfs_rename(struct vop_rename_args *);
#ifdef APPLE
static int smbfs_mkdir0(struct vop_mkdir_args *);
#else
static int smbfs_mkdir(struct vop_mkdir_args *);
#endif
static int smbfs_rmdir(struct vop_rmdir_args *);
static int smbfs_symlink(struct vop_symlink_args *);
static int smbfs_readdir(struct vop_readdir_args *);
static int smbfs_bmap(struct vop_bmap_args *);
static int smbfs_strategy(struct vop_strategy_args *);
static int smbfs_print(struct vop_print_args *);
static int smbfs_pathconf(struct vop_pathconf_args *ap);
static int smbfs_advlock(struct vop_advlock_args *);
#ifdef APPLE
static int smbfs_blktooff(struct vop_blktooff_args *);
static int smbfs_offtoblk(struct vop_offtoblk_args *);
static int smbfs_pagein(struct vop_pagein_args *);
static int smbfs_pageout(struct vop_pageout_args *);
static int smbfs_lock(struct vop_lock_args *);
static int smbfs_unlock(struct vop_unlock_args *);
static int smbfs_islocked(struct vop_islocked_args *);
static int smbfs_getattrlist(struct vop_getattrlist_args *);
#else /* APPLE */
#ifndef FB_RELENG3
static int smbfs_getextattr(struct vop_getextattr_args *ap);
#endif
#endif /* APPLE */

vop_t **smbfs_vnodeop_p;
static struct vnodeopv_entry_desc smbfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) smbfs_access },
	{ &vop_advlock_desc,		(vop_t *) smbfs_advlock },
	{ &vop_bmap_desc,		(vop_t *) smbfs_bmap },
	{ &vop_close_desc,		(vop_t *) smbfs_close },
#ifdef APPLE
	{ &vop_create_desc,		(vop_t *) smbfs_create0 },
#else
	{ &vop_create_desc,		(vop_t *) smbfs_create },
#endif
	{ &vop_fsync_desc,		(vop_t *) smbfs_fsync },
	{ &vop_getattr_desc,		(vop_t *) smbfs_getattr },
#ifdef APPLE
	{ &vop_pagein_desc,		(vop_t *) smbfs_pagein },
#else /* APPLE */
	{ &vop_getpages_desc,		(vop_t *) smbfs_getpages },
#endif /* APPLE */
	{ &vop_inactive_desc,		(vop_t *) smbfs_inactive },
	{ &vop_ioctl_desc,		(vop_t *) smbfs_ioctl },
#ifdef APPLE
	{ &vop_islocked_desc,		(vop_t *) smbfs_islocked },
#else /* APPLE */
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
#endif /* APPLE */
	{ &vop_link_desc,		(vop_t *) smbfs_link },
#ifdef APPLE
	{ &vop_lock_desc,		(vop_t *) smbfs_lock },
#else /* APPLE */
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
#endif /* APPLE */
	{ &vop_lookup_desc,		(vop_t *) smbfs_lookup },
#ifdef APPLE
	{ &vop_mkdir_desc,		(vop_t *) smbfs_mkdir0 },
#else /* APPLE */
	{ &vop_mkdir_desc,		(vop_t *) smbfs_mkdir },
#endif /* APPLE */
	{ &vop_mknod_desc,		(vop_t *) smbfs_mknod },
	{ &vop_open_desc,		(vop_t *) smbfs_open },
	{ &vop_pathconf_desc,		(vop_t *) smbfs_pathconf },
	{ &vop_print_desc,		(vop_t *) smbfs_print },
#ifdef APPLE
	{ &vop_pageout_desc,		(vop_t *) smbfs_pageout },
#else /* APPLE */
	{ &vop_putpages_desc,		(vop_t *) smbfs_putpages },
#endif /* APPLE */
	{ &vop_read_desc,		(vop_t *) smbfs_read },
	{ &vop_readdir_desc,		(vop_t *) smbfs_readdir },
	{ &vop_reclaim_desc,		(vop_t *) smbfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) smbfs_remove },
	{ &vop_rename_desc,		(vop_t *) smbfs_rename },
	{ &vop_rmdir_desc,		(vop_t *) smbfs_rmdir },
	{ &vop_setattr_desc,		(vop_t *) smbfs_setattr },
	{ &vop_strategy_desc,		(vop_t *) smbfs_strategy },
	{ &vop_symlink_desc,		(vop_t *) smbfs_symlink },
#ifdef APPLE
	{ &vop_unlock_desc,		(vop_t *) smbfs_unlock },
#else /* APPLE */
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
#endif /* APPLE */
	{ &vop_write_desc,		(vop_t *) smbfs_write },
#ifdef APPLE
	{ &vop_abortop_desc,	(vop_t *) nop_abortop },
	{ &vop_searchfs_desc,	(vop_t *) err_searchfs },
	{ &vop_copyfile_desc,	(vop_t *) err_copyfile },
	{ &vop_getattrlist_desc, (vop_t *) smbfs_getattrlist},
	{ &vop_offtoblk_desc,	(vop_t *) smbfs_offtoblk },
	{ &vop_blktooff_desc,	(vop_t *) smbfs_blktooff },
	{ &vop_cmap_desc,	(vop_t *) err_cmap }, /* as in nfs */
#else
#ifndef FB_RELENG3
	{ &vop_getextattr_desc, 	(vop_t *) smbfs_getextattr },
/*	{ &vop_setextattr_desc,		(vop_t *) smbfs_setextattr },*/
#endif
#endif /* APPLE */
	{ NULL, NULL }
};

#ifdef APPLE
struct vnodeopv_desc smbfs_vnodeop_opv_desc =
#else
static struct vnodeopv_desc smbfs_vnodeop_opv_desc =
#endif /* APPLE */
	{ &smbfs_vnodeop_p, smbfs_vnodeop_entries };

VNODEOP_SET(smbfs_vnodeop_opv_desc);

#ifdef APPLE
int smbtraceindx = 0;
struct smbtracerec smbtracebuf[SMBTBUFSIZ] = {{0,0,0,0}};
uint smbtracemask = 0x00000000;
#endif /* APPLE */

/*
 * smbfs_down is called when either an smb_rq_simple or smb_t2_request call
 * has a request time out. It uses vfs_event_signal() to tell interested
 * parties the connection with the server is "down".
 * 
 * Note our UE Timeout is what triggers being here.  The operation
 * timeout may be much longer.  XXX If a connection has responded to
 * any request in the last UETIMEOUT seconds then we do not label it "down"
 * We probably need a different event & dialogue for the case of a
 * connection being responsive to at least one but not all operations.
 */
void
smbfs_down(struct smbmount *smp)
{
	if (!smp || (smp->sm_status & SM_STATUS_TIMEO))
		return;
	vfs_event_signal(&smp->sm_mp->mnt_stat.f_fsid, VQ_NOTRESP, 0);
	smp->sm_status |= SM_STATUS_TIMEO;
}

/*
 * smbfs_up is called when smb_rq_simple or smb_t2_request has successful
 * communication with a server. It uses vfs_event_signal() to tell interested
 * parties the connection is OK again if the connection was having problems.
 */
void
smbfs_up(struct smbmount *smp)
{
	if (!smp || !(smp->sm_status & SM_STATUS_TIMEO))
		return;
	smp->sm_status &= ~SM_STATUS_TIMEO;
	vfs_event_signal(&smp->sm_mp->mnt_stat.f_fsid, VQ_NOTRESP, 1);
}

void
smbfs_dead(struct smbmount *smp)
{
	if (!smp || (smp->sm_status & SM_STATUS_DEAD))
		return;
	vfs_event_signal(&smp->sm_mp->mnt_stat.f_fsid, VQ_DEAD, 0);
	smp->sm_status |= SM_STATUS_DEAD;
}

static int
smbfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ucred *cred = ap->a_cred;
	u_int mode = ap->a_mode;
	struct smbmount *smp = VTOSMBFS(vp);
	int error = 0;

	SMBVDEBUG("\n");
	if ((mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		    case VREG: case VDIR: case VLNK:
			return EROFS;
		    default:
			break;
		}
	}
	if (cred->cr_uid == 0)
		return 0;
	if (cred->cr_uid != smp->sm_args.uid) {
		mode >>= 3;
		if (!groupmember(smp->sm_args.gid, cred))
			mode >>= 3;
	}
	error = (((vp->v_type == VREG) ? smp->sm_args.file_mode : smp->sm_args.dir_mode) & mode) == mode ? 0 : EACCES;
	return error;
}

/* ARGSUSED */
static int
smbfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	struct vattr vattr;
	int mode = ap->a_mode;
	int error, accmode;
	int attrcacheupdated = 0;
	u_int16_t old_fid;

	SMBVDEBUG("%s,%d\n", np->n_name, np->n_opencount);
	if (vp->v_type != VREG && vp->v_type != VDIR) { 
		SMBFSERR("open eacces vtype=%d\n", vp->v_type);
		return EACCES;
	}
	if (vp->v_type == VDIR) {
		np->n_opencount++;
		return 0;
	}
	if (np->n_flag & NMODIFIED) {
		if ((error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
					     ap->a_p, 1)) == EINTR)
			return error;
		smbfs_attr_cacheremove(np);
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return error;
		np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return error;
		if (np->n_mtime.tv_sec != vattr.va_mtime.tv_sec) {
			error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
						ap->a_p, 1);
			if (error == EINTR)
				return error;
			np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
		}
	}
	/*
	 * Just do a read-only open if we're not opening for writing.
	 * If we are opening for writing, open read/write - and, if we
	 * already have it open read-only, close that open if the read/
	 * write open succeeds.
	 */
	if (np->n_opencount) {
		if (mode & FWRITE) {
			/*
			 * We're opening read/write, and we already have
			 * it open - do we have it open read/write?
			 */
			if ((np->n_rwstate & SMB_AM_OPENMODE) == SMB_AM_OPENRW) {
				/*
				 * Yes - just bump the open count.
				 */
				np->n_opencount++;
				return 0;
			}
		} else {
			/*
			 * We're opening read-only, and we already have
			 * it open either read-only or read/write;
			 * just bump the open count.
			 */
			np->n_opencount++;
			return 0;
		}
	}
	/*
	 * Use DENYNONE to give unixy semantics of permitting
	 * everything not forbidden by permissions.  Ie denial
	 * is up to server with clients/openers needing to use
	 * advisory locks for further control.
	 */
	accmode = SMB_SM_DENYNONE|SMB_AM_OPENREAD;
	/*
	 * If this is on a file system mounted read-only, we shouldn't
	 * get here with FWRITE set - the layers above us should've
	 * checked for that (e.g., by calling VOP_ACCESS, thus calling
	 * "smbfs_access()", which rejects write access to file systems
	 * mounted read-only), so we might be able to skip the check
	 * for MNT_RDONLY, but we'll be paranoid for now.
	 */
	if ((mode & FWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		accmode = SMB_SM_DENYNONE|SMB_AM_OPENRW;
	smb_makescred(&scred, ap->a_p, ap->a_cred);
	old_fid = np->n_fid;
	error = smbfs_smb_open(np, accmode, &scred, &attrcacheupdated);
	if (!error) {
		if (np->n_opencount != 0) {
			/*
			 * We already had it open (presumably because it
			 * was open read-only and we're now opening it
			 * read/write); close the old open.
			 * XXX - what if the close fails?
			 */
			smbfs_smb_close(np->n_mount->sm_share, old_fid, 
			    &np->n_mtime, &scred);
		}
		np->n_opencount++;
	}
	if (error || !attrcacheupdated) {
		/* remove this from the attr_cache if open could not
		 * update the existing cached entry
		 */
		smbfs_attr_cacheremove(np);
	}
	return error;
}

static int
smbfs_closel(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct proc *p = ap->a_p;
	struct smb_cred scred;
	int error;

	SMBVDEBUG("name=%s, pid=%d, c=%d\n",np->n_name, p->p_pid, np->n_opencount);

	smb_makescred(&scred, p, ap->a_cred);

	if (np->n_opencount == 0) {
#ifndef APPLE
		SMBERROR("Negative opencount\n");
#endif
		return 0;
	}
	if (vp->v_type == VDIR) {
		if (--np->n_opencount)
			return 0;
		if (np->n_dirseq) {
			smbfs_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}
		error = 0;
	} else {
		error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred, p, 1);
		if (--np->n_opencount)
			return error;
		/*
		 * XXX - if we have the file open for reading and writing,
		 * and we're closing the last open for writing, it'd be
		 * nice if we could open for reading and close for reading
		 * and writing, so we give up our write access and stop
		 * blocking other clients from doing deny-write opens.
		 */
#ifdef APPLE
		/*
		 * VOP_GETATTR removed due to ubc_invalidate panic when ubc
		 * already torn down.  VOP_GETATTR here made no sense anyway.
		 * The smbfs_smb_close() doesn't use them and then the
		 * smbfs_attr_cacheremove() invalidates them.  Nor do
		 * server side effects explain this, as the getattr will
		 * often hit the cache, sending no rpcs.
		 */
		if (ubc_isinuse(vp, 1)) {
			np->n_opencount = 1;	/* wait for inactive to close */
			if (UBCINFOEXISTS(vp) && ubc_issetflags(vp, UI_WASMAPPED)) {
				/* set the "do not cache" bit so that inactive gets 
				 * called immediately after the last mmap reference
				 * is gone
				 */
				ubc_uncache(vp);
			}
		} else
#endif /* APPLE */
		{
			error = smbfs_smb_close(np->n_mount->sm_share, np->n_fid, 
			   &np->n_mtime, &scred);
		}
	}
	if (np->n_flag & NATTRCHANGED)
		smbfs_attr_cacheremove(np);
	return error;
}

/*
 * XXX: VOP_CLOSE() usually called without lock held which is bad.  Here we
 * do some heruistic to determine if vnode should be locked.
 */
static int
smbfs_close(ap)
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	int error, dolock;

	VI_LOCK(vp);
	dolock = (vp->v_flag & VXLOCK) == 0;
	if (dolock)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY | LK_INTERLOCK, p);
	else
		VI_UNLOCK(vp);
	error = smbfs_closel(ap);
	if (dolock)
		VOP_UNLOCK(vp, 0, p);
	return error;
}

/*
 * smbfs_getattr call from vfs.
 */
static int
smbfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np;
	u_long vid;
	struct vattr *va=ap->a_vap;
	struct smbfattr fattr;
	struct smb_cred scred;
	int error;

	if (SMB_STALEVP(vp, 0))
		return (EIO);
	vid = vp->v_id;
	np = VTOSMB(vp);
	SMBVDEBUG("%lx: '%s' %d\n", (long)vp, np->n_name, (vp->v_flag & VROOT) != 0);
	error = smbfs_attr_cachelookup(vp, va);
	if (!error)
		return 0;
	SMBVDEBUG("not in the cache\n");
	smb_makescred(&scred, ap->a_p, ap->a_cred);
	error = smbfs_smb_lookup(np, NULL, NULL, &fattr, &scred);
	if (error) {
		SMBVDEBUG("error %d\n", error);
		return error;
	}
	/*
	 * we may have lost the vnode during the lookup.  we return EIO,
	 * but if we knew whether this was a stat (vs fstat) it is arguably
	 * better to return ENOENT (or EBADF).
	 */
	if (SMB_STALEVP(vp, vid))
		return (EIO);
	smbfs_attr_cacheenter(vp, &fattr);
	smbfs_attr_cachelookup(vp, va);
	return 0;
}

static int
smbfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct vattr *vap = ap->a_vap;
	struct timespec *mtime, *atime;
	struct smb_cred scred;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	u_quad_t tsize = 0;
	int isreadonly, doclose, error = 0, savefid, saverwstate;
	int attrcacheupdated;
#ifdef APPLE
	off_t newround;
#endif

	SMBVDEBUG("\n");
	if (vap->va_flags != (u_long)VNOVAL)
		return EOPNOTSUPP;
	isreadonly = (vp->v_mount->mnt_flag & MNT_RDONLY);
	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL || 
	     vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	     vap->va_mode != (mode_t)VNOVAL) && isreadonly)
		return EROFS;
	smb_makescred(&scred, ap->a_p, ap->a_cred);
	if (vap->va_size != (u_quad_t)VNOVAL) {
 		switch (vp->v_type) {
 		    case VDIR:
 			return EISDIR;
 		    case VREG:
			break;
 		    default:
			return EINVAL;
  		};
		if (isreadonly)
			return EROFS;
		doclose = 0;
 		tsize = np->n_size;
#ifdef APPLE
		newround = round_page_64((off_t)vap->va_size);
		if ((off_t)tsize > newround) {
			if (!ubc_invalidate(vp, newround,
					    (size_t)(tsize - newround)))
				panic("smbfs_setattr: ubc_invalidate");
		}
#endif /* APPLE */
		/*
		 * XXX VM coherence on extends -  consider delaying
		 * this until after zero fill (smbfs_0extend)
		 */
		smbfs_setsize(vp, (off_t)vap->va_size);

		savefid = -1;
		saverwstate = np->n_rwstate;
		if (np->n_opencount == 0 ||
		    (np->n_rwstate & SMB_AM_OPENMODE) == SMB_AM_OPENREAD) {
			/*
			 * We don't have an open FID for it, or we have
			 * one but it's open read-only, not read/write;
			 * we'll need a read/write FID to set the size.
			 *
			 * XXX - if we're using a dialect with
			 * a "set path info" call, we could use it.
			 */
			if (np->n_opencount != 0) {
				/*
				 * Save the existing read-only FID,
				 * and put it back when we're done;
				 * "smbfs_smb_open()" will overwrite
				 * it.
				 *
				 * The vnode is locked, so that's safe.
				 */
				savefid = np->n_fid;
			}
			error = smbfs_smb_open(np,
					       SMB_SM_DENYNONE|SMB_AM_OPENRW,
					       &scred, &attrcacheupdated);
			if (error == 0)
				doclose = 1;
		}
		if (error == 0)
			error = smbfs_smb_setfsize(np, vap->va_size, &scred);
#ifdef APPLE
		if (!error && tsize < vap->va_size)
			error = smbfs_0extend(vp, tsize, vap->va_size, &scred,
					      ap->a_p, SMBWRTTIMO);
#endif /* APPLE */
		if (doclose) {
			/*
			 * Close the new FID.
			 */
			smbfs_smb_close(ssp, np->n_fid, NULL, &scred);
			if (savefid > 0) {
				/*
				 * Put back the saved FID, and the
				 * saved "granted mode" flags.
				 */
				np->n_fid = savefid;
				np->n_rwstate = saverwstate;
			}
		}
		if (error) {
			smbfs_setsize(vp, (off_t)tsize);
			return error;
		} else 	 {
			/* if success, blow away statfs cache */
			smp->sm_statfstime = 0;
		}
  	}
	mtime = atime = NULL;
	if (vap->va_mtime.tv_sec != VNOVAL)
		mtime = &vap->va_mtime;
	if (vap->va_atime.tv_sec != VNOVAL)
		atime = &vap->va_atime;
	if (mtime != atime) {
#if 0
		if (mtime == NULL)
			mtime = &np->n_mtime;
		if (atime == NULL)
			atime = &np->n_atime;
#endif
		/*
		 * If file is opened for writing, then we can use handle-based
		 * calls.
		 * If not, use path-based ones.
		 */
		if (np->n_opencount == 0 ||
		    (np->n_rwstate & SMB_AM_OPENMODE) == SMB_AM_OPENREAD) {
			if (vcp->vc_flags & SMBV_WIN95) {
				error = VOP_OPEN(vp, FWRITE, ap->a_cred,
						 ap->a_p);
				if (!error) {
/*
					error = smbfs_smb_setfattrNT(np, 0,
							mtime, atime, &scred);
					VOP_GETATTR(vp, &vattr, ap->a_cred,
						    ap->a_p);
*/
					if (mtime)
						np->n_mtime = *mtime;
					VOP_CLOSE(vp, FWRITE, ap->a_cred, ap->a_p);
				}
			} else if ((vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)) {
				error = smbfs_smb_setptime2(np, mtime, atime, 0, &scred);
/*				error = smbfs_smb_setpattrNT(np, 0, mtime, atime, &scred);*/
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) {
				error = smbfs_smb_setptime2(np, mtime, atime, 0, &scred);
			} else {
				error = smbfs_smb_setpattr(np, NULL, 0, 0, mtime, &scred);
			}
		} else {
			if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
				error = smbfs_smb_setfattrNT(np, 0, mtime, atime, &scred);
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN1_0) {
				error = smbfs_smb_setftime(np, mtime, atime, &scred);
			} else {
				/*
				 * I have no idea how to handle this for core
				 * level servers. The possible solution is to
				 * update mtime when the FID is closed, as
				 * that can be set in an SMB close operation.
				 *
				 * Or should we us "smbfs_smb_setpattr()"?
				 * That uses Set File Attributes, which is
				 * in the core protocol.
				 */
				SMBERROR("can't update times on an opened file\n");
			}
		}
	}
	/*
	 * Invalidate attribute cache in case if server doesn't set
	 * required attributes.
	 */
	smbfs_attr_cacheremove(np);	/* invalidate cache */
	VOP_GETATTR(vp, vap, ap->a_cred, ap->a_p);
	np->n_mtime.tv_sec = vap->va_mtime.tv_sec;
	return error;
}

#ifdef APPLE
static int
smb_flushvp(struct vnode *vp, struct proc *p, struct ucred *cred, int inval)
{
	struct smb_cred scred;

	/* XXX log ubc_clean errors and provide nowait option */
	(void)ubc_clean(vp, inval == 1);
	(void)ubc_clean(vp, inval == 1); /* stalls us until pageouts complete */
	smb_makescred(&scred, p, cred);
	return (smbfs_smb_flush(VTOSMB(vp), &scred));
}


static int
smb_flushrange(struct vnode *vp, struct uio *uio)
{
	off_t soff, eoff;

	soff = trunc_page_64(uio->uio_offset);
	eoff = round_page_64(uio->uio_offset + uio->uio_resid);
	if (!ubc_pushdirty_range(vp, soff, (off_t)(eoff - soff)))
		return (EINVAL);
	if (!ubc_invalidate(vp, soff, (size_t)(eoff - soff)))
		return (EINVAL);
	return (0);
}
#endif /* APPLE */

/*
 * smbfs_read call.
 */
static int
smbfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
#ifdef APPLE
	off_t soff, eoff;
	upl_t upl;
	int error, remaining, xfersize;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
#endif /* APPLE */

	SMBVDEBUG("\n");
	if (vp->v_type != VREG && vp->v_type != VDIR)
		return (EPERM);
#ifdef APPLE
	/*
 	 * FreeBSD vs Darwin VFS difference; we can get VOP_READ without
 	 * preceeding VOP_OPEN via the exec path, so do it implicitly.
	 *
	 * XXX - when is the implied open closed?
 	 */
 	if (np->n_opencount == 0) {
 		error = VOP_OPEN(vp, FREAD, ap->a_cred, uio->uio_procp);
 		if (error)
 			return (error);
 	}
	/*
	 * Here we push any mmap-dirtied pages.
	 * The vnode lock is held, so we shouldn't need to lock down
	 * all pages across the whole vop.
	 */
	error = smb_flushrange(vp, uio);
 	if (error)
 		return (error);
	smb_makescred(&scred, uio->uio_procp, ap->a_cred);
	error = smbfs_smb_flush(np, &scred);
 	if (error)
 		return (error);
	/*
	 * In order to maintain some synchronisation 
	 * between memory-mapped access and reads from 
	 * a file, we build a upl covering the range
	 * we're about to read, and once the read
	 * completes, dump all the pages.
	 *
	 * Loop reading chunks small enough to be covered by a upl.
	 */
	while (!error && uio->uio_resid > 0) {
		remaining = uio->uio_resid;
		xfersize = MIN(remaining, MAXPHYS);
		/* create a upl for this range */
		soff = trunc_page_64(uio->uio_offset);
		eoff = round_page_64(uio->uio_offset + xfersize);
		error = ubc_create_upl(vp, soff, (long)(eoff - soff), &upl,
				       NULL, 0);
		if (error)
			break;
		uio->uio_resid = xfersize;
		/* do the wire transaction */
		error = smbfs_readvnode(vp, uio, ap->a_cred);
		/* dump the pages */
		if (ubc_upl_abort(upl, UPL_ABORT_DUMP_PAGES))
			panic("smbfs_read: ubc_upl_abort");
		uio->uio_resid += remaining - xfersize; /* restore true resid */
		if (uio->uio_resid == remaining) /* nothing transferred? */
			break;
	}
	return (error);
#else
 	return smbfs_readvnode(vp, uio, ap->a_cred);
#endif /* APPLE */
}

static int
smbfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbmount *smp = VTOSMBFS(vp);
	struct uio *uio = ap->a_uio;
#ifdef APPLE
	off_t soff, eoff;
	upl_t upl;
	int error, remaining, xfersize;
	struct smbnode *np = VTOSMB(vp);
#endif /* APPLE */
	int timo = SMBWRTTIMO;

	SMBVDEBUG("%d,ofs=%d,sz=%d\n",vp->v_type, (int)uio->uio_offset, uio->uio_resid);
	if (vp->v_type != VREG)
		return (EPERM);
#ifdef APPLE
	/*
 	 * Potential FreeBSD vs Darwin VFS difference: we are not ensured a
 	 * VOP_OPEN, so do it implicitly.  The log message is because we
	 * anticipate only READ VOPs before OPEN.  If this becomes
	 * an expected code path then remove the "log".
	 *
	 * We also force an open for writing if it's not already open
	 * for writing.
	 *
	 * XXX - when is the implied open closed?
 	 */
 	if (np->n_opencount == 0 ||
	    (np->n_rwstate & SMB_AM_OPENMODE) != SMB_AM_OPENRW) {
		log(LOG_NOTICE, "smbfs_write: implied open: opencount %d, rwstate 0x%x\n",
		    np->n_opencount, np->n_rwstate);
 		error = VOP_OPEN(vp, FWRITE, ap->a_cred, uio->uio_procp);
 		if (error)
 			return (error);
 	}
	/*
	 * Here we push any mmap-dirtied pages.
	 * The vnode lock is held, so we shouldn't need to lock down
	 * all pages across the whole vop.
	 */
	error = smb_flushrange(vp, uio);
	/*
	 * Note that since our lower layers take the uio directly,
	 * we don't copy it into these pages; we're going to 
	 * invalidate them all when we're done anyway.
	 *
	 * Loop writing chunks small enough to be covered by a upl.
	 */
	while (!error && uio->uio_resid > 0) {
		remaining = uio->uio_resid;
		xfersize = MIN(remaining, MAXPHYS);
		/* create a upl for this range */
		soff = trunc_page_64(uio->uio_offset);
		eoff = round_page_64(uio->uio_offset + xfersize);
		error = ubc_create_upl(vp, soff, (long)(eoff - soff), &upl,
				       NULL, 0);
		if (error)
			break;
		uio->uio_resid = xfersize;
		/* do the wire transaction */
		error = smbfs_writevnode(vp, uio, ap->a_cred, ap->a_ioflag,
					 timo);
		timo = 0;
		/* dump the pages */
		if (ubc_upl_abort(upl, UPL_ABORT_DUMP_PAGES))
			panic("smbfs_write: ubc_upl_abort");
		uio->uio_resid += remaining - xfersize; /* restore true resid */
		if (uio->uio_resid == remaining) /* nothing transferred? */
			break;
	}
#else
 	error = smbfs_writevnode(vp, uio, ap->a_cred,ap->a_ioflag, timo);
#endif /* APPLE */
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return (error);
}

/*
 * smbfs_create call
 * Create a regular file. On entry the directory to contain the file being
 * created is locked.  We must release before we return. We must also free
 * the pathname buffer pointed at by cnp->cn_pnbuf, always on error, or
 * only if the SAVESTART bit in cn_flags is clear on success.
 */
static int
smbfs_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp=ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbmount *smp = VTOSMBFS(dvp);
	struct vnode *vp;
	struct vattr vattr;
	struct smbfattr fattr;
	struct smb_cred scred;
	const char *name = cnp->cn_nameptr;
	int nmlen = cnp->cn_namelen;
	int error;
	

	SMBVDEBUG("\n");
	*vpp = NULL;
	if (vap->va_type != VREG)
		return EOPNOTSUPP;
	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc)))
		return error;
	smb_makescred(&scred, cnp->cn_proc, cnp->cn_cred);
	
	error = smbfs_smb_create(dnp, name, nmlen, &scred);
	if (error)
		return error;
	error = smbfs_smb_lookup(dnp, &name, &nmlen, &fattr, &scred);
	if (error)
		return error;
	smbfs_attr_touchdir(dnp);
	error = smbfs_nget(VTOVFS(dvp), dvp, name, nmlen, &fattr, &vp);
	if (error)
		goto bad;
	*vpp = vp;
	if (cnp->cn_flags & MAKEENTRY && name == cnp->cn_nameptr) {
#ifdef APPLE
		if (cnp->cn_namelen <=  NCHNAMLEN) /* X namecache limitation */
#endif
			cache_enter(dvp, vp, cnp);
	} else if (cnp->cn_flags & MAKEENTRY) {
		struct componentname cn = *cnp;

		cn.cn_nameptr = (char *)name;
		cn.cn_namelen = nmlen;
#ifdef APPLE
		{
			int indx;
			char *cp;

			cn.cn_hash = 0;
			for (cp = cn.cn_nameptr, indx = 1;
			     *cp != 0 && *cp != '/'; indx++, cp++)
				cn.cn_hash += (unsigned char)*cp * indx;
		}
		if (cn.cn_namelen <=  NCHNAMLEN) /* X namecache limitation */
#endif
			cache_enter(dvp, vp, &cn);
	}
	error = 0;
bad:
	if (name != cnp->cn_nameptr)
		smbfs_name_free(name);
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return error;
}

#ifdef APPLE
static int
smbfs_create0(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap; 
{
	int error;

	error = smbfs_create(ap);
	nop_create(ap);
	return (error);
}
#endif /* APPLE */


static int
smbfs_remove(ap)
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *dnp = VTOSMB(ap->a_dvp);
	struct componentname *cnp = ap->a_cnp;
	struct proc *p = cnp->cn_proc;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_cred scred;
	int error;

#ifdef APPLE
	/*
	 * Paranoia (VDIR is screened off by _unlink in system call layer)
	 */
	if (vp->v_type == VDIR) {
		error = EPERM;
		goto out;
	}
	if (UBCISVALID(vp) ? ubc_isinuse(vp, 1) : vp->v_usecount != 1) {
		error = EBUSY;
		goto out;
	}
	if (!ubc_invalidate(vp, (off_t)0, (size_t)np->n_size))
		panic("smbfs_remove: ubc_invalidate");
	smbfs_setsize(vp, (off_t)0);
	smb_makescred(&scred, p, cnp->cn_cred);
	if (np->n_opencount) {
 		if (np->n_opencount > 1)
			log(LOG_WARNING, "smbfs_remove: n_opencount=%d\n",
			    np->n_opencount);
		np->n_opencount = 0;
		error = smbfs_smb_close(np->n_mount->sm_share, np->n_fid, 
					&np->n_mtime, &scred);
 		if (error)
			log(LOG_WARNING, "smbfs_remove: close error=%d\n",
			    error);
	}
#else
	/* freebsd bug: as in smbfs_rename EBUSY better for open file case */
	if (vp->v_type == VDIR || np->n_opencount || vp->v_usecount != 1)
		return EPERM;
	smb_makescred(&scred, p, cnp->cn_cred);
#endif /* APPLE */
	error = smbfs_smb_delete(np, &scred);
	smb_vhashrem(np, p);
	smbfs_attr_touchdir(dnp);
	cache_purge(vp);
#ifdef APPLE
	/* XXX Q4BP: Ought above cache_purge preceed rpc as in nfs? */
out:
	if (error == EBUSY)
		SMBERROR("warning: pid %d(%.*s) unlink open file(%.*s)\n",
			 p->p_pid, sizeof(p->p_comm), p->p_comm,
			 np->n_nmlen, np->n_name);
	if (ap->a_dvp != vp)
		VOP_UNLOCK(vp, 0, p);
	/* XXX ufs calls ubc_uncache even for errors, but we follow hfs precedent */
	if (!error)
		(void) ubc_uncache(vp);
	vrele(vp);
	vput(ap->a_dvp);
#endif /* APPLE */
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return error;
}

/*
 * smbfs_file rename call
 */
static int
smbfs_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct smbmount *smp = VTOSMBFS(fvp);
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct proc *p = tcnp->cn_proc;
	struct smb_cred scred;
	u_int16_t flags = 6;
	int error=0;
	int hiderr;
	struct smbnode *fnp = VTOSMB(fvp);
	struct smbnode *tnp = tvp ? VTOSMB(tvp) : NULL;
	struct smbnode *tdnp = VTOSMB(tdvp);
	struct smbnode *fdnp = VTOSMB(fdvp);

	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
#ifdef APPLE
out:
		if (error == EBUSY)
			SMBERROR("warning: pid %d(%.*s) rename open file(%.*s)\n",
				 p->p_pid, sizeof(p->p_comm), p->p_comm,
				 fnp->n_nmlen, fnp->n_name);
		nop_rename(ap);
		return (error);
#endif /* APPLE */
	}

	/*
	 * Since there are no hard links (from our client point of view)
	 * fvp==tvp means the arguments are case-variants.  (If they
	 * were identical the rename syscall doesnt call us.)
	 */
	if (tvp && tvp != fvp &&
#ifdef APPLE
	    (UBCISVALID(tvp) ? ubc_isinuse(tvp, 1) : tvp->v_usecount > 1)) {
#else
	    tvp->v_usecount > 1) {
#endif /* APPLE */
		error = EBUSY;
		goto out;
	}
	flags = 0x10;			/* verify all writes */
	if (fvp->v_type == VDIR) {
		flags |= 2;
	} else if (fvp->v_type == VREG) {
		flags |= 1;
	} else {
		error = EINVAL;
		goto out;
	}
	smb_makescred(&scred, p, tcnp->cn_cred);
#ifdef notnow
	/*
	 * Samba doesn't implement SMB_COM_MOVE call...
	 */
	if (SMB_DIALECT(SSTOCN(smp->sm_share)) >= SMB_DIALECT_LANMAN1_0) {
		error = smbfs_smb_move(fnp, tdnp, tcnp->cn_nameptr,
				       tcnp->cn_namelen, flags, &scred);
	} else
#endif
	{
		/*
		 * vnode lock gives local atomicity for delete+rename
		 * distributed atomicity XXX
		 */
		if (tvp && tvp != fvp) {
			cache_purge(tvp);
			error = smbfs_smb_delete(tnp, &scred);
			if (error)
				goto out;
			smb_vhashrem(tnp, p);
		}
		cache_purge(fvp);
		error = smbfs_smb_rename(fnp, tdnp, tcnp->cn_nameptr,
					 tcnp->cn_namelen, &scred);
#ifdef APPLE
		/*
		 * XXX
		 * If file is open and server *allowed* the rename we should
		 * alter n_name (or entire node) so that reconnections
		 * would use the correct name.
		 */
		if (error && fnp->n_opencount) {
			if (UBCISVALID(fvp) ? ubc_isinuse(fvp, 1)
					    : fvp->v_usecount > 1) {
				error = EBUSY;
				goto out;
			}
 			if (fnp->n_opencount > 1) /* paranoia */
				log(LOG_WARNING,
				    "smbfs_rename: n_opencount=%d!\n",
			 	    fnp->n_opencount);
			error = smb_flushvp(fvp, p, tcnp->cn_cred,
					    1); /* with invalidate */
 			if (error)
				log(LOG_WARNING,
				    "smbfs_rename: flush error=%d\n", error);
			fnp->n_opencount = 0;
			error = smbfs_smb_close(VTOSMBFS(fvp)->sm_share,
						fnp->n_fid, &fnp->n_mtime,
						&scred);
 			if (error)
				log(LOG_WARNING,
				    "smbfs_rename: close error=%d\n", error);
			error = smbfs_smb_rename(fnp, tdnp,
	 					 tcnp->cn_nameptr,
	 					 tcnp->cn_namelen, &scred);
		}
#endif /* APPLE */
	}
	if (!error)
		smb_vhashrem(fnp, p);
#ifdef APPLE
	/*
	 *	Source			Target
	 *	Dot	Hidden		Dot	HIDE
	 *	Dot	Unhidden	Dot	HIDE! (Puma recovery)
	 *	NoDot	Hidden		Dot	HIDE (Win hid it)
	 *	NoDot	Unhidden	Dot	HIDE
	 *	Dot	Hidden		NoDot	UNHIDE
	 *	Dot	Unhidden	NoDot	UNHIDE
	 *	NoDot	Hidden		NoDot	HIDE! (Win hid it)
	 *	NoDot	Unhidden	NoDot	UNHIDE
	 */
	if (!error && tcnp->cn_nameptr[0] == '.') {
		if ((hiderr = smbfs_smb_hideit(tdnp, tcnp->cn_nameptr,
					       tcnp->cn_namelen, &scred)))
			SMBERROR("hiderr %d", hiderr);
	} else if (!error && tcnp->cn_nameptr[0] != '.' &&
		   fcnp->cn_nameptr[0] == '.') {
		if ((hiderr = smbfs_smb_unhideit(tdnp, tcnp->cn_nameptr,
					       tcnp->cn_namelen, &scred)))
			SMBERROR("(un)hiderr %d", hiderr);
	}
#endif /* APPLE */

	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
#ifndef APPLE
out:
#endif /* APPLE */
	if (error == EBUSY)
		SMBERROR("warning: pid %d(%.*s) rename open file(%.*s)\n",
			 p->p_pid, sizeof(p->p_comm), p->p_comm,
			 fnp->n_nmlen, fnp->n_name);
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	smbfs_attr_touchdir(fdnp);
	if (tdvp != fdvp)
		smbfs_attr_touchdir(tdnp);
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return error;
}

/*
 * sometime it will come true...
 */
static int
smbfs_link(ap)
	struct vop_link_args /* {
		struct vnode *a_vp;
		struct vnode *a_tdvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct proc *p = ap->a_cnp->cn_proc;
	struct smbnode *np = VTOSMB(ap->a_vp);

	SMBERROR("warning: pid %d(%.*s) hardlink(%.*s)\n",
		 p->p_pid, sizeof(p->p_comm), p->p_comm,
		 np->n_nmlen, np->n_name);
#ifdef APPLE
	return (err_link(ap));
#else
	return EOPNOTSUPP;
#endif /* APPLE */
}

/*
 * smbfs_symlink link create call.
 */
static int
smbfs_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	struct proc *p = ap->a_cnp->cn_proc;

	SMBERROR("warning: pid %d(%.*s) symlink(%.*s)\n",
		 p->p_pid, sizeof(p->p_comm), p->p_comm,
		 ap->a_cnp->cn_namelen, ap->a_cnp->cn_nameptr);
#ifdef APPLE
	return (err_symlink(ap));
#else
	return EOPNOTSUPP;
#endif /* APPLE */
}

static int
smbfs_mknod(ap) 
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct proc *p = ap->a_cnp->cn_proc;

	SMBERROR("warning: pid %d(%.*s) mknod(%.*s)\n",
		 p->p_pid, sizeof(p->p_comm), p->p_comm,
		 ap->a_cnp->cn_namelen, ap->a_cnp->cn_nameptr);
#ifdef APPLE
	return (err_mknod(ap));
#else
	return EOPNOTSUPP;
#endif /* APPLE */
}

static int
smbfs_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
/*	struct vattr *vap = ap->a_vap;*/
	struct vnode *vp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbmount *smp = VTOSMBFS(dvp);
	struct vattr vattr;
	struct smb_cred scred;
	struct smbfattr fattr;
	const char *name = cnp->cn_nameptr;
	int len = cnp->cn_namelen;
	int error, hiderr;

	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc))) {
		return error;
	}	
	if (name[0] == '.' && (len == 1 || (len == 2 && name[1] == '.')))
		return EEXIST;
	smb_makescred(&scred, cnp->cn_proc, cnp->cn_cred);
	error = smbfs_smb_mkdir(dnp, name, len, &scred);
	if (error)
		return error;
	error = smbfs_smb_lookup(dnp, &name, &len, &fattr, &scred);
	if (error)
		return error;
	smbfs_attr_touchdir(dnp);
	error = smbfs_nget(VTOVFS(dvp), dvp, name, len, &fattr, &vp);
	if (error)
		goto bad;
#ifdef APPLE
	if (name[0] == '.')
		if ((hiderr = smbfs_smb_hideit(VTOSMB(vp), NULL, 0, &scred)))
			SMBERROR("hiderr %d", hiderr);
#endif
	*ap->a_vpp = vp;
bad:
	if (name != cnp->cn_nameptr)
		smbfs_name_free(name);
	/* if success, blow away statfs cache */
	smp->sm_statfstime = 0;
	return (error);
}

#ifdef APPLE
static int
smbfs_mkdir0(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	int error;

	error = smbfs_mkdir(ap);
	nop_mkdir(ap);
	return (error);
}
#endif /* APPLE */


/*
 * smbfs_remove directory call
 */
static int
smbfs_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	if (dvp == vp)
#ifdef APPLE
	/* XXX other OSX fs test fs nodes here, not vnodes. Why? */
	{
		error = EINVAL;
		vrele(dvp);
		goto bad;
	}
#else
		return EINVAL;
#endif /* APPLE */

	smb_makescred(&scred, cnp->cn_proc, cnp->cn_cred);
	error = smbfs_smb_rmdir(np, &scred);
#ifdef APPLE
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF) {
		FREE_ZONE(ap->a_cnp->cn_pnbuf, ap->a_cnp->cn_pnlen, M_NAMEI);
		ap->a_cnp->cn_flags &= ~HASBUF;
	}
#endif /* APPLE */
	dnp->n_flag |= NMODIFIED;
	smbfs_attr_touchdir(dnp);
	/* XXX Q4BP: nfs purges dvp.  Why was this commented out? */
/*	cache_purge(dvp); */
	cache_purge(vp);
	smb_vhashrem(np, cnp->cn_proc);
#ifdef APPLE
	vput(dvp);
bad:
	vput(vp);
#endif /* APPLE */
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;
	return error;
}

/*
 * smbfs_readdir call
 */
static int
smbfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int error;

	if (vp->v_type != VDIR)
		return (EPERM);
#ifdef notnow
	if (ap->a_ncookies) {
		printf("smbfs_readdir: no support for cookies now...");
		return (EOPNOTSUPP);
	}
#endif
	error = smbfs_readvnode(vp, uio, ap->a_cred);
	return error;
}

/* ARGSUSED */
static int
smbfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_vp;
		struct ucred * a_cred;
		int  a_waitfor;
		struct proc * a_p;
	} */ *ap;
{
#ifdef APPLE
	int error;

	error = smb_flushvp(ap->a_vp, ap->a_p, ap->a_cred, 0);
	if (!error)
		VTOSMBFS(ap->a_vp)->sm_statfstime = 0;
	nop_fsync(ap);
	return (error);
#else
/*	return (smb_flush(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_p, 1));*/
	return (0);
#endif /* APPLE */
}

static 
int smbfs_print (ap) 
	struct vop_print_args /* {
	struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);

	if (np == NULL) {
		printf("no smbnode data\n");
		return (0);
	}
	printf("tag VT_SMBFS, name = %s, parent = %p, opencount = %d",
	    np->n_name, np->n_parent ? SMBTOV(np->n_parent) : NULL,
	    np->n_opencount);
	lockmgr_printinfo(&np->n_lock);
	printf("\n");
	return (0);
}

static int
smbfs_pathconf (ap)
	struct vop_pathconf_args  /* {
	struct vnode *vp;
	int name;
	register_t *retval;
	} */ *ap;
{
	struct smbmount *smp = VFSTOSMBFS(VTOVFS(ap->a_vp));
	struct smb_vc *vcp = SSTOVC(smp->sm_share);
	register_t *retval = ap->a_retval;
	int error = 0;
	
	switch (ap->a_name) {
	    case _PC_LINK_MAX:
		*retval = 0;
		break;
	    case _PC_NAME_MAX:
		*retval = (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) ?
			  255 : 12;
		break;
	    case _PC_PATH_MAX:
		*retval = 800;	/* XXX: a correct one ? */
		break;
	    default:
		error = EINVAL;
	}
	return error;
}

static int
smbfs_strategy (ap) 
	struct vop_strategy_args /* {
	struct buf *a_bp
	} */ *ap;
{
	struct buf *bp=ap->a_bp;
	struct ucred *cr;
	struct proc *p;
	int error = 0;

	SMBVDEBUG("\n");
	if (bp->b_flags & B_PHYS)
		panic("smbfs physio");
	if (bp->b_flags & B_ASYNC)
		p = (struct proc *)0;
	else
		p = curproc;	/* XXX */
	if (bp->b_flags & B_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;

	if ((bp->b_flags & B_ASYNC) == 0 )
		error = smbfs_doio(bp, cr, p);
	return error;
}

static int
smbfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
#ifdef APPLE
		*ap->a_bnp = ap->a_bn * btodb(vp->v_mount->mnt_stat.f_iosize,
					      DEV_BSIZE);
#else
		*ap->a_bnp = ap->a_bn * btodb(vp->v_mount->mnt_stat.f_iosize);
#endif /* APPLE */
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
#ifndef APPLE
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
#endif /* APPLE */
	return (0);
}

int
smbfs_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t a_data;
		int fflag;
		struct ucred *cred;
		struct proc *p;
	} */ *ap;
{
	#pragma unused(ap)
	return EINVAL;
}

#ifndef APPLE
static char smbfs_atl[] = "rhsvda";
static int
smbfs_getextattr(struct vop_getextattr_args *ap)
/* {
	IN struct vnode *a_vp;
	IN char *a_name;
	INOUT struct uio *a_uio;
	IN struct ucred *a_cred;
	IN struct proc *a_p;
};
*/
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	struct ucred *cred = ap->a_cred;
	struct uio *uio = ap->a_uio;
	const char *name = ap->a_name;
	struct smbnode *np = VTOSMB(vp);
	struct vattr vattr;
	char buf[10];
	int i, attr, error;

	error = VOP_ACCESS(vp, VREAD, cred, p);
	if (error)
		return error;
	error = VOP_GETATTR(vp, &vattr, cred, p);
	if (error)
		return error;
	if (strcmp(name, "dosattr") == 0) {
		attr = np->n_dosattr;
		for (i = 0; i < 6; i++, attr >>= 1)
			buf[i] = (attr & 1) ? smbfs_atl[i] : '-';
		buf[i] = 0;
		error = uiomove(buf, i, uio);
		
	} else
		error = EINVAL;
	return error;
}
#endif /* APPLE */

/* SMB locks do not map to POSIX.1 advisory locks in several ways:
 * 1 - SMB provides no way to find an existing lock on the server.
 *     So, the F_GETLK operation can only report locks created by processes
 *     on this client. 
 * 2 - SMB locks cannot overlap an existing locked region of a file. So,
 *     F_SETLK/F_SETLKW operations that establish locks cannot extend an
 *     existing lock.
 * 3 - When unlocking a SMB locked region, the region to unlock must correspond
 *     exactly to an existing locked region. So, F_SETLK F_UNLCK operations
 *     cannot split an existing lock or unlock more than was locked (this is
 *     especially important because file files are closed, we recieve a request
 *     to unlock the entire file: l_whence and l_start point to the beginning
 *     of the file, and l_len is zero).
 * The result... SMB cannot support POSIX.1 advisory locks. It can however
 * support BSD flock() locks, so that's what this implementation will allow. 
 */
int
smbfs_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	caddr_t id = ap->a_id;
	int operation = ap->a_op;
	struct flock *fl = ap->a_fl;
	int flags = ap->a_flags;
	struct smbmount *smp = VFSTOSMBFS(vp->v_mount);
	struct smb_share *ssp = smp->sm_share;
	struct smbnode *np = VTOSMB(vp);
	struct proc *p = curproc;
 	struct smb_cred scred;
	off_t start, end;
	u_int64_t len;
	struct smbfs_lockf *lock;
	int error;
	/* Since the pid passed to the SMB server is only 16 bits and a_id
	 * is 32 bits, and since we are negotiating locks between local processes
	 * with the code in smbfs_lockf.c, just pass a 1 for our pid to the server.
	 */
	caddr_t smbid = (caddr_t)1;
	int largelock = (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES) != 0;
	u_int32_t timeout;
	
	/* Only regular files can have locks */
	if (vp->v_type != VREG)
		return (EISDIR);
	
	/* No support for F_POSIX style locks */
	if (flags & F_POSIX)
		return (err_advlock(ap));
    
	/*
	 * Avoid the common case of unlocking when smbnode has no locks.
	 */
	if (np->smb_lockf == (struct smbfs_lockf *)0) {
		if (operation != F_SETLK) {
			fl->l_type = F_UNLCK;
			return (0);
		}
	}
	
	/*
	 * Convert the flock structure into a start, end, and len.
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
		start = np->n_size + fl->l_start;
		break;
	default:
		return (EINVAL);
	}

	if (start < 0)
		return (EINVAL);
	if (!largelock && (start & 0xffffffff00000000LL))
		return (EINVAL);
	if (fl->l_len == 0) {
		/* lock from start to EOF */
 		end = -1;
		if (!largelock)
			/* maximum file size 2^32 - 1 bytes */
			len = 0x00000000ffffffffULL - start;
		else
			/* maximum file size 2^64 - 1 bytes */
			len = 0xffffffffffffffffULL - start;
	} else {
		/* lock fl->l_len bytes from start */
		end = start + fl->l_len - 1;
		len = fl->l_len;
	}

	/* F_FLOCK style locks only use F_SETLK and F_UNLCK,
	 * and always lock the entire file.
	 */
	if ((operation != F_SETLK && operation != F_UNLCK) ||
		start != 0 || end != -1) {
		return (EINVAL);
	}
	
	/*
	 * Create the lockf structure
	 */
	MALLOC(lock, struct smbfs_lockf *, sizeof *lock, M_LOCKF, M_WAITOK);
	lock->lf_start = start;
	lock->lf_end = end;
	lock->lf_id = id;
	lock->lf_smbnode = np;
	lock->lf_type = fl->l_type;
	lock->lf_next = (struct smbfs_lockf *)0;
	TAILQ_INIT(&lock->lf_blkhd);
	lock->lf_flags = flags;

	smb_makescred(&scred, p, p ? p->p_ucred : NULL);
	timeout = (flags & F_WAIT) ? -1 : 0;

	/*
	 * Do the requested operation.
	 */
	switch(operation) {
	case F_SETLK:
		/* get local lock */
		error = smbfs_setlock(lock);
		if (!error) {
			/* get remote lock */
			error = smbfs_smb_lock(np, SMB_LOCK_EXCL, smbid, start, len, largelock, &scred, timeout);
			if (error) {
				/* remote lock failed */
				/* Create another lockf structure for the clear */
				MALLOC(lock, struct smbfs_lockf *, sizeof *lock, M_LOCKF, M_WAITOK);
				lock->lf_start = start;
				lock->lf_end = end;
				lock->lf_id = id;
				lock->lf_smbnode = np;
				lock->lf_type = F_UNLCK;
				lock->lf_next = (struct smbfs_lockf *)0;
				TAILQ_INIT(&lock->lf_blkhd);
				lock->lf_flags = flags;
				/* clear local lock (this will always be successful) */
				(void) smbfs_clearlock(lock);
				FREE(lock, M_LOCKF);
			}
		}
		break;
	case F_UNLCK:
		/* clear local lock (this will always be successful) */
		error = smbfs_clearlock(lock);
		FREE(lock, M_LOCKF);
		/* clear remote lock */
		error = smbfs_smb_lock(np, SMB_LOCK_RELEASE, smbid, start, len, largelock, &scred, timeout);
		break;
	case F_GETLK:
		error = smbfs_getlock(lock, fl);
		FREE(lock, M_LOCKF);
		break;
	default:
		error = EINVAL;
		_FREE(lock, M_LOCKF);
		break;
	}
	
	if (error == EDEADLK && !(flags & F_WAIT))
		error = EAGAIN;

	return (error);
}

static int
smbfs_pathcheck(struct smbmount *smp, const char *name, int nmlen, int nameiop)
{
	const char *cp, *endp;
	int error;

	/* Check name only if CREATE, DELETE, or RENAME */
	if (nameiop == LOOKUP)
		return 0;

	/*
	 * Normally, we'd return EINVAL when the name is syntactically invalid,
	 * but ENAMETOOLONG makes it clear that the name is the problem (and
	 * allows Carbon to return a more meaningful error).
	 */
	error = ENAMETOOLONG;

	/*
	 * Note: This code does not prevent the smb file system client
	 * from creating filenames which are difficult to use with
	 * other clients. For example, you can create "  foo  " or
	 * "foo..." which cannot be moved, renamed, or deleted by some
	 * other clients.
	 */
	if (!nmlen)
		return error;
	if (SMB_DIALECT(SSTOVC(smp->sm_share)) < SMB_DIALECT_LANMAN2_0) {
		/*
		 * Name should conform short 8.3 format
		 */

		/* Look for optional period */
		cp = index(name, '.');
		if (cp != NULL) {
			/*
			 * If there's a period, then:
			 *   1 - the main part of the name must be 1 to 8 chars long
			 *   2 - the extension must be 1 to 3 chars long
			 *   3 - there cannot be more than one period
			 * On a DOS volume, a trailing period in a name is ignored,
			 * so we don't want to create "foo." and confuse programs
			 * when the file actually created is "foo"
			 */
			if ((cp == name) ||	/* no name chars */
				(cp - name > 8) || /* name is too long */
				((nmlen - ((long)(cp - name) + 1)) > 3) || /* extension is too long */
				(nmlen == ((long)(cp - name) + 1)) || /* no extension chars */
				(index(cp + 1, '.') != NULL)) { /* multiple periods */
				return error;
			}
		} else {
			/*
			 * There is no period, so main part of the name
			 * must be no longer than 8 chars.
			 */
			if (nmlen > 8)
				return error;
		}
		/* check for illegal characters */
		for (cp = name, endp = name + nmlen; cp < endp; ++cp) {
			/*
			 * check for other 8.3 illegal characters, wildcards,
			 * and separators.
			 *
			 * According to the FAT32 File System spec, the following
			 * characters are illegal in 8.3 file names: Values less than 0x20,
			 * and the values 0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B,
			 * 0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, and 0x7C.
			 * The various SMB specs say the same thing with the additional
			 * illegal character 0x20 -- control characters (0x00...0x1f)
			 * are not mentioned. So, we'll add the space character and
			 * won't filter out control characters unless we find they cause
			 * interoperability problems.
			 */
			switch (*cp) {
			case 0x20:	/* space */
			case 0x22:	/* "     */
			case 0x2A:	/* *     */
			case 0x2B:	/* +     */
			case 0x2C:	/* ,     */
						/* 0x2E (period) was handled above */
			case 0x2F:	/* /     */
			case 0x3A:	/* :     */
			case 0x3B:	/* ;     */
			case 0x3C:	/* <     */
			case 0x3D:	/* =     */
			case 0x3E:	/* >     */
			case 0x3F:	/* ?     */
			case 0x5B:	/* [     */
			case 0x5C:	/* \     */
			case 0x5D:	/* ]     */
			case 0x7C:	/* |     */
				/* illegal character found */
				return error;
				break;
			default:
				break;
			}
		}
	} else {
		/*
		 * Long name format
		 */

		/* make sure the name isn't too long */
		if (nmlen > 255)
			return error;
		/* check for illegal characters */
		for (cp = name, endp = name + nmlen; cp < endp; ++cp) {
			/*
			 * The set of illegal characters in long names is the same as
			 * 8.3 except the characters 0x20, 0x2b, 0x2c, 0x3b, 0x3d, 0x5b,
			 * and 0x5d are now legal, and the restrictions on periods was
			 * removed.
			 */
			switch (*cp) {
			case 0x22:	/* "     */
			case 0x2A:	/* *     */
			case 0x2F:	/* /     */
			case 0x3A:	/* :     */
			case 0x3C:	/* <     */
			case 0x3E:	/* >     */
			case 0x3F:	/* ?     */
			case 0x5C:	/* \     */
			case 0x7C:	/* |     */
				/* illegal character found */
				return error;
				break;
			default:
				break;
			}
		}
	}
	return 0;
}

#ifndef PDIRUNLOCK
#define	PDIRUNLOCK	0
#endif

/*
 * Things go even weird without fixed inode numbers...
 */
int
smbfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct proc *p = cnp->cn_proc;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *vp;
	struct smbmount *smp;
	struct mount *mp = dvp->v_mount;
	struct smbnode *dnp;
	struct smbfattr fattr, *fap;
	struct smb_cred scred;
	const char *name = cnp->cn_nameptr;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	int nmlen = cnp->cn_namelen;
	int lockparent, wantparent, error, islastcn, isdot;
	
	SMBVDEBUG("\n");
	cnp->cn_flags &= ~PDIRUNLOCK;
	if (dvp->v_type != VDIR)
		return ENOTDIR;
	if ((flags & ISDOTDOT) && (dvp->v_flag & VROOT)) {
		SMBFSERR("invalid '..'\n");
		return EIO;
	}
#ifdef SMB_VNODE_DEBUG
	{
		char *cp, c;

		cp = name + nmlen;
		c = *cp;
		*cp = 0;
		SMBVDEBUG("%d '%s' in '%s' id=d\n", nameiop, name, 
			VTOSMB(dvp)->n_name);
		*cp = c;
	}
#endif
	islastcn = flags & ISLASTCN;
	if (islastcn && (mp->mnt_flag & MNT_RDONLY) && (nameiop != LOOKUP))
		return EROFS;
	if ((error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, p)) != 0)
		return error;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	smp = VFSTOSMBFS(mp);
	dnp = VTOSMB(dvp);
	isdot = (nmlen == 1 && name[0] == '.');

	error = smbfs_pathcheck(smp, cnp->cn_nameptr, cnp->cn_namelen, nameiop);
	if (error)
		SMBERROR("warning: pid %d(%.*s) bad filename(%.*s)\n",
			 p->p_pid, sizeof(p->p_comm), p->p_comm, nmlen, name);
	if (error) 
		return ENOENT; /* XXX use "error" as is? */

	error = cache_lookup(dvp, vpp, cnp);
	SMBVDEBUG("cache_lookup returned %d\n", error);
	if (error > 0)
		return error;
	if (error) {		/* name was found */
		struct vattr vattr;
		u_long vpid;

		vp = *vpp;
		vpid = vp->v_id;
		if (dvp == vp) {	/* lookup on current */
			vref(vp);
			error = 0;
			SMBVDEBUG("cached '.'\n");
		} else if (flags & ISDOTDOT) {
			VOP_UNLOCK(dvp, 0, p);	/* unlock parent */
			cnp->cn_flags |= PDIRUNLOCK;
			error = vget(vp, LK_EXCLUSIVE, p);
			if (!error && lockparent && islastcn) {
				error = vn_lock(dvp, LK_EXCLUSIVE, p);
				if (error == 0)
					cnp->cn_flags &= ~PDIRUNLOCK;
			}
		} else {
			error = vget(vp, LK_EXCLUSIVE, p);
			if (!lockparent || error || !islastcn) {
				VOP_UNLOCK(dvp, 0, p);
				cnp->cn_flags |= PDIRUNLOCK;
			}
		}
		if (!error) {
			if (vpid == vp->v_id) {
			   if (!VOP_GETATTR(vp, &vattr, cnp->cn_cred, p)
			/*    && vattr.va_ctime.tv_sec == VTOSMB(vp)->n_ctime*/) {
				if (nameiop != LOOKUP && islastcn)
					cnp->cn_flags |= SAVENAME;
				SMBVDEBUG("use cached vnode\n");
				return (0);
			   }
			   cache_purge(vp);
			}
			vput(vp);
			if (lockparent && dvp != vp && islastcn)
				VOP_UNLOCK(dvp, 0, p);
		}
		error = vn_lock(dvp, LK_EXCLUSIVE, p);
		*vpp = NULLVP;
		if (error) {
			cnp->cn_flags |= PDIRUNLOCK;
			return (error);
		}
		cnp->cn_flags &= ~PDIRUNLOCK;
	}
	/* 
	 * entry is not in the cache or has been expired
	 */
	error = 0;
	*vpp = NULLVP;
	smb_makescred(&scred, p, cnp->cn_cred);
	fap = &fattr;
	if (flags & ISDOTDOT) {
		error = smbfs_smb_lookup(dnp->n_parent, NULL, NULL, fap, &scred);
		SMBVDEBUG("result of dotdot lookup: %d\n", error);
	} else {
		fap = &fattr;
		/* this can allocate a new "name" so use "out" from here on */
		error = smbfs_smb_lookup(dnp, &name, &nmlen, fap, &scred);
/*		if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.')*/
		SMBVDEBUG("result of smbfs_smb_lookup: %d\n", error);
	}
	if (error && error != ENOENT)
		goto out;
	if (error) {			/* entry not found */
		/*
		 * Handle RENAME or CREATE case...
		 */
		if ((nameiop == CREATE || nameiop == RENAME) && wantparent && islastcn) {
			cnp->cn_flags |= SAVENAME;
			if (!lockparent) {
				VOP_UNLOCK(dvp, 0, p);
				cnp->cn_flags |= PDIRUNLOCK;
			}
			error = EJUSTRETURN;
			goto out;
		}
		error = ENOENT;
		goto out;
	}/* else {
		SMBVDEBUG("Found entry %s with id=%d\n", fap->entryName, fap->dirEntNum);
	}*/
	/*
	 * handle DELETE case ...
	 */
	if (nameiop == DELETE && islastcn) { 	/* delete last component */
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, p);
		if (error)
			goto out;
		if (isdot) {
			VREF(dvp);
			*vpp = dvp;
			goto out;
		}
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp);
		if (error)
			goto out;
		*vpp = vp;
#ifndef APPLE
		cnp->cn_flags |= SAVENAME;
#endif
		if (!lockparent) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		goto out;
	}
	if (nameiop == RENAME && islastcn && wantparent) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, p);
		if (error)
			goto out;
		if (isdot) {
			error = EISDIR;
			goto out;
		}
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp);
		if (error)
			goto out;
		*vpp = vp;
		cnp->cn_flags |= SAVENAME;
		if (!lockparent) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		goto out;
	}
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, p);
		error = smbfs_nget(mp, dvp, name, nmlen, NULL, &vp);
		if (error) {
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, p);
			goto out;
		}
		if (lockparent && islastcn) {
			error = vn_lock(dvp, LK_EXCLUSIVE, p);
			if (error) {
				cnp->cn_flags |= PDIRUNLOCK;
				vput(vp);
				goto out;
			}
		}
		*vpp = vp;
	} else if (isdot) {
		vref(dvp);
		*vpp = dvp;
	} else {
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp);
		if (error)
			goto out;
		*vpp = vp;
		SMBVDEBUG("lookup: getnewvp!\n");
		if (!lockparent || !islastcn) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
	}
	if ((cnp->cn_flags & MAKEENTRY)/* && !islastcn*/) {
		struct componentname cn = *cnp;

/*		VTOSMB(*vpp)->n_ctime = VTOSMB(*vpp)->n_vattr.va_ctime.tv_sec;*/
		cn.cn_nameptr = (char *)name;
		cn.cn_namelen = nmlen;
#ifdef APPLE
		{
			int indx;
			char *cp;

			cn.cn_hash = 0;
			for (cp = cn.cn_nameptr, indx = 1;
			     *cp != 0 && *cp != '/'; indx++, cp++)
				cn.cn_hash += (unsigned char)*cp * indx;
		}
		if (cn.cn_namelen <=  NCHNAMLEN) /* X namecache limitation */
#endif
			cache_enter(dvp, *vpp, &cn);
	}
	error = 0;
out:
	if (name != cnp->cn_nameptr)
		smbfs_name_free(name);
	return error;
}


#ifdef APPLE

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
smbfs_getattrlist(ap)
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
	 * Reject requests that have an invalid bitmap count (indicating
	 * a change in the headers that this code isn't prepared
	 * to handle).
	 *
	 * NOTE: we don't use ATTR_BIT_MAP_COUNT, because that could
	 * change in the header without this code changing.
	 */
	if (alist->bitmapcount != 5)
		return EINVAL;

	/*
	 * Reject requests that ask for non-existent attributes.
	 */
	if (((alist->commonattr & ~ATTR_CMN_VALIDMASK) != 0) ||
	    ((alist->volattr & ~ATTR_VOL_VALIDMASK) != 0) ||
	    ((alist->dirattr & ~ATTR_DIR_VALIDMASK) != 0) ||
	    ((alist->fileattr & ~ATTR_FILE_VALIDMASK) != 0) ||
	    ((alist->forkattr & ~ATTR_FORK_VALIDMASK) != 0))
		return EINVAL;

	/*
	 * Requesting volume information requires setting the
	 * ATTR_VOL_INFO bit. Also, volume info requests are
	 * mutually exclusive with all other info requests.
	 */
	if ((alist->volattr != 0) &&
	    (((alist->volattr & ATTR_VOL_INFO) == 0) ||
	     (alist->dirattr != 0) || (alist->fileattr != 0)))
		return EINVAL;

	/*
	 * Reject requests that ask for anything other than volume
	 * capabilities.  We return EOPNOTSUPP for this, as Carbon
	 * expects to get that if requests for ATTR_CMN_ values
	 * aren't supported by the file system.
	 */
	if ((alist->commonattr != 0) ||
	    (alist->volattr != (ATTR_VOL_INFO | ATTR_VOL_CAPABILITIES)) ||
	    (alist->dirattr != 0) ||
	    (alist->fileattr != 0) ||
	    (alist->forkattr != 0)) {
		return EOPNOTSUPP;
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
	attrbufsize = MIN(ap->a_uio->uio_resid, (int)sizeof results);
	results.buffer_size = attrbufsize;
	
	/* The capabilities[] array defines what this volume supports. */
	results.capabilities.capabilities[VOL_CAPABILITIES_FORMAT] =
		VOL_CAP_FMT_NO_ROOT_TIMES |
		VOL_CAP_FMT_FAST_STATFS;
	results.capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] =
		VOL_CAP_INT_FLOCK;
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
		/* While the SMB file system is case sensitive and case preserving,
		 * not all SMB/CIFS servers are case sensitive and case preserving.
		 * That's because the volume used for storage on a SMB/CIFS server
		 * may not be case sensitive or case preserving. So, rather than
		 * providing a wrong yes or no answer for VOL_CAP_FMT_CASE_SENSITIVE
		 * and VOL_CAP_FMT_CASE_PRESERVING, we'll deny knowledge of those
		 * volume attributes.
		 */
#if 0
		VOL_CAP_FMT_CASE_SENSITIVE |
		VOL_CAP_FMT_CASE_PRESERVING |
#endif
		VOL_CAP_FMT_FAST_STATFS;
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
		VOL_CAP_INT_FLOCK;
	results.capabilities.valid[VOL_CAPABILITIES_RESERVED1] = 0;
	results.capabilities.valid[VOL_CAPABILITIES_RESERVED2] = 0;

	/* Copy the results to the caller. */
	error = uiomove((caddr_t) &results, attrbufsize, ap->a_uio);
	
	return error;
}


/* offtoblk converts a file offset to a logical block number */
static int 
smbfs_offtoblk(ap)
	struct vop_offtoblk_args /* {
		struct vnode *a_vp;
		off_t a_offset;
		daddr_t *a_lblkno;
	} */ *ap;
{
	*ap->a_lblkno = ap->a_offset / PAGE_SIZE_64;
	return(0);
}


/* blktooff converts a logical block number to a file offset */
static int     
smbfs_blktooff(ap)
	struct vop_blktooff_args /* {   
		struct vnode *a_vp;
		daddr_t a_lblkno;
		off_t *a_offset;
	} */ *ap;
{	
	*ap->a_offset = (off_t)ap->a_lblkno * PAGE_SIZE_64;
	return(0);
}


static int
smbfs_pagein(ap)
	struct vop_pagein_args /* {
		struct vnode *	a_vp,
		upl_t		a_pl,
		vm_offset_t	a_pl_offset,
		off_t		a_f_offset, 
		size_t		a_size,
		struct ucred *	a_cred,
		int		a_flags
	} */ *ap;
{       
	struct vnode *vp;
	upl_t pl;
	size_t size;
	off_t f_offset;
	vm_offset_t pl_offset, ioaddr;
	struct proc *p;
	int error, nocommit;
	struct smbnode *np;
	struct smbmount *smp;
	struct ucred *cred;
	struct smb_cred scred;
	struct uio uio;
	struct iovec iov;
	kern_return_t   kret;

	f_offset = ap->a_f_offset;
	size = ap->a_size;
	pl = ap->a_pl;
	pl_offset = ap->a_pl_offset;
	vp = ap->a_vp;
	if (UBCINVALID(vp))     
		panic("smbfs_pagein: ubc invalid vp=0x%x", vp);
	if (UBCINFOMISSING(vp)) 
		panic("smbfs_pagein: No mapping: vp=0x%x", vp);
	nocommit = ap->a_flags & UPL_NOCOMMIT;
	np = VTOSMB(vp);
	if (size <= 0 || f_offset < 0 || f_offset >= (off_t)np->n_size ||
	    f_offset & PAGE_MASK_64 || size & PAGE_MASK) {
		error = EINVAL;
		goto exit;
	}
	kret = ubc_upl_map(pl, &ioaddr);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pagein: ubc_upl_map %d!", kret);
	cred = ubc_getcred(vp);
	if (cred == NOCRED)
		cred = ap->a_cred;
	p = current_proc();
	/*
 	 * Potential FreeBSD vs Darwin VFS difference: we are not ensured a
 	 * VOP_OPEN, so do it implicitly.  The log message is because we
	 * anticipate only READ VOPs before OPEN.  If this becomes
	 * an expected code path then remove the "log".
	 *
	 * XXX - when is the implied open closed?
 	 */
 	if (np->n_opencount == 0) {
		log(LOG_NOTICE, "smbfs_pagein: implied open\n");
 		error = VOP_OPEN(vp, FREAD, cred, p);
 		if (error)
 			goto unmapexit;
 	}
	smb_makescred(&scred, p, cred);

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = f_offset;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;
	if (f_offset + size > (off_t)np->n_size) { /* stop at EOF */
		size -= PAGE_SIZE;
		size += np->n_size & PAGE_MASK_64;
	}
	uio.uio_resid = size;
	iov.iov_len  = uio.uio_resid;
	iov.iov_base = (caddr_t)(ioaddr + pl_offset);

	smp = VFSTOSMBFS(vp->v_mount);
	(void) smbfs_smb_flush(np, &scred);
	error = smb_read(smp->sm_share, np->n_fid, &uio, &scred);
	if (!error && uio.uio_resid)
		error = EFAULT;
	if (!error && size != ap->a_size)
		bzero((caddr_t)(ioaddr + pl_offset) + size, ap->a_size - size);
unmapexit:
	kret = ubc_upl_unmap(pl);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pagein: ubc_upl_unmap %d", kret);
exit:
	if (error)
		log(LOG_WARNING, "smbfs_pagein: read error=%d\n", error);
	if (nocommit)
		return (error);
	if (error) {
		(void)ubc_upl_abort_range(pl, pl_offset, ap->a_size,
					  UPL_ABORT_ERROR |
					  UPL_ABORT_FREE_ON_EMPTY);
	} else
		(void)ubc_upl_commit_range(pl, pl_offset, ap->a_size,
					   UPL_COMMIT_CLEAR_DIRTY |
					   UPL_COMMIT_FREE_ON_EMPTY);
	SMBVDEBUG("paged read done: %d\n", error);
	return (error);
}


static int
smbfs_pageout(ap) 
	struct vop_pageout_args /* {
		struct vnode	*a_vp,
		upl_t	a_pl,
		vm_offset_t	a_pl_offset,
		off_t	a_f_offset,
		size_t	a_size,   
		struct ucred	*a_cred,
		int	a_flags
	} */ *ap;
{       
	struct vnode *vp;
	upl_t pl;
	size_t size;
	off_t f_offset;
	vm_offset_t pl_offset, ioaddr;
	struct proc *p;
	int error, nocommit;
	struct smbnode *np;
	struct smbmount *smp;
	struct ucred *cred;
	struct smb_cred scred;
	struct uio uio;
	struct iovec iov;
	kern_return_t   kret;

	f_offset = ap->a_f_offset;
	size = ap->a_size;
	pl = ap->a_pl;
	pl_offset = ap->a_pl_offset;
	vp = ap->a_vp;
	smp = VFSTOSMBFS(vp->v_mount);
	if (UBCINVALID(vp))     
		panic("smbfs_pageout: ubc invalid vp=0x%x", vp);
	if (UBCINFOMISSING(vp)) 
		panic("smbfs_pageout: No mapping: vp=0x%x", vp);
	nocommit = ap->a_flags & UPL_NOCOMMIT;
	if (pl == (upl_t)NULL)
		panic("smbfs_pageout: no upl");
	np = VTOSMB(vp);
	if (size <= 0 || f_offset < 0 || f_offset >= (off_t)np->n_size ||
	    f_offset & PAGE_MASK_64 || size & PAGE_MASK) {
		error = EINVAL;
		goto exit;
	}
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		error = EROFS;
		goto exit;
	}
	kret = ubc_upl_map(pl, &ioaddr);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pageout: ubc_upl_map %d!", kret);
	cred = ubc_getcred(vp);
	if (cred == NOCRED)
		cred = ap->a_cred;
	p = current_proc();
	/*
 	 * Potential FreeBSD vs Darwin VFS difference: we are not ensured a
 	 * VOP_OPEN, so do it implicitly.  The log message is because we
	 * anticipate only READ VOPs before OPEN.  If this becomes
	 * an expected code path then remove the "log".
	 *
	 * We also force an open for writing if it's not already open
	 * for writing.
	 *
	 * XXX - when is the implied open closed?
	 */
 	if (np->n_opencount == 0 ||
	    (np->n_rwstate & SMB_AM_OPENMODE) != SMB_AM_OPENRW) {
		log(LOG_NOTICE, "smbfs_pageout: implied open: opencount %d, rwstate 0x%x\n",
		    np->n_opencount, np->n_rwstate);
 		error = VOP_OPEN(vp, FWRITE, cred, p);
 		if (error)
 			goto unmapexit;
 	}
	smb_makescred(&scred, p, cred);

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = f_offset;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = p;
	if (f_offset + size > (off_t)np->n_size) { /* stop at EOF */
		size -= PAGE_SIZE;
		size += np->n_size & PAGE_MASK_64;
	}
	uio.uio_resid = size;
	iov.iov_len  = uio.uio_resid;
	iov.iov_base = (caddr_t)(ioaddr + pl_offset);

	vp->v_numoutput++;
	SMBVDEBUG("ofs=%d, resid=%d\n", (int)uio.uio_offset, uio.uio_resid);
	error = smb_write(smp->sm_share, np->n_fid, &uio, &scred, SMBWRTTIMO);
	np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);
	vp->v_numoutput--;
unmapexit:
	kret = ubc_upl_unmap(pl);
	if (kret != KERN_SUCCESS)
		panic("smbfs_pageout: ubc_upl_unmap %d", kret);
exit:
	if (error)
		log(LOG_WARNING, "smbfs_pageout: write error=%d\n", error);
	else {
		/* if success, blow away statfs cache */
		smp->sm_statfstime = 0;
	}
	if (nocommit)
		return (error);
	if (error) {
		(void)ubc_upl_abort_range(pl, pl_offset, ap->a_size,
					  UPL_ABORT_DUMP_PAGES |
					  UPL_ABORT_FREE_ON_EMPTY);
	} else
		(void)ubc_upl_commit_range(pl, pl_offset, ap->a_size,
					   UPL_COMMIT_CLEAR_DIRTY |
					   UPL_COMMIT_FREE_ON_EMPTY);
	SMBVDEBUG("paged write done: %d\n", error);
	return (error);
}


int
smbfs_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
/*
 * XXX Good news: We have a real VOP_LOCK now
 * XXX Bad news: VOP_LOCK call chain doesnt check for EINTR
 * XXX Bad news: So much for interruptible mounts
 */
	return (lockmgr(&VTOSMB(vp)->n_lock, ap->a_flags, &vp->v_interlock,
		ap->a_p));
}


int
smbfs_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTOSMB(vp)->n_lock, ap->a_flags | LK_RELEASE,
		&vp->v_interlock, ap->a_p));
}       


int
smbfs_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	return (lockstatus(&VTOSMB(ap->a_vp)->n_lock));
}
#endif /* APPLE */
