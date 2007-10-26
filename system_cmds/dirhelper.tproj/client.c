#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "dirhelper.h"

#define DIRHELPER_BOOTSTRAP_NAME "com.apple.bsd.dirhelper"

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
	kern_return_t kr;
	mach_port_t mp;

	kr = bootstrap_look_up(bootstrap_port, DIRHELPER_BOOTSTRAP_NAME, &mp);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "bootstrap_look_up(): %s\n", bootstrap_strerror(kr));
		exit(EXIT_FAILURE);
	}

	kr = __dirhelper_create_user_local(mp);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "dirhelper_create_user_local(): %s\n", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
