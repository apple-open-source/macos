/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sys_interfaces.h"

__private_extern__ interface_list_t *
sys_interfaces(void)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[1024]; /* XXX */
	int offset, addrlen, extra, delta;
	int sock;
	interface_t *iface;
	interface_list_t *my_interfaces;

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0) return NULL;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0)
	{
		close(sock);
		return NULL;
	}

	my_interfaces = (interface_list_t *)malloc(sizeof(interface_list_t));
	my_interfaces->count = 0;
	my_interfaces->interface = NULL;

	delta = sizeof(struct ifreq);
	addrlen = delta - IFNAMSIZ;
	extra = 0;

	offset = 0;

	while (offset <= ifc.ifc_len)
	{
		ifr = (struct ifreq *)(ifc.ifc_buf + offset);

#ifndef _NO_SOCKADDR_LENGTH_
		extra = ifr->ifr_addr.sa_len - addrlen;
		if (extra < 0) extra = 0;
#endif

		offset = offset + delta + extra;

		if (ifr->ifr_addr.sa_family != AF_INET) continue;
		if (ioctl(sock, SIOCGIFFLAGS, (char *)ifr) < 0) continue;

		my_interfaces->count++;
		if (my_interfaces->count == 1)
		{
			my_interfaces->interface = (interface_t *)malloc(sizeof(interface_t));
		}
		else
		{
			my_interfaces->interface = (interface_t *)realloc(my_interfaces->interface, my_interfaces->count * sizeof(interface_t));
		}

		iface = &(my_interfaces->interface[my_interfaces->count - 1]);
		memset(iface, 0, sizeof(interface_t));

		memmove(iface->name, ifr->ifr_name, IFNAMSIZ);
		iface->flags = ifr->ifr_ifru.ifru_flags;
		iface->addr.s_addr = ((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr.s_addr;
		ioctl(sock, SIOCGIFNETMASK, (char *)ifr);
		iface->mask.s_addr = ((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr.s_addr;
		iface->netaddr.s_addr = iface->addr.s_addr & iface->mask.s_addr;
		iface->bcast.s_addr = iface->netaddr.s_addr | (~iface->mask.s_addr);
	}

	close(sock);
	return my_interfaces;
}

__private_extern__  void
sys_interfaces_release(interface_list_t *l)
{
	if (l == NULL) return;

	if (l->interface != NULL) free(l->interface);
	free(l);
}

__private_extern__ int
sys_is_my_address(interface_list_t *l, struct in_addr *a)
{
	int i;

	if (l == NULL) return 0;
	
	for (i = 0; i < l->count; i++)
	{
		if (a->s_addr == l->interface[i].addr.s_addr) return 1;
	}
	return 0;
}

__private_extern__ int
sys_is_my_network(interface_list_t *l, struct in_addr *a)
{
	int i;

	if (l == NULL) return 0;
	
	for (i = 0; i < l->count; i++)
	{
		if ((a->s_addr & l->interface[i].mask.s_addr) ==
			l->interface[i].netaddr.s_addr) return 1;

	}
	return 0;
}
