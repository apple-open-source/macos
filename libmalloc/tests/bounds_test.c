#include <darwintest.h>
#include <malloc/malloc.h>
#include <malloc_private.h>
#include <stdlib.h>
#include <../src/internal.h>

T_DECL(bounds_sanity, "Pointer Bounds Sanity Check")
{
	size_t size = rand() % 1024;
	printf("Allocating %zu bytes...", size);
	void *ptr = malloc(size);
	T_EXPECT_NOTNULL(ptr, "allocation succeeded");
	T_EXPECT_LE(size, malloc_size(ptr), "requested size smaller or equal to actual size");
	size = rand() % 1024;
	printf("Reallocating %zu bytes...", size);
	ptr = realloc(ptr, size);
	T_EXPECT_NOTNULL(ptr, "reallocation succeeded");
	T_EXPECT_LE(size, malloc_size(ptr), "requested size smaller or equal to actual size");
	free(ptr);
	size = rand() % 1024;
	printf("Zero allocating %zu bytes...", size);
	ptr = calloc(1, size);
	T_EXPECT_NOTNULL(ptr, "zero allocation succeeded");
	T_EXPECT_LE(size, malloc_size(ptr), "requested size smaller or equal to actual size");
	free(ptr);
}
