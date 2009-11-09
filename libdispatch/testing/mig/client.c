#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hello_logger.h"

int main(int argc, char *argv[])
{
	kern_return_t kr;
	mach_port_t mp;

	if (argc != 2) {
		fprintf(stderr, "I need a string to send!\n");
		exit(EXIT_FAILURE);
	}

	kr = bootstrap_look_up(bootstrap_port, HELLO_LOGGER_BOOTSTRAP_NAME, &mp);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "bootstrap_look_up(): %s\n", bootstrap_strerror(kr));
		exit(EXIT_FAILURE);
	}

	kr = example(mp, argv[1]);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "test_hello_logger(): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
