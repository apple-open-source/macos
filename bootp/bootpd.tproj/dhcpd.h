
#ifndef _S_DHCPD_H
#define _S_DHCPD_H

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
/*
 * dhcpd.h
 * - DHCP server definitions
 */

#include "dhcplib.h"
#include "bootpd.h"

void
dhcp_init();

void
dhcp_request(request_t * request, dhcp_msgtype_t msgtype,
	     boolean_t dhcp_allocate);

boolean_t
dhcp_bootp_allocate(char * idstr, char * hwstr, struct dhcp * rq,
		    interface_t * if_p, struct timeval * time_in_p,
		    struct in_addr * iaddr_p, id * subnet_p);

#define DHCP_CLIENT_TYPE		"dhcp"

/* default time to leave an ip address pending before re-using it */
#define DHCP_PENDING_SECS		60

#define DHCP_MIN_LEASE	((dhcp_lease_t)60 * 60) /* one hour */
#define DHCP_MAX_LEASE	((dhcp_lease_t)60 * 60 * 24 * 30) /* one month */

#define SUBNETPROP_LEASE_MIN		"lease_min"
#define SUBNETPROP_LEASE_MAX		"lease_max"

#define TIME_DRIFT_PERCENT		0.99

static __inline__ u_long
lease_prorate(u_long lease_time)
{
    double d = lease_time * TIME_DRIFT_PERCENT;

    return ((u_long)d);
}

struct dhcp * 
make_dhcp_reply(struct dhcp * reply, int pkt_size, 
		struct in_addr server_id, dhcp_msgtype_t msg, 
		struct dhcp * request, dhcpoa_t * options);
int
dhcp_max_message_size(dhcpol_t * client_options);

#endif _S_DHCPD_H
