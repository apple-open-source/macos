//
//  pgm_testing.h
//  libmalloc
//
//  Shared testing code for ProbGuard.
//

#ifndef _PGM_TESTING_H_
#define _PGM_TESTING_H_

#pragma mark -
#pragma mark Mocks

#define PGM_MOCK_RANDOM
static uint32_t expected_upper_bound;
static uint32_t rand_ret_value;
static uint32_t rand_ret_values[10];
static uint32_t rand_call_count;
static bool rand_use_ret_values;
static uint32_t
rand_uniform(uint32_t upper_bound)
{
	T_QUIET; T_EXPECT_EQ(upper_bound, expected_upper_bound, "rand_uniform(upper_bound)");
	if (rand_use_ret_values) {
		T_QUIET; T_ASSERT_LT(rand_call_count, 10, NULL);
		rand_ret_value = rand_ret_values[rand_call_count];
	}
	rand_call_count++;
	return rand_ret_value;
}

#define PGM_MOCK_TRACE_COLLECT
static uint8_t *expected_trace_buffers[3];
static size_t expected_trace_sizes[3];
static uint32_t capture_trace_call_count;
static size_t collect_trace_ret_value;
MALLOC_ALWAYS_INLINE
static inline size_t
my_trace_collect(uint8_t *buffer, size_t size)
{
	T_QUIET; T_ASSERT_LT(capture_trace_call_count, 3, NULL);
	T_QUIET; T_EXPECT_EQ(buffer, expected_trace_buffers[capture_trace_call_count], "my_trace_collect(buffer)");
	T_QUIET; T_EXPECT_EQ(size, expected_trace_sizes[capture_trace_call_count], "my_trace_collect(size)");
	capture_trace_call_count++;

	return collect_trace_ret_value;
}

#define PGM_MOCK_PAGE_ACCESS
static vm_address_t expected_inaccessible_page;
static vm_address_t expected_read_write_page;
static void
mark_inaccessible(vm_address_t page) {
	T_QUIET; T_EXPECT_EQ(page, expected_inaccessible_page, "mark_inaccessible(page)");
}
static void
mark_read_write(vm_address_t page) {
	T_QUIET; T_EXPECT_EQ(page, expected_read_write_page, "mark_read_write(page)");
}

static vm_address_t expected_cause;
static const char *expected_msg;
static void
report_fatal_error(vm_address_t addr, const char *msg) {
	T_QUIET; T_EXPECT_EQ(addr, expected_cause, "MALLOC_REPORT_FATAL_ERROR(): cause");
	T_QUIET; T_EXPECT_EQ(msg, expected_msg, "MALLOC_REPORT_FATAL_ERROR(): message");
}
#undef MALLOC_REPORT_FATAL_ERROR
#define MALLOC_REPORT_FATAL_ERROR(cause, message) \
	report_fatal_error(cause, message); \
	T_END

#undef memcpy
static vm_address_t expected_dest;
static vm_address_t expected_src;
static size_t expected_size;
static void
memcpy(void *dest, void *src, size_t size)
{
	T_QUIET; T_EXPECT_EQ(dest, (void *)expected_dest, "memcpy(): dest");
	T_QUIET; T_EXPECT_EQ(src, (void *)expected_src, "memcpy(): src");
	T_QUIET; T_EXPECT_EQ(size, expected_size, "memcpy(): size");
}


#pragma mark -
#pragma mark Wrapped Zone Mocks

static malloc_zone_t wrapped_zone;

static vm_address_t expected_size_ptr;
static size_t size_ret_value;
static size_t
wrapped_size(malloc_zone_t *zone, const void *ptr) {
	T_QUIET; T_EXPECT_EQ(zone, &wrapped_zone, "wrapped_size(): zone");
	T_QUIET; T_EXPECT_EQ(ptr, (void *)expected_size_ptr, "wrapped_size(): ptr");
	return size_ret_value;
}

static size_t expected_malloc_size;
static vm_address_t malloc_ret_value;
static void *
wrapped_malloc(malloc_zone_t *zone, size_t size) {
	T_QUIET; T_EXPECT_EQ(zone, &wrapped_zone, "wrapped_malloc(): zone");
	T_QUIET; T_EXPECT_EQ(size, expected_malloc_size, "wrapped_malloc(): size");
	return (void *)malloc_ret_value;
}

static vm_address_t expected_free_ptr;
static void
wrapped_free(malloc_zone_t *zone, void *ptr) {
	T_QUIET; T_EXPECT_EQ(zone, &wrapped_zone, "wrapped_free(): zone");
	T_QUIET; T_EXPECT_EQ(ptr, (void *)expected_free_ptr, "wrapped_free(): ptr");
}

static malloc_zone_t wrapped_zone = {
	.size = wrapped_size,
	.malloc = wrapped_malloc,
	.free = wrapped_free
};


#pragma mark -
#pragma mark Test Harness

#include "../src/pgm_malloc.c"
// Dependencies
#include "../src/has_section.c"
#include "../src/malloc_common.c"
#include "../src/stack_trace.c"
#include "../src/wrapper_zones.c"

static slot_t slots[10];
static metadata_t metadata[10];
static pgm_zone_t zone = {
	.wrapped_zone = &wrapped_zone,
	.slots = slots,
	.metadata = metadata
};

// Stub out cross-file dependencies.
void malloc_report(uint32_t flags, const char *fmt, ...) { __builtin_trap(); }
void malloc_report_simple(const char *fmt, ...) { __builtin_trap(); }

#endif // _PGM_TESTING_H_
