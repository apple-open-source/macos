/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1998-1999 Apple Computer, Inc. All Rights Reserved.
 *
 *	Modification History:
 *
 *	02-Feb-2004	Alfred Perlstein	Adapted to autofs.
 *	02-Feb-2000	Clark Warner		Added copyfile to table
 *	17-Aug-1999	Pat Dirks		New today.
 *
 * $Id: autofs_vnops.c,v 1.28 2005/03/12 03:18:55 lindak Exp $
 */

#include <mach/mach_types.h>
#include <mach/machine/boolean.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/attr.h>
#include <sys/vnode_if.h>
#include <sys/vm.h>
#include <sys/errno.h>
#include <vfs/vfs_support.h>
#include <sys/uio.h>
#include <sys/xattr.h>

#include "autofs.h"

extern int groupmember(gid_t gid, struct ucred* cred);

#ifdef DEBUG
uint32_t autofs_debug = 1;
#else
uint32_t autofs_debug = 0;
#endif

#define vprint(label, vp) do { } while (0)

#define VOPFUNC int (*)(void *)

#define AUTOFS_BLOCKSIZE 512

/* Global vfs data structures for autofs. */
int (**autofs_vnodeop_p) (void *);
struct vnodeopv_entry_desc autofs_vnodeop_entries[] = {
	{&vnop_default_desc, (VOPFUNC)vn_default_error},
	{&vnop_lookup_desc, (VOPFUNC)autofs_cached_lookup},	/* cached lookup */
	{&vnop_create_desc, (VOPFUNC)autofs_create},		/* create		- DEBUGGER */
	{&vnop_open_desc, (VOPFUNC)autofs_open},			/* open			- DEBUGGER */
	{&vnop_close_desc, (VOPFUNC)nop_close},				/* close		- NOP */
	{&vnop_getattr_desc, (VOPFUNC)autofs_getattr},		/* getattr */
	{&vnop_setattr_desc, (VOPFUNC)autofs_setattr},		/* setattr */
	{&vnop_mmap_desc, (VOPFUNC)autofs_mmap},			/* mmap			- DEBUGGER */
	{&vnop_fsync_desc, (VOPFUNC)nop_fsync},				/* fsync		- NOP */
	{&vnop_remove_desc, (VOPFUNC)autofs_remove},		/* remove */
	{&vnop_rename_desc, (VOPFUNC)autofs_rename},		/* rename */
	{&vnop_mkdir_desc, (VOPFUNC)autofs_mkdir},			/* mkdir */
	{&vnop_rmdir_desc, (VOPFUNC)autofs_rmdir},			/* rmdir */
	{&vnop_symlink_desc, (VOPFUNC)autofs_symlink},		/* symlink */
	{&vnop_readdir_desc, (VOPFUNC)autofs_readdir},		/* readdir */
	{&vnop_readlink_desc, (VOPFUNC)autofs_readlink},	/* readlink */
	{&vnop_inactive_desc, (VOPFUNC)autofs_inactive},	/* inactive */
	{&vnop_reclaim_desc, (VOPFUNC)autofs_reclaim},		/* reclaim */
	{&vnop_pathconf_desc, (VOPFUNC)autofs_pathconf},	/* pathconf */
	{&vnop_setxattr_desc, (VOPFUNC)autofs_setxattr},	/* setxattr */
	{&vnop_getxattr_desc, (VOPFUNC)autofs_getxattr},	/* getxattr */
	{&vnop_listxattr_desc, (VOPFUNC)autofs_listxattr},	/* listxattr */
	{&vnop_removexattr_desc, (VOPFUNC)autofs_removexattr},	/* removexattr */
   {(struct vnodeop_desc *) NULL, (int (*) ()) NULL}
};

struct vnodeopv_desc autofsfs_vnodeop_opv_desc =
	{ &autofs_vnodeop_p, autofs_vnodeop_entries };

extern struct vnodeop_desc vnop_remove_desc;

static int autofs_trigger(vnode_t dp, vnode_t *vpp, struct componentname *cnp, uid_t uid, vfs_context_t context);
static int autofs_proc_is_mounter(vnode_t vp, proc_t p);
static int autofs_lookup_int(vnode_t dp, vnode_t *vpp, struct componentname *cnp, vfs_context_t context);

const char *
opstr(int nameiop)
{
	switch (nameiop & OPMASK) {
	case LOOKUP:
		return ("LOOKUP");
	case CREATE:
		return ("CREATE");
	case DELETE:
		return ("DELETE");
	case RENAME:
		return ("RENAME");
	default:
		return ("UNKNOWN");
	}
}

/*
 * Create a regular file
#% create	dvp	L U U
#% create	vpp	- L -
#
vnop_create {
	IN WILLRELE vnode_t dvp;
	OUT vnode_t *vpp;
	IN struct componentname *cnp;
	IN struct vnode_attr *vap;
	IN vfs_context_t context;

	 We are responsible for freeing the namei buffer, it is done in
	 hfs_makenode(), unless there is a previous error.

*/

int
autofs_create(ap)
struct vnop_create_args /* {
	vnode_t a_dvp;
	vnode_t *a_vpp;
	struct componentname *a_cnp;
	struct vnode_attr *a_vap;
	vfs_context_t a_context;
} */ *ap;
{
	return (err_create(ap));
}



/*
 * Open called.
#% open		vp	L L L
#
 vnop_open {
	 IN vnode_t vp;
	 IN int mode;
	 IN vfs_context_t context;
 */

int
autofs_open(ap)
struct vnop_open_args /* {
	vnode_t a_vp;
	int  a_mode;
	vfs_context_t a_context;
} */ *ap;
{
	return (0);
}

int
autofs_mmap(ap)
struct vnop_mmap_args /* {
	vnode_t a_vp;
	int  a_fflags;
} */ *ap;
{
	return (EINVAL);
}

/*
#% getattr	vp	= = =
#
 vnop_getattr {
	 IN vnode_t vp;
	 IN struct vnode_attr *vap;
	 IN vfs_context_t context;

*/
int
autofs_getattr(ap)
struct vnop_getattr_args /* {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	struct vnode_attr *a_vap;
	vfs_context_t a_context;
} */ *ap;
{
	vnode_t vp     = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
	struct autofsnode *sp = VTOA(vp);
	struct autofsnode *an2;

	lck_rw_lock_shared(sp->s_lock);

	VATTR_RETURN(vap, va_type, vnode_vtype(vp));
	VATTR_RETURN(vap, va_mode, sp->s_mode);
	VATTR_RETURN(vap, va_nlink, sp->s_linkcount);
	VATTR_RETURN(vap, va_uid, sp->s_uid);
	VATTR_RETURN(vap, va_gid, sp->s_gid);
	VATTR_RETURN(vap, va_fsid, vfs_statfs(VTOVFS(vp))->f_fsid.val[0]);
	VATTR_RETURN(vap, va_fileid, sp->s_nodeid);
	switch (vnode_vtype(vp)) {
	case VDIR:
		VATTR_RETURN(vap, va_data_size, (sp->s_u.d.d_entrycount + 2) * sizeof(struct dirent));
		break;
	case VREG:
		VATTR_RETURN(vap, va_data_size, sp->s_u.f.f_size);
		break;
	case VLNK:
		VATTR_RETURN(vap, va_data_size, sp->s_u.s.s_length);
		break;
	default:
		VATTR_RETURN(vap, va_data_size, 0);
		break;
	}
	VATTR_RETURN(vap, va_iosize, AUTOFS_BLOCKSIZE);
	vap->va_access_time.tv_sec = sp->s_accesstime.tv_sec;
	vap->va_access_time.tv_nsec = sp->s_accesstime.tv_usec * 1000;
	VATTR_SET_SUPPORTED(vap, va_access_time);
	vap->va_modify_time.tv_sec = sp->s_modificationtime.tv_sec;
	vap->va_modify_time.tv_nsec = sp->s_modificationtime.tv_usec * 1000;
	VATTR_SET_SUPPORTED(vap, va_modify_time);
	vap->va_change_time.tv_sec = sp->s_changetime.tv_sec;
	vap->va_change_time.tv_nsec = sp->s_changetime.tv_usec * 1000;
	VATTR_SET_SUPPORTED(vap, va_change_time);
	VATTR_RETURN(vap, va_gen, sp->s_generation);
	VATTR_RETURN(vap, va_flags, sp->s_flags);
	if (sp->s_clonedfrom == NULL) {
		/* This is the template node, which should be marked appropriately: */
		if (sp->s_nodeflags & IN_TRIGGER) {
			vap->va_flags |= SF_AUTOMOUNT;	/* trigger */
			if (sp->s_nodeflags & IN_UID) vap->va_flags |= (SF_UID | SF_AUTH);  /* Needs auth */
			TAILQ_FOREACH(an2, &sp->s_clonehd, s_clonelst) {
				if ((vfs_context_ucred(ap->a_context)->cr_uid == an2->s_cloneuid) &&
					(ATOV(an2) != NULL) &&
					(vnode_mountedhere(ATOV(an2)) != NULL)) {
					/* Found a vnode for calling uid with mounted filesystem: */
					vap->va_flags |= SF_MOUNTED;	/* mounted */
					break;
				};
			}
		}
	}
	VATTR_RETURN(vap, va_rdev, sp->s_rdev);
	VATTR_RETURN(vap, va_total_size, AUTOFS_BLOCKSIZE * ((vap->va_data_size + AUTOFS_BLOCKSIZE - 1) / AUTOFS_BLOCKSIZE));
	VATTR_RETURN(vap, va_filerev, 0);
	
	lck_rw_unlock_shared(sp->s_lock);

	return 0;
}

/*
 * Change the mode on a file or directory.
 * vnode vp must be locked on entry.
 */
int
autofs_chmod(vnode_t vp, int mode, vfs_context_t context)
{
	struct autofsnode *sp = VTOA(vp);
	int result;

	sp->s_mode &= ~ALLPERMS;
	sp->s_mode |= (mode & ALLPERMS);
	sp->s_nodeflags |= IN_CHANGE;

	result = 0;
	
	return result;
}

/*
 * Change the flags on a file or directory.
 * vnode vp must be locked on entry.
 */
int
autofs_chflags(
	vnode_t vp,
	u_long flags,
	vfs_context_t context)
{
	struct autofsnode *sp = VTOA(vp);
	int result;

	sp->s_flags &= SF_SETTABLE;
	sp->s_flags |= (flags & UF_SETTABLE);
	sp->s_nodeflags |= IN_CHANGE;
	
	result = 0;

	return result;
}



/*
 * Perform chown operation on vnode vp;
 * vnode vp must be locked on entry.
 */
int
autofs_chown(
	vnode_t vp,
	uid_t uid,
	gid_t gid,
	vfs_context_t context)
{
	struct autofsnode *sp = VTOA(vp);
	uid_t ouid;
	gid_t ogid;
	int result = 0;

	if (uid == (uid_t)VNOVAL) uid = sp->s_uid;
	if (gid == (gid_t)VNOVAL) gid = sp->s_gid;

	ogid = sp->s_gid;
	ouid = sp->s_uid;

	sp->s_gid = gid;
	sp->s_uid = uid;

	if (ouid != uid || ogid != gid) sp->s_nodeflags |= IN_CHANGE;
	
	result = 0;

	return result;
}

static int
autofs_setattr_int(vnode_t vp, struct vnode_attr *vap, vfs_context_t context)
{
	struct autofsnode *sp = VTOA(vp);
	struct timeval atimeval, mtimeval;
	uid_t nuid;
	uid_t ngid;
	int result = 0;

	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		if ((result = autofs_chflags(vp, vap->va_flags, context))) {
			goto Err_Exit;
		}
	}
	VATTR_SET_SUPPORTED(vap, va_flags);

	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	nuid = VATTR_IS_ACTIVE(vap, va_uid) ? vap->va_uid : VNOVAL;
	ngid = VATTR_IS_ACTIVE(vap, va_gid) ? vap->va_gid : VNOVAL;
	if ((nuid != (uid_t)VNOVAL) || (ngid != (gid_t)VNOVAL)) {
		result = autofs_chown(vp, nuid, ngid, context);
		if (result) goto Err_Exit;
	}
	VATTR_SET_SUPPORTED(vap, va_uid);
	VATTR_SET_SUPPORTED(vap, va_gid);
	
	sp = VTOA(vp);
	if ((VATTR_IS_ACTIVE(vap, va_access_time)) ||
		(VATTR_IS_ACTIVE(vap, va_modify_time))) {
		if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
			sp->s_nodeflags |= IN_ACCESS;
			atimeval.tv_sec = vap->va_access_time.tv_sec;
			atimeval.tv_usec = vap->va_access_time.tv_nsec / 1000;
		}
		if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
			sp->s_nodeflags |= IN_CHANGE | IN_UPDATE;
			mtimeval.tv_sec = vap->va_modify_time.tv_sec;
			mtimeval.tv_usec = vap->va_modify_time.tv_nsec / 1000;
		}
		if ((result = autofs_update(vp, &atimeval, &mtimeval, 1))) {
			goto Err_Exit;
		}
	}
	VATTR_SET_SUPPORTED(vap, va_access_time);
	VATTR_SET_SUPPORTED(vap, va_modify_time);

	if (VATTR_IS_ACTIVE(vap, va_mode)) {
		result = autofs_chmod(vp, (int)vap->va_mode, context);
	}
	VATTR_SET_SUPPORTED(vap, va_mode);

Err_Exit:
	DBG_VOP(("autofs_setattr: returning %d...\n", result));
	return result;
}

int
autofs_setattr(ap)
	struct vnop_setattr_args /* {
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct autofsnode *sp = VTOA(vp);
	struct vnode_attr *vap = ap->a_vap;
	int result;

	lck_rw_lock_shared(sp->s_lock);

	result = autofs_setattr_int(vp, vap, ap->a_context);
	
	lck_rw_unlock_shared(sp->s_lock);

	return result;
}

/*
 * Mkdir system call

#% mkdir	dvp	L U U
#% mkdir	vpp	- L -
#
 vnop_mkdir {
	 IN WILLRELE vnode_t dvp;
	 OUT vnode_t *vpp;
	 IN struct componentname *cnp;
	 IN struct vnode_attr *vap;
	 IN vfs_context_t context;

	 We are responsible for freeing the namei buffer,
	 it is done in autofs_makenode(), unless there is
	 a previous error.

*/

int
autofs_mkdir(ap)
struct vnop_mkdir_args /* {
	vnode_t a_dvp;
	vnode_t *a_vpp;
	struct componentname *a_cnp;
	struct vnode_attr *a_vap;
	vfs_context_t a_context;
} */ *ap;
{
	int retval;
	vnode_t dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	int mode = MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode);
	
	if (autofs_debug) vprint("autofs_mkdir enter dvp", dvp);

	/* Lock the parent directory: */
	lck_rw_lock_shared(VTOA(dvp)->s_lock);

	/* autofs_new_directory returns VTOA(*ap->a_vpp) locked */
	retval = autofs_new_directory(VTOVFS(dvp), dvp, cnp->cn_nameptr, VTOAFS(dvp)->autofs_nextid++, mode, ap->a_vpp, ap->a_context);
	if (retval) goto Err_Exit;
	
	retval = autofs_setattr_int(*ap->a_vpp, ap->a_vap, ap->a_context);

	/* Unlock the newly created directory */
	lck_rw_unlock_exclusive(VTOA(*ap->a_vpp)->s_lock);

Err_Exit:
	if (autofs_debug) {
		if (*ap->a_vpp) {
			vprint("autofs_mkdir exiting vp:", *ap->a_vpp);
		} else {
			DBG_VOP(("autofs_mkdir exiting vp: NULL"));
		}
		vprint("autofs_mkdir exiting dvp:", dvp);
	}
	if (retval != 0) {
		if (*ap->a_vpp) autofs_remove_entry(VTOA(*ap->a_vpp));
	}

	/* Unlock the parent directory: */
	lck_rw_unlock_shared(VTOA(dvp)->s_lock);

	return retval;
}



/*

#% remove	dvp	L U U
#% remove	vp	L U U
#
 vnop_remove {
	 IN WILLRELE vnode_t dvp;
	 IN WILLRELE vnode_t vp;
	 IN struct componentname *cnp;
	 IN vfs_context_t context;

	 */
int
autofs_remove(ap)
struct vnop_remove_args /* {
	struct vnodeop_desc *a_desc;
	vnode_t a_dvp;
	vnode_t a_vp;
	struct componentname *a_cnp;
	int a_flags;
	vfs_context_t a_context;
} */ *ap;
{
	vnode_t vp = ap->a_vp;
	vnode_t dvp = ap->a_dvp;
	struct autofsnode *sp = VTOA(vp);
	struct autofsnode *dsp = VTOA(dvp);
	struct timeval tv;
	int retval = 0;

	lck_rw_lock_exclusive(dsp->s_lock);
	lck_rw_lock_exclusive(sp->s_lock);
	
	/* Don't allow deletes of busy files (option used by Carbon) */
	if ((ap->a_flags & VNODE_REMOVE_NODELETEBUSY) && vnode_isinuse(vp, 0))
	{
		retval = EBUSY;
		goto out;
	}

	/* This is sort of silly right now but someday it may make sense... */
	if (sp->s_nodeflags & IN_MODIFIED) {
		microtime(&tv);
		autofs_update(vp, &tv, &tv, 0);
	}

	/* remove the entry from the namei cache: */
	cache_purge(vp);

	/* remove entry from tree and reclaim any resources consumed: */
	retval = autofs_remove_entry(sp);

out:

	if (!retval) VTOA(dvp)->s_nodeflags |= IN_CHANGE | IN_UPDATE;

	lck_rw_unlock_exclusive(sp->s_lock);
	lck_rw_unlock_exclusive(dsp->s_lock);

	return retval;
}



/*
#% rmdir	dvp	L U U
#% rmdir	vp	L U U
#
 vnop_rmdir {
	 IN WILLRELE vnode_t dvp;
	 IN WILLRELE vnode_t vp;
	 IN struct componentname *cnp;
	 IN vfs_context_t context;
 */
int
autofs_rmdir(ap)
	struct vnop_rmdir_args /* {
		vnode_t a_dvp;
		vnode_t a_vp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
} */ *ap;
{
	struct vnop_remove_args ap2;

	ap2.a_desc = &vnop_remove_desc;
	ap2.a_dvp = ap->a_dvp;
	ap2.a_vp = ap->a_vp;
	ap2.a_cnp = ap->a_cnp;
	ap2.a_flags = 0;
	ap2.a_context = ap->a_context;

	if (autofs_debug) {
		vprint("autofs_rmdir dvp", ap2.a_dvp);
		if (ap2.a_vp != NULL)
			vprint("autofs_rmdir vp", ap2.a_vp);
	}
	return (autofs_remove(&ap2));
}



/*
#
#% symlink	dvp	L U U
#% symlink	vpp	- U -
#
# XXX - note that the return vnode has already been vrele'ed
#	by the filesystem layer.  To use it you must use vget,
#	possibly with a further namei.
#
 vnop_symlink {
	 IN WILLRELE vnode_t dvp;
	 OUT WILLRELE vnode_t *vpp;
	 IN struct componentname *cnp;
	 IN struct vnode_attr *vap;
	 IN char *target;
	 IN vfs_context_t context;

	 We are responsible for freeing the namei buffer,
	 it is done in autofs_makenode(), unless there is
	 a previous error.
*/

int
autofs_symlink(ap)
	struct vnop_symlink_args /* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		char *a_target;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	int retval;

	*vpp = NULL;

	/* Lock the parent directory: */
	lck_rw_lock_exclusive(VTOA(dvp)->s_lock);
	
	/* autofs_new_symlink returns VTOA(vpp) locked */
	retval = autofs_new_symlink(VTOVFS(dvp), dvp, cnp->cn_nameptr,
	    VTOAFS(dvp)->autofs_nextid++, ap->a_target, vpp, ap->a_context);
	if (retval)
		goto Err_Exit;
	
	/* Unlock the newly created symlink: */
	lck_rw_unlock_exclusive(VTOA(*vpp)->s_lock);

Err_Exit:
	/* Unlock the parent directory: */
	lck_rw_unlock_exclusive(VTOA(dvp)->s_lock);
	
	return retval;
}


/*
#
#% readlink	vp	L L L
#
 vnop_readlink {
	 IN vnode_t vp;
	 INOUT struct uio *uio;
	 IN struct ucred *cred;
	 IN vfs_context_t context;
	 */

int
autofs_readlink(ap)
struct vnop_readlink_args /* {
	vnode_t a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
	vfs_context_t a_context;
} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct autofsnode *sp = VTOA(vp);
	struct uio *uio = ap->a_uio;
	int retval;
	unsigned long count;

	if (uio_offset(uio) > sp->s_u.s.s_length) {
		return (0);
	}

	// LP64todo - fix this!
	if (uio_offset(uio) + uio_resid(uio) <= sp->s_u.s.s_length) {
		count = uio_resid(uio);
	} else {
		count = sp->s_u.s.s_length - uio_offset(uio);
	}
	retval = uiomove((void *)((unsigned char *)sp->s_u.s.s_symlinktarget +
		uio_offset(uio)), count, uio);
	return (retval);
}

/*
#% readdir	vp	L L L
#
vnop_readdir {
	IN vnode_t vp;
	INOUT struct uio *uio;
	INOUT int *eofflag;
	OUT int *ncookies;
	INOUT u_long **cookies;
	IN vfs_context_t context;
*/
int
autofs_readdir(ap)
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
	struct autofsnode *sp = VTOA(vp);
	register struct uio *uio = ap->a_uio;
	off_t diroffset;	/* Offset into simulated directory file */
	struct autofsnode *entry;
	errno_t result = 0;

	if (ap->a_numdirent)
		*ap->a_numdirent = 0;

	if (ap->a_flags & (VNODE_READDIR_EXTENDED | VNODE_READDIR_REQSEEKOFF))
		return (EINVAL);

	DBG_VOP(("\tuio_offset = %d, uio_resid = %d\n", (int) uio_offset(uio), uio_resid(uio)));

	lck_rw_lock_exclusive(sp->s_lock);
	
 	if (sp->s_nodeflags & IN_DEFERRED) {
		vnode_t dp;
 		int needs_put = 0;		/* Non-zero if we need to vnode_put(dp) */
		/* Generation of this directory's contents was deferred: trigger a request for it now. */
		if ((PARENTNODE(sp) != NULL) && (PARENTNODE(sp) != VTOA(vp))) {
			result = autofs_to_vnode(vnode_mount(vp), PARENTNODE(sp), &dp);
			if (result != 0) goto Err_Exit;
			needs_put = 1;
		} else {
			dp = vp;		/* vp is alread locked and ref'ed */
		}
		result = autofs_trigger(dp, &vp, NULL, vfs_context_ucred(ap->a_context)->cr_uid, ap->a_context);
		if (needs_put)
			vnode_put(dp);
		if (result)
			goto Err_Exit;
	}
	
	diroffset = 0;

	/*
	 * We must synthesize . and ..
	 */
	DBG_VOP(("\tstarting ... uio_offset = %d, uio_resid = %d\n",
		(int) uio_offset(uio), uio_resid(uio)));
	if (uio_offset(uio) == diroffset) {
		DBG_VOP(("\tAdding .\n"));
		diroffset += autofs_adddirentry(sp->s_nodeid, sp->s_type, ".",
		    sp, uio);
		DBG_VOP(("\t   after adding ., uio_offset = %d, "
			"uio_resid = %d\n",
			(int) uio_offset(uio), uio_resid(uio)));
	}
	if ((uio_resid(uio) > 0) && (diroffset > uio_offset(uio))) {
		/*
		 * Oops - we skipped over a partial entry: at best,
		 * diroffset should've just matched uio_offset(uio)
		 */
		result = EINVAL;
		goto Err_Exit;
	}

	if (uio_offset(uio) == diroffset) {
		DBG_VOP(("\tAdding ..\n"));
		if (PARENTNODE(sp) != NULL) {
			diroffset += autofs_adddirentry(PARENTNODE(sp)->s_nodeid,
			    PARENTNODE(sp)->s_type, "..", PARENTNODE(sp), uio);
			if (ap->a_numdirent)
				++(*ap->a_numdirent);
		} else {
			diroffset += autofs_adddirentry(sp->s_nodeid,
			    sp->s_type, "..", sp, uio);
			if (ap->a_numdirent)
				++(*ap->a_numdirent);
		}
		DBG_VOP(("\t   after adding .., uio_offset = %d, "
			"uio_resid = %d\n",
			(int) uio_offset(uio), uio_resid(uio)));
	}
	if ((uio_resid(uio) > 0) && (diroffset > uio_offset(uio))) {
		/*
		 * Oops - we skipped over a partial entry: at best,
		 * diroffset should've just matched uio_offset(uio)
		 */
		result = EINVAL;
		goto Err_Exit;
	}

	/* OK, so much for the fakes.  Now for the "real thing": */
	TAILQ_FOREACH(entry, &sp->s_u.d.d_subnodes, s_sibling) {
		if (diroffset == uio_offset(uio)) {
			/* Return this entry */
			diroffset += autofs_adddirentry(entry->s_nodeid,
			    entry->s_type, entry->s_name, entry, uio);
			if (ap->a_numdirent)
				++(*ap->a_numdirent);
		}
		if ((uio_resid(uio) > 0) && (diroffset > uio_offset(uio))) {
			/*
			 * Oops - we skipped over a partial entry:
			 * at best, diroffset should've just matched
			 * uio_offset(uio)
			 */
			result = EINVAL;
			goto Err_Exit;
		}
	}

	if (ap->a_eofflag) {
		/* If we ran all the way through the list, there is no more */
		*ap->a_eofflag = (entry == NULL);
	}

Err_Exit:
	/* Unlock the parent directory: */
	lck_rw_unlock_exclusive(sp->s_lock);
	
	return result;
}

static int
autofs_trigger(vnode_t dp, vnode_t *vpp, struct componentname *cnp, uid_t uid, vfs_context_t context)
{
	vnode_t vp, newvp;
	struct autofsnode *an, *an2;
	int error;
	char *nameptr = NULL;
	long namelen = 0;

	if (cnp) {
		nameptr = cnp->cn_nameptr;
		namelen = cnp->cn_namelen;
	}

	vp = *vpp;
	an = VTOA(vp);

	/*
	 * To avoid spurious triggers (like when Finder browses into /Network/Servers),
	 * we make triggers act like symlinks and respect the FOLLOW flag.  Note
	 * that FOLLOW only affects the last component of the path; intermediate
	 * symlinks are always followed.  This matches the behavior of namei().
	 *
	 * So, if not following, and at the end of the path, don't trigger.
	 */
	if (((an->s_nodeflags & IN_DEFERRED) == 0) &&
		(cnp != NULL) &&
		((cnp->cn_flags & (FOLLOW|ISLASTCN)) == ISLASTCN)) return 0;

	/*
	 * We may be a multi-user node, check and switch to that vnode
	 * if we need to.
	 * Root gets the base node.  (gg semantics)
	 */
	if (an->s_nodeflags & IN_UID) {
		DBG_VOP(("autofs_trigger: begin redirecting to clone node."));
		/* find ourselves. */
		TAILQ_FOREACH(an2, &an->s_clonehd, s_clonelst) {
			if (an2->s_cloneuid == uid) break;
		}
		if (an2 != NULL) {
			/*
			 * A cloned node already exists; switch to the clone
			 */
			DBG_VOP(("autofs_trigger: cloned node exists."));
			if (an2->s_nodeflags & IN_BYPASS) {
				an2 = NULL;
			} else {
				error = autofs_to_vnode(vnode_mount(dp), an2, &newvp);
				if (error) {
					*vpp = NULL;
					return (error);
				}
				DBG_VOP(("autofs_trigger: end redirecting to UID child."));
			}
		} else {
			/*
			 * After this block 'newvp' will have our newly created
			 * vnode, which v_data will point to our new autofsnode.
			 */
			DBG_VOP(("autofs_trigger: creating new UID child."));
			error = autofs_clonenode(vnode_mount(dp), an, &an2, uid, context);
			if (error) return error;
		}

		/* Switch to the cloned node: */
		if (an2 != NULL) {
			VNODE_PUT(vp);
			vp = ATOV(an2);
			*vpp = vp;
			an = an2;
		}
	}

	/* If we are not a trigger, just return. */
	DBG_VOP(("autofs_trigger:(IN_TRIGGER|IN_DEFERRED) %d\n", __LINE__));
	if ((an->s_nodeflags & (IN_TRIGGER|IN_DEFERRED)) == 0) return (0);

	/* If we are the mounter then just return it. */
	DBG_VOP(("autofs_trigger:autofs_proc_is_mounter %d\n", __LINE__));
	if (autofs_proc_is_mounter(vp, vfs_context_proc(context))) return (0);

	/*
	 * We may need to wait for some time after the actual mount
	 * is completed before allowing processes to proceeed.
	 *
	 * For now, though, let calls through the moment the mount's
	 * been done to avoid a deadlock with diskarbitrationd, which
	 * may start doing I/O before the response to the mount
	 * callout is generated:
	 */
	/* If we're mounted on, we're ok. */
	DBG_VOP(("autofs_trigger:VDIR %d\n", __LINE__));
	if ((vnode_vtype(vp) == VDIR) && (vnode_mountedhere(vp) != NULL)) return (0);

	DBG_VOP(("autofs_trigger:IN_MOUNT %d\n", __LINE__));
	if ((an->s_nodeflags & IN_MOUNT) != 0) goto waitforit;

waitforit:
	/* Only trigger request for deferred contents (at most) once: */
	an->s_nodeflags &= ~IN_DEFERRED;

	DBG_VOP(("autofs_trigger: dropping directory lock for autofs_request %p.\n", dp));
	error = autofs_request(dp, vp, nameptr, namelen, context);
	DBG_VOP(("autofs_trigger: reaquiring directory lock %p.\n", dp));
	DBG_VOP(("autofs_trigger: mount request returned %d\n", error));
	if (error && an->s_clonedfrom) {
		/*
		 * Mark this node as a failed mount attempt but return it
		 * to allow access to child nodes:
		 */
		an->s_nodeflags |= IN_BYPASS;

		error = autofs_to_vnode(vnode_mount(dp), an->s_clonedfrom, &newvp);
		if (error == 0) {
			*vpp = newvp;
			VNODE_PUT(vp);
		}
	};
	DBG_VOP(("autofs_trigger: returning %d\n", error));
	return (error);
}


/*
 *# 
 *#% lookup       dvp     L ? ?
 *#% lookup       vpp     - L -
 */
int
autofs_cached_lookup(ap)
	struct vnop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t dp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	u_long nameiop = cnp->cn_nameiop;
	u_long flags = cnp->cn_flags;
//	struct ucred *cred = vfs_context_ucred(ap->a_context);
//	proc_t p = vfs_context_proc(ap->a_context);
	vnode_t *vpp = ap->a_vpp;
	u_int32_t target_vnode_id;
	int error = 0;

	DBG_VOP(("autofs_cached_lookup called, name = %s, namelen = %ld\n",
		ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen));
	if (flags & ISLASTCN) DBG_VOP(("\tISLASTCN is set\n"));

	*vpp = NULL;

	/* Lock the parent directory: */
	lck_rw_lock_shared(VTOA(dp)->s_lock);
	
	if ((flags & ISLASTCN) &&
		(vnode_vfsisrdonly(dp)) &&
		((nameiop == DELETE) || (nameiop == RENAME))) {
		error = EROFS;
		goto Err_Exit;
	}

	/*
	 * Look up an entry in the namei cache
	 */
	error = cache_lookup(dp, vpp, cnp);
	if (error == 0) {
		/* There was no entry in the cache for this parent
		 * vnode/name pair:
		 * do the full-blown pathname lookup
		 */
		error = autofs_lookup_int(dp, vpp, cnp, ap->a_context);
		goto Err_Exit;
	}
	if (error == ENOENT) goto Err_Exit;

	/* An entry matching the parent vnode/name was found in the cache: */
	target_vnode_id = vnode_vid(*vpp);
	error = vnode_getwithvid(*vpp, target_vnode_id);
	if (error == 0) {
		error = autofs_trigger(dp, vpp, cnp, vfs_context_ucred(ap->a_context)->cr_uid, ap->a_context);
		if (error) {
			VNODE_PUT(*vpp);
			goto Err_Exit;
		}
	}

Err_Exit:
	if (autofs_debug && error == 0) {
		if (ap->a_dvp != NULL) vprint("cachedlookup parent", ap->a_dvp);
		if (*vpp != NULL) vprint("cachedlookup error", *vpp);
	};
	
	/* Unlock the parent directory: */
	lck_rw_unlock_shared(VTOA(dp)->s_lock);

	return (error);
}

int
autofs_to_vnode(mp, an, vpp)
	mount_t mp;
	struct autofsnode *an;
	vnode_t *vpp;
{
	int error;

	*vpp = ATOV(an);
	if (*vpp == NULL) {
		enum vtype vtype;

		switch (an->s_type) {
		  case AUTOFS_DIRECTORY:
			vtype = VDIR;
			break;
		  case AUTOFS_SYMLINK:
			vtype = VLNK;
			break;
		  default:
			vtype = VBAD;
			panic("autofs_to_vnode: unknown node type %d", an->s_type);
			break;
		}
		
		error = autofs_getnewvnode(mp, an, vtype, an->s_nodeid, vpp);
		if (error) {
			panic("autofs_to_vnode getvnode %d", error);
		}
	} else {
		error = VNODE_GET(*vpp);
	}
	return (error);
}

/*
 * Check to see if we are the person serving the trigger, if so
 * return true.
 * We do this by walking the ancestor tree looking for the pid recorded
 * in the request structure hung off the autofsnode.
 */
static int
autofs_proc_is_mounter(vp, p)
	vnode_t vp;
	proc_t p;
{
	struct autofsnode *an;

	an = VTOA(vp);
	if (an->s_mounterpid == 0) {
		/*
		 * If we get here, the vnode is a trigger, but has no mounter.
		 * This should never happen.  There must be a mounter, or else
		 * there is no way to actually do the automount (the process
		 * doing the mount() call would trigger the automount).
		 *
		 * If we ever do get here, just prevent the trigger (to avoid
		 * deadlock) and log a message.
		 */
		printf("autofs: No mounter for ino=%d, name=%s, uid=%d\n",
			an->s_nodeid, an->s_name, an->s_cloneuid);
		return 1;
	}

	/* Is process p the mounter, or one of its children? */
	return(proc_pid(p) == an->s_mounterpid || proc_isinferior(proc_pid(p), an->s_mounterpid));

}


static int
autofs_lookup_int(vnode_t dp, vnode_t *vpp, struct componentname *cnp, vfs_context_t context)
{
	u_long nameiop = cnp->cn_nameiop;
	u_long flags = cnp->cn_flags;
	long namelen = cnp->cn_namelen;
	struct autofsnode *entry;
	vnode_t vp = NULL;
	int error = 0;
	boolean_t found = FALSE;
	boolean_t isDot = FALSE;
	boolean_t isDotDot = FALSE;
	vnode_t starting_parent = dp;
	struct autofsnode *dsp = VTOA(dp);
	vnode_t pdp;

	DBG_VOP(("autofs_lookup called, name = %s, namelen = %ld\n",
		cnp->cn_nameptr, cnp->cn_namelen));
	if (flags & ISLASTCN) DBG_VOP(("\tISLASTCN is set\n"));

	*vpp = NULL;

	if (namelen > NAME_MAX) {
		error = ENAMETOOLONG;
		goto Err_Exit;
	}
	
Lookup_start:

	/* first check for "." and ".." */
	if (cnp->cn_nameptr[0] == '.') {
		if (namelen == 1) {
			/*
			   "." requested
			 */
			isDot = TRUE;
			found = TRUE;

			vp = dp;
			VNODE_GET(vp);

			error = 0;

			goto Std_Exit;
		} else if ((namelen == 2) && (cnp->cn_nameptr[1] == '.')) {
			/* ".." requested */
			isDotDot = TRUE;
			found = TRUE;

			if ((PARENTNODE(dsp) != NULL) && (PARENTNODE(dsp) != VTOA(dp))) {
				error = autofs_to_vnode(vnode_mount(dp), PARENTNODE(dsp), &vp);
				if (error != 0) goto Err_Exit;
			} else {
				vp = dp;		/* dp is alread locked and ref'ed */
				error = vnode_get(vp);
				if (error != 0) goto Err_Exit;
			}

			goto Std_Exit;
		}
	}

	/*
	 * finally, just look for entries by name
	 * (making sure the entry's length matches the cnp's namelen...
	 */
	TAILQ_FOREACH(entry, &dsp->s_u.d.d_subnodes, s_sibling) {
		if ((bcmp(cnp->cn_nameptr, entry->s_name, (unsigned)namelen) == 0) &&
			(*(entry->s_name + namelen) == '\0')) {
			found = TRUE;
			error = autofs_to_vnode(vnode_mount(dp), entry, &vp);
			if (error != 0) goto Err_Exit;
			break;
		}
	}
	
	if ((nameiop == LOOKUP) && (entry == NULL)) {
		/* Didn't find entry; see if parent contents are deferred. */
		if (!(dsp->s_nodeflags & IN_DEFERRED)) goto Std_Exit;
		
		/* Generation of this directory's contents was deferred:
		   Locate the parent's parent directory and trigger a request
		   for this parent directory.
		 */
		if ((PARENTNODE(dsp) != NULL) && (PARENTNODE(dsp) != VTOA(dp))) {
			error = autofs_to_vnode(vnode_mount(dp), PARENTNODE(dsp), &pdp);
			if (error != 0) goto Err_Exit;
		} else {
			pdp = dp;		/* dp is alread locked and ref'ed */
		}
		error = autofs_trigger(pdp, &dp, cnp, vfs_context_ucred(context)->cr_uid, context);
		if (error) {
			VNODE_PUT(vp);
			goto Err_Exit;
		}
		
		/* The directory contents are now final: try the lookup again. */
		goto Lookup_start;
	}

	/* If we found something, try to trigger */
	if (found) {
		error = autofs_trigger(dp, &vp, cnp, vfs_context_ucred(context)->cr_uid, context);
		if (error) {
			VNODE_PUT(vp);
			goto Err_Exit;
		}
	}

Std_Exit:
	if (found) {
		if ((nameiop == DELETE) && (flags & ISLASTCN)) {
			/* mjs - was authorisation checks here only */
		}

		if ((nameiop == RENAME) && (flags & WANTPARENT) &&
		    (flags & ISLASTCN)) {

			if (isDot) {
				VNODE_PUT(vp);
				error = EISDIR;
				goto Err_Exit;
			}
		}
	} else {
		/* The specified entry wasn't found: */
		error = ENOENT;

		if ((flags & ISLASTCN) &&
		    ((nameiop == CREATE) ||
		     (nameiop == RENAME) ||
		     ((nameiop == DELETE) && (flags & DOWHITEOUT) &&
		      (flags & ISWHITEOUT)))) {

			error = EJUSTRETURN;
		}
	}

	*vpp = vp;

Err_Exit:
	DBG_VOP(("autofs_lookup: error = %d, vp = %p, found = %d.\n",
		error, vp, found));
	DBG_VOP(("autofs_lookup: dp = %p; starting_parent = %p.\n",
		dp, starting_parent));
	return error;
}

/*
 *# 
 *#% lookup       dvp     L ? ?
 *#% lookup       vpp     - L -
 */
int
autofs_lookup(ap)
	struct vnop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	errno_t result;
	
	lck_rw_lock_shared(VTOA(ap->a_dvp)->s_lock);
	
	result = autofs_lookup_int(ap->a_dvp, ap->a_vpp, ap->a_cnp, ap->a_context);

	lck_rw_unlock_shared(VTOA(ap->a_dvp)->s_lock);
	
	return result;
}

/*

#% pathconf	vp	L L L
#
 vnop_pathconf {
	 IN vnode_t vp;
	 IN int name;
	 OUT register_t *retval;
	 IN vfs_context_t context;
*/
int
autofs_pathconf(ap)
struct vnop_pathconf_args /* {
	vnode_t a_vp;
	int a_name;
	int *a_retval;
	vfs_context_t a_context;
} */ *ap;
{
	DBG_VOP(("autofs_pathconf called\n"));

	switch (ap->a_name)
	{
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}


/*
 * Update the access, modified, and node change times as specified by the
 * IACCESS, IUPDATE, and ICHANGE flags respectively. The IMODIFIED flag is
 * used to specify that the node needs to be updated but that the times have
 * already been set. The access and modified times are taken from the second
 * and third parameters; the node change time is always taken from the current
 * time. If waitfor is set, then wait for the disk write of the node to
 * complete.
 */

int
autofs_update(vnode_t vp, struct timeval *access, struct timeval *modify, int waitfor)
{
	struct autofsnode *sp = VTOA(vp);
	
	DBG_ASSERT(sp != NULL);

	if (((sp->s_nodeflags & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) != 0) &&
	    !(vnode_vfsisrdonly(vp))) {
		if (sp->s_nodeflags & IN_ACCESS) sp->s_accesstime = *access;
		if (sp->s_nodeflags & IN_UPDATE) sp->s_modificationtime = *modify;
		if (sp->s_nodeflags & IN_CHANGE) microtime(&sp->s_changetime);
	}

	/* After the updates are finished, clear the flags */
	sp->s_nodeflags &= ~(IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE);

	return (0);
}



/*
#
#% inactive	vp	L U U
#
 vnop_inactive {
	 IN vnode_t vp;
	 IN proc_t p;
	 IN vfs_context_t context;
*/

int
autofs_inactive(ap)
struct vnop_inactive_args /* {
	vnode_t a_vp;
	proc_t a_p;
	vfs_context_t a_context;
} */ *ap;
{
	vnode_t vp = ap->a_vp;
//	proc_t p = vfs_context_proc(ap->a_context);
	struct autofsnode *sp = VTOA(vp);
	struct timeval tv;

	if (autofs_debug) vprint("autofs_inactive", vp);

	/*
	 * Ignore nodes related to stale file handles.
	 */
	if (vnode_vtype(vp) == VNON) {
		DBG_VOP(("autofs_inactive: VNON!\n"));
		goto out;
	}

	/* This is sort of silly but might make sense in the future: */
	if (sp->s_nodeflags &
	    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) {
		lck_rw_lock_exclusive(sp->s_lock);
		microtime(&tv);
		autofs_update(vp, &tv, &tv, 0);
		lck_rw_unlock_exclusive(sp->s_lock);
	}

out:
	DBG_VOP(("autofs_inactive: linkcnt = %d\n", sp->s_linkcount));

	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (vnode_vtype(vp) == VNON) {
		vnode_recycle(vp);
	}

	return (0);
}


/*
 *#
 *#% reclaim      vp      U U U
 *#
 */
int
autofs_reclaim(ap)
	struct vnop_reclaim_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		vfs_context_t a_context;
} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct autofsnode *an = VTOA(vp);

	if (autofs_debug) {
		vprint("autofs_reclaim", vp);
		printf("s_linkcount = %d\n", an->s_linkcount);
	}
	if (an->s_linkcount < 0)
		panic("autofs_reclaim: negative linkcount for '%s'",  an->s_name);

	if (an->s_linkcount > 0) {
		DBG_VOP(("autofs_reclaim: s_linkcount = %d, NULLing vnode ptr\n", an->s_linkcount));
		an->s_vp = NULL;
		return (0);
	}

	DBG_VOP(("autofs_reclaim: s_linkcount = 0, freeing autofs node\n"));

	an = VTOA(vp);
	autofs_destroynode(an);

	return (0);
}

/*
 * On entry:
 *	source's parent directory is unlocked
 *	source file or directory is unlocked
 *	destination's parent directory is locked
 *	destination file or directory is locked if it exists
 *
 * On exit:
 *	all denodes should be released
 *
 */
int
autofs_rename(ap)
	struct vnop_rename_args  /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_fdvp;
		vnode_t a_fvp;
		struct componentname *a_fcnp;
		vnode_t a_tdvp;
		vnode_t a_tvp;
		struct componentname *a_tcnp;
		vfs_context_t a_context;
	} */ *ap;
{
	/* we don't support renames. */
	return err_rename(ap);
}

int
autofs_print(struct autofsnode *ap)
{
	char *name = ap->s_name;

	printf("name: %s, linkcount %d\n",
	    name != NULL && *name == '\0' ? "<<ROOT>>" : name,
	    ap->s_linkcount);
	return (0);
}

int 
autofs_getxattr(ap)
 	struct vnop_getxattr_args /* {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	char * a_name;
	uio_t a_uio;
	size_t *a_size;
	int a_options;
	vfs_context_t a_context;
}; */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct autofsnode *sp = VTOA(vp);
	struct uio *uio = ap->a_uio;
	size_t attrsize;
	int retval = 0;

	/* do not support position argument */
	if (uio_offset(uio) != 0) {
		retval = EINVAL;
		goto out;
	}

	/* network category xattr */
	if (strncmp(ap->a_name, XATTR_CATEGORYNAME, XATTR_CATEGORYNAMELEN) == 0) {
		
		lck_rw_lock_shared(sp->s_lock);

		/* check if xattr exists */
		if (!(sp->xattr_data.xattr_category)) {
			retval = ENOATTR;
			goto out_unlock;
		}

		/* return the size of attribute */
		*ap->a_size = XATTR_CATEGORY_MAXSIZE;
		
		/* if uio is NULL, return the size of xattr */
		if (uio == NULL) {
			goto out_unlock;
		}
		
		/* check attribute size being passed */
		attrsize = uio_resid(uio);
		if (attrsize < XATTR_CATEGORY_MAXSIZE) {
			retval = ERANGE;
			goto out_unlock;
		}

		/* copy out attribute data */
		retval = uiomove((caddr_t)&(sp->xattr_data.xattr_category), attrsize, uio);

	} else {
		/* autofs only supports network category xattr */
		retval = ENOATTR;
		goto out;
	}

out_unlock:
	lck_rw_unlock_shared(sp->s_lock);
	
out:
	return (retval);
}

int 
autofs_setxattr(ap)
	struct vnop_setxattr_args /* {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	char * a_name;
	uio_t a_uio;
	int a_options;
	vfs_context_t a_context;
}; */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct autofsnode *sp = VTOA(vp);
	struct uio *uio = ap->a_uio;
	struct autofs_xattr_data new_xattr_data;
	size_t attrsize;
	int retval = 0;

	/* network category xattr */
	if (strncmp(ap->a_name, XATTR_CATEGORYNAME, XATTR_CATEGORYNAMELEN) == 0) {

		/* do not support position argument */
		if (uio_offset(uio) != 0) {
			retval = EINVAL;
			goto out;
		}

		/* check attribute size being passed */
		attrsize = uio_resid(uio);
		if (attrsize != XATTR_CATEGORY_MAXSIZE) {
			retval = EINVAL;
			goto out;
		}

		lck_rw_lock_exclusive(sp->s_lock);
		
		/* create new xattr option and xattr exists */
		if ((ap->a_options & XATTR_CREATE) && sp->xattr_data.xattr_category) {
			retval = EEXIST;
			goto out_unlock;
		}
		
		/* replace old xattr and xattr does not exist */
		if ((ap->a_options & XATTR_REPLACE) && !(sp->xattr_data.xattr_category)) {
			retval = ENOATTR;
			goto out_unlock;
		}

		/* copy in attribute data */
		retval = uiomove((caddr_t) &new_xattr_data.xattr_category, attrsize, uio);
		if (retval) {
			goto out_unlock;
		}

		/* set the attribute */
		sp->xattr_data.xattr_category = new_xattr_data.xattr_category;

	} else {
		/* autofs only supports network category xattr */
		retval = EPERM;
		goto out;
	}

out_unlock:
	lck_rw_unlock_exclusive(sp->s_lock);
	
out:
	return (retval);
}

int
autofs_removexattr(ap)
	struct vnop_removexattr_args /* {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	char * a_name;
	int a_options;
	vfs_context_t a_context;
}; */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct autofsnode *sp = VTOA(vp);
	int retval = 0;

	/* network category xattr */
	if (strncmp(ap->a_name, XATTR_CATEGORYNAME, XATTR_CATEGORYNAMELEN) == 0) {

		lck_rw_lock_shared(sp->s_lock);

		/* check if xattr exists */
		if (!(sp->xattr_data.xattr_category)) {
			retval = ENOATTR;
			goto out_unlock;
		}

		sp->xattr_data.xattr_category = 0;
		
	} else {
		/* autofs only supports network category xattr */
		retval = ENOATTR;
		goto out;
	}

out_unlock:
	lck_rw_unlock_shared(sp->s_lock);
	
out:
	return (retval);
}

int 
autofs_listxattr(ap)
	struct vnop_listxattr_args /* {
	struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	uio_t a_uio;
	size_t *a_size;
	int a_options;
	vfs_context_t a_context;
}; */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct autofsnode *sp = VTOA(vp);
	struct uio *uio = ap->a_uio;
	int retval = 0;
	
	*ap->a_size = 0;
	
	/* return zero if no extended attributes defined */
	if (!sp->xattr_data.xattr_category) {
		retval = 0;
		goto out;
	}

	lck_rw_lock_shared(sp->s_lock);

	/* return network.category */
	if (uio == NULL) {
		*ap->a_size += XATTR_CATEGORYNAMELEN;		
	} else if (uio_resid(uio) < XATTR_CATEGORYNAMELEN) {
		retval = ERANGE;
		goto out_unlock;
	} else {
		retval = uiomove((caddr_t) XATTR_CATEGORYNAME, XATTR_CATEGORYNAMELEN, uio);
		if (retval) {
			retval = ERANGE;
			goto out_unlock;
		}
	}

out_unlock:
	lck_rw_unlock_shared(sp->s_lock);
	
out:
	return (retval);
}

