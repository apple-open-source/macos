/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

#pragma ident	"@(#)auto_vnops.c	1.70	05/12/19 SMI"

#include <mach/mach_types.h>
#include <mach/machine/boolean.h>
#include <mach/host_priv.h>
#include <mach/host_special_ports.h>
#include <mach/vm_map.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

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
#include <sys/vfs_context.h>
#include <sys/vm.h>
#include <sys/errno.h>
#include <vfs/vfs_support.h>
#include <sys/uio.h>
#include <sys/xattr.h>
#include <sys/syslog.h>

#include <kern/assert.h>
#include <kern/host.h>

#include "autofs.h"
#include "autofs_kern.h"
#include "autofs_protUser.h"

/*
 *  Vnode ops for autofs
 */
static int auto_getattr(struct vnop_getattr_args *);
static int auto_setattr(struct vnop_setattr_args *);
static int auto_lookup(struct vnop_lookup_args *);
static int auto_readdir(struct vnop_readdir_args *);
static int auto_readlink(struct vnop_readlink_args *);
static int auto_pathconf(struct vnop_pathconf_args *);
static int auto_getxattr(struct vnop_getxattr_args *);
static int auto_setxattr(struct vnop_setxattr_args *);
static int auto_listxattr(struct vnop_listxattr_args *);
static int auto_removexattr(struct vnop_removexattr_args *);
static int auto_reclaim(struct vnop_reclaim_args *);
static int auto_kqfilt_add(struct vnop_kqfilt_add_args *);
static int auto_kqfilt_remove(struct vnop_kqfilt_remove_args *);

int (**autofs_vnodeop_p)(void *);

#define VOPFUNC int (*)(void *)

struct vnodeopv_entry_desc autofs_vnodeop_entries[] = {
	{&vnop_default_desc, (VOPFUNC)vn_default_error},
	{&vnop_lookup_desc, (VOPFUNC)auto_lookup},		/* lookup */
	{&vnop_open_desc, (VOPFUNC)nop_open},			/* open	- NOP */
	{&vnop_close_desc, (VOPFUNC)nop_close},			/* close - NOP */
	{&vnop_getattr_desc, (VOPFUNC)auto_getattr},		/* getattr */
	{&vnop_setattr_desc, (VOPFUNC)auto_setattr},		/* setattr */
	{&vnop_fsync_desc, (VOPFUNC)nop_fsync},			/* fsync - NOP */
	{&vnop_readdir_desc, (VOPFUNC)auto_readdir},		/* readdir */
	{&vnop_readlink_desc, (VOPFUNC)auto_readlink},		/* readlink */
	{&vnop_pathconf_desc, (VOPFUNC)auto_pathconf},		/* pathconf */
	{&vnop_setxattr_desc, (VOPFUNC)auto_setxattr},		/* setxattr */
	{&vnop_getxattr_desc, (VOPFUNC)auto_getxattr},		/* getxattr */
	{&vnop_listxattr_desc, (VOPFUNC)auto_listxattr},	/* listxattr */
	{&vnop_removexattr_desc, (VOPFUNC)auto_removexattr},	/* removexattr */
	{&vnop_inactive_desc, (VOPFUNC)nop_inactive},		/* inactive - NOP */
	{&vnop_reclaim_desc, (VOPFUNC)auto_reclaim},		/* reclaim */
	{&vnop_kqfilt_add_desc, (VOPFUNC)auto_kqfilt_add},
	{&vnop_kqfilt_remove_desc, (VOPFUNC)auto_kqfilt_remove},
	{NULL, NULL}
};

struct vnodeopv_desc autofsfs_vnodeop_opv_desc =
	{ &autofs_vnodeop_p, autofs_vnodeop_entries };

extern struct vnodeop_desc vnop_remove_desc;

static uint32_t
auto_bsd_flags(fnnode_t *fnp, vfs_context_t context)
{
	vnode_t vp = fntovn(fnp);
	fninfo_t *fnip = vfstofni(vnode_mount(vp));
	uint32_t flags = 0;
	boolean_t istrigger;
	int error;

	/*
	 * Do we know whether this is a trigger?
	 */
	if (fnp->fn_istrigger == FN_TRIGGER_UNKNOWN) {
		/*
		 * No.  Are we an automounter?
		 */
		if (auto_is_automounter(vfs_context_pid(context))) {
			/*
			 * We are.  automountd doesn't care
			 * whether this is a trigger or not,
			 * and we don't want to have to call
			 * back to automountd to check whether
			 * this is a trigger.  Just say it's not.
			 */
			return (0);
		}

		/*
		 * No, so let's find out if this is a trigger.
		 * XXX - the answer might be different in the future if
		 * maps change.
		 */
		auto_fninfo_lock_shared(fnip, context);
		if (vp == fnip->fi_rootvp) {
			/*
			 * This vnode is the root of a mount.
			 */
			if (fnip->fi_flags & MF_DIRECT) {
				/*
				 * It's the root of a direct map; check
				 * whether it's a trigger or not.
				 */
				error = auto_check_trigger_request(fnip,
				    ".", 1, &istrigger);
				if (error) {
					/*
					 * We got an error; say it's not a
					 * trigger.
					 */
					fnp->fn_istrigger = FN_ISNT_TRIGGER;
				} else {
					if (istrigger)
						fnp->fn_istrigger = FN_IS_TRIGGER;
					else
						fnp->fn_istrigger = FN_ISNT_TRIGGER;
				}
			} else {
				/*
				 * It's the root of an indirect map, so
				 * it's not a trigger.  (The triggers are
				 * below the root.)
				 */
				fnp->fn_istrigger = FN_ISNT_TRIGGER;
			}
		} else {
			/*
			 * This vnode isn't the root of a mount.
			 */
			if (fnip->fi_flags & MF_DIRECT) {
				/*
				 * XXX - we don't usually see these;
				 * what should we do if we do see one?
				 */
				fnp->fn_istrigger = FN_ISNT_TRIGGER;
			} else {
				/*
				 * Check whether it's a trigger.
				 */
				error = auto_check_trigger_request(fnip,
				    fnp->fn_name, fnp->fn_namelen, &istrigger);
				if (error) {
					/*
					 * We got an error; say it's not a
					 * trigger.
					 */
					fnp->fn_istrigger = FN_ISNT_TRIGGER;
				} else {
					if (istrigger)
						fnp->fn_istrigger = FN_IS_TRIGGER;
					else
						fnp->fn_istrigger = FN_ISNT_TRIGGER;
				}
			}
		}
		auto_fninfo_unlock_shared(fnip, context);
	}
	switch (fnp->fn_istrigger) {

	case FN_TRIGGER_UNKNOWN:
		log(LOG_ERR, "Trigger status of %s unknown\n", fnp->fn_name);
		/* say it's not */
		flags = 0;
		break;

	case FN_IS_TRIGGER:
		flags = SF_AUTOMOUNT;
		break;

	case FN_ISNT_TRIGGER:
		flags = 0;
		break;
	}
	return (flags);
}

static int
auto_getattr(ap)
	struct vnop_getattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
	fnnode_t *fnp = vntofn(vp);

	AUTOFS_DPRINT((4, "auto_getattr vp %p\n", (void *)vp));

	/* XXX - lock? */

	VATTR_RETURN(vap, va_rdev, 0);
	switch (vnode_vtype(vp)) {
	case VDIR:
		/*
		 * fn_linkcnt doesn't count the "." link, as we're using it
		 * as a count of references to the fnnode from other vnnodes
		 * (so that if it goes to 0, we know no other fnnode refers
		 * to it).
		 */
		VATTR_RETURN(vap, va_nlink, fnp->fn_linkcnt + 1);

		/*
		 * The "size" of a directory is the number of entries
		 * in its list of directory entries, plus 1.
		 * (Solaris added 1 for some reason.)
		 */
		VATTR_RETURN(vap, va_data_size, fnp->fn_direntcnt + 1);
		break;
	case VLNK:
		VATTR_RETURN(vap, va_nlink, fnp->fn_linkcnt);
		VATTR_RETURN(vap, va_data_size, fnp->fn_symlinklen);
		break;
	default:
		VATTR_RETURN(vap, va_nlink, fnp->fn_linkcnt);
		VATTR_RETURN(vap, va_data_size, 0);
		break;
	}
	VATTR_RETURN(vap, va_total_size, roundup(vap->va_data_size, AUTOFS_BLOCKSIZE));
	VATTR_RETURN(vap, va_iosize, AUTOFS_BLOCKSIZE);
	VATTR_RETURN(vap, va_uid, fnp->fn_uid);
	VATTR_RETURN(vap, va_gid, fnp->fn_gid);
	VATTR_RETURN(vap, va_mode, fnp->fn_mode);
	/*
	 * Does our caller want the BSD flags?
	 */
	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		if (vnode_isdir(vp)) {
			/*
			 * Find out what the flags are for this directory,
			 * based on whether it's a trigger or not.
			 */
			VATTR_RETURN(vap, va_flags,
			    auto_bsd_flags(fnp, ap->a_context));
		} else {
			/*
			 * Non-directories are never triggers.
			 */
			VATTR_RETURN(vap, va_flags, 0);
		}
	}
	vap->va_access_time.tv_sec = fnp->fn_atime.tv_sec;
	vap->va_access_time.tv_nsec = fnp->fn_atime.tv_usec * 1000;
	VATTR_SET_SUPPORTED(vap, va_access_time);
	vap->va_modify_time.tv_sec = fnp->fn_mtime.tv_sec;
	vap->va_modify_time.tv_nsec = fnp->fn_mtime.tv_usec * 1000;
	VATTR_SET_SUPPORTED(vap, va_modify_time);
	vap->va_change_time.tv_sec = fnp->fn_ctime.tv_sec;
	vap->va_change_time.tv_nsec = fnp->fn_ctime.tv_usec * 1000;
	VATTR_SET_SUPPORTED(vap, va_change_time);
	VATTR_RETURN(vap, va_fileid, fnp->fn_nodeid);
	VATTR_RETURN(vap, va_fsid, vfs_statfs(vnode_mount(vp))->f_fsid.val[0]);
	VATTR_RETURN(vap, va_filerev, 0);
	VATTR_RETURN(vap, va_type, vnode_vtype(vp));

	return (0);
}

static int
auto_setattr(ap)
	struct vnop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
	fnnode_t *fnp = vntofn(vp);

	AUTOFS_DPRINT((4, "auto_setattr vp %p\n", (void *)vp));

	/*
	 * Only root or the owner can change the attributes.
	 */

	/*
	 * All you can set are the UID, the GID, and the permissions;
	 * that's to allow the automounter to give the mount point to
	 * the user on whose behalf we're doing the mount, and make it
	 * writable by them, so we can do AFP and SMB mounts as that user
	 * (so the connection can be authenticated as them).
	 */
	VATTR_SET_SUPPORTED(vap, va_uid);
	if (VATTR_IS_ACTIVE(vap, va_uid))
		fnp->fn_uid = vap->va_uid;
	VATTR_SET_SUPPORTED(vap, va_gid);
	if (VATTR_IS_ACTIVE(vap, va_gid))
		fnp->fn_gid = vap->va_gid;
	VATTR_SET_SUPPORTED(vap, va_mode);
	if (VATTR_IS_ACTIVE(vap, va_mode))
		fnp->fn_mode = vap->va_mode & ALLPERMS;

	AUTO_KNOTE(vp, NOTE_ATTRIB);

	return (0);
}

static int
map_is_fstab(fnnode_t *dfnp)
{
	static const char fstab[] = "-fstab";
	struct fninfo *fnip = vfstofni(vnode_mount(fntovn(dfnp)));

	if (fnip->fi_maplen == sizeof fstab &&
	    bcmp(fnip->fi_map, fstab, fnip->fi_maplen) == 0)
		return (1);
	return (0);
}

static boolean_t
name_is_us(struct componentname *cnp)
{
	mach_port_t automount_port;
	int error;
	kern_return_t ret;
	boolean_t is_us;

	error = auto_get_automountd_port(&automount_port);
	if (error)
		return (0);
	ret = autofs_check_thishost(automount_port, cnp->cn_nameptr,
	    cnp->cn_namelen, &is_us);
	auto_release_automountd_port(automount_port);
	if (ret == KERN_SUCCESS)
		return (is_us);
	else
		return (0);
}

#define OP_NOTHING	-1
#define OP_LOOKUP	0
#define OP_MOUNT	1

static int
auto_lookup(ap)
	struct vnop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	vnode_t vp;
	struct componentname *cnp = ap->a_cnp;
	u_long nameiop = cnp->cn_nameiop;
	u_long flags = cnp->cn_flags;
	int namelen = cnp->cn_namelen;
	vfs_context_t context = ap->a_context;
	ucred_t cred = vfs_context_ucred(context);
	int error = 0;
	fninfo_t *dfnip;
	fnnode_t *dfnp = NULL;
	fnnode_t *fnp = NULL;
	char *searchnm;
	int searchnmlen;
	int operation;		/* OP_NOTHING, OP_LOOKUP, or OP_MOUNT */

	dfnip = vfstofni(vnode_mount(dvp));
	AUTOFS_DPRINT((3, "auto_lookup: dvp=%p (%s) name=%.*s\n",
	    (void *)dvp, dfnip->fi_map, namelen, cnp->cn_nameptr));

	/* This must be a directory. */
	if (!vnode_isdir(dvp))
		return (ENOTDIR);

	/*
	 * XXX - is this necessary?
	 */
	if (namelen == 0) {
		error = vnode_get(dvp);
		if (error)
			return (error);
		*vpp = dvp;
		return (0);
	}

	/* first check for "." and ".." */
	if (cnp->cn_nameptr[0] == '.') {
		if (namelen == 1) {
			/*
			 * "." requested
			 */

			/*
			 * Thou shalt not rename ".".
			 * (No, the VFS layer doesn't catch this for us.)
			 */
			if ((nameiop == RENAME) && (flags & WANTPARENT) &&
			    (flags & ISLASTCN))
				return (EISDIR);

			error = vnode_get(dvp);
			if (error)
				return (error);
			*vpp = dvp;
			return (0);
		} else if ((namelen == 2) && (cnp->cn_nameptr[1] == '.')) {
			fnnode_t *pdfnp;

			pdfnp = (vntofn(dvp))->fn_parent;
			assert(pdfnp != NULL);

			/*
			 * Since it is legitimate to have the VROOT flag set for the
			 * subdirectories of the indirect map in autofs filesystem,
			 * rootfnnodep is checked against fnnode of dvp instead of
			 * just checking whether VROOT flag is set in dvp
			 */

#if 0
			if (pdfnp == pdfnp->fn_globals->fng_rootfnnodep) {
				vnode_t vp;

				vfs_lock_wait(vnode_mount(dvp));
				if (vnode_mount(dvp)->vfs_flag & VFS_UNMOUNTED) {
					vfs_unlock(vnode_mount(dvp));
					return (EIO);
				}
				vp = vnode_mount(dvp)->mnt_vnodecovered;
				error = vnode_get(vp);	/* XXX - what if it fails? */
				vfs_unlock(vnode_mount(dvp));
				error = VNOP_LOOKUP(vp, nm, vpp, pnp, flags, rdir, cred);
				vnode_put(vp);
				return (error);
			} else {
#else
			{
#endif
				*vpp = fntovn(pdfnp);
				return (vnode_get(*vpp));
			}
		}
	}

top:
	dfnp = vntofn(dvp);
	searchnm = cnp->cn_nameptr;
	searchnmlen = namelen;

	AUTOFS_DPRINT((3, "auto_lookup: dvp=%p dfnp=%p\n", (void *)dvp,
	    (void *)dfnp));

	/*
	 * If a lookup of or mount on the directory in which we're doing
	 * the lookup is in progress, wait for it to finish, and, if it
	 * got an error, return that error.
	 */
	lck_mtx_lock(dfnp->fn_lock);
	if (dfnp->fn_flags & (MF_LOOKUP | MF_INPROG)) {
		lck_mtx_unlock(dfnp->fn_lock);
		error = auto_wait4mount(dfnp, context);
		if (error == EAGAIN)
			goto top;
		if (error)
			return (error);
	} else
		lck_mtx_unlock(dfnp->fn_lock);

	auto_fninfo_lock_shared(dfnip, context);

	/*
	 * See if we have done something with this name already, so we
	 * already have it.
	 */
	lck_rw_lock_shared(dfnp->fn_rwlock);
	fnp = auto_search(dfnp, cnp->cn_nameptr, cnp->cn_namelen);
	if (fnp == NULL) {
		/*
		 * No, we don't.
		 */
		error = ENOENT;
		if (dvp == dfnip->fi_rootvp) {
			/*
			 * 'dfnp', i.e. the directory in which we're doing
			 * a lookup, is the root directory of an autofs
			 * mount.
			 */
			if (dfnip->fi_flags & MF_DIRECT) {
				/*
				 * That mount is for a direct map.
				 *
				 * Any attempt to do a lookup on something
				 * in this directory should have triggered
				 * a mount on it; presumably the mount
				 * failed or only caused triggers to be
				 * planted on subdirectories of it.
				 *
				 * That means that the only stuff in this
				 * directory is the stuff we put there, and
				 * there's nothing we can get from automountd,
				 * so just return "not found".
				 */
				error = ENOENT;
			} else {
				/*
				 * That mount is for an indirect map.
				 * Therefore, the item we're looking up
				 * is a key in that map.  We need to
				 * create an entry for it.
				 *
				 * First, check whether this map is
				 * in the process of being unmounted.
				 * If so, return ENOENT; see
				 * auto_control_ioctl() for the reason
				 * why this is done.
				 */
				if (dfnip->fi_flags & MF_UNMOUNTING) {
					error = ENOENT;
					goto fail;
				}

				/*
				 * Create the fnnode first, as we must
				 * drop the writer lock before creating
				 * the fnnode, because allocating a
				 * vnode for it might involve reclaiming
				 * an autofs vnode and hence removing
				 * it from the containing directory -
				 * which might be this directory.
				 */
				lck_rw_unlock_shared(dfnp->fn_rwlock);

				/*
				 * If we're looking up an entry
				 * in the -fstab map, and the
				 * name we're looking up refers
				 * to us, just make this a
				 * symlink to "/".
				 *
				 * XXX - passed kcred in Solaris.
				 */
				if (map_is_fstab(dfnp) &&
				    name_is_us(cnp)) {
					char *tmp;

					error = auto_makefnnode(&fnp, VLNK,
					    vnode_mount(dvp), cnp, NULL, dvp,
					    0, cred, dfnp->fn_globals);
					if (error)
						goto fail;

					/*
					 * Set the symlink target information.
					 */
					MALLOC(tmp, char *, 1, M_AUTOFS,
					    M_WAITOK);
					bcopy("/", tmp, 1);
					fnp->fn_symlink = tmp;
					fnp->fn_symlinklen = 1;
				} else {
					error = auto_makefnnode(&fnp, VDIR,
					    vnode_mount(dvp), cnp, NULL, dvp,
					    0, cred, dfnp->fn_globals);
					if (error)
						goto fail;
				}

				/*
				 * Now re-acquire the writer lock, and
				 * enter the fnnode in the directory.
				 *
				 * Note that somebody might have
				 * created the name while we weren't
				 * holding the lock; if so, then if
				 * it could get the vnode for the fnnode
				 * for that name, auto_enter will return
				 * EEXIST, otherwise it'll return the error
				 * it got when trying to get the vnode.
				 */
				lck_rw_lock_exclusive(dfnp->fn_rwlock);
				error = auto_enter(dfnp, cnp, NULL, &fnp);
				if (error) {
					if (error == EEXIST) {
						/*
						 * We found the name, and
						 * got the vnode for its
						 * fnnode.  Act as if
						 * the auto_search()
						 * above succeeded.
						 */
						error = 0;
					} else {
						/*
						 * We found the name, but
						 * but couldn't get the
						 * vnode for its fnnode.
						 * That's probably because
						 * it was on its way to
						 * destruction; release
						 * all locks and start again
						 * from the top.
						 */
						error = EAGAIN;
					}
				}
			}
		} else if ((dfnp->fn_dirents == NULL) &&
		    !vnode_isvroot(dvp) &&
		    vnode_isvroot(fntovn(dfnp->fn_parent))) {
			/*
			 * dfnp is a directory under the mount point
			 * of an indirect map, so that it's a directory
			 * on which a mount would be triggered, such
			 * as /net/{hostname}.
			 *
			 * Thus, what would be found in it would be
			 * directories corresponding to submounts, such
			 * as /net/{hostname}/exports if that host
			 * exported /exports.
			 *
			 * If something were mounted directly on it,
			 * we wouldn't be here, as the lookup of
			 * an item under it would have gone into that
			 * file system.  Therefore, this is a case
			 * where the indirect map doesn't cause any
			 * mount on the map entry, just submounts;
			 * again, /net/{hostname}/exports would be
			 * an example if that host exported /exports
			 * but didn't export /.
			 *
			 * An autofs trigger will be mounted on it.
			 */
			error = vnode_get(fntovn(dfnp));
			fnp = dfnp;
			searchnm = dfnp->fn_name;
			searchnmlen = dfnp->fn_namelen;
		}
	} else {
		/*
		 * Yes, we did.  Try to get an iocount on the vnode.
		 */
		error = vnode_get(fntovn(fnp));
	}

fail:
	if (error == EAGAIN) {
		auto_fninfo_unlock_shared(dfnip, context);
		lck_rw_done(dfnp->fn_rwlock);
		goto top;
	}
	if (error) {
		auto_fninfo_unlock_shared(dfnip, context);
		lck_rw_done(dfnp->fn_rwlock);
		return (error);
	}

	/*
	 * We now have the actual fnnode we're interested in.
	 * The 'MF_LOOKUP' indicates another thread is currently
	 * performing a daemon lookup of this node, therefore we
	 * wait for its completion.
	 * The 'MF_INPROG' indicates another thread is currently
	 * performing a daemon mount of this node, we wait for it
	 * to be done if we are performing a MOUNT. We don't
	 * wait for it if we are performing a LOOKUP.
	 * We can release the reader/writer lock as soon as we acquire
	 * the mutex, since the state of the lock can only change by
	 * first acquiring the mutex.
	 */
	lck_mtx_lock(fnp->fn_lock);
	lck_rw_done(dfnp->fn_rwlock);

	/*
	 * If we're the automounter or a child of the automounter,
	 * there's nothing to trigger.
	 */
	if (auto_is_automounter(vfs_context_pid(context))) {
		auto_fninfo_unlock_shared(dfnip, context);
		lck_mtx_unlock(fnp->fn_lock);
		*vpp = fntovn(fnp);
		return (0);
	}

	/*
	 * Assume we're going to be doing a mount.  Then:
	 *
	 *	if this isn't the last component of the pathname, do
	 *	so, so we can continue with the lookup;
	 *
	 *	if this is the last component of the pathname, and
	 *	this operation is explicitly not supposed to trigger
	 *	mounts, or it's an operation type that's not supposed
	 *	to trigger mounts, don't do so, otherwise do so.
	 *
	 * For CREATE: don't trigger mount on the last component of the
	 * pathname.  If the target name doesn't exist, there's nothing
	 * to trigger.  If it does exist and there's something mounted
	 * there, there's nothing to trigger.  If it does exist but there's
	 * nothing mounted there, either somebody mounts something there
	 * before the next reference (e.g., the home directory mechanism),
	 * in which case we don't want any mounts triggered for it, or
	 * somebody refers to it before a mount is done on it, in which
	 * case we trigger the mount *then*.
	 *
	 * For DELETE: don't trigger mount on the last component of the
	 * pathname.  We don't allow removal of autofs objects.
	 *
	 * For RENAME: don't trigger mount on the last component of the
	 * pathname.  We don't allow renames of autofs objects.
	 */
	operation = OP_MOUNT;
	if ((flags & ISLASTCN) &&
	    (nameiop != LOOKUP || (flags & NOTRIGGER)))
		operation = OP_LOOKUP;

	/*
	 * If there's a lookup in progress on this (waiting for a reply
	 * from automountd), or if this operation is a mount and
	 * there's a mount in progress, just wait for that operation
	 * to complete, and then try the lookup again if the in-progress
	 * operation succeeded, otherwise return the error from the
	 * operation.
	 */
	if ((fnp->fn_flags & MF_LOOKUP) ||
	    ((operation == OP_MOUNT) && (fnp->fn_flags & MF_INPROG))) {
		auto_fninfo_unlock_shared(dfnip, context);
		lck_mtx_unlock(fnp->fn_lock);
		error = auto_wait4mount(fnp, context);
		vnode_put(fntovn(fnp));
		if (error && error != EAGAIN)
			return (error);
		goto top;
	}

	/*
	 * If the fnnode is not a directory, or already has something
	 * mounted on top of it, or has subdirectories, there's nothing
	 * to do; no mount is needed, and at least one lookup on it
	 * succeeded in the past and we're now holding it down so
	 * if a lookup to automountd failed now we still can't get
	 * rid of it.
	 */
	lck_rw_lock_shared(fnp->fn_rwlock);
	if (!vnode_isdir(fntovn(fnp)) || vnode_mountedhere(fntovn(fnp)) ||
	    fnp->fn_direntcnt != 0) {
		/*
		 * got the fnnode, check for any errors
		 * on the previous operation on that node.
		 */
		error = fnp->fn_error;
		if ((error == EINTR) || (error == EAGAIN)) {
			/*
			 * previous operation on this node was
			 * not completed, do a lookup now.
			 */
			operation = OP_LOOKUP;
		} else {
			/*
			 * previous operation completed. Return
			 * a pointer to the node only if there was
			 * no error.
			 */
			lck_rw_unlock_shared(fnp->fn_rwlock);
			auto_fninfo_unlock_shared(dfnip, context);
			lck_mtx_unlock(fnp->fn_lock);
			if (!error)
				*vpp = fntovn(fnp);
			else
				vnode_put(fntovn(fnp));
			return (error);
		}
	}
	lck_rw_unlock_shared(fnp->fn_rwlock);

	/*
	 * Since I got to this point, it means I'm the one
	 * responsible for triggering the mount/look-up of this node.
	 */
	switch (operation) {
	case OP_LOOKUP:
		AUTOFS_BLOCK_OTHERS(fnp, MF_LOOKUP);
		fnp->fn_error = 0;
		lck_mtx_unlock(fnp->fn_lock);
		error = auto_lookup_aux(fnp, searchnm, searchnmlen, context);
		auto_fninfo_unlock_shared(dfnip, context);
		vp = fntovn(fnp);
		if (!error) {
			/*
			 * Return this vnode
			 */
			*vpp = vp;
		} else {
			/*
			 * release our iocount on this vnode
			 * and return error
			 */
			vnode_put(vp);
		}
		break;
	case OP_MOUNT:
		AUTOFS_BLOCK_OTHERS(fnp, MF_INPROG);
		fnp->fn_error = 0;
		lck_mtx_unlock(fnp->fn_lock);
		/*
		 * auto_new_mount_thread fires up a new thread which
		 * calls automountd finishing up the work
		 */
		auto_new_mount_thread(fnp, searchnm, searchnmlen, context);

		/*
		 * At this point, we are simply another thread
		 * waiting for the mount to complete
		 */
		error = auto_wait4mount(fnp, context);
		auto_fninfo_unlock_shared(dfnip, context);
		vp = fntovn(fnp);
		if (!error) {
			/*
			 * Return this vnode
			 */
			*vpp = vp;
		} else {
			/*
			 * release our iocount on this vnode
			 * and return error
			 */
			vnode_put(vp);
		}
		break;
	default:
		auto_fninfo_unlock_shared(dfnip, context);
		log(LOG_WARNING, "auto_lookup: unknown operation %d\n",
		    operation);
	}

	AUTOFS_DPRINT((5, "auto_lookup: name=%s *vpp=%p return=%d\n",
	    cnp->cn_nameptr, (void *)*vpp, error));

	return (error);
}

static int autofs_nobrowse = 0;

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

#define MAXDIRBUFSIZE	65536

/*
 * "Transient" fnnodes are fnnodes on which a lookup is in progress
 * or don't have anything mounted on them and don't have subdirectories.
 * Those are subject to evaporating in the near term, so we don't
 * return them from a readdir - and don't filter them out from names
 * we get from automountd.
 */
#define IS_TRANSIENT(fnp) \
	(((fnp)->fn_flags & MF_LOOKUP) || \
	 (!vnode_mountedhere(fntovn(fnp)) && (fnp)->fn_direntcnt == 0))

int
auto_readdir(ap)
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
	struct uio *uiop = ap->a_uio;
	int status;
	uint64_t return_offset;
	boolean_t return_eof;
	byte_buffer return_buffer;
	mach_msg_type_number_t return_bufcount;
	vm_map_offset_t map_data;
	vm_offset_t data;
	fnnode_t *fnp = vntofn(vp);
	fnnode_t *cfnp, *nfnp;
	struct dirent *dp;
	off_t offset;
	uint64_t outcount = 0, count;
        void *outbuf;
	user_ssize_t alloc_count;
	fninfo_t *fnip = vfstofni(vnode_mount(vp));
	kern_return_t ret;
	int error = 0;
	int reached_max = 0;
	int myeof = 0;
	int this_reclen;
	mach_port_t automount_port;

        AUTOFS_DPRINT((4, "auto_readdir vp=%p offset=%lld\n",
            (void *)vp, uio_offset(uiop)));
                                                
	if (ap->a_numdirent != NULL)
		*ap->a_numdirent = 0;

	if (ap->a_flags & (VNODE_READDIR_EXTENDED | VNODE_READDIR_REQSEEKOFF))
		return (EINVAL);

	if (ap->a_eofflag != NULL)
		*ap->a_eofflag = 0;

	alloc_count = uio_resid(uiop);
	/*
	 * Reject too-small user requests.
	 */
	if (alloc_count < DIRENT_RECLEN(1))
		return (EINVAL);
	/*
	 * Trim too-large user requests.
	 */
	if (alloc_count > MAXDIRBUFSIZE)
		alloc_count = MAXDIRBUFSIZE;

	auto_fninfo_lock_shared(fnip, ap->a_context);
	lck_rw_lock_shared(fnp->fn_rwlock);
	
	if (uio_offset(uiop) >= AUTOFS_DAEMONCOOKIE) {
		if (fnip->fi_flags & MF_DIRECT) {
			/*
			 * This mount is for a direct map.
			 *
			 * Any attempt to do an open of this directory,
			 * such as the one that got the descriptor from
			 * which the caller is reading, should have
			 * triggered a mount on it; presumably the mount
			 * failed or only caused triggers to be planted
			 * on subdirectories of it.
			 *
			 * That means that the only stuff in this
			 * directory is the stuff we put there, and
			 * there's nothing we can get from automountd,
			 * so just treat this as an EOF.
			 */
			myeof = 1;
			if (ap->a_eofflag != NULL)
				*ap->a_eofflag = 1;
			goto done;
		}

		/*
		 * This mount is for an indirect map.
		 *
		 * If we're in the middle of unmounting that map, and
		 * this is the root directory for that map, we won't
		 * create anything under it in a lookup, so we should
		 * only return directory entries for things that are
		 * already there.
		 */
		if (fnip->fi_flags & MF_UNMOUNTING && vp == fnip->fi_rootvp) {
			myeof = 1;
			if (ap->a_eofflag != NULL)
				*ap->a_eofflag = 1;
			goto done;
		}

again:
		/*
		 * Do readdir of daemon contents only
		 * Drop readers lock and reacquire after reply.
		 */
		lck_rw_unlock_shared(fnp->fn_rwlock);

		count = 0;
		error = auto_get_automountd_port(&automount_port);
		if (error)
			goto done_nolock;
		ret = autofs_readdir(automount_port, fnip->fi_map,
		    uio_offset(uiop), alloc_count, &status,
		    &return_offset, &return_eof, &return_buffer,
		    &return_bufcount);
		auto_release_automountd_port(automount_port);
		/*
		 * reacquire previously dropped lock
		 */
		lck_rw_lock_shared(fnp->fn_rwlock);

		if (ret == KERN_SUCCESS)
			error = status;
		else {
			/* XXX - deal with Mach errors */
			error = EIO;
		}

		if (error)
			goto done;

		ret = vm_map_copyout(kernel_map, &map_data,
		    (vm_map_copy_t)return_buffer);
		if (ret != KERN_SUCCESS) {
			/* XXX - deal with Mach errors */
			error = EIO;
			goto done;
		}
		data = CAST_DOWN(vm_offset_t, map_data);

		if (return_bufcount != 0) {
			struct dirent *odp;	/* next in output buffer */
			struct dirent *cdp;	/* current examined entry */

			/*
			 * Check for duplicates here
			 */
			dp = (struct dirent *)data;
			odp = dp;
			cdp = dp;
			do {
				this_reclen = RECLEN(cdp);
				cfnp = auto_search(fnp, cdp->d_name,
				    cdp->d_namlen);
				if (cfnp == NULL || IS_TRANSIENT(cfnp)) {
					/*
					 * entry not found in kernel list,
					 * or found but is transient, so
					 * include it in readdir output.
					 *
					 * If we are skipping entries. then
					 * we need to copy this entry to the
					 * correct position in the buffer
					 * to be copied out.
					 */
					if (cdp != odp)
						bcopy(cdp, odp,
						    (size_t)this_reclen);
					odp = nextdp(odp);
					outcount += this_reclen;
					if (ap->a_numdirent)
						++(*ap->a_numdirent);
				} else {
					/*
					 * Entry was found in the kernel
					 * list. If it is the first entry
					 * in this buffer, then just skip it
					 */
					if (odp == dp) {
						dp = nextdp(dp);
						odp = dp;
					}
				}
				count += this_reclen;
				cdp = (struct dirent *)
				    ((char *)cdp + this_reclen);
			} while (count < return_bufcount);

			if (outcount)
				error = uiomove((caddr_t)dp, outcount, uiop);
			uio_setoffset(uiop, return_offset);
		} else {
			if (return_eof == 0) {
				/*
				 * alloc_count not large enough for one
				 * directory entry
				 */
				error = EINVAL;
			}
		}
		vm_deallocate(kernel_map, data, return_bufcount);
		if (return_eof && !error) {
			myeof = 1;
			if (ap->a_eofflag != NULL)
				*ap->a_eofflag = 1;
		}
		if (!error && !myeof && outcount == 0) {
			/*
			 * call daemon with new cookie, all previous
			 * elements happened to be duplicates
			 */
			goto again;
		}
		goto done;
	}

	/*
	 * Not past the "magic" offset, so we return only the entries
	 * we get without talking to the daemon.
	 */
	MALLOC(outbuf, void *, alloc_count, M_AUTOFS, M_WAITOK);
	dp = outbuf;
	if (uio_offset(uiop) == 0) {
		/*
		 * first time: so fudge the . and ..
		 */
		this_reclen = DIRENT_RECLEN(1);
		if (alloc_count < this_reclen) {
			error = EINVAL;
			goto done;
		}
		dp->d_ino = (ino_t)fnp->fn_nodeid;
		dp->d_reclen = (uint16_t)this_reclen;
#if 0
		dp->d_type = DT_DIR;
#else
		dp->d_type = DT_UNKNOWN;
#endif
		dp->d_namlen = 1;

		/* use strncpy() to zero out uninitialized bytes */

		(void) strncpy(dp->d_name, ".",
		    DIRENT_NAMELEN(this_reclen));
		outcount += dp->d_reclen;
		dp = nextdp(dp);

		if (ap->a_numdirent)
			++(*ap->a_numdirent);

		this_reclen = DIRENT_RECLEN(2);
		if (alloc_count < outcount + this_reclen) {
			error = EINVAL;
			FREE(outbuf, M_AUTOFS);
			goto done;
		}
		dp->d_reclen = (uint16_t)this_reclen;
		dp->d_ino = (ino_t)fnp->fn_parent->fn_nodeid;
#if 0
		dp->d_type = DT_DIR;
#else
		dp->d_type = DT_UNKNOWN;
#endif
		dp->d_namlen = 2;

		/* use strncpy() to zero out uninitialized bytes */

		(void) strncpy(dp->d_name, "..",
		    DIRENT_NAMELEN(this_reclen));
		outcount += dp->d_reclen;
		dp = nextdp(dp);

		if (ap->a_numdirent)
			++(*ap->a_numdirent);
	}

	offset = 2;
	cfnp = fnp->fn_dirents;
	while (cfnp != NULL) {
		nfnp = cfnp->fn_next;
		offset = cfnp->fn_offset;
		lck_rw_lock_shared(cfnp->fn_rwlock);
		if ((offset >= uio_offset(uiop)) && !IS_TRANSIENT(cfnp)) {
			int reclen;

			lck_rw_unlock_shared(cfnp->fn_rwlock);

			/*
			 * include node only if its offset is greater or
			 * equal to the one required and isn't
			 * transient
			 */
			reclen = (int)DIRENT_RECLEN(cfnp->fn_namelen);
			if (outcount + reclen > alloc_count) {
				reached_max = 1;
				break;
			}
			dp->d_reclen = (uint16_t)reclen;
			dp->d_ino = (ino_t)cfnp->fn_nodeid;
#if 0
			dp->d_type = vnode_isdir(fntovn(cfnp)) ? DT_DIR : DT_LNK;
#else
			dp->d_type = DT_UNKNOWN;
#endif
			dp->d_namlen = cfnp->fn_namelen;

			/* use strncpy() to zero out uninitialized bytes */

			(void) strncpy(dp->d_name, cfnp->fn_name,
			    DIRENT_NAMELEN(reclen));
			outcount += dp->d_reclen;
			dp = nextdp(dp);

			if (ap->a_numdirent)
				++(*ap->a_numdirent);
		} else
			lck_rw_unlock_shared(cfnp->fn_rwlock);
		cfnp = nfnp;
	}

	if (outcount)
		error = uiomove(outbuf, outcount, uiop);
	if (!error) {
		if (reached_max) {
			/*
			 * This entry did not get added to the buffer on this,
			 * call. We need to add it on the next call therefore
			 * set uio_offset to this entry's offset.  If there
			 * wasn't enough space for one dirent, return EINVAL.
			 */
			uio_setoffset(uiop, offset);
			if (outcount == 0)
				error = EINVAL;
		} else if (autofs_nobrowse ||
		    (fnip->fi_mntflags & AUTOFS_MNT_NORDDIR) ||
		    (fnp->fn_trigger != NULL) ||
		    (!vnode_isvroot(vp) &&
		    vnode_isvroot(fntovn(fnp->fn_parent)) &&
		    (fnp->fn_dirents == NULL))) {
			/*
			 * done reading directory entries
			 */
			uio_setoffset(uiop, offset + 1);
			if (ap->a_eofflag != NULL)
				*ap->a_eofflag = 1;
		} else {
			/*
			 * Need to get the rest of the entries from the daemon.
			 */
			uio_setoffset(uiop, AUTOFS_DAEMONCOOKIE);
		}
	}
	FREE(outbuf, M_AUTOFS);

done:
	lck_rw_unlock_shared(fnp->fn_rwlock);
done_nolock:
	auto_fninfo_unlock_shared(fnip, ap->a_context);
	AUTOFS_DPRINT((5, "auto_readdir vp=%p offset=%lld eof=%d\n",
	    (void *)vp, uio_offset(uiop), myeof));	
	return (error);
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

static int
auto_readlink(ap)
	struct vnop_readlink_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	uio_t uiop = ap->a_uio;
	fnnode_t *fnp = vntofn(vp);
	int error;
	struct timeval now;

	AUTOFS_DPRINT((4, "auto_readlink: vp=%p\n", (void *)vp));

	microtime(&now);
	fnp->fn_ref_time = now.tv_sec;

	if (!vnode_islnk(vp))
		error = EINVAL;
	else {
		assert(!(fnp->fn_flags & (MF_INPROG | MF_LOOKUP)));
		fnp->fn_atime = now;
		error = uiomove(fnp->fn_symlink, MIN(fnp->fn_symlinklen,
		    uio_resid(uiop)), uiop);
	}

	AUTOFS_DPRINT((5, "auto_readlink: error=%d\n", error));
	return (error);
}

static int
auto_pathconf(ap)
	struct vnop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
		vfs_context_t a_context;
	} */ *ap;
{
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		/* arbitrary limit matching HFS; autofs has no hard limit */
		*ap->a_retval = 32767;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		break;
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 200112;		/* _POSIX_CHOWN_RESTRICTED */
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		break;
	case _PC_CASE_SENSITIVE:
		*ap->a_retval = 1;
		break;
	case _PC_CASE_PRESERVING:
		*ap->a_retval = 1;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int 
auto_getxattr(ap)
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
	vnode_t vp = ap->a_vp;
	fnnode_t *fnp = vntofn(vp);
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
		
		lck_rw_lock_shared(fnp->fn_rwlock);

		/* check if xattr exists */
		if (!(fnp->xattr_data.xattr_category)) {
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
		retval = uiomove((caddr_t)&(fnp->xattr_data.xattr_category), attrsize, uio);

	} else {
		/* autofs only supports network category xattr */
		retval = ENOATTR;
		goto out;
	}

out_unlock:
	lck_rw_unlock_shared(fnp->fn_rwlock);
	
out:
	return (retval);
}

static int 
auto_setxattr(ap)
	struct vnop_setxattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		char * a_name;
		uio_t a_uio;
		int a_options;
		vfs_context_t a_context;
	}; */ *ap;
{
	vnode_t vp = ap->a_vp;
	fnnode_t *fnp = vntofn(vp);
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

		lck_rw_lock_exclusive(fnp->fn_rwlock);
		
		/* create new xattr option and xattr exists */
		if ((ap->a_options & XATTR_CREATE) && fnp->xattr_data.xattr_category) {
			retval = EEXIST;
			goto out_unlock;
		}
		
		/* replace old xattr and xattr does not exist */
		if ((ap->a_options & XATTR_REPLACE) && !(fnp->xattr_data.xattr_category)) {
			retval = ENOATTR;
			goto out_unlock;
		}

		/* copy in attribute data */
		retval = uiomove((caddr_t) &new_xattr_data.xattr_category, attrsize, uio);
		if (retval) {
			goto out_unlock;
		}

		/* set the attribute */
		fnp->xattr_data.xattr_category = new_xattr_data.xattr_category;

		AUTO_KNOTE(vp, NOTE_ATTRIB);
	} else {
		/* autofs only supports network category xattr */
		retval = EPERM;
		goto out;
	}

out_unlock:
	lck_rw_unlock_exclusive(fnp->fn_rwlock);
	
out:
	return (retval);
}

static int
auto_removexattr(ap)
	struct vnop_removexattr_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		char * a_name;
		int a_options;
		vfs_context_t a_context;
	}; */ *ap;
{
	struct vnode *vp = ap->a_vp;
	fnnode_t *fnp = vntofn(vp);
	int retval = 0;

	/* network category xattr */
	if (strncmp(ap->a_name, XATTR_CATEGORYNAME, XATTR_CATEGORYNAMELEN) == 0) {

		lck_rw_lock_shared(fnp->fn_rwlock);

		/* check if xattr exists */
		if (!(fnp->xattr_data.xattr_category)) {
			retval = ENOATTR;
			goto out_unlock;
		}

		fnp->xattr_data.xattr_category = 0;
		
		AUTO_KNOTE(vp, NOTE_ATTRIB);
	} else {
		/* autofs only supports network category xattr */
		retval = ENOATTR;
		goto out;
	}

out_unlock:
	lck_rw_unlock_shared(fnp->fn_rwlock);
	
out:
	return (retval);
}

static int 
auto_listxattr(ap)
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
	fnnode_t *fnp = vntofn(vp);
	struct uio *uio = ap->a_uio;
	int retval = 0;
	
	*ap->a_size = 0;
	
	/* return zero if no extended attributes defined */
	if (!fnp->xattr_data.xattr_category) {
		retval = 0;
		goto out;
	}

	lck_rw_lock_shared(fnp->fn_rwlock);

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
	lck_rw_unlock_shared(fnp->fn_rwlock);
	
out:
	return (retval);
}

static int
auto_reclaim(ap)
	struct vnop_reclaim_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	fnnode_t *fnp = vntofn(vp);
	fnnode_t *dfnp = fnp->fn_parent;

	AUTOFS_DPRINT((4, "auto_reclaim: vp=%p fn_link=%d\n",
	    (void *)vp, fnp->fn_linkcnt));

	if (dfnp != NULL) {
		/*
		 * There are no filesystem calls in progress on this
		 * vnode, and none will be made until we're done.
		 *
		 * Thus, it's safe to disconnect this from its parent
		 * directory.
		 */
		lck_rw_lock_exclusive(dfnp->fn_rwlock);
		/*
		 * There are no active references to this.
		 * If there's only one link to this, namely the link to it
		 * from its parent, get rid of it by removing it from
		 * its parent's list of child fnnodes and recycle it;
		 * a subsequent reference to it will recreate it if
		 * the name is still there in the map.
		 */
		if (fnp->fn_linkcnt == 1)
			auto_disconnect(dfnp, fnp);
		else if (fnp->fn_linkcnt == 0) {
			/*
			 * Root vnode; we've already removed it from the
			 * "parent" (the master node for all autofs file
			 * systems) - just null out the parent pointer, so
			 * that we don't trip any assertions
			 * in auto_freefnnode().
			 */
			fnp->fn_parent = NULL;
		}
		lck_rw_unlock_exclusive(dfnp->fn_rwlock);
	}
	auto_freefnnode(fnp);
	vnode_clearfsnode(vp);
	AUTOFS_DPRINT((5, "auto_reclaim: (exit) vp=%p freed\n",
	    (void *)vp));
	return (0);
}


static void
filt_autofsdetach(struct knote *kn)
{
	vnode_t vp = (vnode_t)kn->kn_hook;
	fnnode_t *fnp = vntofn(vp);

	if (vnode_getwithvid(vp, kn->kn_hookid))
		return;

	if (1) {  /* ! KNDETACH_VNLOCKED */
		lck_mtx_lock(fnp->fn_lock);
		(void) KNOTE_DETACH(&fnp->fn_knotes, kn);
		lck_mtx_unlock(fnp->fn_lock);
	}

	vnode_put(vp);
}

static int
filt_autofsvnode(struct knote *kn, long hint)
{
	vnode_t vp = (vnode_t)kn->kn_hook;

	if (hint == 0)  {
		if ((vnode_getwithvid(vp, kn->kn_hookid) != 0)) {
			hint = NOTE_REVOKE;
		} else
			vnode_put(vp);
	}
	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}
	
	return (kn->kn_fflags != 0);
}

static struct filterops autofsvnode_filtops = 
	{ 1, NULL, filt_autofsdetach, filt_autofsvnode };

/*
 * Add a kqueue filter.
 */
static int
auto_kqfilt_add(ap)
	struct vnop_kqfilt_add_args /* {
		vnode_t a_vp;
		struct knote *a_kn;
		struct proc *p;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct knote *kn = ap->a_kn;
	fnnode_t *fnp = vntofn(vp);

	switch (kn->kn_filter) {

	case EVFILT_VNODE:
		kn->kn_fop = &autofsvnode_filtops;
		break;

	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)vp;
	kn->kn_hookid = vnode_vid(vp);

	lck_mtx_lock(fnp->fn_lock);
	KNOTE_ATTACH(&fnp->fn_knotes, kn);
	lck_mtx_unlock(fnp->fn_lock);

	return (0);
}

/*
 * Remove a kqueue filter
 */
static int
auto_kqfilt_remove(ap)
	struct vnop_kqfilt_remove_args /* {
		vnode_t a_vp;
		uintptr_t ident;
		vfs_context_t a_context;
	} */__unused *ap;
{
	int result;

	result = ENOTSUP; /* XXX */
	
	return (result);
}
