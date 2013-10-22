/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __L2TPK_H__
#define __L2TPK_H__

#define L2TP_UDP_PORT 		1701

#define PPPPROTO_L2TP		18		/* TEMP - move to ppp.h - 1..32 are reserved */
#define L2TP_NAME		"L2TP"		/* */

#ifdef KERNEL
SYSCTL_DECL(_net_ppp_l2tp);
#endif

/* some default values */
#define L2TP_DEFAULT_WINDOW_SIZE	4	/* default window size for both sides */
#define L2TP_DEFAULT_INITIAL_TIMEOUT	1		/* 1 seconds */
#define L2TP_DEFAULT_TIMEOUT_CAP	4		/* 4 seconds */
#define L2TP_DEFAULT_RETRY_COUNT	9	
#define L2TP_DEFAULT_CONNECT_TIMEOUT		1	/* 1 seconds */
#define L2TP_DEFAULT_CONNECT_RETRY_COUNT	60	/* 60 tries */

#define L2TP_OPT_FLAGS			1	/* see flags definition below */
#define L2TP_OPT_PEERADDRESS		2	/* peer IP address */
#define L2TP_OPT_TUNNEL_ID		3	/* tunnel id for the connection */
#define L2TP_OPT_NEW_TUNNEL_ID		4	/* create a new tunnel id for the connection */
#define L2TP_OPT_PEER_TUNNEL_ID		5	/* peer tunnel id for the connection */
#define L2TP_OPT_SESSION_ID		6	/* session id for the connection */
#define L2TP_OPT_PEER_SESSION_ID	7	/* peer session id for the connection */
#define L2TP_OPT_WINDOW			8	/* our receive window */
#define L2TP_OPT_PEER_WINDOW		9	/* peer receive window */
#define L2TP_OPT_INITIAL_TIMEOUT	10	/* reliable connection layer intial retry timeout */
#define L2TP_OPT_TIMEOUT_CAP		11	/* reliable connection layer timeout cap */
#define L2TP_OPT_MAX_RETRIES		12	/* reliable connection layer max retries */
#define L2TP_OPT_ACCEPT			13	/* accept incomming connect request and transfer to new socket */
#define L2TP_OPT_OURADDRESS		14	/* our IP address */
#define L2TP_OPT_BAUDRATE		15	/* tunnel baudrate */
#define L2TP_OPT_RELIABILITY		16	/* turn on/off reliability layer */
#define L2TP_OPT_SETDELEGATEDPID    17  /* set the delegated process for traffic statistics */

/* flags definition */
#define L2TP_FLAG_DEBUG		0x00000002	/* debug mode, send verbose logs to syslog */
#define L2TP_FLAG_CONTROL	0x00000004	/* this is a control session (as opposed to a data session) */
#define L2TP_FLAG_SEQ_REQ	0x00000008	/* our sequencing required (ignored for control connection) */
#define L2TP_FLAG_PEER_SEQ_REQ	0x00000010	/* peer sequencing required (ignored for control connection) */
#define L2TP_FLAG_ADAPT_TIMER	0x00000020	/* use adaptative timer for reliable layer */
#define L2TP_FLAG_IPSEC		0x00000040	/* is IPSec used for this connection */

/* control and data flags */
#define L2TP_FLAGS_T		0x8000
#define L2TP_FLAGS_L		0x4000
#define L2TP_FLAGS_S		0x0800
#define L2TP_FLAGS_O		0x0200
#define L2TP_FLAGS_P		0x0100

#define L2TP_VERSION_MASK	0x000F
#define L2TP_VERSION		2


/* define well known values */
#define L2TP_HDR_VERSION	2
#define L2TP_CNTL_HDR_SIZE	12	/* control headers are always this size */
#define L2TP_DATA_HDR_SIZE	8	/* hdr size for data we send - without sequencing */

struct l2tp_header {
    /* header for control messages */
    u_int16_t	flags_vers;
    u_int16_t	len;
    u_int16_t	tunnel_id;
    u_int16_t	session_id;
    u_int16_t	ns;				
    u_int16_t	nr;
    u_int16_t	off_size;
};

struct sockaddr_l2tp {
    u_int8_t	l2tp_len;			/* sizeof(struct sockaddr_ppp) + variable part */
    u_int8_t	l2tp_family;			/* AF_PPPCTL */
    u_int16_t	l2tp_proto;			/* protocol coding address - PPPPROTO_L2TP */
    u_int16_t 	l2tp_tunnel_id;
    u_int16_t	l2tp_session_id;
    u_int8_t	pad[8];
};
 
#endif
