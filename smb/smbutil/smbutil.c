/*
 * Copyright (c) 2001 - 2007 Apple Inc. All rights reserved
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>

#include <smbclient/smbclient.h>
#include <smbclient/ntstatus.h>


#include "common.h"

extern char *__progname;

static void help(void);
static void help_usage(void);
static int  cmd_help(int argc, char *argv[]);

int verbose = 0;

typedef int cmd_fn_t (int argc, char *argv[]);
typedef void cmd_usage_t (void);


static struct commands {
	const char *	name;
	cmd_fn_t*	fn;
	cmd_usage_t *	usage;
} commands[] = {
	{"help",		cmd_help,		help_usage},
	{"lookup",		cmd_lookup,		lookup_usage},
	{"status",		cmd_status,		status_usage},
	{"view",		cmd_view,		view_usage},
	{"dfs",			cmd_dfs,		dfs_usage},
	{"identity",	cmd_identity,	identity_usage},
	{NULL, NULL, NULL}
};

static struct commands *
lookupcmd(const char *name)
{
	struct commands *cmd;

	for (cmd = commands; cmd->name; cmd++) {
		if (strcmp(cmd->name, name) == 0)
			return cmd;
	}
	return NULL;
}

void 
ntstatus_to_err(NTSTATUS status)
{
	switch (status) {
		case STATUS_NO_SUCH_DEVICE:
			err(EX_UNAVAILABLE, "failed to intitialize the smb library");
			break;
		case STATUS_LOGON_FAILURE:
			err(EX_NOPERM, "server rejected the authentication");
			break;
		case STATUS_CONNECTION_REFUSED:
			err(EX_NOHOST, "server connection failed");
			break;
		case STATUS_NO_SUCH_USER:
			err(EX_NOUSER, "no such network user");
			break;
		case STATUS_INVALID_HANDLE:
			err(EX_UNAVAILABLE, "invalid handle, internal error");
			break;
		case STATUS_NO_MEMORY:
			err(EX_UNAVAILABLE, "no memory, internal error");
			break;
		case STATUS_INVALID_PARAMETER:
			err(EX_USAGE, "Invalid parameter. Please correct the URL and try again");
			break;
		case STATUS_BAD_NETWORK_NAME:
			err(EX_NOHOST, "share name doesn't exist");
			break;
		case STATUS_NOT_SUPPORTED:
			err(EX_NOHOST, "operation not supported by server");
			break;
		default:
			err(EX_OSERR, "unknown status %d", status);
			break;
	}
}

int
cmd_help(int argc, char *argv[])
{
	struct commands *cmd;
	char *cp;
    
	if (argc < 2)
		help_usage();
	cp = argv[1];
	cmd = lookupcmd(cp);
	if (cmd == NULL)
		errx(EX_DATAERR, "unknown command %s", cp);
	if (cmd->usage == NULL)
		errx(EX_DATAERR, "no specific help for command %s", cp);
	cmd->usage();
	exit(0);
}

int
main(int argc, char *argv[])
{
	struct commands *cmd;
	char *cp;
	int opt;

	if (argc < 2)
		help();

	while ((opt = getopt(argc, argv, "hv")) != EOF) {
		switch (opt) {
		    case 'h':
			help();
			/*NOTREACHED */
		    case 'v':
			verbose = 1;
			break;
		    default:
			warnx("invalid option %c", opt);
			help();
			/*NOTREACHED */
		}
	}
	if (optind >= argc)
		help();

	cp = argv[optind];
	cmd = lookupcmd(cp);
	if (cmd == NULL)
		errx(EX_DATAERR, "unknown command %s", cp);

	argc -= optind;
	argv += optind;
	optind = optreset = 1;
	return cmd->fn(argc, argv);
}

static void
help(void) {
	fprintf(stderr, "\n");
	fprintf(stderr, "usage: %s [-hv] subcommand [args]\n", __progname);
	fprintf(stderr, "where subcommands are:\n"
	" help		display help on specified subcommand\n"
	" lookup 	resolve NetBIOS name to IP address\n"
	" status 	resolve IP address or DNS name to NetBIOS names\n"
	" view		list resources on specified host\n"
	" dfs		list DFS referrals\n"
	" identity	identity of the user as known by the specified host\n"
	"\n");
	exit(1);
}

static void
help_usage(void) {
	fprintf(stderr, "usage: smbutil help command\n");
	exit(1);
}
