/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * interfaces.c
 * - get the list of interfaces in the system
 */

/*
 * Modification History
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <strings.h>
#include <syslog.h>
#include <netdb.h>
#include "interfaces.h"
#include <arpa/inet.h>
#include <syslog.h>
#include <net/if_types.h>
#include "util.h"

extern struct ether_addr *	ether_aton(char *);

static struct sockaddr_in init_sin = {sizeof(init_sin), AF_INET};

#define MAX_IF		16

static boolean_t
S_get_ifreq_buf(int * sock_p, struct ifconf * ifconf_p)
{
    struct ifreq * 	ifreq = NULL;
    int			size = sizeof(struct ifreq) * MAX_IF;
    int			sockfd;

    size = sizeof(struct ifreq) * MAX_IF;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	syslog(LOG_INFO, "socket call failed");
	return (FALSE);
    }

    while (1) {
	if (ifreq != NULL)
	    ifreq = (struct ifreq *)realloc(ifreq, size);
	else
	    ifreq = (struct ifreq *)malloc(size);
	ifconf_p->ifc_len = size;
	ifconf_p->ifc_req = ifreq;
	if (ioctl(sockfd, SIOCGIFCONF, (caddr_t)ifconf_p) < 0
	    || ifconf_p->ifc_len <= 0) {
	    syslog(LOG_INFO, "ioctl SIOCGIFCONF failed");
	    goto err;
	}
	if ((ifconf_p->ifc_len + SOCK_MAXADDRLEN + IFNAMSIZ) < size)
	    break;
	size *= 2;
    }
    *sock_p = sockfd;
    return (TRUE);
  err:
    close(sockfd);
    if (ifreq)
	free(ifreq);
    return (FALSE);
}

void *
inet_addrinfo_copy(void * p)
{
    inet_addrinfo_t * src = (inet_addrinfo_t *)p;
    inet_addrinfo_t * dest;

    dest = malloc(sizeof(*dest));
    if (dest) {
	bcopy(src, dest, sizeof(*dest));
    }
    return (dest);
}
void
inet_addrinfo_free(void * p)
{
    free(p);
    return;
}

static interface_t *
S_next_entry(interface_list_t * interfaces, char * name)
{
    interface_t * entry;

    if (interfaces->count >= interfaces->size)
	return (NULL);

    entry = interfaces->list + interfaces->count++;
    bzero(entry, sizeof(*entry));
    strcpy(entry->name, name);
    dynarray_init(&entry->inet, inet_addrinfo_free,
		  inet_addrinfo_copy);
    return (entry);
}

static boolean_t
S_build_interface_list(interface_list_t * interfaces)
{
    struct ifconf 	ifconf;
    struct ifreq *	ifrp;
    int			size;
    int			sockfd;

    if (S_get_ifreq_buf(&sockfd, &ifconf) == FALSE)
	return (FALSE);

    size = (ifconf.ifc_len / sizeof(struct ifreq));
    interfaces->list 
	= (interface_t *)malloc(size * sizeof(*(interfaces->list)));
    if (interfaces->list == NULL)
	goto err;

#define IFR_NEXT(ifr)	\
    ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
      MAX(0, (int) (ifr)->ifr_addr.sa_len - (int) sizeof((ifr)->ifr_addr))))

    interfaces->count = 0;
    interfaces->size = size;
    for (ifrp = (struct ifreq *) ifconf.ifc_buf;
	 (char *) ifrp < &ifconf.ifc_buf[ifconf.ifc_len];
	 ifrp = IFR_NEXT(ifrp)) {

	switch (ifrp->ifr_addr.sa_family) {
	  case AF_INET: {
	      inet_addrinfo_t	info;
	      struct ifreq	ifr;
	      short		flags;
	      interface_t *	entry;
	      u_char 		name[IFNAMSIZ + 1];

	      strncpy(name, ifrp->ifr_name, sizeof(name));
	      name[IFNAMSIZ] = '\0';
	      entry = ifl_find_name(interfaces, name);
	      if (entry == NULL) { /* new entry */
		  entry = S_next_entry(interfaces, name);
		  if (entry == NULL) {
		      /* NOT REACHED */
		      syslog(LOG_ERR,
			     "interfaces: S_next_entry returns NULL"); 
		      continue;
		  }
		      
		  ifr = *ifrp;
		  if (ioctl(sockfd, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		      syslog(LOG_INFO, "%s: ioctl(SIOGIFFLAGS), %s (%d)",
			     name, strerror(errno), errno);
		  }
		  else {
		      entry->flags = ifr.ifr_flags;
		  }
	      }
	      flags = entry->flags;
	      bzero(&info, sizeof(info));
	      info.addr = ((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr;
	      ifr = *ifrp;
	      if (ioctl(sockfd, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
		  syslog(LOG_INFO, "%s: ioctl(SIOGIFNETMASK), %s (%d)",
			 name, strerror(errno), errno);
	      }
	      else {
		  info.mask = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
	      }
	      ifr = *ifrp;
	      if (flags & IFF_BROADCAST) {
		  if (ioctl(sockfd, SIOCGIFBRDADDR, (caddr_t)&ifr)< 0) {
		      syslog(LOG_INFO, "%s: ioctl(SIOCGBRDADDR), %s (%d)",
			     name, strerror(errno), errno);
		  }
		  else {
		      info.broadcast 
			  = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
		  }
	      }
	      info.netaddr.s_addr = htonl(iptohl(info.addr)
					  & iptohl(info.mask));
	      dynarray_add(&entry->inet, inet_addrinfo_copy(&info));
	      break;
	  }
	  case AF_LINK: {
	      struct sockaddr_dl * dl_p;
	      interface_t *	entry;
	      struct ifreq	ifr;
	      u_char 		name[IFNAMSIZ + 1];

	      dl_p = (struct sockaddr_dl *)&ifrp->ifr_addr;
	      strncpy(name, ifrp->ifr_name, sizeof(name));
	      name[IFNAMSIZ] = '\0';
	      entry = ifl_find_name(interfaces, name);
	      if (entry == NULL) { /* new entry */
		  entry = S_next_entry(interfaces, name);
		  if (entry == NULL) {
		      /* NOT REACHED */
		      syslog(LOG_ERR,
			     "interfaces: S_next_entry returns NULL"); 
		      continue;
		  }
		  ifr = *ifrp;
		  if (ioctl(sockfd, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		      syslog(LOG_INFO, "%s: ioctl(SIOGIFFLAGS), %s (%d)",
			     name, strerror(errno), errno);
		  }
		  else {
		      entry->flags = ifr.ifr_flags;
		  }
	      }
	      if (dl_p->sdl_alen > sizeof(entry->link.addr)) {
		  syslog(LOG_DEBUG,
			 "%s: link type %d address length %d > %d", name,
			 dl_p->sdl_type, dl_p->sdl_alen, sizeof(entry->link.addr));
		  entry->link.alen = sizeof(entry->link.addr);
	      }
	      else {
		  entry->link.alen = dl_p->sdl_alen;
	      }
	      bcopy(dl_p->sdl_data + dl_p->sdl_nlen, entry->link.addr, entry->link.alen);
	      entry->link.type = dl_p->sdl_type;
	      entry->link.index = dl_p->sdl_index;
	      break;
	  }
	}
    }
    /* make it the "right" size (plus 1 more in case it's 0) */
    interfaces->list = (interface_t *)
	realloc(interfaces->list, 
		sizeof(*(interfaces->list)) * (interfaces->count + 1));
    if (ifconf.ifc_buf)
	free(ifconf.ifc_buf);
    close(sockfd);
    return (TRUE);
  err:
    if (interfaces->list)
	free(interfaces->list);
    interfaces->list = NULL;
    if (ifconf.ifc_buf)
	free(ifconf.ifc_buf);
    close(sockfd);
    return (FALSE);
}

int
ifl_count(interface_list_t * list_p)
{
    return (list_p->count);
}

interface_t *		
ifl_at_index(interface_list_t * list_p, int i)
{
    if (i >= list_p->count || i < 0)
	return (NULL);
    return (list_p->list + i);
}

int
ifl_index(interface_list_t * list_p, interface_t * if_p)
{
    return (if_p - list_p->list);
}

/*
 * Function: ifl_first_broadcast_inet
 *
 * Purpose:
 *   Return the first non-loopback, broadcast capable interface.
 */
interface_t *
ifl_first_broadcast_inet(interface_list_t * list_p)
{
    int i;
    for (i = 0; i < list_p->count; i++) {
	interface_t * if_p = list_p->list + i;

	if (dynarray_count(&if_p->inet) > 0
	    && !(if_p->flags & IFF_LOOPBACK)
	    && (if_p->flags & IFF_BROADCAST))
	    return (list_p->list + i);
    }
    return (NULL);
}

interface_t *
ifl_find_ip(interface_list_t * list_p, struct in_addr iaddr)
{
    int 	i;

    for (i = 0; i < list_p->count; i++) {
	interface_t * if_p = list_p->list + i;
	int j;

	for (j = 0; j < dynarray_count(&if_p->inet); j++) {
	    inet_addrinfo_t *	info;

	    info = dynarray_element(&if_p->inet, j);
	    if (info->addr.s_addr == iaddr.s_addr)
		return (if_p);
	}
    }
    return (NULL);
}

interface_t *
ifl_find_subnet(interface_list_t * list_p, struct in_addr iaddr)
{
    int 	i;
    u_long	addr_hl = iptohl(iaddr);

    for (i = 0; i < list_p->count; i++) {
	interface_t * if_p = list_p->list + i;
	int j;

	for (j = 0; j < dynarray_count(&if_p->inet); j++) {
	    inet_addrinfo_t *	info = dynarray_element(&if_p->inet, j);
	    u_long 		ifnetaddr_hl = iptohl(info->netaddr);
	    u_long 		ifmask_hl = iptohl(info->mask);

	    if ((addr_hl & ifmask_hl) == ifnetaddr_hl)
		return (if_p);
	}
    }
    return (NULL);
}

interface_t *
ifl_find_name(interface_list_t * list_p, const char * name)
{
    int i;

    for (i = 0; i < list_p->count; i++) {
	if (strcmp(list_p->list[i].name, name) == 0)
	    return (list_p->list + i);
    }
    return (NULL);
}

interface_t *
ifl_find_link(interface_list_t * list_p, int index)
{
    int i;

    for (i = 0; i < list_p->count; i++) {
	if (list_p->list[i].link.type != 0
	    && list_p->list[i].link.index == index)
	    return (list_p->list + i);
    }
    return (NULL);
}

interface_list_t *
ifl_init()
{
    interface_list_t * list_p = (interface_list_t *)malloc(sizeof(*list_p));
    if (list_p == NULL
	|| S_build_interface_list(list_p) == FALSE) {
	if (list_p)
	    free(list_p);
	return (NULL);
    }
    return (list_p);
}

void
ifl_free(interface_list_t * * iflist)
{
    if (iflist != NULL && *iflist != NULL) {
	int 			i;
	interface_list_t * 	list_p = *iflist;
	
	for (i = 0; i < list_p->count; i++) {
	    dynarray_free(&list_p->list[i].inet);
	}
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
char *
if_name(interface_t * if_p)
{
    return (if_p->name);
}

short
if_flags(interface_t * if_p)
{
    return (if_p->flags);
}

int
if_inet_count(interface_t * if_p)
{
    return (dynarray_count(&if_p->inet));
}

inet_addrinfo_t *
if_inet_addr_at(interface_t * if_p, int i)
{
    return (dynarray_element(&if_p->inet, i));
}

/* get the primary address, mask, broadcast */
struct in_addr
if_inet_addr(interface_t * if_p)
{
    inet_addrinfo_t * 	info = if_inet_addr_at(if_p, 0);

    if (info == NULL) {
	struct in_addr	zeroes = { 0 };
	return (zeroes);
    }
    return (info->addr);
}

struct in_addr
if_inet_netmask(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->mask);
}

struct in_addr
if_inet_netaddr(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->netaddr);
}

struct in_addr
if_inet_broadcast(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->broadcast);
}

void
if_setflags(interface_t * if_p, short flags)
{
    if_p->flags = flags;
    return;
}

boolean_t
if_inet_valid(interface_t * if_p)
{
    return (dynarray_count(&if_p->inet) > 0);
}

void
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
    dynarray_free(&if_p->inet);
    free(if_p);
    *if_p_p = NULL;
    return;
}

interface_t *
if_dup(interface_t * intface)
{
    interface_t * new_p;

    new_p = (interface_t *)calloc(1, sizeof(*new_p));
    if (new_p == NULL) {
	return (NULL);
    }
    *new_p = *intface;
    (void)dynarray_dup(&new_p->inet, &intface->inet);
    return (new_p);
}

static __inline__ boolean_t
if_inet_addr_remove_at(interface_t * if_p, int i)
{
    return (dynarray_free_element(&if_p->inet, i));
}

int
if_inet_find_ip(interface_t * if_p, struct in_addr iaddr)
{
    int i;
    for (i = 0; i < if_inet_count(if_p); i++) {
	inet_addrinfo_t *	info = if_inet_addr_at(if_p, i);
	if (info->addr.s_addr == iaddr.s_addr) {
	    return (i);
	}
    }
    return (INDEX_BAD);
}

/*
 * Function: if_inet_addr_add
 * Purpose:
 *   Add an inet address to the list.
 * Note:
 *   Most recently added addresses always end up at the end.
 *   If the address was already in the list, deleted first
 *   then added to the end.
 */
boolean_t
if_inet_addr_add(interface_t * if_p, inet_addrinfo_t * info)
{
    int 		i = if_inet_find_ip(if_p, info->addr);
    
    if (i != INDEX_BAD) {
	if_inet_addr_remove_at(if_p, i);
    }
    return (dynarray_add(&if_p->inet, inet_addrinfo_copy(info)));

}

boolean_t
if_inet_addr_remove(interface_t * if_p, struct in_addr iaddr)
{
    int 	i = if_inet_find_ip(if_p, iaddr);

    if (i == INDEX_BAD)
	return (FALSE);
    return (if_inet_addr_remove_at(if_p, i));
}


int
if_link_type(interface_t * if_p)
{
    return (if_p->link.type);
}

int
if_link_dhcptype(interface_t * if_p)
{
    if (if_p->link.type == IFT_IEEE1394) {
	return (ARPHRD_IEEE1394_EUI64);
    }
    else {
	return (dl_to_arp_hwtype(if_p->link.type));
    }
}

int
if_link_arptype(interface_t * if_p)
{
    return (dl_to_arp_hwtype(if_p->link.type));
}


void *
if_link_address(interface_t * if_p)
{
    return (if_p->link.addr);
}

int
if_link_length(interface_t * if_p)
{
    return (if_p->link.alen);
}

#ifdef TEST_INTERFACES

#if 0
void
sockaddr_dl_print(struct sockaddr_dl * dl_p)
{
    int i;

    printf("link: len %d index %d family %d type 0x%x nlen %d alen %d"
	   " slen %d addr ", dl_p->sdl_len, 
	   dl_p->sdl_index,  dl_p->sdl_family, dl_p->sdl_type,
	   dl_p->sdl_nlen, dl_p->sdl_alen, dl_p->sdl_slen);
    for (i = 0; i < dl_p->sdl_alen; i++) 
	printf("%s%x", i ? ":" : "", 
	       ((unsigned char *)dl_p->sdl_data + dl_p->sdl_nlen)[i]);
    printf("\n");
}
#endif

void
link_addr_print(link_addr_t * link)
{
    int i;

    printf("link: index %d type 0x%x alen %d%s", link->index, link->type,
	   link->alen, link->alen > 0 ? " addr" : "");
    for (i = 0; i < link->alen; i++) {
	printf("%c%x", i ? ':' : ' ', link->addr[i]);
    }
    printf("\n");
}

void
ifl_print(interface_list_t * list_p)
{
    int i;
    int count = 0;
    
    printf("Interface count = %d\n", list_p->count);
    for (i = 0; i < list_p->count; i++) {
	interface_t * 	if_p = list_p->list + i;
	int		j;
	
	if (i > 0)
	    printf("\n");
	
	printf("%s:\n", if_name(if_p));
	
	for (j = 0; j < if_inet_count(if_p); j++) {
	    inet_addrinfo_t * info = if_inet_addr_at(if_p, j);
	    
	    printf("inet: %s", inet_ntoa(info->addr));
	    printf(" netmask %s", inet_ntoa(info->mask));
	    if (if_flags(if_p) & IFF_BROADCAST)
		printf(" %s\n", inet_ntoa(info->broadcast));
	    else
		printf("\n");
	}
	if (if_p->link.type != 0) {
	    link_addr_print(&if_p->link);
	}
	count++;
    }
    return;
}

int
main()
{
    interface_list_t * list_p = ifl_init(FALSE);
    if (list_p != NULL) {
	ifl_print(list_p);
    }
    exit(0);
}
#endif TEST_INTERFACES
