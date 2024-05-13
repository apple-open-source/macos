/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
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
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 */
/*
 *	File:	vm/vm_user.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	User-exported virtual memory functions.
 */

/*
 * There are three implementations of the "XXX_allocate" functionality in
 * the kernel: mach_vm_allocate (for any task on the platform), vm_allocate
 * (for a task with the same address space size, especially the current task),
 * and vm32_vm_allocate (for the specific case of a 32-bit task). vm_allocate
 * in the kernel should only be used on the kernel_task. vm32_vm_allocate only
 * makes sense on platforms where a user task can either be 32 or 64, or the kernel
 * task can be 32 or 64. mach_vm_allocate makes sense everywhere, and is preferred
 * for new code.
 *
 * The entrypoints into the kernel are more complex. All platforms support a
 * mach_vm_allocate-style API (subsystem 4800) which operates with the largest
 * size types for the platform. On platforms that only support U32/K32,
 * subsystem 4800 is all you need. On platforms that support both U32 and U64,
 * subsystem 3800 is used disambiguate the size of parameters, and they will
 * always be 32-bit and call into the vm32_vm_allocate APIs. On non-U32/K32 platforms,
 * the MIG glue should never call into vm_allocate directly, because the calling
 * task and kernel_task are unlikely to use the same size parameters
 *
 * New VM call implementations should be added here and to mach_vm.defs
 * (subsystem 4800), and use mach_vm_* "wide" types.
 */

#include <debug.h>

#include <vm_cpm.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>    /* to get vm_address_t */
#include <mach/memory_object.h>
#include <mach/std_types.h>     /* to get pointer_t */
#include <mach/upl.h>
#include <mach/vm_attributes.h>
#include <mach/vm_param.h>
#include <mach/vm_statistics.h>
#include <mach/mach_syscalls.h>
#include <mach/sdt.h>

#include <mach/host_priv_server.h>
#include <mach/mach_vm_server.h>
#include <mach/memory_entry_server.h>
#include <mach/vm_map_server.h>

#include <kern/host.h>
#include <kern/kalloc.h>
#include <kern/task.h>
#include <kern/misc_protos.h>
#include <vm/vm_fault.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/memory_object.h>
#include <vm/vm_pageout.h>
#include <vm/vm_protos.h>
#include <vm/vm_purgeable_internal.h>
#if CONFIG_DEFERRED_RECLAIM
#include <vm/vm_reclaim_internal.h>
#endif /* CONFIG_DEFERRED_RECLAIM */
#include <vm/vm_init.h>

#include <san/kasan.h>

#include <libkern/OSDebug.h>
#include <IOKit/IOBSD.h>
#include <sys/kdebug_triage.h>

#if     VM_CPM
#include <vm/cpm.h>
#endif  /* VM_CPM */

static void mach_memory_entry_no_senders(ipc_port_t, mach_port_mscount_t);

__attribute__((always_inline))
int
vm_map_kernel_flags_vmflags(vm_map_kernel_flags_t vmk_flags)
{
	int flags = vmk_flags.__vm_flags & VM_FLAGS_ANY_MASK;

	/* in vmk flags the meaning of fixed/anywhere is inverted */
	return flags ^ (VM_FLAGS_FIXED | VM_FLAGS_ANYWHERE);
}

__attribute__((always_inline, overloadable))
void
vm_map_kernel_flags_set_vmflags(
	vm_map_kernel_flags_t  *vmk_flags,
	int                     vm_flags,
	vm_tag_t                vm_tag)
{
	vm_flags ^= (VM_FLAGS_FIXED | VM_FLAGS_ANYWHERE);
	vmk_flags->__vm_flags &= ~VM_FLAGS_ANY_MASK;
	vmk_flags->__vm_flags |= (vm_flags & VM_FLAGS_ANY_MASK);
	vmk_flags->vm_tag = vm_tag;
}

__attribute__((always_inline, overloadable))
void
vm_map_kernel_flags_set_vmflags(
	vm_map_kernel_flags_t  *vmk_flags,
	int                     vm_flags_and_tag)
{
	vm_flags_and_tag ^= (VM_FLAGS_FIXED | VM_FLAGS_ANYWHERE);
	vmk_flags->__vm_flags &= ~VM_FLAGS_ANY_MASK;
	vmk_flags->__vm_flags |= (vm_flags_and_tag & VM_FLAGS_ANY_MASK);
	VM_GET_FLAGS_ALIAS(vm_flags_and_tag, vmk_flags->vm_tag);
}

__attribute__((always_inline))
void
vm_map_kernel_flags_and_vmflags(
	vm_map_kernel_flags_t  *vmk_flags,
	int                     vm_flags_mask)
{
	/* this function doesn't handle the inverted FIXED/ANYWHERE */
	assert(vm_flags_mask & VM_FLAGS_ANYWHERE);
	vmk_flags->__vm_flags &= vm_flags_mask;
}

bool
vm_map_kernel_flags_check_vmflags(
	vm_map_kernel_flags_t   vmk_flags,
	int                     vm_flags_mask)
{
	int vmflags = vmk_flags.__vm_flags & VM_FLAGS_ANY_MASK;

	/* Note: up to 16 still has good calling conventions */
	static_assert(sizeof(vm_map_kernel_flags_t) == 8);

#if DEBUG || DEVELOPMENT
	/*
	 * All of this compiles to nothing if all checks pass.
	 */
#define check(field, value)  ({ \
	vm_map_kernel_flags_t fl = VM_MAP_KERNEL_FLAGS_NONE; \
	fl.__vm_flags = (value); \
	fl.field = 0; \
	assert(fl.__vm_flags == 0); \
})

	/* bits 0-7 */
	check(vmf_fixed, VM_FLAGS_ANYWHERE); // kind of a lie this is inverted
	check(vmf_purgeable, VM_FLAGS_PURGABLE);
	check(vmf_4gb_chunk, VM_FLAGS_4GB_CHUNK);
	check(vmf_random_addr, VM_FLAGS_RANDOM_ADDR);
	check(vmf_no_cache, VM_FLAGS_NO_CACHE);
	check(vmf_resilient_codesign, VM_FLAGS_RESILIENT_CODESIGN);
	check(vmf_resilient_media, VM_FLAGS_RESILIENT_MEDIA);
	check(vmf_permanent, VM_FLAGS_PERMANENT);

	/* bits 8-15 */
	check(vmf_tpro, VM_FLAGS_TPRO);
	check(vmf_overwrite, VM_FLAGS_OVERWRITE);

	/* bits 16-23 */
	check(vmf_superpage_size, VM_FLAGS_SUPERPAGE_MASK);
	check(vmf_return_data_addr, VM_FLAGS_RETURN_DATA_ADDR);
	check(vmf_return_4k_data_addr, VM_FLAGS_RETURN_4K_DATA_ADDR);

	{
		vm_map_kernel_flags_t fl = VM_MAP_KERNEL_FLAGS_NONE;

		/* check user tags will never clip */
		fl.vm_tag = VM_MEMORY_COUNT - 1;
		assert(fl.vm_tag == VM_MEMORY_COUNT - 1);

		/* check kernel tags will never clip */
		fl.vm_tag = VM_MAX_TAG_VALUE - 1;
		assert(fl.vm_tag == VM_MAX_TAG_VALUE - 1);
	}


#undef check
#endif /* DEBUG || DEVELOPMENT */

	return (vmflags & ~vm_flags_mask) == 0;
}

kern_return_t
vm_purgable_control(
	vm_map_t                map,
	vm_offset_t             address,
	vm_purgable_t           control,
	int                     *state);

kern_return_t
mach_vm_purgable_control(
	vm_map_t                map,
	mach_vm_offset_t        address,
	vm_purgable_t           control,
	int                     *state);

kern_return_t
mach_memory_entry_ownership(
	ipc_port_t      entry_port,
	task_t          owner,
	int             ledger_tag,
	int             ledger_flags);

IPC_KOBJECT_DEFINE(IKOT_NAMED_ENTRY,
    .iko_op_stable     = true,
    .iko_op_no_senders = mach_memory_entry_no_senders);

/*
 *	mach_vm_allocate allocates "zero fill" memory in the specfied
 *	map.
 */
kern_return_t
mach_vm_allocate_external(
	vm_map_t                map,
	mach_vm_offset_t        *addr,
	mach_vm_size_t          size,
	int                     flags)
{
	vm_tag_t tag;

	VM_GET_FLAGS_ALIAS(flags, tag);
	return mach_vm_allocate_kernel(map, addr, size, flags, tag);
}

kern_return_t
mach_vm_allocate_kernel(
	vm_map_t                map,
	mach_vm_offset_t        *addr,
	mach_vm_size_t          size,
	int                     flags,
	vm_tag_t                tag)
{
	vm_map_offset_t map_addr;
	vm_map_size_t   map_size;
	kern_return_t   result;
	vm_map_kernel_flags_t vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;

	/* filter out any kernel-only flags */
	if (flags & ~VM_FLAGS_USER_ALLOCATE) {
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_ALLOCATE_KERNEL_BADFLAGS_ERROR), KERN_INVALID_ARGUMENT /* arg */);
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_kernel_flags_set_vmflags(&vmk_flags, flags, tag);

	if (map == VM_MAP_NULL) {
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_ALLOCATE_KERNEL_BADMAP_ERROR), KERN_INVALID_ARGUMENT /* arg */);
		return KERN_INVALID_ARGUMENT;
	}
	if (size == 0) {
		*addr = 0;
		return KERN_SUCCESS;
	}

	if (vmk_flags.vmf_fixed) {
		map_addr = vm_map_trunc_page(*addr, VM_MAP_PAGE_MASK(map));
	} else {
		map_addr = 0;
	}
	map_size = vm_map_round_page(size,
	    VM_MAP_PAGE_MASK(map));
	if (map_size == 0) {
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_ALLOCATE_KERNEL_BADSIZE_ERROR), KERN_INVALID_ARGUMENT /* arg */);
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_kernel_flags_update_range_id(&vmk_flags, map);

	result = vm_map_enter(
		map,
		&map_addr,
		map_size,
		(vm_map_offset_t)0,
		vmk_flags,
		VM_OBJECT_NULL,
		(vm_object_offset_t)0,
		FALSE,
		VM_PROT_DEFAULT,
		VM_PROT_ALL,
		VM_INHERIT_DEFAULT);

#if KASAN
	if (result == KERN_SUCCESS && map->pmap == kernel_pmap) {
		kasan_notify_address(map_addr, map_size);
	}
#endif
	if (result != KERN_SUCCESS) {
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_ALLOCATE_KERNEL_VMMAPENTER_ERROR), result /* arg */);
	}
	*addr = map_addr;
	return result;
}

/*
 *	vm_allocate
 *	Legacy routine that allocates "zero fill" memory in the specfied
 *	map (which is limited to the same size as the kernel).
 */
kern_return_t
vm_allocate_external(
	vm_map_t        map,
	vm_offset_t     *addr,
	vm_size_t       size,
	int             flags)
{
	vm_map_kernel_flags_t vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
	vm_map_offset_t map_addr;
	vm_map_size_t   map_size;
	kern_return_t   result;

	/* filter out any kernel-only flags */
	if (flags & ~VM_FLAGS_USER_ALLOCATE) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_kernel_flags_set_vmflags(&vmk_flags, flags);

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	if (size == 0) {
		*addr = 0;
		return KERN_SUCCESS;
	}

	if (vmk_flags.vmf_fixed) {
		map_addr = vm_map_trunc_page(*addr, VM_MAP_PAGE_MASK(map));
	} else {
		map_addr = 0;
	}
	map_size = vm_map_round_page(size,
	    VM_MAP_PAGE_MASK(map));
	if (map_size == 0) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_kernel_flags_update_range_id(&vmk_flags, map);

	result = vm_map_enter(
		map,
		&map_addr,
		map_size,
		(vm_map_offset_t)0,
		vmk_flags,
		VM_OBJECT_NULL,
		(vm_object_offset_t)0,
		FALSE,
		VM_PROT_DEFAULT,
		VM_PROT_ALL,
		VM_INHERIT_DEFAULT);

#if KASAN
	if (result == KERN_SUCCESS && map->pmap == kernel_pmap) {
		kasan_notify_address(map_addr, map_size);
	}
#endif

	*addr = CAST_DOWN(vm_offset_t, map_addr);
	return result;
}

/*
 *	mach_vm_deallocate -
 *	deallocates the specified range of addresses in the
 *	specified address map.
 */
kern_return_t
mach_vm_deallocate(
	vm_map_t                map,
	mach_vm_offset_t        start,
	mach_vm_size_t  size)
{
	if ((map == VM_MAP_NULL) || (start + size < start)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == (mach_vm_offset_t) 0) {
		return KERN_SUCCESS;
	}

	return vm_map_remove_guard(map,
	           vm_map_trunc_page(start,
	           VM_MAP_PAGE_MASK(map)),
	           vm_map_round_page(start + size,
	           VM_MAP_PAGE_MASK(map)),
	           VM_MAP_REMOVE_NO_FLAGS,
	           KMEM_GUARD_NONE).kmr_return;
}

/*
 *	vm_deallocate -
 *	deallocates the specified range of addresses in the
 *	specified address map (limited to addresses the same
 *	size as the kernel).
 */
kern_return_t
vm_deallocate(
	vm_map_t                map,
	vm_offset_t             start,
	vm_size_t               size)
{
	if ((map == VM_MAP_NULL) || (start + size < start)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == (vm_offset_t) 0) {
		return KERN_SUCCESS;
	}

	return vm_map_remove_guard(map,
	           vm_map_trunc_page(start,
	           VM_MAP_PAGE_MASK(map)),
	           vm_map_round_page(start + size,
	           VM_MAP_PAGE_MASK(map)),
	           VM_MAP_REMOVE_NO_FLAGS,
	           KMEM_GUARD_NONE).kmr_return;
}

/*
 *	mach_vm_inherit -
 *	Sets the inheritance of the specified range in the
 *	specified map.
 */
kern_return_t
mach_vm_inherit(
	vm_map_t                map,
	mach_vm_offset_t        start,
	mach_vm_size_t  size,
	vm_inherit_t            new_inheritance)
{
	if ((map == VM_MAP_NULL) || (start + size < start) ||
	    (new_inheritance > VM_INHERIT_LAST_VALID)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	return vm_map_inherit(map,
	           vm_map_trunc_page(start,
	           VM_MAP_PAGE_MASK(map)),
	           vm_map_round_page(start + size,
	           VM_MAP_PAGE_MASK(map)),
	           new_inheritance);
}

/*
 *	vm_inherit -
 *	Sets the inheritance of the specified range in the
 *	specified map (range limited to addresses
 */
kern_return_t
vm_inherit(
	vm_map_t                map,
	vm_offset_t             start,
	vm_size_t               size,
	vm_inherit_t            new_inheritance)
{
	if ((map == VM_MAP_NULL) || (start + size < start) ||
	    (new_inheritance > VM_INHERIT_LAST_VALID)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	return vm_map_inherit(map,
	           vm_map_trunc_page(start,
	           VM_MAP_PAGE_MASK(map)),
	           vm_map_round_page(start + size,
	           VM_MAP_PAGE_MASK(map)),
	           new_inheritance);
}

/*
 *	mach_vm_protect -
 *	Sets the protection of the specified range in the
 *	specified map.
 */

kern_return_t
mach_vm_protect(
	vm_map_t                map,
	mach_vm_offset_t        start,
	mach_vm_size_t  size,
	boolean_t               set_maximum,
	vm_prot_t               new_protection)
{
	if ((map == VM_MAP_NULL) || (start + size < start) ||
	    (new_protection & ~(VM_PROT_ALL | VM_PROT_COPY))) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	return vm_map_protect(map,
	           vm_map_trunc_page(start,
	           VM_MAP_PAGE_MASK(map)),
	           vm_map_round_page(start + size,
	           VM_MAP_PAGE_MASK(map)),
	           new_protection,
	           set_maximum);
}

/*
 *	vm_protect -
 *	Sets the protection of the specified range in the
 *	specified map. Addressability of the range limited
 *	to the same size as the kernel.
 */

kern_return_t
vm_protect(
	vm_map_t                map,
	vm_offset_t             start,
	vm_size_t               size,
	boolean_t               set_maximum,
	vm_prot_t               new_protection)
{
	if ((map == VM_MAP_NULL) || (start + size < start) ||
	    (new_protection & ~VM_VALID_VMPROTECT_FLAGS)
#if defined(__x86_64__)
	    || ((new_protection & VM_PROT_UEXEC) && !pmap_supported_feature(map->pmap, PMAP_FEAT_UEXEC))
#endif
	    ) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	return vm_map_protect(map,
	           vm_map_trunc_page(start,
	           VM_MAP_PAGE_MASK(map)),
	           vm_map_round_page(start + size,
	           VM_MAP_PAGE_MASK(map)),
	           new_protection,
	           set_maximum);
}

/*
 * mach_vm_machine_attributes -
 * Handle machine-specific attributes for a mapping, such
 * as cachability, migrability, etc.
 */
kern_return_t
mach_vm_machine_attribute(
	vm_map_t                        map,
	mach_vm_address_t               addr,
	mach_vm_size_t          size,
	vm_machine_attribute_t  attribute,
	vm_machine_attribute_val_t* value)              /* IN/OUT */
{
	if ((map == VM_MAP_NULL) || (addr + size < addr)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	return vm_map_machine_attribute(
		map,
		vm_map_trunc_page(addr,
		VM_MAP_PAGE_MASK(map)),
		vm_map_round_page(addr + size,
		VM_MAP_PAGE_MASK(map)),
		attribute,
		value);
}

/*
 * vm_machine_attribute -
 * Handle machine-specific attributes for a mapping, such
 * as cachability, migrability, etc. Limited addressability
 * (same range limits as for the native kernel map).
 */
kern_return_t
vm_machine_attribute(
	vm_map_t        map,
	vm_address_t    addr,
	vm_size_t       size,
	vm_machine_attribute_t  attribute,
	vm_machine_attribute_val_t* value)              /* IN/OUT */
{
	if ((map == VM_MAP_NULL) || (addr + size < addr)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	return vm_map_machine_attribute(
		map,
		vm_map_trunc_page(addr,
		VM_MAP_PAGE_MASK(map)),
		vm_map_round_page(addr + size,
		VM_MAP_PAGE_MASK(map)),
		attribute,
		value);
}

/*
 * mach_vm_read -
 * Read/copy a range from one address space and return it to the caller.
 *
 * It is assumed that the address for the returned memory is selected by
 * the IPC implementation as part of receiving the reply to this call.
 * If IPC isn't used, the caller must deal with the vm_map_copy_t object
 * that gets returned.
 *
 * JMM - because of mach_msg_type_number_t, this call is limited to a
 * single 4GB region at this time.
 *
 */
kern_return_t
mach_vm_read(
	vm_map_t                map,
	mach_vm_address_t       addr,
	mach_vm_size_t  size,
	pointer_t               *data,
	mach_msg_type_number_t  *data_size)
{
	kern_return_t   error;
	vm_map_copy_t   ipc_address;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((mach_msg_type_number_t) size != size) {
		return KERN_INVALID_ARGUMENT;
	}

	error = vm_map_copyin(map,
	    (vm_map_address_t)addr,
	    (vm_map_size_t)size,
	    FALSE,              /* src_destroy */
	    &ipc_address);

	if (KERN_SUCCESS == error) {
		*data = (pointer_t) ipc_address;
		*data_size = (mach_msg_type_number_t) size;
		assert(*data_size == size);
	}
	return error;
}

/*
 * vm_read -
 * Read/copy a range from one address space and return it to the caller.
 * Limited addressability (same range limits as for the native kernel map).
 *
 * It is assumed that the address for the returned memory is selected by
 * the IPC implementation as part of receiving the reply to this call.
 * If IPC isn't used, the caller must deal with the vm_map_copy_t object
 * that gets returned.
 */
kern_return_t
vm_read(
	vm_map_t                map,
	vm_address_t            addr,
	vm_size_t               size,
	pointer_t               *data,
	mach_msg_type_number_t  *data_size)
{
	kern_return_t   error;
	vm_map_copy_t   ipc_address;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	mach_msg_type_number_t dsize;
	if (os_convert_overflow(size, &dsize)) {
		/*
		 * The kernel could handle a 64-bit "size" value, but
		 * it could not return the size of the data in "*data_size"
		 * without overflowing.
		 * Let's reject this "size" as invalid.
		 */
		return KERN_INVALID_ARGUMENT;
	}

	error = vm_map_copyin(map,
	    (vm_map_address_t)addr,
	    (vm_map_size_t)size,
	    FALSE,              /* src_destroy */
	    &ipc_address);

	if (KERN_SUCCESS == error) {
		*data = (pointer_t) ipc_address;
		*data_size = dsize;
		assert(*data_size == size);
	}
	return error;
}

/*
 * mach_vm_read_list -
 * Read/copy a list of address ranges from specified map.
 *
 * MIG does not know how to deal with a returned array of
 * vm_map_copy_t structures, so we have to do the copyout
 * manually here.
 */
kern_return_t
mach_vm_read_list(
	vm_map_t                        map,
	mach_vm_read_entry_t            data_list,
	natural_t                       count)
{
	mach_msg_type_number_t  i;
	kern_return_t   error;
	vm_map_copy_t   copy;

	if (map == VM_MAP_NULL ||
	    count > VM_MAP_ENTRY_MAX) {
		return KERN_INVALID_ARGUMENT;
	}

	error = KERN_SUCCESS;
	for (i = 0; i < count; i++) {
		vm_map_address_t map_addr;
		vm_map_size_t map_size;

		map_addr = (vm_map_address_t)(data_list[i].address);
		map_size = (vm_map_size_t)(data_list[i].size);

		if (map_size != 0) {
			error = vm_map_copyin(map,
			    map_addr,
			    map_size,
			    FALSE,              /* src_destroy */
			    &copy);
			if (KERN_SUCCESS == error) {
				error = vm_map_copyout(
					current_task()->map,
					&map_addr,
					copy);
				if (KERN_SUCCESS == error) {
					data_list[i].address = map_addr;
					continue;
				}
				vm_map_copy_discard(copy);
			}
		}
		data_list[i].address = (mach_vm_address_t)0;
		data_list[i].size = (mach_vm_size_t)0;
	}
	return error;
}

/*
 * vm_read_list -
 * Read/copy a list of address ranges from specified map.
 *
 * MIG does not know how to deal with a returned array of
 * vm_map_copy_t structures, so we have to do the copyout
 * manually here.
 *
 * The source and destination ranges are limited to those
 * that can be described with a vm_address_t (i.e. same
 * size map as the kernel).
 *
 * JMM - If the result of the copyout is an address range
 * that cannot be described with a vm_address_t (i.e. the
 * caller had a larger address space but used this call
 * anyway), it will result in a truncated address being
 * returned (and a likely confused caller).
 */

kern_return_t
vm_read_list(
	vm_map_t                map,
	vm_read_entry_t data_list,
	natural_t               count)
{
	mach_msg_type_number_t  i;
	kern_return_t   error;
	vm_map_copy_t   copy;

	if (map == VM_MAP_NULL ||
	    count > VM_MAP_ENTRY_MAX) {
		return KERN_INVALID_ARGUMENT;
	}

	error = KERN_SUCCESS;
	for (i = 0; i < count; i++) {
		vm_map_address_t map_addr;
		vm_map_size_t map_size;

		map_addr = (vm_map_address_t)(data_list[i].address);
		map_size = (vm_map_size_t)(data_list[i].size);

		if (map_size != 0) {
			error = vm_map_copyin(map,
			    map_addr,
			    map_size,
			    FALSE,              /* src_destroy */
			    &copy);
			if (KERN_SUCCESS == error) {
				error = vm_map_copyout(current_task()->map,
				    &map_addr,
				    copy);
				if (KERN_SUCCESS == error) {
					data_list[i].address =
					    CAST_DOWN(vm_offset_t, map_addr);
					continue;
				}
				vm_map_copy_discard(copy);
			}
		}
		data_list[i].address = (mach_vm_address_t)0;
		data_list[i].size = (mach_vm_size_t)0;
	}
	return error;
}

/*
 * mach_vm_read_overwrite -
 * Overwrite a range of the current map with data from the specified
 * map/address range.
 *
 * In making an assumption that the current thread is local, it is
 * no longer cluster-safe without a fully supportive local proxy
 * thread/task (but we don't support cluster's anymore so this is moot).
 */

kern_return_t
mach_vm_read_overwrite(
	vm_map_t                map,
	mach_vm_address_t       address,
	mach_vm_size_t  size,
	mach_vm_address_t       data,
	mach_vm_size_t  *data_size)
{
	kern_return_t   error;
	vm_map_copy_t   copy;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	error = vm_map_copyin(map, (vm_map_address_t)address,
	    (vm_map_size_t)size, FALSE, &copy);

	if (KERN_SUCCESS == error) {
		if (copy) {
			assertf(copy->size == (vm_map_size_t) size, "Req size: 0x%llx, Copy size: 0x%llx\n", (uint64_t) size, (uint64_t) copy->size);
		}

		error = vm_map_copy_overwrite(current_thread()->map,
		    (vm_map_address_t)data,
		    copy, (vm_map_size_t) size, FALSE);
		if (KERN_SUCCESS == error) {
			*data_size = size;
			return error;
		}
		vm_map_copy_discard(copy);
	}
	return error;
}

/*
 * vm_read_overwrite -
 * Overwrite a range of the current map with data from the specified
 * map/address range.
 *
 * This routine adds the additional limitation that the source and
 * destination ranges must be describable with vm_address_t values
 * (i.e. the same size address spaces as the kernel, or at least the
 * the ranges are in that first portion of the respective address
 * spaces).
 */

kern_return_t
vm_read_overwrite(
	vm_map_t        map,
	vm_address_t    address,
	vm_size_t       size,
	vm_address_t    data,
	vm_size_t       *data_size)
{
	kern_return_t   error;
	vm_map_copy_t   copy;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	error = vm_map_copyin(map, (vm_map_address_t)address,
	    (vm_map_size_t)size, FALSE, &copy);

	if (KERN_SUCCESS == error) {
		if (copy) {
			assertf(copy->size == (vm_map_size_t) size, "Req size: 0x%llx, Copy size: 0x%llx\n", (uint64_t) size, (uint64_t) copy->size);
		}

		error = vm_map_copy_overwrite(current_thread()->map,
		    (vm_map_address_t)data,
		    copy, (vm_map_size_t) size, FALSE);
		if (KERN_SUCCESS == error) {
			*data_size = size;
			return error;
		}
		vm_map_copy_discard(copy);
	}
	return error;
}


/*
 * mach_vm_write -
 * Overwrite the specified address range with the data provided
 * (from the current map).
 */
kern_return_t
mach_vm_write(
	vm_map_t                        map,
	mach_vm_address_t               address,
	pointer_t                       data,
	mach_msg_type_number_t          size)
{
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	return vm_map_copy_overwrite(map, (vm_map_address_t)address,
	           (vm_map_copy_t) data, size, FALSE /* interruptible XXX */);
}

/*
 * vm_write -
 * Overwrite the specified address range with the data provided
 * (from the current map).
 *
 * The addressability of the range of addresses to overwrite is
 * limited bu the use of a vm_address_t (same size as kernel map).
 * Either the target map is also small, or the range is in the
 * low addresses within it.
 */
kern_return_t
vm_write(
	vm_map_t                        map,
	vm_address_t                    address,
	pointer_t                       data,
	mach_msg_type_number_t          size)
{
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	return vm_map_copy_overwrite(map, (vm_map_address_t)address,
	           (vm_map_copy_t) data, size, FALSE /* interruptible XXX */);
}

/*
 * mach_vm_copy -
 * Overwrite one range of the specified map with the contents of
 * another range within that same map (i.e. both address ranges
 * are "over there").
 */
kern_return_t
mach_vm_copy(
	vm_map_t                map,
	mach_vm_address_t       source_address,
	mach_vm_size_t  size,
	mach_vm_address_t       dest_address)
{
	vm_map_copy_t copy;
	kern_return_t kr;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = vm_map_copyin(map, (vm_map_address_t)source_address,
	    (vm_map_size_t)size, FALSE, &copy);

	if (KERN_SUCCESS == kr) {
		if (copy) {
			assertf(copy->size == (vm_map_size_t) size, "Req size: 0x%llx, Copy size: 0x%llx\n", (uint64_t) size, (uint64_t) copy->size);
		}

		kr = vm_map_copy_overwrite(map,
		    (vm_map_address_t)dest_address,
		    copy, (vm_map_size_t) size, FALSE /* interruptible XXX */);

		if (KERN_SUCCESS != kr) {
			vm_map_copy_discard(copy);
		}
	}
	return kr;
}

kern_return_t
vm_copy(
	vm_map_t        map,
	vm_address_t    source_address,
	vm_size_t       size,
	vm_address_t    dest_address)
{
	vm_map_copy_t copy;
	kern_return_t kr;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = vm_map_copyin(map, (vm_map_address_t)source_address,
	    (vm_map_size_t)size, FALSE, &copy);

	if (KERN_SUCCESS == kr) {
		if (copy) {
			assertf(copy->size == (vm_map_size_t) size, "Req size: 0x%llx, Copy size: 0x%llx\n", (uint64_t) size, (uint64_t) copy->size);
		}

		kr = vm_map_copy_overwrite(map,
		    (vm_map_address_t)dest_address,
		    copy, (vm_map_size_t) size, FALSE /* interruptible XXX */);

		if (KERN_SUCCESS != kr) {
			vm_map_copy_discard(copy);
		}
	}
	return kr;
}

/*
 * mach_vm_map -
 * Map some range of an object into an address space.
 *
 * The object can be one of several types of objects:
 *	NULL - anonymous memory
 *	a named entry - a range within another address space
 *	                or a range within a memory object
 *	a whole memory object
 *
 */
kern_return_t
mach_vm_map_external(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t          initial_size,
	mach_vm_offset_t        mask,
	int                     flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	vm_map_kernel_flags_t vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;

	/* filter out any kernel-only flags */
	if (flags & ~VM_FLAGS_USER_MAP) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_kernel_flags_set_vmflags(&vmk_flags, flags);
	/* range_id is set by mach_vm_map_kernel */
	return mach_vm_map_kernel(target_map, address, initial_size, mask,
	           vmk_flags, port, offset, copy,
	           cur_protection, max_protection,
	           inheritance);
}

kern_return_t
mach_vm_map_kernel(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t          initial_size,
	mach_vm_offset_t        mask,
	vm_map_kernel_flags_t   vmk_flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	kern_return_t           kr;
	vm_map_offset_t         vmmaddr;

	vmmaddr = (vm_map_offset_t) *address;

	/* filter out any kernel-only flags */
	if (!vm_map_kernel_flags_check_vmflags(vmk_flags, VM_FLAGS_USER_MAP)) {
		return KERN_INVALID_ARGUMENT;
	}

	/* range_id is set by vm_map_enter_mem_object */
	kr = vm_map_enter_mem_object(target_map,
	    &vmmaddr,
	    initial_size,
	    mask,
	    vmk_flags,
	    port,
	    offset,
	    copy,
	    cur_protection,
	    max_protection,
	    inheritance);

#if KASAN
	if (kr == KERN_SUCCESS && target_map->pmap == kernel_pmap) {
		kasan_notify_address(vmmaddr, initial_size);
	}
#endif

	*address = vmmaddr;
	return kr;
}


/* legacy interface */
__attribute__((always_inline))
kern_return_t
vm_map_64_external(
	vm_map_t                target_map,
	vm_offset_t             *address,
	vm_size_t               size,
	vm_offset_t             mask,
	int                     flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	static_assert(sizeof(vm_offset_t) == sizeof(mach_vm_offset_t));

	return mach_vm_map_external(target_map, (mach_vm_offset_t *)address,
	           size, mask, flags, port, offset, copy,
	           cur_protection, max_protection, inheritance);
}

/* temporary, until world build */
__attribute__((always_inline))
kern_return_t
vm_map_external(
	vm_map_t                target_map,
	vm_offset_t             *address,
	vm_size_t               size,
	vm_offset_t             mask,
	int                     flags,
	ipc_port_t              port,
	vm_offset_t             offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	static_assert(sizeof(vm_offset_t) == sizeof(mach_vm_offset_t));

	return mach_vm_map_external(target_map, (mach_vm_offset_t *)address,
	           size, mask, flags, port, offset, copy,
	           cur_protection, max_protection, inheritance);
}

/*
 * mach_vm_remap_new -
 * Behaves like mach_vm_remap, except that VM_FLAGS_RETURN_DATA_ADDR is always set
 * and {cur,max}_protection are in/out.
 */
kern_return_t
mach_vm_remap_new_external(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t          size,
	mach_vm_offset_t        mask,
	int                     flags,
	mach_port_t             src_tport,
	mach_vm_offset_t        memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,   /* IN/OUT */
	vm_prot_t               *max_protection,   /* IN/OUT */
	vm_inherit_t            inheritance)
{
	vm_map_kernel_flags_t   vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
	vm_map_t                src_map;
	kern_return_t           kr;

	/* filter out any kernel-only flags */
	if (flags & ~VM_FLAGS_USER_REMAP) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_kernel_flags_set_vmflags(&vmk_flags,
	    flags | VM_FLAGS_RETURN_DATA_ADDR);

	if (target_map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((*cur_protection & ~VM_PROT_ALL) ||
	    (*max_protection & ~VM_PROT_ALL) ||
	    (*cur_protection & *max_protection) != *cur_protection) {
		return KERN_INVALID_ARGUMENT;
	}
	if ((*max_protection & (VM_PROT_WRITE | VM_PROT_EXECUTE)) ==
	    (VM_PROT_WRITE | VM_PROT_EXECUTE)) {
		/*
		 * XXX FBDP TODO
		 * enforce target's "wx" policies
		 */
		return KERN_PROTECTION_FAILURE;
	}

	if (copy || *max_protection == VM_PROT_READ || *max_protection == VM_PROT_NONE) {
		src_map = convert_port_to_map_read(src_tport);
	} else {
		src_map = convert_port_to_map(src_tport);
	}

	if (src_map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	static_assert(sizeof(mach_vm_offset_t) == sizeof(vm_map_address_t));

	/* range_id is set by vm_map_remap */
	kr = vm_map_remap(target_map,
	    address,
	    size,
	    mask,
	    vmk_flags,
	    src_map,
	    memory_address,
	    copy,
	    cur_protection,    /* IN/OUT */
	    max_protection,    /* IN/OUT */
	    inheritance);

	vm_map_deallocate(src_map);

	if (kr == KERN_SUCCESS) {
		ipc_port_release_send(src_tport);  /* consume on success */
	}
	return kr;
}

/*
 * mach_vm_remap -
 * Remap a range of memory from one task into another,
 * to another address range within the same task, or
 * over top of itself (with altered permissions and/or
 * as an in-place copy of itself).
 */
kern_return_t
mach_vm_remap_external(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t  size,
	mach_vm_offset_t        mask,
	int                     flags,
	vm_map_t                src_map,
	mach_vm_offset_t        memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,    /* OUT */
	vm_prot_t               *max_protection,    /* OUT */
	vm_inherit_t            inheritance)
{
	vm_tag_t tag;
	VM_GET_FLAGS_ALIAS(flags, tag);

	return mach_vm_remap_kernel(target_map, address, size, mask, flags, tag, src_map, memory_address,
	           copy, cur_protection, max_protection, inheritance);
}

static kern_return_t
mach_vm_remap_kernel_helper(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t          size,
	mach_vm_offset_t        mask,
	int                     flags,
	vm_tag_t                tag,
	vm_map_t                src_map,
	mach_vm_offset_t        memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,   /* IN/OUT */
	vm_prot_t               *max_protection,   /* IN/OUT */
	vm_inherit_t            inheritance)
{
	vm_map_kernel_flags_t   vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
	kern_return_t           kr;

	if (VM_MAP_NULL == target_map || VM_MAP_NULL == src_map) {
		return KERN_INVALID_ARGUMENT;
	}

	/* filter out any kernel-only flags */
	if (flags & ~VM_FLAGS_USER_REMAP) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_kernel_flags_set_vmflags(&vmk_flags, flags, tag);

	static_assert(sizeof(mach_vm_offset_t) == sizeof(vm_map_address_t));

	/* range_id is set by vm_map_remap */
	kr = vm_map_remap(target_map,
	    address,
	    size,
	    mask,
	    vmk_flags,
	    src_map,
	    memory_address,
	    copy,
	    cur_protection,    /* IN/OUT */
	    max_protection,    /* IN/OUT */
	    inheritance);

#if KASAN
	if (kr == KERN_SUCCESS && target_map->pmap == kernel_pmap) {
		kasan_notify_address(*address, size);
	}
#endif
	return kr;
}

kern_return_t
mach_vm_remap_kernel(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t  size,
	mach_vm_offset_t        mask,
	int                     flags,
	vm_tag_t                tag,
	vm_map_t                src_map,
	mach_vm_offset_t        memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,   /* OUT */
	vm_prot_t               *max_protection,   /* OUT */
	vm_inherit_t            inheritance)
{
	*cur_protection = VM_PROT_NONE;
	*max_protection = VM_PROT_NONE;

	return mach_vm_remap_kernel_helper(target_map,
	           address,
	           size,
	           mask,
	           flags,
	           tag,
	           src_map,
	           memory_address,
	           copy,
	           cur_protection,
	           max_protection,
	           inheritance);
}

kern_return_t
mach_vm_remap_new_kernel(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t  size,
	mach_vm_offset_t        mask,
	int                     flags,
	vm_tag_t                tag,
	vm_map_t                src_map,
	mach_vm_offset_t        memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,   /* IN/OUT */
	vm_prot_t               *max_protection,   /* IN/OUT */
	vm_inherit_t            inheritance)
{
	if ((*cur_protection & ~VM_PROT_ALL) ||
	    (*max_protection & ~VM_PROT_ALL) ||
	    (*cur_protection & *max_protection) != *cur_protection) {
		return KERN_INVALID_ARGUMENT;
	}

	flags |= VM_FLAGS_RETURN_DATA_ADDR;

	return mach_vm_remap_kernel_helper(target_map,
	           address,
	           size,
	           mask,
	           flags,
	           tag,
	           src_map,
	           memory_address,
	           copy,
	           cur_protection,
	           max_protection,
	           inheritance);
}

/*
 * vm_remap_new -
 * Behaves like vm_remap, except that VM_FLAGS_RETURN_DATA_ADDR is always set
 * and {cur,max}_protection are in/out.
 */
kern_return_t
vm_remap_new_external(
	vm_map_t                target_map,
	vm_offset_t             *address,
	vm_size_t               size,
	vm_offset_t             mask,
	int                     flags,
	mach_port_t             src_tport,
	vm_offset_t             memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,       /* IN/OUT */
	vm_prot_t               *max_protection,       /* IN/OUT */
	vm_inherit_t            inheritance)
{
	static_assert(sizeof(vm_map_offset_t) == sizeof(vm_offset_t));

	return mach_vm_remap_new_external(target_map,
	           (vm_map_offset_t *)address,
	           size,
	           mask,
	           flags,
	           src_tport,
	           memory_address,
	           copy,
	           cur_protection, /* IN/OUT */
	           max_protection, /* IN/OUT */
	           inheritance);
}

/*
 * vm_remap -
 * Remap a range of memory from one task into another,
 * to another address range within the same task, or
 * over top of itself (with altered permissions and/or
 * as an in-place copy of itself).
 *
 * The addressability of the source and target address
 * range is limited by the size of vm_address_t (in the
 * kernel context).
 */
kern_return_t
vm_remap_external(
	vm_map_t                target_map,
	vm_offset_t             *address,
	vm_size_t               size,
	vm_offset_t             mask,
	int                     flags,
	vm_map_t                src_map,
	vm_offset_t             memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,    /* OUT */
	vm_prot_t               *max_protection,    /* OUT */
	vm_inherit_t            inheritance)
{
	static_assert(sizeof(vm_offset_t) == sizeof(mach_vm_offset_t));

	return mach_vm_remap_external(target_map, (mach_vm_offset_t *)address,
	           size, mask, flags, src_map, memory_address, copy,
	           cur_protection, max_protection, inheritance);
}

/*
 * NOTE: these routine (and this file) will no longer require mach_host_server.h
 * when mach_vm_wire and vm_wire are changed to use ledgers.
 */
#include <mach/mach_host_server.h>
/*
 *	mach_vm_wire
 *	Specify that the range of the virtual address space
 *	of the target task must not cause page faults for
 *	the indicated accesses.
 *
 *	[ To unwire the pages, specify VM_PROT_NONE. ]
 */
kern_return_t
mach_vm_wire_external(
	host_priv_t             host_priv,
	vm_map_t                map,
	mach_vm_offset_t        start,
	mach_vm_size_t  size,
	vm_prot_t               access)
{
	if (host_priv == HOST_PRIV_NULL) {
		return KERN_INVALID_HOST;
	}

	return mach_vm_wire_kernel(map, start, size, access, VM_KERN_MEMORY_MLOCK);
}

kern_return_t
mach_vm_wire_kernel(
	vm_map_t                map,
	mach_vm_offset_t        start,
	mach_vm_size_t  size,
	vm_prot_t               access,
	vm_tag_t                tag)
{
	kern_return_t           rc;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_TASK;
	}

	if (access & ~VM_PROT_ALL || (start + size < start)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (access != VM_PROT_NONE) {
		rc = vm_map_wire_kernel(map,
		    vm_map_trunc_page(start,
		    VM_MAP_PAGE_MASK(map)),
		    vm_map_round_page(start + size,
		    VM_MAP_PAGE_MASK(map)),
		    access, tag,
		    TRUE);
	} else {
		rc = vm_map_unwire(map,
		    vm_map_trunc_page(start,
		    VM_MAP_PAGE_MASK(map)),
		    vm_map_round_page(start + size,
		    VM_MAP_PAGE_MASK(map)),
		    TRUE);
	}
	return rc;
}

/*
 *	vm_wire -
 *	Specify that the range of the virtual address space
 *	of the target task must not cause page faults for
 *	the indicated accesses.
 *
 *	[ To unwire the pages, specify VM_PROT_NONE. ]
 */
kern_return_t
vm_wire(
	host_priv_t             host_priv,
	vm_map_t                map,
	vm_offset_t             start,
	vm_size_t               size,
	vm_prot_t               access)
{
	kern_return_t           rc;

	if (host_priv == HOST_PRIV_NULL) {
		return KERN_INVALID_HOST;
	}

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_TASK;
	}

	if ((access & ~VM_PROT_ALL) || (start + size < start)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		rc = KERN_SUCCESS;
	} else if (access != VM_PROT_NONE) {
		rc = vm_map_wire_kernel(map,
		    vm_map_trunc_page(start,
		    VM_MAP_PAGE_MASK(map)),
		    vm_map_round_page(start + size,
		    VM_MAP_PAGE_MASK(map)),
		    access, VM_KERN_MEMORY_OSFMK,
		    TRUE);
	} else {
		rc = vm_map_unwire(map,
		    vm_map_trunc_page(start,
		    VM_MAP_PAGE_MASK(map)),
		    vm_map_round_page(start + size,
		    VM_MAP_PAGE_MASK(map)),
		    TRUE);
	}
	return rc;
}

/*
 *	vm_msync
 *
 *	Synchronises the memory range specified with its backing store
 *	image by either flushing or cleaning the contents to the appropriate
 *	memory manager.
 *
 *	interpretation of sync_flags
 *	VM_SYNC_INVALIDATE	- discard pages, only return precious
 *				  pages to manager.
 *
 *	VM_SYNC_INVALIDATE & (VM_SYNC_SYNCHRONOUS | VM_SYNC_ASYNCHRONOUS)
 *				- discard pages, write dirty or precious
 *				  pages back to memory manager.
 *
 *	VM_SYNC_SYNCHRONOUS | VM_SYNC_ASYNCHRONOUS
 *				- write dirty or precious pages back to
 *				  the memory manager.
 *
 *	VM_SYNC_CONTIGUOUS	- does everything normally, but if there
 *				  is a hole in the region, and we would
 *				  have returned KERN_SUCCESS, return
 *				  KERN_INVALID_ADDRESS instead.
 *
 *	RETURNS
 *	KERN_INVALID_TASK		Bad task parameter
 *	KERN_INVALID_ARGUMENT		both sync and async were specified.
 *	KERN_SUCCESS			The usual.
 *	KERN_INVALID_ADDRESS		There was a hole in the region.
 */

kern_return_t
mach_vm_msync(
	vm_map_t                map,
	mach_vm_address_t       address,
	mach_vm_size_t  size,
	vm_sync_t               sync_flags)
{
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_TASK;
	}

	return vm_map_msync(map, (vm_map_address_t)address,
	           (vm_map_size_t)size, sync_flags);
}

/*
 *	vm_msync
 *
 *	Synchronises the memory range specified with its backing store
 *	image by either flushing or cleaning the contents to the appropriate
 *	memory manager.
 *
 *	interpretation of sync_flags
 *	VM_SYNC_INVALIDATE	- discard pages, only return precious
 *				  pages to manager.
 *
 *	VM_SYNC_INVALIDATE & (VM_SYNC_SYNCHRONOUS | VM_SYNC_ASYNCHRONOUS)
 *				- discard pages, write dirty or precious
 *				  pages back to memory manager.
 *
 *	VM_SYNC_SYNCHRONOUS | VM_SYNC_ASYNCHRONOUS
 *				- write dirty or precious pages back to
 *				  the memory manager.
 *
 *	VM_SYNC_CONTIGUOUS	- does everything normally, but if there
 *				  is a hole in the region, and we would
 *				  have returned KERN_SUCCESS, return
 *				  KERN_INVALID_ADDRESS instead.
 *
 *	The addressability of the range is limited to that which can
 *	be described by a vm_address_t.
 *
 *	RETURNS
 *	KERN_INVALID_TASK		Bad task parameter
 *	KERN_INVALID_ARGUMENT		both sync and async were specified.
 *	KERN_SUCCESS			The usual.
 *	KERN_INVALID_ADDRESS		There was a hole in the region.
 */

kern_return_t
vm_msync(
	vm_map_t        map,
	vm_address_t    address,
	vm_size_t       size,
	vm_sync_t       sync_flags)
{
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_TASK;
	}

	return vm_map_msync(map, (vm_map_address_t)address,
	           (vm_map_size_t)size, sync_flags);
}


int
vm_toggle_entry_reuse(int toggle, int *old_value)
{
	vm_map_t map = current_map();

	assert(!map->is_nested_map);
	if (toggle == VM_TOGGLE_GETVALUE && old_value != NULL) {
		*old_value = map->disable_vmentry_reuse;
	} else if (toggle == VM_TOGGLE_SET) {
		vm_map_entry_t map_to_entry;

		vm_map_lock(map);
		vm_map_disable_hole_optimization(map);
		map->disable_vmentry_reuse = TRUE;
		__IGNORE_WCASTALIGN(map_to_entry = vm_map_to_entry(map));
		if (map->first_free == map_to_entry) {
			map->highest_entry_end = vm_map_min(map);
		} else {
			map->highest_entry_end = map->first_free->vme_end;
		}
		vm_map_unlock(map);
	} else if (toggle == VM_TOGGLE_CLEAR) {
		vm_map_lock(map);
		map->disable_vmentry_reuse = FALSE;
		vm_map_unlock(map);
	} else {
		return KERN_INVALID_ARGUMENT;
	}

	return KERN_SUCCESS;
}

/*
 *	mach_vm_behavior_set
 *
 *	Sets the paging behavior attribute for the  specified range
 *	in the specified map.
 *
 *	This routine will fail with KERN_INVALID_ADDRESS if any address
 *	in [start,start+size) is not a valid allocated memory region.
 */
kern_return_t
mach_vm_behavior_set(
	vm_map_t                map,
	mach_vm_offset_t        start,
	mach_vm_size_t          size,
	vm_behavior_t           new_behavior)
{
	vm_map_offset_t align_mask;

	if ((map == VM_MAP_NULL) || (start + size < start)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	switch (new_behavior) {
	case VM_BEHAVIOR_REUSABLE:
	case VM_BEHAVIOR_REUSE:
	case VM_BEHAVIOR_CAN_REUSE:
	case VM_BEHAVIOR_ZERO:
		/*
		 * Align to the hardware page size, to allow
		 * malloc() to maximize the amount of re-usability,
		 * even on systems with larger software page size.
		 */
		align_mask = PAGE_MASK;
		break;
	default:
		align_mask = VM_MAP_PAGE_MASK(map);
		break;
	}

	return vm_map_behavior_set(map,
	           vm_map_trunc_page(start, align_mask),
	           vm_map_round_page(start + size, align_mask),
	           new_behavior);
}

/*
 *	vm_behavior_set
 *
 *	Sets the paging behavior attribute for the  specified range
 *	in the specified map.
 *
 *	This routine will fail with KERN_INVALID_ADDRESS if any address
 *	in [start,start+size) is not a valid allocated memory region.
 *
 *	This routine is potentially limited in addressibility by the
 *	use of vm_offset_t (if the map provided is larger than the
 *	kernel's).
 */
kern_return_t
vm_behavior_set(
	vm_map_t                map,
	vm_offset_t             start,
	vm_size_t               size,
	vm_behavior_t           new_behavior)
{
	if (start + size < start) {
		return KERN_INVALID_ARGUMENT;
	}

	return mach_vm_behavior_set(map,
	           (mach_vm_offset_t) start,
	           (mach_vm_size_t) size,
	           new_behavior);
}

/*
 *	mach_vm_region:
 *
 *	User call to obtain information about a region in
 *	a task's address map. Currently, only one flavor is
 *	supported.
 *
 *	XXX The reserved and behavior fields cannot be filled
 *	    in until the vm merge from the IK is completed, and
 *	    vm_reserve is implemented.
 *
 *	XXX Dependency: syscall_vm_region() also supports only one flavor.
 */

kern_return_t
mach_vm_region(
	vm_map_t                 map,
	mach_vm_offset_t        *address,               /* IN/OUT */
	mach_vm_size_t          *size,                  /* OUT */
	vm_region_flavor_t       flavor,                /* IN */
	vm_region_info_t         info,                  /* OUT */
	mach_msg_type_number_t  *count,                 /* IN/OUT */
	mach_port_t             *object_name)           /* OUT */
{
	vm_map_offset_t         map_addr;
	vm_map_size_t           map_size;
	kern_return_t           kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	map_addr = (vm_map_offset_t)*address;
	map_size = (vm_map_size_t)*size;

	/* legacy conversion */
	if (VM_REGION_BASIC_INFO == flavor) {
		flavor = VM_REGION_BASIC_INFO_64;
	}

	kr = vm_map_region(map,
	    &map_addr, &map_size,
	    flavor, info, count,
	    object_name);

	*address = map_addr;
	*size = map_size;
	return kr;
}

/*
 *	vm_region_64 and vm_region:
 *
 *	User call to obtain information about a region in
 *	a task's address map. Currently, only one flavor is
 *	supported.
 *
 *	XXX The reserved and behavior fields cannot be filled
 *	    in until the vm merge from the IK is completed, and
 *	    vm_reserve is implemented.
 *
 *	XXX Dependency: syscall_vm_region() also supports only one flavor.
 */

kern_return_t
vm_region_64(
	vm_map_t                 map,
	vm_offset_t             *address,               /* IN/OUT */
	vm_size_t               *size,                  /* OUT */
	vm_region_flavor_t       flavor,                /* IN */
	vm_region_info_t         info,                  /* OUT */
	mach_msg_type_number_t  *count,                 /* IN/OUT */
	mach_port_t             *object_name)           /* OUT */
{
	vm_map_offset_t         map_addr;
	vm_map_size_t           map_size;
	kern_return_t           kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	map_addr = (vm_map_offset_t)*address;
	map_size = (vm_map_size_t)*size;

	/* legacy conversion */
	if (VM_REGION_BASIC_INFO == flavor) {
		flavor = VM_REGION_BASIC_INFO_64;
	}

	kr = vm_map_region(map,
	    &map_addr, &map_size,
	    flavor, info, count,
	    object_name);

	*address = CAST_DOWN(vm_offset_t, map_addr);
	*size = CAST_DOWN(vm_size_t, map_size);

	if (KERN_SUCCESS == kr && map_addr + map_size > VM_MAX_ADDRESS) {
		return KERN_INVALID_ADDRESS;
	}
	return kr;
}

kern_return_t
vm_region(
	vm_map_t                        map,
	vm_address_t                    *address,       /* IN/OUT */
	vm_size_t                       *size,          /* OUT */
	vm_region_flavor_t              flavor, /* IN */
	vm_region_info_t                info,           /* OUT */
	mach_msg_type_number_t  *count, /* IN/OUT */
	mach_port_t                     *object_name)   /* OUT */
{
	vm_map_address_t        map_addr;
	vm_map_size_t           map_size;
	kern_return_t           kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	map_addr = (vm_map_address_t)*address;
	map_size = (vm_map_size_t)*size;

	kr = vm_map_region(map,
	    &map_addr, &map_size,
	    flavor, info, count,
	    object_name);

	*address = CAST_DOWN(vm_address_t, map_addr);
	*size = CAST_DOWN(vm_size_t, map_size);

	if (KERN_SUCCESS == kr && map_addr + map_size > VM_MAX_ADDRESS) {
		return KERN_INVALID_ADDRESS;
	}
	return kr;
}

/*
 *	vm_region_recurse: A form of vm_region which follows the
 *	submaps in a target map
 *
 */
kern_return_t
mach_vm_region_recurse(
	vm_map_t                        map,
	mach_vm_address_t               *address,
	mach_vm_size_t          *size,
	uint32_t                        *depth,
	vm_region_recurse_info_t        info,
	mach_msg_type_number_t  *infoCnt)
{
	vm_map_address_t        map_addr;
	vm_map_size_t           map_size;
	kern_return_t           kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	map_addr = (vm_map_address_t)*address;
	map_size = (vm_map_size_t)*size;

	kr = vm_map_region_recurse_64(
		map,
		&map_addr,
		&map_size,
		depth,
		(vm_region_submap_info_64_t)info,
		infoCnt);

	*address = map_addr;
	*size = map_size;
	return kr;
}

/*
 *	vm_region_recurse: A form of vm_region which follows the
 *	submaps in a target map
 *
 */
kern_return_t
vm_region_recurse_64(
	vm_map_t                        map,
	vm_address_t                    *address,
	vm_size_t                       *size,
	uint32_t                        *depth,
	vm_region_recurse_info_64_t     info,
	mach_msg_type_number_t  *infoCnt)
{
	vm_map_address_t        map_addr;
	vm_map_size_t           map_size;
	kern_return_t           kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	map_addr = (vm_map_address_t)*address;
	map_size = (vm_map_size_t)*size;

	kr = vm_map_region_recurse_64(
		map,
		&map_addr,
		&map_size,
		depth,
		(vm_region_submap_info_64_t)info,
		infoCnt);

	*address = CAST_DOWN(vm_address_t, map_addr);
	*size = CAST_DOWN(vm_size_t, map_size);

	if (KERN_SUCCESS == kr && map_addr + map_size > VM_MAX_ADDRESS) {
		return KERN_INVALID_ADDRESS;
	}
	return kr;
}

kern_return_t
vm_region_recurse(
	vm_map_t                        map,
	vm_offset_t             *address,       /* IN/OUT */
	vm_size_t                       *size,          /* OUT */
	natural_t                       *depth, /* IN/OUT */
	vm_region_recurse_info_t        info32, /* IN/OUT */
	mach_msg_type_number_t  *infoCnt)       /* IN/OUT */
{
	vm_region_submap_info_data_64_t info64;
	vm_region_submap_info_t info;
	vm_map_address_t        map_addr;
	vm_map_size_t           map_size;
	kern_return_t           kr;

	if (VM_MAP_NULL == map || *infoCnt < VM_REGION_SUBMAP_INFO_COUNT) {
		return KERN_INVALID_ARGUMENT;
	}


	map_addr = (vm_map_address_t)*address;
	map_size = (vm_map_size_t)*size;
	info = (vm_region_submap_info_t)info32;
	*infoCnt = VM_REGION_SUBMAP_INFO_COUNT_64;

	kr = vm_map_region_recurse_64(map, &map_addr, &map_size,
	    depth, &info64, infoCnt);

	info->protection = info64.protection;
	info->max_protection = info64.max_protection;
	info->inheritance = info64.inheritance;
	info->offset = (uint32_t)info64.offset; /* trouble-maker */
	info->user_tag = info64.user_tag;
	info->pages_resident = info64.pages_resident;
	info->pages_shared_now_private = info64.pages_shared_now_private;
	info->pages_swapped_out = info64.pages_swapped_out;
	info->pages_dirtied = info64.pages_dirtied;
	info->ref_count = info64.ref_count;
	info->shadow_depth = info64.shadow_depth;
	info->external_pager = info64.external_pager;
	info->share_mode = info64.share_mode;
	info->is_submap = info64.is_submap;
	info->behavior = info64.behavior;
	info->object_id = info64.object_id;
	info->user_wired_count = info64.user_wired_count;

	*address = CAST_DOWN(vm_address_t, map_addr);
	*size = CAST_DOWN(vm_size_t, map_size);
	*infoCnt = VM_REGION_SUBMAP_INFO_COUNT;

	if (KERN_SUCCESS == kr && map_addr + map_size > VM_MAX_ADDRESS) {
		return KERN_INVALID_ADDRESS;
	}
	return kr;
}

kern_return_t
mach_vm_purgable_control(
	vm_map_t                map,
	mach_vm_offset_t        address,
	vm_purgable_t           control,
	int                     *state)
{
	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	if (control == VM_PURGABLE_SET_STATE_FROM_KERNEL) {
		/* not allowed from user-space */
		return KERN_INVALID_ARGUMENT;
	}

	return vm_map_purgable_control(map,
	           vm_map_trunc_page(address, VM_MAP_PAGE_MASK(map)),
	           control,
	           state);
}

kern_return_t
mach_vm_purgable_control_external(
	mach_port_t             target_tport,
	mach_vm_offset_t        address,
	vm_purgable_t           control,
	int                     *state)
{
	vm_map_t map;
	kern_return_t kr;

	if (control == VM_PURGABLE_GET_STATE) {
		map = convert_port_to_map_read(target_tport);
	} else {
		map = convert_port_to_map(target_tport);
	}

	kr = mach_vm_purgable_control(map, address, control, state);
	vm_map_deallocate(map);

	return kr;
}

kern_return_t
vm_purgable_control(
	vm_map_t                map,
	vm_offset_t             address,
	vm_purgable_t           control,
	int                     *state)
{
	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	if (control == VM_PURGABLE_SET_STATE_FROM_KERNEL) {
		/* not allowed from user-space */
		return KERN_INVALID_ARGUMENT;
	}

	return vm_map_purgable_control(map,
	           vm_map_trunc_page(address, VM_MAP_PAGE_MASK(map)),
	           control,
	           state);
}

kern_return_t
vm_purgable_control_external(
	mach_port_t             target_tport,
	vm_offset_t             address,
	vm_purgable_t           control,
	int                     *state)
{
	vm_map_t map;
	kern_return_t kr;

	if (control == VM_PURGABLE_GET_STATE) {
		map = convert_port_to_map_read(target_tport);
	} else {
		map = convert_port_to_map(target_tport);
	}

	kr = vm_purgable_control(map, address, control, state);
	vm_map_deallocate(map);

	return kr;
}


/*
 *	Ordinarily, the right to allocate CPM is restricted
 *	to privileged applications (those that can gain access
 *	to the host priv port).  Set this variable to zero if
 *	you want to let any application allocate CPM.
 */
unsigned int    vm_allocate_cpm_privileged = 0;

/*
 *	Allocate memory in the specified map, with the caveat that
 *	the memory is physically contiguous.  This call may fail
 *	if the system can't find sufficient contiguous memory.
 *	This call may cause or lead to heart-stopping amounts of
 *	paging activity.
 *
 *	Memory obtained from this call should be freed in the
 *	normal way, viz., via vm_deallocate.
 */
kern_return_t
vm_allocate_cpm(
	host_priv_t             host_priv,
	vm_map_t                map,
	vm_address_t            *addr,
	vm_size_t               size,
	int                     flags)
{
	vm_map_address_t        map_addr;
	vm_map_size_t           map_size;
	kern_return_t           kr;
	vm_map_kernel_flags_t   vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;

	if (vm_allocate_cpm_privileged && HOST_PRIV_NULL == host_priv) {
		return KERN_INVALID_HOST;
	}

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	map_addr = (vm_map_address_t)*addr;
	map_size = (vm_map_size_t)size;

	vm_map_kernel_flags_set_vmflags(&vmk_flags, flags);
	vm_map_kernel_flags_update_range_id(&vmk_flags, map);

	kr = vm_map_enter_cpm(map, &map_addr, map_size, vmk_flags);

	*addr = CAST_DOWN(vm_address_t, map_addr);
	return kr;
}


kern_return_t
mach_vm_page_query(
	vm_map_t                map,
	mach_vm_offset_t        offset,
	int                     *disposition,
	int                     *ref_count)
{
	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	return vm_map_page_query_internal(
		map,
		vm_map_trunc_page(offset, PAGE_MASK),
		disposition, ref_count);
}

kern_return_t
vm_map_page_query(
	vm_map_t                map,
	vm_offset_t             offset,
	int                     *disposition,
	int                     *ref_count)
{
	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	return vm_map_page_query_internal(
		map,
		vm_map_trunc_page(offset, PAGE_MASK),
		disposition, ref_count);
}

kern_return_t
mach_vm_page_range_query(
	vm_map_t                map,
	mach_vm_offset_t        address,
	mach_vm_size_t          size,
	mach_vm_address_t       dispositions_addr,
	mach_vm_size_t          *dispositions_count)
{
	kern_return_t           kr = KERN_SUCCESS;
	int                     num_pages = 0, i = 0;
	mach_vm_size_t          curr_sz = 0, copy_sz = 0;
	mach_vm_size_t          disp_buf_req_size = 0, disp_buf_total_size = 0;
	mach_msg_type_number_t  count = 0;

	void                    *info = NULL;
	void                    *local_disp = NULL;
	vm_map_size_t           info_size = 0, local_disp_size = 0;
	mach_vm_offset_t        start = 0, end = 0;
	int                     effective_page_shift, effective_page_size, effective_page_mask;

	if (map == VM_MAP_NULL || dispositions_count == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	effective_page_shift = vm_self_region_page_shift_safely(map);
	if (effective_page_shift == -1) {
		return KERN_INVALID_ARGUMENT;
	}
	effective_page_size = (1 << effective_page_shift);
	effective_page_mask = effective_page_size - 1;

	if (os_mul_overflow(*dispositions_count, sizeof(int), &disp_buf_req_size)) {
		return KERN_INVALID_ARGUMENT;
	}

	start = vm_map_trunc_page(address, effective_page_mask);
	end = vm_map_round_page(address + size, effective_page_mask);

	if (end < start) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((end - start) < size) {
		/*
		 * Aligned size is less than unaligned size.
		 */
		return KERN_INVALID_ARGUMENT;
	}

	if (disp_buf_req_size == 0 || (end == start)) {
		return KERN_SUCCESS;
	}

	/*
	 * For large requests, we will go through them
	 * MAX_PAGE_RANGE_QUERY chunk at a time.
	 */

	curr_sz = MIN(end - start, MAX_PAGE_RANGE_QUERY);
	num_pages = (int) (curr_sz >> effective_page_shift);

	info_size = num_pages * sizeof(vm_page_info_basic_data_t);
	info = kalloc_data(info_size, Z_WAITOK);

	local_disp_size = num_pages * sizeof(int);
	local_disp = kalloc_data(local_disp_size, Z_WAITOK);

	if (info == NULL || local_disp == NULL) {
		kr = KERN_RESOURCE_SHORTAGE;
		goto out;
	}

	while (size) {
		count = VM_PAGE_INFO_BASIC_COUNT;
		kr = vm_map_page_range_info_internal(
			map,
			start,
			vm_map_round_page(start + curr_sz, effective_page_mask),
			effective_page_shift,
			VM_PAGE_INFO_BASIC,
			(vm_page_info_t) info,
			&count);

		assert(kr == KERN_SUCCESS);

		for (i = 0; i < num_pages; i++) {
			((int*)local_disp)[i] = ((vm_page_info_basic_t)info)[i].disposition;
		}

		copy_sz = MIN(disp_buf_req_size, num_pages * sizeof(int) /* an int per page */);
		kr = copyout(local_disp, (mach_vm_address_t)dispositions_addr, copy_sz);

		start += curr_sz;
		disp_buf_req_size -= copy_sz;
		disp_buf_total_size += copy_sz;

		if (kr != 0) {
			break;
		}

		if ((disp_buf_req_size == 0) || (curr_sz >= size)) {
			/*
			 * We might have inspected the full range OR
			 * more than it esp. if the user passed in
			 * non-page aligned start/size and/or if we
			 * descended into a submap. We are done here.
			 */

			size = 0;
		} else {
			dispositions_addr += copy_sz;

			size -= curr_sz;

			curr_sz = MIN(vm_map_round_page(size, effective_page_mask), MAX_PAGE_RANGE_QUERY);
			num_pages = (int)(curr_sz >> effective_page_shift);
		}
	}

	*dispositions_count = disp_buf_total_size / sizeof(int);

out:
	kfree_data(local_disp, local_disp_size);
	kfree_data(info, info_size);
	return kr;
}

kern_return_t
mach_vm_page_info(
	vm_map_t                map,
	mach_vm_address_t       address,
	vm_page_info_flavor_t   flavor,
	vm_page_info_t          info,
	mach_msg_type_number_t  *count)
{
	kern_return_t   kr;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = vm_map_page_info(map, address, flavor, info, count);
	return kr;
}

/* map a (whole) upl into an address space */
kern_return_t
vm_upl_map(
	vm_map_t                map,
	upl_t                   upl,
	vm_address_t            *dst_addr)
{
	vm_map_offset_t         map_addr;
	kern_return_t           kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = vm_map_enter_upl(map, upl, &map_addr);
	*dst_addr = CAST_DOWN(vm_address_t, map_addr);
	return kr;
}

kern_return_t
vm_upl_unmap(
	vm_map_t                map,
	upl_t                   upl)
{
	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	return vm_map_remove_upl(map, upl);
}

/* map a part of a upl into an address space with requested protection. */
kern_return_t
vm_upl_map_range(
	vm_map_t                map,
	upl_t                   upl,
	vm_offset_t             offset_to_map,
	vm_size_t               size_to_map,
	vm_prot_t               prot_to_map,
	vm_address_t            *dst_addr)
{
	vm_map_offset_t         map_addr, aligned_offset_to_map, adjusted_offset;
	kern_return_t           kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}
	aligned_offset_to_map = VM_MAP_TRUNC_PAGE(offset_to_map, VM_MAP_PAGE_MASK(map));
	adjusted_offset =  offset_to_map - aligned_offset_to_map;
	size_to_map = VM_MAP_ROUND_PAGE(size_to_map + adjusted_offset, VM_MAP_PAGE_MASK(map));

	kr = vm_map_enter_upl_range(map, upl, aligned_offset_to_map, size_to_map, prot_to_map, &map_addr);
	*dst_addr = CAST_DOWN(vm_address_t, (map_addr + adjusted_offset));
	return kr;
}

/* unmap a part of a upl that was mapped in the address space. */
kern_return_t
vm_upl_unmap_range(
	vm_map_t                map,
	upl_t                   upl,
	vm_offset_t             offset_to_unmap,
	vm_size_t               size_to_unmap)
{
	vm_map_offset_t         aligned_offset_to_unmap, page_offset;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	aligned_offset_to_unmap = VM_MAP_TRUNC_PAGE(offset_to_unmap, VM_MAP_PAGE_MASK(map));
	page_offset =  offset_to_unmap - aligned_offset_to_unmap;
	size_to_unmap = VM_MAP_ROUND_PAGE(size_to_unmap + page_offset, VM_MAP_PAGE_MASK(map));

	return vm_map_remove_upl_range(map, upl, aligned_offset_to_unmap, size_to_unmap);
}

/* Retrieve a upl for an object underlying an address range in a map */

kern_return_t
vm_map_get_upl(
	vm_map_t                map,
	vm_map_offset_t         map_offset,
	upl_size_t              *upl_size,
	upl_t                   *upl,
	upl_page_info_array_t   page_list,
	unsigned int            *count,
	upl_control_flags_t     *flags,
	vm_tag_t                tag,
	int                     force_data_sync)
{
	upl_control_flags_t map_flags;
	kern_return_t       kr;

	if (VM_MAP_NULL == map) {
		return KERN_INVALID_ARGUMENT;
	}

	map_flags = *flags & ~UPL_NOZEROFILL;
	if (force_data_sync) {
		map_flags |= UPL_FORCE_DATA_SYNC;
	}

	kr = vm_map_create_upl(map,
	    map_offset,
	    upl_size,
	    upl,
	    page_list,
	    count,
	    &map_flags,
	    tag);

	*flags = (map_flags & ~UPL_FORCE_DATA_SYNC);
	return kr;
}

/*
 * mach_make_memory_entry_64
 *
 * Think of it as a two-stage vm_remap() operation.  First
 * you get a handle.  Second, you get map that handle in
 * somewhere else. Rather than doing it all at once (and
 * without needing access to the other whole map).
 */
kern_return_t
mach_make_memory_entry_64(
	vm_map_t                target_map,
	memory_object_size_t    *size,
	memory_object_offset_t  offset,
	vm_prot_t               permission,
	ipc_port_t              *object_handle,
	ipc_port_t              parent_handle)
{
	vm_named_entry_kernel_flags_t   vmne_kflags;

	if ((permission & MAP_MEM_FLAGS_MASK) & ~MAP_MEM_FLAGS_USER) {
		/*
		 * Unknown flag: reject for forward compatibility.
		 */
		return KERN_INVALID_VALUE;
	}

	vmne_kflags = VM_NAMED_ENTRY_KERNEL_FLAGS_NONE;
	if (permission & MAP_MEM_LEDGER_TAGGED) {
		vmne_kflags.vmnekf_ledger_tag = VM_LEDGER_TAG_DEFAULT;
	}
	return mach_make_memory_entry_internal(target_map,
	           size,
	           offset,
	           permission,
	           vmne_kflags,
	           object_handle,
	           parent_handle);
}

kern_return_t
mach_make_memory_entry_internal(
	vm_map_t                target_map,
	memory_object_size_t    *size,
	memory_object_offset_t  offset,
	vm_prot_t               permission,
	vm_named_entry_kernel_flags_t   vmne_kflags,
	ipc_port_t              *object_handle,
	ipc_port_t              parent_handle)
{
	vm_named_entry_t        parent_entry;
	vm_named_entry_t        user_entry;
	kern_return_t           kr = KERN_SUCCESS;
	vm_object_t             object;
	vm_map_size_t           map_size;
	vm_map_offset_t         map_start, map_end;
	vm_map_offset_t         tmp;

	/*
	 * Stash the offset in the page for use by vm_map_enter_mem_object()
	 * in the VM_FLAGS_RETURN_DATA_ADDR/MAP_MEM_USE_DATA_ADDR case.
	 */
	vm_object_offset_t      offset_in_page;

	unsigned int            access;
	vm_prot_t               protections;
	vm_prot_t               original_protections, mask_protections;
	unsigned int            wimg_mode;
	boolean_t               use_data_addr;
	boolean_t               use_4K_compat;

	DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x\n", target_map, offset, *size, permission);

	user_entry = NULL;

	if ((permission & MAP_MEM_FLAGS_MASK) & ~MAP_MEM_FLAGS_ALL) {
		/*
		 * Unknown flag: reject for forward compatibility.
		 */
		DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_VALUE);
		return KERN_INVALID_VALUE;
	}

	parent_entry = mach_memory_entry_from_port(parent_handle);
	if (parent_entry && parent_entry->is_copy) {
		DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_ARGUMENT);
		return KERN_INVALID_ARGUMENT;
	}

	if (target_map == NULL || target_map->pmap == kernel_pmap) {
		offset = pgz_decode(offset, *size);
	}

	if (__improbable(vm_map_range_overflows(target_map, offset, *size))) {
		return KERN_INVALID_ARGUMENT;
	}

	original_protections = permission & VM_PROT_ALL;
	protections = original_protections;
	mask_protections = permission & VM_PROT_IS_MASK;
	access = GET_MAP_MEM(permission);
	use_data_addr = ((permission & MAP_MEM_USE_DATA_ADDR) != 0);
	use_4K_compat = ((permission & MAP_MEM_4K_DATA_ADDR) != 0);

	user_entry = NULL;

	map_start = vm_map_trunc_page(offset, VM_MAP_PAGE_MASK(target_map));

	if (permission & MAP_MEM_ONLY) {
		boolean_t               parent_is_object;

		if (__improbable(os_add_overflow(offset, *size, &map_end))) {
			return KERN_INVALID_ARGUMENT;
		}
		map_end = vm_map_round_page(offset + *size, VM_MAP_PAGE_MASK(target_map));
		if (__improbable(map_end == 0 && *size != 0)) {
			return KERN_INVALID_ARGUMENT;
		}
		map_size = map_end - map_start;

		if (use_data_addr || use_4K_compat || parent_entry == NULL) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_ARGUMENT);
			return KERN_INVALID_ARGUMENT;
		}

		parent_is_object = parent_entry->is_object;
		if (!parent_is_object) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_ARGUMENT);
			return KERN_INVALID_ARGUMENT;
		}
		object = vm_named_entry_to_vm_object(parent_entry);
		if (parent_is_object && object != VM_OBJECT_NULL) {
			wimg_mode = object->wimg_bits;
		} else {
			wimg_mode = VM_WIMG_USE_DEFAULT;
		}
		if ((access != parent_entry->access) &&
		    !(parent_entry->protection & VM_PROT_WRITE)) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_RIGHT);
			return KERN_INVALID_RIGHT;
		}
		vm_prot_to_wimg(access, &wimg_mode);
		if (access != MAP_MEM_NOOP) {
			parent_entry->access = access;
		}
		if (parent_is_object && object &&
		    (access != MAP_MEM_NOOP) &&
		    (!(object->nophyscache))) {
			if (object->wimg_bits != wimg_mode) {
				vm_object_lock(object);
				vm_object_change_wimg_mode(object, wimg_mode);
				vm_object_unlock(object);
			}
		}
		if (object_handle) {
			*object_handle = IP_NULL;
		}
		DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_SUCCESS);
		return KERN_SUCCESS;
	} else if (permission & MAP_MEM_NAMED_CREATE) {
		int     ledger_flags = 0;
		task_t  owner;
		bool    fully_owned = false;

		if (__improbable(os_add_overflow(offset, *size, &map_end))) {
			return KERN_INVALID_ARGUMENT;
		}
		map_end = vm_map_round_page(map_end, VM_MAP_PAGE_MASK(target_map));
		map_size = map_end - map_start;
		if (__improbable(map_size == 0)) {
			*size = 0;
			*object_handle = IPC_PORT_NULL;
			return KERN_SUCCESS;
		}
		if (__improbable(map_end == 0)) {
			return KERN_INVALID_ARGUMENT;
		}

		if (use_data_addr || use_4K_compat) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_ARGUMENT);
			return KERN_INVALID_ARGUMENT;
		}

		/*
		 * Force the creation of the VM object now.
		 */
#if __LP64__
		if (map_size > ANON_MAX_SIZE) {
			kr = KERN_FAILURE;
			goto make_mem_done;
		}
#endif /* __LP64__ */

		object = vm_object_allocate(map_size);
		assert(object != VM_OBJECT_NULL);
		vm_object_lock(object);

		/*
		 * XXX
		 * We use this path when we want to make sure that
		 * nobody messes with the object (coalesce, for
		 * example) before we map it.
		 * We might want to use these objects for transposition via
		 * vm_object_transpose() too, so we don't want any copy or
		 * shadow objects either...
		 */
		object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
		VM_OBJECT_SET_TRUE_SHARE(object, TRUE);

		owner = current_task();
		if ((permission & MAP_MEM_PURGABLE) ||
		    vmne_kflags.vmnekf_ledger_tag) {
			assert(object->vo_owner == NULL);
			assert(object->resident_page_count == 0);
			assert(object->wired_page_count == 0);
			assert(owner != TASK_NULL);
			if (vmne_kflags.vmnekf_ledger_no_footprint) {
				ledger_flags |= VM_LEDGER_FLAG_NO_FOOTPRINT;
				object->vo_no_footprint = TRUE;
			}
			if (permission & MAP_MEM_PURGABLE) {
				if (!(permission & VM_PROT_WRITE)) {
					/* if we can't write, we can't purge */
					vm_object_unlock(object);
					vm_object_deallocate(object);
					kr = KERN_INVALID_ARGUMENT;
					goto make_mem_done;
				}
				VM_OBJECT_SET_PURGABLE(object, VM_PURGABLE_NONVOLATILE);
				if (permission & MAP_MEM_PURGABLE_KERNEL_ONLY) {
					VM_OBJECT_SET_PURGEABLE_ONLY_BY_KERNEL(object, TRUE);
				}
#if __arm64__
				if (owner->task_legacy_footprint) {
					/*
					 * For ios11, we failed to account for
					 * this memory.  Keep doing that for
					 * legacy apps (built before ios12),
					 * for backwards compatibility's sake...
					 */
					owner = kernel_task;
				}
#endif /* __arm64__ */
				vm_purgeable_nonvolatile_enqueue(object, owner);
				/* all memory in this named entry is "owned" */
				fully_owned = true;
			}
		}

		if (vmne_kflags.vmnekf_ledger_tag) {
			/*
			 * Bill this object to the current task's
			 * ledgers for the given tag.
			 */
			if (vmne_kflags.vmnekf_ledger_no_footprint) {
				ledger_flags |= VM_LEDGER_FLAG_NO_FOOTPRINT;
			}
			object->vo_ledger_tag = vmne_kflags.vmnekf_ledger_tag;
			kr = vm_object_ownership_change(
				object,
				vmne_kflags.vmnekf_ledger_tag,
				owner, /* new owner */
				ledger_flags,
				FALSE); /* task_objq locked? */
			if (kr != KERN_SUCCESS) {
				vm_object_unlock(object);
				vm_object_deallocate(object);
				goto make_mem_done;
			}
			/* all memory in this named entry is "owned" */
			fully_owned = true;
		}

#if CONFIG_SECLUDED_MEMORY
		if (secluded_for_iokit && /* global boot-arg */
		    ((permission & MAP_MEM_GRAB_SECLUDED))) {
			object->can_grab_secluded = TRUE;
			assert(!object->eligible_for_secluded);
		}
#endif /* CONFIG_SECLUDED_MEMORY */

		/*
		 * The VM object is brand new and nobody else knows about it,
		 * so we don't need to lock it.
		 */

		wimg_mode = object->wimg_bits;
		vm_prot_to_wimg(access, &wimg_mode);
		if (access != MAP_MEM_NOOP) {
			object->wimg_bits = wimg_mode;
		}

		vm_object_unlock(object);

		/* the object has no pages, so no WIMG bits to update here */

		user_entry = mach_memory_entry_allocate(object_handle);
		vm_named_entry_associate_vm_object(
			user_entry,
			object,
			0,
			map_size,
			(protections & VM_PROT_ALL));
		user_entry->internal = TRUE;
		user_entry->is_sub_map = FALSE;
		user_entry->offset = 0;
		user_entry->data_offset = 0;
		user_entry->protection = protections;
		user_entry->access = access;
		user_entry->size = map_size;
		user_entry->is_fully_owned = fully_owned;

		/* user_object pager and internal fields are not used */
		/* when the object field is filled in.		      */

		*size = CAST_DOWN(vm_size_t, (user_entry->size -
		    user_entry->data_offset));
		DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_SUCCESS);
		return KERN_SUCCESS;
	}

	if (permission & MAP_MEM_VM_COPY) {
		vm_map_copy_t   copy;

		if (target_map == VM_MAP_NULL) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_TASK);
			return KERN_INVALID_TASK;
		}

		if (__improbable(os_add_overflow(offset, *size, &map_end))) {
			return KERN_INVALID_ARGUMENT;
		}
		map_end = vm_map_round_page(map_end, VM_MAP_PAGE_MASK(target_map));
		map_size = map_end - map_start;
		if (__improbable(map_size == 0)) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_ARGUMENT);
			return KERN_INVALID_ARGUMENT;
		}
		if (__improbable(map_end == 0)) {
			return KERN_INVALID_ARGUMENT;
		}

		if (use_data_addr || use_4K_compat) {
			offset_in_page = offset - map_start;
			if (use_4K_compat) {
				offset_in_page &= ~((signed)(0xFFF));
			}
		} else {
			offset_in_page = 0;
		}

		kr = vm_map_copyin_internal(target_map,
		    map_start,
		    map_size,
		    VM_MAP_COPYIN_ENTRY_LIST,
		    &copy);
		if (kr != KERN_SUCCESS) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, kr);
			return kr;
		}
		assert(copy != VM_MAP_COPY_NULL);

		user_entry = mach_memory_entry_allocate(object_handle);
		user_entry->backing.copy = copy;
		user_entry->internal = FALSE;
		user_entry->is_sub_map = FALSE;
		user_entry->is_copy = TRUE;
		user_entry->offset = 0;
		user_entry->protection = protections;
		user_entry->size = map_size;
		user_entry->data_offset = offset_in_page;

		/* is all memory in this named entry "owned"? */
		vm_map_entry_t entry;
		user_entry->is_fully_owned = TRUE;
		for (entry = vm_map_copy_first_entry(copy);
		    entry != vm_map_copy_to_entry(copy);
		    entry = entry->vme_next) {
			if (entry->is_sub_map ||
			    VME_OBJECT(entry) == VM_OBJECT_NULL ||
			    VM_OBJECT_OWNER(VME_OBJECT(entry)) == TASK_NULL) {
				/* this memory is not "owned" */
				user_entry->is_fully_owned = FALSE;
				break;
			}
		}

		*size = CAST_DOWN(vm_size_t, (user_entry->size -
		    user_entry->data_offset));
		DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_SUCCESS);
		return KERN_SUCCESS;
	}

	if ((permission & MAP_MEM_VM_SHARE)
	    || parent_entry == NULL
	    || (permission & MAP_MEM_NAMED_REUSE)) {
		vm_map_copy_t   copy;
		vm_prot_t       cur_prot, max_prot;
		vm_map_kernel_flags_t vmk_flags;
		vm_map_entry_t parent_copy_entry;

		if (target_map == VM_MAP_NULL) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_TASK);
			return KERN_INVALID_TASK;
		}

		if (__improbable(os_add_overflow(offset, *size, &map_end))) {
			return KERN_INVALID_ARGUMENT;
		}
		map_end = vm_map_round_page(map_end, VM_MAP_PAGE_MASK(target_map));
		map_size = map_end - map_start;
		if (__improbable(map_size == 0)) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_INVALID_ARGUMENT);
			return KERN_INVALID_ARGUMENT;
		}
		if (__improbable(map_end == 0)) {
			/* rounding overflow */
			return KERN_INVALID_ARGUMENT;
		}

		vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
		vmk_flags.vmkf_range_id = KMEM_RANGE_ID_DATA;
		parent_copy_entry = VM_MAP_ENTRY_NULL;
		if (!(permission & MAP_MEM_VM_SHARE)) {
			vm_map_t tmp_map, real_map;
			vm_map_version_t version;
			vm_object_t tmp_object;
			vm_object_offset_t obj_off;
			vm_prot_t prot;
			boolean_t wired;
			bool contended;

			/* resolve any pending submap copy-on-write... */
			if (protections & VM_PROT_WRITE) {
				tmp_map = target_map;
				vm_map_lock_read(tmp_map);
				kr = vm_map_lookup_and_lock_object(&tmp_map,
				    map_start,
				    protections | mask_protections,
				    OBJECT_LOCK_EXCLUSIVE,
				    &version,
				    &tmp_object,
				    &obj_off,
				    &prot,
				    &wired,
				    NULL,                       /* fault_info */
				    &real_map,
				    &contended);
				if (kr != KERN_SUCCESS) {
					vm_map_unlock_read(tmp_map);
				} else {
					vm_object_unlock(tmp_object);
					vm_map_unlock_read(tmp_map);
					if (real_map != tmp_map) {
						vm_map_unlock_read(real_map);
					}
				}
			}
			/* ... and carry on */

			/* stop extracting if VM object changes */
			vmk_flags.vmkf_copy_single_object = TRUE;
			if ((permission & MAP_MEM_NAMED_REUSE) &&
			    parent_entry != NULL &&
			    parent_entry->is_object) {
				vm_map_copy_t parent_copy;
				parent_copy = parent_entry->backing.copy;
				/*
				 * Assert that the vm_map_copy is coming from the right
				 * zone and hasn't been forged
				 */
				vm_map_copy_require(parent_copy);
				assert(parent_copy->cpy_hdr.nentries == 1);
				parent_copy_entry = vm_map_copy_first_entry(parent_copy);
				assert(!parent_copy_entry->is_sub_map);
			}
		}

		if (use_data_addr || use_4K_compat) {
			offset_in_page = offset - map_start;
			if (use_4K_compat) {
				offset_in_page &= ~((signed)(0xFFF));
			}
		} else {
			offset_in_page = 0;
		}

		if (mask_protections) {
			/*
			 * caller is asking for whichever proctections are
			 * available: no required protections.
			 */
			cur_prot = VM_PROT_NONE;
			max_prot = VM_PROT_NONE;
		} else {
			/*
			 * Caller wants a memory entry with "protections".
			 * Make sure we extract only memory that matches that.
			 */
			cur_prot = protections;
			max_prot = protections;
		}
		if (target_map->pmap == kernel_pmap) {
			/*
			 * Get "reserved" map entries to avoid deadlocking
			 * on the kernel map or a kernel submap if we
			 * run out of VM map entries and need to refill that
			 * zone.
			 */
			vmk_flags.vmkf_copy_pageable = FALSE;
		} else {
			vmk_flags.vmkf_copy_pageable = TRUE;
		}
		vmk_flags.vmkf_copy_same_map = FALSE;
		assert(map_size != 0);
		kr = vm_map_copy_extract(target_map,
		    map_start,
		    map_size,
		    FALSE,                      /* copy */
		    &copy,
		    &cur_prot,
		    &max_prot,
		    VM_INHERIT_SHARE,
		    vmk_flags);
		if (kr != KERN_SUCCESS) {
			DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, kr);
			if (VM_MAP_PAGE_SHIFT(target_map) < PAGE_SHIFT) {
//				panic("DEBUG4K %s:%d kr 0x%x", __FUNCTION__, __LINE__, kr);
			}
			return kr;
		}
		assert(copy != VM_MAP_COPY_NULL);

		if (mask_protections) {
			/*
			 * We just want as much of "original_protections"
			 * as we can get out of the actual "cur_prot".
			 */
			protections &= cur_prot;
			if (protections == VM_PROT_NONE) {
				/* no access at all: fail */
				DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_PROTECTION_FAILURE);
				if (VM_MAP_PAGE_SHIFT(target_map) < PAGE_SHIFT) {
//					panic("DEBUG4K %s:%d kr 0x%x", __FUNCTION__, __LINE__, kr);
				}
				vm_map_copy_discard(copy);
				return KERN_PROTECTION_FAILURE;
			}
		} else {
			/*
			 * We want exactly "original_protections"
			 * out of "cur_prot".
			 */
			assert((cur_prot & protections) == protections);
			assert((max_prot & protections) == protections);
			/* XXX FBDP TODO: no longer needed? */
			if ((cur_prot & protections) != protections) {
				if (VM_MAP_PAGE_SHIFT(target_map) < PAGE_SHIFT) {
//					panic("DEBUG4K %s:%d kr 0x%x", __FUNCTION__, __LINE__, KERN_PROTECTION_FAILURE);
				}
				vm_map_copy_discard(copy);
				DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_PROTECTION_FAILURE);
				return KERN_PROTECTION_FAILURE;
			}
		}

		if (!(permission & MAP_MEM_VM_SHARE)) {
			vm_map_entry_t copy_entry;

			/* limit size to what's actually covered by "copy" */
			assert(copy->cpy_hdr.nentries == 1);
			copy_entry = vm_map_copy_first_entry(copy);
			map_size = copy_entry->vme_end - copy_entry->vme_start;

			if ((permission & MAP_MEM_NAMED_REUSE) &&
			    parent_copy_entry != VM_MAP_ENTRY_NULL &&
			    VME_OBJECT(copy_entry) == VME_OBJECT(parent_copy_entry) &&
			    VME_OFFSET(copy_entry) == VME_OFFSET(parent_copy_entry) &&
			    parent_entry->offset == 0 &&
			    parent_entry->size == map_size &&
			    (parent_entry->data_offset == offset_in_page)) {
				/* we have a match: re-use "parent_entry" */

				/* release our new "copy" */
				vm_map_copy_discard(copy);
				/* get extra send right on handle */
				parent_handle = ipc_port_copy_send_any(parent_handle);

				*size = CAST_DOWN(vm_size_t,
				    (parent_entry->size -
				    parent_entry->data_offset));
				*object_handle = parent_handle;
				DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_SUCCESS);
				return KERN_SUCCESS;
			}

			/* no match: we need to create a new entry */
			object = VME_OBJECT(copy_entry);
			vm_object_lock(object);
			wimg_mode = object->wimg_bits;
			if (!(object->nophyscache)) {
				vm_prot_to_wimg(access, &wimg_mode);
			}
			if (object->wimg_bits != wimg_mode) {
				vm_object_change_wimg_mode(object, wimg_mode);
			}
			vm_object_unlock(object);
		}

		user_entry = mach_memory_entry_allocate(object_handle);
		user_entry->backing.copy = copy;
		user_entry->is_sub_map = FALSE;
		user_entry->is_object = FALSE;
		user_entry->internal = FALSE;
		user_entry->protection = protections;
		user_entry->size = map_size;
		user_entry->data_offset = offset_in_page;

		if (permission & MAP_MEM_VM_SHARE) {
			vm_map_entry_t copy_entry;

			user_entry->is_copy = TRUE;
			user_entry->offset = 0;

			/* is all memory in this named entry "owned"? */
			user_entry->is_fully_owned = TRUE;
			for (copy_entry = vm_map_copy_first_entry(copy);
			    copy_entry != vm_map_copy_to_entry(copy);
			    copy_entry = copy_entry->vme_next) {
				if (copy_entry->is_sub_map ||
				    VM_OBJECT_OWNER(VME_OBJECT(copy_entry)) == TASK_NULL) {
					/* this memory is not "owned" */
					user_entry->is_fully_owned = FALSE;
					break;
				}
			}
		} else {
			user_entry->is_object = TRUE;
			user_entry->internal = object->internal;
			user_entry->offset = VME_OFFSET(vm_map_copy_first_entry(copy));
			user_entry->access = GET_MAP_MEM(permission);
			/* is all memory in this named entry "owned"? */
			if (VM_OBJECT_OWNER(vm_named_entry_to_vm_object(user_entry)) != TASK_NULL) {
				user_entry->is_fully_owned = TRUE;
			}
		}

		*size = CAST_DOWN(vm_size_t, (user_entry->size -
		    user_entry->data_offset));
		DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_SUCCESS);
		return KERN_SUCCESS;
	}

	/* The new object will be based on an existing named object */
	if (parent_entry == NULL) {
		kr = KERN_INVALID_ARGUMENT;
		goto make_mem_done;
	}

	if (parent_entry->is_copy) {
		panic("parent_entry %p is_copy not supported", parent_entry);
		kr = KERN_INVALID_ARGUMENT;
		goto make_mem_done;
	}

	if (use_data_addr || use_4K_compat) {
		/*
		 * submaps and pagers should only be accessible from within
		 * the kernel, which shouldn't use the data address flag, so can fail here.
		 */
		if (parent_entry->is_sub_map) {
			panic("Shouldn't be using data address with a parent entry that is a submap.");
		}
		/*
		 * Account for offset to data in parent entry and
		 * compute our own offset to data.
		 */
		if (__improbable(os_add3_overflow(offset, *size, parent_entry->data_offset, &map_size))) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
		if (map_size > parent_entry->size) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}

		if (__improbable(os_add_overflow(offset, parent_entry->data_offset, &map_start))) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
		map_start = vm_map_trunc_page(map_start, PAGE_MASK);
		offset_in_page = (offset + parent_entry->data_offset) - map_start;
		if (use_4K_compat) {
			offset_in_page &= ~((signed)(0xFFF));
		}
		if (__improbable(os_add3_overflow(offset, parent_entry->data_offset, *size, &map_end))) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
		map_end = vm_map_round_page(map_end, PAGE_MASK);
		if (__improbable(map_end == 0 && *size != 0)) {
			/* rounding overflow */
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
		map_size = map_end - map_start;
	} else {
		if (__improbable(os_add_overflow(offset, *size, &map_end))) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
		map_end = vm_map_round_page(map_end, PAGE_MASK);
		if (__improbable(map_end == 0 && *size != 0)) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
		map_size = map_end - map_start;
		offset_in_page = 0;

		if (__improbable(os_add_overflow(offset, map_size, &tmp))) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
		if ((offset + map_size) > parent_entry->size) {
			kr = KERN_INVALID_ARGUMENT;
			goto make_mem_done;
		}
	}

	if (mask_protections) {
		/*
		 * The caller asked us to use the "protections" as
		 * a mask, so restrict "protections" to what this
		 * mapping actually allows.
		 */
		protections &= parent_entry->protection;
	}
	if ((protections & parent_entry->protection) != protections) {
		kr = KERN_PROTECTION_FAILURE;
		goto make_mem_done;
	}

	if (__improbable(os_add_overflow(parent_entry->offset, map_start, &tmp))) {
		kr = KERN_INVALID_ARGUMENT;
		goto make_mem_done;
	}
	user_entry = mach_memory_entry_allocate(object_handle);
	user_entry->size = map_size;
	user_entry->offset = parent_entry->offset + map_start;
	user_entry->data_offset = offset_in_page;
	user_entry->is_sub_map = parent_entry->is_sub_map;
	user_entry->is_copy = parent_entry->is_copy;
	user_entry->protection = protections;

	if (access != MAP_MEM_NOOP) {
		user_entry->access = access;
	}

	if (parent_entry->is_sub_map) {
		vm_map_t map = parent_entry->backing.map;
		vm_map_reference(map);
		user_entry->backing.map = map;
	} else {
		object = vm_named_entry_to_vm_object(parent_entry);
		assert(object != VM_OBJECT_NULL);
		assert(object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC);
		vm_named_entry_associate_vm_object(
			user_entry,
			object,
			user_entry->offset,
			user_entry->size,
			(user_entry->protection & VM_PROT_ALL));
		assert(user_entry->is_object);
		/* we now point to this object, hold on */
		vm_object_lock(object);
		vm_object_reference_locked(object);
#if VM_OBJECT_TRACKING_OP_TRUESHARE
		if (!object->true_share &&
		    vm_object_tracking_btlog) {
			btlog_record(vm_object_tracking_btlog, object,
			    VM_OBJECT_TRACKING_OP_TRUESHARE,
			    btref_get(__builtin_frame_address(0), 0));
		}
#endif /* VM_OBJECT_TRACKING_OP_TRUESHARE */

		VM_OBJECT_SET_TRUE_SHARE(object, TRUE);
		if (object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
			object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
		}
		vm_object_unlock(object);
	}
	*size = CAST_DOWN(vm_size_t, (user_entry->size -
	    user_entry->data_offset));
	DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, KERN_SUCCESS);
	return KERN_SUCCESS;

make_mem_done:
	DEBUG4K_MEMENTRY("map %p offset 0x%llx size 0x%llx prot 0x%x -> entry %p kr 0x%x\n", target_map, offset, *size, permission, user_entry, kr);
	return kr;
}

kern_return_t
_mach_make_memory_entry(
	vm_map_t                target_map,
	memory_object_size_t    *size,
	memory_object_offset_t  offset,
	vm_prot_t               permission,
	ipc_port_t              *object_handle,
	ipc_port_t              parent_entry)
{
	memory_object_size_t    mo_size;
	kern_return_t           kr;

	mo_size = (memory_object_size_t)*size;
	kr = mach_make_memory_entry_64(target_map, &mo_size,
	    (memory_object_offset_t)offset, permission, object_handle,
	    parent_entry);
	*size = mo_size;
	return kr;
}

kern_return_t
mach_make_memory_entry(
	vm_map_t                target_map,
	vm_size_t               *size,
	vm_offset_t             offset,
	vm_prot_t               permission,
	ipc_port_t              *object_handle,
	ipc_port_t              parent_entry)
{
	memory_object_size_t    mo_size;
	kern_return_t           kr;

	mo_size = (memory_object_size_t)*size;
	kr = mach_make_memory_entry_64(target_map, &mo_size,
	    (memory_object_offset_t)offset, permission, object_handle,
	    parent_entry);
	*size = CAST_DOWN(vm_size_t, mo_size);
	return kr;
}

/*
 *	task_wire
 *
 *	Set or clear the map's wiring_required flag.  This flag, if set,
 *	will cause all future virtual memory allocation to allocate
 *	user wired memory.  Unwiring pages wired down as a result of
 *	this routine is done with the vm_wire interface.
 */
kern_return_t
task_wire(
	vm_map_t        map,
	boolean_t       must_wire __unused)
{
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	return KERN_NOT_SUPPORTED;
}

kern_return_t
vm_map_exec_lockdown(
	vm_map_t        map)
{
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_lock(map);
	map->map_disallow_new_exec = TRUE;
	vm_map_unlock(map);

	return KERN_SUCCESS;
}

__private_extern__ vm_named_entry_t
mach_memory_entry_allocate(ipc_port_t *user_handle_p)
{
	vm_named_entry_t user_entry;

	user_entry = kalloc_type(struct vm_named_entry,
	    Z_WAITOK | Z_ZERO | Z_NOFAIL);
	named_entry_lock_init(user_entry);

	*user_handle_p = ipc_kobject_alloc_port((ipc_kobject_t)user_entry,
	    IKOT_NAMED_ENTRY,
	    IPC_KOBJECT_ALLOC_MAKE_SEND | IPC_KOBJECT_ALLOC_NSREQUEST);

#if VM_NAMED_ENTRY_DEBUG
	/* backtrace at allocation time, for debugging only */
	user_entry->named_entry_bt = btref_get(__builtin_frame_address(0), 0);
#endif /* VM_NAMED_ENTRY_DEBUG */
	return user_entry;
}

/*
 *	mach_memory_object_memory_entry_64
 *
 *	Create a named entry backed by the provided pager.
 *
 */
kern_return_t
mach_memory_object_memory_entry_64(
	host_t                  host,
	boolean_t               internal,
	vm_object_offset_t      size,
	vm_prot_t               permission,
	memory_object_t         pager,
	ipc_port_t              *entry_handle)
{
	vm_named_entry_t        user_entry;
	ipc_port_t              user_handle;
	vm_object_t             object;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	size = vm_object_round_page(size);

	if (pager == MEMORY_OBJECT_NULL && internal) {
		object = vm_object_allocate(size);
		if (object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
			object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
		}
	} else {
		object = memory_object_to_vm_object(pager);
		if (object != VM_OBJECT_NULL) {
			vm_object_reference(object);
		}
	}
	if (object == VM_OBJECT_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	user_entry = mach_memory_entry_allocate(&user_handle);
	user_entry->size = size;
	user_entry->offset = 0;
	user_entry->protection = permission & VM_PROT_ALL;
	user_entry->access = GET_MAP_MEM(permission);
	user_entry->is_sub_map = FALSE;

	vm_named_entry_associate_vm_object(user_entry, object, 0, size,
	    (user_entry->protection & VM_PROT_ALL));
	user_entry->internal = object->internal;
	assert(object->internal == internal);
	if (VM_OBJECT_OWNER(object) != TASK_NULL) {
		/* all memory in this entry is "owned" */
		user_entry->is_fully_owned = TRUE;
	}

	*entry_handle = user_handle;
	return KERN_SUCCESS;
}

kern_return_t
mach_memory_object_memory_entry(
	host_t          host,
	boolean_t       internal,
	vm_size_t       size,
	vm_prot_t       permission,
	memory_object_t pager,
	ipc_port_t      *entry_handle)
{
	return mach_memory_object_memory_entry_64( host, internal,
	           (vm_object_offset_t)size, permission, pager, entry_handle);
}


kern_return_t
mach_memory_entry_purgable_control(
	ipc_port_t      entry_port,
	vm_purgable_t   control,
	int             *state)
{
	if (control == VM_PURGABLE_SET_STATE_FROM_KERNEL) {
		/* not allowed from user-space */
		return KERN_INVALID_ARGUMENT;
	}

	return memory_entry_purgeable_control_internal(entry_port, control, state);
}

kern_return_t
memory_entry_purgeable_control_internal(
	ipc_port_t      entry_port,
	vm_purgable_t   control,
	int             *state)
{
	kern_return_t           kr;
	vm_named_entry_t        mem_entry;
	vm_object_t             object;

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (control != VM_PURGABLE_SET_STATE &&
	    control != VM_PURGABLE_GET_STATE &&
	    control != VM_PURGABLE_SET_STATE_FROM_KERNEL) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((control == VM_PURGABLE_SET_STATE ||
	    control == VM_PURGABLE_SET_STATE_FROM_KERNEL) &&
	    (((*state & ~(VM_PURGABLE_ALL_MASKS)) != 0) ||
	    ((*state & VM_PURGABLE_STATE_MASK) > VM_PURGABLE_STATE_MASK))) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map ||
	    mem_entry->is_copy) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	assert(mem_entry->is_object);
	object = vm_named_entry_to_vm_object(mem_entry);
	if (object == VM_OBJECT_NULL) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_lock(object);

	/* check that named entry covers entire object ? */
	if (mem_entry->offset != 0 || object->vo_size != mem_entry->size) {
		vm_object_unlock(object);
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_unlock(mem_entry);

	kr = vm_object_purgable_control(object, control, state);

	vm_object_unlock(object);

	return kr;
}

kern_return_t
mach_memory_entry_access_tracking(
	ipc_port_t      entry_port,
	int             *access_tracking,
	uint32_t        *access_tracking_reads,
	uint32_t        *access_tracking_writes)
{
	return memory_entry_access_tracking_internal(entry_port,
	           access_tracking,
	           access_tracking_reads,
	           access_tracking_writes);
}

kern_return_t
memory_entry_access_tracking_internal(
	ipc_port_t      entry_port,
	int             *access_tracking,
	uint32_t        *access_tracking_reads,
	uint32_t        *access_tracking_writes)
{
	vm_named_entry_t        mem_entry;
	vm_object_t             object;
	kern_return_t           kr;

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map ||
	    mem_entry->is_copy) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	assert(mem_entry->is_object);
	object = vm_named_entry_to_vm_object(mem_entry);
	if (object == VM_OBJECT_NULL) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

#if VM_OBJECT_ACCESS_TRACKING
	vm_object_access_tracking(object,
	    access_tracking,
	    access_tracking_reads,
	    access_tracking_writes);
	kr = KERN_SUCCESS;
#else /* VM_OBJECT_ACCESS_TRACKING */
	(void) access_tracking;
	(void) access_tracking_reads;
	(void) access_tracking_writes;
	kr = KERN_NOT_SUPPORTED;
#endif /* VM_OBJECT_ACCESS_TRACKING */

	named_entry_unlock(mem_entry);

	return kr;
}

#if DEVELOPMENT || DEBUG
/* For dtrace probe in mach_memory_entry_ownership */
extern int proc_selfpid(void);
extern char *proc_name_address(void *p);
#endif /* DEVELOPMENT || DEBUG */

/* Kernel call only, MIG uses *_from_user() below */
kern_return_t
mach_memory_entry_ownership(
	ipc_port_t      entry_port,
	task_t          owner,
	int             ledger_tag,
	int             ledger_flags)
{
	task_t                  cur_task;
	kern_return_t           kr;
	vm_named_entry_t        mem_entry;
	vm_object_t             object;

	cur_task = current_task();
	if (cur_task != kernel_task &&
	    ((owner != cur_task && owner != TASK_NULL) ||
	    (ledger_flags & VM_LEDGER_FLAG_NO_FOOTPRINT) ||
	    (ledger_flags & VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG) ||
	    ledger_tag == VM_LEDGER_TAG_NETWORK)) {
		bool transfer_ok = false;

		/*
		 * An entitlement is required to:
		 * + tranfer memory ownership to someone else,
		 * + request that the memory not count against the footprint,
		 * + tag as "network" (since that implies "no footprint")
		 *
		 * Exception: task with task_no_footprint_for_debug == 1 on internal build
		 */
		if (!cur_task->task_can_transfer_memory_ownership &&
		    IOCurrentTaskHasEntitlement("com.apple.private.memory.ownership_transfer")) {
			cur_task->task_can_transfer_memory_ownership = TRUE;
		}
		if (cur_task->task_can_transfer_memory_ownership) {
			/* we're allowed to transfer ownership to any task */
			transfer_ok = true;
		}
#if DEVELOPMENT || DEBUG
		if (!transfer_ok &&
		    ledger_tag == VM_LEDGER_TAG_DEFAULT &&
		    (ledger_flags & VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG) &&
		    cur_task->task_no_footprint_for_debug) {
			int         to_panic = 0;
			static bool init_bootarg = false;

			/*
			 * Allow performance tools running on internal builds to hide memory usage from phys_footprint even
			 * WITHOUT an entitlement. This can be enabled by per task sysctl vm.task_no_footprint_for_debug=1
			 * with the ledger tag VM_LEDGER_TAG_DEFAULT and flag VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG.
			 *
			 * If the boot-arg "panic_on_no_footprint_for_debug" is set, the kernel will
			 * panic here in order to detect any abuse of this feature, which is intended solely for
			 * memory debugging purpose.
			 */
			if (!init_bootarg) {
				PE_parse_boot_argn("panic_on_no_footprint_for_debug", &to_panic, sizeof(to_panic));
				init_bootarg = true;
			}
			if (to_panic) {
				panic("%s: panic_on_no_footprint_for_debug is triggered by pid %d procname %s", __func__, proc_selfpid(), get_bsdtask_info(cur_task)? proc_name_address(get_bsdtask_info(cur_task)) : "?");
			}

			/*
			 * Flushing out user space processes using this interface:
			 * $ dtrace -n 'task_no_footprint_for_debug {printf("%d[%s]\n", pid, execname); stack(); ustack();}'
			 */
			DTRACE_VM(task_no_footprint_for_debug);
			transfer_ok = true;
		}
#endif /* DEVELOPMENT || DEBUG */
		if (!transfer_ok) {
#define TRANSFER_ENTITLEMENT_MAX_LENGTH 1024 /* XXX ? */
			const char *our_id, *their_id;
			our_id = IOTaskGetEntitlement(current_task(), "com.apple.developer.memory.transfer-send");
			their_id = IOTaskGetEntitlement(owner, "com.apple.developer.memory.transfer-accept");
			if (our_id && their_id &&
			    !strncmp(our_id, their_id, TRANSFER_ENTITLEMENT_MAX_LENGTH)) {
				/* allow transfer between tasks that have matching entitlements */
				if (strnlen(our_id, TRANSFER_ENTITLEMENT_MAX_LENGTH) < TRANSFER_ENTITLEMENT_MAX_LENGTH &&
				    strnlen(their_id, TRANSFER_ENTITLEMENT_MAX_LENGTH) < TRANSFER_ENTITLEMENT_MAX_LENGTH) {
					transfer_ok = true;
				} else {
					/* complain about entitlement(s) being too long... */
					assertf((strlen(our_id) <= TRANSFER_ENTITLEMENT_MAX_LENGTH &&
					    strlen(their_id) <= TRANSFER_ENTITLEMENT_MAX_LENGTH),
					    "our_id:%lu their_id:%lu",
					    strlen(our_id), strlen(their_id));
				}
			}
		}
		if (!transfer_ok) {
			/* transfer denied */
			return KERN_NO_ACCESS;
		}

		if (ledger_flags & VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG) {
			/*
			 * We've made it past the checks above, so we either
			 * have the entitlement or the sysctl.
			 * Convert to VM_LEDGER_FLAG_NO_FOOTPRINT.
			 */
			ledger_flags &= ~VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG;
			ledger_flags |= VM_LEDGER_FLAG_NO_FOOTPRINT;
		}
	}

	if (ledger_flags & ~VM_LEDGER_FLAGS) {
		return KERN_INVALID_ARGUMENT;
	}
	if (ledger_tag == VM_LEDGER_TAG_UNCHANGED) {
		/* leave "ledger_tag" unchanged */
	} else if (ledger_tag < 0 ||
	    ledger_tag > VM_LEDGER_TAG_MAX) {
		return KERN_INVALID_ARGUMENT;
	}
	if (owner == TASK_NULL) {
		/* leave "owner" unchanged */
		owner = VM_OBJECT_OWNER_UNCHANGED;
	}

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map ||
	    !mem_entry->is_fully_owned) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	if (mem_entry->is_object) {
		object = vm_named_entry_to_vm_object(mem_entry);
		if (object == VM_OBJECT_NULL) {
			named_entry_unlock(mem_entry);
			return KERN_INVALID_ARGUMENT;
		}
		vm_object_lock(object);
		/* check that named entry covers entire object ? */
		if (mem_entry->offset != 0 || object->vo_size != mem_entry->size) {
			vm_object_unlock(object);
			named_entry_unlock(mem_entry);
			return KERN_INVALID_ARGUMENT;
		}
		named_entry_unlock(mem_entry);
		kr = vm_object_ownership_change(object,
		    ledger_tag,
		    owner,
		    ledger_flags,
		    FALSE);                             /* task_objq_locked */
		vm_object_unlock(object);
	} else if (mem_entry->is_copy) {
		vm_map_copy_t copy;
		vm_map_entry_t entry;

		copy = mem_entry->backing.copy;
		named_entry_unlock(mem_entry);
		for (entry = vm_map_copy_first_entry(copy);
		    entry != vm_map_copy_to_entry(copy);
		    entry = entry->vme_next) {
			object = VME_OBJECT(entry);
			if (entry->is_sub_map ||
			    object == VM_OBJECT_NULL) {
				kr = KERN_INVALID_ARGUMENT;
				break;
			}
			vm_object_lock(object);
			if (VME_OFFSET(entry) != 0 ||
			    entry->vme_end - entry->vme_start != object->vo_size) {
				vm_object_unlock(object);
				kr = KERN_INVALID_ARGUMENT;
				break;
			}
			kr = vm_object_ownership_change(object,
			    ledger_tag,
			    owner,
			    ledger_flags,
			    FALSE);                             /* task_objq_locked */
			vm_object_unlock(object);
			if (kr != KERN_SUCCESS) {
				kr = KERN_INVALID_ARGUMENT;
				break;
			}
		}
	} else {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	return kr;
}

/* MIG call from userspace */
kern_return_t
mach_memory_entry_ownership_from_user(
	ipc_port_t      entry_port,
	mach_port_t     owner_port,
	int             ledger_tag,
	int             ledger_flags)
{
	task_t owner = TASK_NULL;
	kern_return_t kr;

	if (IP_VALID(owner_port)) {
		if (ip_kotype(owner_port) == IKOT_TASK_ID_TOKEN) {
			task_id_token_t token = convert_port_to_task_id_token(owner_port);
			(void)task_identity_token_get_task_grp(token, &owner, TASK_GRP_MIG);
			task_id_token_release(token);
			/* token ref released */
		} else {
			owner = convert_port_to_task_mig(owner_port);
		}
	}
	/* hold task ref on owner (Nullable) */

	if (owner && task_is_a_corpse(owner)) {
		/* identity token can represent a corpse, disallow it */
		task_deallocate_mig(owner);
		owner = TASK_NULL;
	}

	/* mach_memory_entry_ownership() will handle TASK_NULL owner */
	kr = mach_memory_entry_ownership(entry_port, owner, /* Nullable */
	    ledger_tag, ledger_flags);

	if (owner) {
		task_deallocate_mig(owner);
	}

	if (kr == KERN_SUCCESS) {
		/* MIG rule, consume port right on success */
		ipc_port_release_send(owner_port);
	}
	return kr;
}

kern_return_t
mach_memory_entry_get_page_counts(
	ipc_port_t      entry_port,
	unsigned int    *resident_page_count,
	unsigned int    *dirty_page_count)
{
	kern_return_t           kr;
	vm_named_entry_t        mem_entry;
	vm_object_t             object;
	vm_object_offset_t      offset;
	vm_object_size_t        size;

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map ||
	    mem_entry->is_copy) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	assert(mem_entry->is_object);
	object = vm_named_entry_to_vm_object(mem_entry);
	if (object == VM_OBJECT_NULL) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_lock(object);

	offset = mem_entry->offset;
	size = mem_entry->size;
	size = vm_object_round_page(offset + size) - vm_object_trunc_page(offset);
	offset = vm_object_trunc_page(offset);

	named_entry_unlock(mem_entry);

	kr = vm_object_get_page_counts(object, offset, size, resident_page_count, dirty_page_count);

	vm_object_unlock(object);

	return kr;
}

kern_return_t
mach_memory_entry_phys_page_offset(
	ipc_port_t              entry_port,
	vm_object_offset_t      *offset_p)
{
	vm_named_entry_t        mem_entry;
	vm_object_t             object;
	vm_object_offset_t      offset;
	vm_object_offset_t      data_offset;

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map ||
	    mem_entry->is_copy) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	assert(mem_entry->is_object);
	object = vm_named_entry_to_vm_object(mem_entry);
	if (object == VM_OBJECT_NULL) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	offset = mem_entry->offset;
	data_offset = mem_entry->data_offset;

	named_entry_unlock(mem_entry);

	*offset_p = offset - vm_object_trunc_page(offset) + data_offset;
	return KERN_SUCCESS;
}

kern_return_t
mach_memory_entry_map_size(
	ipc_port_t             entry_port,
	vm_map_t               map,
	memory_object_offset_t offset,
	memory_object_offset_t size,
	mach_vm_size_t         *map_size)
{
	vm_named_entry_t        mem_entry;
	vm_object_t             object;
	vm_object_offset_t      object_offset_start, object_offset_end;
	vm_map_copy_t           copy_map, target_copy_map;
	vm_map_offset_t         overmap_start, overmap_end, trimmed_start;
	kern_return_t           kr;

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	if (mem_entry->is_object) {
		object = vm_named_entry_to_vm_object(mem_entry);
		if (object == VM_OBJECT_NULL) {
			named_entry_unlock(mem_entry);
			return KERN_INVALID_ARGUMENT;
		}

		object_offset_start = mem_entry->offset;
		object_offset_start += mem_entry->data_offset;
		object_offset_start += offset;
		object_offset_end = object_offset_start + size;
		object_offset_start = vm_map_trunc_page(object_offset_start,
		    VM_MAP_PAGE_MASK(map));
		object_offset_end = vm_map_round_page(object_offset_end,
		    VM_MAP_PAGE_MASK(map));

		named_entry_unlock(mem_entry);

		*map_size = object_offset_end - object_offset_start;
		return KERN_SUCCESS;
	}

	if (!mem_entry->is_copy) {
		panic("unsupported type of mem_entry %p", mem_entry);
	}

	assert(mem_entry->is_copy);
	if (VM_MAP_COPY_PAGE_MASK(mem_entry->backing.copy) == VM_MAP_PAGE_MASK(map)) {
		*map_size = vm_map_round_page(mem_entry->offset + mem_entry->data_offset + offset + size, VM_MAP_PAGE_MASK(map)) - vm_map_trunc_page(mem_entry->offset + mem_entry->data_offset + offset, VM_MAP_PAGE_MASK(map));
		DEBUG4K_SHARE("map %p (%d) mem_entry %p offset 0x%llx + 0x%llx + 0x%llx size 0x%llx -> map_size 0x%llx\n", map, VM_MAP_PAGE_MASK(map), mem_entry, mem_entry->offset, mem_entry->data_offset, offset, size, *map_size);
		named_entry_unlock(mem_entry);
		return KERN_SUCCESS;
	}

	DEBUG4K_SHARE("mem_entry %p copy %p (%d) map %p (%d) offset 0x%llx size 0x%llx\n", mem_entry, mem_entry->backing.copy, VM_MAP_COPY_PAGE_SHIFT(mem_entry->backing.copy), map, VM_MAP_PAGE_SHIFT(map), offset, size);
	copy_map = mem_entry->backing.copy;
	target_copy_map = VM_MAP_COPY_NULL;
	DEBUG4K_ADJUST("adjusting...\n");
	kr = vm_map_copy_adjust_to_target(copy_map,
	    mem_entry->data_offset + offset,
	    size,
	    map,
	    FALSE,
	    &target_copy_map,
	    &overmap_start,
	    &overmap_end,
	    &trimmed_start);
	if (kr == KERN_SUCCESS) {
		if (target_copy_map->size != copy_map->size) {
			DEBUG4K_ADJUST("copy %p (%d) map %p (%d) offset 0x%llx size 0x%llx overmap_start 0x%llx overmap_end 0x%llx trimmed_start 0x%llx map_size 0x%llx -> 0x%llx\n", copy_map, VM_MAP_COPY_PAGE_SHIFT(copy_map), map, VM_MAP_PAGE_SHIFT(map), (uint64_t)offset, (uint64_t)size, (uint64_t)overmap_start, (uint64_t)overmap_end, (uint64_t)trimmed_start, (uint64_t)copy_map->size, (uint64_t)target_copy_map->size);
		}
		*map_size = target_copy_map->size;
		if (target_copy_map != copy_map) {
			vm_map_copy_discard(target_copy_map);
		}
		target_copy_map = VM_MAP_COPY_NULL;
	}
	named_entry_unlock(mem_entry);
	return kr;
}

/*
 * mach_memory_entry_port_release:
 *
 * Release a send right on a named entry port.  This is the correct
 * way to destroy a named entry.  When the last right on the port is
 * released, mach_memory_entry_no_senders() willl be called.
 */
void
mach_memory_entry_port_release(
	ipc_port_t      port)
{
	assert(ip_kotype(port) == IKOT_NAMED_ENTRY);
	ipc_port_release_send(port);
}

vm_named_entry_t
mach_memory_entry_from_port(ipc_port_t port)
{
	if (IP_VALID(port)) {
		return ipc_kobject_get_stable(port, IKOT_NAMED_ENTRY);
	}
	return NULL;
}

/*
 * mach_memory_entry_no_senders:
 *
 * Destroys the memory entry associated with a mach port.
 * Memory entries have the exact same lifetime as their owning port.
 *
 * Releasing a memory entry is done by calling
 * mach_memory_entry_port_release() on its owning port.
 */
static void
mach_memory_entry_no_senders(ipc_port_t port, mach_port_mscount_t mscount)
{
	vm_named_entry_t named_entry;

	named_entry = ipc_kobject_dealloc_port(port, mscount, IKOT_NAMED_ENTRY);

	if (named_entry->is_sub_map) {
		vm_map_deallocate(named_entry->backing.map);
	} else if (named_entry->is_copy) {
		vm_map_copy_discard(named_entry->backing.copy);
	} else if (named_entry->is_object) {
		assert(named_entry->backing.copy->cpy_hdr.nentries == 1);
		vm_map_copy_discard(named_entry->backing.copy);
	} else {
		assert(named_entry->backing.copy == VM_MAP_COPY_NULL);
	}

#if VM_NAMED_ENTRY_DEBUG
	btref_put(named_entry->named_entry_bt);
#endif /* VM_NAMED_ENTRY_DEBUG */

	named_entry_lock_destroy(named_entry);
	kfree_type(struct vm_named_entry, named_entry);
}

/* Allow manipulation of individual page state.  This is actually part of */
/* the UPL regimen but takes place on the memory entry rather than on a UPL */

kern_return_t
mach_memory_entry_page_op(
	ipc_port_t              entry_port,
	vm_object_offset_t      offset,
	int                     ops,
	ppnum_t                 *phys_entry,
	int                     *flags)
{
	vm_named_entry_t        mem_entry;
	vm_object_t             object;
	kern_return_t           kr;

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map ||
	    mem_entry->is_copy) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	assert(mem_entry->is_object);
	object = vm_named_entry_to_vm_object(mem_entry);
	if (object == VM_OBJECT_NULL) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_reference(object);
	named_entry_unlock(mem_entry);

	kr = vm_object_page_op(object, offset, ops, phys_entry, flags);

	vm_object_deallocate(object);

	return kr;
}

/*
 * mach_memory_entry_range_op offers performance enhancement over
 * mach_memory_entry_page_op for page_op functions which do not require page
 * level state to be returned from the call.  Page_op was created to provide
 * a low-cost alternative to page manipulation via UPLs when only a single
 * page was involved.  The range_op call establishes the ability in the _op
 * family of functions to work on multiple pages where the lack of page level
 * state handling allows the caller to avoid the overhead of the upl structures.
 */

kern_return_t
mach_memory_entry_range_op(
	ipc_port_t              entry_port,
	vm_object_offset_t      offset_beg,
	vm_object_offset_t      offset_end,
	int                     ops,
	int                     *range)
{
	vm_named_entry_t        mem_entry;
	vm_object_t             object;
	kern_return_t           kr;

	mem_entry = mach_memory_entry_from_port(entry_port);
	if (mem_entry == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(mem_entry);

	if (mem_entry->is_sub_map ||
	    mem_entry->is_copy) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	assert(mem_entry->is_object);
	object = vm_named_entry_to_vm_object(mem_entry);
	if (object == VM_OBJECT_NULL) {
		named_entry_unlock(mem_entry);
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_reference(object);
	named_entry_unlock(mem_entry);

	kr = vm_object_range_op(object,
	    offset_beg,
	    offset_end,
	    ops,
	    (uint32_t *) range);

	vm_object_deallocate(object);

	return kr;
}

/* ******* Temporary Internal calls to UPL for BSD ***** */

extern int kernel_upl_map(
	vm_map_t        map,
	upl_t           upl,
	vm_offset_t     *dst_addr);

extern int kernel_upl_unmap(
	vm_map_t        map,
	upl_t           upl);

extern int kernel_upl_commit(
	upl_t                   upl,
	upl_page_info_t         *pl,
	mach_msg_type_number_t   count);

extern int kernel_upl_commit_range(
	upl_t                   upl,
	upl_offset_t             offset,
	upl_size_t              size,
	int                     flags,
	upl_page_info_array_t   pl,
	mach_msg_type_number_t  count);

extern int kernel_upl_abort(
	upl_t                   upl,
	int                     abort_type);

extern int kernel_upl_abort_range(
	upl_t                   upl,
	upl_offset_t             offset,
	upl_size_t               size,
	int                     abort_flags);


kern_return_t
kernel_upl_map(
	vm_map_t        map,
	upl_t           upl,
	vm_offset_t     *dst_addr)
{
	return vm_upl_map(map, upl, dst_addr);
}


kern_return_t
kernel_upl_unmap(
	vm_map_t        map,
	upl_t           upl)
{
	return vm_upl_unmap(map, upl);
}

kern_return_t
kernel_upl_commit(
	upl_t                   upl,
	upl_page_info_t        *pl,
	mach_msg_type_number_t  count)
{
	kern_return_t   kr;

	kr = upl_commit(upl, pl, count);
	upl_deallocate(upl);
	return kr;
}


kern_return_t
kernel_upl_commit_range(
	upl_t                   upl,
	upl_offset_t            offset,
	upl_size_t              size,
	int                     flags,
	upl_page_info_array_t   pl,
	mach_msg_type_number_t  count)
{
	boolean_t               finished = FALSE;
	kern_return_t           kr;

	if (flags & UPL_COMMIT_FREE_ON_EMPTY) {
		flags |= UPL_COMMIT_NOTIFY_EMPTY;
	}

	if (flags & UPL_COMMIT_KERNEL_ONLY_FLAGS) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = upl_commit_range(upl, offset, size, flags, pl, count, &finished);

	if ((flags & UPL_COMMIT_NOTIFY_EMPTY) && finished) {
		upl_deallocate(upl);
	}

	return kr;
}

kern_return_t
kernel_upl_abort_range(
	upl_t                   upl,
	upl_offset_t            offset,
	upl_size_t              size,
	int                     abort_flags)
{
	kern_return_t           kr;
	boolean_t               finished = FALSE;

	if (abort_flags & UPL_COMMIT_FREE_ON_EMPTY) {
		abort_flags |= UPL_COMMIT_NOTIFY_EMPTY;
	}

	kr = upl_abort_range(upl, offset, size, abort_flags, &finished);

	if ((abort_flags & UPL_COMMIT_FREE_ON_EMPTY) && finished) {
		upl_deallocate(upl);
	}

	return kr;
}

kern_return_t
kernel_upl_abort(
	upl_t                   upl,
	int                     abort_type)
{
	kern_return_t   kr;

	kr = upl_abort(upl, abort_type);
	upl_deallocate(upl);
	return kr;
}

/*
 * Now a kernel-private interface (for BootCache
 * use only).  Need a cleaner way to create an
 * empty vm_map() and return a handle to it.
 */

kern_return_t
vm_region_object_create(
	vm_map_t                target_map,
	vm_size_t               size,
	ipc_port_t              *object_handle)
{
	vm_named_entry_t        user_entry;
	vm_map_t                new_map;

	user_entry = mach_memory_entry_allocate(object_handle);

	/* Create a named object based on a submap of specified size */

	new_map = vm_map_create_options(PMAP_NULL, VM_MAP_MIN_ADDRESS,
	    vm_map_round_page(size, VM_MAP_PAGE_MASK(target_map)),
	    VM_MAP_CREATE_PAGEABLE);
	vm_map_set_page_shift(new_map, VM_MAP_PAGE_SHIFT(target_map));

	user_entry->backing.map = new_map;
	user_entry->internal = TRUE;
	user_entry->is_sub_map = TRUE;
	user_entry->offset = 0;
	user_entry->protection = VM_PROT_ALL;
	user_entry->size = size;

	return KERN_SUCCESS;
}

ppnum_t vm_map_get_phys_page(           /* forward */
	vm_map_t        map,
	vm_offset_t     offset);

ppnum_t
vm_map_get_phys_page(
	vm_map_t                map,
	vm_offset_t             addr)
{
	vm_object_offset_t      offset;
	vm_object_t             object;
	vm_map_offset_t         map_offset;
	vm_map_entry_t          entry;
	ppnum_t                 phys_page = 0;

	map_offset = vm_map_trunc_page(addr, PAGE_MASK);

	vm_map_lock(map);
	while (vm_map_lookup_entry(map, map_offset, &entry)) {
		if (entry->is_sub_map) {
			vm_map_t        old_map;
			vm_map_lock(VME_SUBMAP(entry));
			old_map = map;
			map = VME_SUBMAP(entry);
			map_offset = (VME_OFFSET(entry) +
			    (map_offset - entry->vme_start));
			vm_map_unlock(old_map);
			continue;
		}
		if (VME_OBJECT(entry) == VM_OBJECT_NULL) {
			vm_map_unlock(map);
			return (ppnum_t) 0;
		}
		if (VME_OBJECT(entry)->phys_contiguous) {
			/* These are  not standard pageable memory mappings */
			/* If they are not present in the object they will  */
			/* have to be picked up from the pager through the  */
			/* fault mechanism.  */
			if (VME_OBJECT(entry)->vo_shadow_offset == 0) {
				/* need to call vm_fault */
				vm_map_unlock(map);
				vm_fault(map, map_offset, VM_PROT_NONE,
				    FALSE /* change_wiring */, VM_KERN_MEMORY_NONE,
				    THREAD_UNINT, NULL, 0);
				vm_map_lock(map);
				continue;
			}
			offset = (VME_OFFSET(entry) +
			    (map_offset - entry->vme_start));
			phys_page = (ppnum_t)
			    ((VME_OBJECT(entry)->vo_shadow_offset
			    + offset) >> PAGE_SHIFT);
			break;
		}
		offset = (VME_OFFSET(entry) + (map_offset - entry->vme_start));
		object = VME_OBJECT(entry);
		vm_object_lock(object);
		while (TRUE) {
			vm_page_t dst_page = vm_page_lookup(object, offset);
			if (dst_page == VM_PAGE_NULL) {
				if (object->shadow) {
					vm_object_t old_object;
					vm_object_lock(object->shadow);
					old_object = object;
					offset = offset + object->vo_shadow_offset;
					object = object->shadow;
					vm_object_unlock(old_object);
				} else {
					vm_object_unlock(object);
					break;
				}
			} else {
				phys_page = (ppnum_t)(VM_PAGE_GET_PHYS_PAGE(dst_page));
				vm_object_unlock(object);
				break;
			}
		}
		break;
	}

	vm_map_unlock(map);
	return phys_page;
}

kern_return_t
mach_vm_deferred_reclamation_buffer_init(
	task_t task,
	mach_vm_offset_t address,
	mach_vm_size_t size)
{
#if CONFIG_DEFERRED_RECLAIM
	return vm_deferred_reclamation_buffer_init_internal(task, address, size);
#else
	(void) task;
	(void) address;
	(void) size;
	(void) indices;
	return KERN_NOT_SUPPORTED;
#endif /* CONFIG_DEFERRED_RECLAIM */
}

kern_return_t
mach_vm_deferred_reclamation_buffer_synchronize(
	task_t task,
	mach_vm_size_t num_entries_to_reclaim)
{
#if CONFIG_DEFERRED_RECLAIM
	return vm_deferred_reclamation_buffer_synchronize_internal(task, num_entries_to_reclaim);
#else
	(void) task;
	(void) num_entries_to_reclaim;
	return KERN_NOT_SUPPORTED;
#endif /* CONFIG_DEFERRED_RECLAIM */
}

kern_return_t
mach_vm_deferred_reclamation_buffer_update_reclaimable_bytes(task_t task, mach_vm_size_t reclaimable_bytes)
{
#if CONFIG_DEFERRED_RECLAIM
	return vm_deferred_reclamation_buffer_update_reclaimable_bytes_internal(task, reclaimable_bytes);
#else
	(void) task;
	(void) reclaimable_bytes;
	return KERN_NOT_SUPPORTED;
#endif /* CONFIG_DEFERRED_RECLAIM */
}

#if 0
kern_return_t kernel_object_iopl_request(       /* forward */
	vm_named_entry_t        named_entry,
	memory_object_offset_t  offset,
	upl_size_t              *upl_size,
	upl_t                   *upl_ptr,
	upl_page_info_array_t   user_page_list,
	unsigned int            *page_list_count,
	int                     *flags);

kern_return_t
kernel_object_iopl_request(
	vm_named_entry_t        named_entry,
	memory_object_offset_t  offset,
	upl_size_t              *upl_size,
	upl_t                   *upl_ptr,
	upl_page_info_array_t   user_page_list,
	unsigned int            *page_list_count,
	int                     *flags)
{
	vm_object_t             object;
	kern_return_t           ret;

	int                     caller_flags;

	caller_flags = *flags;

	if (caller_flags & ~UPL_VALID_FLAGS) {
		/*
		 * For forward compatibility's sake,
		 * reject any unknown flag.
		 */
		return KERN_INVALID_VALUE;
	}

	/* a few checks to make sure user is obeying rules */
	if (*upl_size == 0) {
		if (offset >= named_entry->size) {
			return KERN_INVALID_RIGHT;
		}
		*upl_size = (upl_size_t) (named_entry->size - offset);
		if (*upl_size != named_entry->size - offset) {
			return KERN_INVALID_ARGUMENT;
		}
	}
	if (caller_flags & UPL_COPYOUT_FROM) {
		if ((named_entry->protection & VM_PROT_READ)
		    != VM_PROT_READ) {
			return KERN_INVALID_RIGHT;
		}
	} else {
		if ((named_entry->protection &
		    (VM_PROT_READ | VM_PROT_WRITE))
		    != (VM_PROT_READ | VM_PROT_WRITE)) {
			return KERN_INVALID_RIGHT;
		}
	}
	if (named_entry->size < (offset + *upl_size)) {
		return KERN_INVALID_ARGUMENT;
	}

	/* the callers parameter offset is defined to be the */
	/* offset from beginning of named entry offset in object */
	offset = offset + named_entry->offset;

	if (named_entry->is_sub_map ||
	    named_entry->is_copy) {
		return KERN_INVALID_ARGUMENT;
	}

	named_entry_lock(named_entry);

	/* This is the case where we are going to operate */
	/* on an already known object.  If the object is */
	/* not ready it is internal.  An external     */
	/* object cannot be mapped until it is ready  */
	/* we can therefore avoid the ready check     */
	/* in this case.  */
	assert(named_entry->is_object);
	object = vm_named_entry_to_vm_object(named_entry);
	vm_object_reference(object);
	named_entry_unlock(named_entry);

	if (!object->private) {
		if (*upl_size > MAX_UPL_TRANSFER_BYTES) {
			*upl_size = MAX_UPL_TRANSFER_BYTES;
		}
		if (object->phys_contiguous) {
			*flags = UPL_PHYS_CONTIG;
		} else {
			*flags = 0;
		}
	} else {
		*flags = UPL_DEV_MEMORY | UPL_PHYS_CONTIG;
	}

	ret = vm_object_iopl_request(object,
	    offset,
	    *upl_size,
	    upl_ptr,
	    user_page_list,
	    page_list_count,
	    (upl_control_flags_t)(unsigned int)caller_flags);
	vm_object_deallocate(object);
	return ret;
}
#endif

/*
 * These symbols are looked up at runtime by vmware, VirtualBox,
 * despite not being exported in the symbol sets.
 */

#if defined(__x86_64__)

kern_return_t
mach_vm_map(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t  initial_size,
	mach_vm_offset_t        mask,
	int                     flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance);

kern_return_t
mach_vm_remap(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t  size,
	mach_vm_offset_t        mask,
	int                     flags,
	vm_map_t                src_map,
	mach_vm_offset_t        memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,
	vm_prot_t               *max_protection,
	vm_inherit_t            inheritance);

kern_return_t
mach_vm_map(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t  initial_size,
	mach_vm_offset_t        mask,
	int                     flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	return mach_vm_map_external(target_map, address, initial_size, mask, flags, port,
	           offset, copy, cur_protection, max_protection, inheritance);
}

kern_return_t
mach_vm_remap(
	vm_map_t                target_map,
	mach_vm_offset_t        *address,
	mach_vm_size_t  size,
	mach_vm_offset_t        mask,
	int                     flags,
	vm_map_t                src_map,
	mach_vm_offset_t        memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,   /* OUT */
	vm_prot_t               *max_protection,   /* OUT */
	vm_inherit_t            inheritance)
{
	return mach_vm_remap_external(target_map, address, size, mask, flags, src_map, memory_address,
	           copy, cur_protection, max_protection, inheritance);
}

kern_return_t
vm_map(
	vm_map_t                target_map,
	vm_offset_t             *address,
	vm_size_t               size,
	vm_offset_t             mask,
	int                     flags,
	ipc_port_t              port,
	vm_offset_t             offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance);

kern_return_t
vm_map(
	vm_map_t                target_map,
	vm_offset_t             *address,
	vm_size_t               size,
	vm_offset_t             mask,
	int                     flags,
	ipc_port_t              port,
	vm_offset_t             offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	static_assert(sizeof(vm_offset_t) == sizeof(mach_vm_offset_t));

	return mach_vm_map(target_map, (mach_vm_offset_t *)address,
	           size, mask, flags, port, offset, copy,
	           cur_protection, max_protection, inheritance);
}

#endif /* __x86_64__ */
