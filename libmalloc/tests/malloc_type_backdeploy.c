//
//  malloc_type_backdeploy.c
//  libsystem_malloc
//

#define __APPLE_BLEACH_SDK__

// rdar://127521206 (SDK sanitizer should not remove bridgeOS Availability machinery)
#ifndef __API_AVAILABLE_PLATFORM_bridgeos
#define __API_AVAILABLE_PLATFORM_bridgeos(x) bridgeos,introduced=x
#endif
#ifndef __API_UNAVAILABLE_PLATFORM_bridgeos
#define __API_UNAVAILABLE_PLATFORM_bridgeos bridgeos,unavailable
#endif

#pragma GCC diagnostic ignored "-Wgcc-compat"
#pragma GCC diagnostic ignored "-Wunguarded-availability-new"

#import <malloc/malloc.h>
#include <stdlib.h>
#include <darwintest.h>

#if defined(__LP64__) && !TARGET_OS_BRIDGE && !TARGET_OS_VISION && \
		(!defined(_MALLOC_TYPE_MALLOC_IS_BACKDEPLOYING) || \
		 !_MALLOC_TYPE_MALLOC_IS_BACKDEPLOYING)
#error "Test must be backdeploying!"
#endif

const size_t alloc_max = 2 * 1024 * 1024;

T_DECL(malloc_backdeploy, "malloc backdeploy",
		T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG_VM_PREFERRED)
{
	size_t sz = rand() % alloc_max;
	void *ptr = malloc(sz);
	T_ASSERT_NOTNULL(ptr, "malloc, size %lu", sz);

	ptr = realloc(ptr, sz + 1);
	T_ASSERT_NOTNULL(ptr, "reallocate");
	free(ptr);
}

T_DECL(malloc_options_backdeploy, "malloc with options backdeploy",
		T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG_VM_PREFERRED)
{
	size_t sz = ((rand() % alloc_max) &
			~(MALLOC_ZONE_MALLOC_DEFAULT_ALIGN - 1ul));
	void *ptr = malloc_zone_malloc_with_options(NULL,
			MALLOC_ZONE_MALLOC_DEFAULT_ALIGN, sz,
			MALLOC_ZONE_MALLOC_OPTION_NONE);
	T_ASSERT_NOTNULL(ptr, "malloc_zone_malloc_with_options, size %lu align %d",
			sz, MALLOC_ZONE_MALLOC_DEFAULT_ALIGN);
	free(ptr);
}

T_DECL(calloc_backdeploy, "calloc backdeploy",
		T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG_VM_PREFERRED)
{
	size_t sz = rand() % alloc_max;
	void *ptr = calloc(1, sz);
	T_ASSERT_NOTNULL(ptr, "calloc, size %lu", sz);
	free(ptr);
}

T_DECL(valloc_backdeploy, "valloc backdeploy",
		T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG_VM_PREFERRED)
{
	size_t sz = rand() % alloc_max;
	void *ptr = valloc(sz);
	T_ASSERT_NOTNULL(ptr, "valloc, size %lu", sz);
	free(ptr);
}

T_DECL(aligned_alloc_backdeploy, "aligned_alloc backdeploy",
		T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG_VM_PREFERRED)
{
	size_t sz = (rand() % alloc_max) & ~(sizeof(void *) - 1ul);
	void *ptr = aligned_alloc(sizeof(void *), sz);
	T_ASSERT_NOTNULL(ptr, "aligned_alloc, size %lu align %lu", sz,
			sizeof(void *));
	free(ptr);
}

T_DECL(posix_memalign_backdeploy, "posix_memalign backdeploy",
		T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG_VM_PREFERRED)
{
	void *ptr;
	size_t sz = rand() % alloc_max;
	int ret = posix_memalign(&ptr, sizeof(void *), sz);
	T_ASSERT_EQ(ret, 0, "posix_memalign, size %lu align %lu", sz,
			sizeof(void *));
	T_ASSERT_NOTNULL(ptr, "posix_memalign");
	free(ptr);
}
