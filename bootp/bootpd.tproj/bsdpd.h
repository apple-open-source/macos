/*
 * Copyright (c) 1999, 2000 Apple Computer, Inc. All rights reserved.
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

/*
 * bsdpd.h
 * - Boot Server Discovery Protocol (BSDP) server definitions
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com)		November 23, 1999
 * - created
 */

boolean_t
bsdp_init();

void
bsdp_request(dhcp_msgtype_t dhcp_msgtype, interface_t * intface,
	     u_char * rxpkt, int n, dhcpol_t * rq_options, 
	     struct in_addr * dstaddr_p, struct timeval * time_in_p);
boolean_t
old_netboot_request(interface_t * intface,
		    u_char * rxpkt, int n, dhcpol_t * rq_options, 
		    struct in_addr * dstaddr_p, struct timeval * time_in_p);

