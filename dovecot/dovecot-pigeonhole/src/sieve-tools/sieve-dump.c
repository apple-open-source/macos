/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "array.h"
#include "master-service.h"
#include "master-service-settings.h"
#include "mail-storage-service.h"
#include "mail-user.h"

#include "sieve.h"
#include "sieve-extensions.h"
#include "sieve-tool.h"

#include "sieve-ext-debug.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sysexits.h>

/*
 * Print help
 */

static void print_help(void)
{
	printf(
"Usage: sieve-dump [-h] [-P <plugin>] [-x <extensions>]\n"
"                  <sieve-binary> [<out-file>]\n"
	);
}

/*
 * Tool implementation
 */

int main(int argc, char **argv) 
{
	struct sieve_instance *svinst;
	struct sieve_binary *sbin;
	const char *binfile, *outfile;
	bool hexdump = FALSE;
	int exit_status = EXIT_SUCCESS;
	int c;

	sieve_tool = sieve_tool_init("sieve-dump", &argc, &argv, "hP:x:", FALSE);
		
	binfile = outfile = NULL;

	while ((c = sieve_tool_getopt(sieve_tool)) > 0) {
		switch (c) {
		case 'h':
			/* produce hexdump */
			hexdump = TRUE;
			break;
		default:
			print_help();
			i_fatal_status(EX_USAGE, "Unknown argument: %c", c);
			break;
		}
	}

	if ( optind < argc ) {
		binfile = argv[optind++];
	} else { 
		print_help();
		i_fatal_status(EX_USAGE, "Missing <script-file> argument");
	}

	if ( optind < argc ) {
		outfile = argv[optind++];
	} 
	
	/* Finish tool initialization */
	svinst = sieve_tool_init_finish(sieve_tool, FALSE);

	/* Register debug extension */
	(void) sieve_extension_register(svinst, &debug_extension, TRUE);
		
	/* Dump binary */
	sbin = sieve_load(svinst, binfile, NULL);
	if ( sbin != NULL ) {
		sieve_tool_dump_binary_to(sbin, outfile == NULL ? "-" : outfile, hexdump);
	
		sieve_close(&sbin);
	} else {
		i_error("failed to load binary: %s", binfile);
		exit_status = EXIT_FAILURE;
	}

	sieve_tool_deinit(&sieve_tool);

	return exit_status;
}

