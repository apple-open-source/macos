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

#ifndef _NETINET_IF_FIREWIRE_H_
#define _NETINET_IF_FIREWIRE_H_
#include <net/ethernet.h>
#include <netinet/in.h>
#include <net/if_arp.h>

#define FIREWIREMCAST_V4_LEN		4
#define FIREWIREMCAST_V6_LEN		1

const u_char ipv4multicast[FIREWIREMCAST_V4_LEN]	= {0x01, 0x00, 0x5e, 0x00};
const u_char ipv6multicast[FIREWIREMCAST_V6_LEN]	= {0xFF};
const u_char fwbroadcastaddr[FIREWIRE_ADDR_LEN]		= {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/*
 * Macro to map an IP multicast address to an FIREWIRE multicast address.
 * The high-order 25 bits of the FIREWIRE address are statically assigned,
 * and the low-order 23 bits are taken from the low end of the IP address.
 */
#define FIREWIRE_MAP_IP_MULTICAST(ipaddr, enaddr) \
	/* struct in_addr *ipaddr; */ \
	/* u_char enaddr[FIREWIRE_ADDR_LEN];	   */ \
{ \
	(enaddr)[0] = ipv4multicast[0]; \
	(enaddr)[1] = ipv4multicast[1]; \
	(enaddr)[2] = ipv4multicast[2]; \
	(enaddr)[3] = ipv4multicast[3]; \
	(enaddr)[4] = ((u_char *)ipaddr)[0]; \
	(enaddr)[5] = ((u_char *)ipaddr)[1] & 0x7f; \
	(enaddr)[6] = ((u_char *)ipaddr)[2]; \
	(enaddr)[7] = ((u_char *)ipaddr)[3]; \
}

/*
 * Macro to map an IPv6 multicast address to an FIREWIRE multicast address.
 * The high-order 16 bits of the FIREWIRE address are statically assigned,
 * and the low-order 48 bits are taken from the low end of the IPv6 address.
 */
#define FIREWIRE_MAP_IPV6_MULTICAST(ip6addr, enaddr)	\
/* struct	in6_addr *ip6addr; */						\
/* u_char	enaddr[FIREWIRE_ADDR_LEN]; */				\
{                                                       \
	(enaddr)[0] = ((u_char *)ip6addr)[0];									\
	(enaddr)[1] = ((u_char *)ip6addr)[1];									\
	(enaddr)[2] = ((u_char *)ip6addr)[10];				\
	(enaddr)[3] = ((u_char *)ip6addr)[11];				\
	(enaddr)[4] = ((u_char *)ip6addr)[12];				\
	(enaddr)[5] = ((u_char *)ip6addr)[13];				\
	(enaddr)[6] = ((u_char *)ip6addr)[14];				\
	(enaddr)[7] = ((u_char *)ip6addr)[15];				\
}

#endif
