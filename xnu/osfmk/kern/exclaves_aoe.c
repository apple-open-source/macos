/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#if CONFIG_EXCLAVES

#include <kern/debug.h>
#include <kern/sched_prim.h>
#include <kern/queue.h>

#include <mach/task.h>

#include <Exclaves/Exclaves.h>

#include "exclaves_aoe.h"
#include "exclaves_boot.h"
#include "exclaves_resource.h"
#include "exclaves_debug.h"

#include "kern/exclaves.tightbeam.h"

#define EXCLAVES_AOE_PROXY "com.apple.service.AlwaysOnExclavesProxy"

static exclavesmessagequeueproxy_exclavesmessagequeueproxy_s aoeproxy_client;

static kern_return_t
exclaves_aoe_boot(void)
{
	exclaves_id_t aoeproxy_id = exclaves_service_lookup(
		EXCLAVES_DOMAIN_KERNEL, EXCLAVES_AOE_PROXY);

	if (aoeproxy_id == EXCLAVES_INVALID_ID) {
		/*
		 * For now just silently return if the AOE proxy can't be found.
		 * In future this should call:
		 *    exclaves_requirement_assert(EXCLAVES_R_AOE,
		 *        "exclaves always on exclave proxy not found");
		 */
		return KERN_SUCCESS;
	}

	tb_endpoint_t ep = tb_endpoint_create_with_value(
		TB_TRANSPORT_TYPE_XNU, aoeproxy_id, TB_ENDPOINT_OPTIONS_NONE);

	tb_error_t ret =
	    exclavesmessagequeueproxy_exclavesmessagequeueproxy__init(&aoeproxy_client, ep);
	if (ret != TB_ERROR_SUCCESS) {
		return KERN_FAILURE;
	}

	return KERN_SUCCESS;
}
EXCLAVES_BOOT_TASK(exclaves_aoe_boot, EXCLAVES_BOOT_RANK_ANY);

kern_return_t
exclaves_aoe_setup(uint8_t *num_message, uint8_t *num_worker)
{
	exclaves_resource_t *conclave = task_get_conclave(current_task());
	assert3p(conclave, !=, NULL);

	/* Return with an error if uninitialised. */
	if (aoeproxy_client.connection == NULL) {
		return KERN_NOT_SUPPORTED;
	}

	lck_mtx_lock(&conclave->r_mutex);

	if (!queue_empty(&conclave->r_conclave.c_aoe_q)) {
		lck_mtx_unlock(&conclave->r_mutex);
		return KERN_FAILURE; /* Already initialised. */
	}

	/*
	 * Iterate over each AOE Service in the conclave and call setup for each
	 * one.
	 */

	__block uint8_t nmessage = 0;
	__block uint8_t nworker = 0;
	__block bool saw_error = false;
	__block tb_error_t ret = TB_ERROR_SUCCESS;

	/* BEGIN IGNORE CODESTYLE */
	exclaves_resource_aoeservice_iterate(conclave->r_name,
	    ^(exclaves_resource_t *aoe_service) {

		ret = exclavesmessagequeueproxy_exclavesmessagequeueproxy_setup(
		    &aoeproxy_client, aoe_service->r_id,
		    ^(exclavesmessagequeueproxy_exclavesmessagequeueproxy_setup__result_s result) {

			exclavesmessagequeuetypes_workercount_s *wc =
			    exclavesmessagequeueproxy_exclavesmessagequeueproxy_setup__result_get_success(&result);
			if (wc != NULL) {

				/*
				 * Allocate an aoe item for each service to be
				 * used as a per-service rendezvous for message
				 * threads and to hold worker counts for worker
				 * requests.
				 */
				aoe_item_t *aitem = kalloc_type(aoe_item_t,
				    Z_WAITOK | Z_ZERO | Z_NOFAIL);
				aitem->aoei_serviceid = aoe_service->r_id;
				aitem->aoei_message_count = 0;
				aitem->aoei_work_count = 0;
				aitem->aoei_worker_count = 0;

				queue_enter(&conclave->r_conclave.c_aoe_q, aitem,
				    aoe_item_t *, aoei_chain);

				nmessage++;
				nworker += *wc;
				return;
			}

			exclavesmessagequeueproxy_proxyerror_s *error =
			    exclavesmessagequeueproxy_exclavesmessagequeueproxy_setup__result_get_failure(&result);
			assert3p(error, !=, NULL);

			exclaves_debug_printf(show_errors,
			    "AOE setup failed for service: %llu (error: %llu)\n",
			    aoe_service->r_id, error->tag);
			saw_error = true;
		});

		/* Break out early for errors. */
		if (saw_error || ret != TB_ERROR_SUCCESS) {
			return (bool)true;
		}

		return (bool)(false);
	});
	/* END IGNORE CODESTYLE */

	if (saw_error || ret != TB_ERROR_SUCCESS) {
		exclaves_aoe_teardown();
		lck_mtx_unlock(&conclave->r_mutex);
		return KERN_FAILURE;
	}

	lck_mtx_unlock(&conclave->r_mutex);

	if (nmessage == 0) {
		return KERN_FAILURE;
	}

	*num_message = nmessage;
	*num_worker = nworker;

	return KERN_SUCCESS;
}

static bool
exclaves_aoe_service_is_idle(const aoe_item_t * const item)
{
	return item->aoei_message_count == 0 && item->aoei_work_count == 0 && item->aoei_worker_count == 0;
}

static void
exclaves_aoe_service_try_take_assertion(exclaves_resource_t * const conclave, aoe_item_t * const item)
{
	assert3p(conclave, !=, NULL);
	LCK_MTX_ASSERT(&conclave->r_mutex, LCK_MTX_ASSERT_OWNED);

	if (item->aoei_assertion_id == 0 && exclaves_aoe_service_is_idle(item)) {
		const char *desc = exclaves_conclave_get_domain(conclave);
		__assert_only IOReturn ret = IOExclaveLPWCreateAssertion(&item->aoei_assertion_id, desc);
		assert3u(ret, ==, kIOReturnSuccess);
	}
}

static void
exclaves_aoe_service_drop_assertion(exclaves_resource_t * const __assert_only conclave, aoe_item_t * const item)
{
	assert3p(conclave, !=, NULL);
	LCK_MTX_ASSERT(&conclave->r_mutex, LCK_MTX_ASSERT_OWNED);

	__assert_only IOReturn ret = IOExclaveLPWReleaseAssertion(item->aoei_assertion_id);
	assert3u(ret, ==, kIOReturnSuccess);
	item->aoei_assertion_id = 0;
}

static void
exclaves_aoe_service_try_drop_assertion(exclaves_resource_t * const __assert_only conclave, aoe_item_t * const item)
{
	assert3p(conclave, !=, NULL);
	LCK_MTX_ASSERT(&conclave->r_mutex, LCK_MTX_ASSERT_OWNED);

	if (item->aoei_assertion_id && exclaves_aoe_service_is_idle(item)) {
		exclaves_aoe_service_drop_assertion(conclave, item);
	}
}

void
exclaves_aoe_teardown(void)
{
	exclaves_resource_t *conclave = task_get_conclave(current_task());
	assert3p(conclave, !=, NULL);

	LCK_MTX_ASSERT(&conclave->r_mutex, LCK_MTX_ASSERT_OWNED);

	aoe_item_t *aitem = NULL;
	while (!queue_empty(&conclave->r_conclave.c_aoe_q)) {
		queue_remove_first(&conclave->r_conclave.c_aoe_q, aitem,
		    aoe_item_t *, aoei_chain);

		exclaves_aoe_service_drop_assertion(conclave, aitem);

		kfree_type(aoe_item_t, aitem);
	}
}

static wait_result_t
exclaves_aoe_claim_work(exclaves_resource_t *conclave,
    exclavesmessagequeuetypes_serviceidentifier_s *id)
{
	while (true) {
		lck_mtx_lock(&conclave->r_mutex);

		aoe_item_t *aitem = NULL;
		queue_iterate(&conclave->r_conclave.c_aoe_q, aitem,
		    aoe_item_t *, aoei_chain) {
			if (aitem->aoei_work_count != 0) {
				aitem->aoei_work_count--;
				aitem->aoei_worker_count++;
				*id = aitem->aoei_serviceid;

				lck_mtx_unlock(&conclave->r_mutex);
				return THREAD_AWAKENED;
			}
		}

		/* Nothing on the work queue, sleep */
		assert_wait(&conclave->r_conclave.c_aoe_q,
		    THREAD_INTERRUPTIBLE);

		lck_mtx_unlock(&conclave->r_mutex);

		wait_result_t wr = thread_block(THREAD_CONTINUE_NULL);
		assert(wr == THREAD_AWAKENED || wr == THREAD_INTERRUPTED);

		if (wr == THREAD_INTERRUPTED) {
			return wr;
		}
	}
}

static void
exclaves_aoe_finish_work(exclaves_resource_t *conclave,
    exclavesmessagequeuetypes_serviceidentifier_s id)
{
	bool work_finished = false;

	lck_mtx_lock(&conclave->r_mutex);

	aoe_item_t *aitem = NULL;
	queue_iterate(&conclave->r_conclave.c_aoe_q, aitem,
	    aoe_item_t *, aoei_chain) {
		if (id == aitem->aoei_serviceid) {
			aitem->aoei_worker_count--;

			exclaves_aoe_service_try_drop_assertion(conclave, aitem);

			work_finished = true;
		}
	}

	lck_mtx_unlock(&conclave->r_mutex);

	assert(work_finished);
}

static void
exclaves_aoe_post_work(exclaves_resource_t *conclave,
    exclavesmessagequeuetypes_serviceidentifier_s service_id, uint8_t worker_count)
{
	lck_mtx_lock(&conclave->r_mutex);

	/* Find the associated aoe item. */
	aoe_item_t *aitem = NULL;
	queue_iterate(&conclave->r_conclave.c_aoe_q, aitem, aoe_item_t *,
	    aoei_chain) {
		if (aitem->aoei_serviceid == service_id) {
			if (worker_count != 0) {
				aitem->aoei_work_count += worker_count;
				thread_wakeup(&conclave->r_conclave.c_aoe_q);
			} else {
				// If there are no workers, check if the active assertion can be dropped.
				exclaves_aoe_service_try_drop_assertion(conclave, aitem);
			}
			break;
		}
	}

	lck_mtx_unlock(&conclave->r_mutex);
}

/*
 * Worker thread run-loop.
 */
kern_return_t
exclaves_aoe_work_loop(void)
{
	uint64_t id =
	    EXCLAVESMESSAGEQUEUETYPES_SERVICEIDENTIFIER_INVALID;
	exclaves_resource_t *conclave = task_get_conclave(current_task());
	assert3p(conclave, !=, NULL);

	/* Return with an error if uninitialised. */
	if (aoeproxy_client.connection == NULL) {
		return KERN_NOT_SUPPORTED;
	}

	/*
	 * Mark this thread as being an Exclaves AOE thread. After this point
	 * cannot return to userspace.
	 */
	current_thread()->options |= TH_OPT_AOE;

	// Wait to be interrupted or aborted..
	while (exclaves_aoe_claim_work(conclave, &id) != THREAD_INTERRUPTED) {
		// Call into AOE proxy to process.

		assert3u(id, !=, EXCLAVESMESSAGEQUEUETYPES_SERVICEIDENTIFIER_INVALID);

		/* BEGIN IGNORE CODESTYLE */
		__assert_only tb_error_t ret = exclavesmessagequeueproxy_exclavesmessagequeueproxy_workerinvoke(
		    &aoeproxy_client, id);

		assert3u(ret, ==, TB_ERROR_SUCCESS);

		exclaves_aoe_finish_work(conclave, id);
	}

	/*
	 * This thread was aborted, assert that the thread has actually aborted
	 * and won't try to return to userspace.
	 */
	assert3u(current_thread()->sched_flags & TH_SFLAG_ABORT, !=, 0);

	return KERN_SUCCESS;
}

static wait_result_t
exclaves_aoe_claim_message(exclaves_resource_t *conclave, aoe_item_t *item)
{
	while (true) {
		lck_mtx_lock(&conclave->r_mutex);

		/* Claim message and return immediately if available. */
		if (item->aoei_message_count > 0) {
			item->aoei_message_count--;
			lck_mtx_unlock(&conclave->r_mutex);
			return THREAD_AWAKENED;
		}

		/* Nothing on the message queue, sleep. */
		assert_wait(&item->aoei_message_count,
		    THREAD_INTERRUPTIBLE);

		lck_mtx_unlock(&conclave->r_mutex);

		wait_result_t wr = thread_block(THREAD_CONTINUE_NULL);
		assert(wr == THREAD_AWAKENED || wr == THREAD_INTERRUPTED);

		if (wr == THREAD_INTERRUPTED) {
			return wr;
		}
	}
}

static void
exclaves_aoe_post_message(exclaves_resource_t *conclave,
    __unused exclavesmessagequeuetypes_serviceidentifier_s id)
{
	lck_mtx_lock(&conclave->r_mutex);

	aoe_item_t *aitem = NULL;
	queue_iterate(&conclave->r_conclave.c_aoe_q, aitem, aoe_item_t *,
	    aoei_chain) {
		if (aitem->aoei_serviceid == id) {
		    exclaves_aoe_service_try_take_assertion(conclave, aitem);

			aitem->aoei_message_count++;
			thread_wakeup(&aitem->aoei_message_count);
			break;
		}
	}

	lck_mtx_unlock(&conclave->r_mutex);
}

static aoe_item_t *
exclaves_aoe_associate_serviceid(void)
{
	exclaves_resource_t *conclave = task_get_conclave(current_task());
	assert3p(conclave, !=, NULL);

	lck_mtx_lock(&conclave->r_mutex);

	aoe_item_t *aitem = NULL;
	queue_iterate(&conclave->r_conclave.c_aoe_q, aitem, aoe_item_t *,
	    aoei_chain) {
		if (!aitem->aoei_associated) {
			aitem->aoei_associated = true;
			lck_mtx_unlock(&conclave->r_mutex);

			return aitem;
		}
	}

	lck_mtx_unlock(&conclave->r_mutex);

	return NULL;
}


/* Message thread run-loop. */
kern_return_t
exclaves_aoe_message_loop(void)
{
	exclaves_resource_t *conclave = task_get_conclave(current_task());
	assert3p(conclave, !=, NULL);

	/* Return with an error if uninitialised. */
	if (aoeproxy_client.connection == NULL) {
		return KERN_NOT_SUPPORTED;
	}

	/* Claim a message endpoint. */
	aoe_item_t *item = exclaves_aoe_associate_serviceid();
	if (item == NULL) {
		return KERN_NOT_FOUND;
	}

	/*
	 * Mark this thread as being an Exclaves AOE thread. After this point
	 * cannot return to userspace.
	 */
	current_thread()->options |= TH_OPT_AOE;

	// Wait to be interrupted or aborted..
	while (exclaves_aoe_claim_message(conclave, item) !=
	    THREAD_INTERRUPTED) {
		// Call into AOE proxy to handle message.

		/* BEGIN IGNORE CODESTYLE */
		__assert_only tb_error_t ret = exclavesmessagequeueproxy_exclavesmessagequeueproxy_messagedeliver(
		    &aoeproxy_client, item->aoei_serviceid,
		    ^(workercount__opt_s wc_opt) {

			exclavesmessagequeuetypes_workercount_s *wc = NULL;
			wc = workercount__opt_get(&wc_opt);

			// Post work for the worker threads.
			exclaves_aoe_post_work(conclave, item->aoei_serviceid, wc ? *wc : 0);
		});
		/* END IGNORE CODESTYLE */

		assert3u(ret, ==, TB_ERROR_SUCCESS);
	}

	/*
	 * This thread was aborted, assert that the thread has actually aborted
	 * and won't try to return to userspace.
	 */
	assert3u(current_thread()->sched_flags & TH_SFLAG_ABORT, !=, 0);

	return KERN_SUCCESS;
}

tb_error_t
exclaves_aoe_upcall_work_available(const xnuupcallsv2_aoeworkinfo_s *work_info,
    tb_error_t (^completion)(void))
{
	assert3p(work_info, !=, NULL);

	const xnuupcallsv2_aoeworkinfo_conclavework_s *cw =
	    xnuupcallsv2_aoeworkinfo_conclavework__get(work_info);

	// Only conclave work is supported right now.
	assert3p(cw, !=, NULL);

	exclavesmessagequeuetypes_serviceidentifier_s id = cw->field0;
	assert3u(id, !=, EXCLAVESMESSAGEQUEUETYPES_SERVICEIDENTIFIER_INVALID);

	exclaves_resource_t *conclave =
	    exclaves_conclave_lookup_by_aoeserviceid(id);
	if (conclave == NULL ||
	    queue_empty(&conclave->r_conclave.c_aoe_q)) {
		exclaves_debug_printf(show_errors,
		    "exclaves: work available but conclave not found or "
		    "uninitialised: %llu\n", id);
		completion();
		return TB_ERROR_USER_FAILURE;
	}

	exclaves_aoe_post_message(conclave, id);

	return completion();
}

#endif /* CONFIG_EXCLAVES */
