/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  security.c
 *  security
 *
 *  Created by Michael Brouwer on Tue May 06 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "security.h"

#include "leaks.h"
#include "readline.h"

#include "db_commands.h"
#include "keychain_add.h"
#include "keychain_create.h"
#include "keychain_delete.h"
#include "keychain_list.h"
#include "keychain_lock.h"
#include "keychain_set_settings.h"
#include "keychain_show_info.h"
#include "keychain_unlock.h"
#include "key_create.h"
#include "keychain_find.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Maximum length of an input line in interactive mode. */
#define MAX_LINE_LEN 4096
/* Maximum number of arguments on an input line in interactive mode. */
#define MAX_ARGS 32

/* Entry in commands array for a command. */
typedef struct command
{
	const char *c_name;    /* name of the command. */
	command_func c_func;   /* function to execute the command. */
	const char *c_usage;   /* usage sting for command. */
	const char *c_help;    /* help string for (or description of) command. */
} command;

/* The default prompt. */
const char *prompt_string = "security> ";

/* The name of this program. */
const char *prog_name;


/* Forward declarations of static functions. */
static int help(int argc, char * const *argv);

/*
 * The command array itself.
 * Add commands here at will.
 * Matching is done on a prefix basis.  The first command in the array
 * gets matched first.
 */
const command commands[] =
{
	{ "help", help,
	  "[command ...]",
	  "Show all commands. Or show usage for a command." },
    
	{ "list-keychains", keychain_list,
	  "[-d user|system|common|alternate] [-s [keychain...]]\n"
	  "    -d    Use the specified domain.\n"
	  "    -s    Set the searchlist to the specified keychains.\n"
	  "With no parameters display the searchlist.",
	  "Display or manipulate the keychain search list." },

	{ "default-keychain", keychain_default,
	  "[-d user|system|common|alternate] [-s [keychain]]\n"
	  "    -d    Use the specified domain.\n"
	  "    -s    Set the default keychain to the specified keychain.\n"
	  "With no parameters display the default keychain.",
	  "Display or set the default keychain." },

	{ "login-keychain", keychain_login,
	  "[-d user|system|common|alternate] [-s [keychain]]\n"
	  "    -d    Use the specified domain.\n"
	  "    -s    Set the login keychain to the specified keychain.\n"
	  "With no parameters display the login keychain.",
	  "Display or set the login keychain." },

	{ "create-keychain", keychain_create,
	  "[-P] [-p password] [keychains...]\n"
	  "    -p    Use \"password\" as the password for the keychains being created.\n"
	  "    -P    Prompt the user for a password using the SecurityAgent.",
	  "Create keychains and add them to the search list." },

	{ "delete-keychain", keychain_delete,
	  "[keychains...]",
	  "Delete keychains and remove them from the search list." },

    { "lock-keychain", keychain_lock,
      "[-a | keychain]\n"
      "     -a Lock all keychains.",
      "Lock the specified keychain."},

    { "unlock-keychain", keychain_unlock,
      "[-u] [-p password] [keychain]\n"
      "     -p Use \"password\" as the password to unlock the keychain.\n"
      "     -u Do not use the password.",
      "Unlock the specified keychain."},

    { "set-keychain-settings", keychain_set_settings,
      "[-lu] [-t locktimeout] [keychain]\n"
      "     -l  Lock keychain when the system sleeps.\n"
      "     -u  Lock keychain after certain period of time.\n"
      "     -t  Timeout in seconds before the keychain locks.\n",
      "Set settings for a keychain."},

    { "show-keychain-info", keychain_show_info,
	  "[keychain]",
	  "Show the settings for keychain." },

    { "dump-keychain", keychain_dump,
	  "[-dr] [keychain...]\n"
      "     -d  Dump data of items.\n"
      "     -r  Dump the raw (encrypted) data of items.",
	  "Dump the contents of one or more keychains." },

    { "create-keypair", key_create_pair,
	  "[-a alg] [-s size] [-f date] [-t date] [-v days] [-k keychain] [-n name] [-A|-T app1:app2:...]\n"
	  "    -a  Use alg as the algorithm, can be rsa, dh, dsa or fee (default rsa)\n"
	  "    -s  Specify the keysize in bits (default 512)\n"
	  "    -f  Make a key valid from the specified date\n"
	  "    -t  Make a key valid to the specified date\n"
	  "    -v  Make a key valid for the number of days specified from today\n"
	  "    -k  Use the specified keychain rather than the default\n"
	  "    -A  Allow any application to access without warning.\n"
	  "    -T  Allow the applications specified to access without warning.\n"
	  "If no options are provided ask the user interactively",
	  "Create an assymetric keypair." },

    { "add-internet-password", keychain_add_internet_password,
	  "[-a accountName] [-d securityDomain] [-p path] [-P port] [-r protocol] [-s serverName] [-t authenticationType] [-w passwordData] [keychain]\n"
	  "    -a Use \"accountName\".\n"
	  "    -d Use \"securityDomain\".\n"
	  "    -p Use \"path\".\n"
	  "    -P Use \"port\".\n"
	  "    -r Use \"protocol\".\n"
	  "    -s Use \"serverName\".\n"
	  "    -t Use \"authenticationType\".\n"
      "    -w Use passwordData.\n"
	  "If no keychains is specified the password is added to the default keychain.",
      "Add an internet password item."},

	{ "add-generic-password", keychain_add_generic_password,
	  "[-a accountName] [-s serviceName] [-p passwordData] [keychain]\n"
	  "    -a Use \"accountName\".\n"
	  "    -s Use \"serverName\".\n"  
      "    -p Use passwordData.\n"
	  "If no keychains is specified the password is added to the default keychain.",
      "Add a generic password item."},
      
	{ "add-certificates", keychain_add_certificates,
	  "[-k keychain] file...\n"
	  "If no keychains is specified the certificates are added to the default keychain.",
      "Add certificates to a keychain."},

	{ "find-internet-password", keychain_find_internet_password,
	  "[-a accountName] [-d securityDomain] [-g] [-p path] [-P port] [-r protocol] [-s serverName] [-t authenticationType] [keychain...]\n"
	  "    -a Match on \"accountName\" when searching.\n"
	  "    -d Match on \"securityDomain\" when searching.\n"
	  "    -g Display the password for the item found.\n"
	  "    -p Match on \"path\" when searching.\n"
	  "    -P Match on \"port\" when searching.\n"
	  "    -r Match on \"protocol\" when searching.\n"
	  "    -s Match on \"serverName\" when searching.\n"
	  "    -t Match on \"authenticationType\" when searching.\n"
	  "If no keychains are specified the default search list is used.",
      "Find an internet password item."},

	{ "find-generic-password", keychain_find_generic_password,
	  "[-a accountName] [-s serviceName] [keychain...]\n"
	  "    -a Match on \"accountName\" when searching.\n"
      "    -g Display the password for the item found.\n"
	  "    -s Match on \"serviceName\" when searching.\n"
	  	  "If no keychains are specified the default search list is used.",
      "Find a generic password item."},

	{ "find-certificate", keychain_find_certificate,
	  "[-a] [-e emailAddress] [-m] [-p] [keychain...]\n"
	  "    -a Find all matching certificates, not just the first one.\n"
	  "    -e Match on \"emailAddress\" when searching.\n"
	  "    -m Show the \"emailAddresses\" in the certificate.\n"
	  "    -p Output certificate in pem form.\n"
	  	  "If no keychains are specified the default search list is used.",
      "Find a certificate item."},

    { "create-db", db_create,
	  "[-ao0] [-g dl|cspdl] [-m mode] [name]\n"
	  "    -a  Turn off autocommit\n"
	  "    -g  Attach to \"guid\" rather than the AppleFileDL\n"
	  "    -m  Set the inital mode of the created db to \"mode\"\n"
	  "    -o  Force using openparams argument\n"
	  "    -0  Force using version 0 openparams\n"
	  "If no name is provided ask the user interactively",
	  "Create an db using the DL." },

	{ "leaks", leaks,
	  "[-cycles] [-nocontext] [-nostacks] [-exclude symbol]\n"
	  "    -cycles       Use a stricter algorithm (Man leaks for details).\n"
	  "    -nocontext    Withhold the hex dumps of the leaked memory.\n"
	  "    -nostacks     Don't show stack traces fo leaked memory.\n"
	  "    -exclude      Ignore leaks called from \"symbol\".\n"
	  "(Set the environment variable MallocStackLogging to get symbolic traces.)",
	  "Run /usr/bin/leaks on this proccess." },

	{}
};

/* Global variables. */
int do_quiet = 0;
int do_verbose = 0;

/* Return 1 if name matches command. */
static int
match_command(const char *command, const char *name)
{
	return !strncmp(command, name, strlen(name));
}

/* The help command. */
static int
help(int argc, char * const *argv)
{
	const command *c;

	if (argc > 1)
	{
		char * const *arg;
		for (arg = argv + 1; *arg; ++arg)
		{
			int found = 0;

			for (c = commands; c->c_name; ++c)
			{
				if (match_command(c->c_name, *arg))
				{
					found = 1;
					break;
				}
			}

			if (found)
				fprintf(stderr, "Usage: %s %s\n", c->c_name, c->c_usage);
			else
			{
				fprintf(stderr, "%s: no such command: %s\n", argv[0], *arg);
				return 1;
			}
		}
	}
	else
	{
		for (c = commands; c->c_name; ++c)
			fprintf(stderr, "    %-17s %s\n", c->c_name, c->c_help);
	}

	return 0;
}

/* States for split_line parser. */
typedef enum
{
	SKIP_WS,
	READ_ARG,
	READ_ARG_ESCAPED,
	QUOTED_ARG,
	QUOTED_ARG_ESCAPED
} parse_state;

/* Split a line into multiple arguments and return them in *pargc and *pargv. */
static void
split_line(char *line, int *pargc, char * const **pargv)
{
	static char *argvec[MAX_ARGS + 1];
	int argc = 0;
	char *ptr = line;
	char *dst = line;
	parse_state state = SKIP_WS;
	int quote_ch = 0;

	for (ptr = line; *ptr; ++ptr)
	{
		if (state == SKIP_WS)
		{
			if (isspace(*ptr))
				continue;

			if (*ptr == '"' || *ptr == '\'')
			{
				quote_ch = *ptr;
				state = QUOTED_ARG;
				argvec[argc] = dst;
				continue; /* Skip the quote. */
			}
			else
			{
				state = READ_ARG;
				argvec[argc] = dst;
			}
		}

		if (state == READ_ARG)
		{
			if (*ptr == '\\')
			{
				state = READ_ARG_ESCAPED;
				continue;
			}
			else if (isspace(*ptr))
			{
				/* 0 terminate each arg. */
				*dst++ = '\0';
				argc++;
				state = SKIP_WS;
				if (argc >= MAX_ARGS)
					break;
			}
			else
				*dst++ = *ptr;
		}

		if (state == QUOTED_ARG)
		{
			if (*ptr == '\\')
			{
				state = QUOTED_ARG_ESCAPED;
				continue;
			}
			if (*ptr == quote_ch)
			{
				/* 0 terminate each arg. */
				*dst++ = '\0';
				argc++;
				state = SKIP_WS;
				if (argc >= MAX_ARGS)
					break;
			}
			else
				*dst++ = *ptr;
		}

		if (state == READ_ARG_ESCAPED)
		{
			*dst++ = *ptr;
			state = READ_ARG;
		}

		if (state == QUOTED_ARG_ESCAPED)
		{
			*dst++ = *ptr;
			state = QUOTED_ARG;
		}
	}

	if (state != SKIP_WS)
	{
		/* Terminate last arg. */
		*dst++ = '\0';
		argc++;
	}

	/* Teminate arg vector. */
	argvec[argc] = NULL;

	*pargv = argvec;
	*pargc = argc;
}

/* Print a (hopefully) useful usage message. */
static int
usage(void)
{
	const char *p = strrchr(prog_name, '/');
	prog_name = p ? p + 1 : prog_name;
	fprintf(stderr,
		"Usage: %s [-h] [-i] [-l] [-p prompt] [-q] [-v] [command] [opt ...]\n"
		"    -i    Run in interactive mode.\n"
		"    -l    Run /usr/bin/leaks -nocontext before exiting.\n"
		"    -p    Set the prompt to \"prompt\" (implies -i).\n"
		"    -q    Be less verbose.\n"
		"    -v    Be more verbose about what's going on.\n"
		"%s commands are:\n", prog_name, prog_name);
	help(0, NULL);
	return 2;
}

/* Execute a single command. */ 
static int
execute_command(int argc, char * const *argv)
{
	const command *c;
	int found = 0;

	/* Nothing to do. */
	if (argc == 0)
		return 0;

	for (c = commands; c->c_name; ++c)
	{
		if (match_command(c->c_name, argv[0]))
		{
			found = 1;
			break;
		}
	}

	if (found)
	{
		int result;

		/* Reset getopt for command proc. */
		optind = 1;
		optreset = 1;

		if (do_verbose)
		{
			int ix;

			fprintf(stderr, "%s", c->c_name);
			for (ix = 1; ix < argc; ++ix)
				fprintf(stderr, " \"%s\"", argv[ix]);
			fprintf(stderr, "\n");
		}

		result = c->c_func(argc, argv);
		if (result == 2)
			fprintf(stderr, "Usage: %s %s\n        %s\n", c->c_name, c->c_usage, c->c_help);

		return result;
	}
	else
	{
		fprintf(stderr, "unknown command \"%s\"\n", argv[0]);
		return 1;
	}
}

int
main(int argc, char * const *argv)
{
	int result = 0;
	int do_help = 0;
	int do_interactive = 0;
	int do_leaks = 0;
	int ch;

	/* Remember my name. */
	prog_name = argv[0];

	/* Do getopt stuff for global options. */
	optind = 1;
	optreset = 1;
	while ((ch = getopt(argc, argv, "hilp:qv")) != -1)
	{
		switch  (ch)
		{
		case 'h':
			do_help = 1;
			break;
		case 'i':
			do_interactive = 1;
			break;
		case 'l':
			do_leaks = 1;
			break;
		case 'p':
			do_interactive = 1;
			prompt_string = optarg;
			break;
		case 'q':
			do_quiet = 1;
			break;
		case 'v':
			do_verbose = 1;
			break;
		case '?':
		default:
			return usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (do_help)
	{
		/* Munge argc/argv so that argv[0] is something. */
		return help(argc + 1, argv - 1);
	}
	else if (argc > 0)
		result = execute_command(argc, argv);
	else if (do_interactive)
	{
		/* In interactive mode we just read commands and run them until readline returns NULL. */
		for (;;)
		{
			static char buffer[MAX_LINE_LEN];
			char * const *av, *input;
			int ac;

			fprintf(stderr, "%s", prompt_string);
			input = readline(buffer, MAX_LINE_LEN);
			if (!input)
				break;

			split_line(input, &ac, &av);
			result = execute_command(ac, av);
			if (result == -1)
			{
				result = 0;
				break;
			}

			if (result && ! do_quiet)
			{
				fprintf(stderr, "%s: returned %d\n", av[0], result);
			}
		}
	}
	else
		result = usage();

	if (do_leaks)
	{
		char *const argvec[3] = { "leaks", "-nocontext", NULL };
		leaks(2, argvec);
	}

	return result;
}
