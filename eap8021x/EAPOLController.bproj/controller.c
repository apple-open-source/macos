/*
 * Copyright (c) 2002-2009 Apple Inc. All rights reserved.
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
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <sys/param.h>
#include <sys/types.h>
#include <pwd.h>
#include <pthread.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFRunLoop.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <TargetConditionals.h>

#include "mylog.h"
#include "myCFUtil.h"
#include "controller.h"
#include "server.h"
#include "ClientControlInterface.h"
#include "EAPOLControlTypes.h"

#ifndef kSCEntNetRefreshConfiguration
#define kSCEntNetRefreshConfiguration	CFSTR("RefreshConfiguration")
#endif kSCEntNetRefreshConfiguration

#ifndef kSCEntNetEAPOL
#define kSCEntNetEAPOL		CFSTR("EAPOL")
#endif kSCEntNetEAPOL

static SCDynamicStoreRef	S_store;
static char * 			S_eapolclient_path = NULL;

typedef struct eapolClient_s {
    LIST_ENTRY(eapolClient_s)	link;

    EAPOLControlState		state;

    if_name_t			if_name;
    CFStringRef			notification_key;
    CFStringRef			force_renew_key;
    struct {
	uid_t			uid;
	gid_t			gid;
    } owner;

    pid_t			pid;
    mach_port_t			notify_port;
    mach_port_t			bootstrap;
    CFMachPortRef		session_cfport;
    CFRunLoopSourceRef		session_rls;
    boolean_t			notification_sent;
    boolean_t			retry;
    boolean_t			user_input_provided;

    boolean_t			keep_it;
    boolean_t			console_user;	/* started by console user */
    EAPOLControlMode		mode;
    CFDictionaryRef		config_dict;
    CFDictionaryRef		user_input_dict;

    CFDictionaryRef		status_dict;
#if ! TARGET_OS_EMBEDDED
    CFDictionaryRef		loginwindow_config;
#endif /* ! TARGET_OS_EMBEDDED */
} eapolClient, *eapolClientRef;

static LIST_HEAD(clientHead, eapolClient_s)	S_head;
static struct clientHead * S_clientHead_p = &S_head;

#if TARGET_OS_EMBEDDED
static __inline__ boolean_t
is_console_user(uid_t check_uid)
{
    return (TRUE);
}

#else /* TARGET_OS_EMBEDDED */

static SCNetworkInterfaceRef
copy_interface_in_set(SCNetworkSetRef set, const char * ifname)
{
    int				count = 0;
    int				i;
    CFStringRef			ifname_cf = NULL;
    CFArrayRef			services;
    SCNetworkInterfaceRef	the_interface = NULL;

    services = SCNetworkSetCopyServices(set);
    if (services != NULL) {
	count = CFArrayGetCount(services);
	ifname_cf = CFStringCreateWithCString(NULL, ifname,
					      kCFStringEncodingASCII);
    }
    for (i = 0; i < count; i++) {
	CFStringRef		bsd_name;
	SCNetworkInterfaceRef	sc_if;
	SCNetworkServiceRef	service = CFArrayGetValueAtIndex(services, i);

	sc_if = SCNetworkServiceGetInterface(service);
	if (sc_if != NULL) {
	    bsd_name = SCNetworkInterfaceGetBSDName(sc_if);
	    if (bsd_name != NULL && CFEqual(bsd_name, ifname_cf)) {
		the_interface = sc_if;
		CFRetain(the_interface);
		break;
	    }
	}
    }
    if (ifname_cf != NULL) {
	CFRelease(ifname_cf);
    }
    if (services != NULL) {
	CFRelease(services);
    }
    return (the_interface);
}

static SCNetworkInterfaceRef
copy_if(const char * ifname)
{
    SCPreferencesRef		prefs;
    SCNetworkSetRef		set = NULL;
    SCNetworkInterfaceRef	sc_if = NULL;

    prefs = SCPreferencesCreate(NULL,
				CFSTR("EAPOLControlCopyLoginWindowProfile"),
				NULL);
    if (prefs == NULL) {
	goto failed;
    }
    set = SCNetworkSetCopyCurrent(prefs);
    if (set == NULL) {
	goto failed;
    }
    sc_if = copy_interface_in_set(set, ifname);
    if (sc_if == NULL) {
	goto failed;
    }

 failed:
    if (set != NULL) {
	CFRelease(set);
    }
    if (prefs != NULL) {
	CFRelease(prefs);
    }
    return (sc_if);
}

#define kEAPOLLoginWindow	CFSTR("EAPOL.LoginWindow")

/*
 * Function: copy_loginwindow_config
 * Purpose:
 *   Grabs the specified LoginWindow configuration for the specified
 *   interface using the SCNetworkConfiguration API's.
 */
static CFDictionaryRef
copy_loginwindow_config(const char * interface_name, CFStringRef unique_id)
{
    int				count;
    CFDictionaryRef		dict;
    int				i;
    CFDictionaryRef		ret_dict = NULL;
    SCNetworkInterfaceRef	sc_if;
    void * *			values;
    void *			values_static[10];
    int				values_static_count = sizeof(values_static) / sizeof(values_static[0]);

    sc_if = copy_if(interface_name);
    if (sc_if == NULL) {
	goto done;
    }
    dict = SCNetworkInterfaceGetExtendedConfiguration(sc_if, kEAPOLLoginWindow);
    if (isA_CFDictionary(dict) == NULL) {
	goto done;
    }
    count = CFDictionaryGetCount(dict);
    if (count == 0) {
	goto done;
    }

    /* get the list of profile dictionaries */
    if (count > values_static_count) {
	values = (void * *)malloc(sizeof(void *) * count);
	if (values == NULL) {
	    goto done;
	}
    }
    else {
	values = values_static;
    }
    CFDictionaryGetKeysAndValues(dict, NULL, (const void * *)values);

    /* walk the list of LoginWindow profiles to find a match */
    for (i = 0; i < count; i++) {
	CFDictionaryRef		this_dict = (CFDictionaryRef)values[i];
	CFStringRef		this_unique_id;
	
	if (isA_CFDictionary(this_dict) == NULL) {
	    continue;
	}
	this_unique_id = CFDictionaryGetValue(this_dict,
					      kEAPOLControlUniqueIdentifier);
	if (isA_CFString(this_unique_id) == NULL) {
	    continue;
	}
	if (CFEqual(this_unique_id, unique_id)) {
	    ret_dict = CFRetain(this_dict);
	    break;
	}
    }
    if (values != values_static) {
	free(values);
    }
    
 done:
    if (sc_if != NULL) {
	CFRelease(sc_if);
    }
    return (ret_dict);
}

static void
clear_loginwindow_config(eapolClientRef client)
{
    my_CFRelease(&client->loginwindow_config);
    return;
}

static void
set_loginwindow_config(eapolClientRef client)
{
    CFStringRef		unique_id;

    clear_loginwindow_config(client);
    if (client->config_dict != NULL) {
	unique_id = CFDictionaryGetValue(client->config_dict,
					 kEAPOLControlUniqueIdentifier);
	if (isA_CFString(unique_id) != NULL) {
	    client->loginwindow_config
		= copy_loginwindow_config(client->if_name, unique_id);
	}
    }
    return;
}

static uid_t
login_window_uid(void)
{
    static uid_t	login_window_uid = -1;

    /* look up the _securityagent user */
    if (login_window_uid == -1) {
	struct passwd *	pwd = getpwnam("_securityagent");
	if (pwd == NULL) {
	    my_log(LOG_NOTICE,
		   "EAPOLController: getpwnam(_securityagent) failed");
	    return (92);
	}
	login_window_uid = pwd->pw_uid;
    }
    return (login_window_uid);
}

static boolean_t
is_console_user(uid_t check_uid)
{
    uid_t	uid;
    CFStringRef user;

    user = SCDynamicStoreCopyConsoleUser(S_store, &uid, NULL);
    if (user == NULL) {
	return (FALSE);
    }
    CFRelease(user);
    return (check_uid == uid);
}
#endif /* TARGET_OS_EMBEDDED */

static int
eapolClientStop(eapolClientRef client);

static void
eapolClientSetState(eapolClientRef client, EAPOLControlState state);


static CFNumberRef
make_number(int val)
{
    return (CFNumberCreate(NULL, kCFNumberIntType, &val));
}

eapolClientRef
eapolClientLookupInterface(const char * if_name)
{
    eapolClientRef	scan;

    LIST_FOREACH(scan, S_clientHead_p, link) {
	if (strcmp(if_name, scan->if_name) == 0) {
	    return (scan);
	}
    }
    return (NULL);
}

eapolClientRef
eapolClientLookupProcess(pid_t pid)
{
    eapolClientRef	scan;

    LIST_FOREACH(scan, S_clientHead_p, link) {
	if (pid == scan->pid) {
	    return (scan);
	}
    }
    return (NULL);
}

eapolClientRef
eapolClientLookupSession(mach_port_t session_port)
{
    eapolClientRef	scan;

    LIST_FOREACH(scan, S_clientHead_p, link) {
	if (scan->session_cfport != NULL
	    && session_port == CFMachPortGetPort(scan->session_cfport)) {
	    return (scan);
	}
    }
    return (NULL);
}

eapolClientRef
eapolClientAdd(const char * if_name)
{
    eapolClientRef client;

    client = malloc(sizeof(*client));
    if (client == NULL) {
	return (NULL);
    }
    bzero(client, sizeof(*client));
    strlcpy(client->if_name, if_name, sizeof(client->if_name));
    client->pid = -1;
    LIST_INSERT_HEAD(S_clientHead_p, client, link);
    return (client);
}

void
eapolClientRemove(eapolClientRef client)
{
#if ! TARGET_OS_EMBEDDED
    clear_loginwindow_config(client);
#endif /* ! TARGET_OS_EMBEDDED */
    LIST_REMOVE(client, link);
    free(client);
    return;
}

static void
eapolClientInvalidate(eapolClientRef client)
{
    eapolClientSetState(client, kEAPOLControlStateIdle);
    client->pid = -1;
    client->owner.uid = 0;
    client->owner.gid = 0;
    client->keep_it = FALSE;
    client->mode = kEAPOLControlModeNone;
    client->retry = FALSE;
    client->notification_sent = FALSE;
    client->console_user = FALSE;
    client->user_input_provided = FALSE;
    my_CFRelease(&client->notification_key);
    my_CFRelease(&client->force_renew_key);
    if (client->notify_port != MACH_PORT_NULL) {
	(void)mach_port_deallocate(mach_task_self(), client->notify_port);
	client->notify_port = MACH_PORT_NULL;
    }
    if (client->bootstrap != MACH_PORT_NULL) {
	(void)mach_port_deallocate(mach_task_self(), client->bootstrap);
	client->bootstrap = MACH_PORT_NULL;
    }
    if (client->session_cfport != NULL) {
	CFMachPortInvalidate(client->session_cfport);
	my_CFRelease(&client->session_cfport);
    }
    if (client->session_rls != NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
			      client->session_rls, kCFRunLoopDefaultMode);
	my_CFRelease(&client->session_rls);
    }
    my_CFRelease(&client->config_dict);
    my_CFRelease(&client->user_input_dict);
    my_CFRelease(&client->status_dict);
    return;
}

static void
eapolClientNotify(eapolClientRef client)
{
    mach_msg_empty_send_t	msg;
    kern_return_t		status;

    if (client->notify_port == MACH_PORT_NULL) {
	return;
    }
    if (client->notification_sent == TRUE) {
	/* no need to send more than a single message */
	return;
    }
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_remote_port = client->notify_port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_id = 0;
    status = mach_msg(&msg.header,			/* msg */
		      MACH_SEND_MSG | MACH_SEND_TIMEOUT,/* options */
		      msg.header.msgh_size,		/* send_size */
		      0,				/* rcv_size */
		      MACH_PORT_NULL,			/* rcv_name */
		      0,				/* timeout */
		      MACH_PORT_NULL);			/* notify */
    if (status != KERN_SUCCESS) {
	my_log(LOG_NOTICE,  "eapolClientNotify: mach_msg(%s) failed: %s",
	       client->if_name, mach_error_string(status));
    }
    client->notification_sent = TRUE;
    return;
}

static CFStringRef
eapolClientNotificationKey(eapolClientRef client)
{
    CFStringRef		if_name_cf;

    if (client->notification_key == NULL) {
	if_name_cf = CFStringCreateWithCString(NULL, client->if_name, 
					       kCFStringEncodingASCII);
	client->notification_key
	    = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    if_name_cf,
							    kSCEntNetEAPOL);
	my_CFRelease(&if_name_cf);
    }
    return (client->notification_key);
}

static void
eapolClientSetState(eapolClientRef client, EAPOLControlState state)
{
    client->state = state;
    if (S_store == NULL) {
	return;
    }
    SCDynamicStoreNotifyValue(S_store, eapolClientNotificationKey(client));
    return;
}

static void
eapolClientPublishStatus(eapolClientRef client, CFDictionaryRef status_dict)

{
    CFRetain(status_dict);
    my_CFRelease(&client->status_dict);
    client->status_dict = status_dict;
    if (S_store != NULL) {
	SCDynamicStoreNotifyValue(S_store, eapolClientNotificationKey(client));
    }
    return;
}

static CFStringRef
eapolClientForceRenewKey(eapolClientRef client)
{
    CFStringRef		if_name_cf;

    if (client->force_renew_key == NULL) {
	if_name_cf = CFStringCreateWithCString(NULL, client->if_name, 
					       kCFStringEncodingASCII);
	client->force_renew_key
	    = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    if_name_cf,
							    kSCEntNetRefreshConfiguration);
	my_CFRelease(&if_name_cf);
    }
    return (client->force_renew_key);
}

static void
eapolClientForceRenew(eapolClientRef client)

{
    if (S_store == NULL) {
	return;
    }
    SCDynamicStoreNotifyValue(S_store, eapolClientForceRenewKey(client));
    return;
}

static void 
exec_callback(pid_t pid, int status, struct rusage * rusage, void * context)
{
    eapolClientRef	client;
    boolean_t		keep_it;
    
    client = eapolClientLookupProcess(pid);
    if (client == NULL) {
	return;
    }
    if (client->state != kEAPOLControlStateIdle) {
	my_log(LOG_NOTICE, 
	       "EAPOLController: eapolclient(%s) pid=%d exited with status %d",
	       client->if_name,  pid, status);
    }
    keep_it = client->keep_it;
    eapolClientInvalidate(client);
    if (keep_it == FALSE) {
	eapolClientRemove(client);
    }
    return;
}

static int
eapolClientStart(eapolClientRef client, uid_t uid, gid_t gid, 
		 CFDictionaryRef config_dict, mach_port_t bootstrap)
{
    char * 	argv[] = { S_eapolclient_path, 
			   "-i", 		/* 1 */
			   client->if_name,	/* 2 */
			   NULL,		/* 3 */
			   NULL,		/* 4 */
			   NULL,		/* 5 */
			   NULL,		/* 6 */
			   NULL,		/* 7 */
			   NULL };
    char	gid_str[32];
    boolean_t	on_console = FALSE;
    int		status = 0;
    char 	uid_str[32];

    if (bootstrap == MACH_PORT_NULL) {
	argv[3] = "-s";
    }
    else {
	snprintf(uid_str, sizeof(uid_str), "%u", uid);
	snprintf(gid_str, sizeof(gid_str), "%u", gid);
	argv[3] = "-u";
	argv[4] = uid_str;
	argv[5] = "-g";
	argv[6] = gid_str;
	on_console = is_console_user(uid);
	if (on_console == FALSE) {
	    argv[7] = "-n";
	}
    }
    client->pid = _SCDPluginExecCommand(exec_callback, NULL, 0, 0,
					S_eapolclient_path, argv);
    if (client->pid == -1) {
	status = errno;
    }
    else {
	if (bootstrap != MACH_PORT_NULL) {
	    client->owner.uid = uid;
	    client->owner.gid = gid;
	    client->console_user = on_console;
#if TARGET_OS_EMBEDDED
	    client->mode = kEAPOLControlModeUser;
	    client->bootstrap = bootstrap;
#else /* TARGET_OS_EMBEDDED */
	    if (on_console == FALSE
		&& uid == login_window_uid()) {
		client->mode = kEAPOLControlModeLoginWindow;
	    }
	    else {
		client->mode = kEAPOLControlModeUser;
		client->bootstrap = bootstrap;
	    }
#endif /* TARGET_OS_EMBEDDED */
	}
	else {
	    client->mode = kEAPOLControlModeSystem;
	}
	if (config_dict != NULL) {
	    client->config_dict = CFRetain(config_dict);
	}
	eapolClientSetState(client, kEAPOLControlStateStarting);
    }
    return (status);
}

static int
eapolClientUpdate(eapolClientRef client, CFDictionaryRef config_dict)
{
    int			status = 0;

    switch (client->state) {
    case kEAPOLControlStateStarting:
    case kEAPOLControlStateRunning:
	break;
    case kEAPOLControlStateIdle:
	status = ENOENT;
	goto done;
    default:
	status = EBUSY;
	goto done;
    }

    if (client->state == kEAPOLControlStateRunning) {
	/* tell the client to re-read */
	eapolClientNotify(client);
    }
    my_CFRelease(&client->config_dict);
    my_CFRelease(&client->user_input_dict);
    client->config_dict = CFRetain(config_dict);
    client->retry = FALSE;
    client->user_input_provided = FALSE;

 done:
    return (status);
}

static int
eapolClientProvideUserInput(eapolClientRef client, CFDictionaryRef user_input)
{
    int			status = 0;

    switch (client->state) {
    case kEAPOLControlStateRunning:
	break;
    case kEAPOLControlStateStarting:
	status = EINVAL;
	goto done;
    case kEAPOLControlStateIdle:
	status = ENOENT;
	goto done;
    default:
	status = EBUSY;
	goto done;
    }

    /* tell the client to re-read */
    eapolClientNotify(client);
    my_CFRelease(&client->user_input_dict);
    if (user_input != NULL) {
	client->user_input_dict = CFRetain(user_input);
    }
    client->retry = FALSE;
    client->user_input_provided = TRUE;

 done:
    return (status);
}

int 
ControllerGetState(if_name_t if_name, int * state)
{
    eapolClientRef 	client;
    int			status = 0;

    *state = kEAPOLControlStateIdle;
    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	status = ENOENT;
    }
    else {
	*state = client->state;
    }
    return (status);
}

int 
ControllerCopyStateAndStatus(if_name_t if_name, 
			     int * state,
			     CFDictionaryRef * status_dict)
{
    eapolClientRef 	client;
    int			status = 0;

    *state = kEAPOLControlStateIdle;
    *status_dict = NULL;
    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	status = ENOENT;
    }
    else {
	if (client->status_dict != NULL) {
	    *status_dict = CFRetain(client->status_dict);
	}
	*state = client->state;
    }
    return (status);
}

int
ControllerStart(if_name_t if_name, uid_t uid, gid_t gid,
		CFDictionaryRef config_dict, mach_port_t bootstrap)
{
    eapolClientRef 	client;
    int			status = 0;

    client = eapolClientLookupInterface(if_name);
    if (client != NULL) {
	if (client->state != kEAPOLControlStateIdle) {
	    if (client->state == kEAPOLControlStateRunning) {
		status = EEXIST;
	    }
	    else {
		status = EBUSY;
	    }
	    goto done;
	}
    }
    else {
	client = eapolClientAdd(if_name);
	if (client == NULL) {
	    status = ENOMEM;
	    goto done;
	}
    }
#if TARGET_OS_EMBEDDED
    /* automatically map all requests by root to the mobile user */
    if (uid == 0) {
	static gid_t	mobile_gid = -1;
	static uid_t	mobile_uid = -1;

	if (mobile_uid == -1) {
	    struct passwd *	pwd;

	    /* lookup the mobile user */
	    pwd = getpwnam("mobile");
	    if (pwd != NULL) {
		mobile_uid = pwd->pw_uid;
		mobile_gid = pwd->pw_gid;
	    }
	    else {
		my_log(LOG_NOTICE,
		       "EAPOLController: getpwnam(mobile) failed");
	    }
	}
	if (mobile_uid != -1) {
	    uid = mobile_uid;
	    if (mobile_gid != -1) {
		gid = mobile_gid;
	    }
	}
    }
#endif /* TARGET_OS_EMBEDDED */    
    status = eapolClientStart(client, uid, gid, config_dict, bootstrap);
 done:
    if (status != 0) {
	(void)mach_port_deallocate(mach_task_self(), bootstrap);
    }
    return (status);
}

static int
eapolClientStop(eapolClientRef client)
{
    int			status = 0;

    switch (client->state) {
    case kEAPOLControlStateRunning:
    case kEAPOLControlStateStarting:
	break;
    case kEAPOLControlStateIdle:
	status = ENOENT;
	goto done;
    default:
	status = EBUSY;
	goto done;
    }
    eapolClientSetState(client, kEAPOLControlStateStopping);
    my_CFRelease(&client->config_dict);

    /* send a message to stop it */
    eapolClientNotify(client);
    /* should set a timeout so that if it doesn't detach, we kill it XXX */
#if 0
    if (client->pid != -1 && client->pid != 0) {
	kill(client->pid, SIGTERM);
    }
#endif 0

 done:
    return (status);

}

#if ! TARGET_OS_EMBEDDED
static boolean_t
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef key, boolean_t def)
{
    CFBooleanRef 	b;
    boolean_t		ret = def;

    b = isA_CFBoolean(CFDictionaryGetValue(plist, key));
    if (b != NULL) {
	ret = CFBooleanGetValue(b);
    }
    return (ret);
}
#endif /* ! TARGET_OS_EMBEDDED */

int
ControllerStop(if_name_t if_name, uid_t uid, gid_t gid)
{
    eapolClientRef 	client;
    int			status = 0;

    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	status = ENOENT;
	goto done;
    }
#if TARGET_OS_EMBEDDED 
    if (uid != 0 && uid != client->owner.uid) {
	status = EPERM;
	goto done;
    }
#else /* TARGET_OS_EMBEDDED */
    if (uid != 0) {
	if (uid != client->owner.uid) {
	    if (client->mode == kEAPOLControlModeSystem
		&& S_get_plist_boolean(client->config_dict, CFSTR("AllowStop"), 
				       TRUE)
		&& (uid == login_window_uid() || is_console_user(uid))) {
		/* allow the change */
	    }
	    else {
		status = EPERM;
		goto done;
	    }
	}
	else if (client->mode == kEAPOLControlModeLoginWindow
		 && uid == login_window_uid()) {
	    /* LoginWindow mode is being turned off, clear the config */
	    clear_loginwindow_config(client);
	} 
    }

#endif /* TARGET_OS_EMBEDDED */
    status = eapolClientStop(client);
 done:
    return (status);
}


int
ControllerUpdate(if_name_t if_name, uid_t uid, gid_t gid,
		 CFDictionaryRef config_dict)
{
    eapolClientRef 	client;
    int			status = 0;

    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	status = ENOENT;
	goto done;
    }
    if (uid != 0 && uid != client->owner.uid) {
	status = EPERM;
	goto done;
    }
    status = eapolClientUpdate(client, config_dict);
 done:
    return (status);
}

int
ControllerProvideUserInput(if_name_t if_name, uid_t uid, gid_t gid,
			   CFDictionaryRef user_input)
{
    eapolClientRef 	client;
    int			status = 0;

    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	status = ENOENT;
	goto done;
    }
    if (uid != 0 && uid != client->owner.uid) {
	status = EPERM;
	goto done;
    }
    status = eapolClientProvideUserInput(client, user_input);
 done:
    return (status);
}

int
ControllerRetry(if_name_t if_name, uid_t uid, gid_t gid)
{
    eapolClientRef 	client;
    int			status = 0;

    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	status = ENOENT;
	goto done;
    }
    if (uid != 0 && uid != client->owner.uid) {
	status = EPERM;
	goto done;
    }
    switch (client->state) {
    case kEAPOLControlStateStarting:
	goto done;
    case kEAPOLControlStateRunning:
	break;
    case kEAPOLControlStateIdle:
	status = ENOENT;
	goto done;
    default:
	status = EBUSY;
	goto done;
    }
    /* tell the client to re-read */
    eapolClientNotify(client);
    client->retry = TRUE;

 done:
    return (status);

}

int
ControllerSetLogLevel(if_name_t if_name, uid_t uid, gid_t gid,
		      int32_t level)
{
    eapolClientRef 	client;
    int32_t		cur_level = -1;
    CFNumberRef		log_prop;
    int			status = 0;

    client = eapolClientLookupInterface(if_name);
    if (client == NULL || client->config_dict == NULL) {
	status = ENOENT;
	goto done;
    }
    if (uid != 0 && uid != client->owner.uid) {
	status = EPERM;
	goto done;
    }
    switch (client->state) {
    case kEAPOLControlStateStarting:
    case kEAPOLControlStateRunning:
	break;
    case kEAPOLControlStateIdle:
	status = ENOENT;
	goto done;
    default:
	status = EBUSY;
	goto done;
    }
    log_prop = CFDictionaryGetValue(client->config_dict, 
				    kEAPOLControlLogLevel);
    if (log_prop) {
	(void)CFNumberGetValue(log_prop, kCFNumberSInt32Type, &cur_level);
    }
    if (level < 0) {
	level = -1;
    }
    /* if the log level changed, update it and tell the client */
    if (cur_level != level) {
	int			count;
	CFMutableDictionaryRef	dict = NULL;

	count = CFDictionaryGetCount(client->config_dict);
	if (level >= 0) {
	    count++;
	}
	dict = CFDictionaryCreateMutableCopy(NULL, count,
					     client->config_dict);
	if (level >= 0) {
	    CFNumberRef		level_prop;

	    level_prop = make_number(level);
	    CFDictionarySetValue(dict, kEAPOLControlLogLevel, level_prop);
	    my_CFRelease(&level_prop);
	}
	else {
	    CFDictionaryRemoveValue(dict, kEAPOLControlLogLevel);
	}
	my_CFRelease(&client->config_dict);
	client->config_dict = dict;

	if (client->state == kEAPOLControlStateRunning) {
	    /* tell the client to re-read */
	    eapolClientNotify(client);
	}
    }
 done:
    return (status);

}

#if ! TARGET_OS_EMBEDDED

static CFDictionaryRef
system_eapol_copy(const char * if_name)
{
    CFDictionaryRef	dict;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@/%s/%@"),
				  kSCDynamicStoreDomainSetup,
				  kSCCompNetwork,
				  kSCCompInterface,
				  if_name,
				  kSCEntNetEAPOL);
    dict = SCDynamicStoreCopyValue(S_store, key);
    CFRelease(key);
    return (dict);
}

int
ControllerStartSystem(if_name_t if_name, uid_t uid, gid_t gid,
		      CFDictionaryRef options)
{
    eapolClientRef 	client;
    CFDictionaryRef	dict = NULL;
    int			status = 0;

    /* make sure that 802.1X isn't already running on the interface */
    client = eapolClientLookupInterface(if_name);
    if (client != NULL && client->state != kEAPOLControlStateIdle) {
	if (client->state == kEAPOLControlStateRunning) {
	    status = EEXIST;
	}
	else {
	    status = EBUSY;
	}
	goto done;
    }

    /* verify that non-root caller is either loginwindow or a logged-in user */
    if (uid != 0
	&& uid != login_window_uid() 
	&& is_console_user(uid) == FALSE) {
	status = EPERM;
	goto done;
    }

    /* check whether system mode is configured */
    dict = system_eapol_copy(if_name);
    if (dict == NULL) {
	status = ESRCH;
	goto done;
    }

    /* if there's no client entry yet, create it */
    if (client == NULL) {
	client = eapolClientAdd(if_name);
	if (client == NULL) {
	    status = ENOMEM;
	    goto done;
	}
    }
    /* start system mode over the specific interface */
    status = eapolClientStart(client, 0, 0, dict, MACH_PORT_NULL);

 done:
    my_CFRelease(&dict);
    return (status);
}

int 
ControllerCopyLoginWindowConfiguration(if_name_t if_name, 
				       CFDictionaryRef * config_data_p)
{
    eapolClientRef 	client;
    int			status = 0;

    *config_data_p = NULL;
    client = eapolClientLookupInterface(if_name);
    if (client == NULL
	|| client->loginwindow_config == NULL) {
	status = ENOENT;
    }
    else {
	*config_data_p = CFRetain(client->loginwindow_config);
    }
    return (status);
}
#endif /* ! TARGET_OS_EMBEDDED */

int
ControllerClientAttach(pid_t pid, if_name_t if_name,
		       mach_port_t notify_port,
		       mach_port_t * session_port,
		       CFDictionaryRef * control_dict,
		       mach_port_t * bootstrap)
{
    CFMutableDictionaryRef	dict;
    CFNumberRef			command_cf;
    eapolClientRef		client;
    int				result = 0;

    *session_port = MACH_PORT_NULL;
    *control_dict = NULL;
    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	result = ENOENT;
	goto failed;
    }
    if (pid != client->pid) {
	result = EPERM;
	goto failed;
    }
    if (client->session_cfport != NULL) {
	result = EEXIST;
	goto failed;
    }
    client->notify_port = notify_port;
    client->session_cfport 
	= CFMachPortCreate(NULL, server_handle_request, NULL, NULL);
    *session_port = CFMachPortGetPort(client->session_cfport);
    client->session_rls
	= CFMachPortCreateRunLoopSource(NULL, client->session_cfport, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
		       client->session_rls, kCFRunLoopDefaultMode);
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (client->state == kEAPOLControlStateStarting) {
	CFNumberRef			mode_cf;

	command_cf = make_number(kEAPOLClientControlCommandRun);
	if (client->config_dict != NULL) {
	    CFDictionarySetValue(dict, kEAPOLClientControlConfiguration,
				 client->config_dict);
	}
	mode_cf = make_number(client->mode);
	CFDictionarySetValue(dict, kEAPOLClientControlMode,
			     mode_cf);
	CFRelease(mode_cf);
    }
    else {
	command_cf = make_number(kEAPOLClientControlCommandStop);
    }
    CFDictionarySetValue(dict, kEAPOLClientControlCommand, command_cf);
    CFRelease(command_cf);
    *control_dict = dict;
    eapolClientSetState(client, kEAPOLControlStateRunning);
    client->keep_it = TRUE;
    *bootstrap = client->bootstrap;
#if ! TARGET_OS_EMBEDDED
    if (client->mode == kEAPOLControlModeLoginWindow) {
	set_loginwindow_config(client);
    }
#endif /* ! TARGET_OS_EMBEDDED */
    return (result);
 failed:
    (void)mach_port_deallocate(mach_task_self(), notify_port);
    return (result);

}

int
ControllerClientDetach(mach_port_t session_port)
{
    eapolClientRef	client;
    int			result = 0;

    client = eapolClientLookupSession(session_port);
    if (client == NULL) {
	result = EINVAL;
	goto failed;
    }
    my_log(LOG_DEBUG,  "EAPOLController: detaching port 0x%x", session_port);
    eapolClientInvalidate(client);
 failed:
    return (result);
}

int
ControllerClientGetConfig(mach_port_t session_port,
			  CFDictionaryRef * control_dict)
{
    eapolClientRef		client;
    CFNumberRef			command_cf = NULL;
    CFMutableDictionaryRef	dict = NULL;
    int				result = 0;

    *control_dict = NULL;
    client = eapolClientLookupSession(session_port);
    if (client == NULL) {
	result = EINVAL;
	goto failed;
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (client->state == kEAPOLControlStateRunning) {
	if (client->retry) {
	    command_cf = make_number(kEAPOLClientControlCommandRetry);
	    client->retry = FALSE;
	}
	else if (client->user_input_provided) {
	    command_cf
		= make_number(kEAPOLClientControlCommandTakeUserInput);
	    if (client->user_input_dict != NULL) {
		CFDictionarySetValue(dict, 
				     kEAPOLClientControlUserInput,
				     client->user_input_dict);
	    }
	    client->user_input_provided = FALSE;
	    my_CFRelease(&client->user_input_dict);
	}
	else {
	    command_cf = make_number(kEAPOLClientControlCommandRun);
	    if (client->config_dict != NULL) {
		CFDictionarySetValue(dict, kEAPOLClientControlConfiguration,
				     client->config_dict);
	    }
	}
    }
    else {
	command_cf = make_number(kEAPOLClientControlCommandStop);
    }
    CFDictionarySetValue(dict, kEAPOLClientControlCommand, command_cf);
    *control_dict = dict;
    client->notification_sent = FALSE;
    my_CFRelease(&command_cf);
 failed:
    return (result);
}

int
ControllerClientReportStatus(mach_port_t session_port,
			     CFDictionaryRef status_dict)
{
    eapolClientRef	client;
    int			result = 0;

    client = eapolClientLookupSession(session_port);
    if (client == NULL) {
	result = EINVAL;
	goto failed;
    }
    eapolClientPublishStatus(client, status_dict);
 failed:
    return (result);
}

int
ControllerClientForceRenew(mach_port_t session_port)
{
    eapolClientRef	client;
    int			result = 0;

    client = eapolClientLookupSession(session_port);
    if (client == NULL) {
	result = EINVAL;
	goto failed;
    }
    (void)eapolClientForceRenew(client);
 failed:
    return (result);
}


#if TARGET_OS_EMBEDDED
static SCDynamicStoreRef
dynamic_store_create(void)
{
    SCDynamicStoreRef		store;

    store = SCDynamicStoreCreate(NULL, CFSTR("EAPOLController"), NULL, NULL);
    if (store == NULL) {
	my_log(LOG_NOTICE, "EAPOLController: SCDynamicStoreCreate() failed, %s",
	       SCErrorString(SCError()));
    }
    return (store);
}

static void *
ControllerThread(void * arg)
{
    server_start();
    CFRunLoopRun();
    return (arg);
}

static void
ControllerBegin(void)
{
    pthread_attr_t	attr;
    int			ret;
    pthread_t		thread;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
	my_log(LOG_NOTICE, "EAPOLController: pthread_attr_init failed %d", ret);
	return;
    }
    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
	my_log(LOG_NOTICE, 
	       "EAPOLController: pthread_attr_setdetachstate failed %d", ret);
	goto done;
    }
    ret = pthread_create(&thread, &attr, ControllerThread, NULL);
    if (ret != 0) {
	my_log(LOG_NOTICE, "EAPOLController: pthread_create failed %d", ret);
	goto done;
    }
    
 done:
    (void)pthread_attr_destroy(&attr);
    return;
}

#else /* TARGET_OS_EMBEDDED */

static void
console_user_changed()
{
    eapolClientRef	scan;
    uid_t		uid = 0;
    CFStringRef		user;

    user = SCDynamicStoreCopyConsoleUser(S_store, &uid, NULL);
    LIST_FOREACH(scan, S_clientHead_p, link) {
	if (scan->console_user) {
	    if (user == NULL || scan->owner.uid != uid) {
		/* user logged out or fast-user switch */
		(void)eapolClientStop(scan);
		clear_loginwindow_config(scan);
	    }
	}
	else if (user == NULL) {
	    clear_loginwindow_config(scan);
	}
    }
    my_CFRelease(&user);
    return;
}

static Boolean
myCFStringArrayToCStringArray(CFArrayRef arr, char * buffer, int * buffer_size,
			      int * ret_count)
{
    int		count = CFArrayGetCount(arr);
    int 	i;
    char *	offset = NULL;	
    int		space;
    char * *	strlist = NULL;

    if (ret_count != NULL) {
	*ret_count = 0;
    }
    space = count * sizeof(char *);
    if (buffer != NULL) {
	if (*buffer_size < space) {
	    /* not enough space for even the pointer list */
	    return (FALSE);
	}
	strlist = (char * *)buffer;
	offset = buffer + space; /* the start of the 1st string */
    }
    for (i = 0; i < count; i++) {
	CFIndex		len = 0;
	CFStringRef	str;

	str = CFArrayGetValueAtIndex(arr, i);
	if (buffer != NULL) {
	    len = *buffer_size - space;
	    if (len <= 0) {
		return (FALSE);
	    }
	}
	CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)),
			 kCFStringEncodingASCII, '\0',
			 FALSE, (uint8_t *)offset, len - 1, &len);
	if (buffer != NULL) {
	    strlist[i] = offset;
	    offset[len] = '\0';
	    offset += len + 1;
	}
	space += len + 1;
    }
    *buffer_size = space;
    if (ret_count != NULL) {
	*ret_count = count;
    }
    return (TRUE);
}

static CFStringRef
mySCNetworkInterfacePathCopyInterfaceName(CFStringRef path)
{
    CFArrayRef          arr;
    CFStringRef         ifname = NULL;

    arr = CFStringCreateArrayBySeparatingStrings(NULL, path, CFSTR("/"));
    if (arr == NULL) {
        goto done;
    }
    /* "domain:/Network/Interface/<ifname>[/<entity>]" =>
     * {"domain:","Network","Interface","<ifname>"[,"<entity>"]}
     */
    if (CFArrayGetCount(arr) < 4) {
        goto done;
    }
    ifname = CFRetain(CFArrayGetValueAtIndex(arr, 3));
 done:
    if (arr != NULL) {
        CFRelease(arr);
    }
    return (ifname);
}

static const char * *
copy_interface_list(int * ret_iflist_count)
{
    CFDictionaryRef	dict;
    CFArrayRef		iflist_cf = NULL;
    const char * *	iflist = NULL;
    int			iflist_count = 0;
    int			iflist_size = 0;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    dict = SCDynamicStoreCopyValue(S_store, key);
    my_CFRelease(&key);

    if (isA_CFDictionary(dict) != NULL) {
	iflist_cf = CFDictionaryGetValue(dict, kSCPropNetInterfaces);
	iflist_cf = isA_CFArray(iflist_cf);
    }
    if (iflist_cf == NULL) {
	goto done;
    }
    if (myCFStringArrayToCStringArray(iflist_cf, NULL, &iflist_size, NULL)
	== FALSE) {
	goto done;
    }
    iflist = malloc(iflist_size);
    if (iflist == NULL) {
	goto done;
    }
    if (myCFStringArrayToCStringArray(iflist_cf, (void *)iflist, &iflist_size, 
				      &iflist_count) == FALSE) {
	free(iflist);
	iflist = NULL;
	goto done;
    }
 done:
    *ret_iflist_count = iflist_count;
    my_CFRelease(&dict);
    return (iflist);
}

#define INDEX_NONE		(-1)

static int
strlist_item_index(const char * * strlist, int strlist_count, const char * item)
{
    int		i;

    for (i = 0; i < strlist_count; i++) {
	if (strcmp(strlist[i], item) == 0) {
	    return (i);
	}
    }
    return (INDEX_NONE);
}


static void
handle_config_changed()
{
    const void * *	config_dicts = NULL;
    const char * *	config_iflist = NULL;
    int			count = 0;
    CFDictionaryRef	dict;
    int			i;
    const char * *	iflist = NULL;
    int			iflist_count = 0;
    CFStringRef		key;
    CFArrayRef		patterns;
    eapolClientRef	scan;
    int			status;

    iflist = copy_interface_list(&iflist_count);
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							kSCCompAnyRegex,
							kSCEntNetEAPOL);
    patterns = CFArrayCreate(NULL, (void *)&key, 1, &kCFTypeArrayCallBacks);
    my_CFRelease(&key);
    dict = SCDynamicStoreCopyMultiple(S_store, NULL, patterns);
    my_CFRelease(&patterns);

    if (dict != NULL) {
	count = CFDictionaryGetCount(dict);
    }

    /* get list of interface configurations in config_iflist and config_dicts */
    if (count != 0) {
	CFMutableArrayRef	iflist_cf = NULL;
	const void * *		keys;
	int 			size;

	keys = (const void * *)malloc(sizeof(*keys) * count);
	config_dicts = (const void * *)malloc(sizeof(*config_dicts) * count);
	CFDictionaryGetKeysAndValues(dict, keys, config_dicts);
	iflist_cf = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (i = 0; i < count; i++) {
	    CFStringRef		name;
	    
	    name = mySCNetworkInterfacePathCopyInterfaceName(keys[i]);
	    CFArrayAppendValue(iflist_cf, name);
	    my_CFRelease(&name);
	}
	if (myCFStringArrayToCStringArray(iflist_cf, NULL, &size,
					  NULL) == FALSE) {
	    my_log(LOG_NOTICE, 
		   "EAPOLController: config_iflist failed to calculate size");
	    count = 0;
	}
	else {
	    config_iflist = malloc(size);
	    if (myCFStringArrayToCStringArray(iflist_cf, (void *)config_iflist,
					      &size, NULL) == FALSE) {
		free(config_iflist);
		config_iflist = NULL;
		count = 0;
	    }
	}
	free(keys);
	my_CFRelease(&iflist_cf);
    }

    /* change existing interface configurations */
    LIST_FOREACH(scan, S_clientHead_p, link) {
	if (strlist_item_index(iflist, iflist_count, scan->if_name)
	    == INDEX_NONE) {
	    /* interface doesn't exist, skip it */
	    continue;
	}
	i = strlist_item_index(config_iflist, count, scan->if_name);
	if (scan->mode == kEAPOLControlModeSystem) {
	    if (i == INDEX_NONE) {
		status = eapolClientStop(scan);
		if (status != 0) {
		    my_log(LOG_NOTICE, "EAPOLController handle_config_changed:"
			   " eapolClientStop (%s) failed %d", 
			   scan->if_name, status);
		}
	    }
	    else {
		if (CFEqual(config_dicts[i], scan->config_dict) == FALSE) {
		    status = eapolClientUpdate(scan, config_dicts[i]);
		    if (status != 0) {
			my_log(LOG_NOTICE, 
			       "EAPOLController handle_config_changed: "
			       "eapolClientUpdate (%s) failed %d",
			       scan->if_name, status);
		    }
		}
	    }
	}
	else if (i != INDEX_NONE) {
	    if (scan->state == kEAPOLControlStateIdle) {
		status = eapolClientStart(scan, 0, 0, 
					  config_dicts[i], MACH_PORT_NULL);
		if (status != 0) {
		    my_log(LOG_NOTICE, 
			   "EAPOLController handle_config_changed:"
			   " eapolClientStart (%s) failed %d",
			   scan->if_name, status);
		}
	    }
	}
    }

    /* start any that are missing */
    for (i = 0; i < count; i++) {
	eapolClientRef		client;

	client = eapolClientLookupInterface(config_iflist[i]);
	if (client == NULL) {
	    client = eapolClientAdd(config_iflist[i]);
	    if (client == NULL) {
		my_log(LOG_NOTICE, 
		       "EAPOLController handle_config_changed:"
		       " eapolClientAdd (%s) failed", config_iflist[i]);
	    }
	    else {
		status = eapolClientStart(client, 0, 0, 
					  config_dicts[i], MACH_PORT_NULL);
		if (status != 0) {
		    my_log(LOG_NOTICE, 
			   "EAPOLController handle_config_changed:"
			   " eapolClientStart (%s) failed %d",
			   scan->if_name, status);
		}
	    }
	}
    }

    if (iflist != NULL) {
	free(iflist);
    }
    if (config_iflist != NULL) {
	free(config_iflist);
    }
    if (config_dicts != NULL) {
	free(config_dicts);
    }
    my_CFRelease(&dict);
    return;
}

static void
eapol_handle_change(SCDynamicStoreRef store, CFArrayRef changes, void * arg)
{
    boolean_t		config_changed = FALSE;
    CFStringRef		console_user_key;
    CFIndex		count;
    CFIndex		i;
    boolean_t		iflist_changed = FALSE;
    boolean_t		user_changed = FALSE;

    console_user_key = SCDynamicStoreKeyCreateConsoleUser(NULL);
    count = CFArrayGetCount(changes);
    if (count == 0) {
	goto done;
    }
    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);

	if (CFEqual(cache_key, console_user_key)) {
	    user_changed = TRUE;
	}
        else if (CFStringHasPrefix(cache_key, kSCDynamicStoreDomainSetup)) {
	    config_changed = TRUE;
	}
	else if (CFStringHasSuffix(cache_key, kSCCompInterface)) {
	    /* list of interfaces changed */
	    iflist_changed = TRUE;
	}
    }

    if (iflist_changed || config_changed) {
	handle_config_changed();
    }
    if (user_changed) {
	console_user_changed();
    }

 done:
    my_CFRelease(&console_user_key);
    return;
}

static SCDynamicStoreRef
dynamic_store_create(void)
{
    CFMutableArrayRef		keys = NULL;
    CFStringRef			key;
    CFRunLoopSourceRef		rls;
    CFArrayRef			patterns = NULL;
    SCDynamicStoreRef		store;
    SCDynamicStoreContext	context;

    bzero(&context, sizeof(context));
    store = SCDynamicStoreCreate(NULL, CFSTR("EAPOLController"), 
				 eapol_handle_change, &context);
    if (store == NULL) {
	my_log(LOG_NOTICE, "EAPOLController: SCDynamicStoreCreate() failed, %s",
	       SCErrorString(SCError()));
	return (NULL);
    }
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* console user */
    key = SCDynamicStoreKeyCreateConsoleUser(NULL);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);

    /* list of interfaces */
    key = SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);

    /* requested interface configurations */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							kSCCompAnyRegex,
							kSCEntNetEAPOL);
    patterns = CFArrayCreate(NULL, (void *)&key, 1, &kCFTypeArrayCallBacks);
    my_CFRelease(&key);
    SCDynamicStoreSetNotificationKeys(store, keys, patterns);
    my_CFRelease(&keys);
    my_CFRelease(&patterns);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    my_CFRelease(&rls);
    return (store);
}

static void
ControllerBegin(void)
{
    server_start();
    handle_config_changed();
    return;
}

#endif /* TARGET_OS_EMBEDDED */

/*
 * configd plugin-specific routines:
 */
void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    Boolean		ok;
    uint8_t		path[MAXPATHLEN];
    CFURLRef		url;

    if (server_active()) {
	my_log(LOG_NOTICE, "ipconfig server already active");
	return;
    }
    /* get a path to eapolclient */
    url = CFBundleCopyResourceURL(bundle, CFSTR("eapolclient"), NULL, NULL);
    if (url == NULL) {
	my_log(LOG_NOTICE, 
	       "EAPOLController: failed to get URL for eapolclient");
	return;
    }
    ok = CFURLGetFileSystemRepresentation(url, TRUE, path, sizeof(path));
    CFRelease(url);
    if (ok == FALSE) {
	my_log(LOG_NOTICE, 
	       "EAPOLController: failed to get path for eapolclient");
	return;
    }
    S_eapolclient_path = strdup((const char *)path);

    /* register the EAPOLController server port */
    server_register();
    return;
}

void
start(const char *bundleName, const char *bundleDir)
{
    if (S_eapolclient_path == NULL) {
	return;
    }
    LIST_INIT(S_clientHead_p);
    S_store = dynamic_store_create();
    return;
}

void
prime()
{
    if (S_eapolclient_path == NULL) {
	return;
    }
    ControllerBegin();
    return;
}
