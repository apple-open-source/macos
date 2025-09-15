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
#include "mock_pmap.h"

#include <vm/pmap.h>

T_MOCK(void *,
pmap_steal_memory, (vm_size_t size, vm_size_t alignment))
{
	return checked_alloc_align(size, alignment);
}


T_MOCK(void,
pmap_startup, (vm_offset_t * startp, vm_offset_t * endp))
{
	// TODO rdar://136915968
}

T_MOCK(boolean_t,
pmap_virtual_region, (unsigned int region_select, vm_map_offset_t * startp, vm_map_size_t * size))
{
	return false; // TODO rdar://136915968
}

extern const struct page_table_attr * const native_pt_attr;


T_MOCK(pmap_t,
pmap_create_options, (
	ledger_t ledger,
	vm_map_size_t size,
	unsigned int flags))
{
	pmap_t p = (pmap_t)calloc(1, sizeof(struct pmap));
	// this is needed for pmap_shared_region_size_min()
	p->pmap_pt_attr = native_pt_attr;

	return p;
}

T_MOCK(void,
pmap_set_nested, (
	pmap_t pmap))
{
}

T_MOCK(kern_return_t,
pmap_nest, (
	pmap_t grand,
	pmap_t subord,
	addr64_t vstart,
	uint64_t size))
{
	return KERN_SUCCESS;
}

T_MOCK(kern_return_t,
pmap_unnest_options, (
	pmap_t grand,
	addr64_t vaddr,
	uint64_t size,
	unsigned int option))
{
	return KERN_SUCCESS;
}

T_MOCK(void,
pmap_remove_options, (
	pmap_t pmap,
	vm_map_address_t start,
	vm_map_address_t end,
	int options))
{
}

T_MOCK(void,
pmap_destroy, (
	pmap_t pmap))
{
}
T_MOCK_DYNAMIC(uint64_t,
    pmap_shared_region_size_min, (pmap_t pmap), (pmap),
{
	// the default behaviour for arm64
	return 0x0000000002000000ULL;
})

T_MOCK_DYNAMIC(
	unsigned int,
	pmap_cache_attributes,
	(ppnum_t phys), (phys),
	{ return 0; })

T_MOCK_DYNAMIC(
	pmap_paddr_t,
	kvtophys,
	(vm_offset_t offs), (offs),
	{ return 0; })
