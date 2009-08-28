/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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
 * interfaces.c
 * - interface related routines
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/sockio.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <ctype.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <ifaddrs.h>
#include <sys/sysctl.h>
#include <net/route.h>

#include "interfaces.h"
#include "ip6config_utils.h"

static struct sockaddr_in6 init_sin6 = {sizeof(init_sin6), AF_INET6};

static interface_t *
if_next_entry(interface_list_t * interfaces, char * name)
{
    interface_t * entry;

    if (interfaces->count >= interfaces->size)
	return (NULL);

    entry = interfaces->list + interfaces->count++;
    bzero(entry, sizeof(*entry));
    strcpy(entry->name, name);
    return (entry);
}

static boolean_t
if_build_interface_list(interface_list_t * interfaces)
{
    struct ifaddrs	*ifaddrs = NULL;
    struct ifaddrs	*ifa;
    int			size = 0;

    if (getifaddrs(&ifaddrs) < 0) {
	return FALSE;
    }

    for (ifa = ifaddrs; ifa != NULL && ifa->ifa_addr != NULL;
	 ifa = ifa->ifa_next) {
	/* count the number of AF_INET6 and AF_LINK that were returned */
	if ((ifa->ifa_addr->sa_family == AF_INET6) ||
		(ifa->ifa_addr->sa_family == AF_LINK)) {
	    size++;
	}
    }

    interfaces->list = (interface_t *)malloc(size * sizeof(*(interfaces->list)));
    if (interfaces->list == NULL) {
	goto err;
    }

    interfaces->count = 0;
    interfaces->size = size;

    for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
	if (ifa->ifa_addr == NULL) {
	    continue;
	}
	switch (ifa->ifa_addr->sa_family) {
	    case AF_LINK: {
		struct sockaddr_dl * 	dl_p;
		interface_t *		entry;
		struct if_data *	if_data;
		char 			name[IFNAMSIZ + 1];

		dl_p = (struct sockaddr_dl *)ifa->ifa_addr;
		strncpy(name, ifa->ifa_name, sizeof(name));
		name[IFNAMSIZ] = '\0';
		entry = ifl_find_name(interfaces, name);
		if (entry == NULL) {
		    /* new entry */
		    entry = if_next_entry(interfaces, name);
		    if (entry == NULL) {
			/* NOT REACHED */
			syslog(LOG_ERR,
			       "if_build_interface_list interfaces: if_next_entry returns NULL");
			continue;
		    }
		    entry->flags = ifa->ifa_flags;
		}
		if (dl_p->sdl_alen > sizeof(entry->link_address.addr)) {
		    syslog(LOG_DEBUG,
			   "%s: link type %d address length %d > %d", name,
			   dl_p->sdl_type, dl_p->sdl_alen,
			   sizeof(entry->link_address.addr));
		    entry->link_address.alen = sizeof(entry->link_address.addr);
		}
		else {
		    entry->link_address.alen = dl_p->sdl_alen;
		}
		bcopy(dl_p->sdl_data + dl_p->sdl_nlen, entry->link_address.addr,
		      entry->link_address.alen);
		entry->link_address.type = dl_p->sdl_type;
		entry->link_address.index = dl_p->sdl_index;
		if_data = (struct if_data *)ifa->ifa_data;
		entry->type = if_data->ifi_type;
		break;
	    }
	}
    }

    /* make it the "right" size (plus 1 more in case it's 0) */
    interfaces->list = (interface_t *)realloc(interfaces->list,
					      sizeof(*(interfaces->list)) * (interfaces->count + 1));

    freeifaddrs(ifaddrs);
    return (TRUE);
  err:
    if (interfaces->list)
	free(interfaces->list);
    interfaces->list = NULL;
    freeifaddrs(ifaddrs);
    return (FALSE);
}

__private_extern__ int
ifl_count(interface_list_t * list_p)
{
    return (list_p->count);
}

__private_extern__ interface_t *
ifl_at_index(interface_list_t * list_p, int i)
{
    if (i >= list_p->count || i < 0)
	return (NULL);
    return (list_p->list + i);
}

__private_extern__ interface_t *
ifl_find_name(interface_list_t * list_p, const char * name)
{
    int i;

    for (i = 0; i < list_p->count; i++) {
	if (strcmp(list_p->list[i].name, name) == 0)
	    return (list_p->list + i);
    }
    return (NULL);
}

__private_extern__ interface_list_t *
ifl_init()
{
    interface_list_t * list_p = (interface_list_t *)malloc(sizeof(*list_p));
    if (list_p == NULL || if_build_interface_list(list_p) == FALSE) {
	if (list_p)
	    free(list_p);
	return (NULL);
    }
    return (list_p);
}

__private_extern__ void
ifl_free(interface_list_t * * iflist)
{
    if (iflist != NULL && *iflist != NULL) {
	interface_list_t * 	list_p = *iflist;

	if (list_p->list)
	    free(list_p->list);
	free(list_p);
	*iflist = NULL;
    }
    return;
}

/*
 * Functions: if_*
 * Purpose:
 *   Interface-specific routines.
 */
__private_extern__ char *
if_name(interface_t * if_p)
{
    return (if_p->name);
}

__private_extern__ short
if_flags(interface_t * if_p)
{
    return (if_p->flags);
}

__private_extern__ void
if_setflags(interface_t * if_p, short flags)
{
    if_p->flags = flags;
    return;
}

__private_extern__ void
if_free(interface_t * * if_p_p)
{
    interface_t * if_p;

    if (if_p_p == NULL) {
	return;
    }
    if_p = *if_p_p;
    if (if_p == NULL) {
	return;
    }
    free(if_p);
    *if_p_p = NULL;
    return;
}

__private_extern__ interface_t *
if_dup(interface_t * intface)
{
    interface_t * new_p;

    new_p = (interface_t *)calloc(1, sizeof(*new_p));
    if (new_p == NULL) {
	return (NULL);
    }
    *new_p = *intface;
    return (new_p);
}

void
if_link_copy(interface_t * dest, const interface_t * source)
{
    dest->link_address = source->link_address;
    return;
}

void
if_link_update(interface_t * if_p)
{
    char *			buf = NULL;
    size_t			buf_len = 0;
    struct sockaddr_dl *	dl_p;
    struct if_msghdr * 		ifm;
    int				mib[6];

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_LINK;
    mib[4] = NET_RT_IFLIST;
    mib[5] = if_p->link_address.index; /* ask for exactly one interface */

    if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() size failed: %s", strerror(errno));
	goto failed;
    }
    buf = malloc(buf_len);
    if (sysctl(mib, 6, buf, &buf_len, NULL, 0) < 0) {
	fprintf(stderr, "sysctl() failed: %s", strerror(errno));
	goto failed;
    }
    ifm = (struct if_msghdr *)buf;
    switch (ifm->ifm_type) {
    case RTM_IFINFO:
	dl_p = (struct sockaddr_dl *)(ifm + 1);
	if (dl_p->sdl_alen > sizeof(if_p->link_address.addr)) {
	    syslog(LOG_DEBUG,
		   "%s: link type %d address length %d > %d", if_name(if_p),
		   dl_p->sdl_type, dl_p->sdl_alen,
		   sizeof(if_p->link_address.addr));
	    if_p->link_address.alen = sizeof(if_p->link_address.addr);
	}
	else {
	    if_p->link_address.alen = dl_p->sdl_alen;
	}
	bcopy(dl_p->sdl_data + dl_p->sdl_nlen,
	      if_p->link_address.addr, if_p->link_address.alen);
	if_p->link_address.type = dl_p->sdl_type;
    }
 failed:
    if (buf != NULL) {
	free(buf);
    }
    return;
}

int
if_ift_type(interface_t * if_p)
{
    return (if_p->type);
}

