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
    Zone.cpp
    Garbage Collected Heap
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#include "Admin.h"
#include "Bitmap.h"
#include "BlockIterator.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Environment.h"
#include "Large.h"
#include "Locks.h"
#include "Range.h"
#include "Region.h"
#include "Statistics.h"
#include "Subzone.h"
#include "Thread.h"
#include "WriteBarrierIterator.h"
#include "ThreadLocalCollector.h"
#include "Zone.h"

#include "auto_weak.h"
#include "auto_trace.h"
#include "auto_dtrace.h"

#include <mach-o/dyld.h>
#include <mach-o/ldsyms.h>
#include <mach-o/dyld_priv.h>
#include <sys/mman.h>
#include <Block.h>

struct auto_zone_cursor {
    auto_zone_t *zone;
    size_t garbage_count;
    void **garbage;
    volatile unsigned index;
    size_t block_count;
    size_t byte_count;
};


namespace Auto {

#if defined(DEBUG)
#warning DEBUG is set
#endif

    class ResourceTracker : public AuxAllocated {
        boolean_t (^_should_collect)(void);
    public:
        ResourceTracker *_next;
        ResourceTracker *_prev;
        char _description[0];

        ResourceTracker(const char *description, boolean_t (^test)(void), ResourceTracker *next) : _should_collect(Block_copy(test)), _next(next), _prev(NULL) {
            strcpy(_description, description);
            if (_next)
                _next->_prev = this;
        };
        
        static ResourceTracker *create_resource_tracker(const char *description, boolean_t (^test)(void), ResourceTracker *next) {
            ResourceTracker *tracker = new(strlen(description)+1) ResourceTracker(description, test, next);
            return tracker;
        }
        
        ~ResourceTracker() { Block_release(_should_collect); }
        
        const char *description() { return _description; }
        
        boolean_t probe() { return _should_collect(); }
        
        void unlink() {
            if (_next)
                _next->_prev = _prev;
            if (_prev)
                _prev->_next = _next;
        }
    };
    
    
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
    :   _registered_threads_key(thread_registration_key)
    {
        ASSERTION(page_size == vm_page_size);
    
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
        pthread_mutex_init(&_roots_lock, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);
        pthread_rwlock_init(&_associations_lock, NULL);
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
        
        _partition.initialize(this);
        
        // initialize the large list
        _large_list = NULL;
        _large_lock = 0;
        
        // initialize roots hash set.
        _datasegments_lock = 0;
        _zombies_lock = 0;
        
        // initialize regions list
        _region_list = NULL;
        _region_lock = 0;
        _coverage_lock = 0;
        
        // initialize flags
        _repair_write_barrier = false;

        _state = idle;
        
        _allocation_counter = 0;
        
        _collection_checking_enabled = 0;
        
        // prime the first region
        allocate_region();
        
        if (_first_zone == NULL)
            _first_zone = this;
        
        pthread_mutex_init(&_worker_lock, NULL);
        pthread_cond_init(&_worker_cond, NULL);
        _has_work = false;
        _worker_func = NULL;
        _worker_arg = NULL;
        _worker_count = 0;
        _average_collection_time = 100000; // 100 ms. Seed with a large value to discourage collection at startup.
        _sleeping_workers = 0;
        _stats.idle_timer().start();
        
        pthread_mutex_init(&_compaction_lock, NULL);
        
        // need to wait for NMOS.
        _compaction_disabled = true;

#if TARGET_IPHONE_SIMULATOR
#       warning no TLV support on iOS simulator
#else
        // listen for changes to thread-local storage

        dyld_register_tlv_state_change_handler(dyld_tlv_state_allocated, 
            ^(enum dyld_tlv_states state, const dyld_tlv_info *info) 
        {
            if (this->current_thread()) {
                this->add_datasegment(info->tlv_addr, info->tlv_size);
            }
        });

        dyld_register_tlv_state_change_handler(dyld_tlv_state_deallocated, 
            ^(enum dyld_tlv_states state, const dyld_tlv_info *info) 
        {
            if (this->current_thread()) {
                this->remove_datasegment(info->tlv_addr, info->tlv_size);
            }
        });
#endif
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
            auto_error(this, "~Zone(): registered threads list not empty", NULL);
    }


    //
    // memory allocation from within arena
    //
#if UseArena  
    // low half of arena in one region, top half used for large allocations
    void *Zone::arena_allocate_large(usword_t size) {
        size = align2(size, allocate_quantum_large_log2);
        usword_t nbits = size >> allocate_quantum_large_log2;
        // look through our arena for free space on this alignment
        usword_t start = 0;
        // someday... track _first_free
        // compute quanta end point as (arena size) - (space reserved for subzones) converted to quanta
        usword_t end = ((1ul << arena_size_log2) - ((uintptr_t)_large_start - (uintptr_t)_arena)) >> allocate_quantum_large_log2;
        if (nbits > (end - start)) {
            return NULL;
        }
        end -= nbits; // can't find anything that big past this point :-)
        SpinLock lock(&_large_bits_lock);
        while (start <= end) {
            // actually, find last clear bit. If 0, we have an allocation, otherwise we have a new start XXX
            if (_large_bits.bits_are_clear(start, nbits)) {
                _large_bits.set_bits(start, nbits);
                void *address = displace(_large_start, start << allocate_quantum_large_log2);
                commit_memory(address, size);
                return address;
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
        size = align2(size, allocate_quantum_large_log2);
        usword_t nbits = size >> allocate_quantum_large_log2;
        usword_t start = ((char *)address - (char *)_large_start) >> allocate_quantum_large_log2;
        SpinLock lock(&_large_bits_lock);
        _large_bits.clear_bits(start, nbits);
        uncommit_memory(address, size);
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
        
        Region *r = _region_list;
        while (r) {
            if (r->subzones_remaining() != 0)
                return r; // another thread allocated a region
            r = r->next();
        }

        // allocate new region
        Region *region = Region::new_region(this);
        
        // if allocated
        if (region) {
            
            {
                SpinLock lock(&_coverage_lock);

                // update coverage range
                _coverage.expand_range(*region);
            }
            
            // keep region list sorted to aid in sorting free subzone blocks
            if (_region_list == NULL || region->address() < _region_list->address()) {
                // add to front of list
                region->set_next(_region_list);
                _region_list = region;
            } else {
                // insert the new region in the appropriate spot
                Region *r = _region_list;
                while (r->next() != NULL && r->next()->address() < region->address()) {
                    r = r->next();
                }
                region->set_next(r->next());
                r->set_next(region);
            }
        }
        return region;
    }
    
        
    
    //
    // allocate_large
    //
    // Allocates a large block from the universal pool (directly from vm_memory.)
    //
    void *Zone::allocate_large(Thread &thread, usword_t &size, const usword_t layout, bool clear, bool refcount_is_one) {
        Large *large = Large::allocate(this, size, layout, refcount_is_one);
        void *address;
        
        {
            SpinLock lock(&_large_lock);

            // Enlivening barrier needs to wrap allocation, setting _in_large bitmap, and adding to large list.
            // Updating _in_large must be done under the enlivening lock because the collector needs _in_large
            // to be updated in order to repend the block during enlivening.
            ConditionBarrier barrier(thread.needs_enlivening());
            if (large) {
                address = large->address();

                // mark in large bit map
                _in_large.set_bit(Large::quantum_index(address));
                
                if (barrier) LargeBlockRef(large).enliven();
                
                // add to large list
                large->add(_large_list);
            } else {
                return NULL;
            }
        }
        
        // get info
        size = large->size();       // reset size to be that of actual allocation
        add_blocks_and_bytes(1, size);
        
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
        
        adjust_allocation_counter(size);
        
        return address;
    }
    
    
    //
    // deallocate_large
    //
    // Release memory allocated for a large block.
    //
    void Zone::deallocate_large(Large *large, void *block) {
        SpinLock lock(&_large_lock);
        deallocate_large_internal(large, block);
    }
    
    void Zone::deallocate_large_internal(Large *large, void *block) {
        // remove from large list
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
    
    static inline bool locked(pthread_rwlock_t *lock) {
        TryWriteLock attempt(lock);
        return !attempt;
    }
      
    bool Zone::is_locked() {
        // TRY every lock. If any of them is held, we're considered locked.
        bool result = (_partition.locked() || locked(&weak_refs_table_lock) || locked(&_large_lock) ||
                       locked(&_roots_lock) || locked(&_datasegments_lock) || locked(&_zombies_lock) ||
                       locked(&_region_lock) || locked(&_coverage_lock) ||
                       locked(&_associations_lock) ||
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
        // allocate_region won't actually allocate if not necessary
        Region *r = allocate_region();
        return (r && r->add_subzone(admin));
    }
    
    inline void clear_block(void *block, const size_t size) {
        void **end = (void **)displace(block, size);
        switch (size >> pointer_size_log2) {
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
            bzero(block, size);
            break;
        }
    }
    
    //
    // block_allocate
    //
    // Allocate a block of memory from the zone.  layout indicates whether the block is an
    // object or not and whether it is scanned or not.
    //
    void *Zone::block_allocate(Thread &thread, const size_t size, const usword_t layout, bool clear, bool refcount_is_one) {
        void *block;
        usword_t allocated_size = size;
        
        // make sure we allocate at least one byte
        if (!allocated_size) allocated_size = 1;

        // try thread cache first since first N sizes dominate all other allocations
        if (allocated_size < allocate_quantum_large) {
            Admin &admin = _partition.admin(allocated_size, layout, refcount_is_one);
            bool is_local = false;
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
                    block = admin.thread_cache_allocate(thread, allocated_size, layout, refcount_is_one, is_local);
                } while (!block && add_subzone(&admin));
            } else {
                do {
                    block = admin.find_allocation(thread, allocated_size, layout, refcount_is_one, is_local);
                } while (!block && add_subzone(&admin));
            }
#ifdef MEASURE_TLC_STATS
            if (is_local) {
                _stats.add_local_allocations(1); 
            } else {
                _stats.add_global_allocations(1);
            }
#endif
            if (block && !is_local) {
                adjust_allocation_counter(allocated_size); // locals are counted when they escape, larges are counted in allocate_large()
            }
        } else {
            // allocate more directly (32 bit: from vm, 64 bit: from top of arena)
            block = allocate_large(thread, allocated_size, layout, clear, refcount_is_one);
            // <rdar://problem/6150518> large blocks always come back fully cleared, either by VM itself, or by a bzero() in allocate_large().
            clear = false;
        }
    
        // if we could not allocate memory then we return here
        if (block == NULL) return NULL;
        
        if (should_collect()) {
            // must not trigger thread local collection here
            auto_zone_collect((auto_zone_t *)this, AUTO_ZONE_COLLECT_RATIO_COLLECTION|AUTO_ZONE_COLLECT_COALESCE);
        }

        // clear the block if requested.
        if (clear) clear_block(block, allocated_size);
        
        if (refcount_is_one)
            GARBAGE_COLLECTION_AUTO_REFCOUNT_ONE_ALLOCATION(allocated_size);
        
#if RECORD_REFCOUNT_STACKS
        if (AUTO_RECORD_REFCOUNT_STACKS) {
            auto_record_refcount_stack(this, ptr, 0);
        }
#endif
        return block;
    }
    
    unsigned Zone::batch_allocate(Thread &thread, size_t size, const usword_t layout, bool clear, bool refcount_is_one, void **results, unsigned num_requested) {
        usword_t allocated_size = size;
        unsigned count = 0;
        
        // make sure we allocate at least one byte
        if (!allocated_size) allocated_size = 1;
        
        if (allocated_size < allocate_quantum_large) {
            Admin &admin = _partition.admin(allocated_size, layout, refcount_is_one);
            count = admin.batch_allocate(thread, allocated_size, layout, refcount_is_one, clear, results, num_requested);
        } else {
            // we don't do bulk allocation of large
        }
        
        // if we could not allocate memory then we return here
        if (count == 0) return 0;
        
        adjust_allocation_counter(allocated_size * count);
        
        if (should_collect()) {
            // must not trigger thread local collection here
            auto_zone_collect((auto_zone_t *)this, AUTO_ZONE_COLLECT_RATIO_COLLECTION|AUTO_ZONE_COLLECT_COALESCE);
        }

        if (count && refcount_is_one && GARBAGE_COLLECTION_AUTO_REFCOUNT_ONE_ALLOCATION_ENABLED()) {
            for (unsigned i=0; i<count; i++)
                GARBAGE_COLLECTION_AUTO_REFCOUNT_ONE_ALLOCATION(allocated_size);
        }
                
        return count;
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
    void Zone::block_deallocate(SubzoneBlockRef block) {
        // BlockRef FIXME: optimize me
        void *address = block.address();
        Subzone *subzone = block.subzone();
        usword_t q = block.q();
        
        // explicitly deallocated blocks must have no associations.
        erase_associations(address);

        SpinLock adminLock(subzone->admin()->lock());
        // XXX could build single operation to update side data once instead of two operations here
        block.dec_refcount_no_lock();
        int layout = subzone->layout(q);
        if (layout & AUTO_OBJECT)
            erase_weak(address);
        // enlivening_enabled only changes while admin lock held
        // it indicates that a collection is in progress.
        // Other scanners though - gdb/dump are vulnerable if they don't also hold the admin locks.
        // They can't properly iterate via the admin if we're coalescing.
        if (((layout & AUTO_UNSCANNED) == AUTO_UNSCANNED) && !_enlivening_enabled) {
            int64_t block_size = subzone->size(q);
            subzone->admin()->deallocate_no_lock(subzone, q, address);    // ignore object -finalize work
            add_blocks_and_bytes(-1, -block_size);
        }
        else {
            // Inhibit finalization for NSDeallocateObject()'d objects.
            // Let collector find this next round when scanner can't be looking at it
            // Lose "Scanned" & possibly "Object"
            // XXX we could chain these and make all scanners process them
            subzone->set_layout(q, AUTO_MEMORY_UNSCANNED);
        }
    }
    void Zone::block_deallocate(LargeBlockRef block) {
        // BlockRef FIXME: optimize me
        void *address = block.address();
        Large *large = block.large();
        int layout = large->layout();
        if (layout & AUTO_OBJECT)
            erase_weak(address);
        large->set_layout(AUTO_MEMORY_UNSCANNED);
        // use the dispatch queue to deallocate the block "immediately"
        // note that we keep a refcount on block so it won't also be found by the collector
        if (_collection_queue) {
            Zone *zone = this;
            dispatch_async(_collection_queue, ^{ zone->deallocate_large(large, address); });
        }
    }
    
    //
    // block_start_large
    // 
    // Return the start of a large block.
    //
    Large *Zone::block_start_large(void *address) {
        // Note that coverage is updated *after* a large is allocated. It may be possible for this test to fail on a large in the process of being allocated.
        if (_coverage.in_range(address)) {
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
            
            return large;
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
            usword_t q;
            // one of the few subzone entries that guards for bad addresses
            return subzone->block_start(address, q);
        } else {
            Large *large = block_start_large(address);
            return large ? large->address() : NULL;
        }
    }


    //
    // block_layout
    //
    // Return the layout of a specified block.
    //
    usword_t Zone::block_layout(void *block) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            usword_t q;
            if (!subzone->block_is_start(block, &q)) return AUTO_TYPE_UNKNOWN;        // might have a pointer to a bogus location in a subzone
            return subzone->layout(q);
        } else if (block_is_start_large(block)) {
            Large *large = Large::large(block);
            return large->layout();
        }

        return AUTO_TYPE_UNKNOWN;
    }
    
    
    //
    // block_set_layout
    //
    // Set the layout of a block.
    //
    // BlockRef FIXME: retire?
    void Zone::block_set_layout(void *block, const usword_t layout) {
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            usword_t q;
            if (!subzone->block_is_start(block, &q)) return;          // might have a pointer to a bogus location in a subzone
            SpinLock lock(subzone->admin()->lock());
            subzone->set_layout(q, layout);
        } else if (block_is_start_large(block)) {
            Large *large = Large::large(block);
            large->set_layout(layout);
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
            thread.block_escaped(value);
            thread.block_escaped(block);
            
            // Creating associations must enliven objects that may become garbage otherwise.
            UnconditionalBarrier barrier(thread.needs_enlivening());
            WriteLock lock(&_associations_lock);
            AssociationsHashMap::iterator i = _associations.find(block);
            ObjectAssociationMap* refs = (i != _associations.end() ? i->second : NULL);
            if (refs == NULL) {
                refs = new ObjectAssociationMap();
                _associations[block] = refs;
            }
            (*refs)[key] = value;
            if (barrier) thread.enliven_block(value);
        } else {
            // setting the association to NULL breaks the association.
            WriteLock lock(&_associations_lock);
            AssociationsHashMap::iterator i = _associations.find(block);
            if (i != _associations.end()) {
                ObjectAssociationMap *refs = i->second;
                ObjectAssociationMap::iterator j = refs->find(key);
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
        ReadLock lock(&_associations_lock);
        AssociationsHashMap::iterator i = _associations.find(block);
        if (i != _associations.end()) {
            ObjectAssociationMap *refs = i->second;
            ObjectAssociationMap::iterator j = refs->find(key);
            if (j != refs->end()) return j->second;
        }
        return NULL;
    }

    //
    // get_associative_hash
    //
    // Returns the associated (random) hash value for a given block.
    //
    size_t Zone::get_associative_hash(void *block) {
        {
            ReadLock lock(&_associations_lock);
            PtrSizeHashMap::iterator i = _hashes.find(block);
            if (i != _hashes.end()) return i->second;
        }
        {
            // the thread local collector doesn't know how to track associative hashes.
            // Doesn't want to, either, since that would require taking the assoc lock on dealloc
            Thread &thread = registered_thread();
            thread.block_escaped(block);

            WriteLock lock(&_associations_lock);
            PtrSizeHashMap::iterator i = _hashes.find(block);
            if (i != _hashes.end()) return i->second;
            return (_hashes[block] = random());
        }
    }

    //
    // erase_associations_internal
    //
    // Assuming association lock held, do the dissassociation dance
    //
    inline void Zone::erase_associations_internal(void *block) {
        AssociationsHashMap::iterator i = _associations.find(block);
        if (i != _associations.end()) {
            ObjectAssociationMap *refs = i->second;
            _associations.erase(i);
            delete refs;
        }
        PtrSizeHashMap::iterator h = _hashes.find(block);
        if (h != _hashes.end()) {
            _hashes.erase(h);
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
        WriteLock lock(&_associations_lock);
        erase_associations_internal(block);
    }

    void Zone::erase_associations_in_range(const Range &r) {
        // <rdar://problem/6463922> When a bundle gets unloaded, search the associations table for keys within this range and remove them.
        WriteLock lock(&_associations_lock);
        PtrVector associationsToRemove;
        for (AssociationsHashMap::iterator i = _associations.begin(); i != _associations.end(); i++) {
            if (r.in_range(i->first)) associationsToRemove.push_back(i->first);
        }
        for (PtrVector::iterator i = associationsToRemove.begin(); i != associationsToRemove.end(); i++) {
            erase_associations_internal(*i);
        }
    }

    //
    // visit_associations_for_key
    //
    // Produces all associations for a given unique key.
    //
    void Zone::visit_associations_for_key(void *key, boolean_t (^visitor) (void *object, void *value)) {
        ReadLock lock(&_associations_lock);
        for (AssociationsHashMap::iterator i = _associations.begin(); i != _associations.end(); i++) {
            ObjectAssociationMap *refs = i->second;
            ObjectAssociationMap::iterator j = refs->find(key);
            if (j != refs->end()) {
                if (!visitor(i->first, j->second))
                    return;
            }
        }
    }

    //
    // sort_free_lists
    //
    // Rebuilds all the admin free lists from subzone side data. Requires that the caller hold the SubzonePartition locked.
    // The newly rebuilt free lists will be sorted.
    //
    void Zone::sort_free_lists() {
        _partition.for_each(^(Admin &admin){
            admin.reset_free_list();
        });
        
        SpinLock lock(&_region_lock);
        for (Region *region = _region_list; region != NULL; region = region->next()) {
            // iterate through the subzones
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.next()) {
                // get the number of quantum in the subzone
                usword_t n = subzone->allocation_count();
                Admin *admin = subzone->admin();
                for (usword_t q = 0; q < n; q = subzone->next_quantum(q)) {
                    if (subzone->is_free(q)) {
                        void *address = subzone->quantum_address(q);
                        FreeListNode *node = new(address) FreeListNode();
                        admin->append_node(node);
                    }
                }
            }
        }
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
        } else if (Large *large = block_start_large(destination)) {
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
        else if (Large *large = block_start_large(address)) {
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
    void Zone::reset_all_marks() {
        for (Region *region = _region_list; region != NULL; region = region->next()) {
            region->clear_marks();
        }
        
        // this is called from collect_end() so should be safe.
        SpinLock lock(&_large_lock);
        for (Large *large = _large_list; large != NULL; large = large->next()) {
            large->clear_mark();
        }
    }
   
    
    //
    // reset_all_pinned
    //
    // Clears the pinned bits on all blocks
    //
    void Zone::reset_all_pinned() {
        for (Region *region = _region_list; region != NULL; region = region->next()) {
            region->pinned().clear();
        }
    }
    
    
    //
    // malloc_statistics
    // rummage around and tot up the requisite numbers
    //
    void Zone::malloc_statistics(malloc_statistics_t *stats) {
        stats->blocks_in_use = _stats.count();
        stats->size_in_use = _stats.size();
        stats->max_size_in_use = stats->size_allocated = 0;
        {
            SpinLock lock(&_large_lock);
            Large *l = _large_list;
            while (l) {
                stats->max_size_in_use += l->size();
                stats->size_allocated += l->vm_size();
                l = l->next();
            }
        }
        
        {
            SubzonePartition::Lock lock(_partition);
            for (Region *region = region_list(); region != NULL; region = region->next()) {
                // iterate through the subzones
                SubzoneRangeIterator iterator(region->subzone_range());
                for (Subzone *sz = iterator.next(); sz != NULL; sz = iterator.next()) {
                    size_t bytes_per_quantum = (1L<<sz->quantum_log2());
                    stats->max_size_in_use += sz->allocation_count() * bytes_per_quantum;
                    stats->size_allocated += sz->allocation_limit() * bytes_per_quantum;
                }
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

    //
    // enlivening_barrier
    //
    // Used by collectors to synchronize with concurrent mutators.
    //
    void Zone::enlivening_barrier() {
        // Thread Local Enlivening.
        // TODO:  we could optimize this to allow threads to enter during one pass, and then do another pass fully locked down.
        Mutex lock(&_registered_threads_mutex);
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            LockedBoolean &needs_enlivening = thread->needs_enlivening();
            spin_lock(&needs_enlivening.lock);
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
        Mutex lock(&_registered_threads_mutex);
        _enlivening_enabled = false;
        _enlivening_complete = false;
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            LockedBoolean &needs_enlivening = thread->needs_enlivening();
            assert(needs_enlivening.state && needs_enlivening.lock != 0);
            needs_enlivening.state = false;
            spin_unlock(&needs_enlivening.lock);
        }
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
    // collect_begin
    //
    // Indicate the beginning of the collection period.
    //
    void Zone::collect_begin() {
        usword_t allocated = _allocation_counter;
        adjust_allocation_counter(-allocated);
        auto_atomic_add(allocated, &_triggered_threshold);
        _garbage_list.commit();
    }
    
    
    //
    // collect
    //
    // Performs the collection process.
    //
    void Zone::collect(bool is_partial, void *current_stack_bottom, CollectionTimer &timer) {
        
		GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)this, AUTO_TRACE_SCANNING_PHASE);


        // inform mutators that they need to add objects to the enlivening queue while scanning.
        // we lock around the rising edge to coordinate with eager block deallocation.
        // Grab all other locks that use ConditionBarrier on the enlivening_lock, then grab the enlivening_lock
        // and mark it.  This ordering guarantees that the the code using the ConditionBarrier can read the condition
        // without locking since they each have already acquired a lock necessary to change the needs_enlivening state.
        // All locks are released after setting needs_enlivening.
        set_needs_enlivening();

        // <rdar://problem/5495573> reserve all of the mark bits for exclusive use by the collector.
        pthread_mutex_lock(&_mark_bits_mutex);
        
        // scan the heap.
        // recycle unused Thread objects.
        recycle_threads();
        
        if (is_partial) collect_partial(current_stack_bottom, timer);
        else collect_full(current_stack_bottom, timer);

        GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)this, AUTO_TRACE_SCANNING_PHASE, _stats.blocks_scanned(), _stats.bytes_scanned());

        scavenge_blocks();
        
        // if weak references are present, threads will still be suspended, resume them after clearing weak references.
        auto_weak_callback_block_t *callbacks = NULL;
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

        // release any unused pages
        // release_pages();
        weak_call_callbacks(callbacks);
        
        if (!is_partial)
            purge_free_space();
    }

        
    //
    // collect_end
    //
    // Indicate the end of the collection period.
    //
    void Zone::collect_end(CollectionTimer &timer, size_t bytes_collected) {
        usword_t triggered = _triggered_threshold;
        auto_atomic_add(-triggered, &_triggered_threshold);
        
        _average_collection_time = (_average_collection_time * 7 + timer.total_time().microseconds()) >> 3;
        _garbage_list.uncommit();
    }
    
    //
    // purge_free_space
    //
    // Called in response to memory pressure to relinquish pages.
    //
    usword_t Zone::purge_free_space() {
        SubzonePartition::Lock lock(_partition);
        usword_t bytes_purged = _partition.purge_free_space_no_lock();
        return bytes_purged;
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
        size_t &_large_count;
        
        // Constructor
        scavenge_blocks_visitor(PointerList& list, size_t &large_count) : _list(list), _large_count(large_count) {}
        
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
                ++_large_count;
            }

            // always continue
            return true;
        }
    };
    
    
    void Zone::scavenge_blocks() {
        _garbage_list.clear_count();
        _large_garbage_count = 0;

        // set up the block scanvenger visitor
        scavenge_blocks_visitor visitor(_garbage_list, _large_garbage_count);
        
        // iterate through all the blocks
        visitAllocatedBlocks(this, visitor);
#ifdef MEASURE_TLC_STATS
        _stats.add_global_collected(_garbage_list.count() - _large_garbage_count);
#endif
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
                cursor->byte_count += auto_zone_size((auto_zone_t *)azone, ptr);
            }
        }
    }


    
    //
    // invalidate_garbage
    //
    void Zone::invalidate_garbage(const size_t garbage_count, void *garbage[]) {
#if DEBUG
        // when debugging, sanity check the garbage list in various ways.
        for (size_t index = 0; index < garbage_count; index++) {
            void *ptr = (void *)garbage[index];
            auto_block_info_sieve<AUTO_BLOCK_INFO_REFCOUNT> block_info(this, ptr);
            if (block_info.refcount() > 0)
                malloc_printf("invalidate_garbage: garbage ptr = %p, has non-zero refcount = %d\n", ptr, block_info.refcount());
        }
#endif
        struct auto_zone_cursor cursor = { (auto_zone_t *)this, garbage_count, garbage, 0, 0, 0 };
        if (control.batch_invalidate) {
            control.batch_invalidate((auto_zone_t *)this, foreach_block_do, &cursor, sizeof(cursor));
        }
    }

    void Zone::handle_overretained_garbage(void *block, int rc, auto_memory_type_t layout) {
        char *name;
        if (is_object(layout)) {
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
        if (is_object(layout) && control.name_for_address) free(name);
    }
    
    size_t Zone::free_garbage(const size_t subzone_garbage_count, void *subzone_garbage[],
                              const size_t large_garbage_count, void *large_garbage[],
                              size_t &blocks_freed, size_t &bytes_freed) {

        blocks_freed = bytes_freed = 0;

        if (collection_checking_enabled()) {
            clear_garbage_checking_count(subzone_garbage, subzone_garbage_count);
            clear_garbage_checking_count(large_garbage, large_garbage_count);
        }

        size_t subzone_overretained_count = 0;
        size_t large_overretained_count = 0;
        {
            WriteLock lock(associations_lock());
            if (subzone_garbage_count) {
                SubzonePartition::Lock lock(_partition);
                for (size_t index = 0; index < subzone_garbage_count; index++) {
                    void *block = subzone_garbage[index];
                    Subzone *subzone = Subzone::subzone(block);
                    usword_t q = subzone->quantum_index_unchecked(block);
                    // we only care if it is nonzero, so don't need to check the overflow table
                    if (!subzone->has_refcount(q)) {
                        if ((subzone->layout(q) & AUTO_OBJECT)) erase_weak(block);
                        blocks_freed++;
                        bytes_freed += subzone->size(q);
                        if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(zone), uintptr_t(block), 0, 0, 0);
                        erase_associations_internal(block);
                        subzone->admin()->deallocate_no_lock(subzone, q, block);
                    } else if (is_zombie(block)) {
                        SubzoneBlockRef ref(subzone, q);
                        zombify_internal(ref);
                    } else {
                        // reuse the buffer to keep track of overretained blocks
                        subzone_garbage[subzone_overretained_count++] = block;
                    }
                }
            }
            
            if (large_garbage_count) {
                SpinLock largeLock(&_large_lock);
                for (size_t index = 0; index < large_garbage_count; index++) {
                    void *block = large_garbage[index];
                    Large *large = Large::large(block);
                    int rc = large->refcount();
                    if (rc == 0) {
                        if (large->layout() & AUTO_OBJECT) erase_weak(block);
                        blocks_freed++;
                        bytes_freed += large->size();
                        if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(zone), uintptr_t(block), 0, 0, 0);
                        erase_associations_internal(block);
                        deallocate_large_internal(large, block);
                    } else if (is_zombie(block)) {
                        LargeBlockRef ref(large);
                        zombify_internal(ref);
                    } else {
                        // reuse the buffer to keep track of overretained blocks
                        large_garbage[large_overretained_count++] = large;
                    }
                }
            }
        }
    
        for (size_t index = 0; index < subzone_overretained_count; index++) {
            SubzoneBlockRef ref(subzone_garbage[index]);
            handle_overretained_garbage(ref);
        }
        for (size_t index = 0; index < large_overretained_count; index++) {
            LargeBlockRef ref((Large *)large_garbage[index]);
            handle_overretained_garbage(ref);
        }
        
        add_blocks_and_bytes(-(int64_t)blocks_freed, -(int64_t)bytes_freed);
        return bytes_freed;
    }
    
    // Dispatch threads store their dispatch_queue_t in thread local storage using __PTK_LIBDISPATCH_KEY0.
    inline bool is_dispatch_thread() {
        return _pthread_getspecific_direct(__PTK_LIBDISPATCH_KEY0) != NULL && !pthread_main_np();
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

            // is this a dispatch thread?
            if (_compaction_timer && is_dispatch_thread()) {
                if (_compaction_pending) {
                    // when compaction timer has fired, it sets the next timer to forever from now.
                    if (_compaction_next_time != DISPATCH_TIME_FOREVER)
                        dispatch_source_set_timer(_compaction_timer, DISPATCH_TIME_FOREVER, 0, 0);
                    _compaction_pending = false;
                }
            }
            
            // add thread to linked list of registered threads
            Mutex lock(&_registered_threads_mutex);
            thread->set_next(_registered_threads);
            LockedBoolean &needs_enlivening = thread->needs_enlivening();
            needs_enlivening.state = _enlivening_enabled;
            _registered_threads = thread;
            if (_enlivening_complete)
                spin_lock(&needs_enlivening.lock);

#if ! TARGET_IPHONE_SIMULATOR
            // add any existing __thread storage to the root set
            dyld_enumerate_tlv_storage(
                ^(enum dyld_tlv_states state, const dyld_tlv_info *info) 
            {
                this->add_datasegment(info->tlv_addr, info->tlv_size);
            });
#endif
        }
        pthread_setspecific(_registered_threads_key, thread);
        return *thread;
    }
    

    //
    // unregister_thread
    //
    void Zone::unregister_thread() {
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
    void Zone::scan_registered_threads(thread_visitor_t visitor) {
        for (Thread *thread = firstScannableThread(); thread != NULL; thread = nextScannableThread(thread)) {
            // visitor is guaranteed to only see threads locked for scanning.
            visitor(thread);
        }
    }

#ifndef __BLOCKS__
    class Zone_thread_visitor_helper : public Zone::thread_visitor {
        void (*_visitor) (Thread *, void *);
        void *_arg;
    public:
        Zone_thread_visitor_helper(void (*visitor) (Thread *, void *), void *arg) : _visitor(visitor), _arg(arg) {}
        virtual void operator () (Thread *thread) { _visitor(thread, _arg); }
    };
#endif
    
    void Zone::scan_registered_threads(void (*visitor) (Thread *, void *), void *arg) {
#ifdef __BLOCKS__
        scan_registered_threads(^(Thread *thread) { visitor(thread, arg); });
#else
        Zone_thread_visitor_helper helper(visitor, arg);
        scan_registered_threads(helper);
#endif
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
    // worker_thread_loop
    //
    // Helper function for recruit_worker_threads() used to get worker threads from dispatch_apply_f.
    //
    void Zone::worker_thread_loop(void *context, size_t step) {
        Zone *zone = (Zone *)context;
        zone->do_volunteer_for_work(true, true);
    }


    //
    // recruit_worker_threads
    //
    // Registers func as a work function. Threads will be recruited to call func repeatedly until it returns false.
    // func should perform a chunk of work and then return true if more work remains or false if all work is done.
    // The calling thread becomes a worker and does not return until all work is complete.
    //
    void Zone::perform_work_with_helper_threads(boolean_t (*work)(void *arg, boolean_t is_dedicated, boolean_t work_to_completion), void *arg) {
        pthread_mutex_lock(&_worker_lock);
        assert(_worker_count == 0);
        assert(_worker_func == NULL);
        assert(_worker_arg == NULL);
        
        _worker_arg = arg;
        _worker_func = work;
        _has_work = true;
        pthread_mutex_unlock(&_worker_lock);

        // This thread becomes a worker thread until all work is done.        
        dispatch_queue_t q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, DISPATCH_QUEUE_OVERCOMMIT);
        dispatch_apply_f((auto_ncpus()+1)/2, q, this, worker_thread_loop);

        // ensure that all worker threads have exited before we return
        pthread_mutex_lock(&_worker_lock);
        while (_worker_count != 0) {
            pthread_cond_wait(&_worker_cond, &_worker_lock);
        }
        _has_work = false;
        _worker_arg = NULL;
        _worker_func = NULL;
        pthread_mutex_unlock(&_worker_lock);
    }
    
    
    //
    // do_volunteer_for_work
    //
    // Helper function for volunteer_for_work(). This actually calls the work function.
    // If work_to_completion is true then the function loops until there is no more work.
    // If work_to_completion is false then the work function is called once.
    //
    boolean_t Zone::do_volunteer_for_work(boolean_t is_dedicated, boolean_t work_to_completion) {
        boolean_t more_work = false;
        
        // Test if worker thread recruiting is enabled. This test is unsynchronized, so we might miss sometimes for true volunteeer threads.
        pthread_mutex_lock(&_worker_lock);
        if (_has_work && (_worker_count < (size_t)auto_ncpus())) {
            _worker_count++;
            worker_print("starting (dedicated = %s, work_to_completion = %s)\n", is_dedicated ? "true" : "false", work_to_completion ? "true" : "false");
            do {
                pthread_mutex_unlock(&_worker_lock);
                more_work = _worker_func(_worker_arg, is_dedicated, work_to_completion);
                pthread_mutex_lock(&_worker_lock);

                if (more_work) {
                    // if there might be more work wake up any sleeping threads
                    if (_sleeping_workers > 0) {
                        pthread_cond_broadcast(&_worker_cond);
                    }
                } else {
                    if (work_to_completion) {
                        if ((_sleeping_workers + 1) < _worker_count) {
                            _sleeping_workers++;
                            pthread_cond_wait(&_worker_cond, &_worker_lock);
                            _sleeping_workers--;
                            more_work = _has_work;
                        }
                    }
                }
            } while (more_work && work_to_completion);
            worker_print("exiting (dedicated = %s, work_to_completion = %s)\n", is_dedicated ? "true" : "false", work_to_completion ? "true" : "false");

            // when a work_to_completion thread exits the loop we know all the work is done
            if (work_to_completion && _has_work) {
                // this will cause all sleeping worker threads to fall out of the loop
                _has_work = false;
            }
            _worker_count--;
            if (_worker_count == _sleeping_workers) {
                pthread_cond_broadcast(&_worker_cond);
            }
        }
        pthread_mutex_unlock(&_worker_lock);
        return more_work;
    }
    
    
    void Zone::register_resource_tracker(const char *description, boolean_t (^should_collect)(void))
    {
        Mutex lock(&_resource_tracker_lock);
        ResourceTracker *tracker = ResourceTracker::create_resource_tracker(description, should_collect, _resource_tracker_list);
        _resource_tracker_list = tracker;
    }
    
    void Zone::unregister_resource_tracker(const char *description)
    {
        Mutex lock(&_resource_tracker_lock);
        ResourceTracker *tracker = _resource_tracker_list;
        while (tracker && strcmp(tracker->description(), description))
            tracker = tracker->_next;
        if (tracker) {
            if (tracker == _resource_tracker_list)
                _resource_tracker_list = tracker->_next;
            tracker->unlink();
            delete tracker;
        }
    }

    boolean_t Zone::resource_tracker_wants_collection() {
        bool collect = false;
        Mutex lock(&_resource_tracker_lock);
        if (_resource_tracker_list) {
            ResourceTracker *tracker = _resource_tracker_list;
            while (tracker && !tracker->probe())
                tracker = tracker->_next;
            if (tracker) {
                if (control.log & AUTO_LOG_COLLECTIONS) {
                    malloc_printf("triggering collection due to external resource tracker: %s\n", tracker->description());
                }
                collect = true;
            }
        }
        return collect;
    }
    
    boolean_t Zone::should_collect() {
        boolean_t collect = false;
        
        volatile int64_t *last_should_collect_time = _stats.last_should_collect_time();
        int64_t start = *last_should_collect_time;
        WallClockTimeDataSource wallTime;
        int64_t current_time = wallTime.current_time();
        
        // Don't examine the statistics too often. Every 10ms is sufficient. */
        if ((wallTime.microseconds_duration(start, current_time) > 10 * 1000 /* 10 ms */) && 
            OSAtomicCompareAndSwap64(start, current_time, last_should_collect_time)) {
            
            // only try to collect if there has been recent allocation activity
            if (_allocation_counter > control.collection_threshold) {
                
                /*
                 This algrithm aims to have the collector running with a particular duty cycle (x% of the time).
                 The collector tracks how long collections take, and the time to wait is computed from the duty cycle.
                 total time = run time + idle time
                 And we want x% duty cycle, so
                 run time = total time * x%
                 Solving for the desired idle time between collections gives:
                 idle time = run time * (1/x - 1)
                 */
                WallClockTimer &idle_timer = _stats.idle_timer();
                uint64_t elapsed = idle_timer.elapsed_microseconds();
                if (elapsed > 10 * USEC_PER_SEC) {
                    // it's been a long time since the last collection, so ignore duty cycle and collect (safety net)
                    collect = true;
                } else {
                    double target_duty_cycle = Environment::default_duty_cycle;
                    uint64_t target_idle_time = ((double)_average_collection_time / target_duty_cycle) * (1.0 - target_duty_cycle);
                    
                    // collect if long enough since last collection
                    collect = elapsed > target_idle_time;
                }
            }
            if (!collect) {
                collect = resource_tracker_wants_collection();
            }
        }
        return collect;
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
                zone->print_block(SubzoneBlockRef(subzone, q), "");
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
            
            zone->print_block(LargeBlockRef(large), "");
             
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
    template <class BlockRef> void Zone::print_block(BlockRef block, const char *tag) {
        char *name = NULL;
        if (block.is_object()) {
            if (control.name_for_address) {
                name = control.name_for_address((auto_zone_t *)this, (vm_address_t)block.address(), 0);
            }
        }
        char desc[64];
        block.get_description(desc, sizeof(desc));
        
        malloc_printf("%s%p(%6d) %s %s %s %s rc(%d) %s %s\n",
                      tag, block.address(), (unsigned)block.size(),
                      block.is_scanned()   ? "scn"  : "   ",
                      block.is_object()    ? "obj"  : "mem",
                      block.is_new()       ? "new"  : "   ",
                      block.is_marked()    ? "mark" : "    ",
                      block.refcount(),
                      desc,
                      name ? name : "");
        if (name) free(name);
    }

};


