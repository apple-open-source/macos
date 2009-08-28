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
    AutoSubzone.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_SUBZONE__
#define __AUTO_SUBZONE__

#include "AutoAdmin.h"
#include "AutoDefs.h"
#include "AutoBitmap.h"
#include "AutoFreeList.h"
#include "AutoWriteBarrier.h"
#include "AutoRegion.h"
#import "auto_tester/auto_tester.h"
#include "auto_zone.h"
#include <cassert>

namespace Auto {

    // Forward declarations
    class Region;
    
    
    //----- Subzone -----//
    
    //
    // A subzone is a region in vm memory managed by automatic garbage collection.  The base address of the subheap is
    // aligned to the subzone quantum size such that the containing subzone can be quickly determined from any refererence 
    // into the subzone.
    // A C++ Subzone object is constructed at this aligned address.  The first chunk of memory are the write-barrier cards
    // that keep track of write-barrier-quantum ranges of objects that have been stored into.
    // Next are the instance variables including a back pointer to the "admin" that contains the free lists.
    // Fleshing out the rest are the "admin" data, one per quantum, indicating if its the start of a block etc.
    // Quantum numbers are used for most operations - the object with quantum 0 starts just after the end of the admin data
    // at the first quantum boundary opportunity.
    // There are two quantum sizes that a Subzone can be configured to manage - small and medium.  We keep a "bias" so that
    // in a bitmap of all subzones we can quickly keep it as dense as possible.
    //
    
    
    class Subzone : public Preallocated {

    private:
        unsigned char  _write_barrier_cards[subzone_write_barrier_max];
        WriteBarrier   _write_barrier;
                                        // write barrier for subzone - must be first
        usword_t       _quantum_log2;   // ilog2 of the quantum used in this admin
        Region         *_region;        // region owning this subzone (with bitmaps for these quanta)
        Admin          *_admin;         // admin for this subzone (reflecting appropriate quanta size)
        usword_t       _quantum_bias;   // the value added to subzone quantum numbers to get a globally
                                        // unique quantum (used to index region pending/mark bits)
        void           *_allocation_address; // base address for allocations
        usword_t       _in_use;         // high water mark
        unsigned char  _side_data[1];   // base for side data

        enum {
            /*
             A quantum with start_bit set indicates the beginning of a block, which may be either allocated or free.
             The block is allocated if any of the remaining bits are nonzero. If all are zero then the block is free.
             Minimally, if a block is allocated either global_bit, or alloc_local_bit, or the garbage bit will be set.
             The block size for an allocated block beginning at quanta q is determined by examining the side data at quanta q+1. 
             If the start bit at q+1 is set then the block size is 1 quanta. If the start bit at q+1 is not set then the remaining
             bits hold the block size (in units of quanta.)
             The block size for a unallocated block can be inferred from the free list it is on.
             */
             
            start_bit_log2 = 7,
            start_bit = 0x1 << start_bit_log2,                  // indicates beginning of block

            /*
             The layout indicates whether the block is an object and whether it is scanned.  Even when a block is marked
             as garbage the scanned bit is important because the background collector needs to scan through even local garbage
             to find references - otherwise it might win a race and deallocate something that will be referenced in a finalize
             run by the local collector.
            */
            layout_log2 = 5,
            layout_mask = 0x3 << layout_log2,                   // indicates block organization
            
            /*
             The global bit is consulted in order to determine the interpretation of all remaining bits.
             When global bit == 1, the age_mask and refcount_mask bits are valid.
             When global bit == 0, alloc_local_bit and garbage_bit are valid.
             When garbage_bit == 1, refcount_mask is again valid since blocks may be marked external in a finalize method.
             When garbage_bit == 0, scan_local_bit is valid.
            */
            global_bit_log2 = 0,
            global_bit = 0x1 << global_bit_log2,                // indicates thread locality of block. 0 -> thread local, 1 -> global

            garbage_bit_log2 = 2,
            garbage_bit = 0x1 << garbage_bit_log2,              // iff global == 0.  marks garbage, alloc_local_bit marks if local.
            
            alloc_local_bit_log2 = 1,
            alloc_local_bit = 0x1 << alloc_local_bit_log2,      // iff global == 0. marks that a block is thread local
                        
            scan_local_bit_log2 = 3,
            scan_local_bit = 0x1 << scan_local_bit_log2,        // iff global == 0, alloc_local == 1. marks thread local to be scanned
            
            refcount_log2 = 3,
            refcount_mask = 0x3 << refcount_log2,               // if global_bit == 1 else garbage_bit == 1. holds inline refcount
            
            age_mask_log2 = 1,
            age_mask = 0x3 << age_mask_log2,                    // iff global_bit == 1. block age bits for background collector
            
            /* Interesting age values. */
            youngest_age = 3,
            eldest_age = 0,
                        
            quanta_size_mask = 0x7f,                            // quanta size mask for blocks of quanta size 2+
        };
        
        // Does a side data value represent the youngest age (includes thread local)? 
        static inline bool is_youngest(unsigned char sd) { return ((sd & age_mask)>>age_mask_log2) == youngest_age; }
        
        // Does a side data value represent the eldest age?
        static inline bool is_eldest(unsigned char sd) { return ((sd & age_mask)>>age_mask_log2) == eldest_age; }

        //
        // subzone_side_data_max
        //
        // Given a constant quantum_log2 returns a constant (optimized code) defining
        // the maximum number of quantum in the subzone.
        //
        static inline usword_t subzone_side_data_max(usword_t quantum_log2) {
            // size of subzone data (non quantum) less size of side data
            usword_t header_size = sizeof(Subzone) - sizeof(unsigned char);
            // quantum size plus one byte for side data
            usword_t bytes_per_quantum = (1LL << quantum_log2) + 1;
            // round up the maximum number quantum (max out side data)
            return (subzone_quantum - header_size + bytes_per_quantum - 1) / bytes_per_quantum;
        }
        

        //
        // subzone_base_data_size
        //
        // Given a constant quantum_log2 returns a constant (optimized code) defining
        // the size of the non quantum data rounded up to a the nearest quantum.
        //
        static inline usword_t subzone_base_data_size(usword_t quantum_log2) {
            return align2(sizeof(Subzone) - sizeof(unsigned char) + subzone_side_data_max(quantum_log2), quantum_log2);
        }
       

        //
        // subzone_allocation_size
        //
        // Given a constant quantum_log2 returns a constant (optimized code) defining
        // the size of the area available for allocating quantum.
        //
        static inline usword_t subzone_allocation_size(usword_t quantum_log2) {
            return subzone_quantum - subzone_base_data_size(quantum_log2);
        }
       

        //
        // subzone_allocation_limit
        //
        // Given a constant quantum_log2 returns a constant (optimized code) defining
        // the number of the quantum that can be allocated.
        //
        static inline usword_t subzone_allocation_limit(usword_t quantum_log2) {
            return partition2(subzone_allocation_size(quantum_log2), quantum_log2);
        }
        
        
      public:
      
      
        //
        // Constructor
        //
        Subzone(Region *region, Admin *admin, usword_t quantum_log2, usword_t quantum_bias)
            : _write_barrier(_write_barrier_cards, _write_barrier_cards, WriteBarrier::bytes_needed(subzone_quantum)),
              _quantum_log2(quantum_log2), _region(region), _admin(admin), _quantum_bias(quantum_bias), _allocation_address(NULL), _in_use(0)
        {
            usword_t base_data_size = is_small() ?
                                        subzone_base_data_size(allocate_quantum_small_log2) :
                                        subzone_base_data_size(allocate_quantum_medium_log2);
            _allocation_address = (void *)displace(this, base_data_size);
        }
        
      
        //
        // Accessors
        //
        usword_t quantum_log2()                const { return _quantum_log2; }
        Region *region()                       const { return _region; }
        Admin *admin()                         const { return _admin; }
        usword_t quantum_bias()                const { return _quantum_bias; }
        
        
        //
        // subzone
        //
        // Return the subzone of an arbitrary memory address.
        //
        static inline Subzone *subzone(void *address) { return (Subzone *)((uintptr_t)address & ~mask(subzone_quantum_log2)); }


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
        // allocation_address
        //
        // Return the first allocation address in the subzone.
        //
        inline void *allocation_address() const { return _allocation_address; }
        

        //
        // allocation_end
        //
        // Return the last allocation address in the subzone.
        //
        inline void *allocation_end() { return displace(this, subzone_quantum); }
        
        
        //
        // base_data_size
        //
        // Return the size of the base data space in the subzone.
        //
        inline usword_t base_data_size() const {
             return is_small() ? subzone_base_data_size(allocate_quantum_small_log2):
                                 subzone_base_data_size(allocate_quantum_medium_log2);
        }


        //
        // base_data_quantum_count
        //
        // Return the number quantum of the base data space occupies.
        //
        inline usword_t base_data_quantum_count(usword_t quantum_log2) const {
             return subzone_base_data_size(quantum_log2) >> quantum_log2;
        }


        //
        // allocation_size
        //
        // Return the size of the allocation space in the subzone.
        //
        inline usword_t allocation_size() const {
             return is_small() ? subzone_allocation_size(allocate_quantum_small_log2) :
                                 subzone_allocation_size(allocate_quantum_medium_log2);
        }


        //
        // allocation_limit
        //
        // Return the number quantum in the subzone.
        //
        inline usword_t allocation_limit() const {
             return is_small() ? subzone_allocation_limit(allocate_quantum_small_log2) :
                                 subzone_allocation_limit(allocate_quantum_medium_log2);
        }
        
        
        //
        // quantum_index
        //
        // Returns a quantum index for a arbitrary pointer.
        // Unless running DEBUG this could be bogus if the pointer refers to the admin (write-barrier) area of a subzone.
        // Callers must have already done a successful is_start or be prepared to validate against quantum_limit.
        //
        inline usword_t quantum_index(void *address, usword_t quantum_log2) const {
            usword_t result = (((uintptr_t)address & mask(subzone_quantum_log2)) >> quantum_log2) - base_data_quantum_count(quantum_log2);
#if DEBUG
            if (result > allocation_limit()) { printf("bad quantum index\n"); __builtin_trap(); }
#endif
            return result;
        }
        inline usword_t quantum_index(void *address) const {
            return is_small() ? quantum_index(address, allocate_quantum_small_log2) :
                                quantum_index(address, allocate_quantum_medium_log2);
        }
        
        
        //
        // quantum_index_unchecked
        //
        // Returns a quantum index for a arbitrary pointer.  Might be bogus if the address is in the admin (writebarrier) area.
        //
        inline usword_t quantum_index_unchecked(void *address, usword_t quantum_log2) const {
            return (((uintptr_t)address & mask(subzone_quantum_log2)) >> quantum_log2) - base_data_quantum_count(quantum_log2);
         }
        inline usword_t quantum_index_unchecked(void *address) const {
            return is_small() ? quantum_index_unchecked(address, allocate_quantum_small_log2) :
                                quantum_index_unchecked(address, allocate_quantum_medium_log2);
        }
        
        
        //
        // allocation_count
        //
        // High water count for this subzone
        //
        inline usword_t allocation_count() const { return _in_use; }

        //
        // add_allocation_count
        //
        // High water count for this subzone
        //
        inline void raise_allocation_count(usword_t q)  { _in_use += q; }

        //
        // subtract_allocation_count
        //
        // High water count for this subzone
        //
        inline void lower_allocation_count(usword_t q)  { _in_use -= q; }

        //
        // quantum_count
        //
        // Returns the number of quantum for a given size.
        //
        inline const usword_t quantum_count(const size_t size) const {
            return partition2(size, _quantum_log2);
        }
        
        
        //
        // quantum_size
        //
        // Returns the size in bytes of n quantum.
        //
        inline const usword_t quantum_size(const usword_t n) const { return n << _quantum_log2; }
        
        
        //
        // quantum_address
        //
        // Returns the address if a specified quantum.
        //
        inline void *quantum_address(const usword_t q) const { return displace(_allocation_address, quantum_size(q)); }
        
        
        //
        // quantum_range
        //
        // Return the range for the block at q.
        //
        inline void quantum_range(const usword_t q, Range &range) const {
            range.set_range(quantum_address(q), size(q));
        }
        inline void quantum_range(void *block, Range &range) const {
            range.set_range(block, size(quantum_index(block)));
        }
        
        //
        // Side data accessors
        //
        inline bool is_free(usword_t q)              const { return (_side_data[q] & ~start_bit) == 0; }
        inline bool is_free(void *address)           const { return is_free(quantum_index(address)); }
        
        inline bool is_start(usword_t q)             const { return (_side_data[q] & start_bit) != 0 && !is_free(q); }
        inline bool block_is_start(usword_t q)       const { return q < allocation_limit() && (_side_data[q] & start_bit) != 0 && !is_free(q); }
        inline bool block_is_start(void *address)    const {
            return (is_small() ? is_bit_aligned(address, allocate_quantum_small_log2) :
                                 is_bit_aligned(address, allocate_quantum_medium_log2)) &&
                   block_is_start(quantum_index_unchecked(address));
        }
        
        inline usword_t length(usword_t q)           const { 
            usword_t result;
            if (q == allocation_limit()-1 || (_side_data[q+1] & start_bit))
                result = 1;
            else {
                ASSERTION(_side_data[q + 1] != 0);
                result = _side_data[q+1] & quanta_size_mask;
            }
            return result;
        }
        inline usword_t length(void *address)        const { return length(quantum_index(address)); }
        
        inline usword_t size(usword_t q)             const { return quantum_size(length(q)); }
        inline usword_t size(void *address)          const { return size(quantum_index(address)); }
        
        inline bool is_new(usword_t q)               const { return q < allocation_limit() && !is_eldest(_side_data[q]); }
        inline bool is_new(void *address)            const { return is_new(quantum_index(address)); }
        
        inline bool is_newest(usword_t q)            const { return is_youngest(_side_data[q]); }
        inline bool is_newest(void *address)         const { return is_newest(quantum_index(address)); }
        

        inline usword_t age(usword_t q)              const { return (_side_data[q] & age_mask) >> age_mask_log2; }
        inline usword_t age(void *address)           const { return age(quantum_index(address)); }
        
        inline usword_t refcount(usword_t q)         const { return (is_live_thread_local(q)) ? 0 : (_side_data[q] & refcount_mask) >> refcount_log2; }
        inline usword_t refcount(void *address)      const { return refcount(quantum_index(address)); }
        
        inline usword_t sideData(void *address) const { return _side_data[quantum_index(address)]; }
        
        inline void incr_refcount(usword_t q) {
            // must remove from local list before incrementing
            unsigned char sd = _side_data[q];
            unsigned char r = (sd & refcount_mask) >> refcount_log2;
            sd &= ~refcount_mask;
            _side_data[q] = sd | ((r+1)<<refcount_log2);
        }
        
        inline void decr_refcount(usword_t q) {
            unsigned char sd = _side_data[q];
            unsigned char r = (sd & refcount_mask) >> refcount_log2;
            sd &= ~refcount_mask;
            _side_data[q] = sd | ((r-1) << refcount_log2);
        }
        
        inline void mature(usword_t q) {
            if (!is_thread_local(q)) {
                unsigned char data = _side_data[q];
                unsigned char current = (data & age_mask) >> age_mask_log2;
                if (current > eldest_age) {
                    data &= ~age_mask;
                    data |= ((current-1) << age_mask_log2);
                    _side_data[q] = data;
                    AUTO_PROBE(auto_probe_mature(quantum_address(q), current-1));
                }
            }
        }
        inline void mature(void *address)                  { mature(quantum_index(address)); }
        
        /* Test if the block is marked as thread local in the side data. Note that a true result does not necessarily mean it is local to the calling thread. */
        inline bool is_thread_local(usword_t q)     const { return (_side_data[q] & (start_bit|alloc_local_bit|global_bit)) == (start_bit|alloc_local_bit); }
        inline bool is_thread_local(void *address)  const { usword_t q = quantum_index(address); return is_start(q) && is_thread_local(q); }
        inline bool is_thread_local_block(void *address)  const { usword_t q = quantum_index_unchecked(address); return q < allocation_limit() && is_start(q) && is_thread_local(q); }
        
        /* Test if the block is thread local and not garbage. Note that a true result does not necessarily mean it is local to the calling thread. */
        inline bool is_live_thread_local(usword_t q)     const { return (_side_data[q] & (start_bit | alloc_local_bit | global_bit|garbage_bit)) == (start_bit|alloc_local_bit); }
        inline bool is_live_thread_local(void *address)  const { return is_live_thread_local(quantum_index(address)); }
        inline bool is_live_thread_local_block(void *address)  const { usword_t q = quantum_index_unchecked(address); return q < allocation_limit() && is_start(q) &&is_live_thread_local(q); }

        // mark a thread-local object as a global one
        // NOTE:  this must be called BEFORE the object can be retained, since it starts the object with rc=0, age=youngest.
        inline void make_global(usword_t q) {
            assert(!is_garbage(q));
            unsigned char data = _side_data[q];
            data &= ~(refcount_mask | age_mask);
            data |= global_bit | (youngest_age << age_mask_log2);
            _side_data[q] = data;
            AUTO_PROBE(auto_probe_make_global(quantum_address(q), youngest_age));
        }
        inline void make_global(void *address)  { make_global(quantum_index(address));}

        /*
         Mark a block as garbage.
         For global data mark global_bit 0 and garbage_bit 1
         For local data merely mark the garbage_bit 1 (keeping global_bit 0)
         When marking garbage also clear the refcount bits since they may get used during finalize, even for local garbage.
         As is, the full layout is preserved.
         */
        inline void mark_global_garbage(usword_t q)          { ASSERTION(!is_thread_local(q)); _side_data[q] = (_side_data[q] & (start_bit|layout_mask)) | garbage_bit; }
        inline void mark_global_garbage(void *address)       { return mark_global_garbage(quantum_index(address)); }
        inline bool is_garbage(usword_t q)           const { return (_side_data[q] & (start_bit|garbage_bit|global_bit)) == (start_bit|garbage_bit); }
        inline bool is_garbage(void *address)        const { return is_garbage(quantum_index(address)); }
        
        inline void mark_local_garbage(usword_t q)          { ASSERTION(is_thread_local(q)); _side_data[q] = (_side_data[q] & (start_bit|layout_mask)) | garbage_bit | alloc_local_bit; }
        inline void mark_local_garbage(void *address)       { return mark_local_garbage(quantum_index(address)); }
        inline bool is_local_garbage(usword_t q)      const { return (_side_data[q] & (start_bit|garbage_bit|alloc_local_bit)) == (start_bit|garbage_bit|alloc_local_bit); }
        
        inline bool is_marked(usword_t q)            const { return q < allocation_limit() && _region->is_marked(_quantum_bias + q); }
        inline bool is_marked(void *address)         const { return is_marked(quantum_index(address)); }
        
        inline usword_t layout(usword_t q)           const { return (_side_data[q] & layout_mask) >> layout_log2; }
        inline usword_t layout(void *address)        const { return layout(quantum_index(address)); }

        inline bool is_scanned(usword_t q)           const { return !(layout(q) & AUTO_UNSCANNED); }
        inline bool is_scanned(void *address)        const { return is_scanned(quantum_index(address)); }
        
        inline bool has_refcount(usword_t q)         const { return !is_thread_local(q) && (_side_data[q] & refcount_mask) != 0; }
        inline bool has_refcount(void *address)      const { return has_refcount(quantum_index(address)); }
        
        inline void set_mark(usword_t q)                   { _region->set_mark(_quantum_bias + q); }
        inline void set_mark(void *address)                { set_mark(quantum_index(address)); }

        inline void clear_mark(usword_t q)                 { _region->clear_mark(_quantum_bias + q); }
        inline void clear_mark(void *address)              { clear_mark(quantum_index(address)); }
        
        inline bool assert_thread_local(usword_t q)         { return is_thread_local(q); }
        
        inline void set_scan_local_block(usword_t q)        { ASSERTION(!is_garbage(q)); if (is_scanned(q)) _side_data[q] |= scan_local_bit; }
        inline void set_scan_local_block(void *address)     { set_scan_local_block(quantum_index(address)); }
        
        inline void clear_scan_local_block(usword_t q)        { _side_data[q] &= ~scan_local_bit; }
        inline void clear_scan_local_block(void *address)     { clear_scan_local_block(quantum_index(address)); }
        
        inline bool should_scan_local_block(usword_t q)     { return assert_thread_local(q) && (_side_data[q] & scan_local_bit); }
        inline bool should_scan_local_block(void *address)  { return should_scan_local_block(quantum_index(address)); }
        
        // mark (if not already marked)
        // return already-marked
        inline bool test_set_mark(usword_t q)              { return _region->test_set_mark(_quantum_bias + q); }
        inline bool test_set_mark(void *address)           { return test_set_mark(quantum_index(address)); }
        
        inline void set_layout(usword_t q, usword_t layout) {
            unsigned d = _side_data[q];
            d &= ~layout_mask;
            d |= (layout << layout_log2);
            _side_data[q] = d;
        }
        inline void set_layout(void *address, usword_t layout) { set_layout(quantum_index(address), layout); }
        
        inline bool is_pending(usword_t q)           const { return _region->is_pending(_quantum_bias + q); }
        inline bool is_pending(void *address)        const { return is_pending(quantum_index(address)); }
        
        inline void set_pending(usword_t q)                { _region->set_pending(_quantum_bias + q); }
        inline void set_pending(void *address)             { set_pending(quantum_index(address)); }
        
        inline void clear_pending(usword_t q)              { _region->clear_pending(_quantum_bias + q); }
        inline void clear_pending(void *address)           { clear_pending(quantum_index(address)); }
        
        //
        // is_used
        //
        // Return true if the quantum is in a used quantum.
        //
        inline bool is_used(usword_t q) const {
            // any data indicates use
            if (!is_free(q)) return true;
            
            // otherwise find the prior start
            for (usword_t s = q; true; s--) {
                if (is_start(s)) {
                    usword_t n = length(s);
                    // make sure q is in range
                    return (q - s) < n;
                }
                if (!s) break;
            }
            return false;
        }
        inline bool is_used(void *address)          const { return is_used(quantum_index(address)); }

        //
        // should_pend
        //
        // High performance check and set for scanning blocks in a subzone (major hotspot.)
        //
        bool should_pend(void *address, usword_t q) {
            unsigned char *sdq;
            
            if (is_small()) {
                if (!is_bit_aligned(address, allocate_quantum_small_log2)) return false;
                sdq = _side_data + q;
                if (q >= subzone_allocation_limit(allocate_quantum_small_log2)) return false;
            } else {
                if (!is_bit_aligned(address, allocate_quantum_medium_log2)) return false;
                sdq = _side_data + q;
                if (q >= subzone_allocation_limit(allocate_quantum_medium_log2)) return false;
            }

            usword_t sd = *sdq;
            if ((sd & start_bit) != start_bit) return false;
            if (test_set_mark(q)) return false;
            
            usword_t layout = (sd & layout_mask) >> layout_log2;
            return !(layout & AUTO_UNSCANNED);
        }
        

        //
        // should_pend_new
        //
        // High performance check and set for scanning new blocks in a subzone (major hotspot.)
        //
        bool should_pend_new(void *address, usword_t q) {
            unsigned char *sdq;
            
            if (is_small()) {
                if (!is_bit_aligned(address, allocate_quantum_small_log2)) return false;
                sdq = _side_data + q;
                if (q >= subzone_allocation_limit(allocate_quantum_small_log2)) return false;
            } else {
                if (!is_bit_aligned(address, allocate_quantum_medium_log2)) return false;
                sdq = _side_data + q;
                if (q >= subzone_allocation_limit(allocate_quantum_medium_log2)) return false;
            }

            usword_t sd = *sdq;
            if ((sd & start_bit) != start_bit || is_eldest(sd)) return false;
            if (test_set_mark(q)) return false;

            usword_t layout = (sd & layout_mask) >> layout_log2;
            return !(layout & AUTO_UNSCANNED);
        }
        
        
        //
        // next_quantum
        //
        // Returns the next q for block or free node.
        //
        inline usword_t next_quantum(usword_t q = 0) const {
            usword_t nq;
            if (is_start(q)) {
                nq = q + length(q);
            } else {
                // FIXME:  accessing the free list without holding the allocation lock is a race condition.
                // SpinLock lock(_admin->lock());
                // FreeListNode *node = (FreeListNode *)quantum_address(q);
                // q = quantum_index(node->next_block());
                // Instead, we simply search for the next block start. Note, this means we no longer
                // return quanta for free blocks.
                usword_t n = allocation_limit();
                nq = q + 1;
                while (nq < n && !is_start(nq)) ++nq;
            }
            // Until <rdar://problem/6404163> is fixed, nq can equal q. This is mostly harmless, because the
            // caller will keep looping over the same q value, until _side_data[q + 1] is updated.
            ASSERTION(nq >= q);
            return nq;
        }

        inline usword_t next_quantum(usword_t q, MemoryReader & reader) const {
            return next_quantum(q);
        }

        //
        // block_start
        //
        // Return the start address for the given address.
        // All clients must (and do) check for NULL return.
        //
        inline void * block_start(void *address) const {
            usword_t q = quantum_index_unchecked(address), s = q;
            // an arbitrary address in our admin area will return a neg (very large) number
            if (q > allocation_limit()) return NULL;
            do {
                if (is_start(s)) {
                    usword_t n = length(s);
                    // make sure q is in range
                    return ((q - s) < n) ? quantum_address(s) : NULL;
                }
            } while (s--);
            return NULL;
        }

        //
        // allocate
        //
        // Initialize side data for a new block.
        //
        inline void allocate(usword_t q, const usword_t n, const usword_t layout, const bool refcount_is_one, const bool is_local) {
            ASSERTION(n <= maximum_quanta && n > 0);
            unsigned char sd;
            sd =    start_bit
                | (layout << layout_log2) 
                | (is_local ?  alloc_local_bit : (global_bit | (youngest_age << age_mask_log2)))
                //| (is_local ?  alloc_local_bit : global_bit) // hides allocation microbenchmark issues
                | (refcount_is_one ? (1 << refcount_log2) : 0);
            
            _side_data[q] = sd;
            if (n > 1) {
                _side_data[q + 1] = n;
                _side_data[q + n - 1] = n;
            }
            // Only touch the next block if it is zero (free)
            // Other threads can touching that block's side data (global/local/garbage)
            if (q+n < allocation_limit() && _side_data[q + n] == 0)
                _side_data[q + n] |= start_bit;
        }

        //
        // cache
        //
        // Initialize side data for a cached block.
        //
        inline void cache(usword_t q, const usword_t n) {
            ASSERTION(n <= maximum_quanta && n > 0);
            _side_data[q] = (start_bit | alloc_local_bit | (AUTO_MEMORY_UNSCANNED << layout_log2) | garbage_bit);
            if (n > 1) {
                _side_data[q + 1] = n;
                _side_data[q + n - 1] = n;
            }
            // Only touch the next block if it is zero (free)
            // Other threads can touching that block's side data (global/local/garbage)
            if (q+n < allocation_limit() && _side_data[q + n] == 0)
                _side_data[q + n] |= start_bit;
        }

        //
        // deallocate
        //
        // Clear side data for an existing block.
        //
        inline void deallocate(usword_t q, usword_t len) {
            bool prev_quanta_allocated = (q > 0 ? (_side_data[q-1] != 0) : false);
            
            _side_data[q] = prev_quanta_allocated ? start_bit : 0;
            if (len > 1) {
                _side_data[q+1] = 0;
                _side_data[q+len-1] = 0;
            }
            if (q+len < allocation_limit()) {
                if (_side_data[q+len] == start_bit)
                    _side_data[q+len] = 0;
            }
        }
        inline void deallocate(usword_t q) { deallocate(q, length(q)); }


        //
        // write_barrier
        //
        // Returns accessor for this subzone's write barrier.
        //
        inline WriteBarrier& write_barrier() {
            return _write_barrier;
        }
        
        //
        // malloc_statistics
        //
        // Adds the Subzone's memory use to stats.
        //
        void malloc_statistics(malloc_statistics_t *stats);
    };
    
    
    //----- SubzoneRangeIterator -----//
    
    //
    // Iterate over a range of memory
    //
    
    class SubzoneRangeIterator : public Range {

      public:
        
        //
        // Constructors
        //
        SubzoneRangeIterator(void *address, const usword_t size)
        : Range(address, size)
        {}
        
        SubzoneRangeIterator(void *address, void *end)
        : Range(address, end)
        {}
        
        SubzoneRangeIterator(Range range)
        : Range(range)
        {}
        
        
        //
        // next
        //
        // Returns next subzone in the range or NULL if no more entries available.
        //
        inline Subzone *next() {
            // if cursor is still in range
            if (address() < end()) {
                // capture cursor position
                Subzone *_next = (Subzone *)address();
                // advance for next call
                set_address(displace(_next, subzone_quantum));
                // return captured cursor position
                return _next;
            }
            
            // at end
            return NULL;
        }
        
    };
    
};


#endif // __AUTO_SUBZONE__

