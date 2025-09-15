/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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

#include "std_safe.h"
#include "unit_test_utils.h"

#include <kern/zalloc.h>
#include <kern/kalloc.h>
#include <vm/vm_kern_xnu.h>
#include <kern/zalloc_internal.h>


#undef kalloc_ext

T_MOCK(struct kalloc_result,
kalloc_ext, (
	void                   *kheap_or_kt_view,
	vm_size_t               size,
	zalloc_flags_t          flags,
	void                   *owner))
{
	void* addr = calloc(1, size);
	return (struct kalloc_result){ .addr = addr, .size = size };
}


T_MOCK(void,
kfree_ext, (void *kheap_or_kt_view, void *data, vm_size_t size))
{
	free(data);
}

T_MOCK(void *,
kalloc_type_impl_internal, (kalloc_type_view_t kt_view, zalloc_flags_t flags))
{
	return calloc(1, kt_view->kt_size);
}
T_MOCK(void *,
kalloc_type_impl_external, (kalloc_type_view_t kt_view, zalloc_flags_t flags))
{
	return calloc(1, kt_view->kt_size);
}

T_MOCK(kmem_return_t,
kmem_alloc_guard, (
	vm_map_t        map,
	vm_size_t       size,
	vm_offset_t     mask,
	kma_flags_t     flags,
	kmem_guard_t    guard))
{
	kmem_return_t kmr = { };
	kmr.kmr_address = (vm_address_t)calloc(1, size);
	// TODO verify allocation rdar://136915968
	// TODO malloc with guard pages?
	kmr.kmr_return = KERN_SUCCESS;
	return kmr;
}

T_MOCK(vm_size_t,
kmem_free_guard, (
	vm_map_t        map,
	vm_offset_t     req_addr,
	vm_size_t       req_size,
	kmf_flags_t     flags,
	kmem_guard_t    guard))
{
	// TODO rdar://136915968
	return req_size;
}

T_MOCK(void *,
zalloc_permanent_tag, (vm_size_t size, vm_offset_t mask, vm_tag_t tag))
{
	// mask is align-1, see ZALIGN()
	return checked_alloc_align(size, mask + 1);
}

T_MOCK(void *,
zalloc_percpu_permanent, (vm_size_t size, vm_offset_t mask))
{
	return MOCK_zalloc_permanent_tag(size, mask, 0);
}

T_MOCK(void,
zalloc_ro_mut, (zone_id_t zid, void *elem, vm_offset_t offset, const void *new_data, vm_size_t new_data_size))
{
	memcpy((void *)((uintptr_t)elem + offset), new_data, new_data_size);
}

T_MOCK(void,
zone_require, (zone_t zone, void *addr))
{
	// TODO rdar://136915968
}

T_MOCK(void,
zone_id_require, (zone_id_t zid, vm_size_t esize, void *addr))
{
	// TODO rdar://136915968
}

T_MOCK(void,
zone_enable_caching, (zone_t zone))
{
}

void *mock_mem_alloc_vm_object(void);

T_MOCK(struct kalloc_result,
zalloc_ext, (zone_t zone, zone_stats_t zstats, zalloc_flags_t flags))
{
	void* addr = NULL;
	if (strcmp(zone->z_name, "vm objects") == 0) {
		addr = mock_mem_alloc_vm_object();
	} else {
		addr = calloc(1, zone->z_elem_size);
	}
	return (struct kalloc_result){ (void *)addr, zone->z_elem_size };
}

T_MOCK(void,
zfree_ext, (zone_t zone, zone_stats_t zstats, void *addr, uint64_t combined_size))
{
	// TODO rdar://136915968
}

T_MOCK(void,
zone_enable_smr, (zone_t zone, struct smr *smr, zone_smr_free_cb_t free_cb))
{
}
