/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
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
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	File:	vm/vm_debug.c.
 *	Author:	Rich Draves
 *	Date:	March, 1990
 *
 *	Exported kernel calls.  See mach_debug/mach_debug.defs.
 */
#include <mach_vm_debug.h>
#include <mach/kern_return.h>
#include <mach/mach_host_server.h>
#include <mach_debug/vm_info.h>
#include <mach_debug/page_info.h>
#include <mach_debug/hash_info.h>

#if MACH_VM_DEBUG
#include <mach/machine/vm_types.h>
#include <mach/memory_object_types.h>
#include <mach/vm_prot.h>
#include <mach/vm_inherit.h>
#include <mach/vm_param.h>
#include <kern/thread.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_object_xnu.h>
#include <kern/task.h>
#include <kern/host.h>
#include <ipc/ipc_port.h>
#include <vm/vm_debug_internal.h>
#endif

#if !MACH_VM_DEBUG
#define __DEBUG_ONLY __unused
#else /* !MACH_VM_DEBUG */
#define __DEBUG_ONLY
#endif /* !MACH_VM_DEBUG */

#ifdef VM32_SUPPORT

#include <mach/vm32_map_server.h>
#include <mach/vm_map.h>

/*
 *	Routine:	mach_vm_region_info [kernel call]
 *	Purpose:
 *		Retrieve information about a VM region,
 *		including info about the object chain.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Retrieve region/object info.
 *		KERN_INVALID_TASK	The map is null.
 *		KERN_NO_SPACE		There is no entry at/after the address.
 *		KERN_RESOURCE_SHORTAGE	Can't allocate memory.
 */

kern_return_t
vm32_region_info(
	__DEBUG_ONLY vm_map_t                   map,
	__DEBUG_ONLY vm32_offset_t              address,
	__DEBUG_ONLY vm_info_region_t           *regionp,
	__DEBUG_ONLY vm_info_object_array_t     *objectsp,
	__DEBUG_ONLY mach_msg_type_number_t     *objectsCntp)
{
#if !MACH_VM_DEBUG
	return KERN_FAILURE;
#else
	vm_map_copy_t copy;
	vm_offset_t addr = 0;   /* memory for OOL data */
	vm_size_t size;         /* size of the memory */
	unsigned int room;      /* room for this many objects */
	unsigned int used;      /* actually this many objects */
	vm_info_region_t region;
	kern_return_t kr;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_TASK;
	}

	size = 0;               /* no memory allocated yet */

	for (;;) {
		vm_map_t cmap;  /* current map in traversal */
		vm_map_t nmap;  /* next map to look at */
		vm_map_entry_t entry;
		vm_object_t object, cobject, nobject;

		/* nothing is locked */

		vm_map_lock_read(map);
		for (cmap = map;; cmap = nmap) {
			/* cmap is read-locked */

			if (!vm_map_lookup_entry_allow_pgz(cmap,
			    (vm_map_address_t)address, &entry)) {
				entry = entry->vme_next;
				if (entry == vm_map_to_entry(cmap)) {
					vm_map_unlock_read(cmap);
					if (size != 0) {
						kmem_free(ipc_kernel_map,
						    addr, size);
					}
					return KERN_NO_SPACE;
				}
			}

			if (entry->is_sub_map) {
				nmap = VME_SUBMAP(entry);
			} else {
				break;
			}

			/* move down to the lower map */

			vm_map_lock_read(nmap);
			vm_map_unlock_read(cmap);
		}

		/* cmap is read-locked; we have a real entry */

		object = VME_OBJECT(entry);
		region.vir_start = (natural_t) entry->vme_start;
		region.vir_end = (natural_t) entry->vme_end;
		region.vir_object = (natural_t)(uintptr_t) object;
		region.vir_offset = (natural_t) VME_OFFSET(entry);
		region.vir_needs_copy = entry->needs_copy;
		region.vir_protection = entry->protection;
		region.vir_max_protection = entry->max_protection;
		region.vir_inheritance = entry->inheritance;
		region.vir_wired_count = entry->wired_count;
		region.vir_user_wired_count = entry->user_wired_count;

		used = 0;
		room = (unsigned int) (size / sizeof(vm_info_object_t));

		if (object == VM_OBJECT_NULL) {
			vm_map_unlock_read(cmap);
			/* no memory needed */
			break;
		}

		vm_object_lock(object);
		vm_map_unlock_read(cmap);

		for (cobject = object;; cobject = nobject) {
			/* cobject is locked */

			if (used < room) {
				vm_info_object_t *vio =
				    &((vm_info_object_t *) addr)[used];

				vio->vio_object =
				    (natural_t)(uintptr_t) cobject;
				vio->vio_size =
				    (natural_t) cobject->vo_size;
				vio->vio_ref_count =
				    cobject->ref_count;
				vio->vio_resident_page_count =
				    cobject->resident_page_count;
				vio->vio_copy =
				    (natural_t)(uintptr_t) cobject->vo_copy;
				vio->vio_shadow =
				    (natural_t)(uintptr_t) cobject->shadow;
				vio->vio_shadow_offset =
				    (natural_t) cobject->vo_shadow_offset;
				vio->vio_paging_offset =
				    (natural_t) cobject->paging_offset;
				vio->vio_copy_strategy =
				    cobject->copy_strategy;
				vio->vio_last_alloc =
				    (vm_offset_t) cobject->last_alloc;
				vio->vio_paging_in_progress =
				    cobject->paging_in_progress +
				    cobject->activity_in_progress;
				vio->vio_pager_created =
				    cobject->pager_created;
				vio->vio_pager_initialized =
				    cobject->pager_initialized;
				vio->vio_pager_ready =
				    cobject->pager_ready;
				vio->vio_can_persist =
				    cobject->can_persist;
				vio->vio_internal =
				    cobject->internal;
				vio->vio_temporary =
				    FALSE;
				vio->vio_alive =
				    cobject->alive;
				vio->vio_purgable =
				    (cobject->purgable != VM_PURGABLE_DENY);
				vio->vio_purgable_volatile =
				    (cobject->purgable == VM_PURGABLE_VOLATILE ||
				    cobject->purgable == VM_PURGABLE_EMPTY);
			}

			used++;
			nobject = cobject->shadow;
			if (nobject == VM_OBJECT_NULL) {
				vm_object_unlock(cobject);
				break;
			}

			vm_object_lock(nobject);
			vm_object_unlock(cobject);
		}

		/* nothing locked */

		if (used <= room) {
			break;
		}

		/* must allocate more memory */

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr, size);
		}
		size = vm_map_round_page(2 * used * sizeof(vm_info_object_t),
		    VM_MAP_PAGE_MASK(ipc_kernel_map));

		kr = kmem_alloc(ipc_kernel_map, &addr, size,
		    KMA_DATA, VM_KERN_MEMORY_IPC);
		if (kr != KERN_SUCCESS) {
			return KERN_RESOURCE_SHORTAGE;
		}
	}

	/* free excess memory; make remaining memory pageable */

	if (used == 0) {
		copy = VM_MAP_COPY_NULL;

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr, size);
		}
	} else {
		vm_size_t size_used = (used * sizeof(vm_info_object_t));
		vm_size_t vmsize_used = vm_map_round_page(size_used,
		    VM_MAP_PAGE_MASK(ipc_kernel_map));

		if (size_used < vmsize_used) {
			bzero((char *)addr + size_used, vmsize_used - size_used);
		}

		kr = vm_map_unwire(ipc_kernel_map, addr, addr + size_used, FALSE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)addr,
		    (vm_map_size_t)size_used, TRUE, &copy);
		assert(kr == KERN_SUCCESS);

		if (size != vmsize_used) {
			kmem_free(ipc_kernel_map,
			    addr + vmsize_used, size - vmsize_used);
		}
	}

	*regionp = region;
	*objectsp = (vm_info_object_array_t) copy;
	*objectsCntp = used;
	return KERN_SUCCESS;
#endif /* MACH_VM_DEBUG */
}

/*
 *  Temporary call for 64 bit data path interface transiotion
 */

kern_return_t
vm32_region_info_64(
	__DEBUG_ONLY vm_map_t                   map,
	__DEBUG_ONLY vm32_offset_t              address,
	__DEBUG_ONLY vm_info_region_64_t        *regionp,
	__DEBUG_ONLY vm_info_object_array_t     *objectsp,
	__DEBUG_ONLY mach_msg_type_number_t     *objectsCntp)
{
#if !MACH_VM_DEBUG
	return KERN_FAILURE;
#else
	vm_map_copy_t copy;
	vm_offset_t addr = 0;   /* memory for OOL data */
	vm_size_t size;         /* size of the memory */
	unsigned int room;      /* room for this many objects */
	unsigned int used;      /* actually this many objects */
	vm_info_region_64_t region;
	kern_return_t kr;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_TASK;
	}

	size = 0;               /* no memory allocated yet */

	for (;;) {
		vm_map_t cmap;  /* current map in traversal */
		vm_map_t nmap;  /* next map to look at */
		vm_map_entry_t entry;
		vm_object_t object, cobject, nobject;

		/* nothing is locked */

		vm_map_lock_read(map);
		for (cmap = map;; cmap = nmap) {
			/* cmap is read-locked */

			if (!vm_map_lookup_entry_allow_pgz(cmap, address, &entry)) {
				entry = entry->vme_next;
				if (entry == vm_map_to_entry(cmap)) {
					vm_map_unlock_read(cmap);
					if (size != 0) {
						kmem_free(ipc_kernel_map,
						    addr, size);
					}
					return KERN_NO_SPACE;
				}
			}

			if (entry->is_sub_map) {
				nmap = VME_SUBMAP(entry);
			} else {
				break;
			}

			/* move down to the lower map */

			vm_map_lock_read(nmap);
			vm_map_unlock_read(cmap);
		}

		/* cmap is read-locked; we have a real entry */

		object = VME_OBJECT(entry);
		region.vir_start = (natural_t) entry->vme_start;
		region.vir_end = (natural_t) entry->vme_end;
		region.vir_object = (natural_t)(uintptr_t) object;
		region.vir_offset = VME_OFFSET(entry);
		region.vir_needs_copy = entry->needs_copy;
		region.vir_protection = entry->protection;
		region.vir_max_protection = entry->max_protection;
		region.vir_inheritance = entry->inheritance;
		region.vir_wired_count = entry->wired_count;
		region.vir_user_wired_count = entry->user_wired_count;

		used = 0;
		room = (unsigned int) (size / sizeof(vm_info_object_t));

		if (object == VM_OBJECT_NULL) {
			vm_map_unlock_read(cmap);
			/* no memory needed */
			break;
		}

		vm_object_lock(object);
		vm_map_unlock_read(cmap);

		for (cobject = object;; cobject = nobject) {
			/* cobject is locked */

			if (used < room) {
				vm_info_object_t *vio =
				    &((vm_info_object_t *) addr)[used];

				vio->vio_object =
				    (natural_t)(uintptr_t) cobject;
				vio->vio_size =
				    (natural_t) cobject->vo_size;
				vio->vio_ref_count =
				    cobject->ref_count;
				vio->vio_resident_page_count =
				    cobject->resident_page_count;
				vio->vio_copy =
				    (natural_t)(uintptr_t) cobject->vo_copy;
				vio->vio_shadow =
				    (natural_t)(uintptr_t) cobject->shadow;
				vio->vio_shadow_offset =
				    (natural_t) cobject->vo_shadow_offset;
				vio->vio_paging_offset =
				    (natural_t) cobject->paging_offset;
				vio->vio_copy_strategy =
				    cobject->copy_strategy;
				vio->vio_last_alloc =
				    (vm_offset_t) cobject->last_alloc;
				vio->vio_paging_in_progress =
				    cobject->paging_in_progress +
				    cobject->activity_in_progress;
				vio->vio_pager_created =
				    cobject->pager_created;
				vio->vio_pager_initialized =
				    cobject->pager_initialized;
				vio->vio_pager_ready =
				    cobject->pager_ready;
				vio->vio_can_persist =
				    cobject->can_persist;
				vio->vio_internal =
				    cobject->internal;
				vio->vio_temporary =
				    FALSE;
				vio->vio_alive =
				    cobject->alive;
				vio->vio_purgable =
				    (cobject->purgable != VM_PURGABLE_DENY);
				vio->vio_purgable_volatile =
				    (cobject->purgable == VM_PURGABLE_VOLATILE ||
				    cobject->purgable == VM_PURGABLE_EMPTY);
			}

			used++;
			nobject = cobject->shadow;
			if (nobject == VM_OBJECT_NULL) {
				vm_object_unlock(cobject);
				break;
			}

			vm_object_lock(nobject);
			vm_object_unlock(cobject);
		}

		/* nothing locked */

		if (used <= room) {
			break;
		}

		/* must allocate more memory */

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr, size);
		}
		size = vm_map_round_page(2 * used * sizeof(vm_info_object_t),
		    VM_MAP_PAGE_MASK(ipc_kernel_map));

		kr = kmem_alloc(ipc_kernel_map, &addr, size,
		    KMA_DATA, VM_KERN_MEMORY_IPC);
		if (kr != KERN_SUCCESS) {
			return KERN_RESOURCE_SHORTAGE;
		}
	}

	/* free excess memory; make remaining memory pageable */

	if (used == 0) {
		copy = VM_MAP_COPY_NULL;

		if (size != 0) {
			kmem_free(ipc_kernel_map, addr, size);
		}
	} else {
		vm_size_t size_used = (used * sizeof(vm_info_object_t));
		vm_size_t vmsize_used = vm_map_round_page(size_used,
		    VM_MAP_PAGE_MASK(ipc_kernel_map));

		if (size_used < vmsize_used) {
			bzero((char *)addr + size_used, vmsize_used - size_used);
		}

		kr = vm_map_unwire(ipc_kernel_map, addr, addr + size_used, FALSE);
		assert(kr == KERN_SUCCESS);

		kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)addr,
		    (vm_map_size_t)size_used, TRUE, &copy);
		assert(kr == KERN_SUCCESS);

		if (size != vmsize_used) {
			kmem_free(ipc_kernel_map,
			    addr + vmsize_used, size - vmsize_used);
		}
	}

	*regionp = region;
	*objectsp = (vm_info_object_array_t) copy;
	*objectsCntp = used;
	return KERN_SUCCESS;
#endif /* MACH_VM_DEBUG */
}
/*
 * Return an array of virtual pages that are mapped to a task.
 */
kern_return_t
vm32_mapped_pages_info(
	__DEBUG_ONLY vm_map_t                   map,
	__DEBUG_ONLY page_address_array_t       *pages,
	__DEBUG_ONLY mach_msg_type_number_t     *pages_count)
{
#if !MACH_VM_DEBUG
	return KERN_FAILURE;
#elif 1 /* pmap_resident_count is gone with rdar://68290810 */
	(void)map; (void)pages; (void)pages_count;
	return KERN_FAILURE;
#else
	pmap_t          pmap;
	vm_size_t       size, size_used;
	unsigned int    actual, space;
	page_address_array_t list;
	mach_vm_offset_t addr = 0;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	pmap = map->pmap;
	size = pmap_resident_count(pmap) * sizeof(vm_offset_t);
	size = vm_map_round_page(size,
	    VM_MAP_PAGE_MASK(ipc_kernel_map));

	for (;;) {
		(void) mach_vm_allocate_kernel(ipc_kernel_map, &addr, size,
		    VM_MAP_KERNEL_FLAGS_ANYWHERE(.vm_tag = VM_KERN_MEMORY_IPC));
		(void) vm_map_unwire(
			ipc_kernel_map,
			vm_map_trunc_page(addr,
			VM_MAP_PAGE_MASK(ipc_kernel_map)),
			vm_map_round_page(addr + size,
			VM_MAP_PAGE_MASK(ipc_kernel_map)),
			FALSE);

		list = (page_address_array_t) addr;
		space = (unsigned int) (size / sizeof(vm_offset_t));

		actual = pmap_list_resident_pages(pmap,
		    list,
		    space);
		if (actual <= space) {
			break;
		}

		/*
		 * Free memory if not enough
		 */
		(void) kmem_free(ipc_kernel_map, addr, size);

		/*
		 * Try again, doubling the size
		 */
		size = vm_map_round_page(actual * sizeof(vm_offset_t),
		    VM_MAP_PAGE_MASK(ipc_kernel_map));
	}
	if (actual == 0) {
		*pages = 0;
		*pages_count = 0;
		(void) kmem_free(ipc_kernel_map, addr, size);
	} else {
		vm_size_t vmsize_used;
		*pages_count = actual;
		size_used = (actual * sizeof(vm_offset_t));
		vmsize_used = vm_map_round_page(size_used,
		    VM_MAP_PAGE_MASK(ipc_kernel_map));
		(void) vm_map_wire_kernel(
			ipc_kernel_map,
			vm_map_trunc_page(addr,
			VM_MAP_PAGE_MASK(ipc_kernel_map)),
			vm_map_round_page(addr + size,
			VM_MAP_PAGE_MASK(ipc_kernel_map)),
			VM_PROT_READ | VM_PROT_WRITE,
			VM_KERN_MEMORY_IPC,
			FALSE);
		(void) vm_map_copyin(ipc_kernel_map,
		    (vm_map_address_t)addr,
		    (vm_map_size_t)size_used,
		    TRUE,
		    (vm_map_copy_t *)pages);
		if (vmsize_used != size) {
			(void) kmem_free(ipc_kernel_map,
			    addr + vmsize_used,
			    size - vmsize_used);
		}
	}

	return KERN_SUCCESS;
#endif /* MACH_VM_DEBUG */
}

#endif /* VM32_SUPPORT */

/*
 *	Routine:	host_virtual_physical_table_info
 *	Purpose:
 *		Return information about the VP table.
 *	Conditions:
 *		Nothing locked.  Obeys CountInOut protocol.
 *	Returns:
 *		KERN_SUCCESS		Returned information.
 *		KERN_INVALID_HOST	The host is null.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
host_virtual_physical_table_info(
	__DEBUG_ONLY host_t                     host,
	__DEBUG_ONLY hash_info_bucket_array_t   *infop,
	__DEBUG_ONLY mach_msg_type_number_t     *countp)
{
#if !MACH_VM_DEBUG
	return KERN_FAILURE;
#else
	vm_offset_t addr = 0;
	vm_size_t size = 0;
	hash_info_bucket_t *info;
	unsigned int potential, actual;
	kern_return_t kr;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	/* start with in-line data */

	info = *infop;
	potential = *countp;

	for (;;) {
		actual = vm_page_info(info, potential);
		if (actual <= potential) {
			break;
		}

		/* allocate more memory */

		if (info != *infop) {
			kmem_free(ipc_kernel_map, addr, size);
		}

		size = vm_map_round_page(actual * sizeof *info,
		    VM_MAP_PAGE_MASK(ipc_kernel_map));
		kr = kmem_alloc(ipc_kernel_map, &addr, size,
		    KMA_PAGEABLE | KMA_DATA, VM_KERN_MEMORY_IPC);
		if (kr != KERN_SUCCESS) {
			return KERN_RESOURCE_SHORTAGE;
		}

		info = (hash_info_bucket_t *) addr;
		potential = (unsigned int) (size / sizeof(*info));
	}

	if (info == *infop) {
		/* data fit in-line; nothing to deallocate */

		*countp = actual;
	} else if (actual == 0) {
		kmem_free(ipc_kernel_map, addr, size);

		*countp = 0;
	} else {
		vm_map_copy_t copy;
		vm_size_t used, vmused;

		used = (actual * sizeof(*info));
		vmused = vm_map_round_page(used, VM_MAP_PAGE_MASK(ipc_kernel_map));

		if (vmused != size) {
			kmem_free(ipc_kernel_map, addr + vmused, size - vmused);
		}

		kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)addr,
		    (vm_map_size_t)used, TRUE, &copy);
		assert(kr == KERN_SUCCESS);

		*infop = (hash_info_bucket_t *) copy;
		*countp = actual;
	}

	return KERN_SUCCESS;
#endif /* MACH_VM_DEBUG */
}
