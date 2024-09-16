//
//  pgm_internals.c
//  libmalloc
//
//  Unit tests for implementation details of ProbGuard.
//
//  NOTE: We redefine the PAGE_SIZE macro below.  Therefore, this test
//        file should not exercise code that vm_maps actual memory!
//

#include <../src/internal.h>

#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(TRUE), T_META_NAMESPACE("pgm"),
		T_META_TAG_VM_PREFERRED);

// Use extra weird page size (not even a power of 2) to expose implicit
// assumptions and help prevent issues caused by different page sizes on macOS
// and iOS.
#undef PAGE_SIZE
#define PAGE_SIZE 1000

#include "pgm_testing.h"


#pragma mark -
#pragma mark Decider Functions

T_DECL(is_full, "is_full")
{
	T_EXPECT_TRUE(is_full(&zone), "zero capacity");
	zone.max_allocations = 1;
	T_EXPECT_FALSE(is_full(&zone), "0/1 capacity");
	zone.num_allocations = 1;
	T_EXPECT_TRUE(is_full(&zone), "1/1 capacity");
}

T_DECL(should_sample_counter, "should_sample_counter")
{
	expected_upper_bound = 7;
	rand_ret_value = 0;
	T_EXPECT_TRUE(should_sample_counter(7), "1/1 -> sample");
	T_EXPECT_TRUE(should_sample_counter(7), "1/1 -> sample");

	rand_ret_value = 1;
	T_EXPECT_FALSE(should_sample_counter(7), "1/2 -> skip");
	T_EXPECT_TRUE (should_sample_counter(7), "2/2 -> sample");
	T_EXPECT_FALSE(should_sample_counter(7), "1/2 -> skip");
	T_EXPECT_TRUE (should_sample_counter(7), "2/2 -> sample");

	rand_ret_value = 2;
	T_EXPECT_FALSE(should_sample_counter(7), "1/3 -> skip");
	T_EXPECT_FALSE(should_sample_counter(7), "2/3 -> skip");
	T_EXPECT_TRUE (should_sample_counter(7), "3/3 -> sample");

	rand_ret_value = 0;
	malloc_thread_options_t opts = {.DisableProbabilisticGuardMalloc = true};
	malloc_set_thread_options(opts);
	T_EXPECT_EQ(TSD_GET_COUNTER(), k_no_sample, "set no sample marker");
	T_EXPECT_FALSE(should_sample_counter(8), "sampling disabled");
	T_EXPECT_EQ(TSD_GET_COUNTER(), k_no_sample, "no sample marker retained");

	opts.DisableProbabilisticGuardMalloc = false;
	malloc_set_thread_options(opts);
	T_EXPECT_EQ(TSD_GET_COUNTER(), 0, "regenerate sample counter");
	T_EXPECT_TRUE(should_sample_counter(7), "sampling re-enabled");
}

T_DECL(should_sample, "should_sample")
{
	zone.sample_counter_range = expected_upper_bound = 7;
	zone.max_allocations = 1;
	T_EXPECT_TRUE (should_sample(&zone, 0), "zero size");
	T_EXPECT_TRUE (should_sample(&zone, 5), "normal size");
	T_EXPECT_TRUE (should_sample(&zone, PAGE_SIZE), "page size");
	T_EXPECT_FALSE(should_sample(&zone, PAGE_SIZE + 1), "size > page size");
	T_EXPECT_EQ(rand_call_count, 3, NULL);

	zone.num_allocations = 1;
	T_EXPECT_FALSE(should_sample(&zone, 5), "zone full");

	zone.max_allocations = 2;
	rand_ret_value = 1;
	T_EXPECT_FALSE(should_sample(&zone, 5), "1/2 -> skip");
	T_EXPECT_TRUE (should_sample(&zone, 5), "2/2 -> sample");

	// Ensure rand_uniform() is only called when needed.
	T_EXPECT_EQ(rand_call_count, 4, NULL);
	T_EXPECT_FALSE(should_sample(&zone, PAGE_SIZE + 1), "bad size");
	zone.num_allocations = 2;
	T_EXPECT_FALSE(should_sample(&zone, 5), "zone full");
	T_EXPECT_EQ(rand_call_count, 4, NULL);
}

T_DECL(is_guarded, "is_guarded")
{
	zone.begin = 2; zone.end = 4;
	T_EXPECT_FALSE(is_guarded(&zone, 1), "before");
	T_EXPECT_TRUE (is_guarded(&zone, 2), "begin inclusive");
	T_EXPECT_TRUE (is_guarded(&zone, 3), "inside");
	T_EXPECT_FALSE(is_guarded(&zone, 4), "end exclusive");
	T_EXPECT_FALSE(is_guarded(&zone, 5), "after");
}


#pragma mark -
#pragma mark Slot <-> Address Mapping

T_DECL(quarantine_size, "quarantine_size")
{
	T_EXPECT_EQ(quarantine_size(0), 1000ul, "0 slots");
	T_EXPECT_EQ(quarantine_size(1), 3000ul, "1 slot");
	T_EXPECT_EQ(quarantine_size(2), 5000ul, "2 slots");
}

T_DECL(page_addr, "page_addr")
{
	zone.num_slots = 2; zone.begin = 50000;
	T_EXPECT_EQ(page_addr(&zone, 0), 51000ul, "slot 1");
	T_EXPECT_EQ(page_addr(&zone, 1), 53000ul, "slot 2");
}

T_DECL(block_addr, "block_addr")
{
	zone.num_slots = 2;	zone.begin = 50000;
	slots[1].offset = 7;
	T_EXPECT_EQ(block_addr(&zone, 0), 51000ul, "slot 1");
	T_EXPECT_EQ(block_addr(&zone, 1), 53007ul, "slot 2");
}

T_DECL(page_idx, "page_idx")
{
	zone.begin = 50000; zone.end = 60000;
	T_EXPECT_EQ(page_idx(&zone, 50000), 0, "page 0, first byte");
	T_EXPECT_EQ(page_idx(&zone, 50999), 0, "page 0, last byte");
	T_EXPECT_EQ(page_idx(&zone, 51000), 1, "page 1, first byte");
}

T_DECL(is_guard_page, "is_guard_page")
{
	zone.begin = 50000; zone.end = 60000;
	T_EXPECT_TRUE (is_guard_page(&zone, 50000), "page 0");
	T_EXPECT_FALSE(is_guard_page(&zone, 51000), "page 1");
	T_EXPECT_TRUE (is_guard_page(&zone, 52000), "page 2");
}


#pragma mark -
#pragma mark Slot Lookup

T_DECL(nearest_slot, "nearest_slot")
{
	zone.num_slots = 7;
	zone.begin = 50000; zone.end = 60000;
	T_EXPECT_EQ(nearest_slot(&zone, 49999), 0, "before quarantine");
	T_EXPECT_EQ(nearest_slot(&zone, 50000), 0, "first byte in quarantine");
	T_EXPECT_EQ(nearest_slot(&zone, 59999), 6, "last  byte in quarantine");
	T_EXPECT_EQ(nearest_slot(&zone, 60000), 6, "after  quarantine");

	T_EXPECT_EQ(nearest_slot(&zone, 52499), 0, "left  half of guard page 1");
	T_EXPECT_EQ(nearest_slot(&zone, 52500), 1, "right half of guard page 1");
	T_EXPECT_EQ(nearest_slot(&zone, 53000), 1, "first byte of slot 1");
	T_EXPECT_EQ(nearest_slot(&zone, 53999), 1, "last  byte of slot 1");
}

T_DECL(lookup_slot, "lookup_slot")
{
	#define TEST_LOOKUP_SLOT(addr, expected_slot, expected_bounds, expected_live_block_addr, msg) \
		{ \
			slot_lookup_t res = lookup_slot(&zone, addr); \
			T_EXPECT_EQ((unsigned int)res.slot, expected_slot, msg ": slot"); \
			T_EXPECT_EQ((unsigned int)res.bounds, expected_bounds, msg ": bounds"); \
			T_EXPECT_EQ(!!res.live_block_addr, expected_live_block_addr, msg ": live_block_addr"); \
		}

	zone.begin = 50000; zone.end = 60000;
	slots[0].offset = 7; slots[0].size = 2;
	TEST_LOOKUP_SLOT(51000, 0, b_oob_slot,       false, "slot 0");
	TEST_LOOKUP_SLOT(51007, 0, b_block_addr,     false, "block address");
	TEST_LOOKUP_SLOT(51008, 0, b_valid,          false, "valid address");
	TEST_LOOKUP_SLOT(51009, 0, b_oob_slot,       false, "slot");
	TEST_LOOKUP_SLOT(52000, 0, b_oob_guard_page, false, "guard page");
	TEST_LOOKUP_SLOT(53007, 1, b_oob_slot,       false, "slot 1");

	slots[0].state = ss_allocated;
	TEST_LOOKUP_SLOT(51007, 0, b_block_addr,     true,  "live block address");
}


#pragma mark -
#pragma mark Allocator Helpers

T_DECL(is_power_of_2, "is_power_of_2")
{
	T_EXPECT_FALSE(is_power_of_2(0), "0 is not a power of 2");
	T_EXPECT_FALSE(is_power_of_2(3), "3 is not a power of 2");
	T_EXPECT_FALSE(is_power_of_2(6), "6 is not a power of 2");

	T_EXPECT_TRUE(is_power_of_2(1), "1 is a power of 2");
	T_EXPECT_TRUE(is_power_of_2(2), "2 is a power of 2");
	T_EXPECT_TRUE(is_power_of_2(4), "4 is a power of 2");

	T_EXPECT_TRUE(powerof2(0), "powerof2(0) is wrong");
}

T_DECL(block_size, "block_size")
{
	T_EXPECT_EQ(block_size( 0), 16ul, NULL);
	T_EXPECT_EQ(block_size( 1), 16ul, NULL);
	T_EXPECT_EQ(block_size(15), 16ul, NULL);
	T_EXPECT_EQ(block_size(16), 16ul, NULL);
	T_EXPECT_EQ(block_size(17), 32ul, NULL);
	T_EXPECT_EQ(block_size(32), 32ul, NULL);
	T_EXPECT_EQ(block_size(33), 48ul, NULL);
}

T_DECL(choose_available_slot, "choose_available_slot")
{
	zone.num_slots = 3;
	slots[0].state = ss_allocated;
	T_EXPECT_EQ(choose_available_slot(&zone), 1, "first free slot");
	T_EXPECT_EQ(zone.rr_slot_index, 2, "rr_slot_index points to next slot");

	slots[1].state = ss_freed;
	T_EXPECT_EQ(choose_available_slot(&zone), 2, "next free slot; no immediate reuse");
	T_EXPECT_EQ(zone.rr_slot_index, 0, "rr_slot_index wraps around to next slot");
}

T_DECL(choose_metadata, "choose_metadata")
{
	zone.max_metadata = 2;
	T_EXPECT_EQ(choose_metadata(&zone, -1), 0, "0/2 -> 0");
	T_EXPECT_EQ(choose_metadata(&zone, -1), 1, "1/2 -> 1");
	T_EXPECT_EQ(rand_call_count, 0, NULL);

	slots[2].metadata = 2; metadata[2].slot = 2;
	T_EXPECT_EQ(choose_metadata(&zone, 2), 2, "reuse metadata");
	T_EXPECT_EQ(rand_call_count, 0, NULL);

	expected_upper_bound = 2; rand_use_ret_values = true;
	slots[0].state = ss_allocated; metadata[0].slot = 0; rand_ret_values[0] = 0;
	slots[1].state = ss_freed;     metadata[1].slot = 1; rand_ret_values[1] = 1;
	T_EXPECT_EQ(choose_metadata(&zone, 3), 1, "full -> random metadata (for freed slot)");
	T_EXPECT_EQ(rand_call_count, 2, "try random index until we find metadata for a freed slot");
}

T_DECL(choose_offset_on_page, "choose_offset_on_page")
{
	uint16_t page_size = 32;
	expected_upper_bound = 2;

	rand_ret_value = 1;
	T_EXPECT_EQ(choose_offset_on_page(5, 16, page_size), (uint16_t)0, "left-aligned");

	rand_ret_value = 0;
	T_EXPECT_EQ(choose_offset_on_page( 0,  1, page_size), (uint16_t)32, "size 0, perfectly right-aligned");
	T_EXPECT_EQ(choose_offset_on_page( 1,  1, page_size), (uint16_t)31, "size 1, perfectly right-aligned");
	T_EXPECT_EQ(choose_offset_on_page( 5,  1, page_size), (uint16_t)27, "perfectly right-aligned");
	T_EXPECT_EQ(choose_offset_on_page( 5,  2, page_size), (uint16_t)26, "right-aligned by 2");
	T_EXPECT_EQ(choose_offset_on_page( 5,  4, page_size), (uint16_t)24, "right-aligned by 4");
	T_EXPECT_EQ(choose_offset_on_page( 5,  8, page_size), (uint16_t)24, "right-aligned by 8");
	T_EXPECT_EQ(choose_offset_on_page( 5, 16, page_size), (uint16_t)16, "right-aligned by 16");
	T_EXPECT_EQ(choose_offset_on_page( 5, 32, page_size),  (uint16_t)0, "right-aligned by page size");
	T_EXPECT_EQ(choose_offset_on_page(32,  1, page_size),  (uint16_t)0, "page size allocation w/o alignment");
	T_EXPECT_EQ(choose_offset_on_page(32,  8, page_size),  (uint16_t)0, "page size allocation w/ alignment");
}

T_DECL(capture_trace, "capture_trace")
{
	uint64_t thread_id = _pthread_threadid_self_np_direct();
	metadata_t m = {};

	expected_trace_buffers[0] = m.trace_buffer;
	expected_trace_sizes[0] = 216;
	collect_trace_ret_value = 100;
	m.dealloc.trace_size = 123;

	capture_trace(&m, /*alloc=*/true);
	T_EXPECT_EQ(m.alloc.thread_id, thread_id, "alloc thread");
	T_EXPECT_NE(m.alloc.time, 0ull, "alloc time");
	T_EXPECT_EQ(m.alloc.trace_size, (uint16_t)100, "alloc trace size");
	T_EXPECT_EQ(m.dealloc.trace_size, (uint16_t)0, "dealloc trace size is reset");

	expected_trace_buffers[1] = &m.trace_buffer[100];
	expected_trace_sizes[1] = 116;  // 216 - 100
	collect_trace_ret_value = 99;

	capture_trace(&m, /*alloc=*/false);
	T_EXPECT_EQ(m.alloc.trace_size, (uint16_t)100, "alloc trace size is dealloc offset");
	T_EXPECT_EQ(m.dealloc.thread_id, thread_id, "dealloc thread");
	T_EXPECT_NE(m.dealloc.time, 0ull, "dealloc time");
	T_EXPECT_EQ(m.dealloc.trace_size, (uint16_t)99, "dealloc trace size");

	m.alloc.trace_size = 200;
	expected_trace_buffers[2] = &m.trace_buffer[108];  // 216 / 2
	expected_trace_sizes[2] = 108;

	capture_trace(&m, /*alloc=*/false);
	T_EXPECT_EQ(m.alloc.trace_size, (uint16_t)108, "offset is clamped");
}
