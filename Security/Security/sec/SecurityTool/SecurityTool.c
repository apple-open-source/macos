/*
 * Copyright (c) 2003-2010,2013 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 * security.c
 */

#include "SecurityTool.h"

#include <SecurityTool/readline.h>

#include "leaks.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <CoreFoundation/CFRunLoop.h>

#if NO_SERVER
#include <securityd/spi.h>
#endif

/* Maximum length of an input line in interactive mode. */
#define MAX_LINE_LEN 4096
/* Maximum number of arguments on an input line in interactive mode. */
#define MAX_ARGS 32


/* The default prompt. */
const char *prompt_string = "security> ";

/* The name of this program. */
const char *prog_name;


/* Forward declarations of static functions. */


/* Global variables. */
int do_quiet = 0;
int do_verbose = 0;

/* Return 1 if name matches command. */
static int
match_command(const char *command_name, const char *name)
{
	return !strncmp(command_name, name, strlen(name));
}

/* The help command. */
int
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
				printf("Usage: %s %s\n", c->c_name, c->c_usage);
			else
			{
				fprintf(stderr, "%s: no such command: %s", argv[0], *arg);
				return 1;
			}
		}
	}
	else
	{
		for (c = commands; c->c_name; ++c)
			printf("    %-17s %s\n", c->c_name, c->c_help);
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
	printf(
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
		fprintf(stderr, "unknown command \"%s\"", argv[0]);
		return 1;
	}
}

static void
receive_notifications(void)
{
	/* Run the CFRunloop to get any pending notifications. */
	while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, TRUE) == kCFRunLoopRunHandledSource);
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
	prog_name = strrchr(argv[0], '/');
	prog_name = prog_name ? prog_name + 1 : argv[0];

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

#if NO_SERVER
# if DEBUG
//    securityd_init();
# endif
#endif

	if (do_help)
	{
		/* Munge argc/argv so that argv[0] is something. */
		return help(argc + 1, argv - 1);
	}
	else if (argc > 0)
	{
		receive_notifications();
		result = execute_command(argc, argv);
		receive_notifications();
	}
	else if (do_interactive)
	{
		/* In interactive mode we just read commands and run them until readline returns NULL. */

        /* Only show prompt string if stdin is a tty. */
        int show_prompt = isatty(0);

		for (;;)
		{
			static char buffer[MAX_LINE_LEN];
			char * const *av, *input;
			int ac;

            if (show_prompt)
                fprintf(stderr, "%s", prompt_string);

			input = readline(buffer, MAX_LINE_LEN);
			if (!input)
				break;

			split_line(input, &ac, &av);
			receive_notifications();
			result = execute_command(ac, av);
			receive_notifications();
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
