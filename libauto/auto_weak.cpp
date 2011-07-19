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
    auto_weak.cpp
    Weak reference accounting
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#include "auto_weak.h"
#include "Locks.h"
#include "Definitions.h"
#include "Range.h"
#include "Zone.h"

using namespace Auto;

struct weak_referrer_array_t {
    weak_referrer_t     *refs;
    usword_t            num_refs;
    usword_t            num_allocated;
    usword_t            max_hash_displacement;
};
typedef struct weak_referrer_array_t weak_referrer_array_t;

struct Auto::weak_entry_t {
    const void *referent;
    weak_referrer_array_t referrers;
};
typedef struct Auto::weak_entry_t weak_entry_t;

typedef std::pair<const void *, void **> WeakPair;
typedef std::vector<WeakPair, Auto::AuxAllocator<WeakPair> > WeakPairVector;
typedef std::vector<weak_referrer_t, Auto::AuxAllocator<WeakPair> > WeakReferrerVector;

static void append_referrer_no_lock(weak_referrer_array_t *list, void **new_referrer,  auto_weak_callback_block_t *new_block);

static inline uintptr_t hash(const void *key) 
{
    uintptr_t k = (uintptr_t)key;

    // Code from CFSet.c
#if __LP64__
    uintptr_t a = 0x4368726973746F70ULL;
    uintptr_t b = 0x686572204B616E65ULL;
#else
    uintptr_t a = 0x4B616E65UL;
    uintptr_t b = 0x4B616E65UL; 
#endif
    uintptr_t c = 1;
    a += k;
#if __LP64__
    a -= b; a -= c; a ^= (c >> 43);
    b -= c; b -= a; b ^= (a << 9);
    c -= a; c -= b; c ^= (b >> 8);
    a -= b; a -= c; a ^= (c >> 38);
    b -= c; b -= a; b ^= (a << 23);
    c -= a; c -= b; c ^= (b >> 5);
    a -= b; a -= c; a ^= (c >> 35);
    b -= c; b -= a; b ^= (a << 49);
    c -= a; c -= b; c ^= (b >> 11);
    a -= b; a -= c; a ^= (c >> 12);
    b -= c; b -= a; b ^= (a << 18);
    c -= a; c -= b; c ^= (b >> 22);
#else
    a -= b; a -= c; a ^= (c >> 13);
    b -= c; b -= a; b ^= (a << 8);
    c -= a; c -= b; c ^= (b >> 13);
    a -= b; a -= c; a ^= (c >> 12);
    b -= c; b -= a; b ^= (a << 16);
    c -= a; c -= b; c ^= (b >> 5);
    a -= b; a -= c; a ^= (c >> 3);
    b -= c; b -= a; b ^= (a << 10);
    c -= a; c -= b; c ^= (b >> 15);
#endif
    return c;
}

// Up until this size the weak referrer array grows one slot at a time. Above this size it grows by doubling.
#define WEAK_TABLE_DOUBLE_SIZE 8

// Grow the refs list. Rehashes the entries.
static void grow_refs(weak_referrer_array_t *list)
{
    usword_t old_num_allocated = list->num_allocated;
    usword_t num_refs = list->num_refs;
    weak_referrer_t *old_refs = list->refs;
    usword_t new_allocated = old_num_allocated < WEAK_TABLE_DOUBLE_SIZE ? old_num_allocated + 1 : old_num_allocated + old_num_allocated;
    list->refs = (weak_referrer_t *)aux_malloc(new_allocated * sizeof(weak_referrer_t));
    list->num_allocated = aux_malloc_size(list->refs)/sizeof(weak_referrer_t);
    bzero(list->refs, list->num_allocated * sizeof(weak_referrer_t));
    // for larger tables drop one entry from the end to give an odd number of hash buckets for better hashing
    if ((list->num_allocated > WEAK_TABLE_DOUBLE_SIZE) && !(list->num_allocated & 1)) list->num_allocated--;
    list->num_refs = 0;
    list->max_hash_displacement = 0;
    
    usword_t i;
    for (i=0; i < old_num_allocated && num_refs > 0; i++) {
        if (old_refs[i].referrer != NULL) {
            append_referrer_no_lock(list, old_refs[i].referrer, old_refs[i].block);
            num_refs--;
        }
    }
    if (old_refs)
        aux_free(old_refs);
}

// Add the given referrer to list
// Does not perform duplicate checking.
static void append_referrer_no_lock(weak_referrer_array_t *list, void **new_referrer,  auto_weak_callback_block_t *new_block)
{
    if ((list->num_refs == list->num_allocated) || ((list->num_refs >= WEAK_TABLE_DOUBLE_SIZE) && (list->num_refs >= list->num_allocated * 2 / 3))) {
        grow_refs(list);
    }
    usword_t index = hash(new_referrer) % list->num_allocated, hash_displacement = 0;
    while (list->refs[index].referrer != NULL) {
        index++;
        hash_displacement++;
        if (index == list->num_allocated)
            index = 0;
    }
    if (list->max_hash_displacement < hash_displacement) {
        list->max_hash_displacement = hash_displacement;
        //malloc_printf("max_hash_displacement: %d allocated: %d\n", list->max_hash_displacement, list->num_allocated);
    }
    weak_referrer_t &ref = list->refs[index];
    ref.referrer = new_referrer;
    ref.block = new_block;
    list->num_refs++;
}


// Remove old_referrer from list, if it's present.
// Does not remove duplicates.
// fixme this is slow if old_referrer is not present.
static void remove_referrer_no_lock(weak_referrer_array_t *list, void **old_referrer)
{
    usword_t index = hash(old_referrer) % list->num_allocated;
    usword_t start_index = index, hash_displacement = 0;
    while (list->refs[index].referrer != old_referrer) {
        index++;
        hash_displacement++;
        if (index == list->num_allocated)
            index = 0;
        if (index == start_index || hash_displacement > list->max_hash_displacement) {
            malloc_printf("%s: attempted to remove unregistered weak referrer %p\n", auto_prelude(), old_referrer);
            return;
        }
    }
    list->refs[index].referrer = NULL;
    list->num_refs--;
}


// Add new_entry to the zone's table of weak references.
// Does not check whether the referent is already in the table.
// Does not update num_weak_refs.
static void weak_entry_insert_no_lock(Zone *azone, weak_entry_t *new_entry)
{
    weak_entry_t *table = azone->weak_refs_table;

    if (!table) { malloc_printf("no auto weak ref table!\n"); return; }

    usword_t table_size = azone->max_weak_refs;
    usword_t hash_index = hash(new_entry->referent) % table_size;
    usword_t index = hash_index;

    do {
        weak_entry_t *entry = table + index;
        if (entry->referent == NULL) {
            *entry = *new_entry;
            return;
        }
        index++; if (index == table_size) index = 0;
    } while (index != hash_index);
    malloc_printf("no room for new entry in auto weak ref table!\n");
}


// Remove entry from the zone's table of weak references, and rehash
// Does not update num_weak_refs.
static void weak_entry_remove_no_lock(Zone *azone, weak_entry_t *entry)
{
    // remove entry
    entry->referent = NULL;
    if (entry->referrers.refs) aux_free(entry->referrers.refs);
    entry->referrers.refs = NULL;
    entry->referrers.num_refs = 0;
    entry->referrers.num_allocated = 0;

    // rehash after entry
    weak_entry_t *table = azone->weak_refs_table;
    usword_t table_size = azone->max_weak_refs;
    usword_t hash_index = entry - table;
    usword_t index = hash_index;

    if (!table) return;

    do {
        index++; if (index == table_size) index = 0;
        if (!table[index].referent) return;
        weak_entry_t entry = table[index];
        table[index].referent = NULL;
        weak_entry_insert_no_lock(azone, &entry);
    } while (index != hash_index);
}


// Grow the given zone's table of weak references if it is full.
static void weak_grow_maybe_no_lock(Zone *azone)
{
    if (azone->num_weak_refs >= azone->max_weak_refs * 3 / 4) {
        // grow table
        usword_t old_max = azone->max_weak_refs;
        usword_t new_max = old_max ? old_max * 2 + 1 : 15;
        weak_entry_t *old_entries = azone->weak_refs_table;
        weak_entry_t *new_entries = (weak_entry_t *)aux_calloc(new_max, sizeof(weak_entry_t));
        azone->max_weak_refs = new_max;
        azone->weak_refs_table = new_entries;

        if (old_entries) {
            weak_entry_t *entry;
            weak_entry_t *end = old_entries + old_max;
            for (entry = old_entries; entry < end; entry++) {
                weak_entry_insert_no_lock(azone, entry);
            }
            aux_free(old_entries);
        }
    }
}

// Verify that no locations in the range supplied are registered as referrers
// (Expensive, but good for debugging)
// Return first location in range found to still be registered

void **auto_weak_find_first_referrer(auto_zone_t *zone, void **location, unsigned long count) {
    Zone *azone = (Zone *)zone;
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    usword_t counter = 0;
    for (; counter < azone->max_weak_refs; ++counter) {
        if (!azone->weak_refs_table[counter].referent) continue;
        weak_referrer_t *refs = azone->weak_refs_table[counter].referrers.refs;
        usword_t index = 0;
        for (; index < azone->weak_refs_table[counter].referrers.num_allocated; ++index) {
            if ((refs[index].referrer >= location) && (refs[index].referrer < (location + count))) {
                void **result = refs[index].referrer;
                return result;
            }
        }
    }
    return NULL;
}

// Return the weak reference table entry for the given referent. 
// If there is no entry for referent, return NULL.
static weak_entry_t *weak_entry_for_referent(Zone *azone, const void *referent)
{
    weak_entry_t *table = azone->weak_refs_table;

    if (!table) return NULL;
    
    usword_t table_size = azone->max_weak_refs;
    usword_t hash_index = hash(referent) % table_size;
    usword_t index = hash_index;

    do {
        weak_entry_t *entry = table + index;
        if (entry->referent == referent) return entry;
        if (entry->referent == NULL) return NULL;
        index++; if (index == table_size) index = 0;
    } while (index != hash_index);

    return NULL;
}

#ifdef __BLOCKS__

void weak_enumerate_weak_references(Auto::Zone *azone, const void *referent, weak_ref_visitor_t visitor) {
    weak_entry_t *entry = weak_entry_for_referent(azone, referent);
    if (entry) {
        weak_referrer_t *refs = entry->referrers.refs;
        usword_t count = entry->referrers.num_allocated;
        for (usword_t i = 0; i < count; ++i) {
            weak_referrer_t ref = refs[i];
            if (ref.referrer) {
                if ((uintptr_t(ref.block) & 1)) ref.block = (auto_weak_callback_block_t*)displace(ref.block, -1);
                ASSERTION(ref.referrer[0] == referent);
                visitor(ref);
            }
        }
    }
}

// run through the table while all threads are quiescent and just dump the contents

void weak_enumerate_table(Zone *zone, weak_ref_visitor_t visitor) {
    for (usword_t i = 0, count = zone->max_weak_refs; i < count; ++i) {
        weak_entry_t &entry = zone->weak_refs_table[i];
        const void *referent = entry.referent;
        if (!referent) continue;
        weak_referrer_t *refs = entry.referrers.refs;
        usword_t ref_count = entry.referrers.num_allocated;
        for (usword_t j = 0; j < ref_count; ++j) {
            weak_referrer_t ref = refs[j];
            if (ref.referrer) {
                ASSERTION(referent == ref.referrer[0]);
                if ((uintptr_t(ref.block) & 1)) ref.block = (auto_weak_callback_block_t*)displace(ref.block, -1);
                visitor(ref);
            }
        }
    }
}

void weak_enumerate_table_fixup(Auto::Zone *zone, weak_ref_fixer_t fixer) {
    for (usword_t i = 0, count = zone->max_weak_refs; i < count; ++i) {
        weak_entry_t &entry = zone->weak_refs_table[i];
        const void *referent = entry.referent;
        if (!referent) continue;
        weak_referrer_t *refs = entry.referrers.refs;
        usword_t ref_count = entry.referrers.num_allocated;
        for (usword_t j = 0; j < ref_count; ++j) {
            weak_referrer_t &ref = refs[j];
            if (ref.referrer) {
                fixer(ref);
            }
        }
    }
}

#endif


// Given a pointer to a weak reference entry, clear all referrers, etc.

static void weak_clear_entry_no_lock(Zone *azone, weak_entry_t *entry, uintptr_t *weak_refs_count, auto_weak_callback_block_t **head)
{
    // clear referrers, update counters, update lists
    usword_t count = entry->referrers.num_allocated;
    usword_t index = 0;
    for (; index < count; ++index) {
        weak_referrer_t *ref = &entry->referrers.refs[index];
        if (ref->referrer) {
            if (azone->control.log & AUTO_LOG_WEAK) malloc_printf("%s: WEAK: clearing ref to %p at %p (value was %p)\n", auto_prelude(), entry->referent, ref->referrer, *ref->referrer);
            if (*ref->referrer != entry->referent) {
                malloc_printf("__weak value %p at location %p not equal to %p and so will not be cleared\n", *ref->referrer, ref->referrer, entry->referent);
                void **base = (void **)auto_zone_base_pointer((auto_zone_t*)azone, ref->referrer);
                if (base) {
                    auto_memory_type_t type = auto_zone_get_layout_type((auto_zone_t*)azone, base);
                    malloc_printf("...location is %s starting at %p with first slot value %p\n",
                                  (type & AUTO_OBJECT) ? "an object" : "a data block",
                                  base,
                                  *base);
                }
                continue;
            }
            *ref->referrer = NULL;
            ++*weak_refs_count;
            if (ref->block) {
                if (uintptr_t(ref->block) & 1) {
                    old_auto_weak_callback_block *old_block = (old_auto_weak_callback_block *)displace(ref->block, -1);
                    if (old_block->callback_function && old_block->next == NULL) {
                        old_block->next = *head;
                        *head = ref->block;
                    }
                } else {
                    auto_weak_callback_block_t *block = ref->block;
                    if (block->callback_function && block->next == NULL) {
                        // chain it if isn't already chained & there is a callout to call
                        block->next = *head;
                        *head = block;
                    }
                }
            }
        }
    }
    
    weak_entry_remove_no_lock(azone, entry);
    azone->num_weak_refs--;
}


// Given a set of newly-discovered garbage, zero out any weak 
// references to the garbage. 
auto_weak_callback_block_t *weak_clear_references(Zone *azone, size_t garbage_count, vm_address_t *garbage,
                           uintptr_t *weak_referents_count, uintptr_t *weak_refs_count)
{
    usword_t i;

    *weak_referents_count = *weak_refs_count = 0;
    auto_weak_callback_block_t *head = reinterpret_cast<auto_weak_callback_block_t *>(-1);

    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    
    // if no weak references, don't bother searching.
    if (azone->num_weak_refs == 0) {
        return NULL;
    }
    
    for (i = 0; i < garbage_count; i++) {
        weak_entry_t *entry = weak_entry_for_referent(azone, (void *)garbage[i]);
        if (entry) {
            weak_clear_entry_no_lock(azone, entry, weak_refs_count, &head);
            ++*weak_referents_count;
        }
    }
    return head;
}

// Unregister an already-registered weak reference. 
// This is used when referrer's storage is about to go away, but referent 
//   isn't dead yet. (Otherwise, zeroing referrer later would be a 
//   bad memory access.)
// Does nothing if referent/referrer is not a currently active weak reference.
// fixme currently requires old referent value to be passed in (lame)
// fixme unregistration should be automatic if referrer is collected
static void weak_unregister_no_lock(Zone *azone, const void *referent, void **referrer)
{
    weak_entry_t *entry;

    if (azone->control.log & AUTO_LOG_WEAK) malloc_printf("%s: WEAK: unregistering weak reference to %p at %p\n", auto_prelude(), referent, referrer);

    if ((entry = weak_entry_for_referent(azone, referent))) {
        remove_referrer_no_lock(&entry->referrers, referrer);
        if (entry->referrers.num_refs == 0) {
            weak_entry_remove_no_lock(azone, entry);
            azone->num_weak_refs--;
        }
    } 
}

static void weak_register_no_lock(Zone *azone, const void *referent, void **referrer,  auto_weak_callback_block_t *block) {
    if (*referrer) weak_unregister_no_lock(azone, *referrer, referrer);
    if (referent) {
        weak_entry_t *entry;
        if ((entry = weak_entry_for_referent(azone, referent))) {
            append_referrer_no_lock(&entry->referrers, referrer, block);
        } 
        else {
            weak_entry_t new_entry;
            new_entry.referent = referent;
            new_entry.referrers.refs = NULL;
            new_entry.referrers.num_refs = 0;
            new_entry.referrers.num_allocated = 0;
            append_referrer_no_lock(&new_entry.referrers, referrer, block);
            weak_grow_maybe_no_lock(azone);
            azone->num_weak_refs++;
            weak_entry_insert_no_lock(azone, &new_entry);
        }
    }
    // make sure that anyone accessing this via a read gets the value
    *referrer = (void *)referent;
}

// Register a new weak reference.
// referent is the object being weakly pointed to
// referrer is the memory location that will be zeroed when referent dies
// Does not check whether referent is currently live (fixme probably should)
// Does not check whether referrer is already a weak reference location for 
//   the given referent or any other referent (fixme maybe should)
// Does not change the scannability of referrer; referrer should be non-scanned
void weak_register(Zone *azone, const void *referent, void **referrer,  auto_weak_callback_block_t *block)
{
    if (azone->control.log & AUTO_LOG_WEAK) malloc_printf("%s: WEAK: registering weak reference to %p at %p\n", auto_prelude(), referent, referrer);
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    auto_weak_callback_block_t *new_block = (block == NULL || block->isa != NULL) ? block : (auto_weak_callback_block_t *)displace(block, 1);
    weak_register_no_lock(azone, referent, referrer, new_block);
}

void weak_transfer_weak_referents(Auto::Zone *azone, const void *oldReferent, const void *newReferent) {
    weak_entry_t *entry = weak_entry_for_referent(azone, oldReferent);
    if (entry == NULL) return;
    
    usword_t i, count = entry->referrers.num_allocated, found = 0;
    weak_referrer_t *refs = (weak_referrer_t *)alloca(count * sizeof(weak_referrer_t));
    for (i = 0; i < count; ++i) {
        weak_referrer_t &ref = entry->referrers.refs[i];
        if (ref.referrer) {
            ASSERTION(*ref.referrer == oldReferent);
            refs[found++] = ref;
        }
    }
    for (i = 0; i < found; ++i) {
        weak_referrer_t &ref = refs[i];
        weak_register_no_lock(azone, newReferent, ref.referrer, ref.block);
    }
    ASSERTION(weak_entry_for_referent(azone, oldReferent) == NULL);
}

void weak_transfer_weak_contents(Auto::Zone *azone, void *oldBlock[], void *newBlock[], const uint8_t *map) {
    usword_t index = 0;
    uint8_t byte;
    while ((byte = *map++)) {
        uint8_t skips = (byte >> 4);
        index += skips;
        uint8_t weaks = (byte & 0x0F);
        while (weaks--) {
            void **slot = &oldBlock[index];
            const void *referent = *slot;
            if (referent) {
                weak_entry_t *entry = weak_entry_for_referent(azone, referent);
                weak_referrer_array_t &referrers = entry->referrers;
                usword_t probe = hash(slot) % referrers.num_allocated, hash_displacement = 0;
                while (hash_displacement++ <= referrers.max_hash_displacement) {
                    weak_referrer_t &ref = referrers.refs[probe];
                    if (ref.referrer == slot) {
                        newBlock[index] = NULL;
                        weak_register_no_lock(azone, referent, &newBlock[index], ref.block);
                        weak_unregister_no_lock(azone, referent, slot);
                        break;
                    }
                    if (++probe == referrers.num_allocated)
                        probe = 0;
                }
            }
            ++index;
        }
    }
}

// IDEA:  can make this more efficient by walking all of the slots, and checking to see if any of them corresponds to a known
// stored weak reference.

void weak_transfer_weak_contents_unscanned(Auto::Zone *azone, void *oldBlock[], void *newBlock[], size_t size, bool forwarding) {
#if DEBUG
    // check word 1 to ensure it isn't being used as a weak reference. we shouldn't be here if that's the case.
    if (forwarding && newBlock[0]) {
        const void *referent = newBlock[0];
        weak_entry_t *entry = weak_entry_for_referent(azone, referent);
        if (entry) {
            weak_referrer_array_t &referrers = entry->referrers;
            for (usword_t i = 0; i < referrers.num_allocated; ++i) {
                weak_referrer_t &ref = referrers.refs[i];
                ASSERTION(ref.referrer != &oldBlock[0]);
            }
        }
    }
#endif
    // NOTE:  loop starts at word 1 to avoid the forwarding pointer slot.
    for (void **slot = (forwarding ? oldBlock + 1 : oldBlock), **limit = (void **)displace(oldBlock, size - sizeof(void*)); slot <= limit; ++slot) {
        const void *referent = *slot;
        if (referent) {
            weak_entry_t *entry = weak_entry_for_referent(azone, referent);
            if (entry) {
                weak_referrer_array_t &referrers = entry->referrers;
                usword_t probe = hash(slot) % referrers.num_allocated, hash_displacement = 0;
                while (hash_displacement++ <= referrers.max_hash_displacement) {
                    weak_referrer_t &ref = referrers.refs[probe];
                    if (ref.referrer == slot) {
                        // found one, transfer ownership to the same offset in newBlock.
                        ptrdiff_t index = slot - oldBlock;
                        newBlock[index] = NULL;
                        weak_register_no_lock(azone, referent, &newBlock[index], ref.block);
                        weak_unregister_no_lock(azone, referent, slot);
                        break;
                    }
                    if (++probe == referrers.num_allocated)
                        probe = 0;
                }
            }
        }
    }
}

#if 0

static void referrers_in_range_no_lock(Auto::Zone *azone, const Range &range, void (^block) (weak_referrer_t &ref)) {
    usword_t counter = 0;
    for (; counter < azone->max_weak_refs; ++counter) {
        if (!azone->weak_refs_table[counter].referent) continue;
        weak_referrer_t *refs = azone->weak_refs_table[counter].referrers.refs;
        usword_t index = 0;
        for (; index < azone->weak_refs_table[counter].referrers.num_allocated; ++index) {
            if (range.in_range(refs[index].referrer)) {
                block(refs[index]);
            }
        }
    }
}

void weak_transfer_weak_contents_unscanned(Auto::Zone *azone, void *oldBlock[], void *newBlock[], size_t size) {
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    __block WeakReferrerVector refs;
    referrers_in_range_no_lock(azone, Range(oldBlock, size), ^(weak_referrer_t &ref) {
        refs.push_back(ref);
    });
    for (WeakReferrerVector::iterator i = refs.begin(), end = refs.end(); i != end; ++i) {
        weak_referrer_t &ref = *i;
        ptrdiff_t index = ref.referrer - oldBlock;
        weak_register_no_lock(azone, &newBlock[index], ref.referrer, ref.block);
    }
}
#endif

void weak_unregister(Zone *azone, const void *referent, void **referrer)
{
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    weak_unregister_no_lock(azone, referent, referrer);
}

void weak_unregister_range_no_lock(Auto::Zone *azone, void **referrers, size_t count) {
    for (void **slot = referrers, **limit = referrers + (count - 1); slot <= limit; ++slot) {
        const void *referent = *slot;
        if (referent) {
            weak_entry_t *entry = weak_entry_for_referent(azone, referent);
            if (entry) {
                weak_referrer_array_t &referrers = entry->referrers;
                usword_t probe = hash(slot) % referrers.num_allocated, hash_displacement = 0;
                while (hash_displacement++ <= referrers.max_hash_displacement) {
                    weak_referrer_t &ref = referrers.refs[probe];
                    if (ref.referrer == slot) {
                        // found one, unregister it.
                        weak_unregister_no_lock(azone, referent, slot);
                        break;
                    }
                    if (++probe == referrers.num_allocated)
                        probe = 0;
                }
            }
        }
    }
}

void weak_unregister_range(Auto::Zone *azone, void **referrers, size_t count) {
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    weak_unregister_range_no_lock(azone, referrers, count);
}

void weak_unregister_with_layout(Zone *azone, void *block[], const unsigned char *layout) {
    size_t index = 0;
    unsigned char byte;
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    while ((byte = *layout++)) {
        uint8_t skips = (byte >> 4);
        index += skips; 
        uint8_t weaks = (byte & 0x0F);
        while (weaks--) {
            void **slot = &block[index++];
            const void *referent = *slot;
            if (referent) {
                weak_entry_t *entry = weak_entry_for_referent(azone, referent);
                if (entry) {
                    remove_referrer_no_lock(&entry->referrers, slot);
                    if (entry->referrers.num_refs == 0) {
                        weak_entry_remove_no_lock(azone, entry);
                        azone->num_weak_refs--;
                    }
                }
            }
        }
    }
}

/*
    weak_unregister_data_segment
    
    Given an about to be unmapped datasegment address range, this walks the entire
    weak references table searching for weak references in this range. This is likely
    to be pretty slow, but will also be pretty infrequently called. Since the table
    may need to be rehashed as entries are removed, this first walks the table
    and gathers together <referent, referrer> pairs into a vector, and then removes
    the pairs by walking the vector.
 */

struct WeakUnregister {
    Zone *_zone;
    WeakUnregister(Zone *zone) : _zone(zone) {}
    void operator() (const WeakPair &pair) {
        weak_unregister_no_lock(_zone, pair.first, pair.second);
    }
};

void weak_unregister_data_segment(Zone *azone, void *base, size_t size) {
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    if (azone->num_weak_refs == 0) return;
    Auto::Range datasegment(base, size);
    WeakPairVector weakrefsToUnregister;
    weak_entry_t *weak_refs_table = azone->weak_refs_table;
    for (usword_t counter = 0; counter < azone->max_weak_refs; ++counter) {
        void *referent = (void*) weak_refs_table[counter].referent;
        if (!referent) continue;
        weak_referrer_array_t &referrers = weak_refs_table[counter].referrers;
        weak_referrer_t *refs = referrers.refs;
        for (usword_t index = 0; index < referrers.num_allocated; ++index) {
            void **referrer = refs[index].referrer;
            if (datasegment.in_range((void*)referrer)) {
                weakrefsToUnregister.push_back(WeakPair(referent, referrer));
            }
        }
    }
    std::for_each(weakrefsToUnregister.begin(), weakrefsToUnregister.end(), WeakUnregister(azone));
}

void weak_call_callbacks(auto_weak_callback_block_t *block) {
    if (block == NULL) return;
    while (block != (void *)-1) {
        auto_weak_callback_block_t *next;
        if (uintptr_t(block) & 0x1) {
            old_auto_weak_callback_block *old_block = (old_auto_weak_callback_block *)displace(block, -1);
            next = old_block->next;
            // clear the link so it can be used during next cycle
            old_block->next = NULL;
            (*old_block->callback_function)(old_block->arg1, old_block->arg2);
        } else {
            next = block->next;
            // clear the link so it can be used during next cycle
            block->next = NULL;
            (*block->callback_function)(block->target);
        }
        block = next;
    }
}

__private_extern__ void weak_print_stats(void) DEPRECATED_ATTRIBUTE;
__private_extern__ void weak_print_stats(void) 
{
    Zone *azone = (Zone *)auto_zone();
    if (!azone) {
        fprintf(stderr, "weak table empty (GC off)\n");
        return;
    }

    weak_entry_t *table = azone->weak_refs_table;
    if (!table) {
        fprintf(stderr, "weak table empty\n");
        return;
    }
    
    usword_t chainlen = 0;
    usword_t chaincount = 0;
    usword_t chain = 0;
    usword_t chainmax = 0;

    usword_t table_size = azone->max_weak_refs;
    usword_t start;
    usword_t i;
    // find the start of some chain
    for (start = 0; start < azone->max_weak_refs; start++) {
        weak_entry_t *entry = table + start;
        if (! entry->referent) break;
    }
    for ( ; start < azone->max_weak_refs; start++) {
        weak_entry_t *entry = table + start;
        if (entry->referent) break;
    }
    if (start == azone->max_weak_refs) {
        fprintf(stderr, "weak table empty\n");
        return;
    }

    // add up all chains
    i = start;
    do {
        weak_entry_t *entry = table + i;
        if (entry->referent) chain++;
        else if (chain) {
            if (chain > chainmax) chainmax = chain;
            chainlen += chain;
            chaincount++;
            chain = 0;
        }
        i++; if (i == table_size) i = 0;
    } while (i != start);

    fprintf(stderr, "weak table %lu/%lu used, %.1g avg / %lu max chain\n", 
            chainlen, azone->max_weak_refs, 
            chaincount ? chainlen/(double)chaincount : 0.0, chainmax);
}
