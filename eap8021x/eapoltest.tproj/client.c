/*
 * Copyright (c) 2002-2014 Apple Inc. All rights reserved.
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
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include "EAPOLControl.h"
#include "EAPOLControlPrivate.h"
#include "EAPOLControlPrefs.h"
#include "myCFUtil.h"
#include <TargetConditionals.h>

typedef int func_t(int argc, char * argv[]);
typedef func_t * funcptr_t;

static char * 	progname = NULL;
static int 	S_command_index;

static void	command_usage();
static void	usage();

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

static void
dump_plist(FILE * f, CFTypeRef p)
{
    CFDataRef	data;

    data = CFPropertyListCreateData(NULL, p,
				    kCFPropertyListXMLFormat_v1_0,
				    0, NULL);
    if (data == NULL) {
	return;
    }
    fwrite(CFDataGetBytePtr(data), CFDataGetLength(data), 1, f);
    CFRelease(data);
    return;
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
	    fprintf(stdout, "Status dict:\n");
	    dump_plist(stdout, dict);
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
    (void)CFStringGetBytes(cfstr, range, kCFStringEncodingMacRoman,
			   0, FALSE, (uint8_t *)str, len, &l);
    str[l] = '\0';
    return ((int)l);
}

static void
monitor_eapol_change(SCDynamicStoreRef store, CFArrayRef changes, void * arg)
{
    CFIndex		count;
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
    CFRelease(rls);
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

#if ! TARGET_OS_EMBEDDED
static int
S_start_system(int argc, char * argv[])
{
    CFDictionaryRef	dict = NULL;
    int 		result;

    if (argc > 1) {
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
    }
    result = EAPOLControlStartSystem(argv[0], dict);
    fprintf(stderr, "EAPOLControlStartSystem returned %d\n", result);
    my_CFRelease(&dict);
    return (result);
}
#endif /* ! TARGET_OS_EMBEDDED */

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
S_input(int argc, char * argv[])
{
    CFDictionaryRef	dict = NULL;
    int 		result;

    if (argc > 1) {
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
    }
    result = EAPOLControlProvideUserInput(argv[0], dict);
    fprintf(stderr, "EAPOLControlProvideUserInput returned %d\n", result);
    my_CFRelease(&dict);
    return (result);
}

static int
S_set_verbose(int argc, char * argv[])
{
    uint32_t		log_flags = 0;
    int 		result;

    if (strcasecmp(argv[0], "on") == 0) {
	log_flags = -1;
    }
    else if (strcasecmp(argv[0], "off") == 0) {
	log_flags = 0;
    }
    else {
	command_usage();
    }
    if (EAPOLControlPrefsSetLogFlags(log_flags) == FALSE) {
	fprintf(stderr, "Failed to set verbose %s\n",
		log_flags != 0 ? "on" : "off");
	result = 1;
    }
    else {
	result = 0;
    }
    return (result);
}

#if ! TARGET_OS_EMBEDDED
static int
S_loginwindow_config(int argc, char * argv[])
{
    CFDictionaryRef	dict = NULL;
    char *		ifname = argv[0];
    int 		result;

    result = EAPOLControlCopyLoginWindowConfiguration(ifname, &dict);
    if (result == 0) {
	fprintf(stdout,
		"EAPOLControlCopyLoginWindowConfiguration(%s):\n", ifname);
	if (dict != NULL) {
	    dump_plist(stdout, dict);
	    CFRelease(dict);
	}
    }
    else {
	fprintf(stderr,
		"EAPOLControlCopyLoginWindowConfiguration(%s) returned %d\n",
		ifname, result);
    }
    return (result);
}

static int
S_set_user_autoconnect(int argc, char * argv[])
{
    Boolean		enable = FALSE;
    const char * 	enable_str = argv[0];
    int 		result;

    if (strcasecmp(enable_str, "on") == 0) {
	enable = TRUE;
    }
    else if (strcasecmp(enable_str, "off") == 0) {
	enable = FALSE;
    }
    else {
	command_usage();
    }
    EAPOLControlSetUserAutoConnectEnabled(enable);
    return (0);
}

static int
S_get_user_autoconnect(int argc, char * argv[])
{
    Boolean		enable;

    enable = EAPOLControlIsUserAutoConnectEnabled();
    printf("%s\n", enable ? "on" : "off");
    return (0);
}

static int
S_did_user_cancel(int argc, char * argv[])
{
    boolean_t		did_cancel;

    did_cancel = EAPOLControlDidUserCancel(argv[0]);
    printf("%s\n", did_cancel ? "true" : "false");
    return (0);
}

static int
S_show_autodetect_info(int argc, char * argv[])
{
    CFDictionaryRef	info;
    int			result;

    result = EAPOLControlCopyAutoDetectInformation(&info);
    if (info != NULL) {
	CFShow(info);
	CFRelease(info);
    }
    return (result);
}

#endif /* ! TARGET_OS_EMBEDDED */

static int
S_wait_for_state(const char * ifname,
		 SCDynamicStoreRef store, EAPOLControlState desired_state,
		 EAPOLControlState ok_state)
{
    int 			result;

    while (1) {
	EAPOLControlState 	state;
	CFDictionaryRef		status_dict = NULL;

	result = EAPOLControlCopyStateAndStatus(ifname, &state, &status_dict);
	if (result != 0) {
	    fprintf(stderr,
		    "EAPOLControlCopyStateAndStatus(%s) returned %d (%s)\n",
		    ifname, result, strerror(result));
	    break;
	}
	my_CFRelease(&status_dict);
	if (state == desired_state) {
	    break;
	}
	if (state != ok_state) {
	    fprintf(stderr,
		    "EAPOLControlState on %s is %d (!= %d)\n",
		    ifname, state, ok_state);
	    result = EINVAL;
	    break;
	}
	if (SCDynamicStoreNotifyWait(store) == FALSE) {
	    fprintf(stderr, "SCDynamicStoreNotifyWait failed\n");
	    result = EINVAL;
	    break;
	}
    }
    return (result);
}

static int
S_stress_start(int argc, char * argv[])
{
    CFDictionaryRef	dict = NULL;
    int			i;
    char *		ifname = argv[0];
    int 		result;
    SCDynamicStoreRef	store;

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
    store = config_session_start(NULL, NULL, ifname);
    if (store == NULL) {
	CFRelease(dict);
	return (EINVAL);
    }

    for (i = 0; TRUE; i++) {
	result = EAPOLControlStart(ifname, dict);
	if (result != 0) {
	    fprintf(stderr, "EAPOLControlStart(%s) returned %d (%s)\n",
		    ifname, result, strerror(result));
	    break;
	}
	result = S_wait_for_state(ifname, store, 
				  kEAPOLControlStateRunning,
				  kEAPOLControlStateStarting);
	if (result != 0) {
	    fprintf(stderr, "Waiting for Running failed\n");
	    break;
	}
	result = EAPOLControlStop(ifname);
	if (result != 0) {
	    fprintf(stderr, "EAPOLControlStop(%s) returned %d (%s)\n",
		    ifname, result, strerror(result));
	    break;
	}
	(void)S_wait_for_state(ifname, store, 
			       kEAPOLControlStateIdle,
			       kEAPOLControlStateStopping);
    }
    fprintf(stderr, "Failed at iteration %d\n", i + 1);
    my_CFRelease(&dict);
    return (result);
}

#include <EAP8021X/EAPCertificateUtil.h>

static CFStringRef
identity_copy_username(SecIdentityRef identity)
{
    SecCertificateRef 	cert;
    OSStatus		status;
    CFStringRef		username;

    status = SecIdentityCopyCertificate(identity, &cert);
    if (status != noErr) {
	fprintf(stderr, "SecIdentityCopyCertificate failed %ld\n",
		(long)status);
	return (NULL);
    }
    username = EAPSecCertificateCopyUserNameString(cert);
    CFRelease(cert);
    return (username);
}

static int
S_show_identities(int argc, char * argv[])
{
    CFIndex			count;
    int				i;
    CFArrayRef			list;
    OSStatus			status;

    status = EAPSecIdentityListCreate(&list);
    if (status != noErr) {
	fprintf(stderr, "EAPSecIdentityListCreate returned %ld\n",
		(long)status);
	return (-1);
    }
    count = CFArrayGetCount(list);
    printf("Number of identities: %d\n", (int)count);
    for (i = 0; i < count; i++) {
	EAPSecIdentityHandleRef	handle;
	SecIdentityRef 		identity;
	CFStringRef		username;

	identity = (SecIdentityRef)CFArrayGetValueAtIndex(list, i);
	username = identity_copy_username(identity);
	SCPrint(TRUE, stdout, CFSTR("\n%d. '%@'\n"), i + 1, username);
	CFRelease(username);
	handle = EAPSecIdentityHandleCreate(identity);
	dump_plist(stdout, handle);
	CFRelease(handle);

    }
    CFRelease(list);
    return (0);
}

typedef struct {
    char *	command;
    funcptr_t	func;
    int		argc;
    char *	usage;
} commandInfo, *commandInfoRef;

static commandInfo commands[] = {
    { "state", S_state, 1, "<interface_name>" },
    { "start", S_start, 2, "<interface_name> <config_file>" },
    { "stop", S_stop, 1, "<interface_name>" },
    { "retry", S_retry, 1, "<interface_name>" },
    { "update", S_update, 2, "<interface_name> <config_file>" },
    { "input", S_input, 1, "<interface_name> [ <config_file> ]" },
    { "monitor", S_monitor, 0, "[ <interface_name> ]" },
    { "set_verbose", S_set_verbose, 1, "( on | off )" },
    { "stress_start", S_stress_start, 2, "<interface_name> <config_file>"  },
    { "show_identities", S_show_identities, 0 },
#if ! TARGET_OS_EMBEDDED
    { "start_system", S_start_system, 1, "<interface_name> [ <config_file> ]"},
    { "loginwindow_config", S_loginwindow_config, 1, "<interface_name>" },
    { "auto_detect_info", S_show_autodetect_info, 0, NULL },
    { "set_user_autoconnect", S_set_user_autoconnect, 1, "( on | off )" },
    { "get_user_autoconnect", S_get_user_autoconnect, 0, "" },
    { "did_user_cancel", S_did_user_cancel, 1, "<interface_name>" },
#endif /* ! TARGET_OS_EMBEDDED */
    { NULL, NULL, 0, NULL },
};

static void
usage(void)
{
    commandInfoRef	cmd;
    int 		i;

    fprintf(stderr, "usage: %s <command> <args>\n", progname);
    fprintf(stderr, "where <command> is one of ");
    for (i = 0, cmd = commands; cmd->command; i++, cmd++) {
	fprintf(stderr, "%s%s",  i == 0 ? "" : ", ", cmd->command);
    }
    fprintf(stderr, "\n");
    exit(1);
}

static void
command_usage(void)
{
    commandInfoRef	cmd = commands + S_command_index;

    fprintf(stderr, "usage: %s %s\n", 
	    cmd->command, cmd->usage ? cmd->usage : "");
    exit(1);
}

static funcptr_t
S_lookup_func(char * cmd, int argc)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcmp(cmd, commands[i].command) == 0) {
	    S_command_index = i;
	    if (argc < commands[i].argc) {
		command_usage();
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
    if (func == NULL) {
	usage();
    }
    else {
	argv++; argc--;
	exit((*func)(argc, argv));
    }
    exit(0);
}
