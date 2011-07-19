/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
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
 * rtutil.h
 * - routing table routines
 */

/* 
 * Modification History
 *
 * June 23, 2009	Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#ifndef _S_RTUTIL_H
#define _S_RTUTIL_H

#include <net/route.h>
#include <netinet/in.h>
#include <mach/boolean.h>

int
subnet_route_if_index(struct in_addr netaddr, struct in_addr netmask);

boolean_t
host_route(int cmd, struct in_addr iaddr);

boolean_t
subnet_route_add(struct in_addr gateway, struct in_addr netaddr, 
		 struct in_addr netmask, const char * ifname);

boolean_t
subnet_route_delete(struct in_addr gateway, struct in_addr netaddr, 
		    struct in_addr netmask);
void
flush_routes(int if_index, const struct in_addr ip,
	     const struct in_addr broadcast);

int
rt_xaddrs(const char * cp, const char * cplim, struct rt_addrinfo * rtinfo);

#endif /* _S_RTUTIL_H */
