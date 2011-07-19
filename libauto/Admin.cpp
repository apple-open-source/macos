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
    Admin.cpp
    Automatic Garbage Collection
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
*/

#include "Admin.h"
#include "Bitmap.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Zone.h"
#include "Locks.h"
#include "BlockRef.h"
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
    void Admin::initialize(Zone *zone, const usword_t quantum_log2, const usword_t layout) {
        _zone = zone;
        _quantum_log2 = quantum_log2;
        _active_subzone = NULL;
        _admin_lock = 0;
        _freelist_search_cap = 0;
        _layout = layout;
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
        
        for (usword_t m = 0; m < AllocationCache::cache_size; m++) {
            for (FreeListNode *node = _cache[m].head(); node != NULL; node = node->next()) {
                empty_space += node->size();
            }
        }
        
        return empty_space;
    }
    
    // The smallest sized block of memory to pass to uncommit_memory().
    const usword_t min_purgeable_size = Auto::page_size;
    
    //
    // visit_purgeable_nodes
    //
    // Visits all free list nodes that exceed 1 page in size, and purgeable subzones.
    //
    template <typename PurgeVisitor> void Admin::visit_purgeable_nodes(PurgeVisitor &visitor) {
        const usword_t quantum_size = (1 << _quantum_log2);
        
        // Visit medium sized free list nodes that are at > min_purgeable_size in size.
        if (is_medium()) {
            for (usword_t m = maximum_quanta, block_size = quantum_size * maximum_quanta; m > 0 && block_size > min_purgeable_size; --m, block_size -= quantum_size) {
                for (FreeListNode *node = _cache[m].head(); node != NULL; node = node->next()) {
                    if (!node->is_purged()) {
                        visitor(node);
                    }
                }
            }
        }
        
        // Visit 0th bucket nodes, which are all >= maximum_quanta * quantum_size.
        for (FreeListNode *node = _cache[0].head(); node != NULL; node = node->next()) {
            if (node->size() > min_purgeable_size && !node->is_purged()) {
                visitor(node);
            }
        }
        
        // Visit unused portions of subzones.
        for (Subzone *subzone = _purgeable_subzones; subzone != NULL; subzone = subzone->nextSubzone()) {
            if (!subzone->is_purged()) {
                visitor(subzone);
            }
        }
    }
    
    //
    // purgeable_free_space()
    //
    // Returns how much free list space could be recovered by purge_free_space().
    //
    usword_t Admin::purgeable_free_space() {
        SpinLock lock(&_admin_lock);
        return purgeable_free_space_no_lock();
    }
    
    struct PurgeableFreeSpaceVisitor {
        usword_t purgeable_bytes;
        
        PurgeableFreeSpaceVisitor() : purgeable_bytes(0) {}
        
        // Sum sizes of medium sized free list nodes that are at least min_purgeable_size in size.
        void operator() (FreeListNode *node) {
            Range r(node->purgeable_range());
            usword_t r_size = r.size();
            if (r_size >= min_purgeable_size) {
                purgeable_bytes += r_size;
            }
        }
        
        void operator() (Subzone *subzone) {
            Range r(subzone->purgeable_range());
            usword_t r_size = r.size();
            if (r_size >= min_purgeable_size) {
                purgeable_bytes += r_size;
            }
        }
    };

    usword_t Admin::purgeable_free_space_no_lock() {
        PurgeableFreeSpaceVisitor visitor;
        visit_purgeable_nodes(visitor);
        return visitor.purgeable_bytes;
    }
    
    //
    // purge_free_space()
    //
    // Relinquish free space pages to the system where possible.
    //
    usword_t Admin::purge_free_space() {
        SpinLock lock(&_admin_lock);
        return purge_free_space_no_lock();
    }
    
    struct PurgingVisitor {
        usword_t bytes_purged;
        usword_t subzone_bytes_purged;
        
        PurgingVisitor() : bytes_purged(0), subzone_bytes_purged(0) {}
        
        // Purge medium sized free list nodes that are at least min_purgeable_size in size.
        void operator() (FreeListNode *node) {
            Range r(node->purgeable_range());
            usword_t r_size = r.size();
            if (r_size >= min_purgeable_size) {
                madvise(r.address(), r_size, MADV_FREE_REUSABLE);
                node->set_purged(true);
                bytes_purged += r_size;
            }
        }
        
        // Purge unused portions of subzones.
        void operator() (Subzone *subzone) {
            Range r(subzone->purgeable_range());
            usword_t r_size = r.size();
            if (r_size >= min_purgeable_size) {
                madvise(r.address(), r_size, MADV_FREE_REUSABLE);
                subzone->set_purged(true);
                bytes_purged += r_size;
                subzone_bytes_purged += r_size;
            }
        }
    };
    
    usword_t Admin::purge_free_space_no_lock() {
        PurgingVisitor visitor;
        visit_purgeable_nodes(visitor);
        return visitor.bytes_purged;
    }
    
    // Before a free node is handed out, must make sure it is marked for reuse to the VM system.
    
    static void reuse_node(FreeListNode *node) {
        if (node->is_purged()) {
            Range r(node->purgeable_range());
            madvise(r.address(), r.size(), MADV_FREE_REUSE);
            node->set_purged(false);
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
            auto_fatal("%s", buffer);
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
        for (usword_t m = 0; m < AllocationCache::cache_size; m++) {
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
        FreeListNode *node = _cache[index].pop();
        if (node && test_node_integrity(node)) {
            // bigger nodes can be "purged"
            if (is_medium()) reuse_node(node);
            return node;
        }
        return NULL;
    }


    //
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
    // insert_node
    //
    // Inserts a new node on to the specified FreeList (keeping it sorted).
    // Also tracks the range of active FreeList entries.
    //
    inline void Admin::insert_node(usword_t index, void *address, usword_t size) {
        _cache[index].insert(address, size);
        if (index > _freelist_search_cap || _freelist_search_cap == 0)
            _freelist_search_cap = index;
    }

    //
    // append_node
    //
    // Appends a new node on to the tail of the specified FreeList.
    // Also tracks the range of active FreeList entries.
    //
    void Admin::append_node(FreeListNode *node) {
        usword_t index = cache_slot(node->size());
        _cache[index].append(node);
        if (index > _freelist_search_cap || _freelist_search_cap == 0)
            _freelist_search_cap = index;
    }

    
    //
    // mark_allocated
    //
    // Set tables with information for new allocation.
    //
    void Admin::mark_allocated(void *address, const usword_t n, const usword_t layout, const bool refcount_is_one, const bool is_local) {
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
    void Admin::mark_cached(Subzone *subzone, usword_t q, const usword_t n) {
        // mark as thread local garbage while in the cache.
        subzone->cache(q, n);
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
    
    // try to reuse a subzone from the purgeable list. only choose a subzone with enough space to make it worth reusing.
    void Admin::activate_purged_subzone() {
        ASSERTION(_active_subzone == NULL);
        Subzone *subzone = _purgeable_subzones, *prev_subzone = NULL;
        while (subzone != NULL) {
            usword_t top = subzone->allocation_count();
            usword_t unused = subzone->allocation_limit() - top;
            if (unused > AllocationCache::cache_size) {
                if (prev_subzone)
                    prev_subzone->setNextSubzone(subzone->nextSubzone());
                else
                    _purgeable_subzones = subzone->nextSubzone();
                subzone->setNextSubzone(NULL);
                subzone->set_purgeable(false);
                _active_subzone = subzone;
                if (subzone->is_purged()) {
                    Range r(subzone->purgeable_range());
                    madvise(r.address(), r.size(), MADV_FREE_REUSE);
                    subzone->set_purged(false);
                }
                break;
            }
            prev_subzone = subzone;
            subzone = subzone->nextSubzone();
        }
    }
    
    inline void *Admin::find_allocation_no_lock(usword_t n) {
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
            if (_freelist_search_cap >= n)
               _freelist_search_cap = n-1; // nothing on the free lists size n or more
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
        } else if (_active_subzone) {
            // See if we can get a free block from unused territory
            usword_t top = _active_subzone->allocation_count();
            usword_t unused = _active_subzone->allocation_limit() - top;
            
            ASSERTION(unused >= n);
            address = _active_subzone->quantum_address(top);
            *(void **)address = NULL;
            _active_subzone->raise_allocation_count(n);
                        
            // if remainder fits on non-0 free list, put it there now.  That way we're guaranteed
            // to be able to satisfy any request.
            unused -= n;
            if (unused == 0) {
                set_active_subzone(NULL);
            } else if (unused < AllocationCache::cache_size) {
                push_node(unused, _active_subzone->quantum_address(top+n), _active_subzone->quantum_size(unused));
                _active_subzone->raise_allocation_count(unused);
                set_active_subzone(NULL);
            }
            
            // try to reuse a subzone from the purgeable list. only choose a subzone with enough space to make it worth reusing.
            if (_active_subzone == NULL) {
                activate_purged_subzone();
            }
        } else {
            return NULL;
        }
                
        return address;
    }
    
    unsigned Admin::batch_allocate_from_freelist_slot_no_lock(usword_t cache_index, usword_t requested_size, const bool clear, void **results, unsigned num_requested) {
        unsigned count = 0;
        FreeListNode *node;
        assert(num_requested > 0);
        
        do {
            node = pop_node(cache_index);
            if (node) {
                void *addr = node->address();
                usword_t node_size = node->size();
                
                // calculate how many blocks we can allocate from this node
                usword_t alloc_count = node_size / requested_size;
                
                // cap alloc_count to the number requested
                if (alloc_count > num_requested) alloc_count = num_requested;
                
                // zero the allocated blocks
                usword_t alloc_size = alloc_count * requested_size;
                if (clear)
                    bzero(addr, alloc_size);
                
                // calculate the block pointers and mark them allocated
                for (usword_t i=0; i<alloc_count; i++) {
                    results[count++] = addr;
                    addr = displace(addr, requested_size);
                }
                
                // if any remainder, put it back on the appropriate free list
                if (node_size > alloc_size) {
                    usword_t remainder_size = node_size - alloc_size;
                    // figure out which cache slot it should go
                    usword_t m = cache_slot(remainder_size);
                    // push the remainder onto the free list
                    push_node(m, addr, remainder_size);
                }
                
                num_requested -= alloc_count;
            }
        } while (node && num_requested);      
        return count;
    }
    
    unsigned Admin::batch_allocate_from_subzone_no_lock(Subzone *subzone, size_t size, const bool clear, void **results, unsigned num_requested) {
        // See if we can get a blocks from unused territory
        usword_t top = subzone->allocation_count();
        usword_t unused = subzone->allocation_limit() - top;
        usword_t quantum_size = quantum_count(size);
        usword_t available = unused / quantum_size;
        unsigned count = 0;

        if (available > 0) {
            if (available > num_requested)
                available = num_requested;
            void *address = subzone->quantum_address(top);
            do {
                results[count++] = address;
                address = displace(address, size);
            } while (count < available);
            if (clear)
                bzero(results[0], count * size);
            subzone->raise_allocation_count(quantum_size*count);
            // if remainder fits on non-0 free list, put it there.
            unused -= quantum_size * count;
            if ((unused > 0) && (unused < AllocationCache::cache_size)) {
                push_node(unused, address, subzone->quantum_size(unused));
                subzone->raise_allocation_count(unused);
                unused = 0;
            }
            if (unused == 0 && subzone == _active_subzone) {
                set_active_subzone(NULL);
                activate_purged_subzone();
            } 
        }
        return count;
    }

    unsigned Admin::batch_allocate(Thread &thread, size_t &size, const usword_t layout, const bool refcount_is_one, const bool clear, void **results, unsigned num_requested) {
        // if AUTO_USE_GUARDS is on, always take the slow path.
        if (Environment::guard_pages) return 0;
        usword_t n = quantum_count(size);
        size = (n << _quantum_log2);
        SpinLock lock(&_admin_lock);
        
        unsigned count = 0;

        // we might try to reduce fragmentation by checking for exact multiple free list slots first
        // we could also try to improve locality by using larger block sizes first
        // but for now we just use the same strategy as the non-batch allocator
        for (usword_t cache_index = n; cache_index < AllocationCache::cache_size && count < num_requested; ++cache_index) {
            count += batch_allocate_from_freelist_slot_no_lock(cache_index, size, clear, &results[count], num_requested - count);
        }
        
        // if we still don't have enough, try the big chunks list
        if (count < num_requested) {
            count += batch_allocate_from_freelist_slot_no_lock(0, size, clear, &results[count], num_requested - count);
        }
        
        // try purged memory
        if (count < num_requested) {
            if (!_active_subzone)
                activate_purged_subzone();
            while (count < num_requested && _active_subzone) {
                count += batch_allocate_from_subzone_no_lock(_active_subzone, size, clear, &results[count], num_requested - count);
                if (!_active_subzone)
                    activate_purged_subzone();
            }
        }
        
        // mark all the blocks allocated, and enliven
        UnconditionalBarrier barrier(thread.needs_enlivening());
        for (usword_t i=0; i<count; i++) {
            mark_allocated(results[i], n, layout, refcount_is_one, false);
            if (barrier) SubzoneBlockRef(results[i]).enliven();
        }
        _zone->set_write_barrier_range(results, count * sizeof(void *));
        _zone->add_blocks_and_bytes(count, count * size);
        return count;
    }
    
    void *Admin::thread_cache_allocate(Thread &thread, usword_t &size, const usword_t layout, const bool refcount_is_one, bool &is_local) {
        // size is inout.  on input it is the requested size.
        usword_t n = partition2(size, allocate_quantum_small_log2);
        FreeList &list = thread.allocation_cache(layout)[n];
        if (!list.head()) {
            // per thread-cache queue is empty
            // amortize cost of admin lock by finding several
            SpinLock lock(&_admin_lock);
            usword_t node_size = (n << allocate_quantum_small_log2);
            ConditionBarrier barrier(thread.needs_enlivening());
            int alloc_count;
            for (alloc_count = 0; alloc_count < cached_list_node_initial_count; ++alloc_count) {
                void *node = find_allocation_no_lock(n);
                if (!node) break;    // ran out
                Subzone *subzone = Subzone::subzone(node);
                usword_t q = subzone->quantum_index_unchecked(node);
                mark_cached(subzone, q, n);
                // skip write-barrier since nodes are allocated refcount==1
                list.push(node, node_size);
                if (barrier) SubzoneBlockRef(subzone, q).enliven();
            }
            zone()->add_blocks_and_bytes(alloc_count, alloc_count * (1L << allocate_quantum_small_log2) * n);
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
            is_local = false;
        }
        else {
            // thread local allocation
            mark_allocated(address, n, layout, false, true);
            thread.add_local_allocation(address);
            is_local = true;
        }
        
        return address;
    }
    
    //
    // find_allocation
    //
    // Find the next available quanta for the allocation.  Returns NULL if none found.
    // Allocate otherwise.
    //
    void *Admin::find_allocation(Thread &thread, usword_t &size, const usword_t layout, const bool refcount_is_one, bool &is_local) {

        SpinLock lock(&_admin_lock);

        // determine the number of quantum we needed
        usword_t n = quantum_count(size);
        ASSERTION(n < AllocationCache::cache_size);
        void *address = find_allocation_no_lock(n);

        if (address) {
            // mark as allocated
            size = n << _quantum_log2;
            ConditionBarrier barrier(thread.needs_enlivening());
            if (refcount_is_one || !Environment::thread_collections) {
                // mark as a global (heap) object
                mark_allocated(address, n, layout, refcount_is_one, false);
                is_local = false;
            } else {
                // thread local allocation
                mark_allocated(address, n, layout, false, true);
                thread.add_local_allocation(address);
                is_local = true;
            }
            if (barrier) SubzoneBlockRef(address).enliven();
            zone()->add_blocks_and_bytes(1, (1L << _quantum_log2) * n);
        }
        return address;
    }
    
    //
    // lowest_available_list
    //
    // Searches the heads of all free lists for a node with size >= n, and returns the list with the lowest head.
    //
    FreeList *Admin::lowest_available_list(usword_t n) {
        // start with bucket 0, which should always contain some free space.
        FreeList *candidate_list = &_cache[0];
        FreeListNode *candidate_node = candidate_list->head();
        
        for (usword_t i = n; i <= _freelist_search_cap; i++) {
            FreeListNode *node = _cache[i].head();
            if (node != NULL && (candidate_node == NULL || node->address() < candidate_node->address())) {
                candidate_list = &_cache[i];
                candidate_node = node;
            }
        }
        
        return candidate_list;
    }
    
    //
    // allocate_lower_block_no_lock
    //
    // Allocates a block of the same size and layout of the block identified by (subzone, q, block_address), at a lower heap address.
    // If no lower block can be found, returns block_address.
    //
    void *Admin::allocate_lower_block_no_lock(Subzone *subzone, usword_t q, void *block_address) {
        // determine the number of quantum we needed
        const usword_t size = subzone->size(q);
        const unsigned layout = subzone->layout(q);
        usword_t n = quantum_count(size);
        ASSERTION(n < AllocationCache::cache_size);
        
        // check to see if we'll be able to lower the block's address.
        FreeList *list = lowest_available_list(n);
        FreeListNode *node = list->head();
        if (node == NULL || node->address() > block_address) {
            return block_address;
        }
        if (is_medium()) reuse_node(node);
        list->pop();
        void *address = node->address();

        // see if there is any room left over in this node.
        ASSERTION(node->size() >= size);
        usword_t remainder_size = node->size() - size;
        if (remainder_size) {
            // determine the address of the remainder
            void *remainder_address = displace(address, size);
            // figure out which cache slot it should go
            usword_t m = cache_slot(remainder_size);
            // insert the remainder onto the free list
            insert_node(m, remainder_address, remainder_size);
        }
        
        // mark as allocated.
        Subzone *copySubzone = Subzone::subzone(address);
        usword_t copyQ = copySubzone->quantum_index_unchecked(address);
        copySubzone->allocate(copyQ, n, layout, false, false);
        
        return address;
    }

    //
    // allocate_different_block_no_lock
    //
    // Allocates a block of the same size and layout of the block identified by (subzone, q, block_address), at a different heap address.
    // If no other block can be found, returns block_address. Used by AUTO_COMPACTION_SCRAMBLE mode.
    //
    void *Admin::allocate_different_block_no_lock(Subzone *subzone, usword_t q, void *block_address) {
        // determine the number of quantum we needed
        const usword_t size = subzone->size(q);
        const unsigned layout = subzone->layout(q);
        usword_t n = quantum_count(size);
        ASSERTION(n < AllocationCache::cache_size);
        void *address = find_allocation_no_lock(n);

        if (address) {
            // mark as allocated
            Subzone *copySubzone = Subzone::subzone(address);
            usword_t copyQ = copySubzone->quantum_index_unchecked(address);
            copySubzone->allocate(copyQ, n, layout, false, false);

            return address;
        }
        
        // couldn't find a block to allocate, simply return the original block address.
        return block_address;
    }
    
    //
    // allocate_destination_block_no_lock
    //
    // Allocates a block of the same size and layout of the block identified by (subzone, q, block_address), at a different heap address.
    // Calls either allocate_lower_block_no_lock or allocate_different_block_no_lock, depending on the compaction mode.
    //
    void *Admin::allocate_destination_block_no_lock(Subzone *subzone, usword_t q, void *block_address) {
        static void *(Admin::*block_allocator)(Subzone *subzone, usword_t q, void *block_address) = (Environment::scramble_heap ? &Admin::allocate_different_block_no_lock : &Admin::allocate_lower_block_no_lock);
        return (this->*block_allocator)(subzone, q, block_address);
    }

    //
    // deallocate
    //
    // Clear tables of information after deallocation.
    //
    void Admin::deallocate(Subzone *subzone, usword_t q, void *address) {
        SpinLock lock(&_admin_lock);
        deallocate_no_lock(subzone, q, address);
    }

    //
    // deallocate_no_lock
    //
    // Clear tables of information after deallocation.  Assumes _admin_lock held.
    //
    void Admin::deallocate_no_lock(Subzone *subzone, usword_t q, void *address) {
        AUTO_PROBE(auto_probe_admin_deallocate(address));
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
                    // before coalescing, mark node as potentially reused.
                    if (is_medium()) reuse_node(next_node);
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
                    // before coalescing, mark node as potentially reused.
                    if (is_medium()) reuse_node(prev_node);
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
        
        // If the block we're freeing is at the end of the subzone, lower the allocation count and add the
        // subzone to a list of subzones known to have free space at the end. The subzones are reused if the
        // active subzone is exhausted, and there is enough free space to satisfy any requested size.
        // When the system indicates there is memory pressure, purge_free_space() is called which checks to
        // see if there are zones in the purgeable list.
        
        if (next_q == highwater) {
            subzone->lower_allocation_count(quantum_count(free_size));
            if (subzone != _active_subzone && !subzone->is_purgeable()) {
                // make this subzone eligible for purging in purge_free_space().
                subzone->setNextSubzone(_purgeable_subzones);
                _purgeable_subzones = subzone;
                subzone->set_purgeable(true);
            }
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
