/*
 * Copyright (c) 2001-2010 Apple Inc. All rights reserved.
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
 * October 26, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 * May 21, 2008		Dieter Siegmund (dieter@apple.com)
 * - added multiple Supplicant support
 */

/*
 * EAPOLSocket.c
 * - "object" that wraps access to EAP over LAN
 */
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <sys/queue.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <stdint.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/dlil.h>
#include <net/ndrv.h>
#include <net/ethernet.h>
#include <net/if_media.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "EAPOLClient.h"
#include "EAPOLControl.h"
#include "EAPOLControlPrivate.h"
#include "EAPUtil.h"
#include "EAPOLUtil.h"
#include "eapol_socket.h"
#include "FDHandler.h"
#include "Timer.h"
#include "EAPOLSocket.h"
#include "EAPOLSocketPrivate.h"
#include "myCFUtil.h"
#include "mylog.h"
#include "printdata.h"
#include <TargetConditionals.h>

#if ! TARGET_OS_EMBEDDED
#include "my_darwin.h"
#endif /* TARGET_OS_EMBEDDED */

#include "InterestNotification.h"

#ifndef NO_WIRELESS
#include "wireless.h"
#endif /* NO_WIRELESS */

#define EAPOLSOCKET_RECV_BUFSIZE	1600
#define EAPOLSOCKET_SEND_BUFSIZE	1600

static const struct ether_addr eapol_multicast = {
    EAPOL_802_1_X_GROUP_ADDRESS
};

/* MTU */
#define kMTU				CFSTR("MTU")
#define EAPOL_MTU_DEFAULT		1280
#define EAPOL_MTU_MIN			576
static int				S_mtu;

/* pre-auth tunables */
#define kPreauthentication		CFSTR("Preauthentication")
#define kScanDelayAuthenticatedSeconds	CFSTR("ScanDelayAuthenticatedSeconds")
#define kScanDelayRoamSeconds		CFSTR("ScanDelayRoamSeconds")
#define kScanPeriodSeconds		CFSTR("ScanPeriodSeconds")
#define kEnablePreauthentication	CFSTR("EnablePreauthentication")
#define kNumberOfScans			CFSTR("NumberOfScans")

/* 
 * Static: S_enable_preauth
 * 
 * Purpose:
 *   Controls whether pre-authentication will occur on wireless interfaces.
 */
static bool	S_enable_preauth = FALSE;

/*
 * Static Variable: S_scan_delay_authenticated_secs, S_scan_delay_roam_secs
 *
 * Purpose:
 *   Affect when the SSID-directed scan will occur.
 *   
 *   S_scan_delay_authenticated_secs controls when/if the scan gets scheduled
 *   after the main Supplicant reaches the Authenticated state.
 *
 *   S_scan_delay_roam_secs controls when/if the scan gets scheduled after
 *   we roam from one AP to another.
 *
 *   If the value is >= 0, the scan will be scheduled after that many seconds.
 *   If the value is < 0, the scan will not be scheduled.
 */
#define SCAN_DELAY_AUTHENTICATED_SECS	10
#define SCAN_DELAY_ROAM_SECS		10

static int	S_scan_delay_authenticated_secs = SCAN_DELAY_AUTHENTICATED_SECS;
static int	S_scan_delay_roam_secs = SCAN_DELAY_ROAM_SECS;

/*
 * Static: S_scan_period_secs
 *
 * Purpose:
 *   After a scan completes, controls when/if another scan gets scheduled
 *   in a certain period of time.
 *
 *   A periodic scan gets scheduled if the value is > 0, otherwise it does
 *   not get scheduled.
 */
#define SCAN_PERIOD_SECS		(-1)
static int	S_scan_period_secs = SCAN_PERIOD_SECS;

/* 
 * Static: S_number_of_scans
 *
 * Purpose:
 *   The number of 802.11 scans to do each time we initiate a scan.
 */
#define NUMBER_OF_SCANS			1
static int	S_number_of_scans = NUMBER_OF_SCANS;

/* 
 * Static: S_debug
 *
 * Purpose:
 *   Controls whether the packet trace is dumped to stdout or not.
 */
static bool	S_debug = FALSE;

struct EAPOLSocket_s;

TAILQ_HEAD(EAPOLSocketHead_s, EAPOLSocket_s);

struct EAPOLSocketSource_s {
    EAPOLClientRef			client;
    char				if_name[IF_NAMESIZE];
    int					if_name_length;
    struct ether_addr			ether;
    EAPOLSocketReceiveData		rx;
    FDHandler *				handler;
    bool				is_wireless;
    bool				is_wpa_enterprise;
    bool				link_active;
    bool				authenticated;
    InterestNotificationRef		interest;
    struct ether_addr			authenticator_mac;
    bool				authenticator_mac_valid;
#ifndef NO_WIRELESS
    wireless_t				wref;
    CFStringRef				ssid;
    /* BSSID for the default 802.1X connection */
    struct ether_addr			bssid;
    bool				bssid_valid;
#endif /* NO_WIRELESS */
    CFRunLoopObserverRef		observer;
    bool				process_removals;
    TimerRef				scan_timer;
    SCDynamicStoreRef			store;
    EAPOLSocketRef			sock;
    struct EAPOLSocketHead_s		preauth_sockets;
    int					preauth_sockets_count;
    EAPOLControlMode			mode;
};

struct EAPOLSocket_s {
    struct ether_addr			bssid;
    EAPOLSocketReceiveCallbackRef	func;
    void *				arg1;
    void *				arg2;
    EAPOLSocketSourceRef		source;
    SupplicantRef			supp;
    bool				remove;
    TAILQ_ENTRY(EAPOLSocket_s)		link;
};

/**
 ** forward declarations
 **/
static int
EAPOLSocketSourceTransmit(EAPOLSocketSourceRef source,
			  EAPOLSocketRef sock,
			  EAPOLPacketType packet_type,
			  void * body, unsigned int body_length);
static bool
EAPOLSocketSourceUpdateWirelessInfo(EAPOLSocketSourceRef source);

static EAPOLSocketRef
EAPOLSocketSourceLookupPreauthSocket(EAPOLSocketSourceRef source, 
				     const struct ether_addr * bssid);

static void
EAPOLSocketSourceInitiateScan(EAPOLSocketSourceRef source);

static void
EAPOLSocketSourceCancelScan(EAPOLSocketSourceRef source);

static void
EAPOLSocketSourceScheduleScan(EAPOLSocketSourceRef source, int delay_secs);

static void
EAPOLSocketSourceMarkPreauthSocketsForRemoval(EAPOLSocketSourceRef source);

static void
EAPOLSocketSourceScheduleHandshakeNotification(EAPOLSocketSourceRef source);

static void
EAPOLSocketSourceUnscheduleHandshakeNotification(EAPOLSocketSourceRef source);

static void
EAPOLSocketSourceForceRenew(EAPOLSocketSourceRef source);

/**
 ** EAPOLSocket* routines
 **/

static boolean_t
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef key, boolean_t def)
{
    CFBooleanRef 	b;
    boolean_t		ret = def;

    b = isA_CFBoolean(CFDictionaryGetValue(plist, key));
    if (b) {
	ret = CFBooleanGetValue(b);
    }
    if (eapolclient_should_log(kLogFlagTunables)) {
	FILE *	log_file = eapolclient_log_file();

	SCPrint(TRUE, log_file,
		CFSTR("%@ = %s\n"), key, ret == TRUE ? "true" : "false");
	fflush(log_file);
    }
    return (ret);
}


static int
S_get_plist_int(CFDictionaryRef plist, CFStringRef key, int def)
{
    CFNumberRef 	n;
    int			ret = def;

    n = isA_CFNumber(CFDictionaryGetValue(plist, key));
    if (n) {
	if (CFNumberGetValue(n, kCFNumberIntType, &ret) == FALSE) {
	    ret = def;
	}
    }
    if (eapolclient_should_log(kLogFlagTunables)) {
	FILE *	log_file = eapolclient_log_file();

	SCPrint(TRUE, log_file, CFSTR("%@ = %d\n"), key, ret);
	fflush(log_file);
    }
    return (ret);
}

void
EAPOLSocketSetGlobals(SCPreferencesRef prefs)
{
    CFDictionaryRef	plist;
    CFNumberRef		mtu;

    if (prefs == NULL) {
	return;
    }
    mtu = SCPreferencesGetValue(prefs, kMTU);
    if (isA_CFNumber(mtu) != NULL) {
	int		mtu_val;

	if (CFNumberGetValue(mtu, kCFNumberIntType, 
			     &mtu_val) == FALSE) {
	    my_log(LOG_NOTICE, "com.apple.eapolclient.MTU invalid");
	}
	else if (mtu_val < EAPOL_MTU_MIN) {
	    my_log(LOG_NOTICE,
		   "com.apple.eapolclient.MTU %d < %d, using default %d",
		   mtu_val, EAPOL_MTU_MIN, EAPOL_MTU_DEFAULT);
	}
	else {
	    S_mtu = mtu_val;
	}
    }

    plist = SCPreferencesGetValue(prefs, kPreauthentication);
    if (isA_CFDictionary(plist) != NULL) {
	S_enable_preauth
	    = S_get_plist_boolean(plist, kEnablePreauthentication,
				  FALSE);
	S_scan_delay_authenticated_secs
	    = S_get_plist_int(plist, kScanDelayAuthenticatedSeconds,
			      SCAN_DELAY_AUTHENTICATED_SECS);
	S_scan_delay_roam_secs
	    = S_get_plist_int(plist, kScanDelayRoamSeconds,
			      SCAN_DELAY_ROAM_SECS);
	S_scan_period_secs
	    = S_get_plist_int(plist, kScanPeriodSeconds,
			      SCAN_PERIOD_SECS);
	S_number_of_scans
	    = S_get_plist_int(plist, kNumberOfScans,
			      NUMBER_OF_SCANS);
    }
    return;
}

void
EAPOLSocketSetDebug(boolean_t debug)
{
    S_debug = debug;
    return;
}

static void
EAPOLSocketMarkForRemoval(EAPOLSocketRef sock)
{
    sock->remove = TRUE;
    sock->source->process_removals = TRUE;
    return;
}

static boolean_t
EAPOLSocketIsMain(EAPOLSocketRef sock)
{
    return (sock->source->sock == sock);
}

const char *
EAPOLSocketIfName(EAPOLSocketRef sock, uint32_t * name_length)
{
    EAPOLSocketSourceRef	source = sock->source;

    if (name_length != NULL) {
	*name_length = source->if_name_length;
    }
    return (source->if_name);
}

const char *
EAPOLSocketName(EAPOLSocketRef sock)
{
    const char *	name;

    if (EAPOLSocketIsMain(sock)) {
	name = "(main)";
    }
    else {
	name = ether_ntoa(&sock->bssid);
    }
    return (name);
}

boolean_t
EAPOLSocketIsWireless(EAPOLSocketRef sock)
{
    return (sock->source->is_wireless);
}

static void
EAPOLSocketFree(EAPOLSocketRef * sock_p)
{
    EAPOLSocketRef 		sock;

    if (sock_p == NULL) {
	return;
    }
    sock = *sock_p;
    if (sock != NULL) {
	EAPOLSocketSourceRef	source;

	source = sock->source;
	if (sock == source->sock) {
	    /* main supplicant */
	    source->sock = NULL;
	}
	else {
	    /* pre-auth supplicant */
	    TAILQ_REMOVE(&source->preauth_sockets, sock, link);
	    source->preauth_sockets_count--;
	}
	free(sock);
    }
    *sock_p = NULL;
    return;
}

boolean_t
EAPOLSocketSetKey(EAPOLSocketRef sock, wirelessKeyType type, 
		    int index, const uint8_t * key, int key_length)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else NO_WIRELESS
    if (sock->source->is_wireless == FALSE) {
	return (FALSE);
    }
    return (wireless_set_key(sock->source->wref, type, index, key, key_length));
#endif /* NO_WIRELESS */
}

CFStringRef
EAPOLSocketGetSSID(EAPOLSocketRef sock)
{
#ifdef NO_WIRELESS
    return (NULL);
#else NO_WIRELESS
    if (sock->source->is_wireless == FALSE) {
	return (NULL);
    }
    return (sock->source->ssid);
#endif /* NO_WIRELESS */
}

int
EAPOLSocketMTU(EAPOLSocketRef sock)
{
    if (S_mtu != 0) {
	return (S_mtu);
    }
    return (EAPOL_MTU_DEFAULT);
}

const struct ether_addr *
EAPOLSocketGetAuthenticatorMACAddress(EAPOLSocketRef sock)
{
    EAPOLSocketSourceRef	source = sock->source;

    if (source->sock != sock || source->authenticator_mac_valid == FALSE) {
	return (NULL);
    }
    return (&source->authenticator_mac);
}

void
EAPOLSocketEnableReceive(EAPOLSocketRef sock,
			 EAPOLSocketReceiveCallback * func,
			 void * arg1, void * arg2)
{
    sock->func = func;
    sock->arg1 = arg1;
    sock->arg2 = arg2;
    return;
}

void
EAPOLSocketDisableReceive(EAPOLSocketRef sock)
{
    sock->func = NULL;
    return;
}

int
EAPOLSocketTransmit(EAPOLSocketRef sock,
		    EAPOLPacketType packet_type,
		    void * body, unsigned int body_length)
{
    return (EAPOLSocketSourceTransmit(sock->source, sock, packet_type,
				      body, body_length));
}

void
EAPOLSocketClearPMKCache(EAPOLSocketRef sock)
{
#ifndef NO_WIRELESS
    EAPOLSocketSourceRef	source = sock->source;

    if (source->is_wireless == FALSE || source->sock != sock) {
	return;
    }
    if (wireless_clear_wpa_pmk_cache(source->wref)) {
	eapolclient_log(kLogFlagBasic, "PMK cache cleared\n");
    }
#endif /* NO_WIRELESS */
    return;
}

boolean_t
EAPOLSocketSetPMK(EAPOLSocketRef sock, 
		  const uint8_t * key, int key_length)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else /* NO_WIRELESS */
    const struct ether_addr *	bssid;
    EAPOLSocketSourceRef	source = sock->source;

    if (source->is_wireless == FALSE || source->is_wpa_enterprise == FALSE) {
	return (FALSE);
    }
    if (source->sock == sock) {
	/* main supplicant */
	bssid = NULL;
	if (key_length != 0 && source->authenticated == FALSE) {
	    EAPOLSocketSourceScheduleHandshakeNotification(source);
	}
	else {
	    /* if the notification is still active, de-activate it */
	    EAPOLSocketSourceUnscheduleHandshakeNotification(source);
	}
    }
    else {
	/* pre-auth supplicant */
	bssid = &sock->bssid;
    }
    if (eapolclient_should_log(kLogFlagBasic)) {
	if (bssid == NULL) {
	    eapolclient_log(kLogFlagBasic, "set_key %d\n", key_length);
	}
	else {
	    eapolclient_log(kLogFlagBasic, "set_key %s %d\n",
			    ether_ntoa(bssid), key_length);
	}
    }
    return (wireless_set_wpa_pmk(source->wref, bssid, key, key_length));
#endif /* NO_WIRELESS */
}

bool
EAPOLSocketIsLinkActive(EAPOLSocketRef sock)
{
    return (sock->source->link_active);
}

void
EAPOLSocketReportStatus(EAPOLSocketRef sock, CFDictionaryRef status_dict)
{
    EAPOLClientRef		client;
    int				result;
    EAPOLSocketSourceRef	source = sock->source;

    client = source->client;
    if (client == NULL) {
	return;
    }

    /* for now, only report status for the main supplicant */
    if (source->sock == sock) {
	EAPClientStatus		client_status;
	SupplicantState		supplicant_state;

	supplicant_state = Supplicant_get_state(sock->supp, &client_status);
	switch (supplicant_state) {
	case kSupplicantStateInactive:
	    EAPOLSocketSourceUnscheduleHandshakeNotification(source);
	    source->authenticated = FALSE;
	    break;
	case kSupplicantStateAuthenticated:
	    if (source->authenticated == FALSE) {
		EAPOLSocketSourceUnscheduleHandshakeNotification(source);
		EAPOLSocketSourceForceRenew(source);
		source->authenticated = TRUE;
	    }
	    break;
	case kSupplicantStateHeld:
	    EAPOLSocketSourceUnscheduleHandshakeNotification(source);
	    source->authenticated = FALSE;
	    EAPOLSocketSourceForceRenew(source);
	    break;
	case kSupplicantStateLogoff:
	    if (EAPOLSocketIsWireless(sock) == FALSE) {
		/* 5900529: wait for 1/2 second before the force renew */
		usleep(500 * 1000);
	    }
	    EAPOLSocketSourceForceRenew(source);
	    break;
	}
	result = EAPOLClientReportStatus(client, status_dict);
	if (result != 0) {
	    my_log(LOG_NOTICE, "EAPOLClientReportStatus failed: %s",
		   strerror(result));
	}
	if (S_enable_preauth && sock->source->is_wireless) {
	    switch (supplicant_state) {
	    case kSupplicantStateAuthenticated:
		EAPOLSocketSourceScheduleScan(sock->source,
					      S_scan_delay_authenticated_secs);
		break;
	    default:
		/* get rid of the pre-auth supplicants */
		EAPOLSocketSourceCancelScan(sock->source);
		EAPOLSocketSourceMarkPreauthSocketsForRemoval(sock->source);
		break;
	    }
	}
    }
    else {
	EAPClientStatus		client_status;

	switch (Supplicant_get_state(sock->supp, &client_status)) {
	case kSupplicantStateHeld:
	    my_log(LOG_NOTICE, "Supplicant %s Held, status %d",
		   ether_ntoa(&sock->bssid), client_status);
	    eapolclient_log(kLogFlagBasic,
			    "Supplicant %s Held, status %d\n",
			    ether_ntoa(&sock->bssid), client_status);
	    EAPOLSocketMarkForRemoval(sock);
	    break;
	case kSupplicantStateAuthenticated:
	    if (eapolclient_should_log(kLogFlagBasic)) {
		eapolclient_log(kLogFlagBasic,
				"Supplicant %s Authenticated - Complete\n",
				ether_ntoa(&sock->bssid));
	    }
	    EAPOLSocketMarkForRemoval(sock);
	    break;
	case kSupplicantStateAuthenticating:
	    /* check for user input required, if so kill it */
	    if (client_status == kEAPClientStatusUserInputRequired) {
		my_log(LOG_NOTICE,
		       "Supplicant %s Authenticating, requires user input",
		       ether_ntoa(&sock->bssid));
		eapolclient_log(kLogFlagBasic,
				"Supplicant %s Authenticating,"
				" requires user input\n", 
				ether_ntoa(&sock->bssid));
		EAPOLSocketMarkForRemoval(sock);
	    }
	    break;
	case kSupplicantStateConnecting:
	case kSupplicantStateAcquired:
	case kSupplicantStateLogoff:
	case kSupplicantStateInactive:
	case kSupplicantStateDisconnected:
	default:
	    break;
	}
    }
    return;
}

EAPOLControlMode
EAPOLSocketGetMode(EAPOLSocketRef sock)
{
    return (sock->source->mode);
}

void
EAPOLSocketStopClient(EAPOLSocketRef sock)
{
    EAPOLSocketSourceRef	source = sock->source;

    if (source->sock != sock) {
	return;
    }
#if TARGET_OS_EMBEDDED
    EAPOLControlStop(EAPOLSocketIfName(sock, NULL));
#else /* TARGET_OS_EMBEDDED */
    EAPOLClientUserCancelled(source->client);
#endif /* TARGET_OS_EMBEDDED */
    if (EAPOLSocketIsWireless(sock)) {
	wireless_disassociate(source->wref);
    }
    return;
}

/**
 ** packet validation/printing routines
 **/
static void
ether_header_fprint(FILE * f, struct ether_header * eh_p)
{
    fprintf(f, "Ether packet: dest %s ",
	    ether_ntoa((void *)eh_p->ether_dhost));
    fprintf(f, "source %s type 0x%04x\n", 
	    ether_ntoa((void *)eh_p->ether_shost),
	    ntohs(eh_p->ether_type));
    return;
}

static bool
ether_header_valid(struct ether_header * eh_p, unsigned int length,
		   FILE * f)
{
    if (length < sizeof(*eh_p)) {
	if (f != NULL) {
	    fprintf(f, "Packet length %d < sizeof(*eh_p) %ld\n",
		    length, sizeof(*eh_p));
	    fprint_data(f, (void *)eh_p, length);
	}
	return (FALSE);
    }
    if (f != NULL) {
	ether_header_fprint(f, eh_p);
    }
    return (TRUE);
}

/**
 ** EAPOLSocketSource routines
 **/

static SCDynamicStoreRef
link_event_register(const char * if_name,
		    SCDynamicStoreCallBack func, void * arg)
{
    CFMutableArrayRef		keys = NULL;
    CFStringRef			key;
    CFRunLoopSourceRef		rls;
    SCDynamicStoreRef		store;
    SCDynamicStoreContext	context;

    bzero(&context, sizeof(context));
    context.info = arg;
    store = SCDynamicStoreCreate(NULL, CFSTR("EAPOLClient"), 
				 func, &context);
    if (store == NULL) {
	my_log(LOG_NOTICE, "SCDynamicStoreCreate() failed, %s",
	       SCErrorString(SCError()));
	return (NULL);
    }
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    key = SCDynamicStoreKeyCreate(NULL, 
				  CFSTR("%@/%@/%@/%s/%@"),
				  kSCDynamicStoreDomainState,
				  kSCCompNetwork,
				  kSCCompInterface,
				  if_name,
				  kSCEntNetLink);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);
    SCDynamicStoreSetNotificationKeys(store, keys, NULL);
    my_CFRelease(&keys);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    my_CFRelease(&rls);
    return (store);
}

static bool
get_number(CFNumberRef num_cf, uint32_t * num_p)
{
    if (isA_CFNumber(num_cf) == NULL
	|| CFNumberGetValue(num_cf, kCFNumberIntType, num_p) == FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

static void
EAPOLSocketSourceForceRenew(EAPOLSocketSourceRef source)
{
    EAPOLClientRef	client;

    client = source->client;
    if (client == NULL) {
	return;
    }
    eapolclient_log(kLogFlagBasic, "force renew\n");
    (void)EAPOLClientForceRenew(client);
    return;
}

static void
EAPOLSocketSourceStop(EAPOLSocketSourceRef source)
{
    my_log(LOG_NOTICE, "%s STOP", source->if_name);
    Supplicant_stop(source->sock->supp);
    EAPOLSocketSourceFree(&source);
    exit(EX_OK);
    /* NOT REACHED */
    return;
}

static void
EAPOLSocketSourceClientNotification(EAPOLClientRef client, Boolean server_died,
				    void * context)
{
    EAPOLClientControlCommand	command;
    CFNumberRef			command_cf;
    CFDictionaryRef		control_dict = NULL;
    int				result;
    EAPOLSocketSourceRef	source = (EAPOLSocketSourceRef)context;

    if (server_died) {
	my_log(LOG_NOTICE, "%s: EAPOLController died", source->if_name);
	if (source->mode == kEAPOLControlModeUser) {
	    goto stop;
	}
	/* just exit, don't send EAPOL Logoff packet <rdar://problem/6418520> */
	exit(EX_OK);
    }
    result = EAPOLClientGetConfig(client, &control_dict);
    if (result != 0) {
	my_log(LOG_NOTICE, "%s: EAPOLClientGetConfig failed, %s",
	       source->if_name, strerror(result));
	goto stop;
    }
    if (control_dict == NULL) {
	my_log(LOG_NOTICE, "%s: EAPOLClientGetConfig returned NULL control",
	       source->if_name);
	goto stop;
    }
    command_cf = CFDictionaryGetValue(control_dict,
				      kEAPOLClientControlCommand);
    if (get_number(command_cf, &command) == FALSE) {
	my_log(LOG_NOTICE, "%s: invalid/missing command",
	       source->if_name);
	goto stop;
    }
    if (Supplicant_control(source->sock->supp, command, 
			   control_dict) == TRUE) {
	goto stop;
    }
    my_CFRelease(&control_dict);
    return;

 stop:
    EAPOLSocketSourceStop(source);
    /* NOT REACHED */
    return;
}

static EAPOLSocketRef
EAPOLSocketSourceLookupPreauthSocket(EAPOLSocketSourceRef source, 
				     const struct ether_addr * bssid)
{
    EAPOLSocketRef		scan;

    TAILQ_FOREACH(scan, &source->preauth_sockets, link) {
	if (bcmp(&scan->bssid, bssid, sizeof(scan->bssid)) == 0) {
	    return (scan);
	}
    }
    return (NULL);
}

static void
EAPOLSocketSourceMarkPreauthSocketsForRemoval(EAPOLSocketSourceRef source)
{
    EAPOLSocketRef		scan;

    TAILQ_FOREACH(scan, &source->preauth_sockets, link) {
	EAPOLSocketMarkForRemoval(scan);
    }
    return;
}

static bool
is_link_active(const char * name)
{
    bool		active = FALSE;
    struct ifmediareq	ifm;
    int			s;

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
    if ((ifm.ifm_status & IFM_AVALID) == 0
	|| (ifm.ifm_status & IFM_ACTIVE) != 0) {
	active = TRUE;
    }

 done:
    if (s >= 0) {
	close(s);
    }
    return (active);
}

static void
EAPOLSocketSourceLinkStatusChanged(SCDynamicStoreRef session,
				   CFArrayRef _not_used,
				   void * info)
{
    EAPOLSocketSourceRef	source = (EAPOLSocketSourceRef)info;

    source->link_active = is_link_active(source->if_name);
    eapolclient_log(kLogFlagBasic, "link %s\n",
		    source->link_active ? "active" : "inactive");

    /* make sure our wireless information is up to date */
    if (source->is_wireless) {
	EAPOLSocketSourceUpdateWirelessInfo(source);
    }

    /* let the 802.1X Supplicant know about the link status change */
    if (source->sock != NULL) {
	Supplicant_link_status_changed(source->sock->supp, source->link_active);
    }
    return;
}

static void
EAPOLSocketSourceReceive(void * arg1, void * arg2)
{
    uint32_t			buf[EAPOLSOCKET_RECV_BUFSIZE / sizeof(uint32_t)];
    EAPOLPacketRef		eapol_p;
    struct ether_header *	eh_p = (struct ether_header *)buf;
    uint16_t			ether_type;
    int				length;
    int 			n;
    EAPOLSocketReceiveDataRef	rx;
    EAPOLSocketRef		sock = NULL;
    EAPOLSocketSourceRef 	source = (EAPOLSocketSourceRef)arg1;

    n = recv(FDHandler_fd(source->handler), (char *)buf, sizeof(buf), 0);
    if (n <= 0) {
	if (n < 0) {
	    my_log(LOG_NOTICE, "EAPOLSocketSourceReceive: recv failed %s",
		   strerror(errno));
	}
	goto done;
    }
    if (S_debug) {
	printf("\n"
	       "----------------------------------------\n");
	timestamp_fprintf(stdout, "Receive Packet Size: %d\n", n);
    }
    if (ether_header_valid(eh_p, n, S_debug ? stdout : NULL)
	== FALSE) {
	goto done;
    }
    ether_type = ntohs(eh_p->ether_type);
    switch (ether_type) {
    case EAPOL_802_1_X_ETHERTYPE:
    case IEEE80211_PREAUTH_ETHERTYPE:
	break;
    default:
	if (S_debug) {
	    fprintf(stdout, "Unexpected ethertype (%02x)\n",
		    ether_type);
	}
	goto done;
    }
    eapol_p = (void *)(eh_p + 1);
    length = n - sizeof(*eh_p);
    if (EAPOLPacketValid(eapol_p, length, S_debug ? stdout : NULL) == FALSE) {
	goto done;
    }
    if (ether_type == EAPOL_802_1_X_ETHERTYPE) {
	bcopy(eh_p->ether_shost, &source->authenticator_mac, 
	      sizeof(source->authenticator_mac));
	source->authenticator_mac_valid = TRUE;
	if (source->is_wireless) {
	    if (source->bssid_valid == FALSE
		|| bcmp(eh_p->ether_shost, &source->bssid,
			sizeof(eh_p->ether_shost)) != 0) {
		EAPOLSocketSourceUpdateWirelessInfo(source);
	    }
	}
	else {
	}
    }
    rx = &source->rx;
    rx->length = length;
    rx->eapol_p = eapol_p;
    if (eapolclient_should_log(kLogFlagPacketDetails)) {
	FILE *	log_file = eapolclient_log_file();
	
	eapolclient_log(kLogFlagPacketDetails,
			"Receive Packet Size %d\n", n);
	ether_header_fprint(log_file, eh_p);
	EAPOLPacketValid(eapol_p, length, log_file);
	fflush(log_file);
    }
    else if (eapolclient_should_log(kLogFlagBasic)) {
	eapolclient_log(kLogFlagBasic,
			"Receive Size %d Type 0x%04x From %s\n",
			n, ntohs(eh_p->ether_type),
			ether_ntoa((void *)eh_p->ether_shost));
    }
    /* dispatch the packet to the right socket */
    if (ether_type == EAPOL_802_1_X_ETHERTYPE) {
	sock = source->sock;
    }
    else {
	sock = EAPOLSocketSourceLookupPreauthSocket(source,
						    (const struct ether_addr *)
						    eh_p->ether_shost);
    }
    if (sock != NULL && sock->func != NULL) {
	(*sock->func)(sock->arg1, sock->arg2, rx);
    }
    rx->eapol_p = NULL;
    if (S_debug) {
	fflush(stdout);
	fflush(stderr);
    }

 done:
    return;
}

static int
EAPOLSocketSourceTransmit(EAPOLSocketSourceRef source,
			  EAPOLSocketRef sock,
			  EAPOLPacketType packet_type,
			  void * body, unsigned int body_length)
{
    uint32_t			buf[EAPOLSOCKET_SEND_BUFSIZE / sizeof(uint32_t)];
    EAPOLPacket *		eapol_p;
    struct ether_header *	eh_p;
    struct sockaddr_ndrv 	ndrv;
    unsigned int		size;

    size = sizeof(*eh_p) + sizeof(*eapol_p);
    if (body != NULL) {
	size += body_length;
    }
    else {
	body_length = 0;
    }

    bzero(buf, size);
    eh_p = (struct ether_header *)buf;
    eapol_p = (void *)(eh_p + 1);

    if (source->sock == sock) {
	if (source->is_wireless) {
	    /* if we don't know the bssid, try to update it now */
	    if (source->bssid_valid == FALSE) {
		EAPOLSocketSourceUpdateWirelessInfo(source);
		if (source->bssid_valid == FALSE) {
		    /* bssid unknown, drop the packet */
		    eapolclient_log(kLogFlagBasic,
				    "Transmit: unknown BSSID,"
				    " not sending %d bytes\n",
				    body_length + sizeof(*eapol_p));
		    my_log(LOG_DEBUG,
			   "EAPOLSocketSourceTransmit: unknown BSSID"
			   ", not sending %d bytes",
			   body_length + sizeof(*eapol_p));
		    return (-1);
		}
	    }
	    /* copy the current bssid */
	    bcopy(&source->bssid, &eh_p->ether_dhost,
		  sizeof(eh_p->ether_dhost));
	}
	else {
	    /* ethernet uses the multicast address */
	    bcopy(&eapol_multicast, &eh_p->ether_dhost, 
		  sizeof(eh_p->ether_dhost));
	}
	eh_p->ether_type = htons(EAPOL_802_1_X_ETHERTYPE);
    }
    else {
	/* pre-auth uses a specific BSSID */
	bcopy(&sock->bssid, &eh_p->ether_dhost,
	      sizeof(eh_p->ether_dhost));
	eh_p->ether_type = htons(IEEE80211_PREAUTH_ETHERTYPE);

    }
    bcopy(&source->ether, eh_p->ether_shost, 
	  sizeof(eh_p->ether_shost));
    eapol_p->protocol_version = EAPOL_802_1_X_PROTOCOL_VERSION;
    eapol_p->packet_type = packet_type;
    EAPOLPacketSetLength(eapol_p, body_length);
    if (body != NULL) {
	bcopy(body, eapol_p->body, body_length);
    }

    /* the contents of ndrv are ignored */
    bzero(&ndrv, sizeof(ndrv));
    ndrv.snd_len = sizeof(ndrv);
    ndrv.snd_family = AF_NDRV;

    if (S_debug) {
	printf("\n"
	       "========================================\n");
	timestamp_fprintf(stdout, "Transmit Packet Size %d\n", size);
	ether_header_valid(eh_p, size, stdout);
	EAPOLPacketValid(eapol_p, body_length + sizeof(*eapol_p), stdout);
	fflush(stdout);
	fflush(stderr);
    }

    if (eapolclient_should_log(kLogFlagPacketDetails)) {
	FILE *		log_file = eapolclient_log_file();

	eapolclient_log(kLogFlagPacketDetails,
			"Transmit Packet Size %d\n", 
			body_length + sizeof(*eapol_p));
	ether_header_fprint(log_file, eh_p);
	EAPOLPacketValid(eapol_p, 
			 body_length + sizeof(*eapol_p),
			 log_file);
	fflush(log_file);
    }
    else if (eapolclient_should_log(kLogFlagBasic)) {
	eapolclient_log(kLogFlagBasic,
			"Transmit Size %d Type 0x%04x To %s\n",
			body_length + sizeof(*eapol_p),
			ntohs(eh_p->ether_type),
			ether_ntoa((void *)eh_p->ether_dhost));
    }
    if (sendto(FDHandler_fd(source->handler), eh_p, size, 
	       0, (struct sockaddr *)&ndrv, sizeof(ndrv)) < size) {
	my_log(LOG_NOTICE, "EAPOLSocketSourceTransmit: sendto failed, %s",
	       strerror(errno));
	return (-1);
    }
    return (0);
}

static void
EAPOLSocketSourceRemovePreauthSockets(EAPOLSocketSourceRef source)
{
    int				i;
    EAPOLSocketRef		remove_list[source->preauth_sockets_count];
    int				remove_list_count;
    EAPOLSocketRef		scan;

    /* remove all pre-auth sockets marked with remove */
    remove_list_count = 0;
    TAILQ_FOREACH(scan, &source->preauth_sockets, link) {
	if (scan->remove) {
	    remove_list[remove_list_count++] = scan;
	}
    }
    for (i = 0; i < remove_list_count; i++) {
	EAPOLSocketRef	sock = remove_list[i];

	if (eapolclient_should_log(kLogFlagBasic)) {
	    eapolclient_log(kLogFlagBasic, "Removing Supplicant for %s\n",
			    ether_ntoa(&sock->bssid));
	}
	Supplicant_free(&sock->supp);
	EAPOLSocketFree(&sock);
    }
    return;
}

#define N_REMOVE_STATIC		10
static void
EAPOLSocketSourceObserver(CFRunLoopObserverRef observer, 
			  CFRunLoopActivity activity, void * info)
{
    EAPOLSocketSourceRef	source = (EAPOLSocketSourceRef)info;

    if (source->process_removals) {
	EAPOLSocketSourceRemovePreauthSockets(source);
	source->process_removals = FALSE;
    }
    return;
}

static bool
fd_is_socket(int fd)
{
    struct stat		sb;

    if (fstat(fd, &sb) == 0) {
	if (S_ISSOCK(sb.st_mode)) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

EAPOLSocketSourceRef
EAPOLSocketSourceCreate(const char * if_name,
			const struct ether_addr * ether,
			CFDictionaryRef * control_dict_p)
{
    int				fd = -1;
    FDHandler *			handler = NULL;
    bool			is_wireless = FALSE;
    CFRunLoopObserverRef	observer = NULL;
    int				result;
    EAPOLSocketSourceRef	source = NULL;
    SCDynamicStoreRef		store = NULL;
    TimerRef			scan_timer = NULL;
    wireless_t			wref = NULL;

    *control_dict_p = NULL;
#ifndef NO_WIRELESS
    /* is this a wireless interface? */
    if (wireless_bind(if_name, &wref)) {
	is_wireless = TRUE;
    }
#endif /* NO_WIRELESS */
    if (fd_is_socket(STDIN_FILENO)) {
	/* already have the socket we need */
	fd = 0;
    }
    else {
	fd = eapol_socket(if_name, is_wireless);
	if (fd == -1) {
	    my_log(LOG_NOTICE,
		   "EAPOLSocketSourceCreate: eapol_socket(%s) failed, %m");
	    goto failed;
	}
    }
    handler = FDHandler_create(fd);
    if (handler == NULL) {
	my_log(LOG_NOTICE, "EAPOLSocketSourceCreate: FDHandler_create failed");
	goto failed;
    }

    source = malloc(sizeof(*source));
    if (source == NULL) {
	my_log(LOG_NOTICE, "EAPOLSocketSourceCreate: malloc failed");
	goto failed;
    }
    bzero(source, sizeof(*source));
    if (is_wireless) {
	CFRunLoopObserverContext context = { 0, NULL, NULL, NULL, NULL };
	context.info = source;
	observer = CFRunLoopObserverCreate(NULL,
					   kCFRunLoopBeforeWaiting,
					   TRUE, 0,
					   EAPOLSocketSourceObserver,
					   &context);
	if (observer == NULL) {
	    my_log(LOG_INFO, "CFRunLoopObserverCreate failed\n");
	    goto failed;
	}
	scan_timer = Timer_create();
	if (scan_timer == NULL) {
	    my_log(LOG_INFO, "Timer_create failed\n");
	    goto failed;
	}
    }
    store = link_event_register(if_name,
				EAPOLSocketSourceLinkStatusChanged,
				source);
    if (store == NULL) {
	my_log(LOG_NOTICE, "link_event_register failed: %s",
	       SCErrorString(SCError()));
	goto failed;
    }
    TAILQ_INIT(&source->preauth_sockets);
    strlcpy(source->if_name, if_name, sizeof(source->if_name));
    source->if_name_length = strlen(source->if_name);
    source->ether = *ether;
    source->handler = handler;
    source->store = store;
    source->is_wireless = is_wireless;
    source->wref = wref;
    FDHandler_enable(handler, EAPOLSocketSourceReceive, source, NULL);
    EAPOLSocketSourceLinkStatusChanged(source->store, NULL, source);
    source->client = EAPOLClientAttach(source->if_name,
				       EAPOLSocketSourceClientNotification, 
				       source, control_dict_p, &result);
    if (source->client == NULL) {
	my_log(LOG_NOTICE, "EAPOLClientAttach(%s) failed: %s",
	       source->if_name, strerror(result));
    }
    if (observer != NULL) {
	source->observer = observer;
	CFRunLoopAddObserver(CFRunLoopGetCurrent(), source->observer, 
			     kCFRunLoopDefaultMode);
    }
    source->scan_timer = scan_timer;
    return (source);

 failed:
#ifndef NO_WIRELESS
    if (wref != NULL) {
	wireless_free(wref);
    }
#endif /* NO_WIRELESS */
    if (source != NULL) {
	free(source);
    }
    if (handler != NULL) {
	FDHandler_free(&handler);
    }
    else if (fd >= 0) {
	close(fd);
    }
    if (store != NULL) {
	CFRelease(store);
    }
    if (observer != NULL) {
	CFRelease(observer);
    }
    Timer_free(&scan_timer);
    return (NULL);
}


static void
EAPOLSocketSourceRemoveSocketWithBSSID(EAPOLSocketSourceRef source,
				       const struct ether_addr * bssid)
{
    EAPOLSocketRef	sock;

    sock = EAPOLSocketSourceLookupPreauthSocket(source, bssid);
    if (sock == NULL) {
	/* no such socket */
	return;
    }
    if (eapolclient_should_log(kLogFlagBasic)) {
	eapolclient_log(kLogFlagBasic, "Removing Supplicant for %s\n",
			ether_ntoa(bssid));
    }
    Supplicant_free(&sock->supp);
    EAPOLSocketFree(&sock);
    return;
}

static bool
EAPOLSocketSourceUpdateWirelessInfo(EAPOLSocketSourceRef source)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else NO_WIRELESS
    struct ether_addr	ap_mac;
    bool 		ap_mac_valid = FALSE;
    bool		changed = FALSE;

    if (source->is_wireless == FALSE) {
	return (FALSE);
    }
    ap_mac_valid = wireless_ap_mac(source->wref, &ap_mac);
    if (ap_mac_valid == FALSE) {
	my_log(LOG_DEBUG,
	       "EAPOLSocketSourceUpdateWirelessInfo: not associated");
	changed = source->bssid_valid;
	source->bssid_valid = FALSE;
	source->is_wpa_enterprise = FALSE;
	EAPOLSocketSourceUnscheduleHandshakeNotification(source);
	eapolclient_log(kLogFlagBasic, "Disassociated\n");
	my_CFRelease(&source->ssid);
	Timer_cancel(source->scan_timer);
	wireless_scan_cancel(source->wref);
	source->authenticated = FALSE;
    }
    else {
	CFStringRef	ssid;

	if (source->bssid_valid == FALSE
	    || bcmp(&ap_mac, &source->bssid, sizeof(ap_mac)) != 0) {
	    changed = TRUE;

	    if (S_enable_preauth) {
		/* remove any pre-auth socket with the new bssid */
		EAPOLSocketSourceRemoveSocketWithBSSID(source, &ap_mac);
		if (source->bssid_valid == TRUE) {
		    /* we roamed */
		    EAPOLSocketSourceScheduleScan(source,
						  S_scan_delay_roam_secs);
		}
	    }
	}
	source->bssid_valid = TRUE;
	source->bssid = ap_mac;
	ssid = wireless_copy_ssid_string(source->wref);
	source->is_wpa_enterprise = wireless_is_wpa_enterprise(source->wref);
	if (source->ssid != NULL && ssid != NULL
	    && !CFEqual(source->ssid, ssid)) {
	    EAPOLSocketSourceCancelScan(source);
	}
	my_CFRelease(&source->ssid);
	source->ssid = ssid;
	if (S_debug) {
	    SCLog(TRUE, LOG_NOTICE, 
		  CFSTR("EAPOLSocketSourceUpdateWirelessInfo:"
			" ssid %@ bssid %s"),
		  (source->ssid != NULL) ? source->ssid : CFSTR("<unknown>"),
		  ether_ntoa(&ap_mac));
	}
	if (eapolclient_should_log(kLogFlagBasic)) {
	    FILE *	log_file = eapolclient_log_file();

	    eapolclient_log(kLogFlagBasic, "Associated");
	    SCPrint(TRUE, log_file, CFSTR(" SSID %@ BSSID %s\n"),
		    (source->ssid != NULL) ? source->ssid : CFSTR("<unknown>"),
		    ether_ntoa(&ap_mac));
	    fflush(log_file);
	}
    }
    return (changed);
#endif /* NO_WIRELESS */
}

void
EAPOLSocketSourceFree(EAPOLSocketSourceRef * source_p)
{
    EAPOLSocketSourceRef 	source;

    if (source_p == NULL) {
	return;
    }
    source = *source_p;

    if (source != NULL) {
	FDHandler_free(&source->handler);
#ifndef NO_WIRELESS
	if (source->is_wireless) {
	    wireless_free(source->wref);
	}
	my_CFRelease(&source->ssid);
#endif /* NO_WIRELESS */
	if (source->observer != NULL) {
	    CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), source->observer, 
				    kCFRunLoopDefaultMode);
	    my_CFRelease(&source->observer);
	}
	my_CFRelease(&source->store);
	EAPOLClientDetach(&source->client);

	Timer_free(&source->scan_timer);
	EAPOLSocketSourceUnscheduleHandshakeNotification(source);
	free(source);
    }
    *source_p = NULL;
    return;
}


static EAPOLSocketRef
EAPOLSocketSourceCreateSocket(EAPOLSocketSourceRef source, 
			      const struct ether_addr * bssid)
{
    EAPOLSocketRef		sock = NULL;

    sock = malloc(sizeof(*sock));
    if (sock == NULL) {
	my_log(LOG_NOTICE, "EAOLSocketSourceCreateSocket: malloc failed");
	return (NULL);
    }
    bzero(sock, sizeof(*sock));
    sock->source = source;
    if (bssid != NULL) {
	sock->bssid = *bssid;
	TAILQ_INSERT_TAIL(&source->preauth_sockets, sock, link);
	source->preauth_sockets_count++;
    }
    else {
	source->sock = sock;
    }
    return (sock);
}

SupplicantRef
EAPOLSocketSourceCreateSupplicant(EAPOLSocketSourceRef source,
				  CFDictionaryRef control_dict)
{
    CFDictionaryRef		config_dict = NULL;
    EAPOLControlMode		mode = kEAPOLControlModeNone;
    bool			should_stop = FALSE;
    EAPOLSocketRef		sock = NULL;
    SupplicantRef		supp = NULL;

    if (control_dict != NULL) {
	EAPOLClientControlCommand	command;
	CFNumberRef			command_cf;
	CFNumberRef			mode_cf;

	command_cf = CFDictionaryGetValue(control_dict,
					  kEAPOLClientControlCommand);
	if (get_number(command_cf, &command) == FALSE) {
	    goto failed;
	}
	if (command != kEAPOLClientControlCommandRun) {
	    my_log(LOG_NOTICE, "%s: received stop command", source->if_name);
	    goto failed;
	}
	mode_cf = CFDictionaryGetValue(control_dict,
				       kEAPOLClientControlMode);
	if (mode_cf != NULL
	    && get_number(mode_cf, &mode) == FALSE) {
	    my_log(LOG_NOTICE, "%s: Mode property invalid",
		   source->if_name);
	    goto failed;
	}
	config_dict = CFDictionaryGetValue(control_dict,
					   kEAPOLClientControlConfiguration);
	if (config_dict == NULL) {
	    my_log(LOG_NOTICE, "%s: configuration empty", source->if_name);
	    goto failed;
	}
    }
    source->mode = mode;
    sock = EAPOLSocketSourceCreateSocket(source, NULL);
    if (sock == NULL) {
	goto failed;
    }
    supp = Supplicant_create(sock);
    if (supp == NULL) {
	goto failed;
    }
    switch (mode) {
    case kEAPOLControlModeSystem:
    case kEAPOLControlModeLoginWindow:
	Supplicant_set_no_ui(supp);
	break;
    default:
	break;
    }
    if (config_dict != NULL) {
	Supplicant_update_configuration(supp, config_dict, &should_stop);
	if (should_stop) {
	    return (NULL);
	}
    }
    sock->supp = supp;
    return (supp);

 failed:
    EAPOLSocketFree(&sock);
    Supplicant_free(&supp);
    return (NULL);
}

static void
S_log_bssid_list(CFArrayRef bssid_list)
{
    int		count;
    int		i;
    FILE *	log_file = eapolclient_log_file();

    count = CFArrayGetCount(bssid_list);
    eapolclient_log(kLogFlagBasic, "Scan complete: %d AP%s = {", 
		    count, (count == 1) ? "" : "s");
    for (i = 0; i < count; i++) {
	CFDataRef			bssid_data;
	const struct ether_addr *	bssid;
		
	bssid_data = CFArrayGetValueAtIndex(bssid_list, i);
	bssid = (const struct ether_addr *)CFDataGetBytePtr(bssid_data);
	fprintf(log_file, "%s%s", (i == 0) ? "" : ", ",
		ether_ntoa(bssid));
    }
    fprintf(log_file, "}\n");
    fflush(log_file);
}

static void
EAPOLSocketSourceScanCallback(wireless_t wref,
			      CFArrayRef bssid_list, void * arg)
{
    EAPOLSocketSourceRef	source = (EAPOLSocketSourceRef)arg;

    if (bssid_list == NULL) {
	eapolclient_log(kLogFlagBasic, "Scan complete: no APs\n");
    }
    else if (source->bssid_valid == FALSE) {
	eapolclient_log(kLogFlagBasic,
			"Scan complete: Supplicant bssid unknown\n");
	my_log(LOG_NOTICE, "main Supplicant bssid is unknown, skipping");
    }
    else {
	int	count;
	int	i;

	if (eapolclient_should_log(kLogFlagBasic)) {
	    S_log_bssid_list(bssid_list);
	}
	count = CFArrayGetCount(bssid_list);
	for (i = 0; i < count; i++) {
	    CFDataRef			bssid_data;
	    const struct ether_addr *	bssid;
	    EAPOLSocketRef		sock;
		
	    bssid_data = CFArrayGetValueAtIndex(bssid_list, i);
	    bssid = (const struct ether_addr *)CFDataGetBytePtr(bssid_data);
	    if (bcmp(bssid, &source->bssid, sizeof(source->bssid)) == 0) {
		/* skip matching on the main Supplicant */
		continue;
	    }
	    sock = EAPOLSocketSourceLookupPreauthSocket(source, bssid);
	    if (sock != NULL) {
		/* already one running */
		continue;
	    }
	    sock = EAPOLSocketSourceCreateSocket(source, bssid);
	    if (sock == NULL) {
		continue;
	    }
	    sock->supp = Supplicant_create_with_supplicant(sock,
							   source->sock->supp);
	    if (sock->supp == NULL) {
		my_log(LOG_NOTICE, "Supplicant create %s failed",
		       ether_ntoa(&sock->bssid));
		if (eapolclient_should_log(kLogFlagBasic)) {
		    eapolclient_log(kLogFlagBasic,
				    "Supplicant create %s failed\n",
				    ether_ntoa(&sock->bssid));
		}
		EAPOLSocketFree(&sock);
	    }
	    else {
		if (eapolclient_should_log(kLogFlagBasic)) {
		    eapolclient_log(kLogFlagBasic,
				    "Supplicant %s created\n",
				    ether_ntoa(&sock->bssid));
		}
		Supplicant_start(sock->supp);
	    }
	}
    }
    if (S_scan_period_secs > 0) {
	EAPOLSocketSourceScheduleScan(source, S_scan_period_secs);
    }
    return;
}

static void
EAPOLSocketSourceInitiateScan(EAPOLSocketSourceRef source)
{
    if (source->ssid != NULL) {
	wireless_scan(source->wref, source->ssid,
		      S_number_of_scans, EAPOLSocketSourceScanCallback,
		      (void *)source);
	eapolclient_log(kLogFlagBasic, "Scan initiated\n");
    }
    return;
}

static void
EAPOLSocketSourceCancelScan(EAPOLSocketSourceRef source)
{
    Timer_cancel(source->scan_timer);
    wireless_scan_cancel(source->wref);
    return;
}


static void
EAPOLSocketSourceScheduleScan(EAPOLSocketSourceRef source, int delay)
{
    struct timeval	t;

    if (delay < 0) {
	/* don't schedule a scan if the delay is negative */
	return;
    }
    t.tv_sec = delay;
    t.tv_usec = 0;
    Timer_set_relative(source->scan_timer, t,
		       (void *)EAPOLSocketSourceInitiateScan,
		       (void *)source, NULL, NULL);
    return;
}

static boolean_t
EAPOLSocketSourceReleaseHandshakeNotification(EAPOLSocketSourceRef source)
{
    if (source->interest == NULL) {
	return (FALSE);
    }
    InterestNotificationRelease(source->interest);
    source->interest = NULL;
    return (TRUE);
}

static void
EAPOLSocketSourceHandshakeComplete(InterestNotificationRef interest_p,
				   const void * arg)
{
    EAPClientStatus		client_status;
    EAPOLSocketSourceRef	source = (EAPOLSocketSourceRef)arg;
    SupplicantState		supplicant_state;

    eapolclient_log(kLogFlagBasic, "4-way handshake complete\n");
    supplicant_state = Supplicant_get_state(source->sock->supp, &client_status);
    if (supplicant_state == kSupplicantStateAuthenticated) {
	EAPOLSocketSourceForceRenew(source);
    }
    EAPOLSocketSourceReleaseHandshakeNotification(source);
    return;
}

static void
EAPOLSocketSourceScheduleHandshakeNotification(EAPOLSocketSourceRef source)
{
    EAPOLSocketSourceUnscheduleHandshakeNotification(source);
    source->interest
	= InterestNotificationCreate(source->if_name, 
				     EAPOLSocketSourceHandshakeComplete,
				     source);
    if (source->interest != NULL) {
	source->authenticated = TRUE;
	eapolclient_log(kLogFlagBasic,
			"4-way handshake notification scheduled\n");
    }
    return;
}

static void
EAPOLSocketSourceUnscheduleHandshakeNotification(EAPOLSocketSourceRef source)
{
    if (EAPOLSocketSourceReleaseHandshakeNotification(source)) {
	eapolclient_log(kLogFlagBasic,
			"4-way handshake notification unscheduled\n");
    }
    return;
}
