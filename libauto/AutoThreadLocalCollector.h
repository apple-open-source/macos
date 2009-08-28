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
 *  AutoThreadLocalCollector.h
 *  auto
 *
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_THREAD_LOCAL_COLLECTOR__
#define __AUTO_THREAD_LOCAL_COLLECTOR__

#include "AutoDefs.h"
#include "AutoRange.h"
#include "AutoThreadLocalScanner.h"
#include "AutoSubZone.h"
#include "AutoZone.h"


namespace Auto {

    //----- Collector -----//
    
    //
    // Responsible for garbage collection.
    //
    

    class ThreadLocalCollector : public ThreadLocalScanner {

    private:
        Thread &_thread;
    
        void scan_local_block(void *startingBlock);
        void scan_marked_blocks();
        void scavenge_local(size_t count, vm_address_t garbage[]);
        void evict_local_garbage(size_t count, vm_address_t garbage[]);
        void process_local_garbage(bool finalizeNow);
        
    public:
    
        ThreadLocalCollector(Zone *zone, void *current_stack_bottom, Thread &thread) : ThreadLocalScanner(zone, current_stack_bottom, thread.locals()), _thread(thread) {}

        //
        // should_collect
        //
        static bool should_collect(Zone *zone, Thread &thread, bool canFinalizeNow);
                
        //
        // collect
        //
        void collect(bool finalizeNow);
        
        //
        // eject_local_block
        //
        // removes block and all referenced stack local blocks
        //
        void eject_local_block(void *startingBlock);
    };
};

#endif
