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
 * Portions Copyright 2007-2016 Apple Inc.
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
#include "triggers.h"
#include "triggers_priv.h"
#include "autofs_kern.h"

lck_grp_t *autofs_lck_grp;
lck_mtx_t *autofs_global_lock;
lck_mtx_t *autofs_nodeid_lock;

u_int autofs_mounts;

__private_extern__ int auto_module_start(kmod_info_t *, void *);
__private_extern__ int auto_module_stop(kmod_info_t *, void *);

extern uint32_t vnode_recycle_on_inactive;

SYSCTL_DECL(_vfs_generic);
SYSCTL_NODE(_vfs_generic, OID_AUTO, autofs, CTLFLAG_RW, 0, "Automounter (autofs) file system");
SYSCTL_INT(_vfs_generic_autofs, OID_AUTO, vnode_recycle_on_inactive, CTLFLAG_RW, &vnode_recycle_on_inactive, 1, "Should autofs vnodes be recycled when become inactive");
/*
 * autofs VFS operations
 */
static int auto_mount(mount_t, vnode_t, user_addr_t, vfs_context_t);
static int auto_start(mount_t, int, vfs_context_t);
static int auto_unmount(mount_t, int, vfs_context_t);
static int auto_vfs_getattr(mount_t, struct vfs_attr *, vfs_context_t);
static int auto_sync(mount_t, int, vfs_context_t);
static int auto_vget(mount_t, ino64_t, vnode_t *, vfs_context_t);
static int autofs_sysctl(int *, u_int, user_addr_t, size_t *, user_addr_t, size_t, vfs_context_t);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

static struct vfsops autofs_vfsops = {
	auto_mount,
	auto_start,
	auto_unmount,
	auto_root,
	NULL,                   /* auto_quotactl */
	auto_vfs_getattr,       /* was auto_statfs */
	auto_sync,
	auto_vget,
	NULL,                   /* auto_fhtovp */
	NULL,                   /* auto_vptofh */
	NULL,                   /* auto_init */
	autofs_sysctl,
	NULL,                   /* auto_vfs_setattr */
	// Remaining ops unused
};

#pragma clang diagnostic pop

extern struct vnodeopv_desc autofsfs_vnodeop_opv_desc;
struct vnodeopv_desc * autofs_vnodeop_opv_descs[] =
{&autofsfs_vnodeop_opv_desc, NULL};

struct vfs_fsentry autofs_fsentry = {
	&autofs_vfsops,                 /* vfs operations */
	1,                              /* # of vnodeopv_desc being registered (reg, spec, fifo ...) */
	autofs_vnodeop_opv_descs,       /* null terminated;  */
	0,                              /* historic filesystem type number [ unused w. VFS_TBLNOTYPENUM specified ] */
	MNTTYPE_AUTOFS,                 /* filesystem type name */
	VFS_TBLTHREADSAFE | VFS_TBLNOTYPENUM | VFS_TBL64BITREADY | VFS_TBLNATIVEXATTR,
	/* defines the FS capabilities */
	{ NULL, NULL }                  /* reserved for future use; set this to zero*/
};



/*
 * No zones in OS X, so only one autofs_globals structure.
 */
static struct autofs_globals *global_fngp;

struct autofs_globals *
autofs_zone_get_globals(void)
{
	return global_fngp;
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

	fngp = kalloc_type(struct autofs_globals, Z_WAITOK | Z_ZERO);
	if (fngp == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs globals structure\n");
		goto fail;
	}

	/*
	 * Allocate a "dummy" fnnode, not associated with any vnode.
	 */
	fnp = kalloc_type(struct fnnode, Z_WAITOK | Z_ZERO);
	if (fnp == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global fnnode\n");
		goto fail;
	}

	fnp->fn_namelen = sizeof fnnode_name;
	fnp->fn_name = kalloc_data(fnp->fn_namelen + 1, Z_WAITOK | Z_ZERO);
	if (fnp->fn_name == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global fnnode name\n");
		goto fail;
	}
	bcopy(fnnode_name, fnp->fn_name, sizeof fnnode_name);
	fnp->fn_name[sizeof fnnode_name] = '\0';
	fnp->fn_mode = AUTOFS_MODE;
	microtime(&now);
	fnp->fn_crtime = fnp->fn_atime = fnp->fn_mtime = fnp->fn_ctime = now;
	fnp->fn_nodeid = 1;     /* XXX - could just be zero?  XXXab: should be 2 actually */
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

	fnp->fn_mnt_lock = lck_mtx_alloc_init(autofs_lck_grp, NULL);
	if (fnp->fn_mnt_lock == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global fnnode mnt mutex\n");
		goto fail;
	}

	fngp->fng_rootfnnodep = fnp;
	fngp->fng_fnnode_count = 1;
	fngp->fng_printed_not_running_msg = 0;
	fngp->fng_flush_notification_lock = lck_mtx_alloc_init(autofs_lck_grp, LCK_ATTR_NULL);
	if (fngp->fng_flush_notification_lock == NULL) {
		IOLog("autofs_zone_init: Couldn't create autofs global flush notification lock\n");
		goto fail;
	}

	return fngp;

fail:
	if (fnp != NULL) {
		if (fnp->fn_mnt_lock != NULL) {
			lck_mtx_free(fnp->fn_mnt_lock, autofs_lck_grp);
		}
		if (fnp->fn_rwlock != NULL) {
			lck_rw_free(fnp->fn_rwlock, autofs_lck_grp);
		}
		if (fnp->fn_lock != NULL) {
			lck_mtx_free(fnp->fn_lock, autofs_lck_grp);
		}
		if (fnp->fn_name != NULL) {
			kfree_data(fnp->fn_name, fnp->fn_namelen + 1);
		}
		kfree_type(struct fnnode, fnp);
	}

	if (fngp != NULL) {
		if (fngp->fng_flush_notification_lock != NULL) {
			lck_mtx_free(fngp->fng_flush_notification_lock, autofs_lck_grp);
		}
		kfree_type(struct autofs_globals, fngp);
	}

	return NULL;
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
				if ((sc = *s++) == 0) {
					return NULL;
				}
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return (char *)s;
}

/*
 * A "struct mntopt" table is terminated with an entry with a null
 * m_option pointer; therefore, the number of real entries in the
 * table is one fewer than the total number of entries.
 */
static const struct mntopt restropts[] = {
	RESTRICTED_MNTOPTS
};
#define NROPTS  ((sizeof (restropts)/sizeof (restropts[0])) - 1)

/*
 * Check whether the specified option is set in the specified file
 * system.
 */
static uint64_t
optionisset(mount_t mp, const struct mntopt *opt)
{
	uint64_t flags;

	flags = vfs_flags(mp) & opt->m_flag;
	if (opt->m_inverse) {
		return !flags;
	} else {
		return flags;
	}
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
	if (!(fnip->fi_mntflags & AUTOFS_MNT_RESTRICT)) {
		return 0;
	}

	for (i = 0; i < NROPTS; i++) {
		size_t olen = strlen(restropts[i].m_option);

		/* Add "restrict" always and the others insofar set */
		if ((i == 0 || optionisset(mp, &restropts[i])) &&
		    ((p = strstr(buf, restropts[i].m_option)) == NULL ||
		    !((p == buf || p[-1] == ',') &&
		    (p[olen] == '\0' || p[olen] == ',')))) {
			if (len + olen + 1 > maxlen) {
				return -1;
			}

			if (*buf != '\0') {
				buf[len++] = ',';
			}
			bcopy(restropts[i].m_option, &buf[len], olen);
			len += olen;
		}
	}
	if (len + 1 > maxlen) {
		return -1;
	}
	buf[len++] = '\0';
	*curlen = len;
	return 0;
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
	char strbuff[PATH_MAX + 1];
	uint64_t flags;
#ifdef DEBUG
	lck_attr_t *lckattr;
#endif
	struct autofs_globals *fngp;
	int node_type;
	boolean_t lu_verbose;

	AUTOFS_DPRINT((4, "auto_mount: mp %p devvp %p\n", mp, devvp));

	lck_mtx_lock(autofs_global_lock);
	if ((fngp = autofs_zone_get_globals()) == NULL) {
		fngp = autofs_zone_init();
		autofs_zone_set_globals(fngp);
	}
	lck_mtx_unlock(autofs_global_lock);
	if (fngp == NULL) {
		return ENOMEM;
	}

	/*
	 * Get argument version
	 */
	error = copyin(data, (caddr_t)&argsvers, sizeof(argsvers));
	if (error) {
		return error;
	}

	/*
	 * Get arguments
	 */
	switch (argsvers) {
	case 2:
		if (vfs_context_is64bit(context)) {
			error = copyin(data, &args, sizeof(args));
		} else {
			struct autofs_args_32 args32;

			error = copyin(data, &args32, sizeof(args32));
			if (error == 0) {
				args.path = CAST_USER_ADDR_T(args32.path);
				args.opts = CAST_USER_ADDR_T(args32.opts);
				args.map = CAST_USER_ADDR_T(args32.map);
				args.subdir = CAST_USER_ADDR_T(args32.subdir);
				args.key = CAST_USER_ADDR_T(args32.key);
				args.mntflags = args32.mntflags;
				args.direct = args32.direct;
				args.mount_type = args32.mount_type;
				args.node_type = args32.node_type;
			}
		}
		if (error) {
			return error;
		}
		break;

	default:
		return EINVAL;
	}

	/*
	 * We don't support remounts; the AUTOFS_UPDATE_OPTIONS ioctl
	 * should be used for that.
	 */
	flags = vfs_flags(mp) & MNT_CMDFLAGS;
	if (flags & MNT_UPDATE) {
		return EINVAL;
	}

	/*
	 * Allocate fninfo struct and attach it to vfs
	 */
	fnip = kalloc_type(struct fninfo, Z_WAITOK | Z_ZERO);
	if (fnip == NULL) {
		return ENOMEM;
	}

	/*
	 * Assign a unique fsid to the mount
	 */
	vfs_getnewfsid(mp);
	vfs_setfsprivate(mp, fnip);

	/*
	 * Get path for mountpoint
	 */
	error = copyinstr(args.path, strbuff, sizeof(strbuff), &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_path = kalloc_data(len, Z_WAITOK);
	if (fnip->fi_path == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_pathlen = (int)len;
	bcopy(strbuff, fnip->fi_path, len);

	/*
	 * Get default options
	 */
	error = copyinstr(args.opts, strbuff, sizeof(strbuff), &len);
	if (error != 0) {
		goto errout;
	}

	if (autofs_restrict_opts(mp, strbuff, sizeof(strbuff), &len) != 0) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_opts = kalloc_data(len, Z_WAITOK);
	if (fnip->fi_opts == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_optslen = (int)len;
	bcopy(strbuff, fnip->fi_opts, len);

	/*
	 * Get context/map name
	 */
	error = copyinstr(args.map, strbuff, sizeof(strbuff), &len);
	if (error) {
		goto errout;
	}

	fnip->fi_map = kalloc_data(len, Z_WAITOK);
	if (fnip->fi_map == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_maplen = (int)len;
	bcopy(strbuff, fnip->fi_map, len);

	switch (args.mount_type) {
	case MOUNT_TYPE_MAP:
		/*
		 * Top-level mount of a map, done by automount.
		 * Show it as "map XXX".
		 */
		snprintf(vfs_statfs(mp)->f_mntfromname, sizeof(vfs_statfs(mp)->f_mntfromname),
		    "map %.*s", fnip->fi_maplen, fnip->fi_map);
		break;

	case MOUNT_TYPE_TRIGGERED_MAP:
		/*
		 * Map mounted as a result of an automounter map
		 * entry that said "do an autofs mount here".
		 * Show it as "triggered map XXX", to let
		 * the automount command know it's not a top-level
		 * map and that it shouldn't unmount it if auto_master
		 * doesn't mention it.
		 */
		snprintf(vfs_statfs(mp)->f_mntfromname, sizeof(vfs_statfs(mp)->f_mntfromname),
		    "triggered map %.*s", fnip->fi_maplen, fnip->fi_map);
		break;

	case MOUNT_TYPE_SUBTRIGGER:
		/*
		 * Mark this in the from name as a subtrigger, both
		 * to let users know that it's a subtrigger and to
		 * let the automount command know that it's a subtrigger
		 * and that it shouldn't unmount it if auto_master
		 * doesn't mention it.
		 */
		strncpy(vfs_statfs(mp)->f_mntfromname, "subtrigger",
		    sizeof(vfs_statfs(mp)->f_mntfromname) - 1);
		vfs_statfs(mp)->f_mntfromname[sizeof(vfs_statfs(mp)->f_mntfromname) - 1] = (char)0;

		/*
		 * Flag it as a (sub)trigger for our purposes as well.
		 */
		fnip->fi_flags |= MF_SUBTRIGGER;
		break;
	}

	/*
	 * Get subdirectory within map
	 */
	error = copyinstr(args.subdir, strbuff, sizeof(strbuff), &len);
	if (error) {
		goto errout;
	}
	fnip->fi_subdir = kalloc_data(len, Z_WAITOK);
	if (fnip->fi_subdir == NULL) {
		error = ENOMEM;
		goto errout;
	}
	fnip->fi_subdirlen = (int)len;
	bcopy(strbuff, fnip->fi_subdir, len);

	/*
	 * Get the key
	 */
	error = copyinstr(args.key, strbuff, sizeof(strbuff), &len);
	if (error) {
		goto errout;
	}
	fnip->fi_key = kalloc_data(len, Z_WAITOK);
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
	if (args.direct == 1) {
		fnip->fi_flags |= MF_DIRECT;
	}

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

	fnip->fi_busy_shared = 0;
	fnip->fi_busy_exclusive = 0;
	fnip->fi_busy_mtx = lck_mtx_alloc_init(autofs_lck_grp, NULL);
	if (args.direct == 1) {
		/*
		 * For subtriggers, mark this not just as a trigger,
		 * but as a "real" trigger, rather than a "just mounts
		 * autofs" trigger, so that the higher-level frameworks
		 * will see it as ultimately having a real file system
		 * mounted on it, regardless of whether the autofs
		 * trigger is mounted atop it.
		 *
		 * Otherwise, do a lookup on the root node, to determine
		 * whether anything is supposed to be mounted on it or not,
		 * i.e. whether it's a trigger.
		 *
		 * XXX - this means that, if the map entry changes,
		 * the vnode type can't change from trigger to
		 * non-trigger or vice versa.  It also means that
		 * if a map changes from direct to indirect, or
		 * vice versa, the vnode type can't change.
		 */
		if (args.mount_type == MOUNT_TYPE_SUBTRIGGER) {
			node_type = NT_TRIGGER;
		} else {
			error = auto_lookup_request(fnip, fnip->fi_key,
			    fnip->fi_keylen, fnip->fi_subdir,
			    context, &node_type, &lu_verbose);
			if (error) {
				/*
				 * Well, this will get an error when we
				 * try to refer to it, so let's call it
				 * a trigger, so the error will show up.
				 */
				node_type = NT_TRIGGER;
			}
		}
	} else {
		/*
		 * The root directory of an indirect map is never a
		 * trigger.
		 */
		node_type = 0;
	}

	/*
	 * Make the root vnode.
	 */
	error = auto_makefnnode(&rootfnp, node_type, mp, NULL, "", NULL,
	    1, fngp);
	if (error) {
		goto errout;
	}
	myrootvp = fntovn(rootfnp);

	rootfnp->fn_mode = AUTOFS_MODE;
	rootfnp->fn_parent = rootfnp;
	/* account for ".." entry (fn_parent) */
	rootfnp->fn_linkcnt = 1;
	error = vnode_ref(myrootvp);    /* released in auto_unmount */
	if (error) {
		vnode_recycle(myrootvp);
		vnode_put(myrootvp);
		goto errout;
	}
	rootfnp->fn_parentvp = myrootvp;
	rootfnp->fn_parentvid = vnode_vid(myrootvp);
	fnip->fi_rootvp = myrootvp;

	/*
	 * Add to list of top level AUTOFS's if this is a top-level
	 * mount.
	 */
	if (args.mount_type == MOUNT_TYPE_MAP) {
		lck_rw_lock_exclusive(fngp->fng_rootfnnodep->fn_rwlock);
		rootfnp->fn_parent = fngp->fng_rootfnnodep;
		/*fngp->fng_rootfnnodep does not have an associated vp */
		rootfnp->fn_parentvp = NULL;
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

	return 0;

errout:
	assert(fnip != NULL);

	if (fnip->fi_rwlock != NULL) {
		lck_rw_free(fnip->fi_rwlock, autofs_lck_grp);
	}
	if (fnip->fi_busy_mtx) {
		lck_mtx_free(fnip->fi_busy_mtx, autofs_lck_grp);
	}
	if (fnip->fi_path != NULL) {
		kfree_data(fnip->fi_path, fnip->fi_pathlen);
	}
	if (fnip->fi_opts != NULL) {
		kfree_data(fnip->fi_opts, fnip->fi_optslen);
	}
	if (fnip->fi_map != NULL) {
		kfree_data(fnip->fi_map, fnip->fi_maplen);
	}
	if (fnip->fi_subdir != NULL) {
		kfree_data(fnip->fi_subdir, fnip->fi_subdirlen);
	}
	if (fnip->fi_key != NULL) {
		kfree_data(fnip->fi_key, fnip->fi_keylen);
	}
	kfree_type(struct fninfo, fnip);

	AUTOFS_DPRINT((5, "auto_mount: vfs %p root %p fnip %p return %d\n",
	    (void *)mp, (void *)myrootvp, (void *)fnip, error));

	return error;
}

static int
auto_update_options(struct autofs_update_args_64 *update_argsp)
{
	mount_t mp;
	fninfo_t *fnip;
	fnnode_t *fnp;
	char strbuff[PATH_MAX + 1];
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
	if (mp == NULL) {
		return ENOENT;        /* no such mount */
	}
	/*
	 * Make sure this is an autofs mount.
	 */
	if (!auto_is_autofs(mp)) {
		return EINVAL;
	}

	fnip = vfstofni(mp);
	if (fnip == NULL) {
		return EINVAL;
	}

	/*
	 * We can't change the map type if the top-level directory has
	 * subdirectories (i.e., if stuff has happened under it).
	 *
	 * XXX - change whether it's a trigger or not?
	 */
	if ((update_argsp->direct == 1 && !(fnip->fi_flags & MF_DIRECT)) ||
	    (update_argsp->direct != 1 && (fnip->fi_flags & MF_DIRECT))) {
		fnp = vntofn(fnip->fi_rootvp);
		if (fnp->fn_dirents != NULL) {
			return EINVAL;
		}
	}

	/*
	 * Get default options
	 */
	error = copyinstr(update_argsp->opts, strbuff, sizeof(strbuff),
	    &optslen);
	if (error) {
		return error;
	}

	if (autofs_restrict_opts(mp, strbuff, sizeof(strbuff), &optslen) != 0) {
		return EFAULT;
	}

	opts = kalloc_data(optslen, Z_WAITOK);
	bcopy(strbuff, opts, optslen);

	/*
	 * Get context/map name
	 */
	error = copyinstr(update_argsp->map, strbuff, sizeof(strbuff),
	    &maplen);
	if (error) {
		kfree_data(opts, optslen);
		return error;
	}
	map = kalloc_data(maplen, Z_WAITOK);
	bcopy(strbuff, map, maplen);

	/*
	 * We've fetched all the strings; lock out any other references
	 * to fnip, and update the mount information.
	 */
	lck_rw_lock_exclusive(fnip->fi_rwlock);
	was_nobrowse = auto_nobrowse(fnip->fi_rootvp);
	kfree_data(fnip->fi_opts, fnip->fi_optslen);
	fnip->fi_opts = opts;
	fnip->fi_optslen = (int)optslen;
	kfree_data(fnip->fi_map, fnip->fi_maplen);
	fnip->fi_map = map;
	fnip->fi_maplen = (int)maplen;
	if (update_argsp->direct == 1) {
		fnip->fi_flags |= MF_DIRECT;
	} else {
		fnip->fi_flags &= ~MF_DIRECT;
	}
	fnip->fi_flags &= ~MF_UNMOUNTING;
	fnip->fi_mntflags = update_argsp->mntflags;

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
		auto_get_attributes(fnip->fi_rootvp, &vattr);
		vnode_notify(fnip->fi_rootvp, VNODE_EVENT_WRITE, &vattr);
	}
	return 0;
}

int
auto_start(__unused mount_t mp, __unused int flags,
    __unused vfs_context_t context)
{
	return 0;
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
	if (mntflags & MNT_FORCE) {
		return ENOTSUP;
	}

	rvp = fnip->fi_rootvp;
	rfnp = vntofn(rvp);

	/*
	 * At this point, the VFS layer is holding a write lock on
	 * mp, preventing threads from crossing through that mount
	 * point on a lookup; there might, however, be threads that
	 * have already crossed through that mount point and are
	 * in the process of a lookup in this file system.
	 *
	 * The VFS layer has already done a vflush() that checked
	 * all non-root, non-swap, non-system vnodes and, if any
	 * had a zero usecount or a usecount equal to the kusecount,
	 * waited for the iocount to drop to 0 and then reclaimed
	 * them (and, if it's a forced unmount, for vnodes with a
	 * a non-zero usecount, waited for the iocount to drop to
	 * 0 and then deadfs'ed them), so, except for the root
	 * vnode, there should be no vnodes left.
	 *
	 * Unfortunately, if somebody's holding onto the root
	 * vnode, e.g. in a lookup, they could end up getting
	 * some vnode on this file system, whether it's the
	 * root vnode or some other vnode, and possibly getting
	 * a usecount on it, so the vflush() didn't give us a
	 * particularly useful guarantee.
	 *
	 * Holding the write lock will block any mount atop
	 * a vnode in this file system from crossing into
	 * this file system on a lookup.  A triggered mount
	 * will hold a usecount on the mount point, so, if
	 * that vnode is *not* the root vnode, a vflush()
	 * should fail - but, again, there's no guarantee
	 * that the mount won't be triggered *after* the
	 * vflush() is done.  Furthermore, if it *is* being
	 * done on the root vnode, the vflush() won't help.
	 *
	 * So:
	 *
	 *	nobody should be crossing *into* this file
	 *	system and triggering a mount, and, if they
	 *	do, they'll block (modulo 3424361, alas, but
	 *	that just means they'll fail);
	 *
	 *	there might, however, be lookups that have
	 *	already crossed into this file system and
	 *	that will trigger mounts;
	 *
	 *	the only way to wait for all of them to
	 *	finish would be to release the root vnode
	 *	and then do a vflush() waiting for everything
	 *	to drain - but if that fails we'd have to
	 *	recreate the root vnode, and we'd have to
	 *	make sure that the temporary lack of a
	 *	root vnode doesn't break anything;
	 *
	 *	if we don't wait for anll of them to finish,
	 *	we run the risk of dismantling this mount
	 *	while operations are in progress on it (see
	 *	8960014);
	 *
	 * Lock the fninfo, so that no mounts get started until we
	 * release it.
	 */
	error = autofs_mount_set_busy(fnip, MOUNT_BUSY_EXCLUSIVE);
	if (error) {
		/* looks like we won't be able to grab the lock */
		return error;
	}
	lck_rw_lock_exclusive(fnip->fi_rwlock);

	/*
	 * Wait for in-progress operations to finish on vnodes other than
	 * the root vnode, and fail if any usecounts are held on any of
	 * those vnodes.
	 */
	error = vflush(mp, rvp, 0);
	if (error) {
		lck_rw_unlock_exclusive(fnip->fi_rwlock);
		autofs_mount_clear_busy(fnip, MOUNT_BUSY_EXCLUSIVE);
		return error;
	}

	/*
	 * OK, do we have any usecounts on the root vnode, other than
	 * our own, or does it have any live subdirectories?
	 */
	if (vnode_isinuse(rvp, 1) || rfnp->fn_dirents != NULL) {
		/*
		 * Yes - drop the lock and fail.
		 */
		lck_rw_unlock_exclusive(fnip->fi_rwlock);
		autofs_mount_clear_busy(fnip, MOUNT_BUSY_EXCLUSIVE);
		return EBUSY;
	}

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
				lck_rw_unlock_exclusive(fnip->fi_rwlock);
				autofs_mount_clear_busy(fnip, MOUNT_BUSY_EXCLUSIVE);
				return EBUSY;
			}
			if (pfnp) {
				pfnp->fn_next = fnp->fn_next;
			} else {
				myrootfnnodep->fn_dirents = fnp->fn_next;
			}
			fnp->fn_next = NULL;
			break;
		}
		pfnp = fnp;
		fnp = fnp->fn_next;
	}
	lck_rw_unlock_exclusive(myrootfnnodep->fn_rwlock);

	/*
	 * OK, we're committed to the unmount.  Mark the file system
	 * as being unmounted, so that any triggered mounts done on
	 * it will fail rather than trying to refer to the fninfo_t
	 * for it, as we're going to release that fninfo_t.  Then drop
	 * the write lock on the fninfo_t, so that any triggered mounts
	 * waiting for us to release the lock can proceed, see that
	 * MF_UNMOUNTING is set, and fail.
	 */
	fnip->fi_flags |= MF_UNMOUNTING;
	lck_rw_unlock_exclusive(fnip->fi_rwlock);
	autofs_mount_clear_busy(fnip, MOUNT_BUSY_EXCLUSIVE);

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
	vnode_rele(rvp);        /* release reference from auto_mount() */

	/*
	 * Wait for in-flight operations to complete on any remaining vnodes
	 * (such as the root vnode, which we just released).
	 *
	 * The root vnode will be recycled in vflush() when there are
	 * no longer any in-flight operations.
	 */
	vflush(mp, NULLVP, 0);

	lck_rw_free(fnip->fi_rwlock, autofs_lck_grp);
	lck_mtx_free(fnip->fi_busy_mtx, autofs_lck_grp);
	kfree_data(fnip->fi_path, fnip->fi_pathlen);
	kfree_data(fnip->fi_map, fnip->fi_maplen);
	kfree_data(fnip->fi_subdir, fnip->fi_subdirlen);
	kfree_data(fnip->fi_key, fnip->fi_keylen);
	kfree_data(fnip->fi_opts, fnip->fi_optslen);
	kfree_type(struct fninfo, fnip);

	/*
	 * One fewer mounted file system.
	 */
	lck_mtx_lock(autofs_global_lock);
	autofs_mounts--;
	lck_mtx_unlock(autofs_global_lock);

	AUTOFS_DPRINT((5, "auto_unmount: return=0\n"));

	return 0;
}

/*
 * find root of autofs
 */
int
auto_root(mount_t mp, vnode_t *vpp, __unused vfs_context_t context)
{
	int error;
	fninfo_t *fnip;

	fnip = vfstofni(mp);
	*vpp = fnip->fi_rootvp;
	error = vnode_get(*vpp);

	AUTOFS_DPRINT((5, "auto_root: mp %p, *vpp %p, error %d\n",
	    mp, *vpp, error));
	return error;
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

	return 0;
}

/*
 * autofs doesn't have any data or backing store and you can't write into
 * any of the autofs structures, so don't do anything.
 */
static int
auto_sync(__unused mount_t mp, __unused int waitfor,
    __unused vfs_context_t context)
{
	return 0;
}

/*
 * Look up a autofs node by node number.
 * Currently not supported.
 */
static int
auto_vget(__unused mount_t mp, __unused ino64_t ino, __unused vnode_t *vpp,
    __unused vfs_context_t context)
{
	return ENOTSUP;
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
	if (namelen > 1) {
		return ENOTDIR;         /* overloaded error code */
	}
#ifdef DEBUG
	req = CAST_DOWN(struct sysctl_req *, oldp);
#endif

	AUTOFS_DPRINT((4, "autofs_sysctl called.\n"));
	switch (name[0]) {
#ifdef DEBUG
	case AUTOFS_CTL_DEBUG:
		error = SYSCTL_IN(req, &debug, sizeof(debug));
		if (error) {
			break;
		}
		auto_debug_set(debug);
		break;
#endif
	default:
		error = ENOTSUP;
		break;
	}

	return error;
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
static d_open_t  autofs_dev_open;
static d_close_t autofs_dev_close;
static d_ioctl_t autofs_ioctl;

struct cdevsw autofs_cdevsw = {
	autofs_dev_open,
	autofs_dev_close,
	eno_rdwrt,      /* d_read */
	eno_rdwrt,      /* d_write */
	autofs_ioctl,
	eno_stop,
	eno_reset,
	0,              /* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0               /* d_type */
};

int     automounter_pid;
lck_rw_t *autofs_automounter_pid_rwlock;

static int
autofs_dev_open(__unused dev_t dev, __unused int oflags, __unused int devtype,
    struct proc *p)
{
	lck_rw_lock_exclusive(autofs_automounter_pid_rwlock);
	if (automounter_pid != 0) {
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		return EBUSY;
	}
	automounter_pid = proc_pid(p);
	lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
	return 0;
}

static int
autofs_dev_close(__unused dev_t dev, __unused int flag, __unused int fmt,
    __unused struct proc *p)
{
	lck_rw_lock_exclusive(autofs_automounter_pid_rwlock);
	automounter_pid = 0;
	lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
	return 0;
}

static int
autofs_ioctl(__unused dev_t dev, u_long cmd, __unused caddr_t data,
    __unused int flag, __unused struct proc *p)
{
	struct autofs_globals *fngp;
	fnnode_t *fnp;
	vnode_t vp;
	fninfo_t *fnip;
	struct timeval now;
	struct vnode_attr vattr;
	int error;

	lck_mtx_lock(autofs_global_lock);
	if ((fngp = autofs_zone_get_globals()) == NULL) {
		fngp = autofs_zone_init();
		autofs_zone_set_globals(fngp);
	}
	lck_mtx_unlock(autofs_global_lock);
	if (fngp == NULL) {
		return ENOMEM;
	}

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

			/*
			 * Now update the mod times of the root directories
			 * of all indirect map mounts, and deliver change
			 * notifications for them if their contents can
			 * reflect information from Open Directory (i.e.,
			 * they're not nobrowse), as the information
			 * we're getting from Open Directory might now
			 * be different.
			 */
			microtime(&now);
			lck_rw_lock_shared(fngp->fng_rootfnnodep->fn_rwlock);
			for (fnp = fngp->fng_rootfnnodep->fn_dirents;
			    fnp != NULL; fnp = fnp->fn_next) {
				vp = fntovn(fnp);
				fnip = vfstofni(vnode_mount(vp));
				if (fnip->fi_flags & MF_DIRECT) {
					continue;       /* direct map */
				}
				fnp->fn_mtime = now;
				if (vnode_ismonitored(vp) &&
				    !auto_nobrowse(vp)) {
					vfs_get_notify_attributes(&vattr);
					auto_get_attributes(vp, &vattr);
					vnode_notify(vp, VNODE_EVENT_WRITE,
					    &vattr);
				}
			}
			lck_rw_unlock_shared(fngp->fng_rootfnnodep->fn_rwlock);
		}
		error = 0;
		break;

	case AUTOFS_WAITFORFLUSH:
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

	return error;
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
	return is_automounter;
}

/*
 * Structure representing a process that has registered itself as some
 * sort of tracked process by opening a cloning autofs device.
 */
struct tracked_process {
	LIST_ENTRY(tracked_process) entries;
	int     pid;                    /* PID of the nowait process */
	int     minor;                  /* minor device they opened */
};
LIST_HEAD(tracked_process_list, tracked_process);

static struct tracked_process *
tracked_process_alloc(size_t size, int pid, int minor)
{
	struct tracked_process *tp;

	tp = kalloc_type(struct tracked_process, Z_WAITOK | Z_ZERO);
	if (tp != NULL) {
		tp->pid = pid;
		tp->minor = minor;
	}
	return tp;
}

static void
tracked_process_free(struct tracked_process *tp)
{
	kfree_type(struct tracked_process, tp);
}

static int
tracked_process_insert(struct tracked_process_list *list,
    struct tracked_process *newtp)
{
	struct tracked_process *tp;

	/*
	 * We don't really care about keeping this list
	 * sorted.  We essentially assign minor numbers
	 * "randomly", and only care about ensuring that
	 * we don't insert duplicates.
	 */
	LIST_FOREACH(tp, list, entries) {
		if (newtp->minor == tp->minor) {
			/*
			 * This should never happen; caller
			 * will log error.
			 */
			return EBUSY;
		}
	}

	LIST_INSERT_HEAD(list, newtp, entries);
	return 0;
}

static void
tracked_process_remove(__unused struct tracked_process_list *list,
    struct tracked_process *tp)
{
	LIST_REMOVE(tp, entries);
}

static struct tracked_process *
tracked_process_find_minor(struct tracked_process_list *list,
    int minor)
{
	struct tracked_process *tp;

	LIST_FOREACH(tp, list, entries) {
		if (tp->minor == minor) {
			return tp;
		}
	}
	return NULL;
}

static struct tracked_process *
tracked_process_find_pid(struct tracked_process_list *list,
    pid_t pid)
{
	struct tracked_process *tp;

	LIST_FOREACH(tp, list, entries) {
		if (tp->pid == pid) {
			return tp;
		}
	}
	return NULL;
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
static d_open_t  autofs_nowait_dev_open;
static d_close_t autofs_nowait_dev_close;

struct cdevsw autofs_nowait_cdevsw = {
	autofs_nowait_dev_open,
	autofs_nowait_dev_close,
	eno_rdwrt,      /* d_read */
	eno_rdwrt,      /* d_write */
	eno_ioctl,
	eno_stop,
	eno_reset,
	0,              /* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0               /* d_type */
};

struct tracked_process_list nowait_processes;
lck_rw_t *autofs_nowait_processes_rwlock;
static unsigned int autofs_nowait_nextminor = 0;

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
int
autofs_nowait_dev_clone(__unused dev_t dev, int action)
{
	int rv;

	if (action == DEVFS_CLONE_ALLOC) {
		lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
		rv = autofs_nowait_nextminor++ & 0xffff;
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		return rv;
	}

	if (action == DEVFS_CLONE_FREE) {
		return 0;
	}

	return -1;
}

static int
autofs_nowait_dev_open(dev_t dev, __unused int oflags, __unused int devtype,
    struct proc *p)
{
	struct tracked_process *tp;
	int error;

	tp = tracked_process_alloc(sizeof(*tp), proc_pid(p), minor(dev));
	if (tp == NULL) {
		error = ENOMEM;
	} else {
		lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
		error = tracked_process_insert(&nowait_processes, tp);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
	}

	if (error) {
		printf("autofs_nowait_dev_open: *** error %d for minor %d\n",
		    error, minor(dev));
		if (tp != NULL) {
			tracked_process_free(tp);
		}
	}
	return error;
}

static int
autofs_nowait_dev_close(dev_t dev, __unused int flag, __unused int fmt,
    __unused struct proc *p)
{
	struct tracked_process *tp;

	lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
	tp = tracked_process_find_minor(&nowait_processes, minor(dev));
	if (tp != NULL) {
		tracked_process_remove(&nowait_processes, tp);
	}
	lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
	if (tp != NULL) {
		tracked_process_free(tp);
	}
	return 0;
}

/*
 * Check whether this process is a nowait process.
 */
int
auto_is_nowait_process(int pid)
{
	struct tracked_process *tp;
	int rv = 0;

	lck_rw_lock_shared(autofs_nowait_processes_rwlock);
	tp = tracked_process_find_pid(&nowait_processes, pid);
	if (tp != NULL) {
		rv = 1;
	}
	lck_rw_unlock_shared(autofs_nowait_processes_rwlock);
	return rv;
}

/*
 * Opening /dev/autofs_notrigger makes you (but not your children) a
 * notrigger process; those do not trigger mounts.  This is used
 * by srvsvc.
 *
 * Closing /dev/autofs_notrigger makes you no longer a notrigger process;
 * it's closed on exit, so if you exit, you cease to be a notrigger process.
 */
static d_open_t  autofs_notrigger_dev_open;
static d_close_t autofs_notrigger_dev_close;

struct cdevsw autofs_notrigger_cdevsw = {
	autofs_notrigger_dev_open,
	autofs_notrigger_dev_close,
	eno_rdwrt,      /* d_read */
	eno_rdwrt,      /* d_write */
	eno_ioctl,
	eno_stop,
	eno_reset,
	0,              /* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0               /* d_type */
};

struct tracked_process_list notrigger_processes;
lck_rw_t *autofs_notrigger_processes_rwlock;
static unsigned int autofs_notrigger_nextminor = 0;

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
int
autofs_notrigger_dev_clone(__unused dev_t dev, int action)
{
	int rv;

	if (action == DEVFS_CLONE_ALLOC) {
		lck_rw_lock_exclusive(autofs_notrigger_processes_rwlock);
		rv = autofs_notrigger_nextminor++ & 0xffff;
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
		return rv;
	}

	if (action == DEVFS_CLONE_FREE) {
		return 0;
	}

	return -1;
}

static int
autofs_notrigger_dev_open(dev_t dev, __unused int oflags, __unused int devtype,
    struct proc *p)
{
	struct tracked_process *tp;
	int error;

	tp = tracked_process_alloc(sizeof(*tp), proc_pid(p), minor(dev));
	if (tp == NULL) {
		error = ENOMEM;
	} else {
		lck_rw_lock_exclusive(autofs_notrigger_processes_rwlock);
		error = tracked_process_insert(&notrigger_processes, tp);
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
	}

	if (error) {
		printf("autofs_notrigger_dev_open: *** error %d for minor %d\n",
		    error, minor(dev));
		if (tp != NULL) {
			tracked_process_free(tp);
		}
	}
	return error;
}

static int
autofs_notrigger_dev_close(dev_t dev, __unused int flag, __unused int fmt,
    __unused struct proc *p)
{
	struct tracked_process *tp;

	lck_rw_lock_exclusive(autofs_notrigger_processes_rwlock);
	tp = tracked_process_find_minor(&notrigger_processes, minor(dev));
	if (tp != NULL) {
		tracked_process_remove(&notrigger_processes, tp);
	}
	lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
	if (tp != NULL) {
		tracked_process_free(tp);
	}
	return 0;
}

/*
 * Check whether this process is a notrigger process.
 */
int
auto_is_notrigger_process(int pid)
{
	struct tracked_process *tp;
	int rv = 0;

	/*
	 * automountd, and anything it runs, is a notrigger process.
	 */
	if (auto_is_automounter(pid)) {
		return 1;
	}

	lck_rw_lock_shared(autofs_notrigger_processes_rwlock);
	tp = tracked_process_find_pid(&notrigger_processes, pid);
	if (tp != NULL) {
		rv = 1;
	}
	lck_rw_unlock_shared(autofs_notrigger_processes_rwlock);
	return rv;
}

/*
 * Opening /dev/autofs_homedirmounter makes you (but not your children) a
 * homedirmounter process; those processes can perform an fcntl() to arrange
 * that a given autofs vnode not trigger a mount.  This is used by code
 * that remounts home directories, so that nobody does any automounts
 * while they're in the process of doing a remount.
 *
 * Closing /dev/autofs_homedirmounter makes you no longer a homedirmounter
 * process, which means that whatever vnode you set not to trigger a mount
 * reverts to triggering mounts; it's closed on exit, so if you exit, you
 * cease to be a homedirmounter process.
 */
static d_open_t  autofs_homedirmounter_dev_open;
static d_close_t autofs_homedirmounter_dev_close;

struct cdevsw autofs_homedirmounter_cdevsw = {
	autofs_homedirmounter_dev_open,
	autofs_homedirmounter_dev_close,
	eno_rdwrt,      /* d_read */
	eno_rdwrt,      /* d_write */
	eno_ioctl,
	eno_stop,
	eno_reset,
	0,              /* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0               /* d_type */
};

/*
 * Structure representing a process that has registered itself as an
 * homedirmounter process by opening a cloning autofs device.
 */
struct homedirmounter_process {
	struct tracked_process tracker;
	vnode_t mount_point;    /* autofs vnode on which they're doing a mount, if any */
};

static void
homedirmounter_process_free(struct homedirmounter_process *hp)
{
	vnode_t vp;
	fnnode_t *fnp;

	if ((vp = hp->mount_point) != NULL) {
		if ((fnp = vntofn(vp)) != NULL) {
			/*
			 * <13595777> Keep from racing with
			 * homedirmounter
			 */
			if (fnp->fn_flags & MF_HOMEDIRMOUNT_LOCKED) {
				lck_mtx_unlock(fnp->fn_mnt_lock);
			}

			lck_mtx_lock(fnp->fn_lock);
			fnp->fn_flags &= ~(MF_HOMEDIRMOUNT |
			    MF_HOMEDIRMOUNT_LOCKED);
			lck_mtx_unlock(fnp->fn_lock);
		}
		vnode_rele(vp);
	}
	tracked_process_free((struct tracked_process *)hp);
}

struct tracked_process_list homedirmounter_processes;
lck_rw_t *autofs_homedirmounter_processes_rwlock;
static unsigned int autofs_homedirmounter_nextminor = 0;

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
int
autofs_homedirmounter_dev_clone(__unused dev_t dev, int action)
{
	int rv;

	if (action == DEVFS_CLONE_ALLOC) {
		lck_rw_lock_exclusive(autofs_homedirmounter_processes_rwlock);
		rv = autofs_homedirmounter_nextminor++ & 0xffff;
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
		return rv;
	}

	if (action == DEVFS_CLONE_FREE) {
		return 0;
	}

	return -1;
}

static int
autofs_homedirmounter_dev_open(dev_t dev, __unused int oflags, __unused int devtype,
    struct proc *p)
{
	struct homedirmounter_process *hp;
	int error;

	hp = (struct homedirmounter_process *)
	    tracked_process_alloc(sizeof(*hp), proc_pid(p), minor(dev));
	if (hp == NULL) {
		error = ENOMEM;
	} else {
		hp->mount_point = NULL;

		lck_rw_lock_exclusive(autofs_homedirmounter_processes_rwlock);
		error = tracked_process_insert(&homedirmounter_processes, &hp->tracker);
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
	}

	if (error) {
		printf("autofs_homedirmounter_dev_open: *** error %d for minor %d\n",
		    error, minor(dev));
		if (hp != NULL) {
			homedirmounter_process_free(hp);
		}
	}
	return error;
}

static int
autofs_homedirmounter_dev_close(dev_t dev, __unused int flag, __unused int fmt,
    __unused struct proc *p)
{
	struct homedirmounter_process *hp;

	lck_rw_lock_exclusive(autofs_homedirmounter_processes_rwlock);
	hp = (struct homedirmounter_process *)
	    tracked_process_find_minor(&homedirmounter_processes, minor(dev));
	if (hp != NULL) {
		tracked_process_remove(&homedirmounter_processes, &hp->tracker);
	}
	lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
	if (hp != NULL) {
		homedirmounter_process_free(hp);
	}
	return 0;
}

/*
 * Check whether this process is a homedirmounter process and, if we are,
 * and we were passed a vnode_t, also check whether we're the homedirmounter
 * for that vnode.
 */
int
auto_is_homedirmounter_process(vnode_t vp, int pid)
{
	struct homedirmounter_process *hp;
	int rv = 0;

	lck_rw_lock_shared(autofs_homedirmounter_processes_rwlock);
	hp = (struct homedirmounter_process *)
	    tracked_process_find_pid(&homedirmounter_processes, pid);
	if (hp != NULL) {
		rv = (vp == NULL) || (vp == hp->mount_point);
	}
	lck_rw_unlock_shared(autofs_homedirmounter_processes_rwlock);
	return rv;
}

/*
 * If this is a home directory mounter process:
 *
 *	if we haven't already marked a vnode as having a home directory
 *	mount in progress, mark the specified vnode and remember it, and
 *	return 0 (unless we couldn't grab a reference to it, in which
 *	case we return the error);
 *
 *	if we've already marked this vnode as having a home directory
 *	mount in progress, return 0;
 *
 *	if we have already marked some other vnode as having a home
 *	directory mount in progress, return EBUSY.
 *
 * Otherwise, return EINVAL.
 */
int
auto_mark_vnode_homedirmount(vnode_t vp, int pid, int need_lock)
{
	struct homedirmounter_process *hp;
	fnnode_t *fnp = NULL;
	int error;

	lck_rw_lock_shared(autofs_homedirmounter_processes_rwlock);
	hp = (struct homedirmounter_process *)
	    tracked_process_find_pid(&homedirmounter_processes, pid);
	if (hp != NULL) {
		if (hp->mount_point != NULL) {
			/*
			 * We're already a home directory mounter
			 * for some mount point.  Is this that
			 * mount point?
			 */
			if (hp->mount_point == vp) {
				/*
				 * Yes - not an error.  (That means
				 * we don't have to avoid doing the
				 * "make me the home directory
				 * mounter" fsctl if we already
				 * became the home directory mounter
				 * as a result of doing an unmount.)
				 */
				error = 0;
			} else {
				/*
				 * No - that's an error, as we can
				 * only be the home directory mounter
				 * for one vnode at a time.
				 */
				error = EBUSY;
			}
		} else {
			/*
			 * We're not a home directory mounter for
			 * a mount point.  Make us the home
			 * directory mounter for this vnode.
			 * Attempt to grab a reference to it,
			 * as we'll be storing a pointer to it.
			 */
			error = vnode_ref(vp);
			if (error == 0) {
				/*
				 * We succeeded.
				 */
				fnp = vntofn(vp);
				lck_mtx_lock(fnp->fn_lock);
				fnp->fn_flags |= MF_HOMEDIRMOUNT;
				lck_mtx_unlock(fnp->fn_lock);
				hp->mount_point = vp;
			}
		}
		lck_rw_unlock_shared(autofs_homedirmounter_processes_rwlock);
		return error;
	}
	lck_rw_unlock_shared(autofs_homedirmounter_processes_rwlock);

	if ((fnp != NULL) && (need_lock)) {
		/*
		 * <13595777> homedirmounter is getting ready to do a
		 * mount. To keep from racing with an autofs mount already
		 * in progress, take the fn_mnt_lock. This lock will be freed
		 * in autofs_homedirmounter_dev_close(). Its expected that
		 * homedirmounter will open the magic autofs dev, do the magic
		 * fsctl, then close the magic autofs dev.
		 */
		lck_mtx_lock(fnp->fn_mnt_lock);

		lck_mtx_lock(fnp->fn_lock);
		fnp->fn_flags |= MF_HOMEDIRMOUNT_LOCKED;
		lck_mtx_unlock(fnp->fn_lock);
	}

	return EINVAL;
}

/*
 * Opening /dev/autofs_control when nobody else has it open lets you perform
 * various ioctls to control autofs.
 *
 * Opening /dev/autofs when somebody else has it open fails with EBUSY.
 * This is used to ensure that only one instance of the automount command
 * is running at a time.
 */
static d_open_t  auto_control_dev_open;
static d_close_t auto_control_dev_close;
static d_ioctl_t auto_control_ioctl;

struct cdevsw autofs_control_cdevsw = {
	auto_control_dev_open,
	auto_control_dev_close,
	eno_rdwrt,      /* d_read */
	eno_rdwrt,      /* d_write */
	auto_control_ioctl,
	eno_stop,
	eno_reset,
	0,              /* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0               /* d_type */
};

int     autofs_control_isopen;
lck_mtx_t *autofs_control_isopen_lock;

static int
auto_control_dev_open(__unused dev_t dev, __unused int oflags,
    __unused int devtype, __unused struct proc *p)
{
	lck_mtx_lock(autofs_control_isopen_lock);
	if (autofs_control_isopen) {
		lck_mtx_unlock(autofs_control_isopen_lock);
		return EBUSY;
	}
	autofs_control_isopen = 1;
	lck_mtx_unlock(autofs_control_isopen_lock);
	return 0;
}

static int
auto_control_dev_close(__unused dev_t dev, __unused int flag,
    __unused int fmt, __unused struct proc *p)
{
	lck_mtx_lock(autofs_control_isopen_lock);
	autofs_control_isopen = 0;
	lck_mtx_unlock(autofs_control_isopen_lock);
	return 0;
}

static int
auto_control_ioctl(__unused dev_t dev, u_long cmd, caddr_t data,
    __unused int flag, __unused proc_t p)
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
	if (fngp == NULL) {
		return ENOMEM;
	}

	switch (cmd) {
	case AUTOFS_SET_MOUNT_TO:
		trigger_set_mount_to(*(int *)data);
		error = 0;
		break;

	case AUTOFS_UPDATE_OPTIONS_32:
		update_argsp_32 = (struct autofs_update_args_32 *)data;
		update_args.fsid = update_argsp_32->fsid;
		update_args.opts = CAST_USER_ADDR_T(update_argsp_32->opts);
		update_args.map = CAST_USER_ADDR_T(update_argsp_32->map);
		update_args.mntflags = update_argsp_32->mntflags;
		update_args.direct = update_argsp_32->direct;
		update_args.node_type = update_argsp_32->node_type;
		error = auto_update_options(&update_args);
		break;

	case AUTOFS_UPDATE_OPTIONS_64:
		error = auto_update_options((struct autofs_update_args_64 *)data);
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
		/*
		 * Mark this as being unmounted, so that we return ENOENT
		 * for any lookups under it (for an indirect map), and
		 * then try to unmount it.
		 *
		 * We fail lookups under it so that nobody creates
		 * triggers under us while we're being unmounted,
		 * as that can cause the root vnode of the indirect
		 * map to have links to it while it's being removed
		 * from the list of autofs mounts, causing 6491044.
		 * and to prevent the deadlock described below.
		 *
		 * Given that this trigger isn't supposed to be
		 * there in the first place, lookups under it
		 * should fail in any case.
		 *
		 * XXX - does that still apply?
		 */
		error = 0;
		if (fngp != NULL) {
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
			 * Mark this as being unmounted.
			 */
			lck_rw_lock_exclusive(fnip->fi_rwlock);
			fnip->fi_flags |= MF_UNMOUNTING;
			lck_rw_unlock_exclusive(fnip->fi_rwlock);

			/*
			 * Unmount the file system with the specified
			 * fsid; that will provoke an unmount of
			 * all triggered mounts below it.
			 */
			error = vfs_unmountbyfsid((fsid_t *)data, MNT_NOBLOCK,
			    vfs_context_current());

			/*
			 * If that failed, we're no longer in the middle
			 * of unmounting it.  (If it succeeded, it no
			 * longer exists, so we can't unmark it as being
			 * in the middle of being unmounted.)
			 */
			if (error != 0) {
				lck_rw_lock_exclusive(fnip->fi_rwlock);
				fnip->fi_flags &= ~MF_UNMOUNTING;
				lck_rw_unlock_exclusive(fnip->fi_rwlock);
			}
		}
		break;

	case AUTOFS_UNMOUNT_TRIGGERED:
		unmount_triggered_mounts(1);
		error = 0;
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}
