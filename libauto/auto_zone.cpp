/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
    auto_zone.cpp
    Automatic Garbage Collection
    Copyright (c) 2002-2009 Apple Inc. All rights reserved.
 */

#include "auto_zone.h"
#include "auto_impl_utilities.h"
#include "auto_weak.h"
#include "AutoReferenceRecorder.h"
#include "auto_trace.h"
#include "auto_dtrace.h"
#include "AutoZone.h"
#include "AutoLock.h"
#include "AutoInUseEnumerator.h"
#include "AutoThreadLocalCollector.h"
#include "auto_tester/auto_tester.h"

#include <stdlib.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <Block.h>
#include <notify.h>

#define USE_INTERPOSING 0
#define LOG_TIMINGS 0

#if USE_INTERPOSING
#include <mach-o/dyld-interposing.h>
#endif

using namespace Auto;

/*********  Globals     ************/

#ifdef AUTO_TESTER
AutoProbeFunctions *auto_probe_functions = NULL;
#endif

bool auto_set_probe_functions(AutoProbeFunctions *functions) {
#ifdef AUTO_TESTER
    auto_probe_functions = functions;
    return true;
#else
    return false;
#endif
}

static void __auto_trace_collection_begin__(auto_zone_t *zone, boolean_t generational) {}
static void __auto_trace_collection_end__(auto_zone_t *zone, boolean_t generational, size_t objectsReclaimed, size_t bytesReclaimed, size_t totalObjectsInUse, size_t totalBytesInUse) {}
static auto_trace_collection_callouts auto_trace_callouts = { sizeof(auto_trace_collection_callouts), __auto_trace_collection_begin__, __auto_trace_collection_end__ };

void auto_trace_collection_set_callouts(auto_trace_collection_callouts *new_callouts) {
    if (new_callouts->size == sizeof(auto_trace_collection_callouts)) {
        auto_trace_callouts = *new_callouts;
    } else {
        malloc_printf("auto_trace_collection_set_callouts() called with incompatible size (ignored)\n");
    }
}

// Reference count logging support for ObjectAlloc et. al.
void (*__auto_reference_logger)(uint32_t eventtype, void *ptr, uintptr_t data) = NULL;

/*********  Parameters  ************/

#define VM_COPY_THRESHOLD       (40 * 1024)

/*********  Functions ****************/

#if LOG_TIMINGS

#define LOG_ALLOCATION_THRESHOLD 64*1024

static void log_allocation_threshold(auto_date_t time, size_t allocated, size_t finger);
static void log_collection_begin(auto_date_t time, size_t allocated, size_t finger, bool isFull);
static void log_collection_end(auto_date_t time, size_t allocated, size_t finger, size_t recovered);

#endif LOG_TIMINGS

/*********  Allocation Meter ************/

#if defined(AUTO_ALLOCATION_METER)

static bool allocate_meter_inited = false;
static bool allocate_meter = false;
static double allocate_meter_interval = 1.0;
static double allocate_meter_start_time = 0.0;
static double allocate_meter_report_time = 0.0;
static double allocate_meter_count = 0;
static double allocate_meter_total_time = 0.0;

static double nano_time() {
    static mach_timebase_info_data_t timebase_info;
    static double scale = 1.0;
    static unsigned long long delta; 
    if (!timebase_info.denom) {
        mach_timebase_info(&timebase_info);
        scale = ((double)timebase_info.numer / (double)timebase_info.denom) * 1.0E-9;
        delta = mach_absolute_time();
    }
    return (double)(mach_absolute_time() - delta) * scale;
}

static void allocate_meter_init() {
  if (!allocate_meter_inited) {
    const char *env_str = getenv("AUTO_ALLOCATION_METER");
    allocate_meter = env_str != NULL;
    allocate_meter_interval = allocate_meter ? atof(env_str) : 1.0;
    if (allocate_meter_interval <= 0.0) allocate_meter_interval = 1.0;
    allocate_meter_inited = true;
  }
}

static unsigned long long allocate_meter_average() {
  double daverage = allocate_meter_total_time / allocate_meter_count;
  unsigned long long iaverage = (unsigned long long)(daverage * 1000000000.0);
  allocate_meter_count = 1;
  allocate_meter_total_time = daverage;
  return iaverage;
}

static void allocate_meter_start() {
  allocate_meter_start_time = nano_time();
  if (allocate_meter_count == 0.0)
    allocate_meter_report_time = allocate_meter_start_time + allocate_meter_interval;
}

static void allocate_meter_stop() {
  double stoptime = nano_time();
  allocate_meter_count++;
  allocate_meter_total_time += stoptime - allocate_meter_start_time;
  if (stoptime > allocate_meter_report_time) {
    malloc_printf("%u nanosecs/alloc\n", (unsigned)allocate_meter_average());
    allocate_meter_report_time = stoptime + allocate_meter_interval;
  }
}

#endif

/*********  Zone callbacks  ************/


boolean_t auto_zone_is_finalized(auto_zone_t *zone, const void *ptr) {
    Zone *azone = (Zone *)zone;
    // detects if the specified pointer is about to become garbage
    return (ptr && azone->block_is_garbage((void *)ptr));
}

static void auto_collect_internal(Zone *zone, boolean_t generational) {
    size_t garbage_count;
    vm_address_t *garbage;

    // Avoid simultaneous collections.
    if (!OSAtomicCompareAndSwap32(0, 1, &zone->collector_collecting)) return;

    zone->reset_threshold(); // clear threshold.  Till now we might back off & miss a needed collection.
     
    auto_date_t start = auto_date_now();
    Statistics &zone_stats = zone->statistics();
    
    AUTO_PROBE(auto_probe_begin_heap_scan(generational));
    
    // bound the bottom of the stack.
    vm_address_t stack_bottom = auto_get_sp();
    if (zone->control.disable_generational) generational = false;
    auto_trace_callouts.auto_trace_collection_begin((auto_zone_t*)zone, generational); // XXX Collection checker.
	GARBAGE_COLLECTION_COLLECTION_BEGIN((auto_zone_t*)zone, generational ? AUTO_TRACE_GENERATIONAL : AUTO_TRACE_FULL);
#if LOG_TIMINGS
    log_collection_begin(start, zone_stats.size(), zone_stats.allocated(), generational);
#endif
    zone->set_state(scanning);
    
    Thread &collector_thread = zone->register_thread();
    collector_thread.set_in_collector(true);
    zone->collect_begin();

    auto_date_t scan_end;
    zone->collect((bool)generational, (void *)stack_bottom, &scan_end);

    PointerList &list = zone->garbage_list();
    garbage_count = list.count();
    garbage = (vm_address_t *)list.buffer();

    AUTO_PROBE(auto_probe_end_heap_scan(garbage_count, garbage));
    
    auto_date_t enlivening_end = auto_date_now();
    auto_date_t finalize_end;
    size_t bytes_freed = 0;

    // note the garbage so the write-barrier can detect resurrection
	GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)zone, AUTO_TRACE_FINALIZING_PHASE);
    zone->set_state(finalizing);
    size_t block_count = garbage_count, byte_count = 0;
    zone->invalidate_garbage(garbage_count, garbage);
	GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)zone, AUTO_TRACE_FINALIZING_PHASE, (uint64_t)block_count, (uint64_t)byte_count);
    zone->set_state(reclaiming);
    finalize_end = auto_date_now();
	GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)zone, AUTO_TRACE_SCAVENGING_PHASE);
    bytes_freed = zone->free_garbage(generational, garbage_count, garbage, block_count, byte_count);
    zone->clear_zombies();
	GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)zone, AUTO_TRACE_SCAVENGING_PHASE, (uint64_t)block_count, (uint64_t)bytes_freed);

    zone->collect_end();
    collector_thread.set_in_collector(false);

    intptr_t after_in_use = zone_stats.size();
    intptr_t after_allocated = after_in_use + zone_stats.unused();
    auto_date_t collect_end = auto_date_now();
    
	GARBAGE_COLLECTION_COLLECTION_END((auto_zone_t*)zone, (uint64_t)garbage_count, (uint64_t)bytes_freed, (uint64_t)zone_stats.count(), (uint64_t)zone_stats.size());
	auto_trace_callouts.auto_trace_collection_end((auto_zone_t*)zone, generational, garbage_count, bytes_freed, zone_stats.count(), zone_stats.size());
#if LOG_TIMINGS
    log_collection_end(collect_end, after_in_use, after_allocated, bytes_freed);
#endif

    zone->set_state(idle);
    zone->collector_collecting = 0;
    AUTO_PROBE(auto_probe_heap_collection_complete());

    // update collection part of statistics
    auto_statistics_t   *stats = &zone->stats;
    
    int which = generational ? 1 : 0;
    stats->num_collections[which]++;
    stats->bytes_in_use_after_last_collection[which] = after_in_use;
    stats->bytes_allocated_after_last_collection[which] = after_allocated;
    stats->bytes_freed_during_last_collection[which] = bytes_freed;
    stats->last_collection_was_generational = generational;
    
    auto_collection_durations_t *max = &stats->maximum[which];
    auto_collection_durations_t *last = &stats->last[which];
    auto_collection_durations_t *total = &stats->total[which];

    last->scan_duration = scan_end - start;
    last->enlivening_duration = enlivening_end - scan_end;
    last->finalize_duration = finalize_end - enlivening_end;;
    last->reclaim_duration = collect_end - finalize_end;
    last->total_duration = collect_end - start;
    
    // compute max individually (they won't add up, but we'll get max scan & max finalize split out
    if (max->scan_duration < last->scan_duration) max->scan_duration = last->scan_duration;
    if (max->enlivening_duration < last->enlivening_duration) max->enlivening_duration = last->enlivening_duration;
    if (max->finalize_duration < last->finalize_duration) max->finalize_duration = last->finalize_duration;
    if (max->reclaim_duration < last->reclaim_duration) max->reclaim_duration = last->reclaim_duration;
    if (max->total_duration < last->total_duration) max->total_duration = last->total_duration;

    total->scan_duration += last->scan_duration;
    total->enlivening_duration += last->enlivening_duration;
    total->finalize_duration += last->finalize_duration;
    total->reclaim_duration += last->reclaim_duration;
    total->total_duration += last->total_duration;

    if (zone->control.log & AUTO_LOG_COLLECTIONS)
        malloc_printf("%s: %s GC collected %lu objects (%lu bytes) (%lu bytes in use) %d usec "
                      "(%d + %d + %d + %d [scan + freeze + finalize + reclaim])\n", 
                      auto_prelude(), (generational ? "gen." : "full"),
                      garbage_count, bytes_freed, after_in_use,
                      (int)(collect_end - start), // total
                      (int)(scan_end - start), 
                      (int)(enlivening_end - scan_end), 
                      (int)(finalize_end - enlivening_end), 
                      (int)(collect_end - finalize_end));
}

extern "C" void auto_zone_stats(void);

static void auto_collect_with_mode(Zone *zone, auto_collection_mode_t mode) {
    if (mode & AUTO_COLLECT_IF_NEEDED) {
        if (!zone->threshold_reached())
            return;
    }
    bool generational = true, exhaustive = false;
    switch (mode & 0x3) {
      case AUTO_COLLECT_RATIO_COLLECTION:
        // enforce the collection ratio to keep the heap from getting too big.
        if (zone->collection_count++ >= zone->control.full_vs_gen_frequency) {
            zone->collection_count = 0;
            generational = false;
        }
        break;
      case AUTO_COLLECT_GENERATIONAL_COLLECTION:
        generational = true;
        break;
      case AUTO_COLLECT_FULL_COLLECTION:
        generational = false;
        break;
      case AUTO_COLLECT_EXHAUSTIVE_COLLECTION:
        exhaustive = true;
    }
    if (exhaustive) {
         // run collections until objects are no longer reclaimed.
        Statistics &stats = zone->statistics();
        usword_t count, collections = 0;
        //if (zone->control.log & AUTO_LOG_COLLECTIONS) malloc_printf("beginning exhaustive collections\n");
        do {
            count = stats.count();
            auto_collect_internal(zone, false);
        } while (stats.count() < count && ((Environment::exhaustive_collection_limit == 0) || (++collections < Environment::exhaustive_collection_limit)));
        //if (zone->control.log & AUTO_LOG_COLLECTIONS) malloc_printf("ending exhaustive collections\n");
    } else {
        auto_collect_internal(zone, generational);
    }
}

#if USE_DISPATCH_QUEUE

static void auto_collection_work(Zone *zone) {
    // inform other threads that collection has started.
    pthread_mutex_lock(&zone->collection_mutex);
    zone->collection_status_state = 1;
    pthread_mutex_unlock(&zone->collection_mutex);
    
    auto_collect_with_mode(zone, zone->collection_requested_mode);

    // inform blocked threads that collection has finished.
    pthread_mutex_lock(&zone->collection_mutex);
    zone->collection_requested_mode = 0;
    zone->collection_status_state = 0;
    pthread_cond_broadcast(&zone->collection_status);
    AUTO_PROBE(auto_probe_collection_complete());
    pthread_mutex_unlock(&zone->collection_mutex);
}

#else

static void *auto_collection_thread(void *arg) {
    Zone *zone = (Zone *)arg;
    if (zone->control.log & AUTO_LOG_COLLECTIONS) auto_zone_stats();
    pthread_mutex_lock(&zone->collection_mutex);
    for (;;) {
        uint32_t mode_flags;
        while ((mode_flags = zone->collection_requested_mode) == 0) {
            // block until explicity requested to collect.
            pthread_cond_wait(&zone->collection_requested, &zone->collection_mutex);
        }

        // inform other threads that collection has started.
        zone->collection_status_state = 1;
        // no clients
        // pthread_cond_broadcast(&zone->collection_status);
        pthread_mutex_unlock(&zone->collection_mutex);

        auto_collect_with_mode(zone, zone->collection_requested_mode);
        
        // inform blocked threads that collection has finished.
        pthread_mutex_lock(&zone->collection_mutex);
        zone->collection_requested_mode = 0;
        zone->collection_status_state = 0;
        pthread_cond_broadcast(&zone->collection_status);
        AUTO_PROBE(auto_probe_collection_complete());
    }
    
    return NULL;
}

#endif /* USE_DISPATCH_QUEUE */

//
// Primary external entry point for collection
//
void auto_collect(auto_zone_t *zone, auto_collection_mode_t mode, void *collection_context) {
    Zone *azone = (Zone *)zone;

    // The existence of this probe allows the test machinery to tightly control which collection requests are executed. See AutoTestScript.h for more.
    AUTO_PROBE(auto_probe_auto_collect(mode));
    if (azone->collector_disable_count)
        return;
    // see if now would be a good time to run a Thread Local collection (TLC).
    const bool collectIfNeeded = ((mode & AUTO_COLLECT_IF_NEEDED) != 0);
    Thread *thread = azone->current_thread();
    if (thread) {
        // if a thread is calling calls us with the AUTO_COLLECT_IF_NEEDED mode, assume it is safe to run finalizers immediately.
        // otherwise, finalizers will be called asynchronously using the collector dispatch queue.
        const bool exhaustiveCollection = (mode & AUTO_COLLECT_EXHAUSTIVE_COLLECTION) == AUTO_COLLECT_EXHAUSTIVE_COLLECTION;
        const bool finalizeNow = collectIfNeeded || exhaustiveCollection;
        if (exhaustiveCollection || ThreadLocalCollector::should_collect(azone, *thread, finalizeNow)) {
            ThreadLocalCollector tlc(azone, (void *)auto_get_sp(), *thread);
            tlc.collect(finalizeNow);
        }
    }
    if (collectIfNeeded && !azone->threshold_reached())
        return;
    if (azone->collector_collecting)
        return;  // if already running the collector bail early
    if (azone->multithreaded) {
        // request a collection by setting the requested flags, and signaling the collector thread.
        pthread_mutex_lock(&azone->collection_mutex);
        if (azone->collection_requested_mode) {
            // request already in progress
        }
        else {
            azone->collection_requested_mode = mode | 0x1000;         // force non-zero value
#if USE_DISPATCH_QUEUE
            // dispatch a call to auto_collection_work() to begin a collection.
            dispatch_async(azone->collection_queue, azone->collection_block);
#else
            // wake up the collector, telling it to begin a collection.
            pthread_cond_signal(&azone->collection_requested);
#endif
        }
        if (mode & AUTO_COLLECT_SYNCHRONOUS) {
            // wait for the collector to finish the current collection. wait at most 1 second, to avoid deadlocks.
            const struct timespec one_second = { 1, 0 };
            pthread_cond_timedwait_relative_np(&azone->collection_status, &azone->collection_mutex, &one_second);
        }
        pthread_mutex_unlock(&azone->collection_mutex);
    }
    else {
        auto_collect_with_mode(azone, mode);
    }
}

static inline size_t auto_size(auto_zone_t *zone, const void *ptr) {
    Zone *azone = (Zone *)zone;
    return azone->is_block((void *)ptr) ? azone->block_size((void *)ptr) : 0L;
}

boolean_t auto_zone_is_valid_pointer(auto_zone_t *zone, const void *ptr) {
    Zone* azone = (Zone *)zone;
    boolean_t result;
    result = azone->is_block((void *)ptr);
    return result;
}

size_t auto_zone_size(auto_zone_t *zone, const void *ptr) {
    return auto_size(zone, ptr);
}

const void *auto_zone_base_pointer(auto_zone_t *zone, const void *ptr) {
    Zone *azone = (Zone *)zone;
    const void *base = (const void *)azone->block_start((void *)ptr);
    return base;
}

#if DEBUG
void *WatchPoint = (void*)0xFFFFFFFF;
void blainer() {
    sleep(0);
}
#endif


static inline void *auto_malloc(auto_zone_t *zone, size_t size) {
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    void *result = azone->block_allocate(thread, size, AUTO_MEMORY_UNSCANNED, 0, true);
    return result;
}

static void auto_free(auto_zone_t *azone, void *ptr) {
    if (ptr == NULL) return; // XXX_PCB don't mess with NULL pointers.
    Zone *zone = (Zone *)azone;
    unsigned    refcount = zone->block_refcount(ptr);
    if (refcount != 1)
        malloc_printf("*** free() called with %p with refcount %d\n", ptr, refcount);
    zone->block_deallocate(ptr);
}

static void *auto_calloc(auto_zone_t *zone, size_t size1, size_t size2) {
    Zone *azone = (Zone *)zone;
    size1 *= size2;
    void *ptr;
    Thread &thread = azone->registered_thread();
    ptr = azone->block_allocate(thread, size1, AUTO_MEMORY_UNSCANNED, 1, true);
    return ptr;
}

static void *auto_valloc(auto_zone_t *zone, size_t size) {
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    void *result = azone->block_allocate(thread, auto_round_page(size), AUTO_MEMORY_UNSCANNED, 1, true);
    return result;
}

static boolean_t get_type_and_retain_count(Zone *zone, void *ptr, auto_memory_type_t *type, int *rc) {
    boolean_t is_block = zone->is_block(ptr);
    if (is_block) zone->block_refcount_and_layout(ptr, rc, type);
    return is_block;
}

static void *auto_realloc(auto_zone_t *zone, void *ptr, size_t size) {
    Zone *azone = (Zone*)zone;
    if (!ptr) return auto_malloc(zone, size);
    size_t old_size = auto_size(zone, ptr);

    // preserve the layout type, and retain count of the realloc'd object.
    auto_memory_type_t type; int rc = 0;
    if (!get_type_and_retain_count(azone, ptr, &type, &rc)) {
        auto_error(azone, "auto_realloc: can't get type or retain count, ptr (%p) from ordinary malloc zone?", ptr);
        // If we're here because someone used the wrong zone we should let them have what they intended.
        return malloc_zone_realloc(malloc_zone_from_ptr(ptr), ptr, size);
    }
    
    // malloc man page says to allocate a "minimum sized" object if size==0
    if (size == 0) size = allocate_quantum_small;
    
    if (old_size > size) {
        size_t delta = old_size - size;
        // When reducing the size check if the reduction would result in a smaller block being used. If not, reuse the same block.
        // We can reuse the same block if any of these are true:
        // 1) original is a small block, reduced by less than small quanta
        // 2) original is a medium block, new size is still medium, and reduced by less than medium quanta
        // 3) original is a large block, new size is still large, and block occupies the same number of pages
        if ((old_size <= allocate_quantum_medium && delta < allocate_quantum_small) ||
            (old_size <= allocate_quantum_large && size >= allocate_quantum_medium && delta < allocate_quantum_medium) ||
            (size > allocate_quantum_large && auto_round_page(old_size) == auto_round_page(size))) {
            // if the block is scanned, resizing smaller should clear the extra space
            if (type == AUTO_MEMORY_SCANNED)
                bzero(displace(ptr,size), old_size-size);
            return ptr;
        }
    }
    
    // We could here optimize realloc by adding a primitive for small blocks to try to grow in place
    // But given that this allocator is intended for objects, this is not necessary
    Thread &thread = azone->registered_thread();
    void *new_ptr = azone->block_allocate(thread, size, type, (type & AUTO_UNSCANNED) != AUTO_UNSCANNED, (rc != 0));
    auto_zone_write_barrier_memmove((auto_zone_t *)azone, new_ptr, ptr, MIN(size, old_size));
    
    if (rc != 0) azone->block_decrement_refcount(ptr); // don't forget to let go rdar://6593098
    
    // Don't bother trying to eagerly free old memory, even if it seems to be from malloc since,
    // well, that's just a hueristic that can be wrong.  In particular CF has on occasion bumped
    // the refcount of GC memory to guard against use in unregistered threads, and we don't know
    // how often or where this practice has spread. rdar://6063041

    return new_ptr;
}

static void auto_zone_destroy(auto_zone_t *zone) {
    Zone *azone = (Zone*)zone;
    auto_error(azone, "auto_zone_destroy:  %p", zone);
}

static kern_return_t auto_default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr) {
    *ptr = (void *)address;
    return KERN_SUCCESS;
}

static kern_return_t auto_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t zone_address, memory_reader_t reader, vm_range_recorder_t recorder) {
    kern_return_t  err;

    if (!reader) reader = auto_default_reader;
    
    // make sure the zone version numbers match.
    union {
        unsigned *version;
        void *voidStarVersion;
    } u;
    err = reader(task, zone_address + offsetof(malloc_zone_t, version), sizeof(unsigned), &u.voidStarVersion);
    if (err != KERN_SUCCESS || *u.version != AUTO_ZONE_VERSION) return KERN_FAILURE;
        
    InUseEnumerator enumerator(task, context, type_mask, zone_address, reader, recorder);
    err = enumerator.scan();

    return err;
}

static size_t auto_good_size(malloc_zone_t *azone, size_t size) {
    return ((Zone *)azone)->good_block_size(size);
}

unsigned auto_check_counter = 0;
unsigned auto_check_start = 0;
unsigned auto_check_modulo = 1;

static boolean_t auto_check(malloc_zone_t *zone) {
    if (! (++auto_check_counter % 10000)) {
        malloc_printf("%s: At auto_check counter=%d\n", auto_prelude(), auto_check_counter);
    }
    if (auto_check_counter < auto_check_start) return 1;
    if (auto_check_counter % auto_check_modulo) return 1;
    return 1;
}

static char *b2s(int bytes, char *buf, int bufsize) {
    if (bytes < 10*1024) {
        snprintf(buf, bufsize, "%dbytes", bytes);
    } else if (bytes < 10*1024*1024) {
        snprintf(buf, bufsize, "%dKB", bytes / 1024);
    } else {
        snprintf(buf, bufsize, "%dMB", bytes / (1024*1024));
    }
    return buf;
}

static void auto_zone_print(malloc_zone_t *zone, boolean_t verbose) {
    char    buf1[256];
    char    buf2[256];
    Zone             *azone = (Zone *)zone;
    auto_statistics_t   *stats = &azone->stats;
    printf("auto zone %p: in_use=%u  used=%s allocated=%s\n", azone, stats->malloc_statistics.blocks_in_use, b2s(stats->malloc_statistics.size_in_use, buf1, sizeof(buf1)), b2s(stats->malloc_statistics.size_allocated, buf2, sizeof(buf2)));
    if (verbose) azone->print_all_blocks();
}

static void auto_zone_log(malloc_zone_t *zone, void *log_address) {
}

// these force_lock() calls get called when a process calls fork(). we need to be careful not to be in the collector when this happens.

static void auto_zone_force_lock(malloc_zone_t *zone) {
    // if (azone->control.log & AUTO_LOG_UNUSUAL) malloc_printf("%s: auto_zone_force_lock\n", auto_prelude());
    // need to grab the allocation locks in each Admin in each Region
    // After we fork, need to zero out the thread list.
}

static void auto_zone_force_unlock(malloc_zone_t *zone) {
    // if (azone->control.log & AUTO_LOG_UNUSUAL) malloc_printf("%s: auto_zone_force_unlock\n", auto_prelude());
}

#if 0

these are buggy 

// copy, from the internals, the malloc_zone_statistics data wanted from the malloc_introspection API
static void auto_malloc_statistics_new(malloc_zone_t *zone, malloc_statistics_t *stats) {
    ((Zone *)zone)->malloc_statistics(stats);
}
static void auto_malloc_statistics_old(malloc_zone_t *zone, malloc_statistics_t *stats) {
    Zone *azone = (Zone *)zone;
    Statistics statistics;
    azone->statistics(statistics);
    stats->blocks_in_use = statistics.count();
    stats->size_in_use = statistics.size();
    stats->max_size_in_use = statistics.dirty_size();  // + aux_zone max_size_in_use ??
    stats->size_allocated = statistics.allocated();    // + aux_zone size_allocated ??
}
#endif


static void auto_malloc_statistics(malloc_zone_t *zone, malloc_statistics_t *stats) {
    Zone *azone = (Zone *)zone;
    Statistics &statistics = azone->statistics();
    stats->blocks_in_use = statistics.count();
    stats->size_in_use = statistics.size();
    stats->max_size_in_use = statistics.dirty_size();  // + aux_zone max_size_in_use ??
    stats->size_allocated = statistics.allocated();    // + aux_zone size_allocated ??
}

static boolean_t auto_malloc_zone_locked(malloc_zone_t *zone) {
    // this is called by malloc_gdb_po_unsafe, on behalf of GDB, with all other threads suspended.
    // we have to check to see if any of our spin locks or mutexes are held.
    return ((Zone *)zone)->is_locked();
}

/*********  Entry points    ************/

static struct malloc_introspection_t auto_zone_introspect = {
    auto_in_use_enumerator,
    auto_good_size,
    auto_check,
    auto_zone_print,
    auto_zone_log, 
    auto_zone_force_lock, 
    auto_zone_force_unlock,
    auto_malloc_statistics,
    auto_malloc_zone_locked
};

struct malloc_introspection_t auto_zone_introspection() {
    return auto_zone_introspect;
}

static auto_zone_t *gc_zone = NULL;

// DEPRECATED
auto_zone_t *auto_zone(void) {
    return gc_zone;
}

static void willgrow(auto_zone_t *collector, auto_heap_growth_info_t info) {  }

static void getenv_ulong(const char *name, unsigned long *dest) {
    const char *str = getenv(name);
    if (str) *dest = strtoul(str, NULL, 0);
}

static boolean_t getenv_bool(const char *name) {
    const char *str = getenv(name);
    return str && !strcmp(str, "YES");
}

// there can be several autonomous auto_zone's running, in theory at least.
auto_zone_t *auto_zone_create(const char *name) {
    aux_init();
#if defined(AUTO_ALLOCATION_METER)
    allocate_meter_init();
#endif
    pthread_key_t key = Zone::allocate_thread_key();
    if (key == 0) return NULL;
    
    Zone  *azone = new Zone(key);
    azone->basic_zone.size = auto_size;
    azone->basic_zone.malloc = auto_malloc;
    azone->basic_zone.free = auto_free;
    azone->basic_zone.calloc = auto_calloc;
    azone->basic_zone.valloc = auto_valloc;
    azone->basic_zone.realloc = auto_realloc;
    azone->basic_zone.destroy = auto_zone_destroy;
    azone->basic_zone.zone_name = name; // ;
    azone->basic_zone.introspect = &auto_zone_introspect;
    azone->basic_zone.version = AUTO_ZONE_VERSION;
    azone->basic_zone.memalign = NULL;
    azone->control.disable_generational = getenv_bool("AUTO_DISABLE_GENERATIONAL");
    azone->control.malloc_stack_logging = (getenv("MallocStackLogging") != NULL  ||  getenv("MallocStackLoggingNoCompact") != NULL);
    azone->control.log = AUTO_LOG_NONE;
    if (getenv_bool("AUTO_LOG_NOISY"))       azone->control.log |= AUTO_LOG_COLLECTIONS;
    if (getenv_bool("AUTO_LOG_ALL"))         azone->control.log |= AUTO_LOG_ALL;
    if (getenv_bool("AUTO_LOG_COLLECTIONS")) azone->control.log |= AUTO_LOG_COLLECTIONS;
    if (getenv_bool("AUTO_LOG_REGIONS"))     azone->control.log |= AUTO_LOG_REGIONS;
    if (getenv_bool("AUTO_LOG_UNUSUAL"))     azone->control.log |= AUTO_LOG_UNUSUAL;
    if (getenv_bool("AUTO_LOG_WEAK"))        azone->control.log |= AUTO_LOG_WEAK;

    azone->control.collection_threshold = 1024L * 1024L;
    getenv_ulong("AUTO_COLLECTION_THRESHOLD", &azone->control.collection_threshold);
    azone->control.full_vs_gen_frequency = 10;
    getenv_ulong("AUTO_COLLECTION_RATIO", &azone->control.full_vs_gen_frequency);
    azone->control.will_grow = willgrow;

    malloc_zone_register((auto_zone_t*)azone);

#if USE_DISPATCH_QUEUE
    azone->collection_queue = NULL;
    azone->collection_block = NULL;
#else
    azone->collection_thread = pthread_self();
    pthread_cond_init(&azone->collection_requested, NULL);
#endif
    pthread_mutex_init(&azone->collection_mutex, NULL);
    azone->collection_requested_mode = 0;
    pthread_cond_init(&azone->collection_status, NULL);
    azone->collection_status_state = 0;
    
    // register our calling thread so that the zone is ready to go
    azone->register_thread();
    
#if USE_DISPATCH_QUEUE
    char *notify_name = getenv("AUTO_COLLECT_EXHAUSTIVE_NOTIFICATION");
    if (notify_name != NULL) {
        int token;
        notify_register_dispatch(notify_name, &token, dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_HIGH), ^(int token) {
            auto_collect((auto_zone_t *)azone, AUTO_COLLECT_EXHAUSTIVE_COLLECTION, NULL);
        });
    }
#endif

    if (!gc_zone) gc_zone = (auto_zone_t *)azone;   // cache first one for debugging, monitoring
    return (auto_zone_t*)azone;
}

// Obsoleted by <rdar://problem/6628775> remove ZoneMonitor support.
void auto_zone_start_monitor(boolean_t force) {}
void auto_zone_set_class_list(int (*class_list)(void **buffer, int count)) {}

/*********  Reference counting  ************/

void auto_zone_retain(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
#if DEBUG
    if (ptr == WatchPoint) {
        malloc_printf("auto_zone_retain watchpoint: %p\n", WatchPoint);
        blainer();
    }
#endif
    int refcount = azone->block_increment_refcount(ptr);
    if (__auto_reference_logger) __auto_reference_logger(AUTO_RETAIN_EVENT, ptr, uintptr_t(refcount));
    if (Environment::log_reference_counting && malloc_logger) {
        malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(zone), azone->block_size(ptr), 0, uintptr_t(ptr), 0);
    }
}

unsigned int auto_zone_release(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;

#if DEBUG
    if (ptr == WatchPoint) {
        malloc_printf("auto_zone_release watchpoint: %p\n", WatchPoint);
        blainer();
    }
#endif
    int refcount = azone->block_decrement_refcount(ptr);
    if (__auto_reference_logger) __auto_reference_logger(AUTO_RELEASE_EVENT, ptr, uintptr_t(refcount));
    if (Environment::log_reference_counting && malloc_logger) {
        malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(zone), uintptr_t(ptr), 0, 0, 0);
    }
    return refcount;
}


unsigned int auto_zone_retain_count(auto_zone_t *zone, const void *ptr) {
    Zone *azone = (Zone *)zone;
    return azone->block_refcount((void *)ptr);
}

/*********  Write-barrier   ************/


static void handle_resurrection(Zone *azone, const void *recipient, bool recipient_is_block, const void *new_value, size_t offset) 
{
    if (!recipient_is_block || ((auto_memory_type_t)azone->block_layout((void*)recipient) & AUTO_UNSCANNED) != AUTO_UNSCANNED) {
        auto_memory_type_t new_value_type = (auto_memory_type_t) azone->block_layout((void*)new_value);
        char msg[256];
        snprintf(msg, sizeof(msg), "resurrection error for block %p while assigning %p[%d] = %p", new_value, recipient, (int)offset, new_value);
        if (new_value_type == AUTO_OBJECT_SCANNED) {
            // mark the object for zombiehood.
            auto_zone_retain((auto_zone_t*)azone, (void*)new_value); // mark the object ineligible for freeing this time around.
            azone->add_zombie((void*)new_value);
            if (azone->control.name_for_address) {
                // note, the auto lock is held until the callback has had a chance to examine each block.
                char *recipient_name = azone->control.name_for_address((auto_zone_t *)azone, (vm_address_t)recipient, offset);
                char *new_value_name = azone->control.name_for_address((auto_zone_t *)azone, (vm_address_t)new_value, 0);
                snprintf(msg, sizeof(msg), "resurrection error for object %p while assigning %s(%p)[%d] = %s(%p)",
                              new_value, recipient_name, recipient, (int)offset, new_value_name, new_value);
                free(recipient_name);
                free(new_value_name);
            }
        }
        malloc_printf("%s\ngarbage pointer stored into reachable memory, break on auto_zone_resurrection_error to debug\n", msg);
        auto_zone_resurrection_error();
    }
}

// make the resurrection test an inline to be as fast as possible in the write barrier
// recipient may be a GC block or not, as determined by recipient_is_block
// returns true if a resurrection occurred, false if not
inline static bool check_resurrection(Thread &thread, Zone *azone, void *recipient, bool recipient_is_block, const void *new_value, size_t offset) {
    if (new_value &&
        azone->is_block((void *)new_value) &&
        azone->block_is_garbage((void *)new_value) &&
        (!recipient_is_block || !azone->block_is_garbage(recipient))) {
        handle_resurrection(azone, recipient, recipient_is_block, new_value, offset);
        return true;
    }
    return false;
}

// called by objc assignIvar assignStrongCast
boolean_t auto_zone_set_write_barrier(auto_zone_t *zone, const void *dest, const void *new_value) {
    Zone *azone = (Zone *)zone;
    if (!new_value || azone->block_start((void *)new_value) != new_value) {
        *(void **)dest = (void *)new_value;
        return true;
    }
    const void *recipient = azone->block_start((void *)dest);
    if (!recipient) return false;
    Thread &thread = azone->registered_thread();
    size_t offset_in_bytes = (char *)dest - (char *)recipient;
    check_resurrection(thread, azone, (void *)recipient, true, new_value, offset_in_bytes);
    return azone->set_write_barrier(thread, (void *)dest, (void *)new_value);
}

inline bool is_scanned(int layout) {
    return (layout & AUTO_UNSCANNED) != AUTO_UNSCANNED;
}

void *auto_zone_write_barrier_memmove(auto_zone_t *zone, void *dst, const void *src, size_t size) {
    if (size == 0 || dst == src)
        return dst;
    Zone *azone = (Zone *)zone;
    // speculatively determine the base of the destination
    void *base = azone->block_start(dst);
    // If the destination is a scanned block then mark the write barrier
    // We used to check for resurrection but this is a conservative move without exact knowledge
    // and we don't want to choke on a false positive.
    if (base && is_scanned(azone->block_layout(base))) {
        // range check for extra safety.
        size_t block_size = azone->block_size(base);
        ptrdiff_t block_overrun = (ptrdiff_t(dst) + size) - (ptrdiff_t(base) + block_size);
        if (block_overrun > 0) {
            auto_fatal("auto_zone_write_barrier_memmove: will overwrite block %p, size %ld by %ld bytes.", base, block_size, block_overrun);
        }
        // we are only interested in scanning for pointers, so align to pointer boundaries
        const void *ptrSrc;
        size_t ptrSize;
        if (is_pointer_aligned((void *)src) && ((size & (sizeof(void *)-1)) == 0)) {
            // common case, src is pointer aligned and size is multiple of pointer size
            ptrSrc = src;
            ptrSize = size;
        } else {
            // compute pointer aligned src, end, and size within the source buffer
            ptrSrc = align_up((void *)src, pointer_alignment);
            const void *ptrEnd = align_down(displace((void *)src, size), pointer_alignment);
            if ((vm_address_t)ptrEnd > (vm_address_t)ptrSrc) {
                ptrSize = (vm_address_t)ptrEnd - (vm_address_t)ptrSrc;
            } else {
                ptrSize = 0; // copying a range that cannot contain an aligned pointer
            }
        }
        if (ptrSize >= sizeof(void *)) {
            Thread &thread = azone->registered_thread();
            // Pass in aligned src/size. Since dst is only used to determine thread locality it is ok to not align that value
            // Even if we're storing into garbage it might be visible to other garbage long after a TLC collects it, so we need to escape it.
            thread.track_local_memcopy(azone, ptrSrc, dst, ptrSize);
            if (azone->set_write_barrier_range(dst, size)) {
                // must hold enlivening lock for duration of the move; otherwise if we get scheduled out during the move
                // and GC starts and scans our destination before we finish filling it with unique values we lose them
                EnliveningHelper<UnconditionalBarrier> barrier(thread);
                if (barrier) {
                    // add all values in the range.
                    // We could/should only register those that are as yet unmarked.
                    // We also only add values that are objects.
                    void **start = (void **)ptrSrc;
                    void **end = start + ptrSize/sizeof(void *);
                    while (start < end) {
                        void *candidate = *start;
                        if (azone->is_block(candidate) && !azone->block_is_marked(candidate)) barrier.enliven_block(candidate);
                        start++;
                    }
                }
            }
        }
    } else if (base == NULL) {
        // since dst is not in out heap, it is by definition unscanned. unfortunately, many clients already use this on malloc()'d blocks, so we can't warn for that case.
        // if the source pointer comes from a scanned block in our heap, and the destination pointer is in global data, warn about bypassing global write-barriers.
        void *srcbase = azone->block_start((void*)src);
        if (srcbase && is_scanned(azone->block_layout(srcbase)) && azone->is_global_address(dst)) {
            // make this a warning in SnowLeopard.
            auto_error(zone, "auto_zone_write_barrier_memmove:  Copying a scanned block into global data. Break on auto_zone_global_data_memmove_error() to debug.\n", dst);
            auto_zone_global_data_memmove_error();
        }
    }
    // perform the copy
    return memmove(dst, src, size);
}

/*********  Layout  ************/

void* auto_zone_allocate_object(auto_zone_t *zone, size_t size, auto_memory_type_t type, boolean_t initial_refcount_to_one, boolean_t clear) {
    void *ptr;
//    if (allocate_meter) allocate_meter_start();
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    // ALWAYS clear if scanned memory <rdar://problem/5341463>.
    ptr = azone->block_allocate(thread, size, type, clear || (type & AUTO_UNSCANNED) != AUTO_UNSCANNED, initial_refcount_to_one);
    // We only log here because this is the only entry point that normal malloc won't already catch
    if (ptr && malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE | (clear ? MALLOC_LOG_TYPE_CLEARED : 0), uintptr_t(zone), size, initial_refcount_to_one ? 1 : 0, uintptr_t(ptr), 0);
//    if (allocate_meter) allocate_meter_stop();
    return ptr;
}

extern "C" void *auto_zone_create_copy(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
    auto_memory_type_t type; int rc = 0;
    if (!get_type_and_retain_count(azone, ptr, &type, &rc)) {
        // from "somewhere else"
        type = AUTO_MEMORY_UNSCANNED;
        rc = 0;
    }
    if (type == AUTO_OBJECT_SCANNED || type == AUTO_OBJECT_UNSCANNED) {
        // if no weak layouts we could be more friendly
        auto_error(azone, "auto_zone_copy_memory called on object %p\n", ptr);
        return (void *)0;
    }
    size_t size = auto_size(zone, ptr);
    void *result = auto_zone_allocate_object(zone, size, type, (rc == 1), false);
    if (type == AUTO_OBJECT_SCANNED)
        auto_zone_write_barrier_memmove(zone, result, ptr, size);
    else
        memmove(result, ptr, size);
    return result;
}

// Change type to non-object.  This happens when, obviously, no finalize is needed.
void auto_zone_set_nofinalize(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
    auto_memory_type_t type = azone->block_layout(ptr);
    if (type == AUTO_TYPE_UNKNOWN) return;
    // preserve scanned-ness but drop AUTO_OBJECT
    if (type == AUTO_OBJECT && azone->weak_layout_map_for_block(ptr))
        return;  // ignore request for objects that have weak instance variables
    azone->block_set_layout(ptr, type & ~AUTO_OBJECT);
}

// Change type to unscanned.  This is used in very rare cases where a block sometimes holds
// a strong reference and sometimes not.
void auto_zone_set_unscanned(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
    auto_memory_type_t type = azone->block_layout(ptr);
    if (type == AUTO_TYPE_UNKNOWN) return;
    azone->block_set_layout(ptr, type|AUTO_UNSCANNED);
}

extern void auto_zone_clear_stack(auto_zone_t *zone, unsigned long options)
{
    Zone *azone = (Zone *)zone;
    Thread *thread = azone->current_thread();
    if (thread) {
        thread->clear_stack();
    }
}


auto_memory_type_t auto_zone_get_layout_type(auto_zone_t *zone, void *ptr) {
    return (auto_memory_type_t) ((Zone *)zone)->block_layout(ptr);
}


void auto_zone_register_thread(auto_zone_t *zone) {
    ((Zone *)zone)->register_thread();
}


void auto_zone_unregister_thread(auto_zone_t *zone) {
    ((Zone *)zone)->unregister_thread();
}

void auto_zone_assert_thread_registered(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    azone->registered_thread();
}

void auto_zone_register_datasegment(auto_zone_t *zone, void *address, size_t size) {
    ((Zone *)zone)->add_datasegment(address, size);
}

void auto_zone_unregister_datasegment(auto_zone_t *zone, void *address, size_t size) {
    ((Zone *)zone)->remove_datasegment(address, size);
}

/**
 * Computes a conservative estimate of the amount of memory touched by the collector. Examines each
 * small region, determining the high watermark of used blocks, and subtracts out the unused block sizes
 * (to the nearest page boundary). Assumes all of the book keeping bitmaps have been touched. Also subtracts
 * out the sizes of the allocate big entries, since these aren't touched by the allocator itself.
 */
unsigned auto_zone_touched_size(auto_zone_t *zone) {
    Statistics stats;
    ((Zone *)zone)->statistics(stats);
    return stats.size();
}

double auto_zone_utilization(auto_zone_t *zone) {
    Statistics stats;
    ((Zone *)zone)->statistics(stats);
    return (double)stats.small_medium_size() / (double)(stats.small_medium_size() + stats.unused());
}

/*********  Garbage Collection and Compaction   ************/

auto_collection_control_t *auto_collection_parameters(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    return &azone->control;
}



// public entry point.
void auto_zone_statistics(auto_zone_t *zone, auto_statistics_t *stats) {
    if (!stats) return;
    Zone *azone = (Zone *)zone;
    Statistics &statistics = azone->statistics();
    
    azone->stats.malloc_statistics.blocks_in_use = statistics.count();
    azone->stats.malloc_statistics.size_in_use = statistics.size();
    azone->stats.malloc_statistics.max_size_in_use = statistics.dirty_size();
    azone->stats.malloc_statistics.size_allocated = statistics.allocated() + statistics.admin_size();
    // now copy the whole thing over
    if (stats->version == 1) *stats = azone->stats;
    else if (stats->version == 0) {
        memmove(stats, &azone->stats, sizeof(auto_statistics_t)-3*sizeof(long long));
    }
}

// work in progress
typedef struct {
    FILE *f;
    char *buff;
    size_t buff_size;
    size_t buff_pos;
} AutoZonePrintInfo;

static void _auto_zone_stats_printf(AutoZonePrintInfo *info, const char *fmt, ...)
{
    if (info->f) {
        va_list valist;
        va_start(valist, fmt);
        vfprintf(info->f, fmt, valist);
        va_end(valist);
    }
    if (info->buff) {
        if (info->buff_pos < info->buff_size) {
            va_list valist;
            va_start(valist, fmt);
            info->buff_pos += vsnprintf(&info->buff[info->buff_pos], info->buff_size - info->buff_pos, fmt, valist);
            va_end(valist);
        }
    }
}

static void print_zone_stats(AutoZonePrintInfo *info, malloc_statistics_t &stats, const char *message) {
    _auto_zone_stats_printf(info, "%s %10lu %10u %10lu %10lu        %0.2f\n", message,
        stats.size_in_use, stats.blocks_in_use, stats.max_size_in_use, stats.size_allocated,
        ((float)stats.size_in_use)/stats.max_size_in_use);
}

__private_extern__ malloc_zone_t *aux_zone;

static void _auto_zone_stats(AutoZonePrintInfo *info) {
    // Memory first
    malloc_statistics_t mstats;
    _auto_zone_stats_printf(info, "\n            bytes     blocks      dirty     vm     bytes/dirty\n");
    if (gc_zone) {
        malloc_zone_statistics(gc_zone, &mstats);
        print_zone_stats(info, mstats, "auto  ");
        //auto_malloc_statistics_new(gc_zone, &mstats);  // has deadlock; see rdar://5954569
        //print_zone_stats(info, mstats, "newa  ");
        //auto_malloc_statistics_old(gc_zone, &mstats);
        //print_zone_stats(info, mstats, "olda  ");
        malloc_zone_statistics(aux_zone, &mstats);
        print_zone_stats(info, mstats, "aux   ");
    }
    malloc_zone_statistics(malloc_default_zone(), &mstats);
    print_zone_stats(info, mstats, "malloc");
    malloc_zone_statistics(NULL, &mstats);
    print_zone_stats(info, mstats, "total ");
    if (!gc_zone) return;
    
    Zone *azone = (Zone *)gc_zone;
    Statistics &statistics = azone->statistics();
    
    _auto_zone_stats_printf(info, "Regions In Use: %ld\nSubzones In Use: %ld\n", statistics.regions_in_use(), statistics.subzones_in_use());
    
    
    auto_statistics_t *stats = &azone->stats;
    // CPU
//    _auto_zone_stats_printf(info, "\ncpu (microseconds):\n\ntotal %lld usecs = scan %lld + finalize %lld + reclaim %lld\n",
    _auto_zone_stats_printf(info, "\n%ld generational\n%ld full\ncpu (microseconds):\n               total =     scan   + freeze + finalize  + reclaim\nfull+gen  %10lld %10lld %10lld %10lld %10lld\n", statistics.partial_gc_count(), statistics.full_gc_count(),
        // full + gen
        stats->total[0].total_duration + stats->total[1].total_duration,
        stats->total[0].scan_duration + stats->total[1].scan_duration,
        stats->total[0].enlivening_duration + stats->total[1].enlivening_duration,
        stats->total[0].finalize_duration + stats->total[1].finalize_duration,
        stats->total[0].reclaim_duration + stats->total[1].reclaim_duration);
    _auto_zone_stats_printf(info, "gen. max  %10lld %10lld %10lld %10lld %10lld\n",
        stats->maximum[1].total_duration, 
        stats->maximum[1].scan_duration,
        stats->maximum[1].enlivening_duration,
        stats->maximum[1].finalize_duration,
        stats->maximum[1].reclaim_duration);
    _auto_zone_stats_printf(info, "full max  %10lld %10lld %10lld %10lld %10lld\n\n",
        stats->maximum[0].total_duration, 
        stats->maximum[0].scan_duration,
        stats->maximum[0].enlivening_duration,
        stats->maximum[0].finalize_duration,
        stats->maximum[0].reclaim_duration);
    long count = statistics.partial_gc_count();
    if (!count) count = 1;
    _auto_zone_stats_printf(info, "gen. avg  %10lld %10lld %10lld %10lld %10lld\n",
        stats->total[1].total_duration/count, 
        stats->total[1].scan_duration/count,
        stats->total[1].enlivening_duration/count,
        stats->total[1].finalize_duration/count,
        stats->total[1].reclaim_duration/count);
    count = statistics.full_gc_count();
    if (!count) count = 1;
    _auto_zone_stats_printf(info, "full avg  %10lld %10lld %10lld %10lld %10lld\n\n",
        stats->total[0].total_duration/count, 
        stats->total[0].scan_duration/count,
        stats->total[0].enlivening_duration/count,
        stats->total[0].finalize_duration/count,
        stats->total[0].reclaim_duration/count);
    _auto_zone_stats_printf(info, "\nthread collections %lld; total blocks %lld; total bytes %lld\n",
        stats->thread_collections_total,
        stats->thread_blocks_recovered_total,
        stats->thread_bytes_recovered_total);
}

void auto_zone_write_stats(FILE *f) {
    AutoZonePrintInfo info;
    info.f = f;
    info.buff = NULL;
    _auto_zone_stats(&info);
}

void auto_zone_stats() {
    auto_zone_write_stats(stdout);
}

char *auto_zone_stats_string()
{
    AutoZonePrintInfo info;
    info.f = NULL;
    info.buff = NULL;
    info.buff_size = 0;
    do {
        info.buff_size += 2048;
        if (info.buff) free(info.buff);
        info.buff = (char *)malloc(info.buff_size);
        info.buff_pos = 0;
        _auto_zone_stats(&info);
    } while (info.buff_pos > info.buff_size);
    return info.buff;
}

void auto_collector_reenable(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    // although imperfect, try to avoid dropping below zero
    if (azone->collector_disable_count == 0) return;
    OSAtomicDecrement32(&azone->collector_disable_count);
}

void auto_collector_disable(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    OSAtomicIncrement32(&azone->collector_disable_count);
}

boolean_t auto_zone_is_enabled(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    return azone->collector_disable_count == 0;
}

boolean_t auto_zone_is_collecting(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    // FIXME: the result of this function only valid on the collector thread (main for now).
    return !azone->is_state(idle);
}

#if USE_DISPATCH_QUEUE
static void auto_collection_work(Zone *zone);
#endif

void auto_collect_multithreaded(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    if (! azone->multithreaded) {
        if (azone->control.log & AUTO_LOG_COLLECTIONS) malloc_printf("starting dedicated collection thread\n");
#if USE_DISPATCH_QUEUE
        // disable the collector while creating the queue/block to avoid starting the collector while
        // copying collection_block.
        auto_collector_disable(zone);
        
#if 1
        // In general libdispatch limits the number of concurrent jobs based on various factors (# cpus).
        // But we don't want the collector to be kept waiting while long running jobs generate garbage.
        // We avoid collection latency using a special attribute which tells dispatch this queue should 
        // service jobs immediately even if that requires exceeding the usual concurrent limit.
        dispatch_queue_attr_t collection_queue_attrs = dispatch_queue_attr_create();
        dispatch_queue_attr_set_flags(collection_queue_attrs, DISPATCH_QUEUE_OVERCOMMIT);
        dispatch_queue_attr_set_priority(collection_queue_attrs, DISPATCH_QUEUE_PRIORITY_HIGH);
        azone->collection_queue = dispatch_queue_create("Garbage Collection Work Queue", collection_queue_attrs);
        dispatch_release(collection_queue_attrs);
#else
        azone->collection_queue = dispatch_queue_create("Garbage Collection Work Queue", NULL);
#endif
        azone->collection_block = Block_copy(^{ auto_collection_work(azone); });
        auto_collector_reenable(zone);
#else
        pthread_create(&azone->collection_thread, NULL, auto_collection_thread, azone);
#endif
        azone->multithreaded = true;
    }
}


//
// Called by Instruments to lay down a heap dump (via dumpster)
//
void auto_enumerate_references(auto_zone_t *zone, void *referent, 
                               auto_reference_recorder_t callback, // f(zone, ctx, {ref, referrer, offset})
                               void *stack_bottom, void *ctx)
{
    Zone *azone = (Zone *)zone;
    azone->block_collector();
    {
        ReferenceRecorder recorder(azone, referent, callback, stack_bottom, ctx);
        recorder.scan();
        azone->reset_all_marks_and_pending();
    }
    azone->unblock_collector();
}


/********* Weak References ************/


// auto_assign_weak
// The new and improved one-stop entry point to the weak system
// Atomically assign value to *location and track it for zero'ing purposes.
// Assign a value of NULL to deregister from the system.
void auto_assign_weak_reference(auto_zone_t *zone, const void *value, void *const*location, auto_weak_callback_block_t *block) {
    Zone *azone = (Zone *)zone;
    void *base = azone->block_start((void *)location);
    Thread &thread = azone->registered_thread();
    if (check_resurrection(thread, azone, (void *)base, base != NULL, value, (size_t)location - (size_t)base)) {
        // Never allow garbage to be registered since it will never be cleared.
        // Go further and zero it out since it would have been cleared had it been done earlier
        value = NULL;
    }
    
    // Check if this is a store to the stack and don't register it.
    // This handles id *__weak as a parameter
    if (!thread.is_stack_address((void *)location)) {
        // unregister old, register new (if non-NULL)
        weak_register(azone, value, (void **)location, block);
        if (value != NULL) {
            // note: we could check to see if base is local, but then we have to change
            // all weak references on make_global.
            if (base) thread.block_escaped(azone, NULL, base);
            thread.block_escaped(azone, NULL, (void *)value);
            // also zap destination so that dead locals don't need to be pulled out of weak tables
            //thread->track_local_assignment(azone, (void *)location, (void *)value);
        }
    }
}

void *auto_read_weak_reference(auto_zone_t *zone, void **referrer) {
    void *result = *referrer;
    if (result != NULL) {
        // We grab the condition barrier.  Missing the transition is not a real issue.
        // For a missed transition to be problematic the collector would have had to mark
        // the transition before we entered this routine, scanned this thread (not seeing the
        // enlivened read), scanned the heap, and scanned this thread exhaustively before we
        // load *referrer
        Zone *azone = (Zone*)zone;
        Thread &thread = azone->registered_thread();
        EnliveningHelper<ConditionBarrier> barrier(thread);
        if (barrier) {
            // need to tell the collector this block should be scanned.
            result = *referrer;
            if (result && !azone->block_is_marked(result)) barrier.enliven_block(result);
        } else {
            result = *referrer;
        }
    }
    return result;
}

/********* Associative References ************/

void auto_zone_set_associative_ref(auto_zone_t *zone, void *object, void *key, void *value) {
    Zone *azone = (Zone*)zone;
    Thread &thread = azone->registered_thread();
    bool object_is_block = azone->is_block(object);
    // <rdar://problem/6463922> Treat global pointers as unconditionally live.
    if (!object_is_block && !azone->is_global_address(object)) {
        auto_error(zone, "auto_zone_set_associative_ref: object should point to a GC block or a global address, otherwise associations will leak. Break on auto_zone_association_error() to debug.", object);
        auto_zone_association_error(object);
        return;
    }
    check_resurrection(thread, azone, object, object_is_block, value, 0);
    azone->set_associative_ref(object, key, value);
}

void *auto_zone_get_associative_ref(auto_zone_t *zone, void *object, void *key) {
    Zone *azone = (Zone*)zone;
    return azone->get_associative_ref(object, key);
}

void auto_zone_erase_associative_refs(auto_zone_t *zone, void *object) {
    Zone *azone = (Zone*)zone;
    return azone->erase_associations(object);
}

/********* Root References ************/

void auto_zone_add_root(auto_zone_t *zone, void *root, void *value)
{
    Zone *azone = (Zone*)zone;
    Thread &thread = azone->registered_thread();
    check_resurrection(thread, azone, root, false, value, 0);
    (azone)->add_root(root, value);
}

void auto_zone_remove_root(auto_zone_t *zone, void *root) {
    ((Zone *)zone)->remove_root(root);
}

extern void auto_zone_root_write_barrier(auto_zone_t *zone, void *address_of_possible_root_ptr, void *value) {
    if (!value) {
        *(void **)address_of_possible_root_ptr = NULL;
        return;
    }
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    if (thread.is_stack_address(address_of_possible_root_ptr)) {
        // allow writes to the stack without checking for resurrection to allow finalizers to do work
        // always write directly to the stack.
        *(void **)address_of_possible_root_ptr = value;
    } else if (azone->is_root(address_of_possible_root_ptr)) {
        // if local make global before possibly enlivening
        thread.block_escaped(azone, NULL, value);
        // might need to tell the collector this block should be scanned.
        EnliveningHelper<UnconditionalBarrier> barrier(thread);
        if (barrier && !azone->block_is_marked(value)) barrier.enliven_block(value);
        check_resurrection(thread, azone, address_of_possible_root_ptr, false, value, 0);
        *(void **)address_of_possible_root_ptr = value;
    } else if (azone->is_global_address(address_of_possible_root_ptr)) {
        // add_root performs a resurrection check
        auto_zone_add_root(zone, address_of_possible_root_ptr, value);
    } else {
        // This should only be something like storing a globally retained value
        // into a malloc'ed/vm_allocated hunk of memory.  It might be that it is held
        // by GC at some other location and they're storing a go-stale pointer.
        // That "some other location" might in fact be the stack.
        // If so we can't really assert that it's either not-thread-local or retained.
        thread.block_escaped(azone, NULL, value);
        check_resurrection(thread, azone, address_of_possible_root_ptr, false, value, 0);
        // Always write
        *(void **)address_of_possible_root_ptr = value;
    }
}


void auto_zone_print_roots(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    Statistics junk;
    PointerList roots(junk);
    azone->copy_roots(roots);
    usword_t count = roots.count();
    printf("### %lu roots. ###\n", count);
    void ***buffer = (void ***)roots.buffer();
    for (usword_t i = 0; i < count; ++i) {
        void **root = buffer[i];
        printf("%p -> %p\n", root, *root);
    }
}

/********** Atomic operations *********************/

boolean_t auto_zone_atomicCompareAndSwap(auto_zone_t *zone, void *existingValue, void *newValue, void *volatile *location, boolean_t isGlobal, boolean_t issueBarrier) {
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    if (isGlobal) {
        azone->add_root_no_barrier((void *)location);
    }
    check_resurrection(thread, azone, (void *)location, !isGlobal, newValue, 0);
    thread.block_escaped(azone, NULL, newValue);
    EnliveningHelper<UnconditionalBarrier> barrier(thread);
    boolean_t result;
    if (issueBarrier)
        result = OSAtomicCompareAndSwapPtrBarrier(existingValue, newValue, location);
    else
        result = OSAtomicCompareAndSwapPtr(existingValue, newValue, location);
    if (!isGlobal) {
        // mark write-barrier w/o storing
        azone->set_write_barrier((char*)location);
    }
    if (result && barrier && !azone->block_is_marked(newValue)) barrier.enliven_block(newValue);
    return result;
}
/************ Experimental ***********************/

void auto_zone_dump(auto_zone_t *zone,
            auto_zone_stack_dump stack_dump,
            auto_zone_register_dump register_dump,
            auto_zone_node_dump thread_local_node_dump,
            auto_zone_root_dump root_dump,
            auto_zone_node_dump global_node_dump,
            auto_zone_weak_dump weak_dump
    ) {
    
    Auto::Zone *azone = (Auto::Zone *)zone;
    azone->dump(stack_dump, register_dump, thread_local_node_dump, root_dump, global_node_dump, weak_dump);
}

auto_probe_results_t auto_zone_probe_unlocked(auto_zone_t *zone, void *address) {
    Zone *azone = (Zone *)zone;
    auto_probe_results_t result = azone->block_is_start(address) ? auto_is_auto : auto_is_not_auto;
    if ((result & auto_is_auto) && azone->is_local(address))
        result |= auto_is_local;
    return result;
}

void auto_zone_scan_exact(auto_zone_t *zone, void *address, void (^callback)(void *base, unsigned long byte_offset, void *candidate)) {
    Zone *azone = (Zone *)zone;
    auto_memory_type_t layout = azone->block_layout(address);
    // invalid addresses will return layout == -1 and hence won't == 0 below
    if ((layout & AUTO_UNSCANNED) == 0) {
        const unsigned char *map = NULL;
        if (layout & AUTO_OBJECT) {
            map = azone->layout_map_for_block((void *)address);
        }
        unsigned int byte_offset = 0;
        unsigned int size = azone->block_size(address);
        if (map) {
            while (*map) {
                int skip = (*map >> 4) & 0xf;
                int refs = (*map) & 0xf;
                byte_offset = byte_offset + skip*sizeof(void *);
                while (refs--) {
                    callback(address, byte_offset, *(void **)(((char *)address)+byte_offset));
                    byte_offset += sizeof(void *);
                }
                ++map;
            }
        }
        while (byte_offset < size) {
            callback(address, byte_offset, *(void **)(((char *)address)+byte_offset));
            byte_offset += sizeof(void *);
        }
    }
}


/************* API ****************/

boolean_t auto_zone_atomicCompareAndSwapPtr(auto_zone_t *zone, void *existingValue, void *newValue, void *volatile *location, boolean_t issueBarrier) {
    Zone *azone = (Zone *)zone;
    return auto_zone_atomicCompareAndSwap(zone, existingValue, newValue, location, azone->is_global_address((void*)location), issueBarrier);
}

/************ Miscellany **************************/


#if 0
// Watching

#define WatchLimit 16
static const void *WatchPoints[WatchLimit];

void auto_zone_watch(const void *ptr) {
    for (int i = 0; i < WatchLimit; ++i) {
        if (WatchPoints[i]) 
            if (WatchPoints[i] == ptr) return;
            else
                continue;
        WatchPoints[i] = ptr;
        return;
    }
    printf("too many watchpoints already, skipping %p\n", ptr);
}

void auto_zone_watch_free(const void *ptr, const char *msg) {
    for (int i = 0; i < WatchLimit; ++i) {
        if (WatchPoints[i] == NULL) return;
        if (WatchPoints[i] == ptr) {
            printf(msg, ptr);
            while(++i < WatchLimit)
                WatchPoints[i-1] = WatchPoints[i];
            WatchPoints[WatchLimit-1] = NULL;
            return;
        }
    }
}

boolean_t auto_zone_watch_msg(void *ptr, const char *format,  void *extra) {
    for (int i = 0; i < WatchLimit; ++i) {
        if (WatchPoints[i] == NULL) return false;
        if (WatchPoints[i] == ptr) {
            printf(format, ptr, extra);
            return true;
        }
    }
    return false;
}
#endif


#if DEBUG
////////////////// SmashMonitor ///////////////////

static void range_check(void *pointer, size_t size) {
    Zone *azone = (Zone *)gc_zone;
    if (azone) {
        void *base_pointer = azone->block_start(pointer);
        if (base_pointer) {
            size_t block_size = azone->block_size(base_pointer);
            if ((uintptr_t(pointer) + size) > (uintptr_t(base_pointer) + block_size)) {
                malloc_printf("SmashMonitor: range check violation for pointer = %p, size = %lu", pointer, size);
                __builtin_trap();
            }
        }
    }
}

void *SmashMonitor_memcpy(void *dst, const void* src, size_t size) {
    // add some range checking code for auto allocated blocks.
    range_check(dst, size);
    return memcpy(dst, src, size);
}

void *SmashMonitor_memmove(void *dst, const void* src, size_t size) {
    // add some range checking code for auto allocated blocks.
    range_check(dst, size);
    return memmove(dst, src, size);
}

void *SmashMonitor_memset(void *pointer, int value, size_t size) {
    // add some range checking code for auto allocated blocks.
    range_check(pointer, size);
    return memset(pointer, value, size);
}

void SmashMonitor_bzero(void *pointer, size_t size) {
    // add some range checking code for auto allocated blocks.
    range_check(pointer, size);
    bzero(pointer, size);
}


#if USE_INTERPOSING
DYLD_INTERPOSE(SmashMonitor_memcpy, memcpy)
DYLD_INTERPOSE(SmashMonitor_memmove, memmove)
DYLD_INTERPOSE(SmashMonitor_memset, memset)
DYLD_INTERPOSE(SmashMonitor_bzero, bzero)
#endif

#endif

#if LOG_TIMINGS
// allocation & collection rate logging

typedef struct {
    auto_date_t stamp;
    size_t  allocated;
    size_t  finger;
    size_t  recovered;
    char    purpose;    // G or F - start GC; E - end GC; A - allocation threshold
} log_record_t;

#define NRECORDS 2048

log_record_t AutoRecords[NRECORDS];
int AutoRecordsIndex = 0;


static void dumpRecords() {
    int fd = open("/tmp/records", O_CREAT|O_APPEND, 0666);
    int howmany = AutoRecordsIndex - 1;
    write(fd, &AutoRecords[0], howmany*sizeof(log_record_t));
    close(fd);
    AutoRecordsIndex = 0;
}
static log_record_t *getRecord(int dump) {
    for (;;) {
        int index = OSAtomicIncrement32(&AutoRecordsIndex);
        if (index == NRECORDS || dump) {
            dumpRecords();
        }
        else if (index < NRECORDS) return &AutoRecords[index];
    }
}
static void log_allocation_threshold(auto_date_t time, size_t allocated, size_t finger) {
    log_record_t *record = getRecord(0);
    record->stamp = time;
    record->allocated = allocated;
    record->finger = finger;
    record->purpose = 'A';
}
static void log_collection_begin(auto_date_t time, size_t allocated, size_t finger, bool isGen) {
    log_record_t *record = getRecord(0);
    record->stamp = time;
    record->allocated = allocated;
    record->finger = finger;
    record->purpose = isGen ? 'G' : 'F';
}

static void log_collection_end(auto_date_t time, size_t allocated, size_t finger, size_t recovered) {
    log_record_t *record = getRecord(0);
    record->stamp = time;
    record->allocated = allocated;
    record->finger = finger;
    record->recovered = recovered;
    record->purpose = 'E';
}

static double rateps(size_t quant, auto_date_t interval) {
    double quantity = quant;
    return (quantity/interval);
}

void log_analysis() {
    int lastAllocation = -1;
    int collectionBegin = -1;
    for (int index = 0; index < AutoRecordsIndex; ++index) {
        if (AutoRecords[index].purpose == 'A') {
            if (lastAllocation == -1) { lastAllocation = index; continue; }
            auto_date_t interval = AutoRecords[index].stamp - AutoRecords[lastAllocation].stamp;
            size_t quantity = AutoRecords[index].allocated - AutoRecords[lastAllocation].allocated;
            printf("%ld bytes in %lld microseconds, %gmegs/sec allocation rate\n", quantity, interval, rateps(quantity, interval));
            lastAllocation = index;
        }
        else if (AutoRecords[index].purpose == 'G' || AutoRecords[index].purpose == 'F') {
            collectionBegin = index;
            printf("begining %c collection\n", AutoRecords[index].purpose);
        }
        else if (AutoRecords[index].purpose == 'E') {
            auto_date_t interval = AutoRecords[index].stamp - AutoRecords[collectionBegin].stamp;
            size_t quantity = AutoRecords[index].allocated - AutoRecords[collectionBegin].allocated;
            size_t recovered  = AutoRecords[index].recovered;
            quantity += recovered;
            printf("%ld bytes in %lld microseconds, %gmegs/sec rate during collection\n", quantity, interval, rateps(quantity, interval));
            printf("%ld bytes %lld microseconds, %gmegs/sec recovery rate\n", recovered, interval, rateps(recovered, interval));
        }
    }
}

#endif LOG_TIMINGS
