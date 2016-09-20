#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <malloc/malloc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include "stack_logging.h"
#include <sys/stat.h>

#if DARWINTEST
#include <darwintest.h>

#define FAIL(msg, ...) \
T_QUIET; \
T_FAIL(msg, ## __VA_ARGS__)

#define EXPECT_TRUE(expr, msg, ...) \
T_QUIET; \
T_EXPECT_TRUE(expr, msg,  ## __VA_ARGS__)

#define EXPECT_EQ(val1, val2, msg, ...) \
T_QUIET; \
T_EXPECT_EQ(val1, val2, msg,  ## __VA_ARGS__)

#define PAUSE(msg)

#else

#define FAIL(msg, ...) \
{ \
printf("test failure:"); \
printf(msg, ## __VA_ARGS__); \
printf("\n"); \
getchar(); \
}

#define EXPECT_TRUE(expr, msg, ...) \
if (!(expr)) \
FAIL(msg,  ## __VA_ARGS__);

#define EXPECT_EQ(val1, val2, msg, ...) \
if (val1 != val2) \
FAIL(msg,  ## __VA_ARGS__);


// change this to actually pause if you want to examine the stacks using SamplingTools
#define PAUSE(msg) \
printf(msg); \
printf("\n"); \
//getchar();

#endif


const int max_size = 100;
const int allocation_count = 10;
const int item_count = 20;
#define MAX_FRAMES	512

static void
free_ptrs(malloc_zone_t *zone, char *ptrs[], int num_ptrs, boolean_t use_zone_free)
{
	for (int i = 0; i < num_ptrs; i++) {
		size_t len = malloc_size(ptrs[i]);
		
		// set the memory to different values for possible diagnostics later on
		if (use_zone_free) {
			memset(ptrs[i], '!', len);
			zone->free(zone, ptrs[i]);
		} else if (zone) {
			memset(ptrs[i], '@', len);
			malloc_zone_free(zone, ptrs[i]);
		} else {
			memset(ptrs[i], '%', len);
			free(ptrs[i]);
		}
	}
}

static uint64_t
get_stack_id_from_ptr(void *ptr)
{
	size_t ptr_size = malloc_size(ptr) + 8;
	void *idptr = ptr + ptr_size - sizeof(uint64_t);
	
	return * (uint64_t *) idptr;
}

extern uint64_t __mach_stack_logging_shared_memory_address;

static void
check_stacks(char *ptrs[], int num_ptrs, boolean_t lite_mode)
{
	mach_vm_address_t frames[MAX_FRAMES];
	uint32_t frames_count;
	
	for (int i = 0; i < num_ptrs; i++) {
		kern_return_t ret = (lite_mode) ?
		__mach_stack_logging_frames_for_uniqued_stack(mach_task_self(), get_stack_id_from_ptr(ptrs[i]), frames, MAX_FRAMES, &frames_count) :
		__mach_stack_logging_get_frames(mach_task_self(), (mach_vm_address_t) ptrs[i], frames, MAX_FRAMES, &frames_count);
		
		EXPECT_TRUE(ret == KERN_SUCCESS, "return from __mach_stack_logging_get_frames = %d\n", (int) ret);
		EXPECT_TRUE(frames_count > 0, "number of frames returned from __mach_stack_logging_get_frames = %u\n", frames_count);
	}
}

static void
test_malloc(malloc_zone_t *zone, boolean_t lite_mode, boolean_t validate_stacks, boolean_t use_zone_functions, boolean_t use_zone_free)
{
	char *ptrs[allocation_count];
	
	for (int i = 0; i < allocation_count; i++) {
		size_t size = rand() % max_size;
		
		if (use_zone_functions) {
			ptrs[i] = zone->malloc(zone, size);
		} else {
			ptrs[i] = zone ? malloc_zone_malloc(zone, size) : malloc(size);
		}
		
		// fill ptr with numbers in case a leak shows up
		for (int j = 0; j < size; j++) {
			ptrs[i][j] = '0' + i;
		}
	}
	
	if (validate_stacks) {
		check_stacks(ptrs, allocation_count, lite_mode);
	}
	
	PAUSE(zone ? "malloc_zone_malloc" : "malloc");
	
	free_ptrs(zone, ptrs, allocation_count, use_zone_free);
}

static void
test_calloc(malloc_zone_t *zone, boolean_t lite_mode, boolean_t validate_stacks, boolean_t use_zone_functions, boolean_t use_zone_free)
{
	char *ptrs[allocation_count];
	
	for (int i = 0; i < allocation_count; i++) {
		size_t size = rand() % max_size;
		
		if (use_zone_functions) {
			ptrs[i] = zone->calloc(zone, item_count, size);
		} else {
			ptrs[i] = zone ? malloc_zone_calloc(zone, item_count, size) : calloc(item_count, size);
		}
		
		// fill ptr with numbers in case a leak shows up
		for (int j = 0; j < size; j++) {
			ptrs[i][j] = 'A' + i;
		}
	}
	
	if (validate_stacks) {
		check_stacks(ptrs, allocation_count, lite_mode);
	}
	
	PAUSE(zone ? "malloc_zone_calloc" : "calloc");
	
	free_ptrs(zone, ptrs, allocation_count, use_zone_free);
}

static void
test_valloc(malloc_zone_t *zone, boolean_t lite_mode, boolean_t validate_stacks, boolean_t use_zone_functions, boolean_t use_zone_free)
{
	char *ptrs[allocation_count];
	
	for (int i = 0; i < allocation_count; i++) {
		size_t size = rand() % max_size;
		
		if (use_zone_functions) {
			ptrs[i] = zone->valloc(zone, size);
		} else {
			ptrs[i] = zone ? malloc_zone_valloc(zone, size) : valloc(size);
		}
		
		// fill ptr with numbers in case a leak shows up
		for (int j = 0; j < size; j++) {
			ptrs[i][j] = 'a' + i;
		}
	}
	
	if (validate_stacks) {
		check_stacks(ptrs, allocation_count, lite_mode);
	}
	
	PAUSE(zone ? "malloc_zone_valloc" : "valloc");
	
	free_ptrs(zone, ptrs, allocation_count, use_zone_free);
}

static void
test_realloc(malloc_zone_t *zone, boolean_t lite_mode, boolean_t validate_stacks, boolean_t use_zone_functions, boolean_t use_zone_free)
{
	char *ptrs[allocation_count];
	
	for (int i = 0; i < allocation_count; i++) {
		size_t size = rand() % max_size;
		ptrs[i] = zone ? malloc_zone_malloc(zone, size) : malloc(size);
	}
	
	for (int i = 0; i < allocation_count; i++) {
		size_t size = rand() % max_size;
		
		if (use_zone_functions) {
			ptrs[i] = zone->realloc(zone, ptrs[i], size);
		} else {
			ptrs[i] = zone ? malloc_zone_realloc(zone, ptrs[i], size) : realloc(ptrs[i], size);
		}
		
		// fill ptr with numbers in case a leak shows up
		for (int j = 0; j < size; j++) {
			ptrs[i][j] = 'r' + i;
		}
	}
	
	if (validate_stacks) {
		check_stacks(ptrs, allocation_count, lite_mode);
	}
	
	PAUSE(zone ? "malloc_zone_realloc" : "realloc");
	
	free_ptrs(zone, ptrs, allocation_count, use_zone_free);
}

static void
test_batch_malloc(malloc_zone_t *zone, boolean_t lite_mode, boolean_t validate_stacks, boolean_t use_zone_functions, boolean_t use_zone_free)
{
	size_t size = rand() % max_size;
	void *results[allocation_count];
	unsigned num_allocated;
	
	if (use_zone_functions) {
		num_allocated = zone->batch_malloc(zone, size, results, allocation_count);
	} else {
		num_allocated = malloc_zone_batch_malloc(zone, size, results, allocation_count);
	}
	
	if (validate_stacks && num_allocated > 0) {
		check_stacks((char**) results, num_allocated, lite_mode);
	}
	
	PAUSE("malloc_zone_batch_malloc");
	
	for (int i = 0; i < num_allocated; i++) {
		size_t len = malloc_size(results[i]);
		memset(results[i], '$', len);
	}
	
	if (use_zone_free) {
		zone->batch_free(zone, results, num_allocated);
	} else {
		malloc_zone_batch_free(zone, results, num_allocated);
	}
}

static void
test_memalign(malloc_zone_t *zone, boolean_t lite_mode, boolean_t validate_stacks, boolean_t use_zone_functions, boolean_t use_zone_free)
{
	char *ptrs[allocation_count];
	
	for (int i = 0; i < allocation_count; i++) {
		size_t size = rand() % max_size;
		
		if (use_zone_functions) {
			ptrs[i] = zone->memalign(zone, 1024, size);
		} else {
			ptrs[i] = malloc_zone_memalign(zone, 1024, size);
		}
	}
	
	if (validate_stacks) {
		check_stacks(ptrs, allocation_count, lite_mode);
	}
	
	PAUSE("malloc_zone_memalign");
	
	free_ptrs(zone, ptrs, allocation_count, use_zone_free);
}

// tests calling zone->size and zone->free
static void
test_malloc_zone_functions(malloc_zone_t *zone)
{
	void *ptrs[allocation_count];
	
	for (int i = 0; i < allocation_count; i++) {
		size_t size = rand() % max_size;
		ptrs[i] = malloc(size);
		
		size_t allocated_size = malloc_size(ptrs[i]);
		EXPECT_TRUE(allocated_size >= size, "allocated size=%lu requested size=%lu", allocated_size, size);
		
		size_t zone_size = zone->size(zone, ptrs[i]);
		EXPECT_EQ(allocated_size, zone_size, "allocated size=%lu zone size=%lu", allocated_size, zone_size);
	}
	
	for (int i = 0; i < allocation_count; i++) {
		size_t len = malloc_size(ptrs[i]);
		memset(ptrs[i], '&', len);
		zone->free(zone, ptrs[i]);
	}
}

typedef struct {
	void *ptr1;
	size_t ptr1_size;
	void *ptr2;
	size_t ptr2_size;
	int num_ptrs_found;
} zone_enumerator_info;


static void zone_enumerator(task_t task, void *context, unsigned type, vm_range_t *ranges, unsigned count)
{
	zone_enumerator_info *info = (zone_enumerator_info *) context;
	
	for (unsigned int i = 0; i < count; i++) {
		void *ptr = (void*) ranges[i].address;
		size_t size = ranges[i].size;
		
		if (ptr == info->ptr1 && size == info->ptr1_size) {
			info->num_ptrs_found++;
		} else if (ptr == info->ptr2 && size == info->ptr2_size) {
			info->num_ptrs_found++;
		}
	}
}

static void test_zone_enumeration(malloc_zone_t *zone, boolean_t lite_mode_enabled)
{
	// allocate some ptrs with msl
	char *new_ptr_1 = malloc(10);
	char *new_ptr_2 = malloc(10);
	
	// now check to see if enumerating the default zone finds both ptrs
	
	zone_enumerator_info info;
	
	info.ptr1 = new_ptr_1;
	info.ptr1_size = malloc_size(info.ptr1);
	if (lite_mode_enabled) {
		// need to add 8 bytes to get raw size
		info.ptr1_size += 8;
	}
	
	info.ptr2 = new_ptr_2;
	info.ptr2_size = malloc_size(info.ptr2);
	if (lite_mode_enabled) {
		// need to add 8 bytes to get raw size
		info.ptr2_size += 8;
	}
	
	info.num_ptrs_found = 0;
	
	int expected_ptrs_found = 2;
	
	kern_return_t err = zone->introspect->enumerator(mach_task_self(), &info, MALLOC_PTR_IN_USE_RANGE_TYPE, (vm_address_t) zone, NULL, zone_enumerator);
	
	EXPECT_EQ(err, KERN_SUCCESS, "return from default_zone->introspect->enumerator: %d", err);
	EXPECT_EQ(info.num_ptrs_found, expected_ptrs_found, "info.num_ptrs_found:%d expected:%d", info.num_ptrs_found, expected_ptrs_found);
	
	free(new_ptr_1);
	free(new_ptr_2);
}

static void
test_virtual_default_zone(malloc_zone_t *zone, boolean_t nano_allocator_enabled, boolean_t lite_mode_enabled)
{
	// <rdar://problem/26335503> leak in nano zone enumerator
	if (!nano_allocator_enabled)
		test_zone_enumeration(zone, lite_mode_enabled);
}

static void
test_introspection_functions(malloc_zone_t *zone, boolean_t nano_allocator_enabled)
{
	malloc_introspection_t *introspect = zone->introspect;
	
	size_t size = introspect->good_size(zone, 16);
	size_t expected_size = 16;
	EXPECT_EQ(size, expected_size, "introspect->good_size=%lu expected_size=%lu", size, expected_size);
	
	// <rdar://problem/24680189> malloc heap checking still crashes
	//	boolean_t ret = introspect->check(zone);
	//	EXPECT_EQ(ret, true, "introspect->check=%d", (int) ret);
	
	introspect->force_lock(zone);
	introspect->force_unlock(zone);
	
	boolean_t locked = introspect->zone_locked(zone);
	// can't check return value for nano allocator
	// <rdar://problem/26391117> nano_locked checks both the nano zone and helper zone, but nano force lock and force unlock only operate on the nano zone
	if (!nano_allocator_enabled) {
		EXPECT_EQ(locked, false, "introspect->zone_locked=%d", (int) locked);
	}
	
	malloc_statistics_t stats;
	char *p = zone->malloc(zone, 10);
	
	introspect->statistics(zone, &stats);
	// don't check the valus in status because of <rdar://problem/26391877> bug in szone_statistics?
	// also I imagine they could change over time so best not to rely on checing these internals
	
	zone->free(zone, p);
}

static void
test_pressure_relief(malloc_zone_t *default_zone)
{
	// call both the single zone and all zone versions
	// can't rely on return value to be consistent, so just make sure we don't crash
	// or corrupt memory
	malloc_zone_pressure_relief(default_zone, 0);
	malloc_zone_pressure_relief(NULL, 0);
}

static void
do_test(stack_logging_mode_type mode, boolean_t validate_stacks, boolean_t nano_allocator_enabled, boolean_t lite_mode_enabled)
{
	printf("do_test. stack_logging_mode_type=%d validate_stacks=%d nano_allocator_enabled=%d\n", (int) mode, (int) validate_stacks, (int) nano_allocator_enabled);
	
	malloc_zone_t *default_zone = malloc_default_zone();
	malloc_zone_t *default_purgeable_zone = malloc_default_purgeable_zone();
	
	char *ptr = malloc(10);
	
	malloc_zone_t *zone_from_ptr = malloc_zone_from_ptr(ptr);
	EXPECT_EQ(zone_from_ptr, default_zone, "malloc_zone_from_ptr:%p default_zone:%p\n", zone_from_ptr, default_zone);
		
	if (mode != stack_logging_mode_none) {
		test_introspection_functions(default_zone, nano_allocator_enabled);
		test_pressure_relief(default_zone);
		
		printf("turning on stack logging mode %d\n", (int) mode);
		turn_on_stack_logging(mode);
		
		// check to make sure returned default zone hasn't changed
		EXPECT_EQ(default_zone, malloc_default_zone(), "cached default zone:%p  malloc_default_zone():%p", default_zone, malloc_default_zone());
		EXPECT_EQ(default_purgeable_zone, malloc_default_purgeable_zone(), "cached default purgeable zone:%p  malloc_default_purgeable_zone():%p", default_purgeable_zone, malloc_default_purgeable_zone());
		
		malloc_zone_t *zone_from_ptr = malloc_zone_from_ptr(ptr);
		EXPECT_EQ(zone_from_ptr, default_zone, "malloc_zone_from_ptr:%p default_zone:%p\n", zone_from_ptr, default_zone);
		
		test_pressure_relief(default_zone);
	}
	
	test_introspection_functions(default_zone, nano_allocator_enabled);
	test_virtual_default_zone(default_zone, nano_allocator_enabled, lite_mode_enabled);
	
	// test to see if zone->size works on the ptr allocated at the beginning
	size_t ptr_size = default_zone->size(default_zone, ptr);
	EXPECT_TRUE(ptr_size > 0, "ptr_size=%d\n", (int) ptr_size);
	
	free(ptr);
	
	boolean_t lite_mode = lite_mode_enabled;
	
	if (validate_stacks) {
		kern_return_t ret = __mach_stack_logging_start_reading(mach_task_self(), __mach_stack_logging_shared_memory_address, &lite_mode);
		EXPECT_TRUE(ret == KERN_SUCCESS, "return from __mach_stack_logging_start_reading = %d", ret);
	}
	
	// test regular versions
	test_malloc(NULL, lite_mode, validate_stacks, false, false);
	test_calloc(NULL, lite_mode, validate_stacks, false, false);
	test_valloc(NULL, lite_mode, validate_stacks, false, false);
	test_realloc(NULL, lite_mode, validate_stacks, false, false);
	
	// test malloc_zone versions
	test_malloc(default_zone, lite_mode, validate_stacks, false, false);
	test_calloc(default_zone, lite_mode, validate_stacks, false, false);
	test_valloc(default_zone, lite_mode, validate_stacks, false, false);
	test_realloc(default_zone, lite_mode, validate_stacks, false, false);
	test_batch_malloc(default_zone, lite_mode, validate_stacks, false, false);
	test_memalign(default_zone, lite_mode, validate_stacks, false, false);
	
	// test zone-> versions
	// if not lite mode then don't validate stacks, as this goes behind the back of the standard recorder
	if (!lite_mode) {
		validate_stacks = false;
	}
	
	test_malloc(default_zone, lite_mode, validate_stacks, true, false);
	test_calloc(default_zone, lite_mode, validate_stacks, true, false);
	test_valloc(default_zone, lite_mode, validate_stacks, true, false);
	test_realloc(default_zone, lite_mode, validate_stacks, true, false);
	test_batch_malloc(default_zone, lite_mode, validate_stacks, true, false);
	test_memalign(default_zone, lite_mode, validate_stacks, true, false);
	
	test_malloc(default_zone, lite_mode, validate_stacks, false, true);
	test_calloc(default_zone, lite_mode, validate_stacks, false, true);
	test_valloc(default_zone, lite_mode, validate_stacks, false, true);
	test_realloc(default_zone, lite_mode, validate_stacks, false, true);
	test_batch_malloc(default_zone, lite_mode, validate_stacks, false, true);
	test_memalign(default_zone, lite_mode, validate_stacks, false, true);
	
	test_malloc(default_zone, lite_mode, validate_stacks, true, true);
	test_calloc(default_zone, lite_mode, validate_stacks, true, true);
	test_valloc(default_zone, lite_mode, validate_stacks, true, true);
	test_realloc(default_zone, lite_mode, validate_stacks, true, true);
	test_batch_malloc(default_zone, lite_mode, validate_stacks, true, true);
	test_memalign(default_zone, lite_mode, validate_stacks, true, true);
	
	test_malloc_zone_functions(default_zone);
	
	ptr = malloc(10);
	zone_from_ptr = malloc_zone_from_ptr(ptr);
	EXPECT_EQ(zone_from_ptr, default_zone, "malloc_zone_from_ptr:%p default_zone:%p\n", zone_from_ptr, default_zone);

	if (mode != stack_logging_mode_none) {
		turn_off_stack_logging();
	}
	
	zone_from_ptr = malloc_zone_from_ptr(ptr);
	EXPECT_EQ(zone_from_ptr, default_zone, "malloc_zone_from_ptr:%p default_zone:%p\n", zone_from_ptr, default_zone);
	
	free(ptr);
	
	test_pressure_relief(default_zone);
	
	// check that the default zone hasn't changed after turning off stack logging
	EXPECT_EQ(default_zone, malloc_default_zone(), "cached default zone:%p  malloc_default_zone():%p", default_zone, malloc_default_zone());
	EXPECT_EQ(default_purgeable_zone, malloc_default_purgeable_zone(), "cached default purgeable zone:%p  malloc_default_purgeable_zone():%p", default_purgeable_zone, malloc_default_purgeable_zone());

	if (mode != stack_logging_mode_lite) {
		// if lite mode was turned on and then turned off, the zone is still around but allocations will not be done in the lite zone
		// so the enumerator will not find them - that's expected. This is similar to the situation where the nano zone is the default allocator
		// but some of the allocations occur in the helper zone, and calling the nano zone enumerator won't find these either.
		// This test uses small enough allocations that the nano zone always handles the allocations so we can test in that case.
		test_virtual_default_zone(default_zone, nano_allocator_enabled, lite_mode_enabled);
	}
}


#if DARWINTEST

T_DECL(msl_test_full_runtime_no_nano, "Test full mode of malloc stack logging enabled during runtime - not using nano allocator", T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_all, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_malloc_runtime_no_nano, "Test malloc mode of malloc stack logging enabled during runtime - not using nano allocator", T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_malloc, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_vm_runtime_no_nano, "Test vm mode of malloc stack logging enabled during runtime - not using nano allocator", T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = false;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_vm, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_lite_runtime_no_nano, "Test lite mode of malloc stack logging enabled during runtime - not using nano allocator", T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = true;
	
	do_test(stack_logging_mode_lite, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_full_atstart_no_nano, "Test full mode of malloc stack logging enabled at start - not using nano allocator", T_META_ENVVAR("MallocStackLogging=1"), T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_malloc_atstart_no_nano, "Test malloc mode of malloc stack logging enabled at start - not using nano allocator", T_META_ENVVAR("MallocStackLogging=malloc"), T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_vm_atstart_no_nano, "Test vm mode of malloc stack logging enabled at start - not using nano allocator", T_META_ENVVAR("MallocStackLogging=vm"), T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = false;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_lite_atstart_no_nano, "Test lite mode of malloc stack logging enabled at start - not using nano allocator", T_META_ENVVAR("MallocStackLogging=lite"), T_META_ENVVAR("MallocNanoZone=0"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = false;
	boolean_t lite_mode_enabled = true;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_full_runtime_with_nano, "Test full mode of malloc stack logging enabled during runtime - using nano allocator", T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_all, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_malloc_runtime_with_nano, "Test malloc mode of malloc stack logging enabled during runtime - using nano allocator", T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_malloc, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_vm_runtime_with_nano, "Test vm mode of malloc stack logging enabled during runtime - using nano allocator", T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = false;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_vm, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_lite_runtime_with_nano, "Test lite mode of malloc stack logging enabled during runtime - using nano allocator", T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = true;
	
	do_test(stack_logging_mode_lite, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_full_atstart_with_nano, "Test full mode of malloc stack logging enabled at start - using nano allocator", T_META_ENVVAR("MallocStackLogging=1"), T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_malloc_atstart_with_nano, "Test malloc mode of malloc stack logging enabled at start - using nano allocator", T_META_ENVVAR("MallocStackLogging=malloc"), T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_vm_atstart_with_nano, "Test vm mode of malloc stack logging enabled at start - using nano allocator", T_META_ENVVAR("MallocStackLogging=vm"), T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = false;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = false;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_lite_atstart_with_nano, "Test lite mode of malloc stack logging enabled at start - using nano allocator", T_META_ENVVAR("MallocStackLogging=lite"), T_META_ENVVAR("MallocNanoZone=1"), T_META_CHECK_LEAKS(NO))
{
	boolean_t validate_stacks = true;
	boolean_t nano_allocator_enabled = true;
	boolean_t lite_mode_enabled = true;
	
	do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
}

T_DECL(msl_test_serialize_uniquing_table, "Test that that stack uniquing table can be serialized, deserialized and read", T_META_ENVVAR("MallocStackLogging=lite"))
{
	uintptr_t *foo = malloc(sizeof(uintptr_t));
	T_ASSERT_NOTNULL(foo, "malloc");

	uint64_t stackid = foo[1];

	mach_vm_address_t frames1[STACK_LOGGING_MAX_STACK_SIZE];
	uint32_t count1;

	kern_return_t kr;

	boolean_t lite_mode;
	kr  = __mach_stack_logging_start_reading(mach_task_self(), __mach_stack_logging_shared_memory_address, &lite_mode);
	T_ASSERT_MACH_SUCCESS(kr, "start reading");

	kr = __mach_stack_logging_frames_for_uniqued_stack(mach_task_self(), stackid, frames1, STACK_LOGGING_MAX_STACK_SIZE, &count1);
	T_ASSERT_MACH_SUCCESS(kr, "get frames");
	T_ASSERT_TRUE(count1 > 0, "frames not empty");

	struct backtrace_uniquing_table *table = __mach_stack_logging_copy_uniquing_table(mach_task_self());
	T_ASSERT_NOTNULL(table, "get a copy of the uniquing table");

	mach_vm_size_t size = 0;
	void *serialized = __mach_stack_logging_uniquing_table_serialize(table, &size);
	T_ASSERT_NOTNULL(serialized, "serialize the table");

	__mach_stack_logging_uniquing_table_release(table);
	table = NULL;

	table = __mach_stack_logging_uniquing_table_copy_from_serialized(serialized, size);
	T_ASSERT_NOTNULL(table, "deserialize the table");

	kr = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)serialized, size);
	T_ASSERT_MACH_SUCCESS(kr, "deallocate buffer");

	mach_vm_address_t frames2[STACK_LOGGING_MAX_STACK_SIZE];
	uint32_t count2;

	kr = __mach_stack_logging_uniquing_table_read_stack(table, stackid, frames2, &count2, STACK_LOGGING_MAX_STACK_SIZE);
	T_ASSERT_MACH_SUCCESS(kr, "get frames gain");


	T_ASSERT_EQ(count1, count2, "frame counts match");
	T_ASSERT_EQ(0, memcmp(frames1, frames2, sizeof(mach_vm_address_t) * count1), "frames match");

	__mach_stack_logging_uniquing_table_release(table);
	free(foo);
	__mach_stack_logging_stop_reading(mach_task_self());
}


#else

int
main(int argc, const char * argv[])
{
	boolean_t nano_allocator_enabled = true;
	boolean_t validate_stacks = false;
	boolean_t lite_mode_enabled = false;
	
	char *nano_zone = getenv("MallocNanoZone");
	if (nano_zone) {
		if (strcmp(nano_zone, "0") == 0) {
			nano_allocator_enabled = false;
		}
	}
	
	// get the mode from the environment
	char *mode = getenv("MallocStackLogging");
	
	if (!mode) {
		stack_logging_mode_type mode_type = stack_logging_mode_none;
		
		mode = getenv("MallocStackLoggingMode");
		
		if (mode) {
			if (strcmp(mode, "all") == 0) {
				mode_type = stack_logging_mode_all;
				validate_stacks = true;
			} else if (strcmp(mode, "vm") == 0) {
				mode_type = stack_logging_mode_vm;
				validate_stacks = false;
			} else if (strcmp(mode, "malloc") == 0) {
				mode_type = stack_logging_mode_malloc;
				validate_stacks = true;
			} else if (strcmp(mode, "lite") == 0) {
				mode_type = stack_logging_mode_lite;
				validate_stacks = true;
				lite_mode_enabled = true;
			} else if (strcmp(mode, "none") == 0) {
				mode_type = stack_logging_mode_none;
				validate_stacks = false;
			}
		}
		do_test(mode_type, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
	} else {
		// stack logging already turned on, so don't pass in a mode to dynamically enable
		if (strcmp(mode, "lite") == 0) {
			lite_mode_enabled = true;
			validate_stacks = true;
		} else if (strcmp(mode, "vm") == 0) {
			lite_mode_enabled = false;
			validate_stacks = false;
		} else {
			lite_mode_enabled = false;
			validate_stacks = true;
		}
		
		do_test(stack_logging_mode_none, validate_stacks, nano_allocator_enabled, lite_mode_enabled);
	}
	
	PAUSE("At end of test. Run leaks now if desired.\n");
	
	return 0;
}

#endif
