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
#include "auto_zone.h"
#include "auto_impl_utilities.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <sys/time.h>
#include <sys/resource.h>
#define   RUSAGE_SELF     0

static auto_zone_t *gc_zone;

static void
__invalidate(void *ptr, void *context)
{
	printf("invalidating space %p\n", ptr);
}

static void
invalidate(auto_zone_t *zone, void* ptr, void *context)
{
	__invalidate(ptr, NULL);
}

static void
batch_invalidate(auto_zone_t *zone, auto_zone_foreach_object_t foreach, auto_zone_cursor_t cursor)
{
    printf("batch_invalidate!\n");
	foreach(cursor, __invalidate, NULL);
}

static boolean_t 
rb_gc_auto_should_collect(auto_zone_t *zone, const auto_statistics_t *stats, boolean_t about_to_create_new_region)
{
    return 0;
}

static void
init_gc_zone(void)
{
    auto_collection_control_t *control;
    
    gc_zone = auto_zone_create("sample collected zone");
    
    control = auto_collection_parameters(gc_zone);
    //control->invalidate = invalidate;
    //control->should_collect = rb_gc_auto_should_collect;   
    //control->ask_should_collect_frequency = -1;
    control->batch_invalidate = batch_invalidate;
    control->disable_generational = 1;
    control->log = AUTO_LOG_ALL;

    auto_zone_register_thread(gc_zone);

    //auto_collector_disable(gc_zone);
}

#define NOBJECTS 1000000
int *pointers[NOBJECTS];

static void
allocate_objects(long n)
{
    long i;
    //printf("allocate %ld objects\n", n);
    for (i = 0; i < n; i++) {
        pointers[i] = auto_zone_allocate_object(gc_zone, 100, AUTO_OBJECT_SCANNED, 1, 0);
        pointers[i][0] = 10;
    }
}

static void
free_allocated_objects(long n) {
    long i;
    for (i = 0; i < n; ++i)
        malloc_zone_free(gc_zone, pointers[i]);
}

static void
malloc_objects(long n)
{
    long i;
    //printf("malloc %ld pieces\n", n);
    for (i = 0; i < n; i++) {
        pointers[i] = malloc(100);
        pointers[i][0] = 10;
    }
}
static void
free_objects(long n) {
    long i;
    for (i = 0; i < n; ++i)
        free(pointers[i]);
}

static void
collect(void)
{
    auto_collect(gc_zone, AUTO_COLLECT_FULL_COLLECTION, NULL);
}

void testmalloc(long n) {
    malloc_objects(n);
    //free_objects(n);
}

void testauto(long n) {
    allocate_objects(n);
    //free_objects(n);
    //free_allocated_objects(n);
}

int main(int argc, char **argv)
{
    init_gc_zone();
    int do_auto = 1;
    int do_malloc = 1;
    if (argc > 1) {
        if (argv[1][0] == 'b') {
            ;
        }
        else if (argv[1][0] == 'm') {
            do_auto = 0;
        }
        else if (argv[1][0] == 'a') {
            do_malloc = 0;
        }
    }
    else {
    }
#if 1
    if (do_auto) testauto(1);
    if (do_malloc) testmalloc(1);
#endif
    auto_date_t overhead = auto_date_now();
    auto_date_t begin = auto_date_now();
    if (do_auto) testauto(NOBJECTS);
    auto_date_t middle = auto_date_now();
    if (do_malloc) testmalloc(NOBJECTS);
    auto_date_t zend = auto_date_now();

    printf("overhead %lld\n", begin-overhead);
    printf("begin %lld\nafter auto %lld\nafter malloc %lld\n\n", begin, middle, zend);
    
    printf("auto time   %lld\nmalloc time %lld\n", middle-begin, zend-middle);
    printf("sum         %lld\n", middle-begin + zend-middle);
    //collect();
    //pause();
	return 0;
}
