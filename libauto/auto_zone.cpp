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
    auto_zone.cpp
    Automatic Garbage Collection
    Copyright (c) 2002-2011 Apple Inc. All rights reserved.
 */

#include "auto_zone.h"
#include "auto_impl_utilities.h"
#include "auto_weak.h"
#include "auto_trace.h"
#include "auto_dtrace.h"
#include "Zone.h"
#include "Locks.h"
#include "InUseEnumerator.h"
#include "ThreadLocalCollector.h"
#include "auto_tester/auto_tester.h"
#include "BlockIterator.h"

#include <stdlib.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#ifdef __BLOCKS__
#include <Block.h>
#include <notify.h>
#include <dispatch/private.h>
#endif

#define USE_INTERPOSING 0

#if USE_INTERPOSING
#include <mach-o/dyld-interposing.h>
#endif

using namespace Auto;

static char *b2s(uint64_t bytes, char *buf, int bufsize);

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

// Reference count logging support for ObjectAlloc et. al.
void (*__auto_reference_logger)(uint32_t eventtype, void *ptr, uintptr_t data) = NULL;

/*********  Parameters  ************/

#define VM_COPY_THRESHOLD       (40 * 1024)


/*********  Zone callbacks  ************/


boolean_t auto_zone_is_finalized(auto_zone_t *zone, const void *ptr) {
    Zone *azone = (Zone *)zone;
    // detects if the specified pointer is about to become garbage
    return (ptr && azone->block_is_garbage((void *)ptr));
}

static void auto_collect_internal(Zone *zone, boolean_t generational) {
    if (zone->_collector_disable_count) return;
    CollectionTimer timer;
    
    Statistics &zone_stats = zone->statistics();
    
    timer.total_time().start();
    zone_stats.idle_timer().stop();
    if (zone->control.log & AUTO_LOG_TIMINGS) timer.enable_scan_timer();
    
    zone_stats.reset_for_heap_collection();
    
    AUTO_PROBE(auto_probe_begin_heap_scan(generational));
    
    // bound the bottom of the stack.
    vm_address_t stack_bottom = auto_get_sp();
    if (zone->control.disable_generational) generational = false;
	GARBAGE_COLLECTION_COLLECTION_BEGIN((auto_zone_t*)zone, generational ? AUTO_TRACE_GENERATIONAL : AUTO_TRACE_FULL);
    zone->set_state(scanning);
    
    Thread &collector_thread = zone->register_thread();
    collector_thread.set_in_collector(true);
    zone->collect_begin();

    zone->collect((bool)generational, (void *)stack_bottom, timer);
    PointerList &list = zone->garbage_list();
    size_t garbage_count = list.count();
    void **garbage = list.buffer();
    size_t large_garbage_count = zone->large_garbage_count();
    void **large_garbage = (large_garbage_count ? garbage + garbage_count - large_garbage_count : NULL);

    AUTO_PROBE(auto_probe_end_heap_scan(garbage_count, garbage));
    
    size_t bytes_freed = 0;

    // note the garbage so the write-barrier can detect resurrection
	GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)zone, AUTO_TRACE_FINALIZING_PHASE);
    zone->set_state(finalizing);
    size_t block_count = garbage_count, byte_count = 0;
    zone->invalidate_garbage(garbage_count, garbage);
	GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)zone, AUTO_TRACE_FINALIZING_PHASE, (uint64_t)block_count, (uint64_t)byte_count);
    zone->set_state(reclaiming);
	GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)zone, AUTO_TRACE_SCAVENGING_PHASE);
    bytes_freed = zone->free_garbage(garbage_count - large_garbage_count, garbage, large_garbage_count, large_garbage, block_count, byte_count);
    zone->clear_zombies();
	GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)zone, AUTO_TRACE_SCAVENGING_PHASE, (uint64_t)block_count, (uint64_t)bytes_freed);

    timer.total_time().stop();
    zone->collect_end(timer, bytes_freed);
    collector_thread.set_in_collector(false);

	GARBAGE_COLLECTION_COLLECTION_END((auto_zone_t*)zone, (uint64_t)garbage_count, (uint64_t)bytes_freed, (uint64_t)zone_stats.count(), (uint64_t)zone_stats.size());

    zone->set_state(idle);
    AUTO_PROBE(auto_probe_heap_collection_complete());

    WallClockTimer &idle_timer = zone->statistics().idle_timer();
    if (zone->control.log & AUTO_LOG_TIMINGS) {
        const char *idle = idle_timer.time_string();
        char bytes[16];
        b2s(zone->statistics().bytes_scanned(), bytes, sizeof(bytes));
        malloc_printf("%s: %s GC completed in %s after %s idle. scanned %5llu blocks (%s) in %s\n",
                      auto_prelude(), (generational ? "gen." : "full"), timer.total_time().time_string(), idle,
                      zone->statistics().blocks_scanned(), bytes, timer.scan_timer().time_string());
    }
    if (zone->control.log & AUTO_LOG_COLLECTIONS) {
        malloc_statistics_t stats;
        zone->malloc_statistics(&stats);
        char freed[16], in_use[16];
        b2s(zone->statistics().size(), in_use, sizeof(in_use));
        b2s(bytes_freed, freed, sizeof(freed));
        malloc_printf("%s: %s GC collected %5llu blocks (%s). blocks in use: %7llu (%s)\n", 
                      auto_prelude(), (generational ? "gen." : "full"),
                      (unsigned long long)garbage_count, freed, 
                      zone->statistics().count(), in_use);
    }
#ifdef MEASURE_TLC_STATS
    zone->statistics().print_tlc_stats();
#endif
    idle_timer.reset();
    idle_timer.start();
}

//
// old external entry point for collection
//
void auto_collect(auto_zone_t *zone, auto_collection_mode_t mode, void *collection_context) {
    Zone *azone = (Zone *)zone;
    if (!azone->_collection_queue) return;
    auto_collection_mode_t heap_mode = mode & 0x3;
    if ((mode & AUTO_COLLECT_IF_NEEDED) || (mode == 0)) {
        auto_zone_collect(zone, AUTO_ZONE_COLLECT_NO_OPTIONS);
    } else {
        static uintptr_t options_translation[] = {AUTO_ZONE_COLLECT_RATIO_COLLECTION, AUTO_ZONE_COLLECT_GENERATIONAL_COLLECTION, AUTO_ZONE_COLLECT_FULL_COLLECTION, AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION};
        
        auto_zone_options_t request_mode = options_translation[heap_mode];
        auto_zone_collect(zone, request_mode);
        if (mode & AUTO_COLLECT_SYNCHRONOUS) {
            Zone *azone = (Zone *)zone;
            // For synchronous requests we have a problem: we must avoid deadlock with main thread finalization.
            // For dispatch, use a group to implement a wait with timeout.
            dispatch_group_t group = dispatch_group_create();
            dispatch_group_async(group, azone->_collection_queue, ^{});
            dispatch_group_wait(group, dispatch_time(0, 10*NSEC_PER_SEC));
            dispatch_release(group);
        }
    }
}


static inline bool _increment_pending_count(Zone *azone, auto_zone_options_t global_mode, bool coalesce_requested) {
    bool did_coalesce = true;
    Mutex lock(&azone->_collection_mutex);
    if (global_mode < AUTO_ZONE_COLLECT_GLOBAL_MODE_COUNT) {
        if (!coalesce_requested || azone->_pending_collections[global_mode] == 0) {
            /* Check for overflow on the pending count. This should only happen if someone is doing something wrong. */ 
            if (azone->_pending_collections[global_mode] == UINT8_MAX) {
                /* Overflow. Force the request to coalesce. We already have many of the same type queued, so probably benign. */
                auto_error(azone, "pending collection count overflowed", NULL);
            } else {
                azone->_pending_collections[global_mode]++;
                did_coalesce = false;
            }
        }
    }
    return did_coalesce;
}

static inline void _decrement_pending_count(Zone *azone, auto_zone_options_t global_mode) {
    Mutex lock(&azone->_collection_mutex);
    assert(global_mode < AUTO_ZONE_COLLECT_GLOBAL_MODE_COUNT);
    assert(azone->_pending_collections[global_mode] > 0);
    azone->_pending_collections[global_mode]--;
    AUTO_PROBE(auto_probe_collection_complete());
}

static void auto_zone_generational_collection(Zone *zone)
{
    auto_collect_internal(zone, true);
    _decrement_pending_count(zone, AUTO_ZONE_COLLECT_GENERATIONAL_COLLECTION);
}

static void auto_zone_full_collection(Zone *zone)
{
    auto_collect_internal(zone, false);
    _decrement_pending_count(zone, AUTO_ZONE_COLLECT_FULL_COLLECTION);
    
    // If collection checking is enabled, run a check.
    if (zone->collection_checking_enabled())
        zone->increment_check_counts();
}

static void auto_zone_exhaustive_collection(Zone *zone)
{
    // run collections until objects are no longer reclaimed.
    Statistics &stats = zone->statistics();
    uint64_t count, collections = 0;
    do {
        count = stats.count();
        auto_collect_internal(zone, false);
    } while (stats.count() < count && ((Environment::exhaustive_collection_limit == 0) || (++collections < Environment::exhaustive_collection_limit)));
    _decrement_pending_count(zone, AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION);
    
    // If collection checking is enabled, run a check.
    if (zone->collection_checking_enabled())
        zone->increment_check_counts();
}

static void auto_zone_ratio_collection(Zone *zone)
{
    if (zone->_collection_count++ == zone->control.full_vs_gen_frequency) {
        zone->_collection_count = 0;
        auto_collect_internal(zone, false);
    } else {
        auto_collect_internal(zone, true);
    }
    _decrement_pending_count(zone, AUTO_ZONE_COLLECT_RATIO_COLLECTION);
}

void auto_zone_collect(auto_zone_t *zone, auto_zone_options_t options)
{
    AUTO_PROBE(auto_probe_auto_zone_collect(options));
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    
    // First, handle the no options case by promoting to the appropriate mode.
    if (options == AUTO_ZONE_COLLECT_NO_OPTIONS) {
        if (azone->should_collect())
            options = AUTO_ZONE_COLLECT_COALESCE|AUTO_ZONE_COLLECT_RATIO_COLLECTION;
        if (ThreadLocalCollector::should_collect(azone, thread, true))
            options |= AUTO_ZONE_COLLECT_LOCAL_COLLECTION;
    }
    
    // Run TLC modes
    if (options & AUTO_ZONE_COLLECT_LOCAL_COLLECTION) {
        ThreadLocalCollector tlc(azone, (void *)auto_get_sp(), thread);
        tlc.collect(true);
    }
    
    // Volunteer for parallel scanning work.
    if (!pthread_main_np()) azone->volunteer_for_work(true);
    
    auto_zone_options_t global_mode = options & AUTO_ZONE_COLLECT_GLOBAL_COLLECTION_MODE_MASK;
    
    if (global_mode != 0) {
        if (!_increment_pending_count(azone, global_mode, options & AUTO_ZONE_COLLECT_COALESCE)) {
            // Enqueue global collection request
            dispatch_block_t collect_func;
            switch (global_mode) {
                case AUTO_ZONE_COLLECT_NO_OPTIONS:
                    /* This case is impossible */
                    collect_func = NULL;
                    break;
                case AUTO_ZONE_COLLECT_RATIO_COLLECTION:
                    collect_func =  ^{ 
                        auto_zone_ratio_collection((Zone *)dispatch_get_context(dispatch_get_current_queue())); 
                    };
                    break;
                case AUTO_ZONE_COLLECT_GENERATIONAL_COLLECTION:
                    collect_func =  ^{ 
                        auto_zone_generational_collection((Zone *)dispatch_get_context(dispatch_get_current_queue())); 
                    };
                    break;
                case AUTO_ZONE_COLLECT_FULL_COLLECTION:
                    collect_func =  ^{ 
                        auto_zone_full_collection((Zone *)dispatch_get_context(dispatch_get_current_queue())); 
                    };
                    break;
                case AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION:
                    collect_func =  ^{ 
                        auto_zone_exhaustive_collection((Zone *)dispatch_get_context(dispatch_get_current_queue())); 
                    };
                    break;
                default:
                    collect_func = NULL;
                    malloc_printf("%s: Unknown mode %d passed to auto_zone_collect() ignored.\n", auto_prelude(), global_mode);
                    break;
            }
            if (collect_func && azone->_collection_queue) {
                dispatch_async(azone->_collection_queue, collect_func);
            }
        }
    }
}

extern void auto_zone_reap_all_local_blocks(auto_zone_t *zone)
{
    Zone *azone = (Zone *)zone;
    Thread *thread = azone->current_thread();
    if (thread)
        thread->reap_all_local_blocks();
}

void auto_zone_collect_and_notify(auto_zone_t *zone, auto_zone_options_t options, dispatch_queue_t callback_queue, dispatch_block_t completion_callback) {
    Zone *azone = (Zone *)zone;
    auto_zone_collect(zone, options);
    if (callback_queue && completion_callback && azone->_collection_queue) {
        // ensure the proper lifetimes of the callback queue/block.
        dispatch_retain(callback_queue);
        completion_callback = Block_copy(completion_callback);
        dispatch_async(azone->_collection_queue, ^{
            dispatch_async(callback_queue, completion_callback);
            Block_release(completion_callback);
            dispatch_release(callback_queue);
        });
    }
}

void auto_zone_compact(auto_zone_t *zone, auto_zone_compact_options_t options, dispatch_queue_t callback_queue, dispatch_block_t completion_callback) {
    Zone *azone = (Zone *)zone;
    if (!azone->compaction_disabled() && azone->_collection_queue) {
        switch (options) {
        case AUTO_ZONE_COMPACT_ANALYZE: {
            if (callback_queue && completion_callback) {
                dispatch_retain(callback_queue);
                completion_callback = Block_copy(completion_callback);
            }
            dispatch_async(azone->_collection_queue, ^{
                Zone *zone = (Zone *)dispatch_get_context(dispatch_get_current_queue());
                static const char *analyze_name = Environment::get("AUTO_ANALYZE_NOTIFICATION");
                zone->analyze_heap(analyze_name);
                if (callback_queue && completion_callback) {
                    dispatch_async(callback_queue, completion_callback);
                    Block_release(completion_callback);
                    dispatch_release(callback_queue);
                }
            });
            break;
        }
        case AUTO_ZONE_COMPACT_IF_IDLE: {
            if (azone->_compaction_timer && !azone->_compaction_pending) {
                // schedule a compaction for 10 seconds in the future or _compaction_next_time, whichever is later.
                // this will be canceled if more dispatch threads arrive sooner.
                dispatch_time_t when = dispatch_time(0, 10 * NSEC_PER_SEC);
                if (when < azone->_compaction_next_time)
                    when = azone->_compaction_next_time;
                if (when != DISPATCH_TIME_FOREVER) {
                    dispatch_source_set_timer(azone->_compaction_timer, when, 0, 0);
                    azone->_compaction_pending = true;
                }
            }
            break;
        }
        case AUTO_ZONE_COMPACT_NO_OPTIONS: {
            if (callback_queue && completion_callback) {
                dispatch_retain(callback_queue);
                completion_callback = Block_copy(completion_callback);
            }
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC), azone->_collection_queue, ^{
                Zone *zone = (Zone *)dispatch_get_context(dispatch_get_current_queue());
                zone->compact_heap();
                if (callback_queue && completion_callback) {
                    dispatch_async(callback_queue, completion_callback);
                    Block_release(completion_callback);
                    dispatch_release(callback_queue);
                }
            });
            break;
        }
        }
    }
}

void auto_zone_disable_compaction(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    azone->disable_compaction();
}

void auto_zone_register_resource_tracker(auto_zone_t *zone, const char *description, boolean_t (^should_collect)(void))
{
    Zone *azone = (Zone *)zone;
    azone->register_resource_tracker(description, should_collect);
}

void auto_zone_unregister_resource_tracker(auto_zone_t *zone, const char *description)
{
    Zone *azone = (Zone *)zone;
    azone->unregister_resource_tracker(description);
}


boolean_t auto_zone_is_valid_pointer(auto_zone_t *zone, const void *ptr) {
    auto_block_info_sieve<AUTO_BLOCK_INFO_IS_BLOCK> sieve((Zone *)zone, (void *)ptr);
    return sieve.is_block();
}

size_t auto_zone_size(auto_zone_t *zone, const void *ptr) {
    auto_block_info_sieve<AUTO_BLOCK_INFO_SIZE> sieve((Zone *)zone, (void *)ptr);
    return sieve.size();
}

const void *auto_zone_base_pointer(auto_zone_t *zone, const void *ptr) {
    auto_block_info_sieve<AUTO_BLOCK_INFO_BASE_POINTER> sieve((Zone *)zone, (void *)ptr);
    return sieve.base();
}

#if DEBUG
void *WatchPoint = (void *)-1L;
void blainer() {
    sleep(0);
}
#endif


static inline void *auto_malloc(auto_zone_t *zone, size_t size) {
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    void *result = azone->block_allocate(thread, size, AUTO_MEMORY_UNSCANNED, false, true);
    return result;
}

// Sieve class that deallocates a block.
class auto_free_sieve : public sieve_base {
    Zone *_zone;
    
public:
    
    auto_free_sieve(Zone *zone, const void *ptr) __attribute__((always_inline)) : _zone(zone) {
        sieve_base_pointer(zone, ptr, *this);
    }
    
    template <class BlockRef> inline void processBlock(BlockRef ref) TEMPLATE_INLINE {
        unsigned refcount = ref.refcount();
        if (refcount != 1) {
            malloc_printf("*** free() called on collectable block with %p with refcount %d (ignored)\n", ref.address(), refcount);
        } else {
            _zone->block_deallocate(ref);
        }
    }
    
    inline void nonBlock(const void *ptr) {
        if (ptr != NULL)
            error("Deallocating a non-block", ptr);
    }
};

static void auto_free(auto_zone_t *azone, void *ptr) {
    auto_free_sieve sieve((Zone *)azone, (void *)ptr);
}

static void *auto_calloc(auto_zone_t *zone, size_t size1, size_t size2) {
    Zone *azone = (Zone *)zone;
    size1 *= size2;
    void *ptr;
    Thread &thread = azone->registered_thread();
    ptr = azone->block_allocate(thread, size1, AUTO_MEMORY_UNSCANNED, true, true);
    return ptr;
}

static void *auto_valloc(auto_zone_t *zone, size_t size) {
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    void *result = azone->block_allocate(thread, auto_round_page(size), AUTO_MEMORY_UNSCANNED, true, true);
    return result;
}

static void *auto_realloc(auto_zone_t *zone, void *ptr, size_t size) {
    Zone *azone = (Zone*)zone;
    if (!ptr) return auto_malloc(zone, size);
    
    auto_block_info_sieve<AUTO_BLOCK_INFO_SIZE|AUTO_BLOCK_INFO_LAYOUT|AUTO_BLOCK_INFO_REFCOUNT> block_info(azone, (void *)ptr);
    size_t block_size = block_info.size();
    auto_memory_type_t layout = block_info.layout();

    // preserve the layout type, and retain count of the realloc'd object.
    
    if (!block_info.is_block()) {
        auto_error(azone, "auto_realloc: can't get type or retain count, ptr from ordinary malloc zone?", ptr);
        // If we're here because someone used the wrong zone we should let them have what they intended.
        return malloc_zone_realloc(malloc_zone_from_ptr(ptr), ptr, size);
    }
    
    // malloc man page says to allocate a "minimum sized" object if size==0
    if (size == 0) size = allocate_quantum_small;
    
    if (block_size >= size) {
        size_t delta = block_size - size;
        // When reducing the size check if the reduction would result in a smaller block being used. If not, reuse the same block.
        // We can reuse the same block if any of these are true:
        // 1) original is a small block, reduced by less than small quanta
        // 2) original is a medium block, new size is still medium, and reduced by less than medium quanta
        // 3) original is a large block, new size is still large, and block occupies the same number of pages
        if ((block_size <= allocate_quantum_medium && delta < allocate_quantum_small) ||
            (block_size <= allocate_quantum_large && size >= allocate_quantum_medium && delta < allocate_quantum_medium) ||
            (size > allocate_quantum_large && auto_round_page(block_size) == auto_round_page(size))) {
            // if the block is scanned, resizing smaller should clear the extra space
            if (layout == AUTO_MEMORY_SCANNED)
                bzero(displace(ptr,size), delta);
            else if (layout == AUTO_MEMORY_ALL_WEAK_POINTERS)
                weak_unregister_range(azone, (void **)displace(ptr, size), delta / sizeof(void*));
            return ptr;
        }
    }
    
    // We could here optimize realloc by adding a primitive for small blocks to try to grow in place
    // But given that this allocator is intended for objects, this is not necessary
    Thread &thread = azone->registered_thread();
    void *new_ptr = azone->block_allocate(thread, size, layout, is_allocated_cleared(layout), (block_info.refcount() != 0));
    if (new_ptr) {
        size_t min_size = MIN(size, block_size);
        if (is_scanned(layout)) {
            auto_zone_write_barrier_memmove((auto_zone_t *)azone, new_ptr, ptr, min_size);
        } else if (layout == AUTO_MEMORY_ALL_WEAK_POINTERS) {
            memmove(new_ptr, ptr, min_size);
            Auto::SpinLock lock(&azone->weak_refs_table_lock);
            weak_transfer_weak_contents_unscanned(azone, (void **)ptr, (void **)new_ptr, min_size, false);
            if (block_size > size) weak_unregister_range_no_lock(azone, (void **)displace(ptr, size), (block_size - size) / sizeof(void*));
        } else {
            memmove(new_ptr, ptr, min_size);
        }
        
        // BlockRef FIXME: we have already categorized ptr above, we should not need to do it again here
        if (block_info.refcount() != 0) auto_zone_release(zone, ptr); // don't forget to let go rdar://6593098
    }
    
    // Don't bother trying to eagerly free old memory, even if it seems to be from malloc since,
    // well, that's just a hueristic that can be wrong.  In particular CF has on occasion bumped
    // the refcount of GC memory to guard against use in unregistered threads, and we don't know
    // how often or where this practice has spread. rdar://6063041

    return new_ptr;
}

static unsigned	auto_batch_malloc(auto_zone_t *zone, size_t size, void **results, unsigned num_requested) {
    return auto_zone_batch_allocate(zone, size, AUTO_MEMORY_UNSCANNED, true, false, results, num_requested);
}

static void auto_zone_destroy(auto_zone_t *zone) {
    Zone *azone = (Zone*)zone;
    auto_error(azone, "auto_zone_destroy", zone);
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

static char *b2s(uint64_t bytes, char *buf, int bufsize) {
    if (bytes < 1024) {
        snprintf(buf, bufsize, "%4llu bytes", bytes);
    } else if (bytes < 1024*1024) {
        snprintf(buf, bufsize, "%4.3g Kb", (float)bytes / 1024);
    } else if (bytes < 1024*1024*1024) {
        snprintf(buf, bufsize, "%4.3g Mb", (float)bytes / (1024*1024));
    } else {
        snprintf(buf, bufsize, "%4.3g Gb", (float)bytes / (1024*1024*1024));
    }
    return buf;
}

static void auto_zone_print(malloc_zone_t *zone, boolean_t verbose) {
    malloc_statistics_t stats;
    Zone *azone = (Zone *)zone;
    azone->malloc_statistics(&stats);
    char    buf1[256];
    char    buf2[256];
    printf("auto zone %p: in_use=%u  used=%s allocated=%s\n", azone, stats.blocks_in_use, b2s(stats.size_in_use, buf1, sizeof(buf1)), b2s(stats.size_allocated, buf2, sizeof(buf2)));
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

static void auto_malloc_statistics(malloc_zone_t *zone, malloc_statistics_t *stats) {
    Zone *azone = (Zone *)zone;
    azone->malloc_statistics(stats);
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
    auto_malloc_zone_locked,
    auto_zone_enable_collection_checking,
    auto_zone_disable_collection_checking,
    auto_zone_track_pointer,
    (void (*)(malloc_zone_t *, void (^)(void *,void *)))auto_zone_enumerate_uncollected
};

struct malloc_introspection_t auto_zone_introspection() {
    return auto_zone_introspect;
}

static auto_zone_t *gc_zone = NULL;

// DEPRECATED
auto_zone_t *auto_zone(void) {
    return gc_zone;
}

auto_zone_t *auto_zone_from_pointer(void *pointer) {
    malloc_zone_t *zone = malloc_zone_from_ptr(pointer);
    return (zone && zone->introspect == &auto_zone_introspect) ? zone : NULL;
}

static void * volatile queues[__PTK_FRAMEWORK_GC_KEY9-__PTK_FRAMEWORK_GC_KEY0+1];
static void * volatile pressure_sources[__PTK_FRAMEWORK_GC_KEY9-__PTK_FRAMEWORK_GC_KEY0+1];
static void * volatile compaction_timers[__PTK_FRAMEWORK_GC_KEY9-__PTK_FRAMEWORK_GC_KEY0+1];

// there can be several autonomous auto_zone's running, in theory at least.
auto_zone_t *auto_zone_create(const char *name) {
    aux_init();
    pthread_key_t key = Zone::allocate_thread_key();
    if (key == 0) return NULL;
    
    Zone  *azone = new Zone(key);
    azone->basic_zone.size = auto_zone_size;
    azone->basic_zone.malloc = auto_malloc;
    azone->basic_zone.free = auto_free;
    azone->basic_zone.calloc = auto_calloc;
    azone->basic_zone.valloc = auto_valloc;
    azone->basic_zone.realloc = auto_realloc;
    azone->basic_zone.destroy = auto_zone_destroy;
    azone->basic_zone.batch_malloc = auto_batch_malloc;
    azone->basic_zone.zone_name = name; // ;
    azone->basic_zone.introspect = &auto_zone_introspect;
    azone->basic_zone.version = AUTO_ZONE_VERSION;
    azone->basic_zone.memalign = NULL;
    // mark version field with current size of structure.
    azone->control.version = sizeof(auto_collection_control_t);
    azone->control.disable_generational = Environment::read_bool("AUTO_DISABLE_GENERATIONAL", false);
    azone->control.malloc_stack_logging = (Environment::get("MallocStackLogging") != NULL  ||  Environment::get("MallocStackLoggingNoCompact") != NULL);
    azone->control.log = AUTO_LOG_NONE;
    if (Environment::read_bool("AUTO_LOG_TIMINGS"))     azone->control.log |= AUTO_LOG_TIMINGS;
    if (Environment::read_bool("AUTO_LOG_ALL"))         azone->control.log |= AUTO_LOG_ALL;
    if (Environment::read_bool("AUTO_LOG_COLLECTIONS")) azone->control.log |= AUTO_LOG_COLLECTIONS;
    if (Environment::read_bool("AUTO_LOG_REGIONS"))     azone->control.log |= AUTO_LOG_REGIONS;
    if (Environment::read_bool("AUTO_LOG_UNUSUAL"))     azone->control.log |= AUTO_LOG_UNUSUAL;
    if (Environment::read_bool("AUTO_LOG_WEAK"))        azone->control.log |= AUTO_LOG_WEAK;

    azone->control.collection_threshold = (size_t)Environment::read_long("AUTO_COLLECTION_THRESHOLD", 1024L * 1024L);
    azone->control.full_vs_gen_frequency = Environment::read_long("AUTO_COLLECTION_RATIO", 10);

    malloc_zone_register((auto_zone_t*)azone);

    pthread_mutex_init(&azone->_collection_mutex, NULL);
    
    // register our calling thread so that the zone is ready to go
    azone->register_thread();
    
    if (!gc_zone) gc_zone = (auto_zone_t *)azone;   // cache first one for debugging, monitoring

    return (auto_zone_t*)azone;
}

/*********  Reference counting  ************/

void auto_zone_retain(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
    auto_refcount_sieve<AUTO_REFCOUNT_INCREMENT> refcount_sieve(azone, ptr);
#if DEBUG
    if (ptr == WatchPoint) {
        malloc_printf("auto_zone_retain watchpoint: %p\n", WatchPoint);
        blainer();
    }
#endif
    if (__auto_reference_logger) __auto_reference_logger(AUTO_RETAIN_EVENT, ptr, uintptr_t(refcount_sieve.refcount));
    if (Environment::log_reference_counting && malloc_logger) {
        malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(zone), auto_zone_size(zone, ptr), 0, uintptr_t(ptr), 0);
    }
}

unsigned int auto_zone_release(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
    auto_refcount_sieve<AUTO_REFCOUNT_DECREMENT> refcount_sieve(azone, ptr);

#if DEBUG
    if (ptr == WatchPoint) {
        malloc_printf("auto_zone_release watchpoint: %p\n", WatchPoint);
        blainer();
    }
#endif
    if (__auto_reference_logger) __auto_reference_logger(AUTO_RELEASE_EVENT, ptr, uintptr_t(refcount_sieve.refcount));
    if (Environment::log_reference_counting && malloc_logger) {
        malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(zone), uintptr_t(ptr), 0, 0, 0);
    }
    return refcount_sieve.refcount;
}


unsigned int auto_zone_retain_count(auto_zone_t *zone, const void *ptr) {
    auto_block_info_sieve<AUTO_BLOCK_INFO_REFCOUNT> refcount_sieve((Zone *)zone, ptr);
    return refcount_sieve.refcount();
}

/*********  Write-barrier   ************/


// BlockRef FIXME: retire
static void handle_resurrection(Zone *azone, const void *recipient, bool recipient_is_block, const void *new_value, size_t offset) 
{
    if (!recipient_is_block || ((auto_memory_type_t)azone->block_layout((void*)recipient) & AUTO_UNSCANNED) != AUTO_UNSCANNED) {
        auto_memory_type_t new_value_type = (auto_memory_type_t) azone->block_layout((void*)new_value);
        char msg[256];
        snprintf(msg, sizeof(msg), "resurrection error for block %p while assigning %p[%d] = %p", new_value, recipient, (int)offset, new_value);
        if ((new_value_type & AUTO_OBJECT_UNSCANNED) == AUTO_OBJECT) {
            // mark the object for zombiehood.
            bool thread_local = false;
            if (azone->in_subzone_memory((void*)new_value)) {
                Subzone *sz = Subzone::subzone((void*)new_value);
                usword_t q = sz->quantum_index((void*)new_value);
                if (sz->is_thread_local(q)) {
                    thread_local = true;
                    Thread &thread = azone->registered_thread();
                    ThreadLocalCollector *tlc = thread.thread_local_collector();
                    if (tlc) {
                        thread.thread_local_collector()->add_zombie((void*)new_value);
                    } else {
                        auto_error(azone, "resurrection of thread local garbage belonging to another thread", new_value);
                    }
                }
            }
            auto_zone_retain((auto_zone_t*)azone, (void*)new_value); // mark the object ineligible for freeing this time around.
            if (!thread_local) {
                azone->add_zombie((void*)new_value);
            }
            if (azone->control.name_for_address) {
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

template <class BlockRef> static void handle_resurrection(Zone *azone, void *recipient, BlockRef new_value, size_t offset) 
{
    char msg[256];
    snprintf(msg, sizeof(msg), "resurrection error for block %p while assigning %p[%d] = %p", new_value.address(), recipient, (int)offset, new_value.address());
    if (new_value.is_object()) {
        // mark the object for zombiehood.
        bool thread_local = false;
        if (new_value.is_thread_local()) {
            thread_local = true;
            Thread &thread = azone->registered_thread();
            ThreadLocalCollector *tlc = thread.thread_local_collector();
            if (tlc) {
                thread.thread_local_collector()->add_zombie((void*)new_value.address());
            } else {
                auto_error(azone, "resurrection of thread local garbage belonging to another thread", new_value.address());
            }
        }
        auto_zone_retain((auto_zone_t*)azone, (void*)new_value.address()); // mark the object ineligible for freeing this time around.
        if (!thread_local) {
            azone->add_zombie((void*)new_value.address());
        }
        if (azone->control.name_for_address) {
            char *recipient_name = azone->control.name_for_address((auto_zone_t *)azone, (vm_address_t)recipient, offset);
            char *new_value_name = azone->control.name_for_address((auto_zone_t *)azone, (vm_address_t)new_value.address(), 0);
            snprintf(msg, sizeof(msg), "resurrection error for object %p while assigning %s(%p)[%d] = %s(%p)",
                     new_value.address(), recipient_name, recipient, (int)offset, new_value_name, new_value.address());
            free(recipient_name);
            free(new_value_name);
        }
    }
    malloc_printf("%s\ngarbage pointer stored into reachable memory, break on auto_zone_resurrection_error to debug\n", msg);
    auto_zone_resurrection_error();
}

template <class DestBlockRef, class ValueBlockRef> static void handle_resurrection(Zone *azone, DestBlockRef recipient, ValueBlockRef new_value, size_t offset) 
{
    if (recipient.is_scanned()) {
        handle_resurrection(azone, recipient.address(), new_value, offset);
    }
}

// make the resurrection test an inline to be as fast as possible in the write barrier
// recipient may be a GC block or not, as determined by recipient_is_block
// returns true if a resurrection occurred, false if not
// BlockRef FIXME: retire
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

template <class DestBlockRef, class ValueBlockRef> inline static bool check_resurrection(Thread &thread, Zone *azone, DestBlockRef recipient, ValueBlockRef new_value, size_t offset) {
    if (new_value.is_garbage() && (!recipient.is_garbage())) {
        handle_resurrection(azone, recipient, new_value, offset);
        return true;
    }
    return false;
}

template <class BlockRef> inline static bool check_resurrection(Thread &thread, Zone *azone, void *global_recipient, BlockRef new_value, size_t offset) {
    if (new_value.is_garbage()) {
        handle_resurrection(azone, global_recipient, new_value, offset);
        return true;
    }
    return false;
}

//
// set_write_barrier_dest_sieve
//
// set_write_barrier_dest_sieve performs write barrier processing based on the block type of the destination.
// If the destination is a GC block then a resurrection check is performed and the assignment is done.
// Otherwise no operation is performed.
template <class ValueBlockRef> class set_write_barrier_dest_sieve : public sieve_base {
public:
    Zone *_zone;
    const void *_dest;
    ValueBlockRef _new_value;
    const void *_new_value_addr;
    bool _result;

    set_write_barrier_dest_sieve(Zone *zone, const void *dest, ValueBlockRef new_value, const void *new_value_addr) __attribute__((always_inline)) : _zone(zone), _dest(dest), _new_value(new_value), _new_value_addr(new_value_addr), _result(true) {
        sieve_interior_pointer(_zone, _dest, *this);
    }
    
    template <class DestBlockRef> inline void processBlock(DestBlockRef ref) TEMPLATE_INLINE {
        Thread &thread = _zone->registered_thread();
        size_t offset_in_bytes = (char *)_dest - (char *)ref.address();
        check_resurrection(thread, _zone, ref, _new_value, offset_in_bytes);

        if (Environment::unscanned_store_warning && _zone->compaction_enabled() && !ref.is_scanned() && !_new_value.has_refcount()) {
            auto_error(_zone, "auto_zone_set_write_barrier:  Storing a GC-managed pointer in unscanned memory location. Break on auto_zone_unscanned_store_error() to debug.", _new_value_addr);
            auto_zone_unscanned_store_error(_dest, _new_value_addr);
        }

        _zone->set_write_barrier(thread, ref, (const void **)_dest, _new_value, _new_value_addr);
    }

    inline void nonBlock(const void *ptr) __attribute__((always_inline)) {
        Thread &thread = _zone->registered_thread();
        if (thread.is_stack_address((void *)_dest)) {
            *(const void **)_dest = _new_value_addr;
        } else {
            if (Environment::unscanned_store_warning && _zone->compaction_enabled() && !_new_value.has_refcount() && !_zone->is_global_address((void*)_dest)) {
                auto_error(_zone, "auto_zone_set_write_barrier:  Storing a GC-managed pointer in unscanned memory location. Break on auto_zone_unscanned_store_error() to debug.", _new_value_addr);
                auto_zone_unscanned_store_error(_dest, _new_value_addr);
            }
            _result = false;
        }
    }
};

//
// set_write_barrier_value_sieve
//
// set_write_barrier_value_sieve determines whether the value being assigned is a block start pointer.
// If it is, then set_write_barrier_dest_sieve is used to do further write barrier procession based on the destination.
// If it is not then the value is simply assigned.
class set_write_barrier_value_sieve : public sieve_base {
public:
    Zone *_zone;
    const void *_dest;
    const void *_new_value;
    bool _result;
    
    set_write_barrier_value_sieve(Zone *zone, const void *dest, const void *new_value) __attribute__((always_inline)) : _zone(zone), _dest(dest), _new_value(new_value), _result(true) {
        sieve_base_pointer(_zone, _new_value, *this);
    }
    
    template <class BlockRef> inline void processBlock(BlockRef ref) TEMPLATE_INLINE {
        set_write_barrier_dest_sieve<BlockRef> dest(_zone, _dest, ref, _new_value);
        _result = dest._result;
    }

    inline void nonBlock(const void *ptr) __attribute__((always_inline)) {
        *(void **)_dest = (void *)ptr;
    }
};

// called by objc assignIvar assignStrongCast
boolean_t auto_zone_set_write_barrier(auto_zone_t *zone, const void *dest, const void *new_value) {
    set_write_barrier_value_sieve value((Zone *)zone, dest, new_value);
    return value._result;
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
        size_t block_size = auto_zone_size(zone, base);
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
            thread.track_local_memcopy(ptrSrc, dst, ptrSize);
            if (azone->set_write_barrier_range(dst, size)) {
                // must hold enlivening lock for duration of the move; otherwise if we get scheduled out during the move
                // and GC starts and scans our destination before we finish filling it with unique values we lose them
                UnconditionalBarrier barrier(thread.needs_enlivening());
                if (barrier) {
                    // add all values in the range.
                    // We could/should only register those that are as yet unmarked.
                    // We also only add values that are objects.
                    void **start = (void **)ptrSrc;
                    void **end = (void **)displace(start, ptrSize);
                    while (start < end) {
                        void *candidate = *start++;
                        if (azone->is_block(candidate)) thread.enliven_block(candidate);
                    }
                }
                return memmove(dst, src, size);
            }
        }
    } else if (base == NULL) {
        // since dst is not in out heap, it is by definition unscanned. unfortunately, many clients already use this on malloc()'d blocks, so we can't warn for that case.
        // if the source pointer comes from a scanned block in our heap, and the destination pointer is in global data, warn about bypassing global write-barriers.
        void *srcbase = azone->block_start((void*)src);
        if (srcbase && is_scanned(azone->block_layout(srcbase)) && azone->is_global_address(dst)) {
            // make this a warning in SnowLeopard.
            auto_error(zone, "auto_zone_write_barrier_memmove:  Copying a scanned block into global data. Break on auto_zone_global_data_memmove_error() to debug.", dst);
            auto_zone_global_data_memmove_error();
        }
    }
    // perform the copy
    return memmove(dst, src, size);
}

#define CHECK_STACK_READS 0

void *auto_zone_strong_read_barrier(auto_zone_t *zone, void **source) {
    // block a thread during compaction.
    void *volatile *location = (void *volatile *)source;
    void *value = *location;
    Zone *azone = (Zone*)zone;
    if (azone->in_subzone_memory(value)) {
        Thread &thread = azone->registered_thread();
        if (CHECK_STACK_READS) {
            // TODO:  how common are indirections through the stack?
            // allow reads from the stack without blocking, since these will always be pinned.
            if (thread.is_stack_address(source)) return value;
        }
        SpinLock lock(&thread.in_compaction().lock);
        value = *location;
        usword_t q;
        Subzone *subzone = Subzone::subzone(value);
        if (subzone->block_start(value, q)) subzone->mark_pinned(q);
    }
    return value;
}

/*********  Layout  ************/

void* auto_zone_allocate_object(auto_zone_t *zone, size_t size, auto_memory_type_t type, boolean_t initial_refcount_to_one, boolean_t clear) {
    void *ptr;
//    if (allocate_meter) allocate_meter_start();
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    // ALWAYS clear if scanned memory <rdar://problem/5341463>.
    // ALWAYS clear if type is AUTO_MEMORY_ALL_WEAK_POINTERS.
    ptr = azone->block_allocate(thread, size, type, clear || is_allocated_cleared(type), initial_refcount_to_one);
    // We only log here because this is the only entry point that normal malloc won't already catch
    if (ptr && malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE | (clear ? MALLOC_LOG_TYPE_CLEARED : 0), uintptr_t(zone), size, initial_refcount_to_one ? 1 : 0, uintptr_t(ptr), 0);
//    if (allocate_meter) allocate_meter_stop();
    return ptr;
}

extern unsigned auto_zone_batch_allocate(auto_zone_t *zone, size_t size, auto_memory_type_t type, boolean_t initial_refcount_to_one, boolean_t clear, void **results, unsigned num_requested) {
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    unsigned count = azone->batch_allocate(thread, size, type, clear || is_allocated_cleared(type), initial_refcount_to_one, results, num_requested);
    if (count && malloc_logger) {
        for (unsigned i=0; i<count; i++)
            malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE | MALLOC_LOG_TYPE_CLEARED, uintptr_t(zone), size, initial_refcount_to_one ? 1 : 0, uintptr_t(results[i]), 0);
    }
    return count;
}

extern "C" void *auto_zone_create_copy(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;

    auto_block_info_sieve<AUTO_BLOCK_INFO_SIZE|AUTO_BLOCK_INFO_LAYOUT|AUTO_BLOCK_INFO_REFCOUNT> block_info(azone, ptr);

    auto_memory_type_t type; 
    int rc = 0;
    size_t size;
    if (block_info.is_block()) {
        type = block_info.layout();
        rc = block_info.refcount();
        size = block_info.size();
    } else {
        // from "somewhere else"
        type = AUTO_MEMORY_UNSCANNED;
        rc = 0;
        size = malloc_size(ptr);
    }
    
    if (type & AUTO_OBJECT) {
        // if no weak layouts we could be more friendly
        auto_error(azone, "auto_zone_copy_memory called on object", ptr);
        return (void *)0;
    }
    void *result = auto_zone_allocate_object(zone, size, type, (rc == 1), false);
    if (result) memmove(result, ptr, size);
    return result;
}

// Change type to non-object.  This happens when, obviously, no finalize is needed.
void auto_zone_set_nofinalize(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
    auto_memory_type_t type = azone->block_layout(ptr);
    if (type == AUTO_TYPE_UNKNOWN) return;
    // preserve scanned-ness but drop AUTO_OBJECT
    if ((type & AUTO_OBJECT) && azone->weak_layout_map_for_block(ptr))
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

// Turn on the AUTO_POINTERS_ONLY flag for scanned blocks only. This tells the collector
// to treat the remainder of an object as containing pointers only, which is
// needed for compaction.
void auto_zone_set_scan_exactly(auto_zone_t *zone, void *ptr) {
    Zone *azone = (Zone *)zone;
    auto_memory_type_t type = azone->block_layout(ptr);
    if (type & AUTO_UNSCANNED) return;  // not appropriate for unscanned memory types.
    azone->block_set_layout(ptr, type|AUTO_POINTERS_ONLY);
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
    auto_block_info_sieve<AUTO_BLOCK_INFO_LAYOUT> block_info((Zone *)zone, ptr);
    return block_info.layout();
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

/*********  Garbage Collection and Compaction   ************/

auto_collection_control_t *auto_collection_parameters(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    return &azone->control;
}



// public entry point.
void auto_zone_statistics(auto_zone_t *zone, auto_statistics_t *stats) {
    if (!stats) return;
    bzero(stats, sizeof(auto_statistics_t));
}

// work in progress
typedef struct {
    FILE *f;
    char *buff;
    size_t buff_size;
    size_t buff_pos;
} AutoZonePrintInfo;

__private_extern__ malloc_zone_t *aux_zone;

void auto_collector_reenable(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    // although imperfect, try to avoid dropping below zero
    Mutex lock(&azone->_collection_mutex);
    if (azone->_collector_disable_count == 0) return;
    azone->_collector_disable_count--;
}

void auto_collector_disable(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    Mutex lock(&azone->_collection_mutex);
    azone->_collector_disable_count++;
}

boolean_t auto_zone_is_enabled(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    Mutex lock(&azone->_collection_mutex);
    return azone->_collector_disable_count == 0;
}

boolean_t auto_zone_is_collecting(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    // FIXME: the result of this function only valid on the collector thread (main for now).
    return !azone->is_state(idle);
}

void auto_collect_multithreaded(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    dispatch_once(&azone->_zone_init_predicate, ^{
        // In general libdispatch limits the number of concurrent jobs based on various factors (# cpus).
        // But we don't want the collector to be kept waiting while long running jobs generate garbage.
        // We avoid collection latency using a special attribute which tells dispatch this queue should 
        // service jobs immediately even if that requires exceeding the usual concurrent limit.
        azone->_collection_queue = dispatch_queue_create("Garbage Collection Work Queue", NULL);
        dispatch_queue_t target_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, DISPATCH_QUEUE_OVERCOMMIT);
        dispatch_set_target_queue(azone->_collection_queue, target_queue);
        dispatch_set_context(azone->_collection_queue, azone);
        const char *notify_name;
        
#if COMPACTION_ENABLED
        // compaction trigger:  a call to notify_post() with $AUTO_COMPACT_NOTIFICATION
        notify_name = Environment::get("AUTO_COMPACT_NOTIFICATION");
        if (notify_name != NULL) {
            int compact_token_unused = 0;
            notify_register_dispatch(notify_name, &compact_token_unused, azone->_collection_queue, ^(int token) {
                Zone *zone = (Zone *)dispatch_get_context(dispatch_get_current_queue());
                auto_date_t start = auto_date_now();
                zone->compact_heap();
                auto_date_t end = auto_date_now();
                if (Environment::log_compactions) malloc_printf("compaction took %lld microseconds.\n", (end - start));
            });
        } else {
            // compaction timer:  prime it to run forever in the future.
            azone->_compaction_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
            dispatch_source_set_timer(azone->_compaction_timer, DISPATCH_TIME_FOREVER, 0, 0);
            dispatch_source_set_event_handler(azone->_compaction_timer, ^{
                if (!azone->compaction_disabled()) {
                    azone->_compaction_next_time = DISPATCH_TIME_FOREVER;
                    dispatch_source_set_timer(azone->_compaction_timer, DISPATCH_TIME_FOREVER, 0, 0);
                    dispatch_async(azone->_collection_queue, ^{
                        auto_date_t start = auto_date_now();
                        azone->compact_heap();
                        auto_date_t end = auto_date_now();
                        malloc_printf("compaction took %lld microseconds.\n", (end - start));
                        // compute the next allowed time to start a compaction; must wait at least 30 seconds.
                        azone->_compaction_next_time = dispatch_time(0, 30 * NSEC_PER_SEC);
                        azone->_compaction_pending = false;
                    });
                }
            });
            dispatch_resume(azone->_compaction_timer);
        }
        
        // analysis trigger:  a call to notify_post() with $AUTO_ANALYZE_NOTIFICATION
        // currently used by HeapVisualizer to generate an analyis file in /tmp/AppName.analyze.
        notify_name = Environment::get("AUTO_ANALYZE_NOTIFICATION");
        if (notify_name != NULL) {
            int analyze_token_unused = 0;
            notify_register_dispatch(notify_name, &analyze_token_unused, azone->_collection_queue, ^(int token) {
                Zone *zone = (Zone *)dispatch_get_context(dispatch_get_current_queue());
                static const char *analyze_name = Environment::get("AUTO_ANALYZE_NOTIFICATION");
                zone->analyze_heap(analyze_name);
                static const char *reply_name = Environment::get("AUTO_ANALYZE_REPLY");
                if (reply_name) notify_post(reply_name);
            });
        }
#endif /* COMPACTION_ENABLED */
        
        // simulated memory pressure notification.
        notify_name = Environment::get("AUTO_MEMORY_PRESSURE_NOTIFICATION");
        if (notify_name != NULL) {
            int pressure_token_unused = 0;
            notify_register_dispatch(notify_name, &pressure_token_unused, azone->_collection_queue, ^(int token) {
                Zone *zone = (Zone *)dispatch_get_context(dispatch_get_current_queue());
                usword_t size = zone->purge_free_space();
                printf("purged %ld bytes.\n", size);
            });
        } else {
            // If not simulated, then field memory pressure triggers directly from the kernel.
            // TODO:  consider using a concurrent queue to allow purging to happen concurrently with collection/compaction.
#if TARGET_OS_IPHONE
#       warning no memory pressure dispatch source on iOS
#else
            azone->_pressure_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VM, 0, DISPATCH_VM_PRESSURE, azone->_collection_queue);
            if (azone->_pressure_source != NULL) {
                dispatch_source_set_event_handler(azone->_pressure_source, ^{
                    Zone *zone = (Zone *)dispatch_get_context(dispatch_get_current_queue());
                    zone->purge_free_space();
                });
                dispatch_resume(azone->_pressure_source);
            }
#endif
        }
        
        // exhaustive collection notification.
        notify_name = Environment::get("AUTO_COLLECT_NOTIFICATION");
        if (notify_name != NULL) {
            int collect_token_unused = 0;
            notify_register_dispatch(notify_name, &collect_token_unused, dispatch_get_main_queue(), ^(int token) {
                malloc_printf("collecting on demand.\n");
                auto_zone_collect((auto_zone_t *)azone, AUTO_ZONE_COLLECT_LOCAL_COLLECTION | AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION);
                const char *reply_name = Environment::get("AUTO_COLLECT_REPLY");
                if (reply_name) notify_post(reply_name);
            });
        }
        
        // Work around an idiosynchrocy with leaks. These dispatch objects will be reported as leaks because the
        // zone structure is not (and cannot be) scanned by leaks. Since we only support a small number of zones
        // just store these objects in global memory where leaks will find them to suppress the leak report.
        // In practice these are never deallocated anyway, as we don't support freeing an auto zone.
        queues[azone->thread_key()-__PTK_FRAMEWORK_GC_KEY0] = azone->_collection_queue;
        pressure_sources[azone->thread_key()-__PTK_FRAMEWORK_GC_KEY0] = azone->_pressure_source;
        compaction_timers[azone->thread_key()-__PTK_FRAMEWORK_GC_KEY0] = azone->_compaction_timer;
    });
}


//
// Called by Instruments to lay down a heap dump (via dumpster)
//
void auto_enumerate_references(auto_zone_t *zone, void *referent, 
                               auto_reference_recorder_t callback, // f(zone, ctx, {ref, referrer, offset})
                               void *stack_bottom, void *ctx)
{
    // obsolete. use auto_gdb_enumerate_references() or auto_zone_dump().
}


/********* Weak References ************/


// auto_assign_weak
// The new and improved one-stop entry point to the weak system
// Atomically assign value to *location and track it for zero'ing purposes.
// Assign a value of NULL to deregister from the system.
void auto_assign_weak_reference(auto_zone_t *zone, const void *value, const void **location, auto_weak_callback_block_t *block) {
    Zone *azone = (Zone *)zone;
    Thread &thread = azone->registered_thread();
    void *base = azone->block_start((void *)location);
    if (value) {
        if ((base && azone->block_is_garbage(base)) || check_resurrection(thread, azone, (void *)base, base != NULL, value, (size_t)location - (size_t)base)) {
            // Never allow garbage to be registered since it will never be cleared.
            // Go further and zero it out since it would have been cleared had it been done earlier
            // To address <rdar://problem/7217252>, disallow forming new weak references inside garbage objects.
            value = NULL;
        }
    }
    
    // Check if this is a store to the stack and don't register it.
    // This handles id *__weak as a parameter
    if (!thread.is_stack_address((void *)location)) {
        // unregister old, register new (if non-NULL)
        weak_register(azone, value, (void **)location, block);
        if (value != NULL) {
            // note: we could check to see if base is local, but then we have to change
            // all weak references on make_global.
            if (base) thread.block_escaped(base);
            thread.block_escaped((void *)value);
            // also zap destination so that dead locals don't need to be pulled out of weak tables
            //thread->track_local_assignment(azone, (void *)location, (void *)value);
        }
    } else {
        // write the value even though the location is not registered, for __block __weak foo=x case.
        *location = value;
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
        ConditionBarrier barrier(thread.needs_enlivening());
        if (barrier) {
            // need to tell the collector this block should be scanned.
            result = *referrer;
            if (result) thread.enliven_block(result);
        } else {
            result = *referrer;
        }
    }
    return result;
}

extern char CompactionObserverKey;

void auto_zone_set_compaction_observer(auto_zone_t *zone, void *block, void (^observer) (void)) {
    if (observer) {
        observer = Block_copy(observer);
        Block_release(observer);
    }
    auto_zone_set_associative_ref(zone, block, &CompactionObserverKey, observer);
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

void auto_zone_enumerate_associative_refs(auto_zone_t *zone, void *key, boolean_t (^block) (void *object, void *value)) {
    Zone *azone = (Zone*)zone;
    azone->visit_associations_for_key(key, block);
}

size_t auto_zone_get_associative_hash(auto_zone_t *zone, void *object) {
    Zone *azone = (Zone*)zone;
    return azone->get_associative_hash(object);
}

/********* Root References ************/

class auto_zone_add_root_sieve : public sieve_base {
public:
    Zone * const _zone;
    void * const _root;
    
    auto_zone_add_root_sieve(Zone *zone, void *root, void *ptr) __attribute__((always_inline)) : _zone(zone), _root(root) {
        sieve_base_pointer(zone, ptr, *this);
    }
    
    template <class BlockRef> inline void processBlock(BlockRef ref) TEMPLATE_INLINE {
        Thread &thread = _zone->registered_thread();
        check_resurrection(thread, _zone, _root, ref, 0);
        _zone->add_root(_root, ref);
    }
    
    inline void nonBlock(const void *ptr) __attribute__((always_inline)) {
        *(void **)_root = (void *)ptr;
    }
};

void auto_zone_add_root(auto_zone_t *zone, void *root, void *value)
{
    auto_zone_add_root_sieve((Zone *)zone, root, value);
}

void auto_zone_remove_root(auto_zone_t *zone, void *root) {
    ((Zone *)zone)->remove_root(root);
}

void auto_zone_root_write_barrier(auto_zone_t *zone, void *address_of_possible_root_ptr, void *value) {
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
        thread.block_escaped(value);
        // might need to tell the collector this block should be scanned.
        UnconditionalBarrier barrier(thread.needs_enlivening());
        if (barrier) thread.enliven_block(value);
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
        thread.block_escaped(value);
        check_resurrection(thread, azone, address_of_possible_root_ptr, false, value, 0);
        // Always write
        *(void **)address_of_possible_root_ptr = value;
        
        if (Environment::unscanned_store_warning && azone->compaction_enabled()) {
            // catch writes to unscanned memory.
            auto_block_info_sieve<AUTO_BLOCK_INFO_REFCOUNT> info(azone, value);
            if (info.is_block() && info.refcount() == 0) {
                auto_error(zone, "auto_zone_root_write_barrier:  Storing a GC-managed pointer in unscanned memory location. Break on auto_zone_unscanned_store_error() to debug.", value);
                auto_zone_unscanned_store_error(address_of_possible_root_ptr, value);
            }
        }
    }
}


void auto_zone_print_roots(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    PointerList roots;
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
    thread.block_escaped(newValue);
    UnconditionalBarrier barrier(thread.needs_enlivening());
    boolean_t result;
    if (issueBarrier)
        result = OSAtomicCompareAndSwapPtrBarrier(existingValue, newValue, location);
    else
        result = OSAtomicCompareAndSwapPtr(existingValue, newValue, location);
    if (!isGlobal) {
        // mark write-barrier w/o storing
        azone->set_write_barrier((char*)location);
    }
    if (result && barrier) thread.enliven_block(newValue);
    return result;
}

/************ Collection Checking ***********************/

boolean_t auto_zone_enable_collection_checking(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    azone->enable_collection_checking();
    return true;
}

void auto_zone_disable_collection_checking(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    azone->disable_collection_checking();
}

void auto_zone_track_pointer(auto_zone_t *zone, void *pointer) {
    Zone *azone = (Zone *)zone;
    if (azone->collection_checking_enabled())
        azone->track_pointer(pointer);
}

void auto_zone_enumerate_uncollected(auto_zone_t *zone, auto_zone_collection_checking_callback_t callback) {
    Zone *azone = (Zone *)zone;
    azone->enumerate_uncollected(callback);
}


/************ Experimental ***********************/

#ifdef __BLOCKS__

void auto_zone_dump(auto_zone_t *zone,
            auto_zone_stack_dump stack_dump,
            auto_zone_register_dump register_dump,
            auto_zone_node_dump thread_local_node_dump,
            auto_zone_root_dump root_dump,
            auto_zone_node_dump global_node_dump,
            auto_zone_weak_dump weak_dump
    ) {
    
    Auto::Zone *azone = (Auto::Zone *)zone;
    azone->dump_zone(stack_dump, register_dump, thread_local_node_dump, root_dump, global_node_dump, weak_dump);
}

void auto_zone_visit(auto_zone_t *zone, auto_zone_visitor_t *visitor) {
    Auto::Zone *azone = (Auto::Zone *)zone;
    azone->visit_zone(visitor);
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
        unsigned int size = auto_zone_size(zone, address);
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
#endif


/************* API ****************/

boolean_t auto_zone_atomicCompareAndSwapPtr(auto_zone_t *zone, void *existingValue, void *newValue, void *volatile *location, boolean_t issueBarrier) {
    Zone *azone = (Zone *)zone;
    return auto_zone_atomicCompareAndSwap(zone, existingValue, newValue, location, azone->is_global_address((void*)location), issueBarrier);
}

#if DEBUG
////////////////// SmashMonitor ///////////////////

static void range_check(void *pointer, size_t size) {
    Zone *azone = (Zone *)gc_zone;
    if (azone) {
        void *base_pointer = azone->block_start(pointer);
        if (base_pointer) {
            size_t block_size = auto_zone_size((auto_zone_t *)azone,base_pointer);
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
