/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "mempool.h"
#include "imem.h"
#include "array.h"
#include "strfuncs.h"
#include "unlink-directory.h"

#include "sieve.h"
#include "sieve-common.h" 
#include "sieve-error.h"
 
#include "testsuite-common.h"
#include "testsuite-binary.h"

#include <sys/stat.h>
#include <sys/types.h>

/*
 * State
 */

static char *testsuite_binary_tmp = NULL;

/*
 * Initialization
 */

void testsuite_binary_init(void)
{	
	testsuite_binary_tmp = i_strconcat
		(testsuite_tmp_dir_get(), "/binaries", NULL);

	if ( mkdir(testsuite_binary_tmp, 0700) < 0 ) {
		i_fatal("failed to create temporary directory '%s': %m.", 
			testsuite_binary_tmp);		
	}
}

void testsuite_binary_deinit(void)
{
	if ( unlink_directory(testsuite_binary_tmp, TRUE) < 0 ) {
		i_warning("failed to remove temporary directory '%s': %m.",
			testsuite_binary_tmp);
	}
	
	i_free(testsuite_binary_tmp);		
}

void testsuite_binary_reset(void)
{
	testsuite_binary_init();
	testsuite_binary_deinit();
}

/*
 * Binary Access
 */

bool testsuite_binary_save(struct sieve_binary *sbin, const char *name)
{
	return sieve_save(sbin, 
		t_strdup_printf("%s/%s.svbin", testsuite_binary_tmp, name), TRUE, NULL);
}

struct sieve_binary *testsuite_binary_load(const char *name)
{
	struct sieve_instance *svinst = testsuite_sieve_instance;

	return sieve_load(svinst, 
		t_strdup_printf("%s/%s.svbin", testsuite_binary_tmp, name), NULL);
}



