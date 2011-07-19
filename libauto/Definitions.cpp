/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
/*
    Definitions.cpp
    Global Definitions
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#include "Definitions.h"
#include "Environment.h"
#include "Zone.h"


namespace Auto {
    
    //
    // Shadow malloc zone functions
    //
    void *aux_malloc(size_t size) {
        ASSERTION(aux_zone);
        void *new_ptr = malloc_zone_malloc(aux_zone, size);
#if defined(DEBUG)
        if (Environment::print_allocs) malloc_printf("malloc_zone_malloc @%p %d\n", new_ptr, size);
#endif
        return new_ptr;
    }
    void *aux_calloc(size_t count, size_t size) {
        ASSERTION(aux_zone);
        void *new_ptr = malloc_zone_calloc(aux_zone, count, size);
#if defined(DEBUG)
        if (Environment::print_allocs) malloc_printf("malloc_zone_calloc @%p %d\n", new_ptr, count * size);
#endif
        return new_ptr;
    }
    void *aux_valloc(size_t size) {
        ASSERTION(aux_zone);
        void *new_ptr = malloc_zone_valloc(aux_zone, size);
#if defined(DEBUG)
        if (Environment::print_allocs) malloc_printf("malloc_zone_valloc @%p %d\n", new_ptr, size);
#endif
        return new_ptr;
    }
    void *aux_realloc(void *ptr, size_t size) {
        ASSERTION(aux_zone);
        void *new_ptr = malloc_zone_realloc(aux_zone, ptr, size);
#if defined(DEBUG)
        if (Environment::print_allocs) malloc_printf("malloc_zone_realloc @%p %d\n", new_ptr, size);
#endif
        return new_ptr;
    }
    void aux_free(void *ptr) {
        ASSERTION(aux_zone);
#if defined(DEBUG)
        if (Environment::print_allocs) malloc_printf("malloc_zone_free @%p\n", ptr);
#endif
        malloc_zone_free(aux_zone, ptr);
    }

    void uncommit_memory(void *address, usword_t size) {
        madvise(address, size, MADV_FREE_REUSABLE);
    }

    void commit_memory(void *address, usword_t size) {
        madvise(address, size, MADV_FREE_REUSE);
    }
};
