/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_ether.h	8.3 (Berkeley) 5/2/95
 * $FreeBSD: src/sys/netinet/if_ether.h,v 1.24 1999/12/29 04:40:58 peter Exp $
 */

#ifndef _NETINET_IF_FIREWIRE_H_
#define _NETINET_IF_FIREWIRE_H_
#include <net/ethernet.h>
#include <netinet/in.h>
#include <net/if_arp.h>

/*
 * Macro to map an IP multicast address to an FIREWIRE multicast address.
 * The high-order 25 bits of the FIREWIRE address are statically assigned,
 * and the low-order 23 bits are taken from the low end of the IP address.
 */
#define FIREWIRE_MAP_IP_MULTICAST(ipaddr, enaddr) \
	/* struct in_addr *ipaddr; */ \
	/* u_char enaddr[FIREWIRE_ADDR_LEN];	   */ \
{ \
	(enaddr)[0] = 0x01; \
	(enaddr)[1] = 0x00; \
	(enaddr)[2] = 0x5e; \
	(enaddr)[3] = ((u_char *)ipaddr)[0]; \
	(enaddr)[4] = ((u_char *)ipaddr)[1] & 0x7f; \
	(enaddr)[5] = ((u_char *)ipaddr)[2]; \
	(enaddr)[6] = ((u_char *)ipaddr)[3]; \
}

/*
 * Macro to map an IP6 multicast address to an FIREWIRE multicast address.
 * The high-order 16 bits of the FIREWIRE address are statically assigned,
 * and the low-order 32 bits are taken from the low end of the IP6 address.
 */
#define FIREWIRE_MAP_IPV6_MULTICAST(ip6addr, enaddr)	\
/* struct	in6_addr *ip6addr; */						\
/* u_char	enaddr[FIREWIRE_ADDR_LEN]; */				\
{                                                       \
	(enaddr)[0] = 0x33;									\
	(enaddr)[1] = 0x33;									\
	(enaddr)[2] = ((u_char *)ip6addr)[10];				\
	(enaddr)[3] = ((u_char *)ip6addr)[11];				\
	(enaddr)[4] = ((u_char *)ip6addr)[12];				\
	(enaddr)[5] = ((u_char *)ip6addr)[13];				\
	(enaddr)[6] = ((u_char *)ip6addr)[14];				\
	(enaddr)[7] = ((u_char *)ip6addr)[15];				\
}

#define	arp_hrd	ea_hdr.ar_hrd
#define	arp_pro	ea_hdr.ar_pro
#define	arp_hln	ea_hdr.ar_hln
#define	arp_pln	ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op

/*
 * IP and ethernet specific routing flags
 */
#define	RTF_USETRAILERS	RTF_PROTO1	/* use trailers */
#define RTF_ANNOUNCE	RTF_PROTO2	/* announce new arp entry */

// extern u_char	firewire_ipmulticast_min[FIREWIRE_ADDR_LEN];
// extern u_char	firewire_ipmulticast_max[FIREWIRE_ADDR_LEN];

#endif
