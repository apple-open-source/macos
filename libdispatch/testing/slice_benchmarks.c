#include <libkern/OSAtomic.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <libproc.h>
#include <sys/proc_info.h>
#include <dispatch/dispatch.h>
// #include "../src/private.h"
#include <Block.h>

// "normal" loop size
#define LOOP 100000
#define SMALL_LOOP 1000

void report(const char *func, char *full_name, double x, unsigned long loops, char *unit) {
    // XXX: make cols pretty & stuff
    const char *prefix = "bench_";
    const int plen = strlen(prefix);
    assert(!strncmp(func, prefix, plen));
    func += plen;
    char *name;
    asprintf(&name, "[%s] %s", func, full_name);
    assert(name);

    x /= loops;

    if (!strcmp("mach", unit)) {
	static mach_timebase_info_data_t mtb;
	if (!mtb.denom) {
	    (void)mach_timebase_info(&mtb);
	}
	x = (x * mtb.numer) / mtb.denom;
	unit = "ns";
    }

    printf("%-64s %13f%-2s\n", name, x, unit);
    free(name);
}

void bench_queue_mem_use(void) {
    struct proc_taskinfo pti;
    uint64_t target_size;

    // The 1st call eats a little memory that isn't accounted for
    // until the 2nd call.   Also the _first_ printf eats >1M, so
    // if you insert some for debugging make sure it isn't the first!
    proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &pti, sizeof(pti));
    proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &pti, sizeof(pti));
    target_size = pti.pti_virtual_size + 1024*1024;
    int n;

    for(n = 0; target_size >= pti.pti_virtual_size; n++) {
	dispatch_queue_t leak = dispatch_queue_create("to be deleted", NULL);
	assert(leak);
	proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &pti, sizeof(pti));
	//printf("pti_virtual_size %qd; togo %qd, n %d\n", pti.pti_virtual_size, target_size - pti.pti_virtual_size, n);
    }

    report(__FUNCTION__, "#queues to grow VSIZE 1Mbyte", n-1, 1, "x");
}

void bench_message_round_trip(void) {
    dispatch_queue_t q1 = dispatch_queue_create("q1", NULL);
    dispatch_queue_t q2 = dispatch_queue_create("q2", NULL);
    uint64_t start = mach_absolute_time();

    int i;
    for(i = 0; i < LOOP; i++) {
	// make sure we don't build up too much of a backlog
	if (i && !(i & 0x3ff)) {
	    dispatch_sync(q2, ^{});
	}
	dispatch_queue_retain(q2);
	dispatch_async(q1, ^{
	    dispatch_async(q2, ^{
		dispatch_queue_release(q2);
	    });
	});
    }

    // Make sure eveything has drained before we take the end timestamp
    dispatch_sync(q1, ^{});
    dispatch_sync(q2, ^{});

    uint64_t end = mach_absolute_time();
    report(__FUNCTION__, "round trip (async async - implicit copy)", (end - start), LOOP, "mach");
    dispatch_queue_release(q1);
    dispatch_queue_release(q2);
}

void bench_precopy_message_round_trip(void) {
    dispatch_queue_t q1 = dispatch_queue_create("q1", NULL);
    dispatch_queue_t q2 = dispatch_queue_create("q2", NULL);
    assert(q1 && q2);

    unsigned long rc;

    dispatch_block_t b2 = Block_copy(^{
    });
    dispatch_block_t b1 = Block_copy(^{
	unsigned long rc = dispatch_async(q2, b2);
	assert(!rc);
	dispatch_queue_release(q2);
    });
    dispatch_block_t be = Block_copy(^{});
    assert(b1 && b2);
    uint64_t start = mach_absolute_time();

    int i;
    for(i = 0; i < LOOP; i++) {
	// make sure we don't build up too much of a backlog
	if (i && !(i & 0x3ff)) {
	    dispatch_sync(q2, be);
	}
	dispatch_queue_retain(q2);
	rc = dispatch_async(q1, b1);
	assert(!rc);
    }

    // Make sure eveything has drained before we take the end timestamp
    dispatch_sync(q1, be);
    dispatch_sync(q2, be);

    uint64_t end = mach_absolute_time();
    report(__FUNCTION__, "round trip (a/a - precopy)", (end - start), LOOP, "mach");
    dispatch_queue_release(q1);
    dispatch_queue_release(q2);
}

void bench_message_round_type_syncasync(void) {
    dispatch_queue_t q1 = dispatch_queue_create("q1", NULL);
    dispatch_queue_t q2 = dispatch_queue_create("q2", NULL);
    uint64_t start = mach_absolute_time();

    int i;
    for(i = 0; i < LOOP; i++) {
	dispatch_queue_retain(q2);
	dispatch_sync(q1, ^{
	    dispatch_async(q2, ^{
		dispatch_queue_release(q2);
	    });
	});
    }

    // Make sure eveything has drained before we take the end timestamp
    dispatch_sync(q1, ^{});
    dispatch_sync(q2, ^{});

    uint64_t end = mach_absolute_time();
    report(__FUNCTION__, "round trip (s/a - implicit copy)", (end - start), LOOP, "mach");
    dispatch_queue_release(q1);
    dispatch_queue_release(q2);
}

void nothing_f(void *ignored) {
}

void brt_f_q1(void *vq2) {
    unsigned long rc = dispatch_async_f((dispatch_queue_t)vq2, NULL, nothing_f);
    assert(!rc);
}

void bench_message_round_trip_f(void) {
    dispatch_queue_t q1 = dispatch_queue_create("q1", NULL);
    dispatch_queue_t q2 = dispatch_queue_create("q2", NULL);
    uint64_t start = mach_absolute_time();
    unsigned long rc;

    int i;
    for(i = 0; i < LOOP; i++) {
	// make sure we don't build up too much of a backlog
	if (i && !(i & 0x3ff)) {
	    dispatch_sync_f(q2, NULL, nothing_f);
	}
	rc = dispatch_async_f(q1, q2, brt_f_q1);
	assert(!rc);
    }

    // Make sure eveything has drained before we take the end timestamp
    dispatch_sync_f(q1, NULL, nothing_f);
    dispatch_sync_f(q2, NULL, nothing_f);

    uint64_t end = mach_absolute_time();
    report(__FUNCTION__, "round trip (a/a - no blocks)", (end - start), LOOP, "mach");
    dispatch_queue_release(q1);
    dispatch_queue_release(q2);
}

void bench_message_round_type_syncasync_f(void) {
}

struct baton {
    // should extend to keep data on times for latency calc
    int passes_left;
    int at_q;
    int baton_number;

    // Avoid false ache line shares.   Big speed difference on a Mac Pro
    char pad[128 - sizeof(int)*3];
};

pthread_mutex_t kludge;
static int n_baton_kludge;

void pass(dispatch_queue_t *q, struct baton *bat, const int n_queues, dispatch_queue_t complete_q) {
    //fprintf(stderr, "bat#%d q#%d, passes left: %d\n", bat->baton_number, bat->at_q, bat->baton_number);
    if (0 == --(bat->passes_left)) {
	dispatch_queue_resume(complete_q);
	// XXX: atomic
	if (!__sync_sub_and_fetch(&n_baton_kludge, 1)) {
		pthread_mutex_unlock(&kludge);
	}
	return;
    }
    bat->at_q = (bat->at_q + 1) % n_queues;
    unsigned long rc = dispatch_async(q[bat->at_q], ^{ pass(q, bat, n_queues, complete_q); });
    assert(rc == 0);
}

void bench_baton() {
    const int n_queues = 128;
    const int q_div_b = 4;
    const int n_batons = n_queues / q_div_b;
    assert(q_div_b * n_batons == n_queues);
    n_baton_kludge = n_batons;
    dispatch_queue_t *q;
    dispatch_queue_t complete_q = dispatch_queue_create("completion q", NULL);;
    char *q_labels[n_queues];
    int i;
    unsigned long rc;

    // creting a queue ("C"), suspending it, blocking in a dispatch_sync, and 
    // having another queue resume C does not appear to ever unblock the 
    // dispatch_sync.   XXX: make test case and file radar.   (if it still
    // works that way on recent builds, with dispatch inside libsystem, and
    // such)


    pthread_mutex_init(&kludge, NULL);
    rc = pthread_mutex_trylock(&kludge);
    assert(!rc);
    q = alloca(n_queues * sizeof(dispatch_queue_t));

    for(i = 0; i < n_queues; i++) {
	asprintf(q_labels + i, "relay#%d (%s)", i, __FUNCTION__);
	assert(q_labels[i]);
	q[i] = dispatch_queue_create(q_labels[i], NULL);
	assert(q[i]);
    }

    uint64_t start_time = mach_absolute_time();

    for(i = 0; i < n_queues; i += q_div_b) {
	struct baton *bat = valloc(sizeof(struct baton));
	assert(bat);
	bat->passes_left = SMALL_LOOP;
	bat->at_q = i;
	bat->baton_number = i / q_div_b;
	dispatch_queue_suspend(complete_q);
	rc = dispatch_async(q[i], ^{
	    pass(q, bat, n_queues, complete_q);
	});
	assert(rc == 0);
    }

    // XXX: dispatch_sync(complete_q, ^{});
    rc = pthread_mutex_lock(&kludge);
    assert(!rc);
    uint64_t end_time = mach_absolute_time();
    report(__FUNCTION__, "baton pass", (end_time - start_time), SMALL_LOOP*n_batons, "mach");
    // dispatch_queue_release(q);
}

void bench_overload2() {
	const int n_queues = 128;
	const int q_div_b = 1;
	const int n_batons = n_queues / q_div_b;
	n_baton_kludge = n_batons;
	assert(q_div_b * n_batons == n_queues);
	dispatch_queue_t *q = alloca(n_queues * sizeof(dispatch_queue_t));
	dispatch_source_t *ds = alloca(n_queues * sizeof(dispatch_source_t));
	dispatch_queue_t complete_q = dispatch_queue_create("completion q", NULL);
	__block uint64_t start_time = 0;
	uint64_t time_to_start;
	uint64_t end_time;
	char *q_labels[n_queues];
	int i;
	unsigned int rc;

	rc = pthread_mutex_unlock(&kludge);
	assert(!rc);
	rc = pthread_mutex_trylock(&kludge);
	assert(!rc);

	// Start all batons one to two seconds from now.
	time_to_start = (2 + time(NULL)) * 1000000000;

	for(i = 0; i < n_queues; i++) {
		asprintf(q_labels + i, "queue#%d (%s)", i, __FUNCTION__);
		assert(q_labels[i]);
		q[i] = dispatch_queue_create(q_labels[i], NULL);
		assert(q[i]);
		struct baton *bat = valloc(sizeof(struct baton));
		assert(bat);
		bat->passes_left = SMALL_LOOP;
		bat->at_q = i;
		bat->baton_number = i / q_div_b;
		dispatch_queue_suspend(complete_q);
		ds[i] = dispatch_source_timer_create(DISPATCH_TIMER_ABSOLUTE, time_to_start, 0, NULL, q[i], ^(dispatch_event_t event){
			assert(!dispatch_event_get_error(event, NULL));
			// We want to measure the time from the first
			// baton pass, and NOT include hte wait time
			// for eveyone to start to fire
			if (!start_time) {
				uint64_t s = mach_absolute_time();
				__sync_bool_compare_and_swap(&start_time, 0, s);
			}
			pass(q, bat, n_queues, complete_q);
		});
		assert(ds[i]);
	}

	// XXX: dispatch_sync(complete_q, ^{});
	rc = pthread_mutex_lock(&kludge);
	assert(!rc);

	end_time = mach_absolute_time();
	report(__FUNCTION__, "overload#2", (end_time - start_time), SMALL_LOOP*n_batons, "mach");
	// Many releases and free()s

}

void bench_overload1() {
	const int n_queues = 128;
	const int q_div_b = 1;
	const int n_batons = n_queues / q_div_b;
	n_baton_kludge = n_batons;
	assert(q_div_b * n_batons == n_queues);
	dispatch_queue_t *q = alloca(n_queues * sizeof(dispatch_queue_t));
	dispatch_queue_t complete_q = dispatch_queue_create("completion q", NULL);
	__block uint64_t start_time = 0;
	struct timeval time_to_start;
	uint64_t end_time;
	char *q_labels[n_queues];
	int i;
	unsigned int rc;

	rc = pthread_mutex_unlock(&kludge);
	assert(!rc);
	rc = pthread_mutex_trylock(&kludge);
	assert(!rc);

	// Start all batons one to two seconds from now.
	gettimeofday(&time_to_start, NULL);
	time_to_start.tv_sec += 2;

	for(i = 0; i < n_queues; i++) {
		asprintf(q_labels + i, "queue#%d (%s)", i, __FUNCTION__);
		assert(q_labels[i]);
		q[i] = dispatch_queue_create(q_labels[i], NULL);
		assert(q[i]);
		struct baton *bat = valloc(sizeof(struct baton));
		assert(bat);
		bat->passes_left = SMALL_LOOP;
		bat->at_q = i;
		bat->baton_number = i / q_div_b;
		dispatch_queue_suspend(complete_q);
		dispatch_async(q[i], ^(void) {
			struct timeval now;
			gettimeofday(&now, NULL);
			int sec = time_to_start.tv_sec - now.tv_sec;
			if (sec >= 0) {
				int usec = time_to_start.tv_usec + now.tv_usec;
				if (usec > 0 || sec > 0) {
					usleep(1000000 * sec + usec);
				} else {
					// XXX: log here
				}
			} 

			// We want to measure the time from the first
			// baton pass, and NOT include hte wait time
			// for eveyone to start to fire
			if (!start_time) {
				uint64_t s = mach_absolute_time();
				__sync_bool_compare_and_swap(&start_time, 0, s);
			}

			pass(q, bat, n_queues, complete_q);
		});
	}

	// XXX: dispatch_sync(complete_q, ^{});
	rc = pthread_mutex_lock(&kludge);
	assert(!rc);

	end_time = mach_absolute_time();
	report(__FUNCTION__, "overload#1", (end_time - start_time), SMALL_LOOP*n_batons, "mach");
	// Many releases and free()s

}

int main(int argc, char *argv[]) {
    // Someday we will be able to take a list of tests to run, or exclude, or something.

    // There are somewhat diffrent perfomance chararistics when using the
    // main queue, so we use a "normal" queue for all our tests.
    dispatch_queue_t bench_q = dispatch_queue_create("benhmark Q", NULL);
    
    dispatch_async(bench_q, ^{
	// These two aren't as intresting in duel core, they queue all
	// the calls before making them which isn't really what we
	// want to test, is it?     It also limites the number of loops
	// we can spin around.
#if 1
	bench_message_round_trip();
	bench_precopy_message_round_trip();

	bench_message_round_type_syncasync();
	bench_message_round_trip_f();
	bench_message_round_type_syncasync_f();
#endif
	bench_baton();
	bench_overload1();
	bench_overload2();

	// This leaks, so we run it last.  Also it gives
	// wrong results if stdio hasn't been started already,
	// so we definitly don't want to run it first even if
	// the leaks are fixed (or ignored)
	bench_queue_mem_use();

	exit(0);
    });

    dispatch_main();
}
