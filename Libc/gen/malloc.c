/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
#include <pthread_internals.h>

#import <stdlib.h>
#import <stdio.h>
#import <string.h>
#import <unistd.h>
#import <objc/zone.h>
#import <malloc/malloc.h>
#import <fcntl.h>
#import <crt_externs.h>
#import <errno.h>
#import <pthread_internals.h>

#import "scalable_malloc.h"
#import "stack_logging.h"

#define USE_SLEEP_RATHER_THAN_ABORT	0

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

static int malloc_check_sleep = 100; // default 100 second sleep
static int malloc_check_abort = 0; // default is to sleep, not abort

static int malloc_free_abort = 0; // default is not to abort

static int malloc_debug_file;

#define MALLOC_LOCK()		LOCK(_malloc_lock)
#define MALLOC_UNLOCK()		UNLOCK(_malloc_lock)

#define MALLOC_LOG_TYPE_ALLOCATE	stack_logging_type_alloc
#define MALLOC_LOG_TYPE_DEALLOCATE	stack_logging_type_dealloc
#define MALLOC_LOG_TYPE_HAS_ZONE	stack_logging_flag_zone
#define MALLOC_LOG_TYPE_CLEARED		stack_logging_flag_cleared

/*********	Utilities	************/

static inline malloc_zone_t * find_registered_zone(const void *, size_t *) __attribute__((always_inline));
static inline malloc_zone_t *
find_registered_zone(const void *ptr, size_t *returned_size) {
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

static void
_malloc_initialize(void) {
    // guaranteed to be called only once
    (void)malloc_create_zone(0, 0);
    malloc_set_zone_name(malloc_zones[0], "DefaultMallocZone");
    LOCK_INIT(_malloc_lock);
    // malloc_printf("%d registered zones\n", malloc_num_zones);
    // malloc_printf("malloc_zones is at %p; malloc_num_zones is at %p\n", (unsigned)&malloc_zones, (unsigned)&malloc_num_zones);
}

static inline malloc_zone_t *inline_malloc_default_zone(void) __attribute__((always_inline));
static inline malloc_zone_t *
inline_malloc_default_zone(void) {
    if (!malloc_num_zones) _malloc_initialize();
    // malloc_printf("In inline_malloc_default_zone with %d %d\n", malloc_num_zones, malloc_has_debug_zone);
    return malloc_zones[0];
}

malloc_zone_t *
malloc_default_zone(void) {
    return inline_malloc_default_zone();
}

static void
set_flags_from_environment(void) {
    const char *flag;
    int fd;

    flag = getenv("MallocLogFile");
    if (flag) {
	fd = open(flag, O_WRONLY|O_APPEND|O_CREAT, 0644);
	if (fd >= 0) {
           malloc_debug_file = fd;
	   fcntl(fd, F_SETFD, 0); // clear close-on-exec flag  XXX why?
	} else {
	    malloc_printf("Could not open %s, using stderr\n", flag);
	}
    }
    if (getenv("MallocGuardEdges")) {
	malloc_debug_flags = SCALABLE_MALLOC_ADD_GUARD_PAGES;
	malloc_printf("protecting edges\n");
	if (getenv("MallocDoNotProtectPrelude")) {
	    malloc_debug_flags |= SCALABLE_MALLOC_DONT_PROTECT_PRELUDE;
	    malloc_printf("... but not protecting prelude guard page\n");
	}
	if (getenv("MallocDoNotProtectPostlude")) {
	    malloc_debug_flags |= SCALABLE_MALLOC_DONT_PROTECT_POSTLUDE;
	    malloc_printf("... but not protecting postlude guard page\n");
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
	    malloc_printf("recording stacks using standard recorder\n");
	} else {
	    malloc_printf("recording stacks using recorder %p\n", malloc_logger);
	}
	if (stack_logging_dontcompact) malloc_printf("stack logging compaction turned off; VM can increase rapidly\n");
    }
    if (getenv("MallocScribble")) {
	malloc_debug_flags |= SCALABLE_MALLOC_DO_SCRIBBLE;
	malloc_printf("enabling scribbling to detect mods to free blocks\n");
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
	malloc_printf("checks heap after %dth operation and each %d operations\n", malloc_check_start, malloc_check_each);
	flag = getenv("MallocCheckHeapAbort");
	if (flag)
	    malloc_check_abort = strtol(flag, NULL, 0);
	if (malloc_check_abort)
	    malloc_printf("will abort on heap corruption\n");
	else {
	    flag = getenv("MallocCheckHeapSleep");
	    if (flag)
		malloc_check_sleep = strtol(flag, NULL, 0);
	    if (malloc_check_sleep > 0)
		malloc_printf("will sleep for %d seconds on heap corruption\n", malloc_check_sleep);
	    else if (malloc_check_sleep < 0)
		malloc_printf("will sleep once for %d seconds on heap corruption\n", -malloc_check_sleep);
	    else
		malloc_printf("no sleep on heap corruption\n");
	}
    }
    flag = getenv("MallocBadFreeAbort");
    if (flag)
	malloc_free_abort = strtol(flag, NULL, 0);
    if (getenv("MallocHelp")) {
	malloc_printf(
	    "environment variables that can be set for debug:\n"
	    "- MallocLogFile <f> to create/append messages to file <f> instead of stderr\n"
	    "- MallocGuardEdges to add 2 guard pages for each large block\n"
	    "- MallocDoNotProtectPrelude to disable protection (when previous flag set)\n"
	    "- MallocDoNotProtectPostlude to disable protection (when previous flag set)\n"
	    "- MallocStackLogging to record all stacks.  Tools like leaks can then be applied\n"
	    "- MallocStackLoggingNoCompact to record all stacks.  Needed for malloc_history\n"
	    "- MallocScribble to detect writing on free blocks and missing initializers:\n"
	    "  0x55 is written upon free and 0xaa is written on allocation\n"
	    "- MallocCheckHeapStart <n> to start checking the heap after <n> operations\n"
	    "- MallocCheckHeapEach <s> to repeat the checking of the heap after <s> operations\n"
	    "- MallocCheckHeapSleep <t> to sleep <t> seconds on heap corruption\n"
	    "- MallocCheckHeapAbort <b> to abort on heap corruption if <b> is non-zero\n"
	    "- MallocBadFreeAbort <b> to abort on a bad free if <b> is non-zero\n"
	    "- MallocHelp - this help!\n");
    }
}

malloc_zone_t *
malloc_create_zone(vm_size_t start_size, unsigned flags)
{
    malloc_zone_t	*zone;

    if (!malloc_num_zones) {
	char	**env = * _NSGetEnviron();
	char	**p;
	char	*c;

	malloc_debug_file = STDERR_FILENO;
	
	/*
	 * Given that all environment variables start with "Malloc" we optimize by scanning quickly
	 * first the environment, therefore avoiding repeated calls to getenv().
	 * If we are setu/gid these flags are ignored to prevent a malicious invoker from changing
	 * our behaviour.
	 */
	for (p = env; (c = *p) != NULL; ++p) {
	    if (!strncmp(c, "Malloc", 6)) {
		if (!issetugid())
		    set_flags_from_environment(); 
		break;
	    }
	}
    }
    zone = create_scalable_zone(start_size, malloc_debug_flags);
    malloc_zone_register(zone);
    return zone;
}

void
malloc_destroy_zone(malloc_zone_t *zone) {
    malloc_zone_unregister(zone);
    zone->destroy(zone);
}

/*********	Block creation and manipulation	************/

static void
internal_check(void) {
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
	    while (index < num_frames) malloc_printf("%p ", frames[index++]);
	    malloc_printf("\n(Use 'atos' for a symbolic stack)\n");
	}
	if (malloc_check_each > 1) {
	    unsigned	recomm_each = (malloc_check_each > 10) ? malloc_check_each/10 : 1;
	    unsigned	recomm_start = (malloc_check_counter > malloc_check_each+1) ? malloc_check_counter-1-malloc_check_each : 1;
	    malloc_printf("*** Recommend using 'setenv MallocCheckHeapStart %d; setenv MallocCheckHeapEach %d' to narrow down failure\n", recomm_start, recomm_each);
	}
	if (malloc_check_abort)
	    abort();
	if (malloc_check_sleep > 0) {
	    malloc_printf("*** Sleeping for %d seconds to leave time to attach\n",
		malloc_check_sleep);
	    sleep(malloc_check_sleep);
	} else if (malloc_check_sleep < 0) {
	    malloc_printf("*** Sleeping once for %d seconds to leave time to attach\n",
		-malloc_check_sleep);
	    sleep(-malloc_check_sleep);
	    malloc_check_sleep = 0;
	}
    }
    malloc_check_start += malloc_check_each;
}

void *
malloc_zone_malloc(malloc_zone_t *zone, size_t size) {
    void	*ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    ptr = zone->malloc(zone, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, size, 0, (unsigned)ptr, 0);
    return ptr;
}

void *
malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size) {
    void	*ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    ptr = zone->calloc(zone, num_items, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE | MALLOC_LOG_TYPE_CLEARED, (unsigned)zone, num_items * size, 0, (unsigned)ptr, 0);
    return ptr;
}

void *
malloc_zone_valloc(malloc_zone_t *zone, size_t size) {
    void	*ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    ptr = zone->valloc(zone, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, size, 0, (unsigned)ptr, 0);
    return ptr;
}

void *
malloc_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size) {
    void	*new_ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    new_ptr = zone->realloc(zone, ptr, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, (unsigned)ptr, size, (unsigned)new_ptr, 0);
    return new_ptr;
}

void
malloc_zone_free(malloc_zone_t *zone, void *ptr) {
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, (unsigned)ptr, 0, 0, 0);
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    zone->free(zone, ptr);
}

malloc_zone_t *
malloc_zone_from_ptr(const void *ptr) {
    malloc_zone_t	*zone;
    if (!ptr) return NULL;
    zone = find_registered_zone(ptr, NULL);
    return zone;
}

/*********	Functions for zone implementors	************/

void
malloc_zone_register(malloc_zone_t *zone) {
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

void
malloc_zone_unregister(malloc_zone_t *z) {
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
    malloc_printf("*** malloc_zone_unregister() failed for %p\n", z);
}

void
malloc_set_zone_name(malloc_zone_t *z, const char *name) {
    char	*newName;
    if (z->zone_name) {
        free((char *)z->zone_name);
        z->zone_name = NULL;
    }
    newName = malloc_zone_malloc(z, strlen(name) + 1);
    strcpy(newName, name);
    z->zone_name = (const char *)newName;
}

const char *
malloc_get_zone_name(malloc_zone_t *zone) {
    return zone->zone_name;
}

/*
 * XXX malloc_printf now uses _simple_{,v}dprintf.  It only deals with a
 * subset of printf format specifiers, but it doesn't call malloc.
 */
void _simple_dprintf(int, const char *, ...);
void _simple_vdprintf(int, const char *, va_list);

void
malloc_printf(const char *format, ...)
{
    va_list ap;

    if (__is_threaded) {
	/* XXX somewhat rude 'knowing' that pthread_t is a pointer */
	_simple_dprintf(malloc_debug_file, "%s(%d,%p) malloc: ", getprogname(), getpid(), (void *)pthread_self());
    } else {
	_simple_dprintf(malloc_debug_file, "%s(%d) malloc: ", getprogname(), getpid());
    }
    va_start(ap, format);
    _simple_vdprintf(malloc_debug_file, format, ap);
    va_end(ap);
}

/*********	Generic ANSI callouts	************/

void *
malloc(size_t size) {
    void	*retval;
    retval = malloc_zone_malloc(inline_malloc_default_zone(), size);
    if (retval == NULL) {
	errno = ENOMEM;
    }
    return retval;
}

void *
calloc(size_t num_items, size_t size) {
    void	*retval;
    retval = malloc_zone_calloc(inline_malloc_default_zone(), num_items, size);
    if (retval == NULL) {
	errno = ENOMEM;
    }
    return retval;
}

void
free(void *ptr) {
    malloc_zone_t	*zone;
    if (!ptr) return;
    zone = find_registered_zone(ptr, NULL);
    if (zone) {
        malloc_zone_free(zone, ptr);
    } else {
        malloc_printf("***  Deallocation of a pointer not malloced: %p; "
	  "This could be a double free(), or free() called with the middle of an allocated block; "
	  "Try setting environment variable MallocHelp to see tools to help debug\n", ptr);
	if (malloc_free_abort)
	    abort();
    }
}

void *
realloc(void *old_ptr, size_t new_size) {
    void	*retval;
    malloc_zone_t	*zone;
    size_t	old_size = 0;
    if (!old_ptr) {
	retval = malloc_zone_malloc(inline_malloc_default_zone(), new_size);
    } else {
	zone = find_registered_zone(old_ptr, &old_size);
	if (zone && (old_size >= new_size)) return old_ptr;
	if (!zone) zone = inline_malloc_default_zone();
	retval = malloc_zone_realloc(zone, old_ptr, new_size);
    }
    if (retval == NULL) {
	errno = ENOMEM;
    }
    return retval;
}

void *
valloc(size_t size) {
    void	*retval;
    malloc_zone_t	*zone = inline_malloc_default_zone();
    retval = malloc_zone_valloc(zone, size);
    if (retval == NULL) {
	errno = ENOMEM;
    }
    return retval;
}

extern void
vfree(void *ptr) {
    free(ptr);
}

size_t
malloc_size(const void *ptr) {
    size_t	size = 0;
    if (!ptr) return size;
    (void)find_registered_zone(ptr, &size);
    return size;
}

size_t
malloc_good_size (size_t size) {
    malloc_zone_t	*zone = inline_malloc_default_zone();
    return zone->introspect->good_size(zone, size);
}

/*********	Batch methods	************/

unsigned
malloc_zone_batch_malloc(malloc_zone_t *zone, size_t size, void **results, unsigned num_requested) {
    unsigned	(*batch_malloc)(malloc_zone_t *, size_t, void **, unsigned) = zone-> batch_malloc;
    if (! batch_malloc) return 0;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    unsigned	batched = batch_malloc(zone, size, results, num_requested);
    if (malloc_logger) {
	unsigned	index = 0;
	while (index < batched) {
	    malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, size, 0, (unsigned)results[index], 0);
	    index++;
	}
    }
    return batched;
}

void
malloc_zone_batch_free(malloc_zone_t *zone, void **to_be_freed, unsigned num) {
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    if (malloc_logger) {
	unsigned	index = 0;
	while (index < num) {
	    malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (unsigned)zone, (unsigned)to_be_freed[index], 0, 0, 0);
	    index++;
	}
    }
    void	(*batch_free)(malloc_zone_t *, void **, unsigned) = zone-> batch_free;
    if (batch_free) {
	batch_free(zone, to_be_freed, num);
    } else {
	void 	(*free_fun)(malloc_zone_t *, void *) = zone->free;
	while (num--) {
	    void	*ptr = *to_be_freed++;
	    free_fun(zone, ptr);
	}
    }
}

/*********	Functions for performance tools	************/

static kern_return_t
_malloc_default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr) {
    *ptr = (void *)address;
    return 0;
}

kern_return_t
malloc_get_all_zones(task_t task, memory_reader_t reader, vm_address_t **addresses, unsigned *count) {
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
    // printf("Read malloc_zones[%p]=%p\n", remote_malloc_zones, *zones_address_ref);
    if (err) {
        malloc_printf("*** malloc_get_all_zones: error reading zones_address at %p\n", (unsigned)remote_malloc_zones);
        return err;
    }
    zones_address = *zones_address_ref;
    // printf("Reading num_zones at address %p\n", remote_malloc_num_zones);
    err = reader(task, remote_malloc_num_zones, sizeof(unsigned), (void **)&num_zones_ref);
    if (err) {
        malloc_printf("*** malloc_get_all_zones: error reading num_zones at %p\n", (unsigned)remote_malloc_num_zones);
        return err;
    }
    num_zones = *num_zones_ref;
    // printf("Read malloc_num_zones[%p]=%d\n", remote_malloc_num_zones, num_zones);
    *count = num_zones;
    // printf("malloc_get_all_zones succesfully found %d zones\n", num_zones);
    err = reader(task, zones_address, sizeof(malloc_zone_t *) * num_zones, (void **)addresses);
    if (err) {
        malloc_printf("*** malloc_get_all_zones: error reading zones at %p\n", (unsigned)&zones_address);
        return err;
    }
    // printf("malloc_get_all_zones succesfully read %d zones\n", num_zones);
    return err;
}

/*********	Debug helpers	************/

void
malloc_zone_print_ptr_info(void *ptr) {
    malloc_zone_t	*zone;
    if (!ptr) return;
    zone = find_registered_zone(ptr, NULL);
    if (zone) {
        printf("ptr %p in registered zone %p\n", ptr, zone);
    } else {
        printf("ptr %p not in heap\n", ptr);
    }
}

boolean_t
malloc_zone_check(malloc_zone_t *zone) {
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

void
malloc_zone_print(malloc_zone_t *zone, boolean_t verbose) {
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

void
malloc_zone_statistics(malloc_zone_t *zone, malloc_statistics_t *stats) {
    if (!zone) {
	memset(stats, 0, sizeof(stats));
        unsigned	index = 0;
        while (index < malloc_num_zones) {
            zone = malloc_zones[index++];
	    malloc_statistics_t	this_stats;
            zone->introspect->statistics(zone, &this_stats);
	    stats->blocks_in_use += this_stats.blocks_in_use;
	    stats->size_in_use += this_stats.size_in_use;
	    stats->max_size_in_use += this_stats.max_size_in_use;
	    stats->size_allocated += this_stats.size_allocated;
        }
    } else {
	zone->introspect->statistics(zone, stats);
    }
}

void
malloc_zone_log(malloc_zone_t *zone, void *address) {
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

static void
DefaultMallocError(int x) {
    malloc_printf("*** error %d\n", x);
#if USE_SLEEP_RATHER_THAN_ABORT
    sleep(3600);
#else
    abort();
#endif
}

void (*
malloc_error(void (*func)(int)))(int) {
    return DefaultMallocError;
}

void
_malloc_fork_prepare() {
    /* Prepare the malloc module for a fork by insuring that no thread is in a malloc critical section */
    unsigned	index = 0;
    MALLOC_LOCK();
    while (index < malloc_num_zones) {
        malloc_zone_t	*zone = malloc_zones[index++];
        zone->introspect->force_lock(zone);
    }
}

void
_malloc_fork_parent() {
    /* Called in the parent process after a fork() to resume normal operation. */
    unsigned	index = 0;
    MALLOC_UNLOCK();
    while (index < malloc_num_zones) {
        malloc_zone_t	*zone = malloc_zones[index++];
        zone->introspect->force_unlock(zone);
    }
}

void
_malloc_fork_child() {
    /* Called in the child process after a fork() to resume normal operation.  In the MTASK case we also have to change memory inheritance so that the child does not share memory with the parent. */
    unsigned	index = 0;
    MALLOC_UNLOCK();
    while (index < malloc_num_zones) {
        malloc_zone_t	*zone = malloc_zones[index++];
        zone->introspect->force_unlock(zone);
    }
}

/*
 * A Glibc-like mstats() interface.
 *
 * Note that this interface really isn't very good, as it doesn't understand
 * that we may have multiple allocators running at once.  We just massage
 * the result from malloc_zone_statistics in any case.
 */
struct mstats
mstats(void)
{
    malloc_statistics_t s;
    struct mstats m;

    malloc_zone_statistics(NULL, &s);
    m.bytes_total = s.size_allocated;
    m.chunks_used = s.blocks_in_use;
    m.bytes_used = s.size_in_use;
    m.chunks_free = 0;
    m.bytes_free = m.bytes_total - m.bytes_used;	/* isn't this somewhat obvious? */

    return(m);
}

/*****************	OBSOLETE ENTRY POINTS	********************/

#if PHASE_OUT_OLD_MALLOC
#error PHASE OUT THE FOLLOWING FUNCTIONS
#else
#warning PHASE OUT THE FOLLOWING FUNCTIONS
#endif

void
set_malloc_singlethreaded(boolean_t single) {
    static boolean_t warned = 0;
    if (!warned) {
#if PHASE_OUT_OLD_MALLOC
        malloc_printf("*** OBSOLETE: set_malloc_singlethreaded(%d)\n", single);
#endif
        warned = 1;
    }
}

void
malloc_singlethreaded() {
    static boolean_t warned = 0;
    if (!warned) {
        malloc_printf("*** OBSOLETE: malloc_singlethreaded()\n");
        warned = 1;
    }
}

int
malloc_debug(int level) {
    malloc_printf("*** OBSOLETE: malloc_debug()\n");
    return 0;
}
