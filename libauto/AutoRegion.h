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
    AutoRegion.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_REGION__
#define __AUTO_REGION__

#include "AutoAdmin.h"
#include "AutoConfiguration.h"
#include "AutoDefs.h"
#include "AutoRange.h"
#include "AutoStatistics.h"


namespace Auto {

    // Forward declarations
    
    class Zone;
    class Subzone;
    

    //----- Region -----//
    
    class Region : public Range {
    
      private:
        
        Region     *_next;                                  // next region in the list.
        Zone       *_zone;                                  // reference back to main zone
        spin_lock_t _subzone_lock;                          // protects add_subzone().
        usword_t    _i_subzones;                            // number of active subzones
        usword_t    _n_subzones;                            // total number of subzones
        usword_t    _n_quantum;                             // total number of quantum avalable
        Range       _scan_space;                            // space used during scanning - may be used as
                                                            // a stack or pending bits
        Bitmap      _pending;                               // "ready to use" bitmap overlayed on _scan_space
        Bitmap      _marks;                                 // bitmap for the marks used during reachability analysis

      public:
        
        //
        // bytes_needed
        //
        // Return the number of bytes needed to represent a region object of the specified size.
        //
        static const usword_t bytes_needed() {
            return align2(sizeof(Region), page_size_log2);
        }

        //
        // managed_size
        //
        // Return the number of bytes needed for N subzones.
        //
        static usword_t managed_size(usword_t nsubzones) { return (nsubzones << subzone_quantum_log2) + nsubzones * bitmaps_per_region * subzone_bitmap_bytes; }
        
        //
        // allocator
        //
        inline void *operator new(const size_t size) {
            // allocate admin data
            return allocate_memory(Region::bytes_needed());
        }



        //
        // deallocator
        //
        inline void operator delete(void *address) {
            Region *region = (Region *)address;
            
            // release subzone memory
           // XXX region->_zone->arena_deallocate(region->address(), region->size());
            
            // release C++ "Region::" data structure, including admin data
            deallocate_memory(region, Region::bytes_needed());
        }
       
      
        //
        // Constructor
        //
        Region(Zone *zone, void *address, usword_t size, usword_t nzones);
        
        
        //
        // Destructor
        //
        ~Region();


        //
        // Accessors
        //
        inline void set_next(Region *next)        { _next = next; }
        inline Region *next()               const { return _next; }
        inline Zone *zone()                 const { return _zone; }
        inline Range scan_space()           const { return _scan_space; }
        inline spin_lock_t *subzone_lock()        { return &_subzone_lock; }

        //
        //
        // number of subzones remaining
        //
        inline usword_t subzones_remaining() const { return _n_subzones - _i_subzones; }
        
        //
        // new_region
        //
        // Construct and initialize a new region.
        //
        static Region *new_region(Zone *zone );
        
        //
        // subzone_index
        //
        // Returns a regional subzone index for an arbitrary pointer.  Note that this is relative to region memory.  subzone_index in
        // Zone is absolute memory. 
        //
        inline const usword_t subzone_index(void *address) const { return (const usword_t)((uintptr_t)relative_address(address) >> subzone_quantum_log2); }
        
        
        //
        // subzone_size
        //
        // Returns the size of n subzones.
        //
        static inline usword_t subzone_size(const usword_t n) { return n << subzone_quantum_log2; }
        
        
        //
        // subzone_count
        //
        // Returns a number of subzone quantum for a given size.
        //
        static inline usword_t subzone_count(const size_t size) { return partition2(size, subzone_quantum_log2); }
        
        
        //
        // subzone_address
        //
        // Returns the address of the ith subzone
        //
        inline Subzone *subzone_address(const usword_t i) const { return (Subzone *)displace(address(), i << subzone_quantum_log2); }
        
        
        //
        // subzone_range
        //
        // Returns the range of active subzones.
        //
        inline Range subzone_range() const { return Range(address(), subzone_size(_i_subzones)); }
        

        //
        // add_subzone
        //
        // Add a new subzone to one of the admin.
        //
        bool add_subzone(Admin *admin);
        
        
        //
        // clear_all_pending
        //
        // Clear pending bits for the region.
        //
        inline void clear_all_pending() { _pending.clear(); }
        
        
        //
        // is_pending
        //
        // Used by subzone to get pending bit for quantum.
        //
        inline bool is_pending(usword_t q) { return (bool)_pending.bit(q); }
        
        
        //
        // clear_pending
        //
        // Used by subzone to clear pending bit for quantum.
        //
        inline void clear_pending(usword_t q) { _pending.clear_bit(q); }
        
        
        //
        // set_pending
        //
        // Used by subzone to set pending bit for quantum.
        //
        inline void set_pending(usword_t q) { _pending.set_bit(q); }
        
        //
        // set_mark
        //
        // Used by subzone to set mark bit for quantum.
        //
        inline void set_mark(usword_t q) { _marks.set_bit(q); }
        
        //
        // is_marked
        //
        // Used by subzone to get mark bit for quantum.
        //
        inline bool is_marked(usword_t q) { return (bool)_marks.bit(q); }

        //
        // clear_mark
        //
        // Used by subzone to clear mark bit for quantum.
        //
        inline void clear_mark(usword_t q) { _marks.clear_bit(q); }
        
        //
        // test_set_mark
        //
        // Used by subzone to test and set mark bit for quantum.
        //
        inline bool test_set_mark(usword_t q) {
            bool old = (bool)_marks.bit(q);
            if (!old) _marks.set_bit(q);
            return old;
        }
        
        //
        // clear_all_marks
        //
        // Used at the end of collection
        //
        inline void clear_all_marks() { _marks.clear(); }

        //
        // malloc_statistics
        //
        // Adds the region's memory use to stats.
        //
        void malloc_statistics(malloc_statistics_t *stats);

    };
    
    
};


#endif // __AUTO_REGION__
