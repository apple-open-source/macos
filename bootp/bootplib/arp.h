
#ifndef _S_ARP_H
#define _S_ARP_H
/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * arp.h
 */

/*
 * Modification History:
 *
 * 25 Feb 1998	Dieter Siegmund (dieter@apple.com)
 * - created
 */ 

#define ARP_RETURN_SUCCESS			0
#define ARP_RETURN_INTERFACE_NOT_FOUND		1
#define ARP_RETURN_PROXY_ONLY			2
#define ARP_RETURN_PROXY_ON_NON_802		3
#define ARP_RETURN_INTERNAL_ERROR		4
#define ARP_RETURN_WRITE_FAILED			5
#define ARP_RETURN_READ_FAILED			6
#define ARP_RETURN_HOST_NOT_FOUND		7
#define ARP_RETURN_LAST				8

#include <net/route.h>

typedef struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} route_msg;

const char * 	arp_strerror(int err);
int		arp_get(int s, route_msg * msg_p, struct in_addr * iaddr_p, int if_index);
int		arp_set(int s, struct in_addr * iaddr_p, void * hwaddr_p, 
			int hwaddr_len, int temp, int public);
int 		arp_delete(int s, struct in_addr iaddr, int if_index, int export);
int		arp_flush(int s, int all);

int		arp_get_routing_socket();

#endif _S_ARP_H
