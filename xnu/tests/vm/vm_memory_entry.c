/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
#include <darwintest/darwintest.h>
#include <mach/error.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/memory_entry.h>
#include <mach/memory_object_types.h>
#include <mach/vm_page_size.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <unistd.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"),
	T_META_TAG_VM_PREFERRED);


T_DECL(memory_entry_page_counts,
    "Test that page counts are computed correctly for memory entries")
{
	mach_error_t err;
	int ret;
	uint64_t pages = 1024;
	mach_vm_size_t size = pages * vm_page_size;
	mach_port_t memory_entry = MACH_PORT_NULL;
	uint64_t resident, dirty, swapped;

	T_LOG("Creating memory entry");
	err = mach_make_memory_entry_64(mach_task_self(), &size,
	    (memory_object_offset_t)0,
	    (MAP_MEM_NAMED_CREATE | MAP_MEM_LEDGER_TAGGED | VM_PROT_DEFAULT),
	    &memory_entry, MEMORY_OBJECT_NULL);
	T_QUIET; T_ASSERT_MACH_SUCCESS(err, "mach_make_memory_entry()");
	T_QUIET; T_ASSERT_NE(memory_entry, MACH_PORT_NULL, "memory entry is non-null");

	T_LOG("Mapping memory entry");
	mach_vm_address_t addr;
	err = mach_vm_map(mach_task_self(), &addr, size, 0,
	    VM_FLAGS_ANYWHERE, memory_entry, 0, FALSE,
	    VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_NONE);
	T_QUIET; T_ASSERT_MACH_SUCCESS(err, "mach_vm_map()");

	T_LOG("Querying page counts");
	ret = mach_memory_entry_get_page_counts(memory_entry, &resident, &dirty, &swapped);
	T_QUIET; T_ASSERT_MACH_SUCCESS(ret, "mach_memory_entry_get_page_counts()");

	T_EXPECT_EQ(resident, 0ull, "Entry should have no resident pages");
	T_EXPECT_EQ(dirty, 0ull, "Entry should have no dirty pages");
	T_EXPECT_EQ(swapped, 0ull, "Entry should have no swapped pages");

	T_LOG("Faulting mapping");
	memset((void *)addr, 0xAB, size);

	T_LOG("Wiring mapping");
	ret = mlock((void *)addr, size);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "mlock()");

	T_LOG("Querying page counts");
	ret = mach_memory_entry_get_page_counts(memory_entry, &resident, &dirty, &swapped);
	T_QUIET; T_ASSERT_MACH_SUCCESS(ret, "mach_memory_entry_get_page_counts()");

	T_EXPECT_EQ(resident, pages, "Entry should have all resident pages");
	T_EXPECT_EQ(dirty, pages, "Entry should have all dirty pages");
	T_EXPECT_EQ(swapped, 0ull, "Entry should have no swapped pages");

	T_LOG("Un-wiring mapping");
	ret = munlock((void *)addr, size);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "munlock()");

	T_LOG("Evicting backing pages...");
	ret = madvise((void *)addr, size, MADV_PAGEOUT);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "madvise()");

	/* MADV_PAGEOUT is asynchronous */
	sleep(1);

	T_LOG("Querying page counts");
	ret = mach_memory_entry_get_page_counts(memory_entry, &resident, &dirty, &swapped);
	T_QUIET; T_ASSERT_MACH_SUCCESS(ret, "mach_memory_entry_get_page_counts()");

	T_EXPECT_EQ(resident, 0ull, "Entry should have no resident pages");
	T_EXPECT_EQ(dirty, 0ull, "Entry should have no dirty pages");
	T_EXPECT_EQ(swapped, pages, "Entry should have all swapped pages");

	err = mach_vm_deallocate(mach_task_self(), addr, size);
	T_QUIET; T_ASSERT_MACH_SUCCESS(err, "mach_vm_deallocate()");

	err = mach_port_deallocate(mach_task_self(), memory_entry);
	T_QUIET; T_ASSERT_MACH_SUCCESS(err, "mach_port_deallocate()");
}
