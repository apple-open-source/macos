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
    Zone.h
    Garbage Collected Heap
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_ZONE_CORE__
#define __AUTO_ZONE_CORE__

#include "auto_zone.h"
#include "auto_impl_utilities.h"
#include "auto_weak.h"

#include "Bitmap.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Large.h"
#include "Locks.h"
#include "Admin.h"
#include "Region.h"
#include "Statistics.h"
#include "Subzone.h"
#include "SubzonePartition.h"
#include "Thread.h"

#include <algorithm>
#include <cassert>

namespace Auto {

    //
    // Forward declarations.
    //
    class Monitor;
    class ResourceTracker;
    class SubzoneBlockRef;
    class LargeBlockRef;
    
    typedef PointerArray<VMMemoryAllocator> PointerList;

    typedef std::vector<Range, AuxAllocator<Range> > RangeVector;
    class ObjectAssociationMap : public PtrPtrMap, public AuxAllocated {}; // <rdar://problem/7212101> Reduce space usage for each association.
    typedef __gnu_cxx::hash_map<void *, ObjectAssociationMap*, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > AssociationsHashMap;

    
    //----- Zone -----//
    
    enum State {
        idle, scanning, enlivening, finalizing, reclaiming
    };

#define worker_print(fmt, args...)
//#define worker_print(fmt, args...) malloc_printf("worker %d: "fmt, Zone::worker_thread_id(), args);
    
    
    class Zone {
        
#define INVALID_THREAD_KEY_VALUE ((Thread *)-1)
        
      public:

        malloc_zone_t         basic_zone;

        // collection control
        auto_collection_control_t       control;

        // statistics
        spin_lock_t           stats_lock;               // only affects fields below; only a write lock; read access may not be accurate, as we lock statistics independently of the main data structures
    
        // weak references
        usword_t              num_weak_refs;
        usword_t              max_weak_refs;
        struct weak_entry_t  *weak_refs_table;
        spin_lock_t           weak_refs_table_lock;

        dispatch_once_t       _zone_init_predicate;
        dispatch_queue_t      _collection_queue;
        uint32_t              _collection_count;
        uint32_t              _collector_disable_count;     // counter for external disable-collector API
        uint8_t               _pending_collections[AUTO_ZONE_COLLECT_GLOBAL_MODE_COUNT]; // count of pending collections for each mode
        pthread_mutex_t       _collection_mutex;
        
        dispatch_source_t     _pressure_source;
        
        bool                  _compaction_pending;          // true if the compaction timer is armed.
        dispatch_source_t     _compaction_timer;            // resume this timer to trigger in the future.
        dispatch_time_t       _compaction_next_time;        // next allowed time for compaction.
                
      private:
      
        //
        // Shared information
        //
        // watch out for static initialization
        static volatile int32_t _zone_count;                // used to generate _zone_id
        static Zone           *_first_zone;                 // for debugging
        
        //
        // thread management
        //
        Thread                *_registered_threads;         // linked list of registered threads
        pthread_key_t          _registered_threads_key;     // pthread key for looking up Thread instance for this zone
        pthread_mutex_t        _registered_threads_mutex;   // protects _registered_threads and _enlivening_enabled
        bool                   _enlivening_enabled;         // tracks whether new threads should be initialized with enlivening on
        bool                   _enlivening_complete;        // tracks whether or not enlivening has been performed on this collection cycle.
        
        pthread_mutex_t       _mark_bits_mutex;             // protects the per-Region and Large block mark bits.

        //
        // memory management
        //
        Bitmap                 _in_subzone;                 // indicates which allocations are used for subzone region
        Bitmap                 _in_large;                   // indicates which allocations are used for large blocks
        Large                 *_large_list;                 // doubly linked list of large allocations
        spin_lock_t            _large_lock;                 // protects _large_list, _in_large, and large block refcounts
        PtrHashSet             _roots;                      // hash set of registered roots (globals)
        pthread_mutex_t        _roots_lock;                 // protects _roots
        RangeVector            _datasegments;               // registered data segments.
        spin_lock_t            _datasegments_lock;          // protects _datasegments
        PtrHashSet             _zombies;                    // hash set of zombies
        spin_lock_t            _zombies_lock;               // protects _zombies
        Region                *_region_list;                // singly linked list of subzone regions
        spin_lock_t            _region_lock;                // protects _region_list
        bool                   _repair_write_barrier;       // true if write barrier needs to be repaired after full collection.
        Range                  _coverage;                   // range of managed memory
        spin_lock_t            _coverage_lock;              // protects _coverage
        Statistics             _stats;                      // statistics for this zone
        volatile usword_t      _allocation_counter;         // byte allocation counter (reset after each collection).
        volatile usword_t      _triggered_threshold;        // stores _allocation_counter after reset for post collection statistics
        ResourceTracker        *_resource_tracker_list;     // list of registered external resource trackers
        pthread_mutex_t        _resource_tracker_lock;      // protects _resource_tracker_list (use a separate lock because we call out with it held)
        PointerList            _garbage_list;               // vm_map allocated pages to hold the garbage list.
        size_t                 _large_garbage_count;        // how many blocks in the _garbage_list are large (at the end).
        AssociationsHashMap    _associations;               // associative references object -> ObjectAssociationMap*.        
        PtrSizeHashMap         _hashes;                     // associative hash codes.
        pthread_rwlock_t       _associations_lock;          // protects _associations & _hashes
        volatile enum State    _state;                      // the state of the collector
        uint64_t               _average_collection_time;
        volatile int32_t       _collection_checking_enabled;// count of times the collector checking enabled count was called
        
#if UseArena
        void                    *_arena;                    // the actual 32G space (region low, larges high)
        void                    *_large_start;              // half-way into arena + size of bitmaps needed for region
        Bitmap                  _large_bits;                // bitmap of top half - tracks quanta used for large blocks
        spin_lock_t             _large_bits_lock;           // protects _large_bits
#endif
        SubzonePartition        _partition;                 // partitioned subzones
        
        pthread_mutex_t         _worker_lock;
        pthread_cond_t          _worker_cond;
        usword_t                _worker_count;
        usword_t                _sleeping_workers;
        boolean_t               _has_work;
        boolean_t               (*_worker_func)(void *, boolean_t, boolean_t);
        void                    *_worker_arg;
        
        pthread_mutex_t         _compaction_lock;
        boolean_t               _compaction_disabled;
        
        //
        // thread safe Large deallocation routines.
        //
        void deallocate_large(Large *large, void *block);
        void deallocate_large_internal(Large *large, void *block);
        
        
        //
        // allocate_region
        //
        // Allocate and initialize a new subzone region.
        //
        Region *allocate_region();
        
        
        //
        // allocate_large
        //
        // Allocates a large block from the universal pool (directly from vm_memory.)
        //
        void *allocate_large(Thread &thread, usword_t &size, const usword_t layout, bool clear, bool refcount_is_one);
    
    
        //
        // find_large
        //
        // Find a large block in this zone.
        //
        inline Large *find_large(void *block) { return Large::large(block); }


        //
        // deallocate_small_medium
        //
        // Release memory allocated for a small block
        //
        void deallocate_small_medium(void *block);
        

      public:
      
        //
        // raw memory allocation
        //

#if UseArena
        
        // set our one region up
        void *arena_allocate_region(usword_t newsize);
#endif

        // on 32-bit w/o arena, goes directly to vm system
        // w/arena, allocate from the top of the arena
        void *arena_allocate_large(usword_t size);
        
        //
        // raw memory deallocation
        //
        void arena_deallocate(void *, size_t size);
        
        //
        // admin_offset
        //
        // Return the number of bytes to the beginning of the first admin data item.
        //
        static inline const usword_t admin_offset() { return align(sizeof(Zone), page_size); }
        

        //
        // bytes_needed
        // 
        // Calculate the number of bytes needed for zone data
        //
        static inline const usword_t bytes_needed() {
            usword_t in_subzone_size = Bitmap::bytes_needed(subzone_quantum_max);
            usword_t in_large_size = Bitmap::bytes_needed(allocate_quantum_large_max);
#if UseArena
            usword_t arena_size = Bitmap::bytes_needed(allocate_quantum_large_max);
#else
            usword_t arena_size = 0;
#endif
            return admin_offset() + in_subzone_size + in_large_size + arena_size;
        }


        //
        // allocator
        //
        inline void *operator new(const size_t size) {
#if DEBUG
            // allocate zone data
            void *allocation_address = allocate_guarded_memory(bytes_needed());
#else
            void *allocation_address = allocate_memory(bytes_needed());
#endif
        
            if (!allocation_address) error("Can not allocate zone");
            
            return allocation_address;

        }


        //
        // deallocator
        //
        inline void operator delete(void *zone) {
#if DEBUG
            // release zone data
            if (zone) deallocate_guarded_memory(zone, bytes_needed());
#else
            if (zone) deallocate_memory(zone, bytes_needed());
#endif
        }
       
      
        //
        // setup_shared
        //
        // Initialize information used by all zones.
        //
        static void setup_shared();
        
        //
        // allocate_thread_key
        //
        // attempt to allocate a static pthread key for use when creating a new zone
        // returns the new key, or 0 if no keys are available.
        //
        static pthread_key_t allocate_thread_key();

        //
        // Constructors
        //
        Zone(pthread_key_t thread_registration_key);
        
        
        //
        // Destructor
        //
        ~Zone();
        

        //
        // zone
        //
        // Returns the lowest index zone - for debugging purposes only (no locks.)
        //
        static inline Zone *zone() { return _first_zone; }


        //
        // Accessors
        //
        inline Thread         *threads()                    { return _registered_threads; }
        inline pthread_mutex_t *threads_mutex()             { return &_registered_threads_mutex; }
        inline Region         *region_list()                { return _region_list; }
        inline spin_lock_t    *region_lock()                { return &_region_lock; }
        inline Large          *large_list()                 { return _large_list; }
        inline spin_lock_t    *large_lock()                 { return &_large_lock; }
        inline Statistics     &statistics()                 { return _stats; }
        inline Range          &coverage()                   { return _coverage; }
        inline PointerList    &garbage_list()               { return _garbage_list; }
        inline size_t          large_garbage_count()  const { return _large_garbage_count; }
        dispatch_queue_t       collection_queue() const     { return _collection_queue; }
        inline bool            compaction_disabled() const  { return _compaction_disabled; }
        inline bool            compaction_enabled() const   { return !_compaction_disabled; }
        inline pthread_key_t   thread_key() const           { return _registered_threads_key; }
        
        inline void           add_blocks_and_bytes(int64_t block_count, int64_t byte_count) { _stats.add_count(block_count); _stats.add_size(byte_count); }
        
        inline Thread *current_thread_direct() {
            if (_pthread_has_direct_tsd()) {
                #define CASE_FOR_DIRECT_KEY(key) case key: return (Thread *)_pthread_getspecific_direct(key)
                switch (_registered_threads_key) {
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY0);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY1);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY2);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY3);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY4);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY5);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY6);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY7);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY8);
                CASE_FOR_DIRECT_KEY(__PTK_FRAMEWORK_GC_KEY9);
                default: return NULL;
                }
            } else {
                return (Thread *)pthread_getspecific(_registered_threads_key);
            }
        }
        
        //
        // current_thread
        //
        // If the calling thread is registered with the collector, returns the registered Thread object.
        // If the calling thread is not registered, returns NULL.
        //
        inline Thread *current_thread() {
            Thread *thread = current_thread_direct();
            if (__builtin_expect(thread == INVALID_THREAD_KEY_VALUE, 0)) {
                // If we see this then it means some pthread destructor ran after the 
                // zone's destructor and tried to look up a Thread object (tried to perform a GC operation).
                // The collector's destructor needs to run last. We treat this as a fatal error so we will notice immediately.
                // Investigate as a pthreads bug in the ordering of static (Apple internal) pthread keys.
                auto_fatal("Zone::current_thread(): pthread looked up after unregister. Pthreads static key destructor ordering issue?\n");
            }
            return thread;
        }
        
        //
        // registered_thread
        //
        // Returns the Thread object for the calling thread.
        // If the calling thread is not registered, it is registered implicitly, and if warn_if_unregistered is true an error message is logged.
        //
        inline Thread &registered_thread() {
            Thread *thread = current_thread();
            if (!thread) {
                auto_error(this, "GC operation on unregistered thread. Thread registered implicitly. Break on auto_zone_thread_registration_error() to debug.", NULL);
                auto_zone_thread_registration_error();
                return register_thread();
            }
            return *thread;
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
        static void destroy_registered_thread(void *key_value);

        inline void           set_state(enum State ns)      { _state = ns; }
        inline bool           is_state(enum State ns)       { return _state == ns; }
        
        inline pthread_mutex_t      *roots_lock()           { return &_roots_lock; }
        inline PtrHashSet           &roots()                { return _roots; }

        inline pthread_rwlock_t     *associations_lock()    { return &_associations_lock; }
        inline AssociationsHashMap  &associations()         { return _associations; }
        
#if UseArena
        inline void *         arena()                       { return _arena; }
#else
        inline void *         arena()                       { return (void *)0; }
#endif
                
        inline void           adjust_allocation_counter(usword_t n)  { auto_atomic_add(n, &_allocation_counter); }
        
        //
        // subzone_index
        //
        // Returns a subzone index for an arbitrary pointer.  Note that this is relative to absolute memory.  subzone_index in
        // Region is relative memory. 
        //
        static inline const usword_t subzone_index(void *address) { return (((usword_t)address & mask(arena_size_log2)) >> subzone_quantum_log2); }
        
        
        //
        // subzone_count
        //
        // Returns a number of subzone quantum for a given size.
        //
        static inline const usword_t subzone_count(const size_t size) { return partition2(size, subzone_quantum_log2); }


        //
        // activate_subzone
        //
        // Marks the subzone as being active.
        //
        inline void activate_subzone(Subzone *subzone) { _in_subzone.set_bit_atomic(subzone_index(subzone)); }
        
        
        //
        // address_in_arena
        //
        // Given arbitrary address, is it in the arena of GC allocated memory
        //
        inline bool address_in_arena(const void *address) const {
#if UseArena
            //return (((usword_t)address) >> arena_size_log2) == (((usword_t)_arena) >> arena_size_log2);
            return ((usword_t)address & ~mask(arena_size_log2)) == (usword_t)_arena;
#else
            return true;
#endif
        }
        
        
        //
        // in_subzone_memory
        //
        // Returns true if address is in auto managed memory.
        //
        inline const bool in_subzone_memory(void *address) const { return address_in_arena(address) && (bool)_in_subzone.bit(subzone_index(address)); }
        
        
        //
        // in_subzone_bitmap
        //
        // Returns true if address is in a subzone that is in use, as determined by the subzone bitmap.
        //
        inline const bool in_subzone_bitmap(void *address) const { return (bool)_in_subzone.bit(subzone_index(address)); }

        
        //
        // in_large_memory
        //
        // Returns true if address is in auto managed memory.  Since side data is smaller than a large quantum we'll not
        // concern ourselves with rounding.
        //
        inline const bool in_large_memory(void *address) const {
#if UseArena
            usword_t arena_q = ((char *)address - (char *)_large_start) >> allocate_quantum_large_log2;
            return address_in_arena(address) && (arena_q < allocate_quantum_large_max) && (bool)_large_bits.bit(arena_q);
#else
            // since vm_allocate() returns addresses in arbitrary locations, can only really tell by calling Large::block_start() in 32-bit mode.
            return address_in_arena(address);
#endif
        }
        
        
        //
        // in_large_bitmap
        //
        // Returns true if the large bitmap bit corresponding to address is set.
        //
        inline const bool in_large_bitmap(void *address) const { return (bool)_in_large.bit(Large::quantum_index(address)); }
        
        
        //
        // good_block_size
        //
        // Return a block size which maximizes memory usage (no slop.)
        //
        static inline const usword_t good_block_size(usword_t size) {
            if (size <= allocate_quantum_large)  return align2(size, allocate_quantum_medium_log2);
            return align2(size, allocate_quantum_small_log2);
        }
        
        
        //
        // is_block
        //
        // Determines if the specfied address is a block in this zone.
        //
        inline bool is_block(void *address) {
            return _coverage.in_range(address) && block_is_start(address);
        }
        
        
        //
        // block_allocate
        //
        // Allocate a block of memory from the zone.  layout indicates whether the block is an
        // object or not and whether it is scanned or not.
        //
        void *block_allocate(Thread &thread, const size_t size, const usword_t layout, const bool clear, bool refcount_is_one);

        //
        // batch_allocate
        //
        // Allocate many blocks of memory from the zone.  layout indicates whether the block is an
        // object or not and whether it is scanned or not. All allocated blocks are zeroed.
        // Returns the number of blocks allocated.
        //
        unsigned batch_allocate(Thread &thread, size_t size, const usword_t layout, const bool clear, const bool refcount_is_one, void **results, unsigned num_requested);

        //
        // block_deallocate
        //
        // Release a block of memory from the zone, lazily while scanning.
        // 
        void block_deallocate(SubzoneBlockRef block);
        void block_deallocate(LargeBlockRef block);
        

        //
        // block_is_start_large
        //
        // Return true if arbitrary address is the start of a large block.
        //
        inline bool block_is_start_large(void *address) {
            if (Large::is_start(address)) {
#if UseArena
                // compute q using signed shift, the convert to unsigned to detect out of range address using the q < allocate_quantum_large_max test
                usword_t arena_q = ((char *)address - (char *)_large_start) >> allocate_quantum_large_log2;
                return (arena_q < allocate_quantum_large_max) && in_large_bitmap(address);
#else
                return in_large_bitmap(address);
#endif
            }
            return false;
        }

        
        //
        // block_is_start
        //
        // Return true if the arbitrary address is the start of a block.
        // Broken down because of high frequency of use.
        //
        inline bool block_is_start(void *address) {
            if (in_subzone_memory(address)) {
                usword_t q;
                return Subzone::subzone(address)->block_is_start(address, &q);
            }
            return block_is_start_large(address);
        }
        
        
        //
        // block_start_large
        // 
        // Return the start of a large block.
        //
        Large *block_start_large(void *address);


        //
        // block_start
        //
        // Return the base block address of an arbitrary address.
        // Broken down because of high frequency of use.
        //
        void *block_start(void *address);


        //
        // block_layout
        //
        // Return the layout of a block.
        //
        usword_t block_layout(void *address);


        //
        // block_set_layout
        //
        // Set the layout of a block.
        //
        void block_set_layout(void *address, const usword_t layout);


      private:
        //
        // close_locks
        //
        // acquires all locks for critical sections whose behavior changes during scanning
        // enlivening_lock is and must already be held; all other critical sections must
        // order their locks with enlivening_lock acquired first
        //
        inline void close_locks() {
                // acquire all locks for sections that have predicated enlivening work
            // (These locks are in an arbitary order)

            _partition.lock();
            
            // Eventually we'll acquire these as well as we reintroduce ConditionBarrier
            //spin_lock(&_retains_lock);          // retain/release
            //spin_lock(&weak_refs_table_lock);   // weak references
            //spin_lock(&_associations_lock);     // associative references
            //spin_lock(&_roots_lock);            // global roots
        }
        
        inline void open_locks() {
            //spin_unlock(&_roots_lock);
            //spin_unlock(&_associations_lock);
            //spin_unlock(&weak_refs_table_lock);
            //spin_unlock(&_retains_lock);
            _partition.unlock();
         }
        
      public:
      
        //
        // is_locked
        //
        // Called by debuggers, with all other threads suspended, to determine if any locks are held that might cause a deadlock from this thread.
        //
        bool is_locked();
      
      
        //
        // add_subzone
        //
        // when out of subzones, add another one, allocating region if necessary
        // return false if region can't be allocated
        //
        bool add_subzone(Admin *admin);

        //
        // block_refcount
        //
        // Returns the reference count of the specified block.
        //
        template <class BlockRef> usword_t block_refcount(BlockRef block) { return block.refcount(); }


        //
        // block_increment_refcount
        //
        // Increment the reference count of the specified block.
        //
        template <class BlockRef> usword_t block_increment_refcount(BlockRef block) {
            int refcount;
            
            Thread &thread = registered_thread();
            refcount = block.inc_refcount();
            if (refcount == 1) {
                ConditionBarrier barrier(thread.needs_enlivening());
                if (barrier) block.enliven();
            }
            return refcount;
        }


        //
        // block_decrement_refcount
        //
        // Decrement the reference count of the specified block.
        //
        template <class BlockRef> usword_t block_decrement_refcount(BlockRef block) { return block.dec_refcount(); }
        
        //
        // is_local
        //
        // Returns true if the known-to-be-a-block is a thread local node.
        //
        inline bool is_local(void *block) {
            if (in_subzone_memory(block)) {
                Subzone *subzone = Subzone::subzone(block);
                return subzone->is_thread_local(subzone->quantum_index_unchecked(block));
            } 
            return false;
        }


        //
        // block_is_garbage
        //
        // Returns true if the specified block is flagged as garbage.  Only valid 
        // during finalization.
        //
        inline bool block_is_garbage(void *block) {
            if (in_subzone_memory(block)) {
                Subzone *subzone = Subzone::subzone(block);
                usword_t q;
                return subzone->block_is_start(block, &q) && subzone->is_garbage(q);
            } else if (block_is_start_large(block)) {
                return Large::large(block)->is_garbage();
            }

            return false;
        }
        
        //
        // set_associative_ref
        //
        // Creates an association between a given block, a unique pointer-sized key, and a pointer value.
        //
        void set_associative_ref(void *block, void *key, void *value);

        
        //
        // get_associative_ref
        //
        // Returns the associated pointer value for a given block and key.
        //
        void *get_associative_ref(void *block, void *key);

        
        //
        // get_associative_hash
        //
        // Returns the associated (random) hash value for a given block.
        //
        size_t get_associative_hash(void *block);

                
        //
        // erase_associations_internal
        //
        // Assuming association lock held, do the dissassociation dance
        //
        void erase_associations_internal(void *block);
        
        //
        // erase_assocations
        //
        // Removes all associations for a given block. Used to
        // clear associations for explicitly deallocated blocks.
        // When the collector frees blocks, it uses a different code
        // path, to minimize locking overhead. See free_garbage().
        //
        void erase_associations(void *block);

        //
        // erase_associations_in_range
        //
        // Called by remove_datasegment() below, when a data segment is unloaded
        // to automatically break associations referenced by global objects (@string constants).
        //
        void erase_associations_in_range(const Range &r);
        
        //
        // visit_associations_for_key
        //
        // Produces all associations for a given unique key.
        //
        void visit_associations_for_key(void *key, boolean_t (^visitor) (void *object, void *value));
                
        //
        // sort_free_lists
        //
        // Rebuilds all the admin free lists from subzone side data. Requires that the caller hold the SubzonePartition locked.
        // The newly rebuilt free lists will be sorted.
        //
        void sort_free_lists();
        
        //
        // add_root
        //
        // Adds the address as a known root.
        // Performs the assignment in a race-safe way.
        // Escapes thread-local value if necessary.
        //
        template <class BlockRef> inline void add_root(void *root, BlockRef value) {
            Thread &thread = registered_thread();
            thread.block_escaped(value);
            
            UnconditionalBarrier barrier(thread.needs_enlivening());
            Mutex lock(&_roots_lock);
            if (_roots.find(root) == _roots.end()) {
                _roots.insert(root);
            }
            // whether new or old, make sure it gets scanned
            // if new, well, that's obvious, but if old the scanner may already have scanned
            // this root and we'll never see this value otherwise
            if (barrier) value.enliven();
            *(void **)root = value.address();
        }
        
        
        //
        // add_root_no_barrier
        //
        // Adds the address as a known root.
        //
        inline void add_root_no_barrier(void *root) {
#if DEBUG
            // this currently fires if somebody uses the wrong version of objc_atomicCompareAndSwap*
            //if (in_subzone_memory(root)) __builtin_trap();
#endif

            Mutex lock(&_roots_lock);
            if (_roots.find(root) == _roots.end()) {
                _roots.insert(root);
            }
        }
        
        //
        // copy_roots
        //
        // Takes a snapshot of the registered roots during scanning.
        //
        inline void copy_roots(PointerList &list) {
            Mutex lock(&_roots_lock);
            usword_t count = _roots.size();
            list.clear_count();
            list.grow(count);
            list.set_count(count);
            std::copy(_roots.begin(), _roots.end(), (void**)list.buffer());
        }
        
        //
        // copy_roots
        //
        // Takes a snapshot of the registered roots during scanning.
        //
        inline void copy_roots(PtrVector &list) {
            Mutex lock(&_roots_lock);
            usword_t count = _roots.size();
            list.resize(count);
            std::copy(_roots.begin(), _roots.end(), list.begin());
        }

        
        // remove_root
        //
        // Removes the address from the known roots.
        //
        inline void remove_root(void *root) {
            Mutex lock(&_roots_lock);
            PtrHashSet::iterator iter = _roots.find(root);
            if (iter != _roots.end()) {
                _roots.erase(iter);
            }
        }
        
        
        //
        // is_root
        //
        // Returns whether or not the address has been registered.
        //
        inline bool is_root(void *address) {
            Mutex lock(&_roots_lock);
            PtrHashSet::iterator iter = _roots.find(address);
            return (iter != _roots.end());
        }

        //
        // RangeLess
        //
        // Compares two ranges, returning true IFF r1 is left of r2 on the number line.
        // Returns false if the ranges overlap in any way.
        //
        struct RangeLess {
          bool operator()(const Range &r1, const Range &r2) const {
            return (r1.address() < r2.address()) && (r1.end() <= r2.address()); // overlapping ranges will always return false.
          }
        };
        
        //
        // add_datasegment
        //
        // Adds the given data segment address range to a list of known data segments, which is searched by is_global_address().
        //
        inline void add_datasegment(const Range &r) {
            SpinLock lock(&_datasegments_lock);
            RangeVector::iterator i = std::lower_bound(_datasegments.begin(), _datasegments.end(), r, RangeLess());
            _datasegments.insert(i, r);
        }
        
        //
        // RangeExcludes
        //
        // Returns false if the address lies outside the given range.
        //
        struct RangeExcludes {
            Range _range;
            RangeExcludes(const Range &r) : _range(r) {}
            bool operator()(void *address) { return !_range.in_range(address); }
        };

        //
        // RootRemover
        //
        // Used by remove_datasegment() below, removes an address from the
        // root table. Simply an artifact for use with std::for_each().
        //
        struct RootRemover {
            PtrHashSet &_roots;
            RootRemover(PtrHashSet &roots) : _roots(roots) {}
            void operator()(void *address) { 
                PtrHashSet::iterator iter = _roots.find(address);
                if (iter != _roots.end()) _roots.erase(iter);
            }
        };
    
        //
        // remove_datasegment
        //
        // Removes the given data segment address range from the list of known address ranges.
        //
        inline void remove_datasegment(const Range &r) {
            {
                SpinLock lock(&_datasegments_lock);
                // could use std::lower_bound(), or std::equal_range() to speed this up, since they use binary search to find the range.
                // _datasegments.erase(std::remove(_datasegments.begin(), _datasegments.end(), r, _datasegments.end()));
                RangeVector::iterator i = std::lower_bound(_datasegments.begin(), _datasegments.end(), r, RangeLess());
                if (i != _datasegments.end()) _datasegments.erase(i);
            }
            {
                // When a bundle gets unloaded, scour the roots table to make sure no stale roots are left behind.
                Mutex lock(&_roots_lock);
                PtrVector rootsToRemove;
                std::remove_copy_if(_roots.begin(), _roots.end(), std::back_inserter(rootsToRemove), RangeExcludes(r));
                std::for_each(rootsToRemove.begin(), rootsToRemove.end(), RootRemover(_roots));
            }
            erase_associations_in_range(r);
            weak_unregister_data_segment(this, r.address(), r.size());
        }
        
        inline void add_datasegment(void *address, size_t size) { add_datasegment(Range(address, size)); }
        inline void remove_datasegment(void *address, size_t size) { remove_datasegment(Range(address, size)); }

        //
        // is_global_address
        //
        // Binary searches the registered data segment address ranges to determine whether the address could be referring to
        // a global variable.
        //
        inline bool is_global_address(void *address) {
            SpinLock lock(&_datasegments_lock);
            return is_global_address_nolock(address);
        }
        
        inline bool is_global_address_nolock(void *address) {
            return std::binary_search(_datasegments.begin(), _datasegments.end(), Range(address, sizeof(void*)), RangeLess());
        }

#if DEBUG
        //
        // DATASEGMENT REGISTRATION UNIT TEST
        //
        struct RangePrinter {
            void operator() (const Range &r) {
                printf("{ address = %p, end = %p }\n", r.address(), r.end());
            }
        };
        
        inline void print_datasegments() {
            SpinLock lock(&_datasegments_lock);
            std::for_each(_datasegments.begin(), _datasegments.end(), RangePrinter());
        }

        void test_datasegments() {
            Range r1((void*)0x1000, 512), r2((void*)0xA000, 512);
            add_datasegment(r1);
            add_datasegment(r2);
            print_datasegments();
            Range r3(r1), r4(r2);
            r3.adjust(r1.size()), r4.adjust(-r2.size());
            add_datasegment(r3);
            add_datasegment(r4);
            print_datasegments();
            assert(is_global_address(r1.address()));
            assert(is_global_address(displace(r1.address(), 0x10)));
            assert(is_global_address(displace(r1.end(), -sizeof(void*))));
            assert(is_global_address(displace(r2.address(), 0xA0)));
            assert(is_global_address(displace(r3.address(), 0x30)));
            assert(is_global_address(displace(r4.address(), 0x40)));
            remove_datasegment(r2);
            print_datasegments();
            assert(!is_global_address(displace(r2.address(), 0xA0)));
            remove_datasegment(r1);
            assert(!is_global_address(displace(r1.address(), 0x10)));
            print_datasegments();
            remove_datasegment(r3);
            remove_datasegment(r4);
            print_datasegments();
        }
#endif

        //
        // erase_weak
        //
        // unregisters any weak references contained within known AUTO_OBJECT
        //
        inline void erase_weak(void *ptr) {
            if (control.weak_layout_for_address) {
                const unsigned char* weak_layout = control.weak_layout_for_address((auto_zone_t*)zone, ptr);
                if (weak_layout) weak_unregister_with_layout(this, (void**)ptr, weak_layout);
            }
        }

        //
        // add_zombie
        //
        // Adds address to the zombie set.
        //
        inline void add_zombie(void *address) {
            SpinLock lock(&_zombies_lock);
            if (_zombies.find(address) == _zombies.end()) {
                _zombies.insert(address);
            }
        }

        
        //
        // is_zombie
        //
        // Returns whether or not the address is in the zombie set.
        //
        inline bool is_zombie(void *address) {
            SpinLock lock(&_zombies_lock);
            PtrHashSet::iterator iter = _zombies.find(address);
            return (iter != _zombies.end());
        }
        
        //
        // clear_zombies
        //
        inline void clear_zombies() {
            SpinLock lock(&_zombies_lock);
            _zombies.clear();
        }
        
        //
        // zombify_internal
        //
        // Called by free_garbage() on blocks added to the zombie set.
        //
        // Assumes admin/large locks are held by the caller.
        //
        template <class BlockRef> void zombify_internal(BlockRef block) {
            erase_weak(block.address());
            // callback morphs the object into a zombie.
            if (control.resurrect) control.resurrect((auto_zone_t *)this, block.address());
            block.set_layout(AUTO_OBJECT_UNSCANNED);
            block.dec_refcount_no_lock();
        }
        
        //
        // set_write_barrier
        //
        // Set the write barrier byte corresponding to the specified address.
        // If scanning is going on then the value is marked pending.
        //
        template <class DestBlock, class ValueBlock> void set_write_barrier(Thread &thread, DestBlock dest_block, const void **dest_addr, ValueBlock value_block, const void *value) {
            thread.track_local_assignment(dest_block, value_block);

            UnconditionalBarrier barrier(thread.needs_enlivening());
            if (barrier) value_block.enliven();
            *dest_addr = value;
            // only need to mark the card if value can possibly be collected by generational
            if (value_block.is_thread_local() || value_block.is_new())
                dest_block.mark_card(dest_addr);
        }
        
        
        //
        // set_write_barrier_range
        //
        // Set the write barrier bytes corresponding to the specified address & length.
        // Returns if the address is within an allocated block (and barrier set)
        //
        bool set_write_barrier_range(void *address, const usword_t size);
        
        
        //
        // set_write_barrier
        //
        // Set the write barrier byte corresponding to the specified address.
        // Returns if the address is within an allocated block (and barrier set)
        //
        bool set_write_barrier(void *address);


        //
        // mark_write_barriers_untouched
        //
        // iterate through all the write barriers and mark the live cards as provisionally untouched.
        //
        void mark_write_barriers_untouched();


        //
        // clear_untouched_write_barriers
        //
        // iterate through all the write barriers and clear all the cards still marked as untouched.
        //
        void clear_untouched_write_barriers();


        //
        // clear_all_write_barriers
        //
        // iterate through all the write barriers and clear all the marks.
        //
        void clear_all_write_barriers();


        //
        // reset_all_marks
        //
        // Clears the mark flags on all blocks
        //
        void reset_all_marks();
       
        
        //
        // reset_all_pinned
        //
        // Clears the pinned bits on all blocks
        //
        void reset_all_pinned();


        inline void set_repair_write_barrier(bool repair) { _repair_write_barrier = repair; }
        inline bool repair_write_barrier() const { return _repair_write_barrier; }
        
        
        //
        // set_needs_enlivening
        //
        // Inform all known threads that scanning is about to commence, thus blocks will need to be
        // enlivened to make sure they aren't missed during concurrent scanning.
        //
        void set_needs_enlivening();
        
        //
        // enlivening_barrier
        //
        // Called by Collector::scan_barrier() to enliven all blocks that
        // would otherwise be missed by concurrent scanning.
        //
        void enlivening_barrier();
        
        //
        // clear_needs_enlivening
        //
        // Unblocks threads that may be spinning waiting for enlivening to finish.
        //
        void clear_needs_enlivening();
        
        
        //
        // collect_begin
        //
        // Indicate the beginning of the collection period.
        //
        void  collect_begin();
        
        
        //
        // collect_end
        //
        // Indicate the end of the collection period.
        //
        void  collect_end(CollectionTimer &timer, size_t bytes_collected);
        
        //
        // purge_free_space
        //
        // Called in response to memory pressure to relinquish pages.
        //
        usword_t purge_free_space();
        
        //
        // block_collector
        //
        // Called to lock the global mark bits and thread lists.
        // Returns true if successful.
        //
        bool block_collector();
        
        //
        // unblock_collector
        //
        // Called to unlock the global mark bits and thread lists.
        //
        void unblock_collector();
        
        //
        // collect
        //
        // Performs the collection process.
        //
        void collect(bool is_partial, void *current_stack_bottom, CollectionTimer &timer);
        
        //
        // collect_partial
        //
        // Performs a partial (generational) collection.
        //
        void collect_partial(void *current_stack_bottom, CollectionTimer &timer);
        
        //
        // collect_full
        //
        // Performs a full heap collection.
        //
        void collect_full(void *current_stack_bottom, CollectionTimer &timer);

        //
        // analyze_heap
        //
        // Analyzes the compaction viability of the current heap.
        //
        void analyze_heap(const char *path);
        
        //
        // compact_heap
        //
        // Compacts entire garbage collected heap.
        //
        void compact_heap();
        
        //
        // disable_compaction
        //
        // Disables compaction permanently. If called during a compaction, blocks until the compaction finishes.
        //
        void disable_compaction();
        
        //
        // incremental compaction support.
        //
        void set_in_compaction();
        void compaction_barrier();
        void clear_in_compaction();

        //
        // scavenge_blocks
        //
        // Constructs a list of all blocks that are to be garbaged
        //
        void scavenge_blocks();
        
        //
        // invalidate_garbage
        //
        // Given an array of garbage, do callouts for finalization
        //
        void invalidate_garbage(const size_t garbage_count, void *garbage[]);

        //
        // handle_overretained_garbage
        //
        // called when we detect a garbage block has been over retained during finalization
        // logs a (fatal, based on the setting) resurrection error
        // 
        void handle_overretained_garbage(void *block, int rc, auto_memory_type_t layout);
        template <class BlockRef> inline void handle_overretained_garbage(BlockRef block) {
            handle_overretained_garbage(block.address(), block.refcount(), block.layout());
        }
        
        //
        // free_garbage
        //
        // Free subzone/large arrays of garbage, en-masse.
        //
        size_t free_garbage(const size_t subzone_garbage_count, void *subzone_garbage[],
                            const size_t large_garbage_count, void *large_garbage[],
                            size_t &blocks_freed, size_t &bytes_freed);
        
        //
        // release_pages
        //
        // Release any pages that are not in use.
        //
        void release_pages() {
        }
        
        //
        // recycle_threads
        //
        // Searches for unbound threads, queueing them for deletion.
        //
        void recycle_threads();
        
        //
        // register_thread
        //
        // Add the current thread as a thread to be scanned during gc.
        //
        Thread &register_thread();


        //
        // unregister_thread
        //
        // deprecated
        //
        void unregister_thread();

    private:
        Thread *firstScannableThread();
        Thread *nextScannableThread(Thread *thread);

    public:
        //
        // scan_registered_threads
        //
        // Safely enumerates the registered threads, ensuring that their stacks
        // remain valid during the call to the scanner block.
        //
#ifdef __BLOCKS__
        typedef void (^thread_visitor_t) (Thread *thread);
#else
        class thread_visitor {
        public:
            virtual void operator() (Thread *thread) = 0;
        };
        typedef thread_visitor &thread_visitor_t;
#endif
        void scan_registered_threads(thread_visitor_t scanner);
        void scan_registered_threads(void (*visitor) (Thread *, void *), void *arg);

        //
        // suspend_all_registered_threads
        //
        // Suspend all registered threads. Provided for heap snapshots.
        // Acquires _registered_threads_lock so that no new threads can enter the system.
        //
        void suspend_all_registered_threads();


        //
        // resume_all_registered_threads
        //
        // Resumes all suspended registered threads.  Only used by the monitor for heap snapshots.
        // Relinquishes the _registered_threads_lock.
        //
        void resume_all_registered_threads();
        
        
        //
        // perform_work_with_helper_threads
        //
        // Registers func as a work function. Threads will be recruited to call func repeatedly until it returns false.
        // func should perform a chunk of work and then return true if more work remains or false if all work is done.
        // The calling thread becomes a worker and does not return until all work is complete.
        //
        void perform_work_with_helper_threads(boolean_t (*work)(void *arg, boolean_t is_dedicated, boolean_t work_to_completion), void *arg);

        
        //
        // volunteer_for_work
        //
        // May be called by threads to volunteer to do work on the collector's behalf.
        // If work is available then a chunk of work is performed and the thread returns.
        // If no work is available then the call is a no-op.
        // Returns true if there is more work to be done, false if not.
        // The intent of this function is that threads can call it while waiting on a spin lock.
        //
        boolean_t volunteer_for_work(boolean_t work_to_completion) {
            if (_has_work > 0) return do_volunteer_for_work(false, work_to_completion);
            return false;
        }
        
        
        //
        // do_volunteer_for_work
        //
        // Helper function for volunteer_for_work(). This actually calls the work function.
        // If is_dedicated is true then the function loops until there is no more work.
        // If is_dedicated is false then the work function is called once.
        //
        boolean_t do_volunteer_for_work(boolean_t is_dedicated, boolean_t work_to_completion);

        
        //
        // worker_thread_loop
        //
        // Helper function for recruit_worker_threads() used to get worker threads from dispatch_apply_f.
        //
        static void worker_thread_loop(void *context, size_t step);

        
        //
        // weak references.
        //
        unsigned has_weak_references() { return (num_weak_refs != 0); }

        //
        // layout_map_for_block.
        //
        // Used for precise (non-conservative) block scanning.
        //
        const unsigned char *layout_map_for_block(void *block) {
            // FIXME:  for speed, change this to a hard coded offset from the block's word0 field.
            return control.layout_for_address ? control.layout_for_address((auto_zone_t *)this, block) : NULL;
        }
        
        //
        // weak_layout_map_for_block.
        //
        // Used for conservative block with weak references scanning.
        //
        const unsigned char *weak_layout_map_for_block(void *block) {
            // FIXME:  for speed, change this to a hard coded offset from the block's word0 field.
            return control.weak_layout_for_address ? control.weak_layout_for_address((auto_zone_t *)this, block) : NULL;
        }
        
        //
        // name_for_object
        //
        // For blocks with AUTO_OBJECT layout, return a name for the object's type.
        //
        const char *name_for_object(void *object) {
            return control.name_for_object ? control.name_for_object((auto_zone_t *)this, object) : "";
        }
        
        //
        // forward_block
        //
        // Forwards a block to a new location during compaction.
        //
        void *forward_block(Subzone *subzone, usword_t q, void *block);
        
        //
        // move_block
        //
        // Moves the block into its new location (using the forwarding pointer).
        //
        void move_block(Subzone *subzone, usword_t q, void *block);
        
        //
        // print_all_blocks
        //
        // Prints all allocated blocks.
        //
        void print_all_blocks();
        
        
        //
        // print block
        //
        // Print the details of a block
        //
        template <class BlockRef> void print_block(BlockRef block, const char *tag);

        //
        // malloc_statistics
        //
        // computes the necessary malloc statistics
        //
        void malloc_statistics(malloc_statistics_t *stats);

        //
        // should_collect
        //
        // Queries all registered resource trackers (including the internal allocation threshold)
        // to determine whether a collection should run.
        //
        boolean_t should_collect();
        
        //
        // register_resource_tracker
        //
        // Register an external resource tracker. Refer to auto_zone_register_resource_tracker().
        //        
        void register_resource_tracker(const char *description, boolean_t (^should_collect)(void));
        
        //
        // register_resource_tracker
        //
        // Unregister an external resource tracker. Refer to auto_zone_register_resource_tracker().
        //        
        void unregister_resource_tracker(const char *description);
        
        
        //
        // resource_tracker_wants_collection
        //
        // Poll the list of registered resources trackers asking if a collection should be triggered.
        // Returns true if some registered tracker indicates a collection is desired, false if none do.
        //                
        boolean_t resource_tracker_wants_collection();

        //
        // collection_checking_threshold
        //
        // Fetch the current collection checking threshold. 0 = collection checking disabled
        //        
        inline uint32_t const collection_checking_enabled() { return _collection_checking_enabled != 0;}
        
        
        //
        // enable_collection_checking/disable_collection_checking
        //
        // Increment/decrement the collection checking enabled counter.
        // Collection checking is enabled when the counter is nonzero.
        //        
        void enable_collection_checking();
        void disable_collection_checking();
        
        
        //
        // track_pointer
        //
        // Register a block for collection checking. This is a fast no-op if collection checking is disabled.
        //        
        void track_pointer(void *pointer);
        
        
        //
        // increment_check_counts
        //
        // Increment the collection count for all blocks registered for collection checking.
        //        
        void increment_check_counts();

        
        //
        // clear_garbage_checking_count
        //
        // Unregisters garbage blocks from collection checking.
        //        
        void clear_garbage_checking_count(void **garbage, size_t count);
        
        //
        // enumerate_uncollected
        //
        // Enumerates all allocated blocks and calls callback for those that are being tracked.
        //        
        void enumerate_uncollected(auto_zone_collection_checking_callback_t callback);


#ifdef __BLOCKS__
        //
        // dump_zone
        //
        // call blocks with everything needed to recreate heap
        // blocks are called in the order given
        //
        void dump_zone(
            auto_zone_stack_dump stack_dump,
            auto_zone_register_dump register_dump,
            auto_zone_node_dump thread_local_node_dump,
            auto_zone_root_dump root_dump,
            auto_zone_node_dump global_node_dump,
            auto_zone_weak_dump weak_dump_entry
        );
        
        //
        // visit_zone
        //
        // Used to enumerate all of the interesting data structures
        // of the Zone. Supersedes dump_zone().
        //
        void visit_zone(auto_zone_visitor_t *visitor);
#endif
        
   };

    
};


#endif // __AUTO_ZONE_CORE__
