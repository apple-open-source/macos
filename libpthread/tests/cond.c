#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <libkern/OSAtomic.h>

#include <darwintest.h>
#include <darwintest_utils.h>

struct context {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	long waiters;
	long count;
};

static void *wait_thread(void *ptr) {
	int res;
	struct context *context = ptr;

	bool loop = true;
	while (loop) {
		res = pthread_mutex_lock(&context->mutex);
		if (res) {
			T_ASSERT_POSIX_ZERO(res, "[%ld] pthread_mutex_lock", context->count);
		}
		
		if (context->count > 0) {
			++context->waiters;
			res = pthread_cond_wait(&context->cond, &context->mutex);
			if (res) {
				T_ASSERT_POSIX_ZERO(res, "[%ld] pthread_rwlock_unlock", context->count);
			}
			--context->waiters;
			--context->count;
		} else {
			loop = false;
		}
		
		res = pthread_mutex_unlock(&context->mutex);
		if (res) {
			T_ASSERT_POSIX_ZERO(res, "[%ld] pthread_mutex_unlock", context->count);
		}
	}

	return NULL;
}

T_DECL(cond, "pthread_cond",
		T_META_ALL_VALID_ARCHS(YES))
{
	struct context context = {
		.cond = PTHREAD_COND_INITIALIZER,
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.waiters = 0,
		.count = 100000 * dt_ncpu(),
	};
	int i;
	int res;
	int threads = 2;
	pthread_t p[threads];
	for (i = 0; i < threads; ++i) {
		T_ASSERT_POSIX_ZERO(pthread_create(&p[i], NULL, wait_thread, &context), NULL);
	}

	long half = context.count / 2;

	bool loop = true;
	while (loop) {
		res = pthread_mutex_lock(&context.mutex);
		if (res) {
			T_ASSERT_POSIX_ZERO(res, "[%ld] pthread_mutex_lock", context.count);
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
				T_ASSERT_POSIX_ZERO(res, "[%ld] %s", context.count, str);
			}
		}
		if (context.count <= 0) {
			loop = false;
			T_PASS("Completed stres test successfully.");
		}
		
		res = pthread_mutex_unlock(&context.mutex);
		if (res) {
			T_ASSERT_POSIX_ZERO(res, "[%ld] pthread_mutex_unlock", context.count);
		}
	}
	
	for (i = 0; i < threads; ++i) {
		T_ASSERT_POSIX_ZERO(pthread_join(p[i], NULL), NULL);
	}
}
