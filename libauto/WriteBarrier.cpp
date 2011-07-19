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
    WriteBarrier.cpp
    Write Barrier for Generational GC
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#include "Configuration.h"
#include "Definitions.h"
#include "Range.h"
#include "WriteBarrier.h"
#include "Zone.h"


namespace Auto {

    //----- WriteBarrier -----//
    
    
    //
    // scan_marked_ranges
    //
    // Scan ranges in block that are marked in the write barrier.
    //
    void WriteBarrier::scan_marked_ranges(void *address, const usword_t size, write_barrier_scanner_t scanner) {
        // determine the end address
        void *end = displace(address, size);
        // determine the last used address
        void *last = displace(address, size - 1);
        // get the write barrier index for the begining of the block
        usword_t i = card_index(address);
        // get the write barrier index for the end of the block
        const usword_t j = card_index(last);
        
        Range sub_range;
        while (true) {
            // skip over unmarked ranges
            for ( ; i <= j && !is_card_marked(i); i++) {}
            
            // if no marks then we are done
            if (i > j) break;
            
            // scan the marks
            usword_t k = i;
            for ( ; i <= j && is_card_marked(i); i++) {}
            
            // set up the begin and end of the marked range
            void *range_begin = card_address(k);
            void *range_end = card_address(i);
            
            // truncate the range to reflect address range
            if (range_begin < address) range_begin = address;
            if (range_end > end) range_end = end;

            // scan range
            sub_range.set_range(range_begin, range_end);
            scanner(sub_range, this);
        }
    }

#ifndef __BLOCKS__
    class write_barrier_scanner_helper : public WriteBarrier::write_barrier_scanner {
        void (*_scanner) (Range&, WriteBarrier*, void*);
        void *_arg;
    public:
        write_barrier_scanner_helper(void (*scanner) (const Range&, WriteBarrier*, void*), void *arg) : _scanner(scanner), _arg(arg) {}
        virtual void operator() (const Range &range, WriteBarrier *wb) { _scanner(range, wb, _arg); }
    };
#endif

    void WriteBarrier::scan_marked_ranges(void *address, const usword_t size, void (*scanner) (const Range&, WriteBarrier*, void*), void *arg) {
#ifdef __BLOCKS__
        scan_marked_ranges(address, size, ^(const Range &range, WriteBarrier *wb) { scanner(range, wb, arg); });
#else
        write_barrier_scanner_helper helper(scanner, arg);
        scan_marked_ranges(address, size, helper);
#endif
    }

    bool WriteBarrier::range_has_marked_cards(void *address, const usword_t size) {
        void *last = displace(address, size - 1);
        // get the write barrier index for the begining of the block
        usword_t i = card_index(address);
        // get the write barrier index for the end of the block
        const usword_t j = card_index(last);
        while (i <= j) if (is_card_marked(i++)) return true;
        return false;
    }
    
    inline bool compare_and_swap(unsigned char *card, unsigned char old_value, unsigned char new_value) {
#if defined(__arm__)
        // <rdar://problem/7001590> FIXME:  use LDREX/STREX.
        if (*card == old_value) {
            *card = new_value;
            return true;
        }
        return false;
#else
        return __sync_bool_compare_and_swap(card, old_value, new_value);
#endif
    }

    // this should only called from Zone::mark_write_barriers_untouched().
    usword_t WriteBarrier::mark_cards_untouched() {
        usword_t count = 0;
        for (unsigned char *card = (unsigned char*)address() + _protect, *limit = (unsigned char *)end(); card != limit; ++card) {
            if (*card != card_unmarked) {
                if (compare_and_swap(card, (unsigned char)card_marked, (unsigned char)card_marked_untouched))
                    ++count;
            }
        }
        return count;
    }
    
    // this should only called from Zone::clear_untouched_write_barriers().
    usword_t WriteBarrier::clear_untouched_cards() {
        usword_t count = 0;
        for (unsigned char *card = (unsigned char*)address() + _protect, *limit = (unsigned char *)end(); card != limit; ++card) {
            if (*card == card_marked_untouched) {
                if (compare_and_swap(card, (unsigned char)card_marked_untouched, (unsigned char)card_unmarked))
                    ++count;
            }
        }
        return count;
    }
};
