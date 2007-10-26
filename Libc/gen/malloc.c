/*
 * Copyright (c) 1999, 2006, 2007 Apple Inc. All rights reserved.
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
#import <limits.h>
#import <dlfcn.h>

#import "scalable_malloc.h"
#import "stack_logging.h"
#import "malloc_printf.h"
#import "_simple.h"

/*
 * MALLOC_ABSOLUTE_MAX_SIZE - There are many instances of addition to a
 * user-specified size_t, which can cause overflow (and subsequent crashes)
 * for values near SIZE_T_MAX.  Rather than add extra "if" checks everywhere
 * this occurs, it is easier to just set an absolute maximum request size,
 * and immediately return an error if the requested size exceeds this maximum.
 * Of course, values less than this absolute max can fail later if the value
 * is still too large for the available memory.  The largest value added
 * seems to be PAGE_SIZE (in the macro round_page()), so to be safe, we set
 * the maximum to be 2 * PAGE_SIZE less than SIZE_T_MAX.
 */
#define MALLOC_ABSOLUTE_MAX_SIZE	(SIZE_T_MAX - (2 * PAGE_SIZE))

#define USE_SLEEP_RATHER_THAN_ABORT	0

#define INITIAL_ZONES	8  // After this number, we reallocate for new zones

typedef void (malloc_logger_t)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t num_hot_frames_to_skip);

__private_extern__ pthread_lock_t _malloc_lock = 0; // initialized in __libc_init
static malloc_zone_t *initial_malloc_zones[INITIAL_ZONES] = {0};

/* The following variables are exported for the benefit of performance tools */
unsigned malloc_num_zones = 0;
malloc_zone_t **malloc_zones = initial_malloc_zones;
malloc_logger_t *malloc_logger = NULL;

unsigned malloc_debug_flags = 0;

unsigned malloc_check_start = 0; // 0 means don't check
unsigned malloc_check_counter = 0;
unsigned malloc_check_each = 1000;

/* global flag to suppress ASL logging e.g. for syslogd */
int _malloc_no_asl_log = 0;

static int malloc_check_sleep = 100; // default 100 second sleep
static int malloc_check_abort = 0; // default is to sleep, not abort

static int malloc_debug_file = STDERR_FILENO;
/*
 * State indicated by malloc_def_zone_state
 * 0 - the default zone has not yet been created
 * 1 - a Malloc* environment variable has been set
 * 2 - the default zone has been created and an environment variable scan done
 * 3 - a new default zone has been created and another environment variable scan
 */
__private_extern__ int malloc_def_zone_state = 0;
__private_extern__ malloc_zone_t *__zone0 = NULL;

static const char Malloc_Facility[] = "com.apple.Libsystem.malloc";

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
    // Returns a zone which may contain ptr, or NULL.
    // Speed is critical for this function, so it is not guaranteed to return
    // the zone which contains ptr.  For N zones, zones 1 through N - 1 are
    // checked to see if they contain ptr.  If so, the zone containing ptr is
    // returned.  Otherwise the last zone is returned, since it is the last zone
    // in which ptr may reside.  Clients should call zone->size(ptr) on the
    // return value to determine whether or not ptr is an allocated object.
    // This behavior optimizes for the case where ptr is an allocated object,
    // and there is only one zone.
    unsigned index, limit = malloc_num_zones;
    if (limit == 0)
        return NULL;
    
    malloc_zone_t	**zones = malloc_zones;
    for (index = 0; index < limit - 1; ++index, ++zones) {
        malloc_zone_t *zone = *zones;
        size_t size = zone->size(zone, ptr);
        if (size) {
            if (returned_size) *returned_size = size;
            return zone;
        }
    }
    return malloc_zones[index];
}

__private_extern__ __attribute__((noinline)) void
malloc_error_break(void) {
	// Provides a non-inlined place for various malloc error procedures to call
	// that will be called after an error message appears.  It does not make
	// sense for developers to call this function, so it is marked
	// __private_extern__ to prevent it from becoming API.
}

/*********	Creation and destruction	************/

static void set_flags_from_environment(void);

// malloc_zone_register_while_locked may drop the lock temporarily
static void
malloc_zone_register_while_locked(malloc_zone_t *zone) {
    /* Note that given the sequencing it is always safe to first get the number of zones, then get malloc_zones without taking the lock, if all you need is to iterate through the list */
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
    // _malloc_printf(ASL_LEVEL_INFO, "Registered %p malloc_zones at address %p is %p [%d zones]\n", zone, &malloc_zones, malloc_zones, malloc_num_zones);
}

static void
_malloc_initialize(void) {
    MALLOC_LOCK();
    if (malloc_def_zone_state < 2) {
	unsigned n;
	malloc_zone_t	*zone;

	malloc_def_zone_state += 2;
	set_flags_from_environment(); // will only set flags up to two times
	n = malloc_num_zones;
	zone = create_scalable_zone(0, malloc_debug_flags);
	//malloc_zone_register_while_locked may drop the lock temporarily
	malloc_zone_register_while_locked(zone);
	malloc_set_zone_name(zone, "DefaultMallocZone");
	if (n != 0) { // make the default first, for efficiency
	    malloc_zone_t *hold = malloc_zones[0];
	    if(hold->zone_name && strcmp(hold->zone_name, "DefaultMallocZone") == 0) {
		free((void *)hold->zone_name);
		hold->zone_name = NULL;
	    }
	    malloc_zones[0] = malloc_zones[n];
	    malloc_zones[n] = hold;
	}
	// _malloc_printf(ASL_LEVEL_INFO, "%d registered zones\n", malloc_num_zones);
	// _malloc_printf(ASL_LEVEL_INFO, "malloc_zones is at %p; malloc_num_zones is at %p\n", (unsigned)&malloc_zones, (unsigned)&malloc_num_zones);
    }
    MALLOC_UNLOCK();
}

static inline malloc_zone_t *inline_malloc_default_zone(void) __attribute__((always_inline));
static inline malloc_zone_t *
inline_malloc_default_zone(void) {
    if (malloc_def_zone_state < 2) _malloc_initialize();
    // _malloc_printf(ASL_LEVEL_INFO, "In inline_malloc_default_zone with %d %d\n", malloc_num_zones, malloc_has_debug_zone);
    return malloc_zones[0];
}

malloc_zone_t *
malloc_default_zone(void) {
    return inline_malloc_default_zone();
}

// For debugging, allow stack logging to both memory and disk to compare their results.
static void
stack_logging_log_stack_debug(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t size, uintptr_t ptr_arg, uintptr_t return_val, uint32_t num_hot_to_skip)
{
    __disk_stack_logging_log_stack(type_flags, zone_ptr, size, ptr_arg, return_val, num_hot_to_skip);
    stack_logging_log_stack(type_flags, zone_ptr, size, ptr_arg, return_val, num_hot_to_skip);
}

static void
set_flags_from_environment(void) {
    const char	*flag;
    int		fd;
    char	**env = * _NSGetEnviron();
    char	**p;
    char	*c;

    if (malloc_debug_file != STDERR_FILENO) {
	close(malloc_debug_file);
	malloc_debug_file = STDERR_FILENO;
    }
    malloc_debug_flags = 0;
    stack_logging_enable_logging = 0;
    stack_logging_dontcompact = 0;
    malloc_logger = NULL;
    malloc_check_start = 0;
    malloc_check_each = 1000;
    malloc_check_abort = 0;
    malloc_check_sleep = 100;
    /*
     * Given that all environment variables start with "Malloc" we optimize by scanning quickly
     * first the environment, therefore avoiding repeated calls to getenv().
     * If we are setu/gid these flags are ignored to prevent a malicious invoker from changing
     * our behaviour.
     */
    for (p = env; (c = *p) != NULL; ++p) {
	if (!strncmp(c, "Malloc", 6)) {
	    if (issetugid())
		return;
	    break;
	}
    }
    if (c == NULL)
	return;
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
	_malloc_printf(ASL_LEVEL_INFO, "protecting edges\n");
	if (getenv("MallocDoNotProtectPrelude")) {
	    malloc_debug_flags |= SCALABLE_MALLOC_DONT_PROTECT_PRELUDE;
	    _malloc_printf(ASL_LEVEL_INFO, "... but not protecting prelude guard page\n");
	}
	if (getenv("MallocDoNotProtectPostlude")) {
	    malloc_debug_flags |= SCALABLE_MALLOC_DONT_PROTECT_POSTLUDE;
	    _malloc_printf(ASL_LEVEL_INFO, "... but not protecting postlude guard page\n");
	}
    }
    flag = getenv("MallocStackLogging");
    if (!flag) {
	flag = getenv("MallocStackLoggingNoCompact");
	stack_logging_dontcompact = 1;
    } 
    // For debugging, the MallocStackLogging or MallocStackLoggingNoCompact environment variables can be set to
    // values of "memory", "disk", or "both" to control which stack logging mechanism to use.  Those strings appear
    // in the flag variable, and the strtoul() call below will return 0, so then we can do string comparison on the
    // value of flag.  The default stack logging now is disk stack logging, since memory stack logging is not 64-bit-aware.
    if (flag) {
	unsigned long val = strtoul(flag, NULL, 0);
	if (val == 1) val = 0;
	if (val == -1) val = 0;
	if (val) {
	    malloc_logger = (void *)val;
	    _malloc_printf(ASL_LEVEL_INFO, "recording stacks using recorder %p\n", malloc_logger);
	} else if (strcmp(flag,"memory") == 0) {
	    malloc_logger = stack_logging_log_stack;
	    _malloc_printf(ASL_LEVEL_INFO, "recording malloc stacks in memory using standard recorder\n");
	} else if (strcmp(flag,"both") == 0) {
	    malloc_logger = stack_logging_log_stack_debug;
	    _malloc_printf(ASL_LEVEL_INFO, "recording malloc stacks to both memory and disk for comparison debugging\n");
	} else {	// the default is to log to disk
	    malloc_logger = __disk_stack_logging_log_stack;
	    _malloc_printf(ASL_LEVEL_INFO, "recording malloc stacks to disk using standard recorder\n");
	}
	stack_logging_enable_logging = 1;
	if (stack_logging_dontcompact) {
	    if (malloc_logger == __disk_stack_logging_log_stack) {
	      _malloc_printf(ASL_LEVEL_INFO, "stack logging compaction turned off; size of log files on disk can increase rapidly\n");
	    } else {
	      _malloc_printf(ASL_LEVEL_INFO, "stack logging compaction turned off; VM can increase rapidly\n");
	    }
	}
    }
    if (getenv("MallocScribble")) {
	malloc_debug_flags |= SCALABLE_MALLOC_DO_SCRIBBLE;
	_malloc_printf(ASL_LEVEL_INFO, "enabling scribbling to detect mods to free blocks\n");
    }
    if (getenv("MallocErrorAbort")) {
	malloc_debug_flags |= SCALABLE_MALLOC_ABORT_ON_ERROR;
	_malloc_printf(ASL_LEVEL_INFO, "enabling abort() on bad malloc or free\n");
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
	_malloc_printf(ASL_LEVEL_INFO, "checks heap after %dth operation and each %d operations\n", malloc_check_start, malloc_check_each);
	flag = getenv("MallocCheckHeapAbort");
	if (flag)
	    malloc_check_abort = strtol(flag, NULL, 0);
	if (malloc_check_abort)
	    _malloc_printf(ASL_LEVEL_INFO, "will abort on heap corruption\n");
	else {
	    flag = getenv("MallocCheckHeapSleep");
	    if (flag)
		malloc_check_sleep = strtol(flag, NULL, 0);
	    if (malloc_check_sleep > 0)
		_malloc_printf(ASL_LEVEL_INFO, "will sleep for %d seconds on heap corruption\n", malloc_check_sleep);
	    else if (malloc_check_sleep < 0)
		_malloc_printf(ASL_LEVEL_INFO, "will sleep once for %d seconds on heap corruption\n", -malloc_check_sleep);
	    else
		_malloc_printf(ASL_LEVEL_INFO, "no sleep on heap corruption\n");
	}
    }
    if (getenv("MallocHelp")) {
	_malloc_printf(ASL_LEVEL_INFO,
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
	    "- MallocErrorAbort to abort on a bad malloc or free\n"
	    "- MallocHelp - this help!\n");
    }
}

malloc_zone_t *
malloc_create_zone(vm_size_t start_size, unsigned flags)
{
    malloc_zone_t	*zone;

    /* start_size doesn't seemed to actually be used, but we test anyways */
    if (start_size > MALLOC_ABSOLUTE_MAX_SIZE) {
	return NULL;
    }
    if (malloc_def_zone_state < 2) _malloc_initialize();
    zone = create_scalable_zone(start_size, malloc_debug_flags);
    malloc_zone_register(zone);
    return zone;
}

void
malloc_destroy_zone(malloc_zone_t *zone) {
    malloc_zone_unregister(zone);
    zone->destroy(zone);
}

/* called from the {put,set,unset}env routine */
__private_extern__ void
__malloc_check_env_name(const char *name)
{
    MALLOC_LOCK();
    if(malloc_def_zone_state == 2 && strncmp(name, "Malloc", 6) == 0)
	malloc_def_zone_state = 1;
    MALLOC_UNLOCK();
}

/*********	Block creation and manipulation	************/

static void
internal_check(void) {
    static vm_address_t	*frames = NULL;
    static unsigned	num_frames;
    if (malloc_zone_check(NULL)) {
	_malloc_printf(ASL_LEVEL_NOTICE, "MallocCheckHeap: PASSED check at %dth operation\n", malloc_check_counter-1);
	if (!frames) vm_allocate(mach_task_self(), (void *)&frames, vm_page_size, 1);
	thread_stack_pcs(frames, vm_page_size/sizeof(vm_address_t) - 1, &num_frames);
    } else {
	malloc_printf("*** MallocCheckHeap: FAILED check at %dth operation\n", malloc_check_counter-1);
	if (frames) {
	    unsigned	index = 1;
	    _SIMPLE_STRING b = _simple_salloc();
	    if (b) {
		_simple_sappend(b, "Stack for last operation where the malloc check succeeded: ");
		while (index < num_frames) _simple_sprintf(b, "%p ", frames[index++]);
		malloc_printf("%s\n(Use 'atos' for a symbolic stack)\n", _simple_string(b));
		_simple_sfree(b);
	    } else {
		/*
		 * Should only get here if vm_allocate() can't get a single page of
		 * memory, implying _simple_asl_log() would also fail.  So we just
		 * print to the file descriptor.
		 */
		_malloc_printf(MALLOC_PRINTF_NOLOG, "Stack for last operation where the malloc check succeeded: ");
		while (index < num_frames) _malloc_printf(MALLOC_PRINTF_NOLOG, "%p ", frames[index++]);
		_malloc_printf(MALLOC_PRINTF_NOLOG, "\n(Use 'atos' for a symbolic stack)\n");
	    }
	}
	if (malloc_check_each > 1) {
	    unsigned	recomm_each = (malloc_check_each > 10) ? malloc_check_each/10 : 1;
	    unsigned	recomm_start = (malloc_check_counter > malloc_check_each+1) ? malloc_check_counter-1-malloc_check_each : 1;
	    malloc_printf("*** Recommend using 'setenv MallocCheckHeapStart %d; setenv MallocCheckHeapEach %d' to narrow down failure\n", recomm_start, recomm_each);
	}
	if (malloc_check_abort)
	    abort();
	if (malloc_check_sleep > 0) {
	    _malloc_printf(ASL_LEVEL_NOTICE, "*** Sleeping for %d seconds to leave time to attach\n",
		malloc_check_sleep);
	    sleep(malloc_check_sleep);
	} else if (malloc_check_sleep < 0) {
	    _malloc_printf(ASL_LEVEL_NOTICE, "*** Sleeping once for %d seconds to leave time to attach\n",
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
    if (size > MALLOC_ABSOLUTE_MAX_SIZE) {
	return NULL;
    }
    ptr = zone->malloc(zone, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)size, 0, (uintptr_t)ptr, 0);
    return ptr;
}

void *
malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size) {
    void	*ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    if (size > MALLOC_ABSOLUTE_MAX_SIZE) {
	return NULL;
    }
    ptr = zone->calloc(zone, num_items, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE | MALLOC_LOG_TYPE_CLEARED, (uintptr_t)zone, (uintptr_t)(num_items * size), 0, (uintptr_t)ptr, 0);
    return ptr;
}

void *
malloc_zone_valloc(malloc_zone_t *zone, size_t size) {
    void	*ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    if (size > MALLOC_ABSOLUTE_MAX_SIZE) {
	return NULL;
    }
    ptr = zone->valloc(zone, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)size, 0, (uintptr_t)ptr, 0);
    return ptr;
}

void *
malloc_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size) {
    void	*new_ptr;
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    if (size > MALLOC_ABSOLUTE_MAX_SIZE) {
	return NULL;
    }
    new_ptr = zone->realloc(zone, ptr, size);
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)ptr, (uintptr_t)size, (uintptr_t)new_ptr, 0);
    return new_ptr;
}

void
malloc_zone_free(malloc_zone_t *zone, void *ptr) {
    if (malloc_logger) malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)ptr, 0, 0, 0);
    if (malloc_check_start && (malloc_check_counter++ >= malloc_check_start)) {
	internal_check();
    }
    zone->free(zone, ptr);
}

malloc_zone_t *
malloc_zone_from_ptr(const void *ptr) {
    malloc_zone_t	*zone;
    if (!ptr)
        return NULL;
    zone = find_registered_zone(ptr, NULL);
    if (zone && zone->size(zone, ptr))
    return zone;
    return NULL;
}

/*********	Functions for zone implementors	************/

void
malloc_zone_register(malloc_zone_t *zone) {
    MALLOC_LOCK();
    malloc_zone_register_while_locked(zone);
    MALLOC_UNLOCK();
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
 * XXX malloc_printf now uses _simple_*printf.  It only deals with a
 * subset of printf format specifiers, but it doesn't call malloc.
 */

__private_extern__ void
_malloc_vprintf(int flags, const char *format, va_list ap)
{
    _SIMPLE_STRING b;

    if (_malloc_no_asl_log || (flags & MALLOC_PRINTF_NOLOG) || (b = _simple_salloc()) == NULL) {
	if (!(flags & MALLOC_PRINTF_NOPREFIX)) {
	    if (__is_threaded) {
		/* XXX somewhat rude 'knowing' that pthread_t is a pointer */
		_simple_dprintf(malloc_debug_file, "%s(%d,%p) malloc: ", getprogname(), getpid(), (void *)pthread_self());
	    } else {
		_simple_dprintf(malloc_debug_file, "%s(%d) malloc: ", getprogname(), getpid());
	    }
	}
	_simple_vdprintf(malloc_debug_file, format, ap);
	return;
    }
    if (!(flags & MALLOC_PRINTF_NOPREFIX)) {
	if (__is_threaded) {
	    /* XXX somewhat rude 'knowing' that pthread_t is a pointer */
	    _simple_sprintf(b, "%s(%d,%p) malloc: ", getprogname(), getpid(), (void *)pthread_self());
	} else {
	    _simple_sprintf(b, "%s(%d) malloc: ", getprogname(), getpid());
	}
    }
    _simple_vsprintf(b, format, ap);
    _simple_put(b, malloc_debug_file);
    _simple_asl_log(flags & MALLOC_PRINTF_LEVEL_MASK, Malloc_Facility, _simple_string(b));
    _simple_sfree(b);
}

__private_extern__ void
_malloc_printf(int flags, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    _malloc_vprintf(flags, format, ap);
    va_end(ap);
}

void
malloc_printf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    _malloc_vprintf(ASL_LEVEL_ERR, format, ap);
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
    if (zone)
        malloc_zone_free(zone, ptr);
}

void *
realloc(void *in_ptr, size_t new_size) {
    void		*retval;
    void		*old_ptr;
    malloc_zone_t	*zone;
    size_t		old_size = 0;

    // SUSv3: "If size is 0 and ptr is not a null pointer, the object
    // pointed to is freed. If the space cannot be allocated, the object
    // shall remain unchanged."  Also "If size is 0, either a null pointer
    // or a unique pointer that can be successfully passed to free() shall
    // be returned."  We choose to allocate a minimum size object by calling
    // malloc_zone_malloc with zero size, which matches "If ptr is a null
    // pointer, realloc() shall be equivalent to malloc() for the specified
    // size."  So we only free the original memory if the allocation succeeds.
    old_ptr = (new_size == 0) ? NULL : in_ptr;
    if (!old_ptr) {
	retval = malloc_zone_malloc(inline_malloc_default_zone(), new_size);
    } else {
	zone = find_registered_zone(old_ptr, &old_size);
        if (zone && (old_size == 0))
            old_size = zone->size(zone, old_ptr);
        if (zone && (old_size >= new_size)) 
            return old_ptr;
        /*
         * if old_size is still 0 here, it means that either zone was NULL or
         * the call to zone->size() returned 0, indicating the pointer is not
         * not in that zone.  In this case, just use the default zone.
         */
        if (old_size == 0)
            zone = inline_malloc_default_zone();
	retval = malloc_zone_realloc(zone, old_ptr, new_size);
    }
    if (retval == NULL) {
	errno = ENOMEM;
    } else if (new_size == 0) {
	free(in_ptr);
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
    malloc_zone_t *zone = find_registered_zone(ptr, &size);
    /*
     * If we found a zone, and size is 0 then we need to check to see if that
     * zone contains ptr.  If size is nonzero, then we know zone contains ptr.
     */
    if (zone && (size == 0))
        size = zone->size(zone, ptr);
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
	    malloc_logger(MALLOC_LOG_TYPE_ALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)size, 0, (uintptr_t)results[index], 0);
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
	    malloc_logger(MALLOC_LOG_TYPE_DEALLOCATE | MALLOC_LOG_TYPE_HAS_ZONE, (uintptr_t)zone, (uintptr_t)to_be_freed[index], 0, 0, 0);
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
    zone = malloc_zone_from_ptr(ptr);
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
	memset(stats, 0, sizeof(*stats));
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
