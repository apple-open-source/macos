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
    AutoAdmin.cpp
    Automatic Garbage Collection
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
*/

#include "AutoAdmin.h"
#include "AutoBitmap.h"
#include "AutoConfiguration.h"
#include "AutoDefs.h"
#include "AutoZone.h"
#include "AutoLock.h"
#include <stdio.h>
#include <sys/mman.h>

namespace Auto {

    //----- Admin -----//
        
        
    //
    // initialize
    //
    // Set up the admin for initial use.  Provided the data area used for the management tables, the quantum used
    // in the area managed, whether the tables are growable and whether it grows from the other end of the data.
    //
    void Admin::initialize(Zone *zone, const usword_t quantum_log2) {
        _zone = zone;
        _quantum_log2 = quantum_log2;
        _active_subzone = NULL;
        bzero(&_cache[0], sizeof(_cache));
        _admin_lock = 0;
        _freelist_search_cap = 0;
    }


    //
    // unused_count
    //
    // Quanta not on free list (anymore).  We shrink the heap when we can & let
    // allocations battle it out on the free lists first.
    //
    usword_t Admin::unused_count() {
        return _active_subzone->allocation_limit() - _active_subzone->allocation_count();
    }



    //
    // free_space()
    //
    // Sums the free lists.
    //
    usword_t Admin::free_space() {
        SpinLock lock(&_admin_lock);
        usword_t empty_space = 0;
        
        for (usword_t m = 0; m < cache_size; m++) {
            for (FreeListNode *node = _cache[m].head(); node != NULL; node = node->next()) {
                empty_space += node->size();
            }
        }
        
        return empty_space;
    }
    
    //
    // purge_free_space()
    //
    // Relinquish free space pages to the system where possible.
    //
    void Admin::purge_free_space() {
        SpinLock lock(&_admin_lock);
        
        const usword_t min_purgeable_size = 2 * Auto::page_size;
        const usword_t quantum_size = (1 << _quantum_log2);
        
        // loop through all the blocks that are at least 2 pages, to justify the cost of calling madvise().
        for (usword_t m = maximum_quanta, block_size = quantum_size * maximum_quanta; m > 0 && block_size > min_purgeable_size; --m, block_size -= quantum_size) {
            for (FreeListNode *node = _cache[m].head(); node != NULL; node = node->next()) {
                if (!node->purged()) {
                    Range r(node->purgeable_range());
                    if (r.size() >= min_purgeable_size) {
                        madvise(r.address(), r.size(), MADV_FREE);
                    }
                    node->set_purged();     // mark the block purged, even if it was too small to purge, to avoid checking again.
                }
            }
        }
        
        // Purge 0th bucket nodes, which are all >= maximum_quanta * quantum_size.
        for (FreeListNode *node = _cache[0].head(); node != NULL; node = node->next()) {
            if (node->size() > min_purgeable_size && !node->purged()) {
                Range r(node->purgeable_range());
                if (r.size() >= min_purgeable_size) {
                    madvise(r.address(), r.size(), MADV_FREE);
                }
                node->set_purged();         // mark the block purged, even if it was too small to purge, to avoid checking again.
            }
        }
    }
    
    //
    // empty_space()
    //
    // Returns the size of the holes.
    //
    usword_t Admin::empty_space() {
        SpinLock lock(&_admin_lock);
        usword_t empty_space = 0;
        
        // iterate through each free list
        for (FreeListNode *node = _cache[0].head(); node != NULL; node = node->next()) {
            empty_space += node->size();
        }
        
        return empty_space;
    }

#if DEBUG    
    bool Admin::test_node_integrity(FreeListNode *node) {
        bool node_is_valid = false;
        const Range &coverage = _zone->coverage();
        do {
            // make sure the node is a plausible address.
            if (!coverage.in_range(node)) break;
            
            Subzone *subzone = Subzone::subzone((void *)node);
            
            // get quantum number
            usword_t q = subzone->quantum_index(node->address());
            
            // make sure quantum number is in range
            if (q >= subzone->allocation_limit()) break;
            
            // make sure that address is exact quantum
            if (subzone->quantum_address(q) != node->address()) break;
            
            // make sure it is free
            if (!subzone->is_free(q)) break;
            
            // check plausibility of next and previous pointers.
            FreeListNode *next = node->next();
            if (next && !coverage.in_range(next)) break;
            FreeListNode *prev = node->prev();
            if (prev && !coverage.in_range(prev)) break;
            
            // make sure of size redundancy
            if (node->size() != node->size_again()) break;
        
            node_is_valid = true;
        } while (0);
        
        if (!node_is_valid) {
            static char buffer[256];
            if (coverage.in_range(node)) {
                snprintf(buffer, sizeof(buffer), "test_node_integrity:  FreeListNode %p { _prev = %p, _next = %p, _size = %lu } failed integrity check.\n",
                         node, node->prev(), node->next(), node->size());
            } else {
                snprintf(buffer, sizeof(buffer), "test_node_integrity:  FreeListNode %p failed integrity check.\n", node);
            }
            __crashreporter_info__ = buffer;
            malloc_printf("%s", buffer);
            __builtin_trap();
        }
        
        return node_is_valid;
    }
#else
    bool Admin::test_node_integrity(FreeListNode *node) { return true; }
#endif
    
    //
    // test_freelist_integrity
    //
    // Returns true if the free list seems to me okay.
    //
    bool Admin::test_freelist_integrity() {
        SpinLock lock(&_admin_lock);
        
        // iterate through each free list
        for (usword_t m = 0; m < cache_size; m++) {
            // iterate through each free list
            for (FreeListNode *node = _cache[m].head(), *prev_node = NULL; node; node = node->next()) {
                Subzone *subzone = Subzone::subzone((void *)node);
                
                // get quantum number
                usword_t q = subzone->quantum_index(node->address());
                
                // make sure quantum number is in range
                if (q >= subzone->allocation_limit()) return false;
                
                // make sure that address is exact quantum
                if (subzone->quantum_address(q) != node->address()) return false;
                
                // make sure it is free
                if (subzone->is_used(q)) return false;
                
                // make sure the previous pointer is accurate
                if (node->prev() != prev_node) return false;
                
                // make sure of size redundancy
                if (node->size() != node->size_again()) return false;
                
                // update previous for next round
                prev_node = node;
            }
        }
        
        return true;
    }


    //
    // pop_node
    //
    // Pops a node from the specified FreeList. Also
    // performs node consistency checks.
    //
    inline FreeListNode *Admin::pop_node(usword_t index) {
        FreeListNode *head = _cache[index].head();
        return (head && test_node_integrity(head) ? _cache[index].pop() : NULL);
    }


    // push_node
    //
    // Pushes a new node on to the specified FreeList.
    // Also tracks the range of active FreeList entries.
    //
    inline void Admin::push_node(usword_t index, void *address, usword_t size) {
        _cache[index].push(address, size);
        if (index > _freelist_search_cap || _freelist_search_cap == 0)
            _freelist_search_cap = index;
    }

    
    //
    // mark_allocated
    //
    // Set tables with information for new allocation.
    //
    void Admin::mark_allocated(void *address, const usword_t n, const unsigned layout, const bool refcount_is_one, const bool is_local) {
        Subzone *subzone = Subzone::subzone(address);
        // always ZERO the first word before marking an object as allocated, to avoid a race with the scanner.
        // TODO:  consider doing the bzero here, to keep the scanner from seeing stale pointers altogether.
        // TODO:  for the medium admin, might want to release the lock during block clearing, and reaquiring
        // before allocation.
        // XXX Perhaps only by virtue of Intel's total memory order doees this work because
        // the scanner/collector thread might have stale just-freed-link data in its L2/L3 cache
        // and might see the new admin byte and the old data, e.g. this NULL isn't really guaranteed
        // to go across to other processors.
        *(void **)address = NULL;
        subzone->allocate(subzone->quantum_index(address), n, layout, refcount_is_one, is_local);
    }
    
    //
    // mark_cached
    //
    // Set tables with information for new allocation.
    //
    void Admin::mark_cached(void *address, const usword_t n) {
        Subzone *subzone = Subzone::subzone(address);
        // mark as thread local garbage while in the cache.
        subzone->cache(subzone->quantum_index(address), n);
    }
    
    void Admin::mark_cached_range(void *address, usword_t n) {
        Subzone *subzone = Subzone::subzone(address);
        const usword_t maximum_block_size = maximum_quanta << _quantum_log2;
        while (n >= maximum_quanta) {
            subzone->cache(subzone->quantum_index(address), maximum_quanta);
            address = displace(address, maximum_block_size);
            n -= maximum_quanta;
        }
        if (n) subzone->cache(subzone->quantum_index(address), n);
    }
    
    inline void *Admin::find_allocation_no_lock(usword_t n, bool &did_grow) {
        // Fast case, exact fit from the free list. the free list will contain guarded blocks.
        FreeListNode *node = pop_node(n);
        if (node) {
            return node->address();
        }

        if (Environment::guard_pages) {
            if (_active_subzone == NULL) return NULL;

            // allocate directly from the subzone, since there will be no meaningful coalescing.
            const usword_t quantum_size = (1 << _quantum_log2);
            Subzone *subzone = _active_subzone;
            usword_t first_quantum = subzone->allocation_count(), last_quantum = subzone->allocation_limit();
            usword_t available_size = (last_quantum - first_quantum) * quantum_size;

            void *slop_address = subzone->quantum_address(first_quantum);
            void *guard_address = align_up(slop_address);
            usword_t block_size = n << _quantum_log2;
            usword_t slop_size = (uintptr_t)guard_address - (uintptr_t)slop_address;
            while (block_size > slop_size) {
                guard_address = displace(guard_address, page_size);
                slop_size += page_size;
            }
            void *end_address = displace(guard_address, page_size);
            usword_t total_size = (uintptr_t)end_address - (uintptr_t)slop_address;
            // make sure this allocation will actually fit.
            if (available_size < total_size) {
                // this subzone is "full", add another.
                // TODO:  look at free list again, and steal slop away from free blocks. NMOS.
                set_active_subzone(NULL);
                return NULL;
            }
            // account for the number of quanta we're allocating.
            subzone->raise_allocation_count(total_size >> _quantum_log2);
            // permanently allocate the slop (1 quantum at a time for simplicity FIXME later).
            usword_t slop_count = ((slop_size - block_size) >> _quantum_log2);
            if (slop_count) mark_cached_range(slop_address, slop_count);
            // protect the guard page.
            guard_page(guard_address);
            // also cache the guard page itself.
            mark_cached_range(guard_address, (page_size >> _quantum_log2));
            // check to see if there's still enough space in the subzone.
            usword_t remainder_size = available_size - total_size;
            if (remainder_size < (2 * page_size)) {
                // we need another subzone.
                set_active_subzone(NULL);
            }
            return displace(guard_address, -block_size);
        }
        
        // Find bigger block to use, then chop off remainder as appropriate
        void *address = NULL;
        
        // if no block, iterate up through sizes greater than n (best fit)
        for (usword_t i = n + 1; node == NULL && i <= _freelist_search_cap; i++) {
            node = pop_node(i);
        }

        // Grab a free block from the big chunk free list
        if (!node) {
            node = pop_node(0);
            _freelist_search_cap = 0; // indicate nothing on the free lists
        }

        if (node) {
            // Got one.  Now return extra to free list.
            
            // get the address of the free block
            address = node->address();

            // get the full size of the allocation
            Subzone *subzone = Subzone::subzone(address);
            usword_t allocation_size = subzone->quantum_size(n);
            
            // see what's left over
            ASSERTION(node->size() >= allocation_size);
            usword_t remainder_size = node->size() - allocation_size;
            
            // if there is some left over
            if (remainder_size) {
                // determine the address of the remainder
                void *remainder_address = displace(address, allocation_size);
                // figure out which cache slot it should go
                usword_t m = cache_slot(remainder_size);
                // push the remainder onto the free list
                push_node(m, remainder_address, remainder_size);
            }
        }
        else if (_active_subzone) {
            // See if we can get a free block from unused territory
            // mark did_grow so that the will_grow notice will go out after we release the admin lock
            did_grow = true;
            
            usword_t top = _active_subzone->allocation_count();
            usword_t unused = _active_subzone->allocation_limit() - top;
            
            ASSERTION(unused >= n);
            address = _active_subzone->quantum_address(top);
            *(void **)address = NULL;
            _active_subzone->raise_allocation_count(n);
            _zone->statistics().add_dirty(_active_subzone->quantum_size(n));   // track total committed
                        
            // if remainder fits on non-0 free list, put it there now.  That way we're guaranteed
            // to be able to satisfy any request.
            unused -= n;
            if (unused == 0) {
                set_active_subzone(NULL);
            }
            else if (unused < cache_size) {
                push_node(unused, _active_subzone->quantum_address(top+n), _active_subzone->quantum_size(unused));
                _active_subzone->raise_allocation_count(unused);
                set_active_subzone(NULL);
            }
        }
        else {
            return NULL;
        }
                
        return address;
    }
    
    void *Admin::thread_cache_allocate(Thread &thread, usword_t &size, const unsigned layout, const bool refcount_is_one, bool &did_grow) {
        // size is inout.  on input it is the requested size.
        usword_t n = partition2(size, allocate_quantum_small_log2);
        FreeList &list = thread.allocation_cache(n);
        if (!list.head()) {
            // per thread-cache queue is empty
            // amortize cost of admin lock by finding several
            SpinLock lock(&_admin_lock);
            usword_t node_size = (n << allocate_quantum_small_log2);
            EnliveningHelper<ConditionBarrier> barrier(thread);
            for (int i = 0; i < cached_list_node_initial_count; ++i) {
                void *node = find_allocation_no_lock(n, did_grow);
                if (!node) break;    // ran out
                mark_cached(node, n);
                // skip write-barrier since nodes are allocated refcount==1
                list.push(node, node_size);
                if (barrier) barrier.enliven_block(node);
            }
        }
        // grab first node off the cache "line"
        void *address = list.pop()->address();
        if (!address) return NULL;
        size = n << allocate_quantum_small_log2;   // return actual size (out param)
        // mark with requested layout and refcount
        // XXX only have to write the first byte - size is already OKAY
        if (refcount_is_one || !Environment::thread_collections) {
            // mark as a global (heap) object
            mark_allocated(address, n, layout, refcount_is_one, false);
        }
        else {
            // thread local allocation
            mark_allocated(address, n, layout, false, true);
            thread.add_local_allocation(address);
        }
        
        return address;
    }
    
    
    //
    // find_allocation
    //
    // Find the next available quanta for the allocation.  Returns NULL if none found.
    // Allocate otherwise.
    //
    void *Admin::find_allocation(Thread &thread, usword_t &size, const unsigned layout, const bool refcount_is_one, bool &did_grow) {

        SpinLock lock(&_admin_lock);

        // determine the number of quantum we needed
        usword_t n = quantum_count(size);
        ASSERTION(n < cache_size);
        void *address = find_allocation_no_lock(n, did_grow);

        if (address) {
            // mark as allocated
            size = n << _quantum_log2;
            EnliveningHelper<ConditionBarrier> barrier(thread);
            mark_allocated(address, n, layout, refcount_is_one, false);
            if (barrier) barrier.enliven_block(address);
        }
        return address;
    }
    

    //
    // deallocate
    //
    // Clear tables of information after deallocation.
    //
    void Admin::deallocate(void *address) {
        SpinLock lock(&_admin_lock);
        deallocate_no_lock(address);
    }

    //
    // deallocate_no_lock
    //
    // Clear tables of information after deallocation.  Assumes _admin_lock held.
    //
    void Admin::deallocate_no_lock(void *address) {
        
        Subzone *subzone = Subzone::subzone(address);
        usword_t q = subzone->quantum_index(address);
        usword_t n = subzone->length(q);

        // detect double-frees.
        ASSERTION(!subzone->is_free(q));
        if (subzone->is_free(q)) {
            malloc_printf("Admin::deallocate:  attempting to free already freed block %p\n", address);
            return;
        }

        // assume that just this block is free
        void *free_address = address;
        usword_t free_size = subzone->quantum_size(n);

        // adjust statistics now before coalescing
        Statistics &zone_stats = _zone->statistics();
        zone_stats.add_count(-1);
        zone_stats.add_size(-free_size);

        // coalescing seems detrimental to deallocation time, but it improves memory utilization.
        // determine next block
        usword_t next_q = q + n;
        usword_t highwater = subzone->allocation_count();

        // don't bother with coalescing when using guard pages.
        if (!Environment::guard_pages) {
            // if not past end of in use bits and the quantum is not in use
            if (next_q < highwater && subzone->is_free(next_q)) {
                // get the free block following in memory
                FreeListNode *next_node = (FreeListNode *)displace(free_address, free_size);
                if (test_node_integrity(next_node)) {
                    // determine it's size
                    usword_t next_size = next_node->size();
                    // which cache slot is it in
                    usword_t m = cache_slot(next_size);
                    // remove it from the free list
                    _cache[m].remove(next_node);
                    // add space to current free block
                    free_size += next_size;
                }
            }
            
            // check to see if prior quantum is free
            if (q && subzone->is_free(q - 1)) {
                // determine the prior free node
                FreeListNode *this_node = (FreeListNode *)address;
                FreeListNode *prev_node = this_node->prior_node();
                if (test_node_integrity(prev_node)) {
                    // update the current free address to use the prior node address
                    free_address = prev_node->address();
                    // get the prior's size
                    usword_t prev_size = prev_node->size();
                    // add space to current free block
                    free_size += prev_size;
                    // which cache slot is the prior free block in
                    usword_t m = cache_slot(prev_size);
                     // remove it from the free list
                    _cache[m].remove(prev_node);
                }
            }
        }
        
        // scribble on blocks as they are deleted.
        if (Environment::dirty_all_deleted) {
            memset(free_address, 0x55, free_size);
        }
        
        // We can reclaim the entire active subzone space but not any other.  Only the active subzone
        // has an allocation count less than the limit.  If we did lower the per-subzone in_use count, then
        // to find and allocate it we would have to linearly search all the subzones after a failed pop(0).
        // On the other hand, this is a good way to create multi-page free blocks and keep them colder after
        // a peak memory use incident - at least for medium sized subzones.  Revisit this when compaction is
        // explored.
        // What we would need would be a list of subzones with space available.  Not so hard to maintain.
        
        if (next_q == highwater && highwater < subzone->allocation_limit()) {
            subzone->lower_allocation_count(quantum_count(free_size));
            _zone->statistics().add_dirty(-free_size);       // track total committed
        } else {
            // determine which free list the free space should go upon
            usword_t m = cache_slot(free_size);
            // add free space to free lists
            push_node(m, free_address, free_size);
        }

        // clear side data
        subzone->deallocate(q, n);
    }

};
