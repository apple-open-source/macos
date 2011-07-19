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

#include <mach/task.h>
#include <mach/host_priv.h>
#include <mach/host_special_ports.h>

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/queue.h>

#include <kern/host.h>
#include <kern/locks.h>

#include <IOKit/IOLib.h>

#include "autofs.h"
#include "autofs_protUser.h"
#include "triggers.h"
#include "triggers_priv.h"

static lck_grp_t *triggers_lck_grp;

__private_extern__ int triggers_start(kmod_info_t *, void *);
__private_extern__ int triggers_stop(kmod_info_t *, void *);

/* XXX Get rid of this as soon as sys/malloc.h can be updated to define a real M_AUTOFS */
#define M_TRIGGERS M_TEMP

/*
 * TRUE if two fsids are equal.
 */
#define FSIDS_EQUAL(fsid1, fsid2)	\
	((fsid1).val[0] == (fsid2).val[0] && \
	 (fsid1).val[1] == (fsid2).val[1])

static lck_rw_t *resolved_triggers_rwlock;
static TAILQ_HEAD(resolved_triggers, trigger_info) resolved_triggers =
    TAILQ_HEAD_INITIALIZER(resolved_triggers);

/*
 * Timeout for unmounts.
 */
static int mount_to;

/*
 * Set the mount timeout.
 */
void
trigger_set_mount_to(int newval)
{
	mount_to = newval;
}

/*
 * Unmount a file system by fsid.
 *
 * XXX - do this with an automountd upcall, so it doesn't try to unmount
 * from a dead server?
 */
static int
unmount_triggered_mount(fsid_t *fsidp, int flags, vfs_context_t ctx)
{
	return (vfs_unmountbyfsid(fsidp, flags, ctx));
}

/*
 * We've unmounted whatever was mounted atop this trigger, so
 * remove it from the list of resolved triggers, bump its
 * sequence number, and return a "this was unresolved"
 * resolver_result_t for that.
 *
 * Must be called with the mutex held on ti.
 */
static resolver_result_t
trigger_make_unresolved(trigger_info_t *ti)
{
	/*
	 * Remove this from the list of resolved triggers.
	 */
	if (ti->ti_flags & TF_RESOLVED) {
		lck_rw_lock_exclusive(resolved_triggers_rwlock);
		TAILQ_REMOVE(&resolved_triggers, ti, ti_entries);
		lck_rw_unlock_exclusive(resolved_triggers_rwlock);
		ti->ti_flags &= ~TF_RESOLVED;
	}

	/*
	 * The status of the trigger has changed, so bump the
	 * sequence number.
	 */
	ti->ti_seq++;
	return (vfs_resolver_result(ti->ti_seq, RESOLVER_UNRESOLVED, 0));
}

/*
 * Sets the TF_INPROG flag on this trigger.
 * ti->ti_lock should be held before this macro is called.
 */
#define	TRIGGER_BLOCK_OTHERS(ti)	{ \
	lck_mtx_assert((ti)->ti_lock, LCK_MTX_ASSERT_OWNED); \
	assert(!((ti)->ti_flags & TF_INPROG)); \
	(ti)->ti_flags |= TF_INPROG; \
}

#define	TRIGGER_UNBLOCK_OTHERS(ti)	{ \
	trigger_unblock_others(ti); \
}

/*
 * Clears the TF_INPROG flag, and wakes up those threads sleeping on
 * fn_flags if TF_WAITING is set.
 */
static void
trigger_unblock_others(trigger_info_t *ti)
{
	ti->ti_flags &= ~TF_INPROG;
	if (ti->ti_flags & TF_WAITING) {
		ti->ti_flags &= ~TF_WAITING;
		wakeup(&ti->ti_flags);
	}
}

static int
trigger_do_mount_url(void *arg)
{
	struct mount_url_callargs *argsp = arg;
	mach_port_t automount_port;
	int error;
	kern_return_t ret;

	error = auto_get_automountd_port(&automount_port);
	if (error) {
		/*
		 * Release the GSSD port send right, if it's valid,
		 * as we won't be using it.
		 */
		if (IPC_PORT_VALID(argsp->muc_gssd_port))
			auto_release_port(argsp->muc_gssd_port);
		goto done;
	}
	ret = autofs_mount_url(automount_port, argsp->muc_url,
	    argsp->muc_mountpoint, argsp->muc_opts, argsp->muc_this_fsid,
	    argsp->muc_uid, argsp->muc_gssd_port,
	    &argsp->muc_mounted_fsid, &argsp->muc_retflags, &error);
	auto_release_port(automount_port);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs: autofs_mount_url failed, status 0x%08x\n", ret);
		error = EIO;		/* XXX - process Mach errors */
	}

done:
	return (error);
}

/*
 * Starting point for thread to handle mount requests with automountd.
 */
static void
trigger_mount_thread(void *arg)
{
	struct trigger_callargs *argsp = arg;
	vnode_t vp;
	trigger_info_t *ti;
	int error;
	struct timeval now;

	vp = argsp->tc_vp;
	ti = argsp->tc_ti;

	/*
	 * Make the appropriate upcall, and do whatever work is
	 * necessary to process its results.
	 */
	error = (*ti->ti_do_mount)(arg);

	lck_mtx_lock(ti->ti_lock);
	ti->ti_error = error;

	/*
	 * If the mount succeeded, bump the sequence number, as this is
	 * a trigger state change, add this mount to the list of
	 * triggered mounts, and notify the VFS layer that the trigger
	 * has been resolved.
	 */
	if (error == 0) {
		/*
		 * Grab an exclusive lock, so that if we get
		 * this unmounted out from under us before
		 * we add it to the list, the attempt to
		 * remove it from the list won't happen until
		 * after we've added it to the list (so we
		 * don't do a remove before the add, which will
		 * do nothing as it won't find the item to
		 * remove, leaving an orphaned entry).
		 *
		 * XXX - shouldn't the mutex on this entry suffice
		 * to serialize operations on this entry?
		 */
		lck_rw_lock_exclusive(resolved_triggers_rwlock);

		/*
		 * Fill in the fsid of the mounted file system, and
		 * various flags, from the out parameters for the call.
		 */
		ti->ti_mounted_fsid = argsp->tc_mounted_fsid;
		if (argsp->tc_retflags & MOUNT_RETF_DONTUNMOUNT)
			ti->ti_flags |= TF_DONTUNMOUNT;
		else
			ti->ti_flags &= ~TF_DONTUNMOUNT;
		if (argsp->tc_retflags & MOUNT_RETF_DONTPREUNMOUNT)
			ti->ti_flags |= TF_DONTPREUNMOUNT;
		else
			ti->ti_flags &= ~TF_DONTPREUNMOUNT;

		/*
		 * Mark it as having been referenced.
		 */
		microtime(&now);
		ti->ti_ref_time = now.tv_sec;

		/*
		 * Put it the end of the list of resolved triggers,
		 * and note that it's resolved.
		 *
		 * XXX - if it's already marked as resolved, note
		 * that?  I.e., is that a "can't happen"?
		 *
		 * XXX - remove on unresolve, rearm, or both?
		 */
		if (!(ti->ti_flags & TF_RESOLVED)) {
			ti->ti_flags |= TF_RESOLVED;
			TAILQ_INSERT_TAIL(&resolved_triggers, ti,
			    ti_entries);
		}

		lck_rw_unlock_exclusive(resolved_triggers_rwlock);

		ti->ti_seq++;
		vnode_trigger_update(vp,
		    vfs_resolver_result(ti->ti_seq, RESOLVER_RESOLVED, 0));
	}

	/*
	 * Notify threads waiting for mount that
	 * it's done.
	 */
	TRIGGER_UNBLOCK_OTHERS(ti);
	lck_mtx_unlock(ti->ti_lock);

	vnode_rele(vp);	/* release usecount from trigger_resolve() */

	(*ti->ti_rel_mount_args)(argsp);

	/*
	 * Release the iocount from trigger_resolve(); we can't
	 * do that until we're done with *ti, as, if the vnode
	 * gets deadfsed, *ti will be freed.
	 */
	vnode_put(vp);

	thread_terminate(current_thread());
	/* NOTREACHED */
}

static int trigger_thr_success = 0;

/*
 * As the name indicates, this must be called with ti locked.
 */
static int
wait_for_mount_locked(trigger_info_t *ti)
{
	int error;

	/*
	 * OK, we should wait.
	 */
	while (ti->ti_flags & TF_INPROG) {
		ti->ti_flags |= TF_WAITING;

		/*
		 * This wait is interruptable, so you can ^C out of
		 * a mount that's taking a long time.
		 */
		error = msleep(&ti->ti_flags, ti->ti_lock, PSOCK|PCATCH,
		    "triggered_mount", NULL);
		if (error == EINTR || error == ERESTART) {
			/*
			 * Decided not to wait for operation to
			 * finish after all.
			 */
			return (EINTR);
		}
	}

	/*
	 * The mount operation finished; return the status with which
	 * it finished.
	 */
	return (ti->ti_error);
}

/*
 * Resolve this trigger.
 */
static resolver_result_t
trigger_resolve(vnode_t vp, const struct componentname *cnp,
    enum path_operation pop, __unused int flags, void *data, vfs_context_t ctx)
{
	trigger_info_t *ti = data;
	resolver_result_t result;
	int pid = vfs_context_pid(ctx);
	int error;
	struct trigger_callargs *argsp;
	kern_return_t ret;

	/*
	 * If this is the last component, is the operation being done
	 * supposed to trigger a mount?
	 * (If it's not the last component, we'll be doing a lookup
	 * in it, so it must always trigger a mount, so we can do the
	 * lookup in the directory mounted on it.)
	 *
	 * XXX - what about OP_LOOKUP?
	 */
	if (cnp->cn_flags & ISLASTCN) {
		switch (pop) {

		case OP_MOUNT:
			/*
			 * We're trying to mount something on this, which
			 * probably means somebody else already triggered
			 * the mount and we're trying to do the mount for
			 * them.  Triggering a mount is thus pointless.
			 *
			 * XXX - do that only if there's a mount in progress?
			 * XXX - need to protect access to ti_seq with
			 * the mutex?
			 */
			return (vfs_resolver_result(ti->ti_seq,
			    RESOLVER_NOCHANGE, 0));

		case OP_UNMOUNT:
			/*
			 * We don't care.
			 */
			return (vfs_resolver_result(ti->ti_seq,
			    RESOLVER_NOCHANGE, 0));

		case OP_STATFS:
			/*
			 * Don't trigger unless this is a "force a mount"
			 * node.
			 */
			if (!(ti->ti_flags & TF_FORCEMOUNT)) {
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		case OP_ACCESS:
			/*
			 * Various bits of file-management code in OS X love
			 * to try to get access permissions for everything
			 * they see.  Don't trigger a mount on that, unless
			 * this is a "force a mount" node, so that we don't
			 * get mount storms.
			 */
			if (!(ti->ti_flags & TF_FORCEMOUNT)) {
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		case OP_GETATTR:
			/*
			 * stat(), etc. shouldn't trigger mounts, unless this
			 * is a "force a mount" node, so that "ls -l", the
			 * Finder, etc. won't cause mount storms.
			 */
			if (!(ti->ti_flags & TF_FORCEMOUNT)) {
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		case OP_LISTXATTR:
			/*
			 * Don't trigger on this, either.
			 */
			if (!(ti->ti_flags & TF_FORCEMOUNT)) {
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		case OP_MKNOD:
		case OP_MKFIFO:
		case OP_SYMLINK:
		case OP_LINK:
		case OP_MKDIR:
			/*
			 * For create operations: don't trigger mount on the
			 * last component of the pathname.  If the target name
			 * doesn't exist, there's nothing to trigger.  If
			 * it does exist and there's something mounted
			 * there, there's nothing to trigger.  If it does
			 * exist but there's nothing mounted there, either
			 * somebody mounts something there before the next
			 * reference (e.g., the home directory mechanism),
			 * in which case we don't want any mounts triggered
			 * for it, or somebody refers to it before a mount
			 * is done on it, in which case we trigger the mount
			 * *then*.
			 */
			if (!(ti->ti_flags & TF_FORCEMOUNT)) {
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		case OP_UNLINK:
		case OP_RMDIR:
			/*
			 * For delete operations: don't trigger mount on the
			 * last component of the pathname.  We don't allow
			 * removal of autofs objects.
			 */
			if (!(ti->ti_flags & TF_FORCEMOUNT)) {
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		case OP_RENAME:
			/*
			 * For renames: don't trigger mount on the last
			 * component of the pathname.  We don't allow
			 * renames of autofs objects.
			 */
			if (!(ti->ti_flags & TF_FORCEMOUNT)) {
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		case OP_FSCTL:
			/*
			 * Check whether we should trigger a mount for
			 * this; a home directory mounter process should
			 * not do so, so that it can use fsctl() to mark
			 * an autofs trigger as having a home directory mount
			 * in progress on it.
			 */
			if (ti->ti_check_homedirmounter_process != NULL &&
			    (*ti->ti_check_homedirmounter_process)(NULL, pid)) {
				/*
				 * No.
				 */
				return (vfs_resolver_result(ti->ti_seq,
				    RESOLVER_NOCHANGE, 0));
			}
			break;

		default:
			break;
		}
	}

	lck_mtx_lock(ti->ti_lock);

top:
	/*
	 * Is something already mounted here?  If so, we don't need
	 * to do a mount to resolve this (we can't mount something
	 * atop the vnode, we could only mount something atop the
	 * tower of stuff already mounted atop the vnode, which
	 * would not be what's intended, and if something's mounted
	 * here and we didn't mount it, it's probably an AFP or SMB
	 * home directory, so it's probably what's supposed to be
	 * mounted there, just authenticated for the user whose home
	 * directory it is).
	 */
	if (vnode_mountedhere(vp) != NULL) {
		/*
		 * The VFS layer thinks we're unresolved, so bump the
		 * sequence number to let them know that's not the
		 * case.
		 */
		ti->ti_seq++;
		result = vfs_resolver_result(ti->ti_seq, RESOLVER_RESOLVED, 0);
		lck_mtx_unlock(ti->ti_lock);
		return (result);
	}

	/*
	 * Is this file marked as having a home directory mount in
	 * progress?
	 */
	if (ti->ti_check_homedirmount != NULL &&
	    (*ti->ti_check_homedirmount)(vp)) {
		/*
		 * Are we the home directory mounter for this vnode?
		 *
		 * If so, don't trigger a mount, but return success, so
		 * we can manipulate the mount point (e.g., changing
		 * its ownership so the mount can be done as the user
		 * by the NetAuth agent).
		 *
		 * If not, don't trigger a mount and return ENOENT, so
		 * that nobody else forces an automount while a home
		 * directory mount is in progress.  (If somebody needs
		 * that mount to occur, THEY SHOULD WAIT FOR THE HOME
		 * DIRECTORY MOUNT TO FINISH FIRST, as they probably
		 * need the mount done as the user.)
		 */
		if (ti->ti_check_homedirmounter_process != NULL &&
		    (*ti->ti_check_homedirmounter_process)(vp, pid)) {
			result = vfs_resolver_result(ti->ti_seq,
			    RESOLVER_RESOLVED, 0);
		} else {
			result = vfs_resolver_result(ti->ti_seq,
			    RESOLVER_ERROR, ENOENT);
		}
		lck_mtx_unlock(ti->ti_lock);
		return (result);
	}

	/*
	 * Are we a process that should not trigger mounts?  If so,
	 * don't trigger, just succeed.
	 */
	if (ti->ti_check_notrigger_process != NULL) {
		if ((*ti->ti_check_notrigger_process)(pid)) {
			/*
			 * Don't trigger anything, just return
			 * success.
			 */
			result = vfs_resolver_result(ti->ti_seq,
			    RESOLVER_NOCHANGE, 0);
			lck_mtx_unlock(ti->ti_lock);
			return (result);
		}
	}

	/*
	 * We need to do a mount in order to resolve this.
	 * Is there already a mount in progress on this vnode?
	 */
	if (!(ti->ti_flags & TF_INPROG)) {
		/*
		 * No - do the mount.
		 *
		 * First, mark that we have a mount in progress,
		 * preventing anybody else from driving through
		 * and starting a mount.
		 */
		TRIGGER_BLOCK_OTHERS(ti);
		ti->ti_error = 0;
		lck_mtx_unlock(ti->ti_lock);

		/*
		 * Now attempt to grab an iocount on the vnode on which
		 * we're supposed to do a mount.  We need an iocount
		 * to ensure that it doesn't get deadfsed out from
		 * under the mount thread; we have an iocount on it
		 * now, but if we get interrupted while we're waiting
		 * for the mount to complete, we'll drop our iocount.
		 */
		error = vnode_get(vp);	/* released at end of trigger_mount_url_thread */
		if (error != 0)
			goto fail;

		/*
		 * Now attempt to grab a usecount on the vnode on which
		 * we're supposed to do a mount.  We want a usecount
		 * to ensure that a non-forced unmount attempt on the
		 * file system with the trigger will fail, rather than
		 * grabbing a write lock on the file system and blocking
		 * the mount and then doing a vflush() that waits for
		 * the mount point's iocount to be released - which won't
		 * happen until the mount either completes or fails, and
		 * that won't happen until the write lock is released....
		 */
		error = vnode_ref(vp);	/* released at end of trigger_mount_url_thread */
		if (error != 0) {
			vnode_put(vp);	/* release iocount */
			goto fail;
		}

		/*
		 * Allocate the argument structure, and get the arguments
		 * that are specific to our client.
		 */
		argsp = (*ti->ti_get_mount_args)(vp, ctx, &error);
		if (argsp == NULL) {
			vnode_rele(vp);	/* release usecount from above */
			vnode_put(vp);	/* release iocount from above */
			goto fail;
		}

		/*
		 * Fill in the arguments common to all clients.
		 */
		argsp->tc_vp = vp;
		argsp->tc_this_fsid = vfs_statfs(vnode_mount(vp))->f_fsid;
		argsp->tc_ti = ti;
		argsp->tc_origin = current_thread();
		/* These are "out" arguments; just initialize them */
		memset(&argsp->tc_mounted_fsid, 0, sizeof (argsp->tc_mounted_fsid));
		argsp->tc_retflags = 0;	/* until shown otherwise */

		/*
		 * Try to get the gssd special port for our context.
		 */
		ret = vfs_context_get_special_port(ctx, TASK_GSSD_PORT,
		    &argsp->tc_gssd_port);
		if (ret != KERN_SUCCESS) {
			IOLog("trigger_resolve: can't get gssd port for process %d, status 0x%08x\n",
			    vfs_context_pid(ctx), ret);
			error = EIO;
			goto fail_thread;
		}

		/*
		 * Now attempt to create a new thread which calls
		 * trigger_mount_thread; that thread does the bulk
		 * of the work of calling automountd and dealing
		 * with its results, with the help of our client's
		 * do_mount routine.
		 */
		ret = auto_new_thread(trigger_mount_thread, argsp);
		if (ret != KERN_SUCCESS) {
			/*
			 * Release the GSSD port send right, if it's valid,
			 * as we won't be using it.
			 */
			if (IPC_PORT_VALID(argsp->tc_gssd_port))
				auto_release_port(argsp->tc_gssd_port);
			IOLog("trigger_resolve: can't start new mounter thread, status 0x%08x",
			    ret);
			error = EIO;
			goto fail_thread;
		}
		trigger_thr_success++;

		/*
		 * Put the lock back, and fall through.
		 */
		lck_mtx_lock(ti->ti_lock);
	}

	/*
	 * At this point, either there was already a mount in progress
	 * or we've created a thread to do the mount; in either case,
	 * we are now simply another thread waiting for the mount to
	 * complete.
	 */

	/*
	 * Should we wait for the mount to complete, or just drive
	 * through?
	 */
	if (ti->ti_check_nowait_process != NULL) {
		if ((*ti->ti_check_nowait_process)(pid)) {
			/*
			 * Drive through, return ENOENT.
			 */
			result = vfs_resolver_result(ti->ti_seq, RESOLVER_ERROR,
			    ENOENT);
			lck_mtx_unlock(ti->ti_lock);
			return (result);
		}
	}

	/*
	 * OK, we should wait for it.
	 */
	error = wait_for_mount_locked(ti);
	if (error == 0) {
		/*
		 * Mount succeeded.  The sequence number has already been
		 * incremented, and the status of the trigger updated, by
		 * trigger_mount_url_thread(), so this won't change the
		 * status of the trigger - which is as it should be.
		 */
		result = vfs_resolver_result(ti->ti_seq, RESOLVER_RESOLVED, 0);
	} else if (error == EAGAIN) {
		/*
		 * It wasn't that a mount that was in progress, it was
		 * that unmount_tree() was working on this fnnode.
		 * Go back and see whether we need to do the mount.
		 */
		goto top;
	} else {
		/*
		 * Mount failed, or we got interrupted by a signal before
		 * the mount finished.  The sequence number hasn't been
		 * changed, so this won't change the status of the trigger,
		 * it'll just hand an error up.
		 */
		result = vfs_resolver_result(ti->ti_seq, RESOLVER_ERROR,
		    error);
	}
	lck_mtx_unlock(ti->ti_lock);
	return (result);

fail_thread:
	vnode_rele(vp);	/* release usecount from above */
	vnode_put(vp);	/* release iocount from above */
	(*ti->ti_rel_mount_args)(argsp);

fail:
	lck_mtx_lock(ti->ti_lock);
	ti->ti_error = error;
	TRIGGER_UNBLOCK_OTHERS(ti);
	result = vfs_resolver_result(ti->ti_seq, RESOLVER_ERROR, error);
	lck_mtx_unlock(ti->ti_lock);
	return (result);
}

/*
 * "Un-resolve" this trigger, for which read "unmount the file system".
 */
static resolver_result_t
trigger_unresolve(__unused vnode_t vp, int flags, void *data, vfs_context_t ctx)
{
	trigger_info_t *ti = data;
	int error;
	resolver_result_t result;

	/*
	 * Lock the trigger.
	 */
	lck_mtx_lock(ti->ti_lock);

	/*
	 * Unmount whatever we last mounted on that trigger, unless
	 * this is a "never auto-unmount this" mount.
	 *
	 * If that's not mounted there, that's OK; what we mounted
	 * there was probably unmounted by hand, and what was then
	 * mounted there is something that was mounted by hand,
	 * probably a mount containing a network home directory,
	 * and it shouldn't be auto-unmounted as we might not be
	 * able to correctly re-auto-mount it.
	 */
	if (ti->ti_flags & TF_DONTUNMOUNT)
		error = EBUSY;	/* pretend it's always busy */
	else
		error = unmount_triggered_mount(&ti->ti_mounted_fsid, flags,
		    ctx);
	if (error == 0)
		result = trigger_make_unresolved(ti);
	else {
		/*
		 * The status of the resolver *hasn't* changed, so don't
		 * change the sequence number.
		 */
		result = vfs_resolver_result(ti->ti_seq, RESOLVER_ERROR,
		    error);
	}
	lck_mtx_unlock(ti->ti_lock);
	return (result);
}

/*
 * We've been asked to rearm this trigger.
 *
 * If the file system using us has a rearm function, first call it to
 * perform whatever actions it wants to do.
 *
 * Then, if nothing is mounted atop this, we set the trigger's state
 * to unresolved, otherwise we tell our caller that it's resolved.
 *
 * XXX - this is only called if somebody else unmounts something from
 * atop this trigger, not if the VFS layer called the unresolve routine
 * to do the unmount, right?
 */
static resolver_result_t
trigger_rearm(vnode_t vp, __unused int flags, void *data, vfs_context_t ctx)
{
	trigger_info_t *ti = data;
	resolver_result_t result;

	lck_mtx_lock(ti->ti_lock);

	if (ti->ti_rearm != NULL)
		(*ti->ti_rearm)(vp, vfs_context_pid(ctx));

	if (vnode_mountedhere(vp) != NULL) {
		/*
		 * Something's mounted on it, so call it resolved.
		 *
		 * XXX - should we do this?
		 */
		ti->ti_seq++;
		result = vfs_resolver_result(ti->ti_seq, RESOLVER_RESOLVED,
		    0);
	} else {
		/*
		 * Nothing's mounted on it, so call it unresolved.
		 */
		result = trigger_make_unresolved(ti);
	}
	lck_mtx_unlock(ti->ti_lock);
	return (result);
}

/*
 * Reclaim, i.e. free, any data associated with this trigger.
 */
static void
trigger_reclaim(__unused vnode_t vp, void *data)
{
	trigger_info_t *ti = data;

	/*
	 * If we have a routine to call on a release, do so, passing
	 * it the private data pointer.
	 */
	if (ti->ti_reclaim != NULL)
		(*ti->ti_reclaim)(ti->ti_private);
	trigger_free(ti);
}

/*
 * Allocate and initialize a new trigger info structure.  Fill in
 * the trigger-related fields of a "struct vnode_trigger_param"
 * appropriately.  Return a pointer to the structure.
 */
trigger_info_t *
trigger_new_autofs(struct vnode_trigger_param *vnt,
    u_int flags,
    int (*check_notrigger_process)(int),
    int (*check_nowait_process)(int),
    int (*check_homedirmounter_process)(vnode_t, int),
    int (*check_homedirmount)(vnode_t),
    void *(*get_mount_args)(vnode_t, vfs_context_t, int *),
    int (*do_mount)(void *),
    void (*rel_mount_args)(void *),
    void (*rearm)(vnode_t, int),
    void (*reclaim)(void *),
    void *private)
{
	struct timeval now;
	trigger_info_t *ti;

	microtime(&now);

	MALLOC(ti, trigger_info_t *, sizeof (*ti), M_TRIGGERS, M_WAITOK);
	ti->ti_lock = lck_mtx_alloc_init(triggers_lck_grp, NULL);
	ti->ti_seq = 0;
	ti->ti_flags = flags;
	ti->ti_error = 0;
	ti->ti_ref_time = now.tv_sec;
	ti->ti_check_notrigger_process = check_notrigger_process;
	ti->ti_check_nowait_process = check_nowait_process;
	ti->ti_check_homedirmounter_process = check_homedirmounter_process;
	ti->ti_check_homedirmount = check_homedirmount;
	ti->ti_get_mount_args = get_mount_args;
	ti->ti_do_mount = do_mount;
	ti->ti_rel_mount_args = rel_mount_args;
	ti->ti_rearm = rearm;
	ti->ti_reclaim = reclaim;
	ti->ti_private = private;

	vnt->vnt_resolve_func = trigger_resolve;
	vnt->vnt_unresolve_func = trigger_unresolve;
	vnt->vnt_rearm_func = trigger_rearm;
	vnt->vnt_reclaim_func = trigger_reclaim;
	vnt->vnt_data = ti;
	vnt->vnt_flags = VNT_AUTO_REARM;

	return (ti);
}

/*
 * For use outside autofs - shouldn't need to change if autofs changes.
 */

trigger_info_t *
trigger_new(struct vnode_trigger_param *vnt,
    void *(*get_mount_args)(vnode_t, vfs_context_t, int *),
    void (*rel_mount_args)(void *))
{
	return (trigger_new_autofs(vnt, 0, NULL, NULL, NULL, NULL,
	    get_mount_args, trigger_do_mount_url, rel_mount_args,
	    NULL, NULL, NULL));
}

void
trigger_free(trigger_info_t *ti)
{
	/*
	 * Remove this from whatever list it's on.
	 *
	 * XXX - should this ever be on the list of resolved triggers
	 * if it's being freed?
	 */
	lck_rw_lock_exclusive(resolved_triggers_rwlock);
	if (ti->ti_flags & TF_RESOLVED)
		TAILQ_REMOVE(&resolved_triggers, ti, ti_entries);
	lck_rw_unlock_exclusive(resolved_triggers_rwlock);
	lck_mtx_free(ti->ti_lock, triggers_lck_grp);
	FREE(ti, M_TRIGGERS);
}

int
SMBRemountServer(const void *ptr, size_t len, mach_port_t gssd_port)
{
	mach_port_t automount_port;
	int error;
	kern_return_t ret;
	vm_offset_t kmem_buf;
	vm_size_t tbuflen;
	vm_map_copy_t copy;

	error = auto_get_automountd_port(&automount_port);
	if (error)
		return (error);

	/*
	 * The blob has unbounded variable length.  Mach RPC does
	 * not pass the token in-line.  Instead it uses page mapping
	 * to handle these parameters.  We allocate a VM buffer
	 * to hold the token for an upcall and copies the token
	 * (received from the client) into it.  The VM buffer is
	 * marked with a src_destroy flag so that the upcall will
	 * automatically de-allocate the buffer when the upcall is
	 * complete.	 
	 */
	tbuflen = round_page(len);
	ret = vm_allocate(ipc_kernel_map, &kmem_buf, tbuflen,
	    VM_FLAGS_ANYWHERE);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs: vm_allocate failed, status 0x%08x\n", ret);
		/* XXX - deal with Mach errors */
		auto_release_port(automount_port);
		return (EIO);
	}
	ret = vm_map_wire(ipc_kernel_map, vm_map_trunc_page(kmem_buf),
	    vm_map_round_page(kmem_buf + tbuflen),
	    VM_PROT_READ|VM_PROT_WRITE, FALSE);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs: vm_map_wire failed, status 0x%08x\n", ret);
		/* XXX - deal with Mach errors */
		vm_deallocate(ipc_kernel_map, kmem_buf, tbuflen);
		auto_release_port(automount_port);
		return (EIO);
	}
	bcopy(ptr, (void *)kmem_buf, len);
	ret = vm_map_unwire(ipc_kernel_map, vm_map_trunc_page(kmem_buf),
	    vm_map_round_page(kmem_buf + tbuflen), FALSE);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs: vm_map_wire failed, status 0x%08x\n", ret);
		/* XXX - deal with Mach errors */
		vm_deallocate(ipc_kernel_map, kmem_buf, tbuflen);
		auto_release_port(automount_port);
		return (EIO);
	}
	ret = vm_map_copyin(ipc_kernel_map, (vm_map_address_t) kmem_buf,
	    (vm_map_size_t) tbuflen, TRUE, &copy);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs: vm_map_copyin failed, status 0x%08x\n", ret);
		/* XXX - deal with Mach errors */
		vm_deallocate(ipc_kernel_map, kmem_buf, tbuflen);
		auto_release_port(automount_port);
		return (EIO);
	}

	/*
	 * If you've passed us a blob whose size won't fit in a
	 * mach_msg_type_number_t, you've made a mistake.  Don't
	 * make mistakes.
	 */
	ret = autofs_smb_remount_server(automount_port, (byte_buffer) copy,
	    (mach_msg_type_number_t)len, gssd_port);
	auto_release_port(automount_port);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs: autofs_smb_remount_server failed, status 0x%08x\n",
		    ret);
		vm_map_copy_discard(copy);
		return (EIO);		/* XXX - process Mach errors */
	}

	return (0);
}

int
auto_get_automountd_port(mach_port_t *automount_port)
{
	kern_return_t ret;

	*automount_port = MACH_PORT_NULL;
	ret = host_get_automountd_port(host_priv_self(), automount_port);
	if (ret != KERN_SUCCESS) {
		IOLog("autofs: can't get automountd port, status 0x%08x\n",
		    ret);
		return (ECONNREFUSED);
	}
	if (!IPC_PORT_VALID(*automount_port)) {
		IOLog("autofs: automountd port not valid\n");
		return (ECONNRESET);
	}
	return (0);
}

void
auto_release_port(mach_port_t port)
{
	extern void ipc_port_release_send(ipc_port_t);
	
	ipc_port_release_send(port);
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

static void
trigger_update_ref_time(fsid_t *fsidp, time_t now)
{
	trigger_info_t *ti;

	lck_rw_lock_shared(resolved_triggers_rwlock);

	/*
	 * Look for an entry with the specified fsid and,
	 * if we find it, update it.
	 */
	TAILQ_FOREACH(ti, &resolved_triggers, ti_entries) {
		if (FSIDS_EQUAL(ti->ti_mounted_fsid, *fsidp)) {
			ti->ti_ref_time = now;
			break;
		}
	}

	lck_rw_unlock_shared(resolved_triggers_rwlock);
}

/*
 * Item in the information copied from the list of resolved triggers.
 */
struct triggered_mount_info {
	fsid_t	this_fsid;
	fsid_t	mounted_fsid;
	time_t	ref_time;
	u_int	flags;
};

/*
 * Look at all triggered mounts, and, for each mount, if it is an
 * unconditional operation or if it hasn't been referred to recently,
 * unmount it.
 */
void
unmount_triggered_mounts(int unconditional)
{
	trigger_info_t *ti;
	int nmounts, i, j;
	struct triggered_mount_info *mounts;
	struct timeval now;
	int error;
	fsid_t search_fsid;

	/*
	 * Make sure the list of resolved triggers doesn't change out
	 * from under us while we scan it.
	 */
	lck_rw_lock_shared(resolved_triggers_rwlock);

	/*
	 * Count how many resolved triggers we have.
	 */
	nmounts = 0;
	TAILQ_FOREACH_REVERSE(ti, &resolved_triggers, resolved_triggers,
	    ti_entries) {
		nmounts++;
	}

	/*
	 * Allocate an array of fsids and timeout information for
	 * all those mounts.
	 */
	MALLOC(mounts, struct triggered_mount_info *, sizeof (*mounts)*nmounts,
	    M_TRIGGERS, M_WAITOK);

	/*
	 * Make a copy of the mount information for all the triggered
	 * mounts.
	 */
	i = 0;
	TAILQ_FOREACH_REVERSE(ti, &resolved_triggers, resolved_triggers,
	    ti_entries) {
		if (i >= nmounts) {
			/*
			 * This "cannot happen".
			 */
			IOLog("unmount_triggers: resolved_triggers changed out from under us!\n");
			break;
		}

		mounts[i].this_fsid = ti->ti_this_fsid;
		mounts[i].mounted_fsid = ti->ti_mounted_fsid;

		/*
		 * We want the last referenced time for the trigger
		 * for this.
		 */
		mounts[i].ref_time = ti->ti_ref_time;
		mounts[i].flags = ti->ti_flags;
		i++;
	}

	/*
	 * We can now release the rwlock, as we're done looking at
	 * the list of triggered mounts.  Unmounts we do might change
	 * it out from under us - as might other unmounts done while
	 * we're working.
	 */
	lck_rw_unlock_shared(resolved_triggers_rwlock);

	/*
	 * Now work on the snapshot we took of the triggered mounts.
	 * If something gets unmounted before we look at it,
	 * that fsid will no longer have a file system corresponding
	 * to it, and there's nothing to unmount.  If something gets
	 * mounted after we took the snapshot, we miss it; short of
	 * blocking all mounts while we're doing the unmount, there's
	 * not much we can do about that, and even if we do block all
	 * mounts while we're doing the unmount, something might get
	 * mounted after we're done.
	 */
	microtime(&now);
	for (i = nmounts - 1; i >= 0; i--) {
		/*
		 * If this is an autofs mount, don't unmount it;
		 * that'll happen automatically if we unmount what it's
		 * mounted on.
		 */
		if (mounts[i].flags & TF_AUTOFS)
			continue;

		error = 0;	/* assume we will be unmounting this */
		if (mounts[i].flags & TF_DONTUNMOUNT) {
			/*
			 * This is a "don't ever auto-unmount this"
			 * mount; skip it.
			 */
			error = EBUSY;
		} else {
			if (unconditional) {
				/*
				 * This is a "preemptively unmount
				 * everything you can" unmount;
				 * if this is a "don't preemptively
				 * unmount this" mount, skip it.
				 */
				if (mounts[i].flags & TF_DONTPREUNMOUNT)
					error = EBUSY;
			} else {
				/*
				 * This is an auto-unmount; skip it
				 * if it's been referenced recently.
				 */
				if (mounts[i].ref_time + mount_to > now.tv_sec)
					error = EBUSY;
			}
		}
		if (error == 0) {
			/*
			 * OK, we should at least try to unmount this.
			 */
			error = unmount_triggered_mount(&mounts[i].mounted_fsid,
			    0, vfs_context_current());
			if (error == EBUSY) {
				/*
				 * The file system is apparently still
				 * in use, so update its reference
				 * time.
				 */
				trigger_update_ref_time(&mounts[i].mounted_fsid,
				    now.tv_sec);
			}
		}

		if (error == EBUSY) {
			/*
			 * We deemed that file system to be "recently
			 * used" (either referred to recently or currently
			 * in use), or concluded that it shouldn't be
			 * unmounted, so, if it's mounted (directly or
			 * indirectly) atop another triggered mount, mark
			 * that mount as "recently used", so we don't
			 * try to unmount it (which will cause the
			 * VFS layer to try to unmount everything
			 * mounted atop it).
			 *
			 * We scan all triggered mounts below this one
			 * to find one that has a mounted-on fsid equal
			 * to the fsid of this mount.  If that's not
			 * an autofs mount, we've found it, otherwise
			 * we keep searching backwards, now looking
			 * for one with a mounted-on fsid equal to the
			 * fsid of the autofs mount.
			 */
			search_fsid = mounts[i].this_fsid;
			for (j = i - 1; j >= 0; j--) {
				if (FSIDS_EQUAL(mounts[j].mounted_fsid,
				    search_fsid)) {
					if (mounts[j].flags & TF_AUTOFS) {
						/*
						 * OK, what's *this* mounted
						 * on?
						 */
						search_fsid = mounts[j].mounted_fsid;
					} else {
						mounts[j].ref_time = now.tv_sec;
						break;
					}
				}
			}
		}
	}

	/*
	 * Release the Kraken^Wcopy.
	 */
	FREE(mounts, M_TRIGGERS);
}

static lck_mtx_t *unmount_threads_lock;
static int unmount_threads;
static thread_call_t unmounter_thread_call;
static int shutting_down;

/*
 * max number of unmount threads running
 */
static int max_unmount_threads = 5;

static void
decrement_unmount_thread_count(void)
{
	lck_mtx_lock(unmount_threads_lock);
	unmount_threads--;

	/*
	 * If we have no more threads running, and we're shutting
	 * down, wake up anybody waiting for us to run out of
	 * unmount threads.
	 */
	if (shutting_down && unmount_threads == 0)
		wakeup(&unmount_threads);
	lck_mtx_unlock(unmount_threads_lock);
}

/*
 * Time between unmount_triggered_mounts() calls, in seconds.
 */
#define UNMOUNT_TRIGGERED_MOUNTS_TIMER	120	/* 2 minutes */

static void
triggers_unmount_thread(__unused void *arg)
{
	unmount_triggered_mounts(0);
	decrement_unmount_thread_count();

	thread_terminate(current_thread());
	/* NOTREACHED */
}

static void
triggers_do_unmount(__unused void *dummy1, __unused void *dummy2)
{
	kern_return_t result;
	AbsoluteTime deadline;

	/*
	 * If we don't already have too many unmount threads running,
	 * and if we aren't trying to shut down the triggers kext,
	 * attempt an unmount, otherwise just quit.
	 *
	 * We attempt the unmount in a separate thread, so as not to
	 * block the thread doing thread calls.
	 */
	lck_mtx_lock(unmount_threads_lock);
	if (!shutting_down && unmount_threads < max_unmount_threads) {
		unmount_threads++;
		lck_mtx_unlock(unmount_threads_lock);

		result = auto_new_thread(triggers_unmount_thread, NULL);
		if (result != KERN_SUCCESS) {
			IOLog("triggers_do_unmount: Couldn't start unmount thread, status 0x%08x\n",
			    result);

			/*
			 * Undo the thread count increment, as the
			 * thread wasn't created.
			 */
			decrement_unmount_thread_count();
		}
	} else
		lck_mtx_unlock(unmount_threads_lock);

	/*
	 * Schedule the next unmount attempt for UNMOUNT_TRIGGERED_MOUNTS_TIMER
	 * seconds from now.
	 */
	clock_interval_to_deadline(UNMOUNT_TRIGGERED_MOUNTS_TIMER, NSEC_PER_SEC,
	    &deadline);
	thread_call_enter_delayed(unmounter_thread_call, deadline);
}

/*
 * Initialize the module
 */
__private_extern__ int
triggers_start(__unused kmod_info_t *ki, __unused void *data)
{
	AbsoluteTime deadline;

	/*
	 * Set up the lock group.
	 */
	triggers_lck_grp = lck_grp_alloc_init("triggers", NULL);
	if (triggers_lck_grp == NULL) {
		IOLog("triggers_start: Couldn't create triggers lock group\n");
		goto fail;
	}

	resolved_triggers_rwlock = lck_rw_alloc_init(triggers_lck_grp, NULL);
	if (resolved_triggers_rwlock == NULL) {
		IOLog("triggers_start: Couldn't create resolved triggers list lock\n");
		goto fail;
	}

	shutting_down = 0;

	unmount_threads_lock = lck_mtx_alloc_init(triggers_lck_grp, LCK_ATTR_NULL);
	if (unmount_threads_lock == NULL) {
		IOLog("triggers_start: Couldn't create unmount threads lock\n");
		goto fail;
	}
	unmount_threads = 0;
	unmounter_thread_call = thread_call_allocate(triggers_do_unmount, NULL);
	if (unmounter_thread_call == NULL) {
		IOLog("triggers_start: Couldn't create thread_call_t for unmounter thread\n");
		goto fail;
	}

	TAILQ_INIT(&resolved_triggers);

	/*
	 * Schedule the unmounter thread to run UNMOUNT_TRIGGERED_MOUNTS_TIMER
	 * seconds from now.
	 */
	clock_interval_to_deadline(UNMOUNT_TRIGGERED_MOUNTS_TIMER, NSEC_PER_SEC,
	    &deadline);
	thread_call_enter_delayed(unmounter_thread_call, deadline);

	return (KERN_SUCCESS);

fail:
	if (unmounter_thread_call != NULL)
		thread_call_free(unmounter_thread_call);
	if (unmount_threads_lock != NULL)
		lck_mtx_free(unmount_threads_lock, triggers_lck_grp);
	if (resolved_triggers_rwlock != NULL)
		lck_rw_free(resolved_triggers_rwlock, triggers_lck_grp);
	if (triggers_lck_grp != NULL)
		lck_grp_free(triggers_lck_grp);
	return (KERN_FAILURE);
}

__private_extern__ int
triggers_stop(__unused kmod_info_t *ki, __unused void *data)
{
	/*
	 * We cannot be unloaded if any kext that depends on us is
	 * loaded; we rely on those kexts not to be unloaded if
	 * they have any vnodes, so if we're being unloaded, they
	 * have no trigger vnodes, so we know that no mounts will
	 * be triggered, so there won't be any *new* triggered
	 * mounts.
	 *
	 * However, there might still be resolved triggers, if the file
	 * systems with the triggers in question were forcibly unmounted,
	 * so we must check whether there are any resolved triggers left.
	 *
	 * XXX - should check for *any* triggers!
	 */
	lck_rw_lock_shared(resolved_triggers_rwlock);
	if (!TAILQ_EMPTY(&resolved_triggers)) {
		lck_rw_unlock_shared(resolved_triggers_rwlock);
		IOLog("triggers_stop: Can't remove, still some resolved triggers\n");
		return (KERN_NO_ACCESS);
	}

	/*
	 * Cancel any queued unmount calls.
	 */
	thread_call_cancel(unmounter_thread_call);

	/*
	 * Wait until there are no unmount threads running.
	 */
	lck_mtx_lock(unmount_threads_lock);
	shutting_down = 1;
	while (unmount_threads != 0) {
		msleep(&unmount_threads, unmount_threads_lock, PWAIT,
		    "unmount thread terminated", NULL);
	}
	lck_mtx_unlock(unmount_threads_lock);
	lck_mtx_free(unmount_threads_lock, triggers_lck_grp);

	lck_rw_unlock_exclusive(resolved_triggers_rwlock);
	lck_rw_free(resolved_triggers_rwlock, triggers_lck_grp);
	lck_grp_free(triggers_lck_grp);

	return (KERN_SUCCESS);
}
