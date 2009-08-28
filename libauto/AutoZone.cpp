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
    AutoZone.cpp
    Copyright (c) 2004-2009 Apple Inc. All rights reserved.
 */

#include "AutoAdmin.h"
#include "AutoBitmap.h"
#include "AutoBlockIterator.h"
#include "AutoCollector.h"
#include "AutoConfiguration.h"
#include "AutoDefs.h"
#include "AutoEnvironment.h"
#include "AutoLarge.h"
#include "AutoLock.h"
#include "AutoRange.h"
#include "AutoRegion.h"
#include "AutoStatistics.h"
#include "AutoSubzone.h"
#include "AutoMemoryScanner.h"
#include "AutoThread.h"
#include "AutoWriteBarrierIterator.h"
#include "AutoThreadLocalCollector.h"
#include "AutoZone.h"

#include "auto_weak.h"
#include "auto_trace.h"
#include "auto_dtrace.h"

#include <mach-o/dyld.h>
#include <mach-o/ldsyms.h>
#include <sys/mman.h>

struct auto_zone_cursor {
    auto_zone_t *zone;
    size_t garbage_count;
    const vm_address_t *garbage;
    volatile unsigned index;
    size_t block_count;
    size_t byte_count;
};


namespace Auto {

#if defined(DEBUG)
#warning DEBUG is set
#endif


    //
    // Shared information
    //
    Zone *Zone::_first_zone = NULL;
    volatile int32_t Zone::_zone_count = 0;
    
    //
    // setup_shared
    //
    // Initialize information used by all zones.
    //
    void Zone::setup_shared() {
        // initialize the environment
        Environment::initialize();
        
        // if auxiliary malloc zone hasn't been allocated, use the default zone.
        if (!aux_zone && !Zone::zone()) {
            aux_zone = malloc_default_zone();
        }
    }
    
    pthread_key_t Zone::allocate_thread_key() {
        pthread_key_t key = __sync_fetch_and_add(&_zone_count, 1) + __PTK_FRAMEWORK_GC_KEY0;
        if (key <= __PTK_FRAMEWORK_GC_KEY9)
            return key;
        return 0;
    }
    
    //
    // Constructor
    //
    Zone::Zone(pthread_key_t thread_registration_key) 
    :   _registered_threads_key(thread_registration_key), _page_allocator(_stats), _garbage_list(_page_allocator)
    {
        ASSERTION(page_size == vm_page_size);
    
        // Force non-lazy symbol binding for libauto and below, 
        // to prevent deadlocks in dyld while we stop threads.
        // This is deprecated API yet no new way to do this yet (Oct08), rdar unknown
        NSLookupSymbolInImage((const mach_header*)&_mh_dylib_header, "___", NSLOOKUPSYMBOLINIMAGE_OPTION_BIND_FULLY);

        // check to see if global information initialized
        static dispatch_once_t is_auto_initialized = 0;
        dispatch_once(&is_auto_initialized, ^{ setup_shared(); });
        
        // zone is at the beginning of data
        void *next = displace(this, admin_offset());
        
        // initialize basic zone information
        _registered_threads = NULL;
        pthread_key_init_np(_registered_threads_key, destroy_registered_thread);
        
        // <rdar://problem/6456504>:  Set up the registered threads mutex to be recursive, so that scan_registered_threads()
        // can reenter the mutex even when block_collector() has been called during heap monitoring.
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&_registered_threads_mutex, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);
        pthread_mutex_init(&_mark_bits_mutex, NULL);
        
        _enlivening_enabled = false;
        _enlivening_complete = false;
        
        // initialize subzone tracking bit map
        _in_subzone.initialize(subzone_quantum_max, next);
        next = displace(next, Bitmap::bytes_needed(subzone_quantum_max));
        
        // initialize large block tracking bit map
        _in_large.initialize(allocate_quantum_large_max, next);
        next = displace(next, Bitmap::bytes_needed(allocate_quantum_large_max));

#if UseArena
        // initialize arena of large block & region tracking bit map
        _large_bits.initialize(allocate_quantum_large_max, next);
        _large_bits_lock = 0;
        next = displace(next, Bitmap::bytes_needed(allocate_quantum_large_max));
        
        _arena = allocate_memory(1ul << arena_size_log2, 1ul << arena_size_log2);
        if (!_arena) {
            auto_fatal("can't allocate arena for GC\n");
        }
        
        _large_start = NULL;
        // set the coverage to everything. We probably don't need to use at all w/arenas
        _coverage.set_range(_arena, 1ul << arena_size_log2);
#else
        // set up the coverage range
        _coverage.set_range((void *)~0, (void *)0);
#endif
        
        _small_admin.initialize(this, allocate_quantum_small_log2);
        _medium_admin.initialize(this, allocate_quantum_medium_log2);
        
        // initialize the large list
        _large_list = NULL;
        _large_lock = 0;
        
        // initialize roots hash set.
        _roots_lock = 0;
        _datasegments_lock = 0;
        _zombies_lock = 0;
        
        // initialize regions list
        _region_list = NULL;
        _region_lock = 0;
        _retains_lock = 0;
        _coverage_lock = 0;
        
        // initialize flags
        _repair_write_barrier = false;

#if GLOBAL_ENLIVENING_QUEUE
        _needs_enlivening.state = false;
        _needs_enlivening.lock = 0;
#endif

        _state = idle;
        
        // initialize statistics
        _stats.reset();
        usword_t data_size = bytes_needed();
        _stats.add_admin(data_size);
        _allocation_threshold = 0;
        
        // prime the first region
        allocate_region();
        
        if (_first_zone == NULL)
            _first_zone = this;
    }
    
    
    //
    // Destructor
    //
    Zone::~Zone() {
        // release memory used by large
        for (Large *large = _large_list; large; ) {
            Large *next = large->next();
            large->deallocate(this);
            large = next;
        }
        
        // release memory used by regions
        for (Region *region = _region_list; region != NULL; region = region->next()) { 
            Region *next = region->next();
            delete region;
            region = next;
        }
        _region_list = NULL;
                
        // we don't have a clean way to tear down registered threads and give up our tsd key
        // for now require that they already be unregistered
        if (_registered_threads != NULL)
            auto_error(this, "~Zone(): registered threads list not empty\n", NULL);
    }


    //
    // memory allocation from within arena
    //
#if UseArena  
    // low half of arena in one region, top half used for large allocations
    void *Zone::arena_allocate_large(usword_t size) {
        usword_t seeksize = (size + allocate_quantum_large - 1) & ~(allocate_quantum_large-1);
        usword_t nbits = seeksize >> allocate_quantum_large_log2;
        // look through our arena for free space on this alignment
        usword_t start = 0;
        // someday... track _first_free
        usword_t end = 1ul << (arena_size_log2 - allocate_quantum_large_log2 - 1);
        if (nbits > (end - start)) {
            return NULL;
        }
        end -= nbits; // can't find anything that big past this point :-)
        SpinLock lock(&_large_bits_lock);
        while (start <= end) {
            // actually, find last clear bit. If 0, we have an allocation, otherwise we have a new start XXX
            if (_large_bits.bits_are_clear(start, nbits)) {
                _large_bits.set_bits(start, nbits);
                return displace(_large_start, start << allocate_quantum_large_log2);
            }
            start += 1;
        }
        // out of memory
        return NULL;

    }
    
    void *Zone::arena_allocate_region(usword_t newsize) {
        // only one region when using arena
        if (_large_start) return NULL;
		
        // newsize includes room for bitmaps.  Just for sanity make sure it is large quantum aligned.
        usword_t roundedsize = (newsize + subzone_quantum - 1) & ~(subzone_quantum-1);
        _large_start = displace(_arena, roundedsize);
        return _arena;
    }
    
    //
    // raw memory deallocation
    //
    void Zone::arena_deallocate(void *address, size_t size) {
        usword_t seeksize = (size + allocate_quantum_large - 1) & ~(allocate_quantum_large-1);
        usword_t nbits = seeksize >> allocate_quantum_large_log2;
        usword_t start = ((char *)address - (char *)_large_start) >> allocate_quantum_large_log2;
        SpinLock lock(&_large_bits_lock);
        _large_bits.clear_bits(start, nbits);
        madvise(address, size, MADV_FREE);
        //if (address < _first_free) _first_free = address;
    }
#else
    // on 32-bit, goes directly to system (the entire address space is our arena)
    void *Zone::arena_allocate_large(usword_t size) {
        return allocate_memory(size, allocate_quantum_large, VM_MEMORY_MALLOC_LARGE);
    }
    
    void Zone::arena_deallocate(void *address, size_t size) {
        deallocate_memory(address, size);
    }
#endif

    
    //
    // allocate_region
    //
    // Allocate and initialize a new region of subzones
    // returns new (or existing) region, or NULL on true region allocation failure
    //
    Region *Zone::allocate_region() {
        SpinLock lock(&_region_lock);
        
        if (_region_list && _region_list->subzones_remaining() != 0) return _region_list;    // another thread helped us out

        // allocate new region
        Region *region = Region::new_region(this);
        
        // if allocated
        if (region) {
            
            {
                SpinLock lock(&_coverage_lock);

                // update coverage range
                _coverage.expand_range(*region);
            }
            
            // add to front of list
            region->set_next(_region_list);
            _region_list = region;
            
            // set up scan stack
            if (!_scan_stack.is_allocated()) {
                _scan_stack.set_range(region->scan_space());
            }
        }
        return region;
    }
    
        
    
    //
    // allocate_large
    //
    // Allocates a large block from the universal pool (directly from vm_memory.)
    //
    void *Zone::allocate_large(Thread &thread, usword_t &size, const unsigned layout, bool clear, bool refcount_is_one) {
        Large *large = Large::allocate(this, size, layout, refcount_is_one);
        void *address;
        
        {
            SpinLock lock(&_large_lock);

            // Enlivening barrier needs to wrap allocation, setting _in_large bitmap, and adding to large list.
            // Updating _in_large must be done under the enlivening lock because the collector needs _in_large
            // to be updated in order to repend the block during enlivening.
            EnliveningHelper<ConditionBarrier> barrier(thread);
            if (large) {
                address = large->address();

                // mark in large bit map
                _in_large.set_bit(Large::quantum_index(address));
                
                if (barrier) barrier.enliven_block(address);
                
                // add to large list
                large->add(_large_list);
            } else {
                return NULL;
            }
        }
        
        // get info
        size = large->size();       // reset size to be that of actual allocation
        
#if UseArena
        // <rdar://problem/6150518> explicitly clear only in 64-bit, if requested, or the block is scanned.
        if (clear || !(layout & AUTO_UNSCANNED)) {
            bzero(address, size);
        }
#endif
        
        // expand coverage of zone
        {
            SpinLock lock(&_coverage_lock);
            Range large_range(address, size);
            _coverage.expand_range(large_range);
        }
        
        // update statistics
        _stats.add_count(1);
        _stats.add_size(size);
        _stats.add_dirty(size);
        _stats.add_allocated(size);
        
        adjust_threshold_counter(size);
        
        return address;
    }
    
    
    //
    // deallocate_large
    //
    // Release memory allocated for a large block.
    //
    void Zone::deallocate_large(void *block) {
        // locate large admin information.
        Large *large = Large::large(block);
        
        // update statistics
        usword_t size = large->size();
        _stats.add_count(-1);
        _stats.add_size(-size);             // bytes in use
        _stats.add_allocated(-size);        // vm required for bytes in use (100%)
        _stats.add_dirty(-size);            // dirty bytes required for bytes in use (100%)
        
        // remove from large list
        SpinLock lock(&_large_lock);
        large->remove(_large_list);
        
        // clear in large bit map
        _in_large.clear_bit(Large::quantum_index(block));
        
        // release memory for the large block
        large->deallocate(this);
    }
    
    static inline bool locked(spin_lock_t *lock) {
        TrySpinLock attempt(lock);
        return !attempt;
    }
    
    static inline bool locked(pthread_mutex_t *lock) {
        TryMutex attempt(lock);
        return !attempt;
    }
      
    bool Zone::is_locked() {
        // TRY every lock. If any of them is held, we're considered locked.
        bool result = (locked(_small_admin.lock()) || locked(_medium_admin.lock()) ||
                       locked(&weak_refs_table_lock) || locked(&_large_lock) || locked(&_roots_lock) || 
                       locked(&_datasegments_lock) || locked(&_zombies_lock) || locked(&_region_lock) ||
                       locked(&_retains_lock) || locked(&_coverage_lock) || locked(&_associations_lock) ||
#if UseArena
                       locked(&_large_bits_lock) ||
#endif
                       locked(&_registered_threads_mutex));
        if (!result) {
            // check the current registered thread's enlivening queue, to see if it is locked.
            Thread *thread = current_thread();
            if (thread != NULL) {
                LockedBoolean &needs_enlivening = thread->needs_enlivening();
                if (locked(&needs_enlivening.lock))
                    return true;
            }
            // walk all of the regions, ensure none of them are locked, which happens when adding a new subxzone.
            for (Region *region = _region_list; region != NULL; region = region->next()) {
                if (locked(region->subzone_lock())) {
                    return true;
                }
            }
        }
        return result;
    }
    
    //
    // add_subzone
    //
    // find (or create) a region that can (and does) add a subzone to this admin
    //
    bool Zone::add_subzone(Admin *admin) {
        control.will_grow((auto_zone_t *)this, AUTO_HEAP_SUBZONE_EXHAUSTED);
        if (!_region_list->add_subzone(admin)) {
            control.will_grow((auto_zone_t *)this, AUTO_HEAP_REGION_EXHAUSTED);
            // although we don't hold the region lock between these checks, its okay
            // since allocate_region won't actually allocate if not necessary
            if (allocate_region() == NULL) return false;
        }
        return true;
    }
    
    //
    // block_allocate
    //
    // Allocate a block of memory from the zone.  layout indicates whether the block is an
    // object or not and whether it is scanned or not.
    //
    void *Zone::block_allocate(Thread &thread, const size_t size, const unsigned layout, bool clear, bool refcount_is_one) {
        void *block;
        usword_t allocated_size = size;
        bool did_grow = false;

        // make sure we allocate at least one byte
        if (!allocated_size) allocated_size = 1;

        // try thread cache first since first N sizes dominate all other allocations
        if (allocated_size <= (allocate_quantum_small * max_cached_small_multiple)) {
            // On a Core 2 Duo in 32-bit mode the pthread_specific takes about 60 microseconds
            // The spinlock that it usually avoids takes about 400 microseconds
            // The total average allocation time is now down to 960 microseconds, so the lock is phenominally expensive
            // see if we've crossed the current thread's local block allocation threshold. if so, kick off a TLC to try to recirculate some blocks.
            const bool cannotFinalizeNow = false;
            if (ThreadLocalCollector::should_collect(this, thread, cannotFinalizeNow)) {
                ThreadLocalCollector tlc(this, (void*)auto_get_sp(), thread);
                tlc.collect(cannotFinalizeNow);
            }
            do {
                block = _small_admin.thread_cache_allocate(thread, allocated_size, layout, refcount_is_one, did_grow);
            } while (!block && add_subzone(&_small_admin));
        } else if (allocated_size < allocate_quantum_medium) {
            // use small admin
            do {
                block = _small_admin.find_allocation(thread, allocated_size, layout, refcount_is_one, did_grow);
            } while (!block && add_subzone(&_small_admin));
        } else if (allocated_size < allocate_quantum_large) {
            // use medium admin
            do {
                block = _medium_admin.find_allocation(thread, allocated_size, layout, refcount_is_one, did_grow);
            } while (!block && add_subzone(&_medium_admin));
        } else {
            // allocate more directly (32 bit: from vm, 64 bit: from top of arena)
            block = allocate_large(thread, allocated_size, layout, clear, refcount_is_one);
        }
    
        // if we could not allocate memory then we return here
        if (block == NULL) return NULL;
            
        // update statistics for non-cache allocation
        _stats.add_count(1);
        _stats.add_size(allocated_size);
        adjust_threshold_counter(allocated_size);
        
        if (threshold_reached() && multithreaded) {
            auto_collect((auto_zone_t *)this, AUTO_COLLECT_RATIO_COLLECTION, NULL); // rdar:... Don't trip the thread local collector...
        }

        if (did_grow) {
            control.will_grow((auto_zone_t *)this, AUTO_HEAP_HOLES_EXHAUSTED);
        }

        // <rdar://problem/6150518> large blocks always come back fully cleared, either by VM itself, or by a bzero() in allocate_large().
        if (allocated_size >= allocate_quantum_large) return block;

        // initialize block
        if (clear) {
            void **end = (void **)displace(block, allocated_size);
            switch (allocated_size/sizeof(void *)) {

            case 12: end[-12] = NULL;
            case 11: end[-11] = NULL;
            case 10: end[-10] = NULL;
            case 9: end[-9] = NULL;
            case 8: end[-8] = NULL;
            case 7: end[-7] = NULL;
            case 6: end[-6] = NULL;
            case 5: end[-5] = NULL;
            case 4: end[-4] = NULL;
            case 3: end[-3] = NULL;
            case 2: end[-2] = NULL;
            case 1: end[-1] = NULL;
            case 0: break;
            default:
                bzero(block, allocated_size);
                break;
            }
        }
        
#if RECORD_REFCOUNT_STACKS
        if (AUTO_RECORD_REFCOUNT_STACKS) {
            auto_record_refcount_stack(this, ptr, 0);
        }
#endif
#if LOG_TIMINGS
        size_t allocated = _stats.size();
        if ((allocated & ~(LOG_ALLOCATION_THRESHOLD-1)) != ((allocated - size) & ~(LOG_ALLOCATION_THRESHOLD-1)))
            log_allocation_threshold(auto_date_now(), _stats.size(), _stats.allocated());
#endif
        return block;
    }
    //
    // block_deallocate
    //
    // Release a block of memory from the zone, lazily while scanning.
    // Called exclusively from malloc_zone_free() which means that we can assume
    // that the memory has refcount 1.  NSDeallcoateObject knows this and sends us
    // scanned items which we can't immediately reclaim if we are indeed scanning them
    // since we don't want the scanner to see an object at one instant and then have the
    // memory (or isa reference) go crazy on us.
    // 
    void Zone::block_deallocate(void *block) {
        
        // explicitly deallocated blocks must have no associations.
        erase_associations(block);
        
        if (in_subzone_memory(block)) {
            // TODO:  we could handle ALL block freeing this way, instead of swapping the semantics of deallocate_large().
            Subzone *subzone = Subzone::subzone(block);
            if (!subzone->block_is_start(block)) return;  // might be a bogus address
            SpinLock adminLock(subzone->admin()->lock());
            // XXX could build single operation to update side data once instead of two operations here
            dec_refcount_small_medium(subzone, block);
            int layout = subzone->layout(block);
            if (layout & AUTO_OBJECT)
                erase_weak(block);
            // enlivening_enabled only changes while admin lock held
            // it indicates that a collection is in progress.
            // Other scanners though - gdb/dump are vulnerable if they don't also hold the admin locks.
            // They can't properly iterate via the admin if we're coalescing.
            if (((layout & AUTO_UNSCANNED) == AUTO_UNSCANNED) && !_enlivening_enabled) {
                subzone->admin()->deallocate_no_lock(block);    // ignore object -finalize work
            }
            else {
                // Inhibit finalization for NSDeallocateObject()'d objects.
                // Let collector find this next round when scanner can't be looking at it
                // Lose "Scanned" & possibly "Object"
                // XXX we could chain these and make all scanners process them
                subzone->set_layout(block, AUTO_MEMORY_UNSCANNED);
            }
        } else if (in_large_memory(block) && Large::is_start(block)) {
            Large *large = Large::large(block);
            int layout = large->layout();
            if (layout & AUTO_OBJECT)
                erase_weak(block);
            large->set_layout(AUTO_MEMORY_UNSCANNED);
#if USE_DISPATCH_QUEUE
            // use the dispatch queue to deallocate the block "immediately"
            // note that we keep a refcount on block so it won't also be found by the collector
            if (collection_queue) {
                Zone *zone = this;
                dispatch_async(collection_queue, ^{ zone->deallocate_large(block); });
            }
#else
            block_decrement_refcount(block);
#endif
         } else {
            error("Deallocating a non-block", block);
        }
    }
    

    //
    // block_start_large
    // 
    // Return the start of a large block.
    //
    void *Zone::block_start_large(void *address) {
        // Note that coverage is updated *after* a large is allocated. It may be possible for this test to fail on a large in the process of being allocated.
        if (_coverage.in_range(address)) {
            SpinLock lock(&_large_lock); // guard against concurrent deallocation.
            usword_t q = Large::quantum_index(address);
            if (!_in_large.bit(q)) {
                q = _in_large.previous_set(q);
                if (q == not_found) return NULL;
            }
            
            // this could crash if another thread deallocates explicitly, but that's a bug we can't prevent
#if UseArena
            Large *large = Large::quantum_large(q, _arena);
#else
            Large *large = Large::quantum_large(q, (void *)0);
#endif
            if (!large->range().in_range(address)) return NULL;
            
            return large->address();
        }
        
        return NULL;
    }
    

    //
    // block_start
    //
    // Return the base block address of an arbitrary address.
    // Broken down because of high frequency of use.
    //
    void *Zone::block_start(void *address) {
        if (in_subzone_memory(address)) {
            Subzone *subzone = Subzone::subzone(address);
            // one of the few subzone entries that guards for bad addresses
            return subzone->block_start(address);
        } else {
            return block_start_large(address);
        }
    }


    //
    // block_size
    //
    // Return the size of a specified block.
    //
    usword_t Zone::block_size(void *block) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (subzone->block_is_start(block)) return subzone->size(block);
        } else if (in_large_memory(block) && Large::is_start(block)) {
            return Large::size(block);
        }

        return 0;
    }


    //
    // block_layout
    //
    // Return the layout of a specified block.
    //
    int Zone::block_layout(void *block) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (!subzone->block_is_start(block)) return AUTO_TYPE_UNKNOWN;        // might have a pointer to a bogus location in a subzone
            return subzone->layout(block);
        } else if (in_large_memory(block) && Large::is_start(block)) {
            return Large::layout(block);
        }

        return AUTO_TYPE_UNKNOWN;
    }
    
    
    //
    // block_set_layout
    //
    // Set the layout of a block.
    //
    void Zone::block_set_layout(void *block, int layout) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (!subzone->block_is_start(block)) return;          // might have a pointer to a bogus location in a subzone
            SpinLock lock(subzone->admin()->lock());
            subzone->set_layout(block, layout);
        } else if (in_large_memory(block) && Large::is_start(block)) {
            Large::set_layout(block, layout);
        }
    }

    
    //
    // get_refcount_small_medium
    //
    // Return the refcount of a small/medium block.
    //
    int Zone::get_refcount_small_medium(Subzone *subzone, void *block) {
        int refcount = subzone->refcount(block);
        if (refcount == 3) {
            // non-zero reference count, check the overflow table.
            SpinLock lock(&_retains_lock);
            PtrIntHashMap::iterator retain_iter = _retains.find(block);
            if (retain_iter != _retains.end() && retain_iter->first == block) {
                refcount = retain_iter->second;
            }
        }
        return refcount;
    }
    
    
    //
    // inc_refcount_small_medium
    //
    // Increments the refcount of a small/medium block, returning the new value.
    // Requires subzone->admin()->lock() to be held, to protect side data.
    //
    int Zone::inc_refcount_small_medium(Subzone *subzone, void *block) {
        usword_t q = subzone->quantum_index(block);
        int refcount = subzone->refcount(q);
        if (refcount == 3) {
            // non-trivial reference count, check the overflow table.
            SpinLock lock(&_retains_lock);
            PtrIntHashMap::iterator retain_iter = _retains.find(block);
            if (retain_iter != _retains.end() && retain_iter->first == block) {
                refcount = ++retain_iter->second;
            } else {
                // transition from 3 -> 4
                refcount = (_retains[block] = 4);
            }
        } else {
            // transition from 0 -> 1, 1 -> 2, 2 -> 3
            if (refcount == 0) {
                Thread &thread = registered_thread();
                thread.block_escaped(this, subzone, block);
            }
            subzone->incr_refcount(q);
            ++refcount;
        }
        return refcount;
    }
    
    
    //
    // dec_refcount_small_medium
    //
    // Decrements the refcount of a small/medium block, returning the new value.
    // Requires subzone->admin()->lock() to be held, to protect side data.
    //
    
    int Zone::dec_refcount_small_medium(Subzone *subzone, void *block) {
        usword_t q = subzone->quantum_index(block);
        int refcount = subzone->refcount(q);
        if (refcount == 3) {
            // non-trivial reference count, check the overflow table.
            SpinLock lock(&_retains_lock);
            PtrIntHashMap::iterator retain_iter = _retains.find(block);
            if (retain_iter != _retains.end() && retain_iter->first == block) {
                if (--retain_iter->second == 3) {
                    // transition from 4 -> 3
                    _retains.erase(retain_iter);
                    return 3;
                } else {
                    return retain_iter->second;
                }
            } else {
                // transition from 3 -> 2
                subzone->decr_refcount(q);
                return 2;
            }
        } else if (refcount > 0) {
            subzone->decr_refcount(q);
            return refcount - 1;
        }
        // underflow.
        malloc_printf("reference count underflow for %p, break on auto_refcount_underflow_error to debug.\n", block);
        auto_refcount_underflow_error(block);
        return -1;
    }
    
    
    //
    // block_refcount
    //
    // Returns the reference count of the specified block, or zero.
    //
    int Zone::block_refcount(void *block) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (!subzone->block_is_start(block)) return 0;        // might have a pointer to a bogus location in a subzone
            return get_refcount_small_medium(Subzone::subzone(block), block);
        } else if (in_large_memory(block) && Large::is_start(block)) {
            SpinLock lock(&_large_lock);
            return Large::refcount(block);
        }

        return 0;
    }

#if 0    
    void Zone::testRefcounting(void *block) {
        for (int j = 0; j < 7; ++j) {
            printf("\nloop start refcount is %d for %p\n", block_refcount(block), block);
            for (int i = 0; i < 5; ++i) {
                block_increment_refcount(block);
                printf("after increment, it now has refcount %d\n", block_refcount(block));
            }
            for (int i = 0; i < 5; ++i) {
                block_decrement_refcount(block);
                printf("after decrement, it now has refcount %d\n", block_refcount(block));
            }
            for (int i = 0; i < 5; ++i) {
                block_increment_refcount(block);
                printf("after increment, it now has refcount %d\n", block_refcount(block));
            }
            for (int i = 0; i < 5; ++i) {
                block_decrement_refcount(block);
                printf("after decrement, it now has refcount %d\n", block_refcount(block));
            }
            printf("maturing block...\n");
            Subzone::subzone(block)->mature(block);
        }
    }
#endif
        


    //
    // block_increment_refcount
    //
    // Increment the reference count of the specified block.
    //
    int Zone::block_increment_refcount(void *block) {
        int refcount = 0;
        
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (!subzone->block_is_start(block)) return 0;        // might have a pointer to a bogus location in a subzone
            SpinLock lock(subzone->admin()->lock());
            refcount = inc_refcount_small_medium(subzone, block);
            // 0->1 transition.  Must read _needs_enlivening while inside allocation lock.
            // Otherwise might miss transition and then collector might have passed over this block while refcount was 0.
            if (refcount == 1) {
                EnliveningHelper<ConditionBarrier> barrier(registered_thread());
                if (barrier && !block_is_marked(block)) barrier.enliven_block(block);
            }
        } else if (in_large_memory(block) && Large::is_start(block)) {
            SpinLock lock(&_large_lock);
            refcount = Large::refcount(block) + 1;
            Large::set_refcount(block, refcount);
            if (refcount == 1) {
                EnliveningHelper<ConditionBarrier> barrier(registered_thread());
                if (barrier && !block_is_marked(block)) barrier.enliven_block(block);
            }
        }
        
        return refcount;
    }
        

    //
    // block_decrement_refcount
    //
    // Decrement the reference count of the specified block.
    //
    int Zone::block_decrement_refcount(void *block) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (!subzone->block_is_start(block)) return 0;        // might have a pointer to a bogus location in a subzone
            SpinLock lock(subzone->admin()->lock());
            return dec_refcount_small_medium(subzone, block);
        } else if (in_large_memory(block) && Large::is_start(block)) {
            SpinLock lock(&_large_lock);
            int refcount = Large::refcount(block);
            if (refcount <= 0) {
                malloc_printf("reference count underflow for %p, break on auto_refcount_underflow_error to debug\n", block);
                auto_refcount_underflow_error(block);
            }
            else {
                refcount = refcount - 1;
                Large::set_refcount(block, refcount);
            }
            return refcount;
        }
        return 0;
    }
    
    
    void Zone::block_refcount_and_layout(void *block, int *refcount, int *layout) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (!subzone->block_is_start(block)) return;        // might have a pointer to a bogus location in a subzone
            SpinLock lock(subzone->admin()->lock());
            *refcount = get_refcount_small_medium(subzone, block);
            *layout = subzone->layout(block);
        } else if (in_large_memory(block) && Large::is_start(block)) {
            SpinLock lock(&_large_lock);
            Large *large = Large::large(block);
            *refcount = large->refcount();
            *layout = large->layout();
        }
    }


    //
    // set_associative_ref
    //
    // Creates an association between a given block, a unique pointer-sized key, and a pointer value.
    //
    void Zone::set_associative_ref(void *block, void *key, void *value) {
        if (value) {
            // the thread local collector doesn't know how to track associative references
            // Doesn't want to, either, since that would require taking the assoc lock on dealloc
            Thread &thread = registered_thread();
            thread.block_escaped(this, NULL, value);
            thread.block_escaped(this, NULL, block);
            
            // Creating associations must enliven objects that may become garbage otherwise.
            EnliveningHelper<UnconditionalBarrier> barrier(thread);
            SpinLock lock(&_associations_lock);
            AssocationsHashMap::iterator i = _associations.find(block);
            ObjectAssocationHashMap* refs = (i != _associations.end() ? i->second : NULL);
            if (refs == NULL) {
                refs = new ObjectAssocationHashMap();
                _associations[block] = refs;
            }
            (*refs)[key] = value;
            if (barrier) barrier.enliven_block(value);
        } else {
            // setting the association to NULL breaks the association.
            SpinLock lock(&_associations_lock);
            AssocationsHashMap::iterator i = _associations.find(block);
            if (i != _associations.end()) {
                ObjectAssocationHashMap *refs = i->second;
                ObjectAssocationHashMap::iterator j = refs->find(key);
                if (j != refs->end()) {
                    refs->erase(j);
                }
            }
        }
    }
    
    //
    // get_associative_ref
    //
    // Returns the associated pointer value for a given block and key.
    //
    void *Zone::get_associative_ref(void *block, void *key) {
        SpinLock lock(&_associations_lock);
        AssocationsHashMap::iterator i = _associations.find(block);
        if (i != _associations.end()) {
            ObjectAssocationHashMap *refs = i->second;
            ObjectAssocationHashMap::iterator j = refs->find(key);
            if (j != refs->end()) return j->second;
        }
        return NULL;
    }

    //
    // erase_associations_internal
    //
    // Assuming association lock held, do the dissassociation dance
    //
    inline void Zone::erase_associations_internal(void *block) {
        AssocationsHashMap::iterator i = _associations.find(block);
        if (i != _associations.end()) {
            ObjectAssocationHashMap *refs = i->second;
            _associations.erase(i);
            delete refs;
        }
    }
    
    //
    // erase_assocations
    //
    // Removes all associations for a given block. Used to
    // clear associations for explicitly deallocated blocks.
    // When the collector frees blocks, it uses a different code
    // path, to minimize locking overhead. See free_garbage().
    //
    void Zone::erase_associations(void *block) {
        SpinLock lock(&_associations_lock);
        erase_associations_internal(block);
    }

    //
    // scan_associations
    //
    // Iteratively visits all associatively referenced objects. Only one pass over the
    // associations table is necessary, as the set_pending() method is sensitive to whether
    // associations are being scanned, and when blocks are newly marked, associations are also
    // recursively pended.
    //
    void Zone::scan_associations(MemoryScanner &scanner) {
        // Prevent other threads from breaking existing associations. We already own the enlivening lock.
        SpinLock lock(&_associations_lock);
    
        // tell set_pending() to recursively pend associative references.
        _scanning_associations = true;
        
        // consider associative references. these are only reachable if their primary block is.
        for (AssocationsHashMap::iterator i = _associations.begin(); i != _associations.end(); i++) {
            void *address = i->first;
            if (associations_should_be_marked(address)) {
                ObjectAssocationHashMap *refs = i->second;
                for (ObjectAssocationHashMap::iterator j = refs->begin(); j != refs->end(); j++) {
                    scanner.associate_block((void**)address, j->first, j->second);
                }
            }
        }
        
        // scan through all pending blocks until there are no new pending
        scanner.scan_pending_until_done();
        
        _scanning_associations = false;
    }
    
    void Zone::pend_associations(void *block, MemoryScanner &scanner) {
        AssocationsHashMap::iterator i = _associations.find(block);
        if (i != _associations.end()) {
            ObjectAssocationHashMap *refs = i->second;
            for (ObjectAssocationHashMap::iterator j = refs->begin(); j != refs->end(); j++) {
                scanner.associate_block((void**)block, j->first, j->second);
            }
        }
    }

    void Zone::erase_associations_in_range(const Range &r) {
        // <rdar://problem/6463922> When a bundle gets unloaded, search the associations table for keys within this range and remove them.
        SpinLock lock(&_associations_lock);
        PtrVector associationsToRemove;
        for (AssocationsHashMap::iterator i = _associations.begin(); i != _associations.end(); i++) {
            if (r.in_range(i->first)) associationsToRemove.push_back(i->first);
        }
        for (PtrVector::iterator i = associationsToRemove.begin(); i != associationsToRemove.end(); i++) {
            erase_associations_internal(*i);
        }
    }

    //
    // set_write_barrier
    //
    // Set the write barrier byte corresponding to the specified address.
    // If scanning is going on then the value is marked pending.
    //
    bool Zone::set_write_barrier(Thread &thread, void *address, void *value) {
        if (in_subzone_memory(address)) {
            // find the subzone
            Subzone *subzone = Subzone::subzone(address);
            
            thread.track_local_assignment(this, address, value);
            
            EnliveningHelper<UnconditionalBarrier> barrier(thread);
            if (barrier && !block_is_marked(value)) barrier.enliven_block(value);
            *(void **)address = value;   // rdar://5512883

            // mark the write barrier
            subzone->write_barrier().mark_card(address);
            return true;
        } else if (thread.is_stack_address(address)) {
            *(void **)address = value;
            return true;
        } else if (void *block = block_start_large(address)) {
            // get the large block
            Large *large = Large::large(block);
            
            EnliveningHelper<UnconditionalBarrier> barrier(thread);
            if (barrier && !block_is_marked(value)) barrier.enliven_block(value);
            *(void **)address = value;   // rdar://5512883

            thread.block_escaped(this, NULL, value);

            // mark the write barrier
            if (large->is_scanned()) large->write_barrier().mark_card(address);
            return true;
        }
        thread.block_escaped(this, NULL, value);
        return false;
    }

    //
    // set_write_barrier_range
    //
    // Set a range of write barrier bytes to the specified mark value.
    //
    bool Zone::set_write_barrier_range(void *destination, const usword_t size) {
        // First, mark the card(s) associated with the destination.
        if (in_subzone_memory(destination)) {
            // find the subzone
            Subzone *subzone = Subzone::subzone(destination);
            
            // mark the write barrier
            subzone->write_barrier().mark_cards(destination, size);
            return true;
        } else if (void *block = block_start_large(destination)) {
            Large *large = Large::large(block);
            
            // mark the write barrier
            if (large->is_scanned()) large->write_barrier().mark_cards(destination, size);
            return true;
        }
        return false;
    }


    //
    // set_write_barrier
    //
    // Set the write barrier byte corresponding to the specified address.
    //
    bool Zone::set_write_barrier(void *address) {
        if (in_subzone_memory(address)) {
            // find the subzone
            Subzone *subzone = Subzone::subzone(address);
            
            // mark the write barrier
            subzone->write_barrier().mark_card(address);
            return true;
        }
        else if (void *block = block_start_large(address)) {
            // get the large block
            Large *large = Large::large(block);
            
            // mark the write barrier
            if (large->is_scanned()) large->write_barrier().mark_card(address);
            return true;
        }
        return false;
    }
    
    struct mark_write_barriers_untouched_visitor {
        usword_t _count;
        
        mark_write_barriers_untouched_visitor() : _count(0) {}
        
        // visitor function 
        inline bool visit(Zone *zone, WriteBarrier &wb) {
            // clear the write barrier 
            _count += wb.mark_cards_untouched();
            // always continue
            return true;
        }
    };
    void Zone::mark_write_barriers_untouched() {
        mark_write_barriers_untouched_visitor visitor;
        visitWriteBarriers(this, visitor);
    }

    //
    // clear_untouched_write_barriers
    //
    // iterate through all the write barriers and clear marks.
    //
    struct clear_untouched_write_barriers_visitor {
        usword_t _count;
        
        clear_untouched_write_barriers_visitor() : _count(0) {}
        
        // visitor function 
        inline bool visit(Zone *zone, WriteBarrier &wb) {
            // clear the untouched cards. 
            _count += wb.clear_untouched_cards();
            
            // always continue
            return true;
        }
    };
    void Zone::clear_untouched_write_barriers() {
        // this must be done while the _enlivening_lock is held, to keep stragglers from missing writes.
        clear_untouched_write_barriers_visitor visitor;
        visitWriteBarriers(this, visitor);
    }
    
    
    //
    // clear_all_write_barriers
    //
    // iterate through all the write barriers and clear marks.
    //
    struct clear_all_write_barriers_visitor {
        // visitor function 
        inline bool visit(Zone *zone, WriteBarrier &wb) {
            // clear the write barrier 
            wb.clear();
            
            // always continue
            return true;
        }
    };
    void Zone::clear_all_write_barriers() {
        // this is done while the _enlivening_lock is held, to keep stragglers from missing writes.
        // set up the visitor
        clear_all_write_barriers_visitor visitor;
        visitWriteBarriers(this, visitor);
    }

    //
    // reset_all_marks
    //
    // Clears the mark flags on all blocks
    //
    struct reset_all_marks_visitor {
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q, void *block) {
            // clear block's new flag
            subzone->clear_mark(q);
            
            // always continue
            return true;
        }
        
        inline bool visit(Zone *zone, Large *large, void *block) {
            // clear block's new flag
            large->clear_mark();
            
            // always continue
            return true;
        }
    };
    void Zone::reset_all_marks() {
#if 1
        // XXX_PCB:  marks are now in their own separate BitMaps, so just clear their live ranges.
        for (Region *region = _region_list; region != NULL; region = region->next()) {
            region->clear_all_marks();
        }
        
        // this is called from collect_end() so should be safe.
        SpinLock lock(&_large_lock);
        for (Large *large = _large_list; large != NULL; large = large->next()) {
            large->clear_mark();
        }
#else
        // set up all marks visitor
        reset_all_marks_visitor visitor;
        
        // set up iterator
        BlockIterator<reset_all_marks_visitor> iterator(this, visitor);
        
        // visit all the admins
        iterator.visit();
#endif
    }
       
    
    //
    // reset_all_marks_and_pending
    //
    // Clears the mark and ending flags on all blocks
    //
    struct reset_all_marks_and_pending_visitor {
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            // clear block's mark & pending flags
            subzone->clear_mark(q);
            subzone->clear_pending(q);
            
            // always continue
            return true;
        }
        
        inline bool visit(Zone *zone, Large *large) {
            // clear block's mark & pending flags
            large->clear_mark();
            large->clear_pending();
            
            // always continue
            return true;
        }
    };
    void Zone::reset_all_marks_and_pending() {
        // NOTE: the enlivening lock must be held here, which should keep the region and large lists stable.
        // NOTE: marks are now in their own separate BitMaps, so just clear their live ranges.
        for (Region *region = _region_list; region != NULL; region = region->next()) {
            region->clear_all_marks();
            region->clear_all_pending();
        }
        
        SpinLock lock(&_large_lock);
        for (Large *large = _large_list; large != NULL; large = large->next()) {
            large->clear_mark();
            large->clear_pending();
        }
    }
    
    
    
    
    //
    // statistics
    //
    // Returns the statistics for this zone.
    //
    struct statistics_visitor {
        Statistics &_stats;
        Region *_last_region;
        Subzone *_last_subzone;
        
        statistics_visitor(Statistics &stats)
        : _stats(stats)
        , _last_region(NULL)
        , _last_subzone(NULL)
        {}
        
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            // assumes that subzones are iterated linearly across all regions
            if (_last_region != subzone->region()) {
                _last_region = subzone->region();
                _stats.add_admin(Region::bytes_needed());
            }
            
            if (_last_subzone != subzone) {
                _last_subzone = subzone;
                _stats.add_admin(subzone_write_barrier_max);
                _stats.add_allocated(subzone->allocation_size());
                _stats.add_dirty(subzone->allocation_size());
            }
            
            _stats.add_count(1);
            _stats.add_size(subzone->size(q));
            
            return true;
        }
        
        inline bool visit(Zone *zone, Large *large) {
            _stats.add_admin(large->vm_size() - large->size());
            _stats.add_count(1);
            _stats.add_size(large->size());
            
            return true;
        }
    };
    void Zone::statistics(Statistics &stats) {
        
        statistics_visitor visitor(stats);
        visitAllocatedBlocks(this, visitor);
    }

    //
    // malloc_statistics
    // rummage around and tot up the requisite numbers
    //
    void Zone::malloc_statistics(malloc_statistics_t *stats) {
        stats->blocks_in_use = 0;
        stats->size_in_use = 0;
        stats->max_size_in_use = 0;
        stats->size_allocated = 0;
        {
            SpinLock lock(&_large_lock);
            Large *l = _large_list;
            while (l) {
                stats->blocks_in_use++;
                stats->size_in_use += l->vm_size();
                stats->max_size_in_use += l->vm_size();
                stats->size_allocated += l->vm_size();
                l = l->next();
            }
        }
        {
            SpinLock lock(&_region_lock);
            Region *r = _region_list;
            while (r) {
                r->malloc_statistics(stats);
                r = r->next();
            }
        }
    }

    //
    // set_needs_enlivening
    //
    // Informs the write-barrier that blocks need repending.
    //
    void Zone::set_needs_enlivening() {
        close_locks();

        Mutex lock(&_registered_threads_mutex);
        _enlivening_enabled = true;
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            LockedBoolean &needs_enlivening = thread->needs_enlivening();
            assert(needs_enlivening.state == false);
            SpinLock lock(&needs_enlivening.lock);
            needs_enlivening.state = true;
        }

        open_locks();
    }

    class scan_barrier_visitor {
        MemoryScanner &_scanner;
    public:
        scan_barrier_visitor(MemoryScanner &scanner) : _scanner(scanner) {}
        
        void visitPointerChunk(void **pointers, void **limit) {
            while (pointers < limit) {
                _scanner.repend(*pointers++);
            }
        }
    };
    
    //
    // enlivening_barrier
    //
    // Used by collectors to synchronize with concurrent mutators.
    //
    void Zone::enlivening_barrier(MemoryScanner &scanner) {
        // write barriers should no longer repend blocks
        // NOTE:  this assumes NO THREADS ARE SUSPENDED at this point.
        // NOTE: we exit scanning with all thread-local enlivening locks and thread list mutex held, and the thread list mutex, so that
        // no new threads can enter the system, until this round of scanning completes.
        scan_barrier_visitor visitor(scanner);

        // Thread Local Enlivening Queues.
        // TODO:  we could optimize this to allow threads to enter during one pass, and then do another pass fully locked down.
        // TODO:  move this into Zone::scan_barrier(), to preserve the lock ownership.
        pthread_mutex_lock(&_registered_threads_mutex);
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            EnliveningQueue &enlivening_queue = thread->enlivening_queue();
            LockedBoolean &needs_enlivening = enlivening_queue.needs_enlivening();
            spin_lock(&needs_enlivening.lock);
            visitPointerChunks(enlivening_queue.chunks(), enlivening_queue.count(), visitor);
            enlivening_queue.reset();
        }
        _enlivening_complete = true;
    }


    //
    // clear_needs_enlivening
    //
    // Write barriers no longer need to repend blocks.
    // Assumes all thread enlivening locks are held, and that _registered_threads_mutex is held.
    // All of those locks are released.
    //
    void Zone::clear_needs_enlivening() {
        _enlivening_enabled = false;
        _enlivening_complete = false;
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            LockedBoolean &needs_enlivening = thread->needs_enlivening();
            assert(needs_enlivening.state && (!__is_threaded || needs_enlivening.lock != 0));
            needs_enlivening.state = false;
            spin_unlock(&needs_enlivening.lock);
        }
        // this balances the final pthread_mutex_lock() in enlivening_barrier().
        pthread_mutex_unlock(&_registered_threads_mutex);
    }

    //
    // block_collector
    //
    // Called by the monitor to prevent collections.
    //
    bool Zone::block_collector() {
        // Since gdb calls the root/reference tracer with all threads suspended, must
        // be very careful not to deadlock.
        if (pthread_mutex_trylock(&_mark_bits_mutex) != 0)
            return false;
        if (pthread_mutex_trylock(&_registered_threads_mutex) != 0) {
            pthread_mutex_unlock(&_mark_bits_mutex);
            return false;
        }
        return true;
    }
    
    
    //
    // unblock_collector
    //
    // Called by the monitor to enable collections.
    //
    void Zone::unblock_collector() {
        pthread_mutex_unlock(&_registered_threads_mutex);
        pthread_mutex_unlock(&_mark_bits_mutex);
    }
    
    
    //
    // collect
    //
    // Performs the collection process.
    //
    void Zone::collect(bool is_partial, void *current_stack_bottom, auto_date_t *enliveningBegin) {
        
		GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)this, AUTO_TRACE_SCANNING_PHASE);

        // inform mutators that they need to add objects to the enlivening queue while scanning.
        // we lock around the rising edge to coordinate with eager block deallocation.
        // Grab all other locks that use ConditionBarrier on the enlivening_lock, then grab the enlivening_lock
        // and mark it.  This ordering guarantees that the the code using the ConditionBarrier can read the condition
        // without locking since they each have already acquired a lock necessary to change the needs_enlivening state.
        // All locks are released after setting needs_enlivening.
        set_needs_enlivening();

        // construct collector
        Collector collector(this, current_stack_bottom, is_partial);
        
        // <rdar://problem/5495573> reserve all of the mark bits for exclusive use by the collector.
        pthread_mutex_lock(&_mark_bits_mutex);
        
        // run collector in scan stack mode
        collector.collect(false);

        // check if stack overflow occurred
        if (_scan_stack.is_overflow()) {
            _stats.increment_stack_overflow_count();
            
            reset_all_marks_and_pending();

            // let go of the thread list mutex and per-thread _enlivening_locks, which were acquired by Collector::scan_barrier().
            clear_needs_enlivening();
            set_needs_enlivening();

            // try again using pending bits
            collector.collect(true);
        }
        
        _scan_stack.reset();

		GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)this, AUTO_TRACE_SCANNING_PHASE, (uint64_t)collector.blocks_scanned(), (uint64_t)collector.bytes_scanned());

        auto_weak_callback_block_t *callbacks = NULL;

        // update stats - XXX_PCB only count completed collections.
        *enliveningBegin = collector.scan_end;
        _stats.increment_gc_count(is_partial);
        
        // XXX_PCB VMAddressList uses aux_malloc(), which could deadlock if some other thread holds the stack logging spinlock.
        // XXX_PCB this issue is being tracked in <rdar://problem/4501032>.
        _garbage_list.clear_count();
        scavenge_blocks();

        // if weak references are present, threads will still be suspended, resume them after clearing weak references.
        if (has_weak_references()) {
			GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)this, AUTO_TRACE_WEAK_REFERENCE_PHASE);
            uintptr_t weak_referents, weak_references;
            callbacks = weak_clear_references(this, _garbage_list.count(), (vm_address_t *)_garbage_list.buffer(), &weak_referents, &weak_references);
			GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)this, AUTO_TRACE_WEAK_REFERENCE_PHASE, (uint64_t)weak_referents, (uint64_t)(weak_references * sizeof(void*)));
        }

        // Write-Barrier Repair.
        if (!is_partial) {
            if (!_repair_write_barrier) {
                // after running a full collection, tell the next generational collection to repair the write barrier cards.
                _repair_write_barrier = true;
                mark_write_barriers_untouched();
            }
        } else if (_repair_write_barrier) {
            // first generational after a full, clear all cards that are known to not have intergenerational pointers.
            clear_untouched_write_barriers();
            _repair_write_barrier = false;
        }

        // notify mutators that they no longer need to enliven objects.
        // No locks are acquired since enlivening lock is already held and all code that uses ConditionBarrier
        // is blocking on the enlivening_lock already.
        clear_needs_enlivening();

        // garbage list has been computed, can now clear the marks.
        reset_all_marks();
        
        // mark bits can now be reused elsewhere.
        pthread_mutex_unlock(&_mark_bits_mutex);

        // recycle unused Thread objects.
        recycle_threads();
        
        // release any unused pages
        // release_pages();
        weak_call_callbacks(callbacks);
        
        // malloc_printf("Zone::collect(): partial_gc_count = %ld, full_gc_count = %ld, aborted_gc_count = %ld\n",
        //               _stats.partial_gc_count(), _stats.full_gc_count(), _stats.aborted_gc_count());
        
        // malloc_printf("collector.stack_scanned() = %u\n", collector.stack_scanned());

        if (Environment::print_stats) {
            malloc_printf("cnt=%d, sz=%d, max=%d, al=%d, admin=%d\n",
                _stats.count(),
                _stats.size(),
                _stats.dirty_size(),
                _stats.allocated(),
                _stats.admin_size());
        }
    }

        
    //
    // scavenge_blocks
    //
    // Constructs a list of all garbage blocks.
    //
    // Also ages non-garbage blocks, so we can do this while
    // the enlivening lock is held. This prevents a possible race
    // with mutators that adjust reference counts. <rdar://4801771>
    //
    struct scavenge_blocks_visitor {
        PointerList& _list;                               // accumulating list
        
        // Constructor
        scavenge_blocks_visitor(PointerList& list) : _list(list) {}
        
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            if (subzone->is_thread_local(q)) return true;
            
            // always age blocks, to distinguish garbage blocks from blocks allocated during finalization [4843956].
            if (subzone->is_new(q)) subzone->mature(q);
            
            // add unmarked blocks to the garbage list.
            if (!subzone->is_marked(q)) {
                subzone->mark_global_garbage(q);
                _list.add(subzone->quantum_address(q));
            }
            
            // always continue
            return true;
        }
        
        inline bool visit(Zone *zone, Large *large) {
            // always age blocks, to distinguish garbage blocks from blocks allocated during finalization [4843956].
            if (large->is_new()) large->mature();
            
            // add unmarked blocks to the garbage list.
            if (!large->is_marked()) {
                large->mark_garbage();
                _list.add(large->address());
            }

            // always continue
            return true;
        }
    };
    
    
    void Zone::scavenge_blocks() {
        // set up the block scanvenger visitor
        scavenge_blocks_visitor visitor(_garbage_list);
        
        // iterate through all the blocks
        visitAllocatedBlocks(this, visitor);
    }
    
    
    void Zone::recycle_threads() {
        Thread *unbound_threads = NULL;
        {
            Mutex lock(&_registered_threads_mutex);
            Thread::scavenge_threads(&_registered_threads, &unbound_threads);
        }
        while (unbound_threads != NULL) {
            Thread *next = unbound_threads->next();
            delete unbound_threads;
            unbound_threads = next;
        }
    }
    
    static void foreach_block_do(auto_zone_cursor_t cursor, void (*op) (void *ptr, void *data), void *data) {
        Zone *azone = (Auto::Zone *)cursor->zone;
        while (cursor->index < cursor->garbage_count) {
            void *ptr = (void *)cursor->garbage[cursor->index++];
            auto_memory_type_t type = auto_zone_get_layout_type((auto_zone_t *)azone, ptr);
            if (type & AUTO_OBJECT) {
#if DEBUG
                if (ptr == WatchPoint) {
                    malloc_printf("auto_zone invalidating watchpoint: %p\n", WatchPoint);
                }
#endif
                op(ptr, data);
                cursor->block_count++;
                cursor->byte_count += azone->block_size(ptr);
            }
        }
    }


    
    //
    // invalidate_garbage
    //
    void Zone::invalidate_garbage(const size_t garbage_count, const vm_address_t *garbage) {
#if DEBUG
        // when debugging, sanity check the garbage list in various ways.
        for (size_t index = 0; index < garbage_count; index++) {
            void *ptr = (void *)garbage[index];
            int rc = block_refcount(ptr);
            if (rc > 0)
                malloc_printf("invalidate_garbage: garbage ptr = %p, has non-zero refcount = %d\n", ptr, rc);
        }
#endif
        struct auto_zone_cursor cursor = { (auto_zone_t *)this, garbage_count, garbage, 0, 0, 0 };
        if (control.batch_invalidate) {
            control.batch_invalidate((auto_zone_t *)this, foreach_block_do, &cursor, sizeof(cursor));
        }
    }

    static inline void zombify(Auto::Zone *azone, void *ptr) {
        if ((azone->block_layout(ptr) & AUTO_OBJECT)) azone->erase_weak(ptr);
        // callback to morph the object into a zombie.
        if (azone->control.resurrect) azone->control.resurrect((auto_zone_t*)azone, ptr);
        azone->block_set_layout(ptr, AUTO_OBJECT_UNSCANNED);
    }

    void Zone::handle_overretained_garbage(void *block, int rc) {
        char *name;
        auto_memory_type_t layout = block_layout(block);
        if ((layout & AUTO_OBJECT) == AUTO_OBJECT) {
            if (control.name_for_address) {
                name = control.name_for_address((auto_zone_t *)this, (vm_address_t)block, 0);
            } else {
                name = (char *)"object";
            }
        } else {
            name = (char *)"non-object";
        }
        malloc_printf("garbage block %p(%s) was over-retained during finalization, refcount = %d\n"
                      "This could be an unbalanced CFRetain(), or CFRetain() balanced with -release.\n"
                      "Break on auto_zone_resurrection_error() to debug.\n", block, name, rc);
        auto_zone_resurrection_error();
        if (Auto::Environment::resurrection_is_fatal) {
            auto_fatal("fatal resurrection error for garbage block %p(%s): over-retained during finalization, refcount = %d", block, name, rc);
        }            
        if (((layout & AUTO_OBJECT) == AUTO_OBJECT) && control.name_for_address) free(name);
    }
    
    size_t Zone::free_garbage(boolean_t generational, const size_t garbage_count, vm_address_t *garbage, size_t &blocks_freed, size_t &bytes_freed) {
        size_t index;

        SpinLock lock(associations_lock());

        blocks_freed = bytes_freed = 0;
        
        for (index = 0; index < garbage_count; index++) {
            void *ptr = (void *)garbage[index];
            int rc = block_refcount(ptr);
            if (rc == 0) {
                if ((block_layout(ptr) & AUTO_OBJECT)) erase_weak(ptr);
                blocks_freed++;
                bytes_freed += block_size(ptr);
                if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(zone), uintptr_t(ptr), 0, 0, 0);
                erase_associations_internal(ptr);
                if (in_subzone_memory(ptr)) {
                    Subzone::subzone(ptr)->admin()->deallocate(ptr);
                } else if (in_large_memory(ptr)) {
                    deallocate_large(ptr);
                } else {
                    error("Deallocating a non-block", ptr);
                }
            } else if (is_zombie(ptr)) {
                zombify(this, ptr);
                block_decrement_refcount(ptr);
            } else {
                handle_overretained_garbage(ptr, rc);
            }
        }
        
        return bytes_freed;
    }

    //
    // register_thread
    //
    // Add the current thread as a thread to be scanned during gc.
    //
    Thread &Zone::register_thread() {
        // if thread is not registered yet
        Thread *thread = current_thread();
        if (thread == NULL) {
            // be a good citizen, and try to free some recycled threads before allocating more.
            // recycle_threads();
            
            // construct a new Thread 
            thread = new Thread(this);
            
            // add thread to linked list of registered threads
            Mutex lock(&_registered_threads_mutex);
            thread->set_next(_registered_threads);
            thread->needs_enlivening().state = _enlivening_enabled;
            _registered_threads = thread;
        }
        pthread_setspecific(_registered_threads_key, thread);
        return *thread;
    }
    

    //
    // unregister_thread
    //
    // Deprecated.
    //
    void Zone::unregister_thread() {
        return;
    }

    //
    // destroy_registered_thread
    //
    // Pthread key destructor. The collector has a critical dependency on the ordering of pthread destructors.
    // This destructor must run after any other code which might possibly call into the collector.
    // We have arranged with pthreads to have our well known key destructor called after any dynamic keys and
    // any static (Apple internal) keys that might call into the collector (Foundation, CF). On the last iteration
    // (PTHREAD_DESTRUCTOR_ITERATIONS) we unregister the thread. 
    // Note that this implementation is non-portable due to our agreement with pthreads.
    //
    void Zone::destroy_registered_thread(void *key_value) {
        if (key_value != INVALID_THREAD_KEY_VALUE) {
            Thread *thread = (Thread *)key_value;
            Zone *zone = thread->zone();
            pthread_key_t thread_key = zone->_registered_threads_key;
            if (thread->increment_tsd_count() == PTHREAD_DESTRUCTOR_ITERATIONS) {
                // <rdar://problem/6514886>:  Deleting the thread object while the collector is
                // enumerating the list is probably not worth the extra effort here. The collector
                // will scavenge the list, and eventually recycle thread objects, during the next collection.
                thread->unbind();
                // Set the value to a token that marks the thread as unregistered.
                // If this thread calls into the collector again (tries to look up a Thread instance) then we define it as a fatal error.
                key_value = INVALID_THREAD_KEY_VALUE;
            }
            pthread_setspecific(thread_key, key_value);
        }
    }

    // These accessors provide a way to enumerate the registered threads safely but without holding
    // the registered threads mutex. Threads are prevented from exiting by holding each thread's
    // spin lock while its stack is scanned. The lock is always acquired and released while
    // the registered threads mutex is locked, to ensure the integrity of the list. Since newly
    // registered threads are always added to the beginning of the list, the enumeration won't include
    // newly registered threads. The enlivening phase of the collector takes care of that.
    
    inline Thread *Zone::firstScannableThread() {
        Mutex lock(&_registered_threads_mutex);
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            if (thread->lockForScanning()) return thread;
        }
        return NULL;
    }
    
    inline Thread *Zone::nextScannableThread(Thread *thread) {
        Mutex lock(&_registered_threads_mutex);
        thread->unlockForScanning();
        for (thread = thread->next(); thread != NULL; thread = thread->next()) {
            if (thread->lockForScanning()) return thread;
        }
        return NULL;
    }
    
    //
    // scan_registered_threads
    //
    // Safely enumerates registered threads during stack scanning.
    //
    void Zone::scan_registered_threads(thread_scanner_t scanner) {
        for (Thread *thread = firstScannableThread(); thread != NULL; thread = nextScannableThread(thread)) {
            // scanner is guaranteed to only see threads locked for scanning.
            scanner(thread);
        }
    }
    
    //
    // suspend_all_registered_threads
    //
    // Suspend all registered threads. Provided for heap snapshots.
    // Acquires the registered threads mutex so that no new threads can enter the system.
    //
    void Zone::suspend_all_registered_threads() {
        pthread_mutex_lock(&_registered_threads_mutex);
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            thread->suspend();
        }
    }


    //
    // resume_all_registered_threads
    //
    // Resumes all suspended registered threads. Provided for heap snapshots.
    // Relinquishes the registered threads mutex.
    //
    void Zone::resume_all_registered_threads() {
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            thread->resume();
        }
        pthread_mutex_unlock(&_registered_threads_mutex);
    }

    
    //
    // print_all_blocks
    //
    // Prints all allocated blocks.
    //
    struct print_all_blocks_visitor {
        Region *_last_region;                               // previous region visited
        Subzone *_last_subzone;                             // previous admin visited
        bool _is_large;

        // Constructor
        print_all_blocks_visitor() : _last_region(NULL), _is_large(false) {}
        
        // visitor function
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            // if transitioning then print appropriate banner
            if (_last_region != subzone->region()) {
                _last_region = subzone->region();
                malloc_printf("Region [%p..%p]\n", _last_region->address(), _last_region->end());
            }
            
            void *block = subzone->quantum_address(q);
            if (subzone->is_start(q)) {
                zone->print_block(block);
            } else {
                FreeListNode *node = (FreeListNode *)block;
                malloc_printf("   %p(%6d) ### free\n", block, node->size());
            }

            // always continue
            return true;
        }
        
        inline bool visit(Zone *zone, Large *large) {
            if (!_is_large) {
                malloc_printf("Large Blocks\n");
                _is_large = true;
            }
            
            zone->print_block(large->address());
             
            // always continue
            return true;
        }
    };
    void Zone::print_all_blocks() {
        SpinLock lock(&_region_lock);
        print_all_blocks_visitor visitor;
        visitAllBlocks(this, visitor);
    }
    
    
    //
    // print block
    //
    // Print the details of a block
    //
    void Zone::print_block(void *block) {
        print_block(block, "");
    }
    void Zone::print_block(void *block, const char *tag) {
        block = block_start(block);
        if (!block) malloc_printf("%s%p is not a block", tag, block);
        
        if (block) {
            if (in_subzone_memory(block)) {
                Subzone *subzone = Subzone::subzone(block);
                usword_t q = subzone->quantum_index(block);
                
                int rc = block_refcount(block);
                int layout = subzone->layout(q);
                bool is_unscanned = (layout & AUTO_UNSCANNED) != 0;
                bool is_object = (layout & AUTO_OBJECT) != 0;
                bool is_new = subzone->is_new(q);
                bool is_marked = subzone->is_marked(q);
                bool is_pending = false;
                char *class_name = (char *)"";
                if (is_object) {
                    void *isa = *(void **)block;
                    if (isa) class_name = *(char **)displace(isa, 8);
                }
                
                malloc_printf("%s%p(%6d) %s %s %s %s %s rc(%d) q(%u) subzone(%p) %s\n",
                                   tag, block, (unsigned)subzone->size(q),
                                   is_unscanned ? "   "  : "scn",
                                   is_object    ? "obj"  : "mem",
                                   is_new       ? "new"  : "   ",
                                   is_marked    ? "mark" : "    ",
                                   is_pending   ? "pend" : "    ",
                                   rc,
                                   q, subzone,
                                   class_name);
            }  else if (in_large_memory(block) && Large::is_start(block)) {
                Large *large = Large::large(block);
                
                int rc = block_refcount(block);
                int layout = large->layout();
                bool is_unscanned = (layout & AUTO_UNSCANNED) != 0;
                bool is_object = (layout & AUTO_OBJECT) != 0;
                bool is_new = large->is_new();
                bool is_marked = large->is_marked();
                bool is_pending = false;
                char *class_name = (char *)"";
                if (is_object) {
                    void *isa = *(void **)block;
                    if (isa) class_name = *(char **)displace(isa, 8); // XXX 64 bit WRONG
                }
                
                malloc_printf("%s%p(%6d) %s %s %s %s %s rc(%d) %s\n",
                                   tag, block, (unsigned)large->size(),
                                   is_unscanned ? "   "  : "scn",
                                   is_object    ? "obj"  : "mem",
                                   is_new       ? "new"  : "   ",
                                   is_marked    ? "mark" : "    ",
                                   is_pending   ? "pend" : "    ",
                                   rc,
                                   class_name);
            }
            
            return;
        }
      
        malloc_printf("%s%p is not a block", tag, block);
    }



};


