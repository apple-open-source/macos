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
    BlockRef.h
    Block sieve helper classes.
    Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 */

#include "Subzone.h"
#include "Large.h"
#include "Zone.h"
#include "Admin.h"

namespace Auto {
    
    class SubzoneBlockRef {
        Subzone * const _subzone;
        const usword_t _q;
        
    public:
        SubzoneBlockRef(Subzone *subzone, usword_t q) : _subzone(subzone), _q(q) {}
        SubzoneBlockRef(void *ptr) : _subzone(Subzone::subzone(ptr)), _q(_subzone->quantum_index_unchecked(ptr)) {}

        inline Subzone *    subzone()       const { return _subzone; }
        inline usword_t     q()             const { return _q; }
        
        inline Zone *       zone()          const { return subzone()->admin()->zone(); }
        inline usword_t     size()          const { return subzone()->size(q()); }
        inline void *       address()       const { return subzone()->quantum_address(q()); }
        inline bool         has_refcount()  const { return subzone()->has_refcount(q()); }
        inline bool         is_garbage()    const { return subzone()->is_garbage(q()); }
        inline bool         is_scanned()    const { return ::is_scanned(layout()); }
        inline bool         is_object()     const { return ::is_object(layout()); }
        inline auto_memory_type_t layout()  const { return subzone()->layout(q()); }
        inline bool         is_thread_local() const { return subzone()->is_thread_local(q()); }
        inline bool         is_live_thread_local() const { return subzone()->is_live_thread_local(q()); }
        inline bool         is_local_garbage() const { return subzone()->is_local_garbage(q()); }
        inline bool         should_scan_local_block() const { return subzone()->should_scan_local_block(q()); }
        inline void         set_scan_local_block() const { subzone()->set_scan_local_block(q()); }
        inline void         mark_card(const void *addr) const { if (is_scanned()) subzone()->write_barrier().mark_card((void *)addr); }
        inline bool         is_new()        const { return subzone()->is_new(q()); }
        inline bool         is_marked()     const { return subzone()->is_marked(q()); }
        inline void         get_description(char *buf, usword_t bufsz) const { snprintf(buf, bufsz, "Subzone=%p, q=%ld", subzone(), (long)q()); }
        inline void         make_global()   const { subzone()->make_global(q()); }
        inline void         set_layout(auto_memory_type_t layout) const { subzone()->set_layout(q(), layout); }
        
        inline void         enliven()       const {
            if (!subzone()->test_and_set_mark(q()) && subzone()->is_scanned(q()))
                subzone()->test_and_set_pending(q(), true);
        }
        
        usword_t refcount()  const;
        usword_t inc_refcount() const;
        usword_t dec_refcount_no_lock() const;
        usword_t dec_refcount() const;
    };
    
    class LargeBlockRef {
        Large * const _large;
        
    public:
        LargeBlockRef(Large *large) : _large(large) {}
        
        inline Large *      large()         const { return _large; }
        
        inline Zone *       zone()          const { return _large->zone(); }
        inline usword_t     size()          const { return large()->size(); }
        inline bool         has_refcount()  const { return refcount() != 0; }
        inline void *       address()       const { return _large->address(); }
        inline bool         is_garbage()    const { return _large->is_garbage(); }
        inline bool         is_scanned()    const { return _large->is_scanned(); }
        inline bool         is_object()     const { return _large->is_object(); }
        inline auto_memory_type_t layout()  const { return _large->layout(); }
        inline bool         is_thread_local() const { return false; }
        inline bool         is_live_thread_local() const { return false; }
        inline bool         is_local_garbage() const { return false; }
        inline bool         should_scan_local_block() const { return false; }
        inline void         set_scan_local_block() const { }
        inline void         mark_card(const void *addr) const { if (is_scanned()) _large->write_barrier().mark_card((void *)addr); }
        inline bool         is_new()        const { return _large->is_new(); }
        inline bool         is_marked()     const { return _large->is_marked(); }
        inline void         get_description(char *buf, usword_t bufsz) const { snprintf(buf, bufsz, "Large=%p", _large); }
        inline void         make_global()   const { }
        inline void         set_layout(auto_memory_type_t layout) const { _large->set_layout(layout); }
        
        inline void         enliven()       const {
            if (!_large->test_and_set_mark() && _large->is_scanned())
                _large->set_pending();
        }
        
        inline usword_t     refcount()      const { return _large->refcount(); }
        usword_t inc_refcount() const;
        usword_t dec_refcount_no_lock() const;
        usword_t dec_refcount() const;
    };

    class sieve_base {
    public:
        //
        // sieve_base_pointer
        //
        // This template function is used to determine whether an arbitrary pointer is a block start, and dispatches
        // to a handler based on the type of block. The sieve must implement:
        //     void processBlock(SubzoneBlockRef ref);
        //     void processBlock(LargeBlockRef ref);
        //     void nonBlock(void *ptr);
        // The intended use of this function is to be a fast dispatcher based on the type of block. As such it is
        // intended to be inlined, and all the sieve functions are intended to be inlined.
        template <class Sieve> static inline void sieve_base_pointer(Zone *zone, const void *ptr, Sieve &sieve) TEMPLATE_INLINE {
            if (auto_expect_true(zone->address_in_arena(ptr))) {
                if (auto_expect_true(zone->in_subzone_bitmap((void *)ptr))) {
                    Subzone *sz = Subzone::subzone((void *)ptr);
                    usword_t q;
                    if (auto_expect_true(sz->block_is_start((void *)ptr, &q))) {
                        SubzoneBlockRef block(sz, q);
                        sieve.processBlock(block);
                    } else {
                        sieve.nonBlock(ptr);
                    }
                } else if (auto_expect_true(zone->block_is_start_large((void *)ptr))) {
                    LargeBlockRef block(Large::large((void *)ptr));
                    sieve.processBlock(block);
                } else {
                    sieve.nonBlock(ptr);
                }
            } else {
                sieve.nonBlock(ptr);
            }
        }
        
        //
        // auto_base_pointer_sieve
        //
        // This template function is used to determine whether an arbitrary pointer is a pointer into a block, and dispatches
        // to a handler based on the type of block. The sieve must implement:
        //     void processBlock(SubzoneBlockRef ref);
        //     void processBlock(LargeBlockRef ref);
        //     void nonBlock(void *ptr);
        // The intended use of this function is to be a fast dispatcher based on the type of block. As such it is
        // intended to be inlined, and all the sieve functions are intended to be inlined.
        template <class Sieve> static inline void sieve_interior_pointer(Zone *zone, const void *ptr, Sieve &sieve) TEMPLATE_INLINE {
            if (auto_expect_true(zone->address_in_arena(ptr))) {
                if (auto_expect_true(zone->in_subzone_bitmap((void *)ptr))) {
                    Subzone *sz = Subzone::subzone((void *)ptr);
                    usword_t q;
                    if (auto_expect_true(sz->block_start((void *)ptr, q) != NULL)) {
                        SubzoneBlockRef block(sz, q);
                        sieve.processBlock(block);
                    } else {
                        sieve.nonBlock(ptr);
                    }
                } else {
                    // BlockRef FIXME: block_start_large contains a redundant check of the coverage
                    Large *large = zone->block_start_large((void *)ptr);
                    if (auto_expect_true(large != NULL)) {
                        LargeBlockRef block(large);
                        sieve.processBlock(block);
                    } else {
                        sieve.nonBlock(ptr);
                    }
                }
            } else {
                sieve.nonBlock(ptr);
            }
        }
        
        inline void nonBlock(const void *ptr) {
        }
    };
    
    // Sieve classes for use with auto_pointer_sieve and auto_base_pointer_sieve
    
    enum {
        AUTO_BLOCK_INFO_IS_BLOCK = 0, // we always record whether it is a block or not
        AUTO_BLOCK_INFO_LAYOUT = 1,
        AUTO_BLOCK_INFO_SIZE = 2,
        AUTO_BLOCK_INFO_REFCOUNT = 4,
        AUTO_BLOCK_INFO_BASE_POINTER = 8,
    };
    
    // Sieve class that records information about a block.
    // If AUTO_BLOCK_INFO_BASE_POINTER is specified then ptr may be an interior pointer.
    template <int requested_info> class auto_block_info_sieve : public sieve_base {
    private:
        auto_memory_type_t _layout;
        usword_t _refcount;
        size_t _size;
        boolean_t _is_block;
        void *_base;
        
    public:
        auto_block_info_sieve(Zone *zone, const void *ptr) __attribute__((always_inline)) {
            if (requested_info & AUTO_BLOCK_INFO_BASE_POINTER) {
                sieve_interior_pointer(zone, ptr, *this);
            } else {
                sieve_base_pointer(zone, ptr, *this);
            }
        }
        
        template <class BlockRef> void processBlock(BlockRef ref) TEMPLATE_INLINE {
            _is_block = true;
            if (requested_info & AUTO_BLOCK_INFO_LAYOUT)
                _layout = ref.layout();
            if (requested_info & AUTO_BLOCK_INFO_SIZE)
                _size = ref.size();
            if (requested_info & AUTO_BLOCK_INFO_REFCOUNT)
                _refcount = ref.refcount();
            if (requested_info & AUTO_BLOCK_INFO_BASE_POINTER)
                _base = ref.address();
        }
        
        void nonBlock(const void *ptr) __attribute__((always_inline)) {
            _is_block = false;
            if (requested_info & AUTO_BLOCK_INFO_LAYOUT)
                _layout = AUTO_TYPE_UNKNOWN;
            if (requested_info & AUTO_BLOCK_INFO_SIZE)
                _size = 0;
            if (requested_info & AUTO_BLOCK_INFO_REFCOUNT)
                _refcount = 0;
            if (requested_info & AUTO_BLOCK_INFO_BASE_POINTER)
                _base = NULL;
        }
        
        inline boolean_t            is_block()  const { return _is_block; }
        inline auto_memory_type_t   layout()    const { if (!(requested_info & AUTO_BLOCK_INFO_LAYOUT)) __builtin_trap(); return _layout; }
        inline size_t               size()      const { if (!(requested_info & AUTO_BLOCK_INFO_SIZE)) __builtin_trap(); return _size; }
        inline usword_t             refcount()  const { if (!(requested_info & AUTO_BLOCK_INFO_REFCOUNT)) __builtin_trap(); return _refcount; }
        inline void *               base()      const { if (!(requested_info & AUTO_BLOCK_INFO_BASE_POINTER)) __builtin_trap(); return _base; }
    };
    
    enum {
        AUTO_REFCOUNT_INCREMENT,
        AUTO_REFCOUNT_DECREMENT
    };
    
    // Sieve class that deallocates a block.
    template <int refcount_op> class auto_refcount_sieve : public sieve_base {
    public:
        Zone *zone;
        usword_t refcount;
        
        inline auto_refcount_sieve(Zone *z, const void *ptr) __attribute__((always_inline)) : zone(z), refcount(0) {
            sieve_base_pointer(zone, ptr, *this);
        }
        
        template <class BlockRef> inline void processBlock(BlockRef ref) TEMPLATE_INLINE {
            if (refcount_op == AUTO_REFCOUNT_INCREMENT) {
                refcount = zone->block_increment_refcount(ref);
            } else if (refcount_op == AUTO_REFCOUNT_DECREMENT) {
                refcount = zone->block_decrement_refcount(ref);
            } else {
                __builtin_trap();
            }
        }        
    };
}
