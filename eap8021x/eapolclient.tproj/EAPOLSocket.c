/*
 * Copyright (c) 2001-2014 Apple Inc. All rights reserved.
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
#include "symbol_scope.h"
#include <TargetConditionals.h>

#define ALIGNED_BUF(name, size, type)	type 	name[(size) / (sizeof(type))]

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

/* testing tunables */
#define kTesting			CFSTR("Testing")
#define kTransmitPacketLossPercent	CFSTR("TransmitPacketLossPercent")
#define kReceivePacketLossPercent	CFSTR("ReceivePacketLossPercent")

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
 * Static: S_transmit_loss_percent, S_receive_loss_percent
 * Purpose:
 *   When set, used to simulate packet loss with the given packet loss
 *   percentage.
 */
static int 	S_transmit_loss_percent;
static int 	S_receive_loss_percent;

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
    bool				need_force_renew;
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
    EAPPacketRef			eap_tx_packet;
    int					eap_tx_packet_length;
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
static void
EAPOLSocketSourceUpdateWirelessInfo(EAPOLSocketSourceRef source,
				    const struct ether_addr * rx_source_mac);

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
    if (b != NULL) {
	ret = CFBooleanGetValue(b);
    }
    eapolclient_log(kLogFlagTunables,
		    "%@ = %s", key, ret == TRUE ? "true" : "false");
    return (ret);
}


static int
S_get_plist_int_log(CFDictionaryRef plist, CFStringRef key, int def,
		    bool should_log)
{
    CFNumberRef 	n;
    int			ret = def;

    n = isA_CFNumber(CFDictionaryGetValue(plist, key));
    if (n != NULL) {
	if (CFNumberGetValue(n, kCFNumberIntType, &ret) == FALSE) {
	    ret = def;
	}
    }
    if (should_log) {
	eapolclient_log(kLogFlagTunables, "%@ = %d", key, ret);
    }
    return (ret);
}

PRIVATE_EXTERN int
get_plist_int(CFDictionaryRef plist, CFStringRef key, int def)
{
    return (S_get_plist_int_log(plist, key, def, TRUE));
}

/*
 * Function: S_simulated_event_occurred
 * Purpose:
 *   Given the rate of occurrence of the event in 'percent', return
 *   whether the simulated event has occurred.
 * Returns:
 *   true if the event occurred, false otherwise.
 */
static bool
S_simulated_event_occurred(int percent)
{
    u_int32_t	value;

    if (percent == 0) {
	return (false);
    }
    value = arc4random() / (UINT32_MAX / 100);
    if (value < percent) {
	return (true);
    }
    return (false);
}

PRIVATE_EXTERN void
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
	    EAPLOG_FL(LOG_NOTICE, "com.apple.eapolclient.MTU invalid");
	}
	else if (mtu_val < EAPOL_MTU_MIN) {
	    EAPLOG_FL(LOG_NOTICE,
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
	if (S_enable_preauth) {
	    S_scan_delay_authenticated_secs
		= get_plist_int(plist, kScanDelayAuthenticatedSeconds,
				SCAN_DELAY_AUTHENTICATED_SECS);
	    S_scan_delay_roam_secs
		= get_plist_int(plist, kScanDelayRoamSeconds,
				SCAN_DELAY_ROAM_SECS);
	    S_scan_period_secs
		= get_plist_int(plist, kScanPeriodSeconds,
				SCAN_PERIOD_SECS);
	    S_number_of_scans
		= get_plist_int(plist, kNumberOfScans,
				NUMBER_OF_SCANS);
	}
    }

    plist = SCPreferencesGetValue(prefs, kTesting);
    if (isA_CFDictionary(plist) != NULL) {
	S_transmit_loss_percent 
	    = S_get_plist_int_log(plist, kTransmitPacketLossPercent, 0, FALSE);
	if (S_transmit_loss_percent != 0) {
	    EAPLOG(LOG_NOTICE,
		   "Will simulate %d%% transmit packet loss",
		   S_transmit_loss_percent);
	}
	S_receive_loss_percent
	    = S_get_plist_int_log(plist, kReceivePacketLossPercent, 0, FALSE);
	if (S_receive_loss_percent != 0) {
	    EAPLOG(LOG_NOTICE,
		   "Will simulate %d%% receive packet loss",
		   S_receive_loss_percent);
	}
    }
    return;
}

/*
 * Function: EAPOLSocketSetEAPTxPacket
 * Purpose:
 *   Set the last EAP transmit packet.
 *   If 'pkt' is NULL, then just clear the old packet that may be there.
 *   If 'pkt' is not NULL, clear the old packet, and make a copy of the
 *   new packet and save it.
 */
static void
EAPOLSocketSetEAPTxPacket(EAPOLSocketRef sock, EAPPacketRef pkt, int length)
{
    if (sock->eap_tx_packet != NULL) {
	free(sock->eap_tx_packet);
    }
    if (pkt == NULL) {
	sock->eap_tx_packet = NULL;
	sock->eap_tx_packet_length = 0;
    }
    else {
	sock->eap_tx_packet = (EAPPacketRef)malloc(length);
	bcopy(pkt, sock->eap_tx_packet, length);
	sock->eap_tx_packet_length = length;
    }
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

PRIVATE_EXTERN const char *
EAPOLSocketIfName(EAPOLSocketRef sock, uint32_t * name_length)
{
    EAPOLSocketSourceRef	source = sock->source;

    if (name_length != NULL) {
	*name_length = source->if_name_length;
    }
    return (source->if_name);
}

PRIVATE_EXTERN const char *
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

PRIVATE_EXTERN boolean_t
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
	EAPOLSocketSetEAPTxPacket(sock, NULL, 0);
	free(sock);
    }
    *sock_p = NULL;
    return;
}

PRIVATE_EXTERN boolean_t
EAPOLSocketSetKey(EAPOLSocketRef sock, wirelessKeyType type, 
		  int index, const uint8_t * key, int key_length)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else /* NO_WIRELESS */
    if (sock->source->is_wireless == FALSE) {
	return (FALSE);
    }
    return (wireless_set_key(sock->source->wref, type, index, key, key_length));
#endif /* NO_WIRELESS */
}

PRIVATE_EXTERN CFStringRef
EAPOLSocketGetSSID(EAPOLSocketRef sock)
{
#ifdef NO_WIRELESS
    return (NULL);
#else /* NO_WIRELESS */
    if (sock->source->is_wireless == FALSE) {
	return (NULL);
    }
    return (sock->source->ssid);
#endif /* NO_WIRELESS */
}

PRIVATE_EXTERN int
EAPOLSocketMTU(EAPOLSocketRef sock)
{
    if (S_mtu != 0) {
	return (S_mtu);
    }
    return (EAPOL_MTU_DEFAULT);
}

PRIVATE_EXTERN const struct ether_addr *
EAPOLSocketGetAuthenticatorMACAddress(EAPOLSocketRef sock)
{
    EAPOLSocketSourceRef	source = sock->source;

    if (source->sock != sock || source->authenticator_mac_valid == FALSE) {
	return (NULL);
    }
    return (&source->authenticator_mac);
}

PRIVATE_EXTERN void
EAPOLSocketEnableReceive(EAPOLSocketRef sock,
			 EAPOLSocketReceiveCallback * func,
			 void * arg1, void * arg2)
{
    sock->func = func;
    sock->arg1 = arg1;
    sock->arg2 = arg2;
    return;
}

PRIVATE_EXTERN void
EAPOLSocketDisableReceive(EAPOLSocketRef sock)
{
    sock->func = NULL;
    return;
}

PRIVATE_EXTERN int
EAPOLSocketTransmit(EAPOLSocketRef sock,
		    EAPOLPacketType packet_type,
		    void * body, unsigned int body_length)
{
    if (packet_type == kEAPOLPacketTypeEAPPacket) {
	EAPOLSocketSetEAPTxPacket(sock, body, body_length);
    }
    else {
	EAPOLSocketSetEAPTxPacket(sock, NULL, 0);
    }
    return (EAPOLSocketSourceTransmit(sock->source, sock, packet_type,
				      body, body_length));
}

PRIVATE_EXTERN void
EAPOLSocketClearPMKCache(EAPOLSocketRef sock)
{
#ifndef NO_WIRELESS
    EAPOLSocketSourceRef	source = sock->source;

    if (source->is_wireless == FALSE || source->sock != sock) {
	return;
    }
    if (wireless_clear_wpa_pmk_cache(source->wref)) {
	eapolclient_log(kLogFlagBasic, "PMK cache cleared");
    }
#endif /* NO_WIRELESS */
    return;
}

PRIVATE_EXTERN boolean_t
EAPOLSocketHasPMK(EAPOLSocketRef sock)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else /* NO_WIRELESS */
    EAPOLSocketSourceRef	source = sock->source;

    if (source->sock != sock
	|| source->is_wireless == FALSE
	|| source->is_wpa_enterprise == FALSE
	|| source->bssid_valid == FALSE) {
	return (FALSE);
    }
    return (wireless_has_pmk(source->wref, &source->bssid));
#endif /* NO_WIRELESS */
}


PRIVATE_EXTERN boolean_t
EAPOLSocketSetWPAKey(EAPOLSocketRef sock, const uint8_t * msk, int msk_length)
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
	if (msk_length != 0) {
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
    if (bssid == NULL) {
	eapolclient_log(kLogFlagBasic, "set_msk %d",
			msk_length);
    }
    else {
	eapolclient_log(kLogFlagBasic, "set_msk %s %d",
			ether_ntoa(bssid), msk_length);
    }
    return (wireless_set_wpa_msk(source->wref, bssid, msk, msk_length));
#endif /* NO_WIRELESS */
}

PRIVATE_EXTERN bool
EAPOLSocketIsLinkActive(EAPOLSocketRef sock)
{
    return (sock->source->link_active);
}

PRIVATE_EXTERN void
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
		source->need_force_renew = TRUE;
	    }
	    else {
		EAPOLSocketSourceForceRenew(source);
	    }
	    break;
	}
	result = EAPOLClientReportStatus(client, status_dict);
	if (result != 0) {
	    EAPLOG_FL(LOG_NOTICE, "EAPOLClientReportStatus failed: %s",
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
	    EAPLOG(LOG_NOTICE, "Supplicant %s Held, status %d",
		   ether_ntoa(&sock->bssid), client_status);
	    EAPOLSocketMarkForRemoval(sock);
	    break;
	case kSupplicantStateAuthenticated:
	    eapolclient_log(kLogFlagBasic,
			    "Supplicant %s Authenticated - Complete",
			    ether_ntoa(&sock->bssid));
	    EAPOLSocketMarkForRemoval(sock);
	    break;
	case kSupplicantStateAuthenticating:
	    /* check for user input required, if so kill it */
	    if (client_status == kEAPClientStatusUserInputRequired) {
		EAPLOG(LOG_NOTICE,
		       "Supplicant %s Authenticating, requires user input",
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

PRIVATE_EXTERN EAPOLControlMode
EAPOLSocketGetMode(EAPOLSocketRef sock)
{
    return (sock->source->mode);
}

PRIVATE_EXTERN void
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

#if ! TARGET_OS_EMBEDDED
PRIVATE_EXTERN boolean_t
EAPOLSocketReassociate(EAPOLSocketRef sock)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else /* NO_WIRELESS */
    boolean_t			ret;
    EAPOLSocketSourceRef	source = sock->source;

    if (EAPOLSocketIsWireless(sock) == FALSE) {
	return (FALSE);
    }
    if (EAPOLSocketIsMain(sock) == FALSE) {
	return (FALSE);
    }
    ret = wireless_reassociate(source->wref,
			       EAPOLSocketIfName(sock, NULL));
    eapolclient_log(kLogFlagBasic, "re-associate%s", ret ? "" : " failed");
    return (ret);
#endif /* NO_WIRELESS */
}
#endif /* ! TARGET_OS_EMBEDDED */

/**
 ** packet printing
 **/
static void
ether_header_print_to_string(CFMutableStringRef str, struct ether_header * eh_p)
{
    STRING_APPEND(str, "Ether packet: dest %s ",
		  ether_ntoa((void *)eh_p->ether_dhost));
    STRING_APPEND(str, "source %s type 0x%04x\n", 
		  ether_ntoa((void *)eh_p->ether_shost),
		  ntohs(eh_p->ether_type));
    return;
}

/**
 ** EAPOLSocketSource routines
 **/

static SCDynamicStoreRef
link_event_register(const char * if_name, boolean_t is_wireless,
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
	EAPLOG_FL(LOG_NOTICE, "SCDynamicStoreCreate() failed, %s",
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
				  is_wireless
				  ? kSCEntNetAirPort : kSCEntNetLink);
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
    eapolclient_log(kLogFlagBasic, "force renew");
    (void)EAPOLClientForceRenew(client);
    return;
}

static void
EAPOLSocketSourceStop(EAPOLSocketSourceRef source)
{
    EAPLOG(LOG_NOTICE, "%s STOP", source->if_name);
    source->need_force_renew = FALSE;
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
	EAPLOG(LOG_NOTICE, "%s: EAPOLController died", source->if_name);
	if (source->mode == kEAPOLControlModeUser) {
	    goto stop;
	}
	/* just exit, don't send EAPOL Logoff packet <rdar://problem/6418520> */
	exit(EX_OK);
    }
    result = EAPOLClientGetConfig(client, &control_dict);
    if (result != 0) {
	EAPLOG(LOG_NOTICE, "%s: EAPOLClientGetConfig failed, %s",
	       source->if_name, strerror(result));
	goto stop;
    }
    if (control_dict == NULL) {
	EAPLOG_FL(LOG_NOTICE, "%s: EAPOLClientGetConfig returned NULL control",
		  source->if_name);
	goto stop;
    }
    command_cf = CFDictionaryGetValue(control_dict,
				      kEAPOLClientControlCommand);
    if (get_number(command_cf, &command) == FALSE) {
	EAPLOG_FL(LOG_NOTICE, "%s: invalid/missing command",
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
    boolean_t			tell_supplicant = FALSE;

    if (source->is_wireless) {
	boolean_t	link_active;

	link_active = source->link_active;

	/* make sure our wireless information is up to date */
	EAPOLSocketSourceUpdateWirelessInfo(source, NULL);
	if (link_active != source->link_active) {
	    tell_supplicant = TRUE;
	}
    }
    else {
	source->link_active = is_link_active(source->if_name);
	tell_supplicant = TRUE;
    }
    if (tell_supplicant) {
	eapolclient_log(kLogFlagBasic, "link %s",
			source->link_active ? "active" : "inactive");
	/* let the 802.1X Supplicant know about the link status change */
	if (source->sock != NULL) {
	    if (source->link_active == FALSE) {
		/* toss last packet in case Authenticator re-uses identifier */
		EAPOLSocketSetEAPTxPacket(source->sock, NULL, 0);
	    }
	    Supplicant_link_status_changed(source->sock->supp,
					   source->link_active);
	}
    }
    return;
}

static void
EAPOLSocketSourceReceive(void * arg1, void * arg2)
{
    ALIGNED_BUF(buf, EAPOLSOCKET_RECV_BUFSIZE, uint32_t);
    EAPOLPacketRef		eapol_p;
    struct ether_header *	eh_p = (struct ether_header *)buf;
    uint16_t			ether_type;
    int				length;
    ssize_t 			n;
    EAPOLSocketReceiveDataRef	rx;
    EAPOLSocketRef		sock = NULL;
    EAPOLSocketSourceRef 	source = (EAPOLSocketSourceRef)arg1;

    n = recv(FDHandler_fd(source->handler), (char *)buf, sizeof(buf), 0);
    if (n <= 0) {
	if (n < 0) {
	    EAPLOG_FL(LOG_NOTICE, "recv failed %s", strerror(errno));
	}
	goto done;
    }
    if (n < sizeof(*eh_p)) {
	EAPLOG_FL(LOG_NOTICE, "Packet truncated (%d < %d)",
		  n, (int)sizeof(*eh_p));
	goto done;
    }
    ether_type = ntohs(eh_p->ether_type);
    switch (ether_type) {
    case EAPOL_802_1_X_ETHERTYPE:
    case IEEE80211_PREAUTH_ETHERTYPE:
	break;
    default:
	EAPLOG_FL(LOG_NOTICE, "Unexpected ethertype (%02x)", ether_type);
	goto done;
    }
    eapol_p = (void *)(eh_p + 1);
    length = (int)(n - sizeof(*eh_p));
    if (EAPOLPacketIsValid(eapol_p, length, NULL) == FALSE) {
	if (eapolclient_should_log(kLogFlagBasic)) {
	    CFMutableStringRef	log_msg;
	    
	    log_msg = CFStringCreateMutable(NULL, 0);
	    ether_header_print_to_string(log_msg, eh_p);
	    EAPOLPacketIsValid(eapol_p, length, log_msg);
	    EAPLOG(-LOG_DEBUG, "Ignoring Receive Packet Size %d\n%@",
		   n, log_msg);
	    CFRelease(log_msg);
	}
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
		EAPOLSocketSourceUpdateWirelessInfo(source,
						    (const struct ether_addr *)
						    eh_p->ether_shost);
	    }
	}
    }
    rx = &source->rx;
    rx->length = length;
    rx->eapol_p = eapol_p;
    if (eapolclient_should_log(kLogFlagPacketDetails)) {
	CFMutableStringRef	log_msg;

	log_msg = CFStringCreateMutable(NULL, 0);
	ether_header_print_to_string(log_msg, eh_p);
	EAPOLPacketIsValid(eapol_p, length, log_msg);
	EAPLOG(-LOG_DEBUG, "Receive Packet Size %d\n%@", n, log_msg);
	CFRelease(log_msg);
    }
    else if (eapolclient_should_log(kLogFlagBasic)) {
	EAPLOG(LOG_DEBUG,
	       "Receive Size %d Type 0x%04x From %s",
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
    if (sock != NULL) {
	EAPRequestPacketRef	req_p;
	bool			retransmit = FALSE;

	req_p = (EAPRequestPacketRef)eapol_p->body;
	if (sock->eap_tx_packet != NULL) {
	    if (eapol_p->packet_type == kEAPOLPacketTypeEAPPacket
		&& req_p->code == kEAPCodeRequest
		&& req_p->identifier == sock->eap_tx_packet->identifier) {
		retransmit = TRUE;
	    }
	    else {
		EAPOLSocketSetEAPTxPacket(sock, NULL, 0);
	    }
	}
	if (retransmit) {
	    eapolclient_log(kLogFlagBasic,
			    "Retransmit EAP packet %d bytes",
			    sock->eap_tx_packet_length);
	    EAPOLSocketSourceTransmit(sock->source, sock,
				      kEAPOLPacketTypeEAPPacket,
				      sock->eap_tx_packet,
				      sock->eap_tx_packet_length);
	}
	else if (S_receive_loss_percent != 0 
		 && S_simulated_event_occurred(S_receive_loss_percent)) {
	    /* drop the packet */
	    EAPLOG(LOG_NOTICE,
		   "Simulate receive packet loss: dropping %d bytes",
		   length);
	}
	else if (sock->func != NULL) {
	    (*sock->func)(sock->arg1, sock->arg2, rx);
	}
    }
    rx->eapol_p = NULL;

 done:
    return;
}

static int
EAPOLSocketSourceTransmit(EAPOLSocketSourceRef source,
			  EAPOLSocketRef sock,
			  EAPOLPacketType packet_type,
			  void * body, unsigned int body_length)
{
    ALIGNED_BUF(buf, EAPOLSOCKET_SEND_BUFSIZE, uint32_t);
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
		EAPOLSocketSourceUpdateWirelessInfo(source, NULL);
		if (source->bssid_valid == FALSE) {
		    /* bssid unknown, drop the packet */
		    eapolclient_log(kLogFlagBasic,
				    "Transmit: unknown BSSID,"
				    " not sending %d bytes",
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

    if (eapolclient_should_log(kLogFlagPacketDetails)) {
	CFMutableStringRef	log_msg;

	log_msg = CFStringCreateMutable(NULL, 0);
	ether_header_print_to_string(log_msg, eh_p);
	EAPOLPacketIsValid(eapol_p, body_length + sizeof(*eapol_p), log_msg);
	EAPLOG(-LOG_DEBUG, "Transmit Packet Size %d\n%@", 
	       body_length + sizeof(*eapol_p), log_msg);
	CFRelease(log_msg);
    }
    else if (eapolclient_should_log(kLogFlagBasic)) {
	EAPLOG(LOG_DEBUG,
	       "Transmit Size %d Type 0x%04x To %s",
	       body_length + sizeof(*eapol_p),
	       ntohs(eh_p->ether_type),
	       ether_ntoa((void *)eh_p->ether_dhost));
    }
    if (S_transmit_loss_percent != 0 
	&& S_simulated_event_occurred(S_transmit_loss_percent)) {
	EAPLOG(LOG_NOTICE,
	       "Simulating transmit packet loss: dropping %d bytes",
	       body_length);
    }
    else if (sendto(FDHandler_fd(source->handler), eh_p, size,
		    0, (struct sockaddr *)&ndrv, sizeof(ndrv)) < size) {
	EAPLOG(LOG_NOTICE, "EAPOLSocketSourceTransmit: sendto failed, %s",
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
	    eapolclient_log(kLogFlagBasic, "Removing Supplicant for %s",
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

PRIVATE_EXTERN EAPOLSocketSourceRef
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
	    EAPLOG_FL(LOG_NOTICE, "eapol_socket(%s) failed, %s",
		      strerror(errno));
	    goto failed;
	}
    }
    handler = FDHandler_create(fd);
    if (handler == NULL) {
	EAPLOG_FL(LOG_NOTICE, "FDHandler_create failed");
	goto failed;
    }

    source = malloc(sizeof(*source));
    if (source == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
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
	    EAPLOG_FL(LOG_NOTICE, "CFRunLoopObserverCreate failed");
	    goto failed;
	}
	scan_timer = Timer_create();
	if (scan_timer == NULL) {
	    EAPLOG_FL(LOG_NOTICE, "Timer_create failed");
	    goto failed;
	}
    }
    store = link_event_register(if_name, is_wireless,
				EAPOLSocketSourceLinkStatusChanged,
				source);
    if (store == NULL) {
	EAPLOG_FL(LOG_NOTICE, "link_event_register failed: %s",
		  SCErrorString(SCError()));
	goto failed;
    }
    TAILQ_INIT(&source->preauth_sockets);
    strlcpy(source->if_name, if_name, sizeof(source->if_name));
    source->if_name_length = (int)strlen(source->if_name);
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
	EAPLOG_FL(LOG_NOTICE, "EAPOLClientAttach(%s) failed: %s",
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
	eapolclient_log(kLogFlagBasic, "Removing Supplicant for %s",
			ether_ntoa(bssid));
    }
    Supplicant_free(&sock->supp);
    EAPOLSocketFree(&sock);
    return;
}

static void
EAPOLSocketSourceUpdateWirelessInfo(EAPOLSocketSourceRef source,
				    const struct ether_addr * rx_source_mac)
{
#ifdef NO_WIRELESS
    return;
#else /* NO_WIRELESS */
    struct ether_addr	ap_mac;
    bool 		ap_mac_valid = FALSE;

    if (source->is_wireless == FALSE) {
	return;
    }
    ap_mac_valid = wireless_ap_mac(source->wref, &ap_mac);
    if (ap_mac_valid == FALSE) {
	source->bssid_valid = FALSE;
	source->is_wpa_enterprise = FALSE;
	EAPOLSocketSourceUnscheduleHandshakeNotification(source);
	eapolclient_log(kLogFlagBasic, "Disassociated");
	my_CFRelease(&source->ssid);
	Timer_cancel(source->scan_timer);
	wireless_scan_cancel(source->wref);
	source->authenticated = FALSE;
	source->link_active = FALSE;
    }
    else {
	const struct ether_addr *	bssid;
	bool				changed;
	CFStringRef			ssid;
	
	/*
	 * Update the BSSID
	 *
	 * If we have a source MAC address i.e. we're being called from receive,
	 * use that as the BSSID instead of the value from the ioctl: the value
	 * from the ioctl could potentially be stale (see rdar://12350239).
	 *
	 * If we don't have a source MAC address, only update the BSSID if
	 * we don't know a value yet.
	 */
	source->link_active = TRUE;
	if (rx_source_mac != NULL) {
	    bssid = rx_source_mac;
	}
	else if (source->bssid_valid) {
	    return;
	}
	else {
	    bssid = &ap_mac;
	}
	if (source->bssid_valid == FALSE
	    || bcmp(bssid, &source->bssid, sizeof(source->bssid)) != 0) {
	    changed = TRUE;

	    if (S_enable_preauth) {
		/* remove any pre-auth socket with the new bssid */
		EAPOLSocketSourceRemoveSocketWithBSSID(source, bssid);
		if (source->bssid_valid == TRUE) {
		    /* we roamed */
		    EAPOLSocketSourceScheduleScan(source,
					      S_scan_delay_roam_secs);
		}
	    }
	}
	else {
	    changed = FALSE;
	}
	source->bssid_valid = TRUE;
	source->bssid = *bssid;
	ssid = wireless_copy_ssid_string(source->wref);
	source->is_wpa_enterprise = wireless_is_wpa_enterprise(source->wref);
	if (source->ssid != NULL && ssid != NULL
	    && !CFEqual(source->ssid, ssid)) {
	    EAPOLSocketSourceCancelScan(source);
	}
	my_CFRelease(&source->ssid);
	source->ssid = ssid;
	if (changed) {
	    eapolclient_log(kLogFlagBasic,
			    "Associated SSID %@ BSSID %s",
			    (source->ssid != NULL) ? source->ssid
			    : CFSTR("<unknown>"),
			    ether_ntoa(bssid));
	}
    }
    return;
#endif /* NO_WIRELESS */
}

PRIVATE_EXTERN void
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
	if (source->need_force_renew) {
	    /* 5900529: wait for 1/2 second before the force renew */
	    usleep(500 * 1000);
	    EAPOLSocketSourceForceRenew(source);
	}
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
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
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

PRIVATE_EXTERN SupplicantRef
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
	    EAPLOG(LOG_NOTICE, "%s: received stop command", source->if_name);
	    goto failed;
	}
	mode_cf = CFDictionaryGetValue(control_dict,
				       kEAPOLClientControlMode);
	if (mode_cf != NULL
	    && get_number(mode_cf, &mode) == FALSE) {
	    EAPLOG_FL(LOG_NOTICE, "%s: Mode property invalid",
		      source->if_name);
	    goto failed;
	}
	config_dict = CFDictionaryGetValue(control_dict,
					   kEAPOLClientControlConfiguration);
	if (config_dict == NULL) {
	    EAPLOG_FL(LOG_NOTICE, "%s: configuration empty", source->if_name);
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
    CFIndex		count;
    int			i;
    CFMutableStringRef	log_msg;

    count = CFArrayGetCount(bssid_list);
    log_msg = CFStringCreateMutable(NULL, 0);
    for (i = 0; i < count; i++) {
	CFDataRef			bssid_data;
	const struct ether_addr *	bssid;
	
	bssid_data = CFArrayGetValueAtIndex(bssid_list, i);
	bssid = (const struct ether_addr *)CFDataGetBytePtr(bssid_data);
	STRING_APPEND(log_msg, "%s%s", (i == 0) ? "" : ", ",
		      ether_ntoa(bssid));
    }
    EAPLOG(-LOG_DEBUG, "Scan complete: %d AP%s = { %@ }", 
	   count, (count == 1) ? "" : "s", log_msg);
    CFRelease(log_msg);
    return;
}

static void
EAPOLSocketSourceScanCallback(wireless_t wref,
			      CFArrayRef bssid_list, void * arg)
{
    EAPOLSocketSourceRef	source = (EAPOLSocketSourceRef)arg;

    if (bssid_list == NULL) {
	eapolclient_log(kLogFlagBasic, "Scan complete: no APs");
    }
    else if (source->bssid_valid == FALSE) {
	EAPLOG(LOG_NOTICE, "main Supplicant bssid is unknown, skipping");
    }
    else {
	CFIndex	count;
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
		EAPLOG_FL(LOG_NOTICE, "Supplicant create %s failed",
			  ether_ntoa(&sock->bssid));
		EAPOLSocketFree(&sock);
	    }
	    else {
		eapolclient_log(kLogFlagBasic,
				"Supplicant %s created",
				ether_ntoa(&sock->bssid));
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
	eapolclient_log(kLogFlagBasic, "Scan initiated");
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

    eapolclient_log(kLogFlagBasic, "4-way handshake complete");
    supplicant_state = Supplicant_get_state(source->sock->supp, &client_status);
    switch (supplicant_state) {
    case kSupplicantStateAuthenticated:
	if (source->need_force_renew) {
	    EAPOLSocketSourceForceRenew(source);
	}
	break;
    case kSupplicantStateAuthenticating:
	/* if we're still authenticating, we likely lost the EAP Success */
	Supplicant_simulate_success(source->sock->supp);
	break;
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
	if (source->authenticated == FALSE) {
	    /* only need force renew the first time after the link goes up */
	    source->need_force_renew = TRUE;
	}
	else {
	    source->need_force_renew = FALSE;
	}
	source->authenticated = TRUE;
	eapolclient_log(kLogFlagBasic,
			"4-way handshake notification scheduled");
    }
    return;
}

static void
EAPOLSocketSourceUnscheduleHandshakeNotification(EAPOLSocketSourceRef source)
{
    if (EAPOLSocketSourceReleaseHandshakeNotification(source)) {
	eapolclient_log(kLogFlagBasic,
			"4-way handshake notification unscheduled");
    }
    return;
}
