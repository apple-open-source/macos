/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)webdav_vfsops.c 8.6 (Berkeley) 1/21/94
 */

/*
 * webdav Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/un.h>
/* To get funnel prototypes */
#include <kern/thread.h>

#include "webdav.h"

/*****************************************************************************/

typedef int (*PFI)();

extern char *strncpy __P((char *, const char *, size_t));	/* Kernel already includes a copy of strncpy somewhere... */
extern int strcmp __P((const char *, const char *));		/* Kernel already includes a copy of strcmp somewhere... */

extern int closef(register struct file *, register struct proc *);

/* The following refer to kernel global variables used in the loading/initialization: */

extern int maxvfsslots;							/* Total number of slots in the system's vfsconf table */
extern int maxvfsconf;							/* The highest fs type number [old-style ID] in use [dispite its name] */
extern int vfs_opv_numops;						/* The total number of defined vnode operations */
extern int kdp_flag;


/*
 * webdav File System globals:
 */

static char webdav_name[MFSNAMELEN] = "webdav";
static long webdav_mnt_cnt = 0;

/*
 * Global variables defined in other modules:
 */
extern struct vnodeopv_desc webdav_vnodeop_opv_desc;

/*****************************************************************************/

int webdav_init(vfsp)
	struct vfsconf *vfsp;
{
	webdav_hashinit();
	return (0);
}

/*****************************************************************************/

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
int webdav_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct file *fp;
	struct webdav_args args;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	struct socket *so;
	struct vnode *rvp;
	size_t size;
	int error;
	caddr_t name;
	size_t name_size;
	struct timeval tv;

	++webdav_mnt_cnt;

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
	{
		error = EOPNOTSUPP;
		goto bad;
	}

	/* Hammer in noexec so that the wild web won't endanger our users */

	mp->mnt_flag |= MNT_NOEXEC;

	error = copyin(data, (caddr_t) & args, sizeof(struct webdav_args));
	if (error)
	{
		goto bad;
	}

	thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);

	error = getsock(p->p_fd, args.pa_socket, &fp);
	if (error)
	{
		goto bdropfnl;
	}

	so = (struct socket *)fp->f_data;
	if (so->so_proto->pr_domain->dom_family != AF_UNIX)
	{
		error = ESOCKTNOSUPPORT;
		goto bdropfnl;
	}
	thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);


	error = getnewvnode(VT_WEBDAV, mp, webdav_vnodeop_p, &rvp);/* XXX */
	if (error)
	{
		goto bad;
	}

	MALLOC(rvp->v_data, void *, sizeof(struct webdavnode), M_TEMP, M_WAITOK);

	fmp = (struct webdavmount *)_MALLOC(sizeof(struct webdavmount), M_UFSMNT, M_WAITOK);/* XXX */
	rvp->v_type = VDIR;
	rvp->v_flag |= VROOT;
	pt = VTOWEBDAV(rvp);
	bzero(pt, sizeof(struct webdavnode));
	pt->pt_fileid = WEBDAV_ROOTFILEID;
	lockinit(&pt->pt_lock, PINOD, "webdavnode", 0, 0);

	fmp->pm_root = rvp;
	fmp->pm_server = fp;
	fmp->status = WEBDAV_MOUNT_SUPPORTS_STATFS;	/* assume yes until told no */
	fref(fp);

	mp->mnt_data = (qaddr_t)fmp;
	vfs_getnewfsid(mp);

	(void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);

	/* Get the uri from the args and put them in the webdavnode for
	  the purpose of building up URIs in the future */

	MALLOC(name, caddr_t, MAXPATHLEN, M_TEMP, M_WAITOK);
	(void)copyinstr(args.pa_uri, name, MAXPATHLEN - 1, &name_size);

	/* discount the null, we'll add it back later */

	--name_size;

	/* if there already was a trailing slash, blow it off, we'll add it
	  back as part of the normal processing */

	if (name[name_size - 1] == '/')
	{
		--name_size;
	}

	MALLOC(pt->pt_arg, caddr_t, name_size + 2, M_TEMP, M_WAITOK);

	bcopy(name, pt->pt_arg, name_size);
	FREE(name, M_TEMP);

	pt->pt_size = name_size + 1;

	/* put the trailing slash and the null byte in */

	pt->pt_arg[name_size] = '/';
	pt->pt_arg[name_size + 1] = '\0';

	/* put in the vnode pointer */
	pt->pt_vnode = rvp;

	(void)copyinstr(args.pa_config, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

	/* set up the current time for the time defaults in the root
	 * vnode
	 */

	microtime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, &pt->pt_atime);
	pt->pt_mtime = pt->pt_atime;
	pt->pt_ctime = pt->pt_atime;


#ifdef notdef
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy("webdav", mp->mnt_stat.f_mntfromname, sizeof("webdav"));
#endif

	return (0);

bdropfnl:

	thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);

bad:

	--webdav_mnt_cnt;
	return (error);
}

/*****************************************************************************/

int webdav_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
}

/*****************************************************************************/

int webdav_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct vnode *rootvp = VFSTOWEBDAV(mp)->pm_root;
	int error = 0;
	int flags = 0;


	if (mntflags & MNT_FORCE)
	{
		flags |= FORCECLOSE;
	}

	/*
	 * Clear out buffer cache.	I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#ifdef notyet
	mntflushbuf(mp, 0);
	if (mntinvalbuf(mp, 1))
	{
		return (EBUSY);
	}
#endif

	if (rootvp->v_usecount > 1 && !(flags & FORCECLOSE))
	{
		return (EBUSY);
	}

	error = vflush(mp, rootvp, flags);
	if (error)
	{
		return (error);
	}

	/*
	 * Release reference on underlying root vnode
	 */
	vrele(rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(rootvp);
	/*
	 * Shutdown the socket.	 This will cause the select in the
	 * daemon to wake up, and then the accept will get ECONNABORTED
	 * which it interprets as a request to go and bury itself.
	 */
	thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
	error = soshutdown((struct socket *)VFSTOWEBDAV(mp)->pm_server->f_data, 2);
	thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
	/*
	 * Discard reference to underlying file.  Must call closef because
	 * this may be the last reference.
	 */
	closef(VFSTOWEBDAV(mp)->pm_server, (struct proc *)0);
	/*
	 * Finally, throw away the webdavmount structure
	 */
	_FREE(mp->mnt_data, M_UFSMNT);				/* XXX */
	mp->mnt_data = 0;

	--webdav_mnt_cnt;
	return (error);

}

/*****************************************************************************/

int webdav_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct proc *p = current_proc();			/* XXX */
	struct vnode *vp;

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOWEBDAV(mp)->pm_root;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;
	return (0);
}

/*****************************************************************************/

int webdav_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{

	struct vnode *rootvp;
	struct webdavnode *pt;
	struct webdavmount *fmp;
	struct statfs server_sbp;
	int error = 0;
	int server_error = 0;
	struct webdav_cred pcred;
	int vnop = WEBDAV_STATFS;

	rootvp = VFSTOWEBDAV(mp)->pm_root;
	pt = VTOWEBDAV(rootvp);
	fmp = VFSTOWEBDAV(mp);

	bzero((void *) & server_sbp, sizeof(server_sbp));

	/* get the values from the server if we can.  If not, make em up */

	if (fmp->status & WEBDAV_MOUNT_SUPPORTS_STATFS)
	{
		/* while there's a WEBDAV_STATFS request outstanding, sleep */
		while (fmp->status & WEBDAV_MOUNT_STATFS)
		{
			fmp->status |= WEBDAV_MOUNT_STATFS_WANTED;
			(void) tsleep((caddr_t)&fmp->status, PRIBIO, "webdav_statfs", 0);
		}
		
		/* we're making a request so grab the token */
		fmp->status |= WEBDAV_MOUNT_STATFS;

		pcred.pcr_flag = 0;
		/* user level is ingnoring the pcred anyway */

		pcred.pcr_uid = p->p_ucred->cr_uid;
		pcred.pcr_ngroups = p->p_ucred->cr_ngroups;
		bcopy(p->p_ucred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));

		error = webdav_sendmsg(vnop, WEBDAV_USE_URL, pt, &pcred, fmp, p, (void *)NULL, 0,
			&server_error, (void *) & server_sbp, sizeof(server_sbp));

		/* XXX - When this WEBDAV_CHECK_VNODE and goto is removed, the code below
		 * that calls wakeup can be moved up here. */
		 
		/* We pended so check the state of the vnode */
		if (WEBDAV_CHECK_VNODE(rootvp))
		{
			error = EPERM;
			goto bad;
		}
		/* now fall through */
	}


	/* Note, at this point error is set to the value we want to
	  return,  Don't set error without restructuring the routine
	  Note also that we are not retuning server_error.	*/


	sbp->f_flags = 0;
	if (!server_sbp.f_bsize)
	{
		sbp->f_bsize = DEV_BSIZE;
	}
	else
	{
		sbp->f_bsize = server_sbp.f_bsize;
	}

	if (!server_sbp.f_iosize)
	{
		sbp->f_iosize = WEBDAV_IOSIZE;
	}
	else
	{
		sbp->f_iosize = server_sbp.f_iosize;
	}
	
	if (!server_sbp.f_blocks)
	{
		/* server must not support getting quotas so stop trying */
		fmp->status &= ~WEBDAV_MOUNT_SUPPORTS_STATFS;
		sbp->f_blocks = WEBDAV_NUM_BLOCKS;
		sbp->f_bfree = sbp->f_bavail = WEBDAV_FREE_BLOCKS;
	}
	else
	{
		sbp->f_blocks = server_sbp.f_blocks;
		sbp->f_bfree = server_sbp.f_bfree;
		if (!server_sbp.f_bavail)
		{
			sbp->f_bavail = sbp->f_bfree;
		}
		else
		{
			sbp->f_bavail = server_sbp.f_bavail;
		}
	}

	if (!server_sbp.f_files)
	{
		sbp->f_files = WEBDAV_NUM_FILES;
	}

	if (!server_sbp.f_ffree)
	{
		sbp->f_ffree = WEBDAV_FREE_FILES;
	}
	else
	{
		sbp->f_ffree = server_sbp.f_ffree;
	}

	if (sbp != &mp->mnt_stat)
	{
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}

bad:

	/* XXX - This code (everything up to return) can be moved up into the
	 * if statement above that tsleeps when the WEBDAV_CHECK_VNODE is removed
	 */
	
	/* we're done, so release the token */
	fmp->status &= ~WEBDAV_MOUNT_STATFS;
	
	/* if anyone else is waiting, wake them up */
	if ( fmp->status & WEBDAV_MOUNT_STATFS_WANTED )
	{
		fmp->status &= ~WEBDAV_MOUNT_STATFS_WANTED;
		wakeup((caddr_t)&fmp->status);
	}

	return (error);
}

/*****************************************************************************/

#define webdav_fhtovp ((int (*) __P((struct mount *, struct fid *, \
		struct mbuf *, struct vnode **, int *, struct ucred **)))eopnotsupp)
#define webdav_quotactl ((int (*) __P((struct mount *, int, uid_t, caddr_t, \
		struct proc *)))eopnotsupp)
#define webdav_sync ((int (*) __P((struct mount *, int, struct ucred *, \
		struct proc *)))nullop)
#define webdav_sysctl ((int (*) __P((int *, u_int, void *, size_t *, void *, \
		size_t, struct proc *)))eopnotsupp)
#define webdav_vget	((int (*) __P((struct mount *, void *, struct vnode **))) \
		eopnotsupp)
#define webdav_vptofh ((int (*) __P((struct vnode *, struct fid *)))eopnotsupp)

/*****************************************************************************/

struct vfsops webdav_vfsops = {
	webdav_mount,
	webdav_start,
	webdav_unmount,
	webdav_root,
	webdav_quotactl,
	webdav_statfs,
	webdav_sync,
	webdav_vget,
	webdav_fhtovp,
	webdav_vptofh,
	webdav_init,
	webdav_sysctl,
};

/*****************************************************************************/

int webdav_fs_module_start(int loadArgument)
{
	struct vfsconf         *newvfsconf = NULL;
	int j;
	int( ***opv_desc_vector_p)();
	int( **opv_desc_vector)();
	struct vnodeopv_entry_desc *opve_descp;
	int error = 0;
	boolean_t funnel_state;

#pragma unused(loadArgument)

	/*
	 * This routine is responsible for all the initialization that would
	 * ordinarily be done as part of the system startup;	  */

	funnel_state = thread_funnel_set(kernel_flock, TRUE);
#if DEBUG
	/* Debugger(); */
#endif

	kprintf("load_webdavfs: Starting...\n");

	MALLOC(newvfsconf, void *, sizeof(struct vfsconf), M_TEMP, M_WAITOK);

	bzero(newvfsconf, sizeof(struct vfsconf));

	newvfsconf->vfc_vfsops = &webdav_vfsops;
	strncpy(&newvfsconf->vfc_name[0], webdav_name, MFSNAMELEN);
	newvfsconf->vfc_typenum = maxvfsconf++;
	newvfsconf->vfc_refcount = 0;
	newvfsconf->vfc_flags = 0;
	newvfsconf->vfc_mountroot = NULL;			/* Can't mount root of file system */
	newvfsconf->vfc_next = NULL;

	/* Based on vfs_op_init and ... */
	opv_desc_vector_p = webdav_vnodeop_opv_desc.opv_desc_vector_p;

	kprintf("load_webdav: Allocating and initializing VNode ops vector...\n");

	/*
	 * Allocate and init the vector.
	 * Also handle backwards compatibility.
	 */
	/* XXX - shouldn't be M_TEMP */

	MALLOC(*opv_desc_vector_p, PFI * , vfs_opv_numops * sizeof(PFI), M_TEMP, M_WAITOK);
	bzero(*opv_desc_vector_p, vfs_opv_numops * sizeof(PFI));

	opv_desc_vector = *opv_desc_vector_p;
	for (j = 0; webdav_vnodeop_opv_desc.opv_desc_ops[j].opve_op; j++)
	{
		opve_descp = &(webdav_vnodeop_opv_desc.opv_desc_ops[j]);

		/*
		 * Sanity check:  is this operation listed
		 * in the list of operations?  We check this
		 * by seeing if its offest is zero.	 Since
		 * the default routine should always be listed
		 * first, it should be the only one with a zero
		 * offset.	Any other operation with a zero
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
			opve_descp->opve_op->vdesc_offset != VOFFSET(vop_default))
		{
			printf("load_webdav: operation %s not listed in %s.\n", opve_descp->opve_op->vdesc_name,
				"vfs_op_descs");
			panic("load_webdav: bad operation");
		}
		/*
		 * Fill in this entry.
		 */
		opv_desc_vector[opve_descp->opve_op->vdesc_offset] = opve_descp->opve_impl;
	}

	/*
	 * Finally, go back and replace unfilled routines
	 * with their default.	(Sigh, an O(n^3) algorithm.	 I
	 * could make it better, but that'd be work, and n is small.)
	 */
	opv_desc_vector_p = webdav_vnodeop_opv_desc.opv_desc_vector_p;

	/*
	 * Force every operations vector to have a default routine.
	 */
	opv_desc_vector = *opv_desc_vector_p;
	if (opv_desc_vector[VOFFSET(vop_default)] == NULL)
	{
		panic("load_webdav: operation vector without default routine.");
	}
	for (j = 0; j < vfs_opv_numops; j++)
	{
		if (opv_desc_vector[j] == NULL)
		{
			opv_desc_vector[j] = opv_desc_vector[VOFFSET(vop_default)];
		}
	}

	/* Ok, vnode vectors are set up, vfs vectors are set up, add it in */
	error = vfsconf_add(newvfsconf);
	if (error)
	{
		goto done;
	}


done:
	
	if (newvfsconf)
	{
		FREE(newvfsconf, M_TEMP);
	}

	thread_funnel_set(kernel_flock, funnel_state);
	return (error);
}

/*****************************************************************************/

int webdav_fs_module_stop(int unloadArgument)
{
	boolean_t funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	kprintf("unload_webdav: removing webdav from vfs conf. list...\n");

	if (webdav_mnt_cnt)
	{
		thread_funnel_set(kernel_flock, funnel_state);
		return (EBUSY);
	}
	vfsconf_del(webdav_name);
	FREE(*webdav_vnodeop_opv_desc.opv_desc_vector_p, M_TEMP);
	webdav_hashdestroy();

	thread_funnel_set(kernel_flock, funnel_state);
	return (0);
}

/*****************************************************************************/
