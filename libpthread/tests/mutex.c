#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "darwintest_defaults.h"

struct context {
	pthread_mutex_t mutex;
	long value;
	long count;
};

static void *test_thread(void *ptr) {
	int res;
	long old;
	struct context *context = ptr;

	int i = 0;
	char *str;

	do {
		bool try = i++ & 1;

		if (!try){
			str = "pthread_mutex_lock";
			res = pthread_mutex_lock(&context->mutex);
		} else {
			str = "pthread_mutex_trylock";
			res = pthread_mutex_trylock(&context->mutex);
		}
		if (res != 0) {
			if (try && res == EBUSY) {
				continue;
			}
			T_ASSERT_POSIX_ZERO(res, "[%ld] %s", context->count, str);
		}
		
		old = __sync_fetch_and_or(&context->value, 1);
		if ((old & 1) != 0) {
			T_FAIL("[%ld] OR %lx\n", context->count, old);
		}

		old = __sync_fetch_and_and(&context->value, 0);
		if ((old & 1) == 0) {
			T_FAIL("[%ld] AND %lx\n", context->count, old);
		}
	
		res = pthread_mutex_unlock(&context->mutex);
		if (res) {
			T_ASSERT_POSIX_ZERO(res, "[%ld] pthread_mutex_lock", context->count);
		}
	} while (__sync_fetch_and_sub(&context->count, 1) > 0);

	T_PASS("thread completed successfully");

	return NULL;
}

T_DECL(mutex, "pthread_mutex",
	T_META_ALL_VALID_ARCHS(YES))
{
	struct context context = {
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.value = 0,
		.count = 1000000,
	};
	int i;
	int res;
	int threads = 8;
	pthread_t p[threads];
	for (i = 0; i < threads; ++i) {
		res = pthread_create(&p[i], NULL, test_thread, &context);
		T_ASSERT_POSIX_ZERO(res, "pthread_create()");
	}
	for (i = 0; i < threads; ++i) {
		res = pthread_join(p[i], NULL);
		T_ASSERT_POSIX_ZERO(res, "pthread_join()");
	}
}
