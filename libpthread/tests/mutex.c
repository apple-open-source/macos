#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

struct context {
	pthread_mutex_t mutex;
	long value;
	long count;
};

void *test_thread(void *ptr) {
	int res;
	long old;
	struct context *context = ptr;

	int i = 0;
	char *str;

	do {
		bool try = i & 1;

		switch (i++ & 1) {
			case 0:
				str = "pthread_mutex_lock";
				res = pthread_mutex_lock(&context->mutex);
				break;
			case 1:
				str = "pthread_mutex_trylock";
				res = pthread_mutex_trylock(&context->mutex);
				break;
		}
		if (res != 0) {
			if (try && res == EBUSY) {
				continue;
			}
			fprintf(stderr, "[%ld] %s: %s\n", context->count, str, strerror(res));
			abort();
		}
		
		old = __sync_fetch_and_or(&context->value, 1);
		if ((old & 1) != 0) {
			fprintf(stderr, "[%ld] OR %lx\n", context->count, old);
			abort();
		}

		old = __sync_fetch_and_and(&context->value, 0);
		if ((old & 1) == 0) {
			fprintf(stderr, "[%ld] AND %lx\n", context->count, old);
			abort();
		}
	
		res = pthread_mutex_unlock(&context->mutex);
		if (res) {
			fprintf(stderr, "[%ld] pthread_mutex_lock: %s\n", context->count, strerror(res));
			abort();
		}
	} while (__sync_fetch_and_sub(&context->count, 1) > 0);
	exit(0);
}

int main(int argc, char *argv[])
{
	struct context context = {
		.mutex = PTHREAD_MUTEX_INITIALIZER,
		.value = 0,
		.count = 5000000,
	};
	int i;
	int res;
	int threads = 16;
	pthread_t p[threads];
	for (i = 0; i < threads; ++i) {
		res = pthread_create(&p[i], NULL, test_thread, &context);
		assert(res == 0);
	}
	for (i = 0; i < threads; ++i) {
		res = pthread_join(p[i], NULL);
		assert(res == 0);
	}
	
	return 0;
}
