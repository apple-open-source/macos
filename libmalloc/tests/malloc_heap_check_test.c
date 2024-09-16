//
//  malloc_heap_check_test.c
//  libsystem_malloc
//
//  Created by Kim Topley on 4/26/18.
//
#include <stdlib.h>
#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_XZONE, T_META_TAG_VM_NOT_PREFERRED);

static void
run_heap_test(int iterations)
{
	const int MAX_POINTERS = 1024;
	void **pointers = (void **)calloc(MAX_POINTERS, sizeof(void *));
	for (int iteration = 0; iteration < iterations; iteration++) {
		int index = rand() % MAX_POINTERS;
		size_t size = 1 << (rand() % 20);
		if (pointers[index]) {
			if (rand() % 4 == 0) {
				pointers[index] = realloc(pointers[index], size);
				continue;
			}
			free(pointers[index]);
			pointers[index] = NULL;
		}
		void * pointer = malloc(size);
		T_QUIET; T_ASSERT_NOTNULL(pointer, "Allocation failed for %llu\n",
				(uint64_t)size);
		pointers[index] = pointer;
	}
	for (int i = 0; i < MAX_POINTERS; i++) {
		free(pointers[i]);
	}
	free(pointers);
}

T_DECL(malloc_heap_check_no_nano, "malloc heap checking (no Nano)",
	   T_META_ENVVAR("MallocCheckHeapStart=1"),
	   T_META_ENVVAR("MallocCheckHeapEach=1"),
	   T_META_ENVVAR("MallocNanoZone=0"))
{
	run_heap_test(100000);

	// If we get here without crashing, we pass.
	T_PASS("Heap check succeeded");
}
T_DECL(malloc_heap_check_nano, "malloc heap checking (with Nano)",
	   T_META_ENVVAR("MallocCheckHeapStart=1"),
	   T_META_ENVVAR("MallocCheckHeapEach=1"),
	   T_META_ENVVAR("MallocNanoZone=1"))
{
	run_heap_test(100000);

	// If we get here without crashing, we pass.
	T_PASS("Heap check succeeded");
}

T_DECL(malloc_simple_stack_logging, "Test MallocSimpleStackLogging=1",
		T_META_ENVVAR("MallocSimpleStackLogging=1"))
{
	run_heap_test(1000);

	T_PASS("Success");
}

