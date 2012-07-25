/*
 * Copyright (c) 2001-2012 Apple Inc. All rights reserved.
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


#include "EAPOL.h"
#include "EAPOLControlTypes.h"
#include "wireless.h"
#include "ClientControlInterface.h"

typedef struct EAPOLSocket_s EAPOLSocket, * EAPOLSocketRef;

typedef struct {
    EAPOLPacket *		eapol_p;
    unsigned int		length;
} EAPOLSocketReceiveData, *EAPOLSocketReceiveDataRef;

typedef void (EAPOLSocketReceiveCallback)(void * arg1, void * arg2, 
					  EAPOLSocketReceiveDataRef data);
typedef EAPOLSocketReceiveCallback * EAPOLSocketReceiveCallbackRef;

void
EAPOLSocketSetDebug(boolean_t debug);

bool
EAPOLSocketIsLinkActive(EAPOLSocketRef sock);

int
EAPOLSocketMTU(EAPOLSocketRef sock);

const struct ether_addr *
EAPOLSocketGetAuthenticatorMACAddress(EAPOLSocketRef sock);

boolean_t
EAPOLSocketIsWireless(EAPOLSocketRef sock);

boolean_t
EAPOLSocketSetKey(EAPOLSocketRef sock, wirelessKeyType type,
		  int index, const uint8_t * key, int key_length);

boolean_t
EAPOLSocketSetWPAKey(EAPOLSocketRef sock, 
		     const uint8_t * session_key, int session_key_length,
		     const uint8_t * server_key, int server_key_length);

void
EAPOLSocketClearPMKCache(EAPOLSocketRef sock);

CFStringRef
EAPOLSocketGetSSID(EAPOLSocketRef sock);

void
EAPOLSocketDisableReceive(EAPOLSocketRef eapol_socket);

void
EAPOLSocketEnableReceive(EAPOLSocketRef eapol_socket,
			 EAPOLSocketReceiveCallback * func,
			 void * arg1, void * arg2);

int
EAPOLSocketTransmit(EAPOLSocketRef sock,
		    EAPOLPacketType packet_type,
		    void * body, unsigned int body_length);

const char *
EAPOLSocketIfName(EAPOLSocketRef sock, uint32_t * length);

const char *
EAPOLSocketName(EAPOLSocketRef sock);

void
EAPOLSocketReportStatus(EAPOLSocketRef sock, CFDictionaryRef status_dict);

EAPOLControlMode
EAPOLSocketGetMode(EAPOLSocketRef sock);

void
EAPOLSocketStopClient(EAPOLSocketRef sock);

boolean_t
EAPOLSocketReassociate(EAPOLSocketRef sock);

int
get_plist_int(CFDictionaryRef plist, CFStringRef key, int def);

#endif /* _S_EAPOLSOCKET_H */

