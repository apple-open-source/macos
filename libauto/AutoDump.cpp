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
    AutoDump.cpp
    Copyright (c) 2009 Apple Inc. All rights reserved.
 */
 
#include "AutoAdmin.h"
#include "AutoBitmap.h"
#include "AutoBlockIterator.h"
#include "AutoCollector.h"
#include "AutoConfiguration.h"
#include "AutoDefs.h"
#include "AutoEnvironment.h"
#include "AutoLarge.h"
#include "AutoLock.h"
#include "AutoRange.h"
#include "AutoRegion.h"
#include "AutoStatistics.h"
#include "AutoSubzone.h"
#include "AutoMemoryScanner.h"
#include "AutoThread.h"
#include "AutoWriteBarrierIterator.h"
#include "AutoThreadLocalCollector.h"
#include "AutoZone.h"

#include "auto_weak.h"
#include "auto_trace.h"
 
namespace Auto {

    struct dump_all_blocks_visitor {
        void (^node_dump)(const void *address, unsigned long size, unsigned int layout, unsigned long refcount);
        
        // Constructor
        dump_all_blocks_visitor(void) {}
        
        // visitor function for subzone
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            // send single block information
            node_dump(subzone->quantum_address(q), subzone->size(q), subzone->layout(q), subzone->refcount(q));
            // always continue
            return true;
        }
        
        // visitor function for large
        inline bool visit(Zone *zone, Large *large) {
            // send single block information
            node_dump(large->address(), large->size(), large->layout(), large->refcount());
            // always continue
            return true;
        }
    };

    void Zone::dump(
            auto_zone_stack_dump stack_dump,
            auto_zone_register_dump register_dump,
            auto_zone_node_dump thread_local_node_dump,  // unused
            auto_zone_root_dump root_dump,
            auto_zone_node_dump global_node_dump,
            auto_zone_weak_dump weak_dump_entry)
    {
        // Lock out new threads and suspend all others.
        // We don't take many locks nor are we dependent on anything (much) that requires a lock.
        // We don't take, for example, the large lock and are willing to miss a brand new one.
        // We don't take, for example, the regions lock, and are willing to miss a new empty region.
        // We don't take, for example, the refcounts lock, and are willing to provide an inexact refcount.
        // We don't take, for example, the admin locks, and will miss a not-quite-born object.
        
        // XXX need associative refs, too
        
        // XXX_PCB:  grab the thread list mutex, so newly registered threads can't interfere.
        // This can deadlock if called from gdb with other threads suspended
        Mutex lock(&_registered_threads_mutex);
        
        // suspend all threads...
        Thread *thread = threads();
        while (thread != NULL) {
            if (!thread->is_current_thread() && thread->is_bound()) {
                thread->suspend();
            }
            thread = thread->next();
        }
        
        // for all nodes
        dump_all_blocks_visitor visitor;
        visitor.node_dump = global_node_dump;
        visitAllocatedBlocks(this, visitor);

        thread = threads();
        while (thread != NULL) {
            thread->dump(stack_dump, register_dump, thread_local_node_dump);
            thread = thread->next();
        }
        
        // for all roots
        if (root_dump) {
            SpinLock lock(&_roots_lock);
            PtrHashSet::iterator i = _roots.begin();
            while (i != _roots.end()) {
                root_dump((const void **)*i);
                i++;
            }
        }
        
        
        // for all weak
        if (weak_dump_entry) {
            SpinLock lock(&weak_refs_table_lock);
            weak_dump_table(this, weak_dump_entry);
        }
        
        // resume threads

        thread = threads();
        while (thread != NULL) {
            if (!thread->is_current_thread() && thread->is_bound()) thread->resume();
            thread = thread->next();
        }
    }
};
