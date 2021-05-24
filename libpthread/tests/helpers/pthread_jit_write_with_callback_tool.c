#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <pthread/jit_private.h>

static int
test_callback_should_not_actually_run(void *ctx)
{
	(void)ctx;
	printf("This callback was not expected to actually run\n");
	return 0;
}

int
main(int argc, char *argv[])
{
	assert(argc == 2);

	if (!strcmp(argv[1], "pthread_jit_write_protect_np")) {
		printf("Attempting pthread_jit_write_protect_np\n");
		pthread_jit_write_protect_np(false);
		printf("Should not have made it here\n");
	} else if (!strcmp(argv[1], "pthread_jit_write_with_callback_np")) {
		printf("Attempting pthread_jit_write_with_callback_np\n");
		(void)pthread_jit_write_with_callback_np(
				test_callback_should_not_actually_run, NULL);
		printf("Should not have made it here\n");
	}

	return 1;
}
