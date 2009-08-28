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


#ifndef __PPPOE_H__
#define __PPPOE_H__

#include <sys/ioccom.h>
#include <net/ethernet.h>

// PPPoE error codes (bits 8..15 of last cause key)
#define EXIT_PPPoE_NOSERVER  		1
#define EXIT_PPPoE_NOSERVICE  		2
#define EXIT_PPPoE_NOAC 		3
#define EXIT_PPPoE_NOACSERVICE 		4
#define EXIT_PPPoE_CONNREFUSED 		5

#define PPPOE_ETHERTYPE_CTRL 	0x8863
#define PPPOE_ETHERTYPE_DATA 	0x8864

//#define PF_PPPOE 		247		/* TEMP - move to socket.h */
#define PPPPROTO_PPPOE		16		/* TEMP - move to ppp.h - 1..32 are reserved */
//#define APPLE_PPP_NAME_PPPoE	"PPPoE"
#define PPPOE_NAME		"PPPoE"		/* */

#define PPPOE_AC_NAME_LEN	64
#define PPPOE_SERVICE_LEN	64

struct sockaddr_pppoe
{
    struct sockaddr_ppp	ppp;					/* generic ppp address */
    char 		pppoe_ac_name[PPPOE_AC_NAME_LEN];	/* Access Concentrator name */
    char 		pppoe_service[PPPOE_SERVICE_LEN];	/* Service name */
};


#define PPPOE_OPT_FLAGS		1	/* see flags definition below */
#define PPPOE_OPT_INTERFACE	2	/* ethernet interface to use (en0, en1...) */
#define PPPOE_OPT_CONNECT_TIMER	3	/* time allowed for outgoing call (in seconds) */
#define PPPOE_OPT_RING_TIMER	4	/* time allowed for incoming call (in seconds) */
#define PPPOE_OPT_RETRY_TIMER	5	/* connection retry timer (in seconds) */
#define PPPOE_OPT_PEER_ENETADDR	6	/* peer ethernet address */

/* flags definition */
#define PPPOE_FLAG_LOOPBACK	0x00000001	/* loopback mode, for debugging purpose */
#define PPPOE_FLAG_DEBUG	0x00000002	/* debug mode, send verbose logs to syslog */
#define PPPOE_FLAG_PROBE	0x00000004	/* just probe to detect presence of servers */


#endif
