/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 *  pptp.h
 *  ppp
 *
 *  Created by Christophe Allie on Thu May 23 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

#define PPTP_NKE	"PPTP.kext"
#define PPTP_NKE_ID	"com.apple.nke.pptp"

/* PPTP exit codes */
#define EXIT_PPTP_NOSERVER  		1
#define EXIT_PPTP_NOANSWER  		2
#define EXIT_PPTP_PROTOCOLERROR 	5
#define EXIT_PPTP_NETWORKCHANGED 	6
#if TARGET_OS_EMBEDDED
#define EXIT_PPTP_NOEDGE			7
#endif

/* define pptp messages */
#define PPTP_CONTROL_MSG	1
#define PPTP_MANAGEMENT_MSG	2

/* define well known values */
#define PPTP_MAGIC_COOKIE	0x1A2B3C4D	/* magic PPTP cookie */
#define PPTP_VERSION		0x0100		/* PPTP vervion number */
#define PPTP_TCP_PORT		1723		/* well know pptp port */
#define PPTP_RECEIVE_WINDOW	0x40		/* let's have a window size of 64 */

/* generic results codes */
#define PPTP_RESULT_SUCCESS	1
#define PPTP_RESULT_ERROR	2

/*outgoing call result codes */
#define PPTP_OUTGOING_CALL_RESULT_CONNECTED	1
#define PPTP_OUTGOING_CALL_RESULT_ERROR		2
#define PPTP_OUTGOING_CALL_RESULT_NOCARRIER	3
#define PPTP_OUTGOING_CALL_RESULT_BUSY		4
#define PPTP_OUTGOING_CALL_RESULT_NODIALTONE	5
#define PPTP_OUTGOING_CALL_RESULT_TIMEOUT	6
#define PPTP_OUTGOING_CALL_RESULT_DONOTACCEPT	7

/* define pptp control messages */
#define PPTP_START_CONTROL_CONNECTION_REQUEST	1
#define PPTP_START_CONTROL_CONNECTION_REPLY	2
#define PPTP_STOP_CONTROL_CONNECTION_REQUEST	3
#define PPTP_STOP_CONTROL_CONNECTION_REPLY	4
#define PPTP_ECHO_REQUEST			5
#define PPTP_ECHO_REPLY				6
#define PPTP_OUTGOING_CALL_REQUEST		7
#define PPTP_OUTGOING_CALL_REPLY		8
#define PPTP_INCOMING_CALL_REQUEST		9
#define PPTP_INCOMING_CALL_REPLY		10
#define PPTP_INCOMING_CALL_CONNECTED		11
#define PPTP_CALL_CLEAR_REQUEST			12
#define PPTP_CALL_DISCONNECT_NOTIFY		13
#define PPTP_WAN_ERROR_NOTIFY			14
#define PPTP_SET_LINK_INFO			15

/* define framing capabilities */
#define PPTP_ASYNC_FRAMING	1
#define PPTP_SYNC_FRAMING	2

/* define bearer capabilities */
#define PPTP_ANALOG_ACCESS	1
#define PPTP_DIGITAL_ACCESS	2


#define PPTP_VENDOR 	"Mac OS X, Apple Computer, Inc"

struct pptp_header {
    /* header part */
    u_int16_t	len;
    u_int16_t	pptp_msgtype;
    u_int32_t	magic_cookie;
    u_int16_t	ctrl_msgtype;
    u_int16_t	reserved0;
};

struct pptp_start_control_request {
    /* message part */
    u_int16_t	proto_vers;
    u_int16_t	reserved1;
    u_int32_t	framing_caps;
    u_int32_t	bearer_caps;
    u_int16_t	max_channels;
    u_int16_t	firmware_rev;
    u_int8_t	hostname[64];
    u_int8_t	vendor[64];
};

struct pptp_start_control_reply {
    /* message part */
    u_int16_t	proto_vers;
    u_int8_t	result_code;
    u_int8_t	error_code;
    u_int32_t	framing_caps;
    u_int32_t	bearer_caps;
    u_int16_t	max_channels;
    u_int16_t	firmware_rev;
    u_int8_t	hostname[64];
    u_int8_t	vendor[64];
};

struct pptp_outgoing_call_request {
    /* message part */
    u_int16_t	call_id;
    u_int16_t	serial_number;
    u_int32_t	min_bps;
    u_int32_t	max_bps;
    u_int32_t	bearer_type;
    u_int32_t	framing_type;
    u_int16_t	recv_window;
    u_int16_t	processing_delay;
    u_int16_t	phone_len;
    u_int16_t	reserved1;
    u_int8_t	phone[64];
    u_int8_t	subaddress[64];
};

struct pptp_outgoing_call_reply {
    /* message part */
    u_int16_t	call_id;
    u_int16_t	peer_call_id;
    u_int8_t	result_code;
    u_int8_t	error_code;
    u_int16_t	cause_code;
    u_int32_t	connect_speed;
    u_int16_t	recv_window;
    u_int16_t	processing_delay;
    u_int32_t	phys_channel_id;
};

struct pptp_call_clear_request {
    /* message part */
    u_int16_t	call_id;
    u_int16_t	reserved1;
};

struct pptp_set_link_info {
    /* message part */
    u_int16_t	peer_call_id;
    u_int16_t	reserved1;
    u_int32_t	send_accm;
    u_int32_t	recv_accm;
};

struct pptp_echo_request {
    /* message part */
    u_int32_t	identifier;
};

struct pptp_echo_reply {
    /* message part */
    u_int32_t	identifier;
    u_int8_t	result_code;
    u_int8_t	error_code;
    u_int16_t	reserved1;
};


/* pptp functions */

int pptp_outgoing_call(int fd, 
    u_int16_t ourcallid, u_int16_t ourwindow, u_int16_t ourppd,
    u_int16_t *peercallid, u_int16_t *peerwindow, u_int16_t *peerppd);

int pptp_incoming_call(int fd,
    u_int16_t ourcallid, u_int16_t ourwindow, u_int16_t ourppd,
    u_int16_t *peercallid, u_int16_t *peerwindow, u_int16_t *peerppd);

int pptp_echo(int fd, u_int32_t identifier);

int pptp_data_in(int fd);
