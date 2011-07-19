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
    WriteBarrierIterator.h
    Write Barrier Iteration of Subzones and Large blocks
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_WRITE_BARRIER_ITERATOR__
#define __AUTO_WRITE_BARRIER_ITERATOR__


#include "Admin.h"
#include "Definitions.h"
#include "Large.h"
#include "RangeIterator.h"
#include "Region.h"
#include "Subzone.h"
#include "WriteBarrier.h"
#include "Zone.h"


namespace Auto {

    //----- WriteBarrierIterator -----//
    
    //
    // Visit all the write barriers.
    //

    template <class Visitor> bool visitWriteBarriers(Zone *zone, Visitor &visitor) {
        // iterate through the regions first
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            // iterate through the subzones
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.next()) {
                 // extract the write barrier information
                WriteBarrier& wb = subzone->write_barrier();
                
                // let the visitor visit the write barrier
                if (!visitor.visit(zone, wb)) return false;
            }
        }

        // iterate through the large blocks list. we assume that either the large_lock() is held,
        // or that the collector has made large block deallocation lazy.
        for (Large *large = zone->large_list(); large != NULL; large = large->next()) {
            // skip unscanned large blocks, which have no write-barrier cards.
            if (!large->is_scanned()) continue;
            
            // extract the write barrier information
            WriteBarrier& wb = large->write_barrier();
            
            // let the visitor visit the write barrier
            if (!visitor.visit(zone, wb)) return false;
        }
        
        return true;
    }
    
};

#endif // __AUTO_WRITE_BARRIER_ITERATOR__

