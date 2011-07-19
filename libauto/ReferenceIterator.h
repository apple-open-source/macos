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
    ReferenceIterator.h
    Generalized Heap Scanner
    Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_REFERENCE_ITERATOR__
#define __AUTO_REFERENCE_ITERATOR__


#include "Admin.h"
#include "Definitions.h"
#include "Large.h"
#include "Zone.h"
#include "BlockIterator.h"

namespace Auto {

    //----- ReferenceIterator -----//
    
    enum ReferenceKind {
        kRootReference = 0,
        kRetainedReference,
        kRegistersReference,
        kRegistersIndirectReference,
        kStackReference,
        kStackIndirectReference,
        kConservativeHeapReference,
        kConservativeHeapIndirectReference,
        kThreadLocalReference,
        kOldReference,
        kEnlivenedReference,
        kGarbageReference,
        kWeakReference,
        kWeakSlotReference,
        kExactHeapReference,
        kAllPointersHeapReference,
        kAssociativeReference,
        kReferenceKindCount,
    };
    
    // tagged union describing a slot reference.
    class ReferenceInfo {
        const ReferenceKind       _kind;
        union {
            WriteBarrier    *_wb;           // kind == kConservativeHeapReference || kExactHeapReference || kAllPointersHeapReference
            Thread          *_thread;       // kind == kStackReference || kRegistersReference
            struct {
                void        *_object;       // kind == kAssociativeReference   
                void        *_key;
            };
        };
        // Uncomment this if you want to check for calls to the copy constructor.
        // ReferenceInfo(const ReferenceInfo &other) : _kind(kRootReference), _wb(NULL), _thread(NULL), _object(NULL), _key(NULL) {}
    public:
        ReferenceInfo(ReferenceKind kind) : _kind(kind), _object(NULL), _key(NULL) {}
        ReferenceInfo(ReferenceKind kind, WriteBarrier * const wb) : _kind(kind), _wb(wb) {}
        ReferenceInfo(ReferenceKind kind, Thread *thread) : _kind(kind), _thread(thread) {}
        ReferenceInfo(void *object, void *key) : _kind(kAssociativeReference), _object(object), _key(key) {}
        
        // automatically generated copy-constructor is perfectly fine.
        // ReferenceInfo(const ReferenceInfo &other) : _kind(other._kind), _object(other._object), _key(other._key) {}

        ReferenceKind      kind()       const { return _kind; }
        WriteBarrier       &wb()        const { return *_wb; }
        Thread             &thread()    const { return *_thread; }
        void               *object()    const { return _object; }
        void               *key()       const { return _key; }
        
        static const char  *name(ReferenceKind kind) {
            static const char *kReferenceKindNames[] = {
                "root",
                "retained",
                "registers", "registersIndirect",
                "stack", "stackIndirect",
                "conservative", "conservativeIndirect", 
                "threadLocal", "old", "enlivened", "garbage",
                "weak", "weakslot",
                "exact", "allPointers", "associative"
            };
            return kReferenceKindNames[kind];
        }
    };
    
    enum {
        PendingStackIsFixedSize = 0x1,
        PendingStackWantsEagerScanning = 0x2,
        PendingStackChecksPendingBitmap = 0x4,
        PendingStackSupportsThreadedScan = 0x8,
    };
    
    typedef uint32_t PendingStackHint;
    
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
    //      const PendingStackHint hints();
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
        void                       *_stack_bottom;                              // stack pointer of current thread.
        const bool                  _needs_enlivening_barrier;                  // need to enliven in this cycle?
        const bool                  _repair_write_barrier;                      // repairing write barrier in this cycle?
        
        // hints the pending stack provides about its implementation
        const inline bool test_pending_stack_hint(PendingStackHint hint) { return (_pending_stack.hints() & hint) == hint; }
        const inline bool prefer_shallow_pending_stack() { return !test_pending_stack_hint(PendingStackIsFixedSize); }
        const inline bool perform_eager_scanning() { return test_pending_stack_hint(PendingStackWantsEagerScanning); }
        const inline bool scan_pending() { return !test_pending_stack_hint(PendingStackChecksPendingBitmap); }
        const inline bool scan_threaded() { return test_pending_stack_hint(PendingStackSupportsThreadedScan); }
        
    public:
        // provide a way to rebind this template type to another just like STL allocators can do.
        template <typename U> struct rebind { typedef ReferenceIterator<U> other; };
    
        //
        // Constructor
        //
        ReferenceIterator(Zone *zone, ReferenceVisitor &visitor, PendingStack &stack, void *stack_bottom,
                          const bool needs_enlivening = false, const bool repair_write_barrier = false)
            : _zone(zone), _visitor(visitor), _pending_stack(stack), _stack_bottom(stack_bottom),
              _needs_enlivening_barrier(needs_enlivening), _repair_write_barrier(repair_write_barrier)
        {
        }
        
        Zone *zone() { return _zone; }
        
        void flush() { _pending_stack.scan(*this); }
        
        void push(Subzone *subzone, usword_t q) { _pending_stack.push(subzone, q); }
        void push(Large *large) { _pending_stack.push(large); }
        
        bool mark(Subzone *subzone, usword_t q) { return _pending_stack.mark(subzone, q); }
        bool mark(Large *large) { return _pending_stack.mark(large); }

        bool is_marked(Subzone *subzone, usword_t q) { return _pending_stack.is_marked(subzone, q); }
        bool is_marked(Large *large) { return _pending_stack.is_marked(large); }
        
        static bool should_scan(Subzone *subzone, usword_t q) { return Configuration::ScanningStrategy::should_scan(subzone, q); }
        static bool should_scan(Large *large) { return Configuration::ScanningStrategy::should_scan(large); }
        
        static bool should_mark(Subzone *subzone, usword_t q) { return Configuration::ScanningStrategy::should_mark(subzone, q); }
        static bool should_mark(Large *large) { return Configuration::ScanningStrategy::should_mark(large); }
        
        const unsigned char* layout_map_for_block(void *block) { return Configuration::ScanningStrategy::layout_map_for_block(_zone, block); }
        static bool visit_interior_pointers() { return Configuration::ScanningStrategy::visit_interior_pointers(); }
        static bool scan_threads_suspended() { return Configuration::ScanningStrategy::scan_threads_suspended(); }
        inline static pthread_rwlock_t *associations_lock(Zone *zone) { return Configuration::ScanningStrategy::associations_lock(zone); }
        
        // most primitive scanning operation, scans a single pointer-sized word of memory, checking the pointer
        // to see if it actually points in the collector heap. if it does, it is marked. If it a block that should be
        // scanned itself, then it is pushed on to the pending stack for later examination.
        
        inline void scan_reference(const ReferenceInfo &info, void **reference) __attribute__((always_inline)) {
            void *pointer = *reference;
            if (pointer == NULL) return;
            if (_zone->in_subzone_memory(pointer)) {
                Subzone *subzone = Subzone::subzone(pointer);
                usword_t q;
                if (subzone->block_is_start(pointer, &q)) {
                    _visitor.visit(info, reference, subzone, q);
                    if (!mark(subzone, q) && should_scan(subzone, q)) {
                        push(subzone, q);
                    }
                }
            } else if (_zone->block_is_start_large(pointer)) {
                Large *large = Large::large(pointer);
                _visitor.visit(info, reference, large);
                if (!mark(large) && should_scan(large)) {
                    push(large);
                }
            }
        }
        
        inline void scan_reference_indirect(const ReferenceInfo &info, const ReferenceInfo &indirectInfo, void **reference) {
            void *pointer = *reference;
            if (pointer == NULL) return;
            if (_zone->in_subzone_memory(pointer)) {
                Subzone *subzone = Subzone::subzone(pointer);
                usword_t q;
                if (subzone->block_is_start(pointer, &q)) {
                    _visitor.visit(info, reference, subzone, q);
                    if (!mark(subzone, q) && should_scan(subzone, q)) {
                        push(subzone, q);
                    }
                } else {
                    void *block = subzone->block_start(pointer, q);
                    if (block != NULL) {
                        _visitor.visit(indirectInfo, reference, subzone, subzone->quantum_index_unchecked(block));
                        // we don't MARK interior pointers, but the visitor may be interested.
                    }
                }
            } else if (_zone->block_is_start_large(pointer)) {
                Large *large = Large::large(pointer);
                _visitor.visit(info, reference, large);
                if (!mark(large) && should_scan(large)) {
                    push(large);
                }
            }
        }
        
        inline void scan_reference_repair_write_barrier(const ReferenceInfo &info, void **reference, WriteBarrier *wb) {
            void *pointer = *reference;
            if (pointer == NULL) return;
            if (_zone->in_subzone_memory(pointer)) {
                Subzone *subzone = Subzone::subzone(pointer);
                usword_t q;
                if (subzone->block_is_start(pointer, &q)) {
                    _visitor.visit(info, reference, subzone, q);
                    if (subzone->is_thread_local(q) || subzone->is_new(q)) {
                        wb->mark_card(reference);
                    }
                    if (!mark(subzone, q) && should_scan(subzone, q)) {
                        push(subzone, q);
                    }
                }
            } else if (_zone->block_is_start_large(pointer)) {
                Large *large = Large::large(pointer);
                _visitor.visit(info, reference, large);
                if (large->is_new()) {
                    wb->mark_card(reference);
                }
                if (!mark(large) && should_scan(large)) {
                    push(large);
                }
            }
        }
        
        // compatiblity with Thread::scan_other_thread() and WriteBarrier::scan_marked_ranges().
        // when we can use blocks from templates this layer will be unnecessary.
        
        inline void scan_range(const ReferenceInfo &info, const Range &range) {
            void **slots = (void **)range.address();
            void **last = (void **)displace(slots, range.size() - sizeof(void*));
            AUTO_PROBE(auto_probe_scan_range(slots, last));
            while (slots <= last)
                scan_reference(info, slots++);
        }

        inline void scan_range_indirect(const ReferenceInfo &info, const ReferenceInfo &indirectInfo, const Range &range) {
            void **slots = (void **)range.address();
            void **last = (void **)displace(slots, range.size() - sizeof(void*));
            AUTO_PROBE(auto_probe_scan_range(slots, last));
            while (slots <= last)
                scan_reference_indirect(info, indirectInfo, slots++);
        }
        
        inline void scan_range_repair_write_barrier(const ReferenceInfo &info, const Range &range, WriteBarrier *wb) {
            void **slots = (void **)range.address();
            void **last = (void **)displace(slots, range.size() - sizeof(void*));
            AUTO_PROBE(auto_probe_scan_range(slots, last));
            while (slots <= last)
                scan_reference_repair_write_barrier(info, slots++, wb);
        }
        
        static void scan_thread_range(Thread *thread, const Range &range, void *arg) {
            ReferenceIterator *scanner = (ReferenceIterator*)arg;
            ReferenceKind kind = (range.end() == thread->stack_base() ? kStackReference : kRegistersReference);
            ReferenceInfo info(kind, thread);
            if (scanner->visit_interior_pointers()) {
                ReferenceInfo interior_info(ReferenceKind(kind + 1), thread);
                scanner->scan_range_indirect(info, interior_info, range);
            } else
                scanner->scan_range(info, range);
            if (scanner->prefer_shallow_pending_stack()) scanner->flush();
        }

        static void scan_exact_range(const Range &range, WriteBarrier *wb, void *arg) {
            ReferenceIterator *scanner = (ReferenceIterator*)arg;
            ReferenceInfo info(kExactHeapReference, wb);
            if (scanner->_repair_write_barrier)
                scanner->scan_range_repair_write_barrier(info, range, wb);
            else
                scanner->scan_range(info, range);
        }
        
        static void scan_all_pointers_range(const Range &range, WriteBarrier *wb, void *arg) {
            ReferenceIterator *scanner = (ReferenceIterator*)arg;
            ReferenceInfo info(kAllPointersHeapReference, wb);
            if (scanner->_repair_write_barrier)
                scanner->scan_range_repair_write_barrier(info, range, wb);
            else
                scanner->scan_range(info, range);
        }
        
        static void scan_conservative_range(const Range &range, WriteBarrier *wb, void *arg) {
            ReferenceIterator *scanner = (ReferenceIterator*)arg;
            ReferenceInfo info(kConservativeHeapReference, wb);
            if (scanner->_repair_write_barrier)
                scanner->scan_range_repair_write_barrier(info, range, wb);
            else
                scanner->scan_range(info, range);
        }
        
        inline void scan(Subzone *subzone, usword_t q) {
            void *block = subzone->quantum_address(q);
            usword_t size = subzone->size(q);
            usword_t layout = subzone->layout(q);
            WriteBarrier *wb = &subzone->write_barrier();
            if (layout & AUTO_OBJECT) {
                Configuration::ScanningStrategy::scan_object(*this, block, size, layout, layout_map_for_block(block), wb);
            } else {
                Configuration::ScanningStrategy::scan_block(*this, block, size, layout, wb);
            }
        }
        
        inline void scan(Large *large) {
            void *block = large->address();
            usword_t size = large->size();
            usword_t layout = large->layout();
            WriteBarrier *wb = &large->write_barrier();
            if (layout & AUTO_OBJECT) {
                Configuration::ScanningStrategy::scan_object(*this, block, size, layout, layout_map_for_block(block), wb);
            } else {
                Configuration::ScanningStrategy::scan_block(*this, block, size, layout, wb);
            }
        }
        
        class rooted_blocks_visitor {
            ReferenceIterator &_scanner;
            ReferenceVisitor &_visitor;
        public:
            rooted_blocks_visitor(ReferenceIterator &scanner) : _scanner(scanner), _visitor(scanner._visitor) {}
            
            // visitor function for subzone
            inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
                bool is_local = subzone->is_thread_local(q);
                bool has_refcount = subzone->has_refcount(q);
                if (is_local || has_refcount || should_mark(subzone, q)) {
                    ReferenceInfo info(is_local ? kThreadLocalReference : has_refcount ? kRetainedReference : kOldReference);
                    _visitor.visit(info, NULL, subzone, q);
                
                    if (!_scanner.mark(subzone, q) && _scanner.should_scan(subzone, q)) {
                        if (_scanner.perform_eager_scanning()) {
                            _scanner.scan(subzone, q);
                        } else {
                            _scanner.push(subzone, q);
                        }
                    }
                }

                // always continue
                return true;
            }
            
            // visitor function for large
            inline bool visit(Zone *zone, Large *large) {
                if (large->refcount() || should_mark(large)) {
                    ReferenceInfo info(kRetainedReference);
                    _scanner._visitor.visit(info, NULL, large);

                    if (!_scanner.mark(large) && _scanner.should_scan(large)) {
                        if (_scanner.perform_eager_scanning()) {
                            _scanner.scan(large);
                        } else {
                            _scanner.push(large);
                        }
                    }
                }

                // always continue
                return true;
            }
            
            // visitor function for registered root
            inline bool operator ()(void **root) {
                ReferenceInfo info(kRootReference);
                _scanner.scan_reference(info, root);
                if (_scanner.prefer_shallow_pending_stack()) _scanner.flush();
                return true;
            }
        };
        
        class rooted_blocks_concurrent_visitor {
            rooted_blocks_visitor &_standard_visitor;
            
        public:
            rooted_blocks_concurrent_visitor(rooted_blocks_visitor &visitor) : _standard_visitor(visitor) {}
            
            const inline bool visit_larges_concurrently()   const { return false; }
            void visit(Zone *zone, Subzone *subzone, usword_t q)  {}
            
            void inline visit(Zone *z, Subzone *sz) {
                Subzone::PendingCountAccumulator accumulator(z->registered_thread());
                usword_t n = sz->allocation_limit();
                for (usword_t q = 0; q < n; q = sz->next_quantum(q)) {
                    if (!sz->is_free(q)) {
                        _standard_visitor.visit(z, sz, q);
                    }
                }
            }
            
            inline void visit(Zone *z, Large *large) {
                Subzone::PendingCountAccumulator accumulator(z->registered_thread());
                _standard_visitor.visit(z, large);
            }
        };

        inline void scan_roots() {
            // visit rooted blocks.
            rooted_blocks_visitor visitor(*this);
            if (scan_threaded()) {
                rooted_blocks_concurrent_visitor concurrent_visitor(visitor);
                visitAllocatedBlocks_concurrent(_zone, concurrent_visitor);
            } else {
                visitAllocatedBlocks(_zone, visitor);
            }
            
            if (prefer_shallow_pending_stack()) flush();

            // visit registered roots.
            Mutex lock(_zone->roots_lock());
            visitRootsNoLock(_zone, visitor);
        }

        // typedef void (&scanner_block_t) (Range *range);
        
        static void scan_one_thread(Thread *thread, void *arg) {
            // Until <rdar://problem/6393321&6182276> are fixed, have to use the function pointer variants.
            ReferenceIterator* scanner = (ReferenceIterator *)arg;
            if (thread->is_current_thread()) {
                thread->scan_current_thread(&ReferenceIterator::scan_thread_range, arg, scanner->_stack_bottom);
            } else {
                thread->scan_other_thread(&ReferenceIterator::scan_thread_range, arg, scan_threads_suspended());
            }
        }

        inline void scan_threads() {
            // TODO:  coordinate with dying threads.
            // Until <rdar://problem/6393321&6182276> are fixed, have to use a function pointer.
            // scanner_block_t block_scanner = ^(Range *range) { this->scan_range(kStackReference, *range); };

            _zone->scan_registered_threads(&ReferenceIterator::scan_one_thread, this);
        }
        
        inline void push_associations(void *block) {
            AssociationsHashMap &associations(_zone->associations());
            AssociationsHashMap::iterator i = associations.find(block);
            if (i != associations.end()) {
                ObjectAssociationMap *refs = i->second;
                for (ObjectAssociationMap::iterator j = refs->begin(), jend = refs->end(); j != jend; j++) {
                    ObjectAssociationMap::value_type &pair = *j;
                    void *key = pair.first;
                    void *value = pair.second;
                    ReferenceInfo info(block, key);
                    if (_zone->in_subzone_memory(value)) {
                        Subzone *subzone = Subzone::subzone(value);
                        usword_t q = subzone->quantum_index(value);
                        _visitor.visit(info, &pair.second, subzone, q);
                        if (!mark(subzone, q)) {
                            push(subzone, q);
                        }
                    } else if (_zone->block_is_start_large(value)) {
                        Large *large = Large::large(value);
                        _visitor.visit(info, &pair.second, large);
                        if (!mark(large)) {
                            push(large);
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
            inline static bool should_mark(Subzone *subzone, usword_t q) { return Configuration::OriginalScanningStrategy::should_mark(subzone, q); }
            inline static bool should_mark(Large *large) { return Configuration::OriginalScanningStrategy::should_mark(large); }
            inline static bool should_scan(Subzone *subzone, usword_t q) { return true; }
            inline static bool should_scan(Large *large) { return true; }
            
            inline static void scan_block(ReferenceIterator &scanner, void *block, usword_t size, usword_t layout, WriteBarrier *wb) {
                // NOTE:  scan_associations() always pushes blocks whether scanned or unscanned.
                if (is_scanned(layout))
                    Configuration::OriginalScanningStrategy::scan_block(scanner, block, size, layout, wb);
                scanner.push_associations(block);
            }
            
            inline static const unsigned char *layout_map_for_block(Zone *zone, void *block) { return Configuration::OriginalScanningStrategy::layout_map_for_block(zone, block); }
            inline static bool visit_interior_pointers() { return Configuration::OriginalScanningStrategy::visit_interior_pointers(); }
            
            inline static void scan_object(ReferenceIterator &scanner, void *object, usword_t size, usword_t layout, const unsigned char* map, WriteBarrier *wb) {
                // NOTE:  scan_associations() always pushes blocks whether scanned or unscanned.
                if (is_scanned(layout))
                    Configuration::OriginalScanningStrategy::scan_object(scanner, object, size, layout, map, wb);
                scanner.push_associations(object);
            }
        };
        
        inline bool associations_should_be_scanned(void *block) {
            if (_zone->in_subzone_memory(block)) {
                Subzone *subzone = Subzone::subzone(block);
                usword_t q;
                return subzone->block_is_start(block, &q) && is_marked(subzone, q);
            } else if (_zone->block_is_start_large(block)) {
                return is_marked(Large::large(block));
            }
            // <rdar://problem/6463922> Treat non-block pointers as unconditionally live.
            return true;
        }

        inline void scan_associations() {
            typename AssociationScanningConfiguration::PendingStack pendingStack;
            AssociationReferenceIterator associationScanner(_zone, _visitor, pendingStack, _stack_bottom, false, _repair_write_barrier);
            
            // Prevent other threads from breaking existing associations. We already own the enlivening lock.
            ReadLock lock(associations_lock(_zone));
            AssociationsHashMap &associations(_zone->associations());
        
            // consider associative references. these are only reachable if their primary block is.
            for (AssociationsHashMap::iterator i = associations.begin(), iend = associations.end(); i != iend; i++) {
                void *block = i->first;
                if (associations_should_be_scanned(block)) {
                    ObjectAssociationMap *refs = i->second;
                    for (ObjectAssociationMap::iterator j = refs->begin(), jend = refs->end(); j != jend; j++) {
                        ObjectAssociationMap::value_type &pair = *j;
                        void *key = pair.first;
                        void *value = pair.second;
                        ReferenceInfo info(block, key);
                        if (_zone->in_subzone_memory(value)) {
                            Subzone *subzone = Subzone::subzone(value);
                            usword_t q = subzone->quantum_index(value);
                            _visitor.visit(info, &pair.second, subzone, q);
                            if (!associationScanner.mark(subzone, q)) {
                                pendingStack.push(subzone, q);
                            }
                        } else if (_zone->block_is_start_large(value)) {
                            Large *large = Large::large(value);
                            _visitor.visit(info, &pair.second, large);
                            if (!associationScanner.mark(large)) {
                                pendingStack.push(large);
                            }
                        }
                    }
                    if (prefer_shallow_pending_stack()) 
                        pendingStack.scan(associationScanner);
                }
            }
            if (!prefer_shallow_pending_stack()) 
                pendingStack.scan(associationScanner);
        }
        
        // pending_visitor is used to scan all blocks that have their pending bit set
        // (for pending stack implementations that do not do this)
        class pending_visitor {
            PendingStack _stack;
            ReferenceIterator _scanner;
        public:
            pending_visitor(ReferenceIterator* scanner)
            : _scanner(scanner->_zone, scanner->_visitor, _stack, scanner->_stack_bottom,
                       false, scanner->_repair_write_barrier)
            {
            }
            
            pending_visitor(const pending_visitor &visitor)
            : _scanner(visitor._scanner._zone, visitor._scanner._visitor, _stack, visitor._scanner._stack_bottom,
                       false, visitor._scanner->_repair_write_barrier)
            {
            }
            
            void operator() (Subzone *subzone, usword_t q) {
                _scanner.scan(subzone, q);
                _scanner.flush();
            }
            
            void operator() (Large *large) {
                _scanner.scan(large);
                _scanner.flush();
            }
        };
        
        inline void scan() {
            scan_roots();
            
            // scan all pending blocks before we set the enlivening barrier to
            // reduce the amount of time threads are blocked in write barriers
            flush();
            
            if (_needs_enlivening_barrier) {
                _zone->enlivening_barrier();
            }
            
            if (scan_pending()) {
                pending_visitor visitor(this);
                visitPendingBlocks(_zone, visitor);
            }
            
            scan_threads();
            
            // flush again because scanning with the associations scanner is more expensive
            flush();
            
            scan_associations();
        }
    };
    
    // Predefined scanning strategies.

    template <typename ReferenceIterator> class FullScanningStrategy {
    public:
        // provide a way to rebind this template type to another just like STL allocators can do.
        template <typename U> struct rebind { typedef FullScanningStrategy<U> other; };

        inline static bool should_mark(Subzone *subzone, usword_t q) { return false; }
        inline static bool should_mark(Large *large) { return false; };

        inline static bool should_scan(Subzone *subzone, usword_t q) {
            return subzone->is_scanned(q);
        }
        
        inline static bool should_scan(Large *large) {
            return large->is_scanned();
        }
        
        // non-object block scan.
        inline static void scan_block(ReferenceIterator &scanner, void *block, usword_t size, usword_t layout, WriteBarrier *wb) {
            Range range(block, size);
            if (layout == AUTO_POINTERS_ONLY) {
                ReferenceInfo info(kAllPointersHeapReference, wb);
                scanner.scan_range(info, range);
            } else {
                ReferenceInfo info(kConservativeHeapReference, wb);
                if (scanner.visit_interior_pointers()) {
                    // check interior pointers to handle CoreText nastiness.
                    ReferenceInfo indirect_info(kConservativeHeapIndirectReference, wb);
                    scanner.scan_range_indirect(info, indirect_info, range);
                } else {
                    scanner.scan_range(info, range);
                }
            }
        }

        inline static const unsigned char *layout_map_for_block(Zone *zone, void *block) { return zone->layout_map_for_block(block); }
        inline static bool visit_interior_pointers() { return false; }
        inline static bool scan_threads_suspended() { return true; }
        inline static pthread_rwlock_t *associations_lock(Zone *zone) { return zone->associations_lock(); }
        
        // exact object scan.
        inline static void scan_object(ReferenceIterator &scanner, void *object, usword_t size, usword_t layout, const unsigned char* map, WriteBarrier *wb) __attribute__((always_inline)) {
            if (map == NULL) {
                // gcc optimization: all ivars are objects, no map provided.  All additional space treated conservatively.  This isn't good for compaction since
                // we can't cheaply know the limit of where we could relocate and so all the memory is treated conservatively. clang issues a complete map
                // (see <rdar://problem/7159643>). Additionally, if AUTO_POINTERS_ONLY is marked, space beyond the map is treated as relocatable pointers.
                Range range(object, size);
                ReferenceInfo info(layout & AUTO_POINTERS_ONLY ? kAllPointersHeapReference : kConservativeHeapReference, wb);
                if (scanner.visit_interior_pointers()) {
                    ReferenceInfo indirect_info(kConservativeHeapIndirectReference, wb);
                    scanner.scan_range_indirect(info, indirect_info, range);
                } else
                    scanner.scan_range(info, range);
                return;
            }
            ReferenceInfo exactInfo(kExactHeapReference, wb);
            void **slots = (void **)object;
            void **last = (void **)displace(slots, size - sizeof(void*));
            AUTO_PROBE(auto_probe_scan_with_layout(slots, last, map));
            // while not '\0' terminator
            while (unsigned data = *map++) {
                // extract the skip and run
                unsigned skip = data >> 4;
                unsigned run = data & 0xf;
                
                // advance the reference by the skip
                slots += skip;
                
                while (run--) scanner.scan_reference(exactInfo, slots++);
            }

            // since objects can be allocated with extra data at end, scan the remainder. If the AUTO_POINTERS_ONLY bit is
            // turned on in the layout, scan the remainder exactly, otherwise scan conservatively.
            ReferenceInfo remainderInfo((layout & AUTO_POINTERS_ONLY) ? kAllPointersHeapReference : kConservativeHeapReference, wb);
            if (scanner.visit_interior_pointers()) {
                // CFStorage objects keep indirect pointers to themselves, and thus should be pinned.
                ReferenceInfo indirect_info(kConservativeHeapIndirectReference, wb);
                while (slots <= last) scanner.scan_reference_indirect(remainderInfo, indirect_info, slots++);
            } else {
                while (slots <= last) scanner.scan_reference(remainderInfo, slots++);
            }
        }
    };
    
    template <typename ReferenceIterator> class GenerationalScanningStrategy {
    public:
        // provide a way to rebind this template type to another just like STL allocators can do.
        template <typename U> struct rebind { typedef GenerationalScanningStrategy<U> other; };

        // always mark old objects, even if they aren't scanned.
        inline static bool should_mark(Subzone *subzone, usword_t q) { return !subzone->is_new(q); }
        inline static bool should_mark(Large *large) { return !large->is_new(); };
        
        inline static bool should_scan(Subzone *subzone, usword_t q) {
            // only scan a block, if it has ever been written through its write-barrier.
            return subzone->is_scanned(q) && subzone->write_barrier().range_has_marked_cards(subzone->quantum_address(q), subzone->size(q));
        }
        
        inline static bool should_scan(Large *large) {
            return large->is_scanned() && large->write_barrier().range_has_marked_cards(large->address(), large->size());
        }
        
        inline static void scan_block(ReferenceIterator &scanner, void *block, usword_t size, usword_t layout, WriteBarrier *wb) {
            wb->scan_marked_ranges(block, size, layout == AUTO_POINTERS_ONLY ? &ReferenceIterator::scan_all_pointers_range : &ReferenceIterator::scan_conservative_range, &scanner);
        }

        inline static const unsigned char *layout_map_for_block(Zone *zone, void *block) { return zone->layout_map_for_block(block); }
        inline static bool visit_interior_pointers() { return false; }
        inline static bool scan_threads_suspended() { return true; }
        inline static pthread_rwlock_t *associations_lock(Zone *zone) { return zone->associations_lock(); }

        inline static void scan_object(ReferenceIterator &scanner, void *object, usword_t size, usword_t layout, const unsigned char* map, WriteBarrier *wb) {
            if (map == NULL) {
                // gcc optimization: all ivars are objects, no map provided.  All additional space treated conservatively.  This isn't good for compaction since
                // we can't cheaply know the limit of where we could relocate and so all the memory is treated conservatively. clang issues a complete map
                // (see <rdar://problem/7159643>). Additionally, if AUTO_POINTERS_ONLY is marked, space beyond the map is treated as relocatable pointers.
                wb->scan_marked_ranges(object, size, (layout & AUTO_POINTERS_ONLY) ? &ReferenceIterator::scan_all_pointers_range : &ReferenceIterator::scan_conservative_range, &scanner);
                return;
            }
            void **slots = (void **)object;
            void **end = (void **)displace(slots, size);
            AUTO_PROBE(auto_probe_scan_with_layout(slots, end, map));
            // while not '\0' terminator
            while (unsigned data = *map++) {
                // extract the skip and run
                unsigned skip = data >> 4;
                unsigned run = data & 0xf;
                
                // advance the reference by the skip
                slots += skip;
                
                if (run) {
                    // <rdar://problem/6516045>:  make sure we only scan valid ranges.
                    if (slots < end && (slots + run) <= end) {
                        wb->scan_marked_ranges(slots, run * sizeof(void*), &ReferenceIterator::scan_exact_range, &scanner);
                    } else {
                        break;
                    }
                }
                
                slots += run;
            }

            // since objects can be allocated with extra data at end, scan the remainder. If the AUTO_POINTERS_ONLY bit is
            // turned on in the layout, scan the remainder exactly, otherwise scan conservatively.
            if (slots < end) {
                wb->scan_marked_ranges(slots, (end - slots) * sizeof(void*),
                                       (layout & AUTO_POINTERS_ONLY) ?
                                       &ReferenceIterator::scan_exact_range :
                                       &ReferenceIterator::scan_conservative_range, &scanner);
            }
        }
    };
    
};

#endif // __AUTO_REFERENCE_ITERATOR__
