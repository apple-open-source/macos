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
    ThreadLocalCollector.cpp
    Thread Local Collector
    Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 */

#include "ThreadLocalCollector.h"
#include "auto_trace.h"
#include "auto_dtrace.h"
#include "Locks.h"
#include "BlockRef.h"

namespace Auto {
    //
    // is_block_aligned_range()
    // 
    // Returns true if the range of addresses is block aligned, and therefore can be
    // scanned in the 4 word at a time unrolled loop.
    //
    inline bool is_block_aligned_range(void **start, void **end) {
        return (((uintptr_t)start | (uintptr_t)end) & mask(block_alignment)) == 0;
    }
    
    //
    // append_block()
    // 
    // Add block to the list in _tlcBuffer, irrespective of how the buffer is being used at the moment
    //
    inline void ThreadLocalCollector::append_block(void *block) {
        _tlcBuffer[_tlcBufferCount++] = block;
    }
    
    //
    // mark_push_block()
    // 
    // Validates that block is a thread local block start pointer.
    // If it is, and it is unmarked, marks block and adds block to _tlcBuffer/_tlcBufferCount. 
    //
    inline void ThreadLocalCollector::mark_push_block(void *block) {
        if (_zone->in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            if (subzone->block_is_start(q) && subzone->is_thread_local(q)) {
                int32_t blockIndex = _localBlocks.slotIndex(block);
                if (blockIndex != -1 && !_localBlocks.testAndSetMarked(blockIndex)) {
                    append_block(block);
                }
            }
        }
    }
    
    void ThreadLocalCollector::scan_stack_range(const Range &range) {
        // set up the iteration for this range
        void ** reference = (void **)range.address();
        void ** const end = (void **)range.end();
        
        // local copies of valid address info
        const uintptr_t valid_lowest = (uintptr_t)_coverage.address();
        const uintptr_t valid_size = (uintptr_t)_coverage.end() - valid_lowest;

        // iterate through all the potential references
        
        // TODO:  Since stack ranges are large aligned, unroll the loop using that alignment.
        if (is_block_aligned_range(reference, end)) {
            // On both 32 and 64 bit architectures, the smallest block size is 4 words. This loop
            // is therefore scanning 1 quantum at a time.
            while (reference < end) {
                // do four at a time to get a better interleaving of code
                void *referent0 = reference[0];
                void *referent1 = reference[1];
                void *referent2 = reference[2];
                void *referent3 = reference[3];
                reference += 4; // increment here to avoid stall on loop check
               __builtin_prefetch(reference);
                if (((intptr_t)referent0 - valid_lowest) < valid_size) mark_push_block(referent0);
                if (((intptr_t)referent1 - valid_lowest) < valid_size) mark_push_block(referent1);
                if (((intptr_t)referent2 - valid_lowest) < valid_size) mark_push_block(referent2);
                if (((intptr_t)referent3 - valid_lowest) < valid_size) mark_push_block(referent3);
            }
        } else {
            for (void *last_valid_pointer = end - 1; reference <= last_valid_pointer; ++reference) {
                // get referent 
                void *referent = *reference;
                
                // if is a block then check this block out
                if (((intptr_t)referent - valid_lowest) < valid_size) {
                    mark_push_block(referent);
                }
            }
        }
    }
    
    void ThreadLocalCollector::scan_range(const Range &range) {
        // set up the iteration for this range
        void ** reference = (void **)range.address();
        void ** const end = (void **)range.end();
        
        // local copies of valid address info
        const uintptr_t valid_lowest = (uintptr_t)_coverage.address();
        const uintptr_t valid_size = (uintptr_t)_coverage.end() - valid_lowest;
        
        // iterate through all the potential references
        for (void *last_valid_pointer = end - 1; reference <= last_valid_pointer; ++reference) {
            // get referent 
            void *referent = *reference;
            
            // if is a block then check this block out
            if (((intptr_t)referent - valid_lowest) < valid_size) {
                mark_push_block(referent);
            }
        }
    }

    void ThreadLocalCollector::scan_with_layout(const Range &range, const unsigned char* map) {
        // convert to double indirect
        void **reference = (void **)range.address();
        void ** const end = (void **)range.end();
        Range subrange;
        // while not '\0' terminator
        while (unsigned data = *map++) {
            // extract the skip and run
            unsigned skip = data >> 4;
            unsigned run = data & 0xf;
            
            // advance the reference by the skip
            reference += skip;
            
            // scan runs as a range.
            subrange.set_range(reference, reference + run);
            if (subrange.address() < end && subrange.end() <= end) {
                // <rdar://problem/6516045>:  make sure we only scan valid ranges.
                scan_range(subrange);
            } else {
                break;
            }
            reference += run;
        }

        if (reference < end) {
            // since objects can be allocated with extra data at end, scan the remainder conservatively.
            subrange.set_range((void *)reference, end);
            scan_range(subrange);
        }
    }

    inline void ThreadLocalCollector::scan_local_block(Subzone *subzone, usword_t q, void *block) {
        Range range(block, subzone->size(q));
        const unsigned char *map = (subzone->layout(q) & AUTO_OBJECT) ? _zone->layout_map_for_block(block) : NULL;
        if (map)
            scan_with_layout(range, map);
        else
            scan_range(range);
    }
    
    //
    // scan_marked_blocks
    //
    // scans all the blocks in _tlcBuffer
    //
    void ThreadLocalCollector::scan_marked_blocks() {
        size_t index = 0;
        while (index < _tlcBufferCount) {
            void *block = _tlcBuffer[index++];
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            if (subzone->should_scan_local_block(q)) {
                scan_local_block(subzone, q, block);
            }
        }
    }

    //
    // scavenge_local
    //
    // we can't return to the general pool because the general collector thread may
    // still be scanning us.  Instead we return data to our cache.
    //
    void ThreadLocalCollector::scavenge_local(size_t count, void *garbage[]) {
        size_t blocks_freed = 0;
        size_t bytes_freed = 0;
        size_t bytes_dropped = 0;
        
        // if collection checking is on then clear the check count for all the garbage blocks
        Zone *zone = _thread.zone();
        if (zone->collection_checking_enabled()) {
            zone->clear_garbage_checking_count(garbage, count);
        }
        
		GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_SCAVENGING_PHASE);
        for (size_t index = 0; index < count; index++) {
            void *block = garbage[index];
            // Only small quantum blocks are currently allocated locally, take advantage of that.
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            if (!subzone->has_refcount(q)) {
                blocks_freed++;
                size_t block_size = subzone->size(q);
                if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, uintptr_t(_zone), uintptr_t(block), 0, 0, 0);
                if (!_thread.thread_cache_add(block, subzone, q)) {
                    // drop the block on the floor and leave it for the heap collector to find
                    subzone->allocate(q, subzone->length(q), AUTO_UNSCANNED, false, false);
                    bytes_dropped += block_size;
                } else {
                    bytes_freed += block_size;
                }
            } else {
                SubzoneBlockRef ref(subzone, q);
                if (!is_zombie(block)) {
                    _zone->handle_overretained_garbage(block, ref.refcount(), ref.layout());
                } else {
                    // transition the block from local garbage to retained global
                    SpinLock lock(subzone->admin()->lock()); // zombify_internal requires we hold the admin lock
                    subzone->allocate(q, subzone->length(q), subzone->layout(q), true, false);
                    _zone->zombify_internal(ref);
                }
            }
        }
        if (bytes_dropped) {
            _zone->adjust_allocation_counter(bytes_dropped);
        }
		GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)_zone, AUTO_TRACE_SCAVENGING_PHASE, (uint64_t)blocks_freed, (uint64_t)bytes_freed);
    }

    static void finalize_work(Zone *zone, const size_t garbage_count, void *garbage[]) {
        size_t blocks_freed = 0, bytes_freed = 0;
        zone->invalidate_garbage(garbage_count, garbage);
        zone->free_garbage(garbage_count, garbage, 0, NULL, blocks_freed, bytes_freed);  // TODO:  all blocks are in the small admin, create a batched version.
        zone->clear_zombies();
        aux_free(garbage);
    }
    
    // assumes _tlcBuffer/_tlcBufferCount hold the garbage list
    bool ThreadLocalCollector::block_in_garbage_list(void *block) {
        for (size_t i=0; i<_tlcBufferCount; i++) {
            if (_tlcBuffer[i] == block)
                return true;
        }
        return false;
    }
    
    // Assumes _tlcBuffer/_tlcBufferCount hold the garbage list
    void ThreadLocalCollector::evict_local_garbage() {
        // scan the garbage blocks to evict all blocks reachable from the garbage list
        size_t evict_cursor = _tlcBufferCount;
        
        size_t scan_cursor = 0;
        while (scan_cursor < _tlcBufferCount) {
            void *block = _tlcBuffer[scan_cursor++];
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            if (subzone->is_scanned(q)) {
                scan_local_block(subzone, q, block);
            }
        }
        
        usword_t global_size = 0;
        while (evict_cursor < _tlcBufferCount) {
            void *block = _tlcBuffer[evict_cursor++];
            // evict this block, since it is reachable from garbage, but not itself garbage.
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            subzone->make_global(q);
            _localBlocks.remove(block);
            global_size += subzone->size(q);
        }
        if (global_size != 0)
            _zone->adjust_allocation_counter(global_size);
    }
    
    //
    // process_local_garbage
    //
    void ThreadLocalCollector::process_local_garbage(void (*garbage_list_handler)(ThreadLocalCollector *)) {
        // Gather the garbage blocks into _tlcBuffer, which currently holds marked blocks.
        usword_t garbage_count = _localBlocks.count() - _tlcBufferCount;
        if (garbage_count == 0) {
            // no garbage
            // TODO:  if we keep hitting this condition, we could use feedback to increase the thread local threshold.
            _localBlocks.clearFlags();    // clears flags only.
			GARBAGE_COLLECTION_COLLECTION_END((auto_zone_t*)_zone, 0ull, 0ull, _localBlocks.count(), (uint64_t)(-1));
            return;
        }

        _tlcBufferCount = 0;
        size_t scavenged_size = 0; 
        
        // use the mark bit in _localBlocks to generate a garbage list in _tlcBuffer/_tlcBufferCount
        for (uint32_t i = _localBlocks.firstOccupiedSlot(), last = _localBlocks.lastOccupiedSlot(); (i <= last) && (_tlcBufferCount != garbage_count); i++) {
            void *block = _localBlocks.unmarkedPointerAtIndex(i);
            if (block) {
                Subzone *subzone = Subzone::subzone(block);
                usword_t q = subzone->quantum_index_unchecked(block);
                if (subzone->is_thread_local(q)) {
						scavenged_size += subzone->size(q);
                    append_block(block);
                    _localBlocks.remove(i);
                } else {
                    auto_error(_zone, "not thread local garbage", (const void *)block);
                }
            }
        }
#ifdef MEASURE_TLC_STATS
        _zone->statistics().add_local_collected(_tlcBufferCount);
#endif
        
        // clear the marks & compact. must be done before evict_local_garbage(), which does more marking.
        // if the thread is not suspended then we can also possibly shrink the locals list size
        // if the thread IS suspended then we must not allocate
        if (_thread.suspended())
            _localBlocks.clearFlagsRehash();
        else
            _localBlocks.clearFlagsCompact();
        
        AUTO_PROBE(auto_probe_end_local_scan(_tlcBufferCount, &_tlcBuffer[0]));

        garbage_list_handler(this);

        // skip computing the locals size if the probe is not enabled
        if (GARBAGE_COLLECTION_COLLECTION_PHASE_END_ENABLED())
            GARBAGE_COLLECTION_COLLECTION_END((auto_zone_t*)_zone, garbage_count, (uint64_t)scavenged_size, _localBlocks.count(), (uint64_t)_localBlocks.localsSize());
    }

    // Assumes _tlcBuffer/_tlcBufferCount hold the garbage list
    void ThreadLocalCollector::finalize_local_garbage_now(ThreadLocalCollector *tlc) {
        size_t garbage_count = tlc->_tlcBufferCount;
        mark_local_garbage(tlc->_tlcBuffer, garbage_count);
        tlc->_zone->invalidate_garbage(garbage_count, &tlc->_tlcBuffer[0]);
        tlc->scavenge_local(garbage_count, &tlc->_tlcBuffer[0]);
#ifdef MEASURE_TLC_STATS
        tlc->_zone->statistics().add_recycled(garbage_count);
#endif
    }
    
    inline void ThreadLocalCollector::mark_local_garbage(void **garbage_list, size_t garbage_count) {
        for (size_t i = 0; i < garbage_count; i++) {
            void *block = garbage_list[i];
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            subzone->mark_local_garbage(q);
        }
    }
    
    // Assumes _tlcBuffer/_tlcBufferCount hold the garbage list
    void ThreadLocalCollector::finalize_local_garbage_later(ThreadLocalCollector *tlc) {
        size_t garbage_count = tlc->_tlcBufferCount;
        tlc->evict_local_garbage(); // note this modifies _tlcBuffer/_tlcBufferCount
        mark_local_garbage(tlc->_tlcBuffer, garbage_count);
        Zone *z = tlc->_zone;
        void **garbage_copy = (void **)aux_malloc(garbage_count * sizeof(void *));
        memcpy(garbage_copy, tlc->_tlcBuffer, garbage_count * sizeof(void *));
        dispatch_async(tlc->_zone->_collection_queue, ^{ finalize_work(z, garbage_count, garbage_copy); });
#ifdef MEASURE_TLC_STATS
        tlc->_zone->statistics().add_global_freed(garbage_count);
#endif
    }
    
    // Assumes _tlcBuffer/_tlcBufferCount hold the garbage list
    void ThreadLocalCollector::unmark_local_garbage(ThreadLocalCollector *tlc) {
        size_t garbage_count = tlc->_tlcBufferCount;
        tlc->evict_local_garbage(); // note this modifies _tlcBuffer/_tlcBufferCount
        mark_local_garbage(tlc->_tlcBuffer, garbage_count);
        for (uint32_t i=0; i<garbage_count; i++) {
            void *block = tlc->_tlcBuffer[i];
            Subzone *sz = Subzone::subzone(block);
            usword_t q = sz->quantum_index_unchecked(block);
            sz->test_and_clear_mark(q);
            sz->mark_global_garbage(q);
        }
#ifdef MEASURE_TLC_STATS
        tlc->_zone->statistics().add_global_freed(garbage_count);
#endif
    }
    
    //
    // should_collect
    //
    bool ThreadLocalCollector::should_collect(Zone *zone, Thread &thread, bool canFinalizeNow) {
        if (thread.thread_local_collector() == NULL) {
            if (canFinalizeNow) {
                // Since we have permission to finalize now, our criteria for collections is simply that there are some
                // bare minimum number of thread local objects. I strongly suggest that we also consider allocation thresholds
                // for this trigger.
                return (thread.locals().count() >= (local_allocations_size_limit/10));
            } else {
                // If the count has reached the set size limit then try to collect to make space even though we can't finalize.
                if (zone->_collection_queue) {
                    return (thread.locals().count() >= local_allocations_size_limit);
                }
            }
        }
        return false;
    }

    bool ThreadLocalCollector::should_collect_suspended(Thread &thread)
    {
        assert(thread.suspended());
        // Don't do a suspended scan if malloc stack logging is turned on. If the thread happens to be in the middle of an allocation,
        // TLC's own use of the aux_zone() will deadlock.
        bool collect = (malloc_logger == NULL) && thread.tlc_watchdog_should_trigger() && !Sentinel::is_guarded(thread.localsGuard()) && thread.locals().count() > 0;
        if (collect)
            thread.tlc_watchdog_disable();
        else
            thread.tlc_watchdog_tickle();
        return collect;
    }
    

#ifndef __BLOCKS__
    class thread_local_scanner_helper : public Thread::thread_scanner {
        ThreadLocalCollector &_collector;
    public:
        thread_local_scanner_helper(ThreadLocalCollector &collector) : _collector(collector) {}
        virtual void operator() (Thread *thread, Range &range) { _collector.scan_stack_range(range); }
    };
#endif

    
    void ThreadLocalCollector::trace_scanning_phase_end() {
        size_t scanned_size = 0;
        for (usword_t i = 0; i < _tlcBufferCount; i++) {
            void *block = _tlcBuffer[i++];
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            if (subzone->should_scan_local_block(q)) {
                scanned_size += subzone->size(q);
            }
        }
        GARBAGE_COLLECTION_COLLECTION_PHASE_END((auto_zone_t*)_zone, AUTO_TRACE_SCANNING_PHASE, (uint64_t)_tlcBufferCount, (uint64_t)scanned_size);
    }
    
    //
    // collect
    //
    void ThreadLocalCollector::collect(bool finalizeNow) {
        AUTO_PROBE(auto_probe_begin_local_scan());
        assert(_thread.thread_local_collector() == NULL);
        _thread.set_thread_local_collector(this);
        
        _thread.tlc_watchdog_reset();
        
		GARBAGE_COLLECTION_COLLECTION_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_LOCAL);
        
		GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_SCANNING_PHASE);

#ifdef __BLOCKS__		
        // scan the stack for the first set of hits
        _thread.scan_current_thread(^(Thread *thread, const Range &range) {
            this->scan_stack_range(range);
        }, _stack_bottom);
#else
        thread_local_scanner_helper helper(*this);
        _thread.scan_current_thread(helper, _stack_bottom);
#endif
        
        // recurse on what are now the roots
        scan_marked_blocks();
        
        if (GARBAGE_COLLECTION_COLLECTION_PHASE_END_ENABLED()) {
            trace_scanning_phase_end();
        }
        process_local_garbage(finalizeNow ? finalize_local_garbage_now : finalize_local_garbage_later);
        
        _thread.set_thread_local_collector(NULL);
        
        if (_localBlocks.count() > local_allocations_size_limit/2)
            _thread.flush_local_blocks();
        
        AUTO_PROBE(auto_probe_local_collection_complete());
    }
    
    //
    // collect_suspended
    //
    void ThreadLocalCollector::collect_suspended(Range &registers, Range &stack) {
        AUTO_PROBE(auto_probe_begin_local_scan());
        assert(_thread.thread_local_collector() == NULL);
        assert(_thread.suspended());
        _thread.set_thread_local_collector(this);
        
		GARBAGE_COLLECTION_COLLECTION_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_LOCAL);
        
		GARBAGE_COLLECTION_COLLECTION_PHASE_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_SCANNING_PHASE);
        
        scan_range(stack);
        scan_range(registers);
        
        // recurse on what are now the roots
        scan_marked_blocks();
        
        if (GARBAGE_COLLECTION_COLLECTION_PHASE_END_ENABLED()) {
            trace_scanning_phase_end();
        }
        
        process_local_garbage(unmark_local_garbage);
        
        _thread.set_thread_local_collector(NULL);
        
        AUTO_PROBE(auto_probe_local_collection_complete());
    }
    
    void ThreadLocalCollector::reap_all() {
        GARBAGE_COLLECTION_COLLECTION_BEGIN((auto_zone_t*)_zone, AUTO_TRACE_LOCAL);
        _thread.set_thread_local_collector(this);
        process_local_garbage(finalize_local_garbage_now);
        _thread.set_thread_local_collector(NULL);
    }
    
    //
    // eject_local_block
    //
    // removes block and all referenced stack local blocks
    //
    void ThreadLocalCollector::eject_local_block(void *startingBlock) {
        if (_thread.thread_local_collector() != NULL) {
            // if a thread localcollection is in progress then we can't use the tlc buffer in the thread object
            _tlcBuffer = (void **)malloc(local_allocations_size_limit * sizeof(void *));
        }
        Subzone *subzone = Subzone::subzone(startingBlock);
#ifndef NDEBUG
        {
            usword_t q;
            assert(subzone->block_is_start(startingBlock, &q) && subzone->is_thread_local(q));
            assert(_localBlocks.slotIndex(startingBlock) != -1);
        }
#endif

        mark_push_block(startingBlock);

        // mark all local blocks reachable from this block.
        scan_marked_blocks();
        
        // loop over all marked blocks, and mark them as global.
        size_t evicted_size = 0;
        for (size_t i = 0; i < _tlcBufferCount; i++) {
            void *block = _tlcBuffer[i];
            subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);
            assert(subzone->is_thread_local(q));
            subzone->make_global(q);
            _localBlocks.remove(block);
            evicted_size += subzone->size(q);
        }
        // Assertion:  No need to clear flags because all objects marked were removed.
        _zone->adjust_allocation_counter(evicted_size);
        
        if (_thread.thread_local_collector() != NULL) {
            free(_tlcBuffer);
        }
    }

    void ThreadLocalCollector::add_zombie(void *block) {
        if (!_zombies)
            _zombies = new PtrHashSet();
        
        if (_zombies->find(block) == _zombies->end()) {
            _zombies->insert(block);
        }
    }

    inline bool ThreadLocalCollector::is_zombie(void *block) {
        if (_zombies) {
            PtrHashSet::iterator iter = _zombies->find(block);
            return (iter != _zombies->end());
        } else {
            return false;
        }
    }
}
