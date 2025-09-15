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

#include <arm/cpu_data_internal.h> // for BootArgs
#include <pexpert/pexpert.h>  // for PE_state
#include <pexpert/boot.h>  // for bootargs
#include <kern/startup.h>  // for kernel_startup_initialize_upto
#include <IOKit/IOLocks.h> // for IOLock
#include <vm/vm_page_internal.h> // for vm_set_page_size
#include <kern/clock.h> // for clock_config
#include <vm/pmap.h>

#include "std_safe.h"

// This define is supposed to come from the .CFLAGS parsing. if it's not, something is wrong with the Makefile
#ifndef __BUILDING_XNU_LIB_UNITTEST__
#error "not building unittest, something is wrong"
#endif


extern void kernel_startup_bootstrap(void);
extern void scale_setup(void);
extern void vm_mem_bootstrap(void);
extern void waitq_bootstrap(void);
// can't include IOKit/IOKitDebug.h since it's a C++ file
extern void IOTrackingInit(void);
extern void mock_mem_init_vm_objects(void);

extern lck_grp_t * IOLockGroup;
extern IOLock * sKextLoggingLock;
extern bitmap_t * asid_bitmap;
extern zone_t pmap_zone;

void
fake_pmap_init(void)
{
	pmap_zone = zone_create_ext("pmap", sizeof(struct pmap),
	    ZC_ZFREE_CLEARMEM, ZONE_ID_PMAP, NULL);

	static uint64_t asid_bits = 0;
	asid_bitmap = &asid_bits;
}


void
fake_init_bootargs(void)
{
	// see PE_boot_args()
	static boot_args ba;
	PE_state.bootArgs = &ba;
	PE_state.initialized = TRUE;
	BootArgs = &ba; // arm_init()
}

void
fake_kernel_bootstrap(void)
{
	mem_size = 0x0000000080000000ULL; // 2 GB
	max_mem = mem_size;
	scale_setup();

	vm_set_page_size(); // called from arm_init() -> arm_vm_init()
	vm_mem_bootstrap();
	fake_pmap_init();
	clock_config();
}


void
fake_iokit_init(void)
{
	// these are needed for static initializations in iokit to not crash
	IOLockGroup = lck_grp_alloc_init("IOKit", LCK_GRP_ATTR_NULL);
#if IOTRACKING
	IOTrackingInit();
#endif
	sKextLoggingLock = IOLockAlloc();
}

// This is the first function that is called before any initialization in libkernel.
// It's made to be first by the order of object files in the linker command line in Makefile
__attribute__((constructor)) void
fake_kinit(void)
{
	fake_init_bootargs();
	kernel_startup_bootstrap();
	fake_kernel_bootstrap();
	fake_iokit_init();

	kernel_startup_initialize_only(STARTUP_SUB_MACH_IPC);
	kernel_startup_initialize_only(STARTUP_SUB_SYSCTL);

	mock_mem_init_vm_objects();
}
