/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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
 * July 19, 2000 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 *
 * November 15, 2000	Dieter Siegmund (dieter@apple.com)
 * - changed to use new configuration model
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#define USE_FLAT_FILES	"UseFlatFiles"

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

/* debug output on/off */
static boolean_t 		S_debug;

/* dictionary to hold per-interface state: key is the interface name  */
static CFMutableDictionaryRef	S_ifstate_dict = NULL;
static CFMutableDictionaryRef	S_service_ifname_dict = NULL;

/* if set, create empty netinfo config file instead of removing it */
static boolean_t		S_empty_netinfo = TRUE;

/* if set, a PPP interface override the primary */
static boolean_t		S_ppp_override_primary = TRUE;

/* the name of the current primary interface */
static CFStringRef		S_ifn_primary = NULL;

static CFStringRef		S_state_global_ipv4 = NULL;
static CFStringRef		S_state_global_dns = NULL;
static CFStringRef		S_state_global_netinfo = NULL;
static CFStringRef		S_state_global_proxies = NULL;
static CFStringRef		S_setup_global_netinfo = NULL;
static CFStringRef		S_setup_global_proxies = NULL;
static CFStringRef		S_state_interface_prefix = NULL;
static CFStringRef		S_setup_service_prefix = NULL;

#define VAR_RUN_RESOLV_CONF		"/var/run/resolv.conf"
#define VAR_RUN_NICONFIG_LOCAL_XML	"/var/run/niconfig_local.xml"

static __inline__ CFTypeRef
isA_CFType(CFTypeRef obj, CFTypeID type)
{
    if (obj == NULL)
	return (NULL);

    if (CFGetTypeID(obj) != type) {
	return (NULL);
    }
    return (obj);
}

static __inline__ CFTypeRef
isA_CFDictionary(CFTypeRef obj)
{
    return (isA_CFType(obj, CFDictionaryGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFArray(CFTypeRef obj)
{
    return (isA_CFType(obj, CFArrayGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFString(CFTypeRef obj)
{
    return (isA_CFType(obj, CFStringGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFBoolean(CFTypeRef obj)
{
    return (isA_CFType(obj, CFBooleanGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFNumber(CFTypeRef obj)
{
    return (isA_CFType(obj, CFNumberGetTypeID()));
}


static void
my_CFArrayAppendUniqueValue(CFMutableArrayRef arr, CFTypeRef new)
{
    int i;

    for (i = 0; i < CFArrayGetCount(arr); i++) {
	CFStringRef element = CFArrayGetValueAtIndex(arr, i);
	if (CFEqual(element, new)) {
	    return;
	}
    }
    CFArrayAppendValue(arr, new);
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

static void
my_SCDHandleRelease(SCDHandleRef * handle)
{
    if (handle && *handle) {
	SCDHandleRelease(*handle);
	*handle = NULL;
    }
    return;
}

static CFDictionaryRef
my_SCDGet(SCDSessionRef session, CFStringRef key)
{
    SCDHandleRef		data = NULL;
    CFDictionaryRef 		dict = NULL;
    SCDStatus			status;

    status = SCDGet(session, key, &data);
    if (status == SCD_OK) {
	dict = SCDHandleGetData(data);
	dict = isA_CFDictionary(dict);
	if (dict) {
	    CFRetain(dict);
	}
	my_SCDHandleRelease(&data);
    }
    return (dict);
}

static __inline__ void
serviceID_remove(CFTypeRef serviceID)
{
    CFDictionaryRemoveValue(S_service_ifname_dict, serviceID);
    return;
}

static __inline__ void
serviceID_remove_ifname(CFTypeRef ifname)
{
    CFIndex		count = CFDictionaryGetCount(S_service_ifname_dict);
    CFIndex 		i;
    void * *		keys;
    void * *		values;

    if (count == 0) {
	return;
    }

    keys = (void * *)malloc(sizeof(void *) * count);
    values = (void * *)malloc(sizeof(void *) * count);

    if (keys == NULL || values == NULL) {
	goto done;
    }
    CFDictionaryGetKeysAndValues(S_service_ifname_dict, keys, values);
    for (i = 0; i < count; i++) {
	CFStringRef		serviceID = keys[i];
	CFStringRef		ifn = values[i];

	if (CFEqual(ifname, ifn)) {
	    CFDictionaryRemoveValue(S_service_ifname_dict, serviceID);
	}
    }
 done:
    if (keys)
	free(keys);
    if (values)
	free(values);
    return;
}

static __inline__ void
serviceID_add(CFTypeRef serviceID, CFTypeRef ifname)
{
    CFDictionarySetValue(S_service_ifname_dict, serviceID, ifname);
    return;
}

static void
serviceID_add_list(CFArrayRef list, CFTypeRef ifname)
{
    int i;

    for (i = 0; i < CFArrayGetCount(list); i++) {
	CFTypeRef serviceID = CFArrayGetValueAtIndex(list, i);
	serviceID_add(serviceID, ifname);
    }
    return;
}

static __inline__ CFTypeRef
serviceID_get_ifname(CFTypeRef serviceID)
{
    return (CFDictionaryGetValue(S_service_ifname_dict, serviceID));
}

static struct in_addr
cfstring_to_ip(CFStringRef str)
{
    char		buf[32];
    struct in_addr	ip = { 0 };
    CFIndex		l;
    int			n;
    CFRange		range;

    if (str == NULL)
	return ip;

    range = CFRangeMake(0, CFStringGetLength(str));
    n = CFStringGetBytes(str, range, kCFStringEncodingMacRoman,
			 0, FALSE, buf, sizeof(buf), &l);
    buf[l] = '\0';
    inet_aton(buf, &ip);
    return (ip);
}


static int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    CFIndex		l;
    CFIndex		n;
    CFRange		range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    n = CFStringGetBytes(cfstr, range, kCFStringEncodingMacRoman,
			 0, FALSE, str, len, &l);
    str[l] = '\0';
    return (l);
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

static void
append_netinfo_arrays(CFDictionaryRef dict, CFMutableArrayRef ni_addrs,
		      CFMutableArrayRef ni_tags)
{
    CFArrayRef 	addrs;
    CFArrayRef 	tags;

    if (isA_CFDictionary(dict) == NULL)
	return;

    addrs 
	= isA_CFArray(CFDictionaryGetValue(dict,
					   kSCPropNetNetInfoServerAddresses));
    tags = isA_CFArray(CFDictionaryGetValue(dict,
					    kSCPropNetNetInfoServerTags));
    if (addrs && tags) {
	CFIndex		addrs_count = CFArrayGetCount(addrs);
	CFIndex 	tags_count = CFArrayGetCount(tags);

	if (addrs_count > 0) {
	    if (addrs_count == tags_count) {
		CFArrayAppendArray(ni_addrs, addrs,
				   CFRangeMake(0, addrs_count));
		CFArrayAppendArray(ni_tags, tags,
				   CFRangeMake(0, tags_count));
	    }

	}

    }
    return;
}

static CFStringRef
get_broadcast_address(CFDictionaryRef ipv4_dict)
{
    struct in_addr	addr = { 0 };
    CFArrayRef 		arr;
    CFStringRef		broadcast = NULL;
    struct in_addr	mask = { 0 };

    arr = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
					   kSCPropNetIPv4Addresses));
    if (arr && CFArrayGetCount(arr))
	addr = cfstring_to_ip(CFArrayGetValueAtIndex(arr, 0));
    arr = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
					   kSCPropNetIPv4SubnetMasks));
    if (arr && CFArrayGetCount(arr))
	mask = cfstring_to_ip(CFArrayGetValueAtIndex(arr, 0));
    if (addr.s_addr && mask.s_addr) {
	struct in_addr 		b;

	b.s_addr = htonl(ntohl(addr.s_addr) | ~ntohl(mask.s_addr));
	broadcast = CFStringCreateWithFormat(NULL, NULL,
					     CFSTR(IP_FORMAT),
					     IP_LIST(&b));
    }
    return (broadcast);
}

CFTypeRef
highest_serviceID(CFArrayRef list, CFArrayRef order)
{
    int 	i;
    CFRange 	range = CFRangeMake(0, CFArrayGetCount(list));

    if (list == NULL || CFArrayGetCount(list) == 0) {
	return (NULL);
    }
    if (order) {
	for (i = 0; i < CFArrayGetCount(order); i++) {
	    CFTypeRef	serviceID = CFArrayGetValueAtIndex(order, i);
	    if (CFArrayContainsValue(list, range, serviceID)) {
		return (serviceID);
	    }
	}
    }
    return (CFArrayGetValueAtIndex(list, 0));
}

static CFDictionaryRef
make_netinfo_dict(SCDSessionRef session, CFStringRef state_key, 
		  CFDictionaryRef ipv4_dict, 
		  CFDictionaryRef setup_dict)
{
    CFMutableDictionaryRef	ni_dict = NULL;
    boolean_t			has_manual = FALSE;
    boolean_t			has_broadcast = FALSE;
    boolean_t			has_dhcp = FALSE;
    CFIndex			i;
    CFArrayRef			m = NULL;
    CFMutableArrayRef		ni_addrs = NULL;
    CFMutableArrayRef		ni_tags = NULL;
	
    m = isA_CFArray(CFDictionaryGetValue(setup_dict,
					 kSCPropNetNetInfoBindingMethods));
    if (m == NULL) {
	goto netinfo_done;
    }
    ni_addrs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    ni_tags = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (ni_addrs == NULL || ni_tags == NULL) {
	goto netinfo_done;
    }

    /* find out which are configured */
    for (i = 0; i < CFArrayGetCount(m); i++) {
	CFStringRef method = CFArrayGetValueAtIndex(m, i);
	
	if (CFEqual(method,
		    kSCValNetNetInfoBindingMethodsManual)) {
	    has_manual = TRUE;
	}
	else if (CFEqual(method,
			 kSCValNetNetInfoBindingMethodsDHCP)) {
	    has_dhcp = TRUE;
	}
	else if (CFEqual(method,
			 kSCValNetNetInfoBindingMethodsBroadcast)) {
	    has_broadcast = TRUE;
	}
    }
    if (has_dhcp) {
	CFDictionaryRef	state_dict;

	state_dict = my_SCDGet(session, state_key);
	if (state_dict) {
	    append_netinfo_arrays(state_dict, ni_addrs, ni_tags);
	    my_CFRelease(&state_dict);
	}
    }
    if (has_manual) {
	append_netinfo_arrays(setup_dict, ni_addrs, ni_tags);
    }
    if (has_broadcast) {
	CFStringRef		addr;
	    
	addr = get_broadcast_address(ipv4_dict);
	if (addr) {
	    CFStringRef		tag;
	    tag = CFDictionaryGetValue(setup_dict,
				       kSCPropNetNetInfoBroadcastServerTag);
	    tag = isA_CFString(tag);
	    if (tag == NULL) {
		tag = kSCValNetNetInfoDefaultServerTag;
	    }
	    CFArrayAppendValue(ni_addrs, addr);
	    CFArrayAppendValue(ni_tags, tag);
	    CFRelease(addr);
	}
    }
    if (CFArrayGetCount(ni_addrs) == 0) {
	goto netinfo_done;
    }
    ni_dict = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(ni_dict, kSCPropNetNetInfoServerAddresses,
			 ni_addrs);
    CFDictionarySetValue(ni_dict, kSCPropNetNetInfoServerTags,
			 ni_tags);
 netinfo_done:
    my_CFRelease(&ni_addrs);
    my_CFRelease(&ni_tags);
    return (ni_dict);
}

static boolean_t
get_changes(SCDSessionRef session, CFStringRef ifn_cf,
	    CFStringRef pkey, CFArrayRef order, CFDictionaryRef * dict)
{
    CFMutableDictionaryRef	if_dict = NULL;
    CFDictionaryRef		ipv4_dict;
    CFDictionaryRef		prot_dict = NULL;
    CFStringRef			serviceID = NULL;
    CFArrayRef			serviceIDs = NULL;
    boolean_t			something_changed = FALSE;
    CFStringRef			state_key = NULL;

    { /* create a modifyable dictionary, a copy or a new one */
	CFDictionaryRef		d = NULL;
	d = CFDictionaryGetValue(S_ifstate_dict, ifn_cf);
	if (d == NULL) {
	    if_dict
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	    if (if_dict == NULL)
		goto done;
	}
	else {
	    if_dict = CFDictionaryCreateMutableCopy(NULL, 0, d);
	    if (if_dict == NULL)
		goto done;
	}
    }
    state_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						   ifn_cf,
						   pkey);
    if (state_key == NULL) {
	goto done;
    }

    if (CFEqual(pkey, kSCEntNetIPv4)) {
	CFMutableDictionaryRef	dict = NULL;
	CFStringRef		router = NULL;
	CFDictionaryRef		setup_dict = NULL;
	CFStringRef		setup_key = NULL;
	
	serviceID_remove_ifname(ifn_cf);
	ipv4_dict = my_SCDGet(session, state_key);
	if (ipv4_dict == NULL) {
	    goto ipv4_done;
	}
	dict = CFDictionaryCreateMutableCopy(NULL, 0, ipv4_dict);
	my_CFRelease(&ipv4_dict);
	if (dict == NULL) {
	    goto ipv4_done;
	}
	serviceIDs = CFDictionaryGetValue(dict, 
					  kSCCachePropNetServiceIDs);
	serviceIDs = isA_CFArray(serviceIDs);
	if (serviceIDs == NULL || CFArrayGetCount(serviceIDs) == 0) {
	    goto ipv4_done;
	}
	serviceID_add_list(serviceIDs, ifn_cf);
	serviceID = highest_serviceID(serviceIDs, order);
	if (serviceID == NULL) {
	    goto ipv4_done;
	}
	setup_key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, 
						     serviceID, pkey);
	if (setup_key == NULL) {
	    goto ipv4_done;

	}
	setup_dict = my_SCDGet(session, setup_key);
	if (setup_dict) {
	    router = CFDictionaryGetValue(setup_dict,
					  kSCPropNetIPv4Router);
	    if (router) {
		CFDictionarySetValue(dict,
				     kSCPropNetIPv4Router,
				     router);
	    }

	}
	my_CFRelease(&setup_dict);

    ipv4_done:
	my_CFRelease(&setup_key);
	prot_dict = dict;
    }
    else {
	ipv4_dict = CFDictionaryGetValue(if_dict, kSCEntNetIPv4);
	if (ipv4_dict == NULL) {
	    goto else_done;
	}
	serviceIDs = CFDictionaryGetValue(ipv4_dict, 
					  kSCCachePropNetServiceIDs);
	serviceIDs = isA_CFArray(serviceIDs);
	if (serviceIDs)
	    serviceID = highest_serviceID(serviceIDs, order);

	if (CFEqual(pkey, kSCEntNetDNS)) {
	    CFMutableDictionaryRef	dict = NULL;
	    boolean_t			got_info = FALSE;
	    int				i;
	    CFTypeRef			list[] = {
		kSCPropNetDNSServerAddresses,
		kSCPropNetDNSSearchDomains,
		kSCPropNetDNSDomainName,
		NULL,
	    };
	    CFDictionaryRef		setup_dict = NULL;
	    CFDictionaryRef		state_dict = NULL;
	    CFStringRef			setup_key = NULL;
	    
	    state_dict = my_SCDGet(session, state_key);
	    if (serviceID) {
		setup_key 
		    = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
						       serviceID, pkey);
		if (setup_key) {
		    setup_dict = my_SCDGet(session, setup_key);
		}
	    }
	    if (state_dict == NULL && setup_dict == NULL) {
		goto dns_done;
	    }
	    dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	    if (dict == NULL) {
		goto dns_done;
	    }
	    for (i = 0; list[i]; i++) {
		CFTypeRef	val = NULL;
		if (setup_dict) {
		    val = CFDictionaryGetValue(setup_dict, list[i]);
		}
		if (val == NULL && state_dict) {
		    val = CFDictionaryGetValue(state_dict, list[i]);
		}
		if (val) {
		    got_info = TRUE;
		    CFDictionarySetValue(dict, list[i], val);
		}
	    }
	    if (got_info) {
		CFRetain(dict);
		prot_dict = dict;
	    }
	dns_done:
	    my_CFRelease(&dict);
	    my_CFRelease(&setup_key);
	    my_CFRelease(&setup_dict);
	    my_CFRelease(&state_dict);
	}
	else if (CFEqual(pkey, kSCEntNetNetInfo)) {
	    CFDictionaryRef	setup_dict = NULL;
	    CFStringRef		setup_key = NULL;
	    
	    if (serviceID) {
		setup_key 
		    = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
						       serviceID, pkey);
		if (setup_key) {
		    setup_dict = my_SCDGet(session, setup_key);
		}
	    }
	    if (setup_dict == NULL) {
		goto netinfo_done;
	    }
	    prot_dict = make_netinfo_dict(session, state_key, 
					  ipv4_dict, setup_dict);
	netinfo_done:
	    my_CFRelease(&setup_dict);
	    my_CFRelease(&setup_key);
	}
	else {
	    CFDictionaryRef		setup_dict = NULL;
	    CFStringRef			setup_key = NULL;
	    
	    if (serviceID) {
		setup_key 
		    = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
						       serviceID, pkey);
		if (setup_key) {
		    setup_dict = my_SCDGet(session, setup_key);
		}
	    }
	    /* if there's nothing defined in setup, try state */
	    if (setup_dict == NULL) {
		prot_dict = my_SCDGet(session, state_key);
	    }
	    else {
		prot_dict = setup_dict;
	    }
	    my_CFRelease(&setup_key);
	}
    }
 else_done:
    if (prot_dict == NULL) {
	CFDictionaryRef	old = CFDictionaryGetValue(if_dict, pkey);
	if (old) {
	    if (S_debug) {
		SCDLog(LOG_INFO, CFSTR("removed %@ dictionary = %@"),
		       pkey, old);
	    }
	    CFDictionaryRemoveValue(if_dict, pkey);
	    something_changed = TRUE;
	}
	*dict = NULL;
    }
    else {
	CFDictionaryRef	old = CFDictionaryGetValue(if_dict, pkey);

	if (old == NULL || CFEqual(prot_dict, old) == FALSE) {
	    if (S_debug) {
		SCDLog(LOG_INFO, CFSTR("%@ dictionary\nold %@\nnew %@"),
		       pkey, old, prot_dict);
	    }
	    CFDictionarySetValue(if_dict, pkey, prot_dict);
	    something_changed = TRUE;
	    *dict = prot_dict;
	}
	else {
	    *dict = old;
	}
    }
    CFDictionarySetValue(S_ifstate_dict, ifn_cf, if_dict);
 done:
    my_CFRelease(&if_dict);
    my_CFRelease(&prot_dict);
    my_CFRelease(&state_key);
    return (something_changed);
}

static boolean_t
default_route(int cmd, struct in_addr router)
{
    int sockfd;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
    } rtmsg;
    int rtm_seq = 0;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	SCDLog(LOG_INFO,
	       CFSTR("default_route: open routing socket failed, %s"),
	       strerror(errno));
	return (FALSE);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr = router;
    rtmsg.mask.sin_len = sizeof(rtmsg.dst);
    rtmsg.mask.sin_family = AF_INET;

    rtmsg.hdr.rtm_msglen = sizeof(rtmsg);
    if (write(sockfd, &rtmsg, sizeof(rtmsg)) < 0) {
	SCDLog(LOG_INFO,
	       CFSTR("default_route: write routing socket failed, %s"),
	       strerror(errno));
	close(sockfd);
	return (FALSE);
    }

    close(sockfd);
    return (TRUE);
}

static boolean_t
default_route_delete()
{
    struct in_addr ip_zeroes = { 0 };
    return (default_route(RTM_DELETE, ip_zeroes));
}

static boolean_t
default_route_add(struct in_addr router)
{
    return (default_route(RTM_ADD, router));
}


static __inline__ void
remove_router_key(SCDSessionRef session)
{
    (void)SCDRemove(session, S_state_global_ipv4);
    return;
}

static void
set_router(struct in_addr router)
{
    /* assign the new default route */
    (void)default_route_delete();
    (void)default_route_add(router);
    return;
}

static __inline__ void
remove_dns_key(SCDSessionRef session)
{
    (void)SCDRemove(session, S_state_global_dns);
    return;
}

static __inline__ void
empty_dns()
{
    (void)unlink(VAR_RUN_RESOLV_CONF);
}

static void
remove_dns(SCDSessionRef session)
{
    empty_dns();
    remove_dns_key(session);
    return;
}

static void
empty_netinfo(SCDSessionRef session)
{
    if (S_empty_netinfo == FALSE) {
	(void)unlink(VAR_RUN_NICONFIG_LOCAL_XML);
    }
    else {
	int fd = open(VAR_RUN_NICONFIG_LOCAL_XML "-",
		      O_CREAT|O_TRUNC|O_WRONLY, 0644);
	if (fd >= 0) {
	    close(fd);
	    rename(VAR_RUN_NICONFIG_LOCAL_XML "-", VAR_RUN_NICONFIG_LOCAL_XML);
	}
    }

    return;
}

static __inline__ void
remove_netinfo_key(SCDSessionRef session)
{
    (void)SCDRemove(session, S_state_global_netinfo);
    return;
}

static __inline__ void
remove_proxies_key(SCDSessionRef session)
{
    (void)SCDRemove(session, S_state_global_proxies);
    return;
}

static void
remove_netinfo(SCDSessionRef session)
{
    empty_netinfo(session);
    remove_netinfo_key(session);
    return;
}

static void
set_dns(CFArrayRef val_search_domains, CFStringRef val_domain_name,
	CFArrayRef val_servers)
{
    FILE * f = fopen(VAR_RUN_RESOLV_CONF "-", "w");

    /* publish new resolv.conf */
    if (f) {
	int i;

	if (val_domain_name) {
	    char 	domain_name[256];

	    domain_name[0] = '\0';
	    cfstring_to_cstring(val_domain_name, domain_name,
				sizeof(domain_name));
	    fprintf(f, "domain %s\n", domain_name);
	}

	if (val_search_domains) {
	    char 	domain_name[256];

	    fprintf(f, "search");
	    for (i = 0; i < CFArrayGetCount(val_search_domains); i++) {
		cfstring_to_cstring(CFArrayGetValueAtIndex(val_search_domains, i),
				    domain_name, sizeof(domain_name));
		fprintf(f, " %s", domain_name);
	    }
	    fprintf(f, "\n");
	}

	if (val_servers) {
	    for (i = 0; i < CFArrayGetCount(val_servers); i++) {
		struct in_addr	server;
		server = cfstring_to_ip(CFArrayGetValueAtIndex(val_servers,
							       i));
		fprintf(f, "nameserver " IP_FORMAT "\n",
			IP_LIST(&server));
	    }
	}
	fclose(f);
	rename(VAR_RUN_RESOLV_CONF "-", VAR_RUN_RESOLV_CONF);
    }
    return;
}

static void
set_netinfo(CFDictionaryRef dict)
{
    int fd = open(VAR_RUN_NICONFIG_LOCAL_XML "-",
		  O_CREAT|O_TRUNC|O_WRONLY, 0644);
    if (fd >= 0) {
	/* publish new netinfo config */
	CFDataRef	contents;
	contents = CFPropertyListCreateXMLData(NULL, dict);
	if (contents) {
	    CFIndex	len = CFDataGetLength(contents);

	    write(fd, CFDataGetBytePtr(contents), len);
	}
	close(fd);
	rename(VAR_RUN_NICONFIG_LOCAL_XML "-", VAR_RUN_NICONFIG_LOCAL_XML);
    }
    return;
}

static void
remove_global(SCDSessionRef session)
{
    /* update the information in the system */
    (void)default_route_delete();
    empty_dns();
    empty_netinfo(session);

    /* update the cache (atomically) */
    (void)SCDLock(session);
    remove_router_key(session);
    remove_dns_key(session);
    remove_netinfo_key(session);
    remove_proxies_key(session);
    (void)SCDUnlock(session);

    return;
}

static __inline__ SCDStatus
update_cache_key(SCDSessionRef session, CFStringRef key, SCDHandleRef data)
{
    (void)SCDRemove(session, key);
    if (data) {
	return (SCDAdd(session, key, data));
    }
    return (SCD_OK);
}

static void
update_global(SCDSessionRef session, CFStringRef primary,
	      boolean_t ipv4_changed, boolean_t dns_changed,
	      boolean_t netinfo_changed, boolean_t proxies_changed)
{
    SCDHandleRef		dns_data = NULL;
    CFDictionaryRef 		if_dict;
    SCDHandleRef		ipv4_data = NULL;
    SCDHandleRef		netinfo_data = NULL;
    SCDHandleRef		proxies_data = NULL;

    if_dict = CFDictionaryGetValue(S_ifstate_dict, primary);
    if (if_dict == NULL) {
	return;
    }

    if (ipv4_changed) {
	CFDictionaryRef		ipv4_dict = NULL;

	ipv4_dict = CFDictionaryGetValue(if_dict, kSCEntNetIPv4);

	if (ipv4_dict) {
	    CFMutableDictionaryRef 	dict = NULL;
	    CFStringRef			val_router = NULL;

	    dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	    if (dict == NULL) {
		goto done;
	    }
	    val_router = CFDictionaryGetValue(ipv4_dict, kSCPropNetIPv4Router);

	    /* if no router is defined, use the first interface address */
	    if (val_router == NULL) {
		CFArrayRef	arr;
		arr = CFDictionaryGetValue(ipv4_dict,
					   kSCPropNetIPv4Addresses);
		arr = isA_CFArray(arr);
		if (arr && CFArrayGetCount(arr) > 0) {
		    val_router = CFArrayGetValueAtIndex(arr, 0);
		    val_router = isA_CFString(val_router);
		}
	    }
	    
	    if (val_router) {
		CFDictionarySetValue(dict, kSCPropNetIPv4Router,
				     val_router);
	    }
	    CFDictionarySetValue(dict, kSCCachePropNetPrimaryInterface, 
				 primary);
	    ipv4_data = SCDHandleInit();
	    if (ipv4_data == NULL) {
		SCDLog(LOG_ERR, CFSTR("update_global: SCDHandleInit failed"));
		CFRelease(dict);
		goto done;
	    }
	    SCDHandleSetData(ipv4_data, dict);
	    CFRelease(dict);
	    /* route add default ... */
	    if (val_router) {
		set_router(cfstring_to_ip(val_router));
	    }
	}
	else {
	    (void)default_route_delete();
	}
    }
    if (dns_changed) {
	CFDictionaryRef		dict;

	dict = CFDictionaryGetValue(if_dict, kSCEntNetDNS);
	if (dict == NULL) {
	    empty_dns();
	}
	else {
	    set_dns(CFDictionaryGetValue(dict,
					 kSCPropNetDNSSearchDomains),
		    CFDictionaryGetValue(dict,
					 kSCPropNetDNSDomainName),
		    CFDictionaryGetValue(dict,
					 kSCPropNetDNSServerAddresses));
	    dns_data = SCDHandleInit();
	    if (dns_data == NULL) {
		SCDLog(LOG_ERR, CFSTR("update_global: SCDHandleInit failed"));
		goto done;
	    }
	    SCDHandleSetData(dns_data, dict);
	}
    }
    if (netinfo_changed) {
	CFDictionaryRef		dict = NULL;

	dict = CFDictionaryGetValue(if_dict, kSCEntNetNetInfo);
	if (dict) {
	    CFRetain(dict);
	}
	else {
	    CFDictionaryRef 	global;
	    CFDictionaryRef	ipv4_dict = NULL;

	    global = my_SCDGet(session, S_setup_global_netinfo);
	    ipv4_dict = CFDictionaryGetValue(if_dict, kSCEntNetIPv4);

	    if (global && ipv4_dict) {
		CFStringRef		state_key = NULL;

		state_key 
		    = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
							 primary,
							 kSCEntNetNetInfo);
		dict = make_netinfo_dict(session, state_key, ipv4_dict, 
					 global);
		my_CFRelease(&state_key);
	    }
	    my_CFRelease(&global);
	}

	if (dict == NULL) {
	    empty_netinfo(session);
	}
	else {
	    set_netinfo(dict);
	    netinfo_data = SCDHandleInit();
	    if (netinfo_data == NULL) {
		my_CFRelease(&dict);
		SCDLog(LOG_ERR, CFSTR("update_global: SCDHandleInit failed"));
		goto done;
	    }
	    SCDHandleSetData(netinfo_data, dict);
	    my_CFRelease(&dict);
	}
    }
    if (proxies_changed) {
	CFDictionaryRef		dict = NULL;

	dict = CFDictionaryGetValue(if_dict, kSCEntNetProxies);
	if (dict) {
	    CFRetain(dict);
	}
	else {
	    dict = my_SCDGet(session, S_setup_global_proxies);
	}
	if (dict) {
	    proxies_data = SCDHandleInit();
	    if (proxies_data == NULL) {
		my_CFRelease(&dict);
		SCDLog(LOG_ERR, CFSTR("update_global: SCDHandleInit failed"));
		goto done;
	    }
	    SCDHandleSetData(proxies_data, dict);
	    my_CFRelease(&dict);
	}
    }
    /* update the cache (atomically) */
    SCDLock(session);
    if (ipv4_changed) {
	(void)update_cache_key(session, S_state_global_ipv4, ipv4_data);
    }
    if (dns_changed) {
	(void)update_cache_key(session, S_state_global_dns, dns_data);
    }
    if (netinfo_changed) {
	(void)update_cache_key(session, S_state_global_netinfo, netinfo_data);
    }
    if (proxies_changed) {
	(void)update_cache_key(session, S_state_global_proxies, proxies_data);
    }
    SCDUnlock(session);
 done:
    my_SCDHandleRelease(&ipv4_data);
    my_SCDHandleRelease(&dns_data);
    my_SCDHandleRelease(&netinfo_data);
    my_SCDHandleRelease(&proxies_data);
    return;
}

static unsigned int
get_interface_rank(CFArrayRef arr, CFStringRef ifn_cf)
{
    CFIndex i;

    if (ifn_cf != NULL) {
	/* PPP override: make ppp* look the best */
	/* Hack: we should be using the type of the interface, not its name */
	if (CFStringHasPrefix(ifn_cf, CFSTR("ppp"))
	    && S_ppp_override_primary == TRUE) {
	    return (0);
	}
	if (arr != NULL) {
	    for (i = 0; i < CFArrayGetCount(arr); i++) {
		CFStringRef ifname;
		CFStringRef serviceID = CFArrayGetValueAtIndex(arr, i);

		ifname = serviceID_get_ifname(serviceID);
		if (ifname && CFEqual(ifn_cf, ifname)) {
		    return (i + 1);
		}
	    }
	}
    }

    /* return an arbitrarily large number */
    return (1024 * 1024);
}

static CFStringRef
elect_new_primary(SCDSessionRef session, CFArrayRef order)
{  
    CFIndex		count = CFDictionaryGetCount(S_ifstate_dict);
    CFIndex 		i;
    void * *		keys;
    CFStringRef		new_primary = NULL;
    unsigned int 	primary_index = 0;
    void * *		values;

    if (count == 0) {
	return (NULL);
    }

    keys = (void * *)malloc(sizeof(void *) * count);
    values = (void * *)malloc(sizeof(void *) * count);

    if (keys == NULL || values == NULL) {
	goto done;
    }

    CFDictionaryGetKeysAndValues(S_ifstate_dict, keys, values);

    for (i = 0; i < count; i++) {
	CFStringRef		ifn_cf = keys[i];
	CFDictionaryRef		if_dict = values[i];
	unsigned int		if_index;
	CFDictionaryRef 	ipv4_dict = NULL;

	if (CFEqual(ifn_cf, CFSTR("lo0"))) {
	    /* don't bother with loopback */
	    continue;
	}

	ipv4_dict = CFDictionaryGetValue(if_dict, kSCEntNetIPv4);
	if (ipv4_dict == NULL)
	    continue;

	if (ipv4_dict) {
	    struct in_addr	addr = { 0 };
	    CFArrayRef 		arr;

	    arr = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
						   kSCPropNetIPv4Addresses));
	    if (arr && CFArrayGetCount(arr))
		addr = cfstring_to_ip(CFArrayGetValueAtIndex(arr, 0));
	    if (addr.s_addr == 0) {
		if (S_debug) {
		    SCDLog(LOG_INFO, CFSTR("%@ has no address, ignoring"),
			   ifn_cf);
		}
		continue;
	    }
	}
	if_index = get_interface_rank(order, ifn_cf);
	if (new_primary == NULL || if_index < primary_index) {
	    my_CFRelease(&new_primary);
	    CFRetain(ifn_cf);
	    new_primary = ifn_cf;
	    primary_index = if_index;
	}
    }
 done:
    if (values)
	free(values);
    if (keys)
	free(keys);
    return (new_primary);
}

static void
ip_handle_change(SCDSessionRef session, CFArrayRef order, CFStringRef ifn_cf)
{
    boolean_t		dns_changed = FALSE;
    CFDictionaryRef	dns_dict = NULL;
    unsigned int	if_index = -1;
    boolean_t		ipv4_changed = FALSE;
    CFDictionaryRef 	ipv4_dict = NULL;
    boolean_t		ni_changed = FALSE;
    CFDictionaryRef 	ni_dict = NULL;
    boolean_t		proxies_changed = FALSE;
    CFDictionaryRef 	proxies_dict = NULL;
    CFArrayRef		val_dns_servers = NULL;

    if (CFEqual(ifn_cf, CFSTR("lo0"))) {
	/* don't bother with loopback */
	return;
    }
    ipv4_changed
	= get_changes(session, ifn_cf, kSCEntNetIPv4, order, &ipv4_dict);

    if_index = get_interface_rank(order, ifn_cf);
    if (ipv4_dict) {
	struct in_addr		addr = { 0 };
	CFArrayRef 		arr;

	if (S_debug) {
	    SCDLog(LOG_INFO, CFSTR("IPv4 %@ = %@"),
		   ifn_cf, ipv4_dict);
	}
	arr = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
					       kSCPropNetIPv4Addresses));
	if (arr && CFArrayGetCount(arr))
	    addr = cfstring_to_ip(CFArrayGetValueAtIndex(arr, 0));

	if (addr.s_addr == 0) {
	    if (S_debug) {
		SCDLog(LOG_INFO, CFSTR("%@ has no IP address, ignoring"),
		       ifn_cf);
	    }
	    goto done;
	}
    }
    dns_changed = get_changes(session, ifn_cf, kSCEntNetDNS, order, &dns_dict);
    if (dns_dict) {
	if (S_debug) {
	    SCDLog(LOG_INFO, CFSTR("DNS %@ = %@"), ifn_cf, dns_dict);
	}
	val_dns_servers
	    = isA_CFArray(CFDictionaryGetValue(dns_dict,
					       kSCPropNetDNSServerAddresses));
    }
    ni_changed = get_changes(session, ifn_cf, kSCEntNetNetInfo, order, 
			     &ni_dict);
    if (ni_dict) {
	if (S_debug) {
	    SCDLog(LOG_INFO, CFSTR("NetInfo %@ = %@"), ifn_cf, ni_dict);
	}
    }
    proxies_changed = get_changes(session, ifn_cf, kSCEntNetProxies, order, 
				  &proxies_dict);
    if (proxies_dict) {
	if (S_debug) {
	    SCDLog(LOG_INFO, CFSTR("Proxies %@ = %@"), ifn_cf, proxies_dict);
	}
    }
    if (S_ifn_primary && CFEqual(S_ifn_primary, ifn_cf)) {
	/* currently primary */
	if (ipv4_changed == FALSE && dns_changed == FALSE
	    && ni_changed == FALSE && proxies_changed == FALSE)
	    goto done;
	if (ipv4_changed && ipv4_dict == NULL) {
	    if (S_debug) {
		SCDLog(LOG_INFO, CFSTR("%@ is no longer primary"), ifn_cf);
	    }
	    /* we're no longer primary */
	    my_CFRelease(&S_ifn_primary);
	    remove_global(session);
	    S_ifn_primary = elect_new_primary(session, order);
	    if (S_ifn_primary) {
		SCDLog(LOG_INFO, CFSTR("%@ is new primary"), S_ifn_primary);
		update_global(session, S_ifn_primary, TRUE, TRUE, TRUE, TRUE);
	    }
	}
	else {
	    update_global(session, S_ifn_primary, ipv4_changed,
			  dns_changed, ni_changed, proxies_changed);
	}
    }
    else {
	unsigned int	primary_index;

	primary_index = get_interface_rank(order, S_ifn_primary);

	if (ipv4_dict && (S_ifn_primary == NULL || if_index < primary_index)) {
	    if (S_debug) {
		SCDLog(LOG_INFO, CFSTR("%@ is new primary"), ifn_cf);
	    }
	    my_CFRelease(&S_ifn_primary);
	    CFRetain(ifn_cf);
	    S_ifn_primary = ifn_cf;
	    update_global(session, S_ifn_primary, TRUE, TRUE, TRUE, TRUE);
	}
    }
 done:
    return;
}


static CFArrayRef
get_service_order(SCDSessionRef session)
{
    CFArrayRef	 		order = NULL;
    CFNumberRef		 	ppp_override = NULL;
    int				ppp_val = TRUE;
    CFStringRef 		ipv4_key = NULL;
    CFDictionaryRef 		ipv4_dict = NULL;

    ipv4_key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, 
					       kSCEntNetIPv4);
    ipv4_dict = my_SCDGet(session, ipv4_key);
    if (ipv4_dict != NULL) {
	order = CFDictionaryGetValue(ipv4_dict, kSCPropNetServiceOrder);
	order = isA_CFArray(order);
	if (order) {
	    CFRetain(order);
	}
	/* get ppp override primary */
	ppp_override = CFDictionaryGetValue(ipv4_dict, 
					    kSCPropNetPPPOverridePrimary);
	ppp_override = isA_CFNumber(ppp_override);
	if (ppp_override != NULL) {
	    CFNumberGetValue(ppp_override, 
			     kCFNumberIntType, &ppp_val);
	}
	S_ppp_override_primary = (ppp_val != 0) ? TRUE : FALSE;
    }
    else {
	S_ppp_override_primary = TRUE;
    }

    my_CFRelease(&ipv4_key);
    my_CFRelease(&ipv4_dict);
    return (order);
}

static boolean_t
ip_handler(SCDSessionRef session, void * arg)
{
    CFStringRef		change;
    CFArrayRef		changes = NULL;
    CFIndex		count;
    static boolean_t	first = TRUE;
    boolean_t		flat_file_changed = FALSE;
    boolean_t		global_netinfo_changed = FALSE;
    boolean_t		global_proxies_changed = FALSE;
    int			i;
    CFMutableArrayRef	if_changes = NULL;
    CFStringRef		ifl_key = NULL;
    CFArrayRef		service_order = NULL;
    boolean_t		service_order_changed = FALSE;
    SCDStatus		status;

    status = SCDNotifierGetChanges(session, &changes);
    if (status != SCD_OK || changes == NULL) {
	return (TRUE);
    }
    count = CFArrayGetCount(changes);
    if (count == 0) {
	goto done;
    }
    service_order = get_service_order(session);
    if (S_debug) {
	SCDLog(LOG_INFO, CFSTR("ip_handler changes: %@ (%d)"), changes,
	       count);
	if (service_order) {
	    SCDLog(LOG_INFO, CFSTR("ip_handler service_order: %@ "), 
		   service_order);
	}
    }
    ifl_key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, 
					      kSCEntNetIPv4);
    if_changes = CFArrayCreateMutable(NULL, 0, 
				      &kCFTypeArrayCallBacks);
    if (ifl_key == NULL || if_changes == NULL)
	goto done;
    for (i = 0; i < count; i++) {
	change = CFArrayGetValueAtIndex(changes, i);
	if (CFEqual(change, ifl_key)) {
	    service_order_changed = TRUE;
	}
	else if (CFEqual(change, S_setup_global_netinfo)) {
	    global_netinfo_changed = TRUE;
	}
	else if (CFEqual(change, S_setup_global_proxies)) {
	    global_proxies_changed = TRUE;
	}
	else if (CFStringHasSuffix(change, CFSTR(USE_FLAT_FILES))) {
	    flat_file_changed = TRUE;
	}
	else if (CFStringHasPrefix(change, S_state_interface_prefix)) {
	    CFStringRef ifn = parse_component(change, 
					      S_state_interface_prefix);
	    if (ifn) {
		my_CFArrayAppendUniqueValue(if_changes, ifn);
		CFRelease(ifn);
	    }
	}
	else if (CFStringHasPrefix(change, S_setup_service_prefix)) {
	    CFStringRef serviceID = parse_component(change, 
						    S_setup_service_prefix);
	    if (serviceID) {
		CFStringRef ifn = serviceID_get_ifname(serviceID);
		if (ifn) {
		    my_CFArrayAppendUniqueValue(if_changes, ifn);
		}
		CFRelease(serviceID);
	    }
	}
    }
    if (flat_file_changed) {
	SCDHandleRef	data = NULL;
	CFStringRef	key;

	key = SCDKeyCreate(CFSTR("%@" USE_FLAT_FILES), kSCCacheDomainSetup);
	status = SCDGet(session, key, &data);
	my_CFRelease(&key);
	if (status == SCD_OK) {
	    S_empty_netinfo = FALSE;
	    my_SCDHandleRelease(&data);
	}
	else {
	    S_empty_netinfo = TRUE;
	}
    }
    if (first) {
	remove_netinfo(session);
	first = FALSE;
    }
    for (i = 0; i < CFArrayGetCount(if_changes); i++) {
	ip_handle_change(session, service_order, 
			 CFArrayGetValueAtIndex(if_changes, i));
    }

    if (service_order_changed && service_order) {
	CFStringRef new_primary;

	if (S_debug) {
	    SCDLog(LOG_INFO,
		   CFSTR("iphandler: the interface list changed"));
	}
	new_primary = elect_new_primary(session, service_order);
	if (new_primary) {
	    if (S_ifn_primary && CFEqual(new_primary, S_ifn_primary)) {
		if (S_debug) {
		    SCDLog(LOG_INFO, CFSTR("%@ is still primary"),
			   new_primary);
		}
		my_CFRelease(&new_primary);
	    }
	    else {
		my_CFRelease(&S_ifn_primary);
		S_ifn_primary = new_primary;
		remove_global(session);
		update_global(session, S_ifn_primary, TRUE, TRUE, TRUE, TRUE);
		if (S_debug) {
		    SCDLog(LOG_INFO, CFSTR("%@ is the new primary"),
			   S_ifn_primary);
		}
	    }
	}
    }

    if (global_netinfo_changed || global_proxies_changed) {
	if (S_ifn_primary) {
	    update_global(session, S_ifn_primary, FALSE, FALSE, 
			  global_netinfo_changed, global_proxies_changed);
	}
    }
 done:
    my_CFRelease(&ifl_key);
    my_CFRelease(&if_changes);
    my_CFRelease(&service_order);
    my_CFRelease(&changes);
    return (TRUE);
}


void
ip_plugin_init()
{
    CFStringRef 	entities[] = {
	kSCEntNetIPv4,
	kSCEntNetDNS,
	kSCEntNetNetInfo,
	kSCEntNetProxies,
	NULL,
    };
    int			i;
    CFStringRef		key;
     SCDSessionRef	session = NULL;
    SCDStatus 		status;

    S_state_global_ipv4    
	= SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainState, kSCEntNetIPv4);
    S_state_global_dns     
	= SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainState, kSCEntNetDNS);
    S_state_global_netinfo 
	= SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainState, 
					  kSCEntNetNetInfo);
    S_state_global_proxies 
	= SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainState, 
					  kSCEntNetProxies);
    S_setup_global_netinfo 
	= SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, 
					  kSCEntNetNetInfo);
    S_setup_global_proxies
	= SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, 
					  kSCEntNetProxies);
    S_state_interface_prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"),
					    kSCCacheDomainState, 
					    kSCCompNetwork,
					    kSCCompInterface);
    S_setup_service_prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"),
					    kSCCacheDomainSetup, 
					    kSCCompNetwork,
					    kSCCompService);
    S_ifstate_dict
      = CFDictionaryCreateMutable(NULL, 0,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
    S_service_ifname_dict
      = CFDictionaryCreateMutable(NULL, 0,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
    if (S_ifstate_dict == NULL
	|| S_service_ifname_dict == NULL
	|| S_state_global_ipv4 == NULL
	|| S_state_global_dns == NULL
	|| S_state_global_netinfo == NULL
	|| S_state_global_proxies == NULL
	|| S_setup_global_netinfo == NULL
	|| S_setup_global_proxies == NULL
	|| S_state_interface_prefix == NULL
	|| S_setup_service_prefix == NULL) {
	SCDLog(LOG_ERR,
	       CFSTR("ip_plugin_init: couldn't allocate cache keys"));
	return;
    }
    status = SCDOpen(&session, CFSTR("IPMonitor"));
    if (status != SCD_OK) {
	SCDLog(LOG_ERR, CFSTR("ip_plugin_init SCDOpen failed: %s"),
	       SCDError(status));
	return;
    }
    S_debug = SCDOptionGet(session, kSCDOptionDebug);
    /* add notifiers for any IPv4, DNS, or NetInfo changes in state or setup */
    for (i = 0; entities[i]; i++) {
	key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 kSCCompAnyRegex,
						 entities[i]);
	status = SCDNotifierAdd(session, key, kSCDRegexKey);
	CFRelease(key);
	key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
					       kSCCompAnyRegex,
					       entities[i]);
	status = SCDNotifierAdd(session, key, kSCDRegexKey);
	CFRelease(key);
    }

    /* add notifier for setup global netinfo */
    status = SCDNotifierAdd(session, S_setup_global_netinfo, 0);

    /* add notifier for ServiceOrder/PPPOverridePrimary changes for IPv4 */
    key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
    status = SCDNotifierAdd(session, key, 0);
    CFRelease(key);

    /* add notifier flat file */
    key = SCDKeyCreate(CFSTR("%@" USE_FLAT_FILES), kSCCacheDomainSetup);
    status = SCDNotifierAdd(session, key, 0);
    CFRelease(key);

    SCDNotifierInformViaCallback(session, ip_handler, NULL);
    remove_dns(session);

    return;
}

void
start(const char *bundleName, const char *bundleDir)
{
    ip_plugin_init();
    return;
}

