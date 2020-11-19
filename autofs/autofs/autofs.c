/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
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

//  Created by aburlyga on 4/8/20.

#include <IOKit/IOLib.h>

#include <kern/locks.h>

#include <mach/mach_types.h>

#include <miscfs/devfs/devfs.h>

#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/vnode.h> /*XXXab: this one needed only because of triggers.h below*/

#include "triggers.h"
#include "autofs.h"
#include "autofs_kern.h"

#pragma mark externs
extern u_int autofs_mounts;
extern int   automounter_pid;
extern int   autofs_control_isopen;

static int autofs_major = -1;
static int autofs_nowait_major = -1;
static int autofs_notrigger_major = -1;
static int autofs_homedirmounter_major = -1;
static int autofs_control_major = -1;

static void *autofs_devfs;
static void *autofs_nowait_devfs;
static void *autofs_notrigger_devfs;
static void *autofs_homedirmounter_devfs;
static void *autofs_control_devfs;

extern struct cdevsw autofs_cdevsw;
extern struct cdevsw autofs_nowait_cdevsw;
extern struct cdevsw autofs_notrigger_cdevsw;
extern struct cdevsw autofs_homedirmounter_cdevsw;
extern struct cdevsw autofs_control_cdevsw;

extern struct vfs_fsentry autofs_fsentry;
static vfstable_t auto_vfsconf;

/* XXXab: HACK! fix it properly, by moving sysctl related stuff here */
extern struct sysctl_oid sysctl__vfs_generic_autofs;
extern struct sysctl_oid sysctl__vfs_generic_autofs_vnode_recycle_on_inactive;

struct tracked_process {
	LIST_ENTRY(tracked_process) entries;
	int	pid;			/* PID of the nowait process */
	int	minor;			/* minor device they opened */
};
LIST_HEAD(tracked_process_list, tracked_process);

extern struct tracked_process_list nowait_processes;
extern struct tracked_process_list notrigger_processes;
extern struct tracked_process_list homedirmounter_processes;

extern lck_grp_t *autofs_lck_grp;
extern lck_mtx_t *autofs_global_lock;
extern lck_mtx_t *autofs_nodeid_lock;
extern lck_rw_t  *autofs_automounter_pid_rwlock;
extern lck_rw_t  *autofs_nowait_processes_rwlock;
extern lck_rw_t  *autofs_notrigger_processes_rwlock;
extern lck_mtx_t *autofs_control_isopen_lock;
extern lck_rw_t  *autofs_homedirmounter_processes_rwlock;

extern int autofs_nowait_dev_clone(dev_t, int);
extern int autofs_notrigger_dev_clone(dev_t, int);
extern int autofs_homedirmounter_dev_clone(dev_t, int);
extern struct autofs_globals *autofs_zone_get_globals(void);

#pragma mark declarations
kern_return_t autofs_start(kmod_info_t * ki, void *d);
kern_return_t autofs_stop(kmod_info_t *ki, void *d);

#pragma mark definitions
kern_return_t
autofs_start(kmod_info_t * ki, void *d)
{
		errno_t error;

		/*
		 * Set up the lock group and the global locks.
		 */
		autofs_lck_grp = lck_grp_alloc_init("autofs", NULL);
		if (autofs_lck_grp == NULL) {
			IOLog("auto_module_start: Couldn't create autofs lock group\n");
			goto fail;
		}
		autofs_global_lock = lck_mtx_alloc_init(autofs_lck_grp, LCK_ATTR_NULL);
		if (autofs_global_lock == NULL) {
			IOLog("auto_module_start: Couldn't create autofs global lock\n");
			goto fail;
		}
		autofs_nodeid_lock = lck_mtx_alloc_init(autofs_lck_grp, LCK_ATTR_NULL);
		if (autofs_nodeid_lock == NULL) {
			IOLog("auto_module_start: Couldn't create autofs node ID lock\n");
			goto fail;
		}
		autofs_automounter_pid_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
		if (autofs_automounter_pid_rwlock == NULL) {
			IOLog("auto_module_start: Couldn't create autofs automounter pid lock\n");
			goto fail;
		}
		autofs_nowait_processes_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
		if (autofs_nowait_processes_rwlock == NULL) {
			IOLog("auto_module_start: Couldn't create autofs nowait_processes list lock\n");
			goto fail;
		}
		autofs_notrigger_processes_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
		if (autofs_notrigger_processes_rwlock == NULL) {
			IOLog("auto_module_start: Couldn't create autofs notrigger_processes list lock\n");
			goto fail;
		}
		autofs_homedirmounter_processes_rwlock = lck_rw_alloc_init(autofs_lck_grp, NULL);
		if (autofs_homedirmounter_processes_rwlock == NULL) {
			IOLog("auto_module_start: Couldn't create autofs homedirmounter_processes list lock\n");
			goto fail;
		}
		autofs_control_isopen_lock = lck_mtx_alloc_init(autofs_lck_grp, LCK_ATTR_NULL);
		if (autofs_control_isopen_lock == NULL) {
			IOLog("auto_module_start: Couldn't create autofs control device lock\n");
			goto fail;
		}

		/*
		 * Add the autofs device.
		 */
		autofs_major = cdevsw_add(-1, &autofs_cdevsw);
		if (autofs_major == -1) {
			IOLog("auto_module_start: cdevsw_add failed on autofs device\n");
			goto fail;
		}
		autofs_devfs = devfs_make_node(makedev(autofs_major, 0),
			DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600, AUTOFS_DEVICE);
		if (autofs_devfs == NULL) {
			IOLog("auto_module_start: devfs_make_node failed on autofs device\n");
			goto fail;
		}

		/*
		 * Add the autofs nowait device.  Everybody's allowed to open it.
		 */
		autofs_nowait_major = cdevsw_add(-1, &autofs_nowait_cdevsw);
		if (autofs_nowait_major == -1) {
			IOLog("auto_module_start: cdevsw_add failed on autofs_nowait device\n");
			goto fail;
		}
		autofs_nowait_devfs = devfs_make_node_clone(makedev(autofs_nowait_major, 0),
			DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, autofs_nowait_dev_clone,
			AUTOFS_NOWAIT_DEVICE);
		if (autofs_nowait_devfs == NULL) {
			IOLog("auto_module_start: devfs_make_node failed on autofs nowait device\n");
			goto fail;
		}

		/*
		 * Add the autofs notrigger device.  Everybody's allowed to open it.
		 */
		autofs_notrigger_major = cdevsw_add(-1, &autofs_notrigger_cdevsw);
		if (autofs_notrigger_major == -1) {
			IOLog("auto_module_start: cdevsw_add failed on autofs_notrigger device\n");
			goto fail;
		}
		autofs_notrigger_devfs = devfs_make_node_clone(makedev(autofs_notrigger_major, 0),
			DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, autofs_notrigger_dev_clone,
			AUTOFS_NOTRIGGER_DEVICE);
		if (autofs_notrigger_devfs == NULL) {
			IOLog("auto_module_start: devfs_make_node failed on autofs notrigger device\n");
			goto fail;
		}

		/*
		 * Add the autofs homedirmounter device.
		 */
		autofs_homedirmounter_major = cdevsw_add(-1, &autofs_homedirmounter_cdevsw);
		if (autofs_homedirmounter_major == -1) {
			IOLog("auto_module_start: cdevsw_add failed on autofs_homedirmounter device\n");
			goto fail;
		}
		autofs_homedirmounter_devfs = devfs_make_node_clone(makedev(autofs_homedirmounter_major, 0),
			DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, autofs_homedirmounter_dev_clone,
			AUTOFS_HOMEDIRMOUNTER_DEVICE);
		if (autofs_homedirmounter_devfs == NULL) {
			IOLog("auto_module_start: devfs_make_node failed on autofs homedirmounter device\n");
			goto fail;
		}

		/*
		 * Add the autofs control device.
		 */
		autofs_control_major = cdevsw_add(-1, &autofs_control_cdevsw);
		if (autofs_control_major == -1) {
			IOLog("auto_module_start: cdevsw_add failed on autofs control device\n");
			goto fail;
		}
		autofs_control_devfs = devfs_make_node(makedev(autofs_control_major, 0),
			DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600, AUTOFS_CONTROL_DEVICE);
		if (autofs_control_devfs == NULL) {
			IOLog("auto_module_start: devfs_make_node failed on autofs control device\n");
			goto fail;
		}

		/*
		 * Register the file system.
		 */
		error = vfs_fsadd(&autofs_fsentry, &auto_vfsconf);
		if (error != 0) {
			IOLog("auto_module_start: Error %d from vfs_fsadd\n", error);
			goto fail;
		}

		/*
		 * Register sysctl on success
		 */
		sysctl_register_oid(&sysctl__vfs_generic_autofs);
		sysctl_register_oid(&sysctl__vfs_generic_autofs_vnode_recycle_on_inactive);

		return (KERN_SUCCESS);

	fail:
		if (autofs_control_devfs != NULL)
			devfs_remove(autofs_control_devfs);
		if (autofs_control_major != -1) {
			if (cdevsw_remove(autofs_control_major, &autofs_control_cdevsw) == -1)
				panic("auto_module_start: can't remove autofs control device from cdevsw");
		}
		if (autofs_nowait_devfs != NULL)
			devfs_remove(autofs_nowait_devfs);
		if (autofs_nowait_major != -1) {
			if (cdevsw_remove(autofs_nowait_major, &autofs_nowait_cdevsw) == -1)
				panic("auto_module_start: can't remove autofs nowait device from cdevsw");
		}
		if (autofs_notrigger_devfs != NULL)
			devfs_remove(autofs_notrigger_devfs);
		if (autofs_notrigger_major != -1) {
			if (cdevsw_remove(autofs_notrigger_major, &autofs_notrigger_cdevsw) == -1)
				panic("auto_module_start: can't remove autofs notrigger device from cdevsw");
		}
		if (autofs_homedirmounter_devfs != NULL)
			devfs_remove(autofs_homedirmounter_devfs);
		if (autofs_homedirmounter_major != -1) {
			if (cdevsw_remove(autofs_homedirmounter_major, &autofs_homedirmounter_cdevsw) == -1)
				panic("auto_module_start: can't remove autofs homedirmounter device from cdevsw");
		}
		if (autofs_devfs != NULL)
			devfs_remove(autofs_devfs);
		if (autofs_major != -1) {
			if (cdevsw_remove(autofs_major, &autofs_cdevsw) == -1)
				panic("auto_module_start: can't remove autofs device from cdevsw");
		}
		if (autofs_control_isopen_lock != NULL)
			lck_mtx_free(autofs_control_isopen_lock, autofs_lck_grp);
		if (autofs_nowait_processes_rwlock != NULL)
			lck_rw_free(autofs_nowait_processes_rwlock, autofs_lck_grp);
		if (autofs_notrigger_processes_rwlock != NULL)
			lck_rw_free(autofs_notrigger_processes_rwlock, autofs_lck_grp);
		if (autofs_homedirmounter_processes_rwlock != NULL)
			lck_rw_free(autofs_homedirmounter_processes_rwlock, autofs_lck_grp);
		if (autofs_automounter_pid_rwlock != NULL)
			lck_rw_free(autofs_automounter_pid_rwlock, autofs_lck_grp);
		if (autofs_nodeid_lock != NULL)
			lck_mtx_free(autofs_nodeid_lock, autofs_lck_grp);
		if (autofs_global_lock != NULL)
			lck_mtx_free(autofs_global_lock, autofs_lck_grp);
		if (autofs_lck_grp != NULL)
			lck_grp_free(autofs_lck_grp);
		return (KERN_FAILURE);
}

kern_return_t autofs_stop(kmod_info_t *ki, void *d)
{
	struct autofs_globals *fngp;
	int error;

	lck_mtx_lock(autofs_global_lock);
	lck_mtx_lock(autofs_nodeid_lock);
	lck_rw_lock_exclusive(autofs_automounter_pid_rwlock);
	lck_rw_lock_exclusive(autofs_nowait_processes_rwlock);
	lck_rw_lock_exclusive(autofs_notrigger_processes_rwlock);
	lck_rw_lock_exclusive(autofs_homedirmounter_processes_rwlock);
	lck_mtx_lock(autofs_control_isopen_lock);
	if (autofs_mounts != 0) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, still %u mounts active\n", autofs_mounts));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_nodeid_lock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (automounter_pid != 0) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, automounter still running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_nodeid_lock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (!LIST_EMPTY(&nowait_processes)) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, still nowait processes running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_nodeid_lock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (!LIST_EMPTY(&notrigger_processes)) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, still notrigger processes running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_nodeid_lock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (!LIST_EMPTY(&homedirmounter_processes)) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, still homedirmounter processes running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_nodeid_lock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	if (autofs_control_isopen) {
		AUTOFS_DPRINT((2, "auto_module_stop: Can't remove, automount command is running\n"));
		lck_mtx_unlock(autofs_control_isopen_lock);
		lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
		lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
		lck_mtx_unlock(autofs_nodeid_lock);
		lck_mtx_unlock(autofs_global_lock);
		return (KERN_NO_ACCESS);
	}
	AUTOFS_DPRINT((10, "auto_module_stop: removing autofs from vfs conf. list...\n"));

	fngp = autofs_zone_get_globals();
	if (fngp) {
		assert(fngp->fng_fnnode_count == 1);
	}

	error = vfs_fsremove(auto_vfsconf);
	if (error) {
		IOLog("auto_module_stop: Error %d from vfs_remove\n",
		    error);
		return (KERN_FAILURE);
	}

	devfs_remove(autofs_devfs);
	autofs_devfs = NULL;
	if (cdevsw_remove(autofs_major, &autofs_cdevsw) == -1)
		panic("auto_module_stop: can't remove autofs device from cdevsw");
	autofs_major = -1;
	devfs_remove(autofs_nowait_devfs);
	autofs_nowait_devfs = NULL;
	if (cdevsw_remove(autofs_nowait_major, &autofs_nowait_cdevsw) == -1)
		panic("auto_module_stop: can't remove autofs nowait device from cdevsw");
	autofs_nowait_major = -1;
	devfs_remove(autofs_notrigger_devfs);
	autofs_notrigger_devfs = NULL;
	if (cdevsw_remove(autofs_notrigger_major, &autofs_notrigger_cdevsw) == -1)
		panic("auto_module_stop: can't remove autofs notrigger device from cdevsw");
	autofs_notrigger_major = -1;
	devfs_remove(autofs_homedirmounter_devfs);
	autofs_homedirmounter_devfs = NULL;
	if (cdevsw_remove(autofs_homedirmounter_major, &autofs_homedirmounter_cdevsw) == -1)
		panic("auto_module_stop: can't remove autofs homedirmounter device from cdevsw");
	autofs_homedirmounter_major = -1;
	devfs_remove(autofs_control_devfs);
	autofs_control_devfs = NULL;
	if (cdevsw_remove(autofs_control_major, &autofs_control_cdevsw) == -1)
		panic("auto_module_start: can't remove autofs control device from cdevsw");

	if (fngp) {
		/*
		 * Free up the root fnnode.
		 */
		FREE(fngp->fng_rootfnnodep->fn_name, M_AUTOFS);
		FREE(fngp->fng_rootfnnodep, M_AUTOFS);

		lck_mtx_free(fngp->fng_flush_notification_lock, autofs_lck_grp);

		if (fngp->fng_rootfnnodep->fn_mnt_lock != NULL)
			lck_mtx_free(fngp->fng_rootfnnodep->fn_mnt_lock, autofs_lck_grp);
		if (fngp->fng_rootfnnodep->fn_rwlock != NULL)
			lck_rw_free(fngp->fng_rootfnnodep->fn_rwlock, autofs_lck_grp);
		if (fngp->fng_rootfnnodep->fn_lock != NULL)
			lck_mtx_free(fngp->fng_rootfnnodep->fn_lock, autofs_lck_grp);

                FREE(fngp, M_AUTOFS);
        }

	lck_mtx_unlock(autofs_nodeid_lock);
	lck_mtx_free(autofs_nodeid_lock, autofs_lck_grp);
	lck_mtx_unlock(autofs_global_lock);
	lck_mtx_free(autofs_global_lock, autofs_lck_grp);
	lck_rw_unlock_exclusive(autofs_automounter_pid_rwlock);
	lck_rw_free(autofs_automounter_pid_rwlock, autofs_lck_grp);
	lck_rw_unlock_exclusive(autofs_nowait_processes_rwlock);
	lck_rw_free(autofs_nowait_processes_rwlock, autofs_lck_grp);
	lck_rw_unlock_exclusive(autofs_notrigger_processes_rwlock);
	lck_rw_free(autofs_notrigger_processes_rwlock, autofs_lck_grp);
	lck_rw_unlock_exclusive(autofs_homedirmounter_processes_rwlock);
	lck_rw_free(autofs_homedirmounter_processes_rwlock, autofs_lck_grp);
	lck_mtx_unlock(autofs_control_isopen_lock);
	lck_mtx_free(autofs_control_isopen_lock, autofs_lck_grp);
	lck_grp_free(autofs_lck_grp);

	sysctl_unregister_oid(&sysctl__vfs_generic_autofs_vnode_recycle_on_inactive);
	sysctl_unregister_oid(&sysctl__vfs_generic_autofs);

	return (KERN_SUCCESS);
}
