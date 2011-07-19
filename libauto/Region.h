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
    Region.h
    Contiguous range of subzones.
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_REGION__
#define __AUTO_REGION__

#include "Admin.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Range.h"
#include "Statistics.h"
#include "Locks.h"

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
        Bitmap      _pinned;                                // bitmap for compaction

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

        inline Bitmap &pending()                  { return _pending; }
        inline Bitmap &marks()                    { return _marks; }
        inline Bitmap &pinned()                   { return _pinned; }

        //
        //
        // number of subzones remaining
        //
        inline usword_t subzones_remaining() const { return _n_subzones - _i_subzones; }

        inline usword_t active_subzones()    const { return _i_subzones; }
        
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
        inline Range subzone_range() { SpinLock lock(&_subzone_lock); return Range(address(), subzone_size(_i_subzones)); }
        

        //
        // add_subzone
        //
        // Add a new subzone to one of the admin.
        //
        bool add_subzone(Admin *admin);
        
        
        //
        // test_and_set_mark
        //
        // Used by subzone to test and set mark bit for quantum.
        //
        inline bool test_and_set_mark(usword_t q) {
            return (bool)_marks.test_set_bit_atomic(q);
        }


        //
        // test_and_clear_mark
        //
        // Used by subzone to test and clear mark bit for quantum.
        //
        inline bool test_and_clear_mark(usword_t q) {
            return (bool)_marks.test_clear_bit_atomic(q);
        }
        
        
        //
        // clear_marks
        //
        // Used at the end of collection
        //
        inline void clear_marks() {
            _marks.clear();
        }
    };
    
    
};


#endif // __AUTO_REGION__
