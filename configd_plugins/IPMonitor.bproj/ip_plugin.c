/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
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
 *
 * March 19, 2001	Dieter Siegmund (dieter@apple.com)
 * - use service state instead of interface state
 *
 * July 16, 2001	Allan Nathanson <ajn@apple.com>
 * - update to public SystemConfiguration.framework APIs
 *
 * August 28, 2001	Dieter Siegmund (dieter@apple.com)
 * - specify the interface name when installing the default route
 * - this ensures that default traffic goes to the highest priority
 *   service when multiple interfaces are configured to be on the same subnet
 * September 16, 2002	Dieter Siegmund (dieter@apple.com)
 * - don't elect a link-local service to be primary unless it's the only
 *   one that's available
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

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>	// for SCLog()

#define USE_FLAT_FILES	"UseFlatFiles"

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

/* debug output on/off */
static boolean_t 		S_debug = FALSE;

/* are we netbooted?  If so, don't touch the default route */
static boolean_t		S_netboot = FALSE;

/* dictionary to hold per-service state: key is the serviceID */
static CFMutableDictionaryRef	S_service_state_dict = NULL;

/* if set, a PPP interface overrides the primary */
static boolean_t		S_ppp_override_primary = TRUE;

/* if set, create empty netinfo config file instead of removing it */
static boolean_t		S_empty_netinfo = TRUE;

/* the current primary serviceID */
static CFStringRef		S_primary_serviceID = NULL;

static CFStringRef		S_state_global_ipv4 = NULL;
static CFStringRef		S_state_global_dns = NULL;
static CFStringRef		S_state_global_netinfo = NULL;
static CFStringRef		S_state_global_proxies = NULL;
static CFStringRef		S_state_service_prefix = NULL;
static CFStringRef		S_setup_global_ipv4 = NULL;
static CFStringRef		S_setup_global_netinfo = NULL;
static CFStringRef		S_setup_global_proxies = NULL;
static CFStringRef		S_setup_service_prefix = NULL;

#define VAR_RUN_RESOLV_CONF		"/var/run/resolv.conf"
#define VAR_RUN_NICONFIG_LOCAL_XML	"/var/run/niconfig_local.xml"

#ifndef KERN_NETBOOT
#define KERN_NETBOOT            40      /* int: are we netbooted? 1=yes,0=no */
#endif KERN_NETBOOT

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

static CFDictionaryRef
my_SCDCopy(SCDynamicStoreRef session, CFStringRef key)
{
    CFDictionaryRef 	dict;

    dict = SCDynamicStoreCopyValue(session, key);
    if (isA_CFDictionary(dict) == NULL) {
	my_CFRelease(&dict);
    }
    return dict;
}

static struct in_addr
cfstring_to_ip(CFStringRef str)
{
    char		buf[32];
    struct in_addr	ip = { 0 };
    CFIndex		l;
    int			n;
    CFRange		range;

    if (isA_CFString(str) == NULL)
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

static void
append_netinfo_broadcast_addresses(CFDictionaryRef netinfo_dict,
				   CFDictionaryRef ipv4_dict,
				   CFMutableArrayRef ni_addrs,
				   CFMutableArrayRef ni_tags)
{
    CFArrayRef		addrs;
    CFIndex		addrs_count;
    CFIndex		i;
    CFArrayRef		masks;
    CFIndex 		masks_count;
    CFStringRef		tag;

    tag = CFDictionaryGetValue(netinfo_dict,
			       kSCPropNetNetInfoBroadcastServerTag);
    tag = isA_CFString(tag);
    if (tag == NULL) {
	tag = kSCValNetNetInfoDefaultServerTag;
    }
    addrs = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
					     kSCPropNetIPv4Addresses));
    masks = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
					     kSCPropNetIPv4SubnetMasks));
    if (addrs == NULL || masks == NULL) {
	return;
    }
    masks_count = CFArrayGetCount(masks);
    addrs_count = CFArrayGetCount(addrs);
    if (addrs_count != masks_count) {
	return;
    }

    for (i = 0; i < addrs_count; i++) {
	struct in_addr	addr = { 0 };
	CFStringRef	broadcast = NULL;
	struct in_addr	mask = { 0 };

	addr = cfstring_to_ip(CFArrayGetValueAtIndex(addrs, i));
	mask = cfstring_to_ip(CFArrayGetValueAtIndex(masks, i));
	if (addr.s_addr && mask.s_addr) {
	    struct in_addr 		b;

	    b.s_addr = htonl(ntohl(addr.s_addr) | ~ntohl(mask.s_addr));
	    broadcast = CFStringCreateWithFormat(NULL, NULL,
						 CFSTR(IP_FORMAT),
						 IP_LIST(&b));
	    CFArrayAppendValue(ni_addrs, broadcast);
	    CFArrayAppendValue(ni_tags, tag);
	    my_CFRelease(&broadcast);
	}
    }
    return;
}

static CFDictionaryRef
make_netinfo_dict(SCDynamicStoreRef session,
		  CFStringRef state_key,
		  CFStringRef setup_key,
		  CFDictionaryRef ipv4_dict)
{
    boolean_t			has_manual = FALSE;
    boolean_t			has_broadcast = FALSE;
    boolean_t			has_dhcp = FALSE;
    CFIndex			i;
    CFArrayRef			m = NULL;
    CFMutableArrayRef		ni_addrs = NULL;
    CFMutableDictionaryRef	ni_dict = NULL;
    CFMutableArrayRef		ni_tags = NULL;
    CFDictionaryRef		setup_dict;

    setup_dict = my_SCDCopy(session, setup_key);
    if (setup_dict == NULL) {
	goto netinfo_done;
    }
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
	CFDictionaryRef		state_dict;

	state_dict = my_SCDCopy(session, state_key);
	if (state_dict) {
	    append_netinfo_arrays(state_dict, ni_addrs, ni_tags);
	}
	my_CFRelease(&state_dict);
    }
    if (has_manual) {
	append_netinfo_arrays(setup_dict, ni_addrs, ni_tags);
    }
    if (has_broadcast) {
	append_netinfo_broadcast_addresses(setup_dict, ipv4_dict,
					   ni_addrs, ni_tags);
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
    my_CFRelease(&setup_dict);
    return (ni_dict);
}

static boolean_t
get_changes(SCDynamicStoreRef session, CFStringRef serviceID,
	    CFStringRef pkey, CFArrayRef order, CFDictionaryRef * dict)
{
    CFDictionaryRef		prot_dict = NULL;
    CFMutableDictionaryRef	service_dict = NULL;
    boolean_t			something_changed = FALSE;
    CFDictionaryRef		setup_dict = NULL;
    CFStringRef			setup_key = NULL;
    CFDictionaryRef		state_dict = NULL;
    CFStringRef			state_key = NULL;

    { /* create a modifyable dictionary, a copy or a new one */
	CFDictionaryRef		d = NULL;
	d = CFDictionaryGetValue(S_service_state_dict, serviceID);
	if (d == NULL) {
	    service_dict
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	    if (service_dict == NULL)
		goto done;
	}
	else {
	    service_dict = CFDictionaryCreateMutableCopy(NULL, 0, d);
	    if (service_dict == NULL) {
		goto done;
	    }
	}
    }
    state_key
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainState,
						      serviceID,
						      pkey);
    setup_key
	= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      serviceID,
						      pkey);
    if (state_key == NULL || setup_key == NULL) {
	goto done;
    }

    if (CFEqual(pkey, kSCEntNetIPv4)) {
	CFMutableDictionaryRef	dict = NULL;
	CFStringRef		router = NULL;

	state_dict = my_SCDCopy(session, state_key);
	if (state_dict == NULL) {
	    goto ipv4_done;
	}
	setup_dict = my_SCDCopy(session, setup_key);
	dict = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);
	if (dict && setup_dict) {
	    router = CFDictionaryGetValue(setup_dict,
					  kSCPropNetIPv4Router);
	    if (router) {
		CFDictionarySetValue(dict,
				     kSCPropNetIPv4Router,
				     router);
	    }
	}
    ipv4_done:
	prot_dict = dict;
    }
    else {
	CFDictionaryRef		ipv4_dict;

	ipv4_dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv4);
	if (ipv4_dict == NULL) {
	    goto else_done;
	}
	if (CFEqual(pkey, kSCEntNetDNS)) {
	    CFMutableDictionaryRef	dict = NULL;
	    boolean_t			got_info = FALSE;
	    int				i;
	    CFTypeRef			list[] = {
		kSCPropNetDNSServerAddresses,
		kSCPropNetDNSSearchDomains,
		kSCPropNetDNSDomainName,
		kSCPropNetDNSSortList,
		NULL,
	    };

	    state_dict = my_SCDCopy(session, state_key);
	    setup_dict = my_SCDCopy(session, setup_key);

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
	    if (got_info == FALSE) {
		my_CFRelease(&dict);
	    }
	dns_done:
	    prot_dict = dict;
	}
	else if (CFEqual(pkey, kSCEntNetNetInfo)) {
	    prot_dict = make_netinfo_dict(session, state_key,
					  S_setup_global_netinfo, ipv4_dict);
	}
	else {
	    setup_dict = my_SCDCopy(session, setup_key);
	    if (setup_dict) {
		prot_dict = CFRetain(setup_dict);
	    }
	    else {
		state_dict = my_SCDCopy(session, state_key);
		if (state_dict) {
		    prot_dict = CFRetain(state_dict);
		}
	    }
	}
    }

 else_done:
    if (prot_dict == NULL) {
	CFDictionaryRef	old = CFDictionaryGetValue(service_dict, pkey);
	if (old) {
	    SCLog(S_debug, LOG_INFO, CFSTR("removed %@ dictionary = %@"),
		  pkey, old);
	    CFDictionaryRemoveValue(service_dict, pkey);
	    something_changed = TRUE;
	}
	*dict = NULL;
    }
    else {
	CFDictionaryRef	old = CFDictionaryGetValue(service_dict, pkey);

	if (old == NULL || CFEqual(prot_dict, old) == FALSE) {
	    SCLog(S_debug, LOG_INFO, CFSTR("%@ dictionary\nold %@\nnew %@"),
		  pkey,
		  (old != NULL) ? (CFTypeRef)old : (CFTypeRef)CFSTR("(NULL)"),
		  prot_dict);
	    CFDictionarySetValue(service_dict, pkey, prot_dict);
	    something_changed = TRUE;
	    *dict = prot_dict;
	}
	else {
	    *dict = old;
	}
    }
    CFDictionarySetValue(S_service_state_dict, serviceID, service_dict);
 done:
    my_CFRelease(&service_dict);
    my_CFRelease(&prot_dict);
    my_CFRelease(&setup_dict);
    my_CFRelease(&setup_key);
    my_CFRelease(&state_dict);
    my_CFRelease(&state_key);
    return (something_changed);
}

static boolean_t
route(int cmd, struct in_addr gateway, struct in_addr netaddr,
      struct in_addr netmask, char * ifname, boolean_t proxy_arp)
{
    boolean_t			default_route = (netaddr.s_addr == 0);
    int 			len;
    boolean_t			ret = TRUE;
    int 			rtm_seq = 0;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
	struct sockaddr_dl	link;
    } 				rtmsg;
    int 			sockfd = -1;

    if (default_route && S_netboot) {
	return (TRUE);
    }

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("route: open routing socket failed, %s"),
	      strerror(errno));
	return (FALSE);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    if (default_route) {
	if (proxy_arp) {
	    /* if we're doing proxy arp, don't set the gateway flag */
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
	rtmsg.link.sdl_len = sizeof(rtmsg.link);
	rtmsg.link.sdl_family = AF_LINK;
	rtmsg.link.sdl_nlen = strlen(ifname);
	rtmsg.hdr.rtm_addrs |= RTA_IFP;
	bcopy(ifname, rtmsg.link.sdl_data, rtmsg.link.sdl_nlen);
    }
    else {
	/* no link information */
	len -= sizeof(rtmsg.link);
    }
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
	if ((cmd == RTM_ADD) && (errno == EEXIST)) {
		// no sense complaining about a route which already exists
		;
	}
	else if ((cmd == RTM_DELETE) && (errno == ESRCH)) {
		// no sense complaining about a route which isn't there
		;
	}
	else {
	    SCLog(TRUE, LOG_DEBUG,
		  CFSTR("route: write routing socket failed, %s"),
		  strerror(errno));
	    ret = FALSE;
	}
    }

    close(sockfd);
    return (ret);
}

static boolean_t
default_route_delete()
{
    struct in_addr ip_zeros = { 0 };
    return (route(RTM_DELETE, ip_zeros, ip_zeros, ip_zeros, NULL, FALSE));
}

static boolean_t
default_route_add(struct in_addr router, char * ifname, boolean_t proxy_arp)
{
    struct in_addr ip_zeros = { 0 };
    return (route(RTM_ADD, router, ip_zeros, ip_zeros, ifname, proxy_arp));
}


static boolean_t
multicast_route_delete()
{
    struct in_addr gateway = { htonl(INADDR_LOOPBACK) };
    struct in_addr netaddr = { htonl(INADDR_UNSPEC_GROUP) };
    struct in_addr netmask = { htonl(IN_CLASSD_NET) };

    return (route(RTM_DELETE, gateway, netaddr, netmask, "lo0", FALSE));
}

static boolean_t
multicast_route_add()
{
    struct in_addr gateway = { htonl(INADDR_LOOPBACK) };
    struct in_addr netaddr = { htonl(INADDR_UNSPEC_GROUP) };
    struct in_addr netmask = { htonl(IN_CLASSD_NET) };

    return (route(RTM_ADD, gateway, netaddr, netmask, "lo0", FALSE));
}


static void
set_router(struct in_addr router, char * ifname, boolean_t proxy_arp)
{
    /* assign the new default route, ensure local multicast route available */
    (void)default_route_delete();
    if (router.s_addr) {
	(void)default_route_add(router, ifname, proxy_arp);
	(void)multicast_route_delete();
    }
    else {
	(void)multicast_route_add();
    }

    return;
}

static __inline__ void
empty_dns()
{
    (void)unlink(VAR_RUN_RESOLV_CONF);
}

static void
empty_netinfo(SCDynamicStoreRef session)
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

static void
set_dns(CFArrayRef val_search_domains,
	CFStringRef val_domain_name,
	CFArrayRef val_servers,
	CFArrayRef val_sortlist)
{
    FILE * f = fopen(VAR_RUN_RESOLV_CONF "-", "w");

    /* publish new resolv.conf */
    if (f) {
	int i;

	if (isA_CFString(val_domain_name)) {
	    char 	domain_name[256];

	    domain_name[0] = '\0';
	    cfstring_to_cstring(val_domain_name, domain_name,
				sizeof(domain_name));
	    fprintf(f, "domain %s\n", domain_name);
	}

	if (isA_CFArray(val_search_domains)) {
	    char 	domain_name[256];

	    fprintf(f, "search");
	    for (i = 0; i < CFArrayGetCount(val_search_domains); i++) {
		CFStringRef	domain;

		domain = CFArrayGetValueAtIndex(val_search_domains, i);
		if (isA_CFString(domain)) {
		    cfstring_to_cstring(domain, domain_name, sizeof(domain_name));
		    fprintf(f, " %s", domain_name);
		}
	    }
	    fprintf(f, "\n");
	}

	if (isA_CFArray(val_servers)) {
	    for (i = 0; i < CFArrayGetCount(val_servers); i++) {
		CFStringRef	nameserver;

		nameserver = CFArrayGetValueAtIndex(val_servers, i);
		if (isA_CFString(nameserver)) {
		    struct in_addr	server;

		    server = cfstring_to_ip(nameserver);
		    fprintf(f, "nameserver " IP_FORMAT "\n", IP_LIST(&server));
		}
	    }
	}

	if (isA_CFArray(val_sortlist)) {
	    char 	addrmask[256];

	    fprintf(f, "sortlist");
	    for (i = 0; i < CFArrayGetCount(val_sortlist); i++) {
		CFStringRef	address;

		address = CFArrayGetValueAtIndex(val_sortlist, i);
		if (isA_CFString(address)) {
		    cfstring_to_cstring(address, addrmask, sizeof(addrmask));
		    fprintf(f, " %s", addrmask);
		}
	    }
	    fprintf(f, "\n");
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
	    CFRelease(contents);
	}
	close(fd);
	rename(VAR_RUN_NICONFIG_LOCAL_XML "-", VAR_RUN_NICONFIG_LOCAL_XML);
    }
    return;
}

static void
remove_global(SCDynamicStoreRef session)
{
    struct in_addr ip_zeros = { 0 };
    CFMutableArrayRef remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* router */
    set_router(ip_zeros, NULL, FALSE);
    CFArrayAppendValue(remove, S_state_global_ipv4);

    /* dns */
    empty_dns();
    CFArrayAppendValue(remove, S_state_global_dns);

    /* netinfo */
    empty_netinfo(session);
    CFArrayAppendValue(remove, S_state_global_netinfo);

    /* proxies */
    CFArrayAppendValue(remove, S_state_global_proxies);

    /* update cache (atomically) */
    SCDynamicStoreSetMultiple(session, NULL, remove, NULL);
    CFRelease(remove);
    return;
}

boolean_t
router_is_our_address(CFStringRef router, CFArrayRef addr_list)
{
    int 		i;
    struct in_addr	r = cfstring_to_ip(router);

    for (i = 0; i < CFArrayGetCount(addr_list); i++) {
	struct in_addr	ip;

	ip = cfstring_to_ip(CFArrayGetValueAtIndex(addr_list, i));
	if (r.s_addr == ip.s_addr) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static void
update_global(SCDynamicStoreRef session, CFStringRef primary,
	      boolean_t ipv4_changed, boolean_t dns_changed,
	      boolean_t netinfo_changed, boolean_t proxies_changed)
{
    CFMutableArrayRef		keys_remove;
    CFMutableDictionaryRef	keys_set;
    CFDictionaryRef 		service_dict;

    service_dict = CFDictionaryGetValue(S_service_state_dict, primary);
    if (service_dict == NULL) {
	return;
    }

    keys_remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    keys_set    = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);

    if (ipv4_changed) {
	CFDictionaryRef		ipv4_dict = NULL;
	boolean_t		proxy_arp = FALSE;

	ipv4_dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv4);

	if (ipv4_dict) {
	    CFArrayRef			addrs = NULL;
	    CFMutableDictionaryRef 	dict = NULL;
	    CFStringRef			if_name = NULL;
	    char			ifn[IFNAMSIZ + 1] = { '\0' };
	    char *			ifn_p = NULL;
	    CFStringRef			val_router = NULL;

	    dict = CFDictionaryCreateMutable(NULL, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	    if (dict == NULL) {
		goto done;
	    }
	    val_router = CFDictionaryGetValue(ipv4_dict, kSCPropNetIPv4Router);

	    addrs = CFDictionaryGetValue(ipv4_dict,
					 kSCPropNetIPv4Addresses);
	    addrs = isA_CFArray(addrs);
	    if (addrs && CFArrayGetCount(addrs) > 0) {
		if (val_router == NULL) {
		    /* no router defined, use the first interface address */
		    val_router = CFArrayGetValueAtIndex(addrs, 0);
		    val_router = isA_CFString(val_router);
		    if (val_router)
			proxy_arp = TRUE;
		}
		else {
		    /* do proxy ARP if router is an interface address */
		    proxy_arp = router_is_our_address(val_router, addrs);
		}
	    }
	    if (val_router) {
		CFDictionarySetValue(dict, kSCPropNetIPv4Router,
				     val_router);
	    }
	    if_name = CFDictionaryGetValue(ipv4_dict, CFSTR("InterfaceName"));
	    if (if_name) {
		CFDictionarySetValue(dict,
				     kSCDynamicStorePropNetPrimaryInterface,
				     if_name);
		if (CFStringGetCString(if_name, ifn, sizeof(ifn),
				       kCFStringEncodingMacRoman)) {
		    ifn_p = ifn;
		}
	    }
	    CFDictionarySetValue(dict, CFSTR("PrimaryService"), primary);
	    CFDictionarySetValue(keys_set, S_state_global_ipv4, dict);
	    CFRelease(dict);
	    /* route add default ... */
	    if (val_router) {
		set_router(cfstring_to_ip(val_router), ifn_p, proxy_arp);
	    }
	}
	else {
	    struct in_addr ip_zeros = { 0 };

	    CFArrayAppendValue(keys_remove, S_state_global_ipv4);
	    set_router(ip_zeros, NULL, FALSE);
	}
    }
    if (dns_changed) {
	CFDictionaryRef		dict;

	dict = CFDictionaryGetValue(service_dict, kSCEntNetDNS);
	if (dict == NULL) {
	    empty_dns();
	    CFArrayAppendValue(keys_remove, S_state_global_dns);
	}
	else {
	    set_dns(CFDictionaryGetValue(dict, kSCPropNetDNSSearchDomains),
		    CFDictionaryGetValue(dict, kSCPropNetDNSDomainName),
		    CFDictionaryGetValue(dict, kSCPropNetDNSServerAddresses),
		    CFDictionaryGetValue(dict, kSCPropNetDNSSortList));
	    CFDictionarySetValue(keys_set, S_state_global_dns, dict);
	}
    }
    if (netinfo_changed) {
	CFDictionaryRef	dict = NULL;
	CFDictionaryRef	ipv4_dict = NULL;

	ipv4_dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv4);
	if (ipv4_dict) {
	    CFStringRef		state_key = NULL;

	    state_key
		= SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      primary,
							      kSCEntNetNetInfo);
	    dict = make_netinfo_dict(session, state_key,
				     S_setup_global_netinfo,
				     ipv4_dict);
	    my_CFRelease(&state_key);
	}
	if (dict == NULL) {
	    empty_netinfo(session);
	    CFArrayAppendValue(keys_remove, S_state_global_netinfo);
	}
	else {
	    set_netinfo(dict);
	    CFDictionarySetValue(keys_set, S_state_global_netinfo, dict);
	    my_CFRelease(&dict);
	}
    }
    if (proxies_changed) {
	CFDictionaryRef		dict = NULL;

	dict = CFDictionaryGetValue(service_dict, kSCEntNetProxies);
	if (dict) {
	    CFRetain(dict);
	}
	else {
	    dict = my_SCDCopy(session, S_setup_global_proxies);
	}
	if (dict == NULL) {
	    CFArrayAppendValue(keys_remove, S_state_global_proxies);
	}
	else {
	    CFDictionarySetValue(keys_set, S_state_global_proxies, dict);
	    my_CFRelease(&dict);
	}
    }
    /* update the cache (atomically) */
    SCDynamicStoreSetMultiple(session, keys_set, keys_remove, NULL);
 done:
    my_CFRelease(&keys_set);
    my_CFRelease(&keys_remove);
    return;
}

static unsigned int
get_service_rank(CFArrayRef arr, CFStringRef serviceID)
{
    CFDictionaryRef	d;
    CFIndex 		i;
    CFDictionaryRef	ipv4_dict;

    if (serviceID == NULL) {
	goto done;
    }
    d = CFDictionaryGetValue(S_service_state_dict, serviceID);
    if (d == NULL) {
	goto done;
    }

    ipv4_dict = CFDictionaryGetValue(d, kSCEntNetIPv4);
    if (ipv4_dict) {
	CFStringRef	if_name;
	CFNumberRef    	override = NULL;

	if_name = CFDictionaryGetValue(ipv4_dict, CFSTR("InterfaceName"));
	if (S_ppp_override_primary == TRUE
	    && if_name != NULL
	    && CFStringHasPrefix(if_name, CFSTR("ppp"))) {
	    /* PPP override: make ppp* look the best */
	    /* Hack: should use interface type, not interface name */
	    return (0);
	}
	/* check for the "OverridePrimary" property */
	override = CFDictionaryGetValue(ipv4_dict,
					CFSTR("OverridePrimary"));
	if (isA_CFNumber(override) != NULL) {
	    int		val = 0;

	    CFNumberGetValue(override,  kCFNumberIntType, &val);
	    if (val != 0) {
		return (0);
	    }
	}
    }

    if (serviceID != NULL && arr != NULL) {
	for (i = 0; i < CFArrayGetCount(arr); i++) {
	    CFStringRef s = isA_CFString(CFArrayGetValueAtIndex(arr, i));

	    if (s == NULL) {
		continue;
	    }
	    if (CFEqual(serviceID, s)) {
		return (i + 1);
	    }
	}
    }

 done:
    /* return an arbitrarily large number */
    return (1024 * 1024);
}

static __inline__ boolean_t
in_addr_is_linklocal(struct in_addr iaddr)
{
    return (IN_LINKLOCAL(ntohl(iaddr.s_addr)));
}

static CFStringRef
elect_new_primary(SCDynamicStoreRef session, CFArrayRef order)
{
    CFIndex		count;
    CFIndex 		i;
    const void * *	keys;
    boolean_t		new_is_linklocal = TRUE;
    CFStringRef		new_primary = NULL;
    unsigned int 	primary_index = 0;
    const void * *	values;

    count = CFDictionaryGetCount(S_service_state_dict);
    if (count == 0) {
	return (NULL);
    }

    keys   = (const void * *)malloc(sizeof(void *) * count);
    values = (const void * *)malloc(sizeof(void *) * count);

    if (keys == NULL || values == NULL) {
	goto done;
    }

    CFDictionaryGetKeysAndValues(S_service_state_dict, keys, values);

    for (i = 0; i < count; i++) {
	struct in_addr		addr = { 0 };
	CFArrayRef 		arr;
	CFDictionaryRef 	ipv4_dict = NULL;
	boolean_t		is_linklocal;
	CFStringRef		serviceID = keys[i];
	CFDictionaryRef		service_dict = values[i];
	unsigned int		service_index;

	ipv4_dict = CFDictionaryGetValue(service_dict, kSCEntNetIPv4);
	if (ipv4_dict == NULL) {
	    continue;
	}
	arr = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
					       kSCPropNetIPv4Addresses));
	if (arr && CFArrayGetCount(arr)) {
	    addr = cfstring_to_ip(CFArrayGetValueAtIndex(arr, 0));
	}
	if (addr.s_addr == 0) {
	    SCLog(S_debug, LOG_INFO, CFSTR("%@ has no address, ignoring"), serviceID);
	    continue;
	}
	is_linklocal = in_addr_is_linklocal(addr);
	if (is_linklocal && new_is_linklocal == FALSE) {
	    /* avoid making the link-local service primary */
	    continue;
	}
	service_index = get_service_rank(order, serviceID);
	if (new_primary == NULL || service_index < primary_index
	    || (is_linklocal == FALSE && new_is_linklocal == TRUE)) {
	    my_CFRelease(&new_primary);
	    CFRetain(serviceID);
	    new_primary = serviceID;
	    primary_index = service_index;
	    new_is_linklocal = is_linklocal;
	}
    }
 done:
    if (values)
	free(values);
    if (keys)
	free(keys);
    return (new_primary);
}

static boolean_t
ip_handle_change(SCDynamicStoreRef session, CFArrayRef order, CFStringRef serviceID)
{
    boolean_t		dns_changed = FALSE;
    CFDictionaryRef	dns_dict = NULL;
    boolean_t		election_needed = FALSE;
    boolean_t		ipv4_changed = FALSE;
    CFDictionaryRef 	ipv4_dict = NULL;
    boolean_t		ni_changed = FALSE;
    CFDictionaryRef 	ni_dict = NULL;
    boolean_t		proxies_changed = FALSE;
    CFDictionaryRef 	proxies_dict = NULL;
    unsigned int	service_index = -1;

    ipv4_changed
	= get_changes(session, serviceID, kSCEntNetIPv4, order, &ipv4_dict);

    service_index = get_service_rank(order, serviceID);
    if (ipv4_dict) {
	struct in_addr		addr = { 0 };
	CFArrayRef 		arr;

	SCLog(S_debug, LOG_INFO, CFSTR("IPv4 %@ = %@"), serviceID, ipv4_dict);
	arr = isA_CFArray(CFDictionaryGetValue(ipv4_dict,
					       kSCPropNetIPv4Addresses));
	if (arr && CFArrayGetCount(arr))
	    addr = cfstring_to_ip(CFArrayGetValueAtIndex(arr, 0));

	if (addr.s_addr == 0) {
	    SCLog(S_debug, LOG_INFO, CFSTR("%@ has no IP address"), serviceID);
	    ipv4_dict = NULL;
	}
    }
    dns_changed = get_changes(session, serviceID, kSCEntNetDNS, order,
			      &dns_dict);
    SCLog(S_debug && dns_dict, LOG_INFO, CFSTR("DNS %@ = %@"), serviceID, dns_dict);
    ni_changed = get_changes(session, serviceID, kSCEntNetNetInfo, order,
			     &ni_dict);
    SCLog(S_debug && ni_dict, LOG_INFO, CFSTR("NetInfo %@ = %@"), serviceID, ni_dict);
    proxies_changed = get_changes(session, serviceID, kSCEntNetProxies, order,
				  &proxies_dict);
    SCLog(S_debug && proxies_dict, LOG_INFO, CFSTR("Proxies %@ = %@"), serviceID, proxies_dict);
    if (S_primary_serviceID && CFEqual(S_primary_serviceID, serviceID)) {
	/* currently primary */
	if (ipv4_changed == FALSE && dns_changed == FALSE
	    && ni_changed == FALSE && proxies_changed == FALSE)
	    goto done;
	if (ipv4_dict) {
	    update_global(session, serviceID, ipv4_changed,
			  dns_changed, ni_changed, proxies_changed);
	}
	if (ipv4_changed) {
	    election_needed = TRUE;
	}
    }
    else {
	election_needed = TRUE;
    }
 done:
    return (election_needed);
}


static CFArrayRef
get_service_order(SCDynamicStoreRef session)
{
    CFArrayRef	 		order = NULL;
    CFNumberRef		 	ppp_override = NULL;
    int				ppp_val = TRUE;
    CFStringRef 		ipv4_key = NULL;
    CFDictionaryRef 		ipv4_dict = NULL;

    ipv4_key
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    ipv4_dict = my_SCDCopy(session, ipv4_key);
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
	    CFNumberGetValue(ppp_override, kCFNumberIntType, &ppp_val);
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

static void
ip_handler(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    CFStringRef		change;
    CFIndex		count;
    static boolean_t	first = TRUE;
    boolean_t		flat_file_changed = FALSE;
    boolean_t		global_ipv4_changed = FALSE;
    boolean_t		global_netinfo_changed = FALSE;
    boolean_t		global_proxies_changed = FALSE;
    int			i;
    CFMutableArrayRef	service_changes = NULL;
    CFArrayRef		service_order = NULL;

    count = CFArrayGetCount(changes);
    if (count == 0) {
	goto done;
    }
    service_order = get_service_order(session);
    SCLog(S_debug, LOG_INFO, CFSTR("ip_handler changes: %@ (%d)"), changes, count);
    SCLog(S_debug && service_order, LOG_INFO,
	  CFSTR("ip_handler service_order: %@ "), service_order);
    service_changes = CFArrayCreateMutable(NULL, 0,
					   &kCFTypeArrayCallBacks);
    if (service_changes == NULL)
	goto done;
    for (i = 0; i < count; i++) {
	change = CFArrayGetValueAtIndex(changes, i);
	if (CFEqual(change, S_setup_global_ipv4)) {
	    global_ipv4_changed = TRUE;
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
    if (flat_file_changed) {
	CFPropertyListRef	data = NULL;
	CFStringRef		key;

	key = SCDynamicStoreKeyCreate(NULL,
				      CFSTR("%@" USE_FLAT_FILES),
				      kSCDynamicStoreDomainSetup);
	data = SCDynamicStoreCopyValue(session, key);
	my_CFRelease(&key);
	if (data) {
	    S_empty_netinfo = FALSE;
	    CFRelease(data);
	}
	else {
	    S_empty_netinfo = TRUE;
	}
    }
    if (first) {
	/* initialize netinfo state */
	empty_netinfo(session);
	(void)SCDynamicStoreRemoveValue(session, S_state_global_netinfo);
	first = FALSE;
    }
    for (i = 0; i < CFArrayGetCount(service_changes); i++) {
	if (ip_handle_change(session, service_order,
			     CFArrayGetValueAtIndex(service_changes, i))
		== TRUE) {
	    global_ipv4_changed = TRUE;
	}
    }

    if (global_ipv4_changed && service_order) {
	CFStringRef new_primary;

	SCLog(S_debug, LOG_INFO,
	      CFSTR("iphandler: running service election"));
	new_primary = elect_new_primary(session, service_order);
	if (new_primary) {
	    if (S_primary_serviceID
		&& CFEqual(new_primary, S_primary_serviceID)) {
		SCLog(S_debug, LOG_INFO, CFSTR("%@ is still primary"),
		      new_primary);
		my_CFRelease(&new_primary);
	    }
	    else {
		my_CFRelease(&S_primary_serviceID);
		S_primary_serviceID = new_primary;
		remove_global(session);
		update_global(session, S_primary_serviceID,
			      TRUE, TRUE, TRUE, TRUE);
		SCLog(S_debug, LOG_INFO, CFSTR("%@ is the new primary"),
		      S_primary_serviceID);
	    }
	}
	else {
	    if (S_primary_serviceID) {
		SCLog(S_debug, LOG_INFO, CFSTR("%@ is no longer primary"),
		      S_primary_serviceID);
		my_CFRelease(&S_primary_serviceID);
		remove_global(session);
	    }
	}
    }

    if (global_netinfo_changed || global_proxies_changed) {
	if (S_primary_serviceID) {
	    update_global(session, S_primary_serviceID, FALSE, FALSE,
			  global_netinfo_changed, global_proxies_changed);
	}
    }
 done:
    my_CFRelease(&service_changes);
    my_CFRelease(&service_order);
    return;
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
    CFMutableArrayRef	keys = NULL;
    CFMutableArrayRef	patterns = NULL;
    CFRunLoopSourceRef	rls = NULL;
    SCDynamicStoreRef	session = NULL;

    if (S_netboot_root() != 0) {
	S_netboot = TRUE;
    }
    session = SCDynamicStoreCreate(NULL, CFSTR("IPMonitor"), ip_handler, NULL);
    if (session == NULL) {
	SCLog(TRUE, LOG_ERR, CFSTR("ip_plugin_init SCDynamicStoreCreate failed: %s"),
	      SCErrorString(SCError()));
	return;
    }
    S_state_global_ipv4
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetIPv4);
    S_state_global_dns
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetDNS);
    S_state_global_netinfo
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetNetInfo);
    S_state_global_proxies
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetProxies);
    S_setup_global_ipv4
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    S_setup_global_netinfo
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetNetInfo);
    S_setup_global_proxies
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetProxies);
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

    if (S_service_state_dict == NULL
	|| S_state_global_ipv4 == NULL
	|| S_state_global_dns == NULL
	|| S_state_global_netinfo == NULL
	|| S_state_global_proxies == NULL
	|| S_setup_global_ipv4 == NULL
	|| S_setup_global_netinfo == NULL
	|| S_setup_global_proxies == NULL
	|| S_state_service_prefix == NULL
	|| S_setup_service_prefix == NULL) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("ip_plugin_init: couldn't allocate cache keys"));
	return;
    }

    keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (keys == NULL || patterns == NULL) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("ip_plugin_init: couldn't allocate notification keys/patterns"));
	return;
    }

    /* add notifiers for any IPv4, DNS, or NetInfo changes in state or setup */
    for (i = 0; entities[i]; i++) {
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(
		NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, entities[i]);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(
		NULL, kSCDynamicStoreDomainSetup, kSCCompAnyRegex, entities[i]);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);
    }

    /* add notifier for setup global netinfo */
    CFArrayAppendValue(keys, S_setup_global_netinfo);

    /* add notifier for ServiceOrder/PPPOverridePrimary changes for IPv4 */
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(
		NULL, kSCDynamicStoreDomainSetup, kSCEntNetIPv4);
    CFArrayAppendValue(keys, key);
    CFRelease(key);

    /* add notifier flat file */
    key = SCDynamicStoreKeyCreate(
		NULL, CFSTR("%@" USE_FLAT_FILES), kSCDynamicStoreDomainSetup);
    CFArrayAppendValue(keys, key);
    CFRelease(key);

    if (!SCDynamicStoreSetNotificationKeys(session, keys, patterns)) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("ip_plugin_init SCDynamicStoreSetNotificationKeys failed: %s"),
	      SCErrorString(SCError()));
	goto done;
    }

    rls = SCDynamicStoreCreateRunLoopSource(NULL, session, 0);
    if (rls == NULL) {
	SCLog(TRUE, LOG_ERR,
	      CFSTR("ip_plugin_init SCDynamicStoreCreateRunLoopSource failed: %s"),
	      SCErrorString(SCError()));
	goto done;
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    /* initialize dns configuration */
    empty_dns();
    (void)SCDynamicStoreRemoveValue(session, S_state_global_dns);

  done:
    my_CFRelease(&keys);
    my_CFRelease(&patterns);
    my_CFRelease(&session);
    return;
}

void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    if (bundleVerbose) {
	S_debug = 1;
    }

    ip_plugin_init();
    return;
}


#ifdef  MAIN
int
main(int argc, char **argv)
{
    _sc_log     = FALSE;
    _sc_verbose = (argc > 1) ? TRUE : FALSE;

    load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
    CFRunLoopRun();
    /* not reached */
    exit(0);
    return 0;
}
#endif

