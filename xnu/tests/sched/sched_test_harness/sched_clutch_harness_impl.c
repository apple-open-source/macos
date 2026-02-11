// Copyright (c) 2023 Apple Inc.  All rights reserved.

#include <stdint.h>
#include <stdio.h>
#include "../../../bsd/sys/kdebug.h" // Want tracecodes from source without searching BSD headers

/* Harness interface */
#include "sched_clutch_harness.h"

/* Include kernel header depdencies */
#include "shadow_headers/misc_needed_defines.h"
#include <kern/sched_common.h>

/* Header for Clutch policy code under-test */
#include <kern/sched_clutch.h>

/* Include non-header dependencies */
#define KERNEL_DEBUG_CONSTANT_IST(a0, a1, a2, a3, a4, a5, a6) clutch_impl_log_tracepoint(a1, a2, a3, a4, a5)
#include "shadow_headers/misc_needed_deps.c"
#include "shadow_headers/sched_prim.c"

static test_hw_topology_t curr_hw_topo = {
	.psets = NULL,
	.num_psets = 0,
	.total_cpus = 0,
};
static int _curr_cpu = 0;

processor_t
current_processor(void)
{
	if (_curr_cpu == 0) {
		/* Assumes boot CPU of id 0 */
		return master_processor;
	} else {
		return processor_array[_curr_cpu];
	}
}

unsigned int
ml_get_cluster_count(void)
{
	return (unsigned int)curr_hw_topo.num_psets;
}

unsigned int
ml_get_cpu_count(void)
{
	return (unsigned int)curr_hw_topo.total_cpus;
}

/* Mocked-out Clutch functions */
static boolean_t
sched_thread_sched_pri_promoted(thread_t thread)
{
	(void)thread;
	return FALSE;
}

#define cpus processor_array

/* Clutch policy code under-test, safe to include now after satisfying its dependencies */
#include <kern/sched_clutch.c>
#include <kern/sched_common.c>
#include <kern/processor.c>

/* Realtime policy code under-test */
#include <kern/sched_rt.c>

/* Implementation of sched_clutch_harness.h interface */

int root_bucket_to_highest_pri[TH_BUCKET_SCHED_MAX] = {
	MAXPRI_USER,
	BASEPRI_FOREGROUND,
	BASEPRI_USER_INITIATED,
	BASEPRI_DEFAULT,
	BASEPRI_UTILITY,
	MAXPRI_THROTTLE
};

int clutch_interactivity_score_max = -1;
uint64_t clutch_root_bucket_wcel_us[TH_BUCKET_SCHED_MAX];
uint64_t clutch_root_bucket_warp_us[TH_BUCKET_SCHED_MAX];
const unsigned int CLUTCH_THREAD_SELECT = MACH_SCHED_CLUTCH_THREAD_SELECT;

/* Implementation of sched_runqueue_harness.h interface */

static test_pset_t single_pset = {
	.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
	.num_cpus = 1,
	.cluster_id = 0,
	.die_id = 0,
};
test_hw_topology_t single_core = {
	.psets = &single_pset,
	.num_psets = 1,
	.total_cpus = 1,
};

char
test_cpu_type_to_char(test_cpu_type_t cpu_type)
{
	switch (cpu_type) {
	case TEST_CPU_TYPE_PERFORMANCE:
		return 'P';
	case TEST_CPU_TYPE_EFFICIENCY:
		return 'E';
	default:
		assert(false);
	}
}

static cluster_type_t
test_cpu_type_to_cluster_type(test_cpu_type_t cpu_type)
{
	switch (cpu_type) {
	case TEST_CPU_TYPE_PERFORMANCE:
		return CLUSTER_TYPE_P;
	case TEST_CPU_TYPE_EFFICIENCY:
		return CLUSTER_TYPE_E;
	default:
		return CLUSTER_TYPE_SMP;
	}
}

static uint64_t unique_tg_id = 0;
static uint64_t unique_thread_id = 0;
static bool first_boot = true;

void
clutch_impl_init_topology(test_hw_topology_t hw_topology)
{
	printf("🗺️  Mock HW Topology: %d psets {", hw_topology.num_psets);
	assert(first_boot); // Not supported to initialize more than one topology
	first_boot = false;
	assert(hw_topology.num_psets <= MAX_PSETS);
	int total_cpus = 0;
	for (int i = 0; i < hw_topology.num_psets; i++) {
		assert((total_cpus + hw_topology.psets[i].num_cpus) <= MAX_CPUS);
		printf(" (%d: %d %c CPUs)", i, hw_topology.psets[i].num_cpus, test_cpu_type_to_char(hw_topology.psets[i].cpu_type));
		cluster_type_t cluster_type = test_cpu_type_to_cluster_type(hw_topology.psets[i].cpu_type);
		processor_set_t pset;
		if (i == 0) {
#if __AMP__
			ml_topology_cluster_t boot_cluster;
			boot_cluster.cluster_type = cluster_type;
			mock_topology_info.boot_cluster = &boot_cluster;
#endif /* __AMP__ */
			processor_bootstrap();
			SCHED(init)();
			SCHED(pset_init)(sched_boot_pset);
			SCHED(rt_init_pset)(sched_boot_pset);
			SCHED(processor_init)(master_processor);
			pset = sched_boot_pset;
		} else {
			pset = pset_create(cluster_type, i, i);
		}
		for (int c = total_cpus; c < total_cpus + hw_topology.psets[i].num_cpus; c++) {
			if (c > 0) {
				processor_t processor = (processor_t)malloc(sizeof(struct processor));
				processor_init(processor, c, pset);
			}
			struct thread_group *not_real_idle_tg = create_tg(0);
			thread_t idle_thread = clutch_impl_create_thread(TH_BUCKET_SHARE_BG, not_real_idle_tg, IDLEPRI);
			idle_thread->bound_processor = cpus[c];
			idle_thread->state = (TH_RUN | TH_IDLE);
			cpus[c]->idle_thread = idle_thread;
			cpus[c]->active_thread = cpus[c]->idle_thread;
			pset_update_processor_state(pset, cpus[c], PROCESSOR_IDLE);
		}
		total_cpus += hw_topology.psets[i].num_cpus;
	}
	processor_avail_count = total_cpus;
	printf(" }\n");
	/* After mock idle thread creation, reset thread/TG start IDs, as the idle threads shouldn't count! */
	unique_tg_id = 0;
	unique_thread_id = 0;
	if (SCHED(cpu_init_completed) != NULL) {
		SCHED(cpu_init_completed)();
	}
	SCHED(rt_init_completed)();
}

#define MAX_LOGGED_TRACE_CODES 10
#define NUM_TRACEPOINT_FIELDS 5
static uint64_t logged_trace_codes[MAX_LOGGED_TRACE_CODES];
static uint32_t logged_trace_codes_ind = 0;
#define MAX_LOGGED_TRACEPOINTS 10000
static uint64_t *logged_tracepoints[MAX_LOGGED_TRACE_CODES];
static uint32_t curr_tracepoint_inds[MAX_LOGGED_TRACE_CODES];
static uint32_t expect_tracepoint_inds[MAX_LOGGED_TRACE_CODES];

void
clutch_impl_init_params(void)
{
	/* Read out Clutch-internal fields for use by the test harness */
	clutch_interactivity_score_max = 2 * sched_clutch_bucket_group_interactive_pri;
	for (int b = TH_BUCKET_FIXPRI; b < TH_BUCKET_SCHED_MAX; b++) {
		clutch_root_bucket_wcel_us[b] = sched_clutch_root_bucket_wcel_us[b] == SCHED_CLUTCH_INVALID_TIME_32 ? 0 : sched_clutch_root_bucket_wcel_us[b];
		clutch_root_bucket_warp_us[b] = sched_clutch_root_bucket_warp_us[b] == SCHED_CLUTCH_INVALID_TIME_32 ? 0 : sched_clutch_root_bucket_warp_us[b];
	}
}

void
clutch_impl_add_logged_trace_code(uint64_t tracepoint)
{
	logged_trace_codes[logged_trace_codes_ind++] = tracepoint;
}

void
clutch_impl_init_tracepoints(void)
{
	/* All filter-included tracepoints */
	clutch_impl_add_logged_trace_code(CLUTCH_THREAD_SELECT);
	/* Init harness-internal allocators */
	for (int i = 0; i < MAX_LOGGED_TRACE_CODES; i++) {
		logged_tracepoints[i] = malloc(MAX_LOGGED_TRACEPOINTS * 5 * sizeof(uint64_t));
		curr_tracepoint_inds[i] = 0;
		expect_tracepoint_inds[i] = 0;
	}
}

struct thread_group *
clutch_impl_create_tg(int interactivity_score)
{
	struct thread_group *tg = malloc(sizeof(struct thread_group));
	sched_clutch_init_with_thread_group(&tg->tg_sched_clutch, tg);
	if (interactivity_score != INITIAL_INTERACTIVITY_SCORE) {
		for (int bucket = TH_BUCKET_SHARE_FG; bucket < TH_BUCKET_SCHED_MAX; bucket++) {
			tg->tg_sched_clutch.sc_clutch_groups[bucket].scbg_interactivity_data.scct_count = interactivity_score;
			tg->tg_sched_clutch.sc_clutch_groups[bucket].scbg_interactivity_data.scct_timestamp = mach_absolute_time();
		}
	}
	tg->tg_id = unique_tg_id++;
	return tg;
}

test_thread_t
clutch_impl_create_thread(int root_bucket, struct thread_group *tg, int pri)
{
	assert((sched_bucket_t)root_bucket == sched_convert_pri_to_bucket(pri) || (sched_bucket_t)root_bucket == TH_BUCKET_FIXPRI);
	assert(tg != NULL);
	thread_t thread = malloc(sizeof(struct thread));
	thread->base_pri = pri;
	thread->sched_pri = pri;
	thread->sched_flags = 0;
	thread->thread_group = tg;
	thread->th_sched_bucket = root_bucket;
	thread->bound_processor = NULL;
	thread->__runq.runq = PROCESSOR_NULL;
	queue_chain_init(thread->runq_links);
	thread->thread_id = unique_thread_id++;
#if CONFIG_SCHED_EDGE
	thread->th_bound_cluster_enqueued = false;
	for (cluster_shared_rsrc_type_t shared_rsrc_type = CLUSTER_SHARED_RSRC_TYPE_MIN; shared_rsrc_type < CLUSTER_SHARED_RSRC_TYPE_COUNT; shared_rsrc_type++) {
		thread->th_shared_rsrc_enqueued[shared_rsrc_type] = false;
		thread->th_shared_rsrc_heavy_user[shared_rsrc_type] = false;
		thread->th_shared_rsrc_heavy_perf_control[shared_rsrc_type] = false;
		thread->th_expired_quantum_on_lower_core = false;
		thread->th_expired_quantum_on_higher_core = false;
	}
#endif /* CONFIG_SCHED_EDGE */
	thread->th_bound_cluster_id = THREAD_BOUND_CLUSTER_NONE;
	thread->reason = AST_NONE;
	thread->sched_mode = TH_MODE_TIMESHARE;
	bzero(&thread->realtime, sizeof(thread->realtime));
	thread->last_made_runnable_time = 0;
	thread->state = TH_RUN;
	return thread;
}

void
impl_set_thread_sched_mode(test_thread_t thread, int mode)
{
	((thread_t)thread)->sched_mode = (sched_mode_t)mode;
}

bool
impl_get_thread_is_realtime(test_thread_t thread)
{
	return ((thread_t)thread)->sched_pri >= BASEPRI_RTQUEUES;
}

void
clutch_impl_set_thread_processor_bound(test_thread_t thread, int cpu_id)
{
	((thread_t)thread)->bound_processor = cpus[cpu_id];
}

void
clutch_impl_cpu_set_thread_current(int cpu_id, test_thread_t thread)
{
	cpus[cpu_id]->active_thread = thread;
	cpus[cpu_id]->first_timeslice = TRUE;
	/* Equivalent logic of pset_commit_processor_to_new_thread() */
	pset_update_processor_state(cpus[cpu_id]->processor_set, cpus[cpu_id], PROCESSOR_RUNNING);
	processor_state_update_from_thread(cpus[cpu_id], thread, true);
	if (((thread_t) thread)->sched_pri >= BASEPRI_RTQUEUES) {
		bit_set(cpus[cpu_id]->processor_set->realtime_map, cpu_id);
		cpus[cpu_id]->deadline = ((thread_t) thread)->realtime.deadline;
	} else {
		bit_clear(cpus[cpu_id]->processor_set->realtime_map, cpu_id);
		cpus[cpu_id]->deadline = UINT64_MAX;
	}
}

test_thread_t
clutch_impl_cpu_clear_thread_current(int cpu_id)
{
	test_thread_t thread = cpus[cpu_id]->active_thread;
	cpus[cpu_id]->active_thread = cpus[cpu_id]->idle_thread;
	bit_clear(cpus[cpu_id]->processor_set->realtime_map, cpu_id);
	pset_update_processor_state(cpus[cpu_id]->processor_set, cpus[cpu_id], PROCESSOR_IDLE);
	processor_state_update_idle(cpus[cpu_id]);
	return thread;
}

static bool
is_logged_clutch_trace_code(uint64_t clutch_trace_code)
{
	for (int i = 0; i < logged_trace_codes_ind; i++) {
		if (logged_trace_codes[i] == clutch_trace_code) {
			return true;
		}
	}
	return false;
}

static bool
is_logged_trace_code(uint64_t trace_code)
{
	if (KDBG_EXTRACT_CLASS(trace_code) == DBG_MACH && KDBG_EXTRACT_SUBCLASS(trace_code) == DBG_MACH_SCHED_CLUTCH) {
		if (is_logged_clutch_trace_code(KDBG_EXTRACT_CODE(trace_code))) {
			return true;
		}
	}
	return false;
}

static int
trace_code_to_ind(uint64_t trace_code)
{
	for (int i = 0; i < logged_trace_codes_ind; i++) {
		if (trace_code == logged_trace_codes[i]) {
			return i;
		}
	}
	return -1;
}

void
clutch_impl_log_tracepoint(uint64_t trace_code, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
{
	if (is_logged_trace_code(trace_code)) {
		int ind = trace_code_to_ind(KDBG_EXTRACT_CODE(trace_code));
		assert(ind >= 0);
		if (curr_tracepoint_inds[ind] < MAX_LOGGED_TRACEPOINTS) {
			logged_tracepoints[ind][curr_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 0] = KDBG_EXTRACT_CODE(trace_code);
			logged_tracepoints[ind][curr_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 1] = a1;
			logged_tracepoints[ind][curr_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 2] = a2;
			logged_tracepoints[ind][curr_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 3] = a3;
			logged_tracepoints[ind][curr_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 4] = a4;
		} else if (curr_tracepoint_inds[ind] == MAX_LOGGED_TRACEPOINTS) {
			printf("Ran out of pre-allocated memory to log tracepoints (%d points)...will no longer log tracepoints\n",
			    MAX_LOGGED_TRACEPOINTS);
		}
		curr_tracepoint_inds[ind]++;
	}
}

void
clutch_impl_pop_tracepoint(uint64_t clutch_trace_code, uint64_t *arg1, uint64_t *arg2, uint64_t *arg3, uint64_t *arg4)
{
	int ind = trace_code_to_ind(clutch_trace_code);
	if (expect_tracepoint_inds[ind] >= curr_tracepoint_inds[ind]) {
		/* Indicate that there isn't a matching tracepoint drop found to consume */
		*arg1 = -1;
		*arg2 = -1;
		*arg3 = -1;
		*arg4 = -1;
		return;
	}
	assert(logged_tracepoints[ind][expect_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 0] == clutch_trace_code);
	*arg1 = logged_tracepoints[ind][expect_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 1];
	*arg2 = logged_tracepoints[ind][expect_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 2];
	*arg3 = logged_tracepoints[ind][expect_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 3];
	*arg4 = logged_tracepoints[ind][expect_tracepoint_inds[ind] * NUM_TRACEPOINT_FIELDS + 4];
	expect_tracepoint_inds[ind]++;
}

uint64_t
impl_get_thread_tid(test_thread_t thread)
{
	return ((thread_t)thread)->thread_id;
}

#pragma mark - Realtime

static test_thread_t
impl_dequeue_realtime_thread(processor_set_t pset)
{
	thread_t thread = rt_runq_dequeue(&pset->rt_runq);
	pset_update_rt_stealable_state(pset);
	return thread;
}

void
impl_set_thread_realtime(test_thread_t thread, uint32_t period, uint32_t computation, uint32_t constraint, bool preemptible, uint8_t priority_offset, uint64_t deadline)
{
	thread_t t = (thread_t) thread;
	t->realtime.period = period;
	t->realtime.computation = computation;
	t->realtime.constraint = constraint;
	t->realtime.preemptible = preemptible;
	t->realtime.priority_offset = priority_offset;
	t->realtime.deadline = deadline;
}

void
impl_sched_rt_spill_policy_set(unsigned policy)
{
	sched_rt_spill_policy = policy;
}

void
impl_sched_rt_steal_policy_set(unsigned policy)
{
	sched_rt_steal_policy = policy;
}

#pragma mark -- IPI Subsystem

sched_ipi_type_t
sched_ipi_action(processor_t dst, thread_t thread, sched_ipi_event_t event)
{
	/* Forward to the policy-specific implementation */
	return SCHED(ipi_policy)(dst, thread, (dst->active_thread == dst->idle_thread), event);
}

#define MAX_LOGGED_IPIS 10000
typedef struct {
	int cpu_id;
	sched_ipi_type_t ipi_type;
} logged_ipi_t;
static logged_ipi_t logged_ipis[MAX_LOGGED_IPIS];
static uint32_t curr_ipi_ind = 0;
static uint32_t expect_ipi_ind = 0;

void
sched_ipi_perform(processor_t dst, sched_ipi_type_t ipi)
{
	/* Record the IPI type and where we sent it */
	logged_ipis[curr_ipi_ind].cpu_id = dst->cpu_id;
	logged_ipis[curr_ipi_ind].ipi_type = ipi;
	curr_ipi_ind++;
}

sched_ipi_type_t
sched_ipi_policy(processor_t dst, thread_t thread,
    boolean_t dst_idle, sched_ipi_event_t event)
{
	(void)dst;
	(void)thread;
	(void)dst_idle;
	(void)event;
	if (event == SCHED_IPI_EVENT_REBALANCE) {
		return SCHED_IPI_IMMEDIATE;
	}
	/* For now, default to deferred IPI */
	return SCHED_IPI_DEFERRED;
}

sched_ipi_type_t
sched_ipi_deferred_policy(processor_set_t pset,
    processor_t dst, thread_t thread, sched_ipi_event_t event)
{
	(void)pset;
	(void)dst;
	(void)thread;
	(void)event;
	return SCHED_IPI_DEFERRED;
}
