/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#include <knrheap/heap.h>
#include <malloc/malloc.h>

#include "../base.h"
#include "../malloc_common.h"

static size_t
knrheap_zone_malloc_size(malloc_zone_t *zone __unused, const void * __unsafe_indexable ptr)
{
    return liblibc_knrheap_malloc_size(ptr);
}

static void * __alloc_size(2)
knrheap_zone_malloc(malloc_zone_t *zone __unused, size_t sz)
{
    return liblibc_knrheap_malloc(sz);
}

static void * __alloc_size(2,3)
knrheap_zone_calloc(malloc_zone_t *zone __unused, size_t num_items, size_t sz)
{
    return liblibc_knrheap_calloc(num_items, sz);
}

static void * __alloc_size(2)
knrheap_zone_valloc(malloc_zone_t *zone __unused, size_t sz)
{
    return liblibc_knrheap_valloc(sz);
}

static void
knrheap_zone_free(malloc_zone_t *zone __unused, void * __unsafe_indexable ptr)
{
    return liblibc_knrheap_free(ptr);
}

static void * __alloc_size(3)
knrheap_zone_realloc(malloc_zone_t *zone __unused, void * __unsafe_indexable ptr, size_t sz)
{
    return liblibc_knrheap_realloc(ptr, sz);
}

static void
knrheap_zone_destroy(malloc_zone_t *zone __unused)
{

}

static void * __alloc_align(2) __alloc_size(3)
knrheap_zone_memalign(malloc_zone_t *zone __unused, size_t alignment, size_t sz)
{
    void * __unsafe_indexable ptr;
    if (!liblibc_knrheap_posix_memalign(&ptr, alignment, sz)) {
        return __unsafe_forge_bidi_indexable(void *, ptr, sz);
    }

    return NULL;
}

static boolean_t
knrheap_zone_claimed_address(malloc_zone_t *zone __unused, void * __unsafe_indexable ptr) {
    return liblibc_knrheap_claimed_address(ptr);
}

static void
knrheap_zone_try_free_default(malloc_zone_t *zone __unused, void * __unsafe_indexable ptr) {
    if (liblibc_knrheap_claimed_address(ptr)) {
        liblibc_knrheap_free(ptr);
    } else {
        find_zone_and_free(ptr, true);
    }
}

static size_t
knrheap_zone_malloc_good_size(malloc_zone_t *zone __unused, size_t sz) {
    return liblibc_knrheap_malloc_good_size(sz);
}

static boolean_t
knrheap_zone_check(malloc_zone_t *zone __unused) {
    return liblibc_knrheap_check();
}

static malloc_introspection_t knrheap_zone_instrospect = {
    .enumerator = NULL,
    .good_size = knrheap_zone_malloc_good_size,
    .check = knrheap_zone_check,
    .print = NULL,
    .log = NULL,
    .force_lock = NULL,
    .force_unlock = NULL,
    .statistics = NULL,
    .zone_locked = NULL,
    .enable_discharge_checking = NULL,
    .disable_discharge_checking = NULL,
    .discharge = NULL,
    .enumerate_discharged_pointers = NULL,
    .reinit_lock = NULL,
    .print_task = NULL,
    .task_statistics = NULL,
};

static malloc_zone_t knrheap_zone = {
    .reserved1 = NULL,
    .reserved2 = NULL,
    .size = knrheap_zone_malloc_size,
    .malloc = knrheap_zone_malloc,
    .calloc = knrheap_zone_calloc,
    .valloc = knrheap_zone_valloc,
    .free = knrheap_zone_free,
    .realloc = knrheap_zone_realloc,
    .destroy = knrheap_zone_destroy,
    .zone_name = NULL,
    .batch_malloc = NULL,
    .batch_free = NULL,
    .introspect = &knrheap_zone_instrospect,
    .version = 13,
    .memalign = knrheap_zone_memalign,
    .free_definite_size = NULL,
    .pressure_relief = NULL,
    .claimed_address = knrheap_zone_claimed_address,
    .try_free_default = knrheap_zone_try_free_default,
};

malloc_zone_t *
liblibc_knrheap_create_zone(void)
{
    return &knrheap_zone;
}
