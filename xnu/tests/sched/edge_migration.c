// Copyright (c) 2024 Apple Inc.  All rights reserved.

#include "sched_test_harness/sched_policy_darwintest.h"
#include "sched_test_harness/sched_edge_harness.h"

T_GLOBAL_META(T_META_NAMESPACE("xnu.scheduler"),
    T_META_RADAR_COMPONENT_NAME("xnu"),
    T_META_RADAR_COMPONENT_VERSION("scheduler"),
    T_META_RUN_CONCURRENTLY(true),
    T_META_OWNER("emily_peterson"));

SCHED_POLICY_T_DECL(migration_cluster_bound,
    "Verify that cluster-bound threads always choose the bound "
    "cluster except when its derecommended")
{
	int ret;
	init_migration_harness(dual_die);
	struct thread_group *tg = create_tg(0);
	test_thread_t threads[dual_die.num_psets];
	int idle_load = 0;
	int low_load = 100000;
	int high_load = 10000000;
	for (int i = 0; i < dual_die.num_psets; i++) {
		threads[i] = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
		set_thread_cluster_bound(threads[i], i);
		set_pset_load_avg(i, TH_BUCKET_SHARE_DF, low_load);
	}
	for (int i = 0; i < dual_die.num_psets; i++) {
		set_current_processor(pset_id_to_cpu_id(i));
		for (int j = 0; j < dual_die.num_psets; j++) {
			/* Add extra load to the bound cluster, so we're definitely not just idle short-circuiting */
			set_pset_load_avg(j, TH_BUCKET_SHARE_DF, high_load);
			ret = choose_pset_for_thread_expect(threads[j], j);
			T_QUIET; T_EXPECT_TRUE(ret, "Expecting the bound cluster");
			set_pset_load_avg(j, TH_BUCKET_SHARE_DF, low_load);
		}
	}
	SCHED_POLICY_PASS("Cluster bound chooses bound cluster");
	/* Derecommend the bound cluster */
	for (int i = 0; i < dual_die.num_psets; i++) {
		set_pset_derecommended(i);
		int replacement_pset = -1;
		for (int j = 0; j < dual_die.num_psets; j++) {
			/* Find the first homogenous cluster and mark it as idle so we choose it */
			if ((i != j) && (dual_die.psets[i].cpu_type == dual_die.psets[j].cpu_type)) {
				replacement_pset = j;
				set_pset_load_avg(replacement_pset, TH_BUCKET_SHARE_DF, idle_load);
				break;
			}
		}
		ret = choose_pset_for_thread_expect(threads[i], replacement_pset);
		T_QUIET; T_EXPECT_TRUE(ret, "Expecting the idle pset when the bound cluster is derecommended");
		/* Restore pset conditions */
		set_pset_recommended(i);
		set_pset_load_avg(replacement_pset, TH_BUCKET_SHARE_DF, low_load);
	}
	SCHED_POLICY_PASS("Cluster binding is soft");
}

SCHED_POLICY_T_DECL(migration_should_yield,
    "Verify that we only yield if there's a \"good enough\" thread elsewhere "
    "to switch to")
{
	int ret;
	init_migration_harness(basic_amp);
	struct thread_group *tg = create_tg(0);
	test_thread_t yielder = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	int p_pset = 0;
	int p_cpu = pset_id_to_cpu_id(p_pset);
	cpu_set_thread_current(p_cpu, yielder);
	ret = cpu_check_should_yield(p_cpu, false);
	T_QUIET; T_EXPECT_TRUE(ret, "No thread present to yield to");
	ret = tracepoint_expect(EDGE_SHOULD_YIELD, get_thread_tid(yielder), p_pset, 0, 4);
	T_QUIET; T_EXPECT_TRUE(ret, "SCHED_EDGE_YIELD_DISALLOW");

	test_thread_t background = create_thread(TH_BUCKET_SHARE_BG, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_BG]);
	enqueue_thread(pset_target(p_pset), background);
	ret = cpu_check_should_yield(p_cpu, true);
	T_QUIET; T_EXPECT_TRUE(ret, "Should yield to a low priority thread on the current runqueue");
	ret = tracepoint_expect(EDGE_SHOULD_YIELD, get_thread_tid(yielder), p_pset, 0, 0);
	T_QUIET; T_EXPECT_TRUE(ret, "SCHED_EDGE_YIELD_RUNQ_NONEMPTY");
	SCHED_POLICY_PASS("Basic yield behavior on single pset");

	int e_pset = 1;
	int e_cpu = pset_id_to_cpu_id(e_pset);
	ret = dequeue_thread_expect(pset_target(p_pset), background);
	T_QUIET; T_EXPECT_TRUE(ret, "Only background thread in runqueue");
	set_tg_sched_bucket_preferred_pset(tg, TH_BUCKET_SHARE_BG, e_pset);
	enqueue_thread(pset_target(e_pset), background);
	ret = cpu_check_should_yield(p_cpu, false);
	T_QUIET; T_EXPECT_TRUE(ret, "Should not yield in order to running rebalance native thread");
	ret = tracepoint_expect(EDGE_SHOULD_YIELD, get_thread_tid(yielder), p_cpu, 0, 4);
	T_QUIET; T_EXPECT_TRUE(ret, "SCHED_EDGE_YIELD_DISALLOW");

	ret = dequeue_thread_expect(pset_target(e_pset), background);
	T_QUIET; T_EXPECT_TRUE(ret, "Only background thread in runqueue");
	set_tg_sched_bucket_preferred_pset(tg, TH_BUCKET_SHARE_BG, p_pset);
	cpu_set_thread_current(e_cpu, background);
	ret = cpu_check_should_yield(p_cpu, true);
	T_QUIET; T_EXPECT_TRUE(ret, "Should yield in order to running rebalance foreign thread");
	ret = tracepoint_expect(EDGE_SHOULD_YIELD, get_thread_tid(yielder), p_cpu, 0, 2);
	T_QUIET; T_EXPECT_TRUE(ret, "SCHED_EDGE_YIELD_FOREIGN_RUNNING");

	enqueue_thread(pset_target(p_pset), background);
	cpu_set_thread_current(e_cpu, yielder);
	ret = cpu_check_should_yield(e_cpu, true);
	T_QUIET; T_EXPECT_TRUE(ret, "Should yield in order to steal thread");
	ret = tracepoint_expect(EDGE_SHOULD_YIELD, get_thread_tid(yielder), e_pset, 0, 3);
	T_QUIET; T_EXPECT_TRUE(ret, "SCHED_EDGE_YIELD_STEAL_POSSIBLE");
	SCHED_POLICY_PASS("Thread yields in order to steal from other psets");
}

SCHED_POLICY_T_DECL(migration_stir_the_pot_basic,
    "Verify stir-the-pot succeeds to rotate threads across P and E-cores after"
    "their respective quanta have expired")
{
	int ret;
	init_migration_harness(basic_amp);

	struct thread_group *tg = create_tg(0);
	test_thread_t starts_p = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	test_thread_t starts_e = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	test_thread_t other_p_thread = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	int p_cpu = 0;
	int e_cpu = 2;
	int other_e_cpu = 3;
	int other_p_cpu = 1;
	cpu_set_thread_current(p_cpu, starts_p);
	cpu_set_thread_current(e_cpu, starts_e);
	cpu_set_thread_current(other_p_cpu, other_p_thread);
	int p_pset = 0;
	int e_pset = 1;

	/* Thread on low core type "pays its dues" */
	cpu_expire_quantum(e_cpu);

	/* Thread on high core type should locate swap candidate */
	cpu_expire_quantum(p_cpu);
	ret = ipi_expect(e_cpu, TEST_IPI_IMMEDIATE);
	T_QUIET; T_EXPECT_TRUE(ret, "Should have found stir-the-pot candidate with expired quantum");

	/* Thread on low core type should respond to IPI by preempting... */
	ret = thread_avoid_processor_expect(starts_e, e_cpu, false, true);
	T_QUIET; T_EXPECT_TRUE(ret, "Thread should preempt to get on P-core");

	/* (Simulate as if we are switching to another quantum-expired thread) */
	test_thread_t other_expired_thread = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	cpu_set_thread_current(other_e_cpu, other_expired_thread);
	cpu_expire_quantum(other_e_cpu);
	cpu_clear_thread_current(other_e_cpu);
	cpu_set_thread_current(e_cpu, other_expired_thread);

	/* ...and choosing the corresponding P-core for swap */
	ret = choose_pset_for_thread_expect(starts_e, p_pset);
	T_QUIET; T_EXPECT_TRUE(ret, "Should choose P-cores despite no idle cores there");

	/* Upon arrival, thread swapping in should preempt its predecessor */
	enqueue_thread(pset_target(p_pset), starts_e);
	ret = cpu_check_preempt_current(p_cpu, true);
	T_QUIET; T_EXPECT_TRUE(ret, "P-core should preempt quantum expired thread");

	/* ...and preempted thread on P-core should spill down to E, completing the swap */
	ret = dequeue_thread_expect(pset_target(p_pset), starts_e);
	T_QUIET; T_ASSERT_TRUE(ret, "e_starts was enqueued on P");
	cpu_set_thread_current(p_cpu, starts_e);
	ret = choose_pset_for_thread_expect(starts_p, e_pset);
	T_QUIET; T_EXPECT_TRUE(ret, "p_starts spilled to E, completing swap");

	/*
	 * And a second swap should be initiated for the other E-expired thread
	 * that switched on-core afterwards.
	 */
	cpu_expire_quantum(other_p_cpu);
	ret = ipi_expect(e_cpu, TEST_IPI_IMMEDIATE);
	T_QUIET; T_EXPECT_TRUE(ret, "Should have found stir-the-pot candidate with expired quantum");

	SCHED_POLICY_PASS("Stir-the-pot successfully initiated by P-core and completed");

	/* Clean-up and reset to initial conditions */
	cpu_set_thread_current(p_cpu, starts_p);
	cpu_set_thread_current(e_cpu, starts_e);
	cpu_set_thread_current(other_p_cpu, other_p_thread);
	cpu_set_thread_current(other_e_cpu, other_expired_thread);

	/* Now P-core expires quantum first */
	cpu_expire_quantum(p_cpu);

	/* Thread on E-core "pays its dues" and responds to self-message by preempting */
	cpu_expire_quantum(e_cpu);
	ret = thread_avoid_processor_expect(starts_e, e_cpu, false, true);
	T_QUIET; T_EXPECT_TRUE(ret, "Thread should preempt to get on P-core");

	/* ...and choosing the corresponding P-core for swap */
	cpu_clear_thread_current(e_cpu);
	ret = choose_pset_for_thread_expect(starts_e, p_pset);
	T_QUIET; T_EXPECT_TRUE(ret, "Should choose P-cores despite no idle cores there");

	/* Upon arrival, thread swapping in should preempt its predecessor */
	enqueue_thread(pset_target(p_pset), starts_e);
	ret = cpu_check_preempt_current(p_cpu, true);
	T_QUIET; T_EXPECT_TRUE(ret, "P-core should preempt quantum expired thread");

	/* ...and preempted thread on P-core should spill down to E, completing the swap */
	ret = dequeue_thread_expect(pset_target(p_pset), starts_e);
	T_QUIET; T_ASSERT_TRUE(ret, "e_starts was enqueued on P");
	cpu_set_thread_current(p_cpu, starts_e);
	ret = choose_pset_for_thread_expect(starts_p, e_pset);
	T_QUIET; T_EXPECT_TRUE(ret, "p_starts spilled to E, completing swap");

	SCHED_POLICY_PASS("Stir-the-pot successfully initiated by E-core and completed");
}

SCHED_POLICY_T_DECL(migration_ipi_policy,
    "Verify we send the right type of IPI in different cross-core preemption scenarios")
{
	int ret;
	init_migration_harness(dual_die);
	struct thread_group *tg = create_tg(0);
	thread_t thread = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	int dst_pcore = 3;
	int src_pcore = 0;

	set_current_processor(src_pcore);
	cpu_send_ipi_for_thread(dst_pcore, thread, TEST_IPI_EVENT_PREEMPT);
	ret = ipi_expect(dst_pcore, TEST_IPI_IDLE);
	T_QUIET; T_EXPECT_TRUE(ret, "Idle CPU");

	thread_t core_busy = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	cpu_set_thread_current(dst_pcore, core_busy);
	set_current_processor(src_pcore);
	cpu_send_ipi_for_thread(dst_pcore, thread, TEST_IPI_EVENT_PREEMPT);
	ret = ipi_expect(dst_pcore, TEST_IPI_IMMEDIATE);
	T_QUIET; T_EXPECT_TRUE(ret, "Should immediate IPI to preempt on P-core");
	SCHED_POLICY_PASS("Immediate IPIs to preempt P-cores");

	int dst_ecore = 13;
	int ecluster_id = 5;
	set_tg_sched_bucket_preferred_pset(tg, TH_BUCKET_SHARE_DF, ecluster_id);
	set_current_processor(src_pcore);
	cpu_send_ipi_for_thread(dst_ecore, thread, TEST_IPI_EVENT_PREEMPT);
	ret = ipi_expect(dst_ecore, TEST_IPI_IDLE);
	T_QUIET; T_EXPECT_TRUE(ret, "Idle CPU");

	cpu_set_thread_current(dst_ecore, core_busy);
	set_current_processor(src_pcore);
	cpu_send_ipi_for_thread(dst_ecore, thread, TEST_IPI_EVENT_PREEMPT);
	ret = ipi_expect(dst_ecore, TEST_IPI_IMMEDIATE);
	T_QUIET; T_EXPECT_TRUE(ret, "Should immediate IPI to preempt for E->E");
	SCHED_POLICY_PASS("Immediate IPIs to cluster homogeneous with preferred");
}

SCHED_POLICY_T_DECL(migration_max_parallelism,
    "Verify we report expected values for recommended width of parallel workloads")
{
	int ret;
	init_migration_harness(dual_die);
	uint32_t num_pclusters = 4;
	uint32_t num_pcores = 4 * num_pclusters;
	uint32_t num_eclusters = 2;
	uint32_t num_ecores = 2 * num_eclusters;
	for (thread_qos_t qos = THREAD_QOS_UNSPECIFIED; qos < THREAD_QOS_LAST; qos++) {
		for (int shared_rsrc = 0; shared_rsrc < 2; shared_rsrc++) {
			for (int rt = 0; rt < 2; rt++) {
				uint64_t options = 0;
				uint32_t expected_width = 0;
				if (shared_rsrc) {
					options |= QOS_PARALLELISM_CLUSTER_SHARED_RESOURCE;
				}
				if (rt) {
					options |= QOS_PARALLELISM_REALTIME;
					/* Recommend P-width */
					expected_width = shared_rsrc ? num_pclusters : num_pcores;
				} else if (qos == THREAD_QOS_BACKGROUND || qos == THREAD_QOS_MAINTENANCE) {
					/* Recommend E-width */
					expected_width = shared_rsrc ? num_eclusters : num_ecores;
				} else {
					/* Recommend full width */
					expected_width = shared_rsrc ? (num_eclusters + num_pclusters) : (num_pcores + num_ecores);
				}
				ret = max_parallelism_expect(qos, options, expected_width);
				T_QUIET; T_EXPECT_TRUE(ret, "Unexpected width for QoS %d shared_rsrc %d RT %d",
				    qos, shared_rsrc, rt);
			}
		}
	}
	SCHED_POLICY_PASS("Correct recommended parallel width for all configurations");
}

SCHED_POLICY_T_DECL(migration_rebalance_basic, "Verify that basic rebalance steal and "
    "running rebalance mechanisms kick in")
{
	int ret;
	test_hw_topology_t topo = SCHED_POLICY_DEFAULT_TOPO;
	init_migration_harness(topo);
	int sched_bucket = TH_BUCKET_SHARE_DF;
	struct thread_group *tg = create_tg(0);
	thread_t thread = create_thread(sched_bucket, tg, root_bucket_to_highest_pri[sched_bucket]);

	for (int preferred_pset_id = 0; preferred_pset_id < topo.num_psets; preferred_pset_id++) {
		set_tg_sched_bucket_preferred_pset(tg, sched_bucket, preferred_pset_id);
		sched_policy_push_metadata("preferred_pset_id", preferred_pset_id);
		for (int running_on_pset_id = 0; running_on_pset_id < topo.num_psets; running_on_pset_id++) {
			/* Running rebalance */
			int running_on_cpu = pset_id_to_cpu_id(running_on_pset_id);
			cpu_set_thread_current(running_on_cpu, thread);
			sched_policy_push_metadata("running_on_pset_id", running_on_pset_id);
			for (int c = 0; c < topo.total_cpus; c++) {
				sched_policy_push_metadata("evaluate_cpu", c);
				int evaluate_pset = cpu_id_to_pset_id(c);
				bool want_rebalance = cpu_processor_balance(c);
				if (evaluate_pset == running_on_pset_id) {
					T_QUIET; T_EXPECT_FALSE(want_rebalance, "should be no thread available for rebalance %s",
					    sched_policy_dump_metadata());
					sched_policy_pop_metadata();
					continue;
				}
				bool should_rebalance = (topo.psets[evaluate_pset].cpu_type == topo.psets[preferred_pset_id].cpu_type) &&
				    (topo.psets[running_on_pset_id].cpu_type != topo.psets[preferred_pset_id].cpu_type);
				T_QUIET; T_EXPECT_EQ(want_rebalance, should_rebalance, "should rebalance to move thread to preferred type "
				    "if not there already %s", sched_policy_dump_metadata());
				if (should_rebalance) {
					ret = tracepoint_expect(EDGE_REBAL_RUNNING, 0, c, running_on_cpu, 0);
					T_QUIET; T_EXPECT_TRUE(ret, "EDGE_REBAL_RUNNING tracepoint");
					ret = thread_avoid_processor_expect(thread, running_on_cpu, false, true);
					T_QUIET; T_EXPECT_TRUE(ret, "thread will preempt in response to running rebalance IPI %s",
					    sched_policy_dump_metadata());
					/* Try loading all other cores of the preferred type, forcing this decision to find the idle one */
					for (int p = 0; p < topo.num_psets; p++) {
						if ((topo.psets[p].cpu_type == topo.psets[preferred_pset_id].cpu_type) &&
						    (p != evaluate_pset)) {
							set_pset_load_avg(p, sched_bucket, 10000000);
						}
					}
					ret = thread_avoid_processor_expect(thread, running_on_cpu, false, true);
					T_QUIET; T_EXPECT_TRUE(ret, "...even if all other cores (except rebalancer) are full %s",
					    sched_policy_dump_metadata());
					/* Unload cores for clean-up */
					for (int p = 0; p < topo.num_psets; p++) {
						if ((topo.psets[p].cpu_type == topo.psets[preferred_pset_id].cpu_type) &&
						    (p != evaluate_pset)) {
							set_pset_load_avg(p, sched_bucket, 0);
						}
					}
				}
				sched_policy_pop_metadata();
			}
			cpu_clear_thread_current(running_on_cpu);
			sched_policy_pop_metadata();

			/* Rebalance steal */
			int enqueued_pset = running_on_pset_id;
			enqueue_thread(pset_target(enqueued_pset), thread);
			sched_policy_push_metadata("enqueued_pset", enqueued_pset);
			for (int c = 0; c < topo.total_cpus; c++) {
				sched_policy_push_metadata("evaluate_cpu", c);
				int evaluate_pset = cpu_id_to_pset_id(c);
				if ((topo.psets[evaluate_pset].cpu_type != topo.psets[enqueued_pset].cpu_type) &&
				    ((topo.psets[enqueued_pset].cpu_type != TEST_CPU_TYPE_PERFORMANCE) ||
				    (topo.psets[preferred_pset_id].cpu_type != TEST_CPU_TYPE_PERFORMANCE))) {
					/* Only evaluate steal between mismatching cluster types and where spill is not allowed */
					thread_t stolen_thread = cpu_steal_thread(c);
					bool should_rebalance_steal = (topo.psets[evaluate_pset].cpu_type == topo.psets[preferred_pset_id].cpu_type) &&
					    (topo.psets[enqueued_pset].cpu_type != topo.psets[preferred_pset_id].cpu_type);
					bool did_rebalance_steal = (stolen_thread == thread);
					if (stolen_thread != NULL) {
						T_QUIET; T_EXPECT_EQ(stolen_thread, thread, "should only be one thread to steal?");
					}
					T_QUIET; T_EXPECT_EQ(did_rebalance_steal, should_rebalance_steal, "should rebalance steal to move "
					    "thread to preferred type if not already there %s", sched_policy_dump_metadata());
					if (did_rebalance_steal) {
						ret = tracepoint_expect(EDGE_REBAL_RUNNABLE, 0, evaluate_pset, enqueued_pset, 0);
						T_QUIET; T_EXPECT_TRUE(ret, "EDGE_REBAL_RUNNABLE tracepoint");
						/* Put back stolen thread */
						enqueue_thread(pset_target(enqueued_pset), thread);
					}
				}
				sched_policy_pop_metadata();
			}

			ret = dequeue_thread_expect(pset_target(enqueued_pset), thread);
			T_QUIET; T_EXPECT_TRUE(ret, "thread correctly where we left it");
			sched_policy_pop_metadata();
		}
		sched_policy_pop_metadata();
	}
	SCHED_POLICY_PASS("Rebalance mechanisms kicking in!");
}

static test_pset_t two_of_each_psets[6] = {
	{
		.cpu_type = TEST_CPU_TYPE_EFFICIENCY,
		.num_cpus = 2,
		.cluster_id = 0,
		.die_id = 0,
	},
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 2,
		.cluster_id = 1,
		.die_id = 0,
	},
	{
		.cpu_type = TEST_CPU_TYPE_EFFICIENCY,
		.num_cpus = 2,
		.cluster_id = 2,
		.die_id = 1,
	},
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 2,
		.cluster_id = 3,
		.die_id = 1,
	},
};
test_hw_topology_t two_of_each = {
	.psets = &two_of_each_psets[0],
	.num_psets = 4,
	.total_cpus = 8,
};

static void
clear_threads_from_topo(void)
{
	test_hw_topology_t topo = get_hw_topology();
	int pset_first_cpu = 0;
	for (int p = 0; p < topo.num_psets; p++) {
		while (!runqueue_empty(pset_target(p))) {
			(void)dequeue_thread_expect(pset_target(p), (test_thread_t)0xc0ffee);
		}
		for (int b = 0; b < TH_BUCKET_SCHED_MAX; b++) {
			set_pset_load_avg(p, b, 0);
		}
		for (int c = pset_first_cpu; c < pset_first_cpu + topo.psets[p].num_cpus; c++) {
			cpu_clear_thread_current(c);
		}
		pset_first_cpu += topo.psets[p].num_cpus;
	}
}

typedef enum {
	enqueued = 0,
	running = 1,
	thread_type_max = 2,
} thread_type_t;

typedef enum {
	e_recc = 0,
	p_recc = 1,
	recc_type_max = 2,
} recc_type_t;

static char *
thread_recc_to_core_type_char(recc_type_t recc)
{
	switch (recc) {
	case e_recc:
		return "E";
	case p_recc:
		return "P";
	default:
		assert(false);
	}
}

static char
pset_id_to_core_type_char(int pset_id)
{
	return test_cpu_type_to_char(get_hw_topology().psets[pset_id].cpu_type);
}

static void
no_steal_expect(int stealing_pset, char *explanation)
{
	test_thread_t no_steal = cpu_steal_thread(pset_id_to_cpu_id(stealing_pset));
	T_EXPECT_NULL(no_steal, "No thread stolen because: %s (%p)", explanation, no_steal);
}

/*
 * For convenience when handling arrays with one test thread per each
 * possible recommendation type, map the recommendation type to an
 * index in such an array.
 */
static int
recc_type_to_ind(recc_type_t recc)
{
	return (int)recc;
}

static void
foreign_steal_expect(int stealing_pset, int stolen_from_pset,
    test_thread_t thread_candidates_matrix[thread_type_max][4][recc_type_max],
    recc_type_t thread_recommendation)
{
	int ret;
	test_thread_t thread = cpu_steal_thread(pset_id_to_cpu_id(stealing_pset));
	char stealing_type = pset_id_to_core_type_char(stealing_pset);
	char stolen_type = pset_id_to_core_type_char(stolen_from_pset);
	char *recc_type = thread_recc_to_core_type_char(thread_recommendation);
	T_EXPECT_EQ(thread, thread_candidates_matrix[enqueued][stolen_from_pset][recc_type_to_ind(thread_recommendation)],
	    "%c (%d) rebalance-steals %s-recommended from %c (%d)", stealing_type, stealing_pset,
	    recc_type, stolen_type, stolen_from_pset);
	ret = tracepoint_expect(EDGE_REBAL_RUNNABLE,
	    get_thread_tid(thread_candidates_matrix[enqueued][stolen_from_pset][recc_type_to_ind(thread_recommendation)]),
	    stealing_pset, stolen_from_pset, 0);
	T_QUIET; T_EXPECT_TRUE(ret, "EDGE_REBAL_RUNNABLE %c->%c %s-recommended tracepoint",
	    stolen_type, stealing_type, recc_type);
}

static void
work_steal_expect(int stealing_pset, int stolen_from_pset,
    test_thread_t thread_candidates_matrix[thread_type_max][4][recc_type_max],
    recc_type_t thread_recommendation)
{
	int ret;
	test_thread_t thread = cpu_steal_thread(pset_id_to_cpu_id(stealing_pset));
	char stealing_type = pset_id_to_core_type_char(stealing_pset);
	char stolen_type = pset_id_to_core_type_char(stolen_from_pset);
	char *recc_type = thread_recc_to_core_type_char(thread_recommendation);
	T_EXPECT_EQ(thread, thread_candidates_matrix[enqueued][stolen_from_pset][recc_type_to_ind(thread_recommendation)],
	    "%c (%d) work-steals %s-recommended from %c (%d)", stealing_type, stealing_pset,
	    recc_type, stolen_type, stolen_from_pset);
	ret = tracepoint_expect(EDGE_STEAL,
	    get_thread_tid(thread_candidates_matrix[enqueued][stolen_from_pset][recc_type_to_ind(thread_recommendation)]),
	    stealing_pset, stolen_from_pset, 0);
	T_QUIET; T_EXPECT_TRUE(ret, "EDGE_STEAL %c->%c %s-recommended tracepoint",
	    stolen_type, stealing_type, recc_type);
}

static void
running_rebalance_expect(int rebalancing_pset, char *target_name,
    int num_target_cpus, int *target_cpus)
{
	int ret;
	char rebalancing_type = pset_id_to_core_type_char(rebalancing_pset);
	bool want_rebalance = cpu_processor_balance(pset_id_to_cpu_id(rebalancing_pset));
	T_EXPECT_TRUE(want_rebalance, "Send running rebalance %s->%c IPIs",
	    target_name, rebalancing_type);
	for (int i = 0; i < num_target_cpus; i++) {
		ret = tracepoint_expect(EDGE_REBAL_RUNNING, 0, pset_id_to_cpu_id(rebalancing_pset),
		    target_cpus[i], 0);
		T_QUIET; T_EXPECT_TRUE(ret, "EDGE_REBAL_RUNNING %s->%c IPI tracepoint %d",
		    target_name, rebalancing_type, i);
	}
}

SCHED_POLICY_T_DECL(migration_steal_order, "Verify that steal policy steps "
    "happen in the right order")
{
	int sched_bucket = TH_BUCKET_SHARE_DF;
	init_migration_harness(two_of_each);
	for (int config = 0; config < 2; config++) {
		/*
		 * Enqueue one thread of each recommendation type on each pset,
		 * and set one thread of each recommendation type on each pset
		 * running on a core.
		 */
		struct thread_group *p_tg = create_tg(0);
		int p_pset = 1;
		set_tg_sched_bucket_preferred_pset(p_tg, sched_bucket, p_pset);
		struct thread_group *e_tg = create_tg(0);
		int e_pset = 0;
		set_tg_sched_bucket_preferred_pset(e_tg, sched_bucket, e_pset);
		test_thread_t threads[thread_type_max][4][recc_type_max];
		for (int p = 0; p < two_of_each.num_psets; p++) {
			for (recc_type_t r = 0; r < recc_type_max; r++) {
				threads[enqueued][p][r] = create_thread(sched_bucket, (r == e_recc) ? e_tg : p_tg,
				    root_bucket_to_highest_pri[sched_bucket]);
				enqueue_thread(pset_target(p), threads[enqueued][p][r]);
				T_LOG("Enqueued thread %p on pset %d, recc %d", threads[enqueued][p][r], p, r);
				threads[running][p][r] = create_thread(sched_bucket, (r == e_recc) ? e_tg : p_tg,
				    root_bucket_to_highest_pri[sched_bucket]);
				int run_cpu_id = pset_id_to_cpu_id(p) + r;
				cpu_set_thread_current(run_cpu_id, threads[running][p][r]);
			}
		}
		int other_p_pset = 3;
		int other_e_pset = 2;
		if (config == 0) {
			/* ~~~~~ P-core steal/idle path ~~~~~ */
			/* 1. Foreign rebalance steal */
			foreign_steal_expect(other_p_pset, e_pset, threads, p_recc);
			foreign_steal_expect(other_p_pset, other_e_pset, threads, p_recc);
			/* 2. Native work-steal */
			work_steal_expect(other_p_pset, p_pset, threads, p_recc);
			/* 3. Running rebalance */
			no_steal_expect(other_p_pset, "Want to perform running rebalance");
			running_rebalance_expect(other_p_pset, "E", 2,
			    (int[]){pset_id_to_cpu_id(e_pset) + p_recc, pset_id_to_cpu_id(other_e_pset) + p_recc});
			cpu_clear_thread_current(pset_id_to_cpu_id(e_pset) + p_recc);
			cpu_clear_thread_current(pset_id_to_cpu_id(other_e_pset) + p_recc);
			/* 4. Work-steal from anywhere allowed */
			no_steal_expect(other_p_pset, "Nothing left a P-core wants to steal");
			SCHED_POLICY_PASS("Verified steal order steps for stealing P-core");
		} else {
			/* ~~~~~ E-core steal/idle path ~~~~~ */
			/* 1. Foreign rebalance steal */
			/* Foreign pset search starts with highest id */
			foreign_steal_expect(other_e_pset, p_pset, threads, e_recc);
			foreign_steal_expect(other_e_pset, other_p_pset, threads, e_recc);
			/* 2. Native work-steal */
			work_steal_expect(other_e_pset, e_pset, threads, e_recc);
			work_steal_expect(other_e_pset, e_pset, threads, p_recc);
			/* 3. Running rebalance */
			no_steal_expect(other_e_pset, "Want to perform running rebalance");
			running_rebalance_expect(other_e_pset, "P", 2,
			    (int[]){pset_id_to_cpu_id(p_pset) + e_recc, pset_id_to_cpu_id(other_p_pset) + e_recc});
			cpu_clear_thread_current(pset_id_to_cpu_id(p_pset) + e_recc);
			cpu_clear_thread_current(pset_id_to_cpu_id(other_p_pset) + e_recc);
			/* 4. Work-steal from anywhere allowed */
			for (int i = 0; i < 2; i++) {
				int src_pset = (i == 0) ? other_p_pset : p_pset;
				no_steal_expect(other_e_pset, "Non-zero edge (P->E) steal requires excess "
				    "threads in the runqueue");
				cpu_set_thread_current(pset_id_to_cpu_id(src_pset) + e_recc,
				    create_thread(sched_bucket, p_tg, root_bucket_to_highest_pri[sched_bucket]));
				work_steal_expect(other_e_pset, src_pset, threads, p_recc);
			}
			no_steal_expect(other_e_pset, "Nothing left of interest to steal");
			SCHED_POLICY_PASS("Verified steal order steps for stealing E-core");
		}
		clear_threads_from_topo();
	}
}

static bool shush = false;

static void
work_steal_expect_simple(int stealing_pset, int stolen_from_pset,
    test_thread_t stolen_thread, char *msg)
{
	int ret;
	test_thread_t found_thread = cpu_steal_thread(pset_id_to_cpu_id(stealing_pset));
	if (shush) {
		T_QUIET;
	}
	T_EXPECT_EQ(found_thread, stolen_thread, msg);
	ret = tracepoint_expect(EDGE_STEAL, get_thread_tid(stolen_thread), stealing_pset, stolen_from_pset, 0);
	T_QUIET; T_EXPECT_TRUE(ret, "EDGE_STEAL tracepoint for %s", msg);
}

SCHED_POLICY_T_DECL(migration_steal_only_excess_by_qos, "Verify that steal logic "
    "only steals across hetergeneous psets when there are excess threads at that QoS")
{
	init_migration_harness(dual_die);
	int p_pset = 1;
	int p_pset_cpus = get_hw_topology().psets[p_pset].num_cpus;
	int e_pset = 0;
	int other_p_pset = 2;

	/* Load P-pset core-by-core until there's an excess thread for E-pset to steal */
	test_thread_t default_threads[p_pset_cpus + 1];
	struct thread_group *tg = create_tg(0);
	set_tg_sched_bucket_preferred_pset(tg, TH_BUCKET_SHARE_DF, p_pset);
	for (int i = 0; i < p_pset_cpus + 1; i++) {
		default_threads[i] = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	}
	for (int i = 0; i < p_pset_cpus; i++) {
		enqueue_thread(pset_target(p_pset), default_threads[i]);
		increment_mock_time_us(5); // Get FIFO order out
		no_steal_expect(e_pset, "No excess threads yet");
	}
	enqueue_thread(pset_target(p_pset), default_threads[p_pset_cpus]);
	work_steal_expect_simple(e_pset, p_pset, default_threads[0], "P->E Excess thread stolen");
	no_steal_expect(e_pset, "Back to no excess threads");
	/* Allow P-pset to swipe up non-excess threads */
	for (int i = 1; i < p_pset_cpus + 1; i++) {
		work_steal_expect_simple(other_p_pset, p_pset, default_threads[i],
		    "Homogenous (P->P) can steal non-excess threads");
	}
	no_steal_expect(other_p_pset, "All threads stolen already");
	SCHED_POLICY_PASS("Heterogenous psets only steal excess threads, while homogeneous steal any");
	clear_threads_from_topo();

	/* Enqueue "pyramid" of threads at different QoSes */
	test_thread_t per_qos_threads[TH_BUCKET_SCHED_MAX];
	for (int bucket = 0; bucket < TH_BUCKET_SCHED_MAX; bucket++) {
		set_tg_sched_bucket_preferred_pset(tg, bucket, p_pset);
		per_qos_threads[bucket] = create_thread(bucket, tg, root_bucket_to_highest_pri[bucket]);
		if (bucket == 0) {
			set_thread_sched_mode(per_qos_threads[bucket], TH_MODE_FIXED);
		}
	}
	for (int bucket = 0; bucket < TH_BUCKET_SCHED_MAX; bucket++) {
		enqueue_thread(pset_target(p_pset), per_qos_threads[bucket]);
		if (bucket < p_pset_cpus) {
			no_steal_expect(e_pset, "No excess threads yet");
		}
	}
	for (int qos_with_excess = p_pset_cpus; qos_with_excess < TH_BUCKET_SCHED_MAX; qos_with_excess++) {
		work_steal_expect_simple(e_pset, p_pset, per_qos_threads[qos_with_excess],
		    "Steal from highest QoS with non-idle load");
	}
	SCHED_POLICY_PASS("Heterogeneous psets only steal from excess QoSes");
}

static test_pset_t pair_p_psets[2] = {
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 1,
		.cluster_id = 0,
		.die_id = 0,
	},
	{
		.cpu_type = TEST_CPU_TYPE_PERFORMANCE,
		.num_cpus = 1,
		.cluster_id = 1,
		.die_id = 0,
	},
};
test_hw_topology_t pair_p = {
	.psets = &pair_p_psets[0],
	.num_psets = 2,
	.total_cpus = 2,
};

SCHED_POLICY_T_DECL(migration_steal_no_cluster_bound,
    "Verify that cluster-bound threads do not get stolen to a different pset")
{
	init_migration_harness(pair_p);
	int load_multiplier = 10;
	int loaded_pset = 0;
	int idle_pset = 1;
	int num_bound_threads = pair_p.psets[loaded_pset].num_cpus * load_multiplier;
	enum { eBound = 0, eNativeFirst = 1, eRoundRobin = 2, eMax = 3 } bound_type;
	test_thread_t bound_threads[eMax][num_bound_threads];
	struct thread_group *tg = create_tg(0);
	for (bound_type = 0; bound_type < eMax; bound_type++) {
		for (int i = 0; i < num_bound_threads; i++) {
			bound_threads[bound_type][i] = create_thread(TH_BUCKET_SHARE_DF, tg,
			    root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
			switch (bound_type) {
			case eBound:
				set_thread_cluster_bound(bound_threads[bound_type][i], loaded_pset);
				break;
			case eNativeFirst:
				edge_set_thread_shared_rsrc(bound_threads[bound_type][i], true);
				break;
			case eRoundRobin:
				edge_set_thread_shared_rsrc(bound_threads[bound_type][i], false);
				break;
			default:
				T_QUIET; T_ASSERT_FAIL("Invalid bound case");
			}
			increment_mock_time_us(5); // Get FIFO order
			enqueue_thread(pset_target(loaded_pset), bound_threads[bound_type][i]);
		}
		no_steal_expect(idle_pset, "Refuse to steal cluster bound threads");
	}
	test_thread_t unbound_thread = create_thread(TH_BUCKET_SHARE_DF, tg,
	    root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	increment_mock_time_us(5);
	enqueue_thread(pset_target(loaded_pset), unbound_thread);
	work_steal_expect_simple(idle_pset, loaded_pset, unbound_thread,
	    "Pluck out the unbound thread to steal");
	no_steal_expect(idle_pset, "Still refuse to steal cluster bound threads");
	SCHED_POLICY_PASS("Cluster bound threads cannot be stolen");
}

SCHED_POLICY_T_DECL(migration_steal_highest_pri,
    "Verify that higher priority threads are stolen first, across silos")
{
	init_migration_harness(pair_p);
	int idle_pset = 0;
	int loaded_pset = 1;
	int max_pri_to_subtract = 4;
	int high_bucket = TH_BUCKET_SHARE_FG;
	int low_bucket = TH_BUCKET_SHARE_BG;
	int num_buckets = low_bucket - high_bucket + 1;
	int num_silos = 2;
	int num_threads = num_silos * num_buckets * (max_pri_to_subtract + 1);
	test_thread_t threads[num_threads];
 #define silo_bucket_pri_to_ind(silo, bucket, sub_pri) \
	(silo * (num_buckets * (max_pri_to_subtract + 1)) + \
	            (bucket - high_bucket) * ((max_pri_to_subtract + 1)) + sub_pri)
	/* Create a bunch of threads for the different silos, buckets, and priority values */
	for (int s = 0; s < num_silos; s++) {
		struct thread_group *silo_tg = create_tg(0);
		for (int b = high_bucket; b <= low_bucket; b++) {
			set_tg_sched_bucket_preferred_pset(silo_tg, b, s);
			for (int p = 0; p <= max_pri_to_subtract; p++) {
				threads[silo_bucket_pri_to_ind(s, b, p)] =
				    create_thread(b, silo_tg, root_bucket_to_highest_pri[b] - p);
			}
		}
	}
	/* Despite enqueueing in a random order, the threads should be stolen out in priority order */
	int rand_seed = 777777;
	enqueue_threads_arr_rand_order(pset_target(loaded_pset), rand_seed, num_threads, threads);
	shush = true; // Quiet work_steal_expect_simple()'s expects
	for (int b = high_bucket; b <= low_bucket; b++) {
		for (int p = 0; p <= max_pri_to_subtract; p++) {
			for (int s = 0; s < num_silos; s++) {
				T_QUIET; work_steal_expect_simple(idle_pset, loaded_pset,
				    threads[silo_bucket_pri_to_ind(s, b, p)], "Higher pri threads stolen first");
			}
		}
	}
	shush = false;
	no_steal_expect(idle_pset, "Already stole all the threads");
	SCHED_POLICY_PASS("Higher priority threads stolen first across silos");
}

SCHED_POLICY_T_DECL(migration_harmonious_chosen_pset,
    "Verify that different migration mechanisms agree about where a thread "
    "should be, given current system conditions")
{
	int ret;
	test_hw_topology_t topo = SCHED_POLICY_DEFAULT_TOPO;
	init_migration_harness(topo);
	int sched_bucket = TH_BUCKET_SHARE_DF;
	struct thread_group *tg = create_tg(0);
	thread_t thread = create_thread(sched_bucket, tg, root_bucket_to_highest_pri[sched_bucket]);
	int max_load_threads = 20;
	test_thread_t load_threads[max_load_threads];
	for (int i = 0; i < max_load_threads; i++) {
		load_threads[i] = create_thread(sched_bucket, tg, root_bucket_to_highest_pri[sched_bucket]);
	}

	/* Iterate conditions with different preferred psets and pset loads */
	for (int preferred_pset_id = 0; preferred_pset_id < topo.num_psets; preferred_pset_id++) {
		set_tg_sched_bucket_preferred_pset(tg, sched_bucket, preferred_pset_id);
		sched_policy_push_metadata("preferred_pset_id", preferred_pset_id);
		for (int loaded_pset_id = 0; loaded_pset_id < topo.num_psets; loaded_pset_id++) {
			/* Load the loaded_pset */
			enqueue_threads_arr(pset_target(loaded_pset_id), max_load_threads, load_threads);
			bool preferred_is_idle = preferred_pset_id != loaded_pset_id;
			sched_policy_push_metadata("loaded_pset_id", loaded_pset_id);

			/* Where the thread proactively wants to go */
			int chosen_pset = choose_pset_for_thread(thread);
			bool chose_the_preferred_pset = chosen_pset == preferred_pset_id;
			if (preferred_is_idle) {
				T_QUIET; T_EXPECT_TRUE(chose_the_preferred_pset, "Should always choose the preferred pset if idle %s",
				    sched_policy_dump_metadata());
			}

			/* Thread generally should not avoid a processor in its chosen pset */
			for (int c = 0; c < topo.psets[chosen_pset].num_cpus; c++) {
				int avoid_cpu_id = pset_id_to_cpu_id(chosen_pset) + c;
				sched_policy_push_metadata("avoid_cpu_id", avoid_cpu_id);
				ret = thread_avoid_processor_expect(thread, avoid_cpu_id, false, false);
				T_QUIET; T_EXPECT_TRUE(ret, "Thread should not want to leave processor in just chosen pset %s",
				    sched_policy_dump_metadata());
				sched_policy_pop_metadata();
			}

			/* Extra assertions we can make based on the preferred pset being idle */
			if (preferred_is_idle) {
				/* Thread should avoid processor in non-preferred pset to get to the idle preferred pset */
				for (int c = 0; c < topo.total_cpus; c++) {
					if (cpu_id_to_pset_id(c) != preferred_pset_id) {
						sched_policy_push_metadata("avoid_non_preferred_cpu_id", c);
						ret = thread_avoid_processor_expect(thread, c, false, true);
						T_QUIET; T_EXPECT_TRUE(ret, "Thread should avoid processor in non-preferred pset to get to idle "
						    "preferred pset %s", sched_policy_dump_metadata());
						sched_policy_pop_metadata();
					}
				}
			}

			/* Other cores should not want to rebalance the running thread away from its chosen pset */
			int chosen_cpu = pset_id_to_cpu_id(chosen_pset);
			cpu_set_thread_current(chosen_cpu, thread);
			for (int c = 0; c < topo.total_cpus; c++) {
				if ((cpu_id_to_pset_id(c) != chosen_pset) && (cpu_id_to_pset_id(c) != loaded_pset_id)) {
					sched_policy_push_metadata("stealing_cpu_id", c);
					thread_t stolen_thread = cpu_steal_thread(c);
					if (stolen_thread != NULL) {
						T_QUIET; T_EXPECT_NE(stolen_thread, thread, "Should not steal back thread from its chosen_pset %s",
						    sched_policy_dump_metadata());
						if (stolen_thread != thread) {
							/* Put back the stolen load thread */
							enqueue_thread(pset_target(loaded_pset_id), stolen_thread);
						}
					}
					bool want_rebalance = cpu_processor_balance(c);
					T_QUIET; T_EXPECT_FALSE(want_rebalance, "Should not rebalance thread away from its chosen_pset %s",
					    sched_policy_dump_metadata());
					sched_policy_pop_metadata();
				}
			}

			(void)dequeue_threads_expect_ordered_arr(pset_target(loaded_pset_id), max_load_threads, load_threads);
			clear_threads_from_topo();
			for (int pset = 0; pset < topo.num_psets; pset++) {
				T_QUIET; T_EXPECT_TRUE(runqueue_empty(pset_target(pset)), "pset %d wasn't cleared at the end of test "
				    "scenario %s", pset, sched_policy_dump_metadata());
			}
			sched_policy_pop_metadata();
		}
		sched_policy_pop_metadata();
	}
	SCHED_POLICY_PASS("Policy is harmonious on the subject of a thread's chosen pset");
}

SCHED_POLICY_T_DECL(migration_search_order,
    "Verify that we iterate psets for spill and steal in the expected order")
{
	int ret;
	init_migration_harness(dual_die);
	int expected_orders[6][6] = {
		{0, 3, 1, 2, 4, 5},
		{1, 2, 4, 5, 0, 3},
		{2, 1, 4, 5, 0, 3},
		{3, 0, 4, 5, 1, 2},
		{4, 5, 1, 2, 3, 0},
		{5, 4, 1, 2, 3, 0},
	};
	for (int src_pset_id = 0; src_pset_id < dual_die.num_psets; src_pset_id++) {
		ret = iterate_pset_search_order_expect(src_pset_id, UINT64_MAX, 0, expected_orders[src_pset_id], dual_die.num_psets);
		T_QUIET; T_EXPECT_EQ(ret, -1, "Mismatched search order at ind %d for src_pset_id %d",
		    ret, src_pset_id);
	}
	SCHED_POLICY_PASS("Search order sorts on migration weight, then locality, then pset id");
	uint64_t p_mask = 0b110110;
	int expected_p_orders[6][6] = {
		{1, 2, 4, 5, -1, -1},
		{1, 2, 4, 5, -1, -1},
		{2, 1, 4, 5, -1, -1},
		{4, 5, 1, 2, -1, -1},
		{4, 5, 1, 2, -1, -1},
		{5, 4, 1, 2, -1, -1},
	};
	uint64_t e_mask = 0b001001;
	int expected_e_orders[6][6] = {
		{0, 3, -1, -1, -1, -1},
		{0, 3, -1, -1, -1, -1},
		{0, 3, -1, -1, -1, -1},
		{3, 0, -1, -1, -1, -1},
		{3, 0, -1, -1, -1, -1},
		{3, 0, -1, -1, -1, -1},
	};
	for (int i = 0; i < 2; i++) {
		for (int src_pset_id = 0; src_pset_id < dual_die.num_psets; src_pset_id++) {
			uint64_t mask = (i == 0) ? p_mask : e_mask;
			int *expected_order_masked = (i == 0) ? expected_p_orders[src_pset_id] : expected_e_orders[src_pset_id];
			ret = iterate_pset_search_order_expect(src_pset_id, mask, 0, expected_order_masked, dual_die.num_psets);
			T_QUIET; T_EXPECT_EQ(ret, -1, "Mismatched masked search order at ind %d for src_pset_id %d",
			    ret, src_pset_id);
		}
	}
	SCHED_POLICY_PASS("Search order traversal respects candidate mask");
}
