/*
 * Copyright (c) 1999-2024 Apple Inc. All rights reserved.
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
 * - get the list of interfaces in the system
 */

/*
 * Modification History
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <strings.h>
#include <netdb.h>
#include "interfaces.h"
#include <arpa/inet.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if_media.h>
#include <net/route.h>
#include <ifaddrs.h>
#include "util.h"
#include "IPConfigurationLog.h"
#include "symbol_scope.h"

#define my_log	SC_log

#if !NO_SYSTEMCONFIGURATION
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>
#endif /* !NO_SYSTEMCONFIGURATION */

STATIC boolean_t
S_get_ifmediareq(int s, const char * name, struct ifmediareq * ifmr_p);

STATIC uint64_t
S_get_eflags(int s, const char * name);

STATIC boolean_t
S_ifmediareq_get_is_wireless(struct ifmediareq * ifmr_p);

STATIC link_status_t
S_ifmediareq_get_link_status(struct ifmediareq * ifmr_p);

#if !NO_SYSTEMCONFIGURATION
STATIC uint8_t
S_interface_get_flags(const char * ifname)
{
    CFStringRef			ifname_cf;
    uint8_t			flags = 0;
    SCNetworkInterfaceRef	netif;

    ifname_cf = CFStringCreateWithCString(NULL, ifname,
					  kCFStringEncodingUTF8);
    netif = _SCNetworkInterfaceCreateWithBSDName(NULL, ifname_cf, 0);
    if (netif != NULL) {
	if (_SCNetworkInterfaceIsTetheredHotspot(netif)) {
	    flags |= kInterfaceTypeFlagIsTethered;
	}
	if (_SCNetworkInterfaceIsCarPlay(netif)) {
	    flags |= kInterfaceTypeFlagIsCarPlay;
	}
	CFRelease(netif);
    }
    CFRelease(ifname_cf);
    return (flags);
}

#else /* !NO_SYSTEMCONFIGURATION */

STATIC uint8_t
S_interface_get_flags(const char * ifname)
{
#pragma unused(ifname)
    return (0);
}
#endif /* !NO_SYSTEMCONFIGURATION */


PRIVATE_EXTERN void *
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

PRIVATE_EXTERN void
inet_addrinfo_free(void * p)
{
    free(p);
    return;
}

STATIC interface_t *
S_next_entry(interface_list_t * interfaces, const char * name)
{
    interface_t * entry;

    if (interfaces->count >= interfaces->size)
	return (NULL);

    entry = interfaces->list + interfaces->count++;
    bzero(entry, sizeof(*entry));
    strlcpy(entry->name, name, sizeof(entry->name));
    dynarray_init(&entry->inet, inet_addrinfo_free,
		  inet_addrinfo_copy);
    return (entry);
}

STATIC __inline__ int
count_ifaddrs(const struct ifaddrs * ifap)
{
    int		count;

    for (count = 0; ifap != NULL && ifap->ifa_addr != NULL; 
	 ifap = ifap->ifa_next) {
	count++;
    }
    return (count);
}

#if NO_SYSTEMCONFIGURATION || NO_WIRELESS

static bool
is_infra_wifi(const char * if_name)
{
    return (false);
}

#else /* NO_SYSTEMCONFIGURATION || NO_WIRELESS */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IO80211/Apple80211API.h>
#include <Kernel/IOKit/apple80211/apple80211_ioctl.h>

static boolean_t
is_infra_wifi(const char * if_name)
{
    CFStringRef		if_name_cf;
    boolean_t 		is_infra = FALSE;
    CFStringRef 	key = CFSTR(APPLE80211_REGKEY_INTERFACE_NAME);
    CFDictionaryRef	match_dict;
    CFDictionaryRef 	name_dict;
    CFStringRef		role;
    io_service_t 	service = MACH_PORT_NULL;

    if_name_cf = CFStringCreateWithCString(NULL, if_name,
					   kCFStringEncodingUTF8);
    name_dict = CFDictionaryCreate(NULL,
				   (const void * *)&key,
				   (const void * *)&if_name_cf,
				   1,
				   &kCFTypeDictionaryKeyCallBacks,
				   &kCFTypeDictionaryValueCallBacks);
    CFRelease(if_name_cf);
    key = CFSTR(kIOPropertyMatchKey);
    match_dict = CFDictionaryCreate(NULL,
				    (const void * *)&key,
				    (const void * *)&name_dict,
				    1,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);
    CFRelease(name_dict);

    /* match_dict is released by IOServiceGetMatchingService :-( */
    service = IOServiceGetMatchingService(kIOMainPortDefault, match_dict);
    if (service == 0) {
	goto done;
    }
    role = IORegistryEntryCreateCFProperty(service,
					   CFSTR(APPLE80211_REGKEY_IF_ROLE),
					   kCFAllocatorDefault, 0);
    if (role != NULL) {
	is_infra = (isA_CFString(role) != NULL)
	    && CFEqual(role, CFSTR(APPLE80211_IF_ROLE_STR_INFRA));
	CFRelease(role);
    }
    IOObjectRelease(service);

 done:
    return (is_infra);
}


#endif /* NO_SYSTEMCONFIGURATION || NO_WIRELESS */

#ifdef TEST_INTERFACE_CHANGES
#include <sys/queue.h>

typedef struct IFNameIndex IFNameIndex, *IFNameIndexRef;

#define LIST_HEAD_IFNameIndex LIST_HEAD(IFNameIndexHead, IFNameIndex)
#define LIST_ENTRY_IFNameIndex LIST_ENTRY(IFNameIndex)

LIST_HEAD_IFNameIndex	S_IFNameIndexHead;
#define IFNAME_INDEX_HEAD	&S_IFNameIndexHead

struct IFNameIndex {
    LIST_ENTRY_IFNameIndex	link;
    char			name[IFNAMSIZ];
    int				index;
};

static IFNameIndexRef
IFNameIndexCreate(const char * name, int index)
{
    IFNameIndexRef	entry;

    entry = malloc(sizeof(*entry));
    bzero(entry, sizeof(*entry));
    strlcpy(entry->name, name, sizeof(entry->name));
    entry->index = index;
    return (entry);
}

static IFNameIndexRef
IFNameIndexLookupByName(const char * name)
{
    IFNameIndexRef	entry;

    LIST_FOREACH(entry, IFNAME_INDEX_HEAD, link) {
	if (strcmp(entry->name, name) == 0) {
	    return (entry);
	}
    }
    return (NULL);
}

static IFNameIndexRef
IFNameIndexLookupByIndex(int index)
{
    IFNameIndexRef	entry;

    LIST_FOREACH(entry, IFNAME_INDEX_HEAD, link) {
	if (entry->index == index) {
	    return (entry);
	}
    }
    return (NULL);
}

static void
IFNameIndexListDump(void)
{
    IFNameIndexRef	entry;

    LIST_FOREACH(entry, IFNAME_INDEX_HEAD, link) {
	printf("%s (%d)\n", entry->name, entry->index);
    }
}

static void
IFNameIndexListAddEntry(const char * name, int index)
{
    IFNameIndexRef entry;

    entry = IFNameIndexLookupByName(name);
    if (entry != NULL) {
	if (entry->index != index) {
	    fprintf(stderr, "%s (%d) has existing index %d\n",
		    name, index, entry->index);
	    IFNameIndexListDump();
	}
	return;
    }
    entry = IFNameIndexLookupByIndex(index);
    if (entry != NULL) {
	if (strcmp(entry->name, name) != 0) {
	    fprintf(stderr, "%s (%d) used by existing %s (%d)\n",
		    name, index, entry->name, entry->index);
	    IFNameIndexListDump();
	}
	return;
    }
    entry = IFNameIndexCreate(name, index);
    LIST_INSERT_HEAD(IFNAME_INDEX_HEAD, entry, link);
}
#endif /* TEST_INTERFACE_CHANGES */


STATIC boolean_t
S_build_interface_list(interface_list_t * interfaces)
{
    struct ifaddrs *	addrs = NULL;
    struct ifaddrs *	ifap = NULL;
    struct ifmediareq	ifmr;
    int			s = -1;
    int			size;
    boolean_t		success = FALSE;

    interfaces->list = NULL;
    if (getifaddrs(&addrs) < 0) {
	goto done;
    }
    size = count_ifaddrs(addrs);
    if (size == 0) {
	goto done;
    }
    interfaces->list 
	= (interface_t *)malloc(size * sizeof(*(interfaces->list)));
    if (interfaces->list == NULL) {
	goto done;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	goto done;
    }
    interfaces->count = 0;
    interfaces->size = size;
    
    for (ifap = addrs; ifap != NULL; ifap = ifap->ifa_next) {
	const char *	name;
	if (ifap->ifa_addr == NULL) {
	    continue;
	}
	name = ifap->ifa_name;
	switch (ifap->ifa_addr->sa_family) {
	  case AF_INET: {
	      inet_addrinfo_t	info;
	      interface_t *	entry;

	      entry = ifl_find_name(interfaces, name);
	      if (entry == NULL) { /* new entry */
		  entry = S_next_entry(interfaces, name);
		  if (entry == NULL) {
		      /* NOT REACHED */
		      my_log(LOG_NOTICE,
			     "interfaces: S_next_entry returns NULL"); 
		      continue;
		  }
		  entry->flags = ifap->ifa_flags;
	      }
	      bzero(&info, sizeof(info));
	
	      /* ALIGN: getifaddrs should align, cast ok. */	
	      info.addr = ((struct sockaddr_in *)
			   (void *)ifap->ifa_addr)->sin_addr;

	      if (ifap->ifa_netmask != NULL) {
		  /* ALIGN: getifaddrs should align, cast ok. */
		  info.mask 
		      = ((struct sockaddr_in *)
			 (void *)ifap->ifa_netmask)->sin_addr;
	      }
	      if (entry->flags & IFF_BROADCAST && ifap->ifa_broadaddr != NULL) {
		  /* ALIGN: getifaddrs should align, cast ok. */
		  info.broadcast 
		      = ((struct sockaddr_in *)(void *)
				ifap->ifa_broadaddr)->sin_addr;
	      }
	      info.netaddr.s_addr = info.addr.s_addr & info.mask.s_addr;
	      dynarray_add(&entry->inet, inet_addrinfo_copy(&info));
	      break;
	  }
	  case AF_LINK: {
	      struct sockaddr_dl * dl_p;
	      interface_t *	entry;
	      struct if_data *	if_data;
		
	      /* ALIGN: getifaddrs should align, cast ok. */
	      dl_p = (struct sockaddr_dl *)
		     (void *)ifap->ifa_addr;

	      entry = ifl_find_name(interfaces, name);
	      if (entry == NULL) { /* new entry */
		  entry = S_next_entry(interfaces, name);
		  if (entry == NULL) {
		      /* NOT REACHED */
		      my_log(LOG_NOTICE,
			     "interfaces: S_next_entry returns NULL"); 
		      continue;
		  }
		  entry->flags = ifap->ifa_flags;
	      }
	      if (dl_p->sdl_alen > sizeof(entry->link_address.addr)) {
		  my_log(LOG_NOTICE,
			 "%s: link type %d address length %d > %ld", name,
			 dl_p->sdl_type, dl_p->sdl_alen,
			 sizeof(entry->link_address.addr));
		  entry->link_address.length = sizeof(entry->link_address.addr);
	      }
	      else {
		  entry->link_address.length = dl_p->sdl_alen;
	      }
	      bcopy(dl_p->sdl_data + dl_p->sdl_nlen, 
		    entry->link_address.addr,
		    entry->link_address.length);
	      entry->link_address.type = dl_p->sdl_type;
	      entry->link_address.index = dl_p->sdl_index;
#ifdef TEST_INTERFACE_CHANGES
	      IFNameIndexListAddEntry(name, dl_p->sdl_index);
#endif /* TEST_INTERFACE_CHANGES */
	      if_data = (struct if_data *)ifap->ifa_data;
	      if (if_data != NULL) {
		  entry->type = if_data->ifi_type;
	      }
	      else {
		  entry->type = dl_p->sdl_type;
	      }
	      if (S_get_ifmediareq(s, name, &ifmr)) {
		  if (entry->type == IFT_ETHER) {
		      uint64_t	eflags;

		      eflags = S_get_eflags(s, name);
		      if ((eflags & IFEF_EXPENSIVE) != 0) {
			  entry->type_flags |= kInterfaceTypeFlagIsExpensive;
		      }
		      if (S_ifmediareq_get_is_wireless(&ifmr)) {
			  entry->type_flags |= kInterfaceTypeFlagIsWireless;
			  if ((eflags & IFEF_AWDL) != 0) {
			      entry->type_flags |= kInterfaceTypeFlagIsAWDL;
			  }
			  else if (is_infra_wifi(name)) {
			      entry->type_flags |= kInterfaceTypeFlagIsWiFiInfra;
			  }
		      }
		      else {
			  uint8_t	flags;

			  flags = S_interface_get_flags(name);
			  if (flags != 0) {
			      entry->type_flags |= flags;
			  }
		      }
		  }
		  entry->link_status
		      = S_ifmediareq_get_link_status(&ifmr);
	      }
	      break;
	  }
	}
    }
    /* make it the "right" size (plus 1 more in case it's 0) */
    interfaces->list = (interface_t *)
	reallocf(interfaces->list,
		 sizeof(*(interfaces->list)) * (interfaces->count + 1));
    success = TRUE;

 done:
    if (addrs != NULL) {
	freeifaddrs(addrs);
    }
    if (success == FALSE) {
	if (interfaces->list != NULL) {
	    free(interfaces->list);
	    interfaces->list = NULL;
	}
    }
    if (s >= 0) {
	close(s);
    }
    return (success);
}

PRIVATE_EXTERN int
ifl_count(interface_list_t * list_p)
{
    return ((list_p != NULL) ? list_p->count: 0);
}

PRIVATE_EXTERN interface_t *		
ifl_at_index(interface_list_t * list_p, int i)
{
    if (i >= ifl_count(list_p) || i < 0)
	return (NULL);
    return (list_p->list + i);
}

/*
 * Function: ifl_first_broadcast_inet
 *
 * Purpose:
 *   Return the first non-loopback, broadcast capable interface.
 */
PRIVATE_EXTERN interface_t *
ifl_first_broadcast_inet(interface_list_t * list_p)
{
    int i;

    for (i = 0; i < ifl_count(list_p); i++) {
	interface_t * if_p = list_p->list + i;

	if (dynarray_count(&if_p->inet) > 0
	    && !(if_p->flags & IFF_LOOPBACK)
	    && (if_p->flags & IFF_BROADCAST))
	    return (list_p->list + i);
    }
    return (NULL);
}

PRIVATE_EXTERN interface_t *
ifl_find_ip(interface_list_t * list_p, struct in_addr iaddr)
{
    int 	i;

    for (i = 0; i < ifl_count(list_p); i++) {
	interface_t * if_p = list_p->list + i;
	int j;

	for (j = 0; j < dynarray_count(&if_p->inet); j++) {
	    inet_addrinfo_t *	info;

	    info = dynarray_element(&if_p->inet, j);
	    if (info->addr.s_addr == iaddr.s_addr) {
		return (if_p);
	    }
	}
    }
    return (NULL);
}

PRIVATE_EXTERN interface_t *
ifl_find_name(interface_list_t * list_p, const char * name)
{
    int i;

    for (i = 0; i < ifl_count(list_p); i++) {
	if (strcmp(list_p->list[i].name, name) == 0)
	    return (list_p->list + i);
    }
    return (NULL);
}

PRIVATE_EXTERN interface_t *
ifl_find_stable_interface(interface_list_t * list_p)
{
    interface_t * if_p;

    if_p = ifl_find_name(list_p, "en0");
    if (if_p == NULL) {
	int		count;
	int		i;
	interface_t *  	if_with_linkaddr_p = NULL;

	count = ifl_count(list_p);
	for (i = 0; i < count; i++) {
	    interface_t *	scan = ifl_at_index(list_p, i);

	    switch (if_ift_type(scan)) {
	    case IFT_ETHER:
	    case IFT_IEEE1394:
		break;
	    default:
		if (if_with_linkaddr_p == NULL
		    && if_link_length(scan) > 0) {
		    if_with_linkaddr_p = scan;
		}
		continue;
	    }
	    if (if_p == NULL) {
		if_p = scan;
	    }
	    else if (strcmp(if_name(scan), if_name(if_p)) < 0) {
		/* pick "lowest" named interface */
		if_p = scan;
	    }
	}
	if (if_p == NULL && if_with_linkaddr_p != NULL) {
	    if_p = if_with_linkaddr_p;
	}
    }
    return (if_p);
}

PRIVATE_EXTERN interface_list_t *
ifl_init(void)
{
    interface_list_t * list_p = (interface_list_t *)malloc(sizeof(*list_p));
    if (list_p == NULL
	|| S_build_interface_list(list_p) == FALSE) {
	if (list_p != NULL)
	    free(list_p);
	return (NULL);
    }
    return (list_p);
}

PRIVATE_EXTERN void
ifl_free(interface_list_t * * iflist)
{
    if (iflist != NULL && *iflist != NULL) {
	int 			i;
	interface_list_t * 	list_p = *iflist;
	
	for (i = 0; i < ifl_count(list_p); i++) {
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
PRIVATE_EXTERN const char *
if_name(interface_t * if_p)
{
    return (if_p->name);
}

PRIVATE_EXTERN uint16_t
if_flags(interface_t * if_p)
{
    return (if_p->flags);
}

PRIVATE_EXTERN int
if_inet_count(interface_t * if_p)
{
    return (dynarray_count(&if_p->inet));
}

PRIVATE_EXTERN inet_addrinfo_t *
if_inet_addr_at(interface_t * if_p, int i)
{
    return (dynarray_element(&if_p->inet, i));
}

/* get the primary address, mask, broadcast */
PRIVATE_EXTERN struct in_addr
if_inet_addr(interface_t * if_p)
{
    inet_addrinfo_t * 	info = if_inet_addr_at(if_p, 0);

    if (info == NULL) {
	struct in_addr	zeroes = { 0 };
	return (zeroes);
    }
    return (info->addr);
}

PRIVATE_EXTERN struct in_addr
if_inet_netmask(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->mask);
}

PRIVATE_EXTERN struct in_addr
if_inet_netaddr(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->netaddr);
}

PRIVATE_EXTERN struct in_addr
if_inet_broadcast(interface_t * if_p)
{
    inet_addrinfo_t * info = if_inet_addr_at(if_p, 0);
    return (info->broadcast);
}

PRIVATE_EXTERN void
if_setflags(interface_t * if_p, uint16_t flags)
{
    if_p->flags = flags;
    return;
}

PRIVATE_EXTERN boolean_t
if_inet_valid(interface_t * if_p)
{
    return (dynarray_count(&if_p->inet) > 0);
}

PRIVATE_EXTERN void
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

PRIVATE_EXTERN interface_t *
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

PRIVATE_EXTERN int
if_inet_match_subnet(interface_t * if_p, struct in_addr match)
{
    int count = if_inet_count(if_p);
    int i;

    for (i = 0; i < count; i++) {
	inet_addrinfo_t *	info = if_inet_addr_at(if_p, i);

	if (in_subnet(info->netaddr, info->mask, match)) {
	    return (i);
	}
    }
    return (INDEX_BAD);
}

PRIVATE_EXTERN void
if_link_copy(interface_t * dest, const interface_t * source)
{
    dest->type_flags = source->type_flags;
    dest->link_status = source->link_status;
    dest->link_address = source->link_address;
    return;
}

PRIVATE_EXTERN boolean_t
if_link_update(interface_t * if_p)
{
    void *			addr;
    int				addr_length;
    char *			buf = NULL;
    size_t			buf_len = 0;
    boolean_t			changed = FALSE;
    struct sockaddr_dl *	dl_p;
    struct if_msghdr * 		ifm;
    int				mib[6];
    int				type;

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
    if (buf_len < (sizeof(*ifm) + sizeof(*dl_p))) {
	/* buf_len == 0 because interface is gone <rdar://problem/33365207> */
	goto failed;
    }
    /* ALIGN: buf is aligned (from malloc), cast ok. */
    ifm = (struct if_msghdr *)(void *)buf;
    if (ifm->ifm_type == RTM_IFINFO) {
	dl_p = (struct sockaddr_dl *)(ifm + 1);
	addr = dl_p->sdl_data + dl_p->sdl_nlen;
	addr_length = dl_p->sdl_alen;
	type = dl_p->sdl_type;
	if (addr_length > sizeof(if_p->link_address.addr)) {
	    my_log(LOG_DEBUG,
		   "%s: link type %d address length %d > %ld",
		   if_name(if_p), 
		   type,
		   addr_length, 
		   sizeof(if_p->link_address.addr));
	    addr_length = sizeof(if_p->link_address.addr);
	}
	if (if_p->link_address.type != type
	    || addr_length != if_p->link_address.length
	    || (addr_length != 0
		&& bcmp(addr, if_p->link_address.addr, addr_length))) {
	    changed = TRUE;
	    if_p->link_address.length = addr_length;
	    bcopy(addr, if_p->link_address.addr, addr_length);
	    if_p->link_address.type = dl_p->sdl_type;
	}
    }
 failed:
    if (buf != NULL) {
	free(buf);
    }
    return (changed);
}

PRIVATE_EXTERN int
if_ift_type(interface_t * if_p)
{
    return (if_p->type);
}

PRIVATE_EXTERN int
if_link_type(interface_t * if_p)
{
    return (if_p->link_address.type);
}

PRIVATE_EXTERN int
if_link_dhcptype(interface_t * if_p)
{
    if (if_p->link_address.type == IFT_IEEE1394) {
	return (ARPHRD_IEEE1394_EUI64);
    }
    else {
	return (dl_to_arp_hwtype(if_p->link_address.type));
    }
}

PRIVATE_EXTERN int
if_link_arptype(interface_t * if_p)
{
    return (dl_to_arp_hwtype(if_p->link_address.type));
}


PRIVATE_EXTERN void *
if_link_address(interface_t * if_p)
{
    return (if_p->link_address.addr);
}

PRIVATE_EXTERN int
if_link_length(interface_t * if_p)
{
    return (if_p->link_address.length);
}

PRIVATE_EXTERN boolean_t
if_link_address_is_private(interface_t * if_p)
{
    link_addr_t *	addr = &if_p->link_address;

#define LOCAL_ADMIN	0x2
    return (addr->type == IFT_ETHER && addr->length == ETHER_ADDR_LEN
	    && (addr->addr[0] & LOCAL_ADMIN) != 0);
}

PRIVATE_EXTERN boolean_t
if_is_ethernet(interface_t * if_p)
{
    return (if_ift_type(if_p) == IFT_ETHER && !if_is_wireless(if_p));
}

PRIVATE_EXTERN boolean_t
if_is_wireless(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsWireless) != 0);
}

PRIVATE_EXTERN boolean_t
if_is_wifi_infra(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsWiFiInfra) != 0);
}

PRIVATE_EXTERN boolean_t
if_is_awdl(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsAWDL) != 0);
}

PRIVATE_EXTERN boolean_t
if_is_tethered(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsTethered) != 0);
}

PRIVATE_EXTERN boolean_t
if_is_carplay(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsCarPlay) != 0);
}

PRIVATE_EXTERN boolean_t
if_is_expensive(interface_t * if_p)
{
    return ((if_p->type_flags & kInterfaceTypeFlagIsExpensive) != 0);
}

PRIVATE_EXTERN const char *
if_type_string(interface_t * if_p)
{
    const char *	str = NULL;

    if (if_is_awdl(if_p)) {
	str = "AWDL";
    }
    else if (if_is_wireless(if_p)) {
	str = "WiFi";
    }
    else {
	switch (if_ift_type(if_p)) {
	case IFT_ETHER:
	    str = "Ethernet";
	    break;
	case IFT_CELLULAR:
	    str = "Cellular";
	    break;
	case IFT_BRIDGE:
	    str = "Bridge";
	    break;
	case IFT_L2VLAN:
	    str = "VLAN";
	    break;
	case IFT_IEEE8023ADLAG:
	    str = "Link Aggregate";
	    break;
	case IFT_IEEE1394:
	    str = "Firewire";
	    break;
	case IFT_STF:
	    str = "6to4";
	    break;
	default:
	    break;
	}
    }
    return (str);
}

PRIVATE_EXTERN int
if_link_index(interface_t * if_p)
{
    return (if_p->link_address.index);
}

PRIVATE_EXTERN link_status_t 
if_get_link_status(interface_t * if_p)
{
    return (if_p->link_status);
}

STATIC int
siocgifmedia(int sockfd, struct ifmediareq * ifmr_p,
	     const char * name)
{
    (void)memset(ifmr_p, 0, sizeof(*ifmr_p));
    (void)strlcpy(ifmr_p->ifm_name, name, sizeof(ifmr_p->ifm_name));
    return (ioctl(sockfd, SIOCGIFMEDIA, (caddr_t)ifmr_p));
}

STATIC boolean_t
S_get_ifmediareq(int s, const char * name, struct ifmediareq * ifmr_p)
{
    boolean_t	ret;

    if (siocgifmedia(s, ifmr_p, name) == -1) {
	if (errno == EOPNOTSUPP) {
	    ifmr_p->ifm_status = IFM_ACTIVE | IFM_AVALID;
    	    ifmr_p->ifm_count = 1;
	    ret = TRUE;
	}
	else {
	    ret = FALSE;
	}
    }
    else {
	ret = TRUE;
    }
    return (ret);
}

STATIC boolean_t
S_ifmediareq_get_is_wireless(struct ifmediareq * ifmr_p)
{
    return (IFM_TYPE(ifmr_p->ifm_current) == IFM_IEEE80211);
}

STATIC link_status_t
S_ifmediareq_get_link_status(struct ifmediareq * ifmr_p)
{
    link_status_t 	link;

    bzero(&link, sizeof(link));
    if (ifmr_p->ifm_count > 0 && (ifmr_p->ifm_status & IFM_AVALID) != 0) {
	link.valid = TRUE;
	if ((ifmr_p->ifm_status & IFM_ACTIVE) != 0) {
	    link.active = TRUE;
	}
	link.wake_on_same_network
	    = ((ifmr_p->ifm_status & IFM_WAKESAMENET) != 0);
    }
    return (link);
}

STATIC int
siocgifeflags(int sockfd, struct ifreq * ifr, const char * name)
{
    (void)memset(ifr, 0, sizeof(*ifr));
    (void)strlcpy(ifr->ifr_name, name, sizeof(ifr->ifr_name));
    return (ioctl(sockfd, SIOCGIFEFLAGS, (caddr_t)ifr));
}

STATIC uint64_t
S_get_eflags(int sockfd, const char * name)
{
    uint64_t		eflags = 0;
    struct ifreq	ifr;

    if (siocgifeflags(sockfd, &ifr, name) == -1) {
	if (errno != ENXIO && errno != EPWROFF && errno != EINVAL) {
	    my_log(LOG_NOTICE,
		   "%s: SIOCGIFEFLAGS failed status, %s",
		   name, strerror(errno));
	}
    }
    else {
	eflags = ifr.ifr_eflags;
    }
    return (eflags);
}

PRIVATE_EXTERN link_status_t
if_link_status_update(interface_t * if_p)
{
    struct ifmediareq 	ifmr;
    int			s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
	uint16_t	eflags;

	if (S_get_ifmediareq(s, if_name(if_p), &ifmr) == FALSE) {
	    if (errno != ENXIO && errno != EPWROFF && errno != EINVAL) {
		my_log(LOG_NOTICE,
		       "%s: failed to get media status, %s",
		       if_name(if_p), strerror(errno));
	    }
	}
	else {
	    if_p->link_status = S_ifmediareq_get_link_status(&ifmr);
	}
	eflags = S_get_eflags(s, if_name(if_p));
	if ((eflags & IFEF_EXPENSIVE) != 0) {
	    if_p->type_flags |= kInterfaceTypeFlagIsExpensive;
	}
	else {
	    if_p->type_flags &= ~kInterfaceTypeFlagIsExpensive;
	}
	close(s);
    }
    return (if_p->link_status);
}


#ifdef TEST_INTERFACES

static const char *
get_ift_type_string(int ift_type)
{
	static char buf[32];
	const char * str;

	switch (ift_type) {
	case IFT_OTHER:
		str = "IFT_OTHER";
		break;
	case IFT_1822:
		str = "IFT_1822";
		break;
	case IFT_HDH1822:
		str = "IFT_HDH1822";
		break;
	case IFT_X25DDN:
		str = "IFT_X25DDN";
		break;
	case IFT_X25:
		str = "IFT_X25";
		break;
	case IFT_ETHER:
		str = "IFT_ETHER";
		break;
	case IFT_ISO88023:
		str = "IFT_ISO88023";
		break;
	case IFT_ISO88024:
		str = "IFT_ISO88024";
		break;
	case IFT_ISO88025:
		str = "IFT_ISO88025";
		break;
	case IFT_ISO88026:
		str = "IFT_ISO88026";
		break;
	case IFT_STARLAN:
		str = "IFT_STARLAN";
		break;
	case IFT_P10:
		str = "IFT_P10";
		break;
	case IFT_P80:
		str = "IFT_P80";
		break;
	case IFT_HY:
		str = "IFT_HY";
		break;
	case IFT_FDDI:
		str = "IFT_FDDI";
		break;
	case IFT_LAPB:
		str = "IFT_LAPB";
		break;
	case IFT_SDLC:
		str = "IFT_SDLC";
		break;
	case IFT_T1:
		str = "IFT_T1";
		break;
	case IFT_CEPT:
		str = "IFT_CEPT";
		break;
	case IFT_ISDNBASIC:
		str = "IFT_ISDNBASIC";
		break;
	case IFT_ISDNPRIMARY:
		str = "IFT_ISDNPRIMARY";
		break;
	case IFT_PTPSERIAL:
		str = "IFT_PTPSERIAL";
		break;
	case IFT_PPP:
		str = "IFT_PPP";
		break;
	case IFT_LOOP:
		str = "IFT_LOOP";
		break;
	case IFT_EON:
		str = "IFT_EON";
		break;
	case IFT_XETHER:
		str = "IFT_XETHER";
		break;
	case IFT_NSIP:
		str = "IFT_NSIP";
		break;
	case IFT_SLIP:
		str = "IFT_SLIP";
		break;
	case IFT_ULTRA:
		str = "IFT_ULTRA";
		break;
	case IFT_DS3:
		str = "IFT_DS3";
		break;
	case IFT_SIP:
		str = "IFT_SIP";
		break;
	case IFT_FRELAY:
		str = "IFT_FRELAY";
		break;
	case IFT_RS232:
		str = "IFT_RS232";
		break;
	case IFT_PARA:
		str = "IFT_PARA";
		break;
	case IFT_ARCNET:
		str = "IFT_ARCNET";
		break;
	case IFT_ARCNETPLUS:
		str = "IFT_ARCNETPLUS";
		break;
	case IFT_ATM:
		str = "IFT_ATM";
		break;
	case IFT_MIOX25:
		str = "IFT_MIOX25";
		break;
	case IFT_SONET:
		str = "IFT_SONET";
		break;
	case IFT_X25PLE:
		str = "IFT_X25PLE";
		break;
	case IFT_ISO88022LLC:
		str = "IFT_ISO88022LLC";
		break;
	case IFT_LOCALTALK  :
		str = "IFT_LOCALTALK";
		break;
	case IFT_SMDSDXI:
		str = "IFT_SMDSDXI";
		break;
	case IFT_FRELAYDCE:
		str = "IFT_FRELAYDCE";
		break;
	case IFT_V35:
		str = "IFT_V35";
		break;
	case IFT_HSSI:
		str = "IFT_HSSI";
		break;
	case IFT_HIPPI:
		str = "IFT_HIPPI";
		break;
	case IFT_MODEM:
		str = "IFT_MODEM";
		break;
	case IFT_AAL5:
		str = "IFT_AAL5";
		break;
	case IFT_SONETPATH:
		str = "IFT_SONETPATH";
		break;
	case IFT_SONETVT:
		str = "IFT_SONETVT";
		break;
	case IFT_SMDSICIP:
		str = "IFT_SMDSICIP";
		break;
	case IFT_PROPVIRTUAL:
		str = "IFT_PROPVIRTUAL";
		break;
	case IFT_PROPMUX:
		str = "IFT_PROPMUX";
		break;
	case IFT_GIF:
		str = "IFT_GIF";
		break;
	case IFT_FAITH:
		str = "IFT_FAITH";
		break;
	case IFT_STF:
		str = "IFT_STF";
		break;
	case IFT_6LOWPAN:
		str = "IFT_6LOWPAN";
		break;
	case IFT_L2VLAN:
		str = "IFT_L2VLAN";
		break;
	case IFT_IEEE8023ADLAG:
		str = "IFT_IEEE8023ADLAG";
		break;
	case IFT_IEEE1394:
		str = "IFT_IEEE1394";
		break;
	case IFT_BRIDGE:
		str = "IFT_BRIDGE";
		break;
	case IFT_ENC:
		str = "IFT_ENC";
		break;
	case IFT_PFLOG:
		str = "IFT_PFLOG";
		break;
	case IFT_PFSYNC:
		str = "IFT_PFSYNC";
		break;
	case IFT_CARP:
		str = "IFT_CARP";
		break;
	case IFT_PKTAP:
		str = "IFT_PKTAP";
		break;
	case IFT_CELLULAR:
		str = "IFT_CELLULAR";
		break;
	default:
		snprintf(buf, sizeof(buf), "IFT_(0x%x)", ift_type);
		str = buf;
		break;
	}
	return (str);
}

static void
link_addr_print(link_addr_t * link)
{
    int i;

    printf("link: index %d type 0x%x alen %d%s", link->index, link->type,
	   link->length, link->length > 0 ? " addr" : "");
    for (i = 0; i < link->length; i++) {
	printf("%c%x", i ? ':' : ' ', link->addr[i]);
    }
    printf("\n");
}

void
ifl_print(interface_list_t * list_p)
{
    int i;
    int count = 0;
    
    printf("Interface count = %d\n", ifl_count(list_p));
    for (i = 0; i < ifl_count(list_p); i++) {
	interface_t * 	if_p = list_p->list + i;
	int		j;
	
	if (i > 0)
	    printf("\n");
	
	printf("%s: type %s (%d) ift_type %s (%d)\n",
	       if_name(if_p),
	       get_ift_type_string(if_link_type(if_p)),
	       if_link_type(if_p),
	       get_ift_type_string(if_ift_type(if_p)),
	       if_ift_type(if_p));
	
	for (j = 0; j < if_inet_count(if_p); j++) {
	    inet_addrinfo_t * 	info = if_inet_addr_at(if_p, j);
	    char		ntopbuf[INET_ADDRSTRLEN];
	    
	    printf("inet: %s",
		   inet_ntop(AF_INET, &info->addr, ntopbuf, sizeof(ntopbuf)));
	    printf(" netmask %s",
		   inet_ntop(AF_INET, &info->mask, ntopbuf, sizeof(ntopbuf)));
	    if (if_flags(if_p) & IFF_BROADCAST) {
		printf(" broadcast %s",
		       inet_ntop(AF_INET, &info->broadcast,
				 ntopbuf, sizeof(ntopbuf)));
		printf(" netaddr %s\n",
		       inet_ntop(AF_INET, &info->netaddr,
				 ntopbuf, sizeof(ntopbuf)));
	    }
	    else {
		printf("\n");
	    }
	}
	if (if_p->link_address.type != 0) {
	    link_addr_print(&if_p->link_address);
	    if (if_p->link_status.valid) {
		printf("Link is %s%s",
		       if_p->link_status.active ? "active" : "inactive",
		       if_p->link_status.wake_on_same_network
		       ? " [wake on same network]" : "");
	    }
	    if (if_is_wireless(if_p)) {
		const char *	label;

		if (if_is_wifi_infra(if_p)) {
		    label = " [infra]";
		}
		else if (if_is_awdl(if_p)) {
		    label = " [awdl]";
		}
		else {
		    label = "";
		}
		printf(" [wireless]%s", label);
	    }
	    if (if_is_tethered(if_p)) {
		printf(" [tethered]");
	    }
	    if (if_is_expensive(if_p)) {
		printf(" [expensive]");
	    }
	    if (if_is_carplay(if_p)) {
		printf(" [carplay]");
	    }
	    printf("\n");
	}
	count++;
    }
    return;
}

int
main()
{
    interface_list_t * list_p = ifl_init();
    if (list_p != NULL) {
	ifl_print(list_p);
    }
    exit(0);
}
#endif /* TEST_INTERFACES */

#ifdef TEST_INTERFACE_CHANGES

/*
 * Function: timestamp_fprintf
 *
 * Purpose:
 *   Print a timestamped message.
 */
static void
timestamp_fprintf(FILE * f, const char * message, ...)
{
    struct timeval	tv;
    struct tm       	tm;
    time_t		t;
    va_list		ap;

    (void)gettimeofday(&tv, NULL);
    t = tv.tv_sec;
    (void)localtime_r(&t, &tm);

    va_start(ap, message);
    fprintf(f, "%04d/%02d/%02d %2d:%02d:%02d.%06d ",
	    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	    tm.tm_hour, tm.tm_min, tm.tm_sec,
	    tv.tv_usec);
    vfprintf(f, message, ap);
    va_end(ap);
}

static void
link_addr_print(const char * name, link_addr_t * link)
{
    int i;

    timestamp_fprintf(stdout,
		      "%s: index %d type 0x%x alen %d%s",
		      name, link->index, link->type,
		      link->length, link->length > 0 ? " addr" : "");
    for (i = 0; i < link->length; i++) {
	printf("%c%x", i ? ':' : ' ', link->addr[i]);
    }
    printf("\n");
}

static void
ifl_compare(interface_list_t * old_p, interface_list_t * new_p)
{
    int i;

    for (i = 0; i < ifl_count(new_p); i++) {
	interface_t * 	new_if_p = new_p->list + i;
	interface_t *	old_if_p = NULL;

	if (old_p != NULL) {
	    old_if_p = ifl_find_name(old_p, if_name(new_if_p));
	}
	if (old_if_p == NULL) {
	    link_addr_print(if_name(new_if_p), &new_if_p->link_address);
	    continue;
	}
	if (if_link_index(new_if_p) != if_link_index(old_if_p)) {
	    printf("%s index changed %d => %d\n",
		   if_name(new_if_p),
		   if_link_index(old_if_p),
		   if_link_index(new_if_p));
	}
    }
    return;
}

static void
interface_list_changed(void)
{
    interface_list_t * 		new_list_p = ifl_init();
    static interface_list_t * 	S_list_p;

    ifl_compare(S_list_p, new_list_p);
    if (S_list_p != NULL) {
	ifl_free(&S_list_p);
    }
    S_list_p = new_list_p;
}

static void
notification_callback(SCDynamicStoreRef session, CFArrayRef changes,
		      void * info)
{
#pragma unused(session)
#pragma unused(changes)
#pragma unused(info)
    interface_list_changed();
}

static bool
register_for_notifications(void)
{
    SCDynamicStoreRef 	session;
    CFStringRef		key;
    CFMutableArrayRef	keys;
    CFRunLoopSourceRef	rls;

    session = SCDynamicStoreCreate(NULL, CFSTR("interfaces.c"),
				   notification_callback,
                                   NULL);
    if (session == NULL) {
	fprintf(stderr, "SCDOpen failed: %s\n", SCErrorString(SCError()));
	return (false);
    }
    /* watch State:/Network/Interface */
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    key = SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    CFArrayAppendValue(keys, key);
    CFRelease(key);
    SCDynamicStoreSetNotificationKeys(session, keys, NULL);
    CFRelease(keys);
    rls = SCDynamicStoreCreateRunLoopSource(NULL, session, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    return (true);
}

int
main(int argc, char * argv[])
{
    register_for_notifications();
    interface_list_changed();
    CFRunLoopRun();
    exit(0);
}

#endif /* TEST_INTERFACE_CHANGES */
