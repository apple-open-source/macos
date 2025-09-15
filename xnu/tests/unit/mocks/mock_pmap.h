/*
 * Copyright (c) 2000-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <vm/pmap.h>
#include <arm/pmap_public.h>
#include "mock_dynamic.h"

T_MOCK_DYNAMIC_DECLARE(
	unsigned int,
	pmap_cache_attributes,
	(ppnum_t phys));

T_MOCK_DYNAMIC_DECLARE(
	pmap_paddr_t,
	kvtophys,
	(vm_offset_t offs));

T_MOCK_DYNAMIC_DECLARE(
	uint64_t,
	pmap_shared_region_size_min,
	(pmap_t pmap));

// This is a useful override for some tests that don't want to deal with huge sizes
// due to the pmap min region size
#define T_MOCK_pmap_shared_region_size_min_RET_PAGE_SIZE() \
	T_MOCK_SET_RETVAL(pmap_shared_region_size_min, uint64_t, PAGE_SIZE)
