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
 *  AutoThreadLocalCollector.cpp
 *  auto
 *
 *  Created by Patrick Beard on 9/18/2008
 *  Copyright 2008-2009 Apple Inc. All rights reserved.
 *
 */

#include "AutoThreadLocalCollector.h"
#include "auto_trace.h"
#include "auto_dtrace.h"

namespace Auto {

    void ThreadLocalCollector::scan_local_block(void *startingBlock) {
        SimplePointerBuffer scannedBlocks;
        set_marked_blocks_buffer(&scannedBlocks);
        
        void *block = startingBlock;
        do {
            int32_t index = _localBlocks.slotIndex(block);
            if (!_localBlocks.wasScanned(index)) {
                Subzone *subzone = Subzone::subzone(block);
                Range range(block, subzone->size(block));
                const unsigned char *map = (subzone->layout(block) & AUTO_OBJECT) ? _zone->layout_map_for_block(block) : NULL;
                if (map)
                    scan_with_layout(range, map, NULL); // TLC doesn't need the write barrier.
                else
                    scan_range(range, NULL);
                if (index != -1) {
                    _localBlocks.setScanned(index);
                    _localBlocks.setMarked(index);
                }
            }
            
            bool shouldScan;
            do {
                block = scannedBlocks.pop();
                if (block) {
                    Subzone *rsz = Subzone::subzone(block);
                    shouldScan = rsz->should_scan_local_block(block);
                }
            } while (block && !shouldScan);
        } while (block != NULL);
    }
    
    //
    // scan_marked_blocks
    //
    void ThreadLocalCollector::scan_marked_blocks() {
        for (uint32_t i = _localBlocks.firstOccupiedSlot(), last = _localBlocks.lastOccupiedSlot(); i <= last; i++) {
            void *block = _localBlocks.markedUnscannedPointerAtIndex(i);
            if (block) {
                Subzone *subzone = Subzone::subzone(block);
                if (subzone->should_scan_local_block(block)) {
                    scan_local_block(block);
                }
            }
        }
    }

    struct garbage_list {
        Zone *zone;
        size_t count;
        vm_address_t garbage[0];
    };

#if DEBUG    
    static size_t blocks_scavenged_locally, bytes_scavenged_locally;
#endif

    //
    // scavenge_local
    //
    // we can't return to the general pool because the general collector thread may
    // still be scanning us.  Instead we return data to our cache.
    //
    void ThreadLocalCollector::scavenge_local(size_t count, vm_address_t garbage[]) {
        size_t blocks_freed = 0;
        size_t bytes_freed = 0;
		GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_SCAVENGING_PHASE);
        for (size_t index = 0; index < count; index++) {
            void *ptr = reinterpret_cast<void*>(garbage[index]);
            // Only small quantum blocks are currently allocated locally, take advantage of that.
            Subzone *subzone = Subzone::subzone(ptr);
            usword_t q = subzone->quantum_index_unchecked(ptr, allocate_quantum_small_log2);
            if (!subzone->has_refcount(q)) {
                blocks_freed++;
                bytes_freed += subzone->size(q);
                if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(_zone), uintptr_t(ptr), 0, 0, 0);
                _thread.thread_cache_add(ptr);
            } else {
                _zone->handle_overretained_garbage(ptr, _zone->block_refcount(ptr));
                // make_global ???
            }
        }
		GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)_zone, AUTO_TRACE_SCAVENGING_PHASE, (uint64_t)blocks_freed, (uint64_t)bytes_freed);

        __sync_add_and_fetch(&_zone->stats.thread_collections_total, 1);
        __sync_add_and_fetch(&_zone->stats.thread_blocks_recovered_total, blocks_freed);
        __sync_add_and_fetch(&_zone->stats.thread_bytes_recovered_total, bytes_freed);

#if DEBUG
        __sync_add_and_fetch(&blocks_scavenged_locally, blocks_freed);
        __sync_add_and_fetch(&bytes_scavenged_locally, bytes_freed);
#endif
    }

#if DEBUG    
    static size_t blocks_scavenged_globally, bytes_scavenged_globally;
#endif

    static void finalize_work(void *args) {
        garbage_list *list = (garbage_list *)args;
        size_t blocks_freed = 0, bytes_freed = 0;
        Zone *zone = list->zone;
        zone->invalidate_garbage(list->count, list->garbage);
        zone->free_garbage(false, list->count, list->garbage, blocks_freed, bytes_freed);  // TODO:  all blocks are in the small admin, create a batched version.
        zone->clear_zombies();
        aux_free(list);

#if DEBUG
        __sync_add_and_fetch(&blocks_scavenged_globally, blocks_freed);
        __sync_add_and_fetch(&bytes_scavenged_globally, bytes_freed);
#endif
    }

    void ThreadLocalCollector::evict_local_garbage(size_t count, vm_address_t garbage[]) {
        set_marked_blocks_buffer(NULL);
        // scan the garbage blocks first.
        _markedBlocksCounter = 0;
        for (size_t i = 0; i < count; ++i) {
            void *block = (void*)garbage[i];
            Subzone *sz = Subzone::subzone(block);
            usword_t layout = sz->layout(block);
            if (!(layout & AUTO_UNSCANNED)) {
                Range range(block, sz->size(block));
                const unsigned char *map = (layout & AUTO_OBJECT) ? _zone->layout_map_for_block(block) : NULL;
                if (map)
                    scan_with_layout(range, map, NULL); // TLC doesn't need the write barrier.
                else
                    scan_range(range, NULL);
             }
        }
        
        // if no blocks were marked, then no evicitions needed.
        if (_markedBlocksCounter == 0) return;
        
        // now, mark all blocks reachable from the garbage blocks themselves.
        scan_marked_blocks();
        for (uint32_t i = _localBlocks.firstOccupiedSlot(); i <= _localBlocks.lastOccupiedSlot(); i++) {
            if (!_localBlocks.validPointerAtIndex(i))
                continue;
            if (_localBlocks.wasMarked(i)) {
                void *block = _localBlocks[i];
                Subzone *subzone = Subzone::subzone(block);
                // evict this block, since it is reachable from garbage, but not itself garbage.
                subzone->make_global(block);
                _localBlocks.remove(i);
            }
        }
    }
    
    //
    // process_local_garbage
    //
    void ThreadLocalCollector::process_local_garbage(bool finalizeNow) {
        // Gather the garbage blocks into a contiguous data structure that can be passed to Zone::invalidate_garbage / free_garbage.
        // TODO:  revisit this when we change the collector to use bitmaps to represent garbage lists.
        usword_t garbage_count = _localBlocks.count() - _markedBlocksCounter;
        if (garbage_count == 0) {
            // TODO:  if we keep hitting this condition, we could use feedback to increase the thread local threshold.
            _localBlocks.clearFlags();    // clears flags only.
			GARBAGE_COLLECTION_COLLECTION_END((auto_zone_t*)_zone, 0ull, 0ull, _localBlocks.count(), (uint64_t)(-1));
            return;
        }

        garbage_list *list = (garbage_list *)aux_malloc(sizeof(garbage_list) + garbage_count * sizeof(vm_address_t));
        list->count = 0;
        list->zone = _zone;
        size_t list_count = 0;
		size_t remaining_size = 0, scavenged_size = 0; 
        
        for (uint32_t i = _localBlocks.firstOccupiedSlot(), last = _localBlocks.lastOccupiedSlot(); i <= last; i++) {
            void *block = _localBlocks.unmarkedPointerAtIndex(i);
            if (block) {
                Subzone *subzone = Subzone::subzone(block);
                if (subzone->is_thread_local(block)) {
					if (GARBAGE_COLLECTION_COLLECTION_END_ENABLED()) {
						scavenged_size += subzone->size(block);
					}
                    list->garbage[list_count++] = reinterpret_cast<vm_address_t>(block);
                    _localBlocks.remove(i);
                    subzone->mark_local_garbage(block);
                } else {
                    auto_error(_zone, "not thread local garbage", (const void *)block);
                }
            } else if (GARBAGE_COLLECTION_COLLECTION_END_ENABLED()) {
				block = _localBlocks.markedPointerAtIndex(i);
				if (block) {
					Subzone *subzone = Subzone::subzone(block);
					if (subzone->is_thread_local(block)) {
						remaining_size += subzone->size(block);
            }
        }
			}
        }
        list->count = list_count;
        
        // clear the marks & compact. must be done before evict_local_garbage(), which does more marking.
        _localBlocks.clearFlagsCompact();

        // if using GCD to finalize and free the garbage, we now compute the set of blocks reachable from the garbage, and cause those to be
        // evicted from the local set.
        if (!finalizeNow) evict_local_garbage(list_count, list->garbage);

        AUTO_PROBE(auto_probe_end_local_scan(list_count, list->garbage));
        
        if (finalizeNow) {
            _zone->invalidate_garbage(list_count, list->garbage);
            scavenge_local(list->count, list->garbage);
            aux_free(list);
        } else {
#if USE_DISPATCH_QUEUE
            dispatch_async(_zone->collection_queue, ^{ finalize_work(list); });
#else
            // should never happen in pthread case.
            __builtin_trap();
#endif
        }
		GARBAGE_COLLECTION_COLLECTION_END((auto_zone_t*)_zone, list_count, (uint64_t)scavenged_size, _localBlocks.count(), (uint64_t)remaining_size);
    }


    //
    // should_collect
    //
    bool ThreadLocalCollector::should_collect(Zone *zone, Thread &thread, bool canFinalizeNow) {
        if (canFinalizeNow) {
            // Since we have permission to finalize now, our criteria for collections is simply that there are some
            // bare minimum number of thread local objects. I strongly suggest that we also consider allocation thresholds
            // for this trigger.
            return (thread.locals().count() >= thread.localAllocationThreshold());
        } else {
#if USE_DISPATCH_QUEUE
            // See if the count has exceeded 10x the local allocation threshold. something is wrong, so we should see if we can reclaim anything.
            if (zone->collection_queue) {
                return (thread.locals().count() >= (10 * thread.localAllocationThreshold()));
            }
#endif
            // until we can request finalizations on a different thread never run TLC in this mode.
            return false;
        }
    }

    //
    // collect
    //
    void ThreadLocalCollector::collect(bool finalizeNow) {
        AUTO_PROBE(auto_probe_begin_local_scan());
		GARBAGE_COLLECTION_COLLECTION_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_LOCAL);
        
		GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_SCANNING_PHASE);
		
        // scan the stack for the first set of hits
        _thread.scan_current_thread(*this);
        
        // recurse on what are now the roots
        scan_marked_blocks();
        
		GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)_zone, AUTO_TRACE_SCANNING_PHASE, (uint64_t)this->blocks_scanned(), (uint64_t)this->bytes_scanned());
        
        process_local_garbage(finalizeNow);
        
        // increase the local allocation threshold by 50%.
        uint32_t newThreshold = 3 * _localBlocks.count() / 2;
        if (newThreshold < 100) newThreshold = 100;
        _thread.setLocalAllocationThreshold(newThreshold);
        
        AUTO_PROBE(auto_probe_local_collection_complete());
    }
    
    //
    // eject_local_block
    //
    // removes block and all referenced stack local blocks
    //
    void ThreadLocalCollector::eject_local_block(void *startingBlock) {
        SimplePointerBuffer scannedBlocks;
        set_marked_blocks_buffer(&scannedBlocks);
        usword_t count = 0, totsize = 0;
        
        void *block = startingBlock;
        do {
            int32_t index = _localBlocks.slotIndex(block);
            if (index != -1) {
                Subzone *subzone = Subzone::subzone(block);
                usword_t size = subzone->size(block);
                totsize += size;
                ++count;
                Range range(block, size);
                const unsigned char *map = (subzone->layout(block) & AUTO_OBJECT) ? _zone->layout_map_for_block(block) : NULL;
                if (map)
                    scan_with_layout(range, map, NULL);
                else
                    scan_range(range, NULL);
                subzone->make_global(block);
                _localBlocks.remove(index);
                //printf("scanned block %p indirectly ejected\n", block);
            }
            
            bool shouldScan;
            do {
                block = scannedBlocks.pop();
                if (block) {
                    Subzone *rsz = Subzone::subzone(block);
                    if (rsz->is_thread_local_block(block)) {    // is an arbitrary address
                        shouldScan = rsz->should_scan_local_block(block);
                        // see process_local_garbage for why we check if the block is marked
                        //remove_local_allocation(block);
                        int32_t index = _localBlocks.slotIndex(block);
                        if (!shouldScan) {
                            rsz->make_global(block);
                            totsize += rsz->size(block);
                            ++count;
                            _localBlocks.remove(index);
                            //printf("unscanned block %p indirectly ejected\n", block);
                            index = -1;
                        }
                    } else {
                        shouldScan = false;
                    }
                }
            } while (block && !shouldScan);
        } while (block != NULL);
        // Assertion:  No need to clear flags because all objects marked were removed.
        // update statistics for non-cache allocation
        Statistics &zone_stats = _zone->statistics();
        zone_stats.add_count(count);
        zone_stats.add_size(totsize);
        _zone->adjust_threshold_counter(totsize);
    }

#if DEBUG
    extern "C" void print_TLC_stats() {
        printf("TLC scavenged %ld bytes (%ld blocks) locally.\n", bytes_scavenged_locally, blocks_scavenged_locally);
        printf("TLC scavenged %ld bytes (%ld blocks) globally.\n", bytes_scavenged_globally, blocks_scavenged_globally);
    }
#endif
}
