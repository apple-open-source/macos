/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)webdav_vfsops.c 8.6 (Berkeley) 1/21/94
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/mount.h>
//#include <sys/syslog.h>
#include <libkern/libkern.h>

#include "webdav.h"
#include "webdav_utils.h"

/*****************************************************************************/

/*
 * Global variables defined in other modules:
 */
extern struct vnodeopv_desc webdav_vnodeop_opv_desc;

/*
 * webdav File System globals:
 */

char webdav_name[MFSNAMELEN] = "webdav";

lck_grp_t *webdav_rwlock_group;
lck_rw_t  ref_tbl_rwlock;


static long webdav_mnt_cnt = 0;
/*
 *¥ vfs_fsadd: second parameter should be (void **)?
 * If so, then the following should be a (void *).
 */
static vfstable_t webdav_vfsconf;


#define WEBDAV_MAX_REFS   256
static struct open_associatecachefile *webdav_ref_table[WEBDAV_MAX_REFS];
static int webdav_vfs_statfs(struct mount *mp, register struct vfsstatfs *sbp, struct webdav_vfsstatfs in_statfs);


static struct vnodeopv_desc *webdav_vnodeop_opv_desc_list[1] =
{
	&webdav_vnodeop_opv_desc
};

/*****************************************************************************/

/* initialize the webdav_ref_table */
static void webdav_init_ref_table(void)
{
	int ref;
	
	lck_rw_lock_exclusive(&ref_tbl_rwlock);
	for ( ref = 0; ref < WEBDAV_MAX_REFS; ++ref )
	{
		webdav_ref_table[ref] = NULL;
	}
	lck_rw_done(&ref_tbl_rwlock);
}

/*****************************************************************************/

/* assign a entry in the webdav_ref_table to associatecachefile and return ref */
__private_extern__
int webdav_assign_ref(struct open_associatecachefile *associatecachefile, int *ref)
{
	int i;
	int error;
	struct timespec ts;
	
	while ( TRUE )
	{
		lck_rw_lock_exclusive(&ref_tbl_rwlock);
		for ( i = 0; i < WEBDAV_MAX_REFS; ++i )
		{
			if ( webdav_ref_table[i] == NULL )
			{
				webdav_ref_table[i] = associatecachefile;
				*ref = i;
				lck_rw_done(&ref_tbl_rwlock);
				return ( 0 );
			}
		}
		
		/* table is completely used... sleep a little and then try again */
		lck_rw_done(&ref_tbl_rwlock);
		ts.tv_sec = 1;
		ts.tv_nsec = 0;
		error = msleep((caddr_t)&webdav_ref_table, NULL, PCATCH, "webdav_get_open_ref", &ts);
		if ( error && (error != EWOULDBLOCK) )
		{
			/* bail out on errors -- the user probably hit control-c */
			*ref = -1;
			return ( EIO );
		}
	}
}

/*****************************************************************************/

/* translate a ref to a pointer to struct open_associatecachefile */
static int webdav_translate_ref(int ref, struct open_associatecachefile **associatecachefile)
{
	int error = 0;
	
	/* range check ref */
	if ( (ref < 0) || (ref >= WEBDAV_MAX_REFS) )
	{
		return ( EIO );
	}
	
	/* translate */
	lck_rw_lock_shared(&ref_tbl_rwlock);	
	*associatecachefile = webdav_ref_table[ref];
	
	if ( *associatecachefile == NULL )
	{
		/* ref wasn't valid */
		error = EIO;
	}

	lck_rw_done(&ref_tbl_rwlock);
	return (error);
}

/*****************************************************************************/

/* release a ref */
__private_extern__
void webdav_release_ref(int ref)
{
	if ( (ref >= 0) && (ref < WEBDAV_MAX_REFS) )
	{
		lck_rw_lock_exclusive(&ref_tbl_rwlock);
		webdav_ref_table[ref] = NULL;
		lck_rw_done(&ref_tbl_rwlock);
		
		wakeup((caddr_t)&webdav_ref_table);
	}
}

/*****************************************************************************/

/*
 * Called once from vfs_fsadd() to allow us to initialize.
 */
static int webdav_init(struct vfsconf *vfsp)
{
	#pragma unused(vfsp)
	
	START_MARKER("webdav_init");
	
	webdav_rwlock_group = lck_grp_alloc_init("webdav-rwlock", LCK_GRP_ATTR_NULL);
	lck_rw_init(&ref_tbl_rwlock, webdav_rwlock_group, LCK_ATTR_NULL);
	webdav_init_ref_table();
	webdav_hashinit();  /* webdav_hashdestroy() is called from webdav_fs_module_stop() */
	
	RET_ERR("webdav_init", 0);
}

/*****************************************************************************/

/*
 * Mount a file system
 */
static int webdav_mount(struct mount *mp, vnode_t devvp, user_addr_t data, vfs_context_t context)
{
	#pragma unused(devvp)
	struct user_webdav_args args;
	struct webdavmount *fmp = NULL;
	vnode_t rvp;
	size_t size;
	int error;
	struct timeval tv;
	struct timespec ts;
	struct vfsstatfs *vfsp;
	struct webdav_timespec64 wts;

	START_MARKER("webdav_mount");
	
	++webdav_mnt_cnt;

	/*
	 * Update is a no-op
	 */
	if (vfs_isupdate(mp))
	{
		error = ENOTSUP;
		goto bad;
	}

	/* Hammer in noexec so that the wild web won't endanger our users */
	vfs_setflags(mp, MNT_NOEXEC);

	/*
	 * copy in the mount arguments
	 */
	if ( vfs_context_is64bit(context) )
	{
		error = copyin(data, (caddr_t)&args, sizeof(struct user_webdav_args));
		if (error)
		{
			goto bad;
		}
	}
	else
	{
		struct webdav_args args_32;
		error = copyin(data, (caddr_t)&args_32, sizeof(struct webdav_args));
		if (error)
		{
			goto bad;
		}
		args.pa_mntfromname			= CAST_USER_ADDR_T(args_32.pa_mntfromname);
		args.pa_version				= args_32.pa_version;
		args.pa_socket_namelen		= args_32.pa_socket_namelen;
		args.pa_socket_name			= CAST_USER_ADDR_T(args_32.pa_socket_name);
		args.pa_vol_name			= CAST_USER_ADDR_T(args_32.pa_vol_name);
		args.pa_flags				= args_32.pa_flags;
		args.pa_server_ident		= args_32.pa_server_ident;
		args.pa_root_id				= args_32.pa_root_id;
		args.pa_root_fileid			= args_32.pa_root_fileid;
		args.pa_dir_size			= args_32.pa_dir_size;
		args.pa_link_max			= args_32.pa_link_max;
		args.pa_name_max			= args_32.pa_name_max;
		args.pa_path_max			= args_32.pa_path_max;
		args.pa_pipe_buf			= args_32.pa_pipe_buf;
		args.pa_chown_restricted	= args_32.pa_chown_restricted;
		args.pa_no_trunc			= args_32.pa_no_trunc;
		bcopy (&args_32.pa_vfsstatfs, &args.pa_vfsstatfs, sizeof (args.pa_vfsstatfs));
	}
	
	if (args.pa_version != kCurrentWebdavArgsVersion)
	{
		/* invalid version argument */
		error = EINVAL;
		printf("the webdav_fs.kext and mount_webdav executables are incompatible\n");
		goto bad;
	}

	/*
	 * create the webdavmount
	 */

	MALLOC(fmp, struct webdavmount *, sizeof(struct webdavmount), M_TEMP, M_WAITOK);
	if ( fmp == NULL )
	{
		error = EINVAL;
		goto bad;
	}
	
	bzero(fmp, sizeof(struct webdavmount));
	
	lck_mtx_init(&fmp->pm_mutex, webdav_rwlock_group, LCK_ATTR_NULL);
	fmp->pm_status = WEBDAV_MOUNT_SUPPORTS_STATFS;	/* assume yes until told no */
	if ( args.pa_flags & WEBDAV_SUPPRESSALLUI )
	{
		/* suppress UI when connection is lost */
		fmp->pm_status |= WEBDAV_MOUNT_SUPPRESS_ALL_UI;
	}
	if ( args.pa_flags & WEBDAV_SECURECONNECTION )
	{
		/* the connection to the server is secure */
		fmp->pm_status |= WEBDAV_MOUNT_SECURECONNECTION;
	}
	
	fmp->pm_server_ident = args.pa_server_ident;
	
	fmp->pm_mountp = mp;
	
	/* Get the volume name from the args and store it for webdav_packvolattr() */
	MALLOC(fmp->pm_vol_name, caddr_t, NAME_MAX + 1, M_TEMP, M_WAITOK);
	bzero(fmp->pm_vol_name, NAME_MAX + 1);
	error = copyinstr(args.pa_vol_name, fmp->pm_vol_name, NAME_MAX, &size);
	if (error)
	{
		goto bad;
	}

	/* Get the server sockaddr from the args */
	MALLOC(fmp->pm_socket_name, struct sockaddr *, args.pa_socket_namelen, M_TEMP, M_WAITOK);
	error = copyin(args.pa_socket_name, fmp->pm_socket_name, args.pa_socket_namelen);
	if (error)
	{
		goto bad;
	}
	
	fmp->pm_open_connections = 0;

	fmp->pm_dir_size = args.pa_dir_size;

	/* copy pathconf values from the args */
	fmp->pm_link_max = args.pa_link_max;
	fmp->pm_name_max = args.pa_name_max;
	fmp->pm_path_max = args.pa_path_max;
	fmp->pm_pipe_buf = args.pa_pipe_buf;
	fmp->pm_chown_restricted = args.pa_chown_restricted;
	fmp->pm_no_trunc = args.pa_no_trunc;

	vfs_setfsprivate(mp, (void *)fmp);
	vfs_getnewfsid(mp);
	
	(void)copyinstr(args.pa_mntfromname, vfs_statfs(mp)->f_mntfromname, MNAMELEN - 1, &size);
	bzero(vfs_statfs(mp)->f_mntfromname + size, MNAMELEN - size);

	/*
	 * create the root vnode
	 */
	
	microtime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);
	
	// Now setup a webdav_timespec64, like webdav_get requires
	timespec_to_webdav_timespec64(ts, &wts);
	error = webdav_get(mp, NULLVP, 1, NULL,
		args.pa_root_id, args.pa_root_fileid, VDIR, wts, wts, wts, fmp->pm_dir_size, &rvp);
	if (error)
	{
		goto bad;
	}
	
	/* release the lock from webdav_get() */
	webdav_unlock(VTOWEBDAV(rvp));
	
	/* hold on to rvp until unmount */
	error = vnode_ref(rvp);
	(void) vnode_put(rvp);
	if (error)
	{
		goto bad;
	}
	
	fmp->pm_root = rvp;
	
	vfs_setauthopaque(mp);
	
	/* initialize statfs data */
	vfsp = vfs_statfs (mp);
	(void) webdav_vfs_statfs(mp, vfsp, args.pa_vfsstatfs);

	return (0);

bad:

	--webdav_mnt_cnt;

	/* free any memory allocated before failure */
	if ( fmp != NULL )
	{
		if ( fmp->pm_vol_name != NULL )
		{
			FREE(fmp->pm_vol_name, M_TEMP);
		}
		if ( fmp->pm_socket_name != NULL )
		{
			FREE(fmp->pm_socket_name, M_TEMP);
		}
		lck_mtx_destroy(&fmp->pm_mutex, webdav_rwlock_group);
		FREE(fmp, M_TEMP);
		
		/* clear the webdavmount in the mount point so anyone looking at it will see it's gone */
		vfs_setfsprivate(mp, NULL);
	}
	
	RET_ERR("webdav_mount", error);
}

/*****************************************************************************/

/*
 * Called just after VFS_MOUNT(9) but before first access.
 */
static int webdav_start(struct mount *mp, int flags, vfs_context_t context)
{
	#pragma unused(mp, flags, context)
	
	START_MARKER("webdav_start");
	
	RET_ERR("webdav_start", 0);
}

/*****************************************************************************/

/*
 * Unmount a file system
 */
static int webdav_unmount(struct mount *mp, int mntflags, vfs_context_t context)
{
	vnode_t rootvp = VFSTOWEBDAV(mp)->pm_root;
	int error = 0;
	int flags = 0;
	struct webdavmount *fmp;
	int server_error;
	struct webdav_request_unmount request_unmount;

	START_MARKER("webdav_unmount");
	
	fmp = VFSTOWEBDAV(mp);

	if (mntflags & MNT_FORCE)
	{
		flags |= FORCECLOSE;
	}

	/* flush all except root vnode */
	error = vflush(mp, rootvp, flags);
	if (error)
	{
		return (error);
	}

	/* see if anyone is using the root vnode besides the reference we took in webdav_mount */
	if ( vnode_isinuse(rootvp, 1) && !(flags & FORCECLOSE) )
	{
		return (EBUSY);
	}

	webdav_copy_creds(context, &request_unmount.pcr);

	/* send the unmount message message to user-land and ignore errors */
	(void) webdav_sendmsg(WEBDAV_UNMOUNT, fmp,
		&request_unmount, sizeof(struct webdav_request_unmount), 
		NULL, 0, 
		&server_error, NULL, 0);

	/* release reference on the root vnode taken in webdav_mount */
	vnode_rele(rootvp);
	
	/* since we blocked (sending the message), drain again */
	error = vflush(mp, NULLVP, flags);

	/* clear the webdavmount in the mount point so anyone looking at it will see it's gone */
	vfs_setfsprivate(mp, NULL);
		
	/* free the webdavmount structure and related allocated memory */
	FREE(fmp->pm_vol_name, M_TEMP);
	FREE(fmp->pm_socket_name, M_TEMP);
	lck_mtx_destroy(&fmp->pm_mutex, webdav_rwlock_group);
	FREE(fmp, M_TEMP);
	
	--webdav_mnt_cnt;
	
	RET_ERR("webdav_unmount", error);
}

/*****************************************************************************/

/*
 * Get (vnode_get) the vnode for the root directory of the file system.
 */
static int webdav_root(struct mount *mp, struct vnode **vpp, vfs_context_t context)
{
	#pragma unused(context)
	int error;
	vnode_t vp;

	START_MARKER("webdav_root");
	
	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOWEBDAV(mp)->pm_root;
	error = vnode_get(vp);
	if ( error )
	{
		*vpp = NULLVP;
	}
	else
	{
		*vpp = vp;
	}
	
	RET_ERR("webdav_root", error);
}


/*
 * Get file system statistics.
 */
static int webdav_vfs_statfs(struct mount *mp, register struct vfsstatfs *sbp, struct webdav_vfsstatfs in_statfs)
{
	struct webdavmount *fmp = VFSTOWEBDAV(mp);

	START_MARKER("webdav_vfs_statfs");

	if (!in_statfs.f_bsize)
		sbp->f_bsize = (uint32_t) S_BLKSIZE;
	else
		sbp->f_bsize = (uint32_t) in_statfs.f_bsize;

	if (!in_statfs.f_iosize)
		sbp->f_iosize = (uint32_t) 0x1000;	/* 0x1000 is as good a size as anything */
	else
		sbp->f_iosize = (uint32_t) in_statfs.f_iosize;
	
	fmp->pm_iosize = sbp->f_iosize;			/* save this for webdav_vfs_getattr to use */

	if (!in_statfs.f_blocks) {
		/* server must not support quota properties */
		fmp->pm_status &= ~WEBDAV_MOUNT_SUPPORTS_STATFS;
		
		sbp->f_blocks = (uint64_t) WEBDAV_NUM_BLOCKS;
		sbp->f_bfree = (uint64_t) WEBDAV_FREE_BLOCKS;
		sbp->f_bavail = (uint64_t) WEBDAV_FREE_BLOCKS;
	}
	else {
		sbp->f_blocks = (uint64_t) in_statfs.f_blocks;
		sbp->f_bfree = (uint64_t) in_statfs.f_bfree;
		if (!in_statfs.f_bavail) {
			sbp->f_bavail = (uint64_t) in_statfs.f_bfree;
		}
		else {
			sbp->f_bavail = (uint64_t) in_statfs.f_bavail;
		}
	}

	if (!in_statfs.f_files)
		sbp->f_files = (uint64_t) WEBDAV_NUM_FILES;
	else
		sbp->f_files = (uint64_t) in_statfs.f_files;

	if (!in_statfs.f_ffree)
		sbp->f_ffree = (uint64_t) WEBDAV_FREE_FILES;
	else
		sbp->f_ffree = (uint64_t) in_statfs.f_ffree;

	if ( fmp->pm_status & WEBDAV_MOUNT_SECURECONNECTION )
		sbp->f_fssubtype = 1; /* secure connection */
	else
		sbp->f_fssubtype = 0; /* regular connection */

	RET_ERR("webdav_vfs_statfs", 0);

	return (0);
}


/*****************************************************************************/

/*
 * Return information about a mounted file system.
 */
static int webdav_vfs_getattr(struct mount *mp, struct vfs_attr *sbp, vfs_context_t context)
{
	struct webdavmount *fmp;
	struct webdav_reply_statfs reply_statfs;
	int error = 0;
	int server_error = 0;
	int callServer = 0;
	struct webdav_request_statfs request_statfs;

	START_MARKER("webdav_vfs_getattr");
	
	fmp = VFSTOWEBDAV(mp);

	bzero(&reply_statfs, sizeof(struct webdav_reply_statfs));

	// Check if we have attributes that must be fetched from the server
	if ( VFSATTR_IS_ACTIVE(sbp, f_bsize) || VFSATTR_IS_ACTIVE(sbp, f_blocks) ||
		 VFSATTR_IS_ACTIVE(sbp, f_bfree) || VFSATTR_IS_ACTIVE(sbp, f_bavail) ||
		 VFSATTR_IS_ACTIVE(sbp, f_files) || VFSATTR_IS_ACTIVE(sbp, f_ffree))
	{
		callServer = 1;
	}
	
	if (callServer)
	{
		/* get the values from the server if we can.  If not, make them up */
		lck_mtx_lock(&fmp->pm_mutex);
		if (fmp->pm_status & WEBDAV_MOUNT_SUPPORTS_STATFS)
		{
			/* while there's a WEBDAV_STATFS request outstanding, sleep */
			while (fmp->pm_status & WEBDAV_MOUNT_STATFS)
			{
				fmp->pm_status |= WEBDAV_MOUNT_STATFS_WANTED;
				error = msleep((caddr_t)&fmp->pm_status, &fmp->pm_mutex, PCATCH, "webdav_vfs_getattr", NULL);
				if ( error )
				{
					/* Note that we specified PCATCH in msleep. */
					/* Don't bother trying to fetching stats */
					/* from the server, break out.  We will return */
					/* whatever msleep returned.  */
					goto ready;
				}
			}
		
			/* we're making a request so grab the token */
			fmp->pm_status |= WEBDAV_MOUNT_STATFS;
			lck_mtx_unlock(&fmp->pm_mutex);

			webdav_copy_creds(context, &request_statfs.pcr);
			request_statfs.root_obj_id = VTOWEBDAV(VFSTOWEBDAV(mp)->pm_root)->pt_obj_id;

			error = webdav_sendmsg(WEBDAV_STATFS, fmp,
				&request_statfs, sizeof(struct webdav_request_statfs), 
				NULL, 0,
				&server_error, &reply_statfs, sizeof(struct webdav_reply_statfs));

			/* we're done, so release the token */
			lck_mtx_lock(&fmp->pm_mutex);
			fmp->pm_status &= ~WEBDAV_MOUNT_STATFS;
		
			/* if anyone else is waiting, wake them up */
			if ( fmp->pm_status & WEBDAV_MOUNT_STATFS_WANTED )
			{
				fmp->pm_status &= ~WEBDAV_MOUNT_STATFS_WANTED;
				wakeup((caddr_t)&fmp->pm_status);
			}
			/* now fall through */
		}
		lck_mtx_unlock(&fmp->pm_mutex);
	}
	
ready:
	/* Note, at this point error is set to the value we want to
	  return,  Don't set error without restructuring the routine
	  Note also that we are not returning server_error.	*/

	if (VFSATTR_IS_ACTIVE(sbp, f_bsize))
	{
		if (!reply_statfs.fs_attr.f_bsize)
		{
			VFSATTR_RETURN(sbp, f_bsize, S_BLKSIZE);
		}
		else
		{
			// ***LP64***
			// reply_statfs.fs_attr.f_bsize is 64-bits,
			// but vfs_attr.f_bsize is 32_bits
			VFSATTR_RETURN(sbp, f_bsize, (uint32_t)reply_statfs.fs_attr.f_bsize);
		}
	}
	
	if (VFSATTR_IS_ACTIVE(sbp, f_iosize))
		VFSATTR_RETURN(sbp, f_iosize, fmp->pm_iosize);
	
	if (VFSATTR_IS_ACTIVE(sbp, f_blocks))
	{
		if (!reply_statfs.fs_attr.f_blocks)
		{
			/* Did we actually get f_blocks back from the WebDAV server? */
			if ( error == 0 && server_error == 0 )
			{
				/* server must not support getting quotas so stop trying */
				fmp->pm_status &= ~WEBDAV_MOUNT_SUPPORTS_STATFS;
			}
		}
		else
		{
			VFSATTR_RETURN(sbp, f_blocks, reply_statfs.fs_attr.f_blocks);
			VFSATTR_RETURN(sbp, f_bfree, reply_statfs.fs_attr.f_bfree);
			if (!reply_statfs.fs_attr.f_bavail)
			{
				VFSATTR_RETURN(sbp, f_bavail, reply_statfs.fs_attr.f_bfree);
			}
			else
			{
				VFSATTR_RETURN(sbp, f_bavail, reply_statfs.fs_attr.f_bavail);
			}
		}
	}
	
	if (VFSATTR_IS_ACTIVE(sbp, f_files))
	{
		if (!reply_statfs.fs_attr.f_files)
		{
			VFSATTR_RETURN(sbp, f_files, WEBDAV_NUM_FILES);
		}
		else
		{
			VFSATTR_RETURN(sbp, f_files, reply_statfs.fs_attr.f_files);
		}
	}
	
	if (VFSATTR_IS_ACTIVE(sbp, f_ffree))
	{
		if (!reply_statfs.fs_attr.f_ffree)
		{
			VFSATTR_RETURN(sbp, f_ffree, WEBDAV_FREE_FILES);
		}
		else
		{
			VFSATTR_RETURN(sbp, f_ffree, reply_statfs.fs_attr.f_ffree);
		}
	}
	
	if (VFSATTR_IS_ACTIVE(sbp, f_fssubtype))
	{
		if ( fmp->pm_status & WEBDAV_MOUNT_SECURECONNECTION )
		{
			VFSATTR_RETURN(sbp, f_fssubtype, 1); /* secure connection */
		}
		else
		{
			VFSATTR_RETURN(sbp, f_fssubtype, 0); /* regular connection */
		}
	}
	
	if ( VFSATTR_IS_ACTIVE(sbp, f_capabilities) )
	{
		vol_capabilities_attr_t *vcapattrptr;

		vcapattrptr = &sbp->f_capabilities;

		/*
		 * Capabilities this volume format has.  Note that
		 * we do not set VOL_CAP_FMT_PERSISTENTOBJECTIDS.
		 * That's because we can't resolve an inode number
		 * into a directory entry (parent and name), which
		 * Carbon would need to support PBResolveFileIDRef.
		 */
		vcapattrptr->capabilities[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_FAST_STATFS; /* statfs is cached by webdavfs, so upper layers shouldn't cache */
		vcapattrptr->capabilities[VOL_CAPABILITIES_INTERFACES] =
			0; /* None of the optional interfaces are implemented. */
		vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

		/* Capabilities we know about: */
		vcapattrptr->valid[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_PERSISTENTOBJECTIDS |
			VOL_CAP_FMT_SYMBOLICLINKS |
			VOL_CAP_FMT_HARDLINKS |
			VOL_CAP_FMT_JOURNAL |
			VOL_CAP_FMT_JOURNAL_ACTIVE |
			VOL_CAP_FMT_NO_ROOT_TIMES |
			VOL_CAP_FMT_SPARSE_FILES |
			VOL_CAP_FMT_ZERO_RUNS |
			/* While WebDAV FS is case sensitive and case preserving,
			* not all WebDAV servers are case sensitive and case preserving.
			* That's because the volume used for storage on a WebDAV server
			* may not be case sensitive or case preserving. So, rather than
			* providing a wrong yes or no answer for VOL_CAP_FMT_CASE_SENSITIVE
			* and VOL_CAP_FMT_CASE_PRESERVING, we'll deny knowledge of those
			* volume attributes.
			*/
#if 0
			VOL_CAP_FMT_CASE_SENSITIVE |
			VOL_CAP_FMT_CASE_PRESERVING |
#endif
			VOL_CAP_FMT_2TB_FILESIZE |
			VOL_CAP_FMT_FAST_STATFS;

			if ( !(fmp->pm_status & WEBDAV_MOUNT_SUPPORTS_STATFS)) {
				// Server does not support quoata properties
				vcapattrptr->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_NO_VOLUME_SIZES;
				vcapattrptr->valid[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_NO_VOLUME_SIZES;
			}
		
			if ( !(fmp->pm_server_ident & WEBDAV_IDISK_SERVER)) {
				// See <rdar://problem/6063471> WebDAV client fails to upload files larger than 4 gigs in size

				// WebDAV protocol does not provide a way to query the maximum file size limit of a server.
				// But if we deny knowledge of the VOL_CAP_FMT_2TB_FILESIZE bit, Finder will not allow
				// files 4 GB or larger to be uploaded to the server.
				//
				// So we will set the VOL_CAP_FMT_2TB_FILESIZE bit and assume the server supports large files,
				// with one exception: iDisk.
				// We don't set this volume capability bit if we know we are connected to an iDisk server,
				// because iDisk servers impose a maximum file size limit of 2 GB.
				//
				vcapattrptr->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_2TB_FILESIZE;
			}

		vcapattrptr->valid[VOL_CAPABILITIES_INTERFACES] =
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
		vcapattrptr->valid[VOL_CAPABILITIES_RESERVED1] = 0;
		vcapattrptr->valid[VOL_CAPABILITIES_RESERVED2] = 0;
		
		VFSATTR_SET_SUPPORTED(sbp, f_capabilities);
	}
	
	if ( VFSATTR_IS_ACTIVE(sbp, f_attributes) )
	{
		enum
		{
			WEBDAV_ATTR_CMN_NATIVE = 0,
			WEBDAV_ATTR_CMN_SUPPORTED = 0,
			WEBDAV_ATTR_VOL_NATIVE = ATTR_VOL_NAME |
				ATTR_VOL_CAPABILITIES |
				ATTR_VOL_ATTRIBUTES,
			WEBDAV_ATTR_VOL_SUPPORTED = WEBDAV_ATTR_VOL_NATIVE,
			WEBDAV_ATTR_DIR_NATIVE = 0,
			WEBDAV_ATTR_DIR_SUPPORTED = 0,
			WEBDAV_ATTR_FILE_NATIVE = 0,
			WEBDAV_ATTR_FILE_SUPPORTED = 0,
			WEBDAV_ATTR_FORK_NATIVE = 0,
			WEBDAV_ATTR_FORK_SUPPORTED = 0,
		};

		vol_attributes_attr_t *volattrptr;

		volattrptr = &sbp->f_attributes;

		volattrptr->validattr.commonattr = WEBDAV_ATTR_CMN_SUPPORTED;
		volattrptr->validattr.volattr = WEBDAV_ATTR_VOL_SUPPORTED;
		volattrptr->validattr.dirattr = WEBDAV_ATTR_DIR_SUPPORTED;
		volattrptr->validattr.fileattr = WEBDAV_ATTR_FILE_SUPPORTED;
		volattrptr->validattr.forkattr = WEBDAV_ATTR_FORK_SUPPORTED;

		volattrptr->nativeattr.commonattr = WEBDAV_ATTR_CMN_NATIVE;
		volattrptr->nativeattr.volattr = WEBDAV_ATTR_VOL_NATIVE;
		volattrptr->nativeattr.dirattr = WEBDAV_ATTR_DIR_NATIVE;
		volattrptr->nativeattr.fileattr = WEBDAV_ATTR_FILE_NATIVE;
		volattrptr->nativeattr.forkattr = WEBDAV_ATTR_FORK_NATIVE;

		VFSATTR_SET_SUPPORTED(sbp, f_attributes);
	}
	
	if ( VFSATTR_IS_ACTIVE(sbp, f_vol_name) )
	{
		(void) strncpy(sbp->f_vol_name, fmp->pm_vol_name, MAXPATHLEN);
		VFSATTR_SET_SUPPORTED(sbp, f_vol_name);
	}

	RET_ERR("webdav_vfs_getattr", error);
}

/*****************************************************************************/

/*
 * webdav_sysctl handles the VFS_CTL_QUERY request which tells interested
 * parties if the connection with the remote server is up or down.
 * It also handles receiving and converting cache file descriptors to vnode_t
 * for webdav_open().
 */
static int webdav_sysctl(int *name, u_int namelen, user_addr_t oldp, size_t *oldlenp,
	user_addr_t newp, size_t newlen, vfs_context_t context)
{
	#pragma unused(oldlenp, newp)
	int error;
	struct sysctl_req *req;
	struct vfsidctl vc;
	struct user_vfsidctl user_vc;
	struct mount *mp;
	struct webdavmount *fmp;
	struct vfsquery vq;

	START_MARKER("webdav_sysctl");
	
	switch ( name[0] )
	{
		case WEBDAV_ASSOCIATECACHEFILE_SYSCTL:
			{
				int ref;
				int fd;
				struct open_associatecachefile *associatecachefile;
				vnode_t vp;
				
				if ( namelen > 3 )
				{
					error = ENOTDIR;	/* overloaded */
					break;
				}
				
				/* make sure there is no incoming data */
				if ( newlen != 0 )
				{
					printf("webdav_sysctl: newlen != 0\n");
					error = EINVAL;
					break;
				}
				
				/*
				 * name[1] is the reference into the webdav_ref_table
				 * name[2] is the file descriptor
				 */
				ref = name[1];
				fd = name[2];
								
				error = webdav_translate_ref(ref, &associatecachefile);
				if ( error != 0 )
				{
					printf("webdav_sysctl: webdav_translate_ref() failed\n");
					break;
				}
				
				error = file_vnode_withvid(fd, &vp, NULL);
				if ( error != 0 )
				{
					printf("webdav_sysctl: file_vnode() failed\n");
					break;
				}
				
				/* take a reference on it so that it won't go away (reference released by webdav_close_nommap()) */
				vnode_get(vp);
				vnode_ref(vp);
				vnode_put(vp);
				
				(void) file_drop(fd);

				/* store the cache file's vnode in the webdavnode */
				associatecachefile->cachevp = vp;
				
				/* store the PID of the process that called us for validation in webdav_open */
				associatecachefile->pid = vfs_context_pid(context);
				
				/* success */
				error = 0;
			}
			break;
			
		case VFS_CTL_QUERY:
			if ( namelen > 1 )
			{
				error = ENOTDIR;	/* overloaded */
				break;
			}
			
			req = CAST_DOWN(struct sysctl_req *, oldp); /* we're new style vfs sysctl. */
			
			if ( vfs_context_is64bit(context) ) 
			{
				error = SYSCTL_IN(req, &user_vc, sizeof(user_vc));
				if ( error )
				{
					break;
				}
				
				mp = vfs_getvfs(&user_vc.vc_fsid);
			} 
			else 
			{
				error = SYSCTL_IN(req, &vc, sizeof(vc));
				if ( error )
				{
					break;
				}
				
				mp = vfs_getvfs(&vc.vc_fsid);
			}

			if ( mp == NULL )
			{
				error = ENOENT;
			}
			else
			{
				fmp = VFSTOWEBDAV(mp);
				bzero(&vq, sizeof(vq));
				if ( fmp != NULL )
				{
					if ( fmp->pm_status & WEBDAV_MOUNT_TIMEO )
					{
						vq.vq_flags |= VQ_NOTRESP;
					}
					if ( fmp->pm_status & WEBDAV_MOUNT_DEAD )
					{
						vq.vq_flags |= VQ_DEAD;
					}
				}
				error = SYSCTL_OUT(req, &vq, sizeof(vq));
			}
			break;
			
		default:
			error = ENOTSUP;
			break;
	}

	RET_ERR("webdav_sysctl", error);
}

/*****************************************************************************/

/* ignore sync requests since syncing a dirty file pushes the entire file to the server */

#define webdav_sync ((int (*)(struct mount *mp, int waitfor, \
		vfs_context_t context)) nullop)

/*****************************************************************************/

struct vfsops webdav_vfsops = {
	webdav_mount,
	webdav_start,
	webdav_unmount,
	webdav_root,
	NULL,
	webdav_vfs_getattr,
	webdav_sync,
	NULL,
	NULL,
	NULL,
	webdav_init,
	webdav_sysctl,
	NULL,
	{0}
};

/*****************************************************************************/

__private_extern__
kern_return_t webdav_fs_module_start(struct kmod_info *ki, void *data)
{
	#pragma unused(ki, data)
	errno_t error;
	struct vfs_fsentry vfe;

	bzero(&vfe, sizeof(struct vfs_fsentry));
	vfe.vfe_vfsops = &webdav_vfsops;	/* vfs operations */
	vfe.vfe_vopcnt = 1;					/* # of vnodeopv_desc being registered (reg, spec, fifo ...) */
	vfe.vfe_opvdescs = webdav_vnodeop_opv_desc_list; /* null terminated;  */
	vfe.vfe_fstypenum = 0;				/* historic file system type number (we have none)*/
	strncpy(vfe.vfe_fsname, webdav_name, strlen(webdav_name));
	
	/* define the FS capabilities */
	vfe.vfe_flags = VFS_TBLNOTYPENUM | VFS_TBL64BITREADY | VFS_TBLTHREADSAFE |
					VFS_TBLFSNODELOCK;
	error = vfs_fsadd(&vfe, &webdav_vfsconf);

	return (error ? KERN_FAILURE : KERN_SUCCESS);
}

/*****************************************************************************/

__private_extern__
kern_return_t webdav_fs_module_stop(struct kmod_info *ki, void *data)
{
	#pragma unused(ki, data)
	int error;

	if (webdav_mnt_cnt == 0)
	{
		error = vfs_fsremove(webdav_vfsconf);
		if ( error == 0 )
		{
			/* free up any memory allocated */
			webdav_hashdestroy();
			lck_rw_destroy(&ref_tbl_rwlock, webdav_rwlock_group);
			lck_grp_free(webdav_rwlock_group);
		}
	}
	else
	{
		error = EBUSY;
	}
	
	return (error ? KERN_FAILURE : KERN_SUCCESS);
}

/*****************************************************************************/
