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
    AutoAdmin.h
    Automatic Garbage Collection
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_ADMIN__
#define __AUTO_ADMIN__

#include "AutoBitmap.h"
#include "AutoConfiguration.h"
#include "AutoDefs.h"
#include "AutoFreeList.h"
#include "AutoRange.h"
#include "AutoStatistics.h"
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
    
        enum {
            cache_size = maximum_quanta + 1                 // size of the free list cache 
        };
    
      private:
      
        Zone           *_zone;                              // managing zone
        usword_t       _quantum_log2;                       // ilog2 of the quantum used in this admin
        FreeList       _cache[cache_size];                  // free lists, one for each quanta size, slot 0 is for large clumps
        Subzone        *_active_subzone;                    // subzone with unused memory
        spin_lock_t    _admin_lock;                         // protects free list, subzone data.
        usword_t       _freelist_search_cap;                // highest nonempty freelist index (excluding big chunk list), or 0 if all are empty
        
      public:
      
        //
        // Accessors
        //
        Zone *zone()            const { return _zone; }
        usword_t quantum_log2() const { return _quantum_log2; }
        spin_lock_t *lock()           { return &_admin_lock; }
      

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
            return n < cache_size ? n : 0;
        }

        
        //
        // initialize
        //
        // Set up the admin for initial use.
        //
        void initialize(Zone *zone, const usword_t quantum_log2);


        //
        // free_space()
        //
        // Sums the free lists.
        //
        usword_t free_space();
        
        //
        // purge_free_space()
        //
        // Relinquish free space pages to the system where possible.
        //
        void purge_free_space();
        
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
        void *find_allocation(Thread &thread, usword_t &size, const unsigned layout, const bool refcount_is_one, bool &did_grow);
                        
        //
        // thread_cache_allocate
        //
        // If per-thread cache available, use it, otherwise fill it and use it if possible, otherwise return NULL
        //
        void *thread_cache_allocate(Thread &thread, usword_t &size, const unsigned layout, const bool refcount_is_one, bool &did_grow);

        //
        // deallocate
        //
        // Mark address as available.
        // Currently, this relinks it onto the free lists & clears the side table data.
        //
        void deallocate(void *address);

        //
        // deallocate_no_lock
        //
        // Mark address as available.
        // Currently, this relinks it onto the free lists & clears the side table data.
        // Unlike vanilla deallocate (above), this assumes that the admin lock is already held.
        //
        void deallocate_no_lock(void *address);

        //
        // mark_cached
        //
        // Set tables with information for cached allocation, one on a per-thread list
        //
        void mark_cached(void *address, const usword_t n);
        void mark_cached_range(void *address, const usword_t n);
        
        // push_node
        //
        // Pushes a new node on to the specified FreeList.
        // Also tracks the range of active FreeList entries.
        //
        void push_node(usword_t index, void *address, usword_t size);

        //
        // mark_allocated
        //
        // Set tables with information for new allocation.
        //
        void mark_allocated(void *address, const usword_t n, const unsigned layout, const bool refcount_is_one, const bool is_local);
        
                
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
        void *find_allocation_no_lock(usword_t n, bool &did_grow);
    };
        
};


#endif // __AUTO_ADMIN__
