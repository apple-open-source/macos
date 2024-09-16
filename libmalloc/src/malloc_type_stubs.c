#include <stdlib.h>
#include <malloc/malloc.h>

// Standard entry points (malloc/_malloc.h)

typedef unsigned long long malloc_type_id_t;

void *
malloc_type_malloc(size_t size, malloc_type_id_t type_id)
{
	return malloc(size);
}

void *
malloc_type_calloc(size_t count, size_t size, malloc_type_id_t type_id)
{
	return calloc(count, size);
}

void
malloc_type_free(void *ptr, malloc_type_id_t type_id)
{
	return free(ptr);
}

void *
malloc_type_realloc(void *ptr, size_t size, malloc_type_id_t type_id)
{
	return realloc(ptr, size);
}

void *
malloc_type_valloc(size_t size, malloc_type_id_t type_id)
{
	return valloc(size);
}

void *
malloc_type_aligned_alloc(size_t alignment, size_t size,
		malloc_type_id_t type_id)
{
	return aligned_alloc(alignment, size);
}

int
malloc_type_posix_memalign(void * __unsafe_indexable *memptr, size_t alignment, size_t size,
		malloc_type_id_t type_id)
{
	return posix_memalign(memptr, alignment, size);
}


// Zone entry points (malloc/malloc.h)

void *
malloc_type_zone_malloc(malloc_zone_t *zone, size_t size,
		malloc_type_id_t type_id)
{
	return malloc_zone_malloc(zone, size);
}

void *
malloc_type_zone_calloc(malloc_zone_t *zone, size_t count, size_t size,
		malloc_type_id_t type_id)
{
	return malloc_zone_calloc(zone, count, size);
}

void
malloc_type_zone_free(malloc_zone_t *zone, void *ptr, malloc_type_id_t type_id)
{
	return malloc_zone_free(zone, ptr);
}

void *
malloc_type_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size,
		malloc_type_id_t type_id)
{
	return malloc_zone_realloc(zone, ptr, size);
}

void *
malloc_type_zone_valloc(malloc_zone_t *zone, size_t size,
		malloc_type_id_t type_id)
{
	return malloc_zone_valloc(zone, size);
}

void *
malloc_type_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size,
		malloc_type_id_t type_id)
{
	return malloc_zone_memalign(zone, alignment, size);
}
