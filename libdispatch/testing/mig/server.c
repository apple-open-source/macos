#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <dispatch/dispatch.h>

#include "hello_logger.h"
#include "hello_loggerServer.h"

static mach_port_t checkin_or_register(char *bname);

int main(void)
{
	mach_port_t mp = checkin_or_register(HELLO_LOGGER_BOOTSTRAP_NAME);
	dispatch_source_t ds = dispatch_source_mig_new(mp, do_hello_logger_subsystem.maxsize, hello_logger_server, NULL, NULL, NULL);

	assert(ds);

	dispatch_main();

	exit(EXIT_SUCCESS);
}

kern_return_t
do_example(mach_port_t test_port __attribute__((unused)), string_t somestring)
{
	fprintf(stdout, "%s\n", somestring);

	return KERN_SUCCESS;
}

mach_port_t
checkin_or_register(char *bname)
{
	kern_return_t kr;
	mach_port_t mp;

	/* If we're started by launchd or the old mach_init */
	kr = bootstrap_check_in(bootstrap_port, bname, &mp);
	if (kr == KERN_SUCCESS)
		return mp;

	abort();
}
