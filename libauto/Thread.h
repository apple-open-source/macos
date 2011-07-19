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
    Thread.h
    Registered Thread Management
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_THREAD__
#define __AUTO_THREAD__


#include "Definitions.h"
#include "PointerHash.h"
#include "Locks.h"
#include "Subzone.h"
#include "AllocationCache.h"

namespace Auto {

    //
    // Forward declarations
    //
    class Zone;
    class ThreadLocalCollector;

    //
    // LocalBlocksHash
    // class for holding per-thread objects and for each, two marks
    // XXX todo: simplify PointerHash to be/use standard C++ hash table
    //
    class LocalBlocksHash : public PointerHash {
    public:
        enum {
            FlagScanned = 0x1,
            FlagMarked = 0x2,
        };
        
        LocalBlocksHash(int initialCapacity) : PointerHash(initialCapacity) {}
        
        inline void setScanned(uint32_t index) { setFlag(index, FlagScanned); }
        inline void setScanned(void *p) { int32_t i = slotIndex(p); if (i != -1) setScanned(i); }
        inline bool wasScanned(uint32_t index) { return flagSet(index, FlagScanned); }
        
        inline void setMarked(uint32_t index) { setFlag(index, FlagMarked); }
        inline void setMarked(void *p) { int32_t i = slotIndex(p); if (i != -1) setMarked(i); }
        inline bool wasMarked(uint32_t index) { return flagSet(index, FlagMarked); }
        
        inline bool testAndSetMarked(uint32_t index) {
            bool old = wasMarked(index);
            if (!old) setMarked(index);
            return old;
        }

        // Shark says all these loads are expensive.
        inline void *markedPointerAtIndex(uint32_t index) {
            vm_address_t value = _pointers[index];
            void *pointer = (void *) (value & ~FlagsMask);
            return ((value & FlagMarked) ? pointer : NULL);
        }
        
        inline void *unmarkedPointerAtIndex(uint32_t index) {
            vm_address_t value = _pointers[index];
            void *pointer = (void *) (value & ~FlagsMask);
            return ((value & FlagMarked) ? NULL : ((value == (vm_address_t)RemovedEntry) ? NULL : pointer));
        }
        
        inline void *markedUnscannedPointerAtIndex(uint32_t index) {
            vm_address_t value = _pointers[index];
            void *pointer = (void *) (value & ~FlagsMask);
            return ((value & (FlagMarked|FlagScanned)) == FlagMarked ? pointer : NULL);
        }
        
        inline void clearFlagsRehash()  { rehash(FlagScanned | FlagMarked); }
        inline void clearFlagsCompact() { compact(FlagScanned | FlagMarked); }
        inline bool isFull() { return count() >= local_allocations_size_limit; }
        
        inline size_t localsSize() {
            size_t size = 0;
            for (uint32_t i = firstOccupiedSlot(), last = lastOccupiedSlot(); i <= last; i++) {
                void *block = (*this)[i];
                if (block) {
                    Subzone *subzone = Subzone::subzone(block);
                    usword_t q = subzone->quantum_index_unchecked(block);
                    size += subzone->size(q);
                }
            }
            return size;
        }            
    };
    


    //----- NonVolatileRegisters -----//
    
    //
    // Used to capture the register state of the current thread.
    //


    
    class NonVolatileRegisters {
      private:
#if defined(__i386__)
        // Non-volatile registers are: ebx, ebp, esp, esi, edi
        usword_t _registers[5];  // buffer for capturing registers
        
        //
        // capture_registers
        //
        // Capture the state of the non-volatile registers.
        //
        static inline void capture_registers(register usword_t *registers) {
            __asm__ volatile ("mov %%ebx,  0(%[registers]) \n" 
                              "mov %%ebp,  4(%[registers]) \n" 
                              "mov %%esp,  8(%[registers]) \n" 
                              "mov %%esi, 12(%[registers]) \n" 
                              "mov %%edi, 16(%[registers]) \n" 
                              : : [registers] "a" (registers) : "memory");
        }
#elif defined(__x86_64__)
        // Non-volatile registers are: rbx rsp rbp r12-r15
        usword_t _registers[7];  // buffer for capturing registers
        
        //
        // capture_registers
        //
        // Capture the state of the non-volatile registers.
        //
        static inline void capture_registers(register usword_t *registers) {
            __asm__ volatile ("movq %%rbx,  0(%[registers]) \n" 
                              "movq %%rsp,  8(%[registers]) \n" 
                              "movq %%rbp, 16(%[registers]) \n" 
                              "movq %%r12, 24(%[registers]) \n" 
                              "movq %%r13, 32(%[registers]) \n" 
                              "movq %%r14, 40(%[registers]) \n" 
                              "movq %%r15, 48(%[registers]) \n" 
                              : : [registers] "a" (registers) : "memory");
        }
#elif defined(__arm__)
        // Non-volatile registers are: r4..r8, r10, r11
        // r9 is saved for simplicity.
        usword_t _registers[8];  // buffer for capturing registers

        //
        // capture_registers
        //
        // Capture the state of the non-volatile registers.
        //
        static inline void capture_registers(register usword_t *registers) {
            __asm__ volatile ("stmia %[registers], {r4-r11}"
                              : : [registers] "r" (registers) : "memory");
        }

#else
#error Unknown Architecture
#endif

      public:
      
        //
        // Constructor
        //
        NonVolatileRegisters() { capture_registers(_registers); }
        
        
        //
        // buffer_range
        //
        // Returns the range of captured registers buffer.
        //
        inline Range buffer_range() { return Range(_registers, sizeof(_registers)); }
        
    };

    //----- Thread -----//
    
    //
    // Track threads that need will be scanned during gc.
    //
    
    union ThreadState;
    
    class Thread : public AuxAllocated {
    
      private:
            
        Thread      *_next;                                 // next thread in linked list
        Zone        *_zone;                                 // managing zone
        pthread_t   _pthread;                               // posix thread
        mach_port_t _thread;                                // mach thread
        void        *_stack_base;                           // cached thread stack base (pthread_get_stackaddr_np(_pthread)).
        LockedBoolean _scanning;                            // if state is true, collector is scanning, unbind will block.
        uint32_t    _suspended;                             // records suspend count.
        void        *_stack_scan_peak;                      // lowest scanned stack address, for stack clearing
        ThreadLocalCollector *_tlc;                         // if a TLC is in progress, this is the collector. Otherwise NULL.
        AllocationCache _allocation_cache[2];               // caches[scanned/unscanned], one for each quanta size, slot 0 is for large clumps
        
        LocalBlocksHash _localAllocations;                  // holds blocks local to this thread
        sentinel_t   _localsGuard;

        LockedBoolean   _needs_enlivening;                  // per-thread support for Zone::enlivening_barrier().
        int32_t     _destructor_count;                      // tracks the number of times the pthread's key destructor has been called
        
        bool        _in_collector;                          // used to indicate that a thread is running inside the collector itself
        uint32_t    _tlc_watchdog_counter;                  // used to detect when the thread is idle so the heap collector can run a TLC
        LockedBoolean _in_compaction;                       // per-thread support for compaction read-barrier.
        Subzone::PendingCountAccumulator *_pending_count_accumulator; // buffers adjustments to subzone pending count
        
        // Buffer used by thread local collector. Enough space to hold max possible local blocks. Last so we don't touch as many pages if it doesn't fill up.
        void        *_tlc_buffer[local_allocations_size_limit];
        
        void get_register_state(ThreadState &state, unsigned &user_count);

        //
        // remove_local
        //
        // remove block from local set.  Assumes its there.
        //
        inline void remove_local(void *block) {
            Sentinel guard(_localsGuard);
            _localAllocations.remove(block);
        }

        void flush_cache(AllocationCache &cache);

        //
        // block_escaped_internal
        //
        // a block is escaping the stack; remove it from local set (cheaply)
        //
        template <class BlockRef> void block_escaped_internal(BlockRef block);
        
    public:
      
        //
        // Constructor. Makes a Thread which is bound to the calling pthread.
        //
        Thread(Zone *zone);
        ~Thread();
        
        //
        // bind
        //
        // Associate the Thread with the calling pthread.
        // This declares the Zone's interest in scanning the calling pthread's stack during collections.
        //
        void bind();
        
        //
        // unbind
        //
        // Disassociate the Thread from the calling pthread.
        // May only be called from the same pthread that previously called bind().
        // unbind() synchronizes with stack scanning to ensure that if a stack scan is in progress
        // the stack will remain available until scanning is complete. Returns true if the thread
        // object can be immediately deleted.
        //
        bool unbind();
        
        //
        // lockForScanning
        //
        // Locks down a thread before concurrent scanning. This blocks a concurrent call to
        // unbind(), so a pthread cannot exit while its stack is being concurrently scanned.
        // Returns true if the thread is currently bound, and thus is known to have a valid stack.
        //
        bool lockForScanning();
        
        //
        // unlockForScanning
        //
        // Relinquishes the scanning lock, which unblocks a concurrent call to unbind().
        //
        void unlockForScanning();
        
        //
        // Accessors
        //
        inline Thread      *next()                { return _next; }
        inline Zone        *zone()                { return _zone; }
        inline pthread_t   pthread()              { return _pthread; }
        inline mach_port_t thread()               { return _thread; }
        inline void        set_next(Thread *next) { _next = next; }
        inline AllocationCache &allocation_cache(const usword_t layout) { return _allocation_cache[layout & AUTO_UNSCANNED]; }
        inline void        *stack_base()          { return _stack_base; }
        inline LocalBlocksHash &locals()          { return _localAllocations; }
        inline sentinel_t &localsGuard()          { return _localsGuard; }
        inline bool       is_bound()              { return _pthread != NULL; }
        inline int32_t    increment_tsd_count()   { return ++_destructor_count; }
        inline void       set_in_collector(bool value) { _in_collector = value; }
        inline bool       in_collector() const    { return _in_collector; }
        inline void       set_thread_local_collector(ThreadLocalCollector *c) { _tlc = c; }
        inline ThreadLocalCollector *thread_local_collector() { return _tlc; }
        inline void       **tlc_buffer()          { return _tlc_buffer; }
        
        inline bool       tlc_watchdog_should_trigger() { return _tlc_watchdog_counter == 4; }
        inline void       tlc_watchdog_disable()  { _tlc_watchdog_counter = 5; }
        inline void       tlc_watchdog_reset()    { _tlc_watchdog_counter = 0; }
        inline void       tlc_watchdog_tickle()   { if (_tlc_watchdog_counter < 4) _tlc_watchdog_counter++; }
        inline void       set_pending_count_accumulator(Subzone::PendingCountAccumulator *a) { _pending_count_accumulator = a; }
        inline Subzone::PendingCountAccumulator   *pending_count_accumulator() const { return _pending_count_accumulator; }

        //
        // Per-thread envlivening, to reduce lock contention across threads while scanning.
        // These are manipulated by Zone::set_needs_enlivening() / clear_needs_enlivening().
        //
        // FIXME:  can we make this lockless altogether?
        //
        LockedBoolean     &needs_enlivening()     { return _needs_enlivening; }

        // BlockRef FIXME: retire
        void enliven_block(void *block);
        
        //
        // Per-thread compaction condition.
        //
        LockedBoolean     &in_compaction()        { return _in_compaction; }
        
        //
        // clear_stack
        //
        // clears stack memory from the current sp to the depth that was scanned by the last collection
        //
        void clear_stack();
        
        //
        // is_current_stack_address
        //
        // If the current thread is registered with the collector, returns true if the given address is within the address
        // range of the current thread's stack. This code assumes that calling pthread_getspecific() is faster than calling
        // pthread_get_stackaddr_np() followed by pthread_get_stacksize_np().
        //
        inline bool is_stack_address(void *address) {
            Range stack(__builtin_frame_address(0), _stack_base);
            return (stack.in_range(address));
        }
        
        //
        // block_escaped
        //
        // inline wrapper around block_escaped_internal to catch the non-local case without making a function call
        //
        template <class BlockRef> inline void block_escaped(BlockRef block) {
            if (block.is_thread_local()) block_escaped_internal(block);
        }
        
        //
        // check_for_escape
        //
        // an assignment is happening.  Check for an escape, e.g. global = local
        //
        template <class DestBlock, class ValueBlock> void track_local_assignment(DestBlock dst, ValueBlock value)
        {
            bool blockStayedLocal = false;
            if (value.is_thread_local()) {
                if (dst.is_live_thread_local()) {
                    dst.set_scan_local_block();
                    //valueSubzone->set_stored_in_heap(value);
                    blockStayedLocal = true;
                }
                if (!blockStayedLocal) {
                    block_escaped_internal(value);
                }
            }
        }
        
        
        //
        // track_local_memcopy
        //
        // make sure that dest is marked if any src's are local,
        // otherwise escape all srcs that are local
        //
        void track_local_memcopy(const void *src, void *dst, size_t size);
        
        //
        // add_local_allocation
        //
        // add a block to this thread's set of tracked local allocations
        //
        void add_local_allocation(void *block) {
            // Limit the size of local block set. This should only trigger rarely.
            Sentinel guard(_localsGuard);
            if (_localAllocations.isFull())
                flush_local_blocks();
            _localAllocations.add(block);
        }
        
        //
        // flush_local_blocks
        //
        // empties the local allocations hash, making all blocks global
        //
        void flush_local_blocks();
        
        //
        // reap_all_local_blocks
        //
        // finalize and free all local blocks without doing any scanning
        // should only be called when it is known the stack cannot root anything, such as thread exit
        //
        void reap_all_local_blocks();
        
        //
        // scan_current_thread
        //
        // Scan the current thread stack and registers for block references.
        //
#ifdef __BLOCKS__
        typedef void (^thread_scanner_t) (Thread *thread, const Range &range);
#else
        class thread_scanner {
        public:
            virtual void operator() (Thread *thread, const Range &range) = 0;
        };
        typedef thread_scanner &thread_scanner_t;
#endif
        
        void scan_current_thread(thread_scanner_t scanner, void *stack_bottom);
        void scan_current_thread(void (*scanner) (Thread*, const Range&, void*), void *arg, void *stack_bottom);
        
        //
        // scan_other_thread
        //
        // Scan a thread other than the current thread stack and registers for block references.
        //
        void scan_other_thread(thread_scanner_t scanner, bool withSuspend);
        void scan_other_thread(void (*scanner) (Thread*, const Range&, void*), void *arg, bool withSuspend);

#ifdef __BLOCKS__
        //
        // dump local objects
        //
        // use callout to dump local objects
        //
        void dump(auto_zone_stack_dump stack_dump, auto_zone_register_dump register_dump, auto_zone_node_dump dump_local_block);

        //
        // visit
        //
        // visits the thread's stack and registers.
        //
        void visit(auto_zone_visitor_t *visitor);
#endif

        //
        // is_current_thread
        //
        // Returns true if the this thread is the current thread.
        //
        inline bool is_current_thread() const {
            return pthread_self() == _pthread;
        }
        
        
        //
        // thread_cache_add
        //
        // return memory to the thread local cache
        // returns true if the block was cached, false if it could not be cached
        //
        bool thread_cache_add(void *block, Subzone *subzone, usword_t q);
        
        
        //
        // unlink
        //
        // Unlink the thread from the list of threads.
        //
        inline void unlink(Thread **link) {
            for (Thread *t = *link; t; link = &t->_next, t = *link) {
                // if found
                if (t == this) {
                    // mend the link
                    *link = t->_next;
                    break;
                }
            }
        }


        //
        // scavenge_threads
        //
        // Walks the list of threads, looking for unbound threads.
        // These are no longer in use, and can be safely deleted.
        //
        static void scavenge_threads(Thread **active_link, Thread **inactive_link) {
            while (Thread *thread = *active_link) {
                SpinLock lock(&thread->_scanning.lock);
                if (!thread->is_bound()) {
                    // remove thread from the active list.
                    *active_link = thread->_next;
                    // put thread on the inactive list.
                    thread->_next = *inactive_link;
                    *inactive_link = thread;
                } else {
                    active_link = &thread->_next;
                }
            }
        }

        //
        // suspend
        //
        // Temporarily suspend the thread from further execution. Logs and terminates process on failure.
        //
        void suspend();

        //
        // resume
        //
        // Resume a suspended thread. Logs and terminates process on failure.
        //
        void resume();

        bool suspended() { return _suspended != 0; }
        
        //
        // description
        //
        // fills in buf with a textual description of the Thread, for debugging
        // returns buf
        //
        char *description(char *buf, size_t bufsz);

    };
    
    // BlockRef FIXME: temporary glue code until all call sites convert to BlockRef.
    template <> void Thread::block_escaped<void *>(void *block); 
};

#endif // __AUTO_THREAD__

