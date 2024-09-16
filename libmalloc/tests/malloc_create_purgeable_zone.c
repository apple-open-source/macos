//
//  malloc_create_purgeable_zone.c
//  libmalloc
//
//  Test for creating a purgeable zone while concurrently adding/removing other zones
//

#include <darwintest.h>
#include <stdlib.h>
#include <malloc/malloc.h>
#include <../src/internal.h>

#if TARGET_OS_WATCH
#define N_ZONE_CREATION_THREADS 4
#else // TARGET_OS_WATCH
#define N_ZONE_CREATION_THREADS 8
#endif // TARGET_OS_WATCH

extern malloc_zone_t **malloc_zones;

static void *
make_purgeable_thread(void *arg)
{
	T_LOG("enable PGM");
	malloc_zone_t *purgeable_zone = malloc_default_purgeable_zone();
	T_ASSERT_NOTNULL(purgeable_zone, "malloc_default_purgeable_zone returned NULL");

	return NULL;
}

static void *
zone_thread(void *arg)
{
	vm_size_t start_size = (vm_size_t)arg;
	while (1) {
		malloc_zone_t *zone = malloc_create_zone(start_size, 0);
		malloc_destroy_zone(zone);
	}
	return NULL;
}

T_DECL(malloc_create_purgeable_zone, "create a purgeable zone while constantly registering zones",
		T_META_TAG_XZONE,
		T_META_TAG_VM_NOT_PREFERRED)
{
	pthread_t zone_threads[N_ZONE_CREATION_THREADS];
	for (int i = 0; i < N_ZONE_CREATION_THREADS; i++) {
		vm_size_t zone_start_size = 1000;
		pthread_create(&zone_threads[i], NULL, zone_thread, (void *)zone_start_size);
	}

	usleep(50);

	pthread_t purgeable_thread;
	pthread_create(&purgeable_thread, NULL, make_purgeable_thread, NULL);
	pthread_join(purgeable_thread, NULL);

	usleep(500);

	T_PASS("finished without crashing");
}

T_DECL(malloc_purgeable_zone_helper,
		"Test that the purgeable zone uses the default xzone as its helper",
		T_META_TAG_XZONE_ONLY)
{
	malloc_zone_t *purgeable_zone = malloc_default_purgeable_zone();
	malloc_zone_t *default_zone = malloc_zones[0];

	T_ASSERT_GE(default_zone->version, 14, "Default zone should be xzone");
	T_ASSERT_TRUE(default_zone->introspect->zone_type == MALLOC_ZONE_TYPE_XZONE,
			"Default zone should be xzone");

	// Allocations smaller than 15k should be served by the default zone, while
	// allocations larger than 32k should be served by the purgeable zone

	void *small_ptr = malloc_zone_malloc(purgeable_zone, KiB(12));
	T_ASSERT_NOTNULL(small_ptr, NULL);
	void *large_ptr = malloc_zone_malloc(purgeable_zone, KiB(64));
	T_ASSERT_NOTNULL(large_ptr, NULL);

	T_ASSERT_EQ(purgeable_zone->size(purgeable_zone, small_ptr), 0,
			"Purgeable zone doesn't claim small allocation");
	T_ASSERT_NE(default_zone->size(default_zone, small_ptr), 0,
			"Default zone claims small allocation");
	T_ASSERT_NE(purgeable_zone->size(purgeable_zone, large_ptr), 0,
			"Purgeable zone claims large allocation");
	T_ASSERT_EQ(default_zone->size(default_zone, large_ptr), 0,
			"Default zone doesn't claim large allocation");

	free(small_ptr);
	free(large_ptr);
}

int
get_purgeable_state(mach_vm_address_t addr)
{
	int state = 0;
	kern_return_t kr = vm_purgable_control(mach_task_self(), addr, VM_PURGABLE_GET_STATE, &state);
	if (kr != KERN_SUCCESS) {
		return VM_PURGABLE_DENY;
	}
	return state;
}

T_DECL(malloc_purgeable_vm_size, "check that the size of a purgeable allocation matches the vm object size",
		T_META_TAG_XZONE)
{
	// All allocations that come out of the purgeable zone should be a VM object,
	// since we need to be able to tag it as purgeable or non-purgeable. The VM
	// object should start where the allocation starts, and should be the same
	// size
	//
	// All allocations that are made to the purgeable zone that are too small
	// to be VM objects should be passed off to the main malloc zone, to reduce
	// fragmentation.
	malloc_zone_t *purgeable_zone = malloc_default_purgeable_zone();
	size_t current_size = 1;
	size_t min_purgeable_size = 0;
	const size_t max_size = MiB(64);
	while (current_size <= max_size) {
		// iterate over all size classes, and try to make a purgeable allocation
		void *ptr = malloc_zone_malloc(purgeable_zone, current_size);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "Allocate 0x%zx bytes", malloc_size(ptr));
		size_t actual_size = purgeable_zone->size(purgeable_zone, ptr);
		if (actual_size == 0) {
			// This allocation is too small to be passed to the purgeable zone
			actual_size = malloc_size(ptr);

			mach_vm_address_t vm_addr = (mach_vm_address_t)ptr;
			T_QUIET; T_ASSERT_EQ(get_purgeable_state(vm_addr), VM_PURGABLE_DENY,
				"Allocation isn't purgeable");
			T_QUIET; T_ASSERT_EQ_ULONG(min_purgeable_size, 0UL,
					"Non-purgeable allocation (%zu) larger than the minimunm"
					"purgeable size (%zu)", current_size, min_purgeable_size);

			// Clients are still able to pass pointers from the main zone to
			// make_purgeable and make_nonpurgeable
			malloc_make_purgeable(ptr);
			int rc = malloc_make_nonpurgeable(ptr);
			T_QUIET; T_ASSERT_POSIX_ZERO(rc,
					"make_nonpurgeable succeeds on non-purgeable memory");
		} else {
			if (min_purgeable_size == 0) {
				min_purgeable_size = current_size;
			}
			// Make sure that the VM object backing this allocation is the
			// correct size
			mach_vm_address_t vm_addr = (mach_vm_address_t)ptr;
			mach_vm_size_t vm_size = 0;
			struct vm_region_extended_info vm_info = { 0 };
			mach_msg_type_number_t count = VM_REGION_EXTENDED_INFO_COUNT;
			mach_port_t name;
			kern_return_t kr = mach_vm_region(mach_task_self(), &vm_addr,
					&vm_size, VM_REGION_EXTENDED_INFO,
					(vm_region_info_t)&vm_info, &count, &name);
			T_QUIET; T_ASSERT_EQ(kr, KERN_SUCCESS, "Read region info");
			T_QUIET; T_ASSERT_EQ_ULLONG((mach_vm_address_t)ptr, vm_addr,
					"VM object starts at beginning of allocation");
			T_QUIET; T_ASSERT_EQ_ULLONG((mach_vm_size_t)actual_size, vm_size,
					"VM object has correct size");

			// The allocation should be nonvolatile when first allocated
			int purgable_state = get_purgeable_state(vm_addr);
			T_QUIET; T_ASSERT_EQ(purgable_state, VM_PURGABLE_NONVOLATILE,
					"Allocation starts non-volatile");

			// Make sure we can make the allocation volatile
			malloc_make_purgeable(ptr);
			purgable_state = get_purgeable_state(vm_addr);
			T_QUIET; T_ASSERT_TRUE(purgable_state == VM_PURGABLE_VOLATILE ||
					purgable_state == VM_PURGABLE_EMPTY,
					"Make allocation volatile");
		}
		T_QUIET; T_ASSERT_GE(actual_size, current_size,
				"Allocation is as large as requested");
		free(ptr);
		// Walk by block size up to 3MB, to ensure we try all possible
		// TINY/SMALL/LARGE size classes, and then step by 1MB
		if (actual_size <= MiB(3)) {
			current_size = actual_size + 1;
		} else {
			current_size = actual_size + MiB(1);
		}
	}

	T_LOG("Successfully checked all sizes, min purgeable size is %zu",
			min_purgeable_size);
}

T_DECL(purgeable_aligned_alloc,
		"Make an aligned purgeable allocation smaller than the minimum purgeable size",
		T_META_TAG_XZONE)
{
	void *ptr = malloc_zone_memalign(malloc_default_purgeable_zone(), KiB(64),
			KiB(32));
	T_ASSERT_NOTNULL(ptr, "Aligned allocation");
	T_ASSERT_GE(malloc_size(ptr), KiB(32), "Allocation is large enough");
	malloc_zone_free(malloc_default_purgeable_zone(), ptr);
}

T_DECL(purgeable_realloc, "Test reallocating pointers from the purgeable zone",
		T_META_TAG_XZONE)
{
	// Test reallocating from the purgeable zone to the purgeable zone
	void *ptr = malloc_zone_malloc(malloc_default_purgeable_zone(), KiB(64));
	T_ASSERT_NOTNULL(ptr, "Purgeable allocation");
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr),
			VM_PURGABLE_NONVOLATILE, "Allocation starts non-volatile");
	*((uint32_t*)ptr) = 0xcafebabe;
	ptr = malloc_zone_realloc(malloc_default_purgeable_zone(), ptr, KiB(128));
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr),
			VM_PURGABLE_NONVOLATILE, "Allocation non-volatile after realloc");
	T_ASSERT_EQ(*((uint32_t*)ptr), 0xcafebabe, "Memory preserved in realloc");
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr + KiB(64)),
			VM_PURGABLE_NONVOLATILE,
			"New part of allocation non-volatile after realloc");
	malloc_zone_free(malloc_default_purgeable_zone(), ptr);

	// Test reallocating from the main zone to the purgeable zone
	ptr = malloc_zone_malloc(malloc_default_purgeable_zone(), KiB(4));
	T_ASSERT_NOTNULL(ptr, "Purgeable allocation");
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr), VM_PURGABLE_DENY,
			"Allocation comes from main zone");
	ptr = malloc_zone_realloc(malloc_default_purgeable_zone(), ptr, KiB(64));
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr),
			VM_PURGABLE_NONVOLATILE, "Allocation non-volatile after realloc");
	malloc_zone_free(malloc_default_purgeable_zone(), ptr);

	// Test reallocating from the purgeable zone to the main zone
	ptr = malloc_zone_malloc(malloc_default_purgeable_zone(), KiB(64));
	T_ASSERT_NOTNULL(ptr, "Purgeable allocation");
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr),
			VM_PURGABLE_NONVOLATILE, "Allocation comes from purgeable zone");
	ptr = malloc_zone_realloc(malloc_default_purgeable_zone(), ptr, KiB(4));
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr),
			VM_PURGABLE_DENY, "Allocation non-purgeable after realloc");
	malloc_zone_free(malloc_default_purgeable_zone(), ptr);

	// Test reallocating huge chunks larger in the purgeable zone
	ptr = malloc_zone_malloc(malloc_default_purgeable_zone(), MiB(6));
	T_ASSERT_NOTNULL(ptr, "Purgeable huge allocation %p:", ptr);
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr),
			VM_PURGABLE_NONVOLATILE, "Allocation starts non-volatile");
	*((uint32_t*)ptr) = 0xcafebabe;
	ptr = malloc_zone_realloc(malloc_default_purgeable_zone(), ptr, MiB(8));
	T_ASSERT_NOTNULL(ptr, "Purgeable huge allocation after realloc:", ptr);
	T_ASSERT_EQ(*((uint32_t*)ptr), 0xcafebabe, "Memory preserved in realloc");
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr),
			VM_PURGABLE_NONVOLATILE, "Allocation is still non-volatile");
	T_ASSERT_EQ(get_purgeable_state((mach_vm_address_t)ptr + MiB(6)),
			VM_PURGABLE_NONVOLATILE, "Tail of allocation is non-volatile");
	malloc_zone_free(malloc_default_purgeable_zone(), ptr);

}
