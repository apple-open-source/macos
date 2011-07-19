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
    ThreadLocalCollector.h
    Thread Local Collector
    Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_THREAD_LOCAL_COLLECTOR__
#define __AUTO_THREAD_LOCAL_COLLECTOR__

#include "Definitions.h"
#include "Range.h"
#include "SubZone.h"
#include "Zone.h"

namespace Auto {

    //----- Collector -----//
    
    //
    // Responsible for garbage collection.
    //
    
    class ThreadLocalCollector {
    public:
        
    private:
        Zone *_zone;
        Range _coverage;
        void *_stack_bottom;
        Sentinel _localsGuard;
        LocalBlocksHash &_localBlocks;
        void **_tlcBuffer; // local_allocations_size_limit entries, backing lives in Thread. Used for several purposes in TLC.
        size_t _tlcBufferCount;
        Thread &_thread;

        PtrHashSet *_zombies;
        
        void scan_stack_range(const Range &range);
        void mark_push_block(void *block);
        void scan_range(const Range &range);
        void scan_with_layout(const Range &range, const unsigned char* map);
        void scan_local_block(Subzone *subzone, usword_t q, void *block);
        void scan_pending_until_done();
        void scan_marked_blocks();
        void scavenge_local(size_t count, void *garbage[]);
        void process_local_garbage(void (*garbage_list_handler)(ThreadLocalCollector *));
        
        friend class thread_local_scanner_helper;
        
        static void finalize_local_garbage_now(ThreadLocalCollector *tlc);
        static void finalize_local_garbage_later(ThreadLocalCollector *tlc);
        static void unmark_local_garbage(ThreadLocalCollector *tlc);
        static void mark_local_garbage(void **garbage_list, size_t garbage_count);
        void trace_scanning_phase_end();
        void append_block(void *block);

    public:
    
        ThreadLocalCollector(Zone *zone, void *current_stack_bottom, Thread &thread)
            : _zone(zone), _coverage(zone->coverage()), _stack_bottom(current_stack_bottom), _localsGuard(thread.localsGuard()), _localBlocks(thread.locals()),
        _tlcBuffer(thread.tlc_buffer()), _tlcBufferCount(0), _thread(thread), _zombies(NULL)
        {
        }

        ~ThreadLocalCollector()
        {
            if (_zombies)
                delete _zombies;
        }
        
        //
        // should_collect
        //
        static bool should_collect(Zone *zone, Thread &thread, bool canFinalizeNow);
                
        //
        // should_collect_suspended
        //
        static bool should_collect_suspended(Thread &thread);
        
        //
        // collect
        //
        void collect(bool finalizeNow);
        
        //
        // collect_suspended
        //
        void collect_suspended(Range &registers, Range &stack);

        //
        // reap_all
        //
        void reap_all();
        
        //
        // eject_local_block
        //
        // removes block and all referenced stack local blocks
        //
        void eject_local_block(void *startingBlock);
        
        //
        // block_in_garbage_list
        //
        // searches the garbage list and returns true if block is in it
        bool block_in_garbage_list(void *block);

        //
        // evict_local_garbage
        //
        // scans the list of garbage blocks. any non-garbage local blocks which are reachable are made global
        void evict_local_garbage();
        
        //
        // add_zombie
        //
        // adds block as a thread local zombie
        void add_zombie(void *block);
        
        //
        // is_zombie
        //
        // test if a block is a thread local zombie
        inline bool is_zombie(void *block);
    };
};

#endif
