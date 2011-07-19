/*
 * Copyright (c) 2002-2011 Apple Inc. All rights reserved.
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
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <string.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <sys/param.h>
#include <sys/types.h>
#include <fcntl.h>
#include <paths.h>
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
#include <EAP8021X/myCFUtil.h>
#include "controller.h"
#include "server.h"
#if ! TARGET_OS_EMBEDDED
#include <CoreFoundation/CFSocket.h>
#include "EAPOLClientConfiguration.h"
#include "EAPOLClientConfigurationPrivate.h"
#include "EAPOLControlPrivate.h"
#include "EAPOLControlTypesPrivate.h"
#endif /* ! TARGET_OS_EMBEDDED */
#include "ClientControlInterface.h"
#include "EAPOLControlTypes.h"
#include "EAPClientProperties.h"
#include "eapol_socket.h"

#ifndef kSCEntNetRefreshConfiguration
#define kSCEntNetRefreshConfiguration	CFSTR("RefreshConfiguration")
#endif /* kSCEntNetRefreshConfiguration */

#ifndef kSCEntNetEAPOL
#define kSCEntNetEAPOL			CFSTR("EAPOL")
#endif /* kSCEntNetEAPOL */

#define kSystemModeManagedExternally	CFSTR("_SystemModeManagedExternally")

static SCDynamicStoreRef	S_store;
static char * 			S_eapolclient_path = NULL;

struct eapolClient_s;
#define LIST_HEAD_clientHead 	LIST_HEAD(clientHead, eapolClient_s)
static LIST_HEAD_clientHead	S_head;
static struct clientHead * S_clientHead_p = &S_head;

#define LIST_ENTRY_eapolClient_s	LIST_ENTRY(eapolClient_s)

typedef struct eapolClient_s {
    LIST_ENTRY_eapolClient_s	link;
    EAPOLControlState		state;

    if_name_t			if_name;
    CFStringRef 		if_name_cf;
    CFStringRef			notification_key;
    CFStringRef			force_renew_key;
    struct {
	uid_t			uid;
	gid_t			gid;
    } owner;

    pid_t			pid;
    mach_port_t			notify_port;
    mach_port_t			bootstrap;
    mach_port_t			au_session;
    CFMachPortRef		session_cfport;
    boolean_t			notification_sent;
    boolean_t			retry;
    boolean_t			user_input_provided;

    boolean_t			console_user;	/* started by console user */
    EAPOLControlMode		mode;
    CFDictionaryRef		config_dict;
    CFDictionaryRef		user_input_dict;

    CFDictionaryRef		status_dict;
#if ! TARGET_OS_EMBEDDED
    CFDictionaryRef		loginwindow_config;
    CFSocketRef			eapol_sock;
    int				eapol_fd;
    bool			packet_received;
    uint64_t			packet_received_time;
    struct ether_addr		authenticator_mac;
    bool			user_cancelled;	
#endif /* ! TARGET_OS_EMBEDDED */
} eapolClient, *eapolClientRef;

#if TARGET_OS_EMBEDDED
static __inline__ boolean_t
is_console_user(uid_t check_uid)
{
    return (TRUE);
}

#else /* TARGET_OS_EMBEDDED */

static void
clear_loginwindow_config(eapolClientRef client)
{
    my_CFRelease(&client->loginwindow_config);
    return;
}

static void
set_loginwindow_config(eapolClientRef client)
{
    CFDictionaryRef	itemID_dict;

    clear_loginwindow_config(client);
    if (client->config_dict == NULL) {
	return;
    }
    /*
     * If there's an EAPOLClientItemID dictionary, save a dictionary containing
     * just that property: don't save the whole dictionary because it contains
     * the name/password.
     */
    itemID_dict = CFDictionaryGetValue(client->config_dict,
				       kEAPOLControlClientItemID);
    if (isA_CFDictionary(itemID_dict) != NULL) {
	CFStringRef	key;

	key = kEAPOLControlClientItemID;
	client->loginwindow_config
	    = CFDictionaryCreate(NULL,
				 (const void * *)&key,
				 (const void * *)&itemID_dict, 1,
				 &kCFTypeDictionaryKeyCallBacks,
				 &kCFTypeDictionaryValueCallBacks);
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

static CFDictionaryRef
S_profile_copy_itemID_dict(EAPOLClientProfileRef profile)
{
    CFStringRef			key;
    CFDictionaryRef		dict;
    EAPOLClientItemIDRef	itemID;
    CFDictionaryRef		itemID_dict;

    itemID = EAPOLClientItemIDCreateWithProfile(profile);
    itemID_dict = EAPOLClientItemIDCopyDictionary(itemID);
    CFRelease(itemID);
    key = kEAPOLControlClientItemID;
    dict = CFDictionaryCreate(NULL,
			      (const void * *)&key,
			      (const void * *)&itemID_dict, 1,
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    CFRelease(itemID_dict);
    return (dict);
}
#endif /* TARGET_OS_EMBEDDED */

#if 0
#endif 0

static int
get_ifm_type(const char * name)
{
    int			i;
    struct ifmediareq	ifm;
    int			media_static[20];
    int			media_static_count = sizeof(media_static) / sizeof(media_static[0]);
    int			s;
    int			ifm_type = 0;
    bool		supports_full_duplex = FALSE;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	perror("socket");
	goto done;
    }
    bzero(&ifm, sizeof(ifm));
    strlcpy(ifm.ifm_name, name, sizeof(ifm.ifm_name));
    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifm) < 0) {
	goto done;
    }
    ifm_type = IFM_TYPE(ifm.ifm_current);
    if (ifm_type != IFM_ETHER) {
	goto done;
    }
    if (ifm.ifm_count == 0) {
	goto done;
    }
    if (ifm.ifm_count > media_static_count) {
	ifm.ifm_ulist = (int *)malloc(ifm.ifm_count * sizeof(int));
    }
    else {
	ifm.ifm_ulist = media_static;
    }
    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifm) == -1) {
	goto done;
    }
    if (ifm.ifm_count == 1
	&& IFM_SUBTYPE(ifm.ifm_ulist[0]) == IFM_AUTO) {
	/* only support autoselect, not really ethernet */
	goto done;
    }
    for (i = 0; i < ifm.ifm_count; i++) {
	if ((ifm.ifm_ulist[i] & IFM_FDX) != 0) {
	    supports_full_duplex = TRUE;
	    break;
	}
    }

 done:
    if (s >= 0) {
	close(s);
    }
    if (ifm_type == IFM_ETHER && supports_full_duplex == FALSE) {
	/* not really ethernet */
	ifm_type = 0;
    }
    return (ifm_type);
}


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
eapolClientLookupInterfaceCF(CFStringRef if_name_cf)
{
    eapolClientRef	scan;

    LIST_FOREACH(scan, S_clientHead_p, link) {
	if (CFEqual(if_name_cf, scan->if_name_cf)) {
	    return (scan);
	}
    }
    return (NULL);
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
    client->if_name_cf = CFStringCreateWithCString(NULL, client->if_name, 
						   kCFStringEncodingASCII);
    client->pid = -1;
#if ! TARGET_OS_EMBEDDED
    client->eapol_fd = -1;
#endif /* ! TARGET_OS_EMBEDDED */
    LIST_INSERT_HEAD(S_clientHead_p, client, link);
    return (client);
}

void
eapolClientRemove(eapolClientRef client)
{
#if ! TARGET_OS_EMBEDDED
    clear_loginwindow_config(client);
#endif /* ! TARGET_OS_EMBEDDED */
    my_CFRelease(&client->if_name_cf);
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
    if (client->au_session != MACH_PORT_NULL) {
	(void)mach_port_deallocate(mach_task_self(), client->au_session);
	client->au_session = MACH_PORT_NULL;
    }
    if (client->session_cfport != NULL) {
	CFMachPortInvalidate(client->session_cfport);
	my_CFRelease(&client->session_cfport);
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
    if (client->notification_key == NULL) {
	client->notification_key
	    = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    client->if_name_cf,
							    kSCEntNetEAPOL);
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
    if (client->force_renew_key == NULL) {
	client->force_renew_key
	    = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    client->if_name_cf,
							    kSCEntNetRefreshConfiguration);
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


#if TARGET_OS_EMBEDDED
static void
eapolClientExited(eapolClientRef client)
{
    return;
}
#else /* TARGET_OS_EMBEDDED */

/**
 ** 802.1X socket monitoring routines
 **/

#include "EAP.h"
#include "EAPOLUtil.h"

static void
handle_config_changed(boolean_t check_system_mode);

#define RECV_SIZE	1600

static void
monitoring_callback(CFSocketRef s, CFSocketCallBackType type, 
		    CFDataRef address, const void * data, void * info)
{
    uint32_t			buf[RECV_SIZE / sizeof(uint32_t)]; /* force alignment */
    eapolClientRef		client = (eapolClientRef)info;
    uint64_t			current_time;
    struct ether_header *	eh_p = (struct ether_header *)buf;
    EAPOLPacketRef		eapol_p;
    int				n;
    EAPRequestPacketRef 	req_p;

    n = recv(client->eapol_fd, buf, sizeof(buf), 0);
    if (n < sizeof(*eh_p)) {
	if (n < 0) {
	    my_log(LOG_NOTICE, "EAPOLController: monitor %s recv failed %s",
		   client->if_name,
		   strerror(errno));
	}
	return;
    }
    eapol_p = (EAPOLPacketRef)(eh_p + 1);
    if (EAPOLPacketValid(eapol_p, n - sizeof(*eh_p), NULL) == FALSE) {
	/* bad packet */
	return;
    }
    req_p = (EAPRequestPacketRef)eapol_p->body;
    if (eapol_p->packet_type != kEAPOLPacketTypeEAPPacket
	|| req_p->code != kEAPCodeRequest
	|| req_p->type != kEAPTypeIdentity) {
	/* only EAP Request Identity packets can trigger */
	return;
    }

    /* grab the current time (in seconds) */
    current_time = (uint64_t)CFAbsoluteTimeGetCurrent();

    /* throttle notifications to no more than once per second */
    if (client->packet_received == FALSE
	|| client->packet_received_time != current_time) {
	bcopy(eh_p->ether_shost, &client->authenticator_mac,
	      sizeof(client->authenticator_mac));
	client->packet_received = TRUE;
	client->packet_received_time = current_time;
	syslog(LOG_DEBUG, "EAPOLController: %s requires 802.1X",
	       client->if_name);
	if (S_store != NULL) {
	    SCDynamicStoreNotifyValue(S_store, 
				      kEAPOLControlAutoDetectInformationNotifyKey);
	}
    }
    return;
}

static void
eapolClientStopMonitoring(eapolClientRef client)
{
    if (client->eapol_fd == -1) {
	return;
    }
    if (client->eapol_sock != NULL) {
	/* remove one socket reference, close the file descriptor */
	CFSocketInvalidate(client->eapol_sock);

	/* release the socket */
	my_CFRelease(&client->eapol_sock);
    }
    else {
	close(client->eapol_fd);
    }
    client->eapol_fd = -1;
    client->packet_received = FALSE;
    syslog(LOG_DEBUG, "EAPOLController: no longer monitoring %s",
	   client->if_name);
    return;
}

static void
eapolClientStartMonitoring(eapolClientRef client)
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    CFRunLoopSourceRef	rls;

    if (client->eapol_fd != -1) {
	syslog(LOG_DEBUG, "EAPOLController: already monitoring %s",
	       client->if_name);
	return;
    }
    my_log(LOG_DEBUG,
	   "EAPOLController: starting monitoring on %s", client->if_name);
    client->eapol_fd = eapol_socket(client->if_name, FALSE);
    if (client->eapol_fd < 0) {
	syslog(LOG_NOTICE,
	       "EAPOLController: failed to open EAPOL socket over %s",
	       client->if_name);
	return;
    }

    /* arrange to be called back when socket has data */
    context.info = client;
    client->eapol_sock
	= CFSocketCreateWithNative(NULL, client->eapol_fd,
				   kCFSocketReadCallBack,
				   monitoring_callback, &context);
    if (client->eapol_sock == NULL) {
	syslog(LOG_NOTICE,
	       "EAPOLController: failed create CFSocket over %s",
	       client->if_name);
	goto failed;
    }
    rls = CFSocketCreateRunLoopSource(NULL, client->eapol_sock, 0);
    if (rls == NULL) {
	syslog(LOG_NOTICE,
	       "EAPOLController: failed create CFRunLoopSource for %s",
	       client->if_name);
	goto failed;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    return;

 failed:
    eapolClientStopMonitoring(client);
    return;
}

static void
eapolClientExited(eapolClientRef client)
{
    boolean_t				no_one_logged_in;
    CFStringRef				user;
    
    if (S_store == NULL) {
	return;
    }
    user = SCDynamicStoreCopyConsoleUser(S_store, NULL, NULL);
    if (user == NULL) {
	no_one_logged_in = TRUE;
    }
    else {
	CFRelease(user);
	no_one_logged_in = FALSE;
    }
    handle_config_changed(no_one_logged_in);
    return;
}

#endif /* TARGET_OS_EMBEDDED */

/**
 ** fork/exec eapolclient routines
 **/
static void 
exec_callback(pid_t pid, int status, struct rusage * rusage, void * context)
{
    eapolClientRef	client;
    
    client = eapolClientLookupProcess(pid);
    if (client == NULL) {
	return;
    }
    if (client->state != kEAPOLControlStateIdle) {
	my_log(LOG_NOTICE, 
	       "EAPOLController: eapolclient(%s) pid=%d exited with status %d",
	       client->if_name,  pid, status);
    }
    eapolClientInvalidate(client);
    eapolClientExited(client);
    return;
}

typedef struct {
    int			eapol_fd;
    eapolClientRef	client;
} exec_context_t;

static void
exec_setup(pid_t pid, void * context)
{
    int			fd;
    int 		i;
    exec_context_t *	ec_p = (exec_context_t *)context;

    if (pid != 0) {
	/* parent: clean up file descriptors */
#if TARGET_OS_EMBEDDED
	close(ec_p->eapol_fd);
#else /* TARGET_OS_EMBEDDED */
	if (ec_p->client->eapol_fd == ec_p->eapol_fd) {
	    eapolClientStopMonitoring(ec_p->client);
	}
	else {
	    close(ec_p->eapol_fd);
	}
#endif /* TARGET_OS_EMBEDDED */
	return;
    }

    /* child: close all fds except the ones we inherit from parent */
    for (i = (getdtablesize() - 1); i >= 0; i--) {
	if (i != ec_p->eapol_fd) {
	    close(i);
	}
    }

    /* re-direct stdin to the inherited eapol_fd */
    if (ec_p->eapol_fd != STDIN_FILENO) {
	dup(ec_p->eapol_fd);			/* stdin */
	close(ec_p->eapol_fd);
    }

    /* re-direct stdout/stderr to /dev/null */
    fd = open(_PATH_DEVNULL, O_RDWR, 0);	/* stdout */
    dup(fd);					/* stderr */
    return;
}

static int
open_eapol_socket(eapolClientRef client)
{
#if ! TARGET_OS_EMBEDDED
    if (client->eapol_fd != -1) {
	return (client->eapol_fd);
    }
#endif /* ! TARGET_OS_EMBEDDED */
    return (eapol_socket(client->if_name,
			 (get_ifm_type(client->if_name) 
			  == IFM_IEEE80211)));
}

static int
eapolClientStart(eapolClientRef client, uid_t uid, gid_t gid, 
		 CFDictionaryRef config_dict, mach_port_t bootstrap,
		 mach_port_t au_session)
{
    char * 			argv[] = { S_eapolclient_path, 	/* 0 */
					   "-i",	       	/* 1 */
					   client->if_name,	/* 2 */
					   NULL,		/* 3 */
					   NULL,		/* 4 */
					   NULL,		/* 5 */
					   NULL,		/* 6 */
					   NULL,		/* 7 */
					   NULL };
    exec_context_t	ec;
    char			gid_str[32];
    int				status = 0;
    char			uid_str[32];

#if ! TARGET_OS_EMBEDDED
    client->user_cancelled = FALSE;
#endif /* ! TARGET_OS_EMBEDDED */

    bzero(&ec, sizeof(ec));
    ec.client = client;
    ec.eapol_fd = open_eapol_socket(client);
    if (ec.eapol_fd < 0) {
	syslog(LOG_NOTICE,
	       "EAPOLController: failed to open EAPOL socket over %s",
	       client->if_name);
	return (errno);
    }
    if (bootstrap != MACH_PORT_NULL) {
	snprintf(uid_str, sizeof(uid_str), "%u", uid);
	snprintf(gid_str, sizeof(gid_str), "%u", gid);
	argv[3] = "-u";
	argv[4] = uid_str;
	argv[5] = "-g";
	argv[6] = gid_str;
    }
    client->pid = _SCDPluginExecCommand2(exec_callback, NULL, 0, 0,
					 S_eapolclient_path, argv,
					 exec_setup, &ec);
    if (client->pid == -1) {
	/* failure, clean-up too */
	exec_setup(-1, &ec);
	status = errno;
    }
    else {
	boolean_t			on_console = FALSE;

	if (bootstrap != MACH_PORT_NULL) {
	    on_console = is_console_user(uid);
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
		client->au_session = au_session;
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
		CFDictionaryRef config_dict, mach_port_t bootstrap,
		mach_port_t au_session)
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
	int	ifm_type;

	/* make sure that the interface is one that we support */
	ifm_type = get_ifm_type(if_name);
	switch (ifm_type) {
	case IFM_ETHER:
	case IFM_IEEE80211:
	    break;
	default:
	    status = ENXIO;
	    goto done;
	}
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
    status = eapolClientStart(client, uid, gid, config_dict, bootstrap,
			      au_session);
 done:
    if (status != 0) {
	if (bootstrap != MACH_PORT_NULL) {
	    (void)mach_port_deallocate(mach_task_self(), bootstrap);
	}
	if (au_session != MACH_PORT_NULL) {
	    (void)mach_port_deallocate(mach_task_self(), au_session);
	}
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
#endif /* 0 */

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
		&& (client->config_dict == NULL
		    || S_get_plist_boolean(client->config_dict,
					   CFSTR("AllowStop"), TRUE))
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
    CFDictionaryRef		dict = NULL;
    EAPOLClientConfigurationRef	cfg;

    cfg = EAPOLClientConfigurationCreate(NULL);
    if (cfg != NULL) {
	CFStringRef		if_name_cf;
	EAPOLClientProfileRef	profile = NULL;

	if_name_cf = CFStringCreateWithCString(NULL, if_name, 
					       kCFStringEncodingASCII);
	profile = EAPOLClientConfigurationGetSystemProfile(cfg, if_name_cf);
	CFRelease(if_name_cf);
	if (profile != NULL) {
	    /* new, profileID-based configuration */
	    dict = S_profile_copy_itemID_dict(profile);
	}
	CFRelease(cfg);
    }
    return (dict);
}

int
ControllerStartSystem(if_name_t if_name, uid_t uid, gid_t gid,
		      CFDictionaryRef options)
{
    eapolClientRef 	client;
    CFDictionaryRef	dict = NULL;
    CFDictionaryRef	itemID_dict = NULL;
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

    /* check whether the caller provided which profile to use */
    if (options != NULL) {
	itemID_dict
	    = CFDictionaryGetValue(options, kEAPOLControlClientItemID);
	if (isA_CFDictionary(itemID_dict) != NULL) {
	    CFMutableDictionaryRef	new_dict;

	    new_dict = CFDictionaryCreateMutableCopy(NULL, 0, options);
	    CFDictionarySetValue(new_dict, kSystemModeManagedExternally,
				 kCFBooleanTrue);
	    dict = new_dict;
	}
    }

    /* check whether system mode is configured in the preferences */
    if (dict == NULL) {
	dict = system_eapol_copy(if_name);
	if (dict == NULL) {
	    status = ESRCH;
	    goto done;
	}
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
    status = eapolClientStart(client, 0, 0, dict, MACH_PORT_NULL,
			      MACH_PORT_NULL);

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

int
ControllerCopyAutoDetectInformation(CFDictionaryRef * info_p)
{
    CFMutableDictionaryRef	all_dict = NULL;
    eapolClientRef		scan;
    int				status = 0;

    LIST_FOREACH(scan, S_clientHead_p, link) {
	CFDataRef		data;
	int			elapsed_time;
	CFNumberRef		num;
	CFMutableDictionaryRef	this_dict = NULL;

	if (scan->state != kEAPOLControlStateIdle
	    || scan->eapol_fd == -1
	    || scan->packet_received == FALSE) {
	    continue;
	}

	elapsed_time = ((uint64_t)CFAbsoluteTimeGetCurrent())
	    - scan->packet_received_time;
	if (elapsed_time < 0) {
	    elapsed_time = 0;
	}
	else if (elapsed_time > 60) {
	    /* if it's been awhile since we saw a packet, ignore this entry */
	    continue;
	}
	this_dict = CFDictionaryCreateMutable(NULL, 0,
					      &kCFTypeDictionaryKeyCallBacks,
					      &kCFTypeDictionaryValueCallBacks);
	num = make_number(elapsed_time);
	CFDictionarySetValue(this_dict, kEAPOLAutoDetectSecondsSinceLastPacket,
			     num);
	CFRelease(num);

	data = CFDataCreate(NULL, (const UInt8 *)&scan->authenticator_mac,
			    sizeof(scan->authenticator_mac));
	CFDictionarySetValue(this_dict, kEAPOLAutoDetectAuthenticatorMACAddress,
			     data);
	CFRelease(data);
	if (all_dict == NULL) {
	    all_dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	}
	CFDictionarySetValue(all_dict, scan->if_name_cf, this_dict);
	CFRelease(this_dict);
    }
    if (all_dict != NULL && CFDictionaryGetCount(all_dict) == 0) {
	status = ENOENT;
	my_CFRelease(&all_dict);
    }
    *info_p = all_dict;
    return (status);
}

boolean_t
ControllerDidUserCancel(if_name_t if_name)
{
    boolean_t		cancelled = FALSE;
    eapolClientRef 	client;

    client = eapolClientLookupInterface(if_name);
    if (client == NULL) {
	cancelled = FALSE;
    }
    else {
	cancelled = client->user_cancelled;
    }
    return (cancelled);
}

#endif /* ! TARGET_OS_EMBEDDED */

int
ControllerClientAttach(pid_t pid, if_name_t if_name,
		       mach_port_t notify_port,
		       mach_port_t * session_port,
		       CFDictionaryRef * control_dict,
		       mach_port_t * bootstrap,
		       mach_port_t * au_session)
{
    CFMutableDictionaryRef	dict;
    CFNumberRef			command_cf;
    eapolClientRef		client;
    int				result = 0;
    CFRunLoopSourceRef		rls;

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
    rls = CFMachPortCreateRunLoopSource(NULL, client->session_cfport, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
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
    *bootstrap = client->bootstrap;
    *au_session = client->au_session;
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
    eapolClientInvalidate(client);
    eapolClientExited(client);

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

#if ! TARGET_OS_EMBEDDED
int
ControllerClientUserCancelled(mach_port_t session_port)
{
    eapolClientRef	client;
    int			result = 0;

    client = eapolClientLookupSession(session_port);
    if (client == NULL) {
	result = EINVAL;
	goto failed;
    }
    client->user_cancelled = TRUE;
    result = eapolClientStop(client);

 failed:
    return (result);
}
#endif /* ! TARGET_OS_EMBEDDED */


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

static CFArrayRef
copy_interface_list(void)
{
    CFDictionaryRef	dict;
    CFArrayRef		iflist = NULL;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    dict = SCDynamicStoreCopyValue(S_store, key);
    my_CFRelease(&key);

    if (isA_CFDictionary(dict) != NULL) {
	iflist = CFDictionaryGetValue(dict, kSCPropNetInterfaces);
	iflist = isA_CFArray(iflist);
    }
    if (iflist != NULL) {
	CFRetain(iflist);
    }
    else {
	iflist = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
    }

    my_CFRelease(&dict);
    return (iflist);
}

typedef struct {
    EAPOLClientConfigurationRef cfg;
    CFMutableArrayRef		configured_iflist;
    CFRange			configured_iflist_range;
    CFMutableDictionaryRef	system_mode_configurations;
    CFMutableArrayRef		system_mode_iflist;
} EAPOLEthernetInfo, * EAPOLEthernetInfoRef;

static void
EAPOLEthernetInfoProcess(const void * key, const void * value, void * context)
{
    EAPOLEthernetInfoRef	info_p = (EAPOLEthernetInfoRef)context;

    if (isA_CFDictionary(value) == NULL) {
	return;
    }
    if (CFStringHasSuffix(key, kSCEntNetEAPOL)) {
	CFStringRef		name;
	EAPOLClientProfileRef	profile = NULL;

	name = mySCNetworkInterfacePathCopyInterfaceName(key);
	if (info_p->cfg == NULL) {
	    info_p->cfg = EAPOLClientConfigurationCreate(NULL);
	}
	if (info_p->cfg != NULL) {
	    profile 
		= EAPOLClientConfigurationGetSystemProfile(info_p->cfg, name);
	}
	if (profile != NULL) {
	    CFDictionaryRef		this_config = NULL;

	    this_config = S_profile_copy_itemID_dict(profile);
	    if (this_config != NULL) {
		CFDictionarySetValue(info_p->system_mode_configurations,
				     name, this_config);
		CFRelease(this_config);
		CFArrayAppendValue(info_p->system_mode_iflist, name);
	    }
	}
	my_CFRelease(&name);
    }
    else {
	int			ifm_type = 0;
	char			ifname[IFNAMSIZ];
	CFStringRef		name;
	CFStringRef		type;

	type = CFDictionaryGetValue(value, kSCPropNetInterfaceType);
	if (type == NULL
	    || CFEqual(type, kSCValNetInterfaceTypeEthernet) == FALSE) {
	    return;
	}
	name = CFDictionaryGetValue(value, kSCPropNetInterfaceDeviceName);
	if (isA_CFString(name) == NULL) {
	    return;
	}
	if (CFStringGetCString(name, ifname, sizeof(ifname),
			       kCFStringEncodingASCII) == FALSE) {
	    return;
	}
	if (CFArrayContainsValue(info_p->configured_iflist,
				 info_p->configured_iflist_range, name)) {
	    return;
	}
	ifm_type = get_ifm_type(ifname);
	if (ifm_type != IFM_ETHER) {
	    /* ignore non-ethernet */
	    return;
	}
	CFArrayAppendValue(info_p->configured_iflist, name);
	info_p->configured_iflist_range.length++;
    }
    return;
}

static void
EAPOLEthernetInfoInit(EAPOLEthernetInfoRef info_p, boolean_t system_mode)
{
    int					count;
    int					i;
    CFStringRef				list[2];
    CFArrayRef				patterns;
    CFDictionaryRef			store_info = NULL;

    bzero(info_p, sizeof(*info_p));
    count = 0;
    list[count++]
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, 
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetInterface);
    if (system_mode) {
	list[count++] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      kSCEntNetEAPOL);
    }
    patterns = CFArrayCreate(NULL, (const void * *)list, count,
			     &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	CFRelease(list[i]);
    }
    store_info = SCDynamicStoreCopyMultiple(S_store, NULL, patterns);
    my_CFRelease(&patterns);
    if (store_info == NULL) {
	return;
    }

    /* build list of configured services and System mode configurations */
    info_p->configured_iflist 
	= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (system_mode) {
	info_p->system_mode_configurations
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	info_p->system_mode_iflist
	    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    CFDictionaryApplyFunction(store_info, EAPOLEthernetInfoProcess,
			      (void *)info_p);
    CFRelease(store_info);
    return;
}

static void
EAPOLEthernetInfoFree(EAPOLEthernetInfoRef info_p)
{
    my_CFRelease(&info_p->cfg);
    my_CFRelease(&info_p->configured_iflist);
    my_CFRelease(&info_p->system_mode_configurations);
    my_CFRelease(&info_p->system_mode_iflist);
    return;
}

static void
update_system_mode_interfaces(CFDictionaryRef system_mode_configurations,
			      CFArrayRef system_mode_iflist)
{
    CFArrayRef				current_iflist = NULL;
    int					i;
    CFRange				range;
    eapolClientRef			scan;
    int					status;

    /* get the current list of interfaces */
    current_iflist = copy_interface_list();
    range = CFRangeMake(0, CFArrayGetCount(current_iflist));

    /* change existing interface configurations */
    LIST_FOREACH(scan, S_clientHead_p, link) {
	CFDictionaryRef		this_config = NULL;

	if (CFArrayContainsValue(current_iflist,
				 range,
				 scan->if_name_cf) == FALSE) {
	    /* interface doesn't exist, stop it */
	    if (scan->state == kEAPOLControlStateIdle) {
		eapolClientStopMonitoring(scan);
	    }
	    else {
		(void)eapolClientStop(scan);
	    }
	    continue;
	}
	this_config
	    = CFDictionaryGetValue(system_mode_configurations,
				   scan->if_name_cf);
	if (scan->mode == kEAPOLControlModeSystem) {
	    /* interface is in System mode */
	    if (scan->config_dict == NULL) {
		/* we must be stopping, ignore it */
		continue;
	    }
	    if (CFDictionaryContainsKey(scan->config_dict,
					kSystemModeManagedExternally)) {
		/* this instance is managed externally, skip it */
		continue;
	    }
	    if (this_config == NULL) {
		/* interface is no longer in System mode */
		status = eapolClientStop(scan);
		if (status != 0) {
		    my_log(LOG_NOTICE, "EAPOLController handle_config_changed:"
			   " eapolClientStop (%s) failed %d", 
			   scan->if_name, status);
		}
	    }
	    else {
		if (CFEqual(this_config, scan->config_dict) == FALSE) {
		    status = eapolClientUpdate(scan, this_config);
		    if (status != 0) {
			my_log(LOG_NOTICE, 
			       "EAPOLController handle_config_changed: "
			       "eapolClientUpdate (%s) failed %d",
			       scan->if_name, status);
		    }
		}
	    }
	}
	else if (this_config != NULL) {
	    if (scan->state == kEAPOLControlStateIdle) {
		status = eapolClientStart(scan, 0, 0, 
					  this_config, MACH_PORT_NULL,
					  MACH_PORT_NULL);
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
    range = CFRangeMake(0, CFArrayGetCount(system_mode_iflist));
    for (i = 0; i < range.length; i++) {
	eapolClientRef		client;
	CFStringRef		if_name_cf;

	if_name_cf = CFArrayGetValueAtIndex(system_mode_iflist, i);
	client = eapolClientLookupInterfaceCF(if_name_cf);
	if (client == NULL) {
	    char *	if_name;

	    if_name = my_CFStringToCString(if_name_cf, kCFStringEncodingASCII);
	    client = eapolClientAdd(if_name);
	    if (client == NULL) {
		my_log(LOG_NOTICE, 
		       "EAPOLController handle_config_changed:"
		       " eapolClientAdd (%s) failed", if_name);
	    }
	    else {
		CFDictionaryRef		this_config;

		this_config
		    = CFDictionaryGetValue(system_mode_configurations,
					   if_name_cf);
		status = eapolClientStart(client, 0, 0,
					  this_config, MACH_PORT_NULL,
					  MACH_PORT_NULL);
		if (status != 0) {
		    my_log(LOG_NOTICE, 
			   "EAPOLController handle_config_changed:"
			   " eapolClientStart (%s) failed %d",
			   client->if_name, status);
		}
	    }
	    free(if_name);
	}
    }
    my_CFRelease(&current_iflist);
    return;
}

static void
update_monitored_interfaces(CFArrayRef configured_iflist)
{
    int				i;
    CFRange			range;
    eapolClientRef		scan;

    /* stop monitoring any interfaces that are no longer configured */
    range = CFRangeMake(0, CFArrayGetCount(configured_iflist));
    LIST_FOREACH(scan, S_clientHead_p, link) {
	if (CFArrayContainsValue(configured_iflist,
				 range,
				 scan->if_name_cf) == FALSE) {
	    if (scan->state == kEAPOLControlStateIdle) {
		eapolClientStopMonitoring(scan);
	    }
	}
    }

    /* start monitoring any interfaces that are configured */
    for (i = 0; i < range.length; i++) {
	eapolClientRef		client;
	CFStringRef		if_name_cf;

	if_name_cf 
	    = (CFStringRef)CFArrayGetValueAtIndex(configured_iflist, i);
	client = eapolClientLookupInterfaceCF(if_name_cf);
	if (client == NULL 
	    || (client->state == kEAPOLControlStateIdle
		&& client->eapol_fd == -1)) {
	    char *	if_name;

	    if_name = my_CFStringToCString(if_name_cf, kCFStringEncodingASCII);
	    if (client == NULL) {
		client = eapolClientAdd(if_name);
		if (client == NULL) {
		    my_log(LOG_NOTICE, 
			   "EAPOLController: monitor "
			   "eapolClientAdd (%s) failed", if_name);
		}
	    }
	    if (client != NULL) {
		eapolClientStartMonitoring(client);
	    }
	    free(if_name);
	}
    }
    return;
}

static void
handle_config_changed(boolean_t check_system_mode)
{
    EAPOLEthernetInfo			info;

    if (S_store == NULL) {
	return;
    }

    /* get a snapshot of the configuration information */
    EAPOLEthernetInfoInit(&info, check_system_mode);

    if (check_system_mode) {
	update_system_mode_interfaces(info.system_mode_configurations,
				      info.system_mode_iflist);
    }
    update_monitored_interfaces(info.configured_iflist);

    EAPOLEthernetInfoFree(&info);
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
	handle_config_changed(TRUE);
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
    SCDynamicStoreContext	context;
    CFArrayRef			keys = NULL;
    CFStringRef			list[2];
    CFArrayRef			patterns = NULL;
    CFRunLoopSourceRef		rls;
    SCDynamicStoreRef		store;

    bzero(&context, sizeof(context));
    store = SCDynamicStoreCreate(NULL, CFSTR("EAPOLController"), 
				 eapol_handle_change, &context);
    if (store == NULL) {
	my_log(LOG_NOTICE, "EAPOLController: SCDynamicStoreCreate() failed, %s",
	       SCErrorString(SCError()));
	return (NULL);
    }

    /* console user */
    list[0]
	= SCDynamicStoreKeyCreateConsoleUser(NULL);

    /* list of interfaces */
    list[1] 
	= SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    keys = CFArrayCreate(NULL, (const void * *)list, 2, &kCFTypeArrayCallBacks);
    CFRelease(list[0]);
    CFRelease(list[1]);
    
    /* requested EAPOL configurations */
    list[0] 
	= SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							kSCCompAnyRegex,
							kSCEntNetEAPOL);
    /* configured services */
    list[1]
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, 
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetInterface);
    patterns = CFArrayCreate(NULL, (const void * *)list, 2,
			     &kCFTypeArrayCallBacks);
    CFRelease(list[0]);
    CFRelease(list[1]);

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
    handle_config_changed(TRUE);
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
