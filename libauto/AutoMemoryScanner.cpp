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
    AutoMemoryScanner.cpp
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoAdmin.h"
#include "AutoBlockIterator.h"
#include "AutoDefs.h"
#include "AutoEnvironment.h"
#include "AutoLarge.h"
#include "AutoRange.h"
#include "AutoMemoryScanner.h"
#include "AutoSubzone.h"
#include "AutoThread.h"
#include "AutoWriteBarrier.h"
#include "AutoZone.h"


namespace Auto {

    //----- MemoryScanner -----//
    
    
    //
    // Constructor
    //
    MemoryScanner::MemoryScanner(Zone *zone, void *current_stack_bottom, bool does_check_block)
    :   _zone(zone), _current_stack_bottom(current_stack_bottom),
        _use_pending(false),
        _some_pending(false),
        _is_partial(false),
        _is_collector(false),
        _does_check_block(does_check_block), 
        _should_coalesce(false),
        _use_exact_scanning(Environment::use_exact_scanning),
        _coalesced_range(),
        _amount_scanned(0),
        _blocks_scanned(0)
    {
        
#if defined(__ppc__)
/*
        whoops, this won't work because valid highest can change during a collection.
        if it does, then we risk missing marking it and/or searching it

        if (has_altivec()) {
            // set up altivec vectors
            uintptr_t valid_lowest = _zone.coverage().address();
            uintptr_t valid_highest = _zone.coverage().end();
            
			
            uintptr_t diff = _valid_highest - _valid_lowest - 1;
            unsigned int lo_vector[4] = { _valid_lowest, _valid_lowest, _valid_lowest, _valid_lowest };
            unsigned int diff_vector[4] = { diff, diff, diff, diff };
            memcpy(&_lo_vector, lo_vector, sizeof(_lo_vector));
            memcpy(&_diff_vector, diff_vector, sizeof(_diff_vector));
        }
*/
#endif
    }
    

    //
    // scan_for_unmarked_blocks
    //
    // Scan block for references to unvisited blocks.
    //
    void MemoryScanner::scan_for_unmarked_blocks(Subzone *subzone, usword_t q, void *block) {
        // get block range
        Range range(block, subzone->size(q));

        // XXX_PCB:  see if we can catch NSDeallocateObject() in the act.
        ASSERTION(subzone->block_is_start(q));

        // get layout
        // This item might have been collected by the per-thread collector
        // and is no longer what it was when we first encountered it.
        // Ideally we would have a scan set and be able to take it out.
        // <rdar://problem/6599526> - Blocks on the scan stack should be scanned. Don't bother checking that here, in the interest of speed.
        if (subzone->layout(q) == AUTO_OBJECT_SCANNED) {
            scan_object_range(range, &subzone->write_barrier());
        } else {
            // we DO see unscanned items on the stack, because we might be scanning locally managed blocks that have been recycled to a thread local free list!!!
            WriteBarrier *wb = &subzone->write_barrier();
            // Avoid write-barrier scanning overhead for blocks smaller than 2 cards in size.
            if (_is_partial && range.size() >= 256)
                wb->scan_marked_ranges(range, *this);
            else
                scan_range(range, wb);
        }

        // during associative reference scanning push associations on to the scan stack.
        if (_zone->_scanning_associations) _zone->pend_associations(block, *this);
    }
    
    void MemoryScanner::scan_for_unmarked_blocks(Large *large, void *block) {
        // get block range
        Range range(block, large->size());

        // <rdar://problem/6599526> - Blocks on the scan stack should be scanned. Don't bother checking that here, in the interest of speed.
        if (large->layout() == AUTO_OBJECT_SCANNED) {
            scan_object_range(range, &large->write_barrier());
        } else {
            // conservative scan of a large block; use the write-barrier to partition the ranges.
            WriteBarrier *wb = &large->write_barrier();
            if (_is_partial)
                wb->scan_marked_ranges(range, *this);
            else
                scan_range(range, wb);
        }
        
        // during associative reference scanning push associations on to the scan stack.
        if (_zone->_scanning_associations) _zone->pend_associations(block, *this);
    }
    
    //
    // scan_object_range
    //
    // Scan block using optional layout map
    //
    void MemoryScanner::scan_object_range(Range &block, WriteBarrier *wb) {
        if (_use_exact_scanning) {
            // request the STRONG layout map
            const unsigned char *map = _zone->layout_map_for_block(block.address());

            // if layout map is present
            if (map) {
                // then scan using map
                scan_with_layout(block, map, wb);
                return;
            }
        } else {
            // request the WEAK layout map
            const unsigned char *map = _zone->weak_layout_map_for_block(block.address());

            // if layout map is present
            if (map) {
                // then scan using map
                scan_with_weak_layout(block, map, wb);
                return;
            }
        }

        // otherwise conservatively scan the block
        if (_is_partial)
            wb->scan_marked_ranges(block, *this);
        else
            scan_range(block, wb);
    }
    
    
    //
    // scan_with_layout
    //
    // Scan block using (strong) layout map
    //
    void MemoryScanner::scan_with_layout(Range &block, const unsigned char* map, WriteBarrier *wb) {
        AUTO_PROBE(auto_probe_scan_with_layout((void *)block.address(), (void *)block.end(), map));
        // convert to double indirect
        void **reference = (void **)block.address();
        void **end = (void **)block.end();
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
                scan_range(subrange, wb);
            } else {
                break;
            }
            reference += run;
        }

        if (reference < end) {
            // since objects can be allocated with extra data at end, scan the remainder conservatively.
            subrange.set_range((void *)reference, end);
            scan_range(subrange, wb);
        }
        AUTO_PROBE(auto_probe_did_scan_with_layout((void *)block.address(), (void *)block.end(), map));
    }


    //
    // scan_with_weak_layout
    //
    // Scan block using (weak) layout map
    //
    void MemoryScanner::scan_with_weak_layout(Range &block, const unsigned char* map, WriteBarrier *wb) {
        AUTO_PROBE(auto_probe_scan_with_weak_layout((void *)block.address(), (void *)block.end(), map));
        // convert to double indirect
        void **reference = (void **)block.address();
        // while not '\0' terminator
        Range subrange;
        while (unsigned data = *map++) {
            // extract the skip and run
            unsigned run = data >> 4;
            unsigned skip = data & 0xf;
            
            // scan runs as a range.
            subrange.set_range(reference, reference + run);
            scan_range(subrange, wb);
            reference += run;

            // advance the reference by the skip (bypass the weak references)
            reference += skip;
        }
        
        // since objects can be allocated with extra data at end, scan the remainder conservatively.
        subrange.set_range((void *)reference, block.end());
        scan_range(subrange, wb);
    }

    //
    // set_pending
    //
    // Set the block as pending if it is a block and has not been marked.
    //
    void MemoryScanner::set_pending(void *block) {
        // exit early if NULL
#if defined(DEBUG)
        _blocks_checked++;
#endif
        if (!block) return;
        if (_zone->in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            usword_t q = subzone->quantum_index_unchecked(block);

            if (!subzone->block_is_start(q)) return;
            AUTO_PROBE(auto_probe_set_pending(block));
            if (is_partial()) {
                if (!subzone->should_pend_new(block, q)) return;
                // <rdar://problem/6550444> Reduce generational scanning time. This doesn't help too much.
                // if (!subzone->write_barrier().range_has_marked_cards(block, subzone->size(q))) return;
            } else {
                if (!subzone->should_pend(block, q)) return;
            }

            if (_use_pending) {
                subzone->set_pending(q);
                set_some_pending();
            } else {
                _zone->scan_stack_push_block(block);
            }
#if defined(DEBUG)
            _blocks_scanned++;
#endif

        } else if (_zone->in_large_memory(block)) {
            if (!Large::is_start(block)) return;
            AUTO_PROBE(auto_probe_set_pending(block));
            Large *large = Large::large(block);
            if (is_partial() && !large->is_new()) return;
            if (large->test_set_mark()) return;
            
            // if block is unscanned, we're done.
            if (large->layout() & AUTO_UNSCANNED) return;
            
            if (_use_pending) {
                large->set_pending();
                set_some_pending();
            } else {
                _zone->scan_stack_push_block(block);
            }
#if defined(DEBUG)
            _blocks_scanned++;
#endif
        }

    }


    //
    // repend
    //
    // Force a block to be rescanned.
    //
    void MemoryScanner::repend(void *block) {
        //void *block = block_start(address);
        //if (!block) return;
        if (_zone->in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);

            if (!subzone->block_is_start(block)) return;
            usword_t q = subzone->quantum_index(block);
            if (subzone->test_set_mark(q)) return;
            if (subzone->layout(q) & AUTO_UNSCANNED) return;
            if (_use_pending) {
                subzone->set_pending(q);
                set_some_pending();
            } else {
                _zone->scan_stack_push_block(block);
            }
        } else if (_zone->in_large_memory(block)) {
            if (!Large::is_start(block)) return;
            Large *large = Large::large(block);
            if (large->test_set_mark()) return;
            if (large->layout() & AUTO_UNSCANNED) return;
            if (_use_pending) {
                large->set_pending();
                set_some_pending();
            } else {
                _zone->scan_stack_push_block(block);
            }
        }
    }


    //
    // check_block
    //
    // Purpose driven examination of block.
    // Override in subclass for task specific block check.
    //
    void MemoryScanner::check_block(void **reference, void *block) {
        set_pending(block);
    }
    
    //
    // associate_block
    //
    // Called during associative reference scanning, to indicate this block is associated
    // with reference via key.
    //
    void MemoryScanner::associate_block(void **reference, void *key, void *block) {
        set_pending(block);
    }


#if defined(__ppc__)
    //
    // isolate_altivec_vector
    //
    // Isolate potential pointers in vector.
    //
    inline vector unsigned int MemoryScanner::isolate_altivec_vector(vector unsigned int data_vector, vector unsigned int lo_vector, vector unsigned int diff_vector) {
        vector unsigned int biased_vector = vec_sub(data_vector, lo_vector);
        vector unsigned int compare_vector = vec_cmpgt(diff_vector, biased_vector);
        return vec_and(data_vector, compare_vector);
     }


    //
    // check_altivec_vector
    //
    // Checks to see if an altivec vector contains pointers.
    //
    inline void MemoryScanner::check_altivec_vector(void **isolated_vector) {
        void *result0 = isolated_vector[0];
        void *result1 = isolated_vector[1];
        void *result2 = isolated_vector[2];
        void *result3 = isolated_vector[3];
        
        if (result0) set_pending(result0);
        if (result1) set_pending(result1);
        if (result2) set_pending(result2);
        if (result3) set_pending(result3);
     }


    //
    // scan_range_altivec
    //
    // Scans the specified aligned range for references to unmarked blocks using altivec.
    //
    void MemoryScanner::scan_range_altivec(void **reference, void **end) {
        typedef union {
            vector unsigned int _as_vector;
            void *_as_pointer[4];
        } ConvertBuffer;
        
        vector unsigned int *reference_vector = (vector unsigned int *)reference;
        vector unsigned int *end_vector = (vector unsigned int *)end;

        vector unsigned int data_vector0 = *reference_vector++; // guaranteed reference != end
        vector unsigned int data_vector1; 

        vector unsigned int *near_end_vector = end_vector - 1;
        vector unsigned int lo_vector = _lo_vector;
        vector unsigned int diff_vector = _diff_vector;
        
        ConvertBuffer buffer0 = {{ NULL, NULL, NULL, NULL }};
        ConvertBuffer buffer1 = {{ NULL, NULL, NULL, NULL }};

        while (reference_vector < near_end_vector) {
            data_vector1 = *reference_vector++;
            buffer1._as_vector = isolate_altivec_vector(data_vector0, lo_vector, diff_vector);
            check_altivec_vector(buffer0._as_pointer);
            data_vector0 = *reference_vector++;
            buffer0._as_vector = isolate_altivec_vector(data_vector1, lo_vector, diff_vector);
            check_altivec_vector(buffer1._as_pointer);
        }
    
        buffer1._as_vector = isolate_altivec_vector(data_vector0, lo_vector, diff_vector);
        check_altivec_vector(buffer0._as_pointer);
        
        if (reference_vector < end_vector) {
            buffer0._as_vector = isolate_altivec_vector(*reference_vector, lo_vector, diff_vector);
            check_altivec_vector(buffer1._as_pointer);
            check_altivec_vector(buffer0._as_pointer);
        } else {
            check_altivec_vector(buffer1._as_pointer);
        }
    }
#endif
    
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
    // scan_range
    //
    // Scans the specified aligned range for references to unmarked blocks.
    //
    void MemoryScanner::scan_range(const Range &range, WriteBarrier *wb) {
        AUTO_PROBE(auto_probe_scan_range((void *)range.address(), (void *)range.end()));
        // set up the iteration for this range
        void **reference = (void **)range.address();
        void **end = (void **)range.end();
        ASSERTION(reference <= end);
        
        // XXX make sure the specified scanning range is always pointer aligned for the current architecture.
        if (!is_pointer_aligned(reference) || !is_pointer_aligned(end)) {
            static char buffer[256];
            snprintf(buffer, sizeof(buffer), "scan_range:  non-pointer aligned range = { %p, %p }\n", reference, end);
            __crashreporter_info__ = buffer;
            __builtin_trap();
        }

        // Check to see if we're repairing the write barrier. If so, never coalesce ranges because each range may have its own unique write barrier.
        // Coalescing only happens when using the pending bitmap, because blocks are visited in heap order.
        const bool repair_write_barrier = (wb && _zone->repair_write_barrier());
        
        // check to see if we should defer scanning until we coalesce consecutive ranges
        if (_should_coalesce && !repair_write_barrier) {
            // if a continuation of the prior range
            if (_coalesced_range.end() == (void *)reference) {
                // expand the coalesced range to include the new range
                _coalesced_range.set_end(end);
                // wait till later
                return;
            }
            
            // use the old coalesced range now
            reference = (void **)_coalesced_range.address();
            end = (void **)_coalesced_range.end();
            
            // save the new range for later
            _coalesced_range = range;
        }
        
        // exit early for trivial case
        if (reference == end) return;

        if (!use_pending()) {
            // try and keep the stack minimal
            const usword_t scan_maximum = 1024;
            usword_t size = (uintptr_t)end - (uintptr_t)reference;
            
            if (size > scan_maximum) {
                end = (void **)displace((void *)reference, scan_maximum);
                Range tail(end, size - scan_maximum);
                // if the write-barrier is being repaired, push it on the stack so we can reuse it later. This will only happen when scanning ranges > 1K in size.
                if (repair_write_barrier) {
                    _zone->scan_stack_push_write_barrier(wb);
                }
                _zone->scan_stack_push_range(tail);
            }
        }
     
        _amount_scanned += (char *)end - (char *)reference;
        
        __builtin_prefetch(reference);

        // local copies of valid address info
        uintptr_t valid_lowest = (uintptr_t)_zone->coverage().address();
        uintptr_t valid_size = (uintptr_t)_zone->coverage().end() - valid_lowest;
        
        // XXX do slow scan always with Arena until performance analysis is done
        if (repair_write_barrier || _does_check_block || !is_block_aligned_range(reference, end)) {
            void *last_valid_pointer = end - 1;
            // iterate through all the potential references
            for ( ; reference <= last_valid_pointer; ++reference) {
                // get referent 
                void *referent = *reference;
                
                // if is a block then check this block out
                if (((intptr_t)referent - valid_lowest) < valid_size && _zone->block_is_start(referent)) {
                    check_block(reference, referent);
                    // update the write-barrier if block is new. this is used during card repair.
                    // FIXME:  check to see if reference is in an old block.
                    if (repair_write_barrier && _zone->is_new(referent))
                        wb->mark_card(reference);
                }
            }
        } else
#if defined(__ppc__)
        if (has_altivec()) {
            scan_range_altivec(reference, end);
        } else
#endif
        {
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
                if (((intptr_t)referent0 - valid_lowest) < valid_size) set_pending(referent0);
                if (((intptr_t)referent1 - valid_lowest) < valid_size) set_pending(referent1);
                if (((intptr_t)referent2 - valid_lowest) < valid_size) set_pending(referent2);
                if (((intptr_t)referent3 - valid_lowest) < valid_size) set_pending(referent3);
            }
        }
    }

    extern "C" {
        void scanMemory(void *context, void *start, void *end) {
            MemoryScanner *scanner = (MemoryScanner *)context;
            Range r(start, end);
            scanner->scan_range(r);
        }
    }
    
    
    void MemoryScanner::scan_external() {
        if (_zone->control.scan_external_callout) _zone->control.scan_external_callout((void *)this, scanMemory);
    }
    
    //
    // scan_range_from_thread
    //
    // Scan a range of memory in the thread's stack.
    // Subclass may want to record context.
    //
    void MemoryScanner::scan_range_from_thread(Range &range, Thread &thread) {
        scan_range(range);
    }
    
    
    //
    // scan_range_from_registers
    //
    // Scan a range of memory containing an image of the thread's registers.
    // Subclass may want to record context.
    //
    void MemoryScanner::scan_range_from_registers(Range &range, Thread &thread, int first_register) {
        scan_range(range);
    }


    //
    // scan_retained_blocks
    //
    // Add all the retained blocks to the scanner.
    //
    struct scan_retained_blocks_visitor {
    
        MemoryScanner &_scanner;                               // scanner requesting retained blocks
        
        // Constructor
        scan_retained_blocks_visitor(MemoryScanner &scanner) : _scanner(scanner) {}
        
        // visitor function for subzone
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            if ((subzone->has_refcount(q) || subzone->is_thread_local(q)) && !subzone->test_set_mark(q)) {
                // if not a scanned block then there is no need to pend it for scanning
                if (subzone->layout(q) & AUTO_UNSCANNED) return true;

                // indicate that all retained blocks must be scanned
                if (_scanner.use_pending()) {
                    subzone->set_pending(q);
                } else {
                    zone->scan_stack_push_block(subzone->quantum_address(q));
                }
            }

            // always continue
            return true;
        }
        
        // visitor function for large
        inline bool visit(Zone *zone, Large *large) {
            if (large->refcount() && !large->test_set_mark()) {
                // if not a scanned block then there is no need to pend it for scanning
                if (large->layout() & AUTO_UNSCANNED) return true;

                // indicate that all retained blocks must be scanned
                if (_scanner.use_pending()) {
                    large->set_pending();
                } else {
                    zone->scan_stack_push_block(large->address());
                }
            }

            // always continue
            return true;
        }
    };
    void MemoryScanner::scan_retained_blocks() {
        // set up retain visitor
        scan_retained_blocks_visitor visitor(*this);
        visitAllocatedBlocks(_zone, visitor);

        // set some pending flag
        set_some_pending();
    }


    //
    // scan_retained_and_old_blocks
    //
    // Add all the retained and old blocks to the scanner.
    //
    struct scan_retained_and_old_blocks_visitor {
    
        MemoryScanner &_scanner;                               // collector requesting retained or old blocks
        const bool _use_pending;
        
        // Constructor
        scan_retained_and_old_blocks_visitor(MemoryScanner &scanner) : _scanner(scanner), _use_pending(scanner.use_pending()) {}
        
        
        // visitor function for subzone
        // mark all old blocks - scan portions via write-barrier cards
        // mark all retained blocks - scan portions via write-barrier cards
        // mark all local blocks - scan portions via write-barrier cards
        // 
        // scan retained new blocks and ((local or old) via write-barrier cards)
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            if (!subzone->is_thread_local(q) && subzone->is_new(q)) {
                if (subzone->has_refcount(q) && !subzone->test_set_mark(q) && subzone->is_scanned(q)) {
                    if (_use_pending) {
                        subzone->set_pending(q);
                    } else {
                        zone->scan_stack_push_block(subzone->quantum_address(q));
                    }
                }
            } else {
                if (!subzone->test_set_mark(q) && subzone->is_scanned(q)) {
                    void *address = subzone->quantum_address(q);
                    usword_t size = subzone->size(q);
                    if (subzone->write_barrier().range_has_marked_cards(address, size)) {
                        if (_use_pending) {
                            subzone->set_pending(q);
                        } else {
                            zone->scan_stack_push_block(address);
                        }
                    }
                }
            }

            // always continue
            return true;
        }
        
        // visitor function for large
        inline bool visit(Zone *zone, Large *large) {
            if (large->is_new()) {
                if (large->refcount() && !large->test_set_mark() && large->is_scanned()) {
                    if (_use_pending) {
                        large->set_pending();
                    } else {
                        zone->scan_stack_push_block(large->address());
                    }
                }
            } else {
                if (!large->test_set_mark() && large->is_scanned()) {
                    void *address = large->address();
                    usword_t size = large->size();
                    if (large->write_barrier().range_has_marked_cards(address, size)) {
                        if (_use_pending) {
                            large->set_pending();
                        } else {
                            zone->scan_stack_push_block(large->address());
                        }
                    }
                }
            }

            // always continue
            return true;
        }
    };
    void MemoryScanner::scan_retained_and_old_blocks() {
        // set up retain and old visitor
        scan_retained_and_old_blocks_visitor visitor(*this);
        visitAllocatedBlocks(_zone, visitor);

        // set some pending flag
        set_some_pending();
    }
    
    
    //
    // scan_root_ranges
    //
    // Add all root ranges to the scanner.
    // Since this can take a while, copy the roots first.  New values will get put on the enlivening queue.
    void MemoryScanner::scan_root_ranges() {
        // to avoid extra allocation, "borrow" the garbage list to snapshot the roots table.
        PointerList &list = _zone->garbage_list();
        _zone->copy_roots(list);
        void **roots = (void **)list.buffer();
        for (usword_t i = 0, count = list.count(); i < count; i++) {
            Range range(roots[i], sizeof(void*));
            scan_range(range);
        }
    }
    
    
    //
    // scan_threads
    //
    // Scan all the stacks for unmarked references.
    //
    void MemoryScanner::scan_thread_ranges(bool withSuspend, bool includeCurrentThread) {
        Zone::thread_scanner_t scanner = includeCurrentThread ?
        ^(Thread *thread) {
            // scan all known threads.
            thread->scan_thread(*this, withSuspend);
        }
        :
        ^(Thread *thread) {
            // filter out the current thread.
            if (!thread->is_current_thread()) {
                thread->scan_thread(*this, withSuspend);
            }
        };
        _zone->scan_registered_threads(scanner);
    }


    //
    // check_roots
    //
    // Scan root blocks.
    //
    void MemoryScanner::check_roots() {
        scan_retained_blocks();
        scan_root_ranges();
    }
    
    
    //
    // scan_pending_blocks
    //
    // Scan until there are no pending blocks
    //
    struct scan_pending_blocks_visitor {
    
        MemoryScanner &_scanner;                               // visiting scanner
        
        scan_pending_blocks_visitor(MemoryScanner &scanner) : _scanner(scanner) {}
        
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            if (subzone->is_pending(q)) {
                subzone->clear_pending(q);
                _scanner.scan_for_unmarked_blocks(subzone, q, subzone->quantum_address(q));
            }
            
            // always continue
            return true;
        }
        
        inline bool visit(Zone *zone, Large *large) {
            if (large->is_pending()) {
                large->clear_pending();
                _scanner.scan_for_unmarked_blocks(large, large->address());
            }
            
            // always continue
            return true;
        }
    };
    void MemoryScanner::scan_pending_blocks() {
        // set up the visitor
        scan_pending_blocks_visitor visitor(*this);
        
        // coalesce admin ranges
        _should_coalesce = true;
        
        // iterate through all the blocks
        visitAllocatedBlocks(_zone, visitor);

        // flush last coalesced range
        scan_range((void *)NULL, (void *)NULL);
        
        // don't coalesce other ranges
        _should_coalesce = false;
    }
    
    
    //
    // scan_pending_until_done
    //
    // Scan through blocks that are marked as pending until there are no new pending.
    //
    void MemoryScanner::scan_pending_until_done() {
        if (use_pending()) {
            while (is_some_pending()) {
                // clear some pending flag
                clear_some_pending();
                
                // scan all admin
                scan_pending_blocks();
            }
        } else {
            while (!_zone->scan_stack_is_empty()) {
                if (_zone->scan_stack_is_range()) {
                    Range range = _zone->scan_stack_pop_range();
                    WriteBarrier *wb = _zone->scan_stack_is_write_barrier() ? _zone->scan_stack_pop_write_barrier() : NULL;
                    scan_range(range, wb);
                } else {
                    void *block = _zone->scan_stack_pop_block();
                    
                    if (_zone->in_subzone_memory(block)) {
                        Subzone *subzone = Subzone::subzone(block);
                        usword_t q = subzone->quantum_index(block);
                        scan_for_unmarked_blocks(subzone, q, block);
                    } else /* if (in_large_memory(block)) */ {
                        Large *large = Large::large(block);
                        scan_for_unmarked_blocks(large, block);
                    }
                }
            }
        }
    }


    //
    // scan
    //
    // Scans memory for reachable objects.  All reachable blocks will be marked.
    //
    void MemoryScanner::scan() {
        _amount_scanned = 0;
        _blocks_scanned = 0;
        
#if defined(DEBUG)        
        _blocks_checked = 0;
        uint64_t start_time = micro_time();
#endif

        // scan root objects
        check_roots();
        
        // scan through all pending blocks until there are no new pending
        scan_pending_until_done();

        // scan all the stacks quickly (this lets threads enter & exit concurrently)
        const bool includeCurrentThread = !_zone->multithreaded;
        scan_thread_ranges(false, includeCurrentThread);
        
        // scan external memory
        scan_external();

        // scan through all pending blocks until there are no new pending
        scan_pending_until_done();
        
        // block all concurrent mutators from this point.
        AUTO_PROBE(auto_scan_barrier());
        scan_barrier();
        
#if defined(DEBUG)        
        uint64_t suspend_start_time = micro_time();
#endif

        // scan all the stacks completely AFTER the scan barrier to prevent a known race condition between
        // enlivening the root of a graph of objects and storing a subgraph soley on the stack.
        // See <rdar://problem/5698807>, and auto_tester/EnliveningRace.m for more information.
        scan_thread_ranges(true, includeCurrentThread);
        scan_pending_until_done();
        AUTO_PROBE(auto_probe_end_thread_scan());
        
#if defined(DEBUG)
        unsigned suspend_end_time = (unsigned)(micro_time() - suspend_start_time);
#endif

        // scan associative references, until there are no new pending
        _zone->scan_associations(*this);
        
#if defined(DEBUG)
        unsigned end_time = (unsigned)(micro_time() - start_time);
        unsigned thread_count = 0;
        for (Thread *thread = _zone->threads(); thread; thread = thread->next()) thread_count++;
        
        if (Environment::print_scan_stats) {
            printf("%s scan %10u (%3lu%%) bytes in %5u usecs, %6u blocks checked, %6u (%3lu%%) blocks scanned, %4u usecs per %3u threads, scan stack max %lu\n",
                   is_partial() ? "Partial" : "Full   ",
                   (unsigned)_amount_scanned, (unsigned)_amount_scanned * 100 / _zone->statistics().size(),
                   end_time,
                   (unsigned)_blocks_checked,
                   (unsigned)_blocks_scanned, (unsigned)_blocks_scanned * 100 / _zone->statistics().count(),
                   suspend_end_time/thread_count, thread_count,
                   _zone->scan_stack_max());
        }
#endif

    }

    //
    // scan_barrier
    //
    // Used by collectors to synchronize with concurrent mutators.
    //
    void MemoryScanner::scan_barrier() {}
};

