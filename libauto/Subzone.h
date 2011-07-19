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
    Subzone.h
    Quantized Memory Allocation
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_SUBZONE__
#define __AUTO_SUBZONE__

#include "Admin.h"
#include "Definitions.h"
#include "Bitmap.h"
#include "FreeList.h"
#include "WriteBarrier.h"
#include "Region.h"
#import "auto_tester/auto_tester.h"
#include "auto_zone.h"
#include "auto_trace.h"
#include "auto_dtrace.h"
#include <cassert>

namespace Auto {

    // Forward declarations
    class Region;
    class Thread;
    
    
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
        Subzone        *_nextSubzone;   // used by admin's _purgeable_subzones list.
        bool           _is_purgeable;   // true if this subzone is in admin's _purgeable_subzones list.
        bool           _is_purged;      // true if uncommit_memory() was called on the inactive range.
        usword_t       _quantum_bias;   // the value added to subzone quantum numbers to get a globally
                                        // unique quantum (used to index region pending/mark bits)
        void           *_allocation_address; // base address for allocations
        usword_t       _in_use;         // high water mark
        unsigned char * volatile _checking_counts; // collection checking counts, by quantum index
        volatile int32_t _pending_count;
        bool           _is_being_scanned;
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
            layout_log2 = 4,
            layout_mask = 0x7 << layout_log2,                   // indicates block organization
            
            /*
             The global bit is consulted in order to determine the interpretation of all remaining bits.
             When global bit == 1, the age and refcount bits are valid.
             When global bit == 0, alloc_local_bit and garbage_bit are valid.
             When garbage_bit == 1, refcount bit is again valid since blocks may be marked external in a finalize method.
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
            refcount_bit = 0x1 << refcount_log2,                // if global_bit == 1 else garbage_bit == 1. holds inline refcount
            
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
              _quantum_log2(quantum_log2), _region(region), _admin(admin), _nextSubzone(NULL), _is_purgeable(false), _is_purged(false),
              _quantum_bias(quantum_bias), _allocation_address(NULL), _in_use(0), _pending_count(0), _is_being_scanned(false)
        {
            usword_t base_data_size = is_small() ?
                                        subzone_base_data_size(allocate_quantum_small_log2) :
                                        subzone_base_data_size(allocate_quantum_medium_log2);
            _allocation_address = (void *)displace(this, base_data_size);
        }
        
      
        //
        // Destructor
        //
        ~Subzone();
        
        
        // pack/unpack a subzone/quantum index pair into a single pointer sized value
        inline uintptr_t pack(usword_t q) { 
            assert(q <= 65536); 
            assert(uintptr_t(this) == (uintptr_t(this) & ~0x1FFFF));
            return (uintptr_t)this | q;
        }
        static Subzone *unpack(uintptr_t packed, usword_t &q) {
            q = ((usword_t)packed & 0x1FFFF);
            return (reinterpret_cast<Subzone *>(packed & ~0x1FFFF));
        }
        
        
        //
        // Accessors
        //
        usword_t quantum_log2()                const { return _quantum_log2; }
        Region *region()                       const { return _region; }
        Admin *admin()                         const { return _admin; }
        
        Subzone *nextSubzone()                 const { return _nextSubzone; }
        void setNextSubzone(Subzone *subzone)        { _nextSubzone = subzone; }
        bool is_purgeable()                    const { return _is_purgeable; }
        void set_purgeable(bool purgeable)           { _is_purgeable = purgeable; }
        bool is_purged()                       const { return _is_purged; }
        void set_purged(bool purged)                 { _is_purged = purged; }

        //
        // purgeable_range
        //
        // Returns the page aligned range of memory that is free at the end of the subzone.
        // This range can be passed to madvise() to return the memory to the system.
        // 
        Range purgeable_range() const {
            usword_t unused = allocation_limit() - _in_use;
            usword_t size = quantum_size(unused);
            if (size >= page_size) {
                void *address = quantum_address(_in_use);
                return Range(align_up(address, page_size_log2), align_down(displace(address, size), page_size_log2));
            } else {
                return Range();
            }
        }
        
        usword_t quantum_bias()                const { return _quantum_bias; }
        
        bool has_pending()                     const { return _pending_count != 0; }
        int32_t add_pending_count(int32_t count)     { return OSAtomicAdd32(count, &_pending_count); }
        int32_t pending_count() const                { return _pending_count; } // note that this queries a volatile value in an unsynchronized manner
        bool is_being_scanned()                     const { return _is_being_scanned; }
        void set_is_being_scanned(bool p)                 { _is_being_scanned = p; }
        
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
            ASSERTION(quantum_log2 == _quantum_log2);
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
            ASSERTION(quantum_log2 == _quantum_log2);
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
        
        //
        // Side data accessors
        //
        inline bool is_free(usword_t q)              const { return (_side_data[q] & ~start_bit) == 0; }
        
        inline bool is_start(usword_t q)             const { return (_side_data[q] & start_bit) != 0 && !is_free(q); }
        inline bool block_is_start(usword_t q)       const { return q < allocation_limit() && (_side_data[q] & start_bit) != 0 && !is_free(q); }
        inline bool block_is_start(void *address, usword_t *q)    const {
            return (is_small() ? is_bit_aligned(address, allocate_quantum_small_log2) :
                                 is_bit_aligned(address, allocate_quantum_medium_log2)) &&
                   block_is_start(*q = quantum_index_unchecked(address));
        }
        
        inline usword_t length(usword_t q)           const { 
            usword_t result;
            if (q == allocation_limit()-1 || (_side_data[q+1] & start_bit))
                result = 1;
            else {
                // ASSERTION(_side_data[q + 1] != 0);
                result = _side_data[q+1] & quanta_size_mask;
            }
            return result;
        }
        
        inline usword_t size(usword_t q)             const { return quantum_size(length(q)); }
        
        inline bool is_new(usword_t q)               const { ASSERTION(!is_thread_local(q)); return !is_eldest(_side_data[q]); }
        
        inline bool is_newest(usword_t q)            const { ASSERTION(!is_thread_local(q)); return is_youngest(_side_data[q]); }
        

        inline usword_t age(usword_t q)              const { ASSERTION(!is_thread_local(q)); return (_side_data[q] & age_mask) >> age_mask_log2; }

        inline void set_age(usword_t q, usword_t age)      {
            ASSERTION(!is_thread_local(q));
            unsigned char data = _side_data[q];
            data &= ~age_mask;
            data |= (age << age_mask_log2);
            _side_data[q] = data;
        }
        
        inline usword_t sideData(void *address) const { return _side_data[quantum_index(address)]; }
        
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
        
        /* Test if the block is marked as thread local in the side data. Note that a true result does not necessarily mean it is local to the calling thread. */
        inline bool is_thread_local(usword_t q)     const { return (_side_data[q] & (start_bit|alloc_local_bit|global_bit)) == (start_bit|alloc_local_bit); }
        
        /* Test if the block is thread local and not garbage. Note that a true result does not necessarily mean it is local to the calling thread. */
        inline bool is_live_thread_local(usword_t q)     const { return (_side_data[q] & (start_bit | alloc_local_bit | global_bit|garbage_bit)) == (start_bit|alloc_local_bit); }

#ifdef MEASURE_TLC_STATS
        void update_block_escaped_stats();
#endif
        
        // mark a thread-local object as a global one
        // NOTE:  this must be called BEFORE the object can be retained, since it starts the object with rc=0, age=youngest.
        inline void make_global(usword_t q) {
            ASSERTION(is_live_thread_local(q));
            unsigned char data = _side_data[q];
            data &= ~(refcount_bit | age_mask);
            data |= global_bit | (youngest_age << age_mask_log2);
            _side_data[q] = data;
            AUTO_PROBE(auto_probe_make_global(quantum_address(q), youngest_age));
            GARBAGE_COLLECTION_AUTO_BLOCK_LOST_THREAD_LOCALITY(quantum_address(q), size(q));
#ifdef MEASURE_TLC_STATS
            update_block_escaped_stats();
#endif
        }

        /*
         Mark a block as garbage.
         For global data mark global_bit 0 and garbage_bit 1
         For local data merely mark the garbage_bit 1 (keeping global_bit 0)
         When marking garbage also clear the refcount bits since they may get used during finalize, even for local garbage.
         As is, the full layout is preserved.
         */
        // Theoretically we should be able to assert !is_thread_local(q) here. But due to the way the bits
        // are encoded the assertion also triggers for a block that was previously marked garbage which was resurrected.
        // Removing the assertion for sake of the unit tests.
        inline void mark_global_garbage(usword_t q)          { /* ASSERTION(!is_thread_local(q)); */ _side_data[q] = (_side_data[q] & (start_bit|layout_mask)) | garbage_bit; }
        inline bool is_garbage(usword_t q)           const { return (_side_data[q] & (start_bit|garbage_bit|global_bit)) == (start_bit|garbage_bit); }
        inline bool is_global_garbage(usword_t q)     const { return !is_thread_local(q) && is_garbage(q); }
        
        inline void mark_local_garbage(usword_t q)          { ASSERTION(is_live_thread_local(q)); _side_data[q] = (_side_data[q] & (start_bit|layout_mask)) | garbage_bit | alloc_local_bit; }
        inline bool is_local_garbage(usword_t q)      const { return (_side_data[q] & (start_bit|garbage_bit|alloc_local_bit)) == (start_bit|garbage_bit|alloc_local_bit); }
        
        inline bool is_marked(usword_t q)            const { return q < allocation_limit() && _region->marks().bit(_quantum_bias + q); }

        inline usword_t layout(usword_t q)           const { return (_side_data[q] & layout_mask) >> layout_log2; }

        inline bool is_scanned(usword_t q)           const { return !(layout(q) & AUTO_UNSCANNED); }
        
        inline bool has_refcount(usword_t q)         const { return !is_live_thread_local(q) && (_side_data[q] & refcount_bit) != 0; }
        inline void set_has_refcount(usword_t q)           { ASSERTION(!is_live_thread_local(q)); _side_data[q] |= refcount_bit; }
        inline void clear_has_refcount(usword_t q)         { ASSERTION(!is_live_thread_local(q)); _side_data[q] &= ~refcount_bit; }
                
        inline void set_scan_local_block(usword_t q)        { ASSERTION(is_live_thread_local(q)); if (is_scanned(q)) _side_data[q] |= scan_local_bit; }
        
        inline void clear_scan_local_block(usword_t q)        { ASSERTION(is_live_thread_local(q)); _side_data[q] &= ~scan_local_bit; }
        
        inline bool should_scan_local_block(usword_t q)     { ASSERTION(is_live_thread_local(q)); return (_side_data[q] & scan_local_bit); }
        
        // mark (if not already marked)
        // return already-marked
        inline bool test_and_set_mark(usword_t q)              { return _region->test_and_set_mark(_quantum_bias + q); }
        
        // Used to mark objects ineligible for compaction.
        inline bool test_and_set_pinned(usword_t q)         { return _region->pinned().test_set_bit_atomic(_quantum_bias + q); }
        inline void mark_pinned(usword_t q)                 { _region->pinned().set_bit_atomic(_quantum_bias + q); }
        inline bool is_pinned(usword_t q)                   { return _region->pinned().bit(_quantum_bias + q); }
        
        inline bool is_compactable(usword_t q) const {
            usword_t biased_q = q + _quantum_bias;
            return _region->marks().bit(biased_q) && !_region->pinned().bit(biased_q);
        }
        
        // Used to mark objects that have been forwarded during compaction.
        inline void mark_forwarded(usword_t q)              { _region->pending().set_bit(_quantum_bias + q); }
        inline void clear_forwarded(usword_t q)             { _region->pending().clear_bit(_quantum_bias + q); }
        inline bool is_forwarded(usword_t q)                { return _region->pending().bit(_quantum_bias + q); }
        
        inline bool test_and_clear_mark(usword_t q)            { return _region->test_and_clear_mark(_quantum_bias + q); }
        
        inline void set_layout(usword_t q, usword_t layout) {
            unsigned d = _side_data[q];
            d &= ~layout_mask;
            d |= (layout << layout_log2);
            _side_data[q] = d;
        }
        
        inline bool test_and_set_pending(usword_t q, bool adjust_pending_count)                { 
            bool result = _region->pending().test_set_bit_atomic(_quantum_bias + q); 
            if (!result && adjust_pending_count) add_pending_count(1); 
            return result; 
        }
        
        inline bool test_and_clear_pending(usword_t q)              { return _region->pending().test_clear_bit_atomic(_quantum_bias + q); }
        
        inline Bitmap::AtomicCursor pending_cursor() { return Bitmap::AtomicCursor(_region->pending(), _quantum_bias, allocation_limit()); }
        
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
        // previous_quantum
        //
        // Returns the previous q for block or free node.
        //
        inline usword_t previous_quantum(usword_t q) {
            // find a prior start before the specified q.
            ASSERTION(q <= allocation_limit());
            while (q--) {
                if (is_start(q))
                    return q;
            }
            return not_found;
        }

        //
        // block_start
        //
        // Return the start address for the given address.
        // All clients must (and do) check for NULL return.
        //
        inline void * block_start(void *address, usword_t &startq) const {
            usword_t q = quantum_index_unchecked(address), s = q;
            // an arbitrary address in our admin area will return a neg (very large) number
            if (q > allocation_limit()) return NULL;
            do {
                if (is_start(s)) {
                    usword_t n = length(s);
                    // make sure q is in range
                    if ((q - s) < n) {
                        startq = s;
                        return quantum_address(s);
                    }
                    return NULL;
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
                | (refcount_is_one ? refcount_bit : 0);
            
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
        // collection_checking_count
        //
        // retrieve the collection checking count for a quanta
        //
        inline uint32_t collection_checking_count(usword_t q) const { 
            // make a copy of the volatile buffer pointer on the stack so it won't be freed while we are using it
            unsigned char *counts = _checking_counts;
            return counts ? counts[q] : 0; 
        }
        
        
        //
        // set_collection_checking_count
        //
        // set the collection checking count for a quanta, allocates the counts buffer if needed
        //
        inline void set_collection_checking_count(usword_t q, uint32_t count) {
            // if this is the first time checking has been requested then we need to allocate a a buffer to hold the counts
            // but don't allocate just to clear the count to zero
            if (_checking_counts == NULL && count != 0) {
                // Use a collectable buffer to store the check counts. This enables unsynchronized cleanup if/when checking is turned off.
                // Note that we only get here via a collection checking request from a user thread, never from the collector internally.
                void *tmp = auto_zone_allocate_object((auto_zone_t *)_admin->zone(), allocation_limit() * sizeof(unsigned char), AUTO_UNSCANNED, true, true);
                if (!OSAtomicCompareAndSwapPtrBarrier(NULL, tmp, (void * volatile *)(void *)&_checking_counts))
                    auto_zone_release((auto_zone_t *)_admin->zone(), tmp);
            }
            
            // make a copy of the volatile buffer pointer on the stack so it won't be freed while we are using it
            unsigned char *counts = _checking_counts;
            if (counts != NULL) {
                counts[q] = count < 255 ? count : 255;
            }
        }
        
        
        //
        // reset_collection_checking
        //
        // frees the memory buffer used for collection checking
        //
        inline void reset_collection_checking() {
            unsigned char *counts = _checking_counts;
            if (OSAtomicCompareAndSwapPtrBarrier(counts, NULL, (void * volatile *)(void *)&_checking_counts))
                auto_zone_release((auto_zone_t *)_admin->zone(), counts);
        }
        
        
        
        //
        // PendingCountAccumulator
        // 
        // PendingCountAccumulator is a per-thread buffer to accumulate updates to the subzone pending count.
        // The accumulator is used during threaded scanning to reduce the total number of atomic updates.
        //
        class PendingCountAccumulator {
            Thread &_thread;
            Subzone *_last_pended_subzone;
            usword_t _pended_count;
            
        public:
            PendingCountAccumulator(Thread &thread);
            
            ~PendingCountAccumulator();
            
            inline void flush_count() {
                if (_last_pended_subzone && _pended_count) {
                    _last_pended_subzone->add_pending_count(_pended_count);
                    _pended_count = 0;
                }
            }
            
            inline void pended_in_subzone(Subzone *sz) {
                if (_last_pended_subzone != sz) {
                    if (_pended_count) {
                        flush_count();
                    }
                    _last_pended_subzone = sz;
                }
                _pended_count++;
            }
        };
        
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
        SubzoneRangeIterator(void *address, const usword_t size) : Range(address, size) {}
        SubzoneRangeIterator(void *address, void *end) : Range(address, end) {}
        SubzoneRangeIterator(Range range) : Range(range) {}
        
        
        //
        // iteration_complete
        //
        // returns true if the iteration has reached the end and no more entries are available
        //
        inline boolean_t iteration_complete() { return !(address() < end()); }
        
        //
        // next
        //
        // Returns next subzone in the range or NULL if no more entries available.
        //
        inline Subzone *next() {
            // if cursor is still in range
            if (address() < end()) {
                // capture cursor position
                Subzone *subzone = (Subzone *)address();
                // advance for next call
                set_address(displace(subzone, subzone_quantum));
                // return captured cursor position
                return subzone;
            }
            
            // at end
            return NULL;
        }
        
        //
        // previous
        //
        // Returns previous subzone in the range or NULL if no more entries available.
        //
        inline Subzone *previous() {
            if (end() > address()) {
                Subzone *prev = (Subzone *)displace(end(), -subzone_quantum);
                set_end(prev);
                return prev;
            }
            return NULL;
        }
    };
    
};


#endif // __AUTO_SUBZONE__

