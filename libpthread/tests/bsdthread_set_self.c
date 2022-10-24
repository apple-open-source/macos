#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/qos.h>

#include <pthread.h>
#include <pthread/tsd_private.h>
#include <pthread/qos_private.h>
#include <pthread/workqueue_private.h>

#include <mach/thread_policy_private.h>

#include <dispatch/dispatch.h>
#include <dispatch/private.h>

#include "darwintest_defaults.h"

T_DECL(bsdthread_set_self_constrained_transition, "bsdthread_ctl(SET_SELF) with overcommit change",
		T_META_ALL_VALID_ARCHS(YES))
{
	dispatch_async(dispatch_get_global_queue(0, 0), ^{
		pthread_priority_t overcommit = (pthread_priority_t)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS) |
			_PTHREAD_PRIORITY_OVERCOMMIT_FLAG;
		pthread_priority_t constrained = overcommit & (~_PTHREAD_PRIORITY_OVERCOMMIT_FLAG);

		T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG, overcommit, 0), NULL);
		T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG, constrained, 0), NULL);
		T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG, overcommit, 0), NULL);
		T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG, constrained, 0), NULL);
		T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG, overcommit, 0), NULL);
		T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG, constrained, 0), NULL);

		T_END;
	});

	dispatch_main();
}

T_DECL(bsdthread_set_self_constrained_threads, "bsdthread_ctl(SET_SELF) with overcommit change",
		T_META_CHECK_LEAKS(NO), T_META_ALL_VALID_ARCHS(YES))
{
	static const int THREADS = 128;
	static atomic_int threads_started;
	dispatch_queue_t q = dispatch_queue_create("my queue", DISPATCH_QUEUE_CONCURRENT);
	dispatch_set_target_queue(q, dispatch_get_global_queue(0, 0));
	for (int i = 0; i < THREADS; i++) {
		dispatch_async(q, ^{
			int thread_id = ++threads_started;
			T_PASS("Thread %d started successfully", thread_id);
			if (thread_id == THREADS){
				T_PASS("All threads started successfully");
				T_END;
			}

			pthread_priority_t overcommit = (pthread_priority_t)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS) |
				_PTHREAD_PRIORITY_OVERCOMMIT_FLAG;
			T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG, overcommit, 0), NULL);

			uint64_t t = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
			while (t > clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - (thread_id == 1 ? 30 : 60) * NSEC_PER_SEC) {
				sleep(1);
			}
			if (thread_id == 1) {
				T_FAIL("Where are my threads?");
				T_END;
			}
		});
	}

	dispatch_main();
}

static void
do_test_qos_class_encode_decode()
{
	qos_class_t qos = QOS_CLASS_UTILITY;
	int relpri = -10;
	unsigned long flags = _PTHREAD_PRIORITY_OVERCOMMIT_FLAG | _PTHREAD_PRIORITY_NEEDS_UNBIND_FLAG;
	pthread_priority_t pp = _pthread_qos_class_encode(qos, relpri, flags);

	unsigned long out_flags;
	int out_relpri;
	T_ASSERT_EQ(qos, _pthread_qos_class_decode(pp, &out_relpri, &out_flags), NULL);
	T_ASSERT_EQ(out_flags, flags, NULL);
	T_ASSERT_EQ(out_relpri, relpri, NULL);
}

static void
do_test_sched_pri_encode_decode()
{
	int pri = 55;
	unsigned long flags = _PTHREAD_PRIORITY_NEEDS_UNBIND_FLAG;
	pthread_priority_t pp = _pthread_sched_pri_encode(pri, flags);

	unsigned long out_flags;
	T_ASSERT_EQ(pri, _pthread_sched_pri_decode(pp, &out_flags), NULL);
	T_ASSERT_EQ(flags, out_flags, NULL);
}

static void
do_test_override_qos_encode_decode()
{
	qos_class_t qos = QOS_CLASS_UTILITY;
	qos_class_t ovr = QOS_CLASS_USER_INITIATED;
	int relpri = -10;
	unsigned long flags = _PTHREAD_PRIORITY_OVERCOMMIT_FLAG | _PTHREAD_PRIORITY_NEEDS_UNBIND_FLAG;
	pthread_priority_t pp = _pthread_qos_class_and_override_encode(qos, relpri, flags, ovr);

	unsigned long out_flags;
	int out_relpri;
	qos_class_t out_ovr;
	T_ASSERT_EQ(qos, _pthread_qos_class_and_override_decode(pp, &out_relpri, &out_flags, &out_ovr), NULL);
	T_ASSERT_EQ(out_flags, flags, NULL);
	T_ASSERT_EQ(out_relpri, relpri, NULL);
	T_ASSERT_EQ(out_ovr, ovr, NULL);
}

T_DECL(pthread_encode_decode_SPIs, "Testing that pthread encode decode SPIs give sane results")
{
	do_test_qos_class_encode_decode();
	do_test_sched_pri_encode_decode();
	do_test_override_qos_encode_decode();
}

#define MAX(a, b) ((a > b) ? a : b)

thread_qos_t
get_thread_override(void)
{
	struct thread_policy_state thps;
	mach_msg_type_number_t count = THREAD_POLICY_STATE_COUNT;
	boolean_t get_default = FALSE;
	mach_port_t th = pthread_mach_thread_np(pthread_self());
	kern_return_t kr = thread_policy_get(th, THREAD_POLICY_STATE,
	    (thread_policy_t)&thps, &count, &get_default);
	if (kr) {
		T_FAIL("Unable to get current thread policy - %d", kr);
	}

	struct thread_requested_policy trp = *((struct thread_requested_policy *)&thps.thps_requested_policy);
	int o = THREAD_QOS_UNSPECIFIED;
	o = MAX(o, trp.thrp_qos_override);
	o = MAX(o, trp.thrp_qos_promote);
	o = MAX(o, trp.thrp_qos_kevent_override);
	o = MAX(o, trp.thrp_qos_wlsvc_override);
	o = MAX(o, trp.thrp_qos_workq_override);
	return o;
}

T_DECL(set_override_and_base_pri, "Re-adjusting requested QoS to set qos and override", T_META_ASROOT(YES))
{
	dispatch_queue_t dq = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, DISPATCH_QUEUE_COOPERATIVE);
	dispatch_async(dq, ^{
		T_ASSERT_EQ(qos_class_self(), QOS_CLASS_USER_INITIATED, "Expected req qos == IN");
		T_ASSERT_EQ(THREAD_QOS_UNSPECIFIED, get_thread_override(), "Thread override is UN");

		pthread_priority_t old_pp = (pthread_priority_t)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS);
		unsigned long flags; int relpri;
		_pthread_qos_class_decode(old_pp, &relpri, &flags);

		// Re-adjust QoS IN thread to a QoS UT with IN override
		pthread_priority_t new_pp = _pthread_qos_class_and_override_encode(QOS_CLASS_UTILITY, relpri, flags, QOS_CLASS_USER_INITIATED);

		T_LOG("Old priority = %#x", old_pp);
		T_LOG("New priority = %#x", new_pp);

		T_ASSERT_POSIX_ZERO(_pthread_set_properties_self(_PTHREAD_SET_SELF_QOS_FLAG | _PTHREAD_SET_SELF_QOS_OVERRIDE_FLAG, new_pp, 0), NULL);

		T_ASSERT_EQ(qos_class_self(), QOS_CLASS_UTILITY, "Expected req QoS is now UT");
		T_ASSERT_EQ(THREAD_QOS_USER_INITIATED, get_thread_override(), "Thread override is IN instead");

		T_END;
	});

	dispatch_main();
}
