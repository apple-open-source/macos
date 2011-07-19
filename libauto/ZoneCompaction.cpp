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
    ZoneCompaction.cpp
    Mostly Copying Compaction Algorithms
    Copyright (c) 2010-2001 Apple Inc. All rights reserved.
 */

#include "ReferenceIterator.h"
#include "Zone.h"

extern "C" {
void *CompactionWatchPoint = NULL;
char CompactionObserverKey;
}

// this controls whether or not non-object backing stores containing weak references should pin. seems to be a requirement for now.
#define NON_OBJECT_WEAK_REFERENCES_SHOULD_PIN 1

namespace Auto {
    static malloc_zone_t *_compaction_zone = NULL;
    static malloc_logger_t *_old_malloc_logger = NULL;
    
    static void _disabled_malloc_logger(uint32_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t) {}
    
    class CompactionZone {
    public:
        CompactionZone() {
            _compaction_zone = malloc_create_zone(4096, 0);
            malloc_set_zone_name(_compaction_zone, "compaction_zone");
            // malloc_zone_unregister(_compaction_zone);
            if (malloc_logger) {
                _old_malloc_logger = malloc_logger;
                malloc_logger = _disabled_malloc_logger;
            }
        }
        
       ~CompactionZone() {
            if (_old_malloc_logger) {
                malloc_logger = _old_malloc_logger;
                _old_malloc_logger = NULL;
            }
            malloc_destroy_zone(_compaction_zone);
            _compaction_zone = NULL;
        }
    };

    template <typename T> struct CompactionZoneAllocator {
        typedef T                 value_type;
        typedef value_type*       pointer;
        typedef const value_type *const_pointer;
        typedef value_type&       reference;
        typedef const value_type& const_reference;
        typedef size_t            size_type;
        typedef ptrdiff_t         difference_type;

        template <typename U> struct rebind { typedef CompactionZoneAllocator<U> other; };

        template <typename U> CompactionZoneAllocator(const CompactionZoneAllocator<U>&) {}
        CompactionZoneAllocator() {}
        CompactionZoneAllocator(const CompactionZoneAllocator&) {}
        ~CompactionZoneAllocator() {}

        pointer address(reference x) const { return &x; }
        const_pointer address(const_reference x) const { return x; }

        pointer allocate(size_type n, const_pointer = 0) {
            return static_cast<pointer>(::malloc_zone_calloc(_compaction_zone, n, sizeof(T)));
        }

        void deallocate(pointer p, size_type) { ::malloc_zone_free(_compaction_zone, p); }

        size_type max_size() const { 
            return static_cast<size_type>(-1) / sizeof(T);
        }

        void construct(pointer p, const value_type& x) { 
            new(p) value_type(x); 
        }

        void destroy(pointer p) { p->~value_type(); }

        void operator=(const CompactionZoneAllocator&);
    };


    template<> struct CompactionZoneAllocator<void> {
        typedef void        value_type;
        typedef void*       pointer;
        typedef const void *const_pointer;

        template <typename U> struct rebind { typedef CompactionZoneAllocator<U> other; };
    };


    template <typename T>
    inline bool operator==(const CompactionZoneAllocator<T>&, 
                           const CompactionZoneAllocator<T>&) {
        return true;
    }

    template <typename T>
    inline bool operator!=(const CompactionZoneAllocator<T>&, 
                           const CompactionZoneAllocator<T>&) {
        return false;
    }

    template <typename ReferenceIterator> class ZonePendingStack {
        typedef std::vector<uintptr_t, CompactionZoneAllocator<uintptr_t> > uintptr_vector;
        uintptr_vector _small_stack, _large_stack;
    public:
        void push(Subzone *subzone, usword_t q) {
            _small_stack.push_back(subzone->pack(q));
        }
        
        void push(Large *large) {
            _large_stack.push_back(uintptr_t(large));
        }
        
        static bool mark(Subzone *subzone, usword_t q) { return subzone->test_and_set_mark(q); }
        static bool mark(Large *large) { return large->test_and_set_mark(); }
        
        static bool is_marked(Subzone *subzone, usword_t q) { return subzone->is_marked(q); }
        static bool is_marked(Large *large) { return large->is_marked(); }
        
        void scan(ReferenceIterator &scanner) {
            for (;;) {
                // prefer scanning small blocks to large blocks, to keep the stacks shallow.
                if (_small_stack.size()) {
                    uintptr_t back = _small_stack.back();
                    _small_stack.pop_back();
                    usword_t q;
                    Subzone *subzone = Subzone::unpack(back, q);
                    scanner.scan(subzone, q);
                } else if (_large_stack.size()) {
                    Large *large = reinterpret_cast<Large*>(_large_stack.back());
                    _large_stack.pop_back();
                    scanner.scan(large);
                } else {
                    return;
                }
            }
        }
        
        const PendingStackHint hints() { return PendingStackWantsEagerScanning; }
        template <typename U> struct rebind { typedef ZonePendingStack<U> other; };
    };

    // Used by fixup_phase below, which needs no actual pending stack.
    template <typename ReferenceIterator> class EmptyPendingStack {
    public:
        void push(Subzone *subzone, usword_t q) {}
        void push(Large *large) {}
        
        static bool mark(Subzone *subzone, usword_t q) { return false; }
        static bool mark(Large *large) { return false; }
        static bool is_marked(Subzone *subzone, usword_t q) { return false; }
        static bool is_marked(Large *large) { return false; }
        
        void scan(ReferenceIterator &scanner) {}
        
        template <typename U> struct rebind { typedef EmptyPendingStack<U> other; };
    };

    
    typedef void (^mark_pinned_t) (void **slot, Subzone *subzone, usword_t q, ReferenceKind kind);

    class CompactionClassifier {
        size_t _kindCounts[kReferenceKindCount];
        Zone *_zone;
        mark_pinned_t _marker;
        struct Configuration;
        typedef ReferenceIterator<Configuration> CompactingReferenceIterator;
        class CompactionScanningStrategy : public FullScanningStrategy<CompactingReferenceIterator> {
        public:
            inline static bool visit_interior_pointers() { return true; }
            inline static bool scan_threads_suspended() { return false; }
            inline static pthread_rwlock_t *associations_lock(Zone *zone) { return NULL; }  // already owned, non-reentrant.
        };
        struct Configuration {
            typedef CompactionClassifier ReferenceVisitor;
            typedef ZonePendingStack<CompactingReferenceIterator> PendingStack;
            typedef CompactionScanningStrategy ScanningStrategy;
        };
    public:
        CompactionClassifier(Zone *zone, mark_pinned_t marker) : _zone(zone), _marker(marker) { bzero(_kindCounts, sizeof(_kindCounts)); }
        
        inline void mark_pinned(void **slot, Subzone *subzone, usword_t q, ReferenceKind kind) { _marker(slot, subzone, q, kind); }
        
        bool is_weak_slot_ivar(void *slot, void *slot_base, usword_t slot_layout) {
            if (slot_layout & AUTO_OBJECT) {
                const unsigned char *weak_layout = _zone->weak_layout_map_for_block(slot_base);
                if (weak_layout) {
                    void **slots = (void **)slot_base;
                    while (unsigned char byte = *weak_layout++) {
                        uint8_t skips = (byte >> 4);
                        slots += skips; 
                        uint8_t weaks = (byte & 0x0F);
                        while (weaks--) {
                            if (slot == (void *)slots++)
                                return true;
                        }
                    }
                }
            }
            return false;
        }
        
        void visit(const ReferenceInfo &info, void **slot, Subzone *subzone, usword_t q) {
            if (subzone->quantum_address(q) == CompactionWatchPoint) {
                printf("visiting a reference to CompactionWatchPoint = %p\n", CompactionWatchPoint);
            }
            ReferenceKind kind = info.kind();
            _kindCounts[kind]++;
            switch (kind) {
            case kAllPointersHeapReference:
            case kAssociativeReference:
            case kExactHeapReference:
            case kRootReference:
                break;
            case kWeakReference:
                {
                    __block void *slot_block = NULL;
                    __block usword_t slot_layout = 0;
                    blockStartNoLockDo(_zone, slot, ^(Subzone *slot_subzone, usword_t slot_q) {
                        slot_block = slot_subzone->quantum_address(slot_q);
                        slot_layout = slot_subzone->layout(slot_q);
                        // don't pin weakly referenced objects that come from objects with layout, or unscanned memory marked as AUTO_MEMORY_ALL_WEAK_POINTERS.
                        if (!is_weak_slot_ivar(slot, slot_block, slot_layout)) {
                            // since there's no layout describing this object containing a weak reference
                            // pin slot_block. this saves having to call weak_transfer_weak_contents_unscanned() for
                            // every unscanned block.
                            // allow weak references from this special layout backing store to be moved. This layout gives
                            // permission to move these pointers. It will never be used for hashed backing stores, unless the
                            // hash functions are indepedent of object pointer values.
                            if (slot_layout != AUTO_MEMORY_ALL_WEAK_POINTERS) {
                                mark_pinned(NULL, slot_subzone, slot_q, kWeakSlotReference);
                                mark_pinned(slot, subzone, q, kWeakReference);
                            } else if (slot == slot_block) {
                                // first word contains a weak reference, so pin the slot's block to avoid problems with forwarding.
                                mark_pinned(NULL, slot_subzone, slot_q, kWeakSlotReference);
                            }
                        }
                    }, ^(Large *slot_large) {
                        slot_block = slot_large->address();
                        slot_layout = slot_large->layout();
                        if (!is_weak_slot_ivar(slot, slot_block, slot_layout)) {
                            // large blocks are never compacted.
                            if (slot_layout != AUTO_MEMORY_ALL_WEAK_POINTERS)
                                mark_pinned(slot, subzone, q, kWeakReference);
                        }
                    });
                    if (!slot_block) {
                        /* can safely compact blocks referenced weakly from outside the gc zone? */
                        if (!_zone->is_global_address_nolock(slot)) {
                            // we can relocate GLOBAL weak references (and malloc blocks for that matter).
                            mark_pinned(slot, subzone, q, kWeakReference);
                        }
                    }
                }
                break;
            default:
                mark_pinned(slot, subzone, q, kind);
                break;
            }
        }
        
        void visit(const ReferenceInfo &info, void **slot, Large *large) {
            if (info.kind() == kWeakReference) {
                // weakly referenced Large block. Pin slot's block if not from a __weak ivar.
                if (_zone->in_subzone_memory(slot)) {
                    usword_t slot_q;
                    Subzone *slot_subzone = Subzone::subzone(slot);
                    void *slot_block = slot_subzone->block_start(slot, slot_q);
                    if (slot_block) {
                        usword_t slot_layout = slot_subzone->layout(slot_q);
                        // don't pin weakly referenced objects that come from objects with layout, or unscanned memory marked as ALL_POINTERS.
                        if (!is_weak_slot_ivar(slot, slot_block, slot_layout)) {
                            // since there's no layout describing this object containing a weak reference
                            // pin slot_block. this saves having to call weak_transfer_weak_contents_unscanned() for
                            // every unscanned block.
                            if (slot_layout != AUTO_MEMORY_ALL_WEAK_POINTERS || slot == slot_block) {
                                mark_pinned(NULL, slot_subzone, slot_q, kWeakSlotReference);
                            }
                        }
                    }
                }
            }
        }

        void visit_weak_callback(auto_weak_callback_block_t *callback) {
            // check the callback slot. if it's inside a subzone block, pin that object (for now).
            if (_zone->in_subzone_memory(callback)) {
                usword_t q;
                Subzone *subzone = Subzone::subzone(callback);
                void *callback_start = subzone->block_start(callback, q);
                if (callback_start) {
                    if (!subzone->is_pinned(q)) {
                        // NOTE:  this will pin any object that contains an embedded auto_weak_callback_block_t.
                        mark_pinned(NULL, subzone, q, kWeakReference);
                    }
                }
            }
        }
        
        void visit_weak_slot(void **slot) {
            // weak reference to SOMETHING that is neither a Subzone nor Large block.
            // pin the owning block if it's neither a __weak ivar or has layout of type AUTO_MEMORY_ALL_WEAK_POINTERS.
            // e.g. [NSNull null] or a constant NSString is used as a key in a map table.
            if (_zone->in_subzone_memory(slot)) {
                usword_t slot_q;
                Subzone *slot_subzone = Subzone::subzone(slot);
                void *slot_block = slot_subzone->block_start(slot, slot_q);
                if (slot_block) {
                    usword_t slot_layout = slot_subzone->layout(slot_q);
                    if (!is_weak_slot_ivar(slot, slot_block, slot_layout)) {
                        if (slot_layout != AUTO_MEMORY_ALL_WEAK_POINTERS || slot == slot_block)
                            mark_pinned(NULL, slot_subzone, slot_q, kWeakSlotReference);
                    }
                }
            }
        }
        
        void classify_weak_reference(weak_referrer_t &ref) {
            void **slot = ref.referrer;
            blockDo(_zone, (void*)*slot,
                    ^(Subzone *subzone, usword_t q) { visit(kWeakReference, slot, subzone, q); },
                    ^(Large *large) { visit(kWeakReference, slot, large); },
                    ^(void *block) { visit_weak_slot(slot); });
            if (uintptr_t(ref.block) & 1) {
                // pin old-school callbacks only.
                visit_weak_callback((auto_weak_callback_block_t *)displace(ref.block, -1));
            }
        }

        //
        // scan_garbage() - Last pass through the heap, scan the otherwise unreachable blocks, classifying all
        // pinning references to live objects. Could just pin all unreachable blocks here, to avoid waisting cycles
        // on moving soon to be collected objects. However, moving them out of the way may still be worth the effort
        // if it allows the heap to shrink.
        //
        void scan_garbage(CompactingReferenceIterator &scanner) {
            // iterate through the regions first
            for (Region *region = _zone->region_list(); region != NULL; region = region->next()) {
                // iterate through the subzones
                SubzoneRangeIterator iterator(region->subzone_range());
                while (Subzone *subzone = iterator.next()) {
                    // get the number of quantum in the subzone
                    usword_t n = subzone->allocation_count();
                    for (usword_t q = 0; q < n; q = subzone->next_quantum(q)) {
                        if (!subzone->is_free(q) && !subzone->test_and_set_mark(q)) {
                            // pin AUTO_OBJECT_UNSCANNED garbage to avoid finalize crashes (e.g. _ripdata_finalize())
                            // caused by the corruption of interior pointers in unscanned memory. this population should be fairly limited.
                            // FIXME:  remove this heuristic in NMOS.
                            usword_t layout = subzone->layout(q);
                            if (layout & AUTO_OBJECT_UNSCANNED) {
                                mark_pinned(NULL, subzone, q, kGarbageReference);
                            } else if (::is_scanned(layout)) {
                                scanner.scan(subzone, q);
                                scanner.flush();
                            }
                        }
                    }
                }
            }

            // iterate through the large blocks
            for (Large *large = _zone->large_list(); large != NULL; large = large->next()) {
                if (!large->test_and_set_mark() && large->is_scanned()) {
                    scanner.scan(large);
                    scanner.flush();
                }
            }
        }

        void scan(void *stack_bottom) {
            Configuration::PendingStack stack;
            CompactingReferenceIterator scanner(_zone, *this, stack, stack_bottom, false, false);
            scanner.scan();
#if NON_OBJECT_WEAK_REFERENCES_SHOULD_PIN
            weak_enumerate_table_fixup(_zone, ^(weak_referrer_t &ref) {
                classify_weak_reference(ref);
            });
#endif
            // now, scan and pin all unmarked (garbage) blocks.
            scan_garbage(scanner);
        }
        
        void printKindCounts() {
            for (int i = 0; i < kReferenceKindCount; ++i) {
                printf("<%s> : %lu\n", ReferenceInfo::name(ReferenceKind(i)), _kindCounts[i]);
            }
        }
    };
    
    struct page_statistics_visitor {
        size_t _pageIndex, _prevUnpinnedPageIndex;
        size_t _blocksPerPage, _pinnedBlocksPerPage;
        size_t _pinnedPages, _unpinnedPages;
        size_t _pinnedBlocks, _unpinnedBlocks;
        size_t _pinnedBytes, _unpinnedBytes;
        
        page_statistics_visitor()
            : _pageIndex(0), _prevUnpinnedPageIndex(0), _blocksPerPage(0), _pinnedBlocksPerPage(0),
              _pinnedPages(0), _unpinnedPages(0), _pinnedBlocks(0), _unpinnedBlocks(0),
              _pinnedBytes(0), _unpinnedBytes(0)
        {
        }
    
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            size_t page = (uintptr_t(subzone->quantum_address(q)) >> page_size_log2);
            if (page != _pageIndex) {
                if (_pageIndex) {
                    if (_pinnedBlocksPerPage == 0) {
                        // printf("page[%lu] = %lu blocks {%lu} %c\n", _pageIndex, _blocksPerPage, subzone->quantum_size(1), (_pageIndex == (_prevUnpinnedPageIndex + 1)) ? '#' : ' ');
                        _prevUnpinnedPageIndex = _pageIndex;
                        ++_unpinnedPages;
                    } else {
                        ++_pinnedPages;
                        _pinnedBlocksPerPage = 0;
                    }
                }
                _pageIndex = page;
                _blocksPerPage = 0;
            }
            ++_blocksPerPage;
            if (subzone->is_pinned(q)) {
                ++_pinnedBlocksPerPage;
                ++_pinnedBlocks;
                _pinnedBytes += subzone->size(q);
            } else {
                ++_unpinnedBlocks;
                _unpinnedBytes += subzone->size(q);
            }
            return true;
        }
        inline bool visit(Zone *zone, Large *large) { return false; }
    };
    
    static void examine_heap_fragmentation(Zone *zone) {
        // for fun, can we make the heap stable?
        zone->suspend_all_registered_threads();

        struct quantum_counts {
            size_t subzones;
            size_t total;
            size_t holes;
            size_t allocs;
            size_t unscanned_allocs;
            size_t unused;
        };
        quantum_counts small_counts = { 0 }, medium_counts = { 0 };
        
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            // iterate through the subzones
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.next()) {
                quantum_counts &counts = subzone->is_small() ? small_counts : medium_counts;
                bool unscanned = (subzone->admin()->layout() & AUTO_UNSCANNED);
                ++counts.subzones;
                usword_t q = 0, n = subzone->allocation_count();    // only examine quanta that have been handed out.
                while (q < n) {
                    usword_t nq = subzone->next_quantum(q);
                    usword_t count = nq - q;
                    if (subzone->is_free(q))
                        counts.holes += count;
                    else {
                        counts.allocs += count;
                        if (unscanned) counts.unscanned_allocs += count;
                    }
                    q = nq;
                }
                counts.unused += (subzone->allocation_limit() - n);
                counts.total += n;
            }
        }
        
        size_t largeSize = 0;
        for (Large *large = zone->large_list(); large != NULL; large = large->next()) {
            largeSize += large->size();
        }
        
        printf("largeSize = %ld\n", largeSize);
        printf("subzones = { %lu, %lu }\n", small_counts.subzones, medium_counts.subzones);
        printf("q total  = { %lu, %lu }\n", small_counts.total, medium_counts.total);
        printf("q allocs = { %lu, %lu }\n", small_counts.allocs, medium_counts.allocs);
        printf("q uallocs= { %lu, %lu }\n", small_counts.unscanned_allocs, medium_counts.unscanned_allocs);
        printf("q holes  = { %lu, %lu }\n", small_counts.holes, medium_counts.holes);
        printf("q unused = { %lu, %lu }\n", small_counts.unused, medium_counts.unused);

        zone->resume_all_registered_threads();
    }
    
    typedef struct { size_t counts[kReferenceKindCount]; } KindCounts;
    typedef std::map<void *, KindCounts, AuxPointerLess, CompactionZoneAllocator<std::pair<void * const, KindCounts> > > PtrKindCountsMap;
    typedef __gnu_cxx::hash_set<void *, AuxPointerHash, AuxPointerEqual, CompactionZoneAllocator<void *> > PtrSet;    

    static void printPinnedReference(FILE *out, void *address, KindCounts &kind) {
        fprintf(out, "%p", address);
        for (int i = 1; i < kReferenceKindCount; ++i) {
            if (kind.counts[i]) fprintf(out, " <%s>[%lu]", ReferenceInfo::name(ReferenceKind(i)), kind.counts[i]);
        }
        fprintf(out, "\n");
    }
    
    static void printPinnedReference(FILE *out, Zone *zone, void *slot_base, void **slot, void *address, const char *name, ReferenceKind kind) {
        if (slot_base) {
            if (zone->block_layout(slot_base) & AUTO_OBJECT)
                fprintf(out, "%p + %ld (%s) -> %p (%s) <%s>\n", slot_base, (long)((char*)slot - (char*)slot_base), zone->name_for_object(slot_base), address, name, ReferenceInfo::name(kind));
            else
                fprintf(out, "%p + %lu -> %p (%s) <%s>\n", slot_base, (long)((char*)slot - (char*)slot_base), address, name, ReferenceInfo::name(kind));
        } else {
            switch (kind) {
            case kRetainedReference:
            case kWeakSlotReference:
            case kGarbageReference:
                fprintf(out, "%p -> %p (%s) <%s>\n", (void*)NULL, address, name, ReferenceInfo::name(kind));    // hack for Pinpoint analysis.
                break;
            default:
                fprintf(out, "%p -> %p (%s) <%s>\n", slot, address, name, ReferenceInfo::name(kind));
            }
        }
    }

    void Zone::analyze_heap(const char *path) {
        CompactionZone compactionZone;
        
        if (true) examine_heap_fragmentation(this);
        __block struct { char buffer[36]; } name;
        FILE *out = fopen(path, "w");
        PtrKindCountsMap pinMap;
        __block PtrKindCountsMap &pinMapRef = pinMap;
        mark_pinned_t marker = ^(void **slot, Subzone *subzone, usword_t q, ReferenceKind kind) {
            subzone->mark_pinned(q);
            pinMapRef[subzone->quantum_address(q)].counts[kind]++;
            printPinnedReference(out, this, block_start(slot), slot, subzone->quantum_address(q),
                                 (subzone->layout(q) & AUTO_OBJECT) ?
                                 name_for_object(subzone->quantum_address(q)) :
                                 (snprintf(name.buffer, sizeof(name.buffer), "%lu bytes", subzone->size(q)), name.buffer),
                                 kind);
        };
        
        // grab all necessary locks to get a coherent picture of the heap.
        Mutex marksLock(&_mark_bits_mutex);
        ReadLock assocLock(&_associations_lock);
        SpinLock weakLock(&weak_refs_table_lock);
        
        reset_all_pinned();
        suspend_all_registered_threads();

        // compute the pinned object set.
        CompactionClassifier classifier(this, marker);
        classifier.scan((void *)auto_get_sp());
        
        page_statistics_visitor visitor;
        visitAllocatedBlocks(this, visitor);
        printf("unpinned { pages = %lu, blocks = %lu, bytes = %lu }\n", visitor._unpinnedPages, visitor._unpinnedBlocks, visitor._unpinnedBytes);
        printf("  pinned { pages = %lu, blocks = %lu, bytes = %lu }\n", visitor._pinnedPages, visitor._pinnedBlocks, visitor._pinnedBytes);
        
        reset_all_marks();
        resume_all_registered_threads();

        if (false) {
            classifier.printKindCounts();
            // dump the pinned object map.
            std::for_each(pinMap.begin(), pinMap.end(), ^(PtrKindCountsMap::value_type &pair) {
                printPinnedReference(out, pair.first, pair.second);
            });
        }

        fclose(out);
    }

    template <class Visitor> void visitAllocatedSubzoneBlocksInReverse(Zone *zone, Visitor& visitor) {
        // iterate through the regions first
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            // iterate through the subzones (in reverse)
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.previous()) {
                // enumerate the allocated blocks in reverse.
                usword_t q = subzone->allocation_count();
                while ((q = subzone->previous_quantum(q)) != not_found) {
                    // skip free blocks, and unmarked blocks. unmarked blocks are on their way
                    // to becoming garbage, and won't have been classified, since they weren't visited.
                    if (!subzone->is_free(q)) {
                        visitor(zone, subzone, q);
                    }
                }
            }
        }
    }
    
    template <class Visitor> void visitAllocatedBlocksForCompaction(Zone *zone, Visitor& visitor) {
        // iterate through the regions first
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            // iterate through the subzones
            SubzoneRangeIterator iterator(region->subzone_range());
            while (Subzone *subzone = iterator.next()) {
                // get the number of quantum in the subzone
                usword_t n = subzone->allocation_count();
                for (usword_t q = 0; q < n; q = subzone->next_quantum(q)) {
                    if (!subzone->is_free(q)) {
                        visitor(zone, subzone, q);
                    }
                }
            }
        }

        // iterate through the large blocks
        for (Large *large = zone->large_list(); large != NULL; large = large->next()) {
            // let the visitor visit the write barrier
            visitor(zone, large);
        }
    }
    
    #pragma mark class forwarding_phase
    
    struct forwarding_phase {
        size_t _objectsMoved, _bytesMoved;
        size_t _pagesCompacted;
        void *_currentPage;
        bool _currentPagePinned;
        bool _pinnedPagesOnly;
        
        forwarding_phase() : _objectsMoved(0), _bytesMoved(0), _pagesCompacted(0), _currentPage(NULL), _currentPagePinned(false), _pinnedPagesOnly(false) {}
        
        void setPinnedPagesOnly(bool pinnedPagesOnly) { _pinnedPagesOnly = pinnedPagesOnly; }
        
        bool is_page_pinned(Subzone *subzone, void *block_page) {
            // see if this page is the first page of a subzone. if so, it contains write-barrier cards which are pinned.
            Range page_range(block_page, page_size);
            void *q_zero_address = subzone->quantum_address(0);
            if (page_range.in_range(q_zero_address) && q_zero_address > block_page) return true;
            // see if this page is pinned by scanning forward.
            usword_t q_page = subzone->quantum_index(block_page);
            usword_t q_prev = subzone->previous_quantum(q_page);
            if (q_prev != not_found && !subzone->is_free(q_prev)) {
                // see if the previous quantum is pinned and overlaps the start of this page.
                Range r(subzone->quantum_address(q_prev), subzone->quantum_size(q_prev));
                if (r.in_range(block_page) && subzone->is_pinned(q_prev)) return true;
            }
            // otherwise, check all of the blocks that span this page for pinnedness.
            usword_t n = subzone->allocation_limit();
            usword_t q_start = q_prev != not_found ? subzone->next_quantum(q_prev) : q_page;
            for (usword_t q = q_start; q < n && page_range.in_range(subzone->quantum_address(q)); q = subzone->next_quantum(q)) {
                if (subzone->is_start(q) && !subzone->is_free(q) && subzone->is_pinned(q)) return true;
            }
            return false;
        }
        
        inline void forward_block(Zone *zone, Subzone *subzone, usword_t q, void *block) {
            if (subzone->layout(q) & AUTO_OBJECT) {
                // for now, don't compact objects without layout maps. eventually, there shouldn't be any of these.
                if (zone->layout_map_for_block(block) == NULL) return;
            }
            void *newBlock = zone->forward_block(subzone, q, block);
            if (newBlock != block) {
                ++_objectsMoved;
                _bytesMoved += subzone->quantum_size(q);
            }
        }
        
        inline void operator() (Zone *zone, Subzone *subzone, usword_t q) {
            if (_pinnedPagesOnly) {
                // first pass, only compact blocks from unpinned pages.
                void *block = subzone->quantum_address(q);
                void *block_page = align_down(block);
                if (block_page != _currentPage) {
                    _currentPage = block_page;
                    _currentPagePinned = is_page_pinned(subzone, block_page);
                    if (!_currentPagePinned) ++_pagesCompacted;
                }
                if (!_currentPagePinned && subzone->is_compactable(q)) {
                    forward_block(zone, subzone, q, block);
                }
            } else if (subzone->is_compactable(q)) {
                // second pass, compact the rest. filter out already moved objects.
                if (!subzone->is_forwarded(q))
                    forward_block(zone, subzone, q, subzone->quantum_address(q));
            }
        }
        size_t objectsMoved() { return _objectsMoved; }
        size_t pagesCompacted() { return _pagesCompacted; }
    };
    
    #pragma mark class fixup_phase

    struct fixup_phase {
        // FIXME:  should refactor the scanner to make this a little bit easier.
        struct Configuration;
        typedef ReferenceIterator<Configuration> FixupReferenceIterator;
        class CompactionScanningStrategy : public FullScanningStrategy<FixupReferenceIterator> {
        public:
            inline static const unsigned char *layout_map_for_block(Zone *zone, void *block) {
                if (zone->in_subzone_memory(block)) {
                    Subzone *subzone = Subzone::subzone(block);
                    usword_t q = subzone->quantum_index_unchecked(block);
                    if (subzone->is_forwarded(q)) {
                        // get the layout information from the forwarded block.
                        block = *(void **)block;
                    }
                }
                return zone->layout_map_for_block(block);
            }
        };
        struct Configuration {
            typedef fixup_phase ReferenceVisitor;
            typedef EmptyPendingStack<FixupReferenceIterator> PendingStack;
            typedef CompactionScanningStrategy ScanningStrategy;
        };
        Configuration::PendingStack _stack;
        FixupReferenceIterator _scanner;
        PtrSet _observers; 
        
        fixup_phase(Zone *zone) : _scanner(zone, *this, _stack, (void *)auto_get_sp()) {}

        inline bool is_compacted_pointer(Zone *zone, void *address) {
            if (zone->in_subzone_memory(address)) {
                usword_t q;
                Subzone *subzone = Subzone::subzone(address);
                if (subzone->block_is_start(address, &q) && subzone->is_forwarded(q)) return true;
            }
            return false;
        }
        
        void check_slot_for_observations(void **slot) {
            __block void *slot_block = NULL;
            Zone *zone = _scanner.zone();
            blockStartNoLockDo(zone, slot, ^(Subzone *slot_subzone, usword_t slot_q) {
                slot_block = slot_subzone->quantum_address(slot_q);
            },^(Large *slot_large) {
                slot_block = slot_large->address();
            });
            if (slot_block) {
                // refactor. need zone.get_assocative_ref_internal().
                AssociationsHashMap &associations = zone->associations();
                AssociationsHashMap::iterator i = associations.find(slot_block);
                if (i != associations.end()) {
                    ObjectAssociationMap *refs = i->second;
                    ObjectAssociationMap::iterator j = refs->find(&CompactionObserverKey);
                    if (j != refs->end()) {
                        void *observer = j->second;
                        if (is_compacted_pointer(zone, observer)) {
                            j->second = observer = *(void **)observer;
                        }
                        _observers.insert(observer);
                    }
                }
            }
        }

        void visit(const ReferenceInfo &info, void **slot, Subzone *subzone, usword_t q) {
            if (subzone->is_forwarded(q)) {
                void *address = *slot;
                if (address == CompactionWatchPoint) {
                    printf("fixing up a reference to CompactionWatchPoint = %p\n", CompactionWatchPoint);
                }
                *slot = *(void**)address;
                check_slot_for_observations(slot);
            }
        }
        
        void visit(const ReferenceInfo &info, void **slot, Large *large) {}
    
        inline void operator() (Zone *zone, Subzone *subzone, usword_t q) {
            // ignore moved blocks themselves.
            usword_t layout = subzone->layout(q);
            if ((layout & AUTO_UNSCANNED) == 0) {
                if (layout & AUTO_OBJECT) {
                    _scanner.scan(subzone, q);
                } else if (layout == AUTO_POINTERS_ONLY) {
                    ReferenceInfo all_pointers_info(kAllPointersHeapReference, &subzone->write_barrier());
                    _scanner.scan_range(all_pointers_info, Range(subzone->quantum_address(q), subzone->size(q)));
                }
            }
        }

        inline void operator() (Zone *zone, Large *large) {
            usword_t layout = large->layout();
            if ((layout & AUTO_UNSCANNED) == 0) {
                if (layout & AUTO_OBJECT) {
                    _scanner.scan(large);
                } else if (layout == AUTO_POINTERS_ONLY) {
                    ReferenceInfo all_pointers_info(kAllPointersHeapReference, &large->write_barrier());
                    _scanner.scan_range(all_pointers_info, Range(large->address(), large->size()));
                }
            }
        }
        
        // fixup associative reference slots.
        inline bool operator() (Zone *zone, void *object, void *key, void *&value) {
            void *address = value;
            if (is_compacted_pointer(zone, address)) {
                value = *(void **)address;
            }
            return true;
        }
        
        // fixup root slots.
        inline void operator() (Zone *zone, void **root) {
            void *address = *root;
            if (is_compacted_pointer(zone, address)) {
                *root = *(void **)address;
            }
        }
        
        // visit weak slots.
        inline void operator () (Zone *zone, weak_referrer_t &ref) {
            // check to see if the referrer is pointing to a block that has been forwarded.
            // Zone::forward_block() should never leave any dangling pointers to blocks in the table,
            // so we check that here with an assertion. 
            void *referent = *ref.referrer;
            ASSERTION(!is_compacted_pointer(zone, referent));
            // fixup the callback slot.
            if (ref.block && is_compacted_pointer(zone, ref.block)) {
                ref.block = (auto_weak_callback_block_t *)*(void **)ref.block;
            }
        }
    };
    
    #pragma mark class move_object_phase

    struct move_object_phase {
        inline void operator() (Zone *zone, Subzone *subzone, usword_t q) {
            if (subzone->is_forwarded(q)) {
                zone->move_block(subzone, q, subzone->quantum_address(q));
            }
        }
        inline void operator() (Zone *zone, Large *large) {}
    };
    
    #pragma mark class deallocate_phase

    struct relocation {
        size_t _size;
        void *_old_block;
        void *_new_block;
        
        relocation(size_t size, void *old_block, void *new_block) : _size(size), _old_block(old_block), _new_block(new_block) {}
    };
    
    typedef std::vector<relocation, CompactionZoneAllocator<relocation> > relocation_vector;

    struct deallocate_phase {
        size_t _objectsDeallocated;
        relocation_vector &_logVector;
        
        deallocate_phase(relocation_vector &logVector) : _objectsDeallocated(0), _logVector(logVector) {}
        
        inline void operator() (Zone *zone, Subzone *subzone, usword_t q) {
            if (subzone->is_forwarded(q)) {
                ++_objectsDeallocated;

                void *block = subzone->quantum_address(q);
                void *copied_block = ((void **)block)[0];
                if (subzone->layout(q) & AUTO_OBJECT) {
                    if (Environment::log_compactions) printf("moved %p -> %p (%s)\n", block, copied_block, zone->name_for_object(copied_block));
                } else {
                    if (Environment::log_compactions) printf("moved %p -> %p (%lu bytes)\n", block, copied_block, subzone->size(q));
                }
                
                if (_old_malloc_logger) _logVector.push_back(relocation(subzone->size(q), block, copied_block));

#if DEBUG
                // this ensures that the old pointer is no longer in the weak table.
                weak_enumerate_weak_references(zone, subzone->quantum_address(q), ^(const weak_referrer_t &ref) {
                    printf("slot = %p, *slot == %p\n", ref.referrer, *ref.referrer);
                    __builtin_trap();
                });
#endif
                
                subzone->admin()->deallocate_no_lock(subzone, q, block);
                subzone->clear_forwarded(q);
            }
        }
        inline void operator() (Zone *zone, Large *large) {}
        size_t objectsDeallocated() { return _objectsDeallocated; }
    };
    
    void *Zone::forward_block(Subzone *subzone, usword_t q, void *block) {
        // used by compacting collector exclusively.
        void *forwarded_block = block;
        if (subzone->is_forwarded(q)) {
            forwarded_block = ((void **)block)[0];
        } else {
            usword_t size = subzone->size(q);
            Admin *block_admin = subzone->admin();
            forwarded_block = block_admin->allocate_destination_block_no_lock(subzone, q, block);
            if (forwarded_block != block) {
                if (subzone->is_scanned(q)) bzero(forwarded_block, size);
                // save the original first word in the first word of the new block.
                ((void **)forwarded_block)[0] = ((void **)block)[0];
                // leave a forwarding address in the old block.
                ((void **)block)[0] = forwarded_block;
                subzone->mark_forwarded(q);

                // transfer ownership of any associative references to forwarded_block.
                AssociationsHashMap::iterator i = _associations.find(block);
                if (i != _associations.end()) {
                    // need to rehash the top level entry.
                    ObjectAssociationMap* refs = i->second;
                    _associations.erase(i);
                    _associations[forwarded_block] = refs;
                }
                
                // transfer hash code to forwarded block.
                PtrSizeHashMap::iterator h = _hashes.find(block);
                if (h != _hashes.end()) {
                    // need to rehash the top level entry.
                    size_t hashValue = h->second;
                    _hashes.erase(h);
                    _hashes[forwarded_block] = hashValue;
                }

                // transfer weak references OF block TO forwarded_block.
                weak_transfer_weak_referents(this, block, forwarded_block);
            }
        }
        return forwarded_block;
    }

    void Zone::move_block(Subzone *subzone, usword_t q, void *block) {
        // used by compacting collector exclusively.
        ASSERTION(subzone->is_forwarded(q));
        void *copied_block = ((void **)block)[0];
        ASSERTION(in_subzone_memory(copied_block));
        usword_t size = subzone->size(q);
        
        // bitwise copy the rest of the old block into the new block.
        memmove(displace(copied_block, sizeof(void*)), displace(block, sizeof(void*)), size - sizeof(void*));
        usword_t layout = subzone->layout(q);
        if (is_scanned(layout)) {
            // be very conservative. if the block has ANY marked cards, mark all of the cards that span
            // the copied block. otherwise due to block alignment, we could lose information.
            if (subzone->write_barrier().range_has_marked_cards(block, size)) {
                Subzone::subzone(copied_block)->write_barrier().mark_cards(copied_block, size);
            }
        }
        
        // Transfer ownership of weak references inside the old block. For objects, we assume that they contain no weak
        // references if they have no weak layout map. For non-objects, a conservative scan is performed which
        // searches for explicitly registered weak references and transfers their ownership. If any callback blocks
        // were registered, they are NOT transferred. This can only be fixed by the did_move_object callback.
        if (layout & AUTO_OBJECT) {
            const unsigned char *weak_map = weak_layout_map_for_block(copied_block);
            if (weak_map != NULL) weak_transfer_weak_contents(this, (void **)block, (void **)copied_block, weak_map);
        } else if (layout == AUTO_MEMORY_ALL_WEAK_POINTERS) {
            // revisit this later, it's not possible to move a weak backing store simply, because the forwarding pointer
            // may be stored over a live weak reference, which makes updating all weak references to an object complex.
            // see the comment in CompactionClassifier::visit(). If an object with this layout has no pointers stored in
            // it yet, then it is safe to move.
            weak_transfer_weak_contents_unscanned(this, (void **)block, (void **)copied_block, size, true);
        }

        // transfer the age of the block.
        Subzone *copied_subzone = Subzone::subzone(copied_block);
        usword_t copied_q = copied_subzone->quantum_index_unchecked(copied_block);
        copied_subzone->set_age(copied_q, subzone->age(q));
    }
    
    typedef void (^compaction_observer_t) (void);
    
    inline void region_apply(Zone *zone, void (^block) (Region *region)) {
        for (Region *region = zone->region_list(); region != NULL; region = region->next()) {
            block(region);
        }
    }
    
    void Zone::compact_heap() {
        // try to start a compaction cycle.
        if (_compaction_disabled) return;
        pthread_mutex_lock(&_compaction_lock);
        if (_compaction_disabled) {
            pthread_mutex_unlock(&_compaction_lock);
            return;
        }

        // compaction operates entirely in its own zone.
        CompactionZone compactionZone;
        relocation_vector logVector;
        
        // grab all necessary locks to get a coherent picture of the heap.
        pthread_mutex_lock(&_mark_bits_mutex);
        pthread_rwlock_wrlock(&_associations_lock);
        spin_lock(&_datasegments_lock);
        spin_lock(&weak_refs_table_lock);
        pthread_mutex_lock(&_roots_lock);
        
        // we're sorting free lists and allocating new blocks, take those locks too.
        _partition.lock();
        
        // the registered threads will remain suspended until compaction completes.
        suspend_all_registered_threads();

        // clear all the pinned & mark bits.
        region_apply(this, ^(Region *region) {
            region->pinned().commit();
            region->pinned().clear();
            region->clear_marks();
        });

        // sort the free lists so that the lowest block can be found by examining the head of each free list.
        // no need to sort if we're in scramble_heap mode.
        if (!Environment::scramble_heap) sort_free_lists();
        
        // examine the amount of purgabe space BEFORE compaction, so we can compare it to after compaction.
        __block usword_t initial_purgeable_bytes = 0;
        _partition.for_each(^(Admin &admin) {
            initial_purgeable_bytes += admin.purgeable_free_space_no_lock();
        });

        // auto_date_t start = auto_date_now();
        // compute the pinned object set.
        __block size_t objectsPinned = 0;
        mark_pinned_t marker = ^(void **slot, Subzone *subzone, usword_t q, ReferenceKind kind) {
            if (!subzone->test_and_set_pinned(q)) ++objectsPinned;
        };
        CompactionClassifier classifier(this, marker);
        classifier.scan((void *)auto_get_sp());

        // auto_date_t end = auto_date_now();
        // printf("classification took %lld microseconds.\n", (end - start));
        
        // use the pinned object set to perform the copies.

        // Compaction is simple. Take two passes through the heap, moving all unpinned blocks. Then another pass through the heap
        // to fix up all of the objects that point at the newly moved objejcts.
        forwarding_phase forwarder;
        forwarder.setPinnedPagesOnly(true);
        visitAllocatedSubzoneBlocksInReverse(this, forwarder);
        forwarder.setPinnedPagesOnly(false);
        visitAllocatedSubzoneBlocksInReverse(this, forwarder);
        
        // 2. fixup all pointers from old to new.
        fixup_phase fixer(this);
        visitAllocatedBlocksForCompaction(this, fixer);
        visitAssociationsNoLock(this, fixer);
        __block fixup_phase &fixer_ref = fixer;
        visitRootsNoLock(this, ^(void **root) { fixer_ref(this, root); return true; });
        weak_enumerate_table_fixup(this, ^(weak_referrer_t &ref) { fixer_ref(this, ref); });

        // 3. call -moveTo: or bitwise/copy and fixup weak references.
        move_object_phase mover;
        visitAllocatedBlocksForCompaction(this, mover);

        // 4. deallocate the compacted objects.
        deallocate_phase deallocator(logVector);
        visitAllocatedBlocksForCompaction(this, deallocator);
        ASSERTION(deallocator.objectsDeallocated() == forwarder.objectsMoved());

        if (Environment::log_compactions) {
            printf("pinned %ld objects.\n", objectsPinned);
            size_t objectsMoved = forwarder.objectsMoved();
            if (objectsMoved) printf("compacted %ld objects, %ld pages\n", objectsMoved, forwarder.pagesCompacted());
            
            // purge the free lists.
            usword_t bytes_purged = _partition.purge_free_space_no_lock();
            printf("purgeable before compaction %lu bytes (%lu pages).\n", initial_purgeable_bytes, initial_purgeable_bytes / page_size);
            printf("purged %lu bytes (%lu pages)\n", bytes_purged, bytes_purged / page_size);
        }

        region_apply(this, ^(Region *region) {
            region->pinned().uncommit();
        });

        reset_all_marks();
        
        resume_all_registered_threads();
        
        // do the logging of relocated blocks.
        std::for_each(logVector.begin(), logVector.end(), ^(const relocation& r) {
            _old_malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)this, (uintptr_t)r._old_block, (uintptr_t)r._size, (uintptr_t)r._new_block, 0);
        });

        // release locks.
        _partition.unlock();
        pthread_mutex_unlock(&_roots_lock);
        spin_unlock(&weak_refs_table_lock);
        spin_unlock(&_datasegments_lock);
        pthread_rwlock_unlock(&_associations_lock);
        pthread_mutex_unlock(&_mark_bits_mutex);

        // Unblock any threads trying to disable compaction.
        pthread_mutex_unlock(&_compaction_lock);
    }
    
    void Zone::disable_compaction() {
        if (!_compaction_disabled) {
            Mutex compaction_lock(&_compaction_lock);
            _compaction_disabled = true;
        }
    }

    void Zone::set_in_compaction() {
        Mutex mutex(&_registered_threads_mutex);
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            // don't block the collector thread for compaction (avoid deadlock).
            if (thread->is_current_thread()) continue;
            LockedBoolean &in_compaction = thread->in_compaction();
            assert(in_compaction.state == false);
            SpinLock lock(&in_compaction.lock);
            in_compaction.state = true;
        }
    }

    void Zone::compaction_barrier() {
        // Thread Local Enlivening.
        // TODO:  we could optimize this to allow threads to enter during one pass, and then do another pass fully locked down.
        pthread_mutex_lock(&_registered_threads_mutex);
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            // don't block the collector thread for enlivening (avoid deadlock).
            if (thread->is_current_thread()) continue;
            LockedBoolean &needs_enlivening = thread->needs_enlivening();
            spin_lock(&needs_enlivening.lock);
        }
        _enlivening_complete = true;
    }

    void Zone::clear_in_compaction() {
        for (Thread *thread = _registered_threads; thread != NULL; thread = thread->next()) {
            // don't block the collector thread for compaction (avoid deadlock).
            if (thread->is_current_thread()) continue;
            LockedBoolean &in_compaction = thread->in_compaction();
            assert(in_compaction.state == true);
            SpinLock lock(&in_compaction.lock);
            in_compaction.state = false;
        }
        pthread_mutex_unlock(&_registered_threads_mutex);
    }

} // namespace Auto
