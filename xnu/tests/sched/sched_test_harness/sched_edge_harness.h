// Copyright (c) 2024 Apple Inc.  All rights reserved.

#pragma once

#include "sched_harness_impl.h"
#include "sched_clutch_harness.h"
/* To get sched_clutch_edge and cluster_shared_rsrc_type_t */
#include <kern/kern_types.h>
/* To get PSET_ID_INVALID */
#include <kern/sched_common.h>

extern void edge_set_thread_shared_rsrc(test_thread_t thread, bool native_first);

#pragma mark - Realtime

extern void              sched_rt_config_set(uint8_t src, uint8_t dst, sched_clutch_edge edge);
extern sched_clutch_edge sched_rt_config_get(uint8_t src, uint8_t dst);
extern uint64_t          rt_deadline_add(uint64_t d, uint64_t e);
extern void              rt_pset_recompute_spill_order(int src_pset_id);
extern int               rt_pset_spill_search_order_at_offset(int src_pset_id, int offset);
extern void              sched_rt_spill_policy_set(unsigned policy);
extern void              sched_rt_steal_policy_set(unsigned policy);
