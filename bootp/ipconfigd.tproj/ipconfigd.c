/*
 * Copyright (c) 1999, 2000 Apple Computer, Inc. All rights reserved.
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
 * ipconfigd.c
 * - daemon that configures interfaces using manual settings, 
 *   manual with DHCP INFORM, BOOTP, or DHCP
 */
/* 
 * Modification History
 *
 * September, 1999 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 *
 * May 8, 2000		Dieter Siegmund (dieter@apple.com)
 * - re-architected to be event-driven to satisfy mobility
 *   requirements
 * - converted to use a single main configuration thread
 *   instead of a thread per interface
 * - removed dependency on objective C
 *
 * June 12, 2000	Dieter Siegmund (dieter@apple.com)
 * - converted to use CFRunLoop
 * - added ability to change the configuration on the fly, either by
 *   a configuration data change, or having the user send a command
 *   via ipconfig
 *
 * July 5, 2000		Dieter Siegmund (dieter@apple.com)
 * - added code to publish information with configd
 * - wrote common IP config module to read information from the cache,
 *   and update the default route, DNS, and netinfo parent(s) on the fly
 *
 * August 20, 2000 	Dieter Siegmund (dieter@apple.com)
 * - moved common IP config module to configd_plugins/IPMonitor.bproj
 * - added code to handle information from setup/cache only
 *
 * October 4, 2000	Dieter Siegmund (dieter@apple.com)
 * - added code to handle error cases and force the interface
 *   state to be ready to avoid hanging the system startup in case
 *   there is bad data in the cache
 *
 * November 20, 2000	Dieter Siegmund (dieter@apple.com)
 * - changed to use new preferences keys and service-based configuration
 */

#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/wait.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/sockio.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/bootp.h>
#import <arpa/inet.h>
#import <fcntl.h>
#import <paths.h>
#import <unistd.h>
#import <syslog.h>

#define SYSTEMCONFIGURATION_NEW_API
#import <SystemConfiguration/SystemConfiguration.h>

#import "rfc_options.h"
#import "dhcp_options.h"
#import "dhcp.h"
#import "interfaces.h"
#import "util.h"
#import "arp.h"
#import <net/if_types.h>
#import <net/if_media.h>

#import "ts_log.h"
#import "host_identifier.h"
#import "threadcompat.h"
#import "dhcplib.h"

#import "ipcfg.h"
#import "ipconfig_types.h"
#import "ipconfigd.h"
#import "server.h"
#import "timer.h"

#import "ipconfigd_globals.h"
#import "ipconfigd_threads.h"
#import "FDSet.h"

#import "dprintf.h"

#define USE_FLAT_FILES	"UseFlatFiles"

#define USER_ERROR			1
#define UNEXPECTED_ERROR 		2
#define TIMEOUT_ERROR			3

/* global variables */
u_short 			G_client_port = IPPORT_BOOTPC;
boolean_t 			G_exit_quick = FALSE;
u_short 			G_server_port = IPPORT_BOOTPS;
u_long				G_link_inactive_secs = LINK_INACTIVE_WAIT_SECS;
u_long				G_gather_secs = GATHER_TIME_SECS;
u_long				G_max_retries = MAX_RETRIES;
boolean_t 			G_must_broadcast = FALSE;
boolean_t			G_verbose = FALSE;
boolean_t			G_debug = FALSE;
bootp_session_t *		G_bootp_session = NULL;
FDSet_t *			G_readers = NULL;
arp_session_t * 		G_arp_session = NULL;

const unsigned char		G_rfc_magic[4] = RFC_OPTIONS_MAGIC;
const struct sockaddr		G_blank_sin = { sizeof(G_blank_sin), AF_INET };
const struct in_addr		G_ip_broadcast = { INADDR_BROADCAST };
const struct in_addr		G_ip_zeroes = { 0 };

/* local variables */
static ptrlist_t		S_ifstate_list;
static interface_list_t	*	S_interfaces = NULL;
static SCDSessionRef		S_scd_session = NULL;
static CFStringRef		S_setup_service_prefix = NULL;
static CFStringRef		S_state_interface_prefix = NULL;

#define PROP_SERVICEID		CFSTR("ServiceID")

static void
configuration_changed(SCDSessionRef session);

static boolean_t
handle_change(SCDSessionRef session, void * arg);


static __inline__ CFTypeRef
isA_CFType(CFTypeRef obj, CFTypeID type)
{
    if (obj == NULL)
	return (NULL);

    if (CFGetTypeID(obj) != type)
	return (NULL);
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

static boolean_t
S_same_subnet(struct in_addr ip1, struct in_addr ip2, struct in_addr mask)
{
    u_long m = ntohl(mask.s_addr);
    u_long val1 = ntohl(ip1.s_addr);
    u_long val2 = ntohl(ip2.s_addr);

    if ((val1 & m) != (val2 & m)) {
	return (FALSE);
    }
    return (TRUE);
}

static __inline__ void
unblock_startup(SCDSessionRef session)
{
    (void)SCDTouch(session, CFSTR("plugin:ipconfigd"));
}

int
inet_dgram_socket()
{
    return (socket(AF_INET, SOCK_DGRAM, 0));
}

int
inet_difaddr(int s, char * name, const struct in_addr * addr)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    if (addr) {
	ifr.ifr_addr = G_blank_sin;
	((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr = *addr;
    }
    return (ioctl(s, SIOCDIFADDR, &ifr));
}

int
inet_aifaddr(int s, char * name, const struct in_addr * addr, 
	     const struct in_addr * mask,
	     const struct in_addr * broadcast)
{
    struct ifaliasreq	ifra;

    bzero(&ifra, sizeof(ifra));
    strncpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
    if (addr) {
	ifra.ifra_addr = G_blank_sin;
	((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr = *addr;
    }
    if (mask) {
	ifra.ifra_mask = G_blank_sin;
	((struct sockaddr_in *)&ifra.ifra_mask)->sin_addr = *mask;
    }
    if (broadcast) {
	ifra.ifra_mask = G_blank_sin;
	((struct sockaddr_in *)&ifra.ifra_broadaddr)->sin_addr = *broadcast;
    }
    return (ioctl(s, SIOCAIFADDR, &ifra));
}

int
sifflags(int s, char * name, short flags)
{
    struct ifreq	ifr;

    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_flags = flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

IFState_t *
IFState_lookup_if(ptrlist_t * list, char * ifname)
{
    int i;

    for (i = 0; i < ptrlist_count(list); i++) {
	IFState_t *	element = ptrlist_element(list, i);
	if (strcmp(if_name(element->if_p), ifname) == 0)
	    return (element);
    }
    return (NULL);
    
}

static boolean_t
get_media_status(char * name, boolean_t * media_status);

static void
ifstate_update_media_status(IFState_t * ifstate) 
{
    ifstate->link.valid = get_media_status(if_name(ifstate->if_p),
					   &ifstate->link.active);
    return;
}

static IFState_t *
IFState_make_if(interface_t * if_p)
{
    IFState_t *   	ifstate;

    ifstate = IFState_lookup_if(&S_ifstate_list, if_name(if_p));
    if (ifstate == NULL) {
	ifstate = malloc(sizeof(*ifstate));
	if (ifstate == NULL) {
	    ts_log(LOG_ERR, "make_ifstate: malloc ifstate failed");
	    return (NULL);
	}
	bzero(ifstate, sizeof(*ifstate));
	ifstate->if_p = if_dup(if_p);
	ifstate->ifname = (void *)
	    CFStringCreateWithCString(NULL,
				      if_name(if_p),
				      kCFStringEncodingMacRoman);
	ifstate_update_media_status(ifstate);
	ifstate->our_addrs_start = if_inet_count(if_p);
	ifstate->method = ipconfig_method_none_e;
	ptrlist_add(&S_ifstate_list, ifstate);
    }
    return (ifstate);
}


#ifdef DEBUG
static void
dump_ifstate_list(ptrlist_t * list)
{
    int i;

    printf("-------start--------\n");
    for (i = 0; i < ptrlist_count(list); i++) {
	IFState_t *	element = ptrlist_element(list, i);
	printf("%s: %s\n", if_name(element->if_p), 
	       ipconfig_method_string(element->method));
    }
    printf("-------end--------\n");
    return;
}
#endif DEBUG

unsigned
count_params(dhcpol_t * options, u_char * tags, int size)
{
    int				i;
    int				rating = 0;

    for (i = 0; i < size; i++) {
	if (dhcpol_find(options, tags[i], NULL, NULL) != NULL)
	    rating++;
    }
    return (rating);
}

static void
ifstate_clear(IFState_t * ifstate)
{
    if (ifstate->published.msg) {
	free(ifstate->published.msg);
	ifstate->published.msg = NULL;
    }
    ifstate->published.ready = FALSE;
    dhcpol_free(&ifstate->published.options);
    if (ifstate->published.pkt) {
	free(ifstate->published.pkt);
    }
    ifstate->published.pkt = 0;
    ifstate->published.pkt_size = 0;

    return;
}

static void
ifstate_publish_clear(IFState_t * ifstate)
{
    ifstate_clear(ifstate);
    if (S_scd_session != NULL) {
	CFStringRef		key;

	/* ipv4 */
	key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 ifstate->ifname,
						 kSCEntNetIPv4);
        (void)SCDRemove(S_scd_session, key);
	my_CFRelease(&key);

	/* dns */
	key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 ifstate->ifname,
						 kSCEntNetDNS);
        (void)SCDRemove(S_scd_session, key);
	my_CFRelease(&key);

	/* netinfo */
	key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 ifstate->ifname,
						 kSCEntNetNetInfo);
        (void)SCDRemove(S_scd_session, key);
	my_CFRelease(&key);
    }
    return;
}

static boolean_t
ifstate_all_ready()
{
    boolean_t		all_ready = TRUE;
    int 		i;

    for (i = 0; i < ptrlist_count(&S_ifstate_list); i++) {
	IFState_t *	scan = ptrlist_element(&S_ifstate_list, i);
	
	if (scan->published.ready == FALSE) {
	    all_ready = FALSE;
	    break;
	}
    }
    if (all_ready) {
	unblock_startup(S_scd_session);
    }
    return (all_ready);
}

static boolean_t
cache_key_different(SCDSessionRef session, CFStringRef key, CFTypeRef value)
{
    SCDHandleRef	current;
    SCDStatus		status;
    boolean_t		ret = TRUE;

    status = SCDGet(session, key, &current);
    if (status == SCD_OK) {
	if (CFEqual(value, SCDHandleGetData(current))) {
	    ret = FALSE;
	}
	SCDHandleRelease(current);
    }
    return (ret);
}

void
ifstate_publish_success(IFState_t * ifstate, void * pkt, int pkt_size)
{
    int				address_count = 0;
    CFMutableArrayRef		array = NULL;
    boolean_t			dns_changed = TRUE;
    CFMutableDictionaryRef	dns_dict = NULL;
    CFStringRef			dns_key = NULL;
    u_char *			dns_domain = NULL;
    int				dns_domain_len = 0;
    struct in_addr *		dns_server = NULL;
    int				dns_server_len = 0;
    int				i;
    char *			host_name = NULL;
    int				host_name_len = 0;
    boolean_t			ipv4_changed = TRUE;
    CFMutableDictionaryRef	ipv4_dict = NULL;
    CFStringRef			ipv4_key = NULL;
    boolean_t			netinfo_changed = TRUE;
    CFMutableDictionaryRef	netinfo_dict = NULL;
    CFStringRef			netinfo_key = NULL;
    struct in_addr *		netinfo_addresses = NULL;
    int				netinfo_addresses_len = 0;
    u_char *			netinfo_tag = NULL;
    int				netinfo_tag_len = 0;
    struct completion_results *	pub;
    struct in_addr *		router = NULL;
    int				router_len = 0;

    ifstate_clear(ifstate);
    pub = &ifstate->published;
    pub->ready = TRUE;
    pub->status = ipconfig_status_success_e;

    if (pkt_size) {
	pub->pkt = malloc(pkt_size);
	if (pub->pkt == NULL) {
	    syslog(LOG_ERR, "ifstate_publish_success %s: malloc failed",
		   if_name(ifstate->if_p));
	    ifstate_all_ready();
	    return;
	}
	bcopy(pkt, pub->pkt, pkt_size);
	pub->pkt_size = pkt_size;
	(void)dhcpol_parse_packet(&pub->options, pub->pkt, 
				  pub->pkt_size, NULL);
    }
    if (S_scd_session == NULL) {
	/* configd is not running */
	return;
    }

    /* create the cache keys */
    ipv4_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						  ifstate->ifname,
						  kSCEntNetIPv4);
    dns_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 ifstate->ifname,
						 kSCEntNetDNS);
    netinfo_key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						     ifstate->ifname,
						     kSCEntNetNetInfo);
    ipv4_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    /* set the ip address array */
    array = CFArrayCreateMutable(NULL, if_inet_count(ifstate->if_p), 
				 &kCFTypeArrayCallBacks);
    for (i = ifstate->our_addrs_start; i < if_inet_count(ifstate->if_p); i++) {
	inet_addrinfo_t * 	if_inet = if_inet_addr_at(ifstate->if_p, i);
	CFStringRef		str;


	if (if_inet->addr.s_addr == 0)
	    continue;
	str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
				       IP_LIST(&if_inet->addr));
	if (str == NULL)
	    continue;
	CFArrayAppendValue(array, str);
	CFRelease(str);
	address_count++;
    }
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array);
    CFRelease(array);

    /* set the ip mask array */
    array = CFArrayCreateMutable(NULL, if_inet_count(ifstate->if_p), 
				 &kCFTypeArrayCallBacks);
    for (i = ifstate->our_addrs_start; i < if_inet_count(ifstate->if_p); i++) {
	inet_addrinfo_t * 	if_inet = if_inet_addr_at(ifstate->if_p, i);
	CFStringRef		str;

	if (if_inet->mask.s_addr == 0)
	    continue;

	str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
				       IP_LIST(&if_inet->mask));
	CFArrayAppendValue(array, str);
	CFRelease(str);
    }
    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4SubnetMasks, array);
    CFRelease(array);

    if (ifstate->serviceID) {
	array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(array, ifstate->serviceID);
	CFDictionarySetValue(ipv4_dict, kSCCachePropNetServiceIDs,
			     array);
	CFRelease(array);
    }

    /* find relevant options */
    host_name = (char *)
	dhcpol_find(&pub->options, 
		    dhcptag_host_name_e,
		    &host_name_len, NULL);
    router = (struct in_addr *)
	dhcpol_find(&pub->options, 
		    dhcptag_router_e,
		    &router_len, NULL);
    dns_server 	= (struct in_addr *)
	dhcpol_find(&pub->options, 
		    dhcptag_domain_name_server_e,
		    &dns_server_len, NULL);
    dns_domain = (u_char *) 
	dhcpol_find(&pub->options, 
		    dhcptag_domain_name_e,
		    &dns_domain_len, NULL);
    netinfo_addresses = (struct in_addr *)
	dhcpol_find(&pub->options, 
		    dhcptag_netinfo_server_address_e,
		    &netinfo_addresses_len, NULL);
    netinfo_tag = (u_char *)
	dhcpol_find(&pub->options, 
		    dhcptag_netinfo_server_tag_e,
		    &netinfo_tag_len, NULL);

    /* set the router */
    if (router) {
	CFStringRef		str;
	str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
				       IP_LIST(router));
	CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
	CFRelease(str);
    }
    /* set the hostname */
    if (host_name) {
	CFStringRef		str;
	str = CFStringCreateWithBytes(NULL, host_name,
				      host_name_len,
				      kCFStringEncodingMacRoman, 
				      FALSE);
	CFDictionarySetValue(ipv4_dict, CFSTR("Hostname"), str);
	CFRelease(str);
    }

    /* set the DNS */
    if (dns_server && dns_server_len >= sizeof(struct in_addr)) {
	dns_dict 
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	array = CFArrayCreateMutable(NULL, 
				     dns_server_len / sizeof(struct in_addr),
				     &kCFTypeArrayCallBacks);
	for (i = 0; i < (dns_server_len / sizeof(struct in_addr)); i++) {
	    CFStringRef		str;
	    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
					   IP_LIST(dns_server + i));
	    CFArrayAppendValue(array, str);
	    CFRelease(str);
	}
	CFDictionarySetValue(dns_dict, kSCPropNetDNSServerAddresses, 
			     array);
	CFRelease(array);

	if (dns_domain) {
	    CFStringRef		str;

	    str = CFStringCreateWithBytes(NULL, dns_domain, dns_domain_len, 
					  kCFStringEncodingMacRoman, FALSE);
	    CFDictionarySetValue(dns_dict, kSCPropNetDNSDomainName, str);
	    CFRelease(str);
	}
    }

    /* set the NetInfo server address/tag */
    if (netinfo_addresses 
	&& netinfo_addresses_len >= sizeof(struct in_addr)
	&& netinfo_tag) {
	int 		n = netinfo_addresses_len / sizeof(struct in_addr);
	CFStringRef	str;
	
	netinfo_dict 
	    = CFDictionaryCreateMutable(NULL, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
	/* server addresses - parallel array */
	array = CFArrayCreateMutable(NULL, n, &kCFTypeArrayCallBacks);
	for (i = 0; i < n; i++) {
	    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
					   IP_LIST(netinfo_addresses + i));
	    CFArrayAppendValue(array, str);
	    CFRelease(str);
	}
	CFDictionarySetValue(netinfo_dict, 
			     kSCPropNetNetInfoServerAddresses, array);
	CFRelease(array);

	/* server tags - parallel array */
	array = CFArrayCreateMutable(NULL, n, &kCFTypeArrayCallBacks);
	str = CFStringCreateWithBytes(NULL, netinfo_tag, netinfo_tag_len, 
				      kCFStringEncodingMacRoman, FALSE);
	for (i = 0; i < n; i++) {
	    CFArrayAppendValue(array, str);
	}
	CFRelease(str);
	CFDictionarySetValue(netinfo_dict, 
			     kSCPropNetNetInfoServerTags, array);
	CFRelease(array);

    }
    
    if (G_debug) {
	SCDLog(LOG_INFO, CFSTR("%@ = %@"), ipv4_key, ipv4_dict);
	if (dns_dict)
	    SCDLog(LOG_INFO, CFSTR("%@ = %@"), dns_key, dns_dict);
	if (netinfo_dict)
	    SCDLog(LOG_INFO, CFSTR("%@ = %@"), netinfo_key, netinfo_dict);
    }

    if (ipv4_dict) {
	if (cache_key_different(S_scd_session, ipv4_key, ipv4_dict)
	    == FALSE) {
	    my_CFRelease(&ipv4_dict);
	    ipv4_changed = FALSE;
	}
    }
    if (dns_dict) {
	if (cache_key_different(S_scd_session, dns_key, dns_dict)
	    == FALSE) {
	    my_CFRelease(&dns_dict);
	    dns_changed = FALSE;
	}
    }
    if (netinfo_dict) {
	if (cache_key_different(S_scd_session, netinfo_key, netinfo_dict)
	    == FALSE) {
	    my_CFRelease(&netinfo_dict);
	    netinfo_changed = FALSE;
	}
    }

    if (ipv4_changed || dns_changed || netinfo_changed) {
	/* update the system configuration cache */
	SCDHandleRef 	ipv4_data = NULL;
	SCDStatus	ipv4_status = SCD_OK;
	SCDHandleRef 	dns_data = NULL;
	SCDStatus 	dns_status = SCD_OK;
	SCDHandleRef 	netinfo_data = NULL;
	SCDStatus 	netinfo_status = SCD_OK;

	if (ipv4_dict) {
	    ipv4_data = SCDHandleInit();
	    SCDHandleSetData(ipv4_data, ipv4_dict);
	    my_CFRelease(&ipv4_dict);
	}

	if (dns_dict) {
	    dns_data = SCDHandleInit();
	    SCDHandleSetData(dns_data, dns_dict);
	    my_CFRelease(&dns_dict);
	}
	if (netinfo_dict) {
	    netinfo_data = SCDHandleInit();
	    SCDHandleSetData(netinfo_data, netinfo_dict);
	    my_CFRelease(&netinfo_dict);
	}

	/* update atomically to avoid needless notifications */
	SCDLock(S_scd_session);
	if (ipv4_changed) {
	    (void)SCDRemove(S_scd_session, ipv4_key);
	    if (ipv4_data) {
		ipv4_status = SCDAdd(S_scd_session, ipv4_key, ipv4_data);
	    }
	}
	if (dns_changed) {
	    (void)SCDRemove(S_scd_session, dns_key);
	    if (dns_data) {
		dns_status = SCDAdd(S_scd_session, dns_key, dns_data);
	    }
	}
	if (netinfo_changed) {
	    (void)SCDRemove(S_scd_session, netinfo_key);
	    if (netinfo_data) {
		netinfo_status = SCDAdd(S_scd_session, netinfo_key, 
					netinfo_data);
	    }
	}
	SCDUnlock(S_scd_session);

	if (ipv4_data && ipv4_status != SCD_OK) {
	    SCDLog(LOG_INFO, 
		   CFSTR("SCDAdd IPV4 %s returned: %s"), 
		   if_name(ifstate->if_p),
		   SCDError(ipv4_status));
	}
	if (dns_data && dns_status != SCD_OK) {
	    SCDLog(LOG_INFO, 
		   CFSTR("SCDAdd DNS %s returned: %s"), 
		   if_name(ifstate->if_p),
		   SCDError(dns_status));
	}
	if (netinfo_data && netinfo_status != SCD_OK) {
	    SCDLog(LOG_INFO, 
		   CFSTR("SCDAdd NetInfo %s returned: %s"), 
		   if_name(ifstate->if_p),
		   SCDError(netinfo_status));
	}
	my_SCDHandleRelease(&ipv4_data);
	my_SCDHandleRelease(&dns_data);
	my_SCDHandleRelease(&netinfo_data);
    }

    my_CFRelease(&ipv4_key);
    my_CFRelease(&dns_key);
    my_CFRelease(&netinfo_key);

    ifstate_all_ready();
    return;
}

void
ifstate_publish_failure(IFState_t * ifstate, ipconfig_status_t status,
			char * msg)
{
    ifstate_publish_clear(ifstate);

    ifstate->published.ready = TRUE;
    ifstate->published.status = status;
    if (msg)
	ifstate->published.msg = strdup(msg);
    syslog(LOG_DEBUG, "%s %s: status = '%s'",
	   ipconfig_method_string(ifstate->method),
	   if_name(ifstate->if_p), ipconfig_status_string(status));
    ifstate_all_ready();
    return;
}

static void
ifstate_set_ready(IFState_t * ifstate, ipconfig_status_t status)
{
    if (ifstate->published.ready == FALSE) {
	ifstate->published.ready = TRUE;
	ifstate->published.status = status;
    }
    ifstate_all_ready();
    return;
}

static void
arpcache_flush() 
{
    int		s = arp_get_routing_socket();

    if (s < 0)
	return;
    (void)arp_flush(s);
    close(s);
}

#if 0
static void
arpcache_delete_one(struct in_addr ip) 
{
    int		s = arp_get_routing_socket();

    if (s < 0)
	return;
    (void)arp_delete(s, &ip, FALSE);
    close(s);
}
#endif 0

int
inet_add(IFState_t * ifstate, const struct in_addr * ip, 
	 const struct in_addr * mask, const struct in_addr * broadcast)
{
    interface_t *	if_p = ifstate->if_p;
    int			ret = 0;
    int 		s = inet_dgram_socket();

    if (ip && mask) {
	ts_log(LOG_DEBUG, "inet_add: %s " IP_FORMAT " netmask " IP_FORMAT, 
	       if_name(if_p), IP_LIST(ip), IP_LIST(mask));
    }
    else {
	ts_log(LOG_DEBUG, "inet_add: %s " IP_FORMAT, 
	       if_name(if_p), IP_LIST(ip));
    }
    if (s < 0) {
	ret = errno;
	ts_log(LOG_ERR, 
	       "inet_add(%s): socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	int			i;
	inet_addrinfo_t 	info;

	if (inet_aifaddr(s, if_name(if_p), ip, mask, broadcast) < 0) {
	    ret = errno;
	    ts_log(LOG_DEBUG, "inet_add(%s) " 
		   IP_FORMAT " inet_aifaddr() failed, %s (%d)", if_name(if_p),
		   IP_LIST(ip), strerror(errno), errno);
	}
	if_setflags(if_p, if_flags(if_p) | IFF_UP);
	bzero(&info, sizeof(info));
	if (ip)
	    info.addr = *ip;
	if (mask)
	    info.mask = *mask;
	if (broadcast)
	    info.broadcast = *broadcast;
	i = if_inet_find_ip(if_p, info.addr);
	if (i != INDEX_BAD) {
	    if (i < ifstate->our_addrs_start) {
		ifstate->our_addrs_start--;
	    }
	}
	if_inet_addr_add(if_p, &info);
	close(s);
    }
    return (ret);
}

int
inet_remove(IFState_t * ifstate, const struct in_addr * ip)
{
    interface_t *	if_p = ifstate->if_p;
    int			ret = 0;
    int 		s = inet_dgram_socket();

    if (ip == NULL)
	return (FALSE);
    ts_log(LOG_DEBUG, "inet_remove: %s " IP_FORMAT, 
	   if_name(if_p), IP_LIST(ip));
    if (s < 0) {
	ret = errno;
	ts_log(LOG_DEBUG, 
	       "inet_remove (%s):socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }	
    else {
	if (inet_difaddr(s, if_name(if_p), ip) < 0) {
	    ret = errno;
	    ts_log(LOG_DEBUG, "inet_remove(%s) " 
		   IP_FORMAT " failed, %s (%d)", if_name(if_p),
		   IP_LIST(ip), strerror(errno), errno);
	}
	if_inet_addr_remove(if_p, *ip);
	close(s);
	if (ifstate->method == ipconfig_method_dhcp_e
	    || ifstate->method == ipconfig_method_bootp_e) {
	    arpcache_flush();
	}
    }
    return (ret);
}


void
usage(u_char * progname)
{
    fprintf(stderr, "useage: %s <options>\n"
	    "<options> are:\n"
	    "-g secs    : gather time in seconds [default 2]\n"
	    "-r count   : retry count [default: 2] \n",
	    progname);
    exit(USER_ERROR);
}


static __inline__ boolean_t
ifstate_all_done()
{
    int i;

    for (i = 0; i < ptrlist_count(&S_ifstate_list);  i++) {
	IFState_t * s = ptrlist_element(&S_ifstate_list, i);

	switch (s->method) {
	  case ipconfig_method_inform_e:
	  case ipconfig_method_dhcp_e:
	  case ipconfig_method_bootp_e:
	      if (s->published.ready == FALSE)
		  return (FALSE);
	      break;
	  default:
	      break;
	}
    }
    return (TRUE);
}

boolean_t
ifstate_get_option(IFState_t * ifstate, int option_code, void * option_data,
		   unsigned int * option_dataCnt)
{
    boolean_t ret = FALSE;

    switch (ifstate->method) {
      case ipconfig_method_inform_e:
      case ipconfig_method_dhcp_e:
      case ipconfig_method_bootp_e: {
	  void * data;
	  int	 len;
	  
	  if (ifstate->published.ready == FALSE 
	      || ifstate->published.pkt == NULL)
	      break; /* out of switch */
	  data = dhcpol_find(&ifstate->published.options, option_code,
			     &len, NULL);
	  if (data) {
	      if (len > *option_dataCnt) {
		  break; /* out of switch */
	      }
	      *option_dataCnt = len;
	      bcopy(data, option_data, *option_dataCnt);
	      ret = TRUE;
	  }
	  break;
      }
    default:
	break;
    } /* switch */
    return (ret);
}


int
get_if_count()
{
    return (ptrlist_count(&S_ifstate_list));
}

boolean_t 
get_if_name(int intface, char * name)
{
    boolean_t		ret = FALSE;
    IFState_t * 	s;

    s = ptrlist_element(&S_ifstate_list, intface);
    if (s) {
	strcpy(name, if_name(s->if_p));
	ret = TRUE;
    }
    return (ret);
}

boolean_t 
get_if_addr(char * name, u_int32_t * addr)
{
    int			i;
    boolean_t		ret = FALSE;

    for (i = 0; i < ptrlist_count(&S_ifstate_list);  i++) {
	IFState_t * s = ptrlist_element(&S_ifstate_list, i);
	if (strcmp(if_name(s->if_p), name) == 0) {
	    if (if_inet_valid(s->if_p)) {
		*addr = if_inet_addr(s->if_p).s_addr;
		ret = TRUE;
		break; /* out of for loop */
	    }
	}
    }
    return (ret);
}

boolean_t
get_if_option(char * name, int option_code, void * option_data, 
	      unsigned int * option_dataCnt)
{
    int 		i;
    boolean_t		ret = FALSE;

    for (i = 0; i < ptrlist_count(&S_ifstate_list);  i++) {
	IFState_t * s = ptrlist_element(&S_ifstate_list, i);
	boolean_t 	name_match = (strcmp(if_name(s->if_p), name) == 0);
	
	if (name[0] == '\0' || name_match) {
	    ret = ifstate_get_option(s, option_code, option_data,
				     option_dataCnt);
	    if (ret == TRUE || name_match)
		break; /* out of for */
	}
    } /* for */
    return (ret);
}

boolean_t
get_if_packet(char * name, void * packet_data, unsigned int * packet_dataCnt)
{
    int			i;
    boolean_t		ret = FALSE;

    for (i = 0; i < ptrlist_count(&S_ifstate_list);  i++) {
	IFState_t * s = ptrlist_element(&S_ifstate_list, i);
	if (strcmp(if_name(s->if_p), name) == 0) {
	    switch (s->method) {
	      case ipconfig_method_inform_e:
	      case ipconfig_method_dhcp_e:
	      case ipconfig_method_bootp_e: {
		  if (s->published.ready == FALSE || s->published.pkt == NULL)
		      break; /* out of switch */
		
		  if (s->published.pkt_size > *packet_dataCnt) {
		      ret = FALSE;
		      break;
		  }
		  *packet_dataCnt = s->published.pkt_size;
		  bcopy(s->published.pkt, packet_data, *packet_dataCnt);
		  ret = TRUE;
		  break; /* out of for */
	      }
	      default:
		  break;
	    } /* switch */
	    break; /* out of for */
	} /* if */
    } /* for */
    return (ret);
}

boolean_t
wait_if(char * name)
{
    return (FALSE);
}

void
wait_all()
{
    return;
}

static ipconfig_func_t *
lookup_func(ipconfig_method_t method)
{
    switch (method) {
      case ipconfig_method_inform_e: {
	  return inform_thread;
	  break;
      }
      case ipconfig_method_manual_e: {
	  return manual_thread;
	  break;
      }

      case ipconfig_method_dhcp_e: {
	  return dhcp_thread;
	  break;
      }

      case ipconfig_method_bootp_e: {
	  return bootp_thread;
	  break;
      }
      default:
	  break;
    }
    return (NULL);
}

static ipconfig_status_t
config_method_start(IFState_t * ifstate, ipconfig_method_t method,
		    ipconfig_method_data_t * data,
		    unsigned int data_len)
{
    start_event_data_t		start_data;
    ipconfig_func_t *		func;
    interface_t * 		if_p = ifstate->if_p;

    /*
     * The correct check is for broadcast interfaces, but
     * since bpf only works with ethernet currently, 
     * we make sure it's ethernet as well.
     */
    if ((if_flags(if_p) & IFF_BROADCAST) == 0
	|| (if_link_arptype(if_p) != ARPHRD_ETHER)) {
	switch (method) {
	  case ipconfig_method_inform_e:
	  case ipconfig_method_dhcp_e:
	  case ipconfig_method_bootp_e:
	      /* can't do DHCP/BOOTP over non-broadcast interfaces */
	      return (ipconfig_status_invalid_operation_e);
	  default:
	      break;
	}
    }
    func = lookup_func(method);
    if (func == NULL) {
	return (ipconfig_status_operation_not_supported_e);
    }
    start_data.config.data = data;
    start_data.config.data_len = data_len;
    return (*func)(ifstate, IFEventID_start_e, &start_data);
}

static ipconfig_status_t
config_method_change(IFState_t * ifstate, ipconfig_method_t method,
		    ipconfig_method_data_t * data,
		    unsigned int data_len, boolean_t * needs_stop)
{
    change_event_data_t		change_data;
    ipconfig_func_t *		func;
    ipconfig_status_t		status;

    *needs_stop = FALSE;

    if (method == ipconfig_method_none_e) {
	return (ipconfig_status_success_e);
    }
    func = lookup_func(method);
    if (func == NULL) {
	return (ipconfig_status_operation_not_supported_e);
    }
    change_data.config.data = data;
    change_data.config.data_len = data_len;
    change_data.needs_stop = FALSE;
    status = (*func)(ifstate, IFEventID_change_e, &change_data);
    *needs_stop = change_data.needs_stop;
    return (status);
}

static boolean_t
if_gifmedia(int sockfd, char * name, boolean_t * status)
{
    struct ifmediareq 	ifmr;
    boolean_t 		valid = FALSE;

    *status = FALSE;
    (void) memset(&ifmr, 0, sizeof(ifmr));
    (void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

    if (ioctl(sockfd, SIOCGIFMEDIA, (caddr_t)&ifmr) >= 0
	&& ifmr.ifm_count > 0
	&& ifmr.ifm_status & IFM_AVALID) {
	valid = TRUE;
	if (ifmr.ifm_status & IFM_ACTIVE)
	    *status = TRUE;
    }
    return (valid);
}

static ipconfig_status_t
config_method_stop(IFState_t * ifstate, ipconfig_method_t method)
{
    ipconfig_func_t *	func;

    my_CFRelease(&ifstate->serviceID);

    if (method == ipconfig_method_none_e) {
	return (ipconfig_status_success_e);
    }

    func = lookup_func(method);
    if (func == NULL) {
	return (ipconfig_status_internal_error_e);
    }
    (*func)(ifstate, IFEventID_stop_e, NULL);
    return (ipconfig_status_success_e);
}

static ipconfig_status_t
config_method_media(IFState_t * ifstate, link_status_t new_status)
{
    ipconfig_func_t *	func;

    ifstate->link = new_status;
    if (ifstate->method == ipconfig_method_none_e) {
	return (ipconfig_status_success_e);
    }

    func = lookup_func(ifstate->method);
    if (func == NULL) {
	return (ipconfig_status_internal_error_e);
    }
    (*func)(ifstate, IFEventID_media_e, NULL);
    return (ipconfig_status_success_e);
}

static boolean_t
serviceID_equal(void * val1, void * val2)
{
    CFStringRef str1 = val1;
    CFStringRef str2 = val2;

    if (val1 == val2)
	return (TRUE);

    if (val1 == NULL || val2 == NULL)
	return (FALSE);
    return (CFEqual(str1, str2));
}

ipconfig_status_t
set_if(char * name, ipconfig_method_t method,
       void * method_data, unsigned int method_data_len,
       void * serviceID)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFState_t *   	ifstate;
    ipconfig_status_t	status = ipconfig_status_success_e;

    if (G_debug)
	ts_log(LOG_INFO, "set %s %s", name, ipconfig_method_string(method));
    if (if_p == NULL) {
	ts_log(LOG_INFO, "set: unknown interface %s", name);
	return (ipconfig_status_interface_does_not_exist_e);
    }
    ifstate = IFState_make_if(if_p);
    if (ifstate == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    if (ifstate->method == method 
	&& serviceID_equal(serviceID, ifstate->serviceID) == TRUE) {
	boolean_t	needs_stop = FALSE;
	status = config_method_change(ifstate, method, method_data,
				      method_data_len, &needs_stop);
	if (status != ipconfig_status_success_e
	    || needs_stop == FALSE) {
	    ifstate_set_ready(ifstate, status);
	    return (status);
	}
    }
    status = config_method_stop(ifstate, ifstate->method);
    if (status != ipconfig_status_success_e) {
	ifstate_set_ready(ifstate, status);
	return (status);
    }
    ifstate->method = ipconfig_method_none_e;
    ifstate_publish_clear(ifstate);
    if (method != ipconfig_method_none_e) {
	status = config_method_start(ifstate, method,
				     method_data, method_data_len);
	ts_log(LOG_DEBUG, "status from %s was %s",
	       ipconfig_method_string(method), 
	       ipconfig_status_string(status));
	if (status == ipconfig_status_success_e) {
	    ifstate->method = method;
	    if (serviceID) {
		ifstate->serviceID = serviceID;
		CFRetain(ifstate->serviceID);
	    }
	}
	else {
	    /* 
	     * make sure that errors not handled by the
	     * configuration method mark the interface as
	     * ready as well
	     */
	    ifstate_set_ready(ifstate, status);
	}
    }
    return (status);
}

static boolean_t
get_media_status(char * name, boolean_t * media_status) 

{
    boolean_t	media_valid = FALSE;
    int		sockfd;
	    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	syslog(LOG_INFO, "get_media_status (%s): socket failed, %s", 
	       name, strerror(errno));
	return (FALSE);
    }
    media_valid = if_gifmedia(sockfd, name, media_status);
    close(sockfd);
    return (media_valid);
}

/*
 * Function: parse_component
 * Purpose:
 *   Given a string 'key' and a string prefix 'prefix',
 *   return the next component in the slash '/' separated
 *   key.  If no slash follows the prefix, return NULL.
 *
 * Examples:
 * 1. key = "a/b/c" prefix = "a/"
 *    returns "b"
 * 2. key = "a/b/c" prefix = "a/b/"
 *    returns NULL
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
	CFRelease(comp);
	return (NULL);
    }
    range.length = CFStringGetLength(comp) - range.location;
    CFStringDelete(comp, range);
    return (comp);
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

static CFStringRef
ipconfig_cfstring_from_method(ipconfig_method_t method)
{
    switch (method) {
	case ipconfig_method_bootp_e:
	    return (kSCValNetIPv4ConfigMethodBOOTP);
	case ipconfig_method_dhcp_e:
	    return (kSCValNetIPv4ConfigMethodDHCP);
	case ipconfig_method_inform_e:
	    return (kSCValNetIPv4ConfigMethodINFORM);
	case ipconfig_method_manual_e:
	    return (kSCValNetIPv4ConfigMethodManual);
        default:
	    break;
    }
    return (NULL);
}

static boolean_t
ipconfig_method_from_cfstring(CFStringRef m, ipconfig_method_t * method)
{
    if (CFEqual(m, kSCValNetIPv4ConfigMethodBOOTP)) {
	*method = ipconfig_method_bootp_e;
    }
    else if (CFEqual(m, kSCValNetIPv4ConfigMethodDHCP)) {
	*method = ipconfig_method_dhcp_e;
    }
    else if (CFEqual(m, kSCValNetIPv4ConfigMethodManual)) {
	*method = ipconfig_method_manual_e;
    }
    else if (CFEqual(m, kSCValNetIPv4ConfigMethodINFORM)) {
	*method = ipconfig_method_inform_e;
    }
    else {
	return (FALSE);
    }
    return (TRUE);
}

static ipconfig_method_data_t *
ipconfig_method_data_from_dict(CFDictionaryRef dict, 
			       ipconfig_method_t * method,
			       int * mdlen)
{
    CFArrayRef			addresses = NULL;
    CFStringRef			config_method;
    int				count = 0;
    CFStringRef			client_id = NULL;
    u_char			cid[255];
    int				cid_len = 0;
    int				i;
    CFArrayRef			masks = NULL;
    ipconfig_method_data_t *	method_data = NULL;
    int				method_data_len = 0;

    config_method = CFDictionaryGetValue(dict, 
					 kSCPropNetIPv4ConfigMethod);
    if (config_method == NULL 
	|| ipconfig_method_from_cfstring(config_method, method) == FALSE) {
	ts_log(LOG_ERR, "ipconfigd: configuration method is missing/invalid");
	goto error;
    }
    addresses = CFDictionaryGetValue(dict, kSCPropNetIPv4Addresses);
    masks = CFDictionaryGetValue(dict, kSCPropNetIPv4SubnetMasks);
    client_id = CFDictionaryGetValue(dict, 
				     kSCPropNetIPv4DHCPClientID);
    if (addresses) {
	count = CFArrayGetCount(addresses);
	if (count == 0) {
		ts_log(LOG_ERR, 
		       "ipconfigd: address array empty");
		goto error;
	}
	if (masks) {
	    if (count != CFArrayGetCount(masks)) {
		ts_log(LOG_ERR, 
		       "ipconfigd: address/mask arrays not same size");
		goto error;
	    }
	}
    }
    switch (*method) {
        case ipconfig_method_inform_e:
	    if (addresses == NULL) {
		ts_log(LOG_ERR, 
		       "ipconfigd: inform method requires address");
		goto error;
	    }
	    /* FALL THROUGH */
	case ipconfig_method_dhcp_e:
	    if (client_id) {
		cid_len = cfstring_to_cstring(client_id, cid, sizeof(cid));
	    }
	    break;
	case ipconfig_method_manual_e:
	    if (addresses == NULL || masks == NULL) {
		ts_log(LOG_ERR, 
		       "ipconfigd: manual method requires address and mask");
		goto error;
	    }
	    break;
       default:
	   break;
    }
    method_data_len = sizeof(*method_data) 
	+ (count * sizeof(method_data->ip[0])) + cid_len;
    method_data = (ipconfig_method_data_t *) malloc(method_data_len);
    if (method_data == NULL) {
	ts_log(LOG_ERR, 
	       "ipconfigd: malloc method_data failed");
	goto error;
    }
    *mdlen = method_data_len;
    bzero(method_data, method_data_len);
    method_data->n_ip = count;
    method_data->n_dhcp_client_id = cid_len;
    for (i = 0; i < count; i++) {
	if (addresses)
	    method_data->ip[i].addr
		= cfstring_to_ip(CFArrayGetValueAtIndex(addresses, i));
	if (masks)
	    method_data->ip[i].mask
		= cfstring_to_ip(CFArrayGetValueAtIndex(masks, i));
	if (G_debug) {
	    printf("%d. " IP_FORMAT " mask " IP_FORMAT "\n", i,
		   IP_LIST(&method_data->ip[i].addr),
		   IP_LIST(&method_data->ip[i].mask));
	}
    }
    if (cid && cid_len) {
	bcopy(cid, 
	      ((void *)method_data->ip) + count * sizeof(method_data->ip[0]),
	      cid_len);
	if (G_debug)
	    printf("DHCP Client ID '%s'\n", cid);
    }
    return (method_data);

 error:
    if (method_data)
	free(method_data);
    return (NULL);
}


static ipcfg_table_t *	S_ipcfg_list = NULL;
static hostconfig_t *	S_hostconfig = NULL;

static void
load_cache_from_iftab(SCDSessionRef session)
{
    int			i;
    CFMutableArrayRef	service_array = NULL;
    CFStringRef 	skey = NULL;
    struct in_addr	router = { 0 };

    if (S_hostconfig) {
#define AUTOMATIC	"-AUTOMATIC-"
	char * val;
	val = hostconfig_lookup(S_hostconfig, "HOSTNAME");
	if (val) {
	    if (G_debug)
		printf("HOSTNAME=%s\n", val);
	}
	val = hostconfig_lookup(S_hostconfig, "ROUTER");
	if (val) {
	    if (G_debug)
		printf("ROUTER=%s\n", val);
	    if (strcmp(val, AUTOMATIC) != 0)
		inet_aton(val, &router);
	}
    }
    if (G_debug) {
	ipcfg_print(S_ipcfg_list);
    }
    service_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < ipcfg_count(S_ipcfg_list); i++) {
	CFMutableDictionaryRef	interface_dict = NULL;
	CFMutableDictionaryRef	ipv4_dict = NULL;
	ipcfg_t *		ipcfg = ipcfg_element(S_ipcfg_list, i);
	CFStringRef		ipv4_key = NULL;
	CFStringRef		interface_key = NULL;
	CFStringRef		serviceID = NULL;
	CFStringRef		str;

	serviceID = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), i);
	if (serviceID == NULL) {
	    goto loop_done;
	}
	ipv4_key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
						    serviceID,
						    kSCEntNetIPv4);
	if (ipv4_key == NULL) {
	    goto loop_done;
	}
	interface_key 
	    = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
					       serviceID,
					       kSCEntNetInterface);
	if (interface_key == NULL) {
	    goto loop_done;
	}
	if (ipcfg == NULL || ipcfg->method == ipconfig_method_none_e) {
	    /* if it was there, remove it */
	    (void)SCDRemove(session, ipv4_key);
	    (void)SCDRemove(session, interface_key);
	}
	else {
	    CFStringRef 	ifn_cf;
	    interface_t * 	if_p = ifl_find_name(S_interfaces, 
						     ipcfg->ifname);

	    if (strcmp(ipcfg->ifname, "lo0") == 0) {
		goto loop_done;
	    }

	    /* 
	     * create the ifstate entry beforehand to make sure it exists
	     * so that at startup we wait for all interfaces that are present
	     */
	    if (if_p) {
		(void)IFState_make_if(if_p);
	    }

	    /* add serviceID in the order in which they are defined in iftab */
	    CFArrayAppendValue(service_array, serviceID);

	    /* create the cache entry for one interface */
	    ipv4_dict 
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	    if (ipv4_dict == NULL)
		goto loop_done;

	    CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4ConfigMethod,
				 ipconfig_cfstring_from_method(ipcfg->method));
	    switch (ipcfg->method) {
	      case ipconfig_method_manual_e:
		  if (router.s_addr 
		      && S_same_subnet(router, ipcfg->addr, ipcfg->mask)) {
		      str = CFStringCreateWithFormat(NULL, NULL, 
						     CFSTR(IP_FORMAT),
						     IP_LIST(&router));
		      CFDictionarySetValue(ipv4_dict, 
					   kSCPropNetIPv4Router, str);
		      CFRelease(str);
		  }
		  /* FALL THROUGH */
	      case ipconfig_method_inform_e: {
		  CFMutableArrayRef array;

		  /* set the ip address array */
		  array = CFArrayCreateMutable(NULL, 1,
					       &kCFTypeArrayCallBacks);
		  str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
						 IP_LIST(&ipcfg->addr));
		  CFArrayAppendValue(array, str);
		  CFRelease(str);
		  CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, 
				       array);
		  CFRelease(array);
		  /* set the ip mask array */
		  array = CFArrayCreateMutable(NULL, 1,
					       &kCFTypeArrayCallBacks);
		  str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
						 IP_LIST(&ipcfg->mask));
		  CFArrayAppendValue(array, str);
		  CFRelease(str);
		  CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4SubnetMasks, 
				       array);
		  CFRelease(array);
		  break;
	      }
	      default:
		  break;
	    }
	    interface_dict 
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	    if (interface_dict == NULL)
		goto loop_done;
	    ifn_cf = CFStringCreateWithCString(NULL, ipcfg->ifname,
					       kCFStringEncodingMacRoman);
	    CFDictionarySetValue(interface_dict, 
				 kSCPropNetInterfaceDeviceName, ifn_cf);
	    CFDictionarySetValue(interface_dict, 
				 kSCPropNetInterfaceType, 
				 kSCValNetInterfaceTypeEthernet);
	    my_CFRelease(&ifn_cf);
	    {
		SCDHandleRef	interface_data = SCDHandleInit();
		SCDHandleRef	ipv4_data = SCDHandleInit();

		SCDHandleSetData(ipv4_data, ipv4_dict);
		SCDHandleSetData(interface_data, interface_dict);
		SCDLock(session);
		(void)SCDRemove(session, ipv4_key);
		(void)SCDAdd(session, ipv4_key, ipv4_data);
		(void)SCDRemove(session, interface_key);
		(void)SCDAdd(session, interface_key, interface_data);
		SCDUnlock(session);
		my_SCDHandleRelease(&ipv4_data);
		my_SCDHandleRelease(&interface_data);
	    }
	}
    loop_done:
	my_CFRelease(&serviceID);
	my_CFRelease(&ipv4_key);
	my_CFRelease(&interface_key);
	my_CFRelease(&ipv4_dict);
	my_CFRelease(&interface_dict);
    }
    skey = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
    if (skey == NULL)
	goto done;
    if (CFArrayGetCount(service_array) == 0) {
	(void)SCDRemove(session, skey);
	/* no interfaces, startup is complete */
	unblock_startup(session);
    }
    else {
	SCDHandleRef		data = SCDHandleInit();
	CFMutableDictionaryRef	sdict;
	
	sdict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(sdict, kSCPropNetServiceOrder, service_array);
	SCDHandleSetData(data, sdict);
	my_CFRelease(&sdict);
	
	SCDLock(session);
	(void)SCDRemove(session, skey);
	(void)SCDAdd(session, skey, data);
	SCDUnlock(session);
	
	my_SCDHandleRelease(&data);
    }
 done:
    my_CFRelease(&skey);
    my_CFRelease(&service_array);
    return;
}

static CFArrayRef
entity_all(SCDSessionRef session, CFStringRef entity)
{
    CFMutableArrayRef		all = NULL;
    CFArrayRef			arr = NULL;
    int				i;
    CFStringRef			key = NULL;
    SCDStatus 			status;

    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
					   kSCCompAnyRegex,
					   entity);
    if (key == NULL) {
	goto done;
    }

    all = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (all == NULL) {
	goto done;
    }
    status = SCDList(session, key, kSCDRegexKey, &arr);
    if (status != SCD_OK || CFArrayGetCount(arr) == 0) {
	goto done;
    }
    for (i = 0; i < CFArrayGetCount(arr); i++) {
	CFDictionaryRef		if_dict = NULL;
	SCDHandleRef		if_handle = NULL;
	CFStringRef 		if_key = NULL;
	CFStringRef 		ifn_cf;
	CFMutableDictionaryRef	ent_dict = NULL;
	SCDHandleRef		ent_handle = NULL;
	CFStringRef 		ent_key = CFArrayGetValueAtIndex(arr, i);
	CFStringRef		serviceID = NULL;
	CFStringRef		type = NULL;
	
	serviceID = parse_component(ent_key, S_setup_service_prefix);
	if (serviceID == NULL) {
	    goto loop_done;
	}
	status = SCDGet(session, ent_key, &ent_handle);
	if (status != SCD_OK) {
	    goto loop_done;
	}
	ent_dict  = (CFMutableDictionaryRef) 
	  isA_CFDictionary(SCDHandleGetData(ent_handle));
	if (ent_dict == NULL) {
	    goto loop_done;
	}
	ent_dict = CFDictionaryCreateMutableCopy(NULL, 0,
						 ent_dict);
	if (ent_dict == NULL) {
	    goto loop_done;
	}
	if_key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
						  serviceID,
						  kSCEntNetInterface);
	if (if_key == NULL) {
	    goto loop_done;
	}
	status = SCDGet(session, if_key, &if_handle);
	if (status != SCD_OK) {
	    goto loop_done;
	}
	if_dict = isA_CFDictionary(SCDHandleGetData(if_handle));
	if (if_dict == NULL) {
	    goto loop_done;
	}
	type = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceType);
	if (type == NULL
	    || CFEqual(type, kSCValNetInterfaceTypeEthernet) == FALSE) {
	    /* we only configure ethernet interfaces currently */
	    goto loop_done;
	}
	ifn_cf = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceDeviceName);
	if (ifn_cf == NULL) {
	    goto loop_done;
	}
	CFDictionarySetValue(ent_dict, kSCPropNetInterfaceDeviceName, ifn_cf);
	CFDictionarySetValue(ent_dict, PROP_SERVICEID, serviceID);
	CFArrayAppendValue(all, ent_dict);
    loop_done:
	my_CFRelease(&ent_dict);
	my_CFRelease(&if_key);
	my_CFRelease(&serviceID);
	my_SCDHandleRelease(&if_handle);
	my_SCDHandleRelease(&ent_handle);
    }
 done:
    my_CFRelease(&key);
    my_CFRelease(&arr);
    if (all) {
	if (CFArrayGetCount(all) == 0) {
	    my_CFRelease(&all);
	}
    }
    return (all);
    
}

#define ARBITRARILY_LARGE_NUMBER	(1000 * 1000)

static int
lookup_order(CFArrayRef order, CFStringRef serviceID)
{
    int i;

    if (order == NULL)
	goto done;

    for (i = 0; i < CFArrayGetCount(order); i++) {
	CFStringRef	sid = CFArrayGetValueAtIndex(order, i);

	if (CFEqual(sid, serviceID))
	    return (i);
    }
 done:
    return (ARBITRARILY_LARGE_NUMBER);
}

static CFDictionaryRef
lookup_entity(CFArrayRef all, CFArrayRef order, CFStringRef ifn_cf)
{
    int 		i;
    CFDictionaryRef	ret = NULL;
    int			rank = ARBITRARILY_LARGE_NUMBER;

    if (all == NULL)
	return (NULL);

    for (i = 0; i < CFArrayGetCount(all); i++) {
	CFDictionaryRef	item = CFArrayGetValueAtIndex(all, i);
	CFStringRef	name;
	CFStringRef	serviceID;

	name = CFDictionaryGetValue(item, kSCPropNetInterfaceDeviceName);
	serviceID = CFDictionaryGetValue(item, PROP_SERVICEID);
	if (name == NULL || serviceID == NULL)
	    continue;
	if (CFEqual(name, ifn_cf)) {
	    int		this_rank = lookup_order(order, serviceID);

	    if (ret == NULL || this_rank < rank) {
		ret = item;
		rank = this_rank;
	    }
	}
    }
    return (ret);
}

CFArrayRef
entity_service_order(SCDSessionRef session, CFStringRef entity)
{
    CFDictionaryRef	dict;
    SCDHandleRef	handle = NULL;
    CFStringRef		key;
    CFArrayRef		order = NULL;
    SCDStatus 		status;

    key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup,
					  kSCEntNetIPv4);
    if (key == NULL)
	return (NULL);
    status = SCDGet(session, key, &handle);
    if (status != SCD_OK) {
	goto done;
    }
    dict = isA_CFDictionary(SCDHandleGetData(handle));
    if (dict == NULL) {
	goto done;
    }
    order = isA_CFArray(CFDictionaryGetValue(dict, kSCPropNetServiceOrder));
    if (order != NULL)
	CFRetain(order);
 done:
    my_CFRelease(&key);
    my_SCDHandleRelease(&handle);
    return (order);
}

static void
handle_configuration_changed(SCDSessionRef session,
			     CFArrayRef all_ipv4, CFArrayRef order)
{
    int i;
    for (i = 0; i < ifl_count(S_interfaces); i++) {
	CFStringRef		ifn_cf = NULL;
	interface_t *		if_p = ifl_at_index(S_interfaces, i);
	CFDictionaryRef		ipv4_dict;
	ipconfig_status_t	status;
	
	if (strcmp(if_name(if_p), "lo0") == 0) {
	    continue;
	}
	ifn_cf = CFStringCreateWithCString(NULL,
					   if_name(if_p),
					   kCFStringEncodingMacRoman);
	if (ifn_cf == NULL) {
	    goto loop_done;
	}
	ipv4_dict = lookup_entity(all_ipv4, order, ifn_cf);
	if (ipv4_dict == NULL) {
	    status = set_if(if_name(if_p), ipconfig_method_none_e, NULL, 0,
			    NULL);
	    if (status != ipconfig_status_success_e) {
		ts_log(LOG_INFO, "ipconfigd: set %s %s failed, %s",
		       if_name(if_p),
		       ipconfig_method_string(ipconfig_method_none_e),
		       ipconfig_status_string(status));
	    }
	}
	else {
	    ipconfig_method_t		method;
	    ipconfig_method_data_t *	method_data = NULL;
	    int				method_data_len = 0;
	    CFStringRef			serviceID;
	    
	    serviceID = CFDictionaryGetValue(ipv4_dict,
					     PROP_SERVICEID);
	    method_data = ipconfig_method_data_from_dict(ipv4_dict, &method,
							 &method_data_len);
	    if (method_data == NULL) {
		status = set_if(if_name(if_p), ipconfig_method_none_e, NULL, 0,
				NULL);
		if (status != ipconfig_status_success_e) {
		    ts_log(LOG_INFO, "ipconfigd: set %s %s failed, %s",
			   if_name(if_p),
			   ipconfig_method_string(ipconfig_method_none_e),
			   ipconfig_status_string(status));
		}
	    }
	    else {
		status = set_if(if_name(if_p), method, 
				method_data, method_data_len, 
				(void *)serviceID);
		if (status != ipconfig_status_success_e) {
		    ts_log(LOG_INFO, "ipconfigd: set %s %s failed, %s",
			   if_name(if_p), ipconfig_method_string(method),
			   ipconfig_status_string(status));
		}
	    }
	    if (method_data) {
		free(method_data);
	    }
	} 
    loop_done:
	my_CFRelease(&ifn_cf);
    }
    return;
}

static void
configuration_changed(SCDSessionRef session)
{
    CFArrayRef		all_ipv4 = NULL;
    CFArrayRef		order = NULL;

    (void)SCDLock(session);
    all_ipv4 = entity_all(session, kSCEntNetIPv4);
    order = entity_service_order(session, kSCEntNetIPv4);
    (void)SCDUnlock(session);
    handle_configuration_changed(session, all_ipv4, order);
    my_CFRelease(&all_ipv4);
    my_CFRelease(&order);
    return;
}

static void
configure_from_cache(SCDSessionRef session)
{
    CFArrayRef		all_ipv4 = NULL;
    int			count = 0;
    int 		i;
    CFArrayRef		order = NULL;

    all_ipv4 = entity_all(session, kSCEntNetIPv4);
    order = entity_service_order(session, kSCEntNetIPv4);
    if (all_ipv4 == NULL) {
	goto done;
    }

    /* 
     * Go through the list of interfaces and find those that have a
     * configuration.  If an interface is present, pre-allocate an ifstate
     * entry so that the system startup will wait for that interface to
     * complete its initialization.
     */
    for (i = 0; i < ifl_count(S_interfaces); i++) {
	CFDictionaryRef		dict;
	interface_t *		if_p = ifl_at_index(S_interfaces, i);
	CFStringRef		ifn_cf = NULL;

	if (strcmp(if_name(if_p), "lo0") == 0) {
	    continue;
	}
	ifn_cf = CFStringCreateWithCString(NULL,
					   if_name(if_p),
					   kCFStringEncodingMacRoman);
	if (ifn_cf == NULL) {
	    goto loop_done;
	}
	dict = lookup_entity(all_ipv4, order, ifn_cf);
	if (dict == NULL) {
	    goto loop_done;
	}
	(void)IFState_make_if(if_p);
	count++;
    loop_done:
	my_CFRelease(&ifn_cf);
    }

 done:
    if (count == 0) {
	unblock_startup(session);
    }
    else {
	handle_configuration_changed(session, all_ipv4, order);
    }

    my_CFRelease(&all_ipv4);
    my_CFRelease(&order);

    return;
}

static void
configure_from_iftab(SCDSessionRef session)
{
    ipcfg_table_t *	new_ipcfg_list = NULL;
    hostconfig_t *	new_hostconfig = NULL;

    if (access("/etc/iftab", R_OK) == 0) {
	char		msg[512];

	new_ipcfg_list = ipcfg_from_file(msg);
	if (new_ipcfg_list == NULL) {
	    ts_log(LOG_ERR, "ipconfigd: failed to get ip config, %s", msg);
	    goto failed;
	}
	else {
	    if (S_ipcfg_list) {
		ipcfg_free(&S_ipcfg_list);
	    }
	    S_ipcfg_list = new_ipcfg_list;
	}

        new_hostconfig = hostconfig_read(msg);
        if (new_hostconfig == NULL) {
	    ts_log(LOG_ERR, "ipconfigd: failed to get hostconfig, %s", msg);
	}
	else {
	    if (S_hostconfig) {
		hostconfig_free(&S_hostconfig);
	    }
	    S_hostconfig = new_hostconfig;
	}
        load_cache_from_iftab(session);
    }
    else {
	unblock_startup(session);
    }
    return;

 failed:
    unblock_startup(session);
    return;
}

static void 
notifier_init(SCDSessionRef session)
{
    CFStringRef		key;
    SCDStatus 		status;

    if (session == NULL) {
	return;
    }
    /* notify when IPv4 config of any service changes */
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
					   kSCCompAnyRegex,
					   kSCEntNetIPv4);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    my_CFRelease(&key);

    /* notify when Interface service <-> interface binding changes */
    key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
					   kSCCompAnyRegex,
					   kSCEntNetInterface);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    my_CFRelease(&key);

    /* notify when the link status of any interface changes */
    key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
					     kSCCompAnyRegex,
					     kSCEntNetLink);
    status = SCDNotifierAdd(session, key, kSCDRegexKey);
    my_CFRelease(&key);

    /* notify when list of interfaces changes */
    key = SCDKeyCreateNetworkInterface(kSCCacheDomainState);
    status = SCDNotifierAdd(session, key, 0);
    my_CFRelease(&key);

    SCDNotifierInformViaCallback(session, handle_change, NULL);
    return;
}

static boolean_t
start_interfaces(SCDSessionRef session, void * arg)
{
    SCDHandleRef	handle = NULL;
    CFArrayRef		changes = NULL;
    CFStringRef		key;
    interface_list_t *	new_interfaces = NULL;
    SCDStatus		status;

    /* remove, cancel, and clean up after the setup: notifier */
    (void)SCDNotifierRemove(session, kSCCacheDomainSetup, 0);
    (void)SCDNotifierCancel(session);
    status = SCDNotifierGetChanges(session, &changes);
    if (status == SCD_OK) {
	my_CFRelease(&changes);
    }

    /* install run-time notifiers */
    notifier_init(session);

    new_interfaces = ifl_init();
    if (new_interfaces == NULL) {
	ts_log(LOG_ERR, "ipconfigd: ifl_init failed");
	return (FALSE);
    }
    if (S_interfaces) {
	ifl_free(&S_interfaces);
    }
    S_interfaces = new_interfaces;

    /* populate cache with flat files or is cache already populated? */
    key = SCDKeyCreate(CFSTR("%@" USE_FLAT_FILES), kSCCacheDomainSetup);
    status = SCDGet(session, key, &handle);
    my_CFRelease(&key);
    if (status == SCD_OK) {
	/* use iftab, hostconfig files to populate the cache */
        my_SCDHandleRelease(&handle);
	configure_from_iftab(session);
    }
    else {
	/* cache is already populated */
	configure_from_cache(session);
    }
    return (TRUE);
}

static void
start(SCDSessionRef session)
{
    SCDHandleRef	handle;
    SCDStatus		status;

    if (session == NULL) {
	return;
    }

    S_setup_service_prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"), 
					  kSCCacheDomainSetup,
					  kSCCompNetwork,
					  kSCCompService);
						       
    S_state_interface_prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"), 
					    kSCCacheDomainState,
					    kSCCompNetwork,
					    kSCCompInterface);
    /* wait for the network setup plug-in to finish loading the config */
    (void)SCDNotifierAdd(session, kSCCacheDomainSetup, 0);
    status = SCDGet(session, kSCCacheDomainSetup, &handle);
    if (status == SCD_OK) {
	/* cache has been primed with configuration status/info */
	SCDHandleRelease(handle);
	start_interfaces(session, NULL);
	return;
    }
    SCDNotifierInformViaCallback(session, start_interfaces, NULL);
    return;
}

static void
parent_exit(int i)
{
    exit(0);
}

static int
fork_child()
{
    int child_pid;
    int fd;

    signal(SIGTERM, parent_exit);
    child_pid = fork();
    switch (child_pid) {
      case -1: {
	  return (-1);
      }
      case 0: {
	   /* child: becomes the daemon (see below) */
	  signal(SIGTERM, SIG_DFL);
	  break;
      }
      default: {
	  int status;
	  /* parent: wait for signal or child exit, then exit */
	  wait4(child_pid, &status, 0, NULL);
	  fprintf(stderr, "ipconfigd: child exited unexpectedly\n");
	  exit(1);
      }
    }
    
    if (setsid() == -1)
	return (-1);
    
    (void)chdir("/");
    
    if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
	(void)dup2(fd, STDIN_FILENO);
	(void)dup2(fd, STDOUT_FILENO);
	(void)dup2(fd, STDERR_FILENO);
	if (fd > 2)
	    (void)close (fd);
    }
    return (0);
}

ipconfig_status_t
state_key_changed(SCDSessionRef session, CFStringRef cache_key)
{
    CFStringRef			ifn_cf = NULL;
    char			ifn[IFNAMSIZ + 1];
    IFState_t *   		ifstate;
    link_status_t		link = {FALSE, FALSE};
    ipconfig_status_t		istatus = ipconfig_status_internal_error_e;
    
    ifn_cf = parse_component(cache_key, S_state_interface_prefix);
    if (ifn_cf == NULL) {
	return (ipconfig_status_internal_error_e);
    }
    cfstring_to_cstring(ifn_cf, ifn, sizeof(ifn));
    link.valid = get_media_status(ifn, &link.active);
    if (link.valid == FALSE) {
	goto done;
    }
    ifstate = IFState_lookup_if(&S_ifstate_list, ifn);
    if (ifstate == NULL) {
	goto done;
    }
    if (G_debug) {
	SCDLog(LOG_INFO, CFSTR("%s link is %s"),
	       ifn, link.active ? "up" : "down");
    }
    istatus = config_method_media(ifstate, link);

 done:
    my_CFRelease(&ifn_cf);
    return (istatus);
}

static boolean_t
handle_change(SCDSessionRef session, void * arg)
{
    CFArrayRef		changes = NULL;
    boolean_t		config_changed = FALSE;
    CFIndex		count;
    CFIndex		i;
    boolean_t		iflist_changed = FALSE;
    SCDStatus		status;

    status = SCDNotifierGetChanges(session, &changes);
    if (status != SCD_OK || changes == NULL) {
	return (TRUE);
    }
    count = CFArrayGetCount(changes);
    if (count == 0) {
	goto done;
    }
    if (G_debug) {
	SCDLog(LOG_INFO, CFSTR("Changes: %@ (%d)"), changes,
	       count);
    }
    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);
        if (CFStringHasPrefix(cache_key, kSCCacheDomainSetup)) {
	    /* IPv4 configuration changed */
	    config_changed = TRUE;
	}
	else if (CFStringHasSuffix(cache_key, kSCCompInterface)) {
	    /* list of interfaces changed */
	    iflist_changed = TRUE;
	}
	else {
	    (void)state_key_changed(session, cache_key);
	}
    }
    /* an interface was added */
    if (iflist_changed) {
	interface_list_t *	new_interfaces;

	new_interfaces = ifl_init();
	if (new_interfaces == NULL) {
	    ts_log(LOG_ERR, "ipconfigd: ifl_init failed");
	}
	else {
	    if (S_interfaces) {
		ifl_free(&S_interfaces);
	    }
	    S_interfaces = new_interfaces;
	    config_changed = TRUE;
	}
    }
    if (config_changed) {
	configuration_changed(session);
    }
 done:
    CFRelease(changes);
    return (TRUE);
}

#if !defined(DARWIN)

static void
user_confirm(CFUserNotificationRef userNotification, 
	     CFOptionFlags responseFlags)
{
    int i;

    /* clean-up the notification */
    for (i = 0; i < ptrlist_count(&S_ifstate_list); i++) {
	IFState_t *	element = ptrlist_element(&S_ifstate_list, i);
	if (element->user_notification == userNotification) {
	    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), element->user_rls, 
				  kCFRunLoopDefaultMode);
	    my_CFRelease(&element->user_rls);
	    my_CFRelease(&element->user_notification);
	    break;
	}
    }
    return;
}

void
ifstate_tell_user(IFState_t * ifstate, char * msg)
{
    CFStringRef			alert_string = NULL;
    CFMutableDictionaryRef	dict = NULL;
    SInt32			error = 0;
    CFUserNotificationRef 	notify = NULL;
    CFRunLoopSourceRef		rls;
 
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kCFUserNotificationAlertHeaderKey, 
			 CFSTR("IP Configuration"));
    alert_string = CFStringCreateWithCString(NULL, msg, 
					     kCFStringEncodingMacRoman);
    CFDictionarySetValue(dict, kCFUserNotificationAlertMessageKey, 
			 alert_string);
    my_CFRelease(&alert_string);
#if 0
    CFDictionarySetValue(dict, kCFUserNotificationDefaultButtonTitleKey, 
			 CFSTR("OK"));
#endif 0
    if (ifstate->user_rls) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), ifstate->user_rls,
			      kCFRunLoopDefaultMode);
	my_CFRelease(&ifstate->user_rls);
    }
    if (ifstate->user_notification != NULL) {
	CFUserNotificationCancel(ifstate->user_notification);
	my_CFRelease(&ifstate->user_notification);
    }
    notify = CFUserNotificationCreate(NULL, 0, 0, &error, dict);
    my_CFRelease(&dict);
    if (notify == NULL) {
	ts_log(LOG_ERR, "CFUserNotificationCreate() failed, %d",
	       error);
	goto done;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, notify, 
						user_confirm, 0);
    if (rls == NULL) {
	ts_log(LOG_ERR, "CFUserNotificationCreateRunLoopSource() failed");
	my_CFRelease(&notify);
    }
    else {
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, 
			   kCFRunLoopDefaultMode);
	ifstate->user_rls = rls;
	ifstate->user_notification = notify;
    }
 done:
    return;
}
#else
void
ifstate_tell_user(IFState_t * ifstate, char * msg)
{
}
#endif

int 
main(int argc, char *argv[])
{
    int 		ch;
    u_char *		progname = argv[0];
    pid_t		ppid;
    SCDStatus 		r;

    if (server_active()) {
	fprintf(stderr, "ipconfig server already active\n");
	exit(1);
    }
    { /* set the random seed */
	struct timeval	start_time;

	gettimeofday(&start_time, 0);
	srandom(start_time.tv_usec & ~start_time.tv_sec);
    }

    while ((ch = getopt(argc, argv, "bc:deg:hHl:r:s:v")) != EOF) {
	switch ((char) ch) {
	case 'b':
	    G_must_broadcast = TRUE;
	    break;
	case 'c': /* client port - for testing */
	    G_client_port = atoi(optarg);
	    break;
	case 'd':
	    G_debug = TRUE;
	    break;
	case 'e': /* send the request and exit */
	    G_exit_quick = TRUE;
	    break;
	case 'g': /* gather time */
	    G_gather_secs = strtoul(optarg, NULL, NULL);
	    break;
	case 'l': /* link inactive time */
	    G_link_inactive_secs = strtoul(optarg, NULL, NULL);
	    break;
	case 'v':
	    G_verbose = TRUE;
	    break;
	case 'r': /* retry count */
	    G_max_retries = strtoul(optarg, NULL, NULL);
	    break;
	case 's': /* server port - for testing */
	    G_server_port = atoi(optarg);
	    break;
	case 'H':
	case 'h':
	    usage(progname);
	    break;
	}
    }

    if ((argc - optind) != 0) {
	usage(progname);
    }

    ppid = getpid();

    if (G_debug == 0) {
	if (fork_child() == -1) {
	    fprintf(stderr, "ipconfigd: fork failed, %s (%d)\n", 
		    strerror(errno), errno);
	    exit(UNEXPECTED_ERROR);
	}
	/* now the child process, parent waits in fork_child */
    }
    ts_log_init(G_verbose);

    if (create_path(DHCPCLIENT_LEASES_DIR, 0700) < 0) {
	ts_log(LOG_DEBUG, "failed to create " 
		DHCPCLIENT_LEASES_DIR ", %s (%d)", strerror(errno), errno);
    }

    G_readers = FDSet_init();
    if (G_readers == NULL) {
	ts_log(LOG_DEBUG, "FDSet_init() failed");
	exit(UNEXPECTED_ERROR);
    }
    G_bootp_session = bootp_session_init(G_client_port);
    if (G_bootp_session == NULL) {
	ts_log(LOG_DEBUG, "bootp_session_init() failed");
	exit(UNEXPECTED_ERROR);
    }

    G_arp_session = arp_session_init();
    if (G_arp_session == NULL) {
	ts_log(LOG_DEBUG, "arp_session_init() failed");
	exit(UNEXPECTED_ERROR);
    }

    if (G_debug) {
	(void) openlog("ipconfigd", LOG_PERROR | LOG_PID, LOG_DAEMON);
    }
    else {
	(void) openlog("ipconfigd", LOG_CONS | LOG_PID, LOG_DAEMON);
    }
    bootp_session_set_debug(G_bootp_session, G_debug);
    ptrlist_init(&S_ifstate_list);
    r = SCDOpen(&S_scd_session, CFSTR("IP Config Server"));
    if (r != SCD_OK) {
	ts_log(LOG_INFO, "SCDOpen failed: %s\n", SCDError(r));
    }
    else {
        SCDOptionSet(S_scd_session, kSCDOptionUseCFRunLoop, TRUE);
        SCDOptionSet(S_scd_session, kSCDOptionUseSyslog, TRUE);
    }
    SCDOptionSet(NULL, kSCDOptionUseCFRunLoop, TRUE);
    SCDOptionSet(NULL, kSCDOptionUseSyslog, TRUE);

    /* begin interface initialization */
    start(S_scd_session);

    /* initialize the MiG server */
    server_init();

    if (G_debug == 0) {
	/* synchronize with parent process */
	kill(ppid, SIGTERM);
    }

    CFRunLoopRun();
    
    bootp_session_free(&G_bootp_session);
    exit(0);
}
