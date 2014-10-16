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
    Thread.cpp
    Registered Thread Management
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#include "Definitions.h"
#include "Thread.h"
#include "Zone.h"
#include "ThreadLocalCollector.h"
#include "BlockIterator.h"
#include <crt_externs.h>

#if defined(__i386__) || defined(__arm__)
// 32-bit x86/arm use no red zone.
#define C_RED_ZONE 0
#elif defined(__x86_64__)
// according to  http://www.x86-64.org/documentation/abi.pdf (page 15)
#define C_RED_ZONE 128
#else
#error Unknown Architecture
#endif


namespace Auto {
    

    //----- Thread -----//

    Thread::Thread(Zone *zone)
        : _next(NULL), _zone(zone), _pthread(NULL), _thread(MACH_PORT_NULL), _stack_base(NULL),
          _scanning(), _suspended(0), _stack_scan_peak(NULL), _tlc(NULL), _localAllocations(64), _localsGuard(SENTINEL_T_INITIALIZER),
          _destructor_count(0), _in_collector(false), _tlc_watchdog_counter(0), _pending_count_accumulator(NULL)
    {
        bind();
    }
    
    void Thread::flush_cache(AllocationCache &cache) {
        usword_t count = 0, size = 0;
        for (usword_t i = 1; i < AllocationCache::cache_size; ++i) {
            FreeList &list = cache[i];
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
        _zone->adjust_allocation_counter(size);
    }
    
    Thread::~Thread() {
        /* If any blocks remain in our local allocations list mark them as global. */
        /* We cannot reclaim them because we cannot finalize here. */
        if (_localAllocations.count() > 0) {
            for (uint32_t i=_localAllocations.firstOccupiedSlot(); i<=_localAllocations.lastOccupiedSlot(); i++) {
                void *block = _localAllocations[i];
                if (block) {
                    Subzone *subzone = Subzone::subzone(block);
                    subzone->make_global(subzone->quantum_index_unchecked(block));
                }
            }
        }
        
        // release the per-thread allocation cache items
        flush_cache(_allocation_cache[AUTO_MEMORY_SCANNED]);
        flush_cache(_allocation_cache[AUTO_MEMORY_UNSCANNED]);
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
        // The Kernel stores stores the environment, and command line arguments on the main thread stack.
        // Skip that to avoid false rooting from the character data.
        _stack_base = pthread_main_np() ? align_down(**(void***)_NSGetArgv(), pointer_alignment) : pthread_get_stackaddr_np(_pthread);
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
        return true;
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
    

    struct enliven_do {
        void operator ()(Subzone *subzone, usword_t q) {
            if (!subzone->test_and_set_mark(q) && subzone->is_scanned(q))
                subzone->test_and_set_pending(q, true);
        }
        
        void operator ()(Large *large) {
            if (!large->test_and_set_mark() && large->is_scanned())
                large->set_pending();
        }
    };
    
    void Thread::enliven_block(void *block) {
        enliven_do op;
        blockDo(_zone, block, op);
    }
    
    
    //
    // flush_local_blocks
    //
    // empties the local allocations hash, making all blocks global
    //
    void Thread::flush_local_blocks()
    {
        Sentinel::assert_guarded(_localsGuard);
        // This only gets called if the local block set grows much larger than expected.
        uint32_t first = _localAllocations.firstOccupiedSlot();
        uint32_t last = _localAllocations.lastOccupiedSlot();
        for (uint32_t i = first; i <= last; i++) {
            void *block = _localAllocations[i];
            if (block) {
                Subzone *subzone = Subzone::subzone(block);
                subzone->make_global(subzone->quantum_index(block));
                _localAllocations.remove(i);
            }
        }
        // this will cause _localAllocations to resize down its internal pointer buffer
        _localAllocations.grow();
    }

    
    //
    // reap_local_blocks
    //
    // finalize and free all local blocks without doing any scanning
    // should only be called when it is known the stack is shallow and cannot root anything
    //
    void Thread::reap_all_local_blocks()
    {
        Sentinel guard(_localsGuard);
        if (_localAllocations.count() > 0) {
            ThreadLocalCollector tlc(_zone, NULL, *this);
            tlc.reap_all();
            // this will cause _localAllocations to resize down its internal pointer buffer
            _localAllocations.grow();
        }
    }

    
    // BlockRef FIXME: temporary glue code until all call sites convert to BlockRef.
    template <> void Thread::block_escaped<void *>(void *block) {
        Subzone *subzone;
        if (!_zone->in_subzone_memory(block))
            return;
        subzone = Subzone::subzone(block);
        usword_t q;
        if (!subzone->block_is_start(block, &q)) return; // we are not interested in non-block pointers
        SubzoneBlockRef ref(subzone, q);
        if (ref.is_thread_local()) block_escaped(ref);
    }

    //
    // block_escaped
    //
    // a block is escaping the stack; remove it from local set (cheaply)
    //
    template <class BlockRef> void Thread::block_escaped_internal(BlockRef block)
    {
        assert(block.is_thread_local());
        void *addr = block.address();
        /*
         It is possible that a thread might construct a pointer to a block which is local to another thread.
         If that pointer gets stored through a write barrier then we wind up here.
         It would be an error for the thread to dereference that pointer, but just storing it is technically ok.
         We must be careful to validate that the block is local to *this* thread.
         */             
        if (auto_expect_false(block.is_local_garbage())) {
            /*
             If we see a local garbage block we must first ensure that it is local to the current thread.
             If it is then we must evict any non-garbage blocks which are reachable from that block.
             However, we don't currently have a way to discover when one thread local garbage block is
             reachable from another thread local garbage block. The scanner is not equipped to handle that.
             So we just escape all blocks reachable from the entire garbage list. This should be very rare.
             Note that the garbage blocks themselves remain thread local garbage. Only reachable non-garbage
             blocks are made global.
             */
            
            // verify the block is in this thread's garbage list
            if (_tlc && _tlc->block_in_garbage_list(addr)) {
                _tlc->evict_local_garbage();
            }
        } else {
            Sentinel guard(_localsGuard);
            // verify the block is local to this thread
            if (_localAllocations.contains(addr)) {
                if (block.should_scan_local_block()) {
                    ThreadLocalCollector scanner(_zone, NULL, *this);
                    scanner.eject_local_block(addr);
                }
                else {	// just do the one
                    block.make_global();
                    _localAllocations.remove(addr);
                    usword_t size = block.size();
                    _zone->adjust_allocation_counter(size);
                }
            }
        }
    }
    
#ifdef DEBUG
    // In release builds the optimizer knows this never gets called. But we need it to link a debug build.
    template <> void Thread::block_escaped_internal<class LargeBlockRef>(LargeBlockRef block) {
        __builtin_trap();
    }
#endif
    
    //
    // track_local_memcopy
    //
    // If dst is contained in a local scanned object, then if src is also scanned and has
    // local objects that they, in turn, are marked stored as well as dest
    // Otherwise, if dst is unknown, mark all local objects at src as escaped.
    // Src might be the stack.
    void Thread::track_local_memcopy(const void *src, void *dst, size_t size) {
        Subzone *dstSubzone = NULL;
        void *dstBase = NULL;
        bool should_track_local = false;
        if (_zone->in_subzone_memory((void *)dst)) {
            dstSubzone = Subzone::subzone((void *)dst);
            usword_t dst_q;
            dstBase = dstSubzone->block_start((void *)dst, dst_q);
            if (dstBase) {
                // if memmoving within block bail early
                size_t dstSize = dstSubzone->size(dst_q);
                if (src > dstBase && src < ((char *)dstBase + dstSize))
                    return;
                if (dstSubzone->is_live_thread_local(dst_q)
                    && (dstSubzone->should_scan_local_block(dst_q) || dstSubzone->is_scanned(dst_q))) {
                    should_track_local = true;
                }
            }
        }
        void **start = (void **)src;
        void **end = start + size/sizeof(void *);
        bool dstStoredInto = false;
        while (start < end) {
            void *candidate = *start;
            if (candidate) {
                if (_zone->in_subzone_memory(candidate)) {
                    Subzone *candidateSubzone = Subzone::subzone(candidate);
                    usword_t q = candidateSubzone->quantum_index_unchecked(candidate);
                    if (q < candidateSubzone->allocation_limit() && candidateSubzone->is_live_thread_local(q)) {// && thread->_localAllocations.contains(candidate))
                        if (should_track_local) {
                            dstStoredInto = true;
                            break;
                        }
                        else {
                            SubzoneBlockRef candidateRef(candidateSubzone, q);
                            block_escaped(candidateRef);
                        }
                    }
                }
            }
            start++;
        }
        if (dstStoredInto) {
            // we can only get here if dstBase is a valid block
            dstSubzone->set_scan_local_block(dstSubzone->quantum_index_unchecked(dstBase));
        }
    }
    
    bool Thread::thread_cache_add(void *block, Subzone *subzone, usword_t q) {
        // don't cache medium subzone blocks.
        bool cached = false;
        if (subzone->is_small()) {
            usword_t n = subzone->length(q);
            if (n <= max_cached_small_multiple) {
                Admin *admin = subzone->admin();
                admin->mark_cached(subzone, q, n);
                FreeList &list = allocation_cache(admin->layout())[n];
                list.push(block, (n << allocate_quantum_small_log2));
                cached = true;
            }
        }
        return cached;
    }
    
    //
    // scan_current_thread
    //
    // Scan the current thread stack and registers for block references.
    //
    void Thread::scan_current_thread(thread_scanner_t scanner, void *stack_bottom) {
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

#ifndef __BLOCKS__
    class thread_scanner_helper : public Thread::thread_scanner {
        void (*_scanner) (Thread*, const Range&, void*);
        void *_arg;
    public:
        thread_scanner_helper(void (*scanner) (Thread*, const Range&, void*), void *arg) : _scanner(scanner), _arg(arg) {}
        virtual void operator() (Thread *thread, const Range &range) { _scanner(thread, range, _arg); }
    };
#endif

    void Thread::scan_current_thread(void (*scanner) (Thread*, const Range&, void*), void *arg, void *stack_bottom) {
#ifdef __BLOCKS__
        scan_current_thread(^(Thread *thread, const Range &range) { scanner(thread, range, arg); }, stack_bottom);
#else
        thread_scanner_helper helper(scanner, arg);
        scan_current_thread(helper, stack_bottom);
#endif
    }

    union ThreadState {
#if defined(__i386__)
        i386_thread_state_t  regs;
#define THREAD_STATE_COUNT i386_THREAD_STATE_COUNT
#define THREAD_STATE_FLAVOR i386_THREAD_STATE
#define THREAD_STATE_SP __esp
#elif defined(__x86_64__)
        x86_thread_state64_t regs;
#define THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
#define THREAD_STATE_FLAVOR x86_THREAD_STATE64
#define THREAD_STATE_SP __rsp
#elif defined(__arm__)
        arm_thread_state_t regs;
#define THREAD_STATE_COUNT ARM_THREAD_STATE_COUNT
#define THREAD_STATE_FLAVOR ARM_THREAD_STATE
#define THREAD_STATE_SP __sp
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
    void Thread::scan_other_thread(thread_scanner_t scanner, bool withSuspend) {
        // <rdar://problem/6398665&6456504> can only safely scan if this thread was locked.
        assert(_scanning.state);

        // suspend the thread while scanning its registers and stack.
        if (withSuspend) suspend();
        
        unsigned user_count;
        ThreadState state;
        get_register_state(state, user_count);

        // scan the registers
        Range register_range((void *)state.data, user_count * sizeof(natural_t));
        scanner(this, register_range);
        
        // scan the stack
        Range stack_range(state.get_stack_pointer(), _stack_base);
        if (_stack_scan_peak > stack_range.address()) {
            _stack_scan_peak = stack_range.address();
        }
        scanner(this, stack_range);
                
        if (withSuspend) {
            if (ThreadLocalCollector::should_collect_suspended(*this)) {
                // Perform a TLC and pull the resulting garbage list into global garbage
                ThreadLocalCollector tlc(_zone, state.get_stack_pointer(), *this);
                // Blocks in the garbage list have already been marked by the roots scan.
                // Since these blocks are known to be garbage, explicitly unmark them now to collect them in this cycle.
                tlc.collect_suspended(register_range, stack_range);
            }
            resume();
        }
    }

    void Thread::scan_other_thread(void (*scanner) (Thread*, const Range&, void*), void *arg, bool withSuspend) {
#ifdef __BLOCKS__
        scan_other_thread(^(Thread *thread, const Range &range) { scanner(thread, range, arg); }, withSuspend);
#else
        thread_scanner_helper helper(scanner, arg);
        scan_other_thread(helper, withSuspend);
#endif
    }

    //
    // suspend
    //
    // Temporarily suspend the thread from further execution.  Returns true if the thread is
    // still alive.
    //
    void Thread::suspend()  {
        // do not suspend this thread
        if (is_current_thread() || !is_bound()) return;
        
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
        if (is_current_thread() || !is_bound()) return;

        if (_suspended == 1) {
            // request thread resumption
            kern_return_t err = thread_resume(_thread);
            
            if (err != KERN_SUCCESS) {
                char thread_description[256];
                description(thread_description, sizeof(thread_description));
                auto_fatal("Thread::resume():  unable to resume a thread:  err = %d, %s\n", err, thread_description);
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
                     needs_enlivening().state ? " on" : "off", _localAllocations.count());
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


#ifdef __BLOCKS__
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
    
    void Thread::visit(auto_zone_visitor_t *visitor) {
        // dump the registers.
        if (!is_bound()) return;
        if (is_current_thread()) {
            // snapshot the stack range.
            auto_address_range_t stack_range = { (void *)auto_get_sp(), _stack_base };
            
            // snapshot the registers.
            NonVolatileRegisters registers;
            Range range = registers.buffer_range();
            auto_address_range_t registers_range = { range.address(), range.end() };
            visitor->visit_thread(_pthread, stack_range, registers_range);
        } else {
            unsigned user_count;
            ThreadState state;
            get_register_state(state, user_count);
            auto_address_range_t stack_range = { state.get_stack_pointer(), _stack_base };
            auto_address_range_t registers_range = { &state.data, &state.data[user_count] };
            visitor->visit_thread(_pthread, stack_range, registers_range);
        }
    }
    
#endif /* __BLOCKS__ */

};
