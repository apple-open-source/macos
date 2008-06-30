
/*
 * Copyright (c) 2001-2004 Apple Computer, Inc. All rights reserved.
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
 */

/*
 * EAPOLSocket.c
 * - "object" that wraps access to EAP over LAN
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/dlil.h>
#include <net/ndrv.h>
#include <net/ethernet.h>

#include "ndrv_socket.h"
#include <EAP8021X/EAPUtil.h>
#include "FDHandler.h"
#include "EAPOLSocket.h"
#ifndef NO_WIRELESS
#include "wireless.h"
#endif NO_WIRELESS
#include "mylog.h"
#include "printdata.h"
#include "convert.h"

#define EAPOL_802_1_X_FAMILY	0x8021ec /* XXX needs official number! */

#define EAPOLSOCKET_RECV_BUFSIZE	1600

static const struct ether_addr eapol_multicast = {
    EAPOL_802_1_X_GROUP_ADDRESS
};

struct EAPOLSocket_s {
    struct sockaddr_dl *		dl_p;
    FDHandler *				handler;
    EAPOLSocketReceiveCallback *	func;
    void *				arg1;
    void *				arg2;
    char				if_name[IF_NAMESIZE + 1];
    int					if_name_length;

    boolean_t				bssid_valid;
    struct ether_addr			bssid;
    int					mtu;
    boolean_t				is_wireless;
#ifndef NO_WIRELESS
    wireless_t				wref;
#endif NO_WIRELESS
    EAPOLSocketReceiveData		rx;
};

static boolean_t	S_debug = FALSE;

static boolean_t
eapol_socket_add_multicast(int s)
{
    struct sockaddr_dl		dl;

    bzero(&dl, sizeof(dl));
    dl.sdl_len = sizeof(dl);
    dl.sdl_family = AF_LINK;
    dl.sdl_type = IFT_ETHER; /* XXX needs to be made generic */
    dl.sdl_nlen = 0;
    dl.sdl_alen = sizeof(eapol_multicast);
    bcopy(&eapol_multicast,
	  dl.sdl_data, 
	  sizeof(eapol_multicast));
    if (ndrv_socket_add_multicast(s, &dl) < 0) {
	my_log(LOG_NOTICE, "eapol_socket: ndrv_socket_add_multicast failed, %s",
	       strerror(errno));
	return (FALSE);
    }
    return (TRUE);
}

int
eapol_socket(char * ifname, boolean_t blocking)
{
    int 			s;

    s = ndrv_socket(ifname);
    if (s < 0) {
	my_log(LOG_NOTICE, "eapol_socket: ndrv_socket failed");
	goto failed;
    }
    if (blocking == FALSE) {
	int opt = 1;

	if (ioctl(s, FIONBIO, &opt) < 0) {
	    my_log(LOG_NOTICE, "eapol_socket: FIONBIO failed, %s", 
		   strerror(errno));
	    goto failed;
	}
    }
    if (ndrv_socket_bind(s, EAPOL_802_1_X_FAMILY, 
			 EAPOL_802_1_X_ETHERTYPE) < 0) {
	my_log(LOG_NOTICE, "eapol_socket: ndrv_socket_bind failed, %s",
	       strerror(errno));
	goto failed;
    }
    return (s);
 failed:
    if (s >= 0) {
	close(s);
    }
    return (-1);
    
}

static boolean_t
EAPOLKeyDescriptorTypeValid(EAPOLKeyDescriptorType descriptor_type) 
{
    switch (descriptor_type) {
    case kEAPOLKeyDescriptorTypeRC4:
	return (TRUE);
	break;
    default:
	break;
    }
    return (FALSE);
}

static const char *
EAPOLKeyDescriptorTypeStr(EAPOLKeyDescriptorType descriptor_type)
{
    switch (descriptor_type) {
    case kEAPOLKeyDescriptorTypeRC4:
	return ("RC4");
	break;
    default:
	break;
    }
    return ("<unknown>");
    
}

static boolean_t
EAPOLPacketTypeValid(EAPOLPacketType type)
{
    if (type >= kEAPOLPacketTypeEAPPacket
	&& type <= kEAPOLPacketTypeEncapsulatedASFAlert) {
	return (TRUE);
    }
    return (FALSE);
}

static const char *
EAPOLPacketTypeStr(EAPOLPacketType type)
{
    static const char * str[] = { 
	"EAP Packet",
	"Start",
	"Logoff",
	"Key",
	"Encapsulated ASF Alert" 
    };

    if (EAPOLPacketTypeValid(type)) {
	return (str[type]);
    }
    return ("<unknown>");
}

void
eap_packet_print(EAPPacketRef pkt_p, unsigned int body_length)
{
    EAPPacketValid(pkt_p, body_length, TRUE);
}

static void
print_eapol_key_descriptor(void * body, unsigned int body_length)
{
    EAPOLKeyDescriptor *	descr_p = body;
    int				key_data_length;
    u_int16_t			key_length;
    const char *		which;
    
    if (descr_p->key_index & kEAPOLKeyDescriptorIndexUnicastFlag) {
	which = "Unicast";
    }
    else {
	which = "Broadcast";
    }
    key_length = EAPOLKeyDescriptorGetLength(descr_p);
    key_data_length = body_length - sizeof(*descr_p);
    printf("EAPOL Key Descriptor: type %s (%d) length %d %s index %d\n",
	   EAPOLKeyDescriptorTypeStr(descr_p->descriptor_type),
	   descr_p->descriptor_type, 
	   key_length, 
	   which,
	   descr_p->key_index & kEAPOLKeyDescriptorIndexMask);
    printf("%-16s", "replay_counter:");
    print_bytes(descr_p->replay_counter, sizeof(descr_p->replay_counter));
    printf("\n");
    printf("%-16s", "key_IV:");
    print_bytes(descr_p->key_IV, sizeof(descr_p->key_IV));
    printf("\n");
    printf("%-16s", "key_signature:");
    print_bytes(descr_p->key_signature, sizeof(descr_p->key_signature));
    printf("\n");
    if (key_data_length > 0) {
	printf("%-16s", "key:");
	print_bytes(descr_p->key, key_data_length);
	printf("\n");
    }
    return;
}

static boolean_t
eapol_key_descriptor_valid(void * body, unsigned int body_length, 
			   boolean_t debug)
{
    EAPOLKeyDescriptor *	descr_p = body;

    if (body_length < sizeof(*descr_p)) {
	if (debug) {
	    printf("eapol_key_descriptor_valid: body_length %d"
		   " < sizeof(*descr_p) %ld\n",
		   body_length, sizeof(*descr_p));
	}
	return (FALSE);
    }
    if (EAPOLKeyDescriptorTypeValid(descr_p->descriptor_type) == FALSE) {
	if (debug) {
	    printf("eapol_key_descriptor_valid: descriptor_type invalid %d",
		   descr_p->descriptor_type);
	}
	return (FALSE);
    }
    if (debug) {
	print_eapol_key_descriptor(body, body_length);
    }
    return (TRUE);
}

static void
ether_header_print(struct ether_header * eh_p)
{
    printf("\nEther packet: dest %s ",
	   ether_ntoa((void *)eh_p->ether_dhost));
    printf("source %s type 0x%04x\n", 
	   ether_ntoa((void *)eh_p->ether_shost),
	   eh_p->ether_type);
    return;
}

static boolean_t
ether_header_valid(struct ether_header * eh_p, unsigned int length,
		   boolean_t debug)
{
    if (length < sizeof(*eh_p)) {
	if (debug) {
	    printf("Packet length %d < sizeof(*eh_p) %ld\n",
		   length, sizeof(*eh_p));
	    print_data((void *)eh_p, length);
	}
	return (FALSE);
    }
    if (debug) {
	ether_header_print(eh_p);
    }
    if (ntohs(eh_p->ether_type) != EAPOL_802_1_X_ETHERTYPE) {
	if (S_debug) {
	    printf("Ethertype != 802.1x ethertype (%02x)\n",
		   EAPOL_802_1_X_ETHERTYPE);
	}
	return (FALSE);
    }
    return (TRUE);
}

static boolean_t
eapol_body_valid(EAPOLPacket * eapol_p, unsigned int length, boolean_t debug)
{
    unsigned int 	body_length;
    boolean_t 		ret = TRUE;

    body_length = EAPOLPacketGetLength(eapol_p);
    length -= sizeof(*eapol_p);
    if (length < body_length) {
	if (debug) {
	    printf("packet length %d < body_length %d\n",
		   length, body_length);
	}
	return (FALSE);
    }
    switch (eapol_p->packet_type) {
    case kEAPOLPacketTypeEAPPacket:
	ret = EAPPacketValid((EAPPacketRef)eapol_p->body, body_length, debug);
	break;
    case kEAPOLPacketTypeKey:
	ret = eapol_key_descriptor_valid(eapol_p->body, body_length, debug);
	break;
    case kEAPOLPacketTypeStart:
    case kEAPOLPacketTypeLogoff:
    case kEAPOLPacketTypeEncapsulatedASFAlert:
	break;
    default:
	if (debug) {
	    printf("unrecognized EAPOL packet type %d\n",
		   eapol_p->packet_type);
	    print_data(((void *)eapol_p) + sizeof(*eapol_p), body_length);
	}
	break;
    }

    if (debug) {
	if (body_length < length) {
	    printf("EAPOL: %d bytes follow body:\n", length - body_length);
	    print_data(((void *)eapol_p) + sizeof(*eapol_p) + body_length, 
		       length - body_length);
	}
    }
    return (ret);
}

static boolean_t
eapol_header_valid(EAPOLPacket * eapol_p, unsigned int length, boolean_t debug)
{
    if (length < sizeof(*eapol_p)) {
	if (debug) {
	    printf("Data length %d < sizeof(*eapol_p) %ld\n",
		   length, sizeof(*eapol_p));
	}
	return (FALSE);
    }
    if (debug) {
	printf("EAPOL: proto version 0x%x type %s (%d) length %d\n",
	       eapol_p->protocol_version, 
	       EAPOLPacketTypeStr(eapol_p->packet_type),
	       eapol_p->packet_type, EAPOLPacketGetLength(eapol_p));
    }
    return (TRUE);
}

static boolean_t
eapol_packet_valid(EAPOLPacket * eapol_p, unsigned int length, boolean_t debug)
{
    if (eapol_header_valid(eapol_p, length, debug) == FALSE) {
	return (FALSE);
    }
    return (eapol_body_valid(eapol_p, length, debug));
}

void
eapol_packet_print(EAPOLPacket * eapol_p, unsigned int length)
{
    (void)eapol_body_valid(eapol_p, length, TRUE);
    return;
}

void
EAPOLSocketSetDebug(boolean_t debug)
{
    S_debug = debug;
    return;
}

void
EAPOLSocket_receive(void * arg1, void * arg2)
{
    char		buf[EAPOLSOCKET_RECV_BUFSIZE];
    int			length;
    int 		n;
    EAPOLSocket * 	sock = arg1;

    n = recv(FDHandler_fd(sock->handler), 
	     buf, EAPOLSOCKET_RECV_BUFSIZE, 0);
    if (n > 0) {
	EAPOLSocketReceiveDataRef	rx = &sock->rx;
	EAPOLPacket *			eapol_p;
	struct ether_header *		eh_p = (struct ether_header *)buf;
	boolean_t 			valid;
	
	if (S_debug) {
	    printf("\n"
		   "----------------------------------------\n");
	    timestamp_fprintf(stdout, "Receive Packet Size: %d\n", n);
	}
	do { /* while (0) */
	    if (ether_header_valid(eh_p, n, S_debug) == FALSE) {
		break;
	    }
	    eapol_p = (void *)(eh_p + 1);
	    length = n - sizeof(*eh_p);
	    if (eapol_header_valid(eapol_p, length, S_debug) == FALSE) {
		break;
	    }
	    valid = eapol_body_valid(eapol_p, length, FALSE);
	    if (valid == FALSE || sock->func == NULL) {
		if (S_debug) {
		    if (valid == FALSE) {
			printf("Invalid packet:\n");
		    }
		    else {
			printf("No receive func installed\n");
		    }
		    eapol_body_valid(eapol_p, length, TRUE);
		}
		break;
	    }
	    eh_p = (struct ether_header *)buf;
	    if (sock->is_wireless) {
		if (sock->bssid_valid == FALSE
		    || bcmp(eh_p->ether_shost, &sock->bssid,
			    sizeof(eh_p->ether_shost)) != 0) {
		    EAPOLSocket_link_update(sock);
		}
	    }
	    rx->length = length;
	    rx->eapol_p = eapol_p;
	    rx->logged = FALSE;
	    (*sock->func)(sock->arg1,
			  sock->arg2,
			  rx);
	    if (rx->logged == FALSE && S_debug) {
		eapol_packet_print(eapol_p, length);
	    }
	    rx->eapol_p = NULL;
	} while (0);
	if (S_debug) {
	    fflush(stdout);
	    fflush(stderr);
	}
    }
    else if (n < 0) {
	my_log(LOG_NOTICE, "EAPOLSocket_receive: recv failed %s",
	       strerror(errno));
    }
    return;
}

const char *
EAPOLSocket_if_name(EAPOLSocket * sock, uint32_t * name_length)
{
    if (name_length != NULL) {
	*name_length = sock->if_name_length;
    }
    return (sock->if_name);
}

EAPOLSocket *
EAPOLSocket_create(int fd, const struct sockaddr_dl * link)
{
#ifndef NO_WIRELESS
    struct ether_addr		ap_mac;
    boolean_t			ap_mac_valid = FALSE;
#endif NO_WIRELESS
    struct sockaddr_dl * 	dl_p = NULL;
    boolean_t			is_wireless = FALSE;
    FDHandler *			handler = NULL;
    EAPOLSocket *		sock = NULL;

    sock = malloc(sizeof(*sock));
    if (sock == NULL) {
	my_log(LOG_NOTICE, "EAOLSocket_create: malloc failed");
	goto failed;
    }
    bzero(sock, sizeof(*sock));
    sock->mtu = 1400; /* XXX - needs to be made generic */
    bcopy(link->sdl_data, sock->if_name, link->sdl_nlen);
    sock->if_name[link->sdl_nlen] = '\0';
    sock->if_name_length = link->sdl_nlen;

#ifndef NO_WIRELESS
    /* is this a wireless interface? */
    if (link->sdl_type == IFT_ETHER) {
	if (wireless_bind(sock->if_name, &sock->wref)) {
	    is_wireless = TRUE;
	    ap_mac_valid = wireless_ap_mac(sock->wref, &ap_mac);
	}
    }
#endif NO_WIRELESS

    /* only add the multicast reception for non-wireless LAN's */
    if (is_wireless == FALSE 
	&& eapol_socket_add_multicast(fd) == FALSE) {
	goto failed;
    }

    handler = FDHandler_create(fd);
    if (handler == NULL) {
	my_log(LOG_NOTICE, "EAPOLSocket_create: FDHandler_create failed");
	goto failed;
    }
    FDHandler_enable(handler, EAPOLSocket_receive, sock, NULL);

    dl_p = malloc(link->sdl_len);
    if (dl_p == NULL) {
	my_log(LOG_NOTICE, "EAPOLSocket_create: malloc failed");
	goto failed;
    }
    bcopy(link, dl_p, link->sdl_len);
    sock->handler = handler;
    sock->dl_p = dl_p;
    sock->is_wireless = is_wireless;

#ifndef NO_WIRELESS
    if (is_wireless && ap_mac_valid) {
	/* if we know the access point's address, save it */
	sock->bssid = ap_mac;
	sock->bssid_valid = TRUE;
    }
#endif NO_WIRELESS
    return (sock);
 failed:
    if (sock != NULL) {
	free(sock);
    }
    if (dl_p != NULL) {
	free(dl_p);
    }
    if (handler != NULL) {
	FDHandler_free(&handler);
	fd = -1;
    }
    if (fd >= 0) {
	close(fd);
    }
    return (NULL);
}

boolean_t
EAPOLSocket_is_wireless(EAPOLSocket * sock)
{
    return (sock->is_wireless);
}

boolean_t
EAPOLSocket_link_update(EAPOLSocket * sock)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else NO_WIRELESS
    struct ether_addr	ap_mac;
    boolean_t 		ap_mac_valid = FALSE;
    boolean_t		changed = FALSE;

    if (sock->is_wireless == FALSE) {
	return (FALSE);
    }

    ap_mac_valid = wireless_ap_mac(sock->wref, &ap_mac);
    if (ap_mac_valid == FALSE) {
	my_log(LOG_DEBUG, "EAPOLSocket_link_update: no longer associated");
	changed = sock->bssid_valid;
	sock->bssid_valid = FALSE;
    }
    else {
	if (sock->bssid_valid == FALSE
	    || bcmp(&ap_mac, &sock->bssid, sizeof(ap_mac)) != 0) {
	    changed = TRUE;
	}
	sock->bssid_valid = TRUE;
	sock->bssid = ap_mac;
	my_log(LOG_DEBUG, "EAPOLSocket_link_update: AP address %s",
	       ether_ntoa(&ap_mac));
    }
    return (changed);
#endif NO_WIRELESS
}

void
EAPOLSocket_free(EAPOLSocket * * sock_p)
{
    EAPOLSocket * sock;

    if (sock_p == NULL) {
	return;
    }
    sock = *sock_p;

    if (sock) {
	if (sock->dl_p) {
	    free(sock->dl_p);
	    sock->dl_p = NULL;
	}
	FDHandler_free(&sock->handler);
#ifndef NO_WIRELESS
	if (sock->is_wireless) {
	    wireless_free(sock->wref);
	}
#endif NO_WIRELESS
	free(sock);
    }
    *sock_p = NULL;
    return;
}

boolean_t
EAPOLSocket_set_key(EAPOLSocket * sock, wirelessKeyType type, 
		    int index, const uint8_t * key, int key_length)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else NO_WIRELESS
    if (sock->is_wireless == FALSE) {
	return (FALSE);
    }
    return (wireless_set_key(sock->wref, type, index, key, key_length));
#endif NO_WIRELESS
}

int
EAPOLSocket_mtu(EAPOLSocket * sock)
{
    return (sock->mtu);
}

void
EAPOLSocket_enable_receive(EAPOLSocket * sock,
			   EAPOLSocketReceiveCallback * func,
			   void * arg1, void * arg2)
{
    sock->func = func;
    sock->arg1 = arg1;
    sock->arg2 = arg2;
    return;
}

void
EAPOLSocket_disable_receive(EAPOLSocket * sock)
{
    sock->func = NULL;
    return;
}

int
EAPOLSocket_transmit(EAPOLSocket * sock,
		     EAPOLPacketType packet_type,
		     void * body, unsigned int body_length,
		     struct sockaddr_dl * dest,
		     boolean_t print_whole_packet)
{
    char			buf[1600];
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

    if (dest != NULL) {
	bcopy(dest->sdl_data + dest->sdl_nlen, 
	      &eh_p->ether_dhost, sizeof(eh_p->ether_dhost));
    }
    else {
	if (sock->is_wireless && sock->bssid_valid) {
	    bcopy(&sock->bssid, &eh_p->ether_dhost,
		  sizeof(eh_p->ether_dhost));
	}
	else {
	    bcopy(&eapol_multicast, &eh_p->ether_dhost, 
		  sizeof(eh_p->ether_dhost));
	}
    }
    bcopy(sock->dl_p->sdl_data + sock->dl_p->sdl_nlen,
	  eh_p->ether_shost, 
	  sizeof(eh_p->ether_shost));
    eh_p->ether_type = htons(EAPOL_802_1_X_ETHERTYPE);
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
	EAPOLSocketReceiveDataRef	rx = &sock->rx;

	/* if we haven't logged the receive packet, log it now */
	if (rx->eapol_p != NULL && rx->logged == FALSE) {
	    eapol_packet_print(rx->eapol_p, rx->length);
	    rx->logged = TRUE;
	}

	printf("\n"
	       "========================================\n");
	timestamp_fprintf(stdout, "Transmit Packet Size %d\n", size);
	ether_header_valid(eh_p, size, TRUE);
	if (print_whole_packet) {
	    eapol_packet_valid(eapol_p, body_length + sizeof(*eapol_p), TRUE);
	}
	else {
	    eapol_header_valid(eapol_p, body_length + sizeof(*eapol_p), TRUE);
	}
	fflush(stdout);
	fflush(stderr);
    }
    if (sendto(FDHandler_fd(sock->handler), eh_p, size, 
	       0, (struct sockaddr *)&ndrv, sizeof(ndrv)) < size) {
	my_log(LOG_NOTICE, "EAPOLSocket_receive: sendto failed, %s",
	       strerror(errno));
	goto failed;
    }
    return (0);

 failed:
    return (-1);
}

boolean_t
EAPOLSocket_set_wpa_session_key(EAPOLSocket * sock, 
				const uint8_t * key, int key_length)
{
#ifdef NO_WIRELESS
    return (FALSE);
#else NO_WIRELESS
    if (sock->is_wireless == FALSE) {
	return (FALSE);
    }
    /* tell the driver the session key used for WPA (if applicable) */
    return (wireless_set_wpa_session_key(sock->wref, key, key_length));
#endif NO_WIRELESS
}
