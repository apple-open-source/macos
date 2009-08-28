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
    AutoStatistics.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_STATISTICS__
#define __AUTO_STATISTICS__


#include "AutoDefs.h"


namespace Auto {

    //----- Statistics -----//
    
    //
    // The collector requires more overhead than standard allocators.
    // There are several kinds of overhead.  Since we round up to quanta size there is a quantization overhead which we don't measure.
    // There are holes in the heap.  _size tracks quanta handed out and _dirty_size tracks the holes within (reported as max_size_in_use).
    // Other overheads include the free lists, the per-quanta 'admin' byte, and the write-barrier (card) bytes.  These are totaled in
    // the _admin_size field.  Other overheads include memory for the weak references table, etc.  In general we use an auxiliary zone for
    // those memory uses and simply tack on the total size of that.
    //

    class Statistics {
    
      private:
    
        volatile uint32_t _count;                                    // number of blocks handed out
        volatile usword_t _size;                                     // total # bytes of blocks in use (including roundup)
        volatile usword_t _dirty_size;                               // size in use + holes (committed memory)
        volatile usword_t _allocated;                                // size of vm allocated memory (reserved)
        volatile usword_t _admin_size;                               // size of additional adminitrative data (vm memory)
        volatile usword_t _small_medium_size;                        // total size of small medium blocks in use
        volatile usword_t _partial_gc_count;                         // number of partial collections
        volatile usword_t _full_gc_count;                            // number of full collections
        volatile usword_t _stack_overflow_count;                     // number of stack overflows
        volatile usword_t _regions_in_use;                           // number of regions
        volatile usword_t _subzones_in_use;                          // number of subzones
      
      public:
      
        //
        // Constructor
        //
        Statistics() { reset(); }
        
        
        //
        // Accessors
        //
        inline usword_t count()                 const { return _count; }
        inline usword_t size()                  const { return _size; }
        inline usword_t dirty_size()            const { return _dirty_size; }
        inline usword_t admin_size()            const { return _admin_size; }
        inline usword_t allocated()             const { return _allocated; }
        inline usword_t unused()                const { return _allocated - _size; }
        inline usword_t small_medium_size()     const { return _small_medium_size; }
        inline usword_t large_size()            const { return _size - _small_medium_size; }
        inline usword_t partial_gc_count()      const { return _partial_gc_count; }
        inline usword_t full_gc_count()         const { return _full_gc_count; }
        inline usword_t stack_overflow_count()  const { return _stack_overflow_count; }
        inline usword_t regions_in_use()        const { return _regions_in_use; }
        inline usword_t subzones_in_use()       const { return _subzones_in_use; }
        
        
        //
        // Accumulators
        //
        inline void add_count(intptr_t n)             { _count += n; }
        inline void add_size(intptr_t size)           { _size += size; }
        inline void add_allocated(intptr_t allocated) { _allocated += allocated; }
        inline void add_dirty(intptr_t size)          { _dirty_size += size; }
        inline void add_admin(intptr_t size)          { _admin_size += size; }
        inline void add_small_medium_size(intptr_t size) { _small_medium_size += _size; }
        inline void increment_gc_count(bool is_partial) {
            if (is_partial) _partial_gc_count++;
            else            _full_gc_count++;
        }
        inline void increment_stack_overflow_count()  { _stack_overflow_count++; }
        inline void increment_regions_in_use()        { _regions_in_use++; }
        inline void increment_subzones_in_use()       { _subzones_in_use++; }
        
        //
        // reset
        //
        // Reset the accumulators
        //
        inline void reset() {
            _count = 0;
            _size = 0;
            _dirty_size = 0;
            _admin_size = 0;
            _allocated = 0;
            _small_medium_size = 0;
            _partial_gc_count = 0;
            _full_gc_count = 0;
            _stack_overflow_count = 0;
            _regions_in_use = 0;
            _subzones_in_use = 0;
        }
        
        
    };

};

#endif // __AUTO_STATISTICS__

