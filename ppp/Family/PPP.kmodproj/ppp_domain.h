/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef __PPP_DOMAIN_H__
#define __PPP_DOMAIN_H__


#define PF_PPP 			248		/* TEMP - move to socket.h */
#define AF_PPPCTL 		1		/* */

#define PPPPROTO_CTL		1		/* */

#define PPP_NAME		"PPP"		/* */


struct sockaddr_pppctl {
    unsigned char	ppp_len;				/* sizeof(struct sockaddr_pppctl) */
    unsigned char	ppp_family;				/* AF_PPPCTL */
    unsigned long 	ppp_reserved;				/* reserved for future use */
};


int ppp_domain_init();
int ppp_domain_dispose();

int ppp_proto_input(u_short subfam, u_short unit, void *data, struct mbuf *m);


#endif