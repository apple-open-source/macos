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
    AutoLarge.h
    Large Block Support
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_LARGE__
#define __AUTO_LARGE__

#include "Definitions.h"
#include "Statistics.h"
#include "WriteBarrier.h"
#include "auto_zone.h"

namespace Auto {

    //
    // Forward declarations
    //
    
    class Zone;

    //----- Large -----//
    
    //
    // Manages information associated with a large block.
    //

    class Large {
    
      private:
        enum {
            initial_age = 5
        };
      
        Large *_prev;                   // previous large block or NULL if head of list
        Large *_next;                   // next large block or NULL if tail of list
        Zone  *_zone;                   // the zone containing the large
        usword_t _vm_size;              // size of the vm allocation
        usword_t _size;                 // size of the requested allocation
        usword_t _layout;               // organization of block
        usword_t _refcount;             // large block reference count
        usword_t _age;                  // block age
        bool _is_pending;               // needs scanning flag
        bool _is_marked;                // has been visited flag
        bool _is_garbage;
        uint32_t _checking_count;       // collection checking - count of survived collections + 1 (0 implies unchecked)
        WriteBarrier _write_barrier;    // write barrier accessor object.
      
        Large(Zone *zone, usword_t vm_size, usword_t size, usword_t layout, usword_t refcount, usword_t age, const WriteBarrier &wb);
        
      public:
      
        //
        // quantum_index
        //
        // Returns a memory index for an arbitrary pointer.
        //
#if UseArena
        // XXX not sure if the compiler makes the mask a no-op on 32-bit so go ahead and leave old one
        // in place until 64-bit tuning occurs
        // Note that the mask(arena_log2 - large_log2) is defined to work on 32-bit without needing a long long
        // computation.
        static inline const usword_t quantum_index(void *address) { return (((usword_t)address >> allocate_quantum_large_log2) & mask(allocate_quantum_large_max_log2)); }
#else
        static inline const usword_t quantum_index(void *address) { return ((usword_t)address >> allocate_quantum_large_log2); }
#endif
        
        
        //
        // quantum_count
        //
        // Returns a number of memory quantum for a given size.
        //
        static inline const usword_t quantum_count(const size_t size) { return partition2(size, allocate_quantum_large_log2); }
        
        
        //
        // quantum_large
        //
        // Returns the Large of the specified large quantum.
        //
        static inline Large *quantum_large(const usword_t q, void*arena) { return (Large *)((usword_t)arena+(q << allocate_quantum_large_log2)); }
        
        
        //
        // side_data_size
        //
        // Return the size of the block header and guarantee small quantum alignment.
        //
        static inline usword_t side_data_size() { return align2(sizeof(Large), allocate_quantum_small_log2); }
      
      
        //
        // address
        //
        // Return the address of the allocation
        //
        inline void *address() { return displace(this, side_data_size()); }
        
        
        //
        // is_start
        //
        // Returns true if the address is the start of a block.
        //
        static inline bool is_start(void *address) {
            return (((uintptr_t)address) & mask(allocate_quantum_large_log2)) == side_data_size();
        }
        
        
        //
        // large
        //
        // Return the address of the large side data.
        //
        static inline Large *large(void *address) { return (Large *)displace(address, -side_data_size()); }
        
      
        //
        // allocate
        //
        // Allocate memory used for the large block.
        //
        static Large *allocate(Zone *zone, const usword_t size, usword_t layout, bool refcount_is_one);


        //
        // deallocate
        //
        // Release memory used by the large block.
        //
        void deallocate(Zone *zone);        

        
        //
        // Accessors
        //
        inline Large *prev()              const { return _prev; }
        inline Large *next()              const { return _next; }
        inline Zone  *zone()              const { return _zone; }
        inline usword_t vm_size()         const { return _vm_size; }
        inline void set_prev(Large *prev)       { _prev = prev; }
        inline void set_next(Large *next)       { _next = next; }
        inline Range range()                    { return Range(address(), size()); }
        inline void mark_garbage()              { _is_garbage = true; }
        inline bool is_garbage()          const { return _is_garbage; }
        
        inline uint32_t collection_checking_count() const { return _checking_count; }
        inline void set_collection_checking_count(uint32_t count) { _checking_count = count; }
        
        //
        // Side data accessors
        //
        inline usword_t size()                       const { return _size; }
        
        inline bool is_new()                         const { return _age != 0; }
        inline bool is_newest()                      const { return _age == initial_age; }
        
        inline usword_t age()                        const { return _age; }
        inline void set_age(usword_t a)                    { _age = a; }
        
        inline void mature()                               { _age--; }
        
        inline bool is_pending()                     const { return _is_pending; }
        
        inline bool is_marked()                      const { return _is_marked; }
        
        inline usword_t layout()                     const { return _layout; }
        
        inline bool is_scanned()                     const { return ::is_scanned(_layout); }
        
        inline bool is_object()                      const { return ::is_object(_layout); }
        
        inline usword_t refcount()                   const { return _refcount; }
        
        inline void set_pending()                          { _is_pending = true; }
        
        inline void clear_pending()                        { _is_pending = false; }
        
        inline void set_mark()                             { _is_marked = true; }
        
        inline void clear_mark()                           { _is_marked = false; }

        inline bool test_and_set_mark() {
            bool old = _is_marked;
            if (!old) _is_marked = true;
            return old;
        }
        
        inline bool test_clear_mark() {
            bool old = _is_marked;
            if (old) _is_marked = false;
            return old;
        }
        
        inline void set_refcount(usword_t refcount)        { _refcount =  refcount; }
        
        inline void set_layout(usword_t layout)            { _layout = layout; }

        // List Operations
        
        //
        // add
        //
        // Puts this Large block on the front of the specified list.
        //
        inline void add(Large *&large_list) {
            _prev = NULL;
            _next = large_list;
            if (large_list) large_list->set_prev(this);
            large_list = this;
        }
        
        //
        // remove
        //
        // Removes this Large block from the specified list.
        //
        inline void remove(Large *&large_list) {
            if (large_list == this) large_list = _next;
            if (_prev) _prev->set_next(_next);
            if (_next) _next->set_prev(_prev);
            _prev = _next = NULL;
        }
        

        //
        // write_barrier
        //
        // Returns accessor for this large block's write barrier.
        //
        inline WriteBarrier& write_barrier() {
            return _write_barrier;
        }
    };
    
};


#endif // __AUTO_LARGE__
