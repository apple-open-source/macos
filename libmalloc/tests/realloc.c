#include <darwintest.h>
#include <stdlib.h>
#include <stdint.h>

#include <malloc/malloc.h>

T_DECL(realloc_failure, "realloc failure", T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG("no_debug"), T_META_TAG_VM_PREFERRED)
{
	void *a = malloc(16);
	T_ASSERT_NOTNULL(a, "malloc(16)");
	void *b = realloc(a, SIZE_MAX - (1 << 17));
	errno_t error = errno;
	size_t a_sz = malloc_size(a);

	T_ASSERT_NULL(b, "realloc should fail");
	T_ASSERT_EQ(error, ENOMEM, "failure should have been ENOMEM");

	T_ASSERT_GT(a_sz, 0ul, "The original pointer should not have been freed");

	free(a);
}

T_DECL(reallocf_failure, "reallocf failure", T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG("no_debug"), T_META_TAG_VM_PREFERRED)
{
	// rdar://134443969: Avoid the tiny zone because it may madvise
	void *a = malloc(65536);
	T_ASSERT_NOTNULL(a, "malloc(65536)");
	void *b = reallocf(a, SIZE_MAX - (1 << 17));
	errno_t error = errno;
	size_t a_sz = malloc_size(a);

	T_ASSERT_NULL(b, "reallocf should fail");
	T_ASSERT_EQ(error, ENOMEM, "failure should have been ENOMEM");

	T_ASSERT_EQ(a_sz, 0ul, "The original pointer should have been freed");
}
