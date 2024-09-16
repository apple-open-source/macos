/*
 * Copyright (c) 2016-2023 Apple Inc. All rights reserved.
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

#include <skywalk/os_skywalk_private.h>
#define _FN_KPRINTF
#include <pexpert/pexpert.h>    /* for PE_parse_boot_argn */
#include <libkern/OSDebug.h>    /* for OSBacktrace */
#include <kern/sched_prim.h>    /* for assert_wait */
#include <vm/vm_memtag.h>

/*
 * Memory allocator with per-CPU caching (magazines), derived from the kmem
 * magazine concept and implementation as described in the following paper:
 * http://www.usenix.org/events/usenix01/full_papers/bonwick/bonwick.pdf
 *
 * That implementation is Copyright 2006 Sun Microsystems, Inc.  All rights
 * reserved.  Use is subject to license terms.
 *
 * This derivative differs from the original kmem slab allocator, in that:
 *
 *   a) There is always a discrete bufctl per object, even for small sizes.
 *      This increases the overhead, but is necessary as Skywalk objects
 *      coming from the slab may be shared (RO or RW) with userland; therefore
 *      embedding the KVA pointer linkage in freed objects is a non-starter.
 *
 *   b) Writing patterns to the slab at slab creation or destruction time
 *      (when debugging is enabled) is not implemented, as the object may
 *      be shared (RW) with userland and thus we cannot panic upon pattern
 *      mismatch episodes.  This can be relaxed so that we conditionally
 *      verify the pattern for kernel-only memory.
 *
 * This derivative also differs from Darwin's mcache allocator (which itself
 * is a derivative of the original kmem slab allocator), in that:
 *
 *   1) The slab layer is internal to skmem_cache, unlike mcache's external
 *      slab layer required to support mbufs.  skmem_cache also supports
 *      constructing and deconstructing objects, while mcache does not.
 *      This brings skmem_cache's model closer to that of the original
 *      kmem slab allocator.
 *
 *   2) mcache allows for batch allocation and free by way of chaining the
 *      objects together using a linked list.  This requires using a part
 *      of the object to act as the linkage, which is against Skywalk's
 *      requirements of not exposing any KVA pointer to userland.  Although
 *      this is supported by skmem_cache, chaining is only possible if the
 *      region is not mapped to userland.  That implies that kernel-only
 *      objects can be chained provided the cache is created with batching
 *      mode enabled, and that the object is large enough to contain the
 *      skmem_obj structure.
 *
 * In other words, skmem_cache is a hybrid of a hybrid custom allocator that
 * implements features that are required by Skywalk.  In addition to being
 * aware of userland access on the buffers, in also supports mirrored backend
 * memory regions.  This allows a cache to manage two independent memory
 * regions, such that allocating/freeing an object from/to one results in
 * allocating/freeing a shadow object in another, thus guaranteeing that both
 * objects share the same lifetime.
 */

static uint32_t ncpu;                   /* total # of initialized CPUs */

static LCK_MTX_DECLARE_ATTR(skmem_cache_lock, &skmem_lock_grp, &skmem_lock_attr);
static struct thread *skmem_lock_owner = THREAD_NULL;

static LCK_GRP_DECLARE(skmem_sl_lock_grp, "skmem_slab");
static LCK_GRP_DECLARE(skmem_dp_lock_grp, "skmem_depot");
static LCK_GRP_DECLARE(skmem_cpu_lock_grp, "skmem_cpu_cache");

#define SKMEM_CACHE_LOCK() do {                 \
	lck_mtx_lock(&skmem_cache_lock);        \
	skmem_lock_owner = current_thread();    \
} while (0)
#define SKMEM_CACHE_UNLOCK() do {               \
	skmem_lock_owner = THREAD_NULL;         \
	lck_mtx_unlock(&skmem_cache_lock);      \
} while (0)
#define SKMEM_CACHE_LOCK_ASSERT_HELD()          \
	LCK_MTX_ASSERT(&skmem_cache_lock, LCK_MTX_ASSERT_OWNED)
#define SKMEM_CACHE_LOCK_ASSERT_NOTHELD()       \
	LCK_MTX_ASSERT(&skmem_cache_lock, LCK_MTX_ASSERT_NOTOWNED)

#define SKM_DEPOT_LOCK(_skm)                    \
	lck_mtx_lock(&(_skm)->skm_dp_lock)
#define SKM_DEPOT_LOCK_SPIN(_skm)               \
	lck_mtx_lock_spin(&(_skm)->skm_dp_lock)
#define SKM_DEPOT_CONVERT_LOCK(_skm)            \
	lck_mtx_convert_spin(&(_skm)->skm_dp_lock)
#define SKM_DEPOT_LOCK_TRY(_skm)                \
	lck_mtx_try_lock(&(_skm)->skm_dp_lock)
#define SKM_DEPOT_LOCK_ASSERT_HELD(_skm)        \
	LCK_MTX_ASSERT(&(_skm)->skm_dp_lock, LCK_MTX_ASSERT_OWNED)
#define SKM_DEPOT_LOCK_ASSERT_NOTHELD(_skm)     \
	LCK_MTX_ASSERT(&(_skm)->skm_dp_lock, LCK_MTX_ASSERT_NOTOWNED)
#define SKM_DEPOT_UNLOCK(_skm)                  \
	lck_mtx_unlock(&(_skm)->skm_dp_lock)

#define SKM_RESIZE_LOCK(_skm)                   \
	lck_mtx_lock(&(_skm)->skm_rs_lock)
#define SKM_RESIZE_LOCK_ASSERT_HELD(_skm)       \
	LCK_MTX_ASSERT(&(_skm)->skm_rs_lock, LCK_MTX_ASSERT_OWNED)
#define SKM_RESIZE_LOCK_ASSERT_NOTHELD(_skm)    \
	LCK_MTX_ASSERT(&(_skm)->skm_rs_lock, LCK_MTX_ASSERT_NOTOWNED)
#define SKM_RESIZE_UNLOCK(_skm)                 \
	lck_mtx_unlock(&(_skm)->skm_rs_lock)

#define SKM_CPU_LOCK(_cp)                       \
	lck_mtx_lock(&(_cp)->cp_lock)
#define SKM_CPU_LOCK_SPIN(_cp)                  \
	lck_mtx_lock_spin(&(_cp)->cp_lock)
#define SKM_CPU_CONVERT_LOCK(_cp)               \
	lck_mtx_convert_spin(&(_cp)->cp_lock)
#define SKM_CPU_LOCK_ASSERT_HELD(_cp)           \
	LCK_MTX_ASSERT(&(_cp)->cp_lock, LCK_MTX_ASSERT_OWNED)
#define SKM_CPU_LOCK_ASSERT_NOTHELD(_cp)        \
	LCK_MTX_ASSERT(&(_cp)->cp_lock, LCK_MTX_ASSERT_NOTOWNED)
#define SKM_CPU_UNLOCK(_cp)                     \
	lck_mtx_unlock(&(_cp)->cp_lock)

#define SKM_ZONE_MAX    256

static struct zone *skm_zone;                   /* zone for skmem_cache */
/*
 * XXX -fbounds-safety: Took out ZC_DESTRUCTIBLE flag because of static assert
 * in ZONE_DEFINE_TYPE
 */
ZONE_DECLARE(skm_zone, struct zone *);

struct skmem_cache *skmem_slab_cache;    /* cache for skmem_slab */
struct skmem_cache *skmem_bufctl_cache;  /* cache for skmem_bufctl */

unsigned int bc_size;                    /* size of bufctl */

/*
 * XXX: -fbounds-safety: we added objsize to skmem_cache_batch_alloc(), but this
 * is only used by -fbounds-safety, so we use __unused if -fbounds-safety is
 * disabled. The utility macro for that is SK_BF_ARG()
 */
#if !__has_ptrcheck
#define SK_FB_ARG __unused
#else
#define SK_FB_ARG
#endif

/*
 * Magazine types (one per row.)
 *
 * The first column defines the number of objects that the magazine can hold.
 * Using that number, we derive the effective number: the aggregate count of
 * object pointers, plus 2 pointers (skmem_mag linkage + magazine type).
 * This would result in an object size that is aligned on the CPU cache
 * size boundary; the exception to this is the KASAN mode where the size
 * would be larger due to the redzone regions.
 *
 * The second column defines the alignment of the magazine.  Because each
 * magazine is used at the CPU-layer cache, we need to ensure there is no
 * false sharing across the CPUs, and align the magazines to the maximum
 * cache alignment size, for simplicity.  The value of 0 may be used to
 * indicate natural pointer size alignment.
 *
 * The third column defines the starting magazine type for a given cache,
 * determined at the cache's creation time based on its chunk size.
 *
 * The fourth column defines the magazine type limit for a given cache.
 * Magazine resizing will only occur if the chunk size is less than this.
 */
static struct skmem_magtype skmem_magtype[] = {
#if defined(__LP64__)
	{ .mt_magsize = 14, .mt_align = 0, .mt_minbuf = 128, .mt_maxbuf = 512,
	  .mt_cache = NULL, .mt_cname = "" },
	{ .mt_magsize = 30, .mt_align = 0, .mt_minbuf = 96, .mt_maxbuf = 256,
	  .mt_cache = NULL, .mt_cname = "" },
	{ .mt_magsize = 46, .mt_align = 0, .mt_minbuf = 64, .mt_maxbuf = 128,
	  .mt_cache = NULL, .mt_cname = "" },
	{ .mt_magsize = 62, .mt_align = 0, .mt_minbuf = 32, .mt_maxbuf = 64,
	  .mt_cache = NULL, .mt_cname = "" },
	{ .mt_magsize = 94, .mt_align = 0, .mt_minbuf = 16, .mt_maxbuf = 32,
	  .mt_cache = NULL, .mt_cname = "" },
	{ .mt_magsize = 126, .mt_align = 0, .mt_minbuf = 8, .mt_maxbuf = 16,
	  .mt_cache = NULL, .mt_cname = "" },
	{ .mt_magsize = 142, .mt_align = 0, .mt_minbuf = 0, .mt_maxbuf = 8,
	  .mt_cache = NULL, .mt_cname = "" },
	{ .mt_magsize = 158, .mt_align = 0, .mt_minbuf = 0, .mt_maxbuf = 0,
	  .mt_cache = NULL, .mt_cname = "" },
#else /* !__LP64__ */
	{ .mt_magsize = 14, .mt_align = 0, .mt_minbuf = 0, .mt_maxbuf = 0,
	  .mt_cache = NULL, .mt_cname = "" },
#endif /* !__LP64__ */
};

/*
 * Hash table bounds.  Start with the initial value, and rescale up to
 * the specified limit.  Ideally we don't need a limit, but in practice
 * this helps guard against runaways.  These values should be revisited
 * in future and be adjusted as needed.
 */
#define SKMEM_CACHE_HASH_INITIAL        64      /* initial hash table size */
#define SKMEM_CACHE_HASH_LIMIT          8192    /* hash table size limit */

/*
 * The last magazine type.
 */
static struct skmem_magtype *skmem_cache_magsize_last;

static TAILQ_HEAD(, skmem_cache) skmem_cache_head;
static boolean_t skmem_cache_ready;
static int skmem_magazine_ctor(struct skmem_obj_info *,
    struct skmem_obj_info *, void *, uint32_t);
static void skmem_magazine_destroy(struct skmem_cache *, struct skmem_mag *,
    int);
static uint32_t skmem_depot_batch_alloc(struct skmem_cache *,
    struct skmem_maglist *, uint32_t *, struct skmem_mag *__bidi_indexable *, uint32_t);
static void skmem_depot_batch_free(struct skmem_cache *, struct skmem_maglist *,
    uint32_t *, struct skmem_mag *);
static void skmem_depot_ws_update(struct skmem_cache *);
static void skmem_depot_ws_zero(struct skmem_cache *);
static void skmem_depot_ws_reap(struct skmem_cache *);
#define SKMEM_CACHE_FREE_NOCACHE    0x1
static void skmem_cache_batch_free_common(struct skmem_cache *, struct skmem_obj *, uint32_t);
static void skmem_cache_magazine_purge(struct skmem_cache *);
static void skmem_cache_magazine_enable(struct skmem_cache *, uint32_t);
static void skmem_cache_magazine_resize(struct skmem_cache *);
static void skmem_cache_hash_rescale(struct skmem_cache *);
static void skmem_cpu_reload(struct skmem_cpu_cache *, struct skmem_mag *, int);
static void skmem_cpu_batch_reload(struct skmem_cpu_cache *,
    struct skmem_mag *, int);
static void skmem_cache_applyall(void (*)(struct skmem_cache *, uint32_t),
    uint32_t);
static void skmem_cache_reclaim(struct skmem_cache *, uint32_t);
static void skmem_cache_reap_start(void);
static void skmem_cache_reap_done(void);
static void skmem_cache_reap_func(thread_call_param_t, thread_call_param_t);
static void skmem_cache_update_func(thread_call_param_t, thread_call_param_t);
static int skmem_cache_resize_enter(struct skmem_cache *, boolean_t);
static void skmem_cache_resize_exit(struct skmem_cache *);
static void skmem_audit_buf(struct skmem_cache *, struct skmem_obj *);
static int skmem_cache_mib_get_sysctl SYSCTL_HANDLER_ARGS;

SYSCTL_PROC(_kern_skywalk_stats, OID_AUTO, cache,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, skmem_cache_mib_get_sysctl, "S,sk_stats_cache",
    "Skywalk cache statistics");

static volatile uint32_t skmem_cache_reaping;
static thread_call_t skmem_cache_reap_tc;
static thread_call_t skmem_cache_update_tc;

extern kern_return_t thread_terminate(thread_t);
extern unsigned int ml_wait_max_cpus(void);

#define SKMEM_DEBUG_NOMAGAZINES 0x1     /* disable magazines layer */
#define SKMEM_DEBUG_AUDIT       0x2     /* audit transactions */
#define SKMEM_DEBUG_MASK        (SKMEM_DEBUG_NOMAGAZINES|SKMEM_DEBUG_AUDIT)

#if DEBUG
static uint32_t skmem_debug = SKMEM_DEBUG_AUDIT;
#else /* !DEBUG */
static uint32_t skmem_debug = 0;
#endif /* !DEBUG */

static uint32_t skmem_clear_min = 0;    /* clear on free threshold */

#define SKMEM_CACHE_UPDATE_INTERVAL     11      /* 11 seconds */
static uint32_t skmem_cache_update_interval = SKMEM_CACHE_UPDATE_INTERVAL;

#define SKMEM_DEPOT_CONTENTION  3       /* max failed trylock per interval */
static int skmem_cache_depot_contention = SKMEM_DEPOT_CONTENTION;

#if (DEVELOPMENT || DEBUG)
SYSCTL_UINT(_kern_skywalk_mem, OID_AUTO, cache_update_interval,
    CTLFLAG_RW | CTLFLAG_LOCKED, &skmem_cache_update_interval,
    SKMEM_CACHE_UPDATE_INTERVAL, "Cache update interval");
SYSCTL_INT(_kern_skywalk_mem, OID_AUTO, cache_depot_contention,
    CTLFLAG_RW | CTLFLAG_LOCKED, &skmem_cache_depot_contention,
    SKMEM_DEPOT_CONTENTION, "Depot contention");

static uint32_t skmem_cache_update_interval_saved = SKMEM_CACHE_UPDATE_INTERVAL;

/*
 * Called by skmem_test_start() to set the update interval.
 */
void
skmem_cache_test_start(uint32_t i)
{
	skmem_cache_update_interval_saved = skmem_cache_update_interval;
	skmem_cache_update_interval = i;
}

/*
 * Called by skmem_test_stop() to restore the update interval.
 */
void
skmem_cache_test_stop(void)
{
	skmem_cache_update_interval = skmem_cache_update_interval_saved;
}
#endif /* (DEVELOPMENT || DEBUG) */

#define SKMEM_TAG_BUFCTL_HASH   "com.apple.skywalk.bufctl.hash"
static SKMEM_TAG_DEFINE(skmem_tag_bufctl_hash, SKMEM_TAG_BUFCTL_HASH);

#define SKMEM_TAG_CACHE_MIB     "com.apple.skywalk.cache.mib"
static SKMEM_TAG_DEFINE(skmem_tag_cache_mib, SKMEM_TAG_CACHE_MIB);

static int __skmem_cache_pre_inited = 0;
static int __skmem_cache_inited = 0;

/*
 * Called before skmem_region_init().
 */
void
skmem_cache_pre_init(void)
{
	vm_size_t skm_size;

	ASSERT(!__skmem_cache_pre_inited);

	ncpu = ml_wait_max_cpus();

	/* allocate extra in case we need to manually align the pointer */
	if (skm_zone == NULL) {
		skm_size = SKMEM_CACHE_SIZE(ncpu);
#if KASAN
		/*
		 * When KASAN is enabled, the zone allocator adjusts the
		 * element size to include the redzone regions, in which
		 * case we assume that the elements won't start on the
		 * alignment boundary and thus need to do some fix-ups.
		 * These include increasing the effective object size
		 * which adds at least 136 bytes to the original size,
		 * as computed by skmem_region_params_config() above.
		 */
		skm_size += (sizeof(void *) + CHANNEL_CACHE_ALIGN_MAX);
#endif /* KASAN */
		skm_size = P2ROUNDUP(skm_size, CHANNEL_CACHE_ALIGN_MAX);
		skm_zone = zone_create(SKMEM_ZONE_PREFIX ".skm", skm_size,
		    ZC_PGZ_USE_GUARDS | ZC_ZFREE_CLEARMEM | ZC_DESTRUCTIBLE);
	}

	TAILQ_INIT(&skmem_cache_head);

	__skmem_cache_pre_inited = 1;
}

/*
 * Called after skmem_region_init().
 */
void
skmem_cache_init(void)
{
	uint32_t cpu_cache_line_size = skmem_cpu_cache_line_size();
	struct skmem_magtype *mtp;
	uint32_t i;

	_CASSERT(SKMEM_CACHE_HASH_LIMIT >= SKMEM_CACHE_HASH_INITIAL);

	_CASSERT(SKM_MODE_NOMAGAZINES == SCA_MODE_NOMAGAZINES);
	_CASSERT(SKM_MODE_AUDIT == SCA_MODE_AUDIT);
	_CASSERT(SKM_MODE_NOREDIRECT == SCA_MODE_NOREDIRECT);
	_CASSERT(SKM_MODE_BATCH == SCA_MODE_BATCH);
	_CASSERT(SKM_MODE_DYNAMIC == SCA_MODE_DYNAMIC);
	_CASSERT(SKM_MODE_CLEARONFREE == SCA_MODE_CLEARONFREE);
	_CASSERT(SKM_MODE_PSEUDO == SCA_MODE_PSEUDO);

	ASSERT(__skmem_cache_pre_inited);
	ASSERT(!__skmem_cache_inited);

	_CASSERT(offsetof(struct skmem_bufctl, bc_addr) == offsetof(struct skmem_bufctl_audit, bc_addr));
	_CASSERT(offsetof(struct skmem_bufctl, bc_addrm) == offsetof(struct skmem_bufctl_audit, bc_addrm));
	_CASSERT(offsetof(struct skmem_bufctl, bc_slab) == offsetof(struct skmem_bufctl_audit, bc_slab));
	_CASSERT(offsetof(struct skmem_bufctl, bc_lim) == offsetof(struct skmem_bufctl_audit, bc_lim));
	_CASSERT(offsetof(struct skmem_bufctl, bc_flags) == offsetof(struct skmem_bufctl_audit, bc_flags));
	_CASSERT(offsetof(struct skmem_bufctl, bc_idx) == offsetof(struct skmem_bufctl_audit, bc_idx));
	_CASSERT(offsetof(struct skmem_bufctl, bc_usecnt) == offsetof(struct skmem_bufctl_audit, bc_usecnt));
	_CASSERT(sizeof(struct skmem_bufctl) == offsetof(struct skmem_bufctl_audit, bc_thread));

	PE_parse_boot_argn("skmem_debug", &skmem_debug, sizeof(skmem_debug));
	skmem_debug &= SKMEM_DEBUG_MASK;

#if (DEVELOPMENT || DEBUG)
	PE_parse_boot_argn("skmem_clear_min", &skmem_clear_min,
	    sizeof(skmem_clear_min));
#endif /* (DEVELOPMENT || DEBUG) */
	if (skmem_clear_min == 0) {
		/* zeroing 2 CPU cache lines practically comes for free */
		skmem_clear_min = 2 * cpu_cache_line_size;
	} else {
		/* round it up to CPU cache line size */
		skmem_clear_min = (uint32_t)P2ROUNDUP(skmem_clear_min,
		    cpu_cache_line_size);
	}

	/* create a cache for buffer control structures */
	if (skmem_debug & SKMEM_DEBUG_AUDIT) {
		bc_size = sizeof(struct skmem_bufctl_audit);
		skmem_bufctl_cache = skmem_cache_create("bufctl.audit",
		    bc_size, sizeof(uint64_t), NULL, NULL,
		    NULL, NULL, NULL, 0);
	} else {
		bc_size = sizeof(struct skmem_bufctl);
		skmem_bufctl_cache = skmem_cache_create("bufctl",
		    bc_size, sizeof(uint64_t), NULL, NULL,
		    NULL, NULL, NULL, 0);
	}

	/* create a cache for slab structures */
	skmem_slab_cache = skmem_cache_create("slab",
	    sizeof(struct skmem_slab), sizeof(uint64_t), NULL, NULL, NULL,
	    NULL, NULL, 0);

	/*
	 * Go thru the magazine type table and create a cache for each.
	 */
	for (i = 0; i < sizeof(skmem_magtype) / sizeof(*mtp); i++) {
		const char *__null_terminated mt_cname = NULL;
		mtp = &skmem_magtype[i];

		if (mtp->mt_align != 0 &&
		    ((mtp->mt_align & (mtp->mt_align - 1)) != 0 ||
		    mtp->mt_align < (int)cpu_cache_line_size)) {
			panic("%s: bad alignment %d", __func__, mtp->mt_align);
			/* NOTREACHED */
			__builtin_unreachable();
		}
		mt_cname = tsnprintf(mtp->mt_cname, sizeof(mtp->mt_cname),
		    "mg.%d", mtp->mt_magsize);

		/* create a cache for this magazine type */
		mtp->mt_cache = skmem_cache_create(mt_cname,
		    SKMEM_MAG_SIZE(mtp->mt_magsize), mtp->mt_align,
		    skmem_magazine_ctor, NULL, NULL, mtp, NULL, 0);

		/* remember the last magazine type */
		skmem_cache_magsize_last = mtp;
	}

	VERIFY(skmem_cache_magsize_last != NULL);
	VERIFY(skmem_cache_magsize_last->mt_minbuf == 0);
	VERIFY(skmem_cache_magsize_last->mt_maxbuf == 0);

	/*
	 * Allocate thread calls for cache reap and update operations.
	 */
	skmem_cache_reap_tc =
	    thread_call_allocate_with_options(skmem_cache_reap_func,
	    NULL, THREAD_CALL_PRIORITY_KERNEL, THREAD_CALL_OPTIONS_ONCE);
	skmem_cache_update_tc =
	    thread_call_allocate_with_options(skmem_cache_update_func,
	    NULL, THREAD_CALL_PRIORITY_KERNEL, THREAD_CALL_OPTIONS_ONCE);
	if (skmem_cache_reap_tc == NULL || skmem_cache_update_tc == NULL) {
		panic("%s: thread_call_allocate failed", __func__);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	/*
	 * We're ready; go through existing skmem_cache entries
	 * (if any) and enable the magazines layer for each.
	 */
	skmem_cache_applyall(skmem_cache_magazine_enable, 0);
	skmem_cache_ready = TRUE;

	/* and start the periodic cache update machinery */
	skmem_dispatch(skmem_cache_update_tc, NULL,
	    (skmem_cache_update_interval * NSEC_PER_SEC));

	__skmem_cache_inited = 1;
}

void
skmem_cache_fini(void)
{
	struct skmem_magtype *mtp;
	uint32_t i;

	if (__skmem_cache_inited) {
		ASSERT(TAILQ_EMPTY(&skmem_cache_head));

		for (i = 0; i < sizeof(skmem_magtype) / sizeof(*mtp); i++) {
			mtp = &skmem_magtype[i];
			skmem_cache_destroy(mtp->mt_cache);
			mtp->mt_cache = NULL;
		}
		skmem_cache_destroy(skmem_slab_cache);
		skmem_slab_cache = NULL;
		skmem_cache_destroy(skmem_bufctl_cache);
		skmem_bufctl_cache = NULL;

		if (skmem_cache_reap_tc != NULL) {
			(void) thread_call_cancel_wait(skmem_cache_reap_tc);
			(void) thread_call_free(skmem_cache_reap_tc);
			skmem_cache_reap_tc = NULL;
		}
		if (skmem_cache_update_tc != NULL) {
			(void) thread_call_cancel_wait(skmem_cache_update_tc);
			(void) thread_call_free(skmem_cache_update_tc);
			skmem_cache_update_tc = NULL;
		}

		__skmem_cache_inited = 0;
	}

	if (__skmem_cache_pre_inited) {
		if (skm_zone != NULL) {
			zdestroy(skm_zone);
			skm_zone = NULL;
		}

		__skmem_cache_pre_inited = 0;
	}
}

/*
 * Create a cache.
 */
struct skmem_cache *
skmem_cache_create(const char *name, size_t bufsize, size_t bufalign,
    skmem_ctor_fn_t ctor, skmem_dtor_fn_t dtor, skmem_reclaim_fn_t reclaim,
    void *private, struct skmem_region *region, uint32_t cflags)
{
	boolean_t pseudo = (region == NULL);
	struct skmem_magtype *mtp;
	struct skmem_cache *__single skm;
#if KASAN
	void *buf;
	size_t skm_align_off;
#endif
	size_t segsize;
	size_t chunksize;
	size_t objsize;
	size_t objalign;
	uint32_t i, cpuid;

	/* enforce 64-bit minimum alignment for buffers */
	if (bufalign == 0) {
		bufalign = SKMEM_CACHE_ALIGN;
	}
	bufalign = P2ROUNDUP(bufalign, SKMEM_CACHE_ALIGN);

	/* enforce alignment to be a power of 2 */
	VERIFY(powerof2(bufalign));

	if (region == NULL) {
		struct skmem_region_params srp = {};

		/* batching is currently not supported on pseudo regions */
		VERIFY(!(cflags & SKMEM_CR_BATCH));

		srp = *skmem_get_default(SKMEM_REGION_INTRINSIC);
		ASSERT(srp.srp_cflags == SKMEM_REGION_CR_PSEUDO);

		/* objalign is always equal to bufalign */
		srp.srp_align = objalign = bufalign;
		srp.srp_r_obj_cnt = 1;
		srp.srp_r_obj_size = (uint32_t)bufsize;
		skmem_region_params_config(&srp);

		/* allocate region for intrinsics */
		region = skmem_region_create(name, &srp, NULL, NULL, NULL);
		VERIFY(region->skr_c_obj_size >= P2ROUNDUP(bufsize, bufalign));
		VERIFY(objalign == region->skr_align);
#if KASAN
		/*
		 * When KASAN is enabled, the zone allocator adjusts the
		 * element size to include the redzone regions, in which
		 * case we assume that the elements won't start on the
		 * alignment boundary and thus need to do some fix-ups.
		 * These include increasing the effective object size
		 * which adds at least 16 bytes to the original size,
		 * as computed by skmem_region_params_config() above.
		 */
		VERIFY(region->skr_c_obj_size >=
		    (bufsize + sizeof(uint64_t) + bufalign));
#endif /* KASAN */
		/* enable magazine resizing by default */
		cflags |= SKMEM_CR_DYNAMIC;

		/*
		 * For consistency with ZC_ZFREE_CLEARMEM on skr->zreg,
		 * even though it's a no-op since the work is done
		 * at the zone layer instead.
		 */
		cflags |= SKMEM_CR_CLEARONFREE;
	} else {
		objalign = region->skr_align;
	}

	ASSERT(region != NULL);
	ASSERT(!(region->skr_mode & SKR_MODE_MIRRORED));
	segsize = region->skr_seg_size;
	ASSERT(bufalign <= segsize);

#if KASAN
	buf = zalloc_flags_buf(skm_zone, Z_WAITOK | Z_ZERO);
	/*
	 * We need to align `buf` such that offsetof(struct skmem_cache, skm_align)
	 * is aligned to a cache line boundary. In KASAN builds, allocations are
	 * preceded by metadata that changes the alignment of the object. The
	 * extra required size is accounted for at the time skm_zone is created.
	 * We then save the actual start of the allocation to skm_start, as it's
	 * the address we need to actually free.
	 */
	skm_align_off = offsetof(struct skmem_cache, skm_align);
	uintptr_t diff = P2ROUNDUP((intptr_t)buf + skm_align_off,
	    CHANNEL_CACHE_ALIGN_MAX) - (intptr_t)buf;
	skm = (void *)((char *)buf + diff);
	skm->skm_start = buf;
#else /* !KASAN */
	/*
	 * We expect that the zone allocator would allocate elements
	 * rounded up to the requested alignment based on the object
	 * size computed in skmem_cache_pre_init() earlier, and
	 * 'skm' is therefore the element address itself.
	 */
	skm = zalloc_flags_buf(skm_zone, Z_WAITOK | Z_ZERO);
#endif /* !KASAN */
	skm->skm_cpu_cache_count = ncpu;

	VERIFY(IS_P2ALIGNED(skm, CHANNEL_CACHE_ALIGN_MAX));

	if ((skmem_debug & SKMEM_DEBUG_NOMAGAZINES) ||
	    (cflags & SKMEM_CR_NOMAGAZINES)) {
		/*
		 * Either the caller insists that this cache should not
		 * utilize magazines layer, or that the system override
		 * to disable magazines layer on all caches has been set.
		 */
		skm->skm_mode |= SKM_MODE_NOMAGAZINES;
	} else {
		/*
		 * Region must be configured with enough objects
		 * to take into account objects at the CPU layer.
		 */
		ASSERT(!(region->skr_mode & SKR_MODE_NOMAGAZINES));
	}

	if (cflags & SKMEM_CR_DYNAMIC) {
		/*
		 * Enable per-CPU cache magazine resizing.
		 */
		skm->skm_mode |= SKM_MODE_DYNAMIC;
	}

	/* region stays around after defunct? */
	if (region->skr_mode & SKR_MODE_NOREDIRECT) {
		skm->skm_mode |= SKM_MODE_NOREDIRECT;
	}

	if (cflags & SKMEM_CR_BATCH) {
		/*
		 * Batch alloc/free involves storing the next object
		 * pointer at the beginning of each object; this is
		 * okay for kernel-only regions, but not those that
		 * are mappable to user space (we can't leak kernel
		 * addresses).
		 */
		_CASSERT(offsetof(struct skmem_obj, mo_next) == 0);
		VERIFY(!(region->skr_mode & SKR_MODE_MMAPOK));

		/* batching is currently not supported on pseudo regions */
		VERIFY(!(region->skr_mode & SKR_MODE_PSEUDO));

		/* validate object size */
		VERIFY(region->skr_c_obj_size >= sizeof(struct skmem_obj));

		skm->skm_mode |= SKM_MODE_BATCH;
	}

	uuid_generate_random(skm->skm_uuid);
	(void) snprintf(skm->skm_name, sizeof(skm->skm_name),
	    "%s.%s", SKMEM_CACHE_PREFIX, name);
	skm->skm_bufsize = bufsize;
	skm->skm_bufalign = bufalign;
	skm->skm_objalign = objalign;
	skm->skm_ctor = ctor;
	skm->skm_dtor = dtor;
	skm->skm_reclaim = reclaim;
	skm->skm_private = private;
	skm->skm_slabsize = segsize;

	skm->skm_region = region;
	/* callee holds reference */
	skmem_region_slab_config(region, skm, true);
	objsize = region->skr_c_obj_size;
	skm->skm_objsize = objsize;

	if (pseudo) {
		/*
		 * Release reference from skmem_region_create()
		 * since skm->skm_region holds one now.
		 */
		ASSERT(region->skr_mode & SKR_MODE_PSEUDO);
		skmem_region_release(region);

		skm->skm_mode |= SKM_MODE_PSEUDO;

		skm->skm_slab_alloc = skmem_slab_alloc_pseudo_locked;
		skm->skm_slab_free = skmem_slab_free_pseudo_locked;
	} else {
		skm->skm_slab_alloc = skmem_slab_alloc_locked;
		skm->skm_slab_free = skmem_slab_free_locked;

		/* auditing was requested? (normal regions only) */
		if (skmem_debug & SKMEM_DEBUG_AUDIT) {
			ASSERT(bc_size == sizeof(struct skmem_bufctl_audit));
			skm->skm_mode |= SKM_MODE_AUDIT;
		}
	}

	/*
	 * Clear upon free (to slab layer) as long as the region is
	 * not marked as read-only for kernel, and if the chunk size
	 * is within the threshold or if the caller had requested it.
	 */
	if (!(region->skr_mode & SKR_MODE_KREADONLY)) {
		if (skm->skm_objsize <= skmem_clear_min ||
		    (cflags & SKMEM_CR_CLEARONFREE)) {
			skm->skm_mode |= SKM_MODE_CLEARONFREE;
		}
	}

	chunksize = bufsize;
	if (bufalign >= SKMEM_CACHE_ALIGN) {
		chunksize = P2ROUNDUP(chunksize, SKMEM_CACHE_ALIGN);
	}

	chunksize = P2ROUNDUP(chunksize, bufalign);
	if (chunksize > objsize) {
		panic("%s: (bufsize %lu, chunksize %lu) > objsize %lu",
		    __func__, bufsize, chunksize, objsize);
		/* NOTREACHED */
		__builtin_unreachable();
	}
	ASSERT(chunksize != 0);
	skm->skm_chunksize = chunksize;

	lck_mtx_init(&skm->skm_sl_lock, &skmem_sl_lock_grp, &skmem_lock_attr);
	TAILQ_INIT(&skm->skm_sl_partial_list);
	TAILQ_INIT(&skm->skm_sl_empty_list);

	/* allocated-address hash table */
	skm->skm_hash_initial = SKMEM_CACHE_HASH_INITIAL;
	skm->skm_hash_limit = SKMEM_CACHE_HASH_LIMIT;
	skm->skm_hash_table = sk_alloc_type_array(struct skmem_bufctl_bkt,
	    skm->skm_hash_initial, Z_WAITOK | Z_NOFAIL, skmem_tag_bufctl_hash);
	skm->skm_hash_size = skm->skm_hash_initial;

	skm->skm_hash_mask = (skm->skm_hash_initial - 1);
	skm->skm_hash_shift = flsll(chunksize) - 1;

	for (i = 0; i < (skm->skm_hash_mask + 1); i++) {
		SLIST_INIT(&skm->skm_hash_table[i].bcb_head);
	}

	lck_mtx_init(&skm->skm_dp_lock, &skmem_dp_lock_grp, &skmem_lock_attr);

	/* find a suitable magazine type for this chunk size */
	for (mtp = skmem_magtype; chunksize <= mtp->mt_minbuf; mtp++) {
		continue;
	}

	skm->skm_magtype = mtp;
	if (!(skm->skm_mode & SKM_MODE_NOMAGAZINES)) {
		skm->skm_cpu_mag_size = skm->skm_magtype->mt_magsize;
	}

	/*
	 * Initialize the CPU layer.  Each per-CPU structure is aligned
	 * on the CPU cache line boundary to prevent false sharing.
	 */
	lck_mtx_init(&skm->skm_rs_lock, &skmem_cpu_lock_grp, &skmem_lock_attr);
	for (cpuid = 0; cpuid < ncpu; cpuid++) {
		struct skmem_cpu_cache *ccp = &skm->skm_cpu_cache[cpuid];

		VERIFY(IS_P2ALIGNED(ccp, CHANNEL_CACHE_ALIGN_MAX));
		lck_mtx_init(&ccp->cp_lock, &skmem_cpu_lock_grp,
		    &skmem_lock_attr);
		ccp->cp_rounds = -1;
		ccp->cp_prounds = -1;
	}

	SKMEM_CACHE_LOCK();
	TAILQ_INSERT_TAIL(&skmem_cache_head, skm, skm_link);
	SKMEM_CACHE_UNLOCK();

	SK_DF(SK_VERB_MEM_CACHE, "\"%s\": skm 0x%llx mode 0x%b",
	    skm->skm_name, SK_KVA(skm), skm->skm_mode, SKM_MODE_BITS);
	SK_DF(SK_VERB_MEM_CACHE,
	    "  bufsz %u bufalign %u chunksz %u objsz %u slabsz %u",
	    (uint32_t)skm->skm_bufsize, (uint32_t)skm->skm_bufalign,
	    (uint32_t)skm->skm_chunksize, (uint32_t)skm->skm_objsize,
	    (uint32_t)skm->skm_slabsize);

	if (skmem_cache_ready) {
		skmem_cache_magazine_enable(skm, 0);
	}

	if (cflags & SKMEM_CR_RECLAIM) {
		skm->skm_mode |= SKM_MODE_RECLAIM;
	}

	return skm;
}

/*
 * Destroy a cache.
 */
void
skmem_cache_destroy(struct skmem_cache *skm)
{
	uint32_t cpuid;

	SKMEM_CACHE_LOCK();
	TAILQ_REMOVE(&skmem_cache_head, skm, skm_link);
	SKMEM_CACHE_UNLOCK();

	ASSERT(skm->skm_rs_busy == 0);
	ASSERT(skm->skm_rs_want == 0);

	/* purge all cached objects for this cache */
	skmem_cache_magazine_purge(skm);

	/*
	 * Panic if we detect there are unfreed objects; the caller
	 * destroying this cache is responsible for ensuring that all
	 * allocated objects have been freed prior to getting here.
	 */
	SKM_SLAB_LOCK(skm);
	if (skm->skm_sl_bufinuse != 0) {
		panic("%s: '%s' (%p) not empty (%llu unfreed)", __func__,
		    skm->skm_name, (void *)skm, skm->skm_sl_bufinuse);
		/* NOTREACHED */
		__builtin_unreachable();
	}
	ASSERT(TAILQ_EMPTY(&skm->skm_sl_partial_list));
	ASSERT(skm->skm_sl_partial == 0);
	ASSERT(TAILQ_EMPTY(&skm->skm_sl_empty_list));
	ASSERT(skm->skm_sl_empty == 0);
	skm->skm_reclaim = NULL;
	skm->skm_ctor = NULL;
	skm->skm_dtor = NULL;
	SKM_SLAB_UNLOCK(skm);

	if (skm->skm_hash_table != NULL) {
#if (DEBUG || DEVELOPMENT)
		for (uint32_t i = 0; i < (skm->skm_hash_mask + 1); i++) {
			ASSERT(SLIST_EMPTY(&skm->skm_hash_table[i].bcb_head));
		}
#endif /* DEBUG || DEVELOPMENT */

		/* XXX -fbounds-safety: __counted_by pointer (skm_hash_table)
		 * cannot be pointed to by any other variable */
		struct skmem_bufctl_bkt *__indexable htable = skm->skm_hash_table;
		sk_free_type_array(struct skmem_bufctl_bkt,
		    skm->skm_hash_size, htable);
		skm->skm_hash_table = NULL;
		htable = NULL;
		skm->skm_hash_size = 0;
	}

	for (cpuid = 0; cpuid < ncpu; cpuid++) {
		lck_mtx_destroy(&skm->skm_cpu_cache[cpuid].cp_lock,
		    &skmem_cpu_lock_grp);
	}
	lck_mtx_destroy(&skm->skm_rs_lock, &skmem_cpu_lock_grp);
	lck_mtx_destroy(&skm->skm_dp_lock, &skmem_dp_lock_grp);
	lck_mtx_destroy(&skm->skm_sl_lock, &skmem_sl_lock_grp);

	SK_DF(SK_VERB_MEM_CACHE, "\"%s\": skm 0x%llx",
	    skm->skm_name, SK_KVA(skm));

	/* callee releases reference */
	skmem_region_slab_config(skm->skm_region, skm, false);
	skm->skm_region = NULL;

#if KASAN
	/* get the original address since we're about to free it */
	zfree(skm_zone, skm->skm_start);
#else
	zfree(skm_zone, skm);
#endif /* KASAN */
}

/*
 * Return the object's region info.
 */
void
skmem_cache_get_obj_info(struct skmem_cache *skm, void *buf,
    struct skmem_obj_info *oi, struct skmem_obj_info *oim)
{
	struct skmem_bufctl_bkt *bcb;
	struct skmem_bufctl *bc;
	struct skmem_slab *sl;

	/*
	 * Search the hash chain to find a matching buffer control for the
	 * given object address.  If not found, panic since the caller has
	 * given us a bogus address.
	 */
	SKM_SLAB_LOCK(skm);
	bcb = SKMEM_CACHE_HASH(skm, buf);
	SLIST_FOREACH(bc, &bcb->bcb_head, bc_link) {
		if (bc->bc_addr == buf) {
			break;
		}
	}

	if (__improbable(bc == NULL)) {
		panic("%s: %s failed to get object info for %p",
		    __func__, skm->skm_name, buf);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	/*
	 * Return the master object's info to the caller.
	 */
	sl = bc->bc_slab;
	SKMEM_OBJ_ADDR(oi) = __unsafe_forge_bidi_indexable(void *, bc->bc_addr,
	    (uint32_t)skm->skm_objsize);
	SKMEM_OBJ_SIZE(oi) = (uint32_t)skm->skm_objsize;
	ASSERT(skm->skm_objsize <= UINT32_MAX);
	SKMEM_OBJ_BUFCTL(oi) = bc;      /* master only; NULL for slave */
	SKMEM_OBJ_IDX_REG(oi) =
	    (sl->sl_seg->sg_index * sl->sl_chunks) + bc->bc_idx;
	SKMEM_OBJ_IDX_SEG(oi) = bc->bc_idx;
	/*
	 * And for slave object.
	 */
	if (oim != NULL) {
		bzero(oim, sizeof(*oim));
		if (bc->bc_addrm != NULL) {
			SKMEM_OBJ_ADDR(oim) = __unsafe_forge_bidi_indexable(
				void *, bc->bc_addrm, oi->oi_size);
			SKMEM_OBJ_SIZE(oim) = oi->oi_size;
			SKMEM_OBJ_IDX_REG(oim) = oi->oi_idx_reg;
			SKMEM_OBJ_IDX_SEG(oim) = oi->oi_idx_seg;
		}
	}
	SKM_SLAB_UNLOCK(skm);
}

/*
 * Magazine constructor.
 */
static int
skmem_magazine_ctor(struct skmem_obj_info *oi, struct skmem_obj_info *oim,
    void *arg, uint32_t skmflag)
{
#pragma unused(oim, skmflag)
	struct skmem_mag *__single mg = SKMEM_OBJ_ADDR(oi);

	ASSERT(oim == NULL);
	ASSERT(arg != NULL);

	/*
	 * Store it in the magazine object since we'll
	 * need to refer to it during magazine destroy;
	 * we can't safely refer to skm_magtype as the
	 * depot lock may not be acquired then.
	 */
	mg->mg_magtype = arg;

	return 0;
}

/*
 * Destroy a magazine (free each object to the slab layer).
 */
static void
skmem_magazine_destroy(struct skmem_cache *skm, struct skmem_mag *mg,
    int nrounds)
{
	int round;

	for (round = 0; round < nrounds; round++) {
		void *__single buf = mg->mg_round[round];
		struct skmem_obj *next;

		if (skm->skm_mode & SKM_MODE_BATCH) {
			next = ((struct skmem_obj *)buf)->mo_next;
			((struct skmem_obj *)buf)->mo_next = NULL;
		}

		/* deconstruct the object */
		if (skm->skm_dtor != NULL) {
			skm->skm_dtor(buf, skm->skm_private);
		}

		/*
		 * In non-batching mode, each object in the magazine has
		 * no linkage to its neighbor, so free individual object
		 * to the slab layer now.
		 */
		if (!(skm->skm_mode & SKM_MODE_BATCH)) {
			skmem_slab_free(skm, buf);
		} else {
			((struct skmem_obj *)buf)->mo_next = next;
		}
	}

	/*
	 * In batching mode, each object is linked to its neighbor at free
	 * time, and so take the bottom-most object and free it to the slab
	 * layer.  Because of the way the list is reversed during free, this
	 * will bring along the rest of objects above it.
	 */
	if (nrounds > 0 && (skm->skm_mode & SKM_MODE_BATCH)) {
		skmem_slab_batch_free(skm, mg->mg_round[nrounds - 1]);
	}

	/* free the magazine itself back to cache */
	skmem_cache_free(mg->mg_magtype->mt_cache, mg);
}

/*
 * Get one or more magazines from the depot.
 */
static uint32_t
skmem_depot_batch_alloc(struct skmem_cache *skm, struct skmem_maglist *ml,
    uint32_t *count, struct skmem_mag *__bidi_indexable *list, uint32_t num)
{
	SLIST_HEAD(, skmem_mag) mg_list = SLIST_HEAD_INITIALIZER(mg_list);
	struct skmem_mag *mg;
	uint32_t need = num, c = 0;

	ASSERT(list != NULL && need > 0);

	if (!SKM_DEPOT_LOCK_TRY(skm)) {
		/*
		 * Track the amount of lock contention here; if the contention
		 * level is high (more than skmem_cache_depot_contention per a
		 * given skmem_cache_update_interval interval), then we treat
		 * it as a sign that the per-CPU layer is not using the right
		 * magazine type, and that we'd need to resize it.
		 */
		SKM_DEPOT_LOCK(skm);
		if (skm->skm_mode & SKM_MODE_DYNAMIC) {
			skm->skm_depot_contention++;
		}
	}

	while ((mg = SLIST_FIRST(&ml->ml_list)) != NULL) {
		SLIST_REMOVE_HEAD(&ml->ml_list, mg_link);
		SLIST_INSERT_HEAD(&mg_list, mg, mg_link);
		ASSERT(ml->ml_total != 0);
		if (--ml->ml_total < ml->ml_min) {
			ml->ml_min = ml->ml_total;
		}
		c++;
		ml->ml_alloc++;
		if (--need == 0) {
			break;
		}
	}
	*count -= c;

	SKM_DEPOT_UNLOCK(skm);

	*list = SLIST_FIRST(&mg_list);

	return num - need;
}

/*
 * Return one or more magazines to the depot.
 */
static void
skmem_depot_batch_free(struct skmem_cache *skm, struct skmem_maglist *ml,
    uint32_t *count, struct skmem_mag *mg)
{
	struct skmem_mag *nmg;
	uint32_t c = 0;

	SKM_DEPOT_LOCK(skm);
	while (mg != NULL) {
		nmg = SLIST_NEXT(mg, mg_link);
		SLIST_INSERT_HEAD(&ml->ml_list, mg, mg_link);
		ml->ml_total++;
		c++;
		mg = nmg;
	}
	*count += c;
	SKM_DEPOT_UNLOCK(skm);
}

/*
 * Update the depot's working state statistics.
 */
static void
skmem_depot_ws_update(struct skmem_cache *skm)
{
	SKM_DEPOT_LOCK_SPIN(skm);
	skm->skm_full.ml_reaplimit = skm->skm_full.ml_min;
	skm->skm_full.ml_min = skm->skm_full.ml_total;
	skm->skm_empty.ml_reaplimit = skm->skm_empty.ml_min;
	skm->skm_empty.ml_min = skm->skm_empty.ml_total;
	SKM_DEPOT_UNLOCK(skm);
}

/*
 * Empty the depot's working state statistics (everything's reapable.)
 */
static void
skmem_depot_ws_zero(struct skmem_cache *skm)
{
	SKM_DEPOT_LOCK_SPIN(skm);
	if (skm->skm_full.ml_reaplimit != skm->skm_full.ml_total ||
	    skm->skm_full.ml_min != skm->skm_full.ml_total ||
	    skm->skm_empty.ml_reaplimit != skm->skm_empty.ml_total ||
	    skm->skm_empty.ml_min != skm->skm_empty.ml_total) {
		skm->skm_full.ml_reaplimit = skm->skm_full.ml_total;
		skm->skm_full.ml_min = skm->skm_full.ml_total;
		skm->skm_empty.ml_reaplimit = skm->skm_empty.ml_total;
		skm->skm_empty.ml_min = skm->skm_empty.ml_total;
		skm->skm_depot_ws_zero++;
	}
	SKM_DEPOT_UNLOCK(skm);
}

/*
 * Reap magazines that's outside of the working set.
 */
static void
skmem_depot_ws_reap(struct skmem_cache *skm)
{
	struct skmem_mag *mg, *nmg;
	uint32_t f, e, reap;

	reap = f = MIN(skm->skm_full.ml_reaplimit, skm->skm_full.ml_min);
	if (reap != 0) {
		(void) skmem_depot_batch_alloc(skm, &skm->skm_full,
		    &skm->skm_depot_full, &mg, reap);
		while (mg != NULL) {
			nmg = SLIST_NEXT(mg, mg_link);
			SLIST_NEXT(mg, mg_link) = NULL;
			skmem_magazine_destroy(skm, mg,
			    mg->mg_magtype->mt_magsize);
			mg = nmg;
		}
	}

	reap = e = MIN(skm->skm_empty.ml_reaplimit, skm->skm_empty.ml_min);
	if (reap != 0) {
		(void) skmem_depot_batch_alloc(skm, &skm->skm_empty,
		    &skm->skm_depot_empty, &mg, reap);
		while (mg != NULL) {
			nmg = SLIST_NEXT(mg, mg_link);
			SLIST_NEXT(mg, mg_link) = NULL;
			skmem_magazine_destroy(skm, mg, 0);
			mg = nmg;
		}
	}

	if (f != 0 || e != 0) {
		os_atomic_inc(&skm->skm_cpu_mag_reap, relaxed);
	}
}

/*
 * Performs periodic maintenance on a cache.  This is serialized
 * through the update thread call, and so we guarantee there's at
 * most one update episode in the system at any given time.
 */
static void
skmem_cache_update(struct skmem_cache *skm, uint32_t arg)
{
#pragma unused(arg)
	boolean_t resize_mag = FALSE;
	boolean_t rescale_hash = FALSE;

	SKMEM_CACHE_LOCK_ASSERT_HELD();

	/* insist that we are executing in the update thread call context */
	ASSERT(sk_is_cache_update_protected());

	/*
	 * If the cache has become much larger or smaller than the
	 * allocated-address hash table, rescale the hash table.
	 */
	SKM_SLAB_LOCK(skm);
	if ((skm->skm_sl_bufinuse > (skm->skm_hash_mask << 1) &&
	    (skm->skm_hash_mask + 1) < skm->skm_hash_limit) ||
	    (skm->skm_sl_bufinuse < (skm->skm_hash_mask >> 1) &&
	    skm->skm_hash_mask > skm->skm_hash_initial)) {
		rescale_hash = TRUE;
	}
	SKM_SLAB_UNLOCK(skm);

	/*
	 * Update the working set.
	 */
	skmem_depot_ws_update(skm);

	/*
	 * If the contention count is greater than the threshold during
	 * the update interval, and if we are not already at the maximum
	 * magazine size, increase it.
	 */
	SKM_DEPOT_LOCK_SPIN(skm);
	if (skm->skm_chunksize < skm->skm_magtype->mt_maxbuf &&
	    (int)(skm->skm_depot_contention - skm->skm_depot_contention_prev) >
	    skmem_cache_depot_contention) {
		ASSERT(skm->skm_mode & SKM_MODE_DYNAMIC);
		resize_mag = TRUE;
	}
	skm->skm_depot_contention_prev = skm->skm_depot_contention;
	SKM_DEPOT_UNLOCK(skm);

	if (rescale_hash) {
		skmem_cache_hash_rescale(skm);
	}

	if (resize_mag) {
		skmem_cache_magazine_resize(skm);
	}
}

/*
 * Reload the CPU's magazines with mg and its follower (if any).
 */
static void
skmem_cpu_batch_reload(struct skmem_cpu_cache *cp, struct skmem_mag *mg,
    int rounds)
{
	ASSERT((cp->cp_loaded == NULL && cp->cp_rounds == -1) ||
	    (cp->cp_loaded && cp->cp_rounds + rounds == cp->cp_magsize));
	ASSERT(cp->cp_magsize > 0);

	cp->cp_loaded = mg;
	cp->cp_rounds = rounds;
	if (__probable(SLIST_NEXT(mg, mg_link) != NULL)) {
		cp->cp_ploaded = SLIST_NEXT(mg, mg_link);
		cp->cp_prounds = rounds;
		SLIST_NEXT(mg, mg_link) = NULL;
	} else {
		ASSERT(SLIST_NEXT(mg, mg_link) == NULL);
		cp->cp_ploaded = NULL;
		cp->cp_prounds = -1;
	}
}

/*
 * Reload the CPU's magazine with mg and save the previous one.
 */
static void
skmem_cpu_reload(struct skmem_cpu_cache *cp, struct skmem_mag *mg, int rounds)
{
	ASSERT((cp->cp_loaded == NULL && cp->cp_rounds == -1) ||
	    (cp->cp_loaded && cp->cp_rounds + rounds == cp->cp_magsize));
	ASSERT(cp->cp_magsize > 0);

	cp->cp_ploaded = cp->cp_loaded;
	cp->cp_prounds = cp->cp_rounds;
	cp->cp_loaded = mg;
	cp->cp_rounds = rounds;
}

/*
 * Allocate constructed object(s) from the cache.
 */
uint32_t
skmem_cache_batch_alloc(struct skmem_cache *skm, struct skmem_obj **list,
    size_t SK_FB_ARG objsize, uint32_t num, uint32_t skmflag)
{
	struct skmem_cpu_cache *cp = SKMEM_CPU_CACHE(skm);
	struct skmem_obj **top = list;
	struct skmem_mag *mg;
	uint32_t need = num;

	ASSERT(list != NULL);
	*list = NULL;

	if (need == 0) {
		return 0;
	}
	ASSERT(need == 1 || (skm->skm_mode & SKM_MODE_BATCH));

	SKM_CPU_LOCK(cp);
	for (;;) {
		/*
		 * If we have an object in the current CPU's loaded
		 * magazine, return it and we're done.
		 */
		if (cp->cp_rounds > 0) {
			int objs = MIN((unsigned int)cp->cp_rounds, need);
			/*
			 * In the SKM_MODE_BATCH case, objects in are already
			 * linked together with the most recently freed object
			 * at the head of the list; grab as many objects as we
			 * can.  Otherwise we'll just grab 1 object at most.
			 */
			*list = cp->cp_loaded->mg_round[cp->cp_rounds - 1];
			cp->cp_rounds -= objs;
			cp->cp_alloc += objs;

			if (skm->skm_mode & SKM_MODE_BATCH) {
				struct skmem_obj *__single tail =
				    cp->cp_loaded->mg_round[cp->cp_rounds];
				list = &tail->mo_next;
				*list = NULL;
			}

			/* if we got them all, return to caller */
			if ((need -= objs) == 0) {
				SKM_CPU_UNLOCK(cp);
				goto done;
			}
		}

		/*
		 * The CPU's loaded magazine is empty.  If the previously
		 * loaded magazine was full, exchange and try again.
		 */
		if (cp->cp_prounds > 0) {
			skmem_cpu_reload(cp, cp->cp_ploaded, cp->cp_prounds);
			continue;
		}

		/*
		 * If the magazine layer is disabled, allocate from slab.
		 * This can happen either because SKM_MODE_NOMAGAZINES is
		 * set, or because we are resizing the magazine now.
		 */
		if (cp->cp_magsize == 0) {
			break;
		}

		/*
		 * Both of the CPU's magazines are empty; try to get
		 * full magazine(s) from the depot layer.  Upon success,
		 * reload and try again.  To prevent potential thrashing,
		 * replace both empty magazines only if the requested
		 * count exceeds a magazine's worth of objects.
		 */
		(void) skmem_depot_batch_alloc(skm, &skm->skm_full,
		    &skm->skm_depot_full, &mg, (need <= cp->cp_magsize) ? 1 : 2);
		if (mg != NULL) {
			SLIST_HEAD(, skmem_mag) mg_list =
			    SLIST_HEAD_INITIALIZER(mg_list);

			if (cp->cp_ploaded != NULL) {
				SLIST_INSERT_HEAD(&mg_list, cp->cp_ploaded,
				    mg_link);
			}
			if (SLIST_NEXT(mg, mg_link) == NULL) {
				/*
				 * Depot allocation returns only 1 magazine;
				 * retain current empty magazine.
				 */
				skmem_cpu_reload(cp, mg, cp->cp_magsize);
			} else {
				/*
				 * We got 2 full magazines from depot;
				 * release the current empty magazine
				 * back to the depot layer.
				 */
				if (cp->cp_loaded != NULL) {
					SLIST_INSERT_HEAD(&mg_list,
					    cp->cp_loaded, mg_link);
				}
				skmem_cpu_batch_reload(cp, mg, cp->cp_magsize);
			}
			skmem_depot_batch_free(skm, &skm->skm_empty,
			    &skm->skm_depot_empty, SLIST_FIRST(&mg_list));
			continue;
		}

		/*
		 * The depot layer doesn't have any full magazines;
		 * allocate directly from the slab layer.
		 */
		break;
	}
	SKM_CPU_UNLOCK(cp);

	if (__probable(num > 1 && (skm->skm_mode & SKM_MODE_BATCH) != 0)) {
		struct skmem_obj *rtop, *__single rlist, *rlistp = NULL;
		uint32_t rlistc, c = 0;

		/*
		 * Get a list of raw objects from the slab layer.
		 */
		rlistc = skmem_slab_batch_alloc(skm, &rlist, need, skmflag);
		ASSERT(rlistc == 0 || rlist != NULL);
		rtop = rlist;

		/*
		 * Construct each object in the raw list.  Upon failure,
		 * free any remaining objects in the list back to the slab
		 * layer, and keep the ones that were successfully constructed.
		 * Here, "oi" and "oim" in each skmem_obj refer to the objects
		 * coming from the master and slave regions (on mirrored
		 * regions), respectively.  They are stored inside the object
		 * temporarily so that we can pass them to the constructor.
		 */
		while (skm->skm_ctor != NULL && rlist != NULL) {
			struct skmem_obj_info *oi = &rlist->mo_info;
			struct skmem_obj_info *oim = &rlist->mo_minfo;
			struct skmem_obj *rlistn = rlist->mo_next;

			/*
			 * Note that the constructor guarantees at least
			 * the size of a pointer at the top of the object
			 * and no more than that.  That means we must not
			 * refer to "oi" and "oim" any longer after the
			 * object goes thru the constructor.
			 */
			if (skm->skm_ctor(oi, ((SKMEM_OBJ_ADDR(oim) != NULL) ?
			    oim : NULL), skm->skm_private, skmflag) != 0) {
				VERIFY(rlist->mo_next == rlistn);
				os_atomic_add(&skm->skm_sl_alloc_fail,
				    rlistc - c, relaxed);
				if (rlistp != NULL) {
					rlistp->mo_next = NULL;
				}
				if (rlist == rtop) {
					rtop = NULL;
					ASSERT(c == 0);
				}
				skmem_slab_batch_free(skm, rlist);
				rlist = NULL;
				rlistc = c;
				break;
			}
			VERIFY(rlist->mo_next == rlistn);

			++c;                    /* # of constructed objs */
			rlistp = rlist;
			if ((rlist = rlist->mo_next) == NULL) {
				ASSERT(rlistc == c);
				break;
			}
		}

		/*
		 * At this point "top" points to the head of the chain we're
		 * going to return to caller; "list" points to the tail of that
		 * chain.  The second chain begins at "rtop", and we append
		 * that after "list" to form a single chain.  "rlistc" is the
		 * number of objects in "rtop" originated from the slab layer
		 * that have been successfully constructed (if applicable).
		 */
		ASSERT(c == 0 || rtop != NULL);
		need -= rlistc;
		*list = rtop;
	} else {
		struct skmem_obj_info oi, oim;
		void *buf;

		ASSERT(*top == NULL && num == 1 && need == 1);

		/*
		 * Get a single raw object from the slab layer.
		 */
		if (skmem_slab_alloc(skm, &oi, &oim, skmflag) != 0) {
			goto done;
		}

		buf = SKMEM_OBJ_ADDR(&oi);
		ASSERT(buf != NULL);

		/*
		 * Construct the raw object.  Here, "oi" and "oim" refer to
		 * the objects coming from the master and slave regions (on
		 * mirrored regions), respectively.
		 */
		if (skm->skm_ctor != NULL &&
		    skm->skm_ctor(&oi, ((SKMEM_OBJ_ADDR(&oim) != NULL) ?
		    &oim : NULL), skm->skm_private, skmflag) != 0) {
			os_atomic_inc(&skm->skm_sl_alloc_fail, relaxed);
			skmem_slab_free(skm, buf);
			goto done;
		}

		need = 0;
		*list = buf;
		ASSERT(!(skm->skm_mode & SKM_MODE_BATCH) ||
		    (*list)->mo_next == NULL);
	}

done:
	/* if auditing is enabled, record this transaction */
	if (__improbable(*top != NULL &&
	    (skm->skm_mode & SKM_MODE_AUDIT) != 0)) {
		skmem_audit_buf(skm,
		    __unsafe_forge_bidi_indexable(struct skmem_obj *, *top, objsize));
	}

	return num - need;
}

/*
 * Free a constructed object to the cache.
 */
void
skmem_cache_free(struct skmem_cache *skm, void *buf)
{
	if (skm->skm_mode & SKM_MODE_BATCH) {
		((struct skmem_obj *)buf)->mo_next = NULL;
	}
	skmem_cache_batch_free_common(skm, (struct skmem_obj *)buf, 0);
}

/*
 * Free a constructed object.
 */
void
skmem_cache_free_nocache(struct skmem_cache *skm, void *buf)
{
	if (skm->skm_mode & SKM_MODE_BATCH) {
		((struct skmem_obj *)buf)->mo_next = NULL;
	}
	skmem_cache_batch_free_common(skm, (struct skmem_obj *)buf, SKMEM_CACHE_FREE_NOCACHE);
}

void
skmem_cache_batch_free(struct skmem_cache *skm, struct skmem_obj *list)
{
	skmem_cache_batch_free_common(skm, list, 0);
}

void
skmem_cache_batch_free_nocache(struct skmem_cache *skm, struct skmem_obj *list)
{
	skmem_cache_batch_free_common(skm, list, SKMEM_CACHE_FREE_NOCACHE);
}

static void
skmem_cache_batch_free_common(struct skmem_cache *skm, struct skmem_obj *list, uint32_t flags)
{
	struct skmem_cpu_cache *cp = SKMEM_CPU_CACHE(skm);
	struct skmem_magtype *mtp;
	/*
	 * XXX -fbounds-safety: Don't mark mg as __single, because it's a struct
	 * with a flexible array, and when we allocate it, the alloc function
	 * returns an __indexable to tell us the bounds. But if we mark this as
	 * __single, we lose that information. It might compile fine, but at
	 * runtime, before we actually assign the count value, there will be a
	 * comparison between current count value and the new count value we
	 * assign, where current count is supposed to be greater than the new
	 * count. Unfortunately, this will most likely fail.
	 */
	struct skmem_mag *mg;
	struct skmem_obj *listn;
#if CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT)
	vm_offset_t tagged_address;           /* address tagging */
	struct skmem_region *region;          /* region source for this cache */
#endif /* CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT) */

	/* if auditing is enabled, record this transaction */
	if (__improbable((skm->skm_mode & SKM_MODE_AUDIT) != 0)) {
		skmem_audit_buf(skm, list);
	}

	if (flags & SKMEM_CACHE_FREE_NOCACHE) {
		goto nocache;
	}

	SKM_CPU_LOCK(cp);
	for (;;) {
		/*
		 * If there's an available space in the current CPU's
		 * loaded magazine, place it there and we're done.
		 */
		if ((unsigned int)cp->cp_rounds <
		    (unsigned int)cp->cp_magsize) {
			/*
			 * In the SKM_MODE_BATCH case, reverse the list
			 * while we place each object into the magazine;
			 * this effectively causes the most recently
			 * freed object to be reused during allocation.
			 */
			if (skm->skm_mode & SKM_MODE_BATCH) {
				listn = list->mo_next;
				list->mo_next = (cp->cp_rounds == 0) ? NULL :
				    cp->cp_loaded->mg_round[cp->cp_rounds - 1];
			} else {
				listn = NULL;
			}
#if CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT)
			/*
			 * If this region is configured to be tagged, we re-tag
			 * the address that's being freed, to protect against
			 * use-after-free bugs. This "re-tagged" address will
			 * reside in the CPU's loaded magazine, and when cache
			 * alloc is called, it is returned to client as is. At
			 * this point, we know that this object will be freed to
			 * the CPU's loaded magazine and not down to the slab
			 * layer, so we won't be double tagging the same address
			 * in the magazine layer and slab layer.
			 */
			region = skm->skm_region;
			if (region->skr_mode & SKR_MODE_MEMTAG) {
				tagged_address = vm_memtag_assign_tag(
					(vm_offset_t)list, skm->skm_objsize);
				vm_memtag_set_tag(tagged_address,
				    skm->skm_objsize);
				cp->cp_loaded->mg_round[cp->cp_rounds++] =
				    __unsafe_forge_bidi_indexable(
					struct skmem_obj *, tagged_address,
					skm->skm_objsize);
			} else {
				cp->cp_loaded->mg_round[cp->cp_rounds++] = list;
			}
#else /* !CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT) */
			cp->cp_loaded->mg_round[cp->cp_rounds++] = list;
#endif /* CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT) */
			cp->cp_free++;

			if ((list = listn) != NULL) {
				continue;
			}

			SKM_CPU_UNLOCK(cp);
			return;
		}

		/*
		 * The loaded magazine is full.  If the previously
		 * loaded magazine was empty, exchange and try again.
		 */
		if (cp->cp_prounds == 0) {
			skmem_cpu_reload(cp, cp->cp_ploaded, cp->cp_prounds);
			continue;
		}

		/*
		 * If the magazine layer is disabled, free to slab.
		 * This can happen either because SKM_MODE_NOMAGAZINES
		 * is set, or because we are resizing the magazine now.
		 */
		if (cp->cp_magsize == 0) {
			break;
		}

		/*
		 * Both magazines for the CPU are full; try to get
		 * empty magazine(s) from the depot.  If we get one,
		 * exchange a full magazine with it and place the
		 * object in there.
		 *
		 * TODO: Because the caller currently doesn't indicate
		 * the number of objects in the list, we choose the more
		 * conservative approach of allocating only 1 empty
		 * magazine (to prevent potential thrashing).  Once we
		 * have the object count, we can replace 1 with similar
		 * logic as used in skmem_cache_batch_alloc().
		 */
		(void) skmem_depot_batch_alloc(skm, &skm->skm_empty,
		    &skm->skm_depot_empty, &mg, 1);
		if (mg != NULL) {
			SLIST_HEAD(, skmem_mag) mg_list =
			    SLIST_HEAD_INITIALIZER(mg_list);

			if (cp->cp_ploaded != NULL) {
				SLIST_INSERT_HEAD(&mg_list, cp->cp_ploaded,
				    mg_link);
			}
			if (SLIST_NEXT(mg, mg_link) == NULL) {
				/*
				 * Depot allocation returns only 1 magazine;
				 * retain current full magazine.
				 */
				skmem_cpu_reload(cp, mg, 0);
			} else {
				/*
				 * We got 2 empty magazines from depot;
				 * release the current full magazine back
				 * to the depot layer.
				 */
				if (cp->cp_loaded != NULL) {
					SLIST_INSERT_HEAD(&mg_list,
					    cp->cp_loaded, mg_link);
				}
				skmem_cpu_batch_reload(cp, mg, 0);
			}
			skmem_depot_batch_free(skm, &skm->skm_full,
			    &skm->skm_depot_full, SLIST_FIRST(&mg_list));
			continue;
		}

		/*
		 * We can't get any empty magazine from the depot, and
		 * so we need to allocate one.  If the allocation fails,
		 * just fall through, deconstruct and free the object
		 * to the slab layer.
		 */
		mtp = skm->skm_magtype;
		SKM_CPU_UNLOCK(cp);
		mg = skmem_cache_alloc(mtp->mt_cache, SKMEM_NOSLEEP);
		SKM_CPU_LOCK(cp);

		if (mg != NULL) {
			/*
			 * XXX -fbounds-safety requires mg to be set before
			 * setting mg->mg_count. But self-assignment mg = mg was
			 * not allowed. As such, we used the following
			 * workaround
			 */
			void *vmg = mg;
			mg = vmg;
			mg->mg_count = mg->mg_magtype->mt_magsize;
			/*
			 * We allocated an empty magazine, but since we
			 * dropped the CPU lock above the magazine size
			 * may have changed.  If that's the case free
			 * the magazine and try again.
			 */
			if (cp->cp_magsize != mtp->mt_magsize) {
				SKM_CPU_UNLOCK(cp);
				skmem_cache_free(mtp->mt_cache, mg);
				SKM_CPU_LOCK(cp);
				continue;
			}

			/*
			 * We have a magazine with the right size;
			 * add it to the depot and try again.
			 */
			ASSERT(SLIST_NEXT(mg, mg_link) == NULL);
			skmem_depot_batch_free(skm, &skm->skm_empty,
			    &skm->skm_depot_empty, mg);
			continue;
		}

		/*
		 * We can't get an empty magazine, so free to slab.
		 */
		break;
	}
	SKM_CPU_UNLOCK(cp);

nocache:
	/*
	 * We weren't able to free the constructed object(s) to the
	 * magazine layer, so deconstruct them and free to the slab.
	 */
	if (__probable((skm->skm_mode & SKM_MODE_BATCH) &&
	    list->mo_next != NULL)) {
		/* whatever is left from original list */
		struct skmem_obj *top = list;

		while (list != NULL && skm->skm_dtor != NULL) {
			listn = list->mo_next;
			list->mo_next = NULL;

			/* deconstruct the object */
			if (skm->skm_dtor != NULL) {
				skm->skm_dtor((void *)list, skm->skm_private);
			}

			list->mo_next = listn;
			list = listn;
		}

		skmem_slab_batch_free(skm, top);
	} else {
		/* deconstruct the object */
		if (skm->skm_dtor != NULL) {
			skm->skm_dtor((void *)list, skm->skm_private);
		}

		skmem_slab_free(skm, (void *)list);
	}
}

/*
 * Return the maximum number of objects cached at the magazine layer
 * based on the chunk size.  This takes into account the starting
 * magazine type as well as the final magazine type used in resizing.
 */
uint32_t
skmem_cache_magazine_max(uint32_t chunksize)
{
	struct skmem_magtype *mtp;
	uint32_t magsize_max;

	VERIFY(ncpu != 0);
	VERIFY(chunksize > 0);

	/* find a suitable magazine type for this chunk size */
	for (mtp = skmem_magtype; chunksize <= mtp->mt_minbuf; mtp++) {
		continue;
	}

	/* and find the last magazine type  */
	for (;;) {
		magsize_max = mtp->mt_magsize;
		if (mtp == skmem_cache_magsize_last ||
		    chunksize >= mtp->mt_maxbuf) {
			break;
		}
		++mtp;
		VERIFY(mtp <= skmem_cache_magsize_last);
	}

	return ncpu * magsize_max * 2; /* two magazines per CPU */
}

/*
 * Return true if SKMEM_DEBUG_NOMAGAZINES is not set on skmem_debug.
 */
boolean_t
skmem_allow_magazines(void)
{
	return !(skmem_debug & SKMEM_DEBUG_NOMAGAZINES);
}

/*
 * Purge all magazines from a cache and disable its per-CPU magazines layer.
 */
static void
skmem_cache_magazine_purge(struct skmem_cache *skm)
{
	struct skmem_cpu_cache *cp;
	struct skmem_mag *mg, *pmg;
	int rounds, prounds;
	uint32_t cpuid, mg_cnt = 0, pmg_cnt = 0;

	SKM_SLAB_LOCK_ASSERT_NOTHELD(skm);

	SK_DF(SK_VERB_MEM_CACHE, "skm 0x%llx", SK_KVA(skm));

	for (cpuid = 0; cpuid < ncpu; cpuid++) {
		cp = &skm->skm_cpu_cache[cpuid];

		SKM_CPU_LOCK_SPIN(cp);
		mg = cp->cp_loaded;
		pmg = cp->cp_ploaded;
		rounds = cp->cp_rounds;
		prounds = cp->cp_prounds;
		cp->cp_loaded = NULL;
		cp->cp_ploaded = NULL;
		cp->cp_rounds = -1;
		cp->cp_prounds = -1;
		cp->cp_magsize = 0;
		SKM_CPU_UNLOCK(cp);

		if (mg != NULL) {
			skmem_magazine_destroy(skm, mg, rounds);
			++mg_cnt;
		}
		if (pmg != NULL) {
			skmem_magazine_destroy(skm, pmg, prounds);
			++pmg_cnt;
		}
	}

	if (mg_cnt != 0 || pmg_cnt != 0) {
		os_atomic_inc(&skm->skm_cpu_mag_purge, relaxed);
	}

	skmem_depot_ws_zero(skm);
	skmem_depot_ws_reap(skm);
}

/*
 * Enable magazines on a cache.  Must only be called on a cache with
 * its per-CPU magazines layer disabled (e.g. due to purge).
 */
static void
skmem_cache_magazine_enable(struct skmem_cache *skm, uint32_t arg)
{
#pragma unused(arg)
	struct skmem_cpu_cache *cp;
	uint32_t cpuid;

	if (skm->skm_mode & SKM_MODE_NOMAGAZINES) {
		return;
	}

	for (cpuid = 0; cpuid < ncpu; cpuid++) {
		cp = &skm->skm_cpu_cache[cpuid];
		SKM_CPU_LOCK_SPIN(cp);
		/* the magazines layer must be disabled at this point */
		ASSERT(cp->cp_loaded == NULL);
		ASSERT(cp->cp_ploaded == NULL);
		ASSERT(cp->cp_rounds == -1);
		ASSERT(cp->cp_prounds == -1);
		ASSERT(cp->cp_magsize == 0);
		cp->cp_magsize = skm->skm_magtype->mt_magsize;
		SKM_CPU_UNLOCK(cp);
	}

	SK_DF(SK_VERB_MEM_CACHE, "skm 0x%llx chunksize %u magsize %d",
	    SK_KVA(skm), (uint32_t)skm->skm_chunksize,
	    SKMEM_CPU_CACHE(skm)->cp_magsize);
}

/*
 * Enter the cache resize perimeter.  Upon success, claim exclusivity
 * on the perimeter and return 0, else EBUSY.  Caller may indicate
 * whether or not they're willing to wait.
 */
static int
skmem_cache_resize_enter(struct skmem_cache *skm, boolean_t can_sleep)
{
	SKM_RESIZE_LOCK(skm);
	if (skm->skm_rs_owner == current_thread()) {
		ASSERT(skm->skm_rs_busy != 0);
		skm->skm_rs_busy++;
		goto done;
	}
	if (!can_sleep) {
		if (skm->skm_rs_busy != 0) {
			SKM_RESIZE_UNLOCK(skm);
			return EBUSY;
		}
	} else {
		while (skm->skm_rs_busy != 0) {
			skm->skm_rs_want++;
			(void) assert_wait(&skm->skm_rs_busy, THREAD_UNINT);
			SKM_RESIZE_UNLOCK(skm);
			(void) thread_block(THREAD_CONTINUE_NULL);
			SK_DF(SK_VERB_MEM_CACHE, "waited for skm \"%s\" "
			    "(0x%llx) busy=%u", skm->skm_name,
			    SK_KVA(skm), skm->skm_rs_busy);
			SKM_RESIZE_LOCK(skm);
		}
	}
	SKM_RESIZE_LOCK_ASSERT_HELD(skm);
	ASSERT(skm->skm_rs_busy == 0);
	skm->skm_rs_busy++;
	skm->skm_rs_owner = current_thread();
done:
	SKM_RESIZE_UNLOCK(skm);
	return 0;
}

/*
 * Exit the cache resize perimeter and unblock any waiters.
 */
static void
skmem_cache_resize_exit(struct skmem_cache *skm)
{
	uint32_t want;

	SKM_RESIZE_LOCK(skm);
	ASSERT(skm->skm_rs_busy != 0);
	ASSERT(skm->skm_rs_owner == current_thread());
	if (--skm->skm_rs_busy == 0) {
		skm->skm_rs_owner = NULL;
		/*
		 * We're done; notify anyone that has lost the race.
		 */
		if ((want = skm->skm_rs_want) != 0) {
			skm->skm_rs_want = 0;
			wakeup((void *)&skm->skm_rs_busy);
			SKM_RESIZE_UNLOCK(skm);
		} else {
			SKM_RESIZE_UNLOCK(skm);
		}
	} else {
		SKM_RESIZE_UNLOCK(skm);
	}
}

/*
 * Recompute a cache's magazine size.  This is an expensive operation
 * and should not be done frequently; larger magazines provide for a
 * higher transfer rate with the depot while smaller magazines reduce
 * the memory consumption.
 */
static void
skmem_cache_magazine_resize(struct skmem_cache *skm)
{
	struct skmem_magtype *mtp = __unsafe_forge_bidi_indexable(
		struct skmem_magtype *, skm->skm_magtype, sizeof(skmem_magtype));

	/* insist that we are executing in the update thread call context */
	ASSERT(sk_is_cache_update_protected());
	ASSERT(!(skm->skm_mode & SKM_MODE_NOMAGAZINES));
	/* depot contention only applies to dynamic mode */
	ASSERT(skm->skm_mode & SKM_MODE_DYNAMIC);

	/*
	 * Although we're executing in the context of the update thread
	 * call, we need to protect the per-CPU states during resizing
	 * against other synchronous cache purge/reenable requests that
	 * could take place in parallel.
	 */
	if (skm->skm_chunksize < mtp->mt_maxbuf) {
		(void) skmem_cache_resize_enter(skm, TRUE);
		skmem_cache_magazine_purge(skm);

		/*
		 * Upgrade to the next magazine type with larger size.
		 */
		SKM_DEPOT_LOCK_SPIN(skm);
		skm->skm_cpu_mag_resize++;
		skm->skm_magtype = ++mtp;
		skm->skm_cpu_mag_size = skm->skm_magtype->mt_magsize;
		skm->skm_depot_contention_prev =
		    skm->skm_depot_contention + INT_MAX;
		SKM_DEPOT_UNLOCK(skm);

		skmem_cache_magazine_enable(skm, 0);
		skmem_cache_resize_exit(skm);
	}
}

/*
 * Rescale the cache's allocated-address hash table.
 */
static void
skmem_cache_hash_rescale(struct skmem_cache *skm)
{
	struct skmem_bufctl_bkt *__indexable old_table, *new_table;
	size_t old_size, new_size;
	uint32_t i, moved = 0;

	/* insist that we are executing in the update thread call context */
	ASSERT(sk_is_cache_update_protected());

	/*
	 * To get small average lookup time (lookup depth near 1.0), the hash
	 * table size should be roughly the same (not necessarily equivalent)
	 * as the cache size.
	 */
	new_size = MAX(skm->skm_hash_initial,
	    (1 << (flsll(3 * skm->skm_sl_bufinuse + 4) - 2)));
	new_size = MIN(skm->skm_hash_limit, new_size);
	old_size = (skm->skm_hash_mask + 1);

	if ((old_size >> 1) <= new_size && new_size <= (old_size << 1)) {
		return;
	}

	new_table = sk_alloc_type_array(struct skmem_bufctl_bkt, new_size,
	    Z_NOWAIT, skmem_tag_bufctl_hash);
	if (__improbable(new_table == NULL)) {
		return;
	}

	for (i = 0; i < new_size; i++) {
		SLIST_INIT(&new_table[i].bcb_head);
	}

	SKM_SLAB_LOCK(skm);

	old_size = (skm->skm_hash_mask + 1);
	old_table = skm->skm_hash_table;

	skm->skm_hash_mask = (new_size - 1);
	skm->skm_hash_table = new_table;
	skm->skm_hash_size = new_size;
	skm->skm_sl_rescale++;

	for (i = 0; i < old_size; i++) {
		struct skmem_bufctl_bkt *bcb = &old_table[i];
		struct skmem_bufctl_bkt *new_bcb;
		struct skmem_bufctl *bc;

		while ((bc = SLIST_FIRST(&bcb->bcb_head)) != NULL) {
			SLIST_REMOVE_HEAD(&bcb->bcb_head, bc_link);
			new_bcb = SKMEM_CACHE_HASH(skm, bc->bc_addr);
			/*
			 * Ideally we want to insert tail here, but simple
			 * list doesn't give us that.  The fact that we are
			 * essentially reversing the order is not a big deal
			 * here vis-a-vis the new table size.
			 */
			SLIST_INSERT_HEAD(&new_bcb->bcb_head, bc, bc_link);
			++moved;
		}
		ASSERT(SLIST_EMPTY(&bcb->bcb_head));
	}

	SK_DF(SK_VERB_MEM_CACHE,
	    "skm 0x%llx old_size %u new_size %u [%u moved]", SK_KVA(skm),
	    (uint32_t)old_size, (uint32_t)new_size, moved);

	SKM_SLAB_UNLOCK(skm);

	sk_free_type_array(struct skmem_bufctl_bkt, old_size, old_table);
}

/*
 * Apply a function to operate on all caches.
 */
static void
skmem_cache_applyall(void (*func)(struct skmem_cache *, uint32_t), uint32_t arg)
{
	struct skmem_cache *skm;

	net_update_uptime();

	SKMEM_CACHE_LOCK();
	TAILQ_FOREACH(skm, &skmem_cache_head, skm_link) {
		func(skm, arg);
	}
	SKMEM_CACHE_UNLOCK();
}

/*
 * Reclaim unused memory from a cache.
 */
static void
skmem_cache_reclaim(struct skmem_cache *skm, uint32_t lowmem)
{
	/*
	 * Inform the owner to free memory if possible; the reclaim
	 * policy is left to the owner.  This is just an advisory.
	 */
	if (skm->skm_reclaim != NULL) {
		skm->skm_reclaim(skm->skm_private);
	}

	if (lowmem) {
		/*
		 * If another thread is in the process of purging or
		 * resizing, bail out and let the currently-ongoing
		 * purging take its natural course.
		 */
		if (skmem_cache_resize_enter(skm, FALSE) == 0) {
			skmem_cache_magazine_purge(skm);
			skmem_cache_magazine_enable(skm, 0);
			skmem_cache_resize_exit(skm);
		}
	} else {
		skmem_depot_ws_reap(skm);
	}
}

/*
 * Thread call callback for reap.
 */
static void
skmem_cache_reap_func(thread_call_param_t dummy, thread_call_param_t arg)
{
#pragma unused(dummy)
	void (*func)(void) = arg;

	ASSERT(func == skmem_cache_reap_start || func == skmem_cache_reap_done);
	func();
}

/*
 * Start reaping all caches; this is serialized via thread call.
 */
static void
skmem_cache_reap_start(void)
{
	SK_DF(SK_VERB_MEM_CACHE, "now running");
	skmem_cache_applyall(skmem_cache_reclaim, skmem_lowmem_check());
	skmem_dispatch(skmem_cache_reap_tc, skmem_cache_reap_done,
	    (skmem_cache_update_interval * NSEC_PER_SEC));
}

/*
 * Stop reaping; this would allow another reap request to occur.
 */
static void
skmem_cache_reap_done(void)
{
	volatile uint32_t *flag = &skmem_cache_reaping;

	*flag = 0;
	os_atomic_thread_fence(seq_cst);
}

/*
 * Immediately reap all unused memory of a cache.  If purging,
 * also purge the cached objects at the CPU layer.
 */
void
skmem_cache_reap_now(struct skmem_cache *skm, boolean_t purge)
{
	/* if SKM_MODE_RECLIAM flag is set for this cache, we purge */
	if (purge || (skm->skm_mode & SKM_MODE_RECLAIM)) {
		/*
		 * If another thread is in the process of purging or
		 * resizing, bail out and let the currently-ongoing
		 * purging take its natural course.
		 */
		if (skmem_cache_resize_enter(skm, FALSE) == 0) {
			skmem_cache_magazine_purge(skm);
			skmem_cache_magazine_enable(skm, 0);
			skmem_cache_resize_exit(skm);
		}
	} else {
		skmem_depot_ws_zero(skm);
		skmem_depot_ws_reap(skm);

		/* clean up cp_ploaded magazines from each CPU */
		SKM_SLAB_LOCK_ASSERT_NOTHELD(skm);

		struct skmem_cpu_cache *cp;
		struct skmem_mag *pmg;
		int prounds;
		uint32_t cpuid;

		for (cpuid = 0; cpuid < ncpu; cpuid++) {
			cp = &skm->skm_cpu_cache[cpuid];

			SKM_CPU_LOCK_SPIN(cp);
			pmg = cp->cp_ploaded;
			prounds = cp->cp_prounds;

			cp->cp_ploaded = NULL;
			cp->cp_prounds = -1;
			SKM_CPU_UNLOCK(cp);

			if (pmg != NULL) {
				skmem_magazine_destroy(skm, pmg, prounds);
			}
		}
	}
}

/*
 * Request a global reap operation to be dispatched.
 */
void
skmem_cache_reap(void)
{
	/* only one reaping episode is allowed at a time */
	if (skmem_lock_owner == current_thread() ||
	    !os_atomic_cmpxchg(&skmem_cache_reaping, 0, 1, acq_rel)) {
		return;
	}

	skmem_dispatch(skmem_cache_reap_tc, skmem_cache_reap_start, 0);
}

/*
 * Reap internal caches.
 */
void
skmem_reap_caches(boolean_t purge)
{
	skmem_cache_reap_now(skmem_slab_cache, purge);
	skmem_cache_reap_now(skmem_bufctl_cache, purge);

	/* packet buffer pool objects */
	pp_reap_caches(purge);

	/* also handle the region cache(s) */
	skmem_region_reap_caches(purge);
}

/*
 * Thread call callback for update.
 */
static void
skmem_cache_update_func(thread_call_param_t dummy, thread_call_param_t arg)
{
#pragma unused(dummy, arg)
	sk_protect_t protect;

	protect = sk_cache_update_protect();
	skmem_cache_applyall(skmem_cache_update, 0);
	sk_cache_update_unprotect(protect);

	skmem_dispatch(skmem_cache_update_tc, NULL,
	    (skmem_cache_update_interval * NSEC_PER_SEC));
}

/*
 * Given an object, find its buffer control and record the transaction.
 */
__attribute__((noinline, cold, not_tail_called))
static inline void
skmem_audit_buf(struct skmem_cache *skm, struct skmem_obj *list)
{
	struct skmem_bufctl_bkt *bcb;
	struct skmem_bufctl *bc;

	ASSERT(!(skm->skm_mode & SKM_MODE_PSEUDO));

	SKM_SLAB_LOCK(skm);
	while (list != NULL) {
		void *__single buf = list;

		bcb = SKMEM_CACHE_HASH(skm, buf);
		SLIST_FOREACH(bc, &bcb->bcb_head, bc_link) {
			if (bc->bc_addr == buf) {
				break;
			}
		}

		if (__improbable(bc == NULL)) {
			panic("%s: %s failed to get bufctl for %p",
			    __func__, skm->skm_name, buf);
			/* NOTREACHED */
			__builtin_unreachable();
		}

		skmem_audit_bufctl(bc);

		if (!(skm->skm_mode & SKM_MODE_BATCH)) {
			break;
		}

		list = list->mo_next;
	}
	SKM_SLAB_UNLOCK(skm);
}

static size_t
skmem_cache_mib_get_stats(struct skmem_cache *skm, void *__sized_by(len) out,
    size_t len)
{
	size_t actual_space = sizeof(struct sk_stats_cache);
	struct sk_stats_cache *__single sca;
	int contention;

	if (out == NULL || len < actual_space) {
		goto done;
	}
	sca = out;

	bzero(sca, sizeof(*sca));
	(void) snprintf(sca->sca_name, sizeof(sca->sca_name), "%s",
	    skm->skm_name);
	uuid_copy(sca->sca_uuid, skm->skm_uuid);
	uuid_copy(sca->sca_ruuid, skm->skm_region->skr_uuid);
	sca->sca_mode = skm->skm_mode;
	sca->sca_bufsize = (uint64_t)skm->skm_bufsize;
	sca->sca_objsize = (uint64_t)skm->skm_objsize;
	sca->sca_chunksize = (uint64_t)skm->skm_chunksize;
	sca->sca_slabsize = (uint64_t)skm->skm_slabsize;
	sca->sca_bufalign = (uint64_t)skm->skm_bufalign;
	sca->sca_objalign = (uint64_t)skm->skm_objalign;

	sca->sca_cpu_mag_size = skm->skm_cpu_mag_size;
	sca->sca_cpu_mag_resize = skm->skm_cpu_mag_resize;
	sca->sca_cpu_mag_purge = skm->skm_cpu_mag_purge;
	sca->sca_cpu_mag_reap = skm->skm_cpu_mag_reap;
	sca->sca_depot_full = skm->skm_depot_full;
	sca->sca_depot_empty = skm->skm_depot_empty;
	sca->sca_depot_ws_zero = skm->skm_depot_ws_zero;
	/* in case of a race this might be a negative value, turn it into 0 */
	if ((contention = (int)(skm->skm_depot_contention -
	    skm->skm_depot_contention_prev)) < 0) {
		contention = 0;
	}
	sca->sca_depot_contention_factor = contention;

	sca->sca_cpu_rounds = 0;
	sca->sca_cpu_prounds = 0;
	for (int cpuid = 0; cpuid < ncpu; cpuid++) {
		struct skmem_cpu_cache *ccp = &skm->skm_cpu_cache[cpuid];

		SKM_CPU_LOCK(ccp);
		if (ccp->cp_rounds > -1) {
			sca->sca_cpu_rounds += ccp->cp_rounds;
		}
		if (ccp->cp_prounds > -1) {
			sca->sca_cpu_prounds += ccp->cp_prounds;
		}
		SKM_CPU_UNLOCK(ccp);
	}

	sca->sca_sl_create = skm->skm_sl_create;
	sca->sca_sl_destroy = skm->skm_sl_destroy;
	sca->sca_sl_alloc = skm->skm_sl_alloc;
	sca->sca_sl_free = skm->skm_sl_free;
	sca->sca_sl_alloc_fail = skm->skm_sl_alloc_fail;
	sca->sca_sl_partial = skm->skm_sl_partial;
	sca->sca_sl_empty = skm->skm_sl_empty;
	sca->sca_sl_bufinuse = skm->skm_sl_bufinuse;
	sca->sca_sl_rescale = skm->skm_sl_rescale;
	sca->sca_sl_hash_size = (skm->skm_hash_mask + 1);

done:
	return actual_space;
}

static int
skmem_cache_mib_get_sysctl SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	struct skmem_cache *skm;
	size_t actual_space;
	size_t buffer_space;
	size_t allocated_space = 0;
	caddr_t __sized_by(allocated_space) buffer = NULL;
	caddr_t scan;
	int error = 0;

	if (!kauth_cred_issuser(kauth_cred_get())) {
		return EPERM;
	}

	net_update_uptime();
	buffer_space = req->oldlen;
	if (req->oldptr != USER_ADDR_NULL && buffer_space != 0) {
		if (buffer_space > SK_SYSCTL_ALLOC_MAX) {
			buffer_space = SK_SYSCTL_ALLOC_MAX;
		}
		caddr_t temp;
		temp = sk_alloc_data(buffer_space, Z_WAITOK, skmem_tag_cache_mib);
		if (__improbable(temp == NULL)) {
			return ENOBUFS;
		}
		buffer = temp;
		allocated_space = buffer_space;
	} else if (req->oldptr == USER_ADDR_NULL) {
		buffer_space = 0;
	}
	actual_space = 0;
	scan = buffer;

	SKMEM_CACHE_LOCK();
	TAILQ_FOREACH(skm, &skmem_cache_head, skm_link) {
		size_t size = skmem_cache_mib_get_stats(skm, scan, buffer_space);
		if (scan != NULL) {
			if (buffer_space < size) {
				/* supplied buffer too small, stop copying */
				error = ENOMEM;
				break;
			}
			scan += size;
			buffer_space -= size;
		}
		actual_space += size;
	}
	SKMEM_CACHE_UNLOCK();

	if (actual_space != 0) {
		int out_error = SYSCTL_OUT(req, buffer, actual_space);
		if (out_error != 0) {
			error = out_error;
		}
	}
	if (buffer != NULL) {
		sk_free_data_sized_by(buffer, allocated_space);
	}

	return error;
}
