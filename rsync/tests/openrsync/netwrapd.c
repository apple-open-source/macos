/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Klara, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * netwrapd - an absolute barebones implementation of socket activating another
 * process (a la inetd).  netwrapd does not have a configuration format; it has
 * three environment variables that it will do something with:
 *
 *  - NETWRAP_PORT: tcp port to listen on.  netwrapd will only listen on
 *      127.0.0.1 (IPv4-only) at the moment, since this is sufficient for our
 *      purposes of testing rsyncd features.  If not set, defaults to 0.
 *  - NETWRAP_PROG: program to execute.  If not set, then it will be inferred
 *      from NETWRAP_ARGS argv[0].  The program is executed with execvp(3), so
 *      the program may be the name of an executable in $PATH or a path to an
 *      executable.
 *  - NETWRAP_ARGS: arguments to pass to the program.  If not set, then argv[0]
 *      will be inferred from NETWRAP_PROG and no arguments will be passed.
 *      This variable will be word-split to produce an argv[] array, starting
 *      with argv[0].
 *
 * netwrapd may take a -p flag, the argument of which names a file in which to
 * write the port we're listening on.  The intention is to allow us to avoid
 * having to choose a port, we just specify port 0 (the default) and write the
 * port chosen by the kernel out for the caller to use.
 */

/* Compatibility bits */
#ifndef howmany
#define	howmany(x, y)	((((x) % (y)) == 0) ? ((x) / (y)) : (((x) / (y)) + 1))
#endif
#ifndef PAGE_SIZE
/*
 * Not critical to have correct, just our arbitrary choice for how many args
 * we want to support.
 */
#define	PAGE_SIZE	4096
#endif

/*
 * This is strictly for testing purposes, so hardcoding a bind address and not
 * allowing, e.g., "localhost", is fine enough for our purposes.
 */
#define	NETWRAP_BIND_ADDR	"127.0.0.1"

#define	NETWRAP_SOCK_BACKLOG	5

#define	NETWRAP_MAX_ARGS	howmany(PAGE_SIZE, sizeof(char *))

static const char *netwrap_prog;
static const char *netwrap_args[NETWRAP_MAX_ARGS];

static const char *netwrap_arg_env;

static void netwrap_argsplit(const char *, const char **, size_t);

#ifndef __printflike
#define	__printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#endif

static void __printflike(1, 2)
netwrap_err(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	_exit(1);
}

static void
fetch_execinfo(void)
{

	netwrap_prog = getenv("NETWRAP_PROG");
	if (netwrap_prog != NULL && *netwrap_prog == '\0')
		netwrap_prog = NULL;	/* Derive from argv[0] */

	netwrap_arg_env = getenv("NETWRAP_ARGS");
	if (netwrap_arg_env != NULL && *netwrap_arg_env == '\0')
		netwrap_arg_env = NULL;	/* Derive argv[0] from NETWRAP_PROG */

	if (netwrap_arg_env != NULL)
		netwrap_argsplit(netwrap_arg_env, &netwrap_args[0], NETWRAP_MAX_ARGS);

	if (netwrap_prog == NULL)
		netwrap_prog = netwrap_args[0];
	else if (netwrap_args[0] == NULL)
		netwrap_args[0] = netwrap_prog;

	if (netwrap_args[0] == NULL && netwrap_prog == NULL)
		netwrap_err("Must specify either NETWRAP_PROG or NETWRAP_ARGS\n");
}

static unsigned short
get_port(void)
{
	char *endp;
	const char *portstr;
	long port;

	portstr = getenv("NETWRAP_PORT");
	if (portstr == NULL || *portstr == '\0')
		return (0);

	errno = 0;
	port = strtol(portstr, &endp, 0);
	if (errno != 0 || *endp != '\0')
		netwrap_err("Bad NETWRAP_PORT\n");
	else if (port < 0 || port > USHRT_MAX)
		netwrap_err("NETWRAP_PORT out of range\n");

	return (htons(((unsigned short)port)));
}

static void
populate_addr(struct sockaddr_in *sin)
{

	sin->sin_family = AF_INET;
	sin->sin_port = get_port();

	if (inet_pton(sin->sin_family, NETWRAP_BIND_ADDR, &sin->sin_addr) != 1)
		netwrap_err("Failed to parse bind addr\n");
}

static int
create_socket(int domain, const struct sockaddr *addr, socklen_t addrlen)
{
	int ret, sock;

	sock = socket(domain, SOCK_STREAM, 0);
	if (sock == -1)
		netwrap_err("Failed to create socket\n");

	ret = bind(sock, addr, addrlen);
	if (ret == -1)
		netwrap_err("Failed to bind\n");

	return (sock);
}

static void
do_exec(void)
{

	execvp(netwrap_prog, (char * const *)netwrap_args);
}

static bool
handle_child(int clsock)
{
	pid_t p;

	p = fork();
	if (p != 0) {
		/*
		 * Don't leak the client socket, return to handle another.
		 */
		close(clsock);
		return (p > 0);
	}

	/*
	 * Wire the socket up to stdout/stdin, leave stderr alone in case the test
	 * program wants to examine it.
	 */
	dup2(clsock, STDIN_FILENO);
	dup2(clsock, STDOUT_FILENO);
	if (clsock != STDIN_FILENO && clsock != STDOUT_FILENO) {
		close(clsock);
		clsock = -1;
	}

	/* Execute the program */
	do_exec();
	_exit(1);
}

static void
usage(const char *progname)
{

	fprintf(stderr, "usage: %s [-p portfile]\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in sin = { 0 };
	socklen_t slen;
	const char *portfile;
	int ch, sock;

	portfile = NULL;
	while ((ch = getopt(argc, argv, "p:")) != -1) {
		switch (ch) {
		case 'p':
			portfile = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	fetch_execinfo();

	populate_addr(&sin);
	sock = create_socket(PF_INET, (const struct sockaddr *)&sin, sizeof(sin));
	assert(sock >= 0);

	slen = sizeof(sin);
	if (getsockname(sock, (struct sockaddr *)&sin, &slen) == -1) {
		close(sock);
		netwrap_err("Failed to get socket information\n");
	}

	if (listen(sock, NETWRAP_SOCK_BACKLOG) == -1) {
		close(sock);
		netwrap_err("Failed to listen\n");
	}

	/*
	 * The only thing we print to stdout is the port chosen, in case it wasn't
	 * specified.
	 */
	if (portfile != NULL) {
		FILE *portf;

		portf = fopen(portfile, "w");
		if (portf == NULL)
			netwrap_err("Failed to open portfile '%s'\n", portfile);

		fprintf(portf, "%d\n", ntohs(sin.sin_port));
		fclose(portf);
	}

	for (;;) {
		struct sockaddr_storage saddr;
		int clsock;

		slen = sizeof(saddr);
		clsock = accept(sock, (struct sockaddr *)&saddr, &slen);
		if (clsock == -1) {
			close(sock);
			netwrap_err("Failed to accept\n");
		}

		if (!handle_child(clsock)) {
			close(sock);
			netwrap_err("Child handler failed\n");
		}
	}
}

/*
 * Parses the program, adding each word to the current arguments as it goes.
 *
 * Lifted from openrsync.
 */
static void
netwrap_argsplit(const char *args, const char **argv, size_t maxargc)
{
	const char *arg, *end;
	char *mprog, *walker;
	char lastquote, quotec;
	size_t argc;

	mprog = strdup(args);
	if (mprog == NULL)
		netwrap_err("strdup: %s\n", strerror(errno));

	argc = 0;
	end = &mprog[strlen(args) + 1];
	quotec = lastquote = '\0';
	for (arg = walker = mprog; *walker != '\0'; walker++) {
		/* Add what we have so far once we hit whitespace. */
		if (isspace(*walker)) {
			lastquote = '\0';
			if (argc == maxargc)
				netwrap_err("Too many arguments (max: %zu)\n", maxargc);
			*walker = '\0';
			argv[argc++] = strdup(arg);
			if (argv[argc - 1] == NULL)
				netwrap_err("strdup: %s\n", strerror(errno));

			/* Skip entire sequence of whitespace. */
			while (isspace(*(walker + 1)))
				walker++;

			arg = walker + 1;
			continue;
		} else if (*walker == '"' || *walker == '\'') {
			char *search = walker + 1;

			quotec = *walker;

			/*
			 * Compatible with the reference rsync, but not with
			 * traditional shell style: we don't strip off the
			 * the beginning quote of the second quoted part of a
			 * single arg.
			 */
			if (arg == walker || quotec != lastquote) {
				memmove(walker, walker + 1, end - (walker + 1));
				search = walker;
				end--;
			}


			/*
			 * Skip to the closing quote; smb rsync doesn't seem to
			 * even try to deal with escaped quotes.  If we didn't
			 * find a closing quote, we'll bail out and report the
			 * error.
			 */
			walker = strchr(search, quotec);
			if (walker == NULL)
				break;

			/*
			 * We'll move the remainder of the string over and
			 * strip off the quote character, then take a step
			 * backward and let us process whichever quote just
			 * replaced our terminal quote.
			 */
			memmove(walker, walker + 1, end - (walker + 1));
			assert(walker > arg);
			end--;
			walker--;

			lastquote = quotec;
			quotec = '\0';

			continue;
		}
	}

	if (quotec != '\0') {
		netwrap_err("Missing terminating `%c` in specified command\n",
		    quotec);
	} else if (walker > arg) {
		if (argc == maxargc)
			netwrap_err("Too many arguments (max: %zu)\n", maxargc);
		*walker = '\0';
		argv[argc++] = strdup(arg);
		if (argv[argc - 1] == NULL)
			netwrap_err("strdup: %s\n", strerror(errno));
	}

	free(mprog);
}
