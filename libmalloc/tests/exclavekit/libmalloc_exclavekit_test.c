#include <microtest/darwintest.h>
#include <stdlib.h>
#include <unistd.h>

/* zone functions */

T_DECL(malloc_default_zone, "returns a non-null default zone") {
    malloc_zone_t *dz = malloc_default_zone();
    T_EXPECT_NOTNULL(dz);
}

T_DECL(malloc_zone_from_ptr, "returns a pointer from the default zone") {
    void *ptr = malloc(1);
    T_EXPECT_NOTNULL(ptr);
    T_EXPECT_EQ(malloc_zone_from_ptr(ptr), malloc_default_zone());
    free(ptr);
}

/* allocation functions */

T_DECL(malloc_size, "returns a valid allocation size") {
    const size_t sz = 1;
    void *ptr = malloc(sz);
    T_EXPECT_NOTNULL(ptr);
    T_EXPECT_GE(malloc_size(ptr), sz);
    free(ptr);
}

T_DECL(malloc, "returns a valid allocation") {
    void *ptr = malloc(1);
    T_EXPECT_NOTNULL(ptr);
    free(ptr);
}

T_DECL(calloc, "returns a valid allocation") {
    void *ptr = calloc(1, 1);
    T_EXPECT_NOTNULL(ptr);
    free(ptr);
}

T_DECL(valloc, "returns a page-aligned allocation") {
    void *ptr = valloc(1);
    T_EXPECT_NOTNULL(ptr);
    T_EXPECT_EQ(((uintptr_t)ptr) % getpagesize(), 0ul);
    free(ptr);
}

T_DECL(realloc, "returns a reallocated allocation") {
    void *ptr = malloc(1);
    T_EXPECT_NOTNULL(ptr);
    void *new_ptr = realloc(ptr, 2);
    T_EXPECT_NOTNULL(new_ptr);
    free(new_ptr);
}

T_DECL(posix_memalign, "returns an aligned allocation") {
    void *ptr = NULL;
    T_EXPECT_EQ(posix_memalign(&ptr, 1, 16), 0);
    T_EXPECT_NOTNULL(ptr);
    T_EXPECT_EQ(((uintptr_t)ptr) % 16, 0ul);
    free(ptr);
}
