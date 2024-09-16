/*
 * Copyright (c) 2000-2024 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */


#include <kern/kern_types.h>
#include <kern/thread_group.h>
#include <mach/mach_types.h>
#include <mach/boolean.h>

#include <kern/coalition.h>

#include <sys/coalition.h>
#include <sys/coalition_private.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/ubc.h> /* mach_to_bsd_errno */

#include <kern/policy_internal.h>

#include <IOKit/IOBSD.h> /* IOTaskHasEntitlement */

/* Coalitions syscalls */

/*
 * Create a new, empty coalition and return its ID.
 *
 * Returns:
 * EINVAL	Flags parameter was invalid
 * ENOMEM	Unable to allocate kernel resources for a new coalition
 * EFAULT	cidp parameter pointed to invalid memory.
 *
 * Returns with reference held for userspace caller.
 */
static
int
coalition_create_syscall(user_addr_t cidp, uint32_t flags)
{
	int error = 0;
	kern_return_t kr;
	uint64_t cid;
	coalition_t coal;
	int type = COALITION_CREATE_FLAGS_GET_TYPE(flags);
	int role = COALITION_CREATE_FLAGS_GET_ROLE(flags);
	boolean_t privileged = !!(flags & COALITION_CREATE_FLAGS_PRIVILEGED);
	boolean_t efficient = !!(flags & COALITION_CREATE_FLAGS_EFFICIENT);

	if ((flags & (~COALITION_CREATE_FLAGS_MASK)) != 0) {
		return EINVAL;
	}
	if (type < 0 || type > COALITION_TYPE_MAX) {
		return EINVAL;
	}

	kr = coalition_create_internal(type, role, privileged, efficient, &coal, &cid);
	if (kr != KERN_SUCCESS) {
		/* for now, the only kr is KERN_RESOURCE_SHORTAGE */
		error = ENOMEM;
		goto out;
	}

	coal_dbg("(addr, %u) -> %llu", flags, cid);
	error = copyout(&cid, cidp, sizeof(cid));
out:
	return error;
}

/*
 * Request to terminate the coalition identified by ID.
 * Attempts to spawn into this coalition using the posix_spawnattr will begin
 * failing. Processes already within the coalition may still fork.
 * Arms the 'coalition is empty' notification when the coalition's active
 * count reaches zero.
 *
 * Returns:
 * ESRCH	No coalition with that ID could be found.
 * EALREADY	The coalition with that ID has already been terminated.
 * EFAULT	cidp parameter pointed to invalid memory.
 * EPERM	Caller doesn't have permission to terminate that coalition.
 */
static
int
coalition_request_terminate_syscall(user_addr_t cidp, uint32_t flags)
{
	kern_return_t kr;
	int error = 0;
	uint64_t cid;
	coalition_t coal;

	if (flags != 0) {
		return EINVAL;
	}

	error = copyin(cidp, &cid, sizeof(cid));
	if (error) {
		return error;
	}

	coal = coalition_find_by_id(cid);
	if (coal == COALITION_NULL) {
		return ESRCH;
	}

	kr = coalition_request_terminate_internal(coal);
	coalition_release(coal);

	switch (kr) {
	case KERN_SUCCESS:
		break;
	case KERN_DEFAULT_SET:
		error = EPERM;
		break;
	case KERN_TERMINATED:
		error = EALREADY;
		break;
	case KERN_INVALID_NAME:
		error = ESRCH;
		break;
	default:
		error = EIO;
		break;
	}

	coal_dbg("(%llu, %u) -> %d", cid, flags, error);

	return error;
}

/*
 * Request the kernel to deallocate the coalition identified by ID, which
 * must be both terminated and empty. This balances the reference taken
 * in coalition_create.
 * The memory containing the coalition object may not be freed just yet, if
 * other kernel operations still hold references to it.
 *
 * Returns:
 * EINVAL	Flags parameter was invalid
 * ESRCH	Coalition ID refers to a coalition that doesn't exist.
 * EBUSY	Coalition has not yet been terminated.
 * EBUSY	Coalition is still active.
 * EFAULT	cidp parameter pointed to invalid memory.
 * EPERM	Caller doesn't have permission to terminate that coalition.
 * Consumes one reference, "held" by caller since coalition_create
 */
static
int
coalition_reap_syscall(user_addr_t cidp, uint32_t flags)
{
	kern_return_t kr;
	int error = 0;
	uint64_t cid;
	coalition_t coal;

	if (flags != 0) {
		return EINVAL;
	}

	error = copyin(cidp, &cid, sizeof(cid));
	if (error) {
		return error;
	}

	coal = coalition_find_by_id(cid);
	if (coal == COALITION_NULL) {
		return ESRCH;
	}

	kr = coalition_reap_internal(coal);
	coalition_release(coal);

	switch (kr) {
	case KERN_SUCCESS:
		break;
	case KERN_DEFAULT_SET:
		error = EPERM;
		break;
	case KERN_TERMINATED:
		error = ESRCH;
		break;
	case KERN_FAILURE:
		error = EBUSY;
		break;
	default:
		error = EIO;
		break;
	}

	coal_dbg("(%llu, %u) -> %d", cid, flags, error);

	return error;
}

/* Syscall demux.
 * Returns EPERM if the calling process is not privileged to make this call.
 */
int
coalition(proc_t p, struct coalition_args *cap, __unused int32_t *retval)
{
	uint32_t operation = cap->operation;
	user_addr_t cidp = cap->cid;
	uint32_t flags = cap->flags;
	int error = 0;
	int type = COALITION_CREATE_FLAGS_GET_TYPE(flags);

	if (!task_is_in_privileged_coalition(proc_task(p), type)) {
		return EPERM;
	}

	switch (operation) {
	case COALITION_OP_CREATE:
		error = coalition_create_syscall(cidp, flags);
		break;
	case COALITION_OP_REAP:
		error = coalition_reap_syscall(cidp, flags);
		break;
	case COALITION_OP_TERMINATE:
		error = coalition_request_terminate_syscall(cidp, flags);
		break;
	default:
		error = ENOSYS;
	}
	return error;
}

/* This is a temporary interface, likely to be changed by 15385642. */
static int __attribute__ ((noinline))
coalition_info_resource_usage(coalition_t coal, user_addr_t buffer, user_size_t bufsize)
{
	kern_return_t kr;
	struct coalition_resource_usage cru = {};

	kr = coalition_resource_usage_internal(coal, &cru);

	switch (kr) {
	case KERN_INVALID_ARGUMENT:
		return EINVAL;
	case KERN_RESOURCE_SHORTAGE:
		return ENOMEM;
	case KERN_SUCCESS:
		break;
	default:
		return EIO; /* shrug */
	}

	return copyout(&cru, buffer, MIN(bufsize, sizeof(cru)));
}

#if DEVELOPMENT || DEBUG
static int __attribute__ ((noinline))
coalition_info_get_debug_info(coalition_t coal, user_addr_t buffer, user_size_t bufsize)
{
	kern_return_t kr;
	struct coalinfo_debuginfo c_debuginfo = {};

	kr = coalition_debug_info_internal(coal, &c_debuginfo);

	if (kr != KERN_SUCCESS) {
		return mach_to_bsd_errno(kr);
	}

	return copyout(&c_debuginfo, buffer, MIN(bufsize, sizeof(c_debuginfo)));
}
#endif /* DEVELOPMENT || DEBUG */

#if CONFIG_THREAD_GROUPS
static int
coalition_info_set_name_internal(coalition_t coal, user_addr_t buffer, user_size_t bufsize)
{
	int error;
	char name[THREAD_GROUP_MAXNAME];

	if (coalition_type(coal) != COALITION_TYPE_JETSAM) {
		return EINVAL;
	}
	bzero(name, sizeof(name));
	error = copyin(buffer, name, MIN(bufsize, sizeof(name) - 1));
	if (error) {
		return error;
	}
	struct thread_group *tg = coalition_get_thread_group(coal);
	thread_group_set_name(tg, name);
	return error;
}

#else /* CONFIG_THREAD_GROUPS */
#define coalition_info_set_name_internal(...) 0
#endif /* CONFIG_THREAD_GROUPS */

static int
coalition_info_efficiency(coalition_t coal, user_addr_t buffer, user_size_t bufsize)
{
	int error = 0;
	if (coalition_type(coal) != COALITION_TYPE_JETSAM) {
		return EINVAL;
	}
	uint64_t flags = 0;
	error = copyin(buffer, &flags, MIN(bufsize, sizeof(flags)));
	if (error) {
		return error;
	}
	if ((flags & COALITION_EFFICIENCY_VALID_FLAGS) == 0) {
		return EINVAL;
	}
	if (flags & COALITION_FLAGS_EFFICIENT) {
		// No longer supported; this flag must be set during create.
		return ENOTSUP;
	}
	return error;
}

static int
coalition_ledger_logical_writes_limit(coalition_t coal, user_addr_t buffer, user_size_t bufsize)
{
	int error = 0;
	int64_t limit = 0;

	if (coalition_type(coal) != COALITION_TYPE_RESOURCE) {
		error = EINVAL;
		goto out;
	}
	error = copyin(buffer, &limit, MIN(bufsize, sizeof(limit)));
	if (error) {
		goto out;
	}


	error = coalition_ledger_set_logical_writes_limit(coal, limit);
out:
	return error;
}

int
coalition_info(proc_t p, struct coalition_info_args *uap, __unused int32_t *retval)
{
	user_addr_t cidp = uap->cid;
	user_addr_t buffer = uap->buffer;
	user_addr_t bufsizep = uap->bufsize;
	user_size_t bufsize;
	uint32_t flavor = uap->flavor;
	int error;
	uint64_t cid;
	coalition_t coal;

	error = copyin(cidp, &cid, sizeof(cid));
	if (error) {
		return error;
	}

	coal = coalition_find_by_id(cid);
	if (coal == COALITION_NULL) {
		return ESRCH;
	}
	/* TODO: priv check? EPERM or ESRCH? */

	if (IS_64BIT_PROCESS(p)) {
		user64_size_t size64;
		error = copyin(bufsizep, &size64, sizeof(size64));
		bufsize = (user_size_t)size64;
	} else {
		user32_size_t size32;
		error = copyin(bufsizep, &size32, sizeof(size32));
		bufsize = (user_size_t)size32;
	}
	if (error) {
		goto bad;
	}

	switch (flavor) {
	case COALITION_INFO_RESOURCE_USAGE:
		error = coalition_info_resource_usage(coal, buffer, bufsize);
		break;
	case COALITION_INFO_SET_NAME:
		error = coalition_info_set_name_internal(coal, buffer, bufsize);
		break;
	case COALITION_INFO_SET_EFFICIENCY:
		error = coalition_info_efficiency(coal, buffer, bufsize);
		break;
#if DEVELOPMENT || DEBUG
	case COALITION_INFO_GET_DEBUG_INFO:
		error = coalition_info_get_debug_info(coal, buffer, bufsize);
		break;
#endif /* DEVELOPMENT || DEBUG */
	default:
		error = EINVAL;
	}

bad:
	coalition_release(coal);
	return error;
}

int
coalition_ledger(__unused proc_t p, __unused struct coalition_ledger_args *uap, __unused int32_t *retval)
{
	user_addr_t cidp = uap->cid;
	user_addr_t buffer = uap->buffer;
	user_addr_t bufsizep = uap->bufsize;
	user_size_t bufsize;
	uint32_t operation = uap->operation;
	int error;
	uint64_t cid;
	coalition_t coal = COALITION_NULL;

	if (!kauth_cred_issuser(kauth_cred_get())) {
		error = EPERM;
		goto out;
	}

	error = copyin(cidp, &cid, sizeof(cid));
	if (error) {
		goto out;
	}

	coal = coalition_find_by_id(cid);
	if (coal == COALITION_NULL) {
		error = ESRCH;
		goto out;
	}

	if (IS_64BIT_PROCESS(p)) {
		user64_size_t size64;
		error = copyin(bufsizep, &size64, sizeof(size64));
		bufsize = (user_size_t)size64;
	} else {
		user32_size_t size32;
		error = copyin(bufsizep, &size32, sizeof(size32));
		bufsize = (user_size_t)size32;
	}
	if (error) {
		goto out;
	}

	switch (operation) {
	case COALITION_LEDGER_SET_LOGICAL_WRITES_LIMIT:
		error = coalition_ledger_logical_writes_limit(coal, buffer, bufsize);
		break;
	default:
		error = EINVAL;
	}
out:
	if (coal != COALITION_NULL) {
		coalition_release(coal);
	}
	return error;
}

static int
coalition_policy_set_suppress(coalition_t coal, coalition_policy_suppress_t value)
{
	int error = 0;

	kern_return_t kr;

	switch (value) {
	case COALITION_POLICY_SUPPRESS_NONE:
		kr = jetsam_coalition_set_policy(coal, TASK_POLICY_DARWIN_BG, TASK_POLICY_DISABLE);
		error = mach_to_bsd_errno(kr);
		break;
	case COALITION_POLICY_SUPPRESS_DARWIN_BG:
		kr = jetsam_coalition_set_policy(coal, TASK_POLICY_DARWIN_BG, TASK_POLICY_ENABLE);
		error = mach_to_bsd_errno(kr);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static int
coalition_policy_get_suppress(coalition_t coal, int32_t *retval)
{
	int value = 0;
	kern_return_t kr = jetsam_coalition_get_policy(coal,
	    TASK_POLICY_DARWIN_BG, &value);

	if (kr != KERN_SUCCESS) {
		return mach_to_bsd_errno(kr);
	}

	switch (value) {
	case TASK_POLICY_DISABLE:
		*retval = (int32_t)COALITION_POLICY_SUPPRESS_NONE;
		break;
	case TASK_POLICY_ENABLE:
		*retval = (int32_t)COALITION_POLICY_SUPPRESS_DARWIN_BG;
		break;
	default:
		panic("unknown coalition policy suppress value %d", value);
		break;
	}

	return 0;
}

int
sys_coalition_policy_set(proc_t p, struct coalition_policy_set_args *uap, __unused int32_t *retval)
{
	uint64_t cid = uap->cid;
	coalition_policy_flavor_t flavor = uap->flavor;
	uint32_t value = uap->value;

	int error = 0;

	if (!IOTaskHasEntitlement(proc_task(p), COALITION_POLICY_ENTITLEMENT)) {
		return EPERM;
	}

	coalition_t coal = coalition_find_by_id(cid);
	if (coal == COALITION_NULL) {
		return ESRCH;
	}

	if (coalition_type(coal) != COALITION_TYPE_JETSAM) {
		error = ENOTSUP;
		goto bad;
	}

	switch (flavor) {
	case COALITION_POLICY_SUPPRESS:
		error = coalition_policy_set_suppress(coal, (coalition_policy_suppress_t)value);
		break;
	default:
		error = EINVAL;
		break;
	}

bad:
	coalition_release(coal);
	return error;
}

int
sys_coalition_policy_get(proc_t p, struct coalition_policy_get_args *uap, int32_t *retval)
{
	uint64_t cid = uap->cid;
	coalition_policy_flavor_t flavor = uap->flavor;

	int error = 0;

	if (!IOTaskHasEntitlement(proc_task(p), COALITION_POLICY_ENTITLEMENT)) {
		return EPERM;
	}

	coalition_t coal = coalition_find_by_id(cid);
	if (coal == COALITION_NULL) {
		return ESRCH;
	}

	if (coalition_type(coal) != COALITION_TYPE_JETSAM) {
		error = ENOTSUP;
		goto bad;
	}

	switch (flavor) {
	case COALITION_POLICY_SUPPRESS:
		error = coalition_policy_get_suppress(coal, retval);
		break;
	default:
		error = EINVAL;
		break;
	}

bad:
	coalition_release(coal);
	return error;
}



#if DEVELOPMENT || DEBUG
static int sysctl_coalition_get_ids SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error, pid;
	proc_t tproc;
	uint64_t value;
	uint64_t ids[COALITION_NUM_TYPES] = {};


	error = SYSCTL_IN(req, &value, sizeof(value));
	if (error) {
		return error;
	}
	if (!req->newptr) {
		pid = proc_getpid(req->p);
	} else {
		pid = (int)value;
	}

	coal_dbg("looking up coalitions for pid:%d", pid);
	tproc = proc_find(pid);
	if (tproc == NULL) {
		coal_dbg("ERROR: Couldn't find pid:%d", pid);
		return ESRCH;
	}

	task_coalition_ids(proc_task(tproc), ids);
	proc_rele(tproc);

	return SYSCTL_OUT(req, ids, sizeof(ids));
}

SYSCTL_PROC(_kern, OID_AUTO, coalitions, CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_coalition_get_ids, "Q", "coalition ids of a given process");


static int sysctl_coalition_get_roles SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error, pid;
	proc_t tproc;
	int value;
	int roles[COALITION_NUM_TYPES] = {};


	error = SYSCTL_IN(req, &value, sizeof(value));
	if (error) {
		return error;
	}
	if (!req->newptr) {
		pid = proc_getpid(req->p);
	} else {
		pid = (int)value;
	}

	coal_dbg("looking up coalitions for pid:%d", pid);
	tproc = proc_find(pid);
	if (tproc == NULL) {
		coal_dbg("ERROR: Couldn't find pid:%d", pid);
		return ESRCH;
	}

	task_coalition_roles(proc_task(tproc), roles);
	proc_rele(tproc);

	return SYSCTL_OUT(req, roles, sizeof(roles));
}

SYSCTL_PROC(_kern, OID_AUTO, coalition_roles, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_coalition_get_roles, "I", "coalition roles of a given process");


static int sysctl_coalition_get_page_count SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error, pid;
	proc_t tproc;
	coalition_t coal;
	uint64_t value;
	uint64_t pgcount[COALITION_NUM_TYPES];


	error = SYSCTL_IN(req, &value, sizeof(value));
	if (error) {
		return error;
	}
	if (!req->newptr) {
		pid = proc_getpid(req->p);
	} else {
		pid = (int)value;
	}

	coal_dbg("looking up coalitions for pid:%d", pid);
	tproc = proc_find(pid);
	if (tproc == NULL) {
		coal_dbg("ERROR: Couldn't find pid:%d", pid);
		return ESRCH;
	}

	memset(pgcount, 0, sizeof(pgcount));

	for (int t = 0; t < COALITION_NUM_TYPES; t++) {
		coal = task_get_coalition(proc_task(tproc), t);
		if (coal != COALITION_NULL) {
			int ntasks = 0;
			pgcount[t] = coalition_get_page_count(coal, &ntasks);
			coal_dbg("PID:%d, Coalition:%lld, type:%d, pgcount:%lld",
			    pid, coalition_id(coal), t, pgcount[t]);
		}
	}

	proc_rele(tproc);

	return SYSCTL_OUT(req, pgcount, sizeof(pgcount));
}

SYSCTL_PROC(_kern, OID_AUTO, coalition_page_count, CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_coalition_get_page_count, "Q", "coalition page count of a specified process");


static int sysctl_coalition_get_pid_list SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error, type, sort_order, pid;
	int value[3];
	int has_pid = 1;

	coalition_t coal = COALITION_NULL;
	proc_t tproc = PROC_NULL;
	int npids = 0;
	int pidlist[100] = { 0, };


	error = SYSCTL_IN(req, &value, sizeof(value));
	if (error) {
		has_pid = 0;
		error = SYSCTL_IN(req, &value, sizeof(value) - sizeof(value[0]));
	}
	if (error) {
		return error;
	}
	if (!req->newptr) {
		type = COALITION_TYPE_RESOURCE;
		sort_order = COALITION_SORT_DEFAULT;
		pid = proc_getpid(req->p);
	} else {
		type = value[0];
		sort_order = value[1];
		if (has_pid) {
			pid = value[2];
		} else {
			pid = proc_getpid(req->p);
		}
	}

	if (type < 0 || type >= COALITION_NUM_TYPES) {
		return EINVAL;
	}

	coal_dbg("getting constituent PIDS for coalition of type %d "
	    "containing pid:%d (sort:%d)", type, pid, sort_order);
	tproc = proc_find(pid);
	if (tproc == NULL) {
		coal_dbg("ERROR: Couldn't find pid:%d", pid);
		return ESRCH;
	}

	coal = task_get_coalition(proc_task(tproc), type);
	if (coal == COALITION_NULL) {
		goto out;
	}

	npids = coalition_get_pid_list(coal, COALITION_ROLEMASK_ALLROLES, sort_order,
	    pidlist, sizeof(pidlist) / sizeof(pidlist[0]));
	if (npids > (int)(sizeof(pidlist) / sizeof(pidlist[0]))) {
		coal_dbg("Too many members in coalition %llu (from pid:%d): %d!",
		    coalition_id(coal), pid, npids);
		npids = sizeof(pidlist) / sizeof(pidlist[0]);
	}

out:
	proc_rele(tproc);

	if (npids < 0) {
		/* npids is a negative errno */
		return -npids;
	}

	if (npids == 0) {
		return ENOENT;
	}

	return SYSCTL_OUT(req, pidlist, sizeof(pidlist[0]) * npids);
}

SYSCTL_PROC(_kern, OID_AUTO, coalition_pid_list, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_coalition_get_pid_list, "I", "list of PIDS which are members of the coalition of the current process");

#if DEVELOPMENT
static int sysctl_coalition_notify SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error, should_set;
	coalition_t coal;
	uint64_t value[2];

	should_set = 1;
	error = SYSCTL_IN(req, value, sizeof(value));
	if (error) {
		error = SYSCTL_IN(req, value, sizeof(value) - sizeof(value[0]));
		if (error) {
			return error;
		}
		should_set = 0;
	}
	if (!req->newptr) {
		return error;
	}

	coal = coalition_find_by_id(value[0]);
	if (coal == COALITION_NULL) {
		coal_dbg("Can't find coalition with ID:%lld", value[0]);
		return ESRCH;
	}

	if (should_set) {
		coalition_set_notify(coal, (int)value[1]);
	}

	value[0] = (uint64_t)coalition_should_notify(coal);

	coalition_release(coal);

	return SYSCTL_OUT(req, value, sizeof(value[0]));
}

SYSCTL_PROC(_kern, OID_AUTO, coalition_notify, CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_coalition_notify, "Q", "get/set coalition notification flag");

extern int unrestrict_coalition_syscalls;
SYSCTL_INT(_kern, OID_AUTO, unrestrict_coalitions,
    CTLFLAG_RW, &unrestrict_coalition_syscalls, 0,
    "unrestrict the coalition interface");

#endif /* DEVELOPMENT */

#include <kern/energy_perf.h>

static int sysctl_coalition_gpu_energy_test SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	if (!req->newptr) {
		return ENOTSUP;
	}

	uint64_t value[4] = {};

	int error = SYSCTL_IN(req, value, sizeof(value));
	if (error) {
		return error;
	}

	kern_return_t kr = KERN_SUCCESS;
	energy_id_t energy_id_out = 0;

	switch (value[0]) {
	case 1:
		kr = current_energy_id(&energy_id_out);
		break;
	case 2:
		kr = task_id_token_to_energy_id((mach_port_name_t) value[1], &energy_id_out);
		break;
	case 3:
		kr = energy_id_report_energy(ENERGY_ID_SOURCE_GPU,
		    (energy_id_t)value[1], (energy_id_t)value[2], value[3]);
		break;
	}

	if (kr != KERN_SUCCESS) {
		return mach_to_bsd_errno(kr);
	}

	value[0] = energy_id_out;

	return SYSCTL_OUT(req, value, sizeof(value[0]));
}

SYSCTL_PROC(_kern, OID_AUTO, coalition_gpu_energy_test, CTLFLAG_MASKED | CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_coalition_gpu_energy_test, "Q", "test coalition gpu energy");


#endif /* DEVELOPMENT || DEBUG */
