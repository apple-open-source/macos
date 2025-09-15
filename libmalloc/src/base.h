/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#ifndef __BASE_H
#define __BASE_H

#include <stddef.h>
#include "platform.h"

#include <malloc/_ptrcheck.h>
__ptrcheck_abi_assume_single()

#ifndef __has_extension
#define __has_extension(x) 0
#endif

#if __has_extension(c_static_assert)
#define MALLOC_STATIC_ASSERT(x, y) _Static_assert((x), y)
#else
#define MALLOC_STATIC_ASSERT(x, y)
#endif

#define MALLOC_ASSERT(e) ({ \
	if (__builtin_expect(!(e), 0)) { \
		__asm__ __volatile__ (""); \
		__builtin_trap(); \
	} \
})

#ifdef DEBUG
#define MALLOC_DEBUG_ASSERT(e) MALLOC_ASSERT(e)
#else
#define MALLOC_DEBUG_ASSERT(e)
#endif

#define MALLOC_FATAL_ERROR(cause, message) ({ \
		_os_set_crash_log_cause_and_message((cause), "FATAL ERROR - " message); \
		__asm__ __volatile__ (""); \
		__builtin_trap(); \
})

#define MALLOC_REPORT_FATAL_ERROR(cause, message) ({ \
		malloc_report(ASL_LEVEL_ERR, "*** FATAL ERROR - " message ".\n"); \
		MALLOC_FATAL_ERROR((cause), message); \
})

#if __has_include(<machine/cpu_capabilities.h>) && (defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__arm64__))
#   define __APPLE_API_PRIVATE
#   include <machine/cpu_capabilities.h>
#   if defined(__i386__) || defined(__x86_64__)
#      define _COMM_PAGE_VERSION_REQD 9
#   else
#      define _COMM_PAGE_VERSION_REQD 3
#   endif
#   undef __APPLE_API_PRIVATE
#elif __has_include(<sys/sysctl.h>)
#   include <sys/sysctl.h>
#endif

#if defined(__i386__) || defined(__x86_64__)
// <rdar://problem/23495834> nano vs. magazine have different definitions
// for this cache-line size.
#   define MALLOC_CACHE_LINE 128
#   define MALLOC_NANO_CACHE_LINE 64
#elif defined(__arm__) || defined(__arm64__)
# 	define MALLOC_CACHE_LINE 128
#   define MALLOC_NANO_CACHE_LINE 64
#else
#   define MALLOC_CACHE_LINE 32
#   define MALLOC_NANO_CACHE_LINE 32
#endif

#define MALLOC_CACHE_ALIGN __attribute__ ((aligned (MALLOC_CACHE_LINE) ))
#define MALLOC_NANO_CACHE_ALIGN __attribute__ ((aligned (MALLOC_NANO_CACHE_LINE) ))
#define MALLOC_EXPORT extern __attribute__((visibility("default")))
#define MALLOC_NOEXPORT __attribute__((visibility("hidden")))
#define MALLOC_NOINLINE __attribute__((noinline))
#define MALLOC_INLINE __inline__
#define MALLOC_ALWAYS_INLINE __attribute__((always_inline))
#define MALLOC_PACKED __attribute__((packed))
#define MALLOC_USED __attribute__((used))
#define MALLOC_UNUSED __attribute__((unused))
#define MALLOC_NORETURN __attribute__((noreturn))
#define MALLOC_COLD __attribute__((cold))
#define MALLOC_NOESCAPE __attribute__((noescape))
#define MALLOC_PRESERVE_MOST __attribute__((preserve_most))
#define MALLOC_FALLTHROUGH __attribute__((fallthrough))
#define CHECK_MAGAZINE_PTR_LOCKED(szone, mag_ptr, fun) {}

#if __has_feature(bounds_safety)
#define __malloc_bidi_indexable __bidi_indexable
#else
#define __malloc_bidi_indexable
#endif

#define SCRIBBLE_BYTE 0xaa /* allocated scribble */
#define SCRABBLE_BYTE 0x55 /* free()'d scribble */
#define SCRUBBLE_BYTE 0xdd /* madvise(..., MADV_FREE) scriblle */

#undef KiB
#undef MiB
#undef GiB
#undef TiB
#define KiB(x) ((uint64_t)(x) << 10)
#define MiB(x) ((uint64_t)(x) << 20)
#define GiB(x) ((uint64_t)(x) << 30)
#define TiB(x) ((uint64_t)(x) << 40)

#define NDEBUG 1
#define trunc_page_quanta(x) trunc_page((x))
#define round_page_quanta(x) round_page((x))
#define vm_page_quanta_size (vm_page_size)
#define vm_page_quanta_shift (vm_page_shift)

/*
 * Large rounds allocation sizes up to MAX(vm_kernel_page_size, page_size).
 * This provides better death row caching performance when vm_kernel_page_size > page_size.
 * The kernel allocates pages of vm_kernel_page_size to back our allocations,
 * so there is no additional physical page cost to doing this.
 * Guard pages are the same size to ensure the full vm allocation size is a multiple of MAX(vm_kernel_page_size, page_size).
 */
#define large_vm_page_quanta_size (vm_kernel_page_size > vm_page_size ? vm_kernel_page_size : vm_page_size)
#define large_vm_page_quanta_mask (vm_kernel_page_mask > vm_page_mask ? vm_kernel_page_mask : vm_page_mask)
#define large_vm_page_quanta_shift (vm_kernel_page_shift > vm_page_shift ? vm_kernel_page_shift : vm_page_shift)

#define trunc_large_page_quanta(x) ((x) & (~large_vm_page_quanta_mask))
#define round_large_page_quanta(x) (trunc_large_page_quanta((x) + large_vm_page_quanta_mask))

/*
 * MALLOC_ABSOLUTE_MAX_SIZE - There are many instances of addition to a
 * user-specified size_t, which can cause overflow (and subsequent crashes)
 * for values near SIZE_T_MAX.  Rather than add extra "if" checks everywhere
 * this occurs, it is easier to just set an absolute maximum request size,
 * and immediately return an error if the requested size exceeds this maximum.
 * Of course, values less than this absolute max can fail later if the value
 * is still too large for the available memory.  The largest value added
 * seems to be large_vm_page_quanta_size (in the macro round_large_page_quanta()), so to be safe, we set
 * the maximum to be 2 * PAGE_SIZE less than SIZE_T_MAX.
 *
 * This value needs to be calculated at runtime, so we'll cache it rather than
 * recalculate on each use.
 */
#define _MALLOC_ABSOLUTE_MAX_SIZE (SIZE_T_MAX - (2 * large_vm_page_quanta_size))

#if defined(MALLOC_BUILDING_XCTESTS)
#define malloc_absolute_max_size _MALLOC_ABSOLUTE_MAX_SIZE
#else
extern size_t malloc_absolute_max_size; // caches the definition above
#endif

#if !MALLOC_TARGET_EXCLAVES
#define malloc_too_large(n) ((n) > malloc_absolute_max_size)
#else
#define malloc_too_large(n) (0)
#endif // !MALLOC_TARGET_EXCLAVES

#if MALLOC_TARGET_EXCLAVES && !defined(MAX)
#define MAX(a, b) (((a)>(b))?(a):(b))
#endif // MALLOC_TARGET_EXCLAVES && !defined(MAX)

// add a guard page before each VM region to help debug
#define MALLOC_ADD_PRELUDE_GUARD_PAGE (1 << 0)
// add a guard page after each VM region to help debug
#define MALLOC_ADD_POSTLUDE_GUARD_PAGE (1 << 1)
// Mask both guard page flags
#define MALLOC_ADD_GUARD_PAGE_FLAGS (MALLOC_ADD_PRELUDE_GUARD_PAGE|MALLOC_ADD_POSTLUDE_GUARD_PAGE)
// apply guard pages to all regions
#define MALLOC_GUARD_ALL (1 << 2)
// Mask for guard page request flags
#define MALLOC_ALL_GUARD_PAGE_FLAGS (MALLOC_ADD_GUARD_PAGE_FLAGS|MALLOC_GUARD_ALL)
// do not protect prelude page
#define MALLOC_DONT_PROTECT_PRELUDE (1 << 3)
// do not protect postlude page
#define MALLOC_DONT_PROTECT_POSTLUDE (1 << 4)
// write 0x55 onto free blocks
#define MALLOC_DO_SCRIBBLE (1 << 5)
// call abort() on any malloc error, such as double free or out of memory.
#define MALLOC_ABORT_ON_ERROR (1 << 6)
// allocate objects such that they may be used with VM purgability APIs
#define MALLOC_PURGEABLE (1 << 7)
// call abort() on malloc errors, but not on out of memory.
#define MALLOC_ABORT_ON_CORRUPTION (1 << 8)
// don't populate the mapping for this allocation
#define MALLOC_NO_POPULATE (1 << 9)
// enable faulting anywhere within this allocation
#define MALLOC_CAN_FAULT (1 << 12)
// guarded metadata allocation
#define MALLOC_GUARDED_METADATA (1 << 13)

// See malloc_implementation.h
// MALLOC_MSL_LITE_WRAPPED_ZONE_FLAGS == (1 << 10)


/*
 * These commpage routines provide fast access to the logical cpu number
 * of the calling processor assuming no pre-emption occurs.
 */

extern unsigned int hyper_shift;
extern unsigned int logical_ncpus;
extern unsigned int phys_ncpus;
#if CONFIG_CLUSTER_AWARE
extern unsigned int ncpuclusters;
#endif // CONFIG_CLUSTER_AWARE

/*
 * msize - a type to refer to the number of quanta of a tiny or small
 * allocation.  A tiny block with an msize of 3 would be 3 << SHIFT_TINY_QUANTUM
 * bytes in size.
 */
typedef unsigned short msize_t;

typedef unsigned int grain_t; // N.B. wide enough to index all free slots
typedef struct large_entry_s large_entry_t;
typedef struct szone_s szone_t;
typedef struct rack_s rack_t;
typedef struct magazine_s magazine_t;
typedef int mag_index_t;
typedef void * __single region_t;

#endif // __BASE_H
