#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc/malloc.h>
#include <darwintest.h>

#include <../src/internal.h>

#if !MALLOC_TARGET_EXCLAVES
#include <sys/sysctl.h>
#include <mach/mach.h>
typedef unsigned seed_type_t;
#else
typedef unsigned long seed_type_t;
#endif // !MALLOC_TARGET_EXCLAVES

// These tests are based on perf_contended_malloc_free, but intended as
// functional stress tests rather than performance tests.

T_GLOBAL_META(T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_NOT_PREFERRED);

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

#if MALLOC_TARGET_EXCLAVES
static pthread_cond_t ready_cond;
static pthread_mutex_t ready_mut;
static uint32_t num_waiting_threads;
#else
static semaphore_t ready_sem, start_sem;
#endif // MALLOC_TARGET_EXCLAVES

static uint32_t nthreads;
static _Atomic uint32_t active_thr;
static _Atomic int64_t todo;

static uint32_t
ncpu(void)
{
#if MALLOC_TARGET_EXCLAVES
	// TODO: Switch to sysctl once liblibc reports multi-cpu. Currently EVE runs
	// tests on a single thread, but it's good to get some concurrenct tests in,
	// even if the threads don't run in parallel
	return 8;
#else
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
#endif // MALLOC_TARGET_EXCLAVES
}

static uint32_t live_allocations;
static void **allocations;
static size_t max_rand, min_size, incr_size;

static void
malloc_threaded_stress(bool singlethreaded, size_t from, size_t to, size_t incr,
		uint32_t live_allocations_count, uint64_t iterations,
		void *(*thread_fn)(void *))
{
	kern_return_t kr;
	int r;
	int batch_size;
	char *e;

#if MALLOC_TARGET_EXCLAVES
	nthreads = singlethreaded ? 1 : ncpu();
	busy_select = 0;
#else
	if (singlethreaded) {
		nthreads = 1;
	} else {
		if ((e = getenv("THREADED_STRESS_NTHREADS"))) {
			nthreads = strtoul(e, NULL, 0);
		}

		if (nthreads < 2) {
			nthreads = ncpu();
		}
	}
	if ((e = getenv("THREADED_STRESS_CPU_BUSY"))) {
		busy_select = strtoul(e, NULL, 0);
	}
#endif // MALLOC_TARGET_EXCLAVES

	atomic_init(&todo, iterations);
	atomic_init(&active_thr, nthreads);

	live_allocations = live_allocations_count;
	allocations = malloc(sizeof(allocations[0]) * live_allocations);
	T_QUIET; T_ASSERT_NOTNULL(allocations, "allocations array");
	incr_size = incr;
	min_size = from;
	max_rand = (to - from) / incr;
	assert((to - from) % incr == 0);

#if MALLOC_TARGET_EXCLAVES
	r = pthread_cond_init(&ready_cond, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, "condvar create");
	r = pthread_mutex_init(&ready_mut, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, "mutex create");
	num_waiting_threads = 0;
#else
	kr = semaphore_create(mach_task_self(), &ready_sem, SYNC_POLICY_FIFO, 0);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_create");
	kr = semaphore_create(mach_task_self(), &start_sem, SYNC_POLICY_FIFO, 0);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_create");
#endif // MALLOC_TARGET_EXCLAVES

	// Allocate thread array on heap to avoid llvm inserting stack check, which
	// doesn't compile
	pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
	for (int i = 0; i < nthreads; i++) {
		r = pthread_create(&threads[i], NULL, thread_fn,
				(void *)(uintptr_t)(i + 1));
		T_QUIET; T_ASSERT_POSIX_ZERO(r, "pthread_create");
	}

#if MALLOC_TARGET_EXCLAVES
	// Wait for all nthreads to signal that they're ready
	for (;;) {
		r = pthread_mutex_lock(&ready_mut);
		iferr (r) {T_QUIET; T_ASSERT_POSIX_ZERO(r, NULL);}
		T_ASSERT_POSIX_ZERO(r, "lock mutex");
		if (num_waiting_threads == nthreads) {
			r = pthread_cond_broadcast(&ready_cond);
			T_ASSERT_POSIX_ZERO(r, "ready condvar broadcast");
			r = pthread_mutex_unlock(&ready_mut);
			T_ASSERT_POSIX_ZERO(r, "ready mutex unlock");
			break;
		} else {
			r = pthread_mutex_unlock(&ready_mut);
			T_ASSERT_POSIX_ZERO(r, "ready mutex unlock");
			yield();
		}
	}
#else
	for (int i = 0; i < nthreads; i++) {
		kr = semaphore_wait(ready_sem);
		iferr (kr) {T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_wait");}
	}

	kr = semaphore_signal_all(start_sem);
	iferr (kr) {T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_signal_all");}
#endif // MALLOC_TARGET_EXCLAVES

	for (int i = 0; i < nthreads; i++) {
		r = pthread_join(threads[i], NULL);
		T_ASSERT_POSIX_ZERO(r, "pthread_join");
	}

	free(threads);
}

static void *
malloc_size_stress_thread(void *arg)
{
	kern_return_t kr;
	int r;
	seed_type_t seed;
	volatile double dummy;
	uint64_t pos, remaining_frees;
	void *alloc;

	seed = (uintptr_t)arg; // each thread repeats its own sequence
	// start threads off in different positions in allocations array
	pos = (seed - 1) * (live_allocations / nthreads);
	remaining_frees = live_allocations;
#if MALLOC_TARGET_EXCLAVES
	r = pthread_mutex_lock(&ready_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, NULL);
	num_waiting_threads++;
	r = pthread_cond_wait(&ready_cond, &ready_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, NULL);
	r = pthread_mutex_unlock(&ready_mut);
	T_QUIET; T_ASSERT_POSIX_ZERO(r, NULL);
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
		} else {
			if (!remaining_frees--) break;
			alloc = NULL;
		}

		// Size without taking ownership to allow another thread to race to free
		(void)malloc_size(allocations[pos % live_allocations]);

		alloc = atomic_exchange(
				(_Atomic(void *) *)&allocations[(pos++)%live_allocations],
				alloc);
		if (alloc) {
			// Size again while definitely allocated
			(void)malloc_size(alloc);

			dummy = busy(second);
			free(alloc);

			// Calling malloc_size on free pointers isn't safe in exclaves
#if !MALLOC_TARGET_EXCLAVES
			// Size again while probably free, but possibly re-allocated
			malloc_size(alloc);
#endif // !MALLOC_TARGET_EXCLAVES
		}
	}

	atomic_fetch_sub_explicit(&active_thr, 1, memory_order_relaxed);
	return NULL;
}

T_DECL(threaded_stress_malloc_size_tiny,
		"multi-threaded stress test for tiny malloc_size",
		T_META_ENVVAR("MallocNanoZone=0"))
{
	uint64_t iterations = 2000000ull;
#if TARGET_OS_TV || TARGET_OS_WATCH
	iterations = 200000ull;
#endif // TARGET_OS_TV || TARGET_OS_WATCH

	malloc_threaded_stress(false, 16, 256, 16, 2048,
			iterations, malloc_size_stress_thread);
}

T_DECL(threaded_stress_malloc_size_nano,
		"multi-threaded stress test for nano malloc_size",
		T_META_ENVVAR("MallocNanoZone=1"))
{
	uint64_t iterations = 2000000ull;
#if TARGET_OS_TV || TARGET_OS_WATCH
	iterations = 200000ull;
#endif // TARGET_OS_TV || TARGET_OS_WATCH

	malloc_threaded_stress(false, 16, 256, 16, 2048,
			iterations, malloc_size_stress_thread);
}

T_DECL(threaded_stress_malloc_size_small,
		"multi-threaded stress test for small malloc_size")
{
	uint64_t iterations = 200000ull;
#if TARGET_OS_TV || TARGET_OS_WATCH
	iterations = 20000ull;
#endif // TARGET_OS_TV || TARGET_OS_WATCH

	malloc_threaded_stress(false, 2048, 8192, 2048, 64,
			iterations, malloc_size_stress_thread);
}

#if !MALLOC_TARGET_EXCLAVES
// Exclaves don't support fork()
static void *
malloc_fork_stress_thread(void *arg)
{
	kern_return_t kr;
	int r;
	unsigned int seed;
	volatile double dummy;
	uint64_t pos, remaining_frees;
	void *alloc;
	bool parent = true;
	uint64_t children = 0;

	char *e;
	unsigned long fork_prob = 100000;
	if ((e = getenv("THREADED_STRESS_FORK_PROB"))) {
		unsigned long env_prob = strtoul(e, NULL, 0);
		if (env_prob) {
			fork_prob = env_prob;
		}
	}

	seed = (uintptr_t)arg; // each thread repeats its own sequence
	// start threads off in different positions in allocations array
	pos = (seed - 1) * (live_allocations / nthreads);
	remaining_frees = live_allocations;
	kr = semaphore_wait_signal(start_sem, ready_sem);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "semaphore_wait_signal");

	while (1) {
		uint64_t first, second;
		uint64_t random = random_busy_counts(&seed, &first, &second);
		if (parent && (random % fork_prob) == 0) {
			pid_t pid = fork();
			if (pid == -1) {
				if (errno != EAGAIN) {
					T_ASSERT_POSIX_SUCCESS(pid, "fork()");
				}
			} else if (pid == 0) {
				parent = false;
			} else {
				children++;
			}
		}

		if (atomic_fetch_sub_explicit(&todo, 1, memory_order_relaxed) > 0) {
			dummy = busy(first);
			alloc = malloc(min_size + (random % (max_rand + 1)) * incr_size);
			iferr (!alloc) { T_ASSERT_POSIX_ZERO(errno, "malloc"); }
			memset(alloc, 'a', 16);
		} else {
			if (!remaining_frees--) break;
			alloc = NULL;
		}
		alloc = atomic_exchange(
				(_Atomic(void *) *)&allocations[(pos++)%live_allocations],
				alloc);
		if (alloc) {
			dummy = busy(second);
			free(alloc);
		}
	}

	if (parent) {
		for (uint64_t i = 0; i < children; i++) {
			int status = 0;
			pid_t child = wait(&status);
			if (child == -1) {
				T_ASSERT_POSIX_SUCCESS(child, "wait()");
			}
			T_QUIET; T_ASSERT_TRUE(WIFEXITED(status), "child exited");
			T_QUIET; T_ASSERT_EQ(WEXITSTATUS(status), 0, "child succeeded");
		}
	}

	atomic_fetch_sub_explicit(&active_thr, 1, memory_order_relaxed);
	return NULL;
}

T_DECL(threaded_stress_fork, "multi-threaded stress test for fork",
		T_META_ENVVAR("MallocNanoZone=0")) // rdar://118860589
{
	uint64_t iterations = 2000000ull;
#if TARGET_OS_TV || TARGET_OS_WATCH
	iterations = 200000ull;
#endif // TARGET_OS_TV || TARGET_OS_WATCH

	malloc_threaded_stress(false, 16, 256, 16, 2048,
			iterations, malloc_fork_stress_thread);
}

T_DECL(threaded_stress_fork_small,
		"multi-threaded stress test of small for fork",
		T_META_ENVVAR("MallocNanoZone=0")) // rdar://118860589
{
	uint64_t iterations = 200000ull;
#if TARGET_OS_TV || TARGET_OS_WATCH
	iterations = 20000ull;
#endif // TARGET_OS_TV || TARGET_OS_WATCH

	malloc_threaded_stress(false, 2048, 8192, 2048, 64,
			iterations, malloc_fork_stress_thread);
}
#endif // MALLOC_TARGET_EXCLAVES
