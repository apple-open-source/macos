/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	zalloc.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	 1985
 *
 */

#ifdef  KERNEL_PRIVATE

#ifndef _KERN_ZALLOC_H_
#define _KERN_ZALLOC_H_

#include <mach/machine/vm_types.h>
#include <mach_debug/zone_info.h>
#include <kern/kern_types.h>
#include <sys/cdefs.h>
#include <os/alloc_util.h>
#include <os/atomic.h>

#ifdef XNU_KERNEL_PRIVATE
#include <kern/startup.h>
#endif /* XNU_KERNEL_PRIVATE */

#if XNU_KERNEL_PRIVATE && !defined(ZALLOC_ALLOW_DEPRECATED)
#define __zalloc_deprecated(msg)       __deprecated_msg(msg)
#else
#define __zalloc_deprecated(msg)
#endif

/*
 * Enable this macro to force type safe zalloc/zalloc_ro/...
 */
#ifndef ZALLOC_TYPE_SAFE
#if __has_ptrcheck
#define ZALLOC_TYPE_SAFE 1
#else
#define ZALLOC_TYPE_SAFE 0
#endif
#endif /* !ZALLOC_TYPE_SAFE */

__BEGIN_DECLS __ASSUME_PTR_ABI_SINGLE_BEGIN

/*!
 * @macro __zpercpu
 *
 * @abstract
 * Annotation that helps denoting a per-cpu pointer that requires usage of
 * @c zpercpu_*() for access.
 */
#define __zpercpu __unsafe_indexable

/*!
 * @typedef zone_id_t
 *
 * @abstract
 * The type for a zone ID.
 */
typedef uint16_t zone_id_t;

/**
 * @enum zone_create_flags_t
 *
 * @abstract
 * Set of flags to pass to zone_create().
 *
 * @discussion
 * Some kernel-wide policies affect all possible created zones.
 * Explicit @c ZC_* win over such policies.
 */
__options_decl(zone_create_flags_t, uint64_t, {
	/** The default value to pass to zone_create() */
	ZC_NONE                 = 0x00000000,

	/** (obsolete) */
	ZC_SEQUESTER            = 0x00000001,
	/** (obsolete) */
	ZC_NOSEQUESTER          = 0x00000002,

	/** Enable per-CPU zone caching for this zone */
	ZC_CACHING              = 0x00000010,
	/** Disable per-CPU zone caching for this zone */
	ZC_NOCACHING            = 0x00000020,

	/** Allocate zone pages as Read-only **/
	ZC_READONLY             = 0x00800000,

	/** Mark zone as a per-cpu zone */
	ZC_PERCPU               = 0x01000000,

	/** Force the created zone to clear every allocation on free */
	ZC_ZFREE_CLEARMEM       = 0x02000000,

	/** Mark zone as non collectable by zone_gc() */
	ZC_NOGC                 = 0x04000000,

	/** Do not encrypt this zone during hibernation */
	ZC_NOENCRYPT            = 0x08000000,

	/** Type requires alignment to be preserved */
	ZC_ALIGNMENT_REQUIRED   = 0x10000000,

	/** Obsolete */
	ZC_NOGZALLOC            = 0x20000000,

	/** Don't asynchronously replenish the zone via callouts */
	ZC_NOCALLOUT            = 0x40000000,

	/** Can be zdestroy()ed, not default unlike zinit() */
	ZC_DESTRUCTIBLE         = 0x80000000,

#ifdef XNU_KERNEL_PRIVATE
	/** This zone is a built object cache */
	ZC_OBJ_CACHE            = 0x0080000000000000,

	/** Use guard pages in PGZ mode */
	ZC_PGZ_USE_GUARDS       = 0x0100000000000000,

	/** Zone doesn't support TBI tagging */
	ZC_NO_TBI_TAG             = 0x0200000000000000,

	/** This zone will back a kalloc type */
	ZC_KALLOC_TYPE          = 0x0400000000000000,

	/** Disable PGZ for this zone */
	ZC_NOPGZ                = 0x0800000000000000,

	/** This zone contains pure data */
	ZC_DATA                 = 0x1000000000000000,

	/** This zone belongs to the VM submap */
	ZC_VM                   = 0x2000000000000000,

	/** Disable kasan quarantine for this zone */
	ZC_KASAN_NOQUARANTINE   = 0x4000000000000000,

	/** Disable kasan redzones for this zone */
	ZC_KASAN_NOREDZONE      = 0x8000000000000000,
#endif /* XNU_KERNEL_PRIVATE */
});

/*!
 * @union zone_or_view
 *
 * @abstract
 * A type used for calls that admit both a zone or a zone view.
 *
 * @discussion
 * @c zalloc() and @c zfree() and their variants can act on both
 * zones and zone views.
 */
union zone_or_view {
	struct kalloc_type_view    *zov_kt_heap;
	struct zone_view           *zov_view;
	struct zone                *zov_zone;
#ifdef __cplusplus
	inline zone_or_view(struct zone_view *zv) : zov_view(zv) {
	}
	inline zone_or_view(struct zone *z) : zov_zone(z) {
	}
	inline zone_or_view(struct kalloc_type_view *kth) : zov_kt_heap(kth) {
	}
#endif
};
#ifdef __cplusplus
typedef union zone_or_view zone_or_view_t;
#else
typedef union zone_or_view zone_or_view_t __attribute__((transparent_union));
#endif

/*!
 * @enum zone_create_ro_id_t
 *
 * @abstract
 * Zone creation IDs for external read only zones
 *
 * @discussion
 * Kexts that desire to use the RO allocator should:
 * 1. Add a zone creation id below
 * 2. Add a corresponding ID to @c zone_reserved_id_t
 * 3. Use @c zone_create_ro with ID from #1 to create a RO zone.
 * 4. Save the zone ID returned from #3 in a SECURITY_READ_ONLY_LATE variable.
 * 5. Use the saved ID for zalloc_ro/zfree_ro, etc.
 */
__enum_decl(zone_create_ro_id_t, zone_id_t, {
	ZC_RO_ID_SANDBOX,
	ZC_RO_ID_PROFILE,
	ZC_RO_ID_PROTOBOX,
	ZC_RO_ID_SB_FILTER,
	ZC_RO_ID_AMFI_OSENTITLEMENTS,
	ZC_RO_ID__LAST = ZC_RO_ID_AMFI_OSENTITLEMENTS,
});

/*!
 * @function zone_create
 *
 * @abstract
 * Creates a zone with the specified parameters.
 *
 * @discussion
 * A Zone is a slab allocator that returns objects of a given size very quickly.
 *
 * @param name          the name for the new zone.
 * @param size          the size of the elements returned by this zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 *
 * @returns             the created zone, this call never fails.
 */
extern zone_t   zone_create(
	const char             *name __unsafe_indexable,
	vm_size_t               size,
	zone_create_flags_t     flags);

/*!
 *
 * @function zone_get_elem_size
 *
 * @abstract
 * Get the intrinsic size of one element allocated by the given zone.
 *
 * @discussion
 * All zones are created to allocate elements of a fixed size, but the size is
 * not always a compile-time constant. @c zone_get_elem_size can be used to
 * retrieve the size of elements allocated by this zone at runtime.
 *
 * @param zone			the zone to inspect
 *
 * @returns			the size of elements allocated by this zone
 */
extern vm_size_t    zone_get_elem_size(zone_t zone);

/*!
 * @function zone_create_ro
 *
 * @abstract
 * Creates a read only zone with the specified parameters from kexts
 *
 * @discussion
 * See notes under @c zone_create_ro_id_t wrt creation and use of RO zones in
 * kexts. Do not use this API to create read only zones in xnu.
 *
 * @param name          the name for the new zone.
 * @param size          the size of the elements returned by this zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 * @param zc_ro_id      an ID declared in @c zone_create_ro_id_t
 *
 * @returns             the zone ID of the created zone, this call never fails.
 */
extern zone_id_t   zone_create_ro(
	const char             *name __unsafe_indexable,
	vm_size_t               size,
	zone_create_flags_t     flags,
	zone_create_ro_id_t     zc_ro_id);

/*!
 * @function zdestroy
 *
 * @abstract
 * Destroys a zone previously made with zone_create.
 *
 * @discussion
 * Zones must have been made destructible for @c zdestroy() to be allowed,
 * passing @c ZC_DESTRUCTIBLE at @c zone_create() time.
 *
 * @param zone          the zone to destroy.
 */
extern void     zdestroy(
	zone_t          zone);

/*!
 * @function zone_require
 *
 * @abstract
 * Requires for a given pointer to belong to the specified zone.
 *
 * @discussion
 * The function panics if the check fails as it indicates that the kernel
 * internals have been compromised.
 *
 * @param zone          the zone the address needs to belong to.
 * @param addr          the element address to check.
 */
extern void     zone_require(
	zone_t          zone,
	void           *addr __unsafe_indexable);

/*!
 * @function zone_require_ro
 *
 * @abstract
 * Version of zone require intended for zones created with ZC_READONLY
 *
 * @discussion
 * This check is not sufficient to fully trust the element.
 *
 * Another check of its content must be performed to prove
 * that the element is "the right one", a typical technique
 * for when the RO data structure is 1:1 with a mutable one,
 * is a simple circularity check with a very strict lifetime
 * (both the mutable and read-only data structures are made
 * and destroyed as close as possible).
 *
 * @param zone_id       the zone id the address needs to belong to.
 * @param elem_size     the element size for this zone.
 * @param addr          the element address to check.
 */
extern void     zone_require_ro(
	zone_id_t       zone_id,
	vm_size_t       elem_size,
	void           *addr __unsafe_indexable);

/*!
 * @enum zalloc_flags_t
 *
 * @brief
 * Flags that can be passed to @c zalloc_internal or @c zalloc_flags.
 *
 * @discussion
 * It is encouraged that any callsite passing flags uses exactly one of:
 * @c Z_WAITOK, @c Z_NOWAIT or @c Z_NOPAGEWAIT, the default being @c Z_WAITOK
 * if nothing else was specified.
 *
 * If any @c Z_NO*WAIT flag is passed alongside @c Z_WAITOK,
 * then @c Z_WAITOK is ignored.
 *
 * @const Z_WAITOK
 * Passing this flag means that zalloc() will be allowed to sleep
 * for memory to become available for this allocation. If the zone
 * isn't exhaustible, zalloc(Z_WAITOK) never fails.
 *
 * If the zone is exhaustible, zalloc() might still fail if the zone
 * is at its maximum allowed memory usage, unless Z_NOFAIL is passed,
 * in which case zalloc() will block until an element is freed.
 *
 * @const Z_NOWAIT
 * Passing this flag means that zalloc is not allowed to ever block.
 *
 * @const Z_NOPAGEWAIT
 * Passing this flag means that zalloc is allowed to wait due to lock
 * contention, but will not wait for the VM to wait for pages when
 * under memory pressure.
 *
 * @const Z_ZERO
 * Passing this flags means that the returned memory has been zeroed out.
 *
 * @const Z_NOFAIL
 * Passing this flag means that the caller expects the allocation to always
 * succeed. This will result in a panic if this assumption isn't correct.
 *
 * This flag is incompatible with @c Z_NOWAIT or @c Z_NOPAGEWAIT.
 * For exhaustible zones, it forces the caller to wait until a zfree() happend
 * if the zone has reached its maximum of allowed elements.
 *
 * @const Z_REALLOCF
 * For the realloc family of functions,
 * free the incoming memory on failure cases.
 *
 #if XNU_KERNEL_PRIVATE
 * @const Z_SET_NOTSHARED
 * Using this flag from external allocations APIs (kalloc_type/zalloc)
 * allows the callsite to skip the shared zone for that sizeclass and
 * directly allocated from the requested zone.
 * Using this flag from internal APIs (zalloc_ext) will skip the shared
 * zone only when a given threshold is exceeded. It will also set a flag
 * to indicate that future allocations to the zone should directly go to
 * the zone instead of the shared zone.
 *
 * @const Z_SPRAYQTN
 * This flag tells the VM to allocate from the "spray quarantine" range when
 * it services the allocation. For more details on what allocations qualify
 * to use this flag see @c KMEM_RANGE_ID_SPRAYQTN.
 *
 * @const Z_KALLOC_ARRAY
 * Instead of returning a standard "pointer" return a pointer that encodes
 * its size-class into the pointer itself (Only for kalloc, might limit
 * the range of allocations that can be done).
 *
 * @const Z_FULLSIZE
 * Used to indicate that the caller will use all available space in excess
 * from the requested allocation size.
 *
 * @const Z_SKIP_KASAN
 * Tell zalloc() not to do any kasan adjustments.
 *
 * @const Z_MAY_COPYINMAP
 * This data allocation might be used with vm_map_copyin().
 * This allows for those allocations to be associated with a proper VM object.
 *
 * @const Z_VM_TAG_BT_BIT
 * Used to blame allocation accounting on the first kext
 * found in the backtrace of the allocation.
 *
 * @const Z_NOZZC
 * Used internally to mark allocations that will skip zero validation.
 *
 * @const Z_PCPU
 * Used internally for the percpu paths.
 *
 * @const Z_VM_TAG_MASK
 * Represents bits in which a vm_tag_t for the allocation can be passed.
 * (used by kalloc for the zone tagging debugging feature).
 #endif
 */
__options_decl(zalloc_flags_t, uint32_t, {
	// values smaller than 0xff are shared with the M_* flags from BSD MALLOC
	Z_WAITOK        = 0x0000,
	Z_NOWAIT        = 0x0001,
	Z_NOPAGEWAIT    = 0x0002,
	Z_ZERO          = 0x0004,
	Z_REALLOCF      = 0x0008,

#if XNU_KERNEL_PRIVATE
	Z_SET_NOTSHARED = 0x0040,
	Z_SPRAYQTN      = 0x0080,
	Z_KALLOC_ARRAY  = 0x0100,
#if KASAN_CLASSIC
	Z_FULLSIZE      = 0x0000,
#else
	Z_FULLSIZE      = 0x0200,
#endif
#if KASAN_CLASSIC
	Z_SKIP_KASAN    = 0x0400,
#else
	Z_SKIP_KASAN    = 0x0000,
#endif
	Z_MAY_COPYINMAP = 0x0800,
	Z_VM_TAG_BT_BIT = 0x1000,
	Z_PCPU          = 0x2000,
	Z_NOZZC         = 0x4000,
#endif /* XNU_KERNEL_PRIVATE */
	Z_NOFAIL        = 0x8000,

	/* convenient c++ spellings */
	Z_NOWAIT_ZERO          = Z_NOWAIT | Z_ZERO,
	Z_WAITOK_ZERO          = Z_WAITOK | Z_ZERO,
	Z_WAITOK_ZERO_NOFAIL   = Z_WAITOK | Z_ZERO | Z_NOFAIL,
#if XNU_KERNEL_PRIVATE
	Z_WAITOK_ZERO_SPRAYQTN = Z_WAITOK | Z_ZERO | Z_SPRAYQTN,
#endif

	Z_KPI_MASK             = Z_WAITOK | Z_NOWAIT | Z_NOPAGEWAIT | Z_ZERO,
#if XNU_KERNEL_PRIVATE
	Z_ZERO_VM_TAG_BT_BIT   = Z_ZERO | Z_VM_TAG_BT_BIT,
	/** used by kalloc to propagate vm tags for -zt */
	Z_VM_TAG_MASK   = 0xffff0000,

#define Z_VM_TAG_SHIFT        16
#define Z_VM_TAG(fl, tag)     ((zalloc_flags_t)((fl) | ((tag) << Z_VM_TAG_SHIFT)))
#define Z_VM_TAG_BT(fl, tag)  ((zalloc_flags_t)(Z_VM_TAG(fl, tag) | Z_VM_TAG_BT_BIT))
#endif
});

/*
 * This type is used so that kalloc_internal has good calling conventions
 * for callers who want to cheaply both know the allocated address
 * and the actual size of the allocation.
 */
struct kalloc_result {
	void         *addr __sized_by(size);
	vm_size_t     size;
};

/*!
 * @typedef zone_stats_t
 *
 * @abstract
 * The opaque type for per-cpu zone stats that are accumulated per zone
 * or per zone-view.
 */
typedef struct zone_stats *__zpercpu zone_stats_t;

/*!
 * @typedef zone_view_t
 *
 * @abstract
 * A view on a zone for accounting purposes.
 *
 * @discussion
 * A zone view uses the zone it references for the allocations backing store,
 * but does the allocation accounting at the view level.
 *
 * These accounting are surfaced by @b zprint(1) and similar tools,
 * which allow for cheap but finer grained understanding of allocations
 * without any fragmentation cost.
 *
 * Zone views are protected by the kernel lockdown and can't be initialized
 * dynamically. They must be created using @c ZONE_VIEW_DEFINE().
 */
typedef struct zone_view *zone_view_t;
struct zone_view {
	zone_t          zv_zone;
	zone_stats_t    zv_stats;
	const char     *zv_name __unsafe_indexable;
	zone_view_t     zv_next;
};

/*!
 * @typedef kalloc_type_view_t
 *
 * @abstract
 * The opaque type created at kalloc_type callsites to redirect calls to
 * the right zone.
 */
typedef struct kalloc_type_view *kalloc_type_view_t;

#if XNU_KERNEL_PRIVATE
/*
 * kalloc_type/kfree_type implementation functions
 */
extern void *__unsafe_indexable kalloc_type_impl_internal(
	kalloc_type_view_t  kt_view,
	zalloc_flags_t      flags);

extern void kfree_type_impl_internal(
	kalloc_type_view_t kt_view,
	void               *ptr __unsafe_indexable);

static inline void *__unsafe_indexable
kalloc_type_impl(
	kalloc_type_view_t      kt_view,
	zalloc_flags_t          flags)
{
	void *__unsafe_indexable addr = kalloc_type_impl_internal(kt_view, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return addr;
}

#define kfree_type_impl(kt_view, ptr) \
	kfree_type_impl_internal(kt_view, (ptr))

#else /* XNU_KERNEL_PRIVATE */

extern void *__unsafe_indexable kalloc_type_impl(
	kalloc_type_view_t  kt_view,
	zalloc_flags_t      flags);

static inline void *__unsafe_indexable
__kalloc_type_impl(
	kalloc_type_view_t  kt_view,
	zalloc_flags_t      flags)
{
	void *__unsafe_indexable addr = (kalloc_type_impl)(kt_view, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return addr;
}

#define kalloc_type_impl(ktv, fl) __kalloc_type_impl(ktv, fl)

extern void kfree_type_impl(
	kalloc_type_view_t  kt_view,
	void                *ptr __unsafe_indexable);

#endif /* XNU_KERNEL_PRIVATE */

/*!
 * @function zalloc
 *
 * @abstract
 * Allocates an element from a specified zone.
 *
 * @discussion
 * If the zone isn't exhaustible and is expandable, this call never fails.
 *
 * @param zone          the zone or zone view to allocate from
 *
 * @returns             NULL or the allocated element
 */
__attribute__((malloc))
extern void *__unsafe_indexable zalloc(
	zone_t          zone);

__attribute__((malloc))
__attribute__((overloadable))
static inline void *__unsafe_indexable
zalloc(zone_view_t view)
{
	return zalloc((zone_t)view);
}

__attribute__((malloc))
__attribute__((overloadable))
static inline void *__unsafe_indexable
zalloc(kalloc_type_view_t kt_view)
{
	return (kalloc_type_impl)(kt_view, Z_WAITOK);
}

/*!
 * @function zalloc_noblock
 *
 * @abstract
 * Allocates an element from a specified zone, but never blocks.
 *
 * @discussion
 * This call is suitable for preemptible code, however allocation
 * isn't allowed from interrupt context.
 *
 * @param zone          the zone or zone view to allocate from
 *
 * @returns             NULL or the allocated element
 */
__attribute__((malloc))
extern void *__unsafe_indexable zalloc_noblock(
	zone_t          zone);

__attribute__((malloc))
__attribute__((overloadable))
static inline void *__unsafe_indexable
zalloc_noblock(zone_view_t view)
{
	return zalloc_noblock((zone_t)view);
}

__attribute__((malloc))
__attribute__((overloadable))
static inline void *__unsafe_indexable
zalloc_noblock(kalloc_type_view_t kt_view)
{
	return (kalloc_type_impl)(kt_view, Z_NOWAIT);
}

/*!
 * @function zalloc_flags()
 *
 * @abstract
 * Allocates an element from a specified zone, with flags.
 *
 * @param zone          the zone or zone view to allocate from
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
__attribute__((malloc))
extern void *__unsafe_indexable zalloc_flags(
	zone_t          zone,
	zalloc_flags_t  flags);

__attribute__((malloc))
__attribute__((overloadable))
static inline void *__unsafe_indexable
__zalloc_flags(
	zone_t          zone,
	zalloc_flags_t  flags)
{
	void *__unsafe_indexable addr = (zalloc_flags)(zone, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return addr;
}

__attribute__((malloc))
__attribute__((overloadable))
static inline void *__unsafe_indexable
__zalloc_flags(
	zone_view_t     view,
	zalloc_flags_t  flags)
{
	return __zalloc_flags((zone_t)view, flags);
}

__attribute__((malloc))
__attribute__((overloadable))
static inline void *__unsafe_indexable
__zalloc_flags(
	kalloc_type_view_t  kt_view,
	zalloc_flags_t      flags)
{
	void *__unsafe_indexable addr = (kalloc_type_impl)(kt_view, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return addr;
}

__attribute__((malloc))
static inline void *__header_indexable
zalloc_flags_buf(
	zone_t          zone,
	zalloc_flags_t  flags)
{
	void *__unsafe_indexable addr = __zalloc_flags(zone, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return __unsafe_forge_bidi_indexable(void *, addr, zone_get_elem_size(zone));
}

#if XNU_KERNEL_PRIVATE && ZALLOC_TYPE_SAFE
#define zalloc_flags(zov, fl) __zalloc_cast(zov, (__zalloc_flags)(zov, fl))
#else
#define zalloc_flags(zov, fl) __zalloc_flags(zov, fl)
#endif

/*!
 * @macro zalloc_id
 *
 * @abstract
 * Allocates an element from a specified zone ID, with flags.
 *
 * @param zid           The proper @c ZONE_ID_* constant.
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
__attribute__((malloc))
extern void *__unsafe_indexable zalloc_id(
	zone_id_t       zid,
	zalloc_flags_t  flags);

__attribute__((malloc))
static inline void *__unsafe_indexable
__zalloc_id(
	zone_id_t       zid,
	zalloc_flags_t  flags)
{
	void *__unsafe_indexable addr = (zalloc_id)(zid, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return addr;
}

#if XNU_KERNEL_PRIVATE
#define zalloc_id(zid, flags) __zalloc_cast(zid, (__zalloc_id)(zid, flags))
#else
#define zalloc_id(zid, fl) __zalloc_id(zid, fl)
#endif

/*!
 * @function zalloc_ro
 *
 * @abstract
 * Allocates an element from a specified read-only zone.
 *
 * @param zone_id       the zone id to allocate from
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
__attribute__((malloc))
extern void *__unsafe_indexable zalloc_ro(
	zone_id_t       zone_id,
	zalloc_flags_t  flags);

__attribute__((malloc))
static inline void *__unsafe_indexable
__zalloc_ro(
	zone_id_t       zone_id,
	zalloc_flags_t  flags)
{
	void *__unsafe_indexable addr = (zalloc_ro)(zone_id, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return addr;
}

#if XNU_KERNEL_PRIVATE
#define zalloc_ro(zid, fl) __zalloc_cast(zid, (__zalloc_ro)(zid, fl))
#else
#define zalloc_ro(zid, fl) __zalloc_ro(zid, fl)
#endif

/*!
 * @function zalloc_ro_mut
 *
 * @abstract
 * Modifies an element from a specified read-only zone.
 *
 * @discussion
 * Modifying compiler-assisted authenticated pointers using this function will
 * not result in a signed pointer being written.  The caller is expected to
 * sign the value appropriately beforehand if they wish to do this.
 *
 * @param zone_id       the zone id to allocate from
 * @param elem          element to be modified
 * @param offset        offset from element
 * @param new_data      pointer to new data
 * @param new_data_size size of modification
 *
 */
extern void zalloc_ro_mut(
	zone_id_t       zone_id,
	void           *elem __unsafe_indexable,
	vm_offset_t     offset,
	const void     *new_data __sized_by(new_data_size),
	vm_size_t       new_data_size);

/*!
 * @function zalloc_ro_update_elem
 *
 * @abstract
 * Update the value of an entire element allocated in the read only allocator.
 *
 * @param zone_id       the zone id to allocate from
 * @param elem          element to be modified
 * @param new_data      pointer to new data
 *
 */
#define zalloc_ro_update_elem(zone_id, elem, new_data)  ({ \
	const typeof(*(elem)) *__new_data = (new_data);                        \
	zalloc_ro_mut(zone_id, elem, 0, __new_data, sizeof(*__new_data));      \
})

/*!
 * @function zalloc_ro_update_field
 *
 * @abstract
 * Update a single field of an element allocated in the read only allocator.
 *
 * @param zone_id       the zone id to allocate from
 * @param elem          element to be modified
 * @param field         the element field to be modified
 * @param new_data      pointer to new data
 *
 */
#define zalloc_ro_update_field(zone_id, elem, field, value)  ({ \
	const typeof((elem)->field) *__value = (value);                        \
	zalloc_ro_mut(zone_id, elem, offsetof(typeof(*(elem)), field),         \
	    __value, sizeof((elem)->field));                                   \
})

#define ZRO_ATOMIC_LONG(op) ZRO_ATOMIC_##op##_64

/*!
 * @enum zro_atomic_op_t
 *
 * @brief
 * Flags that can be used with @c zalloc_ro_*_atomic to specify the desired
 * atomic operations.
 *
 * @discussion
 * This enum provides all flavors of atomic operations supported in sizes 8,
 * 16, 32, 64 bits.
 *
 * @const ZRO_ATOMIC_OR_*
 * To perform an @s os_atomic_or
 *
 * @const ZRO_ATOMIC_XOR_*
 * To perform an @s os_atomic_xor
 *
 * @const ZRO_ATOMIC_AND_*
 * To perform an @s os_atomic_and
 *
 * @const ZRO_ATOMIC_ADD_*
 * To perform an @s os_atomic_add
 *
 * @const ZRO_ATOMIC_XCHG_*
 * To perform an @s os_atomic_xchg
 *
 */
__enum_decl(zro_atomic_op_t, uint32_t, {
	ZRO_ATOMIC_OR_8      = 0x00000010 | 1,
	ZRO_ATOMIC_OR_16     = 0x00000010 | 2,
	ZRO_ATOMIC_OR_32     = 0x00000010 | 4,
	ZRO_ATOMIC_OR_64     = 0x00000010 | 8,

	ZRO_ATOMIC_XOR_8     = 0x00000020 | 1,
	ZRO_ATOMIC_XOR_16    = 0x00000020 | 2,
	ZRO_ATOMIC_XOR_32    = 0x00000020 | 4,
	ZRO_ATOMIC_XOR_64    = 0x00000020 | 8,

	ZRO_ATOMIC_AND_8     = 0x00000030 | 1,
	ZRO_ATOMIC_AND_16    = 0x00000030 | 2,
	ZRO_ATOMIC_AND_32    = 0x00000030 | 4,
	ZRO_ATOMIC_AND_64    = 0x00000030 | 8,

	ZRO_ATOMIC_ADD_8     = 0x00000040 | 1,
	ZRO_ATOMIC_ADD_16    = 0x00000040 | 2,
	ZRO_ATOMIC_ADD_32    = 0x00000040 | 4,
	ZRO_ATOMIC_ADD_64    = 0x00000040 | 8,

	ZRO_ATOMIC_XCHG_8    = 0x00000050 | 1,
	ZRO_ATOMIC_XCHG_16   = 0x00000050 | 2,
	ZRO_ATOMIC_XCHG_32   = 0x00000050 | 4,
	ZRO_ATOMIC_XCHG_64   = 0x00000050 | 8,

	/* cconvenient spellings */
	ZRO_ATOMIC_OR_LONG   = ZRO_ATOMIC_LONG(OR),
	ZRO_ATOMIC_XOR_LONG  = ZRO_ATOMIC_LONG(XOR),
	ZRO_ATOMIC_AND_LONG  = ZRO_ATOMIC_LONG(AND),
	ZRO_ATOMIC_ADD_LONG  = ZRO_ATOMIC_LONG(ADD),
	ZRO_ATOMIC_XCHG_LONG = ZRO_ATOMIC_LONG(XCHG),
});

/*!
 * @function zalloc_ro_mut_atomic
 *
 * @abstract
 * Atomically update an offset in an element allocated in the read only
 * allocator. Do not use directly. Use via @c zalloc_ro_update_field_atomic.
 *
 * @param zone_id       the zone id to allocate from
 * @param elem          element to be modified
 * @param offset        offset in the element to be modified
 * @param op            atomic operation to perform (see @c zro_atomic_op_t)
 * @param value         value for the atomic operation
 *
 */
extern uint64_t zalloc_ro_mut_atomic(
	zone_id_t       zone_id,
	void           *elem __unsafe_indexable,
	vm_offset_t     offset,
	zro_atomic_op_t op,
	uint64_t        value);

/*!
 * @macro zalloc_ro_update_field_atomic
 *
 * @abstract
 * Atomically update a single field of an element allocated in the read only
 * allocator.
 *
 * @param zone_id       the zone id to allocate from
 * @param elem          element to be modified
 * @param field         the element field to be modified
 * @param op            atomic operation to perform (see @c zro_atomic_op_t)
 * @param value         value for the atomic operation
 *
 */
#define zalloc_ro_update_field_atomic(zone_id, elem, field, op, value)  ({ \
	const typeof((elem)->field) __value = (value);                         \
	static_assert(sizeof(__value) == (op & 0xf));                          \
	(os_atomic_basetypeof(&(elem)->field))zalloc_ro_mut_atomic(zone_id,    \
	    elem, offsetof(typeof(*(elem)), field), op, (uint64_t)__value);    \
})

/*!
 * @function zalloc_ro_clear
 *
 * @abstract
 * Zeroes an element from a specified read-only zone.
 *
 * @param zone_id       the zone id to allocate from
 * @param elem          element to be modified
 * @param offset        offset from element
 * @param size          size of modification
 */
extern void    zalloc_ro_clear(
	zone_id_t       zone_id,
	void           *elem __unsafe_indexable,
	vm_offset_t     offset,
	vm_size_t       size);

/*!
 * @function zalloc_ro_clear_field
 *
 * @abstract
 * Zeroes the specified field of an element from a specified read-only zone.
 *
 * @param zone_id       the zone id to allocate from
 * @param elem          element to be modified
 * @param field         offset from element
 */
#define zalloc_ro_clear_field(zone_id, elem, field) \
	zalloc_ro_clear(zone_id, elem, offsetof(typeof(*(elem)), field), \
	    sizeof((elem)->field))

/*!
 * @function zfree_id()
 *
 * @abstract
 * Frees an element previously allocated with @c zalloc_id().
 *
 * @param zone_id       the zone id to free the element to.
 * @param addr          the address to free
 */
extern void     zfree_id(
	zone_id_t       zone_id,
	void           *addr __unsafe_indexable);
#define zfree_id(zid, elem) ({ \
	zone_id_t __zfree_zid = (zid); \
	(zfree_id)(__zfree_zid, (void *)os_ptr_load_and_erase(elem)); \
})


/*!
 * @function zfree_ro()
 *
 * @abstract
 * Frees an element previously allocated with @c zalloc_ro().
 *
 * @param zone_id       the zone id to free the element to.
 * @param addr          the address to free
 */
extern void     zfree_ro(
	zone_id_t       zone_id,
	void           *addr __unsafe_indexable);
#define zfree_ro(zid, elem) ({ \
	zone_id_t __zfree_zid = (zid); \
	(zfree_ro)(__zfree_zid, (void *)os_ptr_load_and_erase(elem)); \
})


/*!
 * @function zfree
 *
 * @abstract
 * Frees an element allocated with @c zalloc*.
 *
 * @discussion
 * If the element being freed doesn't belong to the specified zone,
 * then this call will panic.
 *
 * @param zone          the zone or zone view to free the element to.
 * @param elem          the element to free
 */
extern void     zfree(
	zone_t          zone,
	void           *elem __unsafe_indexable);

__attribute__((overloadable))
static inline void
zfree(
	zone_view_t     view,
	void           *elem __unsafe_indexable)
{
	zfree((zone_t)view, elem);
}

__attribute__((overloadable))
static inline void
zfree(
	kalloc_type_view_t   kt_view,
	void                *elem __unsafe_indexable)
{
	return kfree_type_impl(kt_view, elem);
}

#define zfree(zone, elem) ({ \
	__auto_type __zfree_zone = (zone); \
	(zfree)(__zfree_zone, (void *)os_ptr_load_and_erase(elem)); \
})


/* deprecated KPIS */

__zalloc_deprecated("use zone_create()")
extern zone_t   zinit(
	vm_size_t       size,           /* the size of an element */
	vm_size_t       maxmem,         /* maximum memory to use */
	vm_size_t       alloc,          /* allocation size */
	const char      *name __unsafe_indexable);

#pragma mark: implementation details

#define __ZONE_DECLARE_TYPE(var, type_t) __ZONE_DECLARE_TYPE2(var, type_t)
#define __ZONE_DECLARE_TYPE2(var, type_t) \
	__attribute__((visibility("hidden"))) \
	extern type_t *__single __zalloc__##var##__type_name

#ifdef XNU_KERNEL_PRIVATE
#pragma mark - XNU only interfaces

#include <kern/cpu_number.h>

#pragma GCC visibility push(hidden)

#pragma mark XNU only: zalloc (extended)

#define ZALIGN_NONE             (sizeof(uint8_t)  - 1)
#define ZALIGN_16               (sizeof(uint16_t) - 1)
#define ZALIGN_32               (sizeof(uint32_t) - 1)
#define ZALIGN_PTR              (sizeof(void *)   - 1)
#define ZALIGN_64               (sizeof(uint64_t) - 1)
#define ZALIGN(t)               (_Alignof(t)      - 1)


/*!
 * @function zalloc_permanent_tag()
 *
 * @abstract
 * Allocates a permanent element from the permanent zone
 *
 * @discussion
 * Memory returned by this function is always 0-initialized.
 * Note that the size of this allocation can not be determined
 * by zone_element_size so it should not be used for copyio.
 *
 * @param size          the element size (must be smaller than PAGE_SIZE)
 * @param align_mask    the required alignment for this allocation
 * @param tag           the tag to use for allocations larger than a page.
 *
 * @returns             the allocated element
 */
__attribute__((malloc))
extern void *__sized_by(size) zalloc_permanent_tag(
	vm_size_t       size,
	vm_offset_t     align_mask,
	vm_tag_t        tag)
__attribute__((__diagnose_if__((align_mask & (align_mask + 1)),
    "align mask looks invalid", "error")));

/*!
 * @function zalloc_permanent()
 *
 * @abstract
 * Allocates a permanent element from the permanent zone
 *
 * @discussion
 * Memory returned by this function is always 0-initialized.
 * Note that the size of this allocation can not be determined
 * by zone_element_size so it should not be used for copyio.
 *
 * @param size          the element size (must be smaller than PAGE_SIZE)
 * @param align_mask    the required alignment for this allocation
 *
 * @returns             the allocated element
 */
#define zalloc_permanent(size, align) \
	zalloc_permanent_tag(size, align, VM_KERN_MEMORY_KALLOC)

/*!
 * @function zalloc_permanent_type()
 *
 * @abstract
 * Allocates a permanent element of a given type with its natural alignment.
 *
 * @discussion
 * Memory returned by this function is always 0-initialized.
 *
 * @param type_t        the element type
 *
 * @returns             the allocated element
 */
#define zalloc_permanent_type(type_t) \
	__unsafe_forge_single(type_t *, \
	    zalloc_permanent(sizeof(type_t), ZALIGN(type_t)))

/*!
 * @function zalloc_first_proc_made()
 *
 * @abstract
 * Declare that the "early" allocation phase is done.
 */
extern void zalloc_first_proc_made(void);
/*!
 * @function zalloc_iokit_lockdown()
 *
 * @abstract
 * Declare that iokit matching has started.
 */
extern void zalloc_iokit_lockdown(void);

#pragma mark XNU only: per-cpu allocations

/*!
 * @macro zpercpu_get_cpu()
 *
 * @abstract
 * Get a pointer to a specific CPU slot of a given per-cpu variable.
 *
 * @param ptr           the per-cpu pointer (returned by @c zalloc_percpu*()).
 * @param cpu           the specified CPU number as returned by @c cpu_number()
 *
 * @returns             the per-CPU slot for @c ptr for the specified CPU.
 */
#define zpercpu_get_cpu(ptr, cpu) \
	__zpcpu_cast(ptr, __zpcpu_demangle(ptr) + ptoa((unsigned)(cpu)))

/*!
 * @macro zpercpu_get()
 *
 * @abstract
 * Get a pointer to the current CPU slot of a given per-cpu variable.
 *
 * @param ptr           the per-cpu pointer (returned by @c zalloc_percpu*()).
 *
 * @returns             the per-CPU slot for @c ptr for the current CPU.
 */
#define zpercpu_get(ptr) \
	zpercpu_get_cpu(ptr, cpu_number())

/*!
 * @macro zpercpu_foreach()
 *
 * @abstract
 * Enumerate all per-CPU slots by address.
 *
 * @param it            the name for the iterator
 * @param ptr           the per-cpu pointer (returned by @c zalloc_percpu*()).
 */
#define zpercpu_foreach(it, ptr) \
	for (typeof(ptr) it = zpercpu_get_cpu(ptr, 0), \
	    __end_##it = zpercpu_get_cpu(ptr, zpercpu_count()); \
	    it < __end_##it; it = __zpcpu_next(it))

/*!
 * @macro zpercpu_foreach_cpu()
 *
 * @abstract
 * Enumerate all per-CPU slots by CPU slot number.
 *
 * @param cpu           the name for cpu number iterator.
 */
#define zpercpu_foreach_cpu(cpu) \
	for (unsigned cpu = 0; cpu < zpercpu_count(); cpu++)

/*!
 * @function zalloc_percpu()
 *
 * @abstract
 * Allocates an element from a per-cpu zone.
 *
 * @discussion
 * The returned pointer cannot be used directly and must be manipulated
 * through the @c zpercpu_get*() interfaces.
 *
 * @param zone_or_view  the zone or zone view to allocate from
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
extern void *__zpercpu zalloc_percpu(
	zone_or_view_t  zone_or_view,
	zalloc_flags_t  flags);

static inline void *__zpercpu
__zalloc_percpu(
	zone_or_view_t  zone_or_view,
	zalloc_flags_t  flags)
{
	void *__unsafe_indexable addr = (zalloc_percpu)(zone_or_view, flags);
	if (flags & Z_NOFAIL) {
		__builtin_assume(addr != NULL);
	}
	return addr;
}

#define zalloc_percpu(zov, fl) __zalloc_percpu(zov, fl)

/*!
 * @function zfree_percpu()
 *
 * @abstract
 * Frees an element previously allocated with @c zalloc_percpu().
 *
 * @param zone_or_view  the zone or zone view to free the element to.
 * @param addr          the address to free
 */
extern void     zfree_percpu(
	zone_or_view_t  zone_or_view,
	void *__zpercpu addr);

/*!
 * @function zalloc_percpu_permanent()
 *
 * @abstract
 * Allocates a permanent percpu-element from the permanent percpu zone.
 *
 * @discussion
 * Memory returned by this function is always 0-initialized.
 *
 * @param size          the element size (must be smaller than PAGE_SIZE)
 * @param align_mask    the required alignment for this allocation
 *
 * @returns             the allocated element
 */
extern void *__zpercpu zalloc_percpu_permanent(
	vm_size_t       size,
	vm_offset_t     align_mask);

/*!
 * @function zalloc_percpu_permanent_type()
 *
 * @abstract
 * Allocates a permanent percpu-element from the permanent percpu zone of a given
 * type with its natural alignment.
 *
 * @discussion
 * Memory returned by this function is always 0-initialized.
 *
 * @param type_t        the element type
 *
 * @returns             the allocated element
 */
#define zalloc_percpu_permanent_type(type_t) \
	((type_t *__zpercpu)zalloc_percpu_permanent(sizeof(type_t), ZALIGN(type_t)))


#pragma mark XNU only: SMR support for zones

struct smr;

/*!
 * @typedef zone_smr_free_cb_t
 *
 * @brief
 * Type for the delayed free callback for SMR zones.
 *
 * @description
 * This function is called before an element is reused,
 * or when memory is returned to the system.
 *
 * This function MUST zero the element, and if no special
 * action is to be taken on free, then @c bzero() is a fine
 * callback to use.
 *
 * This function also must be preemption-disabled safe,
 * as it runs with preemption disabled.
 *
 *
 * Note that this function should only clean the fields
 * that must be preserved for stale SMR readers to see.
 * Any field that is accessed after element validation
 * such as a try-retain or acquiring a lock on it must
 * be cleaned up much earlier as they might hold onto
 * expensive resources.
 *
 * The suggested pattern for an SMR type using this facility,
 * is to have 2 functions:
 *
 * - one "retire" stage that tries to clean up as much from
 *   the element as possible, with great care to leave no dangling
 *   pointers around, as elements in this stage might linger
 *   in the allocator for a long time, and this could possibly
 *   be abused during UaF exploitation.
 *
 * - one "smr_free" function which cleans up whatever was left,
 *   and zeroes the rest of the element.
 *
 * <code>
 *     void
 *     type_retire(type_t elem)
 *     {
 *         // invalidating the element makes most fields
 *         // inaccessible to readers.
 *         type_mark_invalid(elem);
 *
 *         // do cleanups for things requiring a validity check
 *         kfree_type(some_type_t, elem->expensive_thing);
 *         type_remove_from_global_list(&elem->linkage);
 *
 *         zfree_smr(type_zone, elem);
 *     }
 *
 *     void
 *     type_smr_free(void *_elem)
 *     {
 *         type_t elem = elem;
 *
 *         // cleanup fields that are used to "find" this element
 *         // and that SMR readers may access hazardously.
 *         lck_ticket_destroy(&elem->lock);
 *         kfree_data(elem->key, elem->keylen);
 *
 *         // compulsory: element must be zeroed fully
 *         bzero(elem, sizeof(*elem));
 *     }
 * </code>
 */
typedef void (*zone_smr_free_cb_t)(void *, size_t);

/*!
 * @function zone_enable_smr()
 *
 * @abstract
 * Enable SMR for a zone.
 *
 * @discussion
 * This can only be done once, and must be done before
 * the first allocation is made with this zone.
 *
 * @param zone          the zone to enable SMR for
 * @param smr           the smr domain to use
 * @param free_cb       the free callback to use
 */
extern void     zone_enable_smr(
	zone_t                  zone,
	struct smr             *smr,
	zone_smr_free_cb_t      free_cb);

/*!
 * @function zone_id_enable_smr()
 *
 * @abstract
 * Enable SMR for a zone ID.
 *
 * @discussion
 * This can only be done once, and must be done before
 * the first allocation is made with this zone.
 *
 * @param zone_id       the zone to enable SMR for
 * @param smr           the smr domain to use
 * @param free_cb       the free callback to use
 */
#define zone_id_enable_smr(zone_id, smr, free_cb)  ({ \
	void (*__cb)(typeof(__zalloc__##zone_id##__type_name), vm_size_t);      \
                                                                                \
	__cb = (free_cb);                                                       \
	zone_enable_smr(zone_by_id(zone_id), smr, (zone_smr_free_cb_t)__cb);    \
})

/*!
 * @macro zalloc_smr()
 *
 * @abstract
 * Allocates an element from an SMR enabled zone
 *
 * @discussion
 * The SMR domain for this zone MUST NOT be entered when calling zalloc_smr().
 *
 * @param zone          the zone to allocate from
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
#define zalloc_smr(zone, flags) \
	zalloc_flags(zone, flags)

/*!
 * @macro zalloc_id_smr()
 *
 * @abstract
 * Allocates an element from a specified zone ID with SMR enabled.
 *
 * @param zid           The proper @c ZONE_ID_* constant.
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
#define zalloc_id_smr(zid, flags) \
	zalloc_id(zid, flags)

/*!
 * @macro zfree_smr()
 *
 * @abstract
 * Frees an element previously allocated with @c zalloc_smr().
 *
 * @discussion
 * When zfree_smr() is called, then the element is not immediately zeroed,
 * and the "free" callback that has been registered with the zone will
 * run later (@see zone_smr_free_cb_t).
 *
 * The SMR domain for this zone MUST NOT be entered when calling zfree_smr().
 *
 *
 * It is guaranteed that the SMR timestamp associated with an element
 * will always be equal or greater than the stamp associated with
 * elements freed before it on the same thread.
 *
 * It means that when freeing multiple elements in a sequence, these
 * must be freed in topological order (parents before children).
 *
 * It is worth noting that calling zfree_smr() on several elements
 * in a given order doesn't necessarily mean they will be effectively
 * reused or cleaned up in that same order, only that their SMR clocks
 * will expire in that order.
 *
 *
 * @param zone          the zone to free the element to.
 * @param elem          the address to free
 */
extern void     zfree_smr(
	zone_t          zone,
	void           *elem __unsafe_indexable);
#define zfree_smr(zone, elem) ({ \
	__auto_type __zfree_zone = (zone); \
	(zfree_smr)(__zfree_zone, (void *)os_ptr_load_and_erase(elem)); \
})


/*!
 * @function zfree_id_smr()
 *
 * @abstract
 * Frees an element previously allocated with @c zalloc_id_smr().
 *
 * @param zone_id       the zone id to free the element to.
 * @param addr          the address to free
 */
extern void     zfree_id_smr(
	zone_id_t       zone_id,
	void           *addr __unsafe_indexable);
#define zfree_id_smr(zid, elem) ({ \
	zone_id_t __zfree_zid = (zid); \
	(zfree_id_smr)(__zfree_zid, (void *)os_ptr_load_and_erase(elem)); \
})

/*!
 * @macro zfree_smr_noclear()
 *
 * @abstract
 * Frees an element previously allocated with @c zalloc_smr().
 *
 * @discussion
 * This variant doesn't clear the pointer passed as an argument,
 * as it is often required for SMR algorithms to function correctly
 * to leave pointers "dangling" to an extent.
 *
 * However it expects the field in question to be an SMR_POINTER()
 * struct.
 *
 * @param zone          the zone to free the element to.
 * @param elem          the address to free
 */
#define zfree_smr_noclear(zone, elem) \
	(zfree_smr)(zone, (void *)smr_unsafe_load(&(elem)))

/*!
 * @macro zfree_id_smr_noclear()
 *
 * @abstract
 * Frees an element previously allocated with @c zalloc_id_smr().
 *
 * @discussion
 * This variant doesn't clear the pointer passed as an argument,
 * as it is often required for SMR algorithms to function correctly
 * to leave pointers "dangling" to an extent.
 *
 * However it expects the field in question to be an SMR_POINTER()
 * struct.
 *
 * @param zone          the zone to free the element to.
 * @param elem          the address to free
 */
#define zfree_id_smr_noclear(zone, elem) \
	(zfree_id_smr)(zone, (void *)smr_unsafe_load(&(elem)))


#pragma mark XNU only: zone creation (extended)

/*!
 * @enum zone_reserved_id_t
 *
 * @abstract
 * Well known pre-registered zones, allowing use of zone_id_require()
 *
 * @discussion
 * @c ZONE_ID__* aren't real zone IDs.
 *
 * @c ZONE_ID__ZERO reserves zone index 0 so that it can't be used, as 0 is too
 * easy a value to produce (by malice or accident).
 *
 * @c ZONE_ID__FIRST_RO_EXT is the first external read only zone ID that corresponds
 * to the first @c zone_create_ro_id_t. There is a 1:1 mapping between zone IDs
 * belonging to [ZONE_ID__FIRST_RO_EXT - ZONE_ID__LAST_RO_EXT] and zone creations IDs
 * listed in @c zone_create_ro_id_t.
 *
 * @c ZONE_ID__FIRST_DYNAMIC is the first dynamic zone ID that can be used by
 * @c zone_create().
 */
__enum_decl(zone_reserved_id_t, zone_id_t, {
	ZONE_ID__ZERO,

	ZONE_ID_PERMANENT,
	ZONE_ID_PERCPU_PERMANENT,

	ZONE_ID_THREAD_RO,
	ZONE_ID_MAC_LABEL,
	ZONE_ID_PROC_RO,
	ZONE_ID_PROC_SIGACTS_RO,
	ZONE_ID_KAUTH_CRED,
	ZONE_ID_CS_BLOB,

	ZONE_ID_SANDBOX_RO,
	ZONE_ID_PROFILE_RO,
	ZONE_ID_PROTOBOX,
	ZONE_ID_SB_FILTER,
	ZONE_ID_AMFI_OSENTITLEMENTS,

	ZONE_ID__FIRST_RO = ZONE_ID_THREAD_RO,
	ZONE_ID__FIRST_RO_EXT = ZONE_ID_SANDBOX_RO,
	ZONE_ID__LAST_RO_EXT = ZONE_ID_AMFI_OSENTITLEMENTS,
	ZONE_ID__LAST_RO = ZONE_ID__LAST_RO_EXT,

	ZONE_ID_PMAP,
	ZONE_ID_VM_MAP,
	ZONE_ID_VM_MAP_ENTRY,
	ZONE_ID_VM_MAP_HOLES,
	ZONE_ID_VM_MAP_COPY,
	ZONE_ID_VM_PAGES,
	ZONE_ID_IPC_PORT,
	ZONE_ID_IPC_PORT_SET,
	ZONE_ID_IPC_KMSG,
	ZONE_ID_IPC_VOUCHERS,
	ZONE_ID_PROC_TASK,
	ZONE_ID_THREAD,
	ZONE_ID_TURNSTILE,
	ZONE_ID_SEMAPHORE,
	ZONE_ID_SELECT_SET,
	ZONE_ID_FILEPROC,

#if !CONFIG_MBUF_MCACHE
	ZONE_ID_MBUF_REF,
	ZONE_ID_MBUF,
	ZONE_ID_CLUSTER_2K,
	ZONE_ID_CLUSTER_4K,
	ZONE_ID_CLUSTER_16K,
	ZONE_ID_MBUF_CLUSTER_2K,
	ZONE_ID_MBUF_CLUSTER_4K,
	ZONE_ID_MBUF_CLUSTER_16K,
#endif /* !CONFIG_MBUF_MCACHE */

	ZONE_ID__FIRST_DYNAMIC,
});

/*!
 * @const ZONE_ID_ANY
 * The value to pass to @c zone_create_ext() to allocate a non pre-registered
 * Zone ID.
 */
#define ZONE_ID_ANY ((zone_id_t)-1)

/*!
 * @const ZONE_ID_INVALID
 * An invalid zone_id_t that corresponds to nothing.
 */
#define ZONE_ID_INVALID ((zone_id_t)-2)

/**!
 * @function zone_by_id
 *
 * @param zid           the specified zone ID.
 * @returns             the zone with that ID.
 */
zone_t zone_by_id(
	size_t                  zid) __pure2;

/**!
 * @function zone_name
 *
 * @param zone          the specified zone
 * @returns             the name of the specified zone.
 */
const char *__unsafe_indexable zone_name(
	zone_t                  zone);

/**!
 * @function zone_heap_name
 *
 * @param zone          the specified zone
 * @returns             the name of the heap this zone is part of, or "".
 */
const char *__unsafe_indexable zone_heap_name(
	zone_t                  zone);

/*!
 * @function zone_create_ext
 *
 * @abstract
 * Creates a zone with the specified parameters.
 *
 * @discussion
 * This is an extended version of @c zone_create().
 *
 * @param name          the name for the new zone.
 * @param size          the size of the elements returned by this zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 * @param desired_zid   a @c zone_reserved_id_t value or @c ZONE_ID_ANY.
 *
 * @param extra_setup   a block that can perform non trivial initialization
 *                      on the zone before it is marked valid.
 *                      This block can call advanced setups like:
 *                      - zone_set_exhaustible()
 *
 * @returns             the created zone, this call never fails.
 */
extern zone_t   zone_create_ext(
	const char             *name __unsafe_indexable,
	vm_size_t               size,
	zone_create_flags_t     flags,
	zone_id_t               desired_zid,
	void                  (^extra_setup)(zone_t));

/*!
 * @macro ZONE_DECLARE
 *
 * @abstract
 * Declares a zone variable and its associated type.
 *
 * @param var           the name of the variable to declare.
 * @param type_t        the type of elements in the zone.
 */
#define ZONE_DECLARE(var, type_t) \
	extern zone_t var; \
	__ZONE_DECLARE_TYPE(var, type_t)

/*!
 * @macro ZONE_DECLARE_ID
 *
 * @abstract
 * Declares the type associated with a zone ID.
 *
 * @param id            the name of zone ID to associate a type with.
 * @param type_t        the type of elements in the zone.
 */
#define ZONE_DECLARE_ID(id, type_t) \
	__ZONE_DECLARE_TYPE(id, type_t)

/*!
 * @macro ZONE_DEFINE
 *
 * @abstract
 * Declares a zone variable to automatically initialize with the specified
 * parameters.
 *
 * @discussion
 * Using ZONE_DEFINE_TYPE is preferred, but not always possible.
 *
 * @param var           the name of the variable to declare.
 * @param name          the name for the zone
 * @param size          the size of the elements returned by this zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 */
#define ZONE_DEFINE(var, name, size, flags) \
	SECURITY_READ_ONLY_LATE(zone_t) var; \
	static_assert(((flags) & ZC_DESTRUCTIBLE) == 0); \
	static __startup_data struct zone_create_startup_spec \
	__startup_zone_spec_ ## var = { &var, name, size, flags, \
	    ZONE_ID_ANY, NULL }; \
	STARTUP_ARG(ZALLOC, STARTUP_RANK_FOURTH, zone_create_startup, \
	    &__startup_zone_spec_ ## var)

/*!
 * @macro ZONE_DEFINE_TYPE
 *
 * @abstract
 * Defines a zone variable to automatically initialize with the specified
 * parameters, associated with a particular type.
 *
 * @param var           the name of the variable to declare.
 * @param name          the name for the zone
 * @param type_t        the type of elements in the zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 */
#define ZONE_DEFINE_TYPE(var, name, type_t, flags) \
	ZONE_DEFINE(var, name, sizeof(type_t), flags); \
	__ZONE_DECLARE_TYPE(var, type_t)

/*!
 * @macro ZONE_DEFINE_ID
 *
 * @abstract
 * Initializes a given zone automatically during startup with the specified
 * parameters.
 *
 * @param zid           a @c zone_reserved_id_t value.
 * @param name          the name for the zone
 * @param type_t        the type of elements in the zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 */
#define ZONE_DEFINE_ID(zid, name, type_t, flags) \
	ZONE_DECLARE_ID(zid, type_t); \
	ZONE_INIT(NULL, name, sizeof(type_t), flags, zid, NULL)

/*!
 * @macro ZONE_INIT
 *
 * @abstract
 * Initializes a given zone automatically during startup with the specified
 * parameters.
 *
 * @param var           the name of the variable to initialize.
 * @param name          the name for the zone
 * @param size          the size of the elements returned by this zone.
 * @param flags         a set of @c zone_create_flags_t flags.
 * @param desired_zid   a @c zone_reserved_id_t value or @c ZONE_ID_ANY.
 * @param extra_setup   a block that can perform non trivial initialization
 *                      (@see @c zone_create_ext()).
 */
#define ZONE_INIT(var, name, size, flags, desired_zid, extra_setup) \
	__ZONE_INIT(__LINE__, var, name, size, flags, desired_zid, extra_setup)

/*!
 * @function zone_id_require
 *
 * @abstract
 * Requires for a given pointer to belong to the specified zone, by ID and size.
 *
 * @discussion
 * The function panics if the check fails as it indicates that the kernel
 * internals have been compromised.
 *
 * This is a variant of @c zone_require() which:
 * - isn't sensitive to @c zone_t::elem_size being compromised,
 * - is slightly faster as it saves one load and a multiplication.
 *
 * @param zone_id       the zone ID the address needs to belong to.
 * @param elem_size     the size of elements for this zone.
 * @param addr          the element address to check.
 */
extern void     zone_id_require(
	zone_id_t               zone_id,
	vm_size_t               elem_size,
	void                   *addr __unsafe_indexable);

/*!
 * @function zone_id_require_aligned
 *
 * @abstract
 * Requires for a given pointer to belong to the specified zone, by ID and size.
 *
 * @discussion
 * Similar to @c zone_id_require() but does more checks such as whether the
 * element is properly aligned.
 *
 * @param zone_id       the zone ID the address needs to belong to.
 * @param addr          the element address to check.
 */
extern void     zone_id_require_aligned(
	zone_id_t               zone_id,
	void                   *addr __unsafe_indexable);

/* Make zone exhaustible, to be called from the zone_create_ext() setup hook */
extern void     zone_set_exhaustible(
	zone_t                  zone,
	vm_size_t               max_elements,
	bool                    exhausts_by_design);

/*!
 * @function zone_raise_reserve()
 *
 * @brief
 * Used to raise the reserve on a zone.
 *
 * @discussion
 * Can be called from any context (zone_create_ext() setup hook or after).
 */
extern void     zone_raise_reserve(
	zone_or_view_t          zone_or_view,
	uint16_t                min_elements);

/*!
 * @function zone_fill_initially
 *
 * @brief
 * Initially fill a non collectable zone to have the specified amount of
 * elements.
 *
 * @discussion
 * This function must be called on a non collectable permanent zone before it
 * has been used yet.
 *
 * @param zone          The zone to fill.
 * @param nelems        The number of elements to be able to hold.
 */
extern void     zone_fill_initially(
	zone_t                  zone,
	vm_size_t               nelems);

/*!
 * @function zone_drain()
 *
 * @abstract
 * Forces a zone to be drained (have all its data structures freed
 * back to its data store, and empty pages returned to the system).
 *
 * @param zone          the zone id to free the objects to.
 */
extern void zone_drain(
	zone_t                  zone);

/*!
 * @struct zone_basic_stats
 *
 * @abstract
 * Used to report basic statistics about a zone.
 *
 * @field zbs_avail     the number of elements in a zone.
 * @field zbs_alloc     the number of allocated elements in a zone.
 * @field zbs_free      the number of free elements in a zone.
 * @field zbs_cached    the number of free elements in the per-CPU caches.
 *                      (included in zbs_free).
 * @field zbs_alloc_fail
 *                      the number of allocation failures.
 */
struct zone_basic_stats {
	uint64_t        zbs_avail;
	uint64_t        zbs_alloc;
	uint64_t        zbs_free;
	uint64_t        zbs_cached;
	uint64_t        zbs_alloc_fail;
};

/*!
 * @function zone_get_stats
 *
 * @abstract
 * Retrieves statistics about zones, include its per-CPU caches.
 *
 * @param zone          the zone to collect stats from.
 * @param stats         the statistics to fill.
 */
extern void zone_get_stats(
	zone_t                  zone,
	struct zone_basic_stats *stats);


/*!
 * @typedef zone_exhausted_cb_t
 *
 * @brief
 * The callback type for the ZONE_EXHAUSTED event.
 */
typedef void (zone_exhausted_cb_t)(zone_id_t zid, zone_t zone, bool exhausted);

/*!
 * @brief
 * The @c ZONE_EXHAUSTED event, which is emited when an exhaustible zone hits its
 * wiring limit.
 *
 * @discussion
 * The @c ZONE_EXHAUSTED event is emitted from a thread that is currently
 * performing zone expansion and no significant amount of work can be performed
 * from this context.
 *
 * In particular, those callbacks cannot allocate any memory, it is expected
 * that they will filter if the zone is of interest, and wake up another thread
 * to perform the actual work (for example via thread call).
 */
EVENT_DECLARE(ZONE_EXHAUSTED, zone_exhausted_cb_t);


#pragma mark XNU only: zone views

/*!
 * @enum zone_kheap_id_t
 *
 * @brief
 * Enumerate a particular kalloc heap.
 *
 * @discussion
 * More documentation about heaps is available in @c <kern/kalloc.h>.
 *
 * @const KHEAP_ID_NONE
 * This value denotes regular zones, not used by kalloc.
 *
 * @const KHEAP_ID_SHARED
 * Indicates zones part of the KHEAP_SHARED heap.
 *
 * @const KHEAP_ID_DATA_BUFFERS
 * Indicates zones part of the KHEAP_DATA_BUFFERS heap.
 *
 * @const KHEAP_ID_KT_VAR
 * Indicates zones part of the KHEAP_KT_VAR heap.
 */
__enum_decl(zone_kheap_id_t, uint8_t, {
	KHEAP_ID_NONE,
	KHEAP_ID_SHARED,
	KHEAP_ID_DATA_BUFFERS,
	KHEAP_ID_KT_VAR,

#define KHEAP_ID_COUNT (KHEAP_ID_KT_VAR + 1)
});

/*!
 * @macro ZONE_VIEW_DECLARE
 *
 * @abstract
 * (optionally) declares a zone view (in a header).
 *
 * @param var           the name for the zone view.
 */
#define ZONE_VIEW_DECLARE(var) \
	extern struct zone_view var[1]

/*!
 * @macro ZONE_VIEW_DEFINE
 *
 * @abstract
 * Defines a given zone view and what it points to.
 *
 * @discussion
 * Zone views can either share a pre-existing zone,
 * or perform a lookup into a kalloc heap for the zone
 * backing the bucket of the proper size.
 *
 * Zone views are initialized during the @c STARTUP_SUB_ZALLOC phase,
 * as the last rank. If views on zones are created, these must have been
 * created before this stage.
 *
 * This macro should not be used to create zone views from default
 * kalloc heap, KALLOC_TYPE_DEFINE should be used instead.
 *
 * @param var           the name for the zone view.
 * @param name          a string describing the zone view.
 * @param heap_or_zone  a @c KHEAP_ID_* constant or a pointer to a zone.
 * @param size          the element size to be allocated from this view.
 */
#define ZONE_VIEW_DEFINE(var, name, heap_or_zone, size) \
	SECURITY_READ_ONLY_LATE(struct zone_view) var[1] = { { \
	    .zv_name = (name), \
	} }; \
	static __startup_data struct zone_view_startup_spec \
	__startup_zone_view_spec_ ## var = { var, { heap_or_zone }, size }; \
	STARTUP_ARG(ZALLOC, STARTUP_RANK_MIDDLE, zone_view_startup_init, \
	    &__startup_zone_view_spec_ ## var)


#pragma mark XNU only: batched allocations

/*!
 * @typedef zstack_t
 *
 * @brief
 * A stack of allocated elements chained with delta encoding.
 *
 * @discussion
 * Some batch allocation interfaces interact with the data heap
 * where leaking kernel pointers is not acceptable. This is why
 * element offsets are used instead.
 */
typedef struct zstack {
	vm_offset_t     z_head;
	uint32_t        z_count;
} zstack_t;

/*!
 * @function zstack_push
 *
 * @brief
 * Push a given element onto a zstack.
 */
extern void zstack_push(
	zstack_t               *stack,
	void                   *elem);

/*!
 * @function zstack_pop
 *
 * @brief
 * Pops an element from a zstack, the caller must check it's not empty.
 */
void *zstack_pop(
	zstack_t               *stack);

/*!
 * @function zstack_empty
 *
 * @brief
 * Returns whether a stack is empty.
 */
static inline uint32_t
zstack_count(zstack_t stack)
{
	return stack.z_count;
}

/*!
 * @function zstack_empty
 *
 * @brief
 * Returns whether a stack is empty.
 */
static inline bool
zstack_empty(zstack_t stack)
{
	return zstack_count(stack) == 0;
}

static inline zstack_t
zstack_load_and_erase(zstack_t *stackp)
{
	zstack_t stack = *stackp;

	*stackp = (zstack_t){ };
	return stack;
}

/*!
 * @function zfree_nozero
 *
 * @abstract
 * Frees an element allocated with @c zalloc*, without zeroing it.
 *
 * @discussion
 * This is for the sake of networking only, no one else should use this.
 *
 * @param zone_id       the zone id to free the element to.
 * @param elem          the element to free
 */
extern void zfree_nozero(
	zone_id_t               zone_id,
	void                   *elem __unsafe_indexable);
#define zfree_nozero(zone_id, elem) ({ \
	zone_id_t __zfree_zid = (zone_id); \
	(zfree_nozero)(__zfree_zid, (void *)os_ptr_load_and_erase(elem)); \
})

/*!
 * @function zalloc_n
 *
 * @abstract
 * Allocates a batch of elements from the specified zone.
 *
 * @discussion
 * This is for the sake of networking only, no one else should use this.
 *
 * @param zone_id       the zone id to allocate the element from.
 * @param count         how many elements to allocate (less might be returned)
 * @param flags         a set of @c zone_create_flags_t flags.
 */
extern zstack_t zalloc_n(
	zone_id_t               zone_id,
	uint32_t                count,
	zalloc_flags_t          flags);

/*!
 * @function zfree_n
 *
 * @abstract
 * Batched variant of zfree(): frees a stack of elements.
 *
 * @param zone_id       the zone id to free the element to.
 * @param stack         a stack of elements to free.
 */
extern void zfree_n(
	zone_id_t               zone_id,
	zstack_t                stack);
#define zfree_n(zone_id, stack) ({ \
	zone_id_t __zfree_zid = (zone_id); \
	(zfree_n)(__zfree_zid, zstack_load_and_erase(&(stack))); \
})

/*!
 * @function zfree_nozero_n
 *
 * @abstract
 * Batched variant of zfree_nozero(): frees a stack of elements without zeroing
 * them.
 *
 * @discussion
 * This is for the sake of networking only, no one else should use this.
 *
 * @param zone_id       the zone id to free the element to.
 * @param stack         a stack of elements to free.
 */
extern void zfree_nozero_n(
	zone_id_t               zone_id,
	zstack_t                stack);
#define zfree_nozero_n(zone_id, stack) ({ \
	zone_id_t __zfree_zid = (zone_id); \
	(zfree_nozero_n)(__zfree_zid, zstack_load_and_erase(&(stack))); \
})

#pragma mark XNU only: cached objects

/*!
 * @typedef zone_cache_ops_t
 *
 * @brief
 * A set of callbacks used for a zcache (cache of composite objects).
 *
 * @field zc_op_alloc
 * The callback to "allocate" a cached object from scratch.
 *
 * @field zc_op_mark_valid
 * The callback that is called when a cached object is being reused,
 * will typically call @c zcache_mark_valid() on the various
 * sub-pieces of the composite cached object.
 *
 * @field zc_op_mark_invalid
 * The callback that is called when a composite object is being freed
 * to the cache. This will typically call @c zcache_mark_invalid()
 * on the various sub-pieces of the composite object.
 *
 * @field zc_op_free
 * The callback to "free" a composite object completely.
 */
typedef const struct zone_cache_ops {
	void         *(*zc_op_alloc)(zone_id_t, zalloc_flags_t);
	void         *(*zc_op_mark_valid)(zone_id_t, void *);
	void         *(*zc_op_mark_invalid)(zone_id_t, void *);
	void          (*zc_op_free)(zone_id_t, void *);
} *zone_cache_ops_t;

#if __has_ptrcheck
static inline char *__bidi_indexable
zcache_transpose_bounds(
	char *__bidi_indexable pointer_with_bounds,
	char *__unsafe_indexable unsafe_pointer)
{
	vm_offset_t offset_from_start = pointer_with_bounds - __ptr_lower_bound(pointer_with_bounds);
	vm_offset_t offset_to_end = __ptr_upper_bound(pointer_with_bounds) - pointer_with_bounds;
	vm_offset_t size = offset_from_start + offset_to_end;
	return __unsafe_forge_bidi_indexable(char *, unsafe_pointer - offset_from_start, size)
	       + offset_from_start;
}
#else
static inline char *__header_indexable
zcache_transpose_bounds(
	char *__header_indexable pointer_with_bounds __unused,
	char *__unsafe_indexable unsafe_pointer)
{
	return unsafe_pointer;
}
#endif // __has_ptrcheck

/*!
 * @function zcache_mark_valid()
 *
 * @brief
 * Mark an element as "valid".
 *
 * @description
 * This function is used to be able to integrate with KASAN or PGZ
 * for a cache of composite objects. It typically is a function
 * called in their @c zc_op_mark_valid() callback.
 *
 * If PGZ or KASAN isn't in use, then this callback is a no-op.
 * Otherwise the @c elem address might be updated.
 *
 * @param zone          the zone the element belongs to.
 * @param elem          the address of the element
 * @returns             the new address to correctly access @c elem.
 */
extern void *__unsafe_indexable zcache_mark_valid(
	zone_t                  zone,
	void                    *elem __unsafe_indexable);

static inline void *
zcache_mark_valid_single(
	zone_t                  zone,
	void                    *elem)
{
	return __unsafe_forge_single(void *, zcache_mark_valid(zone, elem));
}

static inline void *__header_bidi_indexable
zcache_mark_valid_indexable(
	zone_t                  zone,
	void                    *elem __header_bidi_indexable)
{
	return zcache_transpose_bounds((char *)elem, (char *)zcache_mark_valid(zone, elem));
}

/*!
 * @function zcache_mark_invalid()
 *
 * @brief
 * Mark an element as "invalid".
 *
 * @description
 * This function is used to be able to integrate with KASAN or PGZ
 * for a cache of composite objects. It typically is a function
 * called in their @c zc_op_mark_invalid() callback.
 *
 * This function performs validation that @c elem belongs
 * to the right zone and is properly "aligned", and should
 * never be elided under any configuration.
 *
 * @param zone          the zone the element belongs to.
 * @param elem          the address of the element
 * @returns             the new address to correctly access @c elem.
 */
extern void *__unsafe_indexable zcache_mark_invalid(
	zone_t                  zone,
	void                    *elem __unsafe_indexable);

static inline void *
zcache_mark_invalid_single(
	zone_t                  zone,
	void                    *elem)
{
	return __unsafe_forge_single(void *, zcache_mark_invalid(zone, elem));
}

static inline void *__header_bidi_indexable
zcache_mark_invalid_indexable(
	zone_t                  zone,
	void                    *elem __header_bidi_indexable)
{
	return zcache_transpose_bounds((char *)elem, (char *)zcache_mark_invalid(zone, elem));
}

/*!
 * @macro zcache_alloc()
 *
 * @abstract
 * Allocates a composite object from a cache.
 *
 * @param zone_id       The proper @c ZONE_ID_* constant.
 * @param flags         a collection of @c zalloc_flags_t.
 *
 * @returns             NULL or the allocated element
 */
#define zcache_alloc(zone_id, fl) \
	__zalloc_cast(zone_id, zcache_alloc_n(zone_id, 1, fl).z_head)

/*!
 * @function zcache_alloc_n()
 *
 * @abstract
 * Allocates a stack of composite objects from a cache.
 *
 * @param zone_id       The proper @c ZONE_ID_* constant.
 * @param count         how many elements to allocate (less might be returned)
 * @param flags         a set of @c zone_create_flags_t flags.
 *
 * @returns             NULL or the allocated composite object
 */
extern zstack_t zcache_alloc_n(
	zone_id_t               zone_id,
	uint32_t                count,
	zalloc_flags_t          flags,
	zone_cache_ops_t        ops);
#define zcache_alloc_n(zone_id, count, flags) \
	(zcache_alloc_n)(zone_id, count, flags, __zcache_##zone_id##_ops)



/*!
 * @function zcache_free()
 *
 * @abstract
 * Frees a composite object previously allocated
 * with @c zcache_alloc() or @c zcache_alloc_n().
 *
 * @param zone_id       the zcache id to free the object to.
 * @param addr          the address to free
 * @param ops           the pointer to the zcache ops for this zcache.
 */
extern void zcache_free(
	zone_id_t               zone_id,
	void                   *addr __unsafe_indexable,
	zone_cache_ops_t        ops);
#define zcache_free(zone_id, elem) \
	(zcache_free)(zone_id, (void *)os_ptr_load_and_erase(elem), \
	    __zcache_##zone_id##_ops)

/*!
 * @function zcache_free_n()
 *
 * @abstract
 * Frees a stack of composite objects previously allocated
 * with @c zcache_alloc() or @c zcache_alloc_n().
 *
 * @param zone_id       the zcache id to free the objects to.
 * @param stack         a stack of composite objects
 * @param ops           the pointer to the zcache ops for this zcache.
 */
extern void zcache_free_n(
	zone_id_t               zone_id,
	zstack_t                stack,
	zone_cache_ops_t        ops);
#define zcache_free_n(zone_id, stack) \
	(zcache_free_n)(zone_id, zstack_load_and_erase(&(stack)), \
	    __zcache_##zone_id##_ops)


/*!
 * @function zcache_drain()
 *
 * @abstract
 * Forces a zcache to be drained (have all its data structures freed
 * back to the original zones).
 *
 * @param zone_id       the zcache id to free the objects to.
 */
extern void zcache_drain(
	zone_id_t               zone_id);


/*!
 * @macro ZCACHE_DECLARE
 *
 * @abstract
 * Declares the type associated with a zone cache ID.
 *
 * @param id            the name of zone ID to associate a type with.
 * @param type_t        the type of elements in the zone.
 */
#define ZCACHE_DECLARE(id, type_t) \
	__ZONE_DECLARE_TYPE(id, type_t); \
	__attribute__((visibility("hidden"))) \
	extern const zone_cache_ops_t __zcache_##id##_ops


/*!
 * @macro ZCACHE_DEFINE
 *
 * @abstract
 * Defines a zone cache for a given ID and type.
 *
 * @param zone_id       the name of zone ID to associate a type with.
 * @param name          the name for the zone
 * @param type_t        the type of elements in the zone.
 * @param size          the size of elements in the cache
 * @param ops           the ops for this zcache.
 */
#define ZCACHE_DEFINE(zid, name, type_t, size, ops) \
	ZCACHE_DECLARE(zid, type_t);                                            \
	ZONE_DECLARE_ID(zid, type_t);                                           \
	const zone_cache_ops_t __zcache_##zid##_ops = (ops);                    \
	ZONE_INIT(NULL, name, size, ZC_OBJ_CACHE, zid, ^(zone_t z __unused) {   \
	        zcache_ops[zid] = (ops);                                        \
	})

extern zone_cache_ops_t zcache_ops[ZONE_ID__FIRST_DYNAMIC];

#pragma mark XNU only: PGZ support

/*!
 * @function pgz_owned()
 *
 * @brief
 * Returns whether an address is PGZ owned.
 *
 * @param addr          The address to translate.
 * @returns             Whether it is PGZ owned
 */
#if CONFIG_PROB_GZALLOC
extern bool pgz_owned(mach_vm_address_t addr) __pure2;
#else
#define pgz_owned(addr) false
#endif

/*!
 * @function pgz_decode()
 *
 * @brief
 * Translates a PGZ protected virtual address to its unprotected
 * backing store.
 *
 * @discussion
 * This is exposed so that the VM can lookup the vm_page_t for PGZ protected
 * elements since the PGZ protected virtual addresses are maintained by PGZ
 * at the pmap level without the VM involvment.
 *
 * "allow_invalid" schemes relying on sequestering also need this
 * to perform the locking attempts on the unprotected address.
 *
 * @param addr          The address to translate.
 * @param size          The object size.
 * @returns             The unprotected address or @c addr.
 */
#if CONFIG_PROB_GZALLOC
#define pgz_decode(addr, size) \
	((typeof(addr))__pgz_decode((mach_vm_address_t)(addr), size))
#else
#define pgz_decode(addr, size)  (addr)
#endif

/*!
 * @function pgz_decode_allow_invalid()
 *
 * @brief
 * Translates a PGZ protected virtual address to its unprotected
 * backing store, but doesn't assert it is still allocated/valid.
 *
 * @discussion
 * "allow_invalid" schemes relying on sequestering also need this
 * to perform the locking attempts on the unprotected address.
 *
 * @param addr          The address to translate.
 * @param want_zid      The expected zone ID for the element.
 * @returns             The unprotected address or @c addr.
 */
#if CONFIG_PROB_GZALLOC
#define pgz_decode_allow_invalid(addr, want_zid) \
	((typeof(addr))__pgz_decode_allow_invalid((vm_offset_t)(addr), want_zid))
#else
#define pgz_decode_allow_invalid(addr, zid)  (addr)
#endif

#pragma mark XNU only: misc & implementation details

struct zone_create_startup_spec {
	zone_t                 *z_var;
	const char             *z_name __unsafe_indexable;
	vm_size_t               z_size;
	zone_create_flags_t     z_flags;
	zone_id_t               z_zid;
	void                  (^z_setup)(zone_t);
};

extern void     zone_create_startup(
	struct zone_create_startup_spec *spec);

#define __ZONE_INIT1(ns, var, name, size, flags, zid, setup) \
	static __startup_data struct zone_create_startup_spec \
	__startup_zone_spec_ ## ns = { var, name, size, flags, zid, setup }; \
	STARTUP_ARG(ZALLOC, STARTUP_RANK_FOURTH, zone_create_startup, \
	    &__startup_zone_spec_ ## ns)

#define __ZONE_INIT(ns, var, name, size, flags, zid, setup) \
	__ZONE_INIT1(ns, var, name, size, flags, zid, setup) \

#define __zalloc_cast(namespace, expr) \
	((typeof(__zalloc__##namespace##__type_name))__unsafe_forge_single(void *, expr))

#if ZALLOC_TYPE_SAFE
#define zalloc(zov)             __zalloc_cast(zov, (zalloc)(zov))
#define zalloc_noblock(zov)     __zalloc_cast(zov, (zalloc_noblock)(zov))
#endif /* !ZALLOC_TYPE_SAFE */

struct zone_view_startup_spec {
	zone_view_t         zv_view;
	union {
		zone_kheap_id_t zv_heapid;
		zone_t         *zv_zone;
	};
	vm_size_t           zv_size;
};

extern void zone_view_startup_init(
	struct zone_view_startup_spec *spec);

extern void zone_userspace_reboot_checks(void);

#if VM_TAG_SIZECLASSES
extern void __zone_site_register(
	vm_allocation_site_t   *site);

#define VM_ALLOC_SITE_TAG() ({ \
	__PLACE_IN_SECTION("__DATA, __data")                                   \
	static vm_allocation_site_t site = { .refcount = 2, };                 \
	STARTUP_ARG(ZALLOC, STARTUP_RANK_MIDDLE, __zone_site_register, &site);   \
	site.tag;                                                              \
})
#else /* VM_TAG_SIZECLASSES */
#define VM_ALLOC_SITE_TAG()                     VM_KERN_MEMORY_NONE
#endif /* !VM_TAG_SIZECLASSES */

static inline zalloc_flags_t
__zone_flags_mix_tag(zalloc_flags_t flags, vm_tag_t tag)
{
	return (flags & Z_VM_TAG_MASK) ? flags : Z_VM_TAG(flags, (uint32_t)tag);
}

#if DEBUG || DEVELOPMENT
#  define ZPCPU_MANGLE_MASK     0xc0c0000000000000ul
#else /* !(DEBUG || DEVELOPMENT) */
#  define ZPCPU_MANGLE_MASK     0ul
#endif /* !(DEBUG || DEVELOPMENT) */

#define __zpcpu_mangle(ptr)     (__zpcpu_addr(ptr) & ~ZPCPU_MANGLE_MASK)
#define __zpcpu_demangle(ptr)   (__zpcpu_addr(ptr) | ZPCPU_MANGLE_MASK)
#define __zpcpu_addr(e)         ((vm_address_t)(e))
#define __zpcpu_cast(ptr, e)    __unsafe_forge_single(typeof(ptr), e)
#define __zpcpu_next(ptr)       __zpcpu_cast(ptr, __zpcpu_addr(ptr) + PAGE_SIZE)

/**
 * @macro __zpcpu_mangle_for_boot()
 *
 * @discussion
 * Per-cpu variables allocated in zones (as opposed to percpu globals) that need
 * to function early during boot (before @c STARTUP_SUB_ZALLOC) might use static
 * storage marked @c __startup_data and replace it with the proper allocation
 * at the end of the @c STARTUP_SUB_ZALLOC phase (@c STARTUP_RANK_LAST).
 *
 * However, some devices boot from a cpu where @c cpu_number() != 0. This macro
 * provides the proper mangling of the storage into a "fake" percpu pointer so
 * that accesses through @c zpercpu_get() functions properly.
 *
 * This is invalid to use after the @c STARTUP_SUB_ZALLOC phase has completed.
 */
#define __zpcpu_mangle_for_boot(ptr)  ({ \
	assert(startup_phase < STARTUP_SUB_ZALLOC); \
	__zpcpu_cast(ptr, __zpcpu_mangle(__zpcpu_addr(ptr) - ptoa(cpu_number()))); \
})

extern unsigned zpercpu_count(void) __pure2;

#if CONFIG_PROB_GZALLOC

extern vm_offset_t __pgz_decode(
	mach_vm_address_t       addr,
	mach_vm_size_t          size);

extern vm_offset_t __pgz_decode_allow_invalid(
	vm_offset_t             offs,
	zone_id_t               zid);

#endif
#if DEBUG || DEVELOPMENT
/* zone_max_zone is here (but not zalloc_internal.h) for the BSD kernel */
extern unsigned int zone_max_zones(void);

extern size_t zone_pages_wired;
extern size_t zone_guard_pages;
#endif /* DEBUG || DEVELOPMENT */
#if CONFIG_ZLEAKS
extern uint32_t                 zleak_active;
extern vm_size_t                zleak_max_zonemap_size;
extern vm_size_t                zleak_per_zone_tracking_threshold;

extern kern_return_t zleak_update_threshold(
	vm_size_t              *arg,
	uint64_t                value);
#endif /* CONFIG_ZLEAKS */

extern uint32_t                 zone_map_jetsam_limit;

extern kern_return_t zone_map_jetsam_set_limit(uint32_t value);

extern zone_t percpu_u64_zone;

/*!
 * @function mach_memory_info_sample
 *
 * @abstract
 * Helper function for mach_memory_info() (MACH) and memorystatus_collect_jetsam_snapshot_zprint() (BSD)
 * to collect wired memory information.
 *
 * @param names array with `*zonesCnt` elements.
 * @param info array with `*zonesCnt` elements.
 * @param coalesce array with `*zonesCnt` elements, must be set if `redact_info` is true.
 * @param zonesCnt set to the allocated count of the above, and on return will be the actual count.
 * @param memoryInfo optional, if set must have at least `vm_page_diagnose_estimate()` elements.
 * @param memoryInfoCnt optional, if set must be the count of memoryInfo, otherwise if set to 0 then on return will be `vm_page_diagnose_estimate()`.
 * @param redact_info if true sensitive information about zone allocations will be removed.
 */
extern kern_return_t
mach_memory_info_sample(
	mach_zone_name_t *names,
	mach_zone_info_t *info,
	int              *coalesce,
	unsigned int     *zonesCnt,
	mach_memory_info_t *memoryInfo,
	unsigned int       memoryInfoCnt,
	bool               redact_info);

extern void     zone_gc_trim(void);
extern void     zone_gc_drain(void);

#pragma GCC visibility pop
#endif /* XNU_KERNEL_PRIVATE */

/*
 * This macro is currently used by AppleImage4 (rdar://83924635)
 */
#define __zalloc_ptr_load_and_erase(elem) \
	os_ptr_load_and_erase(elem)

__ASSUME_PTR_ABI_SINGLE_END __END_DECLS

#endif  /* _KERN_ZALLOC_H_ */

#endif  /* KERNEL_PRIVATE */
