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
    AutoWriteBarrier.cpp
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoConfiguration.h"
#include "AutoDefs.h"
#include "AutoRange.h"
#include "AutoMemoryScanner.h"
#include "AutoWriteBarrier.h"
#include "AutoZone.h"


namespace Auto {

    //----- WriteBarrier -----//
    
    
    //
    // scan_marked_ranges
    //
    // Scan ranges in block that are marked in the write barrier.
    //
    void WriteBarrier::scan_marked_ranges(void *address, const usword_t size, void (^scanner) (Range&, WriteBarrier*)) {
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

    void WriteBarrier::scan_marked_ranges(void *address, const usword_t size, MemoryScanner &scanner) {
#if RADAR_6545782_FIXED
        scan_marked_ranges(address, size, ^(Range &range, WriteBarrier *wb) { scanner.scan_range(range, wb); });
#else
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
            scanner.scan_range(sub_range, this);
        }
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

    // this should only called from Zone::mark_write_barriers_untouched().
    usword_t WriteBarrier::mark_cards_untouched() {
        usword_t count = 0;
        for (unsigned char *card = (unsigned char*)address() + _protect, *limit = (unsigned char *)end(); card != limit; ++card) {
            if (*card != card_unmarked) {
                if (__sync_bool_compare_and_swap(card, (unsigned char)card_marked, (unsigned char)card_marked_untouched))
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
                if (__sync_bool_compare_and_swap(card, (unsigned char)card_marked_untouched, (unsigned char)card_unmarked))
                    ++count;
            }
        }
        return count;
    }
};
