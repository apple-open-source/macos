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
    auto_impl_utilities.c
    Implementation Utilities
    Copyright (c) 2002-2011 Apple Inc. All rights reserved.
*/

#include "auto_impl_utilities.h"
#include "auto_tester.h"

#include <malloc/malloc.h>
#include <mach/mach.h>
#include <libc.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#if !TARGET_OS_IPHONE
#   include <CrashReporterClient.h>
#else
    // CrashReporterClient not available on iOS
    static void CRSetCrashLogMessage(const char *buffer) { }
#endif

/*********  Implementation utilities    ************/

vm_address_t auto_get_sp() {
    return (vm_address_t)__builtin_frame_address(0);
}

size_t auto_round_page(size_t size) {
    if (!size) return vm_page_size;
    return (size + vm_page_size - 1) & ~ (vm_page_size - 1);
}

/*********  Utilities   ************/
int auto_ncpus()
{
    static int ncpus = 0;
    if (ncpus == 0) {
        int mib[2];
        size_t len = sizeof(ncpus);
        mib[0] = CTL_HW;
        mib[1] = HW_NCPU;
        if (sysctl(mib, 2, &ncpus, &len, NULL, 0) != 0) {
            // sysctl failed. just return 1 and try again next time
            ncpus = 0;
            return 1;
        }
    }
    return ncpus;
}

const char *auto_prelude(void) {
    static char buf[32] = { 0 };
    if (!buf[0]) {
        snprintf(buf, sizeof(buf), "auto malloc[%d]", getpid());
    }
    return (const char *)buf;
}

void auto_error(void *azone, const char *msg, const void *ptr) {
    if (ptr) {
        malloc_printf("*** %s: error for object %p: %s\n", auto_prelude(), ptr, msg);
    } else {
        malloc_printf("*** %s: error: %s\n", auto_prelude(), msg);
    }        
#if 0 && DEBUG
    malloc_printf("*** Sleeping to help debug\n");
    sleep(3600); // to help debug
#endif
}

void auto_fatal(const char *format, ...) {
    static char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    CRSetCrashLogMessage(buffer);
    malloc_printf("%s", buffer);
    __builtin_trap();
}

/*********  Internal allocation ************/

// GrP fixme assumes only one auto zone is ever active in a process

__private_extern__ malloc_zone_t *aux_zone = NULL;

__private_extern__ void aux_init(void) {
    if (!aux_zone) {
        aux_zone = malloc_create_zone(4096, 0); // GrP fixme size?
        malloc_set_zone_name(aux_zone, "aux_zone");
#if !DEBUG
        // make it possible to call free() on these blocks while debugging.
        malloc_zone_unregister(aux_zone);       // PCB don't let fork() mess with aux_zone <rdar://problem/4580709>
#endif
    }
}

/*********  Dealing with time   ************/

double auto_time_interval(auto_date_t after, auto_date_t before) {
#if 0
    static double rate = 0.0;
    if (rate == 0.0) {
        struct mach_timebase_info info;
        mach_timebase_info(&info);
        rate = (double)info.numer / (1.0E9 * (double)info.denom);
    }
    return (after - before) * rate;
#else
    return (after - before) / 1.0E6;
#endif
}


malloc_zone_t *auto_debug_zone(void)
{
    static malloc_zone_t *z = NULL;
    if (!z) {
        z = malloc_create_zone(4096, 0);
        malloc_set_zone_name(z, "auto debug");
    }
    return z;
}

#define CHECK_ADDR(n) \
    entry->stack[n] = (vm_address_t)(__builtin_return_address(n + 1) - 4); \
    if ((__builtin_frame_address(n + 3) == NULL) || (__builtin_return_address(n + 2) == NULL)) { \
        goto done; \
    }

#define CHECK_ADDR2(n) \
CHECK_ADDR(n) \
CHECK_ADDR(n + 1)

#define CHECK_ADDR4(n) \
CHECK_ADDR2(n) \
CHECK_ADDR2(n + 2)

#define CHECK_ADDR8(n) \
CHECK_ADDR4(n) \
CHECK_ADDR4(n + 4)

#define UNUSED         0xffffffff
#define ENTRY_COUNT 32

typedef struct {
    vm_address_t stack[8];
    unsigned int oldCount;  // may be UNUSED
    unsigned int newCount;  // oldCount == newCount for new allocations
} auto_refcount_history_entry_t;

typedef struct {
    vm_address_t address;
    int nextEntry;
    auto_refcount_history_entry_t entries[ENTRY_COUNT];
} auto_refcount_history_t;


static auto_refcount_history_t *history_list = NULL;
static int history_allocated = 0;
static int history_used = 0;


static int history_hash(vm_address_t target)
{
    int index = (target >> 2) % history_allocated;
    while (history_list[index].address != 0  &&  
           history_list[index].address != target) 
    {
        index++;
        if (index == history_allocated) index = 0;
    }
    return index;
}

static spin_lock_t refcount_stacks_lock;

#define auto_refcount_stacks_lock() spin_lock(&refcount_stacks_lock)
#define auto_refcount_stacks_unlock() spin_unlock(&refcount_stacks_lock)


void auto_record_refcount_stack(auto_zone_t *azone, void *ptr, int delta)
{
    int h, e;
    vm_address_t addr = (vm_address_t)ptr;
    auto_refcount_history_t *history = NULL;
    auto_refcount_history_entry_t *entry;

    auto_refcount_stacks_lock();

    if (history_used >= history_allocated * 3 / 4) {
        auto_refcount_history_t *old_list = history_list;
        int old_allocated = history_allocated;
        history_allocated = old_allocated ? old_allocated * 2 : 10000;
        history_list = 
            malloc_zone_calloc(auto_debug_zone(), 
                               history_allocated, sizeof(*history_list));
        // history_used remains unchanged

        // rehash
        for (h = 0; h < history_used; h++) {
            if (old_list[h].address) {
                history_list[history_hash(old_list[h].address)] = old_list[h];
            }
        }
        malloc_zone_free(auto_debug_zone(), old_list);
    }

    history = &history_list[history_hash(addr)];
    if (history->address != addr) {
        // initialize new location
        history->address = (vm_address_t)ptr;
        history->nextEntry = 0;
        for (e = 0; e < ENTRY_COUNT; e++) {
            history->entries[e].oldCount = UNUSED;
        }
        history_used++;
    }

    entry = &history->entries[history->nextEntry++];
    if (history->nextEntry == ENTRY_COUNT) history->nextEntry = 0;

    bzero(entry, sizeof(*entry));
    CHECK_ADDR8(0);
 done:
    entry->oldCount = auto_zone_retain_count((void *)azone, ptr);
    entry->newCount = entry->oldCount + delta;

    auto_refcount_stacks_unlock();
}


void auto_print_refcount_stacks(void *ptr)
{
    int e;
    vm_address_t addr = (vm_address_t)ptr;
    auto_refcount_history_t *history = NULL;
    auto_refcount_history_entry_t *entry;

    auto_refcount_stacks_lock();

    history = &history_list[history_hash(addr)];

    if (history->address != addr) {
        malloc_printf("No refcount history for address %p\n", ptr);
        return;
    }

    fprintf(stderr, "\nREFCOUNT HISTORY FOR %p\n\n", ptr);

    e = history->nextEntry;
    do {
        entry = &history->entries[e];
        if (entry->oldCount != UNUSED) {
            int s;

            if (entry->oldCount == entry->newCount) {
                fprintf(stderr, "\nrefcount %d (new)\tadecodestack  ", entry->newCount);
            } else {
                fprintf(stderr, "refcount %d -> %d \tadecodestack  ", 
                        entry->oldCount, entry->newCount);
            }

            for (s = 0; s < 8  &&  entry->stack[s]; s++) {
                fprintf(stderr, "%p   ", (void *)entry->stack[s]);
            }

            fprintf(stderr, "\n");
        }
        e++;
        if (e == ENTRY_COUNT) e = 0;
    } while (e != history->nextEntry);

    auto_refcount_stacks_unlock();

    fprintf(stderr, "\ndone\n");
}

//
// ptr_set utilities
//
// Pointer sets are used to track the use of allocated objects.
//

#define PTR_SET_DEPTH   4
#define PTR_SET_GROWTH  333

// Internals

struct ptr_set {
    spin_lock_t lock;
    unsigned length;                    // length of set
    void **members;                     // closed hash table of pointers
    void **end;                         // end + 1 of hash table (faster next pointer)
};

static void **ptr_set_find_slot(ptr_set *set, void *ptr);
static void ptr_set_grow(ptr_set *set);

static inline void **ptr_set_next(ptr_set *set, void **slot) { return ++slot == set->end ? set->members : slot; }
    // return the next slot (wrap around)
  
      
static inline intptr_t ptr_set_hash(ptr_set *set, void *ptr) { return (((intptr_t)ptr >> 2) * 2654435761u) % set->length; }
    // return the hash index of the specified pointer


static boolean_t ptr_set_add_no_lock_did_grow(ptr_set *set, void *ptr) {
    // add a member to the set

    boolean_t didGrow = false;
    
    // current slot
    void **slot;
    
    // don't add NULL entries
    if (ptr == NULL) return false;
    
    // make sure it is 4 byte aligned (or will grow forever)
    if ((intptr_t)ptr & 0x3) {
        malloc_printf("*** %s: ptr_set_add(ptr=%p) not pointer aligned\n", auto_prelude(), ptr);
        return false;
    }
    
    // try and try again
    while (1) {
        // find the pointers slot
        slot = ptr_set_find_slot(set, ptr);
        // if found escape loop
        if (slot != NULL) break;
        // grow the table to make more room for the hash group
        ptr_set_grow(set);
	didGrow = true;
    }
    
    // set the slot (may be redundant if the pointer is already in hashtable)
    *slot = ptr;
    return didGrow;
}

static void ptr_set_grow(ptr_set *set) {
    // Provide more room in the hashtable and rehash the old entries.
    
    // current slot
    void **slot;
     
    // capture old hashtable length
    unsigned old_length = set->length;
    // capture old members
    void **old_members = set->members;
    // capture old end
    void **old_end = set->end;
    
    /// new hashtable length
    set->length = (old_length + PTR_SET_GROWTH) | 1;    
    // allocate new hashtable
    set->members = (void **)aux_calloc(set->length, sizeof(void *));
    // set end
    set->end = set->members + set->length;
    
    // rehash old entries
    for (slot = old_members; slot < old_end; slot++) ptr_set_add_no_lock_did_grow(set, *slot);
    
    // release old hashtable
    aux_free(old_members);
}


static void **ptr_set_find_slot(ptr_set *set, void *ptr) {
    // find the slot the ptr should reside
    
    // current slot
    void **slot;
    // depth index
    unsigned depth;
    // ptr  hash
    unsigned hash = ptr_set_hash(set, ptr);

    // iterate for the closed hash depth
    for (depth = 0, slot = set->members + hash; depth < PTR_SET_DEPTH; depth++, slot = ptr_set_next(set, slot)) {
        // get the old member in the slot
        void *old_member = *slot;
        // return the slot if the slot is empty or already contains the ptr
        if (old_member == NULL || old_member == ptr) return slot;
        // otherwise check to see if the entry is a member of the same hash group
        if (hash != ptr_set_hash(set, old_member)) return NULL;
    }
    
    // not found
    return NULL;
}

// Externals

__private_extern__ ptr_set *ptr_set_new() {
    // initialize the pointer set

    ptr_set *set = aux_malloc(sizeof(ptr_set));

    // zero lock
    set->lock = 0;
    // set length
    set->length = PTR_SET_GROWTH;
    // allocate members
    set->members = (void **)aux_calloc(PTR_SET_GROWTH, sizeof(void *));
    // set end
    set->end = set->members + PTR_SET_GROWTH;
    
    return set;
}


__private_extern__ void ptr_set_dispose(ptr_set *set) {
    // release memory allocate by the set
    aux_free(set->members);
    aux_free(set);
}


__private_extern__ void ptr_set_add(ptr_set *set, void *ptr) {
    spin_lock(&set->lock);
    ptr_set_add_no_lock_did_grow(set, ptr);
    spin_unlock(&set->lock);
}

__private_extern__ int ptr_set_is_member_no_lock(ptr_set *set, void *ptr) {
    // check to see if the pointer is a member of the set
    
    // find the slot
    void **slot = ptr_set_find_slot(set, ptr);
    // return true if the slot is found and contains the pointer
    return (slot != NULL && *slot == ptr);
}

__private_extern__ int ptr_set_is_member(ptr_set *set, void *ptr) {
    // check to see if the pointer is a member of the set
    
    spin_lock(&set->lock);
    // find the slot
    void **slot = ptr_set_find_slot(set, ptr);
    // return true if the slot is found and contains the pointer
    boolean_t result = slot != NULL && *slot == ptr;
    spin_unlock(&set->lock);
    return result;
}


__private_extern__ void ptr_set_remove(ptr_set *set, void *ptr) {
    // remove an entry from the set
    
    spin_lock(&set->lock);
    // find the slot
    void **slot = ptr_set_find_slot(set, ptr);

    // if the pointer is found
    if (slot != NULL && *slot == ptr) {
        // find out which hash goup it belongs
        unsigned hash = ptr_set_hash(set, ptr);
        
        // next member slot
        void **next_slot;
        
        // searching for other members to fillin gap
        for (next_slot =  ptr_set_next(set, slot); 1; next_slot =  ptr_set_next(set, next_slot)) {
            // get the next candidate
            void *old_member = *next_slot;
            // if NULL or not member of the same hash group
            if (old_member == NULL || hash != ptr_set_hash(set, old_member)) {
                // NULL out the last slot in the group
                *slot = NULL;
                break;
            }
            
            // shift down the slots
            *slot = *next_slot;
            slot = next_slot;
        }
    }
    spin_unlock(&set->lock);
}

struct ptr_map {
    spin_lock_t lock;
    unsigned length;                    // length of set
    void **members;                     // closed hash table of pointers
    void **end;                         // end + 1 of hash table (faster next pointer)
};

static void **ptr_map_find_slot(ptr_map *map, void *ptr);
static void ptr_map_grow(ptr_map *map);

static inline void **ptr_map_next(ptr_map *map, void **slot) { return ++slot == map->end ? map->members : slot; }
    // return the next slot (wrap around)
  
      
static inline intptr_t ptr_map_hash(ptr_map *map, void *ptr) { return (((uintptr_t)ptr >> 4) * 2654435761u) % map->length; }
    // return the hash index of the specified pointer


static boolean_t ptr_map_add_no_lock_did_grow(ptr_map *map, void *ptr, void *value) {
    // add a member to the map

    boolean_t didGrow = false;
    
    // current slot
    void **slot;
    
    // don't add NULL entries
    if (ptr == NULL) return false;
    
    // make sure it is 16 byte aligned (or will grow forever)
    if ((intptr_t)ptr & 15) {
        malloc_printf("*** %s: ptr_map_add(ptr=%p) not object aligned\n", auto_prelude(), ptr);
        return false;
    }
    
    // try and try again
    while (1) {
        // find the pointers slot
        slot = ptr_map_find_slot(map, ptr);
        // if found escape loop
        if (slot != NULL) break;
        // grow the table to make more room for the hash group
        ptr_map_grow(map);
	didGrow = true;
    }
    
    // map the slot (may be redundant if the pointer is already in hashtable)
    *slot = ptr;
    *(slot + map->length) = value;
    return didGrow;
}

static void ptr_map_grow(ptr_map *map) {
    // Provide more room in the hashtable and rehash the old entries.
    
    // current slot
    void **slot;
     
    // capture old hashtable length
    unsigned old_length = map->length;
    // capture old members
    void **old_members = map->members;
    // capture old end
    void **old_end = map->end;
    
    /// new hashtable length
    map->length = (old_length + PTR_SET_GROWTH) | 1;
    // allocation size
    size_t size = map->length * sizeof(void *);
    
     // allocate & clear new hashtable
    map->members = (void **)aux_calloc(2, size); // enough room for values, too
    // set end
    map->end = map->members + map->length;
    
    // rehash old entries
    for (slot = old_members; slot < old_end; slot++) ptr_map_add_no_lock_did_grow(map, *slot, *(slot+old_length));
    
    // release old hashtable
    aux_free(old_members);
}


static void **ptr_map_find_slot(ptr_map *map, void *ptr) {
    // find the slot the ptr should reside
    
    // current slot
    void **slot;
    // depth index
    unsigned depth;
    // ptr  hash
    unsigned hash = ptr_map_hash(map, ptr);

    // iterate for the closed hash depth
    for (depth = 0, slot = map->members + hash; depth < PTR_SET_DEPTH; depth++, slot = ptr_map_next(map, slot)) {
        // get the old member in the slot
        void *old_member = *slot;
        // return the slot if the slot is empty or already contains the ptr
        if (old_member == NULL || old_member == ptr) return slot;
        // otherwise check to see if the entry is a member of the same hash group
        if (hash != ptr_map_hash(map, old_member)) return NULL;
    }
    
    // not found
    return NULL;
}

// Externals

__private_extern__ ptr_map * ptr_map_new() {
    // initialize the pointer map
    
    ptr_map *map = aux_malloc(sizeof(ptr_map));
    
    // zero lock
    map->lock = 0;
    // set length
    map->length = PTR_SET_GROWTH;
    // allocation size
    size_t size = PTR_SET_GROWTH * sizeof(void *);
    // allocate & clear members
    map->members = (void **)aux_calloc(2, size);
    // set end
    map->end = map->members + PTR_SET_GROWTH;
    
    return map;
}


__private_extern__ void ptr_map_dispose(ptr_map *map) {
    // release memory allocate by the map
    aux_free(map->members);
    aux_free(map);
}


__private_extern__ void ptr_map_set(ptr_map *map, void *ptr, void *value) {
    spin_lock(&map->lock);
    ptr_map_add_no_lock_did_grow(map, ptr, value);
    spin_unlock(&map->lock);
}

__private_extern__ void * ptr_map_get(ptr_map *map, void *ptr) {
    // check to see if the pointer is a member of the set
    
    spin_lock(&map->lock);
    // find the slot
    void **slot = ptr_map_find_slot(map, ptr);
    // return true if the slot is found and contains the pointer
    void *result = (slot != NULL && *slot == ptr) ? *(slot + map->length) : NULL;
    spin_unlock(&map->lock);
    return result;
}

__private_extern__ void *ptr_map_remove(ptr_map *map, void *ptr) {
    // remove an entry from the map
    
    void *result = NULL;
    
    spin_lock(&map->lock);
    // find the slot
    void **slot = ptr_map_find_slot(map, ptr);

    // if the pointer is found
    if (slot != NULL && *slot == ptr) {
	result = *(slot + map->length);
        // find out which hash goup it belongs
        unsigned hash = ptr_map_hash(map, ptr);
        
        // next member slot
        void **next_slot;
        
        // searching for other members to fillin gap
        for (next_slot =  ptr_map_next(map, slot); 1; next_slot =  ptr_map_next(map, next_slot)) {
            // get the next candidate
            void *old_member = *next_slot;
            // if NULL or not member of the same hash group
            if (old_member == NULL || hash != ptr_map_hash(map, old_member)) {
                // NULL out the last slot in the group
                *slot = NULL;
                break;
            }
            
            // shift down the slots
            *slot = *next_slot; *(slot+map->length) = *(next_slot+map->length);
            slot = next_slot;
        }
    }
    spin_unlock(&map->lock);
    
    return result;
}

/************ Miscellany **************************/


// Watching

#define WatchLimit 16
static const void *WatchPoints[WatchLimit];

void auto_zone_watch(const void *ptr) {
    for (int i = 0; i < WatchLimit; ++i) {
        if (WatchPoints[i]) {
            if (WatchPoints[i] == ptr) return;
        } else {
            WatchPoints[i] = ptr;
            return;
        }
    }
    printf("too many watchpoints already, skipping %p\n", ptr);
}

void auto_zone_watch_free(const void *ptr, watch_block_t block) {
    for (int i = 0; i < WatchLimit; ++i) {
        if (WatchPoints[i] == NULL) return;
        if (WatchPoints[i] == ptr) {
            block();
            while(++i < WatchLimit)
                WatchPoints[i-1] = WatchPoints[i];
            WatchPoints[WatchLimit-1] = NULL;
            return;
        }
    }
}

void auto_zone_watch_apply(void *ptr, watch_block_t block) {
    for (int i = 0; i < WatchLimit; ++i) {
        if (WatchPoints[i] == NULL) return;
        if (WatchPoints[i] == ptr) {
            block();
            return;
        }
    }
}

__private_extern__ void auto_refcount_underflow_error(void *block) { }

__private_extern__ void auto_zone_resurrection_error()
{
}

__private_extern__ void auto_zone_thread_local_error(void) { }

__private_extern__ void auto_zone_thread_registration_error() 
{
    AUTO_PROBE(auto_probe_unregistered_thread_error());
}

__private_extern__ void auto_zone_global_data_memmove_error() { }

__private_extern__ void auto_zone_association_error(void *address) { }

__private_extern__ void auto_zone_unscanned_store_error(const void *destination, const void *value) { }
