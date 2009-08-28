/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

#pragma ident	"@(#)auto_vfsops.c	1.58	06/03/24 SMI"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <mach/machine/vm_types.h>
#include <sys/vnode.h>
#include <sys/vfs_context.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/attr.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <miscfs/devfs/devfs.h>
#include <kern/locks.h>
#include <kern/assert.h>

#include <IOKit/IOLib.h>

#include "autofs.h"
#include "autofs_kern.h"

lck_grp_t *autofs_lck_grp;
static lck_mtx_t *autofs_global_lock;
lck_mtx_t *autofs_nodeid_lock;

static u_int autofs_mounts;

__private_extern__ int auto_module_start(kmod_info_t *, void *);
__private_extern__ int auto_module_stop(kmod_info_t *, void *);

/*
 * autofs VFS operations
 */
static int auto_mount(mount_t, vnode_t, user_addr_t, vfs_context_t);
static int auto_start(mount_t, int, vfs_context_t);
static int auto_unmount(mount_t, int, vfs_context_t);
static int auto_root(mount_t, vnode_t *, vfs_context_t);
static int auto_vfs_getattr(mount_t, struct vfs_attr *, vfs_context_t);
static int auto_sync(mount_t, int, vfs_context_t);
static int auto_vget(mount_t, ino64_t, vnode_t *, vfs_context_t);
static int autofs_sysctl(int *, u_int, user_addr_t, size_t *, user_addr_t, size_t, vfs_context_t);

static struct vfsops autofs_vfsops = {
	auto_mount,
	auto_start,
	auto_unmount,
	auto_root,
	NULL,			/* auto_quotactl */
	auto_vfs_getattr, 	/* was auto_statfs */
	auto_sync,
	auto_vget,
	NULL,			/* auto_fhtovp */
	NULL,			/* auto_vptofh */
	NULL,			/* auto_init */
	autofs_sysctl,
	NULL,			/* auto_vfs_setattr */
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL }	/* reserved ops */
};

extern struct vnodeopv_desc autofsfs_vnodeop_opv_desc;
struct vnodeopv_desc * autofs_vnodeop_opv_descs[] =
	{&autofsfs_vnodeop_opv_desc, NULL};

struct vfs_fsentry autofs_fsentry = {
	&autofs_vfsops,			/* vfs operations */
	1,				/* # of vnodeopv_desc being registered (reg, spec, fifo ...) */
	autofs_vnodeop_opv_descs,	/* null terminated;  */
	0,				/* historic filesystem type number [ unused w. VFS_TBLNOTYPENUM specified ] */
	MNTTYPE_AUTOFS,			/* filesystem type name */
	VFS_TBLTHREADSAFE | VFS_TBLNOTYPENUM | VFS_TBL64BITREADY | VFS_TBLNATIVEXATTR,
					/* defines the FS capabilities */
	{ NULL, NULL }			/* reserved for future use; set this to zero*/
};

static vfstable_t  auto_vfsconf;

/*
 * No zones in OS X, so only one autofs_globals structure.
 */
static struct autofs_globals *global_fngp;

static struct autofs_globals *
autofs_zone_get_globals(void)
{
	return (global_fngp);
}

static void
autofs_zone_set_globals(struct autofs_globals *fngp)
{
	global_fngp = fngp;
}

/*
 * rootfnnodep is allocated here.  Its sole purpose is to provide
 * read/write locking for top level fnnodes.  This object is
 * persistent and will not be deallocated until the module is unloaded.
 * XXX - is there some compelling reason for it to exist?  Where is
 * it used?
 *
 * There are no zones in OS X, so this is system-wide.
 */
static struct autofs_globals *
autofs_zone_init(void)
{
	struct autofs_globals *fngp = NULL;
	fnnode_t *fnp = NULL;
	static const char fnnode_name[] = "root_fnnode";
	struct timeval now;
	kern_return_t ret;

	MALLOC(fngp, struct autofs_globals *, sizeof (*fngp), M_AUTOFS,
	    M_WAITOK);
	if (fngp == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs globals structure\n");
		goto fail;
	}
	bzero(fngp, sizeof (*fngp));
	/*
	 * Allocate a "dummy" fnnode, not associated with any vnode.
	 */
	MALLOC(fnp, fnnode_t *, sizeof(fnnode_t), M_AUTOFS, M_WAITOK);
	if (fnp == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global fnnode\n");
		goto fail;
	}
	bzero(fnp, sizeof(*fnp));
	fnp->fn_namelen = sizeof fnnode_name;
	MALLOC(fnp->fn_name, char *, fnp->fn_namelen + 1, M_AUTOFS, M_WAITOK);
	if (fnp->fn_name == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global fnnode name\n");
		goto fail;
	}
	bcopy(fnnode_name, fnp->fn_name, sizeof fnnode_name);
	fnp->fn_name[sizeof fnnode_name] = '\0';
	fnp->fn_mode = AUTOFS_MODE;
	microtime(&now);
	fnp->fn_crtime = fnp->fn_atime = fnp->fn_mtime = fnp->fn_ctime = now;
	fnp->fn_ref_time = now.tv_sec;
	fnp->fn_nodeid = 1;	/* XXX - could just be zero? */
	fnp->fn_globals = fngp;
	fnp->fn_lock = lck_mtx_alloc_init(autofs_lck_grp, NULL);
	if (fnp->fn_lock == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global fnnode mutex\n");
		goto fail;
	}
	fnp->fn_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
	if (fnp->fn_rwlock == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global fnnode rwlock\n");
		goto fail;
	}

	fngp->fng_rootfnnodep = fnp;
	fngp->fng_fnnode_count = 1;
	fngp->fng_printed_not_running_msg = 0;
	fngp->fng_unmount_threads_lock = lck_mtx_alloc_init(autofs_lck_grp,
	    LCK_ATTR_NULL);
	if (fngp->fng_unmount_threads_lock == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global unmount threads lock\n");
		goto fail;
	}
	fngp->fng_unmount_threads = 0;
	fngp->fng_terminate_do_unmount_thread = 0;
	fngp->fng_do_unmount_thread_terminated = 0;
	fngp->fng_flush_notification_lock = lck_mtx_alloc_init(autofs_lck_grp,
	    LCK_ATTR_NULL);
	if (fngp->fng_flush_notification_lock == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global flush notification lock\n");
		goto fail;
	}

	/*
	 * Start the unmounter thread.
	 * XXX - priority?  minclsyspri?
	 */
	ret = auto_new_thread(auto_do_unmount, fngp);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs_zone_init: Couldn't create unmounter thread, status 0x%08x", ret);
		goto fail;
	}
	return (fngp);

fail:
	if (fnp != NULL) {
		if (fnp->fn_rwlock != NULL)
			lck_rw_free(fnp->fn_rwlock, autofs_lck_grp);
		if (fnp->fn_lock != NULL)
			lck_mtx_free(fnp->fn_lock, autofs_lck_grp);
		if (fnp->fn_name != NULL)
			FREE(fnp->fn_name, M_AUTOFS);
		FREE(fnp, M_AUTOFS);
	}
	if (fngp != NULL) {
		if (fngp->fng_flush_notification_lock != NULL)
			lck_mtx_free(fngp->fng_flush_notification_lock,
			    autofs_lck_grp);
		if (fngp->fng_unmount_threads_lock != NULL)
			lck_mtx_free(fngp->fng_unmount_threads_lock,
			    autofs_lck_grp);
		FREE(fngp, M_AUTOFS);
	}
	return (NULL);
}

/*
 * Find the first occurrence of find in s.
 */
static char *
strstr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

/*
 * A "struct mntopt" table is terminated with an entry with a null
 * m_option pointer; therefore, the number of real entries in the
 * table is one fewer than the total number of entries.
 */
static const struct mntopt restropts[] = {
	RESTRICTED_MNTOPTS
};
#define	NROPTS	((sizeof (restropts)/sizeof (restropts[0])) - 1)

/*
 * Check whether the specified option is set in the specified file
 * system.
 */
static uint64_t
optionisset(mount_t mp, const struct mntopt *opt)
{
	uint64_t flags;
	
	flags = vfs_flags(mp) & opt->m_flag;
	if (opt->m_inverse)
		return !flags;
	else
		return flags;
}

/*
 * This routine adds those options to the option string `buf' which are
 * forced by secpolicy_fs_mount.  If the automatic "security" options
 * are set, the option string gets them added if they aren't already
 * there.  We search the string with "strstr" and make sure that
 * the string we find is bracketed with <start|",">MNTOPT<","|"\0">
 *
 * This is one half of the option inheritence algorithm which
 * implements the "restrict" option.  The other half is implemented
 * in automountd; it takes its cue from the options we add here.
 */
static int
autofs_restrict_opts(mount_t mp, char *buf, size_t maxlen, size_t *curlen)
{
	fninfo_t *fnip = vfstofni(mp);
	u_int i;
	char *p;
	size_t len = *curlen - 1;

	/* Unrestricted */
	if (!(fnip->fi_mntflags & AUTOFS_MNT_RESTRICT))
		return (0);

	for (i = 0; i < NROPTS; i++) {
		size_t olen = strlen(restropts[i].m_option);

		/* Add "restrict" always and the others insofar set */
		if ((i == 0 || optionisset(mp, &restropts[i])) &&
		    ((p = strstr(buf, restropts[i].m_option)) == NULL ||
		    !((p == buf || p[-1] == ',') &&
		    (p[olen] == '\0' || p[olen] == ',')))) {

			if (len + olen + 1 > maxlen)
				return (-1);

			if (*buf != '\0')
				buf[len++] = ',';
			bcopy(restropts[i].m_option, &buf[len], olen);
			len += olen;
		}
	}
	if (len + 1 > maxlen)
		return (-1);
	buf[len++] = '\0';
	*curlen = len;
	return (0);
}

/* ARGSUSED */
static int
auto_mount(mount_t mp, __unused vnode_t devvp, user_addr_t data,
    vfs_context_t context)
{
	int error;
	size_t len;
	uint32_t argsvers;
	struct autofs_args_64 args;
	fninfo_t *fnip = NULL;
	vnode_t myrootvp = NULL;
	fnnode_t *rootfnp = NULL;
	char strbuff[PATH_MAX+1];
	uint64_t flags;
#ifdef DEBUG
	lck_attr_t *lckattr;
#endif
	struct autofs_globals *fngp;

	AUTOFS_DPRINT((4, "auto_mount: mp %p devvp %p\n", mp, devvp));

	lck_mtx_lock(autofs_global_lock);
	if ((fngp = autofs_zone_get_globals()) == NULL) {
		fngp = autofs_zone_init();
		autofs_zone_set_globals(fngp);
	}
	lck_mtx_unlock(autofs_global_lock);
	if (fngp == NULL)
		return (ENOMEM);

	/*
	 * Get argument version
	 */
	error = copyin(data, (caddr_t)&argsvers, sizeof (argsvers));
	if (error)
		return (error);

	/*
	 * Get arguments
	 */
	switch (argsvers) {

	case 1:
		if (vfs_context_is64bit(context))
			error = copyin(data, &args, sizeof (args));
		else {
			struct autofs_args_32 args32;

			error = copyin(data, &args32, sizeof (args32));
			if (error == 0) {
				args.path = CAST_USER_ADDR_T(args32.path);
				args.opts = CAST_USER_ADDR_T(args32.opts);
				args.map = CAST_USER_ADDR_T(args32.map);
				args.subdir = CAST_USER_ADDR_T(args32.subdir);
				args.key = CAST_USER_ADDR_T(args32.key);
				args.mntflags = args32.mntflags;
				args.mount_to = args32.mount_to;
				args.mach_to = args32.mach_to;
				args.direct = args32.direct;
				args.trigger = args32.trigger;
			}
		}
		if (error)
			return (error);
		break;

	default:
		return (EINVAL);
	}

	/*
	 * We don't support remounts; the AUTOFS_UPDATE_OPTIONS ioctl
	 * should be used for that.
	 */
	flags = vfs_flags(mp) & MNT_CMDFLAGS;
	if (flags & MNT_UPDATE)
		return (EINVAL);

	/*
	 * Allocate fninfo struct and attach it to vfs
	 */
	MALLOC(fnip, fninfo_t *, sizeof(*fnip), M_AUTOFS, M_WAITOK);
	if (fnip == NULL)
		return (ENOMEM);
	bzero(fnip, sizeof(*fnip));
	fnip->fi_flags |= MF_MOUNTING;	/* mount isn't finished yet */

	fnip->fi_mount_to = args.mount_to;
	fnip->fi_mach_to = args.mach_to;

	/*
	 * Assign a unique fsid to the mount
	 */
	vfs_getnewfsid(mp);
	vfs_setfsprivate(mp, fnip);

	/*
	 * Get path for mountpoint
	 */
	error = copyinstr(args.path, strbuff, sizeof (strbuff), &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	MALLOC(fnip->fi_path, char *, len, M_AUTOFS, M_WAITOK);
	if (fnip->fi_path == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_pathlen = (int)len;
	bcopy(strbuff, fnip->fi_path, len);

	/*
	 * Get default options
	 */
	error = copyinstr(args.opts, strbuff, sizeof (strbuff), &len);

	if (error != 0)
		goto errout;
		
	if (autofs_restrict_opts(mp, strbuff, sizeof (strbuff), &len) != 0) {
		error = EFAULT;
		goto errout;
	}
	MALLOC(fnip->fi_opts, char *, len, M_AUTOFS, M_WAITOK);
	if (fnip->fi_opts == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_optslen = (int)len;
	bcopy(strbuff, fnip->fi_opts, len);

	/*
	 * Get context/map name
	 */
	error = copyinstr(args.map, strbuff, sizeof (strbuff), &len);
	if (error)
		goto errout;
	MALLOC(fnip->fi_map, char *, len, M_AUTOFS, M_WAITOK);
	if (fnip->fi_map == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_maplen = (int)len;
	bcopy(strbuff, fnip->fi_map, len);

	if (args.trigger) {
		/*
		 * Mark this as a trigger, both to let users know
		 * that it's a trigger and to let the automount
		 * command know that it's a trigger.
		 */
		strncpy(vfs_statfs(mp)->f_mntfromname, "trigger",
		    sizeof(vfs_statfs(mp)->f_mntfromname)-1);
		vfs_statfs(mp)->f_mntfromname[sizeof(vfs_statfs(mp)->f_mntfromname)-1] = (char)0;
	} else {
		snprintf(vfs_statfs(mp)->f_mntfromname, sizeof(vfs_statfs(mp)->f_mntfromname),
		    "map %.*s", fnip->fi_maplen, fnip->fi_map);
	}

	/*
	 * Get subdirectory within map
	 */
	error = copyinstr(args.subdir, strbuff, sizeof (strbuff), &len);
	if (error)
		goto errout;
	MALLOC(fnip->fi_subdir, char *, len, M_AUTOFS, M_WAITOK);
	if (fnip->fi_subdir == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_subdirlen = (int)len;
	bcopy(strbuff, fnip->fi_subdir, len);

	/*
	 * Get the key
	 */
	error = copyinstr(args.key, strbuff, sizeof (strbuff), &len);
	if (error)
		goto errout;
	MALLOC(fnip->fi_key, char *, len, M_AUTOFS, M_WAITOK);
	if (fnip->fi_key == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_keylen = (int)len;
	bcopy(strbuff, fnip->fi_key, len);

	fnip->fi_mntflags = args.mntflags;

	/*
	 * Is this a direct mount?
	 */
	if (args.direct == 1)
		fnip->fi_flags |= MF_DIRECT;

	/*
	 * Get an rwlock.
	 */
#ifdef DEBUG
	/*
	 * Enable debugging on the lock.
	 */
	lckattr = lck_attr_alloc_init();
	lck_attr_setdebug(lckattr);
	fnip->fi_rwlock = lck_rw_alloc_init(autofs_lck_grp, lckattr);
	lck_attr_free(lckattr);
#else
	fnip->fi_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
#endif

	/*
	 * Make the root vnode
	 */
	error = auto_makefnnode(&rootfnp, VDIR, mp, NULL, fnip->fi_path,
	    NULL, 1, vfs_context_ucred(context), fngp);
	if (error)
		goto errout;
	myrootvp = fntovn(rootfnp);

	rootfnp->fn_mode = AUTOFS_MODE;
	rootfnp->fn_parent = rootfnp;
	/* account for ".." entry (fn_parent) */
	rootfnp->fn_linkcnt = 1;
	error = vnode_ref(myrootvp);	/* released in auto_unmount */
	if (error) {
		vnode_put(myrootvp);
		vnode_recycle(myrootvp);
		goto errout;
	}
	fnip->fi_rootvp = myrootvp;

	/*
	 * Add to list of top level AUTOFS's if this isn't a trigger
	 * mount.
	 */
	if (!args.trigger) {
		lck_rw_lock_exclusive(fngp->fng_rootfnnodep->fn_rwlock);
		rootfnp->fn_parent = fngp->fng_rootfnnodep;
		rootfnp->fn_next = fngp->fng_rootfnnodep->fn_dirents;
		fngp->fng_rootfnnodep->fn_dirents = rootfnp;
		lck_rw_unlock_exclusive(fngp->fng_rootfnnodep->fn_rwlock);
	}

	/*
	 * One more mounted file system.
	 */
	lck_mtx_lock(autofs_global_lock);
	autofs_mounts++;
	lck_mtx_unlock(autofs_global_lock);

	vnode_put(myrootvp);

	AUTOFS_DPRINT((5, "auto_mount: mp %p root %p fnip %p return %d\n",
	    mp, myrootvp, fnip, error));

	return (0);

errout:
	assert(fnip != NULL);

	if (fnip->fi_rwlock != NULL)
		lck_rw_free(fnip->fi_rwlock, autofs_lck_grp);
	if (fnip->fi_path != NULL)
		FREE(fnip->fi_path, M_AUTOFS);
	if (fnip->fi_opts != NULL)
		FREE(fnip->fi_opts, M_AUTOFS);
	if (fnip->fi_map != NULL)
		FREE(fnip->fi_map, M_AUTOFS);
	if (fnip->fi_subdir != NULL)
		FREE(fnip->fi_subdir, M_AUTOFS);
	if (fnip->fi_key != NULL)
		FREE(fnip->fi_key, M_AUTOFS);
	FREE(fnip, sizeof M_AUTOFS);

	AUTOFS_DPRINT((5, "auto_mount: vfs %p root %p fnip %p return %d\n",
	    (void *)mp, (void *)myrootvp, (void *)fnip, error));

	return (error);
}

static int
auto_update_options(struct autofs_update_args_64 *update_argsp, int pid)
{
	mount_t mp;
	fninfo_t *fnip;
	fnnode_t *fnp;
	char strbuff[PATH_MAX+1];
	int error;
	char *opts, *map;
	size_t optslen, maplen;
	int was_nobrowse;

	/*
	 * Update the mount options on this autofs node.
	 * XXX - how do we make sure mp isn't freed out from under
	 * us?
	 */
	mp = vfs_getvfs(&update_argsp->fsid);
	if (mp == NULL)
		return (ENOENT);	/* no such mount */

	/*
	 * Make sure this is an autofs mount.
	 */
	if (!auto_is_autofs(mp))
		return (EINVAL);

	fnip = vfstofni(mp);
	if (fnip == NULL)
		return (EINVAL);

	/*
	 * We can't change the map type if the top-level directory has
	 * subdirectories (i.e., if stuff has happened under it).
	 */
	if ((update_argsp->direct == 1 && !(fnip->fi_flags & MF_DIRECT)) ||
	    (update_argsp->direct != 1 && (fnip->fi_flags & MF_DIRECT))) {
		fnp = vntofn(fnip->fi_rootvp);
		if (fnp->fn_dirents != NULL)
			return (EINVAL);
	}

	/*
	 * Get default options
	 */
	error = copyinstr(update_argsp->opts, strbuff, sizeof (strbuff),
	    &optslen);
	if (error)
		return (error);

	if (autofs_restrict_opts(mp, strbuff, sizeof (strbuff), &optslen) != 0)
		return (EFAULT);

	MALLOC(opts, char *, optslen, M_AUTOFS, M_WAITOK);
	bcopy(strbuff, opts, optslen);

	/*
	 * Get context/map name
	 */
	error = copyinstr(update_argsp->map, strbuff, sizeof (strbuff),
	    &maplen);
	if (error) {
		FREE(opts, M_AUTOFS);
		return (error);
	}
	MALLOC(map, char *, maplen, M_AUTOFS, M_WAITOK);
	bcopy(strbuff, map, maplen);

	/*
	 * We've fetched all the strings; lock out any other references
	 * to fnip, and update the mount information.
	 */
	lck_rw_lock_exclusive(fnip->fi_rwlock);
	was_nobrowse = auto_nobrowse(fnip->fi_rootvp);
	FREE(fnip->fi_opts, M_AUTOFS);
	fnip->fi_opts = opts;
	fnip->fi_optslen = (int)optslen;
	FREE(fnip->fi_map, M_AUTOFS);
	fnip->fi_map = map;
	fnip->fi_maplen = (int)maplen;
	if (update_argsp->direct == 1)
		fnip->fi_flags |= MF_DIRECT;
	else
		fnip->fi_flags &= ~MF_DIRECT;
	fnip->fi_flags &= ~(MF_MOUNTING|MF_UNMOUNTING);
	fnip->fi_mntflags = update_argsp->mntflags;
	fnip->fi_mount_to = update_argsp->mount_to;
	fnip->fi_mach_to = update_argsp->mach_to;

	snprintf(vfs_statfs(mp)->f_mntfromname, sizeof(vfs_statfs(mp)->f_mntfromname),
	    "map %.*s", fnip->fi_maplen, fnip->fi_map);
	lck_rw_unlock_exclusive(fnip->fi_rwlock);

	/*
	 * Notify anybody looking at this mount's root directory
	 * that it might have changed, unless it's a directory
	 * whose contents reflect only the stuff for which we
	 * already have vnodes and was that way before the change;
	 * this remount might have been done as the result of a network
	 * change, so the map backing it might have changed.  (That
	 * matters only if we enumerate the map on a readdir, which
	 * isn't the case for a directory of the sort described above,
	 * although, if it wasn't a map of that sort before, but is
	 * one now, that could also change what's displayed, so we
	 * deliver a notification in that case.)
	 */
	if (vnode_ismonitored(fnip->fi_rootvp) &&
	    (!was_nobrowse || !auto_nobrowse(fnip->fi_rootvp))) {
		struct vnode_attr vattr;

		vfs_get_notify_attributes(&vattr);
		auto_get_attributes(fnip->fi_rootvp, &vattr, pid);
		vnode_notify(fnip->fi_rootvp, VNODE_EVENT_WRITE, &vattr);
	}
	return (0);
}

/*
 * This is called during a mount when the file system is "ready".
 * We refuse to trigger in auto_root() until that has happened.
 * Otherwise, we are called when the mount system call has a writer
 * lock on mp, which means no lookups can proceed past mp - including
 * the lookup necessary to mount on top of the trigger point!  That
 * means that if a mount is triggered in the process of mounting an
 * autofs file system, and the autofs mount can't complete until the
 * triggered mount completes, we deadlock, as the triggered mount
 * can't complete until the writer lock is released, and that doesn't
 * happen until the autofs mount completes.
 */
int
auto_start(mount_t mp, __unused int flags, __unused vfs_context_t context)
{
	fninfo_t *fnip;

	fnip = vfstofni(mp);
	lck_rw_lock_exclusive(fnip->fi_rwlock);
	fnip->fi_flags &= ~MF_MOUNTING;
	lck_rw_unlock_exclusive(fnip->fi_rwlock);
	return (0);
}

/* ARGSUSED */
static int
auto_unmount(mount_t mp, int mntflags, __unused vfs_context_t context)
{
	fninfo_t *fnip;
	vnode_t rvp;
	fnnode_t *rfnp, *fnp, *pfnp;
	fnnode_t *myrootfnnodep;
	int error;

	fnip = vfstofni(mp);
	AUTOFS_DPRINT((4, "auto_unmount mp %p fnip %p\n", (void *)mp,
			(void *)fnip));

	/*
	 * forced unmount is not supported by this file system
	 * and thus, ENOTSUP, is being returned.
	 *
	 * XXX - forced unmounts are done at system shutdown time;
	 * do we need to make them work?
	 */
	if (mntflags & MNT_FORCE)
		return (ENOTSUP);

#if 0
	assert(vn_vfswlock_held(mp->mnt_vnodecovered));
#endif
	rvp = fnip->fi_rootvp;
	rfnp = vntofn(rvp);

	error = vflush(mp, rvp, 0);
	if (error)
		return (error);

	if (vnode_isinuse(rvp, 1) || rfnp->fn_dirents != NULL)
		return (EBUSY);

	/*
	 * The root vnode is on the linked list of root fnnodes only if
	 * this was not a trigger node. Since we have no way of knowing,
	 * if we don't find it, then we assume it was a trigger node.
	 */
	myrootfnnodep = rfnp->fn_globals->fng_rootfnnodep;
	pfnp = NULL;
	lck_rw_lock_exclusive(myrootfnnodep->fn_rwlock);
	fnp = myrootfnnodep->fn_dirents;
	while (fnp != NULL) {
		if (fnp == rfnp) {
			/*
			 * A check here is made to see if rvp is busy.  If
			 * so, return EBUSY.  Otherwise proceed with
			 * disconnecting it from the list.
			 */
			if (vnode_isinuse(rvp, 1) || rfnp->fn_dirents != NULL) {
				lck_rw_unlock_exclusive(myrootfnnodep->fn_rwlock);
				return (EBUSY);
			}
			if (pfnp)
				pfnp->fn_next = fnp->fn_next;
			else
				myrootfnnodep->fn_dirents = fnp->fn_next;
			fnp->fn_next = NULL;
			break;
		}
		pfnp = fnp;
		fnp = fnp->fn_next;
	}
	lck_rw_unlock_exclusive(myrootfnnodep->fn_rwlock);

	assert(vnode_isinuse(rvp, 0) && !vnode_isinuse(rvp, 1));
	assert(rfnp->fn_direntcnt == 0);
	assert(rfnp->fn_linkcnt == 1);
	/*
	 * The following drops linkcnt to 0, therefore the disconnect is
	 * not attempted when auto_inactive() is called by
	 * vn_rele(). This is necessary because we have nothing to get
	 * disconnected from since we're the root of the filesystem. As a
	 * side effect the node is not freed, therefore I should free the
	 * node here.
	 *
	 * XXX - I really need to think of a better way of doing this.
	 * XXX - this is not Solaris, so maybe there is a better way.
	 */
	rfnp->fn_linkcnt--;

	/*
	 * release last reference to the root vnode
	 */
	vnode_rele(rvp);	/* release reference from auto_mount() */

	/*
	 * Wait for in-flight operations to complete on any remaining vnodes
	 * (such as the root vnode, which we just released).
	 *
	 * The root vnode will be recycled in vflush() when there are
	 * no longer any in-flight operations.
	 */
	vflush(mp, NULLVP, 0);

	lck_rw_free(fnip->fi_rwlock, autofs_lck_grp);
	FREE(fnip->fi_path, M_AUTOFS);
	FREE(fnip->fi_map, M_AUTOFS);
	FREE(fnip->fi_subdir, M_AUTOFS);
	FREE(fnip->fi_key, M_AUTOFS);
	FREE(fnip->fi_opts, M_AUTOFS);
	FREE(fnip, M_AUTOFS);
	
	/*
	 * One fewer mounted file system.
	 */
	lck_mtx_lock(autofs_global_lock);
	autofs_mounts--;
	lck_mtx_unlock(autofs_global_lock);

	AUTOFS_DPRINT((5, "auto_unmount: return=0\n"));

	return (0);
}

extern int thread_notrigger(void);

/*
 * find root of autofs
 */
static int
auto_root(mount_t mp, vnode_t *vpp, vfs_context_t context)
{
	int error;
	int pid;
	fninfo_t *fnip;
	fnnode_t *fnp;
	struct timeval now;

	fnip = vfstofni(mp);
	*vpp = fnip->fi_rootvp;
	error = vnode_get(*vpp);
	if (error)
		goto done;

	/*
	 * If this is a direct map, and the mount of this file system
	 * (not the mount *on* it, the mount *of* it; see auto_start()
	 * for an explanation) is complete, and we're not trying to unmount
	 * it, and we should trigger a mount for this vnode, do so, so that
	 * our caller gets a vnode with something mounted on it if the mount
	 * can be done.
	 */
	pid = vfs_context_pid(context);
	auto_fninfo_lock_shared(fnip, pid);
	if ((fnip->fi_flags & (MF_DIRECT|MF_MOUNTING|MF_UNMOUNTING)) == MF_DIRECT &&
	    !thread_notrigger()) {
		fnp = vntofn(*vpp);
	retry:
		lck_mtx_lock(fnp->fn_lock);
		if (auto_dont_trigger(*vpp, context)) {
			/*
			 * Don't trigger the mount.
			 */
			lck_mtx_unlock(fnp->fn_lock);
			error = 0;
			goto unlock;
		}
		if (fnp->fn_flags & (MF_LOOKUP | MF_INPROG)) {
			/*
			 * Mount or lookup in progress,
			 * wait for it before proceeding.
			 */
			lck_mtx_unlock(fnp->fn_lock);
			error = auto_wait4mount(fnp, context);
			if (error && error != EAGAIN) {
				vnode_put(*vpp);
				goto unlock;
			}
			error = 0;
			/*
			 * Check again whether we should trigger the
			 * mount - an in-progress mount might have
			 * succeeded.
			 */
			goto retry;
		}

		if ((fnp->fn_flags & MF_MOUNTPOINT) &&
		    fnp->fn_trigger != NULL) {
			assert(fnp->fn_dirents == NULL);
			/*
			 * The filesystem that used to sit here
			 * has been forcibly unmounted.
			 */
			lck_mtx_unlock(fnp->fn_lock);
			error = EIO;
			vnode_put(*vpp);
			goto unlock;
		}

		if (fnp->fn_dirents == NULL) {
			/*
			 * Trigger mount, because this is a direct
			 * mountpoint with no subdirs.
			 */
			AUTOFS_BLOCK_OTHERS(fnp, MF_INPROG);
			fnp->fn_error = 0;
			lck_mtx_unlock(fnp->fn_lock);
			microtime(&now);
			fnp->fn_ref_time = now.tv_sec;
			error = auto_do_mount(fnp, ".", 1, context);
			if (error) {
				if (error == EAGAIN)
					goto retry;
				vnode_put(*vpp);
				goto unlock;
			}
		} else {
			/*
			 * Nothing to trigger, just unlock.
			 */
			lck_mtx_unlock(fnp->fn_lock);
			error = 0;
		}
	}
unlock:
	auto_fninfo_unlock_shared(fnip, pid);

done:
	AUTOFS_DPRINT((5, "auto_root: mp %p, *vpp %p, error %d\n",
	    mp, *vpp, error));
	return (error);
}

/*
 * Get file system information.
 */
static int
auto_vfs_getattr(__unused mount_t mp, struct vfs_attr *vfap,
    __unused vfs_context_t context)
{
	AUTOFS_DPRINT((4, "auto_vfs_getattr %p\n", (void *)mp));

	VFSATTR_RETURN(vfap, f_bsize, AUTOFS_BLOCKSIZE);
	VFSATTR_RETURN(vfap, f_iosize, 512);
	VFSATTR_RETURN(vfap, f_blocks, 0);
	VFSATTR_RETURN(vfap, f_bfree, 0);
	VFSATTR_RETURN(vfap, f_bavail, 0);
	VFSATTR_RETURN(vfap, f_files, 0);
	VFSATTR_RETURN(vfap, f_ffree, 0);

	if (VFSATTR_IS_ACTIVE(vfap, f_capabilities)) {
		/*
		 * We support symlinks (although you can't create them,
		 * or anything else), and hard links (to the extent
		 * that ".." looks like a hard link to the parent).
		 *
		 * We're case-sensitive and case-preserving, and
		 * statfs() doesn't involve any over-the-wire ops,
		 * so it's fast.
		 *
		 * We set the hidden bit on some directories.
		 */
		vfap->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_SYMBOLICLINKS |
			VOL_CAP_FMT_HARDLINKS |
			VOL_CAP_FMT_NO_ROOT_TIMES |
			VOL_CAP_FMT_CASE_SENSITIVE |
			VOL_CAP_FMT_CASE_PRESERVING |
			VOL_CAP_FMT_FAST_STATFS | 
			VOL_CAP_FMT_2TB_FILESIZE |
			VOL_CAP_FMT_HIDDEN_FILES;
		vfap->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] =
			VOL_CAP_INT_ATTRLIST;
		vfap->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		vfap->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

		vfap->f_capabilities.valid[VOL_CAPABILITIES_FORMAT] =
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
			VOL_CAP_FMT_FAST_STATFS |
			VOL_CAP_FMT_2TB_FILESIZE |
			VOL_CAP_FMT_OPENDENYMODES |
			VOL_CAP_FMT_HIDDEN_FILES |
			VOL_CAP_FMT_PATH_FROM_ID |
			VOL_CAP_FMT_NO_VOLUME_SIZES;
		vfap->f_capabilities.valid[VOL_CAPABILITIES_INTERFACES] =
			VOL_CAP_INT_SEARCHFS |
			VOL_CAP_INT_ATTRLIST |
			VOL_CAP_INT_NFSEXPORT |
			VOL_CAP_INT_READDIRATTR |
			VOL_CAP_INT_EXCHANGEDATA |
			VOL_CAP_INT_COPYFILE |
			VOL_CAP_INT_ALLOCATE |
			VOL_CAP_INT_VOL_RENAME |
			VOL_CAP_INT_ADVLOCK |
			VOL_CAP_INT_FLOCK |
			VOL_CAP_INT_EXTENDED_SECURITY |
			VOL_CAP_INT_USERACCESS |
			VOL_CAP_INT_MANLOCK |
			VOL_CAP_INT_EXTENDED_ATTR |
			VOL_CAP_INT_NAMEDSTREAMS;
		vfap->f_capabilities.valid[VOL_CAPABILITIES_RESERVED1] = 0;
		vfap->f_capabilities.valid[VOL_CAPABILITIES_RESERVED2] = 0;
		VFSATTR_SET_SUPPORTED(vfap, f_capabilities);
	}

	if (VFSATTR_IS_ACTIVE(vfap, f_attributes)) {
		vfap->f_attributes.validattr.commonattr =
			ATTR_CMN_NAME | ATTR_CMN_DEVID | ATTR_CMN_FSID |
			ATTR_CMN_OBJTYPE | ATTR_CMN_OBJTAG | ATTR_CMN_OBJID |
			ATTR_CMN_PAROBJID |
			ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | ATTR_CMN_ACCTIME |
			ATTR_CMN_OWNERID | ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK |
			ATTR_CMN_FLAGS | ATTR_CMN_USERACCESS | ATTR_CMN_FILEID;
		vfap->f_attributes.validattr.volattr =
			ATTR_VOL_MOUNTPOINT | ATTR_VOL_MOUNTFLAGS |
			ATTR_VOL_MOUNTEDDEVICE | ATTR_VOL_CAPABILITIES |
			ATTR_VOL_ATTRIBUTES;
		vfap->f_attributes.validattr.dirattr =
			ATTR_DIR_LINKCOUNT | ATTR_DIR_MOUNTSTATUS;
		vfap->f_attributes.validattr.fileattr =
			ATTR_FILE_LINKCOUNT | ATTR_FILE_TOTALSIZE |
			ATTR_FILE_IOBLOCKSIZE | ATTR_FILE_DEVTYPE |
			ATTR_FILE_DATALENGTH;
		vfap->f_attributes.validattr.forkattr = 0;
		
		vfap->f_attributes.nativeattr.commonattr =
			ATTR_CMN_NAME | ATTR_CMN_DEVID | ATTR_CMN_FSID |
			ATTR_CMN_OBJTYPE | ATTR_CMN_OBJTAG | ATTR_CMN_OBJID |
			ATTR_CMN_PAROBJID |
			ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | ATTR_CMN_ACCTIME |
			ATTR_CMN_OWNERID | ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK |
			ATTR_CMN_FLAGS | ATTR_CMN_USERACCESS | ATTR_CMN_FILEID;
		vfap->f_attributes.nativeattr.volattr =
			ATTR_VOL_OBJCOUNT |
			ATTR_VOL_MOUNTPOINT | ATTR_VOL_MOUNTFLAGS |
			ATTR_VOL_MOUNTEDDEVICE | ATTR_VOL_CAPABILITIES |
			ATTR_VOL_ATTRIBUTES;
		vfap->f_attributes.nativeattr.dirattr =
			ATTR_DIR_MOUNTSTATUS;
		vfap->f_attributes.nativeattr.fileattr =
			ATTR_FILE_LINKCOUNT | ATTR_FILE_TOTALSIZE |
			ATTR_FILE_IOBLOCKSIZE | ATTR_FILE_DEVTYPE |
			ATTR_FILE_DATALENGTH;
		vfap->f_attributes.nativeattr.forkattr = 0;

		VFSATTR_SET_SUPPORTED(vfap, f_attributes);
	}

	return (0);
}

/*
 * autofs doesn't have any data or backing store and you can't write into
 * any of the autofs structures, so don't do anything.
 */
static int
auto_sync(__unused mount_t mp, __unused int waitfor,
    __unused vfs_context_t context)
{
	return (0);
}

/*
 * Look up a autofs node by node number.
 * Currently not supported.
 */
static int
auto_vget(__unused mount_t mp, __unused ino64_t ino, __unused vnode_t *vpp,
    __unused vfs_context_t context)
{
	return (ENOTSUP);
}

static int
autofs_sysctl(int *name, u_int namelen, __unused user_addr_t oldp,
    __unused size_t *oldlenp, __unused user_addr_t newp,
    __unused size_t newlen, __unused vfs_context_t context)
{
	int error;
#ifdef DEBUG
	struct sysctl_req *req = NULL;
	uint32_t debug;
#endif
	
	/*
	 * All names at this level are terminal
	 */
	if (namelen > 1)
		return ENOTDIR;		/* overloaded error code */

	error = 0;

#ifdef DEBUG
	req = CAST_DOWN(struct sysctl_req *, oldp);
#endif

	AUTOFS_DPRINT((4, "autofs_sysctl called.\n"));
	switch (name[0]) {
#ifdef DEBUG
	case AUTOFS_CTL_DEBUG:
		error = SYSCTL_IN(req, &debug, sizeof(debug));
		if (error)
			break;
		auto_debug_set(debug);
		break;
#endif
	default:
		error = ENOTSUP;
		break;
	}

	return (error);
}

/*
 * Opening /dev/autofs when nobody else has it open makes you the
 * automounter, so you and all your children never trigger mounts and
 * never get stuck waiting for a mount to complete (as it's your job
 * to complete the mount.
 *
 * Opening /dev/autofs when somebody else has it open fails with EBUSY.
 *
 * Closing /dev/autofs clears automounter_pid, so nobody's the automounter;
 * that means if the automounter exits, it ceases to be the automounter.
 */
static d_open_t	 autofs_dev_open;
static d_close_t autofs_dev_close;
static d_ioctl_t autofs_ioctl;

static struct cdevsw autofs_cdevsw = {
	autofs_dev_open,
	autofs_dev_close,
	eno_rdwrt,	/* d_read */
	eno_rdwrt,	/* d_write */
	autofs_ioctl,
	eno_stop,
	eno_reset,
	0,		/* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0		/* d_type */
};

static int	autofs_major = -1;
static void	*autofs_devfs;

static int	automounter_pid;
static lck_rw_t *autofs_automounter_pid_rwlock;

static int
autofs_dev_open(__unused dev_t dev, __unused int oflags, __unused int devtype,
    struct proc *p)
{
	lck_rw_lock_exclusive(autofs_automounter_pid_rwlock);
	if (automounter_pid != 0) {
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		return (EBUSY);
	}
	automounter_pid = proc_pid(p);
	lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
	return (0);
}

static int
autofs_dev_close(__unused dev_t dev, __unused int flag, __unused int fmt,
    __unused struct proc *p)
{
	lck_rw_lock_exclusive(autofs_automounter_pid_rwlock);
	automounter_pid = 0;
	lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
	return (0);
}

static int
autofs_ioctl(__unused dev_t dev, u_long cmd, __unused caddr_t data,
    __unused int flag, __unused struct proc *p)
{
	struct autofs_globals *fngp;
	int error;

	lck_mtx_lock(autofs_global_lock);
	if ((fngp = autofs_zone_get_globals()) == NULL) {
		fngp = autofs_zone_init();
		autofs_zone_set_globals(fngp);
	}
	lck_mtx_unlock(autofs_global_lock);
	if (fngp == NULL)
		return (ENOMEM);

	switch (cmd) {

	case AUTOFS_NOTIFYCHANGE:
		if (fngp != NULL) {
			/*
			 * Post a flush notification, to wake up the
			 * thread waiting for the flush notification.
			 */
			lck_mtx_lock(fngp->fng_flush_notification_lock);
			fngp->fng_flush_notification_pending = 1;
			wakeup(&fngp->fng_flush_notification_pending);
			lck_mtx_unlock(fngp->fng_flush_notification_lock);
		}
		error = 0;
		break;

	case AUTOFS_WAITFORFLUSH:
		lck_mtx_lock(autofs_global_lock);
		if (fngp == NULL)
			fngp = autofs_zone_init();
		lck_mtx_unlock(autofs_global_lock);
		if (fngp == NULL)
			return (ENOMEM);

		/*
		 * Block until there's a flush notification pending or we
		 * get a signal.
		 */
		error = 0;
		lck_mtx_lock(fngp->fng_flush_notification_lock);
		while (error == 0 && !fngp->fng_flush_notification_pending) {
			error = msleep(&fngp->fng_flush_notification_pending,
			    fngp->fng_flush_notification_lock, PWAIT | PCATCH,
			    "flush notification", NULL);
		}
		fngp->fng_flush_notification_pending = 0;
		lck_mtx_unlock(fngp->fng_flush_notification_lock);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * Opening /dev/autofs_nowait makes you (but not your children) a
 * nowait process; those processes trigger mounts, but don't wait
 * for them to finish - instead, they return ENOENT.  This is used
 * by launchd.
 *
 * Closing /dev/autofs_nowait makes you no longer a nowait process;
 * it's closed on exit, so if you exit, you cease to be a nowait process.
 */
static d_open_t	 autofs_nowait_dev_open;
static d_close_t autofs_nowait_dev_close;

static struct cdevsw autofs_nowait_cdevsw = {
	autofs_nowait_dev_open,
	autofs_nowait_dev_close,
	eno_rdwrt,	/* d_read */
	eno_rdwrt,	/* d_write */
	eno_ioctl,
	eno_stop,
	eno_reset,
	0,		/* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0		/* d_type */
};

static int	autofs_nowait_major = -1;
static void	*autofs_nowait_devfs;

/*
 * Structure representing a process that has registered itself as an
 * nowait process by opening a cloning autofs device.
 */
struct nowait_process {
	LIST_ENTRY(nowait_process) entries;
	int	pid;			/* PID of the nowait process */
	int	minor;			/* minor device they opened */
};

static LIST_HEAD(nowaitproclist, nowait_process) nowait_processes;
static lck_rw_t *autofs_nowait_processes_rwlock;

/*
 * Given the dev entry that's being opened, we clone the device.  This driver
 * doesn't actually use the dev entry, since we alreaqdy know who we are by
 * being called from this code.  This routine is a callback registered from
 * devfs_make_node_clone() in autofs_init(); its purpose is to provide a new
 * minor number, or to return -1, if one can't be provided.
 *
 * Parameters:	dev			The device we are cloning from
 *
 * Returns:	>= 0			A new minor device number
 *		-1			Error: ENOMEM ("Can't alloc device")
 *
 * NOTE:	Called with DEVFS_LOCK() held
 */
static int
autofs_nowait_dev_clone(__unused dev_t dev, int action)
{
	int minor;
	struct nowait_process *nowait_process;

	if (action == DEVFS_CLONE_ALLOC) {
		minor = 0;	/* tentative choice of minor */
		lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
		LIST_FOREACH(nowait_process, &nowait_processes, entries) {
			if (minor < nowait_process->minor) {
				/*
				 * None of the nowait processes we've looked
				 * at so far have this minor, and this
				 * minor is less than all of the minors
				 * later in the list (which is always
				 * sorted in increasing order by minor
				 * device number).
				 *
				 * Therefore, it's not in use.
				 */
				break;
			}

			/*
			 * All minors <= nowait_process->minor are in use.
			 * Try the next one after nowait_process->minor.
			 */
			minor = nowait_process->minor + 1;
		}
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		return (minor);
	}

	return (-1);
}

static int
autofs_nowait_dev_open(dev_t dev, __unused int oflags, __unused int devtype,
    struct proc *p)
{
	struct nowait_process *newnowait_process, *nowait_process, *lastnowait_process;

	MALLOC(newnowait_process, struct nowait_process *, sizeof(*newnowait_process),
	    M_AUTOFS, M_WAITOK);
	if (newnowait_process == NULL)
		return (ENOMEM);
	newnowait_process->pid = proc_pid(p);
	newnowait_process->minor = minor(dev);
	/*
	 * Insert the structure in the list of nowait processes in order by
	 * minor device number.
	 */
	lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
	if (LIST_EMPTY(&nowait_processes)) {
		/*
		 * List is empty, insert at the head.
		 */
		LIST_INSERT_HEAD(&nowait_processes, newnowait_process, entries);
	} else {
		/*
		 * List isn't empty, insert in front of the first entry
		 * with a larger minor device number.
		 */
		LIST_FOREACH(nowait_process, &nowait_processes, entries) {
			if (newnowait_process->minor < nowait_process->minor) {
				/*
				 * This entry is the first one with a larger
				 * minor device number.
				 */
				LIST_INSERT_BEFORE(nowait_process,
				    newnowait_process, entries);
				goto done;
			}
			lastnowait_process = nowait_process;
		}
		/*
		 * lastnowait_process is the last entry in the list, and it
		 * doesn't have a larger minor device number than the new
		 * entry; insert the new entry after it.
		 */
		LIST_INSERT_AFTER(lastnowait_process, newnowait_process,
		    entries);
	}
done:
	lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
	return (0);
}

static int
autofs_nowait_dev_close(dev_t dev, __unused int flag, __unused int fmt,
    __unused struct proc *p)
{
	struct nowait_process *nowait_process;

	/*
	 * Remove the nowait_process structure for this device from the
	 * list of nowait processes.
	 */
	lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
	LIST_FOREACH(nowait_process, &nowait_processes, entries) {
		if (minor(dev) == nowait_process->minor) {
			LIST_REMOVE(nowait_process, entries);
			FREE(nowait_process, M_AUTOFS);
			break;
		}
	}
	lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
	return (0);
}

/*
 * Opening /dev/autofs_control when nobody else has it open lets you perform
 * various ioctls to control autofs.
 *
 * Opening /dev/autofs when somebody else has it open fails with EBUSY.
 * This is used to ensure that only one instance of the automount command
 * is running at a time.
 */
static d_open_t	 auto_control_dev_open;
static d_close_t auto_control_dev_close;
static d_ioctl_t auto_control_ioctl;

static struct cdevsw autofs_control_cdevsw = {
	auto_control_dev_open,
	auto_control_dev_close,
	eno_rdwrt,	/* d_read */
	eno_rdwrt,	/* d_write */
	auto_control_ioctl,
	eno_stop,
	eno_reset,
	0,		/* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0		/* d_type */
};

static int	autofs_control_major = -1;
static void	*autofs_control_devfs;

static int	autofs_control_isopen;
static lck_mtx_t *autofs_control_isopen_lock;

static int
auto_control_dev_open(__unused dev_t dev, __unused int oflags,
    __unused int devtype, __unused struct proc *p)
{
	lck_mtx_lock(autofs_control_isopen_lock);
	if (autofs_control_isopen) {
		lck_mtx_unlock(autofs_control_isopen_lock);
		return (EBUSY);
	}
	autofs_control_isopen = 1;
	lck_mtx_unlock(autofs_control_isopen_lock);
	return (0);
}

static int
auto_control_dev_close(__unused dev_t dev, __unused int flag,
    __unused int fmt, __unused struct proc *p)
{
	lck_mtx_lock(autofs_control_isopen_lock);
	autofs_control_isopen = 0;
	lck_mtx_unlock(autofs_control_isopen_lock);
	return (0);
}

static int
auto_control_ioctl(__unused dev_t dev, u_long cmd, caddr_t data,
    __unused int flag, proc_t p)
{
	struct autofs_globals *fngp;
	int error;
	struct autofs_update_args_32 *update_argsp_32;
	struct autofs_update_args_64 update_args;
	mount_t mp;
	fninfo_t *fnip;

	lck_mtx_lock(autofs_global_lock);
	if ((fngp = autofs_zone_get_globals()) == NULL) {
		fngp = autofs_zone_init();
		autofs_zone_set_globals(fngp);
	}
	lck_mtx_unlock(autofs_global_lock);
	if (fngp == NULL)
		return (ENOMEM);

	switch (cmd) {

	case AUTOFS_UPDATE_OPTIONS_32:
		update_argsp_32 = (struct autofs_update_args_32 *)data;
		update_args.fsid = update_argsp_32->fsid;
		update_args.opts = CAST_USER_ADDR_T(update_argsp_32->opts);
		update_args.map = CAST_USER_ADDR_T(update_argsp_32->map);
		update_args.mntflags = update_argsp_32->mntflags;
		update_args.mount_to = update_argsp_32->mount_to;
		update_args.mach_to = update_argsp_32->mach_to;
		update_args.direct = update_argsp_32->direct;
		error = auto_update_options(&update_args, proc_pid(p));
		break;
		
	case AUTOFS_UPDATE_OPTIONS_64:
		error = auto_update_options((struct autofs_update_args_64 *)data,
		    proc_pid(p));
		break;

	case AUTOFS_NOTIFYCHANGE:
		if (fngp != NULL) {
			/*
			 * Post a flush notification, to provoke the
			 * automounter to flush its cache.
			 */
			lck_mtx_lock(fngp->fng_flush_notification_lock);
			fngp->fng_flush_notification_pending = 1;
			wakeup(&fngp->fng_flush_notification_pending);
			lck_mtx_unlock(fngp->fng_flush_notification_lock);
		}
		error = 0;
		break;

	case AUTOFS_UNMOUNT:
		error = 0;
		if (fngp != NULL) {
			/*
			 * Try to unmount the specified autofs subtree
			 * immediately, so that we can unmount the trigger
			 * that mounted it.
			 */
			unmount_tree(fngp, (fsid_t *)data,
			    UNMOUNT_TREE_IMMEDIATE);

			/*
			 * This trigger is being unmounted because automount
			 * didn't find any fstab or automounter map entry
			 * for it.  Don't trigger any mounts on it.
			 */
			mp = vfs_getvfs((fsid_t *)data);
			if (mp == NULL) {
				error = ENOENT;
				break;
			}
			if (!auto_is_autofs(mp)) {
				error = EINVAL;
				break;
			}
			fnip = vfstofni(mp);

			/*
			 * Mark this as being unmounted, so that we
			 * don't trigger any mounts on top of it (for
			 * a direct map) and return ENOENT for any
			 * lookups under it (for an indirect map).
			 *
			 * We block mounts on top of it for reasons
			 * described below; we fail lookups under it
			 * so that nobody creates triggers under us
			 * while we're being unmounted, as that can
			 * cause the root vnode of the indirect map
			 * to have links to it while it's being
			 * removed from the list of autofs mounts,
			 * causing 6491044.
			 */
			lck_rw_lock_exclusive(fnip->fi_rwlock);
			fnip->fi_flags |= MF_UNMOUNTING;
			lck_rw_unlock_exclusive(fnip->fi_rwlock);
			
			/*
			 * Wait for any in-progress mounts on the root vnode
			 * of the mount; to complete; otherwise, we can get
			 * a deadlock if:
			 *
			 *    a thread calls VFS_ROOT() on a direct map
			 *    trigger, triggering a mount - at this point,
			 *    that thread has done a vfs_busy() on the
			 *    mount_t for the direct map autofs mount, so
			 *    it's holding a shared lock on the mount_t;
			 *
			 *    automount then tries to unmount that autofs
			 *    mount, which means it tries to grab an
			 *    exclusive lock on the mount_t and blocks;
			 *
			 *    the automounter does a lookup while trying to
			 *    do the mount requested by the thread above,
			 *    and does a vfs_busy(), which blocks because
			 *    it tries to get a shared lock but somebody
			 *    wants an exclusive lock.
			 *
			 * Once this finishes, we know there won't be any
			 * more automounts on it, as we set MF_UNMOUNTING.
			 */
			auto_wait4mount(vntofn(fnip->fi_rootvp),
			    vfs_context_current());

			/*
			 * Now unmount the trigger.
			 */
			error = vfs_unmountbyfsid((fsid_t *)data, 0,
			    vfs_context_current());

			/*
			 * If that failed, we're no longer in the middle
			 * of unmounting it.  (If it succeeded, it no
			 * longer exists, so we can't unmark it as being
			 * in the middle of being unmounted.
			 */
			if (error != 0) {
				lck_rw_lock_exclusive(fnip->fi_rwlock);
				fnip->fi_flags &= ~MF_UNMOUNTING;
				lck_rw_unlock_exclusive(fnip->fi_rwlock);
			}
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * Check whether this process is a automounter or an inferior of a automounter.
 */
int
auto_is_automounter(int pid)
{
	int is_automounter;

	lck_rw_lock_shared(autofs_automounter_pid_rwlock);
	is_automounter = (automounter_pid != 0 &&
	    (pid == automounter_pid || proc_isinferior(pid, automounter_pid)));
	lck_rw_unlock_shared(autofs_automounter_pid_rwlock);
	return (is_automounter);
}

/*
 * Check whether this process is a nowait process.
 */
int
auto_is_nowait_process(int pid)
{
	struct nowait_process *nowait_process;

	lck_rw_lock_shared(autofs_nowait_processes_rwlock);
	LIST_FOREACH(nowait_process, &nowait_processes, entries) {
		if (pid == nowait_process->pid) {
			lck_rw_unlock_shared(autofs_nowait_processes_rwlock);
			return (1);
		}
	}
	lck_rw_unlock_shared(autofs_nowait_processes_rwlock);
	return (0);
}

/*
 * Initialize the filesystem
 */
__private_extern__ int
auto_module_start(__unused kmod_info_t *ki, __unused void *data)
{
	errno_t error;

	/*
	 * Set up the lock group and the global locks.
	 */
	autofs_lck_grp = lck_grp_alloc_init("autofs", NULL);
	if (autofs_lck_grp == NULL) {
		IOLog("auto_module_start: Couldn't create autofs lock group\n");
		goto fail;
	}
	autofs_global_lock = lck_mtx_alloc_init(autofs_lck_grp, LCK_ATTR_NULL);
	if (autofs_global_lock == NULL) {
		IOLog("auto_module_start: Couldn't create autofs global lock\n");
		goto fail;
	}
	autofs_nodeid_lock = lck_mtx_alloc_init(autofs_lck_grp, LCK_ATTR_NULL);
	if (autofs_nodeid_lock == NULL) {
		IOLog("auto_module_start: Couldn't create autofs node ID lock\n");
		goto fail;
	}
	autofs_automounter_pid_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
	if (autofs_automounter_pid_rwlock == NULL) {
		IOLog("auto_module_start: Couldn't create autofs automounter pid lock\n");
		goto fail;
	}
	autofs_nowait_processes_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
	if (autofs_nowait_processes_rwlock == NULL) {
		IOLog("auto_module_start: Couldn't create autofs nowait_processes list lock\n");
		goto fail;
	}
	autofs_control_isopen_lock = lck_mtx_alloc_init(autofs_lck_grp, LCK_ATTR_NULL);
	if (autofs_control_isopen_lock == NULL) {
		IOLog("auto_module_start: Couldn't create autofs control device lock\n");
		goto fail;
	}
	
	/*
	 * Add the autofs device.
	 */
	autofs_major = cdevsw_add(-1, &autofs_cdevsw);
	if (autofs_major == -1) {
		IOLog("auto_module_start: cdevsw_add failed on autofs device\n");
		goto fail;
	}
	autofs_devfs = devfs_make_node(makedev(autofs_major, 0),
	    DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600, AUTOFS_DEVICE);
	if (autofs_devfs == NULL) {
		IOLog("auto_module_start: devfs_make_node failed on autofs device\n");
		goto fail;
	}
	
	/*
	 * Add the autofs nowait device.  Everybody's allowed to open it.
	 */
	autofs_nowait_major = cdevsw_add(-1, &autofs_nowait_cdevsw);
	if (autofs_nowait_major == -1) {
		IOLog("auto_module_start: cdevsw_add failed on autofs_nowait device\n");
		goto fail;
	}
	autofs_nowait_devfs = devfs_make_node_clone(makedev(autofs_nowait_major, 0),
	    DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, autofs_nowait_dev_clone,
	    AUTOFS_NOWAIT_DEVICE);
	if (autofs_nowait_devfs == NULL) {
		IOLog("auto_module_start: devfs_make_node failed on autofs nowait device\n");
		goto fail;
	}

	/*
	 * Add the autofs control device.
	 */
	autofs_control_major = cdevsw_add(-1, &autofs_control_cdevsw);
	if (autofs_control_major == -1) {
		IOLog("auto_module_start: cdevsw_add failed on autofs control device\n");
		goto fail;
	}
	autofs_control_devfs = devfs_make_node(makedev(autofs_control_major, 0),
	    DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600, AUTOFS_CONTROL_DEVICE);
	if (autofs_control_devfs == NULL) {
		IOLog("auto_module_start: devfs_make_node failed on autofs control device\n");
		goto fail;
	}

	/*
	 * Register the file system.
	 */
	error = vfs_fsadd(&autofs_fsentry, &auto_vfsconf);
	if (error != 0) {
		IOLog("auto_module_start: Error %d from vfs_fsadd\n",
		    error);
		goto fail;
	}

	return (KERN_SUCCESS);

fail:
	if (autofs_control_devfs != NULL)
		devfs_remove(autofs_control_devfs);
	if (autofs_control_major != -1) {
		if (cdevsw_remove(autofs_control_major, &autofs_control_cdevsw) == -1)
			panic("auto_module_start: can't remove autofs control device from cdevsw");
	}
	if (autofs_nowait_devfs != NULL)
		devfs_remove(autofs_nowait_devfs);
	if (autofs_nowait_major != -1) {
		if (cdevsw_remove(autofs_nowait_major, &autofs_nowait_cdevsw) == -1)
			panic("auto_module_start: can't remove autofs nowait device from cdevsw");
	}
	if (autofs_devfs != NULL)
		devfs_remove(autofs_devfs);
	if (autofs_major != -1) {
		if (cdevsw_remove(autofs_major, &autofs_cdevsw) == -1)
			panic("auto_module_start: can't remove autofs device from cdevsw");
	}
	if (autofs_control_isopen_lock != NULL)
		lck_mtx_free(autofs_control_isopen_lock, autofs_lck_grp);
	if (autofs_nowait_processes_rwlock != NULL)
		lck_rw_free(autofs_nowait_processes_rwlock, autofs_lck_grp);
	if (autofs_automounter_pid_rwlock != NULL)
		lck_rw_free(autofs_automounter_pid_rwlock, autofs_lck_grp);
	if (autofs_nodeid_lock != NULL)
		lck_mtx_free(autofs_nodeid_lock, autofs_lck_grp);
	if (autofs_global_lock != NULL)
		lck_mtx_free(autofs_global_lock, autofs_lck_grp);
	if (autofs_lck_grp != NULL)
		lck_grp_free(autofs_lck_grp);
	return (KERN_FAILURE);
}

__private_extern__ int
auto_module_stop(__unused kmod_info_t *ki, __unused void *data)
{
	struct autofs_globals *fngp;
	int error;
	
	lck_mtx_lock(autofs_global_lock);
	lck_rw_lock_exclusive(autofs_automounter_pid_rwlock);
	lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
	lck_mtx_lock(autofs_control_isopen_lock);
	if (autofs_mounts != 0) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, still %u mounts active\n", autofs_mounts));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (automounter_pid != 0) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, automounter still running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (!LIST_EMPTY(&nowait_processes)) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, still nowait processes running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (autofs_control_isopen) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, automount command is running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	AUTOFS_DPRINT((10, "auto_module_stop: removing autofs from vfs conf. list...\n"));

	fngp = autofs_zone_get_globals();
	if (fngp) {
		assert(fngp->fng_fnnode_count == 1);
		assert(fngp->fng_unmount_threads == 0);
	}

	error = vfs_fsremove(auto_vfsconf);
	if (error) {
		IOLog("auto_module_stop: Error %d from vfs_remove\n",
		    error);
		return (KERN_FAILURE);
	}

	devfs_remove(autofs_devfs);
	autofs_devfs = NULL;
	if (cdevsw_remove(autofs_major, &autofs_cdevsw) == -1)
		panic("auto_module_stop: can't remove autofs device from cdevsw");
	autofs_major = -1;
	devfs_remove(autofs_nowait_devfs);
	autofs_nowait_devfs = NULL;
	if (cdevsw_remove(autofs_nowait_major, &autofs_nowait_cdevsw) == -1)
		panic("auto_module_stop: can't remove autofs nowait device from cdevsw");
	autofs_nowait_major = -1;
	devfs_remove(autofs_control_devfs);
	autofs_control_devfs = NULL;
	if (cdevsw_remove(autofs_control_major, &autofs_control_cdevsw) == -1)
		panic("auto_module_start: can't remove autofs control device from cdevsw");

	if (fngp) {
		/*
		 * Tell the unmounter thread to go away.
		 */
		lck_mtx_lock(fngp->fng_unmount_threads_lock);
		fngp->fng_terminate_do_unmount_thread = 1;
		wakeup(&fngp->fng_terminate_do_unmount_thread);

		/*
		 * Wait for it to go away.
		 */
		while (!fngp->fng_do_unmount_thread_terminated) {
			msleep(&fngp->fng_do_unmount_thread_terminated,
			    fngp->fng_unmount_threads_lock, PWAIT,
			    "unmount thread terminated", NULL);
		}
		lck_mtx_unlock(fngp->fng_unmount_threads_lock);

		/*
		 * Free up the root fnnode.
		 */
		FREE(fngp->fng_rootfnnodep->fn_name, M_AUTOFS);
		FREE(fngp->fng_rootfnnodep, M_AUTOFS);

		lck_mtx_free(fngp->fng_unmount_threads_lock, autofs_lck_grp);
		lck_mtx_free(fngp->fng_flush_notification_lock, autofs_lck_grp);
		FREE(fngp, M_AUTOFS);
	}

	lck_mtx_unlock(autofs_nodeid_lock);
	lck_mtx_free(autofs_nodeid_lock, autofs_lck_grp);
	lck_mtx_unlock(autofs_global_lock);
	lck_mtx_free(autofs_global_lock, autofs_lck_grp);
	lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
	lck_rw_free(autofs_automounter_pid_rwlock, autofs_lck_grp);
	lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
	lck_rw_free(autofs_nowait_processes_rwlock, autofs_lck_grp);
	lck_mtx_unlock(autofs_control_isopen_lock);
	lck_mtx_free(autofs_control_isopen_lock, autofs_lck_grp);
	lck_grp_free(autofs_lck_grp);
	
	return (KERN_SUCCESS);
}
