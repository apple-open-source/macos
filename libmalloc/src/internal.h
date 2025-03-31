/*
 * Copyright (c) 2015-2023 Apple Inc. All rights reserved.
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

#ifndef __INTERNAL_H
#define __INTERNAL_H

// Toggles for fixes for specific Radars. If we get enough of these, we
// probably should create a separate header file for them.
#define RDAR_48993662 1

#ifndef OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY
# error "Must set OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY in GCC_PREPROCESSOR_DEFINITIONS"
#endif // OS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY

#include <malloc/_ptrcheck.h>
__ptrcheck_abi_assume_single()

#include <malloc/_platform.h>
#include <Availability.h>
#include <TargetConditionals.h>

#include "platform.h" // build configuration macros

#if !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR && defined(DEBUG)
// Internal to sys/queue.h, will change the size of its data structures
#define QUEUE_MACRO_DEBUG
#endif // !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR && defined(DEBUG)

#if !MALLOC_TARGET_EXCLAVES
# include <_simple.h>
# include <platform/string.h>
# undef memcpy
# define memcpy _platform_memmove
# define _malloc_memcmp_zero_aligned8 _platform_memcmp_zero_aligned8
# include <platform/compat.h>
#else
enum {
	ASL_LEVEL_EMERG = 0,
	ASL_LEVEL_ALERT = 1,
	ASL_LEVEL_CRIT = 2,
	ASL_LEVEL_ERR = 3,
	ASL_LEVEL_WARNING = 4,
	ASL_LEVEL_NOTICE = 5,
	ASL_LEVEL_INFO = 6,
	ASL_LEVEL_DEBUG = 7,
};

typedef void * _SIMPLE_STRING;
#endif // !MALLOC_TARGET_EXCLAVES

#include <assert.h>
#if !MALLOC_TARGET_EXCLAVES
# include <crt_externs.h>
# include <dirent.h>
#endif // !MALLOC_TARGET_EXCLAVES
#include <errno.h>
#include <execinfo.h>
#if !MALLOC_TARGET_EXCLAVES
# include <netinet/in.h> // Workaround for rdar://109946019
# include <libc.h>
#endif // !MALLOC_TARGET_EXCLAVES
#include <limits.h>
#if !MALLOC_TARGET_EXCLAVES
# include <libkern/OSAtomic.h>
#endif // !MALLOC_TARGET_EXCLAVES

#if !TARGET_OS_EXCLAVECORE
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>
#endif // !TARGET_OS_EXCLAVECORE

typedef struct {
	uint64_t opaque[2];
} plat_map_exclaves_t;
#if !MALLOC_TARGET_EXCLAVES
# include <mach/error.h>
# include <mach/mach.h>
# include <mach/mach_init.h>
# include <mach/mach_time.h>
# include <mach/mach_types.h>
# include <mach/mach_vm.h>
# include <mach/shared_region.h>
# include <mach/thread_switch.h>
# include <mach/vm_map.h>
# include <mach/vm_page_size.h>
# include <mach/vm_param.h>
# include <mach/vm_reclaim.h>
# include <mach/vm_reclaim_private.h>
# include <mach/vm_statistics.h>
# include <machine/cpu_capabilities.h>

# if !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
typedef void plat_map_t;
# else
#  if !defined(__x86_64__)
#   include <l4/l4.h>
#  endif // !defined(__x86_64__)
typedef plat_map_exclaves_t plat_map_t;
# endif // !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#else
# include <liblibc/system.h>
# include <l4/l4.h>
# define PAGE_SHIFT L4_PageSizeBits
# define PAGE_SIZE L4_PageSize
# define PAGE_MAX_SIZE L4_PageSize
# define vm_page_size PAGE_SIZE
# define PAGE_MASK (PAGE_SIZE-1)

typedef uint64_t mach_vm_reclaim_id_t;
# define mach_vm_round_page(x) (((mach_vm_offset_t)(x) + PAGE_MASK) & ~((signed)PAGE_MASK))
# define round_page(x) (((vm_offset_t)(x) + PAGE_MASK) & ~((vm_offset_t)PAGE_MASK))

# define KERN_SUCCESS 0
# define KERN_FAILURE 5
# define KERN_NOT_SUPPORTED 46

# define MACH_PORT_NULL NULL

# define VM_FLAGS_FIXED 0
# define VM_FLAGS_ANYWHERE 1

# define VM_MEMORY_MALLOC 1

typedef struct _liblibc_plat_mem_map_t plat_map_t;
_Static_assert(sizeof(plat_map_exclaves_t) == sizeof(plat_map_t),
	"plat_map_t size must match that on exclaves");
#endif // !MALLOC_TARGET_EXCLAVES

#define CHECK_REGIONS (1 << 31)
#define DISABLE_ASLR (1 << 30)
#define DISABLE_LARGE_ASLR (1 << 29)

#include <os/atomic_private.h>
#include <os/base_private.h>
#if !MALLOC_TARGET_EXCLAVES
# include <os/crashlog_private.h>
# include <os/lock_private.h>
# include <os/once_private.h>
#else
# include <_liblibc/_error.h>
#endif // !MALLOC_TARGET_EXCLAVES
#include <os/overflow.h>
#if !TARGET_OS_DRIVERKIT && !MALLOC_TARGET_EXCLAVES
# include <os/feature_private.h>
#endif // !TARGET_OS_DRIVERKIT && !MALLOC_TARGET_EXCLAVES
#if !MALLOC_TARGET_EXCLAVES
# include <os/tsd.h>
#endif // !MALLOC_TARGET_EXCLAVES

#if !MALLOC_TARGET_EXCLAVES
# include <paths.h>
# include <pthread/pthread.h>  // _pthread_threadid_self_np_direct()
# include <pthread/private.h>  // _pthread_threadid_self_np_direct()
# include <pthread/tsd_private.h>  // TSD keys
#else
# include <platform/platform.h>
# include <pthread.h>
#endif // !MALLOC_TARGET_EXCLAVES

#include <ptrauth.h>

#if !MALLOC_TARGET_EXCLAVES
# include <signal.h>
#endif // !MALLOC_TARGET_EXCLAVES
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#if !MALLOC_TARGET_EXCLAVES
# include <struct.h>
#else
# define countof(arr) ({ \
	_Static_assert( \
					!__builtin_types_compatible_p(typeof(arr), typeof(&(arr)[0])), \
					"array must be statically defined"); \
	(sizeof(arr) / sizeof(arr[0])); \
})
# define countof_unsafe(arr) \
    (sizeof(arr) / sizeof(arr[0]))
#endif // !MALLOC_TARGET_EXCLAVES
#include <sys/cdefs.h>
#ifndef __probable
# define __probable(x) __builtin_expect(!!(x),1)
#endif // __probable
#include <sys/mman.h>
#include <sys/queue.h>
#if !MALLOC_TARGET_EXCLAVES
# include <sys/codesign.h>
# include <sys/event.h>
# include <sys/param.h>
# include <sys/stat.h>
# include <sys/sysctl.h>
# include <sys/random.h>
# include <sys/csr.h>
#else
# include <vas/vas.h>
# define howmany(x, y)   ((((x) % (y)) == 0) ? ((x) / (y)) : (((x) / (y)) + 1))
# define powerof2(x)     ((((x)-1)&(x))==0)
# define roundup(x, y) ((((x) % (y)) == 0) ? \
					   (x) : ((x) + ((y) - ((x) % (y)))))
#endif // !MALLOC_TARGET_EXCLAVES
#define rounddown(x,y)  (((x)/(y))*(y))
#include <sys/types.h>
#if !MALLOC_TARGET_EXCLAVES
# include <sys/ulock.h>
# include <sys/vmparam.h>
# include <thread_stack_pcs.h>
#endif // !MALLOC_TARGET_EXCLAVES
#include <unistd.h>
#if !MALLOC_TARGET_EXCLAVES
# include <xlocale.h>
#endif // !MALLOC_TARGET_EXCLAVES

#if !MALLOC_TARGET_EXCLAVES
// pthread reserves 5 TSD keys for libmalloc
# define __TSD_MALLOC_PROB_GUARD_SAMPLE_COUNTER __PTK_LIBMALLOC_KEY0
# define __TSD_MALLOC_ZERO_CORRUPTION_COUNTER   __PTK_LIBMALLOC_KEY1
# define __TSD_MALLOC_THREAD_OPTIONS            __PTK_LIBMALLOC_KEY2
# define __TSD_MALLOC_TYPE_DESCRIPTOR           __PTK_LIBMALLOC_KEY3
# define __TSD_MALLOC_XZONE_THREAD_CACHE        __PTK_LIBMALLOC_KEY4
#else
# include "liblibc_overrides.h"
# define __TSD_MALLOC_TYPE_DESCRIPTOR           __LIBLIBC_XZONE_TSS_KEY
#endif // !MALLOC_TARGET_EXCLAVES

#if !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
# include "dtrace.h"
#endif // !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#include "base.h" // MALLOC_NOEXPORT
#if !MALLOC_TARGET_EXCLAVES
# include "trace.h"
#endif // !MALLOC_TARGET_EXCLAVES
#include "debug.h"
#include "printf.h" // malloc_zone_error
#include "locking.h"
#if !MALLOC_TARGET_EXCLAVES
# include "bitarray.h"
#endif // !MALLOC_TARGET_EXCLAVES
#include "malloc/malloc.h"
#include "early_malloc.h"
#include "instrumentation.h"
#if !MALLOC_TARGET_EXCLAVES
# include "frozen_malloc.h"
# include "legacy_malloc.h"
# include "magazine_malloc.h"
#endif // !MALLOC_TARGET_EXCLAVES
#include "malloc_common.h"
#if !MALLOC_TARGET_EXCLAVES
# include "nano_malloc_common.h"
# include "nanov2_malloc.h"
# include "pgm_malloc.h"
#endif // !MALLOC_TARGET_EXCLAVES
#include "sanitizer_malloc.h"
#if !MALLOC_TARGET_EXCLAVES
# include "purgeable_malloc.h"
#endif // !MALLOC_TARGET_EXCLAVES
#include "malloc_private.h"
#include "malloc/_malloc_type.h"  // public
#include "malloc_type_private.h"  // private
#include "thresholds.h"
#include "vm.h"
#include "malloc_type_internal.h"
#if !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
# include "magazine_rack.h"
# include "magazine_zone.h"
# include "nano_zone_common.h"
# include "nano_zone.h"
# include "nanov2_zone.h"
# include "magazine_inline.h"
#endif // !MALLOC_TARGET_EXCLAVES && MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#include "xzone/xzone_introspect.h"
#include "xzone/xzone_malloc.h"
#if !(MALLOC_TARGET_EXCLAVES_INTROSPECTOR && defined(__x86_64__))
# include "xzone/xzone_inline_internal.h"
#endif // !(MALLOC_TARGET_EXCLAVES_INTROSPECTOR && defined(__x86_64__))
#if !MALLOC_TARGET_EXCLAVES
# include "stack_logging.h"
# include "stack_trace.h"
#endif // !MALLOC_TARGET_EXCLAVES
#include "malloc_implementation.h"

#pragma mark Memory Pressure Notification Masks

/* We will madvise unused memory on pressure warnings if either:
 *  - freed pages are not aggressively madvised by default
 *  - the large cache is enabled (and not enrolled in deferred reclamation)
 */
#if CONFIG_MADVISE_PRESSURE_RELIEF || (CONFIG_LARGE_CACHE && !CONFIG_MAGAZINE_DEFERRED_RECLAIM)
#define MALLOC_MEMORYSTATUS_MASK_PRESSURE_RELIEF ( \
		NOTE_MEMORYSTATUS_PRESSURE_WARN | \
		NOTE_MEMORYSTATUS_PRESSURE_NORMAL)
#else /* CONFIG_MADVISE_PRESSURE_RELIEF || (CONFIG_LARGE_CACHE && !CONFIG_MAGAZINE_DEFERRED_RECLAIM) */
#define MALLOC_MEMORYSTATUS_MASK_PRESSURE_RELIEF 0
#endif

/*
 * Resource Exception Reports are generated on process limits and
 * system-critical memory pressure.
 */
#if ENABLE_MEMORY_RESOURCE_EXCEPTION_HANDLING
#define MALLOC_MEMORYSTATUS_MASK_RESOURCE_EXCEPTION_HANDLING ( \
		NOTE_MEMORYSTATUS_PROC_LIMIT_WARN | \
		NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL | \
		NOTE_MEMORYSTATUS_PRESSURE_CRITICAL )
#else /* ENABLE_MEMORY_RESOURCE_EXCEPTION_HANDLING */
#define MALLOC_MEMORYSTATUS_MASK_RESOURCE_EXCEPTION_HANDLING 0
#endif

/* MallocStackLogging.framework notification dependencies */
#define MSL_MEMORYPRESSURE_MASK ( NOTE_MEMORYSTATUS_PROC_LIMIT_WARN | \
		NOTE_MEMORYSTATUS_PROC_LIMIT_CRITICAL | \
		NOTE_MEMORYSTATUS_PRESSURE_CRITICAL )

/*
 * By default, libdispatch will register eligible processes for memory-pressure
 * notifications and register a notification handler. libdispatch's
 * notification handler will then call into malloc to return unused memory (see
 * `malloc_memory_event_handler()`). We export a mask to libdispatch so that it
 * will only register for notifications for which malloc is prepared to respond
 * to. Because MallocStackLogging.framework relies on its own subset of
 * notifications, we export two masks. libdispatch will initially register for
 * notifications of the `_DEFAULT` flavor. If MallocStackLogging.framework is
 * subsequently enabled, libdispatch will reregister for notifications with
 * the `_MSL` mask.
 */
#define MALLOC_MEMORYPRESSURE_MASK_DEFAULT ( NOTE_MEMORYSTATUS_MSL_STATUS | \
		MALLOC_MEMORYSTATUS_MASK_PRESSURE_RELIEF | \
		MALLOC_MEMORYSTATUS_MASK_RESOURCE_EXCEPTION_HANDLING )
#define MALLOC_MEMORYPRESSURE_MASK_MSL ( MALLOC_MEMORYPRESSURE_MASK_DEFAULT | \
		MSL_MEMORYPRESSURE_MASK )

#pragma mark Globals

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

MALLOC_NOEXPORT
extern bool malloc_slowpath;

#if !MALLOC_TARGET_EXCLAVES
MALLOC_NOEXPORT
extern bool malloc_tracing_enabled;

MALLOC_NOEXPORT
extern unsigned malloc_debug_flags;

MALLOC_NOEXPORT
extern bool malloc_space_efficient_enabled;

MALLOC_NOEXPORT
extern bool malloc_medium_space_efficient_enabled;

MALLOC_NOEXPORT
extern malloc_zone_t *initial_xzone_zone;
#endif // !MALLOC_TARGET_EXCLAVES

MALLOC_NOEXPORT
extern bool malloc_sanitizer_enabled;


#if CONFIG_MALLOC_PROCESS_IDENTITY
MALLOC_NOEXPORT
extern malloc_process_identity_t malloc_process_identity;
#endif

MALLOC_NOEXPORT
void * __sized_by_or_null(size)
_malloc_zone_malloc(malloc_zone_t *zone, size_t size, malloc_zone_options_t mzo) __alloc_size(2);

MALLOC_NOEXPORT
void * __sized_by_or_null(num_items * size)
_malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size, malloc_zone_options_t mzo) __alloc_size(2,3);

MALLOC_NOEXPORT
void * __sized_by_or_null(size)
_malloc_zone_valloc(malloc_zone_t *zone, size_t size, malloc_zone_options_t mzo) __alloc_size(2);

MALLOC_NOEXPORT
void * __sized_by_or_null(size)
_malloc_zone_realloc(malloc_zone_t *zone, void * __unsafe_indexable ptr,
		size_t size, malloc_type_descriptor_t type_desc) __alloc_size(3);

MALLOC_NOEXPORT
void * __sized_by_or_null(size)
_malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size,
		malloc_zone_options_t mzo, malloc_type_descriptor_t type_desc)
		 __alloc_align(2) __alloc_size(3);

#if !MALLOC_TARGET_EXCLAVES
MALLOC_NOEXPORT
void * __sized_by_or_null(size)
_malloc_zone_malloc_with_options_np_outlined(malloc_zone_t *zone, size_t align,
		size_t size, malloc_options_np_t options)
		__alloc_align(2) __alloc_size(3);
#endif // !MALLOC_TARGET_EXCLAVES

#if defined(DARWINTEST) || defined(MALLOC_BUILDING_XCTESTS)
static void inline
_free(void * __unsafe_indexable ptr) {
	return free(ptr);
}
#else
MALLOC_NOEXPORT
void
_free(void * __unsafe_indexable);
#endif // DARWINTEST || MALLOC_BUILDING_XCTESTS

MALLOC_NOEXPORT
void * __sized_by_or_null(new_size)
_realloc(void * __unsafe_indexable in_ptr, size_t new_size) __alloc_size(2);

MALLOC_NOEXPORT
malloc_zone_t *
find_registered_zone(const void * __unsafe_indexable ptr, size_t *returned_size,
		bool known_non_default);

MALLOC_NOEXPORT
int
_posix_memalign(void * __unsafe_indexable *memptr, size_t alignment, size_t size);

MALLOC_NOEXPORT MALLOC_NOINLINE
void
malloc_error_break(void);

#if MALLOC_TARGET_EXCLAVES
MALLOC_NOEXPORT
void malloc_printf(const char * __null_terminated format, ...) __printflike(1,2);
#endif // MALLOC_TARGET_EXCLAVES

#if !MALLOC_TARGET_EXCLAVES
MALLOC_NOEXPORT MALLOC_NOINLINE MALLOC_USED
int
malloc_gdb_po_unsafe(void);

__attribute__((always_inline, const))
static inline bool
malloc_traced(void)
{
	return malloc_tracing_enabled;
}
#endif // !MALLOC_TARGET_EXCLAVES

static inline uint32_t
_malloc_cpu_number(void)
{
#if TARGET_OS_SIMULATOR
	size_t n;
	pthread_cpu_number_np(&n);
	return (uint32_t)n;
#elif MALLOC_TARGET_EXCLAVES
	// rdar://111241228 (Support fetching CPU information)
	return 0;
#else
	return _os_cpu_number();
#endif // TARGET_OS_SIMULATOR
}

#if !MALLOC_TARGET_EXCLAVES
typedef union {
	malloc_thread_options_t options;
	void *storage;
} th_opts_t;

MALLOC_STATIC_ASSERT(sizeof(th_opts_t) == sizeof(void *), "Options fit into pointer bits");

static inline void
_malloc_set_thread_options(malloc_thread_options_t opts)
{
	th_opts_t x = {.options = opts};
	_pthread_setspecific_direct(__TSD_MALLOC_THREAD_OPTIONS, x.storage);
}
#endif // MALLOC_TARGET_EXCLAVES

#if CONFIG_CLUSTER_AWARE

static inline unsigned int
_malloc_cpu_cluster_number(void)
{
#if TARGET_OS_SIMULATOR
#error current cluster id not supported on simulator
#else
	return _os_cpu_cluster_number();
#endif // TARGET_OS_SIMULATOR
}

static inline unsigned int
_malloc_get_cluster_from_cpu(unsigned int cpu_number)
{
#if TARGET_OS_SIMULATOR
#error cluster id lookup not supported on simulator
#else
	return (unsigned int)*(uint8_t *)(uintptr_t)(_COMM_PAGE_CPU_TO_CLUSTER + cpu_number);
#endif
}

#endif // CONFIG_CLUSTER_AWARE

// Gets the allocation size for a calloc(). Multiples size by num_items and adds
// extra_size, storing the result in *total_size. Returns 0 on success, -1 (with
// errno set to ENOMEM) on overflow.
static int MALLOC_INLINE MALLOC_ALWAYS_INLINE
calloc_get_size(size_t num_items, size_t size, size_t extra_size, size_t *total_size)
{
	size_t alloc_size = size;
	if (num_items != 1 && (os_mul_overflow(num_items, size, &alloc_size)
			|| malloc_too_large(alloc_size))) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return -1;
	}
	if (extra_size && (os_add_overflow(alloc_size, extra_size, &alloc_size)
			|| malloc_too_large(alloc_size))) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return -1;
	}
	*total_size = alloc_size;
	return 0;
}

static MALLOC_INLINE kern_return_t
_malloc_default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr)
{
#if !MALLOC_TARGET_EXCLAVES
	MALLOC_ASSERT(task == TASK_NULL || mach_task_is_self(task));
#endif /* !MALLOC_TARGET_EXCLAVES */
	*ptr = __unsafe_forge_bidi_indexable(void *, address, size);
	size = size;
	return KERN_SUCCESS;
}

static MALLOC_INLINE memory_reader_t *
reader_or_in_memory_fallback(memory_reader_t reader, task_t task)
{
	if (reader == NULL) {
#if !MALLOC_TARGET_EXCLAVES
		MALLOC_ASSERT(task == TASK_NULL || mach_task_is_self(task));
#endif /* !MALLOC_TARGET_EXCLAVES */
		return _malloc_default_reader;
	}
	return reader;
}

/*
	* Copies the malloc library's _malloc_msl_lite_hooks_t structure to a given
	* location. Size is passed to allow the structure to  grow. Since this is
	* a temporary arrangement, we don't need to worry about
	* pointer authentication here or in the _malloc_msl_lite_hooks_t structure
	* itself.
	*/
struct _malloc_msl_lite_hooks_s;
typedef void (*set_msl_lite_hooks_callout_t) (struct _malloc_msl_lite_hooks_s *hooksp, size_t size);
void set_msl_lite_hooks(set_msl_lite_hooks_callout_t callout);

static MALLOC_INLINE void
yield(void)
{
#if !MALLOC_TARGET_EXCLAVES
	thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, 1);
#else
	// rdar://113715334 (Provide thread yield platform API)
	// pthread_yield_np();
#endif // !MALLOC_TARGET_EXCLAVES
}

#if CONFIG_FEATUREFLAGS_SIMPLE
#if TARGET_OS_SIMULATOR
#define malloc_secure_feature_enabled(name, fallback, simulator_default) \
	(simulator_default)
#else
// TODO: Do we want the secure fallback to be the same as the no-ff fallback?
#define malloc_secure_feature_enabled(name, fallback, simulator_default) \
	(malloc_internal_security_policy ? \
			os_feature_enabled_simple(libmalloc, name, fallback) : (fallback))
#endif // TARGET_OS_SIMULATOR
#endif // CONFIG_FEATUREFLAGS_SIMPLE

#endif // __INTERNAL_H
