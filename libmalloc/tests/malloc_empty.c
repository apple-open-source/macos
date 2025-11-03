#include <stdlib.h>
#include <darwintest.h>
#include <malloc_private.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED,
		T_META_TAG_ALL_ALLOCATORS);

T_DECL(empty_malloc_valid, "Zero size allocation returns valid pointer")
{
	void *ptr;

	ptr = malloc(0);
	T_ASSERT_NOTNULL(ptr, "Empty malloc returns pointer");
	free(ptr);

	ptr = calloc(1, 0);
	T_ASSERT_NOTNULL(ptr, "Empty calloc returns pointer");
	free(ptr);

	ptr = realloc(NULL, 0);
	T_ASSERT_NOTNULL(ptr, "Empty realloc returns pointer");
	free(ptr);

	ptr = aligned_alloc(sizeof(void *), 0);
	T_ASSERT_NOTNULL(ptr, "Empty aligned_alloc returns pointer");
	free(ptr);

	ptr = reallocf(NULL, 0);
	T_ASSERT_NOTNULL(ptr, "Empty reallocf returns pointer");
	free(ptr);

	ptr = valloc(0);
	T_ASSERT_NOTNULL(ptr, "Empty valloc returns pointer");
	free(ptr);

	int ret = posix_memalign(&ptr, sizeof(void *), 0);
	T_ASSERT_EQ(ret, 0, "posix_memalign returns success");
	T_ASSERT_NOTNULL(ptr, "Empty posix_memalign returns pointer");
	free(ptr);
}
