/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * NOTICE: This file was modified by McAfee Research in 2004 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 * Copyright (c) 2005-2006 SPARTA, Inc.
 */
/*
 */
/*
 *	File:	ipc/mach_port.c
 *	Author:	Rich Draves
 *	Date:   1989
 *
 *	Exported kernel calls.  See mach/mach_port.defs.
 */

#include <mach/port.h>
#include <mach/kern_return.h>
#include <mach/notify.h>
#include <mach/mach_param.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/vm_map.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/exc_guard.h>
#include <mach/mach_port_server.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_kern_xnu.h>
#include <ipc/port.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_right.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_service_port.h>
#include <kern/misc_protos.h>
#include <security/mac_mach_internal.h>
#include <kern/work_interval.h>
#include <kern/policy_internal.h>
#include <kern/coalition.h>
#include <ipc/ipc_service_port.h>
#include <kern/mach_filter.h>
#include <sys/reason.h>


#if IMPORTANCE_INHERITANCE
#include <ipc/ipc_importance.h>
#endif

static TUNABLE(bool, provisional_reply_port_enforced, "-provisional_reply_port_enforced", false);

extern void qsort(void *a, size_t n, size_t es, int (*cmp)(const void *, const void *));
extern int proc_isinitproc(struct proc *p);
static int
mach_port_name_cmp(const void *_n1, const void *_n2)
{
	mach_port_name_t n1 = *(const mach_port_name_t *)_n1;
	mach_port_name_t n2 = *(const mach_port_name_t *)_n2;

	if (n1 == n2) {
		return 0;
	}

	return n1 < n2 ? -1 : 1;
}

kern_return_t mach_port_get_attributes(ipc_space_t space, mach_port_name_t name,
    int flavor, mach_port_info_t info, mach_msg_type_number_t  *count);
kern_return_t mach_port_get_context(ipc_space_t space, mach_port_name_t name,
    mach_vm_address_t *context);
kern_return_t mach_port_get_set_status(ipc_space_t space, mach_port_name_t name,
    mach_port_name_t **members, mach_msg_type_number_t *membersCnt);

/*
 *	Routine:	mach_port_names_helper
 *	Purpose:
 *		A helper function for mach_port_names.
 *
 *	Conditions:
 *		Space containing entry is [at least] read-locked.
 */
static void
mach_port_names_helper(
	ipc_port_timestamp_t    timestamp,
	ipc_entry_t             entry,
	mach_port_name_t        name,
	mach_port_name_t        *names,
	mach_port_type_t        *types,
	ipc_entry_num_t         *actualp)
{
	ipc_entry_bits_t bits;
	ipc_port_request_index_t request;
	mach_port_type_t type = 0;
	ipc_entry_num_t actual;
	ipc_port_t port;

	bits = entry->ie_bits;
	request = entry->ie_request;
	port = ip_object_to_port(entry->ie_object);

	if (bits & MACH_PORT_TYPE_RECEIVE) {
		assert(IP_VALID(port));

		if (request != IE_REQ_NONE) {
			ip_mq_lock(port);
			require_ip_active(port);
			type |= ipc_port_request_type(port, name, request);
			ip_mq_unlock(port);
		}
	} else if (bits & MACH_PORT_TYPE_SEND_RIGHTS) {
		mach_port_type_t reqtype;

		assert(IP_VALID(port));
		ip_mq_lock(port);

		reqtype = (request != IE_REQ_NONE) ?
		    ipc_port_request_type(port, name, request) : 0;

		/*
		 * If the port is alive, or was alive when the mach_port_names
		 * started, then return that fact.  Otherwise, pretend we found
		 * a dead name entry.
		 */
		if (ip_active(port) || IP_TIMESTAMP_ORDER(timestamp, ip_get_death_time(port))) {
			type |= reqtype;
		} else {
			bits &= ~(IE_BITS_TYPE_MASK);
			bits |= MACH_PORT_TYPE_DEAD_NAME;
			/* account for additional reference for dead-name notification */
			if (reqtype != 0) {
				bits++;
			}
		}
		ip_mq_unlock(port);
	}

	type |= IE_BITS_TYPE(bits);

	actual = *actualp;
	names[actual] = name;
	types[actual] = type;
	*actualp = actual + 1;
}

/*
 *	Routine:	mach_port_names [kernel call]
 *	Purpose:
 *		Retrieves a list of the rights present in the space,
 *		along with type information.  (Same as returned
 *		by mach_port_type.)  The names are returned in
 *		no particular order, but they (and the type info)
 *		are an accurate snapshot of the space.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Arrays of names and types returned.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_names(
	ipc_space_t             space,
	mach_port_name_t        **namesp,
	mach_msg_type_number_t  *namesCnt,
	mach_port_type_t        **typesp,
	mach_msg_type_number_t  *typesCnt)
{
	ipc_entry_table_t table;
	ipc_entry_num_t tsize;
	mach_port_index_t index;
	ipc_entry_num_t actual; /* this many names */
	ipc_port_timestamp_t timestamp; /* logical time of this operation */
	mach_port_name_t *names;
	mach_port_type_t *types;
	kern_return_t kr;

	vm_size_t size;         /* size of allocated memory */
	vm_offset_t addr1 = 0;      /* allocated memory, for names */
	vm_offset_t addr2 = 0;      /* allocated memory, for types */
	vm_map_copy_t memory1;  /* copied-in memory, for names */
	vm_map_copy_t memory2;  /* copied-in memory, for types */

	/* safe simplifying assumption */
	static_assert(sizeof(mach_port_name_t) == sizeof(mach_port_type_t));

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	size = 0;

	for (;;) {
		ipc_entry_num_t bound;
		vm_size_t size_needed;

		is_read_lock(space);
		if (!is_active(space)) {
			is_read_unlock(space);
			if (size != 0) {
				kmem_free(ipc_kernel_map, addr1, size);
				kmem_free(ipc_kernel_map, addr2, size);
			}
			return KERN_INVALID_TASK;
		}

		/* upper bound on number of names in the space */
		bound = ipc_entry_table_count(is_active_table(space));
		size_needed = vm_map_round_page(
			(bound * sizeof(mach_port_name_t)),
			VM_MAP_PAGE_MASK(ipc_kernel_map));

		if (size_needed <= size) {
			break;
		}

		is_read_unlock(space);

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr1, size);
			kmem_free(ipc_kernel_map, addr2, size);
		}
		size = size_needed;

		kr = kmem_alloc(ipc_kernel_map, &addr1, size,
		    KMA_DATA, VM_KERN_MEMORY_IPC);
		if (kr != KERN_SUCCESS) {
			return KERN_RESOURCE_SHORTAGE;
		}

		kr = kmem_alloc(ipc_kernel_map, &addr2, size,
		    KMA_DATA, VM_KERN_MEMORY_IPC);
		if (kr != KERN_SUCCESS) {
			kmem_free(ipc_kernel_map, addr1, size);
			return KERN_RESOURCE_SHORTAGE;
		}
	}
	/* space is read-locked and active */

	names = (mach_port_name_t *) addr1;
	types = (mach_port_type_t *) addr2;
	actual = 0;

	timestamp = ipc_port_timestamp();

	table = is_active_table(space);
	tsize = ipc_entry_table_count(table);

	for (index = 1; index < tsize; index++) {
		ipc_entry_t entry = ipc_entry_table_get_nocheck(table, index);
		ipc_entry_bits_t bits = entry->ie_bits;

		if (IE_BITS_TYPE(bits) != MACH_PORT_TYPE_NONE) {
			mach_port_name_t name;

			name = MACH_PORT_MAKE(index, IE_BITS_GEN(bits));
			mach_port_names_helper(timestamp, entry, name, names,
			    types, &actual);
		}
	}

	is_read_unlock(space);

	if (actual == 0) {
		memory1 = VM_MAP_COPY_NULL;
		memory2 = VM_MAP_COPY_NULL;

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr1, size);
			kmem_free(ipc_kernel_map, addr2, size);
		}
	} else {
		vm_size_t size_used;
		vm_size_t vm_size_used;

		size_used = actual * sizeof(mach_port_name_t);
		vm_size_used =
		    vm_map_round_page(size_used,
		    VM_MAP_PAGE_MASK(ipc_kernel_map));

		/*
		 *	Make used memory pageable and get it into
		 *	copied-in form.  Free any unused memory.
		 */

		if (size_used < vm_size_used) {
			bzero((char *)addr1 + size_used, vm_size_used - size_used);
			bzero((char *)addr2 + size_used, vm_size_used - size_used);
		}

		kr = vm_map_unwire(ipc_kernel_map, addr1, addr1 + vm_size_used, FALSE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_unwire(ipc_kernel_map, addr2, addr2 + vm_size_used, FALSE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)addr1,
		    (vm_map_size_t)size_used, TRUE, &memory1);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)addr2,
		    (vm_map_size_t)size_used, TRUE, &memory2);
		assert(kr == KERN_SUCCESS);

		if (vm_size_used != size) {
			kmem_free(ipc_kernel_map,
			    addr1 + vm_size_used, size - vm_size_used);
			kmem_free(ipc_kernel_map,
			    addr2 + vm_size_used, size - vm_size_used);
		}
	}

	*namesp = (mach_port_name_t *) memory1;
	*namesCnt = actual;
	*typesp = (mach_port_type_t *) memory2;
	*typesCnt = actual;
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_type [kernel call]
 *	Purpose:
 *		Retrieves the type of a right in the space.
 *		The type is a bitwise combination of one or more
 *		of the following type bits:
 *			MACH_PORT_TYPE_SEND
 *			MACH_PORT_TYPE_RECEIVE
 *			MACH_PORT_TYPE_SEND_ONCE
 *			MACH_PORT_TYPE_PORT_SET
 *			MACH_PORT_TYPE_DEAD_NAME
 *		In addition, the following pseudo-type bits may be present:
 *			MACH_PORT_TYPE_DNREQUEST
 *				A dead-name notification is requested.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Type is returned.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 */

kern_return_t
mach_port_type(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_type_t        *typep)
{
	mach_port_urefs_t urefs;
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (name == MACH_PORT_NULL) {
		return KERN_INVALID_NAME;
	}

	if (name == MACH_PORT_DEAD) {
		*typep = MACH_PORT_TYPE_DEAD_NAME;
		return KERN_SUCCESS;
	}

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/* space is write-locked and active */
	kr = ipc_right_info(space, name, entry, typep, &urefs);
	/* space is unlocked */

#if 1
	/* JMM - workaround rdar://problem/9121297 (CF being too picky on these bits). */
	*typep &= ~(MACH_PORT_TYPE_SPREQUEST | MACH_PORT_TYPE_SPREQUEST_DELAYED);
#endif

	return kr;
}


/*
 *	Routine:	mach_port_allocate_name [kernel call]
 *	Purpose:
 *		Allocates a right in a space, using a specific name
 *		for the new right.  Possible rights:
 *			MACH_PORT_RIGHT_RECEIVE
 *			MACH_PORT_RIGHT_PORT_SET
 *			MACH_PORT_RIGHT_DEAD_NAME
 *
 *		A new port (allocated with MACH_PORT_RIGHT_RECEIVE)
 *		has no extant send or send-once rights and no queued
 *		messages.  Its queue limit is MACH_PORT_QLIMIT_DEFAULT
 *		and its make-send count is 0.  It is not a member of
 *		a port set.  It has no registered no-senders or
 *		port-destroyed notification requests.
 *
 *		A new port set has no members.
 *
 *		A new dead name has one user reference.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The right is allocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	The name isn't a legal name.
 *		KERN_INVALID_VALUE	"right" isn't a legal kind of right.
 *		KERN_NAME_EXISTS	The name already denotes a right.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *
 *	Restrictions on name allocation:  NT bits are reserved by kernel,
 *	must be set on any chosen name.  Can't do this at all in kernel
 *	loaded server.
 */

kern_return_t
mach_port_allocate_name(
	ipc_space_t             space,
	mach_port_right_t       right,
	mach_port_name_t        name)
{
	kern_return_t           kr;
	mach_port_qos_t         qos = { .name = TRUE };

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_VALUE;
	}

	kr = mach_port_allocate_full(space, right, MACH_PORT_NULL,
	    &qos, &name);
	return kr;
}

/*
 *	Routine:	mach_port_allocate [kernel call]
 *	Purpose:
 *		Allocates a right in a space.  Like mach_port_allocate_name,
 *		except that the implementation picks a name for the right.
 *		The name may be any legal name in the space that doesn't
 *		currently denote a right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The right is allocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal kind of right.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *		KERN_NO_SPACE		No room in space for another right.
 */

kern_return_t
mach_port_allocate(
	ipc_space_t             space,
	mach_port_right_t       right,
	mach_port_name_t        *namep)
{
	kern_return_t           kr;
	mach_port_qos_t         qos = { };

	kr = mach_port_allocate_full(space, right, MACH_PORT_NULL,
	    &qos, namep);
	return kr;
}

/*
 *	Routine:	mach_port_allocate_qos [kernel call]
 *	Purpose:
 *		Allocates a right, with qos options, in a space.  Like
 *		mach_port_allocate_name, except that the implementation
 *		picks a name for the right. The name may be any legal name
 *		in the space that doesn't currently denote a right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The right is allocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal kind of right.
 *		KERN_INVALID_ARGUMENT   The qos request was invalid.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *		KERN_NO_SPACE		No room in space for another right.
 */

kern_return_t
mach_port_allocate_qos(
	ipc_space_t             space,
	mach_port_right_t       right,
	mach_port_qos_t         *qosp,
	mach_port_name_t        *namep)
{
	kern_return_t           kr;

	if (qosp->name) {
		return KERN_INVALID_ARGUMENT;
	}
	kr = mach_port_allocate_full(space, right, MACH_PORT_NULL,
	    qosp, namep);
	return kr;
}

/*
 *	Routine:	mach_port_allocate_full [kernel call]
 *	Purpose:
 *		Allocates a right in a space.  Supports the
 *		special case of specifying a name. The name may
 *		be any legal name in the space that doesn't
 *		currently denote a right.
 *
 *		While we no longer support users requesting
 *		preallocated message for the port, we still
 *		check for errors in such requests and then
 *		just clear the request.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The right is allocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal kind of right, or supplied port
 *                          name is invalid.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *		KERN_NO_SPACE		No room in space for another right.
 */

kern_return_t
mach_port_allocate_full(
	ipc_space_t             space,
	mach_port_right_t       right,
	mach_port_t             proto,
	mach_port_qos_t         *qosp,
	mach_port_name_t        *namep)
{
	kern_return_t           kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (proto != MACH_PORT_NULL) {
		return KERN_INVALID_VALUE;
	}

	if (qosp->name) {
		if (!MACH_PORT_VALID(*namep)) {
			return KERN_INVALID_VALUE;
		}
	}

	/*
	 * Don't actually honor prealloc requests anymore,
	 * (only mk_timer still uses IP_PREALLOC messages, by hand).
	 *
	 * (for security reasons, and because it isn't guaranteed anyway).
	 * Keep old errors for legacy reasons.
	 */
	if (qosp->prealloc) {
		if (qosp->len > MACH_MSG_SIZE_MAX - MAX_TRAILER_SIZE) {
			return KERN_RESOURCE_SHORTAGE;
		}
		if (right != MACH_PORT_RIGHT_RECEIVE) {
			return KERN_INVALID_VALUE;
		}
		qosp->prealloc = 0;
	}

	switch (right) {
	case MACH_PORT_RIGHT_RECEIVE:
	{
		ipc_port_t      port;

		if (qosp->name) {
			kr = ipc_port_alloc_name(space, IPC_PORT_INIT_MESSAGE_QUEUE,
			    *namep, &port);
		} else {
			kr = ipc_port_alloc(space, IPC_PORT_INIT_MESSAGE_QUEUE,
			    namep, &port);
		}
		if (kr == KERN_SUCCESS) {
			ip_mq_unlock(port);
		}
		break;
	}

	case MACH_PORT_RIGHT_PORT_SET:
	{
		ipc_pset_t      pset;

		if (qosp->name) {
			kr = ipc_pset_alloc_name(space, *namep, &pset);
		} else {
			kr = ipc_pset_alloc(space, namep, &pset);
		}
		if (kr == KERN_SUCCESS) {
			ips_mq_unlock(pset);
		}
		break;
	}

	case MACH_PORT_RIGHT_DEAD_NAME:
		kr = ipc_object_alloc_dead(space, namep);
		break;

	default:
		kr = KERN_INVALID_VALUE;
		break;
	}

	return kr;
}

/*
 *	Routine:	mach_port_destroy [kernel call]
 *	Purpose:
 *		Cleans up and destroys all rights denoted by a name
 *		in a space.  The destruction of a receive right
 *		destroys the port, unless a port-destroyed request
 *		has been made for it; the destruction of a port-set right
 *		destroys the port set.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The name is destroyed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 */

kern_return_t
mach_port_destroy(
	ipc_space_t             space,
	mach_port_name_t        name)
{
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_SUCCESS;
	}

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_NAME);
		return kr;
	}
	/* space is write-locked and active */

	kr = ipc_right_destroy(space, name, entry, TRUE, 0); /* unlocks space */
	return kr;
}

/*
 *	Routine:	mach_port_deallocate [kernel call]
 *	Purpose:
 *		Deallocates a user reference from a send right,
 *		send-once right, dead-name right or a port_set right.
 *		May deallocate the right, if this is the last uref,
 *		and destroy the name, if it doesn't denote
 *		other rights.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		The uref is deallocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	The right isn't correct.
 */

kern_return_t
mach_port_deallocate_kernel(
	ipc_space_t             space,
	mach_port_name_t        name,
	ipc_kobject_type_t      kotype)
{
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_SUCCESS;
	}

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_NAME);
		return kr;
	}
	/* space is write-locked */

	if (kotype != IKOT_UNKNOWN && io_kotype(entry->ie_object) != kotype) {
		is_write_unlock(space);
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_RIGHT);
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_right_dealloc(space, name, entry); /* unlocks space */
	return kr;
}

kern_return_t
mach_port_deallocate(
	ipc_space_t             space,
	mach_port_name_t        name)
{
	return mach_port_deallocate_kernel(space, name, IKOT_UNKNOWN);
}

/*
 *	Routine:	mach_port_get_refs [kernel call]
 *	Purpose:
 *		Retrieves the number of user references held by a right.
 *		Receive rights, port-set rights, and send-once rights
 *		always have one user reference.  Returns zero if the
 *		name denotes a right, but not the queried right.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Number of urefs returned.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal value.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 */

kern_return_t
mach_port_get_refs(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_right_t       right,
	mach_port_urefs_t       *urefsp)
{
	mach_port_type_t type;
	mach_port_urefs_t urefs;
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (right >= MACH_PORT_RIGHT_NUMBER) {
		return KERN_INVALID_VALUE;
	}

	if (!MACH_PORT_VALID(name)) {
		if (right == MACH_PORT_RIGHT_SEND ||
		    right == MACH_PORT_RIGHT_SEND_ONCE) {
			*urefsp = 1;
			return KERN_SUCCESS;
		}
		return KERN_INVALID_NAME;
	}

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/* space is write-locked and active */
	kr = ipc_right_info(space, name, entry, &type, &urefs);
	/* space is unlocked */

	if (kr != KERN_SUCCESS) {
		return kr;
	}

	if (type & MACH_PORT_TYPE(right)) {
		switch (right) {
		case MACH_PORT_RIGHT_SEND_ONCE:
			assert(urefs == 1);
			OS_FALLTHROUGH;

		case MACH_PORT_RIGHT_PORT_SET:
		case MACH_PORT_RIGHT_RECEIVE:
			*urefsp = 1;
			break;

		case MACH_PORT_RIGHT_DEAD_NAME:
		case MACH_PORT_RIGHT_SEND:
			assert(urefs > 0);
			*urefsp = urefs;
			break;

		default:
			panic("mach_port_get_refs: strange rights");
		}
	} else {
		*urefsp = 0;
	}

	return kr;
}

/*
 *	Routine:	mach_port_mod_refs
 *	Purpose:
 *		Modifies the number of user references held by a right.
 *		The resulting number of user references must be non-negative.
 *		If it is zero, the right is deallocated.  If the name
 *		doesn't denote other rights, it is destroyed.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Modified number of urefs.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	"right" isn't a legal value.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote specified right.
 *		KERN_INVALID_VALUE	Impossible modification to urefs.
 *		KERN_UREFS_OVERFLOW	Urefs would overflow.
 */

kern_return_t
mach_port_mod_refs(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_right_t       right,
	mach_port_delta_t       delta)
{
	ipc_entry_t entry;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (right >= MACH_PORT_RIGHT_NUMBER) {
		return KERN_INVALID_VALUE;
	}

	if (!MACH_PORT_VALID(name)) {
		if (right == MACH_PORT_RIGHT_SEND ||
		    right == MACH_PORT_RIGHT_SEND_ONCE) {
			return KERN_SUCCESS;
		}
		return KERN_INVALID_NAME;
	}

	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_NAME);
		return kr;
	}

	/* space is write-locked and active */

	kr = ipc_right_delta(space, name, entry, right, delta); /* unlocks */
	return kr;
}


/*
 *	Routine:	mach_port_peek [kernel call]
 *	Purpose:
 *		Peek at the message queue for the specified receive
 *		right and return info about a message in the queue.
 *
 *		On input, seqnop points to a sequence number value
 *		to match the message being peeked. If zero is specified
 *		as the seqno, the first message in the queue will be
 *		peeked.
 *
 *		Only the following trailer types are currently supported:
 *			MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0)
 *
 *				or'ed with one of these element types:
 *			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_NULL)
 *			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SEQNO)
 *			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SENDER)
 *			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT)
 *
 *		On input, the value pointed to by trailer_sizep must be
 *		large enough to hold the requested trailer size.
 *
 *		The message sequence number, id, size, requested trailer info
 *		and requested trailer size are returned in their respective
 *		output parameters upon success.
 *
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Matching message found, out parameters set.
 *		KERN_INVALID_TASK	The space is null or dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 *		KERN_INVALID_VALUE	The input parameter values are out of bounds.
 *		KERN_FAILURE		The requested message was not found.
 */

kern_return_t
mach_port_peek(
	ipc_space_t                     space,
	mach_port_name_t                name,
	mach_msg_trailer_type_t         trailer_type,
	mach_port_seqno_t               *seqnop,
	mach_msg_size_t                 *msg_sizep,
	mach_msg_id_t                   *msg_idp,
	mach_msg_trailer_info_t         trailer_infop,
	mach_msg_type_number_t          *trailer_sizep)
{
	ipc_port_t port;
	kern_return_t kr;
	boolean_t found;
	mach_msg_max_trailer_t max_trailer;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_RIGHT;
	}

	/*
	 * We don't allow anything greater than the audit trailer - to avoid
	 * leaking the context pointer and to avoid variable-sized context issues.
	 */
	if (GET_RCV_ELEMENTS(trailer_type) > MACH_RCV_TRAILER_AUDIT ||
	    REQUESTED_TRAILER_SIZE(TRUE, trailer_type) > *trailer_sizep) {
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_VALUE);
		return KERN_INVALID_VALUE;
	}

	*trailer_sizep = REQUESTED_TRAILER_SIZE(TRUE, trailer_type);

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0,
		    ((KERN_INVALID_NAME == kr) ?
		    kGUARD_EXC_INVALID_NAME :
		    kGUARD_EXC_INVALID_RIGHT));
		return kr;
	}

	/* Port locked and active */
	found = ipc_mqueue_peek_locked(&port->ip_messages, seqnop,
	    msg_sizep, msg_idp, &max_trailer, NULL);
	ip_mq_unlock(port);

	if (found != TRUE) {
		return KERN_FAILURE;
	}

	max_trailer.msgh_seqno = *seqnop;
	memcpy(trailer_infop, &max_trailer, *trailer_sizep);

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_set_mscount [kernel call]
 *	Purpose:
 *		Changes a receive right's make-send count.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Set make-send count.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_set_mscount(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_mscount_t     mscount)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	/* port is locked and active */

	port->ip_mscount = mscount;
	ip_mq_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_set_seqno [kernel call]
 *	Purpose:
 *		Changes a receive right's sequence number.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Set sequence number.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_set_seqno(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_seqno_t       seqno)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	/* port is locked and active */

	ipc_mqueue_set_seqno_locked(&port->ip_messages, seqno);

	ip_mq_unlock(port);
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_get_context [kernel call]
 *	Purpose:
 *		Returns a receive right's context pointer.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Set context pointer.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_get_context(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_vm_address_t       *context)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/* Port locked and active */

	/* For strictly guarded ports, return empty context (which acts as guard) */
	if (port->ip_strict_guard) {
		*context = 0;
	} else {
		*context = port->ip_context;
	}

	ip_mq_unlock(port);
	return KERN_SUCCESS;
}

kern_return_t
mach_port_get_context_from_user(
	mach_port_t             port,
	mach_port_name_t        name,
	mach_vm_address_t       *context)
{
	kern_return_t kr;

	ipc_space_t space = convert_port_to_space_read_no_eval(port);

	if (space == IPC_SPACE_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = mach_port_get_context(space, name, context);

	ipc_space_release(space);
	return kr;
}

/*
 *	Routine:	mach_port_set_context [kernel call]
 *	Purpose:
 *		Changes a receive right's context pointer.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Set context pointer.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote receive rights.
 */

kern_return_t
mach_port_set_context(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_vm_address_t       context)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/* port is locked and active */
	if (port->ip_strict_guard) {
		uint64_t portguard = port->ip_context;
		ip_mq_unlock(port);
		/* For strictly guarded ports, disallow overwriting context; Raise Exception */
		mach_port_guard_exception(name, context, portguard, kGUARD_EXC_SET_CONTEXT);
		return KERN_INVALID_ARGUMENT;
	}

	port->ip_context = context;

	ip_mq_unlock(port);
	return KERN_SUCCESS;
}


/*
 *	Routine:	mach_port_get_set_status [kernel call]
 *	Purpose:
 *		Retrieves a list of members in a port set.
 *		Returns the space's name for each receive right member.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieved list of members.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote a port set.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_get_set_status(
	ipc_space_t                     space,
	mach_port_name_t                name,
	mach_port_name_t                **members,
	mach_msg_type_number_t          *membersCnt)
{
	__block ipc_entry_num_t actual;         /* this many members */
	ipc_entry_num_t maxnames;       /* space for this many members */
	kern_return_t kr;

	vm_size_t size;         /* size of allocated memory */
	vm_offset_t addr;       /* allocated memory */
	vm_map_copy_t memory;   /* copied-in memory */

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_RIGHT;
	}

	size = VM_MAP_PAGE_SIZE(ipc_kernel_map);        /* initial guess */
	actual = 0;

	for (;;) {
		mach_port_name_t *names;
		ipc_object_t psobj;
		ipc_pset_t pset;

		kr = kmem_alloc(ipc_kernel_map, &addr, size,
		    KMA_DATA, VM_KERN_MEMORY_IPC);
		if (kr != KERN_SUCCESS) {
			return KERN_RESOURCE_SHORTAGE;
		}

		kr = ipc_object_translate(space, name, MACH_PORT_RIGHT_PORT_SET, &psobj);
		if (kr != KERN_SUCCESS) {
			kmem_free(ipc_kernel_map, addr, size);
			return kr;
		}

		/* just use a portset reference from here on out */
		pset = ips_object_to_pset(psobj);
		names = (mach_port_name_t *)addr;
		maxnames = (ipc_entry_num_t)(size / sizeof(mach_port_name_t));

		waitq_set_foreach_member_locked(&pset->ips_wqset, ^(struct waitq *wq){
			if (actual < maxnames) {
			        names[actual] = ip_get_receiver_name(ip_from_waitq(wq));
			}
			actual++;
		});

		/* release the portset reference */
		ips_mq_unlock(pset);

		if (actual <= maxnames) {
			break;
		}

		/* didn't have enough memory; allocate more */
		kmem_free(ipc_kernel_map, addr, size);
		size = vm_map_round_page(actual * sizeof(mach_port_name_t),
		    VM_MAP_PAGE_MASK(ipc_kernel_map)) +
		    VM_MAP_PAGE_SIZE(ipc_kernel_map);
		actual = 0;
	}

	if (actual == 0) {
		memory = VM_MAP_COPY_NULL;

		kmem_free(ipc_kernel_map, addr, size);
	} else {
		vm_size_t size_used;
		vm_size_t vm_size_used;

		qsort((void *)addr, actual, sizeof(mach_port_name_t),
		    mach_port_name_cmp);

		size_used = actual * sizeof(mach_port_name_t);
		vm_size_used = vm_map_round_page(size_used,
		    VM_MAP_PAGE_MASK(ipc_kernel_map));

		if (size_used < vm_size_used) {
			bzero((char *)addr + size_used, vm_size_used - size_used);
		}

		/*
		 *	Make used memory pageable and get it into
		 *	copied-in form.  Free any unused memory.
		 */

		kr = vm_map_unwire(ipc_kernel_map, addr, addr + vm_size_used, FALSE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)addr,
		    (vm_map_size_t)size_used, TRUE, &memory);
		assert(kr == KERN_SUCCESS);

		if (vm_size_used != size) {
			kmem_free(ipc_kernel_map,
			    addr + vm_size_used, size - vm_size_used);
		}
	}

	*members = (mach_port_name_t *) memory;
	*membersCnt = actual;
	return KERN_SUCCESS;
}

kern_return_t
mach_port_get_set_status_from_user(
	mach_port_t                     port,
	mach_port_name_t                name,
	mach_port_name_t                **members,
	mach_msg_type_number_t          *membersCnt)
{
	kern_return_t kr;

	ipc_space_t space = convert_port_to_space_read_no_eval(port);

	if (space == IPC_SPACE_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = mach_port_get_set_status(space, name, members, membersCnt);

	ipc_space_release(space);
	return kr;
}

/*
 *	Routine:	mach_port_move_member [kernel call]
 *	Purpose:
 *		If after is MACH_PORT_NULL, removes member
 *		from the port set it is in.  Otherwise, adds
 *		member to after, removing it from any set
 *		it might already be in.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Moved the port.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	Member didn't denote a right.
 *		KERN_INVALID_RIGHT	Member didn't denote a receive right.
 *		KERN_INVALID_NAME	After didn't denote a right.
 *		KERN_INVALID_RIGHT	After didn't denote a port set right.
 *		KERN_NOT_IN_SET
 *			After is MACH_PORT_NULL and Member isn't in a port set.
 */

kern_return_t
mach_port_move_member(
	ipc_space_t             space,
	mach_port_name_t        member,
	mach_port_name_t        after)
{
	ipc_object_t port_obj, ps_obj;
	ipc_port_t port = IP_NULL;
	kern_return_t kr;
	waitq_link_list_t free_l = { };
	waitq_link_t link = WQL_NULL;
	struct waitq_set *keep_waitq_set = NULL;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(member)) {
		return KERN_INVALID_RIGHT;
	}

	if (after == MACH_PORT_DEAD) {
		return KERN_INVALID_RIGHT;
	}

	if (after != MACH_PORT_NULL) {
		link = waitq_link_alloc(WQT_PORT_SET);
		kr = ipc_object_translate_two(space,
		    member, MACH_PORT_RIGHT_RECEIVE, &port_obj,
		    after, MACH_PORT_RIGHT_PORT_SET, &ps_obj);
	} else {
		kr = ipc_object_translate(space,
		    member, MACH_PORT_RIGHT_RECEIVE, &port_obj);
	}
	if (kr != KERN_SUCCESS) {
		goto done;
	}

	port = ip_object_to_port(port_obj);

	if (after != MACH_PORT_NULL) {
		ipc_pset_t nset = ips_object_to_pset(ps_obj);

		ipc_mqueue_add_locked(&port->ip_messages, nset, &link);
		ips_mq_unlock(nset);

		keep_waitq_set = &nset->ips_wqset;
	} else if (!ip_in_pset(port)) {
		kr = KERN_NOT_IN_SET;
	}

	/*
	 * waitq_unlink_all_locked() doesn't dereference `keep_waitq_set,
	 * but we wouldn't want an ABA issue. Fortunately, while `port`
	 * is locked and linked to `nset`, then `nset` can't be reused/freed.
	 */
	waitq_unlink_all_locked(&port->ip_waitq, keep_waitq_set, &free_l);

	ip_mq_unlock(port);

	waitq_link_free_list(WQT_PORT_SET, &free_l);
done:
	if (link.wqlh) {
		waitq_link_free(WQT_PORT_SET, link);
	}

	return kr;
}

/*
 *	Routine:	mach_service_pd_request_notification_check
 *	Purpose:
 *		Check if requesting port destroyed notification on a service port is allowed.
 * Conditions:
 *              Assumes service_port is locked and active.
 */
static bool
mach_service_pd_request_notification_check(
	ipc_port_t              service_port,
	ipc_port_t              notify_port
	)
{
#ifdef MACH_BSD

	uintptr_t task;

	/* Only launchd should be able to register for port destroyed notification on a service port. */
	(void)ipc_port_get_receiver_task_locked(service_port, &task);
	if (task && !proc_isinitproc(get_bsdtask_info((task_t)task))) {
		return false;
	}

	/* Notify port should indicate immovable receive right owned by launchd. */
	if (IP_VALID(notify_port)) {
		ip_mq_lock(notify_port);
		(void)ipc_port_get_receiver_task_locked(notify_port, &task);
		if (task && !proc_isinitproc(get_bsdtask_info((task_t)task))) {
			ip_mq_unlock(notify_port);
			return false;
		}
		if (!notify_port->ip_immovable_receive) {
			ip_mq_unlock(notify_port);
			return false;
		}
		ip_mq_unlock(notify_port);
	}
#endif

	return true;
}

/*
 *	Routine:	mach_port_request_notification [kernel call]
 *	Purpose:
 *		Requests a notification.  The caller supplies
 *		a send-once right for the notification to use,
 *		and the call returns the previously registered
 *		send-once right, if any.  Possible types:
 *
 *		MACH_NOTIFY_PORT_DESTROYED
 *			Requests a port-destroyed notification
 *			for a receive right.  Sync should be zero.
 *		MACH_NOTIFY_SERVICE_PORT_DESTROYED
 *			Special port destroyed notifications to be
 *			used by launchd for service ports only.
 *		MACH_NOTIFY_NO_SENDERS
 *			Requests a no-senders notification for a
 *			receive right.  If there are currently no
 *			senders, sync is less than or equal to the
 *			current make-send count, and a send-once right
 *			is supplied, then an immediate no-senders
 *			notification is generated.
 *		MACH_NOTIFY_DEAD_NAME
 *			Requests a dead-name notification for a send
 *			or receive right.  If the name is already a
 *			dead name, sync is non-zero, and a send-once
 *			right is supplied, then an immediate dead-name
 *			notification is generated.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Requested a notification.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	Bad id value.
 *		KERN_INVALID_NAME	Name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote appropriate right.
 *		KERN_INVALID_CAPABILITY	The notify port is dead.
 *	MACH_NOTIFY_PORT_DESTROYED:
 *		KERN_INVALID_VALUE	Sync isn't zero.
 *		KERN_FAILURE		Re-registering for this notification or registering for a reply port.
 *							If registering for this notification is not allowed on a service port.
 * MACH_NOTIFY_NO_SENDERS:
 *              KERN_FAILURE		Registering for a reply port.
 *	MACH_NOTIFY_SERVICE_PORT_DESTROYED
 *		KERN_INVALID_CAPABILITY	Name is not a service port
 *		KERN_DENIED		Only launchd can set this notification or
 *					Launchd tried to register the old port
 *					destroyed notification
 *	MACH_NOTIFY_DEAD_NAME:
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *		KERN_INVALID_ARGUMENT	Name denotes dead name, but
 *			sync is zero or notify is IP_NULL.
 *		KERN_UREFS_OVERFLOW	Name denotes dead name, but
 *			generating immediate notif. would overflow urefs.
 */

kern_return_t
mach_port_request_notification(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_msg_id_t           id,
	mach_port_mscount_t     sync,
	ipc_port_t              notify,
	ipc_port_t              *previousp)
{
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (notify == IP_DEAD) {
		return KERN_INVALID_CAPABILITY;
	}

	switch (id) {
	case MACH_NOTIFY_PORT_DESTROYED: {
		ipc_port_t port;

		if (sync != 0) {
			return KERN_INVALID_VALUE;
		}

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		/*
		 * you cannot register for port death notifications on a kobject,
		 * kolabel or special reply port.
		 */
		if (ip_is_kobject(port) || ip_is_kolabeled(port) ||
		    port->ip_specialreply || ip_is_reply_port(port)) {
			ip_mq_unlock(port);
			mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_RIGHT);
			return KERN_INVALID_RIGHT;
		}

		if (service_port_defense_enabled && port->ip_service_port &&
		    !mach_service_pd_request_notification_check(port, notify)) {
			ip_mq_unlock(port);
			mach_port_guard_exception(name, 0, 0, kGUARD_EXC_KERN_FAILURE);
			return KERN_FAILURE;
		}

		/* Allow only one registeration of this notification */
		if (ipc_port_has_prdrequest(port)) {
			ip_mq_unlock(port);
			mach_port_guard_exception(name, 0, 0, kGUARD_EXC_KERN_FAILURE);
			return KERN_FAILURE;
		}

		if (port->ip_has_watchport) {
			port->ip_twe->twe_pdrequest = notify;
		} else {
			port->ip_pdrequest = notify;
		}
		ip_mq_unlock(port);
		*previousp = IP_NULL;
		break;
	}

	case MACH_NOTIFY_NO_SENDERS: {
		ipc_port_t port;

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		if (ip_is_reply_port(port)) {
			ip_mq_unlock(port);
			mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_RIGHT);
			return KERN_INVALID_RIGHT;
		}

		ipc_port_nsrequest(port, sync, notify, previousp);
		/* port is unlocked */
		break;
	}

	case MACH_NOTIFY_SEND_POSSIBLE:
	case MACH_NOTIFY_DEAD_NAME: {
		ipc_port_request_opts_t opts = 0;

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_ARGUMENT;
		}

		if (id == MACH_NOTIFY_SEND_POSSIBLE) {
			opts |= IPR_SOR_SPREQ_MASK;
			if (sync) {
				opts |= IPR_SOR_SPARM_MASK;
			}
		}

		kr = ipc_right_request_alloc(space, name, opts, notify, previousp);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		break;
	}

	default:
		return KERN_INVALID_VALUE;
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_insert_right [kernel call]
 *	Purpose:
 *		Inserts a right into a space, as if the space
 *		voluntarily received the right in a message,
 *		except that the right gets the specified name.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Inserted the right.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	The name isn't a legal name.
 *		KERN_NAME_EXISTS	The name already denotes a right.
 *		KERN_INVALID_VALUE	Message doesn't carry a port right.
 *		KERN_INVALID_CAPABILITY	Port is null or dead.
 *		KERN_UREFS_OVERFLOW	Urefs limit would be exceeded.
 *		KERN_RIGHT_EXISTS	Space has rights under another name.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_port_insert_right(
	ipc_space_t                     space,
	mach_port_name_t                name,
	ipc_port_t                      poly,
	mach_msg_type_name_t            polyPoly)
{
	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name) ||
	    !MACH_MSG_TYPE_PORT_ANY_RIGHT(polyPoly)) {
		return KERN_INVALID_VALUE;
	}

	if (!IP_VALID(poly)) {
		return KERN_INVALID_CAPABILITY;
	}

	return ipc_object_copyout_name(space, ip_to_object(poly),
	           polyPoly, name);
}

/*
 *	Routine:	mach_port_extract_right [kernel call]
 *	Purpose:
 *		Extracts a right from a space, as if the space
 *		voluntarily sent the right to the caller.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Extracted the right.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_VALUE	Requested type isn't a port right.
 *		KERN_INVALID_NAME	Name doesn't denote a right.
 *		KERN_INVALID_RIGHT	Name doesn't denote appropriate right.
 */

kern_return_t
mach_port_extract_right(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_msg_type_name_t    msgt_name,
	ipc_port_t              *poly,
	mach_msg_type_name_t    *polyPoly)
{
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_MSG_TYPE_PORT_ANY(msgt_name)) {
		return KERN_INVALID_VALUE;
	}

	if (!MACH_PORT_VALID(name)) {
		/*
		 * really should copy out a dead name, if it is a send or
		 * send-once right being copied, but instead return an
		 * error for now.
		 */
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_object_copyin(space, name, msgt_name, (ipc_object_t *) poly, 0, NULL,
	    (space == current_space() && msgt_name == MACH_MSG_TYPE_COPY_SEND) ?
	    IPC_OBJECT_COPYIN_FLAGS_ALLOW_IMMOVABLE_SEND : IPC_OBJECT_COPYIN_FLAGS_NONE);

	if (kr == KERN_SUCCESS) {
		*polyPoly = ipc_object_copyin_type(msgt_name);
	}
	return kr;
}

/*
 *	Routine:	mach_port_get_status_helper [helper]
 *	Purpose:
 *		Populates a mach_port_status_t structure with
 *		port information.
 *	Conditions:
 *		Port needs to be locked
 *	Returns:
 *		None.
 */
static void
mach_port_get_status_helper(
	ipc_port_t              port,
	mach_port_status_t      *statusp)
{
	/* don't leak set IDs, just indicate that the port is in one or not */
	statusp->mps_pset = ip_in_pset(port);
	statusp->mps_seqno = port->ip_messages.imq_seqno;
	statusp->mps_qlimit = port->ip_messages.imq_qlimit;
	statusp->mps_msgcount = port->ip_messages.imq_msgcount;

	statusp->mps_mscount = port->ip_mscount;
	statusp->mps_sorights = port->ip_sorights;
	statusp->mps_srights = port->ip_srights > 0;
	statusp->mps_pdrequest = ipc_port_has_prdrequest(port);
	statusp->mps_nsrequest = port->ip_nsrequest != IP_NULL;
	statusp->mps_flags = 0;
	if (port->ip_impdonation) {
		statusp->mps_flags |= MACH_PORT_STATUS_FLAG_IMP_DONATION;
		if (port->ip_tempowner) {
			statusp->mps_flags |= MACH_PORT_STATUS_FLAG_TEMPOWNER;
			if (IIT_NULL != ip_get_imp_task(port)) {
				statusp->mps_flags |= MACH_PORT_STATUS_FLAG_TASKPTR;
			}
		}
	}
	if (port->ip_guarded) {
		statusp->mps_flags |= MACH_PORT_STATUS_FLAG_GUARDED;
		if (port->ip_strict_guard) {
			statusp->mps_flags |= MACH_PORT_STATUS_FLAG_STRICT_GUARD;
		}
		if (port->ip_immovable_receive) {
			statusp->mps_flags |= MACH_PORT_STATUS_FLAG_GUARD_IMMOVABLE_RECEIVE;
		}
	}
	if (port->ip_no_grant) {
		statusp->mps_flags |= MACH_PORT_STATUS_FLAG_NO_GRANT;
	}
	return;
}

kern_return_t
mach_port_get_attributes(
	ipc_space_t             space,
	mach_port_name_t        name,
	int                     flavor,
	mach_port_info_t        info,
	mach_msg_type_number_t  *count)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	switch (flavor) {
	case MACH_PORT_LIMITS_INFO: {
		mach_port_limits_t *lp = (mach_port_limits_t *)info;

		if (*count < MACH_PORT_LIMITS_INFO_COUNT) {
			return KERN_FAILURE;
		}

		if (!MACH_PORT_VALID(name)) {
			*count = 0;
			break;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		lp->mpl_qlimit = port->ip_messages.imq_qlimit;
		*count = MACH_PORT_LIMITS_INFO_COUNT;
		ip_mq_unlock(port);
		break;
	}

	case MACH_PORT_RECEIVE_STATUS: {
		mach_port_status_t *statusp = (mach_port_status_t *)info;

		if (*count < MACH_PORT_RECEIVE_STATUS_COUNT) {
			return KERN_FAILURE;
		}

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */
		mach_port_get_status_helper(port, statusp);
		*count = MACH_PORT_RECEIVE_STATUS_COUNT;
		ip_mq_unlock(port);
		break;
	}

	case MACH_PORT_DNREQUESTS_SIZE: {
		ipc_port_request_table_t table;

		if (*count < MACH_PORT_DNREQUESTS_SIZE_COUNT) {
			return KERN_FAILURE;
		}

		if (!MACH_PORT_VALID(name)) {
			*(int *)info = 0;
			break;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		table = port->ip_requests;
		if (table == NULL) {
			*(int *)info = 0;
		} else {
			*(int *)info = (int)ipc_port_request_table_count(table);
		}
		*count = MACH_PORT_DNREQUESTS_SIZE_COUNT;
		ip_mq_unlock(port);
		break;
	}

	case MACH_PORT_INFO_EXT: {
		mach_port_info_ext_t *mp_info = (mach_port_info_ext_t *)info;
		if (*count < MACH_PORT_INFO_EXT_COUNT) {
			return KERN_FAILURE;
		}

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */
		mach_port_get_status_helper(port, &mp_info->mpie_status);
		mp_info->mpie_boost_cnt = port->ip_impcount;
		*count = MACH_PORT_INFO_EXT_COUNT;
		ip_mq_unlock(port);
		break;
	}

	case MACH_PORT_SERVICE_THROTTLED: {
		boolean_t *is_throttled = info;

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		if (!port->ip_service_port) {
			ip_mq_unlock(port);
			return KERN_INVALID_CAPABILITY;
		}

		assert(port->ip_splabel != NULL);
		*is_throttled = ipc_service_port_label_is_throttled((ipc_service_port_label_t)port->ip_splabel);
		*count = MACH_PORT_SERVICE_THROTTLED_COUNT;
		ip_mq_unlock(port);
		break;
	}

	default:
		return KERN_INVALID_ARGUMENT;
		/*NOTREACHED*/
	}

	return KERN_SUCCESS;
}

kern_return_t
mach_port_get_attributes_from_user(
	mach_port_t             port,
	mach_port_name_t        name,
	int                     flavor,
	mach_port_info_t        info,
	mach_msg_type_number_t  *count)
{
	kern_return_t kr;

	ipc_space_t space = convert_port_to_space_read_no_eval(port);

	if (space == IPC_SPACE_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = mach_port_get_attributes(space, name, flavor, info, count);

	ipc_space_release(space);
	return kr;
}

kern_return_t
mach_port_set_attributes(
	ipc_space_t             space,
	mach_port_name_t        name,
	int                     flavor,
	mach_port_info_t        info,
	mach_msg_type_number_t  count)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	switch (flavor) {
	case MACH_PORT_LIMITS_INFO: {
		mach_port_limits_t *mplp = (mach_port_limits_t *)info;

		if (count < MACH_PORT_LIMITS_INFO_COUNT) {
			return KERN_FAILURE;
		}

		if (mplp->mpl_qlimit > MACH_PORT_QLIMIT_MAX) {
			return KERN_INVALID_VALUE;
		}

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		ipc_mqueue_set_qlimit_locked(&port->ip_messages, mplp->mpl_qlimit);
		ip_mq_unlock(port);
		break;
	}
	case MACH_PORT_DNREQUESTS_SIZE: {
		if (count < MACH_PORT_DNREQUESTS_SIZE_COUNT) {
			return KERN_FAILURE;
		}

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */
		ip_mq_unlock(port);
		break;
	}
	case MACH_PORT_TEMPOWNER: {
		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		ipc_importance_task_t release_imp_task = IIT_NULL;
		natural_t assertcnt = 0;

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		/*
		 * don't allow temp-owner importance donation if user
		 * associated it with a kobject already (timer, host_notify target),
		 * or is a special reply port.
		 */
		if (ip_is_kobject(port) || port->ip_specialreply) {
			ip_mq_unlock(port);
			return KERN_INVALID_ARGUMENT;
		}

		if (port->ip_tempowner != 0) {
			if (IIT_NULL != ip_get_imp_task(port)) {
				release_imp_task = ip_get_imp_task(port);
				port->ip_imp_task = IIT_NULL;
				assertcnt = port->ip_impcount;
			}
		} else {
			assertcnt = port->ip_impcount;
		}

		port->ip_impdonation = 1;
		port->ip_tempowner = 1;
		ip_mq_unlock(port);

#if IMPORTANCE_INHERITANCE
		/* drop assertions from previous destination task */
		if (release_imp_task != IIT_NULL) {
			assert(ipc_importance_task_is_any_receiver_type(release_imp_task));
			if (assertcnt > 0) {
				ipc_importance_task_drop_internal_assertion(release_imp_task, assertcnt);
			}
			ipc_importance_task_release(release_imp_task);
		} else if (assertcnt > 0) {
			release_imp_task = current_task()->task_imp_base;
			if (release_imp_task != IIT_NULL &&
			    ipc_importance_task_is_any_receiver_type(release_imp_task)) {
				ipc_importance_task_drop_internal_assertion(release_imp_task, assertcnt);
			}
		}
#else
		if (release_imp_task != IIT_NULL) {
			ipc_importance_task_release(release_imp_task);
		}
#endif /* IMPORTANCE_INHERITANCE */

		break;

#if IMPORTANCE_INHERITANCE
	case MACH_PORT_DENAP_RECEIVER:
	case MACH_PORT_IMPORTANCE_RECEIVER:
		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}

		/*
		 * don't allow importance donation if user associated
		 * it with a kobject already (timer, host_notify target),
		 * or is a special reply port.
		 */
		if (ip_is_kobject(port) || port->ip_specialreply) {
			ip_mq_unlock(port);
			return KERN_INVALID_ARGUMENT;
		}

		/* port is locked and active */
		port->ip_impdonation = 1;
		ip_mq_unlock(port);

		break;
#endif /* IMPORTANCE_INHERITANCE */
	}

	case MACH_PORT_SERVICE_THROTTLED: {
		boolean_t is_throttled = *info;

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		if (!port->ip_service_port) {
			ip_mq_unlock(port);
			return KERN_INVALID_CAPABILITY;
		}

		assert(port->ip_splabel != NULL);
		if (is_throttled) {
			ipc_service_port_label_set_flag(port->ip_splabel, ISPL_FLAGS_THROTTLED);
		} else {
			ipc_service_port_label_clear_flag(port->ip_splabel, ISPL_FLAGS_THROTTLED);
		}
		ip_mq_unlock(port);
		break;
	}

	default:
		return KERN_INVALID_ARGUMENT;
		/*NOTREACHED*/
	}
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_insert_member [kernel call]
 *	Purpose:
 *		Add the receive right, specified by name, to
 *		a portset.
 *		The port cannot already be a member of the set.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Moved the port.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	name didn't denote a right.
 *		KERN_INVALID_RIGHT	name didn't denote a receive right.
 *		KERN_INVALID_NAME	pset_name didn't denote a right.
 *		KERN_INVALID_RIGHT	pset_name didn't denote a portset right.
 *		KERN_ALREADY_IN_SET	name was already a member of pset.
 */

kern_return_t
mach_port_insert_member(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_name_t        psname)
{
	ipc_object_t obj;
	ipc_object_t psobj;
	kern_return_t kr;
	ipc_port_t port = IP_NULL;
	ipc_pset_t pset = IPS_NULL;
	waitq_link_t link;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name) || !MACH_PORT_VALID(psname)) {
		return KERN_INVALID_RIGHT;
	}

	link = waitq_link_alloc(WQT_PORT_SET);

	kr = ipc_object_translate_two(space,
	    name, MACH_PORT_RIGHT_RECEIVE, &obj,
	    psname, MACH_PORT_RIGHT_PORT_SET, &psobj);
	if (kr != KERN_SUCCESS) {
		goto done;
	}

	/* obj and psobj are locked (and were locked in that order) */
	assert(psobj != IO_NULL);
	assert(obj != IO_NULL);

	port = ip_object_to_port(obj);
	pset = ips_object_to_pset(psobj);

	kr = ipc_mqueue_add_locked(&port->ip_messages, pset, &link);

	ips_mq_unlock(pset);
	ip_mq_unlock(port);

done:
	if (link.wqlh) {
		waitq_link_free(WQT_PORT_SET, link);
	}

	return kr;
}

/*
 *	Routine:	mach_port_extract_member [kernel call]
 *	Purpose:
 *		Remove a port from one portset that it is a member of.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Moved the port.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	Member didn't denote a right.
 *		KERN_INVALID_RIGHT	Member didn't denote a receive right.
 *		KERN_INVALID_NAME	After didn't denote a right.
 *		KERN_INVALID_RIGHT	After didn't denote a port set right.
 *		KERN_NOT_IN_SET
 *			After is MACH_PORT_NULL and Member isn't in a port set.
 */

kern_return_t
mach_port_extract_member(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_name_t        psname)
{
	ipc_object_t psobj;
	ipc_object_t obj;
	kern_return_t kr;
	ipc_port_t port = IP_NULL;
	ipc_pset_t pset = IPS_NULL;
	waitq_link_t link;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name) || !MACH_PORT_VALID(psname)) {
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_object_translate_two(space,
	    name, MACH_PORT_RIGHT_RECEIVE, &obj,
	    psname, MACH_PORT_RIGHT_PORT_SET, &psobj);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/* obj and psobj are both locked (and were locked in that order) */
	assert(psobj != IO_NULL);
	assert(obj != IO_NULL);

	port = ip_object_to_port(obj);
	pset = ips_object_to_pset(psobj);

	link = waitq_unlink_locked(&port->ip_waitq, &pset->ips_wqset);

	ips_mq_unlock(pset);
	ip_mq_unlock(port);

	if (link.wqlh) {
		waitq_link_free(WQT_PORT_SET, link);
		return KERN_SUCCESS;
	}

	return KERN_NOT_IN_SET;
}

/*
 *	task_set_port_space:
 *
 *	Obsolete. Set port name space of task to specified size.
 */
kern_return_t
task_set_port_space(
	ipc_space_t     space,
	__unused int    table_entries)
{
	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_guard_locked [helper routine]
 *	Purpose:
 *		Sets a new guard for a locked port.
 *	Conditions:
 *		Port Locked.
 *	Returns:
 *		KERN_SUCCESS		Port Guarded.
 *		KERN_INVALID_ARGUMENT	Port already contains a context/guard.
 */
static kern_return_t
mach_port_guard_locked(
	ipc_port_t              port,
	uint64_t                guard,
	uint64_t                flags)
{
	if (port->ip_context) {
		return KERN_INVALID_ARGUMENT;
	}

	int strict = (flags & MPG_STRICT)? 1 : 0;
	int immovable_receive = (flags & MPG_IMMOVABLE_RECEIVE)? 1 : 0;

	port->ip_context = guard;
	port->ip_guarded = 1;
	port->ip_strict_guard = strict;
	/* ip_immovable_receive bit is sticky and can't be un-guarded */
	if (!port->ip_immovable_receive) {
		port->ip_immovable_receive = immovable_receive;
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_unguard_locked [helper routine]
 *	Purpose:
 *		Removes guard for a locked port.
 *	Conditions:
 *		Port Locked.
 *	Returns:
 *		KERN_SUCCESS		Port Unguarded.
 *		KERN_INVALID_ARGUMENT	Port is either unguarded already or guard mismatch.
 *					This also raises a EXC_GUARD exception.
 */
static kern_return_t
mach_port_unguard_locked(
	ipc_port_t              port,
	mach_port_name_t        name,
	uint64_t                guard)
{
	/* Port locked and active */
	if (!port->ip_guarded) {
		/* Port already unguarded; Raise exception */
		mach_port_guard_exception(name, guard, 0, kGUARD_EXC_UNGUARDED);
		return KERN_INVALID_ARGUMENT;
	}

	if (port->ip_context != guard) {
		/* Incorrect guard; Raise exception */
		mach_port_guard_exception(name, guard, port->ip_context, kGUARD_EXC_INCORRECT_GUARD);
		return KERN_INVALID_ARGUMENT;
	}

	port->ip_context = 0;
	port->ip_guarded = port->ip_strict_guard = 0;
	/* Don't clear the ip_immovable_receive bit */

	return KERN_SUCCESS;
}


/*
 *	Routine:	mach_port_guard_exception [helper routine]
 *	Purpose:
 *		Marks the thread with AST_GUARD for mach port guard violation.
 *		Also saves exception info in thread structure.
 *	Conditions:
 *		None.
 *	Returns:
 *		KERN_FAILURE		Thread marked with AST_GUARD.
 */
void
mach_port_guard_exception(
	mach_port_name_t        name,
	__unused uint64_t       inguard,
	uint64_t                        portguard,
	unsigned                        reason)
{
	mach_exception_code_t code = 0;
	EXC_GUARD_ENCODE_TYPE(code, GUARD_TYPE_MACH_PORT);
	EXC_GUARD_ENCODE_FLAVOR(code, reason);
	EXC_GUARD_ENCODE_TARGET(code, name);
	mach_exception_subcode_t subcode = (uint64_t)portguard;
	thread_t t = current_thread();
	boolean_t fatal = FALSE;

	if (reason <= MAX_OPTIONAL_kGUARD_EXC_CODE &&
	    (get_threadtask(t)->task_exc_guard & TASK_EXC_GUARD_MP_FATAL)) {
		fatal = TRUE;
	} else if (reason <= MAX_FATAL_kGUARD_EXC_CODE) {
		fatal = TRUE;
	}
	thread_guard_violation(t, code, subcode, fatal);
}

/*
 * Deliver a soft or hard immovable guard exception.
 *
 * Conditions: port is marked as immovable.
 */
void
mach_port_guard_exception_immovable(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_t             port,
	uint64_t                portguard)
{
	if (space == current_space()) {
		assert(ip_is_immovable_send(port));

		boolean_t hard = task_get_control_port_options(current_task()) & TASK_CONTROL_PORT_IMMOVABLE_HARD;

		if (ip_is_control(port)) {
			assert(task_is_immovable(current_task()));
			mach_port_guard_exception(name, 0, portguard,
			    hard ? kGUARD_EXC_IMMOVABLE : kGUARD_EXC_IMMOVABLE_NON_FATAL);
		} else {
			/* always fatal exception for non-control port violation */
			mach_port_guard_exception(name, 0, portguard, kGUARD_EXC_IMMOVABLE);
		}
	}
}

/*
 * Deliver a soft or hard immovable guard exception.
 *
 * Conditions: port is marked as immovable and pinned.
 */
void
mach_port_guard_exception_pinned(
	ipc_space_t             space,
	mach_port_name_t        name,
	__assert_only mach_port_t port,
	uint64_t                portguard)
{
	if (space == current_space()) {
		assert(ip_is_immovable_send(port));
		assert(ip_is_control(port)); /* only task/thread control ports can be pinned */

		boolean_t hard = task_get_control_port_options(current_task()) & TASK_CONTROL_PORT_PINNED_HARD;

		assert(task_is_pinned(current_task()));

		mach_port_guard_exception(name, 0, portguard,
		    hard ? kGUARD_EXC_MOD_REFS : kGUARD_EXC_MOD_REFS_NON_FATAL);
	}
}

/*
 *	Routine:	mach_port_guard_ast
 *	Purpose:
 *		Raises an exception for mach port guard violation.
 *	Conditions:
 *		None.
 *	Returns:
 *		None.
 */

void
mach_port_guard_ast(thread_t t,
    mach_exception_data_type_t code, mach_exception_data_type_t subcode)
{
	unsigned int reason = EXC_GUARD_DECODE_GUARD_FLAVOR(code);
	task_t task = get_threadtask(t);
	unsigned int behavior = task->task_exc_guard;
	bool fatal = true;

	assert(task == current_task());
	assert(task != kernel_task);

	if (reason <= MAX_FATAL_kGUARD_EXC_CODE) {
		/*
		 * Fatal Mach port guards - always delivered synchronously if dev mode is on.
		 * Check if anyone has registered for Synchronous EXC_GUARD, if yes then,
		 * deliver it synchronously and then kill the process, else kill the process
		 * and deliver the exception via EXC_CORPSE_NOTIFY.
		 */

		int flags = PX_DEBUG_NO_HONOR;
		exception_info_t info = {
			.os_reason = OS_REASON_GUARD,
			.exception_type = EXC_GUARD,
			.mx_code = code,
			.mx_subcode = subcode,
		};

		if (task_exception_notify(EXC_GUARD, code, subcode, fatal) == KERN_SUCCESS) {
			flags |= PX_PSIGNAL;
		}
		exit_with_mach_exception(get_bsdtask_info(task), info, flags);
	} else {
		/*
		 * Mach port guards controlled by task settings.
		 */

		/* Is delivery enabled */
		if ((behavior & TASK_EXC_GUARD_MP_DELIVER) == 0) {
			return;
		}

		/* If only once, make sure we're that once */
		while (behavior & TASK_EXC_GUARD_MP_ONCE) {
			uint32_t new_behavior = behavior & ~TASK_EXC_GUARD_MP_DELIVER;

			if (OSCompareAndSwap(behavior, new_behavior, &task->task_exc_guard)) {
				break;
			}
			behavior = task->task_exc_guard;
			if ((behavior & TASK_EXC_GUARD_MP_DELIVER) == 0) {
				return;
			}
		}
		fatal = (task->task_exc_guard & TASK_EXC_GUARD_MP_FATAL)
		    && (reason <= MAX_OPTIONAL_kGUARD_EXC_CODE);
		kern_return_t sync_exception_result;
		sync_exception_result = task_exception_notify(EXC_GUARD, code, subcode, fatal);

		if (task->task_exc_guard & TASK_EXC_GUARD_MP_FATAL) {
			if (reason > MAX_OPTIONAL_kGUARD_EXC_CODE) {
				/* generate a simulated crash if not handled synchronously */
				if (sync_exception_result != KERN_SUCCESS) {
					task_violated_guard(code, subcode, NULL, TRUE);
				}
			} else {
				/*
				 * Only generate crash report if synchronous EXC_GUARD wasn't handled,
				 * but it has to die regardless.
				 */

				int flags = PX_DEBUG_NO_HONOR;
				exception_info_t info = {
					.os_reason = OS_REASON_GUARD,
					.exception_type = EXC_GUARD,
					.mx_code = code,
					.mx_subcode = subcode
				};

				if (sync_exception_result == KERN_SUCCESS) {
					flags |= PX_PSIGNAL;
				}

				exit_with_mach_exception(get_bsdtask_info(task), info, flags);
			}
		} else if (task->task_exc_guard & TASK_EXC_GUARD_MP_CORPSE) {
			/* Raise exception via corpse fork if not handled synchronously */
			if (sync_exception_result != KERN_SUCCESS) {
				task_violated_guard(code, subcode, NULL, TRUE);
			}
		}
	}
}

/*
 *	Routine:	mach_port_construct [kernel call]
 *	Purpose:
 *		Constructs a mach port with the provided set of options.
 *	Conditions:
 *		None.
 *	Returns:
 *		KERN_SUCCESS		The right is allocated.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 *      KERN_INVALID_VALUE  Invalid value passed in options
 *      KERN_INVALID_ARGUMENT   Invalid arguments passed in options
 *		KERN_NO_SPACE		No room in space for another right.
 *		KERN_FAILURE		Illegal option values requested.
 */

kern_return_t
mach_port_construct(
	ipc_space_t             space,
	mach_port_options_t     *options,
	uint64_t                context,
	mach_port_name_t        *name)
{
	kern_return_t           kr;
	ipc_port_t              port;
	ipc_port_init_flags_t   init_flags = IPC_PORT_INIT_MESSAGE_QUEUE;
	void *port_splabel = NULL;
	bool filter_msgs = FALSE;
	struct mach_service_port_info sp_info = {};
	size_t sp_name_length = 0;
	user_addr_t service_port_info = 0;

	uint32_t at_most_one_flags = options->flags & (MPO_SERVICE_PORT | MPO_CONNECTION_PORT | MPO_TG_BLOCK_TRACKING);
	if (at_most_one_flags & (at_most_one_flags - 1)) {
		/* at most one of the listed flags can be set */
		return KERN_INVALID_ARGUMENT;
	}

	at_most_one_flags = options->flags & (MPO_REPLY_PORT | MPO_ENFORCE_REPLY_PORT_SEMANTICS |
	    MPO_EXCEPTION_PORT | MPO_PROVISIONAL_REPLY_PORT);
	if (at_most_one_flags & (at_most_one_flags - 1)) {
		/* at most one of the listed flags can be set */
		return KERN_INVALID_ARGUMENT;
	}

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (options->flags & MPO_INSERT_SEND_RIGHT) {
		init_flags |= IPC_PORT_INIT_MAKE_SEND_RIGHT;
	}

	if (options->flags & MPO_FILTER_MSG) {
		init_flags |= IPC_PORT_INIT_FILTER_MESSAGE;
	}

	if (options->flags & MPO_REPLY_PORT) {
		init_flags |= IPC_PORT_INIT_REPLY;
	}

	if (options->flags & MPO_ENFORCE_REPLY_PORT_SEMANTICS) {
		init_flags |= IPC_PORT_ENFORCE_REPLY_PORT_SEMANTICS;
	}

	if (options->flags & MPO_EXCEPTION_PORT) {
		init_flags |= IPC_PORT_INIT_EXCEPTION_PORT;
	}

	if (options->flags & MPO_PROVISIONAL_REPLY_PORT) {
		if (provisional_reply_port_enforced) {
			init_flags |= IPC_PORT_INIT_REPLY;
		} else {
			init_flags |= IPC_PORT_INIT_PROVISIONAL_REPLY;
		}
	}

	if (options->flags & MPO_TG_BLOCK_TRACKING) {
		/* Check the task role to allow only TASK_GRAPHICS_SERVER to set this option */
		if (proc_get_effective_task_policy(current_task(),
		    TASK_POLICY_ROLE) != TASK_GRAPHICS_SERVER) {
			return KERN_DENIED;
		}

		/*
		 * Check the work interval port passed in to make sure it is the render server type.
		 * Since the creation of the render server work interval is privileged, this check
		 * acts as a guard to make sure only the render server is setting the thread group
		 * blocking behavior on the port.
		 */
		mach_port_name_t wi_port_name = options->work_interval_port;
		if (work_interval_port_type_render_server(wi_port_name) == false) {
			return KERN_INVALID_ARGUMENT;
		}
		init_flags |= IPC_PORT_INIT_TG_BLOCK_TRACKING;
	}

	if (options->flags & MPO_SERVICE_PORT) {
#if !(DEVELOPMENT || DEBUG)
#if CONFIG_COALITIONS
		/*
		 * Allow only launchd to add the service port labels
		 * Not enforcing on development/debug kernels to
		 * support testing
		 */
		if (!task_is_in_privileged_coalition(current_task(), COALITION_TYPE_JETSAM)) {
			return KERN_DENIED;
		}
#else /* CONFIG_COALITIONS */
		/*
		 * This flag is not used by launchd on simulators
		 */
		if (proc_isinitproc(get_bsdtask_info(current_task()))) {
			return KERN_DENIED;
		}
#endif /* CONFIG_COALITIONS */
#endif /* !(DEVELOPMENT || DEBUG) */

		if (task_has_64Bit_addr(current_task())) {
			service_port_info = CAST_USER_ADDR_T(options->service_port_info64);
		} else {
			service_port_info = CAST_USER_ADDR_T(options->service_port_info32);
		}

		if (!service_port_info) {
			return KERN_INVALID_ARGUMENT;
		}

		if (copyin(service_port_info, (void *)&sp_info, sizeof(sp_info))) {
			return KERN_MEMORY_ERROR;
		}

		sp_name_length = strnlen(sp_info.mspi_string_name, MACH_SERVICE_PORT_INFO_STRING_NAME_MAX_BUF_LEN);
		if (sp_name_length >= (MACH_SERVICE_PORT_INFO_STRING_NAME_MAX_BUF_LEN)) {
			return KERN_INVALID_ARGUMENT;
		}

		kr = ipc_service_port_label_alloc(&sp_info, &port_splabel);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* Always filter messages on service ports */
		init_flags |= IPC_PORT_INIT_FILTER_MESSAGE;
	}

	if (options->flags & MPO_CONNECTION_PORT) {
		if (!options->service_port_name) {
			return KERN_INVALID_ARGUMENT;
		}

		kr = ipc_service_port_derive_sblabel(options->service_port_name, &port_splabel, &filter_msgs);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		if (filter_msgs) {
			init_flags |= IPC_PORT_INIT_FILTER_MESSAGE;
		}
	}


	if (options->flags & MPO_QLIMIT) {
		const mach_msg_type_number_t count = sizeof(options->mpl) / sizeof(int);
		static_assert(count >= MACH_PORT_LIMITS_INFO_COUNT);

		if (options->mpl.mpl_qlimit > MACH_PORT_QLIMIT_MAX) {
			return KERN_INVALID_VALUE;
		}
	}

	/* Allocate a new port in the IPC space */
	kr = ipc_port_alloc(space, init_flags, name, &port);
	if (kr != KERN_SUCCESS) {
		if (port_splabel != NULL) {
			ipc_service_port_label_dealloc(port_splabel,
			    (options->flags & MPO_SERVICE_PORT));
		}
		return kr;
	}
	/* Port locked and active */

	/* Mutate the new port based on flags - see above for error checks */
	if (options->flags & MPO_QLIMIT) {
		ipc_mqueue_set_qlimit_locked(&port->ip_messages, options->mpl.mpl_qlimit);
	}

	if (options->flags & (MPO_IMPORTANCE_RECEIVER | MPO_DENAP_RECEIVER | MPO_TEMPOWNER)) {
		assert(!port->ip_specialreply);

		port->ip_impdonation = 1;
		if (options->flags & MPO_TEMPOWNER) {
			port->ip_tempowner = 1;
		}
	}

	if (port_splabel != NULL) {
		port->ip_service_port = (bool)(options->flags & MPO_SERVICE_PORT);
		port->ip_splabel = port_splabel;

		/* Check if this is a service port */
		if (service_port_defense_enabled && port->ip_service_port) {
			port->ip_immovable_receive = true;
		}

		/* Check if this is a libxpc connection port */
		if (!port->ip_service_port) {
			assert(options->flags & MPO_CONNECTION_PORT);
			port->ip_immovable_send = true;
			port->ip_immovable_receive = true;
		}
	}

	if (options->flags & MPO_CONTEXT_AS_GUARD) {
		uint64_t flags = 0;
		if (options->flags & MPO_STRICT) {
			flags |= MPG_STRICT;
		}
		if (options->flags & MPO_IMMOVABLE_RECEIVE) {
			flags |= MPG_IMMOVABLE_RECEIVE;
		}
		kr = mach_port_guard_locked(port, context, flags);
		/* A newly allocated and locked port should always be guarded successfully */
		assert(kr == KERN_SUCCESS);
		if (options->flags & MPO_SERVICE_PORT) {
			/*
			 * Setting the guard on a service port triggers a special port destroyed notification
			 * that restores the guard when the receive right moves back to launchd. This
			 * must be a strict guard.
			 */
			assert((options->flags & MPO_STRICT) == MPO_STRICT);
			ipc_service_port_label_set_attr(port_splabel, *name, (mach_port_context_t)context);
		}
	} else {
		port->ip_context = context;
		if (options->flags & MPO_SERVICE_PORT) {
			ipc_service_port_label_set_attr(port_splabel, *name, 0);
		}
	}

	/* Unlock port */
	ip_mq_unlock(port);

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_port_destruct [kernel call]
 *	Purpose:
 *		Destroys a mach port with appropriate guard
 *	Conditions:
 *		None.
 *	Returns:
 *		KERN_SUCCESS		The name is destroyed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	The right isn't correct.
 *		KERN_INVALID_VALUE	The delta for send right is incorrect.
 *		KERN_INVALID_ARGUMENT	Port is either unguarded already or guard mismatch.
 *					This also raises a EXC_GUARD exception.
 */

kern_return_t
mach_port_destruct(
	ipc_space_t             space,
	mach_port_name_t        name,
	mach_port_delta_t       srdelta,
	uint64_t                guard)
{
	kern_return_t           kr;
	ipc_entry_t             entry;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_NAME;
	}

	/* Remove reference for receive right */
	kr = ipc_right_lookup_write(space, name, &entry);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_NAME);
		return kr;
	}
	/* space is write-locked and active */
	kr = ipc_right_destruct(space, name, entry, srdelta, guard);    /* unlocks */

	return kr;
}

/*
 *	Routine:	mach_port_guard [kernel call]
 *	Purpose:
 *		Guard a mach port with specified guard value.
 *		The context field of the port is used as the guard.
 *	Conditions:
 *		None.
 *	Returns:
 *		KERN_SUCCESS		The name is destroyed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	The right isn't correct.
 *		KERN_INVALID_ARGUMENT	Port already contains a context/guard.
 */
kern_return_t
mach_port_guard(
	ipc_space_t             space,
	mach_port_name_t        name,
	uint64_t                guard,
	boolean_t               strict)
{
	kern_return_t           kr;
	ipc_port_t              port;
	uint64_t flags = 0;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_NAME;
	}

	/* Guard can be applied only to receive rights */
	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0,
		    ((KERN_INVALID_NAME == kr) ?
		    kGUARD_EXC_INVALID_NAME :
		    kGUARD_EXC_INVALID_RIGHT));
		return kr;
	}

	/* Port locked and active */
	if (strict) {
		flags = MPG_STRICT;
	}

	kr = mach_port_guard_locked(port, guard, flags);
	ip_mq_unlock(port);

	if (KERN_INVALID_ARGUMENT == kr) {
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_ARGUMENT);
	}

	return kr;
}

/*
 *	Routine:	mach_port_unguard [kernel call]
 *	Purpose:
 *		Unguard a mach port with specified guard value.
 *	Conditions:
 *		None.
 *	Returns:
 *		KERN_SUCCESS		The name is destroyed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	The right isn't correct.
 *		KERN_INVALID_ARGUMENT	Port is either unguarded already or guard mismatch.
 *					This also raises a EXC_GUARD exception.
 */
kern_return_t
mach_port_unguard(
	ipc_space_t             space,
	mach_port_name_t        name,
	uint64_t                guard)
{
	kern_return_t           kr;
	ipc_port_t              port;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_NAME;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0,
		    ((KERN_INVALID_NAME == kr) ?
		    kGUARD_EXC_INVALID_NAME :
		    kGUARD_EXC_INVALID_RIGHT));
		return kr;
	}

	/* Port locked and active */
	kr = mach_port_unguard_locked(port, name, guard);
	ip_mq_unlock(port);

	return kr;
}

/*
 *	Routine:	mach_port_guard_with_flags [kernel call]
 *	Purpose:
 *		Guard a mach port with specified guard value and guard flags.
 *		The context field of the port is used as the guard.
 *	Conditions:
 *		Should hold receive right for that port
 *	Returns:
 *		KERN_SUCCESS		The name is destroyed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	The right isn't correct.
 *		KERN_INVALID_ARGUMENT	Port already contains a context/guard.
 *		KERN_INVALID_CAPABILITY Cannot set MPG_IMMOVABLE_RECEIVE flag for a port with
 *					a movable port-destroyed notification port
 */
kern_return_t
mach_port_guard_with_flags(
	ipc_space_t             space,
	mach_port_name_t        name,
	uint64_t                guard,
	uint64_t                flags)
{
	kern_return_t           kr;
	ipc_port_t              port;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_NAME;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0,
		    ((KERN_INVALID_NAME == kr) ?
		    kGUARD_EXC_INVALID_NAME :
		    kGUARD_EXC_INVALID_RIGHT));
		return kr;
	}

	/* Port locked and active */
	kr = mach_port_guard_locked(port, guard, flags);
	ip_mq_unlock(port);

	if (KERN_INVALID_ARGUMENT == kr) {
		mach_port_guard_exception(name, 0, 0, kGUARD_EXC_INVALID_ARGUMENT);
	}

	return kr;
}

/*
 *	Routine:	mach_port_swap_guard [kernel call]
 *	Purpose:
 *		Swap guard value.
 *	Conditions:
 *		Port should already be guarded.
 *	Returns:
 *		KERN_SUCCESS		The name is destroyed.
 *		KERN_INVALID_TASK	The space is null.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	The name doesn't denote a right.
 *		KERN_INVALID_RIGHT	The right isn't correct.
 *		KERN_INVALID_ARGUMENT	Port doesn't contain a guard; is strictly guarded
 *					or the old_guard doesnt match the context
 */
kern_return_t
mach_port_swap_guard(
	ipc_space_t             space,
	mach_port_name_t        name,
	uint64_t                old_guard,
	uint64_t                new_guard)
{
	kern_return_t           kr;
	ipc_port_t              port;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_NAME;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		mach_port_guard_exception(name, 0, 0,
		    ((KERN_INVALID_NAME == kr) ?
		    kGUARD_EXC_INVALID_NAME :
		    kGUARD_EXC_INVALID_RIGHT));
		return kr;
	}

	/* Port locked and active */
	if (!port->ip_guarded) {
		ip_mq_unlock(port);
		mach_port_guard_exception(name, old_guard, 0, kGUARD_EXC_UNGUARDED);
		return KERN_INVALID_ARGUMENT;
	}

	if (port->ip_strict_guard) {
		uint64_t portguard = port->ip_context;
		ip_mq_unlock(port);
		/* For strictly guarded ports, disallow overwriting context; Raise Exception */
		mach_port_guard_exception(name, old_guard, portguard, kGUARD_EXC_SET_CONTEXT);
		return KERN_INVALID_ARGUMENT;
	}

	if (port->ip_context != old_guard) {
		uint64_t portguard = port->ip_context;
		ip_mq_unlock(port);
		mach_port_guard_exception(name, old_guard, portguard, kGUARD_EXC_INCORRECT_GUARD);
		return KERN_INVALID_ARGUMENT;
	}

	port->ip_context = new_guard;

	ip_mq_unlock(port);

	return KERN_SUCCESS;
}

kern_return_t
mach_port_is_connection_for_service(
	ipc_space_t space,
	mach_port_name_t connection_port_name,
	mach_port_name_t service_port_name,
	uint64_t *filter_policy_id)
{
	mach_port_t service_port;
	mach_port_t connection_port;
	void *service_port_sblabel = NULL;
	void *conn_port_sblabel = NULL;

	kern_return_t ret;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(connection_port_name) || !MACH_PORT_VALID(service_port_name)) {
		return KERN_INVALID_NAME;
	}

	if (!filter_policy_id) {
		return KERN_INVALID_ARGUMENT;
	}

	if (!mach_msg_filter_at_least(MACH_MSG_FILTER_CALLBACKS_VERSION_1)) {
		return KERN_NOT_SUPPORTED;
	}

	ret = ipc_port_translate_receive(space, service_port_name, &service_port);
	if (ret) {
		return ret;
	}

	if (!service_port->ip_service_port) {
		ip_mq_unlock(service_port);
		return KERN_INVALID_CAPABILITY;
	}

	/* Port is locked and active */
	service_port_sblabel = ipc_service_port_get_sblabel(service_port);
	if (service_port_sblabel) {
		mach_msg_filter_retain_sblabel_callback(service_port_sblabel);
	}
	ip_mq_unlock(service_port);

	if (!service_port_sblabel) {
		/* Nothing to check */
		*filter_policy_id = 0;
		return KERN_SUCCESS;
	}

	ret = ipc_port_translate_receive(space, connection_port_name, &connection_port);
	if (ret) {
		mach_msg_filter_dealloc_service_port_sblabel_callback(service_port_sblabel);
		return ret;
	}
	/* Port is locked and active */
	conn_port_sblabel = ipc_service_port_get_sblabel(connection_port);
	if (conn_port_sblabel) {
		mach_msg_filter_retain_sblabel_callback(conn_port_sblabel);
	}
	ip_mq_unlock(connection_port);

	/* This callback will release the sblabel references */
	ret = mach_msg_filter_get_connection_port_filter_policy_callback(service_port_sblabel,
	    conn_port_sblabel, filter_policy_id);

	return ret;
}

#if CONFIG_SERVICE_PORT_INFO
kern_return_t
mach_port_get_service_port_info(
	ipc_space_read_t           space,
	mach_port_name_t           name,
	mach_service_port_info_t   sp_info)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (sp_info == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (!MACH_PORT_VALID(name)) {
		return KERN_INVALID_RIGHT;
	}

	kr = ipc_port_translate_receive(space, name, &port);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	/* port is locked and active */

	if (!port->ip_service_port) {
		ip_mq_unlock(port);
		return KERN_INVALID_CAPABILITY;
	}

	assert(port->ip_splabel != NULL);
	ipc_service_port_label_get_info((ipc_service_port_label_t)port->ip_splabel, sp_info);
	ip_mq_unlock(port);

	return KERN_SUCCESS;
}

#else /* CONFIG_SERVICE_PORT_INFO */

kern_return_t
mach_port_get_service_port_info(
	__unused ipc_space_read_t           space,
	__unused mach_port_name_t           name,
	__unused mach_service_port_info_t   sp_info)
{
	return KERN_NOT_SUPPORTED;
}
#endif /* CONFIG_SERVICE_PORT_INFO */

kern_return_t
mach_port_assert_attributes(
	ipc_space_t             space,
	mach_port_name_t        name,
	int                     flavor,
	mach_port_info_t        info,
	mach_msg_type_number_t  count)
{
	ipc_port_t port;
	kern_return_t kr;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	switch (flavor) {
	case MACH_PORT_GUARD_INFO: {
		mach_port_guard_info_t *mpgi = (mach_port_guard_info_t *)(void *)info;

		if (count < MACH_PORT_GUARD_INFO_COUNT) {
			return KERN_FAILURE;
		}

		if (!MACH_PORT_VALID(name)) {
			return KERN_INVALID_RIGHT;
		}

		kr = ipc_port_translate_receive(space, name, &port);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		/* port is locked and active */

		/* Check if guard value matches else kill the process using fatal guard exception */
		if (!port->ip_guarded) {
			ip_mq_unlock(port);
			/* Port already unguarded; Raise exception */
			mach_port_guard_exception(name, mpgi->mpgi_guard, 0, kGUARD_EXC_UNGUARDED);
			return KERN_INVALID_ARGUMENT;
		}

		if (!port->ip_strict_guard || (port->ip_context != mpgi->mpgi_guard)) {
			ip_mq_unlock(port);
			/* Incorrect guard; Raise exception */
			mach_port_guard_exception(name, mpgi->mpgi_guard, port->ip_context, kGUARD_EXC_INCORRECT_GUARD);
			return KERN_INVALID_ARGUMENT;
		}

		ip_mq_unlock(port);
		break;
	}
	default:
		return KERN_INVALID_ARGUMENT;
	}
	return KERN_SUCCESS;
}
