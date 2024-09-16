//
//  pgm_allocator.c
//  libmalloc
//
//  Unit tests for the main ProbGuard allocator functions.
//
//  NOTE: We redefine the PAGE_SIZE macro below.  Therefore, this test
//        file should not exercise code that vm_maps actual memory!
//

#include <../src/internal.h>

#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(TRUE), T_META_NAMESPACE("pgm"));

// Use weird page size to expose implicit assumptions and help prevent issues
// caused by different page sizes on macOS and iOS.
#undef PAGE_SIZE
#define PAGE_SIZE 1024

#include "pgm_testing.h"


#pragma mark -
#pragma mark Allocator Functions

T_DECL(lookup_size, "lookup_size", T_META_TAG_VM_PREFERRED)
{
	zone.begin = 640000; zone.end = 650240; // lookup_slot
	slots[0] = (slot_t){
		.state = ss_allocated,
		.size = 5,
		.offset = 7
	};
	T_EXPECT_EQ(lookup_size(&zone, 641023), 0ul, "guard page");
	T_EXPECT_EQ(lookup_size(&zone, 641024), 0ul, "slot");
	T_EXPECT_EQ(lookup_size(&zone, 641031), 5ul, "block address");
	T_EXPECT_EQ(lookup_size(&zone, 641032), 0ul, "valid non-block address");

	slots[0].state = ss_freed;
	T_EXPECT_EQ(lookup_size(&zone, 641031),  0ul, "freed block address");
}

T_DECL(allocate, "allocate", T_META_TAG_VM_PREFERRED)
{
	T_EXPECT_NULL(allocate(&zone, 5, 16), "zone full");

	zone.max_allocations = 2; // is_full
	zone.num_slots = 2; zone.rr_slot_index = 1; // choose_available_slot
	zone.max_metadata = 4; zone.num_metadata = 2; // choose_metadata
	expected_upper_bound = 2; rand_ret_value = false; // choose_offset_on_page
	expected_trace_buffers[0] = metadata[2].trace_buffer; // capture_trace
	expected_trace_sizes[0] = 216; collect_trace_ret_value = 88;
	zone.begin = 640000; // page_addr
	expected_read_write_page = 643072; // mark_read_write
	T_EXPECT_EQ(allocate(&zone, 5, 16), 644080ul, "block allocated");
	// Slot metadata
	T_EXPECT_EQ((unsigned int)(slots[1].state), ss_allocated, "slot.state");
	T_EXPECT_EQ((unsigned int)(slots[1].metadata), 2, "slot.metadata");
	T_EXPECT_EQ(slots[1].size, (uint16_t)16, "slot.size");
	T_EXPECT_EQ(slots[1].offset, (uint16_t)1008, "slot.offset");
	T_EXPECT_EQ(metadata[2].alloc.trace_size, (uint16_t)88, "metadata.alloc.trace_size");
	// Zone state
	T_EXPECT_EQ(zone.num_allocations, 1, "zone.num_allocations");
	T_EXPECT_EQ(zone.size_in_use, 16ul, "zone.size_in_use");
	T_EXPECT_EQ(zone.max_size_in_use, 16ul, "zone.max_size_in_use");

	zone.max_size_in_use = 55;
	expected_trace_buffers[1] = metadata[3].trace_buffer; expected_trace_sizes[1] = 216;
	expected_read_write_page = 641024;
	T_EXPECT_NOTNULL(allocate(&zone, 5, 16), "block allocated");
	T_EXPECT_EQ(zone.max_size_in_use, 55ul, "max_size_in_use is high water mark");
}

T_DECL(deallocate, "deallocate", T_META_TAG_VM_PREFERRED)
{
	zone.begin = 640000; zone.end = 650240; // lookup_slot
	slots[0] = (slot_t){
		.state = ss_allocated,
		.metadata = 3,
		.size = 5,
		.offset = 7
	};
	zone.num_allocations = 2; zone.size_in_use = 10;
	metadata[3].alloc.trace_size = 100;
	expected_trace_buffers[0] = &metadata[3].trace_buffer[100]; // capture_trace
	expected_trace_sizes[0] = 216 - 100; collect_trace_ret_value = 88;
	expected_inaccessible_page = 641024; // mark_inaccessible
	zone.num_slots = 1; // page_addr
	deallocate(&zone, 641031);
	// Slot metadata
	T_EXPECT_EQ((unsigned int)(slots[0].state), ss_freed, "slot.state");
	T_EXPECT_EQ(metadata[3].dealloc.trace_size, (uint16_t)88, "metadata.dealloc.trace_size");
	// Zone state
	T_EXPECT_EQ(zone.num_allocations, 1, "zone.num_allocations");
	T_EXPECT_EQ(zone.size_in_use, 5ul, "zone.size_in_use");

	expected_cause = 641031;
	expected_msg = "ProbGuard: invalid pointer passed to free";
	deallocate(&zone, 641031);
	T_FAIL("unreported double free");
}

// TODO(yln): test for reallocate with bad ptr

T_DECL(reallocate_guarded_to_sampled, "reallocate: guarded -> sampled", T_META_TAG_VM_PREFERRED)
{
	zone.begin = 640000; zone.end = 650240; // is_guarded
	slots[0] = (slot_t){ .state = ss_allocated, .metadata = 1, .size = 5 }; // lookup_size
	zone.max_allocations = 2; // is_full
	zone.num_slots = 2; // allocate
	expected_upper_bound = 2; rand_ret_value = true; // allocate -> choose_offset_on_page
	zone.max_metadata = 1; // allocate -> choose_metadata
	expected_trace_buffers[0] = metadata[0].trace_buffer; // allocate -> capture_trace
	expected_trace_sizes[0] = 216;
	expected_read_write_page = 643072; // allocate -> mark_read_write
	expected_dest = 643072; expected_src = 641024; expected_size = 5; // memcpy
	expected_trace_buffers[1] = metadata[1].trace_buffer; // deallocate -> capture_trace
	expected_trace_sizes[1] = 216;
	expected_inaccessible_page = 641024; // allocate -> mark_inaccessible
	T_EXPECT_EQ(reallocate(&zone, 641024, 10, TRUE), 643072ul, "block reallocated");
	T_EXPECT_EQ((unsigned int)(slots[0].state), ss_freed, "source slot");
	T_EXPECT_EQ((unsigned int)(slots[1].state), ss_allocated, "destination slot");
}

T_DECL(reallocate_unguarded_to_sampled, "reallocate: unguarded -> sampled", T_META_TAG_VM_PREFERRED)
{
	expected_size_ptr = 1337; size_ret_value = 5; // wrapped_size
	zone.max_allocations = 2; // is_full
	zone.num_slots = 2; // allocate
	expected_upper_bound = 2; rand_ret_value = true; // allocate -> choose_offset_on_page
	zone.max_metadata = 1; // allocate -> choose_metadata
	expected_trace_buffers[0] = metadata[0].trace_buffer; // allocate -> capture_trace
	expected_trace_sizes[0] = 216;
	zone.begin = 640000; // allocate -> page_addr
	expected_read_write_page = 641024; // allocate -> mark_read_write
	expected_dest = 641024; expected_src = 1337; expected_size = 5; // memcpy
	expected_free_ptr = 1337; // wrapped_free
	T_EXPECT_EQ(reallocate(&zone, 1337, 10, TRUE), 641024ul, "block reallocated");
	T_EXPECT_EQ((unsigned int)(slots[0].state), ss_allocated, "destination slot");
}

T_DECL(reallocate_guarded_to_unsampled, "reallocate: guarded -> unsampled", T_META_TAG_VM_PREFERRED)
{
	zone.begin = 640000; zone.end = 650240; // is_guarded
	slots[0] = (slot_t){ .state = ss_allocated, .metadata = 1, .size = 5 }; // lookup_size
	zone.max_allocations = 2; // is_full
	expected_malloc_size = 10; malloc_ret_value = 1337; // wrapped_malloc
	expected_dest = 1337; expected_src = 641024; expected_size = 5; // memcpy
	expected_trace_buffers[0] = metadata[1].trace_buffer; // deallocate -> capture_trace
	expected_trace_sizes[0] = 216;
	zone.num_slots = 2; // page_addr
	expected_inaccessible_page = 641024; // allocate -> mark_inaccessible
	T_EXPECT_EQ(reallocate(&zone, 641024, 10, FALSE), 1337ul, "block reallocated");
	T_EXPECT_EQ((unsigned int)(slots[0].state), ss_freed, "source slot");
}

T_DECL(reallocate_guarded_to_unsampled_zone_full, "reallocate: guarded -> unsampled (zone full)", T_META_TAG_VM_PREFERRED)
{
	zone.begin = 640000; zone.end = 650240; // is_guarded
	slots[0] = (slot_t){ .state = ss_allocated, .metadata = 1, .size = 5 }; // lookup_size
	zone.max_allocations = 0; // is_full
	expected_malloc_size = 10; malloc_ret_value = 1337; // wrapped_malloc
	expected_dest = 1337; expected_src = 641024; expected_size = 5; // memcpy
	expected_trace_buffers[0] = metadata[1].trace_buffer; // deallocate -> capture_trace
	expected_trace_sizes[0] = 216;
	zone.num_slots = 2; // page_addr
	expected_inaccessible_page = 641024; // allocate -> mark_inaccessible
	T_EXPECT_EQ(reallocate(&zone, 641024, 10, TRUE), 1337ul, "block reallocated");
	T_EXPECT_EQ((unsigned int)(slots[0].state), ss_freed, "source slot");
}
