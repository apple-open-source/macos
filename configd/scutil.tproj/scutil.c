/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>

#ifdef	DEBUG
#include <mach/mach.h>
#include <mach/mach_error.h>
#endif	/* DEBUG */

#include "scutil.h"
#include "SCDPrivate.h"
#include "commands.h"
#include "dictionary.h"

#define LINE_LENGTH 256

SCDSessionRef		session = NULL;
SCDHandleRef		data    = NULL;
int			nesting = 0;
CFMutableArrayRef	sources = NULL;


static char *
getLine(char *buf, int len, FILE *fp)
{
	int x;

	if (fgets(buf, len, fp) == NULL)
		return NULL;

	x = strlen(buf);
	if (buf[x-1] == '\n') {
		/* the entire line fit in the buffer, remove the newline */
		buf[x-1] = '\0';
	} else {
		/* eat the remainder of the line */
		do {
			x = fgetc(fp);
		} while ((x != '\n') && (x != EOF));
	}

	return buf;
}


char *
getString(char **line)
{
	char *s, *e, c, *string;
	int i, isQuoted = 0, escaped = 0;

	if (*line == NULL) return NULL;
	if (**line == '\0') return NULL;

	/* Skip leading white space */
	while (isspace(**line)) *line += 1;

	/* Grab the next string */
	s = *line;
	if (*s == '\0') {
		return NULL;				/* no string available */
	} else if (*s == '"') {
		isQuoted = 1;				/* it's a quoted string */
		s++;
	}

	for (e = s; (c = *e) != '\0'; e++) {
		if (isQuoted && (c == '"'))
			break;				/* end of quoted string */
		if (c == '\\') {
			e++;
			if (*e == '\0')
				break;			/* if premature end-of-string */
			if ((*e == '"') || isspace(*e))
				escaped++;		/* if escaped quote or white space */
		}
		if (!isQuoted && isspace(c))
			break;				/* end of non-quoted string */
	}

	string = malloc(e - s - escaped + 1);

	for (i = 0; s < e; s++) {
		string[i] = *s;
		if (!((s[0] == '\\') && ((s[1] == '"') || isspace(s[1])))) i++;
	}
	string[i] = '\0';

	if (isQuoted)
		e++;					/* move past end of quoted string */

	*line = e;
	return string;
}


boolean_t
process_line(FILE *fp)
{
	char	line[LINE_LENGTH], *s, *arg, **argv = NULL;
	int	i, argc;

	/* if end-of-file, exit */
	if (getLine(line, sizeof(line), fp) == NULL)
		return FALSE;

	if ((nesting > 0) && SCDOptionGet(NULL, kSCDOptionVerbose)) {
		SCDLog(LOG_NOTICE, CFSTR("%d> %s"), nesting, line);
	}

	/* if requested, exit */
	if (strcasecmp(line, "exit") == 0) return FALSE;
	if (strcasecmp(line, "quit") == 0) return FALSE;
	if (strcasecmp(line, "q"   ) == 0) return FALSE;

	/* break up the input line */
	s = line;
	argc = 0;
	while ((arg = getString(&s)) != NULL) {
		if (argc == 0)
			argv = (char **)malloc(2 * sizeof(char *));
		else
			argv = (char **)realloc(argv, ((argc + 2) * sizeof(char *)));
		argv[argc++] = arg;
	}

	/* process the command */
	if (argc > 0) {
		argv[argc] = NULL;	/* just in case... */

		if (*argv[0] != '#')
			do_command(argc, argv);

		for (i = 0; i < argc; i++)
			free(argv[i]);
		free(argv);
	}

	return TRUE;
}


void
runLoopProcessInput(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info)
{
	FILE	*fp = info;

	if (process_line(fp) == FALSE) {
		/* we don't want any more input from this stream, stop listening */
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
				      (CFRunLoopSourceRef)CFArrayGetValueAtIndex(sources, 0),
				      kCFRunLoopDefaultMode);

		/* we no longer need the fd (socket) */
		CFSocketInvalidate(s);

		/* we no longer need to track this source */
		CFArrayRemoveValueAtIndex(sources, 0);

		if (CFArrayGetCount(sources) > 0) {
			/* add the previous input source to the run loop */
			CFRunLoopAddSource(CFRunLoopGetCurrent(),
					   (CFRunLoopSourceRef)CFArrayGetValueAtIndex(sources, 0),
					   kCFRunLoopDefaultMode);
		} else {
			/* no more input sources, we're done! */
			exit (EX_OK);
		}

		/* decrement the nesting level */
		nesting--;
	}

	if (SCDOptionGet(NULL, kSCDOptionUseCFRunLoop)) {
		/* debug information, diagnostics */
		_showMachPortStatus();

		/* if necessary, re-issue prompt */
		if ((CFArrayGetCount(sources) == 1) && isatty(STDIN_FILENO)) {
			printf("> ");
			fflush(stdout);
		}
	}

	return;
}


int
main(int argc, const char *argv[])
{
	extern int	optind;
	int		opt;
	boolean_t	ok;

	/* process any arguments */

	SCDOptionSet(NULL, kSCDOptionUseCFRunLoop, FALSE);

	while ((opt = getopt(argc, argv, "dvr")) != -1)
		switch(opt) {
		case 'd':
			SCDOptionSet(NULL, kSCDOptionDebug, TRUE);
			break;
		case 'v':
			SCDOptionSet(NULL, kSCDOptionVerbose, TRUE);
			break;
		case 'r':
			SCDOptionSet(NULL, kSCDOptionUseCFRunLoop, TRUE);
			break;
		case '?':
		default :
			do_help(0, NULL);
	}
	argc -= optind;
	argv += optind;

	/* start with an empty dictionary */
	do_dictInit(0, NULL);

	if (SCDOptionGet(NULL, kSCDOptionUseCFRunLoop)) {
		CFSocketRef		in;
		CFSocketContext		context = { 0, stdin, NULL, NULL, NULL };
		CFRunLoopSourceRef	rls;

		/* create a "socket" reference with the file descriptor associated with stdin */
		in  = CFSocketCreateWithNative(NULL,
					       STDIN_FILENO,
					       kCFSocketReadCallBack,
					       runLoopProcessInput,
					       &context);

		/* Create a run loop source for the (stdin) file descriptor */
		rls = CFSocketCreateRunLoopSource(NULL, in, nesting);

		/* keep track of input file sources */
		sources = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(sources, rls);

		/* add this source to the run loop */
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

		CFRelease(rls);
		CFRelease(in);
	}

	do {
		/* debug information, diagnostics */
		_showMachPortStatus();

		/* issue prompt */
		if (isatty(STDIN_FILENO)) {
			printf("> ");
			fflush(stdout);
		}

		if (SCDOptionGet(NULL, kSCDOptionUseCFRunLoop)) {
			CFRunLoopRun();	/* process input, process events */
			ok = FALSE;	/* if the RunLoop exited */
		} else {
			/* process command */
			ok = process_line(stdin);
		}
	} while (ok);

	exit (EX_OK);	// insure the process exit status is 0
	return 0;	// ...and make main fit the ANSI spec.
}
