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

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "scutil.h"
#include "commands.h"
#include "dictionary.h"
#include "session.h"
#include "cache.h"
#include "notify.h"
#include "tests.h"

#include "SCDPrivate.h"

const cmdInfo commands[] = {
	/* cmd		minArgs	maxArgs	func					*/
	/* 	usage								*/

	{ "help",	0,	0,	do_help,		0,
		" help				: list available commands"			},

	{ "f.read",	1,	1,	do_readFile,		0,
		" f.read file			: process commands from file"			},

	/* local dictionary manipulation commands */

	{ "d.init",	0,	0,	do_dictInit,		1,
		" d.init			: initialize (empty) dictionary"		},

	{ "d.show",	0,	0,	do_dictShow,		1,
		" d.show			: show dictionary contents"			},

	{ "d.add",	2,	101,	do_dictSetKey,		1,
		" d.add key [*#?] val [v2 ...]	: add information to dictionary\n"
		"       (*=array, #=number, ?=boolean)"				},

	{ "d.remove",	1,	1,	do_dictRemoveKey,		1,
		" d.remove key			: remove key from dictionary"			},

	/* data store manipulation commands */

	{ "open",	0,	0,	do_open,		2,
		" open				: open a session with \"configd\""		},

	{ "close",	0,	0,	do_close,		2,
		" close				: close current \"configd\" session"		},

	{ "lock",	0,	0,	do_lock,		3,
		" lock				: secures write access to data store"		},

	{ "unlock",	0,	0,	do_unlock,		3,
		" unlock			: secures write access to data store"		},

	{ "list",	0,	2,	do_list,		4,
		" list [prefix] [regex]		: list keys in data store"			},

	{ "add",	1,	2,	do_add,			4,
		" add key [session]		: add key in data store w/current dict"		},

	{ "get",	1,	1,	do_get,			4,
		" get key			: get dict from data store w/key"		},

	{ "set",	1,	1,	do_set,			4,
		" set key			: set key in data store w/current dict"		},

	{ "remove",	1,	1,	do_remove,		4,
		" remove key			: remove key from data store"			},

	{ "touch",	1,	1,	do_touch,		4,
		" touch key			: touch key in data store"			},

	{ "n.list",	0,	1,	do_notify_list,		5,
		" n.list [regex]		: list notification keys"			},

	{ "n.add",	1,	2,	do_notify_add,		5,
		" n.add key [regex]		: add notification key"				},

	{ "n.remove",	1,	2,	do_notify_remove,	5,
		" n.remove key [regex]		: remove notification key"			},

	{ "n.changes",	0,	0,	do_notify_changes,	5,
		" n.changes			: list changed keys"				},

	{ "n.wait",	0,	0,	do_notify_wait,		5,
		" n.wait			: wait for changes"				},

	{ "n.watch",	0,	1,	do_notify_callback,	5,
		" n.watch [verbose]		: watch for changes"				},

	{ "n.signal",	1,	2,	do_notify_signal,	5,
		" n.signal sig [pid]		: signal changes"				},

	{ "n.file",	0,	1,	do_notify_file,		5,
		" n.file [identifier]		: watch for changes via file"			},

	{ "n.cancel",	0,	1,	do_notify_cancel,	5,
		" n.cancel			: cancel notification requests"			},

	{ "snapshot",	0,	0,	do_snapshot,		9,
		" snapshot			: save snapshot of cache and session data"	},

#ifdef	DEBUG
	{ "t.ocleak",	0,	1,	test_openCloseLeak,	9,
		" t.ocleak [#]			: test for leaks (open/close)"			},
#endif	/* DEBUG */
};

const int nCommands = (sizeof(commands)/sizeof(cmdInfo));


void
do_command(int argc, char **argv)
{
	int	i;
	char	*cmd = argv[0];

	for (i=0; i<nCommands; i++) {
		if (strcasecmp(cmd, commands[i].cmd) == 0) {
			--argc;
			argv++;
			if (argc < commands[i].minArgs) {
				SCDLog(LOG_INFO, CFSTR("%s: too few arguments"), cmd);
				return;
			} else if (argc > commands[i].maxArgs) {
				SCDLog(LOG_INFO, CFSTR("%s: too many arguments"), cmd);
				return;
			}
			commands[i].func(argc, argv);
			return;
		}
	}

	SCDLog(LOG_INFO, CFSTR("%s: unknown, type \"help\" for command info"), cmd);
	return;
}


void
do_help(int argc, char **argv)
{
	int	g = -1;		/* current group */
	int	i;

	SCDLog(LOG_NOTICE, CFSTR(""));
	SCDLog(LOG_NOTICE, CFSTR("Available commands:"));
	for (i=0; i<nCommands; i++) {
		if (g != commands[i].group) {
			SCDLog(LOG_NOTICE, CFSTR(""));
			g = commands[i].group;
		}
		SCDLog(LOG_NOTICE, CFSTR("%s"), commands[i].usage);
	}
	SCDLog(LOG_NOTICE, CFSTR(""));

	return;
}


void
do_readFile(int argc, char **argv)
{
	FILE		*fp = fopen(argv[0], "r");
	boolean_t	ok;

	if (fp == NULL) {
		SCDLog(LOG_INFO, CFSTR("f.read: could not open file (%s)."), strerror(errno));
		return;
	}

	/* open file, increase nesting level */
	SCDLog(LOG_DEBUG, CFSTR("f.read: reading file (%s)."), argv[0]);
	nesting++;

	if (SCDOptionGet(NULL, kSCDOptionUseCFRunLoop)) {
		CFSocketRef		in;
		CFSocketContext		context = { 0, fp, NULL, NULL, NULL };
		CFRunLoopSourceRef	rls;

		/* create a "socket" reference with the file descriptor associated with stdin */
		in  = CFSocketCreateWithNative(NULL,
					       fileno(fp),
					       kCFSocketReadCallBack,
					       runLoopProcessInput,
					       &context);

		/* Create and add a run loop source for the file descriptor */
		rls = CFSocketCreateRunLoopSource(NULL, in, nesting);

		/*
		 * Remove the current input file from the run loop sources. We
		 * will reactivate the current input file source when we are
		 * finished reading data from the new file.
		 */
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
				      (CFRunLoopSourceRef) CFArrayGetValueAtIndex(sources, 0),
				      kCFRunLoopDefaultMode);

		/* keep track of this new source */
		CFArrayInsertValueAtIndex(sources, 0, rls);

		/* add this source to the run loop */
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

		CFRelease(rls);
		CFRelease(in);
	} else {
		do {
			/* debug information, diagnostics */
			_showMachPortStatus();

			/* process command */
			ok = process_line(fp);
		} while (ok);

		/* decrement the nesting level */
		nesting--;
	}

	return;
}
