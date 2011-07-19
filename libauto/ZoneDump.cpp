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
    ZoneDump.cpp
    Zone Dumping
    Copyright (c) 2009-2011 Apple Inc. All rights reserved.
 */
 
#include "Admin.h"
#include "Bitmap.h"
#include "BlockIterator.h"
#include "Configuration.h"
#include "Definitions.h"
#include "Environment.h"
#include "Large.h"
#include "Locks.h"
#include "Range.h"
#include "Region.h"
#include "Statistics.h"
#include "Subzone.h"
#include "Thread.h"
#include "WriteBarrierIterator.h"
#include "ThreadLocalCollector.h"
#include "Zone.h"

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
            void *block = subzone->quantum_address(q);
            SubzoneBlockRef ref(subzone, q);
            node_dump(block, subzone->size(q), subzone->layout(q), ref.refcount());
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

    void Zone::dump_zone(
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
            Mutex lock(&_roots_lock);
            PtrHashSet::iterator i = _roots.begin();
            while (i != _roots.end()) {
                root_dump((const void **)*i);
                i++;
            }
        }
        
        
        // for all weak
        if (weak_dump_entry) {
            SpinLock lock(&weak_refs_table_lock);
            weak_enumerate_table(this, ^(const weak_referrer_t &ref) { weak_dump_entry((const void **)ref.referrer, *ref.referrer); });
        }
        
        // resume threads

        thread = threads();
        while (thread != NULL) {
            if (!thread->is_current_thread() && thread->is_bound()) thread->resume();
            thread = thread->next();
        }
    }
    
    struct allocated_blocks_visitor {
        auto_zone_visitor_t *_visitor;
        
        // Constructor
        allocated_blocks_visitor(auto_zone_visitor_t *visitor) : _visitor(visitor) {}
        
        // visitor function for subzone
        inline bool visit(Zone *zone, Subzone *subzone, usword_t q) {
            // send single block information
            void *block = subzone->quantum_address(q);
            SubzoneBlockRef ref(subzone, q);
            _visitor->visit_node(block, subzone->size(q), subzone->layout(q), ref.refcount(), subzone->is_thread_local(q));
            // always continue
            return true;
        }
        
        // visitor function for large
        inline bool visit(Zone *zone, Large *large) {
            // send single block information
            _visitor->visit_node(large->address(), large->size(), large->layout(), large->refcount(), false);
            // always continue
            return true;
        }
    };

    
    void Zone::visit_zone(auto_zone_visitor_t *visitor) {
        // Lock out new threads and suspend all others.
        // This can deadlock if called from gdb with other threads suspended
        suspend_all_registered_threads();

        // for all threads
        if (visitor->visit_thread) {
            scan_registered_threads(^(Thread *thread) { thread->visit(visitor); });
        }
        
        // for all nodes
        if (visitor->visit_node) {
            allocated_blocks_visitor ab_visitor(visitor);
            visitAllocatedBlocks(this, ab_visitor);
        }

        // for all roots
        if (visitor->visit_root) {
            Mutex lock(&_roots_lock);
            for (PtrHashSet::iterator i = _roots.begin(), end = _roots.end(); i != end; i++) {
                visitor->visit_root((const void **)*i);
            }
        }
        
        // for all weak
        if (visitor->visit_weak) {
            SpinLock lock(&weak_refs_table_lock);
            weak_enumerate_table(this, ^(const weak_referrer_t &ref) {
                visitor->visit_weak(*ref.referrer, ref.referrer, ref.block);
            });
        }
        
        // for all associations
        if (visitor->visit_association) {
            ReadLock lock(&_associations_lock);
            for (AssociationsHashMap::iterator i = _associations.begin(), iend = _associations.end(); i != iend; i++) {
                void *block = i->first;
                ObjectAssociationMap *refs = i->second;
                for (ObjectAssociationMap::iterator j = refs->begin(), jend = refs->end(); j != jend; j++) {
                    ObjectAssociationMap::value_type &pair = *j;
                    visitor->visit_association(block, pair.first, pair.second);
                }
            }
        }
        
        // resume threads
        resume_all_registered_threads();
    }
};
