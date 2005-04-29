/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1998 Apple Computer, Inc. All Rights Reserved */
/*
 * Change History:
 *
 *	17-Aug-1999	Pat Dirks	New today.
 *	09-Dec-2003	Alfred Perlstein	Autofs.
 *
 * $Id: autofs_vfsops.c,v 1.31 2005/03/12 03:18:54 lindak Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <mach/machine/vm_types.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/attr.h>
#include <sys/sysctl.h>
#include <kern/locks.h>

#include <miscfs/specfs/specdev.h>

#include "autofs.h"

#define LOADABLE_FS 0
#define MAXIMIZE_CACHEABILITY 0

typedef int (*PFI)();


struct vfsops autofs_vfsops = {
	autofs_mount,
	autofs_vfsstart,
	autofs_unmount,
	autofs_root,
	NULL,	/* autofs_quotactl */
	autofs_vfs_getattr, 	/* was autofs_statfs */
	autofs_sync,
	autofs_vget,
	NULL, /* autofs_fhtovp */
	NULL, /* autofs_vptofh */
	autofs_init,
	autofs_sysctl
};

int autofs_active;

#define ROOTMPMODE 0755
#define ROOTPLACEHOLDERMODE 0700
#define AUTOFS_FS_NAME "autofs"
static char autofs_fake_mntfromname[] = "<autofs>";

static vfstable_t loaded_fs_handle;

extern struct vnodeopv_desc autofsfs_vnodeop_opv_desc;
struct vnodeopv_desc * autofs_vnodeop_opv_descs[] =
	{&autofsfs_vnodeop_opv_desc, NULL};

static int autofs_reqequal(struct autofs_req *, struct autofs_req *);

struct vfs_fsentry autofs_fsentry = {
	&autofs_vfsops,				/* vfs operations */
	1,							/* # of vnodeopv_desc being registered (reg, spec, fifo ...) */
	autofs_vnodeop_opv_descs,	/* null terminated;  */
	0,							/* historic filesystem type number [ unused w. VFS_TBLNOTYPENUM specified ] */
	AUTOFS_FS_NAME,				/* filesystem type name */
	VFS_TBLNOTYPENUM | VFS_TBLTHREADSAFE | VFS_TBL64BITREADY,		/* defines the FS capabilities */
    { NULL, NULL }				/* reserved for future use; set this to zero*/
 };

/*
 * VFS Operations.
 *
 * mount system call
 */
int
autofs_mount_fs(vnode_t devvp, struct mount *mp, user_addr_t args, vfs_context_t context)
{
	struct autofs_mntdata *priv_mnt_data;
	autofs_mnt_args_hdr mnt_args_hdr;
	autofs_mnt_args *mnt_args;
	int error;

	DBG_VOP(("autofs_mount_fs called.\n"));
	MALLOC(priv_mnt_data, struct autofs_mntdata *, sizeof(struct autofs_mntdata), M_AUTOFS, M_WAITOK);
	DBG_VOP(("MALLOC succeeded...\n"));

	strncpy(vfs_statfs(mp)->f_fstypename, AUTOFS_FS_NAME, sizeof(vfs_statfs(mp)->f_fstypename));
	if (args == USER_ADDR_NULL) {
		strncpy(vfs_statfs(mp)->f_mntfromname, autofs_fake_mntfromname, sizeof(vfs_statfs(mp)->f_mntfromname)-1);
	} else {
		if ((error = copyin(args, &mnt_args_hdr, sizeof(autofs_mnt_args_hdr)))) goto Error_exit;
		MALLOC(mnt_args, autofs_mnt_args *, mnt_args_hdr.mnt_args_size, M_AUTOFS, M_WAITOK);
		if (mnt_args == NULL) {
			error = ENOMEM;
			goto Error_exit;
		};
		if ((error = copyin(args, mnt_args, mnt_args_hdr.mnt_args_size))) goto Error_exit;
		strncpy(vfs_statfs(mp)->f_mntfromname, mnt_args->devicename, sizeof(vfs_statfs(mp)->f_mntfromname)-1);
	};
	vfs_statfs(mp)->f_mntfromname[sizeof(vfs_statfs(mp)->f_mntfromname)-1] = (char)0;
	
	priv_mnt_data->autofs_mounteddev = (dev_t)0;
	priv_mnt_data->autofs_nextid = FIRST_AUTOFS_ID;
	priv_mnt_data->autofs_filecount = 0;
	priv_mnt_data->autofs_dircount = 0;
	priv_mnt_data->autofs_encodingsused = 0x00000001;
	priv_mnt_data->autofs_nodecnt = 0;
	TAILQ_INIT(&priv_mnt_data->autofs_nodes);
	LIST_INIT(&priv_mnt_data->autofs_reqs);

	/*
	 * Set up the root vnode for fast reference in the future.
	 * Note that autofs_new_directory() returns the vnode with a
	 * refcount of +2.
	 * The root vnode's refcount is maintained unlocked but with a
	 * pos. ref count until unmount.
	 */
	priv_mnt_data->autofs_mp = mp;
	vfs_setfsprivate(mp, (void *)priv_mnt_data);

	error = autofs_new_directory(mp, NULL, "", ROOT_DIRID, (S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH), &priv_mnt_data->autofs_rootvp, context);
	if (error) {
		DBG_VOP(("Attempt to create root directory failed with "
			"error %d.\n", error));
		return (error);
	}

	/*
	 * Drop the freshly acquired reference on the root,
	 * increment v_usecount to 1 to prevent the vnode from beeing freed:
	 */
	vnode_ref(priv_mnt_data->autofs_rootvp);
	vnode_put(priv_mnt_data->autofs_rootvp);
	lck_rw_unlock_exclusive(VTOA(priv_mnt_data->autofs_rootvp)->s_lock);
	
	vfs_getnewfsid(mp);
	autofs_active++;
	error = 0;
	goto Std_exit;

Error_exit:

Std_exit:
	return(error);
}

int
autofs_mount(struct mount *mp, vnode_t devvp, user_addr_t data, vfs_context_t context)
{
	return autofs_mount_fs(devvp, mp, data, context);
}

/*
 * Initialize the filesystem
 */
int
autofs_init(struct vfsconf *vfsp)
{
	DBG_VOP(("autofs_init called.\n"));
	return (0);
}

int
autofs_vfsstart(struct mount *mp, int flags, vfs_context_t context)
{
	DBG_VOP(("autofs_start called.\n"));
	return (0);
}

/*
 * Return the root of a filesystem.
 */
int
autofs_root(struct mount *mp, struct vnode **vpp, vfs_context_t context)
{
	DBG_VOP(("autofs_root called.\n"));

	*vpp = VFSTOAFS(mp)->autofs_rootvp;
	return vnode_get(VFSTOAFS(mp)->autofs_rootvp);
}

/*
 * unmount system call
 */
int
autofs_unmount(struct mount *mp, int mntflags, vfs_context_t context)
{
	struct autofs_mntdata *amp;
	vnode_t root_vp;
	int error, total_nodes;
	struct autofsnode *sp, *sp_next;

	DBG_VOP(("autofs_unmount called.\n"));
	amp = VFSTOAFS(mp);

	root_vp = amp->autofs_rootvp;
	error = vflush(mp, root_vp, (mntflags & MNT_FORCE) ? FORCECLOSE : 0);
	if (error && ((mntflags & MNT_FORCE) == 0)) goto Err_Exit;

	/*
	 * Free the root vnode:
	 * the ref. count has been maintained at +1 ever since mount time.
	 */
	if (root_vp) {
		if ((mntflags & MNT_FORCE) == 0) {
			if (error != 0) goto Err_Exit;
		}

		amp->autofs_rootvp = NULL;

		if (error == 0) {
			VNODE_RELE(root_vp);	/* Drop usecount to zero */
			vnode_recycle(root_vp);
		}
	}

	/* Walk the list of nodes removing them, we'll have to make
	 * several passes because I did not want to use recursion
	 * because of limited kernel stack space.
	 * We should make at most N passes where N is the max depth
	 * of the tree.
	 */
another_pass:
	total_nodes = amp->autofs_nodecnt;
	DBG_VOP(("autofs_unmount: total_nodes = %d\n", total_nodes));
	sp = TAILQ_FIRST(&amp->autofs_nodes);
	while (sp != NULL) {
		sp_next = TAILQ_NEXT(sp, s_mntlst);
		error = autofs_remove_entry(sp);
		/* this might be a directory with children. */
		if (error != 0) {
			sp = sp_next;
			continue;
		}
		TAILQ_REMOVE(&amp->autofs_nodes, sp, s_mntlst);
		amp->autofs_nodecnt--;
		if (amp->autofs_nodecnt < 0) printf("autofs_unmount: amp->autofs_nodecnt < 0\n");
		if (sp->s_linkcount != 0) printf("autofs_unmount: linkcount != 0 (%d)\n", sp->s_linkcount);
		DBG_VOP(("autofs_unmount: killing '%s'\n", sp->s_name));
		FREE(sp->s_name, M_TEMP);
		sp->s_name = NULL;
		FREE(sp, M_AUTOFS);
		sp = sp_next;
	}
	/*
	 * Because some directories may not have been empty we may have
	 * to loop again, however if we haven't made any progress in
	 * deleting nodes then something has gone wrong.
	 */
	if (amp->autofs_nodecnt != 0) {
		/* we should have made some progress... */
		if (total_nodes == amp->autofs_nodecnt) panic("autofs_unmount: stuck.");
		goto another_pass;
	}

	/* All vnodes should be gone, and no errors, clean up the last */
	FREE(amp, M_AUTOFS);

Err_Exit:

	if (mntflags & MNT_FORCE)
		error = 0;

	DBG_VOP(("autofs_unmount: %s code = %d\n",
		error == 0 ? "succeeded" : "failed",
		error));
	if (error == 0) autofs_active--;
	return (error);
}

/*
 * Get file system statistics.
 */
int
autofs_vfs_getattr(struct mount *mp, struct vfs_attr *vfap, vfs_context_t context)
{
	DBG_VOP(("autofs_statfs called.\n"));

	VFSATTR_RETURN(vfap, f_bsize, 512);
	VFSATTR_RETURN(vfap, f_iosize, 512);
	VFSATTR_RETURN(vfap, f_blocks, 1024);
	VFSATTR_RETURN(vfap, f_bfree, 0);
	VFSATTR_RETURN(vfap, f_bavail, 0);
	VFSATTR_RETURN(vfap, f_files, (uint64_t)((unsigned long)VFSTOAFS(mp)->autofs_filecount + (unsigned long)VFSTOAFS(mp)->autofs_dircount));
	VFSATTR_RETURN(vfap, f_ffree, 0);
	
	return (0);
}

/*
 * autofs doesn't have any data or backing store and you can't write into
 * any of the autofs structures, so don't do anything.
 */
int
autofs_sync(struct mount *mp, int waitfor, vfs_context_t context)
{
	return (0);
}

/*
 * Look up a autofs node by node number.
 */
int
autofs_vget(mount_t mp, ino64_t ino, vnode_t *vpp, vfs_context_t context)
{
	struct autofsnode *an;
	struct autofs_mntdata *amp = VFSTOAFS(mp);
	int error;

loop:
	TAILQ_FOREACH(an, &amp->autofs_nodes, s_mntlst) {
		/* do not return cloned nodes */
		if (an->s_clonedfrom != NULL) continue;
		if (an->s_nodeid == ((unsigned long )ino)) {
			error = autofs_to_vnode(mp, an, vpp);
			if (error) goto loop;
			return (0);
		}
	}
	return (ENOENT);
}

int
autofs_vget_uid(mount_t mp, ino64_t ino, vnode_t *vpp, uid_t uid, vfs_context_t context)
{
	int error;
	struct autofsnode *an = NULL;
	vnode_t vputme;

	error = autofs_vget(mp, ino, vpp, context);
	if (error) return (error);
	
	an = VTOA(*vpp);
	if (an->s_nodeflags & IN_UID) {
		/* Locate the per-uid clone of this vnode: */
		vputme = *vpp;
loop:
		TAILQ_FOREACH(an, &VTOA(vputme)->s_clonehd, s_clonelst) {
			if (an->s_cloneuid == uid) {
				error = autofs_to_vnode(mp, an, vpp);
				if (error) goto loop;
				break;
			}
		}
		if (an == NULL) {
			error = autofs_clonenode(mp, VTOA(vputme), &an, uid, context);
			if (error) return error;
		};
		if (an) {
			VNODE_PUT(vputme);
			*vpp = ATOV(an);
		}
	};
	return ((an == NULL) ? ENOENT : 0);
}

void
autofs_kernel2user(struct autofs_req *req, struct autofs_userreq *ureq)
{
	ureq->au_dino = VTOA(req->ar_dp)->s_nodeid;
	ureq->au_ino = VTOA(req->ar_vp)->s_nodeid;
	ureq->au_pid = req->ar_pid;
	ureq->au_uid = req->ar_uid;
	ureq->au_gid = req->ar_gid;
	strncpy(ureq->au_name, req->ar_name, req->ar_namelen + 1);
}

int
autofs_sysctl(int *name, u_int namelen, user_addr_t oldp, size_t *oldlenp, user_addr_t newp, size_t newlen, vfs_context_t context)
{
	int cnt, error, i, requested, vers;
	uint32_t debug;
	struct sysctl_req *req = NULL;
	mount_t mp;
	struct autofs_mntdata *amp = NULL;
	struct autofsnode *an;
	struct vfsquery vq;
	struct autofs_req *r, *rnext, stkreq;
	struct autofs_userreq *userreqlst;
	struct autofs_userreq *userreq;
	vnode_t vp, vputme;
	struct autofs_mounterreq *mounterreq;
	boolean_t is_64_bit;
	struct vfsidctl vc;
	struct user_vfsidctl user_vc;
	
	/*
	 * All names at this level are terminal
	 */
	if (namelen > 1)
		return ENOTDIR;		/* overloaded error code */

	vputme = NULL;
	error = 0;
	userreq = NULL;
	mounterreq = NULL;

	is_64_bit = vfs_context_is64bit(context);
	req = CAST_DOWN(struct sysctl_req *, oldp);

	DBG_VOP(("autofs_sysctl called.\n"));
	switch (name[0]) {
	case VFS_CTL_QUERY:
	case AUTOFS_CTL_DEBUG:
	case AUTOFS_CTL_GETREQS:
	case AUTOFS_CTL_SERVREQ:
	case AUTOFS_CTL_MOUNTER:
	case AUTOFS_CTL_TRIGGER:
		if (is_64_bit) {
			error = SYSCTL_IN(req, &user_vc, sizeof(user_vc));
			if (error)
				 return (error);
			mp = vfs_getvfs(&user_vc.vc_fsid);
		} 
		else {
			error = SYSCTL_IN(req, &vc, sizeof(vc));
			if (error)
				return (error);
			mp = vfs_getvfs(&vc.vc_fsid);
		}
		if (mp == NULL)
			return (ENOENT);
		amp = VFSTOAFS(mp);
		if (amp == NULL)
			return (ENOENT);
		req->newidx = 0;
		if (is_64_bit) {
			req->newptr = user_vc.vc_ptr;
			req->newlen = (size_t)user_vc.vc_len;
		}
		else {
			req->newptr = CAST_USER_ADDR_T(vc.vc_ptr);
			req->newlen = vc.vc_len;
		}
		break;
	default:
		return (ENOTSUP);
		break;
	}

	switch (name[0]) {
	case VFS_CTL_QUERY:
		bzero(&vq, sizeof(vq));
		if (!LIST_EMPTY(&amp->autofs_reqs)) {
			vq.vq_flags |= VQ_ASSIST;
			error = SYSCTL_OUT(req, &vq, sizeof(vq));
		}
		break;
	/* fetch outstanding mount requests. */
	case AUTOFS_CTL_GETREQS:
		error = SYSCTL_IN(req, &vers, sizeof(vers));
		if (error) break;
		
		if (vers != AUTOFS_PROTOVERS) {
			error = EINVAL;	/* XXX: better errno? */
			break;
		}
		cnt = 0;
		LIST_FOREACH(r, &amp->autofs_reqs, ar_list)
		    cnt++;
		/* Is this request only for the count? */
		if (req->oldptr == USER_ADDR_NULL) {
			req->oldidx = cnt * sizeof(*userreqlst);
			break;
		}
		if (cnt == 0) {
			DBG_VOP(("autofs_sysctl cnt == 0.\n"));
			req->oldlen = 0;
			break;
		}
		/* How many entries were requested? */
		requested = req->oldlen / sizeof(*userreqlst);
		/* Are there more than we want?  Then clip. */
		DBG_VOP(("autofs_sysctl cnt == %d, req = %d, oldlen = %d.\n",
			cnt, requested, req->oldlen));
		if (cnt > requested) cnt = requested;
		if (cnt <= 0) break;
		/*
		 * Allocate a staging buffer because we need a
		 * user representation and SYSCTL_OUT may block us.
		 */
		MALLOC(userreqlst, struct autofs_userreq *, sizeof(*userreqlst) * cnt, M_AUTOFS, M_WAITOK);
		/*
		 * Walk the list of requests copying them into the
		 * user buffer.
		 */
		r = LIST_FIRST(&amp->autofs_reqs);
		for (i = 0; i < cnt && r != NULL; i++) {
			autofs_kernel2user(r, &userreqlst[i]);
			r = LIST_NEXT(r, ar_list);
		}
		error = SYSCTL_OUT(req, userreqlst, sizeof(*userreqlst) * i);
		DBG_VOP(("autofs_sysctl i == %d, oldlen = %d.\n",
			i, req->oldlen));
		FREE(userreqlst, M_AUTOFS);
		break;
	case AUTOFS_CTL_SERVREQ:
		MALLOC(userreq, struct autofs_userreq *, sizeof(*userreq), M_AUTOFS, M_WAITOK);
		error = SYSCTL_IN(req, userreq, sizeof(*userreq));
		if (error) break;
		
		/* force nul termination */
		userreq->au_name[sizeof(userreq->au_name) - 1] = '\0';
		error = autofs_vget_uid(mp, userreq->au_ino, &vp, userreq->au_uid, context);
		if (error)
			break;
		vputme = vp;
		an = VTOA(vp);
		stkreq.ar_vp = vp;
		stkreq.ar_uid = userreq->au_uid;
		DBG_VOP(("autofs_sysctl AUTOFS_CTL_SERVREQ...\n"));
		r = LIST_FIRST(&amp->autofs_reqs);
		while (r != NULL) {
			rnext = LIST_NEXT(r, ar_list);
			if (autofs_reqequal(r, &stkreq)) {
				DBG_VOP(("autofs_sysctl AUTOFS_CTL_SERVREQ "
					"match, flags = 0x%lx, errno = %d\n",
					(long)userreq->au_flags,
					userreq->au_errno));
				r->ar_flags = userreq->au_flags;
				r->ar_errno = userreq->au_errno;
				r->ar_onlst = 0;
				LIST_REMOVE(r, ar_list);
				wakeup(r);
#if 0   /* Don't mess with the mounter state: */
				an->s_nodeflags &= ~IN_MOUNT;
				an->s_mounterpid = -1;
#endif
			}
			r = rnext;
		}
		if (r == NULL) {
			DBG_VOP(("autofs_sysctl AUTOFS_CTL_SERVREQ: No matching request?!"));
		};
		DBG_VOP(("autofs_sysctl AUTOFS_CTL_SERVREQ done...\n"));
		break;
	case AUTOFS_CTL_MOUNTER:
	case AUTOFS_CTL_TRIGGER:
		MALLOC(mounterreq, struct autofs_mounterreq *, sizeof(*mounterreq), M_AUTOFS, M_WAITOK);
		error = SYSCTL_IN(req, mounterreq, sizeof(*mounterreq));
		if (error) break;

		error = autofs_vget_uid(mp, mounterreq->amu_ino, &vp, mounterreq->amu_uid, context);
		if (error) break;
		
		vputme = vp;
		an = VTOA(vp);
		if (name[0] == AUTOFS_CTL_MOUNTER) {
			an->s_mounterpid = mounterreq->amu_pid;
		}
		/*
		 * set/clear the UID bit.
		 *
		 * Note that we can do this can be done regardless
		 * of weather we are or are not a trigger.
		 * (very useful for unit testing.)
		 *
		 * XXX Once the UID bit is set, there is no way to clear it.
		 * The sysctl that attempts to clear it ends up getting a
		 * cloned vnode from autofs_vget_uid, above.  That means that
		 * the clone queue will be non-empty, causing the TAILQ_EMPTY
		 * below to return false (and the sysctl returns ENOTEMPTY).
		 */
		if ((mounterreq->amu_flags & AUTOFS_MOUNTERREQ_UID)) {
			if (an->s_type != AUTOFS_DIRECTORY) {
				error = ENOTDIR;
				break;
			}
			an->s_nodeflags |= IN_UID;
		} else if ((mounterreq->amu_flags & AUTOFS_MOUNTERREQ_DEFER)) {
			if (an->s_type != AUTOFS_DIRECTORY) {
				error = ENOTDIR;
				break;
			}
			an->s_nodeflags |= IN_DEFERRED;
#if MAXIMIZE_CACHEABILITY
			vp->v_flags &= ~VNCACHEABLE;
			cache_purge(vp);
#endif
		} else {
			if (!TAILQ_EMPTY(&an->s_clonehd)) {
				error = ENOTEMPTY;
				break;
			}
		}
		
		/*
		 * Setting the mounter PID to -1 is a way to disable a trigger.
		 *
		 * Deferred content directories should not be marked as a
		 * trigger, or else we'll try to do an automount on the
		 * second access (the first access generates the content).
		 */
		if (an->s_mounterpid == -1 || (an->s_nodeflags & IN_DEFERRED)) {
			an->s_nodeflags &= ~IN_TRIGGER;
		 } else {
			an->s_nodeflags |= IN_TRIGGER;
#if MAXIMIZE_CACHEABILITY
			vp->v_flags &= ~VNCACHEABLE;
			cache_purge(vp);
#endif
		 }
		
#if MAXIMIZE_CACHEABILITY
		/* If the node's now neither a trigger nor a deferred now,
		   it's OK to allow caching in the name cache (again);
		   setting either bit in the future will turn VNCACHEABLE off again */
		if ((an->s_nodeflags & (IN_TRIGGER | IN_DEFERRED)) == 0) {
			vp->v_flags |= VNCACHEABLE;
		};
#endif
		DBG_VOP(("AUTOFS_CTL_MOUNTER pid %d, trigger '%s'\n",
			(int)an->s_mounterpid,
			(an->s_nodeflags & IN_TRIGGER) ? "on" : "off"));
		break;
	case AUTOFS_CTL_DEBUG:
		error = SYSCTL_IN(req, &debug, sizeof(debug));
		if (error)
			break;
		autofs_debug = debug;
		break;
	default:
		error = ENOTSUP;
		break;
	}
	
	if (userreq != NULL) FREE(userreq, M_AUTOFS);
	if (mounterreq != NULL) FREE(mounterreq, M_AUTOFS);
	if (vputme != NULL) VNODE_PUT(vputme);
	
	return (error);
}

__private_extern__
kern_return_t
autofs_start(struct kmod_info *ki, void *data)
{
	errno_t error;

	autofs_lck_grp = lck_grp_alloc_init("autofs", NULL);
	
	error = vfs_fsadd(&autofs_fsentry, &loaded_fs_handle);

	return (error ? KERN_FAILURE : KERN_SUCCESS);
}

__private_extern__
kern_return_t
autofs_stop(struct kmod_info *ki, void *data)
{
	int error;
	
	if (autofs_active > 0) {
		DBG_VOP(("unload_autofs: NOT removing autofs from vfs conf. list; autofs_active = %d\n", autofs_active));
		return (EBUSY);
	}
	DBG_VOP(("unload_autofs: removing autofs from vfs conf. list...\n"));
	error = vfs_fsremove(loaded_fs_handle);
	if (error) {
		printf("autofs_stop: Error %d from vfs_remove", error);
		return KERN_FAILURE;
	}
	lck_grp_free(autofs_lck_grp);
	
	return (KERN_SUCCESS);
}

static int
autofs_reqequal(r1, r2)
	struct autofs_req *r1, *r2;
{
	/* same parent dir? */	
	if (r1->ar_vp != r2->ar_vp) {
		DBG_VOP(("dp1 %p dp2 %p\n", r1->ar_vp, r2->ar_vp));
		return (0);
	}
	if (r1->ar_uid != r2->ar_uid) {
		DBG_VOP(("uid1 %d, uid2 %d\n", r1->ar_uid, r2->ar_uid));
		return (0);
	}
	DBG_VOP(("EQUAL!\n"));
	return (1);
}

/* sysctl export would be nice. */
static int autofs_request_timeo = 0;

int
autofs_request(vnode_t dp, vnode_t vp, const char *nameptr, size_t namelen, vfs_context_t context)
{
	struct autofsnode *an;
	struct autofs_req *req, *r;
	struct autofs_mntdata *amp;
	ucred_t cred = vfs_context_ucred(context);
	int myreq = 1;
	int error;

	an = VTOA(vp);
	an->s_nodeflags |= IN_MOUNT;
	DBG_VOP(("autofs_request\n"));
	if (cred->cr_uid != an->s_cloneuid) {
		DBG_VOP(("autofs_request: Hey! UIDs don't match?!"));
	};
	MALLOC(req, struct autofs_req *, sizeof(*req), M_AUTOFS, M_WAITOK);
	MALLOC(req->ar_name, char *, namelen + 1, M_AUTOFS, M_WAITOK);
	bcopy(nameptr, req->ar_name, namelen);
	req->ar_name[namelen] = '\0';
	req->ar_namelen = namelen;
	req->ar_dp = dp;
	req->ar_vp = vp;
	req->ar_pid = vfs_context_pid(context);
	req->ar_uid = cred->cr_uid;
	req->ar_gid = cred->cr_groups[0]; /* XXX: correct? */
	req->ar_flags = 0;
	req->ar_errno = -1;
	req->ar_refcnt = 1;
	req->ar_onlst = 1;
	error = 0;
	amp = VFSTOAFS(VTOVFS(dp));
	LIST_FOREACH(r, &amp->autofs_reqs, ar_list) {
		if (autofs_reqequal(r, req)) {
			FREE(req->ar_name, M_AUTOFS);
			FREE(req, M_AUTOFS);
			req = r;
			r->ar_refcnt++;
			myreq = 0;
			break;
		}
	}
	if (myreq) {
		LIST_INSERT_HEAD(&amp->autofs_reqs, req, ar_list);
	}

	vfs_event_signal(&vfs_statfs(VTOVFS(dp))->f_fsid, VQ_ASSIST, 0);

	while (req->ar_flags == 0 && req->ar_errno == -1 && error == 0) {
		struct timespec timeout = {autofs_request_timeo, 0};
		
		DBG_VOP(("autofs_request sleeping\n"));
		error = msleep(req, NULL, PRIBIO, "autofs",  &timeout);
		DBG_VOP(("autofs_request woke, error = %d\n", error));
	}

	if (req->ar_errno != 0) {
		error = req->ar_errno;
		DBG_VOP(("autofs_request setting req errno to %d.\n", error));
	}
	if (--req->ar_refcnt == 0) {
		if (req->ar_onlst) LIST_REMOVE(req, ar_list);
		FREE(req->ar_name, M_AUTOFS);
		FREE(req, M_AUTOFS);
	}
	DBG_VOP(("autofs_request leaving\n"));
	return (error);
}

