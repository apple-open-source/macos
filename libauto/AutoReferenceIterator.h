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
    AutoReferenceIterator.h
    Replaces MemoryScanner by generalizing heap scanning.
    Copyright (c) 2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_REFERENCE_ITERATOR__
#define __AUTO_REFERENCE_ITERATOR__


#include "AutoAdmin.h"
#include "AutoDefs.h"
#include "AutoLarge.h"
#include "AutoRegion.h"
#include "AutoZone.h"
#include "AutoBlockIterator.h"

namespace Auto {

    //----- ReferenceIterator -----//
    
    enum ReferenceKind {
        kRootReference = 0,
        kRetainedReference,
        kThreadLocalReference,
        kRegistersReference,
        kStackReference,
        kConservativeHeapReference,
        kExactHeapReference,
        kAssociativeReference
    };
    
    // tagged union describing a slot reference.
    class ReferenceInfo {
        ReferenceKind       _kind;
        union {
            WriteBarrier    *_wb;        // kind == kConservativeHeapReference || kExactHeapReference
            Thread          *_thread;    // kind == kStackReference
            void            *_key;       // kind == kAssociativeReference
        };
    public:
        ReferenceInfo(ReferenceKind kind) : _kind(kind), _wb(NULL) {}
        ReferenceInfo(ReferenceKind kind, WriteBarrier *wb) : _kind(kind), _wb(wb) {}
        ReferenceInfo(ReferenceKind kind, Thread *thread) : _kind(kind), _thread(thread) {}
        ReferenceInfo(void *key) : _kind(kAssociativeReference), _key(key) {}

        ReferenceKind       kind()      { return _kind; }
        WriteBarrier       &wb()        { return *_wb; }
        Thread             &thread()    { return *_thread; }
        void               *key()       { return _key; }
    };
    
    //
    // Visits all live block references. The Configuration type parameter must contains 3 typedefs:
    //
    //  struct MyConfiguration {
    //      typedef MyReferenceVisitor ReferenceVisitor;
    //      typedef MyPendingStack PendingStack;
    //      typedef GenerationalScanningStrategy ScanningStrategy;
    //  };
    // 
    //  The ReferenceVisitor type must implement the following methods:
    //
    // class MyReferenceVistor {
    // public:
    //      void visit(const ReferenceInfo &info, void **slot, Subzone *subzone, usword_t q);
    //      void visit(const ReferenceInfo &info, void **slot, Large *large);
    // };
    //
    // The info parameter classifies the reference slot, root, stack, conservatively scanned block, exact scanned block, etc. It is a tagged
    // union that contains a pointer to the controlling write-barrier, Thread* or associtiative reference key.
    // The block being referenced is either a small/medium block, represented by the subzone / quantum pair, or a Large block.
    //
    // The ScanningStrategy type implements how objects are scanned. With careful forward declarations,
    // it is possible to use ReferenceIterator<MyConfiguration>::FullScanningStrategy or GenerationalScanningStrategy.
    // Consult those types for more information on how to implement other scanning strategies.
    //
    // The PendingStack type should be a template type for use with any of a family of ReferenceIterators. Therefore, it
    // must also provide a rebind as follows:
    //
    // template <typename RefererenceIterator> class MyPendingStack {
    //      void push(Subzone *subzone, usword_t q);
    //      void push(Large *large);
    //      void scan(ReferenceIterator &scanner);
    //      template <typename U> struct rebind { typedef MyPendingStack<U> other; };
    // };
    //
    // Periodically, the ReferenceIterator will call the scan() method, which pops all pushed blocks and passes them to
    // scanner.scan(Subzone *subzone, usword_t) or scanner.scan(Large *large).
    // Perhaps marking should also be performed in the pending stack?
    //
    
    template <class Configuration> class ReferenceIterator {
    private:
        typedef typename Configuration::ReferenceVisitor ReferenceVisitor;
        typedef typename Configuration::PendingStack PendingStack;
        Zone                       *_zone;                                      // zone containing blocks
        ReferenceVisitor           &_visitor;                                   // object visiting blocks
        PendingStack               &_pending_stack;                             // stack for deferred scanning.
        usword_t                    _bytes_scanned;                             // amount of memory scanned (in bytes)
        usword_t                    _blocks_scanned;                            // number of blocks scanned
        void                       *_stack_bottom;                              // stack pointer of current thread.
        
    public:
        // provide a way to rebind this template type to another just like STL allocators can do.
        template <typename U> struct rebind { typedef ReferenceIterator<U> other; };
    
        //
        // Constructor
        //
        ReferenceIterator(Zone *zone, ReferenceVisitor &visitor, PendingStack &stack, void *stack_bottom = (void *)auto_get_sp())
            : _zone(zone), _visitor(visitor), _pending_stack(stack), _bytes_scanned(0), _blocks_scanned(0), _stack_bottom(stack_bottom)
        {
        }
        
        // most primitive scanning operation, scans a single pointer-sized word of memory, checking the pointer
        // to see if it actually points in the collector heap. if it does, it is marked. If it a block that should be
        // scanned itself, then it is pushed on to the pending stack for later examination.
        
        inline void scan_reference(ReferenceInfo &info, void **reference) {
            void *pointer = *reference;
            if (pointer == NULL) return;
            if (_zone->in_subzone_memory(pointer)) {
                Subzone *subzone = Subzone::subzone(pointer);
                usword_t q = subzone->quantum_index_unchecked(pointer);
                if (subzone->block_is_start(q)) {
                    _visitor.visit(info, reference, subzone, q);
                    if (!subzone->test_set_mark(q) && Configuration::ScanningStrategy::should_scan(subzone, q)) {
                        _pending_stack.push(subzone, q);
                    }
                }
            } else if (_zone->in_large_memory(pointer) && Large::is_start(pointer)) {
                Large *large = Large::large(pointer);
                _visitor.visit(info, reference, large);
                if (!large->test_set_mark() && Configuration::ScanningStrategy::should_scan(large)) {
                    _pending_stack.push(large);
                }
            }
        }
        
        // compatiblity with Thread::scan_other_thread() and WriteBarrier::scan_marked_ranges().
        // when we can use blocks from templates this layer will be unnecessary.
        
        inline void scan_range(ReferenceInfo &info, Range &range) {
            void **slots = (void **)range.address();
            void **last = (void **)displace(slots, range.size() - sizeof(void*));
            while (slots <= last)
                scan_reference(info, slots++);
        }
        
        static void scan_thread_range(Thread *thread, Range &range, void *arg) {
            ReferenceIterator *scanner = (ReferenceIterator*)arg;
            ReferenceKind kind = (range.end() == thread->stack_base() ? kStackReference : kRegistersReference);
            ReferenceInfo info(kind, thread);
            scanner->scan_range(info, range);
        }

        static void scan_exact_range(Range &range, WriteBarrier *wb, void *arg) {
            ReferenceIterator *scanner = (ReferenceIterator*)arg;
            ReferenceInfo info(kExactHeapReference, wb);
            scanner->scan_range(info, range);
        }
        
        static void scan_conservative_range(Range &range, WriteBarrier *wb, void *arg) {
            ReferenceIterator *scanner = (ReferenceIterator*)arg;
            ReferenceInfo info(kConservativeHeapReference, wb);
            scanner->scan_range(info, range);
        }
        
        inline void scan(Subzone *subzone, usword_t q) {
            void *block = subzone->quantum_address(q);
            usword_t size = subzone->size(q);
            usword_t layout = subzone->layout(q);
            WriteBarrier *wb = &subzone->write_barrier();
            if (layout & AUTO_OBJECT) {
                Configuration::ScanningStrategy::scan_object(*this, block, size, _zone->layout_map_for_block(block), wb);
            } else {
                Configuration::ScanningStrategy::scan_block(*this, block, size, wb);
            }
        }
        
        inline void scan(Large *large) {
            void *block = large->address();
            usword_t size = large->size();
            usword_t layout = large->layout();
            WriteBarrier *wb = &large->write_barrier();
            if (layout & AUTO_OBJECT) {
                Configuration::ScanningStrategy::scan_object(*this, block, size, _zone->layout_map_for_block(block), wb);
            } else {
                Configuration::ScanningStrategy::scan_block(*this, block, size, wb);
            }
        }
        
        class retained_or_local_blocks_visitor {
            ReferenceIterator &_scanner;
            ReferenceVisitor &_visitor;
        public:
            retained_or_local_blocks_visitor(ReferenceIterator &scanner) : _scanner(scanner), _visitor(scanner._visitor) {}
            
            // visitor function for subzone
            inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
                bool has_refcount = subzone->has_refcount(q);
                if ((has_refcount || subzone->is_thread_local(q))) {
                    ReferenceInfo info(has_refcount ? kRetainedReference : kThreadLocalReference);
                    _visitor.visit(info, NULL, subzone, q);
                
                    // eagerly scan each small/medium block as it is encountered. blocks reachable will be queued.
                    if (!subzone->test_set_mark(q) && Configuration::ScanningStrategy::should_scan(subzone, q)) {
                        _scanner.scan(subzone, q);
                    }
                }

                // always continue
                return true;
            }
            
            // visitor function for large
            inline bool visit(Zone *zone, Large *large) {
                if (large->refcount()) {
                    ReferenceInfo info(kRetainedReference);
                    _scanner._visitor.visit(info, NULL, large);

                    // eagerly scan each large block as it is encountered. blocks reachable will be queued.
                    if (!large->test_set_mark() && Configuration::ScanningStrategy::should_scan(large)) {
                        _scanner.scan(large);
                    }
                }

                // always continue
                return true;
            }
        };
        
        inline void scan_roots() {
            // visit the retained and local blocks.
            retained_or_local_blocks_visitor visitor(*this);
            visitAllocatedBlocks(_zone, visitor);
            _pending_stack.scan(*this);
            
            // visit registered roots.
            PtrVector roots;
            _zone->copy_roots(roots);
            ReferenceInfo info(kRootReference);
            for (usword_t i = 0, count = roots.size(); i < count; i++) {
                scan_reference(info, (void**)roots[i]);
                _pending_stack.scan(*this);
            }
        }

        // typedef void (&scanner_block_t) (Range *range);
        
        static void scan_one_thread(Thread *thread, void *arg) {
            // Until <rdar://problem/6393321&6182276> are fixed, have to use the function pointer variants.
            ReferenceIterator* scanner = (ReferenceIterator *)arg;
            if (thread->is_current_thread()) {
                thread->scan_current_thread(&ReferenceIterator::scan_thread_range, arg, scanner->_stack_bottom);
            } else {
                thread->scan_other_thread(&ReferenceIterator::scan_thread_range, arg, true);
            }
        }
        
        inline void scan_threads() {
            // TODO:  coordinate with dying threads.
            // Until <rdar://problem/6393321&6182276> are fixed, have to use a function pointer.
            // scanner_block_t block_scanner = ^(Range *range) { this->scan_range(kStackReference, *range); };
            _zone->scan_registered_threads(&ReferenceIterator::scan_one_thread, this);
        }
        
        inline void push_associations(void *block) {
            AssocationsHashMap &associations(_zone->assocations());
            AssocationsHashMap::iterator i = associations.find(block);
            if (i != associations.end()) {
                ObjectAssocationHashMap *refs = i->second;
                for (ObjectAssocationHashMap::iterator j = refs->begin(); j != refs->end(); j++) {
                    void *key = j->first;
                    void *value = j->second;
                    ReferenceInfo info(key);
                    if (_zone->in_subzone_memory(value)) {
                        Subzone *subzone = Subzone::subzone(value);
                        usword_t q = subzone->quantum_index(value);
                        _visitor.visit(info, (void**)block, subzone, q);
                        if (!subzone->test_set_mark(q) && Configuration::ScanningStrategy::should_scan(subzone, q)) {
                            _pending_stack.push(subzone, q);
                        }
                    } else {
                        Large *large = Large::large(value);
                        _visitor.visit(info, (void**)block, large);
                        if (!large->test_set_mark() && Configuration::ScanningStrategy::should_scan(large)) {
                            _pending_stack.push(large);
                        }
                    }
                }
            }
        }
        
        // internal scanning strategy used to implement association scanning.
        class AssociationScanningStrategy;
        struct AssociationScanningConfiguration;
        typedef typename rebind<AssociationScanningConfiguration>::other AssociationReferenceIterator;
        
        struct AssociationScanningConfiguration {
            typedef typename ReferenceIterator::ReferenceVisitor ReferenceVisitor;
            typedef typename PendingStack::template rebind<AssociationReferenceIterator>::other PendingStack;
            typedef typename AssociationReferenceIterator::AssociationScanningStrategy ScanningStrategy;
            typedef typename Configuration::ScanningStrategy::template rebind<AssociationReferenceIterator>::other OriginalScanningStrategy;
        };
            
        class AssociationScanningStrategy {
        public:
            inline static bool should_scan(Subzone *subzone, usword_t q) { return Configuration::OriginalScanningStrategy::should_scan(subzone, q); }
            inline static bool should_scan(Large *large) { return Configuration::OriginalScanningStrategy::should_scan(large); }
            
            inline static void scan_block(ReferenceIterator &scanner, void *block, usword_t size, WriteBarrier *wb) {
                Configuration::OriginalScanningStrategy::scan_block(scanner, block, size, wb);
                scanner.push_associations(block);
            }
            inline static void scan_object(ReferenceIterator &scanner, void *object, usword_t size, const unsigned char* map, WriteBarrier *wb) {
                Configuration::OriginalScanningStrategy::scan_object(scanner, object, size, map, wb);
                scanner.push_associations(object);
            }
        };
        
        inline bool associations_should_be_scanned(void *block) {
            if (_zone->in_subzone_memory(block)) {
                Subzone *subzone = Subzone::subzone(block);
                return subzone->block_is_start(block) && subzone->is_marked(block);
            } else if (_zone->in_large_memory(block) && Large::is_start(block)) {
                return Large::large(block)->is_marked();
            }
            // <rdar://problem/6463922> Treat non-block pointers as unconditionally live.
            return true;
        }

        inline void scan_associations() {
            typename AssociationScanningConfiguration::PendingStack pendingStack;
            AssociationReferenceIterator associationScanner(_zone, _visitor, pendingStack);
            
            // Prevent other threads from breaking existing associations. We already own the enlivening lock.
            SpinLock lock(_zone->associations_lock());
            AssocationsHashMap &associations(_zone->assocations());
        
            // consider associative references. these are only reachable if their primary block is.
            for (AssocationsHashMap::iterator i = associations.begin(), end = associations.end(); i != end; i++) {
                void *block = i->first;
                if (associations_should_be_scanned(block)) {
                    ObjectAssocationHashMap *refs = i->second;
                    for (ObjectAssocationHashMap::iterator j = refs->begin(); j != refs->end(); j++) {
                        void *key = j->first;
                        void *value = j->second;
                        ReferenceInfo info(key);
                        if (_zone->in_subzone_memory(value)) {
                            Subzone *subzone = Subzone::subzone(value);
                            usword_t q = subzone->quantum_index(value);
                            _visitor.visit(info, (void**)block, subzone, q);
                            if (!subzone->test_set_mark(q) && Configuration::ScanningStrategy::should_scan(subzone, q)) {
                                pendingStack.push(subzone, q);
                            }
                        } else {
                            Large *large = Large::large(value);
                            _visitor.visit(info, (void**)block, large);
                            if (!large->test_set_mark() && Configuration::ScanningStrategy::should_scan(large)) {
                                pendingStack.push(large);
                            }
                        }
                    }
                    pendingStack.scan(associationScanner);
                }
            }
        }
        
        inline void scan() {
            scan_roots();
            scan_threads();
            scan_associations();
            // TODO:  enlivening.
        }
    };
    
    // Predefined scanning strategies.

    template <typename ReferenceIterator> class FullScanningStrategy {
    public:
        // provide a way to rebind this template type to another just like STL allocators can do.
        template <typename U> struct rebind { typedef FullScanningStrategy<U> other; };

        inline static bool should_scan(Subzone *subzone, usword_t q) {
            return subzone->is_scanned(q);
        }
        
        inline static bool should_scan(Large *large) {
            return large->is_scanned();
        }
        
        // conservative block scan.
        inline static void scan_block(ReferenceIterator &scanner, void *block, usword_t size, WriteBarrier *wb) {
            void **slots = (void **)block;
            void **last = (void **)displace(slots, size - sizeof(void*));
            ReferenceInfo info(kConservativeHeapReference, wb);
            while (slots <= last) {
                scanner.scan_reference(info, slots);
                ++slots;
            }
        }

        // exact object scan.
        inline static void scan_object(ReferenceIterator &scanner, void *object, usword_t size, const unsigned char* map, WriteBarrier *wb) {
            ReferenceInfo exactInfo(kExactHeapReference, wb);
            void **slots = (void **)object;
            void **last = (void **)displace(slots, size - sizeof(void*));
            if (map == NULL) {
                // Special-case:  the compiler optimizes layout generation when an object should be scanned completely conservatively. Technically, this isn't
                // correct, because the logical size of the object is all pointers, and the remainder should be scanned conservatively. The scanner doesn't know
                // the physical size of the object, so we make a best guess here. I found this while implementing auto_gdb_enumerate_roots(), which was simply
                // using the layout of the object to determine whether the reference was conservative or not. A bug should be filed.
                while (slots <= last) scanner.scan_reference(exactInfo, slots++);
                return;
            }
            // while not '\0' terminator
            while (unsigned data = *map++) {
                // extract the skip and run
                unsigned skip = data >> 4;
                unsigned run = data & 0xf;
                
                // advance the reference by the skip
                slots += skip;
                
                while (run--) scanner.scan_reference(exactInfo, slots++);
            }

            // since objects can be allocated with extra data at end, scan the remainder conservatively.
            ReferenceInfo conservativeInfo(kConservativeHeapReference, wb);
            while (slots <= last) scanner.scan_reference(conservativeInfo, slots++);
        }
    };
    
    template <typename ReferenceIterator> class GenerationalScanningStrategy {
    public:
        // provide a way to rebind this template type to another just like STL allocators can do.
        template <typename U> struct rebind { typedef GenerationalScanningStrategy<U> other; };
        
        inline static bool should_scan(Subzone *subzone, usword_t q) {
            // only scan a block, if it has ever been written through its write-barrier.
            return subzone->is_scanned(q) && subzone->write_barrier().range_has_marked_cards(subzone->quantum_address(q), subzone->size(q));
        }
        
        inline static bool should_scan(Large *large) {
            return large->is_scanned() && large->write_barrier().range_has_marked_cards(large->address(), large->size());
        }
        
        inline static void scan_block(ReferenceIterator &scanner, void *block, usword_t size, WriteBarrier *wb) {
            wb->scan_marked_ranges(block, size, &ReferenceIterator::scan_conservative_range, &scanner);
        }

        inline static void scan_object(ReferenceIterator &scanner, void *object, usword_t size, const unsigned char* map, WriteBarrier *wb) {
            if (map == NULL) {
                // Special-case:  the compiler optimizes layout generation when an object should be scanned completely conservatively. Technically, this isn't
                // correct, because the logical size of the object is all pointers, and the remainder should be scanned conservatively. The scanner doesn't know
                // the physical size of the object, so we make a best guess here. I found this while implementing auto_gdb_enumerate_roots(), which was simply
                // using the layout of the object to determine whether the reference was conservative or not. A bug should be filed.
                wb->scan_marked_ranges(object, size, &ReferenceIterator::scan_exact_range, &scanner);
                return;
            }
            void **slots = (void **)object;
            void **last = (void **)displace(slots, size - sizeof(void*));
            // while not '\0' terminator
            while (unsigned data = *map++) {
                // extract the skip and run
                unsigned skip = data >> 4;
                unsigned run = data & 0xf;
                
                // advance the reference by the skip
                slots += skip;
                
                wb->scan_marked_ranges(slots, run, &ReferenceIterator::scan_exact_range, &scanner);
                slots += run;
            }

            // since objects can be allocated with extra data at end, scan the remainder conservatively.
            if (slots < last) wb->scan_marked_ranges(slots, (1 + last - slots) * sizeof(void*), &ReferenceIterator::scan_conservative_range, &scanner);
        }
    };
        
};

#endif // __AUTO_REFERENCE_ITERATOR__
