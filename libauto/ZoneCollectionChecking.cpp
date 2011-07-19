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
    ZoneCollectionChecking
    Copyright (c) 2010-2001 Apple Inc. All rights reserved.
 */

#include "auto_zone.h"
#include "Zone.h"
#include "BlockIterator.h"
#ifdef __BLOCKS__
#include <Block.h>
#endif

using namespace Auto;

void Zone::enable_collection_checking() {
    OSAtomicIncrement32(&_collection_checking_enabled);
}


void Zone::disable_collection_checking() {
    uint32_t old_count;
    do {
        old_count = _collection_checking_enabled;
    } while (old_count > 0 && !OSAtomicCompareAndSwap32(old_count, old_count-1, &_collection_checking_enabled));
    
    if (old_count == 1) {
        // Reset the check counts of all blocks.
        
        for (Region *region = region_list(); region != NULL; region = region->next()) {
            SubzoneRangeIterator iterator(region->subzone_range());
            Subzone *sz;
            for (sz = iterator.next(); sz != NULL; sz = iterator.next()) {
                sz->reset_collection_checking();
            }
        }
        
        SpinLock lock(&_large_lock);
        Large *large = _large_list;
        while (large) {
            large->set_collection_checking_count(0);
            large = large->next();
        }
    }
}


void Zone::track_pointer(void *pointer) {
    assert(collection_checking_enabled());
    if (in_subzone_memory(pointer)) {
        Subzone *sz = Subzone::subzone(pointer);
        usword_t q;
        if (sz->block_is_start(pointer, &q)) {
            if (sz->collection_checking_count(q) == 0) {
                sz->set_collection_checking_count(q, 1);
            }
        }
    } else {
        Large *large = block_start_large(pointer);
        if (large && large->collection_checking_count() == 0) {
            large->set_collection_checking_count(1);
        }
    }
}


// Clears the checking count for the blocks in the garbage list.
void Zone::clear_garbage_checking_count(void **garbage, size_t count) {
    for (size_t i=0; i<count; i++) {
        void *block = garbage[i];
        if (in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            // most of the time the checking count is zero already, so don't dirty the page needlessly
            if (subzone->collection_checking_count(q) > 0) {
                subzone->set_collection_checking_count(q, 0);
            }
        } else {
            Large *l = Large::large(block);
            l->set_collection_checking_count(0);
        }
    }
}


//
// checking_blocks_visitor
//
// checking_blocks_visitor searches for blocks with check counts that exceed the collection checking threshold.
// Blocks which it finds are reported via report_uncollected_block().
//
class update_checking_count_visitor  {
    
public:
    
    inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
        uint32_t count = subzone->collection_checking_count(q);
        if (count > 0) {
            subzone->set_collection_checking_count(q, count+1);
        }
        return true;
    }
    
    inline bool visit(Zone *zone, Large *large) {
        uint32_t count = large->collection_checking_count();
        if (count > 0) {
            large->set_collection_checking_count(count+1);
        }
        return true;
    }
};


void Zone::increment_check_counts() {
    update_checking_count_visitor visitor;
    visitAllocatedBlocks(this, visitor);
}

//
// report_uncollected_blocks_visitor
//
// report_uncollected_blocks_visitor searches for blocks with check counts that exceed the collection checking threshold.
// Blocks which it finds are reported via the callback, or just logged.
//
class report_uncollected_blocks_visitor  {
    auto_zone_collection_checking_callback_t _callback;
    Thread &_thread;
    
public:
    
    report_uncollected_blocks_visitor(Zone *zone, auto_zone_collection_checking_callback_t callback) : _callback(callback), _thread(zone->registered_thread()) {
    }
    
    ~report_uncollected_blocks_visitor() {
    }
    
    void report_uncollected_block(Zone *zone, void *block, int32_t count) {
        auto_memory_type_t layout = zone->block_layout(block);
        if (!_callback) {
            char *name;
            bool free_name = false;
            if ((layout & AUTO_OBJECT) == AUTO_OBJECT) {
                if (zone->control.name_for_address) {
                    name = zone->control.name_for_address((auto_zone_t *)this, (vm_address_t)block, 0);
                    free_name = true;
                } else {
                    name = (char *)"object";
                }
            } else {
                name = (char *)"non-object block";
            }
            malloc_printf("%s %p was not collected after %d full collections\n", name, block, count-1);
            if (free_name) free(name);
        } else {
            auto_zone_collection_checking_info info;
            info.is_object = (layout & AUTO_OBJECT) == AUTO_OBJECT;
            info.survived_count = count-1;
            _callback(block, &info);
        }
    }
    
    inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
        uint32_t count = subzone->collection_checking_count(q);
        if (count > 0) {
            report_uncollected_block(zone, subzone->quantum_address(q), count);
        }
        return true;
    }
    
    inline bool visit(Zone *zone, Large *large) {
        uint32_t count = large->collection_checking_count();
        if (count > 0) {
            report_uncollected_block(zone, large->address(), count);
        }
        return true;
    }
};

void Zone::enumerate_uncollected(auto_zone_collection_checking_callback_t callback) {
    dispatch_sync(_collection_queue, ^{
        report_uncollected_blocks_visitor visitor(this, callback);
        visitAllocatedBlocks(this, visitor);
    });
}
