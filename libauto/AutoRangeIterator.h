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
    AutoRangeIterator.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_RANGEITERATOR__
#define __AUTO_RANGEITERATOR__


#include "AutoDefs.h"


namespace Auto {


    //----- RangeIterator -----//
    
    //
    // Iterate over a range of memory
    //
    
    template <class T> class RangeIterator : public Range {

      public:
        
        //
        // Constructors
        //
        RangeIterator(void *address, const usword_t size)
        : Range(address, size)
        {}
        
        RangeIterator(void *address, void *end)
        : Range(address, end)
        {}
        
        RangeIterator(Range &range)
        : Range(range)
        {}
        
        
        //
        // next
        //
        // Returns next entry in the range or NULL if no more entries available.
        //
        inline T *next() {
            // if cursor is still in range
            if (address() < end()) {
                // capture cursor position
                T *_next = (T *)address();
                // advance for next call
                set_address((void *)(_next + 1));
                // return captured cursor position
                return _next;
            }
            
            // at end
            return NULL;
        }
        
    };


};

#endif // __AUTO_RANGEITERATOR__

