/*
 * Copyright (c) 2000-2012 Apple Inc.  All Rights Reserved.
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
 * ip_plugin.c
 * - decides which interface will be made the "primary" interface,
 *   that is, the one with the default route assigned
 */

/*
 * Modification History
 *
 * July 19, 2000	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 *
 * November 15, 2000	Dieter Siegmund (dieter@apple.com)
 * - changed to use new configuration model
 *
 * March 19, 2001	Dieter Siegmund (dieter@apple.com)
 * - use service state instead of interface state
 *
 * July 16, 2001	Allan Nathanson (ajn@apple.com)
 * - update to public SystemConfiguration.framework APIs
 *
 * August 28, 2001	Dieter Siegmund (dieter@apple.com)
 * - specify the interface name when installing the default route
 * - this ensures that default traffic goes to the highest priority
 *   service when multiple interfaces are configured to be on the same subnet
 *
 * September 16, 2002	Dieter Siegmund (dieter@apple.com)
 * - don't elect a link-local service to be primary unless it's the only
 *   one that's available
 *
 * July 16, 2003	Dieter Siegmund (dieter@apple.com)
 * - modifications to support IPv6
 * - don't elect a service to be primary if it doesn't have a default route
 *
 * July 29, 2003	Dieter Siegmund (dieter@apple.com)
 * - support installing a default route to a router that's not on our subnet
 *
 * March 22, 2004	Allan Nathanson (ajn@apple.com)
 * - create expanded DNS configuration
 *
 * June 20, 2006	Allan Nathanson (ajn@apple.com)
 * - add SMB configuration
 *
 * December 5, 2007	Dieter Siegmund (dieter@apple.com)
 * - added support for multiple scoped routes
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>
#include <limits.h>
#include <notify.h>
#include <mach/mach_time.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>	/* for SCLog() */
#include <dnsinfo.h>
#if	!TARGET_OS_IPHONE
#include <dnsinfo_create.h>
#endif	/* !TARGET_OS_IPHONE */

#include <dns_sd.h>
#ifndef	kDNSServiceCompMulticastDNS
#define	kDNSServiceCompMulticastDNS	"MulticastDNS"
#endif
#ifndef	kDNSServiceCompPrivateDNS
#define	kDNSServiceCompPrivateDNS	"PrivateDNS"
#endif
#include <network_information.h>
#include "network_information_priv.h"

enum {
    kProtocolFlagsNone		= 0x0,
    kProtocolFlagsIPv4		= 0x1,
    kProtocolFlagsIPv6		= 0x2
};
typedef uint8_t	ProtocolFlags;

enum {
    kDebugFlag1		= 0x00000001,
    kDebugFlag2		= 0x00000002,
    kDebugFlag4		= 0x00000004,
    kDebugFlag8		= 0x00000008,
    kDebugFlagDefault	= kDebugFlag1,
    kDebugFlagAll	= 0xffffffff
};

#ifdef TEST_IPV4_ROUTELIST
#define ROUTELIST_DEBUG(a, f) { if ((S_IPMonitor_debug & (f)) != 0)  printf a ;}
#else
#define ROUTELIST_DEBUG(a, f)
#endif

#include "set-hostname.h"
#include "dns-configuration.h"
#include "proxy-configuration.h"
#if	!TARGET_OS_IPHONE
#include "smb-configuration.h"
#endif	/* !TARGET_OS_IPHONE */

#define PPP_PREFIX	"ppp"

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

/*
 * IPv4 Route management
 */

typedef uint32_t 	RouteFlags;

enum {
    kRouteIsDirectToInterfaceFlag	= 0x00000001,
    kRouteIsNotSubnetLocalFlag		= 0x00000002,
    kRouteIsScopedFlag			= 0x00000004,
};

typedef struct {
    struct in_addr	dest;
    struct in_addr	mask;
    struct in_addr	gateway;
    char		ifname[IFNAMSIZ];
    unsigned int	ifindex;
    struct in_addr	ifa;
    Rank		rank;
    RouteFlags		flags;
} IPv4Route, *IPv4RouteRef;

typedef struct {
    int			count;
    int			size;
    IPv4Route		list[1];	/* variable length */
} IPv4RouteList, *IPv4RouteListRef;

enum {
    kIPv4RouteListAddRouteCommand,
    kIPv4RouteListRemoveRouteCommand
};

typedef struct {
    char		ifname[IFNAMSIZ];
    uint32_t		flags;
    Rank		rank;
    struct in6_addr	iaddr6;
} IPv6RankedE, *IPv6RankedERef;

typedef struct {
    int			count;
    int			size;
    IPv6RankedE		elem[1];
} IPv6RankedList, *IPv6RankedListRef;

/*
 * Type: Rank
 * Purpose:
 *   A 32-bit value to encode the relative rank of a service.
 *
 *   The top 8 bits are used to hold the rank assertion (first, last, never, default),
 *   the bottom 24 bits are used to store the service index i.e. the position within
 *   the service order array.
 */
#define RANK_ASSERTION_MAKE(r)		((Rank)(r) << 24)
#define kRankAssertionFirst		RANK_ASSERTION_MAKE(0)
#define kRankAssertionDefault		RANK_ASSERTION_MAKE(1)
#define kRankAssertionLast		RANK_ASSERTION_MAKE(2)
#define kRankAssertionNever		RANK_ASSERTION_MAKE(3)
#define kRankAssertionMask		RANK_ASSERTION_MAKE(0xff)
#define RANK_ASSERTION_MASK(r)		((Rank)(r) & kRankAssertionMask)

#define RANK_INDEX_MAKE(r)		((Rank)(r))
#define kRankIndexMask			RANK_INDEX_MAKE(0xffffff)
#define RANK_INDEX_MASK(r)		((Rank)(r) & kRankIndexMask)

static __inline__ Rank
RankMake(uint32_t service_index, Rank primary_rank)
{
    return (RANK_INDEX_MASK(service_index) | RANK_ASSERTION_MASK(primary_rank));
}

static __inline__ Rank
PrimaryRankGetRankAssertion(CFStringRef primaryRank)
{
    if (CFEqual(primaryRank, kSCValNetServicePrimaryRankNever)) {
	return kRankAssertionNever;
    } else if (CFEqual(primaryRank, kSCValNetServicePrimaryRankFirst)) {
	return kRankAssertionFirst;
    } else if (CFEqual(primaryRank, kSCValNetServicePrimaryRankLast)) {
	return kRankAssertionLast;
    }
    return kRankAssertionDefault;
}

typedef uint32_t	IPv4RouteListApplyCommand;

typedef void IPv4RouteListApplyCallBackFunc(IPv4RouteListApplyCommand cmd,
					    IPv4RouteRef route, void * arg);
typedef IPv4RouteListApplyCallBackFunc * IPv4RouteListApplyCallBackFuncPtr;

/* SCDynamicStore session */
static SCDynamicStoreRef	S_session = NULL;

/* debug output flags */
static uint32_t			S_IPMonitor_debug = 0;

/* are we netbooted?  If so, don't touch the default route */
static boolean_t		S_netboot = FALSE;

/* is scoped routing enabled? */
#ifdef RTF_IFSCOPE
static boolean_t		S_scopedroute = FALSE;
static boolean_t		S_scopedroute_v6 = FALSE;
#endif /* RTF_IFSCOPE */

/* dictionary to hold per-service state: key is the serviceID */
static CFMutableDictionaryRef	S_service_state_dict = NULL;
static CFMutableDictionaryRef	S_ipv4_service_rank_dict = NULL;
static CFMutableDictionaryRef	S_ipv6_service_rank_dict = NULL;

/* dictionary to hold per-interface rank information */
static CFMutableDictionaryRef	S_if_rank_dict = NULL;

/* if set, a PPP interface overrides the primary */
static boolean_t		S_ppp_override_primary = FALSE;

/* the current primary serviceID's */
static CFStringRef		S_primary_ipv4 = NULL;
static CFStringRef		S_primary_ipv6 = NULL;
static CFStringRef		S_primary_dns = NULL;
static CFStringRef		S_primary_proxies = NULL;

static CFStringRef		S_state_global_ipv4 = NULL;
static CFStringRef		S_state_global_ipv6 = NULL;
static CFStringRef		S_state_global_dns = NULL;
static CFStringRef		S_state_global_proxies = NULL;
static CFStringRef		S_state_service_prefix = NULL;
static CFStringRef		S_setup_global_ipv4 = NULL;
static CFStringRef		S_setup_service_prefix = NULL;

static CFStringRef		S_multicast_resolvers = NULL;
static CFStringRef		S_private_resolvers = NULL;

static IPv4RouteListRef		S_ipv4_routelist = NULL;

static const struct in_addr	S_ip_zeros = { 0 };
static const struct in6_addr	S_ip6_zeros = IN6ADDR_ANY_INIT;

static boolean_t		S_append_state = FALSE;

static nwi_state_t		S_nwi_state = NULL;

#if	!TARGET_OS_IPHONE
static CFStringRef		S_primary_smb = NULL;
static CFStringRef		S_state_global_smb = NULL;
#endif	/* !TARGET_OS_IPHONE */

#if	!TARGET_OS_IPHONE
#define VAR_RUN_RESOLV_CONF	"/var/run/resolv.conf"
#endif	/* !TARGET_OS_IPHONE */

#ifndef KERN_NETBOOT
#define KERN_NETBOOT		40	/* int: are we netbooted? 1=yes,0=no */
#endif //KERN_NETBOOT

/**
 ** entityType*, GetEntityChanges*
 ** - definitions for the entity types we handle
 **/
enum {
    kEntityTypeIPv4	= 0,
    kEntityTypeIPv6,
    kEntityTypeDNS,
    kEntityTypeProxies,
#if	!TARGET_OS_IPHONE
    kEntityTypeSMB,
#endif	/* !TARGET_OS_IPHONE */
    ENTITY_TYPES_COUNT,
    kEntityTypeServiceOptions	= 31
};
typedef uint32_t	EntityType;

static const CFStringRef *entityTypeNames[ENTITY_TYPES_COUNT] = {
    &kSCEntNetIPv4,	/* 0 */
    &kSCEntNetIPv6,	/* 1 */
    &kSCEntNetDNS,	/* 2 */
    &kSCEntNetProxies,	/* 3 */
#if	!TARGET_OS_IPHONE
    &kSCEntNetSMB,	/* 4 */
#endif	/* !TARGET_OS_IPHONE */
};

#ifndef kSCEntNetIPv4RouteList
#define kSCEntNetIPv4RouteList CFSTR("IPv4RouteList")
#endif

#ifndef kSCEntNetIPv4ServiceDict
#define kSCEntNetIPv4ServiceDict CFSTR("IPv4ServiceDict")
#endif

static IPv4RouteListRef
ipv4_dict_get_routelist(CFDictionaryRef ipv4_dict)
{
    CFDataRef 		routes;
    IPv4RouteListRef 	routes_list = NULL;

    if (isA_CFDictionary(ipv4_dict) == NULL) {
	return (NULL);
    }

    routes = CFDictionaryGetValue(ipv4_dict, kSCEntNetIPv4RouteList);

    if (routes != NULL) {
	routes_list = (IPv4RouteListRef)(void*)CFDataGetBytePtr(routes);
    }
    return (routes_list);
}

static CFStringRef
ipv4_dict_get_ifname(CFDictionaryRef ipv4_dict)
{
    CFDictionaryRef ipv4_service_dict = NULL;

    if (isA_CFDictionary(ipv4_dict) == NULL) {
	return (NULL);
    }

    ipv4_service_dict = CFDictionaryGetValue(ipv4_dict,
					     kSCEntNetIPv4ServiceDict);

    if (isA_CFDictionary(ipv4_service_dict) == NULL) {
	return NULL;
    }

    return CFDictionaryGetValue(ipv4_service_dict, kSCPropInterfaceName);
}

typedef boolean_t GetEntityChangesFunc(CFStringRef serviceID,
				       CFDictionaryRef state_dict,
				       CFDictionaryRef setup_dict,
				       CFDictionaryRef info);
typedef GetEntityChangesFunc * GetEntityChangesFuncRef;

static GetEntityChangesFunc get_ipv4_changes;
static GetEntityChangesFunc get_ipv6_changes;
static GetEntityChangesFunc get_dns_changes;
static GetEntityChangesFunc get_proxies_changes;
#if	!TARGET_OS_IPHONE
static GetEntityChangesFunc get_smb_changes;
#endif	/* !TARGET_OS_IPHONE */

static void
my_CFRelease(void * t);

static void
my_CFArrayAppendUniqueValue(CFMutableArrayRef arr, CFTypeRef new);

static void
my_CFArrayRemoveValue(CFMutableArrayRef arr, CFStringRef key);

static const GetEntityChangesFuncRef entityChangeFunc[ENTITY_TYPES_COUNT] = {
    get_ipv4_changes,	/* 0 */
    get_ipv6_changes,	/* 1 */
    get_dns_changes,	/* 2 */
    get_proxies_changes,/* 3 */
#if	!TARGET_OS_IPHONE
    get_smb_changes,	/* 4 */
#endif	/* !TARGET_OS_IPHONE */
};

/**
 ** keyChangeList
 ** - mechanism to do an atomic update of the SCDynamicStore
 **   when the content needs to be changed across multiple functions
 **/
typedef struct {
    CFMutableArrayRef		notify;
    CFMutableArrayRef		remove;
    CFMutableDictionaryRef	set;
} keyChangeList, * keyChangeListRef;

static CFStringRef
my_CFStringCopyComponent(CFStringRef path, CFStringRef separator,
			 CFIndex component_index)
{
    CFArrayRef			arr;
    CFStringRef			component = NULL;

    arr = CFStringCreateArrayBySeparatingStrings(NULL, path, separator);
    if (arr == NULL) {
	goto done;
    }
    if (CFArrayGetCount(arr) <= component_index) {
	goto done;
    }
    component = CFRetain(CFArrayGetValueAtIndex(arr, component_index));

 done:
    my_CFRelease(&arr);
    return (component);
}

static void
keyChangeListInit(keyChangeListRef keys)
{
    keys->notify = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    keys->remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    keys->set = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    return;
}

static void
keyChangeListFree(keyChangeListRef keys)
{
    my_CFRelease(&keys->notify);
    my_CFRelease(&keys->remove);
    my_CFRelease(&keys->set);
    return;
}

static void
keyChangeListNotifyKey(keyChangeListRef keys, CFStringRef key)
{
    my_CFArrayAppendUniqueValue(keys->notify, key);
    return;
}

static void
keyChangeListRemoveValue(keyChangeListRef keys, CFStringRef key)
{
    my_CFArrayAppendUniqueValue(keys->remove, key);
    CFDictionaryRemoveValue(keys->set, key);
    return;
}

static void
keyChangeListSetValue(keyChangeListRef keys, CFStringRef key, CFTypeRef value)
{
    my_CFArrayRemoveValue(keys->remove, key);
    CFDictionarySetValue(keys->set, key, value);
    return;
}

static void
keyChangeListApplyToStore(keyChangeListRef keys, SCDynamicStoreRef session,
			  CFStringRef network_change_msg)
{
    CFArrayRef		notify = keys->notify;
    CFArrayRef		remove = keys->remove;
    CFDictionaryRef	set = keys->set;
    int			status;

    if (CFArrayGetCount(notify) == 0) {
	notify = NULL;
    }
    if (CFArrayGetCount(remove) == 0) {
	remove = NULL;
    }
    if (CFDictionaryGetCount(set) == 0) {
	set = NULL;
    }
    if (set == NULL && remove == NULL && notify == NULL) {
	return;
    }
    if (S_IPMonitor_debug & kDebugFlag1) {
	if (set != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("IPMonitor: Setting:\n%@"),
		  set);
	}
	if (remove != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("IPMonitor: Removing:\n%@"),
		  remove);
	}
	if (notify != NULL) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("IPMonitor: Notifying:\n%@"),
		  notify);
	}
    }
    (void)SCDynamicStoreSetMultiple(session, set, remove, notify);

    status = notify_post("com.apple.system.config.network_change");
    if (status == NOTIFY_STATUS_OK) {
	if (CFStringGetLength(network_change_msg) != 0) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("network changed:%@"), network_change_msg);
	} else {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("network changed."));
	}
    } else {
	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("IPMonitor: notify_post() failed: error=%ld"), status);
    }

    return;
}

static boolean_t
S_is_network_boot()
{
    int mib[2];
    size_t len;
    int netboot = 0;

    mib[0] = CTL_KERN;
    mib[1] = KERN_NETBOOT;
    len = sizeof(netboot);
    sysctl(mib, 2, &netboot, &len, NULL, 0);
    return (netboot);
}

static __inline__ int
inet6_dgram_socket()
{
    return (socket(AF_INET6, SOCK_DGRAM, 0));
}

#ifdef SIOCDRADD_IN6
static int
siocdradd_in6(int s, int if_index, const struct in6_addr * addr, u_char flags)
{
    struct in6_defrouter	dr;
    struct sockaddr_in6 *	sin6;

    bzero(&dr, sizeof(dr));
    sin6 = &dr.rtaddr;
    sin6->sin6_len = sizeof(struct sockaddr_in6);
    sin6->sin6_family = AF_INET6;
    sin6->sin6_addr = *addr;
    dr.flags = flags;
    dr.if_index = if_index;
    return (ioctl(s, SIOCDRADD_IN6, &dr));
}

static int
siocdrdel_in6(int s, int if_index, const struct in6_addr * addr)
{
    struct in6_defrouter	dr;
    struct sockaddr_in6 *	sin6;

    bzero(&dr, sizeof(dr));
    sin6 = &dr.rtaddr;
    sin6->sin6_len = sizeof(struct sockaddr_in6);
    sin6->sin6_family = AF_INET6;
    sin6->sin6_addr = *addr;
    dr.if_index = if_index;
    return (ioctl(s, SIOCDRDEL_IN6, &dr));
}
#endif /* SIOCDRADD_IN6 */

#ifdef	RTF_IFSCOPE
static boolean_t
S_is_scoped_routing_enabled()
{
    int	    scopedroute	= 0;
    size_t  len		= sizeof(scopedroute);

    if ((sysctlbyname("net.inet.ip.scopedroute",
		      &scopedroute, &len,
		      NULL, 0) == -1)
	&& (errno != ENOENT)) {
	SCLog(TRUE, LOG_ERR, CFSTR("sysctlbyname() failed: %s"), strerror(errno));
    }
    return (scopedroute);
}

static boolean_t
S_is_scoped_v6_routing_enabled()
{
    int	    scopedroute_v6	= 0;
    size_t  len			= sizeof(scopedroute_v6);

    if ((sysctlbyname("net.inet6.ip6.scopedroute",
		      &scopedroute_v6, &len,
		      NULL, 0) == -1)
	&& (errno != ENOENT)) {
	SCLog(TRUE, LOG_ERR, CFSTR("sysctlbyname() failed: %s"), strerror(errno));
    }
    return (scopedroute_v6);
}
#endif /* RTF_IFSCOPE */

static void
my_CFArrayAppendUniqueValue(CFMutableArrayRef arr, CFTypeRef new)
{
    CFIndex	n = CFArrayGetCount(arr);

    if (CFArrayContainsValue(arr, CFRangeMake(0, n), new)) {
	return;
    }
    CFArrayAppendValue(arr, new);
    return;
}

static void
my_CFArrayRemoveValue(CFMutableArrayRef arr, CFStringRef key)
{
    CFIndex	i;

    i = CFArrayGetFirstIndexOfValue(arr,
				    CFRangeMake(0, CFArrayGetCount(arr)),
				    key);
    if (i != kCFNotFound) {
	CFArrayRemoveValueAtIndex(arr, i);
    }
    return;
}

static void
my_CFRelease(void * t)
{
    void * * obj = (void * *)t;

    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

static CFDictionaryRef
my_CFDictionaryGetDictionary(CFDictionaryRef dict, CFStringRef key)
{
    if (isA_CFDictionary(dict) == NULL) {
	return (NULL);
    }
    return (isA_CFDictionary(CFDictionaryGetValue(dict, key)));
}

static CFArrayRef
my_CFDictionaryGetArray(CFDictionaryRef dict, CFStringRef key)
{
    if (isA_CFDictionary(dict) == NULL) {
	return (NULL);
    }
    return (isA_CFArray(CFDictionaryGetValue(dict, key)));
}

static boolean_t
cfstring_to_ipvx(int family, CFStringRef str, void * addr, int addr_size)
{
    char        buf[128];

    if (isA_CFString(str) == NULL) {
	goto done;
    }

    switch (family) {
    case AF_INET:
	if (addr_size < sizeof(struct in_addr)) {
	    goto done;
	}
	break;
    case AF_INET6:
	if (addr_size < sizeof(struct in6_addr)) {
	    goto done;
	}
	break;
    default:
	goto done;
    }
    (void)_SC_cfstring_to_cstring(str, buf, sizeof(buf), kCFStringEncodingASCII);
    if (inet_pton(family, buf, addr) == 1) {
	return (TRUE);
    }
 done:
    bzero(addr, addr_size);
    return (FALSE);
}

static boolean_t
cfstring_to_ip(CFStringRef str, struct in_addr * ip_p)
{
    return (cfstring_to_ipvx(AF_INET, str, ip_p, sizeof(*ip_p)));
}

static boolean_t
cfstring_to_ip6(CFStringRef str, struct in6_addr * ip6_p)
{
    return (cfstring_to_ipvx(AF_INET6, str, ip6_p, sizeof(*ip6_p)));
}

static CF_RETURNS_RETAINED CFStringRef
setup_service_key(CFStringRef serviceID, CFStringRef entity)
{
    return (SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							serviceID,
							entity));
}

static CF_RETURNS_RETAINED CFStringRef
state_service_key(CFStringRef serviceID, CFStringRef entity)
{
    return (SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							kSCDynamicStoreDomainState,
							serviceID,
							entity));
}

static CFDictionaryRef
get_service_setup_entity(CFDictionaryRef services_info, CFStringRef serviceID,
			 CFStringRef entity)
{
    CFStringRef		setup_key;
    CFDictionaryRef	setup_dict;

    setup_key = setup_service_key(serviceID, entity);
    setup_dict = my_CFDictionaryGetDictionary(services_info, setup_key);
    my_CFRelease(&setup_key);
    return (setup_dict);
}

static CFDictionaryRef
get_service_state_entity(CFDictionaryRef services_info, CFStringRef serviceID,
			 CFStringRef entity)
{
    CFStringRef		state_key;
    CFDictionaryRef	state_dict;

    state_key = state_service_key(serviceID, entity);
    state_dict = my_CFDictionaryGetDictionary(services_info, state_key);
    my_CFRelease(&state_key);
    return (state_dict);
}

static boolean_t
dict_get_first_ip(CFDictionaryRef dict, CFStringRef prop, struct in_addr * ip_p)
{
    CFArrayRef		ip_list;

    ip_list = CFDictionaryGetValue(dict, prop);
    if (isA_CFArray(ip_list) != NULL
	&& CFArrayGetCount(ip_list) > 0
	&& cfstring_to_ip(CFArrayGetValueAtIndex(ip_list, 0), ip_p)) {
	return (TRUE);
    }
    return (FALSE);
}

static boolean_t
get_override_primary(CFDictionaryRef dict)
{
    CFTypeRef	override;

    override = CFDictionaryGetValue(dict, kSCPropNetOverridePrimary);
    if (isA_CFNumber(override) != NULL) {
	int	val = 0;

	CFNumberGetValue((CFNumberRef)override, kCFNumberIntType, &val);
	if (val != 0) {
	    return (TRUE);
	}
    }
    else if (isA_CFBoolean(override) != NULL) {
	if (CFBooleanGetValue(override)) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

/**
 ** IPv4Route*
 **/

static __inline__ struct in_addr
subnet_addr(struct in_addr addr, struct in_addr mask)
{
    struct in_addr	net;

    net.s_addr = addr.s_addr & mask.s_addr;
    return (net);
}

static __inline__ int
in_addr_cmp(struct in_addr a, struct in_addr b)
{
    return (uint32_cmp(ntohl(a.s_addr), ntohl(b.s_addr)));
}

static __inline__ int
RouteFlagsCompare(RouteFlags a, RouteFlags b)
{
    return (uint32_cmp(a, b));
}

static void
IPv4RouteCopyDescriptionWithString(IPv4RouteRef r, CFMutableStringRef str)
{
    Rank rank_assertion = RANK_ASSERTION_MASK(r->rank);

    CFStringAppendFormat(str, NULL,
			 CFSTR("Dest " IP_FORMAT
			       " Mask " IP_FORMAT
			       " Gate " IP_FORMAT
			       " Ifp %s Ifa " IP_FORMAT),
			 IP_LIST(&r->dest),
			 IP_LIST(&r->mask),
			 IP_LIST(&r->gateway),
			 (r->ifname[0] != '\0') ? r->ifname : "<none>",
			 IP_LIST(&r->ifa));
    if ((r->flags & kRouteIsNotSubnetLocalFlag) != 0) {
	CFStringAppend(str, CFSTR(" [non-local]"));
    }
    else if ((r->flags & kRouteIsDirectToInterfaceFlag) != 0) {
	CFStringAppend(str, CFSTR(" [direct]"));
    }

    switch(rank_assertion) {
	case kRankAssertionFirst:
	    CFStringAppend(str, CFSTR(" [first]"));
	    break;
	case kRankAssertionLast:
	    CFStringAppend(str, CFSTR(" [last]"));
	    break;
	case kRankAssertionNever:
	    CFStringAppend(str, CFSTR(" [never]"));
	    break;
	default:
	    break;
    }

    if ((r->flags & kRouteIsScopedFlag) != 0) {
	CFStringAppend(str, CFSTR(" [SCOPED]"));
    }
    return;
}

static CFStringRef
IPv4RouteCopyDescription(IPv4RouteRef r)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    IPv4RouteCopyDescriptionWithString(r, str);
    return (str);
}

static __inline__ void
IPv4RoutePrint(IPv4RouteRef route)
{
    CFStringRef	str = IPv4RouteCopyDescription(route);

    SCPrint(TRUE, stdout, CFSTR("%@\n"), str);
    CFRelease(str);
    return;
}

static __inline__ void
IPv4RouteLog(IPv4RouteRef route)
{
    CFStringRef	str = IPv4RouteCopyDescription(route);

    SCLog(TRUE, LOG_NOTICE, CFSTR("%@"), str);
    CFRelease(str);
    return;
}

static int
IPv4RouteCompare(IPv4RouteRef a, Rank a_rank,
		 IPv4RouteRef b, Rank b_rank, boolean_t * same_dest)
{
    int		cmp;

    *same_dest = FALSE;
    cmp = in_addr_cmp(a->dest, b->dest);
    if (cmp == 0) {
	cmp = in_addr_cmp(a->mask, b->mask);
	if (cmp == 0) {
	    int		name_cmp = strcmp(a->ifname, b->ifname);

	    if (name_cmp == 0) {
		cmp = 0;
	    }
	    else {
		*same_dest = TRUE;
		cmp = RankCompare(a_rank, b_rank);
		if (cmp == 0) {
		    cmp = name_cmp;
		}
	    }
	}
    }
    if ((S_IPMonitor_debug & kDebugFlag8) != 0) {
	CFStringRef	a_str;
	CFStringRef	b_str;
	char		ch;

	if (cmp < 0) {
	    ch = '<';
	}
	else if (cmp == 0) {
	    ch = '=';
	}
	else {
	    ch = '>';
	}
	a_str = IPv4RouteCopyDescription(a);
	b_str = IPv4RouteCopyDescription(b);
	SCLog(TRUE, LOG_NOTICE, CFSTR("%@ rank %u %c %@ rank %u"),
	      a_str, a_rank, ch, b_str, b_rank);
	CFRelease(a_str);
	CFRelease(b_str);
    }
    return (cmp);
}

static CFMutableStringRef
IPv4RouteListCopyDescription(IPv4RouteListRef routes)
{
    int			i;
    IPv4RouteRef	r;
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(str, NULL, CFSTR("<IPv4RouteList[%d]> = {"),
			 routes->count);
    for (i = 0, r = routes->list; i < routes->count; i++, r++) {
	CFStringAppendFormat(str, NULL, CFSTR("\n%2d. "), i);
	IPv4RouteCopyDescriptionWithString(r, str);
    }
    CFStringAppend(str, CFSTR("\n}"));
    return (str);
}

static __inline__ void
IPv4RouteListPrint(IPv4RouteListRef routes)
{
    CFStringRef	str = IPv4RouteListCopyDescription(routes);

    SCPrint(TRUE, stdout, CFSTR("%@\n"), str);
    CFRelease(str);
    return;
}

static __inline__ void
IPv4RouteListLog(IPv4RouteListRef routes)
{
    CFStringRef	str = IPv4RouteListCopyDescription(routes);

    SCLog(TRUE, LOG_NOTICE, CFSTR("%@"), str);
    CFRelease(str);
    return;
}

static __inline__ unsigned int
IPv4RouteListComputeSize(unsigned int n)
{
    return (offsetof(IPv4RouteList, list[n]));
}

static IPv4RouteRef
IPv4RouteListFindRoute(IPv4RouteListRef routes, IPv4RouteRef route)
{
    int			i;
    IPv4RouteRef	scan_result = NULL;
    IPv4RouteRef	scan;

    for (i = 0, scan = routes->list; i < routes->count; i++, scan++) {
	if ((scan->dest.s_addr == route->dest.s_addr)
	    && (scan->mask.s_addr == route->mask.s_addr)
	    && (strcmp(scan->ifname, route->ifname) == 0)
	    && (scan->ifa.s_addr == route->ifa.s_addr)
	    && (scan->gateway.s_addr == route->gateway.s_addr)
	    && (scan->flags == route->flags)) {
		scan_result = scan;
		break;
	}
    }
    return (scan_result);
}

static void
IPv4RouteListApply(IPv4RouteListRef old_routes, IPv4RouteListRef new_routes,
		   IPv4RouteListApplyCallBackFuncPtr func, void * arg)
{
    int			i;
    IPv4RouteRef	scan;

    if (old_routes == new_routes && old_routes == NULL) {
	/* both old and new are NULL, so there's nothing to do */
	return;
    }
    if (old_routes != NULL) {
	for (i = 0, scan = old_routes->list;
	     i < old_routes->count;
	     i++, scan++) {
	    IPv4RouteRef	new_route = NULL;

	    if (new_routes != NULL) {
		new_route = IPv4RouteListFindRoute(new_routes, scan);
	    }
	    if (new_route == NULL) {
		if (func != NULL) {
		    (*func)(kIPv4RouteListRemoveRouteCommand, scan, arg);
		}
	    }
	}
    }
    if (new_routes != NULL) {
	for (i = 0, scan = new_routes->list;
	     i < new_routes->count;
	     i++, scan++) {
	    IPv4RouteRef	old_route = NULL;

	    if (old_routes != NULL) {
		old_route = IPv4RouteListFindRoute(old_routes, scan);
	    }
	    if (old_route == NULL) {
		if (func != NULL) {
		    (*func)(kIPv4RouteListAddRouteCommand, scan, arg);
		}
	    }
	}
    }
    return;
}

/*
 * Function: IPv4RouteListAddRoute
 *
 * Purpose:
 *   Add the given IPv4Route to the list of routes, eliminating lower-ranked
 *   duplicates on the same interface, and marking any lower ranked duplicates
 *   on other interfaces with kRouteIsScopedFlag.
 *
 *   This routine assumes that if routes is not NULL, it is malloc'd memory.
 *
 * Returns:
 *   Route list updated with the given route, possibly a different pointer,
 *   due to using realloc'd memory.
 */

enum {
    kScopeNone	= 0,
    kScopeThis	= 1,
    kScopeNext	= 2
};

static IPv4RouteListRef
IPv4RouteListAddRoute(IPv4RouteListRef routes, int init_size,
		      IPv4RouteRef this_route, Rank this_rank)
{
    int			i;
    IPv4RouteRef	first_scan = NULL;
    int			scope_which = kScopeNone;
    IPv4RouteRef	scan;
    int			where = -1;

    if (routes == NULL) {
	routes = (IPv4RouteListRef)malloc(IPv4RouteListComputeSize(init_size));
	routes->size = init_size;
	routes->count = 0;
    }
    for (i = 0, scan = routes->list; i < routes->count;
	 i++, scan++) {
	int		cmp;
	boolean_t	same_dest;

	cmp = IPv4RouteCompare(this_route, this_rank, scan, scan->rank, &same_dest);

	if (same_dest == TRUE && first_scan == NULL) {
	    first_scan = scan;
	}

	if (cmp < 0) {
	    if (where == -1) {
		if (same_dest == TRUE
		    && (first_scan->flags & kRouteIsScopedFlag) == 0) {
		    if ((scan->flags & kRouteIsScopedFlag) != 0) {
			ROUTELIST_DEBUG(("Hit 1: set scope on self\n"),
					kDebugFlag8);
			scope_which = kScopeThis;
		    }
		    else {
			ROUTELIST_DEBUG(("Hit 2: set scope on next\n"),
					kDebugFlag8);
			scope_which = kScopeNext;
		    }
		}
		/* remember our insertion point, but keep going to find a dup */
		where = i;
	    }
	}
	else if (cmp == 0) {
	    /* exact match */
	    if (where != -1) {
		/* this route is a duplicate */
		ROUTELIST_DEBUG(("Hit 3: removing [%d]\n", i), kDebugFlag8);
		routes->count--;
		if (i == routes->count) {
		    /* last slot, decrementing gets rid of it */
		}
		else {
		    bcopy(routes->list + i + 1,
			  routes->list + i,
			  sizeof(routes->list[0]) * (routes->count - i));
		}
		break;
	    }
	    /* resolve conflict using rank */
	    if (this_rank < scan->rank) {
		boolean_t	is_scoped = FALSE;

		if (scan->flags & kRouteIsScopedFlag) {
		    is_scoped = TRUE;
		}
		ROUTELIST_DEBUG(("Hit 4:replacing [%d] rank %u < %u\n", i,
				 this_rank,
				 scan->rank), kDebugFlag8);
		*scan = *this_route;
		scan->rank = this_rank;
		if (is_scoped) {
		    /* preserve whether route was scoped */
		    ROUTELIST_DEBUG(("Hit 5: preserved scope\n"), kDebugFlag8);
		    scan->flags |= kRouteIsScopedFlag;
		}
	    }
	    /* we're done */
	    goto done;
	}
	else {
	    if (same_dest == TRUE) {
		if (scope_which == kScopeNone) {
		    ROUTELIST_DEBUG(("Hit 10: set scope on self\n"),
				    kDebugFlag8);
		    scope_which = kScopeThis;
		}
	    }
#ifdef TEST_IPV4_ROUTELIST
	    else if (where != -1) {
		/* not possible because we maintain a sorted list */
		ROUTELIST_DEBUG(("Hit 11: moved past routes - can't happen\n"),
				kDebugFlag8);
		break;
	    }
#endif /* TEST_IPV4_ROUTELIST */
	}
    }
    if (routes->size == routes->count) {
	int			how_many;
	IPv4RouteListRef	new_routes;
	int			old_size;

	/* double the size */
	old_size = routes->size;
	how_many = old_size * 2;
	new_routes = (IPv4RouteListRef)
	    realloc(routes, IPv4RouteListComputeSize(how_many));
	if (new_routes == NULL) {
	    /* no memory */
	    goto done;
	}
	ROUTELIST_DEBUG(("increasing size from %d to %d\n", old_size,
			 how_many), kDebugFlag8);
	new_routes->size = how_many;
	routes = new_routes;
    }
    if (where == -1) {
	/* add it to the end */
	where = routes->count;
    }
    else {
	/* insert it at [where] */
	bcopy(routes->list + where,
	      routes->list + where + 1,
	      sizeof(routes->list[0]) * (routes->count - where));
    }
    /* copy the route */
    routes->list[where] = *this_route;
    routes->list[where].rank = this_rank;

    /* set the scope */
    switch (scope_which) {
    case kScopeThis:
	routes->list[where].flags |= kRouteIsScopedFlag;
	break;
    case kScopeNext:
	routes->list[where + 1].flags |= kRouteIsScopedFlag;
	break;
    default:
    case kScopeNone:
	break;
    }
    routes->count++;
 done:
    return (routes);
}

/*
 * Function: IPv4RouteListAddRouteList
 *
 * Purpose:
 *   Invoke IPv4RouteListAddRoute for each route in the given list.
 *
 * Returns:
 *   See IPv4RouteListAddRoute for more information.
 */
static IPv4RouteListRef
IPv4RouteListAddRouteList(IPv4RouteListRef routes, int init_size,
			  IPv4RouteListRef service_routes, Rank rank)
{
    int			i;
    IPv4RouteRef	scan;

    for (i = 0, scan = service_routes->list;
	 i < service_routes->count; i++, scan++) {
	routes = IPv4RouteListAddRoute(routes, init_size, scan, rank);
    }
    return (routes);
}

static boolean_t
plist_get_cstring(CFDictionaryRef dict, CFStringRef prop_name,
		  char * buf, int buf_size)
{
    CFStringRef	val;

    val = CFDictionaryGetValue(dict, prop_name);
    if (isA_CFString(val) == NULL) {
	return (FALSE);
    }
    if (CFStringGetCString(val, buf, buf_size, kCFStringEncodingASCII)
	== FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: IPv4RouteListCreateWithDictionary
 *
 * Purpose:
 *   Given the service ipv4 entity dictionary, generate the list of routes.
 *   Currently, this includes just the default route and subnet route,
 *   if the service has a subnet mask.
 *
 * Returns:
 *   If the passed in route_list is NULL or too small, this routine
 *   allocates malloc'd memory to hold the routes.
 */
static IPv4RouteListRef
IPv4RouteListCreateWithDictionary(IPv4RouteListRef routes,
				  CFDictionaryRef dict,
				  CFStringRef primaryRank)
{
    struct in_addr	addr = { 0 };
    RouteFlags		flags = 0;
    unsigned int	ifindex;
    char		ifn[IFNAMSIZ];
    struct in_addr	mask = { 0 };
    int			n = 0;
    boolean_t		add_default = FALSE;
    boolean_t		add_subnet = FALSE;
    IPv4RouteRef	r;
    struct in_addr	subnet = { 0 };
    struct in_addr	router = { 0 };
    Rank		rank = kRankAssertionDefault;

    if (dict == NULL) {
	return (NULL);
    }
    if (plist_get_cstring(dict, kSCPropInterfaceName, ifn, sizeof(ifn))
	== FALSE) {
	return (NULL);
    }
#ifdef TEST_IPV4_ROUTELIST
    ifindex = 0;
#else /* TEST_IPV4_ROUTELIST */
    ifindex = if_nametoindex(ifn);
    if (ifindex == 0) {
	/* interface doesn't exist */
	return (NULL);
    }
#endif /* TEST_IPV4_ROUTELIST */
    if (cfstring_to_ip(CFDictionaryGetValue(dict, kSCPropNetIPv4Router),
		       &router) == 0) {
	(void)dict_get_first_ip(dict, kSCPropNetIPv4DestAddresses, &router);
    }
    if (dict_get_first_ip(dict, kSCPropNetIPv4Addresses, &addr)
	&& dict_get_first_ip(dict, kSCPropNetIPv4SubnetMasks, &mask)) {
	/* subnet route */
	subnet = subnet_addr(addr, mask);
	/* ignore link-local subnets, let IPConfiguration handle them for now */
	if (ntohl(subnet.s_addr) != IN_LINKLOCALNETNUM) {
	    add_subnet = TRUE;
	    n++;
	}
    }
    if (addr.s_addr == 0) {
	/* thanks for playing */
	return (NULL);
    }
    if (router.s_addr == 0) {
	flags |= kRouteIsDirectToInterfaceFlag;
	rank = kRankAssertionLast;
    }
    else {
	/*
	 * If the router address is our address and the subnet mask is
	 * not 255.255.255.255, assume all routes are local to the interface.
	 */
	if (addr.s_addr == router.s_addr
	    && mask.s_addr != INADDR_BROADCAST) {
	    flags |= kRouteIsDirectToInterfaceFlag;
	}
	if (primaryRank != NULL) {
	    rank = PrimaryRankGetRankAssertion(primaryRank);
	} else if (get_override_primary(dict)) {
	    rank = kRankAssertionFirst;
	}
    }

    if (rank == kRankAssertionNever) {
	flags |= kRouteIsScopedFlag;
    }

    if (add_subnet && (flags & kRouteIsDirectToInterfaceFlag) == 0
	&& subnet.s_addr != subnet_addr(router, mask).s_addr) {
	flags |= kRouteIsNotSubnetLocalFlag;
    }

    if (strncmp(ifn, "lo0", sizeof(ifn)) != 0) {
	add_default = TRUE;
	n++;
    }

    if (routes == NULL || routes->size < n) {
	routes = (IPv4RouteListRef)malloc(IPv4RouteListComputeSize(n));
	routes->size = n;
    }
    bzero(routes, IPv4RouteListComputeSize(n));
    routes->count = n;

    /* start at the beginning */
    r = routes->list;

    if (add_default) {
	/* add the default route */
	r->ifindex = ifindex;
	strlcpy(r->ifname, ifn, sizeof(r->ifname));
	r->ifa = addr;
	r->flags = flags;
	if ((flags & kRouteIsDirectToInterfaceFlag) == 0) {
	    r->gateway = router;
	}
	else {
	    r->gateway = addr;
	}
	r->rank = rank;
	if (r->rank == kRankAssertionNever) {
	    r->flags |= kRouteIsScopedFlag;
	}
	r++;
    }

    /* add the subnet route */
    if (add_subnet) {
	r->ifindex = ifindex;
	r->gateway = addr;
	r->dest = subnet;
	r->mask = mask;
	strlcpy(r->ifname, ifn, sizeof(r->ifname));
	r->ifa = addr;
	r->rank = rank;
	if (r->rank == kRankAssertionNever) {
	    r->flags |= kRouteIsScopedFlag;
	}
    }

    return (routes);
}

/*
 * Function: parse_component
 * Purpose:
 *   Given a string 'key' and a string prefix 'prefix',
 *   return the next component in the slash '/' separated
 *   key.
 *
 * Examples:
 * 1. key = "a/b/c" prefix = "a/"
 *    returns "b"
 * 2. key = "a/b/c" prefix = "a/b/"
 *    returns "c"
 */
static CF_RETURNS_RETAINED CFStringRef
parse_component(CFStringRef key, CFStringRef prefix)
{
    CFMutableStringRef	comp;
    CFRange		range;

    if (CFStringHasPrefix(key, prefix) == FALSE) {
	return (NULL);
    }
    comp = CFStringCreateMutableCopy(NULL, 0, key);
    if (comp == NULL) {
	return (NULL);
    }
    CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));
    range = CFStringFind(comp, CFSTR("/"), 0);
    if (range.location == kCFNotFound) {
	return (comp);
    }
    range.length = CFStringGetLength(comp) - range.location;
    CFStringDelete(comp, range);
    return (comp);
}

static CFMutableDictionaryRef
service_dict_copy(CFStringRef serviceID)
{
    CFDictionaryRef		d = NULL;
    CFMutableDictionaryRef	service_dict;

    /* create a modifyable dictionary, a copy or a new one */
    d = CFDictionaryGetValue(S_service_state_dict, serviceID);
    if (d == NULL) {
	service_dict
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    }
    else {
	service_dict = CFDictionaryCreateMutableCopy(NULL, 0, d);
    }
    return (service_dict);
}

static void
dump_service_entity(CFStringRef serviceID, CFStringRef entity,
		    CFStringRef operation, CFTypeRef val)
{

    CFDataRef	route_list;
    CFMutableStringRef this_val = NULL;

    if (CFEqual(entity, kSCEntNetIPv4) && isA_CFDictionary(val) != NULL) {
	CFDictionaryRef    service_dict = NULL;

	route_list = CFDictionaryGetValue(val, kSCEntNetIPv4RouteList);
	if (route_list != NULL) {
	    /* ALIGN: CF should align to at least 8-byte boundaries */
	    this_val = IPv4RouteListCopyDescription((IPv4RouteListRef)
						    (void *)CFDataGetBytePtr(route_list));
	}

	service_dict = CFDictionaryGetValue(val, kSCEntNetIPv4ServiceDict);

	if (service_dict != NULL && isA_CFDictionary(service_dict) != NULL) {
	    if (this_val == NULL) {
		this_val = CFStringCreateMutable(NULL, 0);
	    }
	    CFStringAppendFormat(this_val, NULL, CFSTR("\n <IPv4Dictionary>: %@"), service_dict);
	}
	val = this_val;
    }
    if (val == NULL) {
	val = CFSTR("<none>");
    }
    SCLog(TRUE, LOG_NOTICE, CFSTR("IPMonitor: serviceID %@ %@ %@ value = %@"),
	  serviceID, operation, entity, val);
    my_CFRelease(&this_val);
    return;
}

static boolean_t
service_dict_set(CFStringRef serviceID, CFStringRef entity,
		 CFTypeRef new_val)
{
    boolean_t			changed = FALSE;
    CFTypeRef			old_val;
    CFMutableDictionaryRef	service_dict;

    service_dict = service_dict_copy(serviceID);
    old_val = CFDictionaryGetValue(service_dict, entity);
    if (new_val == NULL) {
	if (old_val != NULL) {
	    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		dump_service_entity(serviceID, entity, CFSTR("Removed:"),
				   old_val);
	    }
	    CFDictionaryRemoveValue(service_dict, entity);
	    changed = TRUE;
	}
    }
    else {
	if (old_val == NULL || CFEqual(new_val, old_val) == FALSE) {
	    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		dump_service_entity(serviceID, entity,
				    CFSTR("Changed: old"), old_val);
		dump_service_entity(serviceID, entity,
				    CFSTR("Changed: new"), new_val);
	    }
	    CFDictionarySetValue(service_dict, entity, new_val);
	    changed = TRUE;
	}
    }
    if (CFDictionaryGetCount(service_dict) == 0) {
	CFDictionaryRemoveValue(S_service_state_dict, serviceID);
    }
    else {
	CFDictionarySetValue(S_service_state_dict, serviceID, service_dict);
    }
    my_CFRelease(&service_dict);
    return (changed);
}

static CFDictionaryRef
service_dict_get(CFStringRef serviceID, CFStringRef entity)
{
    CFDictionaryRef	service_dict;

    service_dict = CFDictionaryGetValue(S_service_state_dict, serviceID);
    if (service_dict == NULL) {
	return (NULL);
    }
    return (CFDictionaryGetValue(service_dict, entity));
}

#ifndef kSCPropNetHostname
#define kSCPropNetHostname CFSTR("Hostname")
#endif

__private_extern__
CFStringRef
copy_dhcp_hostname(CFStringRef serviceID)
{
    CFDictionaryRef 	dict = NULL;
    CFStringRef		hostname = NULL;
    CFDictionaryRef 	service_dict = NULL;

    dict = service_dict_get(serviceID, kSCEntNetIPv4);

    if (dict == NULL || isA_CFDictionary(dict) == NULL) {
	return (NULL);
    }

    service_dict =
	CFDictionaryGetValue(dict, kSCEntNetIPv4ServiceDict);

    if (service_dict == NULL
	|| isA_CFDictionary(service_dict) == NULL) {
	return (NULL);
    }

    hostname =
	CFDictionaryGetValue(service_dict, kSCPropNetHostname);

    if (hostname != NULL) {
	CFRetain(hostname);
    }

    return (hostname);
}

static boolean_t
ipv6_service_dict_set(CFStringRef serviceID, CFDictionaryRef new_val)
{
#ifdef SIOCDRADD_IN6
    int			if_index;
    char		ifn[IFNAMSIZ];
    CFStringRef		new_router = NULL;
    char		ntopbuf[INET6_ADDRSTRLEN];
    CFDictionaryRef	old_val = NULL;
    CFStringRef		old_router = NULL;
    struct in6_addr	router_ip;
    int			s = -1;

    ifn[0] = '\0';
    old_val = service_dict_get(serviceID, kSCEntNetIPv6);
    if (old_val != NULL) {
	plist_get_cstring(old_val, kSCPropInterfaceName, ifn, sizeof(ifn));
	old_router = CFDictionaryGetValue(old_val, kSCPropNetIPv6Router);
    }
    if (ifn[0] == '\0') {
	if (new_val == NULL
	    || plist_get_cstring(new_val, kSCPropInterfaceName,
				 ifn, sizeof(ifn)) == FALSE) {
	    /* no InterfaceName property, ignore it */
	    goto done;
	}
    }
    if_index = if_nametoindex(ifn);
    if (if_index == 0) {
	goto done;
    }
    s = inet6_dgram_socket();
    if (s < 0) {
	syslog(LOG_ERR,
	       "IPMonitor: ipv6_service_dict_set: socket failed, %s",
	       strerror(errno));
	goto done;
    }
    if (new_val != NULL) {
	new_router = CFDictionaryGetValue(new_val, kSCPropNetIPv6Router);
    }
    if (old_router != NULL
	&& (new_router == NULL || CFEqual(old_router, new_router) == FALSE)) {
	/* remove the old Router */
	if (cfstring_to_ip6(old_router, &router_ip)) {
	    if (IN6_IS_ADDR_LINKLOCAL(&router_ip) ||
		IN6_IS_ADDR_MC_LINKLOCAL(&router_ip)) {
		/* scope it */
		router_ip.__u6_addr.__u6_addr16[1] = htons(if_index);
	    }
	    if (siocdrdel_in6(s, if_index, &router_ip) < 0) {
		if (errno != EINVAL) {
		    syslog(LOG_ERR,
			   "IPMonitor: siocdrdel_in6(%s, %s) failed, %s",
			   ifn,
			   inet_ntop(AF_INET6, &router_ip,
				     ntopbuf, sizeof(ntopbuf)),
			   strerror(errno));
		}
	    }
	    else if (S_IPMonitor_debug & kDebugFlag1) {
		syslog(LOG_NOTICE,
		       "IPMonitor: %s removed default route %s",
		       ifn,
		       inet_ntop(AF_INET6, &router_ip,
				 ntopbuf, sizeof(ntopbuf)));
	    }
	}
    }
    /* add the new Router */
    if (cfstring_to_ip6(new_router, &router_ip)) {
	if (IN6_IS_ADDR_LINKLOCAL(&router_ip) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&router_ip)) {
	    /* scope it */
	    router_ip.__u6_addr.__u6_addr16[1] = htons(if_index);
	}
	if (siocdradd_in6(s, if_index, &router_ip, 0) < 0) {
	    if (errno != EINVAL) {
		syslog(LOG_ERR,
		       "IPMonitor: siocdradd_in6(%s, %s) failed, %s",
		       ifn,
		       inet_ntop(AF_INET6, &router_ip,
				 ntopbuf, sizeof(ntopbuf)),
		       strerror(errno));
	    }
	}
	else if (S_IPMonitor_debug & kDebugFlag1) {
	    syslog(LOG_NOTICE,
		   "IPMonitor: %s added default route %s",
		   ifn,
		   inet_ntop(AF_INET6, &router_ip,
			     ntopbuf, sizeof(ntopbuf)));
	}
    }
    close(s);

 done:
#endif /* SIOCDRADD_IN6 */
    return (service_dict_set(serviceID, kSCEntNetIPv6, new_val));
}

#define	ALLOW_EMPTY_STRING	0x1

static CF_RETURNS_RETAINED CFTypeRef
sanitize_prop(CFTypeRef val, uint32_t flags)
{
    if (val != NULL) {
	if (isA_CFString(val)) {
	    CFMutableStringRef	str;

	    str = CFStringCreateMutableCopy(NULL, 0, (CFStringRef)val);
	    CFStringTrimWhitespace(str);
	    if (!(flags & ALLOW_EMPTY_STRING) && (CFStringGetLength(str) == 0)) {
		CFRelease(str);
		str = NULL;
	    }
	    val = str;
	} else {
	    CFRetain(val);
	}
    }

    return val;
}

static void
merge_array_prop(CFMutableDictionaryRef	dict,
		 CFStringRef		key,
		 CFDictionaryRef	state_dict,
		 CFDictionaryRef	setup_dict,
		 uint32_t		flags,
		 Boolean		append)
{
    CFMutableArrayRef	merge_prop;
    CFArrayRef		setup_prop = NULL;
    CFArrayRef		state_prop = NULL;

    if (setup_dict != NULL) {
	setup_prop = isA_CFArray(CFDictionaryGetValue(setup_dict, key));
    }
    if (state_dict != NULL) {
	state_prop = isA_CFArray(CFDictionaryGetValue(state_dict, key));
    }

    if ((setup_prop == NULL) && (state_prop == NULL)) {
	return;
    }

    merge_prop = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (setup_prop != NULL) {
	CFIndex	i;
	CFIndex	n;

	n = CFArrayGetCount(setup_prop);
	for (i = 0; i < n; i++) {
	    CFTypeRef   val;

	    val = CFArrayGetValueAtIndex(setup_prop, i);
	    val = sanitize_prop(val, flags);
	    if (val != NULL) {
		CFArrayAppendValue(merge_prop, val);
		CFRelease(val);
	    }
	}
    }
    if (state_prop != NULL
	&& (setup_prop == NULL || S_append_state)) {
	CFIndex	i;
	CFIndex	n;
	CFRange	setup_range	= CFRangeMake(0, CFArrayGetCount(merge_prop));

	n = CFArrayGetCount(state_prop);
	for (i = 0; i < n; i++) {
	    CFTypeRef   val;

	    val = CFArrayGetValueAtIndex(state_prop, i);
	    val = sanitize_prop(val, flags);
	    if (val != NULL) {
		if (append || !CFArrayContainsValue(merge_prop, setup_range, val)) {
		    CFArrayAppendValue(merge_prop, val);
		}
		CFRelease(val);
	    }
	}
    }
    if (CFArrayGetCount(merge_prop) > 0) {
	CFDictionarySetValue(dict, key, merge_prop);
    }
    CFRelease(merge_prop);
    return;
}

static void
pick_prop(CFMutableDictionaryRef	dict,
	  CFStringRef			key,
	  CFDictionaryRef		state_dict,
	  CFDictionaryRef		setup_dict,
	  uint32_t			flags)
{
	CFTypeRef	val	= NULL;

	if (setup_dict != NULL) {
	    val = CFDictionaryGetValue(setup_dict, key);
	    val = sanitize_prop(val, flags);
	}
	if (val == NULL && state_dict != NULL) {
	    val = CFDictionaryGetValue(state_dict, key);
	    val = sanitize_prop(val, flags);
	}
	if (val != NULL) {
	    CFDictionarySetValue(dict, key, val);
	    CFRelease(val);
	}

	return;
}

/**
 ** GetEntityChangesFunc functions
 **/
static boolean_t
get_ipv4_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		 CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    CFDictionaryRef		aggregated_dict = NULL;
    boolean_t			changed		= FALSE;
    CFMutableDictionaryRef	dict		= NULL;
    CFStringRef			primaryRank	= NULL;
    IPv4RouteListRef		r;
#define R_STATIC		3
    IPv4RouteListRef		routes;
    /* ALIGN: force align */
    uint32_t			routes_buf[roundup(IPv4RouteListComputeSize(R_STATIC), sizeof(uint32_t))];
    CFDataRef			routes_data	= NULL;
    CFDictionaryRef		service_options;

    if (state_dict == NULL) {
	goto done;
    }
    service_options = service_dict_get(serviceID, kSCEntNetService);
    if (service_options != NULL) {
	primaryRank = CFDictionaryGetValue(service_options, kSCPropNetServicePrimaryRank);
    }
    dict = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);
    if (setup_dict != NULL) {
	CFStringRef	router;
	struct in_addr	router_ip;

	router = CFDictionaryGetValue(setup_dict,
				      kSCPropNetIPv4Router);
	if (router != NULL
	    && cfstring_to_ip(router, &router_ip)) {
	    CFDictionarySetValue(dict,
				 kSCPropNetIPv4Router,
				 router);
	}
    }
    routes = (IPv4RouteListRef)(void *)routes_buf;
    routes->size = R_STATIC;
    routes->count = 0;
    r = IPv4RouteListCreateWithDictionary(routes, dict, primaryRank);
    if (r != NULL) {
	routes_data = CFDataCreate(NULL,
				   (const void *)r,
				   IPv4RouteListComputeSize(r->count));
	if (r != routes) {
	    free(r);
	}
    }
    else {
	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("IPMonitor: %@ invalid IPv4 dictionary = %@"),
	      serviceID,
	      dict);
    }
  done:
    if (routes_data != NULL) {
	CFStringRef	keys[2];
	CFTypeRef	values[2];

	keys[0] = kSCEntNetIPv4ServiceDict;
	values[0] = dict;
	keys[1] = kSCEntNetIPv4RouteList;
	values[1] = routes_data;

	aggregated_dict = CFDictionaryCreate(NULL,
					     (const void**)keys,
					     values,
					     sizeof(keys)/sizeof(keys[0]),
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);

    }
    changed = service_dict_set(serviceID, kSCEntNetIPv4, aggregated_dict);
    if (routes_data == NULL) {
	/* clean up the rank too */
	CFDictionaryRemoveValue(S_ipv4_service_rank_dict, serviceID);
    }
    my_CFRelease(&dict);
    my_CFRelease(&aggregated_dict);
    my_CFRelease(&routes_data);
    return (changed);
}

static boolean_t
get_ipv6_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		 CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    struct in6_addr		addr;
    CFArrayRef			addrs;
    boolean_t			changed = FALSE;
    CFMutableDictionaryRef	dict = NULL;
    CFDictionaryRef		new_dict = NULL;
    CFStringRef			router = NULL;
    struct in6_addr		router_ip;
    boolean_t			valid_ip = FALSE;

    if (state_dict == NULL) {
	goto done;
    }
    addrs = isA_CFArray(CFDictionaryGetValue(state_dict,
					     kSCPropNetIPv6Addresses));
    if (addrs != NULL && CFArrayGetCount(addrs) > 0) {
	valid_ip = cfstring_to_ip6(CFArrayGetValueAtIndex(addrs, 0), &addr);
    }
    if (valid_ip == FALSE) {
	if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("IPMonitor: %@ has no valid IPv6 address, ignoring"),
		  serviceID);
	}
	goto done;
    }
    dict = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);
    if (setup_dict != NULL) {
	router = CFDictionaryGetValue(setup_dict,
				      kSCPropNetIPv6Router);
	if (router != NULL && cfstring_to_ip6(router, &router_ip)) {
	    CFDictionarySetValue(dict,
				 kSCPropNetIPv6Router,
				 router);
	}
    }
    else {
	router = CFDictionaryGetValue(dict,
				      kSCPropNetIPv6Router);
	if (router != NULL
	    && cfstring_to_ip6(router, &router_ip) == FALSE) {
	    CFDictionaryRemoveValue(dict, kSCPropNetIPv6Router);
	}
    }
    new_dict = dict;

 done:
    changed = ipv6_service_dict_set(serviceID, new_dict);
    if (new_dict == NULL) {
	/* clean up the rank too */
	CFDictionaryRemoveValue(S_ipv6_service_rank_dict, serviceID);
    }
    my_CFRelease(&new_dict);
    return (changed);
}

static void
accumulate_dns_servers(CFArrayRef in_servers, ProtocolFlags active_protos,
		       CFMutableArrayRef out_servers, CFStringRef interface)
{
    int			count;
    int			i;

    count = CFArrayGetCount(in_servers);
    for (i = 0; i < count; i++) {
	CFStringRef	addr = CFArrayGetValueAtIndex(in_servers, i);
	struct in6_addr	ipv6_addr;
	struct in_addr	ip_addr;

	if (cfstring_to_ip(addr, &ip_addr)) {
	    /* IPv4 address */
	    if ((active_protos & kProtocolFlagsIPv4) == 0
		&& ntohl(ip_addr.s_addr) != INADDR_LOOPBACK) {
		if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		    syslog(LOG_NOTICE,
			   "IPMonitor: no IPv4 connectivity, "
			   "ignoring DNS server address " IP_FORMAT,
			   IP_LIST(&ip_addr));
		}
		continue;
	    }

	    CFRetain(addr);
	}
	else if (cfstring_to_ip6(addr, &ipv6_addr)) {
	    /* IPv6 address */
	    if ((active_protos & kProtocolFlagsIPv6) == 0
		&& !IN6_IS_ADDR_LOOPBACK(&ipv6_addr)) {
		if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		    char	ntopbuf[INET6_ADDRSTRLEN];

		    syslog(LOG_NOTICE,
			   "IPMonitor: no IPv6 connectivity, "
			   "ignoring DNS server address %s",
			   inet_ntop(AF_INET6, &ipv6_addr,
				     ntopbuf, sizeof(ntopbuf)));
		}
		continue;
	    }

	    if ((IN6_IS_ADDR_LINKLOCAL(&ipv6_addr) ||
		 IN6_IS_ADDR_MC_LINKLOCAL(&ipv6_addr))
		&& (interface != NULL)
		&& (CFStringFind(addr, CFSTR("%"), 0).location == kCFNotFound)) {
		// append interface name to IPv6 link local address
		addr = CFStringCreateWithFormat(NULL, NULL,
						CFSTR("%@%%%@"),
						addr,
						interface);
	    } else {
		CFRetain(addr);
	    }
	}
	else {
	    /* bad IP address */
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("IPMonitor: ignoring bad DNS server address '%@'"),
		  addr);
	    continue;
	}

	/* DNS server is valid and one we want */
	CFArrayAppendValue(out_servers, addr);
	CFRelease(addr);
    }
    return;
}

static void
merge_dns_servers(CFMutableDictionaryRef new_dict,
		  CFArrayRef state_servers,
		  CFArrayRef setup_servers,
		  Boolean have_setup,
		  ProtocolFlags active_protos,
		  CFStringRef interface)
{
    CFMutableArrayRef	dns_servers;
    Boolean		have_dns_setup	= FALSE;

    if (state_servers == NULL && setup_servers == NULL) {
	/* no DNS servers */
	return;
    }
    dns_servers = CFArrayCreateMutable(NULL, 0,
				       &kCFTypeArrayCallBacks);
    if (setup_servers != NULL) {
	accumulate_dns_servers(setup_servers, active_protos,
			       dns_servers, interface);
	if (CFArrayGetCount(dns_servers) > 0) {
	    have_dns_setup = TRUE;
	}
    }
    if ((CFArrayGetCount(dns_servers) == 0 || S_append_state)
	&& state_servers != NULL) {
	accumulate_dns_servers(state_servers, active_protos,
			       dns_servers, NULL);
    }

    /*
     * Here, we determine whether or not we want all queries for this DNS
     * configuration to be bound to the associated network interface.
     *
     * For dynamically derived network configurations (i.e. from State:)
     * this would be the preferred option using the argument "Hey, the
     * server told us to use these servers on this network so let's not
     * argue".
     *
     * But, when a DNS configuration has been provided by the user/admin
     * via the Network pref pane (i.e. from Setup:) we opt to not force
     * binding of the outbound queries.  The simplest example why we take
     * this stance is with a multi-homing configuration.  Consider a system
     * with one network service associated with "en0" and a second service
     * associated with "en1".  The "en0" service has been set higher in
     * the network service order so it would be primary but the user/admin
     * wants the DNS queries to go to a server only accessible via "en1".
     * Without this exception we would take the DNS server addresses from
     * the Network pref pane (for "en0") and have the queries bound to
     * "en0" where they'd never reach their intended destination (via
     * "en1").  So, our exception to the rule is that we will not bind
     * user/admin configurations to any specific network interface.
     *
     * We also add an exception to the "follow the dynamically derived
     * network configuration" path for on-the-fly (no Setup: content)
     * network services.
     */
    if (CFArrayGetCount(dns_servers) != 0) {
	CFDictionarySetValue(new_dict,
			     kSCPropNetDNSServerAddresses, dns_servers);
	if (have_setup && !have_dns_setup) {
	    CFDictionarySetValue(new_dict, DNS_CONFIGURATION_SCOPED_QUERY_KEY, kCFBooleanTrue);
	}
    }

    my_CFRelease(&dns_servers);
    return;
}


static boolean_t
get_dns_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    ProtocolFlags		active_protos	= kProtocolFlagsNone;
    boolean_t			changed		= FALSE;
    CFStringRef			domain;
    Boolean			have_setup	= FALSE;
    CFStringRef			interface	= NULL;
    CFDictionaryRef		ipv4;
    CFDictionaryRef		ipv6;
    int				i;
    const struct {
	CFStringRef     key;
	uint32_t	flags;
	Boolean		append;
    } merge_list[] = {
	{ kSCPropNetDNSSearchDomains,			0,			FALSE },
	{ kSCPropNetDNSSortList,			0,			FALSE },
	{ kSCPropNetDNSSupplementalMatchDomains,	ALLOW_EMPTY_STRING,	TRUE  },
	{ kSCPropNetDNSSupplementalMatchOrders,		0,			TRUE  },
    };
    CFMutableDictionaryRef      new_dict = NULL;
    const CFStringRef		pick_list[] = {
	kSCPropNetDNSDomainName,
	kSCPropNetDNSOptions,
	kSCPropNetDNSSearchOrder,
	kSCPropNetDNSServerPort,
	kSCPropNetDNSServerTimeout,
    };
    IPv4RouteListRef		routes = NULL;

    if ((state_dict == NULL) && (setup_dict == NULL)) {
	/* there is no DNS content */
	goto done;
    }

    ipv4 = service_dict_get(serviceID, kSCEntNetIPv4);
    routes = ipv4_dict_get_routelist(ipv4);

    if (routes != NULL) {
	if (get_service_setup_entity(info, serviceID, kSCEntNetIPv4) != NULL) {
	    have_setup = TRUE;
	}

	active_protos |= kProtocolFlagsIPv4;

	interface = ipv4_dict_get_ifname(ipv4);
    }

    ipv6 = service_dict_get(serviceID, kSCEntNetIPv6);
    if (ipv6 != NULL) {
	if (!have_setup &&
	    get_service_setup_entity(info, serviceID, kSCEntNetIPv6) != NULL) {
	    have_setup = TRUE;
	}

	active_protos |= kProtocolFlagsIPv6;

	if (interface == NULL) {
	    interface = CFDictionaryGetValue(ipv6,
					     kSCPropInterfaceName);
	}
    }


    if (active_protos == kProtocolFlagsNone) {
	/* there is no IPv4 nor IPv6 */
	if (state_dict == NULL) {
	    /* ... and no DNS content that we care about */
	    goto done;
	}
	setup_dict = NULL;
    }

    /* merge DNS configuration */
    new_dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

    if (active_protos == kProtocolFlagsNone) {
	merge_dns_servers(new_dict,
			  my_CFDictionaryGetArray(state_dict,
						  kSCPropNetDNSServerAddresses),
			  NULL,
			  FALSE,
			  kProtocolFlagsIPv4 | kProtocolFlagsIPv6,
			  NULL);
    }
    else {
	merge_dns_servers(new_dict,
			  my_CFDictionaryGetArray(state_dict,
						  kSCPropNetDNSServerAddresses),
			  my_CFDictionaryGetArray(setup_dict,
						  kSCPropNetDNSServerAddresses),
			  have_setup,
			  active_protos,
			  interface);
    }

    for (i = 0; i < sizeof(merge_list)/sizeof(merge_list[0]); i++) {
	merge_array_prop(new_dict,
			 merge_list[i].key,
			 state_dict,
			 setup_dict,
			 merge_list[i].flags,
			 merge_list[i].append);
    }

    for (i = 0; i < sizeof(pick_list)/sizeof(pick_list[0]); i++) {
	pick_prop(new_dict,
		  pick_list[i],
		  state_dict,
		  setup_dict,
		  0);
    }

    if (active_protos == kProtocolFlagsNone) {
	/* there is no IPv4 nor IPv6, only supplemental DNS */
	if (CFDictionaryContainsKey(new_dict,
				    kSCPropNetDNSSupplementalMatchDomains)) {
	    /* only keep State: supplemental */
	    CFDictionaryRemoveValue(new_dict, kSCPropNetDNSDomainName);
	    CFDictionaryRemoveValue(new_dict, kSCPropNetDNSSearchDomains);
	    CFDictionaryRemoveValue(new_dict, kSCPropNetDNSSearchOrder);
	    CFDictionaryRemoveValue(new_dict, kSCPropNetDNSSortList);
	} else {
	    goto done;
	}
    }

    if (CFDictionaryGetCount(new_dict) == 0) {
	my_CFRelease(&new_dict);
	goto done;
    }

    if (interface != NULL) {
	CFDictionarySetValue(new_dict, kSCPropInterfaceName, interface);
    }

    if (S_append_state) {
	/*
	 * ensure any specified domain name (e.g. the domain returned by
	 * a DHCP server) is in the search list.
	 */
	domain = CFDictionaryGetValue(new_dict, kSCPropNetDNSDomainName);
	if (isA_CFString(domain)) {
	    CFArrayRef      search;

	    search = CFDictionaryGetValue(new_dict, kSCPropNetDNSSearchDomains);
	    if (isA_CFArray(search) &&
		!CFArrayContainsValue(search, CFRangeMake(0, CFArrayGetCount(search)), domain)) {
		CFMutableArrayRef   new_search;

		new_search = CFArrayCreateMutableCopy(NULL, 0, search);
		CFArrayAppendValue(new_search, domain);
		CFDictionarySetValue(new_dict, kSCPropNetDNSSearchDomains, new_search);
		my_CFRelease(&new_search);
	    }
	}
    }

 done:
    changed = service_dict_set(serviceID, kSCEntNetDNS, new_dict);
    my_CFRelease(&new_dict);
    return (changed);
}

static void
merge_dict(const void *key, const void *value, void *context)
{
	CFMutableDictionaryRef	dict	= (CFMutableDictionaryRef)context;

	CFDictionarySetValue(dict, key, value);
	return;
}

#define	PROXY_AUTO_DISCOVERY_URL	252

static CF_RETURNS_RETAINED CFStringRef
wpadURL_dhcp(CFDictionaryRef dhcp_options)
{
    CFStringRef	urlString	= NULL;

    if (isA_CFDictionary(dhcp_options)) {
	CFDataRef	data;

	data = DHCPInfoGetOptionData(dhcp_options, PROXY_AUTO_DISCOVERY_URL);
	if (data != NULL) {
	    CFURLRef    url;
	    const UInt8	*urlBytes;
	    CFIndex	urlLen;

	    urlBytes = CFDataGetBytePtr(data);
	    urlLen   = CFDataGetLength(data);
	    while ((urlLen > 0) && (urlBytes[urlLen - 1] == 0)) {
		// remove trailing NUL
		urlLen--;
	    }

	    if (urlLen <= 0) {
		return NULL;
	    }

	    url = CFURLCreateWithBytes(NULL, urlBytes, urlLen, kCFStringEncodingUTF8, NULL);
	    if (url != NULL) {
		urlString = CFURLGetString(url);
		if (urlString != NULL) {
		    CFRetain(urlString);
		}
		CFRelease(url);
	    }
	}
    }

    return urlString;
}

static CF_RETURNS_RETAINED CFStringRef
wpadURL_dns(void)
{
    CFURLRef	url;
    CFStringRef	urlString	= NULL;

    url = CFURLCreateWithString(NULL, CFSTR("http://wpad/wpad.dat"), NULL);
    if (url != NULL) {
	urlString = CFURLGetString(url);
	if (urlString != NULL) {
	    CFRetain(urlString);
	}
	CFRelease(url);
    }

    return urlString;
}

static boolean_t
get_proxies_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		    CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    ProtocolFlags		active_protos	= kProtocolFlagsNone;
    boolean_t			changed		= FALSE;
    CFStringRef			interface	= NULL;
    CFDictionaryRef		ipv4;
    CFDictionaryRef		ipv6;
    CFMutableDictionaryRef	new_dict	= NULL;
    const struct {
	CFStringRef     key;
	uint32_t	flags;
	Boolean		append;
    } merge_list[] = {
	{ kSCPropNetProxiesSupplementalMatchDomains,	ALLOW_EMPTY_STRING,	TRUE  },
	{ kSCPropNetProxiesSupplementalMatchOrders,	0,			TRUE  },
    };
    const struct {
	    CFStringRef	key1;	/* an "enable" key */
	    CFStringRef	key2;
	    CFStringRef	key3;
    } pick_list[] = {
	    { kSCPropNetProxiesFTPEnable,	kSCPropNetProxiesFTPProxy,	kSCPropNetProxiesFTPPort	},
	    { kSCPropNetProxiesGopherEnable,	kSCPropNetProxiesGopherProxy,	kSCPropNetProxiesGopherPort	},
	    { kSCPropNetProxiesHTTPEnable,	kSCPropNetProxiesHTTPProxy,	kSCPropNetProxiesHTTPPort	},
	    { kSCPropNetProxiesHTTPSEnable,	kSCPropNetProxiesHTTPSProxy,	kSCPropNetProxiesHTTPSPort	},
	    { kSCPropNetProxiesRTSPEnable,	kSCPropNetProxiesRTSPProxy,	kSCPropNetProxiesRTSPPort	},
	    { kSCPropNetProxiesSOCKSEnable,	kSCPropNetProxiesSOCKSProxy,	kSCPropNetProxiesSOCKSPort	},
	    { kSCPropNetProxiesProxyAutoConfigEnable,
	      kSCPropNetProxiesProxyAutoConfigURLString,
	      kSCPropNetProxiesProxyAutoConfigJavaScript, },
	    { kSCPropNetProxiesProxyAutoDiscoveryEnable,
	      NULL,
	      NULL, }
    };
    IPv4RouteListRef		routes		= NULL;

    if ((state_dict == NULL) && (setup_dict == NULL)) {
	/* there is no proxy content */
	goto done;
    }

    ipv4 = service_dict_get(serviceID, kSCEntNetIPv4);
    routes = ipv4_dict_get_routelist(ipv4);

    if (routes != NULL) {
	active_protos |= kProtocolFlagsIPv4;

	interface = ipv4_dict_get_ifname(ipv4);
    }

    ipv6 = service_dict_get(serviceID, kSCEntNetIPv6);
    if (ipv6 != NULL) {
	active_protos |= kProtocolFlagsIPv6;

	if (interface == NULL) {
	    interface = CFDictionaryGetValue(ipv6,
					     kSCPropInterfaceName);
	}
    }

    if (active_protos == kProtocolFlagsNone) {
	/* there is no IPv4 nor IPv6 */
	if (state_dict == NULL) {
	    /* ... and no proxy content that we care about */
	    goto done;
	}
	setup_dict = NULL;
    }

    if ((setup_dict != NULL) && (state_dict != NULL)) {
	CFIndex			i;
	CFMutableDictionaryRef	setup_copy;

	/*
	 * Merge the per-service "Setup:" and "State:" proxy information with
	 * the "Setup:" information always taking precedence.  Additionally,
	 * ensure that if any group of "Setup:" values (e.g. Enabled, Proxy,
	 * Port) is defined than all of the values for that group will be
	 * used.  That is, we don't allow mixing some of the values from
	 * the "Setup:" keys and others from the "State:" keys.
	 */
	new_dict   = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);

	for (i = 0; i < sizeof(merge_list)/sizeof(merge_list[0]); i++) {
	    merge_array_prop(new_dict,
			     merge_list[i].key,
			     state_dict,
			     setup_dict,
			     merge_list[i].flags,
			     merge_list[i].append);
	}

	setup_copy = CFDictionaryCreateMutableCopy(NULL, 0, setup_dict);
	for (i = 0; i < sizeof(pick_list)/sizeof(pick_list[0]); i++) {
	    if (CFDictionaryContainsKey(setup_copy, pick_list[i].key1)) {
		/*
		 * if a "Setup:" enabled key has been provided than we want to
		 * ignore all of the "State:" keys
		 */
		CFDictionaryRemoveValue(new_dict, pick_list[i].key1);
		if (pick_list[i].key2 != NULL) {
		    CFDictionaryRemoveValue(new_dict, pick_list[i].key2);
		}
		if (pick_list[i].key3 != NULL) {
		    CFDictionaryRemoveValue(new_dict, pick_list[i].key3);
		}
	    } else if (CFDictionaryContainsKey(state_dict, pick_list[i].key1) ||
		       ((pick_list[i].key2 != NULL) && CFDictionaryContainsKey(state_dict, pick_list[i].key2)) ||
		       ((pick_list[i].key3 != NULL) && CFDictionaryContainsKey(state_dict, pick_list[i].key3))) {
		/*
		 * if a "Setup:" enabled key has not been provided and we have
		 * some" "State:" keys than we remove all of of "Setup:" keys
		 */
		CFDictionaryRemoveValue(setup_copy, pick_list[i].key1);
		if (pick_list[i].key2 != NULL) {
		    CFDictionaryRemoveValue(setup_copy, pick_list[i].key2);
		}
		if (pick_list[i].key3 != NULL) {
		    CFDictionaryRemoveValue(setup_copy, pick_list[i].key3);
		}
	    }
	}

	/* merge the "Setup:" keys */
	CFDictionaryApplyFunction(setup_copy, merge_dict, new_dict);
	CFRelease(setup_copy);
    }
    else if (setup_dict != NULL) {
	new_dict = CFDictionaryCreateMutableCopy(NULL, 0, setup_dict);
    }
    else if (state_dict != NULL) {
	new_dict = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);
    }

    if ((new_dict != NULL) && (CFDictionaryGetCount(new_dict) == 0)) {
	CFRelease(new_dict);
	new_dict = NULL;
    }

    if ((new_dict != NULL) && (interface != NULL)) {
	CFDictionarySetValue(new_dict, kSCPropInterfaceName, interface);
    }

    /* process WPAD */
    if (new_dict != NULL) {
	CFDictionaryRef	dhcp_options;
	CFNumberRef	num;
	CFNumberRef	wpad	    = NULL;
	int		wpadEnabled = 0;
	CFStringRef	wpadURL	    = NULL;

	if (CFDictionaryGetValueIfPresent(new_dict,
					  kSCPropNetProxiesProxyAutoDiscoveryEnable,
					  (const void **)&num) &&
	    isA_CFNumber(num)) {
	    /* if we have a WPAD key */
	    wpad = num;
	    if (!CFNumberGetValue(num, kCFNumberIntType, &wpadEnabled)) {
		/* if we don't like the enabled key/value */
		wpadEnabled = 0;
	    }
	}

	if (wpadEnabled) {
	    int	pacEnabled  = 0;

	    num = CFDictionaryGetValue(new_dict, kSCPropNetProxiesProxyAutoConfigEnable);
	    if (!isA_CFNumber(num) ||
		!CFNumberGetValue(num, kCFNumberIntType, &pacEnabled)) {
		/* if we don't like the enabled key/value */
		pacEnabled = 0;
	    }

	    if (pacEnabled) {
		CFStringRef	pacURL;

		pacURL = CFDictionaryGetValue(new_dict, kSCPropNetProxiesProxyAutoConfigURLString);
		if (pacURL != NULL) {
		    if (!isA_CFString(pacURL)) {
			/* if we don't like the PAC URL */
			pacEnabled = 0;
		    }
		} else {
		    CFStringRef	pacJS;

		    pacJS = CFDictionaryGetValue(new_dict, kSCPropNetProxiesProxyAutoConfigJavaScript);
		    if (!isA_CFString(pacJS)) {
			/* if we don't have (or like) the PAC JavaScript */
			pacEnabled = 0;
		    }
		}
	    }

	    if (pacEnabled) {
		/*
		 * we already have a PAC URL so disable WPAD.
		 */
		wpadEnabled = 0;
		goto setWPAD;
	    }

	    /*
	     * if WPAD is enabled and we don't already have a PAC URL then
	     * we check for a DHCP provided URL.  If not available, we use
	     * a PAC URL pointing to a well-known file (wpad.dat) on a
	     * well-known host (wpad.<domain>).
	     */
	    dhcp_options = get_service_state_entity(info, serviceID, kSCEntNetDHCP);
	    wpadURL = wpadURL_dhcp(dhcp_options);
	    if (wpadURL == NULL) {
		wpadURL = wpadURL_dns();
	    }
	    if (wpadURL == NULL) {
		wpadEnabled = 0;    /* if we don't have a WPAD URL */
		goto setWPAD;
	    }

	    pacEnabled = 1;
	    num = CFNumberCreate(NULL, kCFNumberIntType, &pacEnabled);
	    CFDictionarySetValue(new_dict,
				 kSCPropNetProxiesProxyAutoConfigEnable,
				 num);
	    CFRelease(num);
	    CFDictionarySetValue(new_dict,
				 kSCPropNetProxiesProxyAutoConfigURLString,
				 wpadURL);
	    CFRelease(wpadURL);
	}

     setWPAD:
	if (wpad != NULL) {
	    num = CFNumberCreate(NULL, kCFNumberIntType, &wpadEnabled);
	    CFDictionarySetValue(new_dict,
				 kSCPropNetProxiesProxyAutoDiscoveryEnable,
				 num);
	    CFRelease(num);
	}
    }

 done:
    changed = service_dict_set(serviceID, kSCEntNetProxies, new_dict);
    my_CFRelease(&new_dict);
    return (changed);
}

#if	!TARGET_OS_IPHONE
static boolean_t
get_smb_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    boolean_t			changed = FALSE;
    int				i;
    CFMutableDictionaryRef      new_dict = NULL;
    const CFStringRef		pick_list[] = {
	kSCPropNetSMBNetBIOSName,
	kSCPropNetSMBNetBIOSNodeType,
#ifdef	ADD_NETBIOS_SCOPE
	kSCPropNetSMBNetBIOSScope,
#endif	// ADD_NETBIOS_SCOPE
	kSCPropNetSMBWorkgroup,
    };

    if (service_dict_get(serviceID, kSCEntNetIPv4) == NULL) {
	/* there is no IPv4 */
	goto done;
    }

    if (state_dict == NULL && setup_dict == NULL) {
	/* there is no SMB */
	goto done;
    }

    /* merge SMB configuration */
    new_dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

    merge_array_prop(new_dict,
		     kSCPropNetSMBWINSAddresses,
		     state_dict,
		     setup_dict,
		     0,
		     FALSE);
    for (i = 0; i < sizeof(pick_list)/sizeof(pick_list[0]); i++) {
	pick_prop(new_dict,
		  pick_list[i],
		  state_dict,
		  setup_dict,
		  0);
    }

    if (CFDictionaryGetCount(new_dict) == 0) {
	my_CFRelease(&new_dict);
	goto done;
    }

 done:
    changed = service_dict_set(serviceID, kSCEntNetSMB, new_dict);
    my_CFRelease(&new_dict);
    return (changed);
}
#endif	/* !TARGET_OS_IPHONE */

static CFStringRef
services_info_get_interface(CFDictionaryRef services_info,
			    CFStringRef serviceID)
{
    CFStringRef		interface = NULL;
    CFDictionaryRef	ipv4_dict;

    ipv4_dict = get_service_state_entity(services_info, serviceID,
					 kSCEntNetIPv4);
    if (isA_CFDictionary(ipv4_dict) != NULL) {
	interface = CFDictionaryGetValue(ipv4_dict, kSCPropInterfaceName);
    }
    else {
	CFDictionaryRef		ipv6_dict;

	ipv6_dict = get_service_state_entity(services_info, serviceID,
					     kSCEntNetIPv6);
	if (isA_CFDictionary(ipv6_dict) != NULL) {
	    interface = CFDictionaryGetValue(ipv6_dict, kSCPropInterfaceName);
	}
    }
    return (interface);
}

static boolean_t
get_rank_changes(CFStringRef serviceID, CFDictionaryRef state_options,
		 CFDictionaryRef setup_options, CFDictionaryRef services_info)
{
    boolean_t			changed		= FALSE;
    CFMutableDictionaryRef      new_dict	= NULL;
    CFStringRef			new_rank	= NULL;
    CFStringRef			setup_rank	= NULL;
    CFStringRef			state_rank	= NULL;


    /*
     * Check "PrimaryRank" setting
     *
     * Note 1: Rank setting in setup/state option overwrites the
     *         Rank setting in interface
     * Within each rank setting, the following precedence is defined:
     *
     * Note 2: Rank Never > Rank Last > Rank First > Rank None
     */
    if (isA_CFDictionary(setup_options)) {
	setup_rank = CFDictionaryGetValue(setup_options, kSCPropNetServicePrimaryRank);
	setup_rank = isA_CFString(setup_rank);
    }
    if (isA_CFDictionary(state_options)) {
	state_rank = CFDictionaryGetValue(state_options, kSCPropNetServicePrimaryRank);
	state_rank = isA_CFString(state_rank);
    }

    if (((setup_rank != NULL) && CFEqual(setup_rank, kSCValNetServicePrimaryRankNever)) ||
	((state_rank != NULL) && CFEqual(state_rank, kSCValNetServicePrimaryRankNever))) {
	new_rank = kSCValNetServicePrimaryRankNever;
    }
    else if (((setup_rank != NULL) && CFEqual(setup_rank, kSCValNetServicePrimaryRankLast)) ||
	     ((state_rank != NULL) && CFEqual(state_rank, kSCValNetServicePrimaryRankLast))) {
	new_rank = kSCValNetServicePrimaryRankLast;
    }
    else if (((setup_rank != NULL) && CFEqual(setup_rank, kSCValNetServicePrimaryRankFirst)) ||
	     ((state_rank != NULL) && CFEqual(state_rank, kSCValNetServicePrimaryRankFirst))) {
	new_rank = kSCValNetServicePrimaryRankFirst;
    }

    /* This corresponds to Note 1 */
    if (setup_rank == NULL && state_rank == NULL) {
	/* Fetch the interface associated with the service */
	CFStringRef interface;

	interface = services_info_get_interface(services_info, serviceID);

	/* Get the rank on that interface */
	if (interface != NULL) {
	    new_rank = CFDictionaryGetValue(S_if_rank_dict, interface);
	    if (S_IPMonitor_debug & kDebugFlag1) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("serviceID %@ interface %@ rank = %@"),
		      serviceID, interface,
		      (new_rank != NULL) ? new_rank : CFSTR("<none>"));
	    }
	}
    }

    if (new_rank != NULL) {
	new_dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(new_dict, kSCPropNetServicePrimaryRank, new_rank);
    }

    changed = service_dict_set(serviceID, kSCEntNetService, new_dict);
    my_CFRelease(&new_dict);
    return (changed);
}

static CFStringRef
if_rank_key_copy(CFStringRef ifname)
{
    return (SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  ifname,
							  kSCEntNetService));
}

static void
if_rank_set(CFStringRef ifname, CFDictionaryRef rank_dict)
{
    CFStringRef		rank = NULL;

    if (isA_CFDictionary(rank_dict) != NULL) {
	rank = CFDictionaryGetValue(rank_dict, kSCPropNetServicePrimaryRank);
	rank = isA_CFString(rank);
    }

    /* specific rank is asserted */
    if (rank != NULL) {
	if (S_IPMonitor_debug & kDebugFlag1) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("Interface %@ asserted rank %@"),
		  ifname, rank);
	}
	CFDictionarySetValue(S_if_rank_dict, ifname, rank);
    } else {
	if (S_IPMonitor_debug & kDebugFlag1) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("Interface %@ removed rank."),
		  ifname);
	}
	CFDictionaryRemoveValue(S_if_rank_dict, ifname);
    }
    return;
}

static void
if_rank_apply(const void * key, const void * value, void * context)
{
    CFStringRef		ifname;
    CFDictionaryRef	rank_dict = (CFDictionaryRef)value;

    /* State:/Network/Interface/<ifname>/Service, <ifname> is at index 3 */
    ifname = my_CFStringCopyComponent(key, CFSTR("/"), 3);
    if (ifname != NULL) {
	if_rank_set(ifname, rank_dict);
	CFRelease(ifname);
    }
    return;
}

static void
if_rank_dict_init(void)
{
    CFDictionaryRef	info;
    CFStringRef		pattern;
    CFArrayRef		patterns;

    S_if_rank_dict
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);
    pattern = if_rank_key_copy(kSCCompAnyRegex);
    patterns = CFArrayCreate(NULL,
			     (const void **)&pattern, 1,
			     &kCFTypeArrayCallBacks);
    CFRelease(pattern);
    info = SCDynamicStoreCopyMultiple(S_session, NULL, patterns);
    CFRelease(patterns);
    if (info != NULL) {
	CFDictionaryApplyFunction(info, if_rank_apply, NULL);
	CFRelease(info);
    }
    return;

}

static void
add_service_keys(CFStringRef serviceID, CFMutableArrayRef keys, CFMutableArrayRef patterns)
{
    int			i;
    CFStringRef		key;

    if (CFEqual(serviceID, kSCCompAnyRegex)) {
	keys = patterns;
    }

    for (i = 0; i < ENTITY_TYPES_COUNT; i++) {
	key = setup_service_key(serviceID, *entityTypeNames[i]);
	CFArrayAppendValue(keys, key);
	CFRelease(key);
	key = state_service_key(serviceID, *entityTypeNames[i]);
	CFArrayAppendValue(keys, key);
	CFRelease(key);
    }

    key = state_service_key(serviceID, kSCEntNetDHCP);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);

    key = setup_service_key(serviceID, NULL);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);
    key = state_service_key(serviceID, NULL);
    CFArrayAppendValue(patterns, key);
    CFRelease(key);


    return;
}

static CFDictionaryRef
services_info_copy(SCDynamicStoreRef session, CFArrayRef service_list,
		   CFArrayRef if_rank_list)
{
    int			count;
    CFMutableArrayRef	get_keys;
    CFMutableArrayRef	get_patterns;
    int			if_count;
    CFDictionaryRef	info;
    int			s;

    count = CFArrayGetCount(service_list);
    get_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    get_patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFArrayAppendValue(get_keys, S_setup_global_ipv4);
    CFArrayAppendValue(get_keys, S_multicast_resolvers);
    CFArrayAppendValue(get_keys, S_private_resolvers);

    for (s = 0; s < count; s++) {
	CFStringRef	serviceID = CFArrayGetValueAtIndex(service_list, s);

	add_service_keys(serviceID, get_keys, get_patterns);
    }

    if_count = (if_rank_list != NULL)
	       ? CFArrayGetCount(if_rank_list) : 0;
    for (s = 0; s < if_count; s++) {
	CFStringRef	ifname = CFArrayGetValueAtIndex(if_rank_list, s);
	CFStringRef	key;

	key = if_rank_key_copy(ifname);
	CFArrayAppendValue(get_keys, key);
	CFRelease(key);
    }

    info = SCDynamicStoreCopyMultiple(session, get_keys, get_patterns);
    my_CFRelease(&get_keys);
    my_CFRelease(&get_patterns);
    return (info);
}

static int			rtm_seq = 0;

static int
route_open_socket(void)
{
    int sockfd;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) == -1) {
	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("IPMonitor: route_open_socket: socket failed, %s"),
	      strerror(errno));
    }
    return (sockfd);
}

/*
 * Define: ROUTE_MSG_ADDRS_SPACE
 * Purpose:
 *   Since sizeof(sockaddr_dl) > sizeof(sockaddr_in), we need space for
 *   3 sockaddr_in's and 2 sockaddr_dl's, but pad it just in case
 *   someone changes the code and doesn't think to modify this.
 */
#define ROUTE_MSG_ADDRS_SPACE	(3 * sizeof(struct sockaddr_in)	\
				 + 2 * sizeof(struct sockaddr_dl) \
				 + 128)
typedef struct {
    struct rt_msghdr	hdr;
    char		addrs[ROUTE_MSG_ADDRS_SPACE];
} route_msg;

static int
ipv4_route(int sockfd,
	   int cmd, struct in_addr gateway, struct in_addr netaddr,
	   struct in_addr netmask, char * ifname, unsigned int ifindex,
	   struct in_addr ifa, RouteFlags flags)
{
    boolean_t			default_route = (netaddr.s_addr == 0);
    int				len;
    int				ret = 0;
    route_msg			rtmsg;
    union {
	struct sockaddr_in *	in_p;
	struct sockaddr_dl *	dl_p;
	void *			ptr;
    } rtaddr;

    if (default_route && S_netboot) {
	return (0);
    }

    if (ifname == NULL) {
	/* this should not happen, but rather than crash, return an error */
	syslog(LOG_NOTICE,
	       "IPMonitor: ipv4_route ifname is NULL on network address %s",
	       inet_ntoa(netaddr));
	return (EBADF);
    }
    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs
	= RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFP | RTA_IFA;
    if (default_route
	&& (flags & kRouteIsDirectToInterfaceFlag) == 0) {
	rtmsg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
    }
    else {
	rtmsg.hdr.rtm_flags = RTF_UP | RTF_CLONING | RTF_STATIC;
    }
    if ((flags & kRouteIsScopedFlag) != 0) {
#ifdef RTF_IFSCOPE
	if (!S_scopedroute) {
	    return (0);
	}
	if (ifindex == 0) {
	    /* specifically asked for a scoped route, yet no index supplied */
	    syslog(LOG_NOTICE,
		   "IPMonitor: ipv4_route index is 0 on %s-scoped route %s",
		   ifname, inet_ntoa(netaddr));
	    return (EBADF);
	}
	rtmsg.hdr.rtm_index = ifindex;
	rtmsg.hdr.rtm_flags |= RTF_IFSCOPE;
#else /* RTF_IFSCOPE */
	return (0);
#endif /* RTF_IFSCOPE */
    }

    rtaddr.ptr = rtmsg.addrs;

    /* dest */
    rtaddr.in_p->sin_len = sizeof(*rtaddr.in_p);
    rtaddr.in_p->sin_family = AF_INET;
    rtaddr.in_p->sin_addr = netaddr;
    rtaddr.ptr += sizeof(*rtaddr.in_p);

    /* gateway */
    if ((rtmsg.hdr.rtm_flags & RTF_GATEWAY) != 0) {
	/* gateway is an IP address */
	rtaddr.in_p->sin_len = sizeof(*rtaddr.in_p);
	rtaddr.in_p->sin_family = AF_INET;
	rtaddr.in_p->sin_addr = gateway;
	rtaddr.ptr += sizeof(*rtaddr.in_p);
    }
    else {
	/* gateway is the interface itself */
	rtaddr.dl_p->sdl_len = sizeof(*rtaddr.dl_p);
	rtaddr.dl_p->sdl_family = AF_LINK;
	rtaddr.dl_p->sdl_nlen = strlen(ifname);
	bcopy(ifname, rtaddr.dl_p->sdl_data, rtaddr.dl_p->sdl_nlen);
	rtaddr.ptr += sizeof(*rtaddr.dl_p);
    }

    /* mask */
    rtaddr.in_p->sin_len = sizeof(*rtaddr.in_p);
    rtaddr.in_p->sin_family = AF_INET;
    rtaddr.in_p->sin_addr = netmask;
    rtaddr.ptr += sizeof(*rtaddr.in_p);

    /* interface name */
    rtaddr.dl_p->sdl_len = sizeof(*rtaddr.dl_p);
    rtaddr.dl_p->sdl_family = AF_LINK;
    rtaddr.dl_p->sdl_nlen = strlen(ifname);
    bcopy(ifname, rtaddr.dl_p->sdl_data, rtaddr.dl_p->sdl_nlen);
    rtaddr.ptr += sizeof(*rtaddr.dl_p);

    /* interface address */
    rtaddr.in_p->sin_len = sizeof(*rtaddr.in_p);
    rtaddr.in_p->sin_family = AF_INET;
    rtaddr.in_p->sin_addr = ifa;
    rtaddr.ptr += sizeof(*rtaddr.in_p);

    len = sizeof(rtmsg.hdr) + (rtaddr.ptr - (void *)rtmsg.addrs);
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) == -1) {
	ret = errno;
    }
    return (ret);
}

static boolean_t
ipv6_route(int cmd, struct in6_addr gateway, struct in6_addr netaddr,
	   struct in6_addr netmask, char * ifname, boolean_t is_direct)
{
    boolean_t			default_route;
    int				len;
    boolean_t			ret = TRUE;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in6	dst;
	struct sockaddr_in6	gway;
	struct sockaddr_in6	mask;
	struct sockaddr_dl	ifp;
    }				rtmsg;
    int				sockfd = -1;
    struct in6_addr		zeroes = IN6ADDR_ANY_INIT;

    default_route = (bcmp(&zeroes, &netaddr, sizeof(netaddr)) == 0);

    if ((IN6_IS_ADDR_LINKLOCAL(&gateway) ||
	 IN6_IS_ADDR_MC_LINKLOCAL(&gateway)) &&
	(ifname != NULL)) {
	unsigned int	index = if_nametoindex(ifname);

	/* add the scope id to the link local address */
	gateway.__u6_addr.__u6_addr16[1] = (uint16_t)htons(index);
    }
    sockfd = route_open_socket();
    if (sockfd == -1) {
	return (FALSE);
    }
    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    if (default_route) {
	if (is_direct) {
	    /* if router is directly reachable, don't set the gateway flag */
	    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
	}
	else {
	    rtmsg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	}
    }
    else {
	rtmsg.hdr.rtm_flags = RTF_UP | RTF_CLONING | RTF_STATIC;
    }
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    rtmsg.dst.sin6_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin6_family = AF_INET6;
    rtmsg.dst.sin6_addr = netaddr;
    rtmsg.gway.sin6_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin6_family = AF_INET6;
    rtmsg.gway.sin6_addr = gateway;
    rtmsg.mask.sin6_len = sizeof(rtmsg.mask);
    rtmsg.mask.sin6_family = AF_INET6;
    rtmsg.mask.sin6_addr = netmask;

    len = sizeof(rtmsg);
    if (ifname) {
	rtmsg.ifp.sdl_len = sizeof(rtmsg.ifp);
	rtmsg.ifp.sdl_family = AF_LINK;
	rtmsg.ifp.sdl_nlen = strlen(ifname);
	rtmsg.hdr.rtm_addrs |= RTA_IFP;
	bcopy(ifname, rtmsg.ifp.sdl_data, rtmsg.ifp.sdl_nlen);
    }
    else {
	/* no ifp information */
	len -= sizeof(rtmsg.ifp);
    }
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) == -1) {
	if ((cmd == RTM_ADD) && (errno == EEXIST)) {
	    /* no sense complaining about a route that already exists */
	}
	else if ((cmd == RTM_DELETE) && (errno == ESRCH)) {
	    /* no sense complaining about a route that isn't there */
	}
	else {
	    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("IPMonitor ipv6_route: write routing"
			    " socket failed, %s"), strerror(errno));
	    }
	    ret = FALSE;
	}
    }

    close(sockfd);
    return (ret);
}

static boolean_t
ipv6_default_route_delete(void)
{
    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("IPMonitor: IPv6 route delete default"));
    }
    return (ipv6_route(RTM_DELETE, S_ip6_zeros, S_ip6_zeros, S_ip6_zeros,
		       NULL, FALSE));
}

static boolean_t
ipv6_default_route_add(struct in6_addr router, char * ifname,
		       boolean_t is_direct)
{
    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	char	ntopbuf[INET6_ADDRSTRLEN];

	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("IPMonitor: IPv6 route add default"
		    " %s interface %s direct %d"),
	      inet_ntop(AF_INET6, &router, ntopbuf, sizeof(ntopbuf)),
	      ifname, is_direct);
    }
    return (ipv6_route(RTM_ADD, router, S_ip6_zeros, S_ip6_zeros,
		       ifname, is_direct));
}


static int
multicast_route_delete(int sockfd)
{
    struct in_addr gateway = { htonl(INADDR_LOOPBACK) };
    struct in_addr netaddr = { htonl(INADDR_UNSPEC_GROUP) };
    struct in_addr netmask = { htonl(IN_CLASSD_NET) };

    return (ipv4_route(sockfd, RTM_DELETE, gateway, netaddr, netmask, "lo0", 0,
		       gateway, 0));
}

static int
multicast_route_add(int sockfd)
{
    struct in_addr gateway = { htonl(INADDR_LOOPBACK) };
    struct in_addr netaddr = { htonl(INADDR_UNSPEC_GROUP) };
    struct in_addr netmask = { htonl(IN_CLASSD_NET) };

    return (ipv4_route(sockfd, RTM_ADD, gateway, netaddr, netmask, "lo0", 0,
		       gateway, 0));
}

#ifdef RTF_IFSCOPE
static void
set_ipv6_default_interface(char * ifname)
{
    struct in6_ndifreq	ndifreq;
    int			sock;

    bzero((char *)&ndifreq, sizeof(ndifreq));
    if (ifname != NULL) {
	strlcpy(ndifreq.ifname, ifname, sizeof(ndifreq.ifname));
	ndifreq.ifindex = if_nametoindex(ifname);
    } else {
	strlcpy(ndifreq.ifname, "lo0", sizeof(ndifreq.ifname));
	ndifreq.ifindex = 0;
    }

    sock = inet6_dgram_socket();
    if (sock == -1) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("IPMonitor: set_ipv6_default_interface: socket failed, %s"),
	      strerror(errno));
	return;
    }
    if (ioctl(sock, SIOCSDEFIFACE_IN6, (caddr_t)&ndifreq) == -1) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("IPMonitor: set_ipv6_default_interface: ioctl(SIOCSDEFIFACE_IN6) failed, %s"),
	      strerror(errno));
    }
    close(sock);
    return;
}
#endif /* RTF_IFSCOPE */

static void
set_ipv6_router(struct in6_addr * router, char * ifname, boolean_t is_direct)
{
    /* assign the new default route, ensure local multicast route available */
    (void)ipv6_default_route_delete();
    if (router != NULL) {
	(void)ipv6_default_route_add(*router, ifname, is_direct);
    }
    return;
}

#if	!TARGET_OS_IPHONE
static __inline__ void
empty_dns()
{
    (void)unlink(VAR_RUN_RESOLV_CONF);
}

static void
set_dns(CFArrayRef val_search_domains,
	CFStringRef val_domain_name,
	CFArrayRef val_servers,
	CFArrayRef val_sortlist)
{
    FILE * f = fopen(VAR_RUN_RESOLV_CONF "-", "w");

    /* publish new resolv.conf */
    if (f) {
	CFIndex	i;
	CFIndex	n;

	SCPrint(TRUE, f, CFSTR("#\n"));
	SCPrint(TRUE, f, CFSTR("# Mac OS X Notice\n"));
	SCPrint(TRUE, f, CFSTR("#\n"));
	SCPrint(TRUE, f, CFSTR("# This file is not used by the host name and address resolution\n"));
	SCPrint(TRUE, f, CFSTR("# or the DNS query routing mechanisms used by most processes on\n"));
	SCPrint(TRUE, f, CFSTR("# this Mac OS X system.\n"));
	SCPrint(TRUE, f, CFSTR("#\n"));
	SCPrint(TRUE, f, CFSTR("# This file is automatically generated.\n"));
	SCPrint(TRUE, f, CFSTR("#\n"));

	if (isA_CFArray(val_search_domains)) {
	    SCPrint(TRUE, f, CFSTR("search"));
	    n = CFArrayGetCount(val_search_domains);
	    for (i = 0; i < n; i++) {
		CFStringRef	domain;

		domain = CFArrayGetValueAtIndex(val_search_domains, i);
		if (isA_CFString(domain)) {
		    SCPrint(TRUE, f, CFSTR(" %@"), domain);
		}
	    }
	    SCPrint(TRUE, f, CFSTR("\n"));
	}
	else if (isA_CFString(val_domain_name)) {
		SCPrint(TRUE, f, CFSTR("domain %@\n"), val_domain_name);
	}

	if (isA_CFArray(val_servers)) {
	    n = CFArrayGetCount(val_servers);
	    for (i = 0; i < n; i++) {
		CFStringRef	nameserver;

		nameserver = CFArrayGetValueAtIndex(val_servers, i);
		if (isA_CFString(nameserver)) {
		    SCPrint(TRUE, f, CFSTR("nameserver %@\n"), nameserver);
		}
	    }
	}

	if (isA_CFArray(val_sortlist)) {
	    SCPrint(TRUE, f, CFSTR("sortlist"));
	    n = CFArrayGetCount(val_sortlist);
	    for (i = 0; i < n; i++) {
		CFStringRef	address;

		address = CFArrayGetValueAtIndex(val_sortlist, i);
		if (isA_CFString(address)) {
		    SCPrint(TRUE, f, CFSTR(" %@"), address);
		}
	    }
	    SCPrint(TRUE, f, CFSTR("\n"));
	}

	fclose(f);
	rename(VAR_RUN_RESOLV_CONF "-", VAR_RUN_RESOLV_CONF);
    }
    return;
}
#endif	/* !TARGET_OS_IPHONE */

static boolean_t
router_is_our_ipv6_address(CFStringRef router, CFArrayRef addr_list)
{
    CFIndex		i;
    CFIndex		n = CFArrayGetCount(addr_list);
    struct in6_addr	r;

    (void)cfstring_to_ip6(router, &r);
    for (i = 0; i < n; i++) {
	struct in6_addr	ip;

	if (cfstring_to_ip6(CFArrayGetValueAtIndex(addr_list, i), &ip)
	    && bcmp(&r, &ip, sizeof(r)) == 0) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static IPv4RouteListRef
service_dict_get_ipv4_routelist(CFDictionaryRef service_dict)
{
    CFDictionaryRef	dict;
    IPv4RouteListRef	routes = NULL;

    dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv4);

    routes = ipv4_dict_get_routelist(dict);
    return (routes);
}

typedef struct apply_ipv4_route_context {
    IPv4RouteListRef	old;
    IPv4RouteListRef	new;
    int			sockfd;
} apply_ipv4_route_context_t;

/* add/remove a router/32 subnet */
static int
ipv4_route_gateway(int sockfd, int cmd, char * ifn_p,
		   IPv4RouteRef def_route)
{
    struct in_addr		mask;

    mask.s_addr = htonl(INADDR_BROADCAST);
    return (ipv4_route(sockfd, cmd, def_route->ifa,
		       def_route->gateway, mask, ifn_p, def_route->ifindex,
		       def_route->ifa, def_route->flags));
}

/*
 * Function: apply_ipv4_route
 * Purpose:
 *   Callback function that adds/removes the specified route.
 */
static void
apply_ipv4_route(IPv4RouteListApplyCommand cmd, IPv4RouteRef route, void * arg)
{
    apply_ipv4_route_context_t *context = (apply_ipv4_route_context_t *)arg;
    char *			ifn_p;
    int				retval;

    ifn_p = route->ifname;
    switch (cmd) {
    case kIPv4RouteListAddRouteCommand:
	if ((route->flags & kRouteIsNotSubnetLocalFlag) != 0) {
	    retval = ipv4_route_gateway(context->sockfd, RTM_ADD,
					ifn_p, route);
	    if (retval == EEXIST) {
		/* delete and add again */
		(void)ipv4_route_gateway(context->sockfd, RTM_DELETE,
					 ifn_p, route);
		retval = ipv4_route_gateway(context->sockfd, RTM_ADD,
					    ifn_p, route);
	    }
	    if (retval != 0) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("IPMonitor apply_ipv4_route failed to add"
			    " %s/32 route, %s"),
		      inet_ntoa(route->gateway), strerror(retval));
	    }
	    else if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("Added IPv4 Route %s/32"),
		      inet_ntoa(route->gateway));
	    }
	}
	retval = ipv4_route(context->sockfd,
			    RTM_ADD, route->gateway,
			    route->dest, route->mask, ifn_p, route->ifindex,
			    route->ifa, route->flags);
	if (retval == EEXIST) {
	    /* delete and add again */
	    (void)ipv4_route(context->sockfd,
			     RTM_DELETE, route->gateway,
			     route->dest, route->mask, ifn_p, route->ifindex,
			     route->ifa, route->flags);
	    retval = ipv4_route(context->sockfd,
				RTM_ADD, route->gateway,
				route->dest, route->mask,
				ifn_p, route->ifindex,
				route->ifa, route->flags);
	}
	if (retval != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("IPMonitor apply_ipv4_route failed to add"
			" route, %s:"), strerror(retval));
	    IPv4RouteLog(route);
	}
	else if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("Added IPv4 route new[%d] = "),
		  route - context->new->list);
	    IPv4RouteLog(route);
	}
	break;
    case kIPv4RouteListRemoveRouteCommand:
	retval = ipv4_route(context->sockfd,
			    RTM_DELETE, route->gateway,
			    route->dest, route->mask, ifn_p, route->ifindex,
			    route->ifa, route->flags);
	if (retval != 0) {
	    if (retval != ESRCH) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("IPMonitor apply_ipv4_route failed to remove"
			    " route, %s: "), strerror(retval));
		IPv4RouteLog(route);
	    }
	}
	else if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("Removed IPv4 route old[%d] = "),
		  route - context->old->list);
	    IPv4RouteLog(route);
	}
	if ((route->flags & kRouteIsNotSubnetLocalFlag) != 0) {
	    retval = ipv4_route_gateway(context->sockfd, RTM_DELETE,
					ifn_p, route);
	    if (retval != 0) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("IPMonitor apply_ipv4_route failed to remove"
			    " %s/32 route, %s: "),
		      inet_ntoa(route->gateway), strerror(retval));
	    }
	    else if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("Removed IPv4 Route %s/32"),
		      inet_ntoa(route->gateway));
	    }
	}
	break;
    default:
	break;
    }
    return;
}

/*
 * Function: update_ipv4
 *
 * Purpose:
 *   Update the IPv4 configuration based on the latest information.
 *   Publish the State:/Network/Global/IPv4 information, and update the
 *   IPv4 routing table.  IPv4RouteListApply() invokes our callback,
 *   apply_ipv4_route(), to install/remove the routes.
 */
static void
update_ipv4(CFStringRef		primary,
	    IPv4RouteListRef	new_routelist,
	    keyChangeListRef	keys)
{
    apply_ipv4_route_context_t	context;

    if (keys != NULL) {
	if (new_routelist != NULL && primary != NULL) {
	    char *			ifn_p = NULL;
	    IPv4RouteRef		r;
	    CFMutableDictionaryRef	dict = NULL;

	    dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	    /* the first entry is the default route */
	    r = new_routelist->list;
	    if (r->gateway.s_addr != 0) {
		CFStringRef		router;

		router = CFStringCreateWithCString(NULL,
						   inet_ntoa(r->gateway),
						   kCFStringEncodingASCII);
		if (router != NULL) {
		    CFDictionarySetValue(dict, kSCPropNetIPv4Router, router);
		    CFRelease(router);
		}
	    }
	    if (r->ifname[0] != '\0') {
		ifn_p = r->ifname;
	    }
	    if (ifn_p != NULL) {
		CFStringRef		ifname_cf;

		ifname_cf = CFStringCreateWithCString(NULL,
						      ifn_p,
						      kCFStringEncodingASCII);
		if (ifname_cf != NULL) {
		    CFDictionarySetValue(dict,
					 kSCDynamicStorePropNetPrimaryInterface,
					 ifname_cf);
		    CFRelease(ifname_cf);
		}
	    }
	    CFDictionarySetValue(dict, kSCDynamicStorePropNetPrimaryService,
				 primary);
	    keyChangeListSetValue(keys, S_state_global_ipv4, dict);
	    CFRelease(dict);
	}
	else {
	    keyChangeListRemoveValue(keys, S_state_global_ipv4);
	}
    }

    bzero(&context, sizeof(context));
    context.sockfd = route_open_socket();
    if (context.sockfd != -1) {
	if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    if (S_ipv4_routelist == NULL) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("Old Routes = <none>"));
	    }
	    else {
		SCLog(TRUE, LOG_NOTICE, CFSTR("Old Routes = "));
		IPv4RouteListLog(S_ipv4_routelist);
	    }
	    if (new_routelist == NULL) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("New Routes = <none>"));
	    }
	    else {
		SCLog(TRUE, LOG_NOTICE, CFSTR("New Routes = "));
		IPv4RouteListLog(new_routelist);
	    }
	}
	context.old = S_ipv4_routelist;
	context.new = new_routelist;
	IPv4RouteListApply(S_ipv4_routelist, new_routelist,
			   &apply_ipv4_route, (void *)&context);
	if (new_routelist != NULL) {
	    (void)multicast_route_delete(context.sockfd);
	}
	else {
	    (void)multicast_route_add(context.sockfd);
	}
	close(context.sockfd);
    }
    if (S_ipv4_routelist != NULL) {
	free(S_ipv4_routelist);
    }
    S_ipv4_routelist = new_routelist;
    return;
}

static void
update_ipv6(CFDictionaryRef	services_info,
	    CFStringRef		primary,
	    keyChangeListRef	keys)
{
    CFDictionaryRef	ipv6_dict = NULL;

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    ipv6_dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv6);
	}
    }
    if (ipv6_dict != NULL) {
	CFArrayRef		addrs;
	CFMutableDictionaryRef	dict = NULL;
	CFStringRef		if_name = NULL;
	char			ifn[IFNAMSIZ] = { '\0' };
	char *			ifn_p = NULL;
	boolean_t		is_direct = FALSE;
	CFStringRef		val_router = NULL;

	dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	val_router = CFDictionaryGetValue(ipv6_dict, kSCPropNetIPv6Router);
	addrs = CFDictionaryGetValue(ipv6_dict,
				     kSCPropNetIPv6Addresses);
	if (val_router != NULL) {
	    /* no router if router is one of our IP addresses */
	    is_direct = router_is_our_ipv6_address(val_router, addrs);
	    CFDictionarySetValue(dict, kSCPropNetIPv6Router,
				 val_router);
	}
	else {
	    val_router = CFArrayGetValueAtIndex(addrs, 0);
	    is_direct = TRUE;
	}
	if_name = CFDictionaryGetValue(ipv6_dict, kSCPropInterfaceName);
	if (if_name) {
	    CFDictionarySetValue(dict,
				 kSCDynamicStorePropNetPrimaryInterface,
				 if_name);
	    if (CFStringGetCString(if_name, ifn, sizeof(ifn),
				   kCFStringEncodingASCII)) {
		ifn_p = ifn;
	    }
	}
	CFDictionarySetValue(dict, kSCDynamicStorePropNetPrimaryService,
			     primary);
	keyChangeListSetValue(keys, S_state_global_ipv6, dict);
	CFRelease(dict);

#ifdef RTF_IFSCOPE
	if (S_scopedroute_v6) {
	    set_ipv6_default_interface(ifn_p);
	} else
#endif /* RTF_IFSCOPE */
	{ /* route add default ... */
	    struct in6_addr	router;

	    (void)cfstring_to_ip6(val_router, &router);
	    set_ipv6_router(&router, ifn_p, is_direct);
	}
    }
    else {
	keyChangeListRemoveValue(keys, S_state_global_ipv6);
#ifdef RTF_IFSCOPE
	if (S_scopedroute_v6) {
	    set_ipv6_default_interface(NULL);
	} else
#endif /* RTF_IFSCOPE */
	{ /* route delete default ... */
	    set_ipv6_router(NULL, NULL, FALSE);
	}
    }
    return;
}

static void
update_dns(CFDictionaryRef	services_info,
	   CFStringRef		primary,
	   keyChangeListRef	keys)
{
    CFDictionaryRef	dict = NULL;

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    dict = CFDictionaryGetValue(service_dict, kSCEntNetDNS);
	}
    }
    if (dict == NULL) {
#if	!TARGET_OS_IPHONE
	empty_dns();
#endif	/* !TARGET_OS_IPHONE */
	keyChangeListRemoveValue(keys, S_state_global_dns);
    }
    else {
	CFMutableDictionaryRef	new_dict;

#if	!TARGET_OS_IPHONE
	set_dns(CFDictionaryGetValue(dict, kSCPropNetDNSSearchDomains),
		CFDictionaryGetValue(dict, kSCPropNetDNSDomainName),
		CFDictionaryGetValue(dict, kSCPropNetDNSServerAddresses),
		CFDictionaryGetValue(dict, kSCPropNetDNSSortList));
#endif	/* !TARGET_OS_IPHONE */
	new_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	CFDictionaryRemoveValue(new_dict, kSCPropInterfaceName);
	CFDictionaryRemoveValue(new_dict, kSCPropNetDNSSupplementalMatchDomains);
	CFDictionaryRemoveValue(new_dict, kSCPropNetDNSSupplementalMatchOrders);
	CFDictionaryRemoveValue(new_dict, DNS_CONFIGURATION_SCOPED_QUERY_KEY);
	keyChangeListSetValue(keys, S_state_global_dns, new_dict);
	CFRelease(new_dict);
    }
    return;
}

static void
update_dnsinfo(CFDictionaryRef	services_info,
	       CFStringRef	primary,
	       keyChangeListRef	keys,
	       CFArrayRef	service_order)
{
    Boolean		changed;
    CFDictionaryRef	dict	= NULL;
    CFArrayRef		multicastResolvers;
    CFArrayRef		privateResolvers;

    multicastResolvers = CFDictionaryGetValue(services_info, S_multicast_resolvers);
    privateResolvers   = CFDictionaryGetValue(services_info, S_private_resolvers);

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    dict = CFDictionaryGetValue(service_dict, kSCEntNetDNS);
	}
    }

    changed = dns_configuration_set(dict,
				    S_service_state_dict,
				    service_order,
				    multicastResolvers,
				    privateResolvers);
    if (changed) {
	keyChangeListNotifyKey(keys, S_state_global_dns);
    }
    return;
}

static void
update_proxies(CFDictionaryRef	services_info,
	       CFStringRef	primary,
	       keyChangeListRef	keys,
	       CFArrayRef	service_order)
{
    CFDictionaryRef dict	= NULL;
    CFDictionaryRef new_dict;

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    dict = CFDictionaryGetValue(service_dict, kSCEntNetProxies);
	}
    }

    new_dict = proxy_configuration_update(dict,
					  S_service_state_dict,
					  service_order);
    if (new_dict == NULL) {
	keyChangeListRemoveValue(keys, S_state_global_proxies);
    }
    else {
	keyChangeListSetValue(keys, S_state_global_proxies, new_dict);
	CFRelease(new_dict);
    }
    return;
}

#if	!TARGET_OS_IPHONE
static void
update_smb(CFDictionaryRef	services_info,
	   CFStringRef		primary,
	   keyChangeListRef	keys)
{
    CFDictionaryRef	dict	= NULL;

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    dict = CFDictionaryGetValue(service_dict, kSCEntNetSMB);
	}
    }
    if (dict == NULL) {
	keyChangeListRemoveValue(keys, S_state_global_smb);
    }
    else {
	keyChangeListSetValue(keys, S_state_global_smb, dict);
    }

    return;
}
#endif	/* !TARGET_OS_IPHONE */

static Rank
get_service_rank(CFArrayRef order, int n_order, CFStringRef serviceID)
{
    CFIndex		i;
    Rank		rank = kRankIndexMask;

    if (serviceID != NULL && order != NULL && n_order > 0) {
	for (i = 0; i < n_order; i++) {
	    CFStringRef s = isA_CFString(CFArrayGetValueAtIndex(order, i));

	    if (s == NULL) {
		continue;
	    }
	    if (CFEqual(serviceID, s)) {
		rank = i + 1;
		break;
	    }
	}
    }
    return (rank);
}

/**
 ** Service election:
 **/
/*
 * Function: rank_dict_get_service_rank
 * Purpose:
 *   Retrieve the service rank in the given dictionary.
 */
static Rank
rank_dict_get_service_rank(CFDictionaryRef rank_dict, CFStringRef serviceID)
{
    CFNumberRef		rank;
    Rank		rank_val = RankMake(kRankIndexMask, kRankAssertionDefault);

    rank = CFDictionaryGetValue(rank_dict, serviceID);
    if (rank != NULL) {
	CFNumberGetValue(rank, kCFNumberSInt32Type, &rank_val);
    }
    return (rank_val);
}

/*
 * Function: rank_dict_set_service_rank
 * Purpose:
 *   Save the results of ranking the service so we can look it up later without
 *   repeating all of the ranking code.
 */
static void
rank_dict_set_service_rank(CFMutableDictionaryRef rank_dict,
			   CFStringRef serviceID, Rank rank_val)
{
    CFNumberRef		rank;

    rank = CFNumberCreate(NULL, kCFNumberSInt32Type, (const void *)&rank_val);
    if (rank != NULL) {
	CFDictionarySetValue(rank_dict, serviceID, rank);
	CFRelease(rank);
    }
    return;
}

typedef struct election_info {
    int			n_services;
    CFArrayRef		order;
    int			n_order;
    CFStringRef		serviceID;
    CFDictionaryRef	service_dict;
    Rank		rank;
} election_info_t;

typedef boolean_t election_func_t(void * context, election_info_t * info);

/*
 * Function: elect_ipv4
 * Purpose:
 *   This function builds the list of IPv4 routes that should be active.
 *   As elect_new_primary() invokes us with each service, we build up the
 *   result in the passed in context, a pointer to an IPv4RouteListRef.
 */
static boolean_t
elect_ipv4(void * context, election_info_t * info)
{
    Rank		primary_rank;
    IPv4RouteListRef *	routes_p = (IPv4RouteListRef *)context;
    IPv4RouteListRef	service_routes;

    service_routes = service_dict_get_ipv4_routelist(info->service_dict);

    info->rank = get_service_rank(info->order, info->n_order,
				    info->serviceID);

    if (service_routes == NULL) {
	return (FALSE);
    }

    primary_rank = RANK_ASSERTION_MASK(service_routes->list->rank);

    if (S_ppp_override_primary
	     && (strncmp(PPP_PREFIX, service_routes->list->ifname,
			 sizeof(PPP_PREFIX) - 1) == 0)) {
	/* PPP override: make ppp* look the best */
	/* Hack: should use interface type, not interface name */
	primary_rank = kRankAssertionFirst;
    }

    info->rank = RankMake(info->rank, primary_rank);

    if (routes_p != NULL) {
	*routes_p = IPv4RouteListAddRouteList(*routes_p,
					      info->n_services * 3,
					      service_routes,
					      info->rank);
    }

    if (primary_rank == kRankAssertionNever) {
	/* never elect as primary */
	return (FALSE);
    }
    if (strncmp(service_routes->list->ifname, "lo0",
		sizeof(service_routes->list->ifname)) == 0) {
	/* never elect as primary */
	return (FALSE);
    }

    rank_dict_set_service_rank(S_ipv4_service_rank_dict,
			       info->serviceID, info->rank);
    return (TRUE);
}

static void
IPv6RankedListInsertE(IPv6RankedListRef list, IPv6RankedERef e)
{
    int	idx		    = 0;
    int elems_to_be_moved   = 0;

    if (list == NULL) return;

    for (idx = 0; idx < list->count; idx++) {
	if ((e->rank < list->elem[idx].rank)) {
	    break;
	}
    }

    if ((S_IPMonitor_debug & kDebugFlag2) != 0) {
	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("IPv6RankedListInsertE: %s flags 0x%x at index %d with rank %u."),
	      e->ifname, e->flags, idx, e->rank);
    }

    elems_to_be_moved = list->count - idx;
    if (elems_to_be_moved > 0) {
	bcopy((void*) &list->elem[idx], (void*) &list->elem[idx+1],
	      elems_to_be_moved * sizeof(*e));
    }

    bcopy((void*) e, (void*) &list->elem[idx], sizeof(*e));
    list->count++;
    return;
}

static boolean_t
elect_ipv6(void * context, election_info_t * info)
{
    CFStringRef		if_name;
    CFStringRef		primaryRankStr	= NULL;
    CFDictionaryRef	proto_dict;
    CFStringRef		router;
    CFDictionaryRef	service_options;
    IPv6RankedListRef	list = (IPv6RankedListRef) context;
    IPv6RankedE		elem;
    CFDictionaryRef	dns_dict = NULL;
    Rank		primary_rank = kRankAssertionDefault;

    memset((void *)&elem, 0, sizeof(elem));

    info->rank = get_service_rank(info->order, info->n_order, info->serviceID);

    proto_dict = CFDictionaryGetValue(info->service_dict, kSCEntNetIPv6);
    if (proto_dict == NULL) {
	return (FALSE);
    }
    if_name = CFDictionaryGetValue(proto_dict, kSCPropInterfaceName);
    if (if_name == NULL) {
	/* we need an interface name */
	return (FALSE);
    }
    if (CFEqual(if_name, CFSTR("lo0"))) {
	/* don't ever elect loopback */
	return (FALSE);
    }
    CFStringGetCString(if_name, elem.ifname,
		       sizeof(elem.ifname),
		       kCFStringEncodingASCII);
    router = CFDictionaryGetValue(proto_dict,
				  kSCPropNetIPv6Router);
    if (router == NULL) {
	/* can't become primary without a router */
	return (FALSE);
    }

    dns_dict = service_dict_get(info->serviceID, kSCEntNetDNS);
    if (dns_dict != NULL) {
	elem.flags |= NWI_IFSTATE_FLAGS_HAS_DNS;
    }

    service_options = service_dict_get(info->serviceID, kSCEntNetService);
    if (service_options != NULL) {
	primaryRankStr = CFDictionaryGetValue(service_options, kSCPropNetServicePrimaryRank);
	if (primaryRankStr != NULL) {
	    primary_rank = PrimaryRankGetRankAssertion(primaryRankStr);
	}
	if (primary_rank == kRankAssertionNever) {
	    elem.flags |= NWI_IFSTATE_FLAGS_NOT_IN_LIST;
	    elem.rank = RankMake(info->rank, primary_rank);
	    IPv6RankedListInsertE(list, &elem);
	    return (FALSE);
	}
    }

    if (get_override_primary(proto_dict)) {
	primary_rank = kRankAssertionFirst;
    }
    else if (S_ppp_override_primary
	     && CFStringHasPrefix(if_name, CFSTR(PPP_PREFIX))) {
	/* PPP override: make ppp* look the best */
	/* Hack: should use interface type, not interface name */
	primary_rank = kRankAssertionFirst;
    }

    elem.rank = RankMake(info->rank, primary_rank);
    info->rank = RankMake(info->rank, primary_rank);

    (void)cfstring_to_ip6(router, &elem.iaddr6);

    IPv6RankedListInsertE(list, &elem);
    rank_dict_set_service_rank(S_ipv6_service_rank_dict,
			       info->serviceID, info->rank);
    return (TRUE);
}

static void
update_nwi_state_ipv6(nwi_state_t state, IPv6RankedListRef list)
{
    int idx;

    if (state == NULL) {
	return;
    }

    /* A null list indicates to clear the ifstates */
    nwi_state_clear(state, AF_INET6);

    if (list == NULL) {
	return;
    }

    for (idx = 0 ; idx < list->count; idx++) {
	if ((S_IPMonitor_debug & kDebugFlag2) != 0) {
	    char	ntopbuf[INET6_ADDRSTRLEN];

	    inet_ntop(AF_INET6, &list->elem[idx].iaddr6, ntopbuf,
		      sizeof(ntopbuf));

	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("Inserting v6 [%s] into list with flags 0x%x"
			" with rank %lu ipv6 addr %s"),
			list->elem[idx].ifname, list->elem[idx].flags,
			list->elem[idx].rank, ntopbuf);
	}

	nwi_insert_ifstate(state, list->elem[idx].ifname,
			   AF_INET6, list->elem[idx].flags,
			   list->elem[idx].rank,
			   (void *)&list->elem[idx].iaddr6);
    }
    nwi_state_set_last(state, AF_INET6);
}

static boolean_t
interface_has_dns(const void * * values, int values_count,
		  const char * ifname)
{
    boolean_t		has_dns = FALSE;
    int			i;
    CFStringRef		ifname_cf;

    ifname_cf = CFStringCreateWithCString(NULL, ifname, kCFStringEncodingASCII);
    for (i = 0; i < values_count; i++) {
	CFDictionaryRef		dns_dict;

	dns_dict = CFDictionaryGetValue(values[i], kSCEntNetDNS);
	if (dns_dict != NULL) {
	    CFStringRef		this_ifname;

	    this_ifname = CFDictionaryGetValue(dns_dict, kSCPropInterfaceName);
	    if (this_ifname != NULL
		&& CFEqual(this_ifname, ifname_cf)) {
		has_dns = TRUE;
		break;
	    }
	}
    }
    CFRelease(ifname_cf);
    return (has_dns);
}

static void
update_nwi_state_ipv4(nwi_state_t state, IPv4RouteListRef route)
{
    int			idx;
    IPv4RouteRef	r;
    const void * *	values;
    const void *	values_buf[10];
    int			values_count;

    if (state == NULL) {
	return;
    }
    nwi_state_clear(state, AF_INET);
    if (route == NULL) {
	return;
    }
    values_count = CFDictionaryGetCount(S_service_state_dict);
    if (values_count == 0) {
	return;
    }
    if (values_count <= (sizeof(values_buf) / sizeof(values_buf[0]))) {
	values = values_buf;
    }
    else {
	values = (const void * *)malloc(sizeof(*values) * values_count);
    }
    CFDictionaryGetKeysAndValues(S_service_state_dict,
				 NULL, (const void * *)values);
    for (idx = 0, r = route->list; idx < route->count; idx++, r++)  {
	uint64_t		flags = 0ULL;

	if (r->dest.s_addr != 0) {
	    /* we're done, no more default routes */
	    break;
	}
	if (RANK_ASSERTION_MASK(r->rank) == kRankAssertionNever) {
	    flags |= NWI_IFSTATE_FLAGS_NOT_IN_LIST;
	}

	if (interface_has_dns(values, values_count, r->ifname)) {
	    flags |= NWI_IFSTATE_FLAGS_HAS_DNS;
	}
	if ((S_IPMonitor_debug & kDebugFlag2) != 0) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("Inserting v4 [%s] with flags 0x%x primary_rank %u"),
		  r->ifname, flags, r->rank);
	}
	nwi_insert_ifstate(state, r->ifname, AF_INET, flags, r->rank,
			   (void *)&r->ifa);
    }
    if (values != values_buf) {
	free(values);
    }
    nwi_state_set_last(state, AF_INET);
    return;
}

/*
 * Function: elect_new_primary
 * Purpose:
 *   Walk the list of services, passing each service dictionary to "elect_func".
 *   "elect_func" returns rank information about the service that let us
 *   determine the new primary.
 */
static CFStringRef
elect_new_primary(election_func_t * elect_func, void * context,
		  CFArrayRef order, int n_order)
{
    int			count;
    int			i;
    election_info_t	info;
    void * *		keys;
#define N_KEYS_VALUES_STATIC	10
    void *		keys_values_buf[N_KEYS_VALUES_STATIC * 2];
    CFStringRef		new_primary = NULL;
    Rank		new_rank = 0;
    void * *		values;

    count = CFDictionaryGetCount(S_service_state_dict);
    if (count <= N_KEYS_VALUES_STATIC) {
	keys = keys_values_buf;
    }
    else {
	keys = (void * *)malloc(sizeof(*keys) * count * 2);
    }
    values = keys + count;
    CFDictionaryGetKeysAndValues(S_service_state_dict,
				 (const void * *)keys,
				 (const void * *)values);

    info.n_services = count;
    info.order = order;
    info.n_order = n_order;
    new_rank = 0;
    for (i = 0; i < count; i++) {
	boolean_t	found_new_primary = FALSE;

	info.serviceID = (CFStringRef)keys[i];
	info.service_dict = (CFDictionaryRef)values[i];
	info.rank = kRankAssertionMask;

	if ((*elect_func)(context, &info) == FALSE) {
	    continue;
	}
	if (new_primary == NULL
	    || (info.rank < new_rank)) {
	    found_new_primary = TRUE;
	}

	if (found_new_primary) {
	    new_primary = info.serviceID;
	    new_rank = info.rank;
	}
    }
    if (new_primary != NULL) {
	CFRetain(new_primary);
    }
    if (keys != keys_values_buf) {
	free(keys);
    }
    return (new_primary);
}

static uint32_t
service_changed(CFDictionaryRef services_info, CFStringRef serviceID)
{
    uint32_t		changed = 0;
    int			i;

    /* update service options first (e.g. rank) */
    if (get_rank_changes(serviceID,
			 get_service_state_entity(services_info, serviceID,
						  NULL),
			 get_service_setup_entity(services_info, serviceID,
						  NULL),
			 services_info)) {
	changed |= (1 << kEntityTypeServiceOptions);
    }
    /* update IPv4, IPv6, DNS, Proxies, SMB, ... */
    for (i = 0; i < ENTITY_TYPES_COUNT; i++) {
	GetEntityChangesFuncRef func = entityChangeFunc[i];
	if ((*func)(serviceID,
		    get_service_state_entity(services_info, serviceID,
					     *entityTypeNames[i]),
		    get_service_setup_entity(services_info, serviceID,
					     *entityTypeNames[i]),
		    services_info)) {
	    changed |= (1 << i);
	}
    }
    return (changed);
}

static CFArrayRef
service_order_get(CFDictionaryRef services_info)
{
    CFArrayRef		order = NULL;
    CFDictionaryRef	ipv4_dict;

    ipv4_dict = my_CFDictionaryGetDictionary(services_info,
					     S_setup_global_ipv4);
    if (ipv4_dict != NULL) {
	CFNumberRef	ppp_override;
	int		ppp_val = 0;

	order = CFDictionaryGetValue(ipv4_dict, kSCPropNetServiceOrder);
	order = isA_CFArray(order);

	/* get ppp override primary */
	ppp_override = CFDictionaryGetValue(ipv4_dict,
					    kSCPropNetPPPOverridePrimary);
	ppp_override = isA_CFNumber(ppp_override);
	if (ppp_override != NULL) {
	    CFNumberGetValue(ppp_override, kCFNumberIntType, &ppp_val);
	}
	S_ppp_override_primary = (ppp_val != 0) ? TRUE : FALSE;
    }
    else {
	S_ppp_override_primary = FALSE;
    }
    return (order);
}

static boolean_t
set_new_primary(CFStringRef * primary_p, CFStringRef new_primary,
		const char * entity)
{
    boolean_t		changed = FALSE;
    CFStringRef		primary = *primary_p;

    if (new_primary != NULL) {
	if (primary != NULL && CFEqual(new_primary, primary)) {
	    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("IPMonitor: %@ is still primary %s"),
		      new_primary, entity);
	    }
	}
	else {
	    my_CFRelease(primary_p);
	    *primary_p = CFRetain(new_primary);
	    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		SCLog(TRUE, LOG_NOTICE,
		      CFSTR("IPMonitor: %@ is the new primary %s"),
		      new_primary, entity);
	    }
	    changed = TRUE;
	}
    }
    else if (primary != NULL) {
	if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("IPMonitor: %@ is no longer primary %s"),
		  primary, entity);
	}
	my_CFRelease(primary_p);
	changed = TRUE;
    }
    return (changed);
}

static Rank
rank_service_entity(CFDictionaryRef rank_dict, CFStringRef serviceID,
		    CFStringRef entity)
{
    if (service_dict_get(serviceID, entity) == NULL) {
	return (RankMake(kRankIndexMask, kRankAssertionDefault));
    }
    return (rank_dict_get_service_rank(rank_dict, serviceID));
}

static __inline__ unsigned int
IPv6RankedListComputeSize(int n)
{
	return (offsetof(IPv6RankedList, elem[n]));

}

static void
update_interface_rank(CFDictionaryRef services_info, CFStringRef ifname)
{
    CFStringRef		if_rank_key;
    CFDictionaryRef	rank_dict;

    if_rank_key = if_rank_key_copy(ifname);
    rank_dict = CFDictionaryGetValue(services_info, if_rank_key);
    CFRelease(if_rank_key);
    if_rank_set(ifname, rank_dict);
    return;
}

static void
append_serviceIDs_for_interface(CFMutableArrayRef services_changed,
				CFStringRef ifname)
{
    int			count;
    int			i;
    void * *		keys;
#define N_KEYS_VALUES_STATIC    10
    void *		keys_values_buf[N_KEYS_VALUES_STATIC * 2];
    void * *		values;

    count = CFDictionaryGetCount(S_service_state_dict);
    if (count <= N_KEYS_VALUES_STATIC) {
	keys = keys_values_buf;
    } else {
	keys = (void * *)malloc(sizeof(*keys) * count * 2);
    }
    values = keys + count;
    CFDictionaryGetKeysAndValues(S_service_state_dict,
				 (const void * *)keys,
				 (const void * *)values);

    for (i = 0; i < count; i++) {
	CFDictionaryRef		ipv4 = NULL;
	CFStringRef		interface = NULL;
	CFStringRef		serviceID;
	CFDictionaryRef		service_dict;

	serviceID = (CFStringRef)keys[i];
	service_dict = (CFDictionaryRef)values[i];

	/* check if this is a ipv4 dictionary */
	ipv4  = CFDictionaryGetValue(service_dict, kSCEntNetIPv4);
	if (ipv4 != NULL) {
	    interface = ipv4_dict_get_ifname(ipv4);
	    if (interface != NULL && CFEqual(interface, ifname)) {
		if (S_IPMonitor_debug & kDebugFlag1) {
		    SCLog(TRUE, LOG_NOTICE, CFSTR("Found ipv4 service %@ on interface %@."),
			  serviceID, ifname);
		}

		my_CFArrayAppendUniqueValue(services_changed, serviceID);
	    }
	} else {
	    CFDictionaryRef	proto_dict;

	    /* check if this is a ipv6 dictionary */
	    proto_dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv6);
	    if (proto_dict == NULL) {
		continue;
	    }
	    interface = CFDictionaryGetValue(proto_dict, kSCPropInterfaceName);
	    if (interface != NULL && CFEqual(interface, ifname)) {
		if (S_IPMonitor_debug & kDebugFlag1) {
		    SCLog(TRUE, LOG_NOTICE, CFSTR("Found ipv6 service %@ on interface %@."),
			  serviceID, ifname);
		}

		my_CFArrayAppendUniqueValue(services_changed, serviceID);
	    }
	}
    }

    if (keys != keys_values_buf) {
	free(keys);
    }
}

static const CFStringRef *statusEntityNames[] = {
    &kSCEntNetIPSec,
    &kSCEntNetPPP,
    &kSCEntNetVPN,
};

static void
add_status_keys(CFMutableArrayRef patterns)
{
    int	    i;

    for (i = 0; i < sizeof(statusEntityNames)/sizeof(statusEntityNames[0]); i++) {
	CFStringRef	pattern;

	pattern = state_service_key(kSCCompAnyRegex, *statusEntityNames[i]);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);
    }

    return;
}

static
inline
const char *
get_changed_str(CFStringRef serviceID, CFStringRef entity, CFDictionaryRef old_dict)
{
    CFDictionaryRef new_dict    = NULL;

    if (serviceID != NULL) {
	new_dict = service_dict_get(serviceID, entity);
    }

    if (old_dict == NULL) {
	if (new_dict != NULL) {
	    return "+";
	}
    } else {
	if (new_dict == NULL) {
	    return "-";
	} else if (!CFEqual(old_dict, new_dict)) {
	    return "!";
	}
    }
    return "";
}

static CF_RETURNS_RETAINED CFStringRef
generate_log_changes(nwi_state_t	changes_state,
		     boolean_t		dns_changed,
		     boolean_t		dnsinfo_changed,
		     CFDictionaryRef	old_primary_dns,
		     boolean_t		proxy_changed,
		     CFDictionaryRef	old_primary_proxy,
		     boolean_t		smb_changed,
		     CFDictionaryRef	old_primary_smb
		     )
{
    int idx;
    CFMutableStringRef log_output;
    nwi_ifstate_t scan;

    log_output = CFStringCreateMutable(NULL, 0);

    if (changes_state != NULL) {
	for (idx = 0; idx < sizeof(nwi_af_list)/sizeof(nwi_af_list[0]); idx++) {
	    CFMutableStringRef changes = NULL;
	    CFMutableStringRef primary_str = NULL;

	    scan = nwi_state_get_first_ifstate(changes_state, nwi_af_list[idx]);

	    while (scan != NULL) {
		const char * changed_str;

		changed_str = nwi_ifstate_get_diff_str(scan);
		if (changed_str != NULL) {
		    void *		address;
		    const char *	addr_str;
		    char		ntopbuf[INET6_ADDRSTRLEN];

		    address = (void *)nwi_ifstate_get_address(scan);
		    addr_str = inet_ntop(scan->af, address,
				ntopbuf, sizeof(ntopbuf));

		    if (primary_str ==  NULL) {
			primary_str = CFStringCreateMutable(NULL, 0);
			CFStringAppendFormat(primary_str, NULL, CFSTR("%s%s:%s"),
					     nwi_ifstate_get_ifname(scan),
					     changed_str, addr_str);
		    } else {
			if (changes == NULL) {
			    changes = CFStringCreateMutable(NULL, 0);
			}
			CFStringAppendFormat(changes, NULL, CFSTR(", %s"),
					     nwi_ifstate_get_ifname(scan));
			if (strcmp(changed_str,  "") != 0) {
			    CFStringAppendFormat(changes, NULL, CFSTR("%s:%s"),
						 changed_str, addr_str);
			}
		    }
		}
		scan = nwi_ifstate_get_next(scan, scan->af);
	    }

	    if (primary_str != NULL) {
		CFStringAppendFormat(log_output, NULL, CFSTR(" %s(%@"),
				     nwi_af_list[idx] == AF_INET ? "v4" : "v6",
				     primary_str);

		if (changes != NULL && CFStringGetLength(changes) != 0) {
		    CFStringAppendFormat(log_output, NULL, CFSTR("%@"),
					 changes);
		}
		CFStringAppendFormat(log_output, NULL, CFSTR(")"));

		my_CFRelease(&primary_str);
		my_CFRelease(&changes);
	    }
	}
    }

    if (dns_changed || dnsinfo_changed) {
	const char    *str;

	str = get_changed_str(S_primary_dns, kSCEntNetDNS, old_primary_dns);
	if ((strcmp(str, "") == 0) && dnsinfo_changed) {
	    str = "*";	    // dnsinfo change w/no change to primary
	}
	CFStringAppendFormat(log_output, NULL, CFSTR(" DNS%s"), str);
    } else if (S_primary_dns != NULL){
	CFStringAppendFormat(log_output, NULL, CFSTR(" DNS"));
    }

    if (proxy_changed) {
	const char    *str;

	str = get_changed_str(S_primary_proxies, kSCEntNetProxies, old_primary_proxy);
	CFStringAppendFormat(log_output, NULL, CFSTR(" Proxy%s"), str);
    } else if (S_primary_proxies != NULL) {
	CFStringAppendFormat(log_output, NULL, CFSTR(" Proxy"));
    }

#if	!TARGET_OS_IPHONE
    if (smb_changed) {
	const char    *str;

	str = get_changed_str(S_primary_smb, kSCEntNetSMB, old_primary_smb);
	CFStringAppendFormat(log_output, NULL, CFSTR(" SMB%s"), str);
    } else if (S_primary_smb != NULL) {
	CFStringAppendFormat(log_output, NULL, CFSTR(" SMB"));
    }
#endif	// !TARGET_OS_IPHONE

    return log_output;
}

static void
IPMonitorNotify(SCDynamicStoreRef session, CFArrayRef changed_keys,
		void * not_used)
{
    CFIndex		count;
    nwi_state_t		changes_state		= NULL;
    boolean_t		dns_changed		= FALSE;
    boolean_t		dnsinfo_changed		= FALSE;
    boolean_t		global_ipv4_changed	= FALSE;
    boolean_t		global_ipv6_changed	= FALSE;
    int			i;
    CFMutableArrayRef	if_rank_changes		= NULL;
    keyChangeList	keys;
    CFIndex		n;
    CFStringRef		network_change_msg;
    int			n_services;
    int			n_service_order		= 0;
    nwi_state_t		old_nwi_state		= NULL;
    CFDictionaryRef	old_primary_dns		= NULL;
    CFDictionaryRef	old_primary_proxy	= NULL;
#if	!TARGET_OS_IPHONE
    CFDictionaryRef	old_primary_smb		= NULL;
#endif	// !TARGET_OS_IPHONE
    boolean_t		proxies_changed		= FALSE;
    CFArrayRef		service_order;
    CFMutableArrayRef	service_changes		= NULL;
    CFDictionaryRef	services_info		= NULL;
#if	!TARGET_OS_IPHONE
    boolean_t		smb_changed		= FALSE;
#endif	// !TARGET_OS_IPHONE

    count = CFArrayGetCount(changed_keys);
    if (count == 0) {
	return;
    }

    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("IPMonitor: changes %@ (%d)"), changed_keys, count);
    }

    if (S_primary_dns != NULL) {
	old_primary_dns = service_dict_get(S_primary_dns, kSCEntNetDNS);
	if (old_primary_dns != NULL) {
	    old_primary_dns = CFDictionaryCreateCopy(NULL, old_primary_dns);
	}
    }

    if (S_primary_proxies != NULL) {
	old_primary_proxy = service_dict_get(S_primary_proxies, kSCEntNetProxies);
	if (old_primary_proxy != NULL) {
	    old_primary_proxy = CFDictionaryCreateCopy(NULL, old_primary_proxy);
	}
    }

#if	!TARGET_OS_IPHONE
    if (S_primary_smb != NULL) {
	old_primary_smb = service_dict_get(S_primary_smb, kSCEntNetSMB);
	if (old_primary_smb != NULL) {
	    old_primary_smb = CFDictionaryCreateCopy(NULL, old_primary_smb);
	}
    }
#endif	// !TARGET_OS_IPHONE

    keyChangeListInit(&keys);
    service_changes = CFArrayCreateMutable(NULL, 0,
					   &kCFTypeArrayCallBacks);

    for (i = 0; i < count; i++) {
	CFStringRef	change = CFArrayGetValueAtIndex(changed_keys, i);
	if (CFEqual(change, S_setup_global_ipv4)) {
	    global_ipv4_changed = TRUE;
	    global_ipv6_changed = TRUE;
	}
	else if (CFEqual(change, S_multicast_resolvers)) {
	    dnsinfo_changed = TRUE;
	}
	else if (CFEqual(change, S_private_resolvers)) {
	    dnsinfo_changed = TRUE;
	}
#if	!TARGET_OS_IPHONE
	else if (CFEqual(change, CFSTR(_PATH_RESOLVER_DIR))) {
	    dnsinfo_changed = TRUE;
	}
#endif	/* !TARGET_OS_IPHONE */
	else if (CFStringHasPrefix(change, S_state_service_prefix)) {
	    int		i;
	    boolean_t	status_changed = FALSE;

	    for (i = 0; i < sizeof(statusEntityNames)/sizeof(statusEntityNames[0]); i++) {
		if (CFStringHasSuffix(change, *statusEntityNames[i])) {
		    status_changed = TRUE;
		    dnsinfo_changed = TRUE;
		    break;
		}
	    }

	    if (!status_changed) {
		CFStringRef serviceID = parse_component(change,
							S_state_service_prefix);
		if (serviceID) {
		    my_CFArrayAppendUniqueValue(service_changes, serviceID);
		    CFRelease(serviceID);
		}
	    }
	}
	else if (CFStringHasPrefix(change, S_setup_service_prefix)) {
	    CFStringRef serviceID = parse_component(change,
						    S_setup_service_prefix);
	    if (serviceID) {
		my_CFArrayAppendUniqueValue(service_changes, serviceID);
		CFRelease(serviceID);
	    }
	}
	else if (CFStringHasSuffix(change, kSCEntNetService)) {
	    CFStringRef ifname = my_CFStringCopyComponent(change, CFSTR("/"), 3);

	    if (ifname != NULL) {
		if (if_rank_changes == NULL) {
		    if_rank_changes = CFArrayCreateMutable(NULL, 0,
							   &kCFTypeArrayCallBacks);
		}
		my_CFArrayAppendUniqueValue(if_rank_changes, ifname);
		CFRelease(ifname);
	    }
	}
    }

    /* determine which serviceIDs are impacted by the interface rank changes */
    if (if_rank_changes != NULL) {
	n = CFArrayGetCount(if_rank_changes);
	for (i = 0; i < n; i++) {
	    CFStringRef ifname = CFArrayGetValueAtIndex(if_rank_changes, i);

	    if (S_IPMonitor_debug & kDebugFlag1) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("Interface rank changed %@"),
		      ifname);
	    }
	    append_serviceIDs_for_interface(service_changes, ifname);
	}
    }

    /* grab a snapshot of everything we need */
    services_info = services_info_copy(session, service_changes,
				       if_rank_changes);
    service_order = service_order_get(services_info);
    if (service_order != NULL) {
	n_service_order = CFArrayGetCount(service_order);
	if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("IPMonitor: service_order %@ "), service_order);
	}
    }

    if (if_rank_changes != NULL) {
	for (i = 0; i < n; i++) {
	    CFStringRef ifname = CFArrayGetValueAtIndex(if_rank_changes, i);
	    update_interface_rank(services_info, ifname);
	}
    }

    n = CFArrayGetCount(service_changes);
    for (i = 0; i < n; i++) {
	uint32_t	changes;
	CFStringRef	serviceID;

	serviceID = CFArrayGetValueAtIndex(service_changes, i);
	changes = service_changed(services_info, serviceID);
	if ((changes & (1 << kEntityTypeServiceOptions)) != 0) {
	    /* if __Service__ (e.g. PrimaryRank) changed */
	    global_ipv4_changed = TRUE;
	    global_ipv6_changed = TRUE;
	}
	else {
	    if ((changes & (1 << kEntityTypeIPv4)) != 0) {
		global_ipv4_changed = TRUE;
		dnsinfo_changed = TRUE;
		proxies_changed = TRUE;
	    }
	    if ((changes & (1 << kEntityTypeIPv6)) != 0) {
		global_ipv6_changed = TRUE;
		dnsinfo_changed = TRUE;
		proxies_changed = TRUE;
	    }
	}
	if ((changes & (1 << kEntityTypeDNS)) != 0) {
	    if (S_primary_dns != NULL && CFEqual(S_primary_dns, serviceID)) {
		update_dns(services_info, serviceID, &keys);
	    }
	    dnsinfo_changed = TRUE;
	}
	if ((changes & (1 << kEntityTypeProxies)) != 0) {
	    proxies_changed = TRUE;
	}
#if	!TARGET_OS_IPHONE
	if ((changes & (1 << kEntityTypeSMB)) != 0) {
	    if (S_primary_smb != NULL && CFEqual(S_primary_smb, serviceID)) {
		update_smb(services_info, serviceID, &keys);
	    }
	}
#endif	/* !TARGET_OS_IPHONE */
    }

    /* ensure S_nwi_state can hold as many services as we have currently */
    n_services = CFDictionaryGetCount(S_service_state_dict);
    old_nwi_state = nwi_state_copy_priv(S_nwi_state);
    S_nwi_state = nwi_state_new(S_nwi_state, n_services);

    if (global_ipv4_changed || dnsinfo_changed) {
	IPv4RouteListRef	new_routelist = NULL;
	CFStringRef		new_primary;

	if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("IPMonitor: IPv4 service election"));
	}
	new_primary = elect_new_primary(&elect_ipv4, &new_routelist,
					service_order, n_service_order);
	update_nwi_state_ipv4(S_nwi_state, new_routelist);

	if (global_ipv4_changed) {
	    (void)set_new_primary(&S_primary_ipv4, new_primary, "IPv4");
	    update_ipv4(S_primary_ipv4, new_routelist, &keys);
	}
	else if (new_routelist != NULL) {
	    free(new_routelist);
	    new_routelist = NULL;
	}
	my_CFRelease(&new_primary);
    }
    if (global_ipv6_changed || dnsinfo_changed) {
	IPv6RankedListRef	list = NULL;
	CFStringRef		new_primary;
	int			size;

	size = (n_services != 0)
	    ? (sizeof(IPv6RankedList)
	       + IPv6RankedListComputeSize(n_services)) : 0;

	if (size != 0) {
	    list = malloc(size);
	}
	if (list != NULL) {
	    list->count = 0;
	    list->size = size;
	}

	if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
	    SCLog(TRUE, LOG_NOTICE,
		  CFSTR("IPMonitor: IPv6 service election"));
	}
	new_primary = elect_new_primary(&elect_ipv6, list,
					service_order, n_service_order);
	update_nwi_state_ipv6(S_nwi_state, list);

	if (global_ipv6_changed) {
	    (void)set_new_primary(&S_primary_ipv6, new_primary, "IPv6");
	    update_ipv6(services_info, S_primary_ipv6, &keys);
	}

	if (list != NULL) {
	    free(list);
	}
	my_CFRelease(&new_primary);
    }
    if (global_ipv4_changed || global_ipv6_changed) {
	CFStringRef	new_primary_dns	    = NULL;
	CFStringRef	new_primary_proxies = NULL;
#if	!TARGET_OS_IPHONE
	CFStringRef	new_primary_smb	    = NULL;
#endif	/* !TARGET_OS_IPHONE */

	if (S_primary_ipv4 != NULL && S_primary_ipv6 != NULL) {
	    /* decide between IPv4 and IPv6 */
	    if (rank_service_entity(S_ipv4_service_rank_dict,
				    S_primary_ipv4, kSCEntNetDNS)
		<= rank_service_entity(S_ipv6_service_rank_dict,
				       S_primary_ipv6, kSCEntNetDNS)) {
		new_primary_dns = S_primary_ipv4;
	    }
	    else {
		new_primary_dns = S_primary_ipv6;
	    }
	    if (rank_service_entity(S_ipv4_service_rank_dict,
				    S_primary_ipv4, kSCEntNetProxies)
		<= rank_service_entity(S_ipv6_service_rank_dict,
				       S_primary_ipv6, kSCEntNetProxies)) {
		new_primary_proxies = S_primary_ipv4;
	    }
	    else {
		new_primary_proxies = S_primary_ipv6;
	    }
#if	!TARGET_OS_IPHONE
	    if (rank_service_entity(S_ipv4_service_rank_dict,
				    S_primary_ipv4, kSCEntNetSMB)
		<= rank_service_entity(S_ipv6_service_rank_dict,
				       S_primary_ipv6, kSCEntNetSMB)) {
		new_primary_smb = S_primary_ipv4;
	    }
	    else {
		new_primary_smb = S_primary_ipv6;
	    }
#endif	/* !TARGET_OS_IPHONE */

	}
	else if (S_primary_ipv6 != NULL) {
	    new_primary_dns     = S_primary_ipv6;
	    new_primary_proxies = S_primary_ipv6;
#if	!TARGET_OS_IPHONE
	    new_primary_smb     = S_primary_ipv6;
#endif	/* !TARGET_OS_IPHONE */
	}
	else if (S_primary_ipv4 != NULL) {
	    new_primary_dns     = S_primary_ipv4;
	    new_primary_proxies = S_primary_ipv4;
#if	!TARGET_OS_IPHONE
	    new_primary_smb     = S_primary_ipv4;
#endif	/* !TARGET_OS_IPHONE */
	}

	if (set_new_primary(&S_primary_dns, new_primary_dns, "DNS")) {
	    update_dns(services_info, S_primary_dns, &keys);
	    dns_changed = TRUE;
	    dnsinfo_changed = TRUE;
	}
	if (set_new_primary(&S_primary_proxies, new_primary_proxies, "Proxies")) {
	    proxies_changed = TRUE;
	}
#if	!TARGET_OS_IPHONE
	if (set_new_primary(&S_primary_smb, new_primary_smb, "SMB")) {
	    update_smb(services_info, S_primary_smb, &keys);
	    smb_changed = TRUE;
	}
#endif	/* !TARGET_OS_IPHONE */
    }
    if (dnsinfo_changed || global_ipv4_changed || global_ipv6_changed) {
	if (S_nwi_state != NULL) {
	    S_nwi_state->generation_count = mach_absolute_time();
	    if ((S_IPMonitor_debug & kDebugFlag1) != 0) {
		syslog(LOG_NOTICE, "Updating network information");
		_nwi_state_dump(LOG_NOTICE, S_nwi_state);
	    }
	    if (_nwi_state_store(S_nwi_state) == FALSE) {
		SCLog(TRUE, LOG_ERR, CFSTR("Notifying nwi_state_store failed"));
	    }
	}
    }
    if (dnsinfo_changed) {
	update_dnsinfo(services_info, S_primary_dns, &keys, service_order);
    }
    if (proxies_changed ||
	(dnsinfo_changed && (G_supplemental_proxies_follow_dns != NULL) && CFBooleanGetValue(G_supplemental_proxies_follow_dns))
       ) {
	// if proxy change OR supplemental Proxies follow supplemental DNS
	update_proxies(services_info, S_primary_proxies, &keys, service_order);
	proxies_changed = TRUE;
    }
    my_CFRelease(&service_changes);
    my_CFRelease(&services_info);
    my_CFRelease(&if_rank_changes);
    changes_state = nwi_state_diff(old_nwi_state, S_nwi_state);
    if ((S_IPMonitor_debug & kDebugFlag2) != 0) {
	syslog(LOG_NOTICE, "network information diffs: ");
	_nwi_state_dump(LOG_NOTICE, changes_state);
    }
    network_change_msg =
	generate_log_changes(changes_state,
			     dns_changed,
			     dnsinfo_changed,
			     old_primary_dns,
			     proxies_changed,
			     old_primary_proxy,
#if	!TARGET_OS_IPHONE
			     smb_changed,
			     old_primary_smb
#else	// !TARGET_OS_IPHONE
			     FALSE,
			     NULL
#endif	// !TARGET_OS_IPHONE
			     );
    keyChangeListApplyToStore(&keys, session, network_change_msg);
    my_CFRelease(&old_primary_dns);
    my_CFRelease(&old_primary_proxy);
#if	!TARGET_OS_IPHONE
    my_CFRelease(&old_primary_smb);
#endif	// !TARGET_OS_IPHONE
    my_CFRelease(&network_change_msg);

    if (changes_state != NULL) {
	nwi_state_release(changes_state);
    }
    if (old_nwi_state != NULL) {
	nwi_state_release(old_nwi_state);
    }
    keyChangeListFree(&keys);
    return;
}

static void
ip_plugin_init()
{
    CFMutableArrayRef	keys = NULL;
    CFStringRef		pattern;
    CFMutableArrayRef	patterns = NULL;
    CFRunLoopSourceRef	rls = NULL;

    if (S_is_network_boot() != 0) {
	S_netboot = TRUE;
    }

#ifdef RTF_IFSCOPE
    if (S_is_scoped_routing_enabled() != 0) {
	S_scopedroute = TRUE;
    }

    if (S_is_scoped_v6_routing_enabled() != 0) {
	S_scopedroute_v6 = TRUE;
    }
#endif /* RTF_IFSCOPE */

    S_session = SCDynamicStoreCreate(NULL, CFSTR("IPMonitor"),
				   IPMonitorNotify, NULL);
    if (S_session == NULL) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("IPMonitor ip_plugin_init SCDynamicStoreCreate failed: %s"),
	      SCErrorString(SCError()));
	return;
    }
    S_state_global_ipv4
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetIPv4);
    S_state_global_ipv6
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetIPv6);
    S_state_global_dns
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetDNS);
    S_state_global_proxies
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetProxies);
#if	!TARGET_OS_IPHONE
    S_state_global_smb
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetSMB);
#endif	/* !TARGET_OS_IPHONE */
    S_setup_global_ipv4
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    S_state_service_prefix
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainState,
						      CFSTR(""),
						      NULL);
    S_setup_service_prefix
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      CFSTR(""),
						      NULL);
    S_service_state_dict
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);

    S_ipv4_service_rank_dict
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);

    S_ipv6_service_rank_dict
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);

    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* register for State: and Setup: per-service notifications */
    add_service_keys(kSCCompAnyRegex, keys, patterns);

    /* register for State: per-service PPP/VPN/IPSec status notifications */
    add_status_keys(patterns);

    /* register for interface rank notifications */
    pattern = if_rank_key_copy(kSCCompAnyRegex);
    CFArrayAppendValue(patterns, pattern);
    CFRelease(pattern);

    /* add notifier for ServiceOrder/PPPOverridePrimary changes for IPv4 */
    CFArrayAppendValue(keys, S_setup_global_ipv4);

    /* add notifier for multicast DNS configuration (Bonjour/.local) */
    S_multicast_resolvers = SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@"),
						    kSCDynamicStoreDomainState,
						    kSCCompNetwork,
						    CFSTR(kDNSServiceCompMulticastDNS));
    CFArrayAppendValue(keys, S_multicast_resolvers);

    /* add notifier for private DNS configuration (Back to My Mac) */
    S_private_resolvers = SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@"),
						  kSCDynamicStoreDomainState,
						  kSCCompNetwork,
						  CFSTR(kDNSServiceCompPrivateDNS));
    CFArrayAppendValue(keys, S_private_resolvers);

    if (!SCDynamicStoreSetNotificationKeys(S_session, keys, patterns)) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("IPMonitor ip_plugin_init "
		    "SCDynamicStoreSetNotificationKeys failed: %s"),
	      SCErrorString(SCError()));
	goto done;
    }

    rls = SCDynamicStoreCreateRunLoopSource(NULL, S_session, 0);
    if (rls == NULL) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("IPMonitor ip_plugin_init "
		    "SCDynamicStoreCreateRunLoopSource failed: %s"),
	      SCErrorString(SCError()));
	goto done;
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    /* initialize dns configuration */
    (void)dns_configuration_set(NULL, NULL, NULL, NULL, NULL);
#if	!TARGET_OS_IPHONE
    empty_dns();
#endif	/* !TARGET_OS_IPHONE */
    (void)SCDynamicStoreRemoveValue(S_session, S_state_global_dns);

#if	!TARGET_OS_IPHONE
    /* initialize SMB configuration */
    (void)SCDynamicStoreRemoveValue(S_session, S_state_global_smb);
#endif	/* !TARGET_OS_IPHONE */

    if_rank_dict_init();

  done:
    my_CFRelease(&keys);
    my_CFRelease(&patterns);
    return;
}

__private_extern__
void
prime_IPMonitor()
{
    /* initialize multicast route */
    update_ipv4(NULL, NULL, NULL);
    return;
}

static boolean_t
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef key,
		    boolean_t def)
{
    CFBooleanRef	b;
    boolean_t		ret = def;

    b = isA_CFBoolean(CFDictionaryGetValue(plist, key));
    if (b != NULL) {
	ret = CFBooleanGetValue(b);
    }
    return (ret);
}

__private_extern__
void
load_IPMonitor(CFBundleRef bundle, Boolean bundleVerbose)
{
    CFDictionaryRef	info_dict;

    info_dict = CFBundleGetInfoDictionary(bundle);
    if (info_dict != NULL) {
	S_append_state
	    = S_get_plist_boolean(info_dict,
				  CFSTR("AppendStateArrayToSetupArray"),
				  FALSE);
    }
    if (bundleVerbose) {
	S_IPMonitor_debug = kDebugFlagDefault;
    }

    dns_configuration_init(bundle);
    proxy_configuration_init(bundle);
    ip_plugin_init();

#if	!TARGET_OS_IPHONE
    if (S_session != NULL) {
	dns_configuration_monitor(S_session, IPMonitorNotify);
    }
#endif	/* !TARGET_OS_IPHONE */

    load_hostname((S_IPMonitor_debug & kDebugFlag1) != 0);
#if	!TARGET_OS_IPHONE
    load_smb_configuration((S_IPMonitor_debug & kDebugFlag1) != 0);
#endif	/* !TARGET_OS_IPHONE */

    return;
}


#pragma mark -
#pragma mark Standalone test code


#ifdef TEST_IPMONITOR
#include "dns-configuration.c"
#include "set-hostname.c"

int
main(int argc, char **argv)
{
    _sc_log     = FALSE;

    S_IPMonitor_debug = kDebugFlag1;
    if (argc > 1) {
	S_IPMonitor_debug = strtoul(argv[1], NULL, 0);
    }

    load_IPMonitor(CFBundleGetMainBundle(), FALSE);
    prime_IPMonitor();
    CFRunLoopRun();
    /* not reached */
    exit(0);
    return 0;
}
#endif /* TEST_IPMONITOR */

#ifdef TEST_IPV4_ROUTELIST
#include "dns-configuration.c"
#include "set-hostname.c"

struct ipv4_service_contents {
    const char *	addr;
    const char *	mask;
    const char *	dest;
    const char *	router;
    const char *	ifname;
    Rank		rank;
    const CFStringRef	*primaryRank;
};

/*
 *  addr	mask		dest		router		ifname	pri  primaryRank
 */
struct ipv4_service_contents en0_10 = {
    "10.0.0.10", "255.255.255.0", NULL,		"10.0.0.1",	"en0",	10,  NULL
};

struct ipv4_service_contents en0_15 = {
    "10.0.0.19", "255.255.255.0", NULL,		"10.0.0.1",	"en0",	15,  NULL
};

struct ipv4_service_contents en0_30 = {
    "10.0.0.11", "255.255.255.0", NULL,		"10.0.0.1",	"en0",	30,  NULL
};

struct ipv4_service_contents en0_40 = {
    "10.0.0.12", "255.255.255.0", NULL,		"10.0.0.1",	"en0",	40,  NULL
};

struct ipv4_service_contents en0_50 = {
    "10.0.0.13", "255.255.255.0", NULL,		"10.0.0.1",	"en0",	50,  NULL
};

struct ipv4_service_contents en0_110 = {
    "192.168.2.10", "255.255.255.0", NULL,	"192.168.2.1",	"en0",	110, NULL
};

struct ipv4_service_contents en0_1 = {
    "17.202.40.191", "255.255.252.0", NULL,	"17.202.20.1",	"en0",	1,   NULL
};

struct ipv4_service_contents en1_20 = {
    "10.0.0.20", "255.255.255.0", NULL,		"10.0.0.1",	"en1",	20,  NULL
};

struct ipv4_service_contents en1_2 = {
    "17.202.42.24", "255.255.252.0", NULL,	"17.202.20.1",	"en1",	2,   NULL
};

struct ipv4_service_contents en1_125 = {
    "192.168.2.20", "255.255.255.0", NULL,	"192.168.2.1",	"en1",	125, NULL
};

struct ipv4_service_contents fw0_25 = {
    "192.168.2.30", "255.255.255.0", NULL,	"192.168.2.1",	"fw0",	25,  NULL
};

struct ipv4_service_contents fw0_21 = {
    "192.168.3.30", "255.255.255.0", NULL,	"192.168.3.1",	"fw0",	21,  NULL
};

struct ipv4_service_contents ppp0_0_1 = {
    "17.219.156.22", NULL, "17.219.156.1",	"17.219.156.1",	"ppp0", 0,   NULL
};

struct ipv4_service_contents en0_test6 = {
    "17.202.42.113",  "255.255.252.0", NULL,	"17.202.40.1",	"en0",  2,   NULL
};
struct ipv4_service_contents en1_test6 = {
    "17.202.42.111",  "255.255.252.0", NULL,	"17.202.40.1",	"en1",  3,   NULL
};
struct ipv4_service_contents en2_test6 = {
    "17.255.98.164",  "255.255.240.0", NULL,	"17.255.96.1",	"en2",  1,   NULL
};

struct ipv4_service_contents en0_test7 = {
    "17.202.42.113",  "255.255.252.0", NULL,	"17.202.40.1",	"en0",  3,   NULL
};
struct ipv4_service_contents en1_test7 = {
    "17.202.42.111",  "255.255.252.0", NULL,	"17.202.40.1",	"en1",  2,   NULL
};
struct ipv4_service_contents en2_test7 = {
    "17.255.98.164",  "255.255.240.0", NULL,	"17.255.96.1",	"en2",  1,   NULL
};
struct ipv4_service_contents fw0_test6_and_7 = {
    "169.254.11.33",  "255.255.0.0", NULL,	NULL,		"fw0", 0x0ffffff,  NULL
};

struct ipv4_service_contents en0_10_last = {
    "10.0.0.10", "255.255.255.0", NULL,		"10.0.0.1",	"en0",	10,  &kSCValNetServicePrimaryRankLast
};

struct ipv4_service_contents en0_10_never = {
    "10.0.0.10", "255.255.255.0", NULL,		"10.0.0.1",	"en0",	10,  &kSCValNetServicePrimaryRankNever
};

struct ipv4_service_contents en1_20_first = {
    "10.0.0.20", "255.255.255.0", NULL,		"10.0.0.1",	"en1",	20,  &kSCValNetServicePrimaryRankFirst
};

struct ipv4_service_contents en1_20_never = {
    "10.0.0.20", "255.255.255.0", NULL,		"10.0.0.1",	"en1",	20,  &kSCValNetServicePrimaryRankNever
};

struct ipv4_service_contents * test1[] = {
    &en0_40,
    &en0_15,
    &fw0_25,
    &en0_30,
    &en1_20,
    &en0_50,
    &en0_10,
    NULL
};

struct ipv4_service_contents * test2[] = {
    &en0_40,
    &fw0_25,
    &en0_30,
    &en1_20,
    &en0_50,
    &en0_10,
    NULL
};

struct ipv4_service_contents * test3[] = {
    &en0_40,
    &en1_20,
    &en0_50,
    &en0_10,
    &en0_110,
    &en1_125,
    &fw0_25,
    &fw0_21,
    &en0_40,
    &en0_30,
    NULL
};

struct ipv4_service_contents * test4[] = {
    &en0_1,
    &en0_40,
    &en0_30,
    &en1_20,
    &en1_2,
    NULL
};

struct ipv4_service_contents * test5[] = {
    &ppp0_0_1,
    &en0_1,
    &en0_40,
    &en0_30,
    &en1_20,
    &en1_2,
    NULL
};

struct ipv4_service_contents * test6[] = {
    &en0_test6,
    &en1_test6,
    &en2_test6,
    &fw0_test6_and_7,
    NULL
};

struct ipv4_service_contents * test7[] = {
    &en0_test7,
    &en1_test7,
    &en2_test7,
    &fw0_test6_and_7,
    NULL
};

struct ipv4_service_contents * test8[] = {
    &en0_10,
    &en1_20,
    NULL
};

struct ipv4_service_contents * test9[] = {
    &en0_10,
    &en1_20_first,
    &fw0_25,
    NULL
};

struct ipv4_service_contents * test10[] = {
    &en0_10_last,
    &en1_20,
    &fw0_25,
    NULL
};

struct ipv4_service_contents * test11[] = {
    &en0_10_never,
    &en1_20,
    &fw0_25,
    NULL
};

struct ipv4_service_contents * test12[] = {
    &en0_10,
    &en1_20,
    NULL
};

struct ipv4_service_contents * test13[] = {
    &en0_10,
    &en1_20_never,
    NULL
};

struct ipv4_service_contents * test14[] = {
    &en1_20_never,
    NULL
};

static void
dict_add_string(CFMutableDictionaryRef dict, CFStringRef prop_name,
		const char * str)
{
    CFStringRef		prop_val;

    if (str == NULL) {
	return;
    }
    prop_val = CFStringCreateWithCString(NULL,
					 str,
					 kCFStringEncodingASCII);
    CFDictionarySetValue(dict, prop_name, prop_val);
    CFRelease(prop_val);
    return;
}

static void
dict_add_string_as_array(CFMutableDictionaryRef dict, CFStringRef prop_name,
			 const char * str)
{
    CFArrayRef		array;
    CFStringRef		prop_val;

    if (str == NULL) {
	return;
    }
    prop_val = CFStringCreateWithCString(NULL,
					 str,
					 kCFStringEncodingASCII);
    array = CFArrayCreate(NULL,
			  (const void **)&prop_val, 1,
			  &kCFTypeArrayCallBacks);
    CFRelease(prop_val);
    CFDictionarySetValue(dict, prop_name, array);
    CFRelease(array);
    return;
}

static CFDictionaryRef
make_IPv4_dict(struct ipv4_service_contents * t)
{
    CFMutableDictionaryRef	dict;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    dict_add_string_as_array(dict, kSCPropNetIPv4Addresses, t->addr);
    dict_add_string_as_array(dict, kSCPropNetIPv4SubnetMasks, t->mask);
    dict_add_string_as_array(dict, kSCPropNetIPv4DestAddresses, t->dest);
    dict_add_string(dict, kSCPropNetIPv4Router, t->router);
    dict_add_string(dict, kSCPropInterfaceName, t->ifname);
    return (dict);
}

static IPv4RouteListRef
make_IPv4RouteList(struct ipv4_service_contents * * this_test)
{
    IPv4RouteListRef		r;
    IPv4RouteListRef		routes;
    char			routes_buf[IPv4RouteListComputeSize(R_STATIC)];
    IPv4RouteListRef		ret = NULL;
    struct ipv4_service_contents * *	scan_test;

    for (scan_test = this_test; *scan_test != NULL; scan_test++) {
	CFDictionaryRef		dict;

	dict = make_IPv4_dict(*scan_test);
	if (dict == NULL) {
	    fprintf(stderr, "make_IPv4_dict failed\n");
	    exit(1);
	}
	routes = (IPv4RouteListRef)routes_buf;
	routes->size = R_STATIC;
	routes->count = 0;
	r = IPv4RouteListCreateWithDictionary(routes, dict,
		(*scan_test)->primaryRank ? *(*scan_test)->primaryRank : NULL);
	if (r == NULL) {
	    fprintf(stderr, "IPv4RouteListCreateWithDictionary failed\n");
	    exit(1);
	}

	(*scan_test)->rank = RankMake((*scan_test)->rank, kRankAssertionDefault);

	if ((*scan_test)->primaryRank != NULL) {
	    (*scan_test)->rank = RankMake((*scan_test)->rank,
					     PrimaryRankGetRankAssertion(*(*scan_test)->primaryRank));
	}

	if ((*scan_test)->router == NULL) {
	    (*scan_test)->rank = RankMake((*scan_test)->rank,
					    PrimaryRankGetRankAssertion(kSCValNetServicePrimaryRankLast));
	}

	ret = IPv4RouteListAddRouteList(ret, 1, r, (*scan_test)->rank);
	if (r != routes) {
	    free(r);
	}
	CFRelease(dict);
    }
    return (ret);
}

/*
 * Function: run_test
 * Purpose:
 *   Runs through the given set of routes first in the forward direction,
 *   then again backwards.  We should end up with exactly the same set of
 *   routes at the end.
 */
static boolean_t
run_test(const char * name, struct ipv4_service_contents * * this_test)
{
    CFStringRef			descr;
    boolean_t			ret = FALSE;
    IPv4RouteListRef		r;
    IPv4RouteListRef		routes;
    char			routes_buf[IPv4RouteListComputeSize(R_STATIC)];
    IPv4RouteListRef		routes1 = NULL, routes2 = NULL;
    struct ipv4_service_contents * *	scan_test;

    printf("\nStarting test %s\n", name);
    for (scan_test = this_test; *scan_test != NULL; scan_test++) {
	CFDictionaryRef		dict;

	dict = make_IPv4_dict(*scan_test);
	if (dict == NULL) {
	    fprintf(stderr, "make_IPv4_dict failed\n");
	    exit(1);
	}
	routes = (IPv4RouteListRef)routes_buf;
	routes->size = R_STATIC;
	routes->count = 0;
	r = IPv4RouteListCreateWithDictionary(routes, dict,
		(*scan_test)->primaryRank ? *(*scan_test)->primaryRank : NULL);
	if (r == NULL) {
	    fprintf(stderr, "IPv4RouteListCreateWithDictionary failed\n");
	    exit(1);
	}
	if ((S_IPMonitor_debug & kDebugFlag4) != 0) {
	    descr = IPv4RouteListCopyDescription(r);
	    SCLog(TRUE, LOG_NOTICE, CFSTR("test: Adding %@"), descr);
	    CFRelease(descr);
	}

	(*scan_test)->rank = RankMake((*scan_test)->rank, kRankAssertionDefault);

	if ((*scan_test)->primaryRank != NULL) {
	    (*scan_test)->rank = RankMake((*scan_test)->rank,
					    PrimaryRankGetRankAssertion(*(*scan_test)->primaryRank));
	}

	routes1 = IPv4RouteListAddRouteList(routes1, 1, r, (*scan_test)->rank);
	if (r != routes) {
	    free(r);
	}
	CFRelease(dict);
    }
    if ((S_IPMonitor_debug & kDebugFlag4) != 0) {
	if (routes1 != NULL) {
	    descr = IPv4RouteListCopyDescription(routes1);
	    SCLog(TRUE, LOG_NOTICE, CFSTR("Routes are %@"), descr);
	    CFRelease(descr);
	}
    }
    for (scan_test--; scan_test >= this_test; scan_test--) {
	CFDictionaryRef		dict;

	dict = make_IPv4_dict(*scan_test);
	if (dict == NULL) {
	    fprintf(stderr, "make_IPv4_dict failed\n");
	    exit(1);
	}
	routes = (IPv4RouteListRef)routes_buf;
	routes->size = R_STATIC;
	routes->count = 0;
	r = IPv4RouteListCreateWithDictionary(routes, dict,
		(*scan_test)->primaryRank ? *(*scan_test)->primaryRank : NULL);
	if (r == NULL) {
	    fprintf(stderr, "IPv4RouteListCreateWithDictionary failed\n");
	    exit(1);
	}
	if ((S_IPMonitor_debug & kDebugFlag4) != 0) {
	    descr = IPv4RouteListCopyDescription(r);
	    SCLog(TRUE, LOG_NOTICE, CFSTR("test: Adding %@"), descr);
	    CFRelease(descr);
	}
	if ((*scan_test)->primaryRank != NULL) {
	    (*scan_test)->rank = RankMake((*scan_test)->rank,
					    PrimaryRankGetRankAssertion(*(*scan_test)->primaryRank));
	}

	routes2 = IPv4RouteListAddRouteList(routes2, 1, r, (*scan_test)->rank);
	if (r != routes) {
	    free(r);
	}
	CFRelease(dict);
    }
    if ((S_IPMonitor_debug & kDebugFlag4) != 0) {
	if (routes2 != NULL) {
	    descr = IPv4RouteListCopyDescription(routes2);
	    SCLog(TRUE, LOG_NOTICE, CFSTR("Routes are %@"), descr);
	    CFRelease(descr);
	}
    }
    if ((routes1 != NULL && routes2 == NULL)
	|| (routes1 == NULL && routes2 != NULL)) {
	fprintf(stderr, "routes1 is %sNULL but routes2 is %sNULL\n",
	       (routes1 != NULL) ? "not " : "",
	       (routes2 != NULL) ? "not " : "");
    }
    else if (routes1 != NULL && routes2 != NULL) {
	/* check if they are different */
	if (routes1->count != routes2->count) {
	    fprintf(stderr, "routes1 count %d != routes 2 count %d\n",
		    routes1->count, routes2->count);
	}
	else if (bcmp(routes1, routes2,
		      IPv4RouteListComputeSize(routes1->count)) != 0) {
	    fprintf(stderr, "routes1 and routes2 are different\n");
	}
	else {
	    printf("routes1 and routes2 are the same\n");
	    ret = TRUE;
	}
    }
    if (routes1 != NULL) {
	free(routes1);
    }
    if (routes2 != NULL) {
	free(routes2);
    }
    return (ret);
}

typedef struct compare_context {
    IPv4RouteListRef	old;
    IPv4RouteListRef	new;
} compare_context_t;

static void
compare_callback(IPv4RouteListApplyCommand cmd, IPv4RouteRef route, void * arg)
{
    compare_context_t *	context = (compare_context_t *)arg;

    switch (cmd) {
    case kIPv4RouteListAddRouteCommand:
	printf("Add new[%ld] = ", route - context->new->list);
	IPv4RoutePrint(route);
	break;
    case kIPv4RouteListRemoveRouteCommand:
	printf("Remove old[%ld] = ", route - context->old->list);
	IPv4RoutePrint(route);
	break;
    default:
	break;
    }
    return;
}

static void
compare_tests(struct ipv4_service_contents * * old_test,
	      struct ipv4_service_contents * * new_test)
{
    IPv4RouteListRef	new_routes;
    IPv4RouteListRef	old_routes;
    compare_context_t	context;

    old_routes = make_IPv4RouteList(old_test);
    new_routes = make_IPv4RouteList(new_test);

    if (old_routes == NULL) {
	printf("No Old Routes\n");
    }
    else {
	printf("Old Routes = ");
	IPv4RouteListPrint(old_routes);
    }
    if (new_routes == NULL) {
	printf("No New Routes\n");
    }
    else {
	printf("New Routes = ");
	IPv4RouteListPrint(new_routes);
    }
    context.old = old_routes;
    context.new = new_routes;
    IPv4RouteListApply(old_routes, new_routes, compare_callback, &context);
    if (old_routes != NULL) {
	free(old_routes);
    }
    if (new_routes != NULL) {
	free(new_routes);
    }

    return;
}

int
main(int argc, char **argv)
{
    _sc_log     = FALSE;
    _sc_verbose = (argc > 1) ? TRUE : FALSE;

    S_IPMonitor_debug = kDebugFlag1 | kDebugFlag2 | kDebugFlag4;
    if (argc > 1) {
	S_IPMonitor_debug = strtoul(argv[1], NULL, 0);
    }

    if (run_test("test1", test1) == FALSE) {
	fprintf(stderr, "test1 failed\n");
	exit(1);
    }
    if (run_test("test2", test2) == FALSE) {
	fprintf(stderr, "test2 failed\n");
	exit(1);
    }
    if (run_test("test3", test4) == FALSE) {
	fprintf(stderr, "test3 failed\n");
	exit(1);
    }
    if (run_test("test4", test4) == FALSE) {
	fprintf(stderr, "test4 failed\n");
	exit(1);
    }
    if (run_test("test5", test5) == FALSE) {
	fprintf(stderr, "test5 failed\n");
	exit(1);
    }

    printf("\nCompare 1 to 2:\n");
    compare_tests(test1, test2);

    printf("\nCompare 2 to 1:\n");
    compare_tests(test2, test1);

    printf("\nCompare 1 to 1:\n");
    compare_tests(test1, test1);

    printf("\nCompare 1 to 3:\n");
    compare_tests(test1, test3);

    printf("\nCompare 3 to 1:\n");
    compare_tests(test3, test1);

    printf("\nCompare 2 to 3:\n");
    compare_tests(test2, test3);

    printf("\nCompare 3 to 2:\n");
    compare_tests(test3, test2);

    printf("\nCompare 3 to 4:\n");
    compare_tests(test3, test4);

    printf("\nCompare 5 to 4:\n");
    compare_tests(test5, test4);

    printf("\nCompare 6 to 7:\n");
    compare_tests(test6, test7);

    printf("\nCompare 7 to 6:\n");
    compare_tests(test7, test6);

    printf("\nCompare 8 to 9:\n");
    compare_tests(test8, test9);

    printf("\nCompare 8 to 10:\n");
    compare_tests(test8, test10);

    printf("\nCompare 8 to 11:\n");
    compare_tests(test8, test11);

    printf("\nCompare 12 to 13:\n");
    compare_tests(test12, test13);

    printf("\nCompare 13 to 14:\n");
    compare_tests(test13, test14);

    printf("\nChecking for leaks\n");
    char    cmd[128];
    sprintf(cmd, "leaks %d 2>&1", getpid());
    fflush(stdout);
    (void)system(cmd);

    exit(0);
    return (0);
}

#endif /* TEST_IPV4_ROUTELIST */

