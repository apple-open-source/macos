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
    BlockIterator.h
    Template Functions to visit all blocks in the GC heap.
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_BLOCK_ITERATOR__
#define __AUTO_BLOCK_ITERATOR__


#include "Admin.h"
#include "Definitions.h"
#include "Large.h"
#include "Region.h"
#include "Zone.h"
#include "BlockRef.h"

namespace Auto {

    //
    // visitAllocatedBlocks
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
    
    //
    // ConcurrentVisitorHelper
    //
    // Helper class for visitAllocatedBlocks_concurrent that maintains state to drive the enumeration.
    // Visitor must provide:
    //    const boolean_t visitLargesConcurrently const() - return true if larges are heavyweight and should be processed one at a time, false if the entire large list should be treated as one work unit. The visit function will be called once for each large either way.
    //    void visit(Zone *, Large *) - visit a Large
    //    const boolean_t visit_subzone_quanta() const - return true if the visitor wants to iterate over subzone quanta, or false if it just wants to get the subzone
    //    void visit(Zone *, Subzone *, usword_t) - called if visit_subzone_quanta() returns true
    //    void visit(Zone *, Subzone *) - called if visit_subzone_quanta() returns false
    //

    template <class Visitor> class ConcurrentVisitorHelper {
        Zone *_zone;
        Visitor &_visitor;
        Large *_current_large;          // the next large to visit
        Region *_current_region;        // the region currently being visited
        SubzoneRangeIterator _iterator; // subzone iterator for _current_region
        spin_lock_t _lock;              // protects _current_large, _current_region, and _iterator

    private:
        
        // These members invoke the visitor functions. They are separated out so the visitor members can be inlined into them
        // without requiring logic that picks the Large/Subzone to be customized by the Visitor.
        
        // Visit a Large. If the visitor wants to process them all at once, visit them all.
        void visit_large(Large *large_to_visit) {
            do {
                _visitor.visit(_zone, large_to_visit);
                large_to_visit = _visitor.visit_larges_concurrently() ? NULL : large_to_visit->next();
            } while (large_to_visit != NULL);
        }
        
        // Visit the subzone.
        void visit_subzone(Subzone *subzone_to_visit) {
            _visitor.visit(_zone, subzone_to_visit);
        }
        
    public:
        
        //
        // Constructor
        //
        // The constructor sets up the initial state for the visit function.
        //
        ConcurrentVisitorHelper(Zone *zone, Visitor &visitor) : _zone(zone), _visitor(visitor), _current_large(zone->large_list()), _current_region(zone->region_list()), _iterator(_current_region->subzone_range()), _lock(0) {}
        
        //
        // visit
        //
        // Implements a state machine to pick the next piece of work and call the visitor with it.
        //
        inline boolean_t visit(boolean_t is_dedicated, boolean_t work_to_completion) {
            boolean_t did_work;
            do {
                did_work = false;
                Large *large_to_visit = NULL;
                Subzone *subzone_to_visit = NULL;
                {
                    // Hold the lock while picking out a large/subzone to visit
                    SpinLock lock(&_lock);
                    if (_current_large != NULL) {
                        large_to_visit = _current_large;
                        if (_visitor.visit_larges_concurrently()) {
                            _current_large = _current_large->next();
                        } else {
                            // the visitor wants to handle all larges at once so clear out the list
                            _current_large = NULL;
                        }
                        did_work = true;
                    }
                    // if no large, look for a subzone
                    if (large_to_visit == NULL) {
                        subzone_to_visit = _iterator.next();
                        if (!subzone_to_visit && _current_region) {
                            // the current region is done, proceed to the next
                            _current_region = _current_region->next();
                            if (_current_region) {
                                _iterator = SubzoneRangeIterator(_current_region->subzone_range());
                                subzone_to_visit = _iterator.next();
                            }
                        }
                    }                        
                }
                
                if (large_to_visit) visit_large(large_to_visit);
                
                // If we found a subzone, visit all the allocated blocks.
                if (subzone_to_visit != NULL) {
                    did_work = true;
                    visit_subzone(subzone_to_visit);
                }
            } while (did_work && work_to_completion);
            return did_work;
        }
        
        //
        // visitor_wrapper
        //
        // wraps the call to visitor, to interface with the zone worker api
        //
        static boolean_t visitor_wrapper(void *arg, boolean_t is_dedicated, boolean_t work_to_completion) {
            ConcurrentVisitorHelper<Visitor> *helper = (ConcurrentVisitorHelper *)arg;
            return helper->visit(is_dedicated, work_to_completion);
        }
    };
    
    //
    // visitAllocatedBlocks_concurrent
    //
    // Visit all allocated blocks in a concurrent manner using multiple worker threads.
    // Each subzone is visited separately, and larges may be visited individually or all at once depending on the Visitor.
    // Refer to ConcurrentVisitorHelper for what Visitor must implement.
    //
    template <class Visitor> void visitAllocatedBlocks_concurrent(Zone *zone, Visitor& visitor) {
        ConcurrentVisitorHelper<Visitor> helper(zone, visitor);
        zone->perform_work_with_helper_threads(ConcurrentVisitorHelper<Visitor>::visitor_wrapper, &helper);
    }
    
    //
    // visitPendingBlocks
    //
    // Visit all pending blocks.
    //
    
    template <class Visitor> void visitPendingBlocks(Zone *zone, Visitor& visitor) {
        // iterate through the regions first
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            // iterate through the subzones
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.next()) {
                // get the number of quantum in the subzone
                usword_t n = subzone->allocation_limit();
                
                for (usword_t q = 0; q < n; q = subzone->next_quantum(q)) {
                    if (!subzone->is_free(q) && subzone->test_and_clear_pending(q)) {
                        visitor(subzone, q);
                    }
                }
            }
        }

        // iterate through the large blocks
        for (Large *large = zone->large_list(); large != NULL; large = large->next()) {
            // let the visitor visit the write barrier
            if (large->is_pending()) {
                large->clear_pending();
                visitor(large);
            }
        }
    }

    //
    // visitAllBlocks
    //
    // Visit all the blocks including free blocks.
    //
    // BlockRef FIXME: block visitors should hand out BlockRefs
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
    
    //
    // visitAssociationsNoLock
    //
    // Visits all known associative references. Assumes _associations_lock is held.
    //
    
    template <class Visitor> bool visitAssociationsNoLock(Zone *zone, Visitor& visitor) {
        AssociationsHashMap &associations(zone->associations());
        for (AssociationsHashMap::iterator i = associations.begin(), iend = associations.end(); i != iend; i++) {
            void *block = i->first;
            ObjectAssociationMap *refs = i->second;
            for (ObjectAssociationMap::iterator j = refs->begin(), jend = refs->end(); j != jend; j++) {
                ObjectAssociationMap::value_type &pair = *j;
                if (!visitor(zone, block, pair.first, pair.second)) return false;
            }
        }
        return true;
    }
    
    //
    // visitRootsNoLock
    //
    // Visits all registered roots. Assumes _roots_lock is held.
    //
    
    template <typename Visitor> bool visitRootsNoLock(Zone *zone, Visitor visitor) {
        PtrHashSet &roots(zone->roots());
        for (PtrHashSet::const_iterator i = roots.begin(), end = roots.end(); i != end; ++i) {
            if (!visitor((void **)*i)) return false;
        }
        return true;
    }
    
    //
    // blockDo
    //
    // Applies an operation to a block, calling the appropriate operator() according
    // to the kind of block small/medium vs. large.
    //
    
    template <class BlockDo> void blockDo(Zone *zone, void *block, BlockDo &op) {
        if (zone->in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            usword_t q;
            if (subzone->block_is_start(block, &q)) op(subzone, q);
        } else if (zone->block_is_start_large(block)) {
            op(Large::large(block));
        }
    }
    
    inline void blockDo(Zone *zone, void *block, void (^subzoneDo) (Subzone *subzone, usword_t q), void (^largeDo) (Large *large), void (^elseDo) (void *block) = NULL) {
        if (zone->in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            usword_t q;
            if (subzone->block_is_start(block, &q)) subzoneDo(subzone, q);
        } else if (zone->block_is_start_large(block)) {
            largeDo(Large::large(block));
        } else if (elseDo) {
            elseDo(block);
        }
    }
    
    //
    // blockStartNoLockDo
    //
    // Applies either subzoneDo, or largeDo to the block address, depending on what kind of block it is. Takes no locks.
    // Used by compaction.
    //
    
    inline void blockStartNoLockDo(Zone *zone, void *address, void (^subzoneDo) (Subzone *subzone, usword_t q), void (^largeDo) (Large *large)) {
        if (zone->in_subzone_memory(address)) {
            Subzone *subzone = Subzone::subzone(address);
            usword_t q;
            if (subzone->block_start(address, q)) subzoneDo(subzone, q);
        } else if (zone->in_large_memory(address)) {
            Large *large = zone->block_start_large(address);
            if (large) largeDo(large);
        }
    }
};

#endif // __AUTO_BLOCK_ITERATOR__
