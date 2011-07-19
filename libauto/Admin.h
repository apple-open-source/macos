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
    Admin.h
    Automatic Garbage Collection
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_ADMIN__
#define __AUTO_ADMIN__

#include "Bitmap.h"
#include "Configuration.h"
#include "Definitions.h"
#include "AllocationCache.h"
#include "Range.h"
#include "Statistics.h"
#include "auto_impl_utilities.h"

namespace Auto {

    // Forward declarations
    //
    class Region;
    class Subzone;
    class Zone;
    class Thread;
    
    
    //----- Admin -----//
    
    class Admin {
    
      private:
      
        Zone            *_zone;                             // managing zone
        usword_t        _quantum_log2;                      // ilog2 of the quantum used in this admin
        AllocationCache _cache;                             // free lists, one for each quanta size, slot 0 is for large clumps
        Subzone         *_active_subzone;                   // subzone with unused memory
        Subzone         *_purgeable_subzones;               // subzones that contain allocation_count() < allocation_limit(), to be considered when active subzone is exhausted.
        spin_lock_t     _admin_lock;                        // protects free list, subzone data.
        usword_t        _freelist_search_cap;               // highest nonempty freelist index (excluding big chunk list), or 0 if all are empty
        usword_t        _layout;                            // either AUTO_MEMORY_SCANNED or AUTO_MEMORY_UNSCANNED
        PtrIntHashMap   _retains;                           // STL hash map of retain counts, protected by _admin_lock

        //
        // batch_allocate_from_cache_no_lock
        //
        // helper function for batch allocate. performs a batch allocation from a specific free list
        unsigned batch_allocate_from_freelist_slot_no_lock(usword_t cache_slot, usword_t quantum_size, const bool clear, void **results, unsigned num_requested);
        
        //
        // batch_allocate_from_subzone_no_lock
        //
        // helper function for batch allocate. performs a batch allocation from unused/purged space in a specific subzone
        unsigned batch_allocate_from_subzone_no_lock(Subzone *subzone, usword_t requested_size, const bool clear, void **results, unsigned num_requested);

        //
        // activate_purged_subzone
        //
        // try to reuse a subzone from the purgeable list. only choose a subzone with enough space to make it worth reusing.
        //
        void activate_purged_subzone();
        
        //
        // visit_purgeable_nodes
        //
        // Visits all free list nodes that exceed 1 page in size, and purgeable subzones.
        //
        template <typename PurgeVisitor> void visit_purgeable_nodes(PurgeVisitor &visitor);
    
    public:
      
        //
        // Accessors
        //
        Zone *zone()            const { return _zone; }
        usword_t quantum_log2() const { return _quantum_log2; }
        spin_lock_t *lock()           { return &_admin_lock; }
        PtrIntHashMap &retains()      { return _retains; }

        //
        // is_small
        //
        // Return true if it is a small admin.
        //
        inline bool is_small() const { return _quantum_log2 == allocate_quantum_small_log2; }
        
        
        //
        // is_medium
        //
        // Return true if it is a medium admin.
        //
        inline bool is_medium() const { return _quantum_log2 == allocate_quantum_medium_log2; }
        
        
        //
        // layout
        //
        // Returns AUTO_MEMORY_SCANNED or AUTO_MEMORY_UNSCANNED.
        //
        inline const usword_t layout() const { return _layout; }
        
        //
        // quantum_count
        //
        // Returns a number of quantum for a given size.
        //
        inline const usword_t quantum_count(const size_t size) const {
            return partition2(size, _quantum_log2);
        }
        

        //
        // unused_count
        //
        // Returns a number of quantum for a given size.
        //
        usword_t unused_count();
        

        //
        // active_subzone
        //
        // Returns the most recently added subzone
        //
        inline Subzone *active_subzone() { return _active_subzone; }
        

        //
        // set_active_subzone
        //
        // Remember the most recently added subzone.  This holds never used space.
        //
        inline void set_active_subzone(Subzone *sz) { _active_subzone = sz; }
        

        //
        // cache_slot
        //
        // Return the cache slot a free size resides.
        inline usword_t cache_slot(usword_t size) const {
            usword_t n = quantum_count(size);
            return n < AllocationCache::cache_size ? n : 0;
        }

        
        //
        // initialize
        //
        // Set up the admin for initial use.
        //
        void initialize(Zone *zone, const usword_t quantum_log2, const usword_t layout);


        //
        // free_space()
        //
        // Sums the free lists.
        //
        usword_t free_space();

        //
        // purgeable_free_space()
        //
        // Returns how much free list space could be recovered by purge_free_space().
        //
        usword_t purgeable_free_space();
        usword_t purgeable_free_space_no_lock();
        
        //
        // purge_free_space()
        //
        // Relinquish free space pages to the system where possible.
        //
        usword_t purge_free_space();
        usword_t purge_free_space_no_lock();
        
        //
        // empty_space()
        //
        // Returns the size of the space that has yet to be allocated.
        //
        usword_t empty_space();
        
        //
        // test_freelist_integrity
        //
        // Returns true if the free list seems to be okay.
        //
        bool test_freelist_integrity();
        
        //
        // test_node_integrity
        //
        // Returns true if the free list node seems to be okay.
        //
        bool test_node_integrity(FreeListNode *node);
                        
        //
        // find_allocation
        //
        // Find the next available quanta for the allocation.  Returns NULL if none found.
        // Allocate otherwise.
        //
        void *find_allocation(Thread &thread, usword_t &size, const usword_t layout, const bool refcount_is_one, bool &is_local);
        

        //
        // The following methods are used by compaction.
        //
        
        //
        // lowest_available_list
        //
        // Searches the heads of all free lists for a node with size >= n, and returns the list with the lowest head.
        //
        FreeList *lowest_available_list(usword_t n);
        
        //
        // allocate_lower_block_no_lock
        //
        // Allocates a block of the same size and layout of the block identified by (subzone, q, block_address), at a lower heap address.
        // If no lower block can be found, returns block_address.
        //
        void *allocate_lower_block_no_lock(Subzone *subzone, usword_t q, void *block_address);

        //
        // allocate_different_block_no_lock
        //
        // Allocates a block of the same size and layout of the block identified by (subzone, q, block_address), at a different heap address.
        // If no other block can be found, returns block_address. Used by AUTO_COMPACTION_SCRAMBLE mode.
        //
        void *allocate_different_block_no_lock(Subzone *subzone, usword_t q, void *block_address);
        
        
        //
        // allocate_destination_block_no_lock
        //
        // Allocates a block of the same size and layout of the block identified by (subzone, q, block_address), at a different heap address.
        // Calls either allocate_lower_block_no_lock or allocate_different_block_no_lock, depending on the compaction mode.
        //
        void *allocate_destination_block_no_lock(Subzone *subzone, usword_t q, void *block_address);
        
        //
        // thread_cache_allocate
        //
        // If per-thread cache available, use it, otherwise fill it and use it if possible, otherwise return NULL
        //
        void *thread_cache_allocate(Thread &thread, usword_t &size, const usword_t layout, const bool refcount_is_one, bool &is_local);

        //
        // batch_allocate
        //
        // Allocate many blocks. Returns the count of blocks allocated.
        //
        unsigned batch_allocate(Thread &thread, size_t &size, const usword_t layout, const bool refcount_is_one, const bool clear, void **results, unsigned num_requested);

        //
        // deallocate
        //
        // Mark address as available.
        // Currently, this relinks it onto the free lists & clears the side table data.
        //
        void deallocate(Subzone *subzone, usword_t q, void *address);

        //
        // deallocate_no_lock
        //
        // Mark address as available.
        // Currently, this relinks it onto the free lists & clears the side table data.
        // Unlike vanilla deallocate (above), this assumes that the admin lock is already held.
        //
        void deallocate_no_lock(Subzone *subzone, usword_t q, void *address);

        //
        // mark_cached
        //
        // Set tables with information for cached allocation, one on a per-thread list
        //
        void mark_cached(Subzone *subzone, usword_t q, const usword_t n);
        void mark_cached_range(void *address, const usword_t n);
        
        // push_node
        //
        // Pushes a new node on to the specified FreeList.
        // Also tracks the range of active FreeList entries.
        //
        void push_node(usword_t index, void *address, usword_t size);

        // append_node
        //
        // Appends a new node on to the tail of the appropriate FreeList.
        // Also tracks the range of active FreeList entries.
        //
        void append_node(FreeListNode *node);
        
        //
        // insert_node
        //
        // Inserts a new node on to the specified FreeList (keeping it sorted).
        // Also tracks the range of active FreeList entries.
        //
        void insert_node(usword_t index, void *address, usword_t size);

        //
        // mark_allocated
        //
        // Set tables with information for new allocation.
        //
        void mark_allocated(void *address, const usword_t n, const usword_t layout, const bool refcount_is_one, const bool is_local);
        
        //
        // reset_free_list
        //
        // Clears all nodes off of the free lists. Nodes are simply dropped.
        //
        void reset_free_list() {
            for (usword_t i=0; i<AllocationCache::cache_size; i++) {
                _cache[i].reset();
            }
            _freelist_search_cap = 0; // indicate nothing on the free lists
        }
        
      private:
        //
        // pop_node
        //
        // Pops a node from the specified FreeList. Also
        // performs node consistency checks.
        //
        FreeListNode *pop_node(usword_t index);
        
        
        //
        // find_allocation_no_lock
        //
        // find a block of suitable size (for use on the per-thread list)
        //
        void *find_allocation_no_lock(usword_t n);
    };
        
};


#endif // __AUTO_ADMIN__
