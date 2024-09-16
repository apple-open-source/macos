//
//  gmalloc.c
//  libmalloc
//
//  End-to-end integration tests for GuardMalloc.
//

#include <darwintest.h>

#include <malloc/malloc.h>
#include <stdlib.h>

T_GLOBAL_META(
	T_META_ENVVAR("DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib")
);

T_DECL(guard_malloc, "Allocate and free memory with guard malloc enabled", T_META_TAG_VM_NOT_PREFERRED)
{
	const size_t sz = (unsigned)rand();
	void *ptr = malloc(sz);
	T_EXPECT_NOTNULL(ptr, "pointer not NULL");
	T_EXPECT_GE(malloc_size(ptr), sz, "size is equal or larger");
	free(ptr);
}

