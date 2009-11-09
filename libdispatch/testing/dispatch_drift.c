#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "dispatch_test.h"

int
main(int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
	__block uint32_t count = 0;
	__block uint64_t first_time_m = 0ULL;
	__block double first_time_d;
	__block double last_jitter = 0;
	// 10 times a second
	uint64_t interval = 1000000000 / 10;
	double interval_d = interval / 1000000000.0;
	// for 25 seconds
	unsigned int target = 25 / interval_d;

	test_start("Timer drift test");

	dispatch_source_t t = dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL, interval, 0, NULL, dispatch_get_main_queue(),
		^(dispatch_event_t event __attribute__((unused))) {
		    struct timeval now_tv;
		    static double first = 0;
		    gettimeofday(&now_tv, NULL);
		    double now = now_tv.tv_sec + now_tv.tv_usec / 1000000.0;

		    if (count == 0) {
			    // Because this is taken at 1st timer fire,
			    // later jitter values may be negitave.
			    // This doesn't effect the drift calculation.
			    first = now;
		    }
		    double goal = first + interval_d * count;
		    double jitter = goal - now;
		    double drift = jitter - last_jitter;

		    printf("%4d: jitter %f, drift %f\n", count, jitter, drift);
#if 0
		    test_double_less_than("drift", now_d - expected_fire_time_d, .001);
#endif

		    if (target <= ++count) {
			    if (drift < 0) {
				    drift = -drift;
			    }
			    test_double_less_than("drift", drift, 0.0001);
			    test_stop();
		    }
		    last_jitter = jitter;
		});
	
	test_ptr_notnull("timer source", t);

	dispatch_main();
	return 0;
}

