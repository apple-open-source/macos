/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include <mach_assert.h>

#include <mach/mach_types.h>
#include <mach/memory_object.h>
#include <mach/vm_map.h>
#include <mach/vm32_map_server.h>
#include <mach/mach_host.h>
#include <mach/host_priv.h>

#include <kern/ledger.h>
#include <kern/host.h>

#include <device/device_port.h>
#include <vm/memory_object_internal.h>
#include <vm/vm_fault.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_object_internal.h>
#include <vm/vm_pageout_xnu.h>
#include <vm/vm_protos.h>
#include <vm/vm_memtag.h>
#include <vm/vm_memory_entry_xnu.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_iokit.h>
#include <vm/vm_page_internal.h>

#include <kern/zalloc.h>
#include <kern/zalloc_internal.h>

#include <mach/mach_vm.h>

#include <sys/errno.h> /* for the sysctl tests */

#include <tests/xnupost.h> /* for testing-related functions and macros */

extern ledger_template_t        task_ledger_template;

extern kern_return_t
vm_map_copy_adjust_to_target(
	vm_map_copy_t           copy_map,
	vm_map_offset_t         offset,
	vm_map_size_t           size,
	vm_map_t                target_map,
	boolean_t               copy,
	vm_map_copy_t           *target_copy_map_p,
	vm_map_offset_t         *overmap_start_p,
	vm_map_offset_t         *overmap_end_p,
	vm_map_offset_t         *trimmed_start_p);

#define VM_TEST_COLLAPSE_COMPRESSOR             0
#define VM_TEST_WIRE_AND_EXTRACT                0
#define VM_TEST_PAGE_WIRE_OVERFLOW_PANIC        0
#if __arm64__
#define VM_TEST_KERNEL_OBJECT_FAULT             0
#endif /* __arm64__ */
#define VM_TEST_DEVICE_PAGER_TRANSPOSE          (DEVELOPMENT || DEBUG)

#if VM_TEST_COLLAPSE_COMPRESSOR
extern boolean_t vm_object_collapse_compressor_allowed;
#include <IOKit/IOLib.h>
static void
vm_test_collapse_compressor(void)
{
	vm_object_size_t        backing_size, top_size;
	vm_object_t             backing_object, top_object;
	vm_map_offset_t         backing_offset, top_offset;
	unsigned char           *backing_address, *top_address;
	kern_return_t           kr;

	printf("VM_TEST_COLLAPSE_COMPRESSOR:\n");

	/* create backing object */
	backing_size = 15 * PAGE_SIZE;
	backing_object = vm_object_allocate(backing_size);
	assert(backing_object != VM_OBJECT_NULL);
	printf("VM_TEST_COLLAPSE_COMPRESSOR: created backing object %p\n",
	    backing_object);
	/* map backing object */
	backing_offset = 0;
	kr = vm_map_enter(kernel_map, &backing_offset, backing_size, 0,
	    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(),
	    backing_object, 0, FALSE,
	    VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
	backing_address = (unsigned char *) backing_offset;
	printf("VM_TEST_COLLAPSE_COMPRESSOR: "
	    "mapped backing object %p at 0x%llx\n",
	    backing_object, (uint64_t) backing_offset);
	/* populate with pages to be compressed in backing object */
	backing_address[0x1 * PAGE_SIZE] = 0xB1;
	backing_address[0x4 * PAGE_SIZE] = 0xB4;
	backing_address[0x7 * PAGE_SIZE] = 0xB7;
	backing_address[0xa * PAGE_SIZE] = 0xBA;
	backing_address[0xd * PAGE_SIZE] = 0xBD;
	printf("VM_TEST_COLLAPSE_COMPRESSOR: "
	    "populated pages to be compressed in "
	    "backing_object %p\n", backing_object);
	/* compress backing object */
	vm_object_pageout(backing_object);
	printf("VM_TEST_COLLAPSE_COMPRESSOR: compressing backing_object %p\n",
	    backing_object);
	/* wait for all the pages to be gone */
	while (*(volatile int *)&backing_object->resident_page_count != 0) {
		IODelay(10);
	}
	printf("VM_TEST_COLLAPSE_COMPRESSOR: backing_object %p compressed\n",
	    backing_object);
	/* populate with pages to be resident in backing object */
	backing_address[0x0 * PAGE_SIZE] = 0xB0;
	backing_address[0x3 * PAGE_SIZE] = 0xB3;
	backing_address[0x6 * PAGE_SIZE] = 0xB6;
	backing_address[0x9 * PAGE_SIZE] = 0xB9;
	backing_address[0xc * PAGE_SIZE] = 0xBC;
	printf("VM_TEST_COLLAPSE_COMPRESSOR: "
	    "populated pages to be resident in "
	    "backing_object %p\n", backing_object);
	/* leave the other pages absent */
	/* mess with the paging_offset of the backing_object */
	assert(backing_object->paging_offset == 0);
	backing_object->paging_offset = 3 * PAGE_SIZE;

	/* create top object */
	top_size = 9 * PAGE_SIZE;
	top_object = vm_object_allocate(top_size);
	assert(top_object != VM_OBJECT_NULL);
	printf("VM_TEST_COLLAPSE_COMPRESSOR: created top object %p\n",
	    top_object);
	/* map top object */
	top_offset = 0;
	kr = vm_map_enter(kernel_map, &top_offset, top_size, 0,
	    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(),
	    top_object, 0, FALSE,
	    VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
	top_address = (unsigned char *) top_offset;
	printf("VM_TEST_COLLAPSE_COMPRESSOR: "
	    "mapped top object %p at 0x%llx\n",
	    top_object, (uint64_t) top_offset);
	/* populate with pages to be compressed in top object */
	top_address[0x3 * PAGE_SIZE] = 0xA3;
	top_address[0x4 * PAGE_SIZE] = 0xA4;
	top_address[0x5 * PAGE_SIZE] = 0xA5;
	printf("VM_TEST_COLLAPSE_COMPRESSOR: "
	    "populated pages to be compressed in "
	    "top_object %p\n", top_object);
	/* compress top object */
	vm_object_pageout(top_object);
	printf("VM_TEST_COLLAPSE_COMPRESSOR: compressing top_object %p\n",
	    top_object);
	/* wait for all the pages to be gone */
	while (top_object->resident_page_count != 0) {
		IODelay(10);
	}
	printf("VM_TEST_COLLAPSE_COMPRESSOR: top_object %p compressed\n",
	    top_object);
	/* populate with pages to be resident in top object */
	top_address[0x0 * PAGE_SIZE] = 0xA0;
	top_address[0x1 * PAGE_SIZE] = 0xA1;
	top_address[0x2 * PAGE_SIZE] = 0xA2;
	printf("VM_TEST_COLLAPSE_COMPRESSOR: "
	    "populated pages to be resident in "
	    "top_object %p\n", top_object);
	/* leave the other pages absent */

	/* link the 2 objects */
	vm_object_reference(backing_object);
	top_object->shadow = backing_object;
	top_object->vo_shadow_offset = 3 * PAGE_SIZE;
	printf("VM_TEST_COLLAPSE_COMPRESSOR: linked %p and %p\n",
	    top_object, backing_object);

	/* unmap backing object */
	vm_map_remove(kernel_map,
	    backing_offset,
	    backing_offset + backing_size,
	    VM_MAP_REMOVE_NO_FLAGS);
	printf("VM_TEST_COLLAPSE_COMPRESSOR: "
	    "unmapped backing_object %p [0x%llx:0x%llx]\n",
	    backing_object,
	    (uint64_t) backing_offset,
	    (uint64_t) (backing_offset + backing_size));

	/* collapse */
	printf("VM_TEST_COLLAPSE_COMPRESSOR: collapsing %p\n", top_object);
	vm_object_lock(top_object);
	vm_object_collapse(top_object, 0, FALSE);
	vm_object_unlock(top_object);
	printf("VM_TEST_COLLAPSE_COMPRESSOR: collapsed %p\n", top_object);

	/* did it work? */
	if (top_object->shadow != VM_OBJECT_NULL) {
		printf("VM_TEST_COLLAPSE_COMPRESSOR: not collapsed\n");
		printf("VM_TEST_COLLAPSE_COMPRESSOR: FAIL\n");
		if (vm_object_collapse_compressor_allowed) {
			panic("VM_TEST_COLLAPSE_COMPRESSOR: FAIL");
		}
	} else {
		/* check the contents of the mapping */
		unsigned char expect[9] =
		{ 0xA0, 0xA1, 0xA2,             /* resident in top */
		  0xA3, 0xA4, 0xA5,             /* compressed in top */
		  0xB9,         /* resident in backing + shadow_offset */
		  0xBD,         /* compressed in backing + shadow_offset + paging_offset */
		  0x00 };                       /* absent in both */
		unsigned char actual[9];
		unsigned int i, errors;

		errors = 0;
		for (i = 0; i < sizeof(actual); i++) {
			actual[i] = (unsigned char) top_address[i * PAGE_SIZE];
			if (actual[i] != expect[i]) {
				errors++;
			}
		}
		printf("VM_TEST_COLLAPSE_COMPRESSOR: "
		    "actual [%x %x %x %x %x %x %x %x %x] "
		    "expect [%x %x %x %x %x %x %x %x %x] "
		    "%d errors\n",
		    actual[0], actual[1], actual[2], actual[3],
		    actual[4], actual[5], actual[6], actual[7],
		    actual[8],
		    expect[0], expect[1], expect[2], expect[3],
		    expect[4], expect[5], expect[6], expect[7],
		    expect[8],
		    errors);
		if (errors) {
			panic("VM_TEST_COLLAPSE_COMPRESSOR: FAIL");
		} else {
			printf("VM_TEST_COLLAPSE_COMPRESSOR: PASS\n");
		}
	}
}
#else /* VM_TEST_COLLAPSE_COMPRESSOR */
#define vm_test_collapse_compressor()
#endif /* VM_TEST_COLLAPSE_COMPRESSOR */

#if VM_TEST_WIRE_AND_EXTRACT
extern ppnum_t vm_map_get_phys_page(vm_map_t map,
    vm_offset_t offset);
static void
vm_test_wire_and_extract(void)
{
	ledger_t                ledger;
	vm_map_t                user_map, wire_map;
	mach_vm_address_t       user_addr, wire_addr;
	mach_vm_size_t          user_size, wire_size;
	mach_vm_offset_t        cur_offset;
	vm_prot_t               cur_prot, max_prot;
	ppnum_t                 user_ppnum, wire_ppnum;
	kern_return_t           kr;

	ledger = ledger_instantiate(task_ledger_template,
	    LEDGER_CREATE_ACTIVE_ENTRIES);
	pmap_t user_pmap = pmap_create_options(ledger, 0, PMAP_CREATE_64BIT);
	assert(user_pmap);
	user_map = vm_map_create_options(user_pmap,
	    0x100000000ULL,
	    0x200000000ULL,
	    VM_MAP_CREATE_PAGEABLE);
	wire_map = vm_map_create_options(NULL,
	    0x100000000ULL,
	    0x200000000ULL,
	    VM_MAP_CREATE_PAGEABLE);
	user_addr = 0;
	user_size = 0x10000;
	kr = mach_vm_allocate(user_map,
	    &user_addr,
	    user_size,
	    VM_FLAGS_ANYWHERE);
	assert(kr == KERN_SUCCESS);
	wire_addr = 0;
	wire_size = user_size;
	kr = mach_vm_remap(wire_map,
	    &wire_addr,
	    wire_size,
	    0,
	    VM_FLAGS_ANYWHERE,
	    user_map,
	    user_addr,
	    FALSE,
	    &cur_prot,
	    &max_prot,
	    VM_INHERIT_NONE);
	assert(kr == KERN_SUCCESS);
	for (cur_offset = 0;
	    cur_offset < wire_size;
	    cur_offset += PAGE_SIZE) {
		kr = vm_map_wire_and_extract(wire_map,
		    wire_addr + cur_offset,
		    VM_PROT_DEFAULT | VM_PROT_MEMORY_TAG_MAKE(VM_KERN_MEMORY_OSFMK),
		    TRUE,
		    &wire_ppnum);
		assert(kr == KERN_SUCCESS);
		user_ppnum = vm_map_get_phys_page(user_map,
		    user_addr + cur_offset);
		printf("VM_TEST_WIRE_AND_EXTRACT: kr=0x%x "
		    "user[%p:0x%llx:0x%x] wire[%p:0x%llx:0x%x]\n",
		    kr,
		    user_map, user_addr + cur_offset, user_ppnum,
		    wire_map, wire_addr + cur_offset, wire_ppnum);
		if (kr != KERN_SUCCESS ||
		    wire_ppnum == 0 ||
		    wire_ppnum != user_ppnum) {
			panic("VM_TEST_WIRE_AND_EXTRACT: FAIL");
		}
	}
	cur_offset -= PAGE_SIZE;
	kr = vm_map_wire_and_extract(wire_map,
	    wire_addr + cur_offset,
	    VM_PROT_DEFAULT,
	    TRUE,
	    &wire_ppnum);
	assert(kr == KERN_SUCCESS);
	printf("VM_TEST_WIRE_AND_EXTRACT: re-wire kr=0x%x "
	    "user[%p:0x%llx:0x%x] wire[%p:0x%llx:0x%x]\n",
	    kr,
	    user_map, user_addr + cur_offset, user_ppnum,
	    wire_map, wire_addr + cur_offset, wire_ppnum);
	if (kr != KERN_SUCCESS ||
	    wire_ppnum == 0 ||
	    wire_ppnum != user_ppnum) {
		panic("VM_TEST_WIRE_AND_EXTRACT: FAIL");
	}

	printf("VM_TEST_WIRE_AND_EXTRACT: PASS\n");
}
#else /* VM_TEST_WIRE_AND_EXTRACT */
#define vm_test_wire_and_extract()
#endif /* VM_TEST_WIRE_AND_EXTRACT */

#if VM_TEST_PAGE_WIRE_OVERFLOW_PANIC
static void
vm_test_page_wire_overflow_panic(void)
{
	vm_object_t object;
	vm_page_t page;

	printf("VM_TEST_PAGE_WIRE_OVERFLOW_PANIC: starting...\n");

	object = vm_object_allocate(PAGE_SIZE);
	vm_object_lock(object);
	page = vm_page_alloc(object, 0x0);
	vm_page_lock_queues();
	do {
		vm_page_wire(page, 1, FALSE);
	} while (page->wire_count != 0);
	vm_page_unlock_queues();
	vm_object_unlock(object);
	panic("FBDP(%p,%p): wire_count overflow not detected",
	    object, page);
}
#else /* VM_TEST_PAGE_WIRE_OVERFLOW_PANIC */
#define vm_test_page_wire_overflow_panic()
#endif /* VM_TEST_PAGE_WIRE_OVERFLOW_PANIC */

#if __arm64__ && VM_TEST_KERNEL_OBJECT_FAULT
extern int copyinframe(vm_address_t fp, char *frame, boolean_t is64bit);
static void
vm_test_kernel_object_fault(void)
{
	vm_offset_t stack;
	uintptr_t frameb[2];
	int ret;

	kmem_alloc(kernel_map, &stack,
	    kernel_stack_size + ptoa(2),
	    KMA_NOFAIL | KMA_KSTACK | KMA_KOBJECT |
	    KMA_GUARD_FIRST | KMA_GUARD_LAST,
	    VM_KERN_MEMORY_STACK);

	ret = copyinframe((uintptr_t)stack, (char *)frameb, TRUE);
	if (ret != 0) {
		printf("VM_TEST_KERNEL_OBJECT_FAULT: PASS\n");
	} else {
		printf("VM_TEST_KERNEL_OBJECT_FAULT: FAIL\n");
	}

	kmem_free(kernel_map, stack, kernel_stack_size + ptoa(2));
	stack = 0;
}
#else /* __arm64__ && VM_TEST_KERNEL_OBJECT_FAULT */
#define vm_test_kernel_object_fault()
#endif /* __arm64__ && VM_TEST_KERNEL_OBJECT_FAULT */

#if VM_TEST_DEVICE_PAGER_TRANSPOSE
static void
vm_test_device_pager_transpose(void)
{
	memory_object_t device_pager;
	vm_object_t     anon_object, device_object;
	vm_size_t       size;
	vm_map_offset_t device_mapping;
	kern_return_t   kr;

	size = 3 * PAGE_SIZE;
	anon_object = vm_object_allocate(size);
	assert(anon_object != VM_OBJECT_NULL);
	device_pager = device_pager_setup(NULL, 0, size, 0);
	assert(device_pager != NULL);
	device_object = memory_object_to_vm_object(device_pager);
	assert(device_object != VM_OBJECT_NULL);
#if 0
	/*
	 * Can't actually map this, since another thread might do a
	 * vm_map_enter() that gets coalesced into this object, which
	 * would cause the test to fail.
	 */
	vm_map_offset_t anon_mapping = 0;
	kr = vm_map_enter(kernel_map, &anon_mapping, size, 0,
	    VM_MAP_KERNEL_FLAGS_ANYWHERE(),
	    anon_object, 0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
	    VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
#endif
	device_mapping = 0;
	kr = mach_vm_map_kernel(kernel_map,
	    vm_sanitize_wrap_addr_ref(&device_mapping),
	    size,
	    0,
	    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(),
	    (void *)device_pager,
	    0,
	    FALSE,
	    VM_PROT_DEFAULT,
	    VM_PROT_ALL,
	    VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
	memory_object_deallocate(device_pager);

	vm_object_lock(anon_object);
	vm_object_activity_begin(anon_object);
	anon_object->blocked_access = TRUE;
	vm_object_unlock(anon_object);
	vm_object_lock(device_object);
	vm_object_activity_begin(device_object);
	device_object->blocked_access = TRUE;
	vm_object_unlock(device_object);

	assert(anon_object->ref_count == 1);
	assert(!anon_object->named);
	assert(device_object->ref_count == 2);
	assert(device_object->named);

	kr = vm_object_transpose(device_object, anon_object, size);
	assert(kr == KERN_SUCCESS);

	vm_object_lock(anon_object);
	vm_object_activity_end(anon_object);
	anon_object->blocked_access = FALSE;
	vm_object_unlock(anon_object);
	vm_object_lock(device_object);
	vm_object_activity_end(device_object);
	device_object->blocked_access = FALSE;
	vm_object_unlock(device_object);

	assert(anon_object->ref_count == 2);
	assert(anon_object->named);
#if 0
	kr = vm_deallocate(kernel_map, anon_mapping, size);
	assert(kr == KERN_SUCCESS);
#endif
	assert(device_object->ref_count == 1);
	assert(!device_object->named);
	kr = vm_deallocate(kernel_map, device_mapping, size);
	assert(kr == KERN_SUCCESS);

	printf("VM_TEST_DEVICE_PAGER_TRANSPOSE: PASS\n");
}
#else /* VM_TEST_DEVICE_PAGER_TRANSPOSE */
#define vm_test_device_pager_transpose()
#endif /* VM_TEST_DEVICE_PAGER_TRANSPOSE */

extern kern_return_t vm_allocate_external(vm_map_t        map,
    vm_offset_t     *addr,
    vm_size_t       size,
    int             flags);
extern kern_return_t vm_remap_external(vm_map_t                target_map,
    vm_offset_t             *address,
    vm_size_t               size,
    vm_offset_t             mask,
    int                     flags,
    vm_map_t                src_map,
    vm_offset_t             memory_address,
    boolean_t               copy,
    vm_prot_t               *cur_protection,
    vm_prot_t               *max_protection,
    vm_inherit_t            inheritance);
#if PMAP_CREATE_FORCE_4K_PAGES && MACH_ASSERT
extern int debug4k_panic_on_misaligned_sharing;
void vm_test_4k(void);
void
vm_test_4k(void)
{
	pmap_t test_pmap;
	vm_map_t test_map;
	kern_return_t kr;
	vm_address_t expected_addr;
	vm_address_t alloc1_addr, alloc2_addr, alloc3_addr, alloc4_addr;
	vm_address_t alloc5_addr, dealloc_addr, remap_src_addr, remap_dst_addr;
	vm_size_t alloc1_size, alloc2_size, alloc3_size, alloc4_size;
	vm_size_t alloc5_size, remap_src_size;
	vm_address_t fault_addr;
	vm_prot_t cur_prot, max_prot;
	int saved_debug4k_panic_on_misaligned_sharing;

	printf("\n\n\nVM_TEST_4K:%d creating 4K map...\n", __LINE__);
	test_pmap = pmap_create_options(NULL, 0, PMAP_CREATE_64BIT | PMAP_CREATE_FORCE_4K_PAGES);
	assert(test_pmap != NULL);
	test_map = vm_map_create_options(test_pmap,
	    MACH_VM_MIN_ADDRESS,
	    MACH_VM_MAX_ADDRESS,
	    VM_MAP_CREATE_PAGEABLE);
	assert(test_map != VM_MAP_NULL);
	vm_map_set_page_shift(test_map, FOURK_PAGE_SHIFT);
	printf("VM_TEST_4K:%d map %p pmap %p page_size 0x%x\n", __LINE__, test_map, test_pmap, VM_MAP_PAGE_SIZE(test_map));

	alloc1_addr = 0;
	alloc1_size = 1 * FOURK_PAGE_SIZE;
	expected_addr = 0x1000;
	printf("VM_TEST_4K:%d vm_allocate(%p, 0x%lx, 0x%lx)...\n", __LINE__, test_map, alloc1_addr, alloc1_size);
	kr = vm_allocate_external(test_map,
	    &alloc1_addr,
	    alloc1_size,
	    VM_FLAGS_ANYWHERE);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	assertf(alloc1_addr == expected_addr, "alloc1_addr = 0x%lx expected 0x%lx", alloc1_addr, expected_addr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, alloc1_addr);
	expected_addr += alloc1_size;

	printf("VM_TEST_4K:%d vm_deallocate(%p, 0x%lx, 0x%lx)...\n", __LINE__, test_map, alloc1_addr, alloc1_size);
	kr = vm_deallocate(test_map, alloc1_addr, alloc1_size);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, alloc1_addr);

	alloc1_addr = 0;
	alloc1_size = 1 * FOURK_PAGE_SIZE;
	expected_addr = 0x1000;
	printf("VM_TEST_4K:%d vm_allocate(%p, 0x%lx, 0x%lx)...\n", __LINE__, test_map, alloc1_addr, alloc1_size);
	kr = vm_allocate_external(test_map,
	    &alloc1_addr,
	    alloc1_size,
	    VM_FLAGS_ANYWHERE);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	assertf(alloc1_addr == expected_addr, "alloc1_addr = 0x%lx expected 0x%lx", alloc1_addr, expected_addr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, alloc1_addr);
	expected_addr += alloc1_size;

	alloc2_addr = 0;
	alloc2_size = 3 * FOURK_PAGE_SIZE;
	printf("VM_TEST_4K:%d vm_allocate(%p, 0x%lx, 0x%lx)...\n", __LINE__, test_map, alloc2_addr, alloc2_size);
	kr = vm_allocate_external(test_map,
	    &alloc2_addr,
	    alloc2_size,
	    VM_FLAGS_ANYWHERE);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	assertf(alloc2_addr == expected_addr, "alloc2_addr = 0x%lx expected 0x%lx", alloc2_addr, expected_addr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, alloc2_addr);
	expected_addr += alloc2_size;

	alloc3_addr = 0;
	alloc3_size = 18 * FOURK_PAGE_SIZE;
	printf("VM_TEST_4K:%d vm_allocate(%p, 0x%lx, 0x%lx)...\n", __LINE__, test_map, alloc3_addr, alloc3_size);
	kr = vm_allocate_external(test_map,
	    &alloc3_addr,
	    alloc3_size,
	    VM_FLAGS_ANYWHERE);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	assertf(alloc3_addr == expected_addr, "alloc3_addr = 0x%lx expected 0x%lx\n", alloc3_addr, expected_addr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, alloc3_addr);
	expected_addr += alloc3_size;

	alloc4_addr = 0;
	alloc4_size = 1 * FOURK_PAGE_SIZE;
	printf("VM_TEST_4K:%d vm_allocate(%p, 0x%lx, 0x%lx)...\n", __LINE__, test_map, alloc4_addr, alloc4_size);
	kr = vm_allocate_external(test_map,
	    &alloc4_addr,
	    alloc4_size,
	    VM_FLAGS_ANYWHERE);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	assertf(alloc4_addr == expected_addr, "alloc4_addr = 0x%lx expected 0x%lx", alloc4_addr, expected_addr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, alloc3_addr);
	expected_addr += alloc4_size;

	printf("VM_TEST_4K:%d vm_protect(%p, 0x%lx, 0x%lx, READ)...\n", __LINE__, test_map, alloc2_addr, (1UL * FOURK_PAGE_SIZE));
	kr = vm_protect(test_map,
	    alloc2_addr,
	    (1UL * FOURK_PAGE_SIZE),
	    FALSE,
	    VM_PROT_READ);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);

	for (fault_addr = alloc1_addr;
	    fault_addr < alloc4_addr + alloc4_size + (2 * FOURK_PAGE_SIZE);
	    fault_addr += FOURK_PAGE_SIZE) {
		printf("VM_TEST_4K:%d write fault at 0x%lx...\n", __LINE__, fault_addr);
		kr = vm_fault(test_map,
		    fault_addr,
		    VM_PROT_WRITE,
		    FALSE,
		    VM_KERN_MEMORY_NONE,
		    THREAD_UNINT,
		    NULL,
		    0);
		printf("VM_TEST_4K:%d -> 0x%x\n", __LINE__, kr);
		if (fault_addr == alloc2_addr) {
			assertf(kr == KERN_PROTECTION_FAILURE, "fault_addr = 0x%lx kr = 0x%x expected 0x%x", fault_addr, kr, KERN_PROTECTION_FAILURE);
			printf("VM_TEST_4K:%d read fault at 0x%lx...\n", __LINE__, fault_addr);
			kr = vm_fault(test_map,
			    fault_addr,
			    VM_PROT_READ,
			    FALSE,
			    VM_KERN_MEMORY_NONE,
			    THREAD_UNINT,
			    NULL,
			    0);
			assertf(kr == KERN_SUCCESS, "fault_addr = 0x%lx kr = 0x%x expected 0x%x", fault_addr, kr, KERN_SUCCESS);
			printf("VM_TEST_4K:%d -> 0x%x\n", __LINE__, kr);
		} else if (fault_addr >= alloc4_addr + alloc4_size) {
			assertf(kr == KERN_INVALID_ADDRESS, "fault_addr = 0x%lx kr = 0x%x expected 0x%x", fault_addr, kr, KERN_INVALID_ADDRESS);
		} else {
			assertf(kr == KERN_SUCCESS, "fault_addr = 0x%lx kr = 0x%x expected 0x%x", fault_addr, kr, KERN_SUCCESS);
		}
	}

	alloc5_addr = 0;
	alloc5_size = 7 * FOURK_PAGE_SIZE;
	printf("VM_TEST_4K:%d vm_allocate(%p, 0x%lx, 0x%lx)...\n", __LINE__, test_map, alloc5_addr, alloc5_size);
	kr = vm_allocate_external(test_map,
	    &alloc5_addr,
	    alloc5_size,
	    VM_FLAGS_ANYWHERE);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	assertf(alloc5_addr == expected_addr, "alloc5_addr = 0x%lx expected 0x%lx", alloc5_addr, expected_addr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, alloc5_addr);
	expected_addr += alloc5_size;

	dealloc_addr = vm_map_round_page(alloc5_addr, PAGE_SHIFT);
	dealloc_addr += FOURK_PAGE_SIZE;
	printf("VM_TEST_4K:%d vm_deallocate(%p, 0x%lx, 0x%x)...\n", __LINE__, test_map, dealloc_addr, FOURK_PAGE_SIZE);
	kr = vm_deallocate(test_map, dealloc_addr, FOURK_PAGE_SIZE);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	printf("VM_TEST_4K:%d -> 0x%x\n", __LINE__, kr);

	remap_src_addr = vm_map_round_page(alloc3_addr, PAGE_SHIFT);
	remap_src_addr += FOURK_PAGE_SIZE;
	remap_src_size = 2 * FOURK_PAGE_SIZE;
	remap_dst_addr = 0;
	printf("VM_TEST_4K:%d vm_remap(%p, 0x%lx, 0x%lx, 0x%lx, copy=0)...\n", __LINE__, test_map, remap_dst_addr, remap_src_size, remap_src_addr);
	kr = vm_remap_external(test_map,
	    &remap_dst_addr,
	    remap_src_size,
	    0,                    /* mask */
	    VM_FLAGS_ANYWHERE,
	    test_map,
	    remap_src_addr,
	    FALSE,                    /* copy */
	    &cur_prot,
	    &max_prot,
	    VM_INHERIT_DEFAULT);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	assertf(remap_dst_addr == expected_addr, "remap_dst_addr = 0x%lx expected 0x%lx", remap_dst_addr, expected_addr);
	printf("VM_TEST_4K:%d -> 0x%lx\n", __LINE__, remap_dst_addr);
	expected_addr += remap_src_size;

	for (fault_addr = remap_dst_addr;
	    fault_addr < remap_dst_addr + remap_src_size;
	    fault_addr += 4096) {
		printf("VM_TEST_4K:%d write fault at 0x%lx...\n", __LINE__, fault_addr);
		kr = vm_fault(test_map,
		    fault_addr,
		    VM_PROT_WRITE,
		    FALSE,
		    VM_KERN_MEMORY_NONE,
		    THREAD_UNINT,
		    NULL,
		    0);
		assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
		printf("VM_TEST_4K:%d -> 0x%x\n", __LINE__, kr);
	}

	printf("VM_TEST_4K:\n");
	remap_dst_addr = 0;
	remap_src_addr = alloc3_addr + 0xc000;
	remap_src_size = 0x5000;
	printf("VM_TEST_4K: vm_remap(%p, 0x%lx, 0x%lx, %p, copy=0) from 4K to 16K\n", test_map, remap_src_addr, remap_src_size, kernel_map);
	kr = vm_remap_external(kernel_map,
	    &remap_dst_addr,
	    remap_src_size,
	    0,                    /* mask */
	    VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR,
	    test_map,
	    remap_src_addr,
	    FALSE,                    /* copy */
	    &cur_prot,
	    &max_prot,
	    VM_INHERIT_DEFAULT);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	printf("VM_TEST_4K: -> remapped (shared) in map %p at addr 0x%lx\n", kernel_map, remap_dst_addr);

	printf("VM_TEST_4K:\n");
	remap_dst_addr = 0;
	remap_src_addr = alloc3_addr + 0xc000;
	remap_src_size = 0x5000;
	printf("VM_TEST_4K: vm_remap(%p, 0x%lx, 0x%lx, %p, copy=1) from 4K to 16K\n", test_map, remap_src_addr, remap_src_size, kernel_map);
	kr = vm_remap_external(kernel_map,
	    &remap_dst_addr,
	    remap_src_size,
	    0,                    /* mask */
	    VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR,
	    test_map,
	    remap_src_addr,
	    TRUE,                    /* copy */
	    &cur_prot,
	    &max_prot,
	    VM_INHERIT_DEFAULT);
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	printf("VM_TEST_4K: -> remapped (COW) in map %p at addr 0x%lx\n", kernel_map, remap_dst_addr);

	printf("VM_TEST_4K:\n");
	saved_debug4k_panic_on_misaligned_sharing = debug4k_panic_on_misaligned_sharing;
	debug4k_panic_on_misaligned_sharing = 0;
	remap_dst_addr = 0;
	remap_src_addr = alloc1_addr;
	remap_src_size = alloc1_size + alloc2_size;
	printf("VM_TEST_4K: vm_remap(%p, 0x%lx, 0x%lx, %p, copy=0) from 4K to 16K\n", test_map, remap_src_addr, remap_src_size, kernel_map);
	kr = vm_remap_external(kernel_map,
	    &remap_dst_addr,
	    remap_src_size,
	    0,                    /* mask */
	    VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR,
	    test_map,
	    remap_src_addr,
	    FALSE,                    /* copy */
	    &cur_prot,
	    &max_prot,
	    VM_INHERIT_DEFAULT);
	assertf(kr != KERN_SUCCESS, "kr = 0x%x", kr);
	printf("VM_TEST_4K: -> remap (SHARED) in map %p at addr 0x%lx kr=0x%x\n", kernel_map, remap_dst_addr, kr);
	debug4k_panic_on_misaligned_sharing = saved_debug4k_panic_on_misaligned_sharing;

	printf("VM_TEST_4K:\n");
	remap_dst_addr = 0;
	remap_src_addr = alloc1_addr;
	remap_src_size = alloc1_size + alloc2_size;
	printf("VM_TEST_4K: vm_remap(%p, 0x%lx, 0x%lx, %p, copy=1) from 4K to 16K\n", test_map, remap_src_addr, remap_src_size, kernel_map);
	kr = vm_remap_external(kernel_map,
	    &remap_dst_addr,
	    remap_src_size,
	    0,                    /* mask */
	    VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR,
	    test_map,
	    remap_src_addr,
	    TRUE,                    /* copy */
	    &cur_prot,
	    &max_prot,
	    VM_INHERIT_DEFAULT);
#if 000
	assertf(kr != KERN_SUCCESS, "kr = 0x%x", kr);
	printf("VM_TEST_4K: -> remap (COPY) in map %p at addr 0x%lx kr=0x%x\n", kernel_map, remap_dst_addr, kr);
#else /* 000 */
	assertf(kr == KERN_SUCCESS, "kr = 0x%x", kr);
	printf("VM_TEST_4K: -> remap (COPY) in map %p at addr 0x%lx kr=0x%x\n", kernel_map, remap_dst_addr, kr);
#endif /* 000 */


#if 00
	printf("VM_TEST_4K:%d vm_map_remove(%p, 0x%llx, 0x%llx)...\n", __LINE__, test_map, test_map->min_offset, test_map->max_offset);
	vm_map_remove(test_map, test_map->min_offset, test_map->max_offset);
#endif

	printf("VM_TEST_4K: PASS\n\n\n\n");
}
#endif /* PMAP_CREATE_FORCE_4K_PAGES && MACH_ASSERT */

#if MACH_ASSERT
static void
vm_test_map_copy_adjust_to_target_one(
	vm_map_copy_t copy_map,
	vm_map_t target_map)
{
	kern_return_t kr;
	vm_map_copy_t target_copy;
	vm_map_offset_t overmap_start, overmap_end, trimmed_start;

	target_copy = VM_MAP_COPY_NULL;
	/* size is 2 (4k) pages but range covers 3 pages */
	kr = vm_map_copy_adjust_to_target(copy_map,
	    0x0 + 0xfff,
	    0x1002,
	    target_map,
	    FALSE,
	    &target_copy,
	    &overmap_start,
	    &overmap_end,
	    &trimmed_start);
	assert(kr == KERN_SUCCESS);
	assert(overmap_start == 0);
	assert(overmap_end == 0);
	assert(trimmed_start == 0);
	assertf(target_copy->size == 0x3000,
	    "target_copy %p size 0x%llx\n",
	    target_copy, (uint64_t)target_copy->size);
	vm_map_copy_discard(target_copy);

	/* 1. adjust_to_target() for bad offset -> error */
	/* 2. adjust_to_target() for bad size -> error */
	/* 3. adjust_to_target() for the whole thing -> unchanged */
	/* 4. adjust_to_target() to trim start by less than 1 page */
	/* 5. adjust_to_target() to trim end by less than 1 page */
	/* 6. adjust_to_target() to trim start and end by less than 1 page */
	/* 7. adjust_to_target() to trim start by more than 1 page */
	/* 8. adjust_to_target() to trim end by more than 1 page */
	/* 9. adjust_to_target() to trim start and end by more than 1 page */
	/* 10. adjust_to_target() to trim start by more than 1 entry */
	/* 11. adjust_to_target() to trim start by more than 1 entry */
	/* 12. adjust_to_target() to trim start and end by more than 1 entry */
	/* 13. adjust_to_target() to trim start and end down to 1 entry */
}

static void
vm_test_map_copy_adjust_to_target(void)
{
	kern_return_t kr;
	vm_map_t map4k, map16k;
	vm_object_t obj1, obj2, obj3, obj4;
	vm_map_offset_t addr4k, addr16k;
	vm_map_size_t size4k, size16k;
	vm_map_copy_t copy4k, copy16k;
	vm_prot_t curprot, maxprot;

	/* create a 4k map */
	map4k = vm_map_create_options(PMAP_NULL, 0, (uint32_t)-1,
	    VM_MAP_CREATE_PAGEABLE);
	vm_map_set_page_shift(map4k, 12);

	/* create a 16k map */
	map16k = vm_map_create_options(PMAP_NULL, 0, (uint32_t)-1,
	    VM_MAP_CREATE_PAGEABLE);
	vm_map_set_page_shift(map16k, 14);

	/* create 4 VM objects */
	obj1 = vm_object_allocate(0x100000);
	obj2 = vm_object_allocate(0x100000);
	obj3 = vm_object_allocate(0x100000);
	obj4 = vm_object_allocate(0x100000);

	/* map objects in 4k map */
	vm_object_reference(obj1);
	addr4k = 0x1000;
	size4k = 0x3000;
	kr = vm_map_enter(map4k, &addr4k, size4k, 0,
	    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(), obj1, 0,
	    FALSE, VM_PROT_DEFAULT, VM_PROT_DEFAULT,
	    VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
	assert(addr4k == 0x1000);

	/* map objects in 16k map */
	vm_object_reference(obj1);
	addr16k = 0x4000;
	size16k = 0x8000;
	kr = vm_map_enter(map16k, &addr16k, size16k, 0,
	    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(), obj1, 0,
	    FALSE, VM_PROT_DEFAULT, VM_PROT_DEFAULT,
	    VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
	assert(addr16k == 0x4000);

	/* test for <rdar://60959809> */
	ipc_port_t mem_entry;
	memory_object_size_t mem_entry_size;
	mach_vm_size_t map_size;
	mem_entry_size = 0x1002;
	mem_entry = IPC_PORT_NULL;
	kr = mach_make_memory_entry_64(map16k, &mem_entry_size, addr16k + 0x2fff,
	    MAP_MEM_VM_SHARE | MAP_MEM_USE_DATA_ADDR | VM_PROT_READ,
	    &mem_entry, IPC_PORT_NULL);
	assertf(kr == KERN_SUCCESS, "kr 0x%x\n", kr);
	assertf(mem_entry_size == 0x5001, "mem_entry_size 0x%llx\n", (uint64_t) mem_entry_size);
	map_size = 0;
	kr = mach_memory_entry_map_size(mem_entry, map4k, 0, 0x1002, &map_size);
	assertf(kr == KERN_SUCCESS, "kr 0x%x\n", kr);
	assertf(map_size == 0x3000, "mem_entry %p map_size 0x%llx\n", mem_entry, (uint64_t)map_size);
	mach_memory_entry_port_release(mem_entry);

	/* create 4k copy map */
	curprot = VM_PROT_NONE;
	maxprot = VM_PROT_NONE;
	kr = vm_map_copy_extract(map4k, addr4k, 0x3000,
	    FALSE, &copy4k, &curprot, &maxprot,
	    VM_INHERIT_DEFAULT, VM_MAP_KERNEL_FLAGS_NONE);
	assert(kr == KERN_SUCCESS);
	assert(copy4k->size == 0x3000);

	/* create 16k copy map */
	curprot = VM_PROT_NONE;
	maxprot = VM_PROT_NONE;
	kr = vm_map_copy_extract(map16k, addr16k, 0x4000,
	    FALSE, &copy16k, &curprot, &maxprot,
	    VM_INHERIT_DEFAULT, VM_MAP_KERNEL_FLAGS_NONE);
	assert(kr == KERN_SUCCESS);
	assert(copy16k->size == 0x4000);

	/* test each combination */
//	vm_test_map_copy_adjust_to_target_one(copy4k, map4k);
//	vm_test_map_copy_adjust_to_target_one(copy16k, map16k);
//	vm_test_map_copy_adjust_to_target_one(copy4k, map16k);
	vm_test_map_copy_adjust_to_target_one(copy16k, map4k);

	/* assert 1 ref on 4k map */
	assert(os_ref_get_count_raw(&map4k->map_refcnt) == 1);
	/* release 4k map */
	vm_map_deallocate(map4k);
	/* assert 1 ref on 16k map */
	assert(os_ref_get_count_raw(&map16k->map_refcnt) == 1);
	/* release 16k map */
	vm_map_deallocate(map16k);
	/* deallocate copy maps */
	vm_map_copy_discard(copy4k);
	vm_map_copy_discard(copy16k);
	/* assert 1 ref on all VM objects */
	assert(obj1->ref_count == 1);
	assert(obj2->ref_count == 1);
	assert(obj3->ref_count == 1);
	assert(obj4->ref_count == 1);
	/* release all VM objects */
	vm_object_deallocate(obj1);
	vm_object_deallocate(obj2);
	vm_object_deallocate(obj3);
	vm_object_deallocate(obj4);
}
#endif /* MACH_ASSERT */

#if __arm64__ && !KASAN
__attribute__((noinline))
static void
vm_test_per_mapping_internal_accounting(void)
{
	ledger_t ledger;
	pmap_t user_pmap;
	vm_map_t user_map;
	kern_return_t kr;
	ledger_amount_t balance;
	mach_vm_address_t user_addr, user_remap;
	vm_map_offset_t device_addr;
	mach_vm_size_t user_size;
	vm_prot_t cur_prot, max_prot;
	upl_size_t upl_size;
	upl_t upl;
	unsigned int upl_count;
	upl_control_flags_t upl_flags;
	upl_page_info_t *pl;
	ppnum_t ppnum;
	vm_object_t device_object;
	vm_map_offset_t map_start, map_end;
	int pmap_flags;

	pmap_flags = 0;
	if (sizeof(vm_map_offset_t) == 4) {
		map_start = 0x100000000ULL;
		map_end = 0x200000000ULL;
		pmap_flags |= PMAP_CREATE_64BIT;
	} else {
		map_start = 0x10000000;
		map_end = 0x20000000;
	}
	/* create a user address space */
	ledger = ledger_instantiate(task_ledger_template,
	    LEDGER_CREATE_ACTIVE_ENTRIES);
	assert(ledger);
	user_pmap = pmap_create_options(ledger, 0, pmap_flags);
	assert(user_pmap);
	user_map = vm_map_create(user_pmap,
	    map_start,
	    map_end,
	    TRUE);
	assert(user_map);
	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == 0, "balance=0x%llx", balance);
	/* allocate 1 page in that address space */
	user_addr = 0;
	user_size = PAGE_SIZE;
	kr = mach_vm_allocate(user_map,
	    &user_addr,
	    user_size,
	    VM_FLAGS_ANYWHERE);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == 0, "balance=0x%llx", balance);
	/* remap the original mapping */
	user_remap = 0;
	kr = mach_vm_remap(user_map,
	    &user_remap,
	    PAGE_SIZE,
	    0,
	    VM_FLAGS_ANYWHERE,
	    user_map,
	    user_addr,
	    FALSE,                /* copy */
	    &cur_prot,
	    &max_prot,
	    VM_INHERIT_DEFAULT);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == 0, "balance=0x%llx", balance);
	/* create a UPL from the original mapping */
	upl_size = PAGE_SIZE;
	upl = NULL;
	upl_count = 0;
	upl_flags = UPL_FILE_IO | UPL_NO_SYNC | UPL_SET_INTERNAL | UPL_SET_LITE | UPL_SET_IO_WIRE;
	kr = vm_map_create_upl(user_map,
	    (vm_map_offset_t)user_addr,
	    &upl_size,
	    &upl,
	    NULL,
	    &upl_count,
	    &upl_flags,
	    VM_KERN_MEMORY_DIAG);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	pl = UPL_GET_INTERNAL_PAGE_LIST(upl);
	assert(upl_page_present(pl, 0));
	ppnum = upl_phys_page(pl, 0);
	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == 0, "balance=0x%llx", balance);
	device_object = vm_object_allocate(PAGE_SIZE);
	assert(device_object);
	vm_object_lock(device_object);
	VM_OBJECT_SET_PRIVATE(device_object, TRUE);
	VM_OBJECT_SET_PHYS_CONTIGUOUS(device_object, TRUE);
	device_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
	vm_object_unlock(device_object);
	kr = vm_object_populate_with_private(device_object, 0,
	    ppnum, PAGE_SIZE);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);

	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == 0, "balance=0x%llx", balance);
	/* deallocate the original mapping */
	kr = mach_vm_deallocate(user_map, user_addr, PAGE_SIZE);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	/* map the device_object in the kernel */
	device_addr = 0;
	vm_object_reference(device_object);
	kr = vm_map_enter(kernel_map,
	    &device_addr,
	    PAGE_SIZE,
	    0,
	    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(),
	    device_object,
	    0,
	    FALSE,               /* copy */
	    VM_PROT_DEFAULT,
	    VM_PROT_DEFAULT,
	    VM_INHERIT_NONE);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	/* access the device pager mapping */
	*(char *)device_addr = 'x';
	printf("%s:%d 0x%llx: 0x%x\n", __FUNCTION__, __LINE__, (uint64_t)device_addr, *(uint32_t *)device_addr);
	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == 0, "balance=0x%llx", balance);
	/* fault in the remap addr */
	kr = vm_fault(user_map, (vm_map_offset_t)user_remap, VM_PROT_READ,
	    FALSE, 0, TRUE, NULL, 0);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == PAGE_SIZE, "balance=0x%llx", balance);
	/* deallocate remapping */
	kr = mach_vm_deallocate(user_map, user_remap, PAGE_SIZE);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	/* check ledger */
	kr = ledger_get_balance(ledger, task_ledgers.internal, &balance);
	assertf(kr == KERN_SUCCESS, "kr=0x%x", kr);
	assertf(balance == 0, "balance=0x%llx", balance);
	/* TODO: cleanup... */
	printf("%s:%d PASS\n", __FUNCTION__, __LINE__);
}
#endif /* __arm64__ && !KASAN */

static void
vm_test_kernel_tag_accounting_kma(kma_flags_t base, kma_flags_t bit)
{
	vm_tag_t tag = VM_KERN_MEMORY_REASON; /* unused during POST */
	uint64_t init_size = vm_tag_get_size(tag);
	__assert_only uint64_t final_size = init_size + PAGE_SIZE;
	vm_address_t  address;
	kern_return_t kr;

	/*
	 * Test the matrix of:
	 *  - born with or without bit
	 *  - bit flipped or not
	 *  - dies with or without bit
	 */
	for (uint32_t i = 0; i < 4; i++) {
		kma_flags_t flags1 = base | ((i & 1) ? bit : KMA_NONE);
		kma_flags_t flags2 = base | ((i & 2) ? bit : KMA_NONE);

		kr = kmem_alloc(kernel_map, &address, PAGE_SIZE, flags1, tag);
		assert3u(kr, ==, KERN_SUCCESS);

		if (flags1 & (KMA_VAONLY | KMA_PAGEABLE)) {
			assert3u(init_size, ==, vm_tag_get_size(tag));
		} else {
			assert3u(final_size, ==, vm_tag_get_size(tag));
		}

		if ((flags1 ^ flags2) == KMA_VAONLY) {
			if (flags1 & KMA_VAONLY) {
				kernel_memory_populate(address, PAGE_SIZE,
				    KMA_KOBJECT | KMA_NOFAIL, tag);
			} else {
				kernel_memory_depopulate(address, PAGE_SIZE,
				    KMA_KOBJECT, tag);
			}
		}

		if ((flags1 ^ flags2) == KMA_PAGEABLE) {
			if (flags1 & KMA_PAGEABLE) {
				kr = vm_map_wire_kernel(kernel_map,
				    address, address + PAGE_SIZE,
				    VM_PROT_DEFAULT, tag, false);
				assert3u(kr, ==, KERN_SUCCESS);
			} else {
				kr = vm_map_unwire(kernel_map,
				    address, address + PAGE_SIZE, false);
				assert3u(kr, ==, KERN_SUCCESS);
			}
		}

		if (flags2 & (KMA_VAONLY | KMA_PAGEABLE)) {
			assert3u(init_size, ==, vm_tag_get_size(tag));
		} else {
			assert3u(final_size, ==, vm_tag_get_size(tag));
		}

		kmem_free(kernel_map, address, PAGE_SIZE);
		assert3u(init_size, ==, vm_tag_get_size(tag));
	}
}

__attribute__((noinline))
static void
vm_test_kernel_tag_accounting(void)
{
	printf("%s: test running\n", __func__);

	printf("%s: account (KMA_KOBJECT + populate)...\n", __func__);
	vm_test_kernel_tag_accounting_kma(KMA_KOBJECT, KMA_VAONLY);
	printf("%s:     PASS\n", __func__);

	printf("%s: account (regular object + wiring)...\n", __func__);
	vm_test_kernel_tag_accounting_kma(KMA_NONE, KMA_PAGEABLE);
	printf("%s:     PASS\n", __func__);

	printf("%s: test passed\n", __func__);

#undef if_bit
}

__attribute__((noinline))
static void
vm_test_collapse_overflow(void)
{
	vm_object_t object, backing_object;
	vm_object_size_t size;
	vm_page_t m;

	/* create an object for which (int)(size>>PAGE_SHIFT) = 0 */
	size = 0x400000000000ULL;
	assert((int)(size >> PAGE_SHIFT) == 0);
	backing_object = vm_object_allocate(size + PAGE_SIZE);
	assert(backing_object);
	vm_object_reference(backing_object);
	/* insert a page */
	m = VM_PAGE_NULL;
	while (m == VM_PAGE_NULL) {
		m = vm_page_grab();
		if (m == VM_PAGE_NULL) {
			VM_PAGE_WAIT();
		}
	}
	assert(m);
	vm_object_lock(backing_object);
	vm_page_insert(m, backing_object, 0);
	vm_object_unlock(backing_object);
	/* make it back another object */
	object = vm_object_allocate(size);
	assert(object);
	vm_object_reference(object);
	object->shadow = backing_object;
	vm_object_reference(backing_object);
	/* trigger a bypass */
	vm_object_lock(object);
	vm_object_collapse(object, 0, TRUE);
	/* check that it did not bypass the backing object */
	if (object->shadow != backing_object) {
		panic("%s:%d FAIL\n", __FUNCTION__, __LINE__);
	}
	vm_object_unlock(object);

	/* remove the page from the backing object */
	vm_object_lock(backing_object);
	vm_page_remove(m, TRUE);
	vm_object_unlock(backing_object);
	/* trigger a bypass */
	vm_object_lock(object);
	vm_object_collapse(object, 0, TRUE);
	/* check that it did bypass the backing object */
	if (object->shadow == backing_object) {
		panic("%s:%d FAIL\n", __FUNCTION__, __LINE__);
	}
	vm_page_insert(m, object, 0);
	vm_object_unlock(object);

	/* cleanup */
	vm_object_deallocate(object);
	/* "backing_object" already lost its reference during the bypass */
//	vm_object_deallocate(backing_object);

	printf("%s:%d PASS\n", __FUNCTION__, __LINE__);
}

__attribute__((noinline))
static void
vm_test_physical_size_overflow(void)
{
	vm_map_address_t start;
	mach_vm_size_t size;
	kern_return_t kr;
	mach_vm_size_t phys_size;
	bool fail;
	int failures = 0;

	/* size == 0 */
	start = 0x100000;
	size = 0x0;
	kr = vm_map_range_physical_size(kernel_map,
	    start,
	    size,
	    &phys_size);
	fail = (kr != KERN_SUCCESS || phys_size != 0);
	printf("%s:%d %s start=0x%llx size=0x%llx -> kr=%d phys_size=0x%llx\n",
	    __FUNCTION__, __LINE__,
	    (fail ? "FAIL" : "PASS"),
	    (uint64_t)start, size, kr, phys_size);
	failures += fail;

	/* plain wraparound */
	start = 0x100000;
	size = 0xffffffffffffffff - 0x10000;
	kr = vm_map_range_physical_size(kernel_map,
	    start,
	    size,
	    &phys_size);
	fail = (kr != KERN_INVALID_ARGUMENT || phys_size != 0);
	printf("%s:%d %s start=0x%llx size=0x%llx -> kr=%d phys_size=0x%llx\n",
	    __FUNCTION__, __LINE__,
	    (fail ? "FAIL" : "PASS"),
	    (uint64_t)start, size, kr, phys_size);
	failures += fail;

	/* wraparound after rounding */
	start = 0xffffffffffffff00;
	size = 0xf0;
	kr = vm_map_range_physical_size(kernel_map,
	    start,
	    size,
	    &phys_size);
	fail = (kr != KERN_INVALID_ARGUMENT || phys_size != 0);
	printf("%s:%d %s start=0x%llx size=0x%llx -> kr=%d phys_size=0x%llx\n",
	    __FUNCTION__, __LINE__,
	    (fail ? "FAIL" : "PASS"),
	    (uint64_t)start, size, kr, phys_size);
	failures += fail;

	/* wraparound to start after rounding */
	start = 0x100000;
	size = 0xffffffffffffffff;
	kr = vm_map_range_physical_size(kernel_map,
	    start,
	    size,
	    &phys_size);
	fail = (kr != KERN_INVALID_ARGUMENT || phys_size != 0);
	printf("%s:%d %s start=0x%llx size=0x%llx -> kr=%d phys_size=0x%llx\n",
	    __FUNCTION__, __LINE__,
	    (fail ? "FAIL" : "PASS"),
	    (uint64_t)start, size, kr, phys_size);
	failures += fail;

	if (failures) {
		panic("%s: FAIL (failures=%d)", __FUNCTION__, failures);
	}
	printf("%s: PASS\n", __FUNCTION__);
}

#define PTR_UPPER_SHIFT 60
#define PTR_TAG_SHIFT 56
#define PTR_BITS_MASK (((1ULL << PTR_TAG_SHIFT) - 1) | (0xfULL << PTR_UPPER_SHIFT))

static inline vm_map_t
create_map(mach_vm_address_t map_start, mach_vm_address_t map_end);
static inline void
cleanup_map(vm_map_t *map);

__attribute__((noinline))
static void
vm_test_address_canonicalization(void)
{
	T_SKIP("System not designed to support this test, skipping...");
}

boolean_t vm_tests_in_progress = FALSE;

kern_return_t
vm_tests(void)
{
	kern_return_t kr = KERN_SUCCESS;
	vm_tests_in_progress = TRUE;

	vm_test_collapse_compressor();
	vm_test_wire_and_extract();
	vm_test_page_wire_overflow_panic();
	vm_test_kernel_object_fault();
	vm_test_device_pager_transpose();
#if MACH_ASSERT
	vm_test_map_copy_adjust_to_target();
#endif /* MACH_ASSERT */
#if PMAP_CREATE_FORCE_4K_PAGES && MACH_ASSERT
	vm_test_4k();
#endif /* PMAP_CREATE_FORCE_4K_PAGES && MACH_ASSERT */
#if __arm64__ && !KASAN
	vm_test_per_mapping_internal_accounting();
#endif /* __arm64__ && !KASAN */
	vm_test_kernel_tag_accounting();
	vm_test_collapse_overflow();
	vm_test_physical_size_overflow();
	vm_test_address_canonicalization();

	vm_tests_in_progress = FALSE;

	return kr;
}

/*
 * Checks that vm_map_delete() can deal with map unaligned entries.
 * rdar://88969652
 */
static int
vm_map_non_aligned_test(__unused int64_t in, int64_t *out)
{
	vm_map_t map = current_map();
	mach_vm_size_t size = 2 * VM_MAP_PAGE_SIZE(map);
	mach_vm_address_t addr;
	vm_map_entry_t entry;
	kern_return_t kr;

	if (VM_MAP_PAGE_SHIFT(map) > PAGE_SHIFT) {
		kr = mach_vm_allocate(map, &addr, size, VM_FLAGS_ANYWHERE);
		if (kr != KERN_SUCCESS) {
			return ENOMEM;
		}

		vm_map_lock(map);
		if (!vm_map_lookup_entry(map, addr, &entry)) {
			panic("couldn't find the entry we just made: "
			    "map:%p addr:0x%0llx", map, addr);
		}

		/*
		 * Now break the entry into:
		 *  2 * 4k
		 *  2 * 4k
		 *  1 * 16k
		 */
		vm_map_clip_end(map, entry, addr + VM_MAP_PAGE_SIZE(map));
		entry->map_aligned = FALSE;
		vm_map_clip_end(map, entry, addr + PAGE_SIZE * 2);
		vm_map_unlock(map);

		kr = mach_vm_deallocate(map, addr, size);
		assert(kr == KERN_SUCCESS);
	}

	*out = 1;
	return 0;
}
SYSCTL_TEST_REGISTER(vm_map_non_aligned, vm_map_non_aligned_test);

static inline vm_map_t
create_map(mach_vm_address_t map_start, mach_vm_address_t map_end)
{
	ledger_t ledger = ledger_instantiate(task_ledger_template, LEDGER_CREATE_ACTIVE_ENTRIES);
	pmap_t pmap = pmap_create_options(ledger, 0, PMAP_CREATE_64BIT);
	assert(pmap);
	ledger_dereference(ledger);  // now retained by pmap
	vm_map_t map = vm_map_create_options(pmap, map_start, map_end, VM_MAP_CREATE_PAGEABLE);//vm_compute_max_offset
	assert(map);

	return map;
}

static inline void
cleanup_map(vm_map_t *map)
{
	assert(*map);
	kern_return_t kr = vm_map_terminate(*map);
	assert(kr == 0);
	vm_map_deallocate(*map);  // also destroys pmap
}

kern_return_t
mach_vm_remap_new_external(
	vm_map_t                target_map,
	mach_vm_offset_ut      *address,
	mach_vm_size_ut         size,
	mach_vm_offset_ut       mask,
	int                     flags,
	mach_port_t             src_tport,
	mach_vm_offset_ut       memory_address,
	boolean_t               copy,
	vm_prot_ut             *cur_protection_u,
	vm_prot_ut             *max_protection_u,
	vm_inherit_ut           inheritance);
kern_return_t
vm_remap_new_external(
	vm_map_t                target_map,
	vm_offset_ut           *address,
	vm_size_ut              size,
	vm_offset_ut            mask,
	int                     flags,
	mach_port_t             src_tport,
	vm_offset_ut            memory_address,
	boolean_t               copy,
	vm_prot_ut             *cur_protection,
	vm_prot_ut             *max_protection,
	vm_inherit_ut           inheritance);
kern_return_t
mach_vm_remap_external(
	vm_map_t                target_map,
	mach_vm_offset_ut      *address,
	mach_vm_size_ut         size,
	mach_vm_offset_ut       mask,
	int                     flags,
	vm_map_t                src_map,
	mach_vm_offset_ut       memory_address,
	boolean_t               copy,
	vm_prot_ut             *cur_protection,
	vm_prot_ut             *max_protection,
	vm_inherit_ut           inheritance);
kern_return_t
mach_vm_map_external(
	vm_map_t                target_map,
	mach_vm_offset_ut      *address,
	mach_vm_size_ut         initial_size,
	mach_vm_offset_ut       mask,
	int                     flags,
	ipc_port_t              port,
	memory_object_offset_ut offset,
	boolean_t               copy,
	vm_prot_ut              cur_protection,
	vm_prot_ut              max_protection,
	vm_inherit_ut           inheritance);
kern_return_t
mach_vm_wire_external(
	host_priv_t             host_priv,
	vm_map_t                map,
	mach_vm_address_ut      start,
	mach_vm_size_ut         size,
	vm_prot_ut              access);

static int
vm_map_null_tests(__unused int64_t in, int64_t *out)
{
	kern_return_t kr;

	mach_vm_address_t alloced_addr, throwaway_addr;
	mach_vm_address_ut throwaway_addr_ut;
	vm_address_t vm_throwaway_addr;
	vm_address_ut vm_throwaway_addr_ut;
	vm32_address_ut alloced_addr32, throwaway_addr32_u;
	mach_vm_size_t throwaway_size, size_16kb, read_overwrite_data_size;
	vm_size_t vm_size, vm_read_overwrite_data_size;
	vm_size_ut throwaway_size_ut;
	vm32_size_t data_size32, size32_16kb;
	vm32_size_ut data_size32_u, throwaway_size32_u;
	mach_msg_type_number_t read_data_size;
	mach_port_t mem_entry_result;
	pointer_t read_data;
	vm_prot_t prot_default;
	vm_prot_ut prot_allexec_u, prot_default_ut;
	vm_map_t map64, map32;

	map64 = create_map(0, vm_compute_max_offset(true));
	map32 = create_map(0, vm_compute_max_offset(false));

	prot_allexec_u = vm_sanitize_wrap_prot(VM_PROT_ALLEXEC);
	prot_default_ut = vm_sanitize_wrap_prot(VM_PROT_DEFAULT);
	prot_default = VM_PROT_DEFAULT;

	size_16kb = 16 * 1024;
	size32_16kb = (vm32_size_t) size_16kb;

	/*
	 * Allocate some address in the map, just so we can pass a valid looking address to functions so they don't
	 * return before checking VM_MAP_NULL
	 */
	kr = mach_vm_allocate(map64, &alloced_addr, size_16kb, VM_FLAGS_ANYWHERE);
	assert(kr == KERN_SUCCESS);
	kr = vm32_allocate(map32, &alloced_addr32, size32_16kb, VM_FLAGS_ANYWHERE);
	assert(kr == KERN_SUCCESS);

	/*
	 * Call a bunch of MIG entrypoints with VM_MAP_NULL. The goal is to verify they check map != VM_MAP_NULL.
	 * There are no requirements put on the return, so don't assert kr. Just verify no crash occurs.
	 */
	throwaway_size = size_16kb;
	kr = _mach_make_memory_entry(VM_MAP_NULL, &throwaway_size, alloced_addr, VM_PROT_DEFAULT, &mem_entry_result, IPC_PORT_NULL);
	assert(kr != KERN_SUCCESS);
	throwaway_size32_u = vm32_sanitize_wrap_size(size32_16kb);
	kr = vm32_make_memory_entry(VM_MAP_NULL, &throwaway_size32_u, alloced_addr32, VM_PROT_DEFAULT, &mem_entry_result, IPC_PORT_NULL);
	assert(kr != KERN_SUCCESS);
	throwaway_size_ut = vm_sanitize_wrap_size(size_16kb);
	kr = vm32_make_memory_entry_64(VM_MAP_NULL, &throwaway_size_ut, alloced_addr, VM_PROT_DEFAULT, &mem_entry_result, IPC_PORT_NULL);
	assert(kr != KERN_SUCCESS);
	throwaway_size = size_16kb;
	kr = mach_make_memory_entry_64(VM_MAP_NULL, &throwaway_size, alloced_addr, VM_PROT_DEFAULT, &mem_entry_result, IPC_PORT_NULL);
	assert(kr != KERN_SUCCESS);
	vm_size = size_16kb;
	kr = mach_make_memory_entry(VM_MAP_NULL, &vm_size, alloced_addr, VM_PROT_DEFAULT, &mem_entry_result, IPC_PORT_NULL);
	assert(kr != KERN_SUCCESS);

	kr = mach_memory_object_memory_entry(HOST_NULL, true, size_16kb, VM_PROT_DEFAULT, MEMORY_OBJECT_NULL, &mem_entry_result);
	assert(kr != KERN_SUCCESS);
	kr = mach_memory_object_memory_entry_64(HOST_NULL, true, size_16kb, VM_PROT_DEFAULT, MEMORY_OBJECT_NULL, &mem_entry_result);
	assert(kr != KERN_SUCCESS);

	throwaway_addr = alloced_addr;
	kr = mach_vm_allocate(VM_MAP_NULL, &throwaway_addr, size_16kb, VM_FLAGS_ANYWHERE);
	assert(kr != KERN_SUCCESS);
	throwaway_addr32_u = alloced_addr32;
	kr = vm32_allocate(VM_MAP_NULL, &throwaway_addr32_u, size32_16kb, VM_FLAGS_ANYWHERE);
	assert(kr != KERN_SUCCESS);
	kr = vm_allocate_external(VM_MAP_NULL, &vm_throwaway_addr, size_16kb, VM_FLAGS_ANYWHERE);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_deallocate(VM_MAP_NULL, alloced_addr, size_16kb);
	assert(kr != KERN_SUCCESS);
	kr = vm_deallocate(VM_MAP_NULL, alloced_addr, size_16kb);
	assert(kr != KERN_SUCCESS);
	kr = vm32_deallocate(VM_MAP_NULL, throwaway_addr32_u, size32_16kb);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_map(VM_MAP_NULL, &throwaway_addr, size_16kb, 0, VM_FLAGS_ANYWHERE, IPC_PORT_NULL, 0, false, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = mach_vm_map_external(VM_MAP_NULL, &throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, IPC_PORT_NULL, 0, false, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	vm_throwaway_addr = alloced_addr;
	kr = vm_map(VM_MAP_NULL, &vm_throwaway_addr, size_16kb, 0, VM_FLAGS_ANYWHERE, IPC_PORT_NULL, 0, false, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm32_map(VM_MAP_NULL, &throwaway_addr32_u, size32_16kb, 0, VM_FLAGS_ANYWHERE, IPC_PORT_NULL, 0, false, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm32_map_64(VM_MAP_NULL, &throwaway_addr32_u, size32_16kb, 0, VM_FLAGS_ANYWHERE, IPC_PORT_NULL, 0, false, VM_PROT_DEFAULT, VM_PROT_DEFAULT, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_remap(map64, &throwaway_addr, size_16kb, 0, VM_FLAGS_ANYWHERE, VM_MAP_NULL, 0, false, &prot_default, &prot_default, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = mach_vm_remap(VM_MAP_NULL, &throwaway_addr, size_16kb, 0, VM_FLAGS_ANYWHERE, map64, 0, false, &prot_default, &prot_default, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = mach_vm_remap_external(map64, &throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, VM_MAP_NULL, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = mach_vm_remap_external(VM_MAP_NULL, &throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, map64, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm_remap_external(map64, &vm_throwaway_addr, size_16kb, 0, VM_FLAGS_ANYWHERE, VM_MAP_NULL, 0, false, &prot_default, &prot_default, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm_remap_external(VM_MAP_NULL, &vm_throwaway_addr, size_16kb, 0, VM_FLAGS_ANYWHERE, map64, 0, false, &prot_default, &prot_default, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm32_remap(map32, &throwaway_addr32_u, size32_16kb, 0, VM_FLAGS_ANYWHERE, VM_MAP_NULL, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm32_remap(VM_MAP_NULL, &throwaway_addr32_u, size32_16kb, 0, VM_FLAGS_ANYWHERE, map32, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_remap_new_external(VM_MAP_NULL, &throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = mach_vm_remap_new_external(map64, &throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_remap_new_external(VM_MAP_NULL, &throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, false, &prot_allexec_u, &prot_allexec_u, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = mach_vm_remap_new_external(map64, &throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, false, &prot_allexec_u, &prot_allexec_u, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	kr = vm_remap_new_external(VM_MAP_NULL, &vm_throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm_remap_new_external(map64, &vm_throwaway_addr_ut, size_16kb, 0, VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, false, &prot_default_ut, &prot_default_ut, VM_INHERIT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_wire_external(host_priv_self(), VM_MAP_NULL, throwaway_addr_ut, size_16kb, VM_PROT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = mach_vm_wire_external(HOST_PRIV_NULL, map64, throwaway_addr_ut, size_16kb, VM_PROT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	kr = vm_wire(host_priv_self(), VM_MAP_NULL, throwaway_addr, size_16kb, VM_PROT_DEFAULT);
	assert(kr != KERN_SUCCESS);
	kr = vm_wire(HOST_PRIV_NULL, map64, throwaway_addr, size_16kb, VM_PROT_DEFAULT);
	assert(kr != KERN_SUCCESS);

	kr = task_wire(VM_MAP_NULL, false);
	assert(kr != KERN_SUCCESS);
	kr = vm32__task_wire(VM_MAP_NULL, false);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_read(VM_MAP_NULL, alloced_addr, size_16kb, &read_data, &read_data_size);
	assert(kr != KERN_SUCCESS);
	kr = vm_read(VM_MAP_NULL, alloced_addr, size_16kb, &read_data, &read_data_size);
	assert(kr != KERN_SUCCESS);
	kr = vm32_read(VM_MAP_NULL, alloced_addr32, size32_16kb, &read_data, &data_size32);
	assert(kr != KERN_SUCCESS);

	mach_vm_read_entry_t * mach_re = kalloc_type(mach_vm_read_entry_t, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	(*mach_re)[0].address = alloced_addr;
	(*mach_re)[0].size = size_16kb;

	vm_read_entry_t * re = kalloc_type(vm_read_entry_t, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	(*re)[0].address = alloced_addr;
	(*re)[0].size = (vm_size_t) size_16kb;

	vm32_read_entry_t * re_32 = kalloc_type(vm32_read_entry_t, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	(*re_32)[0].address = (vm32_address_t) alloced_addr;
	(*re_32)[0].size = (vm32_size_t) size_16kb;

	kr = mach_vm_read_list(VM_MAP_NULL, *mach_re, 1);
	assert(kr != KERN_SUCCESS);
	kr = vm_read_list(VM_MAP_NULL, *re, 1);
	assert(kr != KERN_SUCCESS);
	kr = vm32_read_list(VM_MAP_NULL, *re_32, 1);
	assert(kr != KERN_SUCCESS);

	kfree_type(mach_vm_read_entry_t, mach_re);
	kfree_type(vm_read_entry_t, re);
	kfree_type(vm32_read_entry_t, re_32);

	kr = mach_vm_read_overwrite(VM_MAP_NULL, alloced_addr, size_16kb, alloced_addr, &read_overwrite_data_size);
	assert(kr != KERN_SUCCESS);
	kr = vm_read_overwrite(VM_MAP_NULL, alloced_addr, size_16kb, alloced_addr, &vm_read_overwrite_data_size);
	assert(kr != KERN_SUCCESS);
	kr = vm32_read_overwrite(VM_MAP_NULL, alloced_addr32, size32_16kb, alloced_addr32, &data_size32_u);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_copy(VM_MAP_NULL, alloced_addr, size_16kb, alloced_addr);
	assert(kr != KERN_SUCCESS);
	kr = vm_copy(VM_MAP_NULL, alloced_addr, size_16kb, alloced_addr);
	assert(kr != KERN_SUCCESS);
	kr = vm32_copy(VM_MAP_NULL, alloced_addr32, size32_16kb, alloced_addr32);
	assert(kr != KERN_SUCCESS);

	kr = mach_vm_write(VM_MAP_NULL, alloced_addr, alloced_addr, (mach_msg_type_number_t) size_16kb);
	assert(kr != KERN_SUCCESS);
	kr = vm_write(VM_MAP_NULL, alloced_addr, alloced_addr, (mach_msg_type_number_t) size_16kb);
	assert(kr != KERN_SUCCESS);
	kr = vm32_write(VM_MAP_NULL, alloced_addr32, alloced_addr, (mach_msg_type_number_t) size_16kb);
	assert(kr != KERN_SUCCESS);

	/*
	 * Cleanup our allocations and maps
	 */
	kr = mach_vm_deallocate(map64, alloced_addr, size_16kb);
	assert(kr == KERN_SUCCESS);
	kr = vm32_deallocate(map32, alloced_addr32, size32_16kb);
	assert(kr == KERN_SUCCESS);

	cleanup_map(&map64);
	cleanup_map(&map32);

	/*
	 * If we made it far without crashing, the test works.
	 */

	*out = 1;
	return 0;
}
SYSCTL_TEST_REGISTER(vm_map_null, vm_map_null_tests);

#if CONFIG_PROB_GZALLOC
extern vm_offset_t pgz_protect_for_testing_only(zone_t zone, vm_offset_t addr, void *fp);

static int
vm_memory_entry_pgz_test(__unused int64_t in, int64_t *out)
{
	kern_return_t kr;
	ipc_port_t mem_entry_ptr;
	mach_vm_address_t allocation_addr = 0;
	vm_size_t size = PAGE_SIZE;

	allocation_addr = (mach_vm_address_t) kalloc_data(size, Z_WAITOK);
	if (!allocation_addr) {
		*out = -1;
		return 0;
	}

	/*
	 * Make sure we get a pgz protected address
	 * If we aren't already protected, try to protect it
	 */
	if (!pgz_owned(allocation_addr)) {
		zone_id_t zid = zone_id_for_element((void *) allocation_addr, size);
		zone_t zone = &zone_array[zid];
		allocation_addr = pgz_protect_for_testing_only(zone, allocation_addr, __builtin_frame_address(0));
	}
	/*
	 * If we still aren't protected, tell userspace to skip the test
	 */
	if (!pgz_owned(allocation_addr)) {
		*out = 2;
		return 0;
	}

	kr = mach_make_memory_entry(kernel_map, &size, (mach_vm_offset_t) allocation_addr, VM_PROT_READ | VM_PROT_WRITE | MAP_MEM_VM_COPY, &mem_entry_ptr, IPC_PORT_NULL);
	assert(kr == KERN_SUCCESS);

	ipc_port_release(mem_entry_ptr);
	kfree_data(allocation_addr, size);

	*out = 1;
	return 0;
}
#else /* CONFIG_PROB_GZALLOC */
static int
vm_memory_entry_pgz_test(__unused int64_t in, int64_t *out)
{
	*out = 1;
	return 0;
}
#endif /* CONFIG_PROB_GZALLOC */

SYSCTL_TEST_REGISTER(vm_memory_entry_pgz, vm_memory_entry_pgz_test);
