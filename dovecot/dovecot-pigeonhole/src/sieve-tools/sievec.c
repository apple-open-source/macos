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
#include "sieve-script.h"
#include "sieve-tool.h"

#include "sieve-ext-debug.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sysexits.h>

/*
 * Print help
 */

static void print_help(void)
{
	printf(
"Usage: sievec [-d] [-P <plugin>] [-x <extensions>] \n"
"              <script-file> [<out-file>]\n"
	);
}

/* 
 * Tool implementation
 */

int main(int argc, char **argv) 
{
	struct sieve_instance *svinst;
	struct stat st;
	struct sieve_binary *sbin;
	bool dump = FALSE;
	const char *scriptfile, *outfile;
	int exit_status = EXIT_SUCCESS;
	int c;
		
	sieve_tool = sieve_tool_init("sievec", &argc, &argv, "dP:x:u:", FALSE);
		
	scriptfile = outfile = NULL;
	while ((c = sieve_tool_getopt(sieve_tool)) > 0) {
		switch (c) {
		case 'd':
			/* dump file */
			dump = TRUE;
			break;
		default:
			print_help();
			i_fatal_status(EX_USAGE, "Unknown argument: %c", c);
			break;
		}
	}

	if ( optind < argc ) {
		scriptfile = argv[optind++];
	} else { 
		print_help();
		i_fatal_status(EX_USAGE, "Missing <script-file> argument");
	}

	if ( optind < argc ) {
		outfile = argv[optind++];
	} else if ( dump ) {
		outfile = "-";
	}

	svinst = sieve_tool_init_finish(sieve_tool, FALSE);

	/* Register debug extension */
	(void) sieve_extension_register(svinst, &debug_extension, TRUE);

	if ( stat(scriptfile, &st) == 0 && S_ISDIR(st.st_mode) ) {
		/* Script directory */
		DIR *dirp;
		struct dirent *dp;
		
		/* Sanity checks on some of the arguments */
		
		if ( dump )
			i_fatal_status(EX_USAGE, 
				"the -d option is not allowed when scriptfile is a directory."); 
		
		if ( outfile != NULL )
			i_fatal_status(EX_USAGE, 
				"the outfile argument is not allowed when scriptfile is a directory."); 
		
		/* Open the directory */
		if ( (dirp = opendir(scriptfile)) == NULL )
			i_fatal("opendir(%s) failed: %m", scriptfile);
			
		/* Compile each sieve file */
		for (;;) {
		
			errno = 0;
			if ( (dp = readdir(dirp)) == NULL ) {
				if ( errno != 0 ) 
					i_fatal("readdir(%s) failed: %m", scriptfile);
				break;
			}
											
			if ( sieve_script_file_has_extension(dp->d_name) ) {
				const char *file;
				
				if ( scriptfile[strlen(scriptfile)-1] == '/' )
					file = t_strconcat(scriptfile, dp->d_name, NULL);
				else
					file = t_strconcat(scriptfile, "/", dp->d_name, NULL);

				sbin = sieve_tool_script_compile(svinst, file, dp->d_name);

				if ( sbin != NULL ) {
					sieve_save(sbin, NULL, TRUE, NULL);		
					sieve_close(&sbin);
				}
			}
		}
   
		/* Close the directory */
		if ( closedir(dirp) < 0 ) 
			i_fatal("closedir(%s) failed: %m", scriptfile); 	
	} else {
		/* Script file (i.e. not a directory)
		 * 
		 *   NOTE: For consistency, stat errors are handled here as well 
		 */	
		sbin = sieve_tool_script_compile(svinst, scriptfile, NULL);

		if ( sbin != NULL ) {
			if ( dump ) 
				sieve_tool_dump_binary_to(sbin, outfile, FALSE);
			else {
				sieve_save(sbin, outfile, TRUE, NULL);
			}
		
			sieve_close(&sbin);
		} else {
			exit_status = EXIT_FAILURE;
		}
	}
		
	sieve_tool_deinit(&sieve_tool);

	return exit_status;
}
