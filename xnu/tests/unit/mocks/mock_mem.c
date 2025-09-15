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
#include "dt_proxy.h"
#include "unit_test_utils.h"

/* This is an implementation of simple fixed size same-size objects pool */
struct mock_mem_pool {
	size_t elem_size;
	char *buffer;
	char *free_head;
	uint32_t free_count;
};

void
mock_mem_init(struct mock_mem_pool* mm, size_t elem_sz, uint32_t count)
{
	mm->elem_size = elem_sz;
	size_t buf_size = elem_sz * count;
	mm->buffer = aligned_alloc(8, buf_size);
	PT_QUIET; PT_ASSERT_NOTNULL(mm->buffer, "failed alloc");
	memset(mm->buffer, 0, buf_size);
	mm->free_head = mm->buffer;
	mm->free_count = count;
}

void *
mock_mem_alloc(struct mock_mem_pool* mm)
{
	PT_QUIET; PT_ASSERT_NOTNULL(mm->buffer, "mock mem not allocated");
	PT_QUIET; PT_ASSERT_TRUE(mm->free_count > 0, "no more space left");
	void *ret = mm->free_head;
	mm->free_head += mm->elem_size;
	mm->free_count--;
	return ret;
}

void
mock_mem_free(struct mock_mem_pool* mm, void *ptr)
{
	// not implemeted yet rdar://136915968
}

struct mock_mem_pool mm_vm_objects;


// this is used for vm_object and vm_page pointer packing
uintptr_t mock_page_ptr_base;

void
mock_mem_init_vm_objects(void)
{
	mock_mem_init(&mm_vm_objects, 256, 100);
	mock_page_ptr_base = (uintptr_t)mm_vm_objects.buffer;
}
void *
mock_mem_alloc_vm_object(void)
{
	return mock_mem_alloc(&mm_vm_objects);
}
