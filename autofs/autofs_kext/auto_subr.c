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
 * Portions Copyright 2007-2011 Apple Inc.
 */

#pragma ident	"@(#)auto_subr.c	1.95	05/12/19 SMI"

#include <mach/task.h>
#include <mach/task_special_ports.h>
#include <mach/thread_act.h>
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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/kauth.h>
#include <sys/attr.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vm.h>
#include <sys/errno.h>
#include <vfs/vfs_support.h>

#include <kern/assert.h>
#include <kern/locks.h>
#include <kern/clock.h>

#include <IOKit/IOLib.h>

#ifdef DEBUG
#include <stdarg.h>
#endif

#include "autofs.h"
#include "triggers.h"
#include "triggers_priv.h"
#include "autofs_kern.h"
#include "autofs_protUser.h"

#define	TYPICALPATH_MAX	64

/*
 * List of subtriggers to be planted on a mount.
 */
typedef struct subtrigger {
	struct mounta mounta;	/* struct mounta from the action list entry for the subtrigger */
	int inplace;		/* is this subtrigger currently in place? */
	struct subtrigger *next;
} subtrigger_t;

static subtrigger_t *auto_make_subtriggers(action_list *);
static void auto_free_subtriggers(subtrigger_t *);
static void auto_trigger_callback(mount_t, vfs_trigger_callback_op_t,
     void *, vfs_context_t);
static void auto_plant_subtriggers(mount_t, subtrigger_t *, vfs_context_t);

/*
 * Parameters passed to an autofs mount thread.
 */
struct autofs_callargs {
	struct trigger_callargs fnc_t;	/* common args */
};

#define fnc_vp			fnc_t.tc_vp
#define fnc_this_fsid		fnc_t.tc_this_fsid
#define fnc_ti			fnc_t.tc_ti
#define fnc_origin		fnc_t.tc_origin

#define fnc_uid			fnc_t.tc_uid
#define fnc_asid		fnc_t.tc_asid
#define fnc_mounted_fsid	fnc_t.tc_mounted_fsid
#define fnc_retflags		fnc_t.tc_retflags

static int auto_mount_request(struct autofs_callargs *, char *, char *,
    char *, int, char *, char *, boolean_t, boolean_t, boolean_t *);

/*
 * Unless we're an automounter (in which case, the process that caused
 * us to be asked to do something already has a reader lock on the fninfo_t,
 * and some other process might be asking for a writer lock, which would
 * prevent us from getting a reader lock, and thus would prevent us from
 * finishing the mount, and thus would prevent the process that caused us
 * to be asked to mount the file system from releasing the reader lock,
 * so everybody's deadlocked), get a reader lock on the fninfo_t.
 */
void
auto_fninfo_lock_shared(fninfo_t *fnip, int pid)
{
	if (!auto_is_automounter(pid))
		lck_rw_lock_shared(fnip->fi_rwlock);
}

/*
 * Release the shared lock, unless we're an automounter, in which case
 * we don't have one.
 */
void
auto_fninfo_unlock_shared(fninfo_t *fnip, int pid)
{
	if (!auto_is_automounter(pid))
		lck_rw_unlock_shared(fnip->fi_rwlock);
}

/*
 * Checks whether a given mount_t is autofs.
 */
int
auto_is_autofs(mount_t mp)
{
	struct vfsstatfs *vfsstat;
	size_t typename_len;
	static const char autofs_typename[] = MNTTYPE_AUTOFS;

	vfsstat = vfs_statfs(mp);
	typename_len = strlen(vfsstat->f_fstypename) + 1;
	if (typename_len != sizeof autofs_typename)
		return (0);	/* no, the strings aren't even the same length */
	if (bcmp(autofs_typename, vfsstat->f_fstypename, typename_len) != 0)
		return (0);	/* same length, different contents */
	return (1);	/* same length, same contents */
}

/*
 * Greasy hack to handle home directory mounting.
 *
 * If a process is a home directory mounter, then the first unmount it does
 * of automounted file systems cause the trigger on which the mount was done
 * to be marked as "home directory mount in progress", and no fsctl
 * operations will trigger a mount on the last component of a pathname.
 *
 * We support an fsctl that also sets "home directory mount in progress"
 * on a vnode; only home directory mounter processes may use this, and
 * they may only do it once.
 *
 * The /dev/autofs_homedirmounter device holds onto the vnode for the
 * trigger in question; when the device is closed, the vnode's "home
 * directory mount in progress" state is cleared.  This means that, if
 * the home directory mounter process dies, things go back to normal
 * for that trigger.
 *
 * All attempts to trigger a mount on a vnode marked as "home directory
 * mount in progress" fail with ENOENT, so that nobody will trigger a
 * mount while the mount is in progress; this closes a race condition
 * that we've seen in 6932244/7482727.
 *
 * As the vnode is held onto, we can set its owner and it'll persist.
 */

/*
 * Called when a trigger is rearmed.
 * Mark it as having a home directory mount in progress on it if
 * appropriate.
 */
static void
auto_rearm(vnode_t vp, int pid)
{
	int error;
	fnnode_t *fnp;

	error = auto_mark_vnode_homedirmount(vp, pid);

	/*
	 * If we get EINVAL, it means we're not a home directory mounter
	 * process, in which case we want to revert the owner of the
	 * trigger back to root, so unprivileged processes can't mount
	 * on it.  (If we are a home directory mounter process, we
	 * want to leave the owner as is, so that whoever triggered
	 * the mount on the vnode can do another mount on it; they,
	 * and root, are the only people who could have done the unmount.)
	 *
	 * If we get EBUSY, it means we're a home directory mounter
	 * process that has already unmounted an automounted file
	 * system, in which case we've made a mistake; log that.
	 *
	 * If we get ENOENT, it means we're a home directory mounter
	 * process and we couldn't get a reference on the trigger;
	 * log that.
	 */
	if (error == EINVAL) {
		fnp = vntofn(vp);
		lck_mtx_lock(fnp->fn_lock);
		fnp->fn_uid = 0;
		lck_mtx_unlock(fnp->fn_lock);
	} else if (error == EBUSY)
		IOLog("auto_rearm: process %d has already unmounted an automounted file system\n",
		    pid);
	else if (error == ENOENT)
		IOLog("auto_rearm: can't get a reference on vnode %p\n", vp);
}

int
auto_lookup_request(fninfo_t *fnip, char *name, int namelen, char *subdir,
    vfs_context_t context, int *node_type, boolean_t *lu_verbose)
{
	boolean_t isdirect;
	mach_port_t automount_port;
	int error;
	kern_return_t ret;

	AUTOFS_DPRINT((4, "auto_lookup_request: path=%s name=%.*s\n",
	    fnip->fi_path, namelen, name));

	isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;

	AUTOFS_DPRINT((4, "auto_lookup_request: using key=%.*s, subdir=%s\n",
	    keylen, key, subdir));

	error = auto_get_automountd_port(&automount_port);
	if (error)
		return error;
	ret = autofs_lookup(automount_port, fnip->fi_map, fnip->fi_path,
	    name, namelen, subdir, fnip->fi_opts, isdirect,
	    kauth_cred_getuid(vfs_context_ucred(context)), &error, node_type,
	    lu_verbose);
	auto_release_port(automount_port);
	if (ret != KERN_SUCCESS) {
		if (ret == MACH_RCV_INTERRUPTED || ret == MACH_SEND_INTERRUPTED) {
			error = EINTR;	// interrupted by a signal
		} else {
			IOLog("autofs: autofs_lookup failed, status 0x%08x\n", ret);
			error = EIO;		/* XXX - process Mach errors */
		}
	}
	AUTOFS_DPRINT((5, "auto_lookup_request: path=%s name=%.*s error=%d\n",
	    fnip->fi_path, namelen, name, error));
	return error;
}

static int
get_key_and_subdirectory(struct fninfo *fnip, char *name, int namelen,
    fnnode_t *parentp, char **keyp, int *keylenp, char **subdirp,
    char *pathbuf, size_t pathbuflen)
{
	fnnode_t *rootfnp, *stopfnp = NULL;
	struct autofs_globals *fngp;
	char *subdir;

	/*
	 * XXX - can we just test fnip->fi_flags & MF_SUBTRIGGER here,
	 * so that we do the "build the subdirectory" stuff if
	 * MF_SUBTRIGGER isn't set and don't do it if it's set?
	 */
	if (fnip->fi_subdir[0] == '\0') {
		rootfnp = vntofn(fnip->fi_rootvp);
		fngp = rootfnp->fn_globals;

		/*
		 * Build the subdirectory for this fnnode.
		 */
		subdir = &pathbuf[pathbuflen];
		*--subdir = '\0';	/* trailing NUL */

		if (!(fnip->fi_flags & MF_DIRECT)) {
			/*
			 * This is an indirect map.  The key will
			 * be the name of the directory in the path
			 * right under the top-level directory of the
			 * map, and the subdirectory will be the rest
			 * of that path.
			 *
			 * That means we stop adding names to the
			 * subdirectory path when we find a directory
			 * *below* the root directory for the map,
			 * as that directory's name is the key, not
			 * part of the subdirectory.  That directory's
			 * parent is the root directory for this mount.
			 */
			stopfnp = rootfnp;
		}
		for (;;) {
			/*
			 * Don't walk up to the Root Of All Evil^W
			 * top-level autofs mounts.
			 */
			if (parentp == fngp->fng_rootfnnodep)
				break;

			/*
			 * Don't walk up to any directory where
			 * we should stop.
			 */
			if (parentp == stopfnp)
				break;

			/*
			 * Add this component to the path.
			 */
			subdir -= namelen;
			if (subdir < pathbuf)
				return (ENAMETOOLONG);
			memcpy(subdir, name, namelen);

			subdir--;
			if (subdir < pathbuf)
				return (ENAMETOOLONG);
			*subdir = '/';

			name = parentp->fn_name;
			namelen = parentp->fn_namelen;
			if (parentp->fn_parent == parentp)
				break;
			parentp = parentp->fn_parent;
		}
	} else
		subdir = fnip->fi_subdir;

	/*
	 * For direct maps, the key for an entry is the mount point
	 * for the map.  For indirect maps, it's the name of the
	 * first component in the pathname past the root of the
	 * mount, which is what we've found.
	 */
	if (fnip->fi_flags & MF_DIRECT) {
		*keyp = fnip->fi_key;
		*keylenp = fnip->fi_keylen;
	} else {
		*keyp = name;
		*keylenp = namelen;
	}
	*subdirp = subdir;
	return (0);
}

int
auto_lookup_aux(struct fninfo *fnip, fnnode_t *parentp, char *name, int namelen,
    vfs_context_t context, int *node_type)
{
	int error = 0;
	char pathbuf[MAXPATHLEN];
	char *key;
	int keylen;
	char *subdir;
	struct autofs_globals *fngp;
	boolean_t lu_verbose;

	/*
	 * If we're the automounter or a child of the automounter,
	 * don't wait for us to finish what we're doing, and
	 * don't ask ourselves to do anything - just say we succeeded.
	 */
	if (auto_is_automounter(vfs_context_pid(context)))
		return (0);

	/*
	 * Find the appropriate key and subdirectory to pass to
	 * auto_lookup_request().
	 */
	error = get_key_and_subdirectory(fnip, name, namelen, parentp,
	    &key, &keylen, &subdir, pathbuf, sizeof(pathbuf));
	if (error != 0)
		return (error);

	error = auto_lookup_request(fnip, key, keylen, subdir, context,
	    node_type, &lu_verbose);
	if (error == 0) {
		fngp = vntofn(fnip->fi_rootvp)->fn_globals;
		fngp->fng_verbose = lu_verbose;
	}

	return (error);
}

int
auto_readdir_aux(struct fninfo *fnip, fnnode_t *dirp, off_t offset,
    u_int alloc_count, int64_t *return_offset, boolean_t *return_eof,
    byte_buffer *return_buffer, mach_msg_type_number_t *return_bufcount)
{
	int error = 0;
	char pathbuf[MAXPATHLEN];
	char *key;
	int keylen;
	char *subdir;
	mach_port_t automount_port;
	boolean_t isdirect;
	kern_return_t ret;

	isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;

	error = auto_get_automountd_port(&automount_port);
	if (error)
		goto done;

	if (dirp == vntofn(fnip->fi_rootvp) && !isdirect) {
		/*
		 * This is the top-level directory of an indirect map
		 * mount, so, to read the contents of the directory,
		 * we should enumerate the map.
		 *
		 * XXX - what if this is really for a direct map
		 * with nothing mounted atop it, but with stuff
		 * mounted in directories underneath it, so it's
		 * "really" an indirect map?  That requires a
		 * subdir readdir.
		 */
		ret = autofs_readdir(automount_port, fnip->fi_map,
		    offset, alloc_count, &error,
		    return_offset, return_eof, return_buffer,
		return_bufcount);
	} else {
		/*
		 * This is a directory under a top-level directory of
		 * an indirect map, or the top-level directory of a
		 * direct map (which can have things underneath it),
		 * so we should look up the map entry for the indirect
		 * map's top-level directory or for the direct map
		 * and extract subdirectory information from it.
		 */

		/*
		 * Find the appropriate key and subdirectory to pass to
		 * autofs_readsubdir().
		 */
		error = get_key_and_subdirectory(fnip, dirp->fn_name,
		    dirp->fn_namelen, dirp->fn_parent, &key, &keylen, &subdir,
		    pathbuf, sizeof(pathbuf));
		if (error != 0) {
			auto_release_port(automount_port);
			goto done;
		}

		ret = autofs_readsubdir(automount_port, fnip->fi_map,
		    key, keylen, subdir, fnip->fi_opts,
		    (uint32_t) dirp->fn_nodeid, offset, alloc_count, &error,
		    return_offset, return_eof, return_buffer,
		return_bufcount);
	}

	auto_release_port(automount_port);
	if (ret != KERN_SUCCESS) {
		if (ret == MACH_RCV_INTERRUPTED || ret == MACH_SEND_INTERRUPTED) {
			error = EINTR;	// interrupted by a signal
		} else {
			IOLog("autofs: autofs_readdir failed, status 0x%08x\n", ret);
			error = EIO;
		}
	}

done:
	AUTOFS_DPRINT((5, "auto_readdir_aux: path=%s name=%.*s subdir=%s error=%d\n",
	    fnip->fi_path, keylen, key, subdir, error));
	return (error);
}

static int
auto_check_homedirmount(vnode_t vp)
{
	fnnode_t *fnp = vntofn(vp);

	return (fnp->fn_flags & MF_HOMEDIRMOUNT);
}

/*
 * For doing mounts atop an autofs node that's a trigger.
 */
static void *
autofs_trigger_get_mount_args(__unused vnode_t vp, vfs_context_t ctx, int *errp)
{
	struct autofs_callargs *argsp;

	/*
	 * Allocate the args structure.
	 */
	MALLOC(argsp, struct autofs_callargs *, sizeof (*argsp), M_AUTOFS,
	    M_WAITOK);

	/*
	 * Get the UID for the process that triggered the mount, so
	 * we do the mount as that user.
	 */
	argsp->fnc_uid = kauth_cred_getuid(vfs_context_ucred(ctx));

	*errp = 0;
	return (argsp);
}

/*
 * Starting point for thread to handle mount requests with automountd.
 */
static int
auto_do_mount(void *arg)
{
	struct autofs_callargs *argsp = arg;
	vnode_t vp;
	fnnode_t *fnp;
	struct fninfo *fnip;
	char *key;
	int keylen;
	boolean_t isdirect, issubtrigger;
	boolean_t mr_verbose;
	int error;
	char pathbuf[MAXPATHLEN];
	char *subdir;
	struct autofs_globals *fngp;

	vp = argsp->fnc_vp;
	fnp = vntofn(vp);
	fnip = vfstofni(vnode_mount(vp));

	/*
	 * This is in a kernel thread, so the PID is 0.
	 */
	auto_fninfo_lock_shared(fnip, 0);

	/*
	 * Is this in the process of being unmounted?  If so, give
	 * up, so that we aren't holding an iocount or usecount on
	 * the vnode, and the unmount can finish.
	 */
	if (fnip->fi_flags & MF_UNMOUNTING) {
		auto_fninfo_unlock_shared(fnip, 0);
		return (ENOENT);
	}

	/*
	 * Find the appropriate key and subdirectory to pass to
	 * auto_mount_request().
	 */
	error = get_key_and_subdirectory(fnip, fnp->fn_name,
	    fnp->fn_namelen, fnp->fn_parent, &key, &keylen,
	    &subdir, pathbuf, sizeof(pathbuf));
	if (error != 0) {
		auto_fninfo_unlock_shared(fnip, 0);
		return (error);
	}

	/*
	 * Set the UID of the mount point to the UID of the process on
	 * whose behalf we're doing the mount; the mount might have to
	 * be done as that user if it requires authentication as that
	 * user.
	 */
	lck_mtx_lock(fnp->fn_lock);
	fnp->fn_uid = argsp->fnc_uid;
	lck_mtx_unlock(fnp->fn_lock);

	isdirect = (fnip->fi_flags & MF_DIRECT) ? TRUE : FALSE;
	issubtrigger = (fnip->fi_flags & MF_SUBTRIGGER) ? TRUE : FALSE;

	error = auto_mount_request(argsp, fnip->fi_map, fnip->fi_path,
	    key, keylen, subdir, fnip->fi_opts, isdirect, issubtrigger,
	    &mr_verbose);
	if (!error) {
		/*
		 * Change setting of "verbose" flag; references to
		 * non-existent names in an autofs file system with
		 * "=v" at the beginning turn verbosity on.
		 */
		fngp = vntofn(fnip->fi_rootvp)->fn_globals;
		fngp->fng_verbose = mr_verbose;
	}

	if (error != 0) {
		/*
		 * Revert the ownership of the mount point.
		 */
		lck_mtx_lock(fnp->fn_lock);
		fnp->fn_uid = 0;
		lck_mtx_unlock(fnp->fn_lock);
	}
	auto_fninfo_unlock_shared(fnip, 0);

	return (error);
}

static void
autofs_trigger_rel_mount_args(void *data)
{
	struct autofs_callargs *argsp = data;

	FREE(argsp, M_AUTOFS);
}

/*
 * Starting point for thread to handle submount requests with
 * automountd.  This is *not* mounting atop an autofs node,
 * it's mounting atop a node for some file system mounted by
 * autofs.
 */
static int
auto_do_submount(void *arg)
{
	struct autofs_callargs *argsp = arg;
	subtrigger_t *subtrigger = argsp->fnc_ti->ti_private;
	struct mounta *m = &subtrigger->mounta;
	char *key;
	int keylen;
	int error;
	boolean_t mr_verbose;

	key = m->key;
	if (key != NULL)
		keylen = (int)strlen(key);
	else {
		/*
		 * automountd handed us a null string; presumably
		 * that means the key is irrelevant, so use a
		 * null string.
		 */
		key = "";
		keylen = 0;
	}
	error = auto_mount_request(argsp, m->map, m->path, key, keylen,
	    m->subdir, m->opts, TRUE, TRUE, &mr_verbose);

	return (error);
}

/*
 * For automounting an autofs trigger for a submount; see the comment in
 * auto_plant_subtriggers() for the reason why we sometimes add that
 * extra level of indirection.
 */
static int
auto_mount_subtrigger_request(
	char *mntpt,
	char *submntpt,
	char *path,
	char *opts,
	char *map,
	char *subdir,
	char *key,
	uint32_t flags,
	uint32_t mntflags,
	int32_t direct,
	fsid_t *fsidp)
{
	int error;
	mach_port_t automount_port;
	kern_return_t ret;
	boolean_t top_level;

	error = auto_get_automountd_port(&automount_port);
	if (error)
		return (error);
	ret = autofs_mount_subtrigger(automount_port, mntpt, submntpt, path,
	    opts, map, subdir, key, flags, mntflags, direct, fsidp,
	    &top_level, &error);
	auto_release_port(automount_port);
	if (ret != KERN_SUCCESS) {
		if (ret == MACH_RCV_INTERRUPTED || ret == MACH_SEND_INTERRUPTED) {
			error = EINTR;	// interrupted by a signal
		} else {
			IOLog("autofs: autofs_mount_subtrigger failed, status 0x%08x\n",
			  ret);
			error = EIO;		/* XXX - process Mach errors */
		}
	}
	return (error);
}

/*
 * For doing mounts atop an autofs node that's a trigger.
 */
static void *
autofs_subtrigger_get_mount_args(__unused vnode_t vp,
    __unused vfs_context_t ctx, int *errp)
{
	struct trigger_callargs *argsp;

	/*
	 * Allocate the args structure.
	 */
	MALLOC(argsp, struct trigger_callargs *, sizeof (*argsp), M_AUTOFS,
	    M_WAITOK);
	*errp = 0;
	return (argsp);
}

/*
 * Starting point for thread to handle subtrigger mount requests with
 * automountd.
 */
static int
auto_do_subtrigger_mount(void *arg)
{
	struct trigger_callargs *tc = arg;
	struct mounta *m = tc->tc_ti->ti_private;
	char *mntpnt;
	int error;

	if (m->dir[0] == '.' && m->dir[1] == '\0') {
		/*
		 * mounting on the trigger node
		 */
		mntpnt = ".";
	} else {
		/*
		 * ignore "./" in front of mountpoint
		 */
		mntpnt = m->dir + 2;
	}

	error = auto_mount_subtrigger_request(m->path, mntpnt, m->path,
	    m->opts, m->map, m->subdir, m->key, m->flags, m->mntflags,
	    m->isdirect, &tc->tc_mounted_fsid);
	tc->tc_retflags = FALSE;	/* we mounted an autofs file system; it's not volfs or NFS, hard or otherwise */

	return (error);
}

static void
autofs_subtrigger_rel_mount_args(void *data)
{
	struct trigger_callargs *argsp = data;

	FREE(argsp, M_AUTOFS);
}

static void
auto_subtrigger_release(void *data)
{
	subtrigger_t *subtrigger = data;

	/*
	 * This is only called for external triggers.  If the trigger info
	 * structure pointing to us is being released, it means that the
	 * vnode with the external trigger in question is being released,
	 * and so the external trigger is going away and would need to
	 * be replanted.
	 */
	subtrigger->inplace = 0;
}

static int
getstring(char **strp, uint8_t **inbufp, mach_msg_type_number_t *bytes_leftp)
{
	uint32_t stringlen;

	if (*bytes_leftp < sizeof (uint32_t)) {
		IOLog("Action list too short for string length");
		return (EIO);
	}
	memcpy(&stringlen, *inbufp, sizeof (uint32_t));
	*inbufp += sizeof (uint32_t);
	*bytes_leftp -= (mach_msg_type_number_t)sizeof (uint32_t);
	if (stringlen == 0xFFFFFFFF) {
		/* Null pointer */
		*strp = NULL;
	} else {
		if (*bytes_leftp < stringlen) {
			IOLog("Action list too short for string data");
			return (EIO);
		}
		MALLOC(*strp, char *, stringlen + 1, M_AUTOFS, M_WAITOK);
		if (*strp == NULL) {
			IOLog("No space for string data in action list");
			return (ENOMEM);
		}
		memcpy(*strp, *inbufp, stringlen);
		(*strp)[stringlen] = '\0';
		*inbufp += stringlen;
		*bytes_leftp -= stringlen;
	}
	return (0);
}

static int
getint(int *intp, uint8_t **inbufp, mach_msg_type_number_t *bytes_leftp)
{
	if (*bytes_leftp < sizeof (int)) {
		IOLog("Action list too short for int");
		return (EIO);
	}
	memcpy(intp, *inbufp, sizeof (int));
	*inbufp += sizeof (int);
	*bytes_leftp -= (mach_msg_type_number_t)sizeof (int);
	return (0);
}

static int
getuint32(uint32_t *uintp, uint8_t **inbufp, mach_msg_type_number_t *bytes_leftp)
{
	if (*bytes_leftp < sizeof (uint32_t)) {
		IOLog("Action list too short for uint32_t");
		return (EIO);
	}
	memcpy(uintp, *inbufp, sizeof (uint32_t));
	*inbufp += sizeof (uint32_t);
	*bytes_leftp -= (mach_msg_type_number_t)sizeof (uint32_t);
	return (0);
}

/*
 * Free the strings pointed to by a struct mounta.
 */
static void
free_mounta_strings(struct mounta *m)
{
	if (m->dir != NULL)
		FREE(m->dir, M_AUTOFS);
	if (m->opts != NULL)
		FREE(m->opts, M_AUTOFS);
	if (m->path != NULL)
		FREE(m->path, M_AUTOFS);
	if (m->map != NULL)
		FREE(m->map, M_AUTOFS);
	if (m->subdir != NULL)
		FREE(m->subdir, M_AUTOFS);
	if (m->trig_mntpnt != NULL)
		FREE(m->trig_mntpnt, M_AUTOFS);
	if (m->key != NULL)
		FREE(m->key, M_AUTOFS);
}

static void
free_action_list(action_list *alp)
{
	action_list *action, *next_action;

	for (action = alp; action != NULL; action = next_action) {
		next_action = action->next;
		free_mounta_strings(&action->mounta);
		FREE(action, M_AUTOFS);
	}
}

static int
auto_mount_request(
	struct autofs_callargs *argsp,
	char *map,
	char *path,
	char *key,
	int keylen,
	char *subdir,
	char *opts,
	boolean_t isdirect,
	boolean_t issubtrigger,
	boolean_t *mr_verbosep)
{
	mach_port_t automount_port;
	int error;
	int mr_type;
	kern_return_t ret;
	byte_buffer actions_buffer;
	mach_msg_type_number_t actions_bufcount;
	vm_map_offset_t map_data;
	vm_offset_t data;
	uint8_t *inbuf;
	mach_msg_type_number_t bytes_left;
	action_list *alphead, *alp, *prevalp;
	subtrigger_t *subtriggers;

	AUTOFS_DPRINT((4, "auto_mount_request: path=%s key=%.*s\n",
	    path, keylen, key));

	alphead = NULL;
	error = auto_get_automountd_port(&automount_port);
	if (error) {
		goto done;
	}
	ret = autofs_mount(automount_port, map, path, key, keylen, subdir,
	    opts, isdirect, issubtrigger, argsp->fnc_this_fsid,
	    argsp->fnc_uid, argsp->fnc_asid, &mr_type,
	    &argsp->fnc_mounted_fsid, &argsp->fnc_retflags,
	    &actions_buffer, &actions_bufcount, &error, mr_verbosep);
	auto_release_port(automount_port);
	if (ret == KERN_SUCCESS) {
		switch (mr_type) {
		case AUTOFS_ACTION:
			error = 0;

			/*
			 * Get the action list.
			 */
			ret = vm_map_copyout(kernel_map, &map_data,
			    (vm_map_copy_t)actions_buffer);
			if (ret != KERN_SUCCESS) {
				if (ret == MACH_RCV_INTERRUPTED || ret == MACH_SEND_INTERRUPTED) {
					error = EINTR;	// interrupted by a signal
				} else {
					/* XXX - deal with Mach errors */
					IOLog("autofs: vm_map_copyout failed, status 0x%08x\n", ret);
					error = EIO;
				}
				goto done;
			}
			data = CAST_DOWN(vm_offset_t, map_data);

			/*
			 * Deserialize the action list.
			 */
			prevalp = NULL;
			inbuf = (uint8_t *)data;
			bytes_left = actions_bufcount;
			while (bytes_left != 0) {
				MALLOC(alp, action_list *, sizeof(*alp),
				    M_AUTOFS, M_WAITOK);
				if (prevalp == NULL)
					alphead = alp;
				else
					prevalp->next = alp;
				bzero(alp, sizeof *alp);
				error = getstring(&alp->mounta.dir, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getstring(&alp->mounta.opts, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getstring(&alp->mounta.path, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getstring(&alp->mounta.map, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getstring(&alp->mounta.subdir, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getstring(&alp->mounta.trig_mntpnt, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getint(&alp->mounta.flags, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getint(&alp->mounta.mntflags, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getuint32(&alp->mounta.isdirect, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getuint32(&alp->mounta.needs_subtrigger, &inbuf,
				    &bytes_left);
				if (error)
					break;
				error = getstring(&alp->mounta.key, &inbuf,
				    &bytes_left);
				if (error)
					break;
				prevalp = alp;
			}
			vm_deallocate(kernel_map, data, actions_bufcount);
			if (error) {
				free_action_list(alphead);
				alphead = NULL;
				goto done;
			}

			/*
			 * If there are any submounts to be lazily done
			 * atop what was mounted, set the callback for
			 * planting triggers for those submounts.  That
			 * will cause the callback to be called in order
			 * to plant the triggers for the first time.
			 */
			if (alphead != NULL) {
				/*
				 * XXX - sanity-check the actions?
				 */
				subtriggers = auto_make_subtriggers(alphead);

				/*
				 * Set the trigger callback for this mount,
				 * with the subtrigger list as the data
				 * to pass to it.  This will cause the
				 * callback to be called with VTC_REPLACE,
				 * to plant the triggers for the first
				 * time; the file system cannot be
				 * unmounted while that is in progress,
				 * so we will not have two threads in
				 * the callback on the same file system
				 * at the same time.
				 */
				error = vfs_settriggercallback(&argsp->fnc_mounted_fsid,
				    auto_trigger_callback, subtriggers, 0,
				    vfs_context_current());
				if (error == EBUSY) {
					/*
					 * This probably means it's getting
					 * unmounted out from under us.
					 * Free the subtrigger list, and
					 * treat that as ENOENT from the
					 * mount.
					 *
					 * (We'd get ENOENT if it had already
					 * been unmounted by the time we
					 * called vfs_settriggercallback().)
					 */
					auto_free_subtriggers(subtriggers);
					error = ENOENT;
				}
			}
			break;
		case AUTOFS_DONE:
			break;
		default:
			error = ENOENT;
			IOLog("auto_mount_request: unknown status %d\n",
			    mr_type);
			break;
		}
	} else {
		if (ret == MACH_RCV_INTERRUPTED || ret == MACH_SEND_INTERRUPTED) {
			error = EINTR;	// interrupted by a signal
		} else {
			IOLog("autofs: autofs_mount failed, status 0x%08x\n", ret);
			error = EIO;		/* XXX - process Mach errors */
		}
	}

done:
	AUTOFS_DPRINT((5, "auto_mount_request: path=%s key=%.*s error=%d\n",
	    path, keylen, key, error));
	return (error);
}

#if 0
/*
 * XXX - Solaris passes a bunch of other crap to automountd;
 * see the umntrequest structure.
 * Is any of that crap needed?
 * Yes - the file system type is used, as it special-cases
 * NFS.  For NFS, it also needs the port number, so it can
 * ping the server before trying to unmount it, so it doesn't
 * get stuck unmounting from an unresponsive server (I'm sure
 * having the mountd keep track of what stuff was mounted from
 * clients sounded like a good idea at the time; in retrospect,
 * it wasn't).  It also uses the "public" option and the
 * mount resource.
 */
static int
auto_send_unmount_request(
	fsid_t fsid,
	char *mntresource,
	char *mntpnt,
	char *fstype,
	char *mntopts)
{
	int error;
	mach_port_t automount_port;
	int status;
	kern_return_t ret;

	AUTOFS_DPRINT((4, "\tauto_send_unmount_request: fstype=%s "
			" mntpnt=%s\n", fstype,	mntpnt));
	error = auto_get_automountd_port(&automount_port);
	if (error)
		goto done;
	ret = autofs_unmount(automount_port, fsid.val[0], fsid.val[1],
	    mntresource, mntpnt, fstype, mntopts, &status);
	auto_release_port(automount_port);
	if (ret == KERN_SUCCESS)
		error = status;
	else {
		if (ret == MACH_RCV_INTERRUPTED || ret == MACH_SEND_INTERRUPTED) {
			error = EINTR;	// interrupted by a signal
		} else {
			IOLog("autofs: autofs_unmount failed, status 0x%08x\n", ret);
			error = EIO;		/* XXX - process Mach errors */
		}
	}

done:
	AUTOFS_DPRINT((5, "\tauto_send_unmount_request: error=%d\n", error));

	return (error);
}
#endif

/*
 * Create a subtrigger list from an action list.  Also frees the
 * action list entries in the process (but not the strings to
 * which they point, as those are now pointed to by the elements
 * in the subtrigger list.
 */
static subtrigger_t *
auto_make_subtriggers(action_list *alp)
{
	action_list *p, *pnext;
	subtrigger_t *subtriggers = NULL, *subtrigger, *prev_subtrigger = NULL;

	for (p = alp; p != NULL; p = pnext) {
		pnext = p->next;
		MALLOC(subtrigger, subtrigger_t *, sizeof(subtrigger_t),
		    M_AUTOFS, M_WAITOK);
		subtrigger->mounta = p->mounta;	/* copies pointers */
		subtrigger->inplace = 0;	/* not planted yet */
		subtrigger->next = NULL;	/* end of the list, so far */
		if (prev_subtrigger == NULL) {
			/* First subtrigger - set the list head */
			subtriggers = subtrigger;
		} else {
			/* Not the first - append to the list */
			prev_subtrigger->next = subtrigger;
		}
		prev_subtrigger = subtrigger;
		FREE(p, M_AUTOFS);
	}
	return (subtriggers);
}

/*
 * Free a subtrigger list.
 */
static void
auto_free_subtriggers(subtrigger_t *subtriggers)
{
	subtrigger_t *subtrigger, *next_subtrigger;

	for (subtrigger = subtriggers; subtrigger != NULL;
	    subtrigger = next_subtrigger) {
		next_subtrigger = subtrigger->next;
		free_mounta_strings(&subtrigger->mounta);
		FREE(subtrigger, M_AUTOFS);
	}
}

static void
auto_trigger_callback(mount_t mp, vfs_trigger_callback_op_t op, void *data,
    vfs_context_t ctx)
{
	subtrigger_t *subtriggers = data;

	switch (op) {

	case VTC_RELEASE:
		/*
		 * Release the subtrigger list.
		 */
		auto_free_subtriggers(subtriggers);
		break;

	case VTC_REPLACE:
		/*
		 * Plant the triggers.
		 */
		auto_plant_subtriggers(mp, subtriggers, ctx);
		break;
	}
}

static void
auto_plant_subtriggers(mount_t mp, subtrigger_t *subtriggers, vfs_context_t ctx)
{
	vnode_t dvp;
	subtrigger_t *subtrigger;
	struct mounta *m;
	int error;
	struct vnode_trigger_param vnt;
	struct vnode_trigger_info vti;
	trigger_info_t *ti;

	AUTOFS_DPRINT((4, "auto_plant_subtriggers: subtriggers=%p\n", (void *)subtriggers));

	/*
	 * XXX - this will hold an iocount on dvp; will that block any
	 * unmounts that might block us from finishing?
	 *
	 * Or do we already hold an iocount on it because it's the
	 * trigger we're resolving?
	 */
	dvp = vfs_vnodecovered(mp);

	for (subtrigger = subtriggers; subtrigger != NULL;
	    subtrigger = subtrigger->next) {
		/*
		 * Is this subtrigger already (still) in place?  If so,
		 * skip it.
		 */
		if (subtrigger->inplace)
			continue;

		m = &subtrigger->mounta;

		AUTOFS_DPRINT((10, "\tsubtrigger on %s/%s\n",
		    vfs_statfs(mp)->f_mntonname, m->trig_mntpnt));

		/*
		 * Tell the VFS layer the directory at mntpnt
		 * should be a trigger.
		 */
		if (m->needs_subtrigger) {
			/*
			 * The trigger causes an autofs subtrigger
			 * mount to be done, and the root of that
			 * submount is a trigger for the real
			 * file system.
			 *
			 * We do that for file systems other than
			 * NFS and autofs, so that we can do the
			 * mount as the user, and can do the
			 * home directory mount dance for it.
			 *
			 * XXX - this is necessary only if
			 * we can't do that with just a trigger;
			 * we might have to be able to do that
			 * with just a trigger for Dfs mounts.
			 */
			ti = trigger_new_autofs(&vnt, TF_AUTOFS,
			    auto_is_notrigger_process,
			    auto_is_nowait_process,
			    auto_is_homedirmounter_process,
			    NULL,
			    autofs_subtrigger_get_mount_args,
			    auto_do_subtrigger_mount,
			    autofs_subtrigger_rel_mount_args,
			    NULL,
			    auto_subtrigger_release, subtrigger);
		} else {
			/*
			 * The trigger will directly cause the
			 * mount to be done.
			 *
			 * We do that for NFS and autofs, because
			 * NFS mounts are done by a process running
			 * with root privilege (mount_nfs is set-UID
			 * root, so that it can get a privileged
			 * port if necessary, and NFS mounts are
			 * multi-user so they shouldn't appear to
			 * have been done by the user who happened
			 * to trigger the mount in any case), and
			 * autofs mounts are done directly in
			 * automountd, which runs as root.  NFS
			 * home directories don't get remounted
			 * at login (as they're multi-user mounts),
			 * and the whole notion of a home directory
			 * on a pseudo-file-system such as autofs
			 * is nonsensical, so we don't need to
			 * worry about the home directory mount
			 * dance for NFS or autofs.
			 *
			 * The whole two-level trigger thing is a
			 * kludge, so we don't do it if we don't
			 * have to.
			 */
			ti = trigger_new_autofs(&vnt, 0,
			    auto_is_notrigger_process,
			    auto_is_nowait_process,
			    auto_is_homedirmounter_process,
			    NULL,
			    autofs_trigger_get_mount_args,
			    auto_do_submount,
			    autofs_trigger_rel_mount_args,
			    NULL,
			    auto_subtrigger_release, subtrigger);
		}
		vti.vti_resolve_func = vnt.vnt_resolve_func;
		vti.vti_unresolve_func = vnt.vnt_unresolve_func;
		vti.vti_rearm_func = vnt.vnt_rearm_func;
		vti.vti_reclaim_func = vnt.vnt_reclaim_func;
		vti.vti_data = vnt.vnt_data;
		vti.vti_flags = vnt.vnt_flags;

		error = vfs_addtrigger(mp, m->trig_mntpnt, &vti, ctx);
		if (error != 0) {
			trigger_free(ti);
			IOLog(
			    "autofs: vfs_addtrigger on %s/%s failed error=%d\n",
			    vfs_statfs(mp)->f_mntonname, m->trig_mntpnt, error);
			continue;
		}

		/*
		 * This trigger is in place now.
		 */
		subtrigger->inplace = 1;
	}

	/*
	 * Release the iocount we got above.
	 */
	vnode_put(dvp);

	AUTOFS_DPRINT((5, "auto_plant_subtriggers: error=%d\n", error));
}

int
auto_makefnnode(
	fnnode_t **fnpp,
	int node_type,
	mount_t mp,
	struct componentname *cnp,
	const char *name,
	vnode_t parent,
	int markroot,
	struct autofs_globals *fngp)
{
	int namelen;
	fnnode_t *fnp;
	struct fninfo *fnip;
	errno_t error;
	struct vnode_trigger_param vnt;
	vnode_t vp;
	char *tmpname;
	struct timeval now;
	/*
	 * autofs uses odd inode numbers
	 * automountd uses even inode numbers
	 */
	static ino_t nodeid = 3;
#ifdef DEBUG
	lck_attr_t *lckattr;
#endif

	fnip = vfstofni(mp);

	if (cnp != NULL) {
		name = cnp->cn_nameptr;
		namelen = cnp->cn_namelen;
	} else
		namelen = (int)strlen(name);

	MALLOC(fnp, fnnode_t *, sizeof(fnnode_t), M_AUTOFS, M_WAITOK);
	bzero(fnp, sizeof(*fnp));
	fnp->fn_namelen = namelen;
	MALLOC(tmpname, char *, fnp->fn_namelen + 1, M_AUTOFS, M_WAITOK);
	bcopy(name, tmpname, namelen);
	tmpname[namelen] = '\0';
	fnp->fn_name = tmpname;
	/*
	 * ".." is added in auto_enter and auto_mount.
	 * "." is added in auto_mkdir and auto_mount.
	 */
	/*
	 * Note that fn_size and fn_linkcnt are already 0 since
	 * we zeroed out *fnp
	 */
	fnp->fn_mode = AUTOFS_MODE;
	microtime(&now);
	fnp->fn_crtime = fnp->fn_atime = fnp->fn_mtime = fnp->fn_ctime = now;
	lck_mtx_lock(autofs_nodeid_lock);
	/* XXX - does this need to be 2 for the root vnode? */
	fnp->fn_nodeid = nodeid;
	nodeid += 2;
	fnp->fn_globals = fngp;
	fngp->fng_fnnode_count++;
	lck_mtx_unlock(autofs_nodeid_lock);

	bzero(&vnt, sizeof(struct vnode_trigger_param));
	vnt.vnt_params.vnfs_mp = mp;
	vnt.vnt_params.vnfs_vtype = (node_type & NT_SYMLINK) ? VLNK : VDIR;
	vnt.vnt_params.vnfs_str = MNTTYPE_AUTOFS;
	vnt.vnt_params.vnfs_dvp = parent;
	vnt.vnt_params.vnfs_fsnode = fnp;
	vnt.vnt_params.vnfs_vops = autofs_vnodeop_p;
	vnt.vnt_params.vnfs_markroot = markroot;
	vnt.vnt_params.vnfs_marksystem = 0;
	vnt.vnt_params.vnfs_rdev = 0;
	vnt.vnt_params.vnfs_filesize = 0;
	vnt.vnt_params.vnfs_cnp = cnp;
	vnt.vnt_params.vnfs_flags = VNFS_NOCACHE | VNFS_CANTCACHE;

	if (node_type & NT_TRIGGER) {
		/*
		 * Home directory mounter processes should not trigger
		 * mounts on fsctl, so they can do the fsctl that
		 * marks a trigger as having a home directory mount
		 * in progress.
		 */
		fnp->fn_trigger_info = trigger_new_autofs(&vnt, 0,
		    auto_is_notrigger_process,
		    auto_is_nowait_process,
		    auto_is_homedirmounter_process,
		    auto_check_homedirmount,
		    autofs_trigger_get_mount_args,
		    auto_do_mount,
		    autofs_trigger_rel_mount_args,
		    auto_rearm,
		    NULL, NULL);
		if (node_type & NT_FORCEMOUNT)
			fnp->fn_trigger_info->ti_flags |= TF_FORCEMOUNT;
		error = vnode_create(VNCREATE_TRIGGER, VNCREATE_TRIGGER_SIZE,
		    &vnt, &vp);
	} else {
		error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE,
		    &vnt.vnt_params, &vp);
	}
	if (error != 0) {
		AUTOFS_DPRINT((5, "auto_makefnnode failed with vnode_create error code %d\n", error));
		if (fnp->fn_trigger_info != NULL)
			trigger_free(fnp->fn_trigger_info);
		FREE(fnp->fn_name, M_TEMP);
		FREE(fnp, M_TEMP);
		return error;
	}

	if (node_type & NT_SYMLINK) {
		char *tmp;

		/*
		 * All autofs symlinks are links to "/".
		 */
		MALLOC(tmp, char *, 1, M_AUTOFS, M_WAITOK);
		bcopy("/", tmp, 1);
		fnp->fn_symlink = tmp;
		fnp->fn_symlinklen = 1;
	}

	fnp->fn_vnode = vp;
	fnp->fn_vid = vnode_vid(vp);

	/*
	 * Solaris has only one reference count on a vnode; when
	 * the count goes to zero, the vnode is inactivated.
	 *
	 * OS X has two reference counts, the iocount, for short-term
	 * holds within a system call, and the usecount, for long-term
	 * holds in another data structure.
	 *
	 * Releasing an iocount doesn't cause a check to be done for the
	 * reference counts going to zero, so that's not sufficient to
	 * get a vnode inactivated and recycled.  autofs expects vnodes
	 * to be inactivated as soon as possible; only vnodes that are
	 * mounted on or are otherwise being held onto (for example,
	 * because they are open or are a process's current or root
	 * directory) should stick around.  In particular, if you
	 * do a stat() on a directory, and the directory isn't open
	 * and isn't a current or root directory for any process, its
	 * vnode should be invalidated and recycled once the stat() is
	 * finished.
	 *
	 * To force this to happen, we add a usecount and then drop it.
	 * The vnode currently has no usecount and nobody yet has a
	 * pointer to it, so the usecount is zero; that means the
	 * usecount will go to 1 and then drop to zero.  That will
	 * set the VL_NEEDINACTIVE flag (as it has a non-zero iocount),
	 * causing the vnode to be inactivated when its iocount drops
	 * to zero.  Note that vnode_ref() can fail; if it does, we
	 * just don't do vnode_rele(), as that'll drive the usecount
	 * negative, and you get a "usecount -ve" crash.
	 *
	 * If we could just call vnode_setneedinactive(), we would, as
	 * it does all that we really want done and doesn't do any of
	 * the other stuff we don't care about, and it can't fail.
	 * However, we can't call vnode_setneedinactive(), as it's not
	 * exported from the kernel.
	 */
	if (vnode_ref(vp) == 0)
		vnode_rele(vp);

#ifdef DEBUG
	/*
	 * Enable debugging on these locks.
	 */
	lckattr = lck_attr_alloc_init();
	lck_attr_setdebug(lckattr);
	fnp->fn_lock = lck_mtx_alloc_init(autofs_lck_grp, lckattr);
	fnp->fn_rwlock = lck_rw_alloc_init(autofs_lck_grp, lckattr);
	lck_attr_free(lckattr);
#else
	fnp->fn_lock = lck_mtx_alloc_init(autofs_lck_grp, NULL);
	fnp->fn_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
#endif
	*fnpp = fnp;

	return (0);
}


void
auto_freefnnode(fnnode_t *fnp)
{
	AUTOFS_DPRINT((4, "auto_freefnnode: fnp=%p\n", (void *)fnp));

	assert(fnp->fn_linkcnt == 0);
	assert(!vnode_isinuse(vp, 1));
	assert(!vnode_isdir(vp) || fnp->fn_dirents == NULL);
	assert(fnp->fn_parent == NULL);

	FREE(fnp->fn_name, M_AUTOFS);
	if (fnp->fn_symlink != NULL)
		FREE(fnp->fn_symlink, M_AUTOFS);
	lck_mtx_free(fnp->fn_lock, autofs_lck_grp);
	lck_rw_free(fnp->fn_rwlock, autofs_lck_grp);

	lck_mtx_lock(autofs_nodeid_lock);
	fnp->fn_globals->fng_fnnode_count--;
	lck_mtx_unlock(autofs_nodeid_lock);
	FREE(fnp, M_AUTOFS);
}

/*
 * Remove the entry for *fnp from the list of directory entries of *dfnp.
 * Must be called with a write lock on *dfnp; it drops the write lock.
 */
void
auto_disconnect(
	fnnode_t *dfnp,
	fnnode_t *fnp)
{
	fnnode_t *tmp, **fnpp;
	vnode_t vp = fntovn(fnp);
	int isdir = vnode_isdir(vp);
	vnode_t dvp;
	struct vnode_attr vattr;

	AUTOFS_DPRINT((4,
	    "auto_disconnect: dfnp=%p fnp=%p linkcnt=%d\n",
	    (void *)dfnp, (void *)fnp, fnp->fn_linkcnt));

	assert(lck_rw_held_exclusive(dfnp->fn_rwlock));
	assert(fnp->fn_linkcnt == 1);

	/*
	 * Decrement by 1 because we're removing the entry in dfnp.
	 */
	fnp->fn_linkcnt--;

	/*
	 * only changed while holding parent's (dfnp) rw_lock
	 */
	fnp->fn_parent = NULL;

	/*
	 * Remove the entry for this vnode from its parent directory.
	 */
	fnpp = &dfnp->fn_dirents;
	for (;;) {
		tmp = *fnpp;
		if (tmp == NULL) {
			panic(
			    "auto_disconnect: %p not in %p dirent list",
			    (void *)fnp, (void *)dfnp);
		}
		if (tmp == fnp) {
			*fnpp = tmp->fn_next;	/* remove it from the list */
			assert(!vnode_isinuse(vp, 1));
			if (isdir) {
				/*
				 * Vnode being disconnected was a directory,
				 * so it had a ".." pointer to its parent;
				 * that's going away, so there's one less
				 * link to the parent, i.e. to this directory.
				 */
				dfnp->fn_linkcnt--;
			}

			/*
			 * One less entry in this directory.
			 */
			dfnp->fn_direntcnt--;
			break;
		}
		fnpp = &tmp->fn_next;
	}

	/*
	 * If the directory from which we removed this is one on which
	 * a readdir will only return names corresponding to the vnodes
	 * we have for it, and somebody cares whether something was
	 * removed from it, notify them.
	 */
	dvp = fntovn(dfnp);
	if (vnode_ismonitored(dvp) && auto_nobrowse(dvp)) {
		vfs_get_notify_attributes(&vattr);
		auto_get_attributes(dvp, &vattr);
		vnode_notify(dvp, VNODE_EVENT_WRITE, &vattr);
	}

	/*
	 * Drop the write lock on the parent.
	 */
	lck_rw_unlock_exclusive(dfnp->fn_rwlock);

	/*
	 * Drop the usecount we held on the parent.
	 * We do this after all use of dfnp, as dropping the usecount
	 * could cause the parent to be reclaimed.
	 */
	vnode_rele(fntovn(dfnp));

	AUTOFS_DPRINT((5, "auto_disconnect: done\n"));
}

/*
 * Add an entry to a directory.
 */
int
auto_enter(fnnode_t *dfnp, struct componentname *cnp, fnnode_t **fnpp)
{
	struct fnnode *cfnp, **spp = NULL;
	vnode_t vp, cvp;
	uint32_t cvid;
	off_t offset = 0;
	off_t diff;
	errno_t error;

	AUTOFS_DPRINT((4, "auto_enter: dfnp=%p, name=%.*s ", (void *)dfnp,
	    cnp->cn_namelen, cnp->cn_nameptr));

	lck_rw_lock_exclusive(dfnp->fn_rwlock);

	cfnp = dfnp->fn_dirents;
	if (cfnp == NULL) {
		/*
		 * Parent directory is empty, so this is the first
		 * entry.
		 *
		 * The offset for the "." entry is 0, and the offset
		 * for the ".." entry is 1, so the offset for this
		 * entry is 2.
		 */
		spp = &dfnp->fn_dirents;
		offset = 2;
	}

	/*
	 * See if there's already an entry with this name.
	 */
	for (; cfnp; cfnp = cfnp->fn_next) {
		if (cfnp->fn_namelen == cnp->cn_namelen &&
		    bcmp(cfnp->fn_name, cnp->cn_nameptr, cnp->cn_namelen) == 0) {
			/*
			 * There is, and this is it.
			 *
			 * Put and recycle the vnode for the fnnode we
			 * were handed, drop the write lock so that we
			 * don't block reclaims, do a vnode_getwithvid()
			 * for the fnnode we found and, if that succeeded,
			 * return EEXIST to indicate that we found and got
			 * that fnnode, otherwise return the error we
			 * got.
			 *
			 * We fetch the vnode from the fnnode we were
			 * handed before dropping its iocount, because
			 * dropping the iocount could cause it to be
			 * reclaimed, thus invalidating the fnnode at
			 * *fnpp.
			 *
			 * We fetch the vnode and its vid from the fnnode
			 * we found before dropping the write lock, as,
			 * when we drop that lock, the vnode might be
			 * reclaimed, freeing the fnnode.
			 */
			vp = fntovn(*fnpp);
			vnode_put(vp);
			vnode_recycle(vp);
			cvp = fntovn(cfnp);
			cvid = cfnp->fn_vid;
			lck_rw_done(dfnp->fn_rwlock);
			error = vnode_getwithvid(cvp, cvid);
			if (error == 0) {
				*fnpp = cfnp;
				error = EEXIST;
			}
			return (error);
		}

		if (cfnp->fn_next != NULL) {
			diff = (off_t)
			    (cfnp->fn_next->fn_offset - cfnp->fn_offset);
			assert(diff != 0);
			if (diff > 1 && offset == 0) {
				offset = cfnp->fn_offset + 1;
				spp = &cfnp->fn_next;
			}
		} else if (offset == 0) {
			offset = cfnp->fn_offset + 1;
			spp = &cfnp->fn_next;
		}
	}

	/*
	 * This fnnode will be pointing to its parent; grab a usecount
	 * on the parent.
	 */
	error = vnode_ref(fntovn(dfnp));
	if (error != 0) {
		lck_rw_done(dfnp->fn_rwlock);
		return (error);
	}

	/*
	 * I don't hold the mutex on fnpp because I created it, and
	 * I'm already holding the writers lock for it's parent
	 * directory, therefore nobody can reference it without me first
	 * releasing the writers lock.
	 */
	(*fnpp)->fn_offset = offset;
	(*fnpp)->fn_next = *spp;
	*spp = *fnpp;
	(*fnpp)->fn_parent = dfnp;
	(*fnpp)->fn_linkcnt++;	/* parent now holds reference to entry */

	/*
	 * dfnp->fn_linkcnt and dfnp->fn_direntcnt protected by dfnp->rw_lock
	 */
	if (vnode_isdir(fntovn(*fnpp))) {
		/*
		 * The new fnnode is a directory, and has a ".." entry
		 * for its parent.  Count that entry.
		 */
		dfnp->fn_linkcnt++;
	}
	dfnp->fn_direntcnt++;	/* count the directory entry for the new fnnode */

	lck_rw_done(dfnp->fn_rwlock);

	AUTOFS_DPRINT((5, "*fnpp=%p\n", (void *)*fnpp));
	return (0);
}

fnnode_t *
auto_search(fnnode_t *dfnp, char *name, int namelen)
{
	vnode_t dvp;
	fnnode_t *p;

	AUTOFS_DPRINT((4, "auto_search: dfnp=%p, name=%.*s...\n",
	    (void *)dfnp, namelen, name));

	dvp = fntovn(dfnp);
	if (!vnode_isdir(dvp)) {
		panic("auto_search: dvp=%p not a directory", dvp);
	}

	assert(lck_rw_held(dfnp->fn_rwlock));
	for (p = dfnp->fn_dirents; p != NULL; p = p->fn_next) {
		if (p->fn_namelen == namelen &&
		    bcmp(p->fn_name, name, namelen) == 0) {
			AUTOFS_DPRINT((5, "auto_search: success\n"));
			return (p);
		}
	}

	AUTOFS_DPRINT((5, "auto_search: failure\n"));
	return (NULL);
}

#ifdef DEBUG
static int autofs_debug = 0;

/*
 * Utilities used by both client and server
 * Standard levels:
 * 0) no debugging
 * 1) hard failures
 * 2) soft failures
 * 3) current test software
 * 4) main procedure entry points
 * 5) main procedure exit points
 * 6) utility procedure entry points
 * 7) utility procedure exit points
 * 8) obscure procedure entry points
 * 9) obscure procedure exit points
 * 10) random stuff
 * 11) all <= 1
 * 12) all <= 2
 * 13) all <= 3
 * ...
 */
/* PRINTFLIKE2 */
void
auto_dprint(int level, const char *fmt, ...)
{
	va_list args;

	if (autofs_debug == level ||
	    (autofs_debug > 10 && (autofs_debug - 10) >= level)) {
		va_start(args, fmt);
		IOLogv(fmt, args);
		va_end(args);
	}
}

void
auto_debug_set(int level)
{
	autofs_debug = level;
}
#endif /* DEBUG */
