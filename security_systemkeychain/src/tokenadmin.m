/*
 * Copyright (c) 2003-2006 Apple Computer, Inc. All Rights Reserved.
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
 * tokenadmin.c
 */

#include "tokenadmin.h"

#include "readline.h"

//#include "cmsutil.h"
//#include "db_commands.h"
#include "create_fv_user.h"
//#include "authz.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#import <Foundation/Foundation.h>

#include <CoreFoundation/CFRunLoop.h>
#include <Security/SecBasePriv.h>
#include <security_asn1/secerr.h>

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
const char *prompt_string = "tokenadmin> ";

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
	  "Show all commands or show usage for a command." },
    
	{ "create-fv-user", create_fv_user,
	  "-u <short user Name> -l <long user Name> [-p password]\n"
	  "    -u    Use the argument as the short (i.e. unix) name for the new user.\n"
	  "    -l    Use the argument as the long name for the new user.\n"
	  "    -p    The option keychain password for the account.\n"
	  "With no parameters display usage.",
	  "Create a new FileVault user protected by a token." },
	  
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
				printf("Usage: %s %s\n", c->c_name, c->c_usage);
			else
			{
				sec_error("%s: no such command: %s", argv[0], *arg);
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

/* Print a (hopefully) useful usage message. */
static int
usage(void)
{
	printf(
		"Usage: %s [-h] [-q] [-v] [command] [opt ...]\n"
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
		sec_error("unknown command \"%s\"", argv[0]);
		return 1;
	}
}

static void
receive_notifications(void)
{
	/* Run the CFRunloop to get any pending notifications. */
	while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, TRUE) == kCFRunLoopRunHandledSource);
}


const char *
sec_errstr(int err)
{
    const char *errString;
//    if (IS_SEC_ERROR(err))
  //      errString = SECErrorString(err);
  //  else
        errString = cssmErrorString(err);
    return errString;
}

void
sec_error(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "%s: ", prog_name);

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void
sec_perror(const char *msg, int err)
{
    sec_error("%s: %s", msg, sec_errstr(err));
}

int
main(int argc, char * const *argv)
{
	int result = 0;
	int do_help = 0;
	int ch;
	const char *shortUserName = NULL;
	const char *longUserName = NULL;
	const char *password = NULL;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	/* Remember my name. */
	prog_name = strrchr(argv[0], '/');
	prog_name = prog_name ? prog_name + 1 : argv[0];

	/* Do getopt stuff for global options. */
	optind = 1;
	optreset = 1;
	while ((ch = getopt(argc, argv, "hqv")) != -1)
	{
		switch  (ch)
		{
		case 'h':
			do_help = 1;
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
	{
		receive_notifications();
		result = execute_command(argc, argv);
		if (result == 2)
			usage();
		receive_notifications();
	}
	else
		result = usage();

	[pool release];
	return result;
}
