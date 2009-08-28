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
    auto_weak.cpp
    Weak reference accounting
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "auto_weak.h"
#include "AutoLock.h"
#include "AutoDefs.h"
#include "AutoRange.h"
#include "AutoZone.h"

using namespace Auto;

struct weak_referrer_t {
    void **referrer;    // clear this address
    auto_weak_callback_block_t *block;
};
typedef struct weak_referrer_t weak_referrer_t;

struct weak_referrer_array_t {
    weak_referrer_t 	*refs;
    unsigned		num_refs;
    unsigned		num_allocated;
    unsigned        max_hash_displacement;
};
typedef struct weak_referrer_array_t weak_referrer_array_t;

struct Auto::weak_entry_t {
    const void *referent;
    weak_referrer_array_t referrers;
};
typedef struct Auto::weak_entry_t weak_entry_t;

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


// Grow the refs list. Rehashes the entries.
static void grow_refs(weak_referrer_array_t *list)
{
    unsigned old_num_allocated = list->num_allocated;
    unsigned num_refs = list->num_refs;
    weak_referrer_t *old_refs = list->refs;
    unsigned new_allocated;
    if (old_num_allocated == 0) {
        new_allocated = 1;
    } else if (old_num_allocated == 1) {
        new_allocated = 2;
    } else {
        new_allocated = old_num_allocated + old_num_allocated - 1;
    }
    list->refs = (weak_referrer_t *)aux_calloc(new_allocated, sizeof(weak_referrer_t));
    list->num_allocated = new_allocated;
    list->num_refs = 0;
    list->max_hash_displacement = 0;
    
    unsigned i;
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
    if (list->num_refs >= list->num_allocated * 2 / 3) {
        grow_refs(list);
    }
    unsigned index = hash(new_referrer) % list->num_allocated, hash_displacement = 0;
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
    list->refs[index].referrer = new_referrer;
    list->refs[index].block = new_block;
    list->num_refs++;
}


// Remove old_referrer from list, if it's present.
// Does not remove duplicates.
// fixme this is slow if old_referrer is not present.
static void remove_referrer_no_lock(weak_referrer_array_t *list, void **old_referrer)
{

    unsigned index = hash(old_referrer) % list->num_allocated;
    unsigned start_index = index, hash_displacement = 0;
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

    unsigned table_size = azone->max_weak_refs;
    unsigned hash_index = hash(new_entry->referent) % table_size;
    unsigned index = hash_index;

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
    unsigned table_size = azone->max_weak_refs;
    unsigned hash_index = entry - table;
    unsigned index = hash_index;

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
        unsigned old_max = azone->max_weak_refs;
        unsigned new_max = old_max ? old_max * 2 + 1 : 15;
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
    unsigned long int counter = 0;
    for (; counter < azone->max_weak_refs; ++counter) {
        if (!azone->weak_refs_table[counter].referent) continue;
        weak_referrer_t *refs = azone->weak_refs_table[counter].referrers.refs;
        unsigned long index = 0;
        for (; index < azone->weak_refs_table[counter].referrers.num_allocated; ++index) {
            if ((refs[index].referrer >= location) && (refs[index].referrer < (location + count))) {
                void **result = refs[index].referrer;
                return result;
            }
        }
    }
    return NULL;
}


// run through the table while all threads are quiessent and just dump the contents
void weak_dump_table(Zone *zone, void (^weak_dump)(const void **address, const void *item)) {
    unsigned long int counter = 0;
    for (; counter < zone->max_weak_refs; ++counter) {
        if (!zone->weak_refs_table[counter].referent) continue;
        weak_referrer_t *refs = zone->weak_refs_table[counter].referrers.refs;
        unsigned long index = 0;
        for (; index < zone->weak_refs_table[counter].referrers.num_allocated; ++index) {
            weak_dump((const void **)refs[index].referrer, zone->weak_refs_table[counter].referent);
        }
    }
}


// Return the weak reference table entry for the given referent. 
// If there is no entry for referent, return NULL.
static weak_entry_t *weak_entry_for_referent(Zone *azone, const void *referent)
{
    weak_entry_t *table = azone->weak_refs_table;

    if (!table) return NULL;
    
    unsigned table_size = azone->max_weak_refs;
    unsigned hash_index = hash(referent) % table_size;
    unsigned index = hash_index;

    do {
        weak_entry_t *entry = table + index;
        if (entry->referent == referent) return entry;
        if (entry->referent == NULL) return NULL;
        index++; if (index == table_size) index = 0;
    } while (index != hash_index);

    return NULL;
}

// Given a pointer to a weak reference entry, clear all referrers, etc.

static void weak_clear_entry_no_lock(Zone *azone, weak_entry_t *entry, uintptr_t *weak_refs_count, auto_weak_callback_block_t **head)
{
    // clear referrers, update counters, update lists
    unsigned count = entry->referrers.num_allocated;
    unsigned index = 0;
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
            if (ref->block && ref->block->callback_function && !ref->block->next) {
                // chain it if isn't already chained & there is a callout to call
                ref->block->next = *head;
                *head = ref->block;
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
    unsigned i;

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

// Register a new weak reference.
// referent is the object being weakly pointed to
// referrer is the memory location that will be zeroed when referent dies
// Does not check whether referent is currently live (fixme probably should)
// Does not check whether referrer is already a weak reference location for 
//   the given referent or any other referent (fixme maybe should)
// Does not change the scannability of referrer; referrer should be non-scanned
void weak_register(Zone *azone, const void *referent, void **referrer,  auto_weak_callback_block_t *block)
{
    weak_entry_t *entry;

    if (azone->control.log & AUTO_LOG_WEAK) malloc_printf("%s: WEAK: registering weak reference to %p at %p\n", auto_prelude(), referent, referrer);

    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    if (*referrer) weak_unregister_no_lock(azone, *referrer, referrer);
    if (referent) {
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


void weak_unregister(Zone *azone, const void *referent, void **referrer)
{
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    weak_unregister_no_lock(azone, referent, referrer);
}

void weak_unregister_with_layout(Zone *azone, void *block[], const unsigned char *layout) {
    size_t index = 0;
    unsigned char byte;
    Auto::SpinLock lock(&azone->weak_refs_table_lock);
    while (byte = *layout++) {
        unsigned skips = (byte >> 4);
        while (skips--) index++;
        unsigned weaks = (byte & 0x0F);
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

typedef std::pair<const void *, void **> WeakPair;
typedef std::vector<WeakPair, Auto::AuxAllocator<WeakPair> > WeakPairVector;

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
    for (unsigned long int counter = 0; counter < azone->max_weak_refs; ++counter) {
        void *referent = (void*) weak_refs_table[counter].referent;
        if (!referent) continue;
        weak_referrer_array_t &referrers = weak_refs_table[counter].referrers;
        weak_referrer_t *refs = referrers.refs;
        for (unsigned long int index = 0; index < referrers.num_allocated; ++index) {
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
        auto_weak_callback_block_t *next = block->next;
        // clear the link so it can be used during next cycle
        block->next = NULL;
        if (block->callback_function) {
	    (*block->callback_function)(block->arg1, block->arg2);
	}
        block = next;
    }
}

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
    
    unsigned chainlen = 0;
    unsigned chaincount = 0;
    unsigned chain = 0;
    unsigned chainmax = 0;

    unsigned table_size = azone->max_weak_refs;
    unsigned start;
    unsigned i;
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

    fprintf(stderr, "weak table %u/%u used, %.1g avg / %u max chain\n", 
            chainlen, azone->max_weak_refs, 
            chaincount ? chainlen/(double)chaincount : 0.0, chainmax);
}
