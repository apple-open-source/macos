/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
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

#include "SCDynamicStoreInternal.h"


const cmdInfo commands[] = {
	/* cmd		minArgs	maxArgs	func			group	ctype			*/
	/* 	usage										*/

	{ "help",	0,	0,	do_help,		0,	0,
		" help                          : list available commands"			},

	{ "f.read",	1,	1,	do_readFile,		0,	0,
		" f.read file                   : process commands from file"			},

	/* local dictionary manipulation commands */

	{ "d.init",	0,	0,	do_dictInit,		1,	0,
		" d.init                        : initialize (empty) dictionary"		},

	{ "d.show",	0,	0,	do_dictShow,		1,	0,
		" d.show                        : show dictionary contents"			},

	{ "d.add",	2,	101,	do_dictSetKey,		1,	0,
		" d.add key [*#?] val [v2 ...]  : add information to dictionary\n"
		"       (*=array, #=number, ?=boolean)"				},

	{ "d.remove",	1,	1,	do_dictRemoveKey,	1,	0,
		" d.remove key                  : remove key from dictionary"			},

	/* data store manipulation commands */

	{ "open",	0,	0,	do_open,		2,	0,
		" open                          : open a session with \"configd\""		},

	{ "close",	0,	0,	do_close,		2,	0,
		" close                         : close current \"configd\" session"		},

	{ "lock",	0,	0,	do_lock,		3,	1,
		" lock                          : secures write access to data store"		},

	{ "unlock",	0,	0,	do_unlock,		3,	1,
		" unlock                        : secures write access to data store"		},

	{ "list",	0,	2,	do_list,		4,	0,
		" list [pattern]                : list keys in data store"			},

	{ "add",	1,	2,	do_add,			4,	0,
		" add key [\"temporary\"]         : add key in data store w/current dict"		},

	{ "get",	1,	1,	do_get,			4,	0,
		" get key                       : get dict from data store w/key"		},

	{ "set",	1,	1,	do_set,			4,	0,
		" set key                       : set key in data store w/current dict"		},

	{ "show",	1,	2,	do_show,		4,	0,
		" show key [\"pattern\"]          : show values in data store w/key"		},

	{ "remove",	1,	1,	do_remove,		4,	0,
		" remove key                    : remove key from data store"			},

	{ "notify",	1,	1,	do_notify,		4,	0,
		" notify key                    : notify key in data store"			},

	{ "touch",	1,	1,	do_touch,		4,	1,
		" touch key                     : touch key in data store"			},

	{ "n.list",	0,	1,	do_notify_list,		5,	0,
		" n.list [\"pattern\"]            : list notification keys"			},

	{ "n.add",	1,	2,	do_notify_add,		5,	0,
		" n.add key [\"pattern\"]         : add notification key"				},

	{ "n.remove",	1,	2,	do_notify_remove,	5,	0,
		" n.remove key [\"pattern\"]      : remove notification key"			},

	{ "n.changes",	0,	0,	do_notify_changes,	5,	0,
		" n.changes                     : list changed keys"				},

	{ "n.watch",	0,	1,	do_notify_watch,	5,	0,
		" n.watch [verbose]             : watch for changes"				},

	{ "n.wait",	0,	0,	do_notify_wait,		5,	2,
		" n.wait                        : wait for changes"				},

	{ "n.callback",	0,	1,	do_notify_callback,	5,	2,
		" n.callback [\"verbose\"]        : watch for changes"				},

	{ "n.signal",	1,	2,	do_notify_signal,	5,	2,
		" n.signal sig [pid]            : signal changes"				},

	{ "n.file",	0,	1,	do_notify_file,		5,	2,
		" n.file [identifier]           : watch for changes via file"			},

	{ "n.cancel",	0,	1,	do_notify_cancel,	5,	0,
		" n.cancel                      : cancel notification requests"			},

	{ "snapshot",	0,	0,	do_snapshot,		9,	2,
		" snapshot                      : save snapshot of store and session data"	},
};

const int nCommands = (sizeof(commands)/sizeof(cmdInfo));

Boolean enablePrivateAPI	= FALSE;


void
do_command(int argc, char **argv)
{
	int	i;
	char	*cmd = argv[0];

	for (i = 0; i < nCommands; i++) {
		if ((commands[i].ctype > 1) && !enablePrivateAPI)  {
			continue;	/* if "private" API and access has not been enabled */
		}

		if (strcasecmp(cmd, commands[i].cmd) == 0) {
			--argc;
			argv++;
			if (argc < commands[i].minArgs) {
				SCPrint(TRUE, stdout, CFSTR("%s: too few arguments\n"), cmd);
				return;
			} else if (argc > commands[i].maxArgs) {
				SCPrint(TRUE, stdout, CFSTR("%s: too many arguments\n"), cmd);
				return;
			}
			commands[i].func(argc, argv);
			return;
		}
	}

	SCPrint(TRUE, stdout, CFSTR("%s: unknown, type \"help\" for command info\n"), cmd);
	return;
}


void
do_help(int argc, char **argv)
{
	int	g = -1;		/* current group */
	int	i;

	SCPrint(TRUE, stdout, CFSTR("\nAvailable commands:\n"));
	for (i = 0; i < nCommands; i++) {
		if ((commands[i].ctype > 0) && !enablePrivateAPI)  {
			continue;	/* if "private" API and access has not been enabled */
		}

		/* check if this is a new command group */
		if (g != commands[i].group) {
			SCPrint(TRUE, stdout, CFSTR("\n"));
			g = commands[i].group;
		}

		/* display the command */
		SCPrint(TRUE, stdout, CFSTR("%s\n"), commands[i].usage);
	}
	SCPrint(TRUE, stdout, CFSTR("\n"));

	return;
}


void
do_readFile(int argc, char **argv)
{
	InputRef		src;

	/* allocate command input stream */
	src = (InputRef)CFAllocatorAllocate(NULL, sizeof(Input), 0);
	src->el = NULL;
	src->fp = fopen(argv[0], "r");

	if (src->fp == NULL) {
		SCPrint(TRUE, stdout, CFSTR("f.read: could not open file (%s).\n"), strerror(errno));
		CFAllocatorDeallocate(NULL, src);
		return;
	}

	/* open file, increase nesting level */
	SCPrint(TRUE, stdout, CFSTR("f.read: reading file (%s).\n"), argv[0]);
	nesting++;

	while (process_line(src) == TRUE) {
	       /* debug information, diagnostics */
		__showMachPortStatus();
	}

	(void)fclose(src->fp);
	CFAllocatorDeallocate(NULL, src);

	return;
}
