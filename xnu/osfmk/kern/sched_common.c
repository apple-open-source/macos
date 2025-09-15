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

#include <os/atomic_private.h>
#include <machine/machine_routines.h>
#include <kern/processor.h>

#include <kern/sched_common.h>

#if __AMP__

void
sched_pset_search_order_compute(sched_pset_search_order_t *search_order_out,
    sched_pset_search_order_sort_data_t *datas, size_t num_datas,
    sched_pset_search_order_sort_cmpfunc_t cmp)
{
	qsort(datas, num_datas, sizeof(sched_pset_search_order_sort_data_t), cmp);
	sched_pset_search_order_t search_order;
	for (int i = 0; i < num_datas; i++) {
		search_order.spso_search_order[i] = datas[i].spsosd_dst_pset_id;
	}
	int num_psets = ml_get_cluster_count();
	for (int i = (int)num_datas; i < num_psets - 1; i++) {
		/*
		 * If fewer sort datas were passed in than the number of psets minus
		 * 1 (AKA the maximum length of a pset search order), then mark the
		 * remaining slots at the end with an invalid pset id.
		 */
		search_order.spso_search_order[i] = PSET_ID_INVALID;
	}
	os_atomic_store_wide(&search_order_out->spso_packed, search_order.spso_packed, relaxed);
}

void
sched_pset_search_order_init(processor_set_t src_pset, sched_pset_search_order_t *search_order_out)
{
	pset_id_t other_pset_id = 0;
	sched_pset_search_order_t spill_order;
	int num_psets = ml_get_cluster_count();
	for (int i = 0; i < MAX_PSETS - 1; i++, other_pset_id++) {
		if (i < num_psets - 1) {
			if (other_pset_id == src_pset->pset_id) {
				/* Exclude the source pset */
				other_pset_id++;
			}
			assert3u(other_pset_id, <, num_psets);
			spill_order.spso_search_order[i] = other_pset_id;
		} else {
			/* Mark unneeded slots with an invalid id, as they should not be accessed */
			spill_order.spso_search_order[i] = PSET_ID_INVALID;
		}
	}
	os_atomic_store_wide(&search_order_out->spso_packed, spill_order.spso_packed, relaxed);
}

bool
sched_iterate_psets_ordered(processor_set_t starting_pset, sched_pset_search_order_t *search_order,
    uint64_t candidate_map, sched_pset_iterate_state_t *istate)
{
	int num_psets = ml_get_cluster_count();
	while (istate->spis_search_index < num_psets - 1) {
		int pset_id;
		if (istate->spis_search_index == -1) {
			/* Initial condition */
			pset_id = starting_pset->pset_id;
			istate->spis_cached_search_order =
			    (sched_pset_search_order_t)os_atomic_load_wide(&search_order->spso_packed, relaxed);
		} else {
			pset_id = istate->spis_cached_search_order.spso_search_order[istate->spis_search_index];
			if (pset_id == PSET_ID_INVALID) {
				/* The given search order does not include all psets */
				break;
			}
			assert3u(pset_id, !=, starting_pset->pset_id);
		}
		istate->spis_search_index++;
		if (bit_test(candidate_map, pset_id)) {
			istate->spis_pset_id = pset_id;
			return true;
		}
	}
	istate->spis_pset_id = -1;
	return false;
}

#endif /* __AMP__ */
