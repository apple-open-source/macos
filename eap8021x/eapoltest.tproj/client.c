
/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <syslog.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <EAP8021X/EAPOLControl.h>
#include "myCFUtil.h"

typedef int func_t(int argc, char * argv[]);
typedef func_t * funcptr_t;
char * progname = NULL;

static const char *
S_state_names(EAPOLControlState state) 
{
    static const char * names[] = {
	"Idle", "Starting", "Running", "Stopping"
    };
    if (state <= kEAPOLControlStateStopping) {
	return (names[state]);
    }
    return ("<unknown>");
}

static int
S_state(int argc, char * argv[])
{
    CFDictionaryRef	dict = NULL;
    int 		result;
    EAPOLControlState 	state;

    result = EAPOLControlCopyStateAndStatus(argv[0], &state, &dict);
    if (result == 0) {
	fprintf(stderr, "EAPOLControlGetState %s\n",
		S_state_names(state));
	if (dict != NULL) {
	    printf("Status dict:\n");
	    CFShow(dict);
	    printf("\n");
	    CFRelease(dict);
	}
    }
    else {
	fprintf(stderr, "EAPOLControlGetState returned %d\n", result);
    }
    return (result);
}

static int
S_start(int argc, char * argv[])
{
    CFDictionaryRef	dict = NULL;
    int 		result;

    if (access(argv[1], R_OK) != 0) {
	fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
	return (errno);
    }
    dict = (CFDictionaryRef)my_CFPropertyListCreateFromFile(argv[1]);
    if (isA_CFDictionary(dict) == NULL) {
	fprintf(stderr, "contents of file %s invalid\n", argv[1]);
	my_CFRelease(&dict);
	return (EINVAL);
    }
    result = EAPOLControlStart(argv[0], dict);
    fprintf(stderr, "EAPOLControlStart returned %d\n", result);
    my_CFRelease(&dict);
    return (result);
}

static int
S_stop(int argc, char * argv[])
{
    int result;

    result = EAPOLControlStop(argv[0]);
    fprintf(stderr, "EAPOLControlStop returned %d\n", result);
    return (result);
}

static int
S_retry(int argc, char * argv[])
{
    int result;

    result = EAPOLControlRetry(argv[0]);
    fprintf(stderr, "EAPOLControlRetry returned %d\n", result);
    return (result);
}

static int
S_update(int argc, char * argv[])
{
    CFDictionaryRef	dict = NULL;
    int 		result;

    if (access(argv[1], R_OK) != 0) {
	fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
	return (errno);
    }
    dict = (CFDictionaryRef)my_CFPropertyListCreateFromFile(argv[1]);
    if (isA_CFDictionary(dict) == NULL) {
	fprintf(stderr, "contents of file %s invalid\n", argv[1]);
	my_CFRelease(&dict);
	return (EINVAL);
    }
    result = EAPOLControlUpdate(argv[0], dict);
    fprintf(stderr, "EAPOLControlUpdate returned %d\n", result);
    my_CFRelease(&dict);
    return (result);
}

static int
S_log(int argc, char * argv[])
{
    int32_t		level;
    int 		result;

    level = strtol(argv[1], 0, 0);
    result = EAPOLControlSetLogLevel(argv[0], level);
    fprintf(stderr, "EAPOLControlSetLogLevel returned %d\n", result);
    return (result);
}

static struct {
    char *	command;
    funcptr_t	func;
    int		argc;
    char *	usage;
} commands[] = {
    { "state", S_state, 1, "<interface_name" },
    { "start", S_start, 2, "<interface_name> <config_file>" },
    { "stop", S_stop, 1, "<interface_name>" },
    { "retry", S_retry, 1, "<interface_name>" },
    { "update", S_update, 2, "<interface_name> <config_file>" },
    { "log", S_log, 2, "<interface_name> <level>" },
    { NULL, NULL, 0, NULL },
};

void
usage()
{
    int i;
    fprintf(stderr, "usage: %s <command> <args>\n", progname);
    fprintf(stderr, "where <command> is one of ");
    for (i = 0; commands[i].command; i++) {
	fprintf(stderr, "%s%s",  i == 0 ? "" : ", ",
		commands[i].command);
    }
    fprintf(stderr, "\n");
    exit(1);
}

static funcptr_t
S_lookup_func(char * cmd, int argc)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcmp(cmd, commands[i].command) == 0) {
	    if (argc < commands[i].argc) {
		fprintf(stderr, "usage: %s %s\n", commands[i].command,
			commands[i].usage ? commands[i].usage : "");
		exit(1);
	    }
	    return commands[i].func;
	}
    }
    return (NULL);
}

int
main(int argc, char * argv[])
{
    funcptr_t		func;

    progname = argv[0];
    if (argc < 2)
	usage();

    argv++; argc--;

    func = S_lookup_func(argv[0], argc - 1);
    if (func == NULL)
	usage();
    argv++; argc--;
    exit ((*func)(argc, argv));
}
