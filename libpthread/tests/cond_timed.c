#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include <libkern/OSAtomic.h>
#include <dispatch/dispatch.h>

#include <darwintest.h>

#define NUM_THREADS 8

struct context {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	long udelay;
	long count;
};

static void *wait_thread(void *ptr) {
	int res;
	struct context *context = ptr;

	bool loop = true;
	while (loop) {
		struct timespec ts;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		tv.tv_sec += (tv.tv_usec + context->udelay) / (__typeof(tv.tv_sec)) USEC_PER_SEC;
		tv.tv_usec = (tv.tv_usec + context->udelay) % (__typeof(tv.tv_usec)) USEC_PER_SEC;
		TIMEVAL_TO_TIMESPEC(&tv, &ts);

		res = pthread_mutex_lock(&context->mutex);
		if (res) {
			fprintf(stderr, "[%ld] pthread_mutex_lock: %s\n", context->count, strerror(res));
			abort();
		}

		if (context->count > 0) {
			res = pthread_cond_timedwait(&context->cond, &context->mutex, &ts);
			if (res != ETIMEDOUT) {
				fprintf(stderr, "[%ld] pthread_cond_timedwait: %s\n", context->count, strerror(res));
				abort();
			}
			--context->count;
		} else {
			loop = false;
		}

		res = pthread_mutex_unlock(&context->mutex);
		if (res) {
			fprintf(stderr, "[%ld] pthread_mutex_unlock: %s\n", context->count, strerror(res));
			abort();
		}
	}

	return NULL;
}

T_DECL(cond_timedwait_timeout, "pthread_cond_timedwait() timeout")
{
	// This testcase launches 8 threads that all perform timed wait on the same
	// conditional variable that is not being signaled in a loop. Ater the total
	// of 8000 timeouts all threads finish and the testcase prints out the
	// expected time (5[ms]*8000[timeouts]/8[threads]=5s) vs elapsed time.
	struct context context = {
		.cond = PTHREAD_COND_INITIALIZER,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.udelay = 5000,
		.count = 8000,
	};

	long uexpected = (context.udelay * context.count) / NUM_THREADS;
	T_LOG("waittime expected: %ld us", uexpected);
	struct timeval start, end;
	gettimeofday(&start, NULL);

	pthread_t p[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; ++i) {
		T_ASSERT_POSIX_ZERO(pthread_create(&p[i], NULL, wait_thread, &context),
							"pthread_create");
	}

	usleep((useconds_t) uexpected);
	bool loop = true;
	while (loop) {
		T_ASSERT_POSIX_ZERO(pthread_mutex_lock(&context.mutex),
							"pthread_mutex_lock");
		if (context.count <= 0) {
			loop = false;
		}
		T_ASSERT_POSIX_ZERO(pthread_mutex_unlock(&context.mutex),
							"pthread_mutex_unlock");
	}

	for (int i = 0; i < NUM_THREADS; ++i) {
		T_ASSERT_POSIX_ZERO(pthread_join(p[i], NULL), "pthread_join");
	}

	gettimeofday(&end, NULL);
	uint64_t uelapsed =
			((uint64_t) end.tv_sec * USEC_PER_SEC + (uint64_t) end.tv_usec) -
			((uint64_t) start.tv_sec * USEC_PER_SEC + (uint64_t) start.tv_usec);
	T_LOG("waittime actual:   %llu us", uelapsed);
}
