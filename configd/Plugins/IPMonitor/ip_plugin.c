/*
 * Copyright (c) 2000-2007 Apple Inc.  All Rights Reserved.
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
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>
#include <limits.h>
#include <notify.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>	/* for SCLog() */
#include <dnsinfo.h>

#include <dns_sd.h>
#ifndef	kDNSServiceCompPrivateDNS
#define	kDNSServiceCompPrivateDNS	"PrivateDNS"
#endif

#include "set-hostname.h"
#include "dns-configuration.h"
#include "smb-configuration.h"

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

/* debug output on/off */
static boolean_t		S_IPMonitor_debug = FALSE;

/* are we netbooted?  If so, don't touch the default route */
static boolean_t		S_netboot = FALSE;

/* dictionary to hold per-service state: key is the serviceID */
static CFMutableDictionaryRef	S_service_state_dict = NULL;

/* if set, a PPP interface overrides the primary */
static boolean_t		S_ppp_override_primary = FALSE;

/* the current primary serviceID's */
static CFStringRef		S_primary_ipv4 = NULL;
static CFStringRef		S_primary_ipv6 = NULL;
static CFStringRef		S_primary_dns = NULL;
static CFStringRef		S_primary_proxies = NULL;
static CFStringRef		S_primary_smb = NULL;

static CFStringRef		S_state_global_ipv4 = NULL;
static CFStringRef		S_state_global_ipv6 = NULL;
static CFStringRef		S_state_global_dns = NULL;
static CFStringRef		S_state_global_proxies = NULL;
static CFStringRef		S_state_global_smb = NULL;
static CFStringRef		S_state_service_prefix = NULL;
static CFStringRef		S_setup_global_ipv4 = NULL;
static CFStringRef		S_setup_global_proxies = NULL;
static CFStringRef		S_setup_global_smb = NULL;
static CFStringRef		S_setup_service_prefix = NULL;

static CFStringRef		S_private_resolvers = NULL;

static struct in_addr		S_router_subnet = { 0 };
static struct in_addr		S_router_subnet_mask = { 0 };

static const struct in_addr	S_ip_zeros = { 0 };
static const struct in6_addr	S_ip6_zeros = IN6ADDR_ANY_INIT;


#define kRouterNeedsLocalIP	CFSTR("com.apple.IPMonitor.RouterNeedsLocalIP")
#define kRouterIsDirect		CFSTR("com.apple.IPMonitor.IsDirect")

#define VAR_RUN_RESOLV_CONF	"/var/run/resolv.conf"

#ifndef KERN_NETBOOT
#define KERN_NETBOOT		40	/* int: are we netbooted? 1=yes,0=no */
#endif KERN_NETBOOT

/**
 ** entityType*, GetEntityChanges*
 ** - definitions for the entity types we handle
 **/
#define ENTITY_TYPES_COUNT	5
enum {
    kEntityTypeIPv4	= 0,
    kEntityTypeIPv6	= 1,
    kEntityTypeDNS	= 2,
    kEntityTypeProxies	= 3,
    kEntityTypeSMB	= 4,
};
typedef uint32_t	EntityType;

static CFStringRef entityTypeNames[ENTITY_TYPES_COUNT];

typedef boolean_t (GetEntityChangesFunc)(CFStringRef serviceID,
					 CFDictionaryRef state_dict,
					 CFDictionaryRef setup_dict,
					 CFDictionaryRef info);
typedef GetEntityChangesFunc * GetEntityChangesFuncRef;

static GetEntityChangesFunc get_ipv4_changes;
static GetEntityChangesFunc get_ipv6_changes;
static GetEntityChangesFunc get_dns_changes;
static GetEntityChangesFunc get_proxies_changes;
static GetEntityChangesFunc get_smb_changes;

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
    get_smb_changes,	/* 4 */
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
keyChangeListApplyToStore(keyChangeListRef keys, SCDynamicStoreRef session)
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
    if (S_IPMonitor_debug) {
	if (set != NULL) {
	    SCLog(TRUE, LOG_INFO, CFSTR("IPMonitor: Setting:\n%@\n"), set);
	}
	if (remove != NULL) {
	    SCLog(TRUE, LOG_INFO, CFSTR("IPMonitor: Removing:\n%@\n"), remove);
	}
	if (notify != NULL) {
	    SCLog(TRUE, LOG_INFO, CFSTR("IPMonitor: Notifying:\n%@\n"), notify);
	}
    }
    (void)SCDynamicStoreSetMultiple(session, set, remove, notify);

    status = notify_post("com.apple.system.config.network_change");
    if (status == NOTIFY_STATUS_OK) {
	    SCLog(TRUE, LOG_INFO, CFSTR("network configuration changed."));
    } else {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("IPMonitor: notify_post() failed: error=%ld"), status);
    }

    return;
}

static boolean_t
S_netboot_root()
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
static CFStringRef
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

static boolean_t
service_dict_set(CFStringRef serviceID, CFStringRef entity,
		 CFDictionaryRef new_dict)
{
    boolean_t			changed = FALSE;
    CFDictionaryRef		old;
    CFMutableDictionaryRef	service_dict;

    service_dict = service_dict_copy(serviceID);
    old = CFDictionaryGetValue(service_dict, entity);
    if (new_dict == NULL) {
	if (old != NULL) {
	    SCLog(S_IPMonitor_debug, LOG_INFO,
		  CFSTR("IPMonitor: serviceID %@ removed %@ dictionary = %@"),
		  serviceID, entity, old);
	    CFDictionaryRemoveValue(service_dict, entity);
	    changed = TRUE;
	}
    }
    else {
	if (old == NULL || CFEqual(new_dict, old) == FALSE) {
	    SCLog(S_IPMonitor_debug, LOG_INFO,
		  CFSTR("IPMonitor: serviceID %@ changed %@"
			" dictionary\nold %@\nnew %@"), serviceID, entity,
		  (old != NULL) ? (CFTypeRef)old : (CFTypeRef)CFSTR("<none>"),
		  new_dict);
	    CFDictionarySetValue(service_dict, entity, new_dict);
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

#define	ALLOW_EMTPY_STRING	1<<0

static CFTypeRef
sanitize_prop(CFTypeRef val, uint32_t flags)
{
    if (val != NULL) {
	if (isA_CFString(val)) {
	    CFMutableStringRef	str;

	    str = CFStringCreateMutableCopy(NULL, 0, (CFStringRef)val);
	    CFStringTrimWhitespace(str);
	    if (!(flags & ALLOW_EMTPY_STRING) && (CFStringGetLength(str) == 0)) {
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
    if (state_prop != NULL) {
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
static __inline__ struct in_addr
subnet_addr(struct in_addr addr, struct in_addr mask)
{
    struct in_addr	net;

    net.s_addr = htonl((uint32_t)ntohl(addr.s_addr)
		       & (uint32_t)ntohl(mask.s_addr));
    return (net);
}

static boolean_t
get_ipv4_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		 CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    struct in_addr		addr = { 0 };
    CFArrayRef			addrs;
    boolean_t			changed = FALSE;
    CFMutableDictionaryRef	dict = NULL;
    struct in_addr		mask = { 0 };
    CFArrayRef			masks;
    CFDictionaryRef		new_dict = NULL;
    CFStringRef			router = NULL;
    boolean_t			valid_ip = FALSE;
    boolean_t			valid_mask = FALSE;

    if (state_dict == NULL) {
	goto done;
    }
    addrs = isA_CFArray(CFDictionaryGetValue(state_dict,
					     kSCPropNetIPv4Addresses));
    if (addrs != NULL && CFArrayGetCount(addrs) > 0) {
	valid_ip = cfstring_to_ip(CFArrayGetValueAtIndex(addrs, 0), &addr);
    }
    masks = isA_CFArray(CFDictionaryGetValue(state_dict,
					     kSCPropNetIPv4SubnetMasks));
    if (masks != NULL && CFArrayGetCount(masks) > 0) {
	valid_mask = cfstring_to_ip(CFArrayGetValueAtIndex(masks, 0), &mask);
    }
    if (valid_ip == FALSE) {
	SCLog(S_IPMonitor_debug, LOG_INFO,
	      CFSTR("IPMonitor: %@ has no valid IP address, ignoring"),
	      serviceID);
	goto done;
    }
    dict = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);
    if (setup_dict != NULL) {
	router = CFDictionaryGetValue(setup_dict,
				      kSCPropNetIPv4Router);
	if (router != NULL) {
	    CFDictionarySetValue(dict,
				 kSCPropNetIPv4Router,
				 router);
	}
    }

    /* check whether the router is direct, or non-local */
    router = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
    if (router != NULL) {
	struct in_addr		router_ip;

	if (cfstring_to_ip(router, &router_ip)) {
	    if (router_ip.s_addr == addr.s_addr) {
		/* default route routes directly to the interface */
		CFDictionarySetValue(dict, kRouterIsDirect, kCFBooleanTrue);
	    }
	    else if (valid_mask
		     && subnet_addr(addr, mask).s_addr
		     != subnet_addr(router_ip, mask).s_addr) {
		/* router is not on the same subnet */
		CFDictionarySetValue(dict, kRouterNeedsLocalIP,
				     kCFBooleanTrue);
	    }
	}
    }
    new_dict = dict;

 done:
    changed = service_dict_set(serviceID, kSCEntNetIPv4, new_dict);
    my_CFRelease(&new_dict);
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
	SCLog(S_IPMonitor_debug, LOG_INFO,
	      CFSTR("IPMonitor: %@ has no valid IPv6 address, ignoring"),
	      serviceID);
	goto done;
    }
    dict = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);
    if (setup_dict != NULL) {
	router = CFDictionaryGetValue(setup_dict,
				      kSCPropNetIPv6Router);
	if (router != NULL) {
	    CFDictionarySetValue(dict,
				 kSCPropNetIPv6Router,
				 router);
	}
    }
    new_dict = dict;
 done:
    changed = service_dict_set(serviceID, kSCEntNetIPv6, new_dict);
    my_CFRelease(&new_dict);
    return (changed);
}

static boolean_t
dns_has_supplemental(CFStringRef serviceID)
{
    CFDictionaryRef     dns_dict;
    CFDictionaryRef     service_dict;

    service_dict = CFDictionaryGetValue(S_service_state_dict, serviceID);
    if (service_dict == NULL) {
	return FALSE;
    }

    dns_dict = CFDictionaryGetValue(service_dict, kSCEntNetDNS);
    if (dns_dict == NULL) {
	return FALSE;
    }

    return CFDictionaryContainsKey(dns_dict, kSCPropNetDNSSupplementalMatchDomains);
}

static boolean_t
get_dns_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    boolean_t			changed = FALSE;
    CFStringRef			domain;
    int				i;
    struct {
	CFStringRef     key;
	uint32_t	flags;
	Boolean		append;
    } merge_list[] = {
	{ kSCPropNetDNSSearchDomains,			0,			FALSE },
	{ kSCPropNetDNSServerAddresses,			0,			FALSE },
	{ kSCPropNetDNSSortList,			0,			FALSE },
	{ kSCPropNetDNSSupplementalMatchDomains,	ALLOW_EMTPY_STRING,	TRUE  },
	{ kSCPropNetDNSSupplementalMatchOrders,		0,			TRUE  },
    };
    CFMutableDictionaryRef      new_dict = NULL;
    CFStringRef		pick_list[] = {
	kSCPropNetDNSDomainName,
	kSCPropNetDNSOptions,
	kSCPropNetDNSSearchOrder,
	kSCPropNetDNSServerPort,
	kSCPropNetDNSServerTimeout,
    };

    if ((state_dict == NULL) && (setup_dict == NULL)) {
	/* there is no DNS */
	goto done;
    }

    if ((service_dict_get(serviceID, kSCEntNetIPv4) == NULL) &&
	(service_dict_get(serviceID, kSCEntNetIPv6) == NULL)) {
	/* there is no IPv4 nor IPv6 */
	goto done;
    }

    // merge DNS configuration
    new_dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);

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

    if (CFDictionaryGetCount(new_dict) == 0) {
	my_CFRelease(&new_dict);
	goto done;
    }

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

 done:
    changed = service_dict_set(serviceID, kSCEntNetDNS, new_dict);
    my_CFRelease(&new_dict);
    return (changed);
}

static boolean_t
get_proxies_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		    CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    boolean_t			changed = FALSE;
    CFDictionaryRef		new_dict = NULL;

    if ((service_dict_get(serviceID, kSCEntNetIPv4) == NULL) &&
	(service_dict_get(serviceID, kSCEntNetIPv6) == NULL)) {
	/* there is no IPv4 nor IPv6 */
	goto done;
    }
    if (setup_dict != NULL) {
	new_dict = setup_dict;
    }
    else {
	new_dict = state_dict;
    }
 done:
    changed = service_dict_set(serviceID, kSCEntNetProxies, new_dict);
    return (changed);
}

static boolean_t
get_smb_changes(CFStringRef serviceID, CFDictionaryRef state_dict,
		CFDictionaryRef setup_dict, CFDictionaryRef info)
{
    boolean_t			changed = FALSE;
    int				i;
    CFMutableDictionaryRef      new_dict = NULL;
    CFStringRef			pick_list[] = {
	kSCPropNetSMBNetBIOSName,
	kSCPropNetSMBNetBIOSNodeType,
	kSCPropNetSMBNetBIOSScope,
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

    // merge SMB configuration
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

static CFStringRef
state_service_key(CFStringRef serviceID, CFStringRef entity)
{
    return (SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							kSCDynamicStoreDomainState,
							serviceID,
							entity));
}

static CFStringRef
setup_service_key(CFStringRef serviceID, CFStringRef entity)
{
    return (SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							kSCDynamicStoreDomainSetup,
							serviceID,
							entity));
}

static CFDictionaryRef
services_info_copy(SCDynamicStoreRef session, CFArrayRef service_list)
{
    int			count;
    CFMutableArrayRef	get_keys;
    int			i;
    int			s;
    CFDictionaryRef	info;

    count = CFArrayGetCount(service_list);
    get_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFArrayAppendValue(get_keys, S_setup_global_ipv4);
    CFArrayAppendValue(get_keys, S_setup_global_proxies);
    CFArrayAppendValue(get_keys, S_setup_global_smb);
    CFArrayAppendValue(get_keys, S_private_resolvers);

    for (s = 0; s < count; s++) {
	CFStringRef	serviceID = CFArrayGetValueAtIndex(service_list, s);

	for (i = 0; i < ENTITY_TYPES_COUNT; i++) {
	    CFStringRef	setup_key;
	    CFStringRef	state_key;

	    setup_key = setup_service_key(serviceID, entityTypeNames[i]);
	    state_key = state_service_key(serviceID, entityTypeNames[i]);
	    CFArrayAppendValue(get_keys, setup_key);
	    CFArrayAppendValue(get_keys, state_key);
	    my_CFRelease(&setup_key);
	    my_CFRelease(&state_key);
	}
    }

    info = SCDynamicStoreCopyMultiple(session, get_keys, NULL);
    my_CFRelease(&get_keys);
    return (info);
}

static CFDictionaryRef
get_service_setup_entity(CFDictionaryRef service_info, CFStringRef serviceID,
			 CFStringRef entity)
{
    CFStringRef		setup_key;
    CFDictionaryRef	setup_dict;

    setup_key = setup_service_key(serviceID, entity);
    setup_dict = my_CFDictionaryGetDictionary(service_info, setup_key);
    my_CFRelease(&setup_key);
    return (setup_dict);
}

static CFDictionaryRef
get_service_state_entity(CFDictionaryRef service_info, CFStringRef serviceID,
			 CFStringRef entity)
{
    CFStringRef		state_key;
    CFDictionaryRef	state_dict;

    state_key = state_service_key(serviceID, entity);
    state_dict = my_CFDictionaryGetDictionary(service_info, state_key);
    my_CFRelease(&state_key);
    return (state_dict);
}

static int			rtm_seq = 0;

static boolean_t
ipv4_route(int cmd, struct in_addr gateway, struct in_addr netaddr,
	   struct in_addr netmask, char * ifname, struct in_addr ifa,
	   boolean_t is_direct)
{
    boolean_t			default_route = (netaddr.s_addr == 0);
    int				len;
    boolean_t			ret = TRUE;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
	struct sockaddr_dl	ifp;
	struct sockaddr_in	ifa;
    }				rtmsg;
    int				sockfd = -1;

    if (default_route && S_netboot) {
	return (TRUE);
    }

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) == -1) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("IPMonitor: ipv4_route: open routing socket failed, %s"),
	      strerror(errno));
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
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.dst.sin_addr = netaddr;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr = gateway;
    rtmsg.mask.sin_len = sizeof(rtmsg.mask);
    rtmsg.mask.sin_family = AF_INET;
    rtmsg.mask.sin_addr = netmask;

    len = sizeof(rtmsg);
    if (ifname) {
	rtmsg.hdr.rtm_addrs |= RTA_IFP | RTA_IFA;
	/* copy the interface name */
	rtmsg.ifp.sdl_len = sizeof(rtmsg.ifp);
	rtmsg.ifp.sdl_family = AF_LINK;
	rtmsg.ifp.sdl_nlen = strlen(ifname);
	bcopy(ifname, rtmsg.ifp.sdl_data, rtmsg.ifp.sdl_nlen);
	/* and the interface address */
	rtmsg.ifa.sin_len = sizeof(rtmsg.ifa);
	rtmsg.ifa.sin_family = AF_INET;
	rtmsg.ifa.sin_addr = ifa;
    }
    else {
	/* no ifp/ifa information */
	len -= sizeof(rtmsg.ifp) + sizeof(rtmsg.ifa);
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
	    SCLog(S_IPMonitor_debug, LOG_INFO,
		  CFSTR("IPMonitor ipv4_route: "
			"write routing socket failed, %s"), strerror(errno));
	    ret = FALSE;
	}
    }

    close(sockfd);
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

    if (IN6_IS_ADDR_LINKLOCAL(&gateway) && ifname != NULL) {
	unsigned int	index = if_nametoindex(ifname);

	/* add the scope id to the link local address */
	gateway.__u6_addr.__u6_addr16[1] = (uint16_t)htons(index);
    }
    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) == -1) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("IPMonitor ipv6_route: open routing socket failed, %s"),
	      strerror(errno));
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
	    SCLog(S_IPMonitor_debug, LOG_INFO,
		  CFSTR("IPMonitor ipv6_route: write routing"
			" socket failed, %s"), strerror(errno));
	    ret = FALSE;
	}
    }

    close(sockfd);
    return (ret);
}

static boolean_t
ipv4_subnet_route_add(struct in_addr local_ip,
		      struct in_addr subnet, struct in_addr mask, char * ifname)
{
    if (S_IPMonitor_debug) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("IPMonitor: IPv4 route add -net "
		    IP_FORMAT " -netmask %s interface %s"),
	      IP_LIST(&subnet), inet_ntoa(mask), ifname);
    }
    return (ipv4_route(RTM_ADD, local_ip, subnet, mask, ifname, local_ip,
		       FALSE));
}

static boolean_t
ipv4_subnet_route_delete(struct in_addr subnet, struct in_addr mask)
{
    if (S_IPMonitor_debug) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("IPMonitor: IPv4 route delete -net "
		    IP_FORMAT " %s"),
	      IP_LIST(&subnet), inet_ntoa(mask));
    }
    return (ipv4_route(RTM_DELETE, S_ip_zeros, subnet, mask, NULL,
		       S_ip_zeros, FALSE));
}


static boolean_t
ipv4_default_route_delete(void)
{
    if (S_IPMonitor_debug) {
	SCLog(TRUE, LOG_INFO, CFSTR("IPMonitor: IPv4 route delete default"));
    }
    return (ipv4_route(RTM_DELETE, S_ip_zeros, S_ip_zeros, S_ip_zeros, NULL,
		       S_ip_zeros, FALSE));
}

static boolean_t
ipv4_default_route_add(struct in_addr router, char * ifname,
		       struct in_addr local_ip, boolean_t is_direct)
{
    if (S_IPMonitor_debug) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("IPMonitor: IPv4 route add default"
		    " %s interface %s direct %d"),
	      inet_ntoa(router), ifname, is_direct);
    }
    return (ipv4_route(RTM_ADD, router, S_ip_zeros, S_ip_zeros, ifname,
		       local_ip, is_direct));
}

static boolean_t
ipv4_default_route_change(struct in_addr router, char * ifname,
			  struct in_addr local_ip, boolean_t is_direct)
{
    if (S_IPMonitor_debug) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("IPMonitor: IPv4 route change default"
		    " %s interface %s direct %d"),
	      inet_ntoa(router), ifname, is_direct);
    }
    return (ipv4_route(RTM_CHANGE, router, S_ip_zeros, S_ip_zeros, ifname,
		       local_ip, is_direct));
}

static boolean_t
ipv6_default_route_delete(void)
{
    if (S_IPMonitor_debug) {
	SCLog(TRUE, LOG_INFO, CFSTR("IPMonitor: IPv6 route delete default"));
    }
    return (ipv6_route(RTM_DELETE, S_ip6_zeros, S_ip6_zeros, S_ip6_zeros, NULL, FALSE));
}

static boolean_t
ipv6_default_route_add(struct in6_addr router, char * ifname,
		       boolean_t is_direct)
{
    if (S_IPMonitor_debug) {
	char	str[128];

	str[0] = '\0';

	inet_ntop(AF_INET6, &router, str, sizeof(str));
	SCLog(TRUE,LOG_INFO,
	      CFSTR("IPMonitor: IPv6 route add default"
		    " %s interface %s direct %d"),
	      str, ifname, is_direct);
    }
    return (ipv6_route(RTM_ADD, router, S_ip6_zeros, S_ip6_zeros, ifname, is_direct));
}


static boolean_t
multicast_route_delete()
{
    struct in_addr gateway = { htonl(INADDR_LOOPBACK) };
    struct in_addr netaddr = { htonl(INADDR_UNSPEC_GROUP) };
    struct in_addr netmask = { htonl(IN_CLASSD_NET) };

    return (ipv4_route(RTM_DELETE, gateway, netaddr, netmask, "lo0",
		       gateway, FALSE));
}

static boolean_t
multicast_route_add()
{
    struct in_addr gateway = { htonl(INADDR_LOOPBACK) };
    struct in_addr netaddr = { htonl(INADDR_UNSPEC_GROUP) };
    struct in_addr netmask = { htonl(IN_CLASSD_NET) };

    return (ipv4_route(RTM_ADD, gateway, netaddr, netmask, "lo0",
		       gateway, FALSE));
}

static void
set_ipv4_router(struct in_addr * router, char * ifname,
		struct in_addr * local_ip, boolean_t is_direct)
{
    if (S_router_subnet.s_addr != 0) {
	ipv4_subnet_route_delete(S_router_subnet, S_router_subnet_mask);
	S_router_subnet.s_addr = S_router_subnet_mask.s_addr = 0;
    }
    /* assign the new default route, ensure local multicast route available */
    (void)ipv4_default_route_delete();
    if (router != NULL) {
	(void)ipv4_default_route_add(*router, ifname,
				     (local_ip != NULL)
				     ? *local_ip : S_ip_zeros,
				     is_direct);
	(void)multicast_route_delete();
    }
    else {
	(void)multicast_route_add();
    }

    return;
}

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

	if (isA_CFString(val_domain_name)) {
	    SCPrint(TRUE, f, CFSTR("domain %@\n"), val_domain_name);
	}

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

static void
update_ipv4(CFDictionaryRef	service_info,
	    CFStringRef		primary,
	    keyChangeListRef	keys)
{
    CFDictionaryRef	ipv4_dict = NULL;

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    ipv4_dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv4);
	}
    }
    if (ipv4_dict != NULL) {
	CFMutableDictionaryRef	dict = NULL;
	CFStringRef		if_name = NULL;
	char			ifn[IFNAMSIZ + 1] = { '\0' };
	char *			ifn_p = NULL;
	boolean_t		is_direct = FALSE;
	struct in_addr		local_ip = { 0 };
	CFStringRef		local_ip_cf;
	CFArrayRef		local_ip_list;
	boolean_t		needs_local_ip = FALSE;
	struct in_addr		router = { 0 };
	CFStringRef		val_router = NULL;

	dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	local_ip_list = CFDictionaryGetValue(ipv4_dict,
					     kSCPropNetIPv4Addresses);
	local_ip_cf = CFArrayGetValueAtIndex(local_ip_list, 0);
	cfstring_to_ip(local_ip_cf, &local_ip);
	val_router = CFDictionaryGetValue(ipv4_dict, kSCPropNetIPv4Router);
	if (val_router != NULL) {
	    cfstring_to_ip(val_router, &router);
	    CFDictionarySetValue(dict, kSCPropNetIPv4Router, val_router);
	    if (CFDictionaryContainsKey(ipv4_dict, kRouterIsDirect)) {
		is_direct = TRUE;
	    }
	    else if (CFDictionaryContainsKey(ipv4_dict, kRouterNeedsLocalIP)) {
		needs_local_ip = TRUE;
	    }
	}
	else {
	    is_direct = TRUE;
	    router = local_ip;
	}
	if_name = CFDictionaryGetValue(ipv4_dict, kSCPropInterfaceName);
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
	keyChangeListSetValue(keys, S_state_global_ipv4, dict);
	CFRelease(dict);

	/* route add default ... */
	if (needs_local_ip) {
	    struct in_addr	m;

	    m.s_addr = htonl(INADDR_BROADCAST);
	    ipv4_subnet_route_add(local_ip, router, m, ifn_p);
	    set_ipv4_router(&local_ip, ifn_p, &local_ip, FALSE);
	    ipv4_default_route_change(router, ifn_p, local_ip, FALSE);
	    S_router_subnet = router;
	    S_router_subnet_mask = m;
	}
	else {
	    set_ipv4_router(&router, ifn_p, &local_ip, is_direct);
	}
    }
    else {
	keyChangeListRemoveValue(keys, S_state_global_ipv4);
	set_ipv4_router(NULL, NULL, NULL, FALSE);
    }
    return;
}

static void
update_ipv6(CFDictionaryRef	service_info,
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
	char			ifn[IFNAMSIZ + 1] = { '\0' };
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

	{ /* route add default ... */
	    struct in6_addr	router;

	    (void)cfstring_to_ip6(val_router, &router);
	    set_ipv6_router(&router, ifn_p, is_direct);
	}
    }
    else {
	keyChangeListRemoveValue(keys, S_state_global_ipv6);
	set_ipv6_router(NULL, NULL, FALSE);
    }
    return;
}

static void
update_dns(CFDictionaryRef	service_info,
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
	empty_dns();
	keyChangeListRemoveValue(keys, S_state_global_dns);
    }
    else {
	set_dns(CFDictionaryGetValue(dict, kSCPropNetDNSSearchDomains),
		CFDictionaryGetValue(dict, kSCPropNetDNSDomainName),
		CFDictionaryGetValue(dict, kSCPropNetDNSServerAddresses),
		CFDictionaryGetValue(dict, kSCPropNetDNSSortList));
	keyChangeListSetValue(keys, S_state_global_dns, dict);
    }
    return;
}

static void
update_dnsinfo(CFDictionaryRef	service_info,
	       CFStringRef	primary,
	       keyChangeListRef	keys,
	       CFArrayRef	service_order)
{
    CFArrayRef	privateResolvers;

    privateResolvers = CFDictionaryGetValue(service_info, S_private_resolvers);

    if (primary == NULL) {
	dns_configuration_set(NULL, NULL, NULL, privateResolvers);
    } else {
	CFDictionaryRef	dict		= NULL;
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    dict = CFDictionaryGetValue(service_dict, kSCEntNetDNS);
	}

	dns_configuration_set(dict, S_service_state_dict, service_order, privateResolvers);
    }
    keyChangeListNotifyKey(keys, S_state_global_dns);
    return;
}

static void
update_proxies(CFDictionaryRef	service_info,
	       CFStringRef	primary,
	       keyChangeListRef	keys)
{
    CFDictionaryRef	dict = NULL;

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    dict = CFDictionaryGetValue(service_dict, kSCEntNetProxies);
	    if (dict == NULL) {
		dict = my_CFDictionaryGetDictionary(service_info,
						    S_setup_global_proxies);
	    }
	}
    }
    if (dict == NULL) {
	keyChangeListRemoveValue(keys, S_state_global_proxies);
    }
    else {
	keyChangeListSetValue(keys, S_state_global_proxies, dict);
    }
    return;
}

static void
update_smb(CFDictionaryRef	service_info,
	   CFStringRef		primary,
	   keyChangeListRef	keys)
{
    CFDictionaryRef	dict	= NULL;

    if (primary != NULL) {
	CFDictionaryRef	service_dict;

	service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
	if (service_dict != NULL) {
	    dict = CFDictionaryGetValue(service_dict, kSCEntNetSMB);
	    if (dict == NULL) {
		dict = my_CFDictionaryGetDictionary(service_info,
						    S_setup_global_smb);
	    }
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

static unsigned int
get_service_rank(CFStringRef proto_key, CFArrayRef order, CFStringRef serviceID)
{
    CFDictionaryRef	d;
    CFIndex		i;
    CFDictionaryRef	proto_dict;

    if (serviceID == NULL) {
	goto done;
    }
    d = CFDictionaryGetValue(S_service_state_dict, serviceID);
    if (d == NULL) {
	goto done;
    }

    proto_dict = CFDictionaryGetValue(d, proto_key);
    if (proto_dict) {
	CFStringRef	if_name;
	CFNumberRef	override = NULL;

	if_name = CFDictionaryGetValue(proto_dict, kSCPropInterfaceName);
	if (S_ppp_override_primary == TRUE
	    && if_name != NULL
	    && CFStringHasPrefix(if_name, CFSTR("ppp"))) {
	    /* PPP override: make ppp* look the best */
	    /* Hack: should use interface type, not interface name */
	    return (0);
	}
	/* check for the "OverridePrimary" property */
	override = CFDictionaryGetValue(proto_dict, kSCPropNetOverridePrimary);
	if (isA_CFNumber(override) != NULL) {
	    int		val = 0;

	    CFNumberGetValue(override,  kCFNumberIntType, &val);
	    if (val != 0) {
		return (0);
	    }
	}
    }

    if (serviceID != NULL && order != NULL) {
	CFIndex	n = CFArrayGetCount(order);

	for (i = 0; i < n; i++) {
	    CFStringRef s = isA_CFString(CFArrayGetValueAtIndex(order, i));

	    if (s == NULL) {
		continue;
	    }
	    if (CFEqual(serviceID, s)) {
		return (i + 1);
	    }
	}
    }

 done:
    return (UINT_MAX);
}

/**
 ** Service election:
 **/
typedef boolean_t (*routerCheckFunc)(CFStringRef str);

static boolean_t
check_ipv4_router(CFStringRef router)
{
    struct in_addr	ip;

    return (cfstring_to_ip(router, &ip));
}

static boolean_t
check_ipv6_router(CFStringRef router)
{
    struct in6_addr	ip6;

    return (cfstring_to_ip6(router, &ip6));
}

struct election_state {
    routerCheckFunc		router_check;
    CFStringRef			proto_key; /* e.g. kSCEntNetIPv4 */
    CFStringRef			router_key;/* e.g. kSCPropNetIPv4Router */
    CFArrayRef			order;
    CFStringRef			new_primary;
    boolean_t			new_has_router;
    unsigned int		new_primary_index;
};

static void
elect_protocol(const void * key, const void * value, void * context)
{
    struct election_state *	elect_p = (struct election_state *)context;
    CFDictionaryRef		proto_dict = NULL;
    CFStringRef			router;
    boolean_t			router_valid = FALSE;
    CFStringRef			serviceID = (CFStringRef)key;
    CFDictionaryRef		service_dict = (CFDictionaryRef)value;
    unsigned int		service_index;

    proto_dict = CFDictionaryGetValue(service_dict, elect_p->proto_key);
    if (proto_dict == NULL) {
	return;
    }
    router = CFDictionaryGetValue(proto_dict, elect_p->router_key);
    router_valid = (*elect_p->router_check)(router);
    if (router_valid == FALSE && elect_p->new_has_router == TRUE) {
	/* skip it */
	return;
    }
    service_index
	= get_service_rank(elect_p->proto_key, elect_p->order, serviceID);
    if (elect_p->new_primary == NULL
	|| service_index < elect_p->new_primary_index
	|| (router_valid && elect_p->new_has_router == FALSE)) {
	my_CFRelease(&elect_p->new_primary);
	elect_p->new_primary = CFRetain(serviceID);
	elect_p->new_primary_index = service_index;
	elect_p->new_has_router = router_valid;
    }
    return;
}

static CFStringRef
elect_new_primary(CFArrayRef order, CFStringRef proto_key,
		  CFStringRef router_key)
{
    struct election_state	elect;

    if (CFEqual(proto_key, kSCEntNetIPv4)) {
	elect.router_check = check_ipv4_router;
    }
    else if (CFEqual(proto_key, kSCEntNetIPv6)) {
	elect.router_check = check_ipv6_router;
    }
    else {
	return (NULL);
    }
    elect.order = order;
    elect.new_primary = NULL;
    elect.new_primary_index = 0;
    elect.new_has_router = FALSE;
    elect.proto_key = proto_key;
    elect.router_key = router_key;
    CFDictionaryApplyFunction(S_service_state_dict, elect_protocol, &elect);
    return (elect.new_primary);
}

static uint32_t
service_changed(CFDictionaryRef services_info, CFStringRef serviceID)
{
    uint32_t		changed = 0;
    int			i;

    for (i = 0; i < ENTITY_TYPES_COUNT; i++) {
	GetEntityChangesFuncRef func = entityChangeFunc[i];
	if ((*func)(serviceID,
		    get_service_state_entity(services_info, serviceID,
					     entityTypeNames[i]),
		    get_service_setup_entity(services_info, serviceID,
					     entityTypeNames[i]),
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
	    SCLog(S_IPMonitor_debug, LOG_INFO,
		  CFSTR("IPMonitor: %@ is still primary %s"),
		  new_primary, entity);
	}
	else {
	    my_CFRelease(primary_p);
	    *primary_p = CFRetain(new_primary);
	    SCLog(S_IPMonitor_debug, LOG_INFO,
		  CFSTR("IPMonitor: %@ is the new primary %s"),
		  new_primary, entity);
	    changed = TRUE;
	}
    }
    else if (primary != NULL) {
	SCLog(S_IPMonitor_debug, LOG_INFO,
	      CFSTR("IPMonitor: %@ is no longer primary %s"), primary, entity);
	my_CFRelease(primary_p);
	changed = TRUE;
    }
    return (changed);
}

static unsigned int
rank_service_entity(CFArrayRef order, CFStringRef primary,
		    CFStringRef proto_key, CFStringRef entity)
{
    CFDictionaryRef	dict;
    dict = service_dict_get(primary, entity);
    if (dict == NULL) {
	return (UINT_MAX);
    }
    return (get_service_rank(proto_key, order, primary));
}

static void
IPMonitorNotify(SCDynamicStoreRef session, CFArrayRef changed_keys,
		void * not_used)
{
    CFIndex		count;
    boolean_t		dnsinfo_changed = FALSE;
    boolean_t		global_ipv4_changed = FALSE;
    boolean_t		global_ipv6_changed = FALSE;
    keyChangeList	keys;
    int			i;
    CFIndex		n;
    CFArrayRef		service_order;
    CFMutableArrayRef	service_changes = NULL;
    CFDictionaryRef	services_info = NULL;

    count = CFArrayGetCount(changed_keys);
    if (count == 0) {
	return;
    }

    SCLog(S_IPMonitor_debug, LOG_INFO,
	  CFSTR("IPMonitor: changes %@ (%d)"), changed_keys, count);

    keyChangeListInit(&keys);
    service_changes = CFArrayCreateMutable(NULL, 0,
					   &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	CFStringRef	change = CFArrayGetValueAtIndex(changed_keys, i);
	if (CFEqual(change, S_setup_global_ipv4)) {
	    global_ipv4_changed = TRUE;
	    global_ipv6_changed = TRUE;
	}
	else if (CFEqual(change, S_setup_global_proxies)) {
	    if (S_primary_proxies != NULL) {
		my_CFArrayAppendUniqueValue(service_changes, S_primary_proxies);
	    }
	}
	else if (CFEqual(change, S_setup_global_smb)) {
	    if (S_primary_smb != NULL) {
		my_CFArrayAppendUniqueValue(service_changes, S_primary_smb);
	    }
	}
	else if (CFEqual(change, S_private_resolvers)) {
	    dnsinfo_changed = TRUE;
	}
	else if (CFStringHasPrefix(change, S_state_service_prefix)) {
	    CFStringRef serviceID = parse_component(change,
						    S_state_service_prefix);
	    if (serviceID) {
		my_CFArrayAppendUniqueValue(service_changes, serviceID);
		CFRelease(serviceID);
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
    }

    /* grab a snapshot of everything we need */
    services_info = services_info_copy(session, service_changes);
    service_order = service_order_get(services_info);
    if (service_order != NULL) {
	SCLog(S_IPMonitor_debug, LOG_INFO,
	      CFSTR("IPMonitor: service_order %@ "), service_order);
    }
    n = CFArrayGetCount(service_changes);
    for (i = 0; i < n; i++) {
	uint32_t	changes;
	CFStringRef	serviceID;
	Boolean		wasSupplemental;

	serviceID = CFArrayGetValueAtIndex(service_changes, i);
	wasSupplemental = dns_has_supplemental(serviceID);
	changes = service_changed(services_info, serviceID);

	if (S_primary_ipv4 != NULL && CFEqual(S_primary_ipv4, serviceID)) {
	    if ((changes & (1 << kEntityTypeIPv4)) != 0) {
		update_ipv4(services_info, serviceID, &keys);
		global_ipv4_changed = TRUE;
	    }
	}
	else if ((changes & (1 << kEntityTypeIPv4)) != 0) {
	    global_ipv4_changed = TRUE;
	}
	if ((changes & (1 << kEntityTypeIPv6)) != 0) {
	    if (S_primary_ipv6 != NULL && CFEqual(S_primary_ipv6, serviceID)) {
		update_ipv6(services_info, serviceID, &keys);
	    }
	    global_ipv6_changed = TRUE;
	}
	if ((changes & (1 << kEntityTypeDNS)) != 0) {
	    if (S_primary_dns != NULL && CFEqual(S_primary_dns, serviceID)) {
		update_dns(services_info, serviceID, &keys);
		dnsinfo_changed = TRUE;
	    }
	    else if (wasSupplemental || dns_has_supplemental(serviceID)) {
		dnsinfo_changed = TRUE;
	    }
	}
	if ((changes & (1 << kEntityTypeProxies)) != 0) {
	    if (S_primary_proxies != NULL && CFEqual(S_primary_proxies, serviceID)) {
		update_proxies(services_info, serviceID, &keys);
	    }
	}
	if ((changes & (1 << kEntityTypeSMB)) != 0) {
	    if (S_primary_smb != NULL && CFEqual(S_primary_smb, serviceID)) {
		update_smb(services_info, serviceID, &keys);
	    }
	}
    }

    if (global_ipv4_changed) {
	CFStringRef new_primary;

	SCLog(S_IPMonitor_debug, LOG_INFO,
	      CFSTR("IPMonitor: IPv4 service election"));
	new_primary = elect_new_primary(service_order,
					kSCEntNetIPv4, kSCPropNetIPv4Router);
	if (set_new_primary(&S_primary_ipv4, new_primary, "IPv4")) {
	    update_ipv4(services_info, S_primary_ipv4, &keys);
	}
	my_CFRelease(&new_primary);
    }
    if (global_ipv6_changed) {
	CFStringRef new_primary;

	SCLog(S_IPMonitor_debug, LOG_INFO,
	      CFSTR("IPMonitor: IPv6 service election"));
	new_primary = elect_new_primary(service_order,
					kSCEntNetIPv6, kSCPropNetIPv6Router);
	if (set_new_primary(&S_primary_ipv6, new_primary, "IPv6")) {
	    update_ipv6(services_info, S_primary_ipv6, &keys);
	}
	my_CFRelease(&new_primary);
    }
    if (global_ipv4_changed || global_ipv6_changed) {
	CFStringRef	new_primary_dns;
	CFStringRef	new_primary_proxies;
	CFStringRef	new_primary_smb;

	if (S_primary_ipv4 != NULL && S_primary_ipv6 != NULL) {
	    /* decide between IPv4 and IPv6 */
	    if (rank_service_entity(service_order, S_primary_ipv4,
				    kSCEntNetIPv4, kSCEntNetDNS)
		<= rank_service_entity(service_order, S_primary_ipv6,
				       kSCEntNetIPv6, kSCEntNetDNS)) {
		new_primary_dns = S_primary_ipv4;
	    }
	    else {
		new_primary_dns = S_primary_ipv6;
	    }
	    if (rank_service_entity(service_order, S_primary_ipv4,
				    kSCEntNetIPv4, kSCEntNetProxies)
		<= rank_service_entity(service_order, S_primary_ipv6,
				       kSCEntNetIPv6, kSCEntNetProxies)) {
		new_primary_proxies = S_primary_ipv4;
	    }
	    else {
		new_primary_proxies = S_primary_ipv6;
	    }
	    if (rank_service_entity(service_order, S_primary_ipv4,
				    kSCEntNetIPv4, kSCEntNetSMB)
		<= rank_service_entity(service_order, S_primary_ipv6,
				       kSCEntNetIPv6, kSCEntNetSMB)) {
		new_primary_smb = S_primary_ipv4;
	    }
	    else {
		new_primary_smb = S_primary_ipv6;
	    }

	}
	else if (S_primary_ipv6 != NULL) {
	    new_primary_dns     = S_primary_ipv6;
	    new_primary_proxies = S_primary_ipv6;
	    new_primary_smb     = S_primary_ipv6;
	}
	else if (S_primary_ipv4 != NULL) {
	    new_primary_dns     = S_primary_ipv4;
	    new_primary_proxies = S_primary_ipv4;
	    new_primary_smb     = S_primary_ipv4;
	}
	else {
	    new_primary_dns     = NULL;
	    new_primary_proxies = NULL;
	    new_primary_smb     = NULL;
	}

	if (set_new_primary(&S_primary_dns, new_primary_dns, "DNS")) {
	    update_dns(services_info, S_primary_dns, &keys);
	    dnsinfo_changed = TRUE;
	}
	if (set_new_primary(&S_primary_proxies, new_primary_proxies, "Proxies")) {
	    update_proxies(services_info, S_primary_proxies, &keys);
	}
	if (set_new_primary(&S_primary_smb, new_primary_smb, "SMB")) {
	    update_smb(services_info, S_primary_smb, &keys);
	}
    }
    if (dnsinfo_changed) {
	update_dnsinfo(services_info, S_primary_dns, &keys, service_order);
    }
    my_CFRelease(&service_changes);
    my_CFRelease(&services_info);
    keyChangeListApplyToStore(&keys, session);
    keyChangeListFree(&keys);
    return;
}

static void
initEntityNames(void)
{
    entityTypeNames[0] = kSCEntNetIPv4;		/* 0 */
    entityTypeNames[1] = kSCEntNetIPv6;		/* 1 */
    entityTypeNames[2] = kSCEntNetDNS;		/* 2 */
    entityTypeNames[3] = kSCEntNetProxies;	/* 3 */
    entityTypeNames[4] = kSCEntNetSMB;		/* 4 */
    return;
}

static void
ip_plugin_init()
{
    int			i;
    CFStringRef		key;
    CFMutableArrayRef	keys = NULL;
    CFMutableArrayRef	patterns = NULL;
    CFRunLoopSourceRef	rls = NULL;
    SCDynamicStoreRef	session = NULL;

    initEntityNames();
    if (S_netboot_root() != 0) {
	S_netboot = TRUE;
    }
    session = SCDynamicStoreCreate(NULL, CFSTR("IPMonitor"),
				   IPMonitorNotify, NULL);
    if (session == NULL) {
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
    S_state_global_smb
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetSMB);
    S_setup_global_ipv4
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    S_setup_global_proxies
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetProxies);
    S_setup_global_smb
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetSMB);
    S_state_service_prefix
	= SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@/"),
				  kSCDynamicStoreDomainState,
				  kSCCompNetwork,
				  kSCCompService);
    S_setup_service_prefix
	= SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@/"),
				  kSCDynamicStoreDomainSetup,
				  kSCCompNetwork,
				  kSCCompService);
    S_service_state_dict
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);

    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* register for State: and Setup: per-service notifications */
    for (i = 0; i < ENTITY_TYPES_COUNT; i++) {
	key = state_service_key(kSCCompAnyRegex, entityTypeNames[i]);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);
	key = setup_service_key(kSCCompAnyRegex, entityTypeNames[i]);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);
    }

    /* add notifier for ServiceOrder/PPPOverridePrimary changes for IPv4 */
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    CFArrayAppendValue(keys, key);
    CFRelease(key);

    /* add notifier for Private DNS configuration */
    S_private_resolvers = SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@"),
						  kSCDynamicStoreDomainState,
						  kSCCompNetwork,
						  CFSTR(kDNSServiceCompPrivateDNS));
    CFArrayAppendValue(keys, S_private_resolvers);

    if (!SCDynamicStoreSetNotificationKeys(session, keys, patterns)) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("IPMonitor ip_plugin_init "
		    "SCDynamicStoreSetNotificationKeys failed: %s"),
	      SCErrorString(SCError()));
	goto done;
    }

    rls = SCDynamicStoreCreateRunLoopSource(NULL, session, 0);
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
    dns_configuration_set(NULL, NULL, NULL, NULL);
    empty_dns();
    (void)SCDynamicStoreRemoveValue(session, S_state_global_dns);

    /* initialize SMB configuration */
    (void)SCDynamicStoreRemoveValue(session, S_state_global_smb);

  done:
    my_CFRelease(&keys);
    my_CFRelease(&patterns);
    my_CFRelease(&session);
    return;
}

__private_extern__
void
prime_IPMonitor()
{
    /* initialize multicast route */
    set_ipv4_router(NULL, NULL, NULL, FALSE);
}

__private_extern__
void
load_IPMonitor(CFBundleRef bundle, Boolean bundleVerbose)
{
    if (bundleVerbose) {
	S_IPMonitor_debug = 1;
    }

    dns_configuration_init(bundle);
    ip_plugin_init();

    load_hostname(S_IPMonitor_debug);
    load_smb_configuration(S_IPMonitor_debug);

    return;
}


#ifdef  MAIN
#undef  MAIN
#include "dns-configuration.c"
#include "set-hostname.c"

int
main(int argc, char **argv)
{
    _sc_log     = FALSE;
    _sc_verbose = (argc > 1) ? TRUE : FALSE;

    load_IPMonitor(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
    prime_IPMonitor();
    CFRunLoopRun();
    /* not reached */
    exit(0);
    return 0;
}
#endif

