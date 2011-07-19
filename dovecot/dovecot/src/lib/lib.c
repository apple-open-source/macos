/* Copyright (c) 2001-2011 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "env-util.h"
#include "hostpid.h"
#include "ipwd.h"
#include "process-title.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

size_t nearest_power(size_t num)
{
	size_t n = 1;

	i_assert(num <= ((size_t)1 << (BITS_IN_SIZE_T-1)));

	while (n < num) n <<= 1;
	return n;
}

void lib_init(void)
{
	/* standard way to get random() return different values.   APPLE */
	srandomdev();						/* APPLE */

	data_stack_init();
	hostpid_init();

	/* APPLE - initialize here, since it calls gmtime() which may
	   otherwise clobber the important contents of its static buffer */
	(void) time_t_max_bits();
}

void lib_deinit(void)
{
	ipwd_deinit();
	data_stack_deinit();
	env_deinit();
	failures_deinit();
	process_title_deinit();
}
