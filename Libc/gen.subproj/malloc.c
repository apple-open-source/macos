/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
#define __POSIX_LIB__
#import <stdlib.h>
#import <stdio.h>
#import <string.h>
#import <unistd.h>
#import <objc/zone.h>
#import <pthread_internals.h>	// for spin lock
#import <objc/malloc.h>
#include <crt_externs.h>

#import "scalable_malloc.h"
#import "stack_logging.h"

#define USE_SLEEP_RATHER_THAN_ABORT	0

#define MAX_ALLOCATION 0xc0000000 // beyond this, assume a programming error
#define INITIAL_ZONES	8  // After this number, we reallocate for new zones

typedef void (malloc_logger_t)(unsigned type, unsigned arg1, unsigned arg2, unsigned arg3, unsigned result, unsigned num_hot_frames_to_skip);

static pthread_lock_t _malloc_lock;
static malloc_zone_t *initial_malloc_zones[INITIAL_ZONES] = {0};

/* The following variables are exported for the benefit of performance tools */
unsigned malloc_num_zones = 0;
malloc_zone_t **malloc_zones = initial_malloc_zones;
malloc_logger_t *malloc_logger = NULL;

unsigned malloc_debug_flags = 0;

unsigned malloc_check_start = 0; // 0 means don't check
unsigned malloc_check_counter = 0;
unsigned malloc_check_each = 1000;

#define MALLOC_LOCK()		LOCK(_malloc_lock)
#define MALLOC_UNLOCK()		UNLOCK(_malloc_lock)

#define MALLOC_LOG_TYPE_ALLOCATE	stack_logging_type_alloc
#define MALLOC_LOG_TYPE_DEALLOCATE	stack_logging_type_dealloc
#define MALLOC_LOG_TYPE_HAS_ZONE	stack_logging_flag_zone
#define MALLOC_LOG_TYPE_CLEARED		stack_logging_flag_cleared

/*********	Utilities	************/

static inline malloc_zone_t *find_registered_zone(const void *ptr, size_t *returned_size) {
    // locates the proper zone
    // if zone found fills returnedSize; else returns NULL
    // See comment in malloc_zone_register() about clients non locking to call this function
    // Speed is critical for this function
    unsigned	index = malloc_num_zones;
    malloc_zone_t	**zones = malloc_zones;
    while (index--) {
        malloc_zone_t	*zone = *zones++;
        size_t	size;
        size = zone->size(zone, ptr);
        if (size) {
            if (returned_size) *returned_size = size;
            return zone;
        }
    }
    return NULL;
}

/*********	Creation and destruction	************/

static void _malloc_initialize(void) {
    // guaranteed to be called only once
    (void)malloc_create_zone(0, 0);
    malloc_set_zone_name(malloc_zones[0], "DefaultMallocZone");
    LOCK_INIT(_malloc_lock);
    // malloc_printf("Malloc: %d registered zones\n", malloc_num_zones);
    // malloc_printf("malloc: malloc_zones is at 0x%x; malloc_num_zones is at 0x%x\n", (unsigned)&malloc_zones, (unsigned)&malloc_num_zones);
}

static inline malloc_zone_t *inline_malloc_default_zone(void) {
    if (!malloc_num_zones) _malloc_initialize();
    // malloc_printf("In inline_malloc_default_zone with %d %d\n", malloc_num_zones, malloc_has_debug_zone);
    return malloc_zones[0];
}

malloc_zone_t *malloc_default_zone(void) {
    return inline_malloc_default_zone();
}

static void set_flags_from_environment(void) {
    const char *flag;
    if (getenv("MallocGuardEdges")) {
	malloc_debug_flags = SCALABLE_MALLOC_ADD_GUARD_PAGES;
	malloc_printf("malloc[%d]: protecting edges\n", getpid());
	if (getenv("MallocDoNotProtectPrelude")) {
	    malloc_debug_flags |= SCALABLE_MALLOC_DONT_PROTECT_PRELUDE;
	    malloc_printf("malloc[%d]: ... but not protecting prelude guard page\n", getpid());
	}
	if (getenv("MallocDoNotProtectPostlude")) {
	    malloc_debug_flags |= SCALABLE_MALLOC_DONT_PROTECT_POSTLUDE;
	    malloc_printf("malloc[%d]: ... but not protecting postlude guard page\n", getpid());
	}
    }
    flag = getenv("MallocStackLogging");
    if (!flag) {
	flag = getenv("MallocStackLoggingNoCompact");
	stack_logging_dontcompact = 1;
    }
    if (flag) {
	unsigned	val = strtoul(flag, NULL, 0);
	if (val == 1) val = 0;
	if (val == -1) val = 0;
	malloc_logger = (val) ? (void *)val : stack_logging_log_stack;
	stack_logging_enable_logging = 1;
	if (malloc_logger == stack_logging_log_stack) {
	    malloc_printf("malloc[%d]: recording stacks using standard recorder\n", getpid());
	} else {
	    malloc_printf("malloc[%d]: recording stacks using recorder %p\n", getpid(), malloc_logger);
	}
	if (stack_logging_dontcompact) malloc_printf("malloc[%d]: stack logging compaction turned off; VM can increase rapidly\n", getpid());
    }
    if (getenv("MallocScribble")) {
	malloc_debug_flags |= SCALABLE_MALLOC_DO_SCRIBBLE;
	malloc_printf("malloc[%d]: enabling scribbling to detect mods to free blocks\n", getpid());
    }
    flag = getenv("MallocCheckHeapStart");
    if (flag) {
	malloc_check_start = strtoul(flag, NULL, 0);
	if (malloc_check_start == 0) malloc_check_start = 1;
	if (malloc_check_start == -1) malloc_check_start = 1;
	flag = getenv("MallocCheckHeapEach");
	if (flag) {
	    malloc_check_each = strtoul(flag, NULL, 0);
	    if (malloc_check_each == 0) malloc_check_each = 1;
	    if (malloc_check_each == -1) malloc_check_each = 1;
	}
	malloc_printf("malloc[%d]: checks heap after %dth operation and each %d operations\n", getpid(), malloc_check_start, malloc_check_each);
    }
    if (getenv("MallocHelp")) {
	malloc_printf(
	    "malloc[%d]: environment variables that can be set for debug:\n"
	    "- MallocGuardEdges to add 2 guard pages for each large block\n"
	    "- MallocDoNotProtectPrelude to disable protection (when previous flag set)\n"
	    "- MallocDoNotProtectPostlude to disable protection (when previous flag set)\n"
	    "- MallocStackLogging to record all stacks.  Tools like leaks can then be applied\n"
	    "- MallocStackLoggingNoCompact to record all stacks.  Needed for malloc_history\n"
	    "- MallocScribble to detect writing on free blocks: 0x55 is written upon free\n"
	    "- MallocCheckHeapStart <n> to check the heap from time to time after <n> operations \n"
	    "- MallocHelp - this help!\n", getpid());
    }
}

malloc_zone_t *malloc_create_zone(vm_size_t start_size, unsigned flags) {
    malloc_zone_t	*zone;
    if (!malloc_num_zones) {
	char	**env = * _NSGetEnviron();
	char	**p;
	char	*c;
	/* Given that all environment variables start with "Malloc" we optimize by scanning quickly first the environment, therefore avoiding repeated calls to getenv() */
	for (p = env; (c = *p) != NULL; ++p) {
	    if (!strncmp(c, "Malloc", 6)) {
		set_flags_from_environment(); 
		break;
	    }
	}

    }
    zone = create_scalable_zone(start_size, malloc_debug_flags);
    malloc_zone_register(zone);
    return zone;
}

void malloc_destroy_zone(malloc_zone_t *zone) {
    malloc_zone_unregister(zone);
    zone->destroy(zone);
}

/*********	Block creation and manipulation	************/

static void internal_check(void) {
    static vm_address_t	*frames = NULL;
    static unsigned	num_frames;
    if (malloc_zone_check(NULL)) {
	malloc_printf("MallocCheckHeap: PASSED check at %dth operation\n", malloc_check_counter-1);
	if (!frames) vm_allocate(mach_task_self(), (void *)&frames, vm_page_size, 1);
	thread_stack_pcs(frames, vm_page_size/sizeof(vm_address_t) - 1, &num_frames);
    } else {
	malloc_printf("*** MallocCheckHeap: FAILED check at %dth operation\n", malloc_check_counter-1);
	if (frames) {
	    unsigned	index = 1;
	    malloc_printf("Stack for last operation where the malloc check succeeded: ");
	    while (index < num_frames) malloc_printf("0x%x ", frames[index++]);
	    malloc_printf("\n(Use 'atos' for a symbolic stack)\n");
	}
	if (malloc_check_each > 1) {
	    unsigned	recomm_each = (malloc_check_each > 10) ? malloc_check_each/10 : 1;
	    unsigned	recomm_start = (malloc_check_counter > malloc_check_each+1) ? malloc_check_counter-1-malloc_check_each : 1;
	    malloc_printf("*** Recommend using 'setenv MallocCheckHeapStart %d; setenv MallocCheckHeapEach %d' to narrow down failure\n", recomm_start, recomm_each);
	}
	malloc_printf("*** Sleeping for 100 seconds to leave time to attach\n");
	sleep(100);
    }
    malloc_check_start += malloc_check_each;
}

void *malloc_zone_malloc(malloc_zone_t *zone, size_t size) {
    void	*ptr;
    if ((unsigned)size >= MAX_ALLOCATION) {
        /* Probably a programming error */
        fprintf(stderr, "*** malloc_zone_malloc[%d]: argument too large: %d\n", getpid(), (unsigned)size);
        return NULL;
    }
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    ptr = zone->malloc(zone, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, size, 0, (unsigned)ptr, 0);
    return ptr;
}

void *malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size) {
    void	*ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    if (((unsigned)num_items >= MAX_ALLOCATION) || ((unsigned)size >= MAX_ALLOCATION) || ((long long)size * num_items >= (long long) MAX_ALLOCATION)) {
        /* Probably a programming error */
        fprintf(stderr, "*** malloc_zone_calloc[%d]: arguments too large: %d,%d\n", getpid(), (unsigned)num_items, (unsigned)size);
        return NULL;
    }
    ptr = zone->calloc(zone, num_items, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE | MALLOC_LOG_TYPE_CLEARED, (unsigned)zone, num_items * size, 0, (unsigned)ptr, 0);
    return ptr;
}

void *malloc_zone_valloc(malloc_zone_t *zone, size_t size) {
    void	*ptr;
    if ((unsigned)size >= MAX_ALLOCATION) {
        /* Probably a programming error */
        fprintf(stderr, "*** malloc_zone_valloc[%d]: argument too large: %d\n", getpid(), (unsigned)size);
        return NULL;
    }
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    ptr = zone->valloc(zone, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, size, 0, (unsigned)ptr, 0);
    return ptr;
}

void *malloc_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size) {
    void	*new_ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    new_ptr = zone->realloc(zone, ptr, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, (unsigned)ptr, size, (unsigned)new_ptr, 0);
    return new_ptr;
}

void malloc_zone_free(malloc_zone_t *zone, void *ptr) {
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, (unsigned)ptr, 0, 0, 0);
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    zone->free(zone, ptr);
}

malloc_zone_t *malloc_zone_from_ptr(const void *ptr) {
    malloc_zone_t	*zone;
    if (!ptr) return NULL;
    zone = find_registered_zone(ptr, NULL);
    return zone;
}

/*********	Functions for zone implementors	************/

void malloc_zone_register(malloc_zone_t *zone) {
    /* Note that given the sequencing it is always safe to first get the number of zones, then get malloc_zones without taking the lock, if all you need is to iterate through the list */
    MALLOC_LOCK();
    if (malloc_num_zones >= INITIAL_ZONES) {
        malloc_zone_t		**zones = malloc_zones;
        malloc_zone_t		*pzone = malloc_zones[0];
        boolean_t		copy = malloc_num_zones == INITIAL_ZONES;
        if (copy) zones = NULL; // to avoid realloc on something not allocated
        MALLOC_UNLOCK();
        zones = pzone->realloc(pzone, zones, (malloc_num_zones + 1) * sizeof(malloc_zone_t *)); // we leak initial_malloc_zones, not worth tracking it
        MALLOC_LOCK();
        if (copy) memcpy(zones, malloc_zones, malloc_num_zones * sizeof(malloc_zone_t *));
        malloc_zones = zones;
    }
    malloc_zones[malloc_num_zones] = zone;
    malloc_num_zones++; // note that we do this after setting malloc_num_zones, so enumerations without taking the lock are safe
    MALLOC_UNLOCK();
    // malloc_printf("Registered %p malloc_zones at address %p is %p [%d zones]\n", zone, &malloc_zones, malloc_zones, malloc_num_zones);
}

void malloc_zone_unregister(malloc_zone_t *z) {
    unsigned	index;
    MALLOC_LOCK();
    index = malloc_num_zones;
    while (index--) {
        malloc_zone_t	*zone = malloc_zones[index];
        if (zone == z) {
            malloc_zones[index] = malloc_zones[--malloc_num_zones];
            MALLOC_UNLOCK();
            return;
        }
    }
    MALLOC_UNLOCK();
    fprintf(stderr, "*** malloc[%d]: malloc_zone_unregister() failed for %p\n", getpid(), z);
}

void malloc_set_zone_name(malloc_zone_t *z, const char *name) {
    char	*newName;
    if (z->zone_name) {
        free((char *)z->zone_name);
        z->zone_name = NULL;
    }
    newName = malloc_zone_malloc(z, strlen(name) + 1);
    strcpy(newName, name);
    z->zone_name = (const char *)newName;
}

const char *malloc_get_zone_name(malloc_zone_t *zone) {
    return zone->zone_name;
}

static char *_malloc_append_unsigned(unsigned value, unsigned base, char *head) {
    if (!value) {
        head[0] = '0';
    } else {
        if (value >= base) head = _malloc_append_unsigned(value / base, base, head);
        value = value % base;
        head[0] = (value < 10) ? '0' + value : 'a' + value - 10;
    }
    return head+1;
}

void malloc_printf(const char *format, ...) {
    va_list	args;
    char	buf[1024];
    char	*head = buf;
    char	ch;
    unsigned	*nums;
    va_start(args, format);
#if LOG_THREAD
    head = _malloc_append_unsigned(((unsigned)&args) >> 12, 16, head);
    *head++ = ' ';
#endif
    nums = args;
    while (ch = *format++) {
        if (ch == '%') {
            ch = *format++;
            if (ch == 's') {
                char	*str = (char *)(*nums++);
                write(2, buf, head - buf);
                head = buf;
                write(2, str, strlen(str));
            } else {
                if (ch == 'p') {
                    *head++ = '0'; *head++ = 'x';
                }
                head = _malloc_append_unsigned(*nums++, (ch == 'd') ? 10 : 16, head);
            }
        } else {
            *head++ = ch;
        }
    }
    write(2, buf, head - buf); fflush(stderr);
    va_end(args);
}

/*********	Generic ANSI callouts	************/

void *malloc(size_t size) {
    return malloc_zone_malloc(inline_malloc_default_zone(), size);
}

void *calloc(size_t num_items, size_t size) {
    return malloc_zone_calloc(inline_malloc_default_zone(), num_items, size);
}

void free(void *ptr) {
    malloc_zone_t	*zone;
    if (!ptr) return;
    zone = find_registered_zone(ptr, NULL);
    if (zone) {
        malloc_zone_free(zone, ptr);
    } else {
        fprintf(stderr, "*** malloc[%d]: Deallocation of a pointer not malloced: %p; This could be a double free(), or free() called with the middle of an allocated block; Try setting environment variable MallocHelp to see tools to help debug\n", getpid(), ptr);
    }
}

void *realloc(void *old_ptr, size_t new_size) {
    malloc_zone_t	*zone;
    size_t	old_size = 0;
    if (!old_ptr) return malloc_zone_malloc(inline_malloc_default_zone(), new_size);
    zone = find_registered_zone(old_ptr, &old_size);
    if (zone && (old_size >= new_size)) return old_ptr;
    if (!zone) zone = inline_malloc_default_zone();
    return malloc_zone_realloc(zone, old_ptr, new_size);
}

void *valloc(size_t size) {
    malloc_zone_t	*zone = inline_malloc_default_zone();
    return malloc_zone_valloc(zone, size);
}

extern void vfree(void *ptr) {
    free(ptr);
}

size_t malloc_size(const void *ptr) {
    size_t	size = 0;
    if (!ptr) return size;
    (void)find_registered_zone(ptr, &size);
    return size;
}

size_t malloc_good_size (size_t size) {
    malloc_zone_t	*zone = inline_malloc_default_zone();
    return zone->introspect->good_size(zone, size);
}

/*********	Functions for performance tools	************/

static kern_return_t _malloc_default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr) {
    *ptr = (void *)address;
    return 0;
}

kern_return_t malloc_get_all_zones(task_t task, memory_reader_t reader, vm_address_t **addresses, unsigned *count) {
    // Note that the 2 following addresses are not correct if the address of the target is different from your own.  This notably occurs if the address of System.framework is slid (e.g. different than at B & I )
    vm_address_t	remote_malloc_zones = (vm_address_t)&malloc_zones;
    vm_address_t	remote_malloc_num_zones = (vm_address_t)&malloc_num_zones;
    kern_return_t	err;
    vm_address_t	zones_address;
    vm_address_t	*zones_address_ref;
    unsigned		num_zones;
    unsigned		*num_zones_ref;
    if (!reader) reader = _malloc_default_reader;
    // printf("Read malloc_zones at address %p should be %p\n", &malloc_zones, malloc_zones);
    err = reader(task, remote_malloc_zones, sizeof(void *), (void **)&zones_address_ref);
    // printf("Read malloc_zones[0x%x]=%p\n", remote_malloc_zones, *zones_address_ref);
    if (err) {
        fprintf(stderr, "*** malloc[%d]: malloc_get_all_zones: error reading zones_address at 0x%x\n", getpid(), (unsigned)remote_malloc_zones);
        return err;
    }
    zones_address = *zones_address_ref;
    // printf("Reading num_zones at address %p\n", remote_malloc_num_zones);
    err = reader(task, remote_malloc_num_zones, sizeof(unsigned), (void **)&num_zones_ref);
    if (err) {
        fprintf(stderr, "*** malloc[%d]: malloc_get_all_zones: error reading num_zones at 0x%x\n",  getpid(), (unsigned)remote_malloc_num_zones);
        return err;
    }
    num_zones = *num_zones_ref;
    // printf("Read malloc_num_zones[0x%x]=%d\n", remote_malloc_num_zones, num_zones);
    *count = num_zones;
    // printf("malloc_get_all_zones succesfully found %d zones\n", num_zones);
    err = reader(task, zones_address, sizeof(malloc_zone_t *) * num_zones, (void **)addresses);
    if (err) {
        fprintf(stderr, "*** malloc[%d]: malloc_get_all_zones: error reading zones at 0x%x\n", getpid(), (unsigned)&zones_address);
        return err;
    }
    // printf("malloc_get_all_zones succesfully read %d zones\n", num_zones);
    return err;
}

/*********	Debug helpers	************/

void malloc_zone_print_ptr_info(void *ptr) {
    malloc_zone_t	*zone;
    if (!ptr) return;
    zone = find_registered_zone(ptr, NULL);
    if (zone) {
        printf("ptr %p in registered zone %p\n", ptr, zone);
    } else {
        printf("ptr %p not in heap\n", ptr);
    }
}

boolean_t malloc_zone_check(malloc_zone_t *zone) {
    boolean_t	ok = 1;
    if (!zone) {
        unsigned	index = 0;
        while (index < malloc_num_zones) {
            zone = malloc_zones[index++];
            if (!zone->introspect->check(zone)) ok = 0;
        }
    } else {
        ok = zone->introspect->check(zone);
    }
    return ok;
}

void malloc_zone_print(malloc_zone_t *zone, boolean_t verbose) {
    if (!zone) {
        unsigned	index = 0;
        while (index < malloc_num_zones) {
            zone = malloc_zones[index++];
            zone->introspect->print(zone, verbose);
        }
    } else {
        zone->introspect->print(zone, verbose);
    }
}

void malloc_zone_log(malloc_zone_t *zone, void *address) {
    if (!zone) {
        unsigned	index = 0;
        while (index < malloc_num_zones) {
            zone = malloc_zones[index++];
            zone->introspect->log(zone, address);
        }
    } else {
        zone->introspect->log(zone, address);
    }
}

/*********	Misc other entry points	************/

static void DefaultMallocError(int x) {
    fprintf(stderr, "*** malloc[%d]: error %d\n", getpid(), x);
#if USE_SLEEP_RATHER_THAN_ABORT
    sleep(3600);
#else
    abort();
#endif
}

void (*malloc_error(void (*func)(int)))(int) {
    return DefaultMallocError;
}

void _malloc_fork_prepare() {
    /* Prepare the malloc module for a fork by insuring that no thread is in a malloc critical section */
    unsigned	index = 0;
    MALLOC_LOCK();
    while (index < malloc_num_zones) {
        malloc_zone_t	*zone = malloc_zones[index++];
        zone->introspect->force_lock(zone);
    }
}

void _malloc_fork_parent() {
    /* Called in the parent process after a fork() to resume normal operation. */
    unsigned	index = 0;
    MALLOC_UNLOCK();
    while (index < malloc_num_zones) {
        malloc_zone_t	*zone = malloc_zones[index++];
        zone->introspect->force_unlock(zone);
    }
}

void _malloc_fork_child() {
    /* Called in the child process after a fork() to resume normal operation.  In the MTASK case we also have to change memory inheritance so that the child does not share memory with the parent. */
    unsigned	index = 0;
    MALLOC_UNLOCK();
    while (index < malloc_num_zones) {
        malloc_zone_t	*zone = malloc_zones[index++];
        zone->introspect->force_unlock(zone);
    }
}

size_t mstats(void) {
    malloc_zone_print(NULL, 0);
    return 1;
}

/*****************	OBSOLETE ENTRY POINTS	********************/

#if PHASE_OUT_OLD_MALLOC
#error PHASE OUT THE FOLLOWING FUNCTIONS
#else
#warning PHASE OUT THE FOLLOWING FUNCTIONS
#endif

void set_malloc_singlethreaded(boolean_t single) {
    static boolean_t warned = 0;
    if (!warned) {
#if PHASE_OUT_OLD_MALLOC
        fprintf(stderr, "*** malloc[%d]: OBSOLETE: set_malloc_singlethreaded(%d)\n", getpid(), single);
#endif
        warned = 1;
    }
}

void malloc_singlethreaded() {
    static boolean_t warned = 0;
    if (!warned) {
        fprintf(stderr, "*** malloc[%d]: OBSOLETE: malloc_singlethreaded()\n", getpid());
        warned = 1;
    }
}

int malloc_debug(int level) {
    fprintf(stderr, "*** malloc[%d]: OBSOLETE: malloc_debug()\n", getpid());
    return 0;
}

/*********	exo_zone - FOR emacs freeze drying	************/

typedef struct {
    malloc_zone_t	basic_zone;
    vm_range_t	range;
    unsigned	flags;
} exo_zone_t;

#define MALLOC_EXO_ZONE_SIZE_IS_OLD	1
#define MALLOC_EXO_ZONE_SIZE_IS_SCALABLE	2

typedef struct {
    unsigned int	lastguyfree:1;
    unsigned int	free:1;
    unsigned int	size:30;
} old_header_t;

#define THIS_FREE			0x8000	// indicates this block is free
#define PREV_FREE			0x4000	// indicates previous block is free
#define	SHIFT_QUANTUM			4

static size_t exo_zone_size(exo_zone_t *zone, const void *ptr) {
    if (zone->flags == MALLOC_EXO_ZONE_SIZE_IS_OLD) {
        if (ptr == (void *)zone->range.address) return zone->range.size;
        return ((old_header_t *)ptr)[-1].size - sizeof(old_header_t);
    }
    if (zone->flags == MALLOC_EXO_ZONE_SIZE_IS_SCALABLE) {
        // this is indeed a valid pointer
        unsigned short		msize_and_free = ((unsigned short *)ptr)[-1];
        return (msize_and_free & THIS_FREE) ? 0 : ((msize_and_free & ~PREV_FREE) << SHIFT_QUANTUM) - sizeof(unsigned short);
    }
    fprintf(stderr, "*** malloc[%d]: exo_zone_size unknown flags()\n", getpid());
    return 0;
}

static void *exo_zone_malloc(exo_zone_t *zone, size_t new_size) {
    return malloc(new_size);
}

static void *exo_zone_calloc(exo_zone_t *zone, size_t num_items, size_t size) {
    return calloc(num_items, size);
}

static void *exo_zone_valloc(exo_zone_t *zone, size_t new_size) {
    return valloc(new_size);
}

static void *exo_zone_realloc(exo_zone_t *zone, void *ptr, size_t new_size) {
    size_t	old_size = exo_zone_size(zone, ptr);
    void	*new_ptr;
    if (new_size <= old_size) return ptr;
    new_ptr = malloc(new_size);
    if (old_size) memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

static void exo_zone_free(exo_zone_t *zone, void *ptr) {
}

static void exo_zone_destroy(exo_zone_t *zone) {
    free(zone);
}

static kern_return_t exo_zone_noop(void) {
    return 0;
}

static size_t exo_zone_good_size(exo_zone_t *zone, size_t size) {
    return size;
}

static struct malloc_introspection_t exo_zone_introspect = {(void *)exo_zone_noop, (void *)exo_zone_good_size, (void *)exo_zone_noop, (void *)exo_zone_noop, (void *)exo_zone_noop, (void *)exo_zone_noop, (void *)exo_zone_noop};

static malloc_zone_t *create_exo_zone(vm_address_t start, vm_size_t size, unsigned flags) {
    exo_zone_t		*zone = calloc(sizeof(exo_zone_t), 1);
    zone->basic_zone.size = (void *)exo_zone_size;
    zone->basic_zone.malloc = (void *)exo_zone_malloc;
    zone->basic_zone.calloc = (void *)exo_zone_calloc;
    zone->basic_zone.valloc = (void *)exo_zone_valloc;
    zone->basic_zone.free = (void *)exo_zone_free;
    zone->basic_zone.realloc = (void *)exo_zone_realloc;
    zone->basic_zone.destroy = (void *)exo_zone_destroy;
    zone->basic_zone.introspect = &exo_zone_introspect;
    zone->flags = flags;
    return (malloc_zone_t *)zone;
}

/*********	Support for emacs FreezeDrying	************/

#define OLD_MALLOC_VERSION  1
#define SCALABLE_MALLOC_VERSION  2

#define MAX_RANGES	1023
typedef struct {
    unsigned	version;
    unsigned	num;
    vm_range_t	ranges[MAX_RANGES];
} malloc_freezedry_ranges;

static void malloc_freezedry_recorder(task_t task, void *rr, unsigned type, vm_range_t *ranges, unsigned num) {
    malloc_freezedry_ranges	*recorded = rr;
    while (num && (recorded->num < MAX_RANGES)) {
        recorded->ranges[recorded->num++] = *ranges;
        ranges++; num--;
    }
    if (recorded->num == MAX_RANGES) fprintf(stderr, "*** malloc[%d]: malloc_freezedry_recorder: exceeding capacity\n", getpid());
}

#define ADDINT(cp,i) (*((int *)cp) = i, cp += sizeof(int))
static kern_return_t default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr) {
    *ptr = (void *)address;
    return 0;
}

int malloc_freezedry(void) {
    malloc_freezedry_ranges	rr;
    unsigned	index = 0;
    char	*cp;
    size_t	size;
    rr.version = SCALABLE_MALLOC_VERSION;
    rr.num = 0;
    while (index < malloc_num_zones) {
        malloc_zone_t	*zone = malloc_zones[index++];
        zone->introspect->enumerator(mach_task_self(), &rr, MALLOC_PTR_REGION_RANGE_TYPE | MALLOC_ADMIN_REGION_RANGE_TYPE, (vm_address_t)zone, default_reader, malloc_freezedry_recorder);
    }
    size = sizeof(rr) - (MAX_RANGES - rr.num) * sizeof(vm_range_t);
    cp = valloc(size);
    memcpy(cp, (void *)&rr, size);
    return (int)cp;
}

#define FETCHINT(i) (i = *((int *)cp), cp += sizeof(int))
int malloc_jumpstart(int cookie) {
    char 	*cp = (char *)cookie;
    int	 	version, regions;
    FETCHINT(version);
    if ((version != OLD_MALLOC_VERSION) && (version != SCALABLE_MALLOC_VERSION)) return 1;
    FETCHINT(regions);
    // fprintf(stderr, "malloc_jumpstart got (version %d): %d regions\n", version, regions);
    while (--regions >= 0) {
        malloc_zone_t	*zone;
        void	**region = (void **)cp;
        if (version == OLD_MALLOC_VERSION) {
            cp += 3 * sizeof(void *);
            // printf("Should add region beginadd=%p endadd=%p zone=%p\n", region[0], region[1], region[2]);
            zone = create_exo_zone((vm_address_t)(region[0]), (vm_size_t)(region[1] - region[0]), MALLOC_EXO_ZONE_SIZE_IS_OLD);
        } else {
            cp += 2 * sizeof(void *);
            // printf("Should add region beginadd=%p endadd=%p zone=%p\n", region[0], region[1], region[2]);
            zone = create_exo_zone((vm_address_t)(region[0]), (vm_size_t)region[1], MALLOC_EXO_ZONE_SIZE_IS_SCALABLE);
        }
        malloc_zone_register(zone);
    }
    return 0;
}

