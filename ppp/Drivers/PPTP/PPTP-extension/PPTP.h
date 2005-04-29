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

#ifndef __PPTP_H__
#define __PPTP_H__

/* GRE definitions */
#define PPTP_GRE_TYPE 		0x880B
#define PPTP_GRE_FLAGS_C	0x80
#define PPTP_GRE_FLAGS_R	0x40
#define PPTP_GRE_FLAGS_K	0x20
#define PPTP_GRE_FLAGS_S	0x10
#define PPTP_GRE_FLAGS_s	0x08

#define PPTP_GRE_FLAGS_A	0x80
#define PPTP_GRE_VER		1



#define PPPPROTO_PPTP		17		/* TEMP - move to ppp.h - 1..32 are reserved */
#define PPTP_NAME		"PPTP"		/* */


#define PPTP_OPT_FLAGS		1	/* see flags definition below */
#define PPTP_OPT_PEERADDRESS	2	/* peer IP address */
#define PPTP_OPT_CALL_ID	3	/* call id for the connection */
#define PPTP_OPT_PEER_CALL_ID	4	/* peer call id for the connection */
#define PPTP_OPT_WINDOW		5	/* our receive window */
#define PPTP_OPT_PEER_WINDOW	6	/* peer receive window */
#define PPTP_OPT_PEER_PPD	7	/* peer packet processing delay */
#define PPTP_OPT_MAXTIMEOUT	8	/* maximum adptative timeout */
#define PPTP_OPT_OURADDRESS	9	/* our IP address */
#define PPTP_OPT_BAUDRATE	10	/* tunnel baudrate */

/* flags definition */
#define PPTP_FLAG_DEBUG		0x00000002	/* debug mode, send verbose logs to syslog */


#endif
