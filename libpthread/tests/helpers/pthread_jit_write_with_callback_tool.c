#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <pthread.h>
#include <dlfcn.h>

static int
test_callback_should_not_actually_run(void *ctx)
{
	(void)ctx;
	printf("This callback was not expected to actually run\n");
	return 0;
}

#define HELPER_LIBRARY_PATH "/AppleInternal/Tests/libpthread/assets/libpthreadjittest.notdylib"

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		// The test is checking whether we exited with a signal to see if the
		// expected abort occurs, so if we need to bail out because of a
		// misconfiguration we should try to exit with an error code instead
		return 1;
	}

	if (!strcmp(argv[1], "pthread_jit_write_protect_np")) {
		printf("Attempting pthread_jit_write_protect_np\n");
		pthread_jit_write_protect_np(false);
		printf("Should not have made it here\n");
	} else if (!strcmp(argv[1], "pthread_jit_write_with_callback_np")) {
		printf("Attempting pthread_jit_write_with_callback_np\n");
		(void)pthread_jit_write_with_callback_np(
				test_callback_should_not_actually_run, NULL);
		printf("Should not have made it here\n");
	} else if (!strcmp(argv[1], "pthread_jit_write_freeze_callbacks_np")) {
		printf("Attempting freeze + dlopen + write_with_callback\n");

		pthread_jit_write_freeze_callbacks_np();

		void *handle = dlopen(HELPER_LIBRARY_PATH, RTLD_NOW);
		if (!handle) {
			printf("dlopen failed\n");
			return 1;
		}

		pthread_jit_write_callback_t cb = dlsym(handle,
				"test_dylib_jit_write_callback");
		if (!cb) {
			printf("dlsym failed\n");
			return 1;
		}

		(void)pthread_jit_write_with_callback_np(cb, NULL);
		printf("Should not have made it here\n");
	}

	return 1;
}
