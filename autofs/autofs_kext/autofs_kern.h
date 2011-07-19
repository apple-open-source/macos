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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2011 Apple Inc.
 */

#ifndef __AUTOFS_KERN_H__
#define __AUTOFS_KERN_H__

#ifdef __APPLE_API_PRIVATE
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/attr.h>
#include <kern/locks.h>

/*
 * Global lock information, defined in auto_vfsops.c.
 */
extern lck_grp_t *autofs_lck_grp;
extern lck_mtx_t *autofs_nodeid_lock;

/* XXX Get rid of this as soon as sys/malloc.h can be updated to define a real M_AUTOFS */
#define M_AUTOFS M_TEMP

/* XXX Get rid of this as soon as sys/vnode.h can be updated to define a real VT_AUTOFS */
#define VT_AUTOFS (VT_OTHER+1)

/*
 * Tracing macro; expands to nothing for non-debug kernels.
 * Also, define MACH_ASSERT in debug kernels, so assert() does something.
 */
#ifndef DEBUG
#define AUTOFS_DPRINT(x)
#else /* DEBUG */
#define AUTOFS_DPRINT(x)	auto_dprint x
#ifndef MACH_ASSERT
#define MACH_ASSERT
#endif /* MACH_ASSERT */
#endif /* DEBUG */

extern int (**autofs_vnodeop_p)(void *);

struct action_list;

/*
 * Per AUTOFS mountpoint information.
 */
typedef struct fninfo {
	lck_rw_t	*fi_rwlock;
	vnode_t		fi_rootvp;		/* root vnode */
	char		*fi_path;		/* autofs mountpoint */
	char 		*fi_map;		/* context/map-name */
	char		*fi_subdir;		/* subdir within map */
	char		*fi_key;		/* key to use on direct maps */
	char		*fi_opts;		/* default mount options */
	uint32_t	fi_mntflags;		/* Boolean mount options */
	int		fi_pathlen;		/* autofs mountpoint len */
	int		fi_maplen;		/* size of context */
	int		fi_subdirlen;
	int		fi_keylen;
	int		fi_optslen;		/* default mount options len */
	int		fi_flags;
} fninfo_t;

/*
 * fi_flags bits:
 *
 *	MF_DIRECT:
 *		- Direct mountpoint if set, indirect otherwise.
 *
 *	MF_UNMOUNTING
 *		- We're in the process of unmounting this, so we should
 *		  return ENOENT for lookups in the root directory (see
 *		  auto_control_ioctl() for a reason why this is done)
 *
 *	MF_SUBTRIGGER
 *		- This is a subtrigger mount.
 */
#define	MF_DIRECT	0x001
#define	MF_UNMOUNTING	0x002
#define	MF_SUBTRIGGER	0x004

/*
 * We handle both directories and symlinks in autofs.
 * This is the vnode-type-specific information.
 */

/*
 * Directory-specific information - list of entries and number of entries.
 */
struct autofs_dir_node {
	struct fnnode *d_dirents;
	int	d_direntcnt;		/* count of entries in d_dirents list */
};
#define fn_dirents	fn_u.d.d_dirents
#define fn_direntcnt	fn_u.d.d_direntcnt

/*
 * Symlink-specific information - contents of symlink.
 */
struct autofs_symlink_node {
	int s_length;
	char *s_symlinktarget;		/* Dynamically allocated */
};

#define fn_symlink	fn_u.s.s_symlinktarget
#define fn_symlinklen	fn_u.s.s_length

/*
 * The AUTOFS locking scheme:
 *
 * The locks:
 * 	fn_lock: protects the fn_node. It must be grabbed to change any
 *		 field on the fn_node, except for those protected by
 *		 fn_rwlock.
 *
 * 	fn_rwlock: readers/writers lock to protect the subdirectory and
 *		   top level list traversal.
 *		   Protects: fn_dirents
 *			     fn_direntcnt
 *			     fn_next
 *		             fn_linkcnt
 *                 - Grab readers when checking if certain fn_node exists
 *                   under fn_dirents.
 *		   - Grab readers when attempting to reference a node
 *                   pointed to by fn_dirents, fn_next, and fn_parent.
 *                 - Grab writers to add a new fnnode under fn_dirents and
 *		     to remove a node pointed to by fn_dirents or fn_next.
 *
 *
 * The flags:
 * 	MF_HOMEDIRMOUNT:
 * 		- A home directory mount is in progress on this node.
 */

/*
 * The inode of AUTOFS
 */
typedef struct fnnode {
	char		*fn_name;
	int		fn_namelen;
	nlink_t		fn_linkcnt;		/* link count */
	mode_t		fn_mode;		/* file mode bits */
	uid_t		fn_uid;			/* owner's uid */
	int		fn_error;		/* mount/lookup error */
	ino_t		fn_nodeid;
	off_t		fn_offset;		/* offset into directory */
	int		fn_flags;
	trigger_info_t	*fn_trigger_info;	/* if this is a trigger, here's the trigger info */
	union {
		struct autofs_dir_node d;
		struct autofs_symlink_node s;
	}		fn_u;
	vnode_t		fn_vnode;
	uint32_t	fn_vid;			/* vid of the vnode */
	struct fnnode	*fn_parent;
	struct fnnode	*fn_next;		/* sibling */
	lck_rw_t	*fn_rwlock;		/* protects list traversal */
	lck_mtx_t	*fn_lock;		/* protects the fnnode */
	struct timeval	fn_crtime;
	struct timeval	fn_atime;
	struct timeval	fn_mtime;
	struct timeval	fn_ctime;
	struct autofs_globals *fn_globals;	/* global variables */
} fnnode_t;

#define vntofn(vp)	((struct fnnode *)(vnode_fsnode(vp)))
#define	fntovn(fnp)	(((fnp)->fn_vnode))
#define	vfstofni(mp)	((fninfo_t *)(vfs_fsprivate(mp)))

#define	MF_HOMEDIRMOUNT	0x001		/* Home directory mount in progress */

#define	AUTOFS_MODE		0555
#define	AUTOFS_BLOCKSIZE	1024

struct autofs_globals {
	fnnode_t		*fng_rootfnnodep;
	int			fng_fnnode_count;
	int			fng_printed_not_running_msg;
	int			fng_verbose;
	lck_mtx_t		*fng_flush_notification_lock;
	int			fng_flush_notification_pending;
};

extern struct vnodeops *auto_vnodeops;

/*
 * We can't call VFS_ROOT(), so, instead, we directly call our VFS_ROOT()
 * routine.
 */
extern int auto_root(mount_t, vnode_t *, vfs_context_t);

/*
 * Utility routines
 */
extern fnnode_t *auto_search(fnnode_t *, char *, int);
extern int auto_enter(fnnode_t *, struct componentname *, fnnode_t **);
extern int auto_wait4unmount_tree(fnnode_t *, vfs_context_t);
extern int auto_makefnnode(fnnode_t **, int, mount_t,
	struct componentname *, const char *, vnode_t, int,
	struct autofs_globals *);
extern void auto_freefnnode(fnnode_t *);
extern void auto_disconnect(fnnode_t *, fnnode_t *);
/*PRINTFLIKE2*/
extern void auto_dprint(int level, const char *fmt, ...)
	__printflike(2, 3);
extern void auto_debug_set(int level);
extern int auto_lookup_aux(struct fninfo *, fnnode_t *, char *, int,
    vfs_context_t, int *);
extern int auto_readdir_aux(struct fninfo *, fnnode_t *, off_t, u_int,
    int64_t *, boolean_t *, byte_buffer *, mach_msg_type_number_t *);
extern int auto_is_automounter(int);
extern int auto_is_nowait_process(int);
extern int auto_is_notrigger_process(int);
extern int auto_is_homedirmounter_process(vnode_t, int);
extern int auto_mark_vnode_homedirmount(vnode_t, int);
extern int auto_is_autofs(mount_t);
extern int auto_nobrowse(vnode_t);
extern void auto_get_attributes(vnode_t, struct vnode_attr *);
extern int auto_lookup_request(fninfo_t *, char *, int, char *,
    vfs_context_t, int *, boolean_t *);

extern void autofs_free_globals(struct autofs_globals *);
	
extern void auto_fninfo_lock_shared(fninfo_t *fnip, int pid);
extern void auto_fninfo_unlock_shared(fninfo_t *fnip, int pid);

#endif /* __APPLE_API_PRIVATE */

#endif	/* __AUTOFS_KERN_H__ */
