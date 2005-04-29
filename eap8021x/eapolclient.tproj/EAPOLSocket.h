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

#ifndef _S_EAPOLSOCKET_H
#define _S_EAPOLSOCKET_H


#include <EAP8021X/EAPOL.h>
#include "wireless.h"

typedef struct EAPOLSocket_s EAPOLSocket;

typedef struct {
    EAPOLPacket *		eapol_p;
    unsigned int		length;
    struct sockaddr_dl *	source_address_p;
    boolean_t			logged;
} EAPOLSocketReceiveData, *EAPOLSocketReceiveDataRef;

typedef void (EAPOLSocketReceiveCallback)(void * arg1, void * arg2, 
					  EAPOLSocketReceiveData * data);
int
eapol_socket(char * ifname, boolean_t blocking);

void
EAPOLSocketSetDebug(boolean_t debug);

void
EAPOLSocket_free(EAPOLSocket * * eapol_p);

EAPOLSocket *
EAPOLSocket_create(int fd, const struct sockaddr_dl * link);

int
EAPOLSocket_mtu(EAPOLSocket * sock);

boolean_t
EAPOLSocket_is_wireless(EAPOLSocket * sock);

boolean_t
EAPOLSocket_set_key(EAPOLSocket * sock, wirelessKeyType type,
		    int index, char * key, int key_length);

boolean_t
EAPOLSocket_set_wpa_session_key(EAPOLSocket * sock, char * key, int key_length);

boolean_t
EAPOLSocket_set_wpa_server_key(EAPOLSocket * sock, char * key, int key_length);

void
EAPOLSocket_link_update(EAPOLSocket * sock);

void
EAPOLSocket_disable_receive(EAPOLSocket * eapol_socket);

void
EAPOLSocket_enable_receive(EAPOLSocket * eapol_socket,
			   EAPOLSocketReceiveCallback * func,
			   void * arg1, void * arg2);

int
EAPOLSocket_transmit(EAPOLSocket * sock,
		     EAPOLPacketType packet_type,
		     void * body, unsigned int body_length,
		     struct sockaddr_dl * dest,
		     boolean_t print_whole_packet);

const char *
EAPOLSocket_if_name(EAPOLSocket * sock, uint32_t * length);

void
eapol_packet_print(EAPOLPacket * eapol_p, unsigned int length);

void
eap_packet_print(EAPPacketRef pkt_p, unsigned int length);

#endif _S_EAPOLSOCKET_H

