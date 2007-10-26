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
 * Portions Copyright 2007 Apple Inc.  All rights reserved.
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
#define DEBUG
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
	int		fi_mount_to;
	int		fi_mach_to;
} fninfo_t;

/*
 * fi_flags bits:
 *
 *	MF_DIRECT:
 *		- Direct mountpoint if set, indirect otherwise.
 *
 *	MF_DONTTRIGGER
 *		- We should not trigger mounts in VFS_ROOT() if set
 *		  (see auto_start() for a reason why this is done).
 */
#define	MF_DIRECT	0x001
#define	MF_DONTTRIGGER	0x002

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

/* xattr values for network.category */
#define XATTR_CATEGORY_MAXSIZE	sizeof(uint32_t)

#ifndef XATTR_CATEGORYNAME
#define XATTR_CATEGORYNAME		"network.category"
#define XATTR_CATEGORYNAMELEN	17
#endif /* XATTR_CATEGORYNAME */

struct autofs_xattr_data {
	/* category value for /Network.  If zero, xattr is not set */
	uint32_t xattr_category;
};

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
 *	MF_INPROG:
 *		- Indicates a mount request has been sent to the daemon.
 *		- If this flag is set, the thread sets MF_WAITING on the
 *                fnnode and sleeps.
 *
 *	MF_WAITING:
 *		- Set by a thread when it puts itself to sleep waiting for
 *		  the ongoing operation on this fnnode to be done.
 *
 * 	MF_LOOKUP:
 * 		- Indicates a lookup request has been sent to the daemon.
 *		- If this flag is set, the thread sets MF_WAITING on the
 *                fnnode and sleeps.
 *
 *	MF_TRIGGER:
 *		- This is a trigger node.
 *
 *	MF_THISUID_MATCH_RQD:
 *		- User-relative context binding kind of node.
 *		- Node with this flag set requires a name match as well
 *		  as a cred match in order to be returned from the directory
 *		  hierarchy.
 *
 * 	MF_MOUNTPOINT:
 * 		- At some point automountd mounted a filesystem on this node.
 * 		If fn_trigger is non-NULL, v_vfsmountedhere is NULL and this
 * 		flag is set then the filesystem must have been forcibly
 * 		unmounted.
 */

/*
 * What we know about whether an autofs directory is a trigger.
 */
typedef enum {
	FN_TRIGGER_UNKNOWN,	/* we don't know whether it's a trigger */
	FN_IS_TRIGGER,		/* yes, it's a trigger */
	FN_ISNT_TRIGGER		/* no, it's not a trigger */
} fn_istrigger_t;

/*
 * The inode of AUTOFS
 */
typedef struct fnnode {
	char		*fn_name;
	int		fn_namelen;
	nlink_t		fn_linkcnt;		/* link count */
	mode_t		fn_mode;		/* file mode bits */
	uid_t		fn_uid;			/* owner's uid */
	gid_t		fn_gid;			/* group's uid */
	int		fn_error;		/* mount/lookup error */
	ino_t		fn_nodeid;
	off_t		fn_offset;		/* offset into directory */
	int		fn_flags;
	fn_istrigger_t	fn_istrigger;		/* is this a trigger? */
	struct autofs_xattr_data xattr_data;
	union {
		struct autofs_dir_node d;
		struct autofs_symlink_node s;
	}		fn_u;
	vnode_t		fn_vnode;
	struct fnnode	*fn_parent;
	struct fnnode	*fn_next;		/* sibling */
	struct fnnode	*fn_trigger; 		/* pointer to next level */
						/* AUTOFS trigger nodes */
	struct action_list *fn_alp;		/* Pointer to mount info */
						/* used for remounting */
						/* trigger nodes */
	ucred_t		fn_cred;		/* pointer to cred, used for */
						/* "thisuser" processing */
	lck_rw_t	*fn_rwlock;		/* protects list traversal */
	lck_mtx_t	*fn_lock;		/* protects the fnnode */
	struct timeval	fn_crtime;
	struct timeval	fn_atime;
	struct timeval	fn_mtime;
	struct timeval	fn_ctime;
	time_t		fn_ref_time;		/* time last referenced */
	time_t		fn_unmount_ref_time;	/* last time unmount was done */
	fsid_t		fn_fsid_mounted;	/* fsid of filesystem mounted */
	vnode_t		fn_seen;		/* vnode already traversed */
	thread_t	fn_thread;		/* thread that has currently */
						/* modified fn_seen */
	struct autofs_globals *fn_globals;	/* global variables */
	struct klist	fn_knotes;		/* knotes attached to this vnode */
} fnnode_t;

#define vntofn(vp)	((struct fnnode *)(vnode_fsnode(vp)))
#define	fntovn(fnp)	(((fnp)->fn_vnode))
#define	vfstofni(mp)	((fninfo_t *)(vfs_fsprivate(mp)))

#define AUTO_KNOTE(vp, hint)	KNOTE(&vntofn(vp)->fn_knotes, (hint))

#define	MF_INPROG	0x002		/* Mount in progress */
#define	MF_WAITING	0x004
#define	MF_LOOKUP	0x008		/* Lookup in progress */
#define	MF_TRIGGER	0x080
#define	MF_THISUID_MATCH_RQD	0x100	/* UID match required for this node */
					/* required for thisuser kind of */
					/* nodes */
#define	MF_MOUNTPOINT	0x200		/* Node is/was a mount point */

#define	AUTOFS_MODE		0555
#define	AUTOFS_BLOCKSIZE	1024

struct autofs_callargs {
	fnnode_t	*fnc_fnp;	/* fnnode */
	char		*fnc_name;	/* path to lookup/mount */
	int		fnc_namelen;	/* length of path */
	thread_t	fnc_origin;	/* thread that fired up this thread */
					/* used for debugging purposes */
	kauth_cred_t	fnc_cred;
	mach_port_t	fnc_gssd_port;	/* mach port for gssd */
};

struct autofs_globals {
	fnnode_t		*fng_rootfnnodep;
	int			fng_fnnode_count;
	int			fng_printed_not_running_msg;
	lck_mtx_t		*fng_unmount_threads_lock;
	int			fng_unmount_threads;
	int			fng_terminate_do_unmount_thread;
	int			fng_do_unmount_thread_terminated;
	int			fng_verbose;
	lck_mtx_t		*fng_flush_notification_lock;
	int			fng_flush_notification_pending;
};

/*
 * Sets the MF_INPROG flag on this fnnode.
 * fnp->fn_lock should be held before this macro is called,
 * operation is either MF_INPROG or MF_LOOKUP.
 */
#define	AUTOFS_BLOCK_OTHERS(fnp, operation)	{ \
	lck_mtx_assert((fnp)->fn_lock, LCK_MTX_ASSERT_OWNED); \
	assert(!((fnp)->fn_flags & operation)); \
	(fnp)->fn_flags |= (operation); \
}

#define	AUTOFS_UNBLOCK_OTHERS(fnp, operation)	{ \
	auto_unblock_others((fnp), (operation)); \
}

extern struct vnodeops *auto_vnodeops;

/*
 * Utility routines
 */
extern int auto_search(fnnode_t *, char *, int, fnnode_t **, ucred_t);
extern int auto_enter(fnnode_t *, struct componentname *, const char *,
	fnnode_t **, const char *, ucred_t);
extern void auto_unblock_others(fnnode_t *, u_int);
extern int auto_wait4mount(fnnode_t *, vfs_context_t);
extern int auto_makefnnode(fnnode_t **, enum vtype, mount_t,
	struct componentname *, const char *, vnode_t, int, ucred_t,
	struct autofs_globals *);
extern void auto_freefnnode(fnnode_t *);
extern void auto_disconnect(fnnode_t *, fnnode_t *);
extern void auto_do_unmount(void *);
/*PRINTFLIKE2*/
extern void auto_dprint(int level, const char *fmt, ...)
	__printflike(2, 3);
extern void auto_debug_set(int level);
extern int auto_lookup_aux(fnnode_t *, char *, int, vfs_context_t);
extern void auto_new_mount_thread(fnnode_t *, char *, int, vfs_context_t);
extern int auto_check_trigger_request(fninfo_t *, char *, int, boolean_t *);
extern int auto_get_automountd_port(mach_port_t *);
extern void auto_release_automountd_port(mach_port_t);
extern int auto_dont_trigger(vnode_t, vfs_context_t);
extern kern_return_t auto_new_thread(void (*)(void *), void *);
extern int auto_is_automounter(pid_t);
extern int auto_is_nowait_process(pid_t);
extern int auto_is_autofs(mount_t);

/*
 * Flags for unmount_tree.
 *
 * If UNMOUNT_TREE_IMMEDIATE is set, unmount items even if they haven't
 * timed out yet.
 *
 * If UNMOUNT_TREE_NONOTIFY is set, do unmounts of non-autofs file
 * systems in the kernel, rather in automountd.
 */
#define UNMOUNT_TREE_IMMEDIATE		0x00000001
#define UNMOUNT_TREE_NONOTIFY		0x00000002

extern void unmount_tree(struct autofs_globals *, fsid_t *, int);
extern void autofs_free_globals(struct autofs_globals *);
	
extern void auto_fninfo_lock_shared(fninfo_t *fnip, vfs_context_t ctx);
extern void auto_fninfo_unlock_shared(fninfo_t *fnip, vfs_context_t ctx);

#endif /* __APPLE_API_PRIVATE */

#endif	/* __AUTOFS_KERN_H__ */
