/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Network interface utility routines
 * Created / Re-written / Mangled by Marc Majka.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <NetInfo/network.h>
#include <NetInfo/socket_lock.h>

#define socket_close close

static interface_list_t *my_interfaces = NULL;

interface_list_t *
sys_interfaces(void)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[1024]; /* XXX */
	int offset, addrlen, extra, delta;
	int sock;
	interface_t *iface;

	if (my_interfaces != NULL) return my_interfaces;

	socket_lock();
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	socket_unlock();

	if (sock < 0) return NULL;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0)
	{
		socket_close(sock);
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

	socket_close(sock);
	return my_interfaces;
}

void
sys_interfaces_release(void)
{
	if (my_interfaces == NULL) return;

	free(my_interfaces->interface);
	free(my_interfaces);
	my_interfaces = NULL;
}

int
sys_is_my_address(struct in_addr *a)
{
	int i;
	interface_list_t *l;

	l = sys_interfaces();
	if (l == NULL) return 0;
	
	for (i = 0; i < l->count; i++)
	{
		if (a->s_addr == l->interface[i].addr.s_addr) return 1;
	}
	return 0;
}

int
sys_is_my_network(struct in_addr *a)
{
	int i;
	interface_list_t *l;

	l = sys_interfaces();
	if (l == NULL) return 0;
	
	for (i = 0; i < l->count; i++)
	{
		if (l->interface[i].flags & IFF_POINTOPOINT) continue;
		if (a->s_addr == l->interface[i].netaddr.s_addr) return 1;
	}
	return 0;
}

int
sys_is_my_broadcast(struct in_addr *a)
{
	int i;
	interface_list_t *l;

	l = sys_interfaces();
	if (l == NULL) return 0;
	
	for (i = 0; i < l->count; i++)
	{
		if (l->interface[i].flags & IFF_POINTOPOINT) continue;
		if (a->s_addr == l->interface[i].bcast.s_addr) return 1;
	}
	return 0;
}

int
sys_is_on_attached_network(struct in_addr *a)
{
	int i;
	interface_list_t *l;
	unsigned long n;
	
	l = sys_interfaces();
	if (l == NULL) return 0;

	for (i = 0; i < l->count; i++)
	{
		if (!(l->interface[i].flags & IFF_POINTOPOINT))
		{
			n = a->s_addr & l->interface[i].mask.s_addr;
			if (n == l->interface[i].netaddr.s_addr) return 1;
		}
		else
		{
			if (a->s_addr == l->interface[i].addr.s_addr) return 1;
		}
	}
	return 0;
}

int
sys_is_loopback(struct in_addr *a)
{
	if (a->s_addr == INADDR_LOOPBACK) return 1;
	return 0;
}

int
sys_is_general_broadcast(struct in_addr *a)
{
	if (a->s_addr == INADDR_BROADCAST) return 1;
	return 0;
}

int
sys_is_standalone(void)
{
	interface_list_t *l;

	l = sys_interfaces();
	if (l == NULL) return 1;
	if (l->count == 0) return 1;
	if (l->count > 1) return 0;
	if (l->interface[0].addr.s_addr == INADDR_LOOPBACK) return 1;
	return 0;
}

char *
interface_name_for_addr(struct in_addr *a)
{
	int i;
	interface_list_t *l;

	l = sys_interfaces();
	if (l == NULL) return NULL;
	
	for (i = 0; i < l->count; i++)
	{
		if (a->s_addr == l->interface[i].addr.s_addr) return l->interface[i].name;
	}
	return NULL;
}


interface_t *
interface_with_name(char *n)
{
	int i;
	interface_list_t *l;

	l = sys_interfaces();
	if (l == NULL) return 0;
	
	for (i = 0; i < l->count; i++)
	{
		if (!strcmp(n, l->interface[i].name)) return &(l->interface[i]);
	}

	return NULL;
}

