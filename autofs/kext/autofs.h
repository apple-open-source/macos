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
/* Copyright (c) 1998, Apple Computer, Inc. All rights reserved. */
/*
 * Created 27-Jun-2003 by Alfred Perlstein
 * Derived from synthfs by Pat Dirks
 *
 * $Id: autofs.h,v 1.23 2005/03/12 03:18:54 lindak Exp $
 */

#ifndef __AUTOFS_H__
#define __AUTOFS_H__

#include <stdint.h>

#include <sys/dirent.h>
#ifndef DT_AUTO
#define	DT_AUTO 15
#endif
#include <sys/stat.h>
#ifndef SF_AUTOMOUNT
#define	SF_AUTOMOUNT  0x80000000
#endif
#ifndef SF_UID
#define SF_UID 0x40000000
#endif
#ifndef SF_AUTH
#define SF_AUTH 0x20000000
#endif
#ifndef SF_MOUNTED
#define SF_MOUNTED 0x10000000
#endif

typedef struct autofs_mnt_args_hdr {
	uint32_t mnt_args_size;
} autofs_mnt_args_hdr;

typedef struct autofs_mnt_args {
	autofs_mnt_args_hdr hdr;			/* Fixed part of mount arguments */
	char devicename[MAXPATHLEN+1];		/* Variable part follows */
} autofs_mnt_args;

/* arg is struct autofs_userreq */
#define AUTOFS_CTL_GETREQS	0x0001	/* Get mount requests. */
#define AUTOFS_CTL_SERVREQ	0x0002	/* Serve mount requests. */

/* arg is struct autofs_mounterreq */
#define AUTOFS_CTL_MOUNTER	0x0003	/* Set as a mounter. */
#define AUTOFS_CTL_TRIGGER	0x0004	/* Set up a trigger. */

#ifdef __APPLE_API_PRIVATE
/* arg is int */
#define AUTOFS_CTL_DEBUG	0x0005	/* toggle debug. */
#endif /* __APPLE_API_PRIVATE */

#define AUTOFS_PROTOVERS	0x02

/*
 * Structure passed back and forth from userland to
 * describe/fullfill a pending mount request.
 */
struct autofs_userreq {
	char au_name[PATH_MAX];	/* name, useful for debug. */
	ino_t au_dino;	/* inode of directory. */
	ino_t au_ino;	/* inode of entry being looked up. */
	pid_t au_pid;	/* pid of (initial) requester. */
	uid_t au_uid;	/* uid of requester. */
	gid_t au_gid;	/* gid of requester. */
	int au_flags;	/* flags (none for now) */
	int au_errno;	/* errno passed back. */
	int au_pad[16];
};

/*
 * Structure passed with AUTOFS_CTL_MOUNTER, AUTOFS_CTL_TRIGGER
 * to register as the one responsible for a mount.
 */
struct autofs_mounterreq {
	ino_t amu_ino;		/* which inode we're mounting. */
	pid_t amu_pid;		/* which pid, (to register someone else.) */
	uid_t amu_uid;		/* which uid (for MOUNTER settings) */
	int amu_flags;		/* flags, see below. */
	int amu_pad[15];
};

/* flags for autofs_mounterreq.amu_flags */
#define AUTOFS_MOUNTERREQ_UID	0x0001	/* seperate vnodes per uid. */
#define AUTOFS_MOUNTERREQ_DEFER 0x0002	/* content generation deferred */

#ifdef KERNEL
#ifdef __APPLE_API_PRIVATE
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/attr.h>
#include <kern/locks.h>

extern lck_grp_t *autofs_lck_grp;	/* Defined in autofs_util.c */

/* XXX Get rid of this as soon as sys/malloc.h can be updated to define a real M_AUTOFS */
#define M_AUTOFS M_TEMP

/* XXX Get rid of this as soon as sys/vnode.h can be updated to define a real VT_AUTOFS */
#define VT_AUTOFS (VT_OTHER+1)

struct autofs_req {
	LIST_ENTRY(autofs_req)	ar_list;	/* hung from mount point. */
	char *ar_name;			/* name waiting for. */
	size_t ar_namelen;		/* strlen(ar_name). */
	vnode_t ar_dp;			/* directory parent of request */
	vnode_t ar_vp;			/* entry requested. */
	pid_t ar_pid;			/* pid of (initial) requester. */
	uid_t ar_uid;			/* uid of requester. */
	gid_t ar_gid;			/* gid of requester. */
	int ar_flags;			/* flags. (nothing right now). */
	int ar_refcnt;			/* refcount. */
	int ar_errno;			/* errno to return. */
	int ar_onlst;			/* on list (should dequeue ar_list). */
};

struct autofsnode;

struct autofs_mntdata {
	mount_t autofs_mp;
	vnode_t autofs_rootvp;
	dev_t autofs_mounteddev;
	unsigned long autofs_nextid;
	unsigned long autofs_filecount;
	unsigned long autofs_dircount;
	unsigned long autofs_encodingsused;
	LIST_HEAD(autofs_fsvnodelist, vnode) autofs_fsvnodes;
	LIST_HEAD(, autofs_req) autofs_reqs;
	TAILQ_HEAD(, autofsnode) autofs_nodes;
	int autofs_nodecnt;
};

/*
 * Various sorts of autofs vnodes:
 */
enum autofsnodetype {
    AUTOFS_DIRECTORY = 1,
    AUTOFS_SYMLINK
};

struct autofs_dir_node {
	unsigned long d_entrycount;
	TAILQ_HEAD(autofs_d_subnodelist, autofsnode) d_subnodes;
};

struct autofs_file_node {
	off_t f_size;
};

struct autofs_symlink_node {
	int s_length;
	char *s_symlinktarget;		/* Dynamically allocated */
};


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

struct autofsnode {
	/* autofsnodes in a given directory */
	TAILQ_ENTRY(autofsnode) s_sibling;
	/* list of autofs nodes for a mount. */
	TAILQ_ENTRY(autofsnode) s_mntlst;
	/* list of autofs nodes cloned for a given mount. */
	TAILQ_ENTRY(autofsnode) s_clonelst;
	TAILQ_HEAD(, autofsnode) s_clonehd;
	enum autofsnodetype s_type;
	struct autofsnode *s_parent;	/* parent dir: use PARENTNODE macro for correct results. */
	/*
	 * When IN_CLONE is set, this is a pointer to the node it
	 * was cloned from.
	 */
	struct autofsnode *s_clonedfrom;
	vnode_t s_vp;
	char *s_name;
	lck_rw_t *s_lock;
	/* Internal flags: IN_CHANGED, IN_MODIFIED, etc. */
	unsigned long s_nodeflags;
	unsigned long s_nodeid;
	unsigned long s_generation;
	pid_t s_mounterpid;	/* pid of registered mounter. */
	mode_t s_mode;
	int s_linkcount;
	uid_t s_cloneuid;	/* uid cloned for. */
	uid_t s_uid;
	gid_t s_gid;
	dev_t s_rdev;
	struct timeval s_createtime;
	struct timeval s_accesstime;
	struct timeval s_modificationtime;
	struct timeval s_changetime;
	struct timeval s_backuptime;
	unsigned long s_flags;	/* inode flags: IMMUTABLE, APPEND, etc. */
	unsigned long s_script;
	struct autofs_xattr_data xattr_data;
	union {
		struct autofs_dir_node d;
		struct autofs_file_node f;
		struct autofs_symlink_node s;
	} s_u;
#ifdef VREF_DEBUG
	int s_relline;
	const char *s_relfile;
#endif /* VREF_DEBUG */
};

#define PARENTNODE(sp) ((sp)->s_clonedfrom ? (sp)->s_clonedfrom->s_parent : (sp)->s_parent)

#ifdef VREF_DEBUG
#define VNODE_GET(vp)							\
	do {								\
		if (VTOA(vp) != NULL) {					\
			VTOA(vp)->s_relline = __LINE__;			\
			VTOA(vp)->s_relfile = __FILE__;			\
		}							\
		vnode_get(vp);						\
	} while (0)

#define VNODE_PUT(vp)							\
	do {								\
		if (VTOA(vp) != NULL) {					\
			VTOA(vp)->s_relline = __LINE__;			\
			VTOA(vp)->s_relfile = __FILE__;			\
		}							\
		vnode_put(vp);						\
	} while (0)

#define VNODE_REF(vp)							\
	do {								\
		if (VTOA(vp) != NULL) {					\
			VTOA(vp)->s_relline = __LINE__;			\
			VTOA(vp)->s_relfile = __FILE__;			\
		}							\
		vnode_ref(vp);						\
	} while (0)
	
#define VNODE_RELE(vp)							\
	do {								\
		if (VTOA(vp) != NULL) {					\
			VTOA(vp)->s_relline = __LINE__;			\
			VTOA(vp)->s_relfile = __FILE__;			\
		}							\
		vnode_rele(vp);						\
	} while (0)
#else
#define VNODE_GET(vp) vnode_get(vp)
#define VNODE_PUT(vp) vnode_put(vp)
#define VNODE_REF(vp) vnode_ref(vp)
#define VNODE_RELE(vp) vnode_rele(vp)
#endif /* VREF_DEBUG */


#define ROOT_DIRID	2
#define FIRST_AUTOFS_ID 0x10

/* These flags are kept in s_nodeflags. */
#define IN_ACCESS	0x0001	/* Access time update request. */
#define IN_CHANGE	0x0002	/* Change time update request. */
#define IN_UPDATE	0x0004	/* Modification time update request. */
#define IN_MODIFIED	0x0008	/* Node has been modified. */
#define IN_RENAME	0x0010	/* Node is being renamed. */
#define IN_TRIGGER	0x0100	/* Node is a trigger. */
#define IN_MOUNT	0x0200	/* Mount pending on node. */
#define IN_UID		0x0400	/* Per uid mount. */
#define IN_CLONE	0x0800	/* is a clone. (special .. semantics) */
#define IN_BYPASS	0x1000  /* Node is trigger that couldn't be mounted */
#define IN_DEFERRED 0x2000	/* Node's content's generation has been deferred */

#if 0
#define IN_SHLOCK               0x0020          /* File has shared lock. */
#define IN_EXLOCK               0x0040          /* File has exclusive lock. */
#define IN_ALLOCATING			0x1000          /* vnode is in transit, wait or ignore */
#define IN_WANT                 0x2000          /* Its being waited for */
#endif

#define AUTOFSTIMES(sp, t1, t2) do {					\
	if ((sp)->s_nodeflags & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) {	\
		(sp)->s_nodeflags |= IN_MODIFIED;			\
		if ((sp)->s_nodeflags & IN_ACCESS) {			\
			(sp)->s_accesstime = *(t1);			\
		}							\
		if ((sp)->s_nodeflags & IN_UPDATE) {			\
			(sp)->s_modificationtime = *(t2);		\
		}							\
		if ((sp)->s_nodeflags & IN_CHANGE) {			\
			microtime(&(sp)->s_changetime);				\
		}							\
		(sp)->s_nodeflags &= ~(IN_ACCESS|IN_CHANGE|IN_UPDATE);	\
	}								\
} while (0)

#define ATTR_REF_DATA(attrrefptr)	\
	(((char *)(attrrefptr)) + ((attrrefptr)->attr_dataoffset))

#define ATOV(SP) ((SP)->s_vp)

#define VTOA(VP) ((struct autofsnode *)(vnode_fsnode(VP)))

#define VTOVFS(VP) (vnode_mount(VP))
#define	ATOVFS(AP) (vnode_mount((AP)->s_vp))
#define SFATOVFS(SFSMP) ((SFSMP)->sfs_mp)

#define VTOAFS(VP) ((struct autofs_mntdata *)(vfs_fsprivate(vnode_mount(VP))))
#define	ATOAFS(AP) ((struct autofs_mntdata *)(vfs_fsprivate(vnode_mount((AP)->s_vp))))
#define	VFSTOAFS(MP) ((struct autofs_mntdata *)(vfs_fsprivate(MP)))

#define DBG_VOP(P)							\
	do {								\
		if (autofs_debug)					\
			printf P;					\
	} while (0)

#define DBG_ASSERT(a)							\
	do {								\
		if (!(a)) { 						\
			panic("File %s line %d: assertion '" #a		\
			    "' failed.", __FILE__, __LINE__);		\
		}							\
	} while (0)

extern int (**autofs_vnodeop_p)(void *);
extern uint32_t autofs_debug;

__BEGIN_DECLS
int autofs_mount(struct mount *mp, vnode_t devvp, user_addr_t data, vfs_context_t context);
int autofs_vfsstart(struct mount *mp, int flags, vfs_context_t context);
int autofs_unmount(struct mount *mp, int mntflags, vfs_context_t context);
int autofs_root(struct mount *mp, struct vnode **vpp, vfs_context_t context);
int autofs_quotactl(struct mount *mp, int cmds, uid_t uid, caddr_t arg, enum uio_seg segflg, vfs_context_t context);
int autofs_vfs_getattr(struct mount *mp, struct vfs_attr *vfap, vfs_context_t context);
int autofs_sync(struct mount *mp, int waitfor, vfs_context_t context);
int autofs_vfs_vget(struct mount *mp, void *ino, struct vnode **vpp, vfs_context_t context);
int autofs_init(struct vfsconf *vfsp);
int	autofs_sysctl(int *name, u_int namelen, user_addr_t oldp, size_t *oldlenp, user_addr_t newp, size_t newlen, vfs_context_t context);
int autofs_vget(mount_t mp, ino64_t ino, vnode_t *vpp, vfs_context_t context);

int	autofs_create(struct vnop_create_args *);
int	autofs_open(struct vnop_open_args *);
int	autofs_mmap(struct vnop_mmap_args *);
int	autofs_getattr(struct vnop_getattr_args *);
int	autofs_setattr(struct vnop_setattr_args *);
int	autofs_rename(struct vnop_rename_args *);
int	autofs_select(struct vnop_select_args *);
int	autofs_remove(struct vnop_remove_args *);
int	autofs_mkdir(struct vnop_mkdir_args *);
int	autofs_rmdir(struct vnop_rmdir_args *);
int	autofs_symlink(struct vnop_symlink_args *);
int	autofs_readlink(struct vnop_readlink_args *);
int	autofs_readdir(struct vnop_readdir_args *);
int	autofs_cached_lookup(struct vnop_lookup_args *);
int	autofs_lookup(struct vnop_lookup_args *);
int	autofs_pathconf(struct vnop_pathconf_args *);
int	autofs_print(struct autofsnode *);
int autofs_getxattr(struct vnop_getxattr_args *);
int autofs_setxattr(struct vnop_setxattr_args *);
int autofs_removexattr(struct vnop_removexattr_args *);
int autofs_listxattr(struct vnop_listxattr_args *);

int	autofs_inactive(struct vnop_inactive_args*);
int	autofs_reclaim(struct vnop_reclaim_args*);

int	autofs_getnewvnode(mount_t mp, struct autofsnode *sp, enum vtype vtype, unsigned long nodeid, vnode_t *vpp);
int autofs_clonenode(mount_t mp, struct autofsnode *an, struct autofsnode **anp, uid_t uid, vfs_context_t context);
int	autofs_to_vnode(mount_t mp, struct autofsnode *an, vnode_t *vpp);
int	autofs_new_directory(mount_t mp, vnode_t dp, const char *name,
    unsigned long nodeid, mode_t mode, vnode_t *vpp, vfs_context_t context);
int	autofs_new_symlink(mount_t mp, vnode_t dp, const char *name,
    unsigned long nodeid, char *targetstring, vnode_t *vpp, vfs_context_t context);
long autofs_adddirentry(u_int32_t fileno, u_int8_t type, const char *name,
    struct autofsnode *ap, struct uio *uio);
int	autofs_remove_entry(struct autofsnode *sp);
int	autofs_request(vnode_t dp, vnode_t vp, const char *nameptr, size_t namelen, vfs_context_t context);
int autofs_update(vnode_t vp, struct timeval *access, struct timeval *modify, int waitfor);

int	autofs_move_rename_entry(vnode_t source_vp,
    vnode_t newparent_vp, char *newname);
int	autofs_derive_vnode_path(vnode_t vp, char *vnpath,
    size_t pathbuffersize);

void	autofs_destroynode(struct autofsnode *an);

#endif /* __APPLE_API_PRIVATE */
#endif /* KERNEL */
#endif /* __AUTOFS_H__ */
