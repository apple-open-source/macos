/*
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
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
#include <darwintest.h>
#include <darwintest_mach.h>
#include <darwintest_posix.h>
#include <mach/kern_return.h>
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/mach_vm.h>
#include <mach/vm_page_size.h>
#include <mach/vm_param.h>
#include <sys/sysctl.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.mach.port_description"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("ipc"));

// kern/ipc_kobject.h
#define IKOT_NAMED_ENTRY 28

T_DECL(vm_named_entry,
    "test mach_port_kobject_description() on a named memory entry")
{
	kern_return_t kr;
	mach_vm_size_t size = vm_page_size;
	mach_port_t named_entry = MACH_PORT_NULL;
	natural_t object_type;
	mach_vm_address_t object_addr;
	kobject_description_t object_description;
	boolean_t dev_kern;
	size_t dev_kern_size = sizeof(dev_kern);
	int ret;

	ret = sysctlbyname("kern.development", &dev_kern, &dev_kern_size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctl(kern.development)");

	// Create a memory entry
	kr = mach_make_memory_entry_64(mach_task_self(), &size, 0ull,
	    MAP_MEM_NAMED_CREATE | VM_PROT_DEFAULT, &named_entry, MACH_PORT_NULL);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_make_memory_entry_64()");

	// Describe it
	kr = mach_port_kobject_description(mach_task_self(), named_entry,
	    &object_type, &object_addr, object_description);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_port_kobject_description()");

	T_LOG("Object Type: %d", object_type);
	T_EXPECT_EQ(object_type, IKOT_NAMED_ENTRY, "object has type IKOT_NAMED_ENTRY");

	T_LOG("Object Address: %llu", object_addr);
	if (dev_kern) {
		T_EXPECT_NE(object_addr, 0ull, "object address is populated on development kernel");
	} else {
		T_EXPECT_EQ(object_addr, 0ull, "object address is zero on release kernel");
	}

	T_LOG("Object Description: %s", object_description);
	T_EXPECT_NE_STR(object_description, "", "object description is populated");

	mach_port_deallocate(mach_task_self(), named_entry);
}
