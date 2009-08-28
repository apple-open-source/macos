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
    AutoInUseEnumerator.cpp
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoBlockIterator.h"
#include "AutoDefs.h"
#include "AutoInUseEnumerator.h"
#include "AutoLarge.h"
#include "AutoZone.h"


namespace Auto {

    //----- InUseEnumerator -----//


    //
    // scan
    //
    // Scan through a task's auto zone looking for 
    //
    kern_return_t InUseEnumerator::scan() {
        // calculate the zone size 
        usword_t zone_size = Zone::bytes_needed();
        // read in zone information
        Zone *zone = (Zone *)read((void *)_zone_address, zone_size);
        // check for successful read
        if (!zone) return _error;
        
        // Don't record the zone object, otherwise objects allocated by the collector appear as leaks. <rdar://problem/6747283>
        // record((void *)_zone_address, zone_size, MALLOC_ADMIN_REGION_RANGE_TYPE);
        
        // record the garbage list.
        PointerList& garbage_list = zone->garbage_list();
        record((void*)garbage_list.buffer(), garbage_list.size(), MALLOC_ADMIN_REGION_RANGE_TYPE);
        
        // iterate though each of the regions
        Region *region = zone->region_list();
        while (region != NULL) {
            // load the base of the region structure
        	Region *regionReader = (Region *)read(region, Region::bytes_needed());
            // check for successful read
            if (!regionReader) return _error;
             
            // record the region object
            record(region, Region::bytes_needed(), MALLOC_ADMIN_REGION_RANGE_TYPE);
            // record the region range
            record(regionReader->address(), regionReader->size(), MALLOC_PTR_REGION_RANGE_TYPE);

            if (_type_mask & (MALLOC_ADMIN_REGION_RANGE_TYPE | MALLOC_PTR_IN_USE_RANGE_TYPE | AUTO_RETAINED_BLOCK_TYPE)) {
                // iterate through the subzones
                SubzoneRangeIterator iterator(regionReader->subzone_range());
                while (Subzone *subzone = iterator.next()) {
                    // map in the subzone header
                    Subzone *subzoneReader = (Subzone *)read(subzone, sizeof(Subzone));
                    record(subzone, subzoneReader->base_data_size(), MALLOC_ADMIN_REGION_RANGE_TYPE);
                    
                    if (_type_mask & (MALLOC_PTR_IN_USE_RANGE_TYPE | AUTO_RETAINED_BLOCK_TYPE)) {
                        // map in the subzone + side data when enumerating blocks. no need to map the blocks themselves.
                        subzoneReader = (Subzone *)read(subzone, subzoneReader->base_data_size());
                        usword_t n = subzoneReader->allocation_limit();
                        MemoryReader reader(_task, _reader);
                        for (usword_t q = 0; q < n; q = subzoneReader->next_quantum(q, reader)) {
                            if (!subzoneReader->is_free(q) && !subzoneReader->is_local_garbage(q)) {
                                unsigned type = MALLOC_PTR_IN_USE_RANGE_TYPE;
                                if (subzoneReader->refcount(q) != 0)
                                    type |= AUTO_RETAINED_BLOCK_TYPE;
                                record(subzoneReader->quantum_address(q), subzoneReader->size(q), type);
                            }
                        }
                    }
                }
            }
            region = regionReader->next();   
        } 

        // iterate through the large
        for (Large *large = zone->large_list(); large; large = large->next()) {
            record(large, sizeof(Large), MALLOC_ADMIN_REGION_RANGE_TYPE);
            Large *largeReader = (Large *)read(large, sizeof(Large));
            unsigned type = MALLOC_PTR_IN_USE_RANGE_TYPE;
            if (largeReader->refcount() != 0)
                type |= AUTO_RETAINED_BLOCK_TYPE;
            record(large->address(), largeReader->size(), type);
            record(displace(large, sizeof(Large) + largeReader->size()), largeReader->vm_size() - (sizeof(Large) + largeReader->size()), MALLOC_ADMIN_REGION_RANGE_TYPE);
            large = largeReader;
        }
        
        return KERN_SUCCESS;
    }

};
