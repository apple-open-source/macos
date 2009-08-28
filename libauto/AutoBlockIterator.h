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
    AutoBlockIterator.h
    Template Functions/Classes to visit all blocks in the GC heap.
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_BLOCKITERATOR__
#define __AUTO_BLOCKITERATOR__


#include "AutoAdmin.h"
#include "AutoDefs.h"
#include "AutoLarge.h"
#include "AutoRegion.h"
#include "AutoZone.h"


namespace Auto {

    //----- BlockIterator -----//
    
    //
    // Visit all allocated blocks.
    //
    
    template <class Visitor> bool visitAllocatedBlocks(Zone *zone, Visitor& visitor) {
        // iterate through the regions first
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            // iterate through the subzones
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.next()) {
                // get the number of quantum in the subzone
                usword_t n = subzone->allocation_limit();
                
                for (usword_t q = 0; q < n; q = subzone->next_quantum(q)) {
                    if (!subzone->is_free(q)) {
                        visitor.visit(zone, subzone, q);
                    }
                }
            }
        }

        // iterate through the large blocks
        for (Large *large = zone->large_list(); large != NULL; large = large->next()) {
            // let the visitor visit the write barrier
            if (!visitor.visit(zone, large)) return false;
        }

        return true;
    }

    //----- AllBlockIterator -----//
    
    //
    // Visit all the blocks including free blocks.
    //
    
    template <class Visitor> bool visitAllBlocks(Zone *zone, Visitor& visitor) {
        // iterate through the regions first
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            // iterate through the subzones
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.next()) {
                // get the number of quantum in the subzone
                usword_t n = subzone->allocation_limit();
                
                for (usword_t q = 0; q < n; q = subzone->next_quantum(q)) {
                    if (!visitor.visit(zone, subzone, q)) return false;
                }
            }
        }

        // iterate through the large
        for (Large *large = zone->large_list(); large != NULL; large = large->next()) {
            // let the visitor visit the write barrier
            if (!visitor.visit(zone, large)) return false;
        }

        return true;
    }
};

#endif // __AUTO_BLOCKITERATOR__

