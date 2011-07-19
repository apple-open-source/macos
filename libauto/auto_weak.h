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
    auto_weak.h
    Weak reference accounting
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#ifndef __AUTO_WEAK__
#define __AUTO_WEAK__

#define AUTO_USE_NEW_WEAK_CALLBACK
#include "auto_impl_utilities.h"

__BEGIN_DECLS

namespace Auto {
    class Zone;
}

/*
The weak table is a hash table governed by a single spin lock.
An allocated blob of memory, most often an object, but under GC any such allocation,
may have its address stored in a __weak marked storage location through use of
compiler generated write-barriers or hand coded uses of the register weak primitive.
Associated with the registration can be a callback block for the case when one of
 the allocated chunks of memory is reclaimed.
The table is hashed on the address of the allocated memory.  When __weak marked memory
 changes its reference, we count on the fact that we can still see its previous reference.

So, in the hash table, indexed by the weakly referenced item, is a list of all locations
 where this address is currently being stored.
 
*/

struct weak_referrer_t {
    void **referrer;    // clear this address
    auto_weak_callback_block_t *block;
};
typedef struct weak_referrer_t weak_referrer_t;

// clear references to garbage
extern auto_weak_callback_block_t *weak_clear_references(Auto::Zone *azone, size_t garbage_count, vm_address_t *garbage, uintptr_t *weak_referents_count, uintptr_t *weak_refs_count);

// register a new weak reference
extern void weak_register(Auto::Zone *azone, const void *referent, void **referrer, auto_weak_callback_block_t *block);

// unregister an existing weak reference
extern void weak_unregister(Auto::Zone *azone, const void *referent, void **referrer);

// unregister all weak references from a block.
extern void weak_unregister_with_layout(Auto::Zone *azone, void *block[], const unsigned char *map);

// unregister weak references in a block range.
extern void weak_unregister_range(Auto::Zone *azone, void **referrers, size_t count);

// call all registered weak reference callbacks.
extern void weak_call_callbacks(auto_weak_callback_block_t *block);

// unregister all weak references within a known address range.
extern void weak_unregister_data_segment(Auto::Zone *azone, void *base, size_t size);

// NOTE:  the remaining routines all assume the weak lock is held by the caller.

// forwards weak references from oldReferent to newReferent.
extern void weak_transfer_weak_referents(Auto::Zone *azone, const void *oldReferent, const void *newReferent);
extern void weak_transfer_weak_contents(Auto::Zone *azone, void *oldBlock[], void *newBlock[], const uint8_t *map);
extern void weak_transfer_weak_contents_unscanned(Auto::Zone *azone, void *oldBlock[], void *newBlock[], size_t size, bool forwarding);

// unregister weak references in a block range.
extern void weak_unregister_range_no_lock(Auto::Zone *azone, void **referrers, size_t count);

#ifdef __BLOCKS__

typedef void (^weak_ref_visitor_t) (const weak_referrer_t &ref);

// dump just the weak references to this block.
extern void weak_enumerate_weak_references(Auto::Zone *azone, const void *referent, weak_ref_visitor_t visitor);

// dump all weak registrations
extern void weak_enumerate_table(Auto::Zone *azone, weak_ref_visitor_t visitor);

// fixup all weak registrations (compaction)
typedef void (^weak_ref_fixer_t) (weak_referrer_t &ref);
extern void weak_enumerate_table_fixup(Auto::Zone *azone, weak_ref_fixer_t fixer);

#endif

__END_DECLS

#endif /* __AUTO_WEAK__ */
