#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <libkern/OSAtomic.h>

struct context {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	long waiters;
	long count;
};

void *wait_thread(void *ptr) {
	int res;
	struct context *context = ptr;

	int i = 0;
	char *str;

	bool loop = true;
	while (loop) {
		res = pthread_mutex_lock(&context->mutex);
		if (res) {
			fprintf(stderr, "[%ld] pthread_mutex_lock: %s\n", context->count, strerror(res));
			abort();
		}
		
		if (context->count > 0) {
			++context->waiters;
			res = pthread_cond_wait(&context->cond, &context->mutex);
			if (res) {
				fprintf(stderr, "[%ld] pthread_rwlock_unlock: %s\n", context->count, strerror(res));
				abort();
			}
			--context->waiters;
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
	struct context context = {
		.cond = PTHREAD_COND_INITIALIZER,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.waiters = 0,
		.count = 500000,
	};
	int i;
	int res;
	int threads = 2;
	pthread_t p[threads];
	for (i = 0; i < threads; ++i) {
		res = pthread_create(&p[i], NULL, wait_thread, &context);
		assert(res == 0);
	}

	long half = context.count / 2;

	bool loop = true;
	while (loop) {
		res = pthread_mutex_lock(&context.mutex);
		if (res) {
			fprintf(stderr, "[%ld] pthread_mutex_lock: %s\n", context.count, strerror(res));
			abort();
		}
		if (context.waiters) {
			char *str;
			if (context.count > half) {
				str = "pthread_cond_broadcast";
				res = pthread_cond_broadcast(&context.cond);
			} else {
				str = "pthread_cond_signal";
				res = pthread_cond_signal(&context.cond);
			}
			if (res != 0) {
				fprintf(stderr, "[%ld] %s: %s\n", context.count, str, strerror(res));
				abort();
			}
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

	return 0;
}
