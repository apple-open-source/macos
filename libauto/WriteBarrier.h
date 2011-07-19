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
    WriteBarrier.h
    Write Barrier for Generational GC
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_WRITE_BARRIER__
#define __AUTO_WRITE_BARRIER__

#include "Configuration.h"
#include "Definitions.h"
#include "Range.h"


namespace Auto {

    
    //
    // Forward declarations
    //
    class Zone;
    

    //----- WriteBarrier -----//

    class WriteBarrier : public Range {
    
      private:
      
        void           *_base;                              // base address of managed range
        usword_t       _protect;                            // protected space to base of write barrier
        
      public:

        //
        // card values
        //
        enum {
            card_unmarked = 0,
            card_marked_untouched = 0x1,
            card_marked = 0x3
        };
      
      
        //
        // Constructor
        //
        // Set up cached allocation of use.
        //
        WriteBarrier(void *base, void *address, const usword_t size, const usword_t protect = 0)
            : Range(address, size), _base(base), _protect(protect)
        {
        }

        
        //
        // bytes_needed
        //
        // Returns the number of write barrier bytes needed to represent 'n' actual bytes.
        //
        static inline const usword_t bytes_needed(usword_t n) {
            return partition2(n, write_barrier_quantum_log2);
        }
        
        
        //
        // card_index
        //
        // Return the write barrier card index for the specified address.
        //
        inline const usword_t card_index(void *address) const {
            uintptr_t normalized = (uintptr_t)address - (uintptr_t)_base;
            usword_t i = normalized >> write_barrier_quantum_log2;
            ASSERTION(_protect <= i);
            ASSERTION(i < size());
            return i;
        }
        
        //
        // contains_card
        //
        // Returns true if the specified address is managed by this write-barrier.
        //
        inline bool contains_card(void *address) {
            usword_t i = card_index(address);
            return (_protect <= i && i < size());
        }
        
        
        //
        // card_address
        //
        // Return the base address of the range managed by the specified card index.
        //
        inline void *card_address(usword_t i) const { return displace(_base, i << write_barrier_quantum_log2); }
        
        
        //
        // is_card_marked
        //
        // Test to see if card i is marked.
        //
        inline bool is_card_marked(usword_t i) { return ((unsigned char *)address())[i] != card_unmarked; }
        
        
        //
        // mark_card
        //
        // Marks the card at index i.
        //
        inline void mark_card(usword_t i) {
            ((unsigned char *)address())[i] = card_marked;
        }

        //
        // mark_cards_untouched
        //
        // Used by the write-barrier repair algorithm. Transitions all cards that are currently marked from card_marked -> card_marked_untouched.
        //
        usword_t mark_cards_untouched();
        
        //
        // clear_untouched_cards
        //
        // Used by the write-barrier repair algorithm. Uses compare and swap to effect the transition card_marked_untouched -> card_unmarked.
        //
        usword_t clear_untouched_cards();
        
        
        //
        // is_card_marked
        //
        // Checks to see if the card corresponding to .
        //
        inline bool is_card_marked(void *address) {
            usword_t i = card_index(address);
            return is_card_marked(i);
        }
        
        
        //
        // mark_card
        //
        // Mark the write barrier card for the specified address.
        //
        inline void mark_card(void *address) {
            const usword_t i = card_index(address);
            mark_card(i);
        }
        
        
        //
        // mark_cards
        //
        // Mark the write barrier cards corresponding to the specified address range.
        //
        inline void mark_cards(void *address, const usword_t size) {
            usword_t i = card_index(address);
            const usword_t j = card_index(displace(address, size - 1));
            for ( ; i <= j; i++) mark_card(i);
        }

        
        //
        // scan_marked_ranges
        //
        // Scan ranges in block that are marked in the write barrier.
        //
#ifdef __BLOCKS__
        typedef void (^write_barrier_scanner_t) (const Range&, WriteBarrier*);
#else
        class write_barrier_scanner {
        public:
            virtual void operator() (const Range &range, WriteBarrier *wb) = 0;
        };
        typedef write_barrier_scanner &write_barrier_scanner_t;
#endif

        void scan_marked_ranges(void *address, const usword_t size, write_barrier_scanner_t scanner);
        void scan_marked_ranges(void *address, const usword_t size, void (*scanner) (const Range&, WriteBarrier*, void*), void *arg);
        
        
        //
        // range_has_marked_cards
        //
        // Returns true if range intersects a range that has cards marked.
        //
        bool range_has_marked_cards(void *address, const usword_t size);
    };

    
};


#endif // __AUTO_WRITE_BARRIER__

