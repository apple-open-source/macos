/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include "sys_interfaces.h"

__private_extern__ interface_list_t *
sys_interfaces(void)
{
	interface_list_t *my_interfaces = NULL;
	interface_t *iface;
	struct ifaddrs *ifa, *p;

	if (getifaddrs(&ifa) < 0) return NULL;

	my_interfaces = (interface_list_t *)malloc(sizeof(interface_list_t));
	my_interfaces->count = 0;
	my_interfaces->interface = NULL;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

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
		iface->name = strdup(p->ifa_name);
		iface->flags = p->ifa_flags;
		iface->addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;
		iface->mask.s_addr = ((struct sockaddr_in *)(p->ifa_netmask))->sin_addr.s_addr;
		iface->netaddr.s_addr = iface->addr.s_addr & iface->mask.s_addr;
		iface->bcast.s_addr = iface->netaddr.s_addr | (~iface->mask.s_addr);
	}

	freeifaddrs(ifa);

	return my_interfaces;
}

__private_extern__  void
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
