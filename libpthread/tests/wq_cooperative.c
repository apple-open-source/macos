#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/qos.h>

#include <dispatch/dispatch.h>
#include <os/lock.h>
#include <mach/mach.h>

#include "../private/pthread/workqueue_private.h"
#include "../private/pthread/qos_private.h"

#include "wq_kevent.h"

_Atomic static int tick = 0;
_Atomic static int phase = 0;

static os_unfair_lock lock1 = OS_UNFAIR_LOCK_INIT;
static os_unfair_lock lock2 = OS_UNFAIR_LOCK_INIT;
static dispatch_group_t group;
static dispatch_group_t group1;
static dispatch_group_t group2;
static dispatch_time_t timeout;

/* Bookkeeping for the number of threads used to satisfy the number of requests */
struct threadlist {
	os_unfair_lock lock;
	pthread_t *array;
	size_t count;
};
static struct threadlist **num_cooperative_threads;

static void
record_thread(int thread_phase, int workq_func_step, pthread_priority_t pp)
{
	unsigned long flags;
	qos_class_t qos = _pthread_qos_class_decode(pp, NULL, &flags);
	char *thread_type;

	if (flags & _PTHREAD_PRIORITY_COOPERATIVE_FLAG) {
		thread_type = "cooperative";
	} else if (flags & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG) {
		thread_type = "overcommit";
	} else {
		thread_type = "non-overcommit";
	}

	pthread_t self = pthread_self();
	fprintf(stdout, "\t[t = %d] phase %d, step %d: %s thread 0x%p at qos %s\n",
			tick++, thread_phase, workq_func_step, thread_type, self,
			QOS_STR(qos));

	if ((flags & _PTHREAD_PRIORITY_COOPERATIVE_FLAG) == 0) {
		return;
	}

	struct threadlist *list = num_cooperative_threads[thread_phase];
	os_unfair_lock_lock(&list->lock);

	for (size_t i = 0; i < list->count; i++) {
		if (list->array[i] == self) {
			os_unfair_lock_unlock(&list->lock);
			return;
		}
	}
	list->count++;
	list->array = realloc(list->array, sizeof(pthread_t) * list->count);
	list->array[list->count - 1] = self;

	os_unfair_lock_unlock(&list->lock);

}

__attribute__((noinline))
static void
block_on_lock1(void)
{
	os_unfair_lock_lock(&lock1);
	os_unfair_lock_unlock(&lock1);
}

__attribute__((noinline))
static void
block_on_lock2(void)
{
	os_unfair_lock_lock(&lock2);
	os_unfair_lock_unlock(&lock2);
}

static void
workqueue_func(pthread_priority_t priority)
{
	dispatch_group_enter(group1);
	dispatch_group_enter(group2);

	record_thread(phase, 1, priority);
	dispatch_group_leave(group1);

	block_on_lock1();

	record_thread(phase, 2, priority);
	dispatch_group_leave(group2);

	block_on_lock2();

	record_thread(phase, 3, priority);
	burn_cpu();
	burn_cpu();
	dispatch_group_leave(group);
}

static void
req_cooperative_wq_threads(qos_class_t qos, size_t num_threads)
{
	int ret;

	for (size_t i = 0; i < num_threads; i++) {
		dispatch_group_enter(group);
		ret = _pthread_workqueue_add_cooperativethreads(1,
				_pthread_qos_class_encode(qos, 0, 0));
		assert(ret == 0);
	}
}

static void
req_wq_threads(qos_class_t qos, size_t num_threads, bool overcommit)
{
	int ret;

	for (size_t i = 0; i < num_threads; i++) {
		dispatch_group_enter(group);
		ret = _pthread_workqueue_addthreads(1,
				_pthread_qos_class_encode(qos, 0,
				(overcommit ? _PTHREAD_PRIORITY_OVERCOMMIT_FLAG : 0)));
		assert(ret == 0);
	}
}

static uint32_t
ncpus(void)
{
	static uint32_t num_cpus;
	if (!num_cpus) {
		uint32_t n;
		size_t s = sizeof(n);
		sysctlbyname("hw.ncpu", &n, &s, NULL, 0);
		num_cpus = n;
	}
	return num_cpus;
}

static void
test_within_cooperative_pool(void)
{
	fprintf(stdout, "Testing cooperative thread requests...\n");
	size_t n = ncpus();
	num_cooperative_threads = calloc(sizeof(struct threadlist *), 3);
	num_cooperative_threads[0] = calloc(sizeof(struct threadlist), 1);
	num_cooperative_threads[1] = calloc(sizeof(struct threadlist), 1);
	num_cooperative_threads[2] = calloc(sizeof(struct threadlist), 1);

	/* Makes sure that threads are blocked when they come out */
	phase = 0;
	os_unfair_lock_lock(&lock1);
	os_unfair_lock_lock(&lock2);

	/* We should be saturating the pool with ncpu threads at UI */
	req_cooperative_wq_threads(QOS_CLASS_USER_INTERACTIVE, n);

	sleep(1);

	dispatch_group_wait(group1, timeout);

	phase = 1;
	os_unfair_lock_unlock(&lock1);

	/* 1 thread for qos UT to make sure bucket doesn't go empty */
	req_cooperative_wq_threads(QOS_CLASS_UTILITY, 2 * n);
	/* 1 thread for qos IN to make sure bucket doesn't go empty */
	req_cooperative_wq_threads(QOS_CLASS_USER_INITIATED, n);

	/* No threads for UT */
	req_cooperative_wq_threads(QOS_CLASS_UTILITY, 2 * n);

	usleep(500);
	dispatch_group_wait(group2, timeout);

	phase = 2;
	os_unfair_lock_unlock(&lock2);

	dispatch_group_wait(group, timeout);
	fprintf(stdout, "\n[test_within_cooperative_pool] sucessfully completed cooperative pool workload\n");

	fprintf(stdout, "[test_within_cooperative_pool] phase 0: Num cooperative threads = %zu\n", num_cooperative_threads[0]->count);
	assert(num_cooperative_threads[0]->count <= n);
	fprintf(stdout, "[test_within_cooperative_pool] phase 1: Num cooperative threads = %zu\n", num_cooperative_threads[1]->count);
	assert(num_cooperative_threads[1]->count <= n + 2);
	fprintf(stdout, "[test_within_cooperative_pool] phase 2: Num cooperative threads = %zu\n", num_cooperative_threads[2]->count);

	for (int i = 0; i < 3; i++) {
		free(num_cooperative_threads[i]->array);
		free(num_cooperative_threads[i]);
	}
	free(num_cooperative_threads);
	num_cooperative_threads = NULL;
}

static void
test_overcommit_and_cooperative_pool(void)
{
	fprintf(stdout, "Testing cooperative and overcommit thread requests together...\n");
	size_t n = ncpus();
	num_cooperative_threads = calloc(sizeof(struct threadlist *), 3);
	num_cooperative_threads[0] = calloc(sizeof(struct threadlist), 1);
	num_cooperative_threads[1] = calloc(sizeof(struct threadlist), 1);
	num_cooperative_threads[2] = calloc(sizeof(struct threadlist), 1);

	/* Makes sure that threads are blocked when they come out */
	phase = 0;
	os_unfair_lock_lock(&lock1);
	os_unfair_lock_lock(&lock2);

	/* Even though we asked for overcommit threads first, we should be seeing
	 * that we get cooperative requests out first */
	req_wq_threads(QOS_CLASS_USER_INTERACTIVE, n, true);
	req_cooperative_wq_threads(QOS_CLASS_USER_INTERACTIVE, n);

	/* Give some time to let the threads all come out to userspace */
	usleep(500);

	/* All the threads are now recorded and about to block on the lock1 */
	dispatch_group_wait(group1, timeout);

	phase = 1;
	os_unfair_lock_unlock(&lock1);

	req_cooperative_wq_threads(QOS_CLASS_UTILITY, 2 * n); // 1 thread
	req_cooperative_wq_threads(QOS_CLASS_USER_INITIATED, n); // 1 thread

	/* All the threads are now recorded and about to block on the lock2 */
	dispatch_group_wait(group2, timeout);

	phase = 2;
	os_unfair_lock_unlock(&lock2);

	dispatch_group_wait(group, timeout);

	fprintf(stdout, "\n[test_overcommit_and_cooperative_pool] sucessfully completed cooperative pool workload\n");

	fprintf(stdout, "[test_overcommit_and_cooperative_pool] phase 0: Num cooperative threads = %zu\n", num_cooperative_threads[0]->count);
	assert(num_cooperative_threads[0]->count <= n);
	fprintf(stdout, "[test_overcommit_and_cooperative_pool] phase 1: Num cooperative threads = %zu\n", num_cooperative_threads[1]->count);
	assert(num_cooperative_threads[1]->count <= n + 2);

	fprintf(stdout, "[test_overcommit_and_cooperative_pool] phase 2: Num cooperative threads = %zu\n", num_cooperative_threads[2]->count);
	/* It's difficult to evaluate this cause what we care about is the number of
	 * threads working on a thread request at any given time - even if the
	 * threads are not unique and are reused (cause overcommit threads picked up
	 * cooperative requests) */

	for (int i = 0; i < phase; i++) {
		free(num_cooperative_threads[i]->array);
		free(num_cooperative_threads[i]);
	}
	free(num_cooperative_threads);
}


int main(int __unused argc, char * __unused argv[]) {
	int ret = 0;

	group = dispatch_group_create();
	assert(group != NULL);
	group1 = dispatch_group_create();
	assert(group1 != NULL);
	group2 = dispatch_group_create();
	assert(group2 != NULL);
	timeout = DISPATCH_TIME_FOREVER;

	ret = _pthread_workqueue_init(workqueue_func, 0, 0);
	assert(ret == 0);

	test_within_cooperative_pool();

	tick = 0;
	fprintf(stdout, "\n");

	test_overcommit_and_cooperative_pool();

	return 0;
}
