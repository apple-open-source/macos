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
    AutoRange.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_RANGE__
#define __AUTO_RANGE__


#include "AutoDefs.h"


namespace Auto {

    //----- Range -----//

    //
    // Manage an area of memory
    //

    class Range {

      private:

        void *_address;                                     // start of range
        void *_end;                                         // end of the range (one byte beyond last usable space)
      
      public:
      
        //
        // Constructors
        //
        Range()                             : _address(NULL),    _end(NULL) {}
        Range(void *address)                : _address(address), _end(address) {}
        Range(void *address, void *end)     : _address(address), _end(end) {}
        Range(void *address, usword_t size) : _address(address), _end(displace(address, size)) {}
        
        
        //
        // Accessors
        //
        inline       Range&   range()                                       { return *this; }
        inline       void     *address()                              const { return _address; }
        inline       void     *end()                                  const { return _end; }
        inline const usword_t size()                                  const { return (uintptr_t)_end - (uintptr_t)_address; }
        inline       void     set_address(void *address)                    { _address = address; }
        inline       void     set_end(void *end)                            { _end = end; }
        inline       void     set_size(usword_t size)                       { _end = displace(_address, size); }
        inline       void     set_range(void *address, void *end)           { _address = address; _end = end; }
        inline       void     set_range(void *address, usword_t size)       { _address = address; _end = displace(address, size); }
        inline       void     adjust_address(intptr_t delta)                { _address = displace(_address, delta); }
        inline       void     adjust_end(intptr_t delta)                    { _end = displace(_end, delta); }
        inline       void     adjust(intptr_t delta)                        { _address = displace(_address, delta), _end = displace(_end, delta); }
        
        
        //
        // is_empty
        //
        // Returns true if the range is empty.
        //
        inline bool is_empty() { return _address == _end; }

        
        //
        // in_range
        //
        // Returns true if the specified address is in range.
        // This form reduces the number of branches.  Works well with invariant lo and hi.
        //
        static inline const bool in_range(void *lo, void *hi, void *address) {
            uintptr_t lo_as_int = (uintptr_t)lo;
            uintptr_t hi_as_int = (uintptr_t)hi;
            uintptr_t diff = hi_as_int - lo_as_int;
            uintptr_t address_as_int = (uintptr_t)address;
            return (address_as_int - lo_as_int) < diff;
        }
        inline const bool in_range(void *address) const { return in_range(_address, _end, address); }
        
        
        //
        // operator ==
        //
        // Used to locate entry in list or hash table (use is_range for exaxt match.)
        inline const bool operator==(const Range *range)  const { return _address == range->_address; }
        inline const bool operator==(const Range &range)  const { return _address == range._address; }
        
        //
        // is_range
        //
        // Return true if the ranges are equivalent.
        //
        inline const bool is_range(const Range& range) const { return _address == range._address && _end == range._end; }
        
        
        //
        // clear
        //
        // Initialize the range to zero.
        //
        inline void clear() { bzero(address(), size()); }
        

        //
        // expand_range
        //
        // Expand the bounds with the specified range.
        //
        inline void expand_range(void *address) {
            if (_address > address) _address = address;
            if (_end < address) _end = address;
        }
        inline void expand_range(Range& range) {
            expand_range(range.address());
            expand_range(range.end());
        }
                
        
        //
        // relative_address
        //
        // Converts an absolute address to an address relative to this address.
        //
        inline void *relative_address(void *address) const { return (void *)((uintptr_t)address - (uintptr_t)_address); }

        
        //
        // absolute_address
        //
        // Converts an address relative to this address to an absolute address.
        //
        inline void *absolute_address(void *address) const { return (void *)((uintptr_t)address + (uintptr_t)_address); }
        
        
    };
        
};

#endif // __AUTO_RANGE__

