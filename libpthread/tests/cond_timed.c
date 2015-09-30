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

struct context {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	long udelay;
	long count;
};

void *wait_thread(void *ptr) {
	int res;
	struct context *context = ptr;

	int i = 0;
	char *str;

	bool loop = true;
	while (loop) {
		struct timespec ts;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		uint64_t ns = tv.tv_usec * NSEC_PER_USEC + context->udelay * NSEC_PER_USEC;
		ts.tv_nsec = ns >= NSEC_PER_SEC ? ns % NSEC_PER_SEC : ns;
		ts.tv_sec = tv.tv_sec + (ns >= NSEC_PER_SEC ? ns / NSEC_PER_SEC : 0);

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

int main(int argc, char *argv[])
{
	const int threads = 8;
	struct context context = {
		.cond = PTHREAD_COND_INITIALIZER,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.udelay = 5000,
		.count = 8000,
	};
	int i;
	int res;

	uint64_t uexpected = (context.udelay * context.count) / threads;
	printf("waittime expected: %llu us\n", uexpected); 
	struct timeval start, end;
	gettimeofday(&start, NULL);

	pthread_t p[threads];
	for (i = 0; i < threads; ++i) {
		res = pthread_create(&p[i], NULL, wait_thread, &context);
		assert(res == 0);
	}

	usleep(uexpected);
	bool loop = true;
	while (loop) {
		res = pthread_mutex_lock(&context.mutex);
		if (res) {
			fprintf(stderr, "[%ld] pthread_mutex_lock: %s\n", context.count, strerror(res));
			abort();
		}
		if (context.count <= 0) {
			loop = false;
		}
		res = pthread_mutex_unlock(&context.mutex);
		if (res) {
			fprintf(stderr, "[%ld] pthread_mutex_unlock: %s\n", context.count, strerror(res));
			abort();
		}
	}

	for (i = 0; i < threads; ++i) {
		res = pthread_join(p[i], NULL);
		assert(res == 0);
	}

	gettimeofday(&end, NULL);
	uint64_t uelapsed = (end.tv_sec * USEC_PER_SEC + end.tv_usec) -
			(start.tv_sec * USEC_PER_SEC + start.tv_usec);
	printf("waittime actual:   %llu us\n", uelapsed);

	return 0;
}
