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
    AutoThread.cpp
    Copyright (c) 2004-2009 Apple Inc. All rights reserved.
 */

#include "AutoDefs.h"
#include "AutoMemoryScanner.h"
#include "AutoThread.h"
#include "AutoZone.h"
#include "AutoThreadLocalCollector.h"

#if defined(__ppc__) || defined(__ppc64__)
// get definitions for C_RED_ZONE.
// http://developer.apple.com/documentation/DeveloperTools/Conceptual/LowLevelABI/Articles/32bitPowerPC.html#//apple_ref/doc/uid/TP40002438-SW6
// http://developer.apple.com/documentation/DeveloperTools/Conceptual/LowLevelABI/Articles/64bitPowerPC.html#//apple_ref/doc/uid/TP40002471-SW17
// NOTE:  the following header file contradicts the public ppc64 ABI, specifying a larger value for C_RED_ZONE.
#include <architecture/ppc/cframe.h>
#elif defined(__i386__)
// 32-bit x86 uses no red zone.
#define C_RED_ZONE 0
#elif defined(__x86_64__)
// according to  http://www.x86-64.org/documentation/abi.pdf (page 15)
#define C_RED_ZONE 128
#else
#error Unknown Architecture
#endif


namespace Auto {

    //----- SimplePointerBuffer -------//

    void SimplePointerBuffer::reset() { 
        _cursor = 0; 
        _count = 0; 
        if (_overflow)
            _overflow->reset();
    }
    
    void SimplePointerBuffer::push(void *p) { 
        if (_count < PointerCount) { 
            _pointers[(_cursor + _count) % PointerCount] = p; 
            _count++;
        } else { 
            if (!_overflow) {
                _overflow = new SimplePointerBuffer();
            }
            _overflow->push(p);
        }
    } 
    
    void *SimplePointerBuffer::pop() { 
        void *result = NULL; 
        if (_count > 0) { 
            result = _pointers[_cursor]; 
            _cursor = (_cursor + 1) % PointerCount; 
            _count--;
        } else {
            if (_overflow)
                result = _overflow->pop();
        }
        return result; 
    }
    

    //----- ThreadMemoryAllocator -----//

    void *ThreadMemoryAllocator::allocate_memory(usword_t size) { return _zone->page_allocator().allocate_memory(size); }
    void ThreadMemoryAllocator::deallocate_memory(void *address, usword_t size) { _zone->page_allocator().deallocate_memory(address, size); }

    //----- Thread -----//

    Thread::Thread(Zone *zone)
        : _next(NULL), _zone(zone), _pthread(NULL), _thread(MACH_PORT_NULL), _stack_base(NULL),
          _scanning(), _suspended(0), _stack_scan_peak(NULL), _localAllocations(64), _localAllocationThreshold(100),
          _enlivening_queue(ThreadMemoryAllocator(_zone)), _destructor_count(0), _in_collector(false)
    {
        bind();
    }
    

    Thread::~Thread() {
        /* If any blocks remain in our local allocations list mark them as global. */
        /* Fixme: Theoretically we can probably simply reclaim them, but can we finalize here? */
        for (uint32_t i=_localAllocations.firstOccupiedSlot(); i<=_localAllocations.lastOccupiedSlot(); i++) {
            void *block = _localAllocations[i];
            if (block) {
                Subzone *subzone = Subzone::subzone(block);
                subzone->make_global(block);
            }
        }
        
        // release the per-thread allocation cache items
        usword_t count = 0, size = 0;
        for (usword_t i = 1; i <= maximum_quanta; i++) {
            FreeList &list = _allocation_cache[i];
            const size_t blockSize = i * allocate_quantum_small;
            while (void *block = list.pop()->address()) {
                // mark the thread local block as global so that it can be collected
                assert(_zone->in_subzone_memory(block));
                Subzone *subzone = Subzone::subzone(block);
                subzone->admin()->mark_allocated(block, i, AUTO_MEMORY_UNSCANNED, false, false);
                ++count;
                size += blockSize;
            }
        }
        // when collected, the nodes will decr size & count although
        // which would be wrong unless we do this.
        Statistics &zone_stats = _zone->statistics();
        zone_stats.add_size(size);
        zone_stats.add_count(count);
    }

    //
    // bind
    //
    // Associate the Thread with the calling pthread.
    // This declares the Zone's interest in scanning the calling pthread's stack during collections.
    //
    void Thread::bind() {
        _pthread = pthread_self();
        _thread = pthread_mach_thread_np(_pthread);
        _stack_base = pthread_get_stackaddr_np(_pthread);
        _stack_scan_peak = _stack_base;
    }
    
    
    //
    // unbind
    //
    // Disassociate the Thread from the calling pthread.
    // May only be called from the same pthread that previously called bind().
    // unbind() synchronizes with stack scanning to ensure that if a stack scan is in progress
    // the stack will remain available until scanning is complete. Returns true if the thread
    // can be reclaimed immediately.
    //
    bool Thread::unbind() {
        SpinLock lock(&_scanning.lock);
        assert(!_scanning.state);
        assert(pthread_self() == _pthread);
        _pthread = NULL;
        _thread = MACH_PORT_NULL;
        _stack_base = NULL;
        _stack_scan_peak = NULL;
        // if the enlivening queue is not empty, the collector may need to process it.
        return _enlivening_queue.count() == 0;
    }
    
    bool Thread::lockForScanning() {
        spin_lock(&_scanning.lock);
        if (is_bound()) {
            _scanning.state = true;
            return true;
        }
        spin_unlock(&_scanning.lock);
        return false;
    }
    
    void Thread::unlockForScanning() {
        _scanning.state = false;
        spin_unlock(&_scanning.lock);
    }
    
    
    //
    // clear_stack
    //
    // clears stack memory from the current sp to the depth that was scanned by the last collection
    //
    void Thread::clear_stack() {
        // We need to be careful about calling functions during stack clearing.
        // We can't use bzero or the like to do the zeroing because we don't know how much stack they use.
        // The amount to clear is typically small so just use a simple loop writing pointer sized NULL values.
        void **sp = (void **)auto_get_sp();
        void **zero_addr = (void **)_stack_scan_peak;
        _stack_scan_peak = sp;
        while (zero_addr < sp) {
            *zero_addr = NULL;
            zero_addr++;
        }
    }
    
    
    //
    // flush_local_blocks
    //
    // empties the local allocations hash, making all blocks global
    //
    void Thread::flush_local_blocks()
    {
        // This only gets called if the local block set grows much larger than expected.
        uint32_t first = _localAllocations.firstOccupiedSlot();
        uint32_t last = _localAllocations.lastOccupiedSlot();
        for (uint32_t i = first; i <= last; i++) {
            void *block = _localAllocations[i];
            if (block) {
                Subzone *subzone = Subzone::subzone(block);
                subzone->make_global(block);
                _localAllocations.remove(i);
            }
        }
        // this will cause _localAllocations to resize down its internal pointer buffer
        _localAllocations.grow();
    }

    //
    // block_escaped
    //
    // a block is escaping the stack; remove it from local set (cheaply)
    //
    void Thread::block_escaped(Zone *zone, Subzone *subzone, void *block) {
        if (subzone == NULL) {
            if (zone->in_subzone_memory(block))
                subzone = Subzone::subzone(block);
        }
        if (!subzone || subzone->is_garbage(block)) return; // escaping is no-op if already garbage
        if (subzone->is_thread_local_block(block)) {  // can this be is_thread_local? i.e. is "block" always a real node?
            bool blockNeedsScan = subzone->should_scan_local_block(block);
            if (_localAllocations.contains(block)) {
                if (blockNeedsScan) {
                    ThreadLocalCollector scanner(zone, NULL, *this);
                    //printf("block %p being ejected\n", block);
                    scanner.eject_local_block(block);
                }
                else {	// just do the one
                    subzone->make_global(block);
                    _localAllocations.remove(block);
                    usword_t size = subzone->size(block);
                    Statistics &zone_stats = _zone->statistics();
                    zone_stats.add_count(1);
                    zone_stats.add_size(size);
                    zone->adjust_threshold_counter(size);
                    //printf("block %p being escaped\n", block);
                }
            } else {
                /*
                 It is possible that a thread might construct a pointer to a block which is local to another thread.
                 If that pointer gets stored through a write barrier then we wind up here.
                 It would be an error for the thread to dereference that pointer, but just storing it is technically ok.
                 So this log is not trustworthy.
                 */
                //malloc_printf("*** %s: found local block not in _localAllocations: %p, break on auto_zone_thread_local_error() to debug\n", auto_prelude(), block);
                //auto_zone_thread_local_error();
            }
        }
    }

    //
    // check_for_escape
    //
    // an assignment is happening.  Check for an escape, e.g. global = local
    //
    void Thread::track_local_assignment(Zone *zone, void *dst, void *value)
    {
        if (zone->in_subzone_memory(value)) {
            Subzone *valueSubzone = Subzone::subzone(value);
            if (valueSubzone->is_thread_local(value)) {
                bool blockStayedLocal = false;
                if (zone->in_subzone_memory(dst)) {
                    Subzone *dstSubzone = Subzone::subzone(dst);
                    void *dst_base = dstSubzone->block_start(dst);
                    if (dst_base && dstSubzone->is_live_thread_local(dst_base)) {
                        dstSubzone->set_scan_local_block(dst_base);
                        //valueSubzone->set_stored_in_heap(value);
                        blockStayedLocal = true;
                    }
                }
                if (!blockStayedLocal) {
                    block_escaped(zone, valueSubzone, value);
                }
            }
        }
    }

    //
    // track_local_memcopy
    //
    // If dst is contained in a local scanned object, then if src is also scanned and has
    // local objects that they, in turn, are marked stored as well as dest
    // Otherwise, if dst is unknown, mark all local objects at src as escaped.
    // Src might be the stack.
    void Thread::track_local_memcopy(Zone *zone, const void *src, void *dst, size_t size) {
        Subzone *dstSubzone = NULL;
        void *dstBase = NULL;
        bool should_track_local = false;
        if (zone->in_subzone_memory((void *)dst)) {
            dstSubzone = Subzone::subzone((void *)dst);
            dstBase = dstSubzone->block_start((void *)dst);
            // if memmoving within block bail early
            size_t dstSize = dstSubzone->size((void *)dstBase);
            if (src > dstBase && src < ((char *)dstBase + dstSize))
                return;
            if (dstSubzone->is_live_thread_local(dstBase)
                && (dstSubzone->should_scan_local_block(dstBase) || dstSubzone->is_scanned(dstBase))) {
                should_track_local = true;
            }
        }
        void **start = (void **)src;
        void **end = start + size/sizeof(void *);
        bool dstStoredInto = false;
        while (start < end) {
            void *candidate = *start;
            if (candidate) {
                if (zone->in_subzone_memory(candidate)) {
                    Subzone *candidateSubzone = Subzone::subzone(candidate);
                    if (candidateSubzone->is_live_thread_local_block(candidate)) {// && thread->_localAllocations.contains(candidate))
                        if (should_track_local) {
                            dstStoredInto = true;
                            break;
                        }
                        else {
                            block_escaped(zone, candidateSubzone, (void *)candidate);
                        }
                    }
                }
            }
            start++;
        }
        if (dstStoredInto) {
            dstSubzone->set_scan_local_block(dstBase);
        }
    }
    
    void Thread::thread_cache_add(void *block) {
        Subzone *subzone = Subzone::subzone(block);
        usword_t n = subzone->length(block);
        subzone->admin()->mark_cached(block, n);
        FreeList &list = allocation_cache(n);
        list.push(block, (n << allocate_quantum_small_log2));
    }
    
    //
    // scan_current_thread
    //
    // Scan the current thread stack and registers for block references.
    //
    void Thread::scan_current_thread(MemoryScanner &scanner) {
        // capture non-volatile registers
        NonVolatileRegisters registers;
        
        // scan the registers
        Range range = registers.buffer_range();
        scanner.scan_range_from_registers(range, *this, 0);

        // scan the stack
        range.set_range(scanner.current_stack_bottom(), _stack_base);
        if (_stack_scan_peak > range.address()) {
            _stack_scan_peak = range.address();
        }
        scanner.scan_range_from_thread(range, *this);
        // Note:  Because this method is also called from TLC, it is NOT correct to call
        // scanner.scan_pending_until_done() here. That method can ONLY be called from the
        // background collector thread.
    }


    void Thread::scan_current_thread(void (^scanner) (Thread *thread, Range &range), void *stack_bottom) {
        // capture non-volatile registers
        NonVolatileRegisters registers;
        
        // scan the registers
        Range range = registers.buffer_range();
        scanner(this, range);

        // scan the stack
        range.set_range(stack_bottom, _stack_base);
        if (_stack_scan_peak > range.address()) {
            _stack_scan_peak = range.address();
        }
        scanner(this, range);
    }

    union ThreadState {
#if defined(__i386__)
        i386_thread_state_t  regs;
#define THREAD_STATE_COUNT i386_THREAD_STATE_COUNT
#define THREAD_STATE_FLAVOR i386_THREAD_STATE
#define THREAD_STATE_SP __esp
#elif defined(__ppc__)
        ppc_thread_state_t   regs;
#define THREAD_STATE_COUNT PPC_THREAD_STATE_COUNT
#define THREAD_STATE_FLAVOR PPC_THREAD_STATE
#define THREAD_STATE_SP __r1
#elif defined(__ppc64__)
        ppc_thread_state64_t regs;
#define THREAD_STATE_COUNT PPC_THREAD_STATE64_COUNT
#define THREAD_STATE_FLAVOR PPC_THREAD_STATE64
#define THREAD_STATE_SP __r1
#elif defined(__x86_64__)
        x86_thread_state64_t regs;
#define THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
#define THREAD_STATE_FLAVOR x86_THREAD_STATE64
#define THREAD_STATE_SP __rsp
#else
#error Unknown Architecture
#endif
        thread_state_data_t  data;
        
        void* get_stack_pointer() {
            // <rdar://problem/6453396> always align the stack address to a pointer boundary. 
            return align_down(reinterpret_cast<void*>(regs.THREAD_STATE_SP - C_RED_ZONE), pointer_alignment);
        }
    };
    

    //
    // get_register_state
    //
    // read another thread's registers
    //
    void Thread::get_register_state(ThreadState &state, unsigned &user_count) {
        // select the register capture flavor
        user_count = THREAD_STATE_COUNT;
        thread_state_flavor_t flavor = THREAD_STATE_FLAVOR;

        // get the thread register state
        kern_return_t err = thread_get_state(_thread, flavor, state.data, &user_count);
        uint64_t retryDelay = 1;
        
        // We sometimes see KERN_ABORTED in conjunction with fork(). Typically a single retry succeeds in that case.
        // We also see various other error codes during thread exit/teardown. Retry generously until the port is dead (MACH_SEND_INVALID_DEST)
        // because otherwise we have a fatal error. Using a logarithmically increasing delay between iterations, which
        // results in a TOTAL sleep time of 1.111111 seconds to let the dying thread settle before we give up.
        while ((err != KERN_SUCCESS) && (err == KERN_ABORTED && retryDelay < 10 * NSEC_PER_SEC)) {
            //malloc_printf("*** %s: unable to get thread state %d. Retrying (retry count: %d)\n", prelude(), err, retryCount);
            struct timespec sleeptime;
            sleeptime.tv_sec = retryDelay / NSEC_PER_SEC;
            sleeptime.tv_nsec = retryDelay % NSEC_PER_SEC;
            nanosleep(&sleeptime, NULL);
            retryDelay *= 10;
            err = thread_get_state(_thread, flavor, state.data, &user_count);
        }

        if (err) {
            // this is a fatal error. the program will crash if we can't scan this thread's state.
            char thread_description[256];
            description(thread_description, sizeof(thread_description));
            auto_fatal("get_register_state():  unable to get thread state:  err = %d, %s\n", err, thread_description);
        }
    }
    
    
    //
    // scan_other_thread
    //
    // Scan a thread other than the current thread stack and registers for block references.
    //
    void Thread::scan_other_thread(MemoryScanner &scanner, bool withSuspend) {
        // <rdar://problem/6398665&6456504> can only safely scan if this thread was locked.
        assert(_scanning.state);
        
        // suspend the thread while scanning its registers and stack.
        if (withSuspend) suspend();

        unsigned user_count;
        ThreadState state;
        get_register_state(state, user_count);
        
        // scan the registers
        Range range((void *)state.data, user_count * sizeof(natural_t));
        scanner.scan_range_from_registers(range, *this, 0);
        
        // scan the stack
        range.set_range(state.get_stack_pointer(), _stack_base);
        if (_stack_scan_peak > range.address()) {
            _stack_scan_peak = range.address();
        }
        scanner.scan_range_from_thread(range, *this);
        scanner.scan_pending_until_done();

        if (withSuspend) resume();
    }
    
    void Thread::scan_other_thread(void (^scanner) (Thread *, Range &), bool withSuspend) {
        // <rdar://problem/6398665&6456504> can only safely scan if this thread was locked.
        assert(_scanning.state);

        // suspend the thread while scanning its registers and stack.
        if (withSuspend) suspend();
        
        unsigned user_count;
        ThreadState state;
        get_register_state(state, user_count);

        // scan the registers
        Range range((void *)state.data, user_count * sizeof(natural_t));
        scanner(this, range);
        
        // scan the stack
        range.set_range(state.get_stack_pointer(), _stack_base);
        if (_stack_scan_peak > range.address()) {
            _stack_scan_peak = range.address();
        }
        scanner(this, range);
        
        if (withSuspend) resume();
    }


    //
    // suspend
    //
    // Temporarily suspend the thread from further execution.  Returns true if the thread is
    // still alive.
    //
    void Thread::suspend()  {
        // do not suspend this thread
        if (is_current_thread()) return;
        
        if (_suspended == 0) {
            // request thread suspension
            kern_return_t err = thread_suspend(_thread);
            
            if (err != KERN_SUCCESS) {
                char thread_description[256];
                description(thread_description, sizeof(thread_description));
                auto_fatal("Thread::suspend():  unable to suspend a thread:  err = %d, %s\n", err, thread_description);
            }
        }
        _suspended++;
    }


    //
    // resume
    //
    // Resume a suspended thread.
    //
    void Thread::resume() {
        // do not resume this thread
        if (is_current_thread()) return;

        if (_suspended == 1) {
            // request thread resumption
            kern_return_t err = thread_resume(_thread);
            
            if (err != KERN_SUCCESS) {
                char thread_description[256];
                description(thread_description, sizeof(thread_description));
                auto_fatal("Thread::resume():  unable to suspend a thread:  err = %d, %s\n", err, thread_description);
            }
        }
        _suspended--;
    }

    
    char *Thread::description(char *buf, size_t bufsz) {
        if (_pthread == NULL) {
            snprintf(buf, bufsz, "Thread %p: unbound", this);
        } else {
            snprintf(buf, bufsz, "Thread %p: _pthread = %p, _thread = 0x%x, _stack_base = %p, enlivening %s, %d local blocks",
                     this,_pthread, _thread, _stack_base,
                     _enlivening_queue.needs_enlivening().state ? " on" : "off", _localAllocations.count());
        }
        return buf;
    }


    extern "C" void auto_print_registered_threads() {
        Zone *zone = Zone::zone();
        Mutex lock(zone->threads_mutex());
        Thread *thread = zone->threads();
        while (thread != NULL) {
            char thread_description[256];
            thread->description(thread_description, sizeof(thread_description));
            malloc_printf("%s\n", thread_description);
            thread = thread->next();
        }
    }


    //
    // dump
    //
    void Thread::dump(auto_zone_stack_dump stack_dump, auto_zone_register_dump register_dump, auto_zone_node_dump dump_local_block) {
        Range stack_range;
        // dump the registers.
        if (!is_bound()) return;
        if (register_dump) {
            if (is_current_thread()) {
                NonVolatileRegisters registers;
                
                // scan the registers
                Range range = registers.buffer_range();
                //scanner.scan_range_from_registers(range, *this, 0);
                register_dump(range.address(), range.size());
                stack_range.set_range(__builtin_frame_address(0), _stack_base);
            }
            else {
                unsigned user_count;
                ThreadState state;
                get_register_state(state, user_count);
                register_dump(&state.data, user_count * sizeof(void *));
                stack_range.set_range(state.get_stack_pointer(), _stack_base);
            }
        }
        // dump the stack
        if (stack_dump) stack_dump(stack_range.address(), stack_range.size());
#if 0
unsafe; thread might be in the middle of an STL set grow; need to put new locks into a tracing build to get this info safely
        // dump the locals
        if (!dump_local_block) return;
        for (uint32_t i=_localAllocations.firstOccupiedSlot(); i<=_localAllocations.lastOccupiedSlot(); i++) {
            void *block = _localAllocations[i];
            if (block) {
                Subzone *subzone = Subzone::subzone(block);
                dump_local_block(block, subzone->size(block), subzone->layout(block), subzone->refcount(block));
            }
        }
#endif
    }
};
