#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc/malloc.h>
#include <darwintest.h>

#include <../src/internal.h> // for platform.h

#if !MALLOC_TARGET_EXCLAVES
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <perfcheck_keys.h>
typedef unsigned seed_type_t;
#else
typedef unsigned long seed_type_t;
#endif // !MALLOC_TARGET_EXCLAVES

T_GLOBAL_META(T_META_TAG_PERF, T_META_TAG_XZONE, T_META_TAG_VM_NOT_PREFERRED);

// number of times malloc & free are called per dt_stat batch
#define ITERATIONS_PER_DT_STAT_BATCH 10000ull
// number of times large malloc is called per dt_stat  batch
#define ITERATIONS_PER_DT_STAT_BATCH_LARGE_MALLOC 1000ull
// max number of allocations kept live during the benchmark (< iterations above)
#define LIVE_ALLOCATIONS 256
// maintain and print progress counters in between measurement batches
#define COUNTERS 0

#define MAX_THREADS 32

// move the darwintest assertion code out of the straight line execution path
// since it is has non-trivial overhead and codegen impact even if the assertion
// is never triggered.
#define iferr(_e) if(__builtin_expect(!!(_e), 0))

#pragma mark -

static uint64_t
random_busy_counts(seed_type_t *seed, uint64_t *first, uint64_t *second)
{
	uint64_t random = rand_r(seed);
	*first = 0x4 + (random & (0x10 - 1));
	random >>= 4;
	*second = 0x4 + (random & (0x10 - 1));
	random >>= 4;
	return random;
}

// By default busy() does no cpu busy work in the malloc bench
enum {
	busy_is_nothing = 0,
	busy_is_cpu_busy,
	busy_is_cpu_yield,
};
static int busy_select = busy_is_nothing;

static double
cpu_busy(uint64_t n)
{
	double d = M_PI;
	uint64_t i;
	for (i = 0; i < n; i++) d *= M_PI;
	return d;
}

static double
cpu_yield(uint64_t n)
{
	uint64_t i;
	for (i = 0; i < n; i++) {
#if defined(__arm__) || defined(__arm64__)
	asm volatile("yield");
#elif defined(__x86_64__) || defined(__i386__)
	asm volatile("pause");
#else
#error Unrecognized architecture
#endif
	}
	return 0;
}

__attribute__((noinline))
static double
busy(uint64_t n)
{
	switch(busy_select) {
	case busy_is_cpu_busy:
		return cpu_busy(n);
	case busy_is_cpu_yield:
		return cpu_yield(n);
	default:
		return 0;
	}
}

#pragma mark -

#if MALLOC_TARGET_EXCLAVES
static pthread_cond_t start_cond, end_cond;
static pthread_mutex_t start_mut, end_mut;
static uint32_t num_waiting_start, num_waiting_end;
static bool done;
#else
static semaphore_t ready_sem, start_sem, end_sem;
#endif // MALLOC_TARGET_EXCLAVES

static uint32_t nthreads;
static pthread_t threads[MAX_THREADS];
static _Atomic uint32_t active_thr;
static _Atomic int64_t todo;
uint64_t iterations_per_dt_stat_batch = ITERATIONS_PER_DT_STAT_BATCH;

#if COUNTERS
static _Atomic uint64_t total_mallocs;
#define ctr_inc(_t) atomic_fetch_add_explicit(&(_t), 1, memory_order_relaxed)
#else
#define ctr_inc(_t)
#endif

static uint32_t
ncpu(void)
{
	static uint32_t activecpu, physicalcpu;
	if (!activecpu) {
		uint32_t n;
		size_t s = sizeof(n);
		sysctlbyname("hw.activecpu", &n, &s, NULL, 0);
		activecpu = n;
		s = sizeof(n);
		sysctlbyname("hw.physicalcpu", &n, &s, NULL, 0);
		physicalcpu = n;
	}
	return MIN(activecpu, physicalcpu);
}

static void
wait_for_ready(void)
{
#if MALLOC_TARGET_EXCLAVES
	// Postcondition: start_mut will be locked
	int rc;
	for (;;) {
		rc = pthread_mutex_lock(&start_mut);
		T_QUIET; T_ASSERT_POSIX_ZERO(rc, "lock start mutex");
		if (num_waiting_start == nthreads) {
			num_waiting_start = 0;
			break;
		} else {
			rc = pthread_mutex_unlock(&start_mut);
			T_QUIET; T_ASSERT_POSIX_ZERO(rc, "unlock start mutex");
			yield();
		}
	}
#else
	kern_return_t kr;
	for (int i = 0; i < nthreads; i++) {
		kr = semaphore_wait(ready_sem);
		iferr (kr) {T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_wait");}
	}
#endif // MALLOC_TARGET_EXCLAVES
}

static void
start_threads(void)
{
#if MALLOC_TARGET_EXCLAVES
	// precondition: start_mut must be locked
	int rc = pthread_cond_broadcast(&start_cond);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "broadcast start");
	rc = pthread_mutex_unlock(&start_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "unlock start mutex");
#else
	kern_return_t kr = semaphore_signal_all(start_sem);
	iferr (kr) {T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_signal_all");}
#endif // MALLOC_TARGET_EXCLAVES
}

static void
wait_for_end(void)
{
#if MALLOC_TARGET_EXCLAVES
	int rc;
	rc = pthread_mutex_lock(&end_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "lock end mutex");
	num_waiting_end = 1;
	rc = pthread_cond_wait(&end_cond, &end_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "wait for end");
	rc = pthread_mutex_unlock(&end_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "unlock end mutex");
#else
	kern_return_t kr = semaphore_wait(end_sem);
	iferr (kr) {T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_wait");}
#endif // MALLOC_TARGET_EXCLAVES
}

__attribute__((noinline))
static void
#if MALLOC_TARGET_EXCLAVES
threaded_bench(int batch_size)
#else
threaded_bench(dt_stat_time_t s, int batch_size)
#endif // MALLOC_TARGET_EXCLAVES
{
	wait_for_ready();
	atomic_init(&active_thr, nthreads);
	atomic_init(&todo, batch_size * iterations_per_dt_stat_batch);
#if !MALLOC_TARGET_EXCLAVES
	dt_stat_token t = dt_stat_begin(s);
#endif // !MALLOC_TARGET_EXCLAVES
	start_threads();
	wait_for_end();
#if !MALLOC_TARGET_EXCLAVES
	dt_stat_end_batch(s, batch_size, t);
#endif // !MALLOC_TARGET_EXCLAVES
}

static void
init_semaphores(void)
{
#if MALLOC_TARGET_EXCLAVES
	int rc = pthread_cond_init(&start_cond, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "start cond init");
	rc = pthread_cond_init(&end_cond, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "end cond init");
	rc = pthread_mutex_init(&start_mut, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "start mutex init");
	rc = pthread_mutex_init(&end_mut, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(rc, "end mutex init");
	num_waiting_start = 0;
	num_waiting_end = 0;
	done = false;
#else
	int kr = semaphore_create(mach_task_self(), &ready_sem, SYNC_POLICY_FIFO, 0);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_create");
	kr = semaphore_create(mach_task_self(), &start_sem, SYNC_POLICY_FIFO, 0);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_create");
	kr = semaphore_create(mach_task_self(), &end_sem, SYNC_POLICY_FIFO, 0);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_create");
#endif // MALLOC_TARGET_EXCLAVES
}

static void
setup_threaded_bench(void* (*thread_fn)(void*), bool singlethreaded)
{
	kern_return_t kr;
	int r;
	char *e;

#if MALLOC_TARGET_EXCLAVES
	nthreads = singlethreaded ? 1 : ncpu();
#else
	if (singlethreaded) {
		nthreads = 1;
	} else {
		if ((e = getenv("DT_STAT_NTHREADS"))) nthreads = strtoul(e, NULL, 0);
		if (nthreads < 2) nthreads = ncpu();
	}
	if ((e = getenv("DT_STAT_CPU_BUSY"))) busy_select = strtoul(e, NULL, 0);
#endif // MALLOC_TARGET_EXCLAVES

	T_QUIET; T_ASSERT_LE(nthreads, MAX_THREADS, "Too many threads requested");

	init_semaphores();

#if MALLOC_TARGET_EXCLAVES
	pthread_attr_t *p_attr = NULL;
#else
	pthread_attr_t attr;
	pthread_attr_t *p_attr = &attr;
	r = pthread_attr_init(p_attr);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, "pthread_attr_init");
	r = pthread_attr_setdetachstate(p_attr, PTHREAD_CREATE_DETACHED);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, "pthread_attr_setdetachstate");
#endif // MALLOC_TARGET_EXCLAVES

	for (int i = 0; i < nthreads; i++) {
		r = pthread_create(&threads[i], p_attr, thread_fn,
				(void *)(uintptr_t)(i+1));
		T_QUIET; T_ASSERT_POSIX_ZERO(r, "pthread_create");
	}
}

#pragma mark -

static _Atomic(void*) allocations[LIVE_ALLOCATIONS];
static size_t max_rand, min_size, incr_size;

static void *
malloc_bench_thread(void * arg)
{
	kern_return_t kr;
	int r;
	seed_type_t seed;
	volatile double dummy;
	uint64_t pos, remaining_frees;
	void *alloc;

restart:
	seed = (uintptr_t)arg; // each thread repeats its own sequence
	// start threads off in different positions in allocations array
	pos = (seed - 1) * (LIVE_ALLOCATIONS / nthreads);
	remaining_frees = LIVE_ALLOCATIONS;

#if MALLOC_TARGET_EXCLAVES
	r = pthread_mutex_lock(&start_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, "lock start mutex");

	num_waiting_start++;

	r = pthread_cond_wait(&start_cond, &start_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, "Wait start condvar");

	r = pthread_mutex_unlock(&start_mut);
	T_ASSERT_POSIX_ZERO(r, "unlock start mutex");

	if (done) {
		return NULL;
	}
#else
	kr = semaphore_wait_signal(start_sem, ready_sem);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_wait_signal");
#endif // MALLOC_TARGET_EXCLAVES

	while (1) {
		uint64_t first, second;
		uint64_t random = random_busy_counts(&seed, &first, &second);
		if (atomic_fetch_sub_explicit(&todo, 1, memory_order_relaxed) > 0) {
			dummy = busy(first);
			alloc = malloc(min_size + (random % (max_rand + 1)) * incr_size);
			iferr (!alloc) { T_ASSERT_POSIX_ZERO(errno, "malloc"); }
			ctr_inc(total_mallocs);
		} else {
			if (!remaining_frees--) break;
			alloc = NULL;
		}
		alloc = atomic_exchange(&allocations[(pos++)%LIVE_ALLOCATIONS], alloc);
		if (alloc) {
			dummy = busy(second);
			free(alloc);
		}
	}

	if (atomic_fetch_sub_explicit(&active_thr, 1, memory_order_relaxed) == 1) {
#if MALLOC_TARGET_EXCLAVES
		for (;;) {
			r = pthread_mutex_lock(&end_mut);
			T_ASSERT_POSIX_ZERO(r, "Lock end mutex");
			if (num_waiting_end > 0) {
				num_waiting_end = 0;
				r = pthread_cond_signal(&end_cond);
				T_QUIET; T_ASSERT_POSIX_ZERO(r, "Signal end mutex");
				r = pthread_mutex_unlock(&end_mut);
				T_QUIET; T_ASSERT_POSIX_ZERO(r, "Unlock end mutex");

				break;
			} else {
				r = pthread_mutex_unlock(&end_mut);
				T_QUIET; T_ASSERT_POSIX_ZERO(r, "Unlock end mutex");
				yield();
			}
		}
#else
		kr = semaphore_signal(end_sem);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_signal");
#endif // MALLOC_TARGET_EXCLAVES
	}
	goto restart;
}

static void
malloc_bench(bool singlethreaded, size_t from, size_t to, size_t incr)
{
	int r;
	int batch_size;
#if COUNTERS
	uint64_t batch = 0;
#endif

	setup_threaded_bench(malloc_bench_thread, singlethreaded);

	incr_size = incr;
	min_size = from;
	max_rand = (to - from) / incr;
	assert((to - from) % incr == 0);

#if !MALLOC_TARGET_EXCLAVES
	dt_stat_time_t s = dt_stat_time_create(
			nthreads > 1 ? "%llu malloc & free multithreaded" :
					"%llu malloc & free singlethreaded",
			iterations_per_dt_stat_batch);
	dt_stat_set_variable((dt_stat_t)s, "threads", nthreads);

	// For now, set the A/B failure threshold to 50% of baseline.
	// 40292129 tracks removing noise and setting a more useful threshold.
	dt_stat_set_variable((dt_stat_t) s, kPCFailureThresholdPctVar, 50.0);
	do {
		batch_size = dt_stat_batch_size(s);
		threaded_bench(s, batch_size);
#if COUNTERS
		fprintf(stderr, "\rbatch: %4llu\t size: %4d\tmallocs: %8llu",
				++batch, batch_size,
				atomic_load_explicit(&total_mallocs, memory_order_relaxed));
#endif
	} while (!dt_stat_stable(s));
#if COUNTERS
	fprintf(stderr, "\n");
#endif
	dt_stat_finalize(s);
#else // !MALLOC_TARGET_EXCLAVES

	// TODO: Collect perf data and repeat test until runtime stabilizes. Or get
	// darwintest_perf into Exclaves/EVE
	for (int i = 0; i < 10; i++) {
		threaded_bench(10);
	}

	// Signal the threads to join because the process isn't torn down between
	// test cases
	wait_for_ready();
	done = true; // setting done will cause threads to return
	start_threads();

	for (int i = 0; i < nthreads; i++) {
		r = pthread_join(threads[i], NULL);
		T_ASSERT_POSIX_ZERO(r, "Join thread");
	}

	for (int i = 0; i < LIVE_ALLOCATIONS; i++) {
		free(allocations[i]);
		allocations[i] = NULL;
	}
#endif // !MALLOC_TARGET_EXCLAVES
}

T_DECL(perf_uncontended_nano_bench, "Uncontended nano malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false),
		T_META_ENVVAR("MallocNanoZone=1"))
{
	malloc_bench(true, 16, 256, 16); // NANO_MAX_SIZE
}

T_DECL(perf_contended_nano_bench, "Contended nano malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false),
		T_META_ENVVAR("MallocNanoZone=1"))
{
	malloc_bench(false, 16, 256, 16); // NANO_MAX_SIZE
}

T_DECL(perf_uncontended_tiny_bench, "Uncontended tiny malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false),
		T_META_ENVVAR("MallocNanoZone=0"))
{
	malloc_bench(true, 16, 1008, 16); // SMALL_THRESHOLD
}

T_DECL(perf_contended_tiny_bench, "Contended tiny malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false),
		T_META_ENVVAR("MallocNanoZone=0"))
{
	malloc_bench(false, 16, 1008, 16); // SMALL_THRESHOLD
}

T_DECL(perf_uncontended_small_bench, "Uncontended small malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false))
{
	malloc_bench(true, 1024, 15 * 1024, 512); // LARGE_THRESHOLD
}

T_DECL(perf_contended_small_bench, "Contended small malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false))
{
	malloc_bench(false, 1024, 15 * 1024, 512); // LARGE_THRESHOLD
}

T_DECL(perf_uncontended_large_bench, "Uncontended large malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false))
{
	iterations_per_dt_stat_batch = ITERATIONS_PER_DT_STAT_BATCH_LARGE_MALLOC;
	malloc_bench(true, 16 * 1024, 256 * 1024, 16 * 1024);
}

T_DECL(perf_contended_large_bench, "Contended large malloc",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false))
{
	iterations_per_dt_stat_batch = ITERATIONS_PER_DT_STAT_BATCH_LARGE_MALLOC;
	malloc_bench(false, 16 * 1024, 256 * 1024, 16 * 1024);
}

// rdar://100479142
#if CONFIG_DEFERRED_RECLAIM

// If deferred reclaim is available but not enabled by default, test it too
T_DECL(perf_contended_large_deferred_reclaim_bench,
		"Contended large malloc (deferred reclaim)",
		T_META_ALL_VALID_ARCHS(NO),
		T_META_LTEPHASE(LTE_POSTINIT), T_META_CHECK_LEAKS(false),
		T_META_ENABLED(!DEFAULT_LARGE_CACHE_ENABLED),
		T_META_ENVVAR("MallocLargeCache=1"))
{
	// Add more iterations to also serve as a stress test for deferred reclaim
	iterations_per_dt_stat_batch =
			ITERATIONS_PER_DT_STAT_BATCH_LARGE_MALLOC * 20;
	malloc_bench(false, 16 * 1024, 256 * 1024, 16 * 1024);
}

#endif // CONFIG_DEFERRED_RECLAIM
