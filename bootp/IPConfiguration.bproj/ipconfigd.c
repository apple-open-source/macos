/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
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
 *
 * March 27, 2001	Dieter Siegmund (dieter@apple.com)
 * - turned ipconfigd into the IPConfiguration bundle
 *
 * May 17, 2001		Dieter Siegmund (dieter@apple.com) 
 * - publish information in service state instead of interface state
 *
 * June 14, 2001	Dieter Siegmund (dieter@apple.com)
 * - publish DHCP options in dynamic store, and allow third party
 *   applications to request additional options using a DHCPClient
 *   preference
 * - add notification handler to automatically force the DHCP client
 *   to renew its lease
 *
 * July 12, 2001	Dieter Siegmund (dieter@apple.com)
 * - don't bother reporting arp collisions with our own interfaces
 *
 * July 19, 2001	Dieter Siegmund (dieter@apple.com)
 * - port to use public SystemConfiguration APIs
 * 
 * August 28, 2001	Dieter Siegmund (dieter@apple.com)
 * - when multiple interfaces are configured to be on the same subnet,
 *   keep the subnet route correct and have it follow the service/interface
 *   with the highest priority
 * - this also eliminates problems with the subnet route getting lost
 *   when an interface is de-configured, yet another interface is on
 *   the same subnet
 *
 * September 10, 2001	Dieter Siegmund (dieter@apple.com)
 * - added multiple service per interface support
 * - separated ad-hoc/link-local address configuration into its own service
 *
 * January 4, 2002	Dieter Siegmund (dieter@apple.com)
 * - always configure the link-local service on the service with the
 *   highest priority service that's active
 * - modified link-local service to optionally allocate an IP address;
 *   if we don't allocate an IP, we just set the link-local subnet
 * - allow a previously failed DHCP service to acquire a link-local IP
 *   address if it later becomes the primary service
 * - a link-local address will only be allocated when a DHCP service fails,
 *   and it is the primary service, and the G_dhcp_failure_configures_linklocal
 *   is TRUE
 *
 * February 1, 2002	Dieter Siegmund (dieter@apple.com)
 * - make IPConfiguration netboot-aware:
 *   + grab the DHCP information from the packet in the device tree
 *
 * May 20, 2002		Dieter Siegmund (dieter@apple.com)
 * - allocate a link-local address more quickly, after the first
 *   DHCP request fails
 * - re-structured the automatic link-local service allocation to do
 *   most of the work from a run-loop observer instead of within the 
 *   context of the caller; this avoids unnecessary re-entrancy issues
 *   and complexity
 *
 * December 3, 2002	Dieter Siegmund (dieter@apple.com)
 * - add support to detect ARP collisions after we have already
 *   assigned ourselves the address
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/socket.h>
#define KERNEL_PRIVATE
#include <sys/ioctl.h>
#undef KERNEL_PRIVATE
#include <ctype.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <syslog.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <mach/boolean.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDPlugin.h>

#include "rfc_options.h"
#include "dhcp_options.h"
#include "interfaces.h"
#include "util.h"
#include "arp.h"

#include "host_identifier.h"
#include "dhcplib.h"
#include "ioregpath.h"

#include "ipcfg.h"
#include "ipconfig_types.h"
#include "ipconfigd.h"
#include "server.h"
#include "timer.h"

#include "ipconfigd_globals.h"
#include "ipconfigd_threads.h"
#include "FDSet.h"

#include "dprintf.h"

#include "cfutil.h"

typedef dynarray_t	IFStateList_t;

#ifndef kSCEntNetIPv4ARPCollision
#define kSCEntNetIPv4ARPCollision	CFSTR("IPv4ARPCollision")
#endif kSCEntNetIPv4ARPCollision

#ifndef kSCPropNetLinkDetaching
#define kSCPropNetLinkDetaching		CFSTR("Detaching")	/* CFBoolean */
#endif kSCPropNetLinkDetaching

#ifndef kSCPropNetOverridePrimary
#define kSCPropNetOverridePrimary	CFSTR("OverridePrimary")
#endif kSCPropNetOverridePrimary

#define kDHCPClientPreferencesID	CFSTR("DHCPClient.xml")
#define kDHCPClientApplicationPref	CFSTR("Application")
#define kDHCPRequestedParameterList	CFSTR("DHCPRequestedParameterList")

#ifndef kSCEntNetDHCP
#define kSCEntNetDHCP			CFSTR("DHCP")
#endif kSCEntNetDHCP

#ifndef kSCValNetIPv4ConfigMethodLinkLocal
#define kSCValNetIPv4ConfigMethodLinkLocal	CFSTR("LinkLocal")
#endif kSCValNetIPv4ConfigMethodLinkLocal


#define MAX_RETRIES				9
#define INITIAL_WAIT_SECS			1
#define MAX_WAIT_SECS				8
#define GATHER_SECS				2
#define LINK_INACTIVE_WAIT_SECS			4
#define ARP_PROBE_COUNT				4
#define ARP_GRATUITOUS_COUNT			1
#define ARP_RETRY_SECS				0
#define ARP_RETRY_USECS				300000
#define DHCP_INIT_REBOOT_RETRY_COUNT		2
#define DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT	2
#define DHCP_FAILURE_CONFIGURES_LINKLOCAL	TRUE
#define DHCP_SUCCESS_DECONFIGURES_LINKLOCAL	TRUE

#define USE_FLAT_FILES			"UseFlatFiles"

#define USER_ERROR			1
#define UNEXPECTED_ERROR 		2
#define TIMEOUT_ERROR			3

/* global variables */
u_short 			G_client_port = IPPORT_BOOTPC;
boolean_t 			G_dhcp_accepts_bootp = FALSE;
boolean_t			G_dhcp_failure_configures_linklocal 
				    = DHCP_FAILURE_CONFIGURES_LINKLOCAL;
boolean_t			G_dhcp_success_deconfigures_linklocal 
				    = DHCP_SUCCESS_DECONFIGURES_LINKLOCAL;
u_long				G_dhcp_allocate_linklocal_at_retry_count 
				    = DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT;
u_long				G_dhcp_init_reboot_retry_count 
				    = DHCP_INIT_REBOOT_RETRY_COUNT;
u_short 			G_server_port = IPPORT_BOOTPS;

/* 
 * Global: G_link_inactive_secs
 * Purpose:
 *   Time to wait after the link goes inactive before unpublishing 
 *   the interface state information
 */
u_long				G_link_inactive_secs = LINK_INACTIVE_WAIT_SECS;

/*
 * Global: G_gather_secs
 * Purpose:
 *   Time to wait for the ideal packet after receiving 
 *   the first acceptable packet.
 */ 
u_long				G_gather_secs = GATHER_SECS;

/*
 * Global: G_initial_wait_secs
 * Purpose:
 *   First timeout interval in seconds.
 */ 
u_long				G_initial_wait_secs = INITIAL_WAIT_SECS;

/*
 * Global: G_max_retries
 * Purpose:
 *   Number of times to retry sending the packet.
 */ 
u_long				G_max_retries = MAX_RETRIES;

/*
 * Global: G_max_wait_secs
 * Purpose:
 *   Maximum timeout interval.  Timeouts timeout[i] are chosen as:
 *   i = 0:
 *     timeout[0] = G_initial_wait_secs;
 *   i > 0:
 *     timeout[i] = min(timeout[i - 1] * 2, G_max_wait_secs);
 *   If G_initial_wait_secs = 4, G_max_wait_secs = 16, the sequence is:
 *     4, 8, 16, 16, ...
 */ 
u_long				G_max_wait_secs = MAX_WAIT_SECS;

boolean_t 			G_must_broadcast = FALSE;
int				G_verbose = FALSE;
int				G_debug = FALSE;
bootp_session_t *		G_bootp_session = NULL;
FDSet_t *			G_readers = NULL;
arp_session_t * 		G_arp_session = NULL;

const unsigned char		G_rfc_magic[4] = RFC_OPTIONS_MAGIC;
const struct sockaddr		G_blank_sin = { sizeof(G_blank_sin), AF_INET };
const struct in_addr		G_ip_broadcast = { INADDR_BROADCAST };
const struct in_addr		G_ip_zeroes = { 0 };

/* local variables */
static CFBundleRef		S_bundle = NULL;
static CFRunLoopObserverRef	S_observer = NULL;
static boolean_t		S_linklocal_election_required = FALSE;
static IFStateList_t		S_ifstate_list;
static interface_list_t	*	S_interfaces = NULL;
static SCDynamicStoreRef	S_scd_session = NULL;
static CFStringRef		S_setup_service_prefix = NULL;
static CFStringRef		S_state_interface_prefix = NULL;
static char * 			S_computer_name = NULL;
static CFStringRef		S_computer_name_key = NULL;
static CFStringRef		S_dhcp_preferences_key = NULL;
static boolean_t		S_verbose = FALSE;
static int			S_arp_probe_count = ARP_PROBE_COUNT;
static int			S_arp_gratuitous_count = ARP_GRATUITOUS_COUNT;
static struct timeval		S_arp_retry = { 
  ARP_RETRY_SECS,
  ARP_RETRY_USECS
};

static struct in_addr		S_netboot_ip;
static struct in_addr		S_netboot_server_ip;
static char			S_netboot_ifname[IFNAMSIZ + 1];

/* 
 * Static: S_linklocal_service_p
 * Purpose:
 *    Service with the link local subnet associated with it.
 * Note:
 *    When non-NULL, this points to a link-local service that is created
 *    as a child of another existing service.
 */
static Service_t *		S_linklocal_service_p = NULL;

#define PROP_SERVICEID		CFSTR("ServiceID")

static void
S_add_dhcp_parameters();

static void
configuration_changed(SCDynamicStoreRef session);

static boolean_t
get_media_status(char * name, boolean_t * media_status);

static ipconfig_status_t
config_method_start(Service_t * service_p, ipconfig_method_t method,
		    ipconfig_method_data_t * data,
		    unsigned int data_len);

static ipconfig_status_t
config_method_change(Service_t * service_p, ipconfig_method_t method,
		     ipconfig_method_data_t * data,
		     unsigned int data_len, boolean_t * needs_stop);


static ipconfig_status_t
config_method_stop(Service_t * service_p);

static ipconfig_status_t
config_method_media(Service_t * service_p);

static ipconfig_status_t
config_method_renew(Service_t * service_p);

static void
service_publish_clear(Service_t * service_p);

static int
inet_attach_interface(char * ifname);

static int
inet_detach_interface(char * ifname);

static boolean_t
all_services_ready();

static void
S_linklocal_elect(CFArrayRef service_order);

static CFArrayRef
S_get_service_order(SCDynamicStoreRef session);

static unsigned int
S_get_service_rank(CFArrayRef arr, Service_t * service_p);

static IFState_t *
IFStateList_linklocal_service(IFStateList_t * list, 
			      Service_t * * ret_service_p);
static void
IFState_service_free(IFState_t * ifstate, CFStringRef serviceID);

static void
S_linklocal_start(Service_t * parent_service_p, boolean_t no_allocate);

/*
 * Function: S_is_our_hardware_address
 *
 * Purpose:
 *   Returns whether the given hardware address is that of any of
 *   our attached network interfaces.
 */
static boolean_t
S_is_our_hardware_address(interface_t * ignored,
			  int hwtype, void * hwaddr, int hwlen)
{
    int i;

    for (i = 0; i < ifl_count(S_interfaces); i++) {
	interface_t *	if_p = ifl_at_index(S_interfaces, i);

	if (hwtype == if_link_arptype(if_p)
	    && hwlen == if_link_length(if_p)
	    && bcmp(hwaddr, if_link_address(if_p), hwlen) == 0) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    if (priority == LOG_DEBUG) {
	if (G_verbose == FALSE)
	    return;
	priority = LOG_INFO;
    }

    va_start(ap, message);
    if (S_scd_session == NULL) {
	vsyslog(priority, message, ap);
    }
    else {
	char	buffer[256];

	vsnprintf(buffer, sizeof(buffer), message, ap);
	SCLog(TRUE, priority, CFSTR("%s"), buffer);
    }
    return;
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

static CFDictionaryRef
my_SCDynamicStoreCopyValue(SCDynamicStoreRef session, CFStringRef key)
{
    CFDictionaryRef 		dict;

    dict = SCDynamicStoreCopyValue(session, key);
    if (dict) {
	if (isA_CFDictionary(dict) == NULL) {
	    my_CFRelease(&dict);
	}
    }
    return (dict);
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

/**
 ** Computer Name handling routines
 **/

char *
computer_name()
{
    return (S_computer_name);
}

static void
computer_name_update(SCDynamicStoreRef session)
{
    char		buf[256];
    CFStringEncoding	encoding;
    CFStringRef 	name;

    if (session == NULL)
	return;

    if (S_computer_name) {
	free(S_computer_name);
	S_computer_name = NULL;
    }

    name = SCDynamicStoreCopyComputerName(session, &encoding);
    if (name == NULL) {
	goto done;
    }
    if (DNSHostNameStringIsClean(name) == FALSE) {
	goto done;
    }
    if (CFStringGetCString(name, buf, sizeof(buf),
			   kCFStringEncodingASCII) == FALSE) {
	goto done;
    }
    S_computer_name = strdup(buf);

 done:
    my_CFRelease(&name);
    return;
}

/**
 ** DHCP Lease routines
 **/
#define DHCPCLIENT_LEASES_DIR		"/var/db/dhcpclient/leases"
#define DHCPCLIENT_LEASE_FILE_FMT	DHCPCLIENT_LEASES_DIR "/%s"

#define LEASE_IP_ADDRESS		"LeaseIPAddress"

static boolean_t
dhcp_lease_init()
{
    if (create_path(DHCPCLIENT_LEASES_DIR, 0700) < 0) {
	my_log(LOG_DEBUG, "failed to create " 
	       DHCPCLIENT_LEASES_DIR ", %s (%d)", strerror(errno), errno);
	return (FALSE);
    }
    return (TRUE);
}

/* 
 * Function: dhcp_lease_read
 *
 * Purpose:
 *   Read the DHCP lease for this interface.
 */
boolean_t
dhcp_lease_read(char * idstr, struct in_addr * iaddr_p)
{
    CFDictionaryRef		dict = NULL;
    char			filename[PATH_MAX];
    CFStringRef			ip_string;
    struct in_addr		ip;
    boolean_t			ret = FALSE;

    snprintf(filename, sizeof(filename), DHCPCLIENT_LEASE_FILE_FMT, idstr);
    dict = my_CFPropertyListCreateFromFile(filename);
    if (dict == NULL) {
	goto done;
    }
    if (isA_CFDictionary(dict) == NULL) {
	goto done;
    }

    /* get the IP address */
    ip_string = CFDictionaryGetValue(dict, CFSTR(LEASE_IP_ADDRESS));
    ip_string = isA_CFString(ip_string);
    if (ip_string == NULL) {
	goto done;
    }
    ip = cfstring_to_ip(ip_string);
    if (ip_valid(ip) == FALSE) {
	goto done;
    }
    *iaddr_p = ip;
    ret = TRUE;
 done:
    my_CFRelease(&dict);
    return (ret);
}

/* 
 * Function: dhcp_lease_write
 *
 * Purpose:
 *   Write the DHCP lease for this interface.
 */
boolean_t
dhcp_lease_write(char * idstr, struct in_addr ip)
{
    CFMutableDictionaryRef	dict = NULL;
    char			filename[PATH_MAX];
    CFStringRef			ip_string = NULL;
    boolean_t			ret = FALSE;

    snprintf(filename, sizeof(filename), DHCPCLIENT_LEASE_FILE_FMT, idstr);
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    ip_string = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
					 IP_LIST(&ip));
    if (ip_string == NULL) {
	goto done;
    }
    CFDictionarySetValue(dict, CFSTR(LEASE_IP_ADDRESS), ip_string);
    my_CFRelease(&ip_string);
    if (my_CFPropertyListWriteFile(dict, filename) < 0) {
	my_log(LOG_INFO, "my_CFPropertyListWriteFile(%s) failed, %s", 
	       filename, strerror(errno));
    }
    ret = TRUE;
 done:
    my_CFRelease(&dict);
    return (ret);
}

/*
 * Function: dhcp_lease_clear
 * Purpose:
 *   Remove the lease file so we don't try to use it again.
 */
void
dhcp_lease_clear(char * idstr)
{
    char		filename[PATH_MAX];
	
    snprintf(filename, sizeof(filename), DHCPCLIENT_LEASE_FILE_FMT, idstr);
    unlink(filename);
    return;
}

static boolean_t
S_same_subnet(struct in_addr ip1, struct in_addr ip2, struct in_addr mask)
{
    u_long m = iptohl(mask);
    u_long val1 = iptohl(ip1);
    u_long val2 = iptohl(ip2);

    if ((val1 & m) != (val2 & m)) {
	return (FALSE);
    }
    return (TRUE);
}

#define STARTUP_KEY	CFSTR("Plugin:IPConfiguration")

static __inline__ void
unblock_startup(SCDynamicStoreRef session)
{
    (void)SCDynamicStoreSetValue(session, STARTUP_KEY, STARTUP_KEY);
}

int
inet_dgram_socket()
{
    return (socket(AF_INET, SOCK_DGRAM, 0));
}

static int
ifflags_set(int s, char * name, short flags)
{
    struct ifreq	ifr;
    int 		ret;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ret = ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr);
    if (ret < 0) {
	return (ret);
    }
    ifr.ifr_flags |= flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

static int
siocprotoattach(int s, char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTOATTACH, &ifr));
}

static int
siocprotodetach(int s, char * name)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    return (ioctl(s, SIOCPROTODETACH, &ifr));
}

static int
siocautoaddr(int s, char * name, int value)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_data = (caddr_t)value;
    return (ioctl(s, SIOCAUTOADDR, &ifr));
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
	ifra.ifra_broadaddr = G_blank_sin;
	((struct sockaddr_in *)&ifra.ifra_broadaddr)->sin_addr = *broadcast;
    }
    return (ioctl(s, SIOCAIFADDR, &ifra));
}

static void
Service_free(void * arg)
{
    Service_t *		service_p = (Service_t *)arg;
    
    SCLog(G_verbose, LOG_INFO, CFSTR("Service_free(%@)"), 
	  service_p->serviceID);
    if (S_linklocal_service_p == service_p) {
	S_linklocal_service_p = NULL;
    }
    service_p->free_in_progress = TRUE;
    config_method_stop(service_p);
    service_publish_clear(service_p);
    if (service_p->user_rls) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), service_p->user_rls,
			      kCFRunLoopDefaultMode);
	my_CFRelease(&service_p->user_rls);
    }
    if (service_p->user_notification != NULL) {
	CFUserNotificationCancel(service_p->user_notification);
	my_CFRelease(&service_p->user_notification);
    }
    my_CFRelease(&service_p->serviceID);
    my_CFRelease(&service_p->parent_serviceID);
    my_CFRelease(&service_p->child_serviceID);
    free(service_p);
    return;
}

static Service_t *
Service_init(IFState_t * ifstate, CFStringRef serviceID,
	     ipconfig_method_t method, 
	     void * method_data, unsigned int method_data_len,
	     Service_t * parent_service_p, ipconfig_status_t * status_p)
{
    Service_t *		service_p = NULL;
    ipconfig_status_t	status = ipconfig_status_success_e;

    if (method == ipconfig_method_linklocal_e) {
	if (S_linklocal_service_p != NULL) {
	    IFState_service_free(service_ifstate(S_linklocal_service_p), 
				 S_linklocal_service_p->serviceID);
	}
    }

    service_p = (Service_t *)malloc(sizeof(*service_p));
    if (service_p == NULL) {
	status = ipconfig_status_allocation_failed_e;
	goto failed;
    }
    bzero(service_p, sizeof(*service_p));
    service_p->method = method;
    service_p->ifstate = ifstate;
    if (serviceID) {
	service_p->serviceID = (void *)CFRetain(serviceID);
    }
    else {
	service_p->serviceID = (void *)
	    CFStringCreateWithFormat(NULL, NULL, 
				     CFSTR("%s-%s"),
				     ipconfig_method_string(method),
				     if_name(service_interface(service_p)));
    }
    if (parent_service_p != NULL) {
	service_p->parent_serviceID 
	    = (void *)CFRetain(parent_service_p->serviceID);
    }
    status = config_method_start(service_p, method, method_data, 
				 method_data_len);
    if (status != ipconfig_status_success_e) {
	goto failed;
    }
    if (parent_service_p != NULL) {
	parent_service_p->child_serviceID 
	    = (void *)CFRetain(service_p->serviceID);
	if (service_p->method == ipconfig_method_linklocal_e) {
	    S_linklocal_service_p = service_p;
	}
    }
    *status_p = status;
    return (service_p);

 failed:
    if (service_p) {
	my_CFRelease(&service_p->serviceID);
	my_CFRelease(&service_p->parent_serviceID);
	free(service_p);
    }
    *status_p = status;
    return (NULL);
}

static Service_t *
IFState_service_with_ID(IFState_t * ifstate, CFStringRef serviceID)
{
    int		j;

    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (CFEqual(serviceID, service_p->serviceID)) {
	    return (service_p);
	}
    }
    return (NULL);
}

static Service_t *
IFState_service_with_ip(IFState_t * ifstate, struct in_addr iaddr)
{
    int		j;

    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (service_p->info.addr.s_addr == iaddr.s_addr) {
	    return (service_p);
	}
    }
    return (NULL);
}

static Service_t *
IFState_linklocal_service(IFState_t * ifstate)
{
    int		j = 0;

    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (service_p->method == ipconfig_method_linklocal_e
	    && service_p->parent_serviceID == NULL
	    && service_p->free_in_progress == FALSE) {
	    return (service_p);
	}
    }
    return (NULL);
}

static void
IFState_services_free(IFState_t * ifstate)
{
    ifstate->free_in_progress = TRUE;
    dynarray_free(&ifstate->services);
    ifstate->free_in_progress = FALSE;
    dynarray_init(&ifstate->services, Service_free, NULL);
    ifstate->startup_ready = TRUE;
    inet_detach_interface(if_name(ifstate->if_p));
    return;
}

static void
IFState_service_free(IFState_t * ifstate, CFStringRef serviceID)
{
    int		j;

    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	if (CFEqual(serviceID, service_p->serviceID)) {
	    dynarray_free_element(&ifstate->services, j);
	    return;
	}
    }
    return;
}

ipconfig_status_t
IFState_service_add(IFState_t * ifstate, CFStringRef serviceID, 
		    ipconfig_method_t method, 
		    void * method_data, unsigned int method_data_len,
		    Service_t * parent_service_p, Service_t * * ret_service_p)
{
    interface_t *	if_p = ifstate->if_p;
    Service_t *		service_p = NULL;
    ipconfig_status_t	status = ipconfig_status_success_e;

    /* attach IP */
    inet_attach_interface(if_name(ifstate->if_p));

    /* try to configure the service */
    service_p = Service_init(ifstate, serviceID, method, 
			     method_data, method_data_len,
			     parent_service_p, &status);
    if (service_p == NULL) {
	my_log(LOG_DEBUG, "status from %s was %s",
	       ipconfig_method_string(method), 
	       ipconfig_status_string(status));
	if (dynarray_count(&ifstate->services) == 0) {
	    /* no services configured, detach IP again */
	    inet_detach_interface(if_name(if_p));
	}
	all_services_ready();
    }
    else {
	dynarray_add(&ifstate->services, service_p);
    }
    if (ret_service_p) {
	*ret_service_p = service_p;
    }
    return (status);
}

static void
IFState_update_media_status(IFState_t * ifstate) 
{
    char * 		ifname = if_name(ifstate->if_p);
    link_status_t	link = {FALSE, FALSE};

    link.valid = get_media_status(ifname, &link.active);
    if (link.valid == FALSE) {
	SCLog(G_verbose, LOG_INFO, CFSTR("%s link is unknown"),
	      ifname);
    }
    else {
	SCLog(G_verbose, LOG_INFO, CFSTR("%s link is %s"),
	      ifname, link.active ? "up" : "down");
    }
    ifstate->link = link;
    return;
}

static IFState_t *
IFState_init(interface_t * if_p)
{
    IFState_t * ifstate;

    ifstate = malloc(sizeof(*ifstate));
    if (ifstate == NULL) {
	my_log(LOG_ERR, "IFState_init: malloc ifstate failed");
	return (NULL);
    }
    bzero(ifstate, sizeof(*ifstate));
    ifstate->if_p = if_dup(if_p);
    ifstate->ifname 
	= (void *) CFStringCreateWithCString(NULL, if_name(if_p),
					     kCFStringEncodingMacRoman);
    IFState_update_media_status(ifstate);
    dynarray_init(&ifstate->services, Service_free, NULL);
    return (ifstate);
}

static void
IFState_free(void * arg)
{
    IFState_t *		ifstate = (IFState_t *)arg;
    
    SCLog(G_verbose, LOG_INFO, CFSTR("IFState_free(%s)"), 
	  if_name(ifstate->if_p));
    IFState_services_free(ifstate);
    my_CFRelease(&ifstate->ifname);
    if_free(&ifstate->if_p);
    free(ifstate);
    return;
}

static IFState_t *
IFStateList_linklocal_service(IFStateList_t * list, Service_t * * ret_service_p)
{
    int 		i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFState_t *		ifstate = dynarray_element(list, i);
	Service_t *		service_p;

	service_p = IFState_linklocal_service(ifstate);
	if (service_p) {
	    if (ret_service_p) {
		*ret_service_p = service_p;
	    }
	    return (ifstate);
	}
    }
    if (ret_service_p) {
	*ret_service_p = NULL;
    }
    return (NULL);
}

static IFState_t *
IFStateList_ifstate_with_name(IFStateList_t * list, char * ifname, int * where)
{
    int i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFState_t *	element = dynarray_element(list, i);
	if (strcmp(if_name(element->if_p), ifname) == 0) {
	    if (where != NULL) {
		*where = i;
	    }
	    return (element);
	}
    }
    return (NULL);
}

static IFState_t *
IFStateList_ifstate_create(IFStateList_t * list, interface_t * if_p)
{
    IFState_t *   	ifstate;

    ifstate = IFStateList_ifstate_with_name(list, if_name(if_p), NULL);
    if (ifstate == NULL) {
	ifstate = IFState_init(if_p);
	if (ifstate) {
	    dynarray_add(list, ifstate);
	}
    }
    return (ifstate);
}

static void
IFStateList_ifstate_free(IFStateList_t * list, char * ifname)
{
    IFState_t *	ifstate;
    int		where = -1;

    ifstate = IFStateList_ifstate_with_name(list, ifname, &where);
    if (ifstate == NULL) {
	return;
    }
    dynarray_free_element(list, where);
    return;
}

#if 0
static void
IFStateList_print(IFStateList_t * list)
{
    int i;
  
    printf("-------start--------\n");
    for (i = 0; i < dynarray_count(list); i++) {
	IFState_t *	ifstate = dynarray_element(list, i);
	int		j;

	printf("%s:", if_name(ifstate->if_p));
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t *	service_p = dynarray_element(&ifstate->services, j);

	    printf("%s%s", (j == 0) ? "" : ", ",
		   ipconfig_method_string(service_p->method));
	}
	printf("\n");
    }
    printf("-------end--------\n");
    return;
}
#endif 0

static IFState_t *
IFStateList_service_with_ID(IFStateList_t * list, CFStringRef serviceID,
			    Service_t * * ret_service)
{
    int 	i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFState_t *	ifstate = dynarray_element(list, i);
	Service_t *	service_p;

	service_p = IFState_service_with_ID(ifstate, serviceID);
	if (service_p) {
	    if (ret_service) {
		*ret_service = service_p;
	    }
	    return (ifstate);
	}
    }
    if (ret_service) {
	*ret_service = NULL;
    }
    return (NULL);
}

static IFState_t *
IFStateList_service_with_ip(IFStateList_t * list, struct in_addr iaddr,
			    Service_t * * ret_service)
{
    int 	i;

    for (i = 0; i < dynarray_count(list); i++) {
	IFState_t *	ifstate = dynarray_element(list, i);
	Service_t *	service_p;

	service_p = IFState_service_with_ip(ifstate, iaddr);
	if (service_p) {
	    if (ret_service) {
		*ret_service = service_p;
	    }
	    return (ifstate);
	}
    }
    if (ret_service) {
	*ret_service = NULL;
    }
    return (NULL);
}

/**
 ** netboot-specific routines
 **/
void
netboot_addresses(struct in_addr * ip, struct in_addr * server_ip)
{
    if (ip)
	*ip = S_netboot_ip;
    if (server_ip)
	*server_ip = S_netboot_server_ip;
}


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

static boolean_t
S_netboot_init()
{
    CFDictionaryRef	chosen = NULL;
    struct dhcp *	dhcp;
    struct in_addr *	iaddr_p;
    interface_t *	if_p;
    IFState_t *		ifstate;
    boolean_t		is_dhcp = TRUE;
    boolean_t		is_netboot = FALSE;
    int			length;
    CFDataRef		response = NULL;

    if (S_netboot_root() == FALSE) {
	goto done;
    }

    chosen = myIORegistryEntryCopyValue("IODeviceTree:/chosen");
    if (chosen == NULL) {
	goto done;
    }
    response = CFDictionaryGetValue(chosen, CFSTR("dhcp-response"));
    if (isA_CFData(response) == NULL) {
	response = CFDictionaryGetValue(chosen, CFSTR("bootp-response"));
	if (isA_CFData(response) == NULL) {
	    goto done;
	}
	is_dhcp = FALSE;
    }
    dhcp = (struct dhcp *)CFDataGetBytePtr(response);
    length = CFDataGetLength(response);
    if (dhcp->dp_yiaddr.s_addr != 0) {
	S_netboot_ip = dhcp->dp_yiaddr;
    }
    else if (dhcp->dp_ciaddr.s_addr != 0) {
	S_netboot_ip = dhcp->dp_ciaddr;
    }
    else {
	goto done;
    }
    S_netboot_server_ip = dhcp->dp_siaddr;
    if_p = ifl_find_ip(S_interfaces, S_netboot_ip);
    if (if_p == NULL) {
	/* not netbooting: some interface (en0) must have the assigned IP */
	goto done;
    }
    ifstate = IFStateList_ifstate_create(&S_ifstate_list, if_p);
    ifstate->netboot = TRUE;
    if (is_dhcp == TRUE) {
	dhcpol_t		options;

	(void)dhcpol_parse_packet(&options, dhcp, length, NULL);
	iaddr_p = (struct in_addr *)dhcpol_find(&options, 
						dhcptag_server_identifier_e, 
						NULL, NULL);
	if (iaddr_p != NULL) {
	    S_netboot_server_ip = *iaddr_p;
	}
	dhcpol_free(&options);
    }
    strcpy(S_netboot_ifname, if_name(if_p));
    is_netboot = TRUE;

 done:
    my_CFRelease(&chosen);
    return (is_netboot);
}

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
service_clear(Service_t * service_p)
{
    if (service_p->published.msg) {
	free(service_p->published.msg);
	service_p->published.msg = NULL;
    }
    service_p->published.ready = FALSE;
    dhcpol_free(&service_p->published.options);
    if (service_p->published.pkt) {
	free(service_p->published.pkt);
    }
    bzero(&service_p->published, sizeof(service_p->published));
    return;
}

static void
service_publish_clear(Service_t * service_p)
{
    service_clear(service_p);
    if (S_scd_session != NULL && service_p->serviceID) {
	CFMutableArrayRef	array;
	CFStringRef		key;

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (array == NULL) {
	    return;
	}

	/* ipv4 */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  service_p->serviceID,
							  kSCEntNetIPv4);
	if (key) {
	    CFArrayAppendValue(array, key);
	}
	my_CFRelease(&key);

	/* dns */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  service_p->serviceID,
							  kSCEntNetDNS);
	if (key) {
	    CFArrayAppendValue(array, key);
	}
	my_CFRelease(&key);

	/* netinfo */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  service_p->serviceID,
							  kSCEntNetNetInfo);
	if (key) {
	    CFArrayAppendValue(array, key);
	}
	my_CFRelease(&key);

	/* dhcp */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  service_p->serviceID,
							  kSCEntNetDHCP);
	if (key) {
	    CFArrayAppendValue(array, key);
	}
	my_CFRelease(&key);
	SCDynamicStoreSetMultiple(S_scd_session, NULL, array, NULL);
	my_CFRelease(&array);
    }
    return;
}

static boolean_t
all_services_ready()
{
    int 		i;

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	int		j;
	IFState_t *	ifstate = dynarray_element(&S_ifstate_list, i);

	if (dynarray_count(&ifstate->services) == 0
	    && ifstate->startup_ready == FALSE) {
	    return (FALSE);
	}
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t *	service_p = dynarray_element(&ifstate->services, j);

	    if (service_p->published.ready == FALSE) {
		return (FALSE);
	    }
	}
    }
    unblock_startup(S_scd_session);
    return (TRUE);
}

static boolean_t
cache_key_different(SCDynamicStoreRef session, CFStringRef key, 
		    CFDictionaryRef value)
{
    CFDictionaryRef	cache_value;
    boolean_t		ret = TRUE;

    cache_value = my_SCDynamicStoreCopyValue(session, key);
    if (cache_value) {
	if (CFEqual(value, cache_value)) {
	    ret = FALSE;
	}
	my_CFRelease(&cache_value);
    }
    return (ret);
}

static __inline__ void
update_key(SCDynamicStoreRef session,
	   CFStringRef key, CFDictionaryRef dict,
	   CFMutableDictionaryRef keys_to_set,
	   CFMutableArrayRef keys_to_remove)
{
    if (dict) {
	if (cache_key_different(session, key, dict)) {
	    CFDictionarySetValue(keys_to_set, key, dict);
	}
    }
    else {
	CFArrayAppendValue(keys_to_remove, key);
    }
    return;
}

static void
publish_keys(CFStringRef ipv4_key, CFDictionaryRef ipv4_dict,
	     CFStringRef dns_key, CFDictionaryRef dns_dict, 
	     CFStringRef netinfo_key, CFDictionaryRef netinfo_dict,
	     CFStringRef dhcp_key, CFDictionaryRef dhcp_dict)
{
    CFMutableDictionaryRef	keys_to_set = NULL;
    CFMutableArrayRef		keys_to_remove = NULL;
    
    if (ipv4_dict)
	SCLog(G_verbose, LOG_INFO, CFSTR("%@ = %@"), ipv4_key, ipv4_dict);
    if (dns_dict)
	SCLog(G_verbose, LOG_INFO, CFSTR("%@ = %@"), dns_key, dns_dict);
    if (netinfo_dict)
	SCLog(G_verbose, LOG_INFO, CFSTR("%@ = %@"), netinfo_key, netinfo_dict);
    if (dhcp_dict)
	SCLog(G_verbose, LOG_INFO, CFSTR("%@ = %@"), dhcp_key, dhcp_dict);

    keys_to_set = CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
    keys_to_remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (keys_to_set == NULL || keys_to_remove == NULL) {
	goto done;
    }
    update_key(S_scd_session, ipv4_key, ipv4_dict, keys_to_set, keys_to_remove);
    update_key(S_scd_session, dns_key, dns_dict, keys_to_set, keys_to_remove);
    update_key(S_scd_session, netinfo_key, netinfo_dict, keys_to_set,
	       keys_to_remove);
    update_key(S_scd_session, dhcp_key, dhcp_dict, keys_to_set, keys_to_remove);
    if (CFArrayGetCount(keys_to_remove) > 0 
	|| CFDictionaryGetCount(keys_to_set) > 0) {
	SCDynamicStoreSetMultiple(S_scd_session,
				  keys_to_set,
				  keys_to_remove,
				  NULL);
    }
 done:
    my_CFRelease(&keys_to_remove);
    my_CFRelease(&keys_to_set);
    return;
}

static void
publish_service(CFStringRef serviceID, CFDictionaryRef ipv4_dict,
		CFDictionaryRef dns_dict, CFDictionaryRef netinfo_dict,
		CFDictionaryRef dhcp_dict)
{
    CFStringRef			dhcp_key = NULL;
    CFStringRef			dns_key = NULL;
    CFStringRef			ipv4_key = NULL;
    CFStringRef			netinfo_key = NULL;

    /* create the cache keys */
    ipv4_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							   kSCDynamicStoreDomainState,
							   serviceID, 
							   kSCEntNetIPv4);
    dns_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  serviceID, 
							  kSCEntNetDNS);
    netinfo_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      serviceID, 
							      kSCEntNetNetInfo);
    dhcp_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							   kSCDynamicStoreDomainState,
							   serviceID,
							   kSCEntNetDHCP);
    if (ipv4_key == NULL || dns_key == NULL || netinfo_key == NULL
	|| dhcp_key == NULL) {
	goto done;
    }

    publish_keys(ipv4_key, ipv4_dict, dns_key, dns_dict, 
		 netinfo_key, netinfo_dict, dhcp_key, dhcp_dict);
 done:
    my_CFRelease(&ipv4_key);
    my_CFRelease(&dns_key);
    my_CFRelease(&netinfo_key);
    my_CFRelease(&dhcp_key);
    return;
}

static CFDictionaryRef
make_dhcp_dict(Service_t * service_p)
{
    CFMutableDictionaryRef	dict;
    struct completion_results *	pub;
    int				tag;

    pub = &service_p->published;
    if (pub->pkt == NULL) {
	return (NULL);
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (tag = 1; tag < 255; tag++) {
	CFDataRef	data;
	CFStringRef	key;
	int		len;
	void * 		option;

	option = dhcpol_get(&pub->options, tag, &len);
	if (option == NULL) {
	    continue;
	}
	key = CFStringCreateWithFormat(NULL, NULL, CFSTR("Option_%d"), tag);
	data = CFDataCreate(NULL, option, len);
	if (key != NULL && data != NULL) {
	    CFDictionarySetValue(dict, key, data);
	}
	my_CFRelease(&key);
	my_CFRelease(&data);
	free(option);
    }

    if (service_p->method == ipconfig_method_dhcp_e) {
	CFDateRef	start;
	
	start = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	CFDictionarySetValue(dict, CFSTR("LeaseStartTime"), start);
	my_CFRelease(&start);
    }

    return (dict);
}

void
service_publish_success(Service_t * service_p, void * pkt, int pkt_size)
{
    CFMutableArrayRef		array = NULL;
    CFDictionaryRef		dhcp_dict = NULL;
    CFMutableDictionaryRef	dns_dict = NULL;
    u_char *			dns_domain = NULL;
    int				dns_domain_len = 0;
    struct in_addr *		dns_server = NULL;
    int				dns_server_len = 0;
    int				i;
    char *			host_name = NULL;
    int				host_name_len = 0;
    inet_addrinfo_t *		info_p;
    CFMutableDictionaryRef	ipv4_dict = NULL;
    CFMutableDictionaryRef	netinfo_dict = NULL;
    struct in_addr *		netinfo_addresses = NULL;
    int				netinfo_addresses_len = 0;
    u_char *			netinfo_tag = NULL;
    int				netinfo_tag_len = 0;
    struct completion_results *	pub;
    boolean_t			publish_parent = FALSE;
    struct in_addr *		router = NULL;
    int				router_len = 0;
    CFStringRef			str;

    if (service_p->serviceID == NULL) {
	return;
    }

    service_clear(service_p);
    pub = &service_p->published;
    info_p = &service_p->info;
    pub->ready = TRUE;
    pub->status = ipconfig_status_success_e;

    if (service_p->parent_serviceID != NULL) {
	Service_t *	parent_service_p;

	parent_service_p = IFState_service_with_ID(service_ifstate(service_p), 
						   service_p->parent_serviceID);
	if (parent_service_p == NULL
	    || parent_service_p->info.addr.s_addr != 0) {
	    return;
	}
	publish_parent = TRUE;
    }
    if (pkt_size) {
	pub->pkt = malloc(pkt_size);
	if (pub->pkt == NULL) {
	    my_log(LOG_ERR, "service_publish_success %s: malloc failed",
		   if_name(service_interface(service_p)));
	    all_services_ready();
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

    ipv4_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    /* set the ip address array */
    array = CFArrayCreateMutable(NULL, 1,  &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT),
				   IP_LIST(&info_p->addr));
    if (array && str) {
	CFArrayAppendValue(array, str);
	CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array);
    }
    my_CFRelease(&str);
    my_CFRelease(&array);
    
    /* set the ip mask array */
    array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), 
				   IP_LIST(&info_p->mask));
    if (array && str) {
	CFArrayAppendValue(array, str);
	CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4SubnetMasks, array);
    }
    my_CFRelease(&str);
    my_CFRelease(&array);

    CFDictionarySetValue(ipv4_dict, CFSTR("InterfaceName"),
			 service_ifstate(service_p)->ifname);

    if (service_ifstate(service_p)->netboot
	&& service_p->parent_serviceID == NULL) {
	CFNumberRef	primary;
	int		enabled = 1;

	/* ensure that we're the primary service */
	primary = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
	CFDictionarySetValue(ipv4_dict, kSCPropNetOverridePrimary,
			     primary);
	CFRelease(primary);
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

    /* publish the DHCP options */
    dhcp_dict = make_dhcp_dict(service_p);

    if (publish_parent) {
	publish_service(service_p->parent_serviceID, 
			ipv4_dict, dns_dict,
			netinfo_dict, dhcp_dict);
    }
    else {
	publish_service(service_p->serviceID, ipv4_dict, dns_dict,
			netinfo_dict, dhcp_dict);
    }
    my_CFRelease(&ipv4_dict);
    my_CFRelease(&dns_dict);
    my_CFRelease(&netinfo_dict);
    my_CFRelease(&dhcp_dict);

    all_services_ready();
    return;
}

void
service_publish_failure_sync(Service_t * service_p, ipconfig_status_t status,
			     char * msg, boolean_t sync)
{
    Service_t *	child_service_p = NULL;
    Service_t *	parent_service_p = NULL;

    if (service_p->child_serviceID != NULL) {
	child_service_p = IFState_service_with_ID(service_ifstate(service_p), 
						  service_p->child_serviceID);
    }
    if (service_p->parent_serviceID != NULL) {
	parent_service_p = IFState_service_with_ID(service_ifstate(service_p), 
						   service_p->parent_serviceID);
    }
    if (child_service_p != NULL
	&& child_service_p->info.addr.s_addr) {
	service_publish_success(child_service_p, NULL, 0);
	service_clear(service_p);
    }
    else if (parent_service_p != NULL
	     && parent_service_p->info.addr.s_addr == 0) {
	ipconfig_status_t status;
		
	/* clear the information in the DynamicStore, but not the status */
	status = parent_service_p->published.status;
	service_publish_clear(parent_service_p);
	parent_service_p->published.status = status;
    }
    else {
	service_publish_clear(service_p);
    }
    service_p->published.ready = TRUE;
    service_p->published.status = status;
    if (msg) {
	service_p->published.msg = strdup(msg);
    }
    my_log(LOG_DEBUG, "%s %s: status = '%s'",
	   ipconfig_method_string(service_p->method),
	   if_name(service_interface(service_p)), 
	   ipconfig_status_string(status));
    if (sync == TRUE) {
	all_services_ready();
    }
    return;
}

void
service_publish_failure(Service_t * service_p, ipconfig_status_t status,
			char * msg)
{
    service_publish_failure_sync(service_p, status, msg, TRUE);
    return;
}

static void
arpcache_flush(const struct in_addr ip, const struct in_addr broadcast) 
{
    int		s = arp_get_routing_socket();

    if (s < 0) {
	return;
    }

    /* blow away all non-permanent arp entries */
    (void)arp_flush(s, FALSE);

    /* remove permanent arp entries for the IP and IP broadcast */
    if (ip.s_addr) { 
	(void)arp_delete(s, ip, FALSE);
    }
    if (broadcast.s_addr) { 
	(void)arp_delete(s, broadcast, FALSE);
    }
    close(s);
}

static int
inet_set_autoaddr(char * ifname, int val)
{
    int 		s = inet_dgram_socket();
    int			ret = 0;

    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "inet_set_autoaddr(%s, %d): socket() failed, %s (%d)",
	       ifname, val, strerror(errno), errno);
    }
    else {
	if (siocautoaddr(s, ifname, val) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG, "inet_set_autoaddr(%s, %d) failed, %s (%d)", 
		   ifname, val, strerror(errno), errno);

	}
	close(s);
    }
    return (ret);
}

int
service_enable_autoaddr(Service_t * service_p)
{
    return (inet_set_autoaddr(if_name(service_interface(service_p)), 1));
}

int
service_disable_autoaddr(Service_t * service_p)
{
    arpcache_flush(G_ip_zeroes, G_ip_zeroes);
    return (inet_set_autoaddr(if_name(service_interface(service_p)), 0));
}

static int
inet_attach_interface(char * ifname)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }

    if (siocprotoattach(s, ifname) < 0) {
	ret = errno;
	my_log(LOG_DEBUG, "siocprotoattach(%s) failed, %s (%d)", 
	       ifname, strerror(errno), errno);
    }
    (void)ifflags_set(s, ifname, IFF_UP);
    close(s);

 done:
    return (ret);
}

static int
inet_detach_interface(char * ifname)
{
    int ret = 0;
    int s = inet_dgram_socket();

    if (s < 0) {
	ret = errno;
	goto done;
    }
    if (siocprotodetach(s, ifname) < 0) {
	ret = errno;
	my_log(LOG_DEBUG, "siocprotodetach(%s) failed, %s (%d)", 
	       ifname, strerror(errno), errno);
    }
    close(s);

 done:
    return (ret);
}

static boolean_t
host_route(int cmd, struct in_addr iaddr)
{
    int 			len;
    boolean_t			ret = TRUE;
    int 			rtm_seq = 0;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
    } 				rtmsg;
    int 			sockfd = -1;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	my_log(LOG_INFO, "host_route: open routing socket failed, %s",
	       strerror(errno));
	ret = FALSE;
	goto done;
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC | RTF_HOST;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.dst.sin_addr = iaddr;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    len = sizeof(rtmsg);
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
	my_log(LOG_DEBUG, "host_route: write routing socket failed, %s",
	       strerror(errno));
	ret = FALSE;
    }
 done:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (ret);
}


static boolean_t
subnet_route(int cmd, struct in_addr gateway, struct in_addr netaddr, 
	     struct in_addr netmask, char * ifname)
{
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

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	my_log(LOG_INFO, "subnet_route: open routing socket failed, %s",
	       strerror(errno));
	ret = FALSE;
	goto done;
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC | RTF_CLONING;
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
	int	error = errno;

	switch (error) {
	case ESRCH:
	case EEXIST:
	    my_log(LOG_DEBUG, "subnet_route: write routing socket failed, %s",
		   strerror(error));
	    break;
	default:
	    my_log(LOG_INFO, "subnet_route: write routing socket failed, %s",
		   strerror(error));
	    break;
	}
	ret = FALSE;
    }
 done:
    if (sockfd >= 0) {
	close(sockfd);
    }
    return (ret);
}

boolean_t
subnet_route_add(struct in_addr gateway, struct in_addr netaddr, 
		 struct in_addr netmask, char * ifname)
{
    return (subnet_route(RTM_ADD, gateway, netaddr, netmask, ifname));
}

boolean_t
subnet_route_delete(struct in_addr gateway, struct in_addr netaddr, 
		    struct in_addr netmask, char * ifname)
{
    return (subnet_route(RTM_DELETE, gateway, netaddr, netmask, ifname));
}

#define RANK_LOWEST	(1024 * 1024)
#define RANK_NONE	(RANK_LOWEST + 1)

static unsigned int
S_get_service_rank(CFArrayRef arr, Service_t * service_p)
{
    int i;
    CFStringRef serviceID = service_p->serviceID;

    if (service_ifstate(service_p)->netboot
	&& service_p->method == ipconfig_method_dhcp_e) {
	/* the netboot service is the best service */
	return (0);
    }
    if (serviceID != NULL && arr != NULL) {
	for (i = 0; i < CFArrayGetCount(arr); i++) {
	    CFStringRef s = isA_CFString(CFArrayGetValueAtIndex(arr, i));

	    if (s == NULL) {
		continue;
	    }
	    if (CFEqual(serviceID, s)) {
		return (i);
	    }
	}
    }
    return (RANK_LOWEST);
}

static CFArrayRef
S_get_service_order(SCDynamicStoreRef session)
{
    CFArrayRef	 		order = NULL;
    CFStringRef 		ipv4_key = NULL;
    CFDictionaryRef 		ipv4_dict = NULL;

    if (session == NULL)
	goto done;

    ipv4_key
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    if (ipv4_key == NULL) {
	goto done;
    }
    ipv4_dict = my_SCDynamicStoreCopyValue(session, ipv4_key);
    if (ipv4_dict != NULL) {
	order = CFDictionaryGetValue(ipv4_dict, kSCPropNetServiceOrder);
	order = isA_CFArray(order);
	if (order) {
	    CFRetain(order);
	}
    }

 done:
    my_CFRelease(&ipv4_key);
    my_CFRelease(&ipv4_dict);
    return (order);
}

/*
 * Function: S_subnet_route_service
 * Purpose:
 *   Given a subnet (expressed as a network/netmask pair), count the
 *   number of services on that subnet, and return the one with the
 *   highest priority.
 */
static boolean_t
S_subnet_route_service(CFArrayRef service_order,
		       struct in_addr netaddr, struct in_addr netmask, 
		       Service_t * * ret_service_p, int * count_p)
{
    unsigned int	best_rank = RANK_NONE;
    Service_t *		best_service_p = NULL;
    int			match_count = 0;
    int 		i;
    boolean_t		ret = TRUE;

    if (service_order == NULL) {
	return (FALSE);
    }
    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	unsigned int	rank;
	IFState_t *	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;
	
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t * 		service_p;
	    inet_addrinfo_t *		info_p;

	    service_p = dynarray_element(&ifstate->services, j);
	    info_p = &service_p->info;

	    if (info_p->addr.s_addr == 0) {
		continue;
	    }
	    if (netmask.s_addr != info_p->mask.s_addr
		|| in_subnet(netaddr, netmask, info_p->addr) == FALSE) {
		continue;
	    }
	    match_count++;
	    rank = S_get_service_rank(service_order, service_p);
	    if (rank < best_rank) {
		best_service_p = service_p;
		best_rank = rank;
	    }
	}
    }
    if (count_p) {
	*count_p = match_count;
    }
    if (ret_service_p) {
	*ret_service_p = best_service_p;
    }
    return (ret);
}

/*
 * Function: order_services
 * Purpose:
 *   Ensure that the service with highest priority owns its
 *   associated subnet route.
 */
static void
order_services(SCDynamicStoreRef session)
{
    int 		i;
    CFArrayRef		service_order = NULL;

    if (S_scd_session == NULL) {
	return;
    }
    service_order = S_get_service_order(S_scd_session);
    if (service_order == NULL) {
	return;
    }

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	IFState_t *		ifstate = dynarray_element(&S_ifstate_list, i);
	int			j;

	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t *			best_service_p = NULL;
	    inet_addrinfo_t *		info_p;
	    Service_t * 		service_p;
	    int				subnet_count = 0;

	    service_p = dynarray_element(&ifstate->services, j);
	    info_p = &service_p->info;

	    if (info_p->addr.s_addr == 0) {
		continue;
	    }
	    if (S_subnet_route_service(service_order, 
				       info_p->netaddr, info_p->mask,
				       &best_service_p, 
				       &subnet_count) == TRUE) {
		/* adjust the subnet route if multiple interfaces on subnet */
		if (best_service_p && subnet_count > 1) {
		    subnet_route_delete(G_ip_zeroes, info_p->netaddr, 
					info_p->mask, NULL);
		    subnet_route_add(best_service_p->info.addr,
				     info_p->netaddr, info_p->mask, 
				     if_name(service_interface(best_service_p)));
		    arpcache_flush(G_ip_zeroes, info_p->broadcast);
		}
	    }
	}
    }
    S_linklocal_election_required = TRUE;
    my_CFRelease(&service_order);
    return;
}

/*
 * Function: service_parent_service
 * Purpose:
 *   Return the parent service pointer of the given service, if the
 *   parent is valid.
 */
Service_t *
service_parent_service(Service_t * service_p)
{
    if (service_p == NULL || service_p->parent_serviceID == NULL) {
	return (NULL);
    }
    return (IFState_service_with_ID(service_ifstate(service_p), 
				    service_p->parent_serviceID));
}

/*
 * Function: linklocal_service_change
 *
 * Purpose:
 *   If we're the parent of the link-local service, 
 *   send a change message to the link-local service, asking it to
 *   either allocate to not allocate an IP.
 */
void
linklocal_service_change(Service_t * parent_service_p, boolean_t no_allocate)
{
    ipconfig_method_data_t	data;
    Service_t *			ll_parent_p;
    boolean_t			needs_stop;

    if (IFStateList_linklocal_service(&S_ifstate_list, NULL) != NULL) {
	/* don't touch user-configured link-local services */
	return;
    }
    ll_parent_p = service_parent_service(S_linklocal_service_p);
    if (ll_parent_p == NULL) {
	S_linklocal_election_required = TRUE;
	return;
    }
    if (parent_service_p != ll_parent_p) {
	/* we're not the one that triggered the link-local service */
	S_linklocal_election_required = TRUE;
	return;
    }
    bzero(&data, sizeof(data));
    data.reserved_0 = no_allocate;
    (void)config_method_change(S_linklocal_service_p,
			       ipconfig_method_linklocal_e,
			       &data, sizeof(data), &needs_stop);
    return;
}

/*
 * Function: S_linklocal_start
 * Purpose:
 *   Start a child link-local service for the given parent service.
 */
static void
S_linklocal_start(Service_t * parent_service_p, boolean_t no_allocate)

{
    ipconfig_method_data_t	data;
    IFState_t *			ifstate = service_ifstate(parent_service_p);
    Service_t *			service_p;
    ipconfig_status_t		status;

    bzero(&data, sizeof(data));
    data.reserved_0 = no_allocate;
    status = IFState_service_add(ifstate, NULL, ipconfig_method_linklocal_e,
				 &data, sizeof(data), parent_service_p,
				 &service_p);
    if (status != ipconfig_status_success_e) {
	my_log(LOG_INFO, 
	       "ipconfigd: failed to start link-local service on %s, %s",
	       if_name(ifstate->if_p),
	       ipconfig_status_string(status));
    }
    return;
}

/*
 * Function: S_linklocal_elect
 * Purpose:
 *   Create a new link-local service whose parent is an existing
 *   service.
 *
 *   If there is an existing user-configured link-local service,
 *   do nothing.
 *
 *   Traverse the list of services, and find the highest-priority active
 *   service.  This service will be the parent of the link-local service.
 *   If there is an existing link-local service, and its parent
 *   is different than the one we just elected, stop the service before
 *   starting the new service.
 *
 */
static void
S_linklocal_elect(CFArrayRef service_order)
{
    unsigned int	best_rank = RANK_NONE;
    Service_t *		best_service_p = NULL;
    int 		i;
    Service_t * 	ll_parent_p = NULL;

    if (IFStateList_linklocal_service(&S_ifstate_list, NULL) != NULL) {
	/* don't touch user-configured link-local services */
	return;
    }

    if (service_order == NULL) {
	return;
    }

    if (S_linklocal_service_p != NULL) {
	ll_parent_p = service_parent_service(S_linklocal_service_p);
	if (ll_parent_p == NULL) {
	    /* the parent of the link-local service is gone */
	    if (G_verbose) {
		my_log(LOG_INFO, "link-local parent is gone");
	    }
	    IFState_service_free(service_ifstate(S_linklocal_service_p), 
				 S_linklocal_service_p->serviceID);
	    /* side-effect: S_linklocal_service_p = NULL */
	}
    }
    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	unsigned int	rank;
	IFState_t *	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	if (ifstate->free_in_progress == TRUE) {
	    continue;
	}
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t * 		service_p;
	    inet_addrinfo_t *		info_p;

	    service_p = dynarray_element(&ifstate->services, j);
	    if (service_p->free_in_progress == TRUE
		|| service_p->method == ipconfig_method_linklocal_e) {
		continue;
	    }
	    info_p = &service_p->info;
	    if (info_p->addr.s_addr == 0) {
		if (service_p->method != ipconfig_method_dhcp_e
		    || G_dhcp_failure_configures_linklocal == FALSE
		    || (service_p->published.status 
			!= ipconfig_status_no_server_e)) {
		    continue;
		}
	    }
	    rank = S_get_service_rank(service_order, service_p);
	    if (rank < best_rank) {
		best_service_p = service_p;
		best_rank = rank;
	    }
	}
    }
    if (ll_parent_p != best_service_p) {
	if (ll_parent_p != NULL) {
	    IFState_service_free(service_ifstate(S_linklocal_service_p), 
				 S_linklocal_service_p->serviceID);
	}
	if (best_service_p != NULL) {
	    boolean_t	no_allocate = TRUE;

	    if (best_service_p->info.addr.s_addr == 0) {
		no_allocate = FALSE;
	    }
	    S_linklocal_start(best_service_p, no_allocate);
	}
    }
    return;
}

int
service_set_address(Service_t * service_p, 
		    struct in_addr addr,
		    struct in_addr mask, 
		    struct in_addr broadcast)
{
    Service_t * 	best_service_p = NULL;
    interface_t *	if_p = service_interface(service_p);
    int			ret = 0;
    struct in_addr	netaddr = { 0 };
    int 		s = inet_dgram_socket();
    CFArrayRef		service_order = NULL;

    if (mask.s_addr == 0) {
	u_int32_t ipval = ntohl(addr.s_addr);

	if (IN_CLASSA(ipval)) {
	    mask.s_addr = htonl(IN_CLASSA_NET);
	}
	else if (IN_CLASSB(ipval)) {
	    mask.s_addr = htonl(IN_CLASSB_NET);
	}
	else {
	    mask.s_addr = htonl(IN_CLASSC_NET);
	}
    }
    if (broadcast.s_addr == 0) {
	broadcast = hltoip(iptohl(addr) | ~iptohl(mask));
    }
    netaddr = hltoip(iptohl(addr) & iptohl(mask));

    my_log(LOG_DEBUG, 
	   "service_set_address(%s): " IP_FORMAT " netmask " IP_FORMAT 
	   " broadcast " IP_FORMAT, if_name(if_p), 
	   IP_LIST(&addr), IP_LIST(&mask), IP_LIST(&broadcast));
    if (s < 0) {
	ret = errno;
	my_log(LOG_ERR, 
	       "service_set_address(%s): socket() failed, %s (%d)",
	       if_name(if_p), strerror(errno), errno);
    }
    else {
	inet_addrinfo_t *	info_p = &service_p->info;

	if (inet_aifaddr(s, if_name(if_p), &addr, &mask, &broadcast) < 0) {
	    ret = errno;
	    my_log(LOG_DEBUG, "service_set_address(%s) " 
		   IP_FORMAT " inet_aifaddr() failed, %s (%d)", if_name(if_p),
		   IP_LIST(&addr), strerror(errno), errno);
	}
	if_setflags(if_p, if_flags(if_p) | IFF_UP);
	ifflags_set(s, if_name(if_p), IFF_UP);
	bzero(info_p, sizeof(*info_p));
	info_p->addr = addr;
	info_p->mask = mask;
	info_p->netaddr = netaddr;
	info_p->broadcast = broadcast;
	close(s);
	(void)host_route(RTM_DELETE, addr);
	(void)host_route(RTM_ADD, addr);
    }
    service_order = S_get_service_order(S_scd_session);
    if (S_subnet_route_service(service_order,
			       netaddr, mask, &best_service_p, NULL) 
	== TRUE) {
	subnet_route_delete(G_ip_zeroes, netaddr, mask, NULL);
	if (best_service_p) {
	    subnet_route_add(best_service_p->info.addr, 
			     netaddr, mask, 
			     if_name(service_interface(best_service_p)));
	}
    }
    arpcache_flush(G_ip_zeroes, broadcast);
    S_linklocal_election_required = TRUE;
    my_CFRelease(&service_order);
    return (ret);
}

int
service_remove_address(Service_t * service_p)
{
    Service_t * 	best_service_p = NULL;
    interface_t *	if_p = service_interface(service_p);
    inet_addrinfo_t *	info_p = &service_p->info;
    int			ret = 0;
    CFArrayRef		service_order = NULL;

    service_order = S_get_service_order(S_scd_session);

    if (info_p->addr.s_addr != 0) {
	inet_addrinfo_t		saved_info;

	/* copy IP info then clear it so that it won't be elected */
	saved_info = service_p->info;
	bzero(info_p, sizeof(*info_p));

	/* if no service on this interface refers to this IP, remove the IP */
	if (IFState_service_with_ip(service_ifstate(service_p),
				    saved_info.addr) == NULL) {
	    int 		s;

	    /*
	     * This can only happen if there's a manual/inform service 
	     * and a BOOTP/DHCP service with the same IP.  Duplicate
	     * manual/inform services are prevented when created.
	     */
	    s = inet_dgram_socket();
	    my_log(LOG_DEBUG, "service_remove_address(%s) " IP_FORMAT, 
		   if_name(if_p), IP_LIST(&saved_info.addr));
	    if (s < 0) {
		ret = errno;
		my_log(LOG_DEBUG, 
		       "service_remove_address(%s) socket() failed, %s (%d)",
		       if_name(if_p), strerror(errno), errno);
	    }	
	    else {
		if (inet_difaddr(s, if_name(if_p), &saved_info.addr) < 0) {
		    ret = errno;
		    my_log(LOG_DEBUG, "service_remove_address(%s) " 
			   IP_FORMAT " failed, %s (%d)", if_name(if_p),
			   IP_LIST(&saved_info.addr), strerror(errno), errno);
		}
		close(s);
	    }
	}
	/* if no service refers to this IP, remove the host route for the IP */
	if (IFStateList_service_with_ip(&S_ifstate_list, 
					saved_info.addr, NULL) == NULL) {
	    (void)host_route(RTM_DELETE, saved_info.addr);
	}
	arpcache_flush(saved_info.addr, saved_info.broadcast);
	if (S_subnet_route_service(service_order, saved_info.netaddr, 
				   saved_info.mask, 
				   &best_service_p, NULL) == TRUE) {
	    subnet_route_delete(G_ip_zeroes, saved_info.netaddr, 
				saved_info.mask, NULL);
	    if (best_service_p) {
		subnet_route_add(best_service_p->info.addr, 
				 saved_info.netaddr, saved_info.mask, 
				 if_name(service_interface(best_service_p)));
	    }
	}
    }
    /* determine a new automatic link-local service if necessary */
    S_linklocal_election_required = TRUE;
    my_CFRelease(&service_order);
    return (ret);
}

static void
set_loopback()
{
    struct in_addr	loopback;
    struct in_addr	loopback_mask;
    int 		s = inet_dgram_socket();

    if (s < 0) {
	my_log(LOG_ERR, 
	       "set_loopback(): socket() failed, %s (%d)",
	       strerror(errno), errno);
	return;
    }
    loopback.s_addr = htonl(INADDR_LOOPBACK);
    loopback_mask.s_addr = htonl(IN_CLASSA_NET);
    if (inet_aifaddr(s, "lo0", &loopback, &loopback_mask, NULL) < 0) {
	my_log(LOG_DEBUG, "set_loopback: inet_aifaddr() failed, %s (%d)", 
	       strerror(errno), errno);
    }
    close(s);
}

static boolean_t
service_get_option(Service_t * service_p, int option_code, void * option_data,
		   unsigned int * option_dataCnt)
{
    boolean_t ret = FALSE;

    switch (service_p->method) {
      case ipconfig_method_inform_e:
      case ipconfig_method_dhcp_e:
      case ipconfig_method_bootp_e: {
	  void * data;
	  int	 len;
	  
	  if (service_p->published.ready == FALSE 
	      || service_p->published.pkt == NULL) {
	      break; /* out of switch */
	  }
	  data = dhcpol_find(&service_p->published.options, option_code,
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
    return (dynarray_count(&S_ifstate_list));
}

boolean_t 
get_if_name(int intface, char * name)
{
    boolean_t		ret = FALSE;
    IFState_t * 	s;

    s = dynarray_element(&S_ifstate_list, intface);
    if (s) {
	strcpy(name, if_name(s->if_p));
	ret = TRUE;
    }
    return (ret);
}

boolean_t 
get_if_addr(char * name, u_int32_t * addr)
{
    IFState_t * 	ifstate;
    int			j;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (FALSE);
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t * service_p = dynarray_element(&ifstate->services, j);

	if (service_p->info.addr.s_addr != 0) {
	    *addr = service_p->info.addr.s_addr;
	    return (TRUE);
	}
    }
    return (FALSE);
}

boolean_t
get_if_option(char * name, int option_code, void * option_data, 
	      unsigned int * option_dataCnt)
{
    int 		i;
    boolean_t		ret = FALSE;

    for (i = 0; i < dynarray_count(&S_ifstate_list);  i++) {
	IFState_t * 	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;
	boolean_t 	name_match = FALSE;

	if (name[0] != '\0') {
	    if (strcmp(if_name(ifstate->if_p), name) == 0) {
		name_match = TRUE;
	    }
	    else {
		continue;
	    }
	}
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t * service_p = dynarray_element(&ifstate->services, j);

	    ret = service_get_option(service_p, option_code, option_data,
				     option_dataCnt);
	    if (ret == TRUE) {
		break; /* out of inner for() */
	    }
	}
	if (ret == TRUE || name_match == TRUE) {
	    break; /* out of outer for() */
	}
    } /* for */
    return (ret);
}

boolean_t
get_if_packet(char * name, void * packet_data, unsigned int * packet_dataCnt)
{
    IFState_t * 	ifstate;
    int			j;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, name, NULL);
    if (ifstate == NULL) {
	return (FALSE);
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t * service_p = dynarray_element(&ifstate->services, j);
	    
	switch (service_p->method) {
	case ipconfig_method_inform_e:
	case ipconfig_method_dhcp_e:
	case ipconfig_method_bootp_e:
	    if (service_p->published.ready == FALSE 
		|| service_p->published.pkt == NULL
		|| service_p->published.pkt_size > *packet_dataCnt) {
		break; /* out of switch */
	    }
	    *packet_dataCnt = service_p->published.pkt_size;
	    bcopy(service_p->published.pkt, packet_data, *packet_dataCnt);
	    return (TRUE);
	    break;
	default:
	    break;
	} /* switch */
    } /* for */
    return (FALSE);
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
      case ipconfig_method_linklocal_e: {
	  return linklocal_thread;
	  break;
      }
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
config_method_start(Service_t * service_p, ipconfig_method_t method,
		    ipconfig_method_data_t * data,
		    unsigned int data_len)
{
    start_event_data_t		start_data;
    ipconfig_func_t *		func;
    interface_t * 		if_p = service_interface(service_p);

    /*
     * The correct check is for broadcast interfaces, but
     * since bpf only works with ethernet currently, 
     * we make sure it's ethernet as well.
     */
    if ((if_flags(if_p) & IFF_BROADCAST) == 0
	|| (if_link_arptype(if_p) != ARPHRD_ETHER)) {
	switch (method) {
	  case ipconfig_method_linklocal_e:
	  case ipconfig_method_inform_e:
	  case ipconfig_method_dhcp_e:
	  case ipconfig_method_bootp_e:
	      /* no ARP/DHCP/BOOTP over non-broadcast interfaces */
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
    return (*func)(service_p, IFEventID_start_e, &start_data);
}

static ipconfig_status_t
config_method_change(Service_t * service_p, ipconfig_method_t method,
		     ipconfig_method_data_t * data,
		     unsigned int data_len, boolean_t * needs_stop)
{
    change_event_data_t		change_data;
    ipconfig_func_t *		func;
    ipconfig_status_t		status;

    *needs_stop = FALSE;

    func = lookup_func(method);
    if (func == NULL) {
	return (ipconfig_status_operation_not_supported_e);
    }
    change_data.config.data = data;
    change_data.config.data_len = data_len;
    change_data.needs_stop = FALSE;
    status = (*func)(service_p, IFEventID_change_e, &change_data);
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
config_method_event(Service_t * service_p, IFEventID_t event, void * data)
{
    ipconfig_status_t	status = ipconfig_status_success_e;
    ipconfig_func_t *	func;
    ipconfig_method_t	method = service_p->method;

    func = lookup_func(method);
    if (func == NULL) {
	SCLog(TRUE, LOG_INFO, 
	      CFSTR("config_method_event(%s): lookup_func(%d) failed"), 
	      IFEventID_names(event), method);
	status = ipconfig_status_internal_error_e;
	goto done;
    }
    (*func)(service_p, event, data);

 done:
    return (status);
    
}

static ipconfig_status_t
config_method_stop(Service_t * service_p)
{
    return (config_method_event(service_p, IFEventID_stop_e, NULL));
}

static ipconfig_status_t
config_method_media(Service_t * service_p)
{
    return (config_method_event(service_p, IFEventID_media_e, NULL));
}

static ipconfig_status_t
config_method_arp_collision(Service_t * service_p, 
			    arp_collision_data_t * evdata)
{
    return (config_method_event(service_p, IFEventID_arp_collision_e, 
				(void *)evdata));
}

static ipconfig_status_t
config_method_renew(Service_t * service_p)
{
    return (config_method_event(service_p, IFEventID_renew_e, NULL));
}

ipconfig_status_t
set_if(char * name, ipconfig_method_t method,
       void * method_data, unsigned int method_data_len,
       void * serviceID)
{
    interface_t * 	if_p = ifl_find_name(S_interfaces, name);
    IFState_t *   	ifstate;

    if (G_verbose)
	my_log(LOG_INFO, "set %s %s", name, ipconfig_method_string(method));
    if (if_p == NULL) {
	my_log(LOG_INFO, "set: unknown interface %s", name);
	return (ipconfig_status_interface_does_not_exist_e);
    }
    ifstate = IFStateList_ifstate_create(&S_ifstate_list, if_p);
    if (ifstate == NULL) {
	return (ipconfig_status_allocation_failed_e);
    }
    /* stop existing services */
    IFState_services_free(ifstate);

    if (method == ipconfig_method_none_e) {
	return (ipconfig_status_success_e);
    }

    /* add a new service */
    return (IFState_service_add(ifstate, serviceID, method, method_data,
				method_data_len, NULL, NULL));
}

static boolean_t
get_media_status(char * name, boolean_t * media_status) 

{
    boolean_t	media_valid = FALSE;
    int		sockfd;
	    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	my_log(LOG_INFO, "get_media_status (%s): socket failed, %s", 
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
    	case ipconfig_method_linklocal_e:
	    return (kSCValNetIPv4ConfigMethodLinkLocal);
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
    else if (CFEqual(m, kSCValNetIPv4ConfigMethodLinkLocal)) {
	*method = ipconfig_method_linklocal_e;
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
	my_log(LOG_ERR, "ipconfigd: configuration method is missing/invalid");
	goto error;
    }
    addresses = CFDictionaryGetValue(dict, kSCPropNetIPv4Addresses);
    masks = CFDictionaryGetValue(dict, kSCPropNetIPv4SubnetMasks);
    client_id = CFDictionaryGetValue(dict, 
				     kSCPropNetIPv4DHCPClientID);
    if (addresses) {
	count = CFArrayGetCount(addresses);
	if (count == 0) {
		my_log(LOG_ERR, 
		       "ipconfigd: address array empty");
		goto error;
	}
	if (masks) {
	    if (count != CFArrayGetCount(masks)) {
		my_log(LOG_ERR, 
		       "ipconfigd: address/mask arrays not same size");
		goto error;
	    }
	}
    }
    switch (*method) {
        case ipconfig_method_inform_e:
	    if (addresses == NULL) {
		my_log(LOG_ERR, 
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
		my_log(LOG_ERR, 
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
	my_log(LOG_ERR, 
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
load_cache_from_iftab(SCDynamicStoreRef session)
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

	serviceID = CFStringCreateWithFormat(NULL, NULL, CFSTR("iftab%d"), i);
	if (serviceID == NULL) {
	    goto loop_done;
	}
	ipv4_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							       kSCDynamicStoreDomainSetup,
							       serviceID,
							       kSCEntNetIPv4);
	if (ipv4_key == NULL) {
	    goto loop_done;
	}
	interface_key 
	    = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  serviceID,
							  kSCEntNetInterface);
	if (interface_key == NULL) {
	    goto loop_done;
	}
	if (ipcfg == NULL || ipcfg->method == ipconfig_method_none_e) {
	    /* if it was there, remove it */
	    (void)SCDynamicStoreRemoveValue(session, ipv4_key);
	    (void)SCDynamicStoreRemoveValue(session, interface_key);
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
		(void)IFStateList_ifstate_create(&S_ifstate_list, if_p);
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

	    (void)SCDynamicStoreSetValue(session, ipv4_key, ipv4_dict);
	    (void)SCDynamicStoreSetValue(session, interface_key, 
					 interface_dict);
	}
    loop_done:
	my_CFRelease(&serviceID);
	my_CFRelease(&ipv4_key);
	my_CFRelease(&interface_key);
	my_CFRelease(&ipv4_dict);
	my_CFRelease(&interface_dict);
    }
    skey 
      = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, 
						   kSCDynamicStoreDomainSetup, 
						   kSCEntNetIPv4);
    if (skey == NULL)
	goto done;
    if (CFArrayGetCount(service_array) == 0) {
	(void)SCDynamicStoreRemoveValue(session, skey);
	/* no interfaces, startup is complete */
	unblock_startup(session);
    }
    else {
	CFMutableDictionaryRef	sdict;
	
	sdict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(sdict, kSCPropNetServiceOrder, service_array);

	(void)SCDynamicStoreSetValue(session, skey, sdict);
	my_CFRelease(&sdict);
    }
 done:
    my_CFRelease(&skey);
    my_CFRelease(&service_array);
    return;
}

static __inline__ CFArrayRef
get_order_array_from_values(CFDictionaryRef values, CFStringRef order_key)
{
    CFDictionaryRef	dict;
    CFArrayRef		order_array = NULL;

    dict = isA_CFDictionary(CFDictionaryGetValue(values, order_key));
    if (dict) {
	order_array = CFDictionaryGetValue(dict, 
					   kSCPropNetServiceOrder);
	order_array = isA_CFArray(order_array);
	if (order_array && CFArrayGetCount(order_array) == 0) {
	    order_array = NULL;
	}
    }
    return (order_array);
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

static CFComparisonResult
compare_serviceIDs(const void *val1, const void *val2, void *context)
{
    CFArrayRef		order_array = (CFArrayRef)context;
    int			rank1;
    int			rank2;

    rank1 = lookup_order(order_array, (CFStringRef)val1);
    rank2 = lookup_order(order_array, (CFStringRef)val2);
    if (rank1 == rank2)
	return (kCFCompareEqualTo);
    if (rank1 < rank2)
	return (kCFCompareLessThan);
    return (kCFCompareGreaterThan);
}

static CFArrayRef
entity_all(SCDynamicStoreRef session)
{
    CFMutableArrayRef		all_services = NULL;
    int				count;
    CFMutableArrayRef		get_keys = NULL;
    CFMutableArrayRef		get_patterns = NULL;
    int				i;
    CFStringRef			key = NULL;
    void * *	 		keys = NULL;
    CFMutableArrayRef		service_IDs = NULL;
    CFStringRef			order_key = NULL;
    CFArrayRef			order_array = NULL;
    CFDictionaryRef		values = NULL;

    get_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    get_patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    service_IDs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    all_services = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (get_keys == NULL || get_patterns == NULL || service_IDs == NULL
	|| all_services == NULL) {
	goto done;
    }

    /* populate patterns array for Setup:/Network/Service/any/{IPv4,Interface} */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetIPv4);
    if (key == NULL) {
	goto done;
    }
    CFArrayAppendValue(get_patterns, key);
    my_CFRelease(&key);
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetInterface);
    if (key == NULL) {
	goto done;
    }
    CFArrayAppendValue(get_patterns, key);
    my_CFRelease(&key);

    /* populate keys array to get Setup:/Network/Global/IPv4 */
    order_key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							   kSCDynamicStoreDomainSetup,
							   kSCEntNetIPv4);
    if (order_key == NULL) {
	goto done;
    }
    CFArrayAppendValue(get_keys, order_key);

    /* get keys and values atomically */
    values = SCDynamicStoreCopyMultiple(session, get_keys, get_patterns);
    if (values == NULL) {
	goto done;
    }

    /* if there are no values, we're done */
    count = CFDictionaryGetCount(values);
    if (count == 0) {
	goto done;
    }

    /* build a list of configured service ID's */
    keys = (void * *)malloc(sizeof(void *) * count);
    if (keys == NULL) {
	goto done;
    }
    CFDictionaryGetKeysAndValues(values, (const void * *)keys, NULL);
    for (i = 0; i < count; i++) {
	CFStringRef		serviceID;
	
	serviceID = parse_component(keys[i], S_setup_service_prefix);
	if (serviceID == NULL) {
	    continue;
	}
	my_CFArrayAppendUniqueValue(service_IDs, serviceID);
	my_CFRelease(&serviceID);
    }
    free(keys);
    keys = NULL;

    /* sort the list according to the defined service order */
    order_array = get_order_array_from_values(values, order_key);
    if (order_array && CFArrayGetCount(service_IDs) > 0) {
	CFRange range =  CFRangeMake(0, CFArrayGetCount(service_IDs));
	
	CFArraySortValues(service_IDs, range, compare_serviceIDs, 
			  (void *)order_array);
    }

    /* populate all_services array with annotated IPv4 dict's */
    for (i = 0; i < CFArrayGetCount(service_IDs); i++) {
	CFStringRef 		key = NULL;
	CFDictionaryRef		if_dict;
	CFStringRef 		ifn_cf;
	CFDictionaryRef		ipv4_dict;
	CFMutableDictionaryRef	service_dict = NULL;
	CFStringRef		serviceID;
	CFStringRef		type = NULL;
	
	serviceID = CFArrayGetValueAtIndex(service_IDs, i);
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  serviceID,
							  kSCEntNetInterface);
	if (key == NULL) {
	    goto loop_done;
	}
	if_dict = CFDictionaryGetValue(values, key);
	my_CFRelease(&key);
	if_dict = isA_CFDictionary(if_dict);
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
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  serviceID,
							  kSCEntNetIPv4);
	if (key == NULL) {
	    goto loop_done;
	}
	ipv4_dict = CFDictionaryGetValue(values, key);
	my_CFRelease(&key);
	ipv4_dict = isA_CFDictionary(ipv4_dict);
	if (ipv4_dict == NULL) {
	    goto loop_done;
	}
	service_dict = CFDictionaryCreateMutableCopy(NULL, 0, ipv4_dict);
	if (service_dict == NULL) {
	    goto loop_done;
	}
	/* annotate with serviceID and interface name */
	CFDictionarySetValue(service_dict, kSCPropNetInterfaceDeviceName, 
			     ifn_cf);
	CFDictionarySetValue(service_dict, PROP_SERVICEID, serviceID);
	CFArrayAppendValue(all_services, service_dict);
    loop_done:
	my_CFRelease(&service_dict);
    }

 done:
    my_CFRelease(&values);
    my_CFRelease(&order_key);
    my_CFRelease(&get_keys);
    my_CFRelease(&get_patterns);
    my_CFRelease(&service_IDs);
    if (all_services == NULL || CFArrayGetCount(all_services) == 0) {
	my_CFRelease(&all_services);
    }
    return (all_services);
}


static CFDictionaryRef
lookup_entity(CFArrayRef all, CFStringRef ifn_cf)
{
    int 		i;

    if (all == NULL)
	return (NULL);

    for (i = 0; i < CFArrayGetCount(all); i++) {
	CFDictionaryRef	item = CFArrayGetValueAtIndex(all, i);
	CFStringRef	name;

	name = CFDictionaryGetValue(item, kSCPropNetInterfaceDeviceName);
	if (CFEqual(name, ifn_cf)) {
	    return (item);
	}
    }
    return (NULL);
}

static CFArrayRef
interface_services_copy(CFArrayRef all, CFStringRef ifn_cf)
{
    int 		i;
    CFMutableArrayRef	list = NULL;

    if (all == NULL) {
	return (NULL);
    }

    list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (list == NULL) {
	return (NULL);
    }
    for (i = 0; i < CFArrayGetCount(all); i++) {
	CFDictionaryRef	item = CFArrayGetValueAtIndex(all, i);
	CFStringRef	name;

	name = CFDictionaryGetValue(item, kSCPropNetInterfaceDeviceName);
	if (CFEqual(name, ifn_cf)) {
	    CFArrayAppendValue(list, item);
	}
    }
    if (CFArrayGetCount(list) == 0) {
	my_CFRelease(&list);
    }
    return (list);
}

typedef struct {
    CFStringRef			serviceID;
    ipconfig_method_t		method;
    ipconfig_method_data_t *	method_data;
    int				method_data_len;
} ServiceConfig_t;

static void
ServiceConfig_list_free(ServiceConfig_t * * list_p_p, int count)
{
    int 		i;
    ServiceConfig_t * 	list_p = *list_p_p;

    for (i = 0; i < count; i++) {
	if (list_p[i].serviceID)
	    my_CFRelease(&list_p[i].serviceID);
	if (list_p[i].method_data)
	    free(list_p[i].method_data);
    }
    free(list_p);
    *list_p_p = NULL;
    return;
}

static __inline__ boolean_t
ipconfig_method_is_dynamic(ipconfig_method_t method)
{
    if (method == ipconfig_method_dhcp_e
	|| method == ipconfig_method_bootp_e) {
	return (TRUE);
    }
    return (FALSE);
}

static __inline__ boolean_t
ipconfig_method_is_manual(ipconfig_method_t method)
{
    if (method == ipconfig_method_manual_e
	|| method == ipconfig_method_inform_e) {
	return (TRUE);
    }
    return (FALSE);
}

static ServiceConfig_t *
ServiceConfig_list_lookup_method(ServiceConfig_t * config_list, int count, 
				 ipconfig_method_t method, 
				 ipconfig_method_data_t * method_data,
				 int method_data_len)
{
    ServiceConfig_t *	config;
    int 		i;

    switch (method) {
      case ipconfig_method_linklocal_e: {
	  for (config = config_list, i = 0; i < count; i++, config++) {
	      if (method == config->method) {
		return (config);
	      }
	  }
	  break;
      }
      case ipconfig_method_dhcp_e:
      case ipconfig_method_bootp_e: {
	  for (config = config_list, i = 0; i < count; i++, config++) {
	      if (ipconfig_method_is_dynamic(config->method))
		return (config);
	  }
	  break;
      }
      case ipconfig_method_manual_e:
      case ipconfig_method_inform_e: {
	  for (config = config_list, i = 0; i < count; i++, config++) {
	      if (ipconfig_method_is_manual(config->method)
		  && (method_data->ip[0].addr.s_addr
		      == config->method_data->ip[0].addr.s_addr)) {
		  return (config);
	      }
	  }	  
	  break;
      }
      default: {
	break;
      }
    }
    return (NULL);
}

static ServiceConfig_t *
ServiceConfig_list_lookup_service(ServiceConfig_t * config_list, int count, 
				  CFStringRef serviceID)
{
    ServiceConfig_t *	config;
    int 		i;

    for (config = config_list, i = 0; i < count; i++, config++) {
	if (CFEqual(serviceID, config->serviceID)) {
	    return (config);
	}
    }
    return (NULL);
}

static ServiceConfig_t *
ServiceConfig_list_init(CFArrayRef all_ipv4, char * ifname, int * count_p)
{
    ServiceConfig_t *	config_list = NULL;
    int			count = 0;
    int			i;
    CFArrayRef 		if_service_list;
    int			if_service_count;
    CFStringRef		ifn_cf = NULL;

    ifn_cf = CFStringCreateWithCString(NULL, ifname,
				       kCFStringEncodingMacRoman);
    if (ifn_cf == NULL) {
	goto done;
    }
    if_service_list = interface_services_copy(all_ipv4, ifn_cf);
    if (if_service_list == NULL) {
	goto done;
    }
    if_service_count = CFArrayGetCount(if_service_list);
    config_list = (ServiceConfig_t *) calloc(if_service_count, 
					     sizeof(*config_list));
    if (config_list == NULL) {
	goto done;
    }

    for (i = 0; i < if_service_count; i++) {
	CFDictionaryRef		ipv4_dict;
	ipconfig_method_t	method;
	ipconfig_method_data_t *method_data;
	int			method_data_len;
	CFStringRef		serviceID;

	ipv4_dict = CFArrayGetValueAtIndex(if_service_list, i);
	serviceID = CFDictionaryGetValue(ipv4_dict,
					 PROP_SERVICEID);
	method_data = ipconfig_method_data_from_dict(ipv4_dict, &method,
						     &method_data_len);
	if (method_data == NULL) {
	    continue;
	}
	if (ServiceConfig_list_lookup_method(config_list, count, method, 
					     method_data, method_data_len)) {
	    boolean_t	is_manual = ipconfig_method_is_manual(method);

	    if (is_manual) {
		my_log(LOG_INFO, "%s: %s " IP_FORMAT " duplicate service",
		       ifname, ipconfig_method_string(method),
		       IP_LIST(&method_data->ip[0].addr));
	    }
	    else {
		my_log(LOG_INFO, "%s: %s ignored",
		       ifname, ipconfig_method_string(method));
	    }
	    free(method_data);
	    continue;
	}
	config_list[count].serviceID = CFRetain(serviceID);
	config_list[count].method = method;
	config_list[count].method_data = method_data;
	config_list[count].method_data_len = method_data_len;
	count++;
    }
 done:
    if (config_list && count == 0) {
	ServiceConfig_list_free(&config_list, count);
    }
    my_CFRelease(&ifn_cf);
    my_CFRelease(&if_service_list);
    *count_p = count;
    return (config_list);
}

static void
free_inactive_services(char * ifname, ServiceConfig_t * config_list, int count)
{
    int			j;
    IFState_t *		ifstate;
    CFMutableArrayRef	list = NULL;

    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifname, NULL);
    if (ifstate == NULL) {
	goto done;
    }
    list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (list == NULL) {
	goto done;
    }
    
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t * service_p = dynarray_element(&ifstate->services, j);
	CFStringRef serviceID = service_p->serviceID;

	if (service_p->parent_serviceID != NULL) {
	    /* this service gets cleaned up on its own */
	    continue;
	}
	if (ServiceConfig_list_lookup_service(config_list, count,
					      serviceID) == NULL) {
	    CFArrayAppendValue(list, serviceID);
	}
    }

    for (j = 0; j < CFArrayGetCount(list); j++) {
	CFStringRef serviceID = CFArrayGetValueAtIndex(list, j);

	IFState_service_free(ifstate, serviceID);
    }

 done:
    my_CFRelease(&list);
    return;
}

static ipconfig_status_t
set_service(IFState_t * ifstate, ServiceConfig_t * config)
{
    CFStringRef		serviceID = config->serviceID;
    Service_t *		service_p;
    IFState_t *		this_ifstate = NULL;

    service_p = IFState_service_with_ID(ifstate, serviceID);
    if (service_p) {
	boolean_t		needs_stop = FALSE;
	ipconfig_status_t	status;

	if (service_p->method == config->method) {
	    status = config_method_change(service_p, config->method, 
					  config->method_data,
					  config->method_data_len, 
					  &needs_stop);
	    if (status == ipconfig_status_success_e
		&& needs_stop == FALSE) {
		return (ipconfig_status_success_e);
	    }
	}
	IFState_service_free(ifstate, serviceID);
    }
    else {
	this_ifstate = IFStateList_service_with_ID(&S_ifstate_list, 
						    serviceID,
						    &service_p);
	if (this_ifstate) {
	    /* service is on other interface, stop it now */
	    IFState_service_free(this_ifstate, serviceID);
	}
    }
    return (IFState_service_add(ifstate, serviceID, config->method,
				config->method_data, config->method_data_len,
				NULL, NULL));
}

static void
handle_configuration_changed(SCDynamicStoreRef session, CFArrayRef all_ipv4)
{
    int i;

    for (i = 0; i < ifl_count(S_interfaces); i++) {
	ServiceConfig_t *	config;
	int			count = 0;
	IFState_t *		ifstate;
	ServiceConfig_t *	if_services = NULL;
	interface_t *		if_p = ifl_at_index(S_interfaces, i);

	if (strcmp(if_name(if_p), "lo0") == 0) {
	    continue;
	}
	if_services = ServiceConfig_list_init(all_ipv4, if_name(if_p), &count);
	if (if_services == NULL) {
	    ifstate =  IFStateList_ifstate_with_name(&S_ifstate_list, 
						     if_name(if_p), NULL);
	    if (ifstate != NULL) {
		IFState_services_free(ifstate);
	    }
	    continue;
	}

	/* stop services that are no longer active */
	free_inactive_services(if_name(if_p), if_services, count);

	ifstate = IFStateList_ifstate_create(&S_ifstate_list, if_p);
	if (ifstate) {
	    int k;

	    /* update each of the services that are configured */
	    for (k = 0, config = if_services; k < count; k++, config++) {
		(void)set_service(ifstate, config);
	    }
	}
	ServiceConfig_list_free(&if_services, count);
    }
    return;
}

static void
configuration_changed(SCDynamicStoreRef session)
{
    CFArrayRef		all_ipv4 = NULL;

    all_ipv4 = entity_all(session);
    handle_configuration_changed(session, all_ipv4);
    my_CFRelease(&all_ipv4);
    return;
}

static void
configure_from_cache(SCDynamicStoreRef session)
{
    CFArrayRef		all_ipv4 = NULL;
    int			count = 0;
    int 		i;

    all_ipv4 = entity_all(session);
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
	dict = lookup_entity(all_ipv4, ifn_cf);
	if (dict == NULL) {
	    goto loop_done;
	}
	(void)IFStateList_ifstate_create(&S_ifstate_list, if_p);
	count++;
    loop_done:
	my_CFRelease(&ifn_cf);
    }

 done:
    if (count == 0) {
	unblock_startup(session);
    }
    else {
	handle_configuration_changed(session, all_ipv4);
    }
    my_CFRelease(&all_ipv4);

    return;
}

static void
configure_from_iftab(SCDynamicStoreRef session)
{
    ipcfg_table_t *	new_ipcfg_list = NULL;
    hostconfig_t *	new_hostconfig = NULL;

    if (access("/etc/iftab", R_OK) == 0) {
	char		msg[512];

	new_ipcfg_list = ipcfg_from_file(msg);
	if (new_ipcfg_list == NULL) {
	    my_log(LOG_ERR, "ipconfigd: failed to get ip config, %s", msg);
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
	    my_log(LOG_ERR, "ipconfigd: failed to get hostconfig, %s", msg);
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
notifier_init(SCDynamicStoreRef session)
{
    CFMutableArrayRef	keys = NULL;
    CFStringRef		key;
    CFMutableStringRef	pattern;
    CFMutableArrayRef	patterns = NULL;
    CFRunLoopSourceRef	rls;

    if (session == NULL) {
	return;
    }
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* notify when IPv4 config of any service changes */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetIPv4);
    CFArrayAppendValue(patterns, key);
    my_CFRelease(&key);

    /* notify when Interface service <-> interface binding changes */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetInterface);
    CFArrayAppendValue(patterns, key);
    my_CFRelease(&key);

    /* notify when the link status of any interface changes */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetLink);
    CFArrayAppendValue(patterns, key);
    my_CFRelease(&key);

    /* notify when there's an ARP collision on any interface */
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							kSCCompAnyRegex,
							kSCEntNetIPv4ARPCollision);
    pattern = CFStringCreateMutableCopy(NULL, 0, key);
    CFStringAppend(pattern, CFSTR(".*"));

    CFArrayAppendValue(patterns, pattern);
    my_CFRelease(&key);
    my_CFRelease(&pattern);

    /* notify when list of interfaces changes */
    key = SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);

    /* notify when the service order changes */
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);

    /* notify when computer name changes */
    S_computer_name_key = SCDynamicStoreKeyCreateComputerName(NULL);
    CFArrayAppendValue(keys, S_computer_name_key);

    /* notify when DHCP client requested parameters is applied */
    S_dhcp_preferences_key 
      = SCDynamicStoreKeyCreatePreferences(NULL, 
					   kDHCPClientPreferencesID, 
					   kSCPreferencesKeyApply);
    CFArrayAppendValue(keys, S_dhcp_preferences_key);
    SCDynamicStoreSetNotificationKeys(session, keys, patterns);
    my_CFRelease(&keys);
    my_CFRelease(&patterns);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, session, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    /* initialize the computer name */
    computer_name_update(session);
    return;
}

static boolean_t
update_interface_list()
{
    interface_list_t *	new_interfaces = NULL;

    new_interfaces = ifl_init();
    if (new_interfaces == NULL) {
	my_log(LOG_ERR, "ipconfigd: ifl_init failed");
	return (FALSE);
    }
    if (S_interfaces) {
	ifl_free(&S_interfaces);
    }
    S_interfaces = new_interfaces;

    return (TRUE);
}

/*
 * Function: check_for_detached_interfaces
 * Purpose:
 *   Remove interface state for any interface that has been removed.
 *   Create a temporary list to store the name of each interface that
 *   has been removed.  Iterate through that list to remove individual
 *   interface state records.  This is done to avoid problems with
 *   iterating over a list while it is modified.
 */
static void 
check_for_detached_interfaces()
{
    int		count = dynarray_count(&S_ifstate_list);
    char * *	names = NULL;
    int		names_count = 0;
    int 	i;


    if (count == 0) {
	return;
    }

    /* allocate worst case scenario in which each ifstate needs to be removed */
    names = (char * *)malloc(sizeof(char *) * count);
    if (names == NULL) {
	return;
    }
    for (i = 0; i < count; i++) {
	IFState_t *	ifstate = dynarray_element(&S_ifstate_list, i);
	
	if (ifl_find_name(S_interfaces, if_name(ifstate->if_p)) == NULL) {
	    names[names_count++] = if_name(ifstate->if_p);
	}
    }
    for (i = 0; i < names_count; i++) {
	IFStateList_ifstate_free(&S_ifstate_list, names[i]);
    }
    free(names);
    return;
}

static void
before_blocking(CFRunLoopObserverRef observer, 
		CFRunLoopActivity activity, void *info)
{
    CFArrayRef		service_order = NULL;

    if (S_linklocal_election_required == FALSE) {
	return;
    }
    S_linklocal_election_required = FALSE;
    if (S_scd_session == NULL) {
	return;
    }
    service_order = S_get_service_order(S_scd_session);
    if (service_order == NULL) {
	return;
    }
    if (G_verbose) {
	my_log(LOG_INFO, "before_blocking: calling S_linklocal_elect");
    }
    S_linklocal_elect(service_order);
    my_CFRelease(&service_order);
    
    return;
}

static boolean_t
start_initialization(SCDynamicStoreRef session)
{
    CFStringRef			key;
    CFPropertyListRef		value = NULL;

    S_observer = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeWaiting,
					 TRUE, 0, before_blocking, NULL);
    if (S_observer != NULL) {
	CFRunLoopAddObserver(CFRunLoopGetCurrent(), S_observer, 
			     kCFRunLoopDefaultMode);
    }
    else {
	my_log(LOG_INFO, 
	       "start_initialization: CFRunLoopObserverCreate failed!\n");
    }
    S_setup_service_prefix = SCDynamicStoreKeyCreate(NULL,
						     CFSTR("%@/%@/%@/"), 
						     kSCDynamicStoreDomainSetup,
						     kSCCompNetwork,
						     kSCCompService);
						       
    S_state_interface_prefix = SCDynamicStoreKeyCreate(NULL,
						       CFSTR("%@/%@/%@/"), 
						       kSCDynamicStoreDomainState,
						       kSCCompNetwork,
						       kSCCompInterface);

    value = SCDynamicStoreCopyValue(session, kSCDynamicStoreDomainSetup);
    if (value == NULL) {
	my_log(LOG_INFO, "IPConfiguration needs PreferencesMonitor to run first");
    }
    my_CFRelease(&value);

    /* install run-time notifiers */
    notifier_init(session);

    (void)update_interface_list();

    (void)S_netboot_init();

    /* populate cache with flat files or is cache already populated? */
    key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@" USE_FLAT_FILES), 
				  kSCDynamicStoreDomainSetup);
    value = SCDynamicStoreCopyValue(session, key);
    my_CFRelease(&key);
    if (value) {
	/* use iftab, hostconfig files to populate the cache */
	configure_from_iftab(session);
    }
    else {
	/* cache is already populated */
	configure_from_cache(session);
    }
    my_CFRelease(&value);
    return (TRUE);
}

#ifdef MAIN
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
#endif MAIN

static void
link_key_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFDictionaryRef		dict = NULL;
    CFStringRef			ifn_cf = NULL;
    char			ifn[IFNAMSIZ + 1];
    IFState_t *   		ifstate;
    int 			j;
    
    ifn_cf = parse_component(cache_key, S_state_interface_prefix);
    if (ifn_cf == NULL) {
	return;
    }
    cfstring_to_cstring(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    if (ifstate == NULL || ifstate->netboot) {
	/* don't propogate media status events for netboot interface */
	goto done;
    }
    IFState_update_media_status(ifstate);
    dict = my_SCDynamicStoreCopyValue(session, cache_key);
    if (dict != NULL) {
	if (CFDictionaryContainsKey(dict, kSCPropNetLinkDetaching)) {
	    IFState_services_free(ifstate);
	    goto done;
	}
    }
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	config_method_media(service_p);
    }

 done:
    my_CFRelease(&dict);
    my_CFRelease(&ifn_cf);
    return;
}

static void *
bytesFromColonHexString(CFStringRef colon_hex, int * len)
{
    CFArrayRef	arr = NULL;
    uint8_t *	bytes = NULL;
    char	hexstr[4];
    int 	i;
    int		n_bytes;
 
    arr = CFStringCreateArrayBySeparatingStrings(NULL, colon_hex, CFSTR(":"));
    if (arr == NULL || CFArrayGetCount(arr) == 0) {
	goto failed;
    }
    n_bytes = CFArrayGetCount(arr);
    bytes = (uint8_t *)malloc(n_bytes);
#define BASE_16		16
    for (i = 0; i < n_bytes; i++) {
	CFStringRef	str = CFArrayGetValueAtIndex(arr, i);
	cfstring_to_cstring(str, hexstr, sizeof(hexstr));
	bytes[i] = (uint8_t)strtoul(hexstr, NULL, BASE_16);
    }
    my_CFRelease(&arr);
    *len = n_bytes;
    return (bytes);

 failed:
    my_CFRelease(&arr);
    return (NULL);
}

static CFStringRef
parse_arp_collision(CFStringRef cache_key, struct in_addr * ipaddr_p,
		    void * * hwaddr, int * hwlen)
{
    CFArrayRef			components = NULL;
    CFStringRef			ifn_cf = NULL;
    CFStringRef			ip_cf = NULL;
    CFStringRef			hwaddr_cf = NULL;

    ipaddr_p->s_addr = 0;
    *hwaddr = NULL;
    *hwlen = 0;

    /* 
     * Turn
     *   State:/Network/Interface/ifname/IPV4ARPCollision/ipaddr/hwaddr 
     * into
     *   { "State:", "Network", "Interface", ifname, "IPV4ARPCollision",
     *      ipaddr, hwaddr }
     */
    components = CFStringCreateArrayBySeparatingStrings(NULL, cache_key, 
							CFSTR("/"));
    if (components == NULL || CFArrayGetCount(components) < 7) {
	goto failed;
    }
    ifn_cf = CFArrayGetValueAtIndex(components, 3);
    ip_cf = CFArrayGetValueAtIndex(components, 5);
    hwaddr_cf = CFArrayGetValueAtIndex(components, 6);
    *ipaddr_p = cfstring_to_ip(ip_cf);
    if (ipaddr_p->s_addr == 0) {
	goto failed;
    }
    *hwaddr = bytesFromColonHexString(hwaddr_cf, hwlen);
    CFRetain(ifn_cf);
    my_CFRelease(&components);
    return (ifn_cf);

 failed:
    my_CFRelease(&components);
    return (NULL);
}

static void
arp_collision(SCDynamicStoreRef session, CFStringRef cache_key)
{
    arp_collision_data_t	evdata;
    void *			hwaddr = NULL;
    int				hwlen;
    CFStringRef			ifn_cf = NULL;
    char			ifn[IFNAMSIZ + 1];
    struct in_addr		ip_addr;
    IFState_t *   		ifstate;
    int 			j;

    ifn_cf = parse_arp_collision(cache_key, &ip_addr, &hwaddr, &hwlen);
    if (ifn_cf == NULL || hwaddr == NULL) {
	goto done;
    }
    if (S_is_our_hardware_address(NULL, ARPHRD_ETHER, hwaddr, hwlen)) {
	goto done;
    }
    cfstring_to_cstring(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&S_ifstate_list, ifn, NULL);
    if (ifstate == NULL || ifstate->netboot) {
	/* don't propogate collision events for netboot interface */
	goto done;
    }
    evdata.ip_addr = ip_addr;
    evdata.hwaddr = hwaddr;
    evdata.hwlen = hwlen;
    for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	Service_t *	service_p = dynarray_element(&ifstate->services, j);

	config_method_arp_collision(service_p, &evdata);
    }

 done:
    if (hwaddr != NULL) {
	free(hwaddr);
    }
    my_CFRelease(&ifn_cf);
    return;
}

static void
dhcp_preferences_changed(SCDynamicStoreRef session)
{
    int i;

    /* merge in the new requested parameters */
    S_add_dhcp_parameters();

    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	IFState_t *	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	/* ask each service to renew immediately to pick up new options */
	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t *	service_p = dynarray_element(&ifstate->services, j);

	    config_method_renew(service_p);
	}
    }
    return;
}

static void
handle_change(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    boolean_t		config_changed = FALSE;
    CFIndex		count;
    boolean_t		dhcp_changed = FALSE;
    CFIndex		i;
    boolean_t		iflist_changed = FALSE;
    boolean_t		name_changed = FALSE;
    boolean_t		order_changed = FALSE;

    count = CFArrayGetCount(changes);
    if (count == 0) {
	goto done;
    }
    SCLog(G_verbose, LOG_INFO, CFSTR("Changes: %@ (%d)"), changes,
	  count);
    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);
	if (CFEqual(cache_key, S_computer_name_key)) {
	    name_changed = TRUE;
	}
	else if (CFEqual(cache_key, S_dhcp_preferences_key)) {
	    dhcp_changed = TRUE;
	}
        else if (CFStringHasPrefix(cache_key, kSCDynamicStoreDomainSetup)) {
	    CFStringRef	key;

	    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							     kSCDynamicStoreDomainSetup,
							     kSCEntNetIPv4);
	    if (key) {
		if (CFEqual(cache_key, key)) {
		    /* service order may have changed */
		    order_changed = TRUE;
		}
	    }
	    my_CFRelease(&key);

	    /* IPv4 configuration changed */
	    config_changed = TRUE;
	}
	else if (CFStringHasSuffix(cache_key, kSCCompInterface)) {
	    /* list of interfaces changed */
	    iflist_changed = TRUE;
	}
    }
    /* the computer name changed */
    if (name_changed) {
	computer_name_update(session);
    }
    /* an interface was added/removed */
    if (iflist_changed) {
	if (update_interface_list()) {
	    config_changed = TRUE;
	    check_for_detached_interfaces();
	}
    }
    /* configuration changed */
    if (config_changed) {
	configuration_changed(session);
    }
    /* dhcp preferences changed */
    if (dhcp_changed) {
	dhcp_preferences_changed(session);
    }
    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);

	if (CFStringHasSuffix(cache_key, kSCEntNetLink)) {
	    link_key_changed(session, cache_key);
	}
	else {
	    CFRange 	range = CFRangeMake(0, CFStringGetLength(cache_key));

	    if (CFStringFindWithOptions(cache_key, kSCEntNetIPv4ARPCollision,
					range, 0, NULL)) {
		arp_collision(session, cache_key);
	    }
	}
    }
    /* service order may have changed */
    if (order_changed) {
	/* ensure that services on the same subnet are ordered */
	order_services(session);
    }
 done:
    return;
}

#ifndef NO_CFUserNotification

static void
user_confirm(CFUserNotificationRef userNotification, 
	     CFOptionFlags responseFlags)
{
    int i;

    /* clean-up the notification */
    for (i = 0; i < dynarray_count(&S_ifstate_list); i++) {
	IFState_t *	ifstate = dynarray_element(&S_ifstate_list, i);
	int		j;

	for (j = 0; j < dynarray_count(&ifstate->services); j++) {
	    Service_t * service_p = dynarray_element(&ifstate->services, j);
	    if (service_p->user_notification == userNotification) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), 
				      service_p->user_rls, 
				      kCFRunLoopDefaultMode);
		my_CFRelease(&service_p->user_rls);
		my_CFRelease(&service_p->user_notification);
		return;
	    }
	}
    }
    return;
}

static void
service_notify_user(Service_t * service_p, CFTypeRef alert_string)
{
    CFMutableDictionaryRef	dict = NULL;
    SInt32			error = 0;
    CFUserNotificationRef 	notify = NULL;
    CFRunLoopSourceRef		rls = NULL;
    CFURLRef			url = NULL;
 
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
	goto done;
    }

    url = CFBundleCopyBundleURL(S_bundle);
    if (url == NULL) {
	goto done;
    }

    CFDictionarySetValue(dict, kCFUserNotificationAlertHeaderKey, 
			 CFSTR("IP Configuration"));
    CFDictionarySetValue(dict, kCFUserNotificationAlertMessageKey, 
			 alert_string);
    CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey,
			 url);

    if (service_p->user_rls) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), service_p->user_rls,
			      kCFRunLoopDefaultMode);
	my_CFRelease(&service_p->user_rls);
    }
    if (service_p->user_notification != NULL) {
	CFUserNotificationCancel(service_p->user_notification);
	my_CFRelease(&service_p->user_notification);
    }
    notify = CFUserNotificationCreate(NULL, 0, 0, &error, dict);
    if (notify == NULL) {
	my_log(LOG_ERR, "CFUserNotificationCreate() failed, %d",
	       error);
	goto done;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, notify, 
						user_confirm, 0);
    if (rls == NULL) {
	my_log(LOG_ERR, "CFUserNotificationCreateRunLoopSource() failed");
	my_CFRelease(&notify);
    }
    else {
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, 
			   kCFRunLoopDefaultMode);
	service_p->user_rls = rls;
	service_p->user_notification = notify;
    }
 done:
    my_CFRelease(&dict);
    my_CFRelease(&url);
    return;
}

#else

static void
service_notify_user(Service_t * service_p, CFTypeRef alertMessage)
{
}

#endif

void
service_tell_user(Service_t * service_p, char * msg)
{
    CFStringRef		alert_string = NULL;

    alert_string = CFStringCreateWithCString(NULL, msg, 
					     kCFStringEncodingMacRoman);
    service_notify_user(service_p, alert_string);
    my_CFRelease(&alert_string);
}

void
service_report_conflict(Service_t * service_p, struct in_addr * ip,   
                        void * hwaddr, struct in_addr * server)
{
    CFMutableArrayRef	array;
    CFStringRef         str;

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (array == NULL) {
	goto done;
    }

    /* add conflicting IP address */
    str = CFStringCreateWithFormat(NULL, NULL,
				   CFSTR(IP_FORMAT), IP_LIST(ip));
    if (str == NULL) {
	goto done;
    }
    CFArrayAppendValue(array, str);
    CFRelease(str);

    /* add " in use by " */
    CFArrayAppendValue(array, CFSTR("IN_USE_BY"));

    /* add conflicting ethernet address */
    str = CFStringCreateWithFormat(NULL, NULL,
				   CFSTR(EA_FORMAT), EA_LIST(hwaddr));
    if (str == NULL) {
	goto done;
    }
    CFArrayAppendValue(array, str);
    CFRelease(str);

    if (server != NULL) {
	switch (service_p->method) {
	case ipconfig_method_bootp_e: {
	    CFArrayAppendValue(array, CFSTR("BOOTP_SERVER"));
	    break;
	}
	case ipconfig_method_dhcp_e: {
	    CFArrayAppendValue(array, CFSTR("DHCP_SERVER"));
	    break;
	}
	default:
	    goto post;
	    break;
	} /* switch */

	/* add server IP address */
	str = CFStringCreateWithFormat(NULL, NULL,
				       CFSTR(IP_FORMAT), IP_LIST(server));
	if (str == NULL) {
	    goto done;
	}
	CFArrayAppendValue(array, str);
	CFRelease(str);
    }

    /* post notification */
 post:
    service_notify_user(service_p, array);

 done:
    my_CFRelease(&array);
    return;
}

#ifdef MAIN
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

int 
main(int argc, char *argv[])
{
    int 		ch;
    u_char *		progname = argv[0];
    pid_t		ppid;

    if (server_active()) {
	fprintf(stderr, "ipconfig server already active\n");
	exit(1);
    }
    { /* set the random seed */
	struct timeval	start_time;

	gettimeofday(&start_time, 0);
	srandom(start_time.tv_usec & ~start_time.tv_sec);
    }

    while ((ch = getopt(argc, argv, "Bbc:dg:hHl:r:s:v")) != EOF) {
	switch ((char) ch) {
	case 'B':
	    G_dhcp_accepts_bootp = TRUE;
	    break;
	case 'b':
	    G_must_broadcast = TRUE;
	    break;
	case 'c': /* client port - for testing */
	    G_client_port = atoi(optarg);
	    break;
	case 'd':
	    G_debug = TRUE;
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

    (void)dhcp_lease_init();

    G_readers = FDSet_init();
    if (G_readers == NULL) {
	my_log(LOG_DEBUG, "FDSet_init() failed");
	exit(UNEXPECTED_ERROR);
    }
    G_bootp_session = bootp_session_init(G_client_port);
    if (G_bootp_session == NULL) {
	my_log(LOG_DEBUG, "bootp_session_init() failed");
	exit(UNEXPECTED_ERROR);
    }

    G_arp_session = arp_session_init(S_is_our_hardware_address);
    if (G_arp_session == NULL) {
	my_log(LOG_DEBUG, "arp_session_init() failed");
	exit(UNEXPECTED_ERROR);
    }

    if (G_debug) {
	(void) openlog("ipconfigd", LOG_PERROR | LOG_PID, LOG_DAEMON);
    }
    else {
	(void) openlog("ipconfigd", LOG_CONS | LOG_PID, LOG_DAEMON);
    }
    bootp_session_set_debug(G_bootp_session, G_debug);
    dynarray_init(&S_ifstate_list, IFState_free, NULL);
    S_scd_session = SCDynamicStoreCreate(NULL, 
					 CFSTR("IPConfiguration"),
					 handle_change, NULL);
    if (S_scd_session == NULL) {
	my_log(LOG_INFO, "SCDynamicStoreCreate failed: %s", 
	       SCErrorString(SCError()));
	if (G_debug == 0) {
	    /* synchronize with parent process */
	    kill(ppid, SIGTERM);
	}
	exit(UNEXPECTED_ERROR);
    }

    /* begin interface initialization */
    start_initialization(S_scd_session);

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
#else MAIN
#define IPCONFIGURATION_PLIST	"IPConfiguration.xml"

/**
 ** Routines to read configuration and convert from CF types to
 ** simple types.
 **/
static boolean_t
S_get_plist_boolean(CFDictionaryRef plist, CFStringRef key,
		     boolean_t def)
{
    CFBooleanRef 	b;
    boolean_t		ret = def;

    b = isA_CFBoolean(CFDictionaryGetValue(plist, key));
    if (b) {
	ret = CFBooleanGetValue(b);
    }
    SCLog(G_verbose, LOG_INFO, CFSTR("%@ = %s"), key, ret == TRUE ? "true" : "false");
    return (ret);
}

static int
S_get_plist_int(CFDictionaryRef plist, CFStringRef key,
		int def)
{
    CFNumberRef 	n;
    int			ret = def;

    n = isA_CFNumber(CFDictionaryGetValue(plist, key));
    if (n) {
	if (CFNumberGetValue(n, kCFNumberIntType, &ret) == FALSE) {
	    ret = def;
	}
    }
    SCLog(G_verbose, LOG_INFO, CFSTR("%@ = %d"), key, ret);
    return (ret);
}

#include <math.h>
static struct timeval
S_get_plist_timeval(CFDictionaryRef plist, CFStringRef key,
		    struct timeval def)
{
    CFNumberRef 	n;
    struct timeval	ret = def;

    n = CFDictionaryGetValue(plist, key);
    if (n) {
	double	f;

	if (CFNumberGetValue(n, kCFNumberDoubleType, &f) == TRUE) {
	    ret.tv_sec = (int)floor(f);
	    ret.tv_usec = (int)((f - floor(f)) * 1000000.0);
	}
    }
    SCLog(G_verbose, LOG_INFO, CFSTR("%@ = %d.%06d"), key, ret.tv_sec,
	  ret.tv_usec);
    return (ret);
}

static u_char *
S_get_char_array(CFArrayRef arr, int * len)
{
    u_char *	buf = NULL;
    int		count = 0;
    int 	i;
    int 	real_count;
	
    count = CFArrayGetCount(arr);
    if (count == 0) {
	goto done;
    }
    buf = malloc(count);
    if (buf == NULL) {
	goto done;
    }
    for (i = 0, real_count = 0; i < count; i++) {
	CFNumberRef	n = isA_CFNumber(CFArrayGetValueAtIndex(arr, i));
	int 		val;
	
	if (n && CFNumberGetValue(n, kCFNumberIntType, &val)) {
	    buf[real_count++] = (u_char) val;
	}
    }
    count = real_count;
 done:
    *len = count;
    if (count == 0 && buf) {
	free(buf);
	buf = NULL;
    }
    return (buf);
}

static u_char *
S_get_plist_char_array(CFDictionaryRef plist, CFStringRef key,
		       int * len)
{
    CFArrayRef	a;

    a = isA_CFArray(CFDictionaryGetValue(plist, key));
    if (a == NULL) {
	return (NULL);
    }
    return (S_get_char_array(a, len));
}



static void 
my_CFNumberAddUniqueToArray(CFNumberRef number, CFMutableArrayRef merge)
{
    number = isA_CFNumber(number);
    if (number == NULL) {
	return;
    }
    my_CFArrayAppendUniqueValue(merge, number);
}

static void
merge_arrays(const void *key, const void *value, void *context)
{
    CFArrayRef	        arr;
    CFDictionaryRef	dict;
    CFMutableArrayRef	merge = (CFMutableArrayRef)context;

    dict = isA_CFDictionary(value);
    if (dict == NULL) {
	return;
    }
    arr = CFDictionaryGetValue(dict, 
			       kDHCPRequestedParameterList);
    if (isA_CFArray(arr) == NULL) {
	return;
    }
    CFArrayApplyFunction(arr, CFRangeMake(0, CFArrayGetCount(arr)), 
			 (CFArrayApplierFunction)my_CFNumberAddUniqueToArray, 
			 merge);
    return;
}

static CFArrayRef
applicationRequestedParametersCopy()
{
    SCPreferencesRef session = NULL;
    CFPropertyListRef data = NULL;
    CFMutableArrayRef array = NULL;

    session = SCPreferencesCreate(NULL, CFSTR("IPConfiguration.DHCPClient.xml"),
				  kDHCPClientPreferencesID);
    if (session == NULL) {
	SCLog(G_verbose, LOG_INFO, CFSTR("SCPreferencesCreate DHCPClient failed: %s"),
	      SCErrorString(SCError()));
	return (NULL);
    }
    data = SCPreferencesGetValue(session, kDHCPClientApplicationPref);
    if (isA_CFDictionary(data) == NULL) {
	goto done;
    }
    if (data) {
	SCLog(G_verbose, LOG_INFO, CFSTR("dictionary is %@"), data);
    }
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (array == NULL) {
	goto done;
    }
    CFDictionaryApplyFunction(data, merge_arrays, array);
    if (CFArrayGetCount(array) == 0) {
	CFRelease(array);
	array = NULL;
    }
	
 done:
    my_CFRelease(&session);
    return (array);
}

static void
S_set_globals(const char * bundleDir)
{
    CFPropertyListRef	plist;
    char		path[PATH_MAX];

    snprintf(path, sizeof(path), 
	     "%s/Resources/" IPCONFIGURATION_PLIST, bundleDir);
    plist = my_CFPropertyListCreateFromFile(path);
    if (plist && isA_CFDictionary(plist)) {
	u_char *	dhcp_params = NULL;
	int		n_dhcp_params = 0;

	G_verbose = S_get_plist_boolean(plist, CFSTR("Verbose"), FALSE);
	G_must_broadcast 
	    = S_get_plist_boolean(plist, CFSTR("MustBroadcast"), FALSE);
	G_max_retries = S_get_plist_int(plist, CFSTR("RetryCount"), 
					MAX_RETRIES);
	G_gather_secs = S_get_plist_int(plist, CFSTR("GatherTimeSeconds"), 
					GATHER_SECS);
	G_link_inactive_secs 
	    = S_get_plist_int(plist, CFSTR("LinkInactiveWaitTimeSeconds"), 
			      LINK_INACTIVE_WAIT_SECS);
	G_initial_wait_secs 
	    = S_get_plist_int(plist, CFSTR("InitialRetryTimeSeconds"), 
			      INITIAL_WAIT_SECS);
	G_max_wait_secs 
	    = S_get_plist_int(plist, CFSTR("MaximumRetryTimeSeconds"), 
			      MAX_WAIT_SECS);
	S_arp_probe_count
	    = S_get_plist_int(plist, CFSTR("ARPProbeCount"), 
			      ARP_PROBE_COUNT);
	S_arp_gratuitous_count
	    = S_get_plist_int(plist, CFSTR("ARPGratuitousCount"), 
			      ARP_GRATUITOUS_COUNT);
	S_arp_retry
	    = S_get_plist_timeval(plist, CFSTR("ARPRetryTimeSeconds"), 
				  S_arp_retry);
	G_dhcp_accepts_bootp 
	    = S_get_plist_boolean(plist, CFSTR("DHCPAcceptsBOOTP"), FALSE);
	G_dhcp_failure_configures_linklocal
	    = S_get_plist_boolean(plist, 
				  CFSTR("DHCPFailureConfiguresLinkLocal"), 
				  DHCP_FAILURE_CONFIGURES_LINKLOCAL);
	G_dhcp_success_deconfigures_linklocal
	    = S_get_plist_boolean(plist, 
				  CFSTR("DHCPSuccessDeconfiguresLinkLocal"), 
				  DHCP_SUCCESS_DECONFIGURES_LINKLOCAL);
	G_dhcp_init_reboot_retry_count
	    = S_get_plist_int(plist, CFSTR("DHCPInitRebootRetryCount"), 
			      DHCP_INIT_REBOOT_RETRY_COUNT);
	G_dhcp_allocate_linklocal_at_retry_count
	    = S_get_plist_int(plist, CFSTR("DHCPAllocateLinkLocalAtRetryCount"),
			      DHCP_ALLOCATE_LINKLOCAL_AT_RETRY_COUNT);
	dhcp_params 
	    = S_get_plist_char_array(plist,
				     kDHCPRequestedParameterList,
				     &n_dhcp_params);
	dhcp_set_default_parameters(dhcp_params, n_dhcp_params);
    }
    my_CFRelease(&plist);
}

static void
S_add_dhcp_parameters()
{
    u_char *	dhcp_params = NULL;
    int		n_dhcp_params = 0;

    CFArrayRef	rp = applicationRequestedParametersCopy();

    if (rp) {
	dhcp_params  = S_get_char_array(rp,
					&n_dhcp_params);
	my_CFRelease(&rp);
    }
    dhcp_set_additional_parameters(dhcp_params, n_dhcp_params);
    return;
}

void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    S_bundle = (CFBundleRef)CFRetain(bundle);
    S_verbose = bundleVerbose;
    return;
}

void
start(const char *bundleName, const char *bundleDir)
{
    if (server_active()) {
	fprintf(stderr, "ipconfig server already active\n");
	return;
    }

    { /* set the random seed */
	struct timeval	start_time;

	gettimeofday(&start_time, 0);
	srandom(start_time.tv_usec & ~start_time.tv_sec);
    }
    
    S_set_globals(bundleDir);
    if (S_verbose == TRUE) {
	G_verbose = TRUE;
    }
    S_add_dhcp_parameters();

    S_scd_session = SCDynamicStoreCreate(NULL, 
					 CFSTR("IPConfiguration"),
					 handle_change, NULL);
    if (S_scd_session == NULL) {
	S_scd_session = NULL;
	my_log(LOG_ERR, "SCDynamicStoreCreate failed: %s", 
	       SCErrorString(SCError()));
    }

    (void)dhcp_lease_init();

    G_readers = FDSet_init();
    if (G_readers == NULL) {
	my_log(LOG_DEBUG, "FDSet_init() failed");
	return;
    }
    G_bootp_session = bootp_session_init(G_client_port);
    if (G_bootp_session == NULL) {
	my_log(LOG_DEBUG, "bootp_session_init() failed");
	return;
    }

    G_arp_session = arp_session_init(S_is_our_hardware_address, &S_arp_retry,
				     &S_arp_probe_count, 
				     &S_arp_gratuitous_count);
    if (G_arp_session == NULL) {
	my_log(LOG_DEBUG, "arp_session_init() failed");
	return;
    }

    bootp_session_set_debug(G_bootp_session, G_debug);
    dynarray_init(&S_ifstate_list, IFState_free, NULL);

    /* set the loopback interface address */
    set_loopback();
    return;
}
void
prime()
{
    if (S_scd_session == NULL) {
	update_interface_list();
    }
    else {
	/* begin interface initialization */
	start_initialization(S_scd_session);
    }

    /* initialize the MiG server */
    server_init();
}

#endif MAIN
