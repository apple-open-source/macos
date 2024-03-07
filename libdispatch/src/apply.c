/*
 * Copyright (c) 2008-2013 Apple Inc. All rights reserved.
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

#include "internal.h"

typedef void (*dispatch_apply_function_t)(void *, size_t);

static char const * const _dispatch_apply_key = "apply";

/* Really only useful to this file. */
typedef struct dispatch_apply_worker_context_s {
	dispatch_apply_t da;
	long invoke_flags;
	uint32_t worker_index;
} dispatch_apply_worker_context_s, *dispatch_apply_worker_context_t;

#define DISPATCH_APPLY_INVOKE_REDIRECT 0x1
#define DISPATCH_APPLY_INVOKE_WAIT     0x2

/* flags for da_dc->dc_data
 *
 * continuation func is a dispatch_apply_function_t (args: item)
 */
#define DA_FLAG_APPLY					0x01ul
// contin func is a dispatch_apply_attr_function_t (args: item, worker idx)
#define DA_FLAG_APPLY_WITH_ATTR			0x02ul

#if DISPATCH_APPLY_USE_CONTINUATION_ALLOCATOR
dispatch_static_assert(sizeof(struct dispatch_apply_s) <= sizeof(struct dispatch_continuation_s),
		"Apply struct should fit inside continuation struct so we can borrow the continuation allocator");
#endif

DISPATCH_ALWAYS_INLINE DISPATCH_MALLOC
static inline dispatch_apply_t
_dispatch_apply_alloc(void)
{
	dispatch_apply_t da;
#if DISPATCH_APPLY_USE_CONTINUATION_ALLOCATOR
	da = (__typeof__(da))_dispatch_continuation_alloc();
#else // DISPATCH_APPLY_USE_CONTINUATION_ALLOCATOR
	da = _dispatch_calloc(1, sizeof(*da));
#endif // DISPATCH_APPLY_USE_CONTINUATION_ALLOCATOR
	return da;
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_apply_free(dispatch_apply_t da)
{
#if DISPATCH_APPLY_USE_CONTINUATION_ALLOCATOR
	_dispatch_continuation_free((dispatch_continuation_t)da);
#else // DISPATCH_APPLY_USE_CONTINUATION_ALLOCATOR
	free(da);
#endif // DISPATCH_APPLY_USE_CONTINUATION_ALLOCATOR
}

static void _dispatch_apply_da_copy_attr(dispatch_apply_t, dispatch_apply_attr_t _Nullable);
static bool _dispatch_attr_is_initialized(dispatch_apply_attr_t attr);

static void
_dispatch_apply_set_attr_behavior(dispatch_apply_attr_t _Nullable attr, size_t worker_index)
{
	if (!attr) {
		return;
	}
	if (attr->per_cluster_parallelism > 0) {
		_dispatch_attr_apply_cluster_set(worker_index, attr->per_cluster_parallelism);
	}
}

static void
_dispatch_apply_clear_attr_behavior(dispatch_apply_attr_t _Nullable attr, size_t worker_index)
{
	if (!attr) {
		return;
	}
	if (attr->per_cluster_parallelism > 0) {
		_dispatch_attr_apply_cluster_clear(worker_index, attr->per_cluster_parallelism);
	}
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_apply_destroy(dispatch_apply_t da)
{
#if DISPATCH_INTROSPECTION
	_dispatch_continuation_free(da->da_dc);
#endif
	if (da->da_once_gates) {
		free(da->da_once_gates);
	}
	if (da->da_attr) {
		dispatch_apply_attr_destroy(da->da_attr);
		free(da->da_attr);
	}
	_dispatch_apply_free(da);
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_apply_invoke3(void *opaque)
{
	dispatch_apply_worker_context_t context = (dispatch_apply_worker_context_t)opaque;
	dispatch_apply_t da = context->da;

	size_t const iter = da->da_iterations;
	size_t idx, done = 0;

	idx = os_atomic_inc_orig(&da->da_index, relaxed);
	if (unlikely(idx >= iter)) return;

	/*
	 * da_dc lives on the stack of the thread calling dispatch_apply.
	 *
	 * da lives until the last worker thread has finished (protected by
	 * da_thr_cnt), but da_dc only lives until the calling thread returns.
	 * da->da_once_gates[worker_index] guarantees that an individual worker
	 * should never reach here if the calling thread has already left.
	 */
	dispatch_function_t const func = da->da_dc->dc_func;
	void *const da_ctxt = da->da_dc->dc_ctxt;
	uintptr_t apply_flags = (uintptr_t)da->da_dc->dc_data;

	_dispatch_perfmon_workitem_dec(); // this unit executes many items

	// Handle nested dispatch_apply rdar://problem/9294578
	dispatch_thread_context_s apply_ctxt = {
		.dtc_key = _dispatch_apply_key,
		.dtc_apply_nesting = da->da_nested,
	};
	_dispatch_thread_context_push(&apply_ctxt);

	dispatch_thread_frame_s dtf;
	dispatch_priority_t old_dbp = 0;
	if (context->invoke_flags & DISPATCH_APPLY_INVOKE_REDIRECT) {
		dispatch_queue_t dq = da->da_dc->dc_other;
		_dispatch_thread_frame_push(&dtf, dq);
		old_dbp = _dispatch_set_basepri(dq->dq_priority);
	}
	dispatch_invoke_flags_t flags = da->da_flags;

	// Striding is the responsibility of the caller.
	do {
		dispatch_invoke_with_autoreleasepool(flags, {
			if (apply_flags & DA_FLAG_APPLY) {
				_dispatch_client_callout2(da_ctxt, idx,
						(dispatch_apply_function_t)func);
			} else if (apply_flags & DA_FLAG_APPLY_WITH_ATTR) {
				_dispatch_client_callout3_a(da_ctxt, idx, context->worker_index,
						(dispatch_apply_attr_function_t)func);
			} else {
				DISPATCH_INTERNAL_CRASH(apply_flags, "apply continuation has invalid flags");
			}
			_dispatch_perfmon_workitem_inc();
			done++;
			idx = os_atomic_inc_orig(&da->da_index, relaxed);
		});
	} while (likely(idx < iter));

	if (context->invoke_flags & DISPATCH_APPLY_INVOKE_REDIRECT) {
		_dispatch_reset_basepri(old_dbp);
		_dispatch_thread_frame_pop(&dtf);
	}
	_dispatch_thread_context_pop(&apply_ctxt);
#if DISPATCH_INTROSPECTION
	os_atomic_sub(&da->da_todo, done, release);
#endif
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_no_op(void *context DISPATCH_UNUSED)
{
	/*
	 * really a no-op.
	 * Refer to _dispatch_apply_invoke2 comments for more details.
	 */
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_apply_invoke2(dispatch_apply_t da, long invoke_flags)
{
	int32_t i = 0;
	/*
	 * Workers start over time but never quit until
	 * there are no more work items left, so we can
	 * allocate an index simply by incrementing.
	 */
	uint32_t worker_index = 0;
	worker_index = os_atomic_inc_orig(&da->da_worker_index, relaxed);

	dispatch_assert(worker_index < (uint32_t)da->da_final_thr_cnt);

	dispatch_apply_worker_context_s context = {da, invoke_flags, worker_index};

	_dispatch_apply_set_attr_behavior(da->da_attr, worker_index);

	/*
	 * (rdar://34322462)
	 * da->da_once_gates[worker_index] is meant to coordinate between
	 * an individual worker thread that this gate belongs to and the
	 * thread that invoked dispatch_apply.
	 *
	 * a. If an individual worker acquires this once-gate, it continues
	 *    to drain work until the job is done. If the thread that called
	 *    dispatch_apply finishes early, the once-gate gives it a chance
	 *    to wait until the worker finishes and propagate priority
	 *    boosts it might have received.
	 *
	 * b. If the thread calling dispatch_apply wins then it avoids the
	 *    individual worker from draining the work.
	 *    There is a subtlety here !!! The thread calling dispatch_apply will
	 *    only start to wait on other worker's once-gates when it knows
	 *    there is no more work left. So if a worker has not come up by then,
	 *    we know there is no work left for it to pick up when it eventually
	 *    arrives. We can thus safely make it avoid draining work.
	 *
	 * Finally, memory for da->da_once_gates[] is calloc'd and every call to
	 * dispatch_once happens-after calloc so fast path for dispatch_once is
	 * guaranteed to see the zero value. This memory lives until the last worker
	 * thread has finished which is protected by da->da_thr_cnt.
	 *
	 */
	dispatch_once_f(&da->da_once_gates[worker_index],
					&context, _dispatch_apply_invoke3);

	_dispatch_apply_clear_attr_behavior(da->da_attr, worker_index);

	if (invoke_flags & DISPATCH_APPLY_INVOKE_WAIT) {
		for (i = 0; i < da->da_final_thr_cnt; i++) {
			if (i == (int32_t)worker_index) continue;
			dispatch_once_f(&da->da_once_gates[i], NULL, _dispatch_no_op);
		}
	}
	if (os_atomic_dec(&da->da_thr_cnt, relaxed) == 0) {
		_dispatch_apply_destroy(da);
	}
}

DISPATCH_NOINLINE
void
_dispatch_apply_invoke(void *ctxt)
{
	_dispatch_apply_invoke2(ctxt, 0);
}

DISPATCH_NOINLINE
static void
_dispatch_apply_invoke_and_wait(void *ctxt)
{
	_dispatch_apply_invoke2(ctxt, DISPATCH_APPLY_INVOKE_WAIT);
	_dispatch_perfmon_workitem_inc();
}

DISPATCH_NOINLINE
void
_dispatch_apply_redirect_invoke(void *ctxt)
{
	_dispatch_apply_invoke2(ctxt, DISPATCH_APPLY_INVOKE_REDIRECT);
}

DISPATCH_ALWAYS_INLINE
static inline dispatch_invoke_flags_t
_dispatch_apply_autorelease_frequency(dispatch_queue_t dq)
{
	dispatch_invoke_flags_t qaf = 0;

	while (dq && !qaf) {
		qaf = _dispatch_queue_autorelease_frequency(dq);
		dq = dq->do_targetq;
	}
	return qaf;
}

DISPATCH_NOINLINE
static void
_dispatch_apply_serial(void *ctxt)
{
	dispatch_apply_t da = (dispatch_apply_t)ctxt;
	dispatch_continuation_t dc = da->da_dc;
	size_t const iter = da->da_iterations;
	dispatch_invoke_flags_t flags;
	size_t idx = 0;

	// no need yet for _set_attr_behavior() for serial applies
	_dispatch_perfmon_workitem_dec(); // this unit executes many items
	flags = _dispatch_apply_autorelease_frequency(dc->dc_other);
	do {
		dispatch_invoke_with_autoreleasepool(flags, {
			if ((uintptr_t)dc->dc_data & DA_FLAG_APPLY) {
				_dispatch_client_callout2(dc->dc_ctxt, idx, (dispatch_apply_function_t)dc->dc_func);
			} else if ((uintptr_t)dc->dc_data & DA_FLAG_APPLY_WITH_ATTR) {
				// when running serially, the only worker is worker number 0
				_dispatch_client_callout3_a(dc->dc_ctxt, idx, 0, (dispatch_apply_attr_function_t)dc->dc_func);
			} else {
				DISPATCH_INTERNAL_CRASH(dc->dc_data, "apply continuation has invalid flags");
			}

			_dispatch_perfmon_workitem_inc();
		});
	} while (++idx < iter);

	_dispatch_apply_destroy(da);
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_apply_f(dispatch_queue_global_t dq, dispatch_apply_t da,
		dispatch_function_t func)
{
	int32_t i = 0;
	dispatch_continuation_t head = NULL, tail = NULL;
	pthread_priority_t pp = _dispatch_get_priority();

	// The current thread does not need a continuation
	int32_t continuation_cnt = da->da_thr_cnt - 1;

	dispatch_assert(continuation_cnt);

	for (i = 0; i < continuation_cnt; i++) {
		dispatch_continuation_t next = _dispatch_continuation_alloc();
		uintptr_t dc_flags = DC_FLAG_CONSUME;

		_dispatch_continuation_init_f(next, dq, da, func,
				DISPATCH_BLOCK_HAS_PRIORITY, dc_flags);
		next->dc_priority = pp | _PTHREAD_PRIORITY_ENFORCE_FLAG;
		next->do_next = head;
		head = next;

		if (!tail) {
			tail = next;
		}
	}

	da->da_final_thr_cnt = da->da_thr_cnt;
	/*
	 * Technically thread calling dispatch_apply does not need a gate for itself;
	 * but, we could reuse da->da_worker_index to assign a gate to an individual
	 * worker with this approach.
	 * Also, we know da_thr_cnt > 0 so should be safe to do the following conversion.
	 */
	da->da_once_gates = _dispatch_calloc((unsigned long)da->da_thr_cnt,
										sizeof(dispatch_once_t));

	// FIXME: dq may not be the right queue for the priority of `head`
	_dispatch_trace_item_push_list(dq, head, tail);
	_dispatch_root_queue_push_inline(dq, head, tail, continuation_cnt);
	// Call the first element directly
	_dispatch_apply_invoke_and_wait(da);
}

DISPATCH_ALWAYS_INLINE DISPATCH_WARN_RESULT
static inline int32_t
_dispatch_queue_try_reserve_apply_width(dispatch_queue_t dq, int32_t da_width)
{
	uint64_t old_state, new_state;
	int32_t width;

	if (unlikely(dq->dq_width == 1)) {
		return 0;
	}

	os_atomic_rmw_loop(&dq->dq_state, old_state, new_state, relaxed, {
		width = (int32_t)_dq_state_available_width(old_state);
		if (unlikely(!width)) {
			os_atomic_rmw_loop_give_up(return 0);
		}
		if (width > da_width) {
			width = da_width;
		}
		new_state = old_state + (uint64_t)width * DISPATCH_QUEUE_WIDTH_INTERVAL;
	});
	return width;
}

DISPATCH_ALWAYS_INLINE
static inline void
_dispatch_queue_relinquish_width(dispatch_queue_t top_dq,
		dispatch_queue_t stop_dq, int32_t da_width)
{
	uint64_t delta = (uint64_t)da_width * DISPATCH_QUEUE_WIDTH_INTERVAL;
	dispatch_queue_t dq = top_dq;

	while (dq != stop_dq) {
		os_atomic_sub(&dq->dq_state, delta, relaxed);
		dq = dq->do_targetq;
	}
}

DISPATCH_NOINLINE
static void
_dispatch_apply_redirect(void *ctxt)
{
	dispatch_apply_t da = (dispatch_apply_t)ctxt;
	int32_t da_width = da->da_thr_cnt - 1;
	dispatch_queue_t top_dq = da->da_dc->dc_other, dq = top_dq;

	do {
		int32_t width = _dispatch_queue_try_reserve_apply_width(dq, da_width);

		if (unlikely(da_width > width)) {
			int32_t excess = da_width - width;
			_dispatch_queue_relinquish_width(top_dq, dq, excess);
			da_width = width;
			if (unlikely(!da_width)) {
				return _dispatch_apply_serial(da);
			}
			da->da_thr_cnt -= excess;
		}
		if (!da->da_flags) {
			/* find first queue in descending target queue order that has
			 * an autorelease frequency set, and use that as the frequency for
			 * this continuation.
			 */
			da->da_flags = _dispatch_queue_autorelease_frequency(dq);
		}
		dq = dq->do_targetq;
	} while (unlikely(dq->do_targetq));

	_dispatch_apply_f(upcast(dq)._dgq, da, _dispatch_apply_redirect_invoke);
	_dispatch_queue_relinquish_width(top_dq, dq, da_width);
}

#define DISPATCH_APPLY_MAX UINT16_MAX // must be < sqrt(SIZE_MAX)

DISPATCH_ALWAYS_INLINE
static inline dispatch_queue_global_t
_dispatch_apply_root_queue(dispatch_queue_t dq)
{
	dispatch_queue_t tq = NULL;

	if (dq) {
		while (unlikely(dq->do_targetq)) {
			tq = dq->do_targetq;

			// If the current root is a custom pri workloop, select it. We have
			// to this check here because custom pri workloops have a fake
			// bottom targetq.
			if (_dispatch_is_custom_pri_workloop(dq)) {
				return upcast(dq)._dgq;
			}

			dq = tq;
		}
	}

	// if the current root queue is a pthread root queue, select it
	if (dq && !_dispatch_is_in_root_queues_array(dq)) {
		return upcast(dq)._dgq;
	}

	pthread_priority_t pp = _dispatch_get_priority();
	dispatch_qos_t qos = _dispatch_qos_from_pp(pp);
	return _dispatch_get_root_queue(qos ? qos : DISPATCH_QOS_DEFAULT, 0);
}

DISPATCH_ALWAYS_INLINE
static inline size_t
_dispatch_apply_calc_thread_count_for_cluster(dispatch_apply_attr_t _Nullable attr, dispatch_qos_t qos)
{
	size_t cluster_max = SIZE_MAX;
	if (attr && attr->per_cluster_parallelism > 0) {
		uint32_t rc = _dispatch_cluster_max_parallelism(qos);
		if (likely(rc > 0)) {
			cluster_max = rc * (uint32_t) (attr->per_cluster_parallelism);
		} else {
			/* if there's no cluster resource parallelism, then our return value
			 * is 0 which means "attr is a meaningless request"
			 */
			cluster_max = 0;
		}
	}
	return cluster_max;
}

DISPATCH_ALWAYS_INLINE
static inline size_t
_dispatch_apply_calc_thread_count(dispatch_apply_attr_t _Nullable attr, size_t nested, dispatch_qos_t qos, bool active)
{
	if (attr && !_dispatch_attr_is_initialized(attr)) {
		DISPATCH_CLIENT_CRASH(attr, "dispatch_apply_attr not initialized using dispatch_apply_attr_init");
	}

	size_t thr_cnt = 0;

	if (likely(!attr)) {
		/* Normal apply: Start with as many threads as the QOS class would
		 * allow. If we are nested inside another apply, account for the fact
		 * that it's calling us N times, so we need to use 1/Nth the threads
		 * we usually would, to stay under the useful parallelism limit.
		 */
		unsigned long flags = active ? DISPATCH_MAX_PARALLELISM_ACTIVE : 0;
		thr_cnt = _dispatch_qos_max_parallelism(qos, flags);
		if (unlikely(nested)) {
			thr_cnt = nested < thr_cnt ? thr_cnt / nested : 1;
		}
	} else {
		/* apply_with_attr: if we are already nested, just go serial.
		 * We should use the minimum of, the max allowed threads for this QOS
		 * level, and the max useful parallel workers based on the requested
		 * attributes (e.g. the number of cluster level resources).
		 */
		if (unlikely(nested)) {
			thr_cnt = 1;
		} else {
			unsigned long flags = active ? DISPATCH_MAX_PARALLELISM_ACTIVE : 0;
			size_t qos_max = _dispatch_qos_max_parallelism(qos, flags);
			size_t cluster_max = _dispatch_apply_calc_thread_count_for_cluster(attr, qos);
			thr_cnt = MIN(qos_max, cluster_max);
		}
	}
	return thr_cnt;
}

static void
_dispatch_apply_with_attr_f(size_t iterations, dispatch_apply_attr_t attr,
		dispatch_queue_t _dq, void *ctxt, dispatch_function_t func, uintptr_t da_flags)
{
	if (unlikely(iterations == 0)) {
		return;
	}
	if (attr && !_dispatch_attr_is_initialized(attr)) {
		DISPATCH_CLIENT_CRASH(attr, "dispatch_apply_attr not initialized using dispatch_apply_attr_init");
	}
	dispatch_thread_context_t dtctxt =
			_dispatch_thread_context_find(_dispatch_apply_key);
	size_t nested = dtctxt ? dtctxt->dtc_apply_nesting : 0;
	dispatch_queue_t old_dq = _dispatch_queue_get_current();
	dispatch_queue_t dq;

	if (likely(_dq == DISPATCH_APPLY_AUTO)) {
		dq = _dispatch_apply_root_queue(old_dq)->_as_dq;
	} else {
		dq = _dq; // silence clang Nullability complaints
	}
	dispatch_qos_t qos = _dispatch_priority_qos(dq->dq_priority) ?:
			_dispatch_priority_fallback_qos(dq->dq_priority);
	if (unlikely(dq->do_targetq)) {
		/* if the queue passed-in is not a root queue, use the current QoS
		 * since the caller participates in the work anyway
		 */
		qos = _dispatch_qos_from_pp(_dispatch_get_priority());
	}

	size_t thr_cnt = _dispatch_apply_calc_thread_count(attr, nested, qos, true);
	if (thr_cnt == 0) {
		DISPATCH_CLIENT_CRASH(attr, "attribute's properties are invalid or meaningless on this system");
	}

	/* dispatch_apply's nesting behavior is a little complicated; it tries to
	 * account for the multiplicative effect of the applies above it to bring
	 * up just the right number of total threads.
	 * dispatch_apply_with_attr is much simpler: it just goes serial if it is
	 * nested at all, and it sets the nested TSD to the max value to indicate
	 * that we are already saturating the CPUs so any applies nested inside
	 * it will also go serial.
	 */
	size_t new_nested;
	if (attr) {
		new_nested = DISPATCH_APPLY_MAX;
	} else {
		if (likely(!nested)) {
			new_nested = iterations;
		} else {
			/* DISPATCH_APPLY_MAX is sqrt(size_max) so we can do this
			 * multiplication without checking for overlow. The actual magnitude
			 * isn't important, it just needs to be >> ncpu.
			 */
			new_nested = nested < DISPATCH_APPLY_MAX && iterations < DISPATCH_APPLY_MAX
					? nested * iterations : DISPATCH_APPLY_MAX;
		}
	}

	/* Notwithstanding any of the above, we should never try to start more
	 * threads than the number of work items. (The excess threads would have
	 * no work to do.)
	 */
	if (iterations < thr_cnt) {
		thr_cnt = iterations;
	}

	struct dispatch_continuation_s dc = {
		.dc_func = (void*)func,
		.dc_ctxt = ctxt,
		.dc_other = dq,
		.dc_data = (void *)da_flags,
	};
	dispatch_apply_t da = _dispatch_apply_alloc();
	os_atomic_init(&da->da_index, 0);
	da->da_iterations = iterations;
	da->da_nested = new_nested;
	da->da_thr_cnt = (int32_t)thr_cnt;
	os_atomic_init(&da->da_worker_index, 0);
	_dispatch_apply_da_copy_attr(da, attr);
	da->da_once_gates = NULL;
#if DISPATCH_INTROSPECTION
	os_atomic_init(&da->da_todo, iterations);
	da->da_dc = _dispatch_continuation_alloc();
	da->da_dc->dc_func = (void *) dc.dc_func;
	da->da_dc->dc_ctxt = dc.dc_ctxt;
	da->da_dc->dc_other = dc.dc_other;
	da->da_dc->dc_data = dc.dc_data;

	da->da_dc->dc_flags = DC_FLAG_ALLOCATED;
#else
	da->da_dc = &dc;
#endif
	da->da_flags = 0;

	if (unlikely(_dispatch_is_custom_pri_workloop(dq))) {
		uint64_t dq_state = os_atomic_load(&dq->dq_state, relaxed);

		if (_dq_state_drain_locked_by_self(dq_state)) {
			// We're already draining on the custom priority workloop, don't go
			// wide, just call inline serially
			return _dispatch_apply_serial(da);
		} else {
			return dispatch_async_and_wait_f(dq, da, _dispatch_apply_serial);
		}
	}

	if (unlikely(dq->dq_width == 1 || thr_cnt <= 1)) {
		return dispatch_sync_f(dq, da, _dispatch_apply_serial);
	}

	if (unlikely(dq->do_targetq)) {
		if (unlikely(dq == old_dq)) {
			return dispatch_sync_f(dq, da, _dispatch_apply_serial);
		} else {
			return dispatch_sync_f(dq, da, _dispatch_apply_redirect);
		}
	}

	dispatch_thread_frame_s dtf;
	_dispatch_thread_frame_push(&dtf, dq);
	_dispatch_apply_f(upcast(dq)._dgq, da, _dispatch_apply_invoke);
	_dispatch_thread_frame_pop(&dtf);
}

DISPATCH_NOINLINE
void
dispatch_apply_f(size_t iterations, dispatch_queue_t _dq, void *ctxt,
		void (*func)(void *, size_t))
{
	_dispatch_apply_with_attr_f(iterations, NULL, _dq, ctxt, (dispatch_function_t)func, DA_FLAG_APPLY);
}

void
dispatch_apply_with_attr_f(size_t iterations, dispatch_apply_attr_t attr, void *ctxt,
		void (*func)(void *, size_t, size_t))
{
	_dispatch_apply_with_attr_f(iterations, attr, DISPATCH_APPLY_AUTO, ctxt, (dispatch_function_t)func, DA_FLAG_APPLY_WITH_ATTR);
}

#ifdef __BLOCKS__
void
dispatch_apply(size_t iterations, dispatch_queue_t dq, void (^work)(size_t))
{
	// We need to do Block_copy here since any __block variables in work need to
	// be copied over to the heap in a single threaded context. See
	// rdar://77167979
	work = _dispatch_Block_copy(work);
	dispatch_apply_f(iterations, dq, work,
			(dispatch_apply_function_t)_dispatch_Block_invoke(work));
	Block_release(work);
}

void
dispatch_apply_with_attr(size_t iterations, dispatch_apply_attr_t attr,
	void (^work)(size_t iteration, size_t worker_index))
{
	work = _dispatch_Block_copy(work);
	dispatch_apply_with_attr_f(iterations, attr, work,
			(dispatch_apply_attr_function_t)_dispatch_Block_invoke(work));
	Block_release(work);
}
#endif

static bool
_dispatch_attr_is_initialized(dispatch_apply_attr_t attr)
{
	return (attr->sig == DISPATCH_APPLY_ATTR_SIG) && (~(attr->guard) == (uintptr_t) attr);
}

void
dispatch_apply_attr_init(dispatch_apply_attr_t _Nonnull attr)
{
	bzero(attr, sizeof(*attr));

	attr->sig = DISPATCH_APPLY_ATTR_SIG;
	attr->guard = ~ (uintptr_t) (attr); /* To prevent leaks from picking it up */
}

void
dispatch_apply_attr_destroy(dispatch_apply_attr_t _Nonnull attr)
{
	bzero(attr, sizeof(*attr));
}

static void
_dispatch_apply_da_copy_attr(dispatch_apply_t da, dispatch_apply_attr_t _Nullable src)
{
	if (src == NULL) {
		da->da_attr = NULL;
		return;
	}
	dispatch_apply_attr_t dst = _dispatch_calloc(1, sizeof(struct dispatch_apply_attr_s));
	dispatch_apply_attr_init(dst);

	dst->per_cluster_parallelism = src->per_cluster_parallelism;
	dst->flags = src->flags;
	// if there were non-POD types, we would manage them here

	da->da_attr = dst;
}

static void
dispatch_apply_attr_set_per_cluster_parallelism(dispatch_apply_attr_t _Nonnull attr,
	size_t threads_per_cluster)
{
	if (threads_per_cluster == 0) {
		DISPATCH_CLIENT_CRASH(threads_per_cluster, "0 is an invalid threads_per_cluster value");
	}
	if (threads_per_cluster > 1) {
		DISPATCH_CLIENT_CRASH(threads_per_cluster, "Invalid threads_per_cluster value, only acceptable value is 1");
	}

	if (attr && !_dispatch_attr_is_initialized(attr)) {
		DISPATCH_CLIENT_CRASH(attr, "dispatch_apply_attr not initialized using dispatch_apply_attr_init");
	}

	attr->per_cluster_parallelism = threads_per_cluster;
}

void
dispatch_apply_attr_set_parallelism(dispatch_apply_attr_t _Nonnull attr,
	dispatch_apply_attr_entity_t entity, size_t threads_per_entity)
{
	switch (entity) {
	case DISPATCH_APPLY_ATTR_ENTITY_CPU:
		if (threads_per_entity != 1) {
			DISPATCH_CLIENT_CRASH(threads_per_entity, "Invalid threads_per_entity value for CPU entity");
		}
		break;
	case DISPATCH_APPLY_ATTR_ENTITY_CLUSTER:
		return dispatch_apply_attr_set_per_cluster_parallelism(attr, threads_per_entity);
	default:
		DISPATCH_CLIENT_CRASH(entity, "Unknown entity");
	}
}

size_t
dispatch_apply_attr_query(dispatch_apply_attr_t attr,
		dispatch_apply_attr_query_t which,
		dispatch_apply_attr_query_flags_t flags)
{
	dispatch_thread_context_t dtctxt = _dispatch_thread_context_find(_dispatch_apply_key);
	size_t current_nested = dtctxt ? dtctxt->dtc_apply_nesting : 0;
	dispatch_queue_t old_dq = _dispatch_queue_get_current();
	dispatch_queue_t dq = _dispatch_apply_root_queue(old_dq)->_as_dq;
	dispatch_qos_t current_qos = _dispatch_priority_qos(dq->dq_priority) ?: _dispatch_priority_fallback_qos(dq->dq_priority);

	switch (which) {
	case DISPATCH_APPLY_ATTR_QUERY_VALID:
		return (dispatch_apply_attr_query(attr, DISPATCH_APPLY_ATTR_QUERY_MAXIMUM_WORKERS, flags) == 0 ? 0 : 1);
	case DISPATCH_APPLY_ATTR_QUERY_LIKELY_WORKERS:
		return _dispatch_apply_calc_thread_count(attr, current_nested, current_qos, true);
	case DISPATCH_APPLY_ATTR_QUERY_MAXIMUM_WORKERS:
		if (flags & DISPATCH_APPLY_ATTR_QUERY_FLAGS_MAX_CURRENT_SCOPE) {
			return _dispatch_apply_calc_thread_count(attr, current_nested, current_qos, true);
		} else {
			/* we SHOULD pass DISPATCH_QOS_UNSPECIFIED - the intention is "at any
			 * possible QOS", more exactly, "at the QOS which has highest limits".
			 * bsdthread_ctl_qos_max_parallelism doesn't accept unspecified,
			 * though, so let's say USER_INTERACTIVE assuming the highest QOS
			 * will be the least limited one.
			 * <rdar://problem/71964119>
			 */
			return _dispatch_apply_calc_thread_count(attr, 0, DISPATCH_QOS_USER_INTERACTIVE, false);
		}
	}
}
