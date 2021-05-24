#include <pthread/pthread.h>
#include <pthread/jit_private.h>

int
test_dylib_jit_write_callback(void *ctx);

int
test_dylib_jit_write_callback(void *ctx)
{
	pthread_jit_write_callback_t cb = ctx;
	return cb(NULL);
}

PTHREAD_JIT_WRITE_ALLOW_CALLBACKS_NP(test_dylib_jit_write_callback);
