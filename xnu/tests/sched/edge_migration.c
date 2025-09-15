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
	test_thread_t background = create_thread(TH_BUCKET_SHARE_BG, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_BG]);
	test_thread_t yielder = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	cpu_set_thread_current(0, yielder);
	ret = cpu_check_should_yield(0, false);
	T_QUIET; T_EXPECT_TRUE(ret, "No thread present to yield to");
	enqueue_thread(pset_target(0), background);
	ret = cpu_check_should_yield(0, true);
	T_QUIET; T_EXPECT_TRUE(ret, "Should yield to a low priority thread on the current runqueue");
	SCHED_POLICY_PASS("Basic yield behavior on single pset");

	ret = dequeue_thread_expect(pset_target(0), background);
	T_QUIET; T_EXPECT_TRUE(ret, "Only background thread in runqueue");
	cpu_set_thread_current(0, yielder); /* Reset current thread */
	enqueue_thread(pset_target(1), background);
	ret = cpu_check_should_yield(0, true);
	T_QUIET; T_EXPECT_TRUE(ret, "Should yield in order to steal thread");
	ret = dequeue_thread_expect(pset_target(1), background);
	T_QUIET; T_EXPECT_TRUE(ret, "Only background thread in runqueue");
	cpu_set_thread_current(pset_id_to_cpu_id(1), background);
	ret = cpu_check_should_yield(pset_id_to_cpu_id(1), false);
	T_QUIET; T_EXPECT_TRUE(ret, "Should not yield in order to rebalance (presumed) native thread");
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
	int p_cpu = 0;
	int e_cpu = 2;
	int other_e_cpu = 3;
	int other_p_cpu = 1;
	cpu_set_thread_current(p_cpu, starts_p);
	cpu_set_thread_current(e_cpu, starts_e);
	int p_pset = 0;
	set_pset_load_avg(p_pset, TH_BUCKET_SHARE_DF, 10000000);
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
	test_thread_t other_p_thread = create_thread(TH_BUCKET_SHARE_DF, tg, root_bucket_to_highest_pri[TH_BUCKET_SHARE_DF]);
	cpu_set_thread_current(other_p_cpu, other_p_thread);
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
			// TODO: Test properly updated load average
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
