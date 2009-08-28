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
    AutoLarge.h
    Large Block Support
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_LARGE__
#define __AUTO_LARGE__

#include "AutoDefs.h"
#include "AutoStatistics.h"
#include "AutoWriteBarrier.h"
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
        usword_t _vm_size;              // size of the vm allocation
        usword_t _size;                 // size of the requested allocation
        usword_t _layout;               // organization of block
        usword_t _refcount;             // large block reference count
        usword_t _age;                  // block age
        bool _is_pending;               // needs scanning flag
        bool _is_marked;                // has been visited flag
        bool _is_garbage;
        WriteBarrier _write_barrier;    // write barrier accessor object.
      
        Large(usword_t vm_size, usword_t size, usword_t layout, usword_t refcount, usword_t age, const WriteBarrier &wb);
        
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
        inline usword_t vm_size()         const { return _vm_size; }
        inline void set_prev(Large *prev)       { _prev = prev; }
        inline void set_next(Large *next)       { _next = next; }
        inline Range range()                    { return Range(address(), size()); }
        inline void mark_garbage()              { _is_garbage = true; }
        inline bool is_garbage()          const { return _is_garbage; }
        
        
        //
        // Side data accessors
        //
        inline usword_t size()                       const { return _size; }
        static inline usword_t size(void *address)         { return large(address)->size(); }
        
        inline bool is_new()                         const { return _age != 0; }
        static inline bool is_new(void *address)           { return large(address)->is_new(); }
        inline bool is_newest()                      const { return _age == initial_age; }
        static inline bool is_newest(void *address)        { return large(address)->is_newest(); }
        
        inline usword_t age()                        const { return _age; }
        static inline usword_t age(void *address)          { return large(address)->age(); }
        inline void set_age(usword_t a)                    { _age = a; }
        static inline void set_age(void *address, usword_t a) { large(address)->set_age(a); }
        
        inline void mature()                               { _age--; }
        static inline void mature(void *address)           { large(address)->mature(); }
        
        inline bool is_pending()                     const { return _is_pending; }
        static inline bool is_pending(void *address)       { return large(address)->is_pending(); }
        
        inline bool is_marked()                      const { return _is_marked; }
        static inline bool is_marked(void *address)        { return large(address)->is_marked(); }
        
        inline usword_t layout()                     const { return _layout; }
        static inline usword_t layout(void *address)       { return large(address)->layout(); }
        
        inline bool is_scanned()                     const { return !(_layout & AUTO_UNSCANNED); }
        static inline bool is_scanned(void *address)       { return large(address)->is_scanned(); }
        
        inline usword_t refcount()                   const { return _refcount; }
        static inline usword_t refcount(void *address)     { return large(address)->refcount(); }
        
        inline void set_pending()                          { _is_pending = true; }
        static inline void set_pending(void *address)      { large(address)->set_pending(); }
        
        inline void clear_pending()                        { _is_pending = false; }
        static inline void clear_pending(void *address)    { large(address)->clear_pending(); }
        
        inline void set_mark()                             { _is_marked = true; }
        static inline void set_mark(void *address)         { large(address)->set_mark(); }
        
        inline void clear_mark()                           { _is_marked = false; }
        static inline void clear_mark(void *address)       { large(address)->clear_mark(); }

        inline bool test_set_mark() {
            bool old = _is_marked;
            if (!old) _is_marked = true;
            return old;
        }
        static inline bool test_set_mark(void *address)    { return large(address)->test_set_mark(); }
        
        inline void set_refcount(usword_t refcount)        { _refcount =  refcount; }
        static inline void set_refcount(void *address, usword_t refcount)
                                                           { large(address)->set_refcount(refcount); }
        
        inline void set_layout(usword_t layout)            { _layout = layout; }
        static inline void set_layout(void *address, usword_t layout)
                                                           { large(address)->set_layout(layout); }

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
