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
    ZoneCollectors.cpp
    Zone Scanning Algorithms
    Copyright (c) 2009-2001 Apple Inc. All rights reserved.
 */

#include "ReferenceIterator.h"
#include "Zone.h"

namespace Auto {
    
    // BitmapPendingStack uses the pending bitmap as its store.
    template <typename ReferenceIterator> class BitmapPendingStack {
    public:
        ReferenceIterator *_scanner;
        CollectionTimer *_collection_timer;
        pthread_mutex_t _scan_mutex;
        pthread_cond_t _scan_cond;
        Region *_scan_region;
        SubzoneRangeIterator _subzone_iterator;
        Large *_scan_large;
        usword_t _scanning_threads;
        bool _pendedLargeBlock;
        bool _pendedSubzoneBlock;

        BitmapPendingStack() : _scanner(NULL), _collection_timer(NULL), _subzone_iterator(NULL), _scanning_threads(0), _pendedLargeBlock(false), _pendedSubzoneBlock(false)
        {
            pthread_mutex_init(&_scan_mutex, NULL);
            pthread_cond_init(&_scan_cond, NULL);
        }
        
        const PendingStackHint hints() { return PendingStackIsFixedSize | PendingStackChecksPendingBitmap | PendingStackSupportsThreadedScan; }

        void set_timer(CollectionTimer *timer) { _collection_timer = timer; }
        
        void push(Subzone *subzone, usword_t q) {
            if (!subzone->test_and_set_pending(q, false)) {
                _pendedSubzoneBlock = true;
                Subzone::PendingCountAccumulator *accumulator = (Subzone::PendingCountAccumulator *)subzone->region()->zone()->registered_thread().pending_count_accumulator();
                if (accumulator)
                    accumulator->pended_in_subzone(subzone);
                else
                    subzone->add_pending_count(1);
            }
        }
        
        void push(Large *large) {
            _pendedLargeBlock = true;
            large->set_pending();
        }
        
        static bool mark(Subzone *subzone, usword_t q) { return subzone->test_and_set_mark(q); }
        static bool mark(Large *large) { return large->test_and_set_mark(); }
        bool is_marked(Subzone *subzone, usword_t q) { return subzone->is_marked(q); }
        bool is_marked(Large *large) { return large->is_marked(); }
        
        void visit(Zone *zone, Large *large) {
            Subzone::PendingCountAccumulator info(zone->registered_thread());
            CollectionTimer::ScanTimer timer;
            if (_collection_timer) timer.start();
            if (large->is_pending()) {
                worker_print("scanning large %p\n", large);
                large->clear_pending();
                _scanner->scan(large);
                zone->statistics().add_blocks_scanned(1);
                zone->statistics().add_bytes_scanned(large->size());
            }
            if (_collection_timer) {
                timer.stop();
                _collection_timer->scan_timer().add_time(timer);
            }
        }
        
        void visit(Zone *zone, Subzone *sz) {
            usword_t blocks_scanned = 0;
            usword_t bytes_scanned = 0;
            Subzone::PendingCountAccumulator info(zone->registered_thread());
            CollectionTimer::ScanTimer timer;
            if (_collection_timer) timer.start();
            worker_print("scanning subzone %p\n", sz);
            int32_t scanned_count;
            do {
                Bitmap::AtomicCursor cursor = sz->pending_cursor();
                scanned_count = 0;
                usword_t q = cursor.next_set_bit();
                while (q != (usword_t)-1) {
                    if (!sz->is_free(q)) {
                        _scanner->scan(sz, q);
                        blocks_scanned++;
                        bytes_scanned += sz->size(q);
                    }
                    scanned_count++;
                    q = cursor.next_set_bit();
                }
                info.flush_count();
            } while (sz->add_pending_count(-scanned_count) != 0);
            if (_collection_timer) {
                timer.stop();
                _collection_timer->scan_timer().add_time(timer);
            }
            zone->statistics().add_blocks_scanned(blocks_scanned);
            zone->statistics().add_bytes_scanned(bytes_scanned);
        }
        
        const inline bool visit_larges_concurrently()   const { return true; }

        void scan(ReferenceIterator &scanner) {
            _scanner = &scanner;
            do {
                _pendedSubzoneBlock = false;
                _pendedLargeBlock = false;
                visitAllocatedBlocks_concurrent(_scanner->zone(), *this);
            } while (_pendedSubzoneBlock || _pendedLargeBlock);
        }
        
        template <typename U> struct rebind { typedef BitmapPendingStack<U> other; };
    };
    
    class PartialCollectionVisitor {
    protected:
        struct Configuration;
        typedef ReferenceIterator<Configuration> PartialReferenceIterator;
        
        struct Configuration {
            typedef PartialCollectionVisitor ReferenceVisitor;
            typedef BitmapPendingStack<PartialReferenceIterator> PendingStack;
            typedef GenerationalScanningStrategy<PartialReferenceIterator> ScanningStrategy;
        };

    public:
        void visit(const ReferenceInfo &info, void **slot, Subzone *subzone, usword_t q) {}
        void visit(const ReferenceInfo &info, void **slot, Large *large) {}
        
        void scan(Zone *zone, void *stack_bottom, CollectionTimer &timer) {
            Configuration::PendingStack stack;
            const bool should_enliven = true;
            PartialReferenceIterator scanner(zone, *this, stack, stack_bottom, should_enliven, zone->repair_write_barrier());
            if (timer.scan_timer_enabled())
                stack.set_timer(&timer);
            scanner.scan();
        }
    };
    
    class FullCollectionVisitor {
    protected:
        struct Configuration;
        typedef ReferenceIterator<Configuration> FullReferenceIterator;
        struct Configuration {
            typedef FullCollectionVisitor ReferenceVisitor;
            typedef BitmapPendingStack<FullReferenceIterator> PendingStack;
            typedef FullScanningStrategy<FullReferenceIterator> ScanningStrategy;
        };
    public:
        void visit(const ReferenceInfo &info, void **slot, Subzone *subzone, usword_t q) {}
        void visit(const ReferenceInfo &info, void **slot, Large *large) {}
        
        void scan(Zone *zone, void *stack_bottom, CollectionTimer &timer) {
            Configuration::PendingStack stack;
            const bool should_enliven = true, dont_repair = false;
            FullReferenceIterator scanner(zone, *this, stack, stack_bottom, should_enliven, dont_repair);
            if (timer.scan_timer_enabled())
                stack.set_timer(&timer);
            scanner.scan();
        }
        
        const boolean_t keep_statistics() const { return true; }
    };
    
    //
    // collect_partial
    //
    // Performs a partial (generational) collection.
    //
    void Zone::collect_partial(void *current_stack_bottom, CollectionTimer &timer) {
        PartialCollectionVisitor visitor;
        visitor.scan(this, current_stack_bottom, timer);
    }
    
    //
    // collect_full
    //
    void Zone::collect_full(void *current_stack_bottom, CollectionTimer &timer) {
        FullCollectionVisitor visitor;
        visitor.scan(this, current_stack_bottom, timer);
    }

} // namespace Auto
