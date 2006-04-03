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
#include <ifaddrs.h>

interface_list_t *
sys_interfaces(void)
{
	interface_t *iface;
	struct ifaddrs *ifa, *p;
	interface_list_t *l;

	if (getifaddrs(&ifa) < 0) return NULL;

	l = (interface_list_t *)calloc(1, sizeof(interface_list_t));
	l->count = 0;
	l->interface = NULL;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

		l->count++;
		if (l->count == 1)
		{
			l->interface = (interface_t *)malloc(sizeof(interface_t));
		}
		else
		{
			l->interface = (interface_t *)realloc(l->interface, l->count * sizeof(interface_t));
		}

		iface = &(l->interface[l->count - 1]);
		memset(iface, 0, sizeof(interface_t));
		iface->name = strdup(p->ifa_name);
		iface->flags = p->ifa_flags;
		iface->addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;
		iface->mask.s_addr = ((struct sockaddr_in *)(p->ifa_netmask))->sin_addr.s_addr;
		iface->netaddr.s_addr = iface->addr.s_addr & iface->mask.s_addr;
		iface->bcast.s_addr = iface->netaddr.s_addr | (~iface->mask.s_addr);
	}

	freeifaddrs(ifa);

	return l;
}

void
sys_interfaces_release(interface_list_t *l)
{
	int i;

	if (l == NULL) return;

	for (i = 0; i < l->count; i++)
	{
		if (l->interface[i].name != NULL) free(l->interface[i].name);
	}

	free(l->interface);
	free(l);
	l = NULL;
}

int
sys_is_my_address(struct in_addr *a)
{
	struct ifaddrs *ifa, *p;

	if (getifaddrs(&ifa) < 0) return 0;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

		if (a->s_addr == ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr)
		{
			freeifaddrs(ifa);
			return 1;
		}
	}

	freeifaddrs(ifa);
	return 0;
}

int
sys_is_my_network(struct in_addr *a)
{
	struct ifaddrs *ifa, *p;
	struct in_addr addr, mask, netaddr;

	if (getifaddrs(&ifa) < 0) return 0;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;
		if (p->ifa_flags & IFF_POINTOPOINT) continue;

		addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;
		mask.s_addr = ((struct sockaddr_in *)(p->ifa_netmask))->sin_addr.s_addr;
		netaddr.s_addr = addr.s_addr & mask.s_addr;
		if (a->s_addr == netaddr.s_addr)
		{
			freeifaddrs(ifa);
			return 1;
		}

	}

	freeifaddrs(ifa);
	return 0;
}

int
sys_is_my_broadcast(struct in_addr *a)
{
	struct ifaddrs *ifa, *p;
	struct in_addr addr, mask, netaddr, bcast;

	if (getifaddrs(&ifa) < 0) return 0;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;
		if (p->ifa_flags & IFF_POINTOPOINT) continue;

		addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;
		mask.s_addr = ((struct sockaddr_in *)(p->ifa_netmask))->sin_addr.s_addr;
		netaddr.s_addr = addr.s_addr & mask.s_addr;
		bcast.s_addr = netaddr.s_addr | (~mask.s_addr);
		if (a->s_addr == bcast.s_addr)
		{
			freeifaddrs(ifa);
			return 1;
		}
	}

	freeifaddrs(ifa);
	return 0;
}


int
sys_is_on_attached_network(struct in_addr *a)
{
	struct ifaddrs *ifa, *p;
	struct in_addr addr, mask, netaddr, n;

	if (getifaddrs(&ifa) < 0) return 0;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

		addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;

		if (p->ifa_flags & IFF_POINTOPOINT)
		{
			if (a->s_addr == addr.s_addr)
			{
				freeifaddrs(ifa);
				return 1;
			}
		}
		else
		{
			mask.s_addr = ((struct sockaddr_in *)(p->ifa_netmask))->sin_addr.s_addr;
			netaddr.s_addr = addr.s_addr & mask.s_addr;
			n.s_addr = a->s_addr & mask.s_addr;
			if (n.s_addr == netaddr.s_addr)
			{
				freeifaddrs(ifa);
				return 1;
			}
		}
	}

	freeifaddrs(ifa);
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
	struct ifaddrs *ifa, *p;
	struct in_addr addr;

	if (getifaddrs(&ifa) < 0) return 1;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

		addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;
		if (addr.s_addr == INADDR_LOOPBACK) continue;
	
		freeifaddrs(ifa);
		return 0;
	}

	freeifaddrs(ifa);
	return 1;
}

char *
interface_name_for_addr(struct in_addr *a)
{
	struct ifaddrs *ifa, *p;
	char *name;

	if (getifaddrs(&ifa) < 0) return NULL;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

		if (a->s_addr == ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr)
		{
			name = strdup(p->ifa_name);
			freeifaddrs(ifa);
			return name;
		}
	}

	freeifaddrs(ifa);
	return NULL;
}

interface_t *
interface_with_name(char *n)
{
	interface_t *iface;
	struct ifaddrs *ifa, *p;

	if (getifaddrs(&ifa) < 0) return NULL;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;
		if (strcmp(p->ifa_name, n) != 0) continue;

		iface = (interface_t *)malloc(sizeof(interface_t));
		memset(iface, 0, sizeof(interface_t));
		iface->name = strdup(p->ifa_name);
		iface->flags = p->ifa_flags;
		iface->addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;
		iface->mask.s_addr = ((struct sockaddr_in *)(p->ifa_netmask))->sin_addr.s_addr;
		iface->netaddr.s_addr = iface->addr.s_addr & iface->mask.s_addr;
		iface->bcast.s_addr = iface->netaddr.s_addr | (~iface->mask.s_addr);

		freeifaddrs(ifa);
		return iface;
	}

	freeifaddrs(ifa);

	return NULL;
}
