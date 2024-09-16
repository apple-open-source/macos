/*
 * Copyright (c) 2017-2020 Apple Inc. All rights reserved.
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

#include <kern/turnstile.h>
#include <kern/cpu_data.h>
#include <kern/mach_param.h>
#include <kern/kern_types.h>
#include <kern/assert.h>
#include <kern/kalloc.h>
#include <kern/thread.h>
#include <kern/clock.h>
#include <kern/policy_internal.h>
#include <kern/task.h>
#include <kern/waitq.h>
#include <kern/sched_prim.h>
#include <kern/zalloc.h>
#include <kern/debug.h>
#include <kern/compact_id.h>
#include <machine/limits.h>
#include <machine/atomic.h>

#include <pexpert/pexpert.h>
#include <os/hash.h>
#include <libkern/section_keywords.h>

static TUNABLE(int, turnstile_max_hop, "turnstile_max_hop", TURNSTILE_MAX_HOP_DEFAULT);
COMPACT_ID_TABLE_DEFINE(static, ctsid_table);
static_assert(CTSID_MAX <= COMPACT_ID_MAX);
static SECURITY_READ_ONLY_LATE(uint32_t) ctsid_nonce;

ZONE_DEFINE_ID(ZONE_ID_TURNSTILE, "turnstiles", struct turnstile,
    ZC_ZFREE_CLEARMEM);

/* Global table for turnstile promote policy for all type of turnstiles */
static const turnstile_promote_policy_t turnstile_promote_policy[TURNSTILE_TOTAL_TYPES] = {
	[TURNSTILE_NONE]          = TURNSTILE_PROMOTE_NONE,
	[TURNSTILE_KERNEL_MUTEX]  = TURNSTILE_KERNEL_PROMOTE,
	[TURNSTILE_ULOCK]         = TURNSTILE_USER_PROMOTE,
	[TURNSTILE_PTHREAD_MUTEX] = TURNSTILE_USER_PROMOTE,
	[TURNSTILE_SYNC_IPC]      = TURNSTILE_USER_IPC_PROMOTE,
	[TURNSTILE_WORKLOOPS]     = TURNSTILE_USER_IPC_PROMOTE,
	[TURNSTILE_WORKQS]        = TURNSTILE_USER_IPC_PROMOTE,
	[TURNSTILE_KNOTE]         = TURNSTILE_USER_IPC_PROMOTE,
	[TURNSTILE_SLEEP_INHERITOR] = TURNSTILE_KERNEL_PROMOTE,
	[TURNSTILE_EPOCH_KERNEL]  = TURNSTILE_KERNEL_PROMOTE,
	[TURNSTILE_EPOCH_USER]    = TURNSTILE_USER_PROMOTE,
};

/* Global table for turnstile hash lock policy for all type of turnstiles */
static const turnstile_hash_lock_policy_t turnstile_hash_lock_policy[TURNSTILE_TOTAL_TYPES] = {
	[TURNSTILE_NONE]          = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_KERNEL_MUTEX]  = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_ULOCK]         = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_PTHREAD_MUTEX] = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_SYNC_IPC]      = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_WORKLOOPS]     = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_WORKQS]        = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_KNOTE]         = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_SLEEP_INHERITOR] = (TURNSTILE_IRQ_UNSAFE_HASH | TURNSTILE_LOCKED_HASH),
	[TURNSTILE_EPOCH_KERNEL]  = TURNSTILE_HASH_LOCK_POLICY_NONE,
	[TURNSTILE_EPOCH_USER]    = TURNSTILE_HASH_LOCK_POLICY_NONE,
};

os_refgrp_decl(static, turnstile_refgrp, "turnstile", NULL);

#if DEVELOPMENT || DEBUG
static LCK_GRP_DECLARE(turnstiles_dev_lock_grp, "turnstile_dev_lock");

/* Array to store stats for multi-hop boosting */
static struct turnstile_stats turnstile_boost_stats[TURNSTILE_MAX_HOP_DEFAULT] = {};
static struct turnstile_stats turnstile_unboost_stats[TURNSTILE_MAX_HOP_DEFAULT] = {};
uint64_t thread_block_on_turnstile_count;
uint64_t thread_block_on_regular_waitq_count;
#endif /* DEVELOPMENT || DEBUG */

#ifndef max
#define max(a, b)        (((a) > (b)) ? (a) : (b))
#endif /* max */

/* Static function declarations */
static turnstile_type_t
turnstile_get_type(struct turnstile *turnstile);
static bool
turnstile_is_send_turnstile(struct turnstile *turnstile);
static uint32_t
turnstile_get_gencount(struct turnstile *turnstile);
static void
turnstile_set_type_and_increment_gencount(struct turnstile *turnstile, turnstile_type_t type);
static void
turnstile_destroy(struct turnstile *turnstile);
static void
turnstile_update_inheritor_workq_priority_chain(struct turnstile *in_turnstile, spl_t s);
static void
turnstile_update_inheritor_thread_priority_chain(struct turnstile **in_turnstile,
    thread_t *out_thread, int total_hop, turnstile_stats_update_flags_t tsu_flags);
static void
turnstile_update_inheritor_turnstile_priority_chain(struct turnstile **in_out_turnstile,
    int total_hop, turnstile_stats_update_flags_t tsu_flags);
static void
thread_update_waiting_turnstile_priority_chain(thread_t *in_thread,
    struct turnstile **out_turnstile, int thread_hop, int total_hop,
    turnstile_stats_update_flags_t tsu_flags);
static boolean_t
turnstile_update_turnstile_promotion_locked(struct turnstile *dst_turnstile,
    struct turnstile *src_turnstile);
static boolean_t
turnstile_update_turnstile_promotion(struct turnstile *dst_turnstile,
    struct turnstile *src_turnstile);
static boolean_t
turnstile_need_turnstile_promotion_update(struct turnstile *dst_turnstile,
    struct turnstile *src_turnstile);
static boolean_t
turnstile_add_turnstile_promotion(struct turnstile *dst_turnstile,
    struct turnstile *src_turnstile);
static boolean_t
turnstile_remove_turnstile_promotion(struct turnstile *dst_turnstile,
    struct turnstile *src_turnstile);
static boolean_t
turnstile_update_thread_promotion_locked(struct turnstile *dst_turnstile,
    thread_t thread);
static boolean_t
turnstile_need_thread_promotion_update(struct turnstile *dst_turnstile,
    thread_t thread);
static boolean_t
thread_add_turnstile_promotion(
	thread_t thread, struct turnstile *turnstile);
static boolean_t
thread_remove_turnstile_promotion(
	thread_t thread, struct turnstile *turnstile);
static boolean_t
thread_needs_turnstile_promotion_update(thread_t thread,
    struct turnstile *turnstile);
static boolean_t
thread_update_turnstile_promotion(
	thread_t thread, struct turnstile *turnstile);
static boolean_t
thread_update_turnstile_promotion_locked(
	thread_t thread, struct turnstile *turnstile);
static boolean_t
workq_add_turnstile_promotion(
	struct workqueue *wq_inheritor, struct turnstile *turnstile);
static turnstile_stats_update_flags_t
thread_get_update_flags_for_turnstile_propagation_stoppage(thread_t thread);
static turnstile_stats_update_flags_t
turnstile_get_update_flags_for_above_UI_pri_change(struct turnstile *turnstile);
static void turnstile_stash_inheritor(turnstile_inheritor_t new_inheritor,
    turnstile_update_flags_t flags);
static int turnstile_compute_thread_push(struct turnstile *turnstile, thread_t thread);

#if DEVELOPMENT || DEBUG
/* Test primitives and interfaces for testing turnstiles */
struct tstile_test_prim {
	struct turnstile *ttprim_turnstile;
	thread_t ttprim_owner;
	lck_spin_t ttprim_interlock;
	uint32_t tt_prim_waiters;
};

struct tstile_test_prim test_prim_ts_inline;
struct tstile_test_prim test_prim_global_htable;
struct tstile_test_prim test_prim_global_ts_kernel;
struct tstile_test_prim test_prim_global_ts_kernel_hash;

static void
tstile_test_prim_init(struct tstile_test_prim *test_prim_ptr);
#endif

union turnstile_type_gencount {
	uint32_t value;
	struct {
		uint32_t ts_type:(8 * sizeof(turnstile_type_t)),
		ts_gencount: (8 * (sizeof(uint32_t) - sizeof(turnstile_type_t)));
	};
};

static turnstile_type_t
turnstile_get_type(struct turnstile *turnstile)
{
	union turnstile_type_gencount type_and_gencount;

	type_and_gencount.value = atomic_load_explicit(&turnstile->ts_type_gencount, memory_order_relaxed);
	return (turnstile_type_t) type_and_gencount.ts_type;
}

/* Only safe to be called from stackshot context */
static bool
turnstile_is_send_turnstile(struct turnstile *turnstile)
{
	if (not_in_kdp) {
		panic("turnstile_is_send_turnstile() called outside of kernel debugger context");
	}

	if (turnstile_get_type(turnstile) == TURNSTILE_SYNC_IPC) {
		ipc_port_t port = (ipc_port_t) turnstile->ts_proprietor;

		return port_send_turnstile(port) == turnstile;
	}

	return false;
}

/* Only safe to be called from stackshot context */
static bool
turnstile_is_receive_turnstile(struct turnstile *turnstile)
{
	if (not_in_kdp) {
		panic("turnstile_is_receive_turnstile() called outside of kernel debugger context");
	}

	if (turnstile_get_type(turnstile) == TURNSTILE_SYNC_IPC) {
		ipc_port_t port = (ipc_port_t) turnstile->ts_proprietor;

		return *port_rcv_turnstile_address(port) == turnstile;
	}

	return false;
}

static uint32_t
turnstile_get_gencount(struct turnstile *turnstile)
{
	union turnstile_type_gencount type_and_gencount;

	type_and_gencount.value = atomic_load_explicit(&turnstile->ts_type_gencount, memory_order_relaxed);
	return (uint32_t) type_and_gencount.ts_gencount;
}

static void
turnstile_set_type_and_increment_gencount(struct turnstile *turnstile, turnstile_type_t type)
{
	union turnstile_type_gencount type_and_gencount;

	/* No need to compare exchange since the store happens under interlock of the primitive */
	type_and_gencount.value = atomic_load_explicit(&turnstile->ts_type_gencount, memory_order_relaxed);
	type_and_gencount.ts_type = type;
	type_and_gencount.ts_gencount++;
	atomic_store_explicit(&turnstile->ts_type_gencount, type_and_gencount.value, memory_order_relaxed);
}


/* Turnstile hashtable Implementation */

/*
 * Maximum number of buckets in the turnstile hashtable. This number affects the
 * performance of the hashtable since it determines the hash collision
 * rate. To experiment with the number of buckets in this hashtable use the
 * "ts_htable_buckets" boot-arg.
 */
#define TURNSTILE_HTABLE_BUCKETS_DEFAULT   32
#define TURNSTILE_HTABLE_BUCKETS_MAX       1024

SLIST_HEAD(turnstile_hashlist, turnstile);

struct turnstile_htable_bucket {
	lck_spin_t                    ts_ht_bucket_lock;
	struct turnstile_hashlist     ts_ht_bucket_list;
};

SECURITY_READ_ONLY_LATE(static uint32_t) ts_htable_buckets;
/* Global hashtable for turnstiles managed with interrupts disabled */
SECURITY_READ_ONLY_LATE(static struct turnstile_htable_bucket *)turnstile_htable_irq_safe;
/* Global hashtable for turnstiles managed with interrupts enabled */
SECURITY_READ_ONLY_LATE(static struct turnstile_htable_bucket *)turnstile_htable;


/* Bucket locks for turnstile hashtable */
LCK_GRP_DECLARE(turnstiles_htable_lock_grp, "turnstiles_htable_locks");

#define turnstile_bucket_lock_init(bucket) \
	lck_spin_init(&bucket->ts_ht_bucket_lock, &turnstiles_htable_lock_grp, LCK_ATTR_NULL)
#define turnstile_bucket_lock(bucket) \
	lck_spin_lock_grp(&bucket->ts_ht_bucket_lock, &turnstiles_htable_lock_grp)
#define turnstile_bucket_unlock(bucket) \
	lck_spin_unlock(&bucket->ts_ht_bucket_lock)

#define kdp_turnstile_bucket_is_locked(bucket) \
	kdp_lck_spin_is_acquired(&bucket->ts_ht_bucket_lock)

/*
 * Name: turnstiles_hashtable_init
 *
 * Description: Initializes the global turnstile hash table.
 *
 * Args:
 *   None
 *
 * Returns:
 *   None
 */
static void
turnstiles_hashtable_init(void)
{
	/* Initialize number of buckets in the hashtable */
	if (PE_parse_boot_argn("ts_htable_buckets", &ts_htable_buckets, sizeof(ts_htable_buckets)) != TRUE) {
		ts_htable_buckets = TURNSTILE_HTABLE_BUCKETS_DEFAULT;
	}

	assert(ts_htable_buckets <= TURNSTILE_HTABLE_BUCKETS_MAX);
	uint32_t ts_htable_size = ts_htable_buckets * sizeof(struct turnstile_htable_bucket);
	turnstile_htable_irq_safe = zalloc_permanent(ts_htable_size, ZALIGN_PTR);
	if (turnstile_htable_irq_safe == NULL) {
		panic("Turnstiles hash table memory allocation failed!");
	}

	turnstile_htable = zalloc_permanent(ts_htable_size, ZALIGN_PTR);
	if (turnstile_htable == NULL) {
		panic("Turnstiles hash table memory allocation failed!");
	}

	/* Initialize all the buckets of the hashtables */
	for (uint32_t i = 0; i < ts_htable_buckets; i++) {
		struct turnstile_htable_bucket *ts_bucket = &(turnstile_htable_irq_safe[i]);
		turnstile_bucket_lock_init(ts_bucket);
		SLIST_INIT(&ts_bucket->ts_ht_bucket_list);

		ts_bucket = &(turnstile_htable[i]);
		turnstile_bucket_lock_init(ts_bucket);
		SLIST_INIT(&ts_bucket->ts_ht_bucket_list);
	}
}

/*
 * Name: turnstile_freelist_empty
 *
 * Description: Checks if the turnstile's freelist is empty
 *              Should be called with the primitive IL held.
 *
 * Args:
 *   Arg1: turnstile
 *
 * Returns:
 *   true if freelist is empty; false otherwise
 */
static inline boolean_t
turnstile_freelist_empty(
	struct turnstile *ts)
{
	return SLIST_EMPTY(&ts->ts_free_turnstiles);
}


/*
 * Name: turnstile_freelist_insert
 *
 * Description: Inserts the turnstile into the freelist of another turnstile
 *              Should be called with the primitive IL held.
 *
 * Args:
 *   Arg1: primitive turnstile
 *   Arg2: turnstile to add to the freelist
 *
 * Returns:
 *   None
 */
static void
turnstile_freelist_insert(
	struct turnstile *dst_ts,
	struct turnstile *free_ts)
{
	assert(turnstile_get_type(dst_ts) == turnstile_get_type(free_ts));
	assert(dst_ts->ts_proprietor == free_ts->ts_proprietor);
	turnstile_state_add(free_ts, TURNSTILE_STATE_FREELIST);
	SLIST_INSERT_HEAD(&dst_ts->ts_free_turnstiles, free_ts, ts_free_elm);
}

/*
 * Name: turnstile_freelist_remove
 *
 * Description: Removes a turnstile from the freelist of a turnstile
 *              Should be called with the primitive IL held.
 *
 * Args:
 *   Arg1: primitive turnstile
 *
 * Returns:
 *   turnstile removed from the freelist
 */
static struct turnstile *
turnstile_freelist_remove(
	struct turnstile *ts)
{
	struct turnstile *ret_turnstile = TURNSTILE_NULL;
	assert(!SLIST_EMPTY(&ts->ts_free_turnstiles));
	ret_turnstile = SLIST_FIRST(&ts->ts_free_turnstiles);
	SLIST_REMOVE_HEAD(&ts->ts_free_turnstiles, ts_free_elm);
	assert(ret_turnstile != TURNSTILE_NULL);
	turnstile_state_remove(ret_turnstile, TURNSTILE_STATE_FREELIST);
	/* Need to initialize the list again, since head and elm are in union */
	SLIST_INIT(&ret_turnstile->ts_free_turnstiles);
	return ret_turnstile;
}

/*
 * Name: turnstile_hash
 *
 * Description: Calculates the hash bucket index for a given proprietor
 *
 * Args:
 *   Arg1: proprietor (key) for hashing
 *
 * Returns:
 *   hash table bucket index for provided proprietor
 */
static inline uint32_t
turnstile_hash(uintptr_t proprietor)
{
	uint32_t hash = os_hash_kernel_pointer((void *)proprietor);
	return hash & (ts_htable_buckets - 1);
}

static inline struct turnstile_htable_bucket *
turnstile_get_bucket(uint32_t index, turnstile_type_t type)
{
	struct turnstile_htable_bucket *ts_bucket;
	int hash_policy = turnstile_hash_lock_policy[type];

	if (hash_policy & TURNSTILE_IRQ_UNSAFE_HASH) {
		ts_bucket = &(turnstile_htable[index]);
	} else {
		ts_bucket = &(turnstile_htable_irq_safe[index]);
	}

	return ts_bucket;
}

/*
 * Name: turnstile_hash_bucket_lock
 *
 * Description: locks the spinlock associated with proprietor's bucket.
 *              if proprietor is specified the index for the hash will be
 *              recomputed and returned in index_proprietor,
 *              otherwise the value save in index_proprietor is used as index.
 *
 * Args:
 *   Arg1: proprietor (key) for hashing
 *   Arg2: index for proprietor in the hash
 *   Arg3: turnstile type
 *
 * Returns: old value of irq if irq were disabled before acquiring the lock.
 */
unsigned
turnstile_hash_bucket_lock(uintptr_t proprietor, uint32_t *index_proprietor, turnstile_type_t type)
{
	struct turnstile_htable_bucket *ts_bucket;
	int hash_policy = turnstile_hash_lock_policy[type];
	bool irq_safe = !(hash_policy & TURNSTILE_IRQ_UNSAFE_HASH);
	spl_t ret = 0;
	uint32_t index;

	/*
	 * If the proprietor is specified, the caller doesn't know
	 * the index in the hash, so compute it.
	 * Otherwise use the value of index provided.
	 */
	if (proprietor) {
		index = turnstile_hash(proprietor);
		*index_proprietor = index;
	} else {
		index = *index_proprietor;
	}

	ts_bucket = turnstile_get_bucket(index, type);

	if (irq_safe) {
		ret = splsched();
	}

	turnstile_bucket_lock(ts_bucket);

	return ret;
}

/*
 * Name: turnstile_hash_bucket_unlock
 *
 * Description: unlocks the spinlock associated with proprietor's bucket.
 *              if proprietor is specified the index for the hash will be
 *              recomputed and returned in index_proprietor,
 *              otherwise the value save in index_proprietor is used as index.
 *
 * Args:
 *   Arg1: proprietor (key) for hashing
 *   Arg2: index for proprietor in the hash
 *   Arg3: turnstile type
 *   Arg4: irq value returned by turnstile_hash_bucket_lock
 *
 */
void
turnstile_hash_bucket_unlock(uintptr_t proprietor, uint32_t *index_proprietor, turnstile_type_t type, unsigned s)
{
	struct turnstile_htable_bucket *ts_bucket;
	int hash_policy = turnstile_hash_lock_policy[type];
	bool irq_safe = !(hash_policy & TURNSTILE_IRQ_UNSAFE_HASH);
	uint32_t index;

	/*
	 * If the proprietor is specified, the caller doesn't know
	 * the index in the hash, so compute it.
	 * Otherwise use the value of index provided.
	 */
	if (proprietor) {
		index = turnstile_hash(proprietor);
		*index_proprietor = index;
	} else {
		index = *index_proprietor;
	}
	ts_bucket = turnstile_get_bucket(index, type);

	turnstile_bucket_unlock(ts_bucket);
	if (irq_safe) {
		splx(s);
	}
}

/*
 * Name: turnstile_htable_lookup_add
 *
 * Description: Lookup the proprietor in the global turnstile hash table.
 *              If an entry is present, add the new turnstile to the entry's freelist.
 *              Otherwise add the passed in turnstile for that proprietor.
 *              The routine assumes that the turnstile->proprietor does not change
 *              while the turnstile is in the global hash table.
 *
 * Args:
 *   Arg1: proprietor
 *   Arg2: new turnstile for primitive
 *   Arg3: turnstile_type_t type
 *
 * Returns:
 *   Previous turnstile for proprietor in the hash table
 */
static struct turnstile *
turnstile_htable_lookup_add(
	uintptr_t proprietor,
	struct turnstile *new_turnstile,
	turnstile_type_t type)
{
	uint32_t index = turnstile_hash(proprietor);
	assert(index < ts_htable_buckets);
	struct turnstile_htable_bucket *ts_bucket;
	int hash_policy = turnstile_hash_lock_policy[type];
	bool needs_lock = !(hash_policy & TURNSTILE_LOCKED_HASH);
	bool irq_safe = !(hash_policy & TURNSTILE_IRQ_UNSAFE_HASH);
	spl_t s;

	ts_bucket = turnstile_get_bucket(index, type);

	if (needs_lock) {
		if (irq_safe) {
			s = splsched();
		}
		turnstile_bucket_lock(ts_bucket);
	}

	struct turnstile *ts;

	SLIST_FOREACH(ts, &ts_bucket->ts_ht_bucket_list, ts_htable_link) {
		if (ts->ts_proprietor == proprietor) {
			/*
			 * Found an entry in the hashtable for this proprietor; add thread turnstile to freelist
			 * and return this turnstile
			 */
			if (needs_lock) {
				turnstile_bucket_unlock(ts_bucket);
				if (irq_safe) {
					splx(s);
				}
			}
			turnstile_freelist_insert(ts, new_turnstile);
			return ts;
		}
	}

	/* No entry for this proprietor; add the new turnstile in the hash table */
	SLIST_INSERT_HEAD(&ts_bucket->ts_ht_bucket_list, new_turnstile, ts_htable_link);
	turnstile_state_add(new_turnstile, TURNSTILE_STATE_HASHTABLE);
	if (needs_lock) {
		turnstile_bucket_unlock(ts_bucket);
		if (irq_safe) {
			splx(s);
		}
	}
	/* Since there was no previous entry for this proprietor, return TURNSTILE_NULL */
	return TURNSTILE_NULL;
}

/*
 * Name: turnstable_htable_lookup_remove
 *
 * Description: Lookup the proprietor in the global turnstile hash table.
 *              For the turnstile in the hash table, if the freelist has turnstiles on it
 *              return one of them from the freelist. Otherwise remove the turnstile from
 *              the hashtable and return that.
 *              The routine assumes that the turnstile->proprietor does not change
 *              while the turnstile is in the global hash table.
 *
 * Args:
 *   Arg1: proprietor
 *   Arg2: free turnstile to be returned
 *   Arg3: turnstile_type_t type
 *
 * Returns:
 *   turnstile for this proprietor in the hashtable after the removal
 */
static struct turnstile *
turnstable_htable_lookup_remove(
	uintptr_t proprietor,
	struct turnstile **free_turnstile,
	turnstile_type_t type)
{
	uint32_t index = turnstile_hash(proprietor);
	assert(index < ts_htable_buckets);
	struct turnstile_htable_bucket *ts_bucket;
	struct turnstile *ret_turnstile = TURNSTILE_NULL;
	int hash_policy = turnstile_hash_lock_policy[type];
	bool needs_lock = !(hash_policy & TURNSTILE_LOCKED_HASH);
	bool irq_safe = !(hash_policy & TURNSTILE_IRQ_UNSAFE_HASH);
	spl_t s;

	ts_bucket = turnstile_get_bucket(index, type);

	if (needs_lock) {
		if (irq_safe) {
			s = splsched();
		}
		turnstile_bucket_lock(ts_bucket);
	}

	struct turnstile *ts, **prev_tslink;
	/* Find the turnstile for the given proprietor in the hashtable */
	SLIST_FOREACH_PREVPTR(ts, prev_tslink, &ts_bucket->ts_ht_bucket_list, ts_htable_link) {
		if (ts->ts_proprietor == proprietor) {
			ret_turnstile = ts;
			break;
		}
	}
	assert(ret_turnstile != TURNSTILE_NULL);

	/* Check if the turnstile has any turnstiles on its freelist */
	if (turnstile_freelist_empty(ret_turnstile)) {
		/* No turnstiles on the freelist; remove the turnstile from the hashtable and mark it freed */
		*prev_tslink = SLIST_NEXT(ret_turnstile, ts_htable_link);
		turnstile_state_remove(ret_turnstile, TURNSTILE_STATE_HASHTABLE);
		if (needs_lock) {
			turnstile_bucket_unlock(ts_bucket);
			if (irq_safe) {
				splx(s);
			}
		}
		*free_turnstile = ret_turnstile;
		return TURNSTILE_NULL;
	} else {
		/*
		 * Turnstile has free turnstiles on its list; leave the hashtable unchanged
		 * and return the first turnstile in the freelist as the free turnstile
		 */
		if (needs_lock) {
			turnstile_bucket_unlock(ts_bucket);
			if (irq_safe) {
				splx(s);
			}
		}
		*free_turnstile = turnstile_freelist_remove(ret_turnstile);
		return ret_turnstile;
	}
}

/*
 * Name: turnstile_htable_lookup
 *
 * Description: Lookup the proprietor in the global turnstile hash table.
 *              The routine assumes that the turnstile->proprietor does not change
 *              while the turnstile is in the global hash table.
 *
 * Args:
 *   Arg1: proprietor
 *   Arg2: turnstile_type_t type
 *
 * Returns:
 *   Turnstile for proprietor in the hash table
 */
static struct turnstile *
turnstile_htable_lookup(
	uintptr_t proprietor,
	turnstile_type_t type)
{
	uint32_t index = turnstile_hash(proprietor);
	assert(index < ts_htable_buckets);
	bool kdp_ctx = !not_in_kdp;
	struct turnstile_htable_bucket *ts_bucket = turnstile_get_bucket(index, type);
	int hash_policy = turnstile_hash_lock_policy[type];
	bool needs_lock = !(hash_policy & TURNSTILE_LOCKED_HASH);
	bool irq_safe = !(hash_policy & TURNSTILE_IRQ_UNSAFE_HASH);
	spl_t s;

	if (needs_lock) {
		if (irq_safe && !kdp_ctx) {
			s = splsched();
		}

		if (kdp_ctx) {
			if (kdp_turnstile_bucket_is_locked(ts_bucket)) {
				/* This should move to TURNSTILE_BUSY once 51725781 is in the build */
				return TURNSTILE_NULL;
			}
		} else {
			turnstile_bucket_lock(ts_bucket);
		}
	}
	struct turnstile *ts = TURNSTILE_NULL;
	struct turnstile *ret_turnstile = TURNSTILE_NULL;

	SLIST_FOREACH(ts, &ts_bucket->ts_ht_bucket_list, ts_htable_link) {
		if (ts->ts_proprietor == proprietor) {
			/* Found an entry in the hashtable for this proprietor */
			ret_turnstile = ts;
			break;
		}
	}

	if (needs_lock && !kdp_ctx) {
		turnstile_bucket_unlock(ts_bucket);
		if (irq_safe) {
			splx(s);
		}
	}

	return ret_turnstile;
}

/*
 * Name: turnstiles_init
 *
 * Description: Initialize turnstile sub system.
 *
 * Args: None.
 *
 * Returns: None.
 */
void
turnstiles_init(void)
{
	ctsid_nonce = (uint32_t)early_random() & CTSID_MASK;
	turnstiles_hashtable_init();

#if DEVELOPMENT || DEBUG
	/* Initialize turnstile test primitive */
	tstile_test_prim_init(&test_prim_ts_inline);
	tstile_test_prim_init(&test_prim_global_htable);
	tstile_test_prim_init(&test_prim_global_ts_kernel);
	tstile_test_prim_init(&test_prim_global_ts_kernel_hash);
#endif
}

/*
 * Name: turnstile_alloc
 *
 * Description: Allocate a turnstile.
 *
 * Args: None.
 *
 * Returns:
 *   turnstile on Success.
 */
struct turnstile *
turnstile_alloc(void)
{
	struct turnstile *turnstile = TURNSTILE_NULL;

	turnstile = zalloc_id(ZONE_ID_TURNSTILE, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	/* Initialize the waitq */
	waitq_init(&turnstile->ts_waitq, WQT_TURNSTILE,
	    SYNC_POLICY_REVERSED | SYNC_POLICY_INIT_LOCKED);

	SLIST_INIT(&turnstile->ts_free_turnstiles);

	turnstile_set_type_and_increment_gencount(turnstile, TURNSTILE_NONE);
	turnstile_state_init(turnstile, TURNSTILE_STATE_THREAD);
	os_ref_init_raw(&turnstile->ts_refcount, &turnstile_refgrp);
	priority_queue_init(&turnstile->ts_inheritor_queue);

#if DEVELOPMENT || DEBUG
	turnstile->ts_thread = current_thread();
#endif

	waitq_unlock(&turnstile->ts_waitq);

	return turnstile;
}

/*
 * Name: turnstile_reference
 *
 * Description: Take a reference on the turnstile.
 *
 * Arg1: turnstile
 *
 * Returns: None.
 */
void
turnstile_reference(struct turnstile *turnstile)
{
	if (turnstile == TURNSTILE_NULL) {
		return;
	}

	zone_id_require(ZONE_ID_TURNSTILE, sizeof(struct turnstile), turnstile);

	os_ref_retain_raw(&turnstile->ts_refcount, &turnstile_refgrp);
}

/*
 * Name: turnstile_deallocate
 *
 * Description: Drop a reference on the turnstile.
 *              Destroy the turnstile if the last ref.
 *
 * Arg1: turnstile
 *
 * Returns: None.
 */
void
turnstile_deallocate(struct turnstile *turnstile)
{
	if (turnstile == TURNSTILE_NULL) {
		return;
	}

	if (__improbable(os_ref_release_raw(&turnstile->ts_refcount,
	    &turnstile_refgrp) == 0)) {
		assert(SLIST_EMPTY(&turnstile->ts_free_turnstiles));
		turnstile_destroy(turnstile);
	}
}

/*
 * Name: turnstile_destroy
 *
 * Description: Deallocates the turnstile.
 *
 * Args:
 *   Arg1: turnstile
 *
 * Returns: None.
 */
static void
turnstile_destroy(struct turnstile *turnstile)
{
	assert3u(turnstile->ts_compact_id, ==, 0);
	assert3u(turnstile->ts_prim_count, ==, 0);
	assert3p(turnstile->ts_inheritor, ==, TURNSTILE_INHERITOR_NULL);
	assert3u(turnstile->ts_state, ==, TURNSTILE_STATE_THREAD);

	waitq_deinit(&turnstile->ts_waitq);

	zfree_id(ZONE_ID_TURNSTILE, turnstile);
}

static uint32_t
ctsid_mangle(compact_id_t cid)
{
	return (cid == ctsid_nonce ? CTSID_MASK : cid) ^ ctsid_nonce;
}

static compact_id_t
ctsid_unmangle(uint32_t ctsid)
{
	ctsid ^= ctsid_nonce;
	return ctsid == CTSID_MASK ? ctsid_nonce : ctsid;
}

uint32_t
turnstile_compact_id_get(void)
{
	compact_id_t cid;

	cid = compact_id_get(&ctsid_table, CTSID_MAX, NULL);
	return ctsid_mangle(cid);
}

void
turnstile_compact_id_put(uint32_t ctsid)
{
	assert3u(ctsid, !=, 0);
	compact_id_put(&ctsid_table, ctsid_unmangle(ctsid));
}

/*
 * Name: turnstile_get_by_id
 *
 * Description: Resolve a turnstile by compact ID
 *
 * Args:
 *   Arg1: turnstile compact ID
 *
 * Returns: a turnstile
 */
struct turnstile *
turnstile_get_by_id(uint32_t tsid)
{
	struct turnstile *ts = TURNSTILE_NULL;

	if (tsid) {
		ts = *compact_id_resolve(&ctsid_table, ctsid_unmangle(tsid));
		assert(ts && ts->ts_compact_id == tsid);
	}
	return ts;
}

/*
 * Name: turnstile_prepare variants
 *
 * Description: Transfer current thread's turnstile to primitive or it's free turnstile list.
 *              Function is called holding the interlock (spinlock) of the primitive.
 */
static struct turnstile *
turnstile_prepare_common(
	uintptr_t              proprietor,
	thread_t               thread,
	struct turnstile      *incoming,
	turnstile_type_t       type)
{
	struct turnstile *thread_turnstile = incoming;

	/* Get the thread's turnstile if no turnstile provided */
	if (thread_turnstile == TURNSTILE_NULL) {
		thread_turnstile = thread->turnstile;
		assert(thread_turnstile != TURNSTILE_NULL);
		assert(thread->inheritor == NULL);
		thread->turnstile = TURNSTILE_NULL;
	}

	/* Prepare the thread turnstile to be the primitive turnstile */
	SLIST_INIT(&thread_turnstile->ts_free_turnstiles);
	turnstile_set_type_and_increment_gencount(thread_turnstile, type);
	thread_turnstile->ts_inheritor = TURNSTILE_INHERITOR_NULL;
	thread_turnstile->ts_proprietor = proprietor;
	turnstile_state_remove(thread_turnstile, TURNSTILE_STATE_THREAD);

	thread_turnstile->ts_priority = 0;
#if DEVELOPMENT || DEBUG
	thread_turnstile->ts_prev_thread = thread_turnstile->ts_thread;
	thread_turnstile->ts_thread = NULL;
#endif

	return thread_turnstile;
}

struct turnstile *
turnstile_prepare(
	uintptr_t             proprietor,
	struct turnstile    **tstore,
	struct turnstile     *turnstile,
	turnstile_type_t      type)
{
	thread_t thread = current_thread();
	struct turnstile *ret_turnstile = TURNSTILE_NULL;
	struct turnstile *thread_turnstile;

	thread_turnstile = turnstile_prepare_common(proprietor, thread,
	    turnstile, type);

	/*
	 * If the primitive stores the turnstile,
	 * If there is already a turnstile, put the thread_turnstile if the primitive currently does not have a
	 * turnstile.
	 * Else, add the thread turnstile to freelist of the primitive turnstile.
	 */
	ret_turnstile = *tstore;
	if (*tstore == TURNSTILE_NULL) {
		turnstile_state_add(thread_turnstile, TURNSTILE_STATE_PROPRIETOR);
		*tstore = thread_turnstile;
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_FREELIST_OPERATIONS, (TURNSTILE_PREPARE))) | DBG_FUNC_NONE,
		    VM_KERNEL_UNSLIDE_OR_PERM(thread_turnstile),
		    VM_KERNEL_UNSLIDE_OR_PERM(proprietor),
		    turnstile_get_type(thread_turnstile), 0, 0);
	} else {
		turnstile_freelist_insert(ret_turnstile, thread_turnstile);
	}
	ret_turnstile = *tstore;

	return ret_turnstile;
}

struct turnstile *
turnstile_prepare_compact_id(
	uintptr_t             proprietor,
	uint32_t              compact_id,
	turnstile_type_t      type)
{
	thread_t thread = current_thread();
	struct turnstile *ret_turnstile = TURNSTILE_NULL;
	struct turnstile *thread_turnstile;
	uint32_t ctsid = thread->ctsid;

	thread_turnstile = turnstile_prepare_common(proprietor, thread,
	    TURNSTILE_NULL, type);

	assert3u(ctsid, !=, 0);
	thread_turnstile->ts_compact_id = ctsid;
	thread->ctsid = 0;

	/*
	 * If the primitive stores the turnstile,
	 * If there is already a turnstile, put the thread_turnstile if the primitive currently does not have a
	 * turnstile.
	 * Else, add the thread turnstile to freelist of the primitive turnstile.
	 */
	if (compact_id == 0) {
		turnstile_state_add(thread_turnstile, TURNSTILE_STATE_PROPRIETOR);
		ret_turnstile = thread_turnstile;
		*compact_id_resolve(&ctsid_table, ctsid_unmangle(ctsid)) = ret_turnstile;
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_FREELIST_OPERATIONS, (TURNSTILE_PREPARE))) | DBG_FUNC_NONE,
		    VM_KERNEL_UNSLIDE_OR_PERM(thread_turnstile),
		    VM_KERNEL_UNSLIDE_OR_PERM(proprietor),
		    turnstile_get_type(thread_turnstile), 0, 0);
	} else {
		ret_turnstile = turnstile_get_by_id(compact_id);
		turnstile_freelist_insert(ret_turnstile, thread_turnstile);
	}

	return ret_turnstile;
}

struct turnstile *
turnstile_prepare_hash(
	uintptr_t             proprietor,
	turnstile_type_t      type)
{
	thread_t thread = current_thread();
	struct turnstile *ret_turnstile = TURNSTILE_NULL;
	struct turnstile *thread_turnstile;

	thread_turnstile = turnstile_prepare_common(proprietor, thread,
	    TURNSTILE_NULL, type);

	/*
	 * Lookup the primitive in the turnstile hash table and see if it already has an entry.
	 */
	ret_turnstile = turnstile_htable_lookup_add(proprietor, thread_turnstile, type);
	if (ret_turnstile == NULL) {
		ret_turnstile = thread_turnstile;
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_FREELIST_OPERATIONS, (TURNSTILE_PREPARE))) | DBG_FUNC_NONE,
		    VM_KERNEL_UNSLIDE_OR_PERM(thread_turnstile),
		    VM_KERNEL_UNSLIDE_OR_PERM(proprietor),
		    turnstile_get_type(thread_turnstile), 0, 0);
	}

	return ret_turnstile;
}

/*
 * Name: turnstile_complete variants
 *
 * Description: Transfer the primitive's turnstile or from it's freelist to current thread.
 *              Current thread will have a turnstile attached to it after this call.
 */
static void
turnstile_complete_common(
	uintptr_t             proprietor __kdebug_only,
	thread_t              thread,
	struct turnstile     *thread_turnstile,
	struct turnstile     *primitive_turnstile,
	struct turnstile    **out_turnstile)
{
	if (primitive_turnstile == NULL) {
		/*
		 * Primitive no longer has a turnstile associated with it, thread_turnstile
		 * was the last turnstile attached to primitive, clear out the inheritor and
		 * set the old inheritor for turnstile cleanup.
		 */
		if (thread_turnstile->ts_inheritor != TURNSTILE_INHERITOR_NULL) {
			turnstile_update_inheritor(thread_turnstile, TURNSTILE_INHERITOR_NULL,
			    (TURNSTILE_IMMEDIATE_UPDATE | TURNSTILE_INHERITOR_THREAD));
			/*
			 * old inheritor is set in curret thread and its priority propagation
			 * will happen in turnstile cleanup call
			 */
		}
		thread_turnstile->ts_proprietor = TURNSTILE_PROPRIETOR_NULL;
		assert(thread_turnstile->ts_inheritor == TURNSTILE_INHERITOR_NULL);

		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_FREELIST_OPERATIONS, (TURNSTILE_COMPLETE))) | DBG_FUNC_NONE,
		    VM_KERNEL_UNSLIDE_OR_PERM(thread_turnstile),
		    VM_KERNEL_UNSLIDE_OR_PERM(proprietor),
		    turnstile_get_type(thread_turnstile), 0, 0);
	} else {
		/* If primitive's turnstile needs priority update, set it up for turnstile cleanup */
		if (turnstile_recompute_priority(primitive_turnstile)) {
			turnstile_reference(primitive_turnstile);
			thread->inheritor = primitive_turnstile;
			thread->inheritor_flags = (TURNSTILE_INHERITOR_TURNSTILE |
			    TURNSTILE_INHERITOR_NEEDS_PRI_UPDATE);
		}
	}

	turnstile_set_type_and_increment_gencount(thread_turnstile, TURNSTILE_NONE);
#if DEVELOPMENT || DEBUG
	thread_turnstile->ts_prev_thread = NULL;
	thread_turnstile->ts_thread = thread;
#endif

	turnstile_state_add(thread_turnstile, TURNSTILE_STATE_THREAD);
	if (out_turnstile == NULL) {
		/* Prepare the turnstile to become the thread's turnstile */
		thread->turnstile = thread_turnstile;
	} else {
		*out_turnstile = thread_turnstile;
	}
}

void
turnstile_complete(
	uintptr_t             proprietor,
	struct turnstile    **tstore,
	struct turnstile    **out_turnstile,
	turnstile_type_t      type __unused)
{
	thread_t thread = current_thread();
	struct turnstile *primitive_turnstile = TURNSTILE_NULL;
	struct turnstile *thread_turnstile = TURNSTILE_NULL;

	assert(thread->inheritor == NULL);

	/*
	 * If the primitive stores the turnstile, check if the primitive turnstile
	 * has any turnstiles on its freelist.
	 */
	assert(*tstore != TURNSTILE_NULL);
	if (turnstile_freelist_empty(*tstore)) {
		/* Last turnstile scenario; remove the primitive->turnstile */
		thread_turnstile = *tstore;
		*tstore = TURNSTILE_NULL;
		turnstile_state_remove(thread_turnstile, TURNSTILE_STATE_PROPRIETOR);
	} else {
		/* Freelist has turnstiles; remove one from the freelist */
		thread_turnstile = turnstile_freelist_remove(*tstore);
	}
	primitive_turnstile = *tstore;

	turnstile_complete_common(proprietor, thread, thread_turnstile,
	    primitive_turnstile, out_turnstile);
}

bool
turnstile_complete_compact_id(
	uintptr_t             proprietor,
	struct turnstile     *primitive_turnstile,
	turnstile_type_t      type __unused)
{
	thread_t thread = current_thread();
	struct turnstile *thread_turnstile = TURNSTILE_NULL;
	bool was_last = turnstile_freelist_empty(primitive_turnstile);
	uint32_t ctsid;

	assert(thread->inheritor == NULL);

	assert(primitive_turnstile != TURNSTILE_NULL);
	if (was_last) {
		/* Last turnstile scenario; remove the primitive->turnstile */
		thread_turnstile = primitive_turnstile;
		turnstile_state_remove(thread_turnstile, TURNSTILE_STATE_PROPRIETOR);
		primitive_turnstile = TURNSTILE_NULL;
	} else {
		/* Freelist has turnstiles; remove one from the freelist */
		thread_turnstile = turnstile_freelist_remove(primitive_turnstile);
	}

	ctsid = thread_turnstile->ts_compact_id;
	assert3u(ctsid, !=, 0);
	thread_turnstile->ts_compact_id = 0;
	thread->ctsid = ctsid;
	if (was_last) {
		*compact_id_resolve(&ctsid_table, ctsid_unmangle(ctsid)) = NULL;
	}

	turnstile_complete_common(proprietor, thread, thread_turnstile,
	    primitive_turnstile, NULL);

	return was_last;
}

void
turnstile_complete_hash(
	uintptr_t             proprietor,
	turnstile_type_t      type)
{
	thread_t thread = current_thread();
	struct turnstile *primitive_turnstile = TURNSTILE_NULL;
	struct turnstile *thread_turnstile = TURNSTILE_NULL;

	assert(thread->inheritor == NULL);

	/* Use the global hash to find and remove a turnstile */
	primitive_turnstile = turnstable_htable_lookup_remove(proprietor, &thread_turnstile, type);

	turnstile_complete_common(proprietor, thread, thread_turnstile,
	    primitive_turnstile, NULL);
}

/*
 * Name: turnstile_kernel_update_inheritor_on_wake_locked
 *
 * Description: Set thread as the inheritor of the turnstile and
 *		boost the inheritor.
 * Args:
 *   Arg1: turnstile
 *   Arg2: new_inheritor
 *   Arg3: flags
 *
 * Called with turnstile locked
 */
void
turnstile_kernel_update_inheritor_on_wake_locked(
	struct turnstile *turnstile,
	turnstile_inheritor_t new_inheritor,
	turnstile_update_flags_t flags __assert_only)
{
	/* for now only kernel primitives are allowed to call this function */
	__assert_only turnstile_promote_policy_t policy =
	    turnstile_promote_policy[turnstile_get_type(turnstile)];

	assert(flags & TURNSTILE_INHERITOR_THREAD);
	assert(policy == TURNSTILE_KERNEL_PROMOTE || policy == TURNSTILE_USER_PROMOTE);

	turnstile_stash_inheritor((thread_t)new_inheritor, TURNSTILE_INHERITOR_THREAD);
	/*
	 * new_inheritor has just been removed from the turnstile waitq,
	 * the turnstile new priority needs to be recomputed so that
	 * when new_inheritor will become this turnstile inheritor can
	 * inherit the correct priority.
	 */
	turnstile_recompute_priority_locked(turnstile);
	turnstile_update_inheritor_locked(turnstile);
}

/*
 * Name: turnstile_update_inheritor_locked
 *
 * Description: Update the inheritor of the turnstile and boost the
 *              inheritor, called with turnstile locked.
 *
 * Args:
 *   Arg1: turnstile
 *   Implicit arg: new inheritor value is stashed in current thread's struct
 *
 * Returns:
 *   old inheritor reference is returned on current thread's struct.
 */
void
turnstile_update_inheritor_locked(
	struct turnstile *turnstile)
{
	turnstile_inheritor_t old_inheritor = turnstile->ts_inheritor;
	turnstile_update_flags_t old_inheritor_flags = turnstile->ts_inheritor_flags;
	thread_t thread = current_thread();
	boolean_t old_inheritor_needs_update = FALSE;
	boolean_t new_inheritor_needs_update = FALSE;
	turnstile_stats_update_flags_t tsu_flags =
	    turnstile_get_update_flags_for_above_UI_pri_change(turnstile);

	assert(waitq_held(&turnstile->ts_waitq));

	/*
	 * Get the new inheritor value from current thread's
	 * struct, the value was stashed by turnstile_update_inheritor
	 */
	turnstile_inheritor_t new_inheritor = thread->inheritor;
	turnstile_update_flags_t new_inheritor_flags = thread->inheritor_flags;

	switch (turnstile_promote_policy[turnstile_get_type(turnstile)]) {
	case TURNSTILE_USER_PROMOTE:
	case TURNSTILE_USER_IPC_PROMOTE:
		break;
	case TURNSTILE_KERNEL_PROMOTE:
		/* some sanity checks, turnstile kernel can push just between threads */
		if (old_inheritor) {
			assert(old_inheritor_flags & TURNSTILE_INHERITOR_THREAD);
		}

		if (new_inheritor) {
			assert(new_inheritor_flags & TURNSTILE_INHERITOR_THREAD);
		}

		break;
	default:
		panic("turnstile promotion for type %d not yet implemented", turnstile_get_type(turnstile));
	}

	/* Check if update is needed */
	if (old_inheritor == new_inheritor && old_inheritor == NULL) {
		goto done;
	}

	if (old_inheritor == new_inheritor) {
		if (new_inheritor_flags & TURNSTILE_INHERITOR_THREAD) {
			thread_t thread_inheritor = (thread_t)new_inheritor;

			assert(old_inheritor_flags & TURNSTILE_INHERITOR_THREAD);

			/* adjust turnstile position in the thread's inheritor list */
			new_inheritor_needs_update = thread_update_turnstile_promotion(
				thread_inheritor, turnstile);
		} else if (new_inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE) {
			struct turnstile *inheritor_turnstile = new_inheritor;

			assert(old_inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE);

			new_inheritor_needs_update = turnstile_update_turnstile_promotion(
				inheritor_turnstile, turnstile);
		} else if (new_inheritor_flags & TURNSTILE_INHERITOR_WORKQ) {
			/*
			 * When we are still picking "WORKQ" then possible racing
			 * updates will call redrive through their own propagation
			 * and we don't need to update anything here.
			 */
			turnstile_stats_update(1, TSU_NO_PRI_CHANGE_NEEDED |
			    TSU_TURNSTILE_ARG | TSU_BOOST_ARG, turnstile);
		} else {
			panic("Inheritor flags lost along the way");
		}

		/* Update turnstile stats */
		if (!new_inheritor_needs_update) {
			turnstile_stats_update(1, TSU_PRI_PROPAGATION |
			    TSU_TURNSTILE_ARG | TSU_BOOST_ARG | tsu_flags, turnstile);
		}
		goto done;
	}

	if (old_inheritor != NULL) {
		if (old_inheritor_flags & TURNSTILE_INHERITOR_THREAD) {
			thread_t thread_inheritor = (thread_t)old_inheritor;

			/* remove turnstile from thread's inheritor list */
			old_inheritor_needs_update = thread_remove_turnstile_promotion(thread_inheritor, turnstile);
		} else if (old_inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE) {
			struct turnstile *old_turnstile = old_inheritor;

			old_inheritor_needs_update = turnstile_remove_turnstile_promotion(
				old_turnstile, turnstile);
		} else if (old_inheritor_flags & TURNSTILE_INHERITOR_WORKQ) {
			/*
			 * We don't need to do anything when the push was WORKQ
			 * because nothing is pushed on in the first place.
			 */
			turnstile_stats_update(1, TSU_NO_PRI_CHANGE_NEEDED |
			    TSU_TURNSTILE_ARG, turnstile);
		} else {
			panic("Inheritor flags lost along the way");
		}
		/* Update turnstile stats */
		if (!old_inheritor_needs_update) {
			turnstile_stats_update(1, TSU_PRI_PROPAGATION | TSU_TURNSTILE_ARG,
			    turnstile);
		}
	}

	if (new_inheritor != NULL) {
		if (new_inheritor_flags & TURNSTILE_INHERITOR_THREAD) {
			thread_t thread_inheritor = (thread_t)new_inheritor;

			assert(new_inheritor_flags & TURNSTILE_INHERITOR_THREAD);
			/* add turnstile to thread's inheritor list */
			new_inheritor_needs_update = thread_add_turnstile_promotion(
				thread_inheritor, turnstile);
		} else if (new_inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE) {
			struct turnstile *new_turnstile = new_inheritor;

			new_inheritor_needs_update = turnstile_add_turnstile_promotion(
				new_turnstile, turnstile);
		} else if (new_inheritor_flags & TURNSTILE_INHERITOR_WORKQ) {
			struct workqueue *wq_inheritor = new_inheritor;

			new_inheritor_needs_update = workq_add_turnstile_promotion(
				wq_inheritor, turnstile);
			if (!new_inheritor_needs_update) {
				turnstile_stats_update(1, TSU_NO_PRI_CHANGE_NEEDED |
				    TSU_TURNSTILE_ARG | TSU_BOOST_ARG, turnstile);
			}
		} else {
			panic("Inheritor flags lost along the way");
		}
		/* Update turnstile stats */
		if (!new_inheritor_needs_update) {
			turnstile_stats_update(1, TSU_PRI_PROPAGATION |
			    TSU_TURNSTILE_ARG | TSU_BOOST_ARG | tsu_flags, turnstile);
		}
	}

done:
	if (old_inheritor_needs_update) {
		old_inheritor_flags |= TURNSTILE_INHERITOR_NEEDS_PRI_UPDATE;
	}

	/*
	 * If new inheritor needs priority updated, then set TURNSTILE_NEEDS_PRI_UPDATE
	 * on the old_inheritor_flags which will be copied to the thread.
	 */
	if (new_inheritor_needs_update) {
		old_inheritor_flags |= TURNSTILE_NEEDS_PRI_UPDATE;
	}

	turnstile->ts_inheritor = new_inheritor;
	turnstile->ts_inheritor_flags = new_inheritor_flags;
	thread->inheritor = old_inheritor;
	thread->inheritor_flags = old_inheritor_flags;
	return;
}

/*
 * Name: turnstile_stash_inheritor
 *
 * Description: Save the new inheritor reference of the turnstile on the
 *              current thread. It will take a thread reference on the inheritor.
 *              Called with the interlock of the primitive held.
 *
 * Args:
 *   Arg1: inheritor
 *   Arg2: flags
 *
 * Returns:
 *   old inheritor reference is stashed on current thread's struct.
 */
static void
turnstile_stash_inheritor(
	turnstile_inheritor_t new_inheritor,
	turnstile_update_flags_t flags)
{
	thread_t thread = current_thread();

	/*
	 * Set the inheritor on calling thread struct, no need
	 * to take the turnstile waitq lock since the inheritor
	 * is protected by the primitive's interlock
	 */
	assert(thread->inheritor == TURNSTILE_INHERITOR_NULL);
	thread->inheritor = new_inheritor;
	thread->inheritor_flags = TURNSTILE_UPDATE_FLAGS_NONE;
	if (new_inheritor == TURNSTILE_INHERITOR_NULL) {
		/* nothing to retain or remember */
	} else if (flags & TURNSTILE_INHERITOR_THREAD) {
		thread->inheritor_flags |= TURNSTILE_INHERITOR_THREAD;
		thread_reference((thread_t)new_inheritor);
	} else if (flags & TURNSTILE_INHERITOR_TURNSTILE) {
		thread->inheritor_flags |= TURNSTILE_INHERITOR_TURNSTILE;
		turnstile_reference((struct turnstile *)new_inheritor);
	} else if (flags & TURNSTILE_INHERITOR_WORKQ) {
		thread->inheritor_flags |= TURNSTILE_INHERITOR_WORKQ;
		workq_reference((struct workqueue *)new_inheritor);
	} else {
		panic("Missing type in flags (%x) for inheritor (%p)", flags,
		    new_inheritor);
	}
}

/*
 * Name: turnstile_update_inheritor
 *
 * Description: Update the inheritor of the turnstile and boost the
 *              inheritor. It will take a thread reference on the inheritor.
 *              Called with the interlock of the primitive held.
 *
 * Args:
 *   Arg1: turnstile
 *   Arg2: inheritor
 *   Arg3: flags - TURNSTILE_DELAYED_UPDATE - update will happen later in assert_wait
 *
 * Returns:
 *   old inheritor reference is stashed on current thread's struct.
 */
void
turnstile_update_inheritor(
	struct turnstile *turnstile,
	turnstile_inheritor_t new_inheritor,
	turnstile_update_flags_t flags)
{
	spl_t spl;

	turnstile_stash_inheritor(new_inheritor, flags);

	/* Do not perform the update if delayed update is specified */
	if (flags & TURNSTILE_DELAYED_UPDATE) {
		return;
	}

	/* lock the turnstile waitq */
	spl = splsched();
	waitq_lock(&turnstile->ts_waitq);

	turnstile_update_inheritor_locked(turnstile);

	waitq_unlock(&turnstile->ts_waitq);
	splx(spl);

	return;
}


/*
 * Name: turnstile_need_thread_promotion_update
 *
 * Description: Check if thread's place in the turnstile waitq needs to be updated.
 *
 * Arg1: dst turnstile
 * Arg2: thread
 *
 * Returns: TRUE: if turnstile_update_thread_promotion_locked needs to be called.
 *          FALSE: otherwise.
 *
 * Condition: thread locked.
 */
static boolean_t
turnstile_need_thread_promotion_update(
	struct turnstile *dst_turnstile,
	thread_t thread)
{
	int thread_link_priority;
	boolean_t needs_update = FALSE;

	thread_link_priority = priority_queue_entry_sched_pri(
		&dst_turnstile->ts_waitq.waitq_prio_queue,
		&thread->wait_prioq_links);

	int priority = turnstile_compute_thread_push(dst_turnstile, thread);

	needs_update = (thread_link_priority == priority) ? FALSE : TRUE;

	return needs_update;
}

/*
 * Name: turnstile_priority_queue_update_entry_key
 *
 * Description: Updates the priority of an entry in a priority queue
 *
 * Arg1: a turnstile/thread/... priority queue
 * Arg2: the element to change the priority of
 * Arg3: the new priority
 *
 * Returns: whether the maximum priority of the queue changed.
 */
static boolean_t
turnstile_priority_queue_update_entry_key(struct priority_queue_sched_max *q,
    priority_queue_entry_sched_t elt, priority_queue_key_t pri)
{
	priority_queue_key_t old_key = priority_queue_max_sched_pri(q);

	if (priority_queue_entry_sched_pri(q, elt) < pri) {
		priority_queue_entry_set_sched_pri(q, elt, pri, false);
		if (priority_queue_entry_increased(q, elt)) {
			return old_key != priority_queue_max_sched_pri(q);
		}
	} else if (priority_queue_entry_sched_pri(q, elt) > pri) {
		priority_queue_entry_set_sched_pri(q, elt, pri, false);
		if (priority_queue_entry_decreased(q, elt)) {
			return old_key != priority_queue_max_sched_pri(q);
		}
	}

	return FALSE;
}

/*
 * Name: turnstile_update_thread_promotion_locked
 *
 * Description: Update dst turnstile's inheritor link since one of the waiting
 *              thread's priority has changed.
 *
 * Arg1: dst turnstile
 * Arg2: thread
 *
 * Returns: TRUE: if the dst turnstile priority has changed and needs propagation.
 *          FALSE: if the dst turnstile priority did not change or it does not need propagation.
 *
 * Condition: dst turnstile and thread are locked.
 */
static boolean_t
turnstile_update_thread_promotion_locked(
	struct turnstile *dst_turnstile,
	thread_t thread)
{
	int thread_link_priority;

	int priority = turnstile_compute_thread_push(dst_turnstile, thread);

	thread_link_priority = priority_queue_entry_sched_pri(
		&dst_turnstile->ts_waitq.waitq_prio_queue,
		&thread->wait_prioq_links);

	if (priority != thread_link_priority) {
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (THREAD_MOVED_IN_TURNSTILE_WAITQ))) | DBG_FUNC_NONE,
		    VM_KERNEL_UNSLIDE_OR_PERM(dst_turnstile),
		    thread_tid(thread),
		    priority,
		    thread_link_priority, 0);
	}

	if (!turnstile_priority_queue_update_entry_key(
		    &dst_turnstile->ts_waitq.waitq_prio_queue,
		    &thread->wait_prioq_links, (priority_queue_key_t)priority)) {
		return FALSE;
	}

	/* Update dst turnstile's priority */
	return turnstile_recompute_priority_locked(dst_turnstile);
}

/*
 * Name: thread_add_turnstile_promotion
 *
 * Description: Add a turnstile to thread's inheritor list and update thread's priority.
 *
 * Arg1: thread
 * Arg2: turnstile
 *
 * Returns: TRUE: if the thread's priority has changed and needs propagation.
 *          FALSE: if the thread's priority did not change or it does not need propagation.
 *
 * Condition: turnstile locked.
 */
static boolean_t
thread_add_turnstile_promotion(
	thread_t thread,
	struct turnstile *turnstile)
{
	boolean_t needs_update = FALSE;

	/* Update the pairing heap */
	thread_lock(thread);

	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
	    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (TURNSTILE_ADDED_TO_THREAD_HEAP))) | DBG_FUNC_NONE,
	    thread_tid(thread),
	    VM_KERNEL_UNSLIDE_OR_PERM(turnstile),
	    turnstile->ts_priority, 0, 0);

	priority_queue_entry_init(&turnstile->ts_inheritor_links);

	switch (turnstile_promote_policy[turnstile_get_type(turnstile)]) {
	case TURNSTILE_USER_PROMOTE:
	case TURNSTILE_USER_IPC_PROMOTE:

		priority_queue_entry_set_sched_pri(&thread->base_inheritor_queue,
		    &turnstile->ts_inheritor_links, turnstile->ts_priority, false);
		if (priority_queue_insert(&thread->base_inheritor_queue,
		    &turnstile->ts_inheritor_links)) {
			needs_update = thread_recompute_user_promotion_locked(thread);
		}

		break;
	case TURNSTILE_KERNEL_PROMOTE:

		priority_queue_entry_set_sched_pri(&thread->sched_inheritor_queue,
		    &turnstile->ts_inheritor_links, turnstile->ts_priority, false);
		if (priority_queue_insert(&thread->sched_inheritor_queue,
		    &turnstile->ts_inheritor_links)) {
			needs_update = thread_recompute_kernel_promotion_locked(thread);
		}

		break;
	default:
		panic("turnstile promotion for type %d not yet implemented", turnstile_get_type(turnstile));
	}

	/* Update turnstile stats */
	if (!needs_update) {
		turnstile_stats_update(1,
		    thread_get_update_flags_for_turnstile_propagation_stoppage(thread) |
		    TSU_TURNSTILE_ARG | TSU_BOOST_ARG,
		    turnstile);
	}

	thread_unlock(thread);

	return needs_update;
}

/*
 * Name: thread_remove_turnstile_promotion
 *
 * Description: Remove turnstile from thread's inheritor list and update thread's priority.
 *
 * Arg1: thread
 * Arg2: turnstile
 *
 * Returns: TRUE: if the thread's priority has changed and needs propagation.
 *          FALSE: if the thread's priority did not change or it does not need propagation.
 *
 * Condition: turnstile locked.
 */
static boolean_t
thread_remove_turnstile_promotion(
	thread_t thread,
	struct turnstile *turnstile)
{
	boolean_t needs_update = FALSE;

	thread_lock(thread);

	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
	    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (TURNSTILE_REMOVED_FROM_THREAD_HEAP))) | DBG_FUNC_NONE,
	    thread_tid(thread),
	    VM_KERNEL_UNSLIDE_OR_PERM(turnstile),
	    0, 0, 0);

	/* Update the pairing heap */

	switch (turnstile_promote_policy[turnstile_get_type(turnstile)]) {
	case TURNSTILE_USER_PROMOTE:
	case TURNSTILE_USER_IPC_PROMOTE:
		if (priority_queue_remove(&thread->base_inheritor_queue,
		    &turnstile->ts_inheritor_links)) {
			needs_update = thread_recompute_user_promotion_locked(thread);
		}
		break;
	case TURNSTILE_KERNEL_PROMOTE:
		if (priority_queue_remove(&thread->sched_inheritor_queue,
		    &turnstile->ts_inheritor_links)) {
			needs_update = thread_recompute_kernel_promotion_locked(thread);
		}
		break;
	default:
		panic("turnstile promotion for type %d not yet implemented", turnstile_get_type(turnstile));
	}

	/* Update turnstile stats */
	if (!needs_update) {
		turnstile_stats_update(1,
		    thread_get_update_flags_for_turnstile_propagation_stoppage(thread) | TSU_TURNSTILE_ARG,
		    turnstile);
	}

	thread_unlock(thread);

	return needs_update;
}

/*
 * Name: thread_needs_turnstile_promotion_update
 *
 * Description: Check if turnstile position in thread's inheritor list needs to be updated.
 *
 * Arg1: thread
 * Arg2: turnstile
 *
 * Returns: TRUE: if thread_update_turnstile_promotion needs to be called.
 *          FALSE: otherwise.
 *
 * Condition: turnstile locked.
 */
static boolean_t
thread_needs_turnstile_promotion_update(
	thread_t thread __assert_only,
	struct turnstile *turnstile)
{
	boolean_t needs_update = FALSE;
	int turnstile_link_priority = 0;

	switch (turnstile_promote_policy[turnstile_get_type(turnstile)]) {
	case TURNSTILE_USER_PROMOTE:
	case TURNSTILE_USER_IPC_PROMOTE:
		turnstile_link_priority = priority_queue_entry_sched_pri(
			&thread->base_inheritor_queue,
			&turnstile->ts_inheritor_links);
		break;
	case TURNSTILE_KERNEL_PROMOTE:
		turnstile_link_priority = priority_queue_entry_sched_pri(
			&thread->sched_inheritor_queue,
			&turnstile->ts_inheritor_links);
		break;
	default:
		panic("turnstile promotion for type %d not yet implemented", turnstile_get_type(turnstile));
	}

	needs_update = (turnstile_link_priority == turnstile->ts_priority) ? FALSE : TRUE;
	return needs_update;
}

/*
 * Name: thread_update_turnstile_promotion_locked
 *
 * Description: Update turnstile position in thread's inheritor list and update thread's priority.
 *
 * Arg1: thread
 * Arg2: turnstile
 *
 * Returns: TRUE: if the thread's priority has changed and needs propagation.
 *          FALSE: if the thread's priority did not change or it does not need propagation.
 *
 * Condition: turnstile and thread are locked.
 */
static boolean_t
thread_update_turnstile_promotion_locked(
	thread_t thread,
	struct turnstile *turnstile)
{
	boolean_t needs_update = FALSE;
	int turnstile_link_priority = 0;

	switch (turnstile_promote_policy[turnstile_get_type(turnstile)]) {
	case TURNSTILE_USER_PROMOTE:
	case TURNSTILE_USER_IPC_PROMOTE:
		turnstile_link_priority = priority_queue_entry_sched_pri(
			&thread->base_inheritor_queue,
			&turnstile->ts_inheritor_links);

		if (turnstile_priority_queue_update_entry_key(&(thread->base_inheritor_queue),
		    &turnstile->ts_inheritor_links, turnstile->ts_priority)) {
			needs_update = thread_recompute_user_promotion_locked(thread);
		}
		break;
	case TURNSTILE_KERNEL_PROMOTE:
		turnstile_link_priority = priority_queue_entry_sched_pri(
			&thread->sched_inheritor_queue,
			&turnstile->ts_inheritor_links);

		if (turnstile_priority_queue_update_entry_key(&(thread->sched_inheritor_queue),
		    &turnstile->ts_inheritor_links, turnstile->ts_priority)) {
			needs_update = thread_recompute_kernel_promotion_locked(thread);
		}
		break;
	default:
		panic("turnstile promotion for type %d not yet implemented", turnstile_get_type(turnstile));
	}

	if (turnstile->ts_priority != turnstile_link_priority) {
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (TURNSTILE_MOVED_IN_THREAD_HEAP))) | DBG_FUNC_NONE,
		    thread_tid(thread),
		    VM_KERNEL_UNSLIDE_OR_PERM(turnstile),
		    turnstile->ts_priority,
		    turnstile_link_priority, 0);
	}

	return needs_update;
}


/*
 * Name: thread_update_turnstile_promotion
 *
 * Description: Update turnstile position in thread's inheritor list and update thread's priority.
 *
 * Arg1: thread
 * Arg2: turnstile
 *
 * Returns: TRUE: if the thread's priority has changed and needs propagation.
 *          FALSE: if the thread's priority did not change or it does not need propagation.
 *
 * Condition: turnstile locked.
 */
static boolean_t
thread_update_turnstile_promotion(
	thread_t thread,
	struct turnstile *turnstile)
{
	/* Before grabbing the thread lock, check if update is needed */
	boolean_t needs_update = thread_needs_turnstile_promotion_update(thread, turnstile);

	if (!needs_update) {
		turnstile_stats_update(1, TSU_NO_PRI_CHANGE_NEEDED |
		    TSU_TURNSTILE_ARG | TSU_BOOST_ARG, turnstile);
		return needs_update;
	}

	thread_lock(thread);

	/* Update the pairing heap */
	needs_update = thread_update_turnstile_promotion_locked(thread, turnstile);

	/* Update turnstile stats */
	if (!needs_update) {
		turnstile_stats_update(1,
		    thread_get_update_flags_for_turnstile_propagation_stoppage(thread) |
		    TSU_TURNSTILE_ARG | TSU_BOOST_ARG,
		    turnstile);
	}

	thread_unlock(thread);

	return needs_update;
}


/*
 * Name: thread_get_inheritor_turnstile_sched_priority
 *
 * Description: Get the max sched priority of all the inheritor turnstiles
 *
 * Arg1: thread
 *
 * Returns: Max sched priority of all the inheritor turnstiles.
 *
 * Condition: thread locked
 */
int
thread_get_inheritor_turnstile_sched_priority(thread_t thread)
{
	struct turnstile *max_turnstile;

	max_turnstile = priority_queue_max(&thread->sched_inheritor_queue,
	    struct turnstile, ts_inheritor_links);

	if (max_turnstile) {
		return priority_queue_entry_sched_pri(
			&thread->sched_inheritor_queue,
			&max_turnstile->ts_inheritor_links);
	}

	return 0;
}

/*
 * Name: thread_get_inheritor_turnstile_base_priority
 *
 * Description: Get the max base priority of all the inheritor turnstiles
 *
 * Arg1: thread
 *
 * Returns: Max base priority of all the inheritor turnstiles.
 *
 * Condition: thread locked
 */
int
thread_get_inheritor_turnstile_base_priority(thread_t thread)
{
	struct turnstile *max_turnstile;

	max_turnstile = priority_queue_max(&thread->base_inheritor_queue,
	    struct turnstile, ts_inheritor_links);

	if (max_turnstile) {
		return priority_queue_entry_sched_pri(
			&thread->base_inheritor_queue,
			&max_turnstile->ts_inheritor_links);
	}

	return 0;
}


/*
 * Name: thread_get_waiting_turnstile
 *
 * Description: Get the turnstile if the thread is waiting on a turnstile.
 *
 * Arg1: thread
 *
 * Returns: turnstile: if the thread is blocked on a turnstile.
 *          TURNSTILE_NULL: otherwise.
 *
 * Condition: thread locked.
 */
struct turnstile *
thread_get_waiting_turnstile(thread_t thread)
{
	waitq_t waitq = thread->waitq;

	/* Check if the thread is on a waitq */
	if (waitq_is_null(waitq)) {
		return TURNSTILE_NULL;
	}

	switch (waitq_type(waitq)) {
	case WQT_PORT:
		return waitq.wq_q->waitq_ts;

	case WQT_TURNSTILE:
		return waitq_to_turnstile(waitq.wq_q);

	default:
		return TURNSTILE_NULL;
	}
}

/*
 * Name: turnstile_lookup_by_proprietor
 *
 * Description: Get turnstile for a proprietor from global
 *              turnstile hash.
 *
 * Arg1: port
 * Arg2: turnstile_type_t type
 *
 * Returns: turnstile: if the proprietor has a turnstile.
 *          TURNSTILE_NULL: otherwise.
 *
 * Condition: proprietor interlock held.
 */
struct turnstile *
turnstile_lookup_by_proprietor(uintptr_t proprietor, turnstile_type_t type)
{
	return turnstile_htable_lookup(proprietor, type);
}

/*
 * Name: thread_get_update_flags_for_turnstile_propagation_stoppage
 *
 * Description: Get the turnstile stats flags based on the thread wait status.
 *
 * Arg1: thread
 *
 * Returns: TSU_THREAD_RUNNABLE: if the thread is runnable.
 *          TSU_NO_TURNSTILE: if thread waiting on a regular waitq.
 *          TSU_NO_PRI_CHANGE_NEEDED: otherwise.
 *
 * Condition: thread locked.
 */
static turnstile_stats_update_flags_t
thread_get_update_flags_for_turnstile_propagation_stoppage(thread_t thread)
{
	waitq_t waitq = thread->waitq;

	/* Check if the thread is on a waitq */
	if (waitq_is_null(waitq)) {
		return TSU_THREAD_RUNNABLE;
	}

	/* Get the safeq if the waitq is a port queue */
	switch (waitq_type(waitq)) {
	case WQT_PORT:
		if (waitq.wq_q->waitq_ts) {
			return TSU_NO_PRI_CHANGE_NEEDED;
		}
		return TSU_NO_TURNSTILE;

	case WQT_TURNSTILE:
		/* Thread blocked on turnstile waitq but no propagation needed */
		return TSU_NO_PRI_CHANGE_NEEDED;

	default:
		return TSU_NO_TURNSTILE;
	}
}


/*
 * Name: turnstile_get_update_flags_for_above_UI_pri_change
 *
 * Description: Get the turnstile stats flags based on the turnstile priority.
 *
 * Arg1: turnstile
 *
 * Returns: TSU_ABOVE_UI_PRI_CHANGE: if turnstile priority is above 47 and it is not an ulock.
 *          TSU_FLAGS_NONE: otherwise.
 *
 * Condition: turnstile locked.
 */
static turnstile_stats_update_flags_t
turnstile_get_update_flags_for_above_UI_pri_change(struct turnstile *turnstile)
{
	if (turnstile->ts_priority >
	    (thread_qos_policy_params.qos_pri[THREAD_QOS_USER_INTERACTIVE] + 1) &&
	    turnstile_get_type(turnstile) != TURNSTILE_ULOCK) {
		return TSU_ABOVE_UI_PRI_CHANGE;
	}

	return TSU_FLAGS_NONE;
}


/*
 * Name: workq_add_turnstile_promotion
 *
 * Description: Connect the workqueue turnstile to the workqueue as a fake
 *              inheritor
 *
 * Arg1: workqueue
 * Arg2: turnstile
 *
 * Condition: turnstile locked.
 */
static boolean_t
workq_add_turnstile_promotion(
	struct workqueue *wq_inheritor __unused,
	struct turnstile *turnstile)
{
	/*
	 * If the push is higher than MAXPRI_THROTTLE then the workqueue should
	 * bring up a thread.
	 */
	return turnstile->ts_priority > MAXPRI_THROTTLE;
}

/*
 * Name: turnstile_need_turnstile_promotion_update
 *
 * Description: Check if turnstile position in turnstile's inheritor list needs to be updated.
 *
 * Arg1: dst turnstile
 * Arg2: src turnstile
 *
 * Returns: TRUE: if turnstile_update_turnstile_promotion needs to be called.
 *          FALSE: otherwise.
 *
 * Condition: src turnstile locked.
 */
static boolean_t
turnstile_need_turnstile_promotion_update(
	struct turnstile *dst_turnstile __assert_only,
	struct turnstile *src_turnstile)
{
	int src_turnstile_link_priority;
	boolean_t needs_update = FALSE;

	src_turnstile_link_priority = priority_queue_entry_sched_pri(
		&dst_turnstile->ts_inheritor_queue,
		&src_turnstile->ts_inheritor_links);

	needs_update = (src_turnstile_link_priority == src_turnstile->ts_priority) ? FALSE : TRUE;
	return needs_update;
}

/*
 * Name: turnstile_update_turnstile_promotion_locked
 *
 * Description: Update dst turnstile's inheritor link since src turnstile's
 *              promote priority has changed.
 *
 * Arg1: dst turnstile
 * Arg2: src turnstile
 *
 * Returns: TRUE: if the dst turnstile priority has changed and needs propagation.
 *          FALSE: if the dst turnstile priority did not change or it does not need propagation.
 *
 * Condition: src and dst turnstile locked.
 */
static boolean_t
turnstile_update_turnstile_promotion_locked(
	struct turnstile *dst_turnstile,
	struct turnstile *src_turnstile)
{
	int src_turnstile_link_priority;
	src_turnstile_link_priority = priority_queue_entry_sched_pri(
		&dst_turnstile->ts_inheritor_queue,
		&src_turnstile->ts_inheritor_links);

	if (src_turnstile->ts_priority != src_turnstile_link_priority) {
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (TURNSTILE_MOVED_IN_TURNSTILE_HEAP))) | DBG_FUNC_NONE,
		    VM_KERNEL_UNSLIDE_OR_PERM(dst_turnstile),
		    VM_KERNEL_UNSLIDE_OR_PERM(src_turnstile),
		    src_turnstile->ts_priority, src_turnstile_link_priority, 0);
	}

	if (!turnstile_priority_queue_update_entry_key(
		    &dst_turnstile->ts_inheritor_queue, &src_turnstile->ts_inheritor_links,
		    src_turnstile->ts_priority)) {
		return FALSE;
	}

	/* Update dst turnstile's priority */
	return turnstile_recompute_priority_locked(dst_turnstile);
}

/*
 * Name: turnstile_update_turnstile_promotion
 *
 * Description: Update dst turnstile's inheritor link since src turnstile's
 *              promote priority has changed.
 *
 * Arg1: dst turnstile
 * Arg2: src turnstile
 *
 * Returns: TRUE: if the dst turnstile priority has changed and needs propagation.
 *          FALSE: if the dst turnstile priority did not change or it does not need propagation.
 *
 * Condition: src turnstile locked.
 */
static boolean_t
turnstile_update_turnstile_promotion(
	struct turnstile *dst_turnstile,
	struct turnstile *src_turnstile)
{
	/* Check if update is needed before grabbing the src turnstile lock */
	boolean_t needs_update = turnstile_need_turnstile_promotion_update(dst_turnstile, src_turnstile);
	if (!needs_update) {
		turnstile_stats_update(1, TSU_NO_PRI_CHANGE_NEEDED |
		    TSU_TURNSTILE_ARG | TSU_BOOST_ARG,
		    src_turnstile);
		return needs_update;
	}

	/* Update the pairing heap */
	waitq_lock(&dst_turnstile->ts_waitq);
	needs_update = turnstile_update_turnstile_promotion_locked(dst_turnstile, src_turnstile);

	/* Update turnstile stats */
	if (!needs_update) {
		turnstile_stats_update(1,
		    (dst_turnstile->ts_inheritor ? TSU_NO_PRI_CHANGE_NEEDED : TSU_NO_INHERITOR) |
		    TSU_TURNSTILE_ARG | TSU_BOOST_ARG, src_turnstile);
	}
	waitq_unlock(&dst_turnstile->ts_waitq);
	return needs_update;
}

/*
 * Name: turnstile_add_turnstile_promotion
 *
 * Description: Add src turnstile to dst turnstile's inheritor link
 *              and update dst turnstile's priority.
 *
 * Arg1: dst turnstile
 * Arg2: src turnstile
 *
 * Returns: TRUE: if the dst turnstile priority has changed and needs propagation.
 *          FALSE: if the dst turnstile priority did not change or it does not need propagation.
 *
 * Condition: src turnstile locked.
 */
static boolean_t
turnstile_add_turnstile_promotion(
	struct turnstile *dst_turnstile,
	struct turnstile *src_turnstile)
{
	boolean_t needs_update = FALSE;

	/* Update the pairing heap */
	waitq_lock(&dst_turnstile->ts_waitq);

	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
	    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (TURNSTILE_ADDED_TO_TURNSTILE_HEAP))) | DBG_FUNC_NONE,
	    VM_KERNEL_UNSLIDE_OR_PERM(dst_turnstile),
	    VM_KERNEL_UNSLIDE_OR_PERM(src_turnstile),
	    src_turnstile->ts_priority, 0, 0);

	priority_queue_entry_init(&src_turnstile->ts_inheritor_links);
	priority_queue_entry_set_sched_pri(&dst_turnstile->ts_inheritor_queue,
	    &src_turnstile->ts_inheritor_links, src_turnstile->ts_priority, false);
	if (priority_queue_insert(&dst_turnstile->ts_inheritor_queue,
	    &src_turnstile->ts_inheritor_links)) {
		/* Update dst turnstile priority */
		needs_update = turnstile_recompute_priority_locked(dst_turnstile);
	}

	/* Update turnstile stats */
	if (!needs_update) {
		turnstile_stats_update(1,
		    (dst_turnstile->ts_inheritor ? TSU_NO_PRI_CHANGE_NEEDED : TSU_NO_INHERITOR) |
		    TSU_TURNSTILE_ARG | TSU_BOOST_ARG, src_turnstile);
	}

	waitq_unlock(&dst_turnstile->ts_waitq);
	return needs_update;
}

/*
 * Name: turnstile_remove_turnstile_promotion
 *
 * Description: Remove src turnstile from dst turnstile's inheritor link
 *              and update dst turnstile's priority.
 *
 * Arg1: dst turnstile
 * Arg2: src turnstile
 *
 * Returns: TRUE: if the dst turnstile priority has changed and needs propagation.
 *          FALSE: if the dst turnstile priority did not change or it does not need propagation.
 *
 * Condition: src turnstile locked.
 */
static boolean_t
turnstile_remove_turnstile_promotion(
	struct turnstile *dst_turnstile,
	struct turnstile *src_turnstile)
{
	boolean_t needs_update = FALSE;

	/* Update the pairing heap */
	waitq_lock(&dst_turnstile->ts_waitq);

	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
	    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (TURNSTILE_REMOVED_FROM_TURNSTILE_HEAP))) | DBG_FUNC_NONE,
	    VM_KERNEL_UNSLIDE_OR_PERM(dst_turnstile),
	    VM_KERNEL_UNSLIDE_OR_PERM(src_turnstile),
	    0, 0, 0);

	if (priority_queue_remove(&dst_turnstile->ts_inheritor_queue,
	    &src_turnstile->ts_inheritor_links)) {
		/* Update dst turnstile priority */
		needs_update = turnstile_recompute_priority_locked(dst_turnstile);
	}

	/* Update turnstile stats */
	if (!needs_update) {
		turnstile_stats_update(1,
		    (dst_turnstile->ts_inheritor ? TSU_NO_PRI_CHANGE_NEEDED : TSU_NO_INHERITOR) |
		    TSU_TURNSTILE_ARG, src_turnstile);
	}

	waitq_unlock(&dst_turnstile->ts_waitq);
	return needs_update;
}

/*
 * Name: turnstile_compute_thread_push
 *
 * Description: Compute the priority at which the thread will push
 *       on the turnstile.
 *
 * Arg1: turnstile
 * Arg2: thread
 *
 * Condition: wq locked
 */
static int
turnstile_compute_thread_push(
	struct turnstile *turnstile,
	thread_t thread)
{
	int priority = 0;
	switch (turnstile_promote_policy[turnstile_get_type(turnstile)]) {
	case TURNSTILE_USER_PROMOTE:
	case TURNSTILE_USER_IPC_PROMOTE:
		priority = thread->base_pri;
		break;
	case TURNSTILE_KERNEL_PROMOTE:
		/*
		 * Ideally this should be policy based
		 * according to the turnstile type.
		 *
		 * The priority with which each thread pushes on
		 * a primitive should be primitive dependent.
		 */
		priority = thread->sched_pri;
		priority = MAX(priority, thread->base_pri);
		priority = MAX(priority, BASEPRI_DEFAULT);
		priority = MIN(priority, MAXPRI_PROMOTE);
		break;
	default:
		panic("turnstile promotion for type %d not yet implemented", turnstile_get_type(turnstile));
	}

	return priority;
}

/*
 * Name: turnstile_waitq_add_thread_priority_queue
 *
 * Description: add thread to the turnstile wq
 *
 * Arg1: turnstile wq
 * Arg2: thread to add
 *
 * Condition: wq locked
 */
void
turnstile_waitq_add_thread_priority_queue(
	struct waitq *wq,
	thread_t thread)
{
	struct turnstile *turnstile = waitq_to_turnstile(wq);
	int priority = turnstile_compute_thread_push(turnstile, thread);

	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
	    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS, (THREAD_ADDED_TO_TURNSTILE_WAITQ))) | DBG_FUNC_NONE,
	    VM_KERNEL_UNSLIDE_OR_PERM(turnstile),
	    thread_tid(thread),
	    priority, 0, 0);
	/*
	 * For turnstile queues (which use priority queues),
	 * insert the thread in the heap based on its priority.
	 * Note that the priority queue implementation
	 * is currently not stable, so does not maintain fifo for
	 * threads at the same pri. Also, if the pri
	 * of the thread changes while its blocked in the waitq,
	 * the thread position should be updated in the priority
	 * queue by calling priority queue increase/decrease
	 * operations.
	 */
	priority_queue_entry_init(&thread->wait_prioq_links);
	priority_queue_entry_set_sched_pri(&wq->waitq_prio_queue,
	    &thread->wait_prioq_links, priority, false);
	priority_queue_insert(&wq->waitq_prio_queue,
	    &thread->wait_prioq_links);
}

/*
 * Name: turnstile_recompute_priority_locked
 *
 * Description: Update turnstile priority based
 *              on highest waiter thread and highest blocking
 *              turnstile.
 *
 * Args: turnstile
 *
 * Returns: TRUE: if the turnstile priority changed and needs propagation.
 *          FALSE: if the turnstile priority did not change or it does not need propagation.
 *
 * Condition: turnstile locked
 */
boolean_t
turnstile_recompute_priority_locked(
	struct turnstile *turnstile)
{
	int old_priority;
	int new_priority;
	boolean_t needs_priority_update = FALSE;
	thread_t max_thread = THREAD_NULL;
	struct turnstile *max_turnstile;
	int thread_max_pri = 0;
	int turnstile_max_pri = 0;

	switch (turnstile_promote_policy[turnstile_get_type(turnstile)]) {
	case TURNSTILE_USER_PROMOTE:
	case TURNSTILE_USER_IPC_PROMOTE:
	case TURNSTILE_KERNEL_PROMOTE:

		old_priority = turnstile->ts_priority;

		max_thread = priority_queue_max(&turnstile->ts_waitq.waitq_prio_queue,
		    struct thread, wait_prioq_links);

		if (max_thread) {
			thread_max_pri = priority_queue_entry_sched_pri(
				&turnstile->ts_waitq.waitq_prio_queue,
				&max_thread->wait_prioq_links);
		}

		max_turnstile = priority_queue_max(&turnstile->ts_inheritor_queue,
		    struct turnstile, ts_inheritor_links);

		if (max_turnstile) {
			assert(turnstile_promote_policy[turnstile_get_type(turnstile)] != TURNSTILE_KERNEL_PROMOTE);
			turnstile_max_pri = priority_queue_entry_sched_pri(
				&turnstile->ts_inheritor_queue,
				&max_turnstile->ts_inheritor_links);
		}

		new_priority = max(thread_max_pri, turnstile_max_pri);
		turnstile->ts_priority = (uint8_t)new_priority;

		if (old_priority != new_priority) {
			KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
			    (TURNSTILE_CODE(TURNSTILE_PRIORITY_OPERATIONS,
			    (TURNSTILE_PRIORITY_CHANGE))) | DBG_FUNC_NONE,
			    VM_KERNEL_UNSLIDE_OR_PERM(turnstile),
			    new_priority,
			    old_priority,
			    0, 0);
		}
		needs_priority_update = (!(old_priority == new_priority)) &&
		    (turnstile->ts_inheritor != NULL);
		break;

	case TURNSTILE_PROMOTE_NONE:
		/* The turnstile was repurposed, do nothing */
		break;

	default:

		panic("Needs implementation for turnstile_recompute_priority");
		break;
	}
	return needs_priority_update;
}


/*
 * Name: turnstile_recompute_priority
 *
 * Description: Update turnstile priority based
 *              on highest waiter thread and highest blocking
 *              turnstile.
 *
 * Args: turnstile
 *
 * Returns: TRUE: if the turnstile priority changed and needs propagation.
 *          FALSE: if the turnstile priority did not change or it does not need propagation.
 */
boolean_t
turnstile_recompute_priority(
	struct turnstile *turnstile)
{
	boolean_t needs_priority_update = FALSE;
	spl_t s = splsched();

	waitq_lock(&turnstile->ts_waitq);

	needs_priority_update = turnstile_recompute_priority_locked(turnstile);

	waitq_unlock(&turnstile->ts_waitq);
	splx(s);
	return needs_priority_update;
}


/*
 * Name: turnstile_workq_proprietor_of_max_turnstile
 *
 * Description: Returns the highest priority and proprietor of a turnstile
 *              pushing on a workqueue turnstile.
 *
 *              This will not return waiters that are at priority
 *              MAXPRI_THROTTLE or lower.
 *
 * Args: turnstile
 *
 * Returns:
 *    Priority of the max entry, or 0
 *    Pointer to the max entry proprietor
 */
int
turnstile_workq_proprietor_of_max_turnstile(
	struct turnstile *turnstile,
	uintptr_t *proprietor_out)
{
	struct turnstile *max_turnstile;
	int max_priority = 0;
	uintptr_t proprietor = 0;

	assert(turnstile_get_type(turnstile) == TURNSTILE_WORKQS);

	spl_t s = splsched();

	waitq_lock(&turnstile->ts_waitq);

	max_turnstile = priority_queue_max(&turnstile->ts_inheritor_queue,
	    struct turnstile, ts_inheritor_links);
	if (max_turnstile) {
		max_priority = priority_queue_entry_sched_pri(
			&turnstile->ts_inheritor_queue,
			&max_turnstile->ts_inheritor_links);
		proprietor = max_turnstile->ts_proprietor;
	}

	waitq_unlock(&turnstile->ts_waitq);
	splx(s);

	if (max_priority <= MAXPRI_THROTTLE) {
		max_priority = 0;
		proprietor = 0;
	}
	if (proprietor_out) {
		*proprietor_out = proprietor;
	}
	return max_priority;
}

/*
 * Name: turnstile_workloop_pusher_info
 *
 * Description: Returns the priority of the turnstile push for a workloop,
 *              and the thread or knote responsible for this push.
 *
 * Args: workloop turnstile
 *
 * Returns:
 *    Priority of the push or 0
 *    Thread (with a +1 reference) with that push or THREAD_NULL.
 *    Port (with a +1 reference) with that push, or IP_NULL.
 *    Sync IPC knote with the highest push (or NULL)
 */
int
turnstile_workloop_pusher_info(
	struct turnstile *turnstile,
	thread_t *thread_out,
	ipc_port_t *port_out,
	struct knote **knote_out)
{
	struct turnstile *max_ts;
	thread_t max_thread;
	int max_thread_pri = 0;
	int max_ts_pri = 0;
	ipc_port_t port;

	assert(turnstile_get_type(turnstile) == TURNSTILE_WORKLOOPS);

	spl_t s = splsched();
	waitq_lock(&turnstile->ts_waitq);

	max_thread = priority_queue_max(&turnstile->ts_waitq.waitq_prio_queue,
	    struct thread, wait_prioq_links);
	if (max_thread) {
		max_thread_pri = priority_queue_entry_sched_pri(
			&turnstile->ts_waitq.waitq_prio_queue,
			&max_thread->wait_prioq_links);
	}

	max_ts = priority_queue_max(&turnstile->ts_inheritor_queue,
	    struct turnstile, ts_inheritor_links);
	if (max_ts) {
		max_ts_pri = priority_queue_entry_sched_pri(
			&turnstile->ts_inheritor_queue,
			&max_ts->ts_inheritor_links);
	}

	/*
	 * Reasons to push on a workloop turnstile are:
	 *
	 * 1. threads in dispatch sync
	 *
	 * 2. sync IPC pushes, which in turn have 4 sub-cases:
	 *
	 *   2.a. special reply port or receive right pushing through a knote
	 *        turnstile,
	 *
	 *   2.b. special reply port stashed on a knote, pushing on the workloop
	 *        directly,
	 *
	 *   2.c. receive right stashed on a knote, pushing on the workloop
	 *        directly,
	 *
	 *   2.d. a receive right monitored by a knote, pushing on the workloop
	 *        directly.
	 *
	 * See ipc_port_send_update_inheritor(), ipc_port_recv_update_inheritor().
	 *
	 * Note: dereferencing the knote in the caller is safe provided this
	 * function i scalled under the proper interlocks (the filt_wllock + req
	 * lock) which serializes with the knote going away.
	 */
	if (max_thread_pri > max_ts_pri) {
		thread_reference(max_thread);
		*thread_out = max_thread;
		*port_out = NULL;
		*knote_out = NULL;
	} else if (max_ts_pri) {
		switch (turnstile_get_type(max_ts)) {
		case TURNSTILE_KNOTE:
			/* 2.a. */
			*thread_out = THREAD_NULL;
			*port_out = IP_NULL;
			*knote_out = (struct knote *)max_ts->ts_proprietor;
			break;

		case TURNSTILE_SYNC_IPC:
			/* 2.[bcd] */
			port = (ipc_port_t)max_ts->ts_proprietor;
			ip_reference(port);
			*thread_out = THREAD_NULL;
			*port_out = port;
			*knote_out = NULL;
			break;

		default:
			panic("Unexpected type for turnstile %p", max_ts);
		}
	} else {
		*thread_out = THREAD_NULL;
		*port_out = IP_NULL;
		*knote_out = NULL;
	}

	waitq_unlock(&turnstile->ts_waitq);
	splx(s);

	return max(max_thread_pri, max_ts_pri);
}

/*
 * Name: turnstile_has_waiters
 *
 * Description: returns if there are waiters on the turnstile
 *
 * Arg1: turnstile: turnstile
 *
 * Returns: TRUE if there are waiters, FALSE otherwise.
 */

boolean_t
turnstile_has_waiters(struct turnstile *turnstile)
{
	boolean_t ret;

	spl_t s = splsched();
	waitq_lock(&turnstile->ts_waitq);
	ret = !priority_queue_empty(&turnstile->ts_waitq.waitq_prio_queue);
	waitq_unlock(&turnstile->ts_waitq);
	splx(s);

	return ret;
}

/*
 * Name: turnstile_update_inheritor_priority_chain
 *
 * Description: Update turnstile inheritor's priority and propagate
 *              the priority if the inheritor is blocked on a turnstile.
 *
 * Arg1: inheritor
 * Arg2: inheritor flags
 *
 * Returns: None.
 */
static void
turnstile_update_inheritor_priority_chain(
	turnstile_inheritor_t inheritor,
	turnstile_update_flags_t turnstile_flags)
{
	struct turnstile *turnstile = TURNSTILE_NULL;
	thread_t thread = THREAD_NULL;
	int total_hop = 0, thread_hop = 0;
	spl_t s;
	turnstile_stats_update_flags_t tsu_flags = ((turnstile_flags & TURNSTILE_UPDATE_BOOST) ?
	    TSU_BOOST_ARG : TSU_FLAGS_NONE) | TSU_PRI_PROPAGATION;

	if (inheritor == NULL) {
		return;
	}

	s = splsched();

	if (turnstile_flags & TURNSTILE_INHERITOR_THREAD) {
		thread = inheritor;
		thread_lock(thread);
		thread_recompute_user_promotion_locked(thread);
		thread_recompute_kernel_promotion_locked(thread);
	} else if (turnstile_flags & TURNSTILE_INHERITOR_TURNSTILE) {
		turnstile = inheritor;
		waitq_lock(&turnstile->ts_waitq);
		turnstile_recompute_priority_locked(turnstile);
		tsu_flags |= turnstile_get_update_flags_for_above_UI_pri_change(turnstile);
	} else {
		/*
		 * we should never call turnstile_update_inheritor_priority_chain()
		 * for a workqueue, they have no "chain" after them.
		 */
		assert((turnstile_flags & TURNSTILE_INHERITOR_WORKQ) == 0);
	}

	while (turnstile != TURNSTILE_NULL || thread != THREAD_NULL) {
		if (turnstile != TURNSTILE_NULL) {
			if (turnstile->ts_inheritor == NULL) {
				turnstile_stats_update(total_hop + 1, TSU_NO_INHERITOR |
				    TSU_TURNSTILE_ARG | tsu_flags,
				    turnstile);
				waitq_unlock(&turnstile->ts_waitq);
				turnstile = TURNSTILE_NULL;
				break;
			}
			if (turnstile->ts_inheritor_flags & TURNSTILE_INHERITOR_THREAD) {
				turnstile_update_inheritor_thread_priority_chain(&turnstile, &thread,
				    total_hop, tsu_flags);
			} else if (turnstile->ts_inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE) {
				turnstile_update_inheritor_turnstile_priority_chain(&turnstile,
				    total_hop, tsu_flags);
			} else if (turnstile->ts_inheritor_flags & TURNSTILE_INHERITOR_WORKQ) {
				turnstile_update_inheritor_workq_priority_chain(turnstile, s);
				turnstile_stats_update(total_hop + 1, TSU_NO_PRI_CHANGE_NEEDED | tsu_flags,
				    NULL);
				return;
			} else {
				panic("Inheritor flags not passed in turnstile_update_inheritor");
			}
		} else if (thread != THREAD_NULL) {
			thread_update_waiting_turnstile_priority_chain(&thread, &turnstile,
			    thread_hop, total_hop, tsu_flags);
			thread_hop++;
		}
		total_hop++;
	}

	splx(s);
	return;
}

/*
 * Name: turnstile_update_inheritor_complete
 *
 * Description: Update turnstile inheritor's priority and propagate the
 *              priority if the inheritor is blocked on a turnstile.
 *              Consumes thread ref of old inheritor returned by
 *              turnstile_update_inheritor. Recursive priority update
 *              will only happen when called with interlock dropped.
 *
 * Args:
 *   Arg1: turnstile
 *   Arg2: interlock held
 *
 * Returns: None.
 */
void
turnstile_update_inheritor_complete(
	struct turnstile *turnstile,
	turnstile_update_complete_flags_t flags __unused)
{
	thread_t thread = current_thread();

	turnstile_update_flags_t inheritor_flags = thread->inheritor_flags;

	turnstile_cleanup();

	/* Perform priority update for new inheritor */
	if (inheritor_flags & TURNSTILE_NEEDS_PRI_UPDATE) {
		turnstile_update_inheritor_priority_chain(turnstile,
		    TURNSTILE_INHERITOR_TURNSTILE | TURNSTILE_UPDATE_BOOST);
	}
}

/*
 * Name: turnstile_cleanup
 *
 * Description: Update priority of a turnstile inheritor
 *              if needed.
 *
 * Args: inheritor and flags passed on thread struct.
 *
 * Returns: None.
 */
void
turnstile_cleanup(void)
{
	thread_t thread = current_thread();

	/* Get the old inheritor from calling thread struct */
	turnstile_inheritor_t old_inheritor = thread->inheritor;
	turnstile_update_flags_t inheritor_flags = thread->inheritor_flags;
	thread->inheritor = THREAD_NULL;
	thread->inheritor_flags = TURNSTILE_UPDATE_FLAGS_NONE;

	if (old_inheritor == TURNSTILE_INHERITOR_NULL) {
		/* no cleanup to do */
		return;
	}

	/* Perform priority demotion for old inheritor */
	if (inheritor_flags & TURNSTILE_INHERITOR_NEEDS_PRI_UPDATE) {
		turnstile_update_inheritor_priority_chain(old_inheritor,
		    inheritor_flags);
	}

	/* Drop thread reference for old inheritor */
	if (inheritor_flags & TURNSTILE_INHERITOR_THREAD) {
		thread_deallocate_safe(old_inheritor);
	} else if (inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE) {
		turnstile_deallocate((struct turnstile *)old_inheritor);
	} else if (inheritor_flags & TURNSTILE_INHERITOR_WORKQ) {
		workq_deallocate_safe((struct workqueue *)old_inheritor);
	} else {
		panic("Inheritor flags lost along the way");
	}
}

/*
 * Name: turnstile_update_thread_priority_chain
 *
 * Description: Priority of a thread blocked on a turnstile
 *              has changed, update the turnstile priority.
 *
 * Arg1: thread: thread whose priority has changed.
 *
 * Returns: None.
 */
void
turnstile_update_thread_priority_chain(thread_t thread)
{
	turnstile_update_inheritor_priority_chain(thread,
	    TURNSTILE_INHERITOR_THREAD | TURNSTILE_UPDATE_BOOST);
}

/*
 * Name: turnstile_update_inheritor_workq_priority_chain
 *
 * Description: Helper function to update turnstile's inheritor(workq)
 *              priority and possibly redrive thread creation
 *
 * Arg1: turnstile: turnstile
 * Arg2: s: whether iterrupts are disabled.
 *
 * Condition: turnstile is locked on entry, it is unlocked on exit,
 *            and interrupts re-enabled.
 */
static void
turnstile_update_inheritor_workq_priority_chain(struct turnstile *turnstile, spl_t s)
{
	struct workqueue *wq = turnstile->ts_inheritor;
	bool workq_lock_held = workq_is_current_thread_updating_turnstile(wq);

	if (__improbable(turnstile->ts_priority <= MAXPRI_THROTTLE)) {
		waitq_unlock(&turnstile->ts_waitq);
		splx(s);
		return;
	}

	if (!workq_lock_held) {
		workq_reference(wq);
		disable_preemption();
	}
	waitq_unlock(&turnstile->ts_waitq);
	splx(s);

	workq_schedule_creator_turnstile_redrive(wq, workq_lock_held);

	if (!workq_lock_held) {
		enable_preemption();
		workq_deallocate_safe(wq);
	}
}

/*
 * Name: turnstile_update_inheritor_thread_priority_chain
 *
 * Description: Helper function to update turnstile's inheritor(thread)
 *              priority.
 *
 * Arg1: in_turnstile: address to turnstile
 * Arg2: out_thread: address to return the thread inheritor
 * Arg3: thread_hop: number to thread hop in propagation chain
 * Arg4: tsu_flags: turnstile update flags
 *
 * Returns: Implicit returns locked thread in out_thread if it needs
 *          further propagation.
 *
 * Condition: *in_turnstile is locked on entry, it is unlocked on exit and
 *            *in_turnstile is set to NULL.
 */
static void
turnstile_update_inheritor_thread_priority_chain(
	struct turnstile **in_turnstile,
	thread_t *out_thread,
	int total_hop,
	turnstile_stats_update_flags_t tsu_flags)
{
	boolean_t needs_update = FALSE;
	struct turnstile *turnstile = *in_turnstile;
	thread_t thread_inheritor = turnstile->ts_inheritor;
	boolean_t first_update = !total_hop;

	assert(turnstile->ts_inheritor_flags & TURNSTILE_INHERITOR_THREAD);
	*in_turnstile = TURNSTILE_NULL;

	/* Check if update is needed before grabbing the thread lock */
	needs_update = thread_needs_turnstile_promotion_update(thread_inheritor, turnstile);
	if (!needs_update && !first_update) {
		turnstile_stats_update(total_hop + 1, TSU_NO_PRI_CHANGE_NEEDED |
		    TSU_TURNSTILE_ARG | tsu_flags, turnstile);
		waitq_unlock(&turnstile->ts_waitq);
		return;
	}

	thread_lock(thread_inheritor);

	/* adjust turnstile position in the thread's inheritor list */
	needs_update = thread_update_turnstile_promotion_locked(
		thread_inheritor, turnstile);

	/*
	 * Check if thread needs further priority propagation,
	 * since the first hop priority update was done in
	 * turnstile_update_inheritor, do not bailout if it is
	 * the first update as needs_update flag would evaluate to
	 * false for that case.
	 */
	if (!needs_update && !first_update) {
		/* Update turnstile stats before returning */
		turnstile_stats_update(total_hop + 1,
		    (thread_get_update_flags_for_turnstile_propagation_stoppage(thread_inheritor)) |
		    TSU_TURNSTILE_ARG | tsu_flags,
		    turnstile);
		thread_unlock(thread_inheritor);
		waitq_unlock(&turnstile->ts_waitq);
		return;
	}

	/* Unlock the turnstile and update the thread */
	waitq_unlock(&turnstile->ts_waitq);
	*out_thread = thread_inheritor;
	return;
}

/*
 * Name: turnstile_update_inheritor_turnstile_priority_chain
 *
 * Description: Helper function to update turnstile's inheritor(turnstile)
 *              priority.
 *
 * Arg1: in_out_turnstile: address to turnstile
 * Arg2: thread_hop: number of thread hop in propagation chain
 * Arg3: tsu_flags: turnstile update flags
 *
 * Returns: Implicit returns locked turnstile in in_out_turnstile if it needs
 *          further propagation.
 *
 * Condition: *in_out_turnstile is locked on entry, *in_out_turnstile on exit,
 *            but the value of *in_out_turnstile might change and turnstile lock
 *            will be dropped for old value and will be acquired for the new value.
 */
static void
turnstile_update_inheritor_turnstile_priority_chain(
	struct turnstile **in_out_turnstile,
	int total_hop,
	turnstile_stats_update_flags_t tsu_flags)
{
	boolean_t needs_update = FALSE;
	struct turnstile *turnstile = *in_out_turnstile;
	struct turnstile *inheritor_turnstile = turnstile->ts_inheritor;
	boolean_t first_update = !total_hop;

	assert(turnstile->ts_inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE);
	*in_out_turnstile = TURNSTILE_NULL;

	/* Check if the inheritor turnstile needs to be updated before grabbing the lock */
	needs_update = turnstile_need_turnstile_promotion_update(inheritor_turnstile, turnstile);
	if (!needs_update && !first_update) {
		turnstile_stats_update(total_hop + 1, TSU_NO_PRI_CHANGE_NEEDED |
		    TSU_TURNSTILE_ARG | tsu_flags,
		    turnstile);
		waitq_unlock(&turnstile->ts_waitq);
		return;
	}

	waitq_lock(&inheritor_turnstile->ts_waitq);

	needs_update = turnstile_update_turnstile_promotion_locked(
		inheritor_turnstile, turnstile);

	/*
	 * Check if turnstile needs further priority propagation,
	 * since the first hop priority update was done in
	 * turnstile_update_inheritor, do not bailout if it is
	 * the first update as needs_update flag would evaluate to
	 * false for that case.
	 */
	if (!needs_update && !first_update) {
		/* Update turnstile stats before returning */
		turnstile_stats_update(total_hop + 1,
		    (inheritor_turnstile->ts_inheritor ? TSU_NO_PRI_CHANGE_NEEDED : TSU_NO_INHERITOR) |
		    TSU_TURNSTILE_ARG | tsu_flags,
		    turnstile);
		waitq_unlock(&inheritor_turnstile->ts_waitq);
		waitq_unlock(&turnstile->ts_waitq);
		return;
	}

	/* Unlock the outer turnstile and update the inner turnstile */
	waitq_unlock(&turnstile->ts_waitq);
	*in_out_turnstile = inheritor_turnstile;
	return;
}

/*
 * Name: thread_update_waiting_turnstile_priority_chain
 *
 * Description: Helper function to update thread's waiting
 *              turnstile priority.
 *
 * Arg1: in_thread: pointer to thread
 * Arg2: out_turnstile: pointer to turnstile to return to caller
 * Arg3: thread_hop: Number of thread hops visited
 * Arg4: total_hop: total hops visited
 * Arg5: tsu_flags: turnstile update flags
 *
 * Returns: *out_turnstile returns the inheritor if it needs further propagation.
 *
 * Condition: *in_thread locked on entry, unlocked on exit and set to NULL.
 */
static void
thread_update_waiting_turnstile_priority_chain(
	thread_t *in_thread,
	struct turnstile **out_turnstile,
	int thread_hop,
	int total_hop,
	turnstile_stats_update_flags_t tsu_flags)
{
	boolean_t needs_update = FALSE;
	thread_t thread = *in_thread;
	struct turnstile *waiting_turnstile = TURNSTILE_NULL;
	uint32_t turnstile_gencount;
	boolean_t first_update = !total_hop;
	uint32_t ticket;

	*in_thread = THREAD_NULL;

	/* Check if thread waiting on a turnstile */
	waiting_turnstile = thread_get_waiting_turnstile(thread);

	if (waiting_turnstile == TURNSTILE_NULL || thread_hop > turnstile_max_hop) {
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE,
		    (TURNSTILE_CODE(TURNSTILE_HEAP_OPERATIONS,
		    (waiting_turnstile ? TURNSTILE_UPDATE_STOPPED_BY_LIMIT : THREAD_NOT_WAITING_ON_TURNSTILE)
		    )) | DBG_FUNC_NONE,
		    thread_tid(thread),
		    turnstile_max_hop,
		    thread_hop,
		    VM_KERNEL_UNSLIDE_OR_PERM(waiting_turnstile), 0);
		turnstile_stats_update(total_hop + 1, TSU_NO_TURNSTILE |
		    TSU_THREAD_ARG | tsu_flags, thread);
		thread_unlock(thread);
		return;
	}

	/* Check if the thread needs to update the waiting turnstile */
	needs_update = turnstile_need_thread_promotion_update(waiting_turnstile, thread);
	if (!needs_update && !first_update) {
		turnstile_stats_update(total_hop + 1, TSU_NO_PRI_CHANGE_NEEDED |
		    TSU_THREAD_ARG | tsu_flags, thread);
		thread_unlock(thread);
		return;
	}

	/*
	 * Do not take turnstile reference here because
	 * 1. The ticket lock guarantees that waiting_turnstile will not be reused by other threads
	 * 2. We are holding other turnstile lock, and doing turnstile free when we dereference it can cause deadlock
	 * 3. Performance optimization
	 */
	if (!waitq_lock_reserve(&waiting_turnstile->ts_waitq, &ticket)) {
		/* take a reference on thread and snapshot of gencount */
		turnstile_gencount = turnstile_get_gencount(waiting_turnstile);
		thread_reference(thread);

		/* drop the thread lock and wait for turnstile ticket */
		thread_unlock(thread);
		waitq_lock_wait(&waiting_turnstile->ts_waitq, ticket);
		thread_lock(thread);

		/* Check if the gencount matches and thread is still waiting on same turnstile */
		if (turnstile_gencount != turnstile_get_gencount(waiting_turnstile) ||
		    waiting_turnstile != thread_get_waiting_turnstile(thread)) {
			turnstile_stats_update(total_hop + 1, TSU_NO_PRI_CHANGE_NEEDED |
			    TSU_THREAD_ARG | tsu_flags, thread);
			/* No updates required, bail out */
			thread_unlock(thread);
			waitq_unlock(&waiting_turnstile->ts_waitq);
			thread_deallocate_safe(thread);
			return;
		}

		/*
		 * The thread is waiting on the waiting_turnstile and we have thread lock,
		 * we can drop the thread reference since it is on waitq and
		 * it could not be removed from the waitq without the thread lock.
		 */
		thread_deallocate_safe(thread);
	}

	/* adjust thread's position on turnstile waitq */
	needs_update = turnstile_update_thread_promotion_locked(waiting_turnstile, thread);

	/*
	 * Check if thread needs further priority propagation,
	 * since the first hop priority update was done in
	 * turnstile_update_inheritor, do not bailout if it is
	 * the first update as needs_update flag would evaluate to
	 * false for that case.
	 */
	if (!needs_update && !first_update) {
		turnstile_stats_update(total_hop + 1,
		    (waiting_turnstile->ts_inheritor ? TSU_NO_PRI_CHANGE_NEEDED : TSU_NO_INHERITOR) |
		    TSU_THREAD_ARG | tsu_flags, thread);
		thread_unlock(thread);
		waitq_unlock(&waiting_turnstile->ts_waitq);
		return;
	}

	/* drop the thread lock and update the turnstile */
	thread_unlock(thread);
	*out_turnstile = waiting_turnstile;
}

/*
 * Name: turnstile_stats_update
 *
 * Description: Function to update turnstile stats for dev kernel.
 *
 * Arg1: hops : number of thread hops in priority propagation
 * Arg2: flags : turnstile stats update flags
 * Arg3: inheritor: inheritor
 *
 * Returns: Nothing
 */
void
turnstile_stats_update(
	int hop __assert_only,
	turnstile_stats_update_flags_t flags __assert_only,
	turnstile_inheritor_t inheritor __assert_only)
{
#if DEVELOPMENT || DEBUG
	if (flags & TSU_TURNSTILE_BLOCK_COUNT) {
		os_atomic_inc(&thread_block_on_turnstile_count, relaxed);
	}

	if (flags & TSU_REGULAR_WAITQ_BLOCK_COUNT) {
		os_atomic_inc(&thread_block_on_regular_waitq_count, relaxed);
	}

	if (hop > TURNSTILE_MAX_HOP_DEFAULT || hop == 0) {
		return;
	}

	assert(hop >= 0);

	/*
	 * Check if turnstile stats needs to be updated.
	 * Bail out if the turnstile or thread does not
	 * have any user promotion.
	 * Bail out if it is the first hop of WQ turnstile
	 * since WQ's use of a turnstile for the admission check
	 * introduces a lot of noise due to state changes.
	 */
	if (flags & TSU_TURNSTILE_ARG) {
		struct turnstile *ts = (struct turnstile *)inheritor;
		if (ts->ts_priority == 0) {
			return;
		}

		if (hop == 1 && turnstile_get_type(ts) == TURNSTILE_WORKQS) {
			return;
		}
	} else if (flags & TSU_THREAD_ARG) {
		thread_t thread = (thread_t)inheritor;
		if (thread->user_promotion_basepri == 0) {
			return;
		}
	} else {
		assert(inheritor == NULL);
	}

	struct turnstile_stats *turnstile_stats;
	if (flags & TSU_BOOST_ARG) {
		turnstile_stats = turnstile_boost_stats;
	} else {
		turnstile_stats = turnstile_unboost_stats;
	}

	if (flags & TSU_PRI_PROPAGATION) {
		os_atomic_inc(&turnstile_stats[hop - 1].ts_priority_propagation, relaxed);
	}

	if (flags & TSU_NO_INHERITOR) {
		os_atomic_inc(&turnstile_stats[hop - 1].ts_no_inheritor, relaxed);
	}

	if (flags & TSU_NO_TURNSTILE) {
		os_atomic_inc(&turnstile_stats[hop - 1].ts_no_turnstile, relaxed);
	}

	if (flags & TSU_NO_PRI_CHANGE_NEEDED) {
		os_atomic_inc(&turnstile_stats[hop - 1].ts_no_priority_change_required, relaxed);
	}

	if (flags & TSU_THREAD_RUNNABLE) {
		os_atomic_inc(&turnstile_stats[hop - 1].ts_thread_runnable, relaxed);
	}

	if (flags & TSU_ABOVE_UI_PRI_CHANGE) {
		os_atomic_inc(&turnstile_stats[hop - 1].ts_above_ui_pri_change, relaxed);
	}
#endif
}

#define STACKSHOT_TURNSTILE_FLAGS_MODE(flags) ((flags) & ~STACKSHOT_TURNSTILE_STATUS_PORTFLAGS)
#define STACKSHOT_TURNSTILE_FLAGS_PORT(flags) ((flags) & STACKSHOT_TURNSTILE_STATUS_PORTFLAGS)
#define STACKSHOT_TURNSTILE_FLAGS_WITHPORT(flags, port) \
     (STACKSHOT_TURNSTILE_FLAGS_MODE(flags) | STACKSHOT_TURNSTILE_FLAGS_PORT(port))

static uint64_t
kdp_turnstile_traverse_inheritor_chain(struct turnstile *ts, uint64_t *flags, uint8_t *hops, struct ipc_service_port_label **isplp)
{
	uint8_t unknown_hops;

	if (waitq_held(&ts->ts_waitq)) {
		*flags |= STACKSHOT_TURNSTILE_STATUS_LOCKED_WAITQ;
		return 0;
	}

	*hops = *hops + 1;
	unknown_hops = *hops;

	/*
	 * If we find a port along the way, include it.  Do this first so the
	 * furthest one wins.
	 */
	if (turnstile_is_send_turnstile(ts)) {
		ipc_port_t port = (ipc_port_t)ts->ts_proprietor;

		if (port && ip_active(port) && port->ip_service_port && port->ip_splabel != NULL) {
			*flags = STACKSHOT_TURNSTILE_FLAGS_WITHPORT(*flags, STACKSHOT_TURNSTILE_STATUS_SENDPORT);
			*isplp = (struct ipc_service_port_label *)port->ip_splabel;
		}
	}
	if (turnstile_is_receive_turnstile(ts)) {
		ipc_port_t port = (ipc_port_t)ts->ts_proprietor;
		if (port && ip_active(port) && port->ip_service_port && port->ip_splabel != NULL) {
			*flags = STACKSHOT_TURNSTILE_FLAGS_WITHPORT(*flags, STACKSHOT_TURNSTILE_STATUS_RECEIVEPORT);
			*isplp = (struct ipc_service_port_label *)port->ip_splabel;
		}
	}
	/*
	 * If a turnstile is inheriting our priority, recurse.  If we get back *exactly* UNKNOWN,
	 * continue on, since we may be able to give a more specific answer.  To
	 * give an accurate hops count, we reset *hops, saving the recursive value in
	 * unknown_hops to use if we can't give a better answer.
	 */
	if (ts->ts_inheritor_flags & TURNSTILE_INHERITOR_TURNSTILE) {
		uint8_t pre_hops = *hops;
		uint64_t ret = kdp_turnstile_traverse_inheritor_chain(ts->ts_inheritor, flags, hops, isplp);
		/*
		 * Note that while flags is usually |=ed, we're checking with != here to
		 * make sure we only replace *exactly* UNKNOWN, ignoring the portflags.
		 */
		if (ret != 0 || STACKSHOT_TURNSTILE_FLAGS_MODE(*flags) != STACKSHOT_TURNSTILE_STATUS_UNKNOWN) {
			return ret;
		}
		/* restore original hops value, saving the new one if we fall through to unknown */
		unknown_hops = *hops;
		*hops = pre_hops;
		*flags = STACKSHOT_TURNSTILE_FLAGS_PORT(*flags);
	}

	if (ts->ts_inheritor_flags & TURNSTILE_INHERITOR_THREAD) {
		*flags |= STACKSHOT_TURNSTILE_STATUS_THREAD;
		return (uint64_t) thread_tid(ts->ts_inheritor);
	}

	if (ts->ts_inheritor_flags & TURNSTILE_INHERITOR_WORKQ) {
		*flags |= STACKSHOT_TURNSTILE_STATUS_WORKQUEUE;
		return VM_KERNEL_UNSLIDE_OR_PERM(ts->ts_inheritor);
	}

	/*
	 * If we found a send turnstile, try to get the task that the turnstile's
	 * port is in the ipc space of
	 */
	if (turnstile_is_send_turnstile(ts)) {
		ipc_port_t port = (ipc_port_t)ts->ts_proprietor;

		if (port && ip_active(port)) {
			if (ip_mq_lock_held_kdp(port)) {
				*flags |= STACKSHOT_TURNSTILE_STATUS_HELD_IPLOCK;
				return 0;
			}
			if (port->ip_receiver_name != 0 && port->ip_receiver) {
				ipc_space_t space = (ipc_space_t) port->ip_receiver;
				task_t dest_task = space->is_task;

				if (dest_task != TASK_NULL) {
					*flags |= STACKSHOT_TURNSTILE_STATUS_BLOCKED_ON_TASK;
					return pid_from_task(dest_task);
				}
			}
		}
	}

	if (turnstile_is_receive_turnstile(ts)) {
		ipc_port_t port = (ipc_port_t)ts->ts_proprietor;
		if (port && ip_active(port)) {
			if (ip_mq_lock_held_kdp(port)) {
				*flags |= STACKSHOT_TURNSTILE_STATUS_HELD_IPLOCK;
				return 0;
			}
			if (port->ip_specialreply) {
				/* try getting the pid stored in the port */
				uint64_t pid_candidate = ipc_special_reply_get_pid_locked(port);

				if (pid_candidate) {
					*flags |= STACKSHOT_TURNSTILE_STATUS_BLOCKED_ON_TASK;
					return pid_candidate;
				}
			}
		}
	}

	*hops = unknown_hops;
	*flags |= STACKSHOT_TURNSTILE_STATUS_UNKNOWN;
	return 0;
}

void
kdp_turnstile_fill_tsinfo(struct turnstile *ts, thread_turnstileinfo_v2_t *tsinfo, struct ipc_service_port_label **isplp)
{
	uint64_t final_inheritor;
	uint64_t flags = 0;
	uint8_t hops = 0;

	tsinfo->turnstile_context  = 0;
	tsinfo->number_of_hops     = 0;
	tsinfo->turnstile_priority = 0;

	assert(ts != TURNSTILE_NULL);

	if (waitq_held(&ts->ts_waitq)) {
		tsinfo->turnstile_flags |= STACKSHOT_TURNSTILE_STATUS_LOCKED_WAITQ;
		return;
	}

	final_inheritor = kdp_turnstile_traverse_inheritor_chain(ts, &flags, &hops, isplp);

	/* store some metadata about the turnstile itself */
	tsinfo->turnstile_flags = flags;
	tsinfo->number_of_hops = hops;
	tsinfo->turnstile_priority = ts->ts_priority;
	tsinfo->turnstile_context = final_inheritor;
}

#if DEVELOPMENT || DEBUG

int sysctl_io_opaque(void *req, void *pValue, size_t valueSize, int *changed);

/*
 * Name: turnstile_get_boost_stats_sysctl
 *
 * Description: Function to get turnstile stats.
 *
 * Args: req : opaque struct to pass to sysctl_io_opaque
 *
 * Returns: errorno
 */
int
turnstile_get_boost_stats_sysctl(
	void *req)
{
	return sysctl_io_opaque(req, turnstile_boost_stats, sizeof(struct turnstile_stats) * TURNSTILE_MAX_HOP_DEFAULT, NULL);
}

/*
 * Name: get_turnstile_stats_sysctl
 *
 * Description: Function to get turnstile stats.
 *
 * Args: req : opaque struct to pass to sysctl_io_opaque
 *
 * Returns: errorno
 */
int
turnstile_get_unboost_stats_sysctl(
	void *req)
{
	return sysctl_io_opaque(req, turnstile_unboost_stats, sizeof(struct turnstile_stats) * TURNSTILE_MAX_HOP_DEFAULT, NULL);
}

/* Testing interface for Development kernels */
#define tstile_test_prim_lock_interlock(test_prim) \
	lck_spin_lock(&test_prim->ttprim_interlock)
#define tstile_test_prim_unlock_interlock(test_prim) \
	lck_spin_unlock(&test_prim->ttprim_interlock)

static void
tstile_test_prim_init(struct tstile_test_prim *test_prim)
{
	test_prim->ttprim_turnstile = TURNSTILE_NULL;
	test_prim->ttprim_owner = NULL;
	lck_spin_init(&test_prim->ttprim_interlock, &turnstiles_dev_lock_grp, LCK_ATTR_NULL);
	test_prim->tt_prim_waiters = 0;
}

int
tstile_test_prim_lock(int val)
{
	struct tstile_test_prim *test_prim;
	boolean_t use_hashtable;
	turnstile_type_t type;
	wait_interrupt_t wait_type;

	switch (val) {
	case SYSCTL_TURNSTILE_TEST_USER_DEFAULT:
		test_prim = &test_prim_ts_inline;
		use_hashtable = FALSE;
		wait_type = THREAD_ABORTSAFE;
		type = TURNSTILE_ULOCK;
		break;
	case SYSCTL_TURNSTILE_TEST_USER_HASHTABLE:
		test_prim = &test_prim_global_htable;
		use_hashtable = TRUE;
		wait_type = THREAD_ABORTSAFE;
		type = TURNSTILE_ULOCK;
		break;
	case SYSCTL_TURNSTILE_TEST_KERNEL_DEFAULT:
		test_prim = &test_prim_global_ts_kernel;
		use_hashtable = FALSE;
		wait_type = THREAD_UNINT | THREAD_WAIT_NOREPORT_USER;
		type = TURNSTILE_KERNEL_MUTEX;
		break;
	case SYSCTL_TURNSTILE_TEST_KERNEL_HASHTABLE:
		test_prim = &test_prim_global_ts_kernel_hash;
		use_hashtable = TRUE;
		wait_type = THREAD_UNINT | THREAD_WAIT_NOREPORT_USER;
		type = TURNSTILE_KERNEL_MUTEX;
		break;

	default:
		return -1;
	}

lock_start:

	/* take the interlock of the primitive */
	tstile_test_prim_lock_interlock(test_prim);

	/* Check if the lock is available */
	if (test_prim->ttprim_owner == NULL && test_prim->tt_prim_waiters == 0) {
		thread_reference(current_thread());
		test_prim->ttprim_owner = current_thread();
		tstile_test_prim_unlock_interlock(test_prim);
		return 0;
	}

	struct turnstile *prim_turnstile = TURNSTILE_NULL;

	/* primitive locked, get a turnstile */
	if (use_hashtable) {
		prim_turnstile = turnstile_prepare_hash((uintptr_t)test_prim, type);
	} else {
		prim_turnstile = turnstile_prepare((uintptr_t)test_prim,
		    &test_prim->ttprim_turnstile, TURNSTILE_NULL, type);
	}

	assert(prim_turnstile != TURNSTILE_NULL);

	/* This is contented acquire case */
	if (test_prim->ttprim_owner == NULL) {
		thread_reference(current_thread());
		test_prim->ttprim_owner = current_thread();

		/* Update the turnstile owner */
		turnstile_update_inheritor(prim_turnstile,
		    current_thread(),
		    (TURNSTILE_IMMEDIATE_UPDATE | TURNSTILE_INHERITOR_THREAD));

		turnstile_update_inheritor_complete(prim_turnstile, TURNSTILE_INTERLOCK_HELD);

		if (use_hashtable) {
			turnstile_complete_hash((uintptr_t)test_prim, type);
		} else {
			turnstile_complete((uintptr_t)test_prim,
			    &test_prim->ttprim_turnstile, NULL, type);
		}

		tstile_test_prim_unlock_interlock(test_prim);

		turnstile_cleanup();
		return 0;
	}

	test_prim->tt_prim_waiters++;
	turnstile_update_inheritor(prim_turnstile,
	    test_prim->ttprim_owner,
	    (TURNSTILE_DELAYED_UPDATE | TURNSTILE_INHERITOR_THREAD));

	waitq_assert_wait64(&prim_turnstile->ts_waitq,
	    CAST_EVENT64_T(test_prim), wait_type,
	    TIMEOUT_WAIT_FOREVER);

	/* drop the interlock */
	tstile_test_prim_unlock_interlock(test_prim);

	turnstile_update_inheritor_complete(prim_turnstile, TURNSTILE_INTERLOCK_NOT_HELD);

	wait_result_t result;
	result = thread_block(THREAD_CONTINUE_NULL);

	/* re-acquire the interlock to get turnstile back */
	tstile_test_prim_lock_interlock(test_prim);
	test_prim->tt_prim_waiters--;

	if (use_hashtable) {
		turnstile_complete_hash((uintptr_t)test_prim, type);
	} else {
		turnstile_complete((uintptr_t)test_prim,
		    &test_prim->ttprim_turnstile, NULL, type);
	}

	tstile_test_prim_unlock_interlock(test_prim);

	turnstile_cleanup();

	/* Return if thread interrupted */
	if (result == THREAD_INTERRUPTED) {
		return 1;
	}

	goto lock_start;
}

int
tstile_test_prim_unlock(int val)
{
	struct tstile_test_prim *test_prim;
	boolean_t use_hashtable;
	turnstile_type_t type;

	switch (val) {
	case SYSCTL_TURNSTILE_TEST_USER_DEFAULT:
		test_prim = &test_prim_ts_inline;
		use_hashtable = FALSE;
		type = TURNSTILE_ULOCK;
		break;
	case SYSCTL_TURNSTILE_TEST_USER_HASHTABLE:
		test_prim = &test_prim_global_htable;
		use_hashtable = TRUE;
		type = TURNSTILE_ULOCK;
		break;
	case SYSCTL_TURNSTILE_TEST_KERNEL_DEFAULT:
		test_prim = &test_prim_global_ts_kernel;
		use_hashtable = FALSE;
		type = TURNSTILE_KERNEL_MUTEX;
		break;
	case SYSCTL_TURNSTILE_TEST_KERNEL_HASHTABLE:
		test_prim = &test_prim_global_ts_kernel_hash;
		use_hashtable = TRUE;
		type = TURNSTILE_KERNEL_MUTEX;
		break;
	default:
		return -1;
	}

	/* take the interlock of the primitive */
	tstile_test_prim_lock_interlock(test_prim);

	if (test_prim->ttprim_owner == NULL) {
		tstile_test_prim_unlock_interlock(test_prim);
		return 1;
	}

	/* Check if the lock is contended */
	if (test_prim->ttprim_owner != NULL && test_prim->tt_prim_waiters == 0) {
		/* lock is not contended */
		thread_t old_owner = test_prim->ttprim_owner;
		test_prim->ttprim_owner = NULL;
		tstile_test_prim_unlock_interlock(test_prim);

		thread_deallocate(old_owner);
		return 0;
	}

	struct turnstile *prim_turnstile = TURNSTILE_NULL;

	thread_t old_owner = test_prim->ttprim_owner;
	test_prim->ttprim_owner = NULL;

	/* primitive locked, get a turnstile */
	if (use_hashtable) {
		prim_turnstile = turnstile_prepare_hash((uintptr_t)test_prim, type);
	} else {
		prim_turnstile = turnstile_prepare((uintptr_t)test_prim,
		    &test_prim->ttprim_turnstile, TURNSTILE_NULL, type);
	}

	assert(prim_turnstile != TURNSTILE_NULL);

	/* Update the turnstile owner */
	turnstile_update_inheritor(prim_turnstile,
	    NULL,
	    (TURNSTILE_IMMEDIATE_UPDATE | TURNSTILE_INHERITOR_THREAD));

	waitq_wakeup64_one(&prim_turnstile->ts_waitq,
	    CAST_EVENT64_T(test_prim),
	    THREAD_AWAKENED, WAITQ_WAKEUP_DEFAULT);

	turnstile_update_inheritor_complete(prim_turnstile, TURNSTILE_INTERLOCK_HELD);

	if (use_hashtable) {
		turnstile_complete_hash((uintptr_t)test_prim, type);
	} else {
		turnstile_complete((uintptr_t)test_prim,
		    &test_prim->ttprim_turnstile, NULL, type);
	}

	tstile_test_prim_unlock_interlock(test_prim);

	turnstile_cleanup();

	if (old_owner) {
		/* Changing this to thread_deallocate_safe to exercise thread_deallocate_safe path */
		thread_deallocate_safe(old_owner);
	}

	return 0;
}

#endif
