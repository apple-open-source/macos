
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
#include <sys/time.h>
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
#include <SystemConfiguration/SystemConfiguration.h>
#include <EAP8021X/EAPOLControl.h>
#include "myCFUtil.h"

typedef int func_t(int argc, char * argv[]);
typedef func_t * funcptr_t;
char * progname = NULL;

void
timestamp_fprintf(FILE * f, const char * message, ...)
{
    struct timeval	tv;
    struct tm       	tm;
    time_t		t;
    va_list		ap;

    (void)gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    (void)localtime_r(&t, &tm);

    va_start(ap, message);
    fprintf(f, "%04d/%02d/%02d %2d:%02d:%02d.%06d ",
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec,
	    tv.tv_usec);
    vfprintf(f, message, ap);
    va_end(ap);
}

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
get_eapol_interface_status(const char * ifname)
{
    CFDictionaryRef	dict = NULL;
    int 		result;
    EAPOLControlState 	state;

    result = EAPOLControlCopyStateAndStatus(ifname, &state, &dict);
    if (result == 0) {
	fprintf(stdout, "EAPOLControlCopyStateAndStatus(%s) =  %s\n", ifname,
		S_state_names(state));
	if (dict != NULL) {
	    SCPrint(TRUE, stdout, CFSTR("Status dict:\n%@\n"), dict);
	    CFRelease(dict);
	}
    }
    else {
	fprintf(stderr, "EAPOLControlCopyStateAndStatus(%s) returned %d\n", ifname, result);
    }
    return (result);

}

static SCDynamicStoreRef
config_session_start(SCDynamicStoreCallBack func, void * arg, const char * ifname)
{
    SCDynamicStoreContext	context;
    CFStringRef			key;
    SCDynamicStoreRef		store;

    bzero(&context, sizeof(context));
    context.info = arg;
    store = SCDynamicStoreCreate(NULL, CFSTR("/usr/local/bin/eapoltest"), 
				 func, &context);
    if (store == NULL) {
	fprintf(stderr, "SCDynamicStoreCreate() failed, %s",
		SCErrorString(SCError()));
	return (NULL);
    }
    /* EAPClient status notifications */
    if (ifname == NULL) {
	/* watch all interfaces */
	CFArrayRef			patterns;

	key = EAPOLControlAnyInterfaceKeyCreate();
	patterns = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
	CFRelease(key);
	SCDynamicStoreSetNotificationKeys(store, NULL, patterns);
	CFRelease(patterns);
    }
    else {
	/* watch just one interface */
	CFArrayRef			keys = NULL;

	key = EAPOLControlKeyCreate(ifname);
	keys = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
	CFRelease(key);
	SCDynamicStoreSetNotificationKeys(store, keys, NULL);
	CFRelease(keys);
    }
    return (store);
}

static int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    CFIndex		l;
    CFIndex		n;
    CFRange		range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    n = CFStringGetBytes(cfstr, range, kCFStringEncodingMacRoman,
			 0, FALSE, (uint8_t *)str, len, &l);
    str[l] = '\0';
    return (l);
}

static void
monitor_eapol_change(SCDynamicStoreRef store, CFArrayRef changes, void * arg)
{
    int 		count;
    int 		i;

    count = CFArrayGetCount(changes);
    for (i = 0; i < count; i++) {
	CFStringRef 	key = CFArrayGetValueAtIndex(changes, i);
	CFStringRef	interface = NULL;
	char 		ifname[16];

	interface = EAPOLControlKeyCopyInterface(key);
	if (interface == NULL) {
	    continue;
	}
	cfstring_to_cstring(interface, ifname, sizeof(ifname));
	CFRelease(interface);
	timestamp_fprintf(stdout, "%s changed\n", ifname);
	get_eapol_interface_status(ifname);
	printf("\n");
    }
    return;
}

static int
S_monitor(int argc, char * argv[])
{
    const char *	ifname = NULL;
    SCDynamicStoreRef 	store = NULL;
    CFRunLoopSourceRef	rls = NULL;

    if (argc > 0) {
	ifname = argv[0];
	get_eapol_interface_status(ifname);
    }

    store = config_session_start(monitor_eapol_change, NULL, ifname);
    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRunLoopRun();

    /* not reached */
    return (0);
}

static int
S_state(int argc, char * argv[])
{
    return (get_eapol_interface_status(argv[0]));
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
    { "monitor", S_monitor, 0, "[ <interface_name> ]" },
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
