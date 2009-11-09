#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#define LAPS (1024 * 1024)
#define THREADS 1

static pthread_rwlock_t pthr_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static semaphore_t wake_port;
static void (*test_func)(void *);
static unsigned int thr_count;

static void
reader(void *ctxt __attribute__((unused)))
{
}

static void
pthr_worker(void *ctxt __attribute__((unused)))
{
	size_t i;
	int r;

	for (i = 0; i < (LAPS / THREADS); i++) {
		r = pthread_rwlock_rdlock(&pthr_rwlock);
		assert(r == 0);
		r = pthread_rwlock_unlock(&pthr_rwlock);
		assert(r == 0);
	}
}

static void
gcd_worker(void *ctxt)
{
	dispatch_queue_t dq = ctxt;
	size_t i;

	for (i = 0; i < (LAPS / THREADS); i++) {
		dispatch_read_sync_f(dq, NULL, reader);
	}
}

static void *
worker(void *ctxt)
{
	kern_return_t kr;

	__sync_add_and_fetch(&thr_count, 1);

	kr = semaphore_wait(wake_port);
	assert(kr == 0);

	test_func(ctxt);

	if (__sync_sub_and_fetch(&thr_count, 1) == 0) {
		kr = semaphore_signal(wake_port);
		assert(kr == 0);
	}

	return NULL;
}

static uint64_t
benchmark(void *ctxt)
{
	pthread_t pthr[THREADS];
	uint64_t cycles;
	void *rval;
	size_t i;
	int r;

	for (i = 0; i < THREADS; i++) {
		r = pthread_create(&pthr[i], NULL, worker, ctxt);
		assert(r == 0);
	}

	while (thr_count != THREADS) {
		sleep(1);
	}

	sleep(1);

	cycles = dispatch_benchmark(1, ^{
		kern_return_t kr;

		kr = semaphore_signal_all(wake_port);
		assert(kr == 0);
		kr = semaphore_wait(wake_port);
		assert(kr == 0);
	});

	for (i = 0; i < THREADS; i++) {
		r = pthread_join(pthr[i], &rval);
		assert(r == 0);
	}

	return cycles;
}

int
main(void)
{
	uint64_t pthr_cycles, gcd_cycles;
	long double ratio;
	dispatch_queue_t dq;
	kern_return_t kr;
	int r;

	dq = dispatch_queue_create("test", NULL);
	assert(dq);

	// pthreads lazily inits the object
	// do not benchmark that fact
	r = pthread_rwlock_rdlock(&pthr_rwlock);
	assert(r == 0);
	r = pthread_rwlock_unlock(&pthr_rwlock);
	assert(r == 0);

	kr = semaphore_create(mach_task_self(), &wake_port, SYNC_POLICY_FIFO, 0);
	assert(kr == 0);

	test_func = pthr_worker;
	pthr_cycles = benchmark(NULL);

	test_func = gcd_worker;
	gcd_cycles = benchmark(dq);

	dispatch_release(dq);

	ratio = pthr_cycles;
	ratio /= gcd_cycles;

	printf("Cycles:\n\tPOSIX\t%llu\n", pthr_cycles);
	printf("\tGCD\t%llu\n", gcd_cycles);
	printf("Ratio:\t%Lf\n", ratio);

	return 0;
}
