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
    AutoLarge.cpp
    Large Block Support
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoLarge.h"
#include "AutoSubzone.h"
#include "AutoZone.h"


namespace Auto {

    //----- Large -----//
    
    /*
     The in_large bitmap tracks the starting quatum of large allocations.
     In 32-bit systems the quanta size is 64K; on 64 it is 128K.
     We allocate the # of pages needed to satisfy the request so as to leave
     small holes in the address space.  We allocate on a quantum boundary always.
     
     At the quantum boundary we establish the "Large" data structure.
     Rounding up to the next small_quanta boundary we provide that as the address
     of the allocation.  It is a constant.
     Only pointers that have the in_large bit set and that have an address of that rounded up
     small quanta size are in fact our pointers.
     
     Beyond the requested size (also rounded) are the write-barrier cards.
    */
    
    Large::Large(usword_t vm_size, usword_t size, usword_t layout, usword_t refcount, usword_t age, const WriteBarrier &wb)
        : _prev(NULL), _next(NULL), _vm_size(vm_size), _size(size), _layout(layout), _refcount(refcount), _age(age),
          _is_pending(false), _is_marked(false), _is_garbage(false), _write_barrier(wb)
    {
    }

    //
    // allocate
    //
    // Allocate memory used for the large block.
    //
    Large *Large::allocate(Zone *zone, const usword_t size, usword_t layout, bool refcount_is_one) {
        // determine the size of the block header
        usword_t header_size = side_data_size();
        
        // determine memory needed for allocation, guarantee minimum quantum alignment
        usword_t allocation_size = align2(size, allocate_quantum_small_log2);
        
        // determine the extra space to allocate for a guard page.
        usword_t guard_size = 0;
        if (Environment::guard_pages) {
            // allocate enough extra space to page-align the guard page.
            usword_t slop_size = align2(header_size + allocation_size, page_size_log2) - (header_size + allocation_size);
            guard_size = slop_size + page_size;
        }
        
        // determine memory for the write barrier, guarantee minimum quantum alignment
        usword_t wb_size = (layout & AUTO_UNSCANNED) ? 0 : align2(WriteBarrier::bytes_needed(allocation_size), allocate_quantum_small_log2);

        // determine total allocation
        usword_t vm_size = align2(header_size + allocation_size + guard_size + wb_size, page_size_log2);

        // allocate memory, construct with placement new.
        void *space = zone->arena_allocate_large(vm_size);
        if (!space) return NULL;
        
        if (Environment::guard_pages) {
            // protect the guard page
            void *guard_address = align_up(displace(space, header_size + allocation_size));
            guard_page(guard_address);
        }
        
        // construct the WriteBarrier here, to simplify the Large constructor.
        void *wb_base = wb_size ? displace(space, side_data_size()) : NULL; // start of area managed by the WriteBarrier itself.
        unsigned char* wb_cards = wb_size ? (unsigned char *)displace(space, header_size + allocation_size + guard_size) : NULL;
        WriteBarrier wb(wb_base, wb_cards, wb_size);
        return new (space) Large(vm_size, allocation_size, layout, refcount_is_one ? 1 : 0, initial_age, wb);
    }


    //
    // deallocate
    //
    // Release memory used by the large block.
    //
    void Large::deallocate(Zone *zone) {
        if (Environment::guard_pages) {
            // unprotect the guard page.
            usword_t header_size = side_data_size();
            void *guard_address = align_up(displace((void *)this, header_size + _size));
            unguard_page(guard_address);
        }
        
        // release large data
        zone->arena_deallocate((void *)this, _vm_size);
    }    
};
