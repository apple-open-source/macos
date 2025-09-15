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

#ifndef __PLATFORM_H
#define __PLATFORM_H

#include <TargetConditionals.h>

#ifndef MALLOC_TARGET_DK_OSX
#define MALLOC_TARGET_DK_OSX 0
#endif // MALLOC_TARGET_DK_OSX

#ifndef MALLOC_TARGET_DK_IOS
#define MALLOC_TARGET_DK_IOS 0
#endif // MALLOC_TARGET_DK_IOS

#ifndef MALLOC_TARGET_DK_VISIONOS
#define MALLOC_TARGET_DK_VISIONOS 0
#endif // MALLOC_TARGET_DK_VISIONOS

#ifndef MALLOC_TARGET_DK_WATCH
#define MALLOC_TARGET_DK_WATCH 0
#endif // MALLOC_TARGET_DK_WATCH

#ifndef MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#define MALLOC_TARGET_EXCLAVES_INTROSPECTOR 0
#endif // MALLOC_TARGET_EXCLAVES_INTROSPECTOR

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) || (TARGET_OS_DRIVERKIT && !MALLOC_TARGET_DK_OSX)
#define MALLOC_TARGET_IOS 1
#else // (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) || (TARGET_OS_DRIVERKIT && !MALLOC_TARGET_DK_OSX)
#define MALLOC_TARGET_IOS 0
#endif // (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) || (TARGET_OS_DRIVERKIT && !MALLOC_TARGET_DK_OSX)

#if TARGET_OS_IOS && !TARGET_OS_VISION
#define MALLOC_TARGET_IOS_ONLY 1
#else
#define MALLOC_TARGET_IOS_ONLY 0
#endif

#ifdef __LP64__
#define MALLOC_TARGET_64BIT 1
#else // __LP64__
#define MALLOC_TARGET_64BIT 0
#endif

#if !defined(TARGET_OS_EXCLAVECORE)
#define TARGET_OS_EXCLAVECORE 0
#endif

#if !defined(TARGET_OS_EXCLAVEKIT)
#define TARGET_OS_EXCLAVEKIT 0
#endif

#if TARGET_OS_EXCLAVECORE || TARGET_OS_EXCLAVEKIT
#define MALLOC_TARGET_EXCLAVES 1
#else // TARGET_OS_EXCLAVECORE || TARGET_OS_EXCLAVEKIT
#define MALLOC_TARGET_EXCLAVES 0
#endif // TARGET_OS_EXCLAVECORE || TARGET_OS_EXCLAVEKIT

// <rdar://problem/12596555>
#define CONFIG_RECIRC_DEPOT 1
#define CONFIG_AGGRESSIVE_MADVISE 1

#if MALLOC_TARGET_IOS
# define DEFAULT_AGGRESSIVE_MADVISE_ENABLED true
// <rdar://problem/12596555>
# define CONFIG_MADVISE_PRESSURE_RELIEF 0
#else // MALLOC_TARGET_IOS
# define DEFAULT_AGGRESSIVE_MADVISE_ENABLED false
// <rdar://problem/12596555>
# define CONFIG_MADVISE_PRESSURE_RELIEF 1
#endif // MALLOC_TARGET_IOS

// <rdar://problem/10397726>
#define CONFIG_RELAXED_INVARIANT_CHECKS 1

// <rdar://problem/19818071>
#define CONFIG_MADVISE_STYLE MADV_FREE_REUSABLE

#if MALLOC_TARGET_64BIT && !TARGET_OS_WATCH && !MALLOC_TARGET_EXCLAVES
#define CONFIG_NANOZONE 1
#else
#define CONFIG_NANOZONE 0
#endif

#if MALLOC_TARGET_64BIT
#define CONFIG_ASLR_INTERNAL 0
#else // MALLOC_TARGET_64BIT
#define CONFIG_ASLR_INTERNAL 1
#endif // MALLOC_TARGET_64BIT

// enable nano checking for corrupt free list
#define NANO_FREE_DEQUEUE_DILIGENCE 1

// Conditional behaviour depends on MallocSpaceEfficient being set by
// JetsamProperties which isn't true on iOS
#if MALLOC_TARGET_IOS || TARGET_OS_DRIVERKIT
#define NANOV2_DEFAULT_MODE NANO_ENABLED
#else
#define NANOV2_DEFAULT_MODE NANO_CONDITIONAL
#endif

// whether to pre-reserve all available nano regions during initialization
#define CONFIG_NANO_RESERVE_REGIONS 0


// This governs a last-free cache of 1 that bypasses the free-list for each region size
#define CONFIG_TINY_CACHE 1
#define CONFIG_SMALL_CACHE 1
#define CONFIG_MEDIUM_CACHE 1

// medium allocator enabled or disabled
#if MALLOC_TARGET_64BIT
#if MALLOC_TARGET_IOS
#define CONFIG_MEDIUM_ALLOCATOR 0
#else // MALLOC_TARGET_IOS
#define CONFIG_MEDIUM_ALLOCATOR 1
#endif // MALLOC_TARGET_IOS
#else // MALLOC_TARGET_64BIT
#define CONFIG_MEDIUM_ALLOCATOR 0
#endif // MALLOC_TARGET_64BIT


#if CONFIG_MEDIUM_ALLOCATOR
#define DEFAULT_MEDIUM_ALLOCATOR_ENABLED 1
#else // CONFIG_MEDIUM_ALLOCATOR
#define DEFAULT_MEDIUM_ALLOCATOR_ENABLED 0
#endif // CONFIG_MEDIUM_ALLOCATOR


// The large last-free cache (aka. death row cache)
#if (TARGET_OS_IOS || TARGET_OS_VISION) || TARGET_OS_OSX || \
		TARGET_OS_SIMULATOR || TARGET_OS_DRIVERKIT
#define CONFIG_LARGE_CACHE 1
#if TARGET_OS_OSX
# define DEFAULT_LARGE_CACHE_ENABLED true
#else
# define DEFAULT_LARGE_CACHE_ENABLED false
#endif // TARGET_OS_OSX
#else
#define CONFIG_LARGE_CACHE 0
#define DEFAULT_LARGE_CACHE_ENABLED false
#endif

// Deferred reclaim
#if CONFIG_LARGE_CACHE
#if (MALLOC_TARGET_IOS_ONLY && !TARGET_OS_SIMULATOR) || \
		(MALLOC_TARGET_64BIT && TARGET_OS_DRIVERKIT && !MALLOC_TARGET_DK_OSX)
#define CONFIG_MAGAZINE_DEFERRED_RECLAIM 1
#define CONFIG_XZM_DEFERRED_RECLAIM 1
#elif TARGET_OS_OSX || (TARGET_OS_VISION && !TARGET_OS_SIMULATOR)
#define CONFIG_MAGAZINE_DEFERRED_RECLAIM 0
#define CONFIG_XZM_DEFERRED_RECLAIM 1
#else
#define CONFIG_MAGAZINE_DEFERRED_RECLAIM 0
#define CONFIG_XZM_DEFERRED_RECLAIM 0
#endif // MALLOC_TARGET_IOS && MALLOC_TARGET_64BIT
#else
#define CONFIG_MAGAZINE_DEFERRED_RECLAIM 0
#define CONFIG_XZM_DEFERRED_RECLAIM 0
#endif // CONFIG_LARGE_CACHE

#if CONFIG_MAGAZINE_DEFERRED_RECLAIM && !CONFIG_LARGE_CACHE
#error "Deferred reclaim requires large cache"
#endif // CONFIG_MAGAZINE_DEFERRED_RECLAIM && !CONFIG_LARGE_CACHE

#if MALLOC_TARGET_IOS || MALLOC_TARGET_EXCLAVES
// The VM system on iOS forces malloc-tagged memory to never be marked as
// copy-on-write, this would include calls we make to vm_copy. Given that the
// kernel would just be doing a memcpy, we force it to happen in userspace.
#define CONFIG_REALLOC_CAN_USE_VMCOPY 0
#else
#define CONFIG_REALLOC_CAN_USE_VMCOPY 1
#endif

#if !TARGET_OS_DRIVERKIT && (!TARGET_OS_OSX || MALLOC_TARGET_64BIT)
#define CONFIG_FEATUREFLAGS_SIMPLE 1
#else
#define CONFIG_FEATUREFLAGS_SIMPLE 0
#endif

// presence of commpage memsize
#define CONFIG_HAS_COMMPAGE_MEMSIZE 1

// presence of commpage number of cpu count
#define CONFIG_HAS_COMMPAGE_NCPUS 1

// Distribute magazines by cluster number if nmagazines == nclusters
#if (MALLOC_TARGET_IOS_ONLY && !TARGET_OS_SIMULATOR) || \
		MALLOC_TARGET_DK_IOS || \
		TARGET_OS_OSX || MALLOC_TARGET_DK_OSX
#define CONFIG_MAGAZINE_PER_CLUSTER 1
#else
#define CONFIG_MAGAZINE_PER_CLUSTER 0
#endif

// Support cluster-aware policies in xzone malloc
#if ((TARGET_OS_IOS || TARGET_OS_VISION) && !TARGET_OS_SIMULATOR) || \
		MALLOC_TARGET_DK_IOS || MALLOC_TARGET_DK_VISIONOS || \
		TARGET_OS_OSX || MALLOC_TARGET_DK_OSX || \
		(TARGET_OS_WATCH && !TARGET_OS_SIMULATOR) || MALLOC_TARGET_DK_WATCH
#define CONFIG_XZM_CLUSTER_AWARE 1
#else
#define CONFIG_XZM_CLUSTER_AWARE 0
#endif

// Build with supporting logic for cluster awareness in either allocator
#if CONFIG_MAGAZINE_PER_CLUSTER || CONFIG_XZM_CLUSTER_AWARE
#define CONFIG_CLUSTER_AWARE 1
#else
#define CONFIG_CLUSTER_AWARE 0
#endif

// Use of hyper-shift for magazine selection.
#define CONFIG_NANO_USES_HYPER_SHIFT 0
#define CONFIG_SZONE_USES_HYPER_SHIFT 0

#if TARGET_OS_OSX
#define CONFIG_CHECK_PLATFORM_BINARY 1
#else
#define CONFIG_CHECK_PLATFORM_BINARY 0
#endif

#define MALLOC_ZERO_POLICY_DEFAULT MALLOC_ZERO_ON_FREE


#ifndef MALLOC_XZONE_ENABLED_DEFAULT
#define MALLOC_XZONE_ENABLED_DEFAULT false
#endif

#if MALLOC_TARGET_64BIT
#define CONFIG_EARLY_MALLOC 1
#else
#define CONFIG_EARLY_MALLOC 0
#endif

#if MALLOC_TARGET_IOS_ONLY || TARGET_OS_VISION || TARGET_OS_OSX || TARGET_OS_WATCH
#define CONFIG_MALLOC_PROCESS_IDENTITY 1
#else
#define CONFIG_MALLOC_PROCESS_IDENTITY 0
#endif

#ifndef CONFIG_SANITIZER
#define CONFIG_SANITIZER 1
#endif

#define MALLOC_SECURE_ALLOCATOR_LAUNCHD_ENABLED_DEFAULT true


#endif // __PLATFORM_H
