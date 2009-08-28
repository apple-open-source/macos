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
    AutoMemoryScanner.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_SCANMEMORY__
#define __AUTO_SCANMEMORY__

#include "AutoDefs.h"
#include "AutoRange.h"


namespace Auto {

    //
    // Forward declarations.
    //
    class Admin;
    class Subzone;
    class Thread;
    class Zone;
    class Large;
    class WriteBarrier;
    
    
    //----- MemoryScanner -----//
    
    //
    // Responsible for garbage collection.
    //
    
    class MemoryScanner {
    
      protected:
      
        Zone                      *_zone;                                   // zone being scanned
        void                      *_current_stack_bottom;                   // bottom of current stack
        
#if defined(__ppc__)
        vector unsigned int       _lo_vector;                               // vector containing lowest valid address x 4
        vector unsigned int       _diff_vector;                             // vector containing range of valid address - 1 x 4
#endif

        bool                      _use_pending;                             // use bitmap
        bool                      _some_pending;                            // indicates some blocks are pending scanning
        bool                      _is_partial;                              // true if this a partial (generational) scan.
        bool                      _is_collector;                            // true if this a collector subclass.
        bool                      _does_check_block;                        // detailed checking of block
        bool                      _should_coalesce;                         // should coalesce ranges
        bool                      _use_exact_scanning;                      // trust exact layouts when scanning
        Range                     _coalesced_range;                         // combination of consecutive ranges

        usword_t                  _amount_scanned;                          // amount of memory scanned (in bytes)
        usword_t                  _blocks_scanned;                          // number of blocks scanned 

#if defined(DEBUG)
        usword_t                  _blocks_checked;                          // number of blocks checked (block may be checked more than once)
#endif
      
      public:
      
        //
        // Constructor
        //
        MemoryScanner(Zone *zone, void *current_stack_bottom, bool does_check_block);
        virtual ~MemoryScanner() {}

        //
        // Accessors
        //
        inline bool use_pending()          { return _use_pending; }
        void set_use_pending(void)         { _use_pending = true; }
        inline bool is_partial()     const { return _is_partial; }
        void set_is_partial(bool b)        { _is_partial = b; }
        Zone *zone()                 const { return _zone; }
        void *current_stack_bottom() const { return _current_stack_bottom; }
        bool is_collector()          const { return _is_collector; }
        
        usword_t bytes_scanned()     const { return _amount_scanned; }
        usword_t blocks_scanned()    const { return _blocks_scanned; }
        

        //
        // Accessors for _some_pending.  Used for bitmap based scanning.
        //
        inline bool is_some_pending     () const { return _some_pending; }
        inline void set_some_pending    ()       { _some_pending = true; }
        inline void clear_some_pending  ()       { _some_pending = false; }

        //
        // scan_for_unmarked_blocks
        //
        // Scan block for references to unvisited blocks.
        //
        void scan_for_unmarked_blocks(Subzone *subzone, usword_t q, void *block);
        void scan_for_unmarked_blocks(Large *large, void *block);

        
        //
        // scan_object_range
        //
        // Scan block using optional layout map
        //
        void scan_object_range(Range &range, WriteBarrier *wb);
        

        //
        // scan_with_layout
        //
        // Scan block using supplied layout
        //
        void scan_with_layout(Range &block, const unsigned char* map, WriteBarrier *wb);
        

        //
        // scan_with_weak_layout
        //
        // Scan block using supplied (weak) layout
        //
        void scan_with_weak_layout(Range &block, const unsigned char* map, WriteBarrier *wb);
        
        
        //
        // set_pending
        //
        // Set the block as pending if is a block and has not been marked.  Sets
        // zone some_pending if not visited (marked) before.
        //
        void set_pending(void *block);


        //
        // repend
        //
        // Force a block to be rescanned.
        //
        void repend(void *address);


        //
        // check_block
        //
        // Purpose driven examination of block.
        // Override in subclass for task specific block check.
        //
        virtual void check_block(void **reference, void *block);


        //
        // associate_block
        //
        // Called during associative reference scanning, to indicate this block is associated
        // with reference via key.
        //
        virtual void associate_block(void **reference, void *key, void *block);
        

#if defined(__ppc__)
        //
        // isolate_altivec_vector
        //
        // Isolate potential pointers in vector.
        //
        inline vector unsigned int isolate_altivec_vector(vector unsigned int data_vector, vector unsigned int lo_vector, vector unsigned int diff_vector);


        //
        // check_altivec_vector
        //
        // Checks to see if an altivec vector contains pointers.
        //
        inline void check_altivec_vector(void **isolated_vector);
    
    
        //
        // scan_range_altivec
        //
        // Scans the specified aligned range for references to unmarked blocks using altivec.
        //
        void scan_range_altivec(void **reference, void **end);
#endif
        
        
        //
        // scan_range
        //
        // Scans the specified aligned range for references to unmarked blocks.
        //
        virtual void scan_range(const Range &range, WriteBarrier *wb = NULL);
        inline void scan_range(void *address, usword_t size, WriteBarrier *wb = NULL) { scan_range(Range(address, size), wb); }
        inline void scan_range(void *address, void *end, WriteBarrier *wb = NULL) { scan_range(Range(address, end), wb); }

        virtual void scan_external();

        //
        // scan_range_from_thread
        //
        // Scan a range of memory in the thread's stack.
        // Subclass may want to record context.
        //
        virtual void scan_range_from_thread(Range &range, Thread &thread);
        
        
        //
        // scan_range_from_registers
        //
        // Scan a range of memory containing an image of the thread's registers.
        // Subclass may want to record context.
        //
        virtual void scan_range_from_registers(Range &range, Thread &thread, int first_register);


        //
        // scan_retained_blocks
        //
        // Add all the retained blocks to the scanner.
        //
        virtual void scan_retained_blocks();


        //
        // scan_retained_and_old_blocks
        //
        // Add all the retained and old blocks to the scanner.
        //
        void scan_retained_and_old_blocks();
        
        
        //
        // scan_root_ranges
        //
        // Add all root ranges to the scanner.
        //
        void scan_root_ranges();
    

        //
        // scan_threads
        //
        // Scan all the stacks for unmarked references.
        //
        void scan_thread_ranges(bool withSuspend, bool includeCurrentThread);


        //
        // check_roots
        //
        // Scan root blocks.
        //
        virtual void check_roots();
        
        
        //
        // scan_pending_blocks
        //
        // Scan until there are no pending blocks
        //
        void scan_pending_blocks();
        
        
        //
        // scan_pending_until_done
        //
        // Scan through blocks that are marked as pending until there are no new pending.
        //
        void scan_pending_until_done();
        
        
        //
        // scan
        //
        // Scans memory for reachable objects.  All reachable blocks will be marked.
        //
        void scan();


        //
        // scan_barrier
        //
        // Used by collectors to synchronize with concurrent mutators.
        //
        virtual void scan_barrier();
    };

};

#endif // __AUTO_SCANMEMORY__

