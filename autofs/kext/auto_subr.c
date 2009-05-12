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

#pragma ident	"@(#)auto_subr.c	1.95	05/12/19 SMI"

#include <mach/task.h>
#include <mach/host_priv.h>
#include <mach/host_special_ports.h>
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
#include <sys/proc_internal.h>
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
#include <sys/syslog.h>

#include <kern/assert.h>
#include <kern/host.h>
#include <kern/locks.h>
#include <kern/clock.h>

#ifdef DEBUG
#include <stdarg.h>
#endif

/*
 * XXX - until this gets added to <mach/host_special_ports.h>
 */
#ifndef HOST_AUTOMOUNTD_PORT
#ifdef HOST_MAX_SPECIAL_KERNEL_PORT
#define HOST_AUTOMOUNTD_PORT	(4 + HOST_MAX_SPECIAL_KERNEL_PORT)
#endif
#endif

#ifndef host_get_automountd_port(host, port)
#define host_get_automountd_port(host, port)		\
	(host_get_special_port((host),				\
        HOST_LOCAL_NODE, HOST_AUTOMOUNTD_PORT, (port)))
#endif

#ifndef host_set_automountd_port(host, port)
#define host_set_automountd_port(host, port)		\
	(host_set_special_port((host), HOST_AUTOMOUNTD_PORT, (port)))
#endif

#include "autofs.h"
#include "autofs_kern.h"
#include "autofs_protUser.h"

#define	TYPICALPATH_MAX	64

static int auto_perform_actions(fninfo_t *, fnnode_t *,
    action_list *, ucred_t);
static int auto_lookup_request(fninfo_t *, char *, int,
    vfs_context_t, boolean_t, boolean_t *);
static int auto_mount_request(fninfo_t *, fnnode_t *, char *, int,
    action_list **, ucred_t, mach_port_t, boolean_t);

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
auto_fninfo_lock_shared(fninfo_t *fnip, vfs_context_t ctx)
{
	if (!auto_is_automounter(vfs_context_pid(ctx)))
		lck_rw_lock_shared(fnip->fi_rwlock);
}

/*
 * Release the shared lock, unless we're an automounter, in which case
 * we don't have one.
 */
void
auto_fninfo_unlock_shared(fninfo_t *fnip, vfs_context_t ctx)
{
	if (!auto_is_automounter(vfs_context_pid(ctx)))
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
 * Clears the MF_INPROG flag, and wakes up those threads sleeping on
 * fn_flags if MF_WAITING is set.
 */
void
auto_unblock_others(
	fnnode_t *fnp,
	u_int operation)		/* either MF_INPROG or MF_LOOKUP */
{
	assert(operation & (MF_INPROG | MF_LOOKUP));
	fnp->fn_flags &= ~operation;
	if (fnp->fn_flags & MF_WAITING) {
		fnp->fn_flags &= ~MF_WAITING;
		wakeup(&fnp->fn_flags);
	}
}

int
auto_wait4mount(fnnode_t *fnp, vfs_context_t context)
{
	int error;
	pid_t pid;

	AUTOFS_DPRINT((4, "auto_wait4mount: fnp=%p\n", (void *)fnp));

	pid = vfs_context_pid(context);
	lck_mtx_lock(fnp->fn_lock);
	while (fnp->fn_flags & (MF_INPROG | MF_LOOKUP)) {
		/*
		 * There is a mount or a lookup in progress.
		 *
		 * Is there a mount in progress?
		 *
		 * XXX - still necessary?
		 */
		if (fnp->fn_flags & MF_INPROG) {
			/*
			 * Yes.  Are we an automounter daemon or a process
			 * spawned by an automount daemon?  If so, we can't
			 * block, as we might be the process doing that
			 * mount.
			 */
			if (auto_is_automounter(pid)) {
				lck_mtx_unlock(fnp->fn_lock);
				return (0);
			}

			/*
			 * No.  Are we a nowait process?  If so, we
			 * should return ENOENT for now.
			 */
			if (auto_is_nowait_process(pid)) {
				lck_mtx_unlock(fnp->fn_lock);
				return (ENOENT);
			}
		}
		fnp->fn_flags |= MF_WAITING;
		/*
		 * XXX - pick non-negative priority for this?
		 * I think in OS X setting PCATCH is sufficient to allow
		 * a signal to interrupt the sleep.
		 */
		error = msleep(&fnp->fn_flags, fnp->fn_lock, PSOCK|PCATCH,
		    "autofs_mount", NULL);
		if (error == EINTR || error == ERESTART) {
			/*
			 * Decided not to wait for operation to
			 * finish after all.
			 */
			lck_mtx_unlock(fnp->fn_lock);
			return (EINTR);
		}
	}
	error = fnp->fn_error;

	if (error == EINTR) {
		/*
		 * The thread doing the mount got interrupted, we need to
		 * try again, by returning EAGAIN.
		 */
		error = EAGAIN;
	}
	lck_mtx_unlock(fnp->fn_lock);

	AUTOFS_DPRINT((5, "auto_wait4mount: fnp=%p error=%d\n", (void *)fnp,
	    error));
	return (error);
}

int
auto_lookup_aux(fnnode_t *fnp, char *name, int namelen, vfs_context_t context)
{
	struct fninfo *fnip;
	boolean_t mountreq = FALSE;
	int error = 0;

	fnip = vfstofni(vnode_mount(fntovn(fnp)));
	error = auto_lookup_request(fnip, name, namelen, context, TRUE, &mountreq);
	if (!error) {
		if (mountreq) {
			/*
			 * The automount daemon is requesting a mount,
			 * implying this entry must be a wildcard match and
			 * therefore in need of verification that the entry
			 * exists on the server.
			 */
			lck_mtx_lock(fnp->fn_lock);
			AUTOFS_BLOCK_OTHERS(fnp, MF_INPROG);
			fnp->fn_error = 0;

			/*
			 * Unblock other lookup requests on this node,
			 * this is needed to let the lookup generated by
			 * the mount call to complete. The caveat is
			 * other lookups on this node can also get by,
			 * i.e., another lookup on this node that occurs
			 * while this lookup is attempting the mount
			 * would return a positive result no matter what.
			 * Therefore two lookups on the this node could
			 * potentially get disparate results.
			 */
			AUTOFS_UNBLOCK_OTHERS(fnp, MF_LOOKUP);
			lck_mtx_unlock(fnp->fn_lock);
			/*
			 * auto_new_mount_thread fires up a new thread which
			 * calls automountd finishing up the work
			 */
			auto_new_mount_thread(fnp, name, namelen, context);

			/*
			 * At this point, we are simply another thread
			 * waiting for the mount to complete
			 */
			error = auto_wait4mount(fnp, context);
		}
	}

	lck_mtx_lock(fnp->fn_lock);
	fnp->fn_error = error;

	/*
	 * Notify threads waiting for lookup/mount that
	 * it's done.
	 */
	if (mountreq) {
		AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
	} else {
		AUTOFS_UNBLOCK_OTHERS(fnp, MF_LOOKUP);
	}
	lck_mtx_unlock(fnp->fn_lock);
	return (error);
}

/*
 * Starting point for thread to handle mount requests with automountd.
 */
static void
auto_mount_thread(void *arg)
{
	struct autofs_callargs *argsp = arg;
	struct fninfo *fnip;
	fnnode_t *fnp;
	vnode_t vp;
	char *name;
	int namelen;
	ucred_t cred;
	action_list *alp = NULL;
	int error;
	struct vfsstatfs *vfsstat;
	mount_t mp;

	fnp = argsp->fnc_fnp;
	vp = fntovn(fnp);
	fnip = vfstofni(vnode_mount(vp));
	name = argsp->fnc_name;
	namelen = argsp->fnc_namelen;
	cred = argsp->fnc_cred;

	error = auto_mount_request(fnip, fnp, name, namelen, &alp, cred,
	    argsp->fnc_gssd_port, TRUE);
	if (!error) {
		error = auto_perform_actions(fnip, fnp, alp, cred);

		/*
		 * Stash the fsid of the filesystem the daemon just
		 * mounted so that at unmount time we can verify that
		 * it's the same one we mounted.  If it's different,
		 * then we'll just assume EBUSY and leave it alone.
		 * This can happen if LoginWindow replaces an 
		 * automounted AFP "guest" mount with a full-access
		 * mount for the authenticated user.
		 */
		mp = vnode_mountedhere(vp);
		if (mp == NULL) {
			fnp->fn_fsid_mounted.val[0] = 0;
			fnp->fn_fsid_mounted.val[1] = 0;
		} else {
			vfsstat = vfs_statfs(mp);
			fnp->fn_fsid_mounted = vfsstat->f_fsid;
		}
	}
	lck_mtx_lock(fnp->fn_lock);
	fnp->fn_error = error;
	if (error) {
		/*
		 * Set the UID for this fnnode back to 0; automountd
		 * might have changed it to the UID of the process
		 * that triggered the mount.
		 */
		fnp->fn_uid = 0;
	}

	/*
	 * Notify threads waiting for mount that
	 * it's done.
	 */
	AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
	lck_mtx_unlock(fnp->fn_lock);

	vnode_rele(vp);	/* release reference from auto_new_mount_thread() */
	kauth_cred_unref(&argsp->fnc_cred);
	FREE(argsp->fnc_name, M_AUTOFS);
	FREE(argsp, M_AUTOFS);

	thread_terminate(current_thread());
	/* NOTREACHED */
}

static int autofs_thr_success = 0;

/*
 * Creates new thread which calls auto_mount_thread which does
 * the bulk of the work calling automountd, via 'auto_perform_actions'.
 */
void
auto_new_mount_thread(fnnode_t *fnp, char *name, int namelen, vfs_context_t context)
{
	struct autofs_callargs *argsp;
	int error;
	kern_return_t ret;

	MALLOC(argsp, struct autofs_callargs *, sizeof (*argsp), M_AUTOFS, M_WAITOK);
	/* XXX - what if this fails? */
	error = vnode_ref(fntovn(fnp));	/* released at end of auto_mount_thread */
	argsp->fnc_fnp = fnp;
	MALLOC(argsp->fnc_name, char *, namelen, M_AUTOFS, M_WAITOK);
	bcopy(name, argsp->fnc_name, namelen);
	argsp->fnc_namelen = namelen;
	argsp->fnc_origin = current_thread();
	argsp->fnc_cred = vfs_context_ucred(context);
	kauth_cred_ref(argsp->fnc_cred);

	ret = vfs_context_get_special_port(context, TASK_GSSD_PORT,
	    &argsp->fnc_gssd_port);
	if (ret != KERN_SUCCESS) {
		panic("autofs: can't get gssd port for process %d, status 0x%08x\n",
		    vfs_context_pid(context), ret);
	}
	
	ret = auto_new_thread(auto_mount_thread, argsp);
	if (ret != KERN_SUCCESS)
		panic("autofs: Can't start new mounter thread, status 0x%08x", ret);
	autofs_thr_success++;
}

/*
 * Ask the automounter whether this fnnode is a trigger or not.
 */
int
auto_check_trigger_request(fninfo_t *fnip, char *key, int keylen,
    boolean_t *istrigger)
{
	int error;
	mach_port_t automount_port;
	char *name;
	int namelen;
	boolean_t isdirect;
	kern_return_t ret;

	if (fnip->fi_flags & MF_DIRECT) {
		name = fnip->fi_key;
		namelen = strlen(fnip->fi_key);
	} else {
		name = key;
		namelen = keylen;
	}

	isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;

	/*
	 * XXX - what about the "hard" argument?
	 */
	error = auto_get_automountd_port(&automount_port);
	if (error)
		goto done;
	ret = autofs_check_trigger(automount_port, fnip->fi_map,
	    fnip->fi_path, name, namelen, fnip->fi_subdir, fnip->fi_opts,
	    isdirect, &error, istrigger);
	auto_release_automountd_port(automount_port);

	if (ret != KERN_SUCCESS)
		error = EIO;		/* XXX - process Mach errors */
done:
	if (error)
		*istrigger = FALSE;
	return (error);
}

int
auto_get_automountd_port(mach_port_t *automount_port)
{
	kern_return_t ret;

	*automount_port = MACH_PORT_NULL;
	ret = host_get_automountd_port(host_priv_self(), automount_port);
	if (ret != KERN_SUCCESS) {
		log(LOG_ERR, "autofs: can't get automountd port, status 0x%08x\n",
		    ret);
		return (ECONNREFUSED);
	}
	if (!IPC_PORT_VALID(*automount_port)) {
		log(LOG_ERR, "autofs: automountd port not valid\n");
		return (ECONNRESET);
	}
	return (0);
}

void
auto_release_automountd_port(mach_port_t automount_port)
{
	extern void ipc_port_release_send(ipc_port_t);
	
	ipc_port_release_send(automount_port);
}

static int
auto_lookup_request(
	fninfo_t *fnip,
	char *key,
	int keylen,
	vfs_context_t context,
	boolean_t hard,
	boolean_t *mountreq)
{
	mach_port_t automount_port;
	struct autofs_globals *fngp;
	char *name;
	int namelen;
	boolean_t isdirect;
	int lu_action;
	boolean_t lu_verbose;
	kern_return_t ret;
	int error = 0;
	ucred_t cred = vfs_context_ucred(context);

	AUTOFS_DPRINT((4, "auto_lookup_request: path=%s name=%.*s\n",
	    fnip->fi_path, keylen, key));

	fngp = vntofn(fnip->fi_rootvp)->fn_globals;

	if (fnip->fi_flags & MF_DIRECT) {
		name = fnip->fi_key;
		namelen = strlen(fnip->fi_key);
	} else {
		name = key;
		namelen = keylen;
	}
	AUTOFS_DPRINT((4, "auto_lookup_request: using key=%.*s\n", namelen,
	    name));

	isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;

	/*
	 * XXX - what about the "hard" argument?
	 */
	error = auto_get_automountd_port(&automount_port);
	if (error)
		goto done;
	ret = autofs_lookup(automount_port, fnip->fi_map, fnip->fi_path,
	    name, namelen, fnip->fi_subdir, fnip->fi_opts, isdirect,
	    kauth_cred_getuid(cred), &error, &lu_action, &lu_verbose);
	auto_release_automountd_port(automount_port);
	if (ret == KERN_SUCCESS) {
		fngp->fng_verbose = lu_verbose;
		if (error == 0) {
			switch (lu_action) {
			case AUTOFS_MOUNT_RQ:
				*mountreq = TRUE;
				break;
			case AUTOFS_NONE:
				break;
			default:
				log(LOG_WARNING,
				    "auto_lookup_request: bad action type %d\n",
				    lu_action);
				error = ENOENT;
			}
		}
	} else
		error = EIO;		/* XXX - process Mach errors */

	/*
	 * XXX - free stuff such as string buffers
	 */

done:
	AUTOFS_DPRINT((5, "auto_lookup_request: path=%s name=%.*s error=%d\n",
	    fnip->fi_path, keylen, key, error));
	return (error);
}

static int
getstring(char **strp, uint8_t **inbufp, mach_msg_type_number_t *bytes_leftp)
{
	uint32_t stringlen;

	if (*bytes_leftp < sizeof (uint32_t)) {
		log(LOG_ERR, "Action list too short for string length");
		return (EIO);
	}
	memcpy(&stringlen, *inbufp, sizeof (uint32_t));
	*inbufp += sizeof (uint32_t);
	*bytes_leftp -= sizeof (uint32_t);
	if (stringlen == 0xFFFFFFFF) {
		/* Null pointer */
		*strp = NULL;
	} else {
		if (*bytes_leftp < stringlen) {
			log(LOG_ERR, "Action list too short for string data");
			return (EIO);
		}
		MALLOC(*strp, char *, stringlen + 1, M_AUTOFS, M_WAITOK);
		if (*strp == NULL) {
			log(LOG_ERR, "No space for string data in action list");
			return (ENOMEM);
		}
		memcpy(*strp, *inbufp, stringlen);
		(*strp)[stringlen] = '\0';
		*inbufp += stringlen;
		*bytes_leftp -= stringlen;
	}
	return (0);
}

static void
free_action_list(action_list *alp)
{
	action_list *action, *next_action;

	for (action = alp; action != NULL; action = next_action) {
		if (action->mounta.dir != NULL)
			FREE(action->mounta.dir, M_AUTOFS);
		if (action->mounta.opts != NULL)
			FREE(action->mounta.opts, M_AUTOFS);
		if (action->mounta.path != NULL)
			FREE(action->mounta.path, M_AUTOFS);
		if (action->mounta.map != NULL)
			FREE(action->mounta.map, M_AUTOFS);
		if (action->mounta.subdir != NULL)
			FREE(action->mounta.subdir, M_AUTOFS);
		if (action->mounta.key != NULL)
			FREE(action->mounta.key, M_AUTOFS);
		next_action = action->next;
		FREE(action, M_AUTOFS);
	}
}

static int
auto_mount_request(
	fninfo_t *fnip,
	fnnode_t *fnp,
	char *key,
	int keylen,
	action_list **alpp,
	ucred_t cred,
	mach_port_t gssd_port,
	boolean_t hard)
{
	mach_port_t automount_port;
	int error;
	struct autofs_globals *fngp;
	char *name;
	int namelen;
	boolean_t isdirect;
	int mr_type;
	boolean_t mr_verbose;
	kern_return_t ret;
	byte_buffer actions_buffer;
	mach_msg_type_number_t actions_bufcount;
	vm_map_offset_t map_data;
	vm_offset_t data;
	uint8_t *inbuf;
	mach_msg_type_number_t bytes_left;
	action_list *alp, *prevalp;

	AUTOFS_DPRINT((4, "auto_mount_request: path=%s name=%.*s\n",
	    fnip->fi_path, keylen, key));

	fngp = vntofn(fnip->fi_rootvp)->fn_globals;

	if (fnip->fi_flags & MF_DIRECT) {
		name = fnip->fi_key;
		namelen = strlen(fnip->fi_key);
	} else {
		name = key;
		namelen = keylen;
	}
	AUTOFS_DPRINT((4, "auto_mount_request: using key=%.*s\n", namelen,
	    name));

	isdirect = fnip->fi_flags & MF_DIRECT ? TRUE : FALSE;

	*alpp = NULL;
	/*
	 * XXX - what about the "hard" argument?
	 */
	error = auto_get_automountd_port(&automount_port);
	if (error)
		goto done;
	ret = autofs_mount(automount_port, fnip->fi_map, fnip->fi_path, name,
	    namelen, fnip->fi_subdir, fnip->fi_opts, isdirect,
	    kauth_cred_getuid(cred), gssd_port,
	    &mr_type, &actions_buffer, &actions_bufcount, &error, &mr_verbose);
	auto_release_automountd_port(automount_port);
	if (ret == KERN_SUCCESS) {
		fngp->fng_verbose = mr_verbose;
		switch (mr_type) {
		case AUTOFS_ACTION:
			error = 0;

			/*
			 * Get the action list.
			 */
			ret = vm_map_copyout(kernel_map, &map_data,
			    (vm_map_copy_t)actions_buffer);
			if (ret != KERN_SUCCESS) {
				/* XXX - deal with Mach errors */
				error = EIO;
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
					*alpp = alp;
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
				if (bytes_left < sizeof (uint32_t)) {
					log(LOG_ERR, "Action list too short for isdirect");
					error = EIO;
					break;
				}
				memcpy(&alp->mounta.isdirect, inbuf,
				    sizeof (uint32_t));
				inbuf += sizeof (uint32_t);
				bytes_left -= sizeof (uint32_t);
				error = getstring(&alp->mounta.key, &inbuf,
				    &bytes_left);
				if (error)
					break;
				prevalp = alp;
			}
			vm_deallocate(kernel_map, data, actions_bufcount);
			if (error) {
				free_action_list(*alpp);
				*alpp = NULL;
				goto done;
			}
			break;
		case AUTOFS_DONE:
			break;
		default:
			error = ENOENT;
			log(LOG_WARNING,
			    "auto_mount_request: unknown status %d\n",
			    mr_type);
			break;
		}
	} else
		error = EIO;		/* XXX - process Mach errors */

	/*
	 * XXX - free stuff such as string buffers
	 */

done:
	AUTOFS_DPRINT((5, "auto_mount_request: path=%s name=%.*s error=%d\n",
	    fnip->fi_path, keylen, key, error));
	return (error);
}

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
	char *mntopts,
	boolean_t hard)
{
	int error;
	mach_port_t automount_port;
	int status;
	kern_return_t ret;

	AUTOFS_DPRINT((4, "\tauto_send_unmount_request: fstype=%s "
			" mntpnt=%s\n", fstype,	mntpnt));
	/*
	 * XXX - what about the "hard" argument?
	 */
	error = auto_get_automountd_port(&automount_port);
	if (error)
		goto done;
	ret = autofs_unmount(automount_port, fsid.val[0], fsid.val[1],
	    mntresource, mntpnt, fstype, mntopts, &status);
	auto_release_automountd_port(automount_port);
	if (ret == KERN_SUCCESS)
		error = status;
	else
		error = EIO;		/* XXX - process Mach errors */

done:
	AUTOFS_DPRINT((5, "\tauto_send_unmount_request: error=%d\n", error));

	return (error);
}

static int
auto_send_mount_trigger_request(
	char *mntpt,
	char *submntpt,
	char *path,
	char *opts,
	char *map,
	char *subdir,
	char *key,
	uint32_t flags,
	uint32_t mntflags,
	int32_t mount_to,
	int32_t mach_to,
	int32_t direct,
	fsid_t *fsid,
	boolean_t *top_level,
	boolean_t hard)
{
	int error;
	mach_port_t automount_port;
	kern_return_t ret;
	int32_t fsid_val0, fsid_val1;

	/*
	 * XXX - what about the "hard" argument?
	 */
	error = auto_get_automountd_port(&automount_port);
	if (error)
		return (error);
	ret = autofs_mount_trigger(automount_port, mntpt, submntpt, path,
	    opts, map, subdir, key, flags, mntflags, mount_to, mach_to,
	    direct, &fsid_val0, &fsid_val1, top_level, &error);
	auto_release_automountd_port(automount_port);
	if (ret == KERN_SUCCESS) {
		fsid->val[0] = fsid_val0;
		fsid->val[1] = fsid_val1;
	} else
		error = EIO;		/* XXX - process Mach errors */
	return (error);
}

static int
auto_perform_actions(
	fninfo_t *dfnip,
	fnnode_t *dfnp,
	action_list *alp,
	ucred_t cred)	/* Credentials of the caller */
{
	action_list *p;
	struct mounta *m;
	int32_t mount_to, mach_to = 0;
	fsid_t fsid;
	boolean_t top_level;
	int error, success = 0;
	vnode_t mvp, dvp, newvp;
	fninfo_t *fnip;
	fnnode_t *newfnp, *mfnp;
	int save_triggers = 0;		/* set when we need to save at least */
					/* one trigger node */
	int update_times = 0;
	char *mntpnt;
	char buff[PATH_MAX];
	struct timeval now;
	struct autofs_globals *fngp;

	AUTOFS_DPRINT((4, "auto_perform_actions: alp=%p\n", (void *)alp));

	fngp = dfnp->fn_globals;
	dvp = fntovn(dfnp);

	/*
	 * We don't bother sanity-checking the action list, as we don't perform
	 * the actions ourselves - we send them back to the automounter that
	 * presumably sent them to us in the first place.
	 *
	 * This means the automounter must be trusted.
	 */
	if (vnode_mountedhere(dvp) != NULL) {
		/*
		 * The daemon successfully mounted a filesystem
		 * on the AUTOFS root node.
		 */
		lck_mtx_lock(dfnp->fn_lock);
		dfnp->fn_flags |= MF_MOUNTPOINT;
		assert(dfnp->fn_dirents == NULL);
		lck_mtx_unlock(dfnp->fn_lock);
		success++;
	} else {
		/*
		 * Clear MF_MOUNTPOINT.
		 */
		lck_mtx_lock(dfnp->fn_lock);
		if (dfnp->fn_flags & MF_MOUNTPOINT) {
			AUTOFS_DPRINT((10, "autofs: clearing mountpoint "
			    "flag on %s.", dfnp->fn_name));
			assert(dfnp->fn_dirents == NULL);
			assert(dfnp->fn_trigger == NULL);
		}
		dfnp->fn_flags &= ~MF_MOUNTPOINT;
		lck_mtx_unlock(dfnp->fn_lock);
	}

	for (p = alp; p != NULL; p = p->next) {
		mount_t mvfsp;

		m = &p->mounta;
		/*
		 * use the parent directory's timeout since it's the
		 * one specified/inherited by automount.
		 */
		mount_to = dfnip->fi_mount_to;
		/*
		 * The mountpoint is relative, and it is guaranteed to
		 * begin with "."
		 *
		 */
		assert(m->dir[0] == '.');
		if (m->dir[0] == '.' && m->dir[1] == '\0') {
			/*
			 * mounting on the trigger node
			 */
			mvp = dvp;
			mntpnt = ".";
			vnode_get(mvp);
			error = 0;
			goto mount;
		}
		/*
		 * ignore "./" in front of mountpoint
		 */
		assert(m->dir[1] == '/');
		mntpnt = m->dir + 2;

		AUTOFS_DPRINT((10, "\tdfnip->fi_path=%s\n", dfnip->fi_path));
		AUTOFS_DPRINT((10, "\tdfnip->fi_flags=%x\n", dfnip->fi_flags));
		AUTOFS_DPRINT((10, "\tmntpnt=%s\n", mntpnt));

		if (dfnip->fi_flags & MF_DIRECT) {
			AUTOFS_DPRINT((10, "\tDIRECT\n"));
			(void) snprintf(buff, PATH_MAX, "%s/%s",
			    dfnip->fi_path, mntpnt);
		} else {
			AUTOFS_DPRINT((10, "\tINDIRECT\n"));
			(void) snprintf(buff, PATH_MAX, "%s/%s/%s",
			    dfnip->fi_path, dfnp->fn_name, mntpnt);
		}

		if (vnode_mountedhere(dvp) == NULL) {
			/*
			 * Daemon didn't mount anything on the root
			 * We have to create the mountpoint if it doesn't
			 * exist already
			 *
			 * Create the fnnode first, as we can't hold
			 * the writer lock while creating the fnnode,
			 * because allocating a vnode for it might involve
			 * reclaiming an autofs vnode and hence removing
			 * it from the containing directory - which might
			 * be this directory.
			 */
			error = auto_makefnnode(&mfnp, VDIR,
			    vnode_mount(dvp), NULL, mntpnt, dvp,
			    0, cred, dfnp->fn_globals);
			if (error) {
				log(LOG_ERR, "autofs: mount of %s "
				    "failed - can't create mountpoint (auto_makefnnode failed).\n",
				    buff);
				continue;
			}
			lck_rw_lock_exclusive(dfnp->fn_rwlock);
			/*
			 * Attempt to create the autofs mount point.
			 * If it fails with EEXIST, it already existed.
			 */
			error = auto_enter(dfnp, NULL, mntpnt, &mfnp);
			if (error) {
				if (error == EEXIST) {
					if (vnode_mountedhere(fntovn(mfnp)) != NULL) {
						panic("auto_perform_actions: "
						    "mfnp=%p covered", (void *)mfnp);
					}
					error = 0;
				}
			} else {
				assert((dfnp->fn_flags & MF_MOUNTPOINT) == 0);
				assert(mfnp->fn_linkcnt == 1);
			}
			if (!error)
				update_times = 1;
			lck_rw_unlock_exclusive(dfnp->fn_rwlock);
			if (!error) {
				/*
				 * mfnp is already held.
				 */
				mvp = fntovn(mfnp);
			} else {
				log(LOG_WARNING, "autofs: mount of %s "
				    "failed - can't create mountpoint.\n",
				    buff);
				continue;
			}
		} else {
			mvp = NULL;
		}
mount:
		error = auto_send_mount_trigger_request(m->path, mntpnt,
		    m->path, m->opts, m->map, m->subdir, m->key,
		    m->flags, m->mntflags, mount_to, mach_to, m->isdirect,
		    &fsid, &top_level, FALSE);
		if (error != 0) {
			log(LOG_WARNING,
			    "autofs: autofs mount of %s failed error=%d\n",
			    buff, error);
			if (mvp != NULL)
				vnode_put(mvp);
			continue;
		}

		/*
		 * XXX LOCK - we need some way to get a mount_t with a "hold"
		 * on it, so that the file system doesn't get unmounted and
		 * the mount_t released.
		 */
		mvfsp = vfs_getvfs(&fsid);
		if (mvfsp != NULL) {
			/*
			 * Well, something's mounted there.  Let's get
			 * the root vnode of the mount (and don't trigger
			 * a mount on it, as a VFS_ROOT() call, or a
			 * direct call to auto_root(), would do).
			 */
			fnip = vfstofni(mvfsp);
			newvp = fnip->fi_rootvp;
			error = vnode_get(newvp);
			if (error) {
				/*
				 * Well, that didn't work.  Try to get rid of
				 * whatever's mounted there.
				 */
				error = vfs_unmountbyfsid(&fsid, 0,
				    vfs_context_current());
				if (error && error != ENOENT) {
					log(LOG_WARNING,
					    "autofs: could not "
					    "unmount vfs=%p\n",
					(void *)mvfsp);
				}
				if (mvp != NULL)
					vnode_put(mvp);
				continue;
			}
		} else {
			/*
			 * Somebody must've unmounted it after the automounter
			 * mounted it but before we got here.
			 */
			if (mvp != NULL)
				vnode_put(mvp);
			continue;
		}

		/*
		 * XXX - the Solaris code set auto_mount to the result of
		 * vfs_matchops(dvp->v_vfsp, vfs_getops(newvp->v_vfsp)),
		 * but dvp is known to be an autofs vnode (as it was the
		 * the result of an fntovn() call), so that check is
		 * really just checking whether newvp is an autofs vnode.
		 *
		 * However, regardless of whether it was true, the Solaris
		 * code also did vntofn(newvp), so either the Solaris
		 * vfs_matchops() call always returned 1, or there's a
		 * rather nasty bug in the Solaris autofs.  We assume it's
		 * the former, meaning auto_mount is always set to 1.
		 */
		newfnp = vntofn(newvp);
		newfnp->fn_parent = dfnp;

		/*
		 * At this time we want to save the AUTOFS filesystem as
		 * a trigger node. (We only do this if the mount occured
		 * on a node different from the root.)
		 * We look at the trigger nodes during
		 * the automatic unmounting to make sure we remove them
		 * as a unit and remount them as a unit if the filesystem
		 * mounted at the root could not be unmounted.
		 */
		if (!top_level) {
			save_triggers++;
			/*
			 * Add AUTOFS mount to list of triggers mounted
			 * under dfnp.
			 */
			newfnp->fn_flags |= MF_TRIGGER;
			lck_rw_lock_exclusive(newfnp->fn_rwlock);
			newfnp->fn_next = dfnp->fn_trigger;
			lck_rw_unlock_exclusive(newfnp->fn_rwlock);
			lck_rw_lock_exclusive(dfnp->fn_rwlock);
			dfnp->fn_trigger = newfnp;
			lck_rw_unlock_exclusive(dfnp->fn_rwlock);
			/*
			 * Do vnode_ref(newvp) here since dfnp now holds
			 * reference to it as one of its trigger nodes.
			 */
			error = vnode_ref(newvp);	/* released in unmount_triggers() */
			if (error != 0) {
				panic("vnode_ref failed with %d: %s, line %u",
				    error, __FILE__, __LINE__);
			}
			/* XXX - what if this fails? */
			AUTOFS_DPRINT((10, "\tadding trigger %s to %s\n",
			    newfnp->fn_name, dfnp->fn_name));
			AUTOFS_DPRINT((10, "\tfirst trigger is %s\n",
			    dfnp->fn_trigger->fn_name));
			if (newfnp->fn_next != NULL)
				AUTOFS_DPRINT((10, "\tnext trigger is %s\n",
				    newfnp->fn_next->fn_name));
			else
				AUTOFS_DPRINT((10, "\tno next trigger\n"));
		}
		vnode_put(newvp);

		success++;

		if (update_times) {
			microtime(&now);
			dfnp->fn_atime = dfnp->fn_mtime = now;
		}

		if (mvp != NULL)
			vnode_put(mvp);
	}

	if (save_triggers) {
		/*
		 * Make sure the parent can't be freed while it has triggers.
		 */
		/* XXX - what if this fails? */
		error = vnode_ref(dvp);		/* released in unmount_triggers() */
		if (error != 0) {
			panic("vnode_ref failed with %d: %s, line %u",
			    error, __FILE__, __LINE__);
		}
	}

	/*
	 * Return failure if daemon didn't mount anything, and all
	 * kernel mounts attempted failed.
	 */
	error = success ? 0 : ENOENT;

	if (alp != NULL) {
		if ((error == 0) && save_triggers) {
			/*
			 * Save action_list information, so that we can use it
			 * when it comes time to remount the trigger nodes
			 * The action list is freed when the directory node
			 * containing the reference to it is unmounted in
			 * unmount_tree().
			 */
			lck_mtx_lock(dfnp->fn_lock);
			assert(dfnp->fn_alp == NULL);
			dfnp->fn_alp = alp;
			lck_mtx_unlock(dfnp->fn_lock);
		} else {
			/*
			 * free the action list now,
			 */
			free_action_list(alp);
		}
	}

	AUTOFS_DPRINT((5, "auto_perform_actions: error=%d\n", error));
	return (error);
}

int
auto_makefnnode(
	fnnode_t **fnpp,
	enum vtype type,
	mount_t mp,
	struct componentname *cnp,
	const char *name,
	vnode_t parent,
	int markroot,
	ucred_t cred,
	struct autofs_globals *fngp)
{
	int namelen;
	fnnode_t *fnp;
	errno_t error;
	struct vnode_fsparam vfsp;
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

	if (cnp != NULL) {
		name = cnp->cn_nameptr;
		namelen = cnp->cn_namelen;
	} else
		namelen = strlen(name);

	MALLOC(fnp, fnnode_t *, sizeof(fnnode_t), M_AUTOFS, M_WAITOK);
	bzero(fnp, sizeof(*fnp));
	fnp->fn_namelen = namelen;
	MALLOC(tmpname, char *, fnp->fn_namelen + 1, M_AUTOFS, M_WAITOK);
	bcopy(name, tmpname, namelen);
	tmpname[namelen] = '\0';
	fnp->fn_name = tmpname;
	if (cred) {
		fnp->fn_uid = kauth_cred_getuid(cred);
		fnp->fn_gid = kauth_cred_getgid(cred);
	} else {
		fnp->fn_uid = 0;
		fnp->fn_gid = 0;
	}
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
	fnp->fn_ref_time = now.tv_sec;
	fnp->fn_istrigger = FN_TRIGGER_UNKNOWN;	/* we don't know yet */
	lck_mtx_lock(autofs_nodeid_lock);
	/* XXX - does this need to be 2 for the root vnode? */
	fnp->fn_nodeid = nodeid;
	nodeid += 2;
	fnp->fn_globals = fngp;
	fngp->fng_fnnode_count++;
	lck_mtx_unlock(autofs_nodeid_lock);

	bzero(&vfsp, sizeof(struct vnode_fsparam));
	vfsp.vnfs_mp = mp;
	vfsp.vnfs_vtype = type;
	vfsp.vnfs_str = MNTTYPE_AUTOFS;
	vfsp.vnfs_dvp = parent;
	vfsp.vnfs_fsnode = fnp;
	vfsp.vnfs_vops = autofs_vnodeop_p;
	vfsp.vnfs_markroot = markroot;
	vfsp.vnfs_marksystem = 0;
	vfsp.vnfs_rdev = 0;
	vfsp.vnfs_filesize = 0;
	vfsp.vnfs_cnp = cnp;
	vfsp.vnfs_flags = VNFS_NOCACHE | VNFS_CANTCACHE;

	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &vp); 
	if (error != 0) {
		if (vp) {
			vnode_put(vp);
			vnode_recycle(vp);
		}
		AUTOFS_DPRINT((5, "auto_makefnnode failed with vnode_create error code %d\n", error));
		FREE(fnp->fn_name, M_TEMP);
		FREE(fnp, M_TEMP);
		return error;
	}

	fnp->fn_vnode = vp;

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
	 * to zero.
	 */
	vnode_ref(vp);
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
	vnode_t vp = fntovn(fnp);

	AUTOFS_DPRINT((4, "auto_freefnnode: fnp=%p\n", (void *)fnp));

	assert(fnp->fn_linkcnt == 0);
	assert(!vnode_isinuse(vp, 1));
	assert(!vnode_isdir(vp) || fnp->fn_dirents == NULL);
	assert(fnp->fn_parent == NULL);

	FREE(fnp->fn_name, M_TEMP);
	if (vnode_islnk(vp))
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
 */
void
auto_disconnect(
	fnnode_t *dfnp,
	fnnode_t *fnp)
{
	fnnode_t *tmp, **fnpp;
	vnode_t vp = fntovn(fnp);
	int isdir = vnode_isdir(vp);
	struct timeval now;

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
			*fnpp = tmp->fn_next; 	/* remove it from the list */
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

	lck_mtx_lock(fnp->fn_lock);
	microtime(&now);
	fnp->fn_atime = fnp->fn_mtime = now;
	lck_mtx_unlock(fnp->fn_lock);

	AUTOFS_DPRINT((5, "auto_disconnect: done\n"));
}

/*
 * Add an entry to a directory.
 * Called with a write lock held on the directory.
 */
int
auto_enter(fnnode_t *dfnp, struct componentname *cnp, const char *name,
    fnnode_t **fnpp)
{
	int namelen;
	struct fnnode *cfnp, **spp = NULL;
	off_t offset = 0;
	off_t diff;
	errno_t error;
	struct timeval now;

	if (cnp != NULL) {
		name = cnp->cn_nameptr;
		namelen = cnp->cn_namelen;
	} else
		namelen = strlen(name);
	AUTOFS_DPRINT((4, "auto_enter: dfnp=%p, name=%.*s ", (void *)dfnp,
	    namelen, name));

	assert(lck_rw_held_exclusive(dfnp->fn_rwlock));

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
		if (cfnp->fn_namelen == namelen &&
		    bcmp(cfnp->fn_name, name, namelen) == 0) {
			/*
			 * There is, and this is it.
			 * Put and recycle the vnode for the fnnode we
			 * were handed, get the vnode for the fnnode we
			 * found and, if that succeeded, return EEXIST
			 * to indicate that we found and got that fnnode,
			 * otherwise return the error from vnode_get.
			 * (This means we act like auto_search(), except
			 * that we return EEXIST if we found the fnnode
			 * and got its vnode.)
			 */
			vnode_put(fntovn(*fnpp));
			vnode_recycle(fntovn(*fnpp));
			error = vnode_get(fntovn(cfnp));
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

	microtime(&now);
	dfnp->fn_ref_time = now.tv_sec;

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

#define	DEEPER(x) (((x)->fn_dirents != NULL) || \
			(vnode_mountedhere(fntovn((x)))) != NULL)

/*
 * The caller should have already vnode_rele'd its reference to the
 * root vnode of this filesystem.
 */
static int
auto_inkernel_unmount(mount_t mp)
{
#if 0
	vnode_t *cvp = vfsp->vfs_vnodecovered;
#endif
	int error;

	AUTOFS_DPRINT((4,
	    "auto_inkernel_unmount: mntpnt(%p)\n", mp));

#if 0
	assert(vn_vfswlock_held(cvp));
#endif

	/*
	 * Perform the unmount
	 * The mountpoint has already been locked by the caller.
	 * XXX - not.
	 */
	error = vfs_unmountbyfsid(&(vfs_statfs(mp)->f_fsid), 0,
	    vfs_context_current());

	/*
	 * ENOENT means "there's no file system with that FSID",
	 * for which read "somebody unmounted it out from under
	 * us".  The goal was to unmount it, and that happened,
	 * so don't return an error.
	 */
	if (error == ENOENT)
		error = 0;

	AUTOFS_DPRINT((5, "auto_inkernel_unmount: exit\n"));
	return (error);
}

/*
 * unmounts trigger nodes in the kernel.
 */
static void
unmount_triggers(fnnode_t *fnp, action_list **alp)
{
	fnnode_t *tp, *next;
	int error = 0;
	mount_t mp;
	vnode_t tvp;

	AUTOFS_DPRINT((4, "unmount_triggers: fnp=%p\n", (void *)fnp));
	assert(lck_rw_held_exclusive(fnp->fn_rwlock));

	*alp = fnp->fn_alp;
	next = fnp->fn_trigger;
	while ((tp = next) != NULL) {
		tvp = fntovn(tp);
		assert(vnode_isinuse(tvp, 1));
		next = tp->fn_next;
		/*
		 * drop writer's lock since the unmount will end up
		 * disconnecting this node from fnp and needs to acquire
		 * the writer's lock again.
		 * next has more than one user reference since it's
		 * a trigger node, therefore can not be accidentally freed
		 * by a VN_RELE
		 */
		lck_rw_unlock_exclusive(fnp->fn_rwlock);

		/* XXX LOCK - need a hold */
		mp = vnode_mount(tvp);

		/*
		 * Its parent was holding a reference to it, since this
		 * is a trigger vnode.
		 */
		vnode_rele(tvp);	/* release reference from auto_perform_actions() */
		if (error = auto_inkernel_unmount(mp)) {
			panic("unmount_triggers: "
			    "unmount of vp=%p failed error=%d",
			    (void *)tvp, error);
		}
		/*
		 * reacquire writer's lock
		 */
		lck_rw_lock_exclusive(fnp->fn_rwlock);
	}

	/*
	 * We were holding a reference to our parent.  Drop that.
	 */
	vnode_rele(fntovn(fnp));	/* release reference from auto_perform_actions() */
	fnp->fn_trigger = NULL;
	fnp->fn_alp = NULL;

	AUTOFS_DPRINT((5, "unmount_triggers: finished\n"));
}

/*
 * This routine locks the mountpoint of every trigger node if they're
 * not busy, or returns EBUSY if any node is busy. If a trigger node should
 * be unmounted first, then it sets nfnp to point to it, otherwise nfnp
 * points to NULL.
 */
static int
triggers_busy(fnnode_t *fnp, fnnode_t **nfnp)
{
	int error = 0, done;
	int lck_error = 0;
	fnnode_t *tp, *t1p;
	mount_t mp;

	assert(lck_rw_held_exclusive(fnp->fn_rwlock));

	*nfnp = NULL;
	for (tp = fnp->fn_trigger; tp != NULL; tp = tp->fn_next) {
		AUTOFS_DPRINT((10, "\ttrigger: %s\n", tp->fn_name));
		/* XXX LOCK - need a hold */
		mp = vnode_mount(fntovn(tp));
		error = 0;
		/*
		 * The vn_vfsunlock will be done in auto_inkernel_unmount.
		 * XXX LOCK - Solaris did a vn_vfswlock() here, to prevent
		 * anybody from changing mp->mnt_vnodecovered.
		 */
#if 0
		lck_error = vfs_busy(mp, LK_NOWAIT);
#else
		lck_error = 0;
#endif
		if (lck_error == 0) {
			lck_mtx_lock(tp->fn_lock);
			assert((tp->fn_flags & MF_LOOKUP) == 0);
			if (tp->fn_flags & MF_INPROG) {
				/*
				 * a mount is in progress
				 */
				error = EBUSY;
			}
			lck_mtx_unlock(tp->fn_lock);
		}
		if (lck_error || error || DEEPER(tp) ||
		    vnode_isinuse(fntovn(tp), 2)) {
			/*
			 * Couldn't lock it because it's busy (lck_error != 0),
			 * a mount is in progress (error != 0, i.e. it's EBUSY),
			 * it has dirents or is mounted on (DEEPER(tp)),
			 * or somebody else is holding a reference to this
			 * vnode (reference count is greater than two; one
			 * reference is for the mountpoint, and the second
			 * is for the trigger node).
			 */
			AUTOFS_DPRINT((10, "\ttrigger busy\n"));
			if ((lck_error == 0) && (error == 0)) {
				*nfnp = tp;
				/*
				 * The matching vnode_put is done in
				 * unmount_tree().
				 */
				vnode_get(fntovn(*nfnp));	/* XXX - what if this fails? */
			}
			/*
			 * Unlock previously locked mountpoints
			 */
			for (done = 0, t1p = fnp->fn_trigger; !done;
			    t1p = t1p->fn_next) {
				/*
				 * Unlock all nodes previously
				 * locked. All nodes up to 'tp'
				 * were successfully locked. If 'lck_err' is
				 * set, then 'tp' was not locked, and thus
				 * should not be unlocked. If
				 * 'lck_err' is not set, then 'tp' was
				 * successfully locked, and it should
				 * be unlocked.
				 */
				if (t1p != tp || !lck_error) {
					mp = vnode_mount(fntovn(t1p));
#if 0
					vfs_unbusy(mp);
#endif
				}
				done = (t1p == tp);
			}
			error = EBUSY;
			break;
		}
	}

	AUTOFS_DPRINT((4, "triggers_busy: error=%d\n", error));
	return (error);
}

/*
 * Unlock previously locked trigger nodes.
 */
static int
triggers_unlock(fnnode_t *fnp)
{
	fnnode_t *tp;
	mount_t mp;

	assert(lck_rw_held_exclusive(fnp->fn_rwlock));

	for (tp = fnp->fn_trigger; tp != NULL; tp = tp->fn_next) {
		AUTOFS_DPRINT((10, "\tunlock trigger: %s\n", tp->fn_name));
		mp = vnode_mount(fntovn(tp));
#if 0
		vfs_unbusy(mp);
#endif
	}

	return (0);
}

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
/*
 * It is the caller's responsibility to grab the VVFSLOCK.
 * Releases the VVFSLOCK upon return.
 * XXX - does "force" really mean *F*O*R*C*E here?  I.e.,
 * should the automounter be told to forcibly unmount if
 * it's set?
 */
static int
unmount_node(vnode_t cvp, int nonotify)
{
	int error = 0;
	fnnode_t *cfnp;
	mount_t mp;
	struct vfsstatfs *vfsstat;
	char *mntopts;

	AUTOFS_DPRINT((4, "\tunmount_node cvp=%p\n", (void *)cvp));

#if 0
	assert(vn_vfswlock_held(cvp));
#endif
	cfnp = vntofn(cvp);
	mp = vnode_mountedhere(cvp);
	vfsstat = vfs_statfs(mp);

	/*
	 * Make sure it's the same filesystem we mounted previously.
	 * If someone else replaced the mount, then the fsid
	 * we recorded in the fnnode won't match. In that case just
	 * assume it's busy and leave it alone.
	 */
	if (cfnp->fn_fsid_mounted.val[0] != vfsstat->f_fsid.val[0] ||
	    cfnp->fn_fsid_mounted.val[1] != vfsstat->f_fsid.val[1])
		return (EBUSY);

	if (nonotify || auto_is_autofs(mp)) {
		/*
		 * Either this unmount is supposed to be done with no
		 * notification (so we don't care about any of the stuff,
		 * such as notifying an NFS server of the unmount, that
		 * automountd would do), or this is an autofs mount and
		 * we know there's nothing special to do (such as notifying
		 * an NFS server), so do the unmount in the kernel.
		 * auto_inkernel_unmount() will vn_vfsunlock(cvp);
		 */
		error = auto_inkernel_unmount(mp);
	} else {
		mntopts = "";	/* XXX - need to pass port number, etc. */

#if 0
		vfs_unbusy(mp);
#endif

		error = auto_send_unmount_request(vfsstat->f_fsid,
		    vfsstat->f_mntfromname, vfsstat->f_mntonname,
		    vfsstat->f_fstypename, mntopts, FALSE);
	}

	AUTOFS_DPRINT((5, "\tunmount_node cvp=%p error=%d\n", (void *)cvp,
	    error));
	return (error);
}

/*
 * vp is the "root" of the AUTOFS filesystem.
 * return EBUSY if anybody other than the expected holders are using this
 * vnode.  (Note: "using", as defined by vnode_isinuse(), counts only
 * long-term holders of a vnode, not iocounts.)
 *
 * Must be called with an rwlock on the vp's fnnode held, so that the
 * trigger list and the directory entries list don't change.
 */
static int
check_auto_node(vnode_t vp)
{
	fnnode_t *fnp, *childfnp;
	int error = 0;
	/*
	 * number of references to expect for
	 * a non-busy vnode.
	 */
	u_int count;

	AUTOFS_DPRINT((4, "\tcheck_auto_node vp=%p ", (void *)vp));
	fnp = vntofn(vp);
	assert(fnp->fn_flags & MF_INPROG);
	assert((fnp->fn_flags & MF_LOOKUP) == 0);

	count = 0;
	if (fnp->fn_flags & MF_TRIGGER) {
		/*
		 * parent holds a pointer to us (trigger)
		 */
		count++;
	}
	if (fnp->fn_trigger != NULL) {
		/*
		 * The trigger nodes have a hold on us.
		 */
		count++;
	}
	if (vnode_isvroot(vp))
		count++;
	/*
	 * Each child holds a pointer to us (through the vnode layer).
	 *
	 * XXX - this code knows how the vnode layer works.
	 * However, this is just a hint, so if it's too optimistic,
	 * we just end up doing a bit more work dismantling stuff
	 * under this vnode, attempting to unmount it, failing, and
	 * putting the triggers back.
	 */
	for (childfnp = fnp->fn_dirents; childfnp != NULL;
	    childfnp = childfnp->fn_next)
		count++;
	assert(vnode_isinuse(vp, 0));
	if (vnode_isinuse(vp, count))
		error = EBUSY;

	AUTOFS_DPRINT((5, "\tcheck_auto_node error=%d ", error));
	return (error);
}

/*
 * rootvp is the root of the AUTOFS filesystem.
 * If rootvp is busy (vnode_isinuse(rootvp, 1)) returns EBUSY.
 * else removes every vnode under this tree.
 * ASSUMPTION: Assumes that the only node which can be busy is
 * the root vnode. This filesystem better be two levels deep only,
 * the root and its immediate subdirs.
 * The daemon will "AUTOFS direct-mount" only one level below the root.
 */
static int
unmount_autofs(vnode_t rootvp)
{
	fnnode_t *fnp, *rootfnp, *nfnp;
	vnode_t vp;
	int error;

	AUTOFS_DPRINT((4, "\tunmount_autofs rootvp=%p ", (void *)rootvp));

	rootfnp = vntofn(rootvp);
	lck_rw_lock_exclusive(rootfnp->fn_rwlock);
	error = check_auto_node(rootvp);
	if (error == 0) {
		/*
		 * Remove all items immediately under it.
		 */
		nfnp = NULL;	/* lint clean */
		for (fnp = rootfnp->fn_dirents; fnp != NULL; fnp = nfnp) {
			vp = fntovn(fnp);
			assert(!vnode_isinuse(vp, 1));
			auto_disconnect(rootfnp, fnp);
			nfnp = fnp->fn_next;
			vnode_recycle(fntovn(fnp));
		}
	}
	lck_rw_unlock_exclusive(rootfnp->fn_rwlock);
	AUTOFS_DPRINT((5, "\tunmount_autofs error=%d ", error));
	return (error);
}

/*
 * max number of unmount threads running
 */
static int autofs_unmount_threads = 5;

/*
 * Attempt to unmount all autofs mounts, or all mounts under a particular
 * top-level mount.
 */
void
unmount_tree(struct autofs_globals *fngp, fsid_t *fsidp, int flags)
{
	vnode_t vp, newvp;
	mount_t mp;
	fnnode_t *fnp, *nfnp, *pfnp;
	action_list *alp;
	int error, ilocked_it = 0;
	fninfo_t *fnip;
	time_t ref_time;
	int autofs_busy_root, unmount_as_unit, unmount_done = 0;
	struct timeval now;

	/*
	 * Got to release lock before attempting unmount in case
	 * it hangs.
	 */
	lck_rw_lock_shared(fngp->fng_rootfnnodep->fn_rwlock);
	if ((fnp = fngp->fng_rootfnnodep->fn_dirents) == NULL) {
		assert(fngp->fng_fnnode_count == 1);
		/*
		 * no autofs mounted, done.
		 */
		lck_rw_unlock_shared(fngp->fng_rootfnnodep->fn_rwlock);
		goto done;
	}
	error = vnode_get(fntovn(fnp));
	/* XXX - what if it fails? */
	lck_rw_unlock_shared(fngp->fng_rootfnnodep->fn_rwlock);

	vp = fntovn(fnp);
	fnip = vfstofni(vnode_mount(vp));
	/* reference time for this unmount round */
	microtime(&now);
	ref_time = now.tv_sec;
	/*
	 * If this is an explicitly requested unmount rather than a
	 * time-based unmount, we need to make sure we don't skip
	 * nodes because we think we saw them recently.
	 */
	lck_mtx_lock(fnp->fn_lock);
	if ((flags & UNMOUNT_TREE_IMMEDIATE) &&
	    fnp->fn_unmount_ref_time >= ref_time)
		ref_time = fnp->fn_unmount_ref_time + 1;
	lck_mtx_unlock(fnp->fn_lock);

	AUTOFS_DPRINT((4, "unmount_tree (ID=%ld)\n", ref_time));
top:
	AUTOFS_DPRINT((10, "unmount_tree: %s\n", fnp->fn_name));
	assert(fnp);
	vp = fntovn(fnp);
	if (vnode_islnk(vp)) {
		/*
		 * can't unmount symbolic links
		 */
		goto next;
	}
	mp = vnode_mount(vp);
	if (fnp->fn_parent == fngp->fng_rootfnnodep &&
	    fsidp != NULL) {
		/*
		 * We're doing an unmount of only one top-level subtree,
		 * and this is a top-level subtree; is this the one we
		 * want?
		 */
		struct vfsstatfs *vfsstat = vfs_statfs(mp);
		if (vfsstat->f_fsid.val[0] != fsidp->val[0] ||
		    vfsstat->f_fsid.val[1] != fsidp->val[1]) {
			/*
			 * No, it's not.  Skip this one.
			 */
			goto next;
		}
	}

	fnip = vfstofni(mp);
	assert(vnode_isinuse(vp, 0));
	error = 0;
	autofs_busy_root = unmount_as_unit = 0;
	alp = NULL;

	ilocked_it = 0;
	lck_mtx_lock(fnp->fn_lock);
	if (fnp->fn_flags & (MF_INPROG | MF_LOOKUP)) {
		/*
		 * Either a mount, lookup or another unmount of this
		 * subtree is in progress, don't attempt to unmount at
		 * this time.
		 */
		lck_mtx_unlock(fnp->fn_lock);
		error = EBUSY;
		goto next;
	}
	if (fnp->fn_unmount_ref_time >= ref_time) {
		/*
		 * Already been here, try next node.
		 */
		lck_mtx_unlock(fnp->fn_lock);
		error = EBUSY;
		goto next;
	}
	fnp->fn_unmount_ref_time = ref_time;

	/*
	 * If we're supposed to try the unmount immediately, ignore timeout
	 * values.
	 */
	if (!(flags & UNMOUNT_TREE_IMMEDIATE)) {
		microtime(&now);
		if (fnp->fn_ref_time + fnip->fi_mount_to >
		    now.tv_sec) {
			/*
			 * Node has been referenced recently, try the
			 * unmount of its children if any.
			 */
			lck_mtx_unlock(fnp->fn_lock);
			AUTOFS_DPRINT((10, "fn_ref_time within range\n"));
			lck_rw_lock_shared(fnp->fn_rwlock);
			if (fnp->fn_dirents) {
				/*
				 * Has subdirectory, attempt their
				 * unmount first
				 */
				nfnp = fnp->fn_dirents;
				error = vnode_get(fntovn(nfnp));
				/* XXX - what if it fails? */
				lck_rw_unlock_shared(fnp->fn_rwlock);

				vnode_put(vp);
				fnp = nfnp;
				goto top;
			}
			lck_rw_unlock_shared(fnp->fn_rwlock);
			/*
			 * No children, try next node.
			 */
			error = EBUSY;
			goto next;
		}
	}

	/*
	 * Block other accesses through here.
	 */
	AUTOFS_BLOCK_OTHERS(fnp, MF_INPROG);
	fnp->fn_error = 0;
	lck_mtx_unlock(fnp->fn_lock);
	ilocked_it = 1;

	lck_rw_lock_exclusive(fnp->fn_rwlock);
	if (fnp->fn_trigger != NULL) {
		/*
		 * This node has triggers mounted under it.
		 * We'll have to try to unmount them before we can unmount
		 * this node, and if we can't unmount all of them as well
		 * as this node, we'll have to remount all the ones we
		 * unmounted.
		 */
		unmount_as_unit = 1;
		if ((vnode_mountedhere(vp) == NULL) && (check_auto_node(vp))) {
			/*
			 * AUTOFS mountpoint is busy, there's
			 * no point trying to unmount. Fall through
			 * to attempt to unmount subtrees rooted
			 * at a possible trigger node, but remember
			 * not to unmount this tree.
			 */
			autofs_busy_root = 1;
		}

		if (triggers_busy(fnp, &nfnp)) {
			lck_rw_unlock_exclusive(fnp->fn_rwlock);
			if (nfnp == NULL) {
				error = EBUSY;
				goto next;
			}
			/*
			 * nfnp is busy, try to unmount it first
			 */
			lck_mtx_lock(fnp->fn_lock);
			AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
			lck_mtx_unlock(fnp->fn_lock);
			vnode_put(vp);
			assert(vnode_isinuse(fntovn(nfnp), 1));
			fnp = nfnp;
			goto top;
		}

		/*
		 * At this point, we know all trigger nodes are locked,
		 * and they're not busy or mounted on.
		 */

		if (autofs_busy_root) {
			/*
			 * Got to unlock the the trigger nodes since
			 * I'm not really going to unmount the filesystem.
			 */
			(void) triggers_unlock(fnp);
		} else {
			/*
			 * Attempt to unmount all the trigger nodes,
			 * save the action_list in case we need to
			 * remount them later. The action_list will be
			 * freed later if there was no need to remount the
			 * trigger nodes.
			 */
			unmount_triggers(fnp, &alp);
		}
	}
	lck_rw_unlock_exclusive(fnp->fn_rwlock);

	if (autofs_busy_root)
		goto next;

	/* XXX LOCK - need a hold */
	mp = vnode_mountedhere(vp);
	if (mp != NULL) {
		/*
		 * Node is mounted on.  Try to unmount whatever's mounted
		 * on it.
		 */
		AUTOFS_DPRINT((10, "\tNode is mounted on\n"));

		/*
		 * Deal with /xfn/host/jurassic alikes here...
		 */
		if (auto_is_autofs(mp)) {
			/*
			 * If the filesystem mounted here is AUTOFS, and it
			 * is busy, try to unmount the tree rooted on it
			 * first.
			 *
			 * We don't call VFS_ROOT() because we don't
			 * want to trigger a mount.
			 */
			AUTOFS_DPRINT((10, "\t\tAUTOFS mounted here\n"));
			fnip = vfstofni(mp);
			newvp = fnip->fi_rootvp;
			error = vnode_get(newvp);
			if (error) {
				panic("unmount_tree: vnode_get on root of vfs=%p failed",
				    (void *)mp);
			}
			nfnp = vntofn(newvp);
			if (DEEPER(nfnp)) {
#if 0
				vfs_unbusy(mp);
#endif
				lck_mtx_lock(fnp->fn_lock);
				AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
				lck_mtx_unlock(fnp->fn_lock);
				vnode_put(vp);
				fnp = nfnp;
				goto top;
			}
			/*
			 * Fall through to unmount this filesystem
			 */
			vnode_put(newvp);
		}

		/*
		 * vfs_unbusy(mp) is done inside unmount_node()
		 */
		error = unmount_node(vp, (flags & UNMOUNT_TREE_NONOTIFY));
		if (error == ECONNRESET) {
			AUTOFS_DPRINT((10, "\tConnection dropped\n"));
			if (vnode_mountedhere(vp) == NULL) {
				/*
				 * The filesystem was unmounted before the
				 * daemon died. Unfortunately we can not
				 * determine whether all the cleanup work was
				 * successfully finished (i.e. update mnttab,
				 * or notify NFS server of the unmount).
				 * We should not retry the operation since the
				 * filesystem has already been unmounted, and
				 * may have already been removed from mnttab,
				 * in such case the devid/rdevid we send to
				 * the daemon will not be matched. So we have
				 * to be content with the partial unmount.
				 * Since the mountpoint is no longer covered, we
				 * clear the error condition.
				 */
				error = 0;
				log(LOG_WARNING,
				    "unmount_tree: automountd connection "
				    "dropped\n");
				if (fnip->fi_flags & MF_DIRECT) {
					log(LOG_WARNING, "unmount_tree: "
					    "%s successfully unmounted - "
					    "do not remount triggers\n",
					    fnip->fi_path);
				} else {
					log(LOG_WARNING, "unmount_tree: "
					    "%s/%s successfully unmounted - "
					    "do not remount triggers\n",
					    fnip->fi_path, fnp->fn_name);
				}
			}
		}
	} else {
		/*
		 * Node isn't mounted on.  Attempt to unmount anything
		 * mounted under it.
		 */
#if 0
		vfs_unbusy(mp);
#endif
		AUTOFS_DPRINT((10, "\tNode is AUTOFS\n"));
		if (unmount_as_unit) {
			/*
			 * There are triggers mounted under it.
			 */
			AUTOFS_DPRINT((10, "\tunmount as unit\n"));
			error = unmount_autofs(vp);
		} else {
			AUTOFS_DPRINT((10, "\tunmount one at a time\n"));
			lck_rw_lock_shared(fnp->fn_rwlock);
			if (fnp->fn_dirents != NULL) {
				/*
				 * Has subdirectory, attempt their
				 * unmount first
				 */
				nfnp = fnp->fn_dirents;
				error = vnode_get(fntovn(nfnp));
				/* XXX - what if it fails? */
				lck_rw_unlock_shared(fnp->fn_rwlock);

				lck_mtx_lock(fnp->fn_lock);
				AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
				lck_mtx_unlock(fnp->fn_lock);
				vnode_put(vp);
				fnp = nfnp;
				goto top;
			}
			lck_rw_unlock_shared(fnp->fn_rwlock);
			goto next;
		}
	}

	if (error) {
		AUTOFS_DPRINT((10, "\tUnmount failed\n"));
		if (alp != NULL) {
			/*
			 * Unmount failed, got to remount triggers.
			 */
			error = auto_perform_actions(fnip, fnp, alp, NULL);
			if (error) {
				log(LOG_WARNING, "autofs: can't remount "
				    "triggers fnp=%p error=%d\n", (void *)fnp,
				    error);
				error = 0;
				/*
				 * The action list should have been
				 * free'd by auto_perform_actions
				 * since an error occured
				 */
				alp = NULL;
			}
		}
	} else {
		/*
		 * The unmount succeeded, which will cause this node to
		 * be removed from its parent if its an indirect mount,
		 * therefore update the parent's atime and mtime now.
		 * I don't update them in auto_disconnect() because I
		 * don't want atime and mtime changing every time a
		 * lookup goes to the daemon and creates a new node.
		 */
		unmount_done = 1;
		if ((fnip->fi_flags & MF_DIRECT) == 0) {
			microtime(&now);
			if (fnp->fn_parent == fngp->fng_rootfnnodep)
				fnp->fn_atime = fnp->fn_mtime = now;
			else
				fnp->fn_parent->fn_atime =
					fnp->fn_parent->fn_mtime = now;
		}

		/*
		 * Free the action list here
		 */
		if (alp != NULL) {
			free_action_list(alp);
			alp = NULL;
		}
	}

	microtime(&now);
	fnp->fn_ref_time = now.tv_sec;

next:
	/*
	 * Obtain parent's readers lock before grabbing
	 * reference to next sibling.
	 * XXX Note that nodes in the top level list (mounted
	 * in user space not by the daemon in the kernel) parent is itself,
	 * therefore grabbing the lock makes no sense, but doesn't
	 * hurt either.
	 */
	pfnp = fnp->fn_parent;
	assert(pfnp != NULL);
	lck_rw_lock_shared(pfnp->fn_rwlock);
	if ((nfnp = fnp->fn_next) != NULL)
		error = vnode_get(fntovn(nfnp));	/* XXX - what if it fails? */
	lck_rw_unlock_shared(pfnp->fn_rwlock);

	if (ilocked_it) {
		lck_mtx_lock(fnp->fn_lock);
		if (unmount_done) {
			/*
			 * Other threads may be waiting for this unmount to
			 * finish. We must let it know that in order to
			 * proceed, it must trigger the mount itself.
			 */
			if (fnp->fn_flags & MF_WAITING)
				fnp->fn_error = EAGAIN;
			unmount_done = 0;
		}
		AUTOFS_UNBLOCK_OTHERS(fnp, MF_INPROG);
		lck_mtx_unlock(fnp->fn_lock);
		ilocked_it = 0;
	}

	/*
	 * If this node has a sibling, attempt to unmount it.
	 */
	if (nfnp != NULL) {
		vnode_put(vp);
		fnp = nfnp;
		/*
		 * Unmount next element
		 */
		goto top;
	}

	/*
	 * This node has no sibling; if it has a parent, we got here
	 * from that parent, and if it's not rootfnnodep (which is the
	 * special fnnode that is the parent of all autofs mounts, which
	 * can't be unmounted), so go back to the parent and try to
	 * unmount it.
	 */
	assert(pfnp != fnp);
	if (pfnp != fngp->fng_rootfnnodep) {
		/*
		 * Now attempt to unmount my parent
		 */
		error = vnode_get(fntovn(pfnp));	/* XXX - what if it fails? */
		vnode_put(vp);
		fnp = pfnp;

		goto top;
	}

	vnode_put(vp);

	/*
	 * At this point we've walked the entire tree and attempted to unmount
	 * as much as we can one level at a time.
	 */
done:
	;
}

static void
unmount_zone_tree(void *arg)
{
	struct autofs_globals *fngp = arg;

	unmount_tree(fngp, NULL, 0);
	lck_mtx_lock(fngp->fng_unmount_threads_lock);
	fngp->fng_unmount_threads--;
	lck_mtx_unlock(fngp->fng_unmount_threads_lock);

	AUTOFS_DPRINT((5, "unmount_tree done. Thread exiting.\n"));

	thread_terminate(current_thread());
	/* NOTREACHED */
}

static struct timespec autofs_unmount_thread_timer = {
	120,	/* in seconds */
	0	/* nanoseconds */
};

void
auto_do_unmount(void *arg)
{
	struct autofs_globals *fngp = arg;

	for (;;) {	/* forever */
		lck_mtx_lock(fngp->fng_unmount_threads_lock);
	newthread:
		msleep(&fngp->fng_terminate_do_unmount_thread,
		    fngp->fng_unmount_threads_lock, PWAIT,
		    "unmount timeout", &autofs_unmount_thread_timer);
		if (fngp->fng_terminate_do_unmount_thread) {
			fngp->fng_do_unmount_thread_terminated = 1;
			wakeup(&fngp->fng_do_unmount_thread_terminated);
			lck_mtx_unlock(fngp->fng_unmount_threads_lock);
			thread_terminate(current_thread());
			/* NOTREACHED */
		}

		if (fngp->fng_unmount_threads < autofs_unmount_threads) {
			fngp->fng_unmount_threads++;
			lck_mtx_unlock(fngp->fng_unmount_threads_lock);

			(void) auto_new_thread(unmount_zone_tree, fngp);
		} else
			goto newthread;
	}
	/* NOTREACHED */
}

/*
 * Check to see whether a reference by this process should trigger a mount
 * request on this vnode.
 *
 * If there's already something mounted there, it shouldn't.
 * Otherwise, if there is no mounter process for this vnode, it should.
 * Otherwise, if this process is the mounter for that vnode, or this
 * process is an inferior of the mounter for that vnode, it shouldn't,
 * as this process is presumably referring to this vnode in order to
 * mount on it.
 *
 * Should be called with the mutex held on the fnnode corresponding
 * to vp.
 */
int
auto_dont_trigger(vnode_t vp, vfs_context_t context)
{
	fnnode_t *fnp;
	pid_t pid;

	fnp = vntofn(vp);

	/*
	 * Is something already mounted on this?
	 */
	if (vnode_mountedhere(vp) != NULL) {
		/*
		 * Yes - the mount's already done, so there's nothing
		 * to trigger.
		 */
		return (1);
	}

	pid = vfs_context_pid(context);

	/*
	 * Are we an automounter?  If so, we never trigger anything.
	 */
	if (auto_is_automounter(pid)) {
		/*
	    	 * We are.
	    	 */
		return (1);
	}

	/*
	 * We're just some random process; trigger the mount.
	 */
	return (0);
}

/*
 * Utility routine to create a new thread.
 * This should really be in xnu, and exported by it.
 */
kern_return_t
auto_new_thread(void (*start)(void *), void *arg)
{
	kern_return_t result;
	thread_t thread;

	result = kernel_thread_start((thread_continue_t)start, arg, &thread);
	if (result != KERN_SUCCESS)
		return (result);

	thread_deallocate(thread);

	return (KERN_SUCCESS);
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
		vprintf(fmt, args); 
		va_end(args);
	}
}

void
auto_debug_set(int level)
{
	autofs_debug = level;
}
#endif /* DEBUG */
