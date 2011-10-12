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
    Definitions.h
    Global Definitions
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_DEFS__
#define __AUTO_DEFS__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#include <sys/mman.h>

#include <pthread.h>
#include <unistd.h>
#include <malloc/malloc.h>

#include <map>
#include <vector>
#include <ext/hash_map>
#include <ext/hash_set>
#include <libkern/OSAtomic.h>
#include <System/pthread_machdep.h>
#include <TargetConditionals.h>

#include "Environment.h"
#include "auto_impl_utilities.h"

//
// utilities and definitions used throughout the Auto namespace
//

#ifdef DEBUG
extern "C" void* WatchPoint;
#endif
extern "C" malloc_zone_t *aux_zone;
extern "C" const char *auto_prelude(void);



namespace Auto {

    //
    // auto_prelude
    //
    // Generate the prelude used for error reporting
    //
    inline const char *prelude(void) { return auto_prelude(); }
    
    
#if defined(DEBUG)
#define ASSERTION(expression) if(!(expression)) { \
        malloc_printf("*** %s: Assertion %s %s.%d\n", prelude(), #expression, __FILE__, __LINE__); \
        __builtin_trap(); \
    }
#else
#define ASSERTION(expression) (void)(expression)
#endif

    //
    // Workaround for declaration problems
    //
    typedef kern_return_t (*auto_memory_reader_t)(task_t remote_task, vm_address_t remote_address, vm_size_t size, void **local_memory);
    typedef void (*auto_vm_range_recorder_t)(task_t, void *, unsigned type, vm_range_t *, unsigned);

    
    typedef unsigned long usword_t;                         // computational word guaranteed to be unsigned
                                                            // assumed to be either 32 or 64 bit
    typedef   signed long  sword_t;                         // computational word guaranteed to be signed
                                                            // assumed to be either 32 or 64 bit
    
    inline usword_t auto_atomic_compare_and_swap(usword_t old_value, usword_t new_value, volatile usword_t *addr)
    {
#if defined(__x86_64__)
        return OSAtomicCompareAndSwap64(old_value, new_value, (volatile int64_t *)addr);
#elif defined(__i386__)
        return OSAtomicCompareAndSwap32(old_value, new_value, (volatile int32_t *)addr);
#else
#error unknown architecture
#endif
    }
    
    inline usword_t auto_atomic_add(sword_t amount, volatile usword_t *addr)
    {
        usword_t old_value, new_value;
        do {
            old_value = *addr;
            new_value = old_value + amount;
        } while (!auto_atomic_compare_and_swap(old_value, new_value, addr));
        return new_value;
    }
    
    


    //
    // Useful constants (descriptives for self commenting)
    //
    enum {
                                                            
        page_size = 0x1000u,                                // vm_page_size but faster since we don't have to load global
        page_size_log2 = 12,                                // ilog2 of page_size

        bits_per_byte = 8,                                  // standard bits per byte
        bits_per_byte_log2 = 3,                             // ilog2 of bits_per_byte
        
        is_64BitWord = sizeof(usword_t) == 8u,              // true if 64-bit computational word
        is_32BitWord = sizeof(usword_t) == 4u,              // true if 32-bit computational word
        
        bytes_per_word = is_64BitWord ? 8u : 4u,            // number of bytes in an unsigned long word
        bytes_per_word_log2 = is_64BitWord ? 3u : 2u,       // ilog2 of bytes_per_word
        
        bits_per_word = is_64BitWord ? 64u : 32u ,          // number of bits in an unsigned long word
        bits_per_word_log2 = is_64BitWord  ? 6u : 5u,       // ilog2 of bits_per_word
        
        bytes_per_quad = 16,                                // bytes in a quad word (vector)
        bytes_per_quad_log2 = 4,                            // ilog2 of bytes_per_quad
        
        bits_per_quad = 128,                                // bits in a quad word (vector)
        bits_per_quad_log2 = 7,                             // ilog2 of bits_per_quad
         
        bits_mask = (usword_t)(bits_per_word - 1),          // mask to get the bit index (shift) in a word
        
        all_zeros = (usword_t)0,                            // a word of all 0 bits
        all_ones = ~all_zeros,                              // a word of all 1 bits
        
        not_found = all_ones,                               // a negative result of search methods
        
        pointer_alignment = is_64BitWord ? 3u : 2u,         // bit alignment required for pointers
        block_alignment = is_64BitWord ? 5u : 4u,           // bit alignment required for allocated blocks
    };
    

    //
    // Useful functions
    //
    
    
    //
    // is_all_ones
    //
    // Returns true if the value 'x' contains all 1s.
    //
    inline const bool is_all_ones(usword_t x) { return !(~x); }
    
    
    //
    // is_all_zeros
    //
    // Returns true if the value 'x' contains all 0s.
    //
    inline const bool is_all_zeros(usword_t x) { return !x; }
    
    //
    // is_some_ones
    //
    // Returns true if the value 'x' contains some 1s.
    //
    inline const bool is_some_ones(usword_t x) { return x != 0; }
    
    
    //
    // is_some_zeros
    //
    // Returns true if the value 'x' contains some 0s.
    //
    inline const bool is_some_zeros(usword_t x) { return ~x != 0; }


    //
    // displace
    //
    // Adjust an address by specified number of bytes.
    //
    inline void *displace(void *address, const intptr_t offset) { return (void *)((char *)address + offset); }
        
    
    //
    // min
    //
    // Return the minumum of two usword_t values.
    //
    static inline const usword_t min(usword_t a, usword_t b) { return a < b ? a : b; }


    //
    // max
    //
    // Return the maximum of two usword_t values.
    //
    static inline const usword_t max(usword_t a, usword_t b) { return a > b ? a : b; }
    
    
    //
    // mask
    //
    // Generate a sequence of n one bits beginning with the least significant bit.
    //
    static inline const usword_t mask(usword_t n) {
        ASSERTION(0 < n && n <= bits_per_word);
        return (2L << (n - 1)) - 1;
    }
    

    //
    // is_power_of_2
    //
    // Returns true if x is an exact power of 2.
    //
    static inline const bool is_power_of_2(usword_t x) { return ((x - 1) & x) == 0; }
    

    //
    // count_leading_zeros
    // 
    // Count the number of leading zeroes
    //
    static inline const usword_t count_leading_zeros(register usword_t value) {
    #if __LP64__
        return value ? __builtin_clzll(value) : bits_per_word;
    #else
        return value ? __builtin_clz(value) : bits_per_word;
    #endif
    }
    
    
    //
    // rotate_bits_left
    //
    // Rotates bits to the left 'n' bits
    //
    inline usword_t rotate_bits_left(usword_t value, usword_t n) { ASSERTION(0 < n && n < bits_per_word); return (value << n) | (value >> (bits_per_word - n)); }

    
    //
    // rotate_bits_right
    //
    // Rotates bits to the right 'n' bits
    //
    inline usword_t rotate_bits_right(usword_t value, usword_t n) { ASSERTION(0 < n && n < bits_per_word); return (value << (bits_per_word - n)) | (value >> n); }

    
    //
    // ilog2
    // 
    // Compute the integer log2 of x such that (x >> ilog2(x)) == 1, ilog2(0) == -1.
    //
    static inline const usword_t ilog2(register usword_t value) { return (bits_per_word - 1) - count_leading_zeros(value); }
    
    
    //
    // partition
    //
    // Determine the partition of 'x' in sets of size 'y'.
    //
    static inline const usword_t partition(usword_t x, usword_t y) { return (x + y - 1) / y; }
    

    //
    // partition2
    //
    // Determine the partition of 'x' in sets of size 2^'y'.
    //
    static inline const usword_t partition2(usword_t x, usword_t y) { return (x + mask(y)) >> y; }
    

    //
    // align
    //
    // Align 'x' up to nearest multiple of alignment 'y'.
    //
    static inline const usword_t align(usword_t x, usword_t y) { return partition(x, y) * y; }
    

    //
    // align2
    //
    // Align 'x' up to nearest multiple of alignment specified by 2^'y'.
    //
    static inline const usword_t align2(usword_t x, usword_t y) { usword_t m = mask(y); return (x + m) & ~m; }
    
    
    //
    // align_down
    //
    // Align and address down to nearest address where the 'n' bits are zero.
    //
    static inline void *align_down(void *address, usword_t n = page_size_log2) {
        usword_t m = mask(n);
        return (void *)((uintptr_t)address & ~m);
    }
    
    
    //
    // align_up
    //
    // Align and address up to nearest address where the 'n' bits are zero.
    //
    static inline void *align_up(void *address, usword_t n = page_size_log2) {
        usword_t m = mask(n);
        return (void *)(((uintptr_t)address + m) & ~m);
    }


    //
    // count_trailing_bits
    // 
    // Count the number of trailing bits, that is, the number of bits following the leading zeroes.
    // Or, out by one ilog2.
    //
    static inline const usword_t count_trailing_bits(register usword_t value) { return bits_per_word - count_leading_zeros(value); }
    

    //
    // trailing_zeros
    // 
    // Return a mask of ones for each consecutive zero starting at the least significant bit.
    // 
    static inline const usword_t trailing_zeros(usword_t x) { return (x - 1) & ~ x; }


    //
    // trailing_ones
    // 
    // Return a mask of ones for each consecutive one starting at the least significant bit.
    // 
    static inline const usword_t trailing_ones(usword_t x) { return x & ~(x + 1); }


    //
    // count_trailing_zeros
    //
    // Return a count of consecutive zeros starting at the least significant bit.
    //
    static inline const usword_t count_trailing_zeros(usword_t x) { return count_trailing_bits(trailing_zeros(x)); }


    //
    // count_trailing_ones
    //
    // Return a count of consecutive ones starting at the least significant bit.
    //
    static inline const usword_t count_trailing_ones(usword_t x) { return count_trailing_bits(trailing_ones(x)); }


    //
    // is_bit_aligned
    //
    // Returns true if the specified address is aligned in a specific bit alignment.
    //
    inline bool is_bit_aligned(void *address, usword_t n) { return !((uintptr_t)address & mask(n)); }
    
    
    //
    // is_pointer_aligned
    //
    // Returns true if the specified address is aligned on a word boundary.
    //
    inline bool is_pointer_aligned(void *address) { return is_bit_aligned(address, pointer_alignment); }
    
    
    //
    // is_block_aligned
    //
    // Returns true if the specified address is aligned on a block boundary (16/32 bytes.)
    //
    inline bool is_block_aligned(void *address) { return is_bit_aligned(address, block_alignment); }
    
    
    
    //
    // is_equal
    //
    // String equality.
    //
    inline bool is_equal(const char *x, const char *y) { return strcmp(x, y) == 0; }


    //
    // error
    //
    // Report an error.
    // 
    inline void error(const char *msg, const void *address = NULL) {
        if (address) malloc_printf("*** %s: agc error for object %p: %s\n", prelude(), address, msg);
        else         malloc_printf("*** %s: agc error: %s\n", prelude(), msg);
#if 0 && defined(DEBUG)
        __builtin_trap();
#endif
    }


    //
    // allocate_memory
    //
    // Allocate vm memory aligned to specified alignment.
    //
    inline void *allocate_memory(usword_t size, usword_t alignment = page_size, signed label = VM_MEMORY_MALLOC) {
        // start search at address zero
        vm_address_t address = 0;
        
#if 0
        switch (label) {
        default:
        case VM_MEMORY_MALLOC: label = VM_MEMORY_APPLICATION_SPECIFIC_1; break; // 240 admin
        case VM_MEMORY_MALLOC_SMALL: label = VM_MEMORY_APPLICATION_SPECIFIC_1 + 1; break; // 241 small/med
        case VM_MEMORY_MALLOC_LARGE: label = VM_MEMORY_APPLICATION_SPECIFIC_1 + 2; break; // 242 large
        }
#endif 
        // vm allocate space
        kern_return_t err = vm_map(mach_task_self(), &address, size, alignment - 1,
                                     VM_FLAGS_ANYWHERE | VM_MAKE_TAG(label),    // first available space
                                     MACH_PORT_NULL,                            // NULL object, so dynamically allocated
                                     0,                                         // offset into object
                                     FALSE,                                     // no need to copy the object
                                     VM_PROT_DEFAULT,                           // current protection
                                     VM_PROT_ALL,                               // maximum protection must be VM_PROT_ALL for madvise() to work. <rdar://problem/7792285>
                                     VM_INHERIT_DEFAULT);                       // standard copy-on-write at fork()
        
        // verify allocation
        if (err != KERN_SUCCESS) {
            malloc_printf("*** %s: Zone::Can not allocate 0x%lx bytes\n", prelude(), size);
            return NULL;
        }
        
#if defined(DEBUG)
        if (Environment::print_allocs) malloc_printf("vm_map @%p %d\n", address, size);
#endif
        // return result
        return (void *)address;
    }


    //
    // deallocate_memory
    //
    // Deallocate vm memory.
    //
    inline void deallocate_memory(void *address, usword_t size) {
#if defined(DEBUG)
        if (Environment::print_allocs) malloc_printf("vm_deallocate @%p %d\n", address, size);
#endif
        kern_return_t err = vm_deallocate(mach_task_self(), (vm_address_t)address, size);
        ASSERTION(err == KERN_SUCCESS);
    }

    //
    // copy_memory
    //
    // Copy vm pages.
    //
    inline void copy_memory(void *dest, void *source, usword_t size) {
        kern_return_t err = vm_copy(mach_task_self(), (vm_address_t)source, size, (vm_address_t)dest);
        ASSERTION(err == KERN_SUCCESS);
    }
    
    //
    // uncommit_memory
    //
    // Give memory back to the system.
    //
    void uncommit_memory(void *address, usword_t size);

    //
    // commit_memory
    //
    // Get uncommitted memory back from the system.
    //
    void commit_memory(void *address, usword_t size);
    
    
    //
    // guard_page
    //
    // Guards one page of memory from all access
    //
    inline void guard_page(void *address) { vm_protect(mach_task_self(), (vm_address_t)address, page_size, false, VM_PROT_NONE); }

    
    //
    // unguard_page
    //
    // Removes guard from page.
    //
    inline void unguard_page(void *address) { vm_protect(mach_task_self(), (vm_address_t)address, page_size, false, VM_PROT_DEFAULT); }

    
    //
    // allocate_guarded_memory
    //
    // Allocate vm memory bounded by guard pages at either end.
    //
    inline void *allocate_guarded_memory(usword_t size) {
        usword_t needed = align2(size, page_size_log2);
        // allocate two extra pages, one for either end
        void * allocation = allocate_memory(needed + 2 * page_size, page_size, VM_MEMORY_MALLOC);
        
        if (allocation) {
            // front guard
            guard_page(allocation);
            // rear guard
            guard_page(displace(allocation, page_size + needed));
            // return allocation skipping front guard
            return displace(allocation, page_size);
        }
        
        // return NULL
        return allocation;
    }
    
    
    //
    // deallocate_guarded_memory
    //
    // Deallocate vm memory and surrounding guard pages.
    //
    inline void deallocate_guarded_memory(void *address, usword_t size) {
        usword_t needed = align2(size, page_size_log2);
        deallocate_memory(displace(address, -page_size), needed + 2 * page_size);
    }
    
    
    //
    // watchpoint
    //
    // Trap if the address matches the specified trigger.
    //
    inline void watchpoint(void *address) {
#if DEBUG
        if (address == WatchPoint) __builtin_trap();
#endif
    }

    
    //
    // micro_time
    // 
    // Returns execution time in microseconds (not real time.)
    //
    uint64_t micro_time();
    

    //
    // MicroTimer
    //
    // Execution timer convenience class.
    //
    class MicroTimer {
        uint64_t _start, _stop;
    public:
        MicroTimer() : _start(0), _stop(0) {}
        void start() { _start = micro_time(); }
        void stop() { _stop = micro_time(); }
        uint64_t elapsed() { return _stop - _start; }
    };
    
    //
    // nano_time
    // 
    // Returns machine time in nanoseconds (rolls over rapidly.)
    //
    double nano_time();

    //
    // NanoTimer
    //
    // Wall clock execution timer.
    //
    class NanoTimer {
        uint64_t _start, _stop;
        mach_timebase_info_data_t &_timebase;
        static mach_timebase_info_data_t &cached_timebase_info();
    public:
        NanoTimer() : _start(0), _stop(0), _timebase(cached_timebase_info()) {}
        
        void start()        { _start = mach_absolute_time(); }
        void stop()         { _stop = mach_absolute_time(); }
        uint64_t elapsed()  { return ((_stop - _start) * _timebase.numer) / _timebase.denom; }
    };

    
    //
    // zone locks
    //
    extern "C" void auto_gc_lock(malloc_zone_t *zone);
    extern "C" void auto_gc_unlock(malloc_zone_t *zone);

    //----- MemoryReader -----//

    //
    // Used to read another task's memory.
    //

    class MemoryReader {
        task_t                  _task;                          // task being probed
        auto_memory_reader_t    _reader;                        // reader used to laod task memory
      public:
        MemoryReader(task_t task, auto_memory_reader_t reader) : _task(task), _reader(reader) {}
        
        //
        // read
        //
        // Read memory from the task into current memory.
        //
        inline void *read(void *task_address, usword_t size) {
            void *local_address;                           // location where memory was read
            kern_return_t err = _reader(_task, (vm_address_t)task_address, (vm_size_t)size, &local_address);
            if (err) return NULL;
            return local_address;
        }
    };

    //----- Preallocated -----//
    
    //
    // Used in classes where memory is presupplied.
    //
    
    class Preallocated {
    
      public:
      
        // prevent incorrect use of new
        void *operator new(size_t size) { error("Must use alternate form of new operator."); return aux_malloc(size); }
      
        // must supply an address
        void *operator new(size_t size, void *address) { return address; }
       
        // do not delete
        void operator delete(void *x) {  }
        
    };
    
    
    //----- AuxAllocated -----//
    
    //
    // Used in classes where memory needs to be allocated from aux malloc.
    //
    
    class AuxAllocated {
    
      public:
    
        //
        // allocator
        //
        inline void *operator new(const size_t size) {
            void *memory = aux_malloc(size);
            if (!memory) error("Failed of allocate memory for auto internal use.");
            return memory;
        }
        
        inline void *operator new(const size_t size, const size_t extra) {
            void *memory = aux_malloc(size+extra);
            if (!memory) error("Failed of allocate memory for auto internal use.");
            return memory;
        }
        

        //
        // deallocator
        //
        inline void operator delete(void *address) {
            if (address) aux_free(address);
        }
    };
    

    //----- AuxAllocator -----//
    
    //
    // Support for STL allocation in aux malloc.
    //
    
    template <typename T> struct AuxAllocator {

        typedef T                 value_type;
        typedef value_type*       pointer;
        typedef const value_type *const_pointer;
        typedef value_type&       reference;
        typedef const value_type& const_reference;
        typedef size_t            size_type;
        typedef ptrdiff_t         difference_type;

        template <typename U> struct rebind { typedef AuxAllocator<U> other; };

        template <typename U> AuxAllocator(const AuxAllocator<U>&) {}
        AuxAllocator() {}
        AuxAllocator(const AuxAllocator&) {}
        ~AuxAllocator() {}

        pointer address(reference x) const { return &x; }
        const_pointer address(const_reference x) const { 
            return x;
        }

        pointer allocate(size_type n, const_pointer = 0) {
            return static_cast<pointer>(aux_calloc(n, sizeof(T)));
        }

        void deallocate(pointer p, size_type) { ::aux_free(p); }

        size_type max_size() const { 
            return static_cast<size_type>(-1) / sizeof(T);
        }

        void construct(pointer p, const value_type& x) { 
            new(p) value_type(x); 
        }

        void destroy(pointer p) { p->~value_type(); }

        void operator=(const AuxAllocator&);

    };


    template<> struct AuxAllocator<void> {
        typedef void        value_type;
        typedef void*       pointer;
        typedef const void *const_pointer;

        template <typename U> struct rebind { typedef AuxAllocator<U> other; };
    };


    template <typename T>
    inline bool operator==(const AuxAllocator<T>&, 
                           const AuxAllocator<T>&) {
        return true;
    }


    template <typename T>
    inline bool operator!=(const AuxAllocator<T>&, 
                           const AuxAllocator<T>&) {
        return false;
    }
    
    struct AuxPointerLess {
      bool operator()(const void *p1, const void *p2) const {
        return p1 < p2;
      }
    };
    
    struct AuxPointerEqual {
        bool operator()(void *p1, void *p2) const {
            return p1 == p2;
        }
    };

    struct AuxPointerHash {
        uintptr_t operator()(void *p) const {
            return (uintptr_t)p;
        }
    };

    typedef std::vector<void *, AuxAllocator<void *> > PtrVector;
    typedef std::map<void *, int, AuxPointerLess, AuxAllocator<std::pair<void * const, int> > > PtrIntMap;
    typedef std::map<void *, void *, AuxPointerLess, AuxAllocator<std::pair<void * const, void * const> > > PtrPtrMap;
    typedef __gnu_cxx::hash_map<void *, void *, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > PtrPtrHashMap;
    typedef __gnu_cxx::hash_map<void *, PtrPtrHashMap, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > PtrAssocHashMap;
    typedef __gnu_cxx::hash_map<void *, int, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > PtrIntHashMap;
    typedef __gnu_cxx::hash_map<void *, size_t, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > PtrSizeHashMap;
    typedef __gnu_cxx::hash_set<void *, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > PtrHashSet;
    
    //
    // PointerArray
    // Stores a contiguous array of pointers, which is resized by amortized doubling.
    // Uses a MemoryAllocator template parameter which must implement the methods:
    //   void *allocator_memory(size_t size);
    //   void deallocator_memory(void *pointer, size_t size);
    //   void uncommit_memory(void *pointer, size_t size);
    //   void copy_memory(void *dest, void *source, size_t size);
    //

    template <typename MemoryAllocator>
    class PointerArray : AuxAllocated {
        usword_t                        _count;
        usword_t                        _capacity;
        void                          **_buffer;
        MemoryAllocator                 _allocator;
    public:
        PointerArray() : _count(0), _capacity(0), _buffer(NULL) {}
        PointerArray(MemoryAllocator allocator) : _count(0), _capacity(0), _buffer(NULL), _allocator(allocator) {}
        ~PointerArray()                 { if (_buffer) _allocator.deallocate_memory(_buffer, _capacity * sizeof(void *)); }
        
        usword_t count()          const { return _count; }
        void clear_count()              { _count = 0; }
        void set_count(usword_t n)      { _count = n; }
        void **buffer()                 { return _buffer; }
        usword_t size()                 { return _capacity * sizeof(void*); }

        void uncommit()                 { if (_buffer) uncommit_memory(_buffer, _capacity * sizeof(void*)); }
        void commit()                   { if (_buffer) commit_memory(_buffer, _capacity * sizeof(void*)); }

        void grow() {
            if (!_buffer) {
                // start off with 1 page.
                _capacity = page_size / sizeof(void*);
                _buffer = (void **) _allocator.allocate_memory(page_size);
             } else {
                // double the capacity.
                vm_size_t old_size = _capacity * sizeof(void *);
                void **new_buffer = (void **) _allocator.allocate_memory(old_size * 2);
                if (!new_buffer) {
                    auto_fatal("PointerArray::grow() _capacity=%lu failed.\n", _capacity * 2);
                }
                _capacity *= 2;
                _allocator.copy_memory(new_buffer, _buffer, old_size);
                _allocator.deallocate_memory(_buffer, old_size);
                _buffer = new_buffer;
            }
        }
        
        void grow(usword_t count) {
            if (count > _capacity) {
                usword_t old_size = _capacity * sizeof(void *);
                if (_capacity == 0L) _capacity = page_size / sizeof(void *);
                while (count > _capacity) _capacity *= 2;
                void **new_buffer = (void **) _allocator.allocate_memory(_capacity * sizeof(void*));
                if (!new_buffer) {
                    auto_fatal("PointerArray::grow(count=%lu) failed.\n", _capacity);
                }
                if (_buffer) {
                    // only copy contents if _count != 0.
                    if (new_buffer && _count) {
                        _allocator.copy_memory(new_buffer, _buffer, old_size);
                    }
                    _allocator.deallocate_memory(_buffer, old_size);
                }
                _buffer = new_buffer;
            }
        }
        
        void add(void *pointer) {
            if (_count == _capacity) grow();
            _buffer[_count++] = pointer;
        }
    };
    
    //
    // PointerQueue
    // Manages a set of pointers as a queue of discontinguous page-sized chunks. This uses memory more efficiently
    // since it never has to copy the buffers themselves, and grows mores slowly than a PointerArray. Also parametrized
    // with a MemoryAllocator type.
    // 
    struct PointerChunk : Preallocated {
        PointerChunk *_next;
        enum { chunk_size = (Auto::page_size / sizeof(void*)) - 1 };
        void *_pointers[chunk_size];

        void **pointers() { return _pointers; }
        void **limit() { return _pointers + chunk_size; }
        PointerChunk *next() { return _next; }
    };
    
    template <class Visitor> inline void visitPointerChunks(PointerChunk *chunks, usword_t count, Visitor& visitor) {
        PointerChunk *chunk = chunks;
        while (chunk != NULL) {
            PointerChunk *next = chunk->next();
            void **pointers = chunk->pointers();
            void **limit = pointers + (count > PointerChunk::chunk_size ? PointerChunk::chunk_size : count);
            visitor(pointers, limit);
            count -= (limit - pointers);
            chunk = next;
        }
    }

    template <typename MemoryAllocator>
    class PointerQueue : AuxAllocated {
        MemoryAllocator _allocator;
        PointerChunk *_head;
        PointerChunk *_tail;
        PointerChunk *_current;
        void **_cursor, **_limit;
        usword_t _count;
        
        void next_chunk() {
            if (_current == NULL) {
                // reset() pointed back to the beginning.
                _current = _head;
            } else {
                _current = _current->_next;
            }
            if (_current == NULL) {
                _current = (PointerChunk *)_allocator.allocate_memory(sizeof(PointerChunk));
                if (_head == NULL) {
                    _head = _tail = _current;
                } else {
                    _tail->_next = _current;
                    _tail = _current;
                }
            }
            _cursor = _current->pointers();
            _limit = _current->limit();
        }

    public:
        PointerQueue() : _head(NULL), _tail(NULL), _current(NULL), _cursor(NULL), _limit(NULL), _count(0) {}
        PointerQueue(MemoryAllocator allocator) : _allocator(allocator), _head(NULL), _tail(NULL), _current(NULL), _cursor(NULL), _limit(NULL), _count(0) {}
        
        ~PointerQueue() {
            PointerChunk *chunk = _head;
            while (chunk != NULL) {
                PointerChunk *next = chunk->_next;
                _allocator.deallocate_memory(chunk, sizeof(PointerChunk));
                chunk = next;
            }
        }
        
        void add(void *pointer) {
            if (_cursor == _limit) next_chunk();
            *_cursor++ = pointer;
            ++_count;
        }
        
        void reset() {
            _current = NULL;
            _cursor = _limit = NULL;
            _count = 0;
        }

        PointerChunk *chunks() { return _head; }
        usword_t count() { return _count; }
        
        usword_t size() {
            usword_t size = 0;
            PointerChunk *chunk = _head;
            while (chunk != NULL) {
                size += Auto::page_size;
                chunk = chunk->_next;
            }
            return size;
        }
    };
    
    class VMMemoryAllocator {
    public:
        inline void *allocate_memory(usword_t size) {
            return Auto::allocate_memory(size);
        }
        
        inline void deallocate_memory(void *address, usword_t size) {
            Auto::deallocate_memory(address, size);
        }
        
        inline void uncommit_memory(void *address, usword_t size) {
            Auto::uncommit_memory(address, size);
        }
        
        inline void copy_memory(void *dest, void *source, usword_t size) {
            Auto::copy_memory(dest, source, size);
        }
    };
};

#endif // __AUTO_DEFS__
