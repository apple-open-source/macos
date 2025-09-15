/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
 * vm/configurator_vm_wire.c
 *
 * Test vm_wire with many different VM states.
 */

#include "configurator/vm_configurator_tests.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.vm.configurator"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM"),
	T_META_RUN_CONCURRENTLY(true),
	T_META_ALL_VALID_ARCHS(true),
	T_META_ASROOT(true)  /* root required for vm_wire on macOS */
	);

/*
 * Update checker state to mirror a successful call to
 * vm_wire(PROT_NONE) a.k.a. unwire
 */
static void
checker_perform_vm_unwire(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	entry_checker_range_t limit = checker_list_find_and_clip(checker_list, start, size);
	FOREACH_CHECKER(checker, limit) {
		assert(checker->user_wired_count > 0);
		checker->user_wired_count--;
	}
	checker_list_simplify(checker_list, start, size);
}


/*
 * Update checker state to mirror a successful call to
 * vm_wire(PROT_NONE) a.k.a. unwire
 * of a range that includes holes.
 */
static kern_return_t
checker_perform_vm_unwire_with_holes(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	entry_checker_range_t limit = checker_list_find_range_including_holes(checker_list, start, size);

	if (limit.head && limit.head->kind == Allocation &&
	    checker_contains_address(limit.head, start)) {
		/* range begins with an allocation - proceed normally */
	} else {
		/* range begins with a hole - do nothing, not even simplify */
		return KERN_INVALID_ADDRESS;
	}

	FOREACH_CHECKER(checker, limit) {
		if (checker->kind == Allocation) {
			assert(checker->user_wired_count > 0);
			checker->user_wired_count--;
		}
	}

	checker_list_simplify(checker_list, start, size);
	return KERN_SUCCESS;
}


/*
 * Update checker state to mirrow a successful call to vm_wire.
 */
static void
checker_perform_vm_wire(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size,
	vm_prot_t wire_prot)
{
	assert(wire_prot != VM_PROT_NONE);

	entry_checker_range_t limit;

	/*
	 * Resolve null objects.
	 * vm_wire does this before clipping
	 */
	limit = checker_list_find_range_including_holes(checker_list, start, size);
	FOREACH_CHECKER(checker, limit) {
		checker_resolve_null_vm_object(checker_list, checker);
	}

	/*
	 * Perform clipping.
	 */
	limit = checker_list_find_range(checker_list, start, size);
	checker_clip_left(checker_list, limit.head, start);
	checker_clip_right(checker_list, limit.tail, start + size);

	/*
	 * Fault and wire.
	 */

	FOREACH_CHECKER(checker, limit) {
		checker->user_wired_count++;
		checker_fault_for_prot_not_cow(checker_list, checker, wire_prot);
	}
	checker_list_simplify(checker_list, start, size);
}


static void
checker_perform_failed_vm_wire(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size,
	vm_prot_t wire_prot)
{
	assert(wire_prot != VM_PROT_NONE);

	/*
	 * failed vm_wire clips entries and resolves null vm_objects
	 * one at a time until the entry that it couldn't change
	 *
	 * failed vm_wire doesn't simplify clipped entries on exit
	 *
	 * failed vm_wire is inconsistent about resident page counts
	 */

	entry_checker_range_t limit =
	    checker_list_find_range_including_holes(checker_list, start, size);
	FOREACH_CHECKER(checker, limit) {
		if (checker->kind != Allocation) {
			/* stop at holes */
			break;
		}

		/* wire of executable entry fails early */
		if (prot_contains_all(checker->protection, VM_PROT_EXECUTE)) {
			// (fixme jit, tpro)
			break;
		}

		/* null vm_objects are resolved before clipping */
		checker_resolve_null_vm_object(checker_list, checker);

		if (checker == limit.head) {
			checker_clip_left(checker_list, checker, start);
		}
		if (checker == limit.tail) {
			checker_clip_right(checker_list, checker, start + size);
		}

		if (!prot_contains_all(checker->protection, wire_prot)) {
			/* stop at protection failures */
			break;
		}

		if (checker != limit.tail && checker->next->kind != Allocation) {
			/* stop if the *next* entry is in range and is an illegal hole */
			break;
		}

		/*
		 * failed vm_wire simplifies and faults in,
		 * except for the cases already short-circuited above
		 */
		checker_fault_for_prot_not_cow(checker_list, checker, wire_prot);
		checker_simplify_left(checker_list, checker);
	}
}


static test_result_t
successful_vm_wire_read_not_cow(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	kern_return_t kr;

	checker_perform_vm_wire(checker_list, start, size, VM_PROT_READ);
	kr = mach_vm_wire(host_priv(), mach_task_self(), start, size, VM_PROT_READ);
	if (kr) {
		T_FAIL("mach_vm_wire failed (%s)", name_for_kr(kr));
		return TestFailed;
	}

	if (verify_vm_state(checker_list, "after vm_wire") != TestSucceeded) {
		return TestFailed;
	}

	checker_perform_vm_unwire(checker_list, start, size);
	kr = mach_vm_wire(host_priv(), mach_task_self(), start, size, VM_PROT_NONE);
	if (kr) {
		T_FAIL("mach_vm_wire(unwire) failed (%s)", name_for_kr(kr));
		return TestFailed;
	}

	if (verify_vm_state(checker_list, "after vm_unwire") != TestSucceeded) {
		return TestFailed;
	}

	return TestSucceeded;
}

static test_result_t
failed_vm_wire_read_not_cow(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	kern_return_t kr;

	checker_perform_failed_vm_wire(checker_list, start, size, VM_PROT_READ);
	kr = mach_vm_wire(host_priv(), mach_task_self(), start, size, VM_PROT_READ);
	if (kr == KERN_SUCCESS) {
		T_FAIL("mach_vm_wire unexpectedly succeeded");
		return TestFailed;
	}

	return verify_vm_state(checker_list, "after unsuccessful vm_wire");
}

static test_result_t
wire_shared_entry(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	/* two entries each sharing the same object */
	vm_entry_checker_t *right_checker = checker_list_nth(checker_list, 1);

	kern_return_t kr;

	/*
	 * Wire the left entry. The right entry also faults in but
	 * stays at wire count zero.
	 */
	checker_perform_vm_wire(checker_list, start, size, VM_PROT_READ);
	checker_fault_for_prot_not_cow(checker_list, right_checker, VM_PROT_READ);
	kr = mach_vm_wire(host_priv(), mach_task_self(), start, size, VM_PROT_READ);
	assert(kr == 0);

	return verify_vm_state(checker_list, "after vm_wire shared");
}

static test_result_t
wire_shared_entry_discontiguous(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	/*
	 * two entries each sharing the same object
	 * but only partially overlap inside that object.
	 * Wiring the left entry does not affect the right entry,
	 * so this looks like an ordinary vm_wire test.
	 */
	return successful_vm_wire_read_not_cow(checker_list, start, size);
}

static test_result_t
wire_shared_entry_partial(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	/*
	 * two entries each sharing the same object
	 * but only partially overlap inside that object
	 */
	vm_entry_checker_t *right_checker = checker_list_nth(checker_list, 1);
	mach_vm_address_t right_offset = DEFAULT_PARTIAL_ENTRY_SIZE;

	kern_return_t kr;

	/*
	 * Wire the left entry. The right entry stays at wire count zero
	 * and only the overlapping section faults in.
	 */
	checker_perform_vm_wire(checker_list, start, size, VM_PROT_READ);
	right_checker->pages_resident = (uint32_t)((size - right_offset) / PAGE_SIZE);
	kr = mach_vm_wire(host_priv(), mach_task_self(), start, size, VM_PROT_READ);
	assert(kr == 0);

	return verify_vm_state(checker_list, "after vm_wire shared partial");
}

static void
checker_make_cow_private(
	checker_list_t *checker_list,
	vm_entry_checker_t *checker)
{
	if (checker->object->self_ref_count == 1) {
		/*
		 * COW but not shared with anything else.
		 * VM resolves COW by using the same object.
		 */
		checker->needs_copy = false;
		return;
	}

	/* make new object */
	vm_object_checker_t *obj_checker = object_checker_clone(checker->object);
	checker_list_append_object(checker_list, obj_checker);

	/* change object and entry to private */
	checker->needs_copy = false;

	/* set new object (decreasing previous object's self_ref_count) */
	checker_set_object(checker, obj_checker);
}

static test_result_t
wire_cow_entry(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	/* Wiring a COW entry resolves COW but has no effect on other copies. */

	vm_entry_checker_t *left_checker = checker_list_nth(checker_list, 0);
	checker_make_cow_private(checker_list, left_checker);

	return successful_vm_wire_read_not_cow(checker_list, start, size);
}

static test_result_t
wire_cow_nocow(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	vm_entry_checker_t *left_checker = checker_list_nth(checker_list, 0);
	checker_make_cow_private(checker_list, left_checker);

	return successful_vm_wire_read_not_cow(checker_list, start, size);
}

static test_result_t
wire_nocow_cow(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	vm_entry_checker_t *left_checker = checker_list_nth(checker_list, 0);
	vm_entry_checker_t *right_checker = left_checker->next;
	checker_make_cow_private(checker_list, right_checker);

	return successful_vm_wire_read_not_cow(checker_list, start, size);
}

static test_result_t
wire_cow_unreadable(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	vm_entry_checker_t *checker = checker_list_nth(checker_list, 0);
	checker_make_shadow_object(checker_list, checker);
	return failed_vm_wire_read_not_cow(checker_list, start, size);
}

static test_result_t
wire_cow_unwriteable(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	vm_entry_checker_t *checker = checker_list_nth(checker_list, 0);
	checker_make_cow_private(checker_list, checker);

	return successful_vm_wire_read_not_cow(checker_list, start, size);
}


/*
 * Test vm_unwire with a range that includes holes.
 * We wire each allocation separately, then unwire the entire range
 * to test unwire's behavior across holes without reference to
 * wire's behavior across holes.
 */
static test_result_t
vm_unwire_holes(
	checker_list_t *checker_list,
	mach_vm_address_t start,
	mach_vm_size_t size)
{
	kern_return_t kr, expected_kr;

	/*
	 * Wire each allocation separately,
	 * then unwire the entire range at once.
	 */

	mach_vm_address_t end = start + size;

	entry_checker_range_t limit =
	    checker_list_find_range_including_holes(checker_list, start, size);

	FOREACH_CHECKER(checker, limit) {
		if (checker->kind == Allocation) {
			/*
			 * we manually "clip" our address range here
			 * because the real checker clipping must
			 * be done inside checker_perform_vm_wire()
			 * because wire's clip behavior is weird
			 */
			mach_vm_address_t clipped_address = max(start, checker->address);
			mach_vm_address_t clipped_end = min(checker_end_address(checker), end);
			mach_vm_size_t clipped_size = clipped_end - clipped_address;
			kr = mach_vm_wire(host_priv(), mach_task_self(),
			    clipped_address, clipped_size, VM_PROT_READ);
			assert(kr == 0);
			checker_perform_vm_wire(checker_list,
			    clipped_address, clipped_size, VM_PROT_READ);
		}
	}

	if (verify_vm_state(checker_list, "before vm_unwire") != TestSucceeded) {
		return TestFailed;
	}

	expected_kr = checker_perform_vm_unwire_with_holes(checker_list, start, size);
	kr = mach_vm_wire(host_priv(), mach_task_self(), start, size, VM_PROT_NONE);
	if (kr != expected_kr) {
		T_FAIL("mach_vm_wire(unwire) returned %d (%s), expected %d (%s)\n",
		    kr, name_for_kr(kr), expected_kr, name_for_kr(expected_kr));
		return TestFailed;
	}

	if (verify_vm_state(checker_list, "after vm_unwire") != TestSucceeded) {
		return TestFailed;
	}

	return TestSucceeded;
}

T_DECL(vm_wire,
    "run vm_wire with various vm configurations")
{
	vm_tests_t tests = {
		.single_entry_1 = successful_vm_wire_read_not_cow,
		.single_entry_2 = successful_vm_wire_read_not_cow,
		.single_entry_3 = successful_vm_wire_read_not_cow,
		.single_entry_4 = successful_vm_wire_read_not_cow,

		.multiple_entries_1 = successful_vm_wire_read_not_cow,
		.multiple_entries_2 = successful_vm_wire_read_not_cow,
		.multiple_entries_3 = successful_vm_wire_read_not_cow,
		.multiple_entries_4 = successful_vm_wire_read_not_cow,
		.multiple_entries_5 = successful_vm_wire_read_not_cow,
		.multiple_entries_6 = successful_vm_wire_read_not_cow,

		.some_holes_1 = failed_vm_wire_read_not_cow,
		.some_holes_2 = failed_vm_wire_read_not_cow,
		.some_holes_3 = failed_vm_wire_read_not_cow,
		.some_holes_4 = failed_vm_wire_read_not_cow,
		.some_holes_5 = failed_vm_wire_read_not_cow,
		.some_holes_6 = failed_vm_wire_read_not_cow,
		.some_holes_7 = failed_vm_wire_read_not_cow,
		.some_holes_8 = failed_vm_wire_read_not_cow,
		.some_holes_9 = failed_vm_wire_read_not_cow,
		.some_holes_10 = failed_vm_wire_read_not_cow,
		.some_holes_11 = failed_vm_wire_read_not_cow,
		.some_holes_12 = failed_vm_wire_read_not_cow,

		.all_holes_1 = failed_vm_wire_read_not_cow,
		.all_holes_2 = failed_vm_wire_read_not_cow,
		.all_holes_3 = failed_vm_wire_read_not_cow,
		.all_holes_4 = failed_vm_wire_read_not_cow,

		.null_entry        = successful_vm_wire_read_not_cow,
		.nonresident_entry = successful_vm_wire_read_not_cow,
		.resident_entry    = successful_vm_wire_read_not_cow,

		.shared_entry               = wire_shared_entry,
		.shared_entry_discontiguous = wire_shared_entry_discontiguous,
		.shared_entry_partial       = wire_shared_entry_partial,
		.shared_entry_pairs         = successful_vm_wire_read_not_cow,
		.shared_entry_x1000         = successful_vm_wire_read_not_cow,

		.cow_entry = wire_cow_entry,
		.cow_unreferenced = wire_cow_entry,
		.cow_nocow = wire_cow_nocow,
		.nocow_cow = wire_nocow_cow,
		.cow_unreadable = wire_cow_unreadable,
		.cow_unwriteable = wire_cow_unwriteable,

		.permanent_entry = successful_vm_wire_read_not_cow,
		.permanent_before_permanent = successful_vm_wire_read_not_cow,
		.permanent_before_allocation = successful_vm_wire_read_not_cow,
		.permanent_before_allocation_2 = successful_vm_wire_read_not_cow,
		.permanent_before_hole = failed_vm_wire_read_not_cow,
		.permanent_after_allocation = successful_vm_wire_read_not_cow,
		.permanent_after_hole = failed_vm_wire_read_not_cow,

		/* TODO: wire vs submaps */
		.single_submap_single_entry = test_is_unimplemented,
		.single_submap_single_entry_first_pages = test_is_unimplemented,
		.single_submap_single_entry_last_pages = test_is_unimplemented,
		.single_submap_single_entry_middle_pages = test_is_unimplemented,
		.single_submap_oversize_entry_at_start = test_is_unimplemented,
		.single_submap_oversize_entry_at_end = test_is_unimplemented,
		.single_submap_oversize_entry_at_both = test_is_unimplemented,

		.submap_before_allocation = test_is_unimplemented,
		.submap_after_allocation = test_is_unimplemented,
		.submap_before_hole = test_is_unimplemented,
		.submap_after_hole = test_is_unimplemented,
		.submap_allocation_submap_one_entry = test_is_unimplemented,
		.submap_allocation_submap_two_entries = test_is_unimplemented,
		.submap_allocation_submap_three_entries = test_is_unimplemented,

		.submap_before_allocation_ro = test_is_unimplemented,
		.submap_after_allocation_ro = test_is_unimplemented,
		.submap_before_hole_ro = test_is_unimplemented,
		.submap_after_hole_ro = test_is_unimplemented,
		.submap_allocation_submap_one_entry_ro = test_is_unimplemented,
		.submap_allocation_submap_two_entries_ro = test_is_unimplemented,
		.submap_allocation_submap_three_entries_ro = test_is_unimplemented,

		.protection_single_000_000 = failed_vm_wire_read_not_cow,
		.protection_single_000_r00 = failed_vm_wire_read_not_cow,
		.protection_single_000_0w0 = failed_vm_wire_read_not_cow,
		.protection_single_000_rw0 = failed_vm_wire_read_not_cow,
		.protection_single_r00_r00 = successful_vm_wire_read_not_cow,
		.protection_single_r00_rw0 = successful_vm_wire_read_not_cow,
		.protection_single_0w0_0w0 = failed_vm_wire_read_not_cow,
		.protection_single_0w0_rw0 = failed_vm_wire_read_not_cow,
		.protection_single_rw0_rw0 = successful_vm_wire_read_not_cow,

		.protection_pairs_000_000 = failed_vm_wire_read_not_cow,
		.protection_pairs_000_r00 = failed_vm_wire_read_not_cow,
		.protection_pairs_000_0w0 = failed_vm_wire_read_not_cow,
		.protection_pairs_000_rw0 = failed_vm_wire_read_not_cow,
		.protection_pairs_r00_000 = failed_vm_wire_read_not_cow,
		.protection_pairs_r00_r00 = successful_vm_wire_read_not_cow,
		.protection_pairs_r00_0w0 = failed_vm_wire_read_not_cow,
		.protection_pairs_r00_rw0 = successful_vm_wire_read_not_cow,
		.protection_pairs_0w0_000 = failed_vm_wire_read_not_cow,
		.protection_pairs_0w0_r00 = failed_vm_wire_read_not_cow,
		.protection_pairs_0w0_0w0 = failed_vm_wire_read_not_cow,
		.protection_pairs_0w0_rw0 = failed_vm_wire_read_not_cow,
		.protection_pairs_rw0_000 = failed_vm_wire_read_not_cow,
		.protection_pairs_rw0_r00 = successful_vm_wire_read_not_cow,
		.protection_pairs_rw0_0w0 = failed_vm_wire_read_not_cow,
		.protection_pairs_rw0_rw0 = successful_vm_wire_read_not_cow,
	};

	run_vm_tests("vm_wire", __FILE__, &tests, argc, argv);
}


T_DECL(vm_unwire,
    "run vm_unwire with various vm configurations")
{
	vm_tests_t tests = {
		.single_entry_1 = test_is_unimplemented,
		.single_entry_2 = test_is_unimplemented,
		.single_entry_3 = test_is_unimplemented,
		.single_entry_4 = test_is_unimplemented,

		.multiple_entries_1 = test_is_unimplemented,
		.multiple_entries_2 = test_is_unimplemented,
		.multiple_entries_3 = test_is_unimplemented,
		.multiple_entries_4 = test_is_unimplemented,
		.multiple_entries_5 = test_is_unimplemented,
		.multiple_entries_6 = test_is_unimplemented,

		.some_holes_1 = vm_unwire_holes,
		.some_holes_2 = vm_unwire_holes,
		.some_holes_3 = vm_unwire_holes,
		.some_holes_4 = vm_unwire_holes,
		.some_holes_5 = vm_unwire_holes,
		.some_holes_6 = vm_unwire_holes,
		.some_holes_7 = vm_unwire_holes,
		.some_holes_8 = vm_unwire_holes,
		.some_holes_9 = vm_unwire_holes,
		.some_holes_10 = vm_unwire_holes,
		.some_holes_11 = vm_unwire_holes,
		.some_holes_12 = vm_unwire_holes,

		.all_holes_1 = vm_unwire_holes,
		.all_holes_2 = vm_unwire_holes,
		.all_holes_3 = vm_unwire_holes,
		.all_holes_4 = vm_unwire_holes,

		.null_entry        = test_is_unimplemented,
		.nonresident_entry = test_is_unimplemented,
		.resident_entry    = test_is_unimplemented,

		.shared_entry               = test_is_unimplemented,
		.shared_entry_discontiguous = test_is_unimplemented,
		.shared_entry_partial       = test_is_unimplemented,
		.shared_entry_pairs         = test_is_unimplemented,
		.shared_entry_x1000         = test_is_unimplemented,

		.cow_entry = test_is_unimplemented,
		.cow_unreferenced = test_is_unimplemented,
		.cow_nocow = test_is_unimplemented,
		.nocow_cow = test_is_unimplemented,
		.cow_unreadable = test_is_unimplemented,
		.cow_unwriteable = test_is_unimplemented,

		.permanent_entry = test_is_unimplemented,
		.permanent_before_permanent = test_is_unimplemented,
		.permanent_before_allocation = test_is_unimplemented,
		.permanent_before_allocation_2 = test_is_unimplemented,
		.permanent_before_hole = test_is_unimplemented,
		.permanent_after_allocation = test_is_unimplemented,
		.permanent_after_hole = test_is_unimplemented,

		.single_submap_single_entry = test_is_unimplemented,
		.single_submap_single_entry_first_pages = test_is_unimplemented,
		.single_submap_single_entry_last_pages = test_is_unimplemented,
		.single_submap_single_entry_middle_pages = test_is_unimplemented,
		.single_submap_oversize_entry_at_start = test_is_unimplemented,
		.single_submap_oversize_entry_at_end = test_is_unimplemented,
		.single_submap_oversize_entry_at_both = test_is_unimplemented,

		.submap_before_allocation = test_is_unimplemented,
		.submap_after_allocation = test_is_unimplemented,
		.submap_before_hole = test_is_unimplemented,
		.submap_after_hole = test_is_unimplemented,
		.submap_allocation_submap_one_entry = test_is_unimplemented,
		.submap_allocation_submap_two_entries = test_is_unimplemented,
		.submap_allocation_submap_three_entries = test_is_unimplemented,

		.submap_before_allocation_ro = test_is_unimplemented,
		.submap_after_allocation_ro = test_is_unimplemented,
		.submap_before_hole_ro = test_is_unimplemented,
		.submap_after_hole_ro = test_is_unimplemented,
		.submap_allocation_submap_one_entry_ro = test_is_unimplemented,
		.submap_allocation_submap_two_entries_ro = test_is_unimplemented,
		.submap_allocation_submap_three_entries_ro = test_is_unimplemented,

		.protection_single_000_000 = test_is_unimplemented,
		.protection_single_000_r00 = test_is_unimplemented,
		.protection_single_000_0w0 = test_is_unimplemented,
		.protection_single_000_rw0 = test_is_unimplemented,
		.protection_single_r00_r00 = test_is_unimplemented,
		.protection_single_r00_rw0 = test_is_unimplemented,
		.protection_single_0w0_0w0 = test_is_unimplemented,
		.protection_single_0w0_rw0 = test_is_unimplemented,
		.protection_single_rw0_rw0 = test_is_unimplemented,

		.protection_pairs_000_000 = test_is_unimplemented,
		.protection_pairs_000_r00 = test_is_unimplemented,
		.protection_pairs_000_0w0 = test_is_unimplemented,
		.protection_pairs_000_rw0 = test_is_unimplemented,
		.protection_pairs_r00_000 = test_is_unimplemented,
		.protection_pairs_r00_r00 = test_is_unimplemented,
		.protection_pairs_r00_0w0 = test_is_unimplemented,
		.protection_pairs_r00_rw0 = test_is_unimplemented,
		.protection_pairs_0w0_000 = test_is_unimplemented,
		.protection_pairs_0w0_r00 = test_is_unimplemented,
		.protection_pairs_0w0_0w0 = test_is_unimplemented,
		.protection_pairs_0w0_rw0 = test_is_unimplemented,
		.protection_pairs_rw0_000 = test_is_unimplemented,
		.protection_pairs_rw0_r00 = test_is_unimplemented,
		.protection_pairs_rw0_0w0 = test_is_unimplemented,
		.protection_pairs_rw0_rw0 = test_is_unimplemented,
	};

	run_vm_tests("vm_unwire", __FILE__, &tests, argc, argv);
}
