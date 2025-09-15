// Copyright (c) 2024 Apple Inc.  All rights reserved.

#include <stdint.h>
#include <stdbool.h>

/* Edge shares some of its implementation with the Clutch scheduler */
#include "sched_clutch_harness_impl.c"

/* Machine-layer mocking */

processor_t
current_processor(void)
{
	return cpus[_curr_cpu];
}

unsigned int
ml_get_die_id(unsigned int cluster_id)
{
	return curr_hw_topo.psets[cluster_id].die_id;
}

uint64_t
ml_cpu_signal_deferred_get_timer(void)
{
	/* Matching deferred_ipi_timer_ns */
	return 64 * NSEC_PER_USEC;
}

static test_cpu_type_t
cluster_type_to_test_cpu_type(cluster_type_t cluster_type)
{
	switch (cluster_type) {
	case CLUSTER_TYPE_E:
		return TEST_CPU_TYPE_EFFICIENCY;
	case CLUSTER_TYPE_P:
		return TEST_CPU_TYPE_PERFORMANCE;
	default:
		assert(false);
	}
}

static unsigned int cpu_count_for_type[TEST_CPU_TYPE_MAX] = { 0 };
static unsigned int recommended_cpu_count_for_type[TEST_CPU_TYPE_MAX] = { 0 };

unsigned int
ml_get_cpu_number_type(cluster_type_t cluster_type, bool logical, bool available)
{
	(void)logical;
	if (available) {
		return recommended_cpu_count_for_type[cluster_type_to_test_cpu_type(cluster_type)];
	} else {
		return cpu_count_for_type[cluster_type_to_test_cpu_type(cluster_type)];
	}
}

static unsigned int cluster_count_for_type[TEST_CPU_TYPE_MAX] = { 0 };

unsigned int
ml_get_cluster_number_type(cluster_type_t cluster_type)
{
	return cluster_count_for_type[cluster_type_to_test_cpu_type(cluster_type)];
}

int sched_amp_spill_deferred_ipi = 1;
int sched_amp_pcores_preempt_immediate_ipi = 1;

/* Implementation of sched_runqueue_harness.h interface */

static test_pset_t basic_amp_psets[2] = {
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 2,
		.cluster_id = 0,
		.die_id = 0,
	},
	{
		.cpu_type = TEST_CPU_TYPE_EFFICIENCY,
		.num_cpus = 4,
		.cluster_id = 1,
		.die_id = 0,
	},
};
test_hw_topology_t basic_amp = {
	.psets = &basic_amp_psets[0],
	.num_psets = 2,
	.total_cpus = 6,
};

static test_pset_t dual_die_psets[6] = {
	{
		.cpu_type = TEST_CPU_TYPE_EFFICIENCY,
		.num_cpus = 2,
		.cluster_id = 0,
		.die_id = 0,
	},
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 4,
		.cluster_id = 1,
		.die_id = 0,
	},
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 4,
		.cluster_id = 2,
		.die_id = 0,
	},
	{
		.cpu_type = TEST_CPU_TYPE_EFFICIENCY,
		.num_cpus = 2,
		.cluster_id = 3,
		.die_id = 1,
	},
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 4,
		.cluster_id = 4,
		.die_id = 1,
	},
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 4,
		.cluster_id = 5,
		.die_id = 1,
	},
};
test_hw_topology_t dual_die = {
	.psets = &dual_die_psets[0],
	.num_psets = 6,
	.total_cpus = 20,
};

#define MAX_NODES 2

static void
edge_impl_set_cluster_type(processor_set_t pset, test_cpu_type_t type)
{
	switch (type) {
	case TEST_CPU_TYPE_EFFICIENCY:
		pset->pset_cluster_type = PSET_AMP_E;
		pset->node = &pset_nodes[0];
		bitmap_set(&pset_nodes[0].pset_map, pset->pset_cluster_id);
		break;
	case TEST_CPU_TYPE_PERFORMANCE:
		pset->pset_cluster_type = PSET_AMP_P;
		pset->node = &pset_nodes[1];
		bitmap_set(&pset_nodes[1].pset_map, pset->pset_cluster_id);
		break;
	default:
		assert(false);
		break;
	}
}

struct mock_topology_info_struct mock_topology_info;

static void
edge_impl_init_runqueues(void)
{
	assert(curr_hw_topo.num_psets != 0);
	clutch_impl_init_topology(curr_hw_topo);
	mock_topology_info.num_cpus = curr_hw_topo.total_cpus;
	sched_edge_init();
	bzero(pset_nodes, sizeof(pset_nodes));
	pset_nodes[0].pset_cluster_type = PSET_AMP_E;
	pset_nodes[1].pset_cluster_type = PSET_AMP_P;
	for (int i = 0; i < MAX_NODES; i++) {
		os_atomic_store(&pset_nodes[i].pset_recommended_map, 0, relaxed);
	}
	for (int i = 0; i < curr_hw_topo.num_psets; i++) {
		pset_array[i] = psets[i];
		edge_impl_set_cluster_type(psets[i], curr_hw_topo.psets[i].cpu_type);
		sched_edge_pset_init(psets[i]);
		bzero(&psets[i]->pset_load_average, sizeof(psets[i]->pset_load_average));
		bzero(&psets[i]->pset_execution_time, sizeof(psets[i]->pset_execution_time));
		assert(psets[i]->cpu_bitmask != 0);
		psets[i]->foreign_psets[0] = 0;
		psets[i]->native_psets[0] = 0;
		psets[i]->local_psets[0] = 0;
		psets[i]->remote_psets[0] = 0;
		cluster_count_for_type[curr_hw_topo.psets[i].cpu_type]++;
		cpu_count_for_type[curr_hw_topo.psets[i].cpu_type] += curr_hw_topo.psets[i].num_cpus;
		recommended_cpu_count_for_type[curr_hw_topo.psets[i].cpu_type] +=
		    curr_hw_topo.psets[i].num_cpus;
		impl_set_pset_recommended(i);
		psets[i]->cpu_running_foreign = 0;
		for (uint state = 0; state < PROCESSOR_STATE_LEN; state++) {
			psets[i]->cpu_state_map[state] = 0;
		}
		/* Initialize realtime queues */
		pset_rt_init(psets[i]);
	}
	for (unsigned int j = 0; j < processor_avail_count; j++) {
		processor_array[j] = cpus[j];
		sched_clutch_processor_init(cpus[j]);
		os_atomic_store(&cpus[j]->stir_the_pot_inbox_cpu, -1, relaxed);
	}
	sched_edge_cpu_init_completed();
	sched_rt_init_completed();
	increment_mock_time(100);
	clutch_impl_init_params();
	clutch_impl_init_tracepoints();
}

void
impl_init_runqueue(void)
{
	assert(curr_hw_topo.num_psets == 0);
	curr_hw_topo = single_core;
	edge_impl_init_runqueues();
}

void
impl_init_migration_harness(test_hw_topology_t hw_topology)
{
	assert(curr_hw_topo.num_psets == 0);
	curr_hw_topo = hw_topology;
	edge_impl_init_runqueues();
}

struct thread_group *
impl_create_tg(int interactivity_score)
{
	return clutch_impl_create_tg(interactivity_score);
}

test_thread_t
impl_create_thread(int root_bucket, struct thread_group *tg, int pri)
{
	return clutch_impl_create_thread(root_bucket, tg, pri);
}

void
impl_set_thread_processor_bound(test_thread_t thread, int cpu_id)
{
	_curr_cpu = cpu_id;
	clutch_impl_set_thread_processor_bound(thread, cpu_id);
}

void
impl_set_thread_cluster_bound(test_thread_t thread, int cluster_id)
{
	/* Should not be already enqueued */
	assert(thread_get_runq_locked((thread_t)thread) == NULL);
	((thread_t)thread)->th_bound_cluster_id = cluster_id;
}

static void
processor_state_update_running_foreign(processor_t processor, thread_t thread)
{
	cluster_type_t current_processor_type = pset_type_for_id(processor->processor_set->pset_cluster_id);
	cluster_type_t thread_type = pset_type_for_id(sched_edge_thread_preferred_cluster(thread));

	boolean_t non_rt_thr = (processor->current_pri < BASEPRI_RTQUEUES);
	boolean_t non_bound_thr = (thread->bound_processor == PROCESSOR_NULL);
	if (non_rt_thr && non_bound_thr && (current_processor_type != thread_type)) {
		bit_set(processor->processor_set->cpu_running_foreign, processor->cpu_id);
	} else {
		bit_clear(processor->processor_set->cpu_running_foreign, processor->cpu_id);
	}
}

void
impl_cpu_set_thread_current(int cpu_id, test_thread_t thread)
{
	_curr_cpu = cpu_id;
	processor_set_t pset = cpus[cpu_id]->processor_set;
	clutch_impl_cpu_set_thread_current(cpu_id, thread);
	processor_state_update_running_foreign(cpus[cpu_id], (thread_t)thread);
	pset_update_processor_state(pset, cpus[cpu_id], PROCESSOR_RUNNING);
	sched_bucket_t bucket = ((((thread_t)thread)->state & TH_IDLE) || (((thread_t)thread)->bound_processor != PROCESSOR_NULL)) ? TH_BUCKET_SCHED_MAX : ((thread_t)thread)->th_sched_bucket;
	os_atomic_store(&cpus[cpu_id]->processor_set->cpu_running_buckets[cpu_id], bucket, relaxed);
	sched_edge_stir_the_pot_update_registry_state((thread_t)thread);

	/* Send followup IPIs for realtime, as needed */
	bit_clear(pset->rt_pending_spill_cpu_mask, cpu_id);
	processor_t next_rt_processor = PROCESSOR_NULL;
	sched_ipi_type_t next_rt_ipi_type = SCHED_IPI_NONE;
	if (rt_pset_has_stealable_threads(pset)) {
		rt_choose_next_processor_for_spill_IPI(pset, cpus[cpu_id], &next_rt_processor, &next_rt_ipi_type);
	} else if (rt_pset_needs_a_followup_IPI(pset)) {
		rt_choose_next_processor_for_followup_IPI(pset, cpus[cpu_id], &next_rt_processor, &next_rt_ipi_type);
	}
	if (next_rt_processor != PROCESSOR_NULL) {
		sched_ipi_perform(next_rt_processor, next_rt_ipi_type);
	}
}

test_thread_t
impl_cpu_clear_thread_current(int cpu_id)
{
	_curr_cpu = cpu_id;
	test_thread_t thread = clutch_impl_cpu_clear_thread_current(cpu_id);
	pset_update_processor_state(cpus[cpu_id]->processor_set, cpus[cpu_id], PROCESSOR_IDLE);
	os_atomic_store(&cpus[cpu_id]->processor_set->cpu_running_buckets[cpu_id], TH_BUCKET_SCHED_MAX, relaxed);
	sched_edge_stir_the_pot_clear_registry_entry();
	return thread;
}

void
impl_cpu_enqueue_thread(int cpu_id, test_thread_t thread)
{
	_curr_cpu = cpu_id;
	if (((thread_t) thread)->sched_pri >= BASEPRI_RTQUEUES) {
		rt_runq_insert(cpus[cpu_id], cpus[cpu_id]->processor_set, (thread_t) thread);
	} else {
		sched_clutch_processor_enqueue(cpus[cpu_id], (thread_t) thread, SCHED_TAILQ);
	}
}

test_thread_t
impl_cpu_dequeue_thread(int cpu_id)
{
	_curr_cpu = cpu_id;
	test_thread_t chosen_thread = sched_rt_choose_thread(cpus[cpu_id]);
	if (chosen_thread != THREAD_NULL) {
		return chosen_thread;
	}
	/* No realtime threads. */
	return sched_clutch_choose_thread(cpus[cpu_id], MINPRI, NULL, 0);
}

test_thread_t
impl_cpu_dequeue_thread_compare_current(int cpu_id)
{
	_curr_cpu = cpu_id;
	assert(cpus[cpu_id]->active_thread != NULL);
	processor_set_t pset = cpus[cpu_id]->processor_set;
	if (rt_runq_count(pset) > 0) {
		return impl_dequeue_realtime_thread(pset);
	} else {
		return sched_clutch_choose_thread(cpus[cpu_id], MINPRI, cpus[cpu_id]->active_thread, 0);
	}
}

bool
impl_processor_csw_check(int cpu_id)
{
	_curr_cpu = cpu_id;
	assert(cpus[cpu_id]->active_thread != NULL);
	ast_t preempt_ast = sched_clutch_processor_csw_check(cpus[cpu_id]);
	return preempt_ast & AST_PREEMPT;
}

void
impl_pop_tracepoint(uint64_t *clutch_trace_code, uint64_t *arg1, uint64_t *arg2,
    uint64_t *arg3, uint64_t *arg4)
{
	clutch_impl_pop_tracepoint(clutch_trace_code, arg1, arg2, arg3, arg4);
}

int
impl_choose_pset_for_thread(test_thread_t thread)
{
	/* Begins search starting from current pset */
	sched_options_t options = SCHED_NONE;
	processor_t chosen_processor = sched_edge_choose_processor(
		current_processor()->processor_set, current_processor(), (thread_t)thread, &options);
	return chosen_processor->processor_set->pset_id;
}

bool
impl_thread_avoid_processor(test_thread_t thread, int cpu_id, bool quantum_expired)
{
	_curr_cpu = cpu_id;
	return sched_edge_thread_avoid_processor(cpus[cpu_id], (thread_t)thread, quantum_expired ? AST_QUANTUM : AST_NONE);
}

void
impl_cpu_expire_quantum(int cpu_id)
{
	_curr_cpu = cpu_id;
	sched_edge_quantum_expire(cpus[cpu_id]->active_thread);
	cpus[cpu_id]->first_timeslice = FALSE;
}

test_thread_t
impl_steal_thread(int cpu_id)
{
	_curr_cpu = cpu_id;
	return sched_edge_processor_idle(psets[cpu_id_to_pset_id(cpu_id)]);
}

bool
impl_processor_balance(int cpu_id)
{
	_curr_cpu = cpu_id;
	return sched_edge_balance(cpus[cpu_id], psets[cpu_id_to_pset_id(cpu_id)]);
}

void
impl_set_current_processor(int cpu_id)
{
	_curr_cpu = cpu_id;
}

void
impl_set_tg_sched_bucket_preferred_pset(struct thread_group *tg, int sched_bucket, int cluster_id)
{
	assert(sched_bucket < TH_BUCKET_SCHED_MAX);
	sched_clutch_t clutch = sched_clutch_for_thread_group(tg);
	bitmap_t modify_bitmap[BITMAP_LEN(TH_BUCKET_SCHED_MAX)] = {0};
	bitmap_set(modify_bitmap, sched_bucket);
	uint32_t tg_bucket_preferred_cluster[TH_BUCKET_SCHED_MAX] = {0};
	tg_bucket_preferred_cluster[sched_bucket] = cluster_id;
	sched_edge_update_preferred_cluster(clutch, modify_bitmap, tg_bucket_preferred_cluster);
}

void
impl_set_pset_load_avg(int cluster_id, int QoS, uint64_t load_avg)
{
	assert(QoS > 0 && QoS < TH_BUCKET_SCHED_MAX);
	pset_array[cluster_id]->pset_load_average[QoS] = load_avg;
}

void
edge_set_thread_shared_rsrc(test_thread_t thread, bool native_first)
{
	int shared_rsrc_type = native_first ? CLUSTER_SHARED_RSRC_TYPE_NATIVE_FIRST :
	    CLUSTER_SHARED_RSRC_TYPE_RR;
	((thread_t)thread)->th_shared_rsrc_heavy_user[shared_rsrc_type] = true;
}

void
impl_set_pset_derecommended(int cluster_id)
{
	processor_set_t pset = pset_array[cluster_id];
	pset->recommended_bitmask = 0;
	atomic_bit_clear(&pset->node->pset_recommended_map, cluster_id, memory_order_relaxed);
	recommended_cpu_count_for_type[cluster_type_to_test_cpu_type(pset->pset_type)] -=
	    bit_count(pset->cpu_bitmask);
}

void
impl_set_pset_recommended(int cluster_id)
{
	processor_set_t pset = pset_array[cluster_id];
	pset->recommended_bitmask = pset->cpu_bitmask;
	atomic_bit_set(&pset->node->pset_recommended_map, cluster_id, memory_order_relaxed);
	recommended_cpu_count_for_type[cluster_type_to_test_cpu_type(pset->pset_type)] +=
	    bit_count(pset->cpu_bitmask);
}

void
impl_pop_ipi(int *cpu_id, test_ipi_type_t *ipi_type)
{
	assert(expect_ipi_ind < curr_ipi_ind);
	*cpu_id = logged_ipis[expect_ipi_ind].cpu_id;
	*ipi_type = (test_ipi_type_t)logged_ipis[expect_ipi_ind].ipi_type;
	expect_ipi_ind++;
}

bool
impl_thread_should_yield(int cpu_id)
{
	_curr_cpu = cpu_id;
	assert(cpus[cpu_id]->active_thread != NULL);
	return sched_edge_thread_should_yield(cpus[cpu_id], cpus[cpu_id]->active_thread);
}

void
impl_send_ipi(int cpu_id, test_thread_t thread, test_ipi_event_t event)
{
	sched_ipi_type_t triggered_ipi = sched_ipi_action(cpus[cpu_id],
	    (thread_t)thread, (sched_ipi_event_t)event);
	sched_ipi_perform(cpus[cpu_id], triggered_ipi);
}

int
rt_pset_spill_search_order_at_offset(int src_pset_id, int offset)
{
	return psets[src_pset_id]->sched_rt_spill_search_order.spso_search_order[offset];
}

void
rt_pset_recompute_spill_order(int src_pset_id)
{
	sched_rt_config_pset_push(psets[src_pset_id]);
}

uint32_t
impl_qos_max_parallelism(int qos, uint64_t options)
{
	return sched_edge_qos_max_parallelism(qos, options);
}

int *
impl_iterate_pset_search_order(int src_pset_id, uint64_t candidate_map, int sched_bucket)
{
	int *psets = (int *)malloc(sizeof(int) * curr_hw_topo.num_psets);
	for (int i = 0; i < curr_hw_topo.num_psets; i++) {
		psets[i] = -1;
	}
	sched_pset_iterate_state_t istate = SCHED_PSET_ITERATE_STATE_INIT;
	int ind = 0;
	processor_set_t starting_pset = pset_array[src_pset_id];
	while (sched_iterate_psets_ordered(starting_pset,
	    &starting_pset->spill_search_order[sched_bucket], candidate_map, &istate)) {
		psets[ind++] = istate.spis_pset_id;
	}
	return psets;
}

test_thread_t
impl_rt_choose_thread(int cpu_id)
{
	return sched_rt_choose_thread(cpus[cpu_id]);
}

void
sched_rt_spill_policy_set(unsigned policy)
{
	impl_sched_rt_spill_policy_set(policy);
}

void
sched_rt_steal_policy_set(unsigned policy)
{
	impl_sched_rt_steal_policy_set(policy);
}
